/***************************************************************************************
 *  Genesis Plus
 *  Sega Paddle Control support
 *
 *  Copyright (C) 2007-2023  Eke-Eke (Genesis Plus GX)
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

#include "../macros.h"
#include "input.h"
#include "state.h"

void paddle_reset(int port)
{
  input.analog[port][0] = 128;
  paddle[port>>2].State = 0x40;
  paddle[port>>2].Counter = 0;
}

INLINE unsigned char paddle_read(int port)
{
  /* FIRE button status (active low) */
  unsigned char temp = ~(input.pad[port] & 0x10);

  /* Pad index */
  int index = port >> 2;

  /* Clear low bits */
  temp &= 0x70;

  /* Japanese model: automatic flip-flop */
  if (region_code < REGION_USA)
  {
    /* two I/O port reads are required to fully read paddles state on control ports 1 & 2 so using two read access latency for switching should be safe */
    /* note: real paddle switching time is approx. 62.5 us period according to https://www.raphnet.net/electronique/sms_paddle/index_en.php#4 */
    if (++paddle[index].Counter > 2)
    {
      paddle[index].Counter = 0;
      paddle[index].State ^= 0x40;
    }
  }

  if (paddle[index].State & 0x40)
  {
    /* return higher bits */
    temp |= (input.analog[port][0] >> 4) & 0x0F;
  }
  else
  {
    /* return lower bits */
    temp |= input.analog[port][0] & 0x0F;

    /* set TR low */
    temp &= ~0x20;
  }

  return temp;
}

INLINE void paddle_write(int index, unsigned char data, unsigned char mask)
{
  /* update bits set as output only */
  paddle[index].State = (paddle[index].State & ~mask) | (data & mask);
}


unsigned char paddle_1_read(void)
{
  return paddle_read(0);
}

unsigned char paddle_2_read(void)
{
  return paddle_read(4);
}

void paddle_1_write(unsigned char data, unsigned char mask)
{
  paddle_write(0, data, mask);
}

void paddle_2_write(unsigned char data, unsigned char mask)
{
  paddle_write(1, data, mask);
}
