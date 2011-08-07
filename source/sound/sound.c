/***************************************************************************************
 *  Genesis Plus
 *  Sound Hardware
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Copyright (C) 2007-2011  Eke-Eke (Genesis Plus GX)
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
#include "Fir_Resampler.h"

/* Cycle-accurate samples */
static unsigned int psg_cycles_ratio;
static unsigned int psg_cycles_count;
static unsigned int fm_cycles_ratio;
static unsigned int fm_cycles_count;

/* YM chip function pointers */
static void (*YM_Reset)(void);
static void (*YM_Update)(long int *buffer, int length);
static void (*YM_Write)(unsigned int a, unsigned int v);

/* Run FM chip for required M-cycles */
static inline void fm_update(unsigned int cycles)
{
  if (cycles > fm_cycles_count)
  {
    /* period to run */
    cycles -= fm_cycles_count;

    /* update cycle count */
    fm_cycles_count += cycles;

    /* number of samples during period */
    unsigned int cnt = cycles / fm_cycles_ratio;

    /* remaining cycles */
    unsigned int remain = cycles % fm_cycles_ratio;
    if (remain)
    {
      /* one sample ahead */
      fm_cycles_count += fm_cycles_ratio - remain;
      cnt++;
    }

    /* select input sample buffer */
    int32 *buffer = Fir_Resampler_buffer();
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
    YM_Update(buffer, cnt);
  }
}

/* Run PSG chip for required M-cycles */
static inline void psg_update(unsigned int cycles)
{
  if (cycles > psg_cycles_count)
  {
    /* period to run */
    cycles -= psg_cycles_count;

    /* update cycle count */
    psg_cycles_count += cycles;

    /* number of samples during period */
    unsigned int cnt = cycles / psg_cycles_ratio;

    /* remaining cycles */
    unsigned int remain = cycles % psg_cycles_ratio;
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
  psg_cycles_ratio  = (unsigned int)(mclk / (double) snd.sample_rate * 2048.0);
  fm_cycles_ratio   = psg_cycles_ratio;
  fm_cycles_count   = 0;
  psg_cycles_count  = 0;

  /* Initialize core emulation (input clock based on input frequency for 100% accuracy)   */
  /* By default, both chips are running at the output frequency.                          */
  SN76489_Init(mclk/15.0,snd.sample_rate);

  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    /* YM2612 */
    YM2612Init(mclk/7.0,snd.sample_rate);
    YM_Reset = YM2612ResetChip;
    YM_Update = YM2612Update;
    YM_Write = YM2612Write;

    /* In HQ mode, YM2612 is running at its original rate (one sample each 144*7 M-cycles)  */
    /* FM stream is resampled to the output frequency at the end of a frame.                */
    if (config.hq_fm)
    {
      fm_cycles_ratio = 144 * 7 * (1 << 11);
      Fir_Resampler_time_ratio(mclk / (double)snd.sample_rate / (144.0 * 7.0), config.rolloff);
    }
  }
  else
  {
    /* YM2413 */
    YM2413Init(mclk/15.0,snd.sample_rate);
    YM_Reset = YM2413ResetChip;
    YM_Update = YM2413Update;
    YM_Write = YM2413Write;

    /* In HQ mode, YM2413 is running at its original rate (one sample each 72*15 M-cycles)  */
    /* FM stream is resampled to the output frequency at the end of a frame.                */
    if (config.hq_fm)
    {
      fm_cycles_ratio = 72 * 15 * (1 << 11);
      Fir_Resampler_time_ratio(mclk / (double)snd.sample_rate / (72.0 * 15.0), config.rolloff);
    }
  }

#ifdef LOGSOUND
  error("%d mcycles per PSG samples\n", psg_cycles_ratio);
  error("%d mcycles per FM samples\n", fm_cycles_ratio);
#endif
}

/* Reset sound chips emulation */
void sound_reset(void)
{
  YM_Reset();
  SN76489_Reset();
  fm_cycles_count = 0;
  psg_cycles_count = 0;
}

