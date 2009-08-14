/****************************************************************************
 *  Genesis Plus
 *  DATEL Action Replay / Pro Action Replay emulation
 *
 *  Copyright (C) 2009  Eke-Eke (GCN/Wii port)
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

#define TYPE_PRO1 0x12
#define TYPE_PRO2 0x22

static struct
{
  uint8 enabled;
  uint8 rom[0x20000];
  uint8 ram[0x10000];
  uint16 regs[13];
  uint16 old[4];
  uint16 data[4];
  uint32 addr[4];
} action_replay;

static void wram_write_byte(uint32 address, uint32 data);
static void wram_write_word(uint32 address, uint32 data);
static void ar_write_regs(uint32 address, uint32 data);
static void ar_write_regs_pro2(uint32 address, uint32 data);

void datel_init(void)
{
  int i;
  memset(&action_replay,0,sizeof(action_replay));

  /* load Action Replay ROM program */
  FILE *f = fopen(AR_ROM,"rb");
  if (!f) return;
  int size = fread(action_replay.rom,1,0x20000,f);
  fclose(f);

  /* detect Action Replay yype */
  if (size < 0x10000)
    action_replay.enabled = TYPE_AR;
  else if (size < 0x20000)
    action_replay.enabled = TYPE_PRO2;
  else
    action_replay.enabled = TYPE_PRO1;

  /* default memory map */
  switch (action_replay.enabled)
  {
    case TYPE_AR:
    {
      /* internal registers mapped at $0000-$ffff */
      m68k_memory_map[0x01].write16 = ar_write_regs;

      /* $0000-$7fff mirrored into $8000-$ffff */
      memcpy(action_replay.rom+0x8000,action_replay.rom,0x8000);
      break;
    }

    case TYPE_PRO1:
    {
      /* internal registers mapped at $0000-$ffff */
      m68k_memory_map[0x01].write16 = ar_write_regs;

      /* RAM (64k) mapped at $400000-$7fffff */
      for (i=0x40; i<0x80; i++)
      {
        m68k_memory_map[i].base     = action_replay.ram;
        m68k_memory_map[i].read8    = NULL;
        m68k_memory_map[i].read16   = NULL;
        m68k_memory_map[i].write8   = NULL;
        m68k_memory_map[i].write16  = NULL;
      }
      break;
    }

    case TYPE_PRO2:
    {
      /* internal registers mapped at $100000-$10ffff */
      m68k_memory_map[0x10].write16 = ar_write_regs_pro2;

      /* RAM (64k) mapped at $400000-$7fffff */
      for (i=0x40; i<0x80; i++)
      {
        m68k_memory_map[i].base     = action_replay.ram;
        m68k_memory_map[i].read8    = NULL;
        m68k_memory_map[i].read16   = NULL;
        m68k_memory_map[i].write8   = NULL;
        m68k_memory_map[i].write16  = NULL;
      }
      break;
    }
  }

#ifdef LSB_FIRST
  /* Byteswap ROM */
  uint8 temp;
  for(i = 0; i < 0x20000; i += 2)
  {
    temp = action_replay.rom[i];
    action_replay.rom[i] = action_replay.rom[i+1];
    action_replay.rom[i+1] = temp;
  }
#endif
}

void datel_reset(void)
{
  if (action_replay.enabled)
  {
    /* reset codes */
    datel_switch(0);

    /* reset internal state */
    memset(action_replay.regs,0,sizeof(action_replay.regs));
    memset(action_replay.old,0,sizeof(action_replay.old));
    memset(action_replay.data,0,sizeof(action_replay.data));
    memset(action_replay.addr,0,sizeof(action_replay.addr));

    /* ROM mapped at $000000-$3fffff */
    int i;
    switch (action_replay.enabled)
    { 
      case TYPE_AR:   /* 32k ROM */
      case TYPE_PRO2: /* 64k ROM */
      {
        for (i=0x00; i<0x40; i++)
        {
          m68k_memory_map[i].base = action_replay.rom;
        }
        break;
      }

      case TYPE_PRO1: /* 128k ROM */
      {
        for (i=0x00; i<0x40; i+=2)
        {
          m68k_memory_map[i].base   = action_replay.rom;
          m68k_memory_map[i+1].base = action_replay.rom + 0x10000;
        }
        break;
      }
    }
  }
}

