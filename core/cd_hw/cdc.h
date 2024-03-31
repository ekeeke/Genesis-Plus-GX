/***************************************************************************************
 *  Genesis Plus
 *  CD data controller (LC8951x compatible)
 *
 *  Copyright (C) 2012-2024  Eke-Eke (Genesis Plus GX)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/
#pragma once

#define cdc scd.cdc_hw

#define CDC_MAIN_CPU_ACCESS 0x42
#define CDC_SUB_CPU_ACCESS  0x43

/* CDC hardware */
typedef struct
{
  uint8_t ifstat;
  uint8_t ifctrl;
  reg16_t dbc;
  reg16_t dac;
  reg16_t pt;
  reg16_t wa;
  uint8_t ctrl[2];
  uint8_t head[2][4];
  uint8_t stat[4];
  int cycles[2];
  uint8_t ram[0x4000 + 2352]; /* 16K external RAM (with one block overhead to handle buffer overrun) */
  uint8_t ar_mask;
  uint8_t irq; /* invert of CDC /INT output */
} cdc_t; 

/* Function prototypes */
extern void cdc_init(void);
extern void cdc_reset(void);
extern int cdc_context_save(uint8_t *state);
extern int cdc_context_load(uint8_t *state);
extern void cdc_dma_init(void);
extern void cdc_dma_update(unsigned int cycles);
extern void cdc_decoder_update(uint32_t header);
extern void cdc_reg_w(unsigned char data);
extern unsigned char cdc_reg_r(void);
extern unsigned short cdc_host_r(uint8_t cpu_access);

