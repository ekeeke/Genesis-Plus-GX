/***************************************************************************************
 *  Genesis Plus 1.2a
 *  M68k Bank access from Z80
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

/*
  Handlers for access to unused addresses and those which make the
  machine lock up.
*/
static inline void z80bank_unused_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Z80 bank unused write %06X = %02X\n", address, data);
#endif
}

static inline unsigned int z80bank_unused_r(unsigned int address)
{
#ifdef LOGERROR
  error("Z80 bank unused read %06X\n", address);
#endif
	return (address & 1) ? 0x00 : 0xFF;
}

static inline void z80bank_lockup_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Z80 bank lockup write %06X = %02X\n", address, data);
#endif
 	gen_running = config.force_dtack;
}

static inline unsigned int z80bank_lockup_r(unsigned int address)
{
#ifdef LOGERROR
  error("Z80 bank lockup read %06X\n", address);
#endif
 	gen_running = config.force_dtack;
  return 0xFF;
}

/* 
	Z80 memory handlers
*/
void z80_write_banked_memory (unsigned int address, unsigned int data)
{
	int offset = address >> 19;

	switch (m68k_writemap_8[offset])
  {
		case VDP:
			/* Valid VDP addresses */
			if (((address >> 16) & 0x07) == 0) 
			{
				switch (address & 0xff)
				{
					case 0x00:	/* Data port */
					case 0x01:
					case 0x02:
					case 0x03:
						vdp_data_w(data << 8 | data);
						return;

					case 0x04:	/* Control port */
					case 0x05:
					case 0x06:
					case 0x07:
						vdp_ctrl_w(data << 8 | data);
						return;

					case 0x10:	/* Unused */
					case 0x12:
					case 0x14:
					case 0x16:
						z80bank_unused_w(address, data);
						return;
					
					case 0x11:	/* PSG */
					case 0x13:
					case 0x15:
					case 0x17:
						psg_write(0, data);
						return;

					case 0x18: /* Unused */
					case 0x19:
					case 0x1a:
					case 0x1b:
						z80bank_unused_w(address, data);
						return;

					case 0x1c: /* Test register */
					case 0x1d:
					case 0x1e:
					case 0x1f:
						vdp_test_w(data << 8 | data);
						return;

					default:	/* Invalid address */
						z80bank_lockup_w(address, data);
						return;
				}
			}

			/* Invalid address */
			z80bank_lockup_w(address, data);
			return;


		case SYSTEM_IO:
		{
			unsigned int base = address >> 8;

			/* Z80 (access prohibited) */
			if (base <= 0xa0ff) 
      {
        z80bank_lockup_w(address, data);
        return;
      }

			/* CONTROL registers */
			if (base <= 0xa1ff)
			{  			
				switch (base & 0xff)
				{
					case 0x00:	/* I/O chip (only gets /LWR) */
						if ((address & 0xe1) == 0x01) io_write((address >> 1) & 0x0f, data);
						else z80bank_unused_w(address, data);
            return;

					case 0x11:	/* BUSREQ */
						if (address & 1) z80bank_unused_w(address, data);
						else gen_busreq_w(data & 1);
		  			return;

					case 0x12:	/* RESET */
						if (address & 1) z80bank_unused_w(address, data);
					  else gen_reset_w(data & 1);
					  return;

					case 0x30:	/* TIME */
						if (cart_hw.time_w) return cart_hw.time_w(address, data);
						else z80bank_unused_w(address, data);
						return;

					case 0x41:	/* BOOTROM */
						if (address & 1)
						{
    					if (data & 1)
    					{
    						rom_readmap[0] = &cart_rom[0];
    						rom_size = genromsize;
    					}
    					else
    					{
    						rom_readmap[0] = &bios_rom[0];
    						rom_size = 0x800;
    					}
    
    					if (!(config.bios_enabled & 2))
    					{
    						config.bios_enabled |= 2;
    						memcpy(bios_rom, cart_rom, 0x800);
    						memset(cart_rom, 0, 0x500000);
    					}
						}
						else z80bank_unused_w (address, data);
						return;

					case 0x10:	/* MEMORY MODE */
					case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x44:	/* RADICA */
					case 0x50:  /* SVP REGISTERS */
						z80bank_unused_w(address, data);
					  return;

					default:	/* Invalid address */
					  z80bank_lockup_w(address, data);
					  return;
				}
			}

			/* Invalid address */
			z80bank_lockup_w(address, data);
			return;
		}

		case ROM:			
			WRITE_BYTE(rom_readmap[offset], address & 0x7ffff, data);
			return;
			
		case SRAM:
			if (address <= sram.end) WRITE_BYTE(sram.sram, address - sram.start, data);
			else z80bank_unused_w(address, data);
			return;

    case EEPROM:
			if ((address == eeprom.type.sda_in_adr) || (address == eeprom.type.scl_adr)) eeprom_write(address, data, 0);
			else z80bank_unused_w(address, data);
			return;
			
    case CART_HW:
			cart_hw.regs_w(address, data);
			return;
		
		case UNUSED: 
			z80bank_unused_w(address, data);
	     return;

    case ILLEGAL:
      z80bank_lockup_w(address, data);
      return;

    default:	/* WRAM */
			z80bank_unused_w(address, data);
      return;
	}
}

