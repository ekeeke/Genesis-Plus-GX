/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-FileCopyrightText: 2017 Alex Chadwick (wup-028-bslug) <https://github.com/Chadderz121/wup-028-bslug> */
/* SPDX-FileCopyrightText: FIX94 and the Nintendont contributors, used with permission <https://github.com/FIX94/Nintendont> */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The WUP-028 driver (libinput_gcad.h), from RiftWii's runtime/rtgcad.c. */

#ifdef HAVE_GCADAPTER

#include "libinput_gcad.h"

#define BIT(tag) (1u << (tag))
#define TAG_OF(value) ((value) & 15u)
#define GEN_OF(value) ((value) >> 4)

uint32_t gcad_get32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void gcad_put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void zero(void* p, uint32_t n) {
    volatile uint8_t* b = (volatile uint8_t*)p;  /* volatile: not turned into a memset call */
    uint32_t i;
    for (i = 0; i < n; ++i) b[i] = 0;
}

static uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }

static uint32_t ms(const gcad* g, uint32_t n) { return n * g->ticks_per_ms; }

int32_t gcad_find_v4(const uint8_t* list, uint32_t list_bytes) {
    /* Entries: [total size][device id][descriptors...], sizes counted in
     * bytes and padded to 4; 0xFFFFFFFF ends the list. The device
     * descriptor's third word holds VID and PID (wiibrew). */
    uint32_t at = 0;
    while (at + 20 <= list_bytes) {
        const uint32_t size = gcad_get32(list + at);
        if (size == 0xFFFFFFFFu || size < 20 || (size & 3) != 0 || size > list_bytes - at) break;
        if (gcad_get32(list + at + 16) == GCAD_VID_PID) return (int32_t)gcad_get32(list + at + 4);
        at += size;
    }
    return -1;
}

int32_t gcad_find_v5(const uint8_t* list, uint32_t count) {
    /* Entries of 12 bytes: device id, VID, PID, ... (wiibrew). */
    uint32_t i;
    if (count > 32) count = 32;
    for (i = 0; i < count; ++i) {
        const uint8_t* e = list + i * 12;
        if (gcad_get32(e + 4) == GCAD_VID_PID) return (int32_t)gcad_get32(e);
    }
    return -1;
}

static int8_t relative(uint8_t raw, uint8_t origin) {
    const int32_t v = (int32_t)raw - (int32_t)origin;
    return (int8_t)(v < -128 ? -128 : v > 127 ? 127 : v);
}

static uint8_t above(uint8_t raw, uint8_t origin) { return raw > origin ? (uint8_t)(raw - origin) : 0; }

int gcad_decode(const uint8_t raw[9], const uint8_t origin[6], gcad_pad* out) {
    /* Status: bit 4 wired, bit 5 wireless (Dolphin's GCAdapter.cpp). */
    const uint8_t status = raw[0], b1 = raw[1], b2 = raw[2];
    uint16_t buttons = 0;
    if ((status & 0x30) == 0) return 0;
    if (b1 & 0x01) buttons |= GCAD_PAD_A;
    if (b1 & 0x02) buttons |= GCAD_PAD_B;
    if (b1 & 0x04) buttons |= GCAD_PAD_X;
    if (b1 & 0x08) buttons |= GCAD_PAD_Y;
    if (b1 & 0x10) buttons |= GCAD_PAD_LEFT;
    if (b1 & 0x20) buttons |= GCAD_PAD_RIGHT;
    if (b1 & 0x40) buttons |= GCAD_PAD_DOWN;
    if (b1 & 0x80) buttons |= GCAD_PAD_UP;
    if (b2 & 0x01) buttons |= GCAD_PAD_START;
    if (b2 & 0x02) buttons |= GCAD_PAD_Z;
    if (b2 & 0x04) buttons |= GCAD_PAD_R;
    if (b2 & 0x08) buttons |= GCAD_PAD_L;
    out->buttons = buttons;
    /* The SDK reports sticks and triggers against the origin the pad
     * sent when it was plugged in; the adapter sends raw values
     * (sticks centred near 128). */
    out->stick_x = relative(raw[3], origin[0]);
    out->stick_y = relative(raw[4], origin[1]);
    out->substick_x = relative(raw[5], origin[2]);
    out->substick_y = relative(raw[6], origin[3]);
    out->trigger_l = above(raw[7], origin[4]);
    out->trigger_r = above(raw[8], origin[5]);
    out->wireless = (status & 0x10) == 0;
    return 1;
}

