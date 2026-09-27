/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  video.c -- frame output, scaling, fullscreen and screenshots.
 *
 *  The core renders 16-bit RGB565 into a top-down DIB section that we own.
 *  Presenting is a single StretchBlt from a memory DC, which keeps the source
 *  rectangle in ordinary top-left coordinates -- StretchDIBits flips its
 *  meaning depending on the sign of biHeight, and that is not worth the
 *  trouble here.
 *
 *  RGB565 is not an arbitrary choice: the core's Blargg NTSC filter only
 *  supports 15- and 16-bit output, so a 32-bit framebuffer would mean giving
 *  up the filter.
 ****************************************************************************/

#include <windows.h>
#include <dwmapi.h>
#include <math.h>
#include <zlib.h>

#include "shared.h"
#include "md_ntsc.h"
#include "sms_ntsc.h"
#include "gui.h"
#include "theme.h"
#include "d3d9_video.h"
#include "d3d11_video.h"
#include "filters.h"

/* Allocated by the frontend, consumed by the core's NTSC blitter. */
md_ntsc_t  *md_ntsc;
sms_ntsc_t *sms_ntsc;

static struct
{
  HBITMAP  dib;
  HDC      mem_dc;
  HGDIOBJ  old_bmp;
  void    *pixels;

  HFONT    font;

  RECT     last_dest;     /* where the frame landed, for border clearing */
  int      dest_valid;

  int      fps;
  char     notice[128];
  DWORD    notice_until;

  /* fullscreen bookkeeping */
  int      fullscreen;
  DWORD    saved_style;
  DWORD    saved_ex_style;
  WINDOWPLACEMENT saved_place;
  HMENU    saved_menu;
} vid;

/* A separate 32bpp GDI buffer for the D3D9 path's overlay -- draw_overlay()
   runs against this unchanged, then alpha gets derived per-pixel (opaque
   wherever GDI drew something over the black background, transparent
   everywhere else) before uploading it as a texture. Sized to match
   whatever the current content area is, recreated only when that
   changes. */
static struct
{
  HBITMAP dib;
  HDC     mem_dc;
  HGDIOBJ old_bmp;
  void   *pixels;   /* 32bpp 0x00RRGGBB, alpha filled in separately */
  int     w, h;
} osd;

/* A separate overlay buffer for destination-space scanlines -- unlike
   the native-resolution approach below (kept as the GDI fallback), this
   generates thin, fixed-width dark lines directly in output pixels, so
   they stay thin regardless of how much the image gets scaled up.
   Native-resolution darkening looked right at modest window sizes but
   became a 50/50 dark/bright split at fullscreen scale (confirmed by
   direct comparison against Kega Fusion's noticeably thinner lines at
   the same 100% setting) -- a native pixel row darkened before scaling
   is exactly as thick as the scale factor makes it, with no way to make
   it thinner without operating on already-scaled pixels. Cached and
   only regenerated when its inputs actually change. */
static struct
{
  HBITMAP dib;
  HDC     mem_dc;
  HGDIOBJ old_bmp;
  void   *pixels;
  int     w, h;
  int     cached_native_h, cached_pct;
  int     cached_y0, cached_span;
} scan_ovl;

static void fx_free(void);   /* defined with the frame processing below */

static int ensure_scanline_dib(int w, int h)
{
  BITMAPV4HEADER bi;
  HDC screen;

  if (scan_ovl.dib && scan_ovl.w == w && scan_ovl.h == h) return 1;

  if (scan_ovl.mem_dc)
  {
    SelectObject(scan_ovl.mem_dc, scan_ovl.old_bmp);
    DeleteDC(scan_ovl.mem_dc);
    scan_ovl.mem_dc = NULL;
  }
  if (scan_ovl.dib)
  {
    DeleteObject(scan_ovl.dib);
    scan_ovl.dib = NULL;
  }
  scan_ovl.w = 0; scan_ovl.h = 0;

  if (w < 1 || h < 1) return 0;

  ZeroMemory(&bi, sizeof(bi));
  bi.bV4Size          = sizeof(BITMAPV4HEADER);
  bi.bV4Width         = w;
  bi.bV4Height        = -h;
  bi.bV4Planes        = 1;
  bi.bV4BitCount      = 32;
  bi.bV4V4Compression = BI_BITFIELDS;
  bi.bV4RedMask       = 0x00FF0000;
  bi.bV4GreenMask     = 0x0000FF00;
  bi.bV4BlueMask      = 0x000000FF;

  screen = GetDC(NULL);
  scan_ovl.dib = CreateDIBSection(screen, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &scan_ovl.pixels, NULL, 0);
  ReleaseDC(NULL, screen);

  if (!scan_ovl.dib || !scan_ovl.pixels) { scan_ovl.dib = NULL; return 0; }

  scan_ovl.mem_dc = CreateCompatibleDC(NULL);
  if (!scan_ovl.mem_dc) { DeleteObject(scan_ovl.dib); scan_ovl.dib = NULL; return 0; }
  scan_ovl.old_bmp = SelectObject(scan_ovl.mem_dc, scan_ovl.dib);

  scan_ovl.w = w;
  scan_ovl.h = h;
  scan_ovl.cached_native_h = -1;   /* force the pattern to (re)generate */
  return 1;
}

/* Fills the overlay with a thin dark line at each native scanline's
   output position -- one destination pixel wide regardless of scale,
   fully transparent everywhere else. Only regenerates when the native
   height, destination size, or intensity actually changed since last
   time. */
static void ensure_scanline_pattern(int native_h, int dest_w, int dest_h, int y0, int span)
{
  uint32 *px;
  uint32 color;
  int dy, step;

  if (scan_ovl.cached_native_h == native_h && scan_ovl.cached_pct == gui.scanline_pct
      && scan_ovl.w == dest_w && scan_ovl.h == dest_h
      && scan_ovl.cached_y0 == y0 && scan_ovl.cached_span == span)
  {
    return;
  }

  px = (uint32 *)scan_ovl.pixels;
  /* Capped at 70% alpha even at the "100%" setting -- real scanline
     effects never fully black out a row (that reads as missing pixels,
     not a texture over the image), the underlying color should always
     show through at least somewhat regardless of how strong the effect
     is set. 0-100% maps proportionally onto this 0-70% effective range. */
  color = ((uint32)((gui.scanline_pct * 178) / 100) << 24);   /* black, capped alpha */

  ZeroMemory(scan_ovl.pixels, (size_t)dest_w * dest_h * 4);

  /* A fixed integer step, not a mapping from native row boundaries: any
     row-boundary-based approach (forward or inverse) still has to round
     somewhere whenever native_h doesn't evenly divide dest_h, which is
     virtually always true, and that rounding shows up as occasional
     uneven gaps no matter how the rounding error gets distributed. A
     constant step has no such rounding to do -- every gap between lines
     is identical by construction, at the cost of the total line count no
     longer being an exact match for native_h. */
  /* Sized from the picture itself (span), not the whole video area, so
     letterboxing doesn't stretch the step; and never below 2: at 1x-ish
     window sizes (span < 2 * native_h) a step of 1 darkened *every* row,
     which is just a uniformly darker picture with no visible lines. */
  step = span / native_h;
  if (step < 2) step = 2;

  for (dy = y0; dy < y0 + span && dy < dest_h; dy += step)
  {
    if (dy < 0) continue;
    uint32 *row = px + (size_t)dy * dest_w;
    int x;
    for (x = 0; x < dest_w; x++) row[x] = color;
  }

  scan_ovl.cached_native_h = native_h;
  scan_ovl.cached_pct = gui.scanline_pct;
  scan_ovl.cached_y0 = y0;
  scan_ovl.cached_span = span;
}


