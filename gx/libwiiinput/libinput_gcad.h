/* SPDX-FileCopyrightText: 2026 RiftWii contributors */
/* SPDX-FileCopyrightText: 2017 Alex Chadwick (wup-028-bslug) <https://github.com/Chadderz121/wup-028-bslug> */
/* SPDX-FileCopyrightText: FIX94 and the Nintendont contributors, used with permission <https://github.com/FIX94/Nintendont> */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The GameCube controller adapter for Wii U (WUP-028) through IOS's
 * /dev/usb/hid: one driver for RiftWii's in-game runtime, its menu and its host tests,
 * taken from RiftWii (github.com/KakarottoCake/Riftwii, runtime/rtgcad.h);
 * libinput_gcadapter.c is its environment here.
 *
 * It is a state machine driven by IPC replies: every request goes out
 * asynchronously through the environment (gcad_env_*, supplied by each
 * user), and its reply comes back to gcad_on_reply. Nothing blocks, so
 * the game's PADRead can drive it and IOS's replies can arrive in its
 * IPC interrupt. Freestanding: no libc, no constant data, no division.
 *
 * Sources (no code taken from any of them):
 *  - wiibrew, "/dev/usb/hid (v4)", "/dev/usb/hid (v5)", "/dev/usb/ven":
 *    ioctl numbers, buffer sizes, the device lists, handles (the open
 *    mode picks one of 16 in v5), SuspendResume/GetDeviceInfo before any
 *    transfer, the v5 interrupt direction word at offset 8, -7022 on
 *    unplug, the manager locked to a handle until AttachFinish.
 *  - Dolphin's IOS HID v4/v5 emulation: exact sizes it checks (0x600,
 *    0x180, 0x20, 0x60), the v4 device entry layout.
 *  - Dolphin's GCAdapter.cpp and wup-028-bslug (MIT, Alex Chadwick 2017):
 *    the adapter's init (0x13) and rumble (0x11) commands, the 37-byte
 *    report starting 0x21, 9 bytes per port, wired/wireless bits 4/5,
 *    the button bits; rumble sent between polls.
 *  - fakemote (embedded-game-controller): Attach, resume and read the
 *    descriptors before AttachFinish; other handles (fakemote uses 15,
 *    libogc 0) may own a device for a while and release it later.
 */
#ifndef LIBINPUT_GCAD_H
#define LIBINPUT_GCAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GCAD_VID_PID 0x057E0337u  /* Nintendo WUP-028 */
#define GCAD_PORTS 4u
#define GCAD_REPORT_BYTES 37u
#define GCAD_TIMEOUT_MS 1500u     /* no report for this long: every port reads as unplugged */
#define GCAD_RELINK_MS 1000u      /* a failed link is tried again after this */
#define GCAD_RESCAN_MS 2000u      /* a device another handle owns, or a failed device list */
#define GCAD_RECAL_MS 3000u       /* X+Y+Start held this long takes a new origin, like the pad */
#define GCAD_ACTIVE_STEP 10       /* a stick or trigger moving this far between reads is a player (gcad_pad_moved) */
#define GCAD_ORIGIN_SPREAD 32     /* a stick at rest reads within this of the middle (128) */
#define GCAD_ORIGIN_STEADY_READS 3 /* readings at rest in a row, each close to the last, before one is the origin */
#define GCAD_ORIGIN_STEADY_STEP 3 /* how close: no axis further than this from the reading before */
#define GCAD_ORIGIN_TRIGGER_MAX 80 /* a trigger at rest reads no more than this */
#define GCAD_ORIGIN_WAIT_MS 500u  /* no reading at rest this long: the axes still off take the middle */
#define GCAD_CTRL_MS 100u         /* SET_PROTOCOL unanswered this long: init anyway */
#define GCAD_SETTLE_MS 500u       /* v5: a newly listed adapter is set up this long after the change */

