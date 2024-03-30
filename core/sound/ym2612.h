/*
**
** software implementation of Yamaha FM sound generator (YM2612/YM3438)
**
** Original code (MAME fm.c)
**
** Copyright (C) 2001, 2002, 2003 Jarek Burczynski (bujar at mame dot net)
** Copyright (C) 1998 Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 1.4 (final beta) 
**
** Additional code & fixes by Eke-Eke for Genesis Plus GX
**
*/

#pragma once

#include <stdint.h>

enum {
  YM2612_DISCRETE = 0,
  YM2612_INTEGRATED,
  YM2612_ENHANCED
};


/* register number to channel number , slot offset */
#define OPN_CHAN(N) (N&3)
#define OPN_SLOT(N) ((N>>2)&3)

/* struct describing a single operator (SLOT) */
typedef struct
{
  int32_t   *DT;        /* detune          :dt_tab[DT]      */
  uint8_t   KSR;        /* key scale rate  :3-KSR           */
  uint32_t  ar;         /* attack rate                      */
  uint32_t  d1r;        /* decay rate                       */
  uint32_t  d2r;        /* sustain rate                     */
  uint32_t  rr;         /* release rate                     */
  uint8_t   ksr;        /* key scale rate  :kcode>>(3-KSR)  */
  uint32_t  mul;        /* multiple        :ML_TABLE[ML]    */

  /* Phase Generator */
  uint32_t  phase;      /* phase counter */
  int32_t   Incr;       /* phase step */

  /* Envelope Generator */
  uint8_t   state;      /* phase type */
  uint32_t  tl;         /* total level: TL << 3 */
  int32_t   volume;     /* envelope counter */
  uint32_t  sl;         /* sustain level:sl_table[SL] */
  uint32_t  vol_out;    /* current output from EG circuit (without AM from LFO) */

  uint8_t  eg_sh_ar;    /*  (attack state)  */
  uint8_t  eg_sel_ar;   /*  (attack state)  */
  uint8_t  eg_sh_d1r;   /*  (decay state)   */
  uint8_t  eg_sel_d1r;  /*  (decay state)   */
  uint8_t  eg_sh_d2r;   /*  (sustain state) */
  uint8_t  eg_sel_d2r;  /*  (sustain state) */
  uint8_t  eg_sh_rr;    /*  (release state) */
  uint8_t  eg_sel_rr;   /*  (release state) */

  uint8_t  ssg;         /* SSG-EG waveform  */
  uint8_t  ssgn;        /* SSG-EG negated output  */

  uint8_t  key;         /* 0=last key was KEY OFF, 1=KEY ON */

  /* LFO */
  uint32_t  AMmask;     /* AM enable flag */

} FM_SLOT;

typedef struct
{
  FM_SLOT  SLOT[4];     /* four SLOTs (operators) */

  uint8_t   ALGO;         /* algorithm */
  uint8_t   FB;           /* feedback shift */
  int32_t   op1_out[2];   /* op1 output for feedback */

  int32_t   *connect1;    /* SLOT1 output pointer */
  int32_t   *connect3;    /* SLOT3 output pointer */
  int32_t   *connect2;    /* SLOT2 output pointer */
  int32_t   *connect4;    /* SLOT4 output pointer */

  int32_t   *mem_connect; /* where to put the delayed sample (MEM) */
  int32_t   mem_value;    /* delayed sample (MEM) value */

  int32_t   pms;          /* channel PMS */
  uint8_t   ams;          /* channel AMS */

  uint32_t  fc;           /* fnum,blk */
  uint8_t   kcode;        /* key code */
  uint32_t  block_fnum;   /* blk/fnum value (for LFO PM calculations) */
} FM_CH;


typedef struct
{
  uint16_t  address;        /* address register     */
  uint8_t   status;         /* status flag          */
  uint32_t  mode;           /* mode  CSM / 3SLOT    */
  uint8_t   fn_h;           /* freq latch           */
  int32_t   TA;             /* timer a value        */
  int32_t   TAL;            /* timer a base         */
  int32_t   TAC;            /* timer a counter      */
  int32_t   TB;             /* timer b value        */
  int32_t   TBL;            /* timer b base         */
  int32_t   TBC;            /* timer b counter      */
  int32_t   dt_tab[8][32];  /* DeTune table         */

} FM_ST;


/***********************************************************/
/* OPN unit                                                */
/***********************************************************/

/* OPN 3slot struct */
typedef struct
{
  uint32_t  fc[3];          /* fnum3,blk3: calculated */
  uint8_t   fn_h;           /* freq3 latch */
  uint8_t   kcode[3];       /* key code */
  uint32_t  block_fnum[3];  /* current fnum value for this slot (can be different betweeen slots of one channel in 3slot mode) */
  uint8_t   key_csm;        /* CSM mode Key-ON flag */

} FM_3SLOT;

/* OPN/A/B common state */
typedef struct
{
  FM_ST  ST;                  /* general state */
  FM_3SLOT SL3;               /* 3 slot mode state */
  unsigned int pan[6*2];      /* fm channels output masks (0xffffffff = enable) */

  /* EG */
  uint32_t  eg_cnt;             /* global envelope generator counter */
  uint32_t  eg_timer;           /* global envelope generator counter works at frequency = chipclock/144/3 */

  /* LFO */
  uint8_t   lfo_cnt;            /* current LFO phase (out of 128) */
  uint32_t  lfo_timer;          /* current LFO phase runs at LFO frequency */
  uint32_t  lfo_timer_overflow; /* LFO timer overflows every N samples (depends on LFO frequency) */
  uint32_t  LFO_AM;             /* current LFO AM step */
  uint32_t  LFO_PM;             /* current LFO PM step */

} FM_OPN;

/***********************************************************/
/* YM2612 chip                                                */
/***********************************************************/
typedef struct
{
  FM_CH   CH[6];  /* channel state */
  uint8_t   dacen;  /* DAC mode  */
  int32_t   dacout; /* DAC output */
  FM_OPN  OPN;    /* OPN state */

} YM2612;

extern void YM2612Init(void);
extern void YM2612Config(int type);
extern void YM2612ResetChip(void);
extern void YM2612Update(int *buffer, int length);
extern void YM2612Write(unsigned int a, unsigned int v);
extern unsigned int YM2612Read(void);
extern int YM2612LoadContext(unsigned char *state);
extern int YM2612SaveContext(unsigned char *state);

