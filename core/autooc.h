/****************************************************************************
 *  autooc.h
 *
 *  Automatic 68k overclock (main 68k and Mega CD sub 68k), see autooc.c
 *
 *  Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX
 *  (see LICENSE.txt).
 ****************************************************************************/

#ifndef _AUTOOC_H_
#define _AUTOOC_H_

#include "m68k.h"

/* a taken branch 1 to this many bytes back may be a loop waiting for
   something (a flag set by an interrupt, the VDP status, a register of the
   other CPU) */
#define AUTOOC_SPIN_BYTES 32

#define AUTOOC_LEVELS 4 /* none, then three steps up to the chosen ratio */

#define AUTOOC_SAMPLE_LINE 112 /* autooc_sample is called at this line */

/* one 68k (cycle counts are those of its core: master clocks for the main
   68k, Mega CD clocks for the sub 68k) */
typedef struct
{
  int enabled;              /* its overclock is automatic */
  m68ki_cpu_core *cpu;
  int spin_turn_max;        /* cycles of one turn of a waiting loop at most */
  unsigned int spin_target; /* last short loop: where it goes back to, */
  unsigned int spin_regs[16]; /* the registers then */
  int spin_last;            /* and when */
  int idle;                 /* cycles it waited this frame */
  int idle_until;           /* stopped: counted as waiting up to this cycle */
  int level;                /* level in use, 0 to AUTOOC_LEVELS - 1 */
  int busy;                 /* per mille of the last frame it worked */
  float ratio[AUTOOC_LEVELS]; /* overclock ratio of each level */
  int slower[AUTOOC_LEVELS];  /* how much more a frame takes one level below */
  int hold;                 /* frames left at level 0 */
  int up_wait, down_wait, down_hold, since_down, roomy, waited_frames;
  unsigned int lag_history; /* one bit a frame, 1 = late */
} t_autooc;

extern t_autooc autooc;     /* main 68k */
extern t_autooc autooc_sub; /* Mega CD sub 68k */

/* Sets the overclock of one 68k: automatic up to max_ratio, or max_ratio all
   the time. Turned on, it starts again from no overclock; already on, the
   levels follow max_ratio. */
extern void autooc_apply(t_autooc *oc, int automatic, float max_ratio);

/* after a game is loaded or reset, or a state loaded: no overclock until the
   game settles (both 68k) */
extern void autooc_reset(void);

/* end of a frame (cycles: its length for that 68k), before its cycle count
   goes back by it */
extern void autooc_end_frame(t_autooc *oc, int cycles);

/* from the 68k cores */
extern void autooc_spin(t_autooc *oc, unsigned int target);
extern void autooc_stopped(t_autooc *oc, int from, int to); /* idle from..to */
extern void autooc_running(t_autooc *oc, int now);          /* runs from now */

/* from the hardware (both 68k) */
extern void autooc_sound_write(void);
extern void autooc_sample(void);

#endif
