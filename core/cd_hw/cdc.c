/***************************************************************************************
 *  Genesis Plus
 *  CD data controller (LC8951x compatible)
 *
 *  Copyright (C) 2012-2024  Eke-Eke (Genesis Plus GX)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

#include "shared.h"

/* IFSTAT register bitmasks */
#define BIT_DTEI  0x40
#define BIT_DECI  0x20
#define BIT_DTBSY 0x08
#define BIT_DTEN  0x02

/* IFCTRL register bitmasks */
#define BIT_DTEIEN  0x40
#define BIT_DECIEN  0x20
#define BIT_DOUTEN  0x02

/* CTRL0 register bitmasks */
#define BIT_DECEN   0x80
#define BIT_AUTORQ  0x10
#define BIT_WRRQ    0x04

/* CTRL1 register bitmasks */
#define BIT_MODRQ   0x08
#define BIT_FORMRQ  0x04
#define BIT_SHDREN  0x01

/* STAT3 register bitmask */
#define BIT_VALST   0x80

/* DMA transfer rate */
/* min. 4 x SUB-CPU cycles (i.e 16 x SCD cycles) per byte (cf https://github.com/MiSTer-devel/MegaCD_MiSTer/tree/master/docs/mcd%20logs) */
/* additional delays caused by SUB-CPU access & periodic refresh (all RAM), GPU operation (Word-RAM only) or PCM playback (PCM RAM only) */
/* are not emulated */
#define DMA_CYCLES_PER_BYTE 16

void cdc_init(void)
{
  memset(&cdc, 0, sizeof(cdc_t));

  /* autodetect CDC configuration */
  if ((scd.type == CD_TYPE_WONDERMEGA_M2) || (scd.type == CD_TYPE_CDX))
  {
    /* LC89513K chip (Wondermega M2 / X'Eye / CDX / Multi-Mega) */
    cdc.ar_mask = 0x1f;
  }
  else 
  {
    /* LC8951 or LC89515 chip (default)*/
    cdc.ar_mask = 0x0f;
  }
}

void cdc_reset(void)
{
  /* reset CDC register index */
  scd.regs[0x04>>1].byte.l = 0x00;

  /* reset CDC registers */
  cdc.ifstat  = 0xff;
  cdc.ifctrl  = 0x00;
  cdc.ctrl[0] = 0x00;
  cdc.ctrl[1] = 0x00;
  cdc.stat[0] = 0x00;
  cdc.stat[1] = 0x00;
  cdc.stat[2] = 0x00;
  cdc.stat[3] = 0x80;
  cdc.head[0][0] = 0x00;
  cdc.head[0][1] = 0x00;
  cdc.head[0][2] = 0x00;
  cdc.head[0][3] = 0x01;
  cdc.head[1][0] = 0x00;
  cdc.head[1][1] = 0x00;
  cdc.head[1][2] = 0x00;
  cdc.head[1][3] = 0x00;

  /* reset CDC DMA & decoder cycle counters */
  cdc.cycles[0] = cdc.cycles[1] = 0;

  /* disable CDC DMA */
  cdc.dma_w = cdc.halted_dma_w = 0;

  /* reset CDC IRQ state */
  cdc.irq = 0;

  /* clear any pending IRQ */
  if (scd.pending & (1 << 5))
  {
    /* clear any pending interrupt level 5 */
    scd.pending &= ~(1 << 5);

    /* update IRQ level */
    s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
  }
}