int gcad_pad_moved(const gcad_pad* before, const gcad_pad* now) {
    /* Newer SDKs take 10 steps for a GameCube controller's sticks and
     * triggers, older ones a change of 9 in the value squared; 10 is the
     * newer rule, and the same for every game. One loop over the six
     * axes keeps the in-game blob small. */
    const int32_t was[6] = {before->stick_x,    before->stick_y,   before->substick_x,
                            before->substick_y, before->trigger_l, before->trigger_r};
    const int32_t is[6] = {now->stick_x, now->stick_y, now->substick_x, now->substick_y, now->trigger_l, now->trigger_r};
    uint32_t axis;
    if (now->buttons != before->buttons) return 1;
    for (axis = 0; axis < 6; ++axis) {
        const int32_t distance = is[axis] > was[axis] ? is[axis] - was[axis] : was[axis] - is[axis];
        if (distance >= GCAD_ACTIVE_STEP) return 1;
    }
    return 0;
}

static uint32_t tag_value(gcad* g, uint32_t tag) { return tag | (g->generation << 4); }

static void failed(gcad* g, uint32_t step, int32_t result) {
    ++g->errors;
    g->last_error = result;
    g->last_error_step = step;
}

/* Submissions: set the slot busy, undo it when IOS refused at once. */
static int ioctl_(gcad* g, uint32_t tag, uint32_t step, uint32_t cmd, void* in, uint32_t in_len, void* out,
                  uint32_t out_len) {
    int32_t r;
    g->busy |= BIT(tag);
    r = gcad_env_ioctl(g, tag_value(g, tag), cmd, in, in_len, out, out_len);
    if (r < 0) {
        g->busy &= ~BIT(tag);
        failed(g, step, r);
        gcad_env_event(g, GCAD_EV_SUBMIT_FAIL, tag | (step << 8), r);
        return 0;
    }
    return 1;
}

static int ioctlv_(gcad* g, uint32_t tag, uint32_t step, uint32_t cmd, void* msg, void* data, uint32_t data_len,
                   int data_in) {
    int32_t r;
    g->busy |= BIT(tag);
    r = gcad_env_ioctlv(g, tag_value(g, tag), cmd, msg, 0x40, data, data_len, data_in);
    if (r < 0) {
        g->busy &= ~BIT(tag);
        failed(g, step, r);
        gcad_env_event(g, GCAD_EV_SUBMIT_FAIL, tag | (step << 8), r);
        return 0;
    }
    return 1;
}

static void set_stale(gcad* g) {
    uint32_t i;
    g->stale = 1;
    for (i = 0; i < GCAD_PORTS; ++i) {
        g->raw[i][0] = 0;
        g->origin_valid[i] = 0;
        g->recal_active[i] = 0;
        g->rumble_want[i] = 0;
    }
}

/* Whether one of a report's six analog bytes (stick x, y, substick x, y,
 * L, R) reads as a pad at rest: a stick near the middle, a trigger let
 * go. */
static int axis_at_rest(uint32_t axis, uint8_t value) {
    if (axis >= 4) return value <= GCAD_ORIGIN_TRIGGER_MAX;
    return value >= 128 - GCAD_ORIGIN_SPREAD && value <= 128 + GCAD_ORIGIN_SPREAD;
}

static int pad_at_rest(const uint8_t analog[6]) {
    uint32_t axis;
    for (axis = 0; axis < 6; ++axis) {
        if (!axis_at_rest(axis, analog[axis])) return 0;
    }
    return 1;
}

/* Counts each axis's readings at rest in a row, each within
 * GCAD_ORIGIN_STEADY_STEP of the one before; whether all six have reached
 * GCAD_ORIGIN_STEADY_READS. */
static int count_steady(gcad* g, uint32_t port, const uint8_t analog[6]) {
    uint32_t axis;
    int all = 1;
    for (axis = 0; axis < 6; ++axis) {
        uint8_t* reads = &g->steady_reads[port][axis];
        const int d = (int)analog[axis] - (int)g->steady[port][axis];
        if (!axis_at_rest(axis, analog[axis])) {
            *reads = 0;
        } else if (*reads != 0 && d <= GCAD_ORIGIN_STEADY_STEP && d >= -GCAD_ORIGIN_STEADY_STEP) {
            if (*reads < GCAD_ORIGIN_STEADY_READS) ++*reads;
        } else {
            *reads = 1;
        }
        g->steady[port][axis] = analog[axis];
        if (*reads < GCAD_ORIGIN_STEADY_READS) all = 0;
    }
    return all;
}

