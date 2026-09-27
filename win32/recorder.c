/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  recorder.c -- audio and video+audio recording. See recorder.h.
 *
 *  Threading: the emulation thread only copies each frame's picture and audio
 *  into queue items. Everything else -- picture conversion, the encoders, the
 *  file -- happens on the recorder thread, which owns the file and (for
 *  video) the Media Foundation objects from start to finish.
 ****************************************************************************/

#define COBJMACROS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include "recorder.h"

/* Windows 7 Media Foundation API version (MF_SDK_VERSION 2, MF_API_VERSION 0x70). */
#define REC_MF_VERSION  0x00020070

#define QUEUE_CAP       32        /* items waiting for the recorder thread */
#define WAIT_MS         1500      /* longest the emulation waits on a full queue */
#define STOP_WAIT_MS    30000     /* longest stopping waits for the file to be finished */

enum { ITEM_VIDEO, ITEM_AUDIO, ITEM_STOP };

typedef struct
{
  int   kind;
  int   w, h;        /* video: picture size */
  int   frames;      /* audio: sample pairs */
  void *data;
} rec_item;

/* Media Foundation entry points, resolved at run time. */
typedef HRESULT (WINAPI *PF_MFStartup)(ULONG, DWORD);
typedef HRESULT (WINAPI *PF_MFShutdown)(void);
typedef HRESULT (WINAPI *PF_MFCreateAttributes)(IMFAttributes **, UINT32);
typedef HRESULT (WINAPI *PF_MFCreateMediaType)(IMFMediaType **);
typedef HRESULT (WINAPI *PF_MFCreateSample)(IMFSample **);
typedef HRESULT (WINAPI *PF_MFCreateMemoryBuffer)(DWORD, IMFMediaBuffer **);
typedef HRESULT (WINAPI *PF_MFCreateSinkWriterFromURL)(LPCWSTR, IMFByteStream *, IMFAttributes *, IMFSinkWriter **);

static struct
{
  /* shared */
  int      mode;
  volatile LONG active;
  volatile LONG failed;
  char     path[MAX_PATH * 2];
  char     error[256];
  int      rate;
  unsigned fps_x1000;          /* frame rate * 1000 */
  int      canvas_w, canvas_h; /* video output size (even) */
  unsigned bitrate;

  HANDLE   thread, sem_items, sem_free, ev_init;
  CRITICAL_SECTION cs;
  int      cs_ready;
  rec_item *queue[QUEUE_CAP];
  int      head, tail;

  volatile LONG audio_frames_in;   /* producer-side count, for the elapsed time */

  /* recorder-thread state */
  int      init_ok;
  FILE    *wav;
  unsigned wav_bytes;
  LONGLONG vframes, aframes;

  HMODULE  mfplat, mfrw;
  int      mf_started;
  PF_MFStartup                 pMFStartup;
  PF_MFShutdown                pMFShutdown;
  PF_MFCreateAttributes        pMFCreateAttributes;
  PF_MFCreateMediaType         pMFCreateMediaType;
  PF_MFCreateSample            pMFCreateSample;
  PF_MFCreateMemoryBuffer      pMFCreateMemoryBuffer;
  PF_MFCreateSinkWriterFromURL pMFCreateSinkWriterFromURL;
  IMFSinkWriter *writer;
  DWORD    vstream, astream;

  unsigned char *nv12;
  int      nv12_size;
  int     *xmap;
  int      geo_sw, geo_sh, geo_dw, geo_dh, geo_ox, geo_oy;
  int      matrix709;
} rec;

static unsigned int rgb_lut[65536];
static int rgb_lut_ready;

static void set_error(const char *msg)
{
  lstrcpynA(rec.error, msg, sizeof(rec.error));
}

static void set_error_hr(const char *what, HRESULT hr)
{
  char buf[256];
  wsprintfA(buf, "%s (error 0x%08lX)", what, (unsigned long)hr);
  set_error(buf);
}