int cdc_context_save(uint8 *state)
{
  uint8 tmp8;
  int bufferptr = 0;

  if (cdc.dma_w == pcm_ram_dma_w)
  {
    tmp8 = 1;
  }
  else if (cdc.dma_w == prg_ram_dma_w)
  {
    tmp8 = 2;
  }
  else if (cdc.dma_w == word_ram_0_dma_w)
  {
    tmp8 = 3;
  }
  else if (cdc.dma_w == word_ram_1_dma_w)
  {
    tmp8 = 4;
  }
  else if (cdc.dma_w == word_ram_2M_dma_w)
  {
    tmp8 = 5;
  }
  else if (cdc.halted_dma_w == prg_ram_dma_w)
  {
    tmp8 = 6;
  }
  else if (cdc.halted_dma_w == word_ram_2M_dma_w)
  {
    tmp8 = 7;
  }
  else
  {
    tmp8 = 0;
  }

  save_param(&cdc.ifstat, sizeof(cdc.ifstat));
  save_param(&cdc.ifctrl, sizeof(cdc.ifctrl));
  save_param(&cdc.dbc, sizeof(cdc.dbc));
  save_param(&cdc.dac, sizeof(cdc.dac));
  save_param(&cdc.pt, sizeof(cdc.pt));
  save_param(&cdc.wa, sizeof(cdc.wa));
  save_param(&cdc.ctrl, sizeof(cdc.ctrl));
  save_param(&cdc.head, sizeof(cdc.head));
  save_param(&cdc.stat, sizeof(cdc.stat));
  save_param(&cdc.cycles, sizeof(cdc.cycles));
  save_param(&cdc.ram, sizeof(cdc.ram));
  save_param(&tmp8, 1);

  return bufferptr;
}

int cdc_context_load(uint8 *state)
{
  uint8 tmp8;
  int bufferptr = 0;

  load_param(&cdc.ifstat, sizeof(cdc.ifstat));
  load_param(&cdc.ifctrl, sizeof(cdc.ifctrl));
  load_param(&cdc.dbc, sizeof(cdc.dbc));
  load_param(&cdc.dac, sizeof(cdc.dac));
  load_param(&cdc.pt, sizeof(cdc.pt));
  load_param(&cdc.wa, sizeof(cdc.wa));
  load_param(&cdc.ctrl, sizeof(cdc.ctrl));
  load_param(&cdc.head, sizeof(cdc.head));
  load_param(&cdc.stat, sizeof(cdc.stat));
  load_param(&cdc.cycles, sizeof(cdc.cycles));
  load_param(&cdc.ram, sizeof(cdc.ram));

  load_param(&tmp8, 1);

  switch (tmp8)
  {
    case 1:
      cdc.dma_w = pcm_ram_dma_w;
      cdc.halted_dma_w = 0;
      break;
    case 2:
      cdc.dma_w = prg_ram_dma_w;
      cdc.halted_dma_w = 0;
      break;
    case 3:
      cdc.dma_w = word_ram_0_dma_w;
      cdc.halted_dma_w = 0;
      break;
    case 4:
      cdc.dma_w = word_ram_1_dma_w;
      cdc.halted_dma_w = 0;
      break;
    case 5:
      cdc.dma_w = word_ram_2M_dma_w;
      cdc.halted_dma_w = 0;
      break;
    case 6:
      cdc.dma_w = 0;
      cdc.halted_dma_w = prg_ram_dma_w;
      break;
    case 7:
      cdc.dma_w = 0;
      cdc.halted_dma_w = word_ram_2M_dma_w;
      break;
    default:
      cdc.dma_w = 0;
      cdc.halted_dma_w = 0;
      break;
  }

  cdc.irq = ~cdc.ifstat & cdc.ifctrl & (BIT_DTEIEN | BIT_DECIEN);

  return bufferptr;
}

