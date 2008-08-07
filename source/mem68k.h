/***************************************************************************************
 *  Genesis Plus 1.2a
 *  68k memory handlers
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

#ifndef _MEM68K_H_
#define _MEM68K_H_

enum {
  SRAM,
  EEPROM,
  J_CART,
  SVP_DRAM,
  SVP_CELL,
  CART_HW,
	REALTEC_ROM,
	VDP,
	SYSTEM_IO,
	UNUSED,
	ILLEGAL,
	WRAM,
  UMK3_HACK,
  PICO_HW,
  ROM
};

extern uint8 m68k_readmap_8[32];
extern uint8 m68k_readmap_16[32];
extern uint8 m68k_writemap_8[32];
extern uint8 m68k_writemap_16[32];


#endif /* _MEM68K_H_ */
