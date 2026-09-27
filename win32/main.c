/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  main.c -- window, menu handling, ROM lifecycle and the emulation loop.
 *
 *  Emulation runs on the UI thread from a PeekMessage loop rather than on a
 *  worker thread. That trades a little responsiveness while a menu is open
 *  (Windows runs its own modal loop there, which we handle explicitly) for
 *  having no locking anywhere around the framebuffer or the core's state.
 ****************************************************************************/

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>

#include "shared.h"
#include "blip_buf.h"
#include "gui.h"
#include "resource.h"
#include "theme.h"
#include "recorder.h"
#include "netplay.h"

/* Required by main.h / the core. */
int log_error = 0;
int debug_on  = 0;

HWND      g_hwnd;
HWND      g_status;
HINSTANCE g_inst;

int emu_running;
int emu_paused;

/* Shared UI font -- the normal system default everywhere, or (when
   gui.large_ui is on) a version scaled up ~17.5% -- the middle of the
   "at least 50%" scale asked for, applied uniformly rather than
   guessing a different amount per control. Created once, lazily, and
   kept for the app's lifetime rather than per-control, since every
   caller wants the exact same font. */
HFONT gui_get_ui_font(void)
{
  static HFONT large_font;
  HFONT stock = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

  if (!gui.large_ui) return stock;

  if (!large_font)
  {
    LOGFONTA lf;
    GetObjectA(stock, sizeof(lf), &lf);
    /* lfHeight is conventionally negative (character height, not cell
       height) -- scaling while preserving sign keeps this correct
       regardless of which convention the stock font happens to use. */
    lf.lfHeight = (LONG)(lf.lfHeight * 1.5);
    large_font = CreateFontIndirectA(&lf);
    if (!large_font) large_font = stock;
  }

  return large_font;
}

/* Set only when emu_paused was switched on by losing focus, not by the
   person pressing pause themselves -- distinguishes "resume automatically
   when focus comes back" from "leave a deliberate pause alone". */
static int auto_paused_by_focus;
static int auto_paused_by_minimize;

static HACCEL g_accel;
static HMENU  g_menu;

static char rom_path[GUI_PATH_LEN];

/*
 * Filename without directory or extension. Deliberately much shorter than a
 * full path: it gets formatted into window titles, status text and save-file
 * names, and wvsprintf has a hard 1024-byte output limit.
 */
static char rom_base[128];

static int16 soundframe[4096];

static int   in_modal_loop;
static int   frames_this_second;
static DWORD fps_tick;
static LARGE_INTEGER perf_freq;
static double next_frame_time;
static double next_rewind_time;
static int    rewind_hit_limit_notified;

/* Mega CD backup RAM header, used to tell formatted RAM from blank RAM. */
static uint8 brm_format[0x40] =
{
  0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x00,0x00,0x00,0x00,0x40,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x53,0x45,0x47,0x41,0x5f,0x43,0x44,0x5f,0x52,0x4f,0x4d,0x00,0x01,0x00,0x00,0x00,
  0x52,0x41,0x4d,0x5f,0x43,0x41,0x52,0x54,0x52,0x49,0x44,0x47,0x45,0x5f,0x5f,0x5f
};

static const uint16 vc_table[4][2] =
{
  /* NTSC, PAL */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0xEA , 0x102}, /* Mode 5 (224 lines) */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0x106, 0x10A}  /* Mode 5 (240 lines) */
};

/****************************************************************************
 * Paths
 ****************************************************************************/

/* wsprintf with an output size: wsprintfA has none and would write past a
   path buffer when the folders are deep or the names long. */
void gui_snprintf(char *out, int out_len, const char *fmt, ...)
{
  char buf[1024];        /* wvsprintf's own limit */
  va_list ap;

  va_start(ap, fmt);
  wvsprintfA(buf, fmt, ap);
  va_end(ap);
  lstrcpynA(out, buf, out_len);
}

char *osd_path(const char *filename)
{
  /*
   * Rotating buffers: the core occasionally has one path in flight while
   * building the next, and a single static buffer would corrupt both.
   */
  static char slots[4][GUI_PATH_LEN];
  static int  next;
  static char base[GUI_PATH_LEN];
  static int  base_ready;

  char *out = slots[next];
  next = (next + 1) & 3;

  if (!base_ready)
  {
    char *slash;
    DWORD len = GetModuleFileNameA(NULL, base, sizeof(base) - 1);

    if (len == 0 || len >= sizeof(base) - 1)
    {
      base[0] = '.';
      base[1] = '\0';
    }
    else
    {
      slash = strrchr(base, '\\');
      if (slash) *slash = '\0';
      else lstrcpyA(base, ".");
    }
    base_ready = 1;
  }

  gui_snprintf(out, GUI_PATH_LEN, "%s\\%s", base, filename);
  return out;
}

static void ensure_dir(const char *name)
{
  CreateDirectoryA(osd_path(name), NULL);
}

static void split_basename(const char *path, char *out, int out_len)
{
  const char *start = path;
  const char *slash = strrchr(path, '\\');
  const char *fwd   = strrchr(path, '/');
  const char *dot;
  int n;

  if (fwd > slash) slash = fwd;
  if (slash) start = slash + 1;

  dot = strrchr(start, '.');
  n = dot ? (int)(dot - start) : (int)strlen(start);
  if (n >= out_len) n = out_len - 1;

  memcpy(out, start, (size_t)n);
  out[n] = '\0';
}

/****************************************************************************
 * Status bar and notices
 ****************************************************************************/

static char status_persistent[128];
static void record_stop(void);   /* defined with the frame loop below */

/* Netplay: game data saved during a session belongs to a made-up start state
   (fresh backup RAM on both sides), so it is never written over the player's
   real saves. Cleared when a game is loaded or hard-reset from the real saves. */
static int np_no_save;
static int np_rom_sha_ready;
static unsigned char np_rom_sha[32];

/* Pane 0 while something is going on that the person should always see:
   "[Netplay: ...] [REC 00:12] Running <game>". */
static void status_apply_prefixes(void)
{
  char buf[400];

  if (!g_status) return;
  buf[0] = '\0';

  if (netplay_state() != NP_IDLE)
  {
    lstrcatA(buf, "[");
    lstrcatA(buf, netplay_status_text());
    lstrcatA(buf, "] ");
  }
  if (recorder_active())
  {
    unsigned s = recorder_seconds();
    char t[40];

    if (s >= 3600) wsprintfA(t, "[REC %u:%02u:%02u] ", s / 3600, (s / 60) % 60, s % 60);
    else           wsprintfA(t, "[REC %02u:%02u] ", s / 60, s % 60);
    lstrcatA(buf, t);
  }
  lstrcatA(buf, status_persistent);
  SendMessageA(g_status, SB_SETTEXTA, 0, (LPARAM)buf);
}
static DWORD status_transient_until;

/* The baseline message pane 0 should show once any transient message
   above it clears -- "Running <game>" or the browsing prompt. */
void gui_status_persistent(const char *fmt, ...)
{
  char buf[1024];        /* wvsprintf's own limit */
  va_list ap;

  va_start(ap, fmt);
  wvsprintfA(buf, fmt, ap);
  va_end(ap);
  lstrcpynA(status_persistent, buf, sizeof(status_persistent));   /* never overruns */

  status_transient_until = 0;
  if (g_status) SendMessageA(g_status, SB_SETTEXTA, 0, (LPARAM)status_persistent);
}

void gui_status(const char *fmt, ...)
{
  char buf[1024];
  va_list ap;

  va_start(ap, fmt);
  wvsprintfA(buf, fmt, ap);
  va_end(ap);

  /* Transient -- reverts to the persistent baseline on its own after a
     few seconds instead of sitting there until something else happens
     to overwrite it, which was the actual bug: there was no revert path
     at all, so whatever the last one-off message was (e.g. "Audio
     settings saved") just stayed there indefinitely. */
  status_transient_until = GetTickCount() + 4000;
  if (g_status) SendMessageA(g_status, SB_SETTEXTA, 0, (LPARAM)buf);
}

void gui_notify(const char *fmt, ...)
{
  char full[1024];       /* wvsprintf's own limit */
  char buf[256];
  va_list ap;

  va_start(ap, fmt);
  wvsprintfA(full, fmt, ap);
  va_end(ap);
  lstrcpynA(buf, full, sizeof(buf));   /* long names are cut, never overrun */

  video_show_notice(buf, 1800);
  gui_status("%s", buf);
}

static void status_set_slot(void)
{
  char buf[32];
  wsprintfA(buf, "Slot %d", gui.state_slot);
  if (g_status) SendMessageA(g_status, SB_SETTEXTA, 1, (LPARAM)buf);
}

/* Lets the browser panel show the game count in the same pane Slot N
   normally occupies -- the slot number means nothing without a ROM
   running anyway, so the two never need the space at the same time. */
void gui_status_slot(const char *text)
{
  if (g_status) SendMessageA(g_status, SB_SETTEXTA, 1, (LPARAM)text);
}

static void status_set_fps(int fps)
{
  static char last_buf[32];
  char buf[32];

  if (emu_running && !emu_paused) wsprintfA(buf, "%d fps", fps);
  else lstrcpyA(buf, emu_running ? "Paused" : "");

  if (lstrcmpA(buf, last_buf) == 0) return;
  lstrcpyA(last_buf, buf);

  if (g_status) SendMessageA(g_status, SB_SETTEXTA, 2, (LPARAM)buf);
}

/*
 * Remembers where the window is so it can reopen there next time. Skipped
 * during fullscreen (that geometry is a full-monitor rect, not a window
 * position) and while minimized (GetWindowRect would just capture the
 * taskbar-parked position, overwriting the last real one for no reason).
 */
static void capture_window_geometry(void)
{
  if (gui.fullscreen) return;
  if (IsIconic(g_hwnd)) return;

  gui.window_maximized = IsZoomed(g_hwnd) ? 1 : 0;

  if (!gui.window_maximized)
  {
    RECT r;
    GetWindowRect(g_hwnd, &r);
    gui.window_x = r.left;
    gui.window_y = r.top;
  }
}

static void layout_status(void)
{
  RECT rc;
  int parts[3];

  if (!g_status) return;

  SendMessage(g_status, WM_SIZE, 0, 0);
  GetClientRect(g_hwnd, &rc);

  /* Middle pane widened from its original "Slot N"-only sizing -- it now
     also carries the browser panel's game count, which runs noticeably
     longer ("15 of 934 games", "999+ games found (list capped)"). */
  parts[0] = rc.right - 220;
  parts[1] = rc.right - 70;
  parts[2] = -1;
  if (parts[0] < 0) parts[0] = 0;
  if (parts[1] < parts[0]) parts[1] = parts[0];

  SendMessage(g_status, SB_SETPARTS, 3, (LPARAM)parts);
}

/* Same area video.c's present() renders into -- client area minus the
   status bar, when it's visible. The browser panel occupies exactly this
   region so nothing black shows through around its edges. */
static void get_content_rect(RECT *out)
{
  GetClientRect(g_hwnd, out);

  if (g_status && IsWindowVisible(g_status))
  {
    RECT sb;
    GetWindowRect(g_status, &sb);
    out->bottom -= (sb.bottom - sb.top);
    if (out->bottom < out->top) out->bottom = out->top;
  }
}

/* Lets browser.c (which can't see the static get_content_rect above)
   trigger a relayout of its own controls -- needed after picking a folder
   from the "Choose ROM Folder" button, which switches the panel from that
   button to the normal search+list view without a window resize to
   otherwise trigger it. */
void browser_relayout(void)
{
  RECT content;
  get_content_rect(&content);
  browser_panel_layout(&content);
}

/****************************************************************************
 * Window title
 ****************************************************************************/

static void update_title(void)
{
  char title[512];

  if (emu_running)
  {
    const char *name = (rominfo.international[0] != 0x20 && rominfo.international[0])
                       ? rominfo.international : rominfo.domestic;

    if (name && name[0] && name[0] != 0x20)
    {
      char trimmed[64];
      int i;
      lstrcpynA(trimmed, name, sizeof(trimmed));
      for (i = lstrlenA(trimmed) - 1; i >= 0 && trimmed[i] == ' '; i--) trimmed[i] = '\0';
      wsprintfA(title, "%s - " APP_NAME, trimmed[0] ? trimmed : rom_base);
    }
    else
    {
      wsprintfA(title, "%s - " APP_NAME, rom_base);
    }
  }
  else
  {
    lstrcpyA(title, APP_NAME);
  }

  SetWindowTextA(g_hwnd, title);
}

/****************************************************************************
 * Menu state
 ****************************************************************************/

static void check_radio(HMENU menu, int base, int count, int selected)
{
  CheckMenuRadioItem(menu, base, base + count - 1, base + selected, MF_BYCOMMAND);
}

/* Empties a menu. With Larger UI the items are owner-drawn and each carries a
   malloc'd copy of its text in itemData (see theme.c), which has to be freed
   here or every rebuild leaks it. */
static void clear_menu(HMENU menu)
{
  while (GetMenuItemCount(menu) > 0)
  {
    MENUITEMINFOA mii;

    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask  = MIIM_FTYPE | MIIM_DATA;
    if (GetMenuItemInfoA(menu, 0, TRUE, &mii) && (mii.fType & MFT_OWNERDRAW) && mii.dwItemData)
      free((void *)mii.dwItemData);
    DeleteMenu(menu, 0, MF_BYPOSITION);
  }
}

static void rebuild_recent_menu(void)
{
  HMENU file_menu = GetSubMenu(g_menu, 0);
  /* Position 2: Open ROM (0), ROM Browser (1), Open Recent (2). */
  HMENU recent = GetSubMenu(file_menu, 2);
  int i, added = 0;

  if (!recent) return;

  clear_menu(recent);

  for (i = 0; i < GUI_RECENT_MAX; i++)
  {
    char label[GUI_PATH_LEN + 64];
    char name[GUI_PATH_LEN];
    const char *console;

    if (!gui.recent[i][0]) continue;

    split_basename(gui.recent[i], name, sizeof(name));

    /* "Sonic The Hedgehog - Mega Drive": the console's short name, i.e. the
       part before " / " ("Mega Drive / Genesis" -> "Mega Drive"). */
    console = browser_console_for_path(gui.recent[i]);
    if (console && console[0])
    {
      char shortname[48];
      char *slash;

      lstrcpynA(shortname, console, sizeof(shortname));
      slash = strstr(shortname, " / ");
      if (slash) *slash = '\0';
      wsprintfA(label, "&%d  %s - %s", added + 1, name, shortname);
    }
    else
    {
      wsprintfA(label, "&%d  %s", added + 1, name);
    }
    AppendMenuA(recent, MF_STRING, (UINT_PTR)(IDM_FILE_RECENT_BASE + i), label);
    added++;
  }

  if (!added)
  {
    AppendMenuA(recent, MF_STRING | MF_GRAYED, (UINT_PTR)IDM_FILE_RECENT_BASE, "(Nothing Yet)");
  }
  else
  {
    AppendMenuA(recent, MF_SEPARATOR, 0, NULL);
    AppendMenuA(recent, MF_STRING, (UINT_PTR)IDM_FILE_RECENT_CLEAR, "&Clear Recent Files");
  }

  if (gui.large_ui) theme_ownerdraw_menu(recent);

  DrawMenuBar(g_hwnd);
}

/* Room reserved for filter entries: IDM_VIDEO_FILTER_BASE .. +23. The menu
   command for entry i is IDM_VIDEO_FILTER_BASE + i, where i is the filter's
   index in filters.c's table. */
#define FILTER_MENU_MAX 24

/* Finds the "Video" top-level menu by what's actually in it (a known,
   always-direct-child item) rather than by position -- a hardcoded
   position here is exactly the same fragility find_render_filter_menu
   already works around one level down, and broke the same way when
   the View menu was inserted before Video, shifting its position. */
