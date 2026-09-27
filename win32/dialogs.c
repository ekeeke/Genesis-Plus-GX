/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  dialogs.c -- the modal dialogs reachable from the menu.
 ****************************************************************************/

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>
#define COBJMACROS
#include <shobjidl.h>

#include <uxtheme.h>

#include "shared.h"
#include "gui.h"
#include "resource.h"
#include "theme.h"
#include "netplay.h"

/****************************************************************************
 * Input configuration
 *
 * Assigning works by listening rather than by asking the user to pick from a
 * list: choose a button, press what you want it to be. The same listening
 * pass covers the keyboard and the gamepad, so there is no separate mode to
 * switch into for controllers.
 ****************************************************************************/

/* Listening state machine */
enum { LISTEN_OFF = 0, LISTEN_WAIT_RELEASE, LISTEN_ACTIVE };

static struct
{
  int       player;
  t_pad_map map;        /* working copy, committed on Save */
  uint8     padtype;
  int       key_fast_forward, key_rewind;  /* working copy */
  int       pad_fast_forward, pad_rewind;  /* working copy */
  int       deadzone;   /* working copy, 0-100% */
  int       listen;
  int       listen_target;  /* which row listening is for, fixed at start */
  int       auto_advance;   /* set by Auto Assign only; Assign/double-click leave it off */
  HWND      dlg;
} inp;

/* Shared by every dialog with a slider-plus-percentage-readout control
   (Audio Levels and Latency, and this one) -- sets the slider's range and
   starting position and writes the matching text into its paired readout,
   all in one call instead of five repeated at every call site. */
static void audio_set_slider(HWND dlg, int id, int text_id, int lo, int hi,
                             int value, const char *suffix)
{
  HWND bar = GetDlgItem(dlg, id);
  char buf[32];

  SendMessage(bar, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
  SendMessage(bar, TBM_SETTICFREQ, (WPARAM)((hi - lo) / 8 ? (hi - lo) / 8 : 1), 0);
  SendMessage(bar, TBM_SETPOS, TRUE, (LPARAM)value);

  wsprintfA(buf, "%d%s", value, suffix);
  SetDlgItemTextA(dlg, text_id, buf);
}

/* Fast forward/rewind are global hotkeys, not per-player pad buttons, so
   they don't belong in t_pad_map -- shown as two extra rows appended
   after the normal pad buttons, in both players' dialogs since it's the
   same single global setting either way (changing it from Player 2's
   dialog affects the same gui.key_fast_forward/key_rewind Player 1's
   would). Reuses the exact same listen/assign/clear machinery via a
   couple of special cases below rather than building a separate UI
   section for them. */
#define EXTRA_KEY_COUNT   2
#define EXTRA_FASTFORWARD PAD_KEYS
#define EXTRA_REWIND      (PAD_KEYS + 1)

static int input_list_count(void)
{
  return PAD_KEYS + EXTRA_KEY_COUNT;
}

static const char *input_list_entry_name(int sel)
{
  if (sel == EXTRA_FASTFORWARD) return "Fast Forward";
  if (sel == EXTRA_REWIND)      return "Rewind";
  return input_button_label(sel);
}

static void input_fill_list(HWND dlg)
{
  HWND list = GetDlgItem(dlg, IDC_INPUT_LIST);
  int sel = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
  int i, count = input_list_count();

  SendMessage(list, LB_RESETCONTENT, 0, 0);

  for (i = 0; i < count; i++)
  {
    char line[128];
    int key, button;

    if (i == EXTRA_FASTFORWARD)      { key = inp.key_fast_forward; button = inp.pad_fast_forward; }
    else if (i == EXTRA_REWIND)      { key = inp.key_rewind; button = inp.pad_rewind; }
    else                              { key = inp.map.key[i]; button = inp.map.button[i]; }

    wsprintfA(line, "%s\t%s", input_key_name(key), input_pad_button_name(button));
    SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)line);
  }

  if (sel == LB_ERR) sel = 0;
  SendMessage(list, LB_SETCURSEL, (WPARAM)sel, 0);
}

