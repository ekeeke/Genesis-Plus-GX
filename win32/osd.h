/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  osd.h -- the contract between the emulator core and the host.
 *
 *  Included by core/shared.h, so every core translation unit sees it.
 ****************************************************************************/

#ifndef _OSD_H_
#define _OSD_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "main.h"
#include "config.h"
#include "error.h"
#include "unzip.h"
#include "fileio.h"

/* The core calls this once per frame to sample the host input devices. */
#define osd_input_update win32_input_update

/*
 * Defining this makes the 8-bit mappers in sms_cart.c call back into the
 * frontend whenever they switch banks. ROM cheat patches are written into the
 * banked window, so without this hook they would be lost on the next bank
 * switch and silently stop working.
 */
#define CHEATS_UPDATE() ROMCheatUpdate()
extern void ROMCheatUpdate(void);

/*
 * Add-on ROMs and BIOS images.
 *
 * The SDL port hardcodes "./name.bin", which resolves against the working
 * directory -- so it breaks the moment the user launches from a shortcut or
 * drops a ROM on the .exe. These resolve against the executable's own folder
 * instead, under a "bios" subdirectory.
 */
#define GG_ROM      osd_path("bios\\ggenie.bin")
#define AR_ROM      osd_path("bios\\areplay.bin")
#define SK_ROM      osd_path("bios\\sk.bin")
#define SK_UPMEM    osd_path("bios\\sk2chip.bin")
#define CD_BIOS_US  osd_path("bios\\bios_CD_U.bin")
#define CD_BIOS_EU  osd_path("bios\\bios_CD_E.bin")
#define CD_BIOS_JP  osd_path("bios\\bios_CD_J.bin")
#define MD_BIOS     osd_path("bios\\bios_MD.bin")
#define MS_BIOS_US  osd_path("bios\\bios_U.sms")
#define MS_BIOS_EU  osd_path("bios\\bios_E.sms")
#define MS_BIOS_JP  osd_path("bios\\bios_J.sms")
#define GG_BIOS     osd_path("bios\\bios.gg")

#endif /* _OSD_H_ */
