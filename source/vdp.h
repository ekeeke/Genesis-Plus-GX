/***************************************************************************************
 *  Genesis Plus 1.2a
 *  Video Display Processor (memory handlers)
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  modified by Eke-Eke (compatibility fixes & additional code), GC/Wii port
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

#ifndef _VDP_H_
#define _VDP_H_

/* VDP context */
extern uint8 sat[0x400];
extern uint8 vram[0x10000];
extern uint8 cram[0x80];               
extern uint8 vsram[0x80];              
extern uint8 reg[0x20];
extern uint16 addr;
extern uint16 addr_latch;
extern uint8 code;
extern uint8 pending;
extern uint16 status;
extern uint8 dmafill;
extern uint8 hint_pending;
extern uint8 vint_pending;
extern uint8 vint_triggered;
extern int8 hvint_updated;

/* Global variables */
extern uint16 ntab;                    
extern uint16 ntbb;                    
extern uint16 ntwb;                    
extern uint16 satb;                    
extern uint16 hscb;                    
extern uint8 border;                   
extern uint8 bg_name_dirty[0x800];     
extern uint16 bg_name_list[0x800];     
extern uint16 bg_list_index;           
extern uint8 bg_pattern_cache[0x80000];
extern uint8 playfield_shift;          
extern uint8 playfield_col_mask;       
extern uint16 playfield_row_mask;      
extern uint32 y_mask;                  
extern int16 h_counter;
extern int16 hc_latch;
extern uint16 v_counter;
extern uint8 im2_flag;
extern uint32 dma_length;
extern int32 fifo_write_cnt;
extern uint32 fifo_lastwrite;
extern uint8 fifo_latency;
extern uint8 vdp_pal;
extern double vdp_timings[4][4];

extern uint8 *vctab;
extern uint8 *hctab;
extern uint8 vc_ntsc_224[262];
extern uint8 vc_pal_224[313];
extern uint8 vc_pal_240[313];
extern uint8 cycle2hc32[488];
extern uint8 cycle2hc40[488];

/* Function prototypes */
extern void vdp_init(void);
extern void vdp_reset(void);
extern void vdp_shutdown(void);
extern void vdp_restore(uint8 *vdp_regs);
extern void vdp_ctrl_w(unsigned int data);
extern unsigned int vdp_ctrl_r(void);
extern void vdp_data_w(unsigned int data);
extern unsigned int vdp_data_r(void);
extern unsigned int vdp_hvc_r(void);
extern void vdp_reg_w(unsigned int r, unsigned int d);
extern void dma_update();
extern void vdp_test_w(unsigned int value);


#endif /* _VDP_H_ */
