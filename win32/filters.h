/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  filters.h -- built-in ("native") render filters.
 *
 *  Every filter takes an RGB565 frame and writes an RGB565 frame that is
 *  exactly `scale` times larger in both directions. Nothing in here touches
 *  Windows, so the whole module builds and tests on any platform, and it is
 *  identical for 32-bit and 64-bit builds.
 ****************************************************************************/

#ifndef _FILTERS_H_
#define _FILTERS_H_

#include <stdint.h>

/* Largest input frame a filter will be given. Must be at least as big as the
   frontend's FRAME_MAX_W / FRAME_MAX_H (video.c checks this at compile time). */
#define FILTER_MAX_W     720
#define FILTER_MAX_H     576

/* Largest output multiplier of any filter in the table. The caller sizes its
   destination buffer from this. GPGX_XBRZ (set by the Makefile unless
   XBRZ=0) adds the xBRZ filters, which go up to 6x. */
#ifdef GPGX_XBRZ
#define FILTER_MAX_SCALE 6
#else
#define FILTER_MAX_SCALE 4
#endif

/*
 * src, dst : RGB565 pixels.
 * *_pitch  : bytes per row (must be even for dst).
 * w, h     : size of the input frame. The output is (w*scale) x (h*scale)
 *            and is written starting at dst[0].
 */
typedef void (*filter_fn)(const uint16_t *src, int src_pitch, int w, int h,
                          uint16_t *dst, int dst_pitch);

#define FILTER_NEEDS_XBRZ 1   /* holds xBRZ's 64 MB table while active */
#define FILTER_SEP_BEFORE 2   /* menu: draw a separator above this entry */

typedef struct
{
  const char *name;   /* menu label; also what the config file stores */
  int         scale;  /* output size / input size, 2..FILTER_MAX_SCALE */
  filter_fn   run;
  int         flags;  /* FILTER_* */
} filter_def_t;

int                 filter_count(void);
const filter_def_t *filter_get(int index);                 /* NULL if out of range */
int                 filter_find(const char *name);         /* -1 if unknown */

/* Call when a filter is selected, before the first frame. Gets the filter
   ready (xBRZ builds its 64 MB table, which takes about a tenth of a second)
   and releases whatever the previously active filter was holding. Returns 0 if
   the filter cannot be used (not enough memory). Filters that need no set-up
   always return 1. Passing an out-of-range index releases everything. */
int                 filter_prepare(int index);

/* Runs filter `index` (no-op if out of range). Returns the scale factor the
   output was produced at, or 0 if nothing was written. */
int                 filter_apply(int index, const uint16_t *src, int src_pitch,
                                 int w, int h, uint16_t *dst, int dst_pitch);

/* Releases the scratch memory the filters keep between frames. */
void                filter_shutdown(void);

#endif /* _FILTERS_H_ */
