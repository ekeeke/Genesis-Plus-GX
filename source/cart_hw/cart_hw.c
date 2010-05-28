
/****************************************************************************
 *  Genesis Plus
 *  Cartridge Hardware support
 *
 *  Copyright (C) 2007, 2008, 2009  Eke-Eke (GCN/Wii port)
 *
 *  Most cartridge protections documented by Haze
 *  (http://haze.mameworld.info/)
 *
 *  Realtec mapper documented by TascoDeluxe
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

#define CART_CNT 26

extern int emulate_address_error;

/* Global Variables */
T_CART cart;

/* Cart database entry */
typedef struct
{
  uint16 chk_1;       /* header checksum */
  uint16 chk_2;       /* real checksum */
  uint8 bank_start;   /* first mapped bank in $400000-$7fffff region */
  uint8 bank_end;     /* last mapped bank in $400000-$7fffff region */
  T_CART_HW cart_hw;  /* hardware description */
} T_CART_ENTRY;

/* Function prototypes */
static void sega_mapper_w(uint32 address, uint32 data);
static void special_mapper_w(uint32 address, uint32 data);
static void realtec_mapper_w(uint32 address, uint32 data);
static void seganet_mapper_w(uint32 address, uint32 data);
static uint32 radica_mapper_r(uint32 address);
static void default_time_w(uint32 address, uint32 data);
static void default_regs_w(uint32 address, uint32 data);
static uint32 default_regs_r(uint32 address);
static void special_regs_w(uint32 address, uint32 data);

/* Games that need extra hardware emulation:
  - copy protection device
  - custom ROM banking device
*/
static const T_CART_ENTRY rom_database[CART_CNT] =
{
/* Funny World & Balloon Boy */
  {0x0000,0x06ab,0x40,0x40,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},1,1,NULL,NULL,NULL,realtec_mapper_w}},
/* Whac-a-Critter */
  {0xffff,0xf863,0x40,0x40,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},1,1,NULL,NULL,NULL,realtec_mapper_w}},
/* Earth Defense */
  {0xffff,0x44fb,0x40,0x40,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},1,1,NULL,NULL,NULL,realtec_mapper_w}},
/* RADICA (Volume 1) (not byteswapped) */
  {0x0000,0x2326,0x00,0x00,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},0,1,radica_mapper_r,NULL,NULL,NULL}},
/* RADICA (Volume 2) */
  {0x4f10,0x0836,0x00,0x00,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},0,1,radica_mapper_r,NULL,NULL,NULL}},
/* RADICA (Volume 1) */
  {0xf424,0x9f82,0x00,0x00,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},0,1,radica_mapper_r,NULL,NULL,NULL}},
/* Lion King 3 */
  {0x0000,0x507c,0x60,0x7f,{{0x00,0x00,0x00,0x00},{0xf0000e,0xf0000e,0xf0000e,0x000000},{0x600000,0x600002,0x600004,0x000000},0,1,NULL,NULL,default_regs_r,special_regs_w}},
/* Super King Kong 99 */
  {0x0000,0x7d6e,0x60,0x7f,{{0x00,0x00,0x00,0x00},{0xf0000e,0xf0000e,0xf0000e,0x000000},{0x600000,0x600002,0x600004,0x000000},0,1,NULL,NULL,default_regs_r,special_regs_w}},
/* Pokemon Stadium */
  {0x0000,0x843c,0x70,0x7f,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},0,1,NULL,NULL,default_regs_r,special_regs_w}},
/* Lion King 2 */
  {0xffff,0x1d9b,0x40,0x40,{{0x00,0x00,0x00,0x00},{0xfffffd,0xfffffd,0x000000,0x000000},{0x400000,0x400004,0x000000,0x000000},0,0,NULL,NULL,default_regs_r,default_regs_w}},
/* Squirell King */
  {0x0000,0x8ec8,0x40,0x40,{{0x00,0x00,0x00,0x00},{0xfffffd,0xfffffd,0x000000,0x000000},{0x400000,0x400004,0x000000,0x000000},0,0,NULL,NULL,default_regs_r,default_regs_w}},
