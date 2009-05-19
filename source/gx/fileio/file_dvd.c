/*
 * file_dvd.c
 * 
 *  ISO9660/Joliet DVD loading support
 *
 *  Softdev (2006)
 *  Eke-Eke (2007,2008,2009)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ********************************************************************************/

#include "shared.h"
#include "gui.h"
#include "dvd.h"
#include "unzip.h"
#include "filesel.h"

/** Minimal ISO Directory Definition **/
#define RECLEN 0            /* Record length */
#define EXTENT 6            /* Extent */
#define FILE_LENGTH 14      /* File length (BIG ENDIAN) */
#define FILE_FLAGS 25       /* File flags */
#define FILENAME_LENGTH 32  /* Filename length */
#define FILENAME 33         /* ASCIIZ filename */

/** Minimal Primary Volume Descriptor **/
#define PVDROOT 0x9c

/** Static Variables **/
static u64 rootdir        = 0;  /* current root directory offset */
static u64 basedir        = 0;  /* base directory offset */
static int rootdirlength  = 0;  /* current root directory length */
static int IsJoliet       = 0;
static int diroffset      = 0;
static char dvdbuffer[2048];

/****************************************************************************
 * Primary Volume Descriptor
 *
 * The PVD should reside between sector 16 and 31.
 * This is for single session DVD only.
 ****************************************************************************/
