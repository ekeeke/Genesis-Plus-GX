
#include "osd.h"

t_config config;


void set_config_defaults(void)
{
  /* sound options */
  config.psg_preamp   = 150;
  config.fm_preamp    = 100;
  config.boost        = 1;
  config.filter       = 1;
  config.hq_fm        = 1;

  /* system options */
  config.region_detect  = 0;
  config.force_dtack    = 0;
  config.bios_enabled   = 0;

  /* display options */
  config.aspect   = 1;
  config.overscan = 1;
  config.render   = 1;
  config.ntsc     = 0;

  /* controllers options */
  config.gun_cursor   = 1;
  config.invert_mouse = 0;
}

