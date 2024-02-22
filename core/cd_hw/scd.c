/***************************************************************************************
 *  Genesis Plus
 *  Mega CD / Sega CD hardware
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

#include "shared.h"

/*--------------------------------------------------------------------------*/
/* Unused area (return open bus data, i.e prefetched instruction word)      */
/*--------------------------------------------------------------------------*/
static unsigned int s68k_read_bus_8(unsigned int address)
{
#ifdef LOGERROR
  error("[SUB 68k] Unused read8 %08X (%08X)\n", address, s68k.pc);
#endif
  address = s68k.pc | (address & 1);
  return READ_BYTE(s68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff);
}

static unsigned int s68k_read_bus_16(unsigned int address)
{
#ifdef LOGERROR
  error("[SUB 68k] Unused read16 %08X (%08X)\n", address, s68k.pc);
#endif
  address = s68k.pc;
  return *(uint16 *)(s68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff));
}

static void s68k_unused_8_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("[SUB 68k] Unused write8 %08X = %02X (%08X)\n", address, data, s68k.pc);
#endif
}

static void s68k_unused_16_w(unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("[SUB 68k] Unused write16 %08X = %04X (%08X)\n", address, data, s68k.pc);
#endif
}

/*--------------------------------------------------------------------------*/
/* Locked area (cause SUB-CPU to wait for /DTACK assertion)    */
/*--------------------------------------------------------------------------*/

static void s68k_lockup_w_8 (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error ("[SUB 68k] Lockup write8 %08X = %02X (%08X)\n", address, data, s68k.pc);
#endif
  s68k_pulse_wait(address, 1);
}

static void s68k_lockup_w_16 (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error ("[SUB 68k] Lockup write16 %08X = %04X (%08X)\n", address, data, s68k.pc);
#endif
  s68k_pulse_wait(address, 1);
}

static unsigned int s68k_lockup_r_8 (unsigned int address)
{ 
#ifdef LOGERROR
  error ("[SUB 68k] Lockup read8 %08X.b (%08X)\n", address, s68k.pc);
#endif
  s68k_pulse_wait(address, 0);
  return 0xff;
}

static unsigned int s68k_lockup_r_16 (unsigned int address)
{
#ifdef LOGERROR
  error ("[SUB 68k] Lockup read16 %08X.w (%08X)\n", address, s68k.pc);
#endif
  s68k_pulse_wait(address, 0);
  return 0xffff;
}

/*--------------------------------------------------------------------------*/
/* PRG-RAM DMA access                                                       */
/*--------------------------------------------------------------------------*/
void prg_ram_dma_w(unsigned int length)
{
  uint16 data;

  /* 16-bit DMA only */
  unsigned int words = length >> 1;

  /* CDC buffer source address */
  uint16 src_index = cdc.dac.w & 0x3ffe;

  /* PRG-RAM destination address*/
  uint32 dst_index = (scd.regs[0x0a>>1].w << 3) & 0x7fffe;

  /* update DMA destination address */
  scd.regs[0x0a>>1].w += (words >> 2);

  /* update DMA source address */
  cdc.dac.w += (words << 1);

  /* DMA transfer */
  while (words--)
  {
    /* read 16-bit word from CDC RAM buffer (big-endian format) */
    data = READ_WORD(cdc.ram, src_index);

    /* write 16-bit word to PRG-RAM */
    *(uint16 *)(scd.prg_ram + dst_index) = data ;

    /* increment CDC buffer source address */
    src_index = (src_index + 2) & 0x3ffe;

    /* increment PRG-RAM destination address */
    dst_index = (dst_index + 2) & 0x7fffe;
  }
}

/*--------------------------------------------------------------------------*/
/* PRG-RAM write protected area                                             */
/*--------------------------------------------------------------------------*/
static void prg_ram_write_byte(unsigned int address, unsigned int data)
{
  address &= 0x7ffff;
  if (address >= (scd.regs[0x02>>1].byte.h << 9))
  {
    WRITE_BYTE(scd.prg_ram, address, data);
    return;
  }
#ifdef LOGERROR
  error("[SUB 68k] PRG-RAM protected write8 %08X = %02X (%08X)\n", address, data, s68k.pc);
#endif
}

static void prg_ram_write_word(unsigned int address, unsigned int data)
{
  address &= 0x7fffe;
  if (address >= (scd.regs[0x02>>1].byte.h << 9))
  {
    *(uint16 *)(scd.prg_ram + address) = data;
    return;
  }
#ifdef LOGERROR
  error("[SUB 68k] PRG-RAM protected write16 %08X = %02X (%08X)\n", address, data, s68k.pc);
#endif
}

/*--------------------------------------------------------------------------*/
/* PRG-RAM bank mirrored access                                             */
/*--------------------------------------------------------------------------*/
static unsigned int prg_ram_z80_read_byte(unsigned int address)
{
  int offset = (address >> 16) & 0x03;

  if (zbank_memory_map[offset].read)
  {
    return zbank_memory_map[offset].read(address);
  }

  return READ_BYTE(m68k.memory_map[offset].base, address & 0xffff);
}

static void prg_ram_z80_write_byte(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x03;

  if (zbank_memory_map[offset].write)
  {
    zbank_memory_map[offset].write(address, data);
  }
  else
  {
    WRITE_BYTE(m68k.memory_map[offset].base, address & 0xffff, data);
  }
}

static unsigned int prg_ram_m68k_read_byte(unsigned int address)
{
  int offset = (address >> 16) & 0x03;

  if (m68k.memory_map[offset].read8)
  {
    return m68k.memory_map[offset].read8(address);
  }

  return READ_BYTE(m68k.memory_map[offset].base, address & 0xffff);
}

static unsigned int prg_ram_m68k_read_word(unsigned int address)
{
  int offset = (address >> 16) & 0x03;

  if (m68k.memory_map[offset].read16)
  {
    return m68k.memory_map[offset].read16(address);
  }

  return *(uint16 *)(m68k.memory_map[offset].base + (address & 0xffff));
}

static void prg_ram_m68k_write_byte(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x03;

  if (m68k.memory_map[offset].write8)
  {
    m68k.memory_map[offset].write8(address, data);
  }
  else
  {
    WRITE_BYTE(m68k.memory_map[offset].base, address & 0xffff, data);
  }
}

static void prg_ram_m68k_write_word(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x03;

  if (m68k.memory_map[offset].write16)
  {
    m68k.memory_map[offset].write16(address, data);
  }
  else
  {
    *(uint16 *)(m68k.memory_map[offset].base + (address & 0xffff)) = data;
  }
}

/*--------------------------------------------------------------------------*/
/* Word-RAM bank mirrored access                                            */
/*--------------------------------------------------------------------------*/
static unsigned int word_ram_z80_read_byte(unsigned int address)
{
  int offset = (address >> 16) & 0x23;

  if (zbank_memory_map[offset].read)
  {
    return zbank_memory_map[offset].read(address);
  }

  return READ_BYTE(m68k.memory_map[offset].base, address & 0xffff);
}

static void word_ram_z80_write_byte(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x23;

  if (zbank_memory_map[offset].write)
  {
    zbank_memory_map[offset].write(address, data);
  }
  else
  {
    *(uint16 *)(m68k.memory_map[offset].base + (address & 0xfffe)) = data | (data << 8);
  }
}

static unsigned int word_ram_m68k_read_byte(unsigned int address)
{
  int offset = (address >> 16) & 0x23;

  if (m68k.memory_map[offset].read8)
  {
    return m68k.memory_map[offset].read8(address);
  }

  return READ_BYTE(m68k.memory_map[offset].base, address & 0xffff);
}

static unsigned int word_ram_m68k_read_word(unsigned int address)
{
  int offset = (address >> 16) & 0x23;

  if (m68k.memory_map[offset].read16)
  {
    return m68k.memory_map[offset].read16(address);
  }

  return *(uint16 *)(m68k.memory_map[offset].base + (address & 0xffff));
}

static void word_ram_m68k_write_byte(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x23;

  if (m68k.memory_map[offset].write8)
  {
    m68k.memory_map[offset].write8(address, data);
  }
  else
  {
    *(uint16 *)(m68k.memory_map[offset].base + (address & 0xfffe)) = data | (data << 8);
  }
}

static void word_ram_m68k_write_word(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x23;

  if (m68k.memory_map[offset].write16)
  {
    m68k.memory_map[offset].write16(address, data);
  }
  else
  {
    *(uint16 *)(m68k.memory_map[offset].base + (address & 0xffff)) = data;
  }
}

static unsigned int word_ram_s68k_read_byte(unsigned int address)
{
  int offset = (address >> 16) & 0x0f;

  if (s68k.memory_map[offset].read8)
  {
    return s68k.memory_map[offset].read8(address);
  }

  return READ_BYTE(s68k.memory_map[offset].base, address & 0xffff);
}

static unsigned int word_ram_s68k_read_word(unsigned int address)
{
  int offset = (address >> 16) & 0x0f;

  if (s68k.memory_map[offset].read16)
  {
    return s68k.memory_map[offset].read16(address);
  }

  return *(uint16 *)(s68k.memory_map[offset].base + (address & 0xffff));
}

static void word_ram_s68k_write_byte(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x0f;

  if (s68k.memory_map[offset].write8)
  {
    s68k.memory_map[offset].write8(address, data);
  }
  else
  {
    *(uint16 *)(s68k.memory_map[offset].base + (address & 0xfffe)) = data | (data << 8);
  }
}

