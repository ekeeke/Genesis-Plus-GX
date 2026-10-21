/****************************************************************************
 *  autooc.c
 *
 *  Automatic 68k overclock (main 68k and Mega CD sub 68k)
 *
 *  Each frame the time a 68k spends waiting is measured: STOP, the polling
 *  of the other CPU's registers that the core already detects (Mega CD), and
 *  short loops that keep branching back to the same place with the same
 *  registers (a game polling a flag or the VDP status until the next frame).
 *  A 68k that does not wait at all within a frame is running late, and when
 *  that happens again its overclock goes up one level; when the frames would
 *  still have time left at the level below, it goes down again. The levels go
 *  from no overclock to the ratio chosen in the menu for that 68k.
 *
 *  The overclock is off while the display or the vertical interrupt is off,
 *  while the palette is all black and while lots of data goes to the Z80 RAM:
 *  screen changes and loading, where overclocked games tend to hang. Each
 *  game starts with it off, and it only goes up once the 68k was seen waiting
 *  (a 68k whose waiting is not recognised is left alone).
 *
 *  Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX
 *  (see LICENSE.txt).
 ****************************************************************************/

#include "shared.h"
#include "autooc.h"

/* per mille of a frame a 68k worked */
#define LAG_BUSY          985   /* at least this: the frame was late */
#define WAIT_BUSY         950   /* at most this: the game waited */
#define DOWN_BUSY         850   /* at most this at the level below: go down */

#define CALIBRATE_FRAMES  30    /* frames the 68k must be seen waiting */
#define UP_WAIT           10    /* frames between two steps up */
#define DOWN_FRAMES       120   /* frames with time left before a step down */
#define DOWN_HOLD         180   /* frames without a step down after a step up, */
#define DOWN_HOLD_MAX     1800  /* doubled up to this when the level bounces */
#define BOUNCE_FRAMES     300   /* (a step up this soon after a step down) */

#define HOLD_START        180   /* frames off after a load, a reset or a state */
#define HOLD_SCREEN       30    /* frames off after the screen is back */
#define HOLD_SOUND        60    /* frames off after data went to the Z80 RAM */
#define SOUND_BURST       64    /* writes to the Z80 RAM in a frame */

/* one turn of a waiting loop takes less than 100 CPU cycles; a longer turn
   ran something else (a routine called from the loop, DMA) */
t_autooc autooc     = { .cpu = &m68k, .spin_turn_max = 100 * 7, .down_hold = DOWN_HOLD };
t_autooc autooc_sub = { .cpu = &s68k, .spin_turn_max = 100 * 4, .down_hold = DOWN_HOLD };

static int sound_writes; /* writes to the Z80 RAM this frame */

static void set_level(t_autooc *oc, int level)
{
  oc->level = level;
#ifdef M68K_OVERCLOCK_SHIFT
  oc->cpu->cycle_ratio = (100 << M68K_OVERCLOCK_SHIFT) / (int)(oc->ratio[level] * 100.0 + 0.5);
#endif
  oc->lag_history = 0;
  oc->roomy = 0;
}

static void calm(t_autooc *oc, int frames)
{
  if (!oc->enabled)
  {
    return;
  }
  if (oc->hold < frames)
  {
    oc->hold = frames;
  }
  if (oc->level != 0)
  {
    set_level(oc, 0);
  }
}

static void calm_all(int frames)
{
  calm(&autooc, frames);
  calm(&autooc_sub, frames);
}

static void reset_one(t_autooc *oc)
{
  oc->spin_target = 0xffffffff;
  oc->idle = 0;
  oc->idle_until = 0;
  oc->busy = 0;
  oc->hold = HOLD_START;
  oc->up_wait = oc->down_wait = 0;
  oc->down_hold = DOWN_HOLD;
  oc->since_down = BOUNCE_FRAMES;
  oc->waited_frames = 0;

  if (oc->enabled)
  {
    set_level(oc, 0);
  }
  else
  {
    oc->level = 0;
  }
}

void autooc_reset(void)
{
  sound_writes = 0;
  reset_one(&autooc);
  reset_one(&autooc_sub);
}

