/*
 * xinput_usb.c - Xbox 360 (XInput) USB controllers on the Nintendo Wii
 *
 * Copyright (c) 2026 VrepliroidV
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * How it works
 *
 * IOS lists every interface of a USB device as its own entry, so a wired
 * Xbox 360 pad shows up several times with the same VID:PID (gamepad,
 * headset, expansion port, security). Each entry from a known XInput vendor
 * is opened once; the one whose interface is FF/5D/01 (or, for 045e:028e,
 * a vendor interface with endpoint 0x81) is kept, the others are closed and
 * remembered so they are not opened again until the device list changes.
 *
 * After a device is kept it gets the same start-up the Windows driver gives
 * it, every step asynchronous and with a timeout:
 *   1. GET_CONFIGURATION, and SET_CONFIGURATION only if it is not set yet
 *      (IOS has normally configured the device already);
 *   2. the vendor request Windows sends before reading (0xC1, 0x01, 0x0100,
 *      20 bytes). Some third-party pads and receivers send no reports until
 *      they get it, and some reset themselves after a second without it;
 *   3. the player LED message;
 *   4. interrupt IN reads.
 * Callbacks run in the IPC interrupt handler; they only copy data and set
 * flags. Closing and logging happen in XInput_Update() on the main thread.
 */

#ifdef HW_RVL

#include <gccore.h>
#include <ogc/usb.h>
#include <ogc/cache.h>
#include <ogc/irq.h>
#include <ogc/lwp_watchdog.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "xinput_usb.h"

#define XI_MAX_PADS          XI_MAX_PORTS
#define XI_LIST_MAX          32     /* libogc tracks up to 32 devices per host */
#define XI_REJECT_MAX        32
#define XI_SCAN_INTERVAL     30     /* updates between device list checks (~0.5 s) */
#define XI_STEP_TIMEOUT      30     /* updates to wait for each start-up request */
#define XI_SETTLE_FRAMES     30     /* wait after the device list changes before opening */
#define XI_RETRY_WAIT        15     /* updates before restarting a failed read */
#define XI_RETRY_WAIT_MAX    120    /* longest wait between restarts (~2 s) */
#define XI_BUF_SIZE          64
#define XI_REPORT_MIN        14     /* bytes up to the right stick */

#define XI_TRIGGER_ON        0x80
#define XI_STICK_ON          16384

#define XI_IF_CLASS          0xFF
#define XI_IF_SUBCLASS       0x5D
#define XI_IF_PROTOCOL       0x01

/* start-up steps */
#define STEP_GETCFG 0
#define STEP_SETCFG 1
#define STEP_MAGIC  2
#define STEP_READY  3

#ifdef XINPUT_USB_LOG
#define XI_LOG_MAX_LINES     400
#endif

typedef struct
{
	bool active;
	u8 gen;                 /* changes on every attach, tags the callbacks */
	s32 fd;
	s32 device_id;
	u16 vid, pid;
	u8 ep_in, ep_out;
	u16 in_len;
	u8 config;
	u8 port;
	u8 led_port;            /* port shown on the LED, 0xFF before the first send */
	u8 step;
	bool step_sent;
	u16 step_wait;
	u16 retry_wait;
	u16 errors;
	u32 reports_at_restart;
	u32 prev_held;
	bool logged_first;
	bool logged_led;

	/* written from USB callbacks */
	volatile u8 ctrl_busy;
	volatile s32 ctrl_result;
	volatile u8 reading;
	volatile u8 removed;
	volatile u8 led_busy;
	volatile s32 led_result;
	volatile s32 read_result;
	volatile u32 held;
	volatile u32 reports;
	volatile u8 first_len;
	u8 first_raw[20];
} XiPad;

static XiPad pads[XI_MAX_PADS];
static u8 ATTRIBUTE_ALIGN(32) rxbuf[XI_MAX_PADS][XI_BUF_SIZE];
static u8 ATTRIBUTE_ALIGN(32) txbuf[XI_MAX_PADS][32];
static u8 ATTRIBUTE_ALIGN(32) ctrlbuf[XI_MAX_PADS][32];

