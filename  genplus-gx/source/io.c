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

	
    I/O controller chip emulation
*/

#include "shared.h"

uint8 io_reg[0x10];
uint8 region_code = REGION_USA;
uint8 pad_type = DEVICE_3BUTTON;

/*****************************************************************************
 * IO Read/Write wrappers
 *
 *****************************************************************************/

unsigned char Player1_Read (void)
{
	return gamepad_read(0);
}

unsigned char Player2_Read (void)
{
	return gamepad_read(4);
}

void Player1_Write (unsigned char data)
{
	gamepad_write(0, data);
}

void Player2_Write (unsigned char data)
{
	gamepad_write(4, data);
}

unsigned char MultiTap1_Read (void)
{
	return multitap_read(0);
}

unsigned char MultiTap2_Read (void)
{
	return multitap_read(1);
}

void MultiTap1_Write (unsigned char data)
{
	multitap_write(0, data);
}

void MultiTap2_Write (unsigned char data)
{
	multitap_write(1, data);
}

/*****************************************************************************
 * I/O chip functions                                                        *
 *																			 *
 *****************************************************************************
*/
struct port_t
{
    void (*data_w)(uint8 data);
    uint8 (*data_r)(void);
} port[3];

void io_reset(void)
{
    /* I/O register default settings */
    uint8 io_def[0x10] =
    {
	    0xA0,
	    0x7F, 0x7F, 0x7F,
	    0x00, 0x00, 0x00,
	    0xFF, 0x00, 0x00,
	    0xFF, 0x00, 0x00,
	    0xFB, 0x00, 0x00,
    };

    /* Initialize I/O registers */
    memcpy (io_reg, io_def, 0x10);

    switch (input.system[0])
    {
		case SYSTEM_GAMEPAD:
			port[0].data_w = Player1_Write;  
			port[0].data_r = Player1_Read;
			break;
		
		case SYSTEM_WAYPLAY:
		case SYSTEM_TEAMPLAYER:
			port[0].data_w = MultiTap1_Write;  
			port[0].data_r = MultiTap1_Read;
			break;

		default:
			port[0].data_w = NULL;  
			port[0].data_r = NULL;
			break;
	}

    switch (input.system[1])
    {
		case SYSTEM_GAMEPAD:
			port[1].data_w = Player2_Write;
			port[1].data_r = Player2_Read;
			break;
		
		case SYSTEM_WAYPLAY:
		case SYSTEM_TEAMPLAYER:
			port[1].data_w = MultiTap2_Write;  
			port[1].data_r = MultiTap2_Read;
			break;

		case SYSTEM_MENACER:
			port[1].data_w = NULL;
			port[1].data_r = lightgun_read;
			break;
		
		default:
			port[1].data_w = NULL;  
			port[1].data_r = NULL;
			break;
	}

    port[2].data_w = NULL;
    port[2].data_r = NULL;

	j_cart = 0;
	input_reset(pad_type);
}

void io_write(int offset, int value)
{
	switch (offset)
	{
		case 0x01: /* Port A Data */
		case 0x02: /* Port B Data */
		case 0x03: /* Port C Data */
			io_reg[offset] = ((value & 0x80) | (value & io_reg[offset+3]));
			if(port[offset-1].data_w) port[offset-1].data_w(value);
			return;

		case 0x05:	/* Port B Ctrl */
			if (((value & 0x7F) == 0x7F) &&
				((input.system[0] == SYSTEM_TEAMPLAYER) ||
				(input.system[1] == SYSTEM_TEAMPLAYER)))
			{
				/* autodetect 4-Way play ! */
				input.system[0] = SYSTEM_WAYPLAY;
				input.system[1] = SYSTEM_WAYPLAY;
				port[0].data_w = MultiTap1_Write;  
				port[0].data_r = MultiTap1_Read;
				port[1].data_w = MultiTap2_Write;  
				port[1].data_r = MultiTap2_Read;
				input_reset(pad_type);
			}

		case 0x04:	/* Port A Ctrl */
		case 0x06:	/* Port C Ctrl */
			io_reg[offset] = value & 0xFF;
			io_reg[offset-3] = ((io_reg[offset-3] & 0x80) | (io_reg[offset-3] & io_reg[offset]));
			break;

		case 0x07:	/* Port A TxData */
		case 0x0A:	/* Port B TxData */
		case 0x0D:	/* Port C TxData */
			io_reg[offset] = value;
			break;

		case 0x09:	/* Port A S-Ctrl */
		case 0x0C:	/* Port B S-Ctrl */
		case 0x0F:	/* Port C S-Ctrl */
			io_reg[offset] = (value & 0xF8);
			break;
	}
}

int io_read(int offset)
{
	uint8 has_scd = 0x20; /* No Sega CD unit attached */
    uint8 gen_ver = 0x00; /* Version 0 hardware */
	uint8 input = 0x7F;   /* default input state */

    switch(offset)
    {
        case 0x00: /* Version */
            return (region_code | has_scd | gen_ver);

        case 0x01: /* Port A Data */
        case 0x02: /* Port B Data */
		case 0x03: /* Port C Data */
            if(port[offset-1].data_r) input = port[offset-1].data_r();
			return (io_reg[offset] | ((~io_reg[offset+3]) & input));
    }

  return (io_reg[offset]);
}
