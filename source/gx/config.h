/****************************************************************************
 *  config.c
 *
 *  Genesis Plus GX configuration file support
 *
 *  Eke-Eke (2008)
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

#ifndef _CONFIG_H_
#define _CONFIG_H_

/****************************************************************************
 * Config Option 
 *
 ****************************************************************************/
typedef struct 
{
  char version[16];
  uint8 hq_fm;
  uint8 psgBoostNoise;
  int32 psg_preamp;
  int32 fm_preamp;
  uint8 filter;
  int16 lp_range;
  int16 low_freq;
  int16 high_freq;
  float lg;
  float mg;
  float hg;
  float rolloff;
  uint8 dac_bits;
  uint8 region_detect;
  uint8 force_dtack;
  uint8 addr_error;
  uint8 bios_enabled;
  uint8 lock_on;
  uint8 hot_swap;
  uint8 romtype;
  int16 xshift;
  int16 yshift;
  int16 xscale;
  int16 yscale;
  uint8 tv_mode;
  uint8 aspect;
  uint8 overscan;
  uint8 render;
  uint8 ntsc;
  uint8 bilinear;
#ifdef HW_RVL
  uint8 trap;
  float gamma;
#endif
  uint8 gun_cursor[2];
  uint8 invert_mouse;
  uint16 pad_keymap[4][MAX_KEYS];
  uint32 wpad_keymap[4*3][MAX_KEYS];
  t_input_config input[MAX_INPUTS];
  uint8 s_auto;
  uint8 s_default;
  uint8 s_device;
  int8 bg_color;
  int8 bg_overlay;
  int16 screen_w;
  uint8 ask_confirm;
  float bgm_volume;
  float sfx_volume;
} t_config;

/* Global data */
t_config config;


extern void config_save(void);
extern void config_load(void);
extern void config_default(void);


#endif /* _CONFIG_H_ */