static void origin_event(gcad* g, uint32_t port, uint32_t how, const uint8_t analog[6]) {
    const uint32_t triggers = (uint32_t)analog[4] << 16 | (uint32_t)analog[5] << 24;
    const uint32_t sticks =
        (uint32_t)analog[0] << 24 | (uint32_t)analog[1] << 16 | (uint32_t)analog[2] << 8 | analog[3];
    gcad_env_event(g, GCAD_EV_ORIGIN, port | how << 8 | triggers, (int32_t)sticks);
}

/* `how` is GCAD_ORIGIN_*: with GCAD_ORIGIN_MIDDLE an axis not at rest
 * takes the middle (a trigger 0) instead of its reading. */
static void set_origin(gcad* g, uint32_t port, const uint8_t analog[6], uint32_t how) {
    uint32_t axis;
    uint8_t* origin = g->origin[port];
    for (axis = 0; axis < 6; ++axis) {
        const uint8_t middle = axis < 4 ? 128 : 0;
        /* At the end of the wait an axis that never settled at rest
         * takes the middle, not whatever it reads at that moment. */
        const int off = how == GCAD_ORIGIN_MIDDLE && g->steady_reads[port][axis] < GCAD_ORIGIN_STEADY_READS;
        origin[axis] = off ? middle : analog[axis];
    }
    g->origin_valid[port] = 1;
    origin_event(g, port, how, origin);
}

static void submit_change(gcad* g, uint32_t now) {
    g->change_step = GCAD_STEP_CHANGE;
    g->change_time = now;
    if (g->version == 4) {
        if (!ioctl_(g, GCAD_TAG_CHANGE, GCAD_STEP_CHANGE, GCAD_V4_GET_DEVICE_CHANGE, 0, 0, g->change, 0x600)) {
            g->change_failed = 1;
        }
    } else if (!ioctl_(g, GCAD_TAG_CHANGE, GCAD_STEP_CHANGE, GCAD_V5_GET_DEVICE_CHANGE, 0, 0, g->change, 0x180)) {
        g->change_failed = 1;
    }
}

static void submit_finish(gcad* g, uint32_t now) {
    g->finish_pending = 0;
    g->change_step = GCAD_STEP_FINISH;
    if (!ioctl_(g, GCAD_TAG_CHANGE, GCAD_STEP_FINISH, GCAD_V5_ATTACH_FINISH, 0, 0, 0, 0)) {
        /* Nothing more to do on this list; list again. */
        submit_change(g, now);
    }
}

/* v5 setup: resume, then the device's parameters. No Attach: IOS 58
 * lists the adapter as attached already, and neither libogc's
 * USB_OpenDevice nor Nintendont's HID driver sends one. */
static void submit_setup(gcad* g, uint32_t step) {
    zero(g->setup_in, sizeof(g->setup_in));
    gcad_put32(g->setup_in, (uint32_t)g->dev_id);
    g->setup_step = (uint8_t)step;
    if (step == GCAD_STEP_RESUME) {
        gcad_put32(g->setup_in + 8, 1);  /* byte 11: resume */
        ioctl_(g, GCAD_TAG_SETUP, step, GCAD_V5_SUSPEND_RESUME, g->setup_in, 0x20, 0, 0);
    } else {
        zero(g->setup_out, sizeof(g->setup_out));
        ioctl_(g, GCAD_TAG_SETUP, step, GCAD_V5_GET_DEVICE_INFO, g->setup_in, 0x20, g->setup_out, 0x60);
    }
}

/* The v4 request block: 16 bytes of padding, device, then the fields. */
static void v4_request(gcad* g, uint8_t* msg, uint32_t endpoint, void* data, uint32_t len) {
    zero(msg, 0x40);
    gcad_put32(msg + 16, (uint32_t)g->dev_id);
    gcad_put32(msg + 20, endpoint);
    gcad_put32(msg + 24, len);
    gcad_put32(msg + 28, gcad_env_address(g, data));
}

static void v5_message(gcad* g, uint8_t* msg, uint32_t out) {
    zero(msg, 0x40);
    gcad_put32(msg, (uint32_t)g->dev_id);
    gcad_put32(msg + 8, out);  /* non-zero: the interrupt OUT endpoint */
}

static int send_out(gcad* g, uint32_t step, uint32_t len) {
    gcad_env_flush(g, g->out_data, sizeof(g->out_data));
    if (g->version == 4) {
        v4_request(g, g->out_msg, 0x02, g->out_data, len);
        return ioctl_(g, GCAD_TAG_OUT, step, GCAD_V4_INTERRUPT_OUT, g->out_msg, 32, 0, 0);
    }
    v5_message(g, g->out_msg, 1);
    return ioctlv_(g, GCAD_TAG_OUT, step, GCAD_V5_INTERRUPT, g->out_msg, g->out_data, len, 1);
}

