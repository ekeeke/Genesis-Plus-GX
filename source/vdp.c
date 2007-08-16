
#include "shared.h"
#include "hvc.h"

/* Pack and unpack CRAM data */
#define PACK_CRAM(d)    ((((d)&0xE00)>>9)|(((d)&0x0E0)>>2)|(((d)&0x00E)<<5))
#define UNPACK_CRAM(d)  ((((d)&0x1C0)>>5)|((d)&0x038)<<2|(((d)&0x007)<<9))

/* Mark a pattern as dirty */
#define MARK_BG_DIRTY(addr)                                     \
{                                                               \
    int name = (addr >> 5) & 0x7FF;                             \
    if(bg_name_dirty[name] == 0) bg_name_list[bg_list_index++] = name; \
    bg_name_dirty[name] |= (1 << ((addr >> 2) & 0x07));         \
}

/* Tables that define the playfield layout */
uint8 shift_table[] = { 6, 7, 0, 8 };
uint8 col_mask_table[] = { 0x0F, 0x1F, 0x0F, 0x3F };
uint16 row_mask_table[] = { 0x0FF, 0x1FF, 0x2FF, 0x3FF };
uint32 y_mask_table[] = { 0x1FC0, 0x1F80, 0x1FC0, 0x1F00 };

/* DMA Timings Table 


    DMA Mode      Width       Display      Transfer Count
    -----------------------------------------------------
    68K > VDP     32-cell     Active       16
                              Blanking     167
                  40-cell     Active       18
                              Blanking     205
    VRAM Fill     32-cell     Active       15
                              Blanking     166
                  40-cell     Active       17
                              Blanking     204
    VRAM Copy     32-cell     Active       8
                              Blanking     83
                  40-cell     Active       9
                              Blanking     102

*/

uint8 dmarate_table[16] = {
	83 , 102,  8,  9,    // M68k to VRAM
	167, 205, 16, 18,    // M68k to VSRAM or CRAM
	166, 204, 15, 17,    // DMA fill
    83 , 102,  8,  9     // DMA Copy
};

uint8 sat[0x400];		/* Internal copy of sprite attribute table */
uint8 vram[0x10000];		/* Video RAM (64Kx8) */
uint8 cram[0x80];		/* On-chip color RAM (64x9) */
uint8 vsram[0x80];		/* On-chip vertical scroll RAM (40x11) */
uint8 reg[0x20];		/* Internal VDP registers (23x8) */
uint16 addr;			/* Address register */
uint16 addr_latch;		/* Latched A15, A14 of address */
uint8 code;			/* Code register */
uint8 pending;			/* Pending write flag */
uint16 status;			/* VDP status flags */
uint16 ntab;			/* Name table A base address */
uint16 ntbb;			/* Name table B base address */
uint16 ntwb;			/* Name table W base address */
uint16 satb;			/* Sprite attribute table base address */
uint16 hscb;			/* Horizontal scroll table base address */
uint16 sat_base_mask;		/* Base bits of SAT */
uint16 sat_addr_mask;		/* Index bits of SAT */
uint8 border;			/* Border color index */
uint8 bg_name_dirty[0x800];	/* 1= This pattern is dirty */
uint16 bg_name_list[0x800];	/* List of modified pattern indices */
uint16 bg_list_index;		/* # of modified patterns in list */
uint8 bg_pattern_cache[0x80000];	/* Cached and flipped patterns */
uint8 playfield_shift;		/* Width of planes A, B (in bits) */
uint8 playfield_col_mask;	/* Vertical scroll mask */
uint16 playfield_row_mask;	/* Horizontal scroll mask */
uint32 y_mask;			/* Name table Y-index bits mask */
uint8 hint_pending;		    /* 0= Line interrupt is pending */
uint8 vint_pending;		    /* 1= Frame interrupt is pending */
int16 h_counter;			/* Raster counter */
int16 hc_latch;				/* latched HCounter (INT2) */
uint16 v_counter;			/* VDP scan line counter */
uint8 im2_flag;			    /* 1= Interlace mode 2 is being used */
uint16 frame_end;			/* End-of-frame (IRQ line) */
uint8 dmafill;			    /* 1= DMA fill has been requested */
uint32 dma_endCycles;       /* DMA ending cycles count */
uint8 vdp_pal   = 0 ;       /* CPU mode (NTSC by default) */
uint8 vdp_rate = 60;        /* CPU speed (60Hz by default)*/
void (*color_update) (int index, uint16 data);

