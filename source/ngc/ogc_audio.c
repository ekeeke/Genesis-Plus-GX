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

/*** AudioSwitchBuffers
     Genesis Plus only provides sound data on completion of each frame.
     To try to make the audio less choppy, this function is called from both the
     DMA completion and update_audio.
     Testing for data in the buffer ensures that there are no clashes.
 ***/
static void AudioSwitchBuffers()
{
  if (ConfigRequested)
  {
    IsPlaying = 0;
    return;
  }

  u32 dma_len = (vdp_pal) ? 3840 : 3200;

  /* restart audio DMA with current soundbuffer */
  AUDIO_InitDMA((u32) soundbuffer[playbuffer], dma_len);
  DCFlushRange(soundbuffer[playbuffer], dma_len);
  AUDIO_StartDMA();

  /* increment soundbuffers index */
  playbuffer++;
  playbuffer &= 0xf;

  if (playbuffer == mixbuffer)
  {
    playbuffer--;
    if ( playbuffer < 0 ) playbuffer = 15;
  }
  IsPlaying = 1;
}


void ogc_audio__init(void)
{
  AUDIO_Init (NULL);
  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_48KHZ);
  AUDIO_RegisterDMACallback (AudioSwitchBuffers);
}

void ogc_audio__reset(void)
{
  AUDIO_StopDMA ();
  IsPlaying = 0;
  mixbuffer = 0;
  playbuffer = 0;
  memset(soundbuffer, 0, 16 * 3840);
}

void ogc_audio__update(void)
{
  /* restart Audio DMA if needed */
  if (!IsPlaying) AudioSwitchBuffers();
}
