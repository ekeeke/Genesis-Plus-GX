/***************************************************************************************
 *  Genesis Plus
 *  Peripheral Input Support
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

t_input input;
int old_system[2] = {-1,-1};

/************************************************************************************/
/*                                                                                  */
/* H-counter values returned in H40 & H32 modes                                     */
/*                                                                                  */
/* Inside VDP, dot counter register is 9-bit, with only upper 8 bits being returned */
/*                                                                                  */
/* The number of dots per raster line is 342 in H32 mode and 420 in H40 mode        */
/*                                                                                  */
/************************************************************************************/
static const uint8 hc_320[210] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
  0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
                                            0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED,
  0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD,
  0xFE, 0xFF
};

static const uint8 hc_256[171] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x91, 0x92, 0x93,
                                                        0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/*****************************************************************************
 * LIGHTGUN support
 *
 *****************************************************************************/
static int x_offset;
static int y_offset;

static void lightgun_reset(int num)
{
  input.analog[num][0] = bitmap.viewport.w >> 1;
  input.analog[num][1] = bitmap.viewport.h >> 1;
}

static void lightgun_update(int num)
{
  /* update only one justifier at once */
  if (input.system[1] == SYSTEM_JUSTIFIER)
  {
    if ((io_reg[2] & 0x30) != (num << 5))
      return;
  }

  if ((input.analog[num][1] == v_counter + y_offset))
  {
    /* HL enabled ? */
    if (io_reg[5] & 0x80)
    {
      /* External Interrupt ? */
      if (reg[11] & 0x08) 
        irq_status = (irq_status & ~0x40) | 0x12;

      /* HV Counter Latch:
        1) some games does not enable HVC latch but instead use bigger X offset 
            --> we force the HV counter value read by the gun routine 
        2) for games using H40 mode, the gun routine scales up the Hcounter value
            --> H-Counter range is approx. 290 dot clocks
      */
      hvc_latch = 0x10000 | (vctab[v_counter] << 8);
      if (reg[12] & 1) 
        hvc_latch |= hc_320[((input.analog[num][0] * 290) / (2 * 320) + x_offset) % 210];
      else
        hvc_latch |= hc_256[(input.analog[num][0] / 2 + x_offset)%171];
    }
  }
}

/* Sega Menacer specific */
unsigned int menacer_read(void)
{
  /* pins should return 0 by default (fix Body Count when mouse is enabled) */
  unsigned int retval = 0x00;
  if (input.pad[4] & INPUT_B)     retval |= 0x01;
  if (input.pad[4] & INPUT_A)     retval |= 0x02;
  if (input.pad[4] & INPUT_C)     retval |= 0x04;
  if (input.pad[4] & INPUT_START) retval |= 0x08;

  return retval;
}

/* Konami Justifier specific */
unsigned int justifier_read(void)
{
  /* TL & TR pins should always return 1 (write only) */
  /* LEFT & RIGHT pins should always return 0 (needed during gun detection) */
  unsigned int retval = 0x73; 

  switch (io_reg[2])
  {
    case 0x40:  /* gun detection */
      return 0x30;

    case 0x00:  /* gun #1 enabled */
      if (input.pad[4] & INPUT_A)     retval &= ~0x01;
      if (input.pad[4] & INPUT_START) retval &= ~0x02;
      return retval;

    case 0x20:  /* gun #2 enabled */
      if (input.pad[5] & INPUT_A)     retval &= ~0x01;
      if (input.pad[5] & INPUT_START) retval &= ~0x02;
      return retval;

    default:  /* guns disabled */
      return retval;
  }
}

/*****************************************************************************
 * SEGA MOUSE support
 *
 *****************************************************************************/
static struct mega_mouse
{
  uint8 State;
  uint8 Counter;
  uint8 Wait;
  uint8 Port;
} mouse;

static inline void mouse_reset(void)
{
  mouse.State   = 0x60;
  mouse.Counter = 0;
  mouse.Wait = 0;
  mouse.Port = (input.system[0] == SYSTEM_MOUSE) ? 0 : 4;
}

