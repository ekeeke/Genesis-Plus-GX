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

/* global datas */
unsigned char soundbuffer[2][3840] ATTRIBUTE_ALIGN(32);
u8 mixbuffer = 0;

static int IsPlaying    = 0;
static int dma_len      = 3200;

/*** AudioDmaCallback
     Genesis Plus only provides sound data on completion of each frame.
     To try to make the audio less choppy, we synchronize emulation with audio DMA
     This ensure we only update audio framebuffer when DMA is finished
 ***/
static void AudioDmaCallback()
{
  frameticker++;
}

void ogc_audio__init(void)
{
  AUDIO_Init (NULL);
  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_48KHZ);
  AUDIO_RegisterDMACallback (AudioDmaCallback);
  mixbuffer = 0;
  memset(soundbuffer, 0, 2 * 3840);
}

void ogc_audio__update(void)
{
  /* buffer size */
  uint32 dma_len = vdp_pal ? 3840 : 3200;

  /* update DMA settings (this will only be taken in account when current DMA finishes) */
  AUDIO_InitDMA((u32) soundbuffer[mixbuffer], dma_len);

  /* flush data from CPU cache */
  DCFlushRange(soundbuffer[mixbuffer], dma_len);

  /* switch buffer */
  mixbuffer ^= 1;
}

void ogc_audio__stop(void)
{
  /* stop audio DMA */
  AUDIO_StopDMA ();
  IsPlaying = 0;

  /* reset audio buffers */
  mixbuffer = 0;
  memset(soundbuffer, 0, 2 * 3840);
}

void ogc_audio__start(void)
{
  if (!IsPlaying)
  {
    /* buffer size */
    dma_len = vdp_pal ? 3840 : 3200;

    /* set first DMA parameters */
    AUDIO_InitDMA((u32) soundbuffer[1], dma_len);

    /* start audio DMA */
    AUDIO_StartDMA();

    IsPlaying = 1;
  }
}