static s32 rejected[XI_REJECT_MAX];
static int num_rejected = 0;
static bool started = false;
static bool usb_init_tried = false;
static bool replug_needed = false;
static bool have_signature = false;
static u32 list_signature = 0;
static u32 frames_to_scan = 0;
static volatile u8 rescan_requested = 1;

/* Vendors known to ship XInput devices. Anything else on the bus (USB drives,
   network adapters) is never opened. The interface check still decides. */
static const u16 xinput_vendors[] =
{
	0x045e, /* Microsoft, and most 2.4 GHz receivers that reuse its ID */
	0x044f, /* Thrustmaster */
	0x046d, /* Logitech */
	0x056e, /* Elecom */
	0x06a3, /* Saitek */
	0x0738, /* Mad Catz */
	0x07ff, /* Mad Catz */
	0x0e6f, /* PDP / Logic3 */
	0x0f0d, /* Hori */
	0x11c9, /* Nacon */
	0x12ab, /* Honey Bee */
	0x1430, /* RedOctane */
	0x146b, /* BigBen */
	0x1532, /* Razer */
	0x15e4, /* Numark */
	0x162e, /* Joytech */
	0x1689, /* Razer Onza */
	0x1bad, /* Harmonix / Mad Catz */
	0x20d6, /* PowerA / BDA */
	0x24c6, /* PowerA */
	0x2dc8, /* 8BitDo */
	0x2e24, /* Hyperkin */
	0x3285  /* Nacon */
};

/* ------------------------------------------------------------------------ */
/* Log                                                                      */
/* ------------------------------------------------------------------------ */

#ifdef XINPUT_USB_LOG
static int log_lines = 0;

static void xi_log(const char *fmt, ...)
{
	if (log_lines >= XI_LOG_MAX_LINES)
		return;

	FILE *f = fopen(XINPUT_USB_LOG, "a");
	if (f == NULL)
		return;

	/* time since the first log line, for spotting resets and bursts */
	static u64 t0 = 0;
	u64 now = gettime();
	if (t0 == 0)
		t0 = now;
	u32 ms = (u32) ticks_to_millisecs(now - t0);
	fprintf(f, "%5u.%03u ", ms / 1000, ms % 1000);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);

	if (++log_lines == XI_LOG_MAX_LINES)
	{
		f = fopen(XINPUT_USB_LOG, "a");
		if (f)
		{
			fputs("(log full)\n", f);
			fclose(f);
		}
	}
}
#else
#define xi_log(...) do { } while (0)
#endif

/* ------------------------------------------------------------------------ */
/* Callbacks (IPC interrupt context)                                        */
/* ------------------------------------------------------------------------ */

/* token = tag << 16 | gen << 8 | slot. gen changes on every attach, tag
   tells start-up steps apart, so a late answer to an old request is ignored. */
static void *make_token_tag(int idx, u8 tag)
{
	return (void *) (uintptr_t) (((u32) tag << 16) | ((u32) pads[idx].gen << 8) | (u32) idx);
}

static void *make_token(int idx)
{
	return make_token_tag(idx, 0xFF);
}

static XiPad *pad_from_token(void *token)
{
	u32 v = (u32) (uintptr_t) token;
	u32 idx = v & 0xFF;
	if (idx >= XI_MAX_PADS)
		return NULL;
	XiPad *p = &pads[idx];
	if (!p->active || p->gen != ((v >> 8) & 0xFF))
		return NULL;
	return p;
}

static u32 parse_report(const u8 *b)
{
	u32 bits = ((u32) b[2] | ((u32) b[3] << 8)) & 0xF7FF;
	s16 lx = (s16) (b[6]  | (b[7]  << 8));
	s16 ly = (s16) (b[8]  | (b[9]  << 8));
	s16 rx = (s16) (b[10] | (b[11] << 8));
	s16 ry = (s16) (b[12] | (b[13] << 8));

	if (b[4] >= XI_TRIGGER_ON) bits |= XI_LT;
	if (b[5] >= XI_TRIGGER_ON) bits |= XI_RT;

	if (ly >  XI_STICK_ON) bits |= XI_LSTICK_UP;
	if (ly < -XI_STICK_ON) bits |= XI_LSTICK_DOWN;
	if (lx < -XI_STICK_ON) bits |= XI_LSTICK_LEFT;
	if (lx >  XI_STICK_ON) bits |= XI_LSTICK_RIGHT;

	if (ry >  XI_STICK_ON) bits |= XI_RSTICK_UP;
	if (ry < -XI_STICK_ON) bits |= XI_RSTICK_DOWN;
	if (rx < -XI_STICK_ON) bits |= XI_RSTICK_LEFT;
	if (rx >  XI_STICK_ON) bits |= XI_RSTICK_RIGHT;

	return bits;
}

