/****************************************************************************
 *  Genesis Plus GX -- Win32 GUI frontend
 *
 *  input.c -- feeds the core's input.pad[] / input.analog[] once per frame.
 *
 *  Keyboard state is polled rather than driven from WM_KEYDOWN so that
 *  several buttons held at once behave correctly and key repeat never leaks
 *  into the emulated pad. Polling reads the global key state, so everything
 *  is gated on the window actually having focus.
 *
 *  XInput is resolved at runtime: linking against it directly would stop the
 *  executable from starting on systems where the DLL is missing, and a
 *  gamepad is optional.
 *
 *  Gamepads reach here through two independent backends, picked per pad by
 *  t_pad_map.device (see GUI_INPUT_XPAD_COUNT in gui.h):
 *
 *   - XInput, for the four modern-pad slots the rest of this file already
 *     had. Kept exactly as it was.
 *   - DirectInput, for everything XInput doesn't cover -- older joysticks,
 *     wheels, flight sticks, and any pad without an XInput-compatible
 *     driver. DirectInput itself also sees XInput-capable pads (as raw HID
 *     joysticks with none of XInput's button semantics), so those are
 *     filtered back out of the DirectInput device list on sight; a modern
 *     pad is only ever addressed through its XInput slot.
 *
 *  The two backends store completely different things in a button mask
 *  (DI_* bits vs XPAD_* bits below), so a mask is only ever meaningful
 *  alongside the device it was captured against -- every function that
 *  takes or returns one also takes that device.
 ****************************************************************************/

#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#define COBJMACROS
#include <dinput.h>

#include "shared.h"
#include "gui.h"
#include "netplay.h"

/****************************************************************************
 * XInput, loaded on demand
 ****************************************************************************/

#define XPAD_DPAD_UP        0x0001
#define XPAD_DPAD_DOWN      0x0002
#define XPAD_DPAD_LEFT      0x0004
#define XPAD_DPAD_RIGHT     0x0008
#define XPAD_START          0x0010
#define XPAD_BACK           0x0020
#define XPAD_LEFT_SHOULDER  0x0100
#define XPAD_RIGHT_SHOULDER 0x0200
#define XPAD_A              0x1000
#define XPAD_B              0x2000
#define XPAD_X              0x4000
#define XPAD_Y              0x8000
#define XPAD_LEFT_TRIGGER   0x00010000
#define XPAD_RIGHT_TRIGGER  0x00020000

#define XPAD_TRIGGER_THRESHOLD 30   /* out of 0-255; comfortably past resting noise */

#define XPAD_MAX_DEVICES    GUI_INPUT_XPAD_COUNT   /* gui.h -- also where DirectInput's own device range starts */
#define XPAD_DEADZONE       10000

/* gui.deadzone[] is 0-100%, the units a person actually sees and adjusts on
   the slider; everywhere else in this file wants it in XInput's own
   -32768..32767 raw units instead. Falls back to this file's own prior
   fixed constant, scaled to a percentage, for a player index outside 0/1 --
   should never actually happen, but a sensible default beats an out-of-
   bounds array read if it ever did. */
static int deadzone_raw(int player)
{
  int pct = (player == 0 || player == 1) ? gui.deadzone[player] : 30;

  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (pct * 32767) / 100;
}

/* Applies a deadzone to one raw analog stick axis (-32768..32767): exactly
   centered (returns 0) anywhere within the deadzone, unchanged otherwise --
   deliberately not also rescaling the remaining range, so this only adds
   the deadzone itself and doesn't change this file's existing analog
   sensitivity/range in the process. */
static int deadzone_clamp(int raw, int dz)
{
  int mag = (raw < 0) ? -raw : raw;
  return (mag <= dz) ? 0 : raw;
}

typedef struct
{
  WORD  wButtons;
  BYTE  bLeftTrigger;
  BYTE  bRightTrigger;
  SHORT sThumbLX;
  SHORT sThumbLY;
  SHORT sThumbRX;
  SHORT sThumbRY;
} XPAD_GAMEPAD;

typedef struct
{
  DWORD        dwPacketNumber;
  XPAD_GAMEPAD Gamepad;
} XPAD_STATE;

typedef DWORD (WINAPI *xinput_get_state_fn)(DWORD, XPAD_STATE *);

static HMODULE             xinput_dll;
static xinput_get_state_fn xinput_get_state;

/* Cached once per frame so four devices do not mean four polls each. */
static XPAD_STATE xpad_state[XPAD_MAX_DEVICES];
static int        xpad_connected[XPAD_MAX_DEVICES];

static int  has_focus = 1;
static int  frame_advance_armed;

/****************************************************************************
 * DirectInput, for joysticks/pads XInput doesn't cover
 *
 * Devices are polled through IDirectInputDevice8::GetDeviceState rather than
 * the buffered/event-driven API: this file already re-reads everything once
 * a frame (and, for the fast-forward/rewind hotkeys, on demand outside the
 * frame loop too), so there is nothing to buffer between reads.
 ****************************************************************************/

/* First 24 joystick buttons as individual bits, then the D-pad/POV/stick
   folded together the same way gamepad_buttons() already folds XInput's
   thumbstick into its own d-pad bits below -- one set of 4 "direction" bits
   fed from whichever of POV hat or main stick is actually deflected, rather
   than separate, individually mappable bits for each. Distinct numeric
   values from the XPAD_* bits above are not required (a mask is only ever
   compared against another mask captured for the very same device, never
   across backends), but bits 24-27 are used here anyway to leave buttons
   25-32 of a device with that many free for the future without renumbering
   anything already saved to gpgx.ini. */
#define DI_MAX_BUTTONS   24
#define DI_BTN(n)        (1u << (n))
#define DI_DPAD_UP       (1u << 24)
#define DI_DPAD_DOWN     (1u << 25)
#define DI_DPAD_LEFT     (1u << 26)
#define DI_DPAD_RIGHT    (1u << 27)

static IDirectInput8A *di_iface;

typedef struct
{
  GUID guidInstance;
  char name[64];
} di_joy_info;

static di_joy_info          joy_info[GUI_INPUT_DI_MAX];
static int                  joy_count;
static IDirectInputDevice8A *joy_dev[GUI_INPUT_DI_MAX];
static DIJOYSTATE2          joy_state[GUI_INPUT_DI_MAX];
static int                  joy_connected[GUI_INPUT_DI_MAX];