static void word_ram_s68k_write_word(unsigned int address, unsigned int data)
{
  int offset = (address >> 16) & 0x0f;

  if (s68k.memory_map[offset].write16)
  {
    s68k.memory_map[offset].write16(address, data);
  }
  else
  {
    *(uint16 *)(s68k.memory_map[offset].base + (address & 0xffff)) = data;
  }
}

/*--------------------------------------------------------------------------*/
/* internal backup RAM (8KB)                                                */
/*--------------------------------------------------------------------------*/
static unsigned int bram_read_byte(unsigned int address)
{
  /* LSB only */
  if (address & 0x01)
  {
    return scd.bram[(address >> 1) & 0x1fff];
  }

  return 0xff;
}

static unsigned int bram_read_word(unsigned int address)
{
  return (scd.bram[(address >> 1) & 0x1fff] | 0xff00);
}

static void bram_write_byte(unsigned int address, unsigned int data)
{
  /* LSB only */
  if (address & 0x01)
  {
    scd.bram[(address >> 1) & 0x1fff] = data;
  }
}

static void bram_write_word(unsigned int address, unsigned int data)
{
  scd.bram[(address >> 1) & 0x1fff] = data & 0xff;
}

/*--------------------------------------------------------------------------*/
/* SUB-CPU polling detection and MAIN-CPU synchronization                   */
/*--------------------------------------------------------------------------*/

static void s68k_poll_detect(unsigned int reg_mask)
{
  /* detect SUB-CPU register polling */
  if (s68k.poll.detected & reg_mask)
  {
    if (s68k.cycles <= s68k.poll.cycle)
    {
      if (s68k.pc == s68k.poll.pc)
      {
        /* SUB-CPU polling confirmed ? */
        if (s68k.poll.detected & 1)
        {
          /* idle SUB-CPU until register is modified */
#ifdef LOG_SCD
          error("s68k stopped from %d cycles\n", s68k.cycles);
#endif
          s68k.cycles = s68k.cycle_end;
          s68k.stopped = reg_mask;
        }
        else
        {
          /* confirm SUB-CPU polling */
          s68k.poll.detected |= 1;
          s68k.poll.cycle = s68k.cycles + 392;
        }
      }
      return;
    }
  }
  else
  {
    /* set SUB-CPU register access flag */
    s68k.poll.detected = reg_mask;
  }

  /* reset SUB-CPU polling detection */
  s68k.poll.cycle = s68k.cycles + 392;
  s68k.poll.pc = s68k.pc;
}

static void s68k_poll_sync(unsigned int reg_mask)
{
  /* relative MAIN-CPU cycle counter */
  unsigned int cycles = (s68k.cycles * MCYCLES_PER_LINE) / SCYCLES_PER_LINE;

  if (!m68k.stopped)
  {
    /* save current MAIN-CPU end cycle count (recursive execution is possible) */
    int end_cycle = m68k.cycle_end;

    /* sync MAIN-CPU with SUB-CPU */
    m68k_run(cycles);

    /* restore MAIN-CPU end cycle count */
    m68k.cycle_end = end_cycle;
  }

  /* MAIN-CPU idle on register polling ? */
  if (m68k.stopped & reg_mask)
  {
    /* sync MAIN-CPU with SUB-CPU */
    m68k.cycles = cycles;

    /* restart MAIN-CPU */
    m68k.stopped = 0;
#ifdef LOG_SCD
    error("m68k started from %d cycles\n", cycles);
#endif
  }

  /* clear CPU register access flags */
  s68k.poll.detected &= ~reg_mask;
  m68k.poll.detected &= ~reg_mask;
}

static void m68k_sync(void)
{
  if (!m68k.stopped)
  {
    /* relative MAIN-CPU cycle counter */
    unsigned int cycles = (s68k.cycles * MCYCLES_PER_LINE) / SCYCLES_PER_LINE;

    /* save current MAIN-CPU end cycle count (recursive execution is possible) */
    int end_cycle = m68k.cycle_end;

    /* sync MAIN-CPU with SUB-CPU */
    m68k_run(cycles);

    /* restore MAIN-CPU end cycle count */
    m68k.cycle_end = end_cycle;
  }
}

/*--------------------------------------------------------------------------*/
/* PCM chip & Gate-Array area                                               */
/*--------------------------------------------------------------------------*/

static unsigned int scd_read_byte(unsigned int address)
{
  /* PCM area (8K) mirrored into $xF0000-$xF7FFF */
  if (!(address & 0x8000))
  {
    /* get /LDS only */
    if (address & 1)
    {
      return pcm_read((address >> 1) & 0x1fff, s68k.cycles);
    }

    return s68k_read_bus_8(address);
  }

#ifdef LOG_SCD
  error("[%d][%d]read byte CD register %X (%X)\n", v_counter, s68k.cycles, address, s68k.pc);
#endif

  /* only A1-A8 are used for decoding */
  address &= 0x1ff;

  /* Memory Mode */
  if (address == 0x03)
  {
    s68k_poll_detect(1<<0x03);

    /* mask BK0 and BK1 bits on SUB-CPU side */
    return scd.regs[0x03>>1].byte.l & 0x1f;
  }

  /* MAIN-CPU communication flags */
  if (address == 0x0e)
  {
    s68k_poll_detect(1<<0x0e);
    return scd.regs[0x0e>>1].byte.h;
  }

  /* CDC transfer status */
  if (address == 0x04)
  {
    s68k_poll_detect(1<<0x04);
    return scd.regs[0x04>>1].byte.h;
  }

  /* GFX operation status */
  if (address == 0x58)
  {
    s68k_poll_detect(1<<0x08);
    return scd.regs[0x58>>1].byte.h;
  }

  /* CDC register data */
  if (address == 0x07)
  {
    return cdc_reg_r();
  }

  /* LED status */
  if (address == 0x00)
  {
    /* register $00 is reserved for MAIN-CPU, we use $06 instead */
    return scd.regs[0x06>>1].byte.h;
  }

  /* RESET status */
  if (address == 0x01)
  {
    /* always return 1 */
    return 0x01;
  }

  /* Font data */
  if ((address >= 0x50) && (address <= 0x57))
  {
    /* shifted 4-bit input (xxxx00) */
    uint8 bits = (scd.regs[0x4e>>1].w >> (((address & 6) ^ 6) << 1)) << 2;

    /* color code */
    uint8 code = scd.regs[0x4c>>1].byte.l;
    
    /* 16-bit font data (4 pixels = 16 bits) */
    uint16 data = (code >> (bits & 4)) & 0x0f;

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 4) & 0xf0);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 8) & 0xf00);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 12) & 0xf000);

    return (address & 1) ? (data & 0xff) : (data >> 8);
  }

  /* MAIN-CPU communication words */
  if ((address & 0x1f0) == 0x10)
  {
    s68k_poll_detect(1 << (address & 0x1f));
  }

  /* Subcode buffer */
  else if (address & 0x100)
  {
    /* 64 x 16-bit mirrored */
    address &= 0x17f;
  }

  /* default registers */
  if (address & 1)
  {
    /* register LSB */
    return scd.regs[address >> 1].byte.l;
  }

  /* register MSB */
  return scd.regs[address >> 1].byte.h;
}

static unsigned int scd_read_word(unsigned int address)
{
  /* PCM area (8K) mirrored into $xF0000-$xF7FFF */
  if (!(address & 0x8000))
  {
    /* get /LDS only */
    return pcm_read((address >> 1) & 0x1fff, s68k.cycles);
  }

#ifdef LOG_SCD
  error("[%d][%d]read word CD register %X (%X)\n", v_counter, s68k.cycles, address, s68k.pc);
#endif

  /* only A1-A8 are used for decoding */
  address &= 0x1ff;

  /* Memory Mode */
  if (address == 0x02)
  {
    s68k_poll_detect(1<<0x03);

    /* mask BK0 and BK1 bits on SUB-CPU side */
    return scd.regs[0x03>>1].w & 0xff1f;
  }

  /* CDC host data (word access only ?) */
  if (address == 0x08)
  {
    return cdc_host_r(CDC_SUB_CPU_ACCESS);
  }

  /* LED & RESET status */
  if (address == 0x00)
  {
    /* register $00 is reserved for MAIN-CPU, we use $06 instead */
    return scd.regs[0x06>>1].w;
  }

  /* Stopwatch counter (word access only ?) */
  if (address == 0x0c)
  {
    /* cycle-accurate counter value */
    return (scd.regs[0x0c>>1].w + ((s68k.cycles - scd.stopwatch) / TIMERS_SCYCLES_RATIO)) & 0xfff;
  }

  /* Font data */
  if ((address >= 0x50) && (address <= 0x56))
  {
    /* shifted 4-bit input (xxxx00) */
    uint8 bits = (scd.regs[0x4e>>1].w >> (((address & 6) ^ 6) << 1)) << 2;

    /* color code */
    uint8 code = scd.regs[0x4c>>1].byte.l;
    
    /* 16-bit font data (4 pixels = 16 bits) */
    uint16 data = (code >> (bits & 4)) & 0x0f;

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 4) & 0xf0);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 8) & 0xf00);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 12) & 0xf000);

    return data;
  }

  /* CDC register data */
  if (address == 0x06)
  {
    return cdc_reg_r();
  }

  /* MAIN-CPU communication words */
  if ((address & 0x1f0) == 0x10)
  {
    /* sync MAIN-CPU with SUB-CPU (fixes Mighty Morphin Power Rangers) */
    m68k_sync();
    s68k_poll_detect(3 << (address & 0x1e));
  }

  /* Subcode buffer */
  else if (address & 0x100)
  {
    /* 64 x 16-bit mirrored */
    address &= 0x17f;
  }

  /* default registers */
  return scd.regs[address >> 1].w;
}

