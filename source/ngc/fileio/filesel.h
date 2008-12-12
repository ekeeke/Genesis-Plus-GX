/*
 * filesel.c
 * 
 *   File Selection menu
 *
 *   code by Softdev (2006), Eke-Eke (2007,2008) 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ********************************************************************************/

#ifndef _FILESEL_H
#define _FILESEL_H

#define MAXJOLIET 256
#define MAXFILES 1000

/* this is emulator specific ! */
#define PAGESIZE 12
#define PAGEOFFSET 120


/* Filelist structure */
typedef struct
{
  u64 offset;
  unsigned int length;
  char flags;
  char filename[MAXJOLIET];
  u16 filename_offset;
}FILEENTRIES;


/* Global Variables */
extern FILEENTRIES filelist[MAXFILES];
extern char rom_filename[MAXJOLIET];
extern int maxfiles;
extern int offset;
extern int selection;
extern int old_selection;
extern int old_offset;
extern int useFAT;
extern int haveDVDdir;
extern int haveFATdir;

extern int FileSelector(unsigned char *buffer);

#endif
