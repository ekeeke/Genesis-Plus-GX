/****************************************************************************
 *  Genesis Plus
 *  Game Genie Hardware emulation
 *
 *  Copyright (C) 2009  Eke-Eke (GCN/Wii port)
 *
 *  Based on documentation from Charles McDonald
 *  (http://cgfm2.emuviews.com/txt/genie.txt)
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

static struct
{
  uint8 enabled;
  uint8 rom[0x10000];
  uint16 regs[0x20];
  uint16 old[6];
  uint16 data[6];
  uint32 addr[6];
} ggenie;

static void ggenie_write_byte(uint32 address, uint32 data);
static void ggenie_write_word(uint32 address, uint32 data);
static void ggenie_write_regs(uint8 offset, uint32 data, uint8 type);
static uint32 ggenie_read_regs(uint32 address);

void ggenie_init(void)
{
  memset(&ggenie,0,sizeof(ggenie));

  /* load Game Genie ROM program */
  FILE *f = fopen(GG_ROM,"rb");
  if (!f) return;
  fread(ggenie.rom,1,0x8000,f);
  fclose(f);

  /* $0000-$7fff mirrored into $8000-$ffff */
  memcpy(ggenie.rom+0x8000,ggenie.rom,0x8000);

#ifdef LSB_FIRST
  /* Byteswap ROM */
  int i;
  uint8 temp;
  for(i = 0; i < 0x10000; i += 2)
  {
    temp = ggenie.rom[i];
    ggenie.rom[i] = ggenie.rom[i+1];
    ggenie.rom[i+1] = temp;
  }
#endif

  /* set flag */
  ggenie.enabled = 1;
}

void ggenie_reset(void)
{
  if (!ggenie.enabled) return;

  /* restore patched ROM */
  int i;
  for (i=0; i<6; i++)
  {
    if (ggenie.regs[0] & (1 << i))
      *(uint16 *)(cart_rom + ggenie.addr[i]) = ggenie.old[i];
  }

  /* reset internal state */
  memset(ggenie.regs,0,sizeof(ggenie.regs));
  memset(ggenie.old,0,sizeof(ggenie.old));
  memset(ggenie.data,0,sizeof(ggenie.data));
  memset(ggenie.addr,0,sizeof(ggenie.addr));

  /* default slot mapping */
  /* reads  are mapped to Game Genie ROM */
  /* writes are mapped to Game Genie internal registers */
  m68k_memory_map[0].base     = ggenie.rom;
  m68k_memory_map[0].write8   = ggenie_write_byte;
  m68k_memory_map[0].write16  = ggenie_write_word;
  m68k_memory_map[0].read16   = NULL;
}


/* Byte write handler */
/* Note: 2nd revision of the Game Genie software use byte writes to set register values on exit */
static void ggenie_write_byte(uint32 address, uint32 data)
{
  /* Lock bit */
  if (ggenie.regs[0] & 0x100) return;

  /* Register offset */
  uint8 offset = (address >> 1) & 0x1f;

  /* Write internal register (lower or upper BYTE) */
  ggenie_write_regs(offset,data,address & 1);
}

/* Word write handler */
static void ggenie_write_word(uint32 address, uint32 data)
{
  /* Lock bit */
  if (ggenie.regs[0] & 0x100) return;

  /* Register offset */
  uint8 offset = (address >> 1) & 0x1f;

  /* Write internal register (full WORD) */
  ggenie_write_regs(offset,data,2);
}

static void ggenie_write_regs(uint8 offset, uint32 data, uint8 type)
{
  /* access type */
  switch (type) 
  {
    case 0:   /* upper byte write */
      data = (ggenie.regs[offset] & 0x00ff) | ((data & 0xff) << 8);
      break;

    case 1:   /* lower byte write */
      data = (ggenie.regs[offset] & 0xff00) | (data & 0xff);
      break;

    default:
      break;
  }

  /* Mode Register */
  if (!offset)
  {
    /* by default, registers are write only */
    m68k_memory_map[0].read16 = NULL;

    /* MODE bits */
    if (data & 0x400)
    {
      /* $0000-$7ffff reads mapped to Cartridge ROM */
      m68k_memory_map[0].base = cart_rom;
    }
    else
    {
      /* $0000-$7ffff reads mapped to Game Genie ROM */
      m68k_memory_map[0].base = ggenie.rom;

      if (data & 0x200)
      {
        /* $0000-$7ffff reads mapped to Game Genie Registers */
        /* code doing this should execute in RAM so we don't need to modify base address */
        m68k_memory_map[0].read16 = ggenie_read_regs; 
      }
    }

    /* LOCK bit */
    if ((data & 0x100) && !(ggenie.regs[0] & 0x100))
    {
      /* patch ROM when GG program exits (LOCK bit set) */
      /* this is done here to handle patched program reads faster & more easily */
      /* on real HW, address decoding would be done on each reads */
      int i,j;
      for (i=0; i<6; i++)
      {
        /* decode patch address */
        /* note: Charles's documment is wrong, first register holds bits 23-16 of patch address */
        ggenie.addr[i] = (ggenie.regs[2+3*i] << 16) | ggenie.regs[3+3*i];

        /* decode patch data */
        ggenie.data[i] = ggenie.regs[4+3*i];

        /* patch is enabled ? */
        if (data & (1 << i))
        {
          /* first look if address has not already been patched */
          for (j=0;j<i;j++)
          {
            if (ggenie.addr[i] == ggenie.addr[j])
            {
              /* disable code for later initialization */
              data &= ~(1 << i);
            }
          }

          /* ave old value and patch ROM if enabled */
          if (data & (1 << i))
          {
            ggenie.old[i] = *(uint16 *)(cart_rom + ggenie.addr[i]);
            *(uint16 *)(cart_rom + ggenie.addr[i]) = ggenie.data[i];
          }
        }
      }
    }
  }

  /* update internal register */
  ggenie.regs[offset] = data;
}

static uint32 ggenie_read_regs(uint32 address)
{
  if (address < 0x40) return ggenie.regs[address >> 1];
  else return *(uint16 *)(cart_rom + address); /* is that correct ? */
}