INLINE void word_ram_switch(uint8 mode)
{
  int i;
  uint16 *ptr1 = (uint16 *)(scd.word_ram_2M);
  uint16 *ptr2 = (uint16 *)(scd.word_ram[0]);
  uint16 *ptr3 = (uint16 *)(scd.word_ram[1]);

  if (mode & 0x04)
  {
    /* 2M -> 1M mode */
    for (i=0; i<0x10000; i++)
    {
      *ptr2++=*ptr1++;
      *ptr3++=*ptr1++;
    }

    /* MAIN-CPU: $200000-$21FFFF is mapped to Word-RAM 0 or 1 */
    for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
    {
      m68k.memory_map[i].read8   = NULL;
      m68k.memory_map[i].read16  = NULL;
      m68k.memory_map[i].write8  = NULL;
      m68k.memory_map[i].write16 = NULL;
      zbank_memory_map[i].read   = NULL;
      zbank_memory_map[i].write  = NULL;
    }

    /* SUB-CPU: $0C0000-$0DFFFF is mapped to Word-RAM 0 or 1 */
    for (i=0x0c; i<0x0e; i++)
    {
      s68k.memory_map[i].read8   = NULL;
      s68k.memory_map[i].read16  = NULL;
      s68k.memory_map[i].write8  = NULL;
      s68k.memory_map[i].write16 = NULL;
    }
  }
  else
  {
    /* 1M -> 2M mode */
    for (i=0; i<0x10000; i++)
    {
      *ptr1++=*ptr2++;
      *ptr1++=*ptr3++;
    }

    /* MAIN-CPU: $200000-$21FFFF is mapped to 256K Word-RAM (lower 128K) */
    for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
    {
      m68k.memory_map[i].base = scd.word_ram_2M + ((i & 0x03) << 16);
    }

    /* SUB-CPU: $0C0000-$0DFFFF is unmapped */
    for (i=0x0c; i<0x0e; i++)
    {
      s68k.memory_map[i].read8   = s68k_read_bus_8;
      s68k.memory_map[i].read16  = s68k_read_bus_16;
      s68k.memory_map[i].write8  = s68k_unused_8_w;
      s68k.memory_map[i].write16 = s68k_unused_16_w;
    }
  }
}

static void scd_write_word(unsigned int address, unsigned int data);

static void scd_write_byte(unsigned int address, unsigned int data)
{
  /* PCM area (8K) mirrored into $xF0000-$xF7FFF */
  if (!(address & 0x8000))
  {
    /* get /LDS only */
    if (address & 1)
    {
      pcm_write((address >> 1) & 0x1fff, data, s68k.cycles);
      return;
    }

    s68k_unused_8_w(address, data);
    return;
  }

#ifdef LOG_SCD
  error("[%d][%d]write byte CD register %X -> 0x%02x (%X)\n", v_counter, s68k.cycles, address, data, s68k.pc);
#endif

  /* Gate-Array registers */
  switch (address & 0x1ff)
  {
    case 0x00: /* LED status */
    {
      /* register $00 is reserved for MAIN-CPU, use $06 instead */
      scd.regs[0x06 >> 1].byte.h = data;
      return;
    }

    case 0x01: /* RESET status */
    {
      /* RESET bit cleared ? */
      if (!(data & 0x01))
      {
        /* reset CD hardware */
        scd_reset(0);
      }
      return;
    }

    case 0x03: /* Memory Mode */
    case 0x02: /* !LDS and !UDS are ignored (verified on real hardware, cf. Krikzz's mcd-verificator) */
    {
      s68k_poll_sync(1<<0x03);

      /* detect MODE & RET bits modifications */
      if ((data ^ scd.regs[0x03 >> 1].byte.l) & 0x05)
      {
        int i;

        /* MODE bit */
        if (data & 0x04)
        {
          /* 2M->1M mode switch */
          if (!(scd.regs[0x03 >> 1].byte.l & 0x04))
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x04);
          }

          /* RET bit in 1M Mode */
          if (data & 0x01)
          {
            /* Word-RAM 1 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 1 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_1_read8;
              m68k.memory_map[i].read16  = cell_ram_1_read16;
              m68k.memory_map[i].write8  = cell_ram_1_write8;
              m68k.memory_map[i].write16 = cell_ram_1_write16;
              zbank_memory_map[i].read   = cell_ram_1_read8;
              zbank_memory_map[i].write  = cell_ram_1_write8;
            }

            /* Word-RAM 0 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_0_read8;
              s68k.memory_map[i].read16  = dot_ram_0_read16;
              s68k.memory_map[i].write8  = dot_ram_0_write8;
              s68k.memory_map[i].write16 = dot_ram_0_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 0 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
            }

            /* writing 1 to RET bit in 1M mode returns Word-RAM to MAIN-CPU in 2M mode */
            scd.dmna = 0;
          }
          else
          {
            /* Word-RAM 0 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 0 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_0_read8;
              m68k.memory_map[i].read16  = cell_ram_0_read16;
              m68k.memory_map[i].write8  = cell_ram_0_write8;
              m68k.memory_map[i].write16 = cell_ram_0_write16;
              zbank_memory_map[i].read   = cell_ram_0_read8;
              zbank_memory_map[i].write  = cell_ram_0_write8;
            }

            /* Word-RAM 1 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_1_read8;
              s68k.memory_map[i].read16  = dot_ram_1_read16;
              s68k.memory_map[i].write8  = dot_ram_1_write8;
              s68k.memory_map[i].write16 = dot_ram_1_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 1 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
            }
          }

          /* clear DMNA bit (swap completed) */
          scd.regs[0x02 >> 1].byte.l = (scd.regs[0x02 >> 1].byte.l & ~0x1f) | (data & 0x1d);
          return;
        }
        else
        {
          /* 1M->2M mode switch */
          if (scd.regs[0x02 >> 1].byte.l & 0x04)
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x00);

            /* RET bit set during 1M mode ? */
            data |= ~scd.dmna & 0x01;
            
            /* check if RET bit is cleared */
            if (!(data & 0x01))
            {
              /* MAIN-CPU: $200000-$23FFFF is unmapped */
              for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
              {
                m68k.memory_map[i].read8   = m68k_read_bus_8;
                m68k.memory_map[i].read16  = m68k_read_bus_16;
                m68k.memory_map[i].write8  = m68k_unused_8_w;
                m68k.memory_map[i].write16 = m68k_unused_16_w;
                zbank_memory_map[i].read   = zbank_unused_r;
                zbank_memory_map[i].write  = zbank_unused_w;
              }

              /* SUB-CPU: access to Word-RAM in $08FFFF-$0BFFFF is unlocked (/DTACK asserted) */
              for (i=0x08; i<0x0c; i++)
              {
                s68k.memory_map[i].read8   = NULL;
                s68k.memory_map[i].read16  = NULL;
                s68k.memory_map[i].write8  = NULL;
                s68k.memory_map[i].write16 = NULL;
              }

              /* set DMNA bit */
              data |= 0x02;

              /* mask BK0-1 bits (MAIN-CPU side only) */
              scd.regs[0x02 >> 1].byte.l = (scd.regs[0x02 >> 1].byte.l & ~0x1f) | (data & 0x1f);
              return;
            }
          }

          /* RET bit set in 2M mode */
          if (data & 0x01)
          {
            /* check if graphics operation is running */
            if (scd.regs[0x58>>1].byte.h & 0x80)
            {
              /* synchronize GFX processing with SUB-CPU */
              gfx_update(s68k.cycles);
            }

            /* check if CDC DMA to 2M Word-RAM is running */
            if (cdc.dma_w == word_ram_2M_dma_w)
            {
              /* synchronize CDC DMA with SUB-CPU */
              cdc_dma_update(s68k.cycles);

              /* halt CDC DMA to 2M Word-RAM (if still running) */
              cdc.halted_dma_w = cdc.dma_w;
              cdc.dma_w = 0;
            }

            /* Word-RAM is returned to MAIN-CPU */
            scd.dmna = 0;

            /* MAIN-CPU: $200000-$23FFFF is mapped to Word-RAM */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
            {
              m68k.memory_map[i].read8   = NULL;
              m68k.memory_map[i].read16  = NULL;
              m68k.memory_map[i].write8  = NULL;
              m68k.memory_map[i].write16 = NULL;
              zbank_memory_map[i].read   = NULL;
              zbank_memory_map[i].write  = NULL;
            }

            /* SUB-CPU: access to Word-RAM in $08FFFF-$0BFFFF is locked (/DTACK not asserted) */
            for (i=0x08; i<0x0c; i++)
            {
              s68k.memory_map[i].read8   = s68k_lockup_r_8;
              s68k.memory_map[i].read16  = s68k_lockup_r_16;
              s68k.memory_map[i].write8  = s68k_lockup_w_8;
              s68k.memory_map[i].write16 = s68k_lockup_w_16;
            }

            /* clear DMNA bit */
            scd.regs[0x02 >> 1].byte.l = (scd.regs[0x02 >> 1].byte.l & ~0x1f) | (data & 0x1d);
            return;
          }
        }
      }

      /* update PM0-1 & MODE bits */
      scd.regs[0x02 >> 1].byte.l = (scd.regs[0x02 >> 1].byte.l & ~0x1c) | (data & 0x1c);
      return;
    }

    case 0x04: /* CDC mode */
    {
      scd.regs[0x04 >> 1].byte.h = data & 0x07;

      /* synchronize CDC DMA (if running) with SUB-CPU */
      if (cdc.dma_w)
      {
        cdc_dma_update(s68k.cycles);
      }

      /* reinitialize CDC data transfer destination (verified on real hardware, cf. Krikzz's mcd-verificator) */
      cdc_dma_init();

      /* reset CDC DMA address (verified on real hardware, cf. Krikzz's mcd-verificator) */
      scd.regs[0x0a >> 1].w = 0;
      return;
    }

    case 0x05: /* CDC register address */
    {
      scd.regs[0x04 >> 1].byte.l = data & cdc.ar_mask;
      return;
    }

    case 0x07: /* CDC register write */
    {
      cdc_reg_w(data);
      return;
    }

    case 0x0f:  /* SUB-CPU communication flags */
    case 0x0e:  /* !LDS and !UDS are ignored (verified on real hardware, cf. Krikzz's mcd-verificator, Space Ace, Dragon's Lair) */
    {
      s68k_poll_sync(1<<0x0f);
      scd.regs[0x0f>>1].byte.l = data;
      return;
    }

    case 0x31: /* Timer */
    case 0x30: /* !LDS and !UDS are ignored (verified on real hardware, cf. Krikzz's mcd-verificator) */
    {
      /* reload timer (one timer clock = 384 CPU cycles) */
      scd.timer = data * TIMERS_SCYCLES_RATIO;

      /* only non-zero data starts timer, writing zero stops it */
      if (data)
      {
        /* adjust regarding current CPU cycle */
        scd.timer += (s68k.cycles - scd.cycles);
      }

      scd.regs[0x30>>1].byte.l = data;
      return;
    }

    case 0x33: /* Interrupts */
    {
      /* update register value before updating interrupts */
      scd.regs[0x32>>1].byte.l = data;

      /* update IEN2 flag */
      scd.regs[0x00].byte.h = (scd.regs[0x00].byte.h & 0x7f) | ((data & 0x04) << 5);

      /* clear level 1 interrupt if disabled ("Batman Returns" option menu) */
      scd.pending &= 0xfd | (data & 0x02);

      /* update IRQ level */
      s68k_update_irq((scd.pending & data) >> 1);
      return;
    }

    case 0x4d: /* Font Color */
    case 0x4c: /* !LDS and !UDS are ignored (verified on real hardware, cf. Krikzz's mcd-verificator) */
    {
       scd.regs[0x4c>>1].byte.l = data;
       break;
    }

    default:
    {
      uint16 reg_16 = address & 0x1fe;

      /* SUB-CPU communication words */
      if ((reg_16 >= 0x20) && (reg_16 <= 0x2f))
      {
        s68k_poll_sync(1 << ((address - 0x10) & 0x1f));
      }

      /* MAIN-CPU communication words */
      else if ((reg_16 >= 0x10) && (reg_16 <= 0x1f))
      {
        /* read-only (Sega Classic Arcade Collection) */
        return;
      }

      /* word-only registers */
      else if (((reg_16 >= 0x08) && (reg_16 <= 0x0d)) ||
               ((reg_16 >= 0x34) && (reg_16 <= 0x35)) ||
               ((reg_16 >= 0x5a) && (reg_16 <= 0x67)))
      {
        scd_write_word(address, (data << 8) | (data & 0xff));
        return;
      }

      /* default registers */
      if (address & 1)
      {
        /* register LSB */
        scd.regs[reg_16 >> 1].byte.l = data;
        return;
      }

      /* register MSB */
      scd.regs[reg_16 >> 1].byte.h = data;
      return;
    }
  }
}

