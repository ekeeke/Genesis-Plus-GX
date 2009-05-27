
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
#endif

#include "gx_input.h"
#include "gx_audio.h"
#include "gx_video.h"
#include "config.h"
#include "file_mem.h"

#define DEFAULT_PATH "/genplus"
#ifdef HW_RVL
#define VERSION "version 1.3.2bW"
#else
#define VERSION "version 1.3.2bG"
#endif

/* globals */
extern void error(char *format, ...);
extern void ClearGGCodes();
extern void GetGGEntries();
extern void MainMenu(void);
extern void legal();
extern void reloadrom (int size, char *name);
extern void shutdown();
extern u32 frameticker;
extern u8 Shutdown;

#endif /* _OSD_H_ */
