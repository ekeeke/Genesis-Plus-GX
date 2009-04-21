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


/*****************************************************************************/
/*  GUI Buttons state                                                        */
/*****************************************************************************/
#define BUTTON_VISIBLE        0x01
#define BUTTON_OVER_SFX       0x10
#define BUTTON_SELECT_SFX     0x20

/*****************************************************************************/
/*  GUI Image state                                                        */
/*****************************************************************************/
#define IMAGE_VISIBLE         0x01
#define IMAGE_REPEAT          0x02
#define IMAGE_FADE            0x10
#define IMAGE_SLIDE_X         0x20
#define IMAGE_SLIDE_Y         0x40

/*****************************************************************************/
/*  GUI png data                                                             */
/*****************************************************************************/

extern const u8 Bg_intro_c1_png[];
extern const u8 Bg_intro_c2_png[];
extern const u8 Bg_intro_c3_png[];
extern const u8 Bg_intro_c4_png[];
extern const u8 Bg_main_png[];
extern const u8 Bg_overlay_png[];

extern const u8 Main_logo_png[];

extern const u8 Banner_main_png[];
extern const u8 Banner_bottom_png[];
extern const u8 Banner_top_png[];

extern const u8 Frame_s1_png[];
extern const u8 Frame_s2_png[];
extern const u8 Frame_title_png[];

extern const u8 Overlay_bar_png[];

extern const u8 Browser_dir_png[];

extern const u8 Star_full_png[];
extern const u8 Star_empty_png[];

extern const u8 Snap_empty_png[];
extern const u8 Snap_frame_png[];

extern const u8 Main_load_png[];
extern const u8 Main_options_png[];
extern const u8 Main_quit_png[];
extern const u8 Main_file_png[];
extern const u8 Main_reset_png[];
extern const u8 Main_ggenie_png[];
extern const u8 Main_showinfo_png[];
#ifdef HW_RVL
extern const u8 Main_play_wii_png[];
#else
extern const u8 Main_play_gcn_png[];
#endif

extern const u8 Option_menu_png[];
extern const u8 Option_ctrl_png[];
extern const u8 Option_sound_png[];
extern const u8 Option_video_png[];
extern const u8 Option_system_png[];

extern const u8 Load_recent_png[];
extern const u8 Load_sd_png[];
extern const u8 Load_dvd_png[];
#ifdef HW_RVL
extern const u8 Load_usb_png[];
#endif

extern const u8 Button_text_png[];
extern const u8 Button_text_over_png[];
extern const u8 Button_icon_png[];
extern const u8 Button_icon_over_png[];
extern const u8 Button_up_png[];
extern const u8 Button_down_png[];
extern const u8 Button_up_over_png[];
extern const u8 Button_down_over_png[];

#ifdef HW_RVL
extern const u8 Key_A_wii_png[];
extern const u8 Key_B_wii_png[];
#else
extern const u8 Key_A_gcn_png[];
extern const u8 Key_B_gcn_png[];
#endif

#ifdef HW_RVL
extern const u8 generic_point_png[];
#endif

extern const u8 button_select_pcm[];
extern const u32 button_select_pcm_size;
extern const u8 button_over_pcm[];
extern const u32 button_over_pcm_size;


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
  gx_texture *texture;  /* temporary texture data                             */
  const u8 *data;       /* pointer to png image data (items icon only)        */
  char text[64];        /* item string (items list only)                      */
  char comment[64];     /* item comment                                       */
  u16 x;                /* item image or text X position (upper left corner)  */
  u16 y;                /* item image or text Y position (upper left corner)  */
  u16 w;                /* item image or text width                           */
  u16 h;                /* item image or text height                          */
} gui_item;

/* Button descriptor */
typedef struct
{
  gx_texture *texture[2];  /* temporary texture datas               */
  const u8 *image[2];       /* pointer to png image datas (default) */
} butn_data;

/* Button descriptor */
typedef struct
{
  butn_data *data;          /* pointer to button image/texture data         */
  u8 state;                 /* button state (ACTIVE,VISIBLE,SELECTED...)    */
  u16 x;                    /* button image X position (upper left corner)  */
  u16 y;                    /* button image Y position (upper left corner)  */
  u16 w;                    /* button image pixels width                    */
  u16 h;                    /* button image pixels height                   */
} gui_butn;

/* Image descriptor */
typedef struct
{
  gx_texture *texture;  /* temporary texture data                 */
  const u8 *data;       /* pointer to png image data              */
  u8 state;             /* image state (VISIBLE)                  */
  u16 x;                /* image X position (upper left corner)   */
  u16 y;                /* image Y position (upper left corner)   */
  u16 w;                /* image width                            */
  u16 h;                /* image height                           */
  u8 alpha;             /* alpha transparency                     */
  u16 xrange[2];        /* image X range (slide effect)           */
  u16 yrange[2];        /* image Y range (slide effect)           */
} gui_image;

/* Menu descriptor */
typedef struct
{
  char title[64];             /* menu title                         */
  s8 selected;                /* index of selected item             */
  u8 offset;                  /* items list offset                  */
  u8 max_items;               /* total number of items              */
  u8 max_buttons;             /* total number of buttons            */
  u8 max_images;              /* total number of background images  */
  u8 shift[2];                /* number of buttons per line         */
  gui_item *items;            /* menu items                         */
  gui_butn *buttons;          /* menu buttons                       */
  gui_image *bg_images;       /* background images                  */
  gui_item *helpers[2];       /* left & right key comments          */
  gui_butn *arrows[2];        /* arrows buttons                     */
  bool screenshot;            /* use gamescreen as background       */
} gui_menu;


/* Global data */
extern u8 SILENT;
extern t_input_menu m_input;

#ifdef HW_RVL
extern gx_texture *w_pointer;
#endif


extern void MainMenu(void);
extern void GUI_InitMenu(gui_menu *menu);
extern void GUI_DeleteMenu(gui_menu *menu);
extern void GUI_DrawMenu(gui_menu *menu);
extern void GUI_DrawMenuFX(gui_menu *menu, u8 speed, u8 out);
extern int GUI_RunMenu(gui_menu *menu);

#endif
