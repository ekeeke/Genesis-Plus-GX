/*
    Copyright (C) 1999, 2000, 2001, 2002, 2003  Charles MacDonald

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
	02/2007: fixed 6BUTTONS gamepad emulation (Eke-Eke)
	06/2007: Added TEAMPLAYER/4WAYPLAY/MENACER emulation (Eke-Eke)
*/

#include "shared.h"

t_input input;
uint8 current_pad = 0;
uint8 j_cart = 0;

void gamepad_reset(uint8 num);
void gamepad_update(uint8 num);
void gamepad_raz(uint8 num);
void lightgun_update(void);
void lightgun_reset(void);
void teamplayer_reset(uint8 port);

/*****************************************************************************
 * Generic INPUTS Control
 *
 *****************************************************************************/
void input_reset (int padtype)
{
	uint8 i;
	
	input.max = 0;
	for (i=0; i<MAX_DEVICES; i++)
	{
		input.dev[i] = NO_DEVICE;
		input.pad[i] = 0;
	}

	switch (input.system[0])
	{
		case SYSTEM_GAMEPAD:
			if (input.max++ < MAX_INPUTS) input.dev[0] = padtype;
			gamepad_reset(0);
			break;
		
		case SYSTEM_WAYPLAY:
			for (i=0; i<4; i++)
			{
				if (input.max++ < MAX_INPUTS) input.dev[i] = padtype;
				gamepad_reset(i);
			}
			current_pad = 0;
			break;

		case SYSTEM_TEAMPLAYER:
			for (i=0; i<4; i++)
			{
				if (input.max++ < MAX_INPUTS) input.dev[i] = padtype;
			}
			teamplayer_reset(0);
			break;
	}
	
	switch (input.system[1])
	{
		case SYSTEM_GAMEPAD:
			if (input.max++ < MAX_INPUTS) input.dev[4] = padtype;
			gamepad_reset(4);
			break;
		
		case SYSTEM_TEAMPLAYER:
			for (i=4; i<8; i++)
			{
				if (input.max++ < MAX_INPUTS) input.dev[i] = padtype;
			}
			teamplayer_reset(1);
			break;

		case SYSTEM_MENACER:
			if (input.max++ < MAX_INPUTS) input.dev[4] = DEVICE_LIGHTGUN;
			lightgun_reset();
			break;
	}

	if (input.max > MAX_INPUTS) input.max = MAX_INPUTS;

	/* J-CART: add two gamepad inputs */
	if (j_cart)
	{
		input.dev[5] = padtype;
		input.dev[6] = padtype;
		gamepad_reset(5);
		gamepad_reset(6);
		input.max+=2;
	}
}

void input_update()
{
	uint8 i;
	switch (input.system[0])
	{
		case SYSTEM_GAMEPAD:
			if (input.dev[0] == DEVICE_6BUTTON) gamepad_update(0);
			break;
		
		case SYSTEM_WAYPLAY:
			for (i=0; i<4; i++)
			{
				if (input.dev[i] == DEVICE_6BUTTON) gamepad_update(i);
			}
			break;
	}
	
	switch (input.system[1])
	{
		case SYSTEM_GAMEPAD:
			if (input.dev[4] == DEVICE_6BUTTON) gamepad_update(4);
			break;
		
		case SYSTEM_MENACER:
			lightgun_update();
			break;
	}
}

void input_raz()
{
	uint8 i;
	switch (input.system[0])
	{
		case SYSTEM_GAMEPAD:
			if (input.dev[0] == DEVICE_6BUTTON) gamepad_raz(0);
			break;
		
		case SYSTEM_WAYPLAY:
			for (i=0; i<4; i++)
			{
				if (input.dev[i] == DEVICE_6BUTTON) gamepad_raz(i);
			}
			break;
	}
	
	switch (input.system[1])
	{
		case SYSTEM_GAMEPAD:
			if (input.dev[4] == DEVICE_6BUTTON) gamepad_raz(4);
			break;
	}
}

/*****************************************************************************
 * LIGHTGUN specific functions
 *
 *****************************************************************************/
struct gun
{
  int16 x;
  int16 y;
} lightgun;

void lightgun_reset(void)
{
	lightgun.x = bitmap.viewport.w >> 1;
	lightgun.y = bitmap.viewport.h >> 1;
}

void lightgun_update(void)
{
	if ((v_counter == lightgun.y))
	{
		if (reg[0x0B] & 8) m68k_set_irq(2);
		if (reg[0x00] & 2)
		{
			hc_latch = lightgun.x + 166;
			if (hc_latch >= bitmap.viewport.w + 52) hc_latch -= (bitmap.viewport.w + 52 + 48);
			hc_latch >>= 1;
		}			
	}
}			

