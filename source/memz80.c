/***************************************************************************************
 *  Genesis Plus 1.2a
 *  Z80 memory handler
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
#define LOG_PORT 0      /* 1= Log Z80 I/O port accesses */

/*
    Handlers for access to unused addresses and those which make the
    machine lock up.
*/
static inline void z80_unused_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Z80 unused write %04X = %02X\n", address, data);
#endif
}

static inline unsigned int z80_unused_r(unsigned int address)
{
#ifdef LOGERROR
  error("Z80 unused read %04X\n", address);
#endif
	return 0xff;
}

static inline void z80_lockup_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Z80 lockup write %04X = %02X\n", address, data);
#endif
 	gen_running = config.force_dtack;
}

static inline unsigned int z80_lockup_r(unsigned int address)
{
#ifdef LOGERROR
  error("Z80 lockup read %04X\n", address);
#endif
 	gen_running = config.force_dtack;
	return 0xff;
}
/*
    VDP access
*/
static inline unsigned int z80_vdp_r(unsigned int address)
{
  switch (address & 0xff)
  {
    case 0x00: /* VDP data port */
    case 0x02:
      return (vdp_data_r() >> 8) & 0xff;
          
    case 0x01: /* VDP data port */ 
    case 0x03:
      return (vdp_data_r() & 0xff);

    case 0x04: /* VDP control port */
    case 0x06:
      return (0xfc | ((vdp_ctrl_r() >> 8) & 3));
          
    case 0x05: /* VDP control port */
    case 0x07:
      return (vdp_ctrl_r() & 0xff);

    case 0x08: /* HV counter */
    case 0x0a:
    case 0x0c:
    case 0x0e:
      return (vdp_hvc_r() >> 8) & 0xff;

    case 0x09: /* HV counter */
    case 0x0b:
    case 0x0d:
    case 0x0f:
      return (vdp_hvc_r() & 0xff);

    case 0x10: /* Unused (PSG) */
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			return z80_lockup_r (address);
		
		case 0x18: /* Unused */
		case 0x19:
		case 0x1a:
		case 0x1b:
			return z80_unused_r(address);
		
		case 0x1c: /* Unused (test register) */
		case 0x1d:
		case 0x1e:
		case 0x1f:
			return z80_unused_r(address);

		default:   /* Invalid VDP addresses */
			return z80_lockup_r(address);
	}
}

static inline void z80_vdp_w(unsigned int address, unsigned int data)
{
  switch (address & 0xff)
  {
    case 0x00: /* VDP data port */
    case 0x01: 
    case 0x02:
    case 0x03:
      vdp_data_w(data << 8 | data);
      return;

    case 0x04: /* VDP control port */
    case 0x05:
    case 0x06:
    case 0x07:
      vdp_ctrl_w(data << 8 | data);
      return;

    case 0x08: /* Unused (HV counter) */
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x0e:
    case 0x0f:
      z80_lockup_w(address, data);
      return;

    case 0x11: /* PSG */
    case 0x13:
    case 0x15:
    case 0x17:
      psg_write(1, data);
			return;

    case 0x10: /* Unused */
    case 0x12:
    case 0x14:
    case 0x16:
		case 0x18:
		case 0x19:
    case 0x1a:
    case 0x1b:
      z80_unused_w(address, data);
      return;

    case 0x1c: /* Test register */
    case 0x1d: 
    case 0x1e:
    case 0x1f:
      vdp_test_w(data << 8 | data);
      return;

    default: /* Invalid VDP addresses */
      z80_lockup_w(address, data);
      return;
  }
}

/*
    Z80 memory handlers
*/
unsigned int cpu_readmem16(unsigned int address)
{
  switch((address >> 13) & 7)
  {
    case 0: /* Work RAM */
    case 1:
      return zram[address & 0x1fff];

    case 2: /* YM2612 */
      return fm_read(1, address & 3);

    case 3: /* VDP */
      if ((address & 0xff00) == 0x7f00) return z80_vdp_r (address);
      return (z80_unused_r(address) | 0xff);

    default: /* V-bus bank */
      return z80_read_banked_memory(zbank | (address & 0x7fff));
  }
}


void cpu_writemem16(unsigned int address, unsigned int data)
{
  switch((address >> 13) & 7)
  {
    case 0: /* Work RAM */
    case 1: 
      zram[address & 0x1fff] = data;
      return;

    case 2: /* YM2612 */
      fm_write(1, address & 3, data);
      return;

    case 3: /* Bank register and VDP */
      switch(address & 0xff00)
      {
        case 0x6000:
          gen_bank_w(data & 1);
          return;

        case 0x7f00:
          z80_vdp_w(address, data);
          return;

        default:
          z80_unused_w(address, data);
          return;
      }
      return;

    default: /* V-bus bank */
      z80_write_banked_memory(zbank | (address & 0x7fff), data);
      return;
  }
}

/*
    Port handlers. Ports are unused when not in Mark III compatability mode.

    Games that access ports anyway:
    - Thunder Force IV reads port $BF in it's interrupt handler.
*/

unsigned int cpu_readport16(unsigned int port)
{
#if LOG_PORT
  error("Z80 read port %04X\n", port);
#endif    
  return 0xFF;
}

void cpu_writeport16(unsigned int port, unsigned int data)
{
#if LOG_PORT
  error("Z80 write %02X to port %04X\n", data, port);
#endif
}
