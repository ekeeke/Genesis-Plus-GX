/****************************************************************************
 *  ogc_audio.c
 *
 *  Genesis Plus GX audio support
 *
 *  code by Softdev (2006), Eke-Eke (2007,2008)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ***************************************************************************/

#include "shared.h"

/* DMA soundbuffers (required to be 32-bytes aligned)
   Length is dimensionned for one frame of emulation (see below)
   To prevent audio clashes, we use double buffering technique:
    one buffer is the active DMA buffer
    the other one is the current work buffer (updated during frame emulation)
   We do not need more since frame emulation is synchronized with DMA execution
*/
u8 soundbuffer[2][3840] ATTRIBUTE_ALIGN(32);

/* Current work soundbuffer */
u8 mixbuffer;

/* Current DMA length (required to be a factor of 32-bytes)
   length is calculated regarding current emulation timings:
    PAL timings : 50 frames/sec, 48000 samples/sec = 960 samples per frame = 3840 bytes (16 bits stereo samples)
    NTSC timings: 60 frames/sec, 48000 samples/sec = 800 samples per frame = 3200 bytes (16 bits stereo samples)
*/
static u32 dma_len  = 3200;

/*** 
      AudioDmaCallback

     Genesis Plus only provides sound data on completion of each frame.
     To try to make the audio less choppy, we synchronize frame emulation with audio DMA
     This ensure we don't access current active audiobuffer until a new DMA has been started
     This also makes frame emulation sync completely independent from video sync
 ***/
static void AudioDmaCallback()
{
  frameticker++;
}

/***
      ogc_audio__init

     This function initializes the Audio Interface and DMA callback
     Default samplerate is set to 48khZ
 ***/
void ogc_audio__init(void)
{
  AUDIO_Init (NULL);
  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_48KHZ);
  AUDIO_RegisterDMACallback (AudioDmaCallback);
  memset(soundbuffer, 0, 2 * 3840);
}

/*** 
      ogc_audio__update

     This function is called at the end of each frame, once the current soundbuffer has been updated
     DMA sync and switching ensure we never access the active DMA buffer
     This function set the next DMA buffer 
     Parameters will be taken in account ONLY when current DMA has finished
 ***/
void ogc_audio__update(void)
{
  AUDIO_InitDMA((u32) soundbuffer[mixbuffer], dma_len);
  DCFlushRange(soundbuffer[mixbuffer], dma_len);
  mixbuffer ^= 1;
}

/*** 
      ogc_audio__start

     This function restarts Audio DMA processing
     This is called only ONCE, when emulation loop starts or when coming back from Main Menu
     DMA length is used for frame emulation synchronization (PAL & NTSC timings)
     DMA is *automatically* restarted once the configured audio length has been output
     When DMA parameters are not updated, same soundbuffer is played again
     DMA parameters can be updated using successive calls to AUDIO_InitDMA (see above)
 ***/
void ogc_audio__start(void)
{
  dma_len = vdp_pal ? 3840 : 3200;
  memset(soundbuffer[0], 0, dma_len);
  AUDIO_InitDMA((u32) soundbuffer[0], dma_len);
  DCFlushRange(soundbuffer[0], dma_len);
  AUDIO_StartDMA();
  mixbuffer = 1;
}

/***
      ogc_audio__stop

     This function reset current Audio DMA process
     This is called when going back to Main Menu
     DMA need to be restarted when going back to the game (see above)
 ***/
void ogc_audio__stop(void)
{
  AUDIO_StopDMA ();
}
