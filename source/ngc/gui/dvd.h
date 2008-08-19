/****************************************************************************
 * Nintendo Gamecube DVD Reading Library
 *
 * This is NOT a complete DVD library, in that it works for reading 
 * ISO9660 discs only.
 *
 * If you need softmod drivecodes etc, look elsewhere.
 * There are known issues with libogc dvd handling, so these work
 * outside of it ,if you will.
 *
 * This is ideal for using with a gc-linux self booting DVD only.
 * Go http://www.gc-linux.org for further information and the tools
 * for your platform.
 *
 * To keep libOGC stable, make sure you call DVD_Init before using
 * these functions.
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
