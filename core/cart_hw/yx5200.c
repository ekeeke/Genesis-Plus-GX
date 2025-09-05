/****************************************************************************
 *  Genesis Plus
 *  YX5200-24SS audio player support
 *
 *  References:
 *   https://cdn.hackaday.io/files/936504006721600/YX5200-24SS%C3%8A%C2%B9%C3%93%C3%83%C3%8B%C2%B5%C3%83%C3%B7%C3%8A%C3%A9V1.6.zh-CN.en.pdf
 *   https://github.com/PowerBroker2/DFPlayerMini_Fast/blob/master/extras/FN-M16P%2BEmbedded%2BMP3%2BAudio%2BModule%2BDatasheet.pdf
 *   https://wiki.keyestudio.com/KS0387_keyestudio_YX5200-24SS_MP3_Module
 *
 *  Limitations:
 *   - MP3 files support only (no WAV/WMA support)
 *   - MP3 files need to be stored in same directory as game file (single device root directory playback only)
 *   - limited set of commands support (track selection, volume setting and playback control)
 *   - no command format or checksum verification
 *   - no command feedback support
 *
 *  Copyright (C) 2025 Eke-Eke (Genesis Plus GX)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/
#define MINIMP3_IMPLEMENTATION

#include "shared.h"
#include "minimp3_ex.h"

/* YX5200 UART interface RX message size */
#define YX5200_RX_BUFFER_SIZE 10

/* YX5200 audio playback limitations */
#define YX5200_MAX_TRACK_INDEX 2999
#define YX5200_MAX_VOLUME 30

typedef struct
{
  uint8 rxCycle;                          /* RX data bits reception cycle (0-9) */
  uint8 rxCounter;                        /* RX message byte counter */
  uint8 rxBuffer[YX5200_RX_BUFFER_SIZE];  /* RX message buffer */
  uint8 playbackEnabled;                  /* audio playback status (0: stopped/paused, 1: playing) */
  uint8 playbackLoop;                     /* audio playback loop mode (0: single playback, 1: repeated playback) */
  uint8 playbackVolume;                   /* audio playback volume (0-30) */
  uint8 audioEnabled;                     /* audio output (DAC) status (0: muted, 1: enabled) */
  uint16 trackIndex;                      /* audio track index (0-2999, with 0 meaning no audio track loaded) */
  mp3d_sample_t audio[2] ;                /* left & right audio channels outputs */
} T_YX5200;

static T_YX5200 yx5200;

static void yx5200_process_cmd(void);
static void yx5200_load_track(uint16 index, uint8 playbackLoop);
static void yx5200_unload_track(void);

static size_t read_cb(void *buf, size_t size, void *user_data)
{
  return cdStreamRead(buf, 1, size, (cdStream*)user_data);
}

static int seek_cb(uint64_t position, void *user_data)
{
  return cdStreamSeek((cdStream*)user_data, position, SEEK_SET);
}

static mp3dec_ex_t mp3dec;
static mp3d_sample_t audioBuffer[2048];
static mp3dec_io_t mp3io = {read_cb, NULL, seek_cb, NULL};

void yx5200_init(int samplerate)
{
  /* YX5200 audio playback rate depends on loaded audio track */
  /* Audio stream is resampled to desired rate using Blip Buffer */
  if (yx5200.trackIndex)
  {
    blip_set_rates(snd.blips[3],mp3dec.info.hz, snd.sample_rate);
  }
  else
  {
    /* set maximal MP3 samplerate when no audio track is loaded */
    blip_set_rates(snd.blips[3],48000, snd.sample_rate);
  }
}

void yx5200_reset(void)
{
  yx5200_unload_track();
  memset(&yx5200, 0x00, sizeof(yx5200));
  yx5200.playbackVolume = 30;
  yx5200.audioEnabled = 1;
}

void yx5200_write(unsigned int rx_data)
{
  /* RX data byte transfer not started ? */
  if (yx5200.rxCycle == 0)
  {
    /* START bit received ? */
    if (!rx_data)
    {
      /* initialize RX data byte reception */
      yx5200.rxBuffer[yx5200.rxCounter] = 0;
      yx5200.rxCycle = 1;
    }
  }

  /* RX data byte transfer reception in progress ? */
  else if (yx5200.rxCycle < 9)
  {
    /* update RX data byte with RX data line state (LSB first) */
    yx5200.rxBuffer[yx5200.rxCounter] |= (rx_data << (yx5200.rxCycle - 1));

    /* increment RX cycle */
    yx5200.rxCycle++;
  }

  /* RX data byte transfer finished ? */
  else
  {
    /* STOP bit received ? */
    if (rx_data)
    {
      /* increment received byte counter */
      yx5200.rxCounter++;

      /* RX message buffer filled ? */
      if (yx5200.rxCounter == YX5200_RX_BUFFER_SIZE)
      {
        /* process RX message */
        yx5200_process_cmd();

        /* reintialize RX message byte counter */
        yx5200.rxCounter = 0;
      }
    }

    /* reset RX cycle */
    yx5200.rxCycle = 0;
  }
}

