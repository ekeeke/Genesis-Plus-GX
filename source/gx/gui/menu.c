/****************************************************************************
 *  menu.c
 *
 *  Genesis Plus GX menu
 *
 *  code by Eke-Eke (march 2009)
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
 ***************************************************************************/

#include "shared.h"
#include "dvd.h"
#include "font.h"
#include "menu.h"
#include "file_dvd.h"
#include "file_fat.h"
#include "filesel.h"

#include <asndlib.h>
#include <oggplayer.h>

/* Global datas */
t_input_menu m_input;

#ifdef HW_RVL
gx_texture *w_pointer;
#endif

#define BG_COLOR_MAX 15

/* various background colors */
static GXColor bg_colors[BG_COLOR_MAX]=
{
  {0xd6,0xcb,0xba,0xff}, /* light gold */
  {0xbb,0xb0,0x99,0xff}, /* gold */
  {0x66,0x66,0x66,0xff}, /* faded grey */
  {0xcc,0xcc,0xcc,0xff}, /* light grey */
  {0xd4,0xd0,0xc8,0xff}, /* cream */
  {0x50,0x51,0x5b,0xff}, /* grey blue */
  {0xb8,0xc7,0xda,0xff}, /* light blue */
  {0xc0,0xcf,0xe7,0xff}, /* sky blue */
  {0x98,0xb1,0xd8,0xff}, /* sea blue */
  {0x7b,0x8c,0xa6,0xff}, /* violet */
  {0xa9,0xc7,0xc6,0xff}, /* green blue */
  {0x7d,0xa4,0x9f,0xff}, /* darker green blue */
  {0x22,0x52,0x74,0xff}, /* dark blue */
  {0x33,0x33,0x33,0xff}, /* dark grey */
  {0x00,0x00,0x00,0xff}  /* black */
};

/*****************************************************************************/
/*  Generic Buttons data                                                      */
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

static butn_data button_text_data =
{
  {NULL,NULL},
  {Button_text_png,Button_text_over_png}
};

static butn_data button_icon_data =
{
  {NULL,NULL},
  {Button_icon_png,Button_icon_over_png}
};

/*****************************************************************************/
/*  Generic GUI items                                                         */
/*****************************************************************************/
static gui_item action_cancel =
{
#ifdef HW_RVL
  NULL,Key_B_wii_png,"","Back",10,422,28,28
#else
  NULL,Key_B_gcn_png,"","Back",10,422,28,28
#endif
};

static gui_item action_select =
{
#ifdef HW_RVL
  NULL,Key_A_wii_png,"","",602,422,28,28
#else
  NULL,Key_A_gcn_png,"","",602,422,28,28
#endif
};


/*****************************************************************************/
/*  Generic GUI backgrounds                                                  */
/*****************************************************************************/
static gui_image bg_main[4] =
{
  {NULL,Bg_main_png,IMAGE_VISIBLE|IMAGE_FADE,146,80,348,288,255,{0,0},{0,0}},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255,{0,0},{0,0}},
  {NULL,Banner_main_png,IMAGE_VISIBLE|IMAGE_SLIDE_Y,0,340,640,140,255,{0,0},{0,0}},
  {NULL,Main_logo_png,IMAGE_VISIBLE|IMAGE_SLIDE_Y,202,362,232,56,255,{0,0},{0,0}}
};

static gui_image bg_misc[5] =
{
  {NULL,Bg_main_png,IMAGE_VISIBLE,146,80,348,288,255,{0,0},{0,0}},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255,{0,0},{0,0}},
  {NULL,Banner_top_png,IMAGE_VISIBLE,0,0,640,108,255,{0,0},{0,0}},
  {NULL,Banner_bottom_png,IMAGE_VISIBLE,0,380,640,100,255,{0,0},{0,0}},
  {NULL,Main_logo_png,IMAGE_VISIBLE,466,40,152,44,255,{0,0},{0,0}}
};

static gui_image bg_list[6] =
{
  {NULL,Bg_main_png,IMAGE_VISIBLE,356,144,348,288,255,{0,0},{0,0}},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255,{0,0},{0,0}},
  {NULL,Banner_top_png,IMAGE_VISIBLE,0,0,640,108,255,{0,0},{0,0}},
  {NULL,Banner_bottom_png,IMAGE_VISIBLE,0,380,640,100,255,{0,0},{0,0}},
  {NULL,Main_logo_png,IMAGE_VISIBLE,466,40,152,44,255,{0,0},{0,0}},
  {NULL,Frame_s1_png,IMAGE_VISIBLE,8,70,372,336,128,{0,0},{0,0}}
};

/*****************************************************************************/
/*  Menu Items description                                                   */
/*****************************************************************************/

/* Main menu */
static gui_item items_main[9] =
{
  {NULL,Main_quit_png    ,"","",128, 84,52,80},
  {NULL,Main_load_png    ,"","",280, 72,80,92},
  {NULL,Main_options_png ,"","",456, 76,60,88},
  {NULL,Main_file_png    ,"","",114,216,80,92},
  {NULL,Main_reset_png   ,"","",282,224,76,84},
  {NULL,Main_ggenie_png  ,"","",450,224,72,84},
#ifdef HW_RVL
  {NULL,Main_play_wii_png,"","", 10,372,84,32},
#else
  {NULL,Main_play_gcn_png,"","", 10,372,84,32},
#endif
  {NULL,Main_takeshot_png,"","",546,334,84,32},
  {NULL,Main_showinfo_png,"","",546,372,84,32}
};

#ifdef HW_RVL
static gui_item items_load[4] =
{
  {NULL,Load_recent_png,"","Load recent files",             276,120,88,96},
  {NULL,Load_sd_png    ,"","Load ROM files from SDCARD",    110,266,88,96},
  {NULL,Load_usb_png   ,"","Load ROM files from USB device",276,266,88,96},
  {NULL,Load_dvd_png   ,"","Load ROM files from DVD",       442,266,88,96}
};
#else
static gui_item items_load[3] =
{
  {NULL,Load_recent_png,"","Load recent files",         110,198,88,96},
  {NULL,Load_sd_png    ,"","Load ROM files from SDCARD",276,198,88,96},
  {NULL,Load_dvd_png   ,"","Load ROM files from DVD",   442,198,88,96}
};
#endif

static gui_item items_options[5] =
{
  {NULL,Option_system_png,"","System settings", 114,142,80,92},
  {NULL,Option_video_png ,"","Video settings",  288,150,64,84},
  {NULL,Option_sound_png ,"","Audio settings",  464,154,44,80},
  {NULL,Option_ctrl_png  ,"","Input settings",  192,286,88,92},
  {NULL,Option_menu_png  ,"","Menu settings",   370,286,60,92}
};

/* Audio options menu */
static gui_item items_audio[5] =
{
  {NULL,NULL,"PSG Volume: 2.50",   "Adjust PSG output level",             0,0,0,0},
  {NULL,NULL,"FM Volume: 1.00",    "Adjust FM output level",              0,0,0,0},
  {NULL,NULL,"Volume Boost: 1x",   "Adjust general output level",         0,0,0,0},
  {NULL,NULL,"LowPass Filter: OFF","Enable/disable sound filtering",      0,0,0,0},
  {NULL,NULL,"HQ YM2612: LINEAR",  "Enable/disable FM chip interpolation",0,0,0,0}
};

/* System options menu */
static gui_item items_system[4] =
{
  {NULL,NULL,"Console Region: AUTO","Select system region",                   0,0,0,0},
  {NULL,NULL,"System Lockups: OFF", "Enable/disable original system lock-ups",0,0,0,0},
  {NULL,NULL,"System BIOS: OFF",    "Enable/disable TMSS BIOS support",       0,0,0,0},
  {NULL,NULL,"SVP Cycles: 1500",    "Adjust SVP chip emulation speed",        0,0,0,0}
};

/* Video options menu */
static gui_item items_video[8] =
{
  {NULL,NULL,"Aspect: STRETCHED",     "Select display aspect ratio",               0,0,0,0},
  {NULL,NULL,"Display: PROGRESSIVE",  "Select video mode type",                    0,0,0,0},
  {NULL,NULL,"TV mode: 50/60Hz",      "Select video refresh rate",                 0,0,0,0},
  {NULL,NULL,"Bilinear Filter: OFF",  "Enable/disable hardware filtering",         0,0,0,0},
  {NULL,NULL,"NTSC Filter: COMPOSITE","Enable/disable NTSC software filtering",    0,0,0,0},
  {NULL,NULL,"Borders: OFF",          "Enable/disable original overscan emulation",0,0,0,0},
  {NULL,NULL,"DISPLAY POSITION",      "Adjust display position",                   0,0,0,0},
  {NULL,NULL,"DISPLAY SIZE",          "Adjust display size",                       0,0,0,0}
};

/* Preferences menu */
static gui_item items_prefs[5] =
{
  {NULL,NULL,"Auto SRAM: OFF",    "Enable/disable automatic SRAM",      0,0,0,0},
  {NULL,NULL,"Auto STATE: OFF",   "Enable/disable automatic Savestate", 0,0,0,0},
  {NULL,NULL,"SFX Volume: 100",   "Adjust sound effects volume",        0,0,0,0},
  {NULL,NULL,"BGM Volume: 100",   "Adjust background music volume",     0,0,0,0},
  {NULL,NULL,"BG Color: DEFAULT", "Change background color",            0,0,0,0}
};

/*****************************************************************************/
/*  Menu Buttons description                                                 */
/*****************************************************************************/

/* Generic Buttons for list menu */
static gui_butn arrow_up = {&arrow_up_data,BUTTON_OVER_SFX,{0,0},14,76,360,32};
static gui_butn arrow_down = {&arrow_down_data,BUTTON_VISIBLE|BUTTON_OVER_SFX,{0,0},14,368,360,32};

