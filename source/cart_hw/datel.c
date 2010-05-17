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

struct
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
  memset(&action_replay,0,sizeof(action_replay));

  /* Open Action Replay ROM */
  FILE *f = fopen(AR_ROM,"rb");
  if (!f)
    return;

  /* ROM size */
  fseek(f, 0, SEEK_END);
  int size = ftell(f);

  /* Detect Action Replay type */
  switch (size)
  {
    case 0x8000:  /* ACTION REPLAY (32K) */
    {
      action_replay.enabled = TYPE_AR;

      /* $0000-$7fff mirrored into $8000-$ffff */
      memcpy(action_replay.rom+0x8000,action_replay.rom,0x8000);
      break;
    }

    case 0x10000:  /* PRO ACTION REPLAY 2 (64K) */
    {
      action_replay.enabled = TYPE_PRO2;

      /* RAM (64k) mapped at $600000-$60ffff */
      m68k_memory_map[0x60].base     = action_replay.ram;
      m68k_memory_map[0x60].read8    = NULL;
      m68k_memory_map[0x60].read16   = NULL;
      m68k_memory_map[0x60].write8   = NULL;
      m68k_memory_map[0x60].write16  = NULL;
      break;
    }

    case 0x20000:  /* PRO ACTION REPLAY (128K) */
    {
      action_replay.enabled = TYPE_PRO1;

      /* RAM (64k) mapped at $420000-$42ffff */
      m68k_memory_map[0x42].base     = action_replay.ram;
      m68k_memory_map[0x42].read8    = NULL;
      m68k_memory_map[0x42].read16   = NULL;
      m68k_memory_map[0x42].write8   = NULL;
      m68k_memory_map[0x42].write16  = NULL;
      break;
    }

    default:
      return;
  }

  if (action_replay.enabled)
  {
    /* Load ROM */
    fseek(f, 0, SEEK_SET);
    int i = 0;
    while (i < size)
    {
      fread(action_replay.rom+i,0x1000,1,f);
      i += 0x1000;
    }

#ifdef LSB_FIRST
    /* Byteswap ROM */
    uint8 temp;
    for(i = 0; i < size; i += 2)
    {
      temp = action_replay.rom[i];
      action_replay.rom[i] = action_replay.rom[i+1];
      action_replay.rom[i+1] = temp;
    }
#endif
  }

  fclose(f);
}

void datel_shutdown(void)
{
  if (action_replay.enabled)
  {
    datel_switch(0);
    action_replay.enabled = 0;
  }
}

void datel_reset(int hard_reset)
{
  /* reset external mapping */
  switch (action_replay.enabled)
  {
    case TYPE_AR:

      /* internal registers mapped at $010000-$01ffff */
      m68k_memory_map[0x01].write16 = ar_write_regs;

      /* internal ROM mapped at $000000-$00ffff */
      m68k_memory_map[0].base = action_replay.rom;
      break;

    case TYPE_PRO2:

      /* internal registers mapped at $100000-$10ffff */
      m68k_memory_map[0x10].write16 = ar_write_regs_pro2;

      /* internal ROM mapped at $000000-$00ffff */
      m68k_memory_map[0].base = action_replay.rom;
      break;

    case TYPE_PRO1:

      /* internal registers mapped at $010000-$01ffff */
      m68k_memory_map[0x01].write16 = ar_write_regs;

      /* internal ROM mapped at $000000-$01ffff */
      m68k_memory_map[0].base = action_replay.rom;
      m68k_memory_map[1].base = action_replay.rom + 0x10000;
      break;

    default:
      return;
  }

  /* clear existing codes */
  datel_switch(0);

  /* reset internal state */
  memset(action_replay.regs,0,sizeof(action_replay.regs));
  memset(action_replay.old,0,sizeof(action_replay.old));
  memset(action_replay.data,0,sizeof(action_replay.data));
  memset(action_replay.addr,0,sizeof(action_replay.addr));

  /* clear RAM on hard reset only */
  if (hard_reset)
    memset(action_replay.ram,0,sizeof(action_replay.ram));
}