void yx5200_update(unsigned int samples)
{
  /* previous audio outputs */
  mp3d_sample_t prev_l = yx5200.audio[0];
  mp3d_sample_t prev_r = yx5200.audio[1];
    
  /* get number of needed YX5200 audio samples */
  samples = blip_clocks_needed(snd.blips[3], samples);

  /* YX5200 audio playback started ? */
  if (yx5200.playbackEnabled)
  {
    int i;
    int samples_available = 0;
    int nb_channels = mp3dec.info.channels;
    int samples_to_read = samples * nb_channels;

    /* read needed audio samples */
    while (samples_available < samples_to_read)
    {
      /* update number of available audio samples */
      samples_available += mp3dec_ex_read(&mp3dec, audioBuffer + samples_available, samples_to_read - samples_available);

      /* assume either end of audio file has been reached or decoding error occurred if not all needed audio samples could be read */
      if (samples_available < samples_to_read)
      {
        /* playback loop enabled and no MP3 decoding error ? */
        if (yx5200.playbackLoop && (mp3dec.last_error == 0))
        {
          /* seek back to start of MP3 file */
          mp3dec_ex_seek(&mp3dec, 0);
        }
        else
        {
          /* stop audio playback */
          yx5200_unload_track();

          /* add silent audio samples */
          for (i=0; i<nb_channels; i++)
          {
            audioBuffer[samples_available++] = 0;
          }

          /* exit loop */
          break;
        }
      }
    }

    /* check audio is not silent or muted */
    if (yx5200.playbackVolume && yx5200.audioEnabled)
    {
      /* update blip buffer with available audio samples */
      int count = 0;
      for (i=0; i<samples_available; i+=nb_channels)
      {
        mp3d_sample_t l = (audioBuffer[i] * yx5200.playbackVolume) / YX5200_MAX_VOLUME;
        mp3d_sample_t r = (audioBuffer[i+nb_channels-1] * yx5200.playbackVolume) / YX5200_MAX_VOLUME;
        blip_add_delta_fast(snd.blips[3], count++, l-prev_l, r-prev_r);
        prev_l = l;
        prev_r = r;
      }
    }
    else
    {
      /* update blip buffer with silent audio ouput */
      blip_add_delta_fast(snd.blips[3], 0, -prev_l, -prev_r);
      prev_l = prev_r = 0;
    }
  }
  else
  {
    /* update blip buffer with silent audio ouput */
    blip_add_delta_fast(snd.blips[3], 0, -prev_l, -prev_r);
    prev_l = prev_r = 0;
  }

  /* save audio outputs for next frame */
  yx5200.audio[0] = prev_l;
  yx5200.audio[1] = prev_r;

  /* end of blip buffer timeframe */
  blip_end_frame(snd.blips[3], samples);
}

int yx5200_context_save(uint8 *state)
{
  int bufferptr = 0;

  save_param(&yx5200, sizeof(yx5200));
  save_param(&mp3dec.cur_sample, sizeof(mp3dec.cur_sample));
  return bufferptr;
}

int yx5200_context_load(uint8 *state)
{
  uint16 index;
  uint64_t cur_sample;
  int bufferptr = 0;

  yx5200_unload_track();
  load_param(&yx5200, sizeof(yx5200));
  load_param(&cur_sample, sizeof(cur_sample));

  index = yx5200.trackIndex;
  yx5200.trackIndex = 0;
  if ((index > 0) && (index <= YX5200_MAX_TRACK_INDEX))
  {
    yx5200_load_track(index, yx5200.playbackLoop);
    mp3dec_ex_seek(&mp3dec, cur_sample);
  }

  return bufferptr;
}

