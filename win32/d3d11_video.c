/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  d3d11_video.c -- see d3d11_video.h for the rationale.
 *
 *  Child window, not the main frame window: unlike D3D9's Present(), which
 *  takes an explicit source and destination rect, DXGI's IDXGISwapChain::
 *  Present() always fills the *entire* window it is bound to -- there is no
 *  per-present rect. This app's status bar is a sibling child window living
 *  below the content area inside the same frame window, so binding the swap
 *  chain to the frame window directly would mean every Present() also
 *  painted over the status bar. Instead, the swap chain is bound to a small
 *  borderless child window that is kept exactly the size and position of
 *  the content area (the same "area" rect video.c already computes), which
 *  is resized only when that content area's size actually changes.
 *
 *  Shaders are compiled once at startup, at runtime, via D3DCompile --
 *  loaded dynamically from d3dcompiler_47.dll (present on any real Windows
 *  10/11 install, and on 7/8 with the DirectX 11 runtime) rather than
 *  linked, so a system without it fails d3d11_init() gracefully instead of
 *  the whole exe refusing to start. There is no build-time shader compiler
 *  in this project's MinGW toolchain, hence compiling at runtime rather
 *  than embedding precompiled bytecode.
 ****************************************************************************/

#include <string.h>
/* Without this, MinGW's d3d11.h/dxgi.h/d3dcompiler.h headers only declare the
   COM vtable structs, not the ID3D11Device_Release(p)-style convenience
   macros this file uses throughout -- unlike d3d9.h, which always defines
   its own equivalents unconditionally in a plain C translation unit. */
#define COBJMACROS
#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3d11_video.h"

typedef struct { float x, y, u, v; } d3d11_vertex_t;

/* A minimal, self-contained pass: one texture, one sampler, straight
   passthrough position (already in clip space -- computed on the CPU side
   from the destination rect, exactly like d3d9_video.c's screen-space quad,
   just expressed as NDC instead of D3D9's XYZRHW). */
static const char *vs_src =
  "struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; };\n"
  "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };\n"
  "VSOut main(VSIn i)\n"
  "{\n"
  "  VSOut o;\n"
  "  o.pos = float4(i.pos, 0.0, 1.0);\n"
  "  o.uv = i.uv;\n"
  "  return o;\n"
  "}\n";

static const char *ps_src =
  "Texture2D    tex0 : register(t0);\n"
  "SamplerState samp0 : register(s0);\n"
  "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };\n"
  "float4 main(VSOut i) : SV_TARGET\n"
  "{\n"
  "  return tex0.Sample(samp0, i.uv);\n"
  "}\n";

/* d3dcompiler.h already typedefs the function pointer type as "pD3DCompile"
   (unfortunately the same spelling a variable holding one would naturally
   get) -- reusing its own typedef name here instead of declaring another,
   and naming the variable itself something else, avoids the clash. */
static HMODULE   d3dcompiler_dll;
static pD3DCompile d3dcompile_fn;

static HWND                        parent_hwnd;
static HWND                        child_hwnd;
static ATOM                        child_class;

static ID3D11Device               *device;
static ID3D11DeviceContext        *context;
static IDXGISwapChain              *swapchain;
static ID3D11RenderTargetView     *rtv;
static int                         swap_w, swap_h;   /* current swap chain / child window size */

static ID3D11VertexShader         *vshader;
static ID3D11PixelShader          *pshader;
static ID3D11InputLayout          *input_layout;
static ID3D11Buffer                *vbuffer;

static ID3D11SamplerState         *samp_point;
static ID3D11SamplerState         *samp_linear;
static ID3D11BlendState           *blend_opaque;
static ID3D11BlendState           *blend_alpha;

static ID3D11Texture2D            *tex;
static ID3D11ShaderResourceView   *tex_srv;
static int                         tex_w, tex_h;
static DXGI_FORMAT                 tex_fmt = DXGI_FORMAT_B5G6R5_UNORM;
static int                         tex_needs_565_to_8888;   /* set once, at init, if the GPU can't take 565 directly */

static ID3D11Texture2D            *overlay_tex;
static ID3D11ShaderResourceView   *overlay_srv;
static int                         overlay_tex_w, overlay_tex_h;

static int                         device_ok;

#define CHILD_CLASS_NAME "GPGXD3D11Surface"

