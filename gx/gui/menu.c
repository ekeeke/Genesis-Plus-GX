/****************************************************************************
 *  menu.c
 *
 *  Genesis Plus GX menu
 *
 *  Copyright Eke-Eke (2009-2025)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

#include "shared.h"
#include "font.h"
#include "gui.h"
#include "filesel.h"
#include "cheats.h"
#include "file_load.h"
#include "file_slot.h"

#ifdef HW_RVL
#include <ogc/usbmouse.h>
#include "wiidrc.h"
#include "retrousb.h"
#endif

#include <ogc/lwp_threads.h>
#include <ogc/lwp_watchdog.h>

/* Credits */
#include "Bg_credits_png.h"

/* Main menu */
#include "Main_load_png.h"
#include "Main_options_png.h"
#include "Main_quit_png.h"
#include "Main_file_png.h"
#include "Main_reset_png.h"
#include "Main_cheats_png.h"
#include "Main_showinfo_png.h"
#include "Main_takeshot_png.h"
#ifdef HW_RVL
#include "Main_play_wii_png.h"
#else
#include "Main_play_gcn_png.h"
#endif

/* Options menu */
#include "Option_menu_png.h"
#include "Option_ctrl_png.h"
#include "Option_sound_png.h"
#include "Option_video_png.h"
#include "Option_system_png.h"

/* Load ROM menu */
#include "Load_recent_png.h"
#include "Load_md_png.h"
#include "Load_ms_png.h"
#include "Load_gg_png.h"
#include "Load_sg_png.h"
#include "Load_cd_png.h"

/* Save Manager menu */
#include "Button_load_png.h"
#include "Button_load_over_png.h"
#include "Button_save_png.h"
#include "Button_save_over_png.h"
#include "Button_special_png.h"
#include "Button_special_over_png.h"
#include "Button_delete_png.h"
#include "Button_delete_over_png.h"

/* Controller Settings */
#include "Ctrl_4wayplay_png.h"
#include "Ctrl_gamepad_png.h"
#include "Ctrl_justifiers_png.h"
#include "Ctrl_menacer_png.h"
#include "Ctrl_mouse_png.h"
#include "Ctrl_xe_1ap_png.h"
#include "Ctrl_activator_png.h"
#include "Ctrl_lightphaser_png.h"
#include "Ctrl_paddle_png.h"
#include "Ctrl_sportspad_png.h"
#include "Ctrl_none_png.h"
#include "Ctrl_teamplayer_png.h"
#include "Ctrl_mastertap_png.h"
#include "Ctrl_graphic_board_png.h"
#include "Ctrl_pad_auto_png.h"
#include "Ctrl_pad2b_png.h"
#include "Ctrl_pad3b_png.h"
#include "Ctrl_pad6b_png.h"
#include "Ctrl_config_png.h"
#include "ctrl_option_off_png.h"
#include "ctrl_option_on_png.h"
#include "ctrl_gamecube_png.h"
#ifdef HW_RVL
#include "ctrl_classic_png.h"
#include "ctrl_nunchuk_png.h"
#include "ctrl_wiimote_png.h"
#include "ctrl_wiiu_png.h"
#include "ctrl_retrousb_png.h"
#endif

/* Generic images */
#include "Button_sm_blue_png.h"
#include "Button_sm_grey_png.h"
#include "Button_sm_yellow_png.h"

/* Exit callback */
void (*reload)(void);


/*****************************************************************************/
/*  Specific Menu Callbacks                                                  */
/*****************************************************************************/
static void ctrlmenu_cb(void);
static void savemenu_cb(void);
static void mainmenu_cb(void);

/*****************************************************************************/
/*  Generic Buttons data                                                     */
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

static butn_data button_icon_sm_data =
{
  {NULL,NULL},
  {Button_icon_sm_png,Button_icon_sm_over_png}
};

static butn_data button_player_data =
{
  {NULL,NULL},
  {Button_sm_blue_png,Button_sm_yellow_png}
};

static butn_data button_player_none_data =
{
  {NULL,NULL},
  {Button_sm_grey_png,NULL}
};

static butn_data button_load_data =
{
  {NULL,NULL},
  {Button_load_png,Button_load_over_png}
};

static butn_data button_save_data =
{
  {NULL,NULL},
  {Button_save_png,Button_save_over_png}
};

static butn_data button_special_data =
{
  {NULL,NULL},
  {Button_special_png,Button_special_over_png}
};

static butn_data button_delete_data =
{
  {NULL,NULL},
  {Button_delete_png,Button_delete_over_png}
};

/*****************************************************************************/
/*  Generic GUI items                                                         */
/*****************************************************************************/
static gui_item action_cancel =
{
  NULL,Key_B_png,"","Back",10,422,28,28
};

static gui_item action_select =
{
  NULL,Key_A_png,"","",602,422,28,28
};

/*****************************************************************************/
/* GUI backgrounds images                                                    */
/*****************************************************************************/
static gui_image bg_main[] =
{
  {NULL,Bg_layer_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Banner_main_png,IMAGE_VISIBLE|IMAGE_SLIDE_BOTTOM,0,340,640,140,255},
  {NULL,Main_logo_png,IMAGE_VISIBLE|IMAGE_SLIDE_BOTTOM,202,362,232,56,255}
};

static gui_image bg_misc[] =
{
  {NULL,Bg_layer_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Banner_top_png,IMAGE_VISIBLE|IMAGE_SLIDE_TOP,0,0,640,108,255},
  {NULL,Banner_bottom_png,IMAGE_VISIBLE|IMAGE_SLIDE_BOTTOM,0,380,640,100,255},
  {NULL,Main_logo_png,IMAGE_VISIBLE|IMAGE_SLIDE_TOP,466,40,152,44,255}
};

static gui_image bg_ctrls[] =
{
  {NULL,Bg_layer_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Banner_top_png,IMAGE_VISIBLE,0,0,640,108,255},
  {NULL,Banner_bottom_png,IMAGE_VISIBLE,0,380,640,100,255},
  {NULL,Main_logo_png,IMAGE_VISIBLE,466,40,152,44,255},
  {NULL,Frame_s2_png,IMAGE_VISIBLE,38,72,316,168,128},
  {NULL,Frame_s2_png,IMAGE_VISIBLE,38,242,316,168,128},
  {NULL,Frame_s3_png,IMAGE_SLIDE_RIGHT,400,134,292,248,128}
};

static gui_image bg_list[] =
{
  {NULL,Bg_layer_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Banner_top_png,IMAGE_VISIBLE,0,0,640,108,255},
  {NULL,Banner_bottom_png,IMAGE_VISIBLE,0,380,640,100,255},
  {NULL,Main_logo_png,IMAGE_VISIBLE,466,40,152,44,255},
  {NULL,Frame_s1_png,IMAGE_VISIBLE,8,70,372,336,76}
};

static gui_image bg_saves[] =
{
  {NULL,NULL,0,0,0,0,0,255},
  {NULL,Bg_layer_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Bg_overlay_png,IMAGE_VISIBLE|IMAGE_REPEAT,0,0,640,480,255},
  {NULL,Banner_top_png,IMAGE_VISIBLE|IMAGE_SLIDE_TOP,0,0,640,108,255},
  {NULL,Banner_bottom_png,IMAGE_VISIBLE|IMAGE_SLIDE_BOTTOM,0,380,640,100,255},
  {NULL,Main_logo_png,IMAGE_VISIBLE|IMAGE_SLIDE_TOP,466,40,152,44,255},
  {NULL,Frame_s1_png,IMAGE_VISIBLE,8,70,372,336,76},
  {NULL,Frame_s1_png,IMAGE_SLIDE_RIGHT,468,108,372,296,76}
};

/*****************************************************************************/
/*  Menu Items description                                                   */
/*****************************************************************************/

/* Main menu */
static gui_item items_main[] =
{
  {NULL,Main_load_png    ,"","",114,162,80,92},
  {NULL,Main_options_png ,"","",290,166,60,88},
  {NULL,Main_quit_png    ,"","",460,170,52,84},
  {NULL,Main_file_png    ,"","",114,216,80,92},
  {NULL,Main_reset_png   ,"","",294,227,52,80},
  {NULL,Main_cheats_png  ,"","",454,218,64,92},
  {NULL,NULL             ,"","", 10,334,84,32},
#ifdef HW_RVL
  {NULL,Main_play_wii_png,"","", 10,372,84,32},
#else
  {NULL,Main_play_gcn_png,"","", 10,372,84,32},
#endif
  {NULL,Main_takeshot_png,"","",546,334,84,32},
  {NULL,Main_showinfo_png,"","",546,372,84,32}
};

/* Controllers menu */
static gui_item items_ctrls[] =
{
  {NULL,NULL,"","",  0,  0,  0,  0},
  {NULL,NULL,"","",  0,  0,  0,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",305,  0, 24,  0},
  {NULL,NULL,"","",  0,  0,  0,  0},
  {NULL,NULL,"","",  0,  0,  0,  0},
  {NULL,Ctrl_config_png,"Keys\nConfig","Configure Controller Keys",530,306,32,32}
};

/* Load menu */
static gui_item items_load[] =
{
  {NULL,Load_recent_png,"","Load recently played games",    119,144,72, 92},
  {NULL,Load_md_png,    "","Load Mega Drive/Genesis games", 278,141,84, 92},
  {NULL,Load_cd_png,    "","Load Sega/Mega CD games",       454,141,64, 92},
  {NULL,Load_ms_png,    "","Load Master System games",      114,284,84, 96},
  {NULL,Load_gg_png,    "","Load Game Gear games",          278,283,84,100},
  {NULL,Load_sg_png,    "","Load SG-1000 games",            454,281,64, 96}
};

/* Option menu */
static gui_item items_options[] =
{
  {NULL,Option_system_png,"","System settings",       114,142,80,92},
  {NULL,Option_video_png, "","Video settings",        288,150,64,84},
  {NULL,Option_sound_png, "","Audio settings",        464,154,44,80},
  {NULL,Option_ctrl_png,  "","Controllers settings",  192,286,88,92},
  {NULL,Option_menu_png,  "","Menu settings",         370,286,60,92}
};

/* Audio options */
static gui_item items_audio[] =
{
  {NULL,NULL,"Master System FM: AUTO",   "Enable/Disable YM2413 chip",                           56,132,276,48},
  {NULL,NULL,"YM2612 Type: DISCRETE",    "Select YM2612 chip model",                             56,132,276,48},
  {NULL,NULL,"High-Quality FM: ON",      "Enable/Disable YM2612/YM2413 high-quality resampling", 56,132,276,48},
  {NULL,NULL,"High-Quality PSG: ON",     "Enable/Disable SN76489 high-quality resampling",       56,132,276,48},
  {NULL,NULL,"FM Volume: 1.00",          "Adjust YM2612/YM2413 output mixing level",             56,132,276,48},
  {NULL,NULL,"PSG Volume: 2.50",         "Adjust SN76489 output mixing level",                   56,132,276,48},
  {NULL,NULL,"CD-DA Volume: 1.00",       "Adjust CD audio output mixing level",                  56,132,276,48},
  {NULL,NULL,"PCM Volume: 1.00",         "Adjust CD hardware RF5C164 output mixing level",       56,132,276,48},
  {NULL,NULL,"Audio Output: STEREO",     "Select audio mixing output type",                      56,132,276,48},
  {NULL,NULL,"Filtering: 3-BAND EQ",     "Select audio filtering type",                          56,132,276,48},
  {NULL,NULL,"Low Gain: 1.00",           "Adjust EQ Low Band Gain",                              56,132,276,48},
  {NULL,NULL,"Mid Gain: 1.00",           "Adjust EQ Mid Band Gain",                              56,132,276,48},
  {NULL,NULL,"High Gain: 1.00",          "Adjust EQ High Band Gain",                             56,132,276,48},
  {NULL,NULL,"Low Freq: 200 Hz",         "Adjust EQ Lowest Frequency",                           56,132,276,48},
  {NULL,NULL,"High Freq: 20000 Hz",      "Adjust EQ Highest Frequency",                          56,132,276,48}
}; 

/* System ROM paths */
static gui_item items_rompaths[] =
{
  {NULL,NULL,"CD BIOS (USA): NOT FOUND",  "Select Sega CD (USA) BIOS",            56,132,276,48},
  {NULL,NULL,"CD BIOS (PAL): NOT FOUND",  "Select Mega CD (PAL) BIOS",            56,132,276,48},
  {NULL,NULL,"CD BIOS (JAPAN): NOT FOUND","Select Mega CD (JAPAN) BIOS",          56,132,276,48},
  {NULL,NULL,"MD BIOS (TMSS): NOT FOUND", "Select Mega Drive / Genesis Boot ROM", 56,132,276,48},
  {NULL,NULL,"MS BIOS (USA): NOT FOUND",  "Select Master System (USA) Boot ROM",  56,132,276,48},
  {NULL,NULL,"MS BIOS (PAL): NOT FOUND",  "Select Master System (PAL) Boot ROM",  56,132,276,48},
  {NULL,NULL,"MS BIOS (JAPAN): NOT FOUND","Select Master System (JAPAN) Boot ROM",56,132,276,48},
  {NULL,NULL,"Game Gear BIOS: NOT FOUND", "Select Game Gear Boot ROM",            56,132,276,48},
  {NULL,NULL,"Game Genie: NOT FOUND",     "Select Game Genie ROM",                56,132,276,48},
  {NULL,NULL,"Action Replay: NOT FOUND",  "Select Action Replay (Pro) ROM",       56,132,276,48},
  {NULL,NULL,"S&K (2MB): NOT FOUND",  "Select Sonic & Knuckle main ROM",      56,132,276,48},
  {NULL,NULL,"S2&K (256KB): NOT FOUND","Select Sonic 2 & Knuckle upmem ROM",   56,132,276,48},
};

/* System options */
static gui_item items_system[] =
{
  {NULL,NULL,"System: MEGA DRIVE/GENESIS", "Select system hardware model",                56,132,276,48},
  {NULL,NULL,"Region: JAPAN",              "Select system region",                        56,132,276,48},
  {NULL,NULL,"VDP Mode: AUTO",             "Select VDP mode",                             56,132,276,48},
  {NULL,NULL,"System Clock: AUTO",         "Select master clock frequency",               56,132,276,48},
  {NULL,NULL,"System Boot: BIOS->CART",    "Select system booting method",                56,132,276,48},
  {NULL,NULL,"System Lockups: OFF",        "Enable/Disable original system lockups",      56,132,276,48},
  {NULL,NULL,"68k Address Error: OFF",     "Enable/Disable 68k address error exceptions", 56,132,276,48},
  {NULL,NULL,"CD Access Time: OFF",        "Enable/Disable CD read/seek latency",         56,132,276,48},
  {NULL,NULL,"CD Add-on: MEGA/SEGA CD",    "Select cartridge mode CD hardware add-on",    56,132,276,48},
  {NULL,NULL,"Lock-On: SONIC&KNUCKLES",    "Select Lock-On cartridge type",               56,132,276,48},
  {NULL,NULL,"Cartridge Swap: OFF",        "Enable/Disable cartridge hot swap",           56,132,276,48},
  {NULL,NULL,"BIOS & Lock-On ROM paths",   "Configure Boot ROM & Lock-On ROM paths",      56,132,276,48},
  {NULL,NULL,"Main 68k Overclock: 3.0x",   "Adjust Mega Drive /Genesis CPU clock speed",  56,132,276,48},
  {NULL,NULL,"Sub 68k Overclock: 3.0x",    "Adjust Sega CD / Mega-CD CPU clock speed",    56,132,276,48},
  {NULL,NULL,"Z80 Overclock: 3.0x",        "Adjust Z80 CPU clock speed",                  56,132,276,48},
  {NULL,NULL,"SVP Cycles: 1500",           "Adjust SVP chip emulation speed",             56,132,276,48}
};

/* Video options */
static gui_item items_video[] =
{
  {NULL,NULL,"Display: PROGRESSIVE",        "Select video mode",                                 56,132,276,48},
  {NULL,NULL,"TV mode: 50/60HZ",            "Select video refresh rate",                         56,132,276,48},
  {NULL,NULL,"VSYNC: AUTO",                 "Enable/Disable sync with video hardware",           56,132,276,48},
  {NULL,NULL,"Bilinear Filter: OFF",        "Enable/Disable GX hardware texture filtering",      56,132,276,48},
  {NULL,NULL,"Deflickering Filter: AUTO",   "Enable/Disable GX hardware framebuffer filtering",  56,132,276,48},
#ifdef HW_RVL
  {NULL,NULL,"Trap Filter: ON",             "Enable/Disable VI hardware composite out filtering",56,132,276,48},
  {NULL,NULL,"Gamma Correction: 1.0",       "Adjust VI hardware gamma correction",               56,132,276,48},
#endif
  {NULL,NULL,"LCD Ghosting Filter: OFF",    "Enable/Disable software LCD image persistence",     56,132,276,48},
  {NULL,NULL,"NTSC Filter: COMPOSITE",      "Enable/Disable software NTSC filtering",            56,132,276,48},
  {NULL,NULL,"NTSC Sharpness: 0.0",         "Adjust edge contrast enhancement/blurring",         56,132,276,48},
  {NULL,NULL,"NTSC Resolution: 0.0",        "Adjust image resolution",                           56,132,276,48},
  {NULL,NULL,"NTSC Artifacts: 0.0",         "Adjust artifacts caused by color changes",          56,132,276,48},
  {NULL,NULL,"NTSC Color Bleed: 0.0",       "Adjust color resolution reduction",                 56,132,276,48},
  {NULL,NULL,"NTSC Color Fringing: 0.0",    "Adjust artifacts caused by brightness changes",     56,132,276,48},
  {NULL,NULL,"Borders: OFF",                "Enable/Disable overscan emulation",                 56,132,276,48},
  {NULL,NULL,"GG screen: ORIGINAL",         "Enable/Disable Game Gear extended screen",          56,132,276,48},
  {NULL,NULL,"Aspect: ORIGINAL (4:3)",      "Select display aspect ratio",                       56,132,276,48},
  {NULL,NULL,"Screen Position (+0,+0)",     "Adjust display position",                           56,132,276,48},
  {NULL,NULL,"Screen Scaling (+0,+0)",      "Adjust display scaling",                            56,132,276,48}
};

/* Menu options */
static gui_item items_prefs[] =
{
  {NULL,NULL,"Auto ROM Load: OFF",  "Enable/Disable automatic ROM loading on startup", 56,132,276,48},
  {NULL,NULL,"Auto Cheats: OFF",    "Enable/Disable automatic cheats activation",      56,132,276,48},
  {NULL,NULL,"Auto Saves: OFF",     "Enable/Disable automatic saves",                  56,132,276,48},
  {NULL,NULL,"ROM Load Device: SD", "Configure default device for ROM files",          56,132,276,48},
  {NULL,NULL,"Saves Device: FAT",   "Configure default device for Save files",         56,132,276,48},
  {NULL,NULL,"SFX Volume: 100",     "Adjust sound effects volume",                     56,132,276,48},
  {NULL,NULL,"BGM Volume: 100",     "Adjust background music volume",                  56,132,276,48},
  {NULL,NULL,"BG Overlay: ON",      "Enable/Disable background overlay",               56,132,276,48},
  {NULL,NULL,"Screen Width: 658",   "Adjust menu screen width in pixels",              56,132,276,48},
  {NULL,NULL,"Show CD Leds: OFF",   "Enable/Disable CD leds display",                  56,132,276,48},
  {NULL,NULL,"Show FPS: OFF",       "Enable/Disable FPS counter",                      56,132,276,48},
#ifdef HW_RVL
  {NULL,NULL,"Wiimote Timeout: OFF","Enable/Disable Wii remote automatic shutdown",    56,132,276,48},
  {NULL,NULL,"Wiimote Calibration: AUTO","Calibrate Wii remote pointer",               56,132,276,48},
#endif
};

/* Save Manager */
static gui_item items_saves[] =
{
  {NULL,NULL,"",""                   ,0,0,0,0},
  {NULL,NULL,"",""                   ,0,0,0,0},
  {NULL,NULL,"",""                   ,0,0,0,0},
  {NULL,NULL,"",""                   ,0,0,0,0},
  {NULL,NULL,"",""                   ,0,0,0,0},
  {NULL,NULL,"","Load file"          ,0,0,0,0},
  {NULL,NULL,"","Set as default file",0,0,0,0},
  {NULL,NULL,"","Delete file"        ,0,0,0,0},
  {NULL,NULL,"","Save file"          ,0,0,0,0}
};

/*****************************************************************************/
/*  Menu Buttons description                                                 */
/*****************************************************************************/

/* Generic Buttons for list menu */
static gui_butn arrow_up = {&arrow_up_data,BUTTON_ACTIVE|BUTTON_OVER_SFX,{0,0,0,0},14,76,360,32};
static gui_butn arrow_down = {&arrow_down_data,BUTTON_ACTIVE|BUTTON_OVER_SFX,{0,0,0,0},14,368,360,32};

/* Generic list menu */
static gui_butn buttons_list[] =
{
  {&button_text_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,1,0,0},56,132,276,48},
  {&button_text_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,1,0,0},56,188,276,48},
  {&button_text_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,1,0,0},56,244,276,48},
  {&button_text_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,1,0,0},56,300,276,48}
};

