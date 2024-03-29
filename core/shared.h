#ifndef _SHARED_H_
#define _SHARED_H_

#include "genesis.h"
#include "vdp_ctrl.h"
#include "vdp_render.h"
#include "mem68k.h"
#include "memz80.h"
#include "membnk.h"
#include "io_ctrl.h"
#include "input.h"
#include "sound.h"
#include "psg.h"
#include "ym2413.h"
#include "ym2612.h"
#ifdef HAVE_YM3438_CORE
#include "ym3438.h"
#endif
#ifdef HAVE_OPLL_CORE
#include "opll.h"
#endif
#include "sram.h"
#include "ggenie.h"
#include "areplay.h"
#include "svp.h"
#include "state.h"

#endif /* _SHARED_H_ */