/****************************************************************************
 * Queue
 ****************************************************************************/

static int queue_push(rec_item *it)
{
  if (WaitForSingleObject(rec.sem_free, WAIT_MS) != WAIT_OBJECT_0) return 0;

  EnterCriticalSection(&rec.cs);
  rec.queue[rec.tail] = it;
  rec.tail = (rec.tail + 1) % QUEUE_CAP;
  LeaveCriticalSection(&rec.cs);

  ReleaseSemaphore(rec.sem_items, 1, NULL);
  return 1;
}

static rec_item *queue_pop(void)
{
  rec_item *it;

  WaitForSingleObject(rec.sem_items, INFINITE);

  EnterCriticalSection(&rec.cs);
  it = rec.queue[rec.head];
  rec.head = (rec.head + 1) % QUEUE_CAP;
  LeaveCriticalSection(&rec.cs);

  return it;
}

/****************************************************************************
 * WAV
 ****************************************************************************/

static void put16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *p, unsigned v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }

static void wav_write_header(FILE *fp, int rate, unsigned data_bytes)
{
  unsigned char h[44];

  memcpy(h, "RIFF", 4);          put32(h + 4, 36 + data_bytes);
  memcpy(h + 8, "WAVEfmt ", 8);  put32(h + 16, 16);
  put16(h + 20, 1);              /* PCM */
  put16(h + 22, 2);              /* stereo */
  put32(h + 24, (unsigned)rate);
  put32(h + 28, (unsigned)rate * 4);
  put16(h + 32, 4);
  put16(h + 34, 16);
  memcpy(h + 36, "data", 4);     put32(h + 40, data_bytes);

  fwrite(h, 1, sizeof(h), fp);
}

static int wav_open(void)
{
  rec.wav = fopen(rec.path, "wb");
  if (!rec.wav)
  {
    set_error("Could not create the recording file.");
    return 0;
  }
  wav_write_header(rec.wav, rec.rate, 0);
  rec.wav_bytes = 0;
  return 1;
}

static void wav_close(void)
{
  if (!rec.wav) return;
  fflush(rec.wav);
  fseek(rec.wav, 0, SEEK_SET);
  wav_write_header(rec.wav, rec.rate, rec.wav_bytes);
  fclose(rec.wav);
  rec.wav = NULL;
}

/****************************************************************************
 * Picture conversion: RGB565 -> NV12 (8-bit 4:2:0), scaled to the fixed
 * output size with nearest-neighbour so pixels stay sharp.
 ****************************************************************************/

static void build_rgb_lut(void)
{
  int i;

  if (rgb_lut_ready) return;
  for (i = 0; i < 65536; i++)
  {
    unsigned r = (i >> 11) & 0x1F, g = (i >> 5) & 0x3F, b = i & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    rgb_lut[i] = (r << 16) | (g << 8) | b;
  }
  rgb_lut_ready = 1;
}

static void geometry_update(int sw, int sh)
{
  int W = rec.canvas_w, H = rec.canvas_h;
  int x;

  if (sw == rec.geo_sw && sh == rec.geo_sh) return;

  if (sw * 2 == W && sh * 2 == H)
  {
    rec.geo_dw = W; rec.geo_dh = H; rec.geo_ox = 0; rec.geo_oy = 0;
  }
  else
  {
    /* Picture size changed mid-recording: fit it inside the fixed frame. */
    double sx = (double)W / sw, sy = (double)H / sh;
    double s = (sx < sy) ? sx : sy;
    int dw = ((int)(sw * s)) & ~1, dh = ((int)(sh * s)) & ~1;

    if (dw < 2) dw = 2;
    if (dh < 2) dh = 2;
    rec.geo_dw = dw; rec.geo_dh = dh;
    rec.geo_ox = ((W - dw) / 2) & ~1;
    rec.geo_oy = ((H - dh) / 2) & ~1;
  }

  for (x = 0; x < rec.geo_dw; x++) rec.xmap[x] = x * sw / rec.geo_dw;

  rec.geo_sw = sw;
  rec.geo_sh = sh;
}

