
#ifndef _SYSTEM_H_
#define _SYSTEM_H_

typedef struct
{
  uint8 *data;			/* Bitmap data */
  int width;			/* Bitmap width (32+512+32) */
  int height;			/* Bitmap height (256) */
  int depth;			/* Color depth (8 bits) */
  int pitch;			/* Width of bitmap in bytes */
  int granularity;		/* Size of each pixel in bytes */
  int remap;			/* 1= Translate pixel data */
  struct
  {
    int x;			/* X offset of viewport within bitmap */
    int y;			/* Y offset of viewport within bitmap */
    int w;			/* Width of viewport */
    int h;			/* Height of viewport */
    int ow;			/* Previous width of viewport */
    int oh;			/* Previous height of viewport */
    int changed;		/* 1= Viewport width or height have changed */
  } viewport;
} t_bitmap;


typedef struct
{
  int sample_rate;		/* Sample rate (8000-44100) */
  int enabled;			/* 1= sound emulation is enabled */
  int buffer_size;		/* Size of sound buffer (in bytes) */
  int16 *buffer[2];		/* Signed 16-bit stereo sound data */
  struct
  {
    int curStage;
    int lastStage;
    int *gens_buffer[2];
    int16 *buffer[2];
  } fm;
  struct
  {
    int curStage;
    int lastStage;
    int16 *buffer;
  } psg;
} t_snd;

/* Global variables */
extern t_bitmap bitmap;
extern t_snd snd;
extern uint8 initialized;
extern uint32 aim_m68k;
extern uint32 count_m68k;
extern uint32 dma_m68k;
extern uint32 aim_z80;
extern uint32 count_z80;
extern uint16 misc68Kcycles;
extern uint16 miscZ80cycles;
extern uint16 lines_per_frame;
extern double Master_Clock;

/* Function prototypes */
void system_init (void);
void system_reset (void);
void system_shutdown (void);
int system_frame(int skip);
void m68k_run (int cyc);
void z80_run (int cyc);
void m68k_freeze(int cycles);
int audio_init (int rate);
void audio_update (void);

#endif /* _SYSTEM_H_ */

