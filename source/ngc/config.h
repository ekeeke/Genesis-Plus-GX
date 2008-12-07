
#ifndef _CONFIG_H_
#define _CONFIG_H_

/****************************************************************************
 * Config Option 
 *
 ****************************************************************************/
typedef struct 
{
  char version[15];
  double psg_preamp;
  double fm_preamp;
  uint8 boost;
  uint8 filter;
  uint8 hq_fm;
  uint8 fm_core;
  int8 sram_auto;
  int8 freeze_auto;
  uint8 region_detect;
  uint8 force_dtack;
  uint8 bios_enabled;
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
  uint16 pad_keymap[4][MAX_KEYS];
  uint32 wpad_keymap[4*3][MAX_KEYS];
  t_input_config input[MAX_DEVICES];
  uint8 gun_cursor;
  uint8 invert_mouse;
} t_config;

/* Global data */
t_config config;


extern void config_save();
extern void config_load();
extern void set_config_defaults(void);
extern bool use_FAT;

#endif /* _CONFIG_H_ */