static void input_set_listening(HWND dlg, int on)
{
  /* IDC_INPUT_LIST is included here again -- graying it out while
     listening previously also hid which button was being assigned,
     since the button name used to be its own first column. Now that
     the button names are separate static labels beside it (never
     disabled, since they're not interactive controls at all), that's
     no longer a problem, and graying this one out makes clear it's
     not interactive while listening. */
  static const int controls[] =
  {
    IDC_INPUT_LIST, IDC_INPUT_ASSIGN, IDC_INPUT_AUTOASSIGN, IDC_INPUT_CLEAR, IDC_INPUT_DEFAULTS,
    IDC_INPUT_DEVICE, IDC_INPUT_PADTYPE, IDOK, IDCANCEL
  };
  int i;

  /* Moved off whatever button was just clicked (Assign or Auto Assign,
     typically) onto a static label before disabling anything, rather
     than disabling every other control first and the focused one last.
     That two-pass approach did stop the original bug (disabling a
     focused control while something else was still enabled let
     Windows cascade focus onto it, and a still-in-flight activation key
     would fire again on whatever it landed on -- confirmed directly:
     Assign correctly setting auto_advance to 0 was having Auto Assign's
     handler fire right after and set it back to 1, purely because Auto
     Assign was still enabled the instant Assign got disabled). But
     disabling a button while it still held focus turned out to leave
     it stuck rendering as grayed even after being genuinely re-enabled
     later (confirmed via IsWindowEnabled returning true while it still
     visibly looked disabled) -- multiple different repaint techniques
     couldn't clear it. Moving focus to a plain static label first means
     nothing ever needs disabling while focused in the first place, so
     neither problem has room to happen: labels aren't in controls[], so
     they're never disabled, and a control that isn't focused when
     disabled has nothing to cascade its focus onto. */
  if (on) SetFocus(GetDlgItem(dlg, IDC_INPUT_LABEL_BASE));

  for (i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++)
  {
    HWND ctl = GetDlgItem(dlg, controls[i]);
    EnableWindow(ctl, on ? FALSE : TRUE);
    RedrawWindow(ctl, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
  }

  /* Whichever pushbutton was tabbed to or clicked most recently (Auto
     Assign, typically, since it's what starts this) keeps Windows'
     blue default-button border even once disabled -- that indicator
     tracks focus history, not enabled state, and won't clear on its
     own. Explicitly handing it back to the dialog's real default. */
  SendMessage(dlg, DM_SETDEFID, (WPARAM)IDOK, 0);

  if (on)
  {
    HWND list = GetDlgItem(dlg, IDC_INPUT_LIST);
    int sel = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
    char hint[128];

    inp.listen = LISTEN_WAIT_RELEASE;
    inp.listen_target = (sel == LB_ERR) ? 0 : sel;

    wsprintfA(hint, "Press the key or gamepad button for \"%s\".%s Esc cancels.",
              input_list_entry_name(inp.listen_target),
              inp.auto_advance ? " Then moves to the next button." : "");
    SetDlgItemTextA(dlg, IDC_INPUT_HINT, hint);
    SetTimer(dlg, 1, 15, NULL);
  }
  else
  {
    inp.listen = LISTEN_OFF;
    KillTimer(dlg, 1);
    SetDlgItemTextA(dlg, IDC_INPUT_HINT,
                    "Pick a button, choose Assign, then press the key you want.");
  }
}

/* After a successful assignment, either moves on to the next button and
   re-arms listening automatically (Auto Assign), or just stops there
   (Assign / double-click) -- inp.auto_advance, set by whichever one
   actually started this listen, decides which. Reuses
   input_set_listening(dlg, 1) as-is when advancing, including its
   "wait for the current key/button to be released first" safety --
   without that, whatever's still physically held from the assignment
   just made could get immediately captured again as the next one. */
static void input_advance_or_stop(HWND dlg, int sel)
{
  if (inp.auto_advance && sel + 1 < input_list_count())
  {
    HWND list = GetDlgItem(dlg, IDC_INPUT_LIST);
    SendMessage(list, LB_SETCURSEL, (WPARAM)(sel + 1), 0);
    input_set_listening(dlg, 1);
  }
  else
  {
    input_set_listening(dlg, 0);
  }
}

static void input_capture_tick(HWND dlg)
{
  int sel = inp.listen_target;
  int vk, button;

  if (sel < 0 || sel >= input_list_count())
  {
    input_set_listening(dlg, 0);
    return;
  }

  /* GetAsyncKeyState below (both directly for Escape, and inside
     input_capture_key()/input_capture_gamepad()) queries key/button
     state system-wide, with no idea which window is focused -- without
     this check, switching away to a different application entirely
     while a button was mid-assignment would still have whatever's
     typed there captured and assigned here, in the background. */
  if (GetForegroundWindow() != dlg) return;

  if (inp.listen == LISTEN_WAIT_RELEASE)
  {
    /* Do not capture the very keypress that opened the prompt. */
    if (!input_any_input_down(inp.map.device)) inp.listen = LISTEN_ACTIVE;
    return;
  }

  if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
  {
    input_set_listening(dlg, 0);
    return;
  }

  vk = input_capture_key();
  if (vk)
  {
    if (sel == EXTRA_FASTFORWARD)      inp.key_fast_forward = vk;
    else if (sel == EXTRA_REWIND)      inp.key_rewind = vk;
    else                                inp.map.key[sel] = vk;
    input_fill_list(dlg);
    input_advance_or_stop(dlg, sel);
    return;
  }

  button = input_capture_gamepad(sel >= PAD_KEYS ? gui.pad[0].device : inp.map.device);
  if (button)
  {
    if (sel == EXTRA_FASTFORWARD)      inp.pad_fast_forward = button;
    else if (sel == EXTRA_REWIND)      inp.pad_rewind = button;
    else                                inp.map.button[sel] = button;
    input_fill_list(dlg);
    input_advance_or_stop(dlg, sel);
  }
}

static void input_load_controls(HWND dlg)
{
  HWND dev = GetDlgItem(dlg, IDC_INPUT_DEVICE);
  HWND type = GetDlgItem(dlg, IDC_INPUT_PADTYPE);
  int tabs[1] = { 65 };
  int i;

  SendMessage(GetDlgItem(dlg, IDC_INPUT_LIST), LB_SETTABSTOPS, 1, (LPARAM)tabs);

  SendMessage(dev, CB_RESETCONTENT, 0, 0);
  SendMessageA(dev, CB_ADDSTRING, 0, (LPARAM)"Keyboard Only");
  for (i = 0; i < 4; i++)
  {
    char buf[32];
    wsprintfA(buf, "Gamepad %d", i + 1);
    SendMessageA(dev, CB_ADDSTRING, 0, (LPARAM)buf);
  }
  SendMessage(dev, CB_SETCURSEL, (WPARAM)(inp.map.device + 1), 0);

  SendMessage(type, CB_RESETCONTENT, 0, 0);
  SendMessageA(type, CB_ADDSTRING, 0, (LPARAM)"Match the Game");
  SendMessageA(type, CB_ADDSTRING, 0, (LPARAM)"3-Button Pad");
  SendMessageA(type, CB_ADDSTRING, 0, (LPARAM)"6-Button Pad");
  SendMessageA(type, CB_ADDSTRING, 0, (LPARAM)"2-Button Pad");

  switch (inp.padtype)
  {
    case DEVICE_PAD3B: SendMessage(type, CB_SETCURSEL, 1, 0); break;
    case DEVICE_PAD6B: SendMessage(type, CB_SETCURSEL, 2, 0); break;
    case DEVICE_PAD2B: SendMessage(type, CB_SETCURSEL, 3, 0); break;
    default:           SendMessage(type, CB_SETCURSEL, 0, 0); break;
  }

  audio_set_slider(dlg, IDC_INPUT_DEADZONE, IDC_INPUT_DEADZONE_TEXT, 0, 100, inp.deadzone, "%");
}

static void input_read_controls(HWND dlg)
{
  int dev = (int)SendMessage(GetDlgItem(dlg, IDC_INPUT_DEVICE), CB_GETCURSEL, 0, 0);
  int type = (int)SendMessage(GetDlgItem(dlg, IDC_INPUT_PADTYPE), CB_GETCURSEL, 0, 0);

  inp.map.device = (dev == CB_ERR) ? -1 : dev - 1;

  switch (type)
  {
    case 1:  inp.padtype = DEVICE_PAD3B; break;
    case 2:  inp.padtype = DEVICE_PAD6B; break;
    case 3:  inp.padtype = DEVICE_PAD2B; break;
    default: inp.padtype = DEVICE_PAD2B | DEVICE_PAD3B | DEVICE_PAD6B; break;
  }

  inp.deadzone = (int)SendMessage(GetDlgItem(dlg, IDC_INPUT_DEADZONE), TBM_GETPOS, 0, 0);
}

static void input_align_labels(HWND dlg)
{
  HWND list = GetDlgItem(dlg, IDC_INPUT_LIST);
  RECT list_rc;
  POINT list_origin;
  int item_height = (int)SendMessage(list, LB_GETITEMHEIGHT, 0, 0);
  int i, count = input_list_count();
  int old_height, tight_height, delta;
  static const int below_list[] = { IDC_INPUT_HINT, IDC_INPUT_DEADZONE_LABEL,
                                     IDC_INPUT_DEADZONE, IDC_INPUT_DEADZONE_TEXT,
                                     IDOK, IDCANCEL };

  /* The listbox's top-left in dialog client coordinates -- GetWindowRect
     gives screen coordinates, ScreenToClient (against the dialog, not
     the listbox itself) converts that to the same coordinate space
     every child control's position is already expressed in. */
  GetWindowRect(list, &list_rc);
  list_origin.x = list_rc.left;
  list_origin.y = list_rc.top;
  ScreenToClient(dlg, &list_origin);

  /* Shrinks the listbox to exactly fit its 14 rows -- its .rc height was
     a guess sized generously enough to avoid clipping before the real
     per-item height was known, leaving empty space below the last row.
     Now that the real height is queried directly, it can be sized
     exactly instead of guessed. */
  old_height = list_rc.bottom - list_rc.top;
  tight_height = item_height * count + 4;   /* +4 for the WS_BORDER frame */
  delta = old_height - tight_height;
  if (delta < 0) delta = 0;

  SetWindowPos(list, NULL, 0, 0, list_rc.right - list_rc.left, tight_height,
               SWP_NOMOVE | SWP_NOZORDER);

  for (i = 0; i < count; i++)
  {
    HWND label = GetDlgItem(dlg, IDC_INPUT_LABEL_BASE + i);
    RECT label_rc;
    POINT label_origin;

    if (!label) continue;

    /* Only Y changes -- X, width, and height stay exactly what the .rc
       template already set them to; only the vertical row spacing
       needed correcting to match the listbox's actual per-item height,
       which doesn't reduce to a clean round number of dialog units.
       The +2 nudges each label down slightly to align better with the
       listbox text's own vertical position within each row. */
    GetWindowRect(label, &label_rc);
    label_origin.x = label_rc.left;
    label_origin.y = label_rc.top;
    ScreenToClient(dlg, &label_origin);

    SetWindowPos(label, NULL, label_origin.x, list_origin.y + i * item_height + 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }

  /* Everything below the listbox (hint text, Save/Cancel) shifts up by
     the same amount the listbox just shrank, closing the gap rather
     than leaving it sitting empty partway down the dialog. */
  for (i = 0; i < (int)(sizeof(below_list) / sizeof(below_list[0])); i++)
  {
    HWND ctl = GetDlgItem(dlg, below_list[i]);
    RECT ctl_rc;
    POINT ctl_origin;

    GetWindowRect(ctl, &ctl_rc);
    ctl_origin.x = ctl_rc.left;
    ctl_origin.y = ctl_rc.top;
    ScreenToClient(dlg, &ctl_origin);

    SetWindowPos(ctl, NULL, ctl_origin.x, ctl_origin.y - delta, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);
  }
}

static INT_PTR CALLBACK input_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  (void)lp;

  switch (msg)
  {
    case WM_INITDIALOG:
    {
      char title[64];
      inp.dlg = dlg;
      wsprintfA(title, "Configure Player %d", inp.player + 1);
      SetWindowTextA(dlg, title);
      input_load_controls(dlg);
      input_fill_list(dlg);
      input_align_labels(dlg);

      /* Explicit, not left to default tab-order focus -- observed the
         dialog occasionally opening already in a listening state when
         triggered via keyboard menu navigation, which this avoids
         regardless of the exact cause by making sure no button ever
         holds initial focus. Returning FALSE tells Windows this dialog
         handled focus itself and not to override it. */
      SetFocus(GetDlgItem(dlg, IDC_INPUT_LIST));
      SendMessage(dlg, DM_SETDEFID, (WPARAM)IDOK, 0);

      /* Applied last, after focus is already settled -- SWP_FRAMECHANGED
         inside this can trigger synchronous frame recalculation, which
         risks pumping other pending messages (like a stray Return still
         working its way through from the menu selection that opened
         this dialog) before focus was safely parked on the list. */
      theme_apply_to_window(dlg);
      return FALSE;
    }

    case WM_TIMER:
      if (inp.listen != LISTEN_OFF) input_capture_tick(dlg);
      return TRUE;

    case WM_HSCROLL:
    {
      HWND bar = (HWND)lp;

      if (GetDlgCtrlID(bar) == IDC_INPUT_DEADZONE)
      {
        char buf[16];
        inp.deadzone = (int)SendMessage(bar, TBM_GETPOS, 0, 0);
        wsprintfA(buf, "%d%%", inp.deadzone);
        SetDlgItemTextA(dlg, IDC_INPUT_DEADZONE_TEXT, buf);
      }
      return TRUE;
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br;
      if (GetDlgCtrlID((HWND)lp) == IDC_INPUT_LIST)
        br = theme_ctlcolor_custom((HDC)wp, RGB(0x33, 0x33, 0x33), 2);
      else
        br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_COMMAND:
      switch (LOWORD(wp))
      {
        case IDC_INPUT_LIST:
          if (HIWORD(wp) == LBN_DBLCLK)
          {
            inp.auto_advance = 0;
            input_set_listening(dlg, 1);
          }
          return TRUE;

        case IDC_INPUT_ASSIGN:
          input_read_controls(dlg);
          inp.auto_advance = 0;
          input_set_listening(dlg, 1);
          return TRUE;

        case IDC_INPUT_AUTOASSIGN:
          input_read_controls(dlg);
          inp.auto_advance = 1;
          SendMessage(GetDlgItem(dlg, IDC_INPUT_LIST), LB_SETCURSEL, 0, 0);
          input_set_listening(dlg, 1);
          return TRUE;

        case IDC_INPUT_CLEAR:
        {
          HWND list = GetDlgItem(dlg, IDC_INPUT_LIST);
          int sel = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
          if (sel != LB_ERR && sel >= 0 && sel < input_list_count())
          {
            if (sel == EXTRA_FASTFORWARD)      { inp.key_fast_forward = 0; inp.pad_fast_forward = 0; }
            else if (sel == EXTRA_REWIND)      { inp.key_rewind = 0; inp.pad_rewind = 0; }
            else
            {
              inp.map.key[sel] = 0;
              inp.map.button[sel] = 0;
            }
            input_fill_list(dlg);
          }
          return TRUE;
        }

        case IDC_INPUT_DEFAULTS:
        {
          /* set_config_defaults() resets everything: the frontend settings, the
             emulation settings (config) and the port devices (input.system).
             Only the controls are wanted here, so put the rest back. */
          t_gui_config saved_gui = gui;
          t_config     saved_cfg = config;
          uint8        saved_sys[2];

          saved_sys[0] = input.system[0];
          saved_sys[1] = input.system[1];

          set_config_defaults();
          inp.map = gui.pad[inp.player];
          inp.padtype = config.input[inp.player].padtype;
          inp.key_fast_forward = gui.key_fast_forward;
          inp.key_rewind = gui.key_rewind;
          inp.pad_fast_forward = gui.pad_fast_forward;
          inp.pad_rewind = gui.pad_rewind;
          inp.deadzone = gui.deadzone[inp.player];

          gui = saved_gui;
          config = saved_cfg;
          input.system[0] = saved_sys[0];
          input.system[1] = saved_sys[1];
          input_load_controls(dlg);
          input_fill_list(dlg);
          return TRUE;
        }

        case IDOK:
          input_read_controls(dlg);
          gui.pad[inp.player] = inp.map;
          config.input[inp.player].padtype = inp.padtype;
          gui.key_fast_forward = inp.key_fast_forward;
          gui.key_rewind = inp.key_rewind;
          gui.pad_fast_forward = inp.pad_fast_forward;
          gui.pad_rewind = inp.pad_rewind;
          gui.deadzone[inp.player] = inp.deadzone;
          EndDialog(dlg, IDOK);
          return TRUE;

        case IDCANCEL:
          if (inp.listen != LISTEN_OFF) { input_set_listening(dlg, 0); return TRUE; }
          EndDialog(dlg, IDCANCEL);
          return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_input(HWND parent, int player)
{
  if (player < 0 || player > 1) return;

  ZeroMemory(&inp, sizeof(inp));
  inp.player  = player;
  inp.map     = gui.pad[player];
  inp.padtype = config.input[player].padtype;
  inp.key_fast_forward = gui.key_fast_forward;
  inp.key_rewind = gui.key_rewind;
  inp.pad_fast_forward = gui.pad_fast_forward;
  inp.pad_rewind = gui.pad_rewind;
  inp.deadzone = gui.deadzone[player];

  if (gui_dialog_box(gui.large_ui ? IDD_INPUT_LARGE : IDD_INPUT, parent, input_proc) == IDOK)
  {
    config_save();
    gui_status("Player %d controls saved", player + 1);
  }
}

/****************************************************************************
 * Audio levels and latency
 ****************************************************************************/

static struct
{
  int volume;
  int fm;
  int psg;
  int cdda;
  int latency;
} aud;

static void audio_refresh_labels(HWND dlg)
{
  char buf[32];

  wsprintfA(buf, "%d%%", aud.volume);
  SetDlgItemTextA(dlg, IDC_AUDIO_VOLUME_TEXT, buf);
  wsprintfA(buf, "%d%%", aud.fm);
  SetDlgItemTextA(dlg, IDC_AUDIO_FM_TEXT, buf);
  wsprintfA(buf, "%d%%", aud.psg);
  SetDlgItemTextA(dlg, IDC_AUDIO_PSG_TEXT, buf);
  wsprintfA(buf, "%d%%", aud.cdda);
  SetDlgItemTextA(dlg, IDC_AUDIO_CDDA_TEXT, buf);
  wsprintfA(buf, "%d", aud.latency);
  SetDlgItemTextA(dlg, IDC_AUDIO_LATENCY_TEXT, buf);
}

static void audio_load_controls(HWND dlg)
{
  audio_set_slider(dlg, IDC_AUDIO_VOLUME,  IDC_AUDIO_VOLUME_TEXT,  0, 100, aud.volume,  "%");
  audio_set_slider(dlg, IDC_AUDIO_FM,      IDC_AUDIO_FM_TEXT,      0, 200, aud.fm,      "%");
  audio_set_slider(dlg, IDC_AUDIO_PSG,     IDC_AUDIO_PSG_TEXT,     0, 200, aud.psg,     "%");
  audio_set_slider(dlg, IDC_AUDIO_CDDA,    IDC_AUDIO_CDDA_TEXT,    0, 200, aud.cdda,    "%");
  audio_set_slider(dlg, IDC_AUDIO_LATENCY, IDC_AUDIO_LATENCY_TEXT, 2, 8,   aud.latency, "");
}

static INT_PTR CALLBACK audio_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
    case WM_INITDIALOG:
      theme_apply_to_window(dlg);
      audio_load_controls(dlg);
      return TRUE;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_HSCROLL:
    {
      HWND bar = (HWND)lp;
      int pos = (int)SendMessage(bar, TBM_GETPOS, 0, 0);
      int id = GetDlgCtrlID(bar);

      switch (id)
      {
        case IDC_AUDIO_VOLUME:  aud.volume  = pos; break;
        case IDC_AUDIO_FM:      aud.fm      = pos; break;
        case IDC_AUDIO_PSG:     aud.psg     = pos; break;
        case IDC_AUDIO_CDDA:    aud.cdda    = pos; break;
        case IDC_AUDIO_LATENCY: aud.latency = pos; break;
        default: break;
      }

      audio_refresh_labels(dlg);

      /* Master volume is audible straight away, the rest applies on Save. */
      if (id == IDC_AUDIO_VOLUME) waveout_set_volume(aud.volume);
      return TRUE;
    }

    case WM_COMMAND:
      switch (LOWORD(wp))
      {
        case IDC_AUDIO_DEFAULTS:
          aud.volume  = 100;
          aud.fm      = 100;
          aud.psg     = 150;
          aud.cdda    = 100;
          aud.latency = 4;
          audio_load_controls(dlg);
          waveout_set_volume(aud.volume);
          return TRUE;

        case IDOK:
          EndDialog(dlg, IDOK);
          return TRUE;

        case IDCANCEL:
          waveout_set_volume(gui.volume);   /* undo the live preview */
          EndDialog(dlg, IDCANCEL);
          return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      waveout_set_volume(gui.volume);
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_audio(HWND parent)
{
  aud.volume  = gui.volume;
  aud.fm      = config.fm_preamp;
  aud.psg     = config.psg_preamp;
  aud.cdda    = config.cdda_volume;
  aud.latency = gui.latency;

  if (gui_dialog_box(gui.large_ui ? IDD_AUDIO_LARGE : IDD_AUDIO, parent, audio_proc) == IDOK)
  {
    gui.volume         = aud.volume;
    config.fm_preamp   = (int16)aud.fm;
    config.psg_preamp  = (int16)aud.psg;
    config.cdda_volume = (int16)aud.cdda;
    gui.latency        = aud.latency;

    waveout_set_volume(gui.volume);
    emu_apply_audio_settings();
    config_save();
    gui_status("Audio settings saved");
  }
}

/****************************************************************************
 * Advanced audio: filter, equalizer, high-quality FM, Master System FM,
 * Sega CD PCM volume
 ****************************************************************************/

static struct
{
  int filter;                 /* 0 off, 1 low-pass, 2 3-band EQ */
  int lp;                     /* low-pass strength, % */
  int lg, mg, hg;             /* EQ gains, % */
  int lowf, highf;            /* EQ band edges, Hz */
  int hqfm;
  int fmunit;                 /* 0 auto, 1 off, 2 on */
  int opll;                   /* 0 MAME, 1 Nuked */
  int pcm;
} adv;

static void adv_enable_by_filter(HWND dlg)
{
  int lp = (adv.filter == 1), eq = (adv.filter == 2);

  EnableWindow(GetDlgItem(dlg, IDC_ADV_LP), lp);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LP_TEXT), lp);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LABEL_LP), lp);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LG), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_MG), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_HG), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LG_TEXT), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_MG_TEXT), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_HG_TEXT), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LOWF), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_HIGHF), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LABEL_LG), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LABEL_MG), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LABEL_HG), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LABEL_LOWF), eq);
  EnableWindow(GetDlgItem(dlg, IDC_ADV_LABEL_HIGHF), eq);
}

