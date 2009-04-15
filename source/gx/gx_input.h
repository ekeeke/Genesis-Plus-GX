/****************************************************************************
 *  gx_input.c
 *
 *  Genesis Plus GX input support
 *
 *  code by Eke-Eke (2008)
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
 ***************************************************************************/

#ifndef _GC_INPUT_H_
#define _GC_INPUT_H_

/* max. supported inputs */
#ifdef HW_DOL
#define MAX_INPUTS 4
#else
#define MAX_INPUTS 8
#endif

/* number of configurable keys */
#define MAX_KEYS 8

/* wrapper for genesis plus */
#define update_input() gx_input_updateEmu()

/* key configuration structure */
typedef struct 
{
  s8 device;
  u8 port;
} t_input_config;

extern u8 ConfigRequested;

extern void gx_input_init(void);
extern void gx_input_setDefault(void);
extern void gx_input_config(u8 num, u8 type, u8 padtype);
extern void gx_input_updateEmu(void);
extern void gx_input_updateMenu(u32 cnt);

extern s8 WPAD_StickX(u8 chan,u8 right);
extern s8 WPAD_StickY(u8 chan,u8 right);

#endif
