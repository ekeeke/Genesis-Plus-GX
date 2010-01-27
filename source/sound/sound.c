/***************************************************************************************
 *  Genesis Plus
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Eke-Eke (2007,2008,2009), additional code & fixes for the GCN/Wii port
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
 *  Sound Hardware
 ****************************************************************************************/

#include "shared.h"
#include "Fir_Resampler.h"

/* Cycle-accurate samples */
static unsigned int psg_cycles_ratio;
static unsigned int psg_cycles_count;
static unsigned int fm_cycles_ratio;
static unsigned int fm_cycles_count;

/* Run FM chip for required M-cycles */
static inline void fm_update(unsigned int cycles)
{
  /* convert to 21.11 fixed point */
  cycles <<= 11;

  if (cycles > fm_cycles_count)
  {
    /* period to run */
    cycles -= fm_cycles_count;

    /* update cycle count */
    fm_cycles_count += cycles;

    /* number of samples during period */
    uint32 cnt = cycles / fm_cycles_ratio;

    /* remaining cycles */
    uint32 remain = cycles % fm_cycles_ratio;
    if (remain)
    {
      /* one sample ahead */
      fm_cycles_count += fm_cycles_ratio - remain;
      cnt++;
    }

    /* select input sample buffer */
    int16 *buffer = Fir_Resampler_buffer();
    if (buffer)
    {
      Fir_Resampler_write(cnt << 1);
    }
    else
    {
      buffer = snd.fm.pos;
      snd.fm.pos += (cnt << 1);
    }

    /* run FM chip & get samples */
    YM2612Update(buffer, cnt);
  }
}

/* Run PSG chip for required M-cycles */
static inline void psg_update(unsigned int cycles)
{
  /* convert to 21.11 fixed point */
  cycles <<= 11;

  if (cycles > psg_cycles_count)
  {
    /* period to run */
    cycles -= psg_cycles_count;

    /* update cycle count */
    psg_cycles_count += cycles;

    /* number of samples during period */
    uint32 cnt = cycles / psg_cycles_ratio;

    /* remaining cycles */
    uint32 remain = cycles % psg_cycles_ratio;
    if (remain)
    {
      /* one sample ahead */
      psg_cycles_count += psg_cycles_ratio - remain;
      cnt++;
    }

    /* run PSG chip & get samples */
    SN76489_Update(snd.psg.pos, cnt);
    snd.psg.pos += cnt;
  }
}

/* Initialize sound chips emulation */
void sound_init(void)
{
  /* Number of M-cycles executed per second.                                              */
  /*                                                                                      */
  /* The original Genesis would run exactly 53693175 M-cycles (53203424 for PAL), with    */
  /* 3420 M-cycles per line and 262 (313 for PAL) lines per frame, which gives an exact   */
  /* framerate of 59.92 (49.70 for PAL) fps.                                              */
  /*                                                                                      */
  /* On some systems, the output framerate is not exactly 60 or 50 fps because we need    */ 
  /* 100% smooth video and therefore frame emulation is synchronized with VSYNC, which    */
  /* period is never exactly 1/60 or 1/50 seconds.                                        */
  /*                                                                                      */
  /* For optimal sound rendering, input samplerate (number of samples rendered per frame) */
  /* is the exact output samplerate (number of samples played per second) divided by the  */
  /* exact output framerate (number of frames emulated per seconds).                      */
  /*                                                                                      */
  /* This ensure there is no audio skipping or lag between emulated frames, while keeping */
  /* accurate timings for sound chips execution & synchronization.                        */
  /*                                                                                      */
  double mclk = MCYCLES_PER_LINE * lines_per_frame * snd.frame_rate;

  /* For better accuracy, sound chips run in synchronization with 68k and Z80 cpus        */
  /* These values give the exact number of M-cycles between 2 rendered samples.           */
  /* we use 21.11 fixed point precision (max. mcycle value is 3420*313 i.e 21 bits max)   */
  psg_cycles_ratio  = (int)((mclk / (double) snd.sample_rate) * 2048.0);
  fm_cycles_ratio   = psg_cycles_ratio;
  fm_cycles_count   = 0;
  psg_cycles_count  = 0;

  /* Initialize core emulation (input clock based on input frequency for 100% accuracy)   */
  /* By default, both chips are running at the output frequency.                          */
  SN76489_Init(mclk/15.0,snd.sample_rate);
  YM2612Init(mclk/7.0,snd.sample_rate);

  /* In HQ mode, YM2612 is running at its original rate (one sample each 144*7 M-cycles)  */
  /* FM stream is resampled to the output frequency at the end of a frame.                */
  if (config.hq_fm)
  {
    fm_cycles_ratio = 144 * 7 * (1 << 11);
    Fir_Resampler_time_ratio(mclk / (double)snd.sample_rate / (144.0 * 7.0));
  }

#ifdef LOGSOUND
  error("%d mcycles per PSG samples\n", psg_cycles_ratio);
  error("%d mcycles per FM samples\n", fm_cycles_ratio);
#endif
}

/* Reset sound chips emulation */
void sound_reset(void)
{
  YM2612ResetChip();
  SN76489_Reset();
  fm_cycles_count = 0;
  psg_cycles_count = 0;
}

/* End of frame update, return the number of samples run so far.  */
int sound_update(unsigned int cycles)
{
  /* run PSG chip until end of frame */
  psg_update(cycles);

  /* number of available samples */
  int size = snd.psg.pos - snd.psg.buffer;

#ifdef LOGSOUND
    error("%d PSG samples available\n",size);
#endif

  /* FM resampling */
  if (config.hq_fm)
  {
    /* get remaining FM samples */
    int remain = Fir_Resampler_input_needed(size);

    if (remain > 0)
    {
      /* resynchronize FM & PSG chips */
      YM2612Update(Fir_Resampler_buffer(), remain);
      Fir_Resampler_write(remain << 1);
    }

    fm_cycles_count = psg_cycles_count;

#ifdef LOGSOUND
    error("%d FM samples available\n",Fir_Resampler_written() >> 1);
#endif
  }
  else
  {
    /* run FM chip until end of frame */
    fm_update(cycles);

#ifdef LOGSOUND
    error("%d FM samples available\n",(snd.fm.pos - snd.fm.buffer)>>1);
#endif
  }

#ifdef LOGSOUND
  error("%lu PSG cycles run\n",psg_cycles_count);
  error("%lu FM cycles run \n",fm_cycles_count);
#endif

  /* adjust PSG & FM cycle counts */
  psg_cycles_count  -= (cycles << 11);
  fm_cycles_count   -= (cycles << 11);

#ifdef LOGSOUND
  error("%lu PSG cycles left\n",psg_cycles_count);
  error("%lu FM cycles left\n",fm_cycles_count);
#endif

  return size;
}

/* Reset FM chip */
void fm_reset(unsigned int cycles)
{
  fm_update(cycles);
  YM2612ResetChip();
}

/* Write FM chip */
void fm_write(unsigned int cycles, unsigned int address, unsigned int data)
{
  if (address & 1)
    fm_update(cycles);
  YM2612Write(address, data);
}

/* Read FM status */
unsigned int fm_read(unsigned int cycles, unsigned int address)
{
  fm_update(cycles);
  return YM2612Read();
}

/* Write PSG chip */
void psg_write(unsigned int cycles, unsigned int data)
{
  psg_update(cycles);
  SN76489_Write(data);
}