uint8 dmatiming = 1;
uint8 vdptiming = 0;

/*--------------------------------------------------------------------------*/
/* Init, reset, shutdown functions                                          */
/*--------------------------------------------------------------------------*/
void vdp_init (void)
{}

void vdp_reset (void)
{
  memset ((char *) sat, 0, sizeof (sat));
  memset ((char *) vram, 0, sizeof (vram));
  memset ((char *) cram, 0, sizeof (cram));
  memset ((char *) vsram, 0, sizeof (vsram));
  memset ((char *) reg, 0, sizeof (reg));

  addr = addr_latch = code = pending = 0;
  ntab = ntbb = ntwb = satb = hscb = 0;
  sat_base_mask = 0xFE00;
  sat_addr_mask = 0x01FF;

  /* Mark all colors as dirty to force a palette update */
  border = 0x00;

  memset ((char *) bg_name_dirty, 0, sizeof (bg_name_dirty));
  memset ((char *) bg_name_list, 0, sizeof (bg_name_list));
  bg_list_index = 0;
  memset ((char *) bg_pattern_cache, 0, sizeof (bg_pattern_cache));

  playfield_shift = 6;
  playfield_col_mask = 0x0F;
  playfield_row_mask = 0x0FF;
  y_mask = 0x1FC0;

  hint_pending = vint_pending = 0;
  h_counter = 0;
  hc_latch = -1;
  v_counter = 0;
  dmafill = 0;
  im2_flag = 0;
  frame_end = 0xE0;
  dma_endCycles = 0;
  
  status = 0x3600; /* fifo empty */

  /* Initialize viewport */
  bitmap.viewport.x = 0x20;
  bitmap.viewport.y = 0x20;
  bitmap.viewport.w = 256;
  bitmap.viewport.h = 224;
  bitmap.viewport.oh = 256;
  bitmap.viewport.ow = 224;
  bitmap.viewport.changed = 1;
}

void vdp_shutdown (void)
{}

/*--------------------------------------------------------------------------*/
/* DMA Operations                                                           */
/*--------------------------------------------------------------------------*/

/* DMA Timings */
/* timing type = 0 (Copy to VRAM), 1 (Copy to VSRAM or CRAM), 2 (VRAM Fill), 3 (VRAM Copy) */
void dma_update(int type, int length)
{
	uint32 dma_cycles;
	uint8 index = 4 * type; /* DMA timing type */

	if (!dmatiming) return; 
	
	/* get the appropriate tranfer rate (bytes/line) for this DMA operation */
	if (!(status&8) && (reg[1]&0x40)) index += 2; /* not in VBLANK and Display ON */
	index += (reg[12] & 1);                       /* 32 or 40 Horizontal Cells    */
	
	/* determinate associated 68kcycles number */
	dma_cycles = (misc68Kcycles * length ) / dmarate_table[index];
	
	if (type < 2)
	{
		/* 68K COPY to V-RAM */
		/* 68K is frozen during DMA operation */
		m68k_freeze(dma_cycles);
    }
	else
	{
		/* VRAM Fill or VRAM Copy */
		/* we keep the number of cycles executed so far */
		/* used for DMA Busy flag update on status read */
		dma_endCycles = count_m68k + m68k_cycles_run() + dma_cycles;
		dma_m68k = 0;
		
		/* set DMA Busy flag */
	    status |= 0x0002;
	}
}

/*  VRAM to VRAM copy
    Read byte from VRAM (source), write to VRAM (addr),
    bump source and add r15 to addr.

    - see how source addr is affected
      (can it make high source byte inc?) */