void cdc_dma_init(void)
{
  /* no effect if data transfer is not started */
  if (cdc.ifstat & BIT_DTEN)
    return;
  
  /* disable DMA by default */
  cdc.dma_w = cdc.halted_dma_w = 0;

  /* check data transfer destination */
  switch (scd.regs[0x04>>1].byte.h & 0x07)
  {
    case 2: /* MAIN-CPU host read */
    case 3: /* SUB-CPU host read */
    {
      /* read 16-bit word from CDC RAM buffer (big-endian format) into gate-array register $08 */
      /* Note: on real-hardware, 16-bit word is not immediately available, cf. https://github.com/MiSTer-devel/MegaCD_MiSTer/blob/master/docs/mcd%20logs/dma_sub_read.jpg for transfer timings */
      scd.regs[0x08>>1].w = READ_WORD(cdc.ram, cdc.dac.w & 0x3ffe);
#ifdef LOG_CDC
      error("CDC host read 0x%04x -> 0x%04x (dbc=0x%x) (%X)\n", cdc.dac.w, scd.regs[0x08>>1].w, cdc.dbc.w, s68k.pc);
#endif

      /* set DSR bit (gate-array register $04) */
      scd.regs[0x04>>1].byte.h |= 0x40;

      /* increment data address counter */
      cdc.dac.w += 2;

      /* decrement data byte counter */
      cdc.dbc.w -= 2;

      /* end of transfer ? */
      if ((int16)cdc.dbc.w < 0)
      {
        /* reset data byte counter (DBCH bits 4-7 should also be set to 1) */
        cdc.dbc.w = 0xffff;

        /* clear !DTEN and !DTBSY */
        cdc.ifstat |= (BIT_DTBSY | BIT_DTEN);

        /* pending Data Transfer End interrupt */
        cdc.ifstat &= ~BIT_DTEI;

        /* Data Transfer End interrupt enabled ? */
        if (cdc.ifctrl & BIT_DTEIEN)
        {
          /* check end of CDC decoder active period */
          if ((cdc.irq & BIT_DECI) && (cdc.cycles[0] > cdc.cycles[1]))
          {
            /* clear pending decoder interrupt */
            cdc.ifstat |= BIT_DECI;

            /* update CDC IRQ state */
            cdc.irq &= ~BIT_DECI;
          }

          /* level 5 interrupt triggered only on CDC /INT falling edge with interrupt enabled on gate-array side */
          if (!cdc.irq && (scd.regs[0x32>>1].byte.l & 0x20))
          {
            /* pending level 5 interrupt */
            scd.pending |= (1 << 5);

            /* update IRQ level */
            s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
          }

          /* update CDC IRQ state */
          cdc.irq |= BIT_DTEI;
        }

        /* set EDT bit (gate-array register $04) */
        scd.regs[0x04>>1].byte.h |= 0x80;
      }

      break;
    }

    case 4: /* PCM RAM DMA */
    {
      cdc.dma_w = pcm_ram_dma_w;
      break;
    }

    case 5: /* PRG-RAM DMA */
    {
      /* check if MAIN-CPU has access to PRG-RAM */
      if (scd.regs[0x00].byte.l & 0x02)
      {
        /* halt DMA to PRG-RAM */
        cdc.halted_dma_w = prg_ram_dma_w;
      }
      else
      {
        /* enable DMA to PRG-RAM */
        cdc.dma_w = prg_ram_dma_w;
      }
      break;
    }

    case 7: /* Word-RAM DMA */
    {
      /* check memory mode */
      if (scd.regs[0x02 >> 1].byte.l & 0x04)
      {
        /* 1M mode */
        if (scd.regs[0x02 >> 1].byte.l & 0x01)
        {
          /* Word-RAM bank 0 is assigned to SUB-CPU */
          cdc.dma_w = word_ram_0_dma_w;
        }
        else
        {
          /* Word-RAM bank 1 is assigned to SUB-CPU */
          cdc.dma_w = word_ram_1_dma_w;
        }
      }
      else
      {
        /* check if MAIN-CPU has access to 2M Word-RAM */
        if (scd.regs[0x02 >> 1].byte.l & 0x01)
        {
          /* halt DMA to 2M Word-RAM */
          cdc.halted_dma_w = word_ram_2M_dma_w;
        }
        else
        {
          /* enable DMA to 2M Word-RAM */
          cdc.dma_w = word_ram_2M_dma_w;
        }
      }
      break;
    }

    default: /* invalid */
    {
#ifdef LOG_CDC
      error("invalid CDC transfer destination (%d)\n", scd.regs[0x04>>1].byte.h & 0x07);
#endif
      break;
    }
  }
}