/* /dev/usb/hid ioctls. */
#define GCAD_V4_GET_DEVICE_CHANGE 0u
#define GCAD_V4_CONTROL 2u
#define GCAD_V4_INTERRUPT_IN 3u
#define GCAD_V4_INTERRUPT_OUT 4u
#define GCAD_V4_GET_VERSION 6u
#define GCAD_V4_SHUTDOWN 7u
#define GCAD_V4_CANCEL_INTERRUPT 8u
#define GCAD_V5_GET_VERSION 0u
#define GCAD_V5_GET_DEVICE_CHANGE 1u
#define GCAD_V5_SHUTDOWN 2u
#define GCAD_V5_GET_DEVICE_INFO 3u
#define GCAD_V5_ATTACH 4u
#define GCAD_V5_RELEASE 5u
#define GCAD_V5_ATTACH_FINISH 6u
#define GCAD_V5_SUSPEND_RESUME 16u
#define GCAD_V5_CANCEL_ENDPOINT 17u
#define GCAD_V5_CONTROL 18u
#define GCAD_V5_INTERRUPT 19u
#define GCAD_V4_VERSION 0x00040001u
#define GCAD_V5_VERSION 0x00050001u

/* Request slots: each has at most one request in flight. */
#define GCAD_TAG_CHANGE 1u   /* device list, then (v5) AttachFinish */
#define GCAD_TAG_SETUP 2u    /* v5: Attach, SuspendResume, GetDeviceInfo */
#define GCAD_TAG_IN 3u       /* the report poll */
#define GCAD_TAG_OUT 4u      /* init, then rumble */
#define GCAD_TAG_STOP 5u     /* gcad_stop's cancels */
#define GCAD_TAGS 6u

/* Link states. */
#define GCAD_LINK_NONE 0u    /* no adapter listed */
#define GCAD_LINK_SETUP 1u   /* v5 setup in flight */
#define GCAD_LINK_INIT 2u    /* init command in flight */
#define GCAD_LINK_POLL 3u    /* reports flowing */
#define GCAD_LINK_FAILED 4u  /* waiting to try again */
#define GCAD_LINK_BUSY 5u    /* v5: another handle owns it for now */
#define GCAD_LINK_CTRL 6u    /* SET_PROTOCOL in flight, init next */
#define GCAD_LINK_SETTLE 7u  /* v5: listed just now, set up after GCAD_SETTLE_MS */

/* Event codes for gcad_env_event (the test page's log). */
#define GCAD_EV_REPLY 1u     /* a = tag | step << 8, b = result */
#define GCAD_EV_SUBMIT_FAIL 2u /* a = tag | step << 8, b = result */
#define GCAD_EV_DEVICE 3u    /* a = device id, b = vid << 16 | pid (each listed device) */
#define GCAD_EV_FOUND 4u     /* a = device id */
#define GCAD_EV_LOST 5u      /* a = device id */
#define GCAD_EV_LINKED 6u    /* a = device id: reports flowing */
#define GCAD_EV_PORT 7u      /* a = port, b = status byte (connected or not) */
#define GCAD_EV_BAD_REPORT 8u /* a = first byte, b = result */
#define GCAD_EV_FIRST_REPORT 9u /* a = device id, b = the four ports' status bytes, port 1 highest */
#define GCAD_EV_ORIGIN 10u   /* a = port | how << 8 | L << 16 | R << 24, b = stick x, y, substick x, y (x highest) */

/* How a port's origin was taken (GCAD_EV_ORIGIN). */
#define GCAD_ORIGIN_RESTING 0u /* from a reading of the pad at rest */
#define GCAD_ORIGIN_MIDDLE 1u  /* nothing steady at rest for GCAD_ORIGIN_WAIT_MS: the axes not settled took the middle */
#define GCAD_ORIGIN_RECAL 2u   /* X+Y+Start held */
#define GCAD_ORIGIN_PASSED 3u  /* not taken: the pad's first reading was not at rest (the values are that reading) */

