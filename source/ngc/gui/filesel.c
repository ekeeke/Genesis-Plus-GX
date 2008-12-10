/****************************************************************************
 * ROM Selection Interface
 *
 * The following features are implemented:
 *   . SDCARD access with LFN support (through softdev's VFAT library)
 *   . DVD access
 *   . easy subdirectory browsing
 *   . ROM browser
 *   . alphabetical file sorting (Marty Disibio)
 *   . load from history list (Marty Disibio)
 *
 ***************************************************************************/
#include "shared.h"
#include "font.h"
#include "fileio_dvd.h"
#include "fileio_fat.h"
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


/***************************************************************************
 * ShowFiles
 *
 * Show filenames list in current directory
 ***************************************************************************/ 
static void ShowFiles (int offset, int selection) 
{
  int i, j;
  char text[MAXJOLIET+2];

  ClearScreen ();
  j = 0;

  for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
  {
    memset(text,0,MAXJOLIET+2);
    if (filelist[i].flags) sprintf(text, "[%s]", filelist[i].filename + filelist[i].filename_offset);
    else sprintf (text, "%s", filelist[i].filename + filelist[i].filename_offset);
    if (j == (selection - offset)) WriteCentre_HL ((j * fheight) + 120, text);
    else WriteCentre ((j * fheight) + 120, text);
    j++;
  }
  SetScreen ();
}

/****************************************************************************
 * FileSelector
 *
 * Let user select a file from the File listing
 ****************************************************************************/ 
int FileSelector() 
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
        ret = useFAT ? FAT_UpdateDir(go_up) : DVD_UpdateDir(go_up);

        /* get new entry list or quit */
        if (ret) maxfiles = useFAT ? FAT_ParseDirectory() : DVD_ParseDirectory();
        else return 0;
      }

      /*** This is a file ***/
      else 
      {
        /* Load file */
        genromsize = useFAT ? FAT_LoadFile(cart_rom) : DVD_LoadFile(cart_rom);
        if (genromsize)
        {
          memfile_autosave();
          reloadrom();
          memfile_autoload();
          return 1;
        }

        return 0;
      }
      redraw = 1;
    }
  }
}
