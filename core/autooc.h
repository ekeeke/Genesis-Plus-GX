/****************************************************************************
 *  autooc.h
 *
 *  Automatic 68k overclock, see autooc.c
 *
 *  Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX
 *  (see LICENSE.txt).
 ****************************************************************************/

#ifndef _AUTOOC_H_
#define _AUTOOC_H_

/* a taken branch 1 to this many bytes back may be a loop waiting for
   something (a flag set by the vertical interrupt, the VDP status) */
#define AUTOOC_SPIN_BYTES 32

#define AUTOOC_LEVELS 4 /* none, then three steps up to the chosen ratio */

#define AUTOOC_SAMPLE_LINE 112 /* autooc_sample is called at this line */

typedef struct
{
  int enabled;              /* the main 68k overclock is automatic */
  unsigned int spin_target; /* last short loop: where it goes back to, */
  unsigned int spin_regs[16]; /* the registers then */
  int spin_last;            /* and when (68k cycle count) */
  int idle;                 /* master cycles the 68k waited this frame */
  int sound_writes;         /* writes to the Z80 RAM this frame */
  int level;                /* level in use, 0 to AUTOOC_LEVELS - 1 */
  int busy;                 /* per mille of the last frame the 68k worked */
  float ratio[AUTOOC_LEVELS]; /* overclock ratio of each level */
} t_autooc;

extern t_autooc autooc;

/* Sets the main 68k overclock: automatic up to max_ratio, or max_ratio all
   the time. Turned on, the game starts again from no overclock; already on,
   the levels follow max_ratio. */
extern void autooc_apply(int automatic, float max_ratio);

/* after a game is loaded or reset, or a state loaded: no overclock until the
   game settles */
extern void autooc_reset(void);

/* end of a frame (cycles: its length in master cycles), before the cycle
   counts go back by it */
extern void autooc_end_frame(int cycles);

/* from the 68k core and the hardware */
extern void autooc_spin(unsigned int target);
extern void autooc_stop(int cycles);
extern void autooc_sound_write(void);
extern void autooc_sample(void);

#endif
