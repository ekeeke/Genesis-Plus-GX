/*
 * bezel.c - PNG bezels around the game picture, for GX (Wii/GameCube)
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
 * See bezel.h. The PNG is read one row at a time and scaled on the way
 * (each screen pixel is the average of the part of the image it covers), so
 * a big PNG never needs memory for the whole image.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <setjmp.h>
#include <malloc.h>
#include <dirent.h>
#include <png.h>

#ifdef GEKKO
#include <ogc/cache.h>
#endif

#include "bezel.h"

#define MAX_TEXTURE 1024     /* GX textures are at most 1024 x 1024 */
#define MAX_PNG_SIDE 16384   /* bigger PNGs are not bezels */
#define MAX_INTERLACED (16 * 1024 * 1024) /* an interlaced PNG is read whole */
#define HOLE_ALPHA 128       /* below this: part of the window */

/* scaling one axis by areas: source pixel i covers [i * dst, (i + 1) * dst)
   and destination pixel o covers [o * src, (o + 1) * src) */
static int overlap(int i, int o, int src, int dst)
{
	int a = i * dst, b = (i + 1) * dst;
	int c = o * src, d = (o + 1) * src;
	int lo = a > c ? a : c, hi = b < d ? b : d;
	return hi > lo ? hi - lo : 0;
}

typedef struct
{
	int sw, sh, tw, th;
	float *hrow;        /* the source row scaled across: premultiplied RGB, A */
	float *acc;         /* rows being summed (ring of nacc rows) */
	int nacc;
	int next_open;      /* first destination row not summed into yet */
	int next_out;       /* first destination row not finished */
	int src_row;        /* source rows done */
	unsigned char *tex; /* the texture being made */
	int tex_w;
	unsigned char *alpha; /* alpha of each screen pixel, for the window */
} scaler;

static void put_pixel(scaler *s, int x, int y, int r, int g, int b, int a)
{
	const int tile = (y >> 2) * (s->tex_w >> 2) + (x >> 2);
	const int p = ((y & 3) << 2) + (x & 3);
	unsigned char *t = s->tex + tile * 64 + p * 2;
	t[0] = a;
	t[1] = r;
	t[32] = g;
	t[33] = b;
	s->alpha[y * s->tw + x] = a;
}

static int clamp255(float v)
{
	int i = (int) (v + 0.5f);
	return i < 0 ? 0 : (i > 255 ? 255 : i);
}

static void finish_row(scaler *s, int o)
{
	const float *acc = s->acc + (o % s->nacc) * s->tw * 4;
	int x;
	for (x = 0; x < s->tw; x++, acc += 4)
	{
		const float a = acc[3];
		if (a < 0.5f)
		{
			put_pixel(s, x, o, 0, 0, 0, 0);
			continue;
		}
		/* back from premultiplied: the colour of what is visible */
		put_pixel(s, x, o, clamp255(acc[0] * 255.0f / a), clamp255(acc[1] * 255.0f / a),
			clamp255(acc[2] * 255.0f / a), clamp255(a));
	}
}

/* one source row, RGBA8 */
static void add_row(scaler *s, const unsigned char *row)
{
	const int i = s->src_row++;
	const float hnorm = 1.0f / s->sw;
	float *h = s->hrow;
	int x, o;

	/* across */
	memset(h, 0, s->tw * 4 * sizeof(float));
	for (x = 0; x < s->sw; x++, row += 4)
	{
		const float a = row[3];
		const float pr = row[0] * a / 255.0f, pg = row[1] * a / 255.0f, pb = row[2] * a / 255.0f;
		const int first = x * s->tw / s->sw;
		int last = ((x + 1) * s->tw - 1) / s->sw;
		if (last >= s->tw)
			last = s->tw - 1;
		for (o = first; o <= last; o++)
		{
			const float w = overlap(x, o, s->sw, s->tw) * hnorm;
			float *d = h + o * 4;
			d[0] += pr * w;
			d[1] += pg * w;
			d[2] += pb * w;
			d[3] += a * w;
		}
	}

	/* down */
	{
		const int first = i * s->th / s->sh;
		int last = ((i + 1) * s->th - 1) / s->sh;
		const float vnorm = 1.0f / s->sh;
		if (last >= s->th)
			last = s->th - 1;
		for (o = first; o <= last; o++)
		{
			float *d = s->acc + (o % s->nacc) * s->tw * 4;
			const float w = overlap(i, o, s->sh, s->th) * vnorm;
			if (o >= s->next_open)
			{
				memset(d, 0, s->tw * 4 * sizeof(float));
				s->next_open = o + 1;
			}
			for (x = 0; x < s->tw * 4; x++)
				d[x] += h[x] * w;
		}
	}

	/* rows the source has fully covered now */
	{
		int done = (i + 1) * s->th / s->sh;
		if (done > s->th)
			done = s->th;
		while (s->next_out < done)
			finish_row(s, s->next_out++);
	}
}