/* Steps inside the slots, for the log. */
#define GCAD_STEP_CHANGE 0u
#define GCAD_STEP_FINISH 1u
#define GCAD_STEP_CTRL 2u    /* SET_PROTOCOL */
#define GCAD_STEP_RESUME 3u
#define GCAD_STEP_INFO 4u
#define GCAD_STEP_INIT 5u
#define GCAD_STEP_RUMBLE 6u
#define GCAD_STEP_POLL 7u
#define GCAD_STEP_CANCEL 8u

/* One port as the SDK's PADStatus would carry it, relative to the
 * origin taken when the controller appeared (as PADRead reports it). */
typedef struct gcad_pad {
    uint16_t buttons;          /* PAD_BUTTON_* bits */
    int8_t stick_x, stick_y, substick_x, substick_y;
    uint8_t trigger_l, trigger_r;
    uint8_t wireless;          /* a WaveBird receiver: no rumble */
} gcad_pad;

/* SDK PAD button bits. */
#define GCAD_PAD_LEFT 0x0001u
#define GCAD_PAD_RIGHT 0x0002u
#define GCAD_PAD_DOWN 0x0004u
#define GCAD_PAD_UP 0x0008u
#define GCAD_PAD_Z 0x0010u
#define GCAD_PAD_R 0x0020u
#define GCAD_PAD_L 0x0040u
#define GCAD_PAD_A 0x0100u
#define GCAD_PAD_B 0x0200u
#define GCAD_PAD_X 0x0400u
#define GCAD_PAD_Y 0x0800u
#define GCAD_PAD_START 0x1000u

/* All state. The buffers IOS reads and writes come first: the whole
 * struct must sit in MEM2, 32-byte aligned (wiibrew: every /dev/usb
 * buffer is in MEM2 and aligned). Each buffer is a whole number of cache
 * lines so flushing one never touches another. */
typedef struct gcad {
    uint8_t change[0x600];     /* device list: v4 fills 0x600, v5 0x180 */
    uint8_t setup_in[0x20];
    uint8_t setup_out[0x60];
    uint8_t in_msg[0x40];      /* v4 request (32 bytes) or v5 message (64) */
    uint8_t in_data[0x40];     /* the report */
    uint8_t out_msg[0x40];
    uint8_t out_data[0x20];    /* init or rumble command */
    uint8_t stop_msg[0x20];    /* gcad_stop's cancel argument */
    uint8_t env[0x40];         /* the environment's own (ioctlv vectors) */

    void* env_ctx;             /* the environment's context */
    int32_t fd;
    uint32_t version;          /* 4 or 5 */
    uint32_t ticks_per_ms;
    uint32_t generation;       /* bumped whenever the link is dropped */
    uint32_t busy;             /* bit per tag: a request in flight */
    uint8_t started, stopping, change_step, setup_step;
    uint8_t finish_pending;    /* v5: AttachFinish owed once setup ends */
    uint8_t change_failed, link, stale;
    int32_t dev_id;            /* -1: none */
    uint32_t change_time, link_time, data_time;

    /* The latest report, per port. */
    uint8_t raw[GCAD_PORTS][9];
    uint8_t origin[GCAD_PORTS][6]; /* stick x, y, substick x, y, L, R */
    uint8_t origin_valid[GCAD_PORTS];
    uint8_t recal_active[GCAD_PORTS];
    uint32_t origin_since[GCAD_PORTS];  /* when the wait for a reading at rest began */
    uint8_t steady[GCAD_PORTS][6];      /* the last reading at rest, while the origin waits */
    uint8_t steady_reads[GCAD_PORTS][6]; /* per axis: readings at rest in a row, each close to the one before */
    uint32_t recal_since[GCAD_PORTS];

    /* Rumble: what the game wants and what the adapter last took. */
    uint8_t rumble_want[GCAD_PORTS];
    uint8_t rumble_sent[GCAD_PORTS];
    uint8_t rumble_flight[GCAD_PORTS];

    /* Counters for the logs. */
    uint32_t reports, bad_reports, errors, changes, links;
    int32_t last_error;
    uint32_t last_error_step;
    int32_t ctrl_result, resume_result, info_result, init_result;
    uint32_t listed;           /* devices in the last list */
} gcad;