/* Same slow-reprobe idea as xpad_known[]/xpad_next_probe[] below: a device
   that failed to Acquire (unplugged, or another app holding it exclusively)
   is only retried about once a second rather than on every single call. */
static int   joy_known[GUI_INPUT_DI_MAX];
static DWORD joy_next_probe[GUI_INPUT_DI_MAX];

static int is_di_device(int device)
{
  return device >= GUI_INPUT_XPAD_COUNT && device < GUI_INPUT_XPAD_COUNT + GUI_INPUT_DI_MAX;
}

/* DirectInput enumerates XInput-capable pads too, as plain HID joysticks
   with none of XInput's button/trigger semantics -- this app already talks
   to those through the XInput backend above, so they're filtered back out
   here rather than showing up a second time as an unlabelled generic
   joystick. Standard technique (see the DirectX SDK's XInput/DirectInput
   sample): an XInput device's underlying HID interface name always
   contains "IG_", and DirectInput's own product GUID happens to pack the
   same vendor/product IDs Raw Input reports for that HID device into its
   first 4 bytes, so matching those two together identifies it without ever
   opening the device itself. */
static int is_xinput_device(const GUID *guid_product)
{
  UINT count = 0, i;
  RAWINPUTDEVICELIST *list;
  int found = 0;

  if (GetRawInputDeviceList(NULL, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0)
    return 0;

  list = (RAWINPUTDEVICELIST *)malloc(sizeof(RAWINPUTDEVICELIST) * count);
  if (!list) return 0;

  if (GetRawInputDeviceList(list, &count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1)
  {
    free(list);
    return 0;
  }

  for (i = 0; i < count && !found; i++)
  {
    RID_DEVICE_INFO info;
    UINT size;
    UINT namelen = 0;
    char *name;

    if (list[i].dwType != RIM_TYPEHID) continue;

    info.cbSize = sizeof(info);
    size = sizeof(info);
    if ((int)GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICEINFO, &info, &size) <= 0)
      continue;

    if ((LONG)MAKELONG((WORD)info.hid.dwVendorId, (WORD)info.hid.dwProductId) != (LONG)guid_product->Data1)
      continue;

    GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, NULL, &namelen);
    if (namelen == 0 || namelen > 4096) continue;

    name = (char *)malloc(namelen);
    if (!name) continue;

    if ((int)GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, name, &namelen) >= 0 &&
        strstr(name, "IG_"))
    {
      found = 1;
    }
    free(name);
  }

  free(list);
  return found;
}

static BOOL CALLBACK di_enum_callback(const DIDEVICEINSTANCEA *inst, void *ref)
{
  (void)ref;

  if (joy_count >= GUI_INPUT_DI_MAX) return DIENUM_STOP;
  if (is_xinput_device(&inst->guidProduct)) return DIENUM_CONTINUE;

  joy_info[joy_count].guidInstance = inst->guidInstance;
  lstrcpynA(joy_info[joy_count].name, inst->tszProductName, sizeof(joy_info[joy_count].name));
  joy_count++;
  return DIENUM_CONTINUE;
}

/* Releases every currently-open device and re-enumerates from scratch, so a
   joystick plugged in after this app started shows up too. Indices can
   shift across a refresh (the OS enumeration order isn't a stable identity),
   which is exactly what t_pad_map.joy_name/gui_input_resolve_device() below
   exist to paper over. */
void gui_input_refresh_joysticks(void)
{
  int i;

  for (i = 0; i < GUI_INPUT_DI_MAX; i++)
  {
    if (joy_dev[i])
    {
      IDirectInputDevice8_Unacquire(joy_dev[i]);
      IDirectInputDevice8_Release(joy_dev[i]);
      joy_dev[i] = NULL;
    }
    joy_known[i] = 0;
    joy_next_probe[i] = 0;
    joy_connected[i] = 0;
    ZeroMemory(&joy_state[i], sizeof(joy_state[i]));
  }

  joy_count = 0;
  if (di_iface)
  {
    IDirectInput8_EnumDevices(di_iface, DI8DEVCLASS_GAMECTRL, di_enum_callback, NULL, DIEDFL_ATTACHEDONLY);
  }
}

int gui_input_joystick_count(void)
{
  return joy_count;
}

const char *gui_input_joystick_name(int index)
{
  if (index < 0 || index >= joy_count) return "";
  return joy_info[index].name;
}

int gui_input_resolve_device(int device, const char *joy_name)
{
  int i;

  if (!is_di_device(device) || !joy_name || !joy_name[0]) return device;

  for (i = 0; i < joy_count; i++)
  {
    if (lstrcmpiA(joy_info[i].name, joy_name) == 0) return GUI_INPUT_XPAD_COUNT + i;
  }

  return device;   /* not currently attached -- keeps reading as disconnected rather than losing the assignment */
}

/* Opens the device and configures it the first time something actually asks
   for its state -- not at enumeration time, since that happens before
   create_main_window() gives this file a window handle to hand
   SetCooperativeLevel(), and there is no reason to open every attached
   joystick if only one of them ends up mapped to a player. */
