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

   25/04/07 Eke-Eke
   Modified for use with GenesisPlus Gamecube's port:
   - made SN76489_Update outputs 16bits mono samples
   - replaced volume table with VGM plugin's one
*/

#ifndef _SN76489_H_
#define _SN76489_H_

#define MAX_SN76489     1

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
extern void SN76489_Init(int which, int PSGClockValue, int SamplingRate);
extern void SN76489_Reset(int which);
extern void SN76489_Config(int which, int mute, int volume, int feedback, int sw_width, int boost_noise);
extern void SN76489_SetContext(int which, uint8 *data);
extern void SN76489_GetContext(int which, uint8 *data);
extern uint8 *SN76489_GetContextPtr(int which);
extern int SN76489_GetContextSize(void);
extern void SN76489_Write(int which, int data);
extern void SN76489_Update(int which, INT16 *buffer, int length);

#endif /* _SN76489_H_ */

