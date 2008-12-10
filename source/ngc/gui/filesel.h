/****************************************************************************
 * ROM Selection Interface
 *
 ***************************************************************************/
#ifndef _FILESEL_H
#define _FILESEL_H

#define MAXJOLIET 256
#define MAXFILES 1000
#define PAGESIZE 12

typedef struct
{
  u64 offset;
  unsigned int length;
  char flags;
  char filename[MAXJOLIET];
  u16 filename_offset;
}FILEENTRIES;

FILEENTRIES filelist[MAXFILES];

/* Global Variables */
extern int maxfiles;
extern int offset;
extern int selection;
extern int old_selection;
extern int old_offset;
extern int useFAT;
extern int haveDVDdir;
extern int haveFATdir;

extern int FileSelector();

#endif
