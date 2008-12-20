/***************************************************************************************
 *  Genesis Plus 1.2a
 *  Main Emulation
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  modified by Eke-Eke (compatibility fixes & additional code), GC/Wii port
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************************/

#include "shared.h"

#define SND_SIZE (snd.buffer_size * sizeof(int16))

/* Global variables */
t_bitmap bitmap;
t_snd snd;
uint8  vdp_rate;
uint16 lines_per_frame;
uint32 aim_m68k;
uint32 count_m68k;
uint32 line_m68k;
uint32 hint_m68k;
uint32 aim_z80;
uint32 count_z80;
uint32 line_z80;
int32 current_z80;
uint8 odd_frame;
uint8 interlaced;
uint8 system_hw;

static inline void audio_update (void);

/****************************************************************
 * Virtual Genesis initialization
 ****************************************************************/
void system_init (void)
{
  /* PAL/NTSC timings */
  vdp_rate        = vdp_pal ? 50 : 60;
  lines_per_frame = vdp_pal ? 313 : 262;

  gen_init ();
  vdp_init ();
  render_init ();
  cart_hw_init();
}

/****************************************************************
 * Virtual Genesis Restart
 ****************************************************************/
void system_reset (void)
{
  aim_m68k    = 0;
  count_m68k  = 0;
  line_m68k   = 0;
  aim_z80     = 0;
  count_z80   = 0;
  line_z80    = 0;
  current_z80 = 0;
  odd_frame   = 0;
  interlaced  = 0;

  /* Cart Hardware reset */
  cart_hw_reset();

  /* Hard reset */
  gen_reset (1); 
  vdp_reset ();
  render_reset ();
  io_reset();
  SN76489_Reset(0);

  /* Sound buffers reset */
  memset (snd.psg.buffer, 0, SND_SIZE);
  memset (snd.fm.buffer[0], 0, SND_SIZE*2);
  memset (snd.fm.buffer[1], 0, SND_SIZE*2);
}

/****************************************************************
 * Virtual Genesis shutdown
 ****************************************************************/
void system_shutdown (void)
{
  gen_shutdown ();
  vdp_shutdown ();
  render_shutdown ();
}

/****************************************************************
 * Virtual Genesis Frame emulation
 ****************************************************************/
int system_frame (int do_skip)
{
  if (!gen_running)
  {
    update_input();
    return 0;
  }

  /* reset cycles counts */
  count_m68k      = 0;
  aim_m68k        = 0;
  aim_z80         = 0;
  count_z80       = 0;
  fifo_write_cnt  = 0;
  fifo_lastwrite  = 0;

  /* update display settings */
  int line;
  int reset = resetline;
  int vdp_height  = bitmap.viewport.h;
  int end_line    = vdp_height + bitmap.viewport.y;
  int start_line  = lines_per_frame - bitmap.viewport.y;
  int old_interlaced = interlaced;
  interlaced = (reg[12] & 2) >> 1;
  if (old_interlaced != interlaced)
  {
    bitmap.viewport.changed = 2;
    im2_flag = ((reg[12] & 6) == 6);
    odd_frame = 1;
  }

  odd_frame ^= 1;

  /* clear VBLANK and DMA flags */
  status &= 0xFFF5;

  /* even/odd field flag (interlaced modes only) */
  if (odd_frame && interlaced) status |= 0x0010;
  else status &= 0xFFEF;

  /* reload HCounter */
  int h_counter = reg[10];

  /* parse sprites for line 0 (done on last line) */
  parse_satb (0x80);

  /* process frame */
  for (line = 0; line < lines_per_frame; line ++)
  {
    /* update VCounter */
    v_counter = line;

    /* update 6-Buttons or Menacer */
    input_update();

    /* update CPU cycle counters */
    hint_m68k = count_m68k;
    line_m68k = aim_m68k;
    line_z80  = aim_z80;
    aim_z80  += z80cycles_per_line;
    aim_m68k += m68cycles_per_line;

    /* Soft Reset ? */
    if (line == reset)
    {
#ifdef NGC
      /* wait for RESET button to be released */
      while (SYS_ResetButtonDown());
#endif
      gen_reset(0);
    }

    /* active display */
    if (line <= vdp_height)
    {
      /* H Interrupt */
      if(--h_counter < 0)
      {
        h_counter = reg[10];
        hint_pending = 1;
        if (reg[0] & 0x10) irq_status = (irq_status & 0xff) | 0x14;

        /* adjust timings to take further decrement in account (see below) */
        if ((line != 0) || (h_counter == 0)) aim_m68k += 36;
      }

      /* HINT will be triggered on next line, approx. 36 cycles before VDP starts line rendering */
      /* during this period, any VRAM/CRAM/VSRAM writes should NOT be taken in account before next line */
      /* as a result, current line is shortened */
      /* fix Lotus 1, Lotus 2 RECS, Striker, Zero the Kamikaze Squirell */
      if ((line < vdp_height) && (h_counter == 0)) aim_m68k -= 36;

      /* update DMA timings */
      if (dma_length) dma_update();

      /* vertical retrace */
      if (line == vdp_height)
      {
        /* render overscan */
        if ((line < end_line) && (!do_skip)) render_line(line, 1);

        /* update inputs (doing this here fix Warriors of Eternal Sun) */
        update_input();

        /* set VBLANK flag */
        status |= 0x08;

        /* Z80 interrupt is 16ms period (one frame) and 64us length (one scanline) */
        zirq = 1;
        z80_set_irq_line(0, ASSERT_LINE);  

        /* delay between HINT, VBLANK and VINT (Dracula, OutRunners, VR Troopers) */
        m68k_run(line_m68k + 84);
        if (zreset && !zbusreq)
        {
          current_z80 = line_z80 + 39 - count_z80;
          if (current_z80 > 0) count_z80 += z80_execute(current_z80);
        }
        else count_z80 = line_z80 + 39;

        /* V Interrupt */
        status |= 0x80;
        vint_pending = 1;

        /* 36 cycles latency after VINT occurence flag (Ex-Mutants, Tyrant) */
        if (reg[1] & 0x20) irq_status = (irq_status & 0xff) | 0x2416; 
      }
      else if (!do_skip) 
      {
        /* render scanline and parse sprites for line n+1 */
        render_line(line, 0);
        if (line < (vdp_height-1)) parse_satb(0x81 + line);
      }
    }
    else
    {
      /* update DMA timings */
      if (dma_length) dma_update();

      /* render overscan */
      if ((!do_skip) && ((line < end_line) || (line >= start_line))) render_line(line, 1);

      /* clear any pending Z80 interrupt */
      if (zirq)
      {
        zirq = 0;
        z80_set_irq_line(0, CLEAR_LINE);
      }
    }

    /* process line */
    m68k_run(aim_m68k);
    if (zreset == 1 && zbusreq == 0)
    {
      current_z80 = aim_z80 - count_z80;
      if (current_z80 > 0) count_z80 += z80_execute(current_z80);
    }
    else count_z80 = aim_z80;
    
    /* SVP chip */
    if (svp) ssp1601_run(SVP_cycles);
  }

  audio_update ();

  return gen_running;
}