/* The transparent window: the pixels below HOLE_ALPHA reached from the
   centre. Uses (and spoils) the alpha map. */
static void find_hole(bezel_image *b, unsigned char *alpha)
{
	const int w = b->width, h = b->height;
	int *stack, size = 1024, n = 0;
	int x0 = w, y0 = h, x1 = -1, y1 = -1;

	b->hole_x = b->hole_y = b->hole_w = b->hole_h = 0;
	if (alpha[(h / 2) * w + w / 2] >= HOLE_ALPHA)
		return; /* opaque in the middle: a background */

	stack = (int *) malloc(size * 2 * sizeof(int));
	if (!stack)
		return;
	stack[n * 2] = w / 2;
	stack[n * 2 + 1] = h / 2;
	n++;

	while (n > 0)
	{
		int x, y, l, r, ny, k;
		n--;
		x = stack[n * 2];
		y = stack[n * 2 + 1];
		if (alpha[y * w + x] >= HOLE_ALPHA)
			continue; /* done already */

		/* the run of this row, marked as done */
		for (l = x; l > 0 && alpha[y * w + l - 1] < HOLE_ALPHA; l--);
		for (r = x; r < w - 1 && alpha[y * w + r + 1] < HOLE_ALPHA; r++);
		memset(alpha + y * w + l, 255, r - l + 1);
		if (l < x0) x0 = l;
		if (r > x1) x1 = r;
		if (y < y0) y0 = y;
		if (y > y1) y1 = y;

		/* one seed for each run touching it above and below */
		for (k = 0; k < 2; k++)
		{
			ny = k ? y + 1 : y - 1;
			if (ny < 0 || ny >= h)
				continue;
			for (x = l; x <= r; x++)
			{
				if (alpha[ny * w + x] >= HOLE_ALPHA)
					continue;
				if (n == size)
				{
					int *more = (int *) realloc(stack, size * 4 * sizeof(int));
					if (!more)
					{
						free(stack);
						return;
					}
					stack = more;
					size *= 2;
				}
				stack[n * 2] = x;
				stack[n * 2 + 1] = ny;
				n++;
				while (x < r && alpha[ny * w + x + 1] < HOLE_ALPHA)
					x++;
			}
		}
	}
	free(stack);

	/* transparent up to all four edges: not a window */
	if (x0 == 0 && y0 == 0 && x1 == w - 1 && y1 == h - 1)
		return;
	/* too small to show a game */
	if ((x1 - x0 + 1) * 4 < w || (y1 - y0 + 1) * 4 < h)
		return;

	b->hole_x = x0;
	b->hole_y = y0;
	b->hole_w = x1 - x0 + 1;
	b->hole_h = y1 - y0 + 1;
}

void bezel_free(bezel_image *b)
{
	if (b->texture)
		free(b->texture);
	memset(b, 0, sizeof(*b));
}

typedef struct
{
	jmp_buf jmp;
} png_error_state;

static void png_error_fn(png_structp png, png_const_charp msg)
{
	png_error_state *e = (png_error_state *) png_get_error_ptr(png);
	(void) msg;
	longjmp(e->jmp, 1);
}

static void png_warning_fn(png_structp png, png_const_charp msg)
{
	(void) png;
	(void) msg;
}

/* a PNG in memory */
typedef struct
{
	const unsigned char *data;
	size_t size, pos;
} png_memory;

static void png_read_memory(png_structp png, png_bytep out, png_size_t len)
{
	png_memory *m = (png_memory *) png_get_io_ptr(png);
	if (len > m->size - m->pos)
		png_error(png, "end of data");
	memcpy(out, m->data + m->pos, len);
	m->pos += len;
}