static void adv_load_controls(HWND dlg)
{
  HWND c;

  c = GetDlgItem(dlg, IDC_ADV_FILTER);
  SendMessage(c, CB_RESETCONTENT, 0, 0);
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Off");
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Low-Pass");
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"3-Band EQ");
  SendMessage(c, CB_SETCURSEL, (WPARAM)adv.filter, 0);

  c = GetDlgItem(dlg, IDC_ADV_FMUNIT);
  SendMessage(c, CB_RESETCONTENT, 0, 0);
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Auto (When the Game Uses It)");
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Off");
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"On");
  SendMessage(c, CB_SETCURSEL, (WPARAM)adv.fmunit, 0);

  c = GetDlgItem(dlg, IDC_ADV_OPLL);
  SendMessage(c, CB_RESETCONTENT, 0, 0);
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"MAME (YM2413)");
  SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Nuked (YM2413)");
  SendMessage(c, CB_SETCURSEL, (WPARAM)adv.opll, 0);

  audio_set_slider(dlg, IDC_ADV_LP,  IDC_ADV_LP_TEXT,  0, 99,  adv.lp, "%");
  audio_set_slider(dlg, IDC_ADV_LG,  IDC_ADV_LG_TEXT,  0, 200, adv.lg, "%");
  audio_set_slider(dlg, IDC_ADV_MG,  IDC_ADV_MG_TEXT,  0, 200, adv.mg, "%");
  audio_set_slider(dlg, IDC_ADV_HG,  IDC_ADV_HG_TEXT,  0, 200, adv.hg, "%");
  audio_set_slider(dlg, IDC_ADV_PCM, IDC_ADV_PCM_TEXT, 0, 200, adv.pcm, "%");

  SetDlgItemInt(dlg, IDC_ADV_LOWF, (UINT)adv.lowf, FALSE);
  SetDlgItemInt(dlg, IDC_ADV_HIGHF, (UINT)adv.highf, FALSE);
  CheckDlgButton(dlg, IDC_ADV_HQFM, adv.hqfm ? BST_CHECKED : BST_UNCHECKED);

  adv_enable_by_filter(dlg);
}

