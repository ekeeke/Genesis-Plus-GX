/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-FileCopyrightText: 2017 Alex Chadwick (wup-028-bslug) <https://github.com/Chadderz121/wup-028-bslug> */
/* SPDX-FileCopyrightText: FIX94 and the Nintendont contributors, used with permission <https://github.com/FIX94/Nintendont> */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The GameCube controller adapter for Wii U (WUP-028).  See input_wii.h.
 *
 * libinput_gcad.c is the driver, a state machine driven by IPC replies;
 * this is its environment on libogc. Both come from RiftWii
 * (github.com/KakarottoCake/Riftwii): the driver from runtime/rtgcad.c,
 * this file from wii/controllers/gcadapter.cpp and the /dev/usb/hid
 * opening in wii/hooks/padhook.cpp.
 *
 * INPUT_Init starts libogc's USB. On a v5 IOS (IOS 58, what the Homebrew
 * Channel runs) libogc then holds /dev/usb/hid's one device-change request,
 * and a second opener is refused there, so the driver goes through libogc:
 * its device list and change notices, USB_OpenDevice, and its control and
 * interrupt messages. On a v4 IOS (a d2x cIOS) libogc's list comes from
 * /dev/usb/oh0 without device ids, and the driver opens /dev/usb/hid
 * itself.
 */

#ifdef HAVE_GCADAPTER

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <gccore.h>
#include <ogc/cache.h>
#include <ogc/ipc.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include <ogc/usb.h>
#include "libinput_gcad.h"
#include "libinput_internal.h"

#define LOOK_MS 2000        /* a missing adapter is looked for this often */
#define HID_TIMEOUT_MS 1500 /* a USB module that does not answer turns the adapter off */
#define ERROR_LINES 12      /* a failure repeating every second is logged this often */
#define VERSION_BYTES 32
#define LIST_BYTES 0x180    /* v5's device list */

static int (*log_print)(const char *fmt, ...);
#define LOG(...) do { if (log_print) log_print(__VA_ARGS__); } while (0)

/* The driver's state and our own USB buffers, in MEM2 as IOS wants them
 * (with them in MEM1 a Wii U's IOS answered -4 to every request that has
 * one): libogc's IPC heap, taken once. */
static s32 heap = -1;
static gcad *state;
static u32 *version_out;

static s32 own_fd = -1;     /* our own /dev/usb/hid, -1 on libogc's */
static bool running;
static bool disabled;       /* INPUT_INIT_NO_GCADAPTER */
static bool failed;         /* would not start: not tried again until INPUT_Init */
static bool looked;
static u64 last_look;
static bool hid_stuck;      /* a request timed out: /dev/usb/hid is left alone */

static gcad_pad pads[GCAD_PORTS];
static bool present[GCAD_PORTS];

/* The driver's log events, queued in the IPC interrupt and printed by the
 * next update. */
typedef struct { u32 code, a; s32 b; } event_t;
#define EVENTS 32
static event_t events[EVENTS];
static volatile u32 event_head, event_tail, events_lost;
static u32 error_lines;

/* On v5, libogc's pending device-change request makes ours fail: the
 * driver's lists then come from libogc's (a change it sees answers the
 * waiting request), and its AttachFinish and Shutdown are answered here. */
static bool shared;
/* On libogc's handle (ogc) every device request goes through libogc too:
 * the ones libogc makes synchronously wait in `ops` for serve_shared,
 * and the interrupt transfers are libogc's. */
static bool ogc;
/* The adapter as libogc opened it (USB_OpenDevice), closed when the
 * driver stops so the next program can take it. */
static s32 ogc_dev_fd = -1;
static volatile bool list_wanted;   /* the driver's device list waits */
static volatile bool ogc_changed;   /* libogc has a list not yet given */
static bool notify_armed;
static u32 list_tag;
static void *list_out;
static s32 refused;                 /* what the refused request answered, logged once */

