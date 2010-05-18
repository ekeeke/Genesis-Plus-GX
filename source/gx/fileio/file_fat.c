/*
 * file_fat.c
 * 
 *  FAT loading support
 *
 *  Eke-Eke (2008,2009)
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
#include "history.h"
#include "unzip.h"
#include "filesel.h"
#include "file_fat.h"
#include "file_dvd.h"

/* current FAT directory */
static char *fatdir;

/* current FAT device */
static int fatType    = -1;

/***************************************************************************
 * FAT_ClearDirectory
 *
 * Clear FAT access flag
 ***************************************************************************/ 
void FAT_ClearDirectory(void)
{
  fatType = -1;
}

/***************************************************************************
 * FAT_UpdateDirectory
 *
 * Update FAT current directory
 * return zero if exiting root
 ***************************************************************************/ 
int FAT_UpdateDirectory(bool go_up, char *dirname)
{
  int size=0;
  char *test;
  char temp[1024];

  /* go up to parent directory */
  if (strcmp(dirname,"..") == 0)
  {
    /* determine last subdirectory namelength */
    sprintf(temp,"%s",fatdir);
    test= strtok(temp,"/");
    while (test != NULL)
    {
      size = strlen(test);
      test = strtok(NULL,"/");
    }

    /* remove last subdirectory name */
    size = strlen(fatdir) - size;
    fatdir[size-1] = 0;
  }
  else if (go_up)
  {
    /* root has no parent directory */
    return 0;
  }
  else
  {
    /* by default, simply append folder name */
    sprintf(fatdir, "%s%s/",fatdir, dirname);
  }
  return 1;
}

/***************************************************************************
 * FAT_ParseDirectory
 *
 * List files into one FAT directory
 ***************************************************************************/ 
int FAT_ParseDirectory(void)
{
  int nbfiles = 0;
  char filename[MAXPATHLEN];
  struct stat filestat;

  /* open directory */
  DIR_ITER *dir = diropen (fatdir);
  if (dir == NULL) 
  {
    GUI_WaitPrompt("Error","Unable to open directory !");
    return -1;
  }

  while ((dirnext(dir, filename, &filestat) == 0) && (nbfiles < MAXFILES))
  {
    if (strcmp(filename,".") != 0)
    {
      memset(&filelist[nbfiles], 0, sizeof (FILEENTRIES));
      sprintf(filelist[nbfiles].filename,"%s",filename);
      filelist[nbfiles].length = filestat.st_size;
      filelist[nbfiles].flags = (filestat.st_mode & S_IFDIR) ? 1 : 0;
      nbfiles++;
    }
  }

  dirclose(dir);

  /* Sort the file list */
  qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);

  return nbfiles;
}

/****************************************************************************
 * FAT_LoadFile
 *
 * This function will load a BIN, SMD or ZIP file from DVD into the ROM buffer.
 * This functions return the actual size of data copied into the buffer
 *
 ****************************************************************************/ 
int FAT_LoadFile(u8 *buffer, u32 selection) 
{
  char fname[MAXPATHLEN];
  int length = 0;

  /* Loading from history */
  if(fatType == TYPE_RECENT)
  {
    /* full filename */
    sprintf(fname,"%s%s",history.entries[selection].filepath,filelist[selection].filename);

    /* get the length of the file */
    struct stat filestat;
    if(stat(fname, &filestat) == 0)
      length = filestat.st_size;
  }
  else
  {
    /* full filename */
    sprintf(fname, "%s%s",fatdir,filelist[selection].filename);

    /* get the length of the file */
    length = filelist[selection].length;
  }

  if (length > 0)
  {
    /* Open file */
    FILE *sdfile = fopen(fname, "rb");
    if (sdfile == NULL)
    {
      GUI_WaitPrompt("Error","Unable to open file !");
      return 0;
    }

    /* Add/move the file to the top of the history. */
    if(fatType == TYPE_RECENT)
      history_add_file(history.entries[selection].filepath, filelist[selection].filename);
    else
      history_add_file(fatdir, filelist[selection].filename);

    /* file browser should be reinitialized */
    if(fatType == TYPE_RECENT)
      fatType = -1;

    /* Read first data chunk */
    unsigned char temp[FATCHUNK];
    fread(temp, FATCHUNK, 1, sdfile);
    fclose(sdfile);

    /* Determine file type */
    if (!IsZipFile ((char *) temp))
    {
      /* re-open and read file */
      sdfile = fopen(fname, "rb");
      if (sdfile)
      {
        char msg[50];
        sprintf(msg,"Loading %d bytes ...", length);
        GUI_MsgBoxOpen("Information",msg,1);
        int done = 0;
        while (length > FATCHUNK)
        {
          fread(buffer + done, FATCHUNK, 1, sdfile);
          length -= FATCHUNK;
          done += FATCHUNK;
        }
        fread(buffer + done, length, 1, sdfile);
        done += length;
        fclose(sdfile);
        return done;
      }
    }
    else
    {
      /* unzip file */
      return UnZipBuffer(buffer, 0, fname);
    }
  }

  return 0;
}

/****************************************************************************
 * OpenFAT
 *
 * Function to load a FAT directory and display to user.
 ****************************************************************************/ 
int FAT_Open(int type)
{
  int max = 0;

  if (type == TYPE_RECENT)
  {
    /* fetch history list */
    int i;
    for(i=0; i < NUM_HISTORY_ENTRIES; i++)
    {
      if(history.entries[i].filepath[0] > 0)
      {
        filelist[i].offset = 0;
        filelist[i].length = 0;
        filelist[i].flags = 0;
        strncpy(filelist[i].filename, history.entries[i].filename, MAXJOLIET-1);
        filelist[i].filename[MAXJOLIET-1] = '\0';
        max++;
      }
      else
      {
        /* Found the end of the list. */
        break;
      }
    }
  }
  else
  {
    /* default directory */
    fatdir = config.sddir;
#ifdef HW_RVL
    if (type == TYPE_USB)
      fatdir = config.usbdir;
#endif

    /* verify current dir exists, otherwise browse from root */
    DIR_ITER *dir = diropen(fatdir);
    if (dir)
      dirclose(dir);
#ifdef HW_RVL
    else if (type == TYPE_USB)
      sprintf (fatdir, "usb:/");
#endif
    else
      sprintf (fatdir, "sd:/");

    /* parse current directory */
    max = FAT_ParseDirectory ();
  }

  if (max < 0)
    return 0;

  if (max == 0)
  {
    GUI_WaitPrompt("Error","No files found !");
    return 0;
  }

  /* check if access type has changed */
  if (type != fatType)
  {
    /* set current access type */
    fatType = type;

    /* reset File selector */
    ClearSelector(max);

    /* clear DVD access flag */
    DVD_ClearDirectory();
  }

  return 1;
}
