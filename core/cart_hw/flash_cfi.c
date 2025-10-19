/****************************************************************************
 *  Genesis Plus
 *  CFI-compliant flash memory (29xxx) support
 *
 *  Limitations:
 *   - 16-bit mode only
 *   - only support commonly used commands (read/program/erase/autoselect/query)
 *   - write-protection configuration based on cartridge mapping (only backup RAM is rewritable)
 *   - instantaneous program/erase operations
 *   - only return manufacturer & device codes in autoselect mode
 *   - do not return extended table & security code area in CFI query mode
 *
 *  Copyright (C) 2025 Eke-Eke (Genesis Plus GX)
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
#include "flash_cfi.h"

#define CFI_QUERY_TABLE_LEN 45

typedef enum
{
  FLASH_READ = 0,
  FLASH_UNLOCKED,
  FLASH_AUTOSELECT,
  FLASH_PROGRAM,
  FLASH_ERASE_INIT,
  FLASH_ERASE_UNLOCKED,
  FLASH_CFI_QUERY,
  FLASH_OTP_ENABLED,
  FLASH_OTP_EXIT_UNLOCKED
} T_CFI_MODE;

typedef struct
{
  uint32 addr;                          /* latched address register */
  uint32 data;                          /* latched data register */
  T_CFI_MODE mode;                      /* current operating mode */
  const uint8 *cfi_data;                /* CFI query data array */
  const uint16 *id;                     /* Manufacturer & Device id codes */
  uint16 otp_area[FLASH_OTP_AREA_SIZE]; /* Secured Silicon Sector / Extended Block area */
  uint8 readmask;                       /* Autoselect mode read address mask */
} T_FLASH_CFI;

static T_FLASH_CFI flash;

/* CFI query data arrays */
static const uint8 cfi_query[MAX_FLASH_CFI_SUPPORTED_TYPES][CFI_QUERY_TABLE_LEN] =
{
  /* M29W320EB */
  {
    0x51,0x52,0x59,0x02,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
    0x27,0x36,0xB5,0xC5,0x04,0x00,0x0A,0x00,0x04,0x00,0x03,0x00,
    0x16,0x02,0x00,0x00,0x00,0x02,0x07,0x00,0x20,0x00,0x3e,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  },

  /* S29GL064N (model 04) */
  {
    0x51,0x52,0x59,0x02,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
    0x27,0x36,0x00,0x00,0x07,0x07,0x0A,0x00,0x03,0x05,0x04,0x00,
    0x17,0x02,0x00,0x05,0x00,0x02,0x07,0x00,0x20,0x00,0x7e,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  }
};

/* Manufacturer & Device id codes */
static const uint16 flash_id[MAX_FLASH_CFI_SUPPORTED_TYPES][16] =
{
  /* M29W320EB */
  {0x0020,0x2257,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000},

  /* S29GL064N (model 04) */
  {0x0001,0x227E,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x2210,0x2200}
};

/* Autoselect mode address read masks (max value = 0x0F) */
static const uint8 flash_readmask[MAX_FLASH_CFI_SUPPORTED_TYPES] =
{
  0x03, /* M29W320EB */
  0x0F  /* S29GL064N (model 04) */
};

void flash_cfi_init(T_FLASH_CFI_TYPE type, const uint16 *otp_data)
{
  memset(&flash, 0x00, sizeof(flash));
  flash.cfi_data  = cfi_query[type];
  flash.id        = flash_id[type];
  flash.readmask  = flash_readmask[type];
  if (otp_data)
  {
    memcpy(flash.otp_area, otp_data, sizeof(flash.otp_area));
  }
}

