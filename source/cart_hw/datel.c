/****************************************************************************
 *  Genesis Plus
 *  Action Replay / Pro Action Replay emulation
 *
 *  Copyright (C) 2009  Eke-Eke (GCN/Wii port)
 *
 *  Based on reverse-engineering done on DATEL softwares
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

#ifdef TEST_AR
static struct
{
  uint8 enabled;
  uint8 rom[0x10000];
  uint16 regs[13];
  uint16 old[4];
  uint16 data[4];
  uint32 addr[4];
} action_replay;

static void ar_write_byte(uint32 address, uint32 data);
static void ar_write_regs(uint32 address, uint32 data);
static void ar_write_word(uint32 address, uint32 data);

void ar_init(void)
{
  memset(&action_replay,0,sizeof(action_replay));

  /* load Game Genie ROM program */
  FILE *f = fopen("./areplay.bin","rb");
  if (!f) return;
  fread(action_replay.rom,1,0x8000,f);
  fclose(f);

  /* $0000-$7fff mirrored into $8000-$ffff */
  memcpy(action_replay.rom+0x8000,action_replay.rom,0x8000);

#ifdef LSB_FIRST
  /* Byteswap ROM */
  int i;
  uint8 temp;
  for(i = 0; i < 0x10000; i += 2)
  {
    temp = action_replay.rom[i];
    action_replay.rom[i] = action_replay.rom[i+1];
    action_replay.rom[i+1] = temp;
  }
#endif

  /* enable registers write */
  m68k_memory_map[1].write16 = ar_write_regs;

  /* set flag */
  action_replay.enabled = 1;
}

void ar_reset(void)
{
  if (!action_replay.enabled) return;

  /* restore patched ROM */
  int i;
  for (i=0; i<4; i++)
  {
    if (action_replay.addr[i] < 0x400000)
      *(uint16 *)(cart_rom + action_replay.addr[i]) = action_replay.old[i];
  }

  /* reset internal state */
  memset(action_replay.regs,0,sizeof(action_replay.regs));
  memset(action_replay.old,0,sizeof(action_replay.old));
  memset(action_replay.data,0,sizeof(action_replay.data));
  memset(action_replay.addr,0,sizeof(action_replay.addr));

  /* slot 0 is mapped to Action replay ROM */
  m68k_memory_map[0].base = action_replay.rom;

  /* reset RAM handlers */
  for (i=0xe0; i<0x100; i++)
  {
    m68k_memory_map[i].write8 = NULL;
    m68k_memory_map[i].write16 = NULL;
  }
}

static void ar_write_byte(uint32 address, uint32 data)
{
}

static void ar_write_word(uint32 address, uint32 data)
{
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
    /* patch data */
    action_replay.data[0] = action_replay.regs[0];
    action_replay.data[1] = action_replay.regs[4];
    action_replay.data[2] = action_replay.regs[7];
    action_replay.data[3] = action_replay.regs[10];

    /* patch address */
    action_replay.addr[0] = (action_replay.regs[1]   | ((action_replay.regs[2]   & 0x7f00) << 8)) << 1;
    action_replay.addr[1] = (action_replay.regs[5]   | ((action_replay.regs[6]   & 0x7f00) << 8)) << 1;
    action_replay.addr[2] = (action_replay.regs[8]   | ((action_replay.regs[9]   & 0x7f00) << 8)) << 1;
    action_replay.addr[3] = (action_replay.regs[11]  | ((action_replay.regs[12]  & 0x7f00) << 8)) << 1;

    int i;
    for (i=0; i<4; i++)
    {
      offset = action_replay.addr[i] >> 16;

      /* ROM area */
      if (offset < 0x40)
      {
        /* store old ROM value */
        action_replay.old[i] = *(uint16 *)(cart_rom + action_replay.addr[i]);
      }
      /* Work RAM area */
      else if (offset >= 0xe0)
      {
        /* patch RAM */
        *(uint16 *)(work_ram + (action_replay.addr[i] & 0xffff)) = action_replay.data[i];

        /* setup handlers */
        m68k_memory_map[offset].write8  = ar_write_byte;
        m68k_memory_map[offset].write16 = ar_write_word;
      }
    }

    for (i=0; i<4; i++)
    {
      offset = action_replay.addr[i] >> 16;

      /* ROM area */
      if (offset < 0x40)
      {
        /* patch ROM */
        *(uint16 *)(cart_rom + action_replay.addr[i]) = action_replay.data[i];;
      }
    }

    /* reads are mapped to Cartridge ROM */
    m68k_memory_map[0].base = cart_rom;
  }
}
#endif
