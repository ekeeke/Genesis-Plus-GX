/* 
    SN76489 emulation
    by Maxim in 2001 and 2002
    converted from my original Delphi implementation

    I'm a C newbie so I'm sure there are loads of stupid things
    in here which I'll come back to some day and redo

    Includes:
    - Super-high quality tone channel "oversampling" by calculating fractional positions on transitions
    - Noise output pattern reverse engineered from actual SMS output
    - Volume levels taken from actual SMS output

    07/08/04  Charles MacDonald
    Modified for use with SMS Plus:
    - Added support for multiple PSG chips.
    - Added reset/config/update routines.
    - Added context management routines.
    - Removed SN76489_GetValues().
    - Removed some unused variables.

   25/04/07 Eke-Eke (Genesis Plus GX)
    - Removed stereo GG support (unused)
    - Made SN76489_Update outputs 16bits mono samples
    - Replaced volume table with VGM plugin's one
   
    05/01/09 Eke-Eke (Genesis Plus GX)
    - Modified Cut-Off frequency (according to Steve Snake: http://www.smspower.org/forums/viewtopic.php?t=1746)

    25/05/09 Eke-Eke (Genesis Plus GX)
    - Removed multichip support (unused)
*/

#include "shared.h"

#include <float.h> // for FLT_MIN
#include <string.h> // for memcpy

#define NoiseInitialState   0x8000  /* Initial state of shift register */
//#define PSG_CUTOFF          0x6     /* Value below which PSG does not output */
#define PSG_CUTOFF          0x1

/*
    More testing is needed to find and confirm feedback patterns for
    SN76489 variants and compatible chips.
*/
enum feedback_patterns {
    FB_BBCMICRO =   0x8005, /* Texas Instruments TMS SN76489N (original) from BBC Micro computer */
    FB_SC3000   =   0x0006, /* Texas Instruments TMS SN76489AN (rev. A) from SC-3000H computer */
    FB_SEGAVDP  =   0x0009, /* SN76489 clone in Sega's VDP chips (315-5124, 315-5246, 315-5313, Game Gear) */
};

enum sr_widths {
  SRW_SC3000BBCMICRO  = 15,
  SRW_SEGAVDP = 16
};

enum volume_modes {
    VOL_TRUNC   =   0,      /* Volume levels 13-15 are identical */
    VOL_FULL    =   1,      /* Volume levels 13-15 are unique */
};

enum mute_values {
    MUTE_ALLOFF =   0,      /* All channels muted */
    MUTE_TONE1  =   1,      /* Tone 1 mute control */
    MUTE_TONE2  =   2,      /* Tone 2 mute control */
    MUTE_TONE3  =   4,      /* Tone 3 mute control */
    MUTE_NOISE  =   8,      /* Noise mute control */
    MUTE_ALLON  =   15,     /* All channels enabled */
};

typedef struct
{
    int Mute; // per-channel muting
    int VolumeArray;
    int BoostNoise; // double noise volume when non-zero
    
    /* Variables */
    float Clock;
    float dClock;
    int PSGStereo;
    int NumClocksForSample;
    int WhiteNoiseFeedback;
    int SRWidth;
    
    /* PSG registers: */
    int Registers[8];        /* Tone, vol x4 */
    int LatchedRegister;
    int NoiseShiftRegister;
    int NoiseFreq;            /* Noise channel signal generator frequency */
    
    /* Output calculation variables */
    int ToneFreqVals[4];      /* Frequency register values (counters) */
    int ToneFreqPos[4];        /* Frequency channel flip-flops */
    int Channels[4];          /* Value of each channel, before stereo is applied */
    float IntermediatePos[4];   /* intermediate values used at boundaries between + and - (does not need double accuracy)*/

    int panning[4];            /* fake stereo - 0..127..254 */

} SN76489_Context;


static const int PSGVolumeValues[2][16] = {
  /* These values are taken from a real SMS2's output */
    {892,892,892,760,623,497,404,323,257,198,159,123,96,75,60,0}, /* I can't remember why 892... :P some scaling I did at some point */
  /* these values are true volumes for 2dB drops at each step (multiply previous by 10^-0.1), normalised at 760 */
  {1516,1205,957,760,603,479,381,303,240,191,152,120,96,76,60,0}
};

static SN76489_Context SN76489;

void SN76489_Init(float PSGClockValue, int SamplingRate)
{
    SN76489_Context *p = &SN76489;
    p->dClock=PSGClockValue/16.0/SamplingRate;
    SN76489_Config(MUTE_ALLON, VOL_FULL, FB_SEGAVDP, SRW_SEGAVDP, config.psgBoostNoise);
    SN76489_Reset();
}

