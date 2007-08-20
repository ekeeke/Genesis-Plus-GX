/*
    sound.c
    YM2612 and SN76489 handler
*/

#include "shared.h"

/* YM2612 data */
int fm_timera_tab[0x400];   /* Precalculated timer A values */
int fm_timerb_tab[0x100];   /* Precalculated timer B values */
uint8 fm_reg[2][0x100];     /* Register arrays (2x256) */
uint8 fm_latch[2];          /* Register latches */
uint8 fm_status;            /* Read-only status flags */
t_timer timer[2];           /* Timers A and B */
extern void *myFM;

/* Initialize the YM2612 and SN76489 emulation */
void sound_init(void)
{
	/* Timers run at half the YM2612 input clock */
	float clock = ((Master_Clock / 1000000.0) / 7.0) / 2.0;
	int i;

	/* Make Timer A table */
	for(i = 0; i < 1024; i += 1)
	{
		/* Formula is "time(us) = 72 * (1024 - A) / clock" */
		fm_timera_tab[i] = (int)((double)(72 * (1024 - i)) / clock);
	}

	/* Make Timer B table */
	for(i = 0; i < 256; i += 1)
	{
		/* Formula is "time(us) = 1152 * (256 - B) / clock" */
		fm_timerb_tab[i] = (int)((double)(1152 * (256 - i)) / clock);
	}
}

void fm_restore(void)
{
	int i;

	if (FM_GENS) YM2612_Reset();
	else YM2612ResetChip(myFM);

	/* feed all the registers and update internal state */
	for(i = 0; i < 0x100; i++)
	{
		if (FM_GENS)
		{
			YM2612_Write(0, i);
			YM2612_Write(1, fm_reg[0][i]);
			YM2612_Write(2, i);
			YM2612_Write(3, fm_reg[1][i]);
		}
		else
		{
			YM2612Write(myFM, 0, i);
			YM2612Write(myFM, 1, fm_reg[0][i]);
			YM2612Write(myFM, 2, i);
			YM2612Write(myFM, 3, fm_reg[1][i]);
		}
	}
} 

void fm_reset(void)
{
	if (FM_GENS) YM2612_Reset();
	else YM2612ResetChip(myFM);

	/* reset timers status */
	timer[0].running = 0;
	timer[1].running = 0;
	timer[0].enable  = 0;
	timer[1].enable  = 0;
	timer[0].count   = 0;
	timer[1].count   = 0;
	timer[0].base    = 0;
	timer[1].base    = 0;
	timer[0].index   = 0;
	timer[1].index   = 0;

	/* reset FM status */
	fm_status = 0;
}

void fm_write(int address, int data)
{
	int a0 = (address & 1);
	int a1 = (address >> 1) & 1;

	if(a0)
	{
		/* Register data */
		fm_reg[a1][fm_latch[a1]] = data;

		/* Timer control only in set A */
		if(a1 == 0)
		switch(fm_latch[a1])
		{
			case 0x24: /* Timer A (LSB) */
				timer[0].index = ((timer[0].index & 0x0003) | (data << 2)) & 0x03FF;
				if (timer[0].base != fm_timera_tab[timer[0].index])
				{
					timer[0].base = fm_timera_tab[timer[0].index];
					timer[0].count = 0;
				}
				break;
			case 0x25: /* Timer A (MSB) */
				timer[0].index = ((timer[0].index & 0x03FC) | (data & 3)) & 0x03FF;
				if (timer[0].base != fm_timera_tab[timer[0].index])
				{
					timer[0].base = fm_timera_tab[timer[0].index];
					timer[0].count = 0;
				}
				break;
			case 0x26: /* Timer B */
				timer[1].index = data;
				if (timer[1].base != fm_timerb_tab[timer[1].index])
				{
					timer[1].base = fm_timerb_tab[timer[1].index];
					timer[1].count = 0;
				}
				break;
			case 0x27: /* Timer Control */
				/* LOAD */
				timer[0].running = (data & 1);
				timer[1].running = (data & 2);
				/* ENABLE */
				timer[0].enable = (data >> 2) & 1;
				timer[1].enable = (data >> 3) & 1;
				/* RESET */
				if(data & 0x10) fm_status &= ~1;
				if(data & 0x20) fm_status &= ~2;
				break;
		}
	}
	else
	{
		/* Register latch */
		fm_latch[a1] = data;
	}

	if(snd.enabled)
	{
		if(snd.fm.curStage - snd.fm.lastStage > 0)
		{
			if (FM_GENS)
			{
				int *tempBuffer[2];       
				tempBuffer[0] = snd.fm.gens_buffer[0] + snd.fm.lastStage;
				tempBuffer[1] = snd.fm.gens_buffer[1] + snd.fm.lastStage;
				YM2612_Update(tempBuffer, snd.fm.curStage - snd.fm.lastStage);
			}
			else
			{
				int16 *tempBuffer[2];       
				tempBuffer[0] = snd.fm.buffer[0] + snd.fm.lastStage;
				tempBuffer[1] = snd.fm.buffer[1] + snd.fm.lastStage;
				YM2612UpdateOne(myFM, tempBuffer, snd.fm.curStage - snd.fm.lastStage);
			}
			snd.fm.lastStage = snd.fm.curStage;
		}

		if (FM_GENS) YM2612_Write(address & 3, data);
		else YM2612Write(myFM, address & 3, data);
	}
}

int fm_read(int address)
{
	return (fm_status);
}

void fm_update_timers(int inc)
{
	int i;  
	
	/* Process YM2612 timers */
	for(i = 0; i < 2; i += 1)
	{
		/* Is the timer running? */
		if(timer[i].running)
		{
			/* Each scanline takes up roughly 64 microseconds */
			timer[i].count += inc;

			/* Check if the counter overflowed */
			if(timer[i].count >= timer[i].base)
			{
				/* Reload counter */
				timer[i].count -= timer[i].base;

				/* Set overflow flag (if flag setting is enabled) */
				if(timer[i].enable) fm_status |= (1 << i);

				/* Notice FM core (some CH operation on TimerA) */
				if(i==0)
				{
					if (FM_GENS) YM2612TimerAOver();
					else YM2612TimerOver(myFM,0);
				}
			}
		}
	}
}

void psg_write(int data)
{
	if(snd.enabled)
	{
		if(snd.psg.curStage - snd.psg.lastStage > 0)
		{
			int16 *tempBuffer;
			tempBuffer = snd.psg.buffer + snd.psg.lastStage;
			if (PSG_MAME) SN76496Update (0, tempBuffer, snd.psg.curStage - snd.psg.lastStage);
			else SN76489_Update(0, tempBuffer, snd.psg.curStage - snd.psg.lastStage);
			snd.psg.lastStage = snd.psg.curStage;
		}

		if (PSG_MAME) SN76496Write(0, data);
		else SN76489_Write(0, data);
	}
}