static int di_ensure_acquired(int idx)
{
  DIPROPRANGE range;
  HRESULT hr;

  if (idx < 0 || idx >= joy_count) return 0;
  if (joy_dev[idx]) return 1;
  if (!di_iface || !g_hwnd) return 0;

  hr = IDirectInput8_CreateDevice(di_iface, &joy_info[idx].guidInstance, &joy_dev[idx], NULL);
  if (FAILED(hr))
  {
    joy_dev[idx] = NULL;
    return 0;
  }

  IDirectInputDevice8_SetDataFormat(joy_dev[idx], &c_dfDIJoystick2);
  /* Background: this app's own "keep controller input working while
     unfocused" option (gui.background_input) already gates every call in
     here on pad_input_allowed(), so the device itself doesn't need to
     refuse input the moment focus moves elsewhere. Non-exclusive: nothing
     here needs to stop other applications (or a second instance) from also
     reading the same joystick. */
  IDirectInputDevice8_SetCooperativeLevel(joy_dev[idx], g_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

  /* Ranged to XInput's own -32768..32767 so every place downstream that
     folds a stick into a d-pad or scales it by gui.deadzone[] treats both
     backends identically. A device that refuses this (rare) just keeps
     whatever narrower default range it already reports -- deadzone
     thresholds computed for the wider range then end up too small to
     matter for it, not too large, so the worst case is a d-pad that goes
     digital a bit earlier than the slider says, never one that doesn't
     respond at all. */
  ZeroMemory(&range, sizeof(range));
  range.diph.dwSize = sizeof(range);
  range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  range.diph.dwHow = DIPH_BYOFFSET;
  range.lMin = -32768;
  range.lMax = 32767;

  range.diph.dwObj = DIJOFS_X;
  IDirectInputDevice8_SetProperty(joy_dev[idx], DIPROP_RANGE, &range.diph);
  range.diph.dwObj = DIJOFS_Y;
  IDirectInputDevice8_SetProperty(joy_dev[idx], DIPROP_RANGE, &range.diph);

  IDirectInputDevice8_Acquire(joy_dev[idx]);
  return 1;
}

static int di_query(int idx, DIJOYSTATE2 *out)
{
  DWORD now = GetTickCount();
  HRESULT hr;

  if (idx < 0 || idx >= joy_count) return 0;
  if (!joy_known[idx] && (int)(now - joy_next_probe[idx]) < 0) return 0;

  if (!di_ensure_acquired(idx))
  {
    joy_known[idx] = 0;
    joy_next_probe[idx] = now + 1000;
    return 0;
  }

  IDirectInputDevice8_Poll(joy_dev[idx]);   /* some devices need this pumped; harmless on the rest */
  hr = IDirectInputDevice8_GetDeviceState(joy_dev[idx], sizeof(DIJOYSTATE2), out);

  if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
  {
    if (SUCCEEDED(IDirectInputDevice8_Acquire(joy_dev[idx])))
      hr = IDirectInputDevice8_GetDeviceState(joy_dev[idx], sizeof(DIJOYSTATE2), out);
  }

  if (FAILED(hr))
  {
    joy_known[idx] = 0;
    joy_next_probe[idx] = now + 1000;
    return 0;
  }

  joy_known[idx] = 1;
  return 1;
}

/* Every joystick button plus the POV hat, folded into the bits above -- just
   the parts of gamepad_buttons()'s job that don't need a deadzone. Shared by
   the once-per-frame cached poll and the on-demand fresh reads (hotkeys
   during rewind, and the "listening" capture in the config dialog) so the
   two never disagree about what a given raw state means. */
static DWORD di_raw_buttons(const DIJOYSTATE2 *js)
{
  DWORD b = 0;
  DWORD pov = js->rgdwPOV[0];
  int i;

  for (i = 0; i < DI_MAX_BUTTONS; i++)
  {
    if (js->rgbButtons[i] & 0x80) b |= DI_BTN(i);
  }

  if (LOWORD(pov) != 0xFFFF)   /* 0xFFFF (all bits of the low word set) = hat centered */
  {
    int deg = (int)(pov / 100);   /* hundredths of a degree, clockwise from north */

    if (deg >= 315 || deg <= 45)  b |= DI_DPAD_UP;
    if (deg >= 45  && deg <= 135) b |= DI_DPAD_RIGHT;
    if (deg >= 135 && deg <= 225) b |= DI_DPAD_DOWN;
    if (deg >= 225 && deg <= 315) b |= DI_DPAD_LEFT;
  }

  return b;
}

/****************************************************************************
 * Setup
 ****************************************************************************/

void gui_input_init(void)
{
  static const char *candidates[] =
  {
    "xinput1_4.dll",     /* Windows 8 and later  */
    "xinput1_3.dll",     /* DirectX SDK redist   */
    "xinput9_1_0.dll",   /* Windows 7 in-box     */
    NULL
  };
  int i;

  for (i = 0; candidates[i]; i++)
  {
    xinput_dll = LoadLibraryA(candidates[i]);
    if (xinput_dll) break;
  }

  if (xinput_dll)
  {
    xinput_get_state =
      (xinput_get_state_fn)(void *)GetProcAddress(xinput_dll, "XInputGetState");
  }

  /* Called before create_main_window(), so g_inst is already set but
     g_hwnd isn't yet -- fine here, since enumerating devices (to populate
     the config dialog's device list and let a saved joy_name resolve
     against it) needs neither; only actually opening one of them, deferred
     to di_ensure_acquired() above, needs a window handle. */
  if (SUCCEEDED(DirectInput8Create(g_inst, DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void **)&di_iface, NULL)))
  {
    gui_input_refresh_joysticks();
  }
}

void gui_input_shutdown(void)
{
  int i;

  for (i = 0; i < GUI_INPUT_DI_MAX; i++)
  {
    if (joy_dev[i])
    {
      IDirectInputDevice8_Unacquire(joy_dev[i]);
      IDirectInputDevice8_Release(joy_dev[i]);
      joy_dev[i] = NULL;
    }
  }
  joy_count = 0;

  if (di_iface)
  {
    IDirectInput8_Release(di_iface);
    di_iface = NULL;
  }

  if (xinput_dll)
  {
    FreeLibrary(xinput_dll);
    xinput_dll = NULL;
    xinput_get_state = NULL;
  }
}

void gui_input_set_focus(int focused)
{
  has_focus = focused;
}

/****************************************************************************
 * Polling helpers
 ****************************************************************************/

/* Tracks the last time either Alt or Ctrl was seen held, so a brief
   grace period after release (below) can still suppress game input --
   not just while the modifier is physically down. Both are shortcut
   modifiers used throughout this app's own accelerator table (Ctrl+O,
   Ctrl+R, Ctrl+C, etc., alongside Alt+Enter), so a key that's also
   mapped to a Genesis button shouldn't leak through as gameplay input
   during or immediately around one of those combos. */
static DWORD last_modifier_seen;

/* Controller input (pad keys, gamepads) can optionally keep working while
   another window has focus. Hotkeys and the mouse never do: typing digits
   or clicking elsewhere must not trigger save slots or lightgun shots. */
static int pad_input_allowed(void)
{
  return has_focus || gui.background_input;
}

/* Ctrl and Alt themselves. A pad button assigned to one of these has to work
   while it is held, so the shortcut-modifier suppression below cannot apply. */
static int is_ctrl_alt_key(int vk)
{
  switch (vk)
  {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU:    case VK_LMENU:    case VK_RMENU:
      return 1;
    default:
      return 0;
  }
}