void mouse_write(unsigned int data)
{
  if (mouse.Counter == 0)
  {
    /* TH 1->0 transition */
    if ((mouse.State&0x40) && !(data&0x40))
    {
      /* start acquisition */
      mouse.Counter = 1;
    }
  }
  else
  {
    /* TR transition */
    if ((mouse.State&0x20) != (data&0x20))
    {
      mouse.Counter ++; /* increment phase */
      mouse.Wait = 1;   /* mouse latency */

      if (mouse.Counter > 9) mouse.Counter = 9;
    }
  }

  /* end of acquisition (TH=1) */
  if (data&0x40) mouse.Counter = 0;

  /* update internal state */
  mouse.State = data;
}

unsigned int mouse_read()
{
  unsigned int temp = 0x00;

  switch (mouse.Counter)
  {
    case 0:     /* initial */
      temp = 0x00;
      break;

    case 1:     /* xxxx1011 */
      temp = 0x0B;
      break;

    case 2:     /* xxxx1111 */
      temp = 0x0F;
      break;

    case 3:     /* xxxx1111 */
      temp = 0x0F;
      break;

    case 4:   /* Axis sign and overflow */
      if (input.analog[2][0] < 0)         temp |= 0x01;
      if (input.analog[2][1] < 0)         temp |= 0x02;
      if (abs(input.analog[2][0]) > 255)  temp |= 0x04;
      if (abs(input.analog[2][1]) > 255)  temp |= 0x08;
      break;

    case 5:   /* Buttons state */
      if (input.pad[mouse.Port] & INPUT_A)     temp |= 0x01;
      if (input.pad[mouse.Port] & INPUT_C)     temp |= 0x02;
      if (input.pad[mouse.Port] & INPUT_B)     temp |= 0x04;
      if (input.pad[mouse.Port] & INPUT_START) temp |= 0x08;
      break;

    case 6:   /* X Axis MSB */
      temp = (input.analog[2][0] >> 4) & 0x0f;
      break;
      
    case 7:   /* X Axis LSB */
      temp = (input.analog[2][0] & 0x0f);
      break;

    case 8:   /* Y Axis MSB */
      temp = (input.analog[2][1] >> 4) & 0x0f;
      break;
      
    case 9:  /* Y Axis LSB */
      temp = (input.analog[2][1] & 0x0f);
      break;
  }

  /* TR-TL handshaking */
  if (mouse.Wait)
  {
    /* wait before ACK, fix some buggy mouse routine (Shangai 2, Wack World,...) */
    mouse.Wait = 0;

    /* TL = !TR */
    temp |= (~mouse.State & 0x20) >> 1;
  }
  else
  {
    /* TL = TR */
    temp |= (mouse.State & 0x20) >> 1;
  }

  return temp;
}


/*****************************************************************************
 * GAMEPAD support (2PLAYERS/4WAYPLAY) 
 *
 *****************************************************************************/
static struct pad
{
  uint8 State;
  uint8 Counter;
  uint8 Delay;
} gamepad[MAX_DEVICES];

static inline void gamepad_raz(int i)
{
  gamepad[i].Counter = 0;
  gamepad[i].Delay   = 0;
}

static inline void gamepad_reset(int i)
{
  gamepad[i].State = 0x00;
  if (input.dev[i] == DEVICE_6BUTTON) gamepad_raz(i);
}

static inline void gamepad_update(int i)
{
  if (gamepad[i].Delay++ > 25) gamepad_raz(i);
}