/****************************************************************
 *              Audio System 
 ****************************************************************/
int audio_init (int rate)
{
  /* Shutdown first */
  audio_shutdown();

  /* Clear the sound data context */
  memset (&snd, 0, sizeof (snd));

  /* Make sure the requested sample rate is valid */
  if (!rate || ((rate < 8000) | (rate > 48000))) return (-1);
  snd.sample_rate = rate;

  /* Calculate the sound buffer size (for one frame) */
  snd.buffer_size = (rate / vdp_rate);

#ifdef DOS
  /* output buffers */
  snd.buffer[0] = (int16 *) malloc(SND_SIZE);
  snd.buffer[1] = (int16 *) malloc(SND_SIZE);
  if (!snd.buffer[0] || !snd.buffer[1]) return (-1);
  memset (snd.buffer[0], 0, SND_SIZE);
  memset (snd.buffer[1], 0, SND_SIZE);
#endif

  /* YM2612 stream buffers */
  snd.fm.buffer[0] = (int *)malloc (SND_SIZE*2);
  snd.fm.buffer[1] = (int *)malloc (SND_SIZE*2);
  if (!snd.fm.buffer[0] || !snd.fm.buffer[1]) return (-1);
  memset (snd.fm.buffer[0], 0, SND_SIZE*2);
  memset (snd.fm.buffer[1], 0, SND_SIZE*2);

  /* SRC buffers */
  if (config.hq_fm && !config.fm_core)
  {
    snd.fm.src_out = (float *) malloc(snd.buffer_size*2*sizeof(float));
    if (!snd.fm.src_out) return (-1);
  }

  /* SN76489 stream buffers */
  snd.psg.buffer = (int16 *)malloc (SND_SIZE);
  if (!snd.psg.buffer) return (-1);
  memset (snd.psg.buffer, 0, SND_SIZE);

  /* Set audio enable flag */
  snd.enabled = 1;

  /* Initialize Sound Chips emulation */
  sound_init(rate);

  return (0);
}

void audio_shutdown(void)
{
  /* free sound buffers */
  if (snd.buffer[0])    free(snd.buffer[0]);
  if (snd.buffer[1])    free(snd.buffer[1]);
  if (snd.fm.buffer[0]) free(snd.fm.buffer[0]);
  if (snd.fm.buffer[1]) free(snd.fm.buffer[1]);
  if (snd.fm.src_out)   free(snd.fm.src_out);
  if (snd.psg.buffer)   free(snd.psg.buffer);
}

static int ll, rr;

static inline void audio_update (void)
{
  int i;
  int l, r;
  double psg_preamp = config.psg_preamp;
  double fm_preamp  = config.fm_preamp;
  int boost  = config.boost;
  int filter = config.filter;

#ifndef DOS
  int16 *sb = (int16 *) soundbuffer[mixbuffer];
#endif

  /* get remaining samples */
  sound_update();

  /* mix samples */
  for (i = 0; i < snd.buffer_size; i ++)
  {
    l = r = (int) ((double)snd.psg.buffer[i] * psg_preamp);
    l += (int) ((double)snd.fm.buffer[0][i] * fm_preamp);
    r += (int) ((double)snd.fm.buffer[1][i] * fm_preamp);
    snd.fm.buffer[0][i] = 0;
    snd.fm.buffer[1][i] = 0;
    snd.psg.buffer[i] = 0;

    /* single-pole low-pass filter (6 dB/octave) */
    if (filter)
    {
      l = (ll + l) >> 1;
      r = (rr + r) >> 1;
      ll = l;
      rr = r;
    }

    /* boost volume if asked*/
    l = l * boost;
    r = r * boost;

    /* clipping */
    if (l > 32767) l = 32767;
    else if (l < -32768) l = -32768;
    if (r > 32767) r = 32767;
    else if (r < -32768) r = -32768;

    /* update sound buffer */
#ifdef DOS
    snd.buffer[0][i] = l;
    snd.buffer[1][i] = r;
#elif LSB_FIRST
    *sb++ = l;
    *sb++ = r;
#else
    *sb++ = r;
    *sb++ = l;
#endif
  }
}