void dma_copy (void)
{
  int length = (reg[20] << 8 | reg[19]) & 0xFFFF;
  int source = (reg[22] << 8 | reg[21]) & 0xFFFF;
  if (!length) length = 0x10000;

  dma_update(3,length);

  do
  {
	  vram[addr] = vram[source];
      MARK_BG_DIRTY (addr);
      source = (source + 1) & 0xFFFF;
      addr += reg[15];
  }
  while (--length);

  reg[19] = length & 0xFF;
  reg[20] = (length >> 8) & 0xFF;
  reg[21] = source & 0xFF; /* not sure */
  reg[22] = (source >> 8) & 0xFF; 
}


/* 68K Copy to VRAM, VSRAM or CRAM */
void dma_vbus (void)
{
  uint32 base, source = ((reg[23] & 0x7F) << 17 | reg[22] << 9 | reg[21] << 1) & 0xFFFFFE;
  uint32 length = (reg[20] << 8 | reg[19]) & 0xFFFF;
  uint8 old_vdptiming = vdptiming;

  if (!length) length = 0x10000;
  base = source;

  /* DMA timings */
  if ((code & 0x0F) == 0x01) dma_update(0,length); /* VRAM */
  else dma_update(1,length); /* CRAM & VSRAM */
  vdptiming = 0;
  
  do
  {
      uint16 temp = vdp_dma_r (source);
      source += 2;
      source = ((base & 0xFE0000) | (source & 0x1FFFF));
      vdp_data_w (temp);
  }
  while (--length);

  vdptiming = old_vdptiming;
  reg[19] = length & 0xFF;
  reg[20] = (length >> 8) & 0xFF;
  reg[21] = (source >> 1) & 0xFF;
  reg[22] = (source >> 9) & 0xFF;
  reg[23] = (reg[23] & 0x80) | ((source >> 17) & 0x7F);
}

/* VRAM FILL */
void dma_fill(uint16 data)
{
	int length = (reg[20] << 8 | reg[19]) & 0xFFFF;
	
	if (!length) length = 0x10000;

	dma_update(2, length);
	WRITE_BYTE(vram, addr, data & 0xFF);

    do
	{
		WRITE_BYTE(vram, addr^1, (data >> 8) & 0xFF);
	    MARK_BG_DIRTY (addr);
	    addr += reg[15];
	}
    while (--length);

    reg[19] = length & 0xFF;
    reg[20] = (length >> 8) & 0xFF;
    dmafill = 0;
}


/*--------------------------------------------------------------------------*/
/* Memory access functions                                                  */
/*--------------------------------------------------------------------------*/

void vdp_ctrl_w (uint16 data)
{
	if (pending == 0)
    {
		if ((data & 0xC000) == 0x8000)
	    {
			uint8 r = (data >> 8) & 0x1F;
	       	uint8 d = data & 0xFF;
	        vdp_reg_w (r, d);
	    }
        else pending = 1;

        addr = ((addr_latch & 0xC000) | (data & 0x3FFF)) & 0xFFFF;
        code = ((code & 0x3C) | ((data >> 14) & 0x03)) & 0x3F;
    }
    else
    {
      /* Clear pending flag */
      pending = 0;

      /* Update address and code registers */
      addr = ((addr & 0x3FFF) | ((data & 3) << 14)) & 0xFFFF;
      code = ((code & 0x03) | ((data >> 2) & 0x3C)) & 0x3F;

      /* Save address bits A15 and A14 */
      addr_latch = (addr & 0xC000);

      if ((code & 0x20) && (reg[1] & 0x10))
	  {
		  switch (reg[23] & 0xC0)
	      {
			  case 0x00:		/* V bus to VDP DMA */
	          case 0x40:		/* V bus to VDP DMA */
			     dma_vbus ();
	              break;

	          case 0x80:		/* VRAM fill */
	             dmafill = 1;
	             break;

	          case 0xC0:		/* VRAM copy */
	             dma_copy ();
	             break;
	      }
	  }
   }
}