static int getpvd()
{
  int sector = 16;
  u32 rootdir32;

  basedir = rootdirlength = 0;
  IsJoliet = -1;

  /** Look for Joliet PVD first **/
  while (sector < 32)
  {
    if (dvd_read (&dvdbuffer, 2048, (u64)(sector << 11)))
    {
      if (memcmp (&dvdbuffer, "\2CD001\1", 8) == 0)
      {
        memcpy(&rootdir32, &dvdbuffer[PVDROOT + EXTENT], 4);
        basedir = (u64)rootdir32;
        memcpy (&rootdirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
        basedir <<= 11;
        IsJoliet = 1;
        break;
      }
    }
    else return 0; /*** Can't read sector! ***/
    sector++;
  }

  if (IsJoliet > 0) return 1; /*** Joliet PVD Found ? ***/

  /*** Look for standard ISO9660 PVD ***/
  sector = 16;
  while (sector < 32)
  {
    if (dvd_read (&dvdbuffer, 2048, (u64)(sector << 11)))
    {
      if (memcmp (&dvdbuffer, "\1CD001\1", 8) == 0)
      {
        memcpy (&rootdir32, &dvdbuffer[PVDROOT + EXTENT], 4);
        basedir = (u64)rootdir32;
        memcpy (&rootdirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
        IsJoliet = 0;
        basedir <<= 11;
        break;
      }
    }
    else return 0; /*** Can't read sector! ***/
      sector++;
  }

  return (IsJoliet == 0);
}

/****************************************************************************
 * getentry
 *
 * Support function to return the next file entry, if any
 * Declared static to avoid accidental external entry.
 ****************************************************************************/
static int getentry(int entrycount)
{
  char fname[512]; /* Huge, but experience has determined this */
  char *ptr;
  char *filename;
  char *filenamelength;
  char *rr;
  int j;
  u32 offset32;

  /* Basic checks */
  if (entrycount >= MAXFILES) return 0;
  if (diroffset >= 2048) return 0;

  /** Decode this entry **/
  if (dvdbuffer[diroffset])  /* Record length available */
  {
    /* Update offsets into sector buffer */
    ptr = (char *) &dvdbuffer[0];
    ptr += diroffset;
    filename = ptr + FILENAME;
    filenamelength = ptr + FILENAME_LENGTH;

    /* Check for wrap round - illegal in ISO spec,
     * but certain crap writers do it! */
    if ((diroffset + dvdbuffer[diroffset]) > 2048) return 0;

    if (*filenamelength)
    {
      memset (&fname, 0, 512);

      /*** Do ISO 9660 first ***/
      if (!IsJoliet) strcpy (fname, filename);
      else
      {
        /*** The more tortuous unicode joliet entries ***/
        for (j = 0; j < (*filenamelength >> 1); j++)
        {
          fname[j] = filename[j * 2 + 1];
        }

        fname[j] = 0;

        if (strlen (fname) >= MAXJOLIET) fname[MAXJOLIET - 1] = 0;
        if (strlen (fname) == 0) fname[0] = filename[0];
      }

      if (strlen (fname) == 0) strcpy (fname, ".");
      else
      {
        if (fname[0] == 1) strcpy (fname, "..");
        else
        {
          /*
           * Move *filenamelength to t,
           * Only to stop gcc warning for noobs :)
           */
          int t = *filenamelength;
          fname[t] = 0;
        }
      }

      /** Rockridge Check **/
      rr = strstr (fname, ";");
      if (rr != NULL) *rr = 0;

      strcpy (filelist[entrycount].filename, fname);
      memcpy (&offset32, &dvdbuffer[diroffset + EXTENT], 4);
      filelist[entrycount].offset = (u64)offset32;
      memcpy (&filelist[entrycount].length, &dvdbuffer[diroffset + FILE_LENGTH], 4);
      memcpy (&filelist[entrycount].flags, &dvdbuffer[diroffset + FILE_FLAGS], 1);

      filelist[entrycount].offset <<= 11;
      filelist[entrycount].flags = filelist[entrycount].flags & 2;
      filelist[entrycount].filename_offset = 0;

      /*** Prepare for next entry ***/
      diroffset += dvdbuffer[diroffset];

      return 1;
    }
  }
  return 0;
}

/***************************************************************************
 * DVD_UpdateRootDir
 *
 * Update DVD current root directory
 ***************************************************************************/ 
int DVD_UpdateDir(bool go_up)
{
  /* root has no parent directory */
  if (go_up && (basedir == rootdir)) return 0;

  /* by default, update current directory */
  rootdir = filelist[selection].offset;
  rootdirlength = filelist[selection].length;

  /* reinit selector (previous value is saved for one level) */
  if (selection == 0)
  {
    selection = old_selection;
    offset = old_offset;
    old_selection = 0;
    old_offset = 0;
  }
  else
  {
    /* save current selector value */
    old_selection = selection;
    old_offset = offset;
    selection = 0;
    offset = 0;
  }

  return 1;
}

/****************************************************************************
 * DVD_ParseDirectory
 *
 * This function will parse the directory tree.
 * It relies on rootdir and rootdirlength being pre-populated by a call to
 * getpvd, a previous parse or a menu selection.
 *
 * The return value is number of files collected, or 0 on failure.
 ****************************************************************************/
int DVD_ParseDirectory(void)
{
  int pdlength;
  u64 pdoffset;
  u64 rdoffset;
  int len = 0;
  int filecount = 0;

  pdoffset = rdoffset = rootdir;
  pdlength = rootdirlength;
  filecount = 0;

  /** Clear any existing values ***/
  memset (&filelist, 0, sizeof (FILEENTRIES) * MAXFILES);

  /*** Get as many files as possible ***/
  while (len < pdlength)
  {
    if (dvd_read (&dvdbuffer, 2048, pdoffset) == 0) return 0;

    diroffset = 0;

    while (getentry (filecount))
    {
      if (strcmp(filelist[filecount].filename,".") && (filecount < MAXFILES))
        filecount++;
    }

    len += 2048;
    pdoffset = rdoffset + len;
  }

  /* Sort the file list */
  qsort(filelist, filecount, sizeof(FILEENTRIES), FileSortCallback);

  return filecount;
}

/****************************************************************************
 * DVD_LoadFile
 *
 * This function will load a BIN, SMD or ZIP file from DVD into the ROM buffer.
 * The index values indicates the file position in filentry list 
 * This functions return the actual size of data copied into the buffer
 *
 ****************************************************************************/ 
int DVD_LoadFile(u8 *buffer) 
{
  /* file size */
  int length = filelist[selection].length;

  if (length > 0)
  {
    /* Read first data chunk */
    char readbuffer[2048];
    u64 discoffset = filelist[selection].offset;
    dvd_read (&readbuffer, 2048, discoffset);

    /* determine file type */
    if (!IsZipFile ((char *) readbuffer))
    {
      char msg[50];
      sprintf(msg,"Loading %d bytes...", length);
      GUI_MsgBoxOpen("INFO",msg);
      /* How many 2k blocks to read */
      int blocks = length / 2048;
      int readoffset = 0;
      int i;

      /* read data chunks */
      for (i = 0; i < blocks; i++)
      {
        dvd_read(readbuffer, 2048, discoffset);
        discoffset += 2048;
        memcpy (buffer + readoffset, readbuffer, 2048);
        readoffset += 2048;
      }

      /* final read */ 
      i = length % 2048;
      if (i)
      {
        dvd_read (readbuffer, 2048, discoffset);
        memcpy (buffer + readoffset, readbuffer, i);
      }

      GUI_MsgBoxClose();
      return length;
    }
    else
    {
      return UnZipBuffer (buffer, discoffset, NULL);
    }
  }

  return 0;
}

/****************************************************************************
 * DVD_Open
 *
 * Function to load a DVD directory and display to user.
 ****************************************************************************/ 

int DVD_Open(void)
{
  /* reset flags */
  useFAT = 0;

  /* is DVD mounted ? */
  if (!getpvd())
  {
    /* mount DVD */
    GUI_MsgBoxOpen("INFO", "Mounting DVD ... Wait");

#ifdef HW_RVL
    u32 val;
    DI_GetCoverRegister(&val);  

    if(val & 0x1)
    {
      GUI_WaitPrompt("ERROR","No Disc inserted !");
      return 0;
    }

    DI_Mount();
    while(DI_GetStatus() & DVD_INIT);
    if (!(DI_GetStatus() & DVD_READY))
    {
      char msg[50];
      sprintf(msg, "DI Status Error: 0x%08X !\n",DI_GetStatus());
      GUI_WaitPrompt("ERROR",msg);
      return 0;
    }
#else
    DVD_Mount();
#endif

    haveDVDdir = 0;
    if (!getpvd())
    {
      GUI_WaitPrompt("ERROR","Disc can not be read !");
      return 0;
    }

    GUI_MsgBoxClose();
  }

  if (haveDVDdir == 0)
  {
    /* reset root directory */
    rootdir = basedir;

    /* parse root directory */
    int max = DVD_ParseDirectory ();
    if (max)
    {
      /* set DVD as default */
      haveDVDdir = 1;
      haveFATdir = 0;

      /* reset File selector */
      maxfiles      = max;
      offset        = 0;
      selection     = 0;
      old_offset    = 0;
      old_selection = 0;
      return 1;
    }
    else
    {
      /* no entries found */
      GUI_WaitPrompt("ERROR","No files found !");
      return 0;
    }
  }

  return 1;
}
