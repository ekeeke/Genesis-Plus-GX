/****************************************************************************
 *  Genesis Plus 1.2a
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald
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
 *  STATE MANAGER
 ***************************************************************************/
#include "shared.h"

unsigned char state[0x23000];
unsigned int bufferptr;

void load_param(void *param, unsigned int size)
{
	memcpy(param, &state[bufferptr], size);
	bufferptr+= size;
}

void save_param(void *param, unsigned int size)
{
	memcpy(&state[bufferptr], param, size);
	bufferptr+= size;
}

void state_load(unsigned char *buffer)
{
	uint32	tmp32;
	uint16 tmp16;
	int i;
	int	height, width;
    unsigned long inbytes, outbytes;
	
	/* get compressed state size & uncompress state file */
	memcpy(&inbytes,&buffer[0],sizeof(inbytes));

#ifdef NGC
    outbytes = 0x23000;
	uncompress ((char *) &state[0], &outbytes, (char *) &buffer[sizeof(inbytes)], inbytes);
#else
	outbytes = inbytes;
	memcpy(&state[0], &buffer[sizeof(inbytes)], outbytes);
#endif

	/* load state */
	system_reset();
	bufferptr = 0;
	
	// gen.c stuff
	load_param(work_ram, sizeof(work_ram));
	load_param(zram, sizeof(zram));
	load_param(&zbusreq, sizeof(zbusreq));
	load_param(&zreset, sizeof(zreset));
	load_param(&zbusack, sizeof(zbusack));
	load_param(&zirq, sizeof(zirq));
	load_param(&zbank, sizeof(zbank));
	load_param(&lastbusreqcnt, sizeof(lastbusreqcnt));
	load_param(&lastbusack, sizeof(lastbusack));

	// io.c stuff
	load_param(io_reg, sizeof(io_reg));
	
	// render.c stuff
	load_param(&object_index_count,sizeof(object_index_count));
		
	// vdp.c stuff
	load_param(sat, sizeof(sat));
	load_param(vram, sizeof(vram));
	load_param(cram, sizeof(cram));
	load_param(vsram, sizeof(vsram));
	load_param(reg, sizeof(reg));
	load_param(&addr, sizeof(addr));
	load_param(&addr_latch, sizeof(addr_latch));
	load_param(&code, sizeof(code));
	load_param(&pending, sizeof(pending));
	load_param(&status, sizeof(status));
	load_param(&ntab, sizeof(ntab));
	load_param(&ntbb, sizeof(ntbb));
	load_param(&ntwb, sizeof(ntwb));
	load_param(&satb, sizeof(satb));
	load_param(&hscb, sizeof(hscb));
	load_param(&sat_base_mask, sizeof(sat_base_mask));
	load_param(&sat_addr_mask, sizeof(sat_addr_mask));
	load_param(&border, sizeof(border));
	load_param(&playfield_shift, sizeof(playfield_shift));
	load_param(&playfield_col_mask, sizeof(playfield_col_mask));
	load_param(&playfield_row_mask, sizeof(playfield_row_mask));
	load_param(&y_mask, sizeof(y_mask));
	load_param(&hint_pending, sizeof(hint_pending));
	load_param(&vint_pending, sizeof(vint_pending));
	load_param(&h_counter, sizeof(h_counter));
	load_param(&hc_latch, sizeof(hc_latch));
	load_param(&v_counter, sizeof(v_counter));
	load_param(&dmafill, sizeof(dmafill));
	load_param(&im2_flag, sizeof(im2_flag));
    load_param(&frame_end, sizeof(frame_end));
	load_param(&dma_endCycles, sizeof(dma_endCycles));
	
	// system.c stuff
	load_param(&aim_m68k,sizeof(aim_m68k));
	load_param(&count_m68k,sizeof(count_m68k));
	load_param(&dma_m68k,sizeof(dma_m68k));
	load_param(&aim_z80,sizeof(aim_z80));
	load_param(&count_z80,sizeof(count_z80));

	// FM stuff
	load_param(fm_reg,sizeof(fm_reg));
	load_param(&fm_status,sizeof(fm_status));
	load_param(timer,sizeof(timer));
	fm_restore();

	// psg stuff
	load_param(&PSG_MAME,1);
	if (PSG_MAME)
	{
		struct SN76496 *R = &sn[0];
		load_param(R->Register,27*4);
	}
	else load_param(SN76489_GetContextPtr (0),SN76489_GetContextSize ());
   		
	// Window size
	load_param(&height, sizeof(int));
	load_param(&width, sizeof(int));
	if (height != bitmap.viewport.h)
	{
		bitmap.viewport.oh = bitmap.viewport.h;
		bitmap.viewport.h = height;
		bitmap.viewport.changed = 1;
	}
	if (width != bitmap.viewport.w)
	{
		bitmap.viewport.ow = bitmap.viewport.w;
		bitmap.viewport.w = width;
		bitmap.viewport.changed = 1;
	}

	// 68000 CPU
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

	// MAME Z80 CPU
	load_param(&tmp32, 4); z80_set_reg(Z80_PC,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_SP,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_AF,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_BC,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_DE,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_HL,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_IX,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_IY,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_R,   tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_I,   tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_AF2, tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_BC2, tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_DE2, tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_HL2, tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_IM,  tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_IFF1,tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_IFF2,tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_HALT,tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_NMI_STATE,tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_IRQ_STATE,tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_DC0, tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_DC1, tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_DC2, tmp32);
	load_param(&tmp32, 4); z80_set_reg(Z80_DC3, tmp32);
	
	// Remake cache
	for (i=0;i<0x800;i++) 
    {
	    bg_name_list[i]=i;
	    bg_name_dirty[i]=0xFF;
    }
	bg_list_index=0x800;
    color_update = color_update_16;
	for(i = 0; i < 0x40; i += 1) color_update(i, *(uint16 *)&cram[i << 1]);
	color_update(0x00, *(uint16 *)&cram[border << 1]);
	color_update (0x40, *(uint16 *)&cram[border << 1]);
	color_update (0x80, *(uint16 *)&cram[border << 1]);
}

