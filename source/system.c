/*
    Copyright (C) 1999, 2000, 2001, 2002, 2003  Charles MacDonald

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "shared.h"

#define CLOCK_NTSC 53693175
#define CLOCK_PAL  53203424
#define SND_SIZE (snd.buffer_size * sizeof(int16))

t_bitmap bitmap;
t_snd snd;
uint32 aim_m68k   = 0;
uint32 count_m68k = 0;
uint32 dma_m68k   = 0;
uint32 aim_z80    = 0;
uint32 count_z80  = 0;
uint16 misc68Kcycles   = 488;
uint16 miscZ80cycles   = 228;
uint16 lines_per_frame = 262;
double CPU_Clock    = (double)CLOCK_NTSC;
void *myFM = NULL;
static int sound_tbl[312];
static int sound_inc[312];
uint8 alttiming = 0;

void m68k_run (int cyc) 
{
	/* cycles remaining to run for the line */
	int cyc_do = cyc - count_m68k;

	/* 68k is not frozen */
	if (cyc_do > 0)
	{
		/* interrupt handling */
		if (vint_pending && (reg[1] & 0x20)) m68k_set_irq(6);
		else if (hint_pending && (reg[0] & 0x10)) m68k_set_irq(4);
		if (cyc_do > 0) m68k_execute(cyc_do);
		count_m68k += cyc_do;
		vint_pending = 0;
	}
}

void z80_run (int cyc) 
{
	int cyc_do = cyc - count_z80;
	if (cyc_do > 0) count_z80 += z80_execute(cyc_do);
}

void m68k_freeze(int cycles)
{
	extern int m68ki_remaining_cycles;
	int extra_cycles = cycles - m68k_cycles_remaining();
 
	if (extra_cycles > 0)
	{
		/* end of 68k execution */
		m68ki_remaining_cycles = 0;

		/* extra cycles to be burned at next execution */
		count_m68k += extra_cycles; 
	}
	else m68ki_remaining_cycles -= cycles; /* we burn some 68k cycles */
}

extern uint8 hq_fm;
int audio_init (int rate)
{
	int i;
	int vclk = (int)(CPU_Clock / 7.0);  /* 68000 and YM2612 clock */
	int zclk = (int)(CPU_Clock / 15.0); /* Z80 and SN76489 clock  */

	/* Clear the sound data context */
	memset (&snd, 0, sizeof (snd));

	/* Make sure the requested sample rate is valid */
	if (!rate || ((rate < 8000) | (rate > 48000))) return (0);
	
	/* Calculate the sound buffer size */
	snd.buffer_size = (rate / vdp_rate);
	snd.sample_rate = rate;

	/* Allocate sound buffers */
	if (FM_GENS)
	{
		snd.fm.gens_buffer[0] = realloc (snd.fm.gens_buffer[0], snd.buffer_size * sizeof (int));
		snd.fm.gens_buffer[1] = realloc (snd.fm.gens_buffer[1], snd.buffer_size * sizeof (int));
		if (!snd.fm.gens_buffer[0] || !snd.fm.gens_buffer[1]) return (-1);
		memset (snd.fm.gens_buffer[0], 0, SND_SIZE*2);
		memset (snd.fm.gens_buffer[1], 0, SND_SIZE*2);
	}
	else
	{
		snd.fm.buffer[0] = realloc (snd.fm.buffer[0], snd.buffer_size * sizeof (int16));
		snd.fm.buffer[1] = realloc (snd.fm.buffer[1], snd.buffer_size * sizeof (int16));
		if (!snd.fm.buffer[0] || !snd.fm.buffer[1]) return (-1);
		memset (snd.fm.buffer[0], 0, SND_SIZE);
		memset (snd.fm.buffer[1], 0, SND_SIZE);
	}

	snd.psg.buffer = realloc (snd.psg.buffer, snd.buffer_size * sizeof (int16));
	if (!snd.psg.buffer) return (-1);
	memset (snd.psg.buffer, 0, SND_SIZE);

	/* Set audio enable flag */
	snd.enabled = 1;

	/* Initialize sound chip emulation */
	if (PSG_MAME) SN76496_sh_start(zclk, 0, rate);
	else
	{
		SN76489_Init(0, zclk, rate);
		SN76489_Config(0, MUTE_ALLON, VOL_FULL, FB_SEGAVDP, SRW_SEGAVDP, 0);
	}

	if (FM_GENS) YM2612_Init(vclk, rate, hq_fm);
	else if (!myFM) myFM = YM2612Init (0, 0, vclk, rate, 0, 0);

	/* Make sound table */
	for (i = 0; i < lines_per_frame; i++)
	{
		float p = (snd.buffer_size * i) / lines_per_frame;
		float q = (snd.buffer_size * (i+1)) / lines_per_frame;
		sound_tbl[i] = p;
		sound_inc[i] = ((q - p) * 1000000) / snd.sample_rate;
	}
	return (0);
}

