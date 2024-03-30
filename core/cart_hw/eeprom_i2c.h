/****************************************************************************
 *  Genesis Plus
 *  I2C Serial EEPROM (24Cxx) boards
 *
 *  Copyright (C) 2007-2016  Eke-Eke (Genesis Plus GX)
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

/* Some notes from 8BitWizard (http://gendev.spritesmind.net/forum/viewtopic.php?t=206):
 *
 * Mode 1 (7-bit) - the chip takes a single byte with a 7-bit memory address and a R/W bit (X24C01)
 * Mode 2 (8-bit) - the chip takes a 7-bit device address and R/W bit followed by an 8-bit memory address;
 * the device address may contain up to three more memory address bits (24C01 - 24C16).
 * You can also string eight 24C01, four 24C02, two 24C08, or various combinations, set their address config lines correctly,
 * and the result appears exactly the same as a 24C16
 * Mode 3 (16-bit) - the chip takes a 7-bit device address and R/W bit followed by a 16-bit memory address (24C32 and larger)
 *
 */

typedef enum
{
  STAND_BY = 0,
  WAIT_STOP,
  GET_DEVICE_ADR,
  GET_WORD_ADR_7BITS,
  GET_WORD_ADR_HIGH,
  GET_WORD_ADR_LOW,
  WRITE_DATA,
  READ_DATA
} T_I2C_STATE;

typedef enum
{
  NO_EEPROM = -1,
  EEPROM_X24C01,
  EEPROM_X24C02,
  EEPROM_24C01,
  EEPROM_24C02,
  EEPROM_24C04,
  EEPROM_24C08,
  EEPROM_24C16,
  EEPROM_24C32,
  EEPROM_24C64,
  EEPROM_24C65,
  EEPROM_24C128,
  EEPROM_24C256,
  EEPROM_24C512
} T_I2C_TYPE;

typedef struct
{
  uint8_t address_bits;
  uint16_t size_mask;
  uint16_t pagewrite_mask;
} T_I2C_SPEC;

static const T_I2C_SPEC i2c_specs[] =
{
  { 7 , 0x7F   , 0x03},
  { 8 , 0xFF   , 0x03},
  { 8 , 0x7F   , 0x07},
  { 8 , 0xFF   , 0x07},
  { 8 , 0x1FF  , 0x0F},
  { 8 , 0x3FF  , 0x0F},
  { 8 , 0x7FF  , 0x0F},
  {16 , 0xFFF  , 0x1F},
  {16 , 0x1FFF , 0x1F},
  {16 , 0x1FFF , 0x3F},
  {16 , 0x3FFF , 0x3F},
  {16 , 0x7FFF , 0x3F},
  {16 , 0xFFFF , 0x7F}
};

typedef struct
{
  char id[16];
  uint32_t sp;
  uint16_t chk;
  void (*mapper_init)(void);
  T_I2C_TYPE eeprom_type;
} T_I2C_GAME;

extern void mapper_i2c_ea_init(void);
extern void mapper_i2c_sega_init(void);
extern void mapper_i2c_acclaim_16M_init(void);
extern void mapper_i2c_acclaim_32M_init(void);
extern void mapper_i2c_jcart_init(void);