static void yx5200_process_cmd(void)
{
  /* Process command code (assume message format and checksum are always correct) */
  switch (yx5200.rxBuffer[3])
  {
    case 0x01:  /* Play next track (only if there is already a track loaded) */
    {
      if ((yx5200.trackIndex > 0) && (yx5200.trackIndex <= YX5200_MAX_TRACK_INDEX))
      {
        yx5200_load_track(yx5200.trackIndex + 1, yx5200.playbackLoop);
      }
      break;
    }

    case 0x02:  /* Play previous track */
    {
      if (yx5200.trackIndex > 1)
      {
        yx5200_load_track(yx5200.trackIndex - 1, yx5200.playbackLoop);
      }
      break;
    }

    case 0x03:  /* Play selected track (1-2999) */
    {
      unsigned int index = (yx5200.rxBuffer[5] << 8) | yx5200.rxBuffer[6];
      if ((index > 0) && (index <= YX5200_MAX_TRACK_INDEX))
      {
        /* playback loop seems to be enabled by default */
        yx5200_load_track(index, 1);
      }
      break;
    }

    case 0x04:  /* Increase playback volume */
    {
      if (yx5200.playbackVolume < YX5200_MAX_VOLUME)
      {
        yx5200.playbackVolume++;
      }
      break;
    }

    case 0x05:  /* Decrease playback volume */
    {
      if (yx5200.playbackVolume > 0)
      {
        yx5200.playbackVolume--;
      }
      break;
    }

    case 0x06:  /* Set playback volume (0-30) */
    {
      unsigned int volume = (yx5200.rxBuffer[5] << 8) | yx5200.rxBuffer[6];
      if (volume <= YX5200_MAX_VOLUME)
      {
        yx5200.playbackVolume = volume;
      }
      break;
    }

    case 0x08:  /* Single-repeat selected track (1-2999) */
    {
      unsigned int index = (yx5200.rxBuffer[5] << 8) | yx5200.rxBuffer[6];
      if ((index > 0) && (index <= YX5200_MAX_TRACK_INDEX))
      {
        /* purpose of this command is not clear in available documentation so, to differentiate it from command 0x03 */
        /* which appears to play selected audio track in loop, assume 'single repeat' playback means selected audio track */
        /* is played only once (to be confirmed) */
        yx5200_load_track(index, 0);
      }
      break;
    }

    case 0x0c:  /* Reset */
    {
      yx5200_reset();
      break;
    }

    case 0x0d:  /* Resume audio playback */
    {
      if (yx5200.trackIndex)
      {
        yx5200.playbackEnabled = 1;
      }
      break;
    }

    case 0x0e:  /* Pause audio playback */
    {
      yx5200.playbackEnabled = 0;
      break;
    }

    case 0x16:  /* Stop audio playback */
    {
      yx5200_unload_track();
      break;
    }

    case 0x19:  /* Enable/disable current audio track loop playback (during playback only) */
    {
      if (yx5200.playbackEnabled)
      {
        yx5200.playbackLoop = (yx5200.rxBuffer[6] & 0x01) ^ 0x01;
      }
      break;
    }

    case 0x1a:  /* Enable/disable DAC output */
    {
      yx5200.audioEnabled = (yx5200.rxBuffer[6] & 0x01) ^ 0x01;
      break;
    }

    default:  /* unsupported command */
    {
      break;
    }
  }
}

static void yx5200_load_track(uint16 index, uint8 playbackLoop)
{
  int i;
  char fname[264];
  
  /* supported filename formats (max 2999 tracks) */
  const char *fmt[4] = {"%s%01d.mp3","%s%02d.mp3","%s%03d.mp3","%s%04d.mp3"};

  /* first stop any audio playback */
  yx5200_unload_track();

  /* attempt to open MP3 file */
  for (i=0; i<4; i++)
  {
    sprintf(fname, fmt[i], rompath, index);
    mp3io.read_data = mp3io.seek_data = cdStreamOpen(fname);
    if (mp3io.read_data)
    {
      /* attempt to initialize MP3 file decoder */
      int res = mp3dec_ex_open_cb(&mp3dec, &mp3io, MP3D_SEEK_TO_SAMPLE|MP3D_DO_NOT_SCAN);

      /* valid MP3 file ? */
      if ((res == 0) && (mp3dec.info.channels > 0) && (mp3dec.info.channels <= 2))
      {
        /* indicate audio track is loaded */
        yx5200.trackIndex = index;

        /* start audio playback */
        yx5200.playbackEnabled = 1;

        /* set playback loop mode */
        yx5200.playbackLoop = playbackLoop;

        /* initialize audio stream resampler */
        blip_set_rates(snd.blips[3], mp3dec.info.hz, snd.sample_rate);
      }
      else
      {
        /* close MP3 file in case MP3 decoder initialization returns an error */
        cdStreamClose(mp3io.read_data);
      }

      /* exit loop when MP3 file has been found */
      break;
    }
  }
}

static void yx5200_unload_track()
{
  /* check audio track is loaded */
  if (yx5200.trackIndex)
  {
    /* close MP3 file decoder */
    mp3dec_ex_close(&mp3dec);

    /* close MP3 file */
    cdStreamClose(mp3io.read_data);

    /* stop audio playback */
    yx5200.playbackEnabled = 0;

    /* no audio track loaded */
    yx5200.trackIndex = 0;
  }
}
