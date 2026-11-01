/* USB pen tablet driver for libogc homebrew.  See input_tablet.h. */

#ifdef HAVE_TABLET

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gccore.h>
#include <ogc/cache.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/mutex.h>
#include <ogc/usb.h>

#include "input_tablet.h"
#include "libinput_hidparse.h"
#include "libinput_internal.h"

#define WACOM_VID     0x056A

#define HID_SET_REPORT 0x09
#define HID_GET_REPORT 0x01
#define HID_FEATURE    0x03

#define REQTYPE_STD_IF_IN    0x81
#define REQTYPE_CLASS_IF_OUT 0x21
#define REQTYPE_CLASS_IF_IN  0xA1

#define MAX_DEVICES 32
#define MAX_IFACES  4
#define CFG_MAX     512
#define RDESC_MAX   2048
#define INTR_MAX    64
#define STACKSIZE   16384
#define QUEUE       256
#define QUEUE_MASK  (QUEUE - 1)

/* Old Wacom protocol (Bamboo, CTL-480 Intuos): report 2, switched on by
 * writing 2 to feature report 2.  Byte 1 holds the flags, then X, Y and
 * pressure as little-endian 16-bit values. */
#define LEGACY_REPORT 2

/* On USBV5 the handle from USB_OpenDevice is a device_id that may read as
 * negative, so the fd's sign says nothing about whether we are open. */
static s32 tb_fd = -1;
static bool tb_opened;
static bool tb_oh0; /* opened through /dev/usb/oh0 rather than USBV5 */
static int tb_last_count = -1;
static s32 tb_pen_id; /* the pen interface's USBV5 device_id */
static int tb_logged_count = -1;
static volatile bool tb_unplugged;
static volatile bool tb_running;
static volatile bool tb_exited;  /* the worker has returned */

/* Devices sorted by the scan: tablets (hidden from libogc's mouse and
 * keyboard drivers, see __wrap_USB_GetDeviceList) and devices that are
 * certainly not tablets (left alone after the first look, so the scan that
 * repeats every second doesn't keep opening the mouse and keyboard). */
#define SEEN_MAX 16
typedef struct { u16 vid, pid; } tb_id;
static tb_id tb_tablets[SEEN_MAX], tb_others[SEEN_MAX];
static int tb_ntablets, tb_nothers;
static bool tb_probe_unsure; /* a descriptor couldn't be read or parsed */

static tb_info tb_dev;
static hidp_desc tb_desc;
static hidp_pen tb_pen;
static u8 tb_ep;
static u16 tb_max_packet;

static tb_sample tb_state;
static tb_sample tb_queue[QUEUE];
static volatile u32 tb_head, tb_tail;
static mutex_t tb_lock = LWP_MUTEX_NULL;

static tb_stats tb_st;
static lwp_t tb_thread = LWP_THREAD_NULL;
static u8 tb_stack[STACKSIZE] ATTRIBUTE_ALIGN(32);

/* DMA buffers.  Static: a transfer that never completes may still land in
 * them after the device is gone. */
static u8 tb_ctl[RDESC_MAX] ATTRIBUTE_ALIGN(32);
static u8 tb_intr[2][INTR_MAX] ATTRIBUTE_ALIGN(32);

/* The touch surface of a Bamboo Pen & Touch: a second interface, sending
 * Wacom's fixed touch packets and the tablet's keys. */
static u8 tb_touch_if, tb_touch_ep;
static u16 tb_touch_pkt;
static tb_finger tb_fingers[TB_MAX_FINGERS];

static int (*tb_logger)(const char *fmt, ...);
#define tb_log(...) (tb_logger ? (void)tb_logger(__VA_ARGS__) : (void)0)

void TB_SetLog(int (*print)(const char *fmt, ...)) { tb_logger = print; }

typedef struct {
	volatile bool done;
	volatile bool pending;
	volatile s32 res;
} tb_xfer;

static tb_xfer tb_ctl_x;

/* An interrupt IN pipe: the pen's, and the touch surface's where there is
 * one.  Under oh0 both go through one handle, naming their endpoints; under
 * USBV5 each interface is a device of its own, with its own handle. */
typedef struct {
	s32 fd;
	u8 ep;
	bool on;
	bool own_fd;   /* opened apart from the pen's, so closed apart */
	int fails;
	tb_xfer x;
	u8 *buf;
} tb_pipe;

#define PEN   0
#define TOUCH 1
static tb_pipe tb_pipes[2];

static s32 tb_xfer_cb(s32 result, void *arg)
{
	tb_xfer *x = arg;

	x->res = result;
	x->done = true;
	return 0;
}

static s32 tb_removed_cb(s32 result, void *arg)
{
	(void)result;
	(void)arg;
	tb_unplugged = true;
	return 0;
}

/* Waits for a transfer, giving up after `ms` or as soon as the device goes. */
static bool tb_wait(tb_xfer *x, int ms)
{
	int waited;

	for (waited = 0; !x->done; waited++) {
		if (tb_unplugged || (ms >= 0 && waited >= ms))
			return false;
		usleep(1000);
	}
	x->pending = false;
	return true;
}

/* A control transfer with a deadline, as libogc's synchronous calls have
 * none.  `data` is staged through tb_ctl. */
static s32 tb_ctrl(u8 type, u8 req, u16 val, u16 idx, u16 len, void *data, int ms)
{
	s32 ret;

	if (len > sizeof(tb_ctl) || tb_ctl_x.pending)
		return TB_ESTATE;

	if (!(type & 0x80) && len)
		memcpy(tb_ctl, data, len);
	else
		memset(tb_ctl, 0, len ? len : 1);
	DCFlushRange(tb_ctl, (len + 31) & ~31u);

	tb_ctl_x.done = false;
	tb_ctl_x.res = -1;
	tb_ctl_x.pending = true;
	if (type & 0x80)
		ret = USB_ReadCtrlMsgAsync(tb_fd, type, req, val, idx, len, tb_ctl,
		                           tb_xfer_cb, &tb_ctl_x);
	else
		ret = USB_WriteCtrlMsgAsync(tb_fd, type, req, val, idx, len,
		                            len ? tb_ctl : NULL, tb_xfer_cb, &tb_ctl_x);
	if (ret < 0) {
		tb_ctl_x.pending = false;
		return ret;
	}
	if (!tb_wait(&tb_ctl_x, ms))
		return -6; /* stays pending: tb_ctl outlives it */

	if ((type & 0x80) && len) {
		DCInvalidateRange(tb_ctl, (len + 31) & ~31u);
		memcpy(data, tb_ctl, len);
	}
	return tb_ctl_x.res;
}