static HMENU find_video_menu(void)
{
  int i, count = GetMenuItemCount(g_menu);

  for (i = 0; i < count; i++)
  {
    HMENU sub = GetSubMenu(g_menu, i);
    if (sub && GetMenuState(sub, IDM_VIDEO_FULLSCREEN, MF_BYCOMMAND) != (UINT)-1) return sub;
  }
  return NULL;
}

/* Finds "Render Filter" by what's actually in it (its first item's command
   ID never changes) rather than by position -- a hardcoded position here
   broke silently the last time an item got added above it in the Video
   menu, and rebuilt the wrong submenu's contents without any error. */
static HMENU find_render_filter_menu(HMENU video_menu)
{
  int i, count = GetMenuItemCount(video_menu);

  for (i = 0; i < count; i++)
  {
    HMENU sub = GetSubMenu(video_menu, i);
    if (sub && GetMenuItemID(sub, 0) == (UINT)IDM_VIDEO_FILTER_NONE) return sub;
  }
  return NULL;
}

static void rebuild_filter_menu(void)
{
  HMENU video_menu = find_video_menu();
  HMENU filters = video_menu ? find_render_filter_menu(video_menu) : NULL;
  int current, count, i;

  if (!filters) return;

  clear_menu(filters);

  current = video_filter_current();
  count   = video_filter_count();
  if (count > FILTER_MENU_MAX) count = FILTER_MENU_MAX;

  AppendMenuA(filters, MF_STRING, (UINT_PTR)IDM_VIDEO_FILTER_NONE, "None");

  if (count > 0)
  {
    AppendMenuA(filters, MF_SEPARATOR, 0, NULL);
    for (i = 0; i < count; i++)
    {
      if (i > 0 && video_filter_separator_before(i)) AppendMenuA(filters, MF_SEPARATOR, 0, NULL);
      AppendMenuA(filters, MF_STRING, (UINT_PTR)(IDM_VIDEO_FILTER_BASE + i), video_filter_name(i));
    }
  }

  if (current < 0 || current >= count)
  {
    CheckMenuItem(filters, IDM_VIDEO_FILTER_NONE, MF_BYCOMMAND | MF_CHECKED);
  }
  else
  {
    CheckMenuItem(filters, (UINT)(IDM_VIDEO_FILTER_BASE + current), MF_BYCOMMAND | MF_CHECKED);
  }

  if (gui.large_ui) theme_ownerdraw_menu(filters);

  DrawMenuBar(g_hwnd);
}

static int system_menu_index(void)
{
  switch (config.system)
  {
    case SYSTEM_SG:           return 1;
    case SYSTEM_SGII:         return 2;
    case SYSTEM_SGII_RAM_EXT: return 3;
    case SYSTEM_MARKIII:      return 4;
    case SYSTEM_SMS:          return 5;
    case SYSTEM_SMS2:         return 6;
    case SYSTEM_GG:           return 7;
    case SYSTEM_MD:           return 8;
    default:                  return 0;
  }
}

static int port_menu_index(int port)
{
  static const int port_a[] = { NO_SYSTEM, SYSTEM_GAMEPAD, SYSTEM_MOUSE, SYSTEM_XE_1AP,
                                SYSTEM_ACTIVATOR, SYSTEM_LIGHTPHASER, SYSTEM_PADDLE,
                                SYSTEM_SPORTSPAD, SYSTEM_GRAPHIC_BOARD, SYSTEM_TEAMPLAYER };
  static const int port_b[] = { NO_SYSTEM, SYSTEM_GAMEPAD, SYSTEM_MOUSE, SYSTEM_MENACER,
                                SYSTEM_JUSTIFIER, SYSTEM_XE_1AP, SYSTEM_ACTIVATOR,
                                SYSTEM_LIGHTPHASER, SYSTEM_PADDLE, SYSTEM_TEAMPLAYER };
  const int *table = port ? port_b : port_a;
  int i;

  for (i = 0; i < 10; i++)
  {
    if (table[i] == input.system[port]) return i;
  }
  return 1;
}

static int port_menu_value(int port, int index)
{
  static const int port_a[] = { NO_SYSTEM, SYSTEM_GAMEPAD, SYSTEM_MOUSE, SYSTEM_XE_1AP,
                                SYSTEM_ACTIVATOR, SYSTEM_LIGHTPHASER, SYSTEM_PADDLE,
                                SYSTEM_SPORTSPAD, SYSTEM_GRAPHIC_BOARD, SYSTEM_TEAMPLAYER };
  static const int port_b[] = { NO_SYSTEM, SYSTEM_GAMEPAD, SYSTEM_MOUSE, SYSTEM_MENACER,
                                SYSTEM_JUSTIFIER, SYSTEM_XE_1AP, SYSTEM_ACTIVATOR,
                                SYSTEM_LIGHTPHASER, SYSTEM_PADDLE, SYSTEM_TEAMPLAYER };

  if (index < 0 || index > 9) return SYSTEM_GAMEPAD;
  return port ? port_b[index] : port_a[index];
}

/* Finds the "Save Slot" submenu by the first command ID it holds, rather
   than by position or by which top-level menu it lives in, so moving it
   between menus (it used to be under File, now Emulation) doesn't break
   this. */
static HMENU find_slot_menu(void)
{
  int t, top_count = GetMenuItemCount(g_menu);

  for (t = 0; t < top_count; t++)
  {
    HMENU top = GetSubMenu(g_menu, t);
    int i, count;

    if (!top) continue;
    count = GetMenuItemCount(top);

    for (i = 0; i < count; i++)
    {
      HMENU sub = GetSubMenu(top, i);
      if (sub && GetMenuItemID(sub, 0) == (UINT)IDM_FILE_SLOT_BASE) return sub;
    }
  }
  return NULL;
}

/* Sets an item's text. With large_ui the menu is owner-drawn and paints
   from the string stored in itemData (see theme.c), so that has to be
   replaced too or the old text would keep showing. */
static void menu_set_text(HMENU menu, UINT id, const char *text)
{
  MENUITEMINFOA mii;

  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(mii);
  mii.fMask  = MIIM_FTYPE | MIIM_DATA;
  if (!GetMenuItemInfoA(menu, id, FALSE, &mii)) return;

  if (mii.fType & MFT_OWNERDRAW)
  {
    char *stored = (char *)malloc((size_t)lstrlenA(text) + 1);
    if (!stored) return;
    lstrcpyA(stored, text);
    if (mii.dwItemData) free((void *)mii.dwItemData);
    mii.fMask      = MIIM_DATA;
    mii.dwItemData = (ULONG_PTR)stored;
  }
  else
  {
    mii.fMask      = MIIM_STRING;
    mii.dwTypeData = (LPSTR)text;
  }
  SetMenuItemInfoA(menu, id, FALSE, &mii);
}

/* "Slot 0 - hh:mm:ss  DD/MM/YYYY" for each slot that holds a save. */
static void update_slot_menu_labels(HMENU slot_menu)
{
  int i;

  if (!slot_menu) return;

  for (i = 0; i < GUI_SLOT_MAX; i++)
  {
    char path[GUI_PATH_LEN], stamp[32], label[64];

    if (!emu_running)
    {
      wsprintfA(label, "Slot &%d", i);
    }
    else
    {
      state_path(i, path, sizeof(path));
      if (state_file_time(path, stamp, sizeof(stamp)))
        wsprintfA(label, "Slot &%d - %s", i, stamp);
      else
        wsprintfA(label, "Slot &%d - Empty", i);
    }
    menu_set_text(slot_menu, IDM_FILE_SLOT_BASE + i, label);
  }
}