uint16 vdp_ctrl_r (void)
{
     /**
     * Return vdp status
     *
     * Bits are
     * 0 	0:1 ntsc:pal
     * 1	DMA Busy
     * 2	During HBlank
     * 3	During VBlank
     * 4	Frame Interlace 0:even 1:odd
     * 5	Sprite collision
     * 6	Too many sprites per line
     * 7	v interrupt occurred
     * 8	Write FIFO full
     * 9	Write FIFO empty
     * 10 - 15	Next word on bus
     */

  uint16 temp;
  int dma_lastCycles = dma_endCycles - (dma_m68k + count_m68k + m68k_cycles_run());
  
  /* reset DMA status flag if needed */
  if ((status & 0x0002) && (dma_lastCycles <= 0)) status &= 0xFFFD;

  temp = status | vdp_pal;
  pending = 0;
  
  /* reset status */
  status ^= 0x0300;     /* toggle FIFO status */
  status &= 0xFF9F;		/* clear sprite overflow  & sprite collision */
  if (!(status & 8)) status &= ~0x0080; /* not in VBLANK: clear vint flag */
  
  if (!(reg[1] & 0x40)) temp |= 0x8; /* no display => in VBLANK*/
  
  return (temp);
}


void vdp_data_w (uint16 data)
{
  /* Clear pending flag */
  pending = 0;

  if (dmafill)
  {
	  dma_fill(data);
	  return;
  }
  
  /* delays VDP RAM access */
  /* hack for Chaos Engine / Soldiers of Fortune */
  if (vdptiming && !(status&8) && (reg[1]&0x40)) 
  {
     if (reg[12] & 1) m68k_freeze(30); // 40 cell
     else m68k_freeze(27); // 32 cell
  }

  switch (code & 0x0F)
  {
    case 0x01:			/* VRAM */
      	  
      /* Byte-swap data if A0 is set */
      if (addr & 1) data = (data >> 8) | (data << 8);

      /* Copy SAT data to the internal SAT */
      if ((addr & sat_base_mask) == satb)
	  {
	    *(uint16 *) & sat[addr & sat_addr_mask] = data;
	  }

      /* Only write unique data to VRAM */
      if (data != *(uint16 *) & vram[addr & 0xFFFE])
	  {
	    /* Write data to VRAM */
	    *(uint16 *) & vram[addr & 0xFFFE] = data;

	    /* Update the pattern cache */
	    MARK_BG_DIRTY (addr);
	  }
      break;

    case 0x03:			/* CRAM */
	{
		uint16 *p = (uint16 *) & cram[(addr & 0x7E)];
		data = PACK_CRAM (data & 0x0EEE);
	    if (data != *p)
	    {
			int index = (addr >> 1) & 0x3F;
	        *p = data;
			color_update (index, *p);
			color_update (0x00, *(uint16 *)&cram[border << 1]);
	     }
	}
    break;

    case 0x05:			/* VSRAM */
      *(uint16 *) & vsram[(addr & 0x7E)] = data;
      break;
  }

  /* Bump address register */
  addr += reg[15];
 
}


uint16 vdp_data_r (void)
{
  uint16 temp = 0;

  /* Clear pending flag */
  pending = 0;

  switch (code & 0x0F)
    {
    case 0x00:			/* VRAM */
      temp = *(uint16 *) & vram[(addr & 0xFFFE)];
      break;

    case 0x08:			/* CRAM */
      temp = *(uint16 *) & cram[(addr & 0x7E)];
      temp = UNPACK_CRAM (temp);
      break;

    case 0x04:			/* VSRAM */
      temp = *(uint16 *) & vsram[(addr & 0x7E)];
      break;
    }

  /* Bump address register */
  addr += reg[15];

  /* return data */
  return (temp);
}


