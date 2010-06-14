/***************************************************************************************
 *  Genesis Plus
 *  Z80 bank access to 68k bus
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Eke-Eke (2007,2008,2009), additional code & fixes for the GCN/Wii port
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

unsigned int zbank_unused_r(unsigned int address)
{
#ifdef LOGERROR
  error("Z80 bank unused read %06X\n", address);
#endif
  return (address & 1) ? 0x00 : 0xff;
}

void zbank_unused_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Z80 bank unused write %06X = %02X\n", address, data);
#endif
}

unsigned int zbank_lockup_r(unsigned int address)
{
#ifdef LOGERROR
  error("Z80 bank lockup read %06X\n", address);
#endif
  if (!config.force_dtack)
  {
    mcycles_z80 = 0xffffffff;
    zstate = 0;
  }
  return 0xff;
}

void zbank_lockup_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Z80 bank lockup write %06X = %02X\n", address, data);
#endif
  if (!config.force_dtack)
  {
    mcycles_z80 = 0xffffffff;
    zstate = 0;
  }
}

/* I/O & Control registers */
unsigned int zbank_read_ctrl_io(unsigned int address)
{
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if (address & 0xe0)
      {
        return zbank_unused_r(address);
      }
      return (io_read((address >> 1) & 0x0f));
    }

    case 0x11:  /* BUSACK */
    {
      if (address & 1)
      {
        return zbank_unused_r(address);
      }
      return 0xff;
    }

    case 0x30:  /* TIME */
    {
      if (!cart.hw.time_r)
      {
        return zbank_unused_r(address);
      }
      unsigned int data = cart.hw.time_r(address);
      if (address & 1)
      {
        return (data & 0xff);
      }
      return (data >> 8);
    }

    case 0x41:  /* OS ROM */
    {
      if (!(address & 1))
      {
        return zbank_unused_r(address);
      }
      return (gen_bankswitch_r() | 0xfe);
    }

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* RESET */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP REGISTERS */
    {
      return zbank_unused_r(address);
    }

    default:  /* Invalid address */
    {
      return zbank_lockup_r(address);
    }
  }
}

void zbank_write_ctrl_io(unsigned int address, unsigned int data)
{
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      /* get /LWR only */
      if ((address & 0xe1) != 0x01)
      {
        zbank_unused_w(address, data);
        return;
      }
      io_write((address >> 1) & 0x0f, data);
      return;
    }

    case 0x11:  /* BUSREQ */
    {
      if (address & 1) 
      {
        zbank_unused_w(address, data);
        return;
      }
      gen_zbusreq_w(data & 1, mcycles_z80);
      return;
    }

    case 0x12:  /* RESET */
    {
      if (address & 1)
      {
        zbank_unused_w(address, data);
        return;
      }
      gen_zreset_w(data & 1, mcycles_z80);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x41:  /* OS ROM */
    {
      if (!(address & 1))
      {
        zbank_unused_w(address, data);
        return;
      }
      gen_bankswitch_w(data & 1);
      return;
    }

    case 0x10:  /* MEMORY MODE */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP REGISTERS */
    {
      zbank_unused_w(address, data);
      return;
    }

    default:  /* Invalid address */
    {
      zbank_lockup_w(address, data);
      return;
    }
  }
}


/* VDP */
unsigned int zbank_read_vdp(unsigned int address)
{
  switch (address & 0xfd)
  {
    case 0x00:    /* DATA */
    {
      return (vdp_data_r() >> 8);
    }
      
    case 0x01:    /* DATA */
    {
      return (vdp_data_r() & 0xff);
    }
      
    case 0x04:    /* CTRL */
    {
      return (((vdp_ctrl_r(mcycles_z80) >> 8) & 3) | 0xfc);
    }

    case 0x05:    /* CTRL */
    {
      return (vdp_ctrl_r(mcycles_z80) & 0xff);
    }
      
    case 0x08:    /* HVC */
    case 0x0c:
    {
      return (vdp_hvc_r(mcycles_z80) >> 8);
    }

    case 0x09:    /* HVC */
    case 0x0d:
    {
      return (vdp_hvc_r(mcycles_z80) & 0xff);
    }

    case 0x18:    /* Unused */
    case 0x19:
    case 0x1c:
    case 0x1d:
    {
      return zbank_unused_r(address);
    }

    default:    /* Invalid address */
    {
      return zbank_lockup_r(address);
    }
  }
}

void zbank_write_vdp(unsigned int address, unsigned int data)
{
  switch (address & 0xfc)
  {
    case 0x00:  /* Data port */
    {
      vdp_data_w(data << 8 | data);
      return;
    }

    case 0x04:  /* Control port */
    {
      vdp_ctrl_w(data << 8 | data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      if (!(address & 1))
      {
        zbank_unused_w(address, data);
        return;
      }
      psg_write(mcycles_z80, data);
      return;
    }
             
    case 0x18: /* Unused */
    {
      zbank_unused_w(address, data);
      return;
    }

    case 0x1c:  /* TEST register */
    {
      vdp_test_w(data << 8 | data);
      return;
    }

    default:  /* Invalid address */
    {
      zbank_lockup_w(address, data);
      return;
    }
  }
}