static void link_failed(gcad* g, uint32_t now, uint32_t link) {
    g->link = (uint8_t)link;
    g->link_time = now;
    set_stale(g);
}

static void start_init(gcad* g, uint32_t now) {
    g->link = GCAD_LINK_INIT;
    zero(g->out_data, sizeof(g->out_data));
    g->out_data[0] = 0x13;
    if (!send_out(g, GCAD_STEP_INIT, 1)) link_failed(g, now, GCAD_LINK_FAILED);
}

/* HID SET_PROTOCOL(report) before the init command, as Nintendont and
 * Dolphin send it: some clones need it, and Mayflash adapters refuse it,
 * which is fine: the answer is not looked at. It goes on the SETUP slot;
 * init follows its reply, or GCAD_CTRL_MS without one. */
static void start_ctrl(gcad* g, uint32_t now) {
    int sent;
    g->link = GCAD_LINK_CTRL;
    g->link_time = now;
    g->setup_step = GCAD_STEP_CTRL;
    if (g->version == 4) {
        zero(g->setup_in, sizeof(g->setup_in));
        gcad_put32(g->setup_in + 16, (uint32_t)g->dev_id);
        g->setup_in[20] = 0x21;  /* class request to the interface */
        g->setup_in[21] = 0x0B;  /* SET_PROTOCOL */
        g->setup_in[23] = 0x01;  /* report protocol */
        sent = ioctl_(g, GCAD_TAG_SETUP, GCAD_STEP_CTRL, GCAD_V4_CONTROL, g->setup_in, 32, 0, 0);
    } else {
        zero(g->setup_out, sizeof(g->setup_out));
        gcad_put32(g->setup_out, (uint32_t)g->dev_id);
        g->setup_out[8] = 0x21;
        g->setup_out[9] = 0x0B;
        g->setup_out[11] = 0x01;
        sent = ioctlv_(g, GCAD_TAG_SETUP, GCAD_STEP_CTRL, GCAD_V5_CONTROL, g->setup_out, g->out_data, 0, 1);
    }
    if (!sent) start_init(g, now);
}

/* Starts (or restarts) the link to g->dev_id. */
static void start_link(gcad* g, uint32_t now) {
    if (g->busy & (BIT(GCAD_TAG_SETUP) | BIT(GCAD_TAG_IN) | BIT(GCAD_TAG_OUT))) {
        /* The old link's requests are still coming back: later. */
        link_failed(g, now, GCAD_LINK_FAILED);
        return;
    }
    ++g->generation;
    if (g->version == 5) {
        g->link = GCAD_LINK_SETUP;
        submit_setup(g, GCAD_STEP_RESUME);
        if (!(g->busy & BIT(GCAD_TAG_SETUP))) link_failed(g, now, GCAD_LINK_FAILED);
    } else {
        start_ctrl(g, now);
    }
}

static void drop_link(gcad* g) {
    gcad_env_event(g, GCAD_EV_LOST, (uint32_t)g->dev_id, 0);
    g->dev_id = -1;
    g->link = GCAD_LINK_NONE;
    ++g->generation;  /* what the old link still has in flight is ignored */
    set_stale(g);
}

static void submit_poll(gcad* g) {
    g->in_data[0] = 0;
    gcad_env_flush(g, g->in_data, sizeof(g->in_data));
    if (g->version == 4) {
        v4_request(g, g->in_msg, 0x81, g->in_data, GCAD_REPORT_BYTES);
        ioctl_(g, GCAD_TAG_IN, GCAD_STEP_POLL, GCAD_V4_INTERRUPT_IN, g->in_msg, 32, 0, 0);
    } else {
        v5_message(g, g->in_msg, 0);
        ioctlv_(g, GCAD_TAG_IN, GCAD_STEP_POLL, GCAD_V5_INTERRUPT, g->in_msg, g->in_data, GCAD_REPORT_BYTES, 0);
    }
}

static void pump_rumble(gcad* g) {
    uint32_t i, differ = 0;
    if (g->busy & BIT(GCAD_TAG_OUT)) return;
    for (i = 0; i < GCAD_PORTS; ++i) differ |= (uint32_t)(g->rumble_want[i] != g->rumble_sent[i]);
    if (!differ) return;
    zero(g->out_data, sizeof(g->out_data));
    g->out_data[0] = 0x11;
    for (i = 0; i < GCAD_PORTS; ++i) {
        g->rumble_flight[i] = g->rumble_want[i];
        g->out_data[1 + i] = g->rumble_want[i];
    }
    send_out(g, GCAD_STEP_RUMBLE, 5);
}

