/* THQ uDraw GameTablet, a Wii Remote extension.  See input_tablet.h.
 *
 * libogc 3.1's wiiuse has no uDraw: an extension it does not know goes
 * through classic_ctrl_handshake and is decoded as a Classic Controller.
 * Two linker wrappers (-Wl,--wrap=classic_ctrl_handshake,--wrap=parse_event)
 * take it out of that path: the handshake one recognises the tablet's ID and
 * gives it an expansion type of its own, which wiiuse and WPAD then leave
 * alone, and the parse_event one keeps the six extension bytes of every
 * report before wiiuse sees them.
 *
 * Report layout (WiiBrew, Dolphin's emulation, rvlpaint's notes):
 *   x = b0 | (b2 & 0x0f) << 8, y = b1 | (b2 >> 4) << 8, 0xfff = no pen
 *   pressure = b3 | (b5 & 0x04) << 6 (nine bits)
 *   b5 bit 0 = upper rocker, bit 1 = lower rocker, both 0 when pressed
 *   b4 is always 0xff and the top five bits of b5 are set
 */

#ifdef HAVE_TABLET

#include <string.h>

#define WIIUSE_INTERNAL_H_INCLUDED /* wiimote_t's fields writable */
#include <gccore.h>
#include <ogc/libversion.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>

#include "input_tablet.h"
#include "libinput_internal.h"

#if _V_MAJOR_ != 3 || _V_MINOR_ != 1
#warning "uDraw: the wiiuse wrappers were written against libogc 3.1; check them against this version"
#endif

/* Not one of wiiuse's EXP_* values, so its event code and WPAD skip it. */
#define EXP_UDRAW   0x40

#define CHANNELS    4
#define RING        64
#define RING_MASK   (RING - 1)
/* No report for this long: the tablet (or its Wii Remote) has gone. */
#define STALE_MS    250
/* Idle pressure is about 8.  The nib touches above PRESS_ON and lifts below
 * PRESS_OFF, so a stroke does not stutter on the threshold. */
#define PRESS_ON    24
#define PRESS_OFF   16
#define MAX_PRESSURE 511

/* Not weak: a weak reference does not pull wiiuse's io.o and events.o out
 * of the archive.  Without the wrappers the link fails on these names. */
void wiiuse_send_next_command(struct wiimote_t *wm); /* wiiuse's, not in its public header */

int  __real_classic_ctrl_handshake(struct wiimote_t *wm, struct classic_ctrl_t *cc, ubyte *data, uword len);
void __real_parse_event(struct wiimote_t *wm);

typedef struct {
	u8 b[6];
	u32 time;
} ud_report;

/* Filled from wiiuse's receive path, which runs in interrupt context (no
 * floating point there), and read with interrupts off. */
static struct {
	ud_report ring[RING];
	volatile u32 head, tail;
	ud_report latest;
	volatile bool attached;
	volatile u32 last;   /* when the latest report came */
} ud_raw[CHANNELS];

/* A decoder's memory: the nib's hysteresis, and where the pen was when it
 * left.  TB_Pop and TB_GetState each have their own, as they read different
 * reports. */
typedef struct {
	bool touching;
	s32 x, y;
} ud_track;

static ud_track ud_popped[CHANNELS], ud_current[CHANNELS];

static s32 ud_min_x = 0x56, ud_max_x = 0x780, ud_min_y = 0x65, ud_max_y = 0x5a5;

static const u8 udraw_id[6] = { 0xff, 0x00, 0xa4, 0x20, 0x01, 0x12 };

static u32 now_ms(void)
{
	return (u32)ticks_to_millisecs(gettime());
}

