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
*/

#ifndef _SN76489_H_
#define _SN76489_H_

/* Function prototypes */
extern void SN76489_Init(int PSGClockValue, int SamplingRate);
extern void SN76489_Reset(void);
extern void SN76489_Shutdown(void);
extern void SN76489_SetContext(uint8 *data);
extern void SN76489_GetContext(uint8 *data);
extern uint8 *SN76489_GetContextPtr(void);
extern int SN76489_GetContextSize(void);
extern void SN76489_Write(int data);
extern void SN76489_Update(INT16 *buffer, int length);
extern void SN76489_BoostNoise(int boost);
extern void SN76489_Config(int mute, int volume, int feedback, int sr_width, int boost_noise);

#endif /* _SN76489_H_ */

