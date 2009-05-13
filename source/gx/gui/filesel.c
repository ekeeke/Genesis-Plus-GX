/*
 * filesel.c
 * 
 *   File Selection menu
 *
 *   Softdev (2006)
 *   Eke-Eke (2007,2008,2009) 
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
#include "gui.h"
#include "file_dvd.h"
#include "file_fat.h"
#include "filesel.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#define PAGESIZE 11
#define PAGEOFFSET 120

/* Global Variables */
int maxfiles      = 0;
int offset        = 0;
int selection     = 0;
int old_selection = 0;
int old_offset    = 0;
int useFAT        = 0;
int haveDVDdir    = 0;
int haveFATdir    = 0;

FILEENTRIES filelist[MAXFILES];

/*****************************************************************************/
/*  GUI Buttons data                                                         */
/*****************************************************************************/
static butn_data arrow_up_data =
{
  {NULL,NULL},
  {Button_up_png,Button_up_over_png}
};

static butn_data arrow_down_data =
{
  {NULL,NULL},
  {Button_down_png,Button_down_over_png}
};

/*****************************************************************************/
/*  GUI Arrows button                                                        */
/*****************************************************************************/

static gui_butn arrow_up = {&arrow_up_data,BUTTON_VISIBLE|BUTTON_OVER_SFX,{0,0,0,0},14,76,360,32};
static gui_butn arrow_down = {&arrow_down_data,BUTTON_VISIBLE|BUTTON_OVER_SFX,{0,0,0,0},14,368,360,32};

/*****************************************************************************/
/*  GUI helpers                                                              */
/*****************************************************************************/
static gui_item action_cancel =
{
#ifdef HW_RVL
  NULL,Key_B_wii_png,"","Previous",10,422,28,28
#else
  NULL,Key_B_gcn_png,"","Previous",10,422,28,28
#endif
};

static gui_item action_select =
{
#ifdef HW_RVL
  NULL,Key_A_wii_png,"","Load ROM file",602,422,28,28
#else
  NULL,Key_A_gcn_png,"","Load ROM file",602,422,28,28
#endif
};

/*****************************************************************************/
/*  GUI Background images                                                    */
/*****************************************************************************/
static gui_image bg_filesel[10] =
{
  {NULL,Bg_main_png,IMAGE_VISIBLE,356,144,348,288,255},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Banner_top_png,IMAGE_VISIBLE,0,0,640,108,255},
  {NULL,Banner_bottom_png,IMAGE_VISIBLE,0,380,640,100,255},
  {NULL,Main_logo_png,IMAGE_VISIBLE,466,40,152,44,255},
  {NULL,Frame_s1_png,IMAGE_VISIBLE,8,70,372,336,200},
  {NULL,Frame_s2_png,0,384,264,248,140,200},
  {NULL,Snap_empty_png,IMAGE_VISIBLE,422,114,164,116,255},
  {NULL,NULL,0,424,116,160,112,255},
  {NULL,Snap_frame_png,IMAGE_VISIBLE,388,112,236,148,255}
};

/*****************************************************************************/
/*  GUI Descriptor                                                           */
/*****************************************************************************/
static gui_menu menu_browser =
{
  "Game Selection",
  -1,-1,
  0,0,10,
  NULL,
  NULL,
  bg_filesel,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  FALSE
};

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
..* ROM file buffer is provided as input
 * ROM size is returned
 *
 ****************************************************************************/ 
