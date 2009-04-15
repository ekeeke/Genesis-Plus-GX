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
#include "menu.h"
#include "font.h"
#include "file_dvd.h"
#include "file_fat.h"
#include "filesel.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

/* this is emulator specific ! */
#define PAGESIZE 14
//#define PAGEOFFSET 120


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
/*static void ShowFiles (int offset, int selection) 
{
  int i, j;
  char text[MAXJOLIET+2];

  gxClearScreen ((GXColor)BLACK);
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
  gxSetScreen ();
}*/

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
//  int redraw = 1;
  int go_up = 0;
  int quit =0;
  int ret;
  int i,size;
  int yoffset = 86;

  char comment[2][30] = {"Back","Load ROM File"};
  char fname[MAXPATHLEN];
  FILE *xml;
  FILE *snap;

#ifdef HW_RVL
  /* allocate wiimote pointer data (only done once) */
  gx_texture *pointer = gxTextureOpenPNG(generic_point_png);
#endif

  /* allocate background overlay texture */
  gui_image *overlay = &bg_overlay_line;
  if (!overlay->texture) overlay->texture = gxTextureOpenPNG(overlay->data);

  /* allocate background image texture */
  gui_image *bg = &bg_right;
  if (!bg->texture) bg->texture = gxTextureOpenPNG(bg->data);

  /* allocate logo texture */
  gui_image *logo = &logo_small;
  if (!logo->texture) logo->texture = gxTextureOpenPNG(logo->data);

  /* allocate background elements textures */
  gui_image *banners[2] = {&top_banner,&bottom_banner};
  gui_item *helpers[2] = {&action_cancel, &action_select};
  for (i=0; i<2; i++)
  {
    /* banners */
    if (!banners[i]->texture) banners[i]->texture = gxTextureOpenPNG(banners[i]->data);

    /* key helpers */
    if (!helpers[i]->texture) helpers[i]->texture = gxTextureOpenPNG(helpers[i]->data);
  }

  /* frames */
  gx_texture *frame_left  = gxTextureOpenPNG(Frame_s1_png);
  gx_texture *frame_right = gxTextureOpenPNG(Frame_s2_png);

  /* arrows */
  gx_texture *arrow_up        = gxTextureOpenPNG(Button_up_png);
  gx_texture *arrow_up_over   = gxTextureOpenPNG(Button_up_over_png);
  gx_texture *arrow_down      = gxTextureOpenPNG(Button_down_png);
  gx_texture *arrow_down_over = gxTextureOpenPNG(Button_down_over_png);

  /* selection bar */
  gx_texture *bar_over = gxTextureOpenPNG(Overlay_bar_png);

  /* directory icon */
  gx_texture *dir_icon = gxTextureOpenPNG(Browser_dir_png);

  /* stars */
  gx_texture *star_full  = gxTextureOpenPNG(Star_full_png);
  gx_texture *star_empty = gxTextureOpenPNG(Star_empty_png);

  /* snapshots */
  gx_texture *snap_frame = gxTextureOpenPNG(Snap_frame_png);
  gx_texture *snap_empty = gxTextureOpenPNG(Snap_empty_png);

  while (!quit)
  {
    /* Draw menu*/
    gxClearScreen ((GXColor)BACKGROUND);
    gxDrawRepeat(overlay->texture,overlay->x,overlay->y,overlay->w,overlay->h);
    gxDrawTexture(bg->texture,bg->x,bg->y,bg->w,bg->h,255);
    gxDrawTexture(banners[0]->texture,banners[0]->x,banners[0]->y,banners[0]->w,banners[0]->h,255);
    gxDrawTexture(banners[1]->texture,banners[1]->x,banners[1]->y,banners[1]->w,banners[1]->h,255);
    gxDrawTexture(helpers[0]->texture,helpers[0]->x,helpers[0]->y,helpers[0]->w,helpers[0]->h,255);
    gxDrawTexture(helpers[1]->texture,helpers[1]->x,helpers[1]->y,helpers[1]->w,helpers[1]->h,255);
    gxDrawTexture(logo->texture,logo->x,logo->y,logo->w,logo->h,255);

    /* Draw title & helps */
    FONT_alignLeft("ROM Files Selection", 22,10,56, (GXColor)WHITE);
    FONT_alignLeft(comment[0], 16, helpers[0]->x+helpers[0]->w+6,helpers[0]->y+(helpers[0]->h-16)/2 + 16, (GXColor)WHITE);
    FONT_alignRight(comment[1], 16, helpers[1]->x - 6, helpers[1]->y+(helpers[1]->h-16)/2 + 16, (GXColor)WHITE);

    /* ROM database informations */
    strncpy(fname, filelist[selection].filename, strlen(filelist[selection].filename) - 4);
    sprintf(fname, "%s%s.xml","/genplus/db/",fname);
    xml = fopen(fname, "rb");
    if (xml)
    {
      gxDrawTexture(frame_right,384,264,frame_right->width,frame_right->height,204);
      fclose(xml); /* TODO */
    }

    /* ROM snapshot */
    strncpy(fname, filelist[selection].filename, strlen(filelist[selection].filename) - 4);
    sprintf(fname, "%s%s.png","/genplus/snap/",fname);
    snap = fopen(fname, "rb");
    if (snap)
    {
      fclose(snap); /* TODO */
    }
    else
    {
      gxDrawTexture(snap_empty,422,114,snap_empty->width,snap_empty->height,255);
    }

    /* Cartridge picture */
    gxDrawTexture(snap_frame,388,112,snap_frame->width,snap_frame->height,255);

    /* File list */
    gxDrawTexture(frame_left,14,76,frame_left->width,frame_left->height,204);
    for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
    {
      if (i == selection)
      {
        gxDrawTexture(bar_over,22,yoffset - ((bar_over->height - dir_icon->height)/2), bar_over->width,bar_over->height,255);
      }

      if (filelist[i].flags)
      {
        /* directory icon */
        gxDrawTexture(dir_icon,26,yoffset,dir_icon->width,dir_icon->height,255);
        FONT_alignLeft(filelist[i].filename + filelist[i].filename_offset, 12,26+dir_icon->width+6,yoffset+(dir_icon->height-12)/2 + 12, (GXColor)WHITE);
      }
      else
      {
        FONT_alignLeft(filelist[i].filename + filelist[i].filename_offset, 12,26,yoffset+(dir_icon->height-12)/2 + 12, (GXColor)WHITE);
      }

      yoffset += 22;
    }

    /* copy EFB to XFB */
    gxSetScreen ();

  /*  if (redraw) ShowFiles (offset, selection);
    redraw = 0;*/
    p = m_input.keys;

    /* scroll displayed filename */
    if (p & PAD_BUTTON_LEFT)
    {
      if (filelist[selection].filename_offset > 0)
      {
        filelist[selection].filename_offset --;
        //redraw = 1;
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
        //redraw = 1;
      }
    }

    /* highlight next item */
    else if (p & PAD_BUTTON_DOWN)
    {
      filelist[selection].filename_offset = 0;
      selection++;
      if (selection == maxfiles) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
      //redraw = 1;
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
      //redraw = 1;
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
      //redraw = 1;
    }

    /* go forward one page */
    else if (p & PAD_TRIGGER_R)
    {
      filelist[selection].filename_offset = 0;
      selection += PAGESIZE;
      if (selection > maxfiles - 1) selection = offset = 0;
      if ((selection - offset) >= PAGESIZE) offset += PAGESIZE;
      //redraw = 1;
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
      //redraw = 1;
    }
  }

#ifdef HW_RVL
  /* allocate wiimote pointer data (only done once) */
  gxTextureClose(&pointer);
#endif

  gxTextureClose(&overlay->texture);
  gxTextureClose(&bg->texture);
  gxTextureClose(&logo->texture);
  for (i=0; i<2; i++)
  {
    gxTextureClose(&banners[i]->texture);
    gxTextureClose(&helpers[i]->texture);
  }
  gxTextureClose(&frame_left);
  gxTextureClose(&frame_right);
  gxTextureClose(&arrow_up);
  gxTextureClose(&arrow_up_over);
  gxTextureClose(&arrow_down);
  gxTextureClose(&arrow_down_over);
  gxTextureClose(&bar_over);
  gxTextureClose(&dir_icon);
  gxTextureClose(&star_full);
  gxTextureClose(&star_empty);
  gxTextureClose(&snap_frame);
  gxTextureClose(&snap_empty);

  if (quit == 2) return 0;
  else if (useFAT) return FAT_LoadFile(buffer);
  else return DVD_LoadFile(buffer);
}
