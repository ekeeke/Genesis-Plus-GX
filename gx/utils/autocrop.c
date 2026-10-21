/*
 * autocrop.c - automatic black-border crop for emulators
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
 * See autocrop.h.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "autocrop.h"

/* First-time waits, in frames. Pixel-only borders need moving frames, or a
   much longer unchanged period on still screens (paused or idle games);
   logo screens rarely stay that long. */
#define HARDWARE_FRAMES      2
#define PIXEL_FRAMES         90
#define PIXEL_MOTION_FRAMES  10
#define PIXEL_STILL_FRAMES   480
/* A layout shown this long is remembered and saved. */
#define ESTABLISH_FRAMES     300
/* After graphics restore a side, remembered layouts wait this many frames,
   so a sprite crossing the border cannot make the picture jump every frame. */
#define MEMORY_COOLDOWN      30
/* Graphics restoring a remembered layout this soon count against it. */
#define STRIKE_FRAMES        60
#define MAX_STRIKES          2
/* Lines of one single colour up to this level (of 31) are borders too:
   dark grey bars instead of black ones. */
#define DIM_LEVEL            6
/* Thinner borders are left alone (units). */
#define MIN_BAND             2
/* Rows or columns next to a band checked for a solid edge (units). */
#define EDGE_DEPTH           4
#define MIN_EDGE_PERCENT     25
/* Non-dark share of the picture inside hardware bands. */
#define MIN_HARDWARE_FILL_PERCENT 25
/* Non-dark share required to trust pixel-only bands: whole picture and the
   area left after the crop. */
#define MIN_SCREEN_FILL_PERCENT   40
#define MIN_CONTENT_FILL_PERCENT  25
/* Motion grid: one sample every 4 units; a frame moves when this many
   samples change. */
#define GRID_STEP            4
#define MOTION_SAMPLES       4
#define MAX_BAND_UNITS       240
#define FILE_HEADER          "wii-autocrop 1"

static inline int ac_min(int a, int b) { return a < b ? a : b; }
static inline int ac_max(int a, int b) { return a > b ? a : b; }

static inline uint16_t ac_pixel(const autocrop_frame *f, int x, int y)
{
	if (f->tiled)
		return f->pixels[((((y >> 2) * (f->pitch >> 2)) + (x >> 2)) << 4) + ((y & 3) << 2) + (x & 3)];
	return f->pixels[(ptrdiff_t)y * f->pitch + x];
}

/* RGB565: 5 bits of red, the top 5 of the 6 green bits, 5 bits of blue. */
static inline int ac_dark(uint16_t p)
{
	return (p >> 11) <= 2 && ((p >> 6) & 0x1f) <= 2 && (p & 0x1f) <= 2;
}

static inline int ac_dim(uint16_t p)
{
	return (p >> 11) <= DIM_LEVEL && ((p >> 6) & 0x1f) <= DIM_LEVEL && (p & 0x1f) <= DIM_LEVEL;
}

/* Pixels of n units, at least one. */
static inline int ac_xpixels(const autocrop_frame *f, int units)
{
	return ac_max(1, units * f->width / AUTOCROP_UNITS);
}

/* A border line is all dark, or all one dim colour (a dark grey bar keeps the
   same colour when the game changes brightness, unlike its dark parts). */
static int ac_row_border(const autocrop_frame *f, int y)
{
	const uint16_t first = ac_pixel(f, 0, y);
	int dark = 1, uniform = ac_dim(first);
	for (int x = 0; x < f->width; x++)
	{
		const uint16_t p = ac_pixel(f, x, y);
		dark = dark && ac_dark(p);
		uniform = uniform && p == first;
		if (!dark && !uniform)
			return 0;
	}
	return 1;
}

static int ac_column_border(const autocrop_frame *f, int x, int y0, int y1)
{
	const uint16_t first = ac_pixel(f, x, y0);
	int dark = 1, uniform = ac_dim(first);
	for (int y = y0; y < y1; y++)
	{
		const uint16_t p = ac_pixel(f, x, y);
		dark = dark && ac_dark(p);
		uniform = uniform && p == first;
		if (!dark && !uniform)
			return 0;
	}
	return 1;
}

static inline int ac_hardware(const autocrop_frame *f, int y)
{
	return f->hardware_rows && f->hardware_rows[y];
}

/* Percentage of samples in a rectangle of pixels that are neither dark nor
   the border colour (ignore, or -1 for none). */
