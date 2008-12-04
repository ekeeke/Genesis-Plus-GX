/***************************************************************************************
 *  Genesis Plus 1.2a
 *  FreezeState support
 *
 *  coded by Eke-Eke, GC/Wii port
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************************/

#include "shared.h"

static unsigned char state[0x24000];
static unsigned int bufferptr;

static inline void load_param(void *param, unsigned int size)
{
	memcpy(param, &state[bufferptr], size);
	bufferptr+= size;
}

static inline void save_param(void *param, unsigned int size)
{
	memcpy(&state[bufferptr], param, size);
	bufferptr+= size;
}

void state_load(unsigned char *buffer)
{
	/* reset buffer pointer */
	bufferptr = 0;

  /* uncompress savestate */
  unsigned long inbytes, outbytes;
	memcpy(&inbytes, buffer, 4);
	outbytes = 0x24000;
  uncompress ((Bytef *)state, &outbytes, (Bytef *)(buffer + 4), inbytes);

	/* reset system */
	system_reset();
	m68k_memory_map[0].base = default_rom;

	// GENESIS
	load_param(work_ram, sizeof(work_ram));
	load_param(zram, sizeof(zram));
	load_param(&zbusreq, sizeof(zbusreq));
	load_param(&zreset, sizeof(zreset));
	load_param(&zbank, sizeof(zbank));
	zbusack = 1 ^(zbusreq & zreset);

	// IO
	load_param(io_reg, sizeof(io_reg));
	
	// VDP
	uint8 temp_reg[0x20];
	load_param(sat, sizeof(sat));
	load_param(vram, sizeof(vram));
	load_param(cram, sizeof(cram));
	load_param(vsram, sizeof(vsram));
	load_param(temp_reg, sizeof(temp_reg));
	load_param(&addr, sizeof(addr));
	load_param(&addr_latch, sizeof(addr_latch));
	load_param(&code, sizeof(code));
	load_param(&pending, sizeof(pending));
	load_param(&status, sizeof(status));
	load_param(&dmafill, sizeof(dmafill));
	load_param(&hint_pending, sizeof(hint_pending));
	load_param(&vint_pending, sizeof(vint_pending));
	load_param(&irq_status, sizeof(irq_status));
	vdp_restore(temp_reg);

	// FM 
	load_param(fm_reg,sizeof(fm_reg));
	fm_restore();

	// PSG
	load_param(SN76489_GetContextPtr (0),SN76489_GetContextSize ());

	// 68000 
	uint16 tmp16;
	uint32 tmp32;
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D0, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D1, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D2, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D3, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D4, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D5, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D6, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D7, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A0, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A1, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A2, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A3, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A4, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A5, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A6, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A7, tmp32);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_PC, tmp32);	
	load_param(&tmp16, 2); m68k_set_reg(M68K_REG_SR, tmp16);
	load_param(&tmp32, 4); m68k_set_reg(M68K_REG_USP,tmp32);

	// Z80 
	load_param(&Z80, sizeof(Z80_Regs));
}

int state_save(unsigned char *buffer)
{
	/* reset buffer pointer */
	bufferptr = 0;

	// GENESIS
	save_param(work_ram, sizeof(work_ram));
	save_param(zram, sizeof(zram));
	save_param(&zbusreq, sizeof(zbusreq));
	save_param(&zreset, sizeof(zreset));
	save_param(&zbank, sizeof(zbank));

	// IO
	save_param(io_reg, sizeof(io_reg));
	
	// VDP
	save_param(sat, sizeof(sat));
	save_param(vram, sizeof(vram));
	save_param(cram, sizeof(cram));
	save_param(vsram, sizeof(vsram));
	save_param(reg, sizeof(reg));
	save_param(&addr, sizeof(addr));
	save_param(&addr_latch, sizeof(addr_latch));
	save_param(&code, sizeof(code));
	save_param(&pending, sizeof(pending));
	save_param(&status, sizeof(status));
	save_param(&dmafill, sizeof(dmafill));
	save_param(&hint_pending, sizeof(hint_pending));
	save_param(&vint_pending, sizeof(vint_pending));
	save_param(&irq_status, sizeof(irq_status));

	// FM 
	save_param(fm_reg,sizeof(fm_reg));

	// PSG 
	save_param(SN76489_GetContextPtr (0),SN76489_GetContextSize ());

	// 68000 
	uint16 tmp16;
	uint32 tmp32;
  tmp32 = m68k_get_reg(NULL, M68K_REG_D0);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_D1); 	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_D2);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_D3);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_D4);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_D5);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_D6);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_D7);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A0);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A1);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A2);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A3);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A4);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A5);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A6);	save_param(&tmp32, 4);
	tmp32 =	m68k_get_reg(NULL, M68K_REG_A7);	save_param(&tmp32, 4);
	tmp32 = m68k_get_reg(NULL, M68K_REG_PC);	save_param(&tmp32, 4);
	tmp16 = m68k_get_reg(NULL, M68K_REG_SR);	save_param(&tmp16, 2); 
	tmp32 = m68k_get_reg(NULL, M68K_REG_USP);	save_param(&tmp32, 4);

	// Z80 
	save_param(&Z80, sizeof(Z80_Regs));

	/* compress state file */
	unsigned long inbytes   = bufferptr;
  unsigned long outbytes  = 0x26000;
  compress2 ((Bytef *)(buffer + 4), &outbytes, (Bytef *)state, inbytes, 9);
	memcpy(buffer, &outbytes, 4);

	/* return total size */
	return (outbytes + 4);
}