/* Main menu */
static gui_butn buttons_main[] =
{
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,0,0,1}, 80,140,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,0,1,1},246,140,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,0,1,0},412,140,148,132},
  {&button_icon_data,                             BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,4,0,1}, 80,194,148,132},
  {&button_icon_data,                             BUTTON_OVER_SFX                  ,{3,4,1,1},246,194,148,132},
  {&button_icon_data,                             BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,3,1,0},412,194,148,132},
  {NULL             ,                             BUTTON_OVER_SFX                  ,{3,1,0,2}, 10,334, 84, 32},
  {NULL             ,                             BUTTON_OVER_SFX                  ,{4,0,0,2}, 10,372, 84, 32},
  {NULL             ,                             BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,1,1,0},546,334, 84, 32},
  {NULL             ,                             BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,0,2,0},546,372, 84, 32}
};

/* Controllers Menu */
static gui_butn buttons_ctrls[] =
{
  {&button_icon_data    ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX                  ,{0,1,0,2}, 60, 88,148,132},
  {&button_icon_data    ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX                  ,{1,0,0,5}, 60,258,148,132},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,1,2,0},250, 79, 84, 32},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,1,3,0},250,117, 84, 32},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,1,4,0},250,155, 84, 32},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,1,5,0},250,193, 84, 32},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,1,5,0},250,249, 84, 32},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,1,6,0},250,287, 84, 32},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,1,7,0},250,325, 84, 32},
  {NULL                 ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{1,0,8,0},250,363, 84, 32},
  {&button_icon_sm_data ,BUTTON_SLIDE_RIGHT|BUTTON_OVER_SFX                            ,{0,1,1,0},436,168,160, 52},
  {&button_icon_sm_data ,BUTTON_SLIDE_RIGHT|BUTTON_OVER_SFX                            ,{1,1,0,0},436,232,160, 52},
  {&button_icon_sm_data ,BUTTON_SLIDE_RIGHT|BUTTON_OVER_SFX|BUTTON_SELECT_SFX          ,{1,0,0,0},436,296,160, 52}
};

/* Load Game menu */
static gui_butn buttons_load[] =
{
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3,0,1}, 80,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3,1,1},246,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3,1,0},412,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,0,0,1}, 80,264,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,0,1,1},246,264,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,0,1,0},412,264,148,132}
};

/* Options menu */
static gui_butn buttons_options[] =
{
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3,0,1}, 80,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,3,1,1},246,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{0,2,1,1},412,120,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{3,0,1,1},162,264,148,132},
  {&button_icon_data,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX|BUTTON_SELECT_SFX,{2,0,1,0},330,264,148,132}
};

/* Save Manager Menu */
static gui_butn buttons_saves[] =
{
  {&button_text_data   ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{0,1,0,0}, 56,102,276,48},
  {&button_text_data   ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,1,0,0}, 56,158,276,48},
  {&button_text_data   ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,1,0,0}, 56,214,276,48},
  {&button_text_data   ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,1,0,0}, 56,270,276,48},
  {&button_text_data   ,BUTTON_VISIBLE|BUTTON_ACTIVE|BUTTON_OVER_SFX,{1,0,0,0}, 56,326,276,48},
  {&button_load_data   ,BUTTON_ACTIVE|BUTTON_SLIDE_RIGHT|BUTTON_OVER_SFX,{0,1,0,0},530,130, 56,56},
  {&button_special_data,BUTTON_ACTIVE|BUTTON_SLIDE_RIGHT|BUTTON_OVER_SFX,{1,1,0,0},530,196, 56,56},
  {&button_delete_data ,BUTTON_ACTIVE|BUTTON_SLIDE_RIGHT|BUTTON_OVER_SFX,{1,1,0,0},530,262, 56,56},
  {&button_save_data   ,BUTTON_ACTIVE|BUTTON_SLIDE_RIGHT|BUTTON_OVER_SFX,{1,0,0,0},530,328, 56,56}
};

/*****************************************************************************/
/*  Menu descriptions                                                        */
/*****************************************************************************/

/* Main menu */
static gui_menu menu_main =
{
  "",
  0,0,
  10,10,4,0,
  items_main,
  buttons_main,
  bg_main,
  {NULL,NULL},
  {NULL,NULL},
  NULL
};

/* Main menu */
gui_menu menu_ctrls =
{
  "Controller Settings",
  0,0,
  13,13,8,0,
  items_ctrls,
  buttons_ctrls,
  bg_ctrls,
  {&action_cancel, &action_select},
  {NULL,NULL},
  ctrlmenu_cb
};

/* Load Game menu */
static gui_menu menu_load =
{
  "Load Game",
  0,0,
  6,6,5,0,
  items_load,
  buttons_load,
  bg_misc,
  {&action_cancel, &action_select},
  {NULL,NULL},
  NULL
};

/* Options menu */
static gui_menu menu_options =
{
  "Settings",
  0,0,
  5,5,5,0,
  items_options,
  buttons_options,
  bg_misc,
  {&action_cancel, &action_select},
  {NULL,NULL},
  NULL
};

/* System ROM Path submenu */
static gui_menu menu_rompaths =
{
  "System ROM Paths",
  0,0,
  12,4,6,0,
  items_rompaths,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  NULL
};

/* System Options menu */
static gui_menu menu_system =
{
  "System Settings",
  0,0,
  0,4,6,0,
  items_system,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  NULL
};

/* Video Options menu */
static gui_menu menu_video =
{
  "Video Settings",
  0,0,
  0,4,6,0,
  items_video,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  NULL
};

/* Sound Options menu */
static gui_menu menu_audio =
{
  "Audio Settings",
  0,0,
  0,4,6,0,
  items_audio,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  NULL
};

/* Sound Options menu */
static gui_menu menu_prefs =
{
  "Menu Settings",
  0,0,
  0,4,6,0,
  items_prefs,
  buttons_list,
  bg_list,
  {&action_cancel, &action_select},
  {&arrow_up,&arrow_down},
  NULL
};


/* Save Manager menu */
static gui_menu menu_saves =
{
  "Save Manager",
  0,0,
  9,9,8,0,
  items_saves,
  buttons_saves,
  bg_saves,
  {&action_cancel, &action_select},
  {NULL,NULL},
  savemenu_cb
};

/****************************************************************************
 * GUI Settings menu
 *
 ****************************************************************************/
static void update_screen_w(void)
{
  vmode->viWidth    = config.screen_w;
  vmode->viXOrigin  = (VI_MAX_WIDTH_NTSC -config.screen_w)/2;
  VIDEO_Configure(vmode);
  VIDEO_Flush();
}

static void update_bgm(void)
{
  SetVolumeOgg(((int)config.bgm_volume * MAX_VOLUME) / 100);
}

static void prefmenu ()
{
  int ret, quit = 0;
  gui_menu *m = &menu_prefs;
  gui_item *items = m->items;
  
  sprintf (items[0].text, "Auto ROM Load: %s", config.autoload ? "ON":"OFF");
  sprintf (items[1].text, "Auto Cheats: %s", config.autocheat ? "ON":"OFF");
  if (config.s_auto == 3) sprintf (items[2].text, "Auto Saves: ALL");
  else if (config.s_auto == 2) sprintf (items[2].text, "Auto Saves: STATE ONLY");
  else if (config.s_auto == 1) sprintf (items[2].text, "Auto Saves: SRAM ONLY");
  else sprintf (items[2].text, "Auto Saves: NONE");
#ifdef HW_RVL
  if (config.l_device == TYPE_USB) sprintf (items[3].text, "ROM Load Device: USB");
  else if (config.l_device == TYPE_DVD) sprintf (items[3].text, "ROM Load Device: DVD");
#else
  if (config.l_device == TYPE_DVD) sprintf (items[3].text, "ROM Load Device: DVD");
#endif
  else sprintf (items[3].text, "ROM Load Device: SD");
  if (config.s_device == 1) sprintf (items[4].text, "Saves Device: MCARD A");
  else if (config.s_device == 2) sprintf (items[4].text, "Saves Device: MCARD B");
  else sprintf (items[4].text, "Saves Device: FAT");
  sprintf (items[5].text, "SFX Volume: %1.1f", config.sfx_volume);
  sprintf (items[6].text, "BGM Volume: %1.1f", config.bgm_volume);
  sprintf (items[7].text, "BG Overlay: %s", config.bg_overlay ? "ON":"OFF");
  sprintf (items[8].text, "Screen Width: %d", config.screen_w);
  sprintf (items[9].text, "Show CD Leds: %s", config.cd_leds ? "ON":"OFF");
  sprintf (items[10].text, "Show FPS: %s", config.fps ? "ON":"OFF");
#ifdef HW_RVL
  sprintf (items[11].text, "Wiimote Timeout: %s", config.autosleep ? "5 MIN":"30 MIN");
  sprintf (items[12].text, "Wiimote Calibration: %s", ((config.calx * config.caly) != 0) ? "MANUAL":"AUTO");
  sprintf (items[12].comment, "%s", ((config.calx * config.caly) != 0) ? "Reset default Wii remote pointer calibration":"Calibrate Wii remote pointer");
  m->max_items = 13;
#else
  m->max_items = 11;
#endif

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("Menu "));

  while (quit == 0)
  {
    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0:   /* Auto load last ROM file on startup */
        config.autoload ^= 1;
        sprintf (items[0].text, "Auto ROM Load: %s", config.autoload ? "ON":"OFF");
        break;

      case 1:   /* Cheats automatic activation */
        config.autocheat ^= 1;
        sprintf (items[1].text, "Auto Cheats: %s", config.autocheat ? "ON":"OFF");
        break;

      case 2:  /*** Auto load/save STATE & SRAM files ***/
        config.s_auto = (config.s_auto + 1) % 4;
        if (config.s_auto == 3) sprintf (items[2].text, "Auto Saves: ALL");
        else if (config.s_auto == 2) sprintf (items[2].text, "Auto Saves: STATE ONLY");
        else if (config.s_auto == 1) sprintf (items[2].text, "Auto Saves: SRAM ONLY");
        else sprintf (items[2].text, "Auto Saves: NONE");
        break;

      case 3:   /*** Default ROM device ***/
#ifdef HW_RVL
        config.l_device = (config.l_device + 1) % (TYPE_DVD + 1);
        if (config.l_device == TYPE_USB) sprintf (items[3].text, "ROM Load Device: USB");
        else if (config.l_device == TYPE_DVD) sprintf (items[3].text, "ROM Load Device: DVD");
#else
        config.l_device ^= 1;
        if (config.l_device == TYPE_DVD) sprintf (items[3].text, "ROM Load Device: DVD");
#endif
        else sprintf (items[3].text, "ROM Load Device: SD");
        break;

      case 4:   /*** Default saves device ***/
        config.s_device = (config.s_device + 1) % 3;
        if (config.s_device == 1) sprintf (items[4].text, "Saves Device: MCARD A");
        else if (config.s_device == 2) sprintf (items[4].text, "Saves Device: MCARD B");
        else sprintf (items[4].text, "Saves Device: FAT");
        break;

      case 5:   /*** Sound effects volume ***/
        GUI_OptionBox(m,0,"SFX Volume",(void *)&config.sfx_volume,10.0,0.0,100.0,0);
        sprintf (items[5].text, "SFX Volume: %1.1f", config.sfx_volume);
        break;

      case 6:   /*** Background music volume ***/
        GUI_OptionBox(m,update_bgm,"BGM Volume",(void *)&config.bgm_volume,10.0,0.0,100.0,0);
        sprintf (items[6].text, "BGM Volume: %1.1f", config.bgm_volume);
        break;

      case 7:   /*** Background overlay ***/
        config.bg_overlay ^= 1;
        if (config.bg_overlay)
        {
          bg_main[1].state  |= IMAGE_VISIBLE;
          bg_misc[1].state  |= IMAGE_VISIBLE;
          bg_ctrls[1].state |= IMAGE_VISIBLE;
          bg_list[1].state  |= IMAGE_VISIBLE;
          bg_saves[2].state |= IMAGE_VISIBLE;
          sprintf (items[7].text, "BG Overlay: ON");
        }
        else
        {
          bg_main[1].state  &= ~IMAGE_VISIBLE;
          bg_misc[1].state  &= ~IMAGE_VISIBLE;
          bg_ctrls[1].state &= ~IMAGE_VISIBLE;
          bg_list[1].state  &= ~IMAGE_VISIBLE;
          bg_saves[2].state &= ~IMAGE_VISIBLE;
          sprintf (items[7].text, "BG Overlay: OFF");
        }
        break;

      case 8:   /*** Screen Width ***/
        GUI_OptionBox(m,update_screen_w,"Screen Width",(void *)&config.screen_w,2,640,VI_MAX_WIDTH_NTSC,1);
        sprintf (items[8].text, "Screen Width: %d", config.screen_w);
        break;

      case 9:   /*** CD LEDS ***/
        config.cd_leds ^= 1;
        sprintf (items[9].text, "Show CD Leds: %s", config.cd_leds ? "ON":"OFF");
        break;

      case 10:   /*** FPS counter ***/
        config.fps ^= 1;
        sprintf (items[10].text, "Show FPS: %s", config.fps ? "ON":"OFF");
        break;

#ifdef HW_RVL
      case 11:   /*** Wii remote auto switch-off ***/
        config.autosleep ^= 1;
        sprintf (items[11].text, "Wiimote Timeout: %s", config.autosleep ? "5min":"30min");
        WPAD_SetIdleTimeout(config.autosleep ? 300 : 1800);
        break;

      case 12:   /*** Wii remote pointer calibration ***/
        if ((config.calx * config.caly) == 0)
        {
          if (GUI_WaitConfirm("Pointer Calibration","Aim center of TV screen"))
          {
            sprintf (items[12].text, "Wiimote Calibration: MANUAL");
            sprintf (items[12].comment, "Reset default Wii remote pointer calibration");
            config.calx = 320 - m_input.ir.x;
            config.caly = 240 - m_input.ir.y;
            m_input.ir.x = 320;
            m_input.ir.y = 240;
          }
        }
        else
        {
          sprintf (items[12].text, "Wiimote Calibration: AUTO");
          sprintf (items[12].comment, "Calibrate Wii remote pointer");
          config.calx = config.caly = 0;
        }
        break;
#endif

      case -1:
        quit = 1;
        break;
    }
  }

  /* stop DVD drive when not in use */
  if (config.l_device != TYPE_DVD)
  {
#ifdef HW_RVL
    DI_StopMotor();
#else
    dvdcmdblk blk;
    DVD_StopMotor(&blk);
#endif
  }

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * Audio Settings menu
 *
 ****************************************************************************/