static int ac_fill(const autocrop_frame *f, int x0, int x1, int y0, int y1, int xstep, int ystep, int ignore)
{
	long content = 0, samples = 0;
	for (int y = y0; y < y1; y += ystep)
		for (int x = x0; x < x1; x += xstep)
		{
			const uint16_t p = ac_pixel(f, x, y);
			samples++;
			content += !ac_dark(p) && (int)p != ignore;
		}
	return samples ? (int)(content * 100 / samples) : 0;
}

/* Measures the dark border of one frame. Returns 0 when the whole frame is
   dark (fades, black screens): those frames must not change the crop. */
static int ac_measure(const autocrop_frame *f, int raw[AUTOCROP_SIDES], int trusted[AUTOCROP_SIDES],
                      int hardware[AUTOCROP_SIDES])
{
	const int w = f->width, h = f->height, vs = f->vscale;
	const int base_height = h / vs;
	int top = 0, bottom = 0, left = 0, right = 0;

	while (top < h && ac_row_border(f, top))
		top++;
	if (top == h)
		return 0;
	while (bottom < h - top && ac_row_border(f, h - 1 - bottom))
		bottom++;

	const int y0 = top, y1 = h - bottom;
	while (left < w && ac_column_border(f, left, y0, y1))
		left++;
	if (left == w)
		return 0;
	while (right < w - left && ac_column_border(f, w - 1 - right, y0, y1))
		right++;
	const int x0 = left, x1 = w - right;

	/* A dark grey border colour does not count as picture in the fill checks,
	   so a logo on a grey background is treated like one on black. */
	int ignore = -1;
	{
		int cx = -1, cy = -1;
		if (top) { cx = 0; cy = 0; }
		else if (bottom) { cx = 0; cy = h - 1; }
		else if (left) { cx = 0; cy = y0; }
		else if (right) { cx = w - 1; cy = y0; }
		if (cx >= 0)
		{
			const uint16_t corner = ac_pixel(f, cx, cy);
			if (!ac_dark(corner) && ac_dim(corner))
				ignore = corner;
		}
	}

	/* Display-off lines at the outer edge of the top and bottom bands. */
	int hardware_top = 0, hardware_bottom = 0;
	while (hardware_top < top && ac_hardware(f, hardware_top))
		hardware_top++;
	while (hardware_bottom < bottom && ac_hardware(f, h - 1 - hardware_bottom))
		hardware_bottom++;

	/* Round toward zero so a partial unit of content is never cropped. */
	raw[AUTOCROP_TOP]    = top / vs;
	raw[AUTOCROP_BOTTOM] = bottom / vs;
	raw[AUTOCROP_LEFT]   = left * AUTOCROP_UNITS / w;
	raw[AUTOCROP_RIGHT]  = right * AUTOCROP_UNITS / w;
	int hardware_band[AUTOCROP_SIDES] = { hardware_top / vs, hardware_bottom / vs, 0, 0 };
	for (int side = 0; side < AUTOCROP_SIDES; side++)
	{
		if (raw[side] < MIN_BAND)
			raw[side] = 0;
		if (hardware_band[side] < MIN_BAND)
			hardware_band[side] = 0;
		hardware[side] = 0;
		trusted[side] = 0;
	}

	const int xstep = ac_xpixels(f, 4), ystep = 4 * vs;

	/* The picture between the display-off lines must not be mostly dark (a
	   logo between black bars), and the bars must leave half the height. */
	const int screen_fill = ac_fill(f, 0, w, hardware_top, h - hardware_bottom, xstep, ystep, ignore);
	const int trust_hardware = screen_fill >= MIN_HARDWARE_FILL_PERCENT &&
		hardware_band[AUTOCROP_TOP] + hardware_band[AUTOCROP_BOTTOM] <= base_height / 2;

	/* Pixel-only bands: trusted on mostly filled pictures only, each band at
	   most a quarter of the picture. */
	int any_pixel_band = 0, trust_pixels = screen_fill >= MIN_SCREEN_FILL_PERCENT;
	for (int side = 0; side < AUTOCROP_SIDES; side++)
	{
		if (raw[side] <= hardware_band[side])
			continue;
		any_pixel_band = 1;
		const int limit = side < AUTOCROP_LEFT ? base_height / 4 : AUTOCROP_UNITS / 4;
		if (raw[side] > limit)
			trust_pixels = 0;
	}

	if (any_pixel_band && trust_pixels)
		trust_pixels = ac_fill(f, x0, x1, y0, y1, xstep, ystep, ignore) >= MIN_CONTENT_FILL_PERCENT;

	/* The edge next to each pixel-only band must be a solid line of graphics,
	   not the outline of a logo floating on black. */
	if (any_pixel_band && trust_pixels)
	{
		const int rows = EDGE_DEPTH * vs, columns = ac_xpixels(f, EDGE_DEPTH);
		const int ex = ac_xpixels(f, 1);
		for (int side = 0; side < AUTOCROP_SIDES && trust_pixels; side++)
		{
			if (raw[side] <= hardware_band[side])
				continue;
			int edge;
			if (side == AUTOCROP_TOP)
				edge = ac_fill(f, x0, x1, y0, ac_min(y0 + rows, y1), ex, vs, ignore);
			else if (side == AUTOCROP_BOTTOM)
				edge = ac_fill(f, x0, x1, ac_max(y1 - rows, y0), y1, ex, vs, ignore);
			else if (side == AUTOCROP_LEFT)
				edge = ac_fill(f, x0, ac_min(x0 + columns, x1), y0, y1, ex, vs, ignore);
			else
				edge = ac_fill(f, ac_max(x1 - columns, x0), x1, y0, y1, ex, vs, ignore);
			trust_pixels = edge >= MIN_EDGE_PERCENT;
		}
	}

	for (int side = 0; side < AUTOCROP_SIDES; side++)
	{
		const int display_off = trust_hardware && hardware_band[side] > 0;
		if (raw[side] > hardware_band[side] && trust_pixels)
			trusted[side] = raw[side];
		else if (display_off)
		{
			/* The display-off part, cropped quickly. */
			trusted[side] = hardware_band[side];
			hardware[side] = 1;
		}
	}

	return 1;
}