static u16 le16(const u8 *p)
{
	return (u16)(p[0] | (p[1] << 8));
}

/* ---- finding the tablet -------------------------------------------------- */

typedef struct {
	u8 number;
	u8 subclass, protocol;
	u16 rdesc_len;
	u8 ep;
	u16 max_packet;
} tb_iface;

/* Every HID interface in a configuration descriptor, with its report
 * descriptor length and interrupt IN endpoint. */
static int list_hid_ifaces(const u8 *cfg, u32 total, tb_iface *out, int max)
{
	int n = 0;
	bool in_hid = false;
	u32 off = 0;

	while (off + 2 <= total) {
		const u8 *d = cfg + off;
		u8 len = d[0], type = d[1];

		if (len < 2 || off + len > total)
			break;

		if (type == USB_DT_INTERFACE && len >= 9) {
			in_hid = d[5] == USB_CLASS_HID && d[3] == 0 && n < max;
			if (in_hid) {
				memset(&out[n], 0, sizeof(out[n]));
				out[n].number = d[2];
				out[n].subclass = d[6];
				out[n].protocol = d[7];
				n++;
			}
		} else if (in_hid && type == USB_DT_HID && len >= 9) {
			/* Report descriptors are listed from byte 6 on. */
			int i;

			for (i = 6; i + 3 <= len; i += 3)
				if (d[i] == USB_DT_REPORT) {
					out[n - 1].rdesc_len = le16(d + i + 1);
					break;
				}
		} else if (in_hid && type == USB_DT_ENDPOINT && len >= 7) {
			if ((d[2] & 0x80) && (d[3] & 0x03) == 0x03 && !out[n - 1].ep) {
				out[n - 1].ep = d[2];
				out[n - 1].max_packet = le16(d + 4) & 0x7FF;
			}
		}
		off += len;
	}
	return n;
}