/* Generic list menu */
static gui_butn buttons_list[4] =
{
  {&button_text_data,BUTTON_VISIBLE|BUTTON_OVER_SFX,{1,1},52,132,276,48},
  {&button_text_data,BUTTON_VISIBLE|BUTTON_OVER_SFX,{1,1},52,188,276,48},
  {&button_text_data,BUTTON_VISIBLE|BUTTON_OVER_SFX,{1,1},52,244,276,48},
  {&button_text_data,BUTTON_VISIBLE|BUTTON_OVER_SFX,{1,1},52,300,276,48}
 };

/* Main menu */
static gui_butn buttons_main[9] =
{
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3}, 80, 50,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3},246, 50,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3},412, 50,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,3}, 80,194,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX                  ,{3,3},246,194,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,2},412,194,148,132},
  {NULL             ,BUTTON_VISIBLE|BUTTON_OVER_SFX                  ,{3,0},  0,360, 88, 48},
  {NULL             ,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{2,1},542,330, 88, 38},
  {NULL             ,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,0},542,370, 88, 48}
};


/* Load Game menu */
#ifdef HW_RVL
static gui_butn buttons_load[4] =
{
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,2},246,102,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,0}, 80,248,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{2,0},246,248,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,0},412,248,148,132}
};
#else
static gui_butn buttons_load[3] =
{
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,0}, 80,180,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,0},246,180,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,0},412,180,148,132}
};
#endif

/* Options menu */
static gui_butn buttons_options[5] =
{
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3}, 80,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3},246,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,2},412,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,0},162,264,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{2,0},330,264,148,132}
};

/*****************************************************************************/
/*  Menu descriptions                                                        */
/*****************************************************************************/

/* Main menu */
static gui_menu menu_main =
{
  "",
  0,0,
  9,9,4,
  items_main,
  buttons_main,
  bg_main,
  {NULL,NULL},
  {NULL,NULL},
  FALSE
};

/* Load Game menu */
static gui_menu menu_load =
{
  "Load Game",
  0,0,
#ifdef HW_RVL
  4,4,5,
#else
  3,3,5,
#endif
  items_load,
  buttons_load,
  bg_misc,
  {&action_cancel, &action_select},
  {NULL,NULL},
  FALSE
};

/* Options menu */
static gui_menu menu_options =
{
  "Settings",
  0,0,
  5,5,5,
  items_options,
  buttons_options,
  bg_misc,
  {&action_cancel, &action_select},
  {NULL,NULL},
  FALSE
};

/* System Options menu */
static gui_menu menu_system =
{
  "System Settings",
  0,0,
  4,4,6,
  items_system,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {NULL,NULL},
  FALSE
};

/* Video Options menu */
static gui_menu menu_video =
{
  "Video Settings",
  0,0,
  8,4,6,
  items_video,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  FALSE
};

/* Sound Options menu */
static gui_menu menu_audio =
{
  "Sound Settings",
  0,0,
  5,4,6,
  items_audio,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  FALSE
};

/* Sound Options menu */
static gui_menu menu_prefs =
{
  "Menu Settings",
  0,0,
  5,4,6,
  items_prefs,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  FALSE
};

/*****************************************************************************/
/*  Generic GUI routines                                                     */
/*****************************************************************************/
/* Allocate texture images data */
void GUI_InitMenu(gui_menu *menu)
{
  int i;
  gui_item *item;
  gui_butn *button;
  gui_image *image;

  /* background elements */
  for (i=0; i<menu->max_images; i++)
  {
    image = &menu->bg_images[i];
    image->texture = gxTextureOpenPNG(image->data,0);
  }

  for (i=0; i<2; i++)
  {
    /* key helpers */
    item = menu->helpers[i];
    if (item)
    {
      item->texture = gxTextureOpenPNG(item->data,0);
    }

    /* arrows */
    button = menu->arrows[i];
    if (button)
    {
      if (!button->data->texture[0]) button->data->texture[0] = gxTextureOpenPNG(button->data->image[0],0);
      if (!button->data->texture[1]) button->data->texture[1] = gxTextureOpenPNG(button->data->image[1],0);

      /* initial state */
      button->state &= ~BUTTON_VISIBLE;
      if (((i==0) && (abs(menu->offset) > 0)) ||
          ((i==1) && (menu->offset + menu->max_buttons) < menu->max_items))
      {
        button->state |= BUTTON_VISIBLE;
      }
    }
  }

  /* menu buttons */
  for (i=0; i<menu->max_buttons; i++)
  {
    button = &menu->buttons[i];
    if (button->data)
    {
      if (!button->data->texture[0]) button->data->texture[0] = gxTextureOpenPNG(button->data->image[0],0);
      if (!button->data->texture[1]) button->data->texture[1] = gxTextureOpenPNG(button->data->image[1],0);
    }
  }

  /* menu items */
  for (i=0; i<menu->max_items; i++)
  {
    item = &menu->items[i];
    if (item->data) item->texture = gxTextureOpenPNG(item->data,0);
  }
}

/* release allocated memory */
void GUI_DeleteMenu(gui_menu *menu)
{
  int i;
  gui_butn *button;
  gui_item *item;
  gui_image *image;

  /* background elements */
  for (i=0; i<menu->max_images; i++)
  {
    image = &menu->bg_images[i];
    gxTextureClose(&image->texture);
  }

  for (i=0; i<2; i++)
  {
    /* key helpers */
    item = menu->helpers[i];
    if (item)
    {
      gxTextureClose(&item->texture);
    }

    /* arrows */
    button = menu->arrows[i];
    if (button)
    {
      gxTextureClose(&button->data->texture[0]);
      gxTextureClose(&button->data->texture[1]);
    }
  }

  /* menu buttons */
  for (i=0; i<menu->max_buttons; i++)
  {
    button = &menu->buttons[i];
    if (button->data)
    {
      gxTextureClose(&button->data->texture[0]);
      gxTextureClose(&button->data->texture[1]);
    }
  }

  /* menu items */
  for (i=0; i<menu->max_items; i++)
  {
    item = &menu->items[i];
    gxTextureClose(&item->texture);
  }
}

void GUI_DrawMenu(gui_menu *menu)
{
  int i;
  gui_item *item;
  gui_butn *button;
  gui_image *image;

  /* background color */
  if (menu->screenshot)
  {
    gxClearScreen((GXColor)BLACK);
    gxDrawScreenshot(128);
  }
  else
  {
    gxClearScreen(bg_colors[config.bg_color]);
  }

  /* background elements */
  for (i=0; i<menu->max_images; i++)
  {
    image = &menu->bg_images[i];
    if (image->state & IMAGE_REPEAT) gxDrawTextureRepeat(image->texture,image->x,image->y,image->w,image->h,image->alpha);
    else if (image->state & IMAGE_VISIBLE)gxDrawTexture(image->texture,image->x,image->y,image->w,image->h,image->alpha);
  }

  /* menu title */
  FONT_write(menu->title, 22,10,56,640,(GXColor)WHITE);

  /* draw buttons + items */
  for (i=0; i<menu->max_buttons; i++)
  {
    button = &menu->buttons[i];

    if (button->state & BUTTON_VISIBLE)
    {
      /* draw button + items */ 
      item = &menu->items[menu->offset +i];
      if (i == menu->selected)
      {
        if (button->data) gxDrawTexture(button->data->texture[1],button->x-4,button->y-4,button->w+8,button->h+8,255);
        if (item->data) gxDrawTexture(item->texture, item->x-4,item->y-4,item->w+8,item->h+8,255);
        else FONT_writeCenter(item->text,18,button->x,button->x+button->w,button->y+(button->h-18)/2+18,(GXColor)DARK_GREY);

        /* update help comment */
        if (menu->helpers[1]) strcpy(menu->helpers[1]->comment,item->comment);
      }
      else
      {
        if (button->data) gxDrawTexture(button->data->texture[0],button->x,button->y,button->w, button->h,255);
        if (item->data) gxDrawTexture(item->texture,item->x,item->y,item->w,item->h,255);
        else FONT_writeCenter(item->text,16,button->x,button->x+button->w,button->y+(button->h - 16)/2+16,(GXColor)DARK_GREY);
      }
    }
  }

  /* draw arrow */
  for (i=0; i<2; i++)
  {
    button = menu->arrows[i];
    if (button)
    {
      if (button->state & BUTTON_VISIBLE)
      {
        if (menu->selected == (menu->max_buttons + i))
        {
          gxDrawTexture(button->data->texture[1],button->x-2,button->y-2,button->w+4,button->h+4,255);
        }
        else
        {
          gxDrawTexture(button->data->texture[0],button->x,button->y,button->w, button->h,255);
        }
      }
    }
  }

  /* left comment */
  item = menu->helpers[0];
  if (item)
  {
    gxDrawTexture(item->texture,item->x,item->y,item->w,item->h,255);
    FONT_write(item->comment,16,item->x+item->w+6,item->y+(item->h-16)/2 + 16,640,(GXColor)WHITE);
  }

  /* right comment */
  item = menu->helpers[1];
  if (item)
  {
    if (menu->selected  < menu->max_buttons)
    {
      gxDrawTexture(item->texture,item->x,item->y,item->w,item->h,255);
      FONT_alignRight(item->comment,16,item->x-6,item->y+(item->h-16)/2+16,(GXColor)WHITE);
    }
  }
}

/* Menu fading */
void GUI_FadeMenu(gui_menu *menu, u8 speed, u8 out)
{
  int alpha = 255;

  while (alpha > 0)
  {
    GUI_DrawMenu(menu);
    gxDrawRectangle(0, 0, 640, 480, out ? (255-alpha) : alpha, (GXColor)BLACK);
    alpha -= speed;
  }

  if (out) gxDrawRectangle(0, 0, 640, 480, 255, (GXColor)BLACK);
}