/*
    The reg[] array is updated at the *end* of this function, so the new
    register data can be compared with the previous data.
*/
void vdp_reg_w (uint8 r, uint8 d)
{
  switch (r)
  {
    case 0x00:			/* CTRL #1 */
	  if (!(d & 0x02)) hc_latch = -1;
	  break;

    case 0x01:			/* CTRL #2 */
	  /* Change the frame timing */
      frame_end = (d & 8) ? 0xF0 : 0xE0;

      /* Check if the viewport height has actually been changed */
      if ((reg[1] & 8) != (d & 8))
	  {
		  /* Update the height of the viewport */
		  bitmap.viewport.oh = bitmap.viewport.h;
		  bitmap.viewport.h = (d & 8) ? 240 : 224;
		  bitmap.viewport.changed = 1;
	  }
	  break;

    case 0x02:			/* NTAB */
      ntab = (d << 10) & 0xE000;
      break;

    case 0x03:			/* NTWB */
      ntwb = (d << 10) & ((reg[12] & 1) ? 0xF000 : 0xF800);
      break;

    case 0x04:			/* NTBB */
      ntbb = (d << 13) & 0xE000;
      break;

    case 0x05:			/* SATB */
      sat_base_mask = (reg[12] & 1) ? 0xFC00 : 0xFE00;
      sat_addr_mask = (reg[12] & 1) ? 0x03FF : 0x01FF;
      satb = (d << 9) & sat_base_mask;
      break;

    case 0x07:
      d &= 0x3F;

      /* Check if the border color has actually changed */
      if (border != d)
	  {
		  /* Mark the border color as modified */
		  border = d;
		  color_update (0x00, *(uint16 *) & cram[(border << 1)]);
	  }
      break;

	case 0x0C:
     /* Check if the viewport width has actually been changed */
     if ((reg[0x0C] & 1) != (d & 1))
	 {
	   /* Update the width of the viewport */
	   bitmap.viewport.ow = bitmap.viewport.w;
	   bitmap.viewport.w = (d & 1) ? 320 : 256;
	   bitmap.viewport.changed = 1;
	 }

     /* See if the S/TE mode bit has changed */
     if ((reg[0x0C] & 8) != (d & 8))
	 {
	   int i;
	   reg[0x0C] = d;

	   /* Update colors */
	   for (i = 0; i < 0x40; i += 1) color_update (i, *(uint16 *) & cram[i << 1]);
	   color_update (0x00, *(uint16 *) & cram[border << 1]);
	 }

     /* Check interlace mode 2 setting */
     im2_flag = ((d & 0x06) == 0x06) ? 1 : 0;

     /* The following register updates check this value */
     reg[0x0C] = d;

     /* Update display-dependant registers */
     vdp_reg_w (0x03, reg[0x03]);
     vdp_reg_w (0x05, reg[0x05]);
     break;

   case 0x0D:			/* HSCB */
     hscb = (d << 10) & 0xFC00;
     break;

   case 0x10:			/* Playfield size */
     playfield_shift = shift_table[(d & 3)];
     playfield_col_mask = col_mask_table[(d & 3)];
     playfield_row_mask = row_mask_table[(d >> 4) & 3];
     y_mask = y_mask_table[(d & 3)];
     break;
 }

 /* Write new register value */
 reg[r] = d;
}


uint16 vdp_hvc_r (void)
{
  int cycles = (count_m68k + m68k_cycles_run ()) % misc68Kcycles;
  uint8 *hctab = (reg[12] & 1) ? cycle2hc40 : cycle2hc32;
  uint16 *vctab = (vdp_pal) ? vc_pal_224 : vc_ntsc_224;
  if ((reg[1] & 8) && vdp_pal) vctab = vc_pal_240;
  
  uint8 hc = (hc_latch == -1) ? hctab[cycles] : (hc_latch&0xFF);

  uint16 vc =  vctab[v_counter];
 
  /* interlace mode 2 */
  if (im2_flag) vc = (vc & 0xFE) | ((vc >> 8) & 0x01);

  return (((vc << 8)&0xFF00) | hc);
}


void vdp_test_w (uint16 value)
{}

int vdp_int_ack_callback (int int_level)
{
	if (vint_pending) vint_pending = 0;
	else hint_pending = 0;
	if (!hint_pending && !vint_pending) m68k_set_irq(0);
    return M68K_INT_ACK_AUTOVECTOR;
}
