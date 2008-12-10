/****************************************************************************
 *
 * FAT loading support
 *
 ***************************************************************************/
#ifndef _FILEIO_FAT_H
#define _FILEIO_FAT_H

#define TYPE_RECENT   0
#define TYPE_SD       1

#ifdef HW_RVL
#define TYPE_USB      2
#endif

extern int FAT_UpdateDir(int go_up);
extern int FAT_ParseDirectory(void);
extern int FAT_LoadFile(unsigned char* buffer);
extern int FAT_Open (int device);

#endif
