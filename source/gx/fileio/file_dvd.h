/*
 * file_dvd.c
 * 
 *   generic ISO9660/Joliet DVD loading support
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

#ifndef _FILE_DVD_H
#define _FILE_DVD_H

extern int DVD_UpdateDir(int go_up);
extern int DVD_ParseDirectory();
extern int DVD_LoadFile(u8 *buffer);
extern int DVD_Open (u8 *buffer);

#endif
