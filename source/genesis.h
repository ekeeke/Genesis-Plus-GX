/***************************************************************************************
 *  Genesis Plus 1.2a
 *  Genesis internals & Bus controller
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

#ifndef _GENESIS_H_
#define _GENESIS_H_

/* Global variables */
extern uint8 *cart_rom;
extern uint8 bios_rom[0x800];
extern uint8 work_ram[0x10000];
extern uint8 zram[0x2000];
extern uint8 zbusreq;
extern uint8 zbusack;
extern uint8 zreset;
extern uint8 zirq;
extern uint32 zbank;
extern uint8 gen_running;
extern uint32 genromsize;
extern uint32 rom_size;
extern int32 resetline;
extern uint8 *rom_readmap[8];

/* Function prototypes */
extern void gen_init(void);
extern void gen_reset(unsigned int hard_reset);
extern void gen_shutdown(void);
extern unsigned int gen_busack_r(void);
extern void gen_busreq_w(unsigned int state);
extern void gen_reset_w(unsigned int state);
extern void gen_bank_w(unsigned int state);
extern int z80_irq_callback(int param);
extern void set_softreset(void);

#endif /* _GEN_H_ */