static void take_report(gcad* g, uint32_t now) {
    uint32_t i, j;
    if (g->reports == 0) {
        uint32_t ports = 0;
        for (i = 0; i < GCAD_PORTS; ++i) ports = (ports << 8) | g->in_data[1 + i * 9];
        gcad_env_event(g, GCAD_EV_FIRST_REPORT, (uint32_t)g->dev_id, (int32_t)ports);
    }
    for (i = 0; i < GCAD_PORTS; ++i) {
        const uint8_t* src = g->in_data + 1 + i * 9;
        const uint8_t was = g->raw[i][0] & 0x30, is = src[0] & 0x30;
        for (j = 0; j < 9; ++j) g->raw[i][j] = src[j];
        if (was != is) gcad_env_event(g, GCAD_EV_PORT, i, src[0]);
        if (!is) {
            g->origin_valid[i] = 0;
            g->recal_active[i] = 0;
            /* The game may never send a stop to an unplugged port. */
            g->rumble_want[i] = 0;
            continue;
        }
        /* The origin comes only from the pad at rest, and steady: a
         * reading far off taken as the origin pinned the stick to a
         * corner until the adapter was replugged (seen at RiftWii's
         * start on a Wii U, 2610-541). For a moment after it appears a
         * pad can also send readings that jump about while sitting
         * still (233,147 then 99,83 on a Wii U, 2610-603), so one at
         * rest counts only after GCAD_ORIGIN_STEADY_READS in a row,
         * each within GCAD_ORIGIN_STEADY_STEP of the last; the port
         * stays empty while it waits. A pad held off the middle for
         * GCAD_ORIGIN_WAIT_MS gets the middle for those axes, where its
         * own origin would make the stick drift once let go. */
        if (!g->origin_valid[i]) {
            const int at_rest = pad_at_rest(src + 3);
            /* The wait starts as the pad appears: every way an origin is
             * dropped (the port emptied, the link lost) also empties the
             * port, so the next report shows it appearing again. */
            if (!was) {
                g->origin_since[i] = now;
                for (j = 0; j < 6; ++j) g->steady_reads[i][j] = 0;
                /* Once per wait, so a log shows what was passed over. */
                if (!at_rest) origin_event(g, i, GCAD_ORIGIN_PASSED, src + 3);
            }
            if (count_steady(g, i, src + 3)) {
                set_origin(g, i, src + 3, GCAD_ORIGIN_RESTING);
            } else if (elapsed(now, g->origin_since[i]) >= ms(g, GCAD_ORIGIN_WAIT_MS)) {
                set_origin(g, i, src + 3, GCAD_ORIGIN_MIDDLE);
            }
        }
        /* X+Y+Start held: a new origin, as the pad itself does. */
        if ((src[1] & 0x0C) == 0x0C && (src[2] & 0x01)) {
            if (!g->recal_active[i]) {
                g->recal_active[i] = 1;
                g->recal_since[i] = now;
            } else if (g->recal_active[i] == 1 && elapsed(now, g->recal_since[i]) >= ms(g, GCAD_RECAL_MS)) {
                set_origin(g, i, src + 3, GCAD_ORIGIN_RECAL);
                g->recal_active[i] = 2;  /* once per hold */
            }
        } else {
            g->recal_active[i] = 0;
        }
    }
    g->data_time = now;
    g->stale = 0;
    ++g->reports;
}

void gcad_init(gcad* g, void* env_ctx, int32_t fd, uint32_t version, uint32_t ticks_per_ms) {
    uint32_t i;
    zero(g, sizeof(*g));
    g->env_ctx = env_ctx;
    g->fd = fd;
    g->version = version;
    g->ticks_per_ms = ticks_per_ms ? ticks_per_ms : 1;
    g->dev_id = -1;
    g->stale = 1;
    for (i = 0; i < GCAD_PORTS; ++i) g->rumble_sent[i] = 0xFF;  /* unknown: the first poll turns rumble off */
}

void gcad_expect(gcad* g, int32_t dev_id) { g->dev_id = dev_id; }