/* Menu transitions effect */
void GUI_DrawMenuFX(gui_menu *menu, u8 speed, u8 out)
{
  int i,temp,xpos,ypos;
  int max_offset = 0;
  gui_item *item;
  gui_butn *button;
  gui_image *image;

  /* find maximal offset */
  for (i=0; i<menu->max_images; i++)
  {
    image = &menu->bg_images[i];

    if (image->state & IMAGE_SLIDE_X)
    {
      temp = (image->x > 320) ? (640 - image->x) : (image->x + image->w);
      if (max_offset < temp) max_offset = temp;
    }

    if (image->state & IMAGE_SLIDE_Y)
    {
      temp = (image->y > 240) ? (480 - image->y) : (image->y + image->h);
      if (max_offset < temp) max_offset = temp;
    }
  }

  /* set position ranges*/
  for (i=0; i<menu->max_images; i++)
  {
    image = &menu->bg_images[i];

    if (image->state & IMAGE_SLIDE_X)
    {
      image->xrange[out] = (image->x > 320) ? (image->x + max_offset) : (image->x - max_offset);
      image->xrange[out^1] = image->x;
    }

    if (image->state & IMAGE_SLIDE_Y)
    {
      image->yrange[out] = (image->y > 240) ? (image->y + max_offset) : (image->y - max_offset);
      image->yrange[out^1] = image->y;
    }
  }

  /* Alpha steps */
  int alpha = out ? 255 : 0;
  int alpha_step = (255 * speed) / max_offset;
  if (out) alpha_step = -alpha_step;

  /* Intialize Menu */
  GUI_InitMenu(menu);

  /* Let's loop until final position has been reached */
  while (max_offset > 0)
  {
    /* background color */
    if (menu->screenshot)
    {
      gxClearScreen((GXColor)BLACK);
      if (alpha > 127) gxDrawScreenshot(128);
      else gxDrawScreenshot(255 - alpha);
    }
    else
    {
      gxClearScreen(bg_colors[config.bg_color]);
    }

    /* background elements */
    for (i=0; i<menu->max_images; i++)
    {
      image = &menu->bg_images[i];

      /* default position */
       xpos = image->x;
       ypos = image->y;

      /* update position */
      if (image->state & IMAGE_SLIDE_X)
      {
        xpos = (image->xrange[0] < image->xrange[1]) ? (image->xrange[1] - max_offset) : (image->xrange[1] + max_offset);
      }
      if (image->state & IMAGE_SLIDE_Y)
      {
        ypos = (image->yrange[0] < image->yrange[1]) ? (image->yrange[1] - max_offset) : (image->yrange[1] + max_offset);
      }

      if ((image->state & IMAGE_FADE) && ((out && (image->alpha > alpha)) || (!out && (image->alpha < alpha))))
      {
        /* FADE In-Out */
        if (image->state & IMAGE_REPEAT) gxDrawTextureRepeat(image->texture,xpos,ypos,image->w,image->h,alpha);
        else if (image->state & IMAGE_VISIBLE)gxDrawTexture(image->texture,xpos,ypos,image->w,image->h,alpha);
      }
      else
      {
        if (image->state & IMAGE_REPEAT) gxDrawTextureRepeat(image->texture,xpos,ypos,image->w,image->h,image->alpha);
        else if (image->state & IMAGE_VISIBLE)gxDrawTexture(image->texture,xpos,ypos,image->w,image->h,image->alpha);
      }
    }

    /* draw buttons + items */
    for (i=0; i<menu->max_buttons; i++)
    {
      button = &menu->buttons[i];

      if (button->state & BUTTON_VISIBLE)
      {
        /* draw button + items */ 
        item = &menu->items[menu->offset + i];
        if (button->data) gxDrawTexture(button->data->texture[0],button->x,button->y,button->w, button->h,alpha);
        if (item->data) gxDrawTexture(item->texture,item->x,item->y,item->w,item->h,alpha);
      }
    }

    /* update offset */
    max_offset -= speed;

    /* update alpha */
    alpha += alpha_step;
    if (alpha > 255) alpha = 255;
    else if (alpha < 0) alpha = 0;

    /* copy EFB to XFB */
    gxSetScreen ();
  }

  /* final position */
  if (!out) 
  {
    GUI_DrawMenu(menu);
    gxSetScreen ();
  }
  else if (menu->screenshot)
  {
    gxClearScreen((GXColor)BLACK);
    gxDrawScreenshot(255);
    gxSetScreen ();
  }

  GUI_DeleteMenu(menu);
}

/* Window Prompt  */
/* prompt window slides in & out */
int GUI_WindowPrompt(gui_menu *parent, char *title, char *items[], u8 nb_items)
{
  int i, ret, quit = 0;
  s32 selected = 0;
  s32 old;
  butn_data *data = &button_text_data;
  u8 delete_me[2];
  s16 p;

#ifdef HW_RVL
  int x,y;
  struct orient_t orient;
#endif

  /* initialize data */
  for (i=0; i<2; i++)
  {
    delete_me[i] = 0;
    if (!data->texture[i])
    {
      data->texture[i] = gxTextureOpenPNG(data->image[i],0);
      delete_me[i] = 1;
    }
  }

  /* initialize texture window */
  gx_texture *window = gxTextureOpenPNG(Frame_s1_png,0);
  gx_texture *top = gxTextureOpenPNG(Frame_s1_title_png,0);

  /* get initial positions */
  int w = data->texture[0]->width;
  int h = data->texture[0]->height;
  int xwindow = (640 - window->width)/2;
  int ywindow = (480 - window->height)/2;
  int xpos = xwindow + (window->width - w)/2;
  int ypos = (window->height - top->height - (h*nb_items) - (nb_items-1)*20)/2;
  ypos = ypos + ywindow + top->height;

  /* set initial vertical offset */
  int yoffset = ywindow + window->height;

  /* slide in */
  parent->selected = -1;
  while (yoffset > 0)
  {
    /* draw parent menu */
    GUI_DrawMenu(parent);

    /* draw window */
    gxDrawTexture(window,xwindow,ywindow-yoffset,window->width,window->height,230);
    gxDrawTexture(top,xwindow,ywindow-yoffset,top->width,top->height,255);

    /* draw title */
    FONT_writeCenter(title,20,xwindow,xwindow+window->width,ywindow+(top->height-20)/2+20-yoffset,(GXColor)WHITE);

    /* draw buttons + text */
    for (i=0; i<nb_items; i++)
    {
      gxDrawTexture(data->texture[0],xpos,ypos+i*(20 + h)-yoffset,w,h,255);
      FONT_writeCenter(items[i],18,xpos,xpos+w,ypos+i*(20 + h)+(h + 18)/2- yoffset,(GXColor)DARK_GREY);
    }

    /* update display */
    gxSetScreen ();

    /* slide speed */
    yoffset -= 60;
  }

  /* draw menu  */
  while (quit == 0)
  {
    /* draw parent menu (should have been initialized first) */
    GUI_DrawMenu(parent);

    /* draw window */
    gxDrawTexture(window,xwindow,ywindow,window->width,window->height,230);
    gxDrawTexture(top,xwindow,ywindow,top->width,top->height,255);

    /* draw title */
    FONT_writeCenter(title,20,xwindow,xwindow+window->width,ywindow+(top->height-20)/2+20,(GXColor)WHITE);

    /* draw buttons + text */
    for (i=0; i<nb_items; i++)
    {
      if (i==selected)
      {
        gxDrawTexture(data->texture[1],xpos-4,ypos+i*(20+h)-4,w+8,h+8,255);
        FONT_writeCenter(items[i],22,xpos,xpos+w,ypos+i*(20+h)+(h+22)/2,(GXColor)DARK_GREY);
      }
      else
      {
        gxDrawTexture(data->texture[0],xpos,ypos+i*(20 + h),w,h,255);
        FONT_writeCenter(items[i],18,xpos,xpos+w,ypos+i*(20+h)+(h+18)/2,(GXColor)DARK_GREY);
      }
    }

    old = selected;
    p = m_input.keys;

#ifdef HW_RVL
    if (Shutdown)
    {
      gxTextureClose(&w_pointer);
      GUI_DeleteMenu(parent);
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
      gxDrawTexture(w_pointer,x-w_pointer->width/2,y-w_pointer->height/2,w_pointer->width,w_pointer->height,255);
      gxResetAngle(0.0);

      /* check for valid buttons */
      selected = -1;
      for (i=0; i<nb_items; i++)
      {
        if ((x>=xpos)&&(x<=(xpos+w))&&(y>=ypos+i*(20 + h))&&(y<=(ypos+i*(20+h)+h)))
        {
          selected = i;
          break;
        }
      }
    }
#endif

    /* update screen */
    gxSetScreen ();

    /* update selection */
    if (p&PAD_BUTTON_UP)
    {
      if (selected > 0) selected --;
    }
    else if (p&PAD_BUTTON_DOWN)
    {
      if (selected < (nb_items -1)) selected ++;
    }

    /* sound fx */
    if (selected != old)
    {
      if (selected >= 0)
      {
        ASND_SetVoice(ASND_GetFirstUnusedVoice(),VOICE_MONO_16BIT,22050,0,(u8 *)button_over_pcm,button_over_pcm_size,
                      ((int)config.sfx_volume * 255) / 100,((int)config.sfx_volume * 255) / 100,NULL);
      }
    }

    if (p & PAD_BUTTON_A)
    {
      if (selected >= 0)
      {
        quit = 1;
        ret = selected;
      }
    }
    else if (p & PAD_BUTTON_B)
    {
      quit = 1;
      ret = -1;
    }
  }

  /* reset initial vertical offset */
  yoffset = 0;

  /* slide out */
  while (yoffset < (ywindow + window->height))
  {
    /* draw parent menu */
    GUI_DrawMenu(parent);

    /* draw window + header */
    gxDrawTexture(window,xwindow,ywindow-yoffset,window->width,window->height,230);
    gxDrawTexture(top,xwindow,ywindow-yoffset,top->width,top->height,255);

    /* draw title */
    FONT_writeCenter(title,20,xwindow,xwindow+window->width,ywindow+(top->height-20)/2+20-yoffset,(GXColor)WHITE);

    /* draw buttons + text */
    for (i=0; i<nb_items; i++)
    {
      gxDrawTexture(data->texture[0],xpos,ypos+i*(20+h)-yoffset,w,h,255);
      FONT_writeCenter(items[i],18,xpos,xpos+w,ypos+i*(20+h)+(h+18)/2-yoffset,(GXColor)WHITE);
    }

    yoffset += 60;
    gxSetScreen ();
  }

  /* close textures */
  gxTextureClose(&window);
  if (delete_me[0]) gxTextureClose(&data->texture[0]);
  if (delete_me[1]) gxTextureClose(&data->texture[1]);

  return ret;
}