void SN76489_Reset(void)
{
    SN76489_Context *p = &SN76489;
    int i;

    p->PSGStereo = 0xFF;

    for(i = 0; i <= 3; i++)
    {
        /* Initialise PSG state */
        p->Registers[2*i] = 1;         /* tone freq=1 */
        p->Registers[2*i+1] = 0xf;     /* vol=off */
        p->NoiseFreq = 0x10;

        /* Set counters to 0 */
        p->ToneFreqVals[i] = 0;

        /* Set flip-flops to 1 */
        p->ToneFreqPos[i] = 1;

        /* Set intermediate positions to do-not-use value */
        p->IntermediatePos[i] = FLT_MIN;

        /* Set panning to centre */
        p->panning[0]=127;
    }

    p->LatchedRegister=0;

    /* Initialise noise generator */
    p->NoiseShiftRegister=NoiseInitialState;

    /* Zero clock */
    p->Clock=0;

}

void SN76489_Shutdown(void)
{
}

void SN76489_BoostNoise(int boost)
{
  SN76489.BoostNoise = boost;
}

void SN76489_Config(int mute, int volume, int feedback, int sr_width, int boost_noise)
{
    SN76489_Context *p = &SN76489;

    p->Mute = mute;
    p->VolumeArray = volume;
    p->WhiteNoiseFeedback = feedback;
    p->SRWidth = sr_width;
    p->BoostNoise = boost_noise;
}

void SN76489_SetContext(uint8 *data)
{
    memcpy(&SN76489, data, sizeof(SN76489_Context));
}

void SN76489_GetContext(uint8 *data)
{
    memcpy(data, &SN76489, sizeof(SN76489_Context));
}

uint8 *SN76489_GetContextPtr(void)
{
    return (uint8 *)&SN76489;
}

int SN76489_GetContextSize(void)
{
    return sizeof(SN76489_Context);
}

void SN76489_Write(int data)
{
    SN76489_Context *p = &SN76489;

  if (data&0x80) {
        /* Latch/data byte  %1 cc t dddd */
        p->LatchedRegister=((data>>4)&0x07);
        p->Registers[p->LatchedRegister]=
            (p->Registers[p->LatchedRegister] & 0x3f0)    /* zero low 4 bits */
            | (data&0xf);                           /* and replace with data */
  } else {
        /* Data byte        %0 - dddddd */
        if (!(p->LatchedRegister%2)&&(p->LatchedRegister<5))
            /* Tone register */
            p->Registers[p->LatchedRegister]=
                (p->Registers[p->LatchedRegister] & 0x00f)    /* zero high 6 bits */
                | ((data&0x3f)<<4);                     /* and replace with data */
    else
            /* Other register */
            p->Registers[p->LatchedRegister]=data&0x0f;       /* Replace with data */
    }
    switch (p->LatchedRegister) {
  case 0:
  case 2:
    case 4: /* Tone channels */
        if (p->Registers[p->LatchedRegister]==0) p->Registers[p->LatchedRegister]=1;    /* Zero frequency changed to 1 to avoid div/0 */
    break;
    case 6: /* Noise */
        p->NoiseShiftRegister=NoiseInitialState;   /* reset shift register */
        p->NoiseFreq=0x10<<(p->Registers[6]&0x3);     /* set noise signal generator frequency */
    break;
    }
}

void SN76489_GGStereoWrite(int data)
{
    SN76489_Context *p = &SN76489;
    p->PSGStereo=data;
}

