/*
 * autocrop.h - automatic black-border crop for emulators
 *
 * Copyright (c) 2026 VrepliroidV
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * C port of the "Auto Crop Black Borders" option of the snes9x-ntsc-lie-pal
 * Libretro core (https://github.com/VrepliroidV/snes9x-ntsc-lie-pal), with the
 * same rules:
 *
 *  - Lines drawn with the display off (hardware rows) are cropped after 2
 *    frames, as long as the picture between them is not mostly dark.
 *  - Lines of one single dim colour (dark grey bars) count as border too.
 *  - Borders that are only dark pixels are cropped only on screens that look
 *    like gameplay: the picture fills at least 40% of the screen, each border
 *    is at most 1/4 of the picture and the edge next to it is a solid line of
 *    graphics. The first time, such a border must stay unchanged for 1.5 s
 *    while the picture moves, or for 8 s on a still picture.
 *  - A layout shown for 5 s is remembered for the game and cropped in the
 *    first frame it comes back. A remembered layout contradicted twice in a
 *    row (graphics inside the cropped area right after it was applied) is
 *    forgotten.
 *  - Graphics inside the cropped area restore that side in the same frame.
 *    Black and dark frames keep the current crop.
 *
 * Units: horizontally 1/256 of the frame width, vertically one emulated line
 * (output rows / vscale). Frames are RGB565, stored either row after row or
 * as a GX texture (4x4 pixel tiles).
 */

#ifndef AUTOCROP_H
#define AUTOCROP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum autocrop_mode
{
	AUTOCROP_OFF = 0,
	AUTOCROP_FIT = 1,     /* keep the pixel shape */
	AUTOCROP_STRETCH = 2, /* fill the area of the whole frame */
	AUTOCROP_MODES
};

/* How the cropped picture is enlarged (chosen by the emulator) */
enum autocrop_scaling
{
	AUTOCROP_SCALE_SMOOTH = 0,  /* fills the frame; sharp scaling when not bilinear */
	AUTOCROP_SCALE_INTEGER = 1, /* whole-number scaling only: every pixel the same size */
	AUTOCROP_SCALINGS
};

enum
{
	AUTOCROP_TOP,
	AUTOCROP_BOTTOM,
	AUTOCROP_LEFT,
	AUTOCROP_RIGHT,
	AUTOCROP_SIDES
};

#define AUTOCROP_UNITS       256 /* horizontal units across the frame */
#define AUTOCROP_MAX_LAYOUTS 16
#define AUTOCROP_GRID_WIDTH  64
#define AUTOCROP_GRID_HEIGHT 60

typedef struct
{
	int band[AUTOCROP_SIDES];
	int strikes;
	int established;
} autocrop_layout;

typedef struct
{
	int applied[AUTOCROP_SIDES]; /* current crop, in units */
	int pending[AUTOCROP_SIDES];
	int stable[AUTOCROP_SIDES];
	int motion[AUTOCROP_SIDES];
	int since_expand;
	int applied_frames;
	int instant_layout;          /* remembered layout applied at once, or -1 */
	int instant_frames;
	int layout_count;
	autocrop_layout layouts[AUTOCROP_MAX_LAYOUTS];
	int layouts_changed;         /* the saved (established) set changed */
	int has_grid;
	uint16_t grid[AUTOCROP_GRID_HEIGHT][AUTOCROP_GRID_WIDTH];
} autocrop_state;

typedef struct
{
	const uint16_t *pixels;       /* RGB565, top-left pixel of the frame */
	int pitch;                    /* pixels per row; texture width when tiled */
	int tiled;                    /* GX texture layout: 4x4 pixel tiles */
	int width, height;            /* in pixels */
	int vscale;                   /* output rows per emulated line (1 or 2) */
	const uint8_t *hardware_rows; /* per output row, non-zero when drawn with
	                                 the display off; may be NULL */
} autocrop_frame;

/* Forgets everything, including the layouts learned for the game. */
void autocrop_reset(autocrop_state *state);

/* Forgets the current crop but keeps the layouts learned for the game. */
void autocrop_reset_tracking(autocrop_state *state);

/* Measures one frame and updates the crop. Returns 1 when state->applied
   changed. */
int autocrop_process(autocrop_state *state, const autocrop_frame *frame);

/* Layout memory file of a game. Loading replaces the learned layouts. */
void autocrop_load_layouts(autocrop_state *state, const char *path);
/* Writes the file when the remembered layouts changed since the last load or
   save (or deletes it when none is left). Returns 1 when it wrote. */
int autocrop_save_layouts(autocrop_state *state, const char *path);

/* Where to draw the crop. For a frame of width pixels and lines emulated
   lines, u0..u1 and v0..v1 receive the visible part of the frame as
   fractions (0 to 1), and xsize, ysize the size to draw it at, relative to
   the area the whole frame is drawn in.

   Stretch fills that area. Fit enlarges the picture evenly, as a frontend
   does when it fits the picture to its screen: xroom and yroom are the
   screen size divided by the frame area (1 and 1 when the frame fills the
   screen). The uncropped frame keeps its size, and a cropped picture grows
   until it meets a box of the screen's shape around the original frame. */
void autocrop_view(const autocrop_state *state, int mode, int width, int lines, float xroom, float yroom,
                   float *u0, float *u1, float *v0, float *v1, float *xsize, float *ysize);

#ifdef __cplusplus
}
#endif

#endif /* AUTOCROP_H */
