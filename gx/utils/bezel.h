/*
 * bezel.h - PNG bezels around the game picture, for GX (Wii/GameCube)
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
 * A bezel is a PNG image that covers the whole screen. It is scaled to the
 * size of the screen (any PNG size works, 1920x1080 bezels made for
 * RetroArch included) and turned into a GX RGBA8 texture.
 *
 * The transparent window in the middle of it (alpha below 50%, found from
 * the centre of the image outwards) is where the game picture goes: the
 * game is drawn to fill that window, and the bezel on top of it, so
 * rounded corners of the window cover the corners of the picture. A PNG
 * without such a window (opaque in the middle, or transparent up to all
 * four edges) is a background: the game is drawn on top of it, at its
 * usual size.
 */

#ifndef BEZEL_H
#define BEZEL_H

#ifdef __cplusplus
extern "C" {
#endif

#define BEZEL_NAME_MAX 128 /* file names (with ".png") shorter than this */

/* Bezel setting: off, chosen by the game's name, or a file name. */
#define BEZEL_OFF  ""
#define BEZEL_AUTO "<auto>" /* "<" is not allowed in FAT file names */

typedef struct
{
	void *texture;            /* GX_TF_RGBA8 (4x4 tiles), 32-byte aligned; NULL: none */
	int width, height;        /* screen size it was made for, in pixels */
	int tex_width, tex_height; /* texture size, multiples of 4 */
	int hole_x, hole_y;       /* transparent window, from the top left */
	int hole_w, hole_h;       /* (in screen pixels); hole_w == 0: none */
} bezel_image;

/* Loads the PNG at path scaled to width x height (at most 1024 x 1024).
   Returns 1 on success; on failure (no file, not a PNG, out of memory) b is
   left empty. b must be empty or loaded before (it is freed first). */
int bezel_load(bezel_image *b, const char *path, int width, int height);

void bezel_free(bezel_image *b);

/* Adds the PNG files in dir to names (count of them already there, room for
   max) and returns the new count. The list stays sorted (ignoring case) and
   without repeated names. Names too long and hidden files are left out. */
int bezel_list(const char *dir, char (*names)[BEZEL_NAME_MAX], int count, int max);

/* Shortens name for a menu: no ".png", at most len characters ("..." at the
   end when cut). out must have room for len + 1 characters. */
void bezel_display_name(const char *name, char *out, int len);

#ifdef __cplusplus
}
#endif

#endif
