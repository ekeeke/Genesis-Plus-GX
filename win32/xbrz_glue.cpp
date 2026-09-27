/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  xbrz_glue.cpp -- RGB565 <-> xBRZ pixel conversion. See xbrz_glue.h.
 *
 *  Pixel layout: the xBRZ sources in xbrz/ read red from the LOW byte of each
 *  32-bit pixel (0x00BBGGRR). That matters: its colour-distance table weights
 *  red, green and blue differently, so handing it the more usual 0x00RRGGBB
 *  would swap red and blue in that weighting and quietly change which edges
 *  it decides to smooth. Pixels are therefore packed the way it reads them.
 *
 *  Conversions:
 *   565 -> 888  replicates the top bits into the bottom (0x1F -> 0xFF), so
 *               full-scale stays full-scale.
 *   888 -> 565  rounds to nearest instead of truncating. The two are exact
 *               inverses for unblended colours, so flat areas and untouched
 *               pixels come back bit-for-bit as they went in; only the
 *               colours xBRZ actually blends are quantised.
 ****************************************************************************/

#include "xbrz_glue.h"
#include "xbrz/xbrz.h"

#include <cstdlib>

namespace
{
uint32_t *g_src;          /* w*h input pixels */
uint32_t *g_dst;          /* (w*f)*(h*f) output pixels */
size_t    g_src_cap;      /* in pixels */
size_t    g_dst_cap;

uint8_t g_expand5[32], g_expand6[64];    /* 565 field -> 8 bit */
uint8_t g_quant5[256], g_quant6[256];    /* 8 bit -> 565 field, rounded */
bool    g_lut_ready;

void build_luts()
{
  if (g_lut_ready) return;

  for (int i = 0; i < 32; i++) g_expand5[i] = (uint8_t)((i << 3) | (i >> 2));
  for (int i = 0; i < 64; i++) g_expand6[i] = (uint8_t)((i << 2) | (i >> 4));
  for (int v = 0; v < 256; v++)
  {
    g_quant5[v] = (uint8_t)((v * 31 + 127) / 255);
    g_quant6[v] = (uint8_t)((v * 63 + 127) / 255);
  }
  g_lut_ready = true;
}

bool reserve(uint32_t **buf, size_t *cap, size_t pixels)
{
  if (pixels <= *cap) return true;

  std::free(*buf);
  *buf = static_cast<uint32_t *>(std::malloc(pixels * sizeof(uint32_t)));
  *cap = *buf ? pixels : 0;
  return *buf != nullptr;
}
}

extern "C" int xbrz_glue_prepare(void)
{
  build_luts();
  return xbrz::prepareTables() ? 1 : 0;
}

extern "C" void xbrz_glue_release(void)
{
  std::free(g_src); g_src = nullptr; g_src_cap = 0;
  std::free(g_dst); g_dst = nullptr; g_dst_cap = 0;
  xbrz::releaseTables();
}

extern "C" int xbrz_glue_scale(int factor, const uint16_t *src, int src_pitch, int w, int h,
                               uint16_t *dst, int dst_pitch)
{
  if (factor < 2 || factor > 6 || w < 1 || h < 1) return 0;

  build_luts();

  const size_t in_px  = (size_t)w * (size_t)h;
  const size_t out_px = in_px * (size_t)factor * (size_t)factor;

  if (!xbrz::prepareTables()) return 0;
  if (!reserve(&g_src, &g_src_cap, in_px) || !reserve(&g_dst, &g_dst_cap, out_px)) return 0;

  for (int y = 0; y < h; y++)
  {
    const uint16_t *row = (const uint16_t *)((const uint8_t *)src + (size_t)y * (size_t)src_pitch);
    uint32_t       *out = g_src + (size_t)y * (size_t)w;

    for (int x = 0; x < w; x++)
    {
      const unsigned c = row[x];
      out[x] = (uint32_t)g_expand5[c >> 11]
             | ((uint32_t)g_expand6[(c >> 5) & 63] << 8)
             | ((uint32_t)g_expand5[c & 31] << 16);
    }
  }

  xbrz::scale((size_t)factor, g_src, g_dst, w, h, xbrz::ColorFormat::RGB);

  const int ow = w * factor, oh = h * factor;

  for (int y = 0; y < oh; y++)
  {
    const uint32_t *in  = g_dst + (size_t)y * (size_t)ow;
    uint16_t       *row = (uint16_t *)((uint8_t *)dst + (size_t)y * (size_t)dst_pitch);

    for (int x = 0; x < ow; x++)
    {
      const uint32_t p = in[x];
      row[x] = (uint16_t)((g_quant5[p & 0xFF] << 11)
                        | (g_quant6[(p >> 8) & 0xFF] << 5)
                        |  g_quant5[(p >> 16) & 0xFF]);
    }
  }

  return 1;
}
