/*
 * filesel.c
 * 
 *   File Selection menu
 *
 *   code by Softdev (2006), Eke-Eke (2007,2008) 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ********************************************************************************/

#include "shared.h"
#include "font.h"
#include "file_dvd.h"
#include "file_fat.h"
#include "filesel.h"

/* Global Variables */
int maxfiles      = 0;
int offset        = 0;
int selection     = 0;
int old_selection = 0;
int old_offset    = 0;
int useFAT        = 0;
int useHistory    = 0;
int haveDVDdir    = 0;
int haveFATdir    = 0;

FILEENTRIES filelist[MAXFILES];

/***************************************************************************
 * ShowFiles
 *
 * Show filenames list in current directory
 ***************************************************************************/ 
static void ShowFiles (int offset, int selection) 
{
  int i, j;
  char text[MAXJOLIET+2];

  ClearScreen ((GXColor)BLACK);
  j = 0;

  for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
  {
    memset(text,0,MAXJOLIET+2);
    if (filelist[i].flags) sprintf(text, "[%s]", filelist[i].filename + filelist[i].filename_offset);
    else sprintf (text, "%s", filelist[i].filename + filelist[i].filename_offset);
    if (j == (selection - offset)) WriteCentre_HL ((j * fheight) + PAGEOFFSET, text);
    else WriteCentre ((j * fheight) + PAGEOFFSET, text);
    j++;
  }
  SetScreen ();
}

/***************************************************************************
 * FileSortCallback (Marty Disibio)
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/ 
int FileSortCallback(const void *f1, const void *f2)
{
  /* Special case for implicit directories */
  if(((FILEENTRIES *)f1)->filename[0] == '.' || ((FILEENTRIES *)f2)->filename[0] == '.')
  {
    if(strcmp(((FILEENTRIES *)f1)->filename, ".") == 0) { return -1; }
    if(strcmp(((FILEENTRIES *)f2)->filename, ".") == 0) { return 1; }
    if(strcmp(((FILEENTRIES *)f1)->filename, "..") == 0) { return -1; }
    if(strcmp(((FILEENTRIES *)f2)->filename, "..") == 0) { return 1; }
  }
  
  /* If one is a file and one is a directory the directory is first. */
  if(((FILEENTRIES *)f1)->flags == 1 && ((FILEENTRIES *)f2)->flags == 0) return -1;
  if(((FILEENTRIES *)f1)->flags == 0 && ((FILEENTRIES *)f2)->flags == 1) return 1;
  
  return stricmp(((FILEENTRIES *)f1)->filename, ((FILEENTRIES *)f2)->filename);
}

/****************************************************************************
 * FileSelector
 *
 * Let user select a file from the File listing
.* ROM file buffer is provided as input
 * ROM size is returned
 *
 ****************************************************************************/ 
int FileSelector(unsigned char *buffer) 
{
  short p;
  int redraw = 1;
  int go_up = 0;
  int ret;
  int i,size;

  while (1)
  {
    if (redraw) ShowFiles (offset, selection);
    redraw = 0;
    p = ogc_input__getMenuButtons();
    
    /* scroll displayed filename */
    if (p & PAD_BUTTON_LEFT)
    {
      if (filelist[selection].filename_offset > 0)
      {
        filelist[selection].filename_offset --;
        redraw = 1;
      }
    }
    else if (p & PAD_BUTTON_RIGHT)
    {
      size = 0;
      for (i=filelist[selection].filename_offset; i<strlen(filelist[selection].filename); i++)
        size += font_size[(int)filelist[selection].filename[i]];
        
      if (size > back_framewidth)
      {
        filelist[selection].filename_offset ++;
        redraw = 1;
      }
    }

    /* highlight next item */
    else if (p & PAD_BUTTON_DOWN)
    {
      filelist[selection].filename_offset = 0;
      selection++;
      if (selection == maxfiles) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
      redraw = 1;
    }

    /* highlight previous item */
    else if (p & PAD_BUTTON_UP)
    {
      filelist[selection].filename_offset = 0;
      selection--;
      if (selection < 0)
      {
        selection = maxfiles - 1;
        offset = selection - PAGESIZE + 1;
      }
      if (selection < offset) offset -= PAGESIZE;
      if (offset < 0) offset = 0;
      redraw = 1;
    }

    /* go back one page */
    else if (p & PAD_TRIGGER_L)
    {
      filelist[selection].filename_offset = 0;
      selection -= PAGESIZE;
      if (selection < 0)
      {
        selection = maxfiles - 1;
        offset = selection - PAGESIZE + 1;
      }
      if (selection < offset) offset -= PAGESIZE;
      if (offset < 0) offset = 0;
      redraw = 1;
    }

    /* go forward one page */
    else if (p & PAD_TRIGGER_R)
    {
      filelist[selection].filename_offset = 0;
      selection += PAGESIZE;
      if (selection > maxfiles - 1) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
      redraw = 1;
    }

    /* quit */
    if (p & PAD_TRIGGER_Z)
    {
      filelist[selection].filename_offset = 0;
      return 0;
    }

    /* open selected file or directory */
    if ((p & PAD_BUTTON_A) || (p & PAD_BUTTON_B))
    {
      filelist[selection].filename_offset = 0;
      go_up = 0;
      
      if (p & PAD_BUTTON_B)
      {
        /* go up one directory or quit */
         go_up = 1;
         selection = useFAT ? 0 : 1;
      }

      /*** This is directory ***/
      if (filelist[selection].flags)
      {
        /* get new directory */
        if (useFAT) ret =FAT_UpdateDir(go_up);
        else ret = DVD_UpdateDir(go_up);

        /* get new entry list or quit */
        if (ret)
        {
          if (useFAT) maxfiles = FAT_ParseDirectory();
          else maxfiles = DVD_ParseDirectory();
        }
        else return 0;
      }

      /*** This is a file ***/
      else 
      {
        /* root directory ? */
        if (go_up) return 0;

        /* Load file */
        if (useFAT) return FAT_LoadFile(buffer);
        else return DVD_LoadFile(buffer);
      }
      redraw = 1;
    }
  }
}
