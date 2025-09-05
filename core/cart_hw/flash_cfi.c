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
  FLASH_CFI_QUERY
} T_CFI_MODE;

typedef struct
{
  uint32 addr;            /* latched address register */
  uint32 data;            /* latched data register */
  T_CFI_MODE mode;        /* current operating mode */
  const uint8 *cfi_data;  /* CFI query data array */
  const uint16 *id;       /* Manufacturer & Device id codes */
  uint8 readmask;         /* Autoselect mode read address mask */
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

void flash_cfi_init(T_FLASH_CFI_TYPE type)
{
  memset(&flash, 0x00, sizeof(flash));
  flash.cfi_data  = cfi_query[type];
  flash.id        = flash_id[type];
  flash.readmask  = flash_readmask[type];
}

void flash_cfi_write(unsigned int address, unsigned int data)
{
  if (flash.mode == FLASH_PROGRAM)
  {
    /* can only clear bits set to 1 in unprotected sectors (for simplicity, assume only backup RAM area is not protected) */
    if (m68k.memory_map[(address>>15)&0xff].base == sram.sram)
    {
      WRITE_WORD(sram.sram, (address << 1) & 0xfffe, data & READ_WORD(sram.sram, (address << 1) & 0xfffe));
    }

    /* reset to read mode */
    flash.mode = FLASH_READ;
  }
  else
  {
    /* only A0-A11 and D0-D7 are decoded */
    address &= 0xfff;
    data &= 0xff;

    /* detect RESET command */
    if (data == 0xf0)
    {
      /* reset to read mode */
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

            /* max. supported Flash Memory mapped size is 8MB */
            for (i=0x00; i<0x80; i++)
            {
              /* can only erase unprotected sectors so, for simplicity, assume only backup RAM area (64KB sector) is not protected */
              if (m68k.memory_map[i].base == sram.sram)
              {
                memset(sram.sram, 0xff, sizeof(sram.sram));
                break;
              }
            }

            /* reset to read mode */
            flash.mode = FLASH_READ;
          }

          /* detect SECTOR ERASE command */
          else if ((data == 0x30))
          {
            /* can only erase unprotected sectors so, for simplicity, assume only backup RAM area is not protected */
            if (m68k.memory_map[(address>>15)&0xff].base == sram.sram)
            {
              /* for simplicity, erase whole backup RAM area (64KB sector) */
              memset(sram.sram, 0xff, sizeof(sram.sram));
            }
 
            /* reset to read mode */
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
  
  /* default read (might be possible only in READ mode ?) */
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