void sound_restore()
{
  int size;
  uint8 *ptr, *temp;

  /* save YM context */
  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    size = YM2612GetContextSize();
    ptr = YM2612GetContextPtr();
  }
  else
  {
    size = YM2413GetContextSize();
    ptr = YM2413GetContextPtr();
  }
  temp = malloc(size);
  if (temp)
  {
    memcpy(temp, ptr, size);
  }

  /* reinitialize sound chips */
  sound_init();

  /* restore YM context */
  if (temp)
  {
    if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
    {
      YM2612Restore(temp);
    }
    else
    {
      YM2413Restore(temp);
    }
    free(temp);
  }
}

int sound_context_save(uint8 *state)
{
  int bufferptr = 0;
  
  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    bufferptr = YM2612SaveContext(state);
  }
  else
  {
    save_param(YM2413GetContextPtr(),YM2413GetContextSize());
  }

  save_param(SN76489_GetContextPtr(),SN76489_GetContextSize());
  save_param(&fm_cycles_count,sizeof(fm_cycles_count));
  save_param(&psg_cycles_count,sizeof(psg_cycles_count));

  return bufferptr;
}

int sound_context_load(uint8 *state, char *version)
{
  int bufferptr = 0;

  if (((system_hw & SYSTEM_PBC) == SYSTEM_MD) || ((version[13] == 0x35) && (version[15] == 0x30)))
  {
    bufferptr = YM2612LoadContext(state);
  }
  else
  {
    load_param(YM2413GetContextPtr(),YM2413GetContextSize());
  }

  load_param(SN76489_GetContextPtr(),SN76489_GetContextSize());

  load_param(&fm_cycles_count,sizeof(fm_cycles_count));
  load_param(&psg_cycles_count,sizeof(psg_cycles_count));
  fm_cycles_count = psg_cycles_count;

  return bufferptr;
}

/* End of frame update, return the number of samples run so far.  */
int sound_update(unsigned int cycles)
{
  /* run PSG & FM chips until end of frame */
  cycles <<= 11;
  psg_update(cycles);
  fm_update(cycles);

  int size = snd.psg.pos - snd.psg.buffer;

#ifdef LOGSOUND
    error("%d PSG samples available\n",size);
#endif

  /* FM resampling */
  if (config.hq_fm)
  {
    /* get available FM samples */
    int avail = Fir_Resampler_avail();

    /* resynchronize FM & PSG chips */
    if (avail < size)
    {
      /* FM chip is late for one (or two) samples */
      do
      {
        YM_Update(Fir_Resampler_buffer(), 1);
        Fir_Resampler_write(2);
        avail = Fir_Resampler_avail();
      }
      while (avail < size);
    }
    else
    {
      /* FM chip is ahead */
      fm_cycles_count += (avail - size) * psg_cycles_ratio;
    }
  }

#ifdef LOGSOUND
  if (config.hq_fm)
    error("%d FM samples (%d) available\n",Fir_Resampler_avail(), Fir_Resampler_written() >> 1);
  else
    error("%d FM samples available\n",(snd.fm.pos - snd.fm.buffer)>>1);
#endif

#ifdef LOGSOUND
  error("%lu PSG cycles run\n",psg_cycles_count);
  error("%lu FM cycles run \n",fm_cycles_count);
#endif

  /* adjust PSG & FM cycle counts for next frame */
  psg_cycles_count -= cycles;
  fm_cycles_count  -= cycles;

#ifdef LOGSOUND
  error("%lu PSG cycles left\n",psg_cycles_count);
  error("%lu FM cycles left\n",fm_cycles_count);
#endif

  return size;
}

/* Reset FM chip */
void fm_reset(unsigned int cycles)
{
  fm_update(cycles << 11);
  YM_Reset();
}

/* Write FM chip */
void fm_write(unsigned int cycles, unsigned int address, unsigned int data)
{
  if (address & 1) fm_update(cycles << 11);
  YM_Write(address, data);
}

/* Read FM status (YM2612 only) */
unsigned int fm_read(unsigned int cycles, unsigned int address)
{
  fm_update(cycles << 11);
  return YM2612Read();
}

/* Write PSG chip */
void psg_write(unsigned int cycles, unsigned int data)
{
  psg_update(cycles << 11);
  SN76489_Write(data);
}