static void scd_write_word(unsigned int address, unsigned int data)
{
  /* PCM area (8K) mirrored into $xF0000-$xF7FFF */
  if (!(address & 0x8000))
  {
    /* get /LDS only */
    pcm_write((address >> 1) & 0x1fff, data & 0xff, s68k.cycles);
    return;
  }

#ifdef LOG_SCD
  error("[%d][%d]write word CD register %X -> 0x%04x (%X)\n", v_counter, s68k.cycles, address, data, s68k.pc);
#endif

  /* Gate-Array registers */
  switch (address & 0x1fe)
  {
    case 0x00: /* LED status & RESET */
    {
      /* only update LED status (register $00 is reserved for MAIN-CPU, use $06 instead) */
      scd.regs[0x06>>1].byte.h = data >> 8;

      /* RESET bit cleared ? */
      if (!(data & 0x01))
      {
        /* reset CD hardware */
        scd_reset(0);
      }
      return;
    }

    case 0x02: /* Memory Mode */
    {
      s68k_poll_sync(1<<0x03);

      /* detect MODE & RET bits modifications */
      if ((data ^ scd.regs[0x03>>1].byte.l) & 0x05)
      {
        int i;

        /* MODE bit */
        if (data & 0x04)
        {
          /* 2M->1M mode switch */
          if (!(scd.regs[0x03 >> 1].byte.l & 0x04))
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x04);
          }

          /* RET bit in 1M Mode */
          if (data & 0x01)
          {
            /* Word-RAM 1 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 1 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_1_read8;
              m68k.memory_map[i].read16  = cell_ram_1_read16;
              m68k.memory_map[i].write8  = cell_ram_1_write8;
              m68k.memory_map[i].write16 = cell_ram_1_write16;
              zbank_memory_map[i].read   = cell_ram_1_read8;
              zbank_memory_map[i].write  = cell_ram_1_write8;
            }

            /* Word-RAM 0 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_0_read8;
              s68k.memory_map[i].read16  = dot_ram_0_read16;
              s68k.memory_map[i].write8  = dot_ram_0_write8;
              s68k.memory_map[i].write16 = dot_ram_0_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 0 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
            }

            /* writing 1 to RET bit in 1M mode returns Word-RAM to MAIN-CPU in 2M mode */
            scd.dmna = 0;
          }
          else
          {
            /* Word-RAM 0 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 0 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_0_read8;
              m68k.memory_map[i].read16  = cell_ram_0_read16;
              m68k.memory_map[i].write8  = cell_ram_0_write8;
              m68k.memory_map[i].write16 = cell_ram_0_write16;
              zbank_memory_map[i].read   = cell_ram_0_read8;
              zbank_memory_map[i].write  = cell_ram_0_write8;
            }

            /* Word-RAM 1 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_1_read8;
              s68k.memory_map[i].read16  = dot_ram_1_read16;
              s68k.memory_map[i].write8  = dot_ram_1_write8;
              s68k.memory_map[i].write16 = dot_ram_1_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 1 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
            }
          }

          /* clear DMNA bit (swap completed) */
          scd.regs[0x03>>1].byte.l = (scd.regs[0x03>>1].byte.l & ~0x1f) | (data & 0x1d);
          return;
        }
        else
        {
          /* 1M->2M mode switch */
          if (scd.regs[0x03>>1].byte.l & 0x04)
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x00);

            /* RET bit set during 1M mode ? */
            data |= ~scd.dmna & 0x01;

            /* check if RET bit is cleared */
            if (!(data & 0x01))
            {
              /* MAIN-CPU: $200000-$23FFFF is unmapped */
              for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
              {
                m68k.memory_map[i].read8   = m68k_read_bus_8;
                m68k.memory_map[i].read16  = m68k_read_bus_16;
                m68k.memory_map[i].write8  = m68k_unused_8_w;
                m68k.memory_map[i].write16 = m68k_unused_16_w;
                zbank_memory_map[i].read   = zbank_unused_r;
                zbank_memory_map[i].write  = zbank_unused_w;
              }

              /* SUB-CPU: access to Word-RAM in $08FFFF-$0BFFFF is unlocked (/DTACK asserted) */
              for (i=0x08; i<0x0c; i++)
              {
                s68k.memory_map[i].read8   = NULL;
                s68k.memory_map[i].read16  = NULL;
                s68k.memory_map[i].write8  = NULL;
                s68k.memory_map[i].write16 = NULL;
              }

              /* set DMNA bit */
              data |= 0x02;

              /* mask BK0-1 bits (MAIN-CPU side only) */
              scd.regs[0x03>>1].byte.l = (scd.regs[0x03>>1].byte.l & ~0x1f) | (data & 0x1f);
              return;
            }
          }

          /* RET bit set in 2M mode */
          if (data & 0x01)
          {
            /* check if graphics operation is running */
            if (scd.regs[0x58>>1].byte.h & 0x80)
            {
              /* synchronize GFX processing with SUB-CPU */
              gfx_update(s68k.cycles);
            }

            /* check if CDC DMA to 2M Word-RAM is running */
            if (cdc.dma_w == word_ram_2M_dma_w)
            {
              /* synchronize CDC DMA with SUB-CPU */
              cdc_dma_update(s68k.cycles);

              /* halt CDC DMA to 2M Word-RAM (if still running) */
              cdc.halted_dma_w = cdc.dma_w;
              cdc.dma_w = 0;
            }

            /* Word-RAM is returned to MAIN-CPU */
            scd.dmna = 0;

            /* MAIN-CPU: $200000-$23FFFF is mapped to Word-RAM */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
            {
              m68k.memory_map[i].read8   = NULL;
              m68k.memory_map[i].read16  = NULL;
              m68k.memory_map[i].write8  = NULL;
              m68k.memory_map[i].write16 = NULL;
              zbank_memory_map[i].read   = NULL;
              zbank_memory_map[i].write  = NULL;
            }

            /* SUB-CPU: access to Word-RAM in $08FFFF-$0BFFFF is locked (/DTACK not asserted) */
            for (i=0x08; i<0x0c; i++)
            {
              s68k.memory_map[i].read8   = s68k_lockup_r_8;
              s68k.memory_map[i].read16  = s68k_lockup_r_16;
              s68k.memory_map[i].write8  = s68k_lockup_w_8;
              s68k.memory_map[i].write16 = s68k_lockup_w_16;
            }

            /* clear DMNA bit */
            scd.regs[0x03>>1].byte.l = (scd.regs[0x03>>1].byte.l & ~0x1f) | (data & 0x1d);
            return;
          }
        }
      }

      /* update PM0-1 & MODE bits */
      scd.regs[0x03>>1].byte.l = (scd.regs[0x03>>1].byte.l & ~0x1c) | (data & 0x1c);
      return;
    }

    case 0x04: /* CDC mode & register address */
    {
      scd.regs[0x04 >> 1].w = data & (0x0700 | cdc.ar_mask);

      /* synchronize CDC DMA (if running) with SUB-CPU */
      if (cdc.dma_w)
      {
        cdc_dma_update(s68k.cycles);
      }

      /* reinitialize CDC data transfer destination (verified on real hardware, cf. Krikzz's mcd-verificator) */
      cdc_dma_init();

      /* reset CDC DMA address (verified on real hardware, cf. Krikzz's mcd-verificator) */
      scd.regs[0x0a >> 1].w = 0;
      return;
    }

    case 0x06: /* CDC register write */
    {
      cdc_reg_w(data);
      return;
    }

    case 0x08: /* CDC host data */
    {
      /* CDC data is also read (although unused) on write access (verified on real hardware, cf. Krikzz's mcd-verificator) */
      cdc_host_r(CDC_SUB_CPU_ACCESS);
      return;
    }

    case 0x0c: /* Stopwatch */
    {
      /* synchronize the counter with SUB-CPU */
      int ticks = (s68k.cycles - scd.stopwatch) / TIMERS_SCYCLES_RATIO;
      scd.stopwatch += (ticks * TIMERS_SCYCLES_RATIO);

      /* any writes clear the counter */
      scd.regs[0x0c>>1].w = 0;
      return;
    }

    case 0x0e:  /* CPU Communication flags */
    {
      s68k_poll_sync(1<<0x0f);

      /* D8-D15 ignored -> only SUB-CPU flags are updated */
      scd.regs[0x0f>>1].byte.l = data & 0xff;
      return;
    }

    case 0x30: /* Timer */
    {
      /* LSB only */
      data &= 0xff;

      /* reload timer (one timer clock = 384 CPU cycles) */
      scd.timer = data * TIMERS_SCYCLES_RATIO;

      /* only non-zero data starts timer, writing zero stops it */
      if (data)
      {
        /* adjust regarding current CPU cycle */
        scd.timer += (s68k.cycles - scd.cycles);
      }

      scd.regs[0x30>>1].byte.l = data;
      return;
    }

    case 0x32: /* Interrupts */
    {
      /* LSB only */
      data &= 0xff;

      /* update register value before updating interrupts */
      scd.regs[0x32>>1].byte.l = data;

      /* update IEN2 flag */
      scd.regs[0x00].byte.h = (scd.regs[0x00].byte.h & 0x7f) | ((data & 0x04) << 5);

      /* clear pending level 1 interrupt if disabled ("Batman Returns" option menu) */
      scd.pending &= 0xfd | (data & 0x02);

      /* update IRQ level */
      s68k_update_irq((scd.pending & data) >> 1);
      return;
    }

    case 0x34: /* CD Fader */
    {
      /* Wondermega hardware (CXD2554M digital filter) */
      if (scd.type == CD_TYPE_WONDERMEGA)
      {
        /* only MSB is latched by CXD2554M chip, LSB is ignored (8-bit digital filter) */
        /* attenuator data is 7-bit only (bits 0-7) */
        data = (data >> 8) & 0x7f;

        /* scale CXD2554M volume (0-127) to full (LC7883KM compatible) volume range (0-1024) */
        cdd.fader[1] = (1024 * data) / 127 ;
      }

      /* Wondermega M2 / X'Eye hardware (SM5841A digital filter) */
      else if (scd.type == CD_TYPE_WONDERMEGA_M2)
      {
        /* only MSB is latched by SM5841A chip, LSB is ignored (8-bit digital filter) */
        data = data >> 8;

        /* attenuator data is set only when command bit (bit 0) is cleared (other commands are ignored) */
        if (data & 0x01) return;

        /* attenuator data is 7-bit only (bits 8-1) and reverted (bit 1 = msb) */
        /* bit reversing formula taken from http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits */
        data = (((((data * 0x0802) & 0x22110) | ((data * 0x8020) & 0x88440)) * 0x10101) >> 16) & 0x7f;

        /* convert & scale SM5841A attenuation (127-0) to full (LC7883KM compatible) volume range (0-1024) */
        cdd.fader[1] = (1024 * (127 - data)) / 127 ;
      }

      /* default CD hardware (LC7883KM digital filter) */
      else
      {
        /* get LC7883KM volume data (12-bit) */
        cdd.fader[1] = data >> 4 ;
      }

      return;
    }

    case 0x36: /* CDD control */
    {
      /* only bit 2 is writable (bits [1:0] forced to 0 by default) */
      scd.regs[0x37>>1].byte.l = data & 0x04;
      return;
    }

    case 0x4a: /* CDD command 9 (controlled by BIOS, word access only ?) */
    {
      scd.regs[0x4a>>1].w = 0;
      cdd_process();
#ifdef LOG_CDD
      error("CDD command: %02x %02x %02x %02x %02x %02x %02x %02x\n",scd.regs[0x42>>1].byte.h, scd.regs[0x42>>1].byte.l, scd.regs[0x44>>1].byte.h, scd.regs[0x44>>1].byte.l, scd.regs[0x46>>1].byte.h, scd.regs[0x46>>1].byte.l, scd.regs[0x48>>1].byte.h, scd.regs[0x48>>1].byte.l);
      error("CDD status:  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",scd.regs[0x38>>1].byte.h, scd.regs[0x38>>1].byte.l, scd.regs[0x3a>>1].byte.h, scd.regs[0x3a>>1].byte.l, scd.regs[0x3c>>1].byte.h, scd.regs[0x3c>>1].byte.l, scd.regs[0x3e>>1].byte.h, scd.regs[0x3e>>1].byte.l, scd.regs[0x40>>1].byte.h, scd.regs[0x40>>1].byte.l);
#endif
      break;
    }

    case 0x66: /* Trace vector base address */
    {
      scd.regs[0x66>>1].w = data;
      
      /* start GFX operation */
      gfx_start(data, s68k.cycles);
      return;
    }

    default:
    {
      /* SUB-CPU communication words */
      if ((address & 0x1f0) == 0x20)
      {
        s68k_poll_sync(3 << ((address - 0x10) & 0x1e));
      }

      /* MAIN-CPU communication words */
      else if ((address & 0x1f0) == 0x10)
      {
        /* read-only (Sega Classic Arcade Collection) */
        return;
      }

      /* default registers */
      scd.regs[(address >> 1) & 0xff].w = data;
      return;
    }
  }
}

