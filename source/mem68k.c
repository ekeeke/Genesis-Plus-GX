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
uint32 m68k_read_bus_8(uint32 address)
{
#ifdef LOGERROR
  error("Unused read8 %08X (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  return m68k_read_pcrelative_8(REG_PC | (address & 1));
}

uint32 m68k_read_bus_16(uint32 address)
{
#ifdef LOGERROR
  error("Unused read16 %08X (%08X)\n", address, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  return m68k_read_pcrelative_16(REG_PC);
}


void m68k_unused_8_w (uint32 address, uint32 data)
{
#ifdef LOGERROR
  error("Unused write8 %08X = %02X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
}

void m68k_unused_16_w (uint32 address, uint32 data)
{
#ifdef LOGERROR
  error("Unused write16 %08X = %04X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
}


/*--------------------------------------------------------------------------*/
/* Illegal area (cause system to lock-up since !DTACK is not returned)      */
/*--------------------------------------------------------------------------*/
void m68k_lockup_w_8 (uint32 address, uint32 data)
{
#ifdef LOGERROR
  error ("Lockup %08X = %02X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
}

void m68k_lockup_w_16 (uint32 address, uint32 data)
{
#ifdef LOGERROR
  error ("Lockup %08X = %04X (%08X)\n", address, data, m68k_get_reg (NULL, M68K_REG_PC));
#endif
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
  }
}

uint32 m68k_lockup_r_8 (uint32 address)
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

uint32 m68k_lockup_r_16 (uint32 address)
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
uint32 eeprom_read_byte(uint32 address)
{
  if (address != eeprom.type.sda_out_adr)
  {
    return READ_BYTE(cart.rom, address);
  }
  return eeprom_read(address, 0);
}

uint32 eeprom_read_word(uint32 address)
{
  if (address != (eeprom.type.sda_out_adr & 0xfffffe))
  {
    return *(uint16 *)(cart.rom + address);
  }
  return eeprom_read(address, 1);
}

void eeprom_write_byte(uint32 address, uint32 data)
{
  if ((address != eeprom.type.sda_in_adr) && (address != eeprom.type.scl_adr))
  {
    m68k_unused_8_w(address, data);
    return;
  }
  eeprom_write(address, data, 0);
}

void eeprom_write_word(uint32 address, uint32 data)
{
  if ((address != (eeprom.type.sda_in_adr & 0xfffffe)) && (address != (eeprom.type.scl_adr & 0xfffffe)))
  {
    m68k_unused_16_w (address, data);
    return;
  }
  eeprom_write(address, data, 1);
}


/*--------------------------------------------------------------------------*/
/* Z80 bus (accessed through I/O chip)                                      */
/*--------------------------------------------------------------------------*/
uint32 z80_read_byte(uint32 address)
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

uint32 z80_read_word(uint32 address)
{
  switch ((address >> 13) & 3)
  {
    case 2:   /* YM2612 */
    {
      unsigned int data = fm_read(mcycles_68k, address & 3);
      return (data << 8 | data);
    }

    case 3:   /* Misc */
    {
      if ((address & 0xff00) == 0x7f00)
      {
        /* VDP (through 68k bus) */
        return m68k_lockup_r_16(address);
      }
      return (m68k_read_bus_16(address) | 0xffff);
    }

    default:  /* ZRAM */
    {
      unsigned int data = zram[address & 0x1fff];
      return (data << 8 | data);
    }
  }
}

void z80_write_byte(uint32 address, uint32 data)
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
          gen_zbank_w(data & 1);
          return;

        case 0x7f:  /* VDP */
          m68k_lockup_w_8(address, data);
          return;
      
        default:
          m68k_unused_8_w(address, data);
          return;
      }
    }
      
    default: /* ZRAM */
    {
      zram[address & 0x1fff] = data;
      mcycles_68k += 8; /* Z80 bus latency (fixes Pacman 2: New Adventures) */
      return;
    }
  }
}

void z80_write_word(uint32 address, uint32 data)
{
  switch ((address >> 13) & 3)
  {
    case 2: /* YM2612 */
    {
      fm_write(mcycles_68k, address & 3, data >> 8);
      return;
    }

    case 3:
    {
      switch ((address >> 8) & 0x7f)
      {
        case 0x60:  /* Bank register */
        {
          gen_zbank_w((data >> 8) & 1);
          return;
        }

        case 0x7f:  /* VDP */
        {
          m68k_lockup_w_16(address, data);
          return;
        }

        default:
        {
          m68k_unused_16_w(address, data);
          return;
        }
      }
    }

    default: /* ZRAM */
    {
      zram[address & 0x1fff] = data >> 8;
      return;
    }
  }
}


