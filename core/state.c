#include "state.h"

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
