
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

#define DEFAULT_PATH "/genplus"

/* globals */
extern void error(char *format, ...);
extern void ClearGGCodes();
extern void GetGGEntries();
extern void reloadrom();
extern void legal();
extern void MainMenu(u32 fps);
extern int ManageSRAM(u8 direction, u8 device);
extern int ManageState(u8 direction, u8 device);
extern void memfile_autosave();
extern void memfile_autoload();
extern void menu_updateInputs(u32 cnt);

extern u8 fat_enabled;
extern u32 frameticker;

#ifdef HW_RVL
extern u8 Shutdown;
#endif

#endif /* _OSD_H_ */