typedef struct { u32 tag; s32 result; } reply_t;
static reply_t replies[4];
static u32 reply_count;
typedef struct { u32 tag, cmd; s32 dev; } op_t;
static op_t ops[4];
static u32 op_count;

static char hid_path[] ATTRIBUTE_ALIGN(32) = "/dev/usb/hid";
/* libogc's other v5 USB device: when /dev/usb/hid answers no version
 * (seen on Wii U IOS 58 and d2x 251), libogc and the driver use this. */
static char ven_path[] ATTRIBUTE_ALIGN(32) = "/dev/usb/ven";

static u32 now_ticks(void) { return (u32)gettime(); }

void INPUT_GCAdapterSetLog(int (*print)(const char *fmt, ...)) { log_print = print; }

static bool buffers(void)
{
    if (state)
        return true;
    if (heap < 0)
        heap = iosCreateHeap((sizeof(gcad) + VERSION_BYTES + 64 + 31) & ~31u);
    if (heap < 0)
        return false;
    state = iosAlloc(heap, sizeof(gcad));
    version_out = iosAlloc(heap, VERSION_BYTES);
    return state && version_out;
}

static bool list_step(u32 tag)
{
    return (tag & 15u) == GCAD_TAG_CHANGE && state->change_step == GCAD_STEP_CHANGE;
}

static s32 on_reply(s32 result, void *tag)
{
    if (!state)
        return 0;
    u32 t = (u32)(uintptr_t)tag;
    if (result < 0 && !shared && state->version == 5 && !state->stopping && list_step(t)) {
        shared = true;
        refused = result;
        list_wanted = true;
        ogc_changed = true; /* libogc's current list answers it */
        return 0;
    }
    gcad_on_reply(state, t, result, now_ticks());
    return 0;
}

static s32 on_ogc_change(s32 result, void *data)
{
    (void)result;
    (void)data;
    notify_armed = false;
    ogc_changed = true;
    return 0;
}

/* Both run with interrupts off (the driver calls its environment from
 * IPC replies or from under _CPU_ISR_Disable). */
static void answer(u32 tag, s32 result)
{
    if (reply_count < sizeof(replies) / sizeof(replies[0]))
        replies[reply_count++] = (reply_t){ tag, result };
}

static bool defer(u32 tag, u32 cmd, s32 dev)
{
    if (op_count >= sizeof(ops) / sizeof(ops[0]))
        return false;
    ops[op_count++] = (op_t){ tag, cmd, dev };
    return true;
}

/* Gives the shared mode's answers to the driver and makes libogc's
 * synchronous calls. Runs with interrupts on: libogc's calls use IPC. */
static void serve_shared(void)
{
    static usb_device_entry list[32] ATTRIBUTE_ALIGN(32);
    op_t todo[sizeof(ops) / sizeof(ops[0])];
    reply_t done[sizeof(replies) / sizeof(replies[0])];
    u32 level, n_ops, n;
    u8 count = 0;
    bool have_list = false;

    if (!shared)
        return;
    if (refused) {
        LOG("GameCube adapter: the device list is libogc's (ours was refused, %d)\n", (int)refused);
        refused = 0;
    }
    _CPU_ISR_Disable(level);
    n_ops = op_count;
    memcpy(todo, ops, sizeof(todo));
    op_count = 0;
    _CPU_ISR_Restore(level);
    for (u32 i = 0; i < n_ops; i++) {
        s32 result;
        if (todo[i].cmd == GCAD_V5_SUSPEND_RESUME) {
            /* Resumes it and reads its descriptors, which v5 wants before
             * any transfer. */
            s32 fd = -1;
            result = USB_OpenDevice(todo[i].dev, GCAD_VID_PID >> 16, GCAD_VID_PID & 0xFFFF, &fd);
            if (result >= 0 && fd >= 0) {
                if (ogc_dev_fd >= 0 && ogc_dev_fd != fd)
                    USB_CloseDevice(&ogc_dev_fd);
                ogc_dev_fd = fd;
            }
        } else {
            result = USB_ClearHalt(todo[i].dev, 0); /* v5: cancels its transfers */
        }
        _CPU_ISR_Disable(level);
        answer(todo[i].tag, result);
        _CPU_ISR_Restore(level);
    }
    if (list_wanted && ogc_changed) {
        ogc_changed = false;
        have_list = true;
        if (USB_GetDeviceList(list, 32, USB_CLASS_HID, &count) < 0)
            count = 0;
    } else if (list_wanted && !notify_armed) {
        notify_armed = true; /* left set when refused: libogc has no HID list to follow */
        USB_DeviceChangeNotifyAsync(USB_CLASS_HID, on_ogc_change, NULL);
    }
    _CPU_ISR_Disable(level);
    u32 now = now_ticks();
    n = reply_count;
    memcpy(done, replies, sizeof(done));
    reply_count = 0;
    for (u32 i = 0; i < n; i++)
        gcad_on_reply(state, done[i].tag, done[i].result, now);
    if (have_list && list_wanted) {
        list_wanted = false;
        memset(list_out, 0, LIST_BYTES);
        memcpy(list_out, list, count * sizeof(usb_device_entry));
        DCFlushRange(list_out, LIST_BYTES); /* the driver invalidates it before reading */
        gcad_on_reply(state, list_tag, count, now);
    }
    _CPU_ISR_Restore(level);
}

