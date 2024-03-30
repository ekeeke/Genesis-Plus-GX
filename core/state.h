#include <stdint.h>
#include "genesis.h"
#include "io_ctrl.h"
#include "loadrom.h"
#include "membnk.h"
#include "system.h"
#include "sound/eq.h"

#pragma once

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