void cdc_dma_update(unsigned int cycles)
{
  /* max number of bytes that can be transfered */
  int dma_bytes = (cycles - cdc.cycles[0] + DMA_CYCLES_PER_BYTE - 1) / DMA_CYCLES_PER_BYTE;

  /* always process blocks of 8 bytes */
  dma_bytes = (dma_bytes / 8) * 8;

  /* end of DMA transfer ? */
  if (cdc.dbc.w < dma_bytes)
  {
    /* transfer remaining bytes using DMA */
    cdc.dma_w(cdc.dbc.w + 1);

    /* update DMA cycle counter */
    cdc.cycles[0] += (cdc.dbc.w + 1) * DMA_CYCLES_PER_BYTE;

    /* reset data byte counter (DBCH bits 4-7 should also be set to 1) */
    cdc.dbc.w = 0xffff;

    /* clear !DTEN and !DTBSY */
    cdc.ifstat |= (BIT_DTBSY | BIT_DTEN);

    /* pending Data Transfer End interrupt */
    cdc.ifstat &= ~BIT_DTEI;

    /* Data Transfer End interrupt enabled ? */
    if (cdc.ifctrl & BIT_DTEIEN)
    {
      /* check end of CDC decoder active period */
      if ((cdc.irq & BIT_DECI) && (cdc.cycles[0] > cdc.cycles[1]))
      {
        /* clear pending decoder interrupt */
        cdc.ifstat |= BIT_DECI;

        /* update CDC IRQ state */
        cdc.irq &= ~BIT_DECI;
      }

      /* level 5 interrupt triggered only on CDC /INT falling edge with interrupt enabled on gate-array side*/
      if (!cdc.irq && (scd.regs[0x32>>1].byte.l & 0x20))
      {
        /* pending level 5 interrupt */
        scd.pending |= (1 << 5);

        /* update IRQ level */
        s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
      }

      /* update CDC IRQ state */
      cdc.irq |= BIT_DTEI;
    }

    /* clear DSR bit & set EDT bit (CD register $04) */
    scd.regs[0x04>>1].byte.h = (scd.regs[0x04>>1].byte.h & 0x07) | 0x80;

    /* SUB-CPU idle on register $04 polling ? */
    if (s68k.stopped & (1<<0x04))
    {
      /* sync SUB-CPU with CDC DMA (only if not already ahead) */
      if (s68k.cycles < cdc.cycles[0])
      {
        s68k.cycles = cdc.cycles[0];
      }

      /* restart SUB-CPU */
      s68k.stopped = 0;
#ifdef LOG_SCD
      error("s68k started from %d cycles\n", s68k.cycles);
#endif
    }

    /* disable DMA */
    cdc.dma_w = cdc.halted_dma_w = 0;
  }
  else if (dma_bytes > 0)
  {
    /* transfer limited amount of bytes using DMA */
    cdc.dma_w(dma_bytes);

    /* decrement data byte counter */
    cdc.dbc.w -= dma_bytes;

    /* update DMA cycle counter */
    cdc.cycles[0] += dma_bytes * DMA_CYCLES_PER_BYTE;
  }
}

void cdc_decoder_update(uint32 header)
{
  /* data decoding enabled ? */
  if (cdc.ctrl[0] & BIT_DECEN)
  {
    /* update HEADx registers with current block header */
    *(uint32 *)(cdc.head[0]) = header;

    /* set !VALST */
    cdc.stat[3] = 0x00;

    /* pending decoder interrupt */
    cdc.ifstat &= ~BIT_DECI;

    /* update CDC decoder end cycle (value adjusted for MCD-verificator CDC FLAGS Tests #40 & #41) */
    cdc.cycles[1] = s68k.cycles + 269000;

    /* decoder interrupt enabled ? */
    if (cdc.ifctrl & BIT_DECIEN)
    {
      /* level 5 interrupt triggered only on CDC /INT falling edge with interrupt enabled on gate-array side */
      /* note: only check DTEI as DECI is cleared automatically between decoder interrupt triggering */
      if (!(cdc.irq & BIT_DTEI) && (scd.regs[0x32>>1].byte.l & 0x20))
      {
        /* pending level 5 interrupt */
        scd.pending |= (1 << 5);

        /* update IRQ level */
        s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
      }

      /* update CDC IRQ state */
      cdc.irq |= BIT_DECI;
    }

    /* buffer RAM write enabled ? */
    if (cdc.ctrl[0] & BIT_WRRQ)
    {
      int offset;

      /* increment block pointer  */
      cdc.pt.w += 2352;

      /* increment write address */
      cdc.wa.w += 2352;

      /* CDC buffer address */
      offset = cdc.pt.w & 0x3fff;

      /* write current block header to RAM buffer (4 bytes) */
      *(uint32 *)(cdc.ram + offset) = header;
      offset += 4;

      /* check decoded block mode */
      if (cdc.head[0][3] == 0x01)
      {
        /* write Mode 1 user data to RAM buffer (2048 bytes) */
        cdd_read_data(cdc.ram + offset, NULL);
        offset += 2048;
      }
      else
      {
        /* check if CD-ROM Mode 2 decoding is enabled */
        if (cdc.ctrl[1] & BIT_MODRQ)
        {
          /* update HEADx registers with current block sub-header & write Mode 2 user data to RAM buffer (max 2328 bytes) */
          cdd_read_data(cdc.ram + offset + 8, cdc.head[1]);

          /* write current block sub-header to RAM buffer (4 bytes x 2) */
          *(uint32 *)(cdc.ram + offset) = *(uint32 *)(cdc.head[1]);
          *(uint32 *)(cdc.ram + offset + 4) = *(uint32 *)(cdc.head[1]);
          offset += 2336;
        }
        else
        {
          /* update HEADx registers with current block sub-header & write Mode 2 user data to RAM buffer (max 2328 bytes) */
          /* NB: when Mode 2 decoding is disabled, sub-header is apparently not written to RAM buffer (required by Wonder Library) */
          cdd_read_data(cdc.ram + offset, cdc.head[1]);
          offset += 2328;
        }

        /* set STAT2 register FORM bit according to sub-header FORM bit when CTRL0 register AUTORQ bit is set */
        if (cdc.ctrl[0] & BIT_AUTORQ)
        {
          cdc.stat[2] = (cdc.ctrl[1] & BIT_MODRQ) | ((cdc.head[1][2] & 0x20) >> 3);
        }
      }

      /* take care of buffer overrun */
      if (offset > 0x4000)
      {
        /* data should be written at the start of buffer */
        memcpy(cdc.ram, cdc.ram + 0x4000, offset - 0x4000);
      }
    }
  }
}

