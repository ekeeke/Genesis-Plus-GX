#include <string.h>
#include "state.h"

// cart_hw/svp/svp.hc

// Special case, as svp is inside the cart.rom allocation

// cart_hw/svp/svp16.h

ssp1601_t *ssp = NULL;
unsigned short *PC;
int g_cycles;

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

// cd_hw/cdc.h

void (*dma_w)(unsigned int length);  /* active DMA callback */
void (*halted_dma_w)(unsigned int length);  /* halted DMA callback */

// cd_hw/cdd.c

#if defined(USE_LIBCHDR)
chd_file *libCHDRfile;
#endif

cdStream *cdTrackStreams[100];
cdStream *cdTocStream;

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

// input_hw/terebi_oekaki.c

struct tablet_t tablet;

// input_hw/xe_1ap.c

struct xe_1ap_t xe_1ap[2];

// m68k/m68k.c

m68ki_cpu_core m68k;
m68ki_cpu_core s68k;

// m68k/m68kcpu.c

int m68k_irq_latency;

// m68k/s68kcpu.c

int s68k_irq_latency;

// sound/psg.c

struct psg_t psg;

// sound/sound.c

#if defined(HAVE_YM3438_CORE) || defined(HAVE_OPLL_CORE)
int fm_buffer[1080 * 2 * 24]; // FM output buffer (large enough to hold a whole frame at original chips rate) 
#else
int fm_buffer[1080 * 2];
#endif

int fm_last[2];
int *fm_ptr;
int fm_cycles_ratio; // Cycle-accurate FM samples
int fm_cycles_start;
int fm_cycles_count;
int fm_cycles_busy;

#ifdef HAVE_YM3438_CORE
ym3438_t ym3438;
short ym3438_accm[24][2];
int ym3438_sample[2];
int ym3438_cycles;
#endif

#ifdef HAVE_OPLL_CORE
opll_t opll;
int opll_accm[18][2];
int opll_sample;
int opll_cycles;
int opll_status;
#endif

// sound/ym2413.h

signed int output[2];
uint32_t  LFO_AM;
int32_t  LFO_PM;
YM2413 ym2413; /* emulated chip */

// sound/ym2612.c

YM2612 ym2612; /* emulated chip */
int32_t  m2,c1,c2;   /* current chip state - Phase Modulation input for operators 2,3,4 */
int32_t  mem;        /* one sample delay memory */
int32_t  out_fm[6];  /* outputs of working channels */
uint32_t op_mask[8][4];  /* operator output bitmasking (DAC quantization) */
int chip_type = YM2612_DISCRETE; /* chip type */

// z80/z80.c

Z80_Regs Z80;
uint8_t z80_last_fetch;
unsigned char *z80_readmap[64];
unsigned char *z80_writemap[64];
uint32_t EA;
uint8_t SZ[256];       /* zero and sign flags */
uint8_t SZ_BIT[256];   /* zero, sign and parity/overflow (=zero) flags for BIT opcode */
uint8_t SZP[256];      /* zero, sign and parity flags */
uint8_t SZHV_inc[256]; /* zero, sign, half carry and overflow flags INC r8 */
uint8_t SZHV_dec[256]; /* zero, sign, half carry and overflow flags DEC r8 */
uint8_t SZHVC_add[2*256*256]; /* flags for ADD opcode */
uint8_t SZHVC_sub[2*256*256]; /* flags for SUB opcode */

#ifdef Z80_OVERCLOCK_SHIFT
uint32_t z80_cycle_ratio;
#endif

// genesis.c

// Cartdrigde / CD information
#ifdef USE_DYNAMIC_ALLOC
external_t *ext;
#else
external_t ext;
#endif

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