static void soundmenu ()
{
  int ret, quit = 0;
  float fm_volume = (float)config.fm_preamp/100.0;
  float psg_volume = (float)config.psg_preamp/100.0;
  float cdda_volume = (float)config.cdda_volume/100.0;
  float pcm_volume = (float)config.pcm_volume/100.0;
  gui_menu *m = &menu_audio;
  gui_item *items = m->items;

  if (config.ym2413 == 0) sprintf (items[0].text, "Master System FM: OFF");
  else if (config.ym2413 == 1) sprintf (items[0].text, "Master System FM: ON");
  else sprintf (items[0].text, "Master System FM: AUTO");

  if (config.ym2612 == YM2612_DISCRETE) sprintf (items[1].text, "YM2612 Type: DISCRETE");
  else if (config.ym2612 == YM2612_INTEGRATED) sprintf (items[1].text, "YM2612 Type: ASIC");
  else sprintf (items[1].text, "YM2612 Type: ENHANCED");

  sprintf (items[2].text, "High-Quality FM: %s", config.hq_fm ? "ON":"OFF");
  sprintf (items[3].text, "High-Quality PSG: %s", config.hq_psg? "ON":"OFF");

  sprintf (items[4].text, "FM Volume: %1.2f", fm_volume);
  sprintf (items[5].text, "PSG Volume: %1.2f", psg_volume);
  sprintf (items[6].text, "CD-DA Volume: %1.2f", cdda_volume);
  sprintf (items[7].text, "PCM Volume: %1.2f", pcm_volume);

  sprintf (items[8].text, "Audio Output: %s", config.mono ? "MONO":"STEREO");

  if (config.filter == 2)
  {
    float lg = (float)config.lg/100.0;
    float mg = (float)config.mg/100.0;
    float hg = (float)config.hg/100.0;

    sprintf(items[9].text, "Filtering: 3-BAND EQ");
    sprintf(items[10].text, "Low Gain: %1.2f", lg);
    strcpy(items[10].comment, "Adjust EQ Low Band Gain");
    sprintf(items[11].text, "Middle Gain: %1.2f", mg);
    sprintf(items[12].text, "High Gain: %1.2f", hg);
    sprintf(items[13].text, "Low Freq: %d", config.low_freq);
    sprintf(items[14].text, "High Freq: %d", config.high_freq);
    m->max_items = 15;
  }
  else if (config.filter == 1)
  {
    int16 lp_range = (config.lp_range * 100 + 0xffff) / 0x10000;
    sprintf (items[9].text, "Filtering: LOW-PASS");
    sprintf (items[10].text, "Low-Pass Rate: %d %%", lp_range);
    strcpy (items[10].comment, "Adjust Low Pass filter");
    m->max_items = 11;
  }
  else
  {
    sprintf (items[9].text, "Filtering: OFF");
    m->max_items = 10;
  }

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("Audio "));

  while (quit == 0)
  {
    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0:
      {
        config.ym2413++;
        if (config.ym2413 > 2) config.ym2413 = 0;
        if (config.ym2413 == 0) sprintf (items[0].text, "Master System FM: OFF");
        else if (config.ym2413 == 1) sprintf (items[0].text, "Master System FM: ON");
        else sprintf (items[0].text, "Master System FM: AUTO");

        /* Automatic detection */
        if ((config.ym2413 & 2) && system_hw && ((system_hw & SYSTEM_PBC) != SYSTEM_MD))
        {
          /* detect if game is using YM2413 */
          sms_cart_init();

          /* restore SRAM */
          slot_autoload(0,config.s_device);
        }
        break;
      }

      case 1:
      {
        config.ym2612++;
        if (config.ym2612 > YM2612_ENHANCED) config.ym2612 = YM2612_DISCRETE;
        if (config.ym2612 == YM2612_DISCRETE) sprintf (items[1].text, "YM2612 Type: DISCRETE");
        else if (config.ym2612 == YM2612_INTEGRATED) sprintf (items[1].text, "YM2612 Type: ASIC");
        else sprintf (items[1].text, "YM2612 Type: ENHANCED");
        YM2612Config(config.ym2612);
        break;
      }

      case 2:
      {
        config.hq_fm ^= 1;
        sprintf (items[2].text, "High-Quality FM: %s", config.hq_fm ? "ON":"OFF");
        break;
      }

      case 3:
      {
        config.hq_psg ^= 1;
        sprintf (items[3].text, "High-Quality PSG: %s", config.hq_psg ? "ON":"OFF");
        break;
      }

      case 4:
      {
        GUI_OptionBox(m,0,"FM Volume",(void *)&fm_volume,0.01,0.0,5.0,0);
        sprintf (items[4].text, "FM Volume: %1.2f", fm_volume);
        config.fm_preamp = (int)(fm_volume * 100.0 + 0.5);
        break;
      }

      case 5:
      {
        GUI_OptionBox(m,0,"PSG Volume",(void *)&psg_volume,0.01,0.0,5.0,0);
        sprintf (items[5].text, "PSG Volume: %1.2f", psg_volume);
        config.psg_preamp = (int)(psg_volume * 100.0 + 0.5);
        if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
        {
          psg_config(0, config.psg_preamp, 0xff);
        }
        else
        {
          psg_config(0, config.psg_preamp, io_reg[6]);
        }
        break;
      }

      case 6:
      {
        GUI_OptionBox(m,0,"CD-DA Volume",(void *)&cdda_volume,0.01,0.0,1.0,0);
        sprintf (items[6].text, "CD-DA Volume: %1.2f", cdda_volume);
        config.cdda_volume = (int)(cdda_volume * 100.0 + 0.5);
        break;
      }

      case 7:
      {
        GUI_OptionBox(m,0,"PCM Volume",(void *)&pcm_volume,0.01,0.0,1.0,0);
        sprintf (items[7].text, "PCM Volume: %1.2f", pcm_volume);
        config.pcm_volume = (int)(pcm_volume * 100.0 + 0.5);
        break;
      }

      case 8:
      {
        config.mono ^= 1;
        sprintf (items[8].text, "Audio Out: %s", config.mono ? "MONO":"STEREO");
        break;
      }

      case 9:
      {
        config.filter = (config.filter + 1) % 3;
        if (config.filter == 2)
        {
          float lg = (float)config.lg/100.0;
          sprintf (items[9].text, "Filtering: 3-BAND EQ");
          sprintf (items[10].text, "Low Gain: %1.2f", lg);
          strcpy (items[10].comment, "Adjust EQ Low Band Gain");
          m->max_items = 15;
          audio_set_equalizer();
        }
        else if (config.filter == 1)
        {
          int lp_range = (config.lp_range * 100 + 0xffff) / 0x10000;
          sprintf (items[9].text, "Filtering: LOW-PASS");
          sprintf (items[10].text, "Low-Pass Rate: %d %%", lp_range);
          strcpy (items[10].comment, "Adjust Low Pass filter");
          m->max_items = 11;
        }
        else
        {
          sprintf (items[9].text, "Filtering: OFF");
          m->max_items = 10;
        }

        while ((m->offset + 4) > m->max_items)
        {
          m->offset--;
          m->selected++;
        }
        break;
      }

      case 10:
      {
        if (config.filter == 1)
        {
          int16 lp_range = (config.lp_range * 100 + 0xffff) / 0x10000;
          GUI_OptionBox(m,0,"Low-Pass Rate (%)",(void *)&lp_range,1,0,100,1);
          sprintf (items[10].text, "Low-Pass Rate: %d %%", lp_range);
          config.lp_range = (lp_range * 0x10000) / 100;
        }
        else
        {
          float lg = (float)config.lg/100.0;
          GUI_OptionBox(m,0,"Low Gain",(void *)&lg,0.01,0.0,2.0,0);
          sprintf (items[10].text, "Low Gain: %1.2f", lg);
          config.lg = (int)(lg * 100.0);
          audio_set_equalizer();
        }
        break;
      }

      case 11:
      {
        float mg = (float)config.mg/100.0;
        GUI_OptionBox(m,0,"Middle Gain",(void *)&mg,0.01,0.0,2.0,0);
        sprintf (items[11].text, "Middle Gain: %1.2f", mg);
        config.mg = (int)(mg * 100.0);
        audio_set_equalizer();
        break;
      }

      case 12:
      {
        float hg = (float)config.hg/100.0;
        GUI_OptionBox(m,0,"High Gain",(void *)&hg,0.01,0.0,2.0,0);
        sprintf (items[12].text, "High Gain: %1.2f", hg);
        config.hg = (int)(hg * 100.0);
        audio_set_equalizer();
        break;
      }

      case 13:
      {
        GUI_OptionBox(m,0,"Low Frequency",(void *)&config.low_freq,10,0,config.high_freq,1);
        sprintf (items[13].text, "Low Freq: %d", config.low_freq);
        audio_set_equalizer();
        break;
      }

      case 14:
      {
        GUI_OptionBox(m,0,"High Frequency",(void *)&config.high_freq,100,config.low_freq,30000,1);
        sprintf (items[14].text, "High Freq: %d", config.high_freq);
        audio_set_equalizer();
        break;
      }

      case -1:
      {
        quit = 1;
        break;
      }
    }
  }

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * System ROM paths menu
 *
 ****************************************************************************/

static void rompathmenu ()
{
  FILE *f;
  int i, ret;
  gui_menu *m = &menu_rompaths;
  gui_item *items = m->items;
  
  for (i=0; i<12; i++)
  {
    f = fopen(config.sys_rom[i],"rb");
    if (f)
    {
      strcpy(strstr(items[i].text,":") + 2, "FOUND");
      fclose(f);
    }
    else
    {
      strcpy(strstr(items[i].text,":") + 2, "NOT FOUND");
    }
  }

  GUI_InitMenu(m);

  while (1)
  {
    ret = GUI_RunMenu(m);

    if (ret < 0)
    {
      GUI_DeleteMenu(m);
      return;
    }
    
    /* Initialize System ROM browser */
    if (OpenDirectory(config.l_device, ret + FILETYPE_MAX))
    {
      /* Get current System ROM path */
      char *dir = GetCurrentDirectory();

      /* Open System ROM browser */
      GUI_DeleteMenu(m);
      i = FileSelector(ret + FILETYPE_MAX);

      /* System ROM selected ? */
      if (i >= 0)
      {
        /* full System ROM pathname */
        sprintf(config.sys_rom[ret],"%s%s",dir,filelist[i].filename);

        /* mark System ROM as found */
        strcpy(strstr(items[ret].text,":") + 2, "FOUND");

        /* BOOT ROM special cases */
        switch (ret)
        {
          case 0:
          case 1:
          case 2:
          {
            /* force CD BOOT ROM reloading */
            system_bios &= ~0x10;
            break;
          }

          case 3:
          {
            /* Genesis BOOT ROM must be reloaded */
            if (load_archive(MD_BIOS, boot_rom, 0x800, NULL) > 0)
            {
              /* check if BOOT ROM header is valid */
              if (!memcmp((char *)(boot_rom + 0x120),"GENESIS OS", 10))
              {
                /* mark Genesis BOOT ROM as loaded */
                system_bios |= SYSTEM_MD;
              }
              else
              {
                /* mark Genesis BOOT ROM as unloaded */
                system_bios &= ~SYSTEM_MD;
              }
            }
            break;
          }

          case 4:
          case 5:
          case 6:
          {
            /* force Master System BOOT ROM reloading */
            system_bios &= ~SYSTEM_SMS;
            break;
          }

          case 7:
          {
            /* force Game Gear BOOT ROM reloading */
            system_bios &= ~SYSTEM_GG;
            break;
          }

          default:
          {
            /* not a BOOT ROM */
            break;
          }
        }
      }

      free(dir);
      GUI_InitMenu(m);
    }
  }
}

/****************************************************************************
 * System Settings menu
 *
 ****************************************************************************/
static const uint16 vc_table[4][2] = 
{
  /* NTSC, PAL */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0xEA , 0x102}, /* Mode 5 (224 lines) */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0x106, 0x10A}  /* Mode 5 (240 lines) */
};

static void systemmenu ()
{
  int ret, quit = 0;
  int reinit = 0;
  gui_menu *m = &menu_system;
  gui_item *items = m->items;

  if (config.system == 0)
    sprintf (items[0].text, "System: AUTO");
  else if (config.system == SYSTEM_SG)
    sprintf (items[0].text, "System: SG-1000");
  else if (config.system == SYSTEM_SGII)
    sprintf (items[0].text, "System: SG-1000 II");
  else if (config.system == SYSTEM_SGII_RAM_EXT)
    sprintf (items[0].text, "System: SG-1000 + RAM EXT.");
  else if (config.system == SYSTEM_MARKIII)
    sprintf (items[0].text, "System: MARK-III");
  else if (config.system == SYSTEM_SMS)
    sprintf (items[0].text, "System: MASTER SYSTEM");
  else if (config.system == SYSTEM_SMS2)
    sprintf (items[0].text, "System: MASTER SYSTEM II");
  else if (config.system == SYSTEM_GG)
    sprintf (items[0].text, "System: GAME GEAR");
  else if (config.system == SYSTEM_MD)
    sprintf (items[0].text, "System: MEGA DRIVE/GENESIS");

  if (config.region_detect == 0)
    sprintf (items[1].text, "Region: AUTO");
  else if (config.region_detect == 1)
    sprintf (items[1].text, "Region: USA");
  else if (config.region_detect == 2)
    sprintf (items[1].text, "Region: PAL");
  else if (config.region_detect == 3)
    sprintf (items[1].text, "Region: JAPAN");

  if (config.vdp_mode == 0)
    sprintf (items[2].text, "VDP Mode: AUTO");
  else if (config.vdp_mode == 1)
    sprintf (items[2].text, "VDP Mode: NTSC");
  else if (config.vdp_mode == 2)
    sprintf (items[2].text, "VDP Mode: PAL");

  if (config.master_clock == 0)
    sprintf (items[3].text, "System Clock: AUTO");
  else if (config.master_clock == 1)
    sprintf (items[3].text, "System Clock: NTSC");
  else if (config.master_clock == 2)
    sprintf (items[3].text, "System Clock: PAL");

  sprintf (items[4].text, "System Boot: %s", (config.bios & 1) ? ((config.bios & 2) ? "BIOS->CART" : "BIOS ONLY") : "CART");
  sprintf (items[5].text, "System Lockups: %s", config.force_dtack ? "OFF" : "ON");
  sprintf (items[6].text, "68k Address Error: %s", config.addr_error ? "ON" : "OFF");
  sprintf (items[7].text, "CD Access Time: %s", config.cd_latency ? "ON" : "OFF");

  if (config.add_on == HW_ADDON_AUTO)
    sprintf (items[8].text, "CD Add-on: AUTO");
  else if (config.add_on == HW_ADDON_MEGACD)
    sprintf (items[8].text, "CD Add-on: MEGA/SEGA CD");
  else if (config.add_on == HW_ADDON_MEGASD)
    sprintf (items[8].text, "CD Add-on: MEGASD");
  else
    sprintf (items[8].text, "CD Add-on: NONE");

  if (config.lock_on == TYPE_GG)
    sprintf (items[9].text, "Lock-On: GAME GENIE");
  else if (config.lock_on == TYPE_AR)
    sprintf (items[9].text, "Lock-On: ACTION REPLAY");
  else if (config.lock_on == TYPE_SK)
    sprintf (items[9].text, "Lock-On: SONIC&KNUCKLES");
  else
    sprintf (items[9].text, "Lock-On: OFF");

  sprintf (items[10].text, "Cartridge Swap: %s", (config.hot_swap & 1) ? "ON":"OFF");

  if (config.m68k_overclock > 1.0)
    sprintf (items[12].text, "Main 68k Overclock: %1.1fx", config.m68k_overclock);
  else
    sprintf (items[12].text, "Main 68k Overclock: OFF");

  if (config.s68k_overclock > 1.0)
    sprintf (items[13].text, "Sub 68k Overclock: %1.1fx", config.s68k_overclock);
  else
    sprintf (items[13].text, "Sub 68k Overclock: OFF");

  if (config.z80_overclock > 1.0)
    sprintf (items[14].text, "Z80 Overclock: %1.1fx", config.z80_overclock);
  else
    sprintf (items[14].text, "Z80 Overclock: OFF");

  if (svp)
  {
    sprintf (items[15].text, "SVP Cycles: %d", SVP_cycles);
    m->max_items = 16;
  }
  else
  {
    m->max_items = 15;
  }

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("System "));

  while (quit == 0)
  {
    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0:  /*** Force System Hardware ***/
      {
        if (config.system == SYSTEM_MD)
        {
          config.system = 0;
          sprintf (items[0].text, "System: AUTO");

          /* Default system hardware (auto) */
          if (system_hw) system_hw = romtype;
        }
        else if (config.system == 0)
        {
          config.system = SYSTEM_SG;
          sprintf (items[0].text, "System: SG-1000");
          if (system_hw) system_hw = SYSTEM_SG;
        }
        else if (config.system == SYSTEM_SG)
        {
          config.system = SYSTEM_SGII;
          sprintf (items[0].text, "System: SG-1000 II");
          if (system_hw) system_hw = SYSTEM_SGII;
        }
        else if (config.system == SYSTEM_SGII)
        {
          config.system = SYSTEM_SGII_RAM_EXT;
          sprintf (items[0].text, "System: SG-1000 + RAM EXT.");
          if (system_hw) system_hw = SYSTEM_SGII_RAM_EXT;
        }
        else if (config.system == SYSTEM_SGII_RAM_EXT)
        {
          config.system = SYSTEM_MARKIII;
          sprintf (items[0].text, "System: MARK-III");
          if (system_hw) system_hw = SYSTEM_MARKIII;
        }
        else if (config.system == SYSTEM_MARKIII)
        {
          config.system = SYSTEM_SMS;
          sprintf (items[0].text, "System: MASTER SYSTEM");
          if (system_hw) system_hw = SYSTEM_SMS;
        }
        else if (config.system == SYSTEM_SMS)
        {
          config.system = SYSTEM_SMS2;
          sprintf (items[0].text, "System: MASTER SYSTEM II");
          if (system_hw) system_hw = SYSTEM_SMS2;
        }
        else if (config.system == SYSTEM_SMS2)
        {
          config.system = SYSTEM_GG;
          sprintf (items[0].text, "System: GAME GEAR");

          if (romtype == SYSTEM_GG)
          {
            /* Game Gear mode  */
            if (system_hw) system_hw = SYSTEM_GG;
          }
          else
          {
            /* Game Gear in MS compatibility mode  */
            if (system_hw) system_hw = SYSTEM_GGMS;
          }
        }
        else if (config.system == SYSTEM_GG)
        {
          config.system = SYSTEM_MD;
          sprintf (items[0].text, "System: MEGA DRIVE/GENESIS");

          if (romtype & SYSTEM_MD)
          {
            /* Default mode */
            if (system_hw) system_hw = romtype;
          }
          else
          {
            /* Mega Drive in MS compatibility mode  */
            if (system_hw) system_hw = SYSTEM_PBC;
          }
        }

        if (system_hw)
        {
          /* restore previous input settings */
          if (old_system[0] != -1)
          {
            input.system[0] = old_system[0];
          }
          if (old_system[1] != -1)
          {
            input.system[1] = old_system[1];
          }

          /* reinitialize audio streams */
          audio_init(snd.sample_rate, snd.frame_rate);

          /* force hard reset */
          system_init();
          system_reset();

          /* restore SRAM */
          slot_autoload(0,config.s_device);
        }
        
        break;
      }

      case 1:  /*** Force Region ***/
      {
        config.region_detect = (config.region_detect + 1) % 4;
        if (config.region_detect == 0)
          sprintf (items[1].text, "Region: AUTO");
        else if (config.region_detect == 1)
          sprintf (items[1].text, "Region: USA");
        else if (config.region_detect == 2)
          sprintf (items[1].text, "Region: PAL");
        else if (config.region_detect == 3)
          sprintf (items[1].text, "Region: JAPAN");

        /* force system reinitialization + region BIOS */
        reinit = 2;
        break;
      }

      case 2:  /*** Force VDP mode ***/
      {
        config.vdp_mode = (config.vdp_mode + 1) % 3;
        if (config.vdp_mode == 0)
          sprintf (items[2].text, "VDP Mode: AUTO");
        else if (config.vdp_mode == 1)
          sprintf (items[2].text, "VDP Mode: NTSC");
        else if (config.vdp_mode == 2)
          sprintf (items[2].text, "VDP Mode: PAL");

        /* force system reinitialization */
        reinit = 1;
        break;
      }

      case 3:  /*** Force Master Clock ***/
      {
        config.master_clock = (config.master_clock + 1) % 3;
        if (config.master_clock == 0)
          sprintf (items[3].text, "System Clock: AUTO");
        else if (config.master_clock == 1)
          sprintf (items[3].text, "System Clock: NTSC");
        else if (config.master_clock == 2)
          sprintf (items[3].text, "System Clock: PAL");

        /* force system reinitialization */
        reinit = 1;
        break;
      }

      case 4:  /*** BIOS support ***/
      {
        if (config.bios == 0) config.bios = 3;
        else if (config.bios == 3) config.bios = 1;
        else config.bios = 0;
        sprintf (items[4].text, "System Boot: %s", (config.bios & 1) ? ((config.bios & 2) ? "BIOS->CART " : "BIOS ONLY") : "CART");
        if ((system_hw == SYSTEM_MD) || (system_hw & SYSTEM_GG) || (system_hw & SYSTEM_SMS))
        {
          /* force hard reset */
          system_init();
          system_reset();

          /* restore SRAM */
          slot_autoload(0,config.s_device);
        }
        break;
      }

      case 5:  /*** force DTACK ***/
      {
        config.force_dtack ^= 1;
        sprintf (items[5].text, "System Lockups: %s", config.force_dtack ? "OFF" : "ON");
        break;
      }

      case 6:  /*** 68k Address Error ***/
      {
        config.addr_error ^= 1;
        m68k.aerr_enabled = config.addr_error; 
        sprintf (items[6].text, "68k Address Error: %s", config.addr_error ? "ON" : "OFF");
        break;
      }

      case 7:  /*** CD Access Time ***/
      {
        config.cd_latency ^= 1;
        sprintf (items[7].text, "CD Access Time: %s", config.cd_latency ? "ON" : "OFF");
        break;
      }

      case 8:  /*** CD add-on ***/
      {
        config.add_on = (config.add_on + 1) % (HW_ADDON_NONE + 1);
        if (config.add_on == HW_ADDON_AUTO)
          sprintf (items[8].text, "CD Add-on: AUTO");
        else if (config.add_on == HW_ADDON_MEGACD)
          sprintf (items[8].text, "CD Add-on: MEGA/SEGA CD");
        else if (config.add_on == HW_ADDON_MEGASD)
          sprintf (items[8].text, "CD Add-on: MEGASD");
        else
          sprintf (items[8].text, "CD Add-on: NONE");
        break;
      }

      case 9:  /*** Cart Lock-On ***/
      {
        config.lock_on = (config.lock_on + 1) % (TYPE_SK + 1);
        if (config.lock_on == TYPE_GG)
          sprintf (items[9].text, "Lock-On: GAME GENIE");
        else if (config.lock_on == TYPE_AR)
          sprintf (items[9].text, "Lock-On: ACTION REPLAY");
        else if (config.lock_on == TYPE_SK)
          sprintf (items[9].text, "Lock-On: SONIC&KNUCKLES");
        else
          sprintf (items[9].text, "Lock-On: OFF");

        if ((system_hw == SYSTEM_MD) || (system_hw == SYSTEM_PICO))
        {
          /* force hard reset */
          system_init();
          system_reset();

          /* restore SRAM */
          slot_autoload(0,config.s_device);

          /* Action Replay switch */
          if (areplay_get_status() < 0)
          {
            menu_main.buttons[6].state &= ~(BUTTON_VISIBLE | BUTTON_ACTIVE);
            menu_main.items[6].data = NULL;
            menu_main.cb = NULL;
            menu_main.buttons[3].shift[1] = 4;
            menu_main.buttons[7].shift[0] = 4;
            menu_main.buttons[8].shift[2] = 1;
          }
          else
          {
            menu_main.buttons[6].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
            menu_main.items[6].data = Button_sm_grey_png;
            menu_main.cb = mainmenu_cb;
            menu_main.buttons[3].shift[1] = 3;
            menu_main.buttons[7].shift[0] = 1;
            menu_main.buttons[8].shift[2] = 2;
          }
        }
        break;
      }

      case 10:  /*** Cartridge Hot Swap ***/
      {
        config.hot_swap ^= 1;
        sprintf (items[10].text, "Cartridge Swap: %s", (config.hot_swap & 1) ? "ON":"OFF");
        break;
      }

      case 11:  /*** System ROM paths ***/
      {
        GUI_DeleteMenu(m);
        rompathmenu();
        GUI_InitMenu(m);
        break;
      }

      case 12:  /*** Main 68k Overclock ***/
      {
        GUI_OptionBox(m,0,"Main 68k Overclock Ratio",(void *)&config.m68k_overclock,0.1,1.0,7.0,0);
        if (config.m68k_overclock > 1.0)
          sprintf (items[12].text, "Main 68k Overclock: %1.1fx", config.m68k_overclock);
        else
          sprintf (items[12].text, "Main 68k Overclock: OFF");
        break;
      }

      case 13:  /*** Sub 68k Overclock ***/
      {
        GUI_OptionBox(m,0,"Sub 68k Overclock Ratio",(void *)&config.s68k_overclock,0.1,1.0,4.0,0);
        if (config.s68k_overclock > 1.0)
          sprintf (items[13].text, "Sub 68k Overclock: %1.1fx", config.s68k_overclock);
        else
          sprintf (items[13].text, "Sub 68k Overclock: OFF");
        break;
      }

      case 14:  /*** Z80 Overclock ***/
      {
        GUI_OptionBox(m,0,"Z80 Overclock Ratio",(void *)&config.z80_overclock,0.1,1.0,15.0,0);
        if (config.z80_overclock > 1.0)
          sprintf (items[14].text, "Z80 Overclock: %1.1fx", config.z80_overclock);
        else
          sprintf (items[14].text, "Z80 Overclock: OFF");
        break;
      }

      case 15:  /*** SVP cycles per line ***/
      {
        GUI_OptionBox(m,0,"SVP Cycles",(void *)&SVP_cycles,1,1,1500,1);
        sprintf (items[15].text, "SVP Cycles: %d", SVP_cycles);
        break;
      }

      case -1:
      {
        quit = 1;
        break;
      }
    }
  }

  if (reinit && system_hw)
  {
    /* reinitialize console region */
    get_region(NULL);

    /* framerate might have changed, reinitialize audio timings */
    audio_init(snd.sample_rate, get_framerate());

    /* system with region BIOS should be reinitialized if region code has changed */
    if ((reinit & 2) && ((system_hw == SYSTEM_MCD) || ((system_hw & SYSTEM_SMS) && (config.bios & 1))))
    {
       system_init();
       system_reset();

       /* restore SRAM */
       slot_autoload(0,config.s_device);
    }
    else
    {
      /* reinitialize I/O region register */
      if (system_hw == SYSTEM_MD)
      {
        io_reg[0x00] = 0x20 | region_code | (config.bios & 1);
      }
      else if (system_hw == SYSTEM_MCD)
      {
        io_reg[0x00] = region_code | (config.bios & 1);
      }
      else
      {
        io_reg[0x00] = 0x80 | (region_code >> 1);
      }

      /* reinitialize VDP timings */
      lines_per_frame = vdp_pal ? 313 : 262;
 
      /* reinitialize NTSC/PAL mode in VDP status */
      if (system_hw & SYSTEM_MD)
      {
        status = (status & ~1) | vdp_pal;
      }

      /* reinitialize VC max value */
      switch (bitmap.viewport.h)
      {
        case 192:
          vc_max = vc_table[0][vdp_pal];
          break;
        case 224:
          vc_max = vc_table[1][vdp_pal];
          break;
        case 240:
          vc_max = vc_table[3][vdp_pal];
          break;
      }
    }
  }

  /* Initialize CPU overclock ratio */
  m68k.cycle_ratio = (100 << M68K_OVERCLOCK_SHIFT) / (int)(config.m68k_overclock * 100.0);
  s68k.cycle_ratio = (100 << M68K_OVERCLOCK_SHIFT) / (int)(config.s68k_overclock * 100.0);
  z80_cycle_ratio  = (100 << Z80_OVERCLOCK_SHIFT) / (int)(config.z80_overclock * 100.0);

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * Video Settings menu
 *
 ****************************************************************************/
