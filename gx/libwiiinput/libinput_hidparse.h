/* A small HID report descriptor parser, enough to find a pen tablet's fields.
 *
 * Kept free of libogc so it can be built and tested on a computer.  Every
 * usage is stored as (page << 16) | id, after folding Wacom's vendor page onto
 * the standard pages it mirrors (see hidp_equivalent). */

#ifndef LIBINPUT_HIDPARSE_H
#define LIBINPUT_HIDPARSE_H

#include <stdbool.h>
#include <stdint.h>

#define HIDP_MAX_FIELDS  256
#define HIDP_MAX_REPORTS 64

#define HIDP_INPUT   0
#define HIDP_FEATURE 1

/* Standard usages */
#define HIDP_GD_MOUSE          0x00010002u
#define HIDP_GD_X              0x00010030u
#define HIDP_GD_Y              0x00010031u
#define HIDP_DG_DIGITIZER      0x000D0001u
#define HIDP_DG_PEN            0x000D0002u
#define HIDP_DG_STYLUS         0x000D0020u
#define HIDP_DG_TIP_PRESSURE   0x000D0030u
#define HIDP_DG_IN_RANGE       0x000D0032u
#define HIDP_DG_INVERT         0x000D003Cu
#define HIDP_DG_TILT_X         0x000D003Du
#define HIDP_DG_TILT_Y         0x000D003Eu
#define HIDP_DG_TIP_SWITCH     0x000D0042u
#define HIDP_DG_BARREL         0x000D0044u
#define HIDP_DG_ERASER         0x000D0045u
#define HIDP_DG_INPUT_MODE     0x000D0052u
#define HIDP_DG_BARREL2        0x000D005Au
#define HIDP_PAGE_BUTTON       0x0009u

/* Wacom's own usages that have no standard equivalent */
#define HIDP_WD_DISTANCE       0xFF0D0132u
#define HIDP_WD_REPORT_VALID   0xFF0D01D0u
#define HIDP_WD_EXPRESSKEY0    0xFF0D0910u
#define HIDP_WD_BUTTONHOME     0xFF0D0990u

typedef struct {
	uint32_t usage;
	uint32_t app;        /* the enclosing application collection's usage */
	uint8_t type;        /* HIDP_INPUT or HIDP_FEATURE */
	uint8_t report_id;   /* 0 when the device uses no report IDs */
	uint16_t bit_offset; /* from the start of the report, after the ID byte */
	uint8_t bit_size;
	int32_t logical_min, logical_max;
} hidp_field;

typedef struct {
	hidp_field fields[HIDP_MAX_FIELDS];
	int nfields;
	bool uses_ids;
	/* Report lengths in bits, without the ID byte, by type and ID. */
	uint16_t bits[2][256];
} hidp_desc;

/* Returns false on a descriptor too broken to use.  Fields past
 * HIDP_MAX_FIELDS are dropped rather than failing the parse. */
bool hidp_parse(const uint8_t *desc, uint32_t len, hidp_desc *out);

/* Wacom's 0xFF0D page repeats the standard ones: 0xFF0D00xx is the digitizer
 * page, 0xFF0D01xx generic desktop, 0xFF0D09xx buttons, and so on. */
uint32_t hidp_equivalent(uint32_t usage);

/* The first field of a type with a usage, optionally in one report (-1 for
 * any). */
const hidp_field *hidp_find(const hidp_desc *d, uint8_t type, uint32_t usage,
                            int report_id);

/* Length in bytes of a report as it is sent, ID byte included. */
uint32_t hidp_report_bytes(const hidp_desc *d, uint8_t type, uint8_t report_id);

/* A field's value from a report, `data` starting after the ID byte. */
int32_t hidp_get(const hidp_field *f, const uint8_t *data, uint32_t len);
void hidp_put(const hidp_field *f, uint8_t *data, uint32_t len, int32_t value);

/* ---- pen tablets ------------------------------------------------------- */

#define HIDP_MAX_PAD 16

typedef struct {
	uint8_t report_id;
	const hidp_field *x, *y, *pressure;
	const hidp_field *tip, *barrel, *barrel2, *eraser, *invert, *in_range;
	const hidp_field *tilt_x, *tilt_y, *distance, *valid;
	/* Tablet buttons (ExpressKeys), from any report but the mouse's. */
	const hidp_field *pad[HIDP_MAX_PAD];
	int npad;
	const hidp_field *input_mode; /* a feature to set to 2, if any */
} hidp_pen;

/* Finds the report carrying the pen: X and Y inside a digitizer, pen or
 * stylus collection.  False when the descriptor describes no pen. */
bool hidp_find_pen(const hidp_desc *d, hidp_pen *pen);

#endif
