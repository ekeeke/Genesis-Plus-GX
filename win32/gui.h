/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  gui.h -- declarations shared between the frontend modules. The emulator
 *           core never sees this file.
 ****************************************************************************/

#ifndef _GUI_H_
#define _GUI_H_

#include <windows.h>

#define APP_NAME    "Genesis Plus GX"
#define APP_CLASS   "GenesisPlusGXWindow"

/* Largest frame the core can produce, and the pixel format it renders in. */
#define FRAME_MAX_W 720
#define FRAME_MAX_H 576
#define FRAME_BPP   2

/****************************************************************************
 * main.c
 ****************************************************************************/

extern HWND      g_hwnd;
extern HWND      g_status;
extern HFONT     gui_get_ui_font(void);
extern HINSTANCE g_inst;

extern int emu_running;   /* a ROM is loaded */
extern const char *emu_rom_filename(void);
extern const char *emu_rom_path(void);
extern int emu_paused;    /* emulation is suspended */

extern int  emu_load_rom(const char *path);
extern int  emu_peek_rom(const char *path);
extern void emu_close_rom(void);
extern double emu_frame_rate(void);      /* real frame rate of the emulated console */
extern const char *emu_rom_base(void);   /* file name of the loaded game, no folder or extension */
extern void emu_reset(int hard);
extern void emu_save_state(int slot);
extern void emu_load_state(int slot);
extern void emu_undo_load_state(void);   /* back to the game as it was before the last load */
extern int  emu_can_undo_load(void);
extern void gui_snprintf(char *out, int out_len, const char *fmt, ...);   /* never overruns out */
extern void emu_apply_region(void);

/* Per-game data files (battery saves, save states, cheats) live in
   <kind>\<Console>\<game name> [<ROM id>]<ext>, e.g.
   states\MegaDrive\Sonic The Hedgehog [A1B2C3D4].gp0
   The console folder groups them; the 8-digit id (a hash of the game's data)
   keeps two different games with the same file name apart.
   emu_data_path(): for_read=0 gives the path a file should be written to;
   for_read=1 gives the path of the existing file to read -- the new name if
   there is one, else the same game under another console folder, else the
   old flat name from earlier versions, else the new name. */
extern void emu_data_path(const char *kind, const char *ext, int for_read, char *out, int out_len);
extern void emu_ensure_data_dir(const char *kind);
extern void state_delete(int slot);
extern void emu_apply_audio_settings(void);

/* Writes to the status bar's message pane. */
extern void gui_status(const char *fmt, ...);
extern void gui_status_persistent(const char *fmt, ...);
extern void gui_status_slot(const char *text);

/* Shows a short overlay message on the video output for a couple of seconds. */
extern void gui_notify(const char *fmt, ...);

extern void gui_update_menu(void);
extern void gui_resize_to_scale(int scale);
extern void apply_always_on_top(void);

/****************************************************************************
 * video.c
 ****************************************************************************/

extern int   video_init(void);
extern void  video_shutdown(void);
extern void  video_frame(void);          /* blit the current core bitmap */
extern void  video_repaint(HDC hdc);     /* redraw from WM_PAINT */
extern void  video_set_ntsc(int mode);   /* 0 off, 1 composite, 2 s-video, 3 rgb */
extern void  video_viewport_changed(void);
extern void  video_force_redraw(void);
extern void  video_set_renderer(int renderer);
extern void  video_set_vsync(int on);
extern void  video_set_vsync_allowed(int allowed);   /* false: skip this frame's VSync wait */
extern void  video_invalidate(void);
extern void  video_preferred_size(int scale, int *w, int *h);
extern int   video_screenshot(char *path_out, int path_len);
extern int   video_save_thumbnail(const char *path);
extern HBITMAP video_load_thumbnail(const char *path);
extern void  state_path(int slot, char *out, int out_len);
extern void  thumb_path(int slot, char *out, int out_len);
/* "hh:mm:ss  DD/MM/YYYY" (local time) of a state file's last save; 0 if it doesn't exist. */
extern int   state_file_time(const char *path, char *out, int out_len);
extern void  video_show_notice(const char *text, int ms);
extern void  video_get_output_rect(RECT *dest, int *src_w, int *src_h);
extern void  video_set_fullscreen(int on);
extern void  video_report_fps(int fps);

/* Built-in render filters (filters.c). Filters are addressed by index into a
   fixed table; -1 means "no filter". */
extern int   video_filter_count(void);
extern const char *video_filter_name(int index);
extern int   video_filter_separator_before(int index);      /* 1 = start a new menu group here */
extern int   video_filter_current(void);                   /* index, or -1 */
extern void  video_set_filter(int index);
extern int   video_set_filter_by_name(const char *name);   /* 1 if it matched */

/****************************************************************************
 * audio.c
 ****************************************************************************/