/* Supper Bubble Bobble */
  {0x0000,0x16cd,0x40,0x40,{{0x55,0x0f,0x00,0x00},{0xffffff,0xffffff,0x000000,0x000000},{0x400000,0x400002,0x000000,0x000000},0,0,NULL,NULL,default_regs_r,NULL}},
/* Mahjong Lover */
  {0x0000,0x7037,0x40,0x40,{{0x90,0xd3,0x00,0x00},{0xffffff,0xffffff,0x000000,0x000000},{0x400000,0x401000,0x000000,0x000000},0,0,NULL,NULL,default_regs_r,NULL}},
/* Elf Wor */
  {0x0080,0x3dba,0x40,0x40,{{0x55,0x0f,0xc9,0x18},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,NULL,NULL,default_regs_r,NULL}},
/* Huan Le Tao Qi Shu - Smart Mouse */
  {0x0000,0x1a28,0x40,0x40,{{0x55,0x0f,0xaa,0xf0},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,NULL,NULL,default_regs_r,NULL}},
/* Ya-Se Chuanshuo */
  {0xffff,0xd472,0x40,0x40,{{0x63,0x98,0xc9,0x18},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,NULL,NULL,default_regs_r,NULL}},
/* Soul Blade */
  {0x0000,0x0c5b,0x40,0x40,{{0x00,0x98,0xc9,0xF0},{0xffffff,0xffffff,0xffffff,0xffffff},{0x400000,0x400002,0x400004,0x400006},0,0,NULL,NULL,default_regs_r,NULL}},
/* King of Fighter 98 */
  {0x0000,0xd0a0,0x48,0x4f,{{0xaa,0xa0,0xf0,0xa0},{0xfc0000,0xffffff,0xffffff,0xffffff},{0x480000,0x4c82c0,0x4cdda0,0x4f8820},0,0,NULL,NULL,default_regs_r,NULL}},
/* Lian Huan Pao - Barver Battle Saga */
  {0x30b9,0x1c2a,0x40,0x40,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},0,0,NULL,NULL,default_regs_r,NULL}},
/* Rockman X3 */
  {0x0000,0x9d0e,0x40,0x40,{{0x0c,0x88,0x00,0x00},{0xffffff,0xffffff,0x000000,0x000000},{0xa13000,0x400004,0x000000,0x000000},0,0,default_regs_r,NULL,default_regs_r,NULL}},
/* Super Mario 2 1998 */
  {0xffff,0x0474,0x00,0x00,{{0x0a,0x00,0x00,0x00},{0xffffff,0x000000,0x000000,0x000000},{0xa13000,0x000000,0x000000,0x000000},0,0,default_regs_r,NULL,NULL,NULL}},
/* Super Mario 2 1998 */
  {0x2020,0xb4eb,0x00,0x00,{{0x1c,0x00,0x00,0x00},{0xffffff,0x000000,0x000000,0x000000},{0xa13000,0x000000,0x000000,0x000000},0,0,default_regs_r,NULL,NULL,NULL}},
/* A Bug's Life */
  {0x7f7f,0x2aad,0x00,0x00,{{0x28,0x1f,0x01,0x00},{0xffffff,0xffffff,0xffffff,0x000000},{0xa13000,0xa13002,0xa1303e,0x000000},0,0,default_regs_r,NULL,NULL,NULL}},
/* King of Fighter 99 */
  {0x0000,0x021e,0x00,0x00,{{0x00,0x01,0x1f,0x00},{0xffffff,0xffffff,0xffffff,0x000000},{0xa13000,0xa13002,0xa1303e,0x000000},0,0,default_regs_r,NULL,NULL,NULL}},
/* Pocket Monster */
  {0xd6fc,0x1eb1,0x00,0x00,{{0x00,0x01,0x1f,0x00},{0xffffff,0xffffff,0xffffff,0x000000},{0xa13000,0xa13002,0xa1303e,0x000000},0,0,default_regs_r,NULL,NULL,NULL}},
/* Game no Kanzume Otokuyou */
  {0x0000,0xf9d1,0x00,0x00,{{0x00,0x00,0x00,0x00},{0x000000,0x000000,0x000000,0x000000},{0x000000,0x000000,0x000000,0x000000},0,0,NULL,seganet_mapper_w,NULL,NULL}}
};


/************************************************************
          Cart Hardware initialization 
*************************************************************/

