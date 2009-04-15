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

#ifndef _MENU_H
#define _MENU_H

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif


#include "Background_intro_c1.h"
#include "Background_intro_c2.h"
#include "Background_intro_c3.h"
#include "Background_intro_c4.h"

#include "Banner_main.h"
#include "Banner_bottom.h"
#include "Banner_top.h"

#include "Background_main.h"
#include "Background_overlay.h"

#include "Frame_s1.h"
#include "Frame_s2.h"
#include "Frame_title.h"

#include "Overlay_bar.h"

#include "Browser_dir.h"

#include "Star_full.h"
#include "Star_empty.h"

#include "Snap_empty.h"
#include "Snap_frame.h"

#include "Main_logo.h"
#include "Main_play.h"
#include "Main_load.h"
#include "Main_options.h"
#include "Main_file.h"
#include "Main_reset.h"
#include "Main_info.h"

#include "Option_ctrl.h"
#include "Option_ggenie.h"
#include "Option_sound.h"
#include "Option_video.h"
#include "Option_system.h"

#include "Load_recent.h"
#include "Load_sd.h"
#include "Load_dvd.h"

#include "Button_text.h"
#include "Button_text_over.h"
#include "Button_icon.h"
#include "Button_icon_over.h"
#include "Button_up.h"
#include "Button_down.h"
#include "Button_up_over.h"
#include "Button_down_over.h"

#ifdef HW_RVL
#include "Load_usb.h"
#include "Key_A_wii.h"
#include "Key_B_wii.h"
#include "Key_home.h"
#include "generic_point.h"
#include "generic_openhand.h"
#else
#include "Key_A_gcn.h"
#include "Key_B_gcn.h"
#include "Key_trigger_Z.h"
#endif

#include "button_select.h"
#include "button_over.h"

/*****************************************************************************/
/*  Generic GUI structures                                                   */
/*****************************************************************************/

/* Menu Inputs */
typedef struct
{
  u16 keys;
#ifdef HW_RVL
  struct ir_t ir;
#endif
} t_input_menu;

/* Item descriptor*/
typedef struct
{
  gx_texture *texture; /* temporary texture data                               */
  const u8 *data;       /* pointer to png image data (items icon only)          */
  char text[64];        /* item string (items list only)                        */
  char comment[64];     /* item comment                                         */
  u16 x;                /* button image or text X position (upper left corner)  */
  u16 y;                /* button image or text Y position (upper left corner)  */
  u16 w;                /* button image or text width                           */
  u16 h;                /* button image or text height                          */
} gui_item;

/* Button descriptor */
typedef struct
{
  gx_texture *texture[2];  /* temporary texture datas                       */
  const u8 *image[2];       /* pointer to png image datas (default)         */
} butn_data;

/* Button descriptor */
typedef struct
{
  butn_data *data;          /* pointer to button image/texture data         */
  u16 x;                    /* button image X position (upper left corner)  */
  u16 y;                    /* button image Y position (upper left corner)  */
  u16 w;                    /* button image pixels width                    */
  u16 h;                    /* button image pixels height                   */
} gui_butn;

/* Image descriptor */
typedef struct
{
  gx_texture *texture; /* temporary texture data                               */
  const u8 *data;       /* pointer to png image data                            */
  u16 x;                /* button image or text X position (upper left corner)  */
  u16 y;                /* button image or text Y position (upper left corner)  */
  u16 w;                /* button image or text width                           */
  u16 h;                /* button image or text height                          */
} gui_image;

/* Menu descriptor */
typedef struct
{
  char title[64];             /* menu title                                         */
  s8 selected;                /* index of selected item                             */
  u8 offset;                  /* items list offset                                  */
  u8 max_items;               /* total number of items                              */
  u8 max_buttons;             /* total number of buttons (not necessary identical)  */
  u8 shift;                   /* number of items by line                            */
  gui_item *items;            /* menu items table                                   */
  gui_butn *buttons;          /* menu buttons table                                 */
  gui_image *overlay;         /* overlay image                                      */
  gui_image *background;      /* background image                                   */
  gui_image *logo;            /* logo image                                         */
  gui_image *frames[2];       /* windows (max. 2)                                   */
  gui_image *banners[2];      /* bottom & top banners                               */
  gui_item *helpers[2];       /* left & right key comments                          */
  gui_butn *arrows[2];        /* items list up & down arrows                        */
} gui_menu;


/* Global data */
extern u8 SILENT;
extern t_input_menu m_input;

/*****************************************************************************/
/*  Common GUI images                                                        */
/*****************************************************************************/
extern gui_image logo_main;
extern gui_image logo_small;
extern gui_image top_banner;
extern gui_image bottom_banner;
extern gui_image main_banner;
extern gui_image bg_right;
extern gui_image bg_center;
extern gui_image bg_overlay_line;
extern gui_image left_frame;
extern gui_image right_frame;

/*****************************************************************************/
/*  Common GUI buttons data                                                  */
/*****************************************************************************/
extern butn_data arrow_up_data;
extern butn_data arrow_down_data;
extern butn_data button_text_data;
extern butn_data button_icon_data;

/*****************************************************************************/
/*  Common GUI items                                                            */
/*****************************************************************************/
extern gui_item action_cancel;
extern gui_item action_select;
extern gui_item action_exit;

#endif
