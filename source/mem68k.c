/***************************************************************************************
 *  Genesis Plus
 *  68k bus handlers
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Copyright (C) 2007-2011  Eke-Eke (Genesis Plus GX)
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
#include "m68kcpu.h"


/*--------------------------------------------------------------------------*/
/* Unused area (return open bus data, i.e prefetched instruction word)      */
/*--------------------------------------------------------------------------*/
unsigned int m68k_read_bus_8(unsigned int address)
{
#ifdef LOGERROR
  error("Unused read8 %08X (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
#endif
  return m68k_read_pcrelative_8(REG_PC | (address & 1));
}

unsigned int m68k_read_bus_16(unsigned int address)
{
#ifdef LOGERROR
  error("Unused read16 %08X (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
#endif
  return m68k_read_pcrelative_16(REG_PC);
}


void m68k_unused_8_w (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Unused write8 %08X = %02X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
#endif
}

void m68k_unused_16_w (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Unused write16 %08X = %04X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
#endif
}


/*--------------------------------------------------------------------------*/
/* Illegal area (cause system to lock-up since !DTACK is not returned)      */
/*--------------------------------------------------------------------------*/
void m68k_lockup_w_8 (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error ("Lockup %08X = %02X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
}

void m68k_lockup_w_16 (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error ("Lockup %08X = %04X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
}

unsigned int m68k_lockup_r_8 (unsigned int address)
{ 
#ifdef LOGERROR
  error ("Lockup %08X.b (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
  return m68k_read_pcrelative_8(REG_PC | (address & 1));
}

unsigned int m68k_lockup_r_16 (unsigned int address)
{
#ifdef LOGERROR
  error ("Lockup %08X.w (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
  return m68k_read_pcrelative_16(REG_PC);
}


/*--------------------------------------------------------------------------*/
/* Z80 bus (accessed through I/O chip)                                      */
/*--------------------------------------------------------------------------*/
unsigned int z80_read_byte(unsigned int address)
{
  switch ((address >> 13) & 3)
  {
    case 2:   /* YM2612 */
    {
      return fm_read(mcycles_68k, address & 3);
    }

    case 3:   /* Misc  */
    {
      if ((address & 0xFF00) == 0x7F00)
      {
        /* VDP (through 68k bus) */
        return m68k_lockup_r_8(address);
      }
      return (m68k_read_bus_8(address) | 0xFF);
    }

    default: /* ZRAM */
    {
      return zram[address & 0x1FFF];
    }
  }
}

unsigned int z80_read_word(unsigned int address)
{
  unsigned int data = z80_read_byte(address);
  return (data | (data << 8));
}

void z80_write_byte(unsigned int address, unsigned int data)
{
  switch ((address >> 13) & 3)
  {
    case 2: /* YM2612 */
    {
      fm_write(mcycles_68k, address & 3, data);
      return;
    }

    case 3:
    {
      switch ((address >> 8) & 0x7F)
      {
        case 0x60:  /* Bank register */
        {
          gen_zbank_w(data & 1);
          return;
        }

        case 0x7F:  /* VDP */
        {
          m68k_lockup_w_8(address, data);
          return;
        }
      
        default:
        {
          m68k_unused_8_w(address, data);
          return;
        }
      }
    }
      
    default: /* ZRAM */
    {
      zram[address & 0x1FFF] = data;
      mcycles_68k += 8; /* ZRAM access latency (fixes Pacman 2: New Adventures) */
      return;
    }
  }
}

void z80_write_word(unsigned int address, unsigned int data)
{
  z80_write_byte(address, data >> 8);
}


/*--------------------------------------------------------------------------*/
/* I/O Control                                                              */
/*--------------------------------------------------------------------------*/
unsigned int ctrl_io_read_byte(unsigned int address)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        return io_68k_read((address >> 1) & 0x0F);
      }
      return m68k_read_bus_8(address);
    }

    case 0x11:  /* BUSACK */
    {
      if (!(address & 1))
      {
        /* Unused bits return prefetched bus data (Time Killers) */
        unsigned int data = m68k_read_pcrelative_8(REG_PC) & 0xFE;
        if (zstate == 3)
        {
          return data;
        }
        return (data | 0x01);
      }
      return m68k_read_bus_8(address);
    }

    case 0x30:  /* TIME */
    {
      if (cart.hw.time_r)
      {
        unsigned int data = cart.hw.time_r(address);
        if (address & 1)
        {
          return (data & 0xFF);
        }
        return (data >> 8);
      }
      return m68k_read_bus_8(address);
    }

    case 0x41:  /* BOOT ROM */
    {
      if ((config.bios & 1) && (address & 1))
      {
        unsigned int data = m68k_read_pcrelative_8(REG_PC) & 0xFE;
        return (gen_bankswitch_r() | data);
      }
      return m68k_read_bus_8(address);
    }

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* RESET */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP REGISTERS */
    {
      return m68k_read_bus_8(address);
    }

    default:  /* Invalid address */
    {
      return m68k_lockup_r_8(address);
    }
  }
}

unsigned int ctrl_io_read_word(unsigned int address)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        unsigned int data = io_68k_read((address >> 1) & 0x0F);
        return (data << 8 | data);
      }
      return m68k_read_bus_16(address); 
   }

    case 0x11:  /* BUSACK */
    {
      /* Unused bits return prefetched bus data (Time Killers) */
      unsigned int data = m68k_read_pcrelative_16(REG_PC) & 0xFEFF;
      if (zstate == 3)
      {
        return data;
      }
      return (data | 0x0100);
    }

    case 0x30:  /* TIME */
    {
      if (cart.hw.time_r)
      {
        return cart.hw.time_r(address);
      }
      return m68k_read_bus_16(address); 
    }
      
    case 0x50:  /* SVP */
    {
      if ((address & 0xFD) == 0)
      {
        return svp->ssp1601.gr[SSP_XST].byte.h;
      }

      if ((address & 0xFF) == 4)
      {
        unsigned int data = svp->ssp1601.gr[SSP_PM0].byte.h;
        svp->ssp1601.gr[SSP_PM0].byte.h &= ~1;
        return data;
      }

      return m68k_read_bus_16(address);
    }

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* RESET */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x41:  /* BOOT ROM */
    case 0x44:  /* RADICA */
    {
      return m68k_read_bus_16(address);
    }

    default:  /* Invalid address */
    {
      return m68k_lockup_r_16(address);
    }
  }
}

