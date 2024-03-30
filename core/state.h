#include <stdint.h>
#include "genesis.h"
#include "io_ctrl.h"
#include "loadrom.h"
#include "membnk.h"

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