#pragma once

#include <stdint.h>
#include "cart_hw/areplay.h"
#include "cart_hw/eeprom_93c.h"
#include "cart_hw/eeprom_i2c.h"
#include "cart_hw/eeprom_spi.h"
#include "cart_hw/ggenie.h"
#include "cart_hw/megasd.h"
#include "cart_hw/sram.h"
#include "input_hw/activator.h"
#include "input_hw/gamepad.h"
#include "input_hw/input.h"
#include "input_hw/graphic_board.h"
#include "sound/eq.h"
#include "genesis.h"
#include "io_ctrl.h"
#include "loadrom.h"
#include "membnk.h"
#include "system.h"
#include "macros.h"
#include "vdp_render.h"
#include "ntsc/md_ntsc.h"
#include "ntsc/sms_ntsc.h"

// cart_hw/svp.h 

// Special case, as svp is inside the cart.rom allocation

// cart_hw/areplay.h

extern struct action_replay_t action_replay;

// cart_hw/eeprom_93c.h

extern T_EEPROM_93C eeprom_93c;

// cart_hw/eeprom_i2c.h

extern struct eeprom_i2c_t eeprom_i2c;

// cart_hw/eeprom_spi.h

extern T_EEPROM_SPI spi_eeprom;

// cart_hw/ggenie.h

extern struct ggenie_t ggenie;

// cart_hw/megasd.h

extern T_MEGASD_HW megasd_hw;

// cart_hw/sram.h

extern T_SRAM sram;

// input_hw/activator.h

extern struct activator_t activator[2];

// input_hw/gamepad.h

extern struct gamepad_t gamepad[MAX_DEVICES];
extern struct flipflop_t flipflop[2];
extern uint8_t latch;

// input_hw/graphic_board.h

extern struct graphic_board_t board;

// input_hw/input.h

extern t_input input;
extern int old_system[2];

// genesis.h

extern uint8_t boot_rom[0x800];
extern uint8_t work_ram[0x10000];
extern uint8_t zram[0x2000];
extern uint32_t zbank;
extern uint8_t zstate;
extern uint8_t pico_current;
extern uint8_t tmss[4];     // TMSS security register

// io_ctrl.h

extern uint8_t io_reg[0x10];
extern uint8_t region_code;
extern struct port_t port[3];

// load_rom.h

extern ROMINFO rominfo;
extern uint8_t romtype;
extern uint8_t rom_region;

// membnk.h

extern t_zbank_memory_map zbank_memory_map[256];

// system.h

extern t_bitmap bitmap;
extern t_snd snd;
extern uint32_t mcycles_vdp;
extern uint8_t system_hw;
extern uint8_t system_bios;
extern uint32_t system_clock;
extern int16_t SVP_cycles; 
extern uint8_t pause_b;
extern EQSTATE eq[2];
extern int16_t llp,rrp;

// vdp.h

