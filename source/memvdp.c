/***************************************************************************************
 *  Genesis Plus 1.2a
 *  68k memory from VDP handler
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

unsigned int vdp_dma_r(unsigned int address)
{
	int offset = address >> 19;

	switch (m68k_readmap_16[offset])
	{
		case ROM:
			if (svp) address -= 2;
			return *(uint16 *)(rom_readmap[offset] + (address & 0x7ffff));

		case UMK3_HACK:
			return *(uint16 *)(&cart_rom[offset << 19] + (address & 0x7ffff));

    	case SVP_DRAM:
			address -= 2;
			return *(uint16 *)(svp->dram + (address & 0x1fffe));
			
		case SVP_CELL:
			address -= 2;
			switch (address >> 16)
			{
				case 0x39:
					address >>= 1;
					address = (address & 0x7001) | ((address & 0x3e) << 6) | ((address & 0xfc0) >> 5);
					return ((uint16 *)svp->dram)[address];
				
				case 0x3A:
					address >>= 1;
					address = (address & 0x7801) | ((address & 0x1e) << 6) | ((address & 0x7e0) >> 4);
					return ((uint16 *)svp->dram)[address];
				
				default:
					return 0xffff;
			}

    case SYSTEM_IO:
      /* Z80 area */
			/* Return $FFFF only when the Z80 isn't hogging the Z-bus.
        (e.g. Z80 isn't reset and 68000 has the bus) */
			if (address <= 0xa0ffff) return (zbusack ? *(uint16 *)(work_ram + (address & 0xffff)) : 0xffff);

			/* The I/O chip and work RAM try to drive the data bus which
        results in both values being combined in random ways when read.
        We return the I/O chip values which seem to have precedence, */
			if (address <= 0xa1001f)
      {
			  int temp = io_read((address >> 1) & 0x0f);
        return (temp << 8 | temp);
      }

      /* All remaining locations access work RAM */
			return *(uint16 *)(work_ram + (address & 0xffff));

		case SRAM:
			if (address <= sram.end) return *(uint16 *)(sram.sram + (address - sram.start));
			return *(uint16 *)(rom_readmap[address >> 19] + (address & 0x7ffff));
		
		case EEPROM:
			if (address == eeprom.type.sda_out_adr) return eeprom_read(address);
			return *(uint16 *)(rom_readmap[address >> 19] + (address & 0x7ffff));

		case J_CART:
			if (address == eeprom.type.sda_out_adr) return eeprom_read(address); /* some games also have EEPROM mapped here */
			else return jcart_read();

		default:
			return *(uint16 *)(work_ram + (address & 0xffff));
  }
}