void SN76489_Update(INT16 *buffer, int length)
{
    SN76489_Context *p = &SN76489;
    int i, j;

    for(j = 0; j < length; j++)
    {
        for (i=0;i<=2;++i)
            if (p->IntermediatePos[i]!=FLT_MIN)
                p->Channels[i]=(short)((p->Mute >> i & 0x1)*PSGVolumeValues[p->VolumeArray][p->Registers[2*i+1]]*p->IntermediatePos[i]);
            else
                p->Channels[i]=(p->Mute >> i & 0x1)*PSGVolumeValues[p->VolumeArray][p->Registers[2*i+1]]*p->ToneFreqPos[i];
    
        p->Channels[3]=(short)((p->Mute >> 3 & 0x1)*PSGVolumeValues[p->VolumeArray][p->Registers[7]]*(p->NoiseShiftRegister & 0x1));
    
        if (p->BoostNoise) p->Channels[3]<<=1; /* double noise volume */
    
        buffer[j] =0;
        for (i=0;i<=3;++i) buffer[j] += p->Channels[i];
    
        p->Clock+=p->dClock;
        p->NumClocksForSample=(int)p->Clock;  /* truncates */
        p->Clock-=p->NumClocksForSample;  /* remove integer part */
        /* Looks nicer in Delphi... */
        /*  Clock:=Clock+p->dClock; */
        /*  NumClocksForSample:=Trunc(Clock); */
        /*  Clock:=Frac(Clock); */
    
        /* Decrement tone channel counters */
        for (i=0;i<=2;++i)
            p->ToneFreqVals[i]-=p->NumClocksForSample;
     
        /* Noise channel: match to tone2 or decrement its counter */
        if (p->NoiseFreq==0x80) p->ToneFreqVals[3]=p->ToneFreqVals[2];
        else p->ToneFreqVals[3]-=p->NumClocksForSample;
    
        /* Tone channels: */
        for (i=0;i<=2;++i) {
            if (p->ToneFreqVals[i]<=0) {   /* If it gets below 0... */
                if (p->Registers[i*2]>PSG_CUTOFF) {
                    /* Calculate how much of the sample is + and how much is - */
                    /* Go to floating point and include the clock fraction for extreme accuracy :D */
                    /* Store as long int, maybe it's faster? I'm not very good at this */
                    p->IntermediatePos[i]=(p->NumClocksForSample-p->Clock+2*p->ToneFreqVals[i])*p->ToneFreqPos[i]/(p->NumClocksForSample+p->Clock);
                    p->ToneFreqPos[i]=-p->ToneFreqPos[i]; /* Flip the flip-flop */
                } else {
                    p->ToneFreqPos[i]=1;   /* stuck value */
                    p->IntermediatePos[i]=FLT_MIN;
                }
                p->ToneFreqVals[i]+=p->Registers[i*2]*(p->NumClocksForSample/p->Registers[i*2]+1);
            } else p->IntermediatePos[i]=FLT_MIN;
        }
    
        /* Noise channel */
        if (p->ToneFreqVals[3]<=0) {   /* If it gets below 0... */
            p->ToneFreqPos[3]=-p->ToneFreqPos[3]; /* Flip the flip-flop */
            if (p->NoiseFreq!=0x80)            /* If not matching tone2, decrement counter */
                p->ToneFreqVals[3]+=p->NoiseFreq*(p->NumClocksForSample/p->NoiseFreq+1);
            if (p->ToneFreqPos[3]==1) {    /* Only once per cycle... */
                int Feedback;
                if (p->Registers[6]&0x4) { /* White noise */
                    /* Calculate parity of fed-back bits for feedback */
                    switch (p->WhiteNoiseFeedback) {
                        /* Do some optimised calculations for common (known) feedback values */
                    case 0x0003:    /* SC-3000, BBC %00000011 */
                    case 0x0009:    /* SMS, GG, MD  %00001001 */
                        /* If two bits fed back, I can do Feedback=(nsr & fb) && (nsr & fb ^ fb) */
                        /* since that's (one or more bits set) && (not all bits set) */
                        Feedback=((p->NoiseShiftRegister&p->WhiteNoiseFeedback) && ((p->NoiseShiftRegister&p->WhiteNoiseFeedback)^p->WhiteNoiseFeedback));
                        break;
                    default:        /* Default handler for all other feedback values */
                        Feedback=p->NoiseShiftRegister&p->WhiteNoiseFeedback;
                        Feedback^=Feedback>>8;
                        Feedback^=Feedback>>4;
                        Feedback^=Feedback>>2;
                        Feedback^=Feedback>>1;
                        Feedback&=1;
                        break;
                    }
                } else      /* Periodic noise */
                    Feedback=p->NoiseShiftRegister&1;
    
                p->NoiseShiftRegister=(p->NoiseShiftRegister>>1) | (Feedback << (p->SRWidth-1));
    
    /* Original code: */
    /*          p->NoiseShiftRegister=(p->NoiseShiftRegister>>1) | ((p->Registers[6]&0x4?((p->NoiseShiftRegister&0x9) && (p->NoiseShiftRegister&0x9^0x9)):p->NoiseShiftRegister&1)<<15); */
            }
        }
    }
}

/*void SN76489_UpdateOne(int which, int *l, int *r)
{
  INT16 tl,tr;
  INT16 *buff[2]={&tl,&tr};
  SN76489_Update(which,buff,1);
  *l=tl;
  *r=tr;
}*/

int SN76489_GetMute()
{
  return SN76489.Mute;
}

void SN76489_SetMute(int val)
{
  SN76489.Mute=val;
}

int SN76489_GetVolType()
{
  return SN76489.VolumeArray;
}

void SN76489_SetVolType(int val)
{
  SN76489.VolumeArray=val;
}

void SN76489_SetPanning(int ch0, int ch1, int ch2, int ch3)
{
  SN76489.panning[0]=ch0;
  SN76489.panning[1]=ch1;
  SN76489.panning[2]=ch2;
  SN76489.panning[3]=ch3;
}