static LRESULT CALLBACK child_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  if (msg == WM_ERASEBKGND) return 1;   /* the swap chain covers the whole thing every frame */
  return DefWindowProcA(hwnd, msg, wp, lp);
}

static void release_swapchain_views(void)
{
  if (rtv) { ID3D11RenderTargetView_Release(rtv); rtv = NULL; }
}

static void release_frame_resources(void)
{
  if (tex_srv)     { ID3D11ShaderResourceView_Release(tex_srv);     tex_srv = NULL; }
  if (tex)         { ID3D11Texture2D_Release(tex);                  tex = NULL; tex_w = 0; tex_h = 0; }
  if (overlay_srv) { ID3D11ShaderResourceView_Release(overlay_srv); overlay_srv = NULL; }
  if (overlay_tex) { ID3D11Texture2D_Release(overlay_tex);          overlay_tex = NULL; overlay_tex_w = 0; overlay_tex_h = 0; }
}

static void release_pipeline(void)
{
  if (vbuffer)      { ID3D11Buffer_Release(vbuffer);              vbuffer = NULL; }
  if (input_layout) { ID3D11InputLayout_Release(input_layout);    input_layout = NULL; }
  if (pshader)      { ID3D11PixelShader_Release(pshader);         pshader = NULL; }
  if (vshader)      { ID3D11VertexShader_Release(vshader);        vshader = NULL; }
  if (samp_point)   { ID3D11SamplerState_Release(samp_point);     samp_point = NULL; }
  if (samp_linear)  { ID3D11SamplerState_Release(samp_linear);    samp_linear = NULL; }
  if (blend_opaque) { ID3D11BlendState_Release(blend_opaque);     blend_opaque = NULL; }
  if (blend_alpha)  { ID3D11BlendState_Release(blend_alpha);      blend_alpha = NULL; }
}

static void release_device(void)
{
  release_frame_resources();
  release_pipeline();
  release_swapchain_views();
  if (swapchain) { IDXGISwapChain_Release(swapchain); swapchain = NULL; }
  if (context)   { ID3D11DeviceContext_Release(context); context = NULL; }
  if (device)    { ID3D11Device_Release(device); device = NULL; }
  swap_w = 0; swap_h = 0;
}

/* Loaded once, kept for the life of the process -- there is nothing to
   release it for that matters (the OS reclaims it at exit regardless), and
   every other DLL this app depends on is handled the same way. */
static int ensure_d3dcompiler(void)
{
  if (d3dcompile_fn) return 1;

  d3dcompiler_dll = LoadLibraryA("d3dcompiler_47.dll");
  if (!d3dcompiler_dll) return 0;

  d3dcompile_fn = (pD3DCompile)(void *)GetProcAddress(d3dcompiler_dll, "D3DCompile");
  if (!d3dcompile_fn) { FreeLibrary(d3dcompiler_dll); d3dcompiler_dll = NULL; return 0; }

  return 1;
}

static int compile_shader(const char *src, const char *entry, const char *target, ID3DBlob **out)
{
  ID3DBlob *errors = NULL;
  HRESULT hr = d3dcompile_fn(src, strlen(src), NULL, NULL, NULL, entry, target,
                            0, 0, out, &errors);
  if (errors) ID3D10Blob_Release(errors);   /* ID3DBlob and ID3D10Blob are the same interface */
  return SUCCEEDED(hr);
}