int GUI_RunMenu(gui_menu *menu)
{
  int selected,quit=0;
  gui_butn *button;
  u16 p;
  u16 max_buttons = menu->max_buttons;
  u16 max_items = menu->max_items;

#ifdef HW_RVL
  int i,x,y;
  struct orient_t orient;
#endif


  while(quit==0)
  {
    GUI_DrawMenu(menu);
    selected = menu->selected;

#ifdef HW_RVL
    if (Shutdown)
    {
      gxTextureClose(&w_pointer);
      GUI_DeleteMenu(menu);
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
      gxDrawTexture(w_pointer, x-w_pointer->width/2, y-w_pointer->height/2, w_pointer->width, w_pointer->height,255);
      gxResetAngle(0.0);

      /* check for valid buttons */
      selected = max_buttons + 2;
      for (i=0; i<max_buttons; i++)
      {
        button = &menu->buttons[i];
        if ((button->state & BUTTON_VISIBLE)&&(x>=button->x)&&(x<=(button->x+button->w))&&(y>=button->y)&&(y<=(button->y+button->h)))
        {
          selected = i;
          break;
        }
      }

      for (i=0; i<2; i++)
      {
        button = menu->arrows[i];
        if (button)
        {
          if (button->state & BUTTON_VISIBLE)
          {
            if ((x<=(button->x+button->w))&&(y>=button->y)&&(y<=(button->y+button->h)))
            {
              selected = max_buttons + i;
              break;
            }
          }
        }
      }
    }
    else
    {
      /* reinitialize selection */
      if (selected >= menu->max_buttons) selected = 0;
    }
#endif

    /* update screen */
    gxSetScreen ();

    /* update menu */
    p = m_input.keys;

    if (selected < max_buttons)
    {
      if (p & PAD_BUTTON_UP)
      {
        button = &menu->buttons[selected];
        if (button->shift[0])
        {
          selected -= button->shift[0];
          if (selected < 0)
          {
            selected = 0;
            if (menu->offset) menu->offset --;
          }
        }
      }
      else if (p & PAD_BUTTON_DOWN)
      {
        button = &menu->buttons[selected];
        if (button->shift[1])
        {
          selected += button->shift[1];
          if (selected >= max_buttons)
          {
            selected = max_buttons - 1;
            if ((menu->offset + selected < (max_items - 1))) menu->offset ++;
          }
        }
      }
      else if (p & PAD_BUTTON_LEFT)
      {
        if (max_buttons == max_items)
        {
          selected --;
          if (selected < 0) selected = 0;
        }
        else
        {
          quit = -1;
        }
      }
      else if (p & PAD_BUTTON_RIGHT)
      {
        if (max_buttons == max_items)
        {
          selected ++;
          if (selected >= max_buttons) selected = max_buttons - 1;
        }
        else
        {
          quit = 1;
        }
      }
      else if (p & PAD_BUTTON_LEFT)
      {
        if (max_buttons == max_items)
        {
          selected --;
          if (selected < 0) selected = 0;
        }
        else
        {
          quit = -1;
        }
      }
      else if (p & PAD_BUTTON_RIGHT)
      {
        if (max_buttons == max_items)
        {
          selected ++;
          if (selected >= max_buttons) selected = max_buttons - 1;
        }
        else
        {
          quit = 1;
        }
      }
    }

    if (p & PAD_BUTTON_A)
    {
      if (selected < max_buttons) quit = 1;
      else if (selected == max_buttons) menu->offset --; /* up arrow */
      else if (selected == (max_buttons+1))menu->offset ++; /* down arrow */
    }
    else if ((p & PAD_BUTTON_B) || (p & PAD_TRIGGER_Z))
    {
      quit = 2;
    }

    /* selected item has changed ? */
    if (menu->selected != selected)
    {
      if (selected < max_buttons)
      {
        /* sound fx */
        button = &menu->buttons[selected];
        if (button->state & BUTTON_OVER_SFX)
        {
          ASND_SetVoice(ASND_GetFirstUnusedVoice(),VOICE_MONO_16BIT,22050,0,(u8 *)button_over_pcm,button_over_pcm_size,
                        ((int)config.sfx_volume * 255) / 100,((int)config.sfx_volume * 255) / 100,NULL);
        }
      }
      else if (selected < (max_buttons + 2))
      {
        /* sound fx */
        button = menu->arrows[selected-max_buttons];
        if (button->state & BUTTON_OVER_SFX)
        {
          ASND_SetVoice(ASND_GetFirstUnusedVoice(),VOICE_MONO_16BIT,22050,0,(u8 *)button_over_pcm,button_over_pcm_size,
                        ((int)config.sfx_volume * 255) / 100,((int)config.sfx_volume * 255) / 100,NULL);
        }
      }

      /* update selection */
      menu->selected = selected;
    }

    /* update arrows buttons status (items list) */
    button = menu->arrows[0];
    if (button)
    {
      if (menu->offset > 0) button->state |= BUTTON_VISIBLE;
      else button->state &= ~BUTTON_VISIBLE;
    }
    button = menu->arrows[1];
    if (button)
    {
      if ((menu->offset + max_buttons) < max_items) button->state |= BUTTON_VISIBLE;
      else button->state &= ~BUTTON_VISIBLE;
    }
  }

  if (quit < 2)
  {
    if (selected < max_buttons)
    {
      /* sound fx */
      button = &menu->buttons[selected];
      if (button->state & BUTTON_SELECT_SFX)
      {
        ASND_SetVoice(ASND_GetFirstUnusedVoice(),VOICE_MONO_16BIT,22050,0,(u8 *)button_select_pcm,button_select_pcm_size,
                      ((int)config.sfx_volume * 255) / 100,((int)config.sfx_volume * 255) / 100,NULL);
      }
    }

    /* return item selection index */
    if (quit < 0) return -2-menu->offset-menu->selected;
    else return (menu->offset + menu->selected);
  }

  /* leave menu (default) */
  return -1;
 }