static const char *step_name(u32 step)
{
    static const char *const names[] = { "list", "attach finish", "set protocol", "resume",
                                         "device info", "init", "rumble", "poll", "cancel" };
    return step < sizeof(names) / sizeof(names[0]) ? names[step] : "?";
}

static bool error_line(void)
{
    if (error_lines > ERROR_LINES)
        return false;
    if (++error_lines > ERROR_LINES) {
        LOG("GameCube adapter: more failures, no longer logged\n");
        return false;
    }
    return true;
}

static void drain(void)
{
    while (event_tail != event_head) {
        event_t e = events[event_tail % EVENTS];
        event_tail = event_tail + 1;
        switch (e.code) {
        case GCAD_EV_SUBMIT_FAIL:
            if (error_line())
                LOG("GameCube adapter: %s not sent (%d)\n", step_name(e.a >> 8), (int)e.b);
            break;
        case GCAD_EV_DEVICE:
            LOG("GameCube adapter: USB device %08x, %04x:%04x\n", (unsigned)e.a,
                (unsigned)e.b >> 16, (unsigned)e.b & 0xFFFF);
            break;
        case GCAD_EV_FOUND:
            LOG("GameCube adapter: found (device %08x)\n", (unsigned)e.a);
            break;
        case GCAD_EV_LOST:
            LOG("GameCube adapter: unplugged\n");
            break;
        case GCAD_EV_LINKED:
            LOG("GameCube adapter: reports flowing\n");
            break;
        case GCAD_EV_PORT:
            LOG("GameCube adapter: port %u %s\n", (unsigned)e.a + 1,
                (e.b & 0x30) ? "plugged in" : "empty");
            break;
        case GCAD_EV_FIRST_REPORT: {
            /* Each port's status byte: 0x10 a wired controller, 0x20 a
             * wireless one, 0x04 the power (grey) cable plugged in. */
            u32 s = (u32)e.b;
            LOG("GameCube adapter: first report, port status %02x %02x %02x %02x\n",
                (unsigned)(s >> 24), (unsigned)((s >> 16) & 0xFF), (unsigned)((s >> 8) & 0xFF),
                (unsigned)(s & 0xFF));
            break;
        }
        case GCAD_EV_ORIGIN: {
            u32 sticks = (u32)e.b, how = (e.a >> 8) & 0xFF;
            const char *what = how == GCAD_ORIGIN_PASSED ? "first reading off rest, skipped"
                             : how == GCAD_ORIGIN_MIDDLE ? "centre: middle (held at start)"
                             : how == GCAD_ORIGIN_RECAL  ? "centre: X+Y+Start"
                                                         : "centre: at rest";
            LOG("GameCube adapter: port %u %s: stick %u,%u C %u,%u L/R %u,%u\n",
                (unsigned)(e.a & 0xFF) + 1, what, (unsigned)(sticks >> 24),
                (unsigned)((sticks >> 16) & 0xFF), (unsigned)((sticks >> 8) & 0xFF),
                (unsigned)(sticks & 0xFF), (unsigned)((e.a >> 16) & 0xFF), (unsigned)(e.a >> 24));
            break;
        }
        case GCAD_EV_BAD_REPORT:
            if (error_line())
                LOG("GameCube adapter: odd report (first byte 0x%02x, result %d)\n",
                    (unsigned)e.a, (int)e.b);
            break;
        case GCAD_EV_REPLY:
            if (e.b < 0 && error_line())
                LOG("GameCube adapter: %s failed (%d)\n", step_name(e.a >> 8), (int)e.b);
            break;
        default:
            break;
        }
    }
    if (events_lost) {
        LOG("GameCube adapter: %u log line(s) dropped\n", (unsigned)events_lost);
        events_lost = 0;
    }
}