void system_init (void)
{
	/* PAL or NTSC timings */
	vdp_rate        = (vdp_pal) ? 50 : 60;
	lines_per_frame = (vdp_pal) ? 313 : 262;
	CPU_Clock = (vdp_pal) ? (double)CLOCK_PAL : (double)CLOCK_NTSC;
	miscZ80cycles = (vdp_pal) ? 227 : 228;
	misc68Kcycles = (vdp_pal) ? 487 : 488;

	gen_init ();
	vdp_init ();
	render_init ();
	sound_init();
}

void system_reset (void)
{
	aim_m68k = 0;
	count_m68k = 0;
	dma_m68k   = 0;
	aim_z80    = 0;
	count_z80  = 0;
	
	gen_reset ();
	io_reset();
	vdp_reset ();
	render_reset ();

	if (snd.enabled)
	{
		fm_reset();
		if (FM_GENS)
		{
			memset (snd.fm.gens_buffer[0], 0, SND_SIZE*2);
			memset (snd.fm.gens_buffer[1], 0, SND_SIZE*2);
		}
		else
		{
			memset (snd.fm.buffer[0], 0, SND_SIZE);
			memset (snd.fm.buffer[1], 0, SND_SIZE);
		}

		if (!PSG_MAME) SN76489_Reset(0);
		memset (snd.psg.buffer, 0, SND_SIZE);
	}
}

void system_shutdown (void)
{
	gen_shutdown ();
	vdp_shutdown ();
	render_shutdown ();
}

int system_frame (int do_skip)
{
	/* update total cycles count */
	dma_m68k += count_m68k;

	/* reset line cycles counts */
	aim_m68k   = 0;
	count_m68k = 0;
	aim_z80   = 0;
	count_z80 = 0;
	lastbusreqcnt = -1000;

	if (!gen_running) return 0;

	/* Clear V-Blank flag */
	status &= 0xFFF7;

	/* Toggle even/odd flag (IM2 only) */
	if (im2_flag) status ^= 0x0010;

	/* Reset HCounter */
	h_counter = reg[10];

	/* Point to start of sound buffer */
	snd.fm.lastStage = snd.fm.curStage = 0;
	snd.psg.lastStage = snd.psg.curStage = 0;

  /* Parse sprites for line 0 (done on line 261 or 312) */
	parse_satb (0x80);

	/* Line processing */
	for (v_counter = 0; v_counter < lines_per_frame; v_counter ++)
	{
		/* update cpu cycles goal for the line */
		aim_z80  += miscZ80cycles;
		aim_m68k += misc68Kcycles;

	  	/* 6-Buttons or Menacer update */
	  	input_update();

		/* If a Z80 interrupt is still pending after a scanline, cancel it */
		if (zirq)
		{
			zirq = 0;
			z80_set_irq_line(0, CLEAR_LINE);
		}

		/* H Int */
		if (v_counter <= frame_end)
		{
			if(--h_counter < 0)
			{
				h_counter = reg[10];
				hint_pending = 1;
			}
		}

	  	/* hack for Lotus 2 Recs */
	  	if (alttiming && !do_skip)
		{
			if (v_counter < frame_end) render_line(v_counter);
			if (v_counter < (frame_end-1)) parse_satb(0x81 + v_counter);
		}

		/* H retrace */
		status |= 0x0004; // HBlank = 1
		m68k_run(aim_m68k - 404);
		status &= 0xFFFB; // HBlank = 0

		if (!alttiming && !do_skip)
		{
			if (v_counter < frame_end) render_line(v_counter);
	        if (v_counter < (frame_end-1)) parse_satb(0x81 + v_counter);
	    }
	  
		if (v_counter == frame_end)
		{
			/* V Retrace */
			status |= 0x0008; // VBlank = 1
			m68k_run(aim_m68k - 360);
			if (zreset == 1 && zbusreq == 0) z80_run(aim_z80 - 168);
			else count_z80 = aim_z80 - 168;

			/* V Int */
		  	vint_pending = 1;
			status |= 0x0080;

			/* Z Int */
			if (zreset == 1 && zbusreq == 0)
			{
				zirq = 1;
				z80_set_irq_line(0, ASSERT_LINE);  
			} 
		}

		/* Process end of line */
		m68k_run(aim_m68k);
		if (zreset == 1 && zbusreq == 0) z80_run(aim_z80);
		else count_z80 = aim_z80;

		/* Update sound buffers and timers */
		fm_update_timers (sound_inc[v_counter]);
		snd.fm.curStage = sound_tbl[v_counter];
		snd.psg.curStage = sound_tbl[v_counter];
	}

	if (snd.enabled) audio_update ();
	return gen_running;
}