/* basic slide effect for option menus */
static void GUI_SlideMenuTitle(gui_menu *m, int title_offset)
{
#ifdef HW_RVL
  gui_butn *button;
  int i,x,y;
  struct orient_t orient;
#endif

  char title[64];
  strcpy(title,m->title);

  while (title_offset > 0)
  {
    strcpy(m->title,title+title_offset);
    m->title[strlen(title)-title_offset-1] = 0;
    GUI_DrawMenu(m);
#ifdef HW_RVL
    if (m_input.ir.valid)
    {
      /* get cursor position */
      x = m_input.ir.x;
      y = m_input.ir.y;

      /* draw wiimote pointer */
      WPAD_Orientation(0,&orient);
      gxResetAngle(orient.roll);
      gxDrawTexture(w_pointer, x-w_pointer->width/2, y-w_pointer->height/2, w_pointer->width, w_pointer->height,255);
      gxResetAngle(0.0);

      /* check for valid buttons */
      m->selected = m->max_buttons + 2;
      for (i=0; i<m->max_buttons; i++)
      {
        button = &m->buttons[i];
        if ((button->state & BUTTON_VISIBLE)&&(x>=button->x)&&(x<=(button->x+button->w))&&(y>=button->y)&&(y<=(button->y+button->h)))
        {
          m->selected = i;
          break;
        }
      }

      for (i=0; i<2; i++)
      {
        button = m->arrows[i];
        if (button)
        {
          if (button->state & BUTTON_VISIBLE)
          {
            if ((x<=(button->x+button->w))&&(y>=button->y)&&(y<=(button->y+button->h)))
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
      /* reinitialize selection */
      if (m->selected >= m->max_buttons) m->selected = 0;
    }
#endif
    gxSetScreen ();
    usleep(6000);
    title_offset--;
  }
  strcpy(m->title,title);
}

/***************************************************************************
 * drawmenu (deprecated)
 *
 * As it says, simply draws the menu with a highlight on the currently
 * selected item :)
 ***************************************************************************/
char menutitle[60] = { "" };
static int menu = 0;

static void drawmenu (char items[][25], int maxitems, int selected)
{

  int i;
  int ypos;

  ypos = (226 - (fheight * maxitems)) >> 1;
  ypos += 130;

  /* reset texture data */
  gx_texture *texture;
  memset(&texture,0,sizeof(gx_texture));

  /* draw background items */
  gxClearScreen (bg_colors[config.bg_color]);
  texture= gxTextureOpenPNG(Bg_main_png,0);
  if (texture)
  {
    gxDrawTexture(texture, (640-texture->width)/2, (480-texture->height)/2, texture->width, texture->height,255);
    if (texture->data) free(texture->data);
    free(texture);
  }
  texture= gxTextureOpenPNG(Banner_bottom_png,0);
  if (texture)
  {
    gxDrawTexture(texture, 0, 480-texture->height, texture->width, texture->height, 255);
    if (texture->data) free(texture->data);
    free(texture);
  }
  texture= gxTextureOpenPNG(Banner_top_png,0);
  if (texture)
  {
    gxDrawTexture(texture, 0, 0, texture->width, texture->height, 255);
    if (texture->data) free(texture->data);
    free(texture);
  }
  texture= gxTextureOpenPNG(Main_logo_png,0);
  if (texture)
  {
    gxDrawTexture(texture, 444, 28, 176, 48, 255);
    if (texture->data) free(texture->data);
    free(texture);
  }

  for (i = 0; i < maxitems; i++)
  {
    if (i == selected) WriteCentre_HL (i * fheight + ypos, (char *) items[i]);
    else WriteCentre (i * fheight + ypos, (char *) items[i]);
  }

  gxSetScreen ();
}

static int domenu (char items[][25], int maxitems, u8 fastmove)
{
  int redraw = 1;
  int quit = 0;
  short p;
  int ret = 0;


  while (quit == 0)
  {
    if (redraw)
    {
      drawmenu (&items[0], maxitems, menu);
      redraw = 0;
    }
    
    p = m_input.keys;
    
    if (p & PAD_BUTTON_UP)
    {
      redraw = 1;
      menu--;
      if (menu < 0) menu = maxitems - 1;
    }
    else if (p & PAD_BUTTON_DOWN)
    {
      redraw = 1;
      menu++;
      if (menu == maxitems) menu = 0;
    }

    if (p & PAD_BUTTON_A)
    {
      quit = 1;
      ret = menu;
    }
    else if (p & PAD_BUTTON_B)
    {
      quit = 1;
      ret = -1;
    }

    if (fastmove)
    {
      if (p & PAD_BUTTON_RIGHT)
      {
        quit = 1;
        ret = menu;
      }
      else if (p & PAD_BUTTON_LEFT)
      {
        quit = 1;
        ret = 0 - 2 - menu;
      }
    }
  }

  return ret;
}

/****************************************************************************
 * Preferences menu
 *
 ****************************************************************************/
static void prefmenu ()
{
  int ret, quit = 0;
  gui_menu *m = &menu_prefs;
  gui_item *items = m->items;

  if (config.sram_auto == 0) sprintf (items[0].text, "SRAM Auto: FAT");
  else if (config.sram_auto == 1) sprintf (items[0].text, "SRAM Auto: MCARD A");
  else if (config.sram_auto == 2) sprintf (items[0].text, "SRAM Auto: MCARD B");
  else sprintf (items[0].text, "SRAM Auto: OFF");
  if (config.state_auto == 0) sprintf (items[1].text, "Savestate Auto: FAT");
  else if (config.state_auto == 1) sprintf (items[1].text, "Savestate Auto: MCARD A");
  else if (config.state_auto == 2) sprintf (items[1].text, "Savestate Auto: MCARD B");
  else sprintf (items[1].text, "Savestate Auto: OFF");
  sprintf (items[2].text, "SFX Volume: %1.1f", config.sfx_volume);
  sprintf (items[3].text, "BGM Volume: %1.1f", config.bgm_volume);
  if (config.bg_color) sprintf (items[4].text, "BG Color: Type %d", config.bg_color);
  else sprintf (items[4].text, "BG Color: DEFAULT");

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("Menu "));

  while (quit == 0)
  {
    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0:  /*** SRAM auto load/save ***/
        config.sram_auto ++;
        if (config.sram_auto > 2) config.sram_auto = -1;
        if (config.sram_auto == 0) sprintf (items[0].text, "SRAM Auto: FAT");
        else if (config.sram_auto == 1) sprintf (items[0].text, "SRAM Auto: MCARD A");
        else if (config.sram_auto == 2) sprintf (items[0].text, "SRAM Auto: MCARD B");
        else sprintf (items[0].text, "SRAM Auto: OFF");
        break;

      case 1:   /*** Savestate auto load/save ***/
        config.state_auto ++;
        if (config.state_auto > 2) config.state_auto = -1;
        if (config.state_auto == 0) sprintf (items[1].text, "Savestate Auto: FAT");
        else if (config.state_auto == 1) sprintf (items[1].text, "Savestate Auto: MCARD A");
        else if (config.state_auto == 2) sprintf (items[1].text, "Savestate Auto: MCARD B");
        else sprintf (items[1].text, "Savestate Auto: OFF");
        break;

      case 2:   /*** Sound effects volume ***/
      case -4:
        if (ret < 0) config.sfx_volume -=10;
        else config.sfx_volume +=10;
        if (config.sfx_volume < 0) config.sfx_volume = 100.0;
        else if (config.sfx_volume > 100) config.sfx_volume = 0.0;
        sprintf (items[2].text, "SFX Volume: %1.1f", config.sfx_volume);
        break;

      case 3:   /*** Background music volume ***/
      case -5:
        if (ret < 0) config.bgm_volume -=10;
        else config.bgm_volume +=10;
        if (config.bgm_volume < 0) config.bgm_volume = 100.0;
        else if (config.bgm_volume > 100) config.bgm_volume = 0.0;
        SetVolumeOgg(((int)config.bgm_volume * 255) / 100);
        sprintf (items[3].text, "BGM Volume: %1.1f", config.bgm_volume);
        break;

      case 4:   /*** Background color ***/
      case -6:
        if (ret < 0) config.bg_color --;
        else config.bg_color ++;
        if (config.bg_color < 0) config.bg_color = BG_COLOR_MAX - 1;
        if (config.bg_color >= BG_COLOR_MAX) config.bg_color = 0;
        if (config.bg_color) sprintf (items[4].text, "BG Color: Type %d", config.bg_color);
        else sprintf (items[4].text, "BG Color: DEFAULT");
        break;

      case -1:
        quit = 1;
        break;
    }
  }

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * Sound Option menu
 *
 ****************************************************************************/
static void soundmenu ()
{
  int ret, quit = 0;
  gui_menu *m = &menu_audio;
  gui_item *items = m->items;

  sprintf (items[0].text, "PSG Volume: %1.2f", (double)config.psg_preamp/100.0);
  sprintf (items[1].text, "FM Volume: %1.2f", (double)config.fm_preamp/100.0);
  sprintf (items[2].text, "Volume Boost: %dX", config.boost);
  sprintf (items[3].text, "LowPass Filter: %s", config.filter ? " ON":"OFF");
  if (config.hq_fm == 0) sprintf (items[4].text, "HQ YM2612: OFF");
  else if (config.hq_fm == 1) sprintf (items[4].text, "HQ YM2612: LINEAR");
  else sprintf (items[4].text, "HQ YM2612: SINC");

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("Sound "));

  while (quit == 0)
  {

    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0:
      case -2:
        if (ret<0) config.psg_preamp --;
        else config.psg_preamp ++;
        if (config.psg_preamp < 0) config.psg_preamp = 500;
        if (config.psg_preamp > 500) config.psg_preamp = 0;
        sprintf (items[0].text, "PSG Volume: %1.2f", (double)config.psg_preamp/100.0);
        break;

      case 1:
      case -3:
        if (ret<0) config.fm_preamp --;
        else config.fm_preamp ++;
        if (config.fm_preamp < 0) config.fm_preamp = 500;
        if (config.fm_preamp > 500) config.fm_preamp = 0;
        sprintf (items[1].text, "FM Volume: %1.2f", (double)config.fm_preamp/100.0);
        break;

      case 2:
        config.boost ++;
        if (config.boost > 4) config.boost = 0;
        sprintf (items[2].text, "Volume Boost: %dX", config.boost);
        break;
      
      case 3:
        config.filter ^= 1;
        sprintf (items[3].text, "LowPass Filter: %s", config.filter ? " ON":"OFF");
        break;

      case 4:
        config.hq_fm ++;
        if (config.hq_fm>2) config.hq_fm = 0;
        if (config.hq_fm == 0) sprintf (items[4].text, "HQ YM2612: OFF");
        else if (config.hq_fm == 1) sprintf (items[4].text, "HQ YM2612: LINEAR");
        else sprintf (items[4].text, "HQ YM2612: SINC");
        if (genromsize) 
        {
          unsigned char *temp = memalign(32,YM2612GetContextSize());
          if (temp) memcpy(temp, YM2612GetContextPtr(), YM2612GetContextSize());
          audio_init(48000);
          if (temp)
          {
            YM2612Restore(temp);
            free(temp);
          }
        }
        break;

      case -1:
        quit = 1;
        break;
    }
  }

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * Misc Option menu
 *
 ****************************************************************************/
static void systemmenu ()
{
  int ret = 255;
  int quit = 0;
  gui_menu *m = &menu_system;
  gui_item *items = m->items;

  if (config.region_detect == 0)      sprintf (items[0].text, "Console Region: AUTO");
  else if (config.region_detect == 1) sprintf (items[0].text, "Console Region:  USA");
  else if (config.region_detect == 2) sprintf (items[0].text, "Console Region:  EUR");
  else if (config.region_detect == 3) sprintf (items[0].text, "Console Region:  JAP");
  sprintf (items[1].text, "System Lockups: %s", config.force_dtack ? "OFF" : "ON");
  sprintf (items[2].text, "System BIOS: %s", (config.bios_enabled & 1) ? "ON":"OFF");
  sprintf (items[3].text, "SVP Cycles: %d", SVP_cycles);

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("System "));

  while (quit == 0)
  {
    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0:  /*** Region Force ***/
        config.region_detect = (config.region_detect + 1) % 4;
        if (config.region_detect == 0)      sprintf (items[0].text, "Console Region: AUTO");
        else if (config.region_detect == 1) sprintf (items[0].text, "Console Region:  USA");
        else if (config.region_detect == 2) sprintf (items[0].text, "Console Region:  EUR");
        else if (config.region_detect == 3) sprintf (items[0].text, "Console Region:  JAP");
        if (genromsize)
        {
          /* force region & cpu mode */
          set_region();
          
          /* reinitialize timings */
          system_init ();
          unsigned char *temp = memalign(32,YM2612GetContextSize());
          if (temp) memcpy(temp, YM2612GetContextPtr(), YM2612GetContextSize());
          audio_init(48000);
          if (temp)
          {
            YM2612Restore(temp);
            free(temp);
          }

          /* reinitialize HVC tables */
          vctab = (vdp_pal) ? ((reg[1] & 8) ? vc_pal_240 : vc_pal_224) : vc_ntsc_224;
          hctab = (reg[12] & 1) ? cycle2hc40 : cycle2hc32;

          /* reinitialize overscan area */
          bitmap.viewport.x = config.overscan ? ((reg[12] & 1) ? 16 : 12) : 0;
          bitmap.viewport.y = config.overscan ? (((reg[1] & 8) ? 0 : 8) + (vdp_pal ? 24 : 0)) : 0;
        }
        break;

      case 1:  /*** force DTACK ***/
        config.force_dtack ^= 1;
        sprintf (items[1].text, "System Lockups: %s", config.force_dtack ? "OFF" : "ON");
        break;

      case 2:  /*** BIOS support ***/
        config.bios_enabled ^= 1;
        sprintf (items[2].text, "System BIOS: %s", (config.bios_enabled & 1) ? "ON":"OFF");
        if (genromsize || (config.bios_enabled == 3)) 
        {
          system_init ();
          audio_init(48000);
          system_reset ();
        }
        break;

      case 3:  /*** SVP emulation ***/
      case -5:
        if (ret<0) SVP_cycles = SVP_cycles ? (SVP_cycles-1) : 1500;
        else SVP_cycles++;
        if (SVP_cycles > 1500) SVP_cycles = 0;
        sprintf (items[3].text, "SVP Cycles: %d", SVP_cycles);
        break;

      case -1:
        quit = 1;
        break;
    }
  }

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * Display Option menu
 *
 ****************************************************************************/