/* ---- Opening /dev/usb/hid ourselves (v4, or v5 without libogc's list) ---- */

static volatile bool async_done;
static volatile s32 async_result;

static s32 on_async(s32 result, void *data)
{
    (void)data;
    async_result = result;
    async_done = true;
    return 0;
}

/* Every call has a time limit: a Wii U's d2x cIOS never returned from
 * opening /dev/usb/hid with the adapter plugged in. */
static bool await_async(s32 submitted, s32 *result)
{
    if (submitted < 0) {
        *result = submitted;
        return true;
    }
    for (u32 i = 0; i < HID_TIMEOUT_MS && !async_done; i++)
        usleep(1000);
    if (!async_done) {
        hid_stuck = true;
        return false;
    }
    *result = async_result;
    return true;
}

static s32 ask_v5_version(s32 fd, bool *ok)
{
    s32 v5 = 0;
    memset(version_out, 0, VERSION_BYTES);
    DCFlushRange(version_out, VERSION_BYTES);
    async_done = false;
    *ok = await_async(IOS_IoctlAsync(fd, GCAD_V5_GET_VERSION, NULL, 0, version_out, VERSION_BYTES,
                                     on_async, NULL), &v5);
    DCInvalidateRange(version_out, VERSION_BYTES);
    return v5;
}

/* v5 is asked first, as libogc does: v4's GetVersion (request 6) is v5's
 * AttachFinish, after which a Wii U's IOS answered -4 to everything. v5
 * serves 16 handles picked by the open mode; libogc takes 0, so 0 then 1. */
