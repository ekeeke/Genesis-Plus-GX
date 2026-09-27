/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  d3d9_video.h -- optional Direct3D9 output path.
 *
 *  GDI's smooth (HALFTONE) stretch is a pure software resampling pass with
 *  no hardware acceleration, regardless of the graphics card -- fine at
 *  native-ish sizes, genuinely expensive filling a 1080p+ display from an
 *  already-upscaled xBRZ frame every 16ms. This hands the same job to the
 *  GPU instead: upload the frame as a texture, let hardware bilinear
 *  filtering do the stretch. Always used in windowed mode (D3D9's own
 *  exclusive fullscreen is never used) -- the app's "fullscreen" is a
 *  borderless popup window, and this just renders into whatever the
 *  current window rect is, same as the GDI path does.
 *
 *  Every entry point is safe to call even when the device isn't
 *  available: init failure, or any per-frame failure, is reported back
 *  so the caller can fall back to the existing GDI path for that frame.
 *  Nothing here is ever the only way to get a picture on screen.
 ****************************************************************************/

#ifndef D3D9_VIDEO_H
#define D3D9_VIDEO_H

#include <windows.h>
#include "types.h"

/* Tries to create a device for this window. Returns 1 on success, 0 on
   any failure (missing d3d9.dll, no compatible adapter, device creation
   failed, etc.) -- 0 is a normal, expected outcome on some systems, not
   an error to surface loudly. */
int  d3d9_init(HWND hwnd);

void d3d9_shutdown(void);

/* Whether the device is currently usable. False after init failure, and
   temporarily false after the device is lost (e.g. some alt-tab/resolution
   change sequences) until it's successfully recovered. */
int  d3d9_available(void);

/* Call after the window's size changes (including entering/leaving
   fullscreen) so the swap chain gets resized to match. */
void d3d9_notify_resize(void);
void d3d9_set_vsync(int on);

/* Renders one frame: clears the whole client area to black (handling the
   letterbox/border), then draws src_pixels (RGB565, src_pitch bytes per
   row, src_w x src_h) stretched into dest with hardware bilinear
   filtering when smooth is set, nearest-neighbour otherwise. client_w/h
   is the current client area size. Returns 1 if the frame was rendered
   (the caller should then draw its overlay and call d3d9_present()), 0
   if the device isn't usable right now -- the caller should fall back to
   the GDI path for this one frame rather than treat it as fatal. */
int  d3d9_render_frame(const uint16 *src_pixels, int src_pitch,
                        int src_w, int src_h, const RECT *dest,
                        int client_w, int client_h, int smooth);

/* Draws argb_pixels (32-bit 0x00RRGGBB or 0xAARRGGBB, argb_pitch bytes
   per row, w x h) as an alpha-blended overlay covering screen_area, on
   top of whatever d3d9_render_frame() already drew this frame. Must be
   called after a successful d3d9_render_frame() and before
   d3d9_present() -- as part of the same frame, not a separate draw
   afterward, which is what caused visible flashing in an earlier
   version of this: a GDI draw living outside D3D9's own buffer keeps
   getting overwritten by the next frame's Present() before it can
   really be seen. Safe to skip calling entirely on frames with nothing
   to overlay. */
int  d3d9_draw_overlay(const uint32 *argb_pixels, int argb_pitch,
                        int w, int h, const RECT *screen_area);

void d3d9_present(void);

#endif