void scd_init(void)
{
  int i;

  /****************************************************************/
  /*  MAIN-CPU low memory map ($000000-$7FFFFF)                   */
  /****************************************************************/

  /* 0x00: boot from CD (default) */
  /* 0x40: boot from cartridge (mode 1) when /CART is asserted */
  int base = scd.cartridge.boot;

  /* $400000-$7FFFFF (resp. $000000-$3FFFFF): cartridge port (/CE0 asserted) */
  cd_cart_init();

  /* $000000-$1FFFFF (resp. $400000-$5FFFFF): expansion port (/ROM asserted) */
  for (i=base+0x00; i<base+0x20; i++)
  {
    /* only VA1-VA17 are connected to expansion port */
    switch (i & 0x02)
    {
      case 0x00:
      {
        /* $000000-$01FFFF (resp. $400000-$41FFFF): internal ROM (128KB), mirrored every 256KB up to $1FFFFF (resp. $5FFFFF) */
        m68k.memory_map[i].base    = scd.bootrom + ((i & 0x01) << 16);
        m68k.memory_map[i].read8   = NULL;
        m68k.memory_map[i].read16  = NULL;
        m68k.memory_map[i].write8  = m68k_unused_8_w;
        m68k.memory_map[i].write16 = m68k_unused_16_w;
        zbank_memory_map[i].read   = NULL;
        zbank_memory_map[i].write  = zbank_unused_w;
        break;
      }

      case 0x02:
      {
        /* $020000-$03FFFF (resp. $420000-$43FFFF): PRG-RAM (first 128KB bank), mirrored every 256KB up to $1FFFFF (resp. $5FFFFF) */
        m68k.memory_map[i].base = scd.prg_ram + ((i & 0x01) << 16);

        /* automatic mirrored range remapping when switching PRG-RAM banks */
        if (i > (base + 0x03))
        {
          m68k.memory_map[i].read8   = prg_ram_m68k_read_byte;
          m68k.memory_map[i].read16  = prg_ram_m68k_read_word;
          m68k.memory_map[i].write8  = prg_ram_m68k_write_byte;
          m68k.memory_map[i].write16 = prg_ram_m68k_write_word;
          zbank_memory_map[i].read   = prg_ram_z80_read_byte;
          zbank_memory_map[i].write  = prg_ram_z80_write_byte;
        }
        else
        {
          m68k.memory_map[i].read8   = NULL;
          m68k.memory_map[i].read16  = NULL;
          m68k.memory_map[i].write8  = NULL;
          m68k.memory_map[i].write16 = NULL;
          zbank_memory_map[i].read   = NULL;
          zbank_memory_map[i].write  = NULL;
        }
        break;
      }
    }
   }

  /* $200000-$3FFFFF (resp. $600000-$7FFFFF): expansion port (/RAS2 asserted) */
  for (i=base+0x20; i<base+0x40; i++)
  {
    /* $200000-$23FFFF (resp. $600000-$63FFFF): Word-RAM in 2M mode (256KB), mirrored up to $3FFFFF (resp. $7FFFFF) */
    m68k.memory_map[i].base  = scd.word_ram_2M + ((i & 0x03) << 16);

    /* automatic mirrored range remapping when switching Word-RAM */
    if (i > (base + 0x23))
    {
      m68k.memory_map[i].read8   = word_ram_m68k_read_byte;
      m68k.memory_map[i].read16  = word_ram_m68k_read_word;
      m68k.memory_map[i].write8  = word_ram_m68k_write_byte;
      m68k.memory_map[i].write16 = word_ram_m68k_write_word;
      zbank_memory_map[i].read   = word_ram_z80_read_byte;
      zbank_memory_map[i].write  = word_ram_z80_write_byte;
    }
    else
    {
      m68k.memory_map[i].read8   = NULL;
      m68k.memory_map[i].read16  = NULL;
      m68k.memory_map[i].write8  = NULL;
      m68k.memory_map[i].write16 = NULL;
      zbank_memory_map[i].read   = NULL;
      zbank_memory_map[i].write  = NULL;
    }
  }

  /****************************************************************/
  /*  SUB-CPU memory map ($000000-$FFFFFF)                        */
  /****************************************************************/

  for (i=0x00; i<0x100; i++)
  {
    /* only A1-A19 are connected to SUB-CPU */
    switch (i & 0x0f) 
    {
      case 0x00:
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:
      case 0x05:
      case 0x06:
      case 0x07:
      {
        /* $000000-$07FFFF (mirrored every 1MB): PRG-RAM (512KB) */
        s68k.memory_map[i].base    = scd.prg_ram + ((i & 0x07) << 16);
        s68k.memory_map[i].read8   = NULL;
        s68k.memory_map[i].read16  = NULL;

        /* first 128KB can be write-protected */
        s68k.memory_map[i].write8  = (i & 0x0e) ? NULL : prg_ram_write_byte;
        s68k.memory_map[i].write16 = (i & 0x0e) ? NULL : prg_ram_write_word;
        break;
      }

      case 0x08:
      case 0x09:
      case 0x0a:
      case 0x0b:
      {
        /* $080000-$0BFFFF (mirrored every 1MB): Word-RAM in 2M mode (256KB)*/
        s68k.memory_map[i].base = scd.word_ram_2M + ((i & 0x03) << 16);

        /* automatic mirrored range remapping when switching Word-RAM */
        if (i > 0x0f)
        {
          s68k.memory_map[i].read8   = word_ram_s68k_read_byte;
          s68k.memory_map[i].read16  = word_ram_s68k_read_word;
          s68k.memory_map[i].write8  = word_ram_s68k_write_byte;
          s68k.memory_map[i].write16 = word_ram_s68k_write_word;
        }
        else
        {
          /* access to Word-RAM in $08FFFF-$0BFFFF is locked by default (/DTACK not asserted) */
          s68k.memory_map[i].read8   = s68k_lockup_r_8;
          s68k.memory_map[i].read16  = s68k_lockup_r_16;
          s68k.memory_map[i].write8  = s68k_lockup_w_8;
          s68k.memory_map[i].write16 = s68k_lockup_w_16;
        }
        break;
      }

      case 0x0c:
      case 0x0d:
      {
        /* $0C0000-$0DFFFF (mirrored every 1MB):  unused in 2M mode (?) */
        s68k.memory_map[i].base = scd.word_ram_2M + ((i & 0x03) << 16);

        /* automatic mirrored range remapping when switching Word-RAM */
        if (i > 0x0f)
        {
          s68k.memory_map[i].read8   = word_ram_s68k_read_byte;
          s68k.memory_map[i].read16  = word_ram_s68k_read_word;
          s68k.memory_map[i].write8  = word_ram_s68k_write_byte;
          s68k.memory_map[i].write16 = word_ram_s68k_write_word;
        }
        else
        {
          s68k.memory_map[i].read8   = s68k_read_bus_8;
          s68k.memory_map[i].read16  = s68k_read_bus_16;
          s68k.memory_map[i].write8  = s68k_unused_8_w;
          s68k.memory_map[i].write16 = s68k_unused_16_w;
        }
        break;
      }

      case 0x0e:
      {
        /* $FE0000-$FEFFFF (mirrored every 1MB): 8KB backup RAM */
        s68k.memory_map[i].base     = NULL;
        s68k.memory_map[i].read8    = bram_read_byte;
        s68k.memory_map[i].read16   = bram_read_word;
        s68k.memory_map[i].write8   = bram_write_byte;
        s68k.memory_map[i].write16  = bram_write_word;
        break;
      }

      case 0x0f:
      {
        /* $FF0000-$FFFFFF (mirrored every 1MB): PCM hardware & SUB-CPU registers  */
        s68k.memory_map[i].base     = NULL;
        s68k.memory_map[i].read8    = scd_read_byte;
        s68k.memory_map[i].read16   = scd_read_word;
        s68k.memory_map[i].write8   = scd_write_byte;
        s68k.memory_map[i].write16  = scd_write_word;
        break;
      }
    }
  }

  /* Initialize CD hardware */
  cdc_init();
  gfx_init();

  /* Initialize CD hardware master clock count per scanline */
  scd.cycles_per_line = (uint32) (MCYCLES_PER_LINE * ((float)SCD_CLOCK / (float)system_clock));

  /* Clear RAM */
  memset(scd.prg_ram, 0x00, sizeof(scd.prg_ram));
  memset(scd.word_ram, 0x00, sizeof(scd.word_ram));
  memset(scd.word_ram_2M, 0x00, sizeof(scd.word_ram_2M));
  memset(scd.bram, 0x00, sizeof(scd.bram));
}

