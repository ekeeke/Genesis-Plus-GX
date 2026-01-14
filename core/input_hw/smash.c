/***************************************************************************************
 *  Genesis Plus
 *  Inovation's Smash Controller support
 *
 *  References:
 *    https://gendev.spritesmind.net/forum/viewtopic.php?t=1083
 *
 *  Copyright (C) 2025  Eke-Eke (Genesis Plus GX)
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

#include "shared.h"

INLINE unsigned char smash_read(int port)
{
  /* 9-buttons input state */
  unsigned int temp = input.pad[port];

  /* 4-bit output (active-low) */
  unsigned char data;

  /* 9-Line to 4-Line decoding (74LS147 chip) */
  if (temp & INPUT_SMASH_UP)
  {
    data = 0x9;
  }
  else if (temp & INPUT_SMASH_DOWN_RIGHT)
  {
    data = 0xD;
  }
  else if (temp & INPUT_SMASH_DOWN)
  {
    data = 0x2;
  }
  else if (temp & INPUT_SMASH_UP_LEFT)
  {
    data = 0x6;
  }
  else if (temp & INPUT_SMASH_DOWN_LEFT)
  {
    data = 0x3;
  }
  else if (temp & INPUT_SMASH_CENTER)
  {
    data = 0x7;
  }
  else if (temp & INPUT_SMASH_LEFT)
  {
    data = 0xA;
  }
  else if (temp & INPUT_SMASH_RIGHT)
  {
    data = 0xE;
  }
  else if (temp & INPUT_SMASH_UP_RIGHT)
  {
    data = 0xB;
  }
  else
  {
    data = 0xF;
  }

  /* TL/TR are connected to GND and TH is disconnected */
  return data | 0x40;
}


unsigned char smash_1_read(void)
{
  return smash_read(0);
}

unsigned char smash_2_read(void)
{
  return smash_read(4);
}