static int create_pipeline(void)
{
  ID3DBlob *vs_blob = NULL, *ps_blob = NULL;
  D3D11_INPUT_ELEMENT_DESC layout[2];
  D3D11_SAMPLER_DESC sdesc;
  D3D11_BLEND_DESC bdesc;
  D3D11_BUFFER_DESC vbdesc;

  if (!ensure_d3dcompiler()) return 0;
  if (!compile_shader(vs_src, "main", "vs_4_0", &vs_blob)) return 0;
  if (!compile_shader(ps_src, "main", "ps_4_0", &ps_blob)) { ID3D10Blob_Release(vs_blob); return 0; }

  if (FAILED(ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vs_blob),
             ID3D10Blob_GetBufferSize(vs_blob), NULL, &vshader)))
  {
    ID3D10Blob_Release(vs_blob); ID3D10Blob_Release(ps_blob);
    return 0;
  }

  if (FAILED(ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(ps_blob),
             ID3D10Blob_GetBufferSize(ps_blob), NULL, &pshader)))
  {
    ID3D10Blob_Release(vs_blob); ID3D10Blob_Release(ps_blob);
    return 0;
  }

  ZeroMemory(layout, sizeof(layout));
  layout[0].SemanticName = "POSITION"; layout[0].Format = DXGI_FORMAT_R32G32_FLOAT;
  layout[0].AlignedByteOffset = 0;
  layout[1].SemanticName = "TEXCOORD"; layout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  layout[1].AlignedByteOffset = 8;

  {
    HRESULT hr = ID3D11Device_CreateInputLayout(device, layout, 2,
                   ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), &input_layout);
    ID3D10Blob_Release(vs_blob);
    ID3D10Blob_Release(ps_blob);
    if (FAILED(hr)) return 0;
  }

  ZeroMemory(&sdesc, sizeof(sdesc));
  sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  sdesc.AddressU = sdesc.AddressV = sdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sdesc.MaxLOD = D3D11_FLOAT32_MAX;
  if (FAILED(ID3D11Device_CreateSamplerState(device, &sdesc, &samp_point))) return 0;

  sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  if (FAILED(ID3D11Device_CreateSamplerState(device, &sdesc, &samp_linear))) return 0;

  ZeroMemory(&bdesc, sizeof(bdesc));
  bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  bdesc.RenderTarget[0].BlendEnable = FALSE;
  if (FAILED(ID3D11Device_CreateBlendState(device, &bdesc, &blend_opaque))) return 0;

  bdesc.RenderTarget[0].BlendEnable = TRUE;
  bdesc.RenderTarget[0].SrcBlend  = D3D11_BLEND_SRC_ALPHA;
  bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  bdesc.RenderTarget[0].BlendOp   = D3D11_BLEND_OP_ADD;
  bdesc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
  bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  bdesc.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
  if (FAILED(ID3D11Device_CreateBlendState(device, &bdesc, &blend_alpha))) return 0;

  ZeroMemory(&vbdesc, sizeof(vbdesc));
  vbdesc.ByteWidth = sizeof(d3d11_vertex_t) * 4;
  vbdesc.Usage = D3D11_USAGE_DYNAMIC;
  vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(ID3D11Device_CreateBuffer(device, &vbdesc, NULL, &vbuffer))) return 0;

  return 1;
}

/* Creates the render target view for whatever the swap chain's current back
   buffer is. Called once after device/swap chain creation and again after
   every successful ResizeBuffers(). */
static int create_rtv(void)
{
  ID3D11Texture2D *backbuf = NULL;
  HRESULT hr;

  release_swapchain_views();

  hr = IDXGISwapChain_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, (void **)&backbuf);
  if (FAILED(hr) || !backbuf) return 0;

  hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)backbuf, NULL, &rtv);
  ID3D11Texture2D_Release(backbuf);
  return SUCCEEDED(hr);
}

static int create_device_and_swapchain(int w, int h)
{
  static const D3D_FEATURE_LEVEL levels[] =
  {
    D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3
  };
  D3D_FEATURE_LEVEL got_level;
  DXGI_SWAP_CHAIN_DESC sd;
  HRESULT hr;

  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 1;
  sd.BufferDesc.Width  = (w < 1) ? 1 : w;
  sd.BufferDesc.Height = (h < 1) ? 1 : h;
  sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 0;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = child_hwnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;   /* the classic, universally-supported swap effect --
                                                  works on every D3D11 feature level and Windows
                                                  version this app targets; the newer flip-model
                                                  effects need Windows 8+ and bring nothing this
                                                  simple single-quad workload benefits from. */

  hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
         levels, sizeof(levels) / sizeof(levels[0]), D3D11_SDK_VERSION,
         &sd, &swapchain, &device, &got_level, &context);

  if (FAILED(hr)) return 0;

  swap_w = w; swap_h = h;
  return create_rtv();
}

/* Resizes the swap chain (and the child window it's bound to) in place,
   the same reasoning as d3d9_video.c's reset_device(): rebuilding the whole
   device on every resize step is what made an earlier version of this
   stutter badly while a window was being dragged. Every view referencing
   the old back buffer has to go before ResizeBuffers(), which is why the
   RTV is released first and recreated after. */
