/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  filters.c -- built-in ("native") render filters.
 *
 *  These replace the old Kega Fusion .rpi plugin loader. An .rpi file is a
 *  32-bit DLL, which a 64-bit process cannot load; a filter that is simply
 *  part of the executable has no such limit, and cannot crash the emulator
 *  the way a third-party DLL could.
 *
 *  Provenance, because it matters for licensing: every filter in THIS file is
 *  written from the published algorithm descriptions, not derived from any
 *  other project's source code -- with one exception, xBRZ, which is Zenju's
 *  GPLv3 code in xbrz/ and is only driven from here (see xbrz/README-xbrz.txt
 *  before distributing a binary that contains it; build with XBRZ=0 to leave
 *  it out).
 *
 *    Scale2x / Scale3x  AdvMAME's pixel-art scalers (Andrea Mazzoleni)
 *    Eagle 2x           the classic Eagle rules
 *    Smooth 2x / 4x     an edge-directed corner smoother in the spirit of
 *                       xBR (which is what it is *not*: it is a compact,
 *                       independently written implementation of the same
 *                       idea, so its output resembles xBR/hq2x but is not
 *                       identical to either)
 *    Scanlines / CRT    plain arithmetic, nothing borrowed
 *    xBRZ 2x - 6x       the real xBRZ by Zenju (xbrz/), optional (GPGX_XBRZ)
 *
 *  Design notes
 *
 *  - Input and output are both RGB565, matching the core's framebuffer and
 *    the DIB the frontend presents from, so no format conversion is needed.
 *  - The edge-detecting filters read up to two pixels past every edge of the
 *    frame. Rather than bounds-check every read, the frame is first copied
 *    into a scratch buffer with a two-pixel border made by repeating the
 *    outermost pixels; the inner loops then index freely.
 *  - Similarity is measured in a luma-weighted YUV-like space, computed once
 *    per pixel per frame into a parallel array, rather than once per
 *    comparison.
 *  - Scratch buffers are allocated lazily, only grow, and are freed by
 *    filter_shutdown().
 ****************************************************************************/

#include "filters.h"

#ifdef GPGX_XBRZ
#include "xbrz_glue.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef uint16_t px_t;

#define PAD 2

/* Address of row n of a destination with a byte pitch. */
#define ROW(base, pitch, n) ((px_t *)((uint8_t *)(base) + (size_t)(n) * (size_t)(pitch)))

/****************************************************************************
 * Scratch memory
 ****************************************************************************/

typedef struct
{
  void  *p;
  size_t cap;
} scratch_t;

static scratch_t sc_pad1, sc_ycc1, sc_pad2, sc_ycc2;

/* Set when a filter could not get its scratch memory and so wrote nothing;
   filter_apply() then reports that instead of claiming success. */
static int alloc_failed;

static void *scratch_get(scratch_t *s, size_t bytes)
{
  if (bytes > s->cap)
  {
    free(s->p);
    s->p   = malloc(bytes);
    s->cap = s->p ? bytes : 0;
    if (!s->p) alloc_failed = 1;
  }
  return s->p;
}

void filter_shutdown(void)
{
#ifdef GPGX_XBRZ
  xbrz_glue_release();
#endif
  free(sc_pad1.p); sc_pad1.p = NULL; sc_pad1.cap = 0;
  free(sc_ycc1.p); sc_ycc1.p = NULL; sc_ycc1.cap = 0;
  free(sc_pad2.p); sc_pad2.p = NULL; sc_pad2.cap = 0;
  free(sc_ycc2.p); sc_ycc2.p = NULL; sc_ycc2.cap = 0;
}

/****************************************************************************
 * Padded planes
 *
 * A plane is a (w + 2*PAD) x (h + 2*PAD) buffer. Callers get a pointer to
 * pixel (0,0) and a stride, and may read anything in [-PAD, w+PAD-1] x
 * [-PAD, h+PAD-1].
 ****************************************************************************/

static px_t *plane_alloc(scratch_t *s, int w, int h, int *stride)
{
  size_t st = (size_t)w + 2 * PAD;
  px_t  *b  = (px_t *)scratch_get(s, st * ((size_t)h + 2 * PAD) * sizeof(px_t));

  if (!b) return NULL;
  *stride = (int)st;
  return b + PAD * st + PAD;
}

