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
 * Information in this module was gleaned from
 * http://en.wikibooks.org/wiki/Genesis_Programming
 ***************************************************************************/

#include "shared.h"
#include "font.h"
#include "rominfo.h"

#define MAXCOMPANY 64

/*** ROM Information ***/
#define ROMCONSOLE 		256
#define ROMCOPYRIGHT 	272
#define ROMDOMESTIC		288
#define ROMWORLD		336
#define ROMTYPE 		384
#define ROMPRODUCT		386
#define ROMCHECKSUM		398
#define ROMIOSUPPORT	400
#define ROMROMSTART		416
#define ROMROMEND		420
#define ROMRAMINFO		424
#define ROMRAMSTART		436
#define ROMRAMEND		440
#define ROMMODEMINFO	444
#define ROMMEMO         456
#define ROMCOUNTRY		496

#define P3BUTTONS 1
#define P6BUTTONS 2
#define PKEYBOARD 4
#define PPRINTER  8
#define PBALL     16
#define PFLOPPY   32
#define PACTIVATOR 64
#define PTEAMPLAYER 128
#define PMSYSTEMPAD 256
#define PSERIAL     512
#define PTABLET    1024
#define PPADDLE    2048
#define PCDROM     4096
#define PMOUSE     8192

typedef struct
{
  char companyid[6];
  char company[30];
} COMPANYINFO;

typedef struct
{
  char pID[2];
  char pName[21];
} PERIPHERALINFO;

int peripherals;
int checksumok;
ROMINFO rominfo;
uint16 GetRealChecksum ();

 /***************************************************************************
  * Genesis ROM Manufacturers
  *
  * Based on the document provided at
  * http://www.zophar.net/tech/files/Genesis_ROM_Format.txt
  ***************************************************************************/
COMPANYINFO companyinfo[MAXCOMPANY] = {
  {"ACLD", "Ballistic"},
  {"RSI", "Razorsoft"},
  {"SEGA", "SEGA"},
  {"TREC", "Treco"},
  {"VRGN", "Virgin Games"},
  {"WSTN", "Westone"},
  {"10", "Takara"},
  {"11", "Taito or Accolade"},
  {"12", "Capcom"},
  {"13", "Data East"},
  {"14", "Namco or Tengen"},
  {"15", "Sunsoft"},
  {"16", "Bandai"},
  {"17", "Dempa"},
  {"18", "Technosoft"},
  {"19", "Technosoft"},
  {"20", "Asmik"},
  {"22", "Micronet"},
  {"23", "Vic Tokai"},
  {"24", "American Sammy"},
  {"29", "Kyugo"},
  {"32", "Wolfteam"},
  {"33", "Kaneko"},
  {"35", "Toaplan"},
  {"36", "Tecmo"},
  {"40", "Toaplan"},
  {"42", "UFL Company Limited"},
  {"43", "Human"},
  {"45", "Game Arts"},
  {"47", "Sage's Creation"},
  {"48", "Tengen"},
  {"49", "Renovation or Telenet"},
  {"50", "Electronic Arts"},
  {"56", "Razorsoft"},
  {"58", "Mentrix"},
  {"60", "Victor Musical Industries"},
  {"69", "Arena"},
  {"70", "Virgin"},
  {"73", "Soft Vision"},
  {"74", "Palsoft"},
  {"76", "Koei"},
  {"79", "U.S. Gold"},
  {"81", "Acclaim/Flying Edge"},
  {"83", "Gametek"},
  {"86", "Absolute"},
  {"87", "Mindscape"},
  {"93", "Sony"},
  {"95", "Konami"},
  {"97", "Tradewest"},
  {"100", "T*HQ Software"},
  {"101", "Tecmagik"},
  {"112", "Designer Software"},
  {"113", "Psygnosis"},
  {"119", "Accolade"},
  {"120", "Code Masters"},
  {"125", "Interplay"},
  {"130", "Activision"},
  {"132", "Shiny & Playmates"},
  {"144", "Atlus"},
  {"151", "Infogrames"},
  {"161", "Fox Interactive"},
  {"177", "Ubisoft"},
  {"239", "Disney Interactive"},
  {"---", "Unknown"}
};

 /***************************************************************************
  * Genesis Peripheral Information
  *
  * Based on the document provided at
  * http://www.zophar.net/tech/files/Genesis_ROM_Format.txt
  ***************************************************************************/
