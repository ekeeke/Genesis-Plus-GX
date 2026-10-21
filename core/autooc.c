/****************************************************************************
 *  autooc.c
 *
 *  Automatic 68k overclock
 *
 *  Each frame the time the main 68k spends waiting is measured: STOP, and
 *  short loops that keep branching back to the same place with the same
 *  registers (a game polling a flag or the VDP status until the next frame).
 *  A game that does not wait at all within a frame is running late, and when
 *  that happens again the overclock goes up one level; when the frames would
 *  still have time left at the level below, it goes down again. The levels go
 *  from no overclock to the ratio chosen in the menu.
 *
 *  The overclock is off while the display or the vertical interrupt is off,
 *  while the palette is all black and while lots of data goes to the Z80 RAM:
 *  screen changes and loading, where overclocked games tend to hang. Each
 *  game starts with it off, and it only goes up once the game was seen
 *  waiting (a game whose waiting is not recognised is left alone).
 *
 *  Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX
 *  (see LICENSE.txt).
 ****************************************************************************/

#include "shared.h"
#include "autooc.h"

t_autooc autooc;

/* one turn of a waiting loop takes less than this (master cycles); a longer
   turn ran something else (a routine called from the loop, DMA) */
#define SPIN_TURN_MAX     (100 * 7)

/* per mille of a frame the 68k worked */
#define LAG_BUSY          985   /* at least this: the frame was late */
#define WAIT_BUSY         950   /* at most this: the game waited */
#define DOWN_BUSY         850   /* at most this at the level below: go down */

#define CALIBRATE_FRAMES  30    /* frames the game must be seen waiting */
#define UP_WAIT           10    /* frames between two steps up */
#define DOWN_FRAMES       120   /* frames with time left before a step down */
#define DOWN_HOLD         180   /* frames without a step down after a step up, */
#define DOWN_HOLD_MAX     1800  /* doubled up to this when the level bounces */
#define BOUNCE_FRAMES     300   /* (a step up this soon after a step down) */

#define HOLD_START        180   /* frames off after a load, a reset or a state */
#define HOLD_SCREEN       30    /* frames off after the screen is back */
#define HOLD_SOUND        60    /* frames off after data went to the Z80 RAM */
#define SOUND_BURST       64    /* writes to the Z80 RAM in a frame */

/* how much more a frame takes one level below (per mille) */
static int slower[AUTOOC_LEVELS];

static int hold;                /* frames left at level 0 */
static int up_wait, down_wait, down_hold = DOWN_HOLD;
static int since_down = BOUNCE_FRAMES, roomy;
static int waited_frames;
static unsigned int lag_history; /* one bit a frame, 1 = late */

static void set_level(int level)
{
  autooc.level = level;
#ifdef M68K_OVERCLOCK_SHIFT
  m68k.cycle_ratio = (100 << M68K_OVERCLOCK_SHIFT) / (int)(autooc.ratio[level] * 100.0 + 0.5);
#endif
  lag_history = 0;
  roomy = 0;
}

static void calm(int frames)
{
  if (hold < frames)
  {
    hold = frames;
  }
  if (autooc.level != 0)
  {
    set_level(0);
  }
}

void autooc_reset(void)
{
  autooc.spin_target = 0xffffffff;
  autooc.idle = 0;
  autooc.sound_writes = 0;
  autooc.busy = 0;
  hold = HOLD_START;
  up_wait = down_wait = 0;
  down_hold = DOWN_HOLD;
  since_down = BOUNCE_FRAMES;
  waited_frames = 0;

  if (autooc.enabled)
  {
    set_level(0);
  }
  else
  {
    autooc.level = 0;
  }
}

void autooc_apply(int automatic, float max_ratio)
{
  int i;

  if (max_ratio < 1.0)
  {
    max_ratio = 1.0;
  }

  /* even steps (in ratio) from 1.0 to max_ratio */
  for (i = 0; i < AUTOOC_LEVELS; i++)
  {
    autooc.ratio[i] = (int)(powf(max_ratio, (float)i / (AUTOOC_LEVELS - 1)) * 100.0 + 0.5) / 100.0;
    slower[i] = i ? (int)(autooc.ratio[i] / autooc.ratio[i - 1] * 1100.0) : 1000;
  }

  if (automatic && max_ratio > 1.0)
  {
    if (autooc.enabled)
    {
      set_level(autooc.level);
    }
    else
    {
      autooc.enabled = 1;
      autooc_reset();
    }
    return;
  }

  autooc.enabled = 0;
  autooc.level = 0;
#ifdef M68K_OVERCLOCK_SHIFT
  m68k.cycle_ratio = (100 << M68K_OVERCLOCK_SHIFT) / (int)(max_ratio * 100.0);
#endif
}

