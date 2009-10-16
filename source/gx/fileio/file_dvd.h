/*
 * file_dvd.c
 * 
 *  ISO9660/Joliet DVD loading support
 *
 *  Softdev (2006)
 *  Eke-Eke (2007,2008,2009)
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
 ********************************************************************************/

#ifndef _FILE_DVD_H
#define _FILE_DVD_H

#define DVDCHUNK (2048)

extern void DVD_ClearDirectory(void);
extern int DVD_UpdateDirectory(bool go_up,u64 offset, u32 length);
extern int DVD_ParseDirectory(void);
extern int DVD_LoadFile(u8 *buffer,u32 selection);
extern int DVD_Open(void);

#endif
