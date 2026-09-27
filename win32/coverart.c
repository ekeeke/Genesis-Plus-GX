/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  coverart.c -- see coverart.h. Decoding is stb_image (public domain,
 *  single header, bundled in this directory) rather than GDI+/WIC: both
 *  of those are COM/C++-flavored APIs that are painful to drive cleanly
 *  from plain C under MinGW, where this whole frontend already lives.
 ****************************************************************************/

#include <windows.h>
#include <stdio.h>

#include "shared.h"
#include "gui.h"
#include "coverart.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include "stb_image.h"

/* Tried in this order against the same base name; whichever exists
   first wins. PNG first since that's what the feature was asked for
   and what coverart_set writes by default. */
static const char *cover_extensions[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif" };
#define COVER_EXT_COUNT (int)(sizeof(cover_extensions) / sizeof(cover_extensions[0]))

/* Same base-name extraction main.c's split_basename does (strip
   directory and extension), kept as its own small copy here rather
   than exposing that static function across files for one caller. */
static void base_name_only(const char *path, char *out, int out_len)
{
  const char *slash, *dot;
  const char *name;
  int len;

  slash = strrchr(path, '\\');
  name = slash ? slash + 1 : path;

  dot = strrchr(name, '.');
  len = dot ? (int)(dot - name) : (int)lstrlenA(name);
  if (len >= out_len) len = out_len - 1;

  memcpy(out, name, (size_t)len);
  out[len] = '\0';
}

static void covers_dir(char *out, int out_len)
{
  lstrcpynA(out, osd_path("covers"), out_len);
}

/* Folder name for a console, from the browser's console label. Matches the
   folder names used for saves, states and cheats. */
static const char *console_dir_name(const char *console)
{
  if (!console) return "Other";
  if (!lstrcmpiA(console, "Mega Drive / Genesis"))  return "MegaDrive";
  if (!lstrcmpiA(console, "Master System"))         return "MasterSystem";
  if (!lstrcmpiA(console, "Game Gear"))             return "GameGear";
  if (!lstrcmpiA(console, "SG-1000"))               return "SG1000";
  if (!lstrcmpiA(console, "Mega CD / Sega CD"))     return "MegaCD";
  return "Other";
}

/* covers\<Console> */
static void console_covers_dir(const char *console, char *out, int out_len)
{
  char tmp[GUI_PATH_LEN * 2];

  wsprintfA(tmp, "%s\\%s", osd_path("covers"), console_dir_name(console));
  lstrcpynA(out, tmp, out_len);
}

void coverart_path_for_rom(const char *rom_full_path, const char *console, char *out, int out_len)
{
  char dir[GUI_PATH_LEN];
  char base[160];
  char tmp[GUI_PATH_LEN * 2];

  console_covers_dir(console, dir, sizeof(dir));
  base_name_only(rom_full_path, base, sizeof(base));
  wsprintfA(tmp, "%s\\%s.png", dir, base);
  lstrcpynA(out, tmp, out_len);
}

static void build_candidate(const char *dir, const char *base, int ext_index, char *out, int out_len)
{
  char tmp[GUI_PATH_LEN * 2];

  wsprintfA(tmp, "%s\\%s%s", dir, base, cover_extensions[ext_index]);
  lstrcpynA(out, tmp, out_len);
}

/* Cache values: -2 unchecked, -1 none, 0..4 = extension found in the console's
   folder, COVER_FLAT + 0..4 = extension found in the flat covers folder that
   earlier versions used. */
#define COVER_FLAT 16

int coverart_find_cached(const char *rom_full_path, const char *console,
                         signed char *cache, char *found_path, int out_len)
{
  char cdir[GUI_PATH_LEN], fdir[GUI_PATH_LEN];
  char base[160];
  int i;

  if (*cache == -1) return 0;   /* already known: no cover on disk for this ROM */

  console_covers_dir(console, cdir, sizeof(cdir));
  covers_dir(fdir, sizeof(fdir));
  base_name_only(rom_full_path, base, sizeof(base));

  if (*cache >= COVER_FLAT)
  {
    build_candidate(fdir, base, *cache - COVER_FLAT, found_path, out_len);
    return 1;
  }
  if (*cache >= 0)
  {
    build_candidate(cdir, base, *cache, found_path, out_len);
    return 1;
  }

  /* COVERART_UNCHECKED: look in the console's folder first, then in the flat
     folder, and remember the answer either way. */
  for (i = 0; i < COVER_EXT_COUNT; i++)
  {
    char candidate[GUI_PATH_LEN];
    build_candidate(cdir, base, i, candidate, sizeof(candidate));
    if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
    {
      *cache = (signed char)i;
      lstrcpynA(found_path, candidate, out_len);
      return 1;
    }
  }
  for (i = 0; i < COVER_EXT_COUNT; i++)
  {
    char candidate[GUI_PATH_LEN];
    build_candidate(fdir, base, i, candidate, sizeof(candidate));
    if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
    {
      *cache = (signed char)(COVER_FLAT + i);
      lstrcpynA(found_path, candidate, out_len);
      return 1;
    }
  }

  *cache = -1;
  return 0;
}

int coverart_find(const char *rom_full_path, const char *console, char *found_path, int out_len)
{
  signed char throwaway = COVERART_UNCHECKED;
  return coverart_find_cached(rom_full_path, console, &throwaway, found_path, out_len);
}

/* Simple nearest-neighbor letterbox fit into a size x size square --
   thumbnails in a grid are small enough that this looks fine, and it
   keeps this file's one job (get a cover into a control) from growing
   into a general-purpose image resampler. Padding is left fully
   transparent (alpha 0) so the control's own background shows through
   whatever the source image's own aspect ratio doesn't fill. */
static HBITMAP load_and_fit(const char *path, int size)
{
  int src_w, src_h, channels;
  unsigned char *pixels;
  HBITMAP hbmp;
  BITMAPINFO bmi;
  unsigned char *dib_bits;
  int fit_w, fit_h, off_x, off_y;
  int x, y;

  pixels = stbi_load(path, &src_w, &src_h, &channels, 4);
  if (!pixels) return NULL;

  if (src_w >= src_h)
  {
    fit_w = size;
    fit_h = (int)((long)size * src_h / src_w);
  }
  else
  {
    fit_h = size;
    fit_w = (int)((long)size * src_w / src_h);
  }
  if (fit_w < 1) fit_w = 1;
  if (fit_h < 1) fit_h = 1;
  off_x = (size - fit_w) / 2;
  off_y = (size - fit_h) / 2;

  ZeroMemory(&bmi, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = size;
  bmi.bmiHeader.biHeight = -size;   /* negative = top-down, matches how we fill it below */
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&dib_bits, NULL, 0);
  if (!hbmp)
  {
    stbi_image_free(pixels);
    return NULL;
  }

  ZeroMemory(dib_bits, (size_t)size * size * 4);   /* transparent letterbox bars */

  for (y = 0; y < fit_h; y++)
  {
    int src_y = y * src_h / fit_h;
    unsigned char *dst_row = dib_bits + ((size_t)(off_y + y) * size + off_x) * 4;
    const unsigned char *src_row = pixels + (size_t)src_y * src_w * 4;

    for (x = 0; x < fit_w; x++)
    {
      int src_x = x * src_w / fit_w;
      const unsigned char *sp = src_row + (size_t)src_x * 4;
      unsigned char *dp = dst_row + (size_t)x * 4;

      /* stb_image gives RGBA; DIBs want BGRA. */
      dp[0] = sp[2];
      dp[1] = sp[1];
      dp[2] = sp[0];
      dp[3] = sp[3];
    }
  }

  stbi_image_free(pixels);
  return hbmp;
}

HBITMAP coverart_load(const char *rom_full_path, const char *console, int size)
{
  char path[GUI_PATH_LEN];
  if (!coverart_find(rom_full_path, console, path, sizeof(path))) return NULL;
  return load_and_fit(path, size);
}

HBITMAP coverart_load_cached(const char *rom_full_path, const char *console, int size, signed char *cache)
{
  char path[GUI_PATH_LEN];
  if (!coverart_find_cached(rom_full_path, console, cache, path, sizeof(path))) return NULL;
  return load_and_fit(path, size);
}

/* Index into cover_extensions of a file name's extension, or -1. */
static int cover_ext_index(const char *file)
{
  const char *dot = strrchr(file, '.');
  int i;

  if (!dot) return -1;
  for (i = 0; i < COVER_EXT_COUNT; i++)
  {
    if (!lstrcmpiA(dot, cover_extensions[i])) return i;
  }
  return -1;
}

int coverart_set(const char *rom_full_path, const char *console, const char *source_image_path)
{
  char dir[GUI_PATH_LEN];
  char base[160];
  char dest[GUI_PATH_LEN];
  int ext = cover_ext_index(source_image_path);
  int i;

  /* Only formats the finder looks for: anything else would be copied and then
     never shown. */
  if (ext < 0) return 0;

  CreateDirectoryA(osd_path("covers"), NULL);
  console_covers_dir(console, dir, sizeof(dir));
  CreateDirectoryA(dir, NULL);

  base_name_only(rom_full_path, base, sizeof(base));
  build_candidate(dir, base, ext, dest, sizeof(dest));

  /* Choosing the cover that is already in place: nothing to do (and nothing
     to delete). */
  if (lstrcmpiA(source_image_path, dest) != 0)
  {
    /* Copy first; the old cover is only replaced once this has worked. */
    if (!CopyFileA(source_image_path, dest, FALSE)) return 0;
  }

  /* Never more than one cover per game in this folder. */
  for (i = 0; i < COVER_EXT_COUNT; i++)
  {
    char other[GUI_PATH_LEN];
    if (i == ext) continue;
    build_candidate(dir, base, i, other, sizeof(other));
    DeleteFileA(other);
  }
  return 1;
}

/* Deletes the game's cover(s) in its console folder and any in the flat folder
   from earlier versions (which would otherwise show through again). */
int coverart_remove(const char *rom_full_path, const char *console)
{
  char cdir[GUI_PATH_LEN], fdir[GUI_PATH_LEN], base[160];
  int pass, i, removed = 0;

  console_covers_dir(console, cdir, sizeof(cdir));
  covers_dir(fdir, sizeof(fdir));
  base_name_only(rom_full_path, base, sizeof(base));

  for (pass = 0; pass < 2; pass++)
  {
    for (i = 0; i < COVER_EXT_COUNT; i++)
    {
      char p[GUI_PATH_LEN];
      build_candidate(pass ? fdir : cdir, base, i, p, sizeof(p));
      if (DeleteFileA(p)) removed = 1;
    }
  }
  return removed;
}
