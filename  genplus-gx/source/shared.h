#ifndef _SHARED_H_
#define _SHARED_H_


#include <stdio.h>
#include <math.h>
#include <zlib.h>

#include "types.h"
#include "macros.h"
#include "m68k.h"
#include "z80.h"
#include "genesis.h"
#include "vdp.h"
#include "render.h"
#include "mem68k.h"
#include "memz80.h"
#include "membnk.h"
#include "memvdp.h"
#include "system.h"
#ifndef NGC
#include "unzip.h"
#include "fileio.h"
#include "loadrom.h"
#endif
#include "io.h"
#include "input.h"
#include "sound.h"
#include "fm.h"
#include "sn76496.h"
#include "osd.h"
#include "state.h"
#include "sram.h"
#include "eeprom.h"
#include "ssf2tnc.h"
#include "sn76489.h"
#include "ym2612.h"

extern uint8 FM_GENS;
extern uint8 PSG_MAME;

#endif /* _SHARED_H_ */

