/****************************************************************************
 *  Genesis Plus 1.2a
 *  Cartridge Hardware support
 *
 *  code by Eke-Eke, GC/Wii port
 *
 *  Lots of protection mechanism have been discovered by Haze
 *  (http://haze.mameworld.info/)
 *
 *  Realtec mapper has been figured out by TascoDeluxe
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
 ***************************************************************************/

#include "shared.h"
#include "m68kcpu.h"

#define CART_CNT 26

/* Function prototypes */
void default_time_w(unsigned int address, unsigned int value);
void special_mapper_w(unsigned int address, unsigned int value);
void realtec_mapper_w(unsigned int address, unsigned int value);
void seganet_mapper_w(unsigned int address, unsigned int value);
unsigned int radica_mapper_r(unsigned int address);
void default_regs_w(unsigned int address, unsigned int value);
unsigned int default_regs_r(unsigned int address);
void special_regs_w(unsigned int address, unsigned int value);

/* Cart database entry */
typedef struct
{
	uint16 chk_1;		/* header checksum */
	uint16 chk_2;		/* real checksum */
	uint8 bank_start;	/* first mapped bank in $400000-$7fffff region */
	uint8 bank_end;		/* last mapped bank in $400000-$7fffff region */
	T_CART_HW cart_hw;	/* hardware description */
} T_CART_ENTRY;

/* Games that need extra hardware emulation:
	- copy protection device
	- custom ROM banking device
*/
T_CART_ENTRY rom_database[CART_CNT] =
{
/* Game no Kanzume Otokuyou */
	{0x0000,0xf9d1,0,0,{{0,0,0,0},{0,0,0,0},{0,0,0,0},0,0,seganet_mapper_w,0,0}},
/* RADICA (Volume 1) (not byteswapped) */
	{0x0000,0x2326,0,0,{{0,0,0,0},{0,0,0,0},{0,0,0,0},0,radica_mapper_r,0,0,0}},
/* RADICA (Volume 2) */
	{0x4f10,0x0836,0,0,{{0,0,0,0},{0,0,0,0},{0,0,0,0},0,radica_mapper_r,0,0,0}},
/* RADICA (Volume 1) */
	{0xf424,0x9f82,0,0,{{0,0,0,0},{0,0,0,0},{0,0,0,0},0,radica_mapper_r,0,0,0}},
/* Funny World & Balloon Boy */
	{0x0000,0x06ab,8,8,{{0,0,0,0},{0,0,0,0},{0,0,0,0},1,0,0,0,realtec_mapper_w}},
/* Whac-a-Critter */
	{0xffff,0xf863,8,8,{{0,0,0,0},{0,0,0,0},{0,0,0,0},1,0,0,0,realtec_mapper_w}},
/* Earth Defense */
	{0xffff,0x44fb,8,8,{{0,0,0,0},{0,0,0,0},{0,0,0,0},1,0,0,0,realtec_mapper_w}},
/* Super Mario 2 1998 */
	{0xffff,0x0474,0,0,{{0x0a,0,0,0},{0xffffff,0,0,0},{0xa13000,0,0,0},0,default_regs_r,0,0,0}},
/* Super Mario 2 1998 */
	{0x2020,0xb4eb,0,0,{{0x1c,0,0,0},{0xffffff,0,0,0},{0xa13000,0,0,0},0,default_regs_r,0,0,0}},
/* Supper Bubble Bobble */
	{0x0000,0x16cd,8,8,{{0x55,0x0f,0,0},{0xffffff,0xffffff,0,0},{0x400000,0x400002,0,0},0,0,0,default_regs_r,0}},
/* Mahjong Lover */
	{0x0000,0x7037,8,8,{{0x90,0xd3,0,0},{0xffffff,0xffffff,0,0},{0x400000,0x401000,0,0},0,0,0,default_regs_r,0}},
/* Lion King 2 */
	{0xffff,0x1d9b,8,8,{{0,0,0,0},{0xfffffd,0xfffffd,0,0},{0x400000,0x400004,0,0},0,0,0,default_regs_r,default_regs_w}},
/* Squirell King */
	{0x0000,0x8ec8,8,8,{{0,0,0,0},{0xfffffd,0xfffffd,0,0},{0x400000,0x400004,0,0},0,0,0,default_regs_r,default_regs_w}},
/* Rockman X3 */
	{0x0000,0x9d0e,8,8,{{0x0c,0x88,0,0},{0xffffff,0xffffff,0,0},{0xa13000,0x400004,0,0},0,default_regs_r,0,default_regs_r,0}},
/* A Bug's Life */
	{0x7f7f,0x2aad,0,0,{{0x28,0x1f,0x01,0},{0xffffff,0xffffff,0xffffff,0},{0xa13000,0xa13002,0xa1303e,0},0,default_regs_r,0,0,0}},
/* King of Fighter 99 */
	{0x0000,0x21e,0,0,{{0x00,0x01,0x1f,0},{0xffffff,0xffffff,0xffffff,0},{0xa13000,0xa13002,0xa1303e,0},0,default_regs_r,0,0,0}},
/* Pocket Monster */
	{0xd6fc,0x1eb1,0,0,{{0x00,0x01,0x1f,0},{0xffffff,0xffffff,0xffffff,0},{0xa13000,0xa13002,0xa1303e,0},0,default_regs_r,0,0,0}},
/* Lion King 3 */
	{0x0000,0x507c,12,15,{{0,0,0,0},{0xf0000e,0xf0000e,0xf0000e,0},{0x600000,0x600002,0x600004,0},0,0,0,default_regs_r,special_regs_w}},
/* Super King Kong 99 */
	{0x0000,0x7d6e,12,15,{{0,0,0,0},{0xf0000e,0xf0000e,0xf0000e,0},{0x600000,0x600002,0x600004,0},0,0,0,default_regs_r,special_regs_w}},
/* Pokemon Stadium */
	{0x0000,0x843c,14,15,{{0,0,0,0},{0,0,0,0},{0,0,0,0},0,0,0,0,special_regs_w}},
/* Elf Wor */
	{0x0080,0x3dba,8,8,{{0x55,0x0f,0xc9,0x18},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,0,default_regs_r,0}},
/* Huan Le Tao Qi Shu - Smart Mouse */
	{0x0000,0x1a28,8,8,{{0x55,0x0f,0xaa,0xf0},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,0,default_regs_r,0}},
/* Ya-Se Chuanshuo */
	{0xffff,0xd472,8,8,{{0x63,0x98,0xc9,0x18},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,0,default_regs_r,0}},
/* Soul Blade */
	{0x0000,0x0c5b,8,8,{{0x00,0x98,0xc9,0xF0},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,0,default_regs_r,0}},
/* King of Fighter 98 */
	{0x0000,0xd0a0,9,9,{{0xaa,0xa0,0xf0,0xa0},{0xfc0000,0xffffff,0xffffff,0xffffff},{0x480000,0x4c82c0,0x4cdda0,0x4f8820},0,0,0,default_regs_r,0}},
/* Lian Huan Pao - Barver Battle Saga */
	{0x30b9,0x1c2a,8,8,{{0,0,0,0},{0,0,0,0},{0,0,0,0},0,0,0,default_regs_r,0}}
};