/* Fills the border by repeating the outermost pixels. */
static void plane_extend(px_t *org, int stride, int w, int h)
{
  int x, y;

  for (y = 0; y < h; y++)
  {
    px_t *row = org + (ptrdiff_t)y * stride;
    for (x = 1; x <= PAD; x++)
    {
      row[-x]        = row[0];
      row[w - 1 + x] = row[w - 1];
    }
  }

  for (y = 1; y <= PAD; y++)
  {
    memcpy(org - (ptrdiff_t)y * stride - PAD,
           org - PAD, (size_t)stride * sizeof(px_t));
    memcpy(org + (ptrdiff_t)(h - 1 + y) * stride - PAD,
           org + (ptrdiff_t)(h - 1) * stride - PAD, (size_t)stride * sizeof(px_t));
  }
}

static px_t *plane_from_src(scratch_t *s, const px_t *src, int pitch,
                            int w, int h, int *stride)
{
  px_t *org = plane_alloc(s, w, h, stride);
  int   y;

  if (!org) return NULL;

  for (y = 0; y < h; y++)
  {
    memcpy(org + (ptrdiff_t)y * (*stride),
           (const uint8_t *)src + (size_t)y * (size_t)pitch,
           (size_t)w * sizeof(px_t));
  }

  plane_extend(org, *stride, w, h);
  return org;
}

/****************************************************************************
 * Colour helpers
 ****************************************************************************/

/* Blend a toward b. alpha is 0..32 (32 = all b). Works on all three RGB565
   fields at once by spreading them apart with a mask so the multiply of one
   field cannot bleed into the next. */
static inline px_t mix(px_t a, px_t b, unsigned alpha)
{
  uint32_t x = ((uint32_t)a | ((uint32_t)a << 16)) & 0x07E0F81Fu;
  uint32_t y = ((uint32_t)b | ((uint32_t)b << 16)) & 0x07E0F81Fu;

  x += ((y - x) * alpha) >> 5;
  x &= 0x07E0F81Fu;
  return (px_t)(x | (x >> 16));
}

/* Luma plus two scaled colour-difference channels. Compact enough that a
   whole frame of them stays cache-friendly. */
typedef struct
{
  uint8_t y;
  int8_t  u, v, pad;
} ycc_t;

static inline void to_ycc(px_t c, ycc_t *o)
{
  int r = (c >> 11) & 31;
  int g = (c >> 5) & 63;
  int b = c & 31;
  int y;

  r = (r << 3) | (r >> 2);
  g = (g << 2) | (g >> 4);
  b = (b << 3) | (b >> 2);

  y    = (77 * r + 150 * g + 29 * b) >> 8;
  o->y = (uint8_t)y;
  o->u = (int8_t)((b - y) >> 1);
  o->v = (int8_t)((r - y) >> 1);
  o->pad = 0;
}

/* Builds the colour-space plane for a padded pixel plane (same layout). */
static ycc_t *ycc_from_plane(scratch_t *s, const px_t *org, int stride, int w, int h)
{
  size_t  total = (size_t)stride * ((size_t)h + 2 * PAD);
  ycc_t  *base  = (ycc_t *)scratch_get(s, total * sizeof(ycc_t));
  const px_t *src = org - (ptrdiff_t)PAD * stride - PAD;
  size_t  i;

  (void)w;
  if (!base) return NULL;

  for (i = 0; i < total; i++) to_ycc(src[i], &base[i]);

  return base + (ptrdiff_t)PAD * stride + PAD;
}

/* Perceptual-ish distance between two pixels. Luma counts most, which is
   what makes an edge look like an edge to a person. Roughly: two different
   colours from the Mega Drive's 512-colour palette are always > EQ_T apart,
   while smooth-gradient noise stays under it. */
static inline int cdist(const ycc_t *a, const ycc_t *b)
{
  int dy = (int)a->y - (int)b->y;
  int du = (int)a->u - (int)b->u;
  int dv = (int)a->v - (int)b->v;

  if (dy < 0) dy = -dy;
  if (du < 0) du = -du;
  if (dv < 0) dv = -dv;

  return 4 * dy + 2 * du + 2 * dv;
}

#define EQ_T 24   /* at or below this, two colours count as "the same" */

/****************************************************************************
 * Scale2x / Scale3x  (AdvMAME)
 ****************************************************************************/

