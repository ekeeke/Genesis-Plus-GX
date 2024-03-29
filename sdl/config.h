
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "types.h"
#include <main.h>

/****************************************************************************
 * Config Option 
 *
 ****************************************************************************/
typedef struct 
{
  uint8_t padtype;
} t_input_config;

typedef struct 
{
  uint8_t hq_fm;
  uint8_t filter;
  uint8_t hq_psg;
  uint8_t ym2612;
  uint8_t ym2413;
  uint8_t ym3438;
  uint8_t opll;
  uint8_t cd_latency;
  int16_t psg_preamp;
  int16_t fm_preamp;
  int16_t cdda_volume;
  int16_t pcm_volume;
  uint32_t lp_range;
  int16_t low_freq;
  int16_t high_freq;
  int16_t lg;
  int16_t mg;
  int16_t hg;
  uint8_t mono;
  uint8_t system;
  uint8_t region_detect;
  uint8_t vdp_mode;
  uint8_t master_clock;
  uint8_t force_dtack;
  uint8_t addr_error;
  uint8_t bios;
  uint8_t lock_on;
  uint8_t add_on;
  uint8_t hot_swap;
  uint8_t invert_mouse;
  uint8_t gun_cursor[2];
  uint8_t overscan;
  uint8_t gg_extra;
  uint8_t ntsc;
  uint8_t lcd;
  uint8_t render;
  uint8_t enhanced_vscroll;
  uint8_t enhanced_vscroll_limit;
  t_input_config input[MAX_INPUTS];
} t_config;

/* Global variables */
extern t_config config;
extern void set_config_defaults(void);

#endif /* _CONFIG_H_ */