int state_save(unsigned char *buffer)
{
	uint32	tmp32;
	uint16	tmp16;
    unsigned long inbytes, outbytes;

	/* save state */
	bufferptr = 0;
	
    // gen.c stuff
	save_param(work_ram, sizeof(work_ram));
	save_param(zram, sizeof(zram));
	save_param(&zbusreq, sizeof(zbusreq));
	save_param(&zreset, sizeof(zreset));
	save_param(&zbusack, sizeof(zbusack));
	save_param(&zirq, sizeof(zirq));
	save_param(&zbank, sizeof(zbank));
	save_param(&lastbusreqcnt, sizeof(lastbusreqcnt));
	save_param(&lastbusack, sizeof(lastbusack));

	// io.c stuff
	save_param(io_reg, sizeof(io_reg));
	
	// render.c stuff
	save_param(&object_index_count,sizeof(object_index_count));
		
	// vdp.c stuff
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
	save_param(&ntab, sizeof(ntab));
	save_param(&ntbb, sizeof(ntbb));
	save_param(&ntwb, sizeof(ntwb));
	save_param(&satb, sizeof(satb));
	save_param(&hscb, sizeof(hscb));
	save_param(&sat_base_mask, sizeof(sat_base_mask));
	save_param(&sat_addr_mask, sizeof(sat_addr_mask));
	save_param(&border, sizeof(border));
	save_param(&playfield_shift, sizeof(playfield_shift));
	save_param(&playfield_col_mask, sizeof(playfield_col_mask));
	save_param(&playfield_row_mask, sizeof(playfield_row_mask));
	save_param(&y_mask, sizeof(y_mask));
	save_param(&hint_pending, sizeof(hint_pending));
	save_param(&vint_pending, sizeof(vint_pending));
	save_param(&h_counter, sizeof(h_counter));
	save_param(&hc_latch, sizeof(hc_latch));
	save_param(&v_counter, sizeof(v_counter));
	save_param(&dmafill, sizeof(dmafill));
	save_param(&im2_flag, sizeof(im2_flag));
    save_param(&frame_end, sizeof(frame_end));
	save_param(&dma_endCycles, sizeof(dma_endCycles));
	
	// system.c stuff
	save_param(&aim_m68k,sizeof(aim_m68k));
	save_param(&count_m68k,sizeof(count_m68k));
	save_param(&dma_m68k,sizeof(dma_m68k));
	save_param(&aim_z80,sizeof(aim_z80));
	save_param(&count_z80,sizeof(count_z80));

	// FM stuff
	save_param(fm_reg,sizeof(fm_reg));
	save_param(&fm_status,sizeof(fm_status));
	save_param(timer,sizeof(timer));
	
	// PSG stuff
	save_param(&PSG_MAME,1);
	if (PSG_MAME)
	{
		struct SN76496 *R = &sn[0];
		save_param(R->Register,27*4);
	}
	else save_param(SN76489_GetContextPtr (0),SN76489_GetContextSize ());
   	
	// Window size
	save_param(&bitmap.viewport.h, sizeof(int));
	save_param(&bitmap.viewport.w, sizeof(int));
	
	// 68000 CPU
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

	// Z80 CPU
	tmp32 = z80_get_reg(Z80_PC);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_SP);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_AF);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_BC);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_DE);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_HL);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_IX);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_IY);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_R);					save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_I);					save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_AF2);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_BC2);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_DE2);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_HL2);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_IM);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_IFF1);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_IFF2);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_HALT);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_NMI_STATE);			save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_IRQ_STATE);			save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_DC0);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_DC1);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_DC2);				save_param(&tmp32, 4);
	tmp32 = z80_get_reg(Z80_DC3);				save_param(&tmp32, 4);

	inbytes = bufferptr;
	
#ifdef NGC
	/* compress state file */
	outbytes = 0x24000;
    compress2 ((char *) &buffer[sizeof(outbytes)], &outbytes, (char *) &state[0], inbytes, 9);
#else
	outbytes = inbytes;
	memcpy(&buffer[sizeof(outbytes)], &state[0], outbytes);
#endif

	/* write compressed size in the first 32 bits for decompression */
	memcpy(&buffer[0], &outbytes, sizeof(outbytes));

	/* return total size */
	return (sizeof(outbytes) + outbytes);
}