static s32 read_cb(s32 result, void *usrdata)
{
	XiPad *p = pad_from_token(usrdata);
	if (p == NULL)
		return 0;

	int idx = p - pads;

	if (p->removed)
	{
		p->reading = 0;
		return 0;
	}

	if (result < 0)
	{
		p->read_result = result;
		p->held = 0; /* no stuck buttons while the read is down */
		p->reading = 0;
		return 0;
	}

	u8 *b = rxbuf[idx];
	DCInvalidateRange(b, XI_BUF_SIZE);

	/* Message type 0 is the input report. The others (LED state, rumble
	   state, attach notices) are 3 bytes long and are skipped. */
	if (result >= XI_REPORT_MIN && b[0] == 0x00)
	{
		p->held = parse_report(b);
		if (p->reports == 0)
		{
			int n = result < (s32) sizeof(p->first_raw) ? result : (s32) sizeof(p->first_raw);
			memcpy(p->first_raw, b, n);
			p->first_len = (u8) n;
		}
		p->reports++;
	}

	if (USB_ReadIntrMsgAsync(p->fd, p->ep_in, p->in_len, b, read_cb, usrdata) < 0)
	{
		p->read_result = -1;
		p->held = 0;
		p->reading = 0;
	}
	return 0;
}

static s32 ctrl_cb(s32 result, void *usrdata)
{
	XiPad *p = pad_from_token(usrdata);
	if (p && p->step == (((u32) (uintptr_t) usrdata >> 16) & 0xFF))
	{
		p->ctrl_result = result;
		p->ctrl_busy = 0;
	}
	return 0;
}

static s32 led_cb(s32 result, void *usrdata)
{
	XiPad *p = pad_from_token(usrdata);
	if (p)
	{
		p->led_result = result;
		p->led_busy = 0;
	}
	return 0;
}

static s32 removal_cb(s32 result, void *usrdata)
{
	XiPad *p = pad_from_token(usrdata);
	if (p)
	{
		p->removed = 1;
		p->reading = 0;
	}
	rescan_requested = 1;
	return 0;
}

/* ------------------------------------------------------------------------ */
/* Device handling (main thread)                                            */
/* ------------------------------------------------------------------------ */

static bool known_vendor(u16 vid)
{
	for (unsigned i = 0; i < sizeof(xinput_vendors) / sizeof(xinput_vendors[0]); i++)
		if (xinput_vendors[i] == vid)
			return true;
	return false;
}

/* device_id is unique per attach on USBv5, so a skipped entry stays skipped
   until it is unplugged. Old IOS reports every device_id as 0; VID:PID is
   the key there. */
static s32 entry_key(const usb_device_entry *e)
{
	return e->device_id ? e->device_id : (s32) (((u32) e->vid << 16) | e->pid);
}

static bool is_rejected(s32 key)
{
	for (int i = 0; i < num_rejected; i++)
		if (rejected[i] == key)
			return true;
	return false;
}

static void reject(s32 key)
{
	if (!is_rejected(key) && num_rejected < XI_REJECT_MAX)
		rejected[num_rejected++] = key;
}

/* Forget skipped entries that are no longer on the bus. */
static void prune_rejected(const usb_device_entry *list, u8 count)
{
	int kept = 0;
	for (int i = 0; i < num_rejected; i++)
	{
		bool present = false;
		for (int k = 0; k < count; k++)
			if (entry_key(&list[k]) == rejected[i])
				present = true;
		if (present)
			rejected[kept++] = rejected[i];
	}
	num_rejected = kept;
}

static bool is_attached(s32 device_id, u16 vid, u16 pid)
{
	for (int i = 0; i < XI_MAX_PADS; i++)
	{
		if (!pads[i].active)
			continue;
		if (device_id != 0 && pads[i].device_id == device_id)
			return true;
		/* old IOS without USBv5: every device_id is 0 */
		if (device_id == 0 && pads[i].vid == vid && pads[i].pid == pid)
			return true;
	}
	return false;
}

