/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  d3d11_video.h -- optional Direct3D 11 output path.
 *
 *  A second, independent hardware-accelerated path alongside d3d9_video.c
 *  (see that file's own header for the rationale for having one at all).
 *  Two reasons to want this one too, not instead:
 *
 *   - Some newer GPU drivers put far less optimization effort into the
 *     now-legacy D3D9 path than into D3D11/12; on a small number of systems
 *     D3D9 is the worse-performing or buggier of the two. Offering both
 *     lets a person route around whichever one their particular driver
 *     handles badly, without losing the other for everyone else.
 *   - It is the API a GPU shader post-processing library such as
 *     librashader (github.com/SnowflakePowered/librashader) actually
 *     targets -- D3D9 support there is explicitly limited/experimental,
 *     so real shader-preset support needs this path to exist regardless
 *     of whether D3D9 stays the default.
 *
 *  Same contract as d3d9_video.h throughout: every entry point is safe to
 *  call even when the device isn't available, and any failure is reported
 *  back so the caller falls back to GDI (or to the D3D9 path, if that one
 *  is available) for that frame rather than being treated as fatal.
 ****************************************************************************/

#ifndef D3D11_VIDEO_H
#define D3D11_VIDEO_H

#include <windows.h>
#include "types.h"

/* Tries to create a device and swap chain for this window. Returns 1 on
   success, 0 on any failure (missing d3d11.dll/dxgi.dll, no compatible
   adapter, shader compiler unavailable, device creation failed, etc.) --
   0 is a normal, expected outcome on some systems, not an error to surface
   loudly. */
int  d3d11_init(HWND hwnd);

void d3d11_shutdown(void);

/* Whether the device is currently usable. False after init failure, and
   temporarily false after the swap chain fails to present (e.g. some
   display-change sequences) until it's successfully recreated. */
int  d3d11_available(void);

/* Call after the window's size changes (including entering/leaving
   fullscreen) so the swap chain gets resized to match. */
void d3d11_notify_resize(void);
void d3d11_set_vsync(int on);

/* Renders one frame: clears the whole client area to black (handling the
   letterbox/border), then draws src_pixels (RGB565, src_pitch bytes per
   row, src_w x src_h) stretched into dest with hardware bilinear
   filtering when smooth is set, nearest-neighbour otherwise. client_w/h
   is the current client area size. Returns 1 if the frame was rendered
   (the caller should then draw its overlay and call d3d11_present()), 0
   if the device isn't usable right now -- the caller should fall back to
   another path for this one frame rather than treat it as fatal. */
int  d3d11_render_frame(const uint16 *src_pixels, int src_pitch,
                         int src_w, int src_h, const RECT *dest,
                         int client_w, int client_h, int smooth);

/* Draws argb_pixels (32-bit 0x00RRGGBB or 0xAARRGGBB, argb_pitch bytes
   per row, w x h) as an alpha-blended overlay covering screen_area, on
   top of whatever d3d11_render_frame() already drew this frame. Must be
   called after a successful d3d11_render_frame() and before
   d3d11_present(), as part of the same frame. Safe to skip calling
   entirely on frames with nothing to overlay. */
int  d3d11_draw_overlay(const uint32 *argb_pixels, int argb_pitch,
                         int w, int h, const RECT *screen_area);

void d3d11_present(void);

/* For a future shader post-processing pass (e.g. librashader): the live
   device, context and the texture the current frame was uploaded into,
   valid only between a successful d3d11_render_frame() and the matching
   d3d11_present(). Returns 0 (leaving the outputs untouched) if not
   currently available. Deliberately typed as void* here so this header
   stays includable from files that don't otherwise need <d3d11.h>; the
   real types are ID3D11Device*, ID3D11DeviceContext* and
   ID3D11ShaderResourceView* respectively. */
int  d3d11_get_frame_resources(void **device, void **context, void **frame_srv);

#endif
