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
 *  SRAM MANAGER
 ***************************************************************************/

#include "shared.h"

T_SRAM sram;

/****************************************************************************
 * A quick guide to SRAM on the Genesis
 *
 * This is based on observations only - it may be completely wrong!
 *
 * The SRAM definition is held at offset 0x1b0 of the ROM header.
 * From looking at several ROMS, an ID appears:
 *
 * 0x1b0 : 0x52 0x41 0xF8 0x20 0x00200001 0x0020ffff
 *
 * Assuming 64k SRAM / Battery RAM throughout
 ****************************************************************************/
void SRAM_Init ()
{
  memset (&sram, 0, sizeof (T_SRAM));
  memset (&sram.sram[0], 0xFF, 0x10000);
  sram.crc = crc32 (0, &sram.sram[0], 0x10000);

  if ((cart_rom[0x1b0] == 0x52) && (cart_rom[0x1b1] == 0x41))
  {
  	  sram.on = 1;
	  sram.write = 1;
	  sram.detected = 1;
      sram.start = READ_WORD_LONG(cart_rom, 0x1b4);
      sram.end = READ_WORD_LONG(cart_rom, 0x1b8);

	  /* some games have incorrect header informations */
	  if ((sram.start > sram.end) || ((sram.end - sram.start) >= 0x10000))
		sram.end = sram.start + 0xffff;
	  sram.start &= 0xfffffffe;
      sram.end |= 1;

	  /* game using serial EEPROM as external RAM */
	  if (sram.end - sram.start < 2) EEPROM_Init();
  }
  else 
  {
	  /* set SRAM memory region by default */
	  sram.start = 0x200000;
      sram.end = 0x20ffff;

	  /* set SRAM ON by default if game is smaller than 2M */
	  if (genromsize <= 0x200000)
	  {
		  sram.on = 1;
		  sram.write = 1;
      }

	  /* some games using EEPROM don't have the correct header */
	  EEPROM_Init();
  }
}
