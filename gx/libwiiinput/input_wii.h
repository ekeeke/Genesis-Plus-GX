#ifndef INPUT_WII_H
#define INPUT_WII_H

#include <gccore.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * Buttons
 *
 * Every controller (Wii Remote, Nunchuk, Classic Controller, GameCube pad,
 * Wii U Pro Controller, Wii U GamePad, DualShock 3, the GameCube controller
 * adapter for Wii U) and the keyboard/mouse
 * are folded into one mask of WPAD_BUTTON_* bits (include <wiiuse/wpad.h>).
 * Two extras exist for the Nunchuk, which has no WPAD_BUTTON_* equivalent.
 * ---------------------------------------------------------------------- */
#define INPUT_BUTTON_Z (1u << 28) /* Nunchuk Z */
#define INPUT_BUTTON_C (1u << 29) /* Nunchuk C */

/* Mouse button bits */
#define INPUT_MOUSE_LEFT   (1 << 0)
#define INPUT_MOUSE_RIGHT  (1 << 1)
#define INPUT_MOUSE_MIDDLE (1 << 2)

/* Analog sticks */
#define INPUT_STICK_LEFT  0
#define INPUT_STICK_RIGHT 1

/* Keyboard LEDs */
typedef enum {
    INPUT_LED_NUM = 0,
    INPUT_LED_CAPS,
    INPUT_LED_SCROLL
} input_led;

/* INPUT_InitEx() flags */
#define INPUT_INIT_NO_POINTER  (1u << 0) /* skip Wii Remote IR (buttons only) */
#define INPUT_INIT_NO_MOUSE    (1u << 1)
#define INPUT_INIT_NO_KEYBOARD (1u << 2)
#define INPUT_INIT_NO_GCADAPTER (1u << 3) /* leave the Wii U GameCube adapter alone */

/* Initialize everything: GameCube pads, Wii Remotes (incl. Nunchuk and
 * Classic Controller), optional Wii U Pro Controller / Wii U GamePad /
 * DualShock 3 / GameCube adapter for Wii U (HAVE_WUPC, HAVE_WIIDRC,
 * HAVE_SICKSAXIS, HAVE_GCADAPTER), USB keyboard and USB mouse. Calls USB_Initialize(). Returns 0 on success, negative on
 * failure. Safe to call twice. */
s32 INPUT_Init(void);
s32 INPUT_InitEx(u32 flags);

/* Stop background work and release devices. */
void INPUT_Shutdown(void);

/* Screen size used for the Wii Remote IR pointer and mouse bounds
 * (default 640x480). */
void INPUT_SetScreenSize(int width, int height);

/* Poll every device once. Call once per frame (or per poll). "Pressed" and
 * "down" report what happened since the previous call. */
void INPUT_Update(void);

/* ---- Controller buttons (WPAD_BUTTON_* | INPUT_BUTTON_*) ---- */
u32 INPUT_PadsHeld(void);     /* controllers only */
u32 INPUT_PadsDown(void);
u32 INPUT_ButtonsHeld(void);  /* controllers + keyboard map + mouse map */
u32 INPUT_ButtonsDown(void);
u32 INPUT_ButtonsUp(void);

/* Name of the device that most recently produced input ("Wiimote",
 * "Nunchuk", "Classic", "GameCube", "Wii U Pro", "GamePad", "DualShock 3",
 * "Keyboard", "Mouse"), or "" if nothing has been used yet. */
const char *INPUT_Source(void);

/* ---- Analog sticks ----
 * x grows right, y grows up, range -1..1 (length clamped to 1, no deadzone
 * applied). The strongest stick across all connected devices wins. Returns
 * the length; *src (optional) receives the device name. */
float INPUT_Stick(int which, float *x, float *y, const char **src);
/* The same stick before its length is clamped to 1 (still divided by the
 * library's per-device range), so an app can apply its own scaling and
 * clamping exactly. Returns the same length as INPUT_Stick(). The ranges:
 * GameCube stick 70 and C-stick 60, Wii U Pro and GamePad 900, DualShock 3
 * 110 from centre; Nunchuk and Classic sticks use wiiuse's magnitude. */
float INPUT_StickRaw(int which, float *x, float *y, const char **src);

/* ---- Wii Remote pointer / motion ---- */
/* IR cursor in screen coordinates; false if no remote is pointing at the
 * screen. */
bool  INPUT_Pointer(float *x, float *y);
/* Largest acceleration magnitude (in g) over all Wii Remotes. */
float INPUT_Gforce(void);

/* ---- Pointer cursor ----
 * One on-screen cursor driven by the Wii Remote IR pointer and/or a USB
 * mouse (and optionally the left stick). Hand-over between devices is
 * smooth: when a remote points at the screen the mouse position follows it.
 * Draw your own cursor sprite at INPUT_CursorPosition() while
 * INPUT_CursorActive() is true. Which devices move it is an option. */
#define INPUT_CURSOR_IR    (1u << 0) /* Wii Remote pointer */
#define INPUT_CURSOR_MOUSE (1u << 1) /* USB mouse */
#define INPUT_CURSOR_STICK (1u << 2) /* left analog stick (off by default) */

