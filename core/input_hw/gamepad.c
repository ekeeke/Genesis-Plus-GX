/***************************************************************************************
 *  Genesis Plus
 *  2-Buttons, 3-Buttons & 6-Buttons controller support
 *  Additional support for J-Cart, 4-Way Play & Master Tap adapters
 *
 *  Copyright (C) 2007-2015  Eke-Eke (Genesis Plus GX)
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
#include "gamepad.h"

static struct
{
  uint8 State;
  uint8 Counter;
  uint8 Timeout;
} gamepad[MAX_DEVICES];

static struct
{
  uint8 Latch;
  uint8 Counter;
} flipflop[2];

static uint8 latch;


void gamepad_reset(int port)
{
  /* default state (Gouketsuji Ichizoku / Power Instinct, Samurai Spirits / Samurai Shodown) */
  gamepad[port].State = 0x40;
  gamepad[port].Counter = 0;
  gamepad[port].Timeout = 0;

  /* reset 4-WayPlay latch */
  latch = 0;

  /* reset Master Tap flip-flop */
  flipflop[port>>2].Latch = 0;
  flipflop[port>>2].Counter = 0;
}

void gamepad_refresh(int port)
{
  /* 6-buttons pad */
  if (gamepad[port].Timeout++ > 25)
  {
    gamepad[port].Counter = 0;
    gamepad[port].Timeout = 0;
  }
}

INLINE unsigned char gamepad_read(int port)
{
  /* D7 is not connected, D6 returns TH input state */
  unsigned int data = gamepad[port].State | 0x3F;

  /* pad value */
  unsigned int val = input.pad[port];

  /* get current TH input pulse counter */
  unsigned int step = gamepad[port].Counter | (data >> 6);

  /* D-PAD & buttons status are returned on D5-D0 (active low) */
  switch (step)
  {
    case 1: /*** First High  ***/
    case 3: /*** Second High ***/
    case 5: /*** Third High  ***/
    {
      /* TH = 1 : ?1CBRLDU */
      data &= ~(val & 0x3F);
      break;
    }

    case 0: /*** First Low  ***/
    case 2: /*** Second Low ***/
    {
      /* TH = 0 : ?0SA00DU */
      data &= ~(val & 0x03);
      data &= ~((val >> 2) & 0x30);
      data &= ~0x0C;
      break;
    }

    /* 6-buttons specific (taken from gen-hw.txt) */
    /* A 6-button gamepad allows the extra buttons to be read based on how */
    /* many times TH is switched from 1 to 0 (and not 0 to 1). Observe the */
    /* following sequence */
    /*
       TH = 1 : ?1CBRLDU    3-button pad return value
       TH = 0 : ?0SA00DU    3-button pad return value
       TH = 1 : ?1CBRLDU    3-button pad return value
       TH = 0 : ?0SA0000    D3-D0 are forced to '0'
       TH = 1 : ?1CBMXYZ    Extra buttons returned in D3-0
       TH = 0 : ?0SA1111    D3-D0 are forced to '1'
    */
    case 4: /*** Third Low ***/
    {
      /* TH = 0 : ?0SA0000    D3-D0 forced to '0' */
      data &= ~((val >> 2) & 0x30);
      data &= ~0x0F;
      break;
    }

    case 6: /*** Fourth Low ***/
    {
      /* TH = 0 : ?0SA1111    D3-D0 forced to '1' */
      data &= ~((val >> 2) & 0x30);
      break;
    }

    case 7: /*** Fourth High ***/
    {
      /* TH = 1 : ?1CBMXYZ    Extra buttons returned in D3-D0 */
      data &= ~(val & 0x30);
      data &= ~((val >> 8) & 0x0F);
      break;
    }
  }

  return data;
}