int FileSelector(unsigned char *buffer)
{
  short p;
  int ret,i,yoffset,string_offset;
  int go_up = 0;
  int quit =0;
  int old = -1;
  char text[MAXPATHLEN];
  char fname[MAXPATHLEN];
  FILE *xml,*snap;

#ifdef HW_RVL
  int x,y;
  gui_butn *button;
  struct orient_t orient;
#endif

  /* Initialize Menu */
  gui_menu *m = &menu_browser;
  GUI_InitMenu(m);

  /* Initialize directory icon */
  gui_image dir_icon;
  dir_icon.texture = gxTextureOpenPNG(Browser_dir_png,0);
  dir_icon.w = dir_icon.texture->width;
  dir_icon.h = dir_icon.texture->height;
  dir_icon.x = 26;
  dir_icon.y = PAGEOFFSET;

  /* Initialize selection bar */
  gui_image bar_over;
  bar_over.texture = gxTextureOpenPNG(Overlay_bar_png,0);
  bar_over.w = bar_over.texture->width;
  bar_over.h = bar_over.texture->height;
  bar_over.x = 22;
  bar_over.y = -(bar_over.h - dir_icon.h)/2;

  while (!quit)
  {
    /* Ensure a file is selected */
    if (!filelist[selection].flags)
    {
      /* get ROM filename without extension */
      sprintf (text, "%s", filelist[selection].filename);
      if (strlen(text) >= 4) text[strlen(text) - 4] = 0;

      /* ROM database informations */
      sprintf (fname, "%s/db/%s.xml", DEFAULT_PATH, text);
      xml = fopen(fname, "rb");
      if (xml)
      {
        bg_filesel[6].state |= IMAGE_VISIBLE;
        fclose(xml); /* TODO */
      }
      else
      {
        bg_filesel[6].state &= ~IMAGE_VISIBLE;
      }

      /* ROM snapshot */
      if (old != selection)
      {
        old = selection;

        /* delete previous texture if any */
        gxTextureClose(&bg_filesel[8].texture);
        bg_filesel[8].state &= ~IMAGE_VISIBLE;

        /* open screenshot file */
        sprintf (fname, "%s/snaps/%s.png", DEFAULT_PATH, text);
        snap = fopen(fname, "rb");
        if (snap)
        {
          bg_filesel[8].texture = gxTextureOpenPNG(0,snap);
          fclose(snap);
          if (bg_filesel[8].texture) bg_filesel[8].state |= IMAGE_VISIBLE;
        }
      }
      strcpy(action_select.comment,"Load ROM File");
    }
    else
    {
      /* update helper */
      if (!strcmp(filelist[selection].filename,".."))
        strcpy(action_select.comment,"Previous Directory");
      else
        strcpy(action_select.comment,"Open Directory");
    }

    /* Draw menu*/
    GUI_DrawMenu(m);

    /* Draw Files list */
    yoffset = PAGEOFFSET;
    for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
    {
      if (i == selection)
      {
        /* scrolling text */
        string_offset = filelist[i].filename_offset/10; 
        if (string_offset >= strlen(filelist[i].filename)) 
        {
          string_offset = 0;
          filelist[i].filename_offset = 0;
        }
        sprintf(text, "%s  ",filelist[i].filename + string_offset);
        strncat(text, filelist[i].filename, string_offset);

        gxDrawTexture(bar_over.texture,bar_over.x,yoffset+bar_over.y,bar_over.w,bar_over.h,255);
        if (filelist[i].flags)
        {
          /* directory icon */
          gxDrawTexture(dir_icon.texture,dir_icon.x-1,yoffset-1,dir_icon.w+2,dir_icon.h+2,255);
          if (FONT_write(text,18,dir_icon.x+dir_icon.w+6,yoffset+16,bar_over.w-dir_icon.w-14,(GXColor)WHITE))
          {
            /* text scrolling */
            filelist[i].filename_offset ++;
          }
        }
        else
        {
          if (FONT_write(text,18,dir_icon.x,yoffset+16,bar_over.w-8,(GXColor)WHITE))
          {
            /* text scrolling */
            filelist[i].filename_offset ++;
          }
        }
      }
      else
      {
        filelist[i].filename_offset = 0;
        if (filelist[i].flags)
        {
          /* directory icon */
          gxDrawTexture(dir_icon.texture,dir_icon.x,yoffset,dir_icon.w,dir_icon.h,255);
          FONT_write(filelist[i].filename,16,dir_icon.x+dir_icon.w+6,yoffset+16,bar_over.w-dir_icon.w-14,(GXColor)WHITE);
        }
        else
        {
          FONT_write(filelist[i].filename,16,dir_icon.x,yoffset+16,bar_over.w-8,(GXColor)WHITE);
        }
      }

      yoffset += 22;
    }

#ifdef HW_RVL
    if (Shutdown)
    {
      gxTextureClose(&w_pointer);
      GUI_DeleteMenu(m);
      GUI_FadeOut();
      shutdown();
      SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    }
    else if (m_input.ir.valid)
    {
      /* get cursor position */
      x = m_input.ir.x;
      y = m_input.ir.y;

      /* draw wiimote pointer */
      WPAD_Orientation(0,&orient);
      gxResetAngle(orient.roll);
      gxDrawTexture(w_pointer, x, y, w_pointer->width, w_pointer->height,255);
      gxResetAngle(0.0);

      /* find selected item */
      yoffset = PAGEOFFSET;
      m->selected = m->max_buttons + 2;
      for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
      {
        if ((x<=380)&&(y>=yoffset)&&(y<(yoffset+22)))
        {
          selection = i;
          m->selected = -1;
          break;
        }
        yoffset += 22;
      }

      /* find selected button */
      for (i=0; i<2; i++)
      {
        button = m->arrows[i];
        if (button)
        {
          if (button->state & BUTTON_VISIBLE)
          {
            if ((x>=button->x)&&(x<=(button->x+button->w))&&(y>=button->y)&&(y<=(button->y+button->h)))
            {
              m->selected = m->max_buttons + i;
              break;
            }
          }
        }
      }
    }
    else
    {
      /* reset indicator */
      m->selected = -1;
    }
#endif

    /* copy EFB to XFB */
    gxSetScreen ();

    p = m_input.keys;

    /* highlight next item */
    if (p & PAD_BUTTON_DOWN)
    {
      filelist[selection].filename_offset = 0;
      selection++;
      if (selection == maxfiles) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
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
    }

    /* go forward one page */
    else if (p & PAD_TRIGGER_R)
    {
      filelist[selection].filename_offset = 0;
      selection += PAGESIZE;
      if (selection > maxfiles - 1) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
    }

    /* quit */
    else if (p & PAD_TRIGGER_Z)
    {
      filelist[selection].filename_offset = 0;
      quit = 2;
    }

    /* open selected file or directory */
    else if ((p & PAD_BUTTON_A) || (p & PAD_BUTTON_B))
    {
      filelist[selection].filename_offset = 0;
      go_up = 0;

      if (p & PAD_BUTTON_B)
      {
        /* go up one directory or quit */
         go_up = 1;
         selection = 0;
      }
#ifdef HW_RVL
      else
      {
        /* arrow buttons selected */
        if (m->selected == m->max_buttons) /* up arrow */
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
        }
        else if (m->selected == (m->max_buttons+1)) /* down arrow */
        {
          filelist[selection].filename_offset = 0;
          selection++;
          if (selection == maxfiles) selection = offset = 0;
          if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
        }
      }
#endif

      /* ensure we are in focus area */
      if (go_up || (m->selected < m->max_buttons))
      {
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
          else
          {
            quit = 2;
          }
        }

        /*** This is a file ***/
        else 
        {
          /* root directory ? */
          if (go_up) quit = 2;
          else quit = 1;
        }
      }
    }
  }

  /* Delete Menu */
  GUI_DeleteMenu(m);
  gxTextureClose(&bar_over.texture);
  gxTextureClose(&dir_icon.texture);

  if (quit == 2) return 0;
  else if (useFAT) return FAT_LoadFile(buffer);
  else return DVD_LoadFile(buffer);
}