#ifdef HW_RVL
#define VI_OFFSET 7
static void update_gamma(void)
{
  VIDEO_SetGamma((int)(config.gamma * 10.0));
  VIDEO_Flush();
}
#else
#define VI_OFFSET 5
#endif

static void videomenu ()
{
  u16 state[2];
  int ret, quit = 0;
  int reinit = 0;
  gui_menu *m = &menu_video;
  gui_item *items = m->items;
  int ntsc_offset = 0;

  if (config.render == 1)
    sprintf (items[0].text,"Display: INTERLACED");
  else if (config.render == 2)
    sprintf (items[0].text, "Display: PROGRESSIVE");
  else
    sprintf (items[0].text, "Display: ORIGINAL");

  if (config.tv_mode == 0)
    sprintf (items[1].text, "TV Mode: 60HZ");
  else if (config.tv_mode == 1)
    sprintf (items[1].text, "TV Mode: 50HZ");
  else
    sprintf (items[1].text, "TV Mode: 50/60HZ");

  if (config.vsync)
    sprintf (items[2].text, "VSYNC: AUTO");
  else
    sprintf (items[2].text, "VSYNC: OFF");

  sprintf (items[3].text, "Bilinear Filter: %s", config.bilinear ? " ON" : "OFF");
  sprintf (items[4].text, "Deflickering Filter: %s", config.vfilter ? "AUTO" : "OFF");

#ifdef HW_RVL
  sprintf (items[5].text, "Trap Filter: %s", config.trap ? " ON" : "OFF");
  sprintf (items[6].text, "Gamma Correction: %1.1f", config.gamma);
#endif

  if (config.lcd > 0)
  {
    int ratio = ((config.lcd * 100) + 254) / 255;
    sprintf (items[VI_OFFSET].text, "LCD Ghosting Filter: %d%%", ratio);
  }
  else
  {
    sprintf (items[VI_OFFSET].text, "LCD Ghosting Filter: OFF");
  }
  
  if (config.ntsc == 1)
    sprintf (items[VI_OFFSET+1].text, "NTSC Filter: COMPOSITE");
  else if (config.ntsc == 2)
    sprintf (items[VI_OFFSET+1].text, "NTSC Filter: S-VIDEO");
  else if (config.ntsc == 3)
    sprintf (items[VI_OFFSET+1].text, "NTSC Filter: RGB");
  else if (config.ntsc == 4)
  {
    sprintf (items[VI_OFFSET+1].text, "NTSC Filter: MANUAL");
    sprintf(items[VI_OFFSET+2].text, "NTSC Sharpness: %1.2f", config.ntsc_sharpness);
    sprintf(items[VI_OFFSET+3].text, "NTSC Resolution: %1.2f", config.ntsc_resolution);
    sprintf(items[VI_OFFSET+4].text, "NTSC Artifacts: %1.2f", config.ntsc_artifacts);
    sprintf(items[VI_OFFSET+5].text, "NTSC Color Bleed: %1.2f", config.ntsc_bleed);
    sprintf(items[VI_OFFSET+6].text, "NTSC Color Fringing: %1.2f", config.ntsc_fringing);
    strcpy(items[VI_OFFSET+2].comment, "Adjust edge contrast enhancement/blurring");
    strcpy(items[VI_OFFSET+3].comment, "Adjust image resolution");
    strcpy(items[VI_OFFSET+4].comment, "Adjust artifacts caused by color changes");
    strcpy(items[VI_OFFSET+5].comment, "Adjust color resolution reduction");
    strcpy(items[VI_OFFSET+6].comment, "Adjust artifacts caused by brightness changes");
    ntsc_offset = 5;
  }
  else
    sprintf (items[VI_OFFSET+1].text, "NTSC Filter: OFF");

  strcpy(items[VI_OFFSET+2+ntsc_offset].comment, "Enable/Disable overscan emulation");
  strcpy(items[VI_OFFSET+3+ntsc_offset].comment, "Enable/Disable Game Gear extended screen");
  strcpy(items[VI_OFFSET+4+ntsc_offset].comment, "Select display aspect ratio");
  strcpy(items[VI_OFFSET+5+ntsc_offset].comment, "Adjust display position");
  strcpy(items[VI_OFFSET+6+ntsc_offset].comment, "Adjust display scaling");

  if (config.overscan == 3)
    sprintf (items[VI_OFFSET+2+ntsc_offset].text, "Borders: FULL");
  else if (config.overscan == 2)
    sprintf (items[VI_OFFSET+2+ntsc_offset].text, "Borders: H ONLY");
  else if (config.overscan == 1)
    sprintf (items[VI_OFFSET+2+ntsc_offset].text, "Borders: V ONLY");
  else
    sprintf (items[VI_OFFSET+2+ntsc_offset].text, "Borders: NONE");

  sprintf(items[VI_OFFSET+3+ntsc_offset].text, "GG Screen: %s", config.gg_extra ? "EXTENDED":"ORIGINAL");

  if (config.aspect == 1)
    sprintf (items[VI_OFFSET+4+ntsc_offset].text,"Aspect: ORIGINAL (4:3)");
  else if (config.aspect == 2)
    sprintf (items[VI_OFFSET+4+ntsc_offset].text, "Aspect: ORIGINAL (16:9)");
  else
  {
    sprintf (items[VI_OFFSET+4+ntsc_offset].text, "Aspect: SCALED");
  }

  sprintf (items[VI_OFFSET+5+ntsc_offset].text, "Screen Position: (%s%02d,%s%02d)",
    (config.xshift < 0) ? "":"+", config.xshift,
    (config.yshift < 0) ? "":"+", config.yshift);

  sprintf (items[VI_OFFSET+6+ntsc_offset].text, "Screen Scaling: (%s%02d,%s%02d)",
    (config.xscale < 0) ? "":"+", config.xscale,
    (config.yscale < 0) ? "":"+", config.yscale);

  m->max_items  = VI_OFFSET + 6 + ntsc_offset + (config.aspect == 0);

  GUI_InitMenu(m);
  GUI_SlideMenuTitle(m,strlen("Video "));

  while (quit == 0)
  {
    ret = GUI_RunMenu(m);

    if (ret > (VI_OFFSET + 1))
    {
      ret += (5 - ntsc_offset);
    }

    switch (ret)
    {
      case 0:  /*** rendering ***/
      {
        config.render = (config.render + 1) % 3;
        if (config.render == 2)
        {
          /* progressive mode is only possible through component cable */
          if (!VIDEO_HaveComponentCable())
          {
            config.render = 0;
          }
        }
        if (config.render == 1)
          sprintf (items[0].text,"Display: INTERLACED");
        else if (config.render == 2)
          sprintf (items[0].text, "Display: PROGRESSIVE");
        else
          sprintf (items[0].text, "Display: ORIGINAL");

        reinit = 1;
        break;
      }

      case 1: /*** tv mode ***/
      {
        config.tv_mode = (config.tv_mode + 1) % 3;
        if (config.tv_mode == 0)
          sprintf (items[1].text, "TV Mode: 60HZ");
        else if (config.tv_mode == 1)
          sprintf (items[1].text, "TV Mode: 50HZ");
        else
          sprintf (items[1].text, "TV Mode: 50/60HZ");
        reinit = 1;
        break;
      }

      case 2: /*** VSYNC ***/
      {
        config.vsync ^= 1;
        if (config.vsync)
          sprintf (items[2].text, "VSYNC: AUTO");
        else
          sprintf (items[2].text, "VSYNC: OFF");
        reinit = 1;
        break;
      }

      case 3: /*** GX Texture filtering ***/
      {
        config.bilinear ^= 1;
        sprintf (items[3].text, "Bilinear Filter: %s", config.bilinear ? " ON" : "OFF");
        break;
      }

      case 4: /*** GX Copy filtering (deflickering filter) ***/
      {
        config.vfilter ^= 1;
        sprintf (items[4].text, "Deflickering Filter: %s", config.vfilter ? "AUTO" : "OFF");
        break;
      }

#ifdef HW_RVL
      case 5: /*** VIDEO Trap filtering ***/
      {
        config.trap ^= 1;
        sprintf (items[5].text, "Trap Filter: %s", config.trap ? " ON" : "OFF");
        break;
      }

      case 6: /*** VIDEO Gamma correction ***/
      {
        if (system_hw) 
        {
          update_gamma();
          state[0] = m->arrows[0]->state;
          state[1] = m->arrows[1]->state;
          m->max_buttons = 0;
          m->max_images = 0;
          m->arrows[0]->state = 0;
          m->arrows[1]->state = 0;
          m->screenshot = 255;
          strcpy(m->title,"");
          GUI_OptionBox(m,update_gamma,"Gamma Correction",(void *)&config.gamma,0.1,0.1,3.0,0);
          m->max_buttons = 4;
          m->max_images = 6;
          m->arrows[0]->state = state[0];
          m->arrows[1]->state = state[1];
          m->screenshot = 0;
          strcpy(m->title,"Video Settings");
          sprintf (items[6].text, "Gamma Correction: %1.1f", config.gamma);
          VIDEO_SetGamma(VI_GM_1_0);
          VIDEO_Flush();
        }
        else
        {
          GUI_WaitPrompt("Error","Please load a game first !\n");
        }
        break;
      }
#endif

      case VI_OFFSET: /*** LCD filter (soft) ***/
      {
        if (config.ntsc)
        {
          GUI_WaitPrompt("Error","NTSC filter must be disabled\n");
          break;
        }

        int16 ratio = ((config.lcd * 100) + 254) / 255;
        GUI_OptionBox(m,0,"LCD Ghosting Ratio (%)",(void *)&ratio,1,0,100,1);
        if (ratio > 0)
        {
          sprintf (items[VI_OFFSET].text, "LCD Ghosting Filter: %d%%", ratio);
        }
        else
        {
          sprintf (items[VI_OFFSET].text, "LCD Ghosting Filter: OFF");
        }
        config.lcd = (ratio * 255) / 100;
        break;
      }

      case VI_OFFSET+1: /*** NTSC filter (soft) ***/
      {
        if (config.lcd)
        {
          GUI_WaitPrompt("Error","LCD filter must be disabled\n");
          break;
        }

        config.ntsc = (config.ntsc + 1) % 5;
        if (config.ntsc == 1)
        {
          sprintf (items[VI_OFFSET+1].text, "NTSC Filter: COMPOSITE");
        }
        else if (config.ntsc == 2)
        {
          sprintf (items[VI_OFFSET+1].text, "NTSC Filter: S-VIDEO");
        }
        else if (config.ntsc == 3)
        {
          sprintf (items[VI_OFFSET+1].text, "NTSC Filter: RGB");
        }
        else if (config.ntsc == 4)
        {
          sprintf (items[VI_OFFSET+1].text, "NTSC Filter: MANUAL");
          strcpy(items[VI_OFFSET+5+2].text, items[VI_OFFSET+2].text);
          strcpy(items[VI_OFFSET+5+3].text, items[VI_OFFSET+3].text);
          strcpy(items[VI_OFFSET+5+4].text, items[VI_OFFSET+4].text);
          strcpy(items[VI_OFFSET+5+5].text, items[VI_OFFSET+5].text);
          strcpy(items[VI_OFFSET+5+6].text, items[VI_OFFSET+6].text);
          strcpy(items[VI_OFFSET+5+2].comment, items[VI_OFFSET+2].comment);
          strcpy(items[VI_OFFSET+5+3].comment, items[VI_OFFSET+3].comment);
          strcpy(items[VI_OFFSET+5+4].comment, items[VI_OFFSET+4].comment);
          strcpy(items[VI_OFFSET+5+5].comment, items[VI_OFFSET+5].comment);
          strcpy(items[VI_OFFSET+5+6].comment, items[VI_OFFSET+6].comment);
          sprintf(items[VI_OFFSET+2].text, "NTSC Sharpness: %1.1f", config.ntsc_sharpness);
          sprintf(items[VI_OFFSET+3].text, "NTSC Resolution: %1.1f", config.ntsc_resolution);
          sprintf(items[VI_OFFSET+4].text, "NTSC Artifacts: %1.1f", config.ntsc_artifacts);
          sprintf(items[VI_OFFSET+5].text, "NTSC Color Bleed: %1.1f", config.ntsc_bleed);
          sprintf(items[VI_OFFSET+6].text, "NTSC Color Fringing: %1.1f", config.ntsc_fringing);
          strcpy(items[VI_OFFSET+2].comment, "Adjust edge contrast enhancement/blurring");
          strcpy(items[VI_OFFSET+3].comment, "Adjust image resolution");
          strcpy(items[VI_OFFSET+4].comment, "Adjust artifacts caused by color changes");
          strcpy(items[VI_OFFSET+5].comment, "Adjust color resolution reduction");
          strcpy(items[VI_OFFSET+6].comment, "Adjust artifacts caused by brightness changes");
          ntsc_offset = 5;
          m->max_items  = VI_OFFSET + 11 + (config.aspect == 0);
        }
        else
        {
          sprintf (items[VI_OFFSET+1].text, "NTSC Filter: OFF");
          strcpy(items[VI_OFFSET+2].text, items[VI_OFFSET+5+2].text);
          strcpy(items[VI_OFFSET+3].text, items[VI_OFFSET+5+3].text);
          strcpy(items[VI_OFFSET+4].text, items[VI_OFFSET+5+4].text);
          strcpy(items[VI_OFFSET+5].text, items[VI_OFFSET+5+5].text);
          strcpy(items[VI_OFFSET+6].text, items[VI_OFFSET+5+6].text);
          strcpy(items[VI_OFFSET+2].comment, items[VI_OFFSET+5+2].comment);
          strcpy(items[VI_OFFSET+3].comment, items[VI_OFFSET+5+3].comment);
          strcpy(items[VI_OFFSET+4].comment, items[VI_OFFSET+5+4].comment);
          strcpy(items[VI_OFFSET+5].comment, items[VI_OFFSET+5+5].comment);
          strcpy(items[VI_OFFSET+6].comment, items[VI_OFFSET+5+6].comment);
          ntsc_offset = 0;
          m->max_items  = VI_OFFSET + 6 + (config.aspect == 0);
        }
        break;
      }

      case VI_OFFSET+2: /*** NTSC Sharpness ***/
        GUI_OptionBox(m,0,"NTSC Sharpness",(void *)&config.ntsc_sharpness,0.01,-1.0,1.0,0);
        sprintf(items[VI_OFFSET+2].text, "NTSC Sharpness: %1.2f", config.ntsc_sharpness);
        break;

      case VI_OFFSET+3: /*** NTSC Resolution ***/
        GUI_OptionBox(m,0,"NTSC Resolution",(void *)&config.ntsc_resolution,0.01,0.0,1.0,0);
        sprintf(items[VI_OFFSET+3].text, "NTSC Resolution: %1.2f", config.ntsc_resolution);
        break;

      case VI_OFFSET+4: /*** NTSC Artifacts ***/
        GUI_OptionBox(m,0,"NTSC Artifacts",(void *)&config.ntsc_artifacts,0.01,-1.0,0.0,0);
        sprintf(items[VI_OFFSET+4].text, "NTSC Artifacts: %1.2f", config.ntsc_artifacts);
        break;

      case VI_OFFSET+5: /*** NTSC Color Bleed ***/
        GUI_OptionBox(m,0,"NTSC Color Bleed",(void *)&config.ntsc_bleed,0.01,-1.0,1.0,0);
        sprintf(items[VI_OFFSET+5].text, "NTSC Color Bleed: %1.2f", config.ntsc_bleed);
        break;

      case VI_OFFSET+6: /*** NTSC Color Fringing ***/
        GUI_OptionBox(m,0,"NTSC Color Fringing",(void *)&config.ntsc_fringing,0.01,-1.0,1.0,0);
        sprintf(items[VI_OFFSET+6].text, "NTSC Color Fringing: %1.2f", config.ntsc_fringing);
        break;

      case VI_OFFSET+7: /*** overscan emulation ***/
      {
        config.overscan = (config.overscan + 1) & 3;
        if (config.overscan == 3)
          sprintf (items[VI_OFFSET+ntsc_offset+2].text, "Borders: FULL");
        else if (config.overscan == 2)
          sprintf (items[VI_OFFSET+ntsc_offset+2].text, "Borders: H ONLY");
        else if (config.overscan == 1)
          sprintf (items[VI_OFFSET+ntsc_offset+2].text, "Borders: V ONLY");
        else
          sprintf (items[VI_OFFSET+ntsc_offset+2].text, "Borders: NONE");
        break;
      }

      case VI_OFFSET+8: /*** Game Gear extended screen */
      {
        config.gg_extra ^= 1;
        sprintf(items[VI_OFFSET+ntsc_offset+3].text, "GG Screen: %s", config.gg_extra ? "EXTENDED":"ORIGINAL");
        break;
      }

      case VI_OFFSET+9: /*** aspect ratio ***/
      {
        config.aspect = (config.aspect + 1) % 3;
        if (config.aspect == 1)
          sprintf (items[VI_OFFSET+ntsc_offset+4].text,"Aspect: ORIGINAL (4:3)");
        else if (config.aspect == 2)
          sprintf (items[VI_OFFSET+ntsc_offset+4].text, "Aspect: ORIGINAL (16:9)");
        else
          sprintf (items[VI_OFFSET+ntsc_offset+4].text, "Aspect: SCALED");

        if (config.aspect)
        {
          /* disable items */
          m->max_items  = VI_OFFSET + ntsc_offset + 6;

          /* reset menu selection */
          if (m->selected < 2)
          {
            m->offset  = VI_OFFSET + ntsc_offset + 2;
            m->selected = 2;
          }
        }
        else
        {
          /* enable items */
          m->max_items  = VI_OFFSET + ntsc_offset + 7;
        }
        break;
      }

      case VI_OFFSET+10: /*** screen position ***/
      {
        if (system_hw) 
        {
          state[0] = m->arrows[0]->state;
          state[1] = m->arrows[1]->state;
          m->max_buttons = 0;
          m->max_images = 0;
          m->arrows[0]->state = 0;
          m->arrows[1]->state = 0;
          m->screenshot = 255;
          strcpy(m->title,"");
          GUI_OptionBox2(m,"X Offset","Y Offset",&config.xshift,&config.yshift,1,-99,99);
          m->max_buttons = 4;
          m->max_images = 6;
          m->arrows[0]->state = state[0];
          m->arrows[1]->state = state[1];
          m->screenshot = 0;
          strcpy(m->title,"Video Settings");
          sprintf (items[VI_OFFSET+ntsc_offset+5].text, "Screen Position: (%s%02d,%s%02d)",
                                            (config.xshift < 0) ? "":"+", config.xshift,
                                            (config.yshift < 0) ? "":"+", config.yshift);
        }
        else
        {
          GUI_WaitPrompt("Error","Please load a game first !\n");
        }
        break;
      }

      case VI_OFFSET+11: /*** screen scaling ***/
      {
        if (system_hw) 
        {
          state[0] = m->arrows[0]->state;
          state[1] = m->arrows[1]->state;
          m->max_buttons = 0;
          m->max_images = 0;
          m->arrows[0]->state = 0;
          m->arrows[1]->state = 0;
          m->screenshot = 255;
          strcpy(m->title,"");
          GUI_OptionBox2(m,"X Scale","Y Scale",&config.xscale,&config.yscale,1,-99,99);
          m->max_buttons = 4;
          m->max_images = 6;
          m->arrows[0]->state = state[0];
          m->arrows[1]->state = state[1];
          m->screenshot = 0;
          strcpy(m->title,"Video Settings");
          sprintf (items[VI_OFFSET+ntsc_offset+6].text, "Screen Scaling: (%s%02d,%s%02d)",
                                            (config.xscale < 0) ? "":"+", config.xscale,
                                            (config.yscale < 0) ? "":"+", config.yscale);
        }
        else
        {
          GUI_WaitPrompt("Error","Please load a game first !\n");
        }
        break;
      }

      case -1:
      {
        quit = 1;
        break;
      }
    }
  }

  if (reinit && system_hw)
  {
    /* framerate might have changed, reinitialize audio timings */
    audio_init(snd.sample_rate, get_framerate());
  }

  GUI_DeleteMenu(m);
}