static INT_PTR CALLBACK audio_adv_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
    case WM_INITDIALOG:
      theme_apply_to_window(dlg);
      adv_load_controls(dlg);
      return TRUE;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_HSCROLL:
    {
      HWND bar = (HWND)lp;
      int pos = (int)SendMessage(bar, TBM_GETPOS, 0, 0);
      int id = GetDlgCtrlID(bar), text = 0;
      char buf[16];

      switch (id)
      {
        case IDC_ADV_LP:  adv.lp  = pos; text = IDC_ADV_LP_TEXT;  break;
        case IDC_ADV_LG:  adv.lg  = pos; text = IDC_ADV_LG_TEXT;  break;
        case IDC_ADV_MG:  adv.mg  = pos; text = IDC_ADV_MG_TEXT;  break;
        case IDC_ADV_HG:  adv.hg  = pos; text = IDC_ADV_HG_TEXT;  break;
        case IDC_ADV_PCM: adv.pcm = pos; text = IDC_ADV_PCM_TEXT; break;
        default: break;
      }
      if (text)
      {
        wsprintfA(buf, "%d%%", pos);
        SetDlgItemTextA(dlg, text, buf);
      }
      return TRUE;
    }

    case WM_COMMAND:
      switch (LOWORD(wp))
      {
        case IDC_ADV_FILTER:
          if (HIWORD(wp) == CBN_SELCHANGE)
          {
            adv.filter = (int)SendDlgItemMessage(dlg, IDC_ADV_FILTER, CB_GETCURSEL, 0, 0);
            if (adv.filter < 0) adv.filter = 0;
            adv_enable_by_filter(dlg);
          }
          return TRUE;

        case IDC_ADV_DEFAULTS:
          adv.filter = 1; adv.lp = 60;
          adv.lg = adv.mg = adv.hg = 100;
          adv.lowf = 200; adv.highf = 8000;
          adv.hqfm = 1; adv.fmunit = 0; adv.opll = 0; adv.pcm = 100;
          adv_load_controls(dlg);
          return TRUE;

        case IDOK:
        {
          int lowf = (int)GetDlgItemInt(dlg, IDC_ADV_LOWF, NULL, FALSE);
          int highf = (int)GetDlgItemInt(dlg, IDC_ADV_HIGHF, NULL, FALSE);

          if (lowf < 20 || lowf > 2000 || highf < 1000 || highf > 20000 || lowf >= highf)
          {
            MessageBoxA(dlg, "The EQ bands need a low frequency of 20-2000 Hz, a high frequency of\n"
                             "1000-20000 Hz, and the low one below the high one.",
                        APP_NAME, MB_OK | MB_ICONINFORMATION);
            return TRUE;
          }
          adv.lowf = lowf;
          adv.highf = highf;
          adv.hqfm = (IsDlgButtonChecked(dlg, IDC_ADV_HQFM) == BST_CHECKED);
          adv.fmunit = (int)SendDlgItemMessage(dlg, IDC_ADV_FMUNIT, CB_GETCURSEL, 0, 0);
          adv.opll = (int)SendDlgItemMessage(dlg, IDC_ADV_OPLL, CB_GETCURSEL, 0, 0);
          if (adv.fmunit < 0) adv.fmunit = 0;
          if (adv.opll < 0) adv.opll = 0;
          EndDialog(dlg, IDOK);
          return TRUE;
        }

        case IDCANCEL:
          EndDialog(dlg, IDCANCEL);
          return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_audio_advanced(HWND parent)
{
  int fm_mode_now = (config.ym2413 & 2) ? 0 : ((config.ym2413 & 1) ? 2 : 1);

  adv.filter = config.filter > 2 ? 0 : config.filter;
  adv.lp     = (int)((config.lp_range * 100u + 32768u) / 65536u);
  adv.lg     = config.lg;
  adv.mg     = config.mg;
  adv.hg     = config.hg;
  adv.lowf   = config.low_freq;
  adv.highf  = config.high_freq;
  adv.hqfm   = config.hq_fm ? 1 : 0;
  adv.fmunit = fm_mode_now;
  adv.opll   = config.opll ? 1 : 0;
  adv.pcm    = config.pcm_volume;

  if (gui_dialog_box(gui.large_ui ? IDD_AUDIO_ADV_LARGE : IDD_AUDIO_ADV, parent, audio_adv_proc) == IDOK)
  {
    int old_unit = fm_mode_now, old_opll = config.opll ? 1 : 0;

    config.filter    = (uint8)adv.filter;
    config.lp_range  = (uint32)((adv.lp * 65536) / 100);
    config.lg        = (int16)adv.lg;
    config.mg        = (int16)adv.mg;
    config.hg        = (int16)adv.hg;
    config.low_freq  = (int16)adv.lowf;
    config.high_freq = (int16)adv.highf;
    config.hq_fm     = (uint8)adv.hqfm;
    config.pcm_volume = (int16)adv.pcm;
    config.ym2413    = (uint8)(adv.fmunit == 0 ? 2 : (adv.fmunit == 2 ? 1 : 0));
    config.opll      = (uint8)adv.opll;

    /* Filter, strength and EQ take effect straight away. */
    if (emu_running) audio_set_equalizer();

    /* A different Master System FM unit or chip means creating the chip anew. */
    if (adv.fmunit != old_unit || adv.opll != old_opll) emu_apply_fm_settings();

    gui_update_menu();
    config_save();
    gui_status("Advanced audio settings saved");
  }
}

/****************************************************************************
 * Keyboard shortcuts
 ****************************************************************************/

static INT_PTR CALLBACK shortcuts_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  (void)lp;

  switch (msg)
  {
    case WM_INITDIALOG:
      theme_apply_to_window(dlg);
      {
        /* One consistent tab stop, wide enough for the longest key name
           ("Backspace (held)"), rather than a hand-guessed mix of one and
           two tabs per line -- that guessing is exactly what left "Toggle
           fullscreen" misaligned from its neighbours before. */
        int tabs[1] = { 190 };
        SendDlgItemMessage(dlg, IDC_SHORTCUTS_TEXT, EM_SETTABSTOPS, 1, (LPARAM)tabs);
      }

      SetDlgItemTextA(dlg, IDC_SHORTCUTS_TEXT,
        "Open a ROM\tCtrl+O\r\n"
        "ROM Browser\tCtrl+B\r\n"
        "Close the ROM\tCtrl+W\r\n"
        "Reset\tCtrl+R\r\n"
        "Hard Reset\tCtrl+Shift+R\r\n"
        "Cheats\tCtrl+C\r\n"
        "\r\n"
        "Pause and Resume\tF2 / Pause\r\n"
        "Stop the ROM\tF3\r\n"
        "Save to the Current Slot\tF5\r\n"
        "Load from the Current Slot\tF8\r\n"
        "Previous / Next Slot\tF6 / F7\r\n"
        "Undo Load State\tF9\r\n"
        "Save a Screenshot\tF12\r\n"
        "Record Video + Audio (Start / Stop)\tShift+F11\r\n"
        "Record Audio Only (Start / Stop)\tCtrl+F11\r\n"
        "Stop Recording\tAlt+F11\r\n"
        "\r\n"
        "Toggle Fullscreen\tAlt+Enter\r\n"
        "Show/Hide the Menu Bar (While Fullscreen)\tEsc\r\n"
        "\r\n"
        "Fast Forward\tTab (Hold)\r\n"
        "Rewind\tBackspace (Hold)\r\n"
        "Advance One Frame While Paused\t\\\r\n"
        "\r\n"
        "Player 1 Defaults:\r\n"
        "D-pad\tArrow keys\r\n"
        "A B C\tZ X C\r\n"
        "X Y Z\tA S D\r\n"
        "Start\tEnter\r\n"
        "Mode\tRight Shift\r\n"
        "\r\n"
        "A connected gamepad works without setup. Change any of the pad\r\n"
        "bindings under Input > Configure Player.");
      return TRUE;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_COMMAND:
      if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)
      {
        EndDialog(dlg, LOWORD(wp));
        return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_shortcuts(HWND parent)
{
  gui_dialog_box(gui.large_ui ? IDD_SHORTCUTS_LARGE : IDD_SHORTCUTS, parent, shortcuts_proc);
}

static INT_PTR CALLBACK menu_guide_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  (void)lp;

  switch (msg)
  {
    case WM_INITDIALOG:
      theme_apply_to_window(dlg);
      {
        int tabs[1] = { 130 };
        SendDlgItemMessage(dlg, IDC_MENUGUIDE_TEXT, EM_SETTABSTOPS, 1, (LPARAM)tabs);
      }

      SetDlgItemTextA(dlg, IDC_MENUGUIDE_TEXT,
        "Every menu in one page. Keyboard shortcuts are in brackets.\r\n"
        "\r\n"
        "FILE\r\n"
        "  Open ROM... (Ctrl+O) - Pick a game file to play: Mega Drive / Genesis, Master System, Game Gear, SG-1000, Mega CD, or a .zip / .gz that holds one.\r\n"
        "  ROM Directory... (Ctrl+B) - Shows the game browser for a folder of games. Double-click a game to play it, right-click for more choices (play with a saved state, cheats, cover image).\r\n"
        "  Recent Files - Your last games, each shown with its console.\r\n"
        "  Close ROM (Ctrl+W) - Stops the game.\r\n"
        "  ROM Information... - Details about the loaded game.\r\n"
        "  Open Genesis Plus GX Folder - Opens the program folder: settings, saves, states, screenshots, recordings, cheats, covers and BIOS files.\r\n"
        "  Exit - Quits.\r\n"
        "\r\n"
        "EMULATION\r\n"
        "  Pause (F2 / Pause), Stop (F3) - Freeze the game, or end it.\r\n"
        "  Reset (Ctrl+R), Hard Reset (Ctrl+Shift+R) - Reset is the console's reset button. Hard Reset power-cycles the console (saved game data is kept).\r\n"
        "  Save State (F5), Load State (F8) - Save or restore the exact moment in the current slot.\r\n"
        "  Save Slot - Choose slot 0-9 (F6 / F7 = previous / next). Each entry shows when the slot was last saved.\r\n"
        "  Manage States... - Thumbnails of all ten slots: left-click loads one, right-click saves or deletes.\r\n"
        "  Run-Ahead (Off, 1, 2, 3 Frames) - Lowers input lag by running the game a few frames ahead and drawing the latest one. Costs extra CPU; off during netplay and recording.\r\n"
        "  Enable Rewind (Backspace, Hold) - Keeps a history of recent play so holding Backspace runs it backwards. Uses about 200 MB and some CPU while on; not available during netplay.\r\n"
        "  Undo Load State (F9) - Puts the game back as it was just before the last state load.\r\n"
        "  Pause When Inactive - Pauses the game while another window has focus.\r\n"
        "\r\n"
        "VIEW\r\n"
        "  List View, Grid View - How the game browser shows your games.\r\n"
        "  Theme - Follow Windows Setting, Light or Dark.\r\n"
        "  Larger UI - Bigger menus and dialogs.\r\n"
        "\r\n"
        "VIDEO\r\n"
        "  Window Size - 1x to 6x.\r\n"
        "  Fullscreen (Alt+Enter), Start ROM in Fullscreen - Press Esc in fullscreen to show or hide the menu bar; the mouse hides itself after a few idle seconds.\r\n"
        "  Always on Top - Keeps the window above every other window.\r\n"
        "  Render Filter - Pixel-art scalers and CRT looks (Scale2x, Smooth, xBRZ, Scanlines, CRT...) applied before the picture is shown.\r\n"
        "  Visual Effects - Smooth Scaling (soft edges when enlarged), Brighten (a gentle brightness lift), LCD Ghosting (bright pixels fade slowly, like a handheld screen).\r\n"
        "  Scanlines - Dark lines between rows: Off, 25, 50, 75 or 100 %.\r\n"
        "  NTSC Filter - Composite, S-Video or RGB television look.\r\n"
        "  Aspect Ratio - Square Pixels, 4:3 (as on a CRT) or Fill the Window.\r\n"
        "  Borders - Hide or show the overscan borders (top/bottom, left/right).\r\n"
        "  Interlaced Mode - Single Field (the default) or Double Field, for games that use the 448-line interlaced mode.\r\n"
        "  Frameskip - Skips drawing some frames on a slow PC (Off, Auto, or Manual by audio-buffer level). Not used during netplay or recording.\r\n"
        "  Renderer - GDI draws with the CPU; Direct3D 9 and Direct3D 11 use the graphics card instead (each falls back to GDI automatically if it isn't available). Direct3D 11 is what a GPU shader tool such as librashader needs.\r\n"
        "  VSync - Wait for the screen's refresh after each frame so motion is even. Works with any renderer; it is skipped for a frame whenever the sound buffer runs low, so it never causes crackle. Off by default.\r\n"
        "\r\n"
        "AUDIO\r\n"
        "  Mute - Silences the sound; untick to hear it again.\r\n"
        "  Levels and Latency... - Master volume, FM and PSG levels, CD audio, and how many frames of sound are buffered.\r\n"
        "  Advanced... - Audio filter (Off, Low-Pass or a 3-Band EQ) and its settings, High-Quality FM, Master System FM unit and chip, and Sega CD PCM volume.\r\n"
        "  FM Chip - Which Genesis FM sound chip is emulated: YM2612 or YM3438, MAME or the more exact Nuked versions.\r\n"
        "  Sample Rate - 44100 or 48000 Hz.\r\n"
        "  High-Quality PSG Resampling, Low-Pass Filter, Mono Output - Cleaner PSG sound, a soft high-frequency roll-off, and mono instead of stereo.\r\n"
        "\r\n"
        "INPUT\r\n"
        "  Configure Player 1 / 2... - Assign keyboard keys and gamepad buttons, and set the gamepad thumbstick's deadzone (how far it has to move off-center before it registers at all). Player 1 defaults: arrows, Z X C = A B C, A S D = X Y Z, Enter = Start, Right Shift = Mode.\r\n"
        "  Port A / Port B Device - What is plugged in: control pad, mouse, light gun and so on.\r\n"
        "  Enable Background Input - Keep reading the controller while another window has focus.\r\n"
        "\r\n"
        "TOOLS\r\n"
        "  Cheats... (Ctrl+C) - Add Game Genie, Action Replay or raw codes, or load a .cht file.\r\n"
        "  Netplay... / Stop Netplay - Two-player game over a local network: one PC hosts, the other joins. Both need the same ROM, the same program file and the same settings.\r\n"
        "  Record - Video + Audio (Shift+F11) makes an .mp4, Audio Only (Ctrl+F11) a .wav. Files go in the recordings folder; Stop Recording (Alt+F11) ends it.\r\n"
        "  Save Screenshot (F12) - Saves a PNG named after the game and the time.\r\n"
        "  Screenshot Output - Final: the render filter is applied, then the picture is stretched to 4:3. Corrected: the plain picture stretched to 4:3 at 640x480. Raw: exactly what the emulator drew, at its own size.\r\n"
        "\r\n"
        "OPTIONS\r\n"
        "  Region - Which region the console reports to the game: detect from the ROM, USA, Europe or Japan.\r\n"
        "  Force VDP Mode - Run the video timing at 60 Hz (NTSC) or 50 Hz (PAL) whatever the region says.\r\n"
        "  Console - Detect the console from the game, or force a specific model.\r\n"
        "  Lock-On Cartridge - Attach a Game Genie, Action Replay or Sonic & Knuckles to the game.\r\n"
        "  Boot from BIOS When Available - Run the console's start-up ROM when a BIOS file is present (see below).\r\n"
        "  Emulate Address Error Exceptions - Hardware-accurate crashes on bad memory access; turn off only for a few hacked games.\r\n"
        "  Show Extended Game Gear Screen - Show the whole Game Gear picture instead of only the part its screen displayed.\r\n"
        "  Show Master System Side Borders - Off by default, which crops the 8-pixel columns at each side; tick to show them.\r\n"
        "  Show Frame Rate - Frames per second on screen.\r\n"
        "\r\n"
        "HELP\r\n"
        "  Keyboard Shortcuts (F1) - The list of hotkeys.\r\n"
        "  Menu Guide - This page.\r\n"
        "  About Genesis Plus GX - Version and credits.\r\n"
        "\r\n"
        "BIOS AND FIRMWARE FILES\r\n"
        "Required or optional firmware files go in the program's \"bios\" folder (Open Genesis Plus GX Folder, then bios).\r\n"
        "\r\n"
        "Filename:\tDescription:\r\n"
        "\r\n"
        "\x95 bios_MD.bin\tMega Drive TMSS start-up ROM - optional\r\n"
        "\x95 bios_CD_E.bin\tMega CD Europe BIOS - required for Mega CD Europe games\r\n"
        "\x95 bios_CD_U.bin\tSega CD USA BIOS - required for Sega CD USA games\r\n"
        "\x95 bios_CD_J.bin\tMega CD Japan BIOS - required for Mega CD Japan games\r\n"
        "\x95 bios_E.sms\tMaster System Europe BIOS - optional\r\n"
        "\x95 bios_U.sms\tMaster System USA BIOS - optional\r\n"
        "\x95 bios_J.sms\tMaster System Japan BIOS - optional\r\n"
        "\x95 bios.gg\tGame Gear BIOS - optional\r\n"
        "\x95 sk.bin\tSonic & Knuckles ROM (lock-on) - optional\r\n"
        "\x95 sk2chip.bin\tSonic & Knuckles UPMEM ROM (lock-on) - optional\r\n"
        "\x95 areplay.bin\tAction Replay ROM (lock-on) - optional\r\n"
        "\x95 ggenie.bin\tGame Genie ROM (lock-on) - optional");
      return TRUE;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_COMMAND:
      if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)
      {
        EndDialog(dlg, LOWORD(wp));
        return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_menu_guide(HWND parent)
{
  gui_dialog_box(gui.large_ui ? IDD_MENUGUIDE_LARGE : IDD_MENUGUIDE, parent, menu_guide_proc);
}

static const char *system_name(void)
{
  if (system_hw == SYSTEM_MCD) return "Sega CD / Mega CD";
  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) return "Genesis / Mega Drive";
  if (system_hw == SYSTEM_GG || system_hw == SYSTEM_GGMS) return "Game Gear";
  if (system_hw == SYSTEM_SMS || system_hw == SYSTEM_SMS2) return "Master System";
  if (system_hw == SYSTEM_SG || system_hw == SYSTEM_SGII || system_hw == SYSTEM_SGII_RAM_EXT) return "SG-1000";
  return "Unknown";
}

static INT_PTR CALLBACK rominfo_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  (void)lp;

  switch (msg)
  {
    case WM_INITDIALOG:
      theme_apply_to_window(dlg);
      {
        char text[2048];
        char domestic[50], international[50], product[14], romtype_str[4],
             country[18], copyright[18];
        uint32 rom_size;

        /* Copied out and trimmed -- the core's own fields are fixed-size
           and space-padded, not necessarily null-terminated at the point
           the real text ends. */
        lstrcpynA(domestic, rominfo.domestic, sizeof(domestic));
        lstrcpynA(international, rominfo.international, sizeof(international));
        lstrcpynA(product, rominfo.product, sizeof(product));
        lstrcpynA(romtype_str, rominfo.ROMType, sizeof(romtype_str));
        lstrcpynA(country, rominfo.country, sizeof(country));
        lstrcpynA(copyright, rominfo.copyright, sizeof(copyright));

        rom_size = rominfo.romend - rominfo.romstart + 1;

        wsprintfA(text,
          "File:\t%s\r\n"
          "System:\t%s\r\n"
          "\r\n"
          "Domestic title:\t%s\r\n"
          "International title:\t%s\r\n"
          "\r\n"
          "Product code:\t%s\r\n"
          "Region:\t%s\r\n"
          "Copyright:\t%s\r\n"
          "\r\n"
          "ROM size:\t%lu KB\r\n"
          "Header checksum:\t%04X\r\n"
          "Calculated checksum:\t%04X %s\r\n",
          emu_rom_filename()[0] ? emu_rom_filename() : "(unknown)",
          system_name(),
          domestic[0] ? domestic : "(none)",
          international[0] ? international : "(none)",
          product[0] ? product : "(none)",
          country[0] ? country : "(none)",
          copyright[0] ? copyright : "(none)",
          (unsigned long)(rom_size / 1024),
          rominfo.checksum,
          rominfo.realchecksum,
          (rominfo.checksum == rominfo.realchecksum) ? "(match)" : "(mismatch)");

        {
          int tabs[1] = { 100 };
          SendDlgItemMessage(dlg, IDC_ROMINFO_TEXT, EM_SETTABSTOPS, 1, (LPARAM)tabs);
        }
        SetDlgItemTextA(dlg, IDC_ROMINFO_TEXT, text);
      }
      return TRUE;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_COMMAND:
      if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)
      {
        EndDialog(dlg, LOWORD(wp));
        return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_rom_info(HWND parent)
{
  if (!emu_running && !emu_rom_filename()[0]) return;
  gui_dialog_box(gui.large_ui ? IDD_ROMINFO_LARGE : IDD_ROMINFO, parent, rominfo_proc);
}

/****************************************************************************
 * About
 ****************************************************************************/

static INT_PTR CALLBACK about_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  (void)lp;

  switch (msg)
  {
    case WM_INITDIALOG:
      theme_apply_to_window(dlg);
      if (gui.large_ui)
      {
        HICON big_icon = (HICON)LoadImageA(g_inst, MAKEINTRESOURCEA(IDI_APPICON),
                                            IMAGE_ICON, 64, 64, 0);
        if (big_icon) SendDlgItemMessage(dlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)big_icon, 0);
      }
      SetDlgItemTextA(dlg, IDC_ABOUT_TEXT,
        "Genesis Plus GX\r\n"
        "Sega Mega Drive / Genesis, Master System, Game Gear,\r\n"
        "SG-1000, Mega CD and Pico emulator.\r\n"
        "\r\n"
        "Core by Charles MacDonald (1998-2003) and\r\n"
        "Eke-Eke (2007-2026).\r\n"
        "Windows GUI by Hazem Abdelghani (2026).\r\n"
        "\r\n"
        "Distributed under the Genesis Plus GX licence: source\r\n"
        "must accompany modified redistributions, and it may\r\n"
        "not be sold or used commercially.");

      SendDlgItemMessage(dlg, IDC_ABOUT_LINK1, WM_SETTEXT, 0,
        (LPARAM)"<a href=\"https://github.com/ekeeke/Genesis-Plus-GX\">Genesis Plus GX</a>");
      SendDlgItemMessage(dlg, IDC_ABOUT_LINK2, WM_SETTEXT, 0,
        (LPARAM)"<a href=\"https://github.com/hazem-abdelghani/Genesis-Plus-GX\">Genesis Plus GX - Windows GUI</a>");
      return TRUE;

    case WM_NOTIFY:
    {
      NMHDR *hdr = (NMHDR *)lp;
      if ((hdr->code == NM_CLICK || hdr->code == NM_RETURN) &&
          (hdr->idFrom == IDC_ABOUT_LINK1 || hdr->idFrom == IDC_ABOUT_LINK2))
      {
        NMLINK *link = (NMLINK *)lp;
        char url[256];

        WideCharToMultiByte(CP_ACP, 0, link->item.szUrl, -1, url, sizeof(url), NULL, NULL);
        ShellExecuteA(dlg, "open", url, NULL, NULL, SW_SHOWNORMAL);
        return TRUE;
      }
      break;
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_COMMAND:
      if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)
      {
        EndDialog(dlg, LOWORD(wp));
        return TRUE;
      }
      return FALSE;

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_about(HWND parent)
{
  gui_dialog_box(gui.large_ui ? IDD_ABOUT_LARGE : IDD_ABOUT, parent, about_proc);
}

/****************************************************************************
 * Mouse cursor while a dialog is open
 ****************************************************************************/

/* Makes sure the cursor is visible. Returns how many ShowCursor(TRUE) calls
   it took (always at least one), so gui_cursor_restore() can undo exactly
   that and leave the counter where fullscreen had put it. */
int gui_cursor_show(void)
{
  int calls = 1;

  while (ShowCursor(TRUE) < 0 && calls < 64) calls++;
  return calls;
}

void gui_cursor_restore(int hidden_count)
{
  while (hidden_count-- > 0) ShowCursor(FALSE);
}

INT_PTR gui_dialog_box(int template_id, HWND parent, DLGPROC proc)
{
  INT_PTR r;
  int c = gui_cursor_show();

  r = DialogBoxParamA(g_inst, MAKEINTRESOURCEA(template_id), parent, proc, 0);
  gui_cursor_restore(c);
  return r;
}

/****************************************************************************
 * Open-file picker
 *
 * Uses the Vista+ "common item" dialog instead of GetOpenFileName. The old
 * API lets Windows restore its own remembered state -- last folder, window
 * size / maximised, even a previous search -- and only treats the start
 * folder as a hint. Here the start folder is forced (SetFolder) and the
 * remembered state is cleared every time, per picker (each has its own
 * GUID so they can't affect one another).
 *
 * `filter` uses the same "Name\0pattern\0Name\0pattern\0\0" layout as
 * OPENFILENAME. Returns 1 and fills `out` with the chosen path, else 0.
 ****************************************************************************/

static const GUID pick_guids[GUI_PICK_COUNT] =
{
  { 0x6f1a5c10, 0x3b7e, 0x4c1a, { 0x9a, 0x21, 0x5d, 0x0e, 0x7b, 0x11, 0xa0, 0x01 } },  /* Open ROM   */
  { 0x6f1a5c10, 0x3b7e, 0x4c1a, { 0x9a, 0x21, 0x5d, 0x0e, 0x7b, 0x11, 0xa0, 0x02 } },  /* Cheat file */
  { 0x6f1a5c10, 0x3b7e, 0x4c1a, { 0x9a, 0x21, 0x5d, 0x0e, 0x7b, 0x11, 0xa0, 0x03 } }   /* Cover      */
};

static int pick_file_legacy(HWND owner, const char *title, const char *filter,
                            const char *start_dir, char *out, int out_len)
{
  OPENFILENAMEA ofn;

  out[0] = '\0';
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize     = sizeof(ofn);
  ofn.hwndOwner       = owner;
  ofn.lpstrFile       = out;
  ofn.nMaxFile        = (DWORD)out_len;
  ofn.lpstrTitle      = title;
  ofn.lpstrFilter     = filter;
  ofn.nFilterIndex    = 1;
  ofn.lpstrInitialDir = (start_dir && start_dir[0]) ? start_dir : NULL;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
              OFN_EXPLORER | OFN_NOCHANGEDIR;

  return GetOpenFileNameA(&ofn) ? 1 : 0;
}

static int pick_file_impl(HWND owner, int which, const char *title, const char *filter,
                          const char *start_dir, char *out, int out_len)
{
  static wchar_t names[8][128], pats[8][256];
  COMDLG_FILTERSPEC specs[8];
  wchar_t wtitle[128], wpath[GUI_PATH_LEN];
  IFileOpenDialog *dlg = NULL;
  HRESULT hr, hr_init;
  const char *p;
  FILEOPENDIALOGOPTIONS opts = 0;
  int n = 0, ok = 0;

  if (!out || out_len <= 0) return 0;
  out[0] = '\0';
  if (which < 0 || which >= GUI_PICK_COUNT) which = 0;

  hr_init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IFileOpenDialog, (void **)&dlg);
  if (FAILED(hr) || !dlg)
  {
    if (SUCCEEDED(hr_init)) CoUninitialize();
    return pick_file_legacy(owner, title, filter, start_dir, out, out_len);
  }

  IFileOpenDialog_SetClientGuid(dlg, &pick_guids[which]);
  IFileOpenDialog_ClearClientData(dlg);

  MultiByteToWideChar(CP_ACP, 0, title, -1, wtitle, 128);
  IFileOpenDialog_SetTitle(dlg, wtitle);

  for (p = filter; p && *p && n < 8; n++)
  {
    MultiByteToWideChar(CP_ACP, 0, p, -1, names[n], 128);
    p += lstrlenA(p) + 1;
    MultiByteToWideChar(CP_ACP, 0, p, -1, pats[n], 256);
    p += lstrlenA(p) + 1;
    specs[n].pszName = names[n];
    specs[n].pszSpec = pats[n];
  }
  if (n > 0)
  {
    IFileOpenDialog_SetFileTypes(dlg, (UINT)n, specs);
    IFileOpenDialog_SetFileTypeIndex(dlg, 1);
  }

  IFileOpenDialog_GetOptions(dlg, &opts);
  IFileOpenDialog_SetOptions(dlg, opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST |
                                  FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);

  if (start_dir && start_dir[0])
  {
    IShellItem *folder = NULL;

    MultiByteToWideChar(CP_ACP, 0, start_dir, -1, wpath, GUI_PATH_LEN);
    if (SUCCEEDED(SHCreateItemFromParsingName(wpath, NULL, &IID_IShellItem, (void **)&folder)) && folder)
    {
      IFileOpenDialog_SetFolder(dlg, folder);
      IShellItem_Release(folder);
    }
  }

  hr = IFileOpenDialog_Show(dlg, owner);
  if (SUCCEEDED(hr))
  {
    IShellItem *item = NULL;

    if (SUCCEEDED(IFileOpenDialog_GetResult(dlg, &item)) && item)
    {
      PWSTR w = NULL;

      if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &w)) && w)
      {
        ok = WideCharToMultiByte(CP_ACP, 0, w, -1, out, out_len, NULL, NULL) > 0;
        CoTaskMemFree(w);
      }
      IShellItem_Release(item);
    }
  }

  IFileOpenDialog_Release(dlg);
  if (SUCCEEDED(hr_init)) CoUninitialize();

  if (!ok) out[0] = '\0';
  return ok;
}