void lightgun_set ()
{
	if ((input.pad[4] & INPUT_RIGHT)) lightgun.x ++;
	if ((input.pad[4] & INPUT_LEFT))  lightgun.x --;
    if ((input.pad[4] & INPUT_UP))    lightgun.y --;
	if ((input.pad[4] & INPUT_DOWN))  lightgun.y ++;
	
	if (lightgun.x < 0) lightgun.x = 0;
	else if (lightgun.x > bitmap.viewport.w-34) lightgun.x = bitmap.viewport.w-34;
	
	if (lightgun.y < 0) lightgun.y = 0;
	else if (lightgun.y > bitmap.viewport.h-8) lightgun.y = bitmap.viewport.h-8;
}	

uint8 lightgun_read (void)
{
	uint8 retval = 0x00;
	if ((input.pad[4] & INPUT_B))     retval |= 0x01;
	if ((input.pad[4] & INPUT_A))     retval |= 0x02;
    if ((input.pad[4] & INPUT_C))     retval |= 0x04;
	if ((input.pad[4] & INPUT_START)) retval |= 0x08;
	return (retval & 0x7F);
}


/*****************************************************************************
 * GAMEPAD specific functions (2PLAYERS/4WAYPLAY) 
 *
 *****************************************************************************/
struct pad
{
  uint8 State;
  uint8 Counter;
  uint8 Delay;
} gamepad[MAX_DEVICES];

void gamepad_reset(uint8 i)
{
	gamepad[i].State = 0x40;
	if (input.dev[i] == DEVICE_6BUTTON) gamepad_raz(i);
}

void gamepad_raz(uint8 i)
{
	gamepad[i].Counter = 0;
	gamepad[i].Delay   = 0;
}

void gamepad_update(uint8 i)
{
	if (gamepad[i].Delay++ > 25) gamepad_raz(i);
}

uint8 gamepad_read (uint8 i)
{
  int control;
  unsigned char retval = 0xFF;

  if (input.dev[i] == NO_DEVICE) return 0x7F;
 
  control = (gamepad[i].State & 0x40) >> 6; /* current TH state */
 
  if (input.dev[i] == DEVICE_6BUTTON)
  {
	  control += (gamepad[i].Counter & 3) << 1; /* TH transitions counter */
  }

  switch (control)
  {
    case 1: /*** First High  ***/
    case 3: /*** Second High ***/
    case 5: /*** Third High  ***/
    
		/* TH = 1 : ?1CBRLDU */
		if (input.pad[i] & INPUT_C)     retval &= ~0x20;
		if (input.pad[i] & INPUT_B)     retval &= ~0x10;
		if (input.pad[i] & INPUT_UP)    retval &= ~0x01;
		if (input.pad[i] & INPUT_DOWN)  retval &= ~0x02;
		if (input.pad[i] & INPUT_LEFT)  retval &= ~0x04;
		if (input.pad[i] & INPUT_RIGHT) retval &= ~0x08;
	    break;

    case 0: /*** First low  ***/
    case 2: /*** Second low ***/
    
		/* TH = 0 : ?0SA00DU */
		if (input.pad[i] & INPUT_A)     retval &= ~0x10;
		if (input.pad[i] & INPUT_START) retval &= ~0x20;
		if (input.pad[i] & INPUT_UP)    retval &= ~0x01;
		if (input.pad[i] & INPUT_DOWN)  retval &= ~0x02;
        retval &= 0xB3; 
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
		if (input.pad[i] & INPUT_A)     retval &= ~0x10;
		if (input.pad[i] & INPUT_START) retval &= ~0x20;
		retval &= 0xB0;
        break;

    case 6: /*** Fourth Low ***/
    
		/* TH = 0 : ?0SA1111    D3-0 are forced to '1'*/
		if (input.pad[i] & INPUT_A)     retval &= ~0x10;
        if (input.pad[i] & INPUT_START) retval &= ~0x20;
		retval &= 0xBF;
		break;

    case 7: /*** Fourth High ***/
    
		/* TH = 1 : ?1CBMXYZ    Extra buttons returned in D3-0*/
		if (input.pad[i] & INPUT_X)    retval &= ~0x04;
        if (input.pad[i] & INPUT_Y)    retval &= ~0x02;
        if (input.pad[i] & INPUT_Z)    retval &= ~0x01;
        if (input.pad[i] & INPUT_B)    retval &= ~0x10;
    	if (input.pad[i] & INPUT_C)    retval &= ~0x20;
     	if (input.pad[i] & INPUT_MODE) retval &= ~0x08;
		break;

    default:
        break;
  }

  /* bit7 is latched*/
  return (retval&0x7f);
}

void gamepad_write (uint8 i, uint8 data)
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
 * TEAMPLAYER specific functions
 *
 *****************************************************************************/
struct teamplayer
{
  uint8 State;
  uint8 Counter;
  uint8 Table[12];
} teamplayer[2];