/****************************************************************************
 * Controllers Settings menu
 ****************************************************************************/
static int player = 0;
static void ctrlmenu_cb(void)
{
  int i, cnt = 1;
  char msg[16];
  gui_menu *m = &menu_ctrls;

  if (m->bg_images[7].state & IMAGE_VISIBLE)
  {
    /* draw device port number */
    if (config.input[player].device != -1)
    {
      sprintf(msg,"%d",config.input[player].port + 1);
      if (m->selected == 11)
        FONT_write(msg,16,m->items[11].x+m->items[11].w+2,m->items[11].y+m->items[11].h+2,640,(GXColor)DARK_GREY);
      else
        FONT_write(msg,14,m->items[11].x+m->items[11].w,m->items[11].y+m->items[11].h,640,(GXColor)DARK_GREY);
    }
  }

  /* draw players index */
  for (i=2; i<MAX_DEVICES+2; i++)
  {
    if (m->selected == i)
    {
      FONT_writeCenter("Player", 16, m->buttons[i].x + 2, m->buttons[i].x + 54, m->buttons[i].y + (m->buttons[i].h - 16)/2 + 16, (GXColor)DARK_GREY);
    }
    else
    {
      FONT_writeCenter("Player", 14, m->buttons[i].x + 4, m->buttons[i].x + 54, m->buttons[i].y + (m->buttons[i].h - 14)/2 + 14, (GXColor)DARK_GREY);
    }
    
    if (input.dev[i-2] != NO_DEVICE)
    {
      sprintf(msg,"%d",cnt++);
      if (m->selected == i)
      {
        FONT_writeCenter(msg,18,m->items[i].x+2,m->items[i].x+m->items[i].w+2,m->buttons[i].y+(m->buttons[i].h-18)/2+18,(GXColor)DARK_GREY);
      }
      else
      {
        FONT_writeCenter(msg,16,m->items[i].x,m->items[i].x+m->items[i].w,m->buttons[i].y+(m->buttons[i].h - 16)/2+16,(GXColor)DARK_GREY);
      }
    }
  }
}

/* Set menu elements depending on current system configuration */
static void ctrlmenu_raz(void)
{
  int i,max = 0;
  gui_menu *m = &menu_ctrls;

  /* update players buttons */
  for (i=0; i<MAX_DEVICES; i++)
  {
    if (input.dev[i] == NO_DEVICE)
    {
      m->buttons[i+2].data  = &button_player_none_data;
      m->buttons[i+2].state &= ~BUTTON_ACTIVE;
      strcpy(m->items[i+2].comment,"");
    }
    else
    {
      m->buttons[i+2].data  = &button_player_data;
      m->buttons[i+2].state |= BUTTON_ACTIVE;
      if ((cart.special & HW_J_CART) && (i > 4))
        sprintf(m->items[i+2].comment,"Configure Player %d (J-CART) settings", max + 1);
      else
        sprintf(m->items[i+2].comment,"Configure Player %d settings", max + 1);
      max++;
    }
  }

  /* update buttons navigation */
  if (input.dev[0] != NO_DEVICE)
    m->buttons[0].shift[3] = 2;
  else if (input.dev[4] != NO_DEVICE)
    m->buttons[0].shift[3] = 6;
  else if (input.dev[5] != NO_DEVICE)
    m->buttons[0].shift[3] = 7;
  else
    m->buttons[0].shift[3] = 0;
  if (input.dev[4] != NO_DEVICE)
    m->buttons[1].shift[3] = 5;
  else if (input.dev[5] != NO_DEVICE)
    m->buttons[1].shift[3] = 6;
  else if (input.dev[0] != NO_DEVICE)
    m->buttons[1].shift[3] = 1;
  else
    m->buttons[1].shift[3] = 0;

  if (input.dev[1] != NO_DEVICE)
    m->buttons[2].shift[1] = 1;
  else if (input.dev[4] != NO_DEVICE)
    m->buttons[2].shift[1] = 4;
  else if (input.dev[5] != NO_DEVICE)
    m->buttons[2].shift[1] = 5;
  else
    m->buttons[2].shift[1] = 0;

  if (input.dev[4] != NO_DEVICE)
    m->buttons[5].shift[1] = 1;
  else if (input.dev[5] != NO_DEVICE)
    m->buttons[5].shift[1] = 2;
  else
    m->buttons[5].shift[1] = 0;

  if (input.dev[3] != NO_DEVICE)
    m->buttons[6].shift[0] = 1;
  else if (input.dev[0] != NO_DEVICE)
    m->buttons[6].shift[0] = 4;
  else
    m->buttons[6].shift[0] = 0;

  if (input.dev[5] != NO_DEVICE)
    m->buttons[6].shift[1] = 1;
  else
    m->buttons[6].shift[1] = 0;

  if (input.dev[6] != NO_DEVICE)
    m->buttons[7].shift[1] = 1;
  else
    m->buttons[7].shift[1] = 0;

  if (input.dev[7] != NO_DEVICE)
    m->buttons[8].shift[1] = 1;
  else
    m->buttons[8].shift[1] = 0;

  if (input.dev[4] != NO_DEVICE)
    m->buttons[7].shift[0] = 1;
  else if (input.dev[3] != NO_DEVICE)
    m->buttons[7].shift[0] = 2;
  else if (input.dev[0] != NO_DEVICE)
    m->buttons[7].shift[0] = 5;
  else
    m->buttons[7].shift[0] = 0;
}