static void f_scale2x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int stride, x, y;
  const px_t *P = plane_from_src(&sc_pad1, src, sp, w, h, &stride);

  if (!P) return;

  for (y = 0; y < h; y++)
  {
    const px_t *r  = P + (ptrdiff_t)y * stride;
    px_t       *o0 = ROW(dst, dp, 2 * y);
    px_t       *o1 = ROW(dst, dp, 2 * y + 1);

    for (x = 0; x < w; x++)
    {
      px_t B = r[x - stride], D = r[x - 1], E = r[x], F = r[x + 1], H = r[x + stride];
      px_t e0 = E, e1 = E, e2 = E, e3 = E;

      if (B != H && D != F)
      {
        e0 = (D == B) ? D : E;
        e1 = (B == F) ? F : E;
        e2 = (D == H) ? D : E;
        e3 = (H == F) ? F : E;
      }

      o0[2 * x] = e0; o0[2 * x + 1] = e1;
      o1[2 * x] = e2; o1[2 * x + 1] = e3;
    }
  }
}

static void f_scale3x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int stride, x, y;
  const px_t *P = plane_from_src(&sc_pad1, src, sp, w, h, &stride);

  if (!P) return;

  for (y = 0; y < h; y++)
  {
    const px_t *r  = P + (ptrdiff_t)y * stride;
    px_t       *o0 = ROW(dst, dp, 3 * y);
    px_t       *o1 = ROW(dst, dp, 3 * y + 1);
    px_t       *o2 = ROW(dst, dp, 3 * y + 2);

    for (x = 0; x < w; x++)
    {
      px_t A = r[x - stride - 1], B = r[x - stride], C = r[x - stride + 1];
      px_t D = r[x - 1],          E = r[x],          F = r[x + 1];
      px_t G = r[x + stride - 1], H = r[x + stride], I = r[x + stride + 1];
      px_t e0 = E, e1 = E, e2 = E, e3 = E, e4 = E, e5 = E, e6 = E, e7 = E, e8 = E;

      if (B != H && D != F)
      {
        e0 = (D == B) ? D : E;
        e1 = ((D == B && E != C) || (B == F && E != A)) ? B : E;
        e2 = (B == F) ? F : E;
        e3 = ((D == B && E != G) || (D == H && E != A)) ? D : E;
        e5 = ((B == F && E != I) || (H == F && E != C)) ? F : E;
        e6 = (D == H) ? D : E;
        e7 = ((D == H && E != I) || (H == F && E != G)) ? H : E;
        e8 = (H == F) ? F : E;
      }

      o0[3 * x] = e0; o0[3 * x + 1] = e1; o0[3 * x + 2] = e2;
      o1[3 * x] = e3; o1[3 * x + 1] = e4; o1[3 * x + 2] = e5;
      o2[3 * x] = e6; o2[3 * x + 1] = e7; o2[3 * x + 2] = e8;
    }
  }
}

/****************************************************************************
 * Eagle 2x
 ****************************************************************************/

static void f_eagle2x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int stride, x, y;
  const px_t *P = plane_from_src(&sc_pad1, src, sp, w, h, &stride);

  if (!P) return;

  for (y = 0; y < h; y++)
  {
    const px_t *r  = P + (ptrdiff_t)y * stride;
    px_t       *o0 = ROW(dst, dp, 2 * y);
    px_t       *o1 = ROW(dst, dp, 2 * y + 1);

    for (x = 0; x < w; x++)
    {
      px_t S = r[x - stride - 1], T = r[x - stride], U = r[x - stride + 1];
      px_t V = r[x - 1],          C = r[x],          W = r[x + 1];
      px_t X = r[x + stride - 1], Y = r[x + stride], Z = r[x + stride + 1];
      px_t e0 = C, e1 = C, e2 = C, e3 = C;

      if (V == S && S == T) e0 = S;
      if (T == U && U == W) e1 = U;
      if (V == X && X == Y) e2 = X;
      if (W == Z && Z == Y) e3 = Z;

      o0[2 * x] = e0; o0[2 * x + 1] = e1;
      o1[2 * x] = e2; o1[2 * x + 1] = e3;
    }
  }
}

