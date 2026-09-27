/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  theme.h -- dark/light mode for the app's own chrome (menus, dialogs,
 *  buttons, the status bar). Follows Windows' own system-wide dark/light
 *  setting by default, with a manual override (Auto/Light/Dark) available
 *  from the Video menu.
 *
 *  This only affects the UI chrome -- the emulated picture itself is
 *  unaffected either way, since that's the game's own rendered output,
 *  not a themed control.
 *
 *  Windows' native controls have no public, documented API for dark
 *  mode -- what's here uses the same handful of undocumented uxtheme.dll
 *  entry points (looked up by ordinal, since they were never given
 *  exported names) that Notepad++, and Windows' own Explorer and
 *  Notepad, use for exactly this. They've been stable since Windows 10
 *  1809 (2018) and are a well-documented reverse-engineering target
 *  (see the "win32-darkmode" project on GitHub), but being undocumented,
 *  Microsoft could change or remove them in a future update without
 *  notice -- if that ever happens, theme_init() simply fails its
 *  GetProcAddress lookups and the app carries on in ordinary light mode,
 *  rather than crashing.
 ****************************************************************************/

#ifndef THEME_H
#define THEME_H

enum { THEME_AUTO = 0, THEME_LIGHT = 1, THEME_DARK = 2 };

/* Call once at startup, after the main window exists. Detects the
   current Windows setting and loads the uxtheme.dll entry points --
   safe to call even on Windows versions that don't have them (or too
   old to have dark mode at all); theme_is_dark() just always reports
   light in that case. */
void theme_init(void);

/* THEME_AUTO/LIGHT/DARK. Persists via config. Doesn't re-apply anything
   by itself -- call theme_apply_to_window() separately afterward (the
   caller already has whichever HWND makes sense to re-apply to). */
void theme_set_mode(int mode);
int  theme_get_mode(void);

/* The effective state right now -- resolves THEME_AUTO against the last
   system reading. */
int theme_is_dark(void);

/* Dark-mode-ifies a single window and every one of its child controls
   (buttons, edits, listboxes, comboboxes, static text) recursively.
   Call once for the main window after it's created, and once more from
   each dialog's WM_INITDIALOG -- dialogs aren't children of the main
   window in the relevant sense, so each needs its own call. Safe (a
   no-op) when the effective theme is light. */
void theme_apply_to_window(HWND hwnd);

/* Call from WM_SETTINGCHANGE. Re-reads the system setting; if the mode
   is THEME_AUTO and the system setting actually changed, re-applies to
   `hwnd` and returns 1 (caller should also invalidate/redraw it). No-op,
   returning 0, if the mode is a manual override or nothing changed. */
int theme_handle_settingchange(HWND hwnd);

/* Call at the top of a WM_CTLCOLOREDIT/WM_CTLCOLORLISTBOX/
   WM_CTLCOLORSTATIC/WM_CTLCOLORBTN handler, passing the HDC from wParam.
   Sets dark background/text colors into it and returns the brush to
   return from the handler -- or NULL if the effective theme is light,
   meaning the caller should fall through to its own default handling
   instead. */
HBRUSH theme_ctlcolor(HDC hdc);

/* cache_id: a small, distinct integer (0-3) per call site, so each
   custom color gets cached separately rather than sharing one brush. */
HBRUSH theme_ctlcolor_custom(HDC hdc, COLORREF bg, int cache_id);

/* Call from WM_NOTIFY, passing the NMHDR from lParam and the status
   bar's own HWND. Handles NM_CUSTOMDRAW to set dark-mode text color
   (SB_SETBKCOLOR, applied in theme_apply_to_window(), only reaches the
   background -- the text itself still needs this separate hook).
   Returns the LRESULT to return from WM_NOTIFY if this notification was
   for the status bar and was handled, or -1 if it wasn't (caller should
   fall through to its own handling in that case). */
LRESULT theme_statusbar_customdraw(NMHDR *hdr, HWND status_bar);

/* Call from WM_NOTIFY the same way as theme_statusbar_customdraw(), but
   without needing the specific header's HWND -- it's identified here by
   class name (SysHeader32) instead, since a ListView's header isn't a
   handle any caller would otherwise have on hand. Needed because the
   "ItemsView" theme applied to it elsewhere gets its background dark
   correctly, but not its text color -- headers have no documented
   message for that, same as the status bar. Returns -1 if this
   notification wasn't for a header at all, so callers can chain
   multiple *_customdraw checks in sequence. */
LRESULT theme_header_customdraw(NMHDR *hdr);

/* Call from the main window's WM_UAHDRAWMENU/WM_UAHDRAWMENUITEM cases
   (message numbers 0x0091/0x0092 -- undocumented, no public constant
   name exists for them). Returns 1 if handled (caller should return 0
   from the message in that case) or 0 if not (light mode, or anything
   about the message looked wrong) -- caller should fall through to
   DefWindowProc in that case. */
int theme_draw_menu_bar(HWND hwnd, LPARAM lp);
int theme_draw_menu_item(LPARAM lp);

/* Converts every item in this menu, and every submenu it contains, to
   MF_OWNERDRAW -- call once at startup, only when gui.large_ui is on.
   Standard, documented mechanism (unlike the WM_UAHDRAWMENU family
   above), used specifically because that family's attempt at resizing
   the menu broke it entirely on real Windows. */
void theme_ownerdraw_menu(HMENU menu);

/* Call from the main window's WM_MEASUREITEM/WM_DRAWITEM handlers.
   Both check CtlType == ODT_MENU themselves and return 0 (unhandled)
   for anything else, so they're safe to call unconditionally from
   those handlers even if something other than a menu ever ends up
   owner-drawn in this app. */
int theme_measure_menu_ownerdraw(LPARAM lp);
int theme_draw_menu_ownerdraw(LPARAM lp);

#endif
