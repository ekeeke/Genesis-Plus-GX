
#ifndef _OSD_H_
#define _OSD_H_

#define NGC 1

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/dir.h>

#include <asndlib.h>
#include <oggplayer.h>

#ifdef HW_RVL
#include <di/di.h>
#include "vi_encoder.h"
#endif

#include "gx_input.h"
#include "gx_audio.h"
#include "gx_video.h"
#include "config.h"

#define DEFAULT_PATH  "/genplus"
#define GG_ROM        "/genplus/ggenie.bin"
#define AR_ROM        "/genplus/areplay.bin"
#define OS_ROM        "/genplus/bios.bin"
#define SK_ROM        "/genplus/sk.bin"
#define SK_UPMEM      "/genplus/sk2chip.bin"

#ifdef HW_RVL
#define VERSION "version 1.3.xW"
#else
#define VERSION "version 1.3.xG"
#endif

#define osd_input_Update() gx_input_UpdateEmu()

/* globals */
extern void error(char *format, ...);
extern void ClearGGCodes();
extern void GetGGEntries();
extern void MainMenu(void);
extern void legal();
extern void reloadrom (int size, char *name);
extern void shutdown();
extern u32 frameticker;
extern u32 Shutdown;
extern u32 ConfigRequested;

#endif /* _OSD_H_ */
