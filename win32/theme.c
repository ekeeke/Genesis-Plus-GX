/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  theme.c -- see theme.h for the overall approach and the caveats
 *  around the undocumented uxtheme.dll entry points this leans on.
 ****************************************************************************/

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>

#include "shared.h"
#include "gui.h"
#include "resource.h"
#include "theme.h"

/* Not in every SDK's dwmapi.h yet -- just an integer constant, safe to
   supply if missing. 20 is the value used since Windows 10 20H1; 19 was
   used briefly before that in early 1809-era builds. Both are set below;
   whichever one the running Windows version doesn't recognize is simply
   ignored (DwmSetWindowAttribute fails harmlessly for it). */
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#define DWMWA_USE_IMMERSIVE_DARK_MODE_PRE20H1 19

/* uxtheme.dll ordinals -- these have no exported names, so GetProcAddress
   only works by number. Stable since Windows 10 1809, per the
   "win32-darkmode" reverse-engineering project; see theme.h for what
   happens if a future Windows update ever changes them. */
typedef enum { AppMode_Default, AppMode_AllowDark, AppMode_ForceDark, AppMode_ForceLight, AppMode_Max } PreferredAppMode;

typedef void             (WINAPI *fn_RefreshImmersiveColorPolicyState)(void);
typedef BOOL              (WINAPI *fn_AllowDarkModeForWindow)(HWND, BOOL);
typedef PreferredAppMode (WINAPI *fn_SetPreferredAppMode)(PreferredAppMode);
typedef void             (WINAPI *fn_FlushMenuThemes)(void);

static fn_RefreshImmersiveColorPolicyState pRefreshImmersiveColorPolicyState;
static fn_AllowDarkModeForWindow           pAllowDarkModeForWindow;
static fn_SetPreferredAppMode              pSetPreferredAppMode;
static fn_FlushMenuThemes                  pFlushMenuThemes;

static int system_dark;   /* last-read system preference, kept fresh via
                              theme_handle_settingchange() */

static int read_system_dark(void)
{
  HKEY key;
  DWORD value = 1, size = sizeof(value);

  /* Missing key/value (older Windows, or a very locked-down install)
     defaults to light -- matches what Windows itself falls back to. */
  if (RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &key) == ERROR_SUCCESS)
  {
    DWORD type;
    if (RegQueryValueExA(key, "AppsUseLightTheme", NULL, &type, (BYTE *)&value, &size) != ERROR_SUCCESS)
      value = 1;
    RegCloseKey(key);
  }
  return value == 0;   /* 0 = dark, 1 = light */
}

void theme_init(void)
{
  HMODULE hux;

  system_dark = read_system_dark();

  hux = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!hux) return;   /* no dark mode support on this Windows version */

  pSetPreferredAppMode              = (fn_SetPreferredAppMode)(void *)GetProcAddress(hux, MAKEINTRESOURCEA(135));
  pAllowDarkModeForWindow           = (fn_AllowDarkModeForWindow)(void *)GetProcAddress(hux, MAKEINTRESOURCEA(133));
  pFlushMenuThemes                  = (fn_FlushMenuThemes)(void *)GetProcAddress(hux, MAKEINTRESOURCEA(136));
  pRefreshImmersiveColorPolicyState = (fn_RefreshImmersiveColorPolicyState)(void *)GetProcAddress(hux, MAKEINTRESOURCEA(104));

  if (pSetPreferredAppMode)
  {
    pSetPreferredAppMode(theme_is_dark() ? AppMode_ForceDark : AppMode_ForceLight);
    if (pRefreshImmersiveColorPolicyState) pRefreshImmersiveColorPolicyState();
  }
}

void theme_set_mode(int mode)
{
  gui.theme_mode = mode;
}

int theme_get_mode(void)
{
  return gui.theme_mode;
}

int theme_is_dark(void)
{
  if (gui.theme_mode == THEME_LIGHT) return 0;
  if (gui.theme_mode == THEME_DARK)  return 1;
  return system_dark;
}

/* A more direct fix than SB_SETBKCOLOR, which turned out not to
   actually change the rendered background despite being the documented
   mechanism for this -- painting it directly here bypasses whatever
   about that message chain was overriding the color. Text color is
   still handled separately by theme_statusbar_customdraw(). */
/* A disabled plain-text (STATIC) control: DefWindowProc's own painting for
   the disabled state overlays a highlight-colored copy of the text offset by
   one pixel on top of whatever color WM_CTLCOLORSTATIC provided (a classic-
   Windows "engraved" look, meant for a light background) -- confirmed by
   direct testing, on a dark background that highlight copy shows up as a
   pale ghost/halo around the text rather than blending in. Painting the
   control ourselves for just that one state, in a single flat color, avoids
   it entirely; the enabled state is untouched and still goes through the
   normal WM_CTLCOLORSTATIC path. */