static void read_name(u8 index, char *out, u32 size)
{
	u8 buf[64];
	s32 ret;
	u32 i, n = 0;

	snprintf(out, size, "%04x:%04x", tb_dev.vid, tb_dev.pid);
	if (index == 0)
		return;

	ret = tb_ctrl(0x80, USB_REQ_GETDESCRIPTOR, (USB_DT_STRING << 8) | index,
	              0x0409, sizeof(buf), buf, 300);
	if (ret < 4 || buf[1] != USB_DT_STRING) {
		tb_log("  name string %u: %d\n", index, (int)ret);
		return;
	}
	if (buf[0] < (u32)ret)
		ret = buf[0];
	for (i = 2; i + 1 < (u32)ret && n + 1 < size; i += 2) {
		u16 c = le16(buf + i);

		if (c == 0)
			break;
		out[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
	}
	while (n > 0 && out[n - 1] == ' ')
		n--;
	if (n > 0)
		out[n] = '\0';
}

static s32 set_feature(u8 iface, const u8 *report, u16 len)
{
	return tb_ctrl(REQTYPE_CLASS_IF_OUT, HID_SET_REPORT,
	               (u16)((HID_FEATURE << 8) | report[0]), iface, len,
	               (void *)report, 300);
}

/* Takes the tablet out of the mouse mode it powers up in, where there is
 * one.  Descriptor-described tablets say how with an Input Mode feature;
 * old Wacoms take a 2 in feature report 2. */
static void switch_mode(const tb_iface *ifc)
{
	u8 rep[64];
	int tries;

	if (tb_dev.legacy) {
		for (tries = 0; tries < 5; tries++) {
			s32 ret;

			rep[0] = LEGACY_REPORT;
			rep[1] = 2;
			ret = set_feature(ifc->number, rep, 2);
			tb_st.mode_set = ret;
			tb_st.mode_get = 0;
			if (ret >= 0) {
				memset(rep, 0, 2);
				ret = tb_ctrl(REQTYPE_CLASS_IF_IN, HID_GET_REPORT,
				              (HID_FEATURE << 8) | LEGACY_REPORT,
				              ifc->number, 2, rep, 300);
				tb_st.mode_get = ret;
				tb_st.mode_val = rep[1];
			}
			tb_log("  tablet mode: set %d get %d (%02x %02x)\n",
			       (int)tb_st.mode_set, (int)tb_st.mode_get, rep[0], rep[1]);
			if (ret >= 2 && rep[1] == 2)
				break;
		}
		return;
	}

	if (tb_pen.input_mode) {
		const hidp_field *f = tb_pen.input_mode;
		u32 len = hidp_report_bytes(&tb_desc, HIDP_FEATURE, f->report_id);
		u32 skip = tb_desc.uses_ids ? 1 : 0;

		if (len > sizeof(rep))
			return;
		memset(rep, 0, sizeof(rep));
		rep[0] = f->report_id;
		hidp_put(f, rep + skip, len - skip, 2);
		tb_log("  input mode (report %u): %d\n", f->report_id,
		       (int)set_feature(ifc->number, rep, (u16)len));
	}
}

/* Reads one HID interface's report descriptor and decides whether it is the
 * tablet's pen interface.  Fills tb_desc and tb_pen when it is. */
static bool probe_iface(const tb_iface *ifc)
{
	static u8 rdesc[RDESC_MAX];
	u16 len = ifc->rdesc_len;
	s32 ret;

	if (len == 0 || len > RDESC_MAX)
		len = RDESC_MAX;

	ret = tb_ctrl(REQTYPE_STD_IF_IN, USB_REQ_GETDESCRIPTOR, USB_DT_REPORT << 8,
	              ifc->number, len, rdesc, 500);
	if (ret <= 0) {
		tb_log("  if %u: no report descriptor (%d)\n", ifc->number, (int)ret);
		tb_probe_unsure = true;
		return false;
	}
	if (ret > len)
		ret = len;
	if (!hidp_parse(rdesc, (u32)ret, &tb_desc)) {
		tb_log("  if %u: report descriptor unreadable (%d bytes)\n",
		       ifc->number, (int)ret);
		tb_probe_unsure = true;
		return false;
	}
	if (hidp_find_pen(&tb_desc, &tb_pen)) {
		tb_log("  if %u: pen in report %u, %d fields\n", ifc->number,
		       tb_pen.report_id, tb_desc.nfields);
		return true;
	}
	tb_log("  if %u: %d fields, no pen\n", ifc->number, tb_desc.nfields);
	return false;
}

/* A Bamboo's touch interface.  Its descriptor gives the finger's range,
 * else Linux's defaults for the two packet formats stand in: 20-byte
 * packets on first-generation Pen & Touch tablets, 64-byte on later ones. */
static void find_touch(const tb_iface *ifc)
{
	static hidp_desc d;
	static u8 rdesc[RDESC_MAX];
	u16 len = ifc->rdesc_len && ifc->rdesc_len <= RDESC_MAX ? ifc->rdesc_len : RDESC_MAX;
	s32 ret;
	int i;

	tb_touch_if = ifc->number;
	tb_touch_ep = ifc->ep;
	tb_touch_pkt = ifc->max_packet;
	tb_dev.has_touch = true;
	tb_dev.touch_max_x = tb_touch_pkt >= 64 ? 4096 : 480;
	tb_dev.touch_max_y = tb_dev.touch_max_x;

	ret = tb_ctrl(REQTYPE_STD_IF_IN, USB_REQ_GETDESCRIPTOR, USB_DT_REPORT << 8,
	              ifc->number, len, rdesc, 500);
	if (ret > 0 && hidp_parse(rdesc, (u32)(ret > len ? len : ret), &d))
		for (i = 0; i < d.nfields; i++) {
			const hidp_field *f = &d.fields[i];

			if (f->type != HIDP_INPUT || f->app == HIDP_GD_MOUSE ||
			    f->logical_max <= 0)
				continue;
			if (f->usage == HIDP_GD_X)
				tb_dev.touch_max_x = f->logical_max;
			else if (f->usage == HIDP_GD_Y)
				tb_dev.touch_max_y = f->logical_max;
		}
	memset(tb_fingers, 0, sizeof(tb_fingers));
	(void)tb_ctrl(REQTYPE_CLASS_IF_OUT, USB_REQ_SETPROTOCOL, 1, ifc->number,
	              0, NULL, 300);
	tb_log("  touch: if %u ep %02x pkt %u, %dx%d (descriptor %d)\n", ifc->number,
	       tb_touch_ep, tb_touch_pkt, (int)tb_dev.touch_max_x,
	       (int)tb_dev.touch_max_y, (int)ret);
}

/* With a device open, decides whether it is a tablet and sets it up. */
static bool probe_device(u16 vid, u16 pid)
{
	static u8 cfg[CFG_MAX];
	tb_iface ifaces[MAX_IFACES];
	u8 dev[18];
	u16 total;
	s32 ret;
	int n, i, pick = -1;

	tb_probe_unsure = true; /* until the descriptors are in */
	ret = tb_ctrl(0x80, USB_REQ_GETDESCRIPTOR, USB_DT_DEVICE << 8, 0,
	              sizeof(dev), dev, 300);
	if (ret < 18) {
		tb_log("  device descriptor: %d\n", (int)ret);
		return false;
	}
	ret = tb_ctrl(0x80, USB_REQ_GETDESCRIPTOR, USB_DT_CONFIG << 8, 0, 9, cfg, 300);
	if (ret < 9)
		return false;
	total = le16(cfg + 2);
	if (total > CFG_MAX)
		total = CFG_MAX;
	ret = tb_ctrl(0x80, USB_REQ_GETDESCRIPTOR, USB_DT_CONFIG << 8, 0, total,
	              cfg, 500);
	if (ret < 9)
		return false;
	if ((u32)ret < total)
		total = (u16)ret;

	tb_probe_unsure = false;
	n = list_hid_ifaces(cfg, total, ifaces, MAX_IFACES);
	for (i = 0; i < n; i++)
		tb_log("  if %u: class 3/%u/%u ep %02x pkt %u rdesc %u\n",
		       ifaces[i].number, ifaces[i].subclass, ifaces[i].protocol,
		       ifaces[i].ep, ifaces[i].max_packet, ifaces[i].rdesc_len);
	if (n == 0)
		return false;

	memset(&tb_dev, 0, sizeof(tb_dev));
	tb_dev.vid = vid;
	tb_dev.pid = pid;

	for (i = 0; i < n && pick < 0; i++)
		if (ifaces[i].ep && probe_iface(&ifaces[i]))
			pick = i;

	/* A Wacom that describes no pen is one of the old fixed-format ones. */
	if (pick < 0 && vid == WACOM_VID && ifaces[0].ep) {
		pick = 0;
		memset(&tb_desc, 0, sizeof(tb_desc));
		memset(&tb_pen, 0, sizeof(tb_pen));
		tb_dev.legacy = true;
		tb_log("  no pen described: old Wacom protocol\n");
	}
	if (pick < 0)
		return false;

	tb_dev.interface = ifaces[pick].number;
	tb_ep = ifaces[pick].ep;
	tb_max_packet = ifaces[pick].max_packet;
	tb_dev.endpoint = tb_ep;
	tb_dev.max_packet = tb_max_packet;

	if (tb_dev.legacy) {
		/* Intuos S/M ranges; grown if the pen goes further. */
		tb_dev.report_id = LEGACY_REPORT;
		tb_dev.max_x = 15200;
		tb_dev.max_y = 9500;
		tb_dev.max_pressure = 1023;
	} else {
		tb_dev.report_id = tb_pen.report_id;
		tb_dev.max_x = tb_pen.x->logical_max;
		tb_dev.max_y = tb_pen.y->logical_max;
		tb_dev.max_pressure = tb_pen.pressure ? (u32)tb_pen.pressure->logical_max : 1;
		tb_dev.has_tilt = tb_pen.tilt_x && tb_pen.tilt_y;
		tb_dev.has_eraser = tb_pen.eraser || tb_pen.invert;
		tb_dev.pad_buttons = (u8)tb_pen.npad;
	}
	tb_dev.has_touch = false;
	if (tb_dev.legacy)
		for (i = 0; i < n; i++)
			if (i != pick && ifaces[i].ep) {
				find_touch(&ifaces[i]);
				break;
			}
	if (tb_dev.max_x <= 0) tb_dev.max_x = 1;
	if (tb_dev.max_y <= 0) tb_dev.max_y = 1;
	if (tb_dev.max_pressure == 0) tb_dev.max_pressure = 1;
	read_name(dev[15], tb_dev.name, sizeof(tb_dev.name));

	if (ifaces[pick].subclass == 1 && ifaces[pick].protocol == 2)
		tb_log("  (boot mouse interface: libogc's mouse driver may claim it"
		       " too -- INPUT_InitEx(INPUT_INIT_NO_MOUSE) avoids that)\n");

	/* Report protocol, in case a mouse driver left it in boot protocol. */
	tb_st.opens++;
	tb_st.proto_ret = tb_ctrl(REQTYPE_CLASS_IF_OUT, USB_REQ_SETPROTOCOL, 1,
	              tb_dev.interface, 0, NULL, 300);
	tb_log("  set protocol: %d\n", (int)tb_st.proto_ret);
	switch_mode(&ifaces[pick]);

	tb_log("  %s: if %u (%u/%u) ep %02x pkt %u, %dx%d, pressure %u%s%s, %u buttons\n",
	       tb_dev.name, tb_dev.interface, ifaces[pick].subclass,
	       ifaces[pick].protocol, tb_ep, tb_max_packet, (int)tb_dev.max_x,
	       (int)tb_dev.max_y, (unsigned)tb_dev.max_pressure,
	       tb_dev.has_tilt ? ", tilt" : "", tb_dev.has_eraser ? ", eraser" : "",
	       tb_dev.pad_buttons);
	return true;
}

/* The interface a USBV5 list entry stands for, kept in its token. */
static u8 entry_iface(const usb_device_entry *e)
{
	return (u8)(e->token >> 8);
}

/* The list entry standing for one interface of the device listed at
 * list[at].  USBV5 lists a device once per interface, the interface number
 * in its token, and in no fixed order: the CTH-460 comes up touch first.
 * -1 when the interface has no entry of its own. */
static int find_entry(const usb_device_entry *list, int count, int at, u8 iface)
{
	int i;

	for (i = 0; i < count; i++)
		if (list[i].vid == list[at].vid && list[i].pid == list[at].pid &&
		    entry_iface(&list[i]) == iface)
			return i;
	return -1;
}

/* Opens the pipe to the touch interface, unless it is open already.  Under
 * oh0 it shares the pen's handle; under USBV5 it is its own entry. */
static void open_touch(const usb_device_entry *list, int count, int at)
{
	int pick;

	if (tb_pipes[TOUCH].on)
		return;
	if (tb_oh0) {
		tb_pipes[TOUCH].fd = tb_fd;
		tb_pipes[TOUCH].ep = tb_touch_ep;
		tb_pipes[TOUCH].on = true;
		return;
	}

	pick = find_entry(list, count, at, tb_touch_if);
	if (pick < 0) {
		tb_log("  touch: no list entry for if %u\n", tb_touch_if);
		return;
	}
	/* USBV5 reads an HID device's descriptors as it opens it, with no
	 * deadline; said first, so a stall here shows in the log. */
	tb_log("  touch: opening id %08x\n", (unsigned)list[pick].device_id);
	if (USB_OpenDevice(list[pick].device_id, list[at].vid, list[at].pid,
	                   &tb_pipes[TOUCH].fd) < 0) {
		tb_log("  touch: open failed\n");
		return;
	}
	tb_pipes[TOUCH].ep = tb_touch_ep;
	tb_pipes[TOUCH].on = true;
	tb_pipes[TOUCH].own_fd = true;
}

/* Under USBV5 an interrupt read goes to the interface its handle stands for,
 * whatever endpoint it names, so the pen has to be read through the pen
 * interface's own entry.  The device was probed through whichever entry came
 * first; if that is the touch interface's, it becomes the touch pipe. */
static void place_pipes(const usb_device_entry *list, int count, int at)
{
	int opened_if = tb_oh0 ? -1 : entry_iface(&list[at]);
	int pe;

	memset(tb_pipes, 0, sizeof(tb_pipes));
	tb_pen_id = list[at].device_id;

	if (!tb_oh0 && opened_if != tb_dev.interface &&
	    (pe = find_entry(list, count, at, tb_dev.interface)) >= 0) {
		s32 opened = tb_fd, pen_fd;

		tb_log("  pen: opening id %08x (if %u)\n", (unsigned)list[pe].device_id,
		       tb_dev.interface);
		if (USB_OpenDevice(list[pe].device_id, list[at].vid, list[at].pid,
		                   &pen_fd) >= 0) {
			if (tb_dev.has_touch && opened_if == tb_touch_if) {
				tb_pipes[TOUCH].fd = opened;
				tb_pipes[TOUCH].ep = tb_touch_ep;
				tb_pipes[TOUCH].on = true;
				tb_pipes[TOUCH].own_fd = true;
			} else
				USB_CloseDevice(&opened);
			tb_fd = pen_fd;
			tb_pen_id = list[pe].device_id;
		} else
			tb_log("  pen: open failed, reading through id %08x\n",
			       (unsigned)list[at].device_id);
	}

	tb_pipes[PEN].fd = tb_fd;
	tb_pipes[PEN].ep = tb_ep;
	tb_pipes[PEN].on = true;
	if (tb_dev.has_touch)
		open_touch(list, count, at);
}

/* ---- sharing the USB bus with libogc's mouse and keyboard ----------------- */

static bool listed(const tb_id *ids, int n, u16 vid, u16 pid)
{
	int i;

	for (i = 0; i < n; i++)
		if (ids[i].vid == vid && ids[i].pid == pid)
			return true;
	return false;
}

static void remember(tb_id *ids, int *n, u16 vid, u16 pid)
{
	if (*n < SEEN_MAX && !listed(ids, *n, vid, pid)) {
		ids[*n].vid = vid;
		ids[*n].pid = pid;
		(*n)++;
	}
}

/* Tells the scan that a device is not a tablet, so it never opens it: for devices
 * another driver in the app already owns. */
void TB_IgnoreDevice(u16 vid, u16 pid)
{
	remember(tb_others, &tb_nothers, vid, pid);
}

/* Pen tablet makers, for the tablets the scan hasn't looked at yet: Wacom,
 * Huion and Gaomon, XP-Pen and UGEE, Veikk, UC-Logic, Waltop. */
static bool tablet_vendor(u16 vid)
{
	return vid == WACOM_VID || vid == 0x256C || vid == 0x28BD || vid == 0x2FEB ||
	       vid == 0x5543 || vid == 0x172F;
}

s32 __real_USB_GetDeviceList(usb_device_entry *list, u8 max, u8 cls, u8 *count);

/* Most tablets also present a boot-protocol mouse, and libogc's mouse driver
 * opens the first one it finds: with a tablet plugged in it could take the
 * tablet instead of the mouse, and steal the pen's reports.  Linked with
 * -Wl,--wrap=USB_GetDeviceList, every list libogc makes leaves tablets out,
 * so its mouse and keyboard drivers only see the rest; this driver asks for
 * the real list. */
s32 __wrap_USB_GetDeviceList(usb_device_entry *list, u8 max, u8 cls, u8 *count)
{
	s32 ret = __real_USB_GetDeviceList(list, max, cls, count);
	u8 i, kept = 0;

	if (ret < 0 || !list || !count)
		return ret;
	for (i = 0; i < *count; i++) {
		u16 vid = list[i].vid, pid = list[i].pid;
		if (tablet_vendor(vid) || listed(tb_tablets, tb_ntablets, vid, pid))
			continue;
		list[kept++] = list[i];
	}
	*count = kept;
	return ret;
}

/* Whether the pen's entry is still in the device list: a device that is
 * still listed was not unplugged, whatever the removal hook said. */
static bool still_listed(void)
{
	static usb_device_entry list[MAX_DEVICES] ATTRIBUTE_ALIGN(32);
	u8 count = 0;
	int i;

	if (tb_oh0)
		return false;
	memset(list, 0, sizeof(list));
	if (__real_USB_GetDeviceList(list, MAX_DEVICES, USB_CLASS_HID, &count) < 0)
		return false;
	for (i = 0; i < count; i++)
		if (list[i].device_id == tb_pen_id)
			return true;
	return false;
}

/* Opens each HID device in turn, keeping the first that is a tablet.
 * USBV0 IOSes open devices by VID/PID through /dev/usb/oh0, USBV5 ones by
 * the device_id the list hands back. */
static s32 open_tablet(void)
{
	static usb_device_entry list[MAX_DEVICES] ATTRIBUTE_ALIGN(32);
	u8 count = 0;
	bool oh0;
	s32 fd;
	int i;

	fd = IOS_Open("/dev/usb/oh0", 0);
	oh0 = fd >= 0;
	if (oh0)
		IOS_Close(fd);

	memset(list, 0, sizeof(list));
	if (__real_USB_GetDeviceList(list, MAX_DEVICES, USB_CLASS_HID, &count) < 0)
		return TB_ENODEV;
	if (count != tb_last_count) {
		tb_log("  IOS %d, %d HID device(s)\n", (int)IOS_GetVersion(), (int)count);
		tb_last_count = count;
	}
	tb_oh0 = oh0;

	for (i = 0; i < count; i++) {
		u16 vid = list[i].vid, pid = list[i].pid;

		if ((vid == 0 && pid == 0) || (vid == 0x057E && pid == 0x0308) ||
		    listed(tb_others, tb_nothers, vid, pid))
			continue;

		tb_unplugged = false;
		/* Once per new device list: the scan repeats every second. */
		if (count != tb_logged_count)
			tb_log("  %04x:%04x id %08x token %08x (%s)\n", vid, pid,
			       (unsigned)list[i].device_id, (unsigned)list[i].token,
			       oh0 ? "oh0" : "usbv5");
		if (USB_OpenDevice(oh0 ? USB_OH0_DEVICE_ID : list[i].device_id,
		                   vid, pid, &tb_fd) < 0)
			continue;

		if (probe_device(vid, pid)) {
			remember(tb_tablets, &tb_ntablets, vid, pid);
			tb_opened = true;
			place_pipes(list, count, i);
			/* Only now: opening and closing entries sets off false removals. */
			tb_unplugged = false;
			USB_DeviceRemovalNotifyAsync(tb_fd, tb_removed_cb, NULL);
			return TB_OK;
		}
		if (!tb_probe_unsure) {
			remember(tb_others, &tb_nothers, vid, pid);
			tb_log("  %04x:%04x is not a tablet, skipped from now on\n", vid, pid);
		}
		USB_CloseDevice(&tb_fd);
		tb_fd = -1;
	}
	tb_logged_count = count;
	return TB_ENODEV;
}

static void close_tablet(void)
{
	if (!tb_opened)
		return;

	/* A read stays outstanding while the pen is away.  Under oh0 closing
	 * is a real IOS_Close, which waits for it -- forever, if nobody
	 * touches the tablet -- so give it a moment and otherwise leave the
	 * handle to the IOS reload that ends the app.  USBV5's close never
	 * reaches IOS. */
	if (tb_oh0 && ((tb_pipes[PEN].x.pending && !tb_wait(&tb_pipes[PEN].x, 50)) ||
	               (tb_pipes[TOUCH].x.pending && !tb_wait(&tb_pipes[TOUCH].x, 50))))
		tb_log("  tablet left open: a read is outstanding\n");
	else {
		if (tb_pipes[TOUCH].own_fd)
			USB_CloseDevice(&tb_pipes[TOUCH].fd);
		USB_CloseDevice(&tb_fd);
	}
	tb_fd = -1;
	tb_opened = false;
	/* Whatever was outstanding belongs to the old device now. */
	memset(tb_pipes, 0, sizeof(tb_pipes));
	tb_ctl_x.pending = false;
	tb_log("  tablet closed\n");
}

/* ---- reports ------------------------------------------------------------- */

static void push(const tb_sample *in)
{
	tb_sample s = *in;
	u32 next;

	s.wiimote = -1;
	LWP_MutexLock(tb_lock);
	if (s.finger)
		tb_state.pad = s.pad;
	else
		tb_state = s;
	next = (tb_head + 1) & QUEUE_MASK;
	if (next == tb_tail)
		tb_tail = (tb_tail + 1) & QUEUE_MASK; /* drop the oldest */
	tb_queue[tb_head] = s;
	tb_head = next;
	LWP_MutexUnlock(tb_lock);
}

static s32 get(const hidp_field *f, const u8 *data, u32 len)
{
	return f ? hidp_get(f, data, len) : 0;
}

static void finish(tb_sample *s)
{
	s->time = (u32)ticks_to_millisecs(gettime());
	if (s->x < 0) s->x = 0;
	if (s->y < 0) s->y = 0;
	if (s->x > tb_dev.max_x) s->x = tb_dev.max_x;
	if (s->y > tb_dev.max_y) s->y = tb_dev.max_y;
	if (s->raw_pressure > tb_dev.max_pressure)
		s->raw_pressure = tb_dev.max_pressure;
	s->nx = (float)s->x / (float)tb_dev.max_x;
	s->ny = (float)s->y / (float)tb_dev.max_y;
	s->pressure = (float)s->raw_pressure / (float)tb_dev.max_pressure;
}

static void legacy_report(const u8 *r, u32 len)
{
	tb_sample s = tb_state;
	bool prox;

	if (r[0] != LEGACY_REPORT || len < 8) {
		tb_st.other++;
		return;
	}
	tb_st.pen_reports++;

	s.in_range = (r[1] & 0x80) != 0;
	prox = (r[1] & 0x40) != 0;
	s.buttons = 0;
	s.raw_pressure = 0;
	if (r[1] & 0x20) { /* data ready */
		s.raw_pressure = le16(r + 6);
		if (r[1] & 0x01) s.buttons |= TB_BUTTON_TIP;
		if (r[1] & 0x02) s.buttons |= TB_BUTTON_BARREL;
		if (r[1] & 0x04) s.buttons |= TB_BUTTON_BARREL2;
	}
	if (prox) {
		s.x = le16(r + 2);
		s.y = le16(r + 4);
		s.eraser = (r[1] & 0x08) != 0;
		/* The range table is a guess; let the pen correct it. */
		if (s.x > tb_dev.max_x) tb_dev.max_x = s.x;
		if (s.y > tb_dev.max_y) tb_dev.max_y = s.y;
		if (s.raw_pressure > tb_dev.max_pressure)
			tb_dev.max_pressure = s.raw_pressure > 1023 ? 2047 : 1023;
	}
	finish(&s);
	push(&s);
}

static void hid_report(const u8 *r, u32 len)
{
	u8 id = tb_desc.uses_ids ? r[0] : 0;
	const u8 *data = tb_desc.uses_ids ? r + 1 : r;
	u32 n = tb_desc.uses_ids ? len - 1 : len;
	tb_sample s = tb_state;
	bool used = false;
	int i;

	if (len == 0)
		return;

	for (i = 0; i < tb_pen.npad; i++) {
		const hidp_field *f = tb_pen.pad[i];

		if (f->report_id != id)
			continue;
		used = true;
		if (hidp_get(f, data, n))
			s.pad |= (u16)(1u << i);
		else
			s.pad &= (u16)~(1u << i);
	}

	if (id == tb_pen.report_id) {
		used = true;
		if (tb_pen.valid && !hidp_get(tb_pen.valid, data, n))
			return;
		tb_st.pen_reports++;

		s.x = get(tb_pen.x, data, n) - tb_pen.x->logical_min;
		s.y = get(tb_pen.y, data, n) - tb_pen.y->logical_min;
		s.raw_pressure = tb_pen.pressure ? (u32)get(tb_pen.pressure, data, n) : 0;
		s.in_range = tb_pen.in_range ? get(tb_pen.in_range, data, n) != 0 : true;
		s.eraser = get(tb_pen.eraser, data, n) || get(tb_pen.invert, data, n);
		s.buttons = 0;
		if (tb_pen.tip ? get(tb_pen.tip, data, n) || get(tb_pen.eraser, data, n)
		               : s.raw_pressure > 0)
			s.buttons |= TB_BUTTON_TIP;
		if (get(tb_pen.barrel, data, n))
			s.buttons |= TB_BUTTON_BARREL;
		if (get(tb_pen.barrel2, data, n))
			s.buttons |= TB_BUTTON_BARREL2;
		s.tilt_x = (s8)get(tb_pen.tilt_x, data, n);
		s.tilt_y = (s8)get(tb_pen.tilt_y, data, n);
		if (!s.in_range) {
			s.buttons = 0;
			s.raw_pressure = 0;
		}
	}

	if (!used) {
		tb_st.other++;
		return;
	}
	finish(&s);
	push(&s);
}

static u16 be16(const u8 *p)
{
	return (u16)((p[0] << 8) | p[1]);
}

/* The tablet keys, in both touch formats: Linux's BTN_LEFT, BTN_FORWARD,
 * BTN_BACK and BTN_RIGHT become keys 0-3. */
static u16 bamboo_keys(u8 bits)
{
	return (u16)(((bits & 0x08) ? 1 : 0) | ((bits & 0x04) ? 2 : 0) |
	             ((bits & 0x02) ? 4 : 0) | ((bits & 0x01) ? 8 : 0));
}

/* A Bamboo touch packet, report 2.  First-generation tablets send 20 bytes
 * holding two finger slots and the keys; later ones up to seven 8-byte
 * messages, each a finger (IDs 2-17) or the keys (ID 128). */
static void touch_report(const u8 *r, u32 len)
{
	tb_sample s = tb_state;
	bool pen_near = tb_state.in_range;
	int i, first = -1;

	if (len < 2 || r[0] != LEGACY_REPORT) {
		tb_st.other++;
		return;
	}
	tb_st.touch_reports++;

	/* The endpoint's size says which format, when IOS reports it (it gave
	 * the CTH-460's touch endpoint as 0); else the packet's length. */
	if (tb_touch_pkt >= 64 || (tb_touch_pkt == 0 && len > 20)) {
		int count = r[1] & 0x07;

		for (i = 0; i < count && 2 + 8 * i + 8 <= (int)len; i++) {
			const u8 *m = r + 2 + 8 * i;

			if (m[0] >= 2 && m[0] < 2 + TB_MAX_FINGERS) {
				tb_finger *f = &tb_fingers[m[0] - 2];

				f->down = (m[1] & 0x80) != 0;
				if (f->down) {
					f->x = (m[2] << 4) | (m[4] >> 4);
					f->y = (m[3] << 4) | (m[4] & 0x0F);
				}
			} else if (m[0] == 128)
				s.pad = bamboo_keys(m[1]);
		}
	} else {
		for (i = 0; i < 2; i++) {
			int off = (r[1] & 0x80) ? 8 * i : 9 * i;
			tb_finger *f = &tb_fingers[i];

			if ((u32)off + 7 > len)
				break;
			f->down = (r[off + 3] & 0x80) != 0;
			if (f->down) {
				f->x = be16(r + off + 3) & 0x7FF;
				f->y = be16(r + off + 5) & 0x7FF;
			}
		}
		s.pad = bamboo_keys(r[1]);
	}

	for (i = 0; i < TB_MAX_FINGERS; i++) {
		tb_finger *f = &tb_fingers[i];

		/* A hand resting on the tablet while the pen draws is not input,
		 * as the tablet's own driver has it. */
		if (pen_near)
			f->down = false;
		/* The descriptor's range is a starting point; follow the fingers. */
		if (f->x > tb_dev.touch_max_x) tb_dev.touch_max_x = f->x;
		if (f->y > tb_dev.touch_max_y) tb_dev.touch_max_y = f->y;
		f->nx = (float)f->x / (float)(tb_dev.touch_max_x ? tb_dev.touch_max_x : 1);
		f->ny = (float)f->y / (float)(tb_dev.touch_max_y ? tb_dev.touch_max_y : 1);
		if (f->down && first < 0)
			first = i;
	}

	s.finger = true;
	s.eraser = false;
	s.tilt_x = s.tilt_y = 0;
	s.buttons = first >= 0 ? TB_BUTTON_TIP : 0;
	s.in_range = first >= 0;
	if (first >= 0) {
		s.x = (s32)(tb_fingers[first].nx * tb_dev.max_x);
		s.y = (s32)(tb_fingers[first].ny * tb_dev.max_y);
	}
	s.raw_pressure = 0;
	finish(&s);
	s.pressure = first >= 0 ? 0.5f : 0.0f;
	push(&s);
}

/* The first few distinct failure codes, which say more than the count. */
static void note_error(s32 ret)
{
	int i;

	for (i = 0; i < tb_st.nfirst; i++)
		if (tb_st.first_err[i] == ret)
			return;
	if (tb_st.nfirst < 4) {
		tb_st.first_err[tb_st.nfirst++] = ret;
		tb_log("  read failed: %d\n", (int)ret);
	}
}

static bool pipe_submit(tb_pipe *p, int idx)
{
	s32 ret;

	p->buf = tb_intr[idx];
	memset(p->buf, 0, INTR_MAX);
	DCFlushRange(p->buf, INTR_MAX);
	p->x.done = false;
	p->x.pending = true;
	/* A full-speed interrupt packet is at most 64 bytes, so asking for that
	 * much always fits whatever the tablet sends, whatever its endpoint
	 * says. */
	ret = USB_ReadIntrMsgAsync(p->fd, p->ep, INTR_MAX, p->buf, tb_xfer_cb, &p->x);
	if (ret < 0) {
		p->x.pending = false;
		note_error(ret);
		tb_st.errors++;
		tb_st.last_ret = ret;
		p->fails++;
		return false;
	}
	return true;
}

/* Hands a finished read to its parser. */
static void pipe_collect(tb_pipe *p, int idx)
{
	s32 ret = p->x.res;

	p->x.pending = false;
	tb_st.last_ret = ret;
	if (ret < 0) {
		note_error(ret);
		tb_st.errors++;
		p->fails++;
		return;
	}
	p->fails = 0;
	if (ret > INTR_MAX)
		ret = INTR_MAX;
	DCInvalidateRange(p->buf, INTR_MAX);

	tb_st.reports++;
	tb_st.last_len = (u8)ret;
	tb_st.last_id = ret > 0 ? p->buf[0] : 0;
	memcpy(tb_st.last, p->buf, (u32)ret);

	if (idx == TOUCH)
		touch_report(p->buf, (u32)ret);
	else if (tb_dev.legacy)
		legacy_report(p->buf, (u32)ret);
	else
		hid_report(p->buf, (u32)ret);
}

static void *tb_worker(void *arg)
{
	(void)arg;

	while (tb_running) {
		bool waiting = false;
		int i;

		if (!tb_opened) {
			int ms;

			if (open_tablet() == TB_OK)
				continue;
			/* Nothing yet: look again in a second. */
			for (ms = 0; ms < 1000 && tb_running; ms += 10)
				usleep(10 * 1000);
			continue;
		}

		/* libogc's USBV5 close calls every removal hook on a device, so
		 * another driver opening the tablet to look at it and closing it
		 * again -- libwiikeyboard does, on every rescan -- reads here as an
		 * unplug.  Believe it only once the device has left the list. */
		if (tb_unplugged && still_listed()) {
			if (tb_st.false_removals++ == 0)
				tb_log("  removal notice, but still listed: ignoring these\n");
			tb_unplugged = false;
			USB_DeviceRemovalNotifyAsync(tb_fd, tb_removed_cb, NULL);
		}
		/* Gone: close, and look for it again.  Failing reads are not a
		 * reason to: a reopen abandons the reads in flight, and every one
		 * keeps a piece of libogc's 16 KB USB heap until it completes. */
		if (tb_unplugged) {
			tb_sample s = tb_state;

			close_tablet();
			s.in_range = false;
			s.buttons = 0;
			s.pad = 0;
			s.finger = false;
			push(&s);
			continue;
		}
		if (tb_pipes[TOUCH].on && tb_pipes[TOUCH].fails >= 200 &&
		    !tb_pipes[TOUCH].x.pending) {
			tb_log("  touch: giving up on it\n");
			tb_pipes[TOUCH].on = false;
		}

		for (i = 0; i < 2; i++) {
			tb_pipe *p = &tb_pipes[i];

			if (p->on && !p->x.pending && !pipe_submit(p, i))
				usleep(p->fails > 20 ? 100 * 1000 : 5 * 1000);
			if (p->x.pending)
				waiting = true;
		}
		if (!waiting)
			continue;

		/* Neither pen nor finger sends anything while away, so no
		 * deadline: wait for whichever answers first. */
		while (tb_running && !tb_unplugged && !(tb_pipes[PEN].x.pending && tb_pipes[PEN].x.done) &&
		       !(tb_pipes[TOUCH].x.pending && tb_pipes[TOUCH].x.done))
			usleep(1000);

		for (i = 0; i < 2; i++) {
			tb_pipe *p = &tb_pipes[i];

			if (p->x.pending && p->x.done) {
				pipe_collect(p, i);
				if (p->x.res < 0)
					usleep(p->fails > 20 ? 100 * 1000 : 5 * 1000);
			}
		}
	}

	close_tablet();
	tb_exited = true;
	return NULL;
}

/* ---- API ----------------------------------------------------------------- */

s32 TB_Init(void)
{
	if (tb_running)
		return TB_OK;

	if (tb_lock == LWP_MUTEX_NULL)
		LWP_MutexInit(&tb_lock, false);
	USB_Initialize();

	memset(&tb_state, 0, sizeof(tb_state));
	tb_state.wiimote = -1;
	memset(&tb_st, 0, sizeof(tb_st));
	tb_head = tb_tail = 0;

	tb_running = true;
	tb_exited = false;
	if (LWP_CreateThread(&tb_thread, tb_worker, NULL, tb_stack, STACKSIZE, 70) < 0) {
		tb_running = false;
		tb_thread = LWP_THREAD_NULL;
		return TB_ENOMEM;
	}
	return TB_OK;
}

void TB_Shutdown(void)
{
	int ms;

	if (!tb_running)
		return;
	tb_running = false;
	if (tb_thread == LWP_THREAD_NULL)
		return;

	/* The worker can be inside a libogc call with no deadline (opening a
	 * USBV5 HID device reads its descriptors synchronously), and joining it
	 * would then hang the caller.  Give it half a second; if it is still
	 * stuck, leave it to the IOS reload that normally follows. */
	for (ms = 0; ms < 500 && !tb_exited; ms += 10)
		usleep(10 * 1000);
	if (tb_exited)
		LWP_JoinThread(tb_thread, NULL);
	else
		tb_log("  tablet thread did not stop\n");
	tb_thread = LWP_THREAD_NULL;
}

bool TB_Connected(void)
{
	tb_info info;
	return tb_opened || inpw_udraw_info(&info);
}

bool TB_GetInfo(tb_info *info)
{
	tb_info ud;

	if (!tb_opened) {
		if (!inpw_udraw_info(&ud))
			return false;
		if (info)
			*info = ud;
		return true;
	}
	if (info)
		*info = tb_dev;
	return true;
}

void TB_GetState(tb_sample *out)
{
	tb_sample ud;

	if (!out)
		return;
	/* before TB_Init there is no lock, and no USB tablet */
	if (tb_lock != LWP_MUTEX_NULL) {
		LWP_MutexLock(tb_lock);
		*out = tb_state;
		LWP_MutexUnlock(tb_lock);
	} else {
		memset(out, 0, sizeof(*out));
		out->wiimote = -1;
	}
	if (inpw_udraw_state(&ud) && (!tb_opened || (s32)(ud.time - out->time) > 0))
		*out = ud;
}

bool TB_Pop(tb_sample *out)
{
	tb_sample s;
	bool got = false;

	if (tb_lock != LWP_MUTEX_NULL) {
		LWP_MutexLock(tb_lock);
		if (tb_tail != tb_head) {
			s = tb_queue[tb_tail];
			tb_tail = (tb_tail + 1) & QUEUE_MASK;
			got = true;
		}
		LWP_MutexUnlock(tb_lock);
	}
	if (!got)
		got = inpw_udraw_pop(&s);
	if (got && out)
		*out = s;
	return got;
}

void TB_ToScreen(const tb_sample *s, int w, int h, int *x, int *y)
{
	s32 max_x = tb_dev.max_x, max_y = tb_dev.max_y;
	float aspect, aw, ah;

	if (s && s->wiimote >= 0)
		inpw_udraw_extent(&max_x, &max_y);
	aspect = (float)max_x / (float)(max_y ? max_y : 1);
	aw = (float)w;
	ah = (float)w / aspect;
	if (!s || max_x <= 0) {
		if (x) *x = 0;
		if (y) *y = 0;
		return;
	}
	if (ah > (float)h) {
		ah = (float)h;
		aw = ah * aspect;
	}
	if (x) *x = (int)(((float)w - aw) / 2 + s->nx * (aw - 1));
	if (y) *y = (int)(((float)h - ah) / 2 + s->ny * (ah - 1));
}

u32 TB_GetFingers(tb_finger *out, u32 max)
{
	u32 i, n = 0;

	LWP_MutexLock(tb_lock);
	for (i = 0; i < TB_MAX_FINGERS && n < max; i++)
		if (tb_fingers[i].down && out)
			out[n++] = tb_fingers[i];
	LWP_MutexUnlock(tb_lock);
	return n;
}

void TB_GetStats(tb_stats *out)
{
	if (out)
		*out = tb_st;
}

#endif /* HAVE_TABLET */
