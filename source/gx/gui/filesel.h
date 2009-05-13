/*
 * filesel.c
 * 
 *   File Selection menu
 *
 *   Softdev (2006)
 *   Eke-Eke (2007,2008,2009) 
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
extern int maxfiles;
extern int offset;
extern int selection;
extern int old_selection;
extern int old_offset;
extern int useFAT;
extern int haveDVDdir;
extern int haveFATdir;

extern int FileSelector(unsigned char *buffer);
extern int FileSortCallback(const void *f1, const void *f2);

#endif
