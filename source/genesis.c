/***************************************************************************************
 *  Genesis Plus 1.2a
 *  Genesis internals & Bus arbitration
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

uint8 *cart_rom;          /* cart_rom NEED to be previously dynamically allocated */
uint8 bios_rom[0x800];
uint8 work_ram[0x10000];  /* 68K work RAM */
uint8 zram[0x2000];		  /* Z80 work RAM */
uint8 zbusreq;			  /* /BUSREQ from Z80 */
uint8 zreset;			  /* /RESET to Z80 */
uint8 zbusack;			  /* /BUSACK to Z80 */
uint8 zirq;			      /* /IRQ to Z80 */
uint32 zbank;			  /* Address of Z80 bank window */
uint8 gen_running;
uint32 genromsize;
uint32 rom_size;
int32 resetline;
uint8 *rom_readmap[8];

/*--------------------------------------------------------------------------*/
/* Init, reset, shutdown functions                                          */
/*--------------------------------------------------------------------------*/
void set_softreset(void)
{
	resetline = (int) ((double) (lines_per_frame - 1) * rand() / (RAND_MAX + 1.0));
}

void gen_init (void)
{
	int i;

	/* initialize CPUs */
	m68k_set_cpu_type (M68K_CPU_TYPE_68000);
	m68k_init();
	z80_init(0,0,0,z80_irq_callback);

	/* default 68000 mapping */
	for (i=16; i<24; i++)
	{
		m68k_readmap_8[i]	= ILLEGAL;
		m68k_readmap_16[i]	= ILLEGAL;
		m68k_writemap_8[i]	= ILLEGAL;
		m68k_writemap_16[i]	= ILLEGAL;
	}

	/* Z80, I/O, CONTROL */
	m68k_readmap_8[20]		= SYSTEM_IO;
	m68k_readmap_16[20]		= SYSTEM_IO;
	m68k_writemap_8[20]		= SYSTEM_IO;
	m68k_writemap_16[20]	= SYSTEM_IO;

  /* SEGA PICO */
  if (system_hw == SYSTEM_PICO)
  {
    m68k_readmap_8[16]	  = PICO_HW;
		m68k_readmap_16[16]	  = PICO_HW;
		m68k_writemap_8[16]	  = PICO_HW;
		m68k_writemap_16[16]	= PICO_HW;

		/* Notaz: there is no IO CONTROL area (Z80/YM2612/IO) */
    m68k_readmap_8[20]	  = UNUSED;
		m68k_readmap_16[20]	  = UNUSED;
		m68k_writemap_8[20]	  = UNUSED;
		m68k_writemap_16[20]  = UNUSED;
  }

	/* VDP */
	for (i=24; i<28; i++)
	{
		m68k_readmap_8[i]	= VDP;
		m68k_readmap_16[i]	= VDP;
		m68k_writemap_8[i]	= VDP;
		m68k_writemap_16[i]	= VDP;
	}

	/* WRAM */
	for (i=28; i<32; i++)
	{
		m68k_readmap_8[i]	= WRAM;
		m68k_readmap_16[i]	= WRAM;
		m68k_writemap_8[i]	= WRAM;
		m68k_writemap_16[i]	= WRAM;
	}
}

void gen_reset (unsigned int hard_reset)
{
	if (hard_reset)
	{
		/* Clear RAM */
		memset (work_ram, 0, sizeof (work_ram));
		memset (zram, 0, sizeof (zram));

		/* Reset ROM mapping */
    if (config.bios_enabled == 3)
    {
      rom_readmap[0] = &bios_rom[0];
      rom_size = 0x800;
    }
    else
    {
      rom_readmap[0] = &cart_rom[0];
      rom_size = genromsize;
    }

		uint8 i;
    for (i=1; i<8; i++) rom_readmap[i] = &cart_rom[i << 19];
	}

	gen_running = 1;
	zreset = 0;		/* Z80 is reset */
	zbusreq = 0;	/* Z80 has control of the Z bus */
	zbusack = 1;	/* Z80 is busy using the Z bus */
	zbank = 0;		/* Assume default bank is 000000-007FFF */
	zirq = 0;		/* No interrupts occuring */
	resetline = -1;

	/* Reset CPUs */
	m68k_pulse_reset ();
	z80_reset ();
	_YM2612_Reset();	

#ifdef NGC
  /* register SOFTRESET */
  SYS_SetResetCallback(set_softreset);
#endif

}

void gen_shutdown (void)
{
}

/*-----------------------------------------------------------------------
  Bus controller chip functions                                            
  -----------------------------------------------------------------------*/
unsigned int gen_busack_r (void)
{
	return zbusack;
}

void gen_busreq_w (unsigned int state)
{
	int z80_cycles_to_run;

	input_raz (); /* from Gens */
  
	if (state)
	{
		/* Bus Request */
		if (!zbusreq && zreset)
		{
			/* Z80 stopped */
			/* z80 was ON during the last 68k cycles */
			/* we execute the appropriate number of z80 cycles */
			z80_cycles_to_run = line_z80 + ((count_m68k - line_m68k)*7)/15;
			z80_run(z80_cycles_to_run);
		}
	}
	else
	{
		/* Bus released */
		if (zbusreq && zreset)
		{
			/* Z80 started */
			/* z80 was OFF during the last 68k cycles */
			/* we burn the appropriate number of z80 cycles */
			z80_cycles_to_run = line_z80 + ((count_m68k - line_m68k)*7)/15;
			count_z80 = z80_cycles_to_run;
		}
	}

	zbusreq = state;
	zbusack = 1 ^ (zbusreq & zreset);
}

void gen_reset_w (unsigned int state)
{
	int z80_cycles_to_run;

	if (state)
	{
		/* stop RESET process */
		if (!zbusreq && !zreset)
		{
			/* Z80 started */
			/* z80 was OFF during the last 68k cycles */
			/* we burn the appropriate number of z80 cycles */
			z80_cycles_to_run = line_z80 + ((count_m68k - line_m68k)*7)/15;
			count_z80 = z80_cycles_to_run;
		}
	}
	else
	{
		/* start RESET process */
		if (!zbusreq && zreset)
		{
			/* Z80 stopped */
			/* z80 was ON during the last 68k cycles */
			/* we execute the appropriate number of z80 cycles */
			z80_cycles_to_run = line_z80 + ((count_m68k - line_m68k)*7)/15;
			z80_run(z80_cycles_to_run);
		}

		/* Reset Z80 & YM2612 */
		_YM2612_Reset();
		z80_reset ();
	}

	zreset = state;
	zbusack = 1 ^ (zbusreq & zreset);
}

void gen_bank_w (unsigned int state)
{
	zbank = ((zbank >> 1) | ((state & 1) << 23)) & 0xFF8000;
}

int z80_irq_callback (int param)
{
	zirq = 0;
	z80_set_irq_line (0, CLEAR_LINE);
	return 0xFF;
}