/* cart hardware detection */
void cart_hw_init()
{
  int i;

  /***************************************************************************************************************
                CARTRIDGE ROM MIRRORING                                                                                   
   ***************************************************************************************************************
  
    Cartridge area is mapped to $000000-$3fffff:

      -> when accessing ROM, 68k address lines A1 to A21 are used by the internal cartridge hardware to decode the
         full 4MB address range.
      -> depending on the ROM total size, some address lines might be ignored, resulting in ROM mirroring.


    Cartridges can use either 8-bits (x2) or 16-bits (x1, x2) Mask ROM chips, each chip size is a factor of 2 bytes:

      -> two 8-bits chips are equivalent to one 16-bits chip, no specific address decoding is required, needed
         address lines are simply connected to each chip, upper address lines are ignored and data lines are
         connected appropriately to each chip (D0-D7 to one chip, D8-D15 to the other one).
         ROM is mirrored each N bytes where N=2^(k+1) is the total ROM size (ROM1+ROM2,ROM1+ROM2,...).

      -> one single 16-bits chip do not need specific address decoding, address lines are simply connected
         depending on the ROM size, upper address lines being ignored.
         ROM is mirrored each N bytes where N=2^k is the size of the ROM chip (ROM1,ROM1,ROM1,...).

      -> two 16-bits chips of the same size are equivalent to one chip of double size, address decoding generally
         is the same except that specific hardware is used (one address line is generally used for chip selection,
         lower ones being used to address the chips and upper ones being ignored).
         ROM is mirrored continuously each N bytes where N=2^(k+1) is the total ROM size (ROM1,ROM2,ROM1,ROM2,...).

      -> two 16-bits chips with different size are mapped differently. Address decoding is done the same way as
         above (one address line used for chip selection) but the ignored & required address lines differ from
         one chip to another, which makes ROM mirroring different.
         ROM2 size is generally half of ROM1 size and ROM are mirrored like that : ROM1,ROM2,ROM2,ROM1,ROM2,ROM2,...

      From the emulator point of view, we only need to distinguish 3 cases:

      1/ total ROM size is a factor of 2: ROM is mirrored each 2^k bytes.

      2/ total ROM size is not a factor of 2 and cartridge uses one or two chips of the same size (Type A):
         ROM is padded up to 2^k and mirrored each 2^k bytes.

      3/ total ROM size is not a factor of 2 and cartridge uses two chips of different sizes (Type B):
         ROM is not padded and the first 2^(k-1) bytes are mirrored each 2^k bytes while the next 2^(k-2) bytes are
         mirrored in the last 2^(k-2) bytes.

  ******************************************************************************************************************/
  
  /* calculate nearest size with factor of 2 */
  int size = 0x10000;
  while (cart.romsize > size)
    size <<= 1;

  /* total ROM size is not a factor of 2  */
  /* TODO: handle more possible ROM configurations (using cartridge database ???) */
  if ((size < MAXROMSIZE) && (cart.romsize < size))
  {
    /* two chips with different size */
    if (config.romtype)
    {
      /* third ROM section is mirrored in the last section */
      memcpy(cart.rom + cart.romsize, cart.rom + 2*cart.romsize - size, size - cart.romsize);
    }
    else
    {
      /* ROM is padded up to 2^k bytes */
      memset(cart.rom + cart.romsize, 0xff, size - cart.romsize);
    }
  }

  /* special case: Sonic & Knuckles */
  /* $200000-$3fffff is mapped to external cartridge */
  if (strstr(rominfo.international,"SONIC & KNUCKLES") != NULL)
  {
    /* disable ROM mirroring */
    size = 0x400000;
  }

  /* ROM is mirrored each 2^k bytes */
  cart.mask = size - 1;

  /**********************************************
          DEFAULT CARTRIDGE MAPPING 
  ***********************************************/
  for (i=0; i<0x40; i++)
  {
    /* cartridge ROM */
    m68k_memory_map[i].base     = cart.rom + ((i<<16) & cart.mask);
    m68k_memory_map[i].read8    = NULL;
    m68k_memory_map[i].read16   = NULL;
    m68k_memory_map[i].write8   = m68k_unused_8_w;
    m68k_memory_map[i].write16  = m68k_unused_16_w;
    zbank_memory_map[i].read    = NULL;
    zbank_memory_map[i].write   = zbank_unused_w;
  }

  for (i=0x40; i<0x80; i++)
  {
    /* unused area */
    m68k_memory_map[i].base     = cart.rom + (i<<16);
    m68k_memory_map[i].read8    = m68k_read_bus_8;
    m68k_memory_map[i].read16   = m68k_read_bus_16;
    m68k_memory_map[i].write8   = m68k_unused_8_w;
    m68k_memory_map[i].write16  = m68k_unused_16_w;
    zbank_memory_map[i].read    = zbank_unused_r;
    zbank_memory_map[i].write   = zbank_unused_w;
  }

  /**********************************************
          BACKUP MEMORY 
  ***********************************************/
  sram_init();
  eeprom_init();
  if (sram.on)
  {
    if (sram.custom)
    {
      /* Serial EEPROM */
      m68k_memory_map[eeprom.type.sda_out_adr >> 16].read8  = eeprom_read_byte;
      m68k_memory_map[eeprom.type.sda_out_adr >> 16].read16 = eeprom_read_word;
      m68k_memory_map[eeprom.type.sda_in_adr >> 16].read8   = eeprom_read_byte;
      m68k_memory_map[eeprom.type.sda_in_adr >> 16].read16  = eeprom_read_word;
      m68k_memory_map[eeprom.type.scl_adr >> 16].write8     = eeprom_write_byte;
      m68k_memory_map[eeprom.type.scl_adr >> 16].write16    = eeprom_write_word;
      zbank_memory_map[eeprom.type.sda_out_adr >> 16].read  = eeprom_read_byte;
      zbank_memory_map[eeprom.type.sda_in_adr >> 16].read   = eeprom_read_byte;
      zbank_memory_map[eeprom.type.scl_adr >> 16].write     = eeprom_write_byte;
    }
    else
    {
      /* Static RAM (64k max.) */
      m68k_memory_map[sram.start >> 16].base    = sram.sram;
      m68k_memory_map[sram.start >> 16].read8   = NULL;
      m68k_memory_map[sram.start >> 16].read16  = NULL;
      m68k_memory_map[sram.start >> 16].write8  = NULL;
      m68k_memory_map[sram.start >> 16].write16 = NULL;
      zbank_memory_map[sram.start >> 16].read   = NULL;
      zbank_memory_map[sram.start >> 16].write  = NULL;
    }
  }

  /**********************************************
          SVP CHIP 
  ***********************************************/
  svp = NULL;
  if (strstr(rominfo.international,"Virtua Racing") != NULL)
  {
    svp_init();

    m68k_memory_map[0x30].base    = svp->dram;
    m68k_memory_map[0x30].read16  = NULL;
    m68k_memory_map[0x30].write16 = svp_write_dram;

    m68k_memory_map[0x31].base    = svp->dram + 0x10000;
    m68k_memory_map[0x31].read16  = NULL;
    m68k_memory_map[0x31].write16 = svp_write_dram;

    m68k_memory_map[0x39].read16  = svp_read_cell_1;
    m68k_memory_map[0x3a].read16  = svp_read_cell_2;
  }

  /**********************************************
          J-CART 
  ***********************************************/
  if (cart.jcart)
  {
    m68k_memory_map[0x38].read16  = jcart_read;
    m68k_memory_map[0x38].write16 = jcart_write;
    m68k_memory_map[0x3f].read16  = jcart_read;
    m68k_memory_map[0x3f].write16 = jcart_write;
  }

  /**********************************************
          LOCK-ON 
  ***********************************************/
  
  /* clear all existing patches */
  ggenie_shutdown();
  datel_shutdown();

  /* initialize extra hardware */
  cart.lock_on = 0;
  switch (config.lock_on)
  {
    case TYPE_GG:
      ggenie_init();
      break;

    case TYPE_AR:
      datel_init();
      break;

    case TYPE_SK:
    {
      /* store S&K ROM above cartridge ROM + SRAM */
      if (cart.romsize > 0x600000) break;

       /* load Sonic & Knuckles ROM (2 MBytes) */
      FILE *f = fopen(SK_ROM,"r+b");
      if (!f) break;
      int done = 0;
      while (done < 0x200000)
      {
        fread(cart.rom + 0x600000 + done, 2048, 1, f);
        done += 2048;
      }
      fclose(f);

      /* load Sonic 2 UPMEM ROM (256 KBytes) */
      f = fopen(SK_UPMEM,"r+b");
      if (!f) break;
      done = 0;
      while (done < 0x40000)
      {
        fread(cart.rom + 0x800000 + done, 2048, 1, f);
        done += 2048;
      }
      fclose(f);
          
#ifdef LSB_FIRST
      /* Byteswap ROM */
      int i;
      uint8 temp;
      for(i = 0; i < 0x240000; i += 2)
      {
        temp = cart.rom[i + 0x600000];
        cart.rom[i + 0x600000] = cart.rom[i + 0x600000 + 1];
        cart.rom[i + 0x600000 + 1] = temp;
      }
#endif

      /*$000000-$1fffff is mapped to S&K ROM */
      for (i=0x00; i<0x20; i++)
        m68k_memory_map[i].base = (cart.rom + 0x600000) + (i<<16);

      cart.lock_on = 1;
      break;
    }

    default:
      break;
  }

  /**********************************************
        Cartridge Extra Hardware
  ***********************************************/
  memset(&cart.hw, 0, sizeof(T_CART_HW));

  /* search for game into database */
  for (i=0; i < CART_CNT + 1; i++)
  {
    /* known cart found ! */
    if ((rominfo.checksum == rom_database[i].chk_1) &&
        (rominfo.realchecksum == rom_database[i].chk_2))
    {
      /* retrieve hardware information */
      memcpy(&cart.hw, &(rom_database[i].cart_hw), sizeof(T_CART_HW));

      /* initialize memory handlers for $400000-$7fffff region */
      int j = rom_database[i].bank_start;
      while (j <= rom_database[i].bank_end)
      {
        if (cart.hw.regs_r)
        {
          m68k_memory_map[j].read8    = cart.hw.regs_r;
          m68k_memory_map[j].read16   = cart.hw.regs_r;
          zbank_memory_map[j].read    = cart.hw.regs_r;
        }
        if (cart.hw.regs_w)
        {
          m68k_memory_map[j].write8   = cart.hw.regs_w;
          m68k_memory_map[j].write16  = cart.hw.regs_w;
          zbank_memory_map[j].write   = cart.hw.regs_w;
        }
        j++;
      }

      /* leave loop */
      i = CART_CNT + 1;
    }
  }

#if M68K_EMULATE_ADDRESS_ERROR
  /* default behavior */
  emulate_address_error = config.addr_error; 
#endif

  /* detect special cartridges */
  if (cart.romsize > 0x800000)
  {
    /* Ultimate MK3 (hack) */
    for (i=0x40; i<0xA0; i++)
    {
      m68k_memory_map[i].base     = cart.rom + (i<<16);
      m68k_memory_map[i].read8    = NULL;
      m68k_memory_map[i].read16   = NULL;
      zbank_memory_map[i].read    = NULL;
    }

#if M68K_EMULATE_ADDRESS_ERROR
    /* this game does not work properly on real hardware */
    emulate_address_error = 0;  
#endif
  }
  else if (cart.romsize > 0x400000)
  {
    /* assume SSF2 mapper */
    cart.hw.bankshift = 1;
    cart.hw.time_w = sega_mapper_w;
  }

  /* default write handler for !TIME signal */
  if (!cart.hw.time_w)
    cart.hw.time_w = default_time_w;
}