int __wrap_classic_ctrl_handshake(struct wiimote_t *wm, struct classic_ctrl_t *cc, ubyte *data, uword len)
{
	/* the ID is the last six bytes of the 224 read from 0xa40020 */
	if (data && len >= 224 && !memcmp(data + 218, udraw_id, sizeof(udraw_id))) {
		u32 level;
		wm->exp.type = EXP_UDRAW;
		if (wm->unid >= 0 && wm->unid < CHANNELS) {
			_CPU_ISR_Disable(level);
			ud_raw[wm->unid].head = ud_raw[wm->unid].tail = 0;
			ud_raw[wm->unid].attached = true;
			ud_raw[wm->unid].last = now_ms();
			_CPU_ISR_Restore(level);
		}
		return 1;
	}
	return __real_classic_ctrl_handshake(wm, cc, data, len);
}

/* libogc's Bluetooth stack drops an output report, while saying it went,
 * when the controller has no buffer free and one report is already waiting
 * (lp_acl_write).  wiiuse sends one command at a time and waits for its
 * answer with no timeout, so a dropped one stops every command after it:
 * an extension's handshake then never ends, and a uDraw (or a Nunchuk) is
 * never seen.  A command that has had no answer for this long goes again. */
#define RESEND_MS 300
#define WIIMOTES  6

static struct {
	const struct cmd_blk_t *cmd;
	u32 since;
} ud_sent[WIIMOTES];

static void resend_stalled(struct wiimote_t *wm)
{
	struct cmd_blk_t *cmd = wm->cmd_head;
	int i = wm->unid;

	if (i < 0 || i >= WIIMOTES)
		return;
	if (!cmd || cmd->state != CMD_SENT) {
		ud_sent[i].cmd = NULL;
		return;
	}
	if (cmd != ud_sent[i].cmd) {
		ud_sent[i].cmd = cmd;
		ud_sent[i].since = now_ms();
	} else if (now_ms() - ud_sent[i].since > RESEND_MS) {
		cmd->state = CMD_READY;
		ud_sent[i].since = now_ms();
		wiiuse_send_next_command(wm);
	}
}

/* Where a report's extension bytes start, counting the report ID. */
static int ext_offset(u8 report)
{
	switch (report) {
	case 0x32: /* buttons, extension */
	case 0x34:
		return 3;
	case 0x35: /* buttons, accelerometer, extension */
		return 6;
	case 0x36: /* buttons, IR, extension */
		return 13;
	case 0x37: /* buttons, accelerometer, IR, extension */
		return 16;
	}
	return -1;
}

void __wrap_parse_event(struct wiimote_t *wm)
{
	int chan = wm->unid;

	if (chan >= 0 && chan < CHANNELS && wm->exp.type == EXP_UDRAW) {
		int off = ext_offset(wm->event_buf[0]);
		if (off >= 0) {
			u32 level, next;
			_CPU_ISR_Disable(level);
			next = (ud_raw[chan].head + 1) & RING_MASK;
			if (next == ud_raw[chan].tail)
				ud_raw[chan].tail = (ud_raw[chan].tail + 1) & RING_MASK; /* drop the oldest */
			memcpy(ud_raw[chan].ring[ud_raw[chan].head].b, wm->event_buf + off, 6);
			ud_raw[chan].ring[ud_raw[chan].head].time = now_ms();
			ud_raw[chan].head = next;
			memcpy(ud_raw[chan].latest.b, wm->event_buf + off, 6);
			ud_raw[chan].latest.time = ud_raw[chan].last = now_ms();
			_CPU_ISR_Restore(level);
		}
	}
	__real_parse_event(wm);
	resend_stalled(wm);
	/* unplugged: wiiuse has put the expansion type back to none */
	if (chan >= 0 && chan < CHANNELS && ud_raw[chan].attached && wm->exp.type != EXP_UDRAW)
		ud_raw[chan].attached = false;
}

static bool fresh(int chan)
{
	return ud_raw[chan].attached && now_ms() - ud_raw[chan].last <= STALE_MS;
}