INLINE void gamepad_write(int port, unsigned char data, unsigned char mask)
{
  /* retrieve TH input state (pulled high when not configured as output by I/O controller) */
  data = ((data & mask) | ~mask) & 0x40;

  /* 6-Buttons controller specific */
  if (input.dev[port] == DEVICE_PAD6B)
  {
    /* TH=0 to TH=1 transition */
    if (!gamepad[port].State && data)
    {
      gamepad[port].Counter = (gamepad[port].Counter + 2) & 6;
      gamepad[port].Timeout = 0;
    }
  }

  /* update TH input state */
  gamepad[port].State = data;
}


/*--------------------------------------------------------------------------*/
/*  Default ports handlers                                                  */
/*--------------------------------------------------------------------------*/

unsigned char gamepad_1_read(void)
{
  return gamepad_read(0);
}

unsigned char gamepad_2_read(void)
{
  return gamepad_read(4);
}

void gamepad_1_write(unsigned char data, unsigned char mask)
{
  gamepad_write(0, data, mask);
}

void gamepad_2_write(unsigned char data, unsigned char mask)
{
  gamepad_write(4, data, mask);
}

/*--------------------------------------------------------------------------*/
/*  4-WayPlay ports handler                                                 */
/*--------------------------------------------------------------------------*/

unsigned char wayplay_1_read(void)
{
  /* check if TH on port B is HIGH */
  if (latch & 0x04)
  {
    /* 4-WayPlay detection : xxxxx00 */
    return 0x7C;
  }

  /* TR & TL on port B select controller # (0-3) */
  return gamepad_read(latch);
}

unsigned char wayplay_2_read(void)
{
  return 0x7F;
}

void wayplay_1_write(unsigned char data, unsigned char mask)
{
  /* TR & TL on port B select controller # (0-3) */
  gamepad_write(latch & 0x03, data, mask);
}

void wayplay_2_write(unsigned char data, unsigned char mask)
{
  /* latch TH, TR & TL state on port B */
  latch = ((data & mask) >> 4) & 0x07;
}


/*--------------------------------------------------------------------------*/
/*  J-Cart memory handlers                                                  */
/*--------------------------------------------------------------------------*/

unsigned int jcart_read(unsigned int address)
{
   /* D6 returns TH state, D14 is fixed low (fixes Micro Machines 2) */
   return (gamepad_read(5) | ((gamepad_read(6) & 0x3F) << 8));
}

void jcart_write(unsigned int address, unsigned int data)
{
  data = (data & 0x01) << 6;
  gamepad_write(5, data, 0x40);
  gamepad_write(6, data, 0x40);
}


/*--------------------------------------------------------------------------*/
/*  Master Tap ports handler (unofficial, designed by Furrtek)              */
/*  cf. http://www.smspower.org/uploads/Homebrew/BOoM-SMS-sms4p_2.png       */
/*--------------------------------------------------------------------------*/
unsigned char mastertap_1_read(void)
{
  return gamepad_read(flipflop[0].Counter);
}

unsigned char mastertap_2_read(void)
{
  return gamepad_read(flipflop[1].Counter + 4);
}

void mastertap_1_write(unsigned char data, unsigned char mask)
{
  /* update bits set as output only */
  data = (flipflop[0].Latch & ~mask) | (data & mask);
  
  /* check TH 1->0 transitions */
  if ((flipflop[0].Latch & 0x40) && !(data & 0x40))
  {
    flipflop[0].Counter = (flipflop[0].Counter + 1) & 0x03;
  }

  /* update internal state */
  flipflop[0].Latch = data;
}

void mastertap_2_write(unsigned char data, unsigned char mask)
{
  /* update bits set as output only */
  data = (flipflop[1].Latch & ~mask) | (data & mask);
  
  /* check TH=1 to TH=0 transition */
  if ((flipflop[1].Latch & 0x40) && !(data & 0x40))
  {
    flipflop[1].Counter = (flipflop[1].Counter + 1) & 0x03;
  }

  /* update internal state */
  flipflop[1].Latch = data;
}