size_t saveState(uint8_t* buffer)
{
  size_t pos = 0;
  
  // cart_hw/svp/svp16.h

  // Special treatment for SVP, since it is stored inside the ROM that we otherwise do not store
  if (svp != NULL)
  {
   if (buffer != NULL) { memcpy(&buffer[pos], svp                  , sizeof(svp_t               )); } pos += sizeof(svp_t               );
   if (buffer != NULL) { memcpy(&buffer[pos], ssp                  , sizeof(ssp1601_t           )); } pos += sizeof(ssp1601_t           );

   // SSP PC, store as offset to svp->iram_rom
   int64_t SSPPC = GET_PC();
   if (buffer != NULL) { memcpy(&buffer[pos], &SSPPC               , sizeof(SSPPC               )); } pos += sizeof(SSPPC               );
   if (buffer != NULL) { memcpy(&buffer[pos], &g_cycles            , sizeof(g_cycles            )); } pos += sizeof(g_cycles            );
  }

  // cart_hw/areplay.c

  if (buffer != NULL) { memcpy(&buffer[pos], &action_replay       , sizeof(action_replay       )); } pos += sizeof(action_replay       );

  // cart_hw/eeprom_93c.c

  if (buffer != NULL) { memcpy(&buffer[pos], &eeprom_93c          , sizeof(eeprom_93c          )); } pos += sizeof(eeprom_93c          );

  // cart_hw/eeprom_i2c.c

  if (buffer != NULL) { memcpy(&buffer[pos], &eeprom_i2c          , sizeof(eeprom_i2c          )); } pos += sizeof(eeprom_i2c          );

  // cart_hw/eeprom_spi.c

  if (buffer != NULL) { memcpy(&buffer[pos], &spi_eeprom          , sizeof(spi_eeprom          )); } pos += sizeof(spi_eeprom          );

  // cart_hw/ggenie.c

  if (buffer != NULL) { memcpy(&buffer[pos], &ggenie              , sizeof(ggenie              )); } pos += sizeof(ggenie              );

  // cart_hw/megasd.c

  if (buffer != NULL) { memcpy(&buffer[pos], &megasd_hw           , sizeof(megasd_hw           )); } pos += sizeof(megasd_hw           );

  // cart_hw/sram.c

  if (buffer != NULL) { memcpy(&buffer[pos], &sram                , sizeof(sram                )); } pos += sizeof(sram                );

  // cd_hw/cdd.c

  if (system_hw == SYSTEM_MCD)
  {
    #ifdef USE_DYNAMIC_ALLOC
     cd_hw_t* cdData = (cd_hw_t*)ext;
    #else
     cd_hw_t* cdData = (cd_hw_t*)&ext;
    #endif

    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->prg_ram            , sizeof(cdData->prg_ram            )); } pos += sizeof(cdData->prg_ram           );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->word_ram           , sizeof(cdData->word_ram           )); } pos += sizeof(cdData->word_ram          );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->word_ram_2M        , sizeof(cdData->word_ram_2M        )); } pos += sizeof(cdData->word_ram_2M       );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->bram               , sizeof(cdData->bram               )); } pos += sizeof(cdData->bram              );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->regs               , sizeof(cdData->regs               )); } pos += sizeof(cdData->regs              );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->cycles             , sizeof(cdData->cycles             )); } pos += sizeof(cdData->cycles            );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->cycles_per_line    , sizeof(cdData->cycles_per_line    )); } pos += sizeof(cdData->cycles_per_line   );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->stopwatch          , sizeof(cdData->stopwatch          )); } pos += sizeof(cdData->stopwatch         );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->timer              , sizeof(cdData->timer              )); } pos += sizeof(cdData->timer             );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->pending            , sizeof(cdData->pending            )); } pos += sizeof(cdData->pending           );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->dmna               , sizeof(cdData->dmna               )); } pos += sizeof(cdData->dmna              );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->type               , sizeof(cdData->type               )); } pos += sizeof(cdData->type              );
    // if (buffer != NULL) { memcpy(&buffer[pos], &cdData->gfx_hw             , sizeof(cdData->gfx_hw             )); } pos += sizeof(cdData->gfx_hw            );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->cdc_hw             , sizeof(cdData->cdc_hw             )); } pos += sizeof(cdData->cdc_hw            );
    if (buffer != NULL) { memcpy(&buffer[pos], &cdData->cdd_hw             , sizeof(cdData->cdd_hw             )); } pos += sizeof(cdData->cdd_hw            );
    // if (buffer != NULL) { memcpy(&buffer[pos], &cdData->pcm_hw             , sizeof(cdData->pcm_hw             )); } pos += sizeof(cdData->pcm_hw            );       
  }

  // input_hw/activator.c

  if (buffer != NULL) { memcpy(&buffer[pos], &activator           , sizeof(activator           )); } pos += sizeof(activator           );

  // input_hw/gamepad.c
  
  if (buffer != NULL) { memcpy(&buffer[pos], &gamepad             , sizeof(gamepad             )); } pos += sizeof(gamepad             );
  if (buffer != NULL) { memcpy(&buffer[pos], &flipflop            , sizeof(flipflop            )); } pos += sizeof(flipflop            );
  if (buffer != NULL) { memcpy(&buffer[pos], &latch               , sizeof(latch               )); } pos += sizeof(latch               );

  // input_hw/graphic_board.c

  if (buffer != NULL) { memcpy(&buffer[pos], &board               , sizeof(board               )); } pos += sizeof(board               );

  // input_hw/input.c
  
  if (buffer != NULL) { memcpy(&buffer[pos], &input               , sizeof(input               )); } pos += sizeof(input               );
  if (buffer != NULL) { memcpy(&buffer[pos], &old_system          , sizeof(old_system          )); } pos += sizeof(old_system          );

  // input_hw/lightgun.c

  if (buffer != NULL) { memcpy(&buffer[pos], &lightgun            , sizeof(lightgun            )); } pos += sizeof(lightgun            );

  // input_hw/mouse.c

  if (buffer != NULL) { memcpy(&buffer[pos], &mouse               , sizeof(mouse               )); } pos += sizeof(mouse               );

  // input_hw/paddle.c

  if (buffer != NULL) { memcpy(&buffer[pos], &paddle              , sizeof(paddle              )); } pos += sizeof(paddle              );

  // input_hw/sportspad.c

  if (buffer != NULL) { memcpy(&buffer[pos], &sportspad           , sizeof(sportspad           )); } pos += sizeof(sportspad           );

  // input_hw/teamplayer.c

  if (buffer != NULL) { memcpy(&buffer[pos], &teamplayer          , sizeof(teamplayer          )); } pos += sizeof(teamplayer          );
  
  // input_hw/terebi_oekaki.c

  if (buffer != NULL) { memcpy(&buffer[pos], &tablet              , sizeof(tablet              )); } pos += sizeof(tablet              );

  // input_hw/xe_1ap.c

  if (buffer != NULL) { memcpy(&buffer[pos], &xe_1ap              , sizeof(xe_1ap              )); } pos += sizeof(xe_1ap              );

  // m68k/m68k.c

  if (buffer != NULL) { memcpy(&buffer[pos], &m68k                , sizeof(m68k                )); } pos += sizeof(m68k                );
  if (buffer != NULL) { memcpy(&buffer[pos], &s68k                , sizeof(s68k                )); } pos += sizeof(s68k                );

  // m68k/m68kcpu.c

  if (buffer != NULL) { memcpy(&buffer[pos], &m68k_irq_latency    , sizeof(m68k_irq_latency    )); } pos += sizeof(m68k_irq_latency    );

  // m68k/s68kcpu.c

  if (buffer != NULL) { memcpy(&buffer[pos], &s68k_irq_latency    , sizeof(s68k_irq_latency    )); } pos += sizeof(s68k_irq_latency    );

  // sound/psg.c

  if (buffer != NULL) { memcpy(&buffer[pos], & psg                , sizeof( psg                )); } pos += sizeof( psg                );
  
  // sound/sound.c

  if (buffer != NULL) { memcpy(&buffer[pos], &fm_buffer           , sizeof(fm_buffer           )); } pos += sizeof(fm_buffer           );
  if (buffer != NULL) { memcpy(&buffer[pos], &fm_last             , sizeof(fm_last             )); } pos += sizeof(fm_last             );
  //if (buffer != NULL) { memcpy(&buffer[pos], fm_ptr               , sizeof(fm_ptr             )); } pos += sizeof(*fm_ptr             );
  if (buffer != NULL) { memcpy(&buffer[pos], &fm_cycles_ratio     , sizeof(fm_cycles_ratio     )); } pos += sizeof(fm_cycles_ratio     );
  if (buffer != NULL) { memcpy(&buffer[pos], &fm_cycles_start     , sizeof(fm_cycles_start     )); } pos += sizeof(fm_cycles_start     );
  if (buffer != NULL) { memcpy(&buffer[pos], &fm_cycles_count     , sizeof(fm_cycles_count     )); } pos += sizeof(fm_cycles_count     );
  if (buffer != NULL) { memcpy(&buffer[pos], &fm_cycles_busy      , sizeof(fm_cycles_busy      )); } pos += sizeof(fm_cycles_busy      );

  #ifdef HAVE_YM3438_CORE
  if (buffer != NULL) { memcpy(&buffer[pos], &ym3438              , sizeof(ym3438              )); } pos += sizeof(ym3438              );
  if (buffer != NULL) { memcpy(&buffer[pos], &ym3438_accm         , sizeof(ym3438_accm         )); } pos += sizeof(ym3438_accm         );
  if (buffer != NULL) { memcpy(&buffer[pos], &ym3438_sample       , sizeof(ym3438_sample       )); } pos += sizeof(ym3438_sample       );
  if (buffer != NULL) { memcpy(&buffer[pos], &ym3438_cycles       , sizeof(ym3438_cycles       )); } pos += sizeof(ym3438_cycles       );
  #endif

  #ifdef HAVE_OPLL_CORE
  if (buffer != NULL) { memcpy(&buffer[pos], &opll                , sizeof(opll                )); } pos += sizeof(opll                );
  if (buffer != NULL) { memcpy(&buffer[pos], &opll_accm           , sizeof(opll_accm           )); } pos += sizeof(opll_accm           );
  if (buffer != NULL) { memcpy(&buffer[pos], &opll_sample         , sizeof(opll_sample         )); } pos += sizeof(opll_sample         );
  if (buffer != NULL) { memcpy(&buffer[pos], &opll_cycles         , sizeof(opll_cycles         )); } pos += sizeof(opll_cycles         );
  if (buffer != NULL) { memcpy(&buffer[pos], &opll_status         , sizeof(opll_status         )); } pos += sizeof(opll_status         );
  #endif

  // sound/ym2413.h

  if (buffer != NULL) { memcpy(&buffer[pos], &output              , sizeof(output              )); } pos += sizeof(output              );
  if (buffer != NULL) { memcpy(&buffer[pos], &LFO_AM              , sizeof(LFO_AM              )); } pos += sizeof(LFO_AM              );
  if (buffer != NULL) { memcpy(&buffer[pos], &LFO_PM              , sizeof(LFO_PM              )); } pos += sizeof(LFO_PM              );
  if (buffer != NULL) { memcpy(&buffer[pos], &ym2413              , sizeof(ym2413              )); } pos += sizeof(ym2413              );

  // sound/ym2612.c

  if (buffer != NULL) { memcpy(&buffer[pos], &ym2612              , sizeof(ym2612              )); } pos += sizeof(ym2612              );
  if (buffer != NULL) { memcpy(&buffer[pos], &m2                  , sizeof(m2                  )); } pos += sizeof(m2                  );
  if (buffer != NULL) { memcpy(&buffer[pos], &c1                  , sizeof(c1                  )); } pos += sizeof(c1                  );
  if (buffer != NULL) { memcpy(&buffer[pos], &c2                  , sizeof(c2                  )); } pos += sizeof(c2                  );
  if (buffer != NULL) { memcpy(&buffer[pos], &mem                 , sizeof(mem                 )); } pos += sizeof(mem                 );
  if (buffer != NULL) { memcpy(&buffer[pos], &out_fm              , sizeof(out_fm              )); } pos += sizeof(out_fm              );
  if (buffer != NULL) { memcpy(&buffer[pos], &op_mask             , sizeof(op_mask             )); } pos += sizeof(op_mask             );
  if (buffer != NULL) { memcpy(&buffer[pos], &chip_type           , sizeof(chip_type           )); } pos += sizeof(chip_type           );

  // z80/z80.c

  if (buffer != NULL) { memcpy(&buffer[pos], &Z80                 , sizeof(Z80                 )); } pos += sizeof(Z80                 );
  if (buffer != NULL) { memcpy(&buffer[pos], &z80_last_fetch      , sizeof(z80_last_fetch      )); } pos += sizeof(z80_last_fetch      );
  if (buffer != NULL) { memcpy(&buffer[pos], &z80_readmap         , sizeof(z80_readmap         )); } pos += sizeof(z80_readmap         );
  if (buffer != NULL) { memcpy(&buffer[pos], &z80_writemap        , sizeof(z80_writemap        )); } pos += sizeof(z80_writemap        );
  if (buffer != NULL) { memcpy(&buffer[pos], &EA                  , sizeof(EA                  )); } pos += sizeof(EA                  );
  if (buffer != NULL) { memcpy(&buffer[pos], &SZ                  , sizeof(SZ                  )); } pos += sizeof(SZ                  );
  if (buffer != NULL) { memcpy(&buffer[pos], &SZ_BIT              , sizeof(SZ_BIT              )); } pos += sizeof(SZ_BIT              );
  if (buffer != NULL) { memcpy(&buffer[pos], &SZP                 , sizeof(SZP                 )); } pos += sizeof(SZP                 );
  if (buffer != NULL) { memcpy(&buffer[pos], &SZHV_inc            , sizeof(SZHV_inc            )); } pos += sizeof(SZHV_inc            );
  if (buffer != NULL) { memcpy(&buffer[pos], &SZHV_dec            , sizeof(SZHV_dec            )); } pos += sizeof(SZHV_dec            );
  if (buffer != NULL) { memcpy(&buffer[pos], &SZHVC_add           , sizeof(SZHVC_add           )); } pos += sizeof(SZHVC_add           );
  if (buffer != NULL) { memcpy(&buffer[pos], &SZHVC_sub           , sizeof(SZHVC_sub           )); } pos += sizeof(SZHVC_sub           );
  
  #ifdef Z80_OVERCLOCK_SHIFT
  if (buffer != NULL) { memcpy(&buffer[pos], &z80_cycle_ratio     , sizeof(z80_cycle_ratio     )); } pos += sizeof(z80_cycle_ratio     );
  #endif

  // genesis.c

  if (buffer != NULL) { memcpy(&buffer[pos], &boot_rom            , sizeof(boot_rom            )); } pos += sizeof(boot_rom            );
  if (buffer != NULL) { memcpy(&buffer[pos], &work_ram            , sizeof(work_ram            )); } pos += sizeof(work_ram            );
  if (buffer != NULL) { memcpy(&buffer[pos], &zram                , sizeof(zram                )); } pos += sizeof(zram                );
  if (buffer != NULL) { memcpy(&buffer[pos], &zbank               , sizeof(zbank               )); } pos += sizeof(zbank               );
  if (buffer != NULL) { memcpy(&buffer[pos], &zstate              , sizeof(zstate              )); } pos += sizeof(zstate              );
  if (buffer != NULL) { memcpy(&buffer[pos], &pico_current        , sizeof(pico_current        )); } pos += sizeof(pico_current        );
  if (buffer != NULL) { memcpy(&buffer[pos], &tmss                , sizeof(tmss                )); } pos += sizeof(tmss                );

  // io_ctrl.c

  if (buffer != NULL) { memcpy(&buffer[pos], &io_reg              , sizeof(io_reg              )); } pos += sizeof(io_reg              );
  if (buffer != NULL) { memcpy(&buffer[pos], &region_code         , sizeof(region_code         )); } pos += sizeof(region_code         );
  if (buffer != NULL) { memcpy(&buffer[pos], &port                , sizeof(port                )); } pos += sizeof(port                );

  // load_rom.c

  if (buffer != NULL) { memcpy(&buffer[pos], &rominfo             , sizeof(rominfo             )); } pos += sizeof(rominfo             );
  if (buffer != NULL) { memcpy(&buffer[pos], &romtype             , sizeof(romtype             )); } pos += sizeof(romtype             );
  if (buffer != NULL) { memcpy(&buffer[pos], &rom_region          , sizeof(rom_region          )); } pos += sizeof(rom_region          );

  // membnk.c

  if (buffer != NULL) { memcpy(&buffer[pos], &zbank_memory_map    , sizeof(zbank_memory_map    )); } pos += sizeof(zbank_memory_map    );

  // system.c

  // if (buffer != NULL) { memcpy(&buffer[pos], &bitmap              , sizeof(bitmap              )); } pos += sizeof(bitmap              );
  // if (buffer != NULL) { memcpy(&buffer[pos], &snd                 , sizeof(snd                 )); } pos += sizeof(snd                 );
  // if (buffer != NULL) { memcpy(&buffer[pos], &mcycles_vdp         , sizeof(mcycles_vdp         )); } pos += sizeof(mcycles_vdp         );
  // if (buffer != NULL) { memcpy(&buffer[pos], &system_hw           , sizeof(system_hw           )); } pos += sizeof(system_hw           );
  // if (buffer != NULL) { memcpy(&buffer[pos], &system_bios         , sizeof(system_bios         )); } pos += sizeof(system_bios         );
  // if (buffer != NULL) { memcpy(&buffer[pos], &system_clock        , sizeof(system_clock        )); } pos += sizeof(system_clock        );
  // if (buffer != NULL) { memcpy(&buffer[pos], &SVP_cycles          , sizeof(SVP_cycles          )); } pos += sizeof(SVP_cycles          );
  if (buffer != NULL) { memcpy(&buffer[pos], &pause_b             , sizeof(pause_b             )); } pos += sizeof(pause_b             );
  // if (buffer != NULL) { memcpy(&buffer[pos], &eq                  , sizeof(eq                  )); } pos += sizeof(eq                  );
  // if (buffer != NULL) { memcpy(&buffer[pos], &llp                 , sizeof(llp                 )); } pos += sizeof(llp                 );
  // if (buffer != NULL) { memcpy(&buffer[pos], &rrp                 , sizeof(rrp                 )); } pos += sizeof(rrp                 );

  // vdp.c

  if (buffer != NULL) { memcpy(&buffer[pos], &sat                 , sizeof(sat                 )); } pos += sizeof(sat                 );
  if (buffer != NULL) { memcpy(&buffer[pos], &vram                , sizeof(vram                )); } pos += sizeof(vram                );
  if (buffer != NULL) { memcpy(&buffer[pos], &cram                , sizeof(cram                )); } pos += sizeof(cram                );
  if (buffer != NULL) { memcpy(&buffer[pos], &vsram               , sizeof(vsram               )); } pos += sizeof(vsram               );
  if (buffer != NULL) { memcpy(&buffer[pos], &reg                 , sizeof(reg                 )); } pos += sizeof(reg                 );
  if (buffer != NULL) { memcpy(&buffer[pos], &hint_pending        , sizeof(hint_pending        )); } pos += sizeof(hint_pending        );
  if (buffer != NULL) { memcpy(&buffer[pos], &vint_pending        , sizeof(vint_pending        )); } pos += sizeof(vint_pending        );
  if (buffer != NULL) { memcpy(&buffer[pos], &status              , sizeof(status              )); } pos += sizeof(status              );
  if (buffer != NULL) { memcpy(&buffer[pos], &dma_length          , sizeof(dma_length          )); } pos += sizeof(dma_length          );
  if (buffer != NULL) { memcpy(&buffer[pos], &dma_endCycles       , sizeof(dma_endCycles       )); } pos += sizeof(dma_endCycles       );
  if (buffer != NULL) { memcpy(&buffer[pos], &dma_type            , sizeof(dma_type            )); } pos += sizeof(dma_type            );
  if (buffer != NULL) { memcpy(&buffer[pos], &ntab                , sizeof(ntab                )); } pos += sizeof(ntab                );
  if (buffer != NULL) { memcpy(&buffer[pos], &ntbb                , sizeof(ntbb                )); } pos += sizeof(ntbb                );
  if (buffer != NULL) { memcpy(&buffer[pos], &ntwb                , sizeof(ntwb                )); } pos += sizeof(ntwb                );
  if (buffer != NULL) { memcpy(&buffer[pos], &satb                , sizeof(satb                )); } pos += sizeof(satb                );
  if (buffer != NULL) { memcpy(&buffer[pos], &hscb                , sizeof(hscb                )); } pos += sizeof(hscb                );
  if (buffer != NULL) { memcpy(&buffer[pos], &bg_name_dirty       , sizeof(bg_name_dirty       )); } pos += sizeof(bg_name_dirty       );
  if (buffer != NULL) { memcpy(&buffer[pos], &bg_name_list        , sizeof(bg_name_list        )); } pos += sizeof(bg_name_list        );
  if (buffer != NULL) { memcpy(&buffer[pos], &bg_list_index       , sizeof(bg_list_index       )); } pos += sizeof(bg_list_index       );
  if (buffer != NULL) { memcpy(&buffer[pos], &hscroll_mask        , sizeof(hscroll_mask        )); } pos += sizeof(hscroll_mask        );
  if (buffer != NULL) { memcpy(&buffer[pos], &playfield_shift     , sizeof(playfield_shift     )); } pos += sizeof(playfield_shift     );
  if (buffer != NULL) { memcpy(&buffer[pos], &playfield_col_mask  , sizeof(playfield_col_mask  )); } pos += sizeof(playfield_col_mask  );
  if (buffer != NULL) { memcpy(&buffer[pos], &playfield_row_mask  , sizeof(playfield_row_mask  )); } pos += sizeof(playfield_row_mask  );
  if (buffer != NULL) { memcpy(&buffer[pos], &vscroll             , sizeof(vscroll             )); } pos += sizeof(vscroll             );
  if (buffer != NULL) { memcpy(&buffer[pos], &odd_frame           , sizeof(odd_frame           )); } pos += sizeof(odd_frame           );
  if (buffer != NULL) { memcpy(&buffer[pos], &im2_flag            , sizeof(im2_flag            )); } pos += sizeof(im2_flag            );
  if (buffer != NULL) { memcpy(&buffer[pos], &interlaced          , sizeof(interlaced          )); } pos += sizeof(interlaced          );
  if (buffer != NULL) { memcpy(&buffer[pos], &vdp_pal             , sizeof(vdp_pal             )); } pos += sizeof(vdp_pal             );
  if (buffer != NULL) { memcpy(&buffer[pos], &h_counter           , sizeof(h_counter           )); } pos += sizeof(h_counter           );
  if (buffer != NULL) { memcpy(&buffer[pos], &v_counter           , sizeof(v_counter           )); } pos += sizeof(v_counter           );
  if (buffer != NULL) { memcpy(&buffer[pos], &vc_max              , sizeof(vc_max              )); } pos += sizeof(vc_max              );
  if (buffer != NULL) { memcpy(&buffer[pos], &lines_per_frame     , sizeof(lines_per_frame     )); } pos += sizeof(lines_per_frame     );
  if (buffer != NULL) { memcpy(&buffer[pos], &max_sprite_pixels   , sizeof(max_sprite_pixels   )); } pos += sizeof(max_sprite_pixels   );
  if (buffer != NULL) { memcpy(&buffer[pos], &fifo_cycles         , sizeof(fifo_cycles         )); } pos += sizeof(fifo_cycles         );
  if (buffer != NULL) { memcpy(&buffer[pos], &hvc_latch           , sizeof(hvc_latch           )); } pos += sizeof(hvc_latch           );
  if (buffer != NULL) { memcpy(&buffer[pos], &vint_cycle          , sizeof(vint_cycle          )); } pos += sizeof(vint_cycle          );
  // if (buffer != NULL) { memcpy(&buffer[pos], &hctab               , sizeof(hctab               )); } pos += sizeof(hctab               );
  if (buffer != NULL) { memcpy(&buffer[pos], &border              , sizeof(border              )); } pos += sizeof(border              );
  if (buffer != NULL) { memcpy(&buffer[pos], &pending             , sizeof(pending             )); } pos += sizeof(pending             );
  if (buffer != NULL) { memcpy(&buffer[pos], &code                , sizeof(code                )); } pos += sizeof(code                );
  if (buffer != NULL) { memcpy(&buffer[pos], &addr                , sizeof(addr                )); } pos += sizeof(addr                );
  if (buffer != NULL) { memcpy(&buffer[pos], &addr_latch          , sizeof(addr_latch          )); } pos += sizeof(addr_latch          );
  if (buffer != NULL) { memcpy(&buffer[pos], &sat_base_mask       , sizeof(sat_base_mask       )); } pos += sizeof(sat_base_mask       );
  if (buffer != NULL) { memcpy(&buffer[pos], &sat_addr_mask       , sizeof(sat_addr_mask       )); } pos += sizeof(sat_addr_mask       );
  if (buffer != NULL) { memcpy(&buffer[pos], &dma_src             , sizeof(dma_src             )); } pos += sizeof(dma_src             );
  if (buffer != NULL) { memcpy(&buffer[pos], &dmafill             , sizeof(dmafill             )); } pos += sizeof(dmafill             );
  if (buffer != NULL) { memcpy(&buffer[pos], &cached_write        , sizeof(cached_write        )); } pos += sizeof(cached_write        );
  if (buffer != NULL) { memcpy(&buffer[pos], &fifo                , sizeof(fifo                )); } pos += sizeof(fifo                );
  if (buffer != NULL) { memcpy(&buffer[pos], &fifo_idx            , sizeof(fifo_idx            )); } pos += sizeof(fifo_idx            );
  if (buffer != NULL) { memcpy(&buffer[pos], &fifo_byte_access    , sizeof(fifo_byte_access    )); } pos += sizeof(fifo_byte_access    );
  // if (buffer != NULL) { memcpy(&buffer[pos], &fifo_timing         , sizeof(fifo_timing         )); } pos += sizeof(fifo_timing         );
  if (buffer != NULL) { memcpy(&buffer[pos], &hblank_start_cycle  , sizeof(hblank_start_cycle  )); } pos += sizeof(hblank_start_cycle  );
  if (buffer != NULL) { memcpy(&buffer[pos], &hblank_end_cycle    , sizeof(hblank_end_cycle    )); } pos += sizeof(hblank_end_cycle    );

  // vdp_render.c

  // if (buffer != NULL) { memcpy(&buffer[pos], &clip                , sizeof(clip                )); } pos += sizeof(clip                );
  // if (buffer != NULL) { memcpy(&buffer[pos], &bg_pattern_cache    , sizeof(bg_pattern_cache    )); } pos += sizeof(bg_pattern_cache    );
  // if (buffer != NULL) { memcpy(&buffer[pos], &name_lut            , sizeof(name_lut            )); } pos += sizeof(name_lut            );
  // if (buffer != NULL) { memcpy(&buffer[pos], &bp_lut              , sizeof(bp_lut              )); } pos += sizeof(bp_lut              );
  // if (buffer != NULL) { memcpy(&buffer[pos], &lut                 , sizeof(lut                 )); } pos += sizeof(lut                 );
  // if (buffer != NULL) { memcpy(&buffer[pos], &pixel               , sizeof(pixel               )); } pos += sizeof(pixel               );
  // if (buffer != NULL) { memcpy(&buffer[pos], &pixel_lut           , sizeof(pixel_lut           )); } pos += sizeof(pixel_lut           );
  // if (buffer != NULL) { memcpy(&buffer[pos], &pixel_lut_m4        , sizeof(pixel_lut_m4        )); } pos += sizeof(pixel_lut_m4        );
  // if (buffer != NULL) { memcpy(&buffer[pos], &linebuf             , sizeof(linebuf             )); } pos += sizeof(linebuf             );
  // if (buffer != NULL) { memcpy(&buffer[pos], &spr_ovr             , sizeof(spr_ovr             )); } pos += sizeof(spr_ovr             );
  // if (buffer != NULL) { memcpy(&buffer[pos], &obj_info            , sizeof(obj_info            )); } pos += sizeof(obj_info            );
  // if (buffer != NULL) { memcpy(&buffer[pos], &object_count        , sizeof(object_count        )); } pos += sizeof(object_count        );
  // if (buffer != NULL) { memcpy(&buffer[pos], &spr_col             , sizeof(spr_col             )); } pos += sizeof(spr_col             );

  return pos;
}

