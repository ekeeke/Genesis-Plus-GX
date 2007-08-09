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
 * Nintendo Gamecube Menus
 *
 * Please put any user menus here! - softdev March 12 2006
 ***************************************************************************/
#include "shared.h"
#include "dvd.h"
#include "rominfo.h"
#include "font.h"

#define PSOSDLOADID 0x7c6000a6

/***************************************************************************
 * drawmenu
 *
 * As it says, simply draws the menu with a highlight on the currently
 * selected item :)
 ***************************************************************************/
char menutitle[60] = { "" };
int menu = 0;

void drawmenu (char items[][20], int maxitems, int selected)
{
  int i;
  int ypos;

  ypos = (310 - (fheight * maxitems)) >> 1;
  ypos += 130;

  ClearScreen ();
  WriteCentre (134, menutitle);

  for (i = 0; i < maxitems; i++)
  {
      if (i == selected) WriteCentre_HL (i * fheight + ypos, (char *) items[i]);
      else WriteCentre (i * fheight + ypos, (char *) items[i]);
  }

  SetScreen ();
}

/****************************************************************************
 * domenu
 *
 * Returns index into menu array when A is pressed, -1 for B
 ****************************************************************************/
int domenu (char items[][20], int maxitems)
{
  int redraw = 1;
  int quit = 0;
  short p;
  int ret = 0;
  signed char a,b;

  while (quit == 0)
  {
      if (redraw)
	  {
	      drawmenu (&items[0], maxitems, menu);
	      redraw = 0;
	  }

      p = PAD_ButtonsDown (0);
      a = PAD_StickY (0);
	  b = PAD_StickX (0);

	  /*** Look for up ***/
      if ((p & PAD_BUTTON_UP) || (a > 70))
	  {
	    redraw = 1;
	    menu--;
        if (menu < 0) menu = maxitems - 1;
	  }

		  /*** Look for down ***/
      if ((p & PAD_BUTTON_DOWN) || (a < -70))
	  {
	    redraw = 1;
	    menu++;
        if (menu == maxitems) menu = 0;
	  }

	  if ((p & PAD_BUTTON_A) || (b > 40) || (p & PAD_BUTTON_RIGHT))
	  {
	    quit = 1;
	    ret = menu;
	  }
	  
	  if ((b < -40) || (p & PAD_BUTTON_LEFT))
	  {
	    quit = 1;
	    ret = 0 - 2 - menu;
	  }

      if (p & PAD_BUTTON_B)
	  {
	    quit = 1;
	    ret = -1;
	  }
  }

  return ret;
}

/****************************************************************************
 * Sound Option menu
 *
 ****************************************************************************/
double psg_preamp = 3.0;
double fm_preamp  = 1.0;
u8 boost = 1;
uint8 clipping = 2;
uint8 hq_fm = 1;
uint8 FM_GENS  = 0;
uint8 PSG_MAME = 0;