static inline unsigned int gamepad_read(int i)
{
  /* bit7 is latched */
  unsigned int retval = 0x7F;

  /* pad status */
  unsigned int pad = input.pad[i];

  /* current TH state */
  unsigned int control = (gamepad[i].State & 0x40) >> 6;

  /* TH transitions counter */
  if (input.dev[i] == DEVICE_6BUTTON)
    control += (gamepad[i].Counter & 3) << 1;

  switch (control)
  {
    case 1: /*** First High  ***/
    case 3: /*** Second High ***/
    case 5: /*** Third High  ***/

      /* TH = 1 : ?1CBRLDU */
      retval &= ~(pad & 0x3F);
      break;

    case 0: /*** First low  ***/
    case 2: /*** Second low ***/

      /* TH = 0 : ?0SA00DU */
      retval &= ~(pad & 0x03);
      retval &= ~((pad >> 2) & 0x30);
      retval &= ~0x0C;
      break;

    /* 6buttons specific (taken from gen-hw.txt) */
    /* A 6-button gamepad allows the extra buttons to be read based on how */
      /* many times TH is switched from 1 to 0 (and not 0 to 1). Observe the */
      /* following sequence */
      /*
       TH = 1 : ?1CBRLDU    3-button pad return value
       TH = 0 : ?0SA00DU    3-button pad return value
       TH = 1 : ?1CBRLDU    3-button pad return value
       TH = 0 : ?0SA0000    D3-0 are forced to '0'
       TH = 1 : ?1CBMXYZ    Extra buttons returned in D3-0
       TH = 0 : ?0SA1111    D3-0 are forced to '1'
    */
    case 4: /*** Third Low ***/

      /* TH = 0 : ?0SA0000    D3-0 are forced to '0'*/
      retval &= ~((pad >> 2) & 0x30);
      retval &= ~0x0F;
      break;

    case 6: /*** Fourth Low ***/

      /* TH = 0 : ?0SA1111    D3-0 are forced to '1'*/
      retval &= ~((pad >> 2) & 0x30);
      break;

    case 7: /*** Fourth High ***/

      /* TH = 1 : ?1CBMXYZ    Extra buttons returned in D3-0*/
      retval &= ~(pad & 0x30);
      retval &= ~((pad >> 8) & 0x0F);
      break;

    default:
      break;
  }

  return retval;
}

static inline void gamepad_write(int i, unsigned int data)
{
  if (input.dev[i] == DEVICE_6BUTTON)
  {
    /* TH=0 to TH=1 transition */
    if (!(gamepad[i].State & 0x40) && (data & 0x40))
    {
      gamepad[i].Counter++;
      gamepad[i].Delay = 0;
    }
  }
  gamepad[i].State = data;
}


/*****************************************************************************
 * TEAMPLAYER adapter support
 *
 *****************************************************************************/
static struct teamplayer
{
  uint8 State;
  uint8 Counter;
  uint8 Table[12];
} teamplayer[2];

static inline void teamplayer_init(int port)
{
  int i,padnum;
  int index = 0;

  /* this table determines which gamepad input should be returned during acquisition sequence
     index  = teamplayer read table index: 0=1st read, 1=2nd read, ...
     table  = high bits are pad index, low bits are pad input shift: 0=RLDU, 4=SABC, 8=MXYZ
  */  
  for (i=0; i<4; i++)
  {
    padnum = (4 * port) + i;
    if (input.dev[padnum] == DEVICE_3BUTTON)
    {
      padnum = padnum << 4;
      teamplayer[port].Table[index++] = padnum;
      teamplayer[port].Table[index++] = padnum | 4;
    }
    else if (input.dev[(4*port) + i] == DEVICE_6BUTTON)
    {
      padnum = padnum << 4;
      teamplayer[port].Table[index++] = padnum;
      teamplayer[port].Table[index++] = padnum | 4;
      teamplayer[port].Table[index++] = padnum | 8;
    }
  }
}

static inline void teamplayer_reset(int port)
{
  teamplayer[port].State = 0x60; /* TH = 1, TR = 1 */
  teamplayer[port].Counter = 0;
}