static int key_down_any(int vk, int allowed)
{
  if (!vk || !allowed) return 0;

  if (is_ctrl_alt_key(vk)) return (GetAsyncKeyState(vk) & 0x8000) != 0;

  if ((GetAsyncKeyState(VK_MENU) & 0x8000) || (GetAsyncKeyState(VK_CONTROL) & 0x8000))
  {
    last_modifier_seen = GetTickCount();
    return 0;
  }
  if (GetTickCount() - last_modifier_seen < 1000) return 0;

  return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static int key_down(int vk)      { return key_down_any(vk, has_focus); }            /* hotkeys */
static int pad_key_down(int vk)  { return key_down_any(vk, pad_input_allowed()); }  /* controller keys */

/* Asking XInput about an empty controller slot can be slow (older XInput DLLs
   look for hardware each time), so a slot that was empty is only tried again
   about once a second. */
static int   xpad_known[XPAD_MAX_DEVICES];
static DWORD xpad_next_probe[XPAD_MAX_DEVICES];

static int xpad_query(int device, XPAD_STATE *st)
{
  DWORD now = GetTickCount();

  if (!xinput_get_state || device < 0 || device >= XPAD_MAX_DEVICES) return 0;
  if (!xpad_known[device] && (int)(now - xpad_next_probe[device]) < 0) return 0;

  if (xinput_get_state((DWORD)device, st) == ERROR_SUCCESS)
  {
    xpad_known[device] = 1;
    return 1;
  }

  xpad_known[device] = 0;
  xpad_next_probe[device] = now + 1000;
  return 0;
}

static void poll_gamepads(void)
{
  int i;

  for (i = 0; i < XPAD_MAX_DEVICES; i++)
  {
    xpad_connected[i] = 0;
    ZeroMemory(&xpad_state[i], sizeof(XPAD_STATE));
  }
  for (i = 0; i < GUI_INPUT_DI_MAX; i++)
  {
    joy_connected[i] = 0;
    ZeroMemory(&joy_state[i], sizeof(joy_state[i]));
  }

  if (!pad_input_allowed()) return;

  if (xinput_get_state)
  {
    for (i = 0; i < XPAD_MAX_DEVICES; i++)
    {
      if (xpad_query(i, &xpad_state[i])) xpad_connected[i] = 1;
    }
  }

  for (i = 0; i < joy_count; i++)
  {
    if (di_query(i, &joy_state[i])) joy_connected[i] = 1;
  }
}

/* Raw stick position, X and Y each -32768..32767, in XInput's own sign
   convention (Y positive = up) regardless of which backend actually holds
   the device -- DirectInput reports Y the opposite way round (down is
   positive), so the DI branch below flips it on the way out rather than
   leaving every caller to remember which backend it's talking to. Returns
   0, leaving the outputs untouched, for an unmapped or disconnected
   device. */
static int pad_stick(int device, int *x, int *y)
{
  if (device >= 0 && device < XPAD_MAX_DEVICES)
  {
    if (!xpad_connected[device]) return 0;
    *x = xpad_state[device].Gamepad.sThumbLX;
    *y = xpad_state[device].Gamepad.sThumbLY;
    return 1;
  }

  if (is_di_device(device))
  {
    int idx = device - XPAD_MAX_DEVICES;
    if (!joy_connected[idx]) return 0;
    *x = joy_state[idx].lX;
    *y = -joy_state[idx].lY;
    return 1;
  }

  return 0;
}

/* Buttons plus the left stick folded into the d-pad bits, and (XInput only)
   the analog triggers folded in as digital past XPAD_TRIGGER_THRESHOLD.
   deadzone_raw is in XInput's own -32768..32767 units, already converted by
   the caller from the 0-100% the person actually sees and adjusts (see
   gui.deadzone[]) -- kept that way here so this function has no notion of
   percentages or which player is asking, just a threshold to compare
   against, the same as the fixed constant this replaced. */
static DWORD gamepad_buttons(int device, int deadzone_raw)
{
  DWORD b;
  int x, y;

  if (device >= 0 && device < XPAD_MAX_DEVICES)
  {
    if (!xpad_connected[device]) return 0;

    b = xpad_state[device].Gamepad.wButtons;
    if (xpad_state[device].Gamepad.bLeftTrigger  > XPAD_TRIGGER_THRESHOLD) b |= XPAD_LEFT_TRIGGER;
    if (xpad_state[device].Gamepad.bRightTrigger > XPAD_TRIGGER_THRESHOLD) b |= XPAD_RIGHT_TRIGGER;
  }
  else if (is_di_device(device))
  {
    int idx = device - XPAD_MAX_DEVICES;
    if (!joy_connected[idx]) return 0;
    b = di_raw_buttons(&joy_state[idx]);
  }
  else
  {
    return 0;
  }

  if (pad_stick(device, &x, &y))
  {
    DWORD up = is_di_device(device) ? DI_DPAD_UP : XPAD_DPAD_UP;
    DWORD dn = is_di_device(device) ? DI_DPAD_DOWN : XPAD_DPAD_DOWN;
    DWORD lf = is_di_device(device) ? DI_DPAD_LEFT : XPAD_DPAD_LEFT;
    DWORD rt = is_di_device(device) ? DI_DPAD_RIGHT : XPAD_DPAD_RIGHT;

    if (y >  deadzone_raw) b |= up;
    if (y < -deadzone_raw) b |= dn;
    if (x < -deadzone_raw) b |= lf;
    if (x >  deadzone_raw) b |= rt;
  }

  return b;
}

/*
 * Returns the mapping to use for a given emulated device slot.
 *
 * Slots 0 and 1 are the two configurable players. Higher slots only exist
 * with a multitap; those get the default layout on the matching XInput pad so
 * a four-player game works without extra setup.
 */
static int is_pad_dev(int d)
{
  return d == DEVICE_PAD3B || d == DEVICE_PAD6B || d == DEVICE_PAD2B;
}

/*
 * Which emulated device slot holds Player 2's control pad, or -1 if the two
 * ports don't both have a plain pad. On a Genesis, port B's pad lives in slot
 * 4 (slots 1-3 only exist with a multitap on port A); on the 8-bit consoles
 * the second pad is slot 1.
 */
int gui_input_p2_slot(void)
{
  int i;

  if (!is_pad_dev(input.dev[0])) return -1;
  for (i = 2; i < MAX_DEVICES; i++)
    if (i != 4 && input.dev[i] != NO_DEVICE) return -1;

  if (is_pad_dev(input.dev[4]) && input.dev[1] == NO_DEVICE) return 4;
  if (is_pad_dev(input.dev[1]) && input.dev[4] == NO_DEVICE) return 1;
  return -1;
}

static void resolve_map(int slot, t_pad_map *out)
{
  /* Genesis port B: Player 2's own controls. (It used to fall through to the
     "multitap" branch below, which has no keys and no gamepad, so Player 2
     couldn't be controlled at all.) */
  if (slot == 4 && is_pad_dev(input.dev[4]) && input.dev[1] == NO_DEVICE)
  {
    *out = gui.pad[1];
  }
  else if (slot < 2)
  {
    *out = gui.pad[slot];
  }
  else
  {
    *out = gui.pad[0];
    memset(out->key, 0, sizeof(out->key));
    out->device = (slot < XPAD_MAX_DEVICES) ? slot : -1;
  }
}

/* Collects one pad's state into a set of INPUT_* bits. */
static uint16 read_pad(int slot)
{
  t_pad_map map;
  DWORD buttons;
  uint16 pad = 0;

  resolve_map(slot, &map);
  buttons = gamepad_buttons(map.device, deadzone_raw(slot));

  #define PRESSED(idx) \
    (pad_key_down(map.key[idx]) || (map.button[idx] && (buttons & map.button[idx])))

  if (PRESSED(PAD_A))     pad |= INPUT_A;
  if (PRESSED(PAD_B))     pad |= INPUT_B;
  if (PRESSED(PAD_C))     pad |= INPUT_C;
  if (PRESSED(PAD_START)) pad |= INPUT_START;
  if (PRESSED(PAD_X))     pad |= INPUT_X;
  if (PRESSED(PAD_Y))     pad |= INPUT_Y;
  if (PRESSED(PAD_Z))     pad |= INPUT_Z;
  if (PRESSED(PAD_MODE))  pad |= INPUT_MODE;

  /* Opposite directions cannot be held on real hardware. */
  if (PRESSED(PAD_UP))         pad |= INPUT_UP;
  else if (PRESSED(PAD_DOWN))  pad |= INPUT_DOWN;

  if (PRESSED(PAD_LEFT))       pad |= INPUT_LEFT;
  else if (PRESSED(PAD_RIGHT)) pad |= INPUT_RIGHT;

  #undef PRESSED

  return pad;
}

/* Cursor position expressed in emulated pixels, clamped to the frame. */
static int cursor_in_frame(int *px, int *py)
{
  POINT pt;
  RECT dest;
  int sw, sh;
  int dw, dh;

  if (!has_focus) return 0;
  if (!GetCursorPos(&pt)) return 0;
  if (!ScreenToClient(g_hwnd, &pt)) return 0;

  video_get_output_rect(&dest, &sw, &sh);

  dw = dest.right - dest.left;
  dh = dest.bottom - dest.top;
  if (dw < 1 || dh < 1) return 0;

  *px = ((pt.x - dest.left) * sw) / dw;
  *py = ((pt.y - dest.top)  * sh) / dh;

  if (*px < 0) *px = 0; else if (*px >= sw) *px = sw - 1;
  if (*py < 0) *py = 0; else if (*py >= sh) *py = sh - 1;

  return 1;
}

/*
 * Movement since the last frame, in emulated pixels.
 *
 * The Sega Mouse reports relative motion, not a position, so an absolute
 * offset from the centre of the frame would read as a steady drift towards
 * the cursor rather than as the cursor moving.
 */
static int cursor_delta(int *dx, int *dy)
{
  static int last_x, last_y;
  static int have_last;
  int x, y;

  *dx = 0;
  *dy = 0;

  if (!cursor_in_frame(&x, &y))
  {
    have_last = 0;
    return 0;
  }

  if (have_last)
  {
    *dx = x - last_x;
    *dy = y - last_y;
  }

  last_x = x;
  last_y = y;
  have_last = 1;

  return 1;
}

/* The pointer is over the game picture itself (not the menu bar, title bar,
   borders or status bar, and not another window in front of it). */
static int cursor_over_picture(void)
{
  POINT pt;
  RECT dest;

  if (!GetCursorPos(&pt)) return 0;
  if (WindowFromPoint(pt) != g_hwnd) return 0;
  if (!ScreenToClient(g_hwnd, &pt)) return 0;

  video_get_output_rect(&dest, NULL, NULL);
  return PtInRect(&dest, pt) ? 1 : 0;
}

static int mouse_button(int vk_button)
{
  if (!has_focus) return 0;
  if (!cursor_over_picture()) return 0;     /* a click on the menu or title bar is not a shot */
  return (GetAsyncKeyState(vk_button) & 0x8000) != 0;
}

/****************************************************************************
 * Per-frame update, called by the core
 ****************************************************************************/

int win32_input_update(void)
{
  int slot;
  int sw, sh;
  RECT dest;

  poll_gamepads();
  video_get_output_rect(&dest, &sw, &sh);

  for (slot = 0; slot < MAX_DEVICES; slot++)
  {
    int x = 0, y = 0;

    input.pad[slot] = 0;

    switch (input.dev[slot])
    {
      case NO_DEVICE:
        break;

      case DEVICE_MOUSE:
      {
        int dx, dy;

        if (!cursor_delta(&dx, &dy)) break;

        /* Y is reported bottom-up by the Sega Mouse. */
        input.analog[slot][0] = dx;
        input.analog[slot][1] = config.invert_mouse ? dy : -dy;

        if (mouse_button(VK_LBUTTON))  input.pad[slot] |= INPUT_MOUSE_LEFT;
        if (mouse_button(VK_RBUTTON))  input.pad[slot] |= INPUT_MOUSE_RIGHT;
        if (mouse_button(VK_MBUTTON))  input.pad[slot] |= INPUT_MOUSE_CENTER;
        if (pad_key_down(gui.pad[0].key[PAD_START])) input.pad[slot] |= INPUT_START;
        break;
      }

      case DEVICE_LIGHTGUN:
      {
        if (!cursor_in_frame(&x, &y)) break;
        input.analog[slot][0] = x;
        input.analog[slot][1] = y;
        if (mouse_button(VK_LBUTTON)) input.pad[slot] |= INPUT_A;
        if (mouse_button(VK_RBUTTON)) input.pad[slot] |= INPUT_B;
        if (mouse_button(VK_MBUTTON)) input.pad[slot] |= INPUT_C;
        if (pad_key_down(gui.pad[0].key[PAD_START])) input.pad[slot] |= INPUT_START;
        break;
      }

      case DEVICE_PADDLE:
      {
        if (cursor_in_frame(&x, &y) && sw > 0)
        {
          input.analog[slot][0] = (x * 256) / sw;
        }
        input.pad[slot] = read_pad(slot) & (INPUT_BUTTON1 | INPUT_BUTTON2 | INPUT_START);
        break;
      }

      case DEVICE_SPORTSPAD:
      {
        if (cursor_in_frame(&x, &y) && sw > 0 && sh > 0)
        {
          input.analog[slot][0] = (x * 256) / sw;
          input.analog[slot][1] = (y * 256) / sh;
        }
        input.pad[slot] = read_pad(slot) & (INPUT_BUTTON1 | INPUT_BUTTON2);
        break;
      }

      case DEVICE_PICO:
      {
        if (cursor_in_frame(&x, &y) && sw > 0 && sh > 0)
        {
          input.analog[0][0] = 0x03c + ((x * (0x17c - 0x03c + 1)) / sw);
          input.analog[0][1] = 0x1fc + ((y * (0x2f7 - 0x1fc + 1)) / sh);
        }
        if (mouse_button(VK_LBUTTON)) input.pad[slot] |= INPUT_PICO_PEN;
        if (mouse_button(VK_RBUTTON)) input.pad[slot] |= INPUT_PICO_RED;
        break;
      }

      case DEVICE_TEREBI:
      {
        if (cursor_in_frame(&x, &y) && sw > 0 && sh > 0)
        {
          input.analog[0][0] = (x * 250) / sw;
          input.analog[0][1] = (y * 250) / sh;
        }
        if (mouse_button(VK_LBUTTON)) input.pad[slot] |= INPUT_B;
        break;
      }

      case DEVICE_GRAPHIC_BOARD:
      {
        if (cursor_in_frame(&x, &y) && sw > 0 && sh > 0)
        {
          input.analog[0][0] = (x * 255) / sw;
          input.analog[0][1] = (y * 255) / sh;
        }
        if (mouse_button(VK_LBUTTON)) input.pad[slot] |= INPUT_GRAPHIC_PEN;
        if (mouse_button(VK_RBUTTON)) input.pad[slot] |= INPUT_GRAPHIC_MENU;
        if (mouse_button(VK_MBUTTON)) input.pad[slot] |= INPUT_GRAPHIC_DO;
        break;
      }

      case DEVICE_XE_1AP:
      {
        uint16 pad = read_pad(slot);
        t_pad_map map;
        DWORD buttons;

        resolve_map(slot, &map);
        buttons = gamepad_buttons(map.device, deadzone_raw(slot));

        if (pad & INPUT_A)     input.pad[slot] |= INPUT_XE_A;
        if (pad & INPUT_B)     input.pad[slot] |= INPUT_XE_B;
        if (pad & INPUT_C)     input.pad[slot] |= INPUT_XE_C;
        if (pad & INPUT_X)     input.pad[slot] |= INPUT_XE_D;
        if (pad & INPUT_START) input.pad[slot] |= INPUT_XE_START;
        if (pad & INPUT_MODE)  input.pad[slot] |= INPUT_XE_SELECT;

        /* Left stick drives the analog axes, d-pad falls back to the extremes. */
        {
          int dz = deadzone_raw(slot);
          int sx, sy;

          if (pad_stick(map.device, &sx, &sy))
          {
            input.analog[slot][0] = 128 + (deadzone_clamp(sx, dz) >> 9);
            input.analog[slot][1] = 128 - (deadzone_clamp(sy, dz) >> 9);
          }
          else
          {
            input.analog[slot][0] = (pad & INPUT_LEFT) ? 0 : ((pad & INPUT_RIGHT) ? 255 : 128);
            input.analog[slot][1] = (pad & INPUT_UP)   ? 0 : ((pad & INPUT_DOWN)  ? 255 : 128);
          }
        }

        /* No DirectInput equivalent of an XInput shoulder button is assumed
           (an arbitrary joystick has no fixed button layout to guess one
           from), so this second analog channel only ever moves off-centre
           for an XInput pad -- same as before DirectInput support existed. */
        input.analog[slot + 1][0] = (!is_di_device(map.device) && (buttons & XPAD_LEFT_SHOULDER)) ? 0 : 128;
        input.analog[slot + 1][1] = 128;

        if (input.analog[slot][0] < 0)   input.analog[slot][0] = 0;
        if (input.analog[slot][0] > 255) input.analog[slot][0] = 255;
        if (input.analog[slot][1] < 0)   input.analog[slot][1] = 0;
        if (input.analog[slot][1] > 255) input.analog[slot][1] = 255;
        break;
      }

      case DEVICE_ACTIVATOR:
      {
        if (pad_key_down('G')) input.pad[slot] |= INPUT_ACTIVATOR_7L;
        if (pad_key_down('H')) input.pad[slot] |= INPUT_ACTIVATOR_7U;
        if (pad_key_down('J')) input.pad[slot] |= INPUT_ACTIVATOR_8L;
        if (pad_key_down('K')) input.pad[slot] |= INPUT_ACTIVATOR_8U;
        break;
      }

      case DEVICE_SMASH:
      {
        if (pad_key_down(VK_NUMPAD9)) input.pad[slot] |= INPUT_SMASH_UP_RIGHT;
        if (pad_key_down(VK_NUMPAD8)) input.pad[slot] |= INPUT_SMASH_UP;
        if (pad_key_down(VK_NUMPAD7)) input.pad[slot] |= INPUT_SMASH_UP_LEFT;
        if (pad_key_down(VK_NUMPAD6)) input.pad[slot] |= INPUT_SMASH_RIGHT;
        if (pad_key_down(VK_NUMPAD5)) input.pad[slot] |= INPUT_SMASH_CENTER;
        if (pad_key_down(VK_NUMPAD4)) input.pad[slot] |= INPUT_SMASH_LEFT;
        if (pad_key_down(VK_NUMPAD3)) input.pad[slot] |= INPUT_SMASH_DOWN_RIGHT;
        if (pad_key_down(VK_NUMPAD2)) input.pad[slot] |= INPUT_SMASH_DOWN;
        if (pad_key_down(VK_NUMPAD1)) input.pad[slot] |= INPUT_SMASH_DOWN_LEFT;
        break;
      }

      default:
        input.pad[slot] = read_pad(slot);
        break;
    }
  }

  /* Netplay: swap in the agreed inputs for this frame (no-op otherwise). */
  {
    int p2 = netplay_active() ? gui_input_p2_slot() : -1;
    if (p2 >= 0) netplay_apply_input(&input.pad[0], &input.pad[p2]);
  }

  return 1;
}

/****************************************************************************
 * Host shortcuts polled outside the core
 ****************************************************************************/

/* Independent of xpad_state[]/joy_state[]/poll_gamepads() on purpose -- that
   cache is only refreshed once per forward emulated frame (inside
   win32_input_update()), which the rewind path deliberately never calls.
   A hotkey checked against that cache while rewinding would read
   whatever button state happened to be true the instant rewind started,
   forever, regardless of what's actually being pressed right now. This
   does its own fresh read every single call instead (xpad_query()/di_query()
   already back off a device that isn't there rather than genuinely
   re-querying it every call, so this isn't as wasteful as it looks), the
   same way input_capture_gamepad() already does for the "listening"
   dialog. */
static int gamepad_button_held_now(int device, int mask, int deadzone_raw_val)
{
  if (!mask) return 0;

  if (device >= 0 && device < XPAD_MAX_DEVICES)
  {
    XPAD_STATE st;
    DWORD b;

    if (!xinput_get_state) return 0;
    ZeroMemory(&st, sizeof(st));
    if (!xpad_query(device, &st)) return 0;

    b = st.Gamepad.wButtons;
    if (st.Gamepad.sThumbLY >  deadzone_raw_val) b |= XPAD_DPAD_UP;
    if (st.Gamepad.sThumbLY < -deadzone_raw_val) b |= XPAD_DPAD_DOWN;
    if (st.Gamepad.sThumbLX < -deadzone_raw_val) b |= XPAD_DPAD_LEFT;
    if (st.Gamepad.sThumbLX >  deadzone_raw_val) b |= XPAD_DPAD_RIGHT;
    if (st.Gamepad.bLeftTrigger  > XPAD_TRIGGER_THRESHOLD) b |= XPAD_LEFT_TRIGGER;
    if (st.Gamepad.bRightTrigger > XPAD_TRIGGER_THRESHOLD) b |= XPAD_RIGHT_TRIGGER;

    return (b & (DWORD)mask) != 0;
  }

  if (is_di_device(device))
  {
    int idx = device - XPAD_MAX_DEVICES;
    DIJOYSTATE2 st;
    DWORD b;

    ZeroMemory(&st, sizeof(st));
    if (!di_query(idx, &st)) return 0;

    b = di_raw_buttons(&st);
    if (st.lY >  deadzone_raw_val) b |= DI_DPAD_DOWN;
    if (st.lY < -deadzone_raw_val) b |= DI_DPAD_UP;
    if (st.lX < -deadzone_raw_val) b |= DI_DPAD_LEFT;
    if (st.lX >  deadzone_raw_val) b |= DI_DPAD_RIGHT;

    return (b & (DWORD)mask) != 0;
  }

  return 0;
}

int gui_input_fast_forward(void)
{
  /* Deliberately absent from the accelerator table so it reaches us here
     regardless of which key it's mapped to. */
  if (gui.key_fast_forward && key_down(gui.key_fast_forward)) return 1;
  if (gamepad_button_held_now(gui.pad[0].device, gui.pad_fast_forward, deadzone_raw(0))) return 1;
  return 0;
}

int gui_input_rewind(void)
{
  if (gui.key_rewind && key_down(gui.key_rewind)) return 1;
  if (gamepad_button_held_now(gui.pad[0].device, gui.pad_rewind, deadzone_raw(0))) return 1;
  return 0;
}

int gui_input_frame_advance(void)
{
  int down = key_down(VK_OEM_5);   /* backslash */

  if (down && !frame_advance_armed)
  {
    frame_advance_armed = 1;
    return 1;
  }
  if (!down) frame_advance_armed = 0;

  return 0;
}

static int save_slot_armed[10];
static int load_slot_armed[10];

/* Returns the slot 0-9 whose Shift+digit was just pressed, or -1. */
int gui_input_save_slot_shortcut(void)
{
  int i;

  for (i = 0; i <= 9; i++)
  {
    int down = key_down('0' + i) && (GetAsyncKeyState(VK_SHIFT) & 0x8000);

    if (down && !save_slot_armed[i])
    {
      save_slot_armed[i] = 1;
      load_slot_armed[i] = 1;   /* same physical key is still down -- don't let releasing Shift a moment early re-trigger this as a plain-digit load */
      return i;
    }
    if (!down) save_slot_armed[i] = 0;
  }

  return -1;
}

/* Returns the slot 0-9 whose plain digit was just pressed, or -1.
   Shift held excludes this so the same key press can't register as
   both a save and a load. */
int gui_input_load_slot_shortcut(void)
{
  int i;

  for (i = 0; i <= 9; i++)
  {
    int key_is_down = key_down('0' + i);
    int shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    if (key_is_down && !shift_down && !load_slot_armed[i])
    {
      load_slot_armed[i] = 1;
      return i;
    }
    /* Cleared only once the physical key itself comes up -- not merely
       when Shift changes -- so a Shift release that happens a moment
       before the digit key's own release can't be misread as a fresh,
       separate plain-digit press of the same still-held key. */
    if (!key_is_down) load_slot_armed[i] = 0;
  }

  return -1;
}

/****************************************************************************
 * Capture helpers for the configuration dialog
 *
 * These read the raw key state directly instead of going through key_down():
 * while the dialog is up the main window has lost focus, which is exactly
 * when the emulated pad should go quiet but capture should still work.
 ****************************************************************************/

int input_capture_key(void)
{
  int vk;

  for (vk = 1; vk < 256; vk++)
  {
    switch (vk)
    {
      /* Mouse buttons belong to the pointer devices, Esc cancels. */
      case VK_LBUTTON: case VK_RBUTTON: case VK_MBUTTON:
      case VK_XBUTTON1: case VK_XBUTTON2:
      case VK_ESCAPE:
        continue;
      default:
        break;
    }

    if (GetAsyncKeyState(vk) & 0x8000) return vk;
  }

  return 0;
}

int input_capture_gamepad(int device)
{
  if (device >= 0 && device < XPAD_MAX_DEVICES)
  {
    static const int masks[] =
    {
      XPAD_DPAD_UP, XPAD_DPAD_DOWN, XPAD_DPAD_LEFT, XPAD_DPAD_RIGHT,
      XPAD_START, XPAD_BACK, XPAD_LEFT_SHOULDER, XPAD_RIGHT_SHOULDER,
      XPAD_A, XPAD_B, XPAD_X, XPAD_Y, XPAD_LEFT_TRIGGER, XPAD_RIGHT_TRIGGER
    };

    XPAD_STATE st;
    DWORD b;
    int i;

    if (!xinput_get_state) return 0;

    ZeroMemory(&st, sizeof(st));
    if (!xpad_query(device, &st)) return 0;

    b = st.Gamepad.wButtons;
    if (st.Gamepad.sThumbLY >  XPAD_DEADZONE) b |= XPAD_DPAD_UP;
    if (st.Gamepad.sThumbLY < -XPAD_DEADZONE) b |= XPAD_DPAD_DOWN;
    if (st.Gamepad.sThumbLX < -XPAD_DEADZONE) b |= XPAD_DPAD_LEFT;
    if (st.Gamepad.sThumbLX >  XPAD_DEADZONE) b |= XPAD_DPAD_RIGHT;
    if (st.Gamepad.bLeftTrigger  > XPAD_TRIGGER_THRESHOLD) b |= XPAD_LEFT_TRIGGER;
    if (st.Gamepad.bRightTrigger > XPAD_TRIGGER_THRESHOLD) b |= XPAD_RIGHT_TRIGGER;

    for (i = 0; i < (int)(sizeof(masks) / sizeof(masks[0])); i++)
    {
      if (b & masks[i]) return masks[i];
    }

    return 0;
  }

  if (is_di_device(device))
  {
    int idx = device - XPAD_MAX_DEVICES;
    DIJOYSTATE2 st;
    DWORD b;
    int i;

    ZeroMemory(&st, sizeof(st));
    if (!di_query(idx, &st)) return 0;

    b = di_raw_buttons(&st);
    if (st.lY >  XPAD_DEADZONE) b |= DI_DPAD_DOWN;
    if (st.lY < -XPAD_DEADZONE) b |= DI_DPAD_UP;
    if (st.lX < -XPAD_DEADZONE) b |= DI_DPAD_LEFT;
    if (st.lX >  XPAD_DEADZONE) b |= DI_DPAD_RIGHT;

    if (b & DI_DPAD_UP)    return DI_DPAD_UP;
    if (b & DI_DPAD_DOWN)  return DI_DPAD_DOWN;
    if (b & DI_DPAD_LEFT)  return DI_DPAD_LEFT;
    if (b & DI_DPAD_RIGHT) return DI_DPAD_RIGHT;
    for (i = 0; i < DI_MAX_BUTTONS; i++)
    {
      if (b & DI_BTN(i)) return (int)DI_BTN(i);
    }

    return 0;
  }

  return 0;
}

int input_any_input_down(int device)
{
  if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return 1;
  if (input_capture_key()) return 1;
  if (input_capture_gamepad(device)) return 1;
  return 0;
}

/****************************************************************************
 * Names for the configuration dialog
 ****************************************************************************/

const char *input_key_name(int vk)
{
  static char name[64];
  UINT scan;
  LONG lparam;

  if (!vk) return "(unassigned)";

  scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
  if (!scan) { wsprintfA(name, "Key %d", vk); return name; }

  lparam = (LONG)(scan << 16);

  switch (vk)
  {
    /* Keys on the grey block report the same scan code as the numpad. */
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
    case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
      lparam |= (1 << 24);
      break;
    default:
      break;
  }

  if (GetKeyNameTextA(lparam, name, sizeof(name)) > 0) return name;

  wsprintfA(name, "Key %d", vk);
  return name;
}

/* mask is only meaningful alongside the device it was captured against (see
   the file banner comment) -- the two backends use overlapping numeric bit
   values for entirely different things, so device picks which of the two
   naming tables below applies. */
const char *input_pad_button_name(int device, int mask)
{
  static const struct { int mask; const char *name; } xinput_names[] =
  {
    { XPAD_DPAD_UP,        "D-pad Up"    },
    { XPAD_DPAD_DOWN,      "D-pad Down"  },
    { XPAD_DPAD_LEFT,      "D-pad Left"  },
    { XPAD_DPAD_RIGHT,     "D-pad Right" },
    { XPAD_START,          "Start button"  },
    { XPAD_BACK,           "Back button"   },
    { XPAD_LEFT_SHOULDER,  "LB button"   },
    { XPAD_RIGHT_SHOULDER, "RB button"   },
    { XPAD_A,              "A button"    },
    { XPAD_B,              "B button"    },
    { XPAD_X,              "X button"    },
    { XPAD_Y,              "Y button"    },
    { XPAD_LEFT_TRIGGER,   "LT button"   },
    { XPAD_RIGHT_TRIGGER,  "RT button"   },
  };
  static char name[32];
  int i;

  if (!mask) return "(unassigned)";

  if (is_di_device(device))
  {
    if (mask == (int)DI_DPAD_UP)    return "D-pad/Stick Up";
    if (mask == (int)DI_DPAD_DOWN)  return "D-pad/Stick Down";
    if (mask == (int)DI_DPAD_LEFT)  return "D-pad/Stick Left";
    if (mask == (int)DI_DPAD_RIGHT) return "D-pad/Stick Right";

    for (i = 0; i < DI_MAX_BUTTONS; i++)
    {
      if (mask == (int)DI_BTN(i))
      {
        wsprintfA(name, "Button %d", i + 1);
        return name;
      }
    }

    wsprintfA(name, "pad 0x%06X", mask);
    return name;
  }

  for (i = 0; i < (int)(sizeof(xinput_names) / sizeof(xinput_names[0])); i++)
  {
    if (xinput_names[i].mask == mask) return xinput_names[i].name;
  }

  /* Should not happen for anything this app itself ever assigns, but
     falls back to the raw value rather than showing nothing if it does. */
  wsprintfA(name, "pad 0x%05X", mask);
  return name;
}

const char *input_button_label(int index)
{
  static const char *labels[PAD_KEYS] =
  {
    "Up", "Down", "Left", "Right",
    "A", "B", "C",
    "X", "Y", "Z", "Start", "Mode"
  };

  if (index < 0 || index >= PAD_KEYS) return "?";
  return labels[index];
}