void gcad_tick(gcad* g, uint32_t now) {
    if (g->stopping || (g->version != 4 && g->version != 5)) return;
    if (!g->started) {
        g->started = 1;
        g->data_time = now;
        submit_change(g, now);
        if (g->dev_id >= 0) {
            gcad_env_event(g, GCAD_EV_FOUND, (uint32_t)g->dev_id, 0);
            start_link(g, now);
        }
        return;
    }
    if (g->change_failed && !(g->busy & BIT(GCAD_TAG_CHANGE)) &&
        elapsed(now, g->change_time) >= ms(g, GCAD_RESCAN_MS)) {
        g->change_failed = 0;
        submit_change(g, now);
    }
    if (g->dev_id >= 0 && (g->link == GCAD_LINK_FAILED || g->link == GCAD_LINK_BUSY) &&
        !(g->busy & (BIT(GCAD_TAG_SETUP) | BIT(GCAD_TAG_IN) | BIT(GCAD_TAG_OUT))) &&
        elapsed(now, g->link_time) >= ms(g, g->link == GCAD_LINK_BUSY ? GCAD_RESCAN_MS : GCAD_RELINK_MS)) {
        start_link(g, now);
    }
    if (g->link == GCAD_LINK_SETTLE && elapsed(now, g->link_time) >= ms(g, GCAD_SETTLE_MS)) {
        g->finish_pending = 1;  /* sent once the setup is done, as fakemote does */
        start_link(g, now);
        if (!(g->busy & BIT(GCAD_TAG_SETUP)) && g->finish_pending) submit_finish(g, now);
    }
    if (g->link == GCAD_LINK_CTRL && elapsed(now, g->link_time) >= ms(g, GCAD_CTRL_MS)) start_init(g, now);
    if (!g->stale && elapsed(now, g->data_time) >= ms(g, GCAD_TIMEOUT_MS)) set_stale(g);
}

static void on_change(gcad* g, int32_t result, uint32_t now) {
    int32_t found;
    int start = 0;
    uint32_t i;
    if (g->change_step == GCAD_STEP_FINISH) {
        submit_change(g, now);
        return;
    }
    if (result < 0) {
        failed(g, GCAD_STEP_CHANGE, result);
        g->change_failed = 1;
        g->change_time = now;
        return;
    }
    ++g->changes;
    gcad_env_invalidate(g, g->change, sizeof(g->change));
    if (g->version == 4) {
        uint32_t at = 0;
        g->listed = 0;
        while (at + 20 <= 0x600) {
            const uint32_t size = gcad_get32(g->change + at);
            if (size == 0xFFFFFFFFu || size < 20 || (size & 3) != 0 || size > 0x600 - at) break;
            gcad_env_event(g, GCAD_EV_DEVICE, gcad_get32(g->change + at + 4), (int32_t)gcad_get32(g->change + at + 16));
            ++g->listed;
            at += size;
        }
        found = gcad_find_v4(g->change, 0x600);
    } else {
        g->listed = (uint32_t)result > 32 ? 32 : (uint32_t)result;
        for (i = 0; i < g->listed; ++i) {
            gcad_env_event(g, GCAD_EV_DEVICE, gcad_get32(g->change + i * 12),
                           (int32_t)gcad_get32(g->change + i * 12 + 4));
        }
        found = gcad_find_v5(g->change, g->listed);
    }
    if (g->dev_id >= 0 && found != g->dev_id) drop_link(g);
    if (found >= 0 && g->dev_id < 0) {
        g->dev_id = found;
        gcad_env_event(g, GCAD_EV_FOUND, (uint32_t)found, 0);
        start = 1;
    } else if (found >= 0 && (g->link == GCAD_LINK_BUSY || g->link == GCAD_LINK_FAILED)) {
        start = 1;  /* a change may mean the owner let it go */
    }
    if (g->version == 5) {
        if (start) {
            /* Given a moment to finish attaching before it is used, as
             * Nintendont waits after a change; gcad_tick goes on. */
            g->link = GCAD_LINK_SETTLE;
            g->link_time = now;
        } else {
            submit_finish(g, now);
        }
    } else {
        if (start) start_link(g, now);
        submit_change(g, now);
    }
}