extern uint8_t ALIGNED_(4) sat[0x400];     /* Internal copy of sprite attribute table */
extern uint8_t ALIGNED_(4) vram[0x10000];  /* Video RAM (64K x 8-bit) */
extern uint8_t ALIGNED_(4) cram[0x80];     /* On-chip color RAM (64 x 9-bit) */
extern uint8_t ALIGNED_(4) vsram[0x80];    /* On-chip vertical scroll RAM (40 x 11-bit) */
extern uint8_t reg[0x20];                  /* Internal VDP registers (23 x 8-bit) */
extern uint8_t hint_pending;               /* 0= Line interrupt is pending */
extern uint8_t vint_pending;               /* 1= Frame interrupt is pending */
extern uint16_t status;                    /* VDP status flags */
extern uint32_t dma_length;                /* DMA remaining length */
extern uint32_t dma_endCycles;             /* DMA end cycle */
extern uint8_t dma_type;                   /* DMA mode */
extern uint16_t ntab;                      /* Name table A base address */
extern uint16_t ntbb;                      /* Name table B base address */
extern uint16_t ntwb;                      /* Name table W base address */
extern uint16_t satb;                      /* Sprite attribute table base address */
extern uint16_t hscb;                      /* Horizontal scroll table base address */
extern uint8_t bg_name_dirty[0x800];       /* 1= This pattern is dirty */
extern uint16_t bg_name_list[0x800];       /* List of modified pattern indices */
extern uint16_t bg_list_index;             /* # of modified patterns in list */
extern uint8_t hscroll_mask;               /* Horizontal Scrolling line mask */
extern uint8_t playfield_shift;            /* Width of planes A, B (in bits) */
extern uint8_t playfield_col_mask;         /* Playfield column mask */
extern uint16_t playfield_row_mask;        /* Playfield row mask */
extern uint16_t vscroll;                   /* Latched vertical scroll value */
extern uint8_t odd_frame;                  /* 1: odd field, 0: even field */
extern uint8_t im2_flag;                   /* 1= Interlace mode 2 is being used */
extern uint8_t interlaced;                 /* 1: Interlaced mode 1 or 2 */
extern uint8_t vdp_pal;                    /* 1: PAL , 0: NTSC (default) */
extern uint8_t h_counter;                  /* Horizontal counter */
extern uint16_t v_counter;                 /* Vertical counter */
extern uint16_t vc_max;                    /* Vertical counter overflow value */
extern uint16_t lines_per_frame;           /* PAL: 313 lines, NTSC: 262 lines */
extern uint16_t max_sprite_pixels;         /* Max. sprites pixels per line (parsing & rendering) */
extern uint32_t fifo_cycles[4];            /* VDP FIFO read-out cycles */
extern uint32_t hvc_latch;                 /* latched HV counter */
extern uint32_t vint_cycle;                /* VINT occurence cycle */
extern const uint8_t *hctab;               /* pointer to H Counter table */

extern uint8_t border;            /* Border color index */
extern uint8_t pending;           /* Pending write flag */
extern uint8_t code;              /* Code register */
extern uint16_t addr;             /* Address register */
extern uint16_t addr_latch;       /* Latched A15, A14 of address */
extern uint16_t sat_base_mask;    /* Base bits of SAT */
extern uint16_t sat_addr_mask;    /* Index bits of SAT */
extern uint16_t dma_src;          /* DMA source address */
extern int dmafill;             /* DMA Fill pending flag */
extern int cached_write;        /* 2nd part of 32-bit CTRL port write (Genesis mode) or LSB of CRAM data (Game Gear mode) */
extern uint16_t fifo[4];          /* FIFO ring-buffer */
extern int fifo_idx;            /* FIFO write index */
extern int fifo_byte_access;    /* FIFO byte access flag */
extern int *fifo_timing;        /* FIFO slots timing table */
extern int hblank_start_cycle;  /* HBLANK flag set cycle */
extern int hblank_end_cycle;    /* HBLANK flag clear cycle */

// vdp_render.h

extern struct clip_t clip[2];
extern uint8_t ALIGNED_(4) bg_pattern_cache[0x80000]; /* Cached and flipped patterns */
extern uint8_t name_lut[0x400]; /* Sprite pattern name offset look-up table (Mode 5) */
extern uint32_t bp_lut[0x10000]; /* Bitplane to packed pixel look-up table (Mode 4) */
extern uint8_t lut[LUT_MAX][LUT_SIZE]; /* Layer priority pixel look-up tables */
extern PIXEL_OUT_T pixel[0x100]; /* Output pixel data look-up tables*/
extern PIXEL_OUT_T pixel_lut[3][0x200];
extern PIXEL_OUT_T pixel_lut_m4[0x40];
extern uint8_t linebuf[2][0x200]; /* Background & Sprite line buffers */
extern uint8_t spr_ovr; /* Sprite limit flag */
extern object_info_t obj_info[2][MAX_SPRITES_PER_LINE];
extern uint8_t object_count[2]; /* Sprite Counter */
extern uint16_t spr_col; /* Sprite Collision Info */