void loadState(const uint8_t* buffer)
{
  size_t pos = 0;

  // cart_hw/svp/svp16.h

  if (svp != NULL)
  {
   // Special treatment for SVP, since it is stored inside the ROM that we otherwise do not store
   if (buffer != NULL) { memcpy(svp                  , &buffer[pos],  sizeof(svp_t               )); } pos += sizeof(svp_t               );
   if (buffer != NULL) { memcpy(ssp                  , &buffer[pos],  sizeof(ssp1601_t           )); } pos += sizeof(ssp1601_t           );

   // SSP PC, store as offset to svp->iram_rom
   int64_t SSPPC = 0;
   if (buffer != NULL) { memcpy(&SSPPC               , &buffer[pos],  sizeof(SSPPC              )); } pos += sizeof(SSPPC               );
   SET_PC(SSPPC);

   if (buffer != NULL) { memcpy(&g_cycles            , &buffer[pos],  sizeof(g_cycles            )); } pos += sizeof(g_cycles            );
  }
  
  // cart_hw/areplay.c

  if (buffer != NULL) { memcpy(&action_replay       , &buffer[pos],  sizeof(action_replay       )); } pos += sizeof(action_replay       );

  // cart_hw/eeprom_93c.c

  if (buffer != NULL) { memcpy(&eeprom_93c          , &buffer[pos],  sizeof(eeprom_93c          )); } pos += sizeof(eeprom_93c          );

  // cart_hw/eeprom_i2c.c

  if (buffer != NULL) { memcpy(&eeprom_i2c          , &buffer[pos],  sizeof(eeprom_i2c          )); } pos += sizeof(eeprom_i2c          );

  // cart_hw/eeprom_spi.c

  if (buffer != NULL) { memcpy(&spi_eeprom          , &buffer[pos],  sizeof(spi_eeprom          )); } pos += sizeof(spi_eeprom          );

  // cart_hw/ggenie.c

  if (buffer != NULL) { memcpy(&ggenie              , &buffer[pos],  sizeof(ggenie              )); } pos += sizeof(ggenie              );

  // cart_hw/megasd.c

  if (buffer != NULL) { memcpy(&megasd_hw           , &buffer[pos],  sizeof(megasd_hw           )); } pos += sizeof(megasd_hw           );

  // cart_hw/sram.c

  if (buffer != NULL) { memcpy(&sram                , &buffer[pos],  sizeof(sram                )); } pos += sizeof(sram                );

  // cd_hw/cdd.c

    if (system_hw == SYSTEM_MCD)
  {
    #ifdef USE_DYNAMIC_ALLOC
     cd_hw_t* cdData = (cd_hw_t*)ext;
    #else
     cd_hw_t* cdData = (cd_hw_t*)&ext;
    #endif

    if (buffer != NULL) { memcpy(&cdData->prg_ram            , &buffer[pos],  sizeof(cdData->prg_ram            )); } pos += sizeof(cdData->prg_ram           );
    if (buffer != NULL) { memcpy(&cdData->word_ram           , &buffer[pos],  sizeof(cdData->word_ram           )); } pos += sizeof(cdData->word_ram          );
    if (buffer != NULL) { memcpy(&cdData->word_ram_2M        , &buffer[pos],  sizeof(cdData->word_ram_2M        )); } pos += sizeof(cdData->word_ram_2M       );
    if (buffer != NULL) { memcpy(&cdData->bram               , &buffer[pos],  sizeof(cdData->bram               )); } pos += sizeof(cdData->bram              );
    if (buffer != NULL) { memcpy(&cdData->regs               , &buffer[pos],  sizeof(cdData->regs               )); } pos += sizeof(cdData->regs              );
    if (buffer != NULL) { memcpy(&cdData->cycles             , &buffer[pos],  sizeof(cdData->cycles             )); } pos += sizeof(cdData->cycles            );
    if (buffer != NULL) { memcpy(&cdData->cycles_per_line    , &buffer[pos],  sizeof(cdData->cycles_per_line    )); } pos += sizeof(cdData->cycles_per_line   );
    if (buffer != NULL) { memcpy(&cdData->stopwatch          , &buffer[pos],  sizeof(cdData->stopwatch          )); } pos += sizeof(cdData->stopwatch         );
    if (buffer != NULL) { memcpy(&cdData->timer              , &buffer[pos],  sizeof(cdData->timer              )); } pos += sizeof(cdData->timer             );
    if (buffer != NULL) { memcpy(&cdData->pending            , &buffer[pos],  sizeof(cdData->pending            )); } pos += sizeof(cdData->pending           );
    if (buffer != NULL) { memcpy(&cdData->dmna               , &buffer[pos],  sizeof(cdData->dmna               )); } pos += sizeof(cdData->dmna              );
    if (buffer != NULL) { memcpy(&cdData->type               , &buffer[pos],  sizeof(cdData->type               )); } pos += sizeof(cdData->type              );
    // if (buffer != NULL) { memcpy(&cdData->gfx_hw             , &buffer[pos],  sizeof(cdData->gfx_hw             )); } pos += sizeof(cdData->gfx_hw            );
    if (buffer != NULL) { memcpy(&cdData->cdc_hw             , &buffer[pos],  sizeof(cdData->cdc_hw             )); } pos += sizeof(cdData->cdc_hw            );
    if (buffer != NULL) { memcpy(&cdData->cdd_hw             , &buffer[pos],  sizeof(cdData->cdd_hw             )); } pos += sizeof(cdData->cdd_hw            );
    // if (buffer != NULL) { memcpy(&cdData->pcm_hw             , &buffer[pos],  sizeof(cdData->pcm_hw             )); } pos += sizeof(cdData->pcm_hw            );       
  }

  // input_hw/activator.c

  if (buffer != NULL) { memcpy(&activator           , &buffer[pos],  sizeof(activator           )); } pos += sizeof(activator           );

  // input_hw/gamepad.c

  if (buffer != NULL) { memcpy(&gamepad             , &buffer[pos],  sizeof(gamepad             )); } pos += sizeof(gamepad             );
  if (buffer != NULL) { memcpy(&flipflop            , &buffer[pos],  sizeof(flipflop            )); } pos += sizeof(flipflop            );
  if (buffer != NULL) { memcpy(&latch               , &buffer[pos],  sizeof(latch               )); } pos += sizeof(latch               );

  // input_hw/graphic_board.c

  if (buffer != NULL) { memcpy(&board               , &buffer[pos],  sizeof(board               )); } pos += sizeof(board               );

  // input_hw/input.c

  if (buffer != NULL) { memcpy(&input               , &buffer[pos],  sizeof(input               )); } pos += sizeof(input               );
  if (buffer != NULL) { memcpy(&old_system          , &buffer[pos],  sizeof(old_system          )); } pos += sizeof(old_system          );

  // input_hw/lightgun.c
  
  if (buffer != NULL) { memcpy(&lightgun            , &buffer[pos],  sizeof(lightgun            )); } pos += sizeof(lightgun            );

  // input_hw/mouse.c

  if (buffer != NULL) { memcpy(&mouse               , &buffer[pos],  sizeof(mouse               )); } pos += sizeof(mouse               );

  // input_hw/paddle.c

  if (buffer != NULL) { memcpy(&paddle              , &buffer[pos],  sizeof(paddle              )); } pos += sizeof(paddle              );

  // input_hw/sportspad.c

  if (buffer != NULL) { memcpy(&sportspad           , &buffer[pos],  sizeof(sportspad           )); } pos += sizeof(sportspad           );

  // input_hw/teamplayer.c

  if (buffer != NULL) { memcpy(&teamplayer          , &buffer[pos],  sizeof(teamplayer          )); } pos += sizeof(teamplayer          );

  // input_hw/terebi_oekaki.c

  if (buffer != NULL) { memcpy(&tablet              , &buffer[pos],  sizeof(tablet              )); } pos += sizeof(tablet              );

  // input_hw/xe_1ap.c

  if (buffer != NULL) { memcpy(&xe_1ap              , &buffer[pos],  sizeof(xe_1ap              )); } pos += sizeof(xe_1ap              );

  // m68k/m68k.c

  if (buffer != NULL) { memcpy(&m68k                , &buffer[pos],  sizeof(m68k                )); } pos += sizeof(m68k                );
  if (buffer != NULL) { memcpy(&s68k                , &buffer[pos],  sizeof(s68k                )); } pos += sizeof(s68k                );

  // m68k/m68kcpu.c

  if (buffer != NULL) { memcpy(&m68k_irq_latency    , &buffer[pos],  sizeof(m68k_irq_latency    )); } pos += sizeof(m68k_irq_latency    );

  // m68k/s68kcpu.c

  if (buffer != NULL) { memcpy(&s68k_irq_latency    , &buffer[pos],  sizeof(s68k_irq_latency    )); } pos += sizeof(s68k_irq_latency    );

  // sound/psg.c

  if (buffer != NULL) { memcpy(& psg                , &buffer[pos],  sizeof( psg                )); } pos += sizeof( psg                );

  // sound/sound.c

  if (buffer != NULL) { memcpy(&fm_buffer           , &buffer[pos],  sizeof(fm_buffer           )); } pos += sizeof(fm_buffer           );
  if (buffer != NULL) { memcpy(&fm_last             , &buffer[pos],  sizeof(fm_last             )); } pos += sizeof(fm_last             );
  //if (buffer != NULL) { memcpy(&*fm_ptr             , &buffer[pos],  sizeof(*fm_ptr             )); } pos += sizeof(*fm_ptr             );
  if (buffer != NULL) { memcpy(&fm_cycles_ratio     , &buffer[pos],  sizeof(fm_cycles_ratio     )); } pos += sizeof(fm_cycles_ratio     );
  if (buffer != NULL) { memcpy(&fm_cycles_start     , &buffer[pos],  sizeof(fm_cycles_start     )); } pos += sizeof(fm_cycles_start     );
  if (buffer != NULL) { memcpy(&fm_cycles_count     , &buffer[pos],  sizeof(fm_cycles_count     )); } pos += sizeof(fm_cycles_count     );
  if (buffer != NULL) { memcpy(&fm_cycles_busy      , &buffer[pos],  sizeof(fm_cycles_busy      )); } pos += sizeof(fm_cycles_busy      );

  #ifdef HAVE_YM3438_CORE
  if (buffer != NULL) { memcpy(&ym3438              , &buffer[pos],  sizeof(ym3438              )); } pos += sizeof(ym3438              );
  if (buffer != NULL) { memcpy(&ym3438_accm         , &buffer[pos],  sizeof(ym3438_accm         )); } pos += sizeof(ym3438_accm         );
  if (buffer != NULL) { memcpy(&ym3438_sample       , &buffer[pos],  sizeof(ym3438_sample       )); } pos += sizeof(ym3438_sample       );
  if (buffer != NULL) { memcpy(&ym3438_cycles       , &buffer[pos],  sizeof(ym3438_cycles       )); } pos += sizeof(ym3438_cycles       );
  #endif

  #ifdef HAVE_OPLL_CORE
  if (buffer != NULL) { memcpy(&opll                , &buffer[pos],  sizeof(opll                )); } pos += sizeof(opll                );
  if (buffer != NULL) { memcpy(&opll_accm           , &buffer[pos],  sizeof(opll_accm           )); } pos += sizeof(opll_accm           );
  if (buffer != NULL) { memcpy(&opll_sample         , &buffer[pos],  sizeof(opll_sample         )); } pos += sizeof(opll_sample         );
  if (buffer != NULL) { memcpy(&opll_cycles         , &buffer[pos],  sizeof(opll_cycles         )); } pos += sizeof(opll_cycles         );
  if (buffer != NULL) { memcpy(&opll_status         , &buffer[pos],  sizeof(opll_status         )); } pos += sizeof(opll_status         );
  #endif

  // sound/ym2413.h

  if (buffer != NULL) { memcpy(&output              , &buffer[pos],  sizeof(output              )); } pos += sizeof(output              );
  if (buffer != NULL) { memcpy(&LFO_AM              , &buffer[pos],  sizeof(LFO_AM              )); } pos += sizeof(LFO_AM              );
  if (buffer != NULL) { memcpy(&LFO_PM              , &buffer[pos],  sizeof(LFO_PM              )); } pos += sizeof(LFO_PM              );
  if (buffer != NULL) { memcpy(&ym2413              , &buffer[pos],  sizeof(ym2413              )); } pos += sizeof(ym2413              );

  // sound/ym2612.c

  if (buffer != NULL) { memcpy(&ym2612              , &buffer[pos],  sizeof(ym2612              )); } pos += sizeof(ym2612              );
  if (buffer != NULL) { memcpy(&m2                  , &buffer[pos],  sizeof(m2                  )); } pos += sizeof(m2                  );
  if (buffer != NULL) { memcpy(&c1                  , &buffer[pos],  sizeof(c1                  )); } pos += sizeof(c1                  );
  if (buffer != NULL) { memcpy(&c2                  , &buffer[pos],  sizeof(c2                  )); } pos += sizeof(c2                  );
  if (buffer != NULL) { memcpy(&mem                 , &buffer[pos],  sizeof(mem                 )); } pos += sizeof(mem                 );
  if (buffer != NULL) { memcpy(&out_fm              , &buffer[pos],  sizeof(out_fm              )); } pos += sizeof(out_fm              );
  if (buffer != NULL) { memcpy(&op_mask             , &buffer[pos],  sizeof(op_mask             )); } pos += sizeof(op_mask             );
  if (buffer != NULL) { memcpy(&chip_type           , &buffer[pos],  sizeof(chip_type           )); } pos += sizeof(chip_type           );

  // z80/z80.c

  if (buffer != NULL) { memcpy(&Z80                 , &buffer[pos],  sizeof(Z80                 )); } pos += sizeof(Z80                 );
  if (buffer != NULL) { memcpy(&z80_last_fetch      , &buffer[pos],  sizeof(z80_last_fetch      )); } pos += sizeof(z80_last_fetch      );
  if (buffer != NULL) { memcpy(&z80_readmap         , &buffer[pos],  sizeof(z80_readmap         )); } pos += sizeof(z80_readmap         );
  if (buffer != NULL) { memcpy(&z80_writemap        , &buffer[pos],  sizeof(z80_writemap        )); } pos += sizeof(z80_writemap        );
  if (buffer != NULL) { memcpy(&EA                  , &buffer[pos],  sizeof(EA                  )); } pos += sizeof(EA                  );
  if (buffer != NULL) { memcpy(&SZ                  , &buffer[pos],  sizeof(SZ                  )); } pos += sizeof(SZ                  );
  if (buffer != NULL) { memcpy(&SZ_BIT              , &buffer[pos],  sizeof(SZ_BIT              )); } pos += sizeof(SZ_BIT              );
  if (buffer != NULL) { memcpy(&SZP                 , &buffer[pos],  sizeof(SZP                 )); } pos += sizeof(SZP                 );
  if (buffer != NULL) { memcpy(&SZHV_inc            , &buffer[pos],  sizeof(SZHV_inc            )); } pos += sizeof(SZHV_inc            );
  if (buffer != NULL) { memcpy(&SZHV_dec            , &buffer[pos],  sizeof(SZHV_dec            )); } pos += sizeof(SZHV_dec            );
  if (buffer != NULL) { memcpy(&SZHVC_add           , &buffer[pos],  sizeof(SZHVC_add           )); } pos += sizeof(SZHVC_add           );
  if (buffer != NULL) { memcpy(&SZHVC_sub           , &buffer[pos],  sizeof(SZHVC_sub           )); } pos += sizeof(SZHVC_sub           );

  #ifdef Z80_OVERCLOCK_SHIFT
  if (buffer != NULL) { memcpy(&z80_cycle_ratio     , &buffer[pos],  sizeof(z80_cycle_ratio     )); } pos += sizeof(z80_cycle_ratio     );
  #endif

  // genesis.c

  if (buffer != NULL) { memcpy(&boot_rom            , &buffer[pos],  sizeof(boot_rom            )); } pos += sizeof(boot_rom            );
  if (buffer != NULL) { memcpy(&work_ram            , &buffer[pos],  sizeof(work_ram            )); } pos += sizeof(work_ram            );
  if (buffer != NULL) { memcpy(&zram                , &buffer[pos],  sizeof(zram                )); } pos += sizeof(zram                );
  if (buffer != NULL) { memcpy(&zbank               , &buffer[pos],  sizeof(zbank               )); } pos += sizeof(zbank               );
  if (buffer != NULL) { memcpy(&zstate              , &buffer[pos],  sizeof(zstate              )); } pos += sizeof(zstate              );
  if (buffer != NULL) { memcpy(&pico_current        , &buffer[pos],  sizeof(pico_current        )); } pos += sizeof(pico_current        );
  if (buffer != NULL) { memcpy(&tmss                , &buffer[pos],  sizeof(tmss                )); } pos += sizeof(tmss                );

  // io_ctrl.c

  if (buffer != NULL) { memcpy(&io_reg              , &buffer[pos],  sizeof(io_reg              )); } pos += sizeof(io_reg              );
  if (buffer != NULL) { memcpy(&region_code         , &buffer[pos],  sizeof(region_code         )); } pos += sizeof(region_code         );
  if (buffer != NULL) { memcpy(&port                , &buffer[pos],  sizeof(port                )); } pos += sizeof(port                );

  // load_rom.c

  if (buffer != NULL) { memcpy(&rominfo             , &buffer[pos],  sizeof(rominfo             )); } pos += sizeof(rominfo             );
  if (buffer != NULL) { memcpy(&romtype             , &buffer[pos],  sizeof(romtype             )); } pos += sizeof(romtype             );
  if (buffer != NULL) { memcpy(&rom_region          , &buffer[pos],  sizeof(rom_region          )); } pos += sizeof(rom_region          );

  // membnk.c

  if (buffer != NULL) { memcpy(&zbank_memory_map    , &buffer[pos],  sizeof(zbank_memory_map    )); } pos += sizeof(zbank_memory_map    );

  // system.c

  // if (buffer != NULL) { memcpy(&bitmap              , &buffer[pos],  sizeof(bitmap              )); } pos += sizeof(bitmap              );
  // if (buffer != NULL) { memcpy(&snd                 , &buffer[pos],  sizeof(snd                 )); } pos += sizeof(snd                 );
  // if (buffer != NULL) { memcpy(&mcycles_vdp         , &buffer[pos],  sizeof(mcycles_vdp         )); } pos += sizeof(mcycles_vdp         );
  // if (buffer != NULL) { memcpy(&system_hw           , &buffer[pos],  sizeof(system_hw           )); } pos += sizeof(system_hw           );
  // if (buffer != NULL) { memcpy(&system_bios         , &buffer[pos],  sizeof(system_bios         )); } pos += sizeof(system_bios         );
  // if (buffer != NULL) { memcpy(&system_clock        , &buffer[pos],  sizeof(system_clock        )); } pos += sizeof(system_clock        );
  // if (buffer != NULL) { memcpy(&SVP_cycles          , &buffer[pos],  sizeof(SVP_cycles          )); } pos += sizeof(SVP_cycles          );
  if (buffer != NULL) { memcpy(&pause_b             , &buffer[pos],  sizeof(pause_b             )); } pos += sizeof(pause_b             );
  // if (buffer != NULL) { memcpy(&eq                  , &buffer[pos],  sizeof(eq                  )); } pos += sizeof(eq                  );
  // if (buffer != NULL) { memcpy(&llp                 , &buffer[pos],  sizeof(llp                 )); } pos += sizeof(llp                 );
  // if (buffer != NULL) { memcpy(&rrp                 , &buffer[pos],  sizeof(rrp                 )); } pos += sizeof(rrp                 );

  // vdp.c

  if (buffer != NULL) { memcpy(&sat                 , &buffer[pos],  sizeof(sat                 )); } pos += sizeof(sat                 );
  if (buffer != NULL) { memcpy(&vram                , &buffer[pos],  sizeof(vram                )); } pos += sizeof(vram                );
  if (buffer != NULL) { memcpy(&cram                , &buffer[pos],  sizeof(cram                )); } pos += sizeof(cram                );
  if (buffer != NULL) { memcpy(&vsram               , &buffer[pos],  sizeof(vsram               )); } pos += sizeof(vsram               );
  if (buffer != NULL) { memcpy(&reg                 , &buffer[pos],  sizeof(reg                 )); } pos += sizeof(reg                 );
  if (buffer != NULL) { memcpy(&hint_pending        , &buffer[pos],  sizeof(hint_pending        )); } pos += sizeof(hint_pending        );
  if (buffer != NULL) { memcpy(&vint_pending        , &buffer[pos],  sizeof(vint_pending        )); } pos += sizeof(vint_pending        );
  if (buffer != NULL) { memcpy(&status              , &buffer[pos],  sizeof(status              )); } pos += sizeof(status              );
  if (buffer != NULL) { memcpy(&dma_length          , &buffer[pos],  sizeof(dma_length          )); } pos += sizeof(dma_length          );
  if (buffer != NULL) { memcpy(&dma_endCycles       , &buffer[pos],  sizeof(dma_endCycles       )); } pos += sizeof(dma_endCycles       );
  if (buffer != NULL) { memcpy(&dma_type            , &buffer[pos],  sizeof(dma_type            )); } pos += sizeof(dma_type            );
  if (buffer != NULL) { memcpy(&ntab                , &buffer[pos],  sizeof(ntab                )); } pos += sizeof(ntab                );
  if (buffer != NULL) { memcpy(&ntbb                , &buffer[pos],  sizeof(ntbb                )); } pos += sizeof(ntbb                );
  if (buffer != NULL) { memcpy(&ntwb                , &buffer[pos],  sizeof(ntwb                )); } pos += sizeof(ntwb                );
  if (buffer != NULL) { memcpy(&satb                , &buffer[pos],  sizeof(satb                )); } pos += sizeof(satb                );
  if (buffer != NULL) { memcpy(&hscb                , &buffer[pos],  sizeof(hscb                )); } pos += sizeof(hscb                );
  if (buffer != NULL) { memcpy(&bg_name_dirty       , &buffer[pos],  sizeof(bg_name_dirty       )); } pos += sizeof(bg_name_dirty       );
  if (buffer != NULL) { memcpy(&bg_name_list        , &buffer[pos],  sizeof(bg_name_list        )); } pos += sizeof(bg_name_list        );
  if (buffer != NULL) { memcpy(&bg_list_index       , &buffer[pos],  sizeof(bg_list_index       )); } pos += sizeof(bg_list_index       );
  if (buffer != NULL) { memcpy(&hscroll_mask        , &buffer[pos],  sizeof(hscroll_mask        )); } pos += sizeof(hscroll_mask        );
  if (buffer != NULL) { memcpy(&playfield_shift     , &buffer[pos],  sizeof(playfield_shift     )); } pos += sizeof(playfield_shift     );
  if (buffer != NULL) { memcpy(&playfield_col_mask  , &buffer[pos],  sizeof(playfield_col_mask  )); } pos += sizeof(playfield_col_mask  );
  if (buffer != NULL) { memcpy(&playfield_row_mask  , &buffer[pos],  sizeof(playfield_row_mask  )); } pos += sizeof(playfield_row_mask  );
  if (buffer != NULL) { memcpy(&vscroll             , &buffer[pos],  sizeof(vscroll             )); } pos += sizeof(vscroll             );
  if (buffer != NULL) { memcpy(&odd_frame           , &buffer[pos],  sizeof(odd_frame           )); } pos += sizeof(odd_frame           );
  if (buffer != NULL) { memcpy(&im2_flag            , &buffer[pos],  sizeof(im2_flag            )); } pos += sizeof(im2_flag            );
  if (buffer != NULL) { memcpy(&interlaced          , &buffer[pos],  sizeof(interlaced          )); } pos += sizeof(interlaced          );
  if (buffer != NULL) { memcpy(&vdp_pal             , &buffer[pos],  sizeof(vdp_pal             )); } pos += sizeof(vdp_pal             );
  if (buffer != NULL) { memcpy(&h_counter           , &buffer[pos],  sizeof(h_counter           )); } pos += sizeof(h_counter           );
  if (buffer != NULL) { memcpy(&v_counter           , &buffer[pos],  sizeof(v_counter           )); } pos += sizeof(v_counter           );
  if (buffer != NULL) { memcpy(&vc_max              , &buffer[pos],  sizeof(vc_max              )); } pos += sizeof(vc_max              );
  if (buffer != NULL) { memcpy(&lines_per_frame     , &buffer[pos],  sizeof(lines_per_frame     )); } pos += sizeof(lines_per_frame     );
  if (buffer != NULL) { memcpy(&max_sprite_pixels   , &buffer[pos],  sizeof(max_sprite_pixels   )); } pos += sizeof(max_sprite_pixels   );
  if (buffer != NULL) { memcpy(&fifo_cycles         , &buffer[pos],  sizeof(fifo_cycles         )); } pos += sizeof(fifo_cycles         );
  if (buffer != NULL) { memcpy(&hvc_latch           , &buffer[pos],  sizeof(hvc_latch           )); } pos += sizeof(hvc_latch           );
  if (buffer != NULL) { memcpy(&vint_cycle          , &buffer[pos],  sizeof(vint_cycle          )); } pos += sizeof(vint_cycle          );
  // if (buffer != NULL) { memcpy(&hctab               , &buffer[pos],  sizeof(hctab               )); } pos += sizeof(hctab               );
  if (buffer != NULL) { memcpy(&border              , &buffer[pos],  sizeof(border              )); } pos += sizeof(border              );
  if (buffer != NULL) { memcpy(&pending             , &buffer[pos],  sizeof(pending             )); } pos += sizeof(pending             );
  if (buffer != NULL) { memcpy(&code                , &buffer[pos],  sizeof(code                )); } pos += sizeof(code                );
  if (buffer != NULL) { memcpy(&addr                , &buffer[pos],  sizeof(addr                )); } pos += sizeof(addr                );
  if (buffer != NULL) { memcpy(&addr_latch          , &buffer[pos],  sizeof(addr_latch          )); } pos += sizeof(addr_latch          );
  if (buffer != NULL) { memcpy(&sat_base_mask       , &buffer[pos],  sizeof(sat_base_mask       )); } pos += sizeof(sat_base_mask       );
  if (buffer != NULL) { memcpy(&sat_addr_mask       , &buffer[pos],  sizeof(sat_addr_mask       )); } pos += sizeof(sat_addr_mask       );
  if (buffer != NULL) { memcpy(&dma_src             , &buffer[pos],  sizeof(dma_src             )); } pos += sizeof(dma_src             );
  if (buffer != NULL) { memcpy(&dmafill             , &buffer[pos],  sizeof(dmafill             )); } pos += sizeof(dmafill             );
  if (buffer != NULL) { memcpy(&cached_write        , &buffer[pos],  sizeof(cached_write        )); } pos += sizeof(cached_write        );
  if (buffer != NULL) { memcpy(&fifo                , &buffer[pos],  sizeof(fifo                )); } pos += sizeof(fifo                );
  if (buffer != NULL) { memcpy(&fifo_idx            , &buffer[pos],  sizeof(fifo_idx            )); } pos += sizeof(fifo_idx            );
  if (buffer != NULL) { memcpy(&fifo_byte_access    , &buffer[pos],  sizeof(fifo_byte_access    )); } pos += sizeof(fifo_byte_access    );
  // if (buffer != NULL) { memcpy(&fifo_timing         , &buffer[pos],  sizeof(fifo_timing         )); } pos += sizeof(fifo_timing         );
  if (buffer != NULL) { memcpy(&hblank_start_cycle  , &buffer[pos],  sizeof(hblank_start_cycle  )); } pos += sizeof(hblank_start_cycle  );
  if (buffer != NULL) { memcpy(&hblank_end_cycle    , &buffer[pos],  sizeof(hblank_end_cycle    )); } pos += sizeof(hblank_end_cycle    );

  // vdp_render.c

  // if (buffer != NULL) { memcpy(&clip                , &buffer[pos],  sizeof(clip                )); } pos += sizeof(clip                );
  // if (buffer != NULL) { memcpy(&bg_pattern_cache    , &buffer[pos],  sizeof(bg_pattern_cache    )); } pos += sizeof(bg_pattern_cache    );
  // if (buffer != NULL) { memcpy(&name_lut            , &buffer[pos],  sizeof(name_lut            )); } pos += sizeof(name_lut            );
  // if (buffer != NULL) { memcpy(&bp_lut              , &buffer[pos],  sizeof(bp_lut              )); } pos += sizeof(bp_lut              );
  // if (buffer != NULL) { memcpy(&lut                 , &buffer[pos],  sizeof(lut                 )); } pos += sizeof(lut                 );
  // if (buffer != NULL) { memcpy(&pixel               , &buffer[pos],  sizeof(pixel               )); } pos += sizeof(pixel               );
  // if (buffer != NULL) { memcpy(&pixel_lut           , &buffer[pos],  sizeof(pixel_lut           )); } pos += sizeof(pixel_lut           );
  // if (buffer != NULL) { memcpy(&pixel_lut_m4        , &buffer[pos],  sizeof(pixel_lut_m4        )); } pos += sizeof(pixel_lut_m4        );
  // if (buffer != NULL) { memcpy(&linebuf             , &buffer[pos],  sizeof(linebuf             )); } pos += sizeof(linebuf             );
  // if (buffer != NULL) { memcpy(&spr_ovr             , &buffer[pos],  sizeof(spr_ovr             )); } pos += sizeof(spr_ovr             );
  // if (buffer != NULL) { memcpy(&obj_info            , &buffer[pos],  sizeof(obj_info            )); } pos += sizeof(obj_info            );
  // if (buffer != NULL) { memcpy(&object_count        , &buffer[pos],  sizeof(object_count        )); } pos += sizeof(object_count        );
  // if (buffer != NULL) { memcpy(&spr_col             , &buffer[pos],  sizeof(spr_col             )); } pos += sizeof(spr_col             );

}