unsigned int z80_read_banked_memory(unsigned int address)
{
	int offset = address >> 19;

	switch (m68k_readmap_8[offset])
	{
    case WRAM: /* NOTE: can't be read on some Genesis models (!)*/
      return z80bank_unused_r(address) | 0xff;

		case SYSTEM_IO:
		{
			unsigned int base = address >> 8;

			/* Z80 (access prohibited) */
			if (base <= 0xa0ff) return z80bank_lockup_r(address);
						
			/* I/O & CONTROL registers */
			if (base <= 0xa1ff)
			{
				switch (base & 0xff)
				{
					case 0x00:	/* I/O chip */
						if (address & 0xe0) return z80bank_unused_r(address);
						return (io_read((address >> 1) & 0x0f));

					case 0x11:	/* BUSACK */
						if (address & 0x01) return 0xff;
						else return (zbusack | 0xfe);

					case 0x30:	/* TIME */
						if (cart_hw.time_r) return cart_hw.time_r(address);
						else z80bank_unused_r(address);

					case 0x10:	/* MEMORY MODE */
				  case 0x12:	/* RESET */
				  case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x41:	/* BOOTROM */
					case 0x44:	/* RADICA */
					case 0x50:	/* SVP REGISTERS */
						return z80bank_unused_r(address);
	
		    	default:	/* Invalid address */
		      	return z80bank_lockup_r(address);
		    }
			}	

			/* Invalid address */
		  return z80bank_lockup_r(address);
		}

    case VDP:
			/* Valid VDP addresses */
			if (((address >> 16) & 0x07) == 0) 
			{
			  switch (address & 0xff)
			  {
				  case 0x00:		/* DATA */
				  case 0x02:
				    return (vdp_data_r() >> 8);
			
          case 0x01:		/* DATA */
				  case 0x03:
				    return (vdp_data_r() & 0xff);
			
				  case 0x04:		/* CTRL */
				  case 0x06:
					 return (0xfc | ((vdp_ctrl_r() >> 8) & 3));

				  case 0x05:		/* CTRL */
				  case 0x07:
				    return (vdp_ctrl_r() & 0xff);
			
				  case 0x08:		/* HVC */
				  case 0x0a:
				  case 0x0c:
				  case 0x0e:
				    return (vdp_hvc_r() >> 8);
			
				  case 0x09:		/* HVC */
				  case 0x0b:
				  case 0x0d:
				  case 0x0f:
				   	return (vdp_hvc_r() & 0xff);
			
				  case 0x18:		/* Unused */
				  case 0x19:
				  case 0x1a:
				  case 0x1b:
				  case 0x1c:
				  case 0x1d:
				  case 0x1e:
				  case 0x1f:
				   	return (z80bank_unused_r(address) | 0xff);

					default:		/* Invalid address */
				   	return z80bank_lockup_r(address);
			  }
		  }
      		
		  /* Invalid address */
		  return (z80bank_lockup_r (address));

		case SRAM:
			if (address <= sram.end) return READ_BYTE(sram.sram, address - sram.start);
			return READ_BYTE(rom_readmap[offset], address & 0x7ffff);

		case EEPROM:
      if (address == eeprom.type.sda_out_adr) return eeprom_read(address, 0);
			return READ_BYTE(rom_readmap[offset], address & 0x7ffff);

		case CART_HW:
			return cart_hw.regs_r(address);

		case UNUSED:
			return z80bank_unused_r(address);

		case ILLEGAL:
			return z80bank_lockup_r(address);

		case UMK3_HACK:
			return READ_BYTE(&cart_rom[offset<<19], address & 0x7ffff);

    default:	/* ROM */
			return READ_BYTE(rom_readmap[offset], address & 0x7ffff);
  }
}
