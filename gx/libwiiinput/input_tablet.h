/* USB pen tablet driver for libogc homebrew.
 *
 * Works from the tablet's own HID report descriptor, so any tablet that
 * describes its pen there is covered: Wacom's current Intuos, One and Intuos
 * Pro lines (whose vendor usage page mirrors the standard digitizer page),
 * and the many Huion, XP-Pen and Gaomon tablets that use the standard page
 * directly.  Older Wacom Bamboo / Intuos (CTL-480 era) tablets, which send a
 * fixed undescribed report, are switched into tablet mode and read in that
 * format instead.
 *
 * A background thread looks for a tablet, reads it, and picks it up again
 * after it is unplugged and plugged back in.
 *
 * The uDraw GameTablet, plugged into a Wii Remote, comes through the same
 * calls: its samples carry the Wii Remote's number in tb_sample.wiimote.
 *
 * An app built with HAVE_TABLET links with three wrappers:
 *     -Wl,--wrap=classic_ctrl_handshake,--wrap=parse_event,--wrap=USB_GetDeviceList
 * The first two keep libogc's wiiuse from taking a uDraw for a Classic
 * Controller.  The third hides tablets from libogc's USB mouse and keyboard
 * drivers, so a USB mouse and a tablet work side by side (a tablet also
 * presents a boot mouse, which libogc's driver would otherwise open instead
 * of the real mouse, stealing the pen's reports).  Without them the link
 * fails on __real_parse_event or __real_USB_GetDeviceList.
 *
 * Built only with HAVE_TABLET (make WITH_TABLET=1).
 */

#ifndef INPUT_TABLET_H
#define INPUT_TABLET_H

#include <gctypes.h>

#define TB_OK       0
#define TB_ENODEV  -1
#define TB_ESTATE  -3
#define TB_ENOMEM  -5

/* tb_sample.buttons */
#define TB_BUTTON_TIP     0x01 /* the nib (or eraser) touching the surface */
#define TB_BUTTON_BARREL  0x02 /* lower side switch */
#define TB_BUTTON_BARREL2 0x04 /* upper side switch */

typedef struct {
	u16 vid, pid;
	char name[48];        /* product string, or "VID:PID" */
	u8 interface;
	u8 endpoint;          /* interrupt IN endpoint, and its packet size */
	u16 max_packet;
	u8 report_id;         /* the pen's report */
	s32 max_x, max_y;     /* coordinate range, from 0 */
	u32 max_pressure;
	bool has_tilt;
	bool has_eraser;
	u8 pad_buttons;       /* buttons on the tablet itself */
	bool legacy;          /* old fixed-format Wacom protocol */
	bool has_touch;       /* a Bamboo Pen & Touch's touch surface */
	s32 touch_max_x, touch_max_y;
} tb_info;

typedef struct {
	s32 x, y;             /* tablet units, 0..max_x / 0..max_y */
	float nx, ny;         /* the same as 0..1 */
	float pressure;       /* 0..1 */
	u32 raw_pressure;
	s8 tilt_x, tilt_y;    /* degrees, when has_tilt */
	u8 buttons;           /* TB_BUTTON_* */
	bool in_range;        /* the pen is near enough to be tracked */
	bool eraser;          /* the pen is upside down */
	u16 pad;              /* tablet buttons held, bit n = button n */
	u32 time;             /* when the report arrived, in milliseconds */
	/* From the touch surface rather than the pen: in_range and
	 * TB_BUTTON_TIP say a finger is down, x/y are the first finger's in
	 * the pen's units, and pressure is a fixed 0.5.  Fingers are ignored
	 * while the pen is in range. */
	bool finger;
	/* The Wii Remote (0-3) whose uDraw sent this, or -1 for the USB tablet.
	 * On a uDraw, TB_BUTTON_BARREL is the lower rocker and TB_BUTTON_BARREL2
	 * the upper one, and in_range is the pen over the drawing area (hovering
	 * or touching). */
	s8 wiimote;
} tb_sample;

#define TB_MAX_FINGERS 4

typedef struct {
	bool down;
	s32 x, y;             /* touch units, 0..touch_max_x / touch_max_y */
	float nx, ny;         /* 0..1 */
} tb_finger;

/* Where the device scan and errors go, e.g. TB_SetLog(printf). */
void TB_SetLog(int (*print)(const char *fmt, ...));

/* Never open this device while looking for a tablet (call before TB_Init), for
 * controllers that another driver already owns. Up to 16 devices. */
void TB_IgnoreDevice(u16 vid, u16 pid);

/* Starts the thread that finds and reads the tablet.  Calls USB_Initialize
 * itself.  Not finding a tablet is not an error: it is picked up whenever one
 * is plugged in.  A uDraw needs WPAD_Init, not this, but is read through the
 * calls below either way. */
s32 TB_Init(void);
void TB_Shutdown(void);

/* A USB tablet is open, or a uDraw is reporting. */
bool TB_Connected(void);
/* The USB tablet's, or else a uDraw's (name "uDraw GameTablet", no USB
 * fields). */
bool TB_GetInfo(tb_info *info);

/* The pen as last reported, by whichever tablet reported last. */
void TB_GetState(tb_sample *out);

/* Every report since the last call, oldest first; false when there are none
 * left.  Tablets report at 130-200 Hz (a uDraw at the Wii Remote's 100), so
 * drawing from these instead of TB_GetState keeps fast strokes smooth.  The
 * queue holds 256 (64 for each uDraw). */
bool TB_Pop(tb_sample *out);

/* Maps a sample onto a w x h area, keeping the tablet's proportions: the
 * active area is centred, and a pen outside it gives an edge position. */
void TB_ToScreen(const tb_sample *s, int w, int h, int *x, int *y);

/* A uDraw on this Wii Remote (0-3) has reported in the last quarter second. */
bool TB_UDrawConnected(int wiimote);
/* The uDraw's drawing area in its raw units; tablets differ a little.  The
 * default, 0x56-0x780 by 0x65-0x5a5, is the printed area's edge. */
void TB_UDrawSetArea(s32 min_x, s32 max_x, s32 min_y, s32 max_y);

/* The fingers on a touch surface now, up to `max`; returns how many. */
u32 TB_GetFingers(tb_finger *out, u32 max);

/* ---- debugging ---------------------------------------------------------- */

typedef struct {
	u32 reports;     /* every interrupt transfer that came back */
	u32 pen_reports;
	u32 touch_reports;
	u32 other;       /* reports that were neither pen nor pad */
	u32 errors;
	s32 last_ret;
	u8 last_id;
	u8 last[64];     /* the last report, as received */
	u8 last_len;
	/* Kept from the latest open, and across reopens for the errors. */
	s32 proto_ret;   /* SET_PROTOCOL */
	s32 mode_set;    /* switching to tablet mode, where one is needed */
	s32 mode_get;
	u8 mode_val;     /* the mode read back */
	s32 first_err[4];
	u8 nfirst;
	u32 opens;
	u32 false_removals; /* removal hooks fired with the tablet still there */
} tb_stats;

void TB_GetStats(tb_stats *out);

#endif /* INPUT_TABLET_H */
