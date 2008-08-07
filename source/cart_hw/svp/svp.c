// The SVP chip emulator

// (c) Copyright 2008, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

/* SVP Controller */
/* modified for Genesis Plus GCN port (eke-eke) */

#include "shared.h"

svp_t *svp = NULL;
uint16 SVP_cycles = 850; 

void svp_init(void)
{
	svp = (void *) ((char *)cart_rom + 0x200000);
	memset(svp, 0, sizeof(*svp));
}

void svp_reset(void)
{
	memcpy(svp->iram_rom + 0x800, cart_rom + 0x800, 0x20000 - 0x800);
	ssp1601_reset(&svp->ssp1601);
}
