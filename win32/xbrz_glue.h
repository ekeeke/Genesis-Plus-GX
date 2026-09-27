/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  xbrz_glue.h -- C interface to the xBRZ scaler (xbrz/).
 *
 *  xBRZ is C++ and works on 32-bit pixels; the rest of the frontend is C and
 *  works on RGB565. This is the whole of the boundary between them. It is
 *  only built when the Makefile's XBRZ option is on (the default), which
 *  defines GPGX_XBRZ.
 *
 *  NOTE ON LICENSING: xbrz/ is GPLv3. See xbrz/README-xbrz.txt before
 *  distributing a binary that contains it.
 ****************************************************************************/

#ifndef _XBRZ_GLUE_H_
#define _XBRZ_GLUE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Builds xBRZ's 64 MB colour-distance table (takes about a tenth of a second
   the first time; no-op afterwards). Returns 0 if the memory isn't available. */
int  xbrz_glue_prepare(void);

/* Frees the table and every scratch buffer. Safe to call at any time. */
void xbrz_glue_release(void);

/* Scales an RGB565 frame by `factor` (2..6) with xBRZ. Same conventions as a
   filter_fn: pitches are in bytes, the output is (w*factor) x (h*factor).
   Returns 1 on success, 0 if it could not run (out of memory, bad factor);
   on 0 nothing has been written to dst. */
int  xbrz_glue_scale(int factor, const uint16_t *src, int src_pitch, int w, int h,
                     uint16_t *dst, int dst_pitch);

#ifdef __cplusplus
}
#endif

#endif /* _XBRZ_GLUE_H_ */