void soundmenu ()
{
	int ret;
	int quit = 0;
	int prevmenu = menu;
	int count = 7;
	char items[7][20];

	strcpy (menutitle, "Sound Options");

	sprintf (items[0], "PSG Volume : %1.2f", psg_preamp);
	sprintf (items[1], "FM Volume  : %1.2f", fm_preamp);
	sprintf (items[2], "Volume Boost:  %dX", boost);
	sprintf (items[3], "HQ YM2612:      %s", hq_fm ? "Y" : "N");
	sprintf (items[4], "FM core :    %s", FM_GENS ? "GENS" : "MAME");
	sprintf (items[5], "PSG core:    %s", PSG_MAME ? "MAME" : "SMSP");

	strcpy (items[6], "Return to previous");

	menu = 0;
	while (quit == 0)
	{
		ret = domenu (&items[0], count);
		switch (ret)
		{
			case 0:
			case -2:
				if (ret<0) psg_preamp -= 0.01;
				else psg_preamp += 0.01;
				if (psg_preamp < 0.0) psg_preamp = 5.0;
				if (psg_preamp > 5.0) psg_preamp = 0.0;
				sprintf (items[0], "PSG Volume : %1.2f", psg_preamp);
				break;

			case 1:
			case -3:
				if (ret<0) fm_preamp -= 0.01;
				else fm_preamp += 0.01;
				if (fm_preamp < 0.0) fm_preamp = 5.0;
				if (fm_preamp > 5.0) fm_preamp = 0.0;
				sprintf (items[1], "FM Volume  : %1.2f", fm_preamp);
				break;

			case 2:
				boost ++;
				if (boost > 4) boost = 0;
				sprintf (items[2],  "Volume Boost:  %dX", boost);
				break;
			
			case 3:
				hq_fm ^= 1;
				sprintf (items[3], "HQ YM2612:      %s", hq_fm ? "Y" : "N");
				if (genromsize) 
				{	audio_init(48000);
					fm_restore();
				}
				break;
			
			case 4:
				FM_GENS ^= 1;
				psg_preamp = PSG_MAME ? (FM_GENS ? 0.85 : 0.50) : (FM_GENS ? 4.0 : 3.0);
				fm_preamp  = 1.0;
				sprintf (items[0], "PSG Volume : %1.2f", psg_preamp);
				sprintf (items[1], "FM Volume  : %1.2f", fm_preamp);
				sprintf (items[4], "FM core :    %s", FM_GENS ? "GENS" : "MAME");
				if (genromsize) 
				{
					audio_init(48000);
					fm_restore();
				}
				break;

			case 5:
				PSG_MAME ^= 1;
				psg_preamp = PSG_MAME ? (FM_GENS ? 0.85 : 0.50) : (FM_GENS ? 4.0 : 3.0);
				fm_preamp  = 1.0;
				sprintf (items[0], "PSG Volume : %1.2f", psg_preamp);
				sprintf (items[1], "FM Volume  : %1.2f", fm_preamp);
				sprintf (items[5], "PSG core:    %s", PSG_MAME ? "MAME" : "SMSP");
				if (genromsize) audio_init(48000);
				break;

			case 6:
			case -1:
				quit = 1;
				break;
		}
	}
	menu = prevmenu;
}

/****************************************************************************
 * Misc Option menu
 *
 ****************************************************************************/
extern void reloadrom ();
extern s16 square[];
extern int oldvwidth, oldvheight;
extern uint8 alttiming;
extern uint8 dmatiming;
extern uint8 vdptiming;

uint8 autoload = 0;
uint8 region_detect = 0;
uint8 cpu_detect = 0;

void miscmenu ()
{
	int ret;
	int quit = 0;
	int prevmenu = menu;
	int count = 9;
	char items[9][20];
	
	sprintf (items[0], "Scale X:      %02d", square[3]);
	sprintf (items[1], "Scale Y:      %02d", square[1]);
	sprintf (items[2], "Vdp Latency:   %s", vdptiming ? "Y" : "N");
	sprintf (items[3], "Dma Timing :   %s", dmatiming ? "Y" : "N");
    sprintf (items[4], "Alt Timing  :   %s", alttiming ? "Y" : "N");
	if (cpu_detect == 0)         sprintf (items[5], "Cpu Mode:   AUTO");
	else if (cpu_detect == 1)    sprintf (items[5], "Cpu Mode:   NTSC");
	else if (cpu_detect == 2)    sprintf (items[5], "Cpu Mode:    PAL");
	if (region_detect == 0)      sprintf (items[6], "Region:     AUTO");
	else if (region_detect == 1) sprintf (items[6], "Region:      USA");
	else if (region_detect == 2) sprintf (items[6], "Region:      EUR");
	else if (region_detect == 3) sprintf (items[6], "Region: JAP-NTSC");
	else if (region_detect == 4) sprintf (items[6], "Region:  JAP-PAL");
	sprintf (items[7], "SRAM autoload: %s", autoload ? "Y" : "N");
	strcpy (items[8], "Return to previous");

	menu = 0;
	while (quit == 0)
	{
		strcpy (menutitle, "");
		ret = domenu (&items[0], count);
		switch (ret)
		{
			case 0:		/*** Scale X ***/
			case -2:
				if (ret<0) square[3] -= 2;
				else square[3] += 2;
				if (square[3] < 40) square[3] = 80;
				if (square[3] > 80) square[3] = 40;
				square[6] = square[3];
				square[0] = square[9] = -square[3];
				oldvwidth = -1;
				sprintf (items[0], "Scale X:      %02d", square[3]);
				break;

			case 1:		/*** Scale Y ***/
			case -3:
				if (ret<0) square[1] -= 2;
				else square[1] += 2;
				if (square[1] < 30) square[1] = 60;
				if (square[1] > 60) square[1] = 30;
				square[4] = square[1];
				square[7] = square[10] = -square[1];
				oldvheight = -1;
				sprintf (items[1], "Scale Y:      %02d", square[1]);
				break;

			case 2:		/*** VDP access latency ***/
				vdptiming ^= 1;
				sprintf (items[2], "Vdp Latency:   %s", vdptiming ? "Y" : "N");
				break;

			case 3:		/*** DMA timing fix ***/
				dmatiming ^= 1;
				sprintf (items[3], "Dma Timing :   %s", dmatiming ? "Y" : "N");
				break;

			case 4:		/*** Alternate rendering timing ***/
				alttiming ^= 1;
				sprintf (items[4], "Alt Timing  :   %s", alttiming ? "Y" : "N");
				break;

			case 5:		/*** Cpu mode : PAL (50hz) or NTSC (60Hz) ***/
				cpu_detect ++;
				if (cpu_detect > 2) cpu_detect = 0;
				if (cpu_detect == 0)      sprintf (items[5], "Cpu Mode:   AUTO");
				else if (cpu_detect == 1) sprintf (items[5], "Cpu Mode:   NTSC");
				else if (cpu_detect == 2) sprintf (items[5], "Cpu Mode:    PAL");
				if (genromsize) reloadrom();
				break;

			case 6:  /* region detection */
				region_detect ++;
				if (region_detect > 4) region_detect = 0;
				if (region_detect == 0)      sprintf (items[6], "Region:     AUTO");
				else if (region_detect == 1) sprintf (items[6], "Region:      USA");
				else if (region_detect == 2) sprintf (items[6], "Region:      EUR");
				else if (region_detect == 3) sprintf (items[6], "Region: JAP-NTSC");
				else if (region_detect == 4) sprintf (items[6], "Region:  JAP-PAL");
				if (genromsize) reloadrom();
				break;
	
			case 7:		/*** VDP access latency ***/
				autoload ^= 1;
				sprintf (items[7], "SRAM autoload: %s", autoload ? "Y" : "N");
				break;

			case 8:
			case -1:
				quit = 1;
				break;
		}
	}
	menu = prevmenu;
}