int gui_pick_file(HWND owner, int which, const char *title, const char *filter,
                  const char *start_dir, char *out, int out_len)
{
  int c = gui_cursor_show();
  int r = pick_file_impl(owner, which, title, filter, start_dir, out, out_len);

  gui_cursor_restore(c);
  return r;
}

/****************************************************************************
 * Netplay (LAN)
 ****************************************************************************/

static np_found np_hosts[8];
static int      np_host_count;

static void np_dlg_status(HWND dlg, const char *text)
{
  SetDlgItemTextA(dlg, IDC_NP_STATUS, text);
}

/* In dark mode the system draws a group box's title in black whatever colour
   the dialog asks for, which vanishes against the dark background. So the
   group boxes of this dialog paint themselves: a thin grey frame and a light
   title. (Only the frame and title are drawn; everything inside is left to the
   controls on top.) */
static LRESULT CALLBACK np_groupbox_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                         UINT_PTR id, DWORD_PTR ref)
{
  (void)ref;

  if (msg == WM_ERASEBKGND) return 1;   /* the dialog has already painted the background */

  if (msg == WM_PAINT)
  {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    char text[128];
    HFONT font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    HGDIOBJ old_font = SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    SIZE sz;
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HBRUSH bg = CreateSolidBrush(RGB(32, 32, 32));
    RECT patch;

    GetClientRect(hwnd, &rc);
    GetWindowTextA(hwnd, text, sizeof(text));
    GetTextExtentPoint32A(dc, text, lstrlenA(text), &sz);

    Rectangle(dc, rc.left, rc.top + sz.cy / 2, rc.right, rc.bottom);

    /* Clear the stretch of frame behind the title, then write the title. */
    patch.left = rc.left + 7; patch.right = rc.left + 13 + sz.cx;
    patch.top = rc.top;       patch.bottom = rc.top + sz.cy;
    FillRect(dc, &patch, bg);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(240, 240, 240));
    TextOutA(dc, rc.left + 10, rc.top, text, lstrlenA(text));

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_font);
    DeleteObject(pen);
    DeleteObject(bg);
    EndPaint(hwnd, &ps);
    return 0;
  }

  if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, np_groupbox_proc, id);

  return DefSubclassProc(hwnd, msg, wp, lp);
}

