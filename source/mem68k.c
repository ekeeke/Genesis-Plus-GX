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

#include "m68kcpu.h"
#include "shared.h"

uint8 m68k_readmap_8[32];
uint8 m68k_readmap_16[32];
uint8 m68k_writemap_8[32];
uint8 m68k_writemap_16[32];


uint8 pico_current;
static uint8 pico_page[7] = {0x00,0x01,0x03,0x07,0x0F,0x1F,0x3F};


static inline unsigned int m68k_read_bus_8(unsigned int address)
{
#ifdef LOGERROR
	if ((address != 0xa11100) && (address != 0xc00004)  && (address != 0xc00006))
	{
		error("Unused read8 %08X (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
		error("D0 = %x \n", m68k_get_reg (NULL, M68K_REG_D0));
		error("D1 = %x \n", m68k_get_reg (NULL, M68K_REG_D1));
		error("D2 = %x \n", m68k_get_reg (NULL, M68K_REG_D2));
		error("D3 = %x \n", m68k_get_reg (NULL, M68K_REG_D3));
		error("D4 = %x \n", m68k_get_reg (NULL, M68K_REG_D4));
		error("D5 = %x \n", m68k_get_reg (NULL, M68K_REG_D5));
		error("D6 = %x \n", m68k_get_reg (NULL, M68K_REG_D6));
		error("D7 = %x \n", m68k_get_reg (NULL, M68K_REG_D7));
		error("A0 = %x \n", m68k_get_reg (NULL, M68K_REG_A0));
		error("A1 = %x \n", m68k_get_reg (NULL, M68K_REG_A1));
		error("A2 = %x \n", m68k_get_reg (NULL, M68K_REG_A2));
		error("A3 = %x \n", m68k_get_reg (NULL, M68K_REG_A3));
		error("A4 = %x \n", m68k_get_reg (NULL, M68K_REG_A4));
		error("A5 = %x \n", m68k_get_reg (NULL, M68K_REG_A5));
		error("A6 = %x \n", m68k_get_reg (NULL, M68K_REG_A6));
		error("A7 = %x \n", m68k_get_reg (NULL, M68K_REG_A7));
	}
#endif
  int bus_addr = (REG_PC & 0xffffe) | (address & 1);
 	int offset = bus_addr >> 19;

	if (offset > 8) return READ_BYTE(work_ram, bus_addr & 0xffff);
	else return READ_BYTE(rom_readmap[offset], bus_addr & 0x7ffff);
}

static inline unsigned int m68k_read_bus_16(unsigned int address)
{
#ifdef LOGERROR
	if ((address != 0xa11100) && (address != 0xc00004)  && (address != 0xc00006))
	{
		error("Unused read16 %08X (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
		error("D0 = %x \n", m68k_get_reg (NULL, M68K_REG_D0));
		error("D1 = %x \n", m68k_get_reg (NULL, M68K_REG_D1));
		error("D2 = %x \n", m68k_get_reg (NULL, M68K_REG_D2));
		error("D3 = %x \n", m68k_get_reg (NULL, M68K_REG_D3));
		error("D4 = %x \n", m68k_get_reg (NULL, M68K_REG_D4));
		error("D5 = %x \n", m68k_get_reg (NULL, M68K_REG_D5));
		error("D6 = %x \n", m68k_get_reg (NULL, M68K_REG_D6));
		error("D7 = %x \n", m68k_get_reg (NULL, M68K_REG_D7));
		error("A0 = %x \n", m68k_get_reg (NULL, M68K_REG_A0));
		error("A1 = %x \n", m68k_get_reg (NULL, M68K_REG_A1));
		error("A2 = %x \n", m68k_get_reg (NULL, M68K_REG_A2));
		error("A3 = %x \n", m68k_get_reg (NULL, M68K_REG_A3));
		error("A4 = %x \n", m68k_get_reg (NULL, M68K_REG_A4));
		error("A5 = %x \n", m68k_get_reg (NULL, M68K_REG_A5));
		error("A6 = %x \n", m68k_get_reg (NULL, M68K_REG_A6));
		error("A7 = %x \n", m68k_get_reg (NULL, M68K_REG_A7));
	}
#endif
  int bus_addr = REG_PC & 0xfffffe;
 	int offset = bus_addr >> 19;
 	
  if (offset > 8) return *(uint16 *)(work_ram + (bus_addr & 0xffff));
  else return *(uint16 *)(rom_readmap[offset] + (bus_addr & 0x7ffff));
}


static inline void m68k_unused_8_w (unsigned int address, unsigned int value)
{
#ifdef LOGERROR
	error("Unused write8 %08X = %02X (%08X)\n", address, value, m68k_get_reg (NULL, M68K_REG_PC));
#endif
}

static inline void m68k_unused_16_w (unsigned int address, unsigned int value)
{
#ifdef LOGERROR
	error("Unused write16 %08X = %04X (%08X)\n", address, value, m68k_get_reg (NULL, M68K_REG_PC));
#endif
}

/*
  Functions to handle memory accesses which cause the Genesis to halt
  either temporarily (press RESET button to restart) or unrecoverably
  (cycle power to restart).
*/

static inline void m68k_lockup_w_8 (unsigned int address, unsigned int value)
{
#ifdef LOGERROR
	error ("Lockup %08X = %02X (%08X)\n", address, value, m68k_get_reg (NULL, M68K_REG_PC));
#endif
	gen_running = config.force_dtack;
	if (!gen_running) m68k_end_timeslice ();
}

static inline void m68k_lockup_w_16 (unsigned int address, unsigned int value)
{
#ifdef LOGERROR
	error ("Lockup %08X = %04X (%08X)\n", address, value, m68k_get_reg (NULL, M68K_REG_PC));
#endif
	gen_running = config.force_dtack;
	if (!gen_running) m68k_end_timeslice ();
}

static inline unsigned int m68k_lockup_r_8 (unsigned int address)
{ 
#ifdef LOGERROR
	error ("Lockup %08X.b (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
#endif
	gen_running = config.force_dtack;
	if (!gen_running) m68k_end_timeslice ();
	return -1;
}

static inline unsigned int m68k_lockup_r_16 (unsigned int address)
{
#ifdef LOGERROR
	error ("Lockup %08X.w (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
#endif
	gen_running = config.force_dtack;
	if (!gen_running) m68k_end_timeslice ();
	return -1;
}

/*--------------------------------------------------------------------------*/
/* 68000 memory handlers                                                    */
/*--------------------------------------------------------------------------*/
unsigned int m68k_read_memory_8(unsigned int address)
{
	int offset = address >> 19;

	switch (m68k_readmap_8[offset])
	{
		case WRAM:
      return READ_BYTE(work_ram, address & 0xffff);

		case SYSTEM_IO:
		{
			unsigned int base = address >> 8;

			/* Z80 */
			if (base <= 0xa0ff) 
      {
				/* Z80 controls Z bus ? */
				if (zbusack) return m68k_read_bus_8(address);
	  			
				/* Read data from Z bus */
				switch (base & 0x60)
				{
					case 0x40: /* YM2612 */
						return fm_read(0, address & 3);

					case 0x60: /* VDP */
						if (base == 0xa07f) return m68k_lockup_r_8(address);
						return (m68k_read_bus_8(address) | 0xff);

					default: /* ZRAM */
					   return zram[address & 0x1fff];
				}
			}
						
			/* I/O & CONTROL registers */
			if (base <= 0xa1ff)
			{
				switch (base & 0xff)
				{
					case 0x00:	/* I/O chip */
						if (address & 0xe0) return m68k_read_bus_8(address);
						return (io_read((address >> 1) & 0x0f));

					case 0x11:	/* BUSACK */
						return (zbusack | (address & 1) | (m68k_read_bus_8(address) & 0xFE));

					case 0x30:	/* TIME */
						if (cart_hw.time_r) return cart_hw.time_r(address);
						else return m68k_read_bus_8(address);

					case 0x10:	/* MEMORY MODE */
				  case 0x12:	/* RESET */
				  case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x41:	/* BOOTROM */
					case 0x44:	/* RADICA */
					case 0x50:	/* SVP REGISTERS */
						return m68k_read_bus_8(address);
	
		    	default:	/* Invalid address */
		      	return m68k_lockup_r_8(address);
		    }
			}
			
			/* Invalid address */
		  return m68k_lockup_r_8(address);
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
				    return ((m68k_read_bus_8(address) & 0xfc) | ((vdp_ctrl_r() >> 8) & 3));

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

					case 0x18:
					case 0x19:
					case 0x1a:
					case 0x1b:
					case 0x1c:
					case 0x1d:
					case 0x1e:
					case 0x1f:	/* Unused */
				   	return m68k_read_bus_8(address);

					default:	/* Invalid address */
						return m68k_lockup_r_8(address);
				}
			}

			/* Invalid address */
			return (m68k_lockup_r_8 (address));

	 	case SRAM:
			if (address <= sram.end) return READ_BYTE(sram.sram, address - sram.start);
			return READ_BYTE(rom_readmap[offset], address & 0x7ffff);

		case EEPROM:
			if (address == eeprom.type.sda_out_adr) return eeprom_read(address, 0);
			return READ_BYTE(rom_readmap[offset], address & 0x7ffff);

		case CART_HW:
			return cart_hw.regs_r(address);

		case UNUSED:
			return m68k_read_bus_8(address);

		case ILLEGAL:
			return m68k_lockup_r_8(address);

		case UMK3_HACK:
			return READ_BYTE(&cart_rom[offset<<19], address & 0x7ffff);

 		case PICO_HW:
      switch (address & 0xff)
      {
        case 0x01:  /* VERSION register */
          return (0x40);

        case 0x03:  /* IO register */
        {
          uint8 retval = 0xff;
          if (input.pad[0] & INPUT_B)     retval &= ~0x80;
          if (input.pad[0] & INPUT_A)     retval &= ~0x10;
          if (input.pad[0] & INPUT_UP)    retval &= ~0x01;
          if (input.pad[0] & INPUT_DOWN)  retval &= ~0x02;
          if (input.pad[0] & INPUT_LEFT)  retval &= ~0x04;
          if (input.pad[0] & INPUT_RIGHT) retval &= ~0x08;
          retval &= ~0x20;
          retval &= ~0x40;
          return retval;
        }

        case 0x05:  /* MSB PEN X coordinate */
          return (input.analog[0][0] >> 8);

        case 0x07:  /* LSB PEN X coordinate */
          return (input.analog[0][0] & 0xff);

        case 0x09:  /* MSB PEN Y coordinate */
          return (input.analog[0][1] >> 8);

        case 0x0b:  /* LSB PEN Y coordinate */
          return (input.analog[0][1] & 0xff);

        case 0x0d:  /* PAGE register */
          return pico_page[pico_current]; /* TODO */

        case 0x10:  /* PCM registers */
        case 0x11:
        case 0x12:
        case 0x13:
          return 0x80; /* TODO */
        
  		  default:
			    return m68k_read_bus_8(address);
    }

    default:	/* ROM */
			return READ_BYTE(rom_readmap[offset], address & 0x7ffff);
  }
}


unsigned int m68k_read_memory_16 (unsigned int address)
{
	int offset = address >> 19;

	switch (m68k_readmap_16[offset])
	{
    case WRAM:
      return *(uint16 *)(work_ram + (address & 0xffff));

		case SVP_DRAM:
			return *(uint16 *)(svp->dram + (address & 0x1fffe));
			
		case SVP_CELL:
			switch (address >> 16)
			{
				case 0x39:
					address >>= 1;
					address = (address & 0x7001) | ((address & 0x3e) << 6) | ((address & 0xfc0) >> 5);
					return *(uint16 *)(svp->dram + (address & 0x1fffe));
				
				case 0x3A:
					address >>= 1;
					address = (address & 0x7801) | ((address & 0x1e) << 6) | ((address & 0x7e0) >> 4);
					return *(uint16 *)(svp->dram + (address & 0x1fffe));
				
				default:
					return m68k_read_bus_16(address);
			}

    case SYSTEM_IO:
		{
		  unsigned int base = address >> 8;

		  /* Z80 */
		  if (base <= 0xa0ff) 
	    {
			  /* Z80 controls Z bus ? */
			  if (zbusack) return m68k_read_bus_16(address);
	  			
				/* Read data from Z bus */
				switch (base & 0x60)
				{
					case 0x40: /* YM2612 */
					{
						int temp = fm_read(0, address & 3);
					  return (temp << 8 | temp);
					}

					case 0x60: /* VDP */
						if (base == 0xa07f) return m68k_lockup_r_16(address);
						return (m68k_read_bus_16(address) | 0xffff);

					default: /* ZRAM */
					{
						int temp = zram[address & 0x1fff];
					  return (temp << 8 | temp);
					}
				}
			}

			/* CONTROL registers */
			if (base <= 0xa1ff)
			{
				switch (base & 0xff)
				{
					case 0x00:	/* I/O chip */
					{
						if (address & 0xe0) return m68k_read_bus_16(address);
	    			int temp = io_read((address >> 1) & 0x0f);
	      		return (temp << 8 | temp);
					}

					case 0x11:	/* BUSACK */
						return ((m68k_read_bus_16(address) & 0xfeff) | (zbusack << 8));

					case 0x50:	/* SVP */
						if (svp)
            {
              switch (address & 0xff)
              {
                case 0:
                case 2:
                  return svp->ssp1601.gr[SSP_XST].h;

                case 4:
                {
                  unsigned int temp = svp->ssp1601.gr[SSP_PM0].h;
                  svp->ssp1601.gr[SSP_PM0].h &= ~1;
                  return temp;
                }
              }
						}
            return m68k_read_bus_16(address);

					case 0x30:	/* TIME */
						if (cart_hw.time_r) return cart_hw.time_r(address);
						else return m68k_read_bus_16(address);

					case 0x10:	/* MEMORY MODE */
					case 0x12:	/* RESET */
					case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x41:	/* BOOTROM */
					case 0x44:	/* RADICA */
						return m68k_read_bus_16(address);
		
					default:	/* Invalid address */
						return m68k_lockup_r_16(address);
				}
      }

			/* Invalid address */
			return m68k_lockup_r_16 (address);
		}

		case VDP:

			/* Valid VDP addresses */
			if (((address >> 16) & 0x07) == 0) 
			{
				switch (address & 0xfc)
			  {
				  case 0x00:	/* DATA */
				   	return vdp_data_r();
				
				  case 0x04:	/* CTRL */
				   	return ((vdp_ctrl_r() & 0x3FF) | (m68k_read_bus_16(address) & 0xFC00));
			
				  case 0x08:	/* HVC */
				  case 0x0c:
				   	return vdp_hvc_r();
			
					case 0x18:	/* Unused */
				  case 0x1c:
				   	return m68k_read_bus_16(address);

					default:
				   	return m68k_lockup_r_16(address);
			  }
			}

			/* Invalid address */
			return m68k_lockup_r_16 (address);
		
		case SRAM:
			if (address <= sram.end) return *(uint16 *)(sram.sram + (address - sram.start));
			return *(uint16 *)(rom_readmap[offset] + (address & 0x7ffff));
		
		case EEPROM:
      if (address == (eeprom.type.sda_out_adr & 0xfffffe)) return eeprom_read(address, 1);
			return *(uint16 *)(rom_readmap[offset] + (address & 0x7ffff));

		case J_CART:
			return jcart_read();
			
		case REALTEC_ROM:
			return *(uint16 *)(&cart_rom[0x7e000] + (address & 0x1fff));

		case UNUSED:
			return m68k_read_bus_16(address);

		case ILLEGAL:
			return m68k_lockup_r_16(address);

		case UMK3_HACK:
			return *(uint16 *)(&cart_rom[offset << 19] + (address & 0x7ffff));

 		case PICO_HW:
      switch (address & 0xff)
      {
        case 0x00:  /* VERSION register */
          return (0x40);

        case 0x02:  /* IO register */
        {
          uint8 retval = 0xff;
          if (input.pad[0] & INPUT_B)     retval &= ~0x80;
          if (input.pad[0] & INPUT_A)     retval &= ~0x10;
          if (input.pad[0] & INPUT_UP)    retval &= ~0x01;
          if (input.pad[0] & INPUT_DOWN)  retval &= ~0x02;
          if (input.pad[0] & INPUT_LEFT)  retval &= ~0x04;
          if (input.pad[0] & INPUT_RIGHT) retval &= ~0x08;
          retval &= ~0x20;
          retval &= ~0x40;
          return retval;
        }

        case 0x04:  /* MSB PEN X coordinate */
          return (input.analog[0][0] >> 8);

        case 0x06:  /* LSB PEN X coordinate */
          return (input.analog[0][0] & 0xff);

        case 0x08:  /* MSB PEN Y coordinate */
          return (input.analog[0][1] >> 8);

        case 0x0a:  /* LSB PEN Y coordinate */
          return (input.analog[0][1] & 0xff);

        case 0x0c:  /* PAGE register */
          return pico_page[pico_current]; /* TODO */

        case 0x10:  /* PCM data register */
          return 0x8000; /* TODO */

        case 0x12:  /* PCM control registe */
          return 0x8000; /* TODO */

       default:
          return m68k_read_bus_16(address);
      }

    default:	/* ROM */
			return *(uint16 *)(rom_readmap[offset] + (address & 0x7ffff));
  }
}


unsigned int m68k_read_memory_32(unsigned int address)
{
  /* Split into 2 reads */
	return ((m68k_read_memory_16 (address) << 16) |
	  		  (m68k_read_memory_16 ((address+ 2)&0xffffff)));
}


void m68k_write_memory_8(unsigned int address, unsigned int value)
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
						vdp_data_w(value << 8 | value);
						return;

					case 0x04:	/* Control port */
					case 0x05:
					case 0x06:
					case 0x07:
						vdp_ctrl_w(value << 8 | value);
						return;

					case 0x10:	/* Unused */
					case 0x12:
					case 0x14:
					case 0x16:
						m68k_unused_8_w(address, value);
						return;
					
					case 0x11:	/* PSG */
					case 0x13:
					case 0x15:
					case 0x17:
						psg_write(0, value);
						return;

					case 0x18: /* Unused */
					case 0x19:
					case 0x1a:
					case 0x1b:
						m68k_unused_8_w(address, value);
						return;

					case 0x1c:  /* Test register */
					case 0x1d:
					case 0x1e:
					case 0x1f:
						vdp_test_w(value << 8 | value);
						return;

					default:	/* Invalid address */
						m68k_lockup_w_8(address, value);
						return;
				}
			}

			/* Invalid address */
			m68k_lockup_w_8(address, value);
			return;


		case SYSTEM_IO:
		{
			unsigned int base = address >> 8;

			/* Z80 */
			if (base <= 0xa0ff) 
	    {
			 	/* Writes are ignored when the Z80 hogs the Z-bus */
			 	if (zbusack)
			  {
			   	m68k_unused_8_w (address, value);
			   	return;
			  }
           
				/* Read data from Z bus */
				switch (base & 0x60)
				{
					case 0x40: /* YM2612 */
						fm_write(0, address & 3, value);
						return;

					case 0x60:
						switch (base & 0x7f)
						{
							case 0x60:	/* Bank register */
								gen_bank_w(value & 1);
								return;

							case 0x7f:	/* VDP */
								m68k_lockup_w_8(address, value);
								return;

							default:
								m68k_unused_8_w(address, value);
								return;
						}

					default: /* ZRAM */
						zram[address & 0x1fff] = value;
            count_m68k ++;
						return;
				}
			}

			/* CONTROL registers */
			if (base <= 0xa1ff)
			{  			
				switch (base & 0xff)
				{
					case 0x00:	/* I/O chip (only gets /LWR) */
						if ((address & 0xe1) == 0x01) io_write((address >> 1) & 0x0f, value);
						else m68k_unused_8_w(address, value);
		  				return;

					case 0x11:	/* BUSREQ */
						if (address & 1) m68k_unused_8_w(address, value);
						else gen_busreq_w(value & 1);
		  				return;

					case 0x12:	/* RESET */
						if (address & 1) m68k_unused_8_w(address, value);
					  	else gen_reset_w(value & 1);
					  	return;

					case 0x30:	/* TIME */
						if (cart_hw.time_w) cart_hw.time_w(address, value);
						else m68k_unused_8_w(address, value);
						return;

					case 0x41:	/* BOOTROM */
						if (address & 1)
						{
    					if (value & 1)
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
						else m68k_unused_8_w (address, value);
						return;

					case 0x10:	/* MEMORY MODE */
					case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x44:	/* RADICA */
					case 0x50:  /* SVP REGISTERS */
						m68k_unused_8_w(address, value);
					  	return;

					default:	/* Invalid address */
					  	m68k_lockup_w_8(address, value);
					  	return;
				}
			}

			/* Invalid address */
			m68k_lockup_w_8(address, value);
			return;
		}

		case ROM:			
			WRITE_BYTE(rom_readmap[offset], address & 0x7ffff, value);
			return;
			
		case SRAM:
			if (address <= sram.end) WRITE_BYTE(sram.sram, address - sram.start, value);
			else m68k_unused_8_w(address, value);
			return;
		
		case EEPROM:
      if ((address == eeprom.type.sda_in_adr) || (address == eeprom.type.scl_adr)) eeprom_write(address, value, 0);
			else m68k_unused_8_w(address, value);
			return;
			
    	case CART_HW:
			cart_hw.regs_w(address, value);
			return;
		
		case UNUSED: 
			m68k_unused_8_w(address, value);
	    return;

		case ILLEGAL: 
      m68k_lockup_w_8(address, value);
      return;

 		case PICO_HW:
      switch (address & 0xff)
      {
        case 0x19:
        case 0x1b:
        case 0x1d:
        case 0x1f:  /* TMSS register */
          return;
        
        default:
          m68k_unused_8_w(address, value);
          return;
      }

		default:	/* WRAM */
			WRITE_BYTE(work_ram, address & 0xffff, value);
      return;
  }
}