/* Compares a coarse sample grid with the previous frame. Returns 1 when
   enough of the picture changed to count as a moving frame. */
static int ac_motion(autocrop_state *state, const autocrop_frame *f)
{
	int changed = 0;
	for (int gy = 0; gy < AUTOCROP_GRID_HEIGHT; gy++)
	{
		const int y = (gy * GRID_STEP + GRID_STEP / 2) * f->vscale;
		const int inside = y < f->height;
		for (int gx = 0; gx < AUTOCROP_GRID_WIDTH; gx++)
		{
			const int x = (gx * GRID_STEP + GRID_STEP / 2) * f->width / AUTOCROP_UNITS;
			const uint16_t sample = inside && x < f->width ? ac_pixel(f, x, y) : 0;
			changed += state->grid[gy][gx] != sample;
			state->grid[gy][gx] = sample;
		}
	}
	const int had_grid = state->has_grid;
	state->has_grid = 1;
	return had_grid && changed >= MOTION_SAMPLES;
}

static inline int ac_same(const int a[AUTOCROP_SIDES], const int b[AUTOCROP_SIDES])
{
	return !memcmp(a, b, sizeof(int) * AUTOCROP_SIDES);
}

static int ac_find(const autocrop_state *state, const int band[AUTOCROP_SIDES])
{
	for (int i = 0; i < state->layout_count; i++)
		if (ac_same(state->layouts[i].band, band))
			return i;
	return -1;
}

static void ac_forget(autocrop_state *state, int index)
{
	if (state->layouts[index].established)
		state->layouts_changed = 1;
	memmove(&state->layouts[index], &state->layouts[index + 1],
	        sizeof(state->layouts[0]) * (state->layout_count - index - 1));
	state->layout_count--;
	if (state->instant_layout == index)
		state->instant_layout = -1;
	else if (state->instant_layout > index)
		state->instant_layout--;
}

/* Moves a layout to the end of the list, the most recently used position. */
static int ac_touch(autocrop_state *state, int index)
{
	const autocrop_layout layout = state->layouts[index];
	const int instant = state->instant_layout == index;
	memmove(&state->layouts[index], &state->layouts[index + 1],
	        sizeof(state->layouts[0]) * (state->layout_count - index - 1));
	state->layouts[state->layout_count - 1] = layout;
	if (instant)
		state->instant_layout = state->layout_count - 1;
	else if (state->instant_layout > index)
		state->instant_layout--;
	return state->layout_count - 1;
}

/* Adds a layout seen for the first time. It is saved once established. */
static void ac_remember(autocrop_state *state, const int band[AUTOCROP_SIDES], int established)
{
	int any = 0;
	for (int side = 0; side < AUTOCROP_SIDES; side++)
		any |= band[side] != 0;
	if (!any)
		return;
	const int found = ac_find(state, band);
	if (found >= 0)
	{
		if (established && !state->layouts[found].established)
		{
			state->layouts[found].established = 1;
			state->layouts_changed = 1;
		}
		return;
	}
	if (state->layout_count == AUTOCROP_MAX_LAYOUTS)
	{
		/* Make room: the least recently used unconfirmed layout, else the oldest. */
		int victim = 0;
		for (int i = 0; i < state->layout_count; i++)
			if (!state->layouts[i].established)
			{
				victim = i;
				break;
			}
		ac_forget(state, victim);
	}
	autocrop_layout *layout = &state->layouts[state->layout_count++];
	memcpy(layout->band, band, sizeof(layout->band));
	layout->strikes = 0;
	layout->established = established;
	if (established)
		state->layouts_changed = 1;
}