/* hardware that need to be reseted on power on */
void cart_hw_reset()
{
  int i;

  /* reset bankshifting */
  if (cart.hw.bankshift)
  {
    for (i=0x00; i<0x40; i++)
      m68k_memory_map[i].base = cart.rom + ((i<<16) & cart.mask);
  }

  /* Realtec mapper */
  if (cart.hw.realtec & 1)
  {
    /* enable BOOTROM */
    for (i=0; i<8; i++)
      memcpy(cart.rom + 0x900000 + i*0x2000, cart.rom + 0x7e000, 0x2000);
    for (i=0x00; i<0x40; i++)
      m68k_memory_map[i].base = cart.rom + 0x900000;
    cart.hw.realtec |= 2;
  }

  /* SVP chip */
  if (svp)
    svp_reset();

  /* Lock-ON */
  switch (config.lock_on)
  {
    case TYPE_GG:
      ggenie_reset();
      break;

    case TYPE_AR:
      datel_reset(1);
      break;

    case TYPE_SK:
      if (cart.lock_on)
      {
      	/* disable UPMEM chip at $300000-$3fffff */
        for (i=0x30; i<0x40; i++)
          m68k_memory_map[i].base = cart.rom + ((i<<16) & cart.mask);
      }
      break;

    default:
      break;
  }

  /* save default cartridge slot mapping */
  cart.base = m68k_memory_map[0].base;
}