/* current cart hardware */
T_CART_HW cart_hw;
uint8 j_cart;

static int old_system[2] = {-1,-1};

/************************************************************
					Cart Hardware initialization 
*************************************************************/

/* hardware that need to be reseted on power on */
void cart_hw_reset()
{
	/* Realtec mapper */
	if (cart_hw.realtec & 1)
	{
		int i;

		/* enable BOOTROM */
		for (i=0; i<8; i++) m68k_readmap_16[i]	= REALTEC_ROM;
		cart_hw.realtec |= 2;
	}
	
	/* SVP chip */
	if (svp) svp_reset();
}

/* cart hardware detection */
void cart_hw_init()
{
	int i;
	
	/**********************************************
					DEFAULT CART MAPPING 
	***********************************************/
	for (i=0; i<8; i++)
	{
		m68k_readmap_8[i]	= ROM;
		m68k_readmap_16[i]	= ROM;
		m68k_writemap_8[i]	= UNUSED;
		m68k_writemap_16[i]	= UNUSED;
	}

	for (i=8; i<16; i++)
	{
		m68k_readmap_8[i]	= UNUSED;
		m68k_readmap_16[i]	= UNUSED;
		m68k_writemap_8[i]	= UNUSED;
		m68k_writemap_16[i]	= UNUSED;
	}
	
  /* restore previous setting */
  if (old_system[0] != -1)  input.system[0] = old_system[0];
  if (old_system[1] != -1)  input.system[1] = old_system[1];

	/**********************************************
					EXTERNAL RAM 
	***********************************************/
	sram_init();
	eeprom_init();
	if (sram.on)
	{
		if (sram.custom)
		{
			/* serial EEPROM */
      m68k_readmap_8[eeprom.type.sda_out_adr >> 19]		= EEPROM;
			m68k_readmap_16[eeprom.type.sda_out_adr >> 19]  = EEPROM;
			m68k_writemap_8[eeprom.type.sda_in_adr >> 19]		= EEPROM;
			m68k_writemap_16[eeprom.type.sda_in_adr >> 19]	= EEPROM;
			m68k_writemap_8[eeprom.type.scl_adr >> 19]      = EEPROM;
			m68k_writemap_16[eeprom.type.scl_adr >> 19]	    = EEPROM;
		}
		else
		{
			/* 64KB SRAM */
      m68k_readmap_8[sram.start >> 19]  = SRAM;
			m68k_readmap_16[sram.start >> 19] = SRAM;
      if (sram.write)
      {
        m68k_writemap_8[sram.start >> 19]   = SRAM;
			  m68k_writemap_16[sram.start >> 19]  = SRAM;
		  }
		}
	}
	/**********************************************
					SVP CHIP 
	***********************************************/
	svp = NULL;
	if (strstr(rominfo.international,"Virtua Racing") != NULL)
	{
		svp_init();
		m68k_readmap_16[6]	= SVP_DRAM;
		m68k_writemap_16[6]	= SVP_DRAM;
		m68k_readmap_16[7]	= SVP_CELL;
		m68k_writemap_16[7]	= SVP_CELL;
	}

	/**********************************************
					SEGA MENACER 
	***********************************************/
  input.x_offset = 0x00;
  input.y_offset = 0x00;

  if (strstr(rominfo.international,"MENACER") != NULL)
	{
		/* save current setting */
    if (old_system[0] == -1) old_system[0] = input.system[0];
    if (old_system[1] == -1) old_system[1] = input.system[1];
     
    input.system[0] = NO_SYSTEM;
    input.system[1] = SYSTEM_MENACER;
    input.x_offset = 0x52;
    input.y_offset = 0x00;
	}
	else if (strstr(rominfo.international,"T2 ; THE ARCADE GAME") != NULL)
  {
    input.system[0] = SYSTEM_GAMEPAD;
    input.system[1] = SYSTEM_MENACER;
    input.x_offset = 0x84;
    input.y_offset = 0x08;
	}
	else if (strstr(rominfo.international,"BODY COUNT") != NULL)
  {
    input.system[0] = SYSTEM_MOUSE;
    input.system[1] = SYSTEM_MENACER;
    input.x_offset = 0x44;
    input.y_offset = 0x18;
	}

	/**********************************************
					KONAMI JUSTIFIER 
	***********************************************/
	if (strstr(rominfo.international,"LETHAL ENFORCERS II") != NULL)
  {
    input.system[0] = SYSTEM_GAMEPAD;
    input.system[1] = SYSTEM_JUSTIFIER;
  input.x_offset = 0x18;
    input.y_offset = 0x00;
	}
	else if (strstr(rominfo.international,"LETHAL ENFORCERS") != NULL)
  {
    input.system[0] = SYSTEM_GAMEPAD;
    input.system[1] = SYSTEM_JUSTIFIER;
    input.x_offset = 0x00;
    input.y_offset = 0x00;
  }

	/**********************************************
					J-CART 
	***********************************************/
	j_cart = 0;
  if (((strstr(rominfo.product,"00000000") != NULL) && (rominfo.checksum == 0x168b))  ||	/* Super Skidmarks, Micro Machines Military*/
		((strstr(rominfo.product,"00000000") != NULL) && (rominfo.checksum == 0x165e))    ||	/* Pete Sampras Tennis (1991), Micro Machines 96 */
		((strstr(rominfo.product,"00000000") != NULL) && (rominfo.checksum == 0xcee0))    ||	/* Micro Machines Military (bad) */
		((strstr(rominfo.product,"00000000") != NULL) && (rominfo.checksum == 0x2c41))    ||	/* Micro Machines 96 (bad) */
		((strstr(rominfo.product,"XXXXXXXX") != NULL) && (rominfo.checksum == 0xdf39))    ||	/* Sampras Tennis 96 */
		((strstr(rominfo.product,"T-123456") != NULL) && (rominfo.checksum == 0x1eae))    ||	/* Sampras Tennis 96 */
		((strstr(rominfo.product,"T-120066") != NULL) && (rominfo.checksum == 0x16a4))    ||	/* Pete Sampras Tennis (1994)*/
		 (strstr(rominfo.product,"T-120096") != NULL))										                    /*  Micro Machines 2 */
	{
		if (genromsize <= 0x380000)	/* just to be sure (checksum might not be enough) */
		{
			j_cart = 1;
			m68k_readmap_16[7]	= J_CART;
			m68k_writemap_16[7]	= J_CART;

      /* save current setting */
      if (old_system[0] == -1) old_system[0] = input.system[0];
      if (old_system[1] == -1) old_system[1] = input.system[1];
       
      /* PORT B by default */
      input.system[0] = SYSTEM_GAMEPAD;
      input.system[1] = SYSTEM_GAMEPAD;
		}
	}

	/**********************************************
					ULTIMATE MK3 HACK
	***********************************************/
  if (genromsize > 0x600000)
  {
    for (i=8; i<20; i++)
    {
      m68k_readmap_8[i]	  = UMK3_HACK;
      m68k_readmap_16[i]	= UMK3_HACK;
    }

#if M68K_EMULATE_ADDRESS_ERROR
    /* this game does not work properly on real hardware */
    emulate_address_error = 0;  
#endif
  }
#if M68K_EMULATE_ADDRESS_ERROR
  /* default behavior */
  else emulate_address_error = 1; 
#endif

	/**********************************************
				Mappers & HW registers 
	***********************************************/
	memset(&cart_hw, 0, sizeof(cart_hw));

	/* default write handler for !TIME signal */
	cart_hw.time_w = default_time_w;

	/* search for game into database */
	for (i=0; i < CART_CNT + 1; i++)
	{
		/* known cart found ! */
		if ((rominfo.checksum == rom_database[i].chk_1) &&
			(realchecksum == rom_database[i].chk_2))
		{
			/* retrieve hardware information */
			memcpy(&cart_hw, &(rom_database[i].cart_hw), sizeof(cart_hw));

			/* initialize memory handlers for $400000-$7fffff region */
			int j = rom_database[i].bank_start;
			while (j <= rom_database[i].bank_end)
			{
				if (cart_hw.regs_r) m68k_readmap_8[j]  = CART_HW;
				if (cart_hw.regs_w) m68k_writemap_8[j] = CART_HW;
				j++;
			}
			i = CART_CNT + 1;
		}
	}
}

