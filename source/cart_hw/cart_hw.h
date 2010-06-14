/****************************************************************************
 *  Genesis Plus
 *  Cartridge Hardware support
 *
 *  Copyright (C) 2007, 2008, 2009  Eke-Eke (GCN/Wii port)
 *
 *  Lots of protection mechanism have been discovered by Haze
 *  (http://haze.mameworld.info/)
 *
 *  Realtec mapper has been figured out by TascoDeluxe
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
 ***************************************************************************/

#ifndef _CART_HW_H_
#define _CART_HW_H_

/* Lock-ON cartridge type */
#define TYPE_GG 0x01  /* Game Genie */
#define TYPE_AR 0x02  /* Action Replay (Pro) */
#define TYPE_SK 0x03  /* Sonic & Knuckles */

/* Cartridge extra hardware */
typedef struct
{
  uint8 regs[4];                                            /* internal registers (R/W) */
  uint32 mask[4];                                           /* registers address mask */
  uint32 addr[4];                                           /* registers address */
  uint16 realtec;                                           /* bit 0: realtec mapper detected, bit 1: bootrom enabled */
  uint16 bankshift;                                         /* cartridge with bankshift mecanism */
  unsigned int (*time_r)(unsigned int address);             /* !TIME signal ($a130xx) read handler  */
  void (*time_w)(unsigned int address, unsigned int data);  /* !TIME signal ($a130xx) write handler */
  unsigned int (*regs_r)(unsigned int address);             /* cart hardware registers read handler  */
  void (*regs_w)(unsigned int address, unsigned int data);  /* cart hardware registers write handler */
} T_CART_HW;

/* Cartridge type */
typedef struct
{
  uint8 *rom;       /* ROM data */
  uint8 *base;      /* ROM base (slot 0) */
  uint32 romsize;   /* ROM size */
  uint32 mask;      /* ROM mask */
  uint8 lock_on;    /* 1: Lock-On port */
  uint8 jcart;      /* 1: J-CART port */
  T_CART_HW hw;     /* Extra hardware */
} T_CART;

/* global variables */
extern T_CART cart;

/* Function prototypes */
extern void cart_hw_init();
extern void cart_hw_reset();

#endif


