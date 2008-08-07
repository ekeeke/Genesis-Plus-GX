/* The SVP chip emulator

 (c) Copyright 2008, Grazvydas "notaz" Ignotas
 Free for non-commercial use.

 For commercial use, separate licencing terms must be obtained.

*/
/* SVP Controller */
/* modified for Genesis Plus GCN port (eke-eke) */

#ifndef _SVP_H_
#define _SVP_H_

#include "shared.h"
#include "ssp16.h"

typedef struct {
	unsigned char iram_rom[0x20000]; // IRAM (0-0x7ff) and program ROM (0x800-0x1ffff)
	unsigned char dram[0x20000];
	ssp1601_t ssp1601;
} svp_t;

extern svp_t *svp;
extern uint16 SVP_cycles; 

extern void svp_init(void);
extern void svp_reset(void);

#endif