static int ensure_osd_dib(int w, int h)
{
  BITMAPV4HEADER bi;
  HDC screen;

  if (osd.dib && osd.w == w && osd.h == h) return 1;

  if (osd.mem_dc)
  {
    SelectObject(osd.mem_dc, osd.old_bmp);
    DeleteDC(osd.mem_dc);
    osd.mem_dc = NULL;
  }
  if (osd.dib)
  {
    DeleteObject(osd.dib);
    osd.dib = NULL;
  }
  osd.w = 0; osd.h = 0;

  if (w < 1 || h < 1) return 0;

  ZeroMemory(&bi, sizeof(bi));
  bi.bV4Size          = sizeof(BITMAPV4HEADER);
  bi.bV4Width         = w;
  bi.bV4Height        = -h;   /* top-down */
  bi.bV4Planes        = 1;
  bi.bV4BitCount      = 32;
  bi.bV4V4Compression = BI_BITFIELDS;
  bi.bV4RedMask       = 0x00FF0000;
  bi.bV4GreenMask     = 0x0000FF00;
  bi.bV4BlueMask      = 0x000000FF;

  screen = GetDC(NULL);
  osd.dib = CreateDIBSection(screen, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &osd.pixels, NULL, 0);
  ReleaseDC(NULL, screen);

  if (!osd.dib || !osd.pixels) { osd.dib = NULL; return 0; }

  osd.mem_dc = CreateCompatibleDC(NULL);
  if (!osd.mem_dc) { DeleteObject(osd.dib); osd.dib = NULL; return 0; }
  osd.old_bmp = SelectObject(osd.mem_dc, osd.dib);

  osd.w = w;
  osd.h = h;
  return 1;
}

/****************************************************************************
 * Render filters
 *
 * The filters themselves live in filters.c -- plain RGB565-in, RGB565-out CPU
 * code with no Windows dependency. This section is only the glue: it owns the
 * output buffer, remembers which filter is active, and hands present() the
 * filtered image.
 *
 * This replaced a loader for Kega Fusion's .rpi plugins. Those are 32-bit
 * DLLs, which forced the whole program to be 32-bit; built-in filters have no
 * such limit, so this same code builds as either a 32-bit or a 64-bit
 * executable. (They also can't crash the emulator, so the exception-guard
 * machinery the plugin loader needed is gone.)
 ****************************************************************************/

/* The output buffer is a fixed-size 16-bit DIB, big enough for the largest
   frame at the largest filter multiplier. GDI reads it back with the same
   stride the filter wrote it with. */
#define FILTER_DST_MAX_W (FRAME_MAX_W * FILTER_MAX_SCALE)
#define FILTER_DST_MAX_H (FRAME_MAX_H * FILTER_MAX_SCALE)

/* filters.c sizes its scratch space from its own limits; they must cover
   everything this frontend can hand it. */
typedef char filter_limits_cover_frame_w[(FILTER_MAX_W >= FRAME_MAX_W) ? 1 : -1];
typedef char filter_limits_cover_frame_h[(FILTER_MAX_H >= FRAME_MAX_H) ? 1 : -1];

static struct
{
  int      index;      /* active filter (see filters.c), -1 = none */

  HBITMAP  dib;
  HDC      mem_dc;
  HGDIOBJ  old_bmp;
  void    *pixels;
  DWORD    pitch;      /* bytes per row of the DIB */
} nf = { -1 };

static int filter_buffer_ensure(void)
{
  struct
  {
    BITMAPINFOHEADER hdr;
    DWORD mask[3];
  } bi;
  HDC screen;

  if (nf.dib) return 1;

  ZeroMemory(&bi, sizeof(bi));
  bi.hdr.biSize        = sizeof(BITMAPINFOHEADER);
  bi.hdr.biWidth       = FILTER_DST_MAX_W;
  bi.hdr.biHeight      = -FILTER_DST_MAX_H;   /* negative: top-down */
  bi.hdr.biPlanes      = 1;
  bi.hdr.biBitCount    = 16;
  bi.hdr.biCompression = BI_BITFIELDS;
  bi.mask[0] = 0xF800;
  bi.mask[1] = 0x07E0;
  bi.mask[2] = 0x001F;

  screen = GetDC(NULL);
  nf.dib = CreateDIBSection(screen, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &nf.pixels, NULL, 0);
  ReleaseDC(NULL, screen);
  if (!nf.dib || !nf.pixels) { nf.dib = NULL; return 0; }

  nf.mem_dc = CreateCompatibleDC(NULL);
  if (!nf.mem_dc) { DeleteObject(nf.dib); nf.dib = NULL; return 0; }
  nf.old_bmp = SelectObject(nf.mem_dc, nf.dib);
  nf.pitch   = FILTER_DST_MAX_W * 2;

  return 1;
}

static void filter_buffer_free(void)
{
  if (nf.mem_dc)
  {
    SelectObject(nf.mem_dc, nf.old_bmp);
    DeleteDC(nf.mem_dc);
    nf.mem_dc = NULL;
  }
  if (nf.dib)
  {
    DeleteObject(nf.dib);
    nf.dib = NULL;
  }
  nf.pixels = NULL;
}

int video_filter_count(void)
{
  return filter_count();
}

const char *video_filter_name(int index)
{
  const filter_def_t *f = filter_get(index);
  return f ? f->name : "";
}

int video_filter_separator_before(int index)
{
  const filter_def_t *f = filter_get(index);
  return f && (f->flags & FILTER_SEP_BEFORE) ? 1 : 0;
}

int video_filter_current(void)
{
  return nf.index;
}

/* index -1 (or anything out of range) turns filtering off. */
void video_set_filter(int index)
{
  const filter_def_t *f = filter_get(index);

  if (!f)
  {
    nf.index = -1;
    gui.render_filter[0] = '\0';
    filter_prepare(-1);          /* give back anything the old filter held */
    filter_buffer_free();        /* and the ~30 MB output picture */
    video_viewport_changed();
    return;
  }

  if (!filter_buffer_ensure())
  {
    nf.index = -1;
    gui.render_filter[0] = '\0';
    filter_prepare(-1);
    gui_status("Not enough memory for the filter output buffer");
    return;
  }

  /* Gets the filter ready (xBRZ builds its 64 MB table here, about a tenth of
     a second, so the first frame does not stall) and releases what the
     previous filter was holding. */
  if (!filter_prepare(index))
  {
    nf.index = -1;
    gui.render_filter[0] = '\0';
    filter_prepare(-1);
    gui_status("Not enough memory for the %s filter", f->name);
    video_viewport_changed();
    return;
  }

  nf.index = index;
  lstrcpynA(gui.render_filter, f->name, sizeof(gui.render_filter));

  /* Mutually exclusive with the NTSC filter: both reshape the image, and
     running them one after the other is a combination that has never been
     looked at, so they are kept as alternatives. */
  if (config.ntsc)
  {
    video_set_ntsc(0);
  }

  video_viewport_changed();
}

/* For restoring the saved setting at startup. Returns 1 if the name matched
   a built-in filter and it is now active. */