void teamplayer_reset(uint8 port)
{
	uint8 i;
	uint8 index = 0;
	uint8 pad_input = 0;

	teamplayer[port].State = 0x60; /* TH = 1, TR = 1 */
	teamplayer[port].Counter = 0;

	/* this table determines which gamepad input should be returned during acquisition sequence
	   index     = teamplayer read table index: 0=1st read, 1=2nd read, ...
	   pad_input = gamepad input 0-14: 0=P1_DIR, 1=P1_SABC, 2=P1_MXYZ, 4=P2_DIR, 5=P2_SABC, ...
	*/	
	for (i=0; i<4; i++)
	{
		if (input.dev[(4*port) + i] == DEVICE_3BUTTON)
		{
			teamplayer[port].Table[index++] = pad_input;
			teamplayer[port].Table[index++] = pad_input + 1;
		}
		else if (input.dev[(4*port) + i] == DEVICE_6BUTTON)
		{
			teamplayer[port].Table[index++] = pad_input;
			teamplayer[port].Table[index++] = pad_input + 1;
			teamplayer[port].Table[index++] = pad_input + 2;
		}
		pad_input += 4;
	}
}


/* SEGA teamplayer returns successively:
     - PAD1 inputs
	 - PAD2 inputs
	 - PAD3 inputs
	 - PAD4 inputs

   Each PAD inputs is obtained through 2 or 3 sequential reads:
     1/ DIR buttons
	 2/ START,A,C,B buttons
	 3/ MODE, X,Y,Z buttons (6Button only !)
*/
uint8 teamplayer_read(uint8 port, uint8 index)
{
	uint8 retval = 0x7F;
	uint8 pad_input = teamplayer[port].Table[index] & 0x03;
	uint8 pad_num = (4 * port) + ((teamplayer[port].Table[index] >> 2) & 0x03);
	
	switch (pad_input)
	{
		case 0:
		   /* Directions Buttons */
		   if (input.pad[pad_num] & INPUT_UP)    retval &= ~0x01;
		   if (input.pad[pad_num] & INPUT_DOWN)  retval &= ~0x02;
		   if (input.pad[pad_num] & INPUT_LEFT)  retval &= ~0x04;
		   if (input.pad[pad_num] & INPUT_RIGHT) retval &= ~0x08;
		   break;
		
		case 1:
		   /* S,A,C,B Buttons */
		   if (input.pad[pad_num] & INPUT_B)     retval &= ~0x01;
		   if (input.pad[pad_num] & INPUT_C)     retval &= ~0x02;
		   if (input.pad[pad_num] & INPUT_A)     retval &= ~0x04;
		   if (input.pad[pad_num] & INPUT_START) retval &= ~0x08;
		   break;

		case 2:
		   /* M,X,Y,Z Buttons (6-Buttons only)*/
		   if (input.pad[pad_num] & INPUT_Z)    retval &= ~0x01;
		   if (input.pad[pad_num] & INPUT_Y)    retval &= ~0x02;
		   if (input.pad[pad_num] & INPUT_X)    retval &= ~0x04;
		   if (input.pad[pad_num] & INPUT_MODE) retval &= ~0x08;
		   break;
	}

	return retval; 
}

/*****************************************************************************
 * MULTITAP generic functions (TEAMPLAYER/4WAYPLAY)
 *
 *****************************************************************************/
void multitap_write (uint8 port, uint8 data)
{
	uint8 old_state;
	switch (input.system[port])
	{	
		case SYSTEM_WAYPLAY:
			if (!port) gamepad_write(current_pad, data);
			else current_pad = (data >> 4) & 0x07;
			break;

		case SYSTEM_TEAMPLAYER:
			old_state = teamplayer[port].State;
			teamplayer[port].State = (data & io_reg[port+4]) | (teamplayer[port].State & ~io_reg[port+4]);
			if (old_state != teamplayer[port].State) teamplayer[port].Counter ++;
			if ((data&0x60) == 0x60) teamplayer[port].Counter = 0;
			break;
	}
}

uint8 multitap_read (uint8 port)
{
	uint8 retval = 0x7F;
	uint8 padnum;

	switch (input.system[port])
	{	
		case SYSTEM_WAYPLAY:
			if (port == 1) return 0x7F;
			if (current_pad >= 4) return 0x70; /* multitap detection (TH2 = 1) */
			return gamepad_read(current_pad);  /* 0x0C = Pad1, 0x1C = Pad2, ... */

		case SYSTEM_TEAMPLAYER:
			switch (teamplayer[port].Counter) /* acquisition sequence steps */
		    {
			   case 0: /* initial state: TH = 1, TR = 1 */
				   retval = 0x73; 
				   break;

			   case 1: /* start request: TH = 0, TR = 1 */
				   retval = 0x3F; 
				   break;

			   case 2:
			   case 3: /* ack request: TH=0, TR handshake */
				   retval = 0x00;
				   break;
			   
			   case 4:
			   case 5:
			   case 6:
			   case 7: /* gamepads type */
				   padnum = (4 * port) + teamplayer[port].Counter - 4;
				   retval = input.dev[padnum];
				   break;
					   
			   default: /* gamepads inputs acquisition */
				   retval = teamplayer_read(port, teamplayer[port].Counter - 8);
				   break;
		    }
	  
		    /* TL must match TR state */
		    retval &= ~0x10;
		    if (teamplayer[port].State & 0x20) retval |= 0x10;
			return retval;
	}
	return retval;
}