static LRESULT CALLBACK static_text_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                   UINT_PTR subclass_id, DWORD_PTR ref_data)
{
  (void)subclass_id;
  (void)ref_data;

  if (msg == WM_PAINT && theme_is_dark() && !IsWindowEnabled(hwnd))
  {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT r;
    char text[256];
    HFONT font, old_font;
    HBRUSH br;
    LONG style;
    UINT align = DT_LEFT;

    hdc = BeginPaint(hwnd, &ps);

    GetClientRect(hwnd, &r);
    br = CreateSolidBrush(RGB(32, 32, 32));   /* the plain dialog background this control sits on */
    FillRect(hdc, &r, br);
    DeleteObject(br);

    style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if ((style & SS_TYPEMASK) == SS_CENTER)     align = DT_CENTER;
    else if ((style & SS_TYPEMASK) == SS_RIGHT) align = DT_RIGHT;

    font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    old_font = (HFONT)SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));

    GetWindowTextA(hwnd, text, sizeof(text));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(120, 120, 120));   /* flat disabled gray, the same one used elsewhere for disabled menu text */
    DrawTextA(hdc, text, -1, &r, align | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, old_font);
    EndPaint(hwnd, &ps);
    return 0;
  }

  if (msg == WM_ENABLE)
  {
    /* Repaint right away rather than waiting for the next unrelated
       invalidate -- the control's look needs to change the moment its
       enabled state does (e.g. switching the Audio Filter dropdown). */
    InvalidateRect(hwnd, NULL, TRUE);
  }

  if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, static_text_subclass_proc, subclass_id);

  return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK statusbar_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                 UINT_PTR subclass_id, DWORD_PTR ref_data)
{
  (void)subclass_id;
  (void)ref_data;

  if (msg == WM_ERASEBKGND && theme_is_dark())
  {
    return 1;   /* suppress -- WM_PAINT below draws the background itself */
  }

  if (msg == WM_PAINT && theme_is_dark())
  {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT r;
    int parts[16];
    int n_parts;
    int i, left;
    HBRUSH br;
    HFONT font, old_font;

    hdc = BeginPaint(hwnd, &ps);

    GetClientRect(hwnd, &r);
    br = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &r, br);
    DeleteObject(br);

    n_parts = (int)SendMessage(hwnd, SB_GETPARTS, 0, 0);
    if (n_parts > 16) n_parts = 16;
    if (n_parts < 1) n_parts = 1;
    SendMessage(hwnd, SB_GETPARTS, (WPARAM)n_parts, (LPARAM)parts);

    font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    old_font = font ? (HFONT)SelectObject(hdc, font) : NULL;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(240, 240, 240));

    left = 0;
    for (i = 0; i < n_parts; i++)
    {
      char text[256];
      RECT part_rect;
      int right = parts[i];

      text[0] = '\0';
      SendMessageA(hwnd, SB_GETTEXTA, (WPARAM)i, (LPARAM)text);

      part_rect.left   = left + 4;
      part_rect.top    = r.top;
      part_rect.right  = (right == -1) ? r.right : right;
      part_rect.bottom = r.bottom;

      DrawTextA(hdc, text, -1, &part_rect, DT_VCENTER | DT_SINGLELINE | DT_LEFT);

      left = right;
    }

    if (old_font) SelectObject(hdc, old_font);

    EndPaint(hwnd, &ps);
    return 0;
  }

  return DefSubclassProc(hwnd, msg, wp, lp);
}

/* A ListView's column-header row is a child window, and it sends its
   custom-draw notifications to ITS parent -- the ListView itself. The ListView
   does not pass them on to its own parent, so the WM_NOTIFY handlers in the
   main window and in dialogs never see them and could not recolour the header
   text. Subclassing the ListView is what lets us receive them where they
   actually arrive; the colour itself is still set by theme_header_customdraw(). */
static LRESULT CALLBACK listview_header_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                      UINT_PTR id, DWORD_PTR ref)
{
  (void)ref;

  if (msg == WM_NOTIFY)
  {
    LRESULT r = theme_header_customdraw((NMHDR *)lp);
    if (r != -1) return r;
  }
  else if (msg == WM_NCDESTROY)
  {
    RemoveWindowSubclass(hwnd, listview_header_subclass_proc, id);
  }

  return DefSubclassProc(hwnd, msg, wp, lp);
}