static bool open_hid(s32 *fd, u32 *version)
{
    static const int handles[2] = { 0, 1 };
    bool ok;
    s32 ret;

    *fd = -1;
    if (hid_stuck) {
        LOG("GameCube adapter: /dev/usb/hid did not answer earlier; left alone\n");
        return false;
    }
    for (int i = 0; i < 2; i++) {
        async_done = false;
        if (!await_async(IOS_OpenAsync(hid_path, handles[i], on_async, NULL), &ret)) {
            LOG("GameCube adapter: /dev/usb/hid did not answer when opened\n");
            return false;
        }
        if (ret < 0) {
            LOG("GameCube adapter: /dev/usb/hid handle %d: open %d\n", handles[i], (int)ret);
            continue;
        }
        *fd = ret;
        s32 v5 = ask_v5_version(*fd, &ok);
        if (!ok) {
            LOG("GameCube adapter: /dev/usb/hid did not answer its v5 GetVersion\n");
            *fd = -1; /* left open: closing it could wait too */
            return false;
        }
        if (v5 == 0 && version_out[0] == GCAD_V5_VERSION) {
            *version = 5;
            return true;
        }
        IOS_Close(*fd);
        *fd = -1;
        s32 v4 = 0;
        async_done = false;
        if (!await_async(IOS_OpenAsync(hid_path, handles[i], on_async, NULL), &ret)) {
            LOG("GameCube adapter: /dev/usb/hid did not answer when opened again for v4\n");
            return false;
        }
        if (ret >= 0) {
            *fd = ret;
            async_done = false;
            if (!await_async(IOS_IoctlAsync(*fd, GCAD_V4_GET_VERSION, NULL, 0, NULL, 0, on_async, NULL), &v4)) {
                LOG("GameCube adapter: /dev/usb/hid did not answer its v4 GetVersion\n");
                *fd = -1;
                return false;
            }
            if (v4 == (s32)GCAD_V4_VERSION) {
                *version = 4;
                return true;
            }
            IOS_Close(*fd);
            *fd = -1;
        } else {
            v4 = ret;
        }
        LOG("GameCube adapter: /dev/usb/hid handle %d: v5 GetVersion %d (%08x), v4 GetVersion %d\n",
            handles[i], (int)v5, (unsigned)version_out[0], (int)v4);
    }
    /* libogc's way when /dev/usb/hid has no version: /dev/usb/ven, v5 only. */
    async_done = false;
    if (!await_async(IOS_OpenAsync(ven_path, 0, on_async, NULL), &ret)) {
        LOG("GameCube adapter: /dev/usb/ven did not answer when opened\n");
        return false;
    }
    if (ret < 0) {
        LOG("GameCube adapter: /dev/usb/ven: open %d\n", (int)ret);
        return false;
    }
    *fd = ret;
    s32 v5 = ask_v5_version(*fd, &ok);
    if (!ok) {
        LOG("GameCube adapter: /dev/usb/ven did not answer its v5 GetVersion\n");
        *fd = -1;
        return false;
    }
    if (v5 == 0 && version_out[0] == GCAD_V5_VERSION) {
        *version = 5;
        LOG("GameCube adapter: /dev/usb/hid has no version here; /dev/usb/ven v5 instead\n");
        return true;
    }
    IOS_Close(*fd);
    *fd = -1;
    LOG("GameCube adapter: /dev/usb/ven: v5 GetVersion %d (%08x)\n", (int)v5, (unsigned)version_out[0]);
    return false;
}

/* The adapter in libogc's HID list: its v5 device id in `dev`, or -1 on
 * v4 (whose list, from /dev/usb/oh0, has no ids). */
static bool listed(s32 *dev)
{
    static usb_device_entry list[32] ATTRIBUTE_ALIGN(32);
    u8 count = 0;

    *dev = -1;
    if (USB_GetDeviceList(list, 32, USB_CLASS_HID, &count) < 0)
        return false;
    for (u32 i = 0; i < count && i < 32; i++) {
        if (((u32)list[i].vid << 16 | list[i].pid) == GCAD_VID_PID) {
            if (list[i].device_id != 0)
                *dev = list[i].device_id;
            return true;
        }
    }
    return false;
}

static void reset_shared(void)
{
    shared = ogc = false;
    list_wanted = ogc_changed = false;
    notify_armed = false;
    refused = 0;
    reply_count = op_count = 0;
    error_lines = 0;
}

static bool start(s32 dev)
{
    u32 version = 5, level;

    if (!buffers()) {
        LOG("GameCube adapter: no IPC memory for its buffers\n");
        return false;
    }
    reset_shared();
    if (dev >= 0) {
        /* libogc's USB owns v5's handle: its device-change notices, its
         * list and its transfers stand in for ours. */
        own_fd = -1;
        shared = ogc = true;
        ogc_changed = true; /* libogc's current list answers the first request */
        notify_armed = USB_DeviceChangeNotifyAsync(USB_CLASS_HID, on_ogc_change, NULL) >= 0;
        LOG("GameCube adapter: through libogc's USB (IOS%d), device %08x\n", IOS_GetVersion(), (unsigned)dev);
    } else {
        if (!open_hid(&own_fd, &version))
            return false;
        LOG("GameCube adapter: /dev/usb/hid v%u on IOS%d, fd %d\n", (unsigned)version, IOS_GetVersion(), (int)own_fd);
    }
    memset(pads, 0, sizeof(pads));
    memset(present, 0, sizeof(present));
    running = true;
    _CPU_ISR_Disable(level);
    gcad_init(state, NULL, own_fd, version, TB_TIMER_CLOCK);
    if (dev >= 0)
        gcad_expect(state, dev);
    gcad_tick(state, now_ticks());
    _CPU_ISR_Restore(level);
    return true;
}