/****************************************************************************
 * Smooth 2x / 4x -- edge-directed corner smoothing
 *
 * Every source pixel becomes a 2x2 block that starts out as four copies of
 * the pixel. Then each of the four corners is examined on its own:
 *
 *      B
 *    D E F        (for the bottom-right corner; the other three are the
 *      H I         same picture reflected)
 *
 * If E differs from both F and H, the corner between them might be a cut
 * across a diagonal edge. Two costs are compared, each the summed colour
 * change along one diagonal direction through the neighbourhood:
 *
 *   anti  -- along the F-H diagonal (and the diagonals either side of it)
 *   main  -- along the E-I diagonal (and the ones either side of it)
 *
 * Small cost = the colour holds steady in that direction = an edge runs that
 * way. If the anti-diagonal is the steadier one, F and H belong to the same
 * feature and E's corner is cut off, so that sub-pixel is blended toward
 * whichever of F/H is closer to E. A cleanly matched pair (F ~ H) is blended
 * harder than a merely plausible one. A corner is only cut if E itself
 * continues away from it (E matches the pixel behind it on at least one
 * side). Everything else stays untouched, which is what keeps flat areas,
 * dithering, single-pixel details and 1-pixel lines from smearing or eroding.
 *
 * The same 12-pixel neighbourhood is used at every corner by reflecting the
 * two step vectors u (toward F) and v (toward H).
 ****************************************************************************/

/* Blend strengths, out of 32, for the corner sub-pixel:
     SMOOTH_CLEAN  F and H match each other, so the corner is a clean cut and
                   the sub-pixel takes the neighbour's colour outright;
     SMOOTH_SOFT   F and H differ, so it is only a plausible edge and gets a
                   partial blend.
   Tuned by eye on pixel-art test scenes (round outlines, 1-pixel diagonals,
   sprites, text) at both 2x and through the 4x cascade. Softer values (16/8)
   leave the 2x result barely smoother than a plain resize, and fall apart at
   4x: the second pass sees the first pass's softened stair-steps as broken
   and turns a 1-pixel line into a chain of dots. */
#define SMOOTH_CLEAN 32
#define SMOOTH_SOFT  12

static inline px_t smooth_corner(const px_t *p, const ycc_t *q, int u, int v)
{
  const ycc_t *qE = q;
  const ycc_t *qF = q + u;
  const ycc_t *qH = q + v;
  const ycc_t *qI = q + u + v;
  px_t         E  = p[0];
  int dEF, dEH, anti, main_, dFH;

  /* Exact match with a neighbour is by far the commonest case; settle it
     without computing any distances. (Same answer as the test below.) */
  if (E == p[u] || E == p[v]) return E;

  dEF = cdist(qE, qF);
  dEH = cdist(qE, qH);

  /* E already matches one of the two neighbours: nothing to cut. */
  if (dEF <= EQ_T || dEH <= EQ_T) return E;

  /* E must continue away from this corner, i.e. match the pixel behind it on
     one side. Without this, an isolated pixel (an eye, a dot of a letter, a
     bright star) counts as a corner of its surroundings on all four sides and
     gets eroded, and a 1-pixel line loses its thickness. */
  if (cdist(qE, q - u) > EQ_T && cdist(qE, q - v) > EQ_T) return E;

  anti = cdist(qE, q + u - v) + cdist(qE, q + v - u)
       + cdist(qI, q + 2 * u) + cdist(qI, q + 2 * v)
       + 4 * cdist(qH, qF);

  main_ = cdist(qH, q - u) + cdist(qH, q + u + 2 * v)
        + cdist(qF, q + 2 * u + v) + cdist(qF, q - v)
        + 4 * cdist(qE, qI);

  if (anti >= main_) return E;

  dFH = cdist(qF, qH);

  return mix(E, (dEF <= dEH) ? p[u] : p[v], (dFH <= EQ_T) ? SMOOTH_CLEAN : SMOOTH_SOFT);
}

/* One 2x pass. p/q are padded planes (origin pointers) with stride ps;
   out is written directly, os pixels per output row. */