/************************************************************
          MAPPER handlers 
*************************************************************/

/* 
  "official" ROM/RAM switch
*/
static void sega_mapper_w(uint32 address, uint32 data)
{
  uint32 i,slot = (address >> 1) & 7;
  uint8 *src;

  switch (slot)
  {
    case 0:
      /* ROM/SRAM switch (Phantasy Star IV, Story of Thor/Beyond Oasis, Sonic 3 & Knuckles) */
      if (data & 1)
      {
        /* $200000-$3fffff is mapped to SRAM (only if SRAM exists) */
        if (sram.on)
        {
          for (i=0x20; i<0x40; i++)
            m68k_memory_map[i].base  = sram.sram;

          if (data & 2)
          {
            /* SRAM write disabled */
            for (i=0x20; i<0x40; i++)
            {
              m68k_memory_map[i].write8  = m68k_unused_8_w;
              m68k_memory_map[i].write16 = m68k_unused_16_w;
              zbank_memory_map[i].write  = zbank_unused_w;
            }
          }
          else
          {
            /* SRAM write enabled */
            for (i=0x20; i<0x40; i++)
            {
              m68k_memory_map[i].write8  = NULL;
              m68k_memory_map[i].write16 = NULL;
              zbank_memory_map[i].write  = NULL;
            }
          }
        }

        if (cart.lock_on)
        {
          /* enable UPMEM chip at $300000-$3fffff */
          for (i=0x30; i<0x40; i++)
            m68k_memory_map[i].base = (cart.rom + 0x800000) + ((i & 3)<<16);
        }
      }
      else
      {
        /* ROM enabled */
        for (i=0x20; i<0x40; i++)
          m68k_memory_map[i].base  = cart.rom + ((i<<16) & cart.mask);

        if (cart.lock_on)
        {
          /* enable cartridge ROM at $300000-$3fffff */
          for (i=0x30; i<0x40; i++)
            m68k_memory_map[i].base = cart.rom + ((i<<16) & cart.mask);
        }
      }
      break;

    default:
      /* ROM Bankswitch (Super Street Fighter 2)
         documented by Bart Trzynadlowski (http://www.trzy.org/files/ssf2.txt) 
      */
      slot = slot << 3; /* 8 x 512k banks */
      src = cart.rom + (data << 19);
      for (i=0; i<8; i++)
        m68k_memory_map[slot++].base = src + (i<<16);
      break;
  }
}

