
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

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "gx_input.h"
#include "gx_audio.h"
#include "gx_video.h"
#include "config.h"

#define DEFAULT_PATH "/genplus"
#ifdef HW_RVL
#define VERSION "version 1.3.2W"
#else
#define VERSION "version 1.3.2G"
#endif

/* globals */
extern void error(char *format, ...);
extern void ClearGGCodes();
extern void GetGGEntries();
extern void legal();

extern void reloadrom (int size, char *name);
extern void shutdown();

extern int ManageSRAM(u8 direction, u8 device);
extern int ManageState(u8 direction, u8 device);
extern void memfile_autosave(s8 autosram, s8 autostate);
extern void memfile_autoload(s8 autosram, s8 autostate);

extern u32 frameticker;
extern char rom_filename[256];

#ifdef HW_RVL
extern u8 Shutdown;
#endif

#endif /* _OSD_H_ */