void scd_reset(int hard)
{
  if (hard)
  {
    int i;

    /* Clear all ASIC registers by default */
    memset(scd.regs, 0, sizeof(scd.regs));

    /* Clear pending DMNA write status */
    scd.dmna = 0;

    /* H-INT default vector */
    *(uint16 *)(m68k.memory_map[scd.cartridge.boot].base + 0x70) = 0xFFFF;
    *(uint16 *)(m68k.memory_map[scd.cartridge.boot].base + 0x72) = 0xFFFF;

    /* Power ON initial values (MAIN-CPU side) */
    scd.regs[0x00>>1].w = 0x0002;
    scd.regs[0x02>>1].w = 0x0001;

    /* 2M mode */
    word_ram_switch(0);

    /* MAIN-CPU access to Word-RAM at $200000-$23FFFF is enabled on reset */
    for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
    {
      m68k.memory_map[i].read8   = NULL;
      m68k.memory_map[i].read16  = NULL;
      m68k.memory_map[i].write8  = NULL;
      m68k.memory_map[i].write16 = NULL;
      zbank_memory_map[i].read   = NULL;
      zbank_memory_map[i].write  = NULL;
    }

    /* SUB-CPU access to Word-RAM at $08FFFF-$0BFFFF is locked on reset (/DTACK not asserted) */
    for (i=0x08; i<0x0c; i++)
    {
      s68k.memory_map[i].read8   = s68k_lockup_r_8;
      s68k.memory_map[i].read16  = s68k_lockup_r_16;
      s68k.memory_map[i].write8  = s68k_lockup_w_8;
      s68k.memory_map[i].write16 = s68k_lockup_w_16;
    }

    /* reset PRG-RAM bank on MAIN-CPU side */
    m68k.memory_map[scd.cartridge.boot + 0x02].base = scd.prg_ram;
    m68k.memory_map[scd.cartridge.boot + 0x03].base = scd.prg_ram + 0x10000;

    /* allow access to PRG-RAM from MAIN-CPU */
    m68k.memory_map[scd.cartridge.boot + 0x02].read8   = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x03].read8   = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x02].read16  = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x03].read16  = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x02].write8  = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x03].write8  = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x02].write16 = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x03].write16 = NULL;
    zbank_memory_map[scd.cartridge.boot + 0x02].read   = NULL;
    zbank_memory_map[scd.cartridge.boot + 0x03].read   = NULL;
    zbank_memory_map[scd.cartridge.boot + 0x02].write  = NULL;
    zbank_memory_map[scd.cartridge.boot + 0x03].write  = NULL;

    /* reset & halt SUB-CPU */
    s68k.cycles = 0;
    s68k_pulse_reset();
    s68k_pulse_halt();

    /* Reset frame cycle counter */
    scd.cycles = 0;
  }
  else
  {
    /* TODO: figure what exactly is reset when RESET bit is cleared by SUB-CPU */
    /* Clear only SUB-CPU side registers (communication registers are not cleared, see msu-md-sample.bin) */
    scd.regs[0x04>>1].w = 0x0000;
    scd.regs[0x0c>>1].w = 0x0000;
    memset(&scd.regs[0x30>>1], 0, sizeof(scd.regs) - 0x30);
  }

  /* SUB-CPU side default values */
  scd.regs[0x08>>1].w = 0xffff;
  scd.regs[0x0a>>1].w = 0xffff;
  scd.regs[0x36>>1].w = 0x0100;
  scd.regs[0x40>>1].w = 0x000f;
  scd.regs[0x42>>1].w = 0xffff;
  scd.regs[0x44>>1].w = 0xffff;
  scd.regs[0x46>>1].w = 0xffff;
  scd.regs[0x48>>1].w = 0xffff;
  scd.regs[0x4a>>1].w = 0xffff;

  /* RESET register always return 1 (register $06 is unused by both sides, it is used for SUB-CPU first register) */
  scd.regs[0x06>>1].byte.l = 0x01;

  /* Reset Timer & Stopwatch counters */
  scd.timer = 0;
  scd.stopwatch = s68k.cycles;

  /* Clear pending interrupts */
  scd.pending = 0;

  /* Clear CPU polling detection */
  memset(&m68k.poll, 0, sizeof(m68k.poll));
  memset(&s68k.poll, 0, sizeof(s68k.poll));

  /* reset CDD cycle counter */
  cdd.cycles = (scd.cycles - s68k.cycles) * 3;

  /* Reset CD hardware */
  cdd_reset();
  cdc_reset();
  gfx_reset();
  pcm_reset();
}

