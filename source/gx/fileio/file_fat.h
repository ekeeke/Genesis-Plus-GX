/*
 * file_fat.c
 * 
 *   generic FAT loading support
 *
 *   code by Eke-Eke (2008) 
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

#ifndef _FILE_FAT_H
#define _FILE_FAT_H

#define TYPE_RECENT   0
#define TYPE_SD       1

#ifdef HW_RVL
#define TYPE_USB      2
#endif

extern int FAT_UpdateDir(int go_up);
extern int FAT_ParseDirectory(void);
extern int FAT_LoadFile(u8* buffer);
extern int FAT_Open (int type, u8 *buffer);

#endif