static void frame_to_nv12(const unsigned short *src, int sw, int sh)
{
  const int W = rec.canvas_w, H = rec.canvas_h;
  unsigned char *Y = rec.nv12;
  unsigned char *UV = rec.nv12 + (size_t)W * H;
  int ox, oy, dw, dh;
  int y, x;

  /* Colour matrix: BT.601 for SD-sized pictures, BT.709 above -- what players
     assume when a file doesn't say. */
  const int cy0 = rec.matrix709 ?  47 :  66, cy1 = rec.matrix709 ? 157 : 129, cy2 = rec.matrix709 ? 16 : 25;
  const int cu0 = rec.matrix709 ? -26 : -38, cu1 = rec.matrix709 ? -87 : -74;
  const int cv1 = rec.matrix709 ? -102 : -94, cv2 = rec.matrix709 ? -10 : -18;

  /* Must run before the geometry is read below. */
  geometry_update(sw, sh);
  ox = rec.geo_ox; oy = rec.geo_oy; dw = rec.geo_dw; dh = rec.geo_dh;

  for (y = 0; y < H; y += 2)
  {
    unsigned char *y0 = Y + (size_t)y * W;
    unsigned char *y1 = y0 + W;
    unsigned char *uv = UV + (size_t)(y / 2) * W;

    if (y < oy || y + 1 >= oy + dh)
    {
      memset(y0, 16, (size_t)W * 2);
      memset(uv, 128, (size_t)W);
      continue;
    }

    {
      const unsigned short *r0 = src + (size_t)(((y - oy) * sh) / dh) * sw;
      const unsigned short *r1 = src + (size_t)(((y + 1 - oy) * sh) / dh) * sw;

      for (x = 0; x < W; x += 2)
      {
        unsigned int p0, p1, p2, p3;
        int R0, G0, B0, R1, G1, B1, R2, G2, B2, R3, G3, B3, R, G, B;

        if (x < ox || x + 1 >= ox + dw)
        {
          y0[x] = y0[x + 1] = y1[x] = y1[x + 1] = 16;
          uv[x] = uv[x + 1] = 128;
          continue;
        }

        p0 = rgb_lut[r0[rec.xmap[x - ox]]];
        p1 = rgb_lut[r0[rec.xmap[x + 1 - ox]]];
        p2 = rgb_lut[r1[rec.xmap[x - ox]]];
        p3 = rgb_lut[r1[rec.xmap[x + 1 - ox]]];

        R0 = (p0 >> 16) & 255; G0 = (p0 >> 8) & 255; B0 = p0 & 255;
        R1 = (p1 >> 16) & 255; G1 = (p1 >> 8) & 255; B1 = p1 & 255;
        R2 = (p2 >> 16) & 255; G2 = (p2 >> 8) & 255; B2 = p2 & 255;
        R3 = (p3 >> 16) & 255; G3 = (p3 >> 8) & 255; B3 = p3 & 255;

        y0[x]     = (unsigned char)(((cy0 * R0 + cy1 * G0 + cy2 * B0 + 128) >> 8) + 16);
        y0[x + 1] = (unsigned char)(((cy0 * R1 + cy1 * G1 + cy2 * B1 + 128) >> 8) + 16);
        y1[x]     = (unsigned char)(((cy0 * R2 + cy1 * G2 + cy2 * B2 + 128) >> 8) + 16);
        y1[x + 1] = (unsigned char)(((cy0 * R3 + cy1 * G3 + cy2 * B3 + 128) >> 8) + 16);

        R = (R0 + R1 + R2 + R3 + 2) >> 2;
        G = (G0 + G1 + G2 + G3 + 2) >> 2;
        B = (B0 + B1 + B2 + B3 + 2) >> 2;

        uv[x]     = (unsigned char)(((cu0 * R + cu1 * G + 112 * B + 128) >> 8) + 128);
        uv[x + 1] = (unsigned char)(((112 * R + cv1 * G + cv2 * B + 128) >> 8) + 128);
      }
    }
  }
}