static int free_port(void)
{
	for (int port = 0; port < XI_MAX_PORTS; port++)
	{
		bool used = false;
		for (int i = 0; i < XI_MAX_PADS; i++)
			if (pads[i].active && pads[i].port == port)
				used = true;
		if (!used)
			return port;
	}
	return 0;
}

typedef struct
{
	u8 ep_in, ep_out;
	u16 in_len;
	u8 config;
	u8 ifclass, ifsub, ifproto;
} XiIface;

/* Finds the XInput gamepad interface. For 045e:028e it also accepts a vendor
   interface whose interrupt IN endpoint is 0x81, like many clones. */
static bool find_interface(const usb_devdesc *dd, bool is_360_id, XiIface *out)
{
	if (dd->configurations == NULL)
		return false;

	for (int c = 0; c < dd->bNumConfigurations; c++)
	{
		const usb_configurationdesc *cfg = &dd->configurations[c];
		if (cfg->interfaces == NULL)
			continue;

		for (int i = 0; i < cfg->bNumInterfaces; i++)
		{
			const usb_interfacedesc *ifd = &cfg->interfaces[i];
			XiIface f;
			memset(&f, 0, sizeof(f));
			f.config = cfg->bConfigurationValue ? cfg->bConfigurationValue : 1;
			f.ifclass = ifd->bInterfaceClass;
			f.ifsub = ifd->bInterfaceSubClass;
			f.ifproto = ifd->bInterfaceProtocol;

			if (ifd->endpoints != NULL)
			{
				for (int e = 0; e < ifd->bNumEndpoints; e++)
				{
					const usb_endpointdesc *ep = &ifd->endpoints[e];
					if ((ep->bmAttributes & 0x03) != USB_ENDPOINT_INTERRUPT)
						continue;
					if ((ep->bEndpointAddress & 0x80) == USB_ENDPOINT_IN)
					{
						if (f.ep_in == 0)
						{
							f.ep_in = ep->bEndpointAddress;
							f.in_len = ep->wMaxPacketSize;
						}
					}
					else if (f.ep_out == 0)
					{
						f.ep_out = ep->bEndpointAddress;
					}
				}
			}

			xi_log("  interface %d: class %02x/%02x/%02x in=%02x(%u) out=%02x",
				ifd->bInterfaceNumber, f.ifclass, f.ifsub, f.ifproto,
				f.ep_in, f.in_len, f.ep_out);

			if (f.ep_in == 0)
				continue;

			bool signature = f.ifclass == XI_IF_CLASS && f.ifsub == XI_IF_SUBCLASS &&
				f.ifproto == XI_IF_PROTOCOL;
			bool clone = is_360_id && f.ifclass == XI_IF_CLASS && f.ep_in == 0x81;

			if (signature || clone)
			{
				if (f.in_len == 0 || f.in_len > XI_BUF_SIZE)
					f.in_len = 32;
				*out = f;
				return true;
			}
		}
	}
	return false;
}