PERIPHERALINFO peripheralinfo[14] = {
  {"J", "3-Button Joypad"},
  {"6", "6-button Joypad"},
  {"K", "Keyboard"},
  {"P", "Printer"},
  {"B", "Control Ball"},
  {"F", "Floppy Drive"},
  {"L", "Activator"},
  {"4", "Team Player"},
  {"0", "MS Joypad"},
  {"R", "RS232C Serial"},
  {"T", "Tablet"},
  {"V", "Paddle"},
  {"C", "CD-ROM"},
  {"M", "Mega Mouse"}
};

/****************************************************************************
 * getcompany
 *
 * Try to determine which company made this rom
 *
 * Ok, for some reason there's no standard for this.
 * It seems that there can be pretty much anything you like following the
 * copyright (C) symbol!
 ****************************************************************************/
int getcompany ()
{
	char *s;
	int i;
	char company[10];

	for (i = 3; i < 8; i++) company[i - 3] = rominfo.copyright[i];
	company[5] = 0;

	/** OK, first look for a hyphen
	 *  Capcom use T-12 for example
	 */
	s = strstr (company, "-");
	if (s != NULL)
	{
		s++;
		strcpy (company, s);
	}

	/** Strip any trailing spaces **/
	for (i = strlen (company) - 1; i >= 0; i--)
		if (company[i] == 32) company[i] = 0;

	if (strlen (company) == 0) return MAXCOMPANY - 1;

	for (i = 0; i < MAXCOMPANY - 1; i++)
	{
		if (!(strncmp (company, companyinfo[i].companyid, strlen (company)))) return i;
	}

	return MAXCOMPANY - 1;
}


 /***************************************************************************
  * getrominfo
  *
  * Pass a pointer to the ROM base address.
  ***************************************************************************/
void getrominfo (char *romheader)
{
  int i,j;

  memset (&rominfo, 0, sizeof (ROMINFO));

  memcpy (&rominfo.consoletype, romheader + ROMCONSOLE, 16);
  memcpy (&rominfo.copyright, romheader + ROMCOPYRIGHT, 16);
  memcpy (&rominfo.domestic, romheader + ROMDOMESTIC, 48);
  memcpy (&rominfo.international, romheader + ROMWORLD, 48);
  memcpy (&rominfo.ROMType, romheader + ROMTYPE, 2);
  memcpy (&rominfo.product, romheader + ROMPRODUCT, 12);
  memcpy (&rominfo.checksum, romheader + ROMCHECKSUM, 2);
  memcpy (&rominfo.io_support, romheader + ROMIOSUPPORT, 16);
  memcpy (&rominfo.romstart, romheader + ROMROMSTART, 4);
  memcpy (&rominfo.romend, romheader + ROMROMEND, 4);
  memcpy (&rominfo.RAMInfo, romheader + ROMRAMINFO, 12);
  memcpy (&rominfo.ramstart, romheader + ROMRAMSTART, 4);
  memcpy (&rominfo.ramend, romheader + ROMRAMEND, 4);
  memcpy (&rominfo.modem, romheader + ROMMODEMINFO, 12);
  memcpy (&rominfo.memo, romheader + ROMMEMO, 40);
  memcpy (&rominfo.country, romheader + ROMCOUNTRY, 16);

  checksumok = (GetRealChecksum ((char *) cart_rom + 512, genromsize - 512)
		== rominfo.checksum);

  peripherals = 0;

  for (i = 0; i < 14; i++)
	for (j=0; j < 14; j++)
		if (rominfo.io_support[i] == peripheralinfo[j].pID[0]) peripherals |= (1 << j);

  if (peripherals & P6BUTTONS) pad_type = DEVICE_6BUTTON;
  else pad_type = DEVICE_3BUTTON;

}

/***************************************************************************
  * Show rom info screen
 ***************************************************************************/
  /* Automatically fixing the checksum is not a cool idea
   * This should be user switchable, or at least only applied
   * when genromsize == ( romend - romstart )
   if(realchecksum != (rominfo.checksum))
   {
   sprintf(msg, "WARNING: Possible hacked ROM loaded!");
   write_font( 10, 224, msg);
   cart_rom[0x18e] = realchecksum >> 8;
   cart_rom[0x18f] = realchecksum & 0xff;
   sprintf(msg, "Checksum corrected to %04x", realchecksum);
   write_font( 10, 248, msg);
   }       
   */