int video_set_filter_by_name(const char *name)
{
  int index = filter_find(name);

  if (index < 0)
  {
    video_set_filter(-1);
    return 0;
  }

  video_set_filter(index);
  return nf.index == index;
}

/*
 * Runs the active filter against the current frame. On success, points
 * *out_dc/out_w/out_h at the filtered result for present() to blit from.
 * Returns 0 if there is no filter (or it could not run), in which case the
 * caller presents the unfiltered frame.
 *
 * The output size is fixed by the filter itself (Scale3x is always 3x), not
 * by the window: the final resize to the window happens afterwards in
 * present(), through D3D9 or StretchBlt, and always honours the person's own
 * Smooth Scaling setting.
 */
static int filter_run(const uint16 *src, int src_pitch, int src_w, int src_h, HDC *out_dc, int *out_w, int *out_h)
{
  int scale;

  if (nf.index < 0 || !nf.pixels) return 0;

  scale = filter_apply(nf.index,
                       (const uint16_t *)src, src_pitch,
                       src_w, src_h,
                       (uint16_t *)nf.pixels, (int)nf.pitch);
  if (!scale) return 0;

  *out_dc = nf.mem_dc;
  *out_w  = src_w * scale;
  *out_h  = src_h * scale;
  return 1;
}


/****************************************************************************
 * Setup
 ****************************************************************************/

int video_init(void)
{
  /* BITMAPINFOHEADER followed by the three BI_BITFIELDS colour masks. */
  struct
  {
    BITMAPINFOHEADER hdr;
    DWORD mask[3];
  } bi;

  HDC screen;

  ZeroMemory(&vid, sizeof(vid));
  ZeroMemory(&bi, sizeof(bi));

  bi.hdr.biSize        = sizeof(BITMAPINFOHEADER);
  bi.hdr.biWidth       = FRAME_MAX_W;
  bi.hdr.biHeight      = -FRAME_MAX_H;   /* negative: top-down */
  bi.hdr.biPlanes      = 1;
  bi.hdr.biBitCount    = 16;
  bi.hdr.biCompression = BI_BITFIELDS;
  bi.mask[0]           = 0xF800;         /* red   */
  bi.mask[1]           = 0x07E0;         /* green */
  bi.mask[2]           = 0x001F;         /* blue  */

  screen = GetDC(NULL);
  vid.dib = CreateDIBSection(screen, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
                             &vid.pixels, NULL, 0);
  ReleaseDC(NULL, screen);

  if (!vid.dib || !vid.pixels) return 0;

  vid.mem_dc = CreateCompatibleDC(NULL);
  if (!vid.mem_dc)
  {
    DeleteObject(vid.dib);
    vid.dib = NULL;
    return 0;
  }
  vid.old_bmp = SelectObject(vid.mem_dc, vid.dib);

  ZeroMemory(vid.pixels, (size_t)FRAME_MAX_W * FRAME_MAX_H * FRAME_BPP);

  vid.font = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, "Segoe UI");

  /* Hand the framebuffer to the core. */
  memset(&bitmap, 0, sizeof(t_bitmap));
  bitmap.width  = FRAME_MAX_W;
  bitmap.height = FRAME_MAX_H;
  bitmap.pitch  = FRAME_MAX_W * FRAME_BPP;
  bitmap.data   = (uint8 *)vid.pixels;
  bitmap.viewport.changed = 3;

  md_ntsc  = (md_ntsc_t *)calloc(1, sizeof(md_ntsc_t));
  sms_ntsc = (sms_ntsc_t *)calloc(1, sizeof(sms_ntsc_t));
  if (!md_ntsc || !sms_ntsc) return 0;

  video_set_ntsc(config.ntsc);

  if (gui.renderer == RENDERER_D3D9)  d3d9_init(g_hwnd);
  if (gui.renderer == RENDERER_D3D11) d3d11_init(g_hwnd);

  return 1;
}

/* Switches the active output path. Whichever one is now selected is (re)tried
   immediately so the change is visible right away rather than waiting for
   the next frame to discover it's unavailable and fall back; if it fails to
   initialize, the per-frame render path below still falls back to GDI on
   its own, same as it always has. */
/* Whichever hardware-accelerated path (if any) is both selected and
   actually working right now -- either can fail to initialize or can
   later become unavailable, in which case this correctly reports "no" and
   the per-frame render path below falls back to GDI, same as always. */
static int hw_renderer_active(void)
{
  if (gui.renderer == RENDERER_D3D9)  return d3d9_available();
  if (gui.renderer == RENDERER_D3D11) return d3d11_available();
  return 0;
}

void video_set_renderer(int renderer)
{
  if (d3d9_available())  d3d9_shutdown();
  if (d3d11_available()) d3d11_shutdown();

  if (renderer == RENDERER_D3D9)  d3d9_init(g_hwnd);
  if (renderer == RENDERER_D3D11) d3d11_init(g_hwnd);
}

/*
 * VSync. After each new frame is presented, wait for the Windows compositor's
 * next refresh (DwmFlush), so frames appear on refresh boundaries. Works the
 * same for the Direct3D and GDI paths. Skipped -- for that frame only -- when
 * the caller says the sound buffer is running low, because waiting up to a
 * whole refresh then would starve the sound card and crackle; better to show
 * that frame early than to stutter. With no compositor (Windows 7 Basic
 * theme) there is nothing to sync to and this does nothing.
 */
static int vsync_allowed = 1;

void video_set_vsync_allowed(int allowed)
{
  vsync_allowed = allowed;
}

static void vsync_wait(void)
{
  BOOL composited = FALSE;

  if (!gui.vsync || !vsync_allowed) return;
  if (FAILED(DwmIsCompositionEnabled(&composited)) || !composited) return;
  DwmFlush();
}

void video_set_vsync(int on)
{
  (void)on;   /* read from gui.vsync each frame */
}

void video_shutdown(void)
{
  d3d9_shutdown();
  d3d11_shutdown();

  if (osd.mem_dc)
  {
    SelectObject(osd.mem_dc, osd.old_bmp);
    DeleteDC(osd.mem_dc);
    osd.mem_dc = NULL;
  }
  if (osd.dib)
  {
    DeleteObject(osd.dib);
    osd.dib = NULL;
  }
  if (scan_ovl.mem_dc)
  {
    SelectObject(scan_ovl.mem_dc, scan_ovl.old_bmp);
    DeleteDC(scan_ovl.mem_dc);
    scan_ovl.mem_dc = NULL;
  }
  if (scan_ovl.dib)
  {
    DeleteObject(scan_ovl.dib);
    scan_ovl.dib = NULL;
  }

  fx_free();

  if (vid.mem_dc)
  {
    SelectObject(vid.mem_dc, vid.old_bmp);
    DeleteDC(vid.mem_dc);
    vid.mem_dc = NULL;
  }
  if (vid.dib)
  {
    DeleteObject(vid.dib);
    vid.dib = NULL;
  }
  if (vid.font)
  {
    DeleteObject(vid.font);
    vid.font = NULL;
  }

  nf.index = -1;
  filter_buffer_free();
  filter_shutdown();

  free(md_ntsc);
  free(sms_ntsc);
  md_ntsc = NULL;
  sms_ntsc = NULL;

  bitmap.data = NULL;
}

