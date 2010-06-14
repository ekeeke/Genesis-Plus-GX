/***************************************************************************************
 *  Genesis Plus
 *  I/O Chip
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

uint8 io_reg[0x10];
uint8 region_code = REGION_USA;

static struct port_t
{
  void (*data_w)(uint32 data);
  uint32 (*data_r)(void);
} port[3];

/*****************************************************************************
 * I/O chip functions                                                        *
 *                                                                           *
 *****************************************************************************/
void io_init(void)
{
  /* Initialize IO Ports handlers */
  switch (input.system[0])
  {
    case SYSTEM_GAMEPAD:
      port[0].data_w = gamepad_1_write;
      port[0].data_r = gamepad_1_read;
      break;

    case SYSTEM_MOUSE:
      port[0].data_w = mouse_write;
      port[0].data_r = mouse_read;
      break;

    case SYSTEM_WAYPLAY:
      port[0].data_w = wayplay_1_write;  
      port[0].data_r = wayplay_1_read;
      break;

    case SYSTEM_TEAMPLAYER:
      port[0].data_w = teamplayer_1_write;  
      port[0].data_r = teamplayer_1_read;
      break;

    default:
      port[0].data_w = NULL;
      port[0].data_r = NULL;
      break;
  }

  switch (input.system[1])
  {
    case SYSTEM_GAMEPAD:
      port[1].data_w = gamepad_2_write;
      port[1].data_r = gamepad_2_read;
      break;

    case SYSTEM_MOUSE:
      port[1].data_w = mouse_write;
      port[1].data_r = mouse_read;
      break;

    case SYSTEM_MENACER:
      port[1].data_w = NULL;
      port[1].data_r = menacer_read;
      break;

    case SYSTEM_JUSTIFIER:
      port[1].data_w = NULL;
      port[1].data_r = justifier_read;
      break;

    case SYSTEM_WAYPLAY:
      port[1].data_w = wayplay_2_write;  
      port[1].data_r = wayplay_2_read;
      break;

    case SYSTEM_TEAMPLAYER:
      port[1].data_w = teamplayer_2_write;  
      port[1].data_r = teamplayer_2_read;
      break;

    default:
      port[1].data_w = NULL;
      port[1].data_r = NULL;
      break;
  }

  /* External Port (unconnected) */
  port[2].data_w = NULL;
  port[2].data_r = NULL;

  /* Initialize connected input devices */
  input_init();
}


void io_reset(void)
{
  /* Reset I/O registers */
  io_reg[0x00] = region_code | 0x20 | (config.tmss & 1);
  io_reg[0x01] = 0x7F;
  io_reg[0x02] = 0x7F;
  io_reg[0x03] = 0x7F;
  io_reg[0x04] = 0x00;
  io_reg[0x05] = 0x00;
  io_reg[0x06] = 0x00;
  io_reg[0x07] = 0xFF;
  io_reg[0x08] = 0x00;
  io_reg[0x09] = 0x00;
  io_reg[0x0A] = 0xFF;
  io_reg[0x0B] = 0x00;
  io_reg[0x0C] = 0x00;
  io_reg[0x0D] = 0xFB;
  io_reg[0x0E] = 0x00;
  io_reg[0x0F] = 0x00;
  
  /* Reset connected input devices */
  input_reset();
}

void io_write(uint32 offset, uint32 value)
{
  switch (offset)
  {
    case 0x01: /* Port A Data */
    case 0x02: /* Port B Data */
    case 0x03: /* Port C Data */
      io_reg[offset] = value & (0x80 | io_reg[offset+3]);
      if(port[offset-1].data_w)
        port[offset-1].data_w(value);
      return;

    case 0x04:      /* Port A Ctrl */
    case 0x05:      /* Port B Ctrl */
    case 0x06:      /* Port C Ctrl */
      io_reg[offset] = value;
      io_reg[offset-3] &= (0x80 | value);
      return;

    case 0x07:      /* Port A TxData */
    case 0x0A:      /* Port B TxData */
    case 0x0D:      /* Port C TxData */
      io_reg[offset] = value;
      return;

    case 0x09:      /* Port A S-Ctrl */
    case 0x0C:      /* Port B S-Ctrl */
    case 0x0F:      /* Port C S-Ctrl */
      io_reg[offset] = value & 0xF8;
      return;
    
    default:        /* Read-only ports */
      return;
  }
}

uint32 io_read(uint32 offset)
{
  switch(offset)
  {
    case 0x01: /* Port A Data */
    case 0x02: /* Port B Data */
    case 0x03: /* Port C Data */
    {
      uint8 input = 0x7F;
      if(port[offset-1].data_r)
        input = port[offset-1].data_r();
      return (io_reg[offset] | ((~io_reg[offset+3]) & input));
    }

    default: /* return register value */
      return (io_reg[offset]);
  }
}