static BOOL CALLBACK apply_to_child(HWND hwnd, LPARAM lparam)
{
  int dark = (int)lparam;
  char class_name[64];
  int skip_generic_theme;
  LONG_PTR ex_style;

  GetClassNameA(hwnd, class_name, sizeof(class_name));
  skip_generic_theme = (lstrcmpiA(class_name, "ListBox") == 0) ||
                       (lstrcmpiA(class_name, "msctls_statusbar32") == 0);

  if (lstrcmpiA(class_name, "Static") == 0)
  {
    /* Plain LTEXT labels: see static_text_subclass_proc for what this
       fixes (a disabled label's own default painting, not anything
       WM_CTLCOLORSTATIC controls). Always subclassed; the fix only
       actually changes anything when the control is later disabled
       while dark mode is active. */
    SetWindowSubclass(hwnd, static_text_subclass_proc, 3, 0);
  }

  /* WS_EX_CLIENTEDGE draws a fixed, light-gray sunken 3D border that's
     part of the control's own non-client rendering -- separate from
     its interior (which WM_CTLCOLOREDIT/LISTBOX already colors
     correctly) and from anything SetWindowTheme touches, so it stayed
     its normal light color regardless of theme, showing as a thin but
     very visible pale sliver around the search box and ROM list. */
  ex_style = GetWindowLongPtrA(hwnd, GWL_EXSTYLE);
  if ((dark && (ex_style & WS_EX_CLIENTEDGE)) ||
      (!dark && !(ex_style & WS_EX_CLIENTEDGE) &&
       (lstrcmpiA(class_name, "Edit") == 0 || lstrcmpiA(class_name, "SysListView32") == 0)))
  {
    RECT r;

    SetWindowLongPtrA(hwnd, GWL_EXSTYLE, dark ? (ex_style & ~WS_EX_CLIENTEDGE) : (ex_style | WS_EX_CLIENTEDGE));

    /* SWP_FRAMECHANGED alone, with no actual size change, wasn't
       enough to force the visual update at startup time -- the stale
       border only got redrawn after a genuine resize (e.g. maximize
       then restore). Nudging the width by a real pixel and back is
       the same technique already proven for the same kind of stale
       non-client-border problem on the ListView header elsewhere in
       this file, forcing a real geometry recalculation rather than
       just marking one as pending. */
    GetWindowRect(hwnd, &r);
    SetWindowPos(hwnd, NULL, 0, 0, (r.right - r.left) + 1, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    SetWindowPos(hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME);
  }

  if (pAllowDarkModeForWindow) pAllowDarkModeForWindow(hwnd, dark ? TRUE : FALSE);

  if (lstrcmpiA(class_name, "SysHeader32") == 0)
  {
    /* A ListView's column-header row is its own child window, visited
       here independently of the SysListView32 branch below that also
       tries to theme it -- handled here instead, explicitly, so it
       always ends up with the theme a list header actually wants
       regardless of which of the two visits happens last. */
    RECT r;
    SetWindowTheme(hwnd, dark ? L"ItemsView" : NULL, NULL);
    /* Column-divider lines are drawn as part of this control's own
       frame, not just its interior -- a plain invalidate left stale
       dark divider lines behind when switching back to light, so this
       forces a real geometry change (nudging the width by a pixel and
       back) to guarantee a full repaint, the same technique used
       elsewhere in this app for controls whose visual state got stuck
       through an ordinary invalidate. */
    GetWindowRect(hwnd, &r);
    SetWindowPos(hwnd, NULL, 0, 0, (r.right - r.left) + 1, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    return TRUE;
  }

  if (lstrcmpiA(class_name, "ComboBox") == 0)
  {
    SetWindowTheme(hwnd, dark ? L"DarkMode_CFD" : NULL, NULL);
  }
  else if (!skip_generic_theme)
  {
    /* Edit controls are included here (unlike ListBox and the status
       bar) specifically so their scrollbar renders dark --
       WM_CTLCOLOREDIT already handles the text/background correctly
       regardless of this call, but the scrollbar arrows/thumb/track
       are separate and only themed through this. */
    SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : NULL, NULL);
  }

  if (lstrcmpiA(class_name, "SysListView32") == 0)
  {
    /* SetWindowTheme("DarkMode_Explorer") above only reaches this
       control's scrollbar and border -- the actual row/cell colors are
       separate and need setting explicitly. The header is handled in
       its own branch above, not here. */
    RECT r;
    COLORREF bg_color = (dark && GetDlgCtrlID(hwnd) == IDC_CHEATS_LIST)
                       ? RGB(0x33, 0x33, 0x33) : RGB(32, 32, 32);
    ListView_SetBkColor(hwnd, dark ? bg_color : RGB(255, 255, 255));
    ListView_SetTextColor(hwnd, dark ? RGB(240, 240, 240) : RGB(0, 0, 0));
    ListView_SetTextBkColor(hwnd, dark ? bg_color : RGB(255, 255, 255));

    /* So this list's header text can be recoloured; see the note above. */
    SetWindowSubclass(hwnd, listview_header_subclass_proc, 2, 0);

    /* Same reasoning and technique as the header's own redraw above --
       a plain invalidate wasn't enough to clear stale column-divider
       rendering left over from before a theme switch. */
    GetWindowRect(hwnd, &r);
    SetWindowPos(hwnd, NULL, 0, 0, (r.right - r.left) + 1, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
  }
  else if (lstrcmpiA(class_name, "msctls_statusbar32") == 0)
  {
    /* SB_SETBKCOLOR is documented to have no effect at all while visual
       styles are active for this control -- unlike everywhere else in
       this file, NULL here would keep the OS's own themed (light)
       background and silently ignore the color set below. Empty
       strings specifically turn styling off for this control, which is
       what actually lets SB_SETBKCOLOR take effect. */
    SetWindowTheme(hwnd, dark ? L"" : NULL, dark ? L"" : NULL);
    SendMessage(hwnd, SB_SETBKCOLOR, 0, dark ? RGB(32, 32, 32) : (LPARAM)CLR_DEFAULT);
    SetWindowSubclass(hwnd, statusbar_subclass_proc, 1, 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
  }

  InvalidateRect(hwnd, NULL, TRUE);
  return TRUE;
}

void theme_apply_to_window(HWND hwnd)
{
  int dark = theme_is_dark();
  BOOL dark_bool = dark ? TRUE : FALSE;

  if (pSetPreferredAppMode)
  {
    pSetPreferredAppMode(dark ? AppMode_ForceDark : AppMode_ForceLight);
    if (pRefreshImmersiveColorPolicyState) pRefreshImmersiveColorPolicyState();
  }

  if (pAllowDarkModeForWindow) pAllowDarkModeForWindow(hwnd, dark_bool);
  SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : NULL, NULL);

  /* Title bar -- both attribute numbers tried, see the #define comment
     above; DwmSetWindowAttribute simply fails (harmlessly) for whichever
     one this Windows version doesn't recognize. */
  DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_bool, sizeof(dark_bool));
  DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_PRE20H1, &dark_bool, sizeof(dark_bool));

  EnumChildWindows(hwnd, apply_to_child, (LPARAM)dark);

  /* Popup menus cache their own theme state; without this, a menu
     opened after switching modes can still show the old one until the
     app restarts. */
  if (pFlushMenuThemes) pFlushMenuThemes();

  /* SWP_FRAMECHANGED forces the non-client area (title bar, borders) to
     actually repaint with the DWM attribute change just made -- without
     it, the change can take effect only on the next unrelated resize. */
  SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  DrawMenuBar(hwnd);
  InvalidateRect(hwnd, NULL, TRUE);
}