void cdc_reg_w(unsigned char data)
{
#ifdef LOG_CDC
  error("CDC register %d write 0x%04x (%X)\n", scd.regs[0x04>>1].byte.l, data, s68k.pc);
#endif
  switch (scd.regs[0x04>>1].byte.l)
  {
    case 0x01:  /* IFCTRL */
    {
      /* previous CDC IRQ state */
      uint8 prev_irq = cdc.irq;

      /* check end of CDC decoder active period */
      if (s68k.cycles > cdc.cycles[1])
      {
        /* clear pending decoder interrupt */
        cdc.ifstat |= BIT_DECI;

        /* update previous CDC IRQ state */
        prev_irq &= ~BIT_DECI;
      }
      
      /* update CDC IRQ state according to DTEIEN and DECIEN bits */
      cdc.irq = ~cdc.ifstat & data & (BIT_DTEIEN | BIT_DECIEN);

      /* level 5 interrupt is triggered on CDC /INT falling edge if interrupt enabled on gate-array side */
      if (cdc.irq && !prev_irq && (scd.regs[0x32>>1].byte.l & 0x20))
      {
        /* pending level 5 interrupt */
        scd.pending |= (1 << 5);

        /* update IRQ level */
        s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
      }

      /* abort any data transfer if data output is disabled */
      if (!(data & BIT_DOUTEN))
      {
        /* clear !DTBSY and !DTEN */
        cdc.ifstat |= (BIT_DTBSY | BIT_DTEN);

        /* disable DMA */
        cdc.dma_w = cdc.halted_dma_w = 0;
      }

      cdc.ifctrl = data;
      break;
    }

    case 0x02:  /* DBCL */
      cdc.dbc.byte.l = data;
      break;

    case 0x03:  /* DBCH */
      cdc.dbc.byte.h = data & 0x0f;
      break;

    case 0x04:  /* DACL */
      cdc.dac.byte.l = data;
      break;

    case 0x05:  /* DACH */
      cdc.dac.byte.h = data;
      break;

    case 0x06:  /* DTRG */
    {
      /* start data transfer if data output is enabled */
      if (cdc.ifctrl & BIT_DOUTEN)
      {
        /* set !DTBSY and !DTEN */
        cdc.ifstat &= ~(BIT_DTBSY | BIT_DTEN);

        /* clear EDT & DSR bits (gate-array register $04) */
        scd.regs[0x04>>1].byte.h &= 0x07;

        /* initialize data transfer destination */
        cdc_dma_init();

        /* initialize DMA cycle counter */
        cdc.cycles[0] = s68k.cycles;
      }

      break;
    }

    case 0x07:  /* DTACK */
    {
      /* clear pending data transfer end interrupt */
      cdc.ifstat |= BIT_DTEI;

      /* update CDC IRQ state */
      cdc.irq &= ~BIT_DTEI;
      break;
    }

    case 0x08:  /* WAL */
      cdc.wa.byte.l = data;
      break;

    case 0x09:  /* WAH */
      cdc.wa.byte.h = data;
      break;

    case 0x0a:  /* CTRL0 */
    {
      /* set CRCOK bit only if decoding is enabled */
      cdc.stat[0] = data & BIT_DECEN;

      /* decoding disabled ? */
      if (!(data & BIT_DECEN))
      {
        /* clear pending decoder interrupt */
        cdc.ifstat |= BIT_DECI;

        /* update CDC IRQ state */
        cdc.irq &= ~BIT_DECI;
      }

      /* update STAT2 register */
      if (data & BIT_AUTORQ)
      {
        /* set MODE bit according to CTRL1 register MODRQ bit & set FORM bit according to sub-header FORM bit*/
        cdc.stat[2] = (cdc.ctrl[1] & BIT_MODRQ) | ((cdc.head[1][2] & 0x20) >> 3);
      }
      else 
      {
        /* set MODE & FORM bits according to CTRL1 register MODRQ & FORMRQ bits */
        cdc.stat[2] = cdc.ctrl[1] & (BIT_MODRQ | BIT_FORMRQ);
      }

      cdc.ctrl[0] = data;
      break;
    }

    case 0x0b:  /* CTRL1 */
    {
      /* update STAT2 register */
      if (cdc.ctrl[0] & BIT_AUTORQ)
      {
        /* set MODE bit according to CTRL1 register MODRQ bit & set FORM bit according to sub-header FORM bit*/
        cdc.stat[2] = (data & BIT_MODRQ) | ((cdc.head[1][2] & 0x20) >> 3);
      }
      else 
      {
        /* set MODE & FORM bits according to CTRL1 register MODRQ & FORMRQ bits */
        cdc.stat[2] = data & (BIT_MODRQ | BIT_FORMRQ);
      }

      cdc.ctrl[1] = data;
      break;
    }

    case 0x0c:  /* PTL */
      cdc.pt.byte.l = data;
      break;
  
    case 0x0d:  /* PTH */
      cdc.pt.byte.h = data;
      break;

    case 0x0f:  /* RESET */
      cdc_reset();
      break;

    default:  /* unemulated registers */
      break;
  }

  /* increment address register (except when register #0 is selected) */
  if (scd.regs[0x04>>1].byte.l)
  {
    scd.regs[0x04>>1].byte.l = (scd.regs[0x04>>1].byte.l + 1) & cdc.ar_mask;
  }
}