static void smooth2x_pass(const px_t *P, const ycc_t *Q, int ps, int w, int h,
                          px_t *out, int os)
{
  int x, y;

  for (y = 0; y < h; y++)
  {
    const px_t  *p  = P + (ptrdiff_t)y * ps;
    const ycc_t *q  = Q + (ptrdiff_t)y * ps;
    px_t        *o0 = out + (ptrdiff_t)(2 * y) * os;
    px_t        *o1 = o0 + os;

    for (x = 0; x < w; x++)
    {
      px_t E = p[x];

      /* All four neighbours identical to E: every corner test would bail out
         immediately, so skip them. Most of a game frame is like this. */
      if (E == p[x - 1] && E == p[x + 1] && E == p[x - ps] && E == p[x + ps])
      {
        o0[2 * x] = E; o0[2 * x + 1] = E;
        o1[2 * x] = E; o1[2 * x + 1] = E;
        continue;
      }

      o0[2 * x]     = smooth_corner(p + x, q + x, -1, -ps);   /* top-left     */
      o0[2 * x + 1] = smooth_corner(p + x, q + x,  1, -ps);   /* top-right    */
      o1[2 * x]     = smooth_corner(p + x, q + x, -1,  ps);   /* bottom-left  */
      o1[2 * x + 1] = smooth_corner(p + x, q + x,  1,  ps);   /* bottom-right */
    }
  }
}

static void f_smooth2x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int    stride;
  px_t  *P = plane_from_src(&sc_pad1, src, sp, w, h, &stride);
  ycc_t *Q = P ? ycc_from_plane(&sc_ycc1, P, stride, w, h) : NULL;

  if (!P || !Q) return;
  smooth2x_pass(P, Q, stride, w, h, dst, dp / (int)sizeof(px_t));
}

/* Two passes, each 2x. The first pass's output goes straight into the
   padded plane the second pass reads from. */
static void f_smooth4x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int    s1, s2;
  px_t  *P1 = plane_from_src(&sc_pad1, src, sp, w, h, &s1);
  ycc_t *Q1 = P1 ? ycc_from_plane(&sc_ycc1, P1, s1, w, h) : NULL;
  px_t  *P2;
  ycc_t *Q2;

  if (!P1 || !Q1) return;

  P2 = plane_alloc(&sc_pad2, 2 * w, 2 * h, &s2);
  if (!P2) return;

  smooth2x_pass(P1, Q1, s1, w, h, P2, s2);
  plane_extend(P2, s2, 2 * w, 2 * h);

  Q2 = ycc_from_plane(&sc_ycc2, P2, s2, 2 * w, 2 * h);
  if (!Q2) return;

  smooth2x_pass(P2, Q2, s2, 2 * w, 2 * h, dst, dp / (int)sizeof(px_t));
}

/****************************************************************************
 * Scanlines 2x
 *
 * Doubles every pixel sideways; the second of each pair of rows is dimmed.
 ****************************************************************************/

/* c * 5/8, done on all three fields at once. */
static inline px_t dim58(px_t c)
{
  return (px_t)(((c >> 1) & 0x7BEF) + ((c >> 3) & 0x18E3));
}

static void f_scan2x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int x, y;

  for (y = 0; y < h; y++)
  {
    const px_t *r  = (const px_t *)((const uint8_t *)src + (size_t)y * (size_t)sp);
    px_t       *o0 = ROW(dst, dp, 2 * y);
    px_t       *o1 = ROW(dst, dp, 2 * y + 1);

    for (x = 0; x < w; x++)
    {
      px_t c = r[x];
      px_t d = dim58(c);

      o0[2 * x] = c; o0[2 * x + 1] = c;
      o1[2 * x] = d; o1[2 * x + 1] = d;
    }
  }
}

/****************************************************************************
 * CRT 3x -- RGB aperture mask with scanlines
 *
 * Every pixel becomes a 3x3 block. The three columns are the red, green and
 * blue phosphor stripes: each stripe carries its own channel at full
 * strength and the other two at a fraction, the way a real mask leaks a
 * little. The three rows follow a beam profile: a bright middle with dimmer
 * top and bottom. A gain above 1 makes up for the light the mask throws
 * away; the three constants below give roughly 80% of the source's
 * brightness with the mask still clearly visible. (An earlier, harsher set --
 * leak 0.30, gain 1.45 -- measured 58% and turned blue skies navy, because
 * bright stripes clip at full scale while dim ones do not.)
 ****************************************************************************/

#define CRT_LEAK  0.45    /* other-channel level inside a stripe   */
#define CRT_GAIN  1.60    /* overall brightness make-up            */

static const double crt_row_w[3] = { 0.75, 1.00, 0.75 };