static BOOL CALLBACK np_untheme_groupbox(HWND child, LPARAM lp)
{
  char cls[16];

  (void)lp;
  if (GetClassNameA(child, cls, sizeof(cls)) && !lstrcmpiA(cls, "Button") &&
      (GetWindowLongPtr(child, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX)
  {
    SetWindowSubclass(child, np_groupbox_proc, 1, 0);
    InvalidateRect(child, NULL, TRUE);
  }
  return TRUE;
}

static INT_PTR CALLBACK netplay_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
    case WM_INITDIALOG:
    {
      char buf[160];
      HWND combo = GetDlgItem(dlg, IDC_NP_DELAY);
      int i;

      theme_apply_to_window(dlg);
      if (theme_is_dark()) EnumChildWindows(dlg, np_untheme_groupbox, 0);

      wsprintfA(buf, "%d", gui.np_port);
      SetDlgItemTextA(dlg, IDC_NP_HOST_PORT, buf);
      SetDlgItemTextA(dlg, IDC_NP_PORT, buf);
      SetDlgItemTextA(dlg, IDC_NP_ADDR, gui.np_addr);

      for (i = 1; i <= 4; i++)
      {
        wsprintfA(buf, "%d frame%s", i, i == 1 ? "" : "s");
        SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)buf);
      }
      SendMessage(combo, CB_SETCURSEL, (WPARAM)(gui.np_delay - 1), 0);

      wsprintfA(buf, "Your address on this network: %s", netplay_local_addresses()[0] ? netplay_local_addresses() : "(unknown)");
      SetDlgItemTextA(dlg, IDC_NP_MYADDR, buf);
      np_dlg_status(dlg, "Both players must load the same ROM and use the same gpgx.exe.");
      return TRUE;
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br;

      /* The list of hosts: #333333 in dark mode (the same shade as the other lists). */
      if (GetDlgCtrlID((HWND)lp) == IDC_NP_LIST)
        br = theme_ctlcolor_custom((HDC)wp, RGB(0x33, 0x33, 0x33), 2);
      else if (msg == WM_CTLCOLOREDIT)      /* the entry boxes stand out from the dialog, as in Cheats */
        br = theme_ctlcolor_custom((HDC)wp, RGB(0x38, 0x38, 0x38), 1);
      else
        br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_COMMAND:
    {
      char err[300], code[64], addr[64], buf[32];
      int port, delay;

      switch (LOWORD(wp))
      {
        case IDC_NP_FIND:
        {
          int i;
          HWND list = GetDlgItem(dlg, IDC_NP_LIST);
          HCURSOR old = SetCursor(LoadCursorA(NULL, IDC_WAIT));

          np_dlg_status(dlg, "Looking for hosts on your network...");
          np_host_count = netplay_discover(np_hosts, 8);
          SetCursor(old);

          SendMessage(list, LB_RESETCONTENT, 0, 0);
          for (i = 0; i < np_host_count; i++)
          {
            char line[160];
            wsprintfA(line, "%s  -  %s  (%s)%s", np_hosts[i].name, np_hosts[i].game[0] ? np_hosts[i].game : "no game",
                      np_hosts[i].addr, np_hosts[i].needs_code ? "  [code needed]" : "");
            SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)line);
          }
          if (np_host_count)
          {
            SendMessage(list, LB_SETCURSEL, 0, 0);
            SetDlgItemTextA(dlg, IDC_NP_ADDR, np_hosts[0].addr);
            wsprintfA(buf, "%u", (unsigned)np_hosts[0].port);
            SetDlgItemTextA(dlg, IDC_NP_PORT, buf);
            np_dlg_status(dlg, np_hosts[0].needs_code ? "Found a host. Enter its session code, then Join." : "Found a host. Press Join.");
          }
          else
          {
            np_dlg_status(dlg, "No hosts found. Type the host's address below, or ask them to Host first.");
          }
          return TRUE;
        }

        case IDC_NP_LIST:
          if (HIWORD(wp) == LBN_SELCHANGE)
          {
            int sel = (int)SendDlgItemMessage(dlg, IDC_NP_LIST, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < np_host_count)
            {
              SetDlgItemTextA(dlg, IDC_NP_ADDR, np_hosts[sel].addr);
              wsprintfA(buf, "%u", (unsigned)np_hosts[sel].port);
              SetDlgItemTextA(dlg, IDC_NP_PORT, buf);
            }
          }
          return TRUE;

        case IDC_NP_HOST:
          GetDlgItemTextA(dlg, IDC_NP_HOST_PORT, buf, sizeof(buf));
          port = atoi(buf);
          GetDlgItemTextA(dlg, IDC_NP_HOST_CODE, code, sizeof(code));
          delay = (int)SendDlgItemMessage(dlg, IDC_NP_DELAY, CB_GETCURSEL, 0, 0) + 1;
          if (delay < 1) delay = 2;

          if (!netplay_host(port, code, delay, err, sizeof(err)))
          {
            np_dlg_status(dlg, err);
            return TRUE;
          }
          gui.np_port = port;
          gui.np_delay = delay;
          config_save();
          gui_notify("Hosting: waiting for Player 2 (%s, port %d)", netplay_local_addresses()[0] ? netplay_local_addresses() : "this PC", port);
          EndDialog(dlg, IDOK);
          return TRUE;

        case IDC_NP_JOIN:
        {
          HCURSOR old;

          GetDlgItemTextA(dlg, IDC_NP_ADDR, addr, sizeof(addr));
          GetDlgItemTextA(dlg, IDC_NP_PORT, buf, sizeof(buf));
          port = atoi(buf);
          GetDlgItemTextA(dlg, IDC_NP_JOIN_CODE, code, sizeof(code));

          np_dlg_status(dlg, "Connecting...");
          old = SetCursor(LoadCursorA(NULL, IDC_WAIT));
          port = netplay_join(addr, port, code, err, sizeof(err));
          SetCursor(old);

          if (!port)
          {
            np_dlg_status(dlg, err);
            return TRUE;
          }
          lstrcpynA(gui.np_addr, addr, sizeof(gui.np_addr));
          config_save();
          EndDialog(dlg, IDOK);
          return TRUE;
        }

        case IDCANCEL:
          EndDialog(dlg, IDCANCEL);
          return TRUE;
      }
      return FALSE;
    }

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_netplay(HWND parent)
{
  gui_dialog_box(gui.large_ui ? IDD_NETPLAY_LARGE : IDD_NETPLAY, parent, netplay_proc);
}