void m68k_write_memory_16 (unsigned int address, unsigned int value)
{
	int offset = address >> 19;

	switch (m68k_writemap_16[offset])
	{
		case SVP_DRAM:
			*(uint16 *)(svp->dram + (address & 0x1fffe)) = value;
			if ((address == 0x30fe06) && value) svp->ssp1601.emu_status &= ~SSP_WAIT_30FE06;
			if ((address == 0x30fe08) && value) svp->ssp1601.emu_status &= ~SSP_WAIT_30FE08;
			return;
		
   	case VDP:
			/* Valid VDP addresses */
			if (((address >> 16) & 0x07) == 0)
			{
				switch (address & 0xfc)
				{
			    case 0x00:	/* DATA */
				   	vdp_data_w(value);
		      	return;
		
			    case 0x04:	/* CTRL */
			    	vdp_ctrl_w(value);
			    	return;
		
				  case 0x10:	/* PSG */
				  case 0x14:
						psg_write(0, value & 0xff);
			    	return;
		
				  case 0x18:	/* Unused */
				   	m68k_unused_16_w(address, value);
				   	return;

				  case 0x1c:	/* Test register */
					  vdp_test_w(value);
					  return;

					default:	/* Invalid address */
						m68k_lockup_w_16 (address, value);
						return;
				}
			}

			/* Invalid address */
			m68k_lockup_w_16 (address, value);
			return;


    case SYSTEM_IO:
		{
			unsigned int base = address >> 8;

			/* Z80 */
			if (base <= 0xa0ff) 
	    {
			 	/* Writes are ignored when the Z80 hogs the Z-bus */
			 	if (zbusack)
			  {
			   	m68k_unused_16_w (address, value);
					return;
			  }
		
			 	/* Write into Z80 address space */
				switch (base & 0x60)
				{
					case 0x40: /* YM2612 */
						fm_write (0, address & 3, value >> 8);
						return;

					case 0x60:
						switch (base & 0x7f)
						{
							case 0x60:	/* Bank register */
								gen_bank_w ((value >> 8) & 1);
								return;

							case 0x7f:	/* VDP */
								m68k_lockup_w_16(address, value);
								return;

							default:
								m68k_unused_16_w(address, value);
								return;
						}

					default: /* ZRAM */
						zram[address & 0x1fff] = value >> 8;
						return;
				}
			}
			
			/* CONTROL registers */
			if (base <= 0xa1ff)
	    {				
				switch (base & 0xff)
				{
					case 0x00:	/* I/O chip */
						if (address & 0xe0) m68k_unused_16_w (address, value);
						else io_write ((address >> 1) & 0x0f, value & 0xff);
	      		return;
	    	
					case 0x11:	/* BUSREQ */
					  gen_busreq_w ((value >> 8) & 1);
					  return;
				
					case 0x12:	/* RESET */
					  gen_reset_w ((value >> 8) & 1);
					  return;
						
					case 0x50:  /* SVP REGISTERS */
						if (svp && ((address & 0xfd) == 0))
						{
								/* just guessing here (Notaz) */
								svp->ssp1601.gr[SSP_XST].h = value;
								svp->ssp1601.gr[SSP_PM0].h |= 2;
								svp->ssp1601.emu_status &= ~SSP_WAIT_PM0;
								return;
            }
            m68k_unused_16_w(address, value);
            return;

					case 0x30:	/* TIME */
						if (cart_hw.time_w)
						{
							cart_hw.time_w(address & 0xfe, value >> 8);
							cart_hw.time_w(address, value & 0xff);
						}
						else m68k_unused_16_w (address, value);
						return;
					
					case 0x41:	/* BOOTROM */
						if (value & 1)
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
						return;
					
					case 0x10:	/* MEMORY MODE */
					case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x44:	/* RADICA */
					  m68k_unused_16_w (address, value);
					  return;
						
					default:	/* Unused */
					  m68k_lockup_w_16 (address, value);
					  return;
	    	}
			}

			/* Invalid address */
			m68k_lockup_w_16 (address, value);
			return;
		}

		case ROM:
			*(uint16 *)(rom_readmap[offset] + (address & 0x7ffff)) = value;
			return;

		case SRAM:
			if (address <= sram.end) *(uint16 *)(sram.sram + (address - sram.start)) = value;
			else m68k_unused_16_w (address, value);
			return;

		case EEPROM:
      if ((address == (eeprom.type.sda_in_adr&0xfffffe)) || (address == (eeprom.type.scl_adr&0xfffffe)))
        eeprom_write(address, value, 1);
			else m68k_unused_16_w (address, value);
			return;
		
		case J_CART:
			jcart_write(value);
			return;

		case UNUSED:
      m68k_unused_16_w (address, value);
      return;

		case ILLEGAL: 
			m68k_lockup_w_16 (address, value);
      return;

 		case PICO_HW:
      switch (address & 0xff)
      {
        case 0x10:  /* PCM data register */
          return; /* TODO */
        
        case 0x12:  /* PCM control resiter */
          return; /* TODO */
          
        case 0x18:
        case 0x1a:
        case 0x1c:
        case 0x1e:  /* TMSS register */
          return;
        
        default:
          m68k_unused_16_w(address, value);
          return;
      }

		default:	/* WRAM */
			*(uint16 *)(work_ram + (address & 0xffff)) = value;
      return;
	}
}

void m68k_write_memory_32 (unsigned int address, unsigned int value)
{
  /* Split into 2 writes */
  m68k_write_memory_16 (address, value >> 16);
  m68k_write_memory_16 ((address+2) & 0xffffff, value & 0xffff);
}
