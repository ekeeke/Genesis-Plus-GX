/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  coverart.h -- loading, caching and managing per-ROM cover images from
 *  a "covers" folder next to the ROM library, matched by base filename
 *  (extension-independent, so "Sonic.zip" and "Sonic.bin" share one cover).
 ****************************************************************************/

#ifndef _COVERART_H_
#define _COVERART_H_

#include <windows.h>

/* Covers live in <exe folder>\covers\<Console>\<base name>.<ext>, where
   <Console> is MegaDrive, MasterSystem, GameGear, SG1000, MegaCD or Other,
   taken from the browser's console label passed as `console` (e.g. "Master
   System"). A cover in the flat covers\ folder used by earlier versions is
   still found when there is none in the console's folder.

   Computes where a cover for this ROM would be written: the .png path in the
   console's folder (the base name only, no subfolder path). Always
   succeeds (writes the .png path even if no cover exists yet there); the
   caller checks existence itself if it needs to know. */
void coverart_path_for_rom(const char *rom_full_path, const char *console, char *out, int out_len);

/* Looks for a cover for this ROM, trying a few common image extensions
   against the same base name. Returns 1 and fills found_path if one
   exists, 0 otherwise. */
int coverart_find(const char *rom_full_path, const char *console, char *found_path, int out_len);

/* Sentinel for a cover_cache slot that hasn't been checked yet. */
#define COVERART_UNCHECKED (-2)

/* Same result as coverart_find, but remembers it in *cache (a small
   per-ROM slot the caller owns, initialized to COVERART_UNCHECKED)
   so repeated calls -- e.g. once per zoom level as the grid rebuilds
   its thumbnails -- reuse the answer instead of re-touching the disk
   every time. Only the very first call for a given ROM does any real
   filesystem work; every call after that is a plain string build. */
int coverart_find_cached(const char *rom_full_path, const char *console,
                         signed char *cache, char *found_path, int out_len);

/* Loads and returns a square HBITMAP (top-down 32bpp DIB section) sized
   exactly size x size, letterboxed to preserve the source image's aspect
   ratio, from whatever cover file coverart_find() would locate for this
   ROM. Returns NULL if no cover exists or it fails to decode -- caller
   falls back to a placeholder. Every returned bitmap is a fresh
   CreateDIBSection the caller owns and must DeleteObject when done;
   this module does not cache bitmaps itself (the browser's image list
   is the cache).*/
HBITMAP coverart_load(const char *rom_full_path, const char *console, int size);

/* Same as coverart_load, but backed by coverart_find_cached instead of
   coverart_find -- ROMs with no cover skip the filesystem lookup
   entirely on every call after the first. */
HBITMAP coverart_load_cached(const char *rom_full_path, const char *console, int size, signed char *cache);

/* Copies a user-picked image file to be this ROM's cover, preserving
   its original extension (coverart_find tries several extensions when
   looking one up, so this doesn't need to normalize format). Any
   existing cover for this ROM under a different extension is removed
   first, so there's never more than one on disk per ROM. Returns 1 on
   success. */
int coverart_set(const char *rom_full_path, const char *console, const char *source_image_path);

/* Deletes whatever cover file coverart_find() would locate for this ROM,
   if any. Returns 1 if a file was actually removed. */
int coverart_remove(const char *rom_full_path, const char *console);

#endif