unsigned char cdc_reg_r(void)
{
  uint8 data;

  switch (scd.regs[0x04>>1].byte.l)
  {
    case 0x01:  /* IFSTAT */
    {
      /* check end of CDC decoder active period */
      if (s68k.cycles > cdc.cycles[1])
      {
        /* clear pending decoder interrupt */
        cdc.ifstat |= BIT_DECI;

        /* update CDC IRQ state */
        cdc.irq &= ~BIT_DECI;
      }

      data = cdc.ifstat;
      break;
    }

    case 0x02:  /* DBCL */
    {
      data = cdc.dbc.byte.l;
      break;
    }

    case 0x03:  /* DBCH */
    {
      data = cdc.dbc.byte.h;
      break;
    }

    case 0x04:  /* HEAD0 */
    {
      data = cdc.head[cdc.ctrl[1] & BIT_SHDREN][0];
      break;
    }

    case 0x05:  /* HEAD1 */
    {
      data = cdc.head[cdc.ctrl[1] & BIT_SHDREN][1];
      break;
    }

    case 0x06:  /* HEAD2 */
    {
      data = cdc.head[cdc.ctrl[1] & BIT_SHDREN][2];
      break;
    }

    case 0x07:  /* HEAD3 */
    {
      data = cdc.head[cdc.ctrl[1] & BIT_SHDREN][3];
      break;
    }

    case 0x08:  /* PTL */
    {
      data = cdc.pt.byte.l;
      break;
    }

    case 0x09:  /* PTH */
    {
      data = cdc.pt.byte.h;
      break;
    }

    case 0x0a:  /* WAL */
    {
      data = cdc.wa.byte.l;
      break;
    }

    case 0x0b:  /* WAH */
    {
      data = cdc.wa.byte.h;
      break;
    }

    case 0x0c: /* STAT0 */
    {
      data = cdc.stat[0];
      break;
    }

    case 0x0d: /* STAT1 (always return 0) */
    {
      data = 0x00;
      break;
    }

    case 0x0e:  /* STAT2 */
    {
      data = cdc.stat[2];
      break;
    }

    case 0x0f:  /* STAT3 */
    {
      data = cdc.stat[3];

      /* clear !VALST (note: this is not 100% correct but BIOS do not seem to care) */
      cdc.stat[3] = BIT_VALST;

      /* clear pending decoder interrupt */
      cdc.ifstat |= BIT_DECI;

      /* update CDC IRQ state */
      cdc.irq &= ~BIT_DECI;
      break;
    }

    default:  /* unemulated registers */
    {
      data = 0xff;
      break;
    }
  }

#ifdef LOG_CDC
  error("CDC register %d read 0x%02X (%X)\n", scd.regs[0x04>>1].byte.l, data, s68k.pc);
#endif

  /* increment address register (except when register #0 is selected) */
  if (scd.regs[0x04>>1].byte.l)
  {
    scd.regs[0x04>>1].byte.l = (scd.regs[0x04>>1].byte.l + 1) & cdc.ar_mask;
  }
  
  return data;
}

