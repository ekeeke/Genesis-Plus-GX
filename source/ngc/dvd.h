/****************************************************************************
 * Nintendo Gamecube DVD Reading Library
 *
 * Low-Level DVD access
 *
 ***************************************************************************/

#ifndef _DVD_H_
#define _DVD_H_

extern u32 dvd_read (void *dst, u32 len, u64 offset);
extern void dvd_motor_off ();

#ifndef HW_RVL
extern void uselessinquiry ();
extern void dvd_drive_detect();
#endif

#endif