int theme_handle_settingchange(HWND hwnd)
{
  int new_dark = read_system_dark();

  if (new_dark == system_dark) return 0;
  system_dark = new_dark;

  if (gui.theme_mode != THEME_AUTO) return 0;   /* manual override -- system change doesn't matter */

  theme_apply_to_window(hwnd);
  return 1;
}

HBRUSH theme_ctlcolor(HDC hdc)
{
  static HBRUSH dark_brush;

  if (!theme_is_dark()) return NULL;

  if (!dark_brush) dark_brush = CreateSolidBrush(RGB(32, 32, 32));

  SetTextColor(hdc, RGB(240, 240, 240));
  SetBkColor(hdc, RGB(32, 32, 32));
  return dark_brush;
}

/* Same as theme_ctlcolor, but with an explicit background color instead
   of the app-wide default -- for the handful of controls that need a
   slightly different shade to stand out from their surroundings
   (search box, cheat code/name fields, certain list controls). Caller
   passes a distinct id per color so each gets its own cached brush
   rather than fighting over one shared static. */
HBRUSH theme_ctlcolor_custom(HDC hdc, COLORREF bg, int cache_id)
{
  static HBRUSH cached[4];

  if (!theme_is_dark()) return NULL;
  if (cache_id < 0 || cache_id >= 4) return theme_ctlcolor(hdc);

  if (!cached[cache_id]) cached[cache_id] = CreateSolidBrush(bg);

  SetTextColor(hdc, RGB(240, 240, 240));
  SetBkColor(hdc, bg);
  return cached[cache_id];
}

LRESULT theme_statusbar_customdraw(NMHDR *hdr, HWND status_bar)
{
  NMCUSTOMDRAW *cd;

  if (!hdr || hdr->hwndFrom != status_bar || hdr->code != NM_CUSTOMDRAW) return -1;
  if (!theme_is_dark()) return -1;

  cd = (NMCUSTOMDRAW *)hdr;

  if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;

  if (cd->dwDrawStage == CDDS_ITEMPREPAINT)
  {
    SetTextColor(cd->hdc, RGB(240, 240, 240));
    return CDRF_DODEFAULT;
  }

  return CDRF_DODEFAULT;
}

LRESULT theme_header_customdraw(NMHDR *hdr)
{
  NMCUSTOMDRAW *cd;
  char class_name[64];

  if (!hdr || hdr->code != NM_CUSTOMDRAW) return -1;

  GetClassNameA(hdr->hwndFrom, class_name, sizeof(class_name));
  if (lstrcmpiA(class_name, "SysHeader32") != 0) return -1;

  if (!theme_is_dark()) return -1;

  cd = (NMCUSTOMDRAW *)hdr;

  if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;

  if (cd->dwDrawStage == CDDS_ITEMPREPAINT)
  {
    SetTextColor(cd->hdc, RGB(240, 240, 240));

    /* CDRF_NEWFONT, not CDRF_DODEFAULT: it tells the control that the colours
       set on the DC are meant to be used. With DODEFAULT, a header themed with
       "ItemsView" (set above) drew its own black text instead. */
    return CDRF_NEWFONT;
  }

  return CDRF_DODEFAULT;
}