/****************************************************************************
 * Manage Save States
 ****************************************************************************/

#define STATEMGR_SLOTS 10

static HBITMAP statemgr_bitmaps[STATEMGR_SLOTS];

static void statemgr_refresh_slot(HWND dlg, int slot)
{
  char state_p[GUI_PATH_LEN], thumb_p[GUI_PATH_LEN], label[96], stamp[32];
  HWND btn = GetDlgItem(dlg, IDC_STATEMGR_SLOT_BASE + slot);
  HWND lbl = GetDlgItem(dlg, IDC_STATEMGR_LABEL_BASE + slot);
  int exists;

  state_path(slot, state_p, sizeof(state_p));
  exists = GetFileAttributesA(state_p) != INVALID_FILE_ATTRIBUTES;

  if (statemgr_bitmaps[slot])
  {
    SendMessage(btn, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
    DeleteObject(statemgr_bitmaps[slot]);
    statemgr_bitmaps[slot] = NULL;
  }

  if (exists)
  {
    thumb_path(slot, thumb_p, sizeof(thumb_p));
    statemgr_bitmaps[slot] = video_load_thumbnail(thumb_p);

    /* Three short lines -- "Slot N", time, date -- because the full
       "hh:mm:ss  DD/MM/YYYY" stamp is wider than a thumbnail column. */
    if (state_file_time(state_p, stamp, sizeof(stamp)) && lstrlenA(stamp) >= 20)
    {
      stamp[8] = '\0';
      wsprintfA(label, "Slot %d\n%s\n%s", slot, stamp, stamp + 10);
    }
    else
    {
      wsprintfA(label, "Slot %d", slot);
    }
  }
  else
  {
    wsprintfA(label, "Slot %d\nEmpty", slot);
  }

  /* Enabled whenever a state exists, even without a loadable thumbnail --
     an older save from before this dialog existed still has no .bmp next
     to it, and should still be loadable, just without a preview image. */
  SendMessage(btn, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)statemgr_bitmaps[slot]);
  EnableWindow(btn, exists);
  SetWindowTextA(lbl, label);
}

