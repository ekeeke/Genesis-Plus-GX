/*
    Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald

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

uint8 *cart_rom;          /* cart_rom NEED to be previously dynamically allocated */
uint8 work_ram[0x10000]; /* 68K work RAM */
uint8 zram[0x2000];		 /* Z80 work RAM */
uint8 zbusreq;			 /* /BUSREQ from Z80 */
uint8 zreset;			 /* /RESET to Z80 */
uint8 zbusack;			 /* /BUSACK to Z80 */
uint8 zirq;			     /* /IRQ to Z80 */
uint32 zbank;			 /* Address of Z80 bank window */
uint8 gen_running;
uint32 lastbusreqcnt;
uint8 lastbusack;
uint32 genromsize;

static int cpu_sync[512]; /* Z80-68K cycles synchronization table */

#ifdef LSB_FIRST
void bswap(uint8 *mem, int length)
{
    int i;
    for(i = 0; i < length; i += 2)
    {
        uint8 temp = mem[i+0];
        mem[i+0] = mem[i+1];
        mem[i+1] = temp;
    }
}
#endif

/*--------------------------------------------------------------------------*/
/* Init, reset, shutdown functions                                          */
/*--------------------------------------------------------------------------*/

void gen_init (void)
{
	int i;
#ifdef LSB_FIRST
  	bswap(cart_rom, genromsize);
#endif
#ifdef NGC
	ShadowROM ();
#else
  	memcpy(shadow_rom,cart_rom,genromsize);
#endif
	m68k_set_cpu_type (M68K_CPU_TYPE_68000);
	m68k_pulse_reset ();
	gen_running = 1;
	for (i=0; i<512; i++) cpu_sync[i] = (int)(((double)i * 7.0) / 15.0);
}

void gen_reset (void)
{
	/* Clear RAM */
	memset (work_ram, 0, sizeof (work_ram));
	memset (zram, 0, sizeof (zram));
	gen_running = 1;
	zreset = 0;			/* Z80 is reset */
	zbusreq = 0;			/* Z80 has control of the Z bus */
	zbusack = 1;			/* Z80 is busy using the Z bus */
	zbank = 0;			/* Assume default bank is 000000-007FFF */
	zirq = 0;			    /* No interrupts occuring */
	lastbusreqcnt = 0;
	lastbusack = 1;

	/* Reset the 68000 emulator */
	m68k_pulse_reset ();
	z80_reset (0);
	z80_set_irq_callback (z80_irq_callback);
}

void gen_shutdown (void)
{
}

/*-----------------------------------------------------------------------
  Bus controller chip functions                                            
  -----------------------------------------------------------------------*/
int gen_busack_r (void)
{
	if (zbusack == 0)
	{
		if ((count_m68k + m68k_cycles_run() - lastbusreqcnt) > 16)
			return 0; /* bus taken */
		else return (lastbusack&1);
	}
	else return 1;
}

void gen_busreq_w (int state)
{
	int z80_cycles_to_run;

	input_raz ();
  
	if (state == 1)
	{
		/* Bus Request */
		lastbusreqcnt = count_m68k + m68k_cycles_run();
		lastbusack = zbusack;
		if (zbusreq == 0)
		{
			/* Z80 stopped */
			/* z80 was ON during the last 68k cycles */
			/* we execute the appropriate number of z80 cycles */
			z80_cycles_to_run = aim_z80 - cpu_sync[aim_m68k - count_m68k -m68k_cycles_run()];
			if (z80_cycles_to_run > 0) z80_run(z80_cycles_to_run);
		}
	}
	else
	{
		/* Bus released */
		if (zbusreq == 1)
		{
			/* Z80 started */
			/* z80 was OFF during the last 68k cycles */
			/* we burn the appropriate number of z80 cycles */
			z80_cycles_to_run = aim_z80 - cpu_sync[aim_m68k - count_m68k - m68k_cycles_run()];
			if (z80_cycles_to_run > 0) count_z80 = z80_cycles_to_run;
		}
	}
	zbusreq = (state & 1);
	zbusack = 1 ^ (zbusreq & zreset);
}

void gen_reset_w (int state)
{
	zreset = (state & 1);
	zbusack = 1 ^ (zbusreq & zreset);

	if (zreset == 0)
	{
		lastbusreqcnt = 0;
		lastbusack = 1;
		fm_reset();
		z80_reset (0);
		z80_set_irq_callback (z80_irq_callback);
	}
}

void gen_bank_w (int state)
{
	zbank = ((zbank >> 1) | ((state & 1) << 23)) & 0xFF8000;
}

int z80_irq_callback (int param)
{
	zirq = 0;
	z80_set_irq_line (0, CLEAR_LINE);
	return 0xFF;
}

int vdp_int_ack_callback (int int_level)
{
	switch (int_level)
	{
		case 4:
			hint_pending = 0;
			vint_pending = 0;
			break;
		case 6:
			vint_pending = 0;
			break;
	}
	return M68K_INT_ACK_AUTOVECTOR;
}