static void try_attach(const usb_device_entry *e)
{
	int idx = -1;
	for (int i = 0; i < XI_MAX_PADS; i++)
	{
		if (!pads[i].active)
		{
			idx = i;
			break;
		}
	}
	if (idx < 0)
		return; /* all slots in use; try again when one frees up */

	xi_log("open %04x:%04x id=%08x", e->vid, e->pid, (u32) e->device_id);

	s32 fd = -1;
	s32 ret = USB_OpenDevice(e->device_id, e->vid, e->pid, &fd);
	if (ret < 0)
	{
		xi_log("  open failed (%d)", ret);
		reject(entry_key(e));
		return;
	}

	usb_devdesc dd;
	memset(&dd, 0, sizeof(dd));
	ret = USB_GetDescriptors(fd, &dd);
	if (ret < 0)
	{
		/* IOS sometimes refuses this until the device is plugged in again */
		xi_log("  descriptors failed (%d): replug the receiver", ret);
		replug_needed = true;
		USB_CloseDevice(&fd);
		reject(entry_key(e));
		return;
	}

	bool is_360_id = e->vid == 0x045e && e->pid == 0x028e;
	XiIface f;
	bool ok = find_interface(&dd, is_360_id, &f);
	USB_FreeDescriptors(&dd);

	if (!ok)
	{
		xi_log("  not a gamepad interface, skipped");
		USB_CloseDevice(&fd);
		reject(entry_key(e));
		return;
	}

	XiPad *p = &pads[idx];
	u8 gen = p->gen + 1;
	memset(p, 0, sizeof(*p));
	p->gen = gen;
	p->fd = fd;
	p->device_id = e->device_id;
	p->vid = e->vid;
	p->pid = e->pid;
	p->ep_in = f.ep_in;
	p->ep_out = f.ep_out;
	p->in_len = f.in_len;
	p->config = f.config;
	p->port = (u8) free_port();
	p->led_port = 0xFF;
	p->step = STEP_GETCFG;
	p->active = true;
	replug_needed = false;

	xi_log("  gamepad on port %d (in=%02x len=%u out=%02x config=%u)",
		p->port + 1, p->ep_in, p->in_len, p->ep_out, p->config);

	ret = USB_DeviceRemovalNotifyAsync(fd, removal_cb, make_token(idx));
	if (ret < 0)
		xi_log("  removal notify: %d", ret);
}

static void close_pad(int idx, const char *why)
{
	XiPad *p = &pads[idx];
	xi_log("port %d closed: %s", p->port + 1, why);

	/* Mark inactive first: callbacks for this attach become no-ops. */
	u32 level = IRQ_Disable();
	p->active = false;
	p->reading = 0;
	IRQ_Restore(level);

	s32 fd = p->fd;
	if (!p->removed)
		USB_CloseDevice(&fd);
	p->fd = -1;
	rescan_requested = 1;
}

static void start_read(int idx)
{
	XiPad *p = &pads[idx];

	/* reports arrived since the last restart: the pad works, reset backoff */
	if (p->reports != p->reports_at_restart)
		p->errors = 0;
	p->reports_at_restart = p->reports;

	p->reading = 1;
	s32 ret = USB_ReadIntrMsgAsync(p->fd, p->ep_in, p->in_len, rxbuf[idx], read_cb, make_token(idx));
	if (ret < 0)
	{
		p->reading = 0;
		p->read_result = ret;
	}
}

static void send_led(int idx)
{
	XiPad *p = &pads[idx];
	if (p->ep_out == 0 || p->led_busy)
		return;

	/* 0x06..0x09: quadrant 1..4 lit */
	txbuf[idx][0] = 0x01;
	txbuf[idx][1] = 0x03;
	txbuf[idx][2] = 0x06 + (p->port & 3);
	DCFlushRange(txbuf[idx], 32);

	p->led_port = p->port;
	p->led_busy = 1;
	if (USB_WriteIntrMsgAsync(p->fd, p->ep_out, 3, txbuf[idx], led_cb, make_token(idx)) < 0)
		p->led_busy = 0;
}

static void issue_ctrl(int idx, u8 reqtype, u8 req, u16 value, u16 index, u16 len)
{
	XiPad *p = &pads[idx];
	p->step_sent = true;
	p->step_wait = 0;
	p->ctrl_result = 0;
	p->ctrl_busy = 1;
	if (len)
		memset(ctrlbuf[idx], 0, sizeof(ctrlbuf[idx]));
	s32 ret = USB_WriteCtrlMsgAsync(p->fd, reqtype, req, value, index, len,
		len ? ctrlbuf[idx] : NULL, ctrl_cb, make_token_tag(idx, p->step));
	if (ret < 0)
	{
		p->ctrl_result = ret;
		p->ctrl_busy = 0;
	}
}

/* One start-up step per call: send the request, then wait for its answer
   or XI_STEP_TIMEOUT updates, then move on whatever the outcome. */