void flash_cfi_write(unsigned int address, unsigned int data)
{
  /* 64KB bank index */
  unsigned int index = (address >> 15) & 0xff;

  if (flash.mode == FLASH_PROGRAM)
  {
    /* assume any writable 64KB bank except first one is not write-protected */
    if (index > 0)
    {
      /* assume any programmed bank should be mapped to SRAM (max 64KB) */
      if (m68k.memory_map[index].base != sram.sram)
      {
        /* if not, assume save data sector is switched (see https://gitlab.com/doragasu/sgdk-flash-save/-/blob/master/src/saveman.c) */
        int i = 0x7f;
        do
        {
          if (m68k.memory_map[i].base == sram.sram)
          {
             /* 64KB bank currently mapped to SRAM is mapped back to cartridge ROM */
             m68k.memory_map[i].base = cart.rom + ((i<<16) & cart.mask);
             
             /* copy current SRAM data to cartridge ROM (should eventually be copied back to SRAM then normally erased) */
             memcpy(m68k.memory_map[i].base, sram.sram, 0x10000);
             break;
          }
        } while (--i > 0);

        /* copy programmed 64KB bank content to SRAM (should normally be filled with 0xFF) */
        memcpy(sram.sram, m68k.memory_map[index].base, 0x10000);

        /* programmed 64KB bank is now mapped to SRAM */
        m68k.memory_map[index].base = sram.sram;
      }

      /* write data to SRAM (flash programming can only clear bits set to 1) */
      WRITE_WORD(sram.sram, (address << 1) & 0xfffe, data & READ_WORD(sram.sram, (address << 1) & 0xfffe));
    }

    /* reset to default read mode */
    flash.mode = FLASH_READ;
  }
  else
  {
    /* only A0-A11 and D0-D7 are decoded */
    address &= 0xfff;
    data &= 0xff;

    /* detect RESET command (always valid except in OTP area read mode) */
    if ((data == 0xf0) && (flash.mode < FLASH_OTP_ENABLED))
    {
      /* reset to default read mode */
      flash.mode = FLASH_READ;
    }
    else
    {
      switch (flash.mode)
      {
        case FLASH_UNLOCKED:
        {
          /* detect supported commands */
          if (address == 0x555)
          {
            if (data == 0x80)
            {
              /* ERASE command initialization */
              flash.mode = FLASH_ERASE_INIT;
            }
            else if (data == 0x88)
            {
              /* ENTER SECURED SILICON SECTOR REGION / EXTENDED BLOCK command */
              flash.mode = FLASH_OTP_ENABLED;
            }
            else if (data == 0x90)
            {
              /* AUTOSELECT command */
              flash.mode = FLASH_AUTOSELECT;
            }
            else if (data == 0xA0)
            {
              /* PROGRAM command */
              flash.mode = FLASH_PROGRAM;
            }
          }
          break;
        }

        case FLASH_OTP_ENABLED:
        {
          /* detect UNLOCK sequence */
          if ((flash.addr == 0x555) && (flash.data == 0xAA) && (address == 0x2AA) && (data == 0x55))
          {
            flash.mode = FLASH_OTP_EXIT_UNLOCKED;
          }
          break;
        }

        case FLASH_OTP_EXIT_UNLOCKED:
        {
          /* detect EXIT SECURED SILICON SECTOR REGION / EXTENDED BLOCK command */
          if ((flash.addr == 0x555) && (flash.data == 0x90) && (data == 0x00))
          {
            /* reset to default read mode */
            flash.mode = FLASH_READ;
          }
          break;
        }

        case FLASH_ERASE_INIT:
        {
          /* detect UNLOCK sequence */
          if ((flash.addr == 0x555) && (flash.data == 0xAA) && (address == 0x2AA) && (data == 0x55))
          {
            flash.mode = FLASH_ERASE_UNLOCKED;
          }
          break;
        }

        case FLASH_ERASE_UNLOCKED:
        {
          /* detect CHIP ERASE command */
          if ((address == 0x555) && (data == 0x10))
          {
            int i;

            /* assume any writable 64KB bank except first one is not write-protected (max. 8MB flash memory) */
            for (i=0x01; i<0x80; i++)
            {
              /* only unprotected sectors can be erased */
              if (m68k.memory_map[i].write16 != m68k_unused_16_w)
              {
                memset(m68k.memory_map[i].base, 0xff, 0x10000);
                break;
              }
            }

            /* reset to default read mode */
            flash.mode = FLASH_READ;
          }

          /* detect SECTOR ERASE command */
          else if (data == 0x30)
          {
            /* assume any writable 64KB bank except first one is not write-protected */
            if (index > 0)
            {
              /* for simplicity, erase whole 64KB bank (assume 64KB writable sectors only) */
              memset(m68k.memory_map[index].base, 0xff, 0x10000);
            }
 
            /* reset to default read mode */
            flash.mode = FLASH_READ;
          }
          break;
        }

        default:
        {
          /* detect UNLOCK sequence */
          if ((flash.addr == 0x555) && (flash.data == 0xAA) && (address == 0x2AA) && (data == 0x55))
          {
            flash.mode = FLASH_UNLOCKED;
          }

          /* detect CFI QUERY command */
          else if ((address == 0x55) && (data == 0x98))
          {
            flash.mode = FLASH_CFI_QUERY;
          }
          break;
        }
      }
    }
  }

  /* latch address & data */
  flash.addr = address;
  flash.data = data;
}

unsigned int flash_cfi_read(unsigned int address)
{
  /* detect current mode */
  if (flash.mode >= FLASH_OTP_ENABLED)
  {
    /* OTP area */
    return flash.otp_area[address & (FLASH_OTP_AREA_SIZE - 1)];
  }

  if (flash.mode == FLASH_AUTOSELECT)
  {
    /* Manufacturer and Device ID codes */
    return flash.id[address & flash.readmask];
  }

  if (flash.mode == FLASH_CFI_QUERY)
  {
    /* CFI data array (identification string, system interface information and device geometry definition only) */
    int index = address - 0x10;
    if ((index >= 0x00) && (index < CFI_QUERY_TABLE_LEN))
    {
      /* D8-D15 are forced to 0 */
      return flash.cfi_data[index];
    }
  }

  /* default read mode */
  if (m68k.memory_map[(address>>15)&0xff].base == sram.sram)
  {
    /* backup RAM (always stored in big endian format) */
    return READ_WORD(sram.sram, (address << 1) & 0xfffe);
  }
  else
  {
    /* cartridge ROM */
    return *(uint16 *)(m68k.memory_map[(address>>15)&0xff].base + ((address << 1) & 0xfffe));
  }
}

int flash_cfi_context_save(uint8 *state)
{
  int bufferptr = 0;

  save_param(&flash.addr, sizeof(flash.addr));
  save_param(&flash.data, sizeof(flash.data));
  save_param(&flash.mode, sizeof(flash.mode));

  return bufferptr;
}

int flash_cfi_context_load(uint8 *state)
{
  int bufferptr = 0;

  load_param(&flash.addr, sizeof(flash.addr));
  load_param(&flash.data, sizeof(flash.data));
  load_param(&flash.mode, sizeof(flash.mode));

  return bufferptr;
}