/* from the file path, or from mem when path is NULL */
static int load_png(bezel_image *b, const char *path, png_memory *mem, int width, int height)
{
	/* everything the error path frees is volatile (kept across longjmp) */
	FILE *volatile fp = NULL;
	char *volatile filebuf = NULL;
	unsigned char *volatile row = NULL;
	unsigned char *volatile image = NULL;
	float *volatile hrow = NULL;
	float *volatile acc = NULL;
	unsigned char *volatile alpha = NULL;
	void *volatile tex = NULL;
	png_structp png = NULL;
	png_infop info = NULL;
	png_error_state err;
	volatile int ok = 0;

	bezel_free(b);
	if (width <= 0 || height <= 0 || width > MAX_TEXTURE || height > MAX_TEXTURE)
		return 0;

	if (path)
	{
		fp = fopen(path, "rb");
		if (!fp)
			return 0;
		filebuf = (char *) malloc(64 * 1024);
		if (filebuf)
			setvbuf(fp, filebuf, _IOFBF, 64 * 1024);
	}

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &err, png_error_fn, png_warning_fn);
	if (png)
		info = png_create_info_struct(png);
	if (!png || !info)
		goto done;

	if (setjmp(err.jmp) == 0)
	{
		png_uint_32 sw, sh;
		int depth, color, interlace, passes, y;
		scaler s;

		if (fp)
			png_init_io(png, fp);
		else
			png_set_read_fn(png, mem, png_read_memory);
		png_set_user_limits(png, MAX_PNG_SIDE, MAX_PNG_SIDE);
		png_read_info(png, info);
		png_get_IHDR(png, info, &sw, &sh, &depth, &color, &interlace, NULL, NULL);

		/* always RGBA, 8 bits */
		png_set_expand(png);
		png_set_strip_16(png);
		if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(png);
		png_set_filler(png, 0xff, PNG_FILLER_AFTER);
		passes = png_set_interlace_handling(png);
		png_read_update_info(png, info);
		if (png_get_rowbytes(png, info) != sw * 4)
			longjmp(err.jmp, 1);

		memset(&s, 0, sizeof(s));
		s.sw = (int) sw;
		s.sh = (int) sh;
		s.tw = width;
		s.th = height;
		s.tex_w = (width + 3) & ~3;
		s.nacc = height / s.sh + 2;

		b->width = width;
		b->height = height;
		b->tex_width = s.tex_w;
		b->tex_height = (height + 3) & ~3;

		tex = memalign(32, b->tex_width * b->tex_height * 4);
		alpha = (unsigned char *) malloc(width * height);
		hrow = (float *) malloc(width * 4 * sizeof(float));
		acc = (float *) malloc(s.nacc * width * 4 * sizeof(float));
		if (!tex || !alpha || !hrow || !acc)
			longjmp(err.jmp, 1);
		memset(tex, 0, b->tex_width * b->tex_height * 4); /* padding: transparent */
		s.tex = (unsigned char *) tex;
		s.alpha = alpha;
		s.hrow = hrow;
		s.acc = acc;

		if (passes > 1)
		{
			/* interlaced: all of it first */
			png_bytep *rows;
			if ((size_t) sw * sh * 4 > MAX_INTERLACED)
				longjmp(err.jmp, 1);
			image = (unsigned char *) malloc((size_t) sw * sh * 4 + sh * sizeof(png_bytep));
			if (!image)
				longjmp(err.jmp, 1);
			rows = (png_bytep *) (image + (size_t) sw * sh * 4);
			for (y = 0; y < (int) sh; y++)
				rows[y] = image + (size_t) y * sw * 4;
			png_read_image(png, rows);
			for (y = 0; y < (int) sh; y++)
				add_row(&s, rows[y]);
		}
		else
		{
			row = (unsigned char *) malloc(sw * 4);
			if (!row)
				longjmp(err.jmp, 1);
			for (y = 0; y < (int) sh; y++)
			{
				png_read_row(png, row, NULL);
				add_row(&s, row);
			}
		}

		b->texture = tex;
		tex = NULL;
		find_hole(b, alpha);
#ifdef GEKKO
		DCFlushRange(b->texture, b->tex_width * b->tex_height * 4);
#endif
		ok = 1;
	}

done:
	if (png)
		png_destroy_read_struct(&png, info ? &info : NULL, NULL);
	if (fp)
		fclose(fp);
	free(filebuf);
	free(row);
	free(image);
	free(hrow);
	free(acc);
	free(alpha);
	if (tex)
		free(tex);
	if (!ok)
		bezel_free(b);
	return ok;
}

int bezel_load(bezel_image *b, const char *path, int width, int height)
{
	return load_png(b, path, NULL, width, height);
}

int bezel_load_memory(bezel_image *b, const void *data, unsigned int size, int width, int height)
{
	png_memory mem;
	mem.data = (const unsigned char *) data;
	mem.size = size;
	mem.pos = 0;
	return load_png(b, NULL, &mem, width, height);
}

static int name_cmp(const void *a, const void *b)
{
	return strcasecmp((const char *) a, (const char *) b);
}

int bezel_list(const char *dir, char (*names)[BEZEL_NAME_MAX], int count, int max)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	const int before = count;

	if (!d)
		return count;

	while (count < max && (e = readdir(d)) != NULL)
	{
		const size_t len = strlen(e->d_name);
		int i, found = 0;
		if (e->d_name[0] == '.' || len < 5 || len >= BEZEL_NAME_MAX)
			continue;
		if (strcasecmp(e->d_name + len - 4, ".png") != 0)
			continue;
		for (i = 0; i < before && !found; i++)
			found = (strcasecmp(names[i], e->d_name) == 0);
		if (!found)
			strcpy(names[count++], e->d_name);
	}
	closedir(d);

	qsort(names, count, BEZEL_NAME_MAX, name_cmp);
	return count;
}

void bezel_display_name(const char *name, char *out, int len)
{
	int n = (int) strlen(name);
	if (n > 4 && strcasecmp(name + n - 4, ".png") == 0)
		n -= 4;
	if (n <= len)
	{
		memcpy(out, name, n);
		out[n] = 0;
		return;
	}
	memcpy(out, name, len - 3);
	strcpy(out + len - 3, "...");
}