static void run_startup(int idx)
{
	XiPad *p = &pads[idx];
	int port = p->port + 1;
	(void) port; /* only used by the log */

	if (!p->step_sent)
	{
		switch (p->step)
		{
		case STEP_GETCFG:
			issue_ctrl(idx, USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_DEVICE,
				USB_REQ_GETCONFIG, 0, 0, 1);
			break;
		case STEP_SETCFG:
			issue_ctrl(idx, USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_DEVICE,
				USB_REQ_SETCONFIG, p->config, 0, 0);
			break;
		case STEP_MAGIC:
			issue_ctrl(idx, USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_VENDOR | USB_CTRLTYPE_REC_INTERFACE,
				0x01, 0x0100, 0, 20);
			break;
		}
		return;
	}

	bool timed_out = false;
	if (p->ctrl_busy)
	{
		if (++p->step_wait < XI_STEP_TIMEOUT)
			return;
		timed_out = true;
	}
	s32 r = p->ctrl_result;
	if (!timed_out && r > 0)
		DCInvalidateRange(ctrlbuf[idx], sizeof(ctrlbuf[idx]));

	switch (p->step)
	{
	case STEP_GETCFG:
		if (timed_out)
		{
			xi_log("port %d: GET_CONFIGURATION: no answer", port);
			p->step = STEP_MAGIC;
		}
		else if (r >= 1 && ctrlbuf[idx][0] == p->config)
		{
			xi_log("port %d: configuration %u already set", port, ctrlbuf[idx][0]);
			p->step = STEP_MAGIC;
		}
		else
		{
			xi_log("port %d: GET_CONFIGURATION result %d, value %u", port, r, ctrlbuf[idx][0]);
			p->step = STEP_SETCFG;
		}
		break;

	case STEP_SETCFG:
		if (timed_out)
			xi_log("port %d: SET_CONFIGURATION: no answer", port);
		else
			xi_log("port %d: SET_CONFIGURATION result %d", port, r);
		p->step = STEP_MAGIC;
		break;

	case STEP_MAGIC:
		if (timed_out)
			xi_log("port %d: start-up request: no answer", port);
		else if (r > 0)
		{
			char hex[3 * 20 + 1];
			int n = r > 20 ? 20 : r;
			for (int i = 0; i < n; i++)
				sprintf(hex + 3 * i, "%02x ", ctrlbuf[idx][i]);
			hex[3 * n] = 0;
			xi_log("port %d: start-up request answered (%d bytes): %s", port, r, hex);
		}
		else
			xi_log("port %d: start-up request result %d", port, r);
		p->step = STEP_READY;
		send_led(idx);
		break;
	}
	p->step_sent = false;
}

static void service_pad(int idx)
{
	XiPad *p = &pads[idx];

	if (p->removed)
	{
		close_pad(idx, p->reports ? "unplugged" : "unplugged before any report");
		return;
	}

	if (p->step != STEP_READY)
	{
		run_startup(idx);
		return;
	}

	if (!p->reading)
	{
		if (p->retry_wait > 0)
		{
			p->retry_wait--;
			return;
		}
		if (p->read_result < 0)
		{
			/* Some receivers fail reads while their controller is off. Keep
			   retrying, more slowly, and log only the first few. */
			if (p->errors < 5)
				xi_log("port %d: read error %d, retrying", p->port + 1, p->read_result);
			p->read_result = 0;
			p->errors++;
		}
		start_read(idx);
		if (!p->reading)
		{
			p->errors++;
			p->retry_wait = XI_RETRY_WAIT;
		}
		else if (p->errors > 0)
		{
			u32 wait = XI_RETRY_WAIT * (u32) p->errors;
			p->retry_wait = wait > XI_RETRY_WAIT_MAX ? XI_RETRY_WAIT_MAX : (u16) wait;
		}
		return;
	}

	if (p->reports > 0 && !p->logged_first)
	{
		p->logged_first = true;
		char hex[3 * 20 + 1];
		int n = p->first_len;
		for (int i = 0; i < n; i++)
			sprintf(hex + 3 * i, "%02x ", p->first_raw[i]);
		hex[3 * n] = 0;
		xi_log("port %d: first report (%d bytes): %s", p->port + 1, n, hex);
	}

	u32 held = p->held;
	if ((held & XI_GUIDE) && !(p->prev_held & XI_GUIDE))
		xi_log("port %d: Guide pressed", p->port + 1);
	p->prev_held = held;

	if (!p->logged_led && p->led_port != 0xFF && !p->led_busy)
	{
		p->logged_led = true;
		xi_log("port %d: LED message result %d", p->port + 1, p->led_result);
	}
}