/************************************************************
					MAPPER handlers 
*************************************************************/
/* 
	"official" ROM/RAM switch
*/
static inline void sega_mapper_w(unsigned int address, unsigned int value)
{
	uint8 bank = (address >> 1) & 7;
	
	switch (bank)
	{
		case 0:
		{
			/* ROM/RAM switch (Phantasy Star IV, Story of Thor/Beyond Oasis, Sonic 3 & Knuckles) */
			if (value & 1)
			{
				/* SRAM is mapped above 200000h */
				m68k_readmap_8[4]	= SRAM;
				m68k_readmap_16[4]	= SRAM;

				if (value & 2)
				{
					/* SRAM write protected */
					m68k_writemap_8[4]	= UNUSED;
					m68k_writemap_16[4]	= UNUSED;
				}
				else
				{
					/* SRAM write enabled */
					m68k_writemap_8[4]	= SRAM;
					m68k_writemap_16[4]	= SRAM;
				}
			}
			else
			{
				/* ROM is mapped above 200000h */
				m68k_readmap_8[4]	= ROM;
				m68k_readmap_16[4]	= ROM;
				m68k_writemap_8[4]	= UNUSED;
				m68k_writemap_16[4]	= UNUSED;
			}
			break;
		}
		
		default:
			/* ROM Bankswitch (Super Street Fighter 2)
			   documented by Bart Trzynadlowski (http://www.trzy.org/files/ssf2.txt) 
			*/
			rom_readmap[bank] = &cart_rom[value << 19];
			break;
	}
}