static void videomenu ()
{
  int ret, quit = 0;
  gui_menu *m = &menu_video;
  gui_item *items = m->items;

  sprintf (items[0].text, "Aspect: %s", config.aspect ? "ORIGINAL" : "STRETCHED");
  if (config.render == 1) sprintf (items[1].text,"Display: INTERLACED");
  else if (config.render == 2) sprintf (items[1].text, "Display: PROGRESSIVE");
  else sprintf (items[1].text, "Display: ORIGINAL");
  if (config.tv_mode == 0) sprintf (items[2].text, "TV Mode: 60HZ");
  else if (config.tv_mode == 1) sprintf (items[2].text, "TV Mode: 50HZ");
  else sprintf (items[2].text, "TV Mode: 50/60HZ");
  sprintf (items[3].text, "Bilinear Filter: %s", config.bilinear ? " ON" : "OFF");
  if (config.ntsc == 1) sprintf (items[4].text, "NTSC Filter: COMPOSITE");
  else if (config.ntsc == 2) sprintf (items[4].text, "NTSC Filter: S-VIDEO");
  else if (config.ntsc == 3) sprintf (items[4].text, "NTSC Filter: RGB");
  else sprintf (items[4].text, "NTSC Filter: OFF");
  sprintf (items[5].text, "Borders: %s", config.overscan ? " ON" : "OFF");

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("Video "));

  while (quit == 0)
  {

    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0: /*** config.aspect ratio ***/
        config.aspect ^= 1;
        sprintf (items[0].text, "Aspect: %s", config.aspect ? "ORIGINAL" : "STRETCHED");
        break;

      case 1:  /*** rendering ***/
        config.render = (config.render + 1) % 3;
        if (config.render == 2)
        {
          if (VIDEO_HaveComponentCable())
          {
            /* progressive mode (60hz only) */
            config.tv_mode = 0;
          }
          else
          {
            /* do nothing if component cable is not detected */
            config.render = 0;
          }
        }
        if (config.render == 1) sprintf (items[1].text,"Display: INTERLACED");
        else if (config.render == 2) sprintf (items[1].text, "Display: PROGRESSIVE");
        else sprintf (items[1].text, "Display: ORIGINAL");
        if (config.tv_mode == 0) sprintf (items[2].text, "TV Mode: 60HZ");
        else if (config.tv_mode == 1) sprintf (items[2].text, "TV Mode: 50HZ");
        else sprintf (items[2].text, "TV Mode: 50/60HZ");
        break;

      case 2: /*** tv mode ***/
        if (config.render != 2) config.tv_mode = (config.tv_mode + 1) % 3;
        if (config.tv_mode == 0) sprintf (items[2].text, "TV Mode: 60HZ");
        else if (config.tv_mode == 1) sprintf (items[2].text, "TV Mode: 50HZ");
        else sprintf (items[2].text, "TV Mode: 50/60HZ");
        break;
    
      case 3: /*** bilinear filtering ***/
        config.bilinear ^= 1;
        sprintf (items[3].text, "Bilinear Filter: %s", config.bilinear ? " ON" : "OFF");
        break;

      case 4: /*** NTSC filter ***/
        config.ntsc ++;
        if (config.ntsc > 3) config.ntsc = 0;
        if (config.ntsc == 1) sprintf (items[4].text, "NTSC Filter: COMPOSITE");
        else if (config.ntsc == 2) sprintf (items[4].text, "NTSC Filter: S-VIDEO");
        else if (config.ntsc == 3) sprintf (items[4].text, "NTSC Filter: RGB");
        else sprintf (items[4].text, "NTSC Filter: OFF");
        break;

      case 5: /*** overscan emulation ***/
        config.overscan ^= 1;
        sprintf (items[5].text, "Borders: %s", config.overscan ? " ON" : "OFF");
        break;

 /*     case 6: 
      case -8:
        if (ret<0) config.xshift --;
        else config.xshift ++;
        break;

      case 7: 
      case -9:
        if (ret<0) config.yshift --;
        else config.yshift ++;
        break;
      
      case 8: 
      case -10:
        if (config.aspect) break;
        if (ret<0) config.xscale --;
        else config.xscale ++;
        break;

      case 9: 
      case -11:
        if (config.aspect) break;
        if (ret<0) config.yscale --;
        else config.yscale ++;
        break;
*/
      case -1:
        quit = 1;
        break;
    }
  }

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * ConfigureJoypads
 ****************************************************************************/
