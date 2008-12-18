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
unsigned char soundbuffer[16][3840] ATTRIBUTE_ALIGN(32);
int mixbuffer = 0;

static int playbuffer  = 0;
static int IsPlaying   = 0;
static u32 dma_len = 3200;

/*** AudioSwitchBuffers
     Genesis Plus only provides sound data on completion of each frame.
     To try to make the audio less choppy, this function is called from both the
     DMA completion and update_audio.
     Testing for data in the buffer ensures that there are no clashes.
 ***/
static void AudioSwitchBuffers()
{
  /* increment soundbuffers index */
  playbuffer++;
  playbuffer &= 0xf;

  /* reset audio DMA parameters */
  AUDIO_InitDMA((u32) soundbuffer[playbuffer], dma_len);
}

void ogc_audio__init(void)
{
  AUDIO_Init (NULL);
  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_48KHZ);
 // AUDIO_RegisterDMACallback (AudioSwitchBuffers);
}

void ogc_audio__reset(void)
{
  IsPlaying = 0;
  mixbuffer = 0;
  playbuffer = 0;
  memset(soundbuffer, 0, 16 * 3840);
}

void ogc_audio__update(void)
{
  /* flush data from CPU cache */
  DCFlushRange(soundbuffer[mixbuffer], dma_len);
  AUDIO_InitDMA((u32) soundbuffer[mixbuffer], dma_len);
}

void ogc_audio__stop(void)
{
  /* stop audio DMA */
  AUDIO_StopDMA ();
  AUDIO_InitDMA((u32) soundbuffer[0], dma_len);
  IsPlaying = 0;
  mixbuffer = 0;
  playbuffer = 0;
}

void ogc_audio__start(void)
{
  if (!IsPlaying)
  {
    /* buffer size */
    dma_len = vdp_pal ? 3840 : 3200;

    /* set default DMA parameters */
    AUDIO_InitDMA((u32) soundbuffer[0], dma_len);

    /* start audio DMA */
    AUDIO_StartDMA();

    IsPlaying = 1;
  }
}