void datel_switch(int enable)
{
  int i,use_wram = 0;
  if (enable)
  {
    for (i=0; i<4; i++)
    {
      if (action_replay.data[i])
      {
        /* store old values & patch new values */
        if (action_replay.addr[i] < 0x400000)
        {
          action_replay.old[i] = *(uint16 *)(cart.rom + (action_replay.addr[i]&~1));
          *(uint16 *)(cart.rom + (action_replay.addr[i]&~1)) = action_replay.data[i];
        }
        else
        {
          use_wram = 1;
          action_replay.old[i] = *(uint16 *)(work_ram + (action_replay.addr[i]&0xfffe));
          if (action_replay.data[i] & 0xff00)
            *(uint16 *)(work_ram + (action_replay.addr[i]&0xfffe)) = action_replay.data[i];
          else
            WRITE_BYTE(work_ram, (action_replay.addr[i]&0xfffe) + 1, action_replay.data[i] & 0xff);
        }
      }
    }

    if (use_wram)
    {
      /* use specific WRAM write handlers */
      for (i=0xe0; i<0x100; i++)
      {
        m68k_memory_map[i].write8   = wram_write_byte;
        m68k_memory_map[i].write16  = wram_write_word;
      }
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
        else
          *(uint16 *)(work_ram + (action_replay.addr[i]&0xfffe)) = action_replay.old[i];
      }
    }

    /* default WRAM write handlers */
    for (i=0xe0; i<0x100; i++)
    {
      m68k_memory_map[i].write8   = NULL;
      m68k_memory_map[i].write16  = NULL;
    }
  }
}


static void wram_write_byte(uint32 address, uint32 data)
{
  int i;
  for (i=0; i<4; i++)
  {
    if (action_replay.data[i])
    {
      if ((address & 0xfffe) == (action_replay.addr[i] & 0xfffe))
      {
        WRITE_BYTE(&action_replay.old[i], address & 1, data);
        if (action_replay.data[i] & 0xff00)
          return;
        if ((address & 0xffff) == ((action_replay.addr[i]&0xfffe) + 1))
          return;
      }
    }
  }

  WRITE_BYTE(work_ram, address & 0xffff, data);
}

static void wram_write_word(uint32 address, uint32 data)
{
  *(uint16 *)(work_ram + (address & 0xffff)) = data;

  int i;
  for (i=0; i<4; i++)
  {
    if (action_replay.data[i])
    {
      if ((address & 0xffff) == (action_replay.addr[i] & 0xfffe))
      {
        action_replay.old[i] = data;
        if (action_replay.data[i] & 0xff00)
          *(uint16 *)(work_ram + (address & 0xfffe)) = action_replay.data[i];
        else
          WRITE_BYTE(work_ram, (action_replay.addr[i] & 0xfffe) + 1, action_replay.data[i]);
      }
    }
  }
}

static void ar_write_regs(uint32 address, uint32 data)
{
  /* register offset */
  int offset = (address & 0xffff) >> 1;
  if (offset > 12)
  {
    m68k_unused_16_w(address,data);
    return;
  }

  /* update internal register */
  action_replay.regs[offset] = data;

  /* exit program */
  if (action_replay.regs[3] == 0xffff)
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

    /* enable Cartridge ROM */
    m68k_memory_map[0].base = cart.rom;
    m68k_memory_map[1].base = cart.rom + ((1<<16) & cart.mask);

    /* disable internal registers (?) */
    m68k_memory_map[0x01].write16 = m68k_unused_16_w;

    /* enable patches */
    datel_switch(1);
  }
}

static void ar_write_regs_pro2(uint32 address, uint32 data)
{
  /* enable Cartridge ROM */
  if (((address & 0xff) == 0x78) && (data == 0xffff))
    m68k_memory_map[0].base = cart.rom;
}