static bool decode(int chan, const ud_report *r, ud_track *t, tb_sample *s)
{
	const u8 *b = r->b;
	s32 rx, ry, w = ud_max_x - ud_min_x, h = ud_max_y - ud_min_y;
	u32 p;

	if (b[4] != 0xff || (b[5] & 0xf8) != 0xf8)
		return false;
	memset(s, 0, sizeof(*s));
	s->wiimote = chan;
	s->time = r->time;
	rx = b[0] | (b[2] & 0x0f) << 8;
	ry = b[1] | (b[2] >> 4) << 8;
	p = b[3] | (b[5] & 0x04) << 6;
	s->in_range = rx != 0xfff && ry != 0xfff;
	if (!(b[5] & 0x01))
		s->buttons |= TB_BUTTON_BARREL2;
	if (!(b[5] & 0x02))
		s->buttons |= TB_BUTTON_BARREL;

	if (!s->in_range) {
		t->touching = false;
		/* keep the last place, as a USB tablet does when the pen leaves */
		s->x = t->x;
		s->y = t->y;
	} else {
		t->touching = p >= (t->touching ? PRESS_OFF : PRESS_ON);
		/* the tablet counts y from the bottom */
		s->x = rx - ud_min_x;
		s->y = ud_max_y - ry;
		if (s->x < 0) s->x = 0;
		if (s->y < 0) s->y = 0;
		if (s->x > w) s->x = w;
		if (s->y > h) s->y = h;
	}
	if (t->touching)
		s->buttons |= TB_BUTTON_TIP;
	s->raw_pressure = p;
	s->pressure = (float)p / MAX_PRESSURE;
	s->nx = (float)s->x / (float)w;
	s->ny = (float)s->y / (float)h;
	t->x = s->x;
	t->y = s->y;
	return true;
}

bool inpw_udraw_pop(tb_sample *out)
{
	int chan;

	for (chan = 0; chan < CHANNELS; chan++) {
		while (ud_raw[chan].tail != ud_raw[chan].head) {
			ud_report r;
			u32 level;
			_CPU_ISR_Disable(level);
			r = ud_raw[chan].ring[ud_raw[chan].tail];
			ud_raw[chan].tail = (ud_raw[chan].tail + 1) & RING_MASK;
			_CPU_ISR_Restore(level);
			if (decode(chan, &r, &ud_popped[chan], out))
				return true;
		}
	}
	return false;
}

/* The pen on the tablet that reported last. */
bool inpw_udraw_state(tb_sample *out)
{
	int chan, best = -1;
	ud_report r;
	u32 level;

	for (chan = 0; chan < CHANNELS; chan++)
		if (fresh(chan) && (best < 0 || (s32)(ud_raw[chan].last - ud_raw[best].last) > 0))
			best = chan;
	if (best < 0)
		return false;
	_CPU_ISR_Disable(level);
	r = ud_raw[best].latest;
	_CPU_ISR_Restore(level);
	return decode(best, &r, &ud_current[best], out);
}

bool inpw_udraw_info(tb_info *info)
{
	int chan;

	for (chan = 0; chan < CHANNELS; chan++)
		if (fresh(chan))
			break;
	if (chan == CHANNELS)
		return false;
	memset(info, 0, sizeof(*info));
	strcpy(info->name, "uDraw GameTablet");
	info->max_x = ud_max_x - ud_min_x;
	info->max_y = ud_max_y - ud_min_y;
	info->max_pressure = MAX_PRESSURE;
	return true;
}

bool TB_UDrawConnected(int wiimote)
{
	return wiimote >= 0 && wiimote < CHANNELS && fresh(wiimote);
}

void TB_UDrawSetArea(s32 min_x, s32 max_x, s32 min_y, s32 max_y)
{
	if (min_x < 0 || min_y < 0 || max_x > 0xffe || max_y > 0xffe || max_x - min_x < 100 || max_y - min_y < 100)
		return;
	ud_min_x = min_x;
	ud_max_x = max_x;
	ud_min_y = min_y;
	ud_max_y = max_y;
}

void inpw_udraw_extent(s32 *w, s32 *h)
{
	*w = ud_max_x - ud_min_x;
	*h = ud_max_y - ud_min_y;
}

#endif /* HAVE_TABLET */
