/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  main.h -- declarations the emulator core expects from the OS layer.
 *
 *  Keep this file small: it is pulled in by osd.h, which the whole core
 *  includes through shared.h. Frontend-internal declarations belong in gui.h.
 ****************************************************************************/

#ifndef _MAIN_H_
#define _MAIN_H_

#define MAX_INPUTS 8

/* Set when the user enables logging; read by error(). */
extern int log_error;

/* Unused by this port, but the core references it in a few debug paths. */
extern int debug_on;

/* Called by the core once per frame to refresh input.pad[] / input.analog[]. */
extern int win32_input_update(void);

/*
 * Resolves a filename against the directory the executable lives in and
 * returns a usable path. The core passes the BIOS macros below straight to
 * load_archive(), which takes a non-const char*.
 *
 * The returned pointer is one of a small rotating set of static buffers, so
 * it stays valid across a handful of calls. Copy it if you need to keep it.
 */
extern char *osd_path(const char *filename);

#endif /* _MAIN_H_ */