extern int old_system[2];
static void inputsmenu(void)
{
  int ret, max_players;
  int i = 0;
  int quit = 0;
  int prevmenu = menu;
  char padmenu[8][25];

  int player = 0;
#ifdef HW_RVL
  u32 exp;
#endif

 /* gui_item items_sys[7][2] =
  {
    {
      {NULL,Ctrl_none      ,"","Port 1 - Unconnected",110,130,48,72},
      {NULL,Ctrl_gamepad   ,"","Port 1 - Gamepad"    , 87,117,96,84},
      {NULL,Ctrl_mouse     ,"","Port 1 - Mouse"      , 97,113,64,88},
      {NULL,Ctrl_menacer   ,"","Port 1 - Menacer"    , 94,113,80,88},
      {NULL,Ctrl_justifier ,"","Port 1 - Justifiers" , 88,117,80,84}
      {NULL,Ctrl_teamplayer,"","Port 1 - Teamplayer" , 94,109,80,92},
      {NULL,Ctrl_4wayplay  ,"","Port 1 - 4 Way Play" , 98,110,72,92}
    },
    {
      {NULL,Ctrl_none      ,"","Port 2 - Unconnected",110,130,48,72},
      {NULL,Ctrl_gamepad   ,"","Port 2 - Gamepad"    , 87,117,96,84},
      {NULL,Ctrl_mouse     ,"","Port 2 - Mouse"      , 97,113,64,88},
      {NULL,Ctrl_menacer   ,"","Port 2 - Menacer"    , 94,113,80,88},
      {NULL,Ctrl_justifier ,"","Port 2 - Justifiers" , 88,117,80,84}
      {NULL,Ctrl_teamplayer,"","Port 2 - Teamplayer" , 94,109,80,92},
      {NULL,Ctrl_4wayplay  ,"","Port 2 - 4 Way Play" , 98,110,72,92}
    }
  };
*/
  strcpy (menutitle, "Press B to return");

  menu = 0;
  while (quit == 0)
  {
    /* update max players */
    max_players = 0;
    if (input.system[0] == SYSTEM_GAMEPAD)
    {
      sprintf (padmenu[0], "Port 1: GAMEPAD");
      max_players ++;
    }
    else if (input.system[0] == SYSTEM_MOUSE)
    {
      sprintf (padmenu[0], "Port 1: MOUSE");
      max_players ++;
    }
    else if (input.system[0] == SYSTEM_WAYPLAY)
    {
      sprintf (padmenu[0], "Port 1: 4-WAYPLAY");
      max_players += 4;
    }
    else if (input.system[0] == SYSTEM_TEAMPLAYER)
    {
      sprintf (padmenu[0], "Port 1: TEAMPLAYER");
      max_players += 4;
    }
    else
      sprintf (padmenu[0], "Port 1: NONE");

    if (input.system[1] == SYSTEM_GAMEPAD)
    {
      sprintf (padmenu[1], "Port 2: GAMEPAD");
      max_players ++;
    }
    else if (input.system[1] == SYSTEM_MOUSE)
    {
      sprintf (padmenu[1], "Port 2: MOUSE");
      max_players ++;
    }
    else if (input.system[1] == SYSTEM_WAYPLAY)
    {
      sprintf (padmenu[1], "Port 2: 4-WAYPLAY");
    }
    else if (input.system[1] == SYSTEM_TEAMPLAYER)
    {
      sprintf (padmenu[1], "Port 2: TEAMPLAYER");
      max_players += 4;
    }
    else if (input.system[1] == SYSTEM_MENACER)
    {
      sprintf (padmenu[1], "Port 2: MENACER");
      max_players += 1;
    }
    else if (input.system[1] == SYSTEM_JUSTIFIER)
    {
      sprintf (padmenu[1], "Port 2: JUSTIFIERS");
      max_players += 2;
    }
    else
      sprintf (padmenu[1], "Port 2: NONE");

    /* JCART special case */
    if (j_cart) max_players +=2;

    /* reset current player nr */
    if (player >= max_players)
    {
      /* remove duplicate assigned inputs */
      if ((0!=player) && (config.input[0].device == config.input[player].device) && (config.input[0].port == config.input[player].port))
      {
          config.input[0].device = -1;
          config.input[0].port = i%4;
      }
      player = 0;
    }

    sprintf (padmenu[2], "Gun Cursor: %s", config.gun_cursor ? " ON":"OFF");
    sprintf (padmenu[3], "Invert Mouse: %s", config.invert_mouse ? " ON":"OFF");
    sprintf (padmenu[4], "Set Player: %d%s", player + 1, (j_cart && (player > 1)) ? "-JCART" : "");

    if (config.input[player].device == 0)
      sprintf (padmenu[5], "Device: GAMECUBE %d", config.input[player].port + 1);
#ifdef HW_RVL
    else if (config.input[player].device == 1)
      sprintf (padmenu[5], "Device: WIIMOTE %d", config.input[player].port + 1);
    else if (config.input[player].device == 2)
      sprintf (padmenu[5], "Device: NUNCHUK %d", config.input[player].port + 1);
    else if (config.input[player].device == 3)
      sprintf (padmenu[5], "Device: CLASSIC %d", config.input[player].port + 1);
#endif
    else
      sprintf (padmenu[5], "Device: NONE");

    /* when using wiimote, force to 3Buttons pad */
    if (config.input[player].device == 1) input.padtype[player] = DEVICE_3BUTTON;
    sprintf (padmenu[6], "%s", (input.padtype[player] == DEVICE_3BUTTON) ? "Type: 3BUTTONS":"Type: 6BUTTONS");

    sprintf (padmenu[7], "Configure Input");

    ret = domenu (&padmenu[0], 8,0);

    switch (ret)
    {
      case 0:
        if (j_cart)
        {
          WaitPrompt("JCART detected !");
          break;
        }
        input.system[0] ++;
        if (input.system[0] == SYSTEM_MENACER) input.system[0] ++;
        if (input.system[0] == SYSTEM_JUSTIFIER) input.system[0] ++;
        if ((input.system[0] == SYSTEM_MOUSE) && (input.system[1] == SYSTEM_MOUSE)) input.system[0] ++;
        if (input.system[0] == SYSTEM_WAYPLAY) input.system[1] = SYSTEM_WAYPLAY;
        if (input.system[0] > SYSTEM_WAYPLAY)
        {
          input.system[0] = NO_SYSTEM;
          input.system[1] = SYSTEM_GAMEPAD;
        }
        io_reset();
        old_system[0] = input.system[0];
        old_system[1] = input.system[1];
        break;
    
      case 1:
        if (j_cart)
        {
          WaitPrompt("JCART detected !");
          break;
        }
        input.system[1] ++;
        if ((input.system[0] == SYSTEM_MOUSE) && (input.system[1] == SYSTEM_MOUSE)) input.system[1] ++;
        if (input.system[1] == SYSTEM_WAYPLAY) input.system[0] = SYSTEM_WAYPLAY;
        if (input.system[1] > SYSTEM_WAYPLAY)
        {
          input.system[1] = NO_SYSTEM;
          input.system[0] = SYSTEM_GAMEPAD;
        }
        io_reset();
        old_system[0] = input.system[0];
        old_system[1] = input.system[1];
        break;

      case 2:
        config.gun_cursor ^= 1;
        break;

      case 3:
        config.invert_mouse ^= 1;
        break;

      case 4:
        /* remove duplicate assigned inputs */
        for (i=0; i<8; i++)
        {
          if ((i!=player) && (config.input[i].device == config.input[player].device) && (config.input[i].port == config.input[player].port))
          {
            config.input[i].device = -1;
            config.input[i].port = i%4;
          }
        }
        player = (player + 1) % max_players;
        break;

      case 5:
#ifdef HW_RVL
        if (config.input[player].device > 0)
        {
          config.input[player].port ++;
        }
        else
        {
          config.input[player].device ++;
          if (config.input[player].device == 1) config.input[player].port = 0;
        }

        if (config.input[player].device == 1)
        {
          exp = 4;
          if (config.input[player].port<4)
          {
            WPAD_Probe(config.input[player].port,&exp);
            if (exp == WPAD_EXP_NUNCHUK) exp = 4;
          }

          while ((config.input[player].port<4) && (exp == 4))
          {
            config.input[player].port ++;
            if (config.input[player].port<4)
            {
              exp = 4;
              WPAD_Probe(config.input[player].port,&exp);
              if (exp == WPAD_EXP_NUNCHUK) exp = 4;
            }
          }

          if (config.input[player].port >= 4)
          {
            config.input[player].port = 0;
            config.input[player].device = 2;
          }
        }

        if (config.input[player].device == 2)
        {
          exp = 4;
          if (config.input[player].port<4)
          {
            WPAD_Probe(config.input[player].port,&exp);
          }

          while ((config.input[player].port<4) && (exp != WPAD_EXP_NUNCHUK))
          {
            config.input[player].port ++;
            if (config.input[player].port<4)
            {
              exp = 4;
              WPAD_Probe(config.input[player].port,&exp);
            }
          }

          if (config.input[player].port >= 4)
          {
            config.input[player].port = 0;
            config.input[player].device = 3;
          }
        }

        if (config.input[player].device == 3)
        {
          exp = 4;
          if (config.input[player].port<4)
          {
            WPAD_Probe(config.input[player].port,&exp);
          }

          while ((config.input[player].port<4) && (exp != WPAD_EXP_CLASSIC))
          {
            config.input[player].port ++;
            if (config.input[player].port<4)
            {
              exp = 4;
              WPAD_Probe(config.input[player].port,&exp);
            }
          }

          if (config.input[player].port >= 4)
          {
            config.input[player].port = player % 4;
            config.input[player].device = 0;
          }
        }
#else
        config.input[player].device = 0;
#endif
        break;
    
      case 6:
        if (config.input[player].device == 1) break;
        input.padtype[player] ^= 1;
        io_reset();
        break;

      case 7:
        if (config.input[player].device < 0) break;
        if (input.padtype[player] == DEVICE_3BUTTON)  /* 3-buttons */
          gx_input_Config(config.input[player].port, config.input[player].device, 4);
        else if (config.input[player].device == 0)    /* 6-buttons w/o MODE */
          gx_input_Config(config.input[player].port, 0, 7);
        else                                          /* 6-buttons */
          gx_input_Config(config.input[player].port, config.input[player].device, 8);
       break;

      case -1:
        /* remove duplicate assigned inputs */
        for (i=0; i<8; i++)
        {
          if ((i!=player) && (config.input[i].device == config.input[player].device) && (config.input[i].port == config.input[player].port))
          {
            config.input[i].device = -1;
            config.input[i].port = i%4;
          }
        }
        quit = 1;
        break;
    }
  }

  menu = prevmenu;
}

/****************************************************************************
 * Main Option menu
 *
 ****************************************************************************/
static void optionmenu(void)
{
  int ret, quit = 0;
  gui_menu *m = &menu_options;

  while (quit == 0)
  {
    GUI_InitMenu(m);
    ret = GUI_RunMenu(m);
    GUI_DeleteMenu(m);
    switch (ret)
    {
      case 0:
        systemmenu();
        break;
      case 1:
        videomenu();
        break;
      case 2:
        soundmenu();
        break;
      case 3:
        inputsmenu();
        break;
      case 4:
        prefmenu();
        break;
      case -1:
        quit = 1;
        break;
    }
  }

  config_save();
}

/****************************************************************************
* Generic Load/Save menu
*
****************************************************************************/
static u8 device = 0;

static int loadsavemenu (int which)
{
  int prevmenu = menu;
  int quit = 0;
  int ret;
  int count = 3;
  char items[3][25];

  menu = 2;

  if (which == 1)
  {
    sprintf(items[1], "Save State");
    sprintf(items[2], "Load State");
  }
  else
  {
    sprintf(items[1], "Save SRAM");
    sprintf(items[2], "Load SRAM");
  }

  while (quit == 0)
  {
    if (device == 0) sprintf(items[0], "Device: FAT");
    else if (device == 1) sprintf(items[0], "Device: MCARD A");
    else if (device == 2) sprintf(items[0], "Device: MCARD B");

    ret = domenu (&items[0], count, 0);
    switch (ret)
    {
      case -1:
        quit = 1;
        break;

      case 0:
        device = (device + 1)%3;
        break;

      case 1:
      case 2:
        if (which == 1) quit = ManageState(ret-1,device);
        else if (which == 0) quit = ManageSRAM(ret-1,device);
        if (quit) return 1;
        break;
    }
  }

  menu = prevmenu;
  return 0;
}


/****************************************************************************
 * File Manager menu
 *
 ****************************************************************************/