static inline unsigned int teamplayer_read(int port)
{
  unsigned int counter = teamplayer[port].Counter;

  /* acquisition sequence */
  switch (counter)
  {
    case 0: /* initial state: TH = 1, TR = 1 -> RLDU = 0011 */
    {
      return 0x73;
    }

    case 1: /* start request: TH = 0, TR = 1 -> RLDU = 1111 */
    {
      return 0x3F; 
    }

    case 2:
    case 3: /* ack request: TH=0, TR=0/1 -> RLDU = 0000 */
    {
      /* TL should match TR */
      return ((teamplayer[port].State & 0x20) >> 1);
    }

    case 4:
    case 5:
    case 6:
    case 7: /* PAD type */
    {
      unsigned int retval = input.dev[(port << 2) + (counter - 4)];

      /* TL should match TR */
      return (((teamplayer[port].State & 0x20) >> 1) | retval);
    }

    default: /* PAD status */
    {
      unsigned int retval = 0x0F;

      /* SEGA teamplayer returns successively PAD1 -> PAD2 -> PAD3 -> PAD4 inputs */
      unsigned int padnum = teamplayer[port].Table[counter - 8] >> 4;

      /* Each PAD inputs is obtained through 2 or 3 sequential reads: RLDU -> SACB -> MXYZ */
      retval &= ~(input.pad[padnum] >> (teamplayer[port].Table[counter - 8] & 0x0F));

      /* TL should match TR */
      return (((teamplayer[port].State & 0x20) >> 1) | retval);
    }
  }
}

static inline void teamplayer_write(int port, unsigned int data)
{
  /* update output bits only */
  unsigned int state = (teamplayer[port].State & ~io_reg[port + 4]) | (data & io_reg[port + 4]);

  /* TH & TR handshaking */
  if ((teamplayer[port].State ^ state) & 0x60)
  {
    if (state & 0x40) 
    {
      /* TH high -> reset counter */
      teamplayer[port].Counter = 0;
    }
    else
    {
      /* increment counter */
      teamplayer[port].Counter ++;
    }

    /* update internal state */
    teamplayer[port].State = state;
  }
}

/*****************************************************************************
 * 4-WAYPLAY adapter support
 *
 *****************************************************************************/
static struct wayplay
{
  uint8 current;
} wayplay;

static inline void wayplay_write(int port, unsigned int data)
{
  if (!port && (io_reg[4] & 0x40)) gamepad_write(wayplay.current, data);
  else wayplay.current = (data >> 4) & 0x07;
}

static inline unsigned int wayplay_read(int port)
{
  if (port) return 0x7F;
  if (wayplay.current >= 4) return 0x70; /* multitap detection (TH2 = 1) */
  return gamepad_read(wayplay.current);  /* 0x0C = Pad1, 0x1C = Pad2, ... */
}


/*****************************************************************************
 * I/O wrappers
 *
 *****************************************************************************/
unsigned int gamepad_1_read (void)
{
  return gamepad_read(0);
}

unsigned int gamepad_2_read (void)
{
  return gamepad_read(4);
}

void gamepad_1_write (unsigned int data)
{
  if (io_reg[4] & 0x40) gamepad_write(0, data);
}

void gamepad_2_write (unsigned int data)
{
  if (io_reg[5] & 0x40) gamepad_write(4, data);
}

unsigned int wayplay_1_read (void)
{
  return wayplay_read(0);
}

unsigned int wayplay_2_read (void)
{
  return wayplay_read(1);
}

void wayplay_1_write (unsigned int data)
{
  wayplay_write(0, data);
}

void wayplay_2_write (unsigned int data)
{
  wayplay_write(1, data);
}

unsigned int teamplayer_1_read (void)
{
  return teamplayer_read(0);
}

unsigned int teamplayer_2_read (void)
{
  return teamplayer_read(1);
}

void teamplayer_1_write (unsigned int data)
{
  teamplayer_write(0, data);
}

void teamplayer_2_write (unsigned int data)
{
  teamplayer_write(1, data);
}

unsigned int jcart_read(unsigned int address)
{
   /* TH2 (output) fixed to 0 on read (fixes Micro Machines 2) */
   return ((gamepad[5].State & 0x40) | (gamepad_read(5) & 0x3f) | ((gamepad_read(6) & 0x3f) << 8));
}

void jcart_write(unsigned int address, unsigned int data)
{
  gamepad_write(5, (data & 1) << 6);
  gamepad_write(6, (data & 1) << 6);
  return;
}