static void poll(void)
{
    u32 level;

    serve_shared();
    _CPU_ISR_Disable(level);
    u32 now = now_ticks();
    gcad_tick(state, now);
    for (u32 p = 0; p < GCAD_PORTS; p++)
        present[p] = gcad_port(state, p, &pads[p], now) != 0;
    _CPU_ISR_Restore(level);
    drain();
}

static void stop(void)
{
    u32 level;

    if (!running)
        return;
    _CPU_ISR_Disable(level);
    gcad_stop(state, now_ticks());
    _CPU_ISR_Restore(level);
    /* Cancels answer quickly; a second is plenty. */
    for (int i = 0; i < 100 && !gcad_idle(state); i++) {
        serve_shared();
        usleep(10000);
    }
    serve_shared();
    bool idle = gcad_idle(state);
    gcad_release(state);
    if (ogc_dev_fd >= 0) {
        USB_CloseDevice(&ogc_dev_fd);
        ogc_dev_fd = -1;
    }
    if (own_fd >= 0)
        IOS_Close(own_fd);
    own_fd = -1;
    running = false;
    memset(present, 0, sizeof(present));
    drain();
    LOG("GameCube adapter: closed%s\n", idle ? "" : " (a request was still in flight)");
}

void inpw_gcadapter_init(bool enabled)
{
    disabled = !enabled;
    failed = looked = false;
}

void inpw_gcadapter_shutdown(void)
{
    stop();
}

/* Looked for every LOOK_MS until it is plugged in, then driven every
 * frame; the driver itself follows it being unplugged and plugged again. */
void inpw_gcadapter_update(void)
{
    if (disabled)
        return;
    if (!running) {
        u64 now = gettime();
        s32 dev;
        if (failed || (looked && diff_msec(last_look, now) < LOOK_MS))
            return;
        looked = true;
        last_look = now;
        if (!listed(&dev))
            return;
        LOG("GameCube adapter: plugged in\n");
        if (!start(dev)) {
            failed = true;
            return;
        }
    }
    poll();
}

bool INPUT_GCAdapterConnected(void)
{
    return running && state->link == GCAD_LINK_POLL;
}

bool INPUT_GCAdapterPad(int port, input_gc_pad *out)
{
    if (port < 0 || port >= (int)GCAD_PORTS || !running || !present[port])
        return false;
    if (out) {
        const gcad_pad *p = &pads[port];
        out->buttons = p->buttons;
        out->stick_x = p->stick_x;
        out->stick_y = p->stick_y;
        out->substick_x = p->substick_x;
        out->substick_y = p->substick_y;
        out->trigger_l = p->trigger_l;
        out->trigger_r = p->trigger_r;
        out->wireless = p->wireless != 0;
    }
    return true;
}

void INPUT_GCAdapterRumble(int port, bool on)
{
    u32 level;

    if (port < 0 || port >= (int)GCAD_PORTS || !running)
        return;
    _CPU_ISR_Disable(level);
    gcad_rumble(state, port, on ? 1 : 0);
    _CPU_ISR_Restore(level);
}

/* ---- The driver's environment: libogc's asynchronous IPC, replies in
 * its interrupt ---- */