#ifdef NGC
/****************************************************************************
 * softdev - 09 Mar 2006
 *
 *
 * Nintendo Gamecube Specific Mixer from here on in ...
 ****************************************************************************/
extern unsigned char soundbuffer[16][3840];
extern int mixbuffer;
extern double psg_preamp;
extern double fm_preamp;
extern u8 boost;
static int ll, rr;

void audio_update (void)
{
	int i;
	int l, r;
	int16 *tempBuffer[2];
	int16 *sb = (int16 *) soundbuffer[mixbuffer];

	/* get remaining samples */
	/* YM2612 */
	if (FM_GENS)
	{
		int *fmBuffer[2];
		fmBuffer[0] = snd.fm.gens_buffer[0] + snd.fm.lastStage;
		fmBuffer[1] = snd.fm.gens_buffer[1] + snd.fm.lastStage;
		YM2612_Update(fmBuffer, snd.buffer_size - snd.fm.lastStage);
	}
	else
	{
		tempBuffer[0] = snd.fm.buffer[0] + snd.fm.lastStage;
		tempBuffer[1] = snd.fm.buffer[1] + snd.fm.lastStage;
		YM2612UpdateOne (myFM, tempBuffer, snd.buffer_size - snd.fm.lastStage);
	}

	/* PSG */
	tempBuffer[0] = snd.psg.buffer + snd.psg.lastStage;
	if (PSG_MAME) SN76496Update (0, tempBuffer[0], snd.buffer_size - snd.psg.lastStage);
	else SN76489_Update(0, tempBuffer[0], snd.buffer_size - snd.psg.lastStage);

	/* mix samples */
	for (i = 0; i < snd.buffer_size; i ++)
	{
		/* PSG */
		l = r = (int) ((double)snd.psg.buffer[i] * psg_preamp);
		
		if (FM_GENS)
		{
			l += (int) ((double)snd.fm.gens_buffer[0][i] * fm_preamp);
			r += (int) ((double)snd.fm.gens_buffer[1][i] * fm_preamp);
			snd.fm.gens_buffer[0][i] = 0;
			snd.fm.gens_buffer[1][i] = 0;
		}
		else
		{
			l += (int) ((double)snd.fm.buffer[0][i] * fm_preamp);
			r += (int) ((double)snd.fm.buffer[1][i] * fm_preamp);
		}

		/* single-pole low-pass filter (6 dB/octave) */
		ll = (ll + l) >> 1;
		rr = (rr + r) >> 1;

		l = ll * boost;
		r = rr * boost;

		/* clipping */
		if (l > 32767) l = 32767;
		else if (l < -32768) l = -32768;
		if (r > 32767) r = 32767;
		else if (r < -32768) r = -32768;

		*sb++ = l;
		*sb++ = r;
  }

  mixbuffer++;
  mixbuffer &= 0xf;
}
#else
void audio_update (void)
{
  int i;
  int16 acc;
  int16 *tempBuffer[2];

  if (FM_GENS)
  {
	  int *fmBuffer[2];
	  fmBuffer[0] = snd.fm.gens_buffer[0] + snd.fm.lastStage;
	  fmBuffer[1] = snd.fm.gens_buffer[1] + snd.fm.lastStage;
	  YM2612_Update(fmBuffer, snd.buffer_size - snd.fm.lastStage);
  }
  else
  {
	  tempBuffer[0] = snd.fm.buffer[0] + snd.fm.lastStage;
	  tempBuffer[1] = snd.fm.buffer[1] + snd.fm.lastStage;
	  YM2612UpdateOne (myFM, tempBuffer, snd.buffer_size - snd.fm.lastStage);
  }

  tempBuffer[0] = snd.psg.buffer + snd.psg.lastStage;
  if (PSG_MAME) SN76496Update (0, tempBuffer[0], snd.buffer_size - snd.psg.lastStage);
  else SN76489_Update(0, tempBuffer[0], snd.buffer_size - snd.psg.lastStage);

  for (i = 0; i < snd.buffer_size; i += 1)
  {
      int16 psg = snd.psg.buffer[i] / 2;
 
	  acc = 0;
      acc += FM_GENS ? snd.fm.gens_buffer[0][i] : snd.fm.buffer[0][i];
      acc += psg;
      snd.buffer[0][i] = acc;

      acc = 0;
      acc += FM_GENS ? snd.fm.gens_buffer[1][i] : snd.fm.buffer[1][i];
      acc += psg;
      snd.buffer[1][i] = acc;

	  if (FM_GENS)
	  {
		  snd.fm.gens_buffer[0][i] = 0;
		  snd.fm.gens_buffer[1][i] = 0;
	  }

  }
}
#endif

