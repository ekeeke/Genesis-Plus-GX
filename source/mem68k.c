/***************************************************************************************
 *  Genesis Plus
 *  68k bus address decoding
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Eke-Eke (2007,2008,2009), additional code & fixes for the GCN/Wii port
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

#include "m68kcpu.h"
#include "shared.h"


/*--------------------------------------------------------------------------*/
/* Unused area (return open bus data, i.e prefetched instruction word)      */
/*--------------------------------------------------------------------------*/
unsigned int m68k_read_bus_8(unsigned int address)
{
#ifdef LOGERROR
  error("Unused read8 %08X (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  return m68k_read_pcrelative_8(REG_PC | (address & 1));
}

unsigned int m68k_read_bus_16(unsigned int address)
{
#ifdef LOGERROR
  error("Unused read16 %08X (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  return m68k_read_pcrelative_16(REG_PC);
}


void m68k_unused_8_w (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Unused write8 %08X = %02X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
}

void m68k_unused_16_w (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error("Unused write16 %08X = %04X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
}


/*--------------------------------------------------------------------------*/
/* Illegal area (cause system to lock-up since !DTACK is not returned)      */
/*--------------------------------------------------------------------------*/
void m68k_lockup_w_8 (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error ("Lockup %08X = %02X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
}

void m68k_lockup_w_16 (unsigned int address, unsigned int data)
{
#ifdef LOGERROR
  error ("Lockup %08X = %04X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
}

unsigned int m68k_lockup_r_8 (unsigned int address)
{ 
#ifdef LOGERROR
  error ("Lockup %08X.b (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
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
  error ("Lockup %08X.w (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
  return m68k_read_pcrelative_16(REG_PC);
}


/*--------------------------------------------------------------------------*/
/* cartridge EEPROM                                                         */
/*--------------------------------------------------------------------------*/
unsigned int eeprom_read_byte(unsigned int address)
{
  if (address == eeprom.type.sda_out_adr)
  {
    return eeprom_read(0);
  }
  return READ_BYTE(cart.rom, address);
}

unsigned int eeprom_read_word(unsigned int address)
{
  if (address == (eeprom.type.sda_out_adr & 0xfffffe))
  {
    return eeprom_read(1);
  }
  return *(uint16 *)(cart.rom + address);
}

void eeprom_write_byte(unsigned int address, unsigned int data)
{
  if ((address == eeprom.type.sda_in_adr) || (address == eeprom.type.scl_adr))
  {
    eeprom_write(address, data, 0);
    return;
  }
  m68k_unused_8_w(address, data);
}

void eeprom_write_word(unsigned int address, unsigned int data)
{
  if ((address == (eeprom.type.sda_in_adr & 0xfffffe)) || (address == (eeprom.type.scl_adr & 0xfffffe)))
  {
    eeprom_write(address, data, 1);
    return;
  }
  m68k_unused_16_w (address, data);
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
      if ((address & 0xff00) == 0x7f00)
      {
        /* VDP (through 68k bus) */
        return m68k_lockup_r_8(address);
      }
      return (m68k_read_bus_8(address) | 0xff);
    }

    default: /* ZRAM */
    {
      return zram[address & 0x1fff];
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
      switch ((address >> 8) & 0x7f)
      {
        case 0x60:  /* Bank register */
        {
          gen_zbank_w(data & 1);
          return;
        }

        case 0x7f:  /* VDP */
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
      zram[address & 0x1fff] = data;
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
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xe0))
      {
        return io_read((address >> 1) & 0x0f);
      }
      return m68k_read_bus_8(address);
    }

    case 0x11:  /* BUSACK */
    {
      if (!(address & 1))
      {
        unsigned int data = m68k_read_pcrelative_8(REG_PC) & 0xfe;
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
          return (data & 0xff);
        }
        return (data >> 8);
      }
      return m68k_read_bus_8(address);
    }

    case 0x41:  /* OS ROM */
    {
      if (address & 1)
      {
        unsigned int data = m68k_read_pcrelative_8(REG_PC) & 0xfe;
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
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xe0))
      {
        unsigned int data = io_read((address >> 1) & 0x0f);
        return (data << 8 | data);
      }
      return m68k_read_bus_16(address); 
   }

    case 0x11:  /* BUSACK */
    {
      unsigned int data = m68k_read_pcrelative_16(REG_PC) & 0xfeff;
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
      if ((address & 0xfd) == 0)
      {
        return svp->ssp1601.gr[SSP_XST].h;
      }

      if ((address & 0xff) == 4)
      {
        unsigned int data = svp->ssp1601.gr[SSP_PM0].h;
        svp->ssp1601.gr[SSP_PM0].h &= ~1;
        return data;
      }

      return m68k_read_bus_16(address);
    }

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* RESET */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x41:  /* OS ROM */
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
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if ((address & 0xe1) == 0x01)
      {
        /* get /LWR only */
        io_write((address >> 1) & 0x0f, data);
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

    case 0x41:  /* OS ROM */
    {
      if (address & 1)
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
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xe0))
      {
        io_write((address >> 1) & 0x0f, data & 0xff);
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
      if (config.tmss & 1)
      {
        gen_tmss_w(address & 3, data);
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x50:  /* SVP REGISTERS */
    {
      if (!(address & 0xfd))
      {
        svp->ssp1601.gr[SSP_XST].h = data;
        svp->ssp1601.gr[SSP_PM0].h |= 2;
        svp->ssp1601.emu_status &= ~SSP_WAIT_PM0;
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x10:  /* MEMORY MODE */
    case 0x20:  /* MEGA-CD */
    case 0x41:  /* OS ROM */
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
  switch (address & 0xfd)
  {
    case 0x00:  /* DATA */
    {
      return (vdp_data_r() >> 8);
    }

    case 0x01:  /* DATA */
    {
      return (vdp_data_r() & 0xff);
    }

    case 0x04:  /* CTRL */
    {
      return (((vdp_ctrl_r(mcycles_68k) >> 8) & 3) | (m68k_read_pcrelative_8(REG_PC) & 0xfc));
    }

    case 0x05:  /* CTRL */
    {
      return (vdp_ctrl_r(mcycles_68k) & 0xff);
    }

    case 0x08:  /* HVC */
    case 0x0c:
    {
      return (vdp_hvc_r(mcycles_68k) >> 8);
    }

    case 0x09:  /* HVC */
    case 0x0d:
    {
      return (vdp_hvc_r(mcycles_68k) & 0xff);
    }

    case 0x18:  /* Unused */
    case 0x19:
    case 0x1c:
    case 0x1d:
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
  switch (address & 0xfc)
  {
    case 0x00:  /* DATA */
    {
      return vdp_data_r();
    }

    case 0x04:  /* CTRL */
    {
      return ((vdp_ctrl_r(mcycles_68k) & 0x3FF) | (m68k_read_pcrelative_16(REG_PC) & 0xFC00));
    }

    case 0x08:  /* HVC */
    case 0x0c:
    {
      return vdp_hvc_r(mcycles_68k);
    }

    case 0x18:  /* Unused */
    case 0x1c:
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
  switch (address & 0xfc)
  {
    case 0x00:  /* Data port */
    {
      vdp_data_w(data << 8 | data);
      return;
    }

    case 0x04:  /* Control port */
    {
      vdp_ctrl_w(data << 8 | data);
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

    case 0x1c:  /* TEST register */
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
  switch (address & 0xfc)
  {
    case 0x00:  /* DATA */
    {
      vdp_data_w(data);
      return;
    }

    case 0x04:  /* CTRL */
    {
      vdp_ctrl_w(data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      psg_write(mcycles_68k, data & 0xff);
      return;
    }

    case 0x18:  /* Unused */
    {
      m68k_unused_16_w(address, data);
      return;
    }
    
    case 0x1c:  /* Test register */
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
  switch (address & 0xff)
  {
    case 0x01:  /* VERSION register */
    {
      return 0x40;
    }

    case 0x03:  /* IO register */
    {
      unsigned int retval = 0xff;
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
      return (input.analog[0][0] & 0xff);
    }

    case 0x09:  /* MSB PEN Y coordinate */
    {
      return (input.analog[0][1] >> 8);
    }

    case 0x0b:  /* LSB PEN Y coordinate */
    {
      return (input.analog[0][1] & 0xff);
    }

    case 0x0d:  /* PAGE register (TODO) */
    {
      return pico_page[pico_current];
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
