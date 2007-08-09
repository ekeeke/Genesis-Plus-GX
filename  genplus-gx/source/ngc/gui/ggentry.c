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
 * Nintendo Gamecube Game Genie Entry
 ***************************************************************************/

#include "shared.h"
#include "font.h"
#include "rominfo.h"

#define MAXCODES 8

typedef struct
{
  int address;
  unsigned short data;
} GGPATCH;

/*** Game Genie Codes Array ***/
unsigned char ggcodes[MAXCODES][10];	/*** Codes are entered as XXXX-XXXX ***/
int gghpos[MAXCODES];		/*** Edit positions ***/
int ggrow = 0;
int editing = 0;
char ggvalidchars[] = "ABCDEFGHJKLMNPRSTVWXYZ0123456789*";
GGPATCH ggpatch[8];
extern char menutitle[];

/****************************************************************************
 * Decode Game Genie entries to memory patches
 ****************************************************************************/
void decode_genie (char *code, int which)
{
  char *p;
  int n, i;
  for (i = 0; i < 8; i++)
  {
	  /*** This should only happen if memory is corrupt! ***/
	  p = strchr (ggvalidchars, code[i]);
	  if (p == NULL)
	  {
		  ggpatch[which].address = ggpatch[which].data = 0;
		  return;
	  }

	  n = p - ggvalidchars;

	  switch (i)
	  {
		  case 0:
			ggpatch[which].data |= n << 3;
			break;

		  case 1:
			ggpatch[which].data |= n >> 2;
			ggpatch[which].address |= (n & 3) << 14;
			break;

		  case 2:
			ggpatch[which].address |= n << 9;
			break;

		  case 3:
			ggpatch[which].address |= (n & 0xF) << 20 | (n >> 4) << 8;
			break;

		  case 4:
			ggpatch[which].data |= (n & 1) << 12;
			ggpatch[which].address |= (n >> 1) << 16;
			break;

		  case 5:
			ggpatch[which].data |= (n & 1) << 15 | (n >> 1) << 8;
			break;

		  case 6:
			ggpatch[which].data |= (n >> 3) << 13;
			ggpatch[which].address |= (n & 7) << 5;
			break;

		  case 7:
			ggpatch[which].address |= n;
			break;
	  }
  }
}

void decode_ggcodes ()
{
  int i, j;
  char thiscode[10];

  /*** Clear out any old patches ***/
  memset (&ggpatch[0], 0, 8 * sizeof (GGPATCH));
  memset (&thiscode, 0, 10);

  j = 0;
  for (i = 0; i < 8; i++)
  {
	  if (strcmp (ggcodes[i], "AAAA-AAAA"))
	  {
		  /*** Move the code into thiscode ***/
		  memcpy (&thiscode, &ggcodes[i], 4);
		  memcpy (&thiscode[4], &ggcodes[i][5], 4);

		  decode_genie (thiscode, j);
		  j++;
	  }
  }

  /*** And now apply the patches ***/
  if (j)
  {
	  for (i = 0; i < j; i++)
	  {
		  if (ggpatch[i].address < rominfo.romend)
		  {
			  /*** Patching ROM space ***/
			  cart_rom[ggpatch[i].address] = ggpatch[i].data & 0x0ff;
			  cart_rom[ggpatch[i].address + 1] = (ggpatch[i].data & 0xff00) >> 8;
		  }
		  else
		  {
			  /*** Patching 68K memory ***/
			  m68k_write_memory_16 (ggpatch[i].address, ggpatch[i].data);
		  }
      }
	  /* TODO : Fix Checksum  */
  }
}

/****************************************************************************
 * ClearGGCodes
 *
 * Should be called whenever a new rom is loaded
 ****************************************************************************/
void ClearGGCodes ()
{
  int i;

  for (i = 0; i < MAXCODES; i++)
  {
	  strcpy (ggcodes[i], "AAAA-AAAA");
	  gghpos[i] = 0;
  }
  ggrow = 0;
}

/****************************************************************************
 * DrawGGCodes
 *
 * Just draw the codes, with the current one highlighted.
 ****************************************************************************/
void DrawGGCodes ()
{
  int i,j;
  unsigned char c[2] = { 0, 0 };

  ClearScreen ();
  WriteCentre (134, menutitle);

  for (i = 0; i < MAXCODES; i++)
  {
	  if (i == ggrow)
	  {
		  /*** Highlight selected ***/
		  WriteCentre_HL (i * fheight + 224, ggcodes[i]);

		  /*** If editing, highlight the current character ***/
		  if (editing)
		  {
			  int hpos = 0;

  			  for (j=0; j<strlen (ggcodes[i]); j++) hpos += font_size[ggcodes[i][j]];
			  hpos = ((640 - hpos) >> 1);
			  for (j=0; j<gghpos[i]; j++) hpos += font_size[ggcodes[i][j]];

			  c[0] = ggcodes[i][gghpos[i]];
			  fntDrawBoxFilled (hpos, (i * fheight) + 224, hpos + font_size[c[0]],
								((i + 1) * fheight) + 224, COLOR_YELLOW);
			  setfontcolour (COLOR_BLUE);
			  write_font (hpos, (i * fheight) + 224, c);
			  setfontcolour (COLOR_WHITE);
		  }
	  }
	  else WriteCentre ((i * fheight) + 224, ggcodes[i]);
  }
  SetScreen ();
}

