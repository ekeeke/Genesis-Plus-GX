
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

#include "ogc_input.h"
#include "ogc_audio.h"
#include "ogc_video.h"
#include "config.h"

#define update_input() ogc_input__update()

#define DEFAULT_PATH "/genplus"

/* globals */
extern u32 diff_usec(long long start,long long end);
extern long long gettime();
extern void error(char *format, ...);
extern int getcompany();
extern void reloadrom();
extern void ClearGGCodes();
extern void GetGGEntries();
extern void legal();
extern void MainMenu(u32 fps);
extern void set_region();
extern int ManageSRAM(u8 direction, u8 device);
extern int ManageState(u8 direction, u8 device);
extern void memfile_autosave();
extern void memfile_autoload();

extern int peripherals;
extern int frameticker;
extern int Shutdown;
extern bool fat_enabled;

#endif /* _OSD_H_ */