/* ==========================================================================
 * Menu bar strip custom-draw (dark mode only)
 * ==========================================================================
 * Everything above (ForceDark/ForceLight, FlushMenuThemes) covers popup
 * dropdowns -- it does not reach the menu *bar* strip itself, which
 * Windows always paints on its own with no documented way to recolor
 * it. The only way in is to intercept the two undocumented messages it
 * sends when that strip needs painting. Both the messages and the
 * structs they carry are unnamed by any public SDK header; the layout
 * here matches what's been a stable, widely-used reverse-engineering
 * result since Windows 10 (the "win32-darkmode" project on GitHub uses
 * the same one). The trailing arrays in UAHMENUITEM are deliberately
 * oversized past what's actually read (iPosition only) as a margin of
 * safety against the exact tail layout being slightly off.
 * A future Windows update changing this would simply make these two
 * handlers stop firing or paint nothing -- theme_is_dark() still gates
 * both, so light mode is entirely unaffected either way. */

#define WM_UAHDRAWMENU     0x0091
#define WM_UAHDRAWMENUITEM 0x0092

typedef struct
{
  HMENU hmenu;
  HDC   hdc;
  DWORD dwFlags;
} UAHMENU;

typedef struct
{
  int   iPosition;
  DWORD rgsizeBar[4];
  DWORD rgsizePopup[8];
} UAHMENUITEM;

typedef struct
{
  DRAWITEMSTRUCT dis;
  UAHMENU um;
  UAHMENUITEM umi;
} UAHDRAWMENUITEM;

int theme_draw_menu_bar(HWND hwnd, LPARAM lp)
{
  UAHMENU *menu = (UAHMENU *)lp;
  MENUBARINFO mbi;
  RECT rc_window, rect;
  HBRUSH br;

  if (!theme_is_dark()) return 0;
  if (!menu || !menu->hdc) return 0;

  mbi.cbSize = sizeof(mbi);
  if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) return 0;

  GetWindowRect(hwnd, &rc_window);

  rect = mbi.rcBar;
  OffsetRect(&rect, -rc_window.left, -rc_window.top);
  rect.right = rc_window.right - rc_window.left;   /* full window width, not just client -- rcBar/client.right both stop short */
  rect.top -= 2;   /* covers a light edge Windows paints in the caption seam just above the bar */

  SelectClipRgn(menu->hdc, NULL);   /* ensure nothing restricts the fill below to less than rect */

  br = CreateSolidBrush(RGB(32, 32, 32));
  FillRect(menu->hdc, &rect, br);
  DeleteObject(br);

  return 1;
}

