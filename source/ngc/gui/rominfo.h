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


typedef struct
{
  char consoletype[18];		/* Genesis or Mega Drive */
  char copyright[18];		/* Copyright message */
  char domestic[50];		/* Domestic name of ROM */
  char international[50];	/* International name of ROM */
  char ROMType[4];
  char product[14];		/* Product type and serial number */
  unsigned short checksum;	/* Checksum */
  char io_support[18];		/* Actually 16 chars :) */
  unsigned int romstart;
  unsigned int romend;
  char RAMInfo[14];
  unsigned int ramstart;
  unsigned int ramend;
  char modem[14];
  char memo[50];
  char country[18];
} ROMINFO;


extern void getrominfo (char *romheader);
extern void showrominfo ();
extern ROMINFO rominfo;