/*****************************************************************************
 * Generic INPUTS Control
 *
 *****************************************************************************/
void input_init(void)
{
  int i,j;
  int player = 0;

  for (i=0; i<MAX_DEVICES; i++)
  {
    input.dev[i] = NO_DEVICE;
    input.pad[i] = 0;
  }

  switch (input.system[0])
  {
    case SYSTEM_GAMEPAD:
      if (player == MAX_INPUTS) return;
      input.dev[0] = config.input[player].padtype;
      player ++;
      break;

    case SYSTEM_MOUSE:
      if (player == MAX_INPUTS) return;
      input.dev[0] = DEVICE_MOUSE;
      player ++;
      break;

    case SYSTEM_WAYPLAY:
      for (j=0; j< 4; j++)
      {
        if (player == MAX_INPUTS) return;
        input.dev[j] = config.input[player].padtype;
        player ++;
      }
      break;

    case SYSTEM_TEAMPLAYER:
      for (j=0; j<4; j++)
      {
        if (player == MAX_INPUTS) return;
        input.dev[j] = config.input[player].padtype;
        player ++;
      }
      teamplayer_init(0);
      break;
  }

  switch (input.system[1])
  {
    case SYSTEM_GAMEPAD:
      if (player == MAX_INPUTS) return;
      input.dev[4] = config.input[player].padtype;
      player ++;
      break;

    case SYSTEM_MOUSE:
      if (player == MAX_INPUTS) return;
      input.dev[4] = DEVICE_MOUSE;
      player ++;
      break;

    case SYSTEM_MENACER:
      if (player == MAX_INPUTS) return;
      input.dev[4] = DEVICE_LIGHTGUN;
      player ++;
      break;

    case SYSTEM_JUSTIFIER:
      for (j=4; j<6; j++)
      {
        if (player == MAX_INPUTS) return;
        input.dev[j] = DEVICE_LIGHTGUN;
        player ++;
      }
      break;

     case SYSTEM_TEAMPLAYER:
      for (j=4; j<8; j++)
      {
        if (player == MAX_INPUTS) return;
        input.dev[j] = config.input[player].padtype;
        player ++;
      }
      teamplayer_init(1);
      break;
  }

  /* J-CART: add two gamepad inputs */
  if (cart.jcart)
  {
    if (player == MAX_INPUTS) return;
    input.dev[5] = config.input[player].padtype;
    player ++;
    if (player == MAX_INPUTS) return;
    input.dev[6] = config.input[player].padtype;
    player ++;
  }
}

void input_reset(void)
{
  /* Reset Controller device */
  int i;
  for (i=0; i<MAX_DEVICES; i++)
  {
    switch (input.dev[i])
    {
      case DEVICE_3BUTTON:
      case DEVICE_6BUTTON:
        gamepad_reset(i);
        break;

      case DEVICE_LIGHTGUN:
        lightgun_reset(i%2);
        break;

      case DEVICE_MOUSE:
        mouse_reset();

      default:
        break;
    }
  }

  /* Team Player */
  if (input.system[0] == SYSTEM_TEAMPLAYER)
    teamplayer_reset(0);
  if (input.system[1] == SYSTEM_TEAMPLAYER)
    teamplayer_reset(1);

  /* 4-Way Play */
  wayplay.current = 0;
}

void input_refresh(void)
{
  int i;
  for (i=0; i<MAX_DEVICES; i++)
  {
    switch (input.dev[i])
    {
      case DEVICE_6BUTTON:
      {
        gamepad_update(i);
        break;
      }

      case DEVICE_LIGHTGUN:
      {
        lightgun_update(i%2);
        break;
      }
    }
  }
}