static const T_I2C_GAME i2c_database[] = 
{
  {"T-50176"  , 0          , 0      , mapper_i2c_ea_init          , EEPROM_X24C01 }, /* Rings of Power */
  {"T-50396"  , 0          , 0      , mapper_i2c_ea_init          , EEPROM_X24C01 }, /* NHLPA Hockey 93 */
  {"T-50446"  , 0          , 0      , mapper_i2c_ea_init          , EEPROM_X24C01 }, /* John Madden Football 93 */
  {"T-50516"  , 0          , 0      , mapper_i2c_ea_init          , EEPROM_X24C01 }, /* John Madden Football 93 (Championship Ed.) */
  {"T-50606"  , 0          , 0      , mapper_i2c_ea_init          , EEPROM_X24C01 }, /* Bill Walsh College Football (warning: invalid SRAM header !) */
  {" T-12046" , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Megaman - The Wily Wars (warning: SRAM hack exists !) */
  {" T-12053" , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Rockman Mega World (warning: SRAM hack exists !) */
  {"MK-1215"  , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Evander 'Real Deal' Holyfield's Boxing */
  {"MK-1228"  , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Greatest Heavyweights of the Ring (U)(E) */
  {"G-5538"   , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Greatest Heavyweights of the Ring (J) */
  {"PR-1993"  , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Greatest Heavyweights of the Ring (Prototype) */
  {" G-4060"  , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Wonderboy in Monster World (warning: SRAM hack exists !) */
  {"00001211" , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Sports Talk Baseball */
  {"00004076" , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Honoo no Toukyuuji Dodge Danpei */
  {"G-4524"   , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Ninja Burai Densetsu */
  {"00054503" , 0          , 0      , mapper_i2c_sega_init        , EEPROM_X24C01 }, /* Game Toshokan  */
  {"T-81033"  , 0          , 0      , mapper_i2c_acclaim_16M_init , EEPROM_X24C02 }, /* NBA Jam (J) */
  {"T-081326" , 0          , 0      , mapper_i2c_acclaim_16M_init , EEPROM_X24C02 }, /* NBA Jam (UE) */
  {"T-081276" , 0          , 0      , mapper_i2c_acclaim_32M_init , EEPROM_24C02  }, /* NFL Quarterback Club */
  {"T-81406"  , 0          , 0      , mapper_i2c_acclaim_32M_init , EEPROM_24C04  }, /* NBA Jam TE */
  {"T-081586" , 0          , 0      , mapper_i2c_acclaim_32M_init , EEPROM_24C16  }, /* NFL Quarterback Club '96 */
  {"T-81476"  , 0          , 0      , mapper_i2c_acclaim_32M_init , EEPROM_24C65  }, /* Frank Thomas Big Hurt Baseball */
  {"T-81576"  , 0          , 0      , mapper_i2c_acclaim_32M_init , EEPROM_24C65  }, /* College Slam */
  {"T-120106" , 0          , 0      , mapper_i2c_jcart_init       , EEPROM_24C08  }, /* Brian Lara Cricket */
  {"00000000" , 0x444e4c44 , 0x168B , mapper_i2c_jcart_init       , EEPROM_24C08  }, /* Micro Machines Military */
  {"00000000" , 0x444e4c44 , 0x165E , mapper_i2c_jcart_init       , EEPROM_24C16  }, /* Micro Machines Turbo Tournament 96 */
  {"T-120096" , 0          , 0      , mapper_i2c_jcart_init       , EEPROM_24C16  }, /* Micro Machines 2 - Turbo Tournament */
  {"T-120146" , 0          , 0      , mapper_i2c_jcart_init       , EEPROM_24C65  }, /* Brian Lara Cricket 96 / Shane Warne Cricket */
  {"00000000" , 0xfffffffc , 0x168B , mapper_i2c_jcart_init       , NO_EEPROM     }, /* Super Skidmarks */
  {"00000000" , 0xfffffffc , 0x165E , mapper_i2c_jcart_init       , NO_EEPROM     }, /* Pete Sampras Tennis (Prototype) */
  {"T-120066" , 0          , 0      , mapper_i2c_jcart_init       , NO_EEPROM     }, /* Pete Sampras Tennis */
  {"T-123456" , 0          , 0      , mapper_i2c_jcart_init       , NO_EEPROM     }, /* Pete Sampras Tennis 96 */
  {"XXXXXXXX" , 0          , 0xDF39 , mapper_i2c_jcart_init       , NO_EEPROM     }, /* Pete Sampras Tennis 96 (Prototype ?) */
};

struct eeprom_i2c_t
{
  uint8_t sda;              /* current SDA line state */
  uint8_t scl;              /* current SCL line state */
  uint8_t old_sda;          /* previous SDA line state */
  uint8_t old_scl;          /* previous SCL line state */
  uint8_t cycles;           /* operation internal cycle (0-9) */
  uint8_t rw;               /* operation type (1:READ, 0:WRITE) */
  uint16_t device_address;  /* device address */
  uint16_t word_address;    /* memory address */
  uint8_t buffer;           /* write buffer */
  T_I2C_STATE state;      /* current operation state */
  T_I2C_SPEC spec;        /* EEPROM characteristics */
  uint8_t scl_in_bit;       /* SCL (write) bit position */
  uint8_t sda_in_bit;       /* SDA (write) bit position */
  uint8_t sda_out_bit;      /* SDA (read) bit position */
};

/* Function prototypes */
extern void eeprom_i2c_init(void);