/****************************************************************************
 * GGEditLine
 *
 * Up/Down traverses valid character array
 * Left/Right moves along current line
 * A exits edit mode
 ****************************************************************************/
void GGEditLine ()
{
  short p;
  char c[2] = { 0, 0 };
  char *v;
  int redraw = 1;
  signed char x, y;

  /** Lose any previous A press **/
  while (PAD_ButtonsDown (0) & PAD_BUTTON_A);

  editing = 1;

  while (!(PAD_ButtonsDown (0) & PAD_BUTTON_A))
  {
	  if (redraw)
	  {
		  DrawGGCodes ();
		  redraw = 0;
	  }

	  p = PAD_ButtonsDown (0);
	  x = PAD_StickX (0);
	  y = PAD_StickY (0);

	  if ((p & PAD_BUTTON_UP) || (y > 70))
	  {
		  /*** Increment the entry ***/
		  redraw = 1;
		  c[0] = ggcodes[ggrow][gghpos[ggrow]];
		  v = strstr (ggvalidchars, c);
		  v++;
		  if (*v == '*') ggcodes[ggrow][gghpos[ggrow]] = 'A';
		  else ggcodes[ggrow][gghpos[ggrow]] = *v;
	  }

	  if ((p & PAD_BUTTON_DOWN) || (y < -70))
	  {
		  /*** Decrement entry ***/
		  redraw = 1;
		  c[0] = ggcodes[ggrow][gghpos[ggrow]];
		  v = strstr (ggvalidchars, c);
		  if (*v == 'A') ggcodes[ggrow][gghpos[ggrow]] = '9';
		  else
		  {
			  v--;
			  ggcodes[ggrow][gghpos[ggrow]] = *v;
		  }
	  }

	  if ((p & PAD_BUTTON_LEFT) || (x < -70))
	  {
		  redraw = 1;
		  gghpos[ggrow]--;
		  if (gghpos[ggrow] == 4) gghpos[ggrow]--;
	  }

	  if ((p & PAD_BUTTON_RIGHT) || (x > 70))
	  {
		  redraw = 1;
		  gghpos[ggrow]++;
		  if (gghpos[ggrow] == 4) gghpos[ggrow]++;
	  }

	  if (gghpos[ggrow] < 0) gghpos[ggrow] = 8;
	  if (gghpos[ggrow] > 8) gghpos[ggrow] = 0;
  }

  /** Lose any previous A press **/
  while (PAD_ButtonsDown (0) & PAD_BUTTON_A);

  editing = 0;
}

/****************************************************************************
 * GGSelectLine
 *
 * Select which line to edit
 ****************************************************************************/
void GGSelectLine ()
{
  int redraw = 1;
  int quit = 0;
  short j;
  signed char y;

  /*** To select a line, just move up or down.
		Pressing A will enter edit mode.
		Pressing B will exit to caller.   ***/

  while (!(PAD_ButtonsDown (0) & PAD_BUTTON_B) && (quit == 0))
  {
	  if (redraw)
	  {
		  DrawGGCodes ();
		  redraw = 0;
	  }

	  j = PAD_ButtonsDown (0);
	  y = PAD_StickY (0);

	  if ((j & PAD_BUTTON_UP) || (y > 70))
	  {
		  ggrow--;
		  redraw = 1;
	  }

	  if ((j & PAD_BUTTON_DOWN) || (y < -70))
	  {
		  ggrow++;
		  redraw = 1;
	  }

	  if (ggrow < 0) ggrow = MAXCODES - 1;
	  if (ggrow == MAXCODES) ggrow = 0;

	  if (j & PAD_BUTTON_B) quit = 1;

	  if (j & PAD_BUTTON_A)
	  {
		  GGEditLine ();
		  redraw = 1;
	  }
  }
}

/****************************************************************************
 * GetGGEntries
 *
 * Screen to return encoded Game Genie codes.
 * No keyboard is available, so it's just a simple wrap round each line kind
 * of thing. 
 ****************************************************************************/
void GetGGEntries ()
{
  editing = 0;
  strcpy (menutitle, "Game Genie Entry");
  GGSelectLine ();
}
