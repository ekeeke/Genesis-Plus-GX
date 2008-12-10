/****************************************************************************
 *
 * DVD ISO9660/Joliet loading support
 *
 ***************************************************************************/
#ifndef _FILEIO_DVD_H
#define _FILEIO_DVD_H

extern int DVD_UpdateDir(int go_up);
extern int DVD_ParseDirectory();
extern int DVD_LoadFile(unsigned char* buffer);
extern int DVD_Open ();

#endif