static void ctrlmenu(void)
{
  int old_player = -1;
  int i = 0;
  int update = 0;
  gui_item *items = NULL;
  u8 *special = NULL;
  u32 exp, index = 0;

  /* System devices */
  gui_item items_sys[2][14] =
  {
    {
      {NULL,Ctrl_none_png           ,"","Select Port 1 device",110,130,48,72},
      {NULL,Ctrl_gamepad_png        ,"","Select Port 1 device",100,109,68,92},
      {NULL,Ctrl_mouse_png          ,"","Select Port 1 device", 97,113,64,88},
      {NULL,Ctrl_menacer_png        ,"","Select Port 1 device", 94,113,80,88},
      {NULL,Ctrl_justifiers_png     ,"","Select Port 1 device", 88,117,80,84},
      {NULL,Ctrl_xe_1ap_png         ,"","Select Port 1 device", 98,118,72,84},
      {NULL,Ctrl_activator_png      ,"","Select Port 1 device", 94,121,72,80},
      {NULL,Ctrl_lightphaser_png    ,"","Select Port 1 device", 89,109,88,92},
      {NULL,Ctrl_paddle_png         ,"","Select Port 1 device", 86,117,96,84},
      {NULL,Ctrl_sportspad_png      ,"","Select Port 1 device", 95,117,76,84},
      {NULL,Ctrl_graphic_board_png  ,"","Select Port 1 device", 90,105,88,96},
      {NULL,Ctrl_mastertap_png      ,"","Select Port 1 device", 96,104,76,96},
      {NULL,Ctrl_teamplayer_png     ,"","Select Port 1 device", 94,109,80,92},
      {NULL,Ctrl_4wayplay_png       ,"","Select Port 1 device", 98,110,72,92}
    },
    {
      {NULL,Ctrl_none_png           ,"","Select Port 2 device",110,300,48,72},
      {NULL,Ctrl_gamepad_png        ,"","Select Port 2 device",100,279,68,92},
      {NULL,Ctrl_mouse_png          ,"","Select Port 2 device", 97,283,64,88},
      {NULL,Ctrl_menacer_png        ,"","Select Port 2 device", 94,283,80,88},
      {NULL,Ctrl_justifiers_png     ,"","Select Port 2 device", 88,287,80,84},
      {NULL,Ctrl_xe_1ap_png         ,"","Select Port 2 device", 98,288,72,84},
      {NULL,Ctrl_activator_png      ,"","Select Port 2 device", 94,291,72,80},
      {NULL,Ctrl_lightphaser_png    ,"","Select Port 2 device", 89,279,88,92},
      {NULL,Ctrl_paddle_png         ,"","Select Port 2 device", 86,287,96,84},
      {NULL,Ctrl_sportspad_png      ,"","Select Port 2 device", 95,287,76,84},
      {NULL,Ctrl_graphic_board_png  ,"","Select Port 2 device", 90,275,88,96},
      {NULL,Ctrl_mastertap_png      ,"","Select Port 1 device", 96,274,76,96},
      {NULL,Ctrl_teamplayer_png     ,"","Select Port 2 device", 94,279,80,92},
      {NULL,Ctrl_4wayplay_png       ,"","Select Port 2 device", 98,280,72,92}
    }
  };    

  /* Specific controller options */
  gui_item items_special[4][4] =
  {
    {
      /* Gamepad option */
      {NULL,Ctrl_pad3b_png,"Pad\nType","Use 3-buttons Pad",528,180,44,28},
      {NULL,Ctrl_pad6b_png,"Pad\nType","Use 6-buttons Pad",528,180,44,28},
      {NULL,Ctrl_pad2b_png,"Pad\nType","Use 2-buttons Pad",528,180,44,28},
      {NULL,Ctrl_pad_auto_png,"Pad\nType","Auto-detect Pad type",528,180,44,28},
    },
    {
      /* Mouse option */
      {NULL,ctrl_option_off_png,"Invert\nMouse","Enable/Disable Y-Axis inversion",534,180,24,24},
      {NULL,ctrl_option_on_png ,"Invert\nMouse","Enable/Disable Y-Axis inversion",534,180,24,24},
      {NULL,NULL,"","",0,0,0,0},
      {NULL,NULL,"","",0,0,0,0},
    },
    {
      /* Gun option */
      {NULL,ctrl_option_off_png,"Show\nCursor","Enable/Disable Lightgun cursor",534,180,24,24},
      {NULL,ctrl_option_on_png ,"Show\nCursor","Enable/Disable Lightgun cursor",534,180,24,24},
      {NULL,NULL,"","",0,0,0,0},
      {NULL,NULL,"","",0,0,0,0},
    },
    {
      /* no option */
      {NULL,NULL,"No Option","",436,180,160,52},
      {NULL,NULL,"","",0,0,0,0},
      {NULL,NULL,"","",0,0,0,0},
      {NULL,NULL,"","",0,0,0,0},
    }
  };

  /* Player Configuration device items */
#ifdef HW_RVL
  gui_item items_device[7] =
  {
    {NULL,ctrl_option_off_png ,"Input\nDevice","Select Input Controller",534,244,24,24},
    {NULL,ctrl_gamecube_png   ,"Input\nDevice","Select Input Controller",530,246,36,24},
    {NULL,ctrl_wiimote_png    ,"Input\nDevice","Select Input Controller",526,250,40,12},
    {NULL,ctrl_nunchuk_png    ,"Input\nDevice","Select Input Controller",532,242,32,32},
    {NULL,ctrl_classic_png    ,"Input\nDevice","Select Input Controller",526,242,40,32},
    {NULL,ctrl_wiiu_png       ,"Input\nDevice","Select Input Controller",526,246,40,24},
    {NULL,ctrl_retrousb_png   ,"Input\nDevice","Select Input Controller",526,246,40,24},
  };
#else
  gui_item items_device[2] =
  {
    {NULL,ctrl_option_off_png ,"Input\nDevice","Select Input Controller",534,244,24,24},
    {NULL,ctrl_gamecube_png   ,"Input\nDevice","Select Input Controller",530,246,36,24}
  };
#endif

  /* initialize menu */
  gui_menu *m = &menu_ctrls;
  GUI_InitMenu(m);

  /* initialize custom buttons */
  button_player_data.texture[0]      = gxTextureOpenPNG(button_player_data.image[0],0);
  button_player_data.texture[1]      = gxTextureOpenPNG(button_player_data.image[1],0);
  button_player_none_data.texture[0] = gxTextureOpenPNG(button_player_none_data.image[0],0);

  /* initialize custom images */
  for (i=0; i<14; i++)
  {
    items_sys[1][i].texture = items_sys[0][i].texture = gxTextureOpenPNG(items_sys[0][i].data,0);
  }
  items_special[0][0].texture = gxTextureOpenPNG(items_special[0][0].data,0);
  items_special[0][1].texture = gxTextureOpenPNG(items_special[0][1].data,0);
  items_special[0][2].texture = gxTextureOpenPNG(items_special[0][2].data,0);
  items_special[0][3].texture = gxTextureOpenPNG(items_special[0][3].data,0);
  items_special[2][0].texture = items_special[1][0].texture = gxTextureOpenPNG(items_special[1][0].data,0);
  items_special[2][1].texture = items_special[1][1].texture = gxTextureOpenPNG(items_special[1][1].data,0);
  items_device[0].texture = items_special[1][0].texture;
  items_device[1].texture = gxTextureOpenPNG(items_device[1].data,0);
#ifdef HW_RVL
  items_device[2].texture = gxTextureOpenPNG(items_device[2].data,0);
  items_device[3].texture = gxTextureOpenPNG(items_device[3].data,0);
  items_device[4].texture = gxTextureOpenPNG(items_device[4].data,0);
  if (WiiDRC_Inited())
  {
    items_device[5].texture = gxTextureOpenPNG(items_device[5].data,0);
  }
  items_device[6].texture = gxTextureOpenPNG(items_device[6].data, 0);
#endif

  /* restore current menu elements */
  player = 0;
  ctrlmenu_raz();
  memcpy(&m->items[0],&items_sys[0][input.system[0]],sizeof(gui_item));
  memcpy(&m->items[1],&items_sys[1][input.system[1]],sizeof(gui_item));

  /* menu title slide effect */
  m->selected = 0;
  GUI_SlideMenuTitle(m,strlen("Controller "));

  while (update != -1)
  {
    /* draw menu */
    GUI_DrawMenu(m);

    /* update menu */
    update = GUI_UpdateMenu(m);

    if (update > 0)
    {
      switch (m->selected)
      {
        case 0:   /* update port 1 system */
        {
          /* fixed configurations */
          if (system_hw)
          {
            if (cart.special & HW_TEREBI_OEKAKI)
            {
              GUI_WaitPrompt("Error","Terebi Oekaki detected !");
              break;
            }
            else if (system_hw == SYSTEM_PICO)
            {
              GUI_WaitPrompt("Error","PICO hardware detected !");
              break;
            }
          }

          /* next connected device */
          input.system[0]++;

          /* allow only one connected mouse */
          if ((input.system[0] == SYSTEM_MOUSE) && (input.system[1] == SYSTEM_MOUSE))
          {
            input.system[0] += 3;
          }

          /* Menacer & Justifiers on Port B only */
          if (input.system[0] == SYSTEM_MENACER)
          {
            input.system[0] += 2; 
          }
 
          /* allow only one gun type */
          if ((input.system[0] == SYSTEM_LIGHTPHASER) && ((input.system[1] == SYSTEM_MENACER) || (input.system[1] == SYSTEM_JUSTIFIER)))
          {
            input.system[0]++;
          }

          /* allow only one connected graphic board */
          if ((input.system[0] == SYSTEM_GRAPHIC_BOARD) && (input.system[1] == SYSTEM_GRAPHIC_BOARD))
          {
            input.system[0]++;
          }

          /* 4-wayplay uses both ports */
          if (input.system[0] == SYSTEM_WAYPLAY)
          {
            input.system[1] = SYSTEM_WAYPLAY;
          }

          /* loop back */
          if (input.system[0] > SYSTEM_WAYPLAY)
          {
            input.system[0] = NO_SYSTEM;
            input.system[1] = SYSTEM_GAMEPAD;
          }

          /* reset I/O ports */
          io_init();
          input_reset();

          /* save current configuration */
          old_system[0] = input.system[0];
          old_system[1] = input.system[1];

          /* update menu elements */
          ctrlmenu_raz();
          memcpy(&m->items[0],&items_sys[0][input.system[0]],sizeof(gui_item));
          memcpy(&m->items[1],&items_sys[1][input.system[1]],sizeof(gui_item));

          if (m->bg_images[7].state & IMAGE_VISIBLE)
          {
            /* slide out configuration window */
            GUI_DrawMenuFX(m, 20, 1);

            /* remove configuration window */
            m->bg_images[7].state &= ~IMAGE_VISIBLE;

            /* disable configuration buttons */
            m->buttons[10].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
            m->buttons[11].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
            m->buttons[12].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);

            /* update directions */
            m->buttons[2].shift[3] = 0;
            m->buttons[3].shift[3] = 0;
            m->buttons[4].shift[3] = 0;
            m->buttons[5].shift[3] = 0;
            m->buttons[6].shift[3] = 0;
            m->buttons[7].shift[3] = 0;
            m->buttons[8].shift[3] = 0;
            m->buttons[9].shift[3] = 0;

            /* update title */
            sprintf(m->title,"Controller Settings");
          }
          break;
        }

        case 1:   /* update port 2 system */
        {
          /* fixed configurations */
          if (system_hw)
          {
            if (cart.special & HW_TEREBI_OEKAKI)
            {
              GUI_WaitPrompt("Error","Terebi Oekaki detected !");
              break;
            }
            else if (system_hw == SYSTEM_PICO)
            {
              GUI_WaitPrompt("Error","PICO hardware detected !");
              break;
            }
          }

          /* next connected device */
          input.system[1]++;

          /* allow only one connected mouse */
          if ((input.system[0] == SYSTEM_MOUSE) && (input.system[1] == SYSTEM_MOUSE))
          {
            input.system[1]++;
          }

          /* allow only one gun type */
          if ((input.system[0] == SYSTEM_LIGHTPHASER) && (input.system[1] == SYSTEM_MENACER))
          {
            input.system[1]++;
          }

          /* allow only one gun type */
          if ((input.system[0] == SYSTEM_LIGHTPHASER) && (input.system[1] == SYSTEM_JUSTIFIER))
          {
            input.system[1]++;
          }

          /* allow only one connected graphic board */
          if ((input.system[0] == SYSTEM_GRAPHIC_BOARD) && (input.system[1] == SYSTEM_GRAPHIC_BOARD))
          {
            input.system[1]++;
          }

          /* 4-wayplay uses both ports */
          if (input.system[1] == SYSTEM_WAYPLAY)
          {
            input.system[0] = SYSTEM_WAYPLAY;
          }

          /* loop back */
          if (input.system[1] > SYSTEM_WAYPLAY)
          {
            input.system[1] = NO_SYSTEM;
            input.system[0] = SYSTEM_GAMEPAD;
          }

          /* reset I/O ports */
          io_init();
          input_reset();

          /* save current configuration */
          old_system[0] = input.system[0];
          old_system[1] = input.system[1];

          /* update menu elements */
          ctrlmenu_raz();
          memcpy(&m->items[0],&items_sys[0][input.system[0]],sizeof(gui_item));
          memcpy(&m->items[1],&items_sys[1][input.system[1]],sizeof(gui_item));

          if (m->bg_images[7].state & IMAGE_VISIBLE)
          {
            /* slide out configuration window */
            GUI_DrawMenuFX(m, 20, 1);

            /* remove configuration window */
            m->bg_images[7].state &= ~IMAGE_VISIBLE;

            /* disable configuration buttons */
            m->buttons[10].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
            m->buttons[11].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
            m->buttons[12].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);

            /* update directions */
            m->buttons[2].shift[3] = 0;
            m->buttons[3].shift[3] = 0;
            m->buttons[4].shift[3] = 0;
            m->buttons[5].shift[3] = 0;
            m->buttons[6].shift[3] = 0;
            m->buttons[7].shift[3] = 0;
            m->buttons[8].shift[3] = 0;
            m->buttons[9].shift[3] = 0;

            /* update title */
            sprintf(m->title,"Controller Settings");
          }

          break;
        }

        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        {
          /* remove duplicate assigned inputs */
          for (i=0; i<MAX_INPUTS; i++)
          {
            if ((i!=player) && (config.input[i].port == config.input[player].port) &&
                ((config.input[i].device == config.input[player].device) || ((config.input[i].device * config.input[player].device) == 2)))
            {
              config.input[i].device = -1;
              config.input[i].port = i%4;
            }
          }

          /* save device index */
          index = m->selected - 2;

          /* update player index */
          old_player = player;
          player = 0;
          for (i=0; i<index; i++)
          {
            if (input.dev[i] != NO_DEVICE) player ++;
          }

          if (m->bg_images[7].state & IMAGE_VISIBLE)
          {
            /* if already displayed, do nothing */
            if (old_player == player) break;
            
            /* slide out configuration window */
            GUI_DrawMenuFX(m, 20, 1);
          }
          else
          {
            /* append configuration window */
            m->bg_images[7].state |= IMAGE_VISIBLE;

            /* enable configuration buttons */
            m->buttons[10].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
            m->buttons[11].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
            m->buttons[12].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);

            /* update directions */
            m->buttons[2].shift[3] = 8;
            m->buttons[3].shift[3] = 7;
            m->buttons[4].shift[3] = 6;
            m->buttons[5].shift[3] = 5;
            m->buttons[6].shift[3] = 4;
            m->buttons[7].shift[3] = 3;
            m->buttons[8].shift[3] = 2;
            m->buttons[9].shift[3] = 1;
          }

          /* retrieve current player informations */
          switch (input.dev[index])
          {
            case DEVICE_PAD2B:
            case DEVICE_PAD3B:
            case DEVICE_PAD6B:
            {
              if (input.system[index/4] == SYSTEM_MASTERTAP)
              {
                /* force 2-buttons pad */
                items = items_special[3];
                special = NULL;
              }
              else
              {
                items = items_special[0];
                special = &config.input[player].padtype;
              }
              break;
            }

            case DEVICE_MOUSE:
            {
              items = items_special[1];
              special = &config.invert_mouse;
              break;
            }

            case DEVICE_LIGHTGUN:
            {
              items = items_special[2];
              if ((input.system[1] == SYSTEM_MENACER) || (input.system[1] == SYSTEM_JUSTIFIER))
              {
                /* Menacer & Justifiers affected to entries 4 & 5 */
                special = &config.gun_cursor[index & 1];
              }
              else
              {
                /* Lightphasers affected to entries 0 & 4 */
                special = &config.gun_cursor[index / 4];
              }
              break;
            }

            default:
            {
              items = items_special[3];
              special = NULL;
              break;
            }
          }

          if (special)
          {
            memcpy(&m->items[10],&items[*special],sizeof(gui_item));
          }
          else
          {
            memcpy(&m->items[10],&items[0],sizeof(gui_item));
          }

          memcpy(&m->items[11],&items_device[config.input[player].device + 1],sizeof(gui_item));

          /* slide in configuration window */
          m->buttons[10].shift[2] = 10 - m->selected;
          m->buttons[11].shift[2] = 11 - m->selected;
          m->buttons[12].shift[2] = 12 - m->selected;
          m->selected = 10;
          GUI_DrawMenuFX(m, 20, 0);

          /* some devices require analog sticks */
          if ((input.dev[index] == DEVICE_XE_1AP) && ((config.input[player].device == -1) || (config.input[player].device == 1)))
          {
            GUI_WaitPrompt("Warning","One Analog Stick required !");
          }
          else if ((input.dev[index] == DEVICE_ACTIVATOR) && ((config.input[player].device != 0) && (config.input[player].device != 3)))
          {
            GUI_WaitPrompt("Warning","Two Analog Sticks required !");
          }

          /* update title */
          if ((cart.special & HW_J_CART) && (player > 1))
          {
            sprintf(m->title,"Controller Settings (Player %d) (J-CART)",player+1);
          }
          else
          {
            sprintf(m->title,"Controller Settings (Player %d)",player+1);
          }
          break;
        }

        case 10: /* specific option */
        {
          if (special)
          {
            /* specific case: controller type */
            if (input.dev[index] < 0x03)
            {
              /* switch pad type */
              *special = (*special + 1) & 0x03;

              /* reinitialize emulated device(s) */
              io_init();
              input_reset();
            }
            else
            {
              /* switch option */
              *special ^= 1;
            }

            /* update menu items */
            memcpy(&m->items[10],&items[*special],sizeof(gui_item));
          }
          break;
        }

        case 11:  /* input controller selection */
        {
          /* no input device */
          if (config.input[player].device < 0)
          {
            /* always try gamecube controllers first */
            config.input[player].device = 0;
            config.input[player].port = 0;
          }
          else
          {
            /* try next port */
            config.input[player].port ++;
          }

          /* autodetect connected gamecube controllers */
          if (config.input[player].device == 0)
          {
            /* find first connected controller */
            exp = 0;
            while ((config.input[player].port < 4) && !exp)
            {
              VIDEO_WaitVSync ();
              exp = PAD_ScanPads() & (1<<config.input[player].port);
              if (!exp) config.input[player].port ++;
            }

            /* no more gamecube controller */
            if (config.input[player].port >= 4)
            {
#ifdef HW_RVL
              /* test wiimote */
              config.input[player].port = 0;
              config.input[player].device = 1;
#else
              /* no input controller left */
              config.input[player].device = -1;
              config.input[player].port = player%4;
#endif
            }
          }

#ifdef HW_RVL
          /* autodetect connected wiimotes (without nunchuk) */
          if (config.input[player].device == 1)
          {
            /* find first connected controller */
            exp = 255;
            while ((config.input[player].port < 4) && (exp == 255))
            {
              WPAD_Probe(config.input[player].port,&exp);

              /* check if this is a Wii U Pro Controller */
              if (exp == WPAD_EXP_CLASSIC)
              {
                WPADData *data = WPAD_Data(config.input[player].port);
                if (data->exp.classic.type == 2)
                  exp = 255;
              }

              /* try next port if no wimote available */
              if (exp == 255)
                config.input[player].port ++;
            }

            /* no more wiimote */
            if (config.input[player].port >= 4)
            {
              /* test wiimote+nunchuk */
              config.input[player].port = 0;
              config.input[player].device = 2;
            }
          }

          /* autodetect connected wiimote+nunchuk */
          if (config.input[player].device == 2)
          {
            /* test current port */
            exp = 255;
            if (config.input[player].port < 4)
            {
              WPAD_Probe(config.input[player].port,&exp);
            }

            /* find first connected controller */
            while ((config.input[player].port < 4) && (exp != WPAD_EXP_NUNCHUK))
            {
              /* try next port */
              config.input[player].port ++;
              if (config.input[player].port < 4)
              {
                exp = 255;
                WPAD_Probe(config.input[player].port,&exp);
              }
            }

            /* no more wiimote+nunchuk */
            if (config.input[player].port >= 4)
            {
              /* test classic controllers */
              config.input[player].port = 0;
              config.input[player].device = 3;
            }
          }

          /* autodetect connected classic controllers */
          if (config.input[player].device == 3)
          {
            /* test current port */
            exp = 255;
            if (config.input[player].port < 4)
            {
              WPAD_Probe(config.input[player].port,&exp);
            }

            /* find first connected controller */
            while ((config.input[player].port<4) && (exp != WPAD_EXP_CLASSIC))
            {
              /* try next port */
              config.input[player].port ++;
              if (config.input[player].port < 4)
              {
                exp = 255;
                WPAD_Probe(config.input[player].port,&exp);
              }
            }

            if (config.input[player].port >= 4)
            {
              /* test WiiU gamepad */
              config.input[player].device = 4;
              config.input[player].port = 0;
            }
          }

          /* autodetect WiiU gamepad */
          if (config.input[player].device == 4)
          {
            /* support for only one gamepad */
            if (!WiiDRC_Inited() || !WiiDRC_Connected() || (config.input[player].port >= 1))
            {
              /* test RetroUSB */
              config.input[player].device = 5;
              config.input[player].port = 0;
            }
          }

          if (config.input[player].device == 5)
          {
              if (!RetroUSB_OK() || config.input[player].port >= 1)
              {
                  /* no input controller left */
                  config.input[player].device = -1;
                  config.input[player].port = player % 4;
              }
          }
#endif

          /* update menu items */
          memcpy(&m->items[11],&items_device[config.input[player].device + 1],sizeof(gui_item));

          break;
        }

        case 12:  /* Controller Keys Configuration */
        {
          if (config.input[player].device >= 0)
          {
            GUI_MsgBoxOpen("Keys Configuration", "",0);
            if (!system_hw && special && (*special == 3))
            {
              /* no auto-detected pad type, use 6-buttons key mapping as default */
              gx_input_Config(config.input[player].port, config.input[player].device, DEVICE_PAD6B);
            }
            else
            {
              gx_input_Config(config.input[player].port, config.input[player].device, input.dev[index]);
            }
            GUI_MsgBoxClose();
          }
          break;
        }
      }
    }

    /* Close Window */
    else if (update < 0)
    {
      if (m->bg_images[7].state & IMAGE_VISIBLE)
      {
        /* slide out configuration window */
        GUI_DrawMenuFX(m, 20, 1);

        /* disable configuration window */
        m->bg_images[7].state &= ~IMAGE_VISIBLE;

        /* disable configuration buttons */
        m->buttons[10].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
        m->buttons[11].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
        m->buttons[12].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);

        /* clear directions */
        m->buttons[2].shift[3] = 0;
        m->buttons[3].shift[3] = 0;
        m->buttons[4].shift[3] = 0;
        m->buttons[5].shift[3] = 0;
        m->buttons[6].shift[3] = 0;
        m->buttons[7].shift[3] = 0;
        m->buttons[8].shift[3] = 0;
        m->buttons[9].shift[3] = 0;

        /* update selector */
        m->selected -= m->buttons[m->selected].shift[2];

        /* restore title */
        sprintf(m->title,"Controller Settings");

        /* stay in menu */
        update = 0;
      }
      else
      {
        /* check we have at least one connected input before leaving */
        old_player = player;
        player = 0;
        for (i=0; i<MAX_DEVICES; i++)
        {
          /* check inputs */
          if (input.dev[i] != NO_DEVICE)
          {
            if (config.input[player].device != -1)
              break;
            player++;
          }
        }
        player = old_player;

        /* no input connected */
        if (i == MAX_DEVICES) 
        {
          /* stay in menu */
          GUI_WaitPrompt("Error","No input connected !");
          update = 0;
        }
      }
    }
  }

  /* remove duplicate assigned inputs before leaving */
  for (i=0; i<MAX_INPUTS; i++)
  {
    if ((i!=player) && (config.input[i].port == config.input[player].port) &&
        ((config.input[i].device == config.input[player].device) || ((config.input[i].device * config.input[player].device) == 2)))
    {
      config.input[i].device = -1;
      config.input[i].port = i%4;
    }
  }

  /* disable configuration window */
  m->bg_images[7].state &= ~IMAGE_VISIBLE;

  /* disable configuration buttons */
  m->buttons[10].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
  m->buttons[11].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);
  m->buttons[12].state &= (~BUTTON_VISIBLE & ~BUTTON_ACTIVE);

  /* clear directions */
  m->buttons[2].shift[3] = 0;
  m->buttons[3].shift[3] = 0;
  m->buttons[4].shift[3] = 0;
  m->buttons[5].shift[3] = 0;
  m->buttons[6].shift[3] = 0;
  m->buttons[7].shift[3] = 0;
  m->buttons[8].shift[3] = 0;
  m->buttons[9].shift[3] = 0;

  /* clear menu items */
  memset(&m->items[0],0,sizeof(gui_item));
  memset(&m->items[1],0,sizeof(gui_item));
  memset(&m->items[10],0,sizeof(gui_item));
  memset(&m->items[11],0,sizeof(gui_item));

  /* clear player buttons */
  m->buttons[2].data  = NULL;
  m->buttons[3].data  = NULL;
  m->buttons[4].data  = NULL;
  m->buttons[5].data  = NULL;
  m->buttons[6].data  = NULL;
  m->buttons[7].data  = NULL;
  m->buttons[8].data  = NULL;
  m->buttons[9].data  = NULL;

  /* delete menu */
  GUI_DeleteMenu(m);

  /* delete custom buttons */
  gxTextureClose(&button_player_data.texture[0]);
  gxTextureClose(&button_player_data.texture[1]);
  gxTextureClose(&button_player_none_data.texture[0]);

  /* delete custom images */
  for (i=0; i<14; i++)
  {
    gxTextureClose(&items_sys[0][i].texture);
  }
  gxTextureClose(&items_special[0][0].texture);
  gxTextureClose(&items_special[0][1].texture);
  gxTextureClose(&items_special[0][2].texture);
  gxTextureClose(&items_special[0][3].texture);
  gxTextureClose(&items_special[1][0].texture);
  gxTextureClose(&items_special[1][1].texture);
  gxTextureClose(&items_device[1].texture);
