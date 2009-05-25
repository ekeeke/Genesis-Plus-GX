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

bool haveFATdir = 0;

/* current FAT directory */
static char fatdir[MAXPATHLEN];

/* current FAT device */
static int fat_type   = 0;
static int useHistory = 0;

/***************************************************************************
 * FAT_UpdateDir
 *
 * Update FAT current root directory
 ***************************************************************************/ 
int FAT_UpdateDir(bool go_up, char *filename)
{
  int size=0;
  char *test;
  char temp[1024];

  /* go up to parent directory */
  if (strcmp(filename,"..") == 0)
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
    sprintf(fatdir, "%s%s/",fatdir, filename);
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
  /* If loading from history then we need to setup a few more things. */
  if(useHistory)
  {  
    /* Get the parent folder for the file. */
    strncpy(fatdir, history.entries[selection].filepath, MAXJOLIET-1);
    fatdir[MAXJOLIET-1] = '\0';

    /* Get the length of the file. This has to be done
     * before calling LoadFile().  */
    char filepath[MAXJOLIET];
    struct stat filestat;
    snprintf(filepath, MAXJOLIET-1, "%s%s", history.entries[selection].filepath, history.entries[selection].filename);
    filepath[MAXJOLIET-1] = '\0';
    if(stat(filepath, &filestat) == 0)
    {
      filelist[selection].length = filestat.st_size;
    }
  }

  /* file size */
  int length = filelist[selection].length;

  if (length > 0)
  {
    /* Add/move the file to the top of the history. */
    if(!useHistory) history_add_file(fatdir, filelist[selection].filename);

    /* full filename */
    char fname[MAXPATHLEN];
    sprintf(fname, "%s%s",fatdir,filelist[selection].filename);

    /* open file */
    FILE *sdfile = fopen(fname, "rb");
    if (sdfile == NULL)
    {
      GUI_WaitPrompt("Error","Unable to open file !");
      haveFATdir = 0;
      return 0;
    }

    /* Read first data chunk */
    unsigned char temp[2048];
    fread(temp, 1, 2048, sdfile);
    fclose(sdfile);

    /* determine file type */
    if (!IsZipFile ((char *) temp))
    {
      /* re-open and read file */
      sdfile = fopen(fname, "rb");
      if (sdfile)
      {
        char msg[50];
        sprintf(msg,"Loading %d bytes ...", length);
        GUI_MsgBoxOpen("Information",msg,1);
        fread(buffer, 1, length, sdfile);
        fclose(sdfile);
        return length;
      }
    }
    else
    {
      /* unzip file */
      return UnZipBuffer (buffer, 0, fname);
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
  char root[10] = "";

  /* FAT header */
#ifdef HW_RVL
  if (type == TYPE_SD) sprintf (root, "sd:");
  else if (type == TYPE_USB) sprintf (root, "usb:");
#endif

  /* if FAT device type changed, reload filelist */
  if (fat_type != type) 
  {
    fat_type = type;
    haveFATdir = 0;
  }

  /* update filelist */
  if (haveFATdir == 0)
  {
    useHistory = 0;
    if (type == TYPE_RECENT)
    {
      /* fetch history list */
      useHistory = 1;
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
      /* reset root directory */
      sprintf (fatdir, "%s%s/roms/", root, DEFAULT_PATH);

      /* if directory doesn't exist, use root as default */
      DIR_ITER *dir = diropen(fatdir);
      if (dir == NULL) sprintf (fatdir, "%s/", root);
      else dirclose(dir);

      /* parse root directory */
      max = FAT_ParseDirectory ();
    }

    if (max > 0)
    {
      /* FAT is default */
      haveFATdir = 1;
      haveDVDdir = 0;

      /* reset File selector */
      FileSelClear(max);
      return 1;
    }
    else
    {
      /* no entries found */
      if (max == 0) GUI_WaitPrompt("Error","No files found !");
      return 0;
    }
  }

  return 1;
}