void ctrl_io_write_byte(unsigned int address, unsigned int data)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if ((address & 0xE1) == 0x01)
      {
        /* get /LWR only */
        io_68k_write((address >> 1) & 0x0F, data);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x11:  /* BUSREQ */
    {
      if (!(address & 1))
      {
        gen_zbusreq_w(data & 1, mcycles_68k);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x12:  /* RESET */
    {
      if (!(address & 1))
      {
        gen_zreset_w(data & 1, mcycles_68k);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x41:  /* BOOT ROM */
    {
      if ((config.bios & 1) && (address & 1))
      {
        gen_bankswitch_w(data & 1);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x10:  /* MEMORY MODE */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP REGISTERS */
    {
      m68k_unused_8_w(address, data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_8(address, data);
      return;
    }
  }
}

void ctrl_io_write_word(unsigned int address, unsigned int data)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        io_68k_write((address >> 1) & 0x0F, data & 0xFF);
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x11:  /* BUSREQ */
    {
      gen_zbusreq_w((data >> 8) & 1, mcycles_68k);
      return;
    }

    case 0x12:  /* RESET */
    {
      gen_zreset_w((data >> 8) & 1, mcycles_68k);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x40:  /* TMSS */
    {
      if (config.bios & 1)
      {
        gen_tmss_w(address & 3, data);
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x50:  /* SVP REGISTERS */
    {
      if (!(address & 0xFD))
      {
        svp->ssp1601.gr[SSP_XST].byte.h = data;
        svp->ssp1601.gr[SSP_PM0].byte.h |= 2;
        svp->ssp1601.emu_status &= ~SSP_WAIT_PM0;
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x10:  /* MEMORY MODE */
    case 0x20:  /* MEGA-CD */
    case 0x41:  /* BOOT ROM */
    case 0x44:  /* RADICA */
    {
      m68k_unused_16_w (address, data);
      return;
    }
            
    default:  /* Invalid address */
    {
      m68k_lockup_w_16 (address, data);
      return;
    }
  }
}


/*--------------------------------------------------------------------------*/
/* VDP                                                                      */
/*--------------------------------------------------------------------------*/
unsigned int vdp_read_byte(unsigned int address)
{
  switch (address & 0xFD)
  {
    case 0x00:  /* DATA */
    {
      return (vdp_68k_data_r() >> 8);
    }

    case 0x01:  /* DATA */
    {
      return (vdp_68k_data_r() & 0xFF);
    }

    case 0x04:  /* CTRL */
    {
      /* Unused bits return prefetched bus data */
      return (((vdp_68k_ctrl_r(mcycles_68k) >> 8) & 3) | (m68k_read_pcrelative_8(REG_PC) & 0xFC));
    }

    case 0x05:  /* CTRL */
    {
      return (vdp_68k_ctrl_r(mcycles_68k) & 0xFF);
    }

    case 0x08:  /* HVC */
    case 0x0C:
    {
      return (vdp_hvc_r(mcycles_68k) >> 8);
    }

    case 0x09:  /* HVC */
    case 0x0D:
    {
      return (vdp_hvc_r(mcycles_68k) & 0xFF);
    }

    case 0x18:  /* Unused */
    case 0x19:
    case 0x1C:
    case 0x1D:
    {
      return m68k_read_bus_8(address);
    }

    default:    /* Invalid address */
    {
      return m68k_lockup_r_8(address);
    }
  }
}

unsigned int vdp_read_word(unsigned int address)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* DATA */
    {
      return vdp_68k_data_r();
    }

    case 0x04:  /* CTRL */
    {
      /* Unused bits return prefetched bus data */
      return ((vdp_68k_ctrl_r(mcycles_68k) & 0x3FF) | (m68k_read_pcrelative_16(REG_PC) & 0xFC00));
    }

    case 0x08:  /* HVC */
    case 0x0C:
    {
      return vdp_hvc_r(mcycles_68k);
    }

    case 0x18:  /* Unused */
    case 0x1C:
    {
      return m68k_read_bus_16(address);
    }

    default:    /* Invalid address */
    {
      return m68k_lockup_r_16(address);
    }
  }
}

void vdp_write_byte(unsigned int address, unsigned int data)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* Data port */
    {
      vdp_68k_data_w(data << 8 | data);
      return;
    }

    case 0x04:  /* Control port */
    {
      vdp_68k_ctrl_w(data << 8 | data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      if (address & 1)
      {
        psg_write(mcycles_68k, data);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x18: /* Unused */
    {
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x1C:  /* TEST register */
    {
      vdp_test_w(data << 8 | data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_8(address, data);
      return;
    }
  }
}

void vdp_write_word(unsigned int address, unsigned int data)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* DATA */
    {
      vdp_68k_data_w(data);
      return;
    }

    case 0x04:  /* CTRL */
    {
      vdp_68k_ctrl_w(data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      psg_write(mcycles_68k, data & 0xFF);
      return;
    }

    case 0x18:  /* Unused */
    {
      m68k_unused_16_w(address, data);
      return;
    }
    
    case 0x1C:  /* Test register */
    {
      vdp_test_w(data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_16 (address, data);
      return;
    }
  }
}


/******* PICO ************************************************/

unsigned int pico_read_byte(unsigned int address)
{
  /* PICO */
  switch (address & 0xFF)
  {
    case 0x01:  /* VERSION register */
    {
      return 0x40;
    }

    case 0x03:  /* IO register */
    {
      unsigned int retval = 0xFF;
      if (input.pad[0] & INPUT_B)     retval &= ~0x10;
      if (input.pad[0] & INPUT_A)     retval &= ~0x80;
      if (input.pad[0] & INPUT_UP)    retval &= ~0x01;
      if (input.pad[0] & INPUT_DOWN)  retval &= ~0x02;
      if (input.pad[0] & INPUT_LEFT)  retval &= ~0x04;
      if (input.pad[0] & INPUT_RIGHT) retval &= ~0x08;
      retval &= ~0x20;
      retval &= ~0x40;
      return retval;
    }

    case 0x05:  /* MSB PEN X coordinate */
    {
      return (input.analog[0][0] >> 8);
    }

    case 0x07:  /* LSB PEN X coordinate */
    {
      return (input.analog[0][0] & 0xFF);
    }

    case 0x09:  /* MSB PEN Y coordinate */
    {
      return (input.analog[0][1] >> 8);
    }

    case 0x0B:  /* LSB PEN Y coordinate */
    {
      return (input.analog[0][1] & 0xFF);
    }

    case 0x0D:  /* PAGE register (TODO) */
    {
      return pico_regs[pico_current];
    }

    case 0x10:  /* PCM registers (TODO) */
    {
      return 0x80;
    }

    default:
    {
      return m68k_read_bus_8(address);
    }
  }
}

unsigned int pico_read_word(unsigned int address)
{
  return (pico_read_byte(address | 1) | (m68k_read_bus_8(address) << 8));
}