#ifdef HW_RVL
  gxTextureClose(&items_device[2].texture);
  gxTextureClose(&items_device[3].texture);
  gxTextureClose(&items_device[4].texture);
  if (WiiDRC_Inited())
  {
    gxTextureClose(&items_device[5].texture);
  }
  gxTextureClose(&items_device[6].texture);
#endif
}

/****************************************************************************
 * Main Option menu
 *
 ****************************************************************************/
static void optionmenu(void)
{
  int ret, quit = 0;
  gui_menu *m = &menu_options;

  GUI_InitMenu(m);
  GUI_DrawMenuFX(m,30,0);

  while (quit == 0)
  {
    ret = GUI_RunMenu(m);

    switch (ret)
    {
      case 0:
        GUI_DeleteMenu(m);
        systemmenu();
        GUI_InitMenu(m);
        break;
      case 1:
        GUI_DeleteMenu(m);
        videomenu();
        GUI_InitMenu(m);
        break;
      case 2:
        GUI_DeleteMenu(m);
        soundmenu();
        GUI_InitMenu(m);
        break;
      case 3:
        GUI_DeleteMenu(m);
        ctrlmenu();
        GUI_InitMenu(m);
        break;
      case 4:
        GUI_DeleteMenu(m);
        prefmenu();
        GUI_InitMenu(m);
        break;
      case -1:
        quit = 1;
        break;
    }
  }

  config_save();
  GUI_DrawMenuFX(m,30,1);
  GUI_DeleteMenu(m);
}

/****************************************************************************
* Save Manager menu
*
****************************************************************************/
static t_slot slots[5];
static gx_texture *star;

static void savemenu_cb(void)
{
  int i;
  char msg[16];
  
  if (sram.on)
  {
    FONT_write("Backup Memory",16,buttons_saves[0].x+16,buttons_saves[0].y+(buttons_saves[0].h-16)/2+16,buttons_saves[0].x+buttons_saves[0].w,(GXColor)DARK_GREY);
    if (slots[0].valid)
    {
      sprintf(msg,"%d/%02d/%02d",slots[0].day,slots[0].month,slots[0].year);
      FONT_alignRight(msg,12,buttons_saves[0].x+buttons_saves[0].w-16,buttons_saves[0].y+(buttons_saves[0].h-28)/2+12,(GXColor)DARK_GREY);
      sprintf(msg,"%02d:%02d",slots[0].hour,slots[0].min);
      FONT_alignRight(msg,12,buttons_saves[0].x+buttons_saves[0].w-16,buttons_saves[0].y+(buttons_saves[0].h-28)/2+28,(GXColor)DARK_GREY);
    }

    if (sram.crc != crc32(0, &sram.sram[0], 0x10000))
      gxDrawTexture(star,22,buttons_saves[0].y+(buttons_saves[0].h-star->height)/2,star->width,star->height,255);
  }
  else
  {
    FONT_writeCenter("Backup Memory disabled",16,buttons_saves[0].x,buttons_saves[0].x+buttons_saves[0].w,buttons_saves[0].y+(buttons_saves[0].h-16)/2+16,(GXColor)DARK_GREY);
  }

  for (i=1; i<5; i++)
  {
    if (slots[i].valid)
    {
      sprintf(msg,"Slot %d",i);
      FONT_write(msg,16,buttons_saves[i].x+16,buttons_saves[i].y+(buttons_saves[i].h-16)/2+16,buttons_saves[i].x+buttons_saves[i].w,(GXColor)DARK_GREY);
      sprintf(msg,"%d/%02d/%02d",slots[i].day,slots[i].month,slots[i].year);
      FONT_alignRight(msg,12,buttons_saves[i].x+buttons_saves[i].w-16,buttons_saves[i].y+(buttons_saves[i].h-28)/2+12,(GXColor)DARK_GREY);
      sprintf(msg,"%02d:%02d",slots[i].hour,slots[i].min);
      FONT_alignRight(msg,12,buttons_saves[i].x+buttons_saves[i].w-16,buttons_saves[i].y+(buttons_saves[i].h-28)/2+28,(GXColor)DARK_GREY);
    }
    else
    {
      FONT_write("Empty Slot",16,buttons_saves[i].x+16,buttons_saves[i].y+(buttons_saves[i].h-16)/2+16,buttons_saves[i].x+buttons_saves[i].h,(GXColor)DARK_GREY);
    }

    if (i == config.s_default)
      gxDrawTexture(star,22,buttons_saves[i].y+(buttons_saves[i].h-star->height)/2,star->width,star->height,255);
  }
}

static int savemenu(void)
{
  int i, update = 0;
  int ret = 0;
  int slot = -1;
  char filename[MAXPATHLEN];
  gui_menu *m = &menu_saves;
  FILE *snap;

  star = gxTextureOpenPNG(Star_full_png,0);
  GUI_InitMenu(m);
  GUI_DrawMenuFX(m,30,0);

  m->bg_images[3].state &= ~IMAGE_SLIDE_TOP;
  m->bg_images[4].state &= ~IMAGE_SLIDE_BOTTOM;
  m->bg_images[5].state &= ~IMAGE_SLIDE_TOP;

  /* detect existing files */
  for (i=0; i<5; i++)
    slot_autodetect(i, config.s_device, &slots[i]);

  /* SRAM disabled */
  if (sram.on)
  {
    m->buttons[0].state |= BUTTON_ACTIVE;
    m->buttons[1].shift[0] = 1;
  }
  else
  {
    m->buttons[0].state &= ~BUTTON_ACTIVE;
    m->buttons[1].shift[0] = 0;
    if (m->selected == 0)
      m->selected = 1;
  }

  while (update != -1)
  {
    /* slot selection */
    if ((m->selected < 5) && (slot != m->selected))
    {
      /* update slot */
      slot = m->selected;

      /* delete previous texture if any */
      gxTextureClose(&bg_saves[0].texture);
      bg_saves[0].state &= ~IMAGE_VISIBLE;
      bg_saves[1].state |= IMAGE_VISIBLE;

      /* state slot */
      if (!config.s_device && slot && slots[slot].valid)
      {
        /* open screenshot file */
        sprintf (filename, "%s/saves/%s__%d.png", DEFAULT_PATH, rom_filename, slot - 1);
        snap = fopen(filename, "rb");
        if (snap)
        {
          /* load texture from file */
          bg_saves[0].texture = gxTextureOpenPNG(0,snap);
          if (bg_saves[0].texture)
          {
            /* set menu background */
            bg_saves[0].w = bg_saves[0].texture->width * 2;
            if (config.aspect & 2) bg_saves[0].w = (bg_saves[0].w * 3) / 4;
            bg_saves[0].h = bg_saves[0].texture->height * 2;
            bg_saves[0].x = (vmode->fbWidth - bg_saves[0].w) / 2;
            bg_saves[0].y = (vmode->efbHeight - bg_saves[0].h) / 2;
            bg_saves[0].state |= IMAGE_VISIBLE;
            bg_saves[1].state &= ~IMAGE_VISIBLE;
          }
          fclose(snap);
        }
      }
    }

    /* draw menu */
    GUI_DrawMenu(m);

    /* update menu */
    update = GUI_UpdateMenu(m);

    if (update > 0)
    {
      switch (m->selected)
      {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4: /* Slot selection */
        {
          /* enable right window */
          m->bg_images[7].state |= IMAGE_VISIBLE;
          m->buttons[5].state |= BUTTON_VISIBLE;
          m->buttons[6].state |= BUTTON_VISIBLE;
          m->buttons[7].state |= BUTTON_VISIBLE;
          m->buttons[8].state |= BUTTON_VISIBLE;

          /* only enable valid options */
          if (slots[slot].valid)
          {
            m->buttons[5].state |= BUTTON_ACTIVE;
            m->buttons[7].state |= BUTTON_ACTIVE;
            m->buttons[6].shift[0] = 1;
            m->buttons[6].shift[1] = 1;
            m->buttons[8].shift[0] = 1;
            m->selected = 5;
          }
          else
          {
            m->buttons[5].state &= ~BUTTON_ACTIVE;
            m->buttons[7].state &= ~BUTTON_ACTIVE;
            m->buttons[6].shift[0] = 0;
            m->buttons[6].shift[1] = 2;
            m->buttons[8].shift[0] = (slot > 0) ? 2 : 0;
            m->selected = 8;
          }

          /* state slot 'only' button */
          if (slot > 0)
          {
            m->buttons[6].state |= BUTTON_ACTIVE;
            m->buttons[5].shift[1] = 1;
            m->buttons[7].shift[0] = 1;
          }
          else
          {
            m->buttons[6].state &= ~BUTTON_ACTIVE;
            m->buttons[5].shift[1] = 2;
            m->buttons[7].shift[0] = 2;
          }

          /* disable left buttons */
          m->buttons[0].state &= ~BUTTON_ACTIVE;
          m->buttons[1].state &= ~BUTTON_ACTIVE;
          m->buttons[2].state &= ~BUTTON_ACTIVE;
          m->buttons[3].state &= ~BUTTON_ACTIVE;
          m->buttons[4].state &= ~BUTTON_ACTIVE;

          /* keep current selection highlighted */
          m->buttons[slot].state |= BUTTON_SELECTED;

          /* slide in window */
          GUI_DrawMenuFX(m, 20, 0);

          break;
        }

        case 5: /* load file */
        {
          if (slots[slot].valid)
          {
            ret = slot_load(slot,config.s_device);
         
            /* force exit */
            if (ret > 0)
            {
              GUI_DrawMenuFX(m, 20, 1);
              m->buttons[slot].state &= ~BUTTON_SELECTED;
              m->bg_images[7].state &= ~IMAGE_VISIBLE;
              if (sram.on)
                m->buttons[0].state |= BUTTON_ACTIVE;
              m->buttons[1].state |= BUTTON_ACTIVE;
              m->buttons[2].state |= BUTTON_ACTIVE;
              m->buttons[3].state |= BUTTON_ACTIVE;
              m->buttons[4].state |= BUTTON_ACTIVE;
              m->buttons[5].state &= ~BUTTON_VISIBLE;
              m->buttons[6].state &= ~BUTTON_VISIBLE;
              m->buttons[7].state &= ~BUTTON_VISIBLE;
              m->buttons[8].state &= ~BUTTON_VISIBLE;
              m->selected = slot;
              update = -1;
            }
          }
          break;
        }

        case 6: /* set default slot */
        {
          config.s_default = slot;
          config_save();
          break;
        }

        case 7: /* delete file */
        {
          if (slots[slot].valid)
          {
            if (GUI_WaitConfirm("Warning","Delete Save File ?"))
            {
              if (slot_delete(slot,config.s_device) >= 0)
              {
                /* hide screenshot */
                gxTextureClose(&bg_saves[0].texture);
                bg_saves[0].state &= ~IMAGE_VISIBLE;
                bg_saves[1].state |= IMAGE_VISIBLE;
                slots[slot].valid = 0;
                update = -1;
              }
            }
          }
          break;
        }

        case 8: /* save file */
        {
          ret = slot_save(slot,config.s_device);

          /* force exit */
          if (ret > 0)
          {
            GUI_DrawMenuFX(m, 20, 1);
            m->buttons[slot].state &= ~BUTTON_SELECTED;
            m->bg_images[7].state &= ~IMAGE_VISIBLE;
            if (sram.on)
              m->buttons[0].state |= BUTTON_ACTIVE;
            m->buttons[1].state |= BUTTON_ACTIVE;
            m->buttons[2].state |= BUTTON_ACTIVE;
            m->buttons[3].state |= BUTTON_ACTIVE;
            m->buttons[4].state |= BUTTON_ACTIVE;
            m->buttons[5].state &= ~BUTTON_VISIBLE;
            m->buttons[6].state &= ~BUTTON_VISIBLE;
            m->buttons[7].state &= ~BUTTON_VISIBLE;
            m->buttons[8].state &= ~BUTTON_VISIBLE;
            m->selected = slot;
            update = -1;
          }
          break;
        }

        default:
          break;
      }
    }

    if (update < 0)
    {
      /* close right window */
      if (m->bg_images[7].state & IMAGE_VISIBLE)
      {
        /* slide out window */
        GUI_DrawMenuFX(m, 20, 1);

        /* clear current selection */
        m->buttons[slot].state &= ~BUTTON_SELECTED;

        /* enable left buttons */
        if (sram.on)
          m->buttons[0].state |= BUTTON_ACTIVE;
        m->buttons[1].state |= BUTTON_ACTIVE;
        m->buttons[2].state |= BUTTON_ACTIVE;
        m->buttons[3].state |= BUTTON_ACTIVE;
        m->buttons[4].state |= BUTTON_ACTIVE;

        /* disable right window */
        m->bg_images[7].state &= ~IMAGE_VISIBLE;
        m->buttons[5].state &= ~BUTTON_VISIBLE;
        m->buttons[6].state &= ~BUTTON_VISIBLE;
        m->buttons[7].state &= ~BUTTON_VISIBLE;
        m->buttons[8].state &= ~BUTTON_VISIBLE;

        /* stay in menu */
        m->selected = slot;
        update = 0;
      }
    }
  }

  /* leave menu */
  m->bg_images[3].state |= IMAGE_SLIDE_TOP;
  m->bg_images[4].state |= IMAGE_SLIDE_BOTTOM;
  m->bg_images[5].state |= IMAGE_SLIDE_TOP;
  GUI_DrawMenuFX(m,30,1);
  GUI_DeleteMenu(m);
  gxTextureClose(&star);
  return ret;
}

/****************************************************************************
 * Load Game menu
 *
 ****************************************************************************/
static int loadgamemenu ()
{
  int ret, filetype;
  gui_menu *m = &menu_load;
  GUI_InitMenu(m);
  GUI_DrawMenuFX(m,30,0);

  while (1)
  {
    ret = GUI_RunMenu(m);

    switch (ret)
    {
      /*** Button B ***/
      case -1: 
        GUI_DrawMenuFX(m,30,1);
        GUI_DeleteMenu(m);
        return 0;

      /*** Load from selected device */
      default:
      {
        /* ROM File type */
        filetype = ret - 1;

        /* Try to open current directory */
        if (ret > 0)
        {
          ret = OpenDirectory(config.l_device, filetype);
        }
        else
        {
          ret = OpenDirectory(TYPE_RECENT, filetype);
        }

        if (ret)
        {
          GUI_DeleteMenu(m);
          if (FileSelector(filetype) > 0)
          {
            /* directly jump to game */
            return 1;
          }
          GUI_InitMenu(m);
        }
        break;
      }
    }
  }

  return 0;
}

/***************************************************************************
  * Show rom info screen
 ***************************************************************************/