/****************************************************************************
 * Media Foundation (MP4: H.264 + AAC)
 ****************************************************************************/

static int mf_load(void)
{
  rec.mfplat = LoadLibraryExA("mfplat.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  rec.mfrw   = LoadLibraryExA("mfreadwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

  if (rec.mfplat && rec.mfrw)
  {
    rec.pMFStartup                 = (PF_MFStartup)GetProcAddress(rec.mfplat, "MFStartup");
    rec.pMFShutdown                = (PF_MFShutdown)GetProcAddress(rec.mfplat, "MFShutdown");
    rec.pMFCreateAttributes        = (PF_MFCreateAttributes)GetProcAddress(rec.mfplat, "MFCreateAttributes");
    rec.pMFCreateMediaType         = (PF_MFCreateMediaType)GetProcAddress(rec.mfplat, "MFCreateMediaType");
    rec.pMFCreateSample            = (PF_MFCreateSample)GetProcAddress(rec.mfplat, "MFCreateSample");
    rec.pMFCreateMemoryBuffer      = (PF_MFCreateMemoryBuffer)GetProcAddress(rec.mfplat, "MFCreateMemoryBuffer");
    rec.pMFCreateSinkWriterFromURL = (PF_MFCreateSinkWriterFromURL)GetProcAddress(rec.mfrw, "MFCreateSinkWriterFromURL");
  }

  if (!rec.pMFStartup || !rec.pMFShutdown || !rec.pMFCreateAttributes || !rec.pMFCreateMediaType ||
      !rec.pMFCreateSample || !rec.pMFCreateMemoryBuffer || !rec.pMFCreateSinkWriterFromURL)
  {
    set_error("Windows Media Foundation isn't available on this system, so video can't be recorded.\n"
              "(On Windows N editions, install the Media Feature Pack.)");
    return 0;
  }
  return 1;
}

static void mf_close(void)
{
  if (rec.writer)
  {
    IMFSinkWriter_Finalize(rec.writer);   /* writes the MP4 index; without it the file won't play */
    IMFSinkWriter_Release(rec.writer);
    rec.writer = NULL;
  }
  if (rec.mf_started)
  {
    rec.pMFShutdown();
    rec.mf_started = 0;
  }
  if (rec.mfrw)   { FreeLibrary(rec.mfrw);   rec.mfrw = NULL; }
  if (rec.mfplat) { FreeLibrary(rec.mfplat); rec.mfplat = NULL; }
}

#define SETU32(t, key, v)   IMFAttributes_SetUINT32((IMFAttributes *)(t), &(key), (UINT32)(v))
#define SETU64(t, key, hi, lo) \
  IMFAttributes_SetUINT64((IMFAttributes *)(t), &(key), ((UINT64)(UINT32)(hi) << 32) | (UINT32)(lo))
#define SETGUID(t, key, g)  IMFAttributes_SetGUID((IMFAttributes *)(t), &(key), &(g))

static int mf_open(void)
{
  IMFAttributes *attr = NULL;
  IMFMediaType *t = NULL;
  wchar_t wpath[MAX_PATH * 2];
  HRESULT hr;
  int ok = 0;
  const int W = rec.canvas_w, H = rec.canvas_h;

  if (!mf_load()) return 0;

  hr = rec.pMFStartup(REC_MF_VERSION, 0);
  if (FAILED(hr)) { set_error_hr("Could not start Windows Media Foundation", hr); return 0; }
  rec.mf_started = 1;

  hr = rec.pMFCreateAttributes(&attr, 2);
  if (FAILED(hr)) { set_error_hr("Could not create the encoder settings", hr); return 0; }
  SETGUID(attr, MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
  SETU32(attr, MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

  MultiByteToWideChar(CP_ACP, 0, rec.path, -1, wpath, MAX_PATH * 2);
  hr = rec.pMFCreateSinkWriterFromURL(wpath, NULL, attr, &rec.writer);
  IMFAttributes_Release(attr);
  if (FAILED(hr)) { set_error_hr("Could not create the MP4 file", hr); return 0; }

  /* --- video: NV12 in, H.264 out ------------------------------------- */
  hr = rec.pMFCreateMediaType(&t);
  if (FAILED(hr)) { set_error_hr("Could not set up video encoding", hr); return 0; }
  SETGUID(t, MF_MT_MAJOR_TYPE, MFMediaType_Video);
  SETGUID(t, MF_MT_SUBTYPE, MFVideoFormat_H264);
  SETU32(t, MF_MT_AVG_BITRATE, rec.bitrate);
  SETU32(t, MF_MT_INTERLACE_MODE, 2);                       /* progressive */
  SETU64(t, MF_MT_FRAME_SIZE, W, H);
  SETU64(t, MF_MT_FRAME_RATE, rec.fps_x1000, 1000);
  SETU64(t, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  SETU32(t, MF_MT_MPEG2_PROFILE, 77);                       /* H.264 Main */
  hr = IMFSinkWriter_AddStream(rec.writer, t, &rec.vstream);
  IMFMediaType_Release(t); t = NULL;
  if (FAILED(hr))
  {
    set_error_hr("Windows' H.264 video encoder isn't available.\n"
                 "(On Windows N editions, install the Media Feature Pack.)", hr);
    return 0;
  }

  hr = rec.pMFCreateMediaType(&t);
  if (FAILED(hr)) { set_error_hr("Could not set up video encoding", hr); return 0; }
  SETGUID(t, MF_MT_MAJOR_TYPE, MFMediaType_Video);
  SETGUID(t, MF_MT_SUBTYPE, MFVideoFormat_NV12);
  SETU32(t, MF_MT_INTERLACE_MODE, 2);
  SETU64(t, MF_MT_FRAME_SIZE, W, H);
  SETU64(t, MF_MT_FRAME_RATE, rec.fps_x1000, 1000);
  SETU64(t, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  SETU32(t, MF_MT_YUV_MATRIX, rec.matrix709 ? 1 : 2);       /* MFVideoTransferMatrix: BT.709 = 1, BT.601 = 2 */
  SETU32(t, MF_MT_VIDEO_NOMINAL_RANGE, 2);                  /* 16-235 */
  hr = IMFSinkWriter_SetInputMediaType(rec.writer, rec.vstream, t, NULL);
  IMFMediaType_Release(t); t = NULL;
  if (FAILED(hr)) { set_error_hr("The video encoder rejected the picture format", hr); return 0; }

  /* --- audio: 16-bit stereo PCM in, AAC out --------------------------- */
  hr = rec.pMFCreateMediaType(&t);
  if (FAILED(hr)) { set_error_hr("Could not set up audio encoding", hr); return 0; }
  SETGUID(t, MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  SETGUID(t, MF_MT_SUBTYPE, MFAudioFormat_AAC);
  SETU32(t, MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
  SETU32(t, MF_MT_AUDIO_SAMPLES_PER_SECOND, rec.rate);
  SETU32(t, MF_MT_AUDIO_NUM_CHANNELS, 2);
  SETU32(t, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 20000);       /* 160 kbit/s */
  SETU32(t, MF_MT_AAC_PAYLOAD_TYPE, 0);                     /* raw AAC */
  SETU32(t, MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
  hr = IMFSinkWriter_AddStream(rec.writer, t, &rec.astream);
  IMFMediaType_Release(t); t = NULL;
  if (FAILED(hr))
  {
    set_error_hr("Windows' AAC audio encoder isn't available.\n"
                 "(On Windows N editions, install the Media Feature Pack.)", hr);
    return 0;
  }

  hr = rec.pMFCreateMediaType(&t);
  if (FAILED(hr)) { set_error_hr("Could not set up audio encoding", hr); return 0; }
  SETGUID(t, MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  SETGUID(t, MF_MT_SUBTYPE, MFAudioFormat_PCM);
  SETU32(t, MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
  SETU32(t, MF_MT_AUDIO_SAMPLES_PER_SECOND, rec.rate);
  SETU32(t, MF_MT_AUDIO_NUM_CHANNELS, 2);
  SETU32(t, MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
  SETU32(t, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, rec.rate * 4);
  SETU32(t, MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  hr = IMFSinkWriter_SetInputMediaType(rec.writer, rec.astream, t, NULL);
  IMFMediaType_Release(t); t = NULL;
  if (FAILED(hr)) { set_error_hr("The audio encoder rejected the sound format", hr); return 0; }

  hr = IMFSinkWriter_BeginWriting(rec.writer);
  if (FAILED(hr)) { set_error_hr("Could not start writing the MP4 file", hr); return 0; }

  ok = 1;
  return ok;
}

static int mf_write(DWORD stream, const void *data, DWORD len, LONGLONG time, LONGLONG duration)
{
  IMFMediaBuffer *buf = NULL;
  IMFSample *sample = NULL;
  BYTE *p = NULL;
  HRESULT hr;
  int ok = 0;

  hr = rec.pMFCreateMemoryBuffer(len, &buf);
  if (FAILED(hr)) goto done;

  hr = IMFMediaBuffer_Lock(buf, &p, NULL, NULL);
  if (FAILED(hr)) goto done;
  memcpy(p, data, len);
  IMFMediaBuffer_Unlock(buf);
  IMFMediaBuffer_SetCurrentLength(buf, len);

  hr = rec.pMFCreateSample(&sample);
  if (FAILED(hr)) goto done;
  IMFSample_AddBuffer(sample, buf);
  IMFSample_SetSampleTime(sample, time);
  IMFSample_SetSampleDuration(sample, duration);

  hr = IMFSinkWriter_WriteSample(rec.writer, stream, sample);
  ok = SUCCEEDED(hr);

done:
  if (sample) IMFSample_Release(sample);
  if (buf) IMFMediaBuffer_Release(buf);
  return ok;
}

/****************************************************************************
 * Recorder thread
 ****************************************************************************/

static LONGLONG video_time(LONGLONG n)
{
  return n * 10000000LL * 1000LL / (LONGLONG)rec.fps_x1000;   /* 100 ns units */
}

static void handle_item(rec_item *it)
{
  if (InterlockedCompareExchange(&rec.failed, 0, 0)) return;

  if (it->kind == ITEM_AUDIO)
  {
    if (rec.mode == REC_AUDIO)
    {
      size_t n = (size_t)it->frames * 4;
      if (rec.wav && fwrite(it->data, 1, n, rec.wav) != n)
      {
        set_error("Could not write to the recording file (disk full?).");
        InterlockedExchange(&rec.failed, 1);
      }
      rec.wav_bytes += (unsigned)n;
    }
    else
    {
      LONGLONG t = rec.aframes * 10000000LL / rec.rate;
      LONGLONG d = ((rec.aframes + it->frames) * 10000000LL / rec.rate) - t;

      if (!mf_write(rec.astream, it->data, (DWORD)it->frames * 4, t, d))
      {
        set_error("The audio encoder stopped working.");
        InterlockedExchange(&rec.failed, 1);
      }
    }
    rec.aframes += it->frames;
  }
  else if (it->kind == ITEM_VIDEO)
  {
    LONGLONG t = video_time(rec.vframes);
    LONGLONG d = video_time(rec.vframes + 1) - t;

    frame_to_nv12((const unsigned short *)it->data, it->w, it->h);

    if (!mf_write(rec.vstream, rec.nv12, (DWORD)rec.nv12_size, t, d))
    {
      set_error("The video encoder stopped working.");
      InterlockedExchange(&rec.failed, 1);
    }
    rec.vframes++;
  }
}

static DWORD WINAPI recorder_thread(LPVOID unused)
{
  HRESULT co;
  int ok;

  (void)unused;
  co = CoInitializeEx(NULL, COINIT_MULTITHREADED);

  if (rec.mode == REC_VIDEO)
  {
    build_rgb_lut();
    rec.nv12_size = rec.canvas_w * rec.canvas_h * 3 / 2;
    rec.nv12 = (unsigned char *)malloc((size_t)rec.nv12_size);
    rec.xmap = (int *)malloc(sizeof(int) * (size_t)rec.canvas_w);
    ok = (rec.nv12 && rec.xmap) ? mf_open() : 0;
    if (!rec.nv12 || !rec.xmap) set_error("Out of memory.");
  }
  else
  {
    ok = wav_open();
  }

  rec.init_ok = ok;
  SetEvent(rec.ev_init);

  if (ok)
  {
    for (;;)
    {
      rec_item *it = queue_pop();
      int stop = (it->kind == ITEM_STOP);

      if (!stop) handle_item(it);
      free(it->data);
      free(it);
      ReleaseSemaphore(rec.sem_free, 1, NULL);

      if (stop) break;
    }
  }

  if (rec.mode == REC_VIDEO) mf_close(); else wav_close();

  free(rec.nv12); rec.nv12 = NULL;
  free(rec.xmap); rec.xmap = NULL;

  if (SUCCEEDED(co)) CoUninitialize();
  return 0;
}

/****************************************************************************
 * Public interface (emulation thread)
 ****************************************************************************/

int recorder_start(int mode, const char *path, int sample_rate, double fps)
{
  const unsigned short *px;
  int pitch, sw = 0, sh = 0;

  if (rec.active) return 0;

  /* A previous recording that did not finish in time is still winding down on
     its own thread: it owns `rec`, so do not wipe it. */
  if (rec.thread)
  {
    set_error("The previous recording is still being finished. Try again in a moment.");
    return 0;
  }

  memset(&rec, 0, sizeof(rec));
  rec.mode  = mode;
  rec.rate  = (sample_rate == 44100) ? 44100 : 48000;
  if (fps < 1.0) fps = 60.0;
  rec.fps_x1000 = (unsigned)(fps * 1000.0 + 0.5);
  lstrcpynA(rec.path, path, sizeof(rec.path));

  if (mode == REC_VIDEO)
  {
    if (!video_capture_frame(&px, &pitch, &sw, &sh) || sw < 16 || sh < 16)
    {
      set_error("There is no picture to record yet.");
      return 0;
    }

    /* 2x the current picture, integer-scaled and sharp. Fixed for the whole
       file: a later change in the game's resolution is letterboxed into it. */
    rec.canvas_w = (sw * 2 + 1) & ~1;
    rec.canvas_h = (sh * 2 + 1) & ~1;
    rec.matrix709 = (rec.canvas_w >= 1280 || rec.canvas_h >= 720);

    {
      double bps = (double)rec.canvas_w * rec.canvas_h * fps * 0.5;
      if (bps < 4000000.0)  bps = 4000000.0;
      if (bps > 20000000.0) bps = 20000000.0;
      rec.bitrate = (unsigned)bps;
    }
  }

  InitializeCriticalSection(&rec.cs);
  rec.cs_ready  = 1;
  rec.sem_items = CreateSemaphoreA(NULL, 0, QUEUE_CAP, NULL);
  rec.sem_free  = CreateSemaphoreA(NULL, QUEUE_CAP, QUEUE_CAP, NULL);
  rec.ev_init   = CreateEventA(NULL, TRUE, FALSE, NULL);
  rec.geo_sw = rec.geo_sh = -1;

  rec.thread = CreateThread(NULL, 0, recorder_thread, NULL, 0, NULL);
  if (!rec.thread || !rec.sem_items || !rec.sem_free || !rec.ev_init)
  {
    set_error("Could not start the recorder.");
    rec.init_ok = 0;
  }
  else
  {
    WaitForSingleObject(rec.ev_init, 20000);
  }

  if (!rec.init_ok)
  {
    /* The thread has already cleaned up after itself when init failed. */
    if (rec.thread)
    {
      WaitForSingleObject(rec.thread, 10000);
      CloseHandle(rec.thread);
    }
    if (rec.sem_items) CloseHandle(rec.sem_items);
    if (rec.sem_free)  CloseHandle(rec.sem_free);
    if (rec.ev_init)   CloseHandle(rec.ev_init);
    DeleteCriticalSection(&rec.cs);
    rec.cs_ready = 0;
    rec.thread = NULL;
    DeleteFileA(rec.path);   /* don't leave an empty file behind */
    return 0;
  }

  InterlockedExchange(&rec.active, mode);
  return 1;
}

void recorder_capture(const short *samples, int frames)
{
  rec_item *it;

  if (!rec.active || InterlockedCompareExchange(&rec.failed, 0, 0)) return;

  if (rec.mode == REC_VIDEO)
  {
    const unsigned short *px;
    int pitch, w, h, y;

    if (video_capture_frame(&px, &pitch, &w, &h) && w > 0 && h > 0)
    {
      unsigned char *dst;

      it = (rec_item *)malloc(sizeof(*it));
      dst = (unsigned char *)malloc((size_t)w * h * 2);
      if (!it || !dst) { free(it); free(dst); goto audio; }

      it->kind = ITEM_VIDEO; it->w = w; it->h = h; it->frames = 0; it->data = dst;
      for (y = 0; y < h; y++)
        memcpy(dst + (size_t)y * w * 2, (const unsigned char *)px + (size_t)y * pitch, (size_t)w * 2);

      if (!queue_push(it))
      {
        free(dst); free(it);
        set_error("The encoder can't keep up.");
        InterlockedExchange(&rec.failed, 1);
        return;
      }
    }
  }

audio:
  if (samples && frames > 0)
  {
    void *copy = malloc((size_t)frames * 4);
    it = (rec_item *)malloc(sizeof(*it));
    if (!copy || !it) { free(copy); free(it); return; }

    memcpy(copy, samples, (size_t)frames * 4);
    it->kind = ITEM_AUDIO; it->w = it->h = 0; it->frames = frames; it->data = copy;

    if (!queue_push(it))
    {
      free(copy); free(it);
      set_error("The encoder can't keep up.");
      InterlockedExchange(&rec.failed, 1);
      return;
    }
    InterlockedExchangeAdd(&rec.audio_frames_in, frames);
  }
}

void recorder_stop(void)
{
  rec_item *it;

  if (!rec.active) return;

  it = (rec_item *)calloc(1, sizeof(*it));
  if (it) it->kind = ITEM_STOP;

  if (!it || !queue_push(it))
  {
    /* Queue jammed: the thread is stuck. Nothing more can be done for the file. */
    free(it);
  }
  else if (WaitForSingleObject(rec.thread, STOP_WAIT_MS) != WAIT_OBJECT_0)
  {
    /* Still finishing after 30 s: leave it to finish on its own. */
    InterlockedExchange(&rec.active, 0);
    return;
  }

  CloseHandle(rec.thread);
  CloseHandle(rec.sem_items);
  CloseHandle(rec.sem_free);
  CloseHandle(rec.ev_init);
  DeleteCriticalSection(&rec.cs);
  rec.cs_ready = 0;
  rec.thread = NULL;
  InterlockedExchange(&rec.active, 0);
}

int recorder_active(void)         { return (int)rec.active; }
int recorder_failed(void)         { return rec.failed ? 1 : 0; }
const char *recorder_error(void)  { return rec.error; }
const char *recorder_path(void)   { return rec.path; }

unsigned recorder_seconds(void)
{
  if (!rec.active || rec.rate <= 0) return 0;
  return (unsigned)((LONGLONG)rec.audio_frames_in / rec.rate);
}