static void on_setup(gcad* g, int32_t result, int current, uint32_t now) {
    if (g->setup_step == GCAD_STEP_CTRL) {
        g->ctrl_result = result;
        if (current && g->link == GCAD_LINK_CTRL && !g->stopping) start_init(g, now);
        return;
    }
    if (current && g->link == GCAD_LINK_SETUP) {
        if (g->setup_step == GCAD_STEP_RESUME) {
            /* -4 when it is already resumed (the state must change). */
            g->resume_result = result;
            submit_setup(g, GCAD_STEP_INFO);
        } else {
            g->info_result = result;
            if (result >= 0) {
                gcad_env_invalidate(g, g->setup_out, sizeof(g->setup_out));
                g->link = GCAD_LINK_INIT;
            } else {
                failed(g, GCAD_STEP_INFO, result);
                link_failed(g, now, GCAD_LINK_BUSY);
            }
        }
        if (g->link == GCAD_LINK_SETUP && !(g->busy & BIT(GCAD_TAG_SETUP))) link_failed(g, now, GCAD_LINK_FAILED);
    }
    if (g->busy & BIT(GCAD_TAG_SETUP)) return;  /* the chain goes on */
    if (g->finish_pending && !g->stopping) submit_finish(g, now);
    if (current && g->link == GCAD_LINK_INIT && !g->stopping) start_ctrl(g, now);
}

static void on_out(gcad* g, int32_t result, int current, uint32_t now) {
    uint32_t i;
    if (!current || g->stopping) return;
    if (g->link == GCAD_LINK_INIT) {
        g->init_result = result;
        if (result < 0) {
            failed(g, GCAD_STEP_INIT, result);
            link_failed(g, now, GCAD_LINK_FAILED);
            return;
        }
        g->link = GCAD_LINK_POLL;
        ++g->links;
        g->data_time = now;
        gcad_env_event(g, GCAD_EV_LINKED, (uint32_t)g->dev_id, 0);
        for (i = 0; i < GCAD_PORTS; ++i) g->rumble_sent[i] = 0xFF;
        submit_poll(g);
        if (!(g->busy & BIT(GCAD_TAG_IN))) link_failed(g, now, GCAD_LINK_FAILED);
        return;
    }
    if (result < 0) {
        failed(g, GCAD_STEP_RUMBLE, result);
        return;
    }
    for (i = 0; i < GCAD_PORTS; ++i) g->rumble_sent[i] = g->rumble_flight[i];
}

static void on_in(gcad* g, int32_t result, int current, uint32_t now) {
    if (!current || g->stopping || g->link != GCAD_LINK_POLL) return;
    if (result < 0) {
        /* -7022 when unplugged; the device list follows. */
        failed(g, GCAD_STEP_POLL, result);
        link_failed(g, now, GCAD_LINK_FAILED);
        return;
    }
    gcad_env_invalidate(g, g->in_data, sizeof(g->in_data));
    if (g->in_data[0] == 0x21 && (result == 0 || (uint32_t)result >= GCAD_REPORT_BYTES)) {
        take_report(g, now);
    } else {
        ++g->bad_reports;
        gcad_env_event(g, GCAD_EV_BAD_REPORT, g->in_data[0], result);
    }
    pump_rumble(g);  /* between polls, never alongside another command */
    submit_poll(g);
    if (!(g->busy & BIT(GCAD_TAG_IN))) link_failed(g, now, GCAD_LINK_FAILED);
}

static void stop_next(gcad* g);

void gcad_on_reply(gcad* g, uint32_t tag_value_, int32_t result, uint32_t now) {
    const uint32_t tag = TAG_OF(tag_value_);
    const int current = GEN_OF(tag_value_) == (g->generation & 0x0FFFFFFFu);
    uint32_t step = 0;
    if (tag == 0 || tag >= GCAD_TAGS) return;
    g->busy &= ~BIT(tag);
    if (tag == GCAD_TAG_CHANGE) step = g->change_step;
    else if (tag == GCAD_TAG_SETUP) step = g->setup_step;
    else if (tag == GCAD_TAG_OUT) step = g->link == GCAD_LINK_INIT ? GCAD_STEP_INIT : GCAD_STEP_RUMBLE;
    else if (tag == GCAD_TAG_IN) step = GCAD_STEP_POLL;
    else step = GCAD_STEP_CANCEL;
    if (tag != GCAD_TAG_IN || result < 0) gcad_env_event(g, GCAD_EV_REPLY, tag | (step << 8), result);
    if (g->stopping) {
        if (tag == GCAD_TAG_SETUP && g->finish_pending) {
            g->finish_pending = 0;
            ioctl_(g, GCAD_TAG_CHANGE, GCAD_STEP_FINISH, GCAD_V5_ATTACH_FINISH, 0, 0, 0, 0);
            g->change_step = GCAD_STEP_FINISH;
        }
        stop_next(g);
        return;
    }
    if (tag == GCAD_TAG_CHANGE) on_change(g, result, now);
    else if (tag == GCAD_TAG_SETUP) on_setup(g, result, current, now);
    else if (tag == GCAD_TAG_OUT) on_out(g, result, current, now);
    else if (tag == GCAD_TAG_IN) on_in(g, result, current, now);
}