static void ac_apply(autocrop_state *state, const int band[AUTOCROP_SIDES])
{
	memcpy(state->applied, band, sizeof(state->applied));
	memcpy(state->pending, band, sizeof(state->pending));
	memset(state->stable, 0, sizeof(state->stable));
	memset(state->motion, 0, sizeof(state->motion));
	state->applied_frames = 0;
}

/* Feeds one measured frame. Returns 1 when the applied crop changed. */
static int ac_update(autocrop_state *state, const int raw[AUTOCROP_SIDES], const int trusted[AUTOCROP_SIDES],
                     const int hardware[AUTOCROP_SIDES], int moving)
{
	int changed = 0;

	if (state->since_expand < MEMORY_COOLDOWN)
		state->since_expand++;
	if (state->instant_layout >= 0 && ++state->instant_frames > STRIKE_FRAMES)
	{
		state->layouts[state->instant_layout].strikes = 0; /* it held: keep trusting it */
		state->instant_layout = -1;
	}

	/* Graphics inside the cropped area: show them right away. */
	for (int side = 0; side < AUTOCROP_SIDES; side++)
		if (raw[side] < state->applied[side])
		{
			state->applied[side] = raw[side];
			state->pending[side] = raw[side];
			state->stable[side] = 0;
			state->motion[side] = 0;
			state->since_expand = 0;
			state->applied_frames = 0;
			changed = 1;
		}
	if (changed && state->instant_layout >= 0)
	{
		/* A remembered layout was wrong right after it was applied. */
		const int index = state->instant_layout;
		state->instant_layout = -1;
		if (++state->layouts[index].strikes >= MAX_STRIKES)
			ac_forget(state, index);
	}

	/* A remembered layout is back: crop it in this same frame. */
	if (state->since_expand >= MEMORY_COOLDOWN && !ac_same(trusted, state->applied))
	{
		int grows = 1;
		for (int side = 0; side < AUTOCROP_SIDES; side++)
			grows &= trusted[side] >= state->applied[side];
		const int found = grows ? ac_find(state, trusted) : -1;
		if (found >= 0 && state->layouts[found].established)
		{
			ac_apply(state, trusted);
			state->instant_layout = ac_touch(state, found);
			state->instant_frames = 0;
			return 1;
		}
	}

	/* New borders: crop them once they stay unchanged. */
	int confirmed = 0;
	for (int side = 0; side < AUTOCROP_SIDES; side++)
	{
		if (trusted[side] <= state->applied[side])
		{
			state->pending[side] = state->applied[side];
			state->stable[side] = 0;
			state->motion[side] = 0;
			continue;
		}
		if (trusted[side] != state->pending[side])
		{
			state->pending[side] = trusted[side];
			state->stable[side] = 0;
			state->motion[side] = 0;
		}
		state->stable[side]++;
		state->motion[side] += moving;
		const int ready = hardware[side] ?
			state->stable[side] >= HARDWARE_FRAMES :
			(state->stable[side] >= PIXEL_FRAMES && state->motion[side] >= PIXEL_MOTION_FRAMES) ||
			state->stable[side] >= PIXEL_STILL_FRAMES;
		if (ready)
		{
			state->applied[side] = trusted[side];
			state->stable[side] = 0;
			state->motion[side] = 0;
			changed = confirmed = 1;
		}
	}
	if (changed)
		state->applied_frames = 0;
	if (confirmed)
		ac_remember(state, state->applied, 0);

	/* A layout that stays on screen long enough is saved for the game. */
	if (!changed && ++state->applied_frames == ESTABLISH_FRAMES)
	{
		const int found = ac_find(state, state->applied);
		if (found >= 0)
		{
			ac_remember(state, state->applied, 1);
			ac_touch(state, found);
		}
	}

	return changed;
}

void autocrop_reset_tracking(autocrop_state *state)
{
	memset(state->applied, 0, sizeof(state->applied));
	memset(state->pending, 0, sizeof(state->pending));
	memset(state->stable, 0, sizeof(state->stable));
	memset(state->motion, 0, sizeof(state->motion));
	state->since_expand = MEMORY_COOLDOWN;
	state->applied_frames = 0;
	state->instant_layout = -1;
	state->instant_frames = 0;
	state->has_grid = 0;
}

