#include "shared.h"

static uint16 next_int = 1;
static uint8 prot_bytes[2]; /* simple protection faking (from Picodrive) */

unsigned int m68k_read_bus_8(unsigned int address)
{
   uint16 temp = m68k_read_bus_16(address);
   return ((address & 1) ? (temp & 0xFF) : (temp >> 8));
}

unsigned int m68k_read_bus_16(unsigned int address)
{
   next_int ^= 0xFFFF;
   return next_int;
}


void m68k_unused_w (unsigned int address, unsigned int value)
{
	error("Unused 3 %08X = %08X \n", address, value);
}

void m68k_unused_8_w (unsigned int address, unsigned int value)
{
	error("Unused 2 %08X = %02X \n", address, value);
}

void m68k_unused_16_w (unsigned int address, unsigned int value)
{
	error("Unused 1 %08X = %04X \n", address, value);
}

/*
    Functions to handle memory accesses which cause the Genesis to halt
    either temporarily (press RESET button to restart) or unrecoverably
    (cycle power to restart).
*/

void m68k_lockup_w_8 (unsigned int address, unsigned int value)
{
	error ("Lockup %08X = %02X (%08X)\n", address, value, m68k_get_reg (NULL, M68K_REG_PC));
	gen_running = 0;
	m68k_end_timeslice ();
}

void m68k_lockup_w_16 (unsigned int address, unsigned int value)
{
	error ("Lockup %08X = %04X (%08X)\n", address, value, m68k_get_reg (NULL, M68K_REG_PC));
	gen_running = 0;
	m68k_end_timeslice ();
}