/****************************************************************************
 * Main Option menu
 *
 ****************************************************************************/
extern void ConfigureJoypads();
extern void GetGGEntries();

void optionmenu ()
{
	int ret;
	int quit = 0;
	int prevmenu = menu;
	int count = 5;
	char items[5][20] = {
		"Misc. Options",
		"Sound Options",
		"Configure Joypads",
		"Game Genie Codes",
		"Return to previous"
	};

	menu = 0;
	while (quit == 0)
	{
		strcpy (menutitle, "");
		ret = domenu (&items[0], count);
		switch (ret)
		{
			case 0:
				miscmenu();
				break;
			case 1:
				soundmenu();
				break;
			case 2:
				ConfigureJoypads();
				break;
			case 3:
				GetGGEntries();
				break;
			case 4:
			case -1:
				quit = 1;
				break;
		}
	}
	menu = prevmenu;
}

/****************************************************************************
* Generic Load/Save menu
*
****************************************************************************/
int CARDSLOT = CARD_SLOTB;
int use_SDCARD = 0;
extern int ManageSRAM (int direction);
extern int ManageState (int direction);

int loadsavemenu (int which)
{
	int prevmenu = menu;
	int quit = 0;
	int ret;
	int count = 5;
	char items[5][20];

	strcpy (menutitle, "");

	if (use_SDCARD) sprintf(items[0], "Device: SDCARD");
	else sprintf(items[0], "Device:  MCARD");

    if (CARDSLOT == CARD_SLOTA) sprintf(items[1], "Use: SLOT A");
    else sprintf(items[1], "Use: SLOT B");

	if (which)
	{
		sprintf(items[2], "Save State");
		sprintf(items[3], "Load State");
	}
	else
	{
		sprintf(items[2], "Save SRAM");
		sprintf(items[3], "Load SRAM");
	}
	sprintf(items[4], "Return to previous");

	menu = 2;

	while (quit == 0)
	{
		ret = domenu (&items[0], count);
		switch (ret)
		{
			case -1:
			case  4:
				quit = 1;
				break;

			case 0:
				use_SDCARD ^= 1;
				if (use_SDCARD) sprintf(items[0], "Device: SDCARD");
				else sprintf(items[0], "Device:  MCARD");
				break;
			case 1:
				CARDSLOT ^= 1;
				if (CARDSLOT == CARD_SLOTA) sprintf(items[1], "Use: SLOT A");
				else sprintf(items[1], "Use: SLOT B");
				break;
			case 2:
			case 3:
				if (which) quit = ManageState(ret-2);
				else quit = ManageSRAM(ret-2);
				if (quit) return 1;
				break;
		}
	}

	menu = prevmenu;
	return 0;
}


