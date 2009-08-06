
#ifndef _CONFIG_H_
#define _CONFIG_H_

/****************************************************************************
 * Config Option 
 *
 ****************************************************************************/
typedef struct 
{
  uint8 padtype;
} t_input_c;

typedef struct 
{
  uint8 hq_fm;
  uint8 psgBoostNoise;
  int32 psg_preamp;
  int32 fm_preamp;
  uint8 filter;
  uint16 low_freq;
  uint16 high_freq;
  float lg;
  float mg;
  float hg;
  uint8 region_detect;
  uint8 force_dtack;
  uint8 addr_error;
  uint8 bios_enabled;
  uint8 lock_on;
  uint8 romtype;
  uint8 overscan;
  uint8 render;
  uint8 ntsc;
  t_input_c input[MAX_INPUTS];
  uint8 gun_cursor[2];
  uint8 invert_mouse;
} t_config;

/* Global variables */
extern t_config config;

#endif /* _CONFIG_H_ */