void gui_update_menu(void)
{
  UINT rom_state = emu_running ? MF_ENABLED : (MF_GRAYED | MF_DISABLED);
  UINT browsing_state = emu_running ? (MF_GRAYED | MF_DISABLED) : MF_ENABLED;

  if (!g_menu) return;

  EnableMenuItem(g_menu, IDM_VIEW_LIST, MF_BYCOMMAND | browsing_state);
  EnableMenuItem(g_menu, IDM_VIEW_GRID, MF_BYCOMMAND | browsing_state);
  EnableMenuItem(g_menu, IDM_VIDEO_LARGE_UI, MF_BYCOMMAND | browsing_state);

  EnableMenuItem(g_menu, IDM_FILE_CLOSE, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_EMU_STOP, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_FILE_ROMINFO, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_FILE_SAVESTATE, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_FILE_LOADSTATE, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_EMU_UNDOLOAD, MF_BYCOMMAND |
                 ((emu_running && emu_can_undo_load()) ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
  EnableMenuItem(g_menu, IDM_FILE_STATEMGR, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_FILE_SCREENSHOT, MF_BYCOMMAND | rom_state);
  {
    int rec = recorder_active();

    EnableMenuItem(g_menu, IDM_FILE_REC_VIDEO,
                   MF_BYCOMMAND | ((emu_running && (rec == REC_NONE || rec == REC_VIDEO)) ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
    EnableMenuItem(g_menu, IDM_FILE_REC_AUDIO,
                   MF_BYCOMMAND | ((emu_running && (rec == REC_NONE || rec == REC_AUDIO)) ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
    EnableMenuItem(g_menu, IDM_FILE_REC_STOP,
                   MF_BYCOMMAND | (rec != REC_NONE ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
    CheckMenuItem(g_menu, IDM_FILE_REC_VIDEO, MF_BYCOMMAND | (rec == REC_VIDEO ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_menu, IDM_FILE_REC_AUDIO, MF_BYCOMMAND | (rec == REC_AUDIO ? MF_CHECKED : MF_UNCHECKED));
  }
  EnableMenuItem(g_menu, IDM_TOOLS_NETPLAY,
                 MF_BYCOMMAND | ((emu_running && netplay_state() == NP_IDLE) ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
  EnableMenuItem(g_menu, IDM_TOOLS_NETPLAY_STOP,
                 MF_BYCOMMAND | (netplay_state() != NP_IDLE ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
  EnableMenuItem(g_menu, IDM_EMU_PAUSE, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_EMU_RESET, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_EMU_HARDRESET, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_EMU_CHEATS, MF_BYCOMMAND | rom_state);
  EnableMenuItem(g_menu, IDM_VIDEO_FULLSCREEN, MF_BYCOMMAND | rom_state);

  CheckMenuItem(g_menu, IDM_EMU_PAUSE,
                MF_BYCOMMAND | (emu_paused ? MF_CHECKED : MF_UNCHECKED));

  update_slot_menu_labels(find_slot_menu());
  check_radio(g_menu, IDM_FILE_SLOT_BASE, GUI_SLOT_MAX, gui.state_slot);
  check_radio(g_menu, IDM_EMU_REGION_BASE, 5, config.region_detect);
  check_radio(g_menu, IDM_EMU_VDPMODE_BASE, 3, config.vdp_mode);
  check_radio(g_menu, IDM_EMU_SYSTEM_BASE, 9, system_menu_index());
  check_radio(g_menu, IDM_EMU_LOCKON_BASE, 4, config.lock_on);

  CheckMenuItem(g_menu, IDM_EMU_BIOS,
                MF_BYCOMMAND | (config.bios ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_EMU_ADDRERROR,
                MF_BYCOMMAND | (config.addr_error ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_EMU_PAUSE_UNFOCUSED,
                MF_BYCOMMAND | (gui.pause_on_focus_loss ? MF_CHECKED : MF_UNCHECKED));

  check_radio(g_menu, IDM_VIEW_LIST, 2, gui.browser_grid_view);
  check_radio(g_menu, IDM_VIDEO_SCALE_BASE, 6, gui.scale - 1);
  check_radio(g_menu, IDM_VIDEO_ASPECT_BASE, 3, gui.aspect);
  check_radio(g_menu, IDM_VIDEO_NTSC_BASE, 4, config.ntsc);
  check_radio(g_menu, IDM_VIDEO_OVERSCAN_BASE, 4, config.overscan);

  CheckMenuItem(g_menu, IDM_VIDEO_FULLSCREEN,
                MF_BYCOMMAND | (gui.fullscreen ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_VIDEO_FULLSCREEN_START,
                MF_BYCOMMAND | (gui.fullscreen_on_load ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_VIDEO_ALWAYS_ON_TOP,
                MF_BYCOMMAND | (gui.always_on_top ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_VIDEO_SMOOTH,
                MF_BYCOMMAND | (gui.smooth ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_INPUT_BACKGROUND,
                MF_BYCOMMAND | (gui.background_input ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_VIDEO_BRIGHTEN,
                MF_BYCOMMAND | (gui.brighten ? MF_CHECKED : MF_UNCHECKED));
  check_radio(g_menu, IDM_VIDEO_RENDERER_BASE, 3, gui.renderer);
  CheckMenuItem(g_menu, IDM_VIDEO_VSYNC,
                MF_BYCOMMAND | (gui.vsync ? MF_CHECKED : MF_UNCHECKED));
  check_radio(g_menu, IDM_VIDEO_SCANLINE_BASE, 5, gui.scanline_pct / 25);
  check_radio(g_menu, IDM_VIDEO_THEME_BASE, 3, theme_get_mode());
  CheckMenuItem(g_menu, IDM_VIDEO_LARGE_UI,
                MF_BYCOMMAND | (gui.large_ui ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_VIDEO_SHOWFPS,
                MF_BYCOMMAND | (gui.show_fps ? MF_CHECKED : MF_UNCHECKED));
  check_radio(g_menu, IDM_VIDEO_INTERLACE_BASE, 2, config.render ? 1 : 0);
  check_radio(g_menu, IDM_VIDEO_LCD_BASE, 4,
              config.lcd == 0 ? 0 : (config.lcd <= 64 ? 1 : (config.lcd <= 128 ? 2 : 3)));
  check_radio(g_menu, IDM_VIDEO_FRAMESKIP_BASE, 6, gui.frameskip);
  check_radio(g_menu, IDM_EMU_RUNAHEAD_BASE, 4, gui.runahead);
  CheckMenuItem(g_menu, IDM_EMU_REWIND,
                MF_BYCOMMAND | (gui.rewind ? MF_CHECKED : MF_UNCHECKED));
  check_radio(g_menu, IDM_TOOLS_SHOT_BASE, 3, gui.shot_mode);
  CheckMenuItem(g_menu, IDM_VIDEO_SMSBORDER,
                MF_BYCOMMAND | (gui.sms_show_border ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_VIDEO_GGEXTRA,
                MF_BYCOMMAND | (config.gg_extra ? MF_CHECKED : MF_UNCHECKED));

  CheckMenuItem(g_menu, IDM_AUDIO_ENABLE,
                /* Menu label is "Mute", so the checkmark means the opposite of
                   what it did as "Enable Sound" -- checked now means sound is
                   OFF. Same underlying gui.sound_enabled and the same click
                   handler (still a plain toggle either way), just presented
                   the other way round, so nothing else in the audio pipeline
                   needed to change. */
                MF_BYCOMMAND | (!gui.sound_enabled ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_AUDIO_MONO,
                MF_BYCOMMAND | (config.mono ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_AUDIO_LOWPASS,
                MF_BYCOMMAND | (config.filter == 1 ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(g_menu, IDM_AUDIO_HQPSG,
                MF_BYCOMMAND | (config.hq_psg ? MF_CHECKED : MF_UNCHECKED));

  check_radio(g_menu, IDM_AUDIO_FMCORE_BASE, 5,
              config.ym3438 ? (gui.nuked_ym2612 ? 3 : 4) : ((config.ym2612 > 2) ? 2 : config.ym2612));
  check_radio(g_menu, IDM_AUDIO_RATE_BASE, 2, (gui.sample_rate == 44100) ? 0 : 1);

  check_radio(g_menu, IDM_INPUT_PORTA_BASE, 10, port_menu_index(0));
  check_radio(g_menu, IDM_INPUT_PORTB_BASE, 10, port_menu_index(1));

  status_set_slot();
}

/****************************************************************************
 * Window sizing
 ****************************************************************************/

/* In dark mode the top-level bar is themed by hand (Windows paints its own
   light-mode chrome around a plain dark background otherwise), and that
   theming goes through an undocumented per-item message (WM_UAHDRAWMENUITEM)
   that -- confirmed by direct testing -- Windows simply does not send for
   every item once the bar has wrapped to a second row: whichever titles land
   on the row Windows drops come up blank until something else (a click, a
   hover, a resize) forces a repaint of that one item, at which point it
   appears for good. This is not specific to Larger UI -- it reproduces in
   plain dark mode at a narrow window, Larger UI just makes it easier to hit
   because the bigger font needs more width. There is no fix for the message
   not being sent, so the window is kept wide enough that the bar never wraps
   in the first place, both when it is first created (create_main_window())
   and whenever it is resized to a scale afterward (gui_resize_to_scale(),
   which otherwise sizes purely from the game picture and knows nothing about
   the menu bar). Measured against the menu's plain text, before Larger UI's
   theme_ownerdraw_menu() converts the items: once converted, their text is no
   longer retrievable through GetMenuStringA(). Returns 0 outside dark mode,
   where the bar is never touched by hand and this does not apply. */
static int menu_bar_min_width(HMENU menu)
{
  HDC mhdc;
  HFONT mfont, mold;
  int needed = 0;
  int mcount, mi;

  if (!theme_is_dark() || !menu) return 0;

  mhdc  = GetDC(NULL);
  mfont = gui.large_ui ? gui_get_ui_font() : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  mold  = (HFONT)SelectObject(mhdc, mfont);
  mcount = GetMenuItemCount(menu);

  for (mi = 0; mi < mcount; mi++)
  {
    char mtext[128];
    SIZE msz;

    if (GetMenuStringA(menu, (UINT)mi, mtext, sizeof(mtext), MF_BYPOSITION) > 0 &&
        GetTextExtentPoint32A(mhdc, mtext, lstrlenA(mtext), &msz))
    {
      needed += msz.cx + (gui.large_ui ? 44 : 40);   /* generous: the real bar's own
        inter-item and edge spacing runs well past itemWidth's own +20 formula,
        confirmed by direct testing (20 left 2 of 9 items still wrapped, 20+a
        flat +60 still left 1 of 9 wrapped) -- a window wider than strictly
        necessary costs nothing, so this errs well on the generous side rather
        than chasing the exact figure further. */
    }
  }

  needed += 40;   /* extra flat margin, on top of the generous per-item one above */

  SelectObject(mhdc, mold);
  ReleaseDC(NULL, mhdc);
  return needed;
}

void gui_resize_to_scale(int scale)
{
  RECT want;
  int w, h, sb = 0;

  if (gui.fullscreen) return;

  video_preferred_size(scale, &w, &h);

  if (g_status && IsWindowVisible(g_status))
  {
    RECT r;
    GetWindowRect(g_status, &r);
    sb = r.bottom - r.top;
  }

  want.left = 0;
  want.top = 0;
  want.right = w;
  want.bottom = h + sb;

  AdjustWindowRectEx(&want,
                     (DWORD)GetWindowLongPtr(g_hwnd, GWL_STYLE), TRUE,
                     (DWORD)GetWindowLongPtr(g_hwnd, GWL_EXSTYLE));

  {
    int mw = menu_bar_min_width(GetMenu(g_hwnd));
    if (mw > want.right - want.left) want.right = want.left + mw;
  }

  SetWindowPos(g_hwnd, NULL, 0, 0,
               want.right - want.left, want.bottom - want.top,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Keeps the window above all others, or lets it behave normally again --
   called once at startup to restore the saved setting, and again whenever
   the person toggles it from the Video menu. */
void apply_always_on_top(void)
{
  SetWindowPos(g_hwnd, gui.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
               0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

/****************************************************************************
 * Save files
 ****************************************************************************/

/****************************************************************************
 * Per-game data files: <kind>\<Console>\<name> [<id>]<ext>
 ****************************************************************************/

static char        rom_id[9];              /* 8 hex digits, "" if not known */
static const char *rom_console = "";       /* folder name, "" if not known  */

static const char *const console_folders[] =
  { "MegaDrive", "MasterSystem", "GameGear", "SG1000", "MegaCD", "Other" };

static const char *console_folder_name(void)
{
  if (system_hw == SYSTEM_MCD) return "MegaCD";
  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) return "MegaDrive";
  if (system_hw == SYSTEM_GG || system_hw == SYSTEM_GGMS) return "GameGear";
  if (system_hw == SYSTEM_SMS || system_hw == SYSTEM_SMS2 || system_hw == SYSTEM_MARKIII) return "MasterSystem";
  if (system_hw == SYSTEM_SG || system_hw == SYSTEM_SGII || system_hw == SYSTEM_SGII_RAM_EXT) return "SG1000";
  return "Other";
}

static uint32 id_hash(uint32 h, const void *data, size_t n)
{
  const uint8 *p = (const uint8 *)data;
  while (n--) { h ^= *p++; h *= 16777619u; }
  return h;
}

/* Works out which console folder and which id the game that was just loaded
   belongs to. Called right after load_rom(), before any cheat patch touches
   the ROM, so the id only depends on the game's own data (a .zip and the
   unpacked file give the same id). For a Mega CD game cart.rom is the CD BIOS,
   so the disc header is hashed instead. */
static void identify_rom(void)
{
  uint32 h = 2166136261u;

  rom_console = console_folder_name();

  if (system_hw == SYSTEM_MCD)
  {
    h = id_hash(h, rominfo.domestic,      sizeof(rominfo.domestic));
    h = id_hash(h, rominfo.international, sizeof(rominfo.international));
    h = id_hash(h, rominfo.product,       sizeof(rominfo.product));
    h = id_hash(h, rominfo.copyright,     sizeof(rominfo.copyright));
    h = id_hash(h, rominfo.country,       sizeof(rominfo.country));
  }
  else
  {
    h = id_hash(h, cart.rom, (size_t)cart.romsize);
  }

  wsprintfA(rom_id, "%08X", (unsigned)h);
}

static int file_exists(const char *path)
{
  return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static void data_path_new(const char *kind, const char *console, const char *ext, char *out, int out_len)
{
  char tmp[GUI_PATH_LEN * 2];

  if (rom_id[0])
    wsprintfA(tmp, "%s\\%s\\%s [%s]%s", osd_path(kind), console, rom_base, rom_id, ext);
  else
    wsprintfA(tmp, "%s\\%s%s", osd_path(kind), rom_base, ext);
  lstrcpynA(out, tmp, out_len);
}

void emu_data_path(const char *kind, const char *ext, int for_read, char *out, int out_len)
{
  char cand[GUI_PATH_LEN];
  size_t i;

  data_path_new(kind, rom_console[0] ? rom_console : "Other", ext, out, out_len);
  if (!for_read || file_exists(out)) return;

  /* The same game under another console folder (the Console option was
     changed after it was saved). */
  for (i = 0; i < sizeof(console_folders) / sizeof(console_folders[0]); i++)
  {
    if (!lstrcmpA(console_folders[i], rom_console)) continue;
    data_path_new(kind, console_folders[i], ext, cand, sizeof(cand));
    if (file_exists(cand)) { lstrcpynA(out, cand, out_len); return; }
  }

  /* Flat name used by earlier versions: <kind>\<name><ext>. */
  {
    char tmp[GUI_PATH_LEN * 2];
    wsprintfA(tmp, "%s\\%s%s", osd_path(kind), rom_base, ext);
    if (file_exists(tmp)) lstrcpynA(out, tmp, out_len);
  }
}

void emu_ensure_data_dir(const char *kind)
{
  char tmp[GUI_PATH_LEN];

  CreateDirectoryA(osd_path(kind), NULL);
  wsprintfA(tmp, "%s\\%s", osd_path(kind), rom_console[0] ? rom_console : "Other");
  CreateDirectoryA(tmp, NULL);
}

/* Earlier versions named these files after the game's file name only. The
   first game with that name to be played takes them over: they are MOVED into
   the new location, so a second, different game with the same name starts
   clean instead of inheriting them. (Nothing is moved if a file already
   exists at the new location.) */
static void adopt_legacy_one(const char *kind, const char *ext)
{
  char legacy[GUI_PATH_LEN], dest[GUI_PATH_LEN];

  wsprintfA(legacy, "%s\\%s%s", osd_path(kind), rom_base, ext);
  if (!file_exists(legacy)) return;

  data_path_new(kind, rom_console[0] ? rom_console : "Other", ext, dest, sizeof(dest));
  if (file_exists(dest)) return;

  emu_ensure_data_dir(kind);
  MoveFileA(legacy, dest);
}

static void adopt_legacy_files(void)
{
  char ext[24];
  int i;

  if (!rom_id[0]) return;

  adopt_legacy_one("saves", ".srm");
  adopt_legacy_one("saves", ".brm");
  adopt_legacy_one("cheats", ".cht");
  for (i = 0; i < GUI_SLOT_MAX; i++)
  {
    wsprintfA(ext, ".gp%d", i);     adopt_legacy_one("states", ext);
    wsprintfA(ext, ".gp%d.bmp", i); adopt_legacy_one("states", ext);
  }
}

/* Writes a file so that a crash or power cut part-way through can never leave
   it half-written: the data goes to "<path>.tmp" first, is flushed, and then
   replaces the real file in one step. Returns 1 only if everything succeeded. */
static int write_file_atomic(const char *path, const void *data, size_t len)
{
  char tmp[GUI_PATH_LEN + 8];
  FILE *fp;
  int ok;

  wsprintfA(tmp, "%s.tmp", path);
  fp = fopen(tmp, "wb");
  if (!fp) return 0;

  ok = (len == 0) || (fwrite(data, 1, len, fp) == len);
  if (fflush(fp) != 0) ok = 0;
  if (fclose(fp) != 0) ok = 0;

  if (ok && !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) ok = 0;
  if (!ok) DeleteFileA(tmp);
  return ok;
}

/* Fingerprint of everything save_backup_ram() writes, so changes can be
   noticed cheaply (a few 64 KB passes) without rewriting files needlessly. */
static uint32 bram_hash_bytes(uint32 h, const uint8 *p, size_t n)
{
  while (n--) { h ^= *p++; h *= 16777619u; }
  return h;
}

static uint32 backup_ram_hash(void)
{
  uint32 h = 2166136261u;

  if (system_hw == SYSTEM_MCD)
  {
    h = bram_hash_bytes(h, scd.bram, 0x2000);
    if (scd.cartridge.id) h = bram_hash_bytes(h, scd.cartridge.area, (size_t)scd.cartridge.mask + 1);
  }
  if (sram.on) h = bram_hash_bytes(h, sram.sram, 0x10000);
  return h;
}

static uint32 bram_saved_hash;
static int    bram_hash_valid;      /* bram_saved_hash matches what is on disk */

static void save_backup_ram(void)
{
  char path[GUI_PATH_LEN];

  if (np_no_save) return;   /* see np_no_save */

  if (!emu_running) return;

  emu_ensure_data_dir("saves");

  if (system_hw == SYSTEM_MCD)
  {
    if (!memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
    {
      emu_data_path("saves", ".brm", 0, path, sizeof(path));
      write_file_atomic(path, scd.bram, 0x2000);
    }

    if (scd.cartridge.id &&
        !memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
    {
      gui_snprintf(path, (int)sizeof(path), "%s\\cart.brm", osd_path("saves"));
      write_file_atomic(path, scd.cartridge.area, (size_t)scd.cartridge.mask + 1);
    }
  }

  if (sram.on)
  {
    emu_data_path("saves", ".srm", 0, path, sizeof(path));
    write_file_atomic(path, sram.sram, 0x10000);
  }

  bram_saved_hash = backup_ram_hash();
  bram_hash_valid = 1;
}

/* Called about every 5 seconds while a game runs: if the game has changed its
   save data since the last write, write it now, so a crash, a power cut or
   Windows shutting down does not lose the progress. */
#define AUTOSAVE_INTERVAL_MS 5000

static void backup_ram_autosave(DWORD now)
{
  static DWORD last_check;

  if (now - last_check < AUTOSAVE_INTERVAL_MS) return;
  last_check = now;

  if (!emu_running || np_no_save) return;
  if (!bram_hash_valid || backup_ram_hash() != bram_saved_hash) save_backup_ram();
}

static void load_backup_ram(void)
{
  FILE *fp;
  char path[GUI_PATH_LEN];

  if (system_hw == SYSTEM_MCD)
  {
    emu_data_path("saves", ".brm", 1, path, sizeof(path));
    fp = fopen(path, "rb");
    if (fp) { fread(scd.bram, 0x2000, 1, fp); fclose(fp); }

    if (memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
    {
      memset(scd.bram, 0x00, 0x200);
      brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] = 0x00;
      brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] =
        (uint8)((sizeof(scd.bram) / 64) - 3);
      memcpy(scd.bram + 0x2000 - 0x40, brm_format, 0x40);
    }

    if (scd.cartridge.id)
    {
      gui_snprintf(path, (int)sizeof(path), "%s\\cart.brm", osd_path("saves"));
      fp = fopen(path, "rb");
      if (fp) { fread(scd.cartridge.area, scd.cartridge.mask + 1, 1, fp); fclose(fp); }

      if (memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
      {
        memset(scd.cartridge.area, 0x00, scd.cartridge.mask + 1);
        brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] =
          (uint8)((((scd.cartridge.mask + 1) / 64) - 3) >> 8);
        brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] =
          (uint8)((((scd.cartridge.mask + 1) / 64) - 3) & 0xff);
        memcpy(scd.cartridge.area + scd.cartridge.mask + 1 - sizeof(brm_format),
               brm_format, sizeof(brm_format));
      }
    }
  }

  if (sram.on)
  {
    emu_data_path("saves", ".srm", 1, path, sizeof(path));
    fp = fopen(path, "rb");
    if (fp) { fread(sram.sram, 0x10000, 1, fp); fclose(fp); }
  }

  /* What is in memory now is what is on disk (or the fresh default). */
  bram_saved_hash = backup_ram_hash();
  bram_hash_valid = 1;
}

/* Persists the cheat list against whatever ROM the list belongs to: the one
   that is running, or the one that was only inspected (emu_peek_rom, from the
   browser's Edit Cheats). rom_base is set in both cases and cleared when a
   running game is closed, and the list is always the one loaded for it. */
void cheats_save_current(void)
{
  if (rom_base[0]) cheats_save_for_rom(rom_base);
}

/****************************************************************************
 * Save states
 ****************************************************************************/

/* The existing state file for a slot (see emu_data_path()). */
void state_path(int slot, char *out, int out_len)
{
  char ext[16];

  wsprintfA(ext, ".gp%d", slot);
  emu_data_path("states", ext, 1, out, out_len);
}

void thumb_path(int slot, char *out, int out_len)
{
  char ext[24];

  wsprintfA(ext, ".gp%d.bmp", slot);
  emu_data_path("states", ext, 1, out, out_len);
}

/* Removes a slot's state and thumbnail, including any copy under an older
   name -- otherwise the old copy would show up in the slot again. */
void state_delete(int slot)
{
  char p[GUI_PATH_LEN];
  int i;

  for (i = 0; i < 8; i++)
  {
    state_path(slot, p, sizeof(p));
    if (!file_exists(p)) break;
    DeleteFileA(p);
  }
  for (i = 0; i < 8; i++)
  {
    thumb_path(slot, p, sizeof(p));
    if (!file_exists(p)) break;
    DeleteFileA(p);
  }
}

int state_file_time(const char *path, char *out, int out_len)
{
  WIN32_FILE_ATTRIBUTE_DATA fad;
  SYSTEMTIME utc, st;

  if (out_len > 0) out[0] = '\0';
  if (!path || !GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) return 0;

  FileTimeToSystemTime(&fad.ftLastWriteTime, &utc);
  if (!SystemTimeToTzSpecificLocalTime(NULL, &utc, &st)) st = utc;

  if (out_len >= 24)
    wsprintfA(out, "%02d:%02d:%02d  %02d/%02d/%04d",
              st.wHour, st.wMinute, st.wSecond, st.wDay, st.wMonth, st.wYear);
  return 1;
}

void emu_save_state(int slot)
{
  char path[GUI_PATH_LEN];
  uint8 *buffer;
  int len;

  if (!emu_running) return;

  buffer = (uint8 *)malloc(STATE_SIZE);
  if (!buffer) { gui_notify("Not enough memory to save the state"); return; }

  len = state_save(buffer);

  emu_ensure_data_dir("states");
  {
    char ext[16];
    wsprintfA(ext, ".gp%d", slot);
    emu_data_path("states", ext, 0, path, sizeof(path));
  }

  if (len > 0 && write_file_atomic(path, buffer, (size_t)len))
  {
    /* state_path() now finds the new file, so the thumbnail lands beside it. */
    thumb_path(slot, path, sizeof(path));
    video_save_thumbnail(path);
    gui_notify("Saved to slot %d", slot);
  }
  else
  {
    gui_notify("Could not write slot %d", slot);
  }

  free(buffer);
}

/* The game as it was just before the last successful state load. */
static uint8 *undo_state;
static int    undo_valid;

void emu_undo_load_state(void)
{
  if (!emu_running || !undo_valid || !undo_state) return;

  undo_valid = 0;
  if (state_load(undo_state))
  {
    emu_apply_sms_border();
    waveout_flush();
    cheats_apply();
    video_viewport_changed();
    gui_notify("Load undone");
  }
  else
  {
    gui_notify("Could not undo the load");
  }
}

int emu_can_undo_load(void)
{
  return undo_valid;
}

void emu_load_state(int slot)
{
  char path[GUI_PATH_LEN];
  uint8 *buffer;
  FILE *fp;
  size_t got;
  int have_undo = 0;
  int loaded = 0;

  if (!emu_running) return;

  state_path(slot, path, sizeof(path));

  fp = fopen(path, "rb");
  if (!fp) { gui_notify("Slot %d is empty", slot); return; }

  buffer = (uint8 *)malloc(STATE_SIZE);
  if (!buffer) { fclose(fp); gui_notify("Not enough memory to load the state"); return; }

  got = fread(buffer, 1, STATE_SIZE, fp);
  fclose(fp);

  if (got > 0)
  {
    /* Keep the game as it is now, so a load pressed by mistake can be undone. */
    if (!undo_state) undo_state = (uint8 *)malloc(STATE_SIZE);
    if (undo_state && state_save(undo_state) > 0) have_undo = 1;

    loaded = state_load(buffer) ? 1 : 0;
  }

  if (loaded)
  {
    undo_valid = have_undo;
    emu_apply_sms_border();
    gui_notify("Loaded slot %d", slot);
    waveout_flush();
    /* Banking was restored from the state, so 8-bit ROM patches now point at the
       wrong page. Rewrite them all. */
    cheats_apply();
    video_viewport_changed();
    gui_update_menu();
  }
  else
  {
    gui_notify(got == 0 ? "Slot %d could not be read" : "Slot %d was saved by a different version", slot);
  }

  free(buffer);
}

/****************************************************************************
 * Audio plumbing
 ****************************************************************************/

/* The Nuked core can behave as a YM2612 (early consoles: status readable only
   at the first port) or as a YM3438 (later ones: readable at any port). The
   core default is the YM3438 behaviour. */
/* Master System only. Unless the side borders are shown, hides the 8-pixel side borders (the column games blank to
   hide scrolling) by cropping 8 pixels off each side of the picture, the same
   way the Game Gear's smaller screen is produced. The core resets this on every
   reset, so it is re-applied after each one. Not used while the Borders
   setting already shows the left/right overscan. */
void emu_apply_sms_border(void)
{
  if (!emu_running) return;
  if (system_hw != SYSTEM_MARKIII && system_hw != SYSTEM_SMS && system_hw != SYSTEM_SMS2) return;

  if (!gui.sms_show_border && !(config.overscan & 2)) bitmap.viewport.x = -8;
  else                                               bitmap.viewport.x = (config.overscan & 2) * 7;

  bitmap.viewport.changed = 3;
}

void emu_apply_nuked_type(void)
{
  OPN2_SetChipType(gui.nuked_ym2612 ? ym3438_mode_ym2612 : ym3438_mode_readmode);
}

/* Re-creates the FM chip after its type changed (Master System FM unit or
   chip, or the Genesis FM core). Swapping chips mid-game can't carry the
   old chip's registers over, so some instruments may stay quiet until the
   game re-sends them or is reset. */
void emu_apply_fm_settings(void)
{
  if (!emu_running) return;

  audio_init(gui.sample_rate, 0);
  sound_init();
  sound_reset();
  audio_reset();
}

void emu_apply_audio_settings(void)
{
  if (!gui.sound_enabled)
  {
    waveout_close();
    return;
  }

  /* Only touch the device when the format actually changed -- reopening it
     on every ROM load produces an audible click for no reason. */
  if (waveout_rate() != gui.sample_rate)
  {
    waveout_close();

    if (!waveout_open(gui.sample_rate))
    {
      gui.sound_enabled = 0;
      gui_status("No audio device available, sound is off");
      return;
    }
  }

  waveout_set_volume(gui.volume);

  if (emu_running)
  {
    /* Only audio_init() -- it's the one that actually depends on the
       sample rate (it rebuilds the blip buffers to match). sound_init()
       does not: reading its implementation directly shows its behaviour
       depends purely on which FM core is configured (config.ym2612/
       config.ym3438), never on the sample rate. Calling it here anyway
       was unnecessarily wiping the FM/PSG chip's entire register/channel
       state on every Sample Rate or Enable Sound change -- silencing
       whatever instruments the running game had already set up and
       wasn't continuously re-sending, since nothing told the game itself
       anything had changed. That's what "missing instruments until a
       reset" was: not a timing bug, but real state being thrown away for
       a change that never needed to touch it. */
    audio_init(gui.sample_rate, 0);
  }
}

/****************************************************************************
 * Region changes
 ****************************************************************************/

void emu_apply_region(void)
{
  if (!emu_running) return;

  get_region(0);

  if ((system_hw == SYSTEM_MCD) || ((system_hw & SYSTEM_SMS) && (config.bios & 1)))
  {
    /* Same order as a hard reset: system_init() starts the save memory from
       scratch, so keep what the game had written and put it back. */
    cheats_suspend();
    save_backup_ram();
    system_init();
    load_backup_ram();
    system_reset();
    emu_apply_sms_border();
  }
  else
  {
    if (system_hw == SYSTEM_MD)
    {
      io_reg[0x00] = (uint8)(0x20 | region_code | (config.bios & 1));
    }
    else
    {
      io_reg[0x00] = (uint8)(0x80 | (region_code >> 1));
    }

    if (vdp_pal)
    {
      status |= 1;
      lines_per_frame = 313;
    }
    else
    {
      status &= ~1;
      lines_per_frame = 262;
    }

    switch (bitmap.viewport.h)
    {
      case 192: vc_max = vc_table[0][vdp_pal]; break;
      case 224: vc_max = vc_table[1][vdp_pal]; break;
      case 240: vc_max = vc_table[3][vdp_pal]; break;
      default: break;
    }
  }

  /* The frame rate moved, so the audio timing has to be rebuilt. This has to
     come after lines_per_frame is updated above: the core derives the frame
     rate from it, and rebuilding first kept the old (60 Hz / 50 Hz) rate. */
  audio_init(snd.sample_rate, 0);

  cheats_apply();
  waveout_flush();
  video_viewport_changed();
}

/****************************************************************************
 * ROM lifecycle
 ****************************************************************************/

static void load_boot_rom(void)
{
  FILE *fp;

  system_bios = 0;
  memset(boot_rom, 0xFF, sizeof(boot_rom));

  fp = fopen(MD_BIOS, "rb");
  if (!fp) return;

  fread(boot_rom, 1, 0x800, fp);
  fclose(fp);

  if (!memcmp((char *)(boot_rom + 0x120), "GENESIS OS", 10))
  {
    int i;

    system_bios = SYSTEM_MD;

    for (i = 0; i < 0x800; i += 2)
    {
      uint8 temp = boot_rom[i];
      boot_rom[i] = boot_rom[i + 1];
      boot_rom[i + 1] = temp;
    }

    for (i = 0x800; i < 0x10000; i++)
    {
      boot_rom[i] = boot_rom[i & 0x7ff];
    }
  }
}

void emu_close_rom(void)
{
  if (!emu_running) return;

  record_stop();
  netplay_disconnect(NULL);

  cheats_save_current();
  cheats_suspend();          /* undo patches while cart.rom is still valid */
  cheats_remove_all();

  save_backup_ram();
  bram_hash_valid = 0;
  audio_shutdown();
  waveout_flush();

  emu_running = 0;
  emu_paused = 0;
  auto_paused_by_focus = 0;
  undo_valid = 0;
  rewind_reset();
  rom_path[0] = '\0';
  rom_base[0] = '\0';
  rom_id[0] = '\0';
  rom_console = "";

  update_title();
  gui_update_menu();
  gui_status_persistent("Open a ROM to start, or drop one on this window");

  /* Back to the window size the Video > Window Size setting asks for, rather
     than leaving the window at whatever size the game's own picture (a
     different aspect ratio, NTSC filter width, or a manual drag) happened to
     leave it at -- video_preferred_size() already gives the plain 320x224
     base size once nothing is running, so this lands on the same size a
     fresh launch with no ROM would. */
  gui_resize_to_scale(gui.scale);

  {
    RECT content;
    get_content_rect(&content);
    browser_panel_layout(&content);
    browser_panel_show(1);
  }

  InvalidateRect(g_hwnd, NULL, TRUE);
}

int emu_load_rom(const char *path)
{
  char attempt[GUI_PATH_LEN];

  if (!path || !path[0]) return 0;

  lstrcpynA(attempt, path, sizeof(attempt));

  /* Checked before the current game is closed: a missing file, a folder, or a
     file that is not a game type must not end the game that is running. */
  {
    DWORD attr = GetFileAttributesA(attempt);

    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
      char msg[GUI_PATH_LEN + 96];
      wsprintfA(msg, "Could not find:\n\n%s", attempt);
      MessageBoxA(g_hwnd, msg, APP_NAME, MB_OK | MB_ICONWARNING);
      return 0;
    }

    if (emu_running && !browser_console_for_path(attempt))
    {
      if (MessageBoxA(g_hwnd,
            "That does not look like a game file (Mega Drive, Master System, Game Gear,\n"
            "SG-1000 or Mega CD, or a .zip / .gz holding one).\n\n"
            "Close the current game and try to open it anyway?",
            APP_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
        return 0;
    }
  }

  emu_close_rom();

  if (!load_rom(attempt))
  {
    char msg[GUI_PATH_LEN + 96];
    wsprintfA(msg, "Could not load:\n\n%s\n\n"
                   "The file may be missing, unreadable, or not a supported ROM.",
              attempt);
    MessageBoxA(g_hwnd, msg, APP_NAME, MB_OK | MB_ICONWARNING);
    gui_status("Loading failed");
    return 0;
  }

  lstrcpynA(rom_path, attempt, sizeof(rom_path));
  split_basename(rom_path, rom_base, sizeof(rom_base));
  identify_rom();
  adopt_legacy_files();
  np_no_save = 0;
  np_rom_sha_ready = 0;

  audio_init(gui.sample_rate, 0);
  system_init();

  load_backup_ram();

  emu_running = 1;
  emu_paused = 0;
  browser_panel_show(0);
  rewind_reset();

  /* Set the audio format up before the reset so the chips come out of reset
     already matched to the output rate. */
  emu_apply_audio_settings();
  system_reset();
  emu_apply_sms_border();

  cheats_load_for_rom(rom_base);
  video_viewport_changed();

  config_add_recent(rom_path);
  config_save();

  rebuild_recent_menu();
  update_title();
  gui_update_menu();
  gui_resize_to_scale(gui.scale);
  gui_status_persistent("Running %s", rom_base);

  if (gui.fullscreen_on_load && !gui.fullscreen)
  {
    video_set_fullscreen(1);
    gui_update_menu();
  }

  next_frame_time = 0.0;
  return 1;
}

/* For ROM Information / Edit Cheats from the browser's context menu on
   a ROM that isn't the one currently running: populates rominfo and
   loads that ROM's cheat list for viewing/editing, without starting
   emulation -- no system_init/system_reset, no audio, emu_running
   stays 0, and the browser panel is left exactly as it was. This is
   only safe because nothing is running yet: load_rom() overwrites the
   same cart.rom buffer a live game would be using, so calling this
   while a *different* ROM is actually playing would corrupt it out
   from under it. Returns 0 without doing anything if a game is
   currently running -- callers fall back to emu_load_rom() then,
   which is the only safe option in that case. */
int emu_peek_rom(const char *path)
{
  char attempt[GUI_PATH_LEN];

  if (emu_running) return 0;
  if (!path || !path[0]) return 0;

  lstrcpynA(attempt, path, sizeof(attempt));

  emu_close_rom();   /* no-op given the emu_running guard above, but mirrors emu_load_rom's own pattern */

  if (!load_rom(attempt)) return 0;

  lstrcpynA(rom_path, attempt, sizeof(rom_path));
  split_basename(rom_path, rom_base, sizeof(rom_base));
  identify_rom();

  cheats_load_for_rom(rom_base);

  return 1;
}

const char *emu_rom_filename(void)
{
  return rom_base;
}

const char *emu_rom_path(void)
{
  return rom_path;
}

void emu_reset(int hard)
{
  if (!emu_running) return;

  cheats_suspend();

  if (hard)
  {
    rewind_reset();      /* the machine starts over: earlier history no longer fits */
    save_backup_ram();
    system_init();
    load_backup_ram();
    np_no_save = 0;         /* backup RAM now matches the real save files again */
  }

  system_reset();

  emu_apply_sms_border();
  cheats_apply();
  waveout_flush();
  gui_notify(hard ? "Hard reset" : "Reset");
}

/****************************************************************************
 * Open dialog
 ****************************************************************************/

static int is_directory(const char *path)
{
  DWORD attr;

  if (!path || !path[0]) return 0;
  attr = GetFileAttributesA(path);
  return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void browse_for_rom(void)
{
  char file[GUI_PATH_LEN] = "";
  const char *start_dir = NULL;

  /* Always start in the ROM directory chosen for the browser, no matter
     where the last ROM was picked from. */
  if (is_directory(gui.rom_dir)) start_dir = gui.rom_dir;

  if (gui_pick_file(g_hwnd, GUI_PICK_ROM, "Open ROM",
        "All supported files\0*.zip;*.gz;*.md;*.gen;*.bin;*.smd;*.mdx;*.sms;*.gg;*.sg;*.68k;*.cue;*.iso;*.chd\0"
        "Mega Drive / Genesis\0*.md;*.gen;*.bin;*.smd;*.mdx;*.68k\0"
        "Master System / Game Gear / SG-1000\0*.sms;*.gg;*.sg\0"
        "Mega CD / Sega CD\0*.cue;*.iso;*.chd\0"
        "Archives\0*.zip;*.gz\0"
        "All files\0*.*\0\0",
        start_dir, file, sizeof(file)))
  {
    emu_load_rom(file);
  }
}

/****************************************************************************
 * Emulation
 ****************************************************************************/

/****************************************************************************
 * Netplay glue -- everything netplay.c needs to know about the emulator
 ****************************************************************************/

static int np_preflight(char *err, int err_len)
{
  if (!emu_running)
  {
    lstrcpynA(err, "Load a game first.", err_len);
    return 0;
  }
  if (system_hw == SYSTEM_MCD)
  {
    lstrcpynA(err, "Netplay supports cartridge games only, not Sega CD.", err_len);
    return 0;
  }
  if (cheats_effective() > 0)
  {
    lstrcpynA(err, "Turn off your cheats first (Tools > Cheats). They would put the two games out of sync.", err_len);
    return 0;
  }
  if (gui_input_p2_slot() < 0)
  {
    lstrcpynA(err, "Netplay needs a plain Control Pad on both ports (Input > Port A / Port B Device).", err_len);
    return 0;
  }
  return 1;
}

static void np_add(np_info *info, const char *name, unsigned value)
{
  if (info->n_settings < NP_MAX_SETTINGS)
  {
    info->name[info->n_settings] = name;
    info->value[info->n_settings] = value;
    info->n_settings++;
  }
}

static void np_collect(np_info *info)
{
  /* Anything here that differs between the two PCs changes what the game
     does, so the connection is refused (and the name shown) if it does. */
  if (!np_rom_sha_ready)
  {
    np_sha256(cart.rom, (size_t)cart.romsize, np_rom_sha);
    np_rom_sha_ready = 1;
  }
  memcpy(info->rom_sha, np_rom_sha, 32);

  np_add(info, "Console",             config.system);
  np_add(info, "Region",              config.region_detect);
  np_add(info, "Detected region",     region_code);
  np_add(info, "TV standard (PAL)",   vdp_pal);
  np_add(info, "System hardware",     system_hw);
  np_add(info, "Force VDP mode",      config.vdp_mode);
  np_add(info, "Master clock",        config.master_clock);
  np_add(info, "DTACK",               config.force_dtack);
  np_add(info, "Address errors",      config.addr_error);
  np_add(info, "Boot from BIOS",      config.bios);
  np_add(info, "Lock-on cartridge",   config.lock_on);
  np_add(info, "Add-on",              config.add_on);
  np_add(info, "Hot swap",            config.hot_swap);
  np_add(info, "FM chip",             (unsigned)config.ym2612 | ((unsigned)config.ym3438 << 8) |
                                      ((unsigned)config.ym2413 << 16) | ((unsigned)config.opll << 24));
  np_add(info, "Nuked FM chip type",  (unsigned)gui.nuked_ym2612);
  np_add(info, "High-quality PSG",    config.hq_psg);
  np_add(info, "Sample rate",         (unsigned)snd.sample_rate);
  np_add(info, "Port A device",       input.system[0]);
  np_add(info, "Port B device",       input.system[1]);
  np_add(info, "Pad type, player 1",  config.input[0].padtype);
  np_add(info, "Pad type, player 2",  config.input[1].padtype);
}

static void np_begin(void)
{
  /* Both machines start from the same power-on state. The backup RAM comes
     out at its defaults (nothing is loaded from disk), and saving is blocked
     until a game is loaded or hard-reset normally. */
  cheats_suspend();
  system_init();
  system_reset();
  emu_apply_sms_border();
  waveout_flush();
  emu_paused = 0;
  auto_paused_by_focus = 0;
  auto_paused_by_minimize = 0;
  np_no_save = 1;
  rewind_reset();
  next_frame_time = 0.0;

  gui_update_menu();
  status_apply_prefixes();
}

static void np_end(void)
{
  gui_update_menu();
  status_apply_prefixes();
}

/* FNV-1a over the whole machine state. The buffer is cleared first so bytes the
   save code doesn't write (padding) hash the same on both sides. */
static unsigned long long np_state_hash(void)
{
  static uint8 *buf;
  unsigned long long h = 14695981039346656037ULL;
  int i, len;

  if (!buf) buf = (uint8 *)malloc(STATE_SIZE);
  if (!buf) return 0;

  memset(buf, 0, STATE_SIZE);
  len = state_save(buf);
  for (i = 0; i < len; i++)
  {
    h ^= buf[i];
    h *= 1099511628211ULL;
  }
  return h;
}

static const char *np_game_title(void)
{
  return rom_base;
}

/* Frames per second the emulated console really runs at: master clock over
   the length of a frame (59.92 Hz NTSC, 49.70 Hz PAL, or whatever Force VDP
   Mode / Force Master Clock make of it). snd.frame_rate is not usable for this:
   the GUI starts the audio with a frame rate of 0, which leaves it at 0. */
double emu_frame_rate(void)
{
  if (lines_per_frame < 1 || system_clock < 1) return 60.0;
  return (double)system_clock / ((double)MCYCLES_PER_LINE * (double)lines_per_frame);
}

/* File name of the loaded game without folder or extension. */
const char *emu_rom_base(void)
{
  return rom_base;
}

/* Problems the person has to read are shown from emulation_step(), between
   frames. netplay code can report one from inside the core's frame (the input
   callback), and a modal dialog there would run the message loop -- repaints
   and all -- in the middle of an emulated frame. */
static char np_pending_msg[300];

static void np_message(const char *text, int level)
{
  if (level) lstrcpynA(np_pending_msg, text, sizeof(np_pending_msg));
  else       gui_notify("%s", text);
  status_apply_prefixes();
}

static const np_host_t np_iface =
{
  np_preflight, np_collect, np_begin, np_end, np_state_hash, np_game_title, np_message
};

/* Things that would change the game's state on one PC only. Blocked while a
   session is running. */
static int np_command_locked(int id)
{
  if (!netplay_active()) return 0;

  if (id == IDM_EMU_PAUSE || id == IDM_EMU_RESET || id == IDM_EMU_HARDRESET) return 1;
  if (id == IDM_FILE_SAVESTATE || id == IDM_FILE_LOADSTATE || id == IDM_FILE_STATEMGR) return 1;
  if (id == IDM_EMU_UNDOLOAD) return 1;
  if (id >= IDM_ACCEL_QUICKSAVE && id <= IDM_ACCEL_SLOT_PREV) return 1;
  if (id >= IDM_FILE_SLOT_BASE && id < IDM_FILE_SLOT_BASE + GUI_SLOT_MAX) return 1;
  if (id == IDM_EMU_CHEATS) return 1;
  if (id >= IDM_EMU_REGION_BASE && id < IDM_EMU_REGION_BASE + 5) return 1;
  if (id >= IDM_EMU_VDPMODE_BASE && id < IDM_EMU_VDPMODE_BASE + 3) return 1;
  if (id >= IDM_EMU_SYSTEM_BASE && id < IDM_EMU_SYSTEM_BASE + 9) return 1;
  if (id >= IDM_EMU_LOCKON_BASE && id < IDM_EMU_LOCKON_BASE + 4) return 1;
  if (id == IDM_EMU_BIOS || id == IDM_EMU_ADDRERROR) return 1;
  if (id == IDM_AUDIO_HQPSG) return 1;       /* re-creates the PSG: would put the two games out of step */
  if (id >= IDM_AUDIO_FMCORE_BASE && id < IDM_AUDIO_FMCORE_BASE + 5) return 1;
  if (id == IDM_AUDIO_ADVANCED) return 1;   /* changes the FM chip type */
  if (id >= IDM_AUDIO_RATE_BASE && id < IDM_AUDIO_RATE_BASE + 2) return 1;
  if (id == IDM_INPUT_P1 || id == IDM_INPUT_P2) return 1;
  if (id >= IDM_INPUT_PORTA_BASE && id < IDM_INPUT_PORTA_BASE + 10) return 1;
  if (id >= IDM_INPUT_PORTB_BASE && id < IDM_INPUT_PORTB_BASE + 10) return 1;
  return 0;
}

/****************************************************************************
 * Recording
 ****************************************************************************/

static void record_stop(void)
{
  char path[GUI_PATH_LEN], err[256];
  const char *name;
  int failed;

  if (!recorder_active()) return;

  failed = recorder_failed();
  lstrcpynA(path, recorder_path(), sizeof(path));
  lstrcpynA(err, recorder_error(), sizeof(err));

  recorder_stop();

  status_apply_prefixes();
  gui_update_menu();

  name = strrchr(path, '\\');
  name = name ? name + 1 : path;

  if (failed)
  {
    char msg[GUI_PATH_LEN + 384];
    wsprintfA(msg, "Recording stopped: %s\n\nWhat was recorded up to that point was kept in:\n%s", err, path);
    MessageBoxA(g_hwnd, msg, APP_NAME, MB_OK | MB_ICONWARNING);
  }
  else
  {
    gui_notify("Recording saved: %s", name);
  }
}

static void record_start(int mode)
{
  char path[GUI_PATH_LEN];
  SYSTEMTIME st;

  if (!emu_running || recorder_active()) return;

  ensure_dir("recordings");
  GetLocalTime(&st);
  gui_snprintf(path, (int)sizeof(path), "%s\\%s_%04d%02d%02d_%02d%02d%02d.%s",
            osd_path("recordings"), rom_base,
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            (mode == REC_VIDEO) ? "mp4" : "wav");

  if (!recorder_start(mode, path, (int)snd.sample_rate, emu_frame_rate()))
  {
    char msg[512];
    wsprintfA(msg, "Could not start recording:\n\n%s", recorder_error());
    MessageBoxA(g_hwnd, msg, APP_NAME, MB_OK | MB_ICONWARNING);
    return;
  }

  gui_update_menu();
  gui_notify("Recording %s", (mode == REC_VIDEO) ? "video + audio" : "audio only");
}

/* `record`: this is a normal-speed frame, so it belongs in a recording.
   Fast-forward and frame-advance frames are left out, which keeps the picture
   and sound continuous instead of leaving gaps or sped-up sections. */
static double timer_lag_frames;   /* how far behind schedule, in frames; only used when sound is off */

/* Waiting for the display's refresh (VSync) can take up to a whole refresh.
   Only allowed when there is room for that: at least two frames of sound queued
   (or, with sound off, the frame timer not running behind). */
static void set_vsync_allowed(void)
{
  int ok;

  if (gui.sound_enabled && !waveout_broken()) ok = (waveout_pending() >= 2);
  else                   ok = (timer_lag_frames < 0.5);
  video_set_vsync_allowed(ok);
}

/****************************************************************************
 * Run-ahead
 *
 * Lowers input lag: after each real frame the game is run a few frames ahead
 * with the current controller state, and it is the last of those frames that
 * gets drawn, so a button press shows up sooner. Then the state from just
 * after the real frame is put back, so the game itself only ever advances one
 * frame at a time. Sound comes from the real frames.
 *
 * The extra frames must not touch anything that survives the reload:
 *  - their audio goes to a scratch blip buffer (the real one keeps its
 *    position); state_load() itself clears whichever buffer is installed,
 *    which is why the scratch one stays installed until the load is done;
 *  - the audio filter memory, which state_load() also clears, is saved and put
 *    back (audio_filter_context_*);
 *  - the viewport is restored, since a reload resets it (Master System border
 *    crop included).
 * Not used during netplay or recording, with cheats on (ROM patches don't
 * survive a reload), or for Sega CD / cartridges with their own audio hardware.
 ****************************************************************************/

static uint8  *ra_state;
static blip_t *ra_blip;
static int     ra_blip_rate;
static int     ra_failed;
static int16   ra_audio[4096];

static int run_ahead_wanted(void)
{
  if (!gui.runahead || ra_failed) return 0;
  if (netplay_active() || recorder_active()) return 0;
  if (system_hw == SYSTEM_MCD || snd.blips[3] || !snd.blips[0]) return 0;
  if (cheats_effective() > 0) return 0;
  return gui.runahead;
}

static void emulate_one_frame(void)
{
  cheats_ram_update();

  if (system_hw == SYSTEM_MCD)              system_frame_scd(0);
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) system_frame_gen(0);
  else                                      system_frame_sms(0);

  if (bitmap.viewport.changed & 1)
  {
    bitmap.viewport.changed &= ~1;
    video_viewport_changed();
  }
}

/* Returns 1 if it ran (and presented) the frame, 0 if run-ahead isn't usable. */
static int run_ahead_frame(void)
{
  int n = run_ahead_wanted();
  int i, samples, len;
  blip_t *real;
  uint8 filter_ctx[512];
  typeof(bitmap.viewport) vp;

  if (!n) return 0;
  if (audio_filter_context_size() > (int)sizeof(filter_ctx)) return 0;

  if (!ra_state) ra_state = (uint8 *)malloc(STATE_SIZE);
  if (!ra_state) return 0;

  /* The scratch blip buffer must match the real one's rates. */
  if (!ra_blip || ra_blip_rate != (int)snd.sample_rate)
  {
    if (ra_blip) blip_delete(ra_blip);
    ra_blip = blip_new((int)snd.sample_rate / 10);
    ra_blip_rate = (int)snd.sample_rate;
    if (!ra_blip) return 0;
    blip_set_rates(ra_blip, (double)system_clock, (double)snd.sample_rate);
  }

  /* 1. The real frame, with its sound. */
  emulate_one_frame();
  samples = audio_update(soundframe);
  if (gui.sound_enabled && waveout_pending() < gui.latency)
  {
    waveout_submit(soundframe, samples);
  }

  /* 2. Remember where the real game is. */
  len = state_save(ra_state);
  audio_filter_context_save(filter_ctx);
  vp = bitmap.viewport;

  /* 3. Run ahead, sending the sound to the scratch buffer. */
  real = snd.blips[0];
  snd.blips[0] = ra_blip;
  blip_clear(ra_blip);

  for (i = 0; i < n; i++)
  {
    emulate_one_frame();
    audio_update(ra_audio);
  }

  /* 4. Draw the last of them. */
  set_vsync_allowed();
  video_frame();

  /* 5. Put the real game back. */
  if (len <= 0 || !state_load(ra_state))
  {
    snd.blips[0] = real;
    ra_failed = 1;
    gui_notify("Run-ahead turned off: could not restore the game state");
  }
  else
  {
    snd.blips[0] = real;
    audio_filter_context_load(filter_ctx);
    bitmap.viewport = vp;
  }

  frames_this_second++;
  rewind_capture_tick(soundframe, samples);
  return 1;
}

static int skip_render;      /* run this frame without drawing it (frameskip) */

static void run_one_frame(int present_video, int record)
{
  int samples;

  /* Normal-speed frames only: not fast-forward, frame advance or a skipped frame. */
  if (record && present_video && !skip_render && run_ahead_frame()) return;

  /* RAM patches are overwritten by the game, so they go back in each frame. */
  cheats_ram_update();

  if (system_hw == SYSTEM_MCD)              system_frame_scd(skip_render);
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) system_frame_gen(skip_render);
  else                                      system_frame_sms(skip_render);

  if (skip_render) present_video = 0;

  if (bitmap.viewport.changed & 1)
  {
    bitmap.viewport.changed &= ~1;
    video_viewport_changed();
  }

  /* audio_update must run every frame or the blip buffers overrun, even
     when the samples are then thrown away. */
  samples = audio_update(soundframe);

  if (gui.sound_enabled && waveout_pending() < gui.latency)
  {
    waveout_submit(soundframe, samples);
  }

  if (record && recorder_active())
  {
    recorder_capture(soundframe, samples);
    if (recorder_failed()) record_stop();
  }

  if (present_video)
  {
    set_vsync_allowed();
    video_frame();
  }

  frames_this_second++;

  /* A frame that was skipped was never drawn, so its picture is stale. */
  if (!skip_render) rewind_capture_tick(soundframe, samples);
}

static double now_seconds(void)
{
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return (double)c.QuadPart / (double)perf_freq.QuadPart;
}

static void tick_fps(void)
{
  DWORD now = GetTickCount();

  if (now - fps_tick >= 1000)
  {
    int fps = (int)(((DWORD)frames_this_second * 1000) / (now - fps_tick));
    video_report_fps(fps);
    status_set_fps(fps);
    frames_this_second = 0;
    fps_tick = now;

    if (recorder_active() || netplay_state() != NP_IDLE) status_apply_prefixes();
  }
}

/* Shows or hides the menu bar while fullscreen, called from the Esc
   accelerator (IDM_ACCEL_LEAVE_FULLSCREEN) below. */
static void toggle_fullscreen_menu(void)
{
  int showing = (GetMenu(g_hwnd) != NULL);

  SetMenu(g_hwnd, showing ? NULL : g_menu);

  if (showing)
  {
    /* Menu bar just got detached. Every way of asking Windows to redraw
       the non-client area on its own (DrawMenuBar()+InvalidateRect(),
       RedrawWindow() with RDW_FRAME, an actual resize-and-restore)
       failed to reliably clear it in testing -- painting directly over
       where it was doesn't depend on any of that, it's just an ordinary
       GDI fill like every other frame in this app already does. */
    RECT rc;
    HDC hdc = GetDC(NULL);

    if (hdc)
    {
      GetWindowRect(g_hwnd, &rc);
      rc.bottom = rc.top + GetSystemMetrics(SM_CYMENU);
      FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
      ReleaseDC(NULL, hdc);
    }
  }

  /* Fullscreen hides the cursor (repeated ShowCursor(FALSE) calls, since
     its internal counter needs to go negative to actually hide) --
     showing the menu bar without also bringing the cursor back left no
     way to see where a click would land. */
  if (showing) while (ShowCursor(FALSE) >= 0) { }
  else          while (ShowCursor(TRUE) < 0) { }

  video_invalidate();
  InvalidateRect(g_hwnd, NULL, TRUE);
}

/* Fullscreen with the menu bar brought up (Esc): the cursor has to stay
   visible while it is being used, but shouldn't sit on the picture forever.
   Hides it after a few idle seconds, shows it again on any mouse movement. With
   the menu bar hidden, fullscreen already keeps the cursor hidden by itself. */
#define CURSOR_IDLE_MS 3000

static void tick_cursor_autohide(void)
{
  static POINT last;
  static DWORD last_move;
  static int hidden;
  POINT p;
  DWORD now = GetTickCount();

  if (!gui.fullscreen || !GetMenu(g_hwnd))
  {
    hidden = 0;            /* the normal show/hide logic owns the cursor */
    last_move = now;
    return;
  }

  if (in_modal_loop)       /* a menu or dialog is open: the person is using the mouse */
  {
    last_move = now;
    if (hidden) { while (ShowCursor(TRUE) < 0) { } hidden = 0; }
    return;
  }

  if (GetCursorPos(&p) && (p.x != last.x || p.y != last.y))
  {
    last = p;
    last_move = now;
    if (hidden) { while (ShowCursor(TRUE) < 0) { } hidden = 0; }
  }
  else if (!hidden && now - last_move >= CURSOR_IDLE_MS)
  {
    while (ShowCursor(FALSE) >= 0) { }
    hidden = 1;
  }
}

static void tick_status_expiry(void)
{
  DWORD now = GetTickCount();

  if (status_transient_until && now >= status_transient_until)
  {
    status_transient_until = 0;
    status_apply_prefixes();
  }
}

/* Frameskip. The sound card sets the pace, so "running behind" shows up as its
   queue running low: skipping the drawing of a frame (the emulation and the
   sound still run) gives the machine time to catch up. Auto skips when the
   queue is nearly empty; Manual when it falls below 25/33/50/75%. At most four
   frames in a row, so the picture keeps moving. Never during netplay (both
   sides must run identical frames) or while recording (it would repeat frames). */
static int frameskip_wanted(void)
{
  static const int threshold[6] = { 0, 25, 25, 33, 50, 75 };
  static int in_a_row;
  int behind;

  if (!gui.frameskip || netplay_active() || recorder_active())
  {
    in_a_row = 0;
    return 0;
  }

  if (gui.sound_enabled && !waveout_broken() && gui.latency > 0)
    behind = (waveout_pending() * 100 / gui.latency) < threshold[gui.frameskip];
  else
    behind = timer_lag_frames > 1.0;

  if (behind && in_a_row < 4)
  {
    in_a_row++;
    return 1;
  }
  in_a_row = 0;
  return 0;
}

/* Sound normally sets the pace. If the device disappears (headset unplugged,
   driver reset) it would otherwise either stall the game or let it run flat
   out, so the frame timer takes over until the device can be opened again.
   Returns 1 while the timer should be used. */
static int audio_pacing_lost(void)
{
  static DWORD last_try;
  DWORD now;

  if (!gui.sound_enabled || !waveout_broken()) return 0;

  now = GetTickCount();
  if (now - last_try > 2000)
  {
    last_try = now;
    waveout_close();
    if (waveout_open(gui.sample_rate))
    {
      waveout_set_volume(gui.volume);
      return 0;
    }
  }
  return 1;
}

/* Buffers are queued but the device is not finishing any. */
static void audio_recover(void)
{
  static DWORD last;
  static int stage;
  DWORD now = GetTickCount();

  if (now - last > 10000) stage = 0;
  last = now;

  if (stage++ == 0)
  {
    waveout_flush();                       /* first try: just clear the queue */
  }
  else
  {
    waveout_close();                       /* then reopen the device */
    if (waveout_open(gui.sample_rate)) waveout_set_volume(gui.volume);
    else gui_status("The sound device is not responding; running without it for now");
  }
}

static void emulation_step(void)
{
  int fast, i, count;
  DWORD now_ms = GetTickCount();

  tick_status_expiry();
  tick_cursor_autohide();
  backup_ram_autosave(now_ms);

  /* Accept a joining player, read the socket, run the handshake. */
  netplay_poll();

  if (np_pending_msg[0])
  {
    char text[300];

    lstrcpynA(text, np_pending_msg, sizeof(text));
    np_pending_msg[0] = '\0';
    MessageBoxA(g_hwnd, text, APP_NAME, MB_OK | MB_ICONWARNING);
    next_frame_time = 0.0;
  }

  if (emu_running && !in_modal_loop && !netplay_active())
  {
    int slot;

    slot = gui_input_save_slot_shortcut();
    if (slot >= 0)
    {
      gui.state_slot = slot;
      emu_save_state(slot);      /* reports its own result */
      gui_update_menu();
    }

    slot = gui_input_load_slot_shortcut();
    if (slot >= 0)
    {
      gui.state_slot = slot;
      emu_load_state(slot);      /* reports its own result */
      gui_update_menu();
    }
  }

  /* A session can't pause: the other player's game keeps waiting on this one. */
  if (netplay_active() && emu_paused)
  {
    emu_paused = 0;
    gui_update_menu();
  }

  if (!emu_running || emu_paused || in_modal_loop)
  {
    if (emu_running && emu_paused && !netplay_active() && gui_input_frame_advance())
    {
      run_one_frame(1, 0);
      return;
    }

    MsgWaitForMultipleObjects(0, NULL, FALSE, 16, QS_ALLINPUT);
    return;
  }

  if (gui.rewind && !netplay_active() && gui_input_rewind())
  {
    /* Matches normal playback's own cadence -- snapshots are now captured
       every single frame (see rewind.c), so popping one every 60th of a
       second reads as real-time-speed reverse motion instead of jumping. */
    double now = now_seconds();
    double period = 1.0 / 60.0;

    if (next_rewind_time == 0.0 || now - next_rewind_time > 0.5)
    {
      next_rewind_time = now;
    }
    else if (now < next_rewind_time)
    {
      MsgWaitForMultipleObjects(0, NULL, FALSE, 1, QS_ALLINPUT);
      return;
    }
    next_rewind_time += period;

    if (rewind_step())
    {
      int audio_frames;
      const int16 *audio;

      rewind_hit_limit_notified = 0;

      /* rewind_step() already restored bitmap.data directly -- no
         forward frame needed to regenerate it, just present what's
         there. */
      video_frame();

      /* Reversed sample order, not forward playback -- this is what
         actually produces the characteristic reversed-sound rewind
         effect, matching the reversed picture. */
      audio = rewind_get_audio(&audio_frames);
      if (gui.sound_enabled && audio_frames > 0 && waveout_pending() < gui.latency)
      {
        waveout_submit(audio, audio_frames);
      }
    }
    else if (!rewind_hit_limit_notified)
    {
      rewind_hit_limit_notified = 1;
      gui_notify("Rewind limit reached");
    }

    next_frame_time = 0.0;
    tick_fps();
    return;
  }

  fast = netplay_active() ? 0 : gui_input_fast_forward();

  if (!fast)
  {
    /* Lockstep: wait for the other player's input for this frame *before*
       touching the frame timer, or every stall would push the schedule
       forward without a frame being run and the game would slow down. */
    if (netplay_active() && !netplay_frame_ready())
    {
      HANDLE h = netplay_wait_handle();

      if (h) MsgWaitForMultipleObjects(1, &h, FALSE, 4, QS_ALLINPUT);
      else   MsgWaitForMultipleObjects(0, NULL, FALSE, 4, QS_ALLINPUT);
      return;
    }

    if (gui.sound_enabled && !audio_pacing_lost())
    {
      /* Let the sound card set the pace. */
      if (waveout_pending() >= gui.latency)
      {
        if (waveout_stalled()) audio_recover();
        MsgWaitForMultipleObjects(0, NULL, FALSE, 1, QS_ALLINPUT);
        return;
      }
    }
    else
    {
      double now = now_seconds();
      double period = 1.0 / emu_frame_rate();

      if (next_frame_time == 0.0 || now - next_frame_time > 0.5)
      {
        next_frame_time = now;
      }
      else if (now < next_frame_time)
      {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 1, QS_ALLINPUT);
        return;
      }
      timer_lag_frames = (now - next_frame_time) / period;
      next_frame_time += period;
    }

    if (netplay_active())
    {
      /* Readiness was checked above. */
      netplay_frame_begin();
      run_one_frame(1, 1);
      netplay_frame_end();
    }
    else
    {
      skip_render = run_ahead_wanted() ? 0 : frameskip_wanted();
      run_one_frame(1, 1);
      skip_render = 0;
    }
  }
  else
  {
    /* Draw one frame in every fast_forward_ratio to keep the window alive. */
    count = gui.fast_forward_ratio;
    for (i = 0; i < count; i++)
    {
      run_one_frame(i == count - 1, 0);
    }
    next_frame_time = 0.0;
  }

  tick_fps();
}

/****************************************************************************
 * Commands
 ****************************************************************************/

static void on_command(int id)
{
  if (np_command_locked(id))
  {
    gui_notify("Not available during netplay");
    return;
  }

  /* Ranged commands first. */
  if (id >= IDM_FILE_RECENT_BASE && id < IDM_FILE_RECENT_BASE + GUI_RECENT_MAX)
  {
    int i = id - IDM_FILE_RECENT_BASE;
    if (gui.recent[i][0])
    {
      char path[GUI_PATH_LEN];
      lstrcpynA(path, gui.recent[i], sizeof(path));
      emu_load_rom(path);
    }
    return;
  }

  if (id >= IDM_FILE_SLOT_BASE && id < IDM_FILE_SLOT_BASE + GUI_SLOT_MAX)
  {
    gui.state_slot = id - IDM_FILE_SLOT_BASE;
    gui_update_menu();
    gui_notify("Slot %d selected", gui.state_slot);
    config_save();
    return;
  }

  /* Force VDP Mode: run the video timing as NTSC (60 Hz) or PAL (50 Hz) whatever
     the region says. Applied the same way as a region change. */
  if (id >= IDM_EMU_VDPMODE_BASE && id < IDM_EMU_VDPMODE_BASE + 3)
  {
    config.vdp_mode = (uint8)(id - IDM_EMU_VDPMODE_BASE);
    emu_apply_region();
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_EMU_REGION_BASE && id < IDM_EMU_REGION_BASE + 5)
  {
    config.region_detect = (uint8)(id - IDM_EMU_REGION_BASE);
    emu_apply_region();
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_EMU_SYSTEM_BASE && id < IDM_EMU_SYSTEM_BASE + 9)
  {
    static const uint8 systems[] =
    {
      0, SYSTEM_SG, SYSTEM_SGII, SYSTEM_SGII_RAM_EXT, SYSTEM_MARKIII,
      SYSTEM_SMS, SYSTEM_SMS2, SYSTEM_GG, SYSTEM_MD
    };
    config.system = systems[id - IDM_EMU_SYSTEM_BASE];
    gui_update_menu();
    config_save();
    gui_status("Console setting applies the next time a ROM is loaded");
    return;
  }

  if (id >= IDM_EMU_LOCKON_BASE && id < IDM_EMU_LOCKON_BASE + 4)
  {
    config.lock_on = (uint8)(id - IDM_EMU_LOCKON_BASE);
    gui_update_menu();
    config_save();
    gui_status("Lock-on cartridge applies the next time a ROM is loaded");
    return;
  }

  if (id >= IDM_VIDEO_SCALE_BASE && id < IDM_VIDEO_SCALE_BASE + 6)
  {
    gui.scale = id - IDM_VIDEO_SCALE_BASE + 1;
    if (gui.fullscreen) video_set_fullscreen(0);
    gui_resize_to_scale(gui.scale);
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_VIDEO_ASPECT_BASE && id < IDM_VIDEO_ASPECT_BASE + 3)
  {
    gui.aspect = id - IDM_VIDEO_ASPECT_BASE;
    video_viewport_changed();
    gui_resize_to_scale(gui.scale);
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_VIDEO_NTSC_BASE && id < IDM_VIDEO_NTSC_BASE + 4)
  {
    video_set_ntsc(id - IDM_VIDEO_NTSC_BASE);
    gui_update_menu();
    rebuild_filter_menu();   /* NTSC and a render filter are mutually exclusive */
    config_save();
    return;
  }

  if (id >= IDM_VIDEO_FILTER_BASE && id < IDM_VIDEO_FILTER_BASE + FILTER_MENU_MAX)
  {
    int i = id - IDM_VIDEO_FILTER_BASE;
    if (i < video_filter_count())
    {
      video_set_filter(i);
      rebuild_filter_menu();
      config_save();
    }
    return;
  }

  if (id >= IDM_TOOLS_SHOT_BASE && id < IDM_TOOLS_SHOT_BASE + 3)
  {
    gui.shot_mode = id - IDM_TOOLS_SHOT_BASE;
    gui_update_menu();
    config_save();
    return;
  }

  /* Interlaced Mode: single field, or double field (both fields, twice the height). */
  if (id >= IDM_VIDEO_INTERLACE_BASE && id < IDM_VIDEO_INTERLACE_BASE + 2)
  {
    config.render = (uint8)(id - IDM_VIDEO_INTERLACE_BASE);
    if (emu_running) bitmap.viewport.changed = 3;
    video_viewport_changed();
    gui_update_menu();
    config_save();
    return;
  }

  /* LCD ghosting: how much of a bright pixel lingers into the next frame. */
  if (id >= IDM_VIDEO_LCD_BASE && id < IDM_VIDEO_LCD_BASE + 4)
  {
    static const uint8 lcd_rate[4] = { 0, 64, 128, 192 };
    config.lcd = lcd_rate[id - IDM_VIDEO_LCD_BASE];
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_EMU_RUNAHEAD_BASE && id < IDM_EMU_RUNAHEAD_BASE + 4)
  {
    gui.runahead = id - IDM_EMU_RUNAHEAD_BASE;
    ra_failed = 0;
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_VIDEO_FRAMESKIP_BASE && id < IDM_VIDEO_FRAMESKIP_BASE + 6)
  {
    gui.frameskip = id - IDM_VIDEO_FRAMESKIP_BASE;
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_VIDEO_OVERSCAN_BASE && id < IDM_VIDEO_OVERSCAN_BASE + 4)
  {
    config.overscan = (uint8)(id - IDM_VIDEO_OVERSCAN_BASE);
    emu_apply_sms_border();
    if (emu_running) bitmap.viewport.changed = 3;
    video_viewport_changed();
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_VIDEO_SCANLINE_BASE && id < IDM_VIDEO_SCANLINE_BASE + 5)
  {
    gui.scanline_pct = (id - IDM_VIDEO_SCANLINE_BASE) * 25;
    video_invalidate();
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_VIDEO_THEME_BASE && id < IDM_VIDEO_THEME_BASE + 3)
  {
    theme_set_mode(id - IDM_VIDEO_THEME_BASE);
    theme_apply_to_window(g_hwnd);
    gui_update_menu();
    config_save();
    return;
  }

  if (id == IDM_VIEW_LIST || id == IDM_VIEW_GRID)
  {
    gui.browser_grid_view = (id == IDM_VIEW_GRID);
    browser_apply_view_mode();
    gui_update_menu();
    config_save();
    return;
  }

  if (id == IDM_VIDEO_LARGE_UI)
  {
    gui.large_ui = !gui.large_ui;
    gui_update_menu();
    config_save();

    if (MessageBoxA(g_hwnd,
          "Genesis Plus GX needs to restart for this to take effect.\n\nRestart now?",
          "Larger UI", MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
      char exe_path[GUI_PATH_LEN];
      char cmdline[GUI_PATH_LEN * 2 + 16];
      STARTUPINFOA si;
      PROCESS_INFORMATION pi;

      GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));

      /* Only hand the new instance a ROM if one is actually running. rom_path
         is also set by ROM Information / Edit Cheats on a game that was only
         inspected (emu_peek_rom), and that must not launch it. */
      if (emu_running && rom_path[0])
        wsprintfA(cmdline, "\"%s\" \"%s\"", exe_path, rom_path);
      else
        wsprintfA(cmdline, "\"%s\"", exe_path);

      ZeroMemory(&si, sizeof(si));
      si.cb = sizeof(si);
      ZeroMemory(&pi, sizeof(pi));

      /* Launch the replacement first, and only close this instance if
         that actually succeeded -- if CreateProcess failed for some
         reason, the setting is still saved (just applies next time the
         user starts the app some other way) rather than leaving them
         with no running instance at all. */
      if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
      {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
      }
    }
    return;
  }

  if (id >= IDM_AUDIO_FMCORE_BASE && id < IDM_AUDIO_FMCORE_BASE + 5)
  {
    int choice = id - IDM_AUDIO_FMCORE_BASE;

    /* 0-2: MAME (YM2612 discrete, YM3438 ASIC, YM3438 enhanced). 3-4: Nuked
       (as a YM2612, as a YM3438). */
    config.ym3438 = (uint8)(choice >= 3);
    if (choice <= 2) config.ym2612 = (uint8)choice;
    if (choice >= 3) gui.nuked_ym2612 = (choice == 3);
    emu_apply_nuked_type();

    if (emu_running)
    {
      /* Unlike Sample Rate, Enable Sound, HQPSG, or Region -- none of
         which actually need sound_init() at all, since its behaviour
         depends purely on which FM core is configured, confirmed by
         reading it -- this menu is the one place that genuinely changes
         that configuration, so sound_init() actually has to run here.
         audio_init() then sound_reset() are the same fix as before:
         audio_init() rebuilds the blip buffers (skipping it left the OLD
         core's accumulator contents in place); sound_reset() resyncs
         fm_cycles_count/fm_cycles_start/fm_ptr with whatever ratio
         sound_init() just set up (skipping it crashed outright, since
         MAME's YM2612 and Nuked's YM3438 run at very different ratios).
         What this can't fix: MAME and Nuked are independent C
         implementations with no shared internal state, so switching
         between them can't carry over which instrument was playing on
         which channel -- the running game isn't told anything changed
         and won't re-send configuration it thinks is already in place.
         Some instruments may stay quiet until the game's own music driver
         naturally re-triggers them (a new area, a menu, etc.) or the game
         is reset. That part is inherent to swapping cores mid-run, not a
         bug this sequence can paper over. */
      audio_init(gui.sample_rate, 0);
      sound_init();
      sound_reset();

      /* audio_reset() is what actually clears the blip buffers/low-pass
         filter state/re-syncs the equalizer -- see the longer note in
         emu_apply_audio_settings(). Missing it here was why some
         instruments stayed silent until an actual reset happened. */
      audio_reset();
    }
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_AUDIO_RATE_BASE && id < IDM_AUDIO_RATE_BASE + 2)
  {
    gui.sample_rate = (id == IDM_AUDIO_RATE_BASE) ? 44100 : 48000;
    emu_apply_audio_settings();
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_INPUT_PORTA_BASE && id < IDM_INPUT_PORTA_BASE + 10)
  {
    input.system[0] = (uint8)port_menu_value(0, id - IDM_INPUT_PORTA_BASE);
    if (emu_running) io_init();
    gui_update_menu();
    config_save();
    return;
  }

  if (id >= IDM_INPUT_PORTB_BASE && id < IDM_INPUT_PORTB_BASE + 10)
  {
    input.system[1] = (uint8)port_menu_value(1, id - IDM_INPUT_PORTB_BASE);
    if (emu_running) io_init();
    gui_update_menu();
    config_save();
    return;
  }

  switch (id)
  {
    case IDM_FILE_OPEN:
      browse_for_rom();
      break;

    case IDM_FILE_BROWSER:
      browser_panel_change_folder();
      break;

    case IDM_FILE_ROMINFO:
      dlg_rom_info(g_hwnd);
      break;

    case IDM_FILE_STATEMGR:
      dlg_state_manager(g_hwnd);
      break;

    case IDM_FILE_CLOSE:
    case IDM_EMU_STOP:
      emu_close_rom();
      break;

    case IDM_FILE_RECENT_CLEAR:
    {
      int i;
      for (i = 0; i < GUI_RECENT_MAX; i++) gui.recent[i][0] = '\0';
      rebuild_recent_menu();
      config_save();
      break;
    }

    case IDM_FILE_SAVESTATE:
    case IDM_ACCEL_QUICKSAVE:
      emu_save_state(gui.state_slot);
      break;

    case IDM_FILE_LOADSTATE:
    case IDM_ACCEL_QUICKLOAD:
      emu_load_state(gui.state_slot);
      break;

    case IDM_ACCEL_SLOT_NEXT:
      gui.state_slot = (gui.state_slot + 1) % GUI_SLOT_MAX;
      gui_update_menu();
      gui_notify("Slot %d selected", gui.state_slot);
      break;

    case IDM_ACCEL_SLOT_PREV:
      gui.state_slot = (gui.state_slot + GUI_SLOT_MAX - 1) % GUI_SLOT_MAX;
      gui_update_menu();
      gui_notify("Slot %d selected", gui.state_slot);
      break;

    case IDM_FILE_REC_VIDEO:
      if (recorder_active() == REC_VIDEO) record_stop();
      else if (!recorder_active()) record_start(REC_VIDEO);
      break;

    case IDM_FILE_REC_AUDIO:
      if (recorder_active() == REC_AUDIO) record_stop();
      else if (!recorder_active()) record_start(REC_AUDIO);
      break;

    case IDM_FILE_REC_STOP:
      record_stop();
      break;

    case IDM_TOOLS_NETPLAY:
      if (!emu_running) gui_notify("Load a game first");
      else if (netplay_state() != NP_IDLE) gui_notify("Netplay is already running");
      else dlg_netplay(g_hwnd);
      break;

    case IDM_TOOLS_NETPLAY_STOP:
      netplay_disconnect("Netplay stopped");
      break;

    case IDM_FILE_SCREENSHOT:
    {
      char path[GUI_PATH_LEN];
      char name[GUI_PATH_LEN];

      if (video_screenshot(path, sizeof(path)))
      {
        char *slash = strrchr(path, '\\');
        lstrcpynA(name, slash ? slash + 1 : path, sizeof(name));
        gui_notify("Saved %s", name);
      }
      else
      {
        gui_notify("Could not save the screenshot");
      }
      break;
    }

    case IDM_FILE_OPENDIR:
      /* The folder holding gpgx.exe, gpgx.ini, states, saves, cheats, ... */
      ShellExecuteA(g_hwnd, "open", osd_path(""), NULL, NULL, SW_SHOWNORMAL);
      break;

    case IDM_FILE_EXIT:
      PostMessage(g_hwnd, WM_CLOSE, 0, 0);
      break;

    case IDM_EMU_PAUSE:
      if (!emu_running) break;
      emu_paused = !emu_paused;
      auto_paused_by_focus = 0;
      if (emu_paused) waveout_flush();
      next_frame_time = 0.0;
      gui_update_menu();
      status_set_fps(0);
      gui_notify(emu_paused ? "Paused" : "Resumed");
      break;

    case IDM_EMU_RESET:
      emu_reset(0);
      break;

    case IDM_EMU_HARDRESET:
      emu_reset(1);
      break;

    case IDM_EMU_CHEATS:
      if (!emu_running) break;
      dlg_cheats(g_hwnd);
      break;

    case IDM_EMU_BIOS:
      /* 0 = cartridge only, 3 = BIOS enabled and booted first. */
      config.bios = (uint8)(config.bios ? 0 : 3);
      gui_update_menu();
      config_save();
      gui_status("BIOS setting applies the next time a ROM is loaded");
      break;

    case IDM_EMU_ADDRERROR:
      config.addr_error = (uint8)(!config.addr_error);
      gui_update_menu();
      config_save();
      break;

    case IDM_EMU_UNDOLOAD:
      emu_undo_load_state();
      gui_update_menu();
      break;

    case IDM_EMU_REWIND:
      gui.rewind = !gui.rewind;
      rewind_enable(gui.rewind);
      if (gui.rewind && !rewind_ready())
      {
        gui.rewind = 0;
        gui_status("Not enough memory for the rewind history");
      }
      gui_update_menu();
      config_save();
      break;

    case IDM_EMU_PAUSE_UNFOCUSED:
      gui.pause_on_focus_loss = !gui.pause_on_focus_loss;
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_FULLSCREEN:
      if (!emu_running) break;
      video_set_fullscreen(!gui.fullscreen);
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_FULLSCREEN_START:
      gui.fullscreen_on_load = !gui.fullscreen_on_load;
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_ALWAYS_ON_TOP:
      gui.always_on_top = !gui.always_on_top;
      apply_always_on_top();
      gui_update_menu();
      config_save();
      break;

    case IDM_ACCEL_LEAVE_FULLSCREEN:
      /* Replaces the right-click that used to do this: confirmed by direct,
         repeated testing that a right-click needed polling in the first
         place because once a menu is detached (SetMenu(NULL)), a second
         click's button-down message simply never arrives at the window
         procedure at all -- not a redraw problem, a message-delivery one.
         Esc, delivered as an ordinary accelerator, has no such problem and
         needs none of that polling machinery. Alt+Enter (IDM_VIDEO_FULLSCREEN)
         still leaves fullscreen entirely, unaffected by this. */
      if (gui.fullscreen)
      {
        toggle_fullscreen_menu();
      }
      break;

    case IDM_VIDEO_SMOOTH:
      gui.smooth = !gui.smooth;
      video_viewport_changed();
      gui_update_menu();
      config_save();
      break;

    case IDM_INPUT_BACKGROUND:
      gui.background_input = !gui.background_input;
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_BRIGHTEN:
      gui.brighten = !gui.brighten;
      video_force_redraw();   /* repaint the current frame (also while paused) */
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_RENDERER_BASE + 0:
    case IDM_VIDEO_RENDERER_BASE + 1:
    case IDM_VIDEO_RENDERER_BASE + 2:
      gui.renderer = id - IDM_VIDEO_RENDERER_BASE;
      video_set_renderer(gui.renderer);
      video_viewport_changed();
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_VSYNC:
      gui.vsync = !gui.vsync;
      video_set_vsync(gui.vsync);
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_FILTER_NONE:
      video_set_filter(-1);
      rebuild_filter_menu();
      config_save();
      break;

    case IDM_VIDEO_SHOWFPS:
      gui.show_fps = !gui.show_fps;
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_SMSBORDER:
      gui.sms_show_border = !gui.sms_show_border;
      emu_apply_sms_border();
      video_viewport_changed();
      gui_update_menu();
      config_save();
      break;

    case IDM_VIDEO_GGEXTRA:
      config.gg_extra = (uint8)(!config.gg_extra);
      if (emu_running) bitmap.viewport.changed = 3;
      video_viewport_changed();
      gui_update_menu();
      config_save();
      break;

    case IDM_AUDIO_ENABLE:
      gui.sound_enabled = !gui.sound_enabled;
      emu_apply_audio_settings();
      gui_update_menu();
      config_save();
      break;

    case IDM_AUDIO_SETTINGS:
      dlg_audio(g_hwnd);
      break;

    case IDM_AUDIO_MONO:
      config.mono = (uint8)(!config.mono);
      gui_update_menu();
      config_save();
      break;

    case IDM_AUDIO_ADVANCED:
      dlg_audio_advanced(g_hwnd);
      break;

    case IDM_AUDIO_LOWPASS:
      config.filter = (uint8)(config.filter ? 0 : 1);
      if (emu_running) audio_set_equalizer();
      gui_update_menu();
      config_save();
      break;

    case IDM_AUDIO_HQPSG:
      config.hq_psg = (uint8)(!config.hq_psg);
      if (emu_running)
      {
        /* Direct psg_init() call, not sound_init() -- sound_init() always
           reinitializes the FM chip too as an inseparable part of the same
           call (confirmed by reading it: FM and PSG setup are one
           function, no way to ask for just one), which has nothing to do
           with this setting and was needlessly wiping FM instrument state
           on every toggle. psg_init() is independently exposed by the
           core for exactly this. Matches sound_init()'s own ternary for
           which PSG variant a given system uses. */
        psg_init((system_hw == SYSTEM_SG) ? PSG_DISCRETE : PSG_INTEGRATED);
      }
      gui_update_menu();
      config_save();
      break;

    case IDM_INPUT_P1:
      dlg_input(g_hwnd, 0);
      break;

    case IDM_INPUT_P2:
      dlg_input(g_hwnd, 1);
      break;

    case IDM_HELP_SHORTCUTS:
      dlg_shortcuts(g_hwnd);
      break;

    case IDM_HELP_MENUGUIDE:
      dlg_menu_guide(g_hwnd);
      break;

    case IDM_HELP_ABOUT:
      dlg_about(g_hwnd);
      break;

    default:
      break;
  }
}

/****************************************************************************
 * Window procedure
 ****************************************************************************/

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
    case WM_COMMAND:
    {
      WORD id = LOWORD(wp);
      WORD notify = HIWORD(wp);

      if (browser_panel_handle_command(id, notify)) return 0;

      if (notify == 0 || notify == 1) on_command(id);
      return 0;
    }

    case 0x0091:   /* WM_UAHDRAWMENU -- undocumented, no public name */
      if (theme_draw_menu_bar(hwnd, lp)) return 0;
      break;

    case 0x0092:   /* WM_UAHDRAWMENUITEM -- undocumented, no public name */
      if (theme_draw_menu_item(lp)) return 0;
      break;

    case WM_INITMENUPOPUP:
    {
      /* Refresh the slot timestamps each time the Save Slot menu opens. */
      HMENU slot_menu = find_slot_menu();
      if (slot_menu && (HMENU)wp == slot_menu)
      {
        update_slot_menu_labels(slot_menu);
        check_radio(slot_menu, IDM_FILE_SLOT_BASE, GUI_SLOT_MAX, gui.state_slot);
        return 0;
      }
      break;
    }

    case WM_MEASUREITEM:
      if (theme_measure_menu_ownerdraw(lp)) return TRUE;
      break;

    case WM_DRAWITEM:
      if (theme_draw_menu_ownerdraw(lp)) return TRUE;
      break;

    case WM_NOTIFY:
    {
      LRESULT sb = theme_statusbar_customdraw((NMHDR *)lp, g_status);
      if (sb != -1) return sb;
      sb = theme_header_customdraw((NMHDR *)lp);
      if (sb != -1) return sb;
      if (browser_panel_handle_notify((NMHDR *)lp)) return 0;
      return 0;
    }

    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      video_repaint(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_ERASEBKGND:
      return 1;   /* video.c paints every pixel it owns */

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    {
      HBRUSH br = browser_panel_ctlcolor((HWND)lp, (HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_CONTEXTMENU:
      if (browser_panel_handle_contextmenu((HWND)wp,
            (lp == (LPARAM)-1) ? -1 : (int)(short)LOWORD(lp),
            (lp == (LPARAM)-1) ? -1 : (int)(short)HIWORD(lp))) return 0;
      break;

    case WM_SIZE:
      if (wp == SIZE_MINIMIZED)
      {
        if (emu_running && !emu_paused && !netplay_active())
        {
          emu_paused = 1;
          auto_paused_by_minimize = 1;
          waveout_flush();
          gui_update_menu();
          status_set_fps(0);
        }
        return 0;
      }

      if (auto_paused_by_minimize)
      {
        auto_paused_by_minimize = 0;
        if (emu_running && emu_paused)
        {
          emu_paused = 0;
          gui_update_menu();
        }
      }

      layout_status();
      video_invalidate();
      capture_window_geometry();
      if (browser_panel_visible())
      {
        RECT content;
        get_content_rect(&content);
        browser_panel_layout(&content);
      }

      /* In dark mode the menu bar is painted by hand, and how many rows it
         wraps into changes with the window's width. A live resize does not
         reliably repaint the whole bar on its own -- confirmed by direct
         testing, a stray white patch from the bar's *previous* size and row
         count is left behind, since Windows only invalidates what its own
         (unaware of the hand-painted content) layout thinks changed. Forcing
         a full non-client repaint here, on every size change, clears it. */
      if (theme_is_dark())
      {
        RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
      }
      return 0;

    case WM_MOVE:
      capture_window_geometry();
      return 0;

    case 0x02E0:   /* WM_DPICHANGED, not in every mingw headers version */
    {
      /* The manifest claims per-monitor awareness, so Windows expects us to
         resize ourselves when the window moves to a different-DPI display. */
      RECT *suggested = (RECT *)lp;

      if (suggested && !gui.fullscreen)
      {
        SetWindowPos(hwnd, NULL,
                     suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
      layout_status();
      video_invalidate();
      return 0;
    }

    case WM_GETMINMAXINFO:
    {
      MINMAXINFO *mmi = (MINMAXINFO *)lp;
      mmi->ptMinTrackSize.x = 256;
      mmi->ptMinTrackSize.y = 224;
      return 0;
    }

    case WM_ACTIVATE:
    {
      int active = (LOWORD(wp) != WA_INACTIVE);

      gui_input_set_focus(active);

      if (!active && gui.pause_on_focus_loss && emu_running && !emu_paused && !netplay_active())
      {
        emu_paused = 1;
        auto_paused_by_focus = 1;
        waveout_flush();
        gui_update_menu();
        status_set_fps(0);
      }
      else if (active && auto_paused_by_focus)
      {
        auto_paused_by_focus = 0;
        if (emu_running && emu_paused)
        {
          emu_paused = 0;
          gui_update_menu();
        }
      }
      next_frame_time = 0.0;
      return 0;
    }

    case WM_ENTERMENULOOP:
    case WM_ENTERSIZEMOVE:
      in_modal_loop = 1;
      waveout_flush();
      return 0;

    case WM_EXITMENULOOP:
    case WM_EXITSIZEMOVE:
      in_modal_loop = 0;
      next_frame_time = 0.0;
      return 0;

    case WM_DROPFILES:
    {
      HDROP drop = (HDROP)wp;
      char path[GUI_PATH_LEN];

      if (DragQueryFileA(drop, 0, path, sizeof(path)))
      {
        emu_load_rom(path);
        SetForegroundWindow(hwnd);
      }
      DragFinish(drop);
      return 0;
    }

    case WM_SYSCOMMAND:
      /* Do not let the screensaver interrupt a game. */
      if ((wp & 0xFFF0) == SC_SCREENSAVE || (wp & 0xFFF0) == SC_MONITORPOWER)
      {
        if (emu_running && !emu_paused) return 0;
      }

      /* F10's default behavior is to activate the menu bar the same way
         Alt does -- both arrive here as SC_KEYMENU, distinguished by lp
         being 0 for F10 specifically (not a character key) versus the
         actual letter code for an Alt+letter mnemonic, which must still
         work normally. Left alone, F10 briefly deactivates the window
         while entering that menu mode, which triggers "Pause emulation
         when in background" as an unintended side effect -- there was
         never an actual F10-to-pause binding, just this indirect path
         through a feature that has nothing to do with F10 itself. */
      if ((wp & 0xFFF0) == SC_KEYMENU && lp == 0) return 0;

      break;

    case WM_QUERYENDSESSION:
      return TRUE;

    case WM_ENDSESSION:
      /* Windows is shutting down or logging off and will end the process
         without a WM_CLOSE: write out everything worth keeping now. */
      if (wp)
      {
        save_backup_ram();
        cheats_save_current();
        config_save();
      }
      return 0;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_SETTINGCHANGE:
      theme_handle_settingchange(hwnd);
      return 0;

    default:
      break;
  }

  return DefWindowProc(hwnd, msg, wp, lp);
}

/****************************************************************************
 * Startup
 ****************************************************************************/

static int create_main_window(void)
{
  int center_window_at_start = 0;
  POINT pt_origin = { 0, 0 };
  WNDCLASSEXA wc;
  RECT rc;
  int w, h;
  int x, y;

  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize        = sizeof(wc);
  wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc   = wnd_proc;
  wc.hInstance     = g_inst;
  wc.hIcon         = LoadIconA(g_inst, MAKEINTRESOURCEA(IDI_APPICON));
  wc.hIconSm       = wc.hIcon;
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = APP_CLASS;

  if (!RegisterClassExA(&wc)) return 0;

  g_menu = LoadMenuA(g_inst, MAKEINTRESOURCEA(IDR_MAINMENU));

  video_preferred_size(gui.scale, &w, &h);
  rc.left = 0; rc.top = 0; rc.right = w; rc.bottom = h;
  AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, TRUE, 0);

  {
    int mw = menu_bar_min_width(g_menu);
    if (mw > rc.right - rc.left) rc.right = rc.left + mw;
  }

  if (gui.large_ui) theme_ownerdraw_menu(g_menu);

  /*
   * Use the saved position only if it still lands on a real monitor --
   * otherwise a since-unplugged second monitor would put the window
   * somewhere the person can never reach. MonitorFromRect returns NULL for
   * a rect that intersects nothing, which is exactly the check needed.
   */
  x = CW_USEDEFAULT;
  y = CW_USEDEFAULT;
  center_window_at_start = 1;      /* until a saved position turns out to be usable */

  if (gui.window_x > -32000 && gui.window_y > -32000)
  {
    RECT candidate;
    candidate.left   = gui.window_x;
    candidate.top    = gui.window_y;
    candidate.right  = gui.window_x + (rc.right - rc.left);
    candidate.bottom = gui.window_y + (rc.bottom - rc.top);

    if (MonitorFromRect(&candidate, MONITOR_DEFAULTTONULL) != NULL)
    {
      x = gui.window_x;
      y = gui.window_y;
      center_window_at_start = 0;
    }
  }

  g_hwnd = CreateWindowExA(0, APP_CLASS, APP_NAME, WS_OVERLAPPEDWINDOW,
                           x, y,
                           rc.right - rc.left, rc.bottom - rc.top,
                           NULL, g_menu, g_inst, NULL);
  if (!g_hwnd) return 0;

  g_status = CreateWindowExA(WS_EX_COMPOSITED, STATUSCLASSNAMEA, NULL,
                             WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                             0, 0, 0, 0, g_hwnd, NULL, g_inst, NULL);
  SendMessage(g_status, WM_SETFONT, (WPARAM)gui_get_ui_font(), TRUE);
  if (gui.large_ui) SendMessage(g_status, SB_SETMINHEIGHT, 30, 0);

  browser_panel_create(g_hwnd);

  g_accel = LoadAcceleratorsA(g_inst, MAKEINTRESOURCEA(IDR_ACCELERATORS));

  DragAcceptFiles(g_hwnd, TRUE);
  layout_status();

  /* Skipped when about to restore maximized: WinMain will maximize right
     after this returns, which would just discard this resize. */
  if (!gui.window_maximized)
  {
    /* Now that the status bar exists its real height is known, so the
       client area can be sized to give exactly the requested scale. */
    gui_resize_to_scale(gui.scale);
  }

  /* No saved position: open in the middle of the screen, not wherever Windows
     cascades a new window. Done after the final resize so the size is settled. */
  if (center_window_at_start && !gui.window_maximized)
  {
    RECT wr, area;
    MONITORINFO mi;
    HMONITOR mon = MonitorFromPoint(pt_origin, MONITOR_DEFAULTTOPRIMARY);

    mi.cbSize = sizeof(mi);
    if (GetWindowRect(g_hwnd, &wr) && GetMonitorInfo(mon, &mi))
    {
      int ww = wr.right - wr.left, wh = wr.bottom - wr.top;

      area = mi.rcWork;
      SetWindowPos(g_hwnd, NULL,
                   area.left + ((area.right - area.left) - ww) / 2,
                   area.top  + ((area.bottom - area.top) - wh) / 2,
                   0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }

  return 1;
}

/* Pulls the first command-line argument out, if there is one. */
static void first_argument(char *out, int out_len)
{
  LPWSTR *argv;
  int argc = 0;

  out[0] = '\0';

  argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) return;

  if (argc >= 2)
  {
    WideCharToMultiByte(CP_ACP, 0, argv[1], -1, out, out_len, NULL, NULL);
  }

  LocalFree(argv);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
  INITCOMMONCONTROLSEX icc;
  MSG msg;
  char startup_rom[GUI_PATH_LEN];

  (void)prev;
  (void)cmdline;

  g_inst = inst;

  QueryPerformanceFrequency(&perf_freq);
  if (perf_freq.QuadPart == 0) perf_freq.QuadPart = 1;

  icc.dwSize = sizeof(icc);
  icc.dwICC  = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_LINK_CLASS;
  InitCommonControlsEx(&icc);

  error_init();
  config_load();
  netplay_init(&np_iface);
  emu_apply_nuked_type();
  gui.fullscreen = 0;   /* never restored across runs -- see the note near WinMain's end */

  ensure_dir("saves");
  ensure_dir("states");
  ensure_dir("screenshots");
  ensure_dir("bios");
  ensure_dir("cheats");
  ensure_dir("covers");
  ensure_dir("recordings");

  /* Before create_main_window(), not after -- that call loads and
     attaches the menu resource, and the app-wide dark mode preference
     needs to already be active for the menu *bar* itself (not just its
     dropdown popups, which pick it up regardless of timing) to render
     dark. */
  theme_init();

  if (!create_main_window())
  {
    MessageBoxA(NULL, "The main window could not be created.",
                APP_NAME, MB_OK | MB_ICONERROR);
    return 1;
  }

  theme_apply_to_window(g_hwnd);

  if (!video_init())
  {
    MessageBoxA(g_hwnd, "The video buffer could not be created.",
                APP_NAME, MB_OK | MB_ICONERROR);
    return 1;
  }

  /* Restore whichever render filter was active last session. A name that no
     longer matches anything (a filter that was removed or renamed, or a
     leftover .rpi filename) just leaves filtering off and clears the saved
     value. */
  if (gui.render_filter[0])
  {
    char saved[64];
    lstrcpynA(saved, gui.render_filter, sizeof(saved));
    video_set_filter_by_name(saved);
  }

  gui_input_init();
  rewind_init();
  rewind_enable(gui.rewind);
  load_boot_rom();

  if (gui.sound_enabled && !waveout_open(gui.sample_rate))
  {
    gui.sound_enabled = 0;
    gui_status("No audio device available, sound is off");
  }
  waveout_set_volume(gui.volume);

  rebuild_recent_menu();
  rebuild_filter_menu();
  gui_update_menu();
  update_title();

  ShowWindow(g_hwnd, gui.window_maximized ? SW_MAXIMIZE : show);
  UpdateWindow(g_hwnd);
  apply_always_on_top();

  /* Confirmed by direct testing: any real resize (either direction)
     fixes a stale-paint artifact that's otherwise present from first
     launch -- so trigger the exact same layout/repaint path a genuine
     WM_SIZE runs, once, right here, rather than waiting for the user
     to happen to resize the window themselves. */
  SendMessage(g_hwnd, WM_SIZE, 0, 0);

  /* Confirmed by direct testing (Larger UI + a dark theme, a window narrow
     enough that the owner-drawn menu bar wraps to more than one row): some
     of the bar's own titles -- not popup items, the top-level titles
     themselves -- come up blank on the very first paint and stay that way
     until something (a click, a hover, or an actual resize) makes Windows
     lay the bar out again, at which point the missing titles appear for
     good. WM_SIZE above does not reach this: it is the non-client menu area,
     not the client area, that came up wrong. A same-size SWP_FRAMECHANGED
     asks for a repaint but not a new layout, and does not fix it either --
     what does is a real size change, so the window is grown by a pixel and
     immediately put back, forcing Windows to lay the bar out again at its
     real width rather than repaint whatever it already (incompletely) had. */
  if (gui.large_ui)
  {
    RECT wr;
    GetWindowRect(g_hwnd, &wr);
    SetWindowPos(g_hwnd, NULL, 0, 0, wr.right - wr.left + 1, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_hwnd, NULL, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  first_argument(startup_rom, sizeof(startup_rom));
  if (startup_rom[0]) emu_load_rom(startup_rom);

  if (!emu_running)
  {
    RECT content;
    gui_status_persistent("Open a ROM to start, or drop one on this window");
    get_content_rect(&content);
    browser_panel_layout(&content);
    browser_panel_show(1);
  }

  /* Fullscreen deliberately does not carry over between runs -- always
     start windowed regardless of what gui.fullscreen was saved as. The
     field itself still gets tracked and saved during the session (menu
     checkmark state, various runtime guards elsewhere read it), it's only
     the startup restore that's skipped. */
  fps_tick = GetTickCount();

  for (;;)
  {
    int quit = 0;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT) { quit = 1; break; }

      if (browser_panel_visible())
      {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN)
        {
          browser_panel_handle_return();
          continue;
        }

        /* IsDialogMessage is documented as usable on any top-level window
           that manages child controls the way a dialog does, not only real
           dialog boxes -- this is what gives Tab/Shift+Tab navigation
           between the search box, list and buttons without hand-rolling
           focus management. Gated on the panel being visible so it can
           never intercept a keystroke meant for the emulator itself, which
           only reads input while this panel is hidden. */
        if (IsDialogMessage(g_hwnd, &msg)) continue;
      }

      /* Ctrl+C (and friends) in the browser's search box is copy, not the
         Cheats shortcut. */
      int text_edit = 0;

      if (msg.message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) &&
          (msg.wParam == 'C' || msg.wParam == 'X' || msg.wParam == 'V' ||
           msg.wParam == 'A' || msg.wParam == 'Z'))
      {
        char cls[16];
        HWND f = GetFocus();

        if (f && GetClassNameA(f, cls, sizeof(cls)) && !lstrcmpiA(cls, "Edit")) text_edit = 1;
      }

      if (text_edit || !g_accel || !TranslateAccelerator(g_hwnd, g_accel, &msg))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    if (quit) break;

    emulation_step();
  }

  emu_close_rom();
  netplay_shutdown();
  config_save();

  waveout_close();
  gui_input_shutdown();
  video_shutdown();
  error_shutdown();

  return (int)msg.wParam;
}
