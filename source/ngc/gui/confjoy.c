/****************************************************************************
 *  Genesis Plus 1.2a
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald
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
 * NGC - Joypad Configuration
 ***************************************************************************/
#include <shared.h>

extern int domenu (char items[][20], int maxitems);
extern unsigned short gcpadmap[];
extern int menu;
extern char menutitle[];
extern int padcal;
extern void reloadrom();

int configpadcount = 11;
char padmenu[11][20] = {
  {"Genesis A -     B"},
  {"Genesis B -     A"},
  {"Genesis C -     X"},
  {"Genesis X -    TL"},
  {"Genesis Y -     Y"},
  {"Genesis Z -    TR"},
  {"Analog    -    70"},
  {"Type  -  3BUTTONS"},
  {"PortA -   GAMEPAD"},
  {"PortB -   GAMEPAD"},
  {"   Exit Config   "}
};

uint8 mpads[6] = {0, 1, 2, 3, 4, 5 }; /*** Default Mapping ***/
uint8 sys_type[2] = {0,0};
uint8 old_sys_type[2] = {0,0};

/****************************************************************************
 * UpdatePadMaps
 ****************************************************************************/
void UpdatePadMaps (uint8 padvalue, int padnum)
{
	padmenu[padnum][15] = ' ';
	padmenu[padnum][16] = ' ';
	switch (padvalue)
	{
		case 0:
		  gcpadmap[padnum] = PAD_BUTTON_B;
		  padmenu[padnum][16] = 'B';
		  break;

		case 1:
		  gcpadmap[padnum] = PAD_BUTTON_A;
		  padmenu[padnum][16] = 'A';
		  break;

		case 2:
		  gcpadmap[padnum] = PAD_BUTTON_X;
		  padmenu[padnum][16] = 'X';
		  break;

		case 3:
		  gcpadmap[padnum] = PAD_TRIGGER_R;
		  padmenu[padnum][15] = 'T';
		  padmenu[padnum][16] = 'R';
		  break;

		case 4:
		  gcpadmap[padnum] = PAD_BUTTON_Y;
		  padmenu[padnum][16] = 'Y';
		  break;

		case 5:
		  gcpadmap[padnum] = PAD_TRIGGER_L;
		  padmenu[padnum][15] = 'T';
		  padmenu[padnum][16] = 'L';
		  break;
	}
}

/****************************************************************************
 * ConfigureJoypads
 ****************************************************************************/
void ConfigureJoypads ()
{
	int ret;
	int quit = 0;
	int prevmenu = menu;

	strcpy (menutitle, "");

	if (pad_type) sprintf (padmenu[7], "Type  -  6BUTTONS");
	else sprintf (padmenu[7], "Type  -  3BUTTONS");

	if (input.system[1] == SYSTEM_MENACER) sprintf (padmenu[8], "PortA -      NONE");
	else if (sys_type[0] == 0)  sprintf (padmenu[8], "PortA -   GAMEPAD");
	else if (sys_type[0] == 1)	sprintf (padmenu[8], "PortA -  MULTITAP");
	else if (sys_type[0] == 2)  sprintf (padmenu[8], "PortA -       NONE");

	if (input.system[1] == SYSTEM_MENACER) sprintf (padmenu[9], "PortB -   MENACER");
	else if (sys_type[1] == 0)	sprintf (padmenu[9], "PortB -   GAMEPAD");
	else if (sys_type[1] == 1)	sprintf (padmenu[9], "PortB -  MULTITAP");
	else if (sys_type[1] == 2)  sprintf (padmenu[9], "PortB -       NONE");
			  
	menu = 0;
	while (quit == 0)
	{
		ret = domenu (&padmenu[0], configpadcount);
		switch (ret)
		{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			  mpads[ret]++;
			  if (mpads[ret] > 5) mpads[ret] = 0;
			  UpdatePadMaps (mpads[ret], ret);
			  break;

			case 6:		/*** Pad calibrate analog ***/
			case -8:
			  if (ret>0) padcal += 2;
			  else padcal -= 2;
			  if (padcal > 90) padcal = 0;
			  if (padcal < 0) padcal = 90;
			  sprintf (padmenu[6], "Analog    -    %02d", padcal);
			  break;

			case 7:
			  pad_type ^= 1;
    		  if (pad_type) sprintf (padmenu[7], "Type  -  6BUTTONS");
			  else sprintf (padmenu[7], "Type  -  3BUTTONS");
			  system_reset();
			  break;

			case 8:
			  if (input.system[1] == SYSTEM_MENACER) break;
			  sys_type[0] ++;
			  if (sys_type[0] > 2) sys_type[0] = 0;

			  if (sys_type[0] == 0)
			  {
				  input.system[0] = SYSTEM_GAMEPAD;
				  sprintf (padmenu[8], "PortA -   GAMEPAD");
			  }
			  else if (sys_type[0] == 1)
			  {
				  input.system[0] = SYSTEM_TEAMPLAYER;
				  sprintf (padmenu[8], "PortA -  MULTITAP");
		      }
			  else if (sys_type[0] == 2)
			  {
				  input.system[0] = NO_SYSTEM;
				  sprintf (padmenu[8], "PortA -      NONE");
			  }
			  break;
		
			case 9:
			  if (input.system[1] == SYSTEM_MENACER) break;
			  sys_type[1] ++;
			  if (sys_type[1] > 2) sys_type[1] = 0;

			  if (sys_type[1] == 0)
			  {
				  input.system[1] = SYSTEM_GAMEPAD;
				  sprintf (padmenu[9], "PortB -   GAMEPAD");
			  }
			  else if (sys_type[1] == 1)
			  {
				  input.system[1] = SYSTEM_TEAMPLAYER;
				  sprintf (padmenu[9], "PortB -  MULTITAP");
		      }
			  else if (sys_type[1] == 2)
			  {
				  input.system[1] = NO_SYSTEM;
				  sprintf (padmenu[9], "PortB -      NONE");
			  }
			  break;
			
			case 10:
			case -1:
			  if ((old_sys_type[0] != sys_type[0]) || (old_sys_type[1] != sys_type[1]))
			  {
				old_sys_type[0] = sys_type[0];
				old_sys_type[1] = sys_type[1];
				system_reset();
			  }
			  quit = 1;
			  break;
		}
	}

	menu = prevmenu;
}
