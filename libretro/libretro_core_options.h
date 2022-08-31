#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Definitions
 ********************************
*/

#if defined(M68K_OVERCLOCK_SHIFT) || defined(Z80_OVERCLOCK_SHIFT)
#define HAVE_OVERCLOCK
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_v2_category option_cats_us[] = {
   {
      "system",
      "System",
      "Configure base hardware selection / region / BIOS / Sega CD save file parameters."
   },
   {
      "video",
      "Video",
      "Configure aspect ratio / display cropping / video filter / frame skipping parameters."
   },
   {
      "audio",
      "Audio",
      "Configure emulated audio devices."
   },
   {
      "input",
      "Input",
      "Configure light gun / mouse input."
   },
   {
      "hacks",
      "Emulation Hacks",
      "Configure processor overclocking and emulation accuracy parameters affecting low-level performance and compatibility."
   },
   {
      "channel_volume",
      "Advanced Channel Volume Settings",
      "Configure the volume of individual hardware audio channels."
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {
   {
      "genesis_plus_gx_system_hw",
      "System Hardware",
      NULL,
      "Runs loaded content with a specific emulated console. 'Auto' will select the most appropriate system for the current game.",
      NULL,
      "system",
      {
         { "auto",                 "Auto"               },
         { "sg-1000",              "SG-1000"            },
         { "sg-1000 II",           "SG-1000 II"         },
         { "mark-III",             "Mark III"           },
         { "master system",        "Master System"      },
         { "master system II",     "Master System II"   },
         { "game gear",            "Game Gear"          },
         { "mega drive / genesis", "Mega Drive/Genesis" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_region_detect",
      "System Region",
      NULL,
      "Specify which region the system is from. For consoles other than the Game Gear, 'PAL' is 50hz while 'NTSC' is 60hz. Games may run faster or slower than normal if the incorrect region is selected.",
      NULL,
      "system",
      {
         { "auto",    "Auto"   },
         { "ntsc-u",  "NTSC-U" },
         { "pal",     "PAL"    },
         { "ntsc-j",  "NTSC-J" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_bios",
      "System Boot ROM",
      NULL,
      "Use official BIOS/bootloader for emulated hardware, if present in RetroArch's system directory. Displays console-specific start-up sequence/animation, then runs loaded content.",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_bram",
      "CD System BRAM",
      NULL,
      "When running Sega CD content, specifies whether to share a single save file between all games from a specific region (Per-BIOS) or to create a separate save file for each game (Per-Game). Note that the Sega CD has limited internal storage, sufficient only for a handful of titles. To avoid running out of space, the 'Per-Game' setting is recommended.",
      NULL,
      "system",
      {
         { "per bios", "Per-BIOS" },
         { "per game", "Per-Game" },
         { NULL, NULL },
      },
      "per bios"
   },
   {
      "genesis_plus_gx_add_on",
      "CD add-on (MD mode) (Requires Restart)",
      NULL,
      "Specify which add-on to use for CD audio playback with supported Mega Drive/Genesis games.",
      NULL,
      "system",
      {
         { "auto",         "Auto" },
         { "sega/mega cd", "Sega/Mega CD" },
         { "megasd",       "MegaSD" },
         { "none",         "None" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_lock_on",
      "Cartridge Lock-On",
      NULL,
      "Lock-On Technology is a Mega Drive/Genesis feature that allowed an older game to connect to the pass-through port of a special cartridge for extended or altered gameplay. This option specifies which type of special 'lock-on' cartridge to emulate. A corresponding bios file must be present in RetroArch's system directory.",
      NULL,
      "system",
      {
         { "disabled",            NULL },
         { "game genie",          "Game Genie" },
         { "action replay (pro)", "Action Replay (Pro)" },
         { "sonic & knuckles",    "Sonic & Knuckles" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_aspect_ratio",
      "Core-Provided Aspect Ratio",
      NULL,
      "Choose the preferred content aspect ratio. This will only apply when RetroArch's aspect ratio is set to 'Core provided' in the Video settings.",
      NULL,
      "video",
      {
         { "auto",     "Auto" },
         { "NTSC PAR", NULL },
         { "PAL PAR",  NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_overscan",
      "Borders",
      NULL,
      "Enable this to display the overscan regions at the top/bottom and/or left/right of the screen. These would normally be hidden by the bezel around the edge of a standard-definition television.",
      NULL,
      "video",
      {
         { "disabled",   NULL },
         { "top/bottom", "Top/Bottom" },
         { "left/right", "Left/Right" },
         { "full",       "Full" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_left_border",
      "Hide Master System Side Borders",
      NULL,
      "Cuts off 8 pixels from either the left side of the screen, or both left and right sides when running Master System games.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "left border", "Left Border Only" },
         { "left & right borders", "Left & Right Borders" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_gg_extra",
      "Game Gear Extended Screen",
      NULL,
      "Forces Game Gear titles to run in 'SMS' mode, with an increased resolution of 256x192. May show additional content, but typically displays a border of corrupt/unwanted image data.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_blargg_ntsc_filter",
      "Blargg NTSC Filter",
      NULL,
      "Apply a video filter to mimic various NTSC TV signals.",
      NULL,
      "video",
      {
         { "disabled",   NULL },
         { "monochrome", "Monochrome" },
         { "composite",  "Composite" },
         { "svideo",     "S-Video" },
         { "rgb",        "RGB" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_lcd_filter",
      "LCD Ghosting Filter",
      NULL,
      "Apply an image 'ghosting' filter to mimic the display characteristics of the Game Gear and 'Genesis Nomad' LCD panels.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_render",
      "Interlaced Mode 2 Output",
      NULL,
      "Interlaced Mode 2 allows the Mega Drive/Genesis to output a double height (high resolution) 320x448 image by drawing alternate scanlines each frame (this is used by 'Sonic the Hedgehog 2' and 'Combat Cars' multiplayer modes). 'Single Field' mimics original hardware, producing each field (320x224) alternatively with flickering/interlacing artefacts. 'Double Field' simulates the interlaced display, which stabilises the image but causes mild blurring.",
      NULL,
      "video",
      {
         { "single field", "Single Field" },
         { "double field", "Double Field" },
         { NULL, NULL },
      },
      "single field"
   },
   {
      "genesis_plus_gx_frameskip",
      "Frameskip",
      NULL,
      "Skip frames to avoid audio buffer under-run (crackling). Improves performance at the expense of visual smoothness. 'Auto' skips frames when advised by the frontend. 'Manual' utilises the 'Frameskip Threshold (%)' setting.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "auto",     "Auto" },
         { "manual",   "Manual" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_frameskip_threshold",
      "Frameskip Threshold (%)",
      NULL,
      "When 'Frameskip' is set to 'Manual', specifies the audio buffer occupancy threshold (percentage) below which frames will be skipped. Higher values reduce the risk of crackling by causing frames to be dropped more frequently.",
      NULL,
      "video",
      {
         { "15", NULL },
         { "18", NULL },
         { "21", NULL },
         { "24", NULL },
         { "27", NULL },
         { "30", NULL },
         { "33", NULL },
         { "36", NULL },
         { "39", NULL },
         { "42", NULL },
         { "45", NULL },
         { "48", NULL },
         { "51", NULL },
         { "54", NULL },
         { "57", NULL },
         { "60", NULL },
         { NULL, NULL },
      },
      "33"
   },
   {
      "genesis_plus_gx_ym2413",
      "Master System FM (YM2413)",
      NULL,
      "Enable emulation of the FM Sound Unit used by certain Sega Mark III/Master System games for enhanced audio output.",
      NULL,
      "audio",
      {
         { "auto",     "Auto" },
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "auto"
   },
#ifdef HAVE_OPLL_CORE
   {
      "genesis_plus_gx_ym2413_core",
      "Master System FM (YM2413) Core",
      NULL,
      "Select method used to emulate the FM Sound Unit of the Sega Mark III/Master System. 'MAME' option is fast, and runs full speed on most systems. 'Nuked' option is cycle accurate, very high quality, and has substantial CPU requirements.",
      NULL,
      "audio",
      {
         { "mame",  "MAME" },
         { "nuked", "Nuked" },
         { NULL, NULL },
      },
      "mame"
   },
#endif
   {
      "genesis_plus_gx_ym2612",
      "Mega Drive / Genesis FM",
      NULL,
#ifdef HAVE_YM3438_CORE
      "Select method used to emulate the FM synthesizer (main sound generator) of the Mega Drive/Genesis. 'MAME' options are fast, and run full speed on most systems. 'Nuked' options are cycle accurate, very high quality, and have substantial CPU requirements. The 'YM2612' chip is used by the original Model 1 Mega Drive/Genesis. The 'YM3438' is used in later Mega Drive/Genesis revisions.",
#else
      "Select method used to emulate the FM synthesizer (main sound generator) of the Mega Drive/Genesis. The 'YM2612' chip is used by the original Model 1 Mega Drive/Genesis. The 'YM3438' is used in later Mega Drive/Genesis revisions.",
#endif
      NULL,
      "audio",
      {
         { "mame (ym2612)",          "MAME (YM2612)" },
         { "mame (asic ym3438)",     "MAME (ASIC YM3438)" },
         { "mame (enhanced ym3438)", "MAME (Enhanced YM3438)" },
#ifdef HAVE_YM3438_CORE
         { "nuked (ym2612)",         "Nuked (YM2612)" },
         { "nuked (ym3438)",         "Nuked (YM3438)" },
#endif
         { NULL, NULL },
      },
      "mame (ym2612)"
   },
   {
      "genesis_plus_gx_sound_output",
      "Sound Output",
      NULL,
      "Select stereo or mono sound reproduction.",
      NULL,
      "audio",
      {
         { "stereo", "Stereo" },
         { "mono",   "Mono" },
         { NULL, NULL },
      },
      "stereo"
   },
   {
      "genesis_plus_gx_audio_filter",
      "Audio Filter",
      NULL,
      "Enable a low pass audio filter to better simulate the characteristic sound of a Model 1 Mega Drive/Genesis.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "low-pass", "Low-Pass" },
#if HAVE_EQ
         { "EQ",       NULL },
#endif
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_lowpass_range",
      "Low-Pass Filter %",
      NULL,
      "Specify the cut-off frequency of the audio low pass filter. A higher value increases the perceived 'strength' of the filter, since a wider range of the high frequency spectrum is attenuated.",
      NULL,
      "audio",
      {
         { "5",  NULL },
         { "10", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "35", NULL },
         { "40", NULL },
         { "45", NULL },
         { "50", NULL },
         { "55", NULL },
         { "60", NULL },
         { "65", NULL },
         { "70", NULL },
         { "75", NULL },
         { "80", NULL },
         { "85", NULL },
         { "90", NULL },
         { "95", NULL },
         { NULL, NULL },
      },
      "60"
   },
   {
      "genesis_plus_gx_psg_preamp",
      "PSG Preamp Level",
      NULL,
      "Set the audio preamplifier level of the emulated SN76496 4-channel Programmable Sound Generator found in the SG-1000, Sega Mark III, Master System, Game Gear and Mega Drive/Genesis.",
      NULL,
      "audio",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { "105", NULL },
         { "110", NULL },
         { "115", NULL },
         { "120", NULL },
         { "125", NULL },
         { "130", NULL },
         { "135", NULL },
         { "140", NULL },
         { "145", NULL },
         { "150", NULL },
         { "155", NULL },
         { "160", NULL },
         { "165", NULL },
         { "170", NULL },
         { "175", NULL },
         { "180", NULL },
         { "185", NULL },
         { "190", NULL },
         { "195", NULL },
         { "200", NULL },
         { NULL, NULL },
      },
      "150"
   },
   {
      "genesis_plus_gx_fm_preamp",
      "FM Preamp Level",
      NULL,
      "Set the audio preamplifier level of the emulated Mega Drive/Genesis FM sound synthetizer or Sega Mark III/Master System FM Sound Unit.",
      NULL,
      "audio",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { "105", NULL },
         { "110", NULL },
         { "115", NULL },
         { "120", NULL },
         { "125", NULL },
         { "130", NULL },
         { "135", NULL },
         { "140", NULL },
         { "145", NULL },
         { "150", NULL },
         { "155", NULL },
         { "160", NULL },
         { "165", NULL },
         { "170", NULL },
         { "175", NULL },
         { "180", NULL },
         { "185", NULL },
         { "190", NULL },
         { "195", NULL },
         { "200", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_cdda_volume",
      "CD-DA Volume",
      NULL,
      "Adjust the mixing volume of the emulated CD audio playback output.",
      NULL,
      "audio",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_pcm_volume",
      "PCM Volume",
      NULL,
      "Adjust the mixing volume of the emulated Sega CD / Mega CD RF5C164 PCM sound generator output.",
      NULL,
      "audio",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
#ifdef HAVE_EQ
   {
      "genesis_plus_gx_audio_eq_low",
      "EQ Low",
      NULL,
      "Adjust the low range band of the internal audio equaliser.",
      NULL,
      "audio",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_eq_mid",
      "EQ Mid",
      NULL,
      "Adjust the middle range band of the internal audio equaliser.",
      NULL,
      "audio",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_eq_high",
      "EQ High",
      NULL,
      "Adjust the high range band of the internal audio equaliser.",
      NULL,
      "audio",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
#endif
   {
      "genesis_plus_gx_gun_input",
      "Light Gun Input",
      NULL,
      "Use a mouse-controlled 'Light Gun' or 'Touchscreen' input.",
      NULL,
      "input",
      {
         { "lightgun",    "Light Gun" },
         { "touchscreen", "Touchscreen" },
         { NULL, NULL },
      },
      "lightgun"
   },
   {
      "genesis_plus_gx_gun_cursor",
      "Show Light Gun Crosshair",
      NULL,
      "Display light gun crosshairs when using the 'MD Menacer', 'MD Justifiers' and 'MS Light Phaser' input device types.",
      NULL,
      "input",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_invert_mouse",
      "Invert Mouse Y-Axis",
      NULL,
      "Inverts the Y-axis of the 'MD Mouse' input device type.",
      NULL,
      "input",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_no_sprite_limit",
      "Remove Per-Line Sprite Limit",
      NULL,
      "Removes the original sprite-per-scanline hardware limit. This reduces flickering but can cause visual glitches, as some games exploit the hardware limit to generate special effects.",
      NULL,
      "hacks",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_enhanced_vscroll",
      "Enchanced per-tile vertical scroll",
      NULL,
      "Allows each individual cell to be scrolled vertically, instead of 16px 2-cell, by averaging out with the vscroll value of the neighbouring cell. This hack only applies to few games that use 2-cell vertical scroll mode.",
      NULL,
      "hacks",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_enhanced_vscroll_limit",
      "Enchanced per-tile vertical scroll limit",
      NULL,
      "Only when Enchance per-tile vertical scroll is enabled. Adjusts the limit of the vertical scroll enhancement. When the vscroll difference between neighbouring tiles is bigger than this limit, the enhancement is disabled.",
      NULL,
      "hacks",
      {
         { "2", NULL },
         { "3",  NULL },
         { "4", NULL },
         { "5",  NULL },
         { "6", NULL },
         { "7",  NULL },
         { "8", NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11",  NULL },
         { "12", NULL },
         { "13",  NULL },
         { "14", NULL },
         { "15",  NULL },
         { "16", NULL },
         { NULL, NULL },
      },
      "8"
   },
#ifdef HAVE_OVERCLOCK
   {
      "genesis_plus_gx_overclock",
      "CPU Speed",
      NULL,
      "Overclock the emulated CPU. Can reduce slowdown, but may cause glitches.",
      NULL,
      "hacks",
      {
         { "100%", NULL },
         { "125%", NULL },
         { "150%", NULL },
         { "175%", NULL },
         { "200%", NULL },
         { NULL, NULL },
      },
      "100%"
   },
#endif
   {
      "genesis_plus_gx_force_dtack",
      "System Lock-Ups",
      NULL,
      "Emulate system lock-ups that occur on real hardware when performing illegal address access. This should only be disabled when playing certain demos and homebrew that rely on illegal behaviour for correct operation.",
      NULL,
      "hacks",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "genesis_plus_gx_addr_error",
      "68K Address Error",
      NULL,
      "The Mega Drive/Genesis Main CPU (Motorola 68000) generates an Address Error exception (crash) when attempting to perform unaligned memory access. Enabling '68K Address Error' simulates this behaviour. It should only be disabled when playing ROM hacks, since these are typically developed using less accurate emulators and may rely on invalid RAM access for correct operation.",
      NULL,
      "hacks",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "genesis_plus_gx_cd_latency",
      "CD access time",
      NULL,
        "Simulate original CD hardware latency when initiating a read or seeking to a specific location on loaded disc. This is required by a few CD games that crash if CD data is available too soon and also fixes CD audio desync issues in some games. Disabling this can be useful with MSU-MD games as it makes CD audio tracks loops more seamless.",
      NULL,
      "hacks",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
#ifdef USE_PER_SOUND_CHANNELS_CONFIG
   {
      "genesis_plus_gx_show_advanced_audio_settings",
      "Show Advanced Audio Volume Settings (Reopen menu)",
      NULL,
      "Enable configuration of low-level audio channel parameters. NOTE: Quick Menu must be toggled for this setting to take effect.",
      NULL,
      "channel_volume",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "genesis_plus_gx_psg_channel_0_volume",
      "PSG Tone Channel 0 Volume %",
      NULL,
      "Reduce the volume of the PSG Tone Channel 0.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_psg_channel_1_volume",
      "PSG Tone Channel 1 Volume %",
      NULL,
      "Reduce the volume of the PSG Tone Channel 1.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_psg_channel_2_volume",
      "PSG Tone Channel 2 Volume %",
      NULL,
      "Reduce the volume of the PSG Tone Channel 2.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_psg_channel_3_volume",
      "PSG Noise Channel 3 Volume %",
      NULL,
      "Reduce the volume of the PSG Noise Channel 3.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_md_channel_0_volume",
      "Mega Drive / Genesis FM Channel 0 Volume %",
      NULL,
      "Reduce the volume of the Mega Drive / Genesis FM Channel 0. Only works with MAME FM emulators.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_md_channel_1_volume",
      "Mega Drive / Genesis FM Channel 1 Volume %",
      NULL,
      "Reduce the volume of the Mega Drive / Genesis FM Channel 1. Only works with MAME FM emulators.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_md_channel_2_volume",
      "Mega Drive / Genesis FM Channel 2 Volume %",
      NULL,
      "Reduce the volume of the Mega Drive / Genesis FM Channel 2. Only works with MAME FM emulators.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_md_channel_3_volume",
      "Mega Drive / Genesis FM Channel 3 Volume %",
      NULL,
      "Reduce the volume of the Mega Drive / Genesis FM Channel 3. Only works with MAME FM emulators.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_md_channel_4_volume",
      "Mega Drive / Genesis FM Channel 4 Volume %",
      NULL,
      "Reduce the volume of the Mega Drive / Genesis FM Channel 4. Only works with MAME FM emulators.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_md_channel_5_volume",
      "Mega Drive / Genesis FM Channel 5 Volume %",
      NULL,
      "Reduce the volume of the Mega Drive / Genesis FM Channel 5. Only works with MAME FM emulators.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_0_volume",
      "Master System FM (YM2413) Channel 0 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 0.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_1_volume",
      "Master System FM (YM2413) Channel 1 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 1.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_2_volume",
      "Master System FM (YM2413) Channel 2 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 2.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_3_volume",
      "Master System FM (YM2413) Channel 3 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 3.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_4_volume",
      "Master System FM (YM2413) Channel 4 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 4.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_5_volume",
      "Master System FM (YM2413) Channel 5 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 5.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_6_volume",
      "Master System FM (YM2413) Channel 6 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 6.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_7_volume",
      "Master System FM (YM2413) Channel 7 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 7.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_sms_fm_channel_8_volume",
      "Master System FM (YM2413) Channel 8 Volume %",
      NULL,
      "Reduce the volume of the Master System FM Channel 8.",
      NULL,
      "channel_volume",
      {
         { "0",   NULL },
         { "10",  NULL },
         { "20",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
#endif
   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us,    /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   &options_pt_br, /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   &options_tr,    /* RETRO_LANGUAGE_TURKISH */
   NULL,           /* RETRO_LANGUAGE_SLOVAK */
   NULL,           /* RETRO_LANGUAGE_PERSIAN */
   NULL,           /* RETRO_LANGUAGE_HEBREW */
   NULL,           /* RETRO_LANGUAGE_ASTURIAN */
   NULL,           /* RETRO_LANGUAGE_FINNISH */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

INLINE void libretro_set_core_options(retro_environment_t environ_cb,
      bool *categories_supported)
{
   unsigned version  = 0;
#ifndef HAVE_NO_LANGEXTRA
   unsigned language = 0;
#endif

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_v2_intl core_options_intl;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL,
            &core_options_intl);
#else
      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
            &options_us);
#endif
   }
   else
   {
      size_t i, j;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_core_option_definition
            *option_v1_defs_us         = NULL;
#ifndef HAVE_NO_LANGEXTRA
      size_t num_options_intl          = 0;
      struct retro_core_option_v2_definition
            *option_defs_intl          = NULL;
      struct retro_core_option_definition
            *option_v1_defs_intl       = NULL;
      struct retro_core_options_intl
            core_options_v1_intl;
#endif
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine total number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      if (version >= 1)
      {
         /* Allocate US array */
         option_v1_defs_us = (struct retro_core_option_definition *)
               calloc(num_options + 1, sizeof(struct retro_core_option_definition));

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
            struct retro_core_option_value *option_values         = option_def_us->values;
            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

            option_v1_def_us->key           = option_def_us->key;
            option_v1_def_us->desc          = option_def_us->desc;
            option_v1_def_us->info          = option_def_us->info;
            option_v1_def_us->default_value = option_def_us->default_value;

            /* Values must be copied individually... */
            while (option_values->value)
            {
               option_v1_values->value = option_values->value;
               option_v1_values->label = option_values->label;

               option_values++;
               option_v1_values++;
            }
         }

#ifndef HAVE_NO_LANGEXTRA
         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&
             options_intl[language])
            option_defs_intl = options_intl[language]->definitions;

         if (option_defs_intl)
         {
            /* Determine number of intl options */
            while (true)
            {
               if (option_defs_intl[num_options_intl].key)
                  num_options_intl++;
               else
                  break;
            }

            /* Allocate intl array */
            option_v1_defs_intl = (struct retro_core_option_definition *)
                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));

            /* Copy parameters from option_defs_intl array */
            for (i = 0; i < num_options_intl; i++)
            {
               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];
               struct retro_core_option_value *option_values           = option_def_intl->values;
               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];
               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;

               option_v1_def_intl->key           = option_def_intl->key;
               option_v1_def_intl->desc          = option_def_intl->desc;
               option_v1_def_intl->info          = option_def_intl->info;
               option_v1_def_intl->default_value = option_def_intl->default_value;

               /* Values must be copied individually... */
               while (option_values->value)
               {
                  option_v1_values->value = option_values->value;
                  option_v1_values->label = option_values->label;

                  option_values++;
                  option_v1_values++;
               }
            }
         }

         core_options_v1_intl.us    = option_v1_defs_us;
         core_options_v1_intl.local = option_v1_defs_intl;

         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);
#else
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
#endif
      }
      else
      {
         /* Allocate arrays */
         variables  = (struct retro_variable *)calloc(num_options + 1,
               sizeof(struct retro_variable));
         values_buf = (char **)calloc(num_options, sizeof(char *));

         if (!variables || !values_buf)
            goto error;

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            const char *key                        = option_defs_us[i].key;
            const char *desc                       = option_defs_us[i].desc;
            const char *default_value              = option_defs_us[i].default_value;
            struct retro_core_option_value *values = option_defs_us[i].values;
            size_t buf_len                         = 3;
            size_t default_index                   = 0;

            values_buf[i] = NULL;

            /* Skip options that are irrelevant when using the
             * old style core options interface */
            if (strcmp(key, "genesis_plus_gx_show_advanced_audio_settings") == 0)
               continue;

            if (desc)
            {
               size_t num_values = 0;

               /* Determine number of values */
               while (true)
               {
                  if (values[num_values].value)
                  {
                     /* Check if this is the default value */
                     if (default_value)
                        if (strcmp(values[num_values].value, default_value) == 0)
                           default_index = num_values;

                     buf_len += strlen(values[num_values].value);
                     num_values++;
                  }
                  else
                     break;
               }

               /* Build values string */
               if (num_values > 0)
               {
                  buf_len += num_values - 1;
                  buf_len += strlen(desc);

                  values_buf[i] = (char *)calloc(buf_len, sizeof(char));
                  if (!values_buf[i])
                     goto error;

                  strcpy(values_buf[i], desc);
                  strcat(values_buf[i], "; ");

                  /* Default value goes first */
                  strcat(values_buf[i], values[default_index].value);

                  /* Add remaining values */
                  for (j = 0; j < num_values; j++)
                  {
                     if (j != default_index)
                     {
                        strcat(values_buf[i], "|");
                        strcat(values_buf[i], values[j].value);
                     }
                  }
               }
            }

            variables[option_index].key   = key;
            variables[option_index].value = values_buf[i];
            option_index++;
         }

         /* Set variables */
         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
      }

error:
      /* Clean up */

      if (option_v1_defs_us)
      {
         free(option_v1_defs_us);
         option_v1_defs_us = NULL;
      }

#ifndef HAVE_NO_LANGEXTRA
      if (option_v1_defs_intl)
      {
         free(option_v1_defs_intl);
         option_v1_defs_intl = NULL;
      }
#endif

      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