void datel_switch(uint8 enable)
{
  int i;
  if (enable)
  {
    int offset;

    /* store old values */
    for (i=0; i<4; i++)
    {
      if (action_replay.data[i])
      {
        offset = action_replay.addr[i] >> 16;

        if (offset < 0x40)        /* cartridge ROM */
          action_replay.old[i] = *(uint16 *)(cart.rom + action_replay.addr[i]);
        else if (offset >= 0xe0)  /* Work RAM */
          action_replay.old[i] = *(uint16 *)(work_ram + (action_replay.addr[i]&0xffff));
      }
    }

    /* patch new values */
    for (i=0; i<4; i++)
    {
      if (action_replay.data[i])
      {
        offset = action_replay.addr[i] >> 16;

        if (offset < 0x40)        /* cartridge ROM */
          *(uint16 *)(cart.rom + action_replay.addr[i]) = action_replay.data[i];
        else if (offset >= 0xe0)  /* Work RAM */
          *(uint16 *)(work_ram + (action_replay.addr[i]&0xffff)) = action_replay.data[i];
      }
    }

    /* set RAM write handlers */
    for (i=0xe0; i<0x100; i++)
    {
      m68k_memory_map[i].write8   = wram_write_byte;
      m68k_memory_map[i].write16  = wram_write_word;
    }
  }
  else
  {
    /* restore original data */
    for (i=0; i<4; i++)
    {
      if (action_replay.data[i])
      {
        if (action_replay.addr[i] < 0x400000)
          *(uint16 *)(cart.rom + action_replay.addr[i]) = action_replay.old[i];
        else if (action_replay.addr[i] >= 0xe00000)
          *(uint16 *)(work_ram + (action_replay.addr[i]&0xffff)) = action_replay.old[i];
      }
    }
  }
}


static void wram_write_byte(uint32 address, uint32 data)
{
  int i;
  for (i=0; i<4; i++)
  {
    if ((address & 0xe0fffe) == (action_replay.addr[i]&0xe0fffe))
    {
      if (address & 1)  /* lower byte write */
        action_replay.old[i] = (action_replay.old[i] & 0xff00) | (data & 0xff);
      else              /* upper byte write */
        action_replay.old[i] = (action_replay.old[i] & 0x00ff) | (data << 8);
      return;
    }
  }

  WRITE_BYTE(work_ram, address & 0xffff, data);
}

static void wram_write_word(uint32 address, uint32 data)
{
  int i;
  for (i=0; i<4; i++)
  {
    if ((address & 0xe0fffe) == (action_replay.addr[i]&0xe0fffe))
    {
      action_replay.old[i] = data;
      return;
    }
  }

  *(uint16 *)(work_ram + (address & 0xffff)) = data;
}

static void ar_write_regs(uint32 address, uint32 data)
{
  if ((address > 0x10018) || (action_replay.regs[3] == 0xffff))
  {
    m68k_unused_16_w(address,data);
    return;
  }

  /* register offset */
  int offset = (address >> 1) & 0x0F;

  /* update internal register */
  action_replay.regs[offset] = data;

  /* decode patch value & address on exit */
  if ((offset == 3) && (data == 0xffff))
  {
    /* decode patch data */
    action_replay.data[0] = action_replay.regs[0];
    action_replay.data[1] = action_replay.regs[4];
    action_replay.data[2] = action_replay.regs[7];
    action_replay.data[3] = action_replay.regs[10];

    /* decode patch address */
    action_replay.addr[0] = (action_replay.regs[1]   | ((action_replay.regs[2]   & 0x7f00) << 8)) << 1;
    action_replay.addr[1] = (action_replay.regs[5]   | ((action_replay.regs[6]   & 0x7f00) << 8)) << 1;
    action_replay.addr[2] = (action_replay.regs[8]   | ((action_replay.regs[9]   & 0x7f00) << 8)) << 1;
    action_replay.addr[3] = (action_replay.regs[11]  | ((action_replay.regs[12]  & 0x7f00) << 8)) << 1;

    /* Cartridge ROM mapped to $000000-$3fffff */
    /* NOTE: codes should be disabled on startup */
    int i;
    for (i=0x00; i<0x40; i++)
    {
      m68k_memory_map[i].base = cart.rom + ((i<<16) & cart.mask);
    }
  }
}


static void ar_write_regs_pro2(uint32 address, uint32 data)
{
  /* TODO */
}