void autooc_spin(unsigned int target)
{
  const int now = m68k.cycles;

  if ((target == autooc.spin_target) && !memcmp(autooc.spin_regs, m68k.dar, sizeof(autooc.spin_regs)))
  {
    int d = now - autooc.spin_last;
    if ((d > 0) && (d <= SPIN_TURN_MAX))
    {
      autooc.idle += d;
    }
  }
  else
  {
    autooc.spin_target = target;
    memcpy(autooc.spin_regs, m68k.dar, sizeof(autooc.spin_regs));
  }

  autooc.spin_last = now;
}

void autooc_stop(int cycles)
{
  if (cycles > 0)
  {
    autooc.idle += cycles;
  }
}

void autooc_sound_write(void)
{
  if (++autooc.sound_writes == SOUND_BURST)
  {
    calm(HOLD_SOUND);
  }
}

void autooc_sample(void)
{
  int i;

  /* display or vertical interrupt off: a screen change or loading */
  if (!(reg[1] & 0x40) || !(reg[1] & 0x20))
  {
    calm(HOLD_SCREEN);
    return;
  }

  /* all black palette: faded out */
  for (i = 0; i < 0x80; i += 4)
  {
    if (*(uint32 *)&cram[i])
    {
      return;
    }
  }
  calm(HOLD_SCREEN);
}

void autooc_end_frame(int frame)
{
  int idle = autooc.idle;
  int busy, late;

  autooc.idle = 0;
  autooc.sound_writes = 0;
  autooc.spin_last -= frame;

  if (frame <= 0)
  {
    return;
  }
  if (idle < 0)
  {
    idle = 0;
  }
  if (idle > frame)
  {
    idle = frame;
  }

  busy = 1000 - (int)((long long)idle * 1000 / frame);
  autooc.busy = busy;

  if (hold > 0)
  {
    hold--;
    lag_history = 0;
    roomy = 0;
    return;
  }

  late = (busy >= LAG_BUSY);
  if ((busy <= WAIT_BUSY) && (waited_frames < CALIBRATE_FRAMES))
  {
    waited_frames++;
  }

  lag_history = (lag_history << 1) | (late ? 1 : 0);
  if (up_wait > 0)
  {
    up_wait--;
  }
  if (down_wait > 0)
  {
    down_wait--;
  }
  if (since_down < BOUNCE_FRAMES)
  {
    since_down++;
  }

  /* late again: one level up */
  if (late && (waited_frames >= CALIBRATE_FRAMES) && (autooc.level < AUTOOC_LEVELS - 1) && (up_wait == 0))
  {
    int late_frames = 0;
    unsigned int bits;
    for (bits = lag_history & 0xff; bits; bits &= bits - 1)
    {
      late_frames++;
    }

    if (late_frames >= 2)
    {
      /* gone down too soon: stay up longer from now on */
      if (since_down < BOUNCE_FRAMES)
      {
        down_hold = (down_hold * 2 > DOWN_HOLD_MAX) ? DOWN_HOLD_MAX : down_hold * 2;
      }
      else
      {
        down_hold = DOWN_HOLD;
      }

      set_level(autooc.level + 1);
      up_wait = UP_WAIT;
      down_wait = down_hold;
      return;
    }
  }

  /* enough time left for the level below */
  if (autooc.level > 0)
  {
    if ((long long)busy * slower[autooc.level] <= (long long)DOWN_BUSY * 1000)
    {
      roomy++;
    }
    else
    {
      roomy = 0;
    }

    if ((roomy >= DOWN_FRAMES) && (down_wait == 0))
    {
      set_level(autooc.level - 1);
      since_down = 0;
    }
  }
}