/* The environment. Each returns what IOS_IoctlAsync / IOS_IoctlvAsync
 * returned (0 when queued); the reply must reach gcad_on_reply with the
 * same `tag` value. For the ioctlv, `msg` is the first vector (in) and
 * `data` the second: in when `data_in`, else io. */
int32_t gcad_env_ioctl(gcad* g, uint32_t tag, uint32_t cmd, void* in, uint32_t in_len, void* out,
                       uint32_t out_len);
int32_t gcad_env_ioctlv(gcad* g, uint32_t tag, uint32_t cmd, void* msg, uint32_t msg_len, void* data,
                        uint32_t data_len, int data_in);
/* The address IOS takes for a pointer inside a v4 request (virtual). */
uint32_t gcad_env_address(gcad* g, void* p);
/* Cache maintenance around a buffer IOS reaches only through that
 * pointer (the SDK's IPC layer handles the ones passed directly). */
void gcad_env_flush(gcad* g, void* p, uint32_t len);
void gcad_env_invalidate(gcad* g, void* p, uint32_t len);
/* For the logs; may do nothing. */
void gcad_env_event(gcad* g, uint32_t code, uint32_t a, int32_t b);

/* Zeroes the state. `version` is what GetVersion answered (4 or 5). */
void gcad_init(gcad* g, void* env_ctx, int32_t fd, uint32_t version, uint32_t ticks_per_ms);
/* The adapter's device id when the loader already knows it: v5 hands
 * its device list out once per change, to whoever asks first, and the
 * menu's USB had it; the list request still goes out, for later changes.
 * Call it right after gcad_init. */
void gcad_expect(gcad* g, int32_t dev_id);
/* Starts the first device list, retries what failed, notices silence.
 * Call it often (every PADRead). `now` is a free-running tick count. */
void gcad_tick(gcad* g, uint32_t now);
/* A reply. `tag` is the value handed to the environment. */
void gcad_on_reply(gcad* g, uint32_t tag, int32_t result, uint32_t now);
/* 1 and the port's state when a controller is plugged into it and the
 * adapter is reporting, else 0. */
int gcad_port(gcad* g, uint32_t port, gcad_pad* out, uint32_t now);
/* PADControlMotor's command for a port: 1 rumbles, 0 and 2 stop. */
void gcad_rumble(gcad* g, uint32_t port, uint32_t command);
/* Stops for good: cancels what is in flight (Shutdown for the device
 * list, the endpoints' transfers) and sends nothing more. Done when
 * gcad_idle is 1; then v5 releases the device and the handle may close. */
void gcad_stop(gcad* g, uint32_t now);
int gcad_idle(const gcad* g);
/* v5: gives the device back (after gcad_stop is idle). */
void gcad_release(gcad* g);

/* Pure helpers (the host tests use them too). */
uint32_t gcad_get32(const uint8_t* p);
void gcad_put32(uint8_t* p, uint32_t v);
/* The adapter in a v4 list: its device id, or -1. */
int32_t gcad_find_v4(const uint8_t* list, uint32_t list_bytes);
/* The adapter in a v5 list of `count` entries: its device id, or -1. */
int32_t gcad_find_v5(const uint8_t* list, uint32_t count);
/* A report's port: 1 and `out` when a controller is plugged in. */
int gcad_decode(const uint8_t raw[9], const uint8_t origin[6], gcad_pad* out);
/* 1 when `now` differs from `before` the way the SDK's PADRead counts a
 * pad in a console port as in use, which keeps the screen from dimming:
 * a button changed, or a stick or trigger moved GCAD_ACTIVE_STEP or more.
 * A resting stick's wobble of a few steps is not a player. */
int gcad_pad_moved(const gcad_pad* before, const gcad_pad* now);

#ifdef __cplusplus
}
#endif

#endif