/* [row][column][channel][8-bit input] -> value already in its 565 field. */
static uint16_t crt_lut[3][3][3][256];
static int      crt_ready;

static void crt_build(void)
{
  int r, c, ch, v;

  for (r = 0; r < 3; r++)
  for (c = 0; c < 3; c++)
  for (ch = 0; ch < 3; ch++)
  for (v = 0; v < 256; v++)
  {
    double f = v * crt_row_w[r] * ((ch == c) ? 1.0 : CRT_LEAK) * CRT_GAIN;
    int    o = (int)(f + 0.5);

    if (o > 255) o = 255;

    crt_lut[r][c][ch][v] = (uint16_t)((ch == 0) ? ((o >> 3) << 11)
                                     : (ch == 1) ? ((o >> 2) << 5)
                                     :              (o >> 3));
  }

  crt_ready = 1;
}

static void f_crt3x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int x, y, r, c;

  if (!crt_ready) crt_build();

  for (y = 0; y < h; y++)
  {
    const px_t *row = (const px_t *)((const uint8_t *)src + (size_t)y * (size_t)sp);
    px_t       *o[3];

    for (r = 0; r < 3; r++) o[r] = ROW(dst, dp, 3 * y + r);

    for (x = 0; x < w; x++)
    {
      px_t px = row[x];
      int  r5 = (px >> 11) & 31;
      int  g6 = (px >> 5) & 63;
      int  b5 = px & 31;
      int  r8 = (r5 << 3) | (r5 >> 2);
      int  g8 = (g6 << 2) | (g6 >> 4);
      int  b8 = (b5 << 3) | (b5 >> 2);

      for (r = 0; r < 3; r++)
      for (c = 0; c < 3; c++)
      {
        o[r][3 * x + c] = (px_t)(crt_lut[r][c][0][r8] |
                                 crt_lut[r][c][1][g8] |
                                 crt_lut[r][c][2][b8]);
      }
    }
  }
}

/****************************************************************************
 * Sharp 4x -- plain integer pixel replication
 *
 * Not a smoother at all: it hands the display stage a big, perfectly crisp
 * image so that the final (non-integer) resize to the window has 4x more
 * samples to work with. With Smooth Scaling on, that is the well-known
 * "sharp bilinear" look: crisp pixel edges without shimmering when the
 * window is not an exact multiple of the game's resolution.
 ****************************************************************************/

static void f_sharp4x(const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int x, y;

  for (y = 0; y < h; y++)
  {
    const px_t *r  = (const px_t *)((const uint8_t *)src + (size_t)y * (size_t)sp);
    px_t       *o0 = ROW(dst, dp, 4 * y);

    for (x = 0; x < w; x++)
    {
      px_t c = r[x];
      o0[4 * x] = c; o0[4 * x + 1] = c; o0[4 * x + 2] = c; o0[4 * x + 3] = c;
    }

    memcpy(ROW(dst, dp, 4 * y + 1), o0, (size_t)w * 4 * sizeof(px_t));
    memcpy(ROW(dst, dp, 4 * y + 2), o0, (size_t)w * 4 * sizeof(px_t));
    memcpy(ROW(dst, dp, 4 * y + 3), o0, (size_t)w * 4 * sizeof(px_t));
  }
}

#ifdef GPGX_XBRZ
/****************************************************************************
 * xBRZ 2x - 6x
 *
 * The real xBRZ ("scale by rules") by Zenju, from the sources in xbrz/,
 * driven through xbrz_glue.cpp. It is the same algorithm the well-known
 * xBRZ render plugins were built from, at default settings, so the picture
 * is the one you would have got from those. Unlike everything above it works
 * on 32-bit pixels and needs a 64 MB table, which is why it lives behind the
 * glue and is only set up while one of these filters is selected.
 *
 * If xBRZ cannot run (out of memory) the frame falls back to plain integer
 * replication at the same size, so the output is always complete.
 ****************************************************************************/

static void replicate(int n, const px_t *src, int sp, int w, int h, px_t *dst, int dp)
{
  int x, y, k, j;

  for (y = 0; y < h; y++)
  {
    const px_t *r  = (const px_t *)((const uint8_t *)src + (size_t)y * (size_t)sp);
    px_t       *o0 = ROW(dst, dp, n * y);

    for (x = 0; x < w; x++)
      for (k = 0; k < n; k++) o0[n * x + k] = r[x];

    for (j = 1; j < n; j++)
      memcpy(ROW(dst, dp, n * y + j), o0, (size_t)w * (size_t)n * sizeof(px_t));
  }
}