static u32 signature_of(const usb_device_entry *list, u8 count)
{
	u32 h = 2166136261u; /* FNV-1a */
	for (int i = 0; i < count; i++)
	{
		u32 w[3] = { (u32) list[i].device_id, list[i].vid, list[i].pid };
		for (int k = 0; k < 3; k++)
			for (int b = 0; b < 4; b++)
			{
				h ^= (w[k] >> (8 * b)) & 0xFF;
				h *= 16777619u;
			}
	}
	return h;
}

static void scan_devices(void)
{
	static usb_device_entry list[XI_LIST_MAX];
	u8 count = 0;

	if (USB_GetDeviceList(list, XI_LIST_MAX, XI_IF_CLASS, &count) < 0)
	{
		if (!usb_init_tried)
		{
			usb_init_tried = true;
			s32 r = USB_Initialize();
			xi_log("USB_Initialize: %d", r);
			(void) r;
		}
		return;
	}
	if (count > XI_LIST_MAX)
		count = XI_LIST_MAX;

	u32 sig = signature_of(list, count);
	if (!have_signature || sig != list_signature)
	{
		have_signature = true;
		list_signature = sig;
		prune_rejected(list, count);

		xi_log("device list (%u):", count);
		for (int i = 0; i < count; i++)
			xi_log("  %04x:%04x id=%08x", list[i].vid, list[i].pid, (u32) list[i].device_id);

		/* Pads whose device left the list are gone, even if no removal
		   callback arrived. */
		for (int i = 0; i < XI_MAX_PADS; i++)
		{
			if (!pads[i].active || pads[i].device_id == 0)
				continue;
			bool present = false;
			for (int k = 0; k < count; k++)
				if (list[k].device_id == pads[i].device_id)
					present = true;
			if (!present)
			{
				pads[i].removed = 1;
				close_pad(i, "no longer listed");
			}
		}

		/* Let a device that just appeared finish its own start-up before
		   it is opened. */
		frames_to_scan = XI_SETTLE_FRAMES;
		return;
	}

	for (int i = 0; i < count; i++)
	{
		const usb_device_entry *e = &list[i];
		if (!known_vendor(e->vid))
			continue;
		if (is_attached(e->device_id, e->vid, e->pid) || is_rejected(entry_key(e)))
			continue;
		try_attach(e);
	}
}

/* ------------------------------------------------------------------------ */
/* Public API                                                               */
/* ------------------------------------------------------------------------ */

void XInput_Update(void)
{
	if (!started)
	{
		started = true;
		xi_log("--- xinput_usb start, IOS %d ---", IOS_GetVersion());
	}

	for (int i = 0; i < XI_MAX_PADS; i++)
		if (pads[i].active)
			service_pad(i);

	if (rescan_requested || frames_to_scan == 0)
	{
		rescan_requested = 0;
		frames_to_scan = XI_SCAN_INTERVAL;
		scan_devices();
	}
	else
	{
		frames_to_scan--;
	}
}

u32 XInput_ButtonsHeld(int port)
{
	u32 bits = 0;
	for (int i = 0; i < XI_MAX_PADS; i++)
	{
		XiPad *p = &pads[i];
		if (p->active && !p->removed && p->port == port && p->reports > 0)
			bits |= p->held;
	}
	return bits;
}

u32 XInput_ConnectedMask(void)
{
	u32 mask = 0;
	for (int i = 0; i < XI_MAX_PADS; i++)
		if (pads[i].active && !pads[i].removed)
			mask |= 1u << pads[i].port;
	return mask;
}

const char *XInput_Status(void)
{
	static char text[48];
	int len = 0;
	text[0] = 0;

	for (int port = 0; port < XI_MAX_PORTS; port++)
	{
		for (int i = 0; i < XI_MAX_PADS; i++)
		{
			XiPad *p = &pads[i];
			if (!p->active || p->removed || p->port != port)
				continue;
			const char *state = p->reports > 0 ? "ok" : "wait";
			len += snprintf(text + len, sizeof(text) - len, "%sP%d %s",
				len ? ", " : "", port + 1, state);
			if (len >= (int) sizeof(text))
				return text;
		}
	}

	if (len == 0)
		snprintf(text, sizeof(text), "%s", replug_needed ? "replug" : "none");
	return text;
}

#endif /* HW_RVL */