int theme_draw_menu_item(LPARAM lp)
{
  UAHDRAWMENUITEM *item = (UAHDRAWMENUITEM *)lp;
  char text[256];
  MENUITEMINFOA mii;
  RECT rect;
  HBRUSH br;
  int selected, disabled;
  int old_mode;
  int use_big_font = 0;
  HGDIOBJ old_font = NULL;
  COLORREF old_text;

  if (!theme_is_dark()) return 0;
  if (!item) return 0;

  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_STRING | MIIM_STATE;
  mii.dwTypeData = text;
  mii.cch = sizeof(text) - 1;
  text[0] = 0;
  GetMenuItemInfoA(item->um.hmenu, item->umi.iPosition, TRUE, &mii);

  /* With Larger UI the bar titles are owner-drawn: an owner-drawn item has no
     text of its own to ask for (that read comes back empty, which painted the
     titles as blanks until the mouse was over them). Its text is the copy kept
     in itemData, drawn here in the larger font. */
  {
    MENUITEMINFOA od;

    memset(&od, 0, sizeof(od));
    od.cbSize = sizeof(od);
    od.fMask = MIIM_FTYPE | MIIM_DATA;
    if (GetMenuItemInfoA(item->um.hmenu, item->umi.iPosition, TRUE, &od) &&
        (od.fType & MFT_OWNERDRAW) && od.dwItemData)
    {
      lstrcpynA(text, (const char *)od.dwItemData, sizeof(text));
      use_big_font = 1;
    }
  }

  rect = item->dis.rcItem;
  selected = (item->dis.itemState & (ODS_HOTLIGHT | ODS_SELECTED)) != 0;
  disabled = (item->dis.itemState & ODS_DISABLED) != 0;

  br = CreateSolidBrush(selected ? RGB(62, 62, 62) : RGB(32, 32, 32));
  FillRect(item->um.hdc, &rect, br);
  DeleteObject(br);

  old_mode = SetBkMode(item->um.hdc, TRANSPARENT);
  old_text = SetTextColor(item->um.hdc, disabled ? RGB(120, 120, 120) : RGB(240, 240, 240));

  if (use_big_font) old_font = SelectObject(item->um.hdc, gui_get_ui_font());
  DrawTextA(item->um.hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  if (use_big_font) SelectObject(item->um.hdc, old_font);

  SetTextColor(item->um.hdc, old_text);
  SetBkMode(item->um.hdc, old_mode);

  return 1;
}

/****************************************************************************
 * Owner-drawn menu items for large_ui
 *
 * A completely different, much older and more conservative mechanism
 * than the WM_UAHDRAWMENU family above -- MF_OWNERDRAW has been part of
 * documented Win32 since Windows 3.1, driven by the standard
 * WM_MEASUREITEM/WM_DRAWITEM messages, not an undocumented, guessed
 * struct layout. Used here (rather than the UAH approach) specifically
 * because an earlier attempt at resizing the menu via WM_UAHMEASUREMENUITEM
 * broke the menu entirely on real Windows -- this trades a little visual
 * conformity (Windows draws less of the chrome around each item for us)
 * for something that can't silently corrupt the menu's own internal
 * layout data the way writing to a guessed struct could.
 ****************************************************************************/

/* Item text is stored here at conversion time (not re-queried at
   measure/draw time) because popup items -- "Save Slot", "Theme", and
   so on -- have no usable command ID to look themselves up by; only
   leaf command items do. Storing the string once, up front, works
   uniformly for both. Never freed -- one-time setup for the app's
   lifetime, same as the cached large font. */
static void convert_item_to_ownerdraw(HMENU menu, int pos)
{
  MENUITEMINFOA mii;
  char text[128];
  char *stored;

  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_STRING | MIIM_FTYPE;
  mii.dwTypeData = text;
  mii.cch = sizeof(text) - 1;
  text[0] = '\0';

  if (!GetMenuItemInfoA(menu, (UINT)pos, TRUE, &mii)) return;

  if (mii.fType & MFT_SEPARATOR)
  {
    mii.fMask = MIIM_FTYPE | MIIM_DATA;
    mii.fType |= MFT_OWNERDRAW;
    mii.dwItemData = 0;   /* NULL itemData marks this a separator at draw time */
    SetMenuItemInfoA(menu, (UINT)pos, TRUE, &mii);
    return;
  }

  stored = (char *)malloc((size_t)lstrlenA(text) + 1);
  if (!stored) return;
  lstrcpyA(stored, text);

  mii.fMask = MIIM_FTYPE | MIIM_DATA;
  mii.fType |= MFT_OWNERDRAW;
  mii.dwItemData = (ULONG_PTR)stored;
  SetMenuItemInfoA(menu, (UINT)pos, TRUE, &mii);
}

/* Recurses into every submenu (Save Slot, Theme, Aspect Ratio, and so
   on) so the whole menu tree is owner-drawn consistently, not just the
   top level. Call once, at startup, only when large_ui is on. */
void theme_ownerdraw_menu(HMENU menu)
{
  int count, i;

  if (!menu) return;
  count = GetMenuItemCount(menu);
  if (count < 0) return;

  for (i = 0; i < count; i++)
  {
    HMENU submenu = GetSubMenu(menu, i);

    convert_item_to_ownerdraw(menu, i);

    if (submenu) theme_ownerdraw_menu(submenu);
  }
}

int theme_measure_menu_ownerdraw(LPARAM lp)
{
  MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lp;
  HDC hdc;
  HFONT font, old_font;
  SIZE sz;
  char *text;

  if (!gui.large_ui) return 0;
  if (!mis || mis->CtlType != ODT_MENU) return 0;

  text = (char *)mis->itemData;

  if (!text)
  {
    /* Separator -- fixed, small height regardless of font size. */
    mis->itemWidth = 8;
    mis->itemHeight = 6;
    return 1;
  }

  hdc = GetDC(NULL);
  font = gui_get_ui_font();
  old_font = (HFONT)SelectObject(hdc, font);
  GetTextExtentPoint32A(hdc, text, lstrlenA(text), &sz);
  SelectObject(hdc, old_font);
  ReleaseDC(NULL, hdc);

  /* Margin beyond the raw text size for the selection highlight and
     checkmark/submenu-arrow gutter, the same way Windows' own default
     menu metrics include room beyond the text itself. */
  mis->itemWidth  = (UINT)sz.cx + 20;
  mis->itemHeight = (UINT)(sz.cy * 1.5);

  return 1;
}

/* An item's position in its menu, found by the string pointer stored as its
   data (unique to it -- there is no usable command ID for a popup item).
   Used both by menu_item_has_submenu() below and by the top-level bar's
   trailing-space fill, which needs to know whether an item is the last one
   on its own row. */
static int menu_item_position(HMENU menu, ULONG_PTR item_data)
{
  int count, i;

  if (!menu || !item_data) return -1;

  count = GetMenuItemCount(menu);
  for (i = 0; i < count; i++)
  {
    MENUITEMINFOA mii;

    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask  = MIIM_DATA;

    if (GetMenuItemInfoA(menu, (UINT)i, TRUE, &mii) && mii.dwItemData == item_data) return i;
  }
  return -1;
}

/* Popup items ("Region", "Theme", "Aspect Ratio", ...) have no usable command
   ID, but the string pointer stored as each item's data is unique to it, so
   the item is found in its menu by that. */
static int menu_item_has_submenu(HMENU menu, ULONG_PTR item_data)
{
  int count, i;

  if (!menu || !item_data) return 0;

  count = GetMenuItemCount(menu);
  for (i = 0; i < count; i++)
  {
    MENUITEMINFOA mii;

    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask  = MIIM_DATA | MIIM_SUBMENU;

    if (GetMenuItemInfoA(menu, (UINT)i, TRUE, &mii) && mii.dwItemData == item_data)
    {
      return mii.hSubMenu != NULL;
    }
  }

  return 0;
}

/* Windows draws a submenu's arrow itself, in black, right after an
   owner-drawn item has been painted -- which vanishes against a dark item.
   So in dark mode the arrow is drawn here in the text colour, and its area is
   then clipped out of the DC so the system's black one cannot go on top.

   It is drawn to match the system's own arrow, measured from a light-mode
   screenshot at a 30 px item height: a staircase triangle 4 px wide and 7 px
   tall (column 0 spans 7 rows, then 5, 3 and 1), its tip 8 px in from the
   item's right edge and its middle about 2 px above the item's centre. The
   size scales with the item height so it stays in proportion. */
static void draw_dark_submenu_arrow(HDC hdc, const RECT *r, int disabled)
{
  int h    = r->bottom - r->top;
  int half = (3 * h + 15) / 30;                     /* 3 at a 30 px item */
  int tip  = r->right - 8;                          /* x of the tip's column */
  int cy   = (r->top + r->bottom) / 2 - (h + 7) / 15;   /* 2 px above centre at 30 px */
  int zone = (h > 24) ? h : 24;
  COLORREF color = disabled ? RGB(120, 120, 120) : RGB(240, 240, 240);
  HBRUSH br = CreateSolidBrush(color);
  int i;

  if (half < 2) half = 2;

  for (i = 0; i <= half; i++)
  {
    int ext = half - i;
    RECT col;

    col.left   = tip - half + i;
    col.right  = col.left + 1;
    col.top    = cy - ext;
    col.bottom = cy + ext + 1;
    FillRect(hdc, &col, br);
  }

  DeleteObject(br);

  ExcludeClipRect(hdc, r->right - zone, r->top, r->right, r->bottom);
}

int theme_draw_menu_ownerdraw(LPARAM lp)
{
  DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
  char *text;
  int selected, disabled, checked, dark;
  HBRUSH br;
  RECT r;

  if (!gui.large_ui) return 0;
  if (!dis || dis->CtlType != ODT_MENU) return 0;

  text     = (char *)dis->itemData;
  selected = (dis->itemState & ODS_SELECTED) != 0;
  disabled = (dis->itemState & ODS_DISABLED) != 0;
  checked  = (dis->itemState & ODS_CHECKED) != 0;
  dark     = theme_is_dark();
  r        = dis->rcItem;

  br = CreateSolidBrush(dark ? (selected ? RGB(62, 62, 62) : RGB(32, 32, 32))
                              : GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_MENU));
  FillRect(dis->hDC, &r, br);

  if (dark && (HMENU)dis->hwndItem == GetMenu(g_hwnd))
  {
    /* Fill from this item's right edge to the window's right edge, but only
       when this item is the last one on its own row -- either the very last
       item in the menu, or immediately followed by an item that wrapped to a
       new row. Doing this for every item would be the bug: Windows repaints
       only the item whose state actually changed (e.g. just "Emulation" when
       its dropdown opens), not the whole bar, so filling rightward from any
       item that has another item sitting to its right would erase that
       neighbour, which is not due to repaint at this moment.

       This used to be decided by comparing GetMenuStringA()'s text for the
       last position against this item's own text -- but GetMenuStringA()
       does not return anything useful once an item has been converted to
       owner-draw (which every item here was, back at startup), so that
       comparison could never actually match and this fill silently never
       ran except where it happened not to be needed. Position, found the
       same way menu_item_has_submenu() finds it (by the item's unique
       itemData pointer), does not have that problem. */
    HMENU top_menu = (HMENU)dis->hwndItem;
    int count = GetMenuItemCount(top_menu);
    int pos = menu_item_position(top_menu, (ULONG_PTR)text);
    int is_last_on_row = 0;

    if (pos >= 0)
    {
      if (pos == count - 1)
      {
        is_last_on_row = 1;   /* the very last item in the menu */
      }
      else
      {
        RECT this_r, next_r;
        if (GetMenuItemRect(g_hwnd, top_menu, (UINT)pos, &this_r) &&
            GetMenuItemRect(g_hwnd, top_menu, (UINT)(pos + 1), &next_r) &&
            next_r.top != this_r.top)
        {
          is_last_on_row = 1;   /* the next item wrapped to a new row */
        }
      }
    }

    if (is_last_on_row)
    {
      RECT rc_window;
      HBRUSH bg_br = CreateSolidBrush(RGB(32, 32, 32));   /* always the plain background, never the highlight color */
      GetWindowRect(g_hwnd, &rc_window);
      {
        int full_width = rc_window.right - rc_window.left;
        if (r.right < full_width)
        {
          RECT trailing = r;
          trailing.left = r.right;
          trailing.right = full_width;   /* full window width, not just client -- same fix as theme_draw_menu_bar */
          /* A couple of pixels above this item's own top, not all the way up to
             row 0: reaching above this item's own row would erase whatever an
             earlier, unrelated row's items already painted. */
          trailing.top = (r.top > 2) ? (r.top - 2) : 0;
          FillRect(dis->hDC, &trailing, bg_br);
        }
      }
      DeleteObject(bg_br);
    }
  }

  DeleteObject(br);

  if (!text)   /* separator */
  {
    HPEN pen = CreatePen(PS_SOLID, 1, dark ? RGB(80, 80, 80) : GetSysColor(COLOR_3DSHADOW));
    HPEN old_pen = (HPEN)SelectObject(dis->hDC, pen);
    int mid = (r.top + r.bottom) / 2;

    MoveToEx(dis->hDC, r.left + 4, mid, NULL);
    LineTo(dis->hDC, r.right - 4, mid);
    SelectObject(dis->hDC, old_pen);
    DeleteObject(pen);
    return 1;
  }

  {
    HFONT font = gui_get_ui_font();
    HFONT old_font = (HFONT)SelectObject(dis->hDC, font);
    RECT text_rect = r;

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, dark
        ? (disabled ? RGB(120, 120, 120) : RGB(240, 240, 240))
        : GetSysColor(disabled ? COLOR_GRAYTEXT : (selected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT)));

    /* Room for the checkmark gutter in a dropdown; the menu bar's own titles have
       none and are centred, or the text would be pushed out of its box. */
    if ((HMENU)dis->hwndItem == GetMenu(g_hwnd))
      DrawTextA(dis->hDC, text, -1, &text_rect, DT_VCENTER | DT_SINGLELINE | DT_CENTER);
    else
      text_rect.left += 24;

    if ((HMENU)dis->hwndItem != GetMenu(g_hwnd))
    {
      char *tab = strchr(text, '\t');
      if (tab)
      {
        RECT shortcut_rect = text_rect;
        *tab = '\0';
        DrawTextA(dis->hDC, text, -1, &text_rect, DT_VCENTER | DT_SINGLELINE | DT_LEFT);
        shortcut_rect.right -= 8;
        /* A shortcut hint reads as a secondary detail next to the item's own
           label, not something that should compete with it for attention --
           confirmed by direct testing, drawing it in the same bright color as
           the label (as an enabled, checked item like Rewind otherwise would)
           looked like the two were equally important. Disabled rows keep
           their single, already-dim color throughout; this only affects
           enabled ones. */
        if (!disabled) SetTextColor(dis->hDC, dark ? RGB(150, 150, 150) : GetSysColor(COLOR_GRAYTEXT));
        DrawTextA(dis->hDC, tab + 1, -1, &shortcut_rect, DT_VCENTER | DT_SINGLELINE | DT_RIGHT);
        *tab = '\t';   /* restore -- this string is reused every time the item repaints */
      }
      else
      {
        DrawTextA(dis->hDC, text, -1, &text_rect, DT_VCENTER | DT_SINGLELINE | DT_LEFT);
      }
    }

    if (checked)
    {
      /* Hand-drawn rather than DrawFrameControl -- that draws a fixed,
         always-light background square behind the glyph regardless of
         theme, which stood out against a dark item background. This
         way nothing is drawn but the checkmark itself, in a color that
         matches the surrounding text. */
      HPEN pen = CreatePen(PS_SOLID, 2, dark ? RGB(240, 240, 240) : RGB(0, 0, 0));
      HPEN old_pen = (HPEN)SelectObject(dis->hDC, pen);
      int cx = r.left + 12;
      int cy = (r.top + r.bottom) / 2;

      MoveToEx(dis->hDC, cx - 5, cy, NULL);
      LineTo(dis->hDC, cx - 2, cy + 4);
      LineTo(dis->hDC, cx + 5, cy - 5);

      SelectObject(dis->hDC, old_pen);
      DeleteObject(pen);
    }

    /* Submenu arrow, dark mode only (the system's black one is right on a
       light menu). Never the menu bar itself, which has no arrows. */
    if (dark && (HMENU)dis->hwndItem != GetMenu(g_hwnd) &&
        menu_item_has_submenu((HMENU)dis->hwndItem, dis->itemData))
    {
      draw_dark_submenu_arrow(dis->hDC, &r, disabled);
    }

    SelectObject(dis->hDC, old_font);
  }

  return 1;
}
