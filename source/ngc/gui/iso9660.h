/****************************************************************************
 *
 * DVD ISO9660/Joliet Parsing
 *
 * This is not intended as a complete guide to ISO9660.
 * Here I use the bare minimum!
 ***************************************************************************/
 #ifndef _ISO9660_H
 #define _ISO9660_H

#define MAXJOLIET 256
#define MAXFILES 1000			/** Restrict to 1000 files per dir **/

typedef struct
{
  u64 offset;
  unsigned int length;
  char flags;
  char filename[MAXJOLIET];
  u16 filename_offset;
} FILEENTRIES;

extern u64 basedir;
extern u64 rootdir;
extern int rootdirlength;

extern int getpvd ();
extern int parseDVDdirectory ();
extern FILEENTRIES filelist[MAXFILES];

#endif