void input_autodetect(void)
{
  /* restore previous settings */
  if (old_system[0] != -1)
    input.system[0] = old_system[0];
  if (old_system[1] != -1)
    input.system[1] = old_system[1];

  /* initialize default GUN settings */
  x_offset = 0x00;
  y_offset = 0x00;

  /**********************************************
          SEGA MENACER 
  ***********************************************/
  if (strstr(rominfo.international,"MENACER") != NULL)
  {
    /* save current setting */
    if (old_system[0] == -1)
      old_system[0] = input.system[0];
    if (old_system[1] == -1)
      old_system[1] = input.system[1];

    input.system[0] = NO_SYSTEM;
    input.system[1] = SYSTEM_MENACER;
    x_offset = 0x52;
    y_offset = 0x00;
  }
  else if (strstr(rominfo.international,"T2 ; THE ARCADE GAME") != NULL)
  {
    /* save current setting */
    if (old_system[0] == -1)
      old_system[0] = input.system[0];
    if (old_system[1] == -1)
      old_system[1] = input.system[1];

    input.system[0] = SYSTEM_GAMEPAD;
    input.system[1] = SYSTEM_MENACER;
    x_offset = 0x84;
    y_offset = 0x08;
  }
  else if (strstr(rominfo.international,"BODY COUNT") != NULL)
  {
    /* save current setting */
    if (old_system[0] == -1)
      old_system[0] = input.system[0];
    if (old_system[1] == -1)
      old_system[1] = input.system[1];

    input.system[0] = SYSTEM_MOUSE;
    input.system[1] = SYSTEM_MENACER;
    x_offset = 0x44;
    y_offset = 0x18;
  }

  /**********************************************
          KONAMI JUSTIFIER 
  ***********************************************/
  else if (strstr(rominfo.international,"LETHAL ENFORCERSII") != NULL)
  {
    /* save current setting */
    if (old_system[0] == -1)
      old_system[0] = input.system[0];
    if (old_system[1] == -1)
      old_system[1] = input.system[1];

    input.system[0] = SYSTEM_GAMEPAD;
    input.system[1] = SYSTEM_JUSTIFIER;
    x_offset = 0x18;
    y_offset = 0x00;
  }
  else if (strstr(rominfo.international,"LETHAL ENFORCERS") != NULL)
  {
    /* save current setting */
    if (old_system[0] == -1)
      old_system[0] = input.system[0];
    if (old_system[1] == -1)
      old_system[1] = input.system[1];

    input.system[0] = SYSTEM_GAMEPAD;
    input.system[1] = SYSTEM_JUSTIFIER;
    x_offset = 0x00;
    y_offset = 0x00;
  }

  /**********************************************
          J-CART 
  ***********************************************/
  cart.jcart = 0;
  if (((strstr(rominfo.product,"00000000")  != NULL) && (rominfo.checksum == 0x168b)) ||  /* Super Skidmarks, Micro Machines Military*/
      ((strstr(rominfo.product,"00000000")  != NULL) && (rominfo.checksum == 0x165e)) ||  /* Pete Sampras Tennis (1991), Micro Machines 96 */
      ((strstr(rominfo.product,"00000000")  != NULL) && (rominfo.checksum == 0xcee0)) ||  /* Micro Machines Military (bad) */
      ((strstr(rominfo.product,"00000000")  != NULL) && (rominfo.checksum == 0x2c41)) ||  /* Micro Machines 96 (bad) */
      ((strstr(rominfo.product,"XXXXXXXX")  != NULL) && (rominfo.checksum == 0xdf39)) ||  /* Sampras Tennis 96 */
      ((strstr(rominfo.product,"T-123456")  != NULL) && (rominfo.checksum == 0x1eae)) ||  /* Sampras Tennis 96 */
      ((strstr(rominfo.product,"T-120066")  != NULL) && (rominfo.checksum == 0x16a4)) ||  /* Pete Sampras Tennis (1994)*/
      (strstr(rominfo.product,"T-120096")    != NULL))                                    /* Micro Machines 2 */
  {
    if (cart.romsize <= 0x380000)  /* just to be sure (checksum might not be enough) */
    {
      cart.jcart = 1;

      /* set default port 1 setting */
      if (input.system[1] != SYSTEM_WAYPLAY)
      {
        old_system[1] = input.system[1];
        input.system[1] = SYSTEM_GAMEPAD;
      }
    }
  }
}