static int resize_swapchain(int w, int h)
{
  HRESULT hr;

  if (w < 1) w = 1;
  if (h < 1) h = 1;

  MoveWindow(child_hwnd, 0, 0, w, h, FALSE);

  release_swapchain_views();
  ID3D11DeviceContext_OMSetRenderTargets(context, 0, NULL, NULL);

  hr = IDXGISwapChain_ResizeBuffers(swapchain, 1, (UINT)w, (UINT)h, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (FAILED(hr)) return 0;

  swap_w = w; swap_h = h;
  return create_rtv();
}

static void ensure_child_class(void)
{
  WNDCLASSEXA wc;

  if (child_class) return;

  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize        = sizeof(wc);
  wc.style         = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc   = child_wnd_proc;
  wc.hInstance     = GetModuleHandleA(NULL);
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = CHILD_CLASS_NAME;

  child_class = RegisterClassExA(&wc);
}

int d3d11_init(HWND hwnd)
{
  device_ok = 0;
  parent_hwnd = hwnd;

  ensure_child_class();
  if (!child_class) return 0;

  child_hwnd = CreateWindowExA(0, CHILD_CLASS_NAME, "", WS_CHILD | WS_VISIBLE,
                                0, 0, 1, 1, hwnd, NULL, GetModuleHandleA(NULL), NULL);
  if (!child_hwnd) return 0;

  if (!create_device_and_swapchain(1, 1))
  {
    DestroyWindow(child_hwnd);
    child_hwnd = NULL;
    return 0;
  }

  if (!create_pipeline())
  {
    release_device();
    DestroyWindow(child_hwnd);
    child_hwnd = NULL;
    return 0;
  }

  /* Not every D3D11 driver/feature-level combination advertises native
     16-bit RGB565 texture support (CheckFormatSupport, not just "did
     CreateTexture2D succeed" -- some drivers accept the format then behave
     oddly). Checked once here rather than per-frame; upload_texture() below
     converts to 32-bit on the CPU first when this comes back false. */
  {
    UINT support = 0;
    HRESULT hr = ID3D11Device_CheckFormatSupport(device, DXGI_FORMAT_B5G6R5_UNORM, &support);
    tex_needs_565_to_8888 = FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_TEXTURE2D);
    tex_fmt = tex_needs_565_to_8888 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;
  }

  device_ok = 1;
  return 1;
}

void d3d11_shutdown(void)
{
  release_device();
  if (child_hwnd) { DestroyWindow(child_hwnd); child_hwnd = NULL; }
  device_ok = 0;
}

int d3d11_available(void)
{
  return device_ok;
}

void d3d11_notify_resize(void)
{
  /* Handled lazily in d3d11_render_frame(), same as d3d9_video.c -- it
     already checks the requested size against the current swap chain size
     every frame and resizes if it no longer matches. */
}

void d3d11_set_vsync(int on)
{
  /* Nothing to do: Present() is always called with a sync interval of 0,
     and VSync is implemented in video.c by waiting for the compositor
     after each frame -- the same architecture d3d9_video.c uses, for the
     same reason (one single frame-pacing mechanism shared by every output
     path, not three separate ones that could disagree with each other). */
  (void)on;
}

/* (Re)creates the main texture only when the size or format actually needs
   to change -- the common case (same content size frame after frame) does
   nothing here at all. */
static int ensure_texture(int w, int h)
{
  D3D11_TEXTURE2D_DESC td;
  D3D11_SHADER_RESOURCE_VIEW_DESC srvd;

  if (tex && tex_w == w && tex_h == h) return 1;

  if (tex_srv) { ID3D11ShaderResourceView_Release(tex_srv); tex_srv = NULL; }
  if (tex)     { ID3D11Texture2D_Release(tex); tex = NULL; }

  ZeroMemory(&td, sizeof(td));
  td.Width = (UINT)w;
  td.Height = (UINT)h;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = tex_fmt;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DYNAMIC;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  if (FAILED(ID3D11Device_CreateTexture2D(device, &td, NULL, &tex)))
  {
    tex_w = 0; tex_h = 0;
    return 0;
  }

  ZeroMemory(&srvd, sizeof(srvd));
  srvd.Format = tex_fmt;
  srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvd.Texture2D.MipLevels = 1;

  if (FAILED(ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource *)tex, &srvd, &tex_srv)))
  {
    ID3D11Texture2D_Release(tex); tex = NULL;
    tex_w = 0; tex_h = 0;
    return 0;
  }

  tex_w = w; tex_h = h;
  return 1;
}