static INT_PTR CALLBACK statemgr_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
    case WM_INITDIALOG:
    {
      int i;
      theme_apply_to_window(dlg);
      for (i = 0; i < STATEMGR_SLOTS; i++) statemgr_refresh_slot(dlg, i);
      return TRUE;
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      HBRUSH br = theme_ctlcolor((HDC)wp);
      if (br) return (LRESULT)br;
      break;
    }

    case WM_CONTEXTMENU:
    {
      HWND target = (HWND)wp;
      int id = GetDlgCtrlID(target);

      if (id >= IDC_STATEMGR_SLOT_BASE && id < IDC_STATEMGR_SLOT_BASE + STATEMGR_SLOTS)
      {
        int slot = id - IDC_STATEMGR_SLOT_BASE;
        char state_p[GUI_PATH_LEN];
        int exists;
        HMENU menu;
        int cmd;

        state_path(slot, state_p, sizeof(state_p));
        exists = GetFileAttributesA(state_p) != INVALID_FILE_ATTRIBUTES;

        menu = CreatePopupMenu();
        AppendMenuA(menu, MF_STRING, 1, "Save to This Slot");
        AppendMenuA(menu, MF_STRING | (exists ? 0 : MF_GRAYED), 2, "Delete");

        /* Documented Windows workaround (MSDN, TrackPopupMenu remarks):
           without this, the popup can fail to dismiss correctly if this
           window isn't confirmed as the foreground one. */
        {
          /* lp is a signed screen position (negative on a monitor left of or
             above the main one), or -1 when opened from the keyboard: then
             open it at the middle of the slot's button. */
          int px = (int)(short)LOWORD(lp), py = (int)(short)HIWORD(lp);

          if (lp == (LPARAM)-1 || (px == -1 && py == -1))
          {
            RECT br;
            GetWindowRect(target, &br);
            px = (br.left + br.right) / 2;
            py = (br.top + br.bottom) / 2;
          }

          SetForegroundWindow(dlg);
          cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                               px, py, 0, dlg, NULL);
        }
        PostMessage(dlg, WM_NULL, 0, 0);
        DestroyMenu(menu);

        if (cmd == 1)
        {
          emu_save_state(slot);
          statemgr_refresh_slot(dlg, slot);
        }
        else if (cmd == 2 && exists)
        {
          state_delete(slot);
          statemgr_refresh_slot(dlg, slot);
        }
        return TRUE;
      }
      return FALSE;
    }

    case WM_COMMAND:
    {
      int id = LOWORD(wp);

      if (id >= IDC_STATEMGR_SLOT_BASE && id < IDC_STATEMGR_SLOT_BASE + STATEMGR_SLOTS &&
          HIWORD(wp) == BN_CLICKED)
      {
        int slot = id - IDC_STATEMGR_SLOT_BASE;
        char state_p[GUI_PATH_LEN];

        state_path(slot, state_p, sizeof(state_p));
        if (GetFileAttributesA(state_p) != INVALID_FILE_ATTRIBUTES)
        {
          emu_load_state(slot);
          EndDialog(dlg, IDOK);
        }
        return TRUE;
      }

      if (id == IDCANCEL)
      {
        EndDialog(dlg, IDCANCEL);
        return TRUE;
      }
      return FALSE;
    }

    case WM_DESTROY:
    {
      int i;
      for (i = 0; i < STATEMGR_SLOTS; i++)
      {
        if (statemgr_bitmaps[i])
        {
          DeleteObject(statemgr_bitmaps[i]);
          statemgr_bitmaps[i] = NULL;
        }
      }
      return FALSE;
    }

    case WM_CLOSE:
      EndDialog(dlg, IDCANCEL);
      return TRUE;
  }

  return FALSE;
}

void dlg_state_manager(HWND parent)
{
  if (!emu_running) return;
  gui_dialog_box(gui.large_ui ? IDD_STATEMGR_LARGE : IDD_STATEMGR, parent, statemgr_proc);
}
