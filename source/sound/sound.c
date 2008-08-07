/***************************************************************************************
 *  Genesis Plus 1.2a
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
 *  Sound Hardware
 ****************************************************************************************/

#include "shared.h"

/* generic functions */
int  (*_YM2612_Write)(unsigned char adr, unsigned char data);
int  (*_YM2612_Read)(void);
void (*_YM2612_Update)(int **buf, int length);
int (*_YM2612_Reset)(void);

static double m68cycles_per_sample;
static double z80cycles_per_sample;

/* YM2612 data */
int fm_reg[2][0x100];		/* Register arrays (2x256) */
double fm_timera_tab[0x400];	/* Precalculated timer A values (in usecs) */
double fm_timerb_tab[0x100];	/* Precalculated timer B values (in usecs) */

/* return the number of samples that should have been rendered so far */
static inline uint32 sound_sample_cnt(uint8 is_z80)
{
	if (is_z80) return (uint32) ((double)(count_z80 + current_z80 - z80_ICount) / z80cycles_per_sample);
	else return (uint32) ((double) count_m68k / m68cycles_per_sample);
}

/* update FM samples */
static inline void fm_update()
{
	if(snd.fm.curStage - snd.fm.lastStage > 1)
	{
        //error("%d(%d): FM update (%d) %d from %d samples (%08x)\n", v_counter, count_z80 + current_z80 - z80_ICount, cpu, snd.fm.curStage , snd.fm.lastStage, m68k_get_reg (NULL, M68K_REG_PC));
		int *tempBuffer[2];       
		tempBuffer[0] = snd.fm.buffer[0] + snd.fm.lastStage;
		tempBuffer[1] = snd.fm.buffer[1] + snd.fm.lastStage;
		_YM2612_Update(tempBuffer, snd.fm.curStage - snd.fm.lastStage);
		snd.fm.lastStage = snd.fm.curStage;
	}
}

/* update PSG samples */
static inline void psg_update()
{
	if(snd.psg.curStage - snd.psg.lastStage > 1)
	{
		int16 *tempBuffer = snd.psg.buffer + snd.psg.lastStage;
		SN76489_Update (0, tempBuffer, snd.psg.curStage - snd.psg.lastStage);
		snd.psg.lastStage = snd.psg.curStage;
	}
}

void sound_init(int rate)
{
	int i;
	double vclk = Master_Clock / 7.0;  /* 68000 and YM2612 clock */
	double zclk = Master_Clock / 15.0; /* Z80 and SN76489 clock  */

	/* Make Timer A table */
	/* Formula is "time(us) = (1024 - A) * 144 * 1000000 / clock" */
	for(i = 0; i < 1024; i += 1) fm_timera_tab[i] = ((double)((1024 - i) * 144) * 1000000.0 / vclk);

	/* Make Timer B table */
	/* Formula is "time(us) = 16 * (256 - B) * 144 * 1000000 / clock" */
	for(i = 0; i < 256; i += 1) fm_timerb_tab[i] = ((double)((256 - i) * 16 * 144) * 1000000.0 / vclk);

	/* Cycle-Accurate sample generation */
	m68cycles_per_sample = ((double)m68cycles_per_line * (double)lines_per_frame) / (double) (rate / vdp_rate);
	z80cycles_per_sample = ((double)z80cycles_per_line * (double)lines_per_frame) / (double) (rate / vdp_rate);

	/* initialize sound chips */
	SN76489_Init(0, (int)zclk, rate);
	SN76489_Config(0, MUTE_ALLON, VOL_FULL, FB_SEGAVDP, SRW_SEGAVDP, 0);

	if (config.fm_core)
	{
		_YM2612_Write  = YM2612_Write;
		_YM2612_Read   = YM2612_Read;
		_YM2612_Update = YM2612_Update;
		_YM2612_Reset  = YM2612_Reset;
		YM2612_Init((int)vclk, rate, config.hq_fm);
	}
	else
	{
		_YM2612_Write  = YM2612Write;
		_YM2612_Read   = YM2612Read;
		_YM2612_Update = YM2612UpdateOne;
		_YM2612_Reset  = YM2612ResetChip;
		YM2612Init ((int)vclk, rate);
	}
} 

void sound_update(void)
{
	/* finalize sound buffers */
	snd.fm.curStage  = snd.buffer_size;
	snd.psg.curStage = snd.buffer_size;

	/* update last samples (if needed) */
	fm_update();
	psg_update();

	/* reset samples count */
	snd.fm.curStage   = 0;
	snd.fm.lastStage  = 0;
	snd.psg.curStage  = 0;
	snd.psg.lastStage = 0;
}

/* YM2612 control */
/* restore FM registers */
void fm_restore(void)
{
	int i;

	_YM2612_Reset();

	/* feed all the registers and update internal state */
	for(i = 0; i < 0x100; i++)
	{
		_YM2612_Write(0, i);
		_YM2612_Write(1, fm_reg[0][i]);
		_YM2612_Write(2, i);
		_YM2612_Write(3, fm_reg[1][i]);
	}
}

/* write FM chip */
void fm_write(unsigned int cpu, unsigned int address, unsigned int data)
{
	snd.fm.curStage = sound_sample_cnt(cpu);
	fm_update();
	_YM2612_Write(address & 3, data);
}

/* read FM status */
unsigned int fm_read(unsigned int cpu, unsigned int address)
{
	snd.fm.curStage = sound_sample_cnt(cpu);
	fm_update();
	return (_YM2612_Read() & 0xff);
}


/* PSG write */
void psg_write(unsigned int cpu, unsigned int data)
{
	snd.psg.curStage = sound_sample_cnt(cpu);
	psg_update();
	SN76489_Write(0, data);
}