static int filemenu ()
{
  int prevmenu = menu;
  int ret;
  int quit = 0;
  int count = 2;
  char items[2][25] = {
    {"SRAM Manager"},
    {"STATE Manager"}
  };

  menu = 0;

  while (quit == 0)
  {
    strcpy (menutitle, "Press B to return");
    ret = domenu (&items[0], count, 0);
    switch (ret)
    {
      case -1: /*** Button B ***/
        ret = 0;
        quit = 1;
        break;

      case 0:   /*** SRAM Manager ***/
      case 1:  /*** SaveState Manager ***/
        if (loadsavemenu(ret)) return 1;
        break;
    }
  }

  menu = prevmenu;
  return 0;
}


/****************************************************************************
 * Load Rom menu
 *
 ****************************************************************************/
static int loadmenu ()
{
  int ret,size;
  gui_menu *m = &menu_load;

  while (1)
  {
    GUI_InitMenu(m);
    ret = GUI_RunMenu(m);
    GUI_DeleteMenu(m);

    switch (ret)
    {
      /*** Button B ***/
      case -1: 
        return 0;

      /*** Load from DVD ***/
#ifdef HW_RVL
      case 3:
#else
      case 2:
#endif
        size = DVD_Open(cart_rom);
        if (size)
        {
          //dvd_motor_off();
          memfile_autosave(-1,config.state_auto);
          reloadrom(size,filelist[selection].filename);
          memfile_autoload(config.sram_auto,config.state_auto);
          return 1;
        }
        break;

      /*** Load from FAT device ***/
      default:
        size = FAT_Open(ret,cart_rom);
        if (size)
        {
          memfile_autosave(-1,config.state_auto);
          reloadrom(size,filelist[selection].filename);
          memfile_autoload(config.sram_auto,config.state_auto);
          return 1;
        }
        break;
    }
  }
}

/***************************************************************************
  * Show rom info screen
 ***************************************************************************/
static void showrominfo ()
{
  int ypos;
  u8 i,j,quit,redraw,max;
  char msg[128];
  short p;
  char pName[14][21];

  quit = 0;
  j = 0;
  redraw = 1;

  /*** Remove any still held buttons ***/
  while (PAD_ButtonsHeld(0))  PAD_ScanPads();
#ifdef HW_RVL
  while (WPAD_ButtonsHeld(0)) WPAD_ScanPads();
#endif

  max = 14;
  for (i = 0; i < 14; i++)
  {
    if (peripherals & (1 << i))
    {
      sprintf(pName[max-14],"%s", peripheralinfo[i].pName);
      max ++;
    }
  }

  while (quit == 0)
  {
    if (redraw)
    {
      gxClearScreen ((GXColor)BLACK);

      ypos = 134;
      WriteCentre(ypos, "ROM Header Information");
      ypos += 2*fheight;

      for (i=0; i<8; i++)
      {
        switch (i+j)
        {
        case 0:
          sprintf (msg, "Console type: %s", rominfo.consoletype);
          break;
        case 1:
          sprintf (msg, "Copyright: %s", rominfo.copyright);
          break;
        case 2:
          sprintf (msg, "Company: %s", companyinfo[getcompany ()].company);
          break;
        case 3:
          sprintf (msg, "Game Domestic Name:");
          break;
        case 4:
          sprintf(msg, " %s",rominfo.domestic);
          break;
        case 5:
          sprintf (msg, "Game International Name:");
          break;
        case 6:
          sprintf(msg, " %s",rominfo.international);
          break;
        case 7:
          sprintf (msg, "Type - %s : %s", rominfo.ROMType, strcmp (rominfo.ROMType, "AI") ? "Game" : "Educational");
          break;
        case 8:
          sprintf (msg, "Product - %s", rominfo.product);
          break;
        case 9:
          sprintf (msg, "Checksum - %04x (%04x) (%s)", rominfo.checksum, realchecksum, (rominfo.checksum == realchecksum) ? "Good" : "Bad");
          break;
        case 10:
          sprintf (msg, "ROM end: $%06X", rominfo.romend);
          break;
        case 11:
          if (svp) sprintf (msg, "SVP Chip detected");
          else if (sram.custom) sprintf (msg, "EEPROM(%dK) - $%06X", ((eeprom.type.size_mask+1)* 8) /1024, (unsigned int)sram.start);
          else if (sram.detected) sprintf (msg, "SRAM Start  - $%06X", sram.start);
          else sprintf (msg, "External RAM undetected");
             
          break;
        case 12:
          if (sram.custom) sprintf (msg, "EEPROM(%dK) - $%06X", ((eeprom.type.size_mask+1)* 8) /1024, (unsigned int)sram.end);
          else if (sram.detected) sprintf (msg, "SRAM End   - $%06X", sram.end);
          else if (sram.on) sprintf (msg, "Default SRAM activated ");
          else sprintf (msg, "SRAM is disactivated  ");
          break;
        case 13:
          if (region_code == REGION_USA) sprintf (msg, "Region - %s (USA)", rominfo.country);
          else if (region_code == REGION_EUROPE) sprintf (msg, "Region - %s (EUR)", rominfo.country);
          else if (region_code == REGION_JAPAN_NTSC) sprintf (msg, "Region - %s (JAP)", rominfo.country);
          else if (region_code == REGION_JAPAN_PAL) sprintf (msg, "Region - %s (JPAL)", rominfo.country);
          break;
        default:
          sprintf (msg, "Supports - %s", pName[i+j-14]);
          break;
      }

      write_font (100, ypos, msg);
      ypos += fheight;
    }

    ypos += fheight;
    WriteCentre (ypos, "Press A to Continue");
    gxSetScreen ();
  }

  p = m_input.keys;
  redraw = 0;

  if ((j<(max-8)) && (p & PAD_BUTTON_DOWN)) {redraw = 1; j++;}
  if ((j>0) && (p & PAD_BUTTON_UP)) {redraw = 1; j--;}
  if (p & PAD_BUTTON_A) quit = 1;
  if (p & PAD_BUTTON_B) quit = 1;
  }
}

/****************************************************************************
 * Main Menu
 *
 ****************************************************************************/
void MainMenu (void)
{
  int ret, quit = 0;

  char *items[3] =
  {
    "View Credits",
    "Exit to Loader",
#ifdef HW_RVL
    "Exit to System Menu"
#else
    "Reset System"
#endif
  };

  /* autosave SRAM */
  memfile_autosave(config.sram_auto,-1);

#ifdef HW_RVL
  if (Shutdown)
  {
    shutdown();
    SYS_ResetSystem(SYS_POWEROFF, 0, 0);
  }

  /* wiimote pointer */
  w_pointer = gxTextureOpenPNG(generic_point_png,0);
#endif

  gui_menu *m = &menu_main;

  /* check if a game is running */
  if (genromsize)
  {
    m->screenshot = 1;
    m->max_items = 9;
    m->max_buttons = 9;
    m->buttons[3].state |= BUTTON_SELECT_SFX;
    m->buttons[5].state |= BUTTON_SELECT_SFX;
    m->buttons[3].shift[1] = 3;
    m->buttons[4].shift[1] = 3;
    m->buttons[5].shift[1] = 2;
  }
  else
  {
    m->screenshot = 0;
    m->max_items = 6;
    m->max_buttons = 6;
    m->buttons[3].state &= ~BUTTON_SELECT_SFX;
    m->buttons[5].state &= ~BUTTON_SELECT_SFX;
    m->buttons[3].shift[1] = 0;
    m->buttons[4].shift[1] = 0;
    m->buttons[5].shift[1] = 0;
  }

  GUI_DrawMenuFX(m,10,0);

  while (quit == 0)
  {
    GUI_InitMenu(m);
    ret = GUI_RunMenu(m);
    GUI_DeleteMenu(m);

    switch (ret)
    {
      case -1: /*** Play Game ***/
      case 6:
        if (genromsize)
        {
          /* menu transition to game screen */
          GUI_DrawMenuFX(m,10,1);
          quit = 1;
        }
        break;

      case 0: /*** Quit Emulator ***/
      {
        GUI_InitMenu(m);
        switch (GUI_WindowPrompt(m, VERSION, items,3))
        {
          case 1:
            GUI_FadeMenu(m,1,1);
#ifdef HW_RVL
            gxTextureClose(&w_pointer);
#endif
            GUI_DeleteMenu(m);
            shutdown();
            exit(0);
            break;

          case 2:
            GUI_FadeMenu(m,1,1);
#ifdef HW_RVL
            gxTextureClose(&w_pointer);
#endif
            GUI_DeleteMenu(m);
            shutdown();
#ifdef HW_RVL
            SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
#else
            SYS_ResetSystem(SYS_HOTRESET,0,0);
#endif
            break;

          default: /* TODO */
            GUI_DeleteMenu(m);
            break;
        }
        break;
      }

      case 1:  /*** Load Game ***/
        quit = loadmenu();
        break;

      case 2:  /*** Options */
        optionmenu ();
        break;

      case 3:  /*** Memory Manager ***/
        if (genromsize) quit = filemenu ();
        break;

      case 4:  /*** Emulator Reset ***/
        if (!genromsize) break;
        system_reset (); 
        gxClearScreen ((GXColor)BLACK);
        gxSetScreen ();
        quit = 1;
        break;

      case 5:   /*** Game Genie ***/
        if (!genromsize) break;
        GetGGEntries();
        break;

      case 7:   /*** ROM Captrure ***/
        if (genromsize) gx_video_Capture();
        break;

      case 8:   /*** ROM Information ***/
        if (genromsize) showrominfo ();
        break;
    }
  }

  /*** Remove any still held buttons ***/
  while (PAD_ButtonsHeld(0))  PAD_ScanPads();
#ifdef HW_RVL
  while (WPAD_ButtonsHeld(0)) WPAD_ScanPads();
#endif

#ifdef HW_RVL
  /* free wiimote pointer data */
  gxTextureClose(&w_pointer);
#endif

#ifndef HW_RVL
  /*** Stop the DVD from causing clicks while playing ***/
  uselessinquiry ();
#endif
}