void scd_update(unsigned int cycles)
{
  int m68k_end_cycles;
  int s68k_run_cycles;
  int s68k_end_cycles = scd.cycles + SCYCLES_PER_LINE;

  /* run both CPU in sync until end of line */
  do
  {
    /* CD hardware remaining cycles until end of line */
    s68k_run_cycles = s68k_end_cycles - scd.cycles;

    /* check Timer interrupt occurence */
    if ((scd.timer > 0) && (scd.timer < s68k_run_cycles))
    {
      /* adjust Sub-CPU and Main-CPU end cycle counters up to Timer interrupt occurence */
      s68k_run_cycles = scd.timer;
      m68k_end_cycles = mcycles_vdp + ((s68k_run_cycles * MCYCLES_PER_LINE) / SCYCLES_PER_LINE);
    }
    else
    {
      /* default Main-CPU end cycle counter (end of line) */
      m68k_end_cycles = cycles;
    }

    /* run both CPU in sync until required cycle counters */
    m68k_run(m68k_end_cycles);
    s68k_run(scd.cycles + s68k_run_cycles);

    /* increment CD hardware cycle counter */
    scd.cycles += s68k_run_cycles;

    /* CDD processing at 75Hz (one clock = 12500000/75 = 500000/3 CPU clocks) */
    cdd.cycles += (s68k_run_cycles * 3);
    if (cdd.cycles >= (500000 * 4))
    {
      /* reload CDD cycle counter */
      cdd.cycles -= (500000 * 4);

      /* update CDD sector */
      cdd_update();

      /* check if CDD communication is enabled */
      if (scd.regs[0x37>>1].byte.l & 0x04)
      {
        /* pending level 4 interrupt */
        scd.pending |= (1 << 4);

        /* level 4 interrupt enabled */
        if (scd.regs[0x32>>1].byte.l & 0x10)
        {
          /* update IRQ level */
          s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
        }
      }
    }

    /* Timer */
    if (scd.timer)
    {
      /* decrement timer */
      scd.timer -= s68k_run_cycles;
      if (scd.timer <= 0)
      {
        /* reload timer (one timer clock = 384 CPU cycles) */
        scd.timer += (scd.regs[0x30>>1].byte.l * TIMERS_SCYCLES_RATIO);

        /* level 3 interrupt enabled ? */
        if (scd.regs[0x32>>1].byte.l & 0x08)
        {
          /* trigger level 3 interrupt */
          scd.pending |= (1 << 3);

          /* update IRQ level */
          s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);
        }
      }
    }
  }
  while ((m68k.cycles < cycles) || (s68k.cycles < s68k_end_cycles));

  /* update CDC DMA processing (if running) */
  if (cdc.dma_w)
  {
    cdc_dma_update(scd.cycles);
  }

  /* update GFX processing (if started) */
  if (scd.regs[0x58>>1].byte.h & 0x80)
  {
    gfx_update(scd.cycles);
  }
}

void scd_end_frame(unsigned int cycles)
{
  /* run Stopwatch until end of frame */
  int ticks = (cycles - scd.stopwatch) / TIMERS_SCYCLES_RATIO;
  scd.regs[0x0c>>1].w = (scd.regs[0x0c>>1].w + ticks) & 0xfff;

  /* adjust Stopwatch counter for next frame (can be negative) */
  scd.stopwatch += (ticks * TIMERS_SCYCLES_RATIO) - cycles;

  /* adjust SUB-CPU, GPU and CDC cycle counters for next frame */
  s68k.cycles   -= cycles;
  gfx.cycles    -= cycles;
  cdc.cycles[0] -= cycles;
  cdc.cycles[1] -= cycles;

  /* reset CPU registers polling */
  m68k.poll.cycle = 0;
  s68k.poll.cycle = 0;
}

int scd_context_save(uint8 *state)
{
  uint16 tmp16;
  uint32 tmp32;
  int bufferptr = 0;

  /* internal harware */
  save_param(scd.regs, sizeof(scd.regs));
  save_param(&scd.cycles, sizeof(scd.cycles));
  save_param(&scd.stopwatch, sizeof(scd.stopwatch));
  save_param(&scd.timer, sizeof(scd.timer));
  save_param(&scd.pending, sizeof(scd.pending));
  save_param(&scd.dmna, sizeof(scd.dmna));

  /* GFX processor */
  bufferptr += gfx_context_save(&state[bufferptr]);

  /* CD Data controller */
  bufferptr += cdc_context_save(&state[bufferptr]);

  /* CD Drive processor */
  bufferptr += cdd_context_save(&state[bufferptr]);

  /* PCM chip */
  bufferptr += pcm_context_save(&state[bufferptr]);

  /* PRG-RAM */
  save_param(scd.prg_ram, sizeof(scd.prg_ram));

  /* Word-RAM */
  if (scd.regs[0x03>>1].byte.l & 0x04)
  {
    /* 1M mode */
    save_param(scd.word_ram, sizeof(scd.word_ram));
  }
  else
  {
    /* 2M mode */
    save_param(scd.word_ram_2M, sizeof(scd.word_ram_2M));
  }

  /* MAIN-CPU & SUB-CPU polling */
  save_param(&m68k.poll, sizeof(m68k.poll));
  save_param(&s68k.poll, sizeof(s68k.poll));

  /* H-INT default vector */
  tmp16 = *(uint16 *)(m68k.memory_map[scd.cartridge.boot].base + 0x72);
  save_param(&tmp16, 2);

  /* SUB-CPU registers */
  tmp32 = s68k_get_reg(M68K_REG_D0);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D1);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D2);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D3);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D4);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D5);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D6);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D7);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A0);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A1);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A2);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A3);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A4);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A5);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A6);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A7);  save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_PC);  save_param(&tmp32, 4);
  tmp16 = s68k_get_reg(M68K_REG_SR);  save_param(&tmp16, 2); 
  tmp32 = s68k_get_reg(M68K_REG_USP); save_param(&tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_ISP); save_param(&tmp32, 4);

  /* SUB-CPU internal state */
  save_param(&s68k.cycles, sizeof(s68k.cycles));
  save_param(&s68k.int_level, sizeof(s68k.int_level));
  save_param(&s68k.stopped, sizeof(s68k.stopped));

  /* bootable MD cartridge */
  if (scd.cartridge.boot)
  {
    bufferptr += md_cart_context_save(&state[bufferptr]);
  }

  return bufferptr;
}