void autooc_apply(t_autooc *oc, int automatic, float max_ratio)
{
  int i;

  if (max_ratio < 1.0)
  {
    max_ratio = 1.0;
  }

  /* even steps (in ratio) from 1.0 to max_ratio */
  for (i = 0; i < AUTOOC_LEVELS; i++)
  {
    oc->ratio[i] = (int)(powf(max_ratio, (float)i / (AUTOOC_LEVELS - 1)) * 100.0 + 0.5) / 100.0;
    oc->slower[i] = i ? (int)(oc->ratio[i] / oc->ratio[i - 1] * 1100.0) : 1000;
  }

  if (automatic && max_ratio > 1.0)
  {
    if (oc->enabled)
    {
      set_level(oc, oc->level);
    }
    else
    {
      oc->enabled = 1;
      reset_one(oc);
    }
    return;
  }

  oc->enabled = 0;
  oc->level = 0;
#ifdef M68K_OVERCLOCK_SHIFT
  oc->cpu->cycle_ratio = (100 << M68K_OVERCLOCK_SHIFT) / (int)(max_ratio * 100.0);
#endif
}

void autooc_spin(t_autooc *oc, unsigned int target)
{
  const int now = oc->cpu->cycles;

  if ((target == oc->spin_target) && !memcmp(oc->spin_regs, oc->cpu->dar, sizeof(oc->spin_regs)))
  {
    int d = now - oc->spin_last;
    if ((d > 0) && (d <= oc->spin_turn_max))
    {
      oc->idle += d;
    }
  }
  else
  {
    oc->spin_target = target;
    memcpy(oc->spin_regs, oc->cpu->dar, sizeof(oc->spin_regs));
  }

  oc->spin_last = now;
}

void autooc_stopped(t_autooc *oc, int from, int to)
{
  /* not twice the same cycles */
  if (from < oc->idle_until)
  {
    from = oc->idle_until;
  }
  if (to > from)
  {
    oc->idle += to - from;
    oc->idle_until = to;
  }
}

void autooc_running(t_autooc *oc, int now)
{
  /* restarted earlier than it was counted as waiting (polling of the other
     CPU's registers ends when that CPU writes them) */
  if (now < oc->idle_until)
  {
    oc->idle -= oc->idle_until - now;
    oc->idle_until = now;
  }
}

void autooc_sound_write(void)
{
  if (++sound_writes == SOUND_BURST)
  {
    calm_all(HOLD_SOUND);
  }
}

void autooc_sample(void)
{
  int i;

  /* display or vertical interrupt off: a screen change or loading */
  if (!(reg[1] & 0x40) || !(reg[1] & 0x20))
  {
    calm_all(HOLD_SCREEN);
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
  calm_all(HOLD_SCREEN);
}

void autooc_end_frame(t_autooc *oc, int frame)
{
  int idle = oc->idle;
  int busy, late;

  oc->idle = 0;
  oc->spin_last -= frame;
  oc->idle_until -= frame;
  if (oc == &autooc)
  {
    sound_writes = 0;
  }

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
  oc->busy = busy;

  if (oc->hold > 0)
  {
    oc->hold--;
    oc->lag_history = 0;
    oc->roomy = 0;
    return;
  }

  late = (busy >= LAG_BUSY);
  if ((busy <= WAIT_BUSY) && (oc->waited_frames < CALIBRATE_FRAMES))
  {
    oc->waited_frames++;
  }

  oc->lag_history = (oc->lag_history << 1) | (late ? 1 : 0);
  if (oc->up_wait > 0)
  {
    oc->up_wait--;
  }
  if (oc->down_wait > 0)
  {
    oc->down_wait--;
  }
  if (oc->since_down < BOUNCE_FRAMES)
  {
    oc->since_down++;
  }

  /* late again: one level up */
  if (late && (oc->waited_frames >= CALIBRATE_FRAMES) && (oc->level < AUTOOC_LEVELS - 1) && (oc->up_wait == 0))
  {
    int late_frames = 0;
    unsigned int bits;
    for (bits = oc->lag_history & 0xff; bits; bits &= bits - 1)
    {
      late_frames++;
    }

    if (late_frames >= 2)
    {
      /* gone down too soon: stay up longer from now on */
      if (oc->since_down < BOUNCE_FRAMES)
      {
        oc->down_hold = (oc->down_hold * 2 > DOWN_HOLD_MAX) ? DOWN_HOLD_MAX : oc->down_hold * 2;
      }
      else
      {
        oc->down_hold = DOWN_HOLD;
      }

      set_level(oc, oc->level + 1);
      oc->up_wait = UP_WAIT;
      oc->down_wait = oc->down_hold;
      return;
    }
  }

  /* enough time left for the level below */
  if (oc->level > 0)
  {
    if ((long long)busy * oc->slower[oc->level] <= (long long)DOWN_BUSY * 1000)
    {
      oc->roomy++;
    }
    else
    {
      oc->roomy = 0;
    }

    if ((oc->roomy >= DOWN_FRAMES) && (oc->down_wait == 0))
    {
      set_level(oc, oc->level - 1);
      oc->since_down = 0;
    }
  }
}
