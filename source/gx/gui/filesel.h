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
  u32 length;
  char flags;
  char filename[MAXJOLIET];
}FILEENTRIES;

/* Globals */
extern int FileSelector(unsigned char *buffer, bool useFAT);
extern int FileSortCallback(const void *f1, const void *f2);
extern void ClearSelector(u32 max);
extern FILEENTRIES filelist[MAXFILES];

#endif