int gcad_port(gcad* g, uint32_t port, gcad_pad* out, uint32_t now) {
    if (port >= GCAD_PORTS || g->link != GCAD_LINK_POLL || g->stale || g->stopping) return 0;
    if (elapsed(now, g->data_time) >= ms(g, GCAD_TIMEOUT_MS)) {
        set_stale(g);
        return 0;
    }
    if (!g->origin_valid[port]) return 0;
    return gcad_decode(g->raw[port], g->origin[port], out);
}

void gcad_rumble(gcad* g, uint32_t port, uint32_t command) {
    if (port >= GCAD_PORTS) return;
    /* A WaveBird has no motor (Dolphin skips it too). */
    g->rumble_want[port] = (uint8_t)(command == 1 && (g->raw[port][0] & 0x10) ? 1 : 0);
}

/* gcad_stop's cancels, one at a time on the STOP slot. */
static void stop_next(gcad* g) {
    if (g->busy & BIT(GCAD_TAG_STOP)) return;
    if (g->version == 4) {
        if ((g->busy & BIT(GCAD_TAG_CHANGE)) && !(g->stopping & 2)) {
            g->stopping |= 2;
            ioctl_(g, GCAD_TAG_STOP, GCAD_STEP_CANCEL, GCAD_V4_SHUTDOWN, 0, 0, 0, 0);
            return;
        }
        if ((g->busy & BIT(GCAD_TAG_IN)) && g->dev_id >= 0 && !(g->stopping & 4)) {
            g->stopping |= 4;
            zero(g->stop_msg, sizeof(g->stop_msg));
            gcad_put32(g->stop_msg, (uint32_t)g->dev_id);
            g->stop_msg[4] = 0x81;
            ioctl_(g, GCAD_TAG_STOP, GCAD_STEP_CANCEL, GCAD_V4_CANCEL_INTERRUPT, g->stop_msg, 8, 0, 0);
            return;
        }
        if ((g->busy & BIT(GCAD_TAG_OUT)) && g->dev_id >= 0 && !(g->stopping & 8)) {
            g->stopping |= 8;
            zero(g->stop_msg, sizeof(g->stop_msg));
            gcad_put32(g->stop_msg, (uint32_t)g->dev_id);
            g->stop_msg[4] = 0x02;
            ioctl_(g, GCAD_TAG_STOP, GCAD_STEP_CANCEL, GCAD_V4_CANCEL_INTERRUPT, g->stop_msg, 8, 0, 0);
        }
        return;
    }
    if ((g->busy & BIT(GCAD_TAG_CHANGE)) && g->change_step == GCAD_STEP_CHANGE && !(g->stopping & 2)) {
        g->stopping |= 2;
        ioctl_(g, GCAD_TAG_STOP, GCAD_STEP_CANCEL, GCAD_V5_SHUTDOWN, 0, 0, 0, 0);
        return;
    }
    if ((g->busy & BIT(GCAD_TAG_IN)) && g->dev_id >= 0 && !(g->stopping & 4)) {
        g->stopping |= 4;
        zero(g->stop_msg, sizeof(g->stop_msg));
        gcad_put32(g->stop_msg, (uint32_t)g->dev_id);
        g->stop_msg[8] = 1;  /* the interrupt IN endpoint */
        ioctl_(g, GCAD_TAG_STOP, GCAD_STEP_CANCEL, GCAD_V5_CANCEL_ENDPOINT, g->stop_msg, 0x20, 0, 0);
        return;
    }
    if ((g->busy & BIT(GCAD_TAG_OUT)) && g->dev_id >= 0 && !(g->stopping & 8)) {
        g->stopping |= 8;
        zero(g->stop_msg, sizeof(g->stop_msg));
        gcad_put32(g->stop_msg, (uint32_t)g->dev_id);
        g->stop_msg[8] = 2;  /* the interrupt OUT endpoint */
        ioctl_(g, GCAD_TAG_STOP, GCAD_STEP_CANCEL, GCAD_V5_CANCEL_ENDPOINT, g->stop_msg, 0x20, 0, 0);
    }
    /* Nothing to release: without an Attach the device was never taken. */
}

void gcad_stop(gcad* g, uint32_t now) {
    (void)now;
    if (g->stopping) return;
    g->stopping = 1;
    set_stale(g);
    stop_next(g);
}

int gcad_idle(const gcad* g) { return g->busy == 0; }

void gcad_release(gcad* g) { (void)g; }

#endif /* HAVE_GCADAPTER */