void video_set_ntsc(int mode)
{
  config.ntsc = (uint8)mode;

  /* Mutually exclusive with a render filter -- see video_set_filter(). */
  if (mode && nf.index >= 0)
  {
    nf.index = -1;
    gui.render_filter[0] = '\0';
    filter_prepare(-1);
    filter_buffer_free();
  }

  switch (mode)
  {
    case 1:
      md_ntsc_init(md_ntsc, &md_ntsc_composite);
      sms_ntsc_init(sms_ntsc, &sms_ntsc_composite);
      break;
    case 2:
      md_ntsc_init(md_ntsc, &md_ntsc_svideo);
      sms_ntsc_init(sms_ntsc, &sms_ntsc_svideo);
      break;
    case 3:
      md_ntsc_init(md_ntsc, &md_ntsc_rgb);
      sms_ntsc_init(sms_ntsc, &sms_ntsc_rgb);
      break;
    default:
      break;
  }

  video_viewport_changed();
}

/*
 * The window moved or resized: the frame is still valid, only the destination
 * rectangle needs recomputing.
 */
void video_invalidate(void)
{
  vid.dest_valid = 0;
  if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

/*
 * The emulated viewport itself changed size. Old pixels outside the new
 * viewport would otherwise stay on screen, so the framebuffer is cleared.
 */
void video_viewport_changed(void)
{
  vid.dest_valid = 0;
  if (bitmap.data)
  {
    ZeroMemory(bitmap.data, (size_t)FRAME_MAX_W * FRAME_MAX_H * FRAME_BPP);
  }
  if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

/* Forces the next present() to redraw fully, without zeroing the frame
   buffer first the way video_viewport_changed() does. That zero-fill is
   correct when the output resolution/mapping itself changed (NTSC filter,
   overscan, aspect ratio) -- there's no reason the old pixels would still
   be valid. For a rewind step the resolution hasn't changed at all, and
   forcibly blacking the buffer first is what a per-frame rendering path
   with any "skip this row, it hasn't changed" assumption can't recover
   from, since state_load() has no way to know about or invalidate an
   optimization like that -- it isn't part of the serialized state. */
void video_force_redraw(void)
{
  vid.dest_valid = 0;
  if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

/****************************************************************************
 * Geometry
 ****************************************************************************/

/* Size of the area the frame is drawn into (client area minus status bar). */
static void get_video_rect(RECT *out)
{
  RECT client;

  GetClientRect(g_hwnd, &client);

  if (g_status && IsWindowVisible(g_status))
  {
    RECT sb;
    GetWindowRect(g_status, &sb);
    client.bottom -= (sb.bottom - sb.top);
    if (client.bottom < client.top) client.bottom = client.top;
  }

  *out = client;
}

/* Current visible source size inside the framebuffer. */
static void get_source_size(int *w, int *h)
{
  int sw = bitmap.viewport.w + 2 * bitmap.viewport.x;
  int sh = bitmap.viewport.h + 2 * bitmap.viewport.y;

  if (sw <= 0) sw = 320;
  if (sh <= 0) sh = 224;

  if (config.ntsc)
  {
    sw = (reg[12] & 0x01) ? MD_NTSC_OUT_WIDTH(sw) : SMS_NTSC_OUT_WIDTH(sw);
  }

  if (interlaced && config.render) sh *= 2;

  if (sw > FRAME_MAX_W) sw = FRAME_MAX_W;
  if (sh > FRAME_MAX_H) sh = FRAME_MAX_H;

  *w = sw;
  *h = sh;
}

/*
 * Whether the 4:3 aspect option should actually apply 4:3 correction.
 *
 * That correction exists to compensate for non-square pixels on a CRT --
 * relevant to the Mega Drive, Master System and SG-1000. The Game Gear is a
 * fixed LCD panel with no CRT to correct for, so stretching it to 4:3 would
 * only distort it. Falls back to square pixels for Game Gear even when 4:3
 * is selected, rather than adding a second menu state the person has to know
 * to pick.
 */
static int use_43_correction(void)
{
  return gui.aspect == ASPECT_43 && system_hw != SYSTEM_GG;
}

static void get_dest_rect(const RECT *area, int sw, int sh, RECT *out)
{
  int aw = area->right - area->left;
  int ah = area->bottom - area->top;
  int dw, dh;
  double ar;

  if (aw < 1) aw = 1;
  if (ah < 1) ah = 1;

  if (gui.aspect == ASPECT_STRETCH)
  {
    dw = aw;
    dh = ah;
  }
  else
  {
    ar = use_43_correction() ? (4.0 / 3.0) : ((double)sw / (double)sh);

    dh = ah;
    dw = (int)(ah * ar + 0.5);
    if (dw > aw)
    {
      dw = aw;
      dh = (int)(aw / ar + 0.5);
    }
  }

  if (dw < 1) dw = 1;
  if (dh < 1) dh = 1;

  out->left   = area->left + (aw - dw) / 2;
  out->top    = area->top  + (ah - dh) / 2;
  out->right  = out->left + dw;
  out->bottom = out->top + dh;
}

/* Where the frame currently lands on screen, and the source size behind it. */
void video_get_output_rect(RECT *dest, int *src_w, int *src_h)
{
  RECT area;
  int sw, sh;

  get_source_size(&sw, &sh);
  get_video_rect(&area);
  get_dest_rect(&area, sw, sh, dest);

  if (src_w) *src_w = sw;
  if (src_h) *src_h = sh;
}

void video_preferred_size(int scale, int *w, int *h)
{
  int sw, sh;

  if (bitmap.data && emu_running) get_source_size(&sw, &sh);
  else { sw = 320; sh = 224; }

  /* Undo the NTSC filter's horizontal expansion so 2x still means 2x. */
  if (config.ntsc) sw = (reg[12] & 0x01) ? 320 : 256;
  if (interlaced && config.render) sh /= 2;

  if (use_43_correction())
  {
    *h = sh * scale;
    *w = (*h * 4) / 3;
  }
  else
  {
    *w = sw * scale;
    *h = sh * scale;
  }
}

/****************************************************************************
 * Presenting
 ****************************************************************************/

static void fill_borders(HDC hdc, const RECT *area, const RECT *dest)
{
  HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
  RECT r;

  if (dest->top > area->top)
  {
    r = *area; r.bottom = dest->top;
    FillRect(hdc, &r, black);
  }
  if (dest->bottom < area->bottom)
  {
    r = *area; r.top = dest->bottom;
    FillRect(hdc, &r, black);
  }
  if (dest->left > area->left)
  {
    r.left = area->left; r.right = dest->left;
    r.top = dest->top; r.bottom = dest->bottom;
    FillRect(hdc, &r, black);
  }
  if (dest->right < area->right)
  {
    r.left = dest->right; r.right = area->right;
    r.top = dest->top; r.bottom = dest->bottom;
    FillRect(hdc, &r, black);
  }
}

static void draw_text_shadowed(HDC hdc, int x, int y, UINT align, const char *text)
{
  HGDIOBJ old_font = SelectObject(hdc, vid.font);
  UINT old_align = SetTextAlign(hdc, align);

  SetBkMode(hdc, TRANSPARENT);

  SetTextColor(hdc, RGB(0, 0, 0));
  TextOutA(hdc, x + 1, y + 1, text, lstrlenA(text));

  SetTextColor(hdc, RGB(255, 255, 255));
  TextOutA(hdc, x, y, text, lstrlenA(text));

  SetTextAlign(hdc, old_align);
  SelectObject(hdc, old_font);
}

/* Per-frame picture processing (Brighten, and scanlines on the GDI path).
 *
 * Works on a separate copy of the frame and leaves the emulator's own
 * framebuffer (vid.pixels / bitmap.data) untouched. It used to darken
 * vid.pixels in place, which meant every repaint of the same frame (while
 * paused, or after a window move) darkened it again, and screenshots,
 * save-state thumbnails and rewind all saw the processed picture.
 *
 * Direct pixel math rather than AlphaBlend: partial-alpha blending proved
 * unreliable across Windows/Wine/driver combinations. On the Direct3D path
 * scanlines are a destination-space overlay instead (see present()), so only
 * Brighten runs here.
 */
#define BRIGHTEN_GAMMA 0.88   /* < 1 lifts mid-tones; black and white stay put */

static struct
{
  HBITMAP dib;
  HDC     mem_dc;
  HGDIOBJ old_bmp;
  void   *pixels;
} fx;

static uint8 bright5[32], bright6[64];
static int   bright_ready;

static void build_brighten_tables(void)
{
  int i;

  if (bright_ready) return;
  for (i = 0; i < 32; i++) bright5[i] = (uint8)(31.0 * pow(i / 31.0, BRIGHTEN_GAMMA) + 0.5);
  for (i = 0; i < 64; i++) bright6[i] = (uint8)(63.0 * pow(i / 63.0, BRIGHTEN_GAMMA) + 0.5);
  bright_ready = 1;
}

/* Same format and size as the emulator's own DIB, so it can be presented,
   filtered or uploaded exactly like it. Created on first use. */
static int ensure_fx_dib(void)
{
  struct
  {
    BITMAPINFOHEADER hdr;
    DWORD mask[3];
  } bi;
  HDC screen;

  if (fx.dib && fx.pixels && fx.mem_dc) return 1;

  ZeroMemory(&bi, sizeof(bi));
  bi.hdr.biSize        = sizeof(BITMAPINFOHEADER);
  bi.hdr.biWidth       = FRAME_MAX_W;
  bi.hdr.biHeight      = -FRAME_MAX_H;
  bi.hdr.biPlanes      = 1;
  bi.hdr.biBitCount    = 16;
  bi.hdr.biCompression = BI_BITFIELDS;
  bi.mask[0]           = 0xF800;
  bi.mask[1]           = 0x07E0;
  bi.mask[2]           = 0x001F;

  screen = GetDC(NULL);
  fx.dib = CreateDIBSection(screen, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &fx.pixels, NULL, 0);
  ReleaseDC(NULL, screen);
  if (!fx.dib || !fx.pixels) { fx.dib = NULL; return 0; }

  fx.mem_dc = CreateCompatibleDC(NULL);
  if (!fx.mem_dc) { DeleteObject(fx.dib); fx.dib = NULL; return 0; }
  fx.old_bmp = SelectObject(fx.mem_dc, fx.dib);

  build_brighten_tables();
  return 1;
}

static void fx_free(void)
{
  if (fx.mem_dc)
  {
    SelectObject(fx.mem_dc, fx.old_bmp);
    DeleteDC(fx.mem_dc);
    fx.mem_dc = NULL;
  }
  if (fx.dib)
  {
    DeleteObject(fx.dib);
    fx.dib = NULL;
  }
  fx.pixels = NULL;
}

/* Copies the sw x sh frame into the fx buffer, brightened and/or with every
   second row darkened (scanline_pct% of each channel removed). */
static void fx_process(int sw, int sh, int brighten, int scan_pct)
{
  int keep = 100 - scan_pct;
  int y, x;

  build_brighten_tables();

  for (y = 0; y < sh; y++)
  {
    const uint16 *s = (const uint16 *)((const uint8 *)vid.pixels + (size_t)y * bitmap.pitch);
    uint16 *d = (uint16 *)((uint8 *)fx.pixels + (size_t)y * bitmap.pitch);
    int dark = (scan_pct > 0) && (y & 1);

    for (x = 0; x < sw; x++)
    {
      uint16 p = s[x];
      int r = (p >> 11) & 0x1F;
      int g = (p >> 5)  & 0x3F;
      int b = p & 0x1F;

      if (brighten)
      {
        r = bright5[r];
        g = bright6[g];
        b = bright5[b];
      }
      if (dark)
      {
        r = (r * keep) / 100;
        g = (g * keep) / 100;
        b = (b * keep) / 100;
      }

      d[x] = (uint16)((r << 11) | (g << 5) | b);
    }
  }
}

static void draw_overlay(HDC hdc, const RECT *area)
{
  if (gui.show_fps && emu_running && !emu_paused)
  {
    char buf[32];

    /* Only needed in fullscreen: that's where the FPS counter's corner
       can land in the letterbox/border band, which nothing else repaints
       every frame the way active game content does. In windowed mode the
       window is sized to closely match the game's aspect ratio, so that
       corner is real game content being redrawn every frame anyway --
       clearing it here was just painting an unnecessary black block on
       top of it. */
    if (gui.fullscreen)
    {
      RECT bg;
      bg.right  = area->right;
      bg.left   = area->right - 100;
      bg.top    = area->top;
      bg.bottom = area->top + 24;
      FillRect(hdc, &bg, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    wsprintfA(buf, "%d fps", vid.fps);
    draw_text_shadowed(hdc, area->right - 10, area->top + 8, TA_RIGHT | TA_TOP, buf);
  }

  if (vid.notice[0] && GetTickCount() < vid.notice_until)
  {
    draw_text_shadowed(hdc, area->left + 10, area->bottom - 28, TA_LEFT | TA_TOP, vid.notice);
  }
  else if (vid.notice[0])
  {
    vid.notice[0] = '\0';

    /* The text was drawn directly onto the frame; erasing it relies on
       something redrawing that exact screen area afterward. When it
       falls within the active game rect, the next frame's own content
       does that automatically. When it falls in the border/letterbox
       band instead -- common in fullscreen, where aspect-preserving fit
       often leaves more letterboxing than a properly-sized window does --
       nothing normally repaints that band until the next actual dest-rect
       change, leaving the stale text stuck on screen indefinitely. */
    vid.dest_valid = 0;
    InvalidateRect(g_hwnd, NULL, TRUE);
  }
}

static void present(HDC hdc)
{
  RECT area, dest;
  int sw, sh;
  HDC src_dc, base_dc;
  int src_w, src_h;
  const uint16 *base_pixels;

  get_video_rect(&area);

  if (!emu_running || !bitmap.data)
  {
    HBRUSH br = CreateSolidBrush(theme_is_dark() ? RGB(0x20, 0x20, 0x20) : RGB(0xFF, 0xFF, 0xFF));
    FillRect(hdc, &area, br);
    DeleteObject(br);
    draw_overlay(hdc, &area);
    return;
  }

  get_source_size(&sw, &sh);
  get_dest_rect(&area, sw, sh, &dest);

  /* Brighten (always) and scanlines (GDI path only -- Direct3D draws them
     as a destination-space overlay, see below) are applied to a copy of the
     frame, before any render filter runs and using the true native sw/sh, so
     the filter sees the processed picture and the emulator's own buffer is
     left alone. */
  base_pixels = (const uint16 *)vid.pixels;
  base_dc     = vid.mem_dc;
  {
    int scan_cpu = (gui.scanline_pct > 0) && !hw_renderer_active();

    if ((gui.brighten || scan_cpu) && ensure_fx_dib())
    {
      fx_process(sw, sh, gui.brighten, scan_cpu ? gui.scanline_pct : 0);
      base_pixels = (const uint16 *)fx.pixels;
      base_dc     = fx.mem_dc;
    }
  }

  if (!vid.dest_valid || !EqualRect(&dest, &vid.last_dest))
  {
    fill_borders(hdc, &area, &dest);
    vid.last_dest = dest;
    vid.dest_valid = 1;
  }

  /* dest (the on-screen box) is sized from the aspect-ratio math above,
     which is about the native sw/sh regardless of what buffer gets read
     from -- only the StretchBlt source rect needs to match whichever
     buffer is actually being presented. */
  src_dc = base_dc;
  src_w = sw;
  src_h = sh;

  {
    const uint16 *src_pixels = base_pixels;
    int src_pitch = bitmap.pitch;

    if (nf.index >= 0)
    {
      HDC filtered_dc;
      int filtered_w, filtered_h;

      if (filter_run(base_pixels, bitmap.pitch, sw, sh, &filtered_dc, &filtered_w, &filtered_h))
      {
        src_dc = filtered_dc;
        src_w  = filtered_w;
        src_h  = filtered_h;
        src_pixels = (const uint16 *)nf.pixels;
        src_pitch  = nf.pitch;
      }
    }

    if (hw_renderer_active())
    {
      int (*hw_render_frame)(const uint16 *, int, int, int, const RECT *, int, int, int);
      int (*hw_draw_overlay)(const uint32 *, int, int, int, const RECT *);
      void (*hw_present)(void);

      if (gui.renderer == RENDERER_D3D11)
      {
        hw_render_frame = d3d11_render_frame;
        hw_draw_overlay = d3d11_draw_overlay;
        hw_present      = d3d11_present;
      }
      else
      {
        hw_render_frame = d3d9_render_frame;
        hw_draw_overlay = d3d9_draw_overlay;
        hw_present      = d3d9_present;
      }

      if (hw_render_frame(src_pixels, src_pitch, src_w, src_h, &dest,
                          area.right - area.left, area.bottom - area.top, gui.smooth))
      {
        int area_w = area.right - area.left;
        int area_h = area.bottom - area.top;

        if (gui.scanline_pct > 0 && ensure_scanline_dib(area_w, area_h))
        {
          ensure_scanline_pattern(sw, area_w, area_h, dest.top - area.top, dest.bottom - dest.top);
          hw_draw_overlay((const uint32 *)scan_ovl.pixels, area_w * 4, area_w, area_h, &area);
        }

        if ((gui.show_fps || vid.notice[0]) && ensure_osd_dib(area_w, area_h))
        {
          RECT local_area;
          uint32 *px = (uint32 *)osd.pixels;
          int n = area_w * area_h;
          int i;

          local_area.left = 0; local_area.top = 0;
          local_area.right = area_w; local_area.bottom = area_h;

          /* Black background, then the existing GDI overlay code draws
             into it completely unchanged -- same function, same text
             positions, same shadow. Only what happens to the pixels
             afterward is new. */
          FillRect(osd.mem_dc, &local_area, (HBRUSH)GetStockObject(BLACK_BRUSH));
          draw_overlay(osd.mem_dc, &local_area);

          /* Alpha derived per-pixel: opaque wherever GDI actually drew
             something over the black background, fully transparent
             everywhere else, so only the text (and its shadow) show up
             against the game content underneath rather than a black
             rectangle covering the whole overlay area. */
          for (i = 0; i < n; i++)
          {
            px[i] = (px[i] & 0x00FFFFFF) ? (px[i] | 0xFF000000) : 0x00000000;
          }

          hw_draw_overlay((const uint32 *)osd.pixels, area_w * 4, area_w, area_h, &area);
        }

        hw_present();
        return;
      }
      /* Not available right now (device lost, still recovering, etc.) --
         fall through to the GDI path below for this one frame rather
         than show nothing. */
    }
  }

  /* The filter's output is a fixed multiple of the source (2x, 3x or 4x), which
     essentially never exactly matches the actual destination rect -- this
     final stretch is doing real, arbitrary-ratio resampling regardless of
     whether a filter already ran, and skipping smooth interpolation here
     produces visible blockiness. Always respects the person's own Smooth
     Scaling setting rather than overriding it. */
  if (gui.smooth)
  {
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
  }
  else
  {
    SetStretchBltMode(hdc, COLORONCOLOR);
  }

  StretchBlt(hdc,
             dest.left, dest.top,
             dest.right - dest.left, dest.bottom - dest.top,
             src_dc,
             0, 0, src_w, src_h,
             SRCCOPY);

  draw_overlay(hdc, &area);
}

void video_frame(void)
{
  HDC hdc = GetDC(g_hwnd);
  if (!hdc) return;
  present(hdc);
  ReleaseDC(g_hwnd, hdc);
  vsync_wait();
}

void video_repaint(HDC hdc)
{
  vid.dest_valid = 0;   /* force a border repaint */
  present(hdc);
}

void video_report_fps(int fps)
{
  vid.fps = fps;
}

void video_show_notice(const char *text, int ms)
{
  /* A previous notice still on screen won't be erased by anything before
     this one draws -- draw_text_shadowed() uses a transparent background,
     so without this the old text and the new text both stay visible,
     overlapping (e.g. spamming the slot-change key showed every slot
     number stacked on top of the last). Same repaint used when a notice
     naturally expires. */
  if (vid.notice[0])
  {
    vid.dest_valid = 0;
    InvalidateRect(g_hwnd, NULL, TRUE);
  }

  lstrcpynA(vid.notice, text, sizeof(vid.notice));
  vid.notice_until = GetTickCount() + ms;
}

/****************************************************************************
 * Fullscreen
 ****************************************************************************/

void video_set_fullscreen(int on)
{
  if (on == vid.fullscreen) return;

  /*
   * Set before touching the window, not after: SetWindowPos below fires
   * WM_SIZE/WM_MOVE synchronously, and the window-position persistence in
   * main.c checks gui.fullscreen to decide whether to save what it sees.
   * Setting this last would let a fullscreen-sized rect get saved as if it
   * were the normal window position.
   */
  vid.fullscreen = on;
  gui.fullscreen = on;

  if (on)
  {
    MONITORINFO mi;
    HMONITOR mon;

    vid.saved_style    = (DWORD)GetWindowLongPtr(g_hwnd, GWL_STYLE);
    vid.saved_ex_style = (DWORD)GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
    vid.saved_place.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(g_hwnd, &vid.saved_place);
    vid.saved_menu = GetMenu(g_hwnd);

    mi.cbSize = sizeof(mi);
    mon = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfo(mon, &mi))
    {
      vid.fullscreen = 0;      /* nothing was changed: do not claim fullscreen */
      gui.fullscreen = 0;
      return;
    }

    SetMenu(g_hwnd, NULL);
    if (g_status) ShowWindow(g_status, SW_HIDE);

    SetWindowLongPtr(g_hwnd, GWL_STYLE,
                     (vid.saved_style & ~(WS_CAPTION | WS_THICKFRAME)) | WS_POPUP);
    SetWindowLongPtr(g_hwnd, GWL_EXSTYLE,
                     vid.saved_ex_style & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                            WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

    SetWindowPos(g_hwnd, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    while (ShowCursor(FALSE) >= 0) { }
  }
  else
  {
    SetWindowLongPtr(g_hwnd, GWL_STYLE, vid.saved_style);
    SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, vid.saved_ex_style);
    SetMenu(g_hwnd, vid.saved_menu);
    if (g_status) ShowWindow(g_status, SW_SHOW);

    SetWindowPlacement(g_hwnd, &vid.saved_place);
    SetWindowPos(g_hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    while (ShowCursor(TRUE) < 0) { }
  }

  vid.dest_valid = 0;
  InvalidateRect(g_hwnd, NULL, TRUE);
}

/****************************************************************************
 * Screenshots
 *
 * Written as PNG. zlib is already linked for ZIP ROM support, so the encoder
 * is a few lines of chunk plumbing rather than a new dependency.
 ****************************************************************************/

static void png_put_u32(unsigned char *p, unsigned long v)
{
  p[0] = (unsigned char)(v >> 24);
  p[1] = (unsigned char)(v >> 16);
  p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)(v);
}

static int png_write_chunk(FILE *f, const char *type, const unsigned char *data,
                           unsigned long len)
{
  unsigned char head[8];
  unsigned char crcbuf[4];
  unsigned long crc;

  png_put_u32(head, len);
  memcpy(head + 4, type, 4);
  if (fwrite(head, 1, 8, f) != 8) return 0;

  crc = crc32(0L, head + 4, 4);
  if (len)
  {
    if (fwrite(data, 1, len, f) != len) return 0;
    crc = crc32(crc, data, len);
  }

  png_put_u32(crcbuf, crc);
  return fwrite(crcbuf, 1, 4, f) == 4;
}

/* The emulator's current frame exactly as the core drew it (RGB565): no
   scanlines, no Brighten, no render filter. Used by the recorder. */
int video_capture_frame(const unsigned short **pixels, int *pitch, int *w, int *h)
{
  int sw, sh;

  if (!emu_running || !bitmap.data) return 0;

  get_source_size(&sw, &sh);
  *pixels = (const unsigned short *)bitmap.data;
  *pitch  = bitmap.pitch;
  *w = sw;
  *h = sh;
  return 1;
}

/* Screenshot image building. Everything here works on packed 24-bit RGB. */

/* Expands an RGB565 picture to packed RGB888. */
static unsigned char *shot_expand(const uint8 *base, int pitch, int w, int h)
{
  unsigned char *out = (unsigned char *)malloc((size_t)w * h * 3);
  unsigned char *dst = out;
  int x, y;

  if (!out) return NULL;

  for (y = 0; y < h; y++)
  {
    const uint16 *src = (const uint16 *)(base + (size_t)y * pitch);

    for (x = 0; x < w; x++)
    {
      uint16 p = src[x];
      int r = (p >> 11) & 0x1F;
      int g = (p >> 5)  & 0x3F;
      int b =  p        & 0x1F;

      /* Replicate the high bits so full white stays full white. */
      *dst++ = (unsigned char)((r << 3) | (r >> 2));
      *dst++ = (unsigned char)((g << 2) | (g >> 4));
      *dst++ = (unsigned char)((b << 3) | (b >> 2));
    }
  }
  return out;
}

/* Bilinear resize of packed RGB888 (16.16 fixed point, pixel centres aligned). */
static unsigned char *shot_resize(const unsigned char *in, int iw, int ih, int ow, int oh)
{
  unsigned char *out = (unsigned char *)malloc((size_t)ow * oh * 3);
  int *x0 = (int *)malloc(sizeof(int) * (size_t)ow * 2);
  int *fx = x0 ? x0 + ow : NULL;
  int x, y, c;

  if (!out || !x0)
  {
    free(out);
    free(x0);
    return NULL;
  }

  for (x = 0; x < ow; x++)
  {
    long long sx = ((long long)(2 * x + 1) * iw * 65536) / (2 * (long long)ow) - 32768;
    if (sx < 0) sx = 0;
    if (sx > (long long)(iw - 1) * 65536) sx = (long long)(iw - 1) * 65536;
    x0[x] = (int)(sx >> 16);
    fx[x] = (int)(sx & 0xFFFF);
  }

  for (y = 0; y < oh; y++)
  {
    long long sy = ((long long)(2 * y + 1) * ih * 65536) / (2 * (long long)oh) - 32768;
    int y0, y1, fy;
    const unsigned char *r0, *r1;
    unsigned char *dst = out + (size_t)y * ow * 3;

    if (sy < 0) sy = 0;
    if (sy > (long long)(ih - 1) * 65536) sy = (long long)(ih - 1) * 65536;
    y0 = (int)(sy >> 16);
    fy = (int)(sy & 0xFFFF);
    y1 = (y0 + 1 < ih) ? y0 + 1 : y0;
    r0 = in + (size_t)y0 * iw * 3;
    r1 = in + (size_t)y1 * iw * 3;

    for (x = 0; x < ow; x++)
    {
      int xa = x0[x] * 3;
      int xb = ((x0[x] + 1 < iw) ? x0[x] + 1 : x0[x]) * 3;
      int wx = fx[x];

      for (c = 0; c < 3; c++)
      {
        int top = (r0[xa + c] * (65536 - wx) + r0[xb + c] * wx) >> 16;
        int bot = (r1[xa + c] * (65536 - wx) + r1[xb + c] * wx) >> 16;
        *dst++ = (unsigned char)((top * (65536 - fy) + bot * fy) >> 16);
      }
    }
  }

  free(x0);
  return out;
}

/*
 * Builds the picture for the chosen screenshot output (Tools >
 * Screenshot Output):
 *   Raw        exactly what the emulator drew, at its own resolution
 *   Corrected  the raw picture stretched to a 4:3 640x480 frame
 *   Final      the render filter applied first, then stretched to 4:3 at the
 *              filter's output width (so a 2x filter gives 640x480 and a 3x
 *              one 960x720); with no filter chosen it equals Corrected
 * Scanlines and Brighten are display effects and are never included.
 */
static unsigned char *shot_build(int *out_w, int *out_h)
{
  int sw, sh;
  unsigned char *raw, *img;

  get_source_size(&sw, &sh);

  if (gui.shot_mode == 2)   /* raw */
  {
    raw = shot_expand(bitmap.data, bitmap.pitch, sw, sh);
    *out_w = sw;
    *out_h = sh;
    return raw;
  }

  if (gui.shot_mode == 0)   /* final: through the render filter first */
  {
    HDC dc;
    int fw, fh;

    if (filter_run((const uint16 *)bitmap.data, bitmap.pitch, sw, sh, &dc, &fw, &fh))
    {
      int th = (fw * 3 / 4) & ~1;

      raw = shot_expand((const uint8 *)nf.pixels, (int)nf.pitch, fw, fh);
      if (!raw) return NULL;
      img = shot_resize(raw, fw, fh, fw, th);
      free(raw);
      *out_w = fw;
      *out_h = th;
      return img;
    }
  }

  /* corrected (also final with no filter active) */
  raw = shot_expand(bitmap.data, bitmap.pitch, sw, sh);
  if (!raw) return NULL;
  img = shot_resize(raw, sw, sh, 640, 480);
  free(raw);
  *out_w = 640;
  *out_h = 480;
  return img;
}

int video_screenshot(char *path_out, int path_len)
{
  int sw, sh, y;
  unsigned long raw_len, comp_len;
  unsigned char *rgb = NULL, *raw = NULL, *comp = NULL;
  unsigned char ihdr[13];
  FILE *f = NULL;
  SYSTEMTIME st;
  char dir[GUI_PATH_LEN];
  char path[GUI_PATH_LEN];
  int ok = 0;

  if (!emu_running || !bitmap.data) return 0;

  rgb = shot_build(&sw, &sh);
  if (!rgb) return 0;

  /* One filter byte (None) in front of each scanline, as PNG wants. */
  raw_len = (unsigned long)sh * (1 + (unsigned long)sw * 3);
  raw = (unsigned char *)malloc(raw_len);
  if (!raw) goto done;

  for (y = 0; y < sh; y++)
  {
    unsigned char *row = raw + (size_t)y * (1 + (size_t)sw * 3);
    *row++ = 0;
    memcpy(row, rgb + (size_t)y * sw * 3, (size_t)sw * 3);
  }

  comp_len = compressBound(raw_len);
  comp = (unsigned char *)malloc(comp_len);
  if (!comp) goto done;

  if (compress2(comp, &comp_len, raw, raw_len, Z_BEST_COMPRESSION) != Z_OK) goto done;

  lstrcpynA(dir, osd_path("screenshots"), sizeof(dir));
  CreateDirectoryA(dir, NULL);

  GetLocalTime(&st);
  {
    const char *game = emu_rom_base();
    int n;

    if (!game || !game[0]) game = "gpgx";
    gui_snprintf(path, (int)sizeof(path), "%s\\%s_%04d%02d%02d_%02d%02d%02d.png",
              dir, game, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    /* Two shots in the same second get _2, _3, ... instead of overwriting. */
    for (n = 2; n < 100 && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; n++)
      gui_snprintf(path, (int)sizeof(path), "%s\\%s_%04d%02d%02d_%02d%02d%02d_%d.png",
                dir, game, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, n);
  }

  f = fopen(path, "wb");
  if (!f) goto done;

  if (fwrite("\211PNG\r\n\032\n", 1, 8, f) != 8) goto done;

  png_put_u32(ihdr + 0, (unsigned long)sw);
  png_put_u32(ihdr + 4, (unsigned long)sh);
  ihdr[8]  = 8;   /* bit depth   */
  ihdr[9]  = 2;   /* colour type: truecolour */
  ihdr[10] = 0;   /* deflate     */
  ihdr[11] = 0;   /* adaptive filtering */
  ihdr[12] = 0;   /* no interlace */

  if (!png_write_chunk(f, "IHDR", ihdr, sizeof(ihdr))) goto done;
  if (!png_write_chunk(f, "IDAT", comp, comp_len)) goto done;
  if (!png_write_chunk(f, "IEND", NULL, 0)) goto done;

  ok = 1;
  if (path_out && path_len > 0) lstrcpynA(path_out, path, path_len);

done:
  if (f) fclose(f);
  free(rgb);
  free(raw);
  free(comp);
  return ok;
}

/****************************************************************************
 * Save-state thumbnails
 *
 * Stored as plain, uncompressed 24-bit BMPs (not the PNG encoder above --
 * these need to be loaded back and shown as HBITMAPs in the state manager
 * dialog, and a raw BMP round-trips through CreateDIBSection with no
 * decoder needed at all, where PNG would need one written or linked in
 * just for this). 128 wide keeps each row (128*3=384 bytes) already a
 * multiple of 4, so there's no row padding to account for when writing.
 ****************************************************************************/

#define THUMB_W 128
#define THUMB_H 96

int video_save_thumbnail(const char *path)
{
  int sw, sh, x, y;
  FILE *f;
  unsigned char header[54];
  unsigned char *row;
  unsigned long row_bytes = (unsigned long)THUMB_W * 3;
  unsigned long image_size = row_bytes * THUMB_H;
  unsigned long file_size = 54 + image_size;
  long w = THUMB_W, h = THUMB_H;

  if (!emu_running || !bitmap.data) return 0;

  get_source_size(&sw, &sh);
  if (sw <= 0 || sh <= 0) return 0;

  f = fopen(path, "wb");
  if (!f) return 0;

  memset(header, 0, sizeof(header));
  header[0] = 'B'; header[1] = 'M';
  memcpy(header + 2, &file_size, 4);
  header[10] = 54;             /* pixel data offset -- fits in one byte here */
  header[14] = 40;             /* biSize (BITMAPINFOHEADER) */
  memcpy(header + 18, &w, 4);
  memcpy(header + 22, &h, 4);  /* positive: bottom-up, standard BMP row order */
  header[26] = 1;              /* biPlanes */
  header[28] = 24;             /* biBitCount */
  memcpy(header + 34, &image_size, 4);

  if (fwrite(header, 1, 54, f) != 54) { fclose(f); return 0; }

  row = (unsigned char *)malloc(row_bytes);
  if (!row) { fclose(f); return 0; }

  for (y = THUMB_H - 1; y >= 0; y--)
  {
    int src_y = (y * sh) / THUMB_H;
    const uint16 *src = (const uint16 *)(bitmap.data + (size_t)src_y * bitmap.pitch);

    for (x = 0; x < THUMB_W; x++)
    {
      int src_x = (x * sw) / THUMB_W;
      uint16 p = src[src_x];
      int r = (p >> 11) & 0x1F;
      int g = (p >> 5)  & 0x3F;
      int b =  p        & 0x1F;

      /* BMP stores BGR, not RGB. */
      row[x * 3 + 0] = (unsigned char)((b << 3) | (b >> 2));
      row[x * 3 + 1] = (unsigned char)((g << 2) | (g >> 4));
      row[x * 3 + 2] = (unsigned char)((r << 3) | (r >> 2));
    }
    if (fwrite(row, 1, row_bytes, f) != row_bytes) { free(row); fclose(f); return 0; }
  }

  free(row);
  fclose(f);
  return 1;
}

/* Returns an HBITMAP the caller owns (DeleteObject it when done), or NULL
   if the file doesn't exist or isn't a BMP this function wrote -- both
   entirely normal (a slot that's never been saved to has no thumbnail
   file at all), not error conditions worth reporting further. */
HBITMAP video_load_thumbnail(const char *path)
{
  FILE *f;
  unsigned char header[54];
  BITMAPINFO bmi;
  HBITMAP hbm;
  void *bits = NULL;
  HDC hdc;
  long w, h;
  unsigned long pixel_offset, row_bytes;

  f = fopen(path, "rb");
  if (!f) return NULL;

  if (fread(header, 1, 54, f) != 54 || header[0] != 'B' || header[1] != 'M')
  {
    fclose(f);
    return NULL;
  }

  memcpy(&w, header + 18, 4);
  memcpy(&h, header + 22, 4);
  pixel_offset = header[10] | ((unsigned long)header[11] << 8) |
                 ((unsigned long)header[12] << 16) | ((unsigned long)header[13] << 24);

  if (w <= 0 || h <= 0 || w > 4096 || h > 4096) { fclose(f); return NULL; }

  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth    = w;
  bmi.bmiHeader.biHeight   = h;
  bmi.bmiHeader.biPlanes   = 1;
  bmi.bmiHeader.biBitCount = 24;
  bmi.bmiHeader.biCompression = BI_RGB;

  hdc = GetDC(NULL);
  hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
  ReleaseDC(NULL, hdc);

  if (hbm && bits)
  {
    row_bytes = (((unsigned long)w * 3 + 3) / 4) * 4;
    fseek(f, (long)pixel_offset, SEEK_SET);
    fread(bits, 1, row_bytes * (unsigned long)h, f);
  }

  fclose(f);
  return hbm;
}

