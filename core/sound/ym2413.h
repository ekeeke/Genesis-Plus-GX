/*
**
** File: ym2413.c - software implementation of YM2413
**                  FM sound generator type OPLL
**
** Copyright (C) 2002 Jarek Burczynski
**
** Version 1.0
**
*/

#pragma once

typedef struct 
{
  uint32_t  ar;       /* attack rate: AR<<2           */
  uint32_t  dr;       /* decay rate:  DR<<2           */
  uint32_t  rr;       /* release rate:RR<<2           */
  uint8_t  KSR;       /* key scale rate               */
  uint8_t  ksl;       /* keyscale level               */
  uint8_t  ksr;       /* key scale rate: kcode>>KSR   */
  uint8_t  mul;       /* multiple: mul_tab[ML]        */

  /* Phase Generator */
  uint32_t phase;     /* frequency counter            */
  uint32_t freq;      /* frequency counter step       */
  uint8_t fb_shift;   /* feedback shift value         */
  int32_t op1_out[2]; /* slot1 output for feedback    */

  /* Envelope Generator */
  uint8_t  eg_type;   /* percussive/nonpercussive mode  */
  uint8_t  state;     /* phase type                     */
  uint32_t  TL;       /* total level: TL << 2           */
  int32_t  TLL;       /* adjusted now TL                */
  int32_t  volume;    /* envelope counter               */
  uint32_t  sl;       /* sustain level: sl_tab[SL]      */

  uint8_t  eg_sh_dp;  /* (dump state)                   */
  uint8_t  eg_sel_dp; /* (dump state)                   */
  uint8_t  eg_sh_ar;  /* (attack state)                 */
  uint16_t eg_sel_ar; /* (attack state)                 */
  uint8_t  eg_sh_dr;  /* (decay state)                  */
  uint8_t  eg_sel_dr; /* (decay state)                  */
  uint8_t  eg_sh_rr;  /* (release state for non-perc.)  */
  uint8_t  eg_sel_rr; /* (release state for non-perc.)  */
  uint8_t  eg_sh_rs;  /* (release state for perc.mode)  */
  uint8_t  eg_sel_rs; /* (release state for perc.mode)  */

  uint32_t  key;      /* 0 = KEY OFF, >0 = KEY ON */

  /* LFO */
  uint32_t  AMmask;   /* LFO Amplitude Modulation enable mask */
  uint8_t  vib;       /* LFO Phase Modulation enable flag (active high)*/

  /* waveform select */
  unsigned int wavetable;
} YM2413_OPLL_SLOT;

typedef struct 
{
  YM2413_OPLL_SLOT SLOT[2];

  /* phase generator state */
  uint32_t  block_fnum;   /* block+fnum */
  uint32_t  fc;           /* Freq. freqement base */
  uint32_t  ksl_base;     /* KeyScaleLevel Base step  */
  uint8_t   kcode;        /* key code (for key scaling) */
  uint8_t   sus;          /* sus on/off (release speed in percussive mode)  */
} YM2413_OPLL_CH;

/* chip state */
typedef struct {
    YM2413_OPLL_CH P_CH[9];   /* OPLL chips have 9 channels */
  uint8_t  instvol_r[9];        /* instrument/volume (or volume/volume in percussive mode)  */

  uint32_t  eg_cnt;             /* global envelope generator counter  */
  uint32_t  eg_timer;           /* global envelope generator counter works at frequency = chipclock/72 */
  uint32_t  eg_timer_add;       /* step of eg_timer */
  uint32_t  eg_timer_overflow;  /* envelope generator timer overlfows every 1 sample (on real chip) */

  uint8_t  rhythm;              /* Rhythm mode  */

  /* LFO */
  uint32_t  lfo_am_cnt;
  uint32_t  lfo_am_inc;
  uint32_t  lfo_pm_cnt;
  uint32_t  lfo_pm_inc;

  uint32_t  noise_rng;      /* 23 bit noise shift register  */
  uint32_t  noise_p;        /* current noise 'phase'  */
  uint32_t  noise_f;        /* current noise period */


/* instrument settings */
/*
  0-user instrument
  1-15 - fixed instruments
  16 -bass drum settings
  17,18 - other percussion instruments
*/
  uint8_t inst_tab[19][8];

  uint32_t  fn_tab[1024];     /* fnumber->increment counter  */

  uint8_t address;          /* address register */
  uint8_t status;          /* status flag       */

  double clock;         /* master clock  (Hz) */
  int rate;            /* sampling rate (Hz)  */
} YM2413;

extern void YM2413Init(void);
extern void YM2413ResetChip(void);
extern void YM2413Update(int *buffer, int length);
extern void YM2413Write(unsigned int a, unsigned int v);
extern unsigned int YM2413Read(void);
extern unsigned char *YM2413GetContextPtr(void);
extern unsigned int YM2413GetContextSize(void);

