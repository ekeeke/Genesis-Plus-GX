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

FILEENTRIES filelist[MAXFILES];

static int offset        = 0;
static int selection     = 0;
static int old_selection = 0;
static int old_offset    = 0;
static int maxfiles      = 0;

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
  NULL,Key_B_png,"","Previous Directory",10,422,28,28
};

static gui_item action_select =
{
  NULL,Key_A_png,"","Load ROM file",602,422,28,28
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
  {NULL,Frame_s1_png,IMAGE_VISIBLE,8,70,372,336,230},
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
int FileSelector(unsigned char *buffer, bool useFAT)
{
  short p;
  int ret,i,yoffset;
  int size = 0;
  int go_up = 0;
  int string_offset = 0;
  int old = -1;
  char text[MAXPATHLEN];
  char fname[MAXPATHLEN];
  FILE *xml,*snap;

#ifdef HW_RVL
  int x,y;
  gui_butn *button;
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

  while (1)
  {
    /* ROM file snapshot/database */
    if (old != selection)
    {
      old = selection;
      string_offset = 0;

      /* delete previous texture if any */
      gxTextureClose(&bg_filesel[8].texture);
      bg_filesel[8].state &= ~IMAGE_VISIBLE;
      bg_filesel[6].state &= ~IMAGE_VISIBLE;

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

        /* open screenshot file */
        sprintf (fname, "%s/snaps/%s.png", DEFAULT_PATH, text);
        snap = fopen(fname, "rb");
        if (snap)
        {
          bg_filesel[8].texture = gxTextureOpenPNG(0,snap);
          if (bg_filesel[8].texture) bg_filesel[8].state |= IMAGE_VISIBLE;
          fclose(snap);
        }
      }
    }

    /* update helper */
    if (m->selected != -1)
    {
      /* out of focus */
      strcpy(action_select.comment,"");
    }
    else if (!filelist[selection].flags)
    {
      /* this is a file */
      strcpy(action_select.comment,"Load ROM File");
    }
    else
    {
      /* this is a directory */
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
        if ((string_offset/10) >= strlen(filelist[i].filename))  string_offset = 0;
        sprintf(text, "%s  ",filelist[i].filename + string_offset/10);
        strncat(text, filelist[i].filename, string_offset/10);

        gxDrawTexture(bar_over.texture,bar_over.x,yoffset+bar_over.y,bar_over.w,bar_over.h,255);
        if (filelist[i].flags)
        {
          /* directory icon */
          gxDrawTexture(dir_icon.texture,dir_icon.x-1,yoffset-1,dir_icon.w+2,dir_icon.h+2,255);
          if (FONT_write(text,18,dir_icon.x+dir_icon.w+6,yoffset+16,bar_over.w-dir_icon.w-14,(GXColor)WHITE))
          {
            /* text scrolling */
            string_offset ++;
          }
        }
        else
        {
          if (FONT_write(text,18,dir_icon.x,yoffset+16,bar_over.w-8,(GXColor)WHITE))
          {
            /* text scrolling */
            string_offset ++;
          }
        }
      }
      else
      {
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
      gxTextureClose(&bar_over.texture);
      gxTextureClose(&dir_icon.texture);
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
      gxDrawTextureRotate(w_pointer, x-w_pointer->width/2, y-w_pointer->height/2, w_pointer->width, w_pointer->height,m_input.ir.angle,255);

      /* find selected item */
      yoffset = PAGEOFFSET  - 4;
      m->selected = m->max_buttons + 2;
      for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
      {
        if ((x<=380)&&(y>=yoffset)&&(y<(yoffset+24)))
        {
          selection = i;
          m->selected = -1;
          break;
        }
        yoffset += 24;
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
    gxSetScreen();

    p = m_input.keys;

    /* highlight next item */
    if (p & PAD_BUTTON_DOWN)
    {
      selection++;
      if (selection == maxfiles) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
    }

    /* highlight previous item */
    else if (p & PAD_BUTTON_UP)
    {
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
      selection += PAGESIZE;
      if (selection > maxfiles - 1) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
    }

    /* quit */
    else if (p & PAD_TRIGGER_Z)
    {
      GUI_DeleteMenu(m);
      gxTextureClose(&bar_over.texture);
      gxTextureClose(&dir_icon.texture);
      return 0;
    }

    /* open selected file or directory */
    else if ((p & PAD_BUTTON_A) || (p & PAD_BUTTON_B))
    {
      string_offset = 0;
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
          /* force going up */
          go_up = (selection == 0);

          /* get new directory */
          if (useFAT)
            ret = FAT_UpdateDirectory(go_up,filelist[selection].filename);
          else
            ret = DVD_UpdateDirectory(go_up,filelist[selection].offset,filelist[selection].length);

          /* get new entry list or quit */
          if (ret)
          {
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

            /* get directory entries */
            if (useFAT)
              maxfiles = FAT_ParseDirectory();
            else
              maxfiles = DVD_ParseDirectory();
          }
          else
          {
            GUI_DeleteMenu(m);
            gxTextureClose(&bar_over.texture);
            gxTextureClose(&dir_icon.texture);
            return 0;
          }
        }

        /*** This is a file ***/
        else 
        {
          /* root directory ? */
          if (go_up)
          {
            GUI_DeleteMenu(m);
            gxTextureClose(&bar_over.texture);
            gxTextureClose(&dir_icon.texture);
            return 0;
          }
          else
          {
            /* user confirmation */
            if (GUI_ConfirmPrompt("Load selected File ?"))
            {
              /* Load ROM file from device */
              if (useFAT)
                size = FAT_LoadFile(buffer,selection);
              else
                size = DVD_LoadFile(buffer,selection);

              /* Reload emulation */
              if (size)
              {
                memfile_autosave(-1,config.state_auto);
                reloadrom(size,filelist[selection].filename);
                memfile_autoload(config.sram_auto,config.state_auto);
              }

              /* Exit */
              GUI_MsgBoxClose();
              GUI_DeleteMenu(m);
              gxTextureClose(&bar_over.texture);
              gxTextureClose(&dir_icon.texture);
              return size;
            }

            /* user canceled */
            GUI_MsgBoxClose();
          }
        }
      }
    }
  }
}

void ClearSelector(u32 max)
{
  maxfiles = max;
  offset = 0;
  selection = 0;
  old_offset = 0;
  old_selection = 0;
}