int32_t gcad_env_ioctl(gcad *g, uint32_t tag, uint32_t cmd, void *in, uint32_t in_len, void *out,
                       uint32_t out_len)
{
    if (g->version == 5 && cmd == GCAD_V5_GET_DEVICE_CHANGE) {
        list_tag = tag;
        list_out = out;
    }
    if (shared) {
        switch (cmd) {
        case GCAD_V5_GET_DEVICE_CHANGE:
            list_wanted = true;
            return 0;
        case GCAD_V5_ATTACH_FINISH: /* libogc sends its own */
            answer(tag, 0);
            return 0;
        case GCAD_V5_SHUTDOWN: /* it would cancel libogc's */
            if (list_wanted) {
                list_wanted = false;
                answer(list_tag, 0);
            }
            answer(tag, 0);
            return 0;
        default:
            break;
        }
    }
    if (ogc) {
        switch (cmd) {
        /* libogc attached it (its AttachFinish) and releases nothing;
         * USB_OpenDevice reads the descriptors with the resume. */
        case GCAD_V5_ATTACH:
        case GCAD_V5_RELEASE:
        case GCAD_V5_GET_DEVICE_INFO:
            answer(tag, 0);
            return 0;
        case GCAD_V5_SUSPEND_RESUME:
        case GCAD_V5_CANCEL_ENDPOINT:
            return defer(tag, cmd, (s32)gcad_get32(in)) ? 0 : -22;
        default:
            return -4;
        }
    }
    return IOS_IoctlAsync(g->fd, (s32)cmd, in, (s32)in_len, out, (s32)out_len, on_reply,
                          (void *)(uintptr_t)tag);
}

int32_t gcad_env_ioctlv(gcad *g, uint32_t tag, uint32_t cmd, void *msg, uint32_t msg_len, void *data,
                        uint32_t data_len, int data_in)
{
    if (ogc) {
        /* The v5 transfer as libogc sends it on its handle: the device and
         * the direction from the driver's message. */
        const uint8_t *m = msg;
        s32 dev = (s32)gcad_get32(m);
        void *t = (void *)(uintptr_t)tag;
        if (cmd == GCAD_V5_CONTROL) {
            /* HID SET_PROTOCOL, which some third-party adapters need before
             * they report their controllers. No buffer with no length:
             * libogc refuses one of length 0. */
            return USB_WriteCtrlMsgAsync(dev, m[8], m[9], (u16)((m[10] << 8) | m[11]),
                                         (u16)((m[12] << 8) | m[13]), (u16)data_len,
                                         data_len ? data : NULL, on_reply, t);
        }
        if (cmd != GCAD_V5_INTERRUPT)
            return -4;
        return gcad_get32(m + 8) != 0
                   ? USB_WriteIntrMsgAsync(dev, 0x02, (u16)data_len, data, on_reply, t)
                   : USB_ReadIntrMsgAsync(dev, 0x81, (u16)data_len, data, on_reply, t);
    }
    /* Each slot its own vectors, kept until the reply: libogc turns them
     * into physical addresses in place. The slots using ioctlv are 2-4. */
    u32 slot = (tag & 15u) - GCAD_TAG_SETUP;
    if (slot > 2)
        return -4;
    ioctlv *vec = (ioctlv *)(g->env + slot * 16u);
    vec[0].data = msg;
    vec[0].len = msg_len;
    vec[1].data = data;
    vec[1].len = data_len;
    return IOS_IoctlvAsync(g->fd, (s32)cmd, data_in ? 2 : 1, data_in ? 0 : 1, vec, on_reply,
                           (void *)(uintptr_t)tag);
}

uint32_t gcad_env_address(gcad *g, void *p)
{
    (void)g;
    return (uint32_t)(uintptr_t)p; /* v4 takes the virtual address */
}

void gcad_env_flush(gcad *g, void *p, uint32_t len)
{
    (void)g;
    DCFlushRange(p, len);
}

void gcad_env_invalidate(gcad *g, void *p, uint32_t len)
{
    (void)g;
    DCInvalidateRange(p, len);
}

void gcad_env_event(gcad *g, uint32_t code, uint32_t a, int32_t b)
{
    (void)g;
    if (event_head - event_tail >= EVENTS) {
        events_lost = events_lost + 1;
        return;
    }
    events[event_head % EVENTS] = (event_t){ code, a, b };
    event_head = event_head + 1;
}

#endif /* HAVE_GCADAPTER */