/* 
	custom ROM Bankswitch used by pirate "Multi-in-1" carts
	(documented by Haze)
*/
static inline void multi_mapper_w(unsigned int address, unsigned int value)
{
	int i;
	uint32 bank_addr = (address & 0x3F) << 16;

	/* those games are generally not bigger than 1MB but it's safer to update all 512K banks */
	for (i=0; i<8; i++)
	{
		if (bank_addr >= genromsize) return;
		rom_readmap[i] = &cart_rom[bank_addr];
		bank_addr += 0x80000;
	}
}

/* 
	Special ROM Bankswitch used for copy protection
	Used by unlicensed cartridges (Lion King III, Super King Kong 99)
	(documented by Haze)
*/
void special_mapper_w(unsigned int address, unsigned int value)
{
	rom_readmap[0] = &cart_rom[value << 15];
}

/* 
	Realtec ROM Bankswitch (Earth Defend, Balloon Boy & Funny World, Whac-A-Critter)
	(documented by TascoDeluxe)
*/
void realtec_mapper_w(unsigned int address, unsigned int value)
{
	uint32 base_addr;

	if (cart_hw.realtec & 2)
	{
		int i;
		cart_hw.realtec &= ~2;
		
		/* disable Realtec BOOTROM	*/
		for (i=0; i<8; i++)
		{
			m68k_readmap_8[i]	= ROM;
			m68k_readmap_16[i]	= ROM;
		}
	}

	switch (address)
	{
		case 0x404000:	/* three lower bits of ROM base address */
			cart_hw.regs[0] = value;
			base_addr = (value | ((cart_hw.regs[1] & 6) << 2)) << 17;
			rom_readmap[0] = &cart_rom[base_addr];
			return;

		case 0x400000:	/* two higher bits of ROM base address */
			cart_hw.regs[1] = value;
			base_addr = ((cart_hw.regs[0] & 7) | ((value & 6) << 2)) << 17;
			rom_readmap[0] = &cart_rom[base_addr];
			return;

		case 0x402000:	/* size of ROM range to map */
			cart_hw.regs[2] = value;
			return;

		default:
			return;
	}
}


