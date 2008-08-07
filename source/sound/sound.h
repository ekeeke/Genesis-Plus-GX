/***************************************************************************************
 *  Genesis Plus 1.2a
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  modified by Eke-Eke (compatibility fixes & additional code), GC/Wii port
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
 *  Sound Hardware
 ****************************************************************************************/

#ifndef _SOUND_H_
#define _SOUND_H_

/* Global variables */
extern int fm_reg[2][0x100];
extern double fm_timera_tab[0x400];
extern double fm_timerb_tab[0x100];

/* Function prototypes */
extern void sound_init(int rate);
extern void sound_update(void);
extern void fm_restore(void);
extern void fm_write(unsigned int cpu, unsigned int  address, unsigned int  data);
extern unsigned int fm_read(unsigned int  cpu, unsigned int  address);
extern void psg_write(unsigned int  cpu, unsigned int  data);
extern int (*_YM2612_Reset)(void);

#endif /* _SOUND_H_ */