unsigned int m68k_lockup_r_8 (unsigned int address)
{ 
	error ("Lockup %08X.b (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
	gen_running = 0;
	m68k_end_timeslice ();
	return -1;
}

unsigned int m68k_lockup_r_16 (unsigned int address)
{
	error ("Lockup %08X.w (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
	gen_running = 0;
	m68k_end_timeslice ();
	return -1;
}


/*--------------------------------------------------------------------------*/
/* 68000 memory handlers                                                    */
/*--------------------------------------------------------------------------*/

unsigned int m68k_read_memory_8 (unsigned int address)
{
	switch ((address >> 21) & 7)
    {
		case 0:		/* ROM Cartridge*/
		case 1:
			/* SRAM */
      		if (sram.on)
			{
				if (address >= sram.start && address <= sram.end)
				{
	   				if (sram.custom) return (EEPROM_Read(address)&0xffff);
					return READ_BYTE(sram.sram, (address - sram.start) & 0xffff);
				}
			}

			/* ROM data */
			if (address < genromsize) return READ_BYTE(cart_rom, address);
			
			/* default */
			return 0x00;

		case 7:		/* RAM */
      		return READ_BYTE(work_ram, address & 0xFFFF);

		case 5:		/* Z80 & I/O */
			if (address <= 0xA0FFFF) /* Z80 area */
			{
				/* Z80 controls Z bus */
				if (zbusack == 1) return (m68k_read_bus_8 (address));
				else
				{
					/* Read data from Z bus */
					switch (address & 0x6000)
					{
						case 0x0000:	/* RAM */
						case 0x2000:
							return (zram[(address & 0x1FFF)]);

						case 0x4000:	/* YM2612 */
							return (fm_read (address & 3));

						case 0x6000:	/* Unused */
							switch (address & 0xFF00)
							{
								case 0x7F00:	/* VDP */
									m68k_lockup_r_8 (address);

								default:	/* Unused */
									return (0xFF);
							}
							break;
					}
				}
			}
      		else if (address <= 0xA1001F) 
			{		  		
				/* I/O */
				return (io_read((address >> 1) & 0x0F));
			}
			else if (address <= 0xA1FFFF) /* CONTROL */
			{
				switch ((address >> 8) & 0xFF)
				{
					case 0x11:		/* BUSACK */
						if ((address & 1) == 0) return (gen_busack_r () | (m68k_read_bus_8 (address) & 0xFE));
						else return (m68k_read_bus_8 (address));

					case 0x00:	/* UNKNOWN (BallZ) */
					case 0x10:	/* MEMORY MODE */
					case 0x12:	/* RESET */
				    case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x41:	/* BOOTROM */
					case 0x50:	/* SVP REGISTERS */
					case 0x30:	/* TIME */
						return (m68k_read_bus_8 (address));

					default:		/* Unused */
						return (m68k_lockup_r_8 (address));
				}
			}
			else
			{
				/* Unused */
		      	return (m68k_lockup_r_8 (address));
			}

			break;

		case 6: 	/* VDP */
			if ((address & 0xE700E0) == 0xC00000)
			{
				switch (address & 0x1F)
				{
					case 0x00:		/* DATA */
					case 0x02:
						return (vdp_data_r () >> 8);

					case 0x01:		/* DATA */
					case 0x03:
						return (vdp_data_r () & 0xFF);

					case 0x04:		/* CTRL */
					case 0x06:
						return ((m68k_read_bus_8 (address) & 0xFC) | (vdp_ctrl_r () >> 8));

					case 0x05:		/* CTRL */
					case 0x07:
						return (vdp_ctrl_r () & 0xFF);

					case 0x08:		/* HVC */
					case 0x0A:
					case 0x0C:
					case 0x0E:
						return (vdp_hvc_r () >> 8);

					case 0x09:		/* HVC */
					case 0x0B:
					case 0x0D:
					case 0x0F:
						return (vdp_hvc_r () & 0xFF);

					case 0x10:		/* PSG */
					case 0x11:
					case 0x12:
					case 0x13:
					case 0x14:
					case 0x15:
					case 0x16:
					case 0x17:
						return (m68k_lockup_r_8 (address));

					case 0x18:		/* Unused */
					case 0x19:
					case 0x1A:
					case 0x1B:
					case 0x1C:
					case 0x1D:
					case 0x1E:
					case 0x1F:
						return (m68k_read_bus_8 (address));
				}
			}
			else
			{
				/* Unused */
				return (m68k_lockup_r_8 (address));
			}
			break;

		case 2: /* Unused */
			/* Some unlicensed games have a simple protection device mapped here */
			return prot_bytes[(address>>2)&1];

		case 3: /* Unused */
			return (m68k_read_bus_8 (address));

		case 4:	 	/* Unused */
			return (m68k_lockup_r_8 (address));
	}

	return -1;
}


unsigned int m68k_read_memory_16 (unsigned int address)
{
	switch ((address >> 21) & 7)
    {
		case 0:		/* ROM Cartridge */
		case 1:
			/* SRAM */
      		if (sram.on)
			{
				if (address >= sram.start && address <= sram.end)
				{
	   				if (sram.custom) return (EEPROM_Read(address) & 0xffff);
					return *(uint16 *)(sram.sram + ((address - sram.start) & 0xffff));
				}
			}

			/* ROM Data */
			if (address < genromsize) return *(uint16 *)(cart_rom + address);
			
			/* j-CART */
			if (j_cart && ((address == 0x3FFFFE) || (address == 0x38FFFE)))
				return (gamepad_read(5) | (gamepad_read(6) << 8));	
			
			/* Virtua Racing SVP */
			if (address == 0x30fe02) return 0x01;
			
			/* default */
			return 0x00;	

		case 7:		/* RAM */
      		return *(uint16 *)(work_ram + (address & 0xffff));

		case 5:		/* Z80 & I/O */
			if (address <= 0xA0FFFF) /* Z80 area */
			{
				if (zbusack == 1) return (m68k_read_bus_16 (address));
				else
				{
					uint8 temp;
					switch (address & 0x6000)
					{
						case 0x0000:	/* RAM */
						case 0x2000:
							temp = zram[address & 0x1FFF];
							return (temp << 8 | temp);

						case 0x4000:	/* YM2612 */
							temp = fm_read (address & 3);
							return (temp << 8 | temp);

						case 0x6000:
							switch (address & 0xFF00)
							{
								case 0x7F00:	/* VDP */
									m68k_lockup_r_16 (address);

								default:	/* Unused */
									return (0xFFFF);
							}
							break;
					}
				}
			}
      		else if (address <= 0xA1001F)	/* I/O */
				{
					uint8 temp = io_read ((address >> 1) & 0x0F);
					return (temp << 8 | temp);
				}
			else if (address <= 0xA1FFFF)	/* CONTROL */
			{
				switch ((address >> 8) & 0xFF)
				{
					case 0x11:	/* BUSACK */
						return ((m68k_read_bus_16 (address) & 0xFEFF) | (gen_busack_r () << 8));

					case 0x00:	/* UNKNOWN (BallZ) */
					case 0x10:	/* MEMORY MODE */
					case 0x12:	/* RESET */
					case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x41:	/* BOOTROM */
					case 0x50:	/* SVP REGISTERS */
					case 0x30:	/* TIME */
						return (m68k_read_bus_16 (address));

					default:	/* Unused */
						return (m68k_lockup_r_16 (address));
				}
			}
			else
			{
				/* Unused */
				return (m68k_lockup_r_16 (address));
			}

			break;

		case 6: 	/* VDP */
			if ((address & 0xE700E0) == 0xC00000)
			{
				switch (address & 0x1F)
				{
					case 0x00:		/* DATA */
					case 0x02:
						return (vdp_data_r ());

					case 0x04:		/* CTRL */
					case 0x06:
						return (vdp_ctrl_r () | (m68k_read_bus_16 (address) & 0xFC00));

					case 0x08:		/* HVC */
					case 0x0A:
					case 0x0C:
					case 0x0E:
						return (vdp_hvc_r ());

					case 0x10:		/* PSG */
					case 0x12:
					case 0x14:
					case 0x16:
						return (m68k_lockup_r_16 (address));

					case 0x18:		/* Unused */
					case 0x1A:
					case 0x1C:
					case 0x1E:
						return (m68k_read_bus_16 (address));
				}
			}
			else
			{
				return (m68k_lockup_r_16 (address));
			}
			break;

		case 2:
		case 3:
			return (m68k_read_bus_16 (address));

		case 4:
			return (m68k_lockup_r_16 (address));
	}

	return (0xA5A5);
}


unsigned int m68k_read_memory_32 (unsigned int address)
{
	/* Split into 2 reads */
	return (m68k_read_memory_16 (address + 0) << 16 |
			m68k_read_memory_16 (address + 2));
}


void m68k_write_memory_8 (unsigned int address, unsigned int value)
{
	switch ((address >> 21) & 7)
	{
		case 7:		/* RAM */
      		WRITE_BYTE(work_ram, address & 0xFFFF, value);
			return;

		case 6:		/* VDP */
			if ((address & 0xE700E0) == 0xC00000)
			{
				switch (address & 0x1F)
				{
					case 0x00:		/* DATA */
					case 0x01:
					case 0x02:
					case 0x03:
						vdp_data_w (value << 8 | value);
						return;

					case 0x04:		/* CTRL */
					case 0x05:
					case 0x06:
					case 0x07:
						vdp_ctrl_w (value << 8 | value);
						return;

					case 0x08:		/* HVC */
					case 0x09:
					case 0x0A:
					case 0x0B:
					case 0x0C:
					case 0x0D:
					case 0x0E:
					case 0x0F:
						m68k_lockup_w_8 (address, value);
						return;

					case 0x10:		/* PSG */
					case 0x12:
					case 0x14:
					case 0x16:
						m68k_unused_8_w (address, value);
						return;

					case 0x11:		/* PSG */
					case 0x13:
					case 0x15:
					case 0x17:
						psg_write (value);
						return;

					case 0x18:		/* Unused */
					case 0x19:
					case 0x1A:
					case 0x1B:
					case 0x1C:
					case 0x1D:
					case 0x1E:
					case 0x1F:
						m68k_unused_8_w (address, value);
						return;
				}
			}
			else
			{
				m68k_lockup_w_8 (address, value);
				return;
			}

		case 5: /* Z80 & IO */
			if (address <= 0xA0FFFF) /* Z80 area */
			{
				if (zbusack == 1)
				{
					m68k_unused_8_w (address, value);
					return;
				}
				else
				{
					switch (address & 0x6000)
					{
						case 0x0000:
						case 0x2000:
							zram[(address & 0x1FFF)] = value;
							return;

						case 0x4000:
							fm_write (address & 3, value);
							return;

						case 0x6000:
							switch (address & 0xFF00)
							{
								case 0x6000:	/* BANK */
									gen_bank_w (value & 1);
									return;

								case 0x7F00:	/* VDP */
									m68k_lockup_w_8 (address, value);
									return;

								default:	/* Unused */
									m68k_unused_8_w (address, value);
									return;
							}
							break;
					}
				}
			}
			else if (address <= 0xA1001F) /* I/O */
				{
					/* I/O chip only gets /LWR */
					if (address & 1) io_write ((address >> 1) & 0x0F, value);
					return;
				}
			else if (address <= 0xA1FFFF) /* CONTROL */
			{  			
				switch ((address >> 8) & 0xFF)
				{
					case 0x11:	/* BUSREQ */
						if ((address & 1) == 0) gen_busreq_w (value & 1);
						else m68k_unused_8_w (address, value);
						return;

					case 0x12:	/* RESET */
						gen_reset_w (value & 1);
						return;

					case 0x30:	/* TIME */
					 	if (SSF2TNC) ssf2bankrom (address, value & 0xf); /* banked ROM */
						else if (address == 0xA130F1) /* banked SRAM */
						{
							sram.on = value & 1;
							sram.write = (value & 2) ? 0 : 1;
						}
						else m68k_unused_8_w (address, value);
						return;

					case 0x10:	/* MEMORY MODE */
					case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x41:	/* BOOTROM */
					case 0x50:  /* SVP REGISTERS */
						m68k_unused_8_w (address, value);
					  	return;

					default:
							m68k_lockup_w_8 (address, value);
							return;
				}
			}
			else
			{
				/* Unused */
				m68k_lockup_w_8 (address, value);
				return;
			}
			break;

		case 0:		/* ROM Cartridge */
		case 1:
	  
			/* external SRAM */
			if (sram.on && sram.write)
			{
				if (address >= sram.start && address <= sram.end)
				{
					/* serial EEPROM */
					if (sram.custom)
					{
						EEPROM_Write(address, value);
						return;
					}
					
					/* normal SRAM */
					WRITE_BYTE(sram.sram, (address - sram.start) & 0xffff, value & 0xff);
					return;
				}
			}
			m68k_unused_8_w (address, value);
			return;

		case 2:		/* Unused */
			/* Some unlicensed games have a simple protection device mapped here */
			prot_bytes[(address>>2)&1] = value;
			return;
		case 3:
			m68k_unused_8_w (address, value);
			return;

		case 4:		/* Unused */
			m68k_lockup_w_8 (address, value);
			return;
	}
}


void m68k_write_memory_16 (unsigned int address, unsigned int value)
{
	switch ((address >> 21) & 7)
	{
		case 7:	 /* Work RAM */
      		*(uint16 *)(work_ram + (address& 0xFFFF)) = value & 0xffff;
		  	return;

		case 6:		/* VDP */
			if ((address & 0xE700E0) == 0xC00000)
			{
				switch (address & 0x1C)
				{
					case 0x00:		/* DATA */
						vdp_data_w (value);
						return;

					case 0x04:		/* CTRL */
						vdp_ctrl_w (value);
						return;

					case 0x08:		/* HV counter */
					case 0x0C:		/* HV counter */
						m68k_lockup_w_16 (address, value);
						return;

					case 0x10:		/* PSG */
					case 0x14:		/* PSG */
						psg_write (value & 0xFF);
						return;

					case 0x18:		/* Unused */
					case 0x1C:		/* Unused */
						m68k_unused_8_w (address, value);
						return;
				}
			}
			else
			{
				/* Invalid address */
				m68k_lockup_w_16 (address, value);
			}
			break;

		case 5:		/* Z80 area, I/O chip, miscellaneous. */
			if (address <= 0xA0FFFF) /* Z80 area */
			{
				/* Writes are ignored when the Z80 hogs the Z-bus */
				if (zbusack == 1)
				{
					m68k_unused_8_w (address, value);
					return;
				}

				/* Write into Z80 address space */
				switch (address & 0x6000)
				{
					case 0x0000:	/* Work RAM */
					case 0x2000:	/* Work RAM */
						zram[(address & 0x1FFF)] = (value >> 8) & 0xFF;
						return;

					case 0x4000:	/* YM2612 */
						fm_write (address & 3, (value >> 8) & 0xFF);
						return;

					case 0x6000:	/* Bank register and VDP */
						switch (address & 0x7F00)
						{
							case 0x6000:	/* Bank register */
								gen_bank_w ((value >> 8) & 1);
								return;

							case 0x7F00:	/* VDP registers */
								m68k_lockup_w_16 (address, value);
								return;

							default:	/* Unused */
								m68k_unused_8_w (address, value);
								return;
						}
						break;
				}
			}
			else if (address <= 0xA1001F) /* I/O */
				{
					io_write ((address >> 1) & 0x0F, value & 0x00FF);
					return;
				}
			else if (address <= 0xA1FFFF) /* CONTROL */
	    	{				
				switch ((address >> 8) & 0xFF)
				{
					case 0x11:	/* BUSREQ */
						gen_busreq_w ((value >> 8) & 1);
						return;

					case 0x12:	/* RESET */
						gen_reset_w ((value >> 8) & 1);
						return;

					case 0x30:	/* TIME */
						if (SSF2TNC) ssf2bankrom (address, value & 0xf); /* banked ROM */
						else if (address == 0xA130F1) /* banked SRAM */
						{
							sram.on = value & 1;
							sram.write = (value & 2) ? 0 : 1;
						}
						else m68k_unused_16_w (address, value);
						return;

					case 0x10:	/* MEMORY MODE */
					case 0x20:	/* MEGA-CD */
					case 0x40:	/* TMSS */
					case 0x41:	/* BOOTROM */
					case 0x50:  /* SVP REGISTERS */
					  	m68k_unused_16_w (address, value);
					  	return;
				
					default:	/* Unused */
						m68k_lockup_w_16 (address, value);
						return;
				}
			}
			else
			{
				m68k_lockup_w_16 (address, value);
				return;
			}

			break;

		case 0:		/* Cartridge ROM */
		case 1:
			/* J-CART extension (TH signal) */
			if ((address == 0x3FFFFE) || (address == 0x38FFFE)) 
			{
				if (!j_cart)
				{
					j_cart = 1;
					input_reset(pad_type);
				}
				gamepad_write(5, (value&1) <<6 );
				gamepad_write(6, (value&1) <<6);
				return;
			}

			/* external SRAM */
			if (sram.on && sram.write) 
			{
				if (address >= sram.start && address <= sram.end)
				{
					/* serial EEPROM */
					if (sram.custom)
					{
						EEPROM_Write(address, value);
						return;
					}
					
					/* normal SRAM */
					*(uint16 *)(sram.sram + ((address - sram.start) & 0xffff)) = value & 0xffff;
					return;
				}
			}
			m68k_unused_16_w (address, value);
			return;

		case 2:		/* Unused */
		case 3:
			m68k_unused_16_w (address, value);
			return;

		case 4:		/* Unused */
			m68k_lockup_w_16 (address, value);
			return;
	}
}


void m68k_write_memory_32 (unsigned int address, unsigned int value)
{
	/* Split into 2 writes */
	m68k_write_memory_16 (address, (value >> 16) & 0xFFFF);
	m68k_write_memory_16 (address + 2, value & 0xFFFF);
}