int scd_context_load(uint8 *state, char *version)
{
  int i;
  uint16 tmp16;
  uint32 tmp32;
  int bufferptr = 0;

  /* internal harware */
  load_param(scd.regs, sizeof(scd.regs));
  load_param(&scd.cycles, sizeof(scd.cycles));
  load_param(&scd.stopwatch, sizeof(scd.stopwatch));
  load_param(&scd.timer, sizeof(scd.timer));
  load_param(&scd.pending, sizeof(scd.pending));
  load_param(&scd.dmna, sizeof(scd.dmna));

  /* GFX processor */
  bufferptr += gfx_context_load(&state[bufferptr]);

  /* CD Data controller */
  bufferptr += cdc_context_load(&state[bufferptr]);

  /* CD Drive processor */
  bufferptr += cdd_context_load(&state[bufferptr], version);

  /* PCM chip */
  bufferptr += pcm_context_load(&state[bufferptr]);

  /* PRG-RAM */
  load_param(scd.prg_ram, sizeof(scd.prg_ram));

  /* PRG-RAM 128K bank mapped on MAIN-CPU side */
  m68k.memory_map[scd.cartridge.boot + 0x02].base = scd.prg_ram + ((scd.regs[0x03>>1].byte.l & 0xc0) << 11);
  m68k.memory_map[scd.cartridge.boot + 0x03].base = m68k.memory_map[scd.cartridge.boot + 0x02].base + 0x10000;

  /* PRG-RAM can only be accessed from MAIN 68K & Z80 if SUB-CPU is halted (Dungeon Explorer USA version) */
  if ((scd.regs[0x00].byte.l & 0x03) != 0x01)
  {
    m68k.memory_map[scd.cartridge.boot + 0x02].read8   = m68k.memory_map[scd.cartridge.boot + 0x03].read8   = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x02].read16  = m68k.memory_map[scd.cartridge.boot + 0x03].read16  = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x02].write8  = m68k.memory_map[scd.cartridge.boot + 0x03].write8  = NULL;
    m68k.memory_map[scd.cartridge.boot + 0x02].write16 = m68k.memory_map[scd.cartridge.boot + 0x03].write16 = NULL;
    zbank_memory_map[scd.cartridge.boot + 0x02].read   = zbank_memory_map[scd.cartridge.boot + 0x03].read   = NULL;
    zbank_memory_map[scd.cartridge.boot + 0x02].write  = zbank_memory_map[scd.cartridge.boot + 0x03].write  = NULL;
  }
  else
  {
    m68k.memory_map[scd.cartridge.boot + 0x02].read8   = m68k.memory_map[scd.cartridge.boot + 0x03].read8   = m68k_read_bus_8;
    m68k.memory_map[scd.cartridge.boot + 0x02].read16  = m68k.memory_map[scd.cartridge.boot + 0x03].read16  = m68k_read_bus_16;
    m68k.memory_map[scd.cartridge.boot + 0x02].write8  = m68k.memory_map[scd.cartridge.boot + 0x03].write8  = m68k_unused_8_w;
    m68k.memory_map[scd.cartridge.boot + 0x02].write16 = m68k.memory_map[scd.cartridge.boot + 0x03].write16 = m68k_unused_16_w;
    zbank_memory_map[scd.cartridge.boot + 0x02].read   = zbank_memory_map[scd.cartridge.boot + 0x03].read   = zbank_unused_r;
    zbank_memory_map[scd.cartridge.boot + 0x02].write  = zbank_memory_map[scd.cartridge.boot + 0x03].write  = zbank_unused_w;
  }

  /* Word-RAM */
  if (scd.regs[0x03>>1].byte.l & 0x04)
  {
    /* 1M Mode */
    load_param(scd.word_ram, sizeof(scd.word_ram));

    if (scd.regs[0x03>>1].byte.l & 0x01)
    {
      /* Word-RAM 1 assigned to MAIN-CPU */
      for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
      {
        /* Word-RAM 1 data mapped at $200000-$21FFFF */
        m68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
        m68k.memory_map[i].read8   = NULL;
        m68k.memory_map[i].read16  = NULL;
        m68k.memory_map[i].write8  = NULL;
        m68k.memory_map[i].write16 = NULL;
        zbank_memory_map[i].read   = NULL;
        zbank_memory_map[i].write  = NULL;
      }

      for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
      {
        /* VRAM cell image mapped at $220000-$23FFFF */
        m68k.memory_map[i].read8   = cell_ram_1_read8;
        m68k.memory_map[i].read16  = cell_ram_1_read16;
        m68k.memory_map[i].write8  = cell_ram_1_write8;
        m68k.memory_map[i].write16 = cell_ram_1_write16;
        zbank_memory_map[i].read   = cell_ram_1_read8;
        zbank_memory_map[i].write  = cell_ram_1_write8;
      }

      /* Word-RAM 0 assigned to SUB-CPU */
      for (i=0x08; i<0x0c; i++)
      {
        /* DOT image mapped at $080000-$0BFFFF */
        s68k.memory_map[i].read8   = dot_ram_0_read8;
        s68k.memory_map[i].read16  = dot_ram_0_read16;
        s68k.memory_map[i].write8  = dot_ram_0_write8;
        s68k.memory_map[i].write16 = dot_ram_0_write16;
      }

      for (i=0x0c; i<0x0e; i++)
      {
        /* Word-RAM 0 data mapped at $0C0000-$0DFFFF */
        s68k.memory_map[i].base    = scd.word_ram[0] + ((i & 0x01) << 16);
        s68k.memory_map[i].read8   = NULL;
        s68k.memory_map[i].read16  = NULL;
        s68k.memory_map[i].write8  = NULL;
        s68k.memory_map[i].write16 = NULL;
      }
    }
    else
    {
      /* Word-RAM 0 assigned to MAIN-CPU */
      for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
      {
        /* Word-RAM 0 data mapped at $200000-$21FFFF */
        m68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
        m68k.memory_map[i].read8   = NULL;
        m68k.memory_map[i].read16  = NULL;
        m68k.memory_map[i].write8  = NULL;
        m68k.memory_map[i].write16 = NULL;
        zbank_memory_map[i].read   = NULL;
        zbank_memory_map[i].write  = NULL;
      }

      for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
      {
        /* VRAM cell image mapped at $220000-$23FFFF */
        m68k.memory_map[i].read8   = cell_ram_0_read8;
        m68k.memory_map[i].read16  = cell_ram_0_read16;
        m68k.memory_map[i].write8  = cell_ram_0_write8;
        m68k.memory_map[i].write16 = cell_ram_0_write16;
        zbank_memory_map[i].read   = cell_ram_0_read8;
        zbank_memory_map[i].write  = cell_ram_0_write8;
      }

      /* Word-RAM 1 assigned to SUB-CPU */
      for (i=0x08; i<0x0c; i++)
      {
        /* DOT image mapped at $080000-$0BFFFF */
        s68k.memory_map[i].read8   = dot_ram_1_read8;
        s68k.memory_map[i].read16  = dot_ram_1_read16;
        s68k.memory_map[i].write8  = dot_ram_1_write8;
        s68k.memory_map[i].write16 = dot_ram_1_write16;
      }

      for (i=0x0c; i<0x0e; i++)
      {
        /* Word-RAM 1 data mapped at $0C0000-$0DFFFF */
        s68k.memory_map[i].base    = scd.word_ram[1] + ((i & 0x01) << 16);
        s68k.memory_map[i].read8   = NULL;
        s68k.memory_map[i].read16  = NULL;
        s68k.memory_map[i].write8  = NULL;
        s68k.memory_map[i].write16 = NULL;
      }
    }
  }
  else
  {
    /* 2M mode */
    load_param(scd.word_ram_2M, sizeof(scd.word_ram_2M));

    /* check RET bit */
    if (scd.regs[0x03>>1].byte.l & 0x01)
    {
      /* MAIN-CPU: $200000-$23FFFF is mapped to 256K Word-RAM */
      for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
      {
        m68k.memory_map[i].base    = scd.word_ram_2M + ((i & 0x03) << 16);
        m68k.memory_map[i].read8   = NULL;
        m68k.memory_map[i].read16  = NULL;
        m68k.memory_map[i].write8  = NULL;
        m68k.memory_map[i].write16 = NULL;
        zbank_memory_map[i].read   = NULL;
        zbank_memory_map[i].write  = NULL;
      }

      /* SUB-CPU: access to Word-RAM at $080000-$0BFFFF is locked (/DTACK not asserted)  */
      for (i=0x08; i<0x0c; i++)
      {
        s68k.memory_map[i].read8   = s68k_lockup_r_8;
        s68k.memory_map[i].read16  = s68k_lockup_r_16;
        s68k.memory_map[i].write8  = s68k_lockup_w_8;
        s68k.memory_map[i].write16 = s68k_lockup_w_16;
      }
    }
    else
    {
      /* MAIN-CPU: $200000-$23FFFF is unmapped */
      for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
      {
        m68k.memory_map[i].base    = scd.word_ram_2M + ((i & 0x03) << 16);
        m68k.memory_map[i].read8   = m68k_read_bus_8;
        m68k.memory_map[i].read16  = m68k_read_bus_16;
        m68k.memory_map[i].write8  = m68k_unused_8_w;
        m68k.memory_map[i].write16 = m68k_unused_16_w;
        zbank_memory_map[i].read   = zbank_unused_r;
        zbank_memory_map[i].write  = zbank_unused_w;
      }

      /* SUB-CPU: access to Word-RAM at $080000-$0BFFFF is unlocked (/DTACK asserted) */
      for (i=0x08; i<0x0c; i++)
      {
        s68k.memory_map[i].read8   = NULL;
        s68k.memory_map[i].read16  = NULL;
        s68k.memory_map[i].write8  = NULL;
        s68k.memory_map[i].write16 = NULL;
      }
    }

    /* SUB-CPU: $0C0000-$0DFFFF is unmapped */
    for (i=0x0c; i<0x0e; i++)
    {
      s68k.memory_map[i].read8   = s68k_read_bus_8;
      s68k.memory_map[i].read16  = s68k_read_bus_16;
      s68k.memory_map[i].write8  = s68k_unused_8_w;
      s68k.memory_map[i].write16 = s68k_unused_16_w;
    }
  }

  /* MAIN-CPU & SUB-CPU polling */
  load_param(&m68k.poll, sizeof(m68k.poll));
  load_param(&s68k.poll, sizeof(s68k.poll));

  /* H-INT default vector */
  load_param(&tmp16, 2);
  *(uint16 *)(m68k.memory_map[scd.cartridge.boot].base + 0x72) = tmp16;

  /* SUB-CPU registers */
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D0, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D1, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D2, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D3, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D4, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D5, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D6, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_D7, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A0, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A1, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A2, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A3, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A4, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A5, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A6, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_A7, tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_PC, tmp32);  
  load_param(&tmp16, 2); s68k_set_reg(M68K_REG_SR, tmp16);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_USP,tmp32);
  load_param(&tmp32, 4); s68k_set_reg(M68K_REG_ISP,tmp32);

  /* SUB-CPU internal state */
  load_param(&s68k.cycles, sizeof(s68k.cycles));
  load_param(&s68k.int_level, sizeof(s68k.int_level));
  load_param(&s68k.stopped, sizeof(s68k.stopped));

  /* bootable MD cartridge hardware */
  if (scd.cartridge.boot)
  {
    bufferptr += md_cart_context_load(&state[bufferptr]);
  }

  return bufferptr;
}

int scd_68k_irq_ack(int level)
{
#ifdef LOG_SCD
  error("INT ack level %d  (%X)\n", level, s68k.pc);
#endif

  /* clear pending interrupt flag */
  scd.pending &= ~(1 << level);

  /* level 2 interrupt acknowledge */
  if (level == 2)
  {
    /* clear IFL2 flag */
    scd.regs[0x00].byte.h &= ~0x01;
  }

  /* update IRQ level */
  s68k_update_irq((scd.pending & scd.regs[0x32>>1].byte.l) >> 1);

  return M68K_INT_ACK_AUTOVECTOR;
}