/* Copies src_w x src_h pixels from src_pixels (RGB565, src_pitch bytes per
   row) into the texture. Converts to 32-bit on the CPU first if the GPU
   doesn't take 565 natively (see the CheckFormatSupport call in d3d11_init). */
static int upload_texture(const uint16 *src_pixels, int src_pitch, int src_w, int src_h)
{
  D3D11_MAPPED_SUBRESOURCE mapped;
  int y;

  if (!ensure_texture(src_w, src_h)) return 0;

  if (FAILED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)tex, 0,
             D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
  {
    return 0;
  }

  for (y = 0; y < src_h; y++)
  {
    const uint16 *srow = (const uint16 *)((const uint8 *)src_pixels + (size_t)y * src_pitch);
    uint8 *drow = (uint8 *)mapped.pData + (size_t)y * mapped.RowPitch;

    if (!tex_needs_565_to_8888)
    {
      memcpy(drow, srow, (size_t)src_w * 2);
    }
    else
    {
      uint32 *drow32 = (uint32 *)drow;
      int x;

      for (x = 0; x < src_w; x++)
      {
        uint16 p = srow[x];
        uint32 r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
        uint32 r8 = (r5 << 3) | (r5 >> 2);
        uint32 g8 = (g6 << 2) | (g6 >> 4);
        uint32 b8 = (b5 << 3) | (b5 >> 2);

        drow32[x] = (0xFFu << 24) | (r8 << 16) | (g8 << 8) | b8;   /* B8G8R8A8: A,R,G,B packed as 0xAARRGGBB in a uint32 LE store == bytes B,G,R,A */
      }
    }
  }

  ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)tex, 0);
  return 1;
}

static void draw_quad(const RECT *dest, ID3D11ShaderResourceView *srv,
                      ID3D11SamplerState *sampler, ID3D11BlendState *blend)
{
  D3D11_MAPPED_SUBRESOURCE mapped;
  d3d11_vertex_t verts[4];
  D3D11_VIEWPORT vp;
  UINT stride = sizeof(d3d11_vertex_t), offset = 0;
  float x0, y0, x1, y1;
  const float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

  /* Direct3D 10+ fixed the D3D9-era half-pixel rasterization offset, so
     (unlike d3d9_video.c's draw_quad) no -0.5 adjustment belongs here --
     applying one would reintroduce a slight blur this API doesn't have. */
  x0 = ((float)dest->left   / (float)swap_w) * 2.0f - 1.0f;
  x1 = ((float)dest->right  / (float)swap_w) * 2.0f - 1.0f;
  y0 = 1.0f - ((float)dest->top    / (float)swap_h) * 2.0f;
  y1 = 1.0f - ((float)dest->bottom / (float)swap_h) * 2.0f;

  verts[0].x = x0; verts[0].y = y0; verts[0].u = 0; verts[0].v = 0;
  verts[1].x = x1; verts[1].y = y0; verts[1].u = 1; verts[1].v = 0;
  verts[2].x = x0; verts[2].y = y1; verts[2].u = 0; verts[2].v = 1;
  verts[3].x = x1; verts[3].y = y1; verts[3].u = 1; verts[3].v = 1;

  if (FAILED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)vbuffer, 0,
             D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
  {
    return;
  }
  memcpy(mapped.pData, verts, sizeof(verts));
  ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)vbuffer, 0);

  vp.TopLeftX = 0; vp.TopLeftY = 0;
  vp.Width = (float)swap_w; vp.Height = (float)swap_h;
  vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
  ID3D11DeviceContext_RSSetViewports(context, 1, &vp);

  ID3D11DeviceContext_IASetInputLayout(context, input_layout);
  ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &vbuffer, &stride, &offset);
  ID3D11DeviceContext_VSSetShader(context, vshader, NULL, 0);
  ID3D11DeviceContext_PSSetShader(context, pshader, NULL, 0);
  ID3D11DeviceContext_PSSetShaderResources(context, 0, 1, &srv);
  ID3D11DeviceContext_PSSetSamplers(context, 0, 1, &sampler);
  ID3D11DeviceContext_OMSetBlendState(context, blend, blend_factor, 0xFFFFFFFF);
  ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rtv, NULL);

  ID3D11DeviceContext_Draw(context, 4, 0);
}