void autocrop_reset(autocrop_state *state)
{
	memset(state, 0, sizeof(*state));
	autocrop_reset_tracking(state);
}

int autocrop_process(autocrop_state *state, const autocrop_frame *frame)
{
	int raw[AUTOCROP_SIDES], trusted[AUTOCROP_SIDES], hardware[AUTOCROP_SIDES];

	if (!frame->pixels || frame->width < 16 || frame->height < 16 || frame->vscale < 1)
		return 0;
	if (!ac_measure(frame, raw, trusted, hardware))
		return 0;
	return ac_update(state, raw, trusted, hardware, ac_motion(state, frame));
}

/* Layout memory file: a header line, then "top bottom left right" for each
   established layout, least recently used first. */
void autocrop_load_layouts(autocrop_state *state, const char *path)
{
	state->layout_count = 0;
	state->instant_layout = -1;
	state->layouts_changed = 0;

	FILE *file = fopen(path, "r");
	if (!file)
		return;
	char header[64];
	if (fgets(header, sizeof(header), file) && !strncmp(header, FILE_HEADER, sizeof(FILE_HEADER) - 1))
	{
		int band[AUTOCROP_SIDES];
		while (state->layout_count < AUTOCROP_MAX_LAYOUTS &&
		       fscanf(file, "%d %d %d %d", &band[0], &band[1], &band[2], &band[3]) == AUTOCROP_SIDES)
		{
			int valid = 1;
			for (int side = 0; side < AUTOCROP_SIDES; side++)
				valid &= band[side] >= 0 && band[side] <= (side < AUTOCROP_LEFT ? MAX_BAND_UNITS : AUTOCROP_UNITS / 2);
			if (valid)
				ac_remember(state, band, 1);
		}
	}
	fclose(file);
	state->layouts_changed = 0;
}

int autocrop_save_layouts(autocrop_state *state, const char *path)
{
	if (!state->layouts_changed)
		return 0;
	state->layouts_changed = 0;

	int any = 0;
	for (int i = 0; i < state->layout_count; i++)
		any |= state->layouts[i].established;
	if (!any)
	{
		remove(path);
		return 1;
	}
	FILE *file = fopen(path, "w");
	if (!file)
		return 0;
	fprintf(file, "%s\n", FILE_HEADER);
	for (int i = 0; i < state->layout_count; i++)
		if (state->layouts[i].established)
			fprintf(file, "%d %d %d %d\n", state->layouts[i].band[0], state->layouts[i].band[1],
			        state->layouts[i].band[2], state->layouts[i].band[3]);
	fclose(file);
	return 1;
}

void autocrop_view(const autocrop_state *state, int mode, int width, int lines, float xroom, float yroom,
                   float *u0, float *u1, float *v0, float *v1, float *xsize, float *ysize)
{
	*u0 = *v0 = 0.0f;
	*u1 = *v1 = 1.0f;
	*xsize = *ysize = 1.0f;
	if (mode == AUTOCROP_OFF || width <= 0 || lines <= 0)
		return;

	/* Round horizontal edges inward to whole pixels. */
	const int left = (state->applied[AUTOCROP_LEFT] * width + AUTOCROP_UNITS - 1) / AUTOCROP_UNITS;
	const int right = (state->applied[AUTOCROP_RIGHT] * width + AUTOCROP_UNITS - 1) / AUTOCROP_UNITS;
	const int top = state->applied[AUTOCROP_TOP], bottom = state->applied[AUTOCROP_BOTTOM];
	if (left + right >= width || top + bottom >= lines)
		return;

	*u0 = (float)left / width;
	*u1 = (float)(width - right) / width;
	*v0 = (float)top / lines;
	*v1 = (float)(lines - bottom) / lines;

	if (mode == AUTOCROP_FIT)
	{
		/* Box of the screen's shape that the whole frame just fits in. */
		if (xroom <= 0.0f || yroom <= 0.0f)
			xroom = yroom = 1.0f;
		const float fit = xroom < yroom ? xroom : yroom;
		const float xbox = xroom / fit, ybox = yroom / fit;

		/* Enlarge the cropped picture evenly until it meets the box. */
		const float w = *u1 - *u0, h = *v1 - *v0;
		const float k = (xbox / w < ybox / h) ? xbox / w : ybox / h;
		*xsize = w * k;
		*ysize = h * k;
	}
}
