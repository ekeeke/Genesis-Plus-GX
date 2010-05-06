/****************************************************************************
 *  menu.c
 *
 *  Genesis Plus GX menus
 *
 *  Eke-Eke (2009)
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

/* PNG images */
/* Intro */
extern const u8 Bg_intro_c1_png[];
extern const u8 Bg_intro_c2_png[];
extern const u8 Bg_intro_c3_png[];
extern const u8 Bg_intro_c4_png[];
extern const u8 Bg_intro_c5_png[];
extern const u8 Bg_credits_png[];

/* ROM Browser */
extern const u8 Overlay_bar_png[];
extern const u8 Browser_dir_png[];
extern const u8 Star_full_png[];
extern const u8 Star_empty_png[];
extern const u8 Snap_empty_png[];
extern const u8 Snap_frame_png[];

/* Main menu */
extern const u8 Main_load_png[];
extern const u8 Main_options_png[];
extern const u8 Main_quit_png[];
extern const u8 Main_file_png[];
extern const u8 Main_reset_png[];
extern const u8 Main_ggenie_png[];
extern const u8 Main_showinfo_png[];
extern const u8 Main_takeshot_png[];
#ifdef HW_RVL
extern const u8 Main_play_wii_png[];
#else
extern const u8 Main_play_gcn_png[];
#endif

/* Options menu */
extern const u8 Option_menu_png[];
extern const u8 Option_ctrl_png[];
extern const u8 Option_sound_png[];
extern const u8 Option_video_png[];
extern const u8 Option_system_png[];

/* Load ROM menu */
extern const u8 Load_recent_png[];
extern const u8 Load_sd_png[];
extern const u8 Load_dvd_png[];
#ifdef HW_RVL
extern const u8 Load_usb_png[];
#endif

/* Save Manager menu */
extern const u8 Button_load_png[];
extern const u8 Button_load_over_png[];
extern const u8 Button_save_png[];
extern const u8 Button_save_over_png[];
extern const u8 Button_special_png[];
extern const u8 Button_special_over_png[];
extern const u8 Button_delete_png[];
extern const u8 Button_delete_over_png[];

/* Controller Settings */
extern const u8 Ctrl_4wayplay_png[];
extern const u8 Ctrl_gamepad_png[];
extern const u8 Ctrl_justifiers_png[];
extern const u8 Ctrl_menacer_png[];
extern const u8 Ctrl_mouse_png[];
extern const u8 Ctrl_none_png[];
extern const u8 Ctrl_teamplayer_png[];
extern const u8 Ctrl_pad3b_png[];
extern const u8 Ctrl_pad6b_png[];
extern const u8 Ctrl_config_png[];
extern const u8 Ctrl_player_png[];
extern const u8 Ctrl_player_over_png[];
extern const u8 Ctrl_player_none_png[];
extern const u8 ctrl_option_off_png[];
extern const u8 ctrl_option_on_png[];
extern const u8 ctrl_gamecube_png[];
#ifdef HW_RVL
extern const u8 ctrl_classic_png[];
extern const u8 ctrl_nunchuk_png[];
extern const u8 ctrl_wiimote_png[];
#endif

extern void MainMenu (void);

#endif