extern int  waveout_open(int sample_rate);
extern void waveout_close(void);
extern void waveout_submit(const short *samples, int frames);
extern int  waveout_pending(void);       /* buffers still queued on the device */
extern int  waveout_broken(void);        /* device closed or refusing data */
extern int  waveout_stalled(void);       /* queued but nothing finishing (about 1/s while true) */
extern void waveout_flush(void);
extern void waveout_set_volume(int percent);
extern int  waveout_rate(void);

/****************************************************************************
 * input.c
 ****************************************************************************/

extern void gui_input_init(void);
extern void gui_input_shutdown(void);
extern void gui_input_set_focus(int focused);
extern int  gui_input_fast_forward(void);
extern int  gui_input_rewind(void);
extern int  gui_input_frame_advance(void);
extern int  gui_input_p2_slot(void);   /* -1 unless both ports have a plain control pad */
extern int  gui_input_save_slot_shortcut(void);
extern int  gui_input_load_slot_shortcut(void);

extern int  input_capture_key(void);
extern int  input_capture_gamepad(int device);
extern int  input_any_input_down(int device);

extern const char *input_key_name(int vk);
extern const char *input_pad_button_name(int mask);
extern const char *input_button_label(int index);

/****************************************************************************
 * cheats.c
 ****************************************************************************/

#define CHEAT_MAX 150

extern void cheats_apply(void);
extern void cheats_suspend(void);
extern void cheats_ram_update(void);
extern void ROMCheatUpdate(void);          /* called by the core on rebanking */

extern int  cheats_add(const char *code, const char *desc);
extern void cheats_remove(int index);
extern void cheats_remove_all(void);
extern void cheats_set_enabled(int index, int on);

extern int  cheats_count(void);
extern int  cheats_active(void);
extern int  cheats_effective(void);        /* 0 while Enable Cheats is off */
extern void cheats_set_all_enabled(int on);
extern int  cheats_is_enabled(int index);
extern int  cheats_is_valid(int index);
extern const char *cheats_get_code(int index);
extern const char *cheats_get_desc(int index);

extern void cheats_load_for_rom(const char *rom_base);
extern void cheats_save_for_rom(const char *rom_base);

/* Saves against whatever ROM is loaded; implemented in main.c. */
extern void cheats_save_current(void);

extern void dlg_cheats(HWND parent);

/****************************************************************************
 * browser.c
 ****************************************************************************/

extern void browser_panel_create(HWND parent);
extern void browser_panel_show(int show);
extern int  browser_panel_visible(void);
extern void browser_panel_layout(const RECT *area);
extern void browser_grid_zoom(int delta);
extern void browser_relayout(void);   /* implemented in main.c */
extern int  browser_panel_handle_command(WORD id, WORD notify_code);
extern int  browser_panel_handle_notify(NMHDR *hdr);
extern int  browser_panel_handle_return(void);
extern int  browser_panel_handle_contextmenu(HWND target, int x, int y);
extern HBRUSH browser_panel_ctlcolor(HWND ctrl, HDC hdc);
extern void browser_panel_change_folder(void);
extern void browser_apply_view_mode(void);

/****************************************************************************
 * rewind.c
 ****************************************************************************/

extern void rewind_init(void);
extern void rewind_reset(void);
extern void rewind_capture_tick(const int16 *audio_samples, int audio_frames);
extern int  rewind_available(void);
extern void rewind_enable(int on);   /* allocates / frees the history (about 200 MB) */
extern int  rewind_ready(void);      /* the history exists (enable succeeded) */
extern int  rewind_step(void);
extern const int16 *rewind_get_audio(int *out_frames);

/****************************************************************************
 * dialogs.c
 ****************************************************************************/

extern void dlg_about(HWND parent);
extern void dlg_audio_advanced(HWND parent);
extern void emu_apply_fm_settings(void);      /* re-create the FM chip after its type changed */
extern void emu_apply_nuked_type(void);
extern void emu_apply_sms_border(void);
extern const char *browser_console_for_path(const char *full_path);   /* NULL if unknown */
extern void dlg_netplay(HWND parent);

/* Fullscreen hides the mouse cursor with a ShowCursor() counter, which would
   also hide it inside any dialog opened meanwhile (e.g. Ctrl+C for cheats).
   gui_dialog_box() shows it for the dialog's lifetime and puts it back. */
extern INT_PTR gui_dialog_box(int template_id, HWND parent, DLGPROC proc);
extern int     gui_cursor_show(void);
extern void    gui_cursor_restore(int hidden_count);

/* Open-file picker (see dialogs.c). `which` selects which remembered-state
   slot the dialog uses so the pickers never share state. */
#define GUI_PICK_ROM    0
#define GUI_PICK_CHEAT  1
#define GUI_PICK_COVER  2
#define GUI_PICK_COUNT  3
extern int gui_pick_file(HWND owner, int which, const char *title, const char *filter,
                         const char *start_dir, char *out, int out_len);
extern void dlg_state_manager(HWND parent);
extern void dlg_input(HWND parent, int player);
extern void dlg_audio(HWND parent);
extern void dlg_shortcuts(HWND parent);
extern void dlg_menu_guide(HWND parent);
extern void dlg_rom_info(HWND parent);

#endif /* _GUI_H_ */