/*--------------------------------------------------------------------------*/
/* I/O Control                                                              */
/*--------------------------------------------------------------------------*/
uint32 ctrl_io_read_byte(uint32 address)
{
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if (address & 0xe0)
      {
        return m68k_read_bus_8(address);
      }
      return io_read((address >> 1) & 0x0f);
    }

    case 0x11:  /* BUSACK */
    {
      if (address & 1)
      {
        return m68k_read_bus_8(address);
      }
      unsigned int data = m68k_read_pcrelative_8(REG_PC) & 0xfe;
      if (zstate ^ 3)
      {
        return (data | 0x01);
      }
      return data;
    }

    case 0x30:  /* TIME */
    {
      if (!cart.hw.time_r)
      {
        return m68k_read_bus_8(address);
      }
      unsigned int data = cart.hw.time_r(address);
      if (address & 1)
      {
        return (data & 0xff);
      }
      return (data >> 8);
    }

    case 0x41:  /* OS ROM */
    {
      if (!(address & 1))
      {
        return m68k_read_bus_8(address);
      }
      unsigned int data = m68k_read_pcrelative_8(REG_PC) & 0xfe;
      return (gen_bankswitch_r() | data);
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

uint32 ctrl_io_read_word(uint32 address)
{
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if (address & 0xe0)
      {
        return m68k_read_bus_16(address); 
      }
      unsigned int data = io_read((address >> 1) & 0x0f);
      return (data << 8 | data);
    }

    case 0x11:  /* BUSACK */
    {
      unsigned int data = m68k_read_pcrelative_16(REG_PC) & 0xfeff;
      if (zstate ^ 3)
      {
        return data | 0x0100;
      }
      return data;
    }

    case 0x30:  /* TIME */
    {
      if (!cart.hw.time_r)
      {
        return m68k_read_bus_16(address); 
      }
      return cart.hw.time_r(address);
    }
      
    case 0x50:  /* SVP */
    {
      switch (address & 0xfe)
      {
        case 0:
        case 2:
        {
          return svp->ssp1601.gr[SSP_XST].h;
        }

        case 4:
        {
          uint32 temp = svp->ssp1601.gr[SSP_PM0].h;
          svp->ssp1601.gr[SSP_PM0].h &= ~1;
          return temp;
        }

        default:
        {
          return m68k_read_bus_16(address);
        }
      }
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

void ctrl_io_write_byte(uint32 address, uint32 data)
{
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if ((address & 0xe1) != 0x01)
      {
        /* get /LWR only */
        m68k_unused_8_w(address, data);
        return;
      }
      io_write((address >> 1) & 0x0f, data);
      return;
    }

    case 0x11:  /* BUSREQ */
    {
      if (address & 1)
      {
        m68k_unused_8_w(address, data);
        return;
      }
      gen_zbusreq_w(data & 1, mcycles_68k);
      return;
    }

    case 0x12:  /* RESET */
    {
      if (address & 1)
      {
        m68k_unused_8_w(address, data);
        return;
      }
      gen_zreset_w(data & 1, mcycles_68k);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x41:  /* OS ROM */
    {
      if (!(address & 1))
      {
        m68k_unused_8_w(address, data);
        return;
      }
      gen_bankswitch_w(data & 1);
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

void ctrl_io_write_word(uint32 address, uint32 data)
{
  switch ((address >> 8) & 0xff)
  {
    case 0x00:  /* I/O chip */
    {
      if (address & 0xe0)
      {
        m68k_unused_16_w(address, data);
        return;
      }
      io_write((address >> 1) & 0x0f, data & 0xff);
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
      cart.hw.time_w(address & 0xfe, data >> 8);
      cart.hw.time_w(address, data & 0xff);
      return;
    }

    case 0x40:  /* TMSS */
    {
      gen_tmss_w(address & 3, data);
      break;
    }

    case 0x50:  /* SVP REGISTERS */
    {
      if (address & 0xfd)
      {
        m68k_unused_16_w(address, data);
        return;
      }
      svp->ssp1601.gr[SSP_XST].h = data;
      svp->ssp1601.gr[SSP_PM0].h |= 2;
      svp->ssp1601.emu_status &= ~SSP_WAIT_PM0;
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
uint32 vdp_read_byte(uint32 address)
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

uint32 vdp_read_word(uint32 address)
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

void vdp_write_byte(uint32 address, uint32 data)
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
      if (!(address & 1))
      {
        m68k_unused_8_w(address, data);
        return;
      }
      psg_write(mcycles_68k, data);
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

void vdp_write_word(uint32 address, uint32 data)
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

uint32 pico_read_byte(uint32 address)
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
      uint8 retval = 0xff;
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

uint32 pico_read_word(uint32 address)
{
  return (pico_read_byte(address | 1) | (m68k_read_bus_8(address) << 8));
}
