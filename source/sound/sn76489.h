
#ifndef _SN76489_H_
#define _SN76489_H_

#define MAX_SN76489     4
#include "shared.h"
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

/* Function prototypes */
void SN76489_Init(int which, int PSGClockValue, int SamplingRate);
void SN76489_Reset(int which);
void SN76489_Shutdown(void);
void SN76489_Config(int which, int mute, int volume, int feedback, int sw_width, int boost_noise);
void SN76489_SetContext(int which, uint8 *data);
void SN76489_GetContext(int which, uint8 *data);
uint8 *SN76489_GetContextPtr(int which);
int SN76489_GetContextSize(void);
void SN76489_Write(int which, int data);
/*void SN76489_GGStereoWrite(int which, int data);*/
void SN76489_Update(int which, INT16 *buffer, int length);

/* Non-standard getters and setters */
int  SN76489_GetMute(int which);
void SN76489_SetMute(int which, int val);
int  SN76489_GetVolType(int which);
void SN76489_SetVolType(int which, int val);

void SN76489_SetPanning(int which, int ch0, int ch1, int ch2, int ch3);

/* and a non-standard data getter */
/*void SN76489_UpdateOne(int which, int *l, int *r);*/

#endif /* _SN76489_H_ */