void showrominfo ()
{
  int ypos;
  u8 i,j,quit,redraw,max;
  char msg[128];
  short p;
  signed char a;
  char pName[14][21];
  uint16 realchecksum = GetRealChecksum (((uint8 *) cart_rom) + 0x200, genromsize - 0x200);

  quit = 0;
  j = 0;
  redraw = 1;

  /*** Remove any still held buttons ***/
  while(PAD_ButtonsHeld(0)) VIDEO_WaitVSync();

  max = 14;
  for (i = 0; i < 14; i++)
  {
	  if (peripherals & (1 << i))
	  {
		  sprintf(pName[max-14],"%s", peripheralinfo[i].pName);
		  max ++;
	  }
  }

  while (quit == 0)
  {
      if (redraw)
      {
		  ClearScreen ();

		  ypos = 134;
		  WriteCentre(ypos, "ROM Header Information");
		  ypos += 2*fheight;

		  for (i=0; i<8; i++)
		  {
			  switch (i+j)
			  {
				case 0:
					sprintf (msg, "Console type: %s", rominfo.consoletype);
					break;
				case 1:
					sprintf (msg, "Copyright: %s", rominfo.copyright);
					break;
				case 2:
					sprintf (msg, "Company: %s", companyinfo[getcompany ()].company);
					break;
				case 3:
					sprintf (msg, "Game Domestic Name:");
					break;
				case 4:
					sprintf(msg, " %s",rominfo.domestic);
					break;
				case 5:
					sprintf (msg, "Game International Name:");
					break;
				case 6:
					sprintf(msg, " %s",rominfo.international);
					break;
				case 7:
					sprintf (msg, "Type - %s : %s", rominfo.ROMType, strcmp (rominfo.ROMType, "AI") ? "Game" : "Educational");
					break;
				case 8:
					sprintf (msg, "Product - %s", rominfo.product);
					break;
				case 9:
					sprintf (msg, "Checksum - %04x (%04x) (%s)", rominfo.checksum, realchecksum, rominfo.checksum == realchecksum ? "Good" : "Bad");
					break;
				case 10:
					sprintf (msg, "ROM end: $%06X", rominfo.romend);
					break;
				case 11:
					if (sram.custom) sprintf (msg, "EEPROM(%dK) - $%06X", ((eeprom.type.size_mask+1)* 8) /1024, (unsigned int)sram.start);
					else if (sram.detected) sprintf (msg, "SRAM Start  - $%06X", rominfo.ramstart);
					else sprintf (msg, "External RAM undetected");
						 
					break;
				case 12:
					if (sram.custom) sprintf (msg, "EEPROM(%dK) - $%06X", ((eeprom.type.size_mask+1)* 8) /1024, (unsigned int)sram.end);
					else if (sram.detected) sprintf (msg, "SRAM End   - $%06X", rominfo.ramend);
					else if (sram.on) sprintf (msg, "Default SRAM activated ");
					else sprintf (msg, "SRAM is disactivated  ");
					break;
				case 13:
					if (region_code == REGION_USA) sprintf (msg, "Region - %s (USA)", rominfo.country);
					else if (region_code == REGION_EUROPE) sprintf (msg, "Region - %s (EUR)", rominfo.country);
					else if (region_code == REGION_JAPAN_NTSC) sprintf (msg, "Region - %s (JAP)", rominfo.country);
					else if (region_code == REGION_JAPAN_PAL) sprintf (msg, "Region - %s (JPAL)", rominfo.country);
					break;
				default:
					sprintf (msg, "Supports - %s", pName[i+j-14]);
					break;
			}

			write_font (100, ypos, msg);
			ypos += fheight;
		}

		ypos += fheight;
		WriteCentre (ypos, "Press A to Continue");
		SetScreen ();
	}

	p = PAD_ButtonsDown (0);
    a = PAD_StickY (0);
	redraw = 0;

	if ((j<(max-8)) && ((p & PAD_BUTTON_DOWN) || (a < -70))) {redraw = 1; j++;}
	if ((j>0) && ((p & PAD_BUTTON_UP) || (a > 70))) {redraw = 1; j--;}
	if (p & PAD_BUTTON_A) quit = 1;
	if (p & PAD_BUTTON_B) quit = 1;
  }
}

/*
 * softdev - New Checksum Calculation
 */
uint16 GetRealChecksum (uint8 * rom, int length)
{
  int i;
  uint16 checksum = 0;

  for (i = 0; i < length; i += 2)
  {
      checksum += (uint16) rom[i];
      checksum += (uint16) rom[i + 1] << 8;
  }

  return checksum;
}