static void showrominfo (void)
{
  char items[15][64];

  /* fill ROM infos */
  sprintf (items[0], "Console Type: %s", rominfo.consoletype);
  sprintf (items[1], "Copyright: %s", rominfo.copyright);
  sprintf (items[2], "Company Name: %s", get_company());
  sprintf (items[3], "Domestic Name:");
  sprintf (items[4], "%s",rominfo.domestic);
  sprintf (items[5], "International Name:");
  sprintf (items[6], "%s",rominfo.international);
  sprintf (items[7], "Type: %s (%s)",rominfo.ROMType, strcmp(rominfo.ROMType, "AI") ? "Game" : "Educational");
  sprintf (items[8], "Product ID: %s", rominfo.product);
  sprintf (items[9], "Checksum: %04x (%04x) (%s)", rominfo.checksum, rominfo.realchecksum,
                                                  (rominfo.checksum == rominfo.realchecksum) ? "GOOD" : "BAD");
  
  sprintf (items[10], "Supports: ");
  if (rominfo.peripherals & (1 << 1))
  {
    strcat(items[10],get_peripheral(1));
    strcat(items[10],", ");
  }
  else if (rominfo.peripherals & (1 << 0))
  {
    strcat(items[10],get_peripheral(0));
    strcat(items[10],", ");
  }
  if (rominfo.peripherals & (1 << 7))
  {
    strcat(items[10],get_peripheral(7));
    strcat(items[10],", ");
  }
  if (rominfo.peripherals & (1 << 8))
  {
    strcat(items[10],get_peripheral(8));
    strcat(items[10],", ");
  }
  if (rominfo.peripherals & (1 << 11))
  {
    strcat(items[10],get_peripheral(11));
    strcat(items[10],", ");
  }
  if (rominfo.peripherals & (1 << 13))
  {
    strcat(items[10],get_peripheral(13));
    strcat(items[10],", ");
  }
  if (strlen(items[10]) > 10)
    items[10][strlen(items[10]) - 2] = 0;

  sprintf (items[11], "ROM end: $%06X", rominfo.romend);

  if (sram.custom)
    sprintf (items[12], "Serial EEPROM");
  else if (sram.detected)
    sprintf (items[12], "SRAM Start: $%06X", sram.start);
  else
    sprintf (items[12], "No Backup Memory specified");

  if (sram.custom == 1) 
    sprintf (items[13], "Type: I2C (24Cxx)");
  else if (sram.custom == 2) 
    sprintf (items[13], "Type: SPI (25x512/95x512)");
  else if (sram.custom == 3) 
    sprintf (items[13], "Type: I2C (93C46)");
  else if (sram.detected)
    sprintf (items[13], "SRAM End: $%06X", sram.end);
  else if (sram.on)
    sprintf (items[13], "SRAM enabled by default");
  else
    sprintf (items[13], "SRAM disabled by default");
  
  if (region_code == REGION_USA)
    sprintf (items[14], "Region Code: %s (USA)", rominfo.country);
  else if (region_code == REGION_EUROPE)
    sprintf (items[14], "Region Code: %s (EUR)", rominfo.country);
  else if (region_code == REGION_JAPAN_NTSC)
    sprintf (items[14], "Region Code: %s (JPN)", rominfo.country);
  else if (region_code == REGION_JAPAN_PAL)
    sprintf (items[14], "Region Code: %s (JPN-PAL)", rominfo.country);
  
  GUI_TextWindow(&menu_main, "ROM Header Info", items, 15, 15);
}

/***************************************************************************
  * Show credits
 ***************************************************************************/
static void showcredits(void)
{
  int offset = 0;
  
  gx_texture *texture = gxTextureOpenPNG(Bg_credits_png,0);
  s16 p = 0;

  while (!p)
  {
    gxClearScreen ((GXColor)BLACK);
    if (texture)
      gxDrawTexture(texture, (640-texture->width)/2, (480-texture->height)/2, texture->width, texture->height,255);

    FONT_writeCenter("Genesis Plus GX Core", 24, 0, 640, 480 - offset, (GXColor)LIGHT_BLUE);
    FONT_writeCenter("improved emulation code & additional features by Eke-Eke", 18, 0, 640, 516 - offset, (GXColor)WHITE);
    FONT_writeCenter("original Genesis Plus (1.3) version by Charles MacDonald", 18, 0, 640, 534 - offset, (GXColor)WHITE);
    FONT_writeCenter("original Z80 core by Juergen Buchmueller", 18, 0, 640, 552 - offset, (GXColor)WHITE);
    FONT_writeCenter("original 68k core (Musashi) by Karl Stenerud", 18, 0, 640, 570 - offset, (GXColor)WHITE);
    FONT_writeCenter("original YM2612/2413 cores by Jarek Burczynski, Tatsuyuki Satoh", 18, 0, 640, 588 - offset, (GXColor)WHITE);
    FONT_writeCenter("SVP core by Gravydas Ignotas (Notaz)", 18, 0, 640, 606 - offset, (GXColor)WHITE);
    FONT_writeCenter("Blip Buffer Library & NTSC Video Filter by Shay Green (Blargg)", 18, 0, 640, 624 - offset, (GXColor)WHITE);
    FONT_writeCenter("3-Band EQ implementation by Neil C", 18, 0, 640, 642 - offset, (GXColor)WHITE);
    FONT_writeCenter("Ogg Vorbis 'Tremor' Library by Xiph.org Foundation", 18, 0, 640, 660 - offset, (GXColor)WHITE);
    FONT_writeCenter("MINIMP3 Library by Lieff", 18, 0, 640, 678 - offset, (GXColor)WHITE);
    FONT_writeCenter("CHD Library by Aaron Giles, Romain Tisserand", 18, 0, 640, 696 - offset, (GXColor)WHITE);
    FONT_writeCenter("DR_FLAC Library by David Reid", 18, 0, 640, 714 - offset, (GXColor)WHITE);
    FONT_writeCenter("ZLIB Library by Jean-loup Gailly & Mark Adler", 18, 0, 640, 732 - offset, (GXColor)WHITE);
    FONT_writeCenter("LZMA Library by Igor Pavlov", 18, 0, 640, 750 - offset, (GXColor)WHITE);
    FONT_writeCenter("ZSTD Library by Meta Platforms, Inc. and affiliates", 18, 0, 640, 768 - offset, (GXColor)WHITE);

    FONT_writeCenter("Special thanks to ...", 20, 0, 640, 808 - offset, (GXColor)LIGHT_GREEN);
    FONT_writeCenter("Nemesis, Tasco Deluxe, Mask of Destiny, Bart Trzynadlowski, Haze,", 18, 0, 640, 844 - offset, (GXColor)WHITE);
    FONT_writeCenter("Jorge Cwik, Stef, Notaz, AamirM, Steve Snake, Charles MacDonald", 18, 0, 640, 862 - offset, (GXColor)WHITE);
    FONT_writeCenter("Spritesmind & SMS Power forums members for their technical help", 18, 0, 640, 880 - offset, (GXColor)WHITE);

    FONT_writeCenter("Gamecube & Wii port", 24, 0, 640, 938 - offset, (GXColor)LIGHT_BLUE);
    FONT_writeCenter("porting code, GUI engine & design by Eke-Eke", 18, 0, 640, 974 - offset, (GXColor)WHITE);
    FONT_writeCenter("original Gamecube port by Softdev, Honkeykong & Markcube", 18, 0, 640, 992 - offset, (GXColor)WHITE);
    FONT_writeCenter("original icons, logo & button design by Low Lines", 18, 0, 640, 1010 - offset, (GXColor)WHITE);
    FONT_writeCenter("credit illustration by Orioto (Deviant Art)", 18, 0, 640, 1028 - offset, (GXColor)WHITE);
    FONT_writeCenter("memory card icon design by Brakken", 18, 0, 640, 1046 - offset, (GXColor)WHITE);
    FONT_writeCenter("libogc by Shagkur & various other contibutors", 18, 0, 640, 1064 - offset, (GXColor)WHITE);
    FONT_writeCenter("libfat by Chism", 18, 0, 640, 1082 - offset, (GXColor)WHITE);
    FONT_writeCenter("wiiuse by Michael Laforest (Para)", 18, 0, 640, 1100 - offset, (GXColor)WHITE);
    FONT_writeCenter("asndlib & OGG player by Francisco Mu�oz (Hermes)", 18, 0, 640, 1118 - offset, (GXColor)WHITE);
    FONT_writeCenter("libwiidrc by Fix94", 18, 0, 640, 1136 - offset, (GXColor)WHITE);
    FONT_writeCenter("libpng by their respective authors", 18, 0, 640, 1154 - offset, (GXColor)WHITE);
    FONT_writeCenter("devkitPPC by Wintermute", 18, 0, 640, 1172 - offset, (GXColor)WHITE);

    FONT_writeCenter("Special thanks to ...", 20, 0, 640, 1212 - offset, (GXColor)LIGHT_GREEN);
    FONT_writeCenter("Softdev, Tmbinc, Costis, Emukiddid, Team Twiizer", 18, 0, 640, 1248 - offset, (GXColor)WHITE);
    FONT_writeCenter("Brakken & former Tehskeen members for their support", 18, 0, 640, 1266 - offset, (GXColor)WHITE);
    FONT_writeCenter("Anca, my wife, for her patience & various ideas", 18, 0, 640, 1284 - offset, (GXColor)WHITE);

    gxSetScreen();
    p = m_input.keys;
    gxSetScreen();
    p |= m_input.keys;
    offset ++;
    if (offset > 1302)
      offset = 0;
  }

  gxTextureClose(&texture);
}

static void exitmenu(void)
{
  char title[64];
  char infos[64];
  char *items[3] =
  {
    "View Credits",
#ifdef HW_RVL
    "Exit to System Menu",
#else
    "Reset System",
#endif
    "Return to Loader",
  };

#ifdef HW_RVL
  extern u8 __Arena2Lo[];
  sprintf(title, "%s (IOS %d)", VERSION, IOS_GetVersion());
  if ((u32)SYS_GetArena2Lo() > (u32)__Arena2Lo)
    sprintf(infos, "%d bytes free (MEM2)", (u32)SYS_GetArena2Size());
  else
    sprintf(infos, "%d bytes free (MEM1)", (u32)SYS_GetArena1Size());
#else
  sprintf(title, "%s (GCN)", VERSION);
  sprintf(infos, "%d bytes free", (u32)SYS_GetArenaSize());
#endif

  /* check if loader stub exists */
  int maxitems = reload ? 3 : 2;

  /* display option window */
  switch (GUI_OptionWindow(&menu_main, title, infos, items, maxitems))
  {
    case 0: /* credits */
      GUI_DeleteMenu(&menu_main);
      showcredits();
      GUI_InitMenu(&menu_main);
      break;

    case 1: /* reset */
#ifdef HW_RVL
      gxTextureClose(&w_pointer);
#endif
      GUI_DeleteMenu(&menu_main);
      GUI_FadeOut();
      shutdown();
#ifdef HW_RVL
      SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
#else
      SYS_ResetSystem(SYS_HOTRESET,0,0);
#endif
      break;

    case 2: /* exit to loader */
#ifdef HW_RVL
      gxTextureClose(&w_pointer);
#endif
      GUI_DeleteMenu(&menu_main);
      GUI_FadeOut();
      shutdown();
      SYS_ResetSystem(SYS_SHUTDOWN,0,0);
      __lwp_thread_stopmultitasking(*reload);
      break;

    default:
      break;
  }
}

/****************************************************************************
 * Main Menu
 *
 ****************************************************************************/

static void mainmenu_cb(void)
{
  char temp[4];
  gui_menu *m = &menu_main;
  int status = areplay_get_status();

  /* Action Replay Switch current status */
  if (status == AR_SWITCH_TRAINER)
    strcpy(temp,"TM");
  else if (status == AR_SWITCH_ON)
    strcpy(temp,"ON");
  else
    strcpy(temp,"OFF");

  /* Display informations */
  if (m->selected == 6)
  {
    FONT_writeCenter("Action\nReplay", 14, m->items[6].x, m->items[6].x + 54, m->items[6].y + (m->items[6].h - 28)/2 + 14, (GXColor)DARK_GREY);
    FONT_writeCenter(temp, 11, m->items[6].x + 56 + 3, m->items[6].x + 78 + 2, m->items[6].y + (m->items[6].h - 11)/2 + 11, (GXColor)DARK_GREY);
  }
  else
  {
    FONT_writeCenter("Action\nReplay", 12, m->items[6].x + 4, m->items[6].x + 54, m->items[6].y + (m->items[6].h - 24)/2 + 12, (GXColor)DARK_GREY);
    FONT_writeCenter(temp, 10, m->items[6].x + 56, m->items[6].x + 78, m->items[6].y + (m->items[6].h - 10)/2 + 10, (GXColor)DARK_GREY);
  }
}

void mainmenu(void)
{
  char filename[MAXPATHLEN];
  int status, quit = 0;

  /* Autosave Backup RAM */
  slot_autosave(0, config.s_device);

#ifdef HW_RVL
  /* Detect shutdown request */
  if (Shutdown)
  {
    GUI_FadeOut();
    shutdown();
    if (reload)
    {
      /* exit to loader if requested */
      SYS_ResetSystem(SYS_SHUTDOWN,0,0);
      __lwp_thread_stopmultitasking(*reload);
    }
    else
    {
      /* shutdown system by default */
      SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    }
  }

  /* Wiimote pointer */
  w_pointer = gxTextureOpenPNG(generic_point_png,0);
#endif

  gui_menu *m = &menu_main;

  /* Update main menu */
  if (!m->screenshot)
  {
    if (config.bg_overlay)
    {
      bg_main[1].state  |= IMAGE_VISIBLE;
      bg_misc[1].state  |= IMAGE_VISIBLE;
      bg_ctrls[1].state |= IMAGE_VISIBLE;
      bg_list[1].state  |= IMAGE_VISIBLE;
      bg_saves[2].state |= IMAGE_VISIBLE;
    }
    else
    {
      bg_main[1].state  &= ~IMAGE_VISIBLE;
      bg_misc[1].state  &= ~IMAGE_VISIBLE;
      bg_ctrls[1].state &= ~IMAGE_VISIBLE;
      bg_list[1].state  &= ~IMAGE_VISIBLE;
      bg_saves[2].state &= ~IMAGE_VISIBLE;
    }

    if (system_hw)
    {
      m->screenshot = 128;
      m->bg_images[0].state &= ~IMAGE_VISIBLE;
      m->items[0].y -= 90;
      m->items[1].y -= 90;
      m->items[2].y -= 90;
      m->buttons[0].y -= 90;
      m->buttons[1].y -= 90;
      m->buttons[2].y -= 90;
      m->buttons[0].shift[1] = 3;
      m->buttons[1].shift[1] = 3;
      m->buttons[2].shift[1] = 3;
      m->buttons[3].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
      m->buttons[4].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
      m->buttons[5].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
      m->buttons[7].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
      m->buttons[8].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
      m->buttons[9].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
      if (areplay_get_status() >= 0)
      {
        menu_main.buttons[6].state |= (BUTTON_VISIBLE | BUTTON_ACTIVE);
        menu_main.items[6].data = Button_sm_grey_png;
        menu_main.cb = mainmenu_cb;
        menu_main.buttons[3].shift[1] = 3;
        menu_main.buttons[7].shift[0] = 1;
        menu_main.buttons[8].shift[2] = 2;
      }
    }
  }

  GUI_InitMenu(m);
  GUI_DrawMenuFX(m,10,0);

  while (quit == 0)
  {
    switch (GUI_RunMenu(m))
    {
      /*** Load Game Menu ***/
      case 0:
      {
        GUI_DrawMenuFX(m,30,1);
        GUI_DeleteMenu(m);

        if (loadgamemenu())
        {
          /* restart emulation */
          reloadrom();

          /* check current controller configuration */
          if (!gx_input_FindDevices())
          {
            GUI_InitMenu(m);
            GUI_DrawMenuFX(m,30,0);
            GUI_WaitPrompt("Error","Invalid Controllers Settings");
            break;
          }

          /* exit to game and reinitialize emulation */
          gxClearScreen((GXColor)BLACK);
          gxSetScreen();
          quit = 1;
          break;
        }

        GUI_InitMenu(m);
        GUI_DrawMenuFX(m,30,0);
        break;
      }

      /*** Options Menu */
      case 1:
      {
        GUI_DrawMenuFX(m,30,1);
        GUI_DeleteMenu(m);
        optionmenu();
        GUI_InitMenu(m);
        GUI_DrawMenuFX(m,30,0);
        break;
      }

      /*** Exit Menu ***/
      case 2:
      {
        exitmenu();
        break;
      }

      /*** Save Manager ***/
      case 3:
      {
        GUI_DrawMenuFX(m,30,1);
        GUI_DeleteMenu(m);

        if (savemenu())
        {
          /* check current controller configuration */
          if (!gx_input_FindDevices())
          {
            GUI_InitMenu(m);
            GUI_DrawMenuFX(m,30,0);
            GUI_WaitPrompt("Error","Invalid Controllers Settings");
            break;
          }

          /* exit to game */
          quit = 1;
          break;
        }

        GUI_InitMenu(m);
        GUI_DrawMenuFX(m,30,0);
        break;
      }

      /*** Soft / Hard reset ***/
      case 4:
      {
        /* check current controller configuration */
        if (!gx_input_FindDevices())
        {
          GUI_WaitPrompt("Error","Invalid Controllers Settings");
          break;
        }

        /* reinitialize emulation */
        GUI_DrawMenuFX(m,10,1);
        GUI_DeleteMenu(m);
        gxClearScreen((GXColor)BLACK);
        gxSetScreen();

        if (system_hw & SYSTEM_MD)
        {
          /* Soft Reset */
          gen_reset(0);
        }
        else if ((system_hw == SYSTEM_SMS) && (region_code != REGION_JAPAN_NTSC))
        {
          /* assert RESET input (export Master System 1 only) */
          io_reg[0x0D] &= ~IO_RESET_HI;
        }
        else
        {
          /* Hard Reset */
          system_reset();
        }

        /* exit to game */
        quit = 1;
        break;
      }

      /*** Cheats menu ***/
      case 5:
      {
        GUI_DrawMenuFX(m,30,1);
        GUI_DeleteMenu(m);
        CheatMenu();
        GUI_InitMenu(m);
        GUI_DrawMenuFX(m,30,0);
        break;
      }

      /*** Action Replay switch ***/
      case 6:
      {
        status = (areplay_get_status() + 1) % (AR_SWITCH_TRAINER + 1);
        areplay_set_status(status);
        status = areplay_get_status();
        GUI_DeleteMenu(m);
        if (status == AR_SWITCH_TRAINER) m->items[6].data = Button_sm_blue_png;
        else if (status == AR_SWITCH_ON) m->items[6].data = Button_sm_yellow_png;
        else m->items[6].data = Button_sm_grey_png;
        GUI_InitMenu(m);
        break;
      }

      /*** Return to Game ***/
      case 7:
      case -1:
      {
        if (system_hw)
        {
          /* check current controller configuration */
          if (!gx_input_FindDevices())
          {
            GUI_WaitPrompt("Error","Invalid Controllers Settings");
            break;
          }

          /* exit to game */
          GUI_DrawMenuFX(m,10,1);
          GUI_DeleteMenu(m);
          quit = 1;
        }
        break;
      }

      /*** Game Capture ***/
      case 8:
      {
        /* PNG filename */
        sprintf(filename,"%s/snaps/%s.png", DEFAULT_PATH, rom_filename);

        /* Save file and return */
        gxSaveScreenshot(filename);
        break;
      }

      /*** ROM information screen ***/
      case 9:
      {
        showrominfo();
        break;
      }
    }
  }

  /*** Remove any still held buttons ***/
  while (PAD_ButtonsHeld(0))
  {
    VIDEO_WaitVSync();
    PAD_ScanPads();
  }
#ifdef HW_RVL
  while (WPAD_ButtonsHeld(0)) 
  {
    VIDEO_WaitVSync();
    WPAD_ScanPads();
  }
  gxTextureClose(&w_pointer);

  /* USB Mouse support */
  if ((input.system[0] == SYSTEM_MOUSE) || (input.system[1] == SYSTEM_MOUSE))
  {
    MOUSE_Init();
  }
  else
  {
    MOUSE_Deinit();
  }
#endif
}