unsigned short cdc_host_r(uint8 cpu_access)
{
  /* read CDC buffered data (gate-array register $08) */
  uint16 data = scd.regs[0x08>>1].w;

  /* check if host data transfer is started for selected CPU */
  if ((scd.regs[0x04>>1].byte.h & 0x47) == cpu_access)
  {
    /* check if EDT bit (gate-array register $04) is set (host data transfer is finished) */
    if (scd.regs[0x04>>1].byte.h & 0x80)
    {
      /* clear DSR bit (gate-array register $04) */
      scd.regs[0x04>>1].byte.h &= ~0x40;
    }
    else
    {
      /* read next 16-bit word from CDC RAM buffer (big-endian format) into gate-array register $08 */
      /* Note: on real-hardware, 16-bit word is not immediately available, cf. https://github.com/MiSTer-devel/MegaCD_MiSTer/blob/master/docs/mcd%20logs/dma_sub_read.jpg for transfer timings */
      scd.regs[0x08>>1].w = READ_WORD(cdc.ram, cdc.dac.w & 0x3ffe);
#ifdef LOG_CDC
      error("CDC host read 0x%04x -> 0x%04x (dbc=0x%x) (%X)\n", cdc.dac.w, scd.regs[0x08>>1].w, cdc.dbc.w, s68k.pc);
#endif

      /* increment data address counter */
      cdc.dac.w += 2;

      /* decrement data byte counter */
      cdc.dbc.w -= 2;

      /* end of transfer ? */
      if ((int16)cdc.dbc.w < 0)
      {
        /* reset data byte counter (DBCH bits 4-7 should also be set to 1) */
        cdc.dbc.w = 0xffff;

        /* clear !DTEN and !DTBSY */
        cdc.ifstat |= (BIT_DTBSY | BIT_DTEN);

        /* pending Data Transfer End interrupt */
        cdc.ifstat &= ~BIT_DTEI;

        /* Data Transfer End interrupt enabled ? */
        if (cdc.ifctrl & BIT_DTEIEN)
        {
          /* check end of CDC decoder active period */
          if ((cdc.irq & BIT_DECI) && (cdc.cycles[0] > cdc.cycles[1]))
          {
            /* clear pending decoder interrupt */
            cdc.ifstat |= BIT_DECI;

            /* update CDC IRQ state */
            cdc.irq &= ~BIT_DECI;
          }

          /* level 5 interrupt triggered only on CDC /INT falling edge with interrupt enabled on gate-array side */
          if (!cdc.irq && (scd.regs[0x32>>1].byte.l & 0x20))
          {
            /* pending level 5 interrupt */
            scd.pending |= (1 << 5);

            /* update IRQ level */
            s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
          }

          /* update CDC IRQ state */
          cdc.irq |= BIT_DTEI;
        }

        /* set EDT bit (gate-array register $04) */
        scd.regs[0x04>>1].byte.h |= 0x80;
      }
    }
  }

  return data;
}
