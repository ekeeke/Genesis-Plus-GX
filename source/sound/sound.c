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

/* cycle-accurate samples */
static int m68cycles_per_sample[2];

/* return the number of samples that should have been rendered so far */
static inline uint32 fm_sample_cnt(uint8 is_z80)
{
  if (is_z80) return ((count_z80 + current_z80 - z80_ICount) * 15) / (m68cycles_per_sample[0] * 7);
  else return (count_m68k / m68cycles_per_sample[0]);
}

static inline uint32 psg_sample_cnt(uint8 is_z80)
{
  if (is_z80) return ((count_z80 + current_z80 - z80_ICount) * 15) / (m68cycles_per_sample[1] * 7);
  else return (count_m68k / m68cycles_per_sample[1]);
}

/* update FM samples */
static inline void fm_update()
{
  if(snd.fm.curStage - snd.fm.lastStage > 0)
  {
    int *tempBuffer[2];
    tempBuffer[0] = snd.fm.buffer[0] + snd.fm.lastStage;
    tempBuffer[1] = snd.fm.buffer[1] + snd.fm.lastStage;
    YM2612UpdateOne(tempBuffer, snd.fm.curStage - snd.fm.lastStage);
    snd.fm.lastStage = snd.fm.curStage;
  }
}

/* update PSG samples */
static inline void psg_update()
{
  if(snd.psg.curStage - snd.psg.lastStage > 0)
  {
    int16 *tempBuffer = snd.psg.buffer + snd.psg.lastStage;
    SN76489_Update(tempBuffer, snd.psg.curStage - snd.psg.lastStage);
    snd.psg.lastStage = snd.psg.curStage;
  }
}

void sound_init(int rate)
{
  double vclk = (vdp_pal ? (double)CLOCK_PAL : (double)CLOCK_NTSC) / 7.0;  /* 68000 and YM2612 clock */
  double zclk = (vdp_pal ? (double)CLOCK_PAL : (double)CLOCK_NTSC) / 15.0; /* Z80 and SN76489 clock  */

  /* cycle-accurate FM & PSG samples */
  m68cycles_per_sample[0] = (int)(((double)m68cycles_per_line * (double)lines_per_frame * (double)vdp_rate / (double)rate) + 0.5);
  m68cycles_per_sample[1] = m68cycles_per_sample[0];

  /* YM2612 is emulated at original frequency (VLCK/144) */
  if (config.hq_fm)
    m68cycles_per_sample[0] = 144;

  /* initialize sound chips */
  SN76489_Init((int)zclk,rate);
  YM2612Init((int)vclk,rate);
} 

void sound_update(int fm_len, int psg_len)
{
  /* finalize sound buffers */
  snd.fm.curStage  = fm_len;
  snd.psg.curStage = psg_len;

  /* update last samples (if needed) */
  fm_update();
  psg_update();

  /* reset samples count */
  snd.fm.curStage   = 0;
  snd.fm.lastStage  = 0;
  snd.psg.curStage  = 0;
  snd.psg.lastStage = 0;
}

/* write FM chip */
void fm_write(unsigned int cpu, unsigned int address, unsigned int data)
{
  if (address & 1)
  {
    snd.fm.curStage = fm_sample_cnt(cpu);
    fm_update();
  }
  YM2612Write(address, data);
}

/* read FM status */
unsigned int fm_read(unsigned int cpu, unsigned int address)
{
  snd.fm.curStage = fm_sample_cnt(cpu);
  fm_update();
  return YM2612Read();
}


/* PSG write */
void psg_write(unsigned int cpu, unsigned int data)
{
  snd.psg.curStage = psg_sample_cnt(cpu);
  psg_update();
  SN76489_Write(data);
}