/* 
  custom ROM Bankswitch used by pirate "Multi-in-1" carts
*/
static void multi_mapper_w(uint32 address, uint32 data)
{
  int i;

  cart.hw.bankshift = 1;

  /* 64 x 64k banks */
  for (i=0; i<64; i++)
    m68k_memory_map[i].base = &cart.rom[((address++) & 0x3f) << 16];
}

/* 
  Special ROM Bankswitch used for copy protection
  Used by unlicensed cartridges (Lion King III, Super King Kong 99)
*/
static void special_mapper_w(uint32 address, uint32 data)
{
  /* 1 x 32k bank */
  memcpy(cart.rom + 0x900000, cart.rom + ((data & 0x7f) << 15), 0x8000);
  memcpy(cart.rom + 0x908000, cart.rom + 0x8000, 0x8000);
  m68k_memory_map[0].base = cart.rom + 0x900000;
}

/* 
  Realtec ROM Bankswitch (Earth Defend, Balloon Boy & Funny World, Whac-A-Critter)
  (Note: register usage is inverted in TascoDlx documentation)
*/
static void realtec_mapper_w(uint32 address, uint32 data)
{
  switch (address)
  {
    case 0x402000:  
    {
      /* number of mapped 64k blocks (the written value is a number of 128k blocks) */
      cart.hw.regs[2] = data << 1;
      return;
    }

    case 0x404000:
    {
      /* 00000xxx */
      cart.hw.regs[0] = data & 7;
      return;
    }

    case 0x400000:  
    {
      /* 00000yy1 */
      cart.hw.regs[1] = data & 6;

      /* mapped start address is 00yy xxx0 0000 0000 0000 0000 */
      uint32 base = (cart.hw.regs[0] << 1) | (cart.hw.regs[1] << 3);

      /* ensure mapped size is not null */
      if (cart.hw.regs[2])
      {
        /* selected blocks are mirrored into the whole cartridge area */
        int i;
        for (i=0x00; i<0x40; i++)
          m68k_memory_map[i].base = &cart.rom[(base + (i % cart.hw.regs[2])) << 16];
      }
      return;
    }
  }
}

