#include "state.h"

// cart_hw/areplay.c

struct action_replay_t action_replay;

// cart_hw/eeprom_93c.c

T_EEPROM_93C eeprom_93c;

// cart_hw/eeprom_i2c.c

struct eeprom_i2c_t eeprom_i2c;

// cart_hw/eeprom_spi.c

T_EEPROM_SPI spi_eeprom;

// cart_hw/ggenie.c

struct ggenie_t ggenie;

// cart_hw/megasd.c

T_MEGASD_HW megasd_hw;

// cart_hw/sram.c

T_SRAM sram;

// input_hw/activator.c

struct activator_t activator[2];

// input_hw/gamepad.c

struct gamepad_t gamepad[MAX_DEVICES];
struct flipflop_t flipflop[2];
uint8_t latch;

// input_hw/graphic_board.c

struct graphic_board_t board;

// input_hw/input.c

t_input input;
int old_system[2] = {-1,-1};

// input_hw/lightgun.c

struct lightgun_t lightgun;

// input_hw/mouse.c

struct mouse_t mouse;

// input_hw/paddle.c

struct paddle_t paddle[2];

// input_hw/sportspad.c

struct sportspad_t sportspad[2];

// input_hw/teamplayer.c

struct teamplayer_t teamplayer[2];

// input terebi_oekaki.c

struct tablet_t tablet;

// input xe_1ap.c

struct xe_1ap_t xe_1ap[2];

// genesis.c

uint8_t boot_rom[0x800];
uint8_t work_ram[0x10000];
uint8_t zram[0x2000];
uint32_t zbank;
uint8_t zstate;
uint8_t pico_current;
uint8_t tmss[4];     // TMSS security register

// io_ctrl.c

uint8_t io_reg[0x10];
uint8_t region_code = REGION_USA;
struct port_t port[3];

// load_rom.c

ROMINFO rominfo;
uint8_t romtype;
uint8_t rom_region;

// membnk.c

t_zbank_memory_map zbank_memory_map[256];

// system.c

t_bitmap bitmap;
t_snd snd;
uint32_t mcycles_vdp;
uint8_t system_hw;
uint8_t system_bios;
uint32_t system_clock;
int16_t SVP_cycles = 800; 
uint8_t pause_b;
EQSTATE eq[2];
int16_t llp,rrp;

// vdp.c

uint8_t ALIGNED_(4) sat[0x400];     /* Internal copy of sprite attribute table */
uint8_t ALIGNED_(4) vram[0x10000];  /* Video RAM (64K x 8-bit) */
uint8_t ALIGNED_(4) cram[0x80];     /* On-chip color RAM (64 x 9-bit) */
uint8_t ALIGNED_(4) vsram[0x80];    /* On-chip vertical scroll RAM (40 x 11-bit) */
uint8_t reg[0x20];                  /* Internal VDP registers (23 x 8-bit) */
uint8_t hint_pending;               /* 0= Line interrupt is pending */
uint8_t vint_pending;               /* 1= Frame interrupt is pending */
uint16_t status;                    /* VDP status flags */
uint32_t dma_length;                /* DMA remaining length */
uint32_t dma_endCycles;             /* DMA end cycle */
uint8_t dma_type;                   /* DMA mode */
uint16_t ntab;                      /* Name table A base address */
uint16_t ntbb;                      /* Name table B base address */
uint16_t ntwb;                      /* Name table W base address */
uint16_t satb;                      /* Sprite attribute table base address */
uint16_t hscb;                      /* Horizontal scroll table base address */
uint8_t bg_name_dirty[0x800];       /* 1= This pattern is dirty */
uint16_t bg_name_list[0x800];       /* List of modified pattern indices */
uint16_t bg_list_index;             /* # of modified patterns in list */
uint8_t hscroll_mask;               /* Horizontal Scrolling line mask */
uint8_t playfield_shift;            /* Width of planes A, B (in bits) */
uint8_t playfield_col_mask;         /* Playfield column mask */
uint16_t playfield_row_mask;        /* Playfield row mask */
uint16_t vscroll;                   /* Latched vertical scroll value */
uint8_t odd_frame;                  /* 1: odd field, 0: even field */
uint8_t im2_flag;                   /* 1= Interlace mode 2 is being used */
uint8_t interlaced;                 /* 1: Interlaced mode 1 or 2 */
uint8_t vdp_pal;                    /* 1: PAL , 0: NTSC (default) */
uint8_t h_counter;                  /* Horizontal counter */
uint16_t v_counter;                 /* Vertical counter */
uint16_t vc_max;                    /* Vertical counter overflow value */
uint16_t lines_per_frame;           /* PAL: 313 lines, NTSC: 262 lines */
uint16_t max_sprite_pixels;         /* Max. sprites pixels per line (parsing & rendering) */
uint32_t fifo_cycles[4];            /* VDP FIFO read-out cycles */
uint32_t hvc_latch;                 /* latched HV counter */
uint32_t vint_cycle;                /* VINT occurence cycle */
const uint8_t *hctab;               /* pointer to H Counter table */

uint8_t border;            /* Border color index */
uint8_t pending;           /* Pending write flag */
uint8_t code;              /* Code register */
uint16_t addr;             /* Address register */
uint16_t addr_latch;       /* Latched A15, A14 of address */
uint16_t sat_base_mask;    /* Base bits of SAT */
uint16_t sat_addr_mask;    /* Index bits of SAT */
uint16_t dma_src;          /* DMA source address */
int dmafill;             /* DMA Fill pending flag */
int cached_write;        /* 2nd part of 32-bit CTRL port write (Genesis mode) or LSB of CRAM data (Game Gear mode) */
uint16_t fifo[4];          /* FIFO ring-buffer */
int fifo_idx;            /* FIFO write index */
int fifo_byte_access;    /* FIFO byte access flag */
int *fifo_timing;        /* FIFO slots timing table */
int hblank_start_cycle;  /* HBLANK flag set cycle */
int hblank_end_cycle;    /* HBLANK flag clear cycle */

// vdp_render.c

struct clip_t clip[2];
uint8_t ALIGNED_(4) bg_pattern_cache[0x80000]; /* Cached and flipped patterns */
uint8_t name_lut[0x400]; /* Sprite pattern name offset look-up table (Mode 5) */
uint32_t bp_lut[0x10000]; /* Bitplane to packed pixel look-up table (Mode 4) */
uint8_t lut[LUT_MAX][LUT_SIZE]; /* Layer priority pixel look-up tables */
PIXEL_OUT_T pixel[0x100]; /* Output pixel data look-up tables*/
PIXEL_OUT_T pixel_lut[3][0x200];
PIXEL_OUT_T pixel_lut_m4[0x40];
uint8_t linebuf[2][0x200]; /* Background & Sprite line buffers */
uint8_t spr_ovr; /* Sprite limit flag */
object_info_t obj_info[2][MAX_SPRITES_PER_LINE];
uint8_t object_count[2]; /* Sprite Counter */
uint16_t spr_col; /* Sprite Collision Info */