/****************************************************************************
 * File Manager menu
 *
 ****************************************************************************/
int filemenu ()
{
	int prevmenu = menu;
	int ret;
	int quit = 0;
	uint32 crccheck;
	int count = 3;
	char items[3][20] = {
		{"SRAM Manager"},
		{"STATE Manager"},
		{"Return to previous"}
	};

	crccheck = crc32 (0, &sram.sram[0], 0x10000);
	if (genromsize && (crccheck != sram.crc)) strcpy (menutitle, "*** SRAM has been modified ***");
	else strcpy (menutitle, "");

	menu = 0;

	while (quit == 0)
	{
		ret = domenu (&items[0], count);
		switch (ret)
		{
			case -1: /*** Button B ***/
			case 2:  /*** Quit ***/
				ret = 0;
				quit = 1;
				break;
			case 0:	 /*** SRAM Manager ***/
			case 1:  /*** SaveState Manager ***/
				if (loadsavemenu(ret)) return 1;
				break;
		}
	}

	menu = prevmenu;
	return 0;
}

/****************************************************************************
 * Load Rom menu
 *
 ****************************************************************************/
extern void OpenDVD ();
extern int OpenSD ();
extern u8 UseSDCARD;

void loadmenu ()
{
	int prevmenu = menu;
	int ret;
	int quit = 0;
	int count = 3;
	char item[3][20] = {
		{"Load from DVD"},
		{"Load from SDCARD"},
		{"Return to previous"}
	};

	menu = UseSDCARD ? 1 : 0;
	
	while (quit == 0)
	{
		strcpy (menutitle, "");
		ret = domenu (&item[0], count);
		switch (ret)
		{
			case -1: /*** Button B ***/
			case 2:  /*** Quit ***/
				quit = 1;
				menu = prevmenu;
				break;
			case 0:	 /*** Load from DVD ***/
				OpenDVD ();
				quit = 1;
				break;
			case 1:  /*** Load from SCDARD ***/
				OpenSD ();
				quit = 1;
				break;
		}
	}
}

/****************************************************************************
 * Main menu
 *
 ****************************************************************************/

void MainMenu ()
{
	menu = 0;
	int ret;
	int quit = 0;
	int *psoid = (int *) 0x80001800;
	void (*PSOReload) () = (void (*)()) 0x80001800; 	/*** Stock PSO/SD Reload call ***/
	int count = 8;
	char items[8][20] = {
		{"Play Game"},
		{"Game Infos"},
		{"Reset Game"},
		{"Load New Game"},
		{"File Management"},
		{"Emulator Options"},
		{"Stop DVD Motor"},
		{"System Reboot"}
	};

	while (quit == 0)
	{
		strcpy (menutitle, "");
		ret = domenu (&items[0], count);
		switch (ret)
		{
			case -1: /*** Button B ***/
			case 0:  /*** Play Game ***/
				quit = 1;
				break;
			case 1:	 /*** ROM Information ***/
				showrominfo ();
				break;
			case 2:  /*** Emulator Reset ***/
				system_reset ();
				quit = 1;
				break;
			case 3:  /*** Load ROM Menu ***/
				loadmenu();
				menu = 0;
				break;
			case 4:  /*** Memory Manager ***/
				quit = filemenu ();
				break;
			case 5:  /*** Emulator Options */
				optionmenu ();
				break;
			case 6:  /*** Stop DVD Motor ***/
				ShowAction("Stopping DVD Motor ...");
				dvd_motor_off();
				break;
			case 7:  /*** SD/PSO Reload ***/
				if (psoid[0] == PSOSDLOADID) PSOReload ();
				else SYS_ResetSystem(SYS_HOTRESET,0,FALSE);
				break;
		}
	}

	/*** Remove any still held buttons ***/
	while(PAD_ButtonsHeld(0)) VIDEO_WaitVSync();

	/*** Stop the DVD from causing clicks while playing ***/
	uselessinquiry ();
}