/* Game no Kanzume Otokuyou ROM Mapper */
static void seganet_mapper_w(uint32 address, uint32 data)
{
  if ((address & 0xff) == 0xf1)
  {
    int i;
    if (data & 1)
    {
      /* ROM Write protected */
      for (i=0; i<0x40; i++)
      {
        m68k_memory_map[i].write8   = m68k_unused_8_w;
        m68k_memory_map[i].write16  = m68k_unused_16_w;
        zbank_memory_map[i].write   = zbank_unused_w;
      }
    }
    else
    {
      /* ROM Write enabled */
      for (i=0; i<0x40; i++)
      {
        m68k_memory_map[i].write8   = NULL;
        m68k_memory_map[i].write16  = NULL;
        zbank_memory_map[i].write   = NULL;
      }
    }
  }
}

/*
  RADICA ROM Bankswitch (use !TIME)
*/
static uint32 radica_mapper_r(uint32 address)
{
  int i = 0;
  address = (address >> 1);
  
  /* 64 x 64k banks */
  for (i = 0; i < 64; i++)
    m68k_memory_map[i].base = &cart.rom[((address++)& 0x3f)<< 16];

  return 0xff;
}


/************************************************************
          default !TIME signal handler 
*************************************************************/

/* default ROM bankswitch */
static void default_time_w(uint32 address, uint32 data)
{
  if ((address & 0xf1) == 0xf1)
    sega_mapper_w(address, data);

  else if (address < 0xa13040)
    multi_mapper_w(address, data);
}


/************************************************************
          Internal register handlers
*************************************************************/

static uint32 default_regs_r(uint32 address)
{
  int i;
  for (i=0; i<4; i++)
  {
    if ((address & cart.hw.mask[i]) == cart.hw.addr[i])
      return cart.hw.regs[i];
  }

  /* unused */
  return 0xffff;
}

static void default_regs_w(uint32 address, uint32 data)
{
  int i;
  for (i=0; i<4; i++)
  {
    if ((address & cart.hw.mask[i]) == cart.hw.addr[i])
      cart.hw.regs[i] = data;
  }
}

/* special register behaviour (Lion King III, Super Donkey Kong  99) */
static void special_regs_w(uint32 address, uint32 data)
{
  /* ROM bankswitch */
  if ((address >> 16) > 0x6f)
  {
    special_mapper_w(address, data);
    return;
  }

  /* write regs */
  default_regs_w(address, data);

  /* bitswapping (documented by Haze) */
  uint32 temp = cart.hw.regs[0];
  switch (cart.hw.regs[1])
  {
    case 1:
      cart.hw.regs[2] = (temp >> 1);
      return;

    case 2:
      cart.hw.regs[2] = ((temp >> 4) | ((temp & 0x0F) << 4));
      return;

    default:
      cart.hw.regs[2] = (((temp >> 7) & 0x01) | ((temp >> 5) & 0x02) |
                         ((temp >> 3) & 0x04) | ((temp >> 1) & 0x08) |
                         ((temp << 1) & 0x10) | ((temp << 3) & 0x20) |
                         ((temp << 5) & 0x40) | ((temp << 7) & 0x80));
      return;
  }
}
