#ifndef LIBINPUT_INTERNAL_H
#define LIBINPUT_INTERNAL_H

/* Trimmed from libwiiinput: only what the pen tablet, uDraw and GameCube adapter drivers need. */

#include <gctypes.h>
#include <stdbool.h>
#include "input_tablet.h"
#include "input_wii.h"

bool inpw_udraw_pop(tb_sample *out);
bool inpw_udraw_state(tb_sample *out);
bool inpw_udraw_info(tb_info *info);
void inpw_udraw_extent(s32 *w, s32 *h);

/* the GameCube controller adapter for Wii U, for the INPUT_GCAdapter* calls in libinput_gcadapter.c */
#ifdef HAVE_GCADAPTER
void inpw_gcadapter_init(bool enabled);
void inpw_gcadapter_shutdown(void);
void inpw_gcadapter_update(void);
#endif

#endif
