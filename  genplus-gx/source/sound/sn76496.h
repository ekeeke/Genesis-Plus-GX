#ifndef SN76496_H
#define SN76496_H

#define MAX_76496 1

struct SN76496interface
{
  int num;
  int baseclock[MAX_76496];
  int volume[MAX_76496];
};

struct SN76496
{
  int SampleRate;
  unsigned int UpdateStep;
  int VolTable[16];
  int Register[8];
  int LastRegister;
  int Volume[4];
  unsigned int RNG;
  int NoiseFB;
  int Period[4];
  int Count[4];
  int Output[4];
};

extern struct SN76496 sn[MAX_76496];

/* Function prototypes */
void SN76496Write (int chip, int data);
void SN76496Update (int chip, signed short int *buffer, int length);
void SN76496_set_clock (int chip, int clock);
void SN76496_set_gain (int chip, int gain);
int SN76496_init (int chip, int clock, int volume, int sample_rate);
int SN76496_sh_start (int clock, int volume, int rate);

#endif
