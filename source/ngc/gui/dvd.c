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

#include "shared.h"
#ifdef WII_DVD
#include "wdvd.h"
#endif
#include "dvd.h"

/** DVD I/O Address base **/
#ifndef HW_RVL
static vu32* const dvd = (u32*)0xCC006000;
static unsigned char *inquiry=(unsigned char *)0x80000004;
#endif

/** Due to lack of memory, we'll use this little 2k keyhole for all DVD operations **/
unsigned char DVDreadbuffer[2048] ATTRIBUTE_ALIGN (32);

#ifdef HW_RVL
u64 DvdMaxOffset = 0x118244F00LL;
#else
u64 DvdMaxOffset = 0x57057C00; // default
#endif

/***************************************************************************
 * dvd_read
 *
 * Read DVD disc sectors
 ***************************************************************************/
int dvd_read (void *dst, unsigned int len, u64 offset)
{
#ifndef HW_RVL
  //unsigned char *buffer = (unsigned char *) (unsigned int) DVDreadbuffer;
  
 // if (len > 2048) return 1; /*** We only allow 2k reads **/
 /* DCInvalidateRange((void *)buffer, len);

  if(offset < DvdMaxOffset)
  {
      dvd[0] = 0x2E;
      dvd[1] = 0;
      dvd[2] = 0xA8000000;
      dvd[3] = (u32)(offset >> 2);
      dvd[4] = len;
      dvd[5] = (u32) buffer;
      dvd[6] = len;
      dvd[7] = 3; *//*** Enable reading with DMA ***/
   //   while (dvd[7] & 1);
    //  memcpy (dst, buffer, len);
 // }
 // else return 1; // Let's not read past end of DVD
   
//  if (dvd[0] & 0x4) return 0; /* Ensure it has completed */
  if (DVD_LowRead(dst,len,offset,NULL) == 1)
    return 0;
#elif WII_DVD

  if (WDVD_LowUnencryptedRead((unsigned char **)&dst, len, (u32)(offset >> 2)) == 1)
    return 0;
#endif
 
  return 1;
}

/****************************************************************************
 * uselessinquiry
 *
 * As the name suggests, this function is quite useless.
 * It's only purpose is to stop any pending DVD interrupts while we use the
 * memcard interface.
 *
 * libOGC tends to foul up if you don't, and sometimes does if you do!
 ****************************************************************************/
#ifndef HW_RVL
void uselessinquiry ()
{
  dvddrvinfo drive_info;

  DVD_LowInquiry(&drive_info,NULL);

/*
  dvd[0] = 0;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0;
  dvd[4] = 0x20;
  dvd[5] = 0x80000000;
  dvd[6] = 0x20;
  dvd[7] = 1;

  while (dvd[7] & 1);*/
}
#endif

/****************************************************************************
 * dvd_motor_off
 *
 * Stop the DVD Motor
 *
 * This can be used to prevent the Disc from spinning during playtime
 ****************************************************************************/
#ifndef HW_RVL
void dvd_motor_off( )
{

  DVD_LowStopMotor(NULL);

  /*
	dvd[0] = 0x2e;
	dvd[1] = 0;
	dvd[2] = 0xe3000000;
	dvd[3] = 0;
	dvd[4] = 0;
	dvd[5] = 0;
	dvd[6] = 0;
	dvd[7] = 1; // Do immediate
	while (dvd[7] & 1);*/

	/*** PSO Stops blackscreen at reload ***/
/*	dvd[0] = 0x14;
	dvd[1] = 0;*/
}
#endif

/****************************************************************************
 * dvd_drive_detect()
 *
 * Detect the DVD Drive Type
 *
 ****************************************************************************/
#ifndef HW_RVL
void dvd_drive_detect()
{
  /*dvd[0] = 0x2e;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0;
  dvd[4] = 0x20;
  dvd[5] = 0x80000000;
  dvd[6] = 0x20;
  dvd[7] = 3;
  while( dvd[7] & 1 );
  DCFlushRange((void *)0x80000000, 32);*/

  dvddrvinfo drive_info;

  DVD_LowInquiry(&drive_info,NULL);

  //int driveid = (int)inquiry[2];
  int driveid = drive_info.dev_code;
  
  if ((driveid == 4) || (driveid == 6) || (driveid == 8))
  {
    /* Gamecube DVD Drive (1.4 GB)*/
    DvdMaxOffset = 0x57057C00;
  }
  else 
  {
    /* Wii DVD Drive (4.7GB) */
    DvdMaxOffset = 0x118244F00LL;
  }
}
#endif
