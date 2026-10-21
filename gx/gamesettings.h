/****************************************************************************
 *  gamesettings.h
 *
 *  Settings of one game: the video, audio and system options can be kept for
 *  a game (Settings > Per-Game). They are stored next to the game's saves as
 *  saves/<system>/<game>.cfg and replace the global options while it is
 *  loaded; config.ini keeps the global ones.
 *
 *  Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX
 *  (see LICENSE.txt).
 ****************************************************************************/

#ifndef _GAMESETTINGS_H_
#define _GAMESETTINGS_H_

/* the loaded game uses its own settings */
extern int gamesettings_active(void);

/* after a game is loaded (rom_filename set), before the system starts: puts
   back the global options, then the game's own ones if it has them */
extern void gamesettings_load(void);

/* keeps the current options for the loaded game from now on */
extern int gamesettings_enable(void);

/* the loaded game uses the global options again; its file is deleted */
extern void gamesettings_disable(void);

/* writes the game's file when it uses its own settings */
extern void gamesettings_save(void);

/* around writing config.ini: the global values of the per-game options are
   put in config, then the game's values back */
extern void gamesettings_begin_global_save(void);
extern void gamesettings_end_global_save(void);

#endif