int d3d11_render_frame(const uint16 *src_pixels, int src_pitch,
                        int src_w, int src_h, const RECT *dest,
                        int client_w, int client_h, int smooth)
{
  FLOAT black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

  if (!device_ok) return 0;

  if (client_w < 1 || client_h < 1) return 0;
  /* Sanity bound, not a real hardware limit -- catches garbage input
     without hardcoding a max the render filter's own multiplier already
     governs (largest possible is 720 * 4), same as d3d9_video.c. */
  if (src_w < 1 || src_h < 1 || src_w > 8192 || src_h > 8192) return 0;

  if (client_w != swap_w || client_h != swap_h)
  {
    if (!resize_swapchain(client_w, client_h)) { device_ok = 0; return 0; }
  }

  if (!rtv) return 0;

  ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rtv, NULL);
  ID3D11DeviceContext_ClearRenderTargetView(context, rtv, black);

  if (!upload_texture(src_pixels, src_pitch, src_w, src_h))
  {
    /* Same philosophy as d3d9_video.c: say so, so the caller draws this
       frame with another path instead of presenting an empty picture. */
    return 0;
  }

  draw_quad(dest, tex_srv, smooth ? samp_linear : samp_point, blend_opaque);
  return 1;
}

static int ensure_overlay_texture(int w, int h)
{
  D3D11_TEXTURE2D_DESC td;
  D3D11_SHADER_RESOURCE_VIEW_DESC srvd;

  if (overlay_tex && overlay_tex_w == w && overlay_tex_h == h) return 1;

  if (overlay_srv) { ID3D11ShaderResourceView_Release(overlay_srv); overlay_srv = NULL; }
  if (overlay_tex) { ID3D11Texture2D_Release(overlay_tex); overlay_tex = NULL; }

  ZeroMemory(&td, sizeof(td));
  td.Width = (UINT)w;
  td.Height = (UINT)h;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DYNAMIC;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  if (FAILED(ID3D11Device_CreateTexture2D(device, &td, NULL, &overlay_tex)))
  {
    overlay_tex_w = 0; overlay_tex_h = 0;
    return 0;
  }

  ZeroMemory(&srvd, sizeof(srvd));
  srvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvd.Texture2D.MipLevels = 1;

  if (FAILED(ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource *)overlay_tex, &srvd, &overlay_srv)))
  {
    ID3D11Texture2D_Release(overlay_tex); overlay_tex = NULL;
    overlay_tex_w = 0; overlay_tex_h = 0;
    return 0;
  }

  overlay_tex_w = w; overlay_tex_h = h;
  return 1;
}

int d3d11_draw_overlay(const uint32 *argb_pixels, int argb_pitch,
                        int w, int h, const RECT *screen_area)
{
  D3D11_MAPPED_SUBRESOURCE mapped;
  int y;

  if (!device_ok) return 0;
  if (w < 1 || h < 1) return 0;
  if (!ensure_overlay_texture(w, h)) return 0;

  if (FAILED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)overlay_tex, 0,
             D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
  {
    return 0;
  }

  for (y = 0; y < h; y++)
  {
    const uint8 *srow = (const uint8 *)argb_pixels + (size_t)y * argb_pitch;
    uint8 *drow = (uint8 *)mapped.pData + (size_t)y * mapped.RowPitch;
    memcpy(drow, srow, (size_t)w * 4);
  }

  ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)overlay_tex, 0);

  /* Point-sampled, same as d3d9_video.c's overlay: this is 1:1 pixel text
     and iconography, not something that should ever be softened. */
  draw_quad(screen_area, overlay_srv, samp_point, blend_alpha);
  return 1;
}

void d3d11_present(void)
{
  if (!device_ok) return;
  IDXGISwapChain_Present(swapchain, 0, 0);
}

int d3d11_get_frame_resources(void **out_device, void **out_context, void **out_frame_srv)
{
  if (!device_ok || !device || !context || !tex_srv) return 0;

  if (out_device)    *out_device    = (void *)device;
  if (out_context)   *out_context   = (void *)context;
  if (out_frame_srv) *out_frame_srv = (void *)tex_srv;
  return 1;
}
