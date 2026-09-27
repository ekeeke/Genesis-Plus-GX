/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  audio.c -- sound output through waveOut.
 *
 *  One buffer is queued per emulated frame and buffers are recycled by
 *  polling WHDR_DONE rather than through a callback, so nothing here runs on
 *  a second thread and no locking is needed. The number of buffers still in
 *  flight is what the main loop uses to pace emulation, which keeps video in
 *  step with the sound card's clock instead of the system timer.
 ****************************************************************************/

#include <windows.h>
#include <mmsystem.h>

#include "shared.h"
#include "gui.h"

/* Enough headers to cover the deepest latency setting with room to spare. */
#define WAVE_BUFFERS 16

/* Worst case is a 50 Hz frame at 48 kHz, plus slack for jitter. */
#define WAVE_FRAMES_MAX 2048
#define WAVE_BUFFER_BYTES (WAVE_FRAMES_MAX * 2 * (int)sizeof(short))

static struct
{
  HWAVEOUT  dev;
  WAVEHDR   hdr[WAVE_BUFFERS];
  short    *data[WAVE_BUFFERS];
  int       next;
  int       volume;      /* 0-100 */
  int       rate;        /* sample rate the device is open at, 0 = closed */
  int       opened;
  int       fail_streak; /* consecutive submits the driver refused */
  DWORD     last_submit; /* when a buffer was last handed to the device */
} wav;

/* Sample rate the device is currently open at, or 0 when it is closed. */
int waveout_rate(void)
{
  return wav.opened ? wav.rate : 0;
}

int waveout_open(int sample_rate)
{
  WAVEFORMATEX wfx;
  int i;

  if (wav.opened) waveout_close();

  ZeroMemory(&wav, sizeof(wav));
  wav.volume = 100;

  ZeroMemory(&wfx, sizeof(wfx));
  wfx.wFormatTag      = WAVE_FORMAT_PCM;
  wfx.nChannels       = 2;
  wfx.nSamplesPerSec  = (DWORD)sample_rate;
  wfx.wBitsPerSample  = 16;
  wfx.nBlockAlign     = (WORD)(wfx.nChannels * wfx.wBitsPerSample / 8);
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
  wfx.cbSize          = 0;

  if (waveOutOpen(&wav.dev, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
  {
    wav.dev = NULL;
    return 0;
  }

  for (i = 0; i < WAVE_BUFFERS; i++)
  {
    wav.data[i] = (short *)malloc(WAVE_BUFFER_BYTES);
    if (!wav.data[i])
    {
      waveout_close();
      return 0;
    }
    ZeroMemory(&wav.hdr[i], sizeof(WAVEHDR));
    wav.hdr[i].lpData         = (LPSTR)wav.data[i];
    wav.hdr[i].dwBufferLength = WAVE_BUFFER_BYTES;
    wav.hdr[i].dwFlags        = WHDR_DONE;   /* free */
  }

  wav.rate   = sample_rate;
  wav.opened = 1;
  wav.last_submit = GetTickCount();
  return 1;
}

void waveout_close(void)
{
  int i;

  if (wav.dev)
  {
    waveOutReset(wav.dev);

    for (i = 0; i < WAVE_BUFFERS; i++)
    {
      if (wav.hdr[i].dwFlags & WHDR_PREPARED)
      {
        waveOutUnprepareHeader(wav.dev, &wav.hdr[i], sizeof(WAVEHDR));
      }
    }

    waveOutClose(wav.dev);
    wav.dev = NULL;
  }

  for (i = 0; i < WAVE_BUFFERS; i++)
  {
    free(wav.data[i]);
    wav.data[i] = NULL;
  }

  wav.opened = 0;
}

void waveout_set_volume(int percent)
{
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  wav.volume = percent;
}

/* The device is gone or refusing everything (headset unplugged, driver
   reset): the caller should stop pacing on it and try to reopen it. */
int waveout_broken(void)
{
  return !wav.opened || wav.fail_streak >= 5;
}

/* The caller only asks while it is waiting for the device to finish a buffer
   (the queue is full). On a working device that never lasts more than a frame
   or two, because each finished buffer lets the next frame run and be
   submitted. So: nothing submitted for 1.5 s while waiting means the device
   has stopped playing. (Asking "did the queue length change?" does not work:
   sampled only while full, it always reads the same.) Returns nonzero about
   every 1.5 s while that lasts. */
int waveout_stalled(void)
{
  DWORD now = GetTickCount();

  if (!wav.opened || waveout_pending() == 0)
  {
    wav.last_submit = now;
    return 0;
  }
  if (now - wav.last_submit > 1500)
  {
    wav.last_submit = now;
    return 1;
  }
  return 0;
}

int waveout_pending(void)
{
  int i, count = 0;

  if (!wav.opened) return 0;

  for (i = 0; i < WAVE_BUFFERS; i++)
  {
    if (!(wav.hdr[i].dwFlags & WHDR_DONE)) count++;
  }
  return count;
}

void waveout_flush(void)
{
  int i;

  if (!wav.opened) return;

  waveOutReset(wav.dev);

  for (i = 0; i < WAVE_BUFFERS; i++)
  {
    if (wav.hdr[i].dwFlags & WHDR_PREPARED)
    {
      waveOutUnprepareHeader(wav.dev, &wav.hdr[i], sizeof(WAVEHDR));
    }
    wav.hdr[i].dwFlags = WHDR_DONE;
  }
  wav.next = 0;
  wav.last_submit = GetTickCount();
}

void waveout_submit(const short *samples, int frames)
{
  WAVEHDR *h;
  int slot, tries;
  int bytes;

  if (!wav.opened || frames <= 0) return;

  if (frames > WAVE_FRAMES_MAX) frames = WAVE_FRAMES_MAX;
  bytes = frames * 2 * (int)sizeof(short);

  /* Find a buffer the device has finished with. */
  for (tries = 0; tries < WAVE_BUFFERS; tries++)
  {
    slot = (wav.next + tries) % WAVE_BUFFERS;
    if (wav.hdr[slot].dwFlags & WHDR_DONE) break;
  }

  /* Everything is still queued -- we are ahead of the device, drop the frame. */
  if (tries == WAVE_BUFFERS) return;

  h = &wav.hdr[slot];

  if (h->dwFlags & WHDR_PREPARED)
  {
    waveOutUnprepareHeader(wav.dev, h, sizeof(WAVEHDR));
  }

  if (wav.volume >= 100)
  {
    memcpy(wav.data[slot], samples, (size_t)bytes);
  }
  else
  {
    /* Scale in software: waveOutSetVolume is not honoured by every driver. */
    int n = frames * 2;
    int i;
    int scale = (wav.volume * 256) / 100;
    short *dst = wav.data[slot];

    for (i = 0; i < n; i++)
    {
      dst[i] = (short)((samples[i] * scale) >> 8);
    }
  }

  h->lpData         = (LPSTR)wav.data[slot];
  h->dwBufferLength = (DWORD)bytes;
  h->dwFlags        = 0;
  h->dwLoops        = 0;

  if (waveOutPrepareHeader(wav.dev, h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
  {
    h->dwFlags = WHDR_DONE;
    wav.fail_streak++;
    return;
  }

  if (waveOutWrite(wav.dev, h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
  {
    waveOutUnprepareHeader(wav.dev, h, sizeof(WAVEHDR));
    h->dwFlags = WHDR_DONE;
    wav.fail_streak++;
    return;
  }

  wav.fail_streak = 0;
  wav.last_submit = GetTickCount();
  wav.next = (slot + 1) % WAVE_BUFFERS;
}