#define XBRZ_FILTER(N) \
  static void f_xbrz##N(const px_t *src, int sp, int w, int h, px_t *dst, int dp) \
  { \
    if (!xbrz_glue_scale(N, src, sp, w, h, dst, dp)) replicate(N, src, sp, w, h, dst, dp); \
  }

XBRZ_FILTER(2)
XBRZ_FILTER(3)
XBRZ_FILTER(4)
XBRZ_FILTER(5)
XBRZ_FILTER(6)
#endif /* GPGX_XBRZ */

/****************************************************************************
 * Table
 ****************************************************************************/

static const filter_def_t filters[] =
{
  /* The order here is the order in the menu; FILTER_SEP_BEFORE starts a new
     group. Names, not positions, are what the settings file stores. */
  { "Scale2x",               2, f_scale2x, 0 },
  { "Scale3x",               3, f_scale3x, 0 },
  { "Eagle 2x",              2, f_eagle2x, 0 },

  { "Smooth 2x (xBR-style)", 2, f_smooth2x, FILTER_SEP_BEFORE },
  { "Smooth 4x (xBR-style)", 4, f_smooth4x, 0 },

#ifdef GPGX_XBRZ
  { "xBRZ 2x",               2, f_xbrz2, FILTER_SEP_BEFORE | FILTER_NEEDS_XBRZ },
  { "xBRZ 3x",               3, f_xbrz3, FILTER_NEEDS_XBRZ },
  { "xBRZ 4x",               4, f_xbrz4, FILTER_NEEDS_XBRZ },
  { "xBRZ 5x",               5, f_xbrz5, FILTER_NEEDS_XBRZ },
  { "xBRZ 6x",               6, f_xbrz6, FILTER_NEEDS_XBRZ },
#endif

  { "Scanlines 2x",          2, f_scan2x, FILTER_SEP_BEFORE },
  { "CRT 3x (RGB mask)",     3, f_crt3x, 0 },
  { "Sharp 4x",              4, f_sharp4x, 0 },
};

#define FILTER_TOTAL ((int)(sizeof(filters) / sizeof(filters[0])))

int filter_count(void)
{
  return FILTER_TOTAL;
}

const filter_def_t *filter_get(int index)
{
  return (index >= 0 && index < FILTER_TOTAL) ? &filters[index] : NULL;
}

static char lower(char c)
{
  return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Case-insensitive match on the filter name. A trailing ".rpi" is ignored so
   a config file written by the old plugin-based build still resolves when the
   plugin happened to share a name with a built-in ("Scale2x.rpi"). */
int filter_find(const char *name)
{
  size_t n;
  int    i;

  if (!name || !name[0]) return -1;

  n = strlen(name);
  if (n > 4 && name[n - 4] == '.' && lower(name[n - 3]) == 'r' &&
      lower(name[n - 2]) == 'p' && lower(name[n - 1]) == 'i')
  {
    n -= 4;
  }

  for (i = 0; i < FILTER_TOTAL; i++)
  {
    const char *f = filters[i].name;
    size_t      k;

    if (strlen(f) != n) continue;
    for (k = 0; k < n; k++)
    {
      if (lower(f[k]) != lower(name[k])) break;
    }
    if (k == n) return i;
  }

  return -1;
}

int filter_prepare(int index)
{
  const filter_def_t *f = filter_get(index);

#ifdef GPGX_XBRZ
  if (f && (f->flags & FILTER_NEEDS_XBRZ)) return xbrz_glue_prepare();

  /* Not an xBRZ filter (or no filter): give back the 64 MB it may have held. */
  xbrz_glue_release();
#endif

  (void)f;
  return 1;
}

int filter_apply(int index, const uint16_t *src, int src_pitch, int w, int h,
                 uint16_t *dst, int dst_pitch)
{
  const filter_def_t *f = filter_get(index);

  if (!f || !src || !dst || w < 1 || h < 1) return 0;
  if (w > FILTER_MAX_W || h > FILTER_MAX_H) return 0;

  alloc_failed = 0;
  f->run(src, src_pitch, w, h, dst, dst_pitch);
  return alloc_failed ? 0 : f->scale;
}