/* Which devices drive the cursor. Default IR | MOUSE; 0 disables it. */
void INPUT_SetCursorMode(u32 modes);
u32  INPUT_GetCursorMode(void);
bool INPUT_CursorActive(void);
void INPUT_CursorPosition(int *x, int *y);
/* True when the cursor moved more than a few pixels since the previous
 * call to INPUT_Update() (IR jitter is ignored). */
bool INPUT_CursorMoved(void);

/* ---- Keyboard (USB HID usage codes, e.g. 0x28 = Enter) ---- */
bool INPUT_KeyboardConnected(void);
bool INPUT_KeyHeld(u8 keycode);      /* live state */
bool INPUT_KeyPressed(u8 keycode);   /* since the last INPUT_Update() */
bool INPUT_KeyReleased(u8 keycode);
s32  INPUT_KeyboardSetLed(input_led led, bool on);

#define INPUT_MOD_SHIFT (1 << 0)
#define INPUT_MOD_CTRL  (1 << 1)
#define INPUT_MOD_ALT   (1 << 2)
#define INPUT_MOD_GUI   (1 << 3)
u8 INPUT_KeyModifiers(void);         /* live INPUT_MOD_* bits */

typedef struct {
    u8   code;   /* USB HID usage code */
    char ch;     /* printable ASCII for the press (shift/caps aware), else 0 */
    u8   mods;   /* INPUT_MOD_* held at the time of the press */
} input_key_event;

/* Pop the next key press (modifier keys alone are not reported). Returns
 * false when the queue is empty. Holding a key does not repeat. */
bool INPUT_KeyPop(input_key_event *ev);

/* Keyboard-as-controller mapping. The default follows the Priiloader
 * layout: arrows = D-pad, Enter = A, Backspace = B, X = 1, Y = 2,
 * Delete = -, Esc/Home = HOME. */
void INPUT_KeyboardMapClear(void);
void INPUT_KeyboardMapSet(u8 keycode, u32 button);
u32  INPUT_KeyboardPadHeld(void);
u32  INPUT_KeyboardPadDown(void);

/* GameCube-pad-as-controller mapping. The default: A, B, D-pad as named,
 * X = 2, Y = 1, R / Z = +, L = -, Start = HOME. MapSet assigns every PAD_*
 * bit in pad_buttons; a button of 0 unmaps them. */
void INPUT_GameCubeMapClear(void);
void INPUT_GameCubeMapSet(u16 pad_buttons, u32 button);

/* Leave some of a Wii Remote's own buttons (WPAD_BUTTON_*) on one channel
 * (0-3) out of the button masks, for when another layer already handles
 * them, such as SDL-wii turning channel 0's A and B into mouse buttons.
 * Expansion controllers on that channel are unaffected. 0 restores them. */
void INPUT_IgnoreWiimoteButtons(int chan, u32 buttons);

/* ---- GameCube controller adapter for Wii U (HAVE_GCADAPTER) ----
 * Nintendo's WUP-028 and the third-party adapters that copy it, in either
 * mode (Wii U / PC). It is looked for every two seconds, so it can be
 * plugged in at any time; its four ports go into the button masks and
 * sticks like the console's own GameCube ports (through the same
 * INPUT_GameCubeMap, source "GameCube"). Sticks centre on where they rest
 * when a controller is plugged in; X+Y+Start held for three seconds takes
 * the centre again, as on the console. */
typedef struct {
    u16  buttons;                  /* PAD_BUTTON_* / PAD_TRIGGER_* */
    s8   stick_x, stick_y;         /* from the centre, y up, about -100..100 */
    s8   substick_x, substick_y;
    u8   trigger_l, trigger_r;     /* 0..255 */
    bool wireless;                 /* a WaveBird receiver: no rumble */
} input_gc_pad;

/* True while the adapter is plugged in and reporting. */
bool INPUT_GCAdapterConnected(void);
/* Port 0-3 as of the last INPUT_Update(); false when nothing is plugged
 * into it. */
bool INPUT_GCAdapterPad(int port, input_gc_pad *out);
/* Rumble needs the adapter's power (grey) cable plugged in. */
void INPUT_GCAdapterRumble(int port, bool on);
/* Where the adapter's steps and errors go, e.g. INPUT_GCAdapterSetLog(printf). */
void INPUT_GCAdapterSetLog(int (*print)(const char *fmt, ...));

/* ---- Mouse ---- */
bool INPUT_MouseConnected(void);
/* Cursor position, clamped to the screen size. */
void INPUT_MousePosition(int *x, int *y);
void INPUT_MouseSetPosition(int x, int y);
/* Multiply movement when updating the position (default 1). The delta
 * reported by INPUT_MouseDelta() stays raw. */
void INPUT_MouseSetSpeed(int multiplier);
/* Movement and wheel (raw, positive = wheel toward the user) accumulated
 * since the last INPUT_Update(). */
void INPUT_MouseDelta(int *dx, int *dy, int *wheel);
u8   INPUT_MouseButtonsHeld(void);
u8   INPUT_MouseButtonsPressed(void);
u8   INPUT_MouseButtonsReleased(void);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_WII_H */