/* Game no Kanzume Otokuyou ROM Mapper */
void seganet_mapper_w(unsigned int address, unsigned int value)
{
	int i;
	switch (address & 0xff)
	{
		case 0x01:
			if (value & 1)
			{
				/* ROM disabled */
				for (i=0; i<8; i++)
				{
					m68k_readmap_8[i]	= UNUSED;
					m68k_readmap_16[i]	= UNUSED;
				}
			}
			else
			{
				/* ROM enabled */
				for (i=0; i<8; i++)
				{
					m68k_readmap_8[i]	= ROM;
					m68k_readmap_16[i]	= ROM;
				}
			}
			break;;

		case 0xf1:
			if (value & 1)
			{
				/* ROM Write protected*/
				for (i=0; i<8; i++)
				{
					m68k_writemap_8[i]	= UNUSED;
					m68k_writemap_16[i]	= UNUSED;
				}
			}
			else
			{
				/* ROM Write enabled */
				for (i=0; i<8; i++)
				{
					m68k_writemap_8[i]	= ROM;
					m68k_writemap_16[i]	= ROM;
				}
			}
			break;

		default:
			break;
	}
}

/*
	RADICA ROM Bankswitch
	(documented by Haze)
*/
unsigned int radica_mapper_r(unsigned int address)
{
	/* 64KB ROM banks */
	uint8 bank = (address >> 1) & 0x3F;
	rom_readmap[0] = &cart_rom[bank * 0x10000];
	rom_readmap[1] = &cart_rom[bank * 0x10000 + 0x80000]; /* ROM is max. 1MB */
	return 0xff;
}


/************************************************************
					default !TIME signal handler 
*************************************************************/

/* default ROM bankswitch */
void default_time_w(unsigned int address, unsigned int value)
{
	if ((address & 0xf1) == 0xf1) sega_mapper_w(address, value);
	else if (address < 0xa13040) multi_mapper_w(address, value);
}

/************************************************************
					Internal register handlers
*************************************************************/

unsigned int default_regs_r(unsigned int address)
{
	uint8 i;
	for (i=0; i<4; i++)
	{
		if ((address & cart_hw.mask[i]) == cart_hw.addr[i])
			return cart_hw.regs[i];
	}

	/* unused */
	return -1;
}

void default_regs_w(unsigned int address, unsigned int value)
{
	uint8 i;
	for (i=0; i<4; i++)
	{
		if ((address & cart_hw.mask[i]) == cart_hw.addr[i])
		{
			cart_hw.regs[i] = value;
		}
	}
}

/* special register behaviour (Lion King III, Super Donkey Kong  99) */
void special_regs_w(unsigned int address, unsigned int value)
{
	/* ROM bankswitch */
	if ((address >> 16) > 0x6f)
	{
		special_mapper_w(address, value);
		return;
	}

	/* write regs */
	default_regs_w(address, value);

	/* bitswapping (documented by Haze) */
	uint8 temp = cart_hw.regs[0];
	switch (cart_hw.regs[1])
	{					
		case 1:
			cart_hw.regs[2] = (temp >> 1);
			return;

		case 2:
			cart_hw.regs[2] = ((temp >> 4) | ((temp & 0x0F) << 4));
			return;

		default:
			cart_hw.regs[2] = (((temp >> 7) & 0x01) | ((temp >> 5) & 0x02) |
				  			   ((temp >> 3) & 0x04) | ((temp >> 1) & 0x08) |
							   ((temp << 1) & 0x10) | ((temp << 3) & 0x20) |
								((temp << 5) & 0x40) | ((temp << 7) & 0x80));
			return;
	}
}
