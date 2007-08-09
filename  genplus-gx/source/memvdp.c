/*
    memvdp.c --
    Memory handlers for when the VDP reads the V-bus during DMA.
*/

#include "shared.h"

unsigned int vdp_dma_r(unsigned int address)
{
	switch ((address >> 21) & 7)
    {
		case 0:	/* Cartridge ROM */
		case 1:
            return *(uint16 *)(cart_rom + address);

		case 2:	/* Unused */
		case 3:
			return 0xFF00;

		case 4:	/* Work RAM */
		case 6:
		case 7:
      		return *(uint16 *)(work_ram + (address & 0xffff));

		case 5:	/* Z80 area and I/O chip */
			/* Z80 area always returns $FFFF */
			if (address <= 0xA0FFFF)
			{
				/* Return $FFFF only when the Z80 isn't hogging the Z-bus.
				   (e.g. Z80 isn't reset and 68000 has the bus) */
				return (zbusack == 0) ? 0xFFFF : *(uint16 *)(work_ram + (address & 0xffff));
			}
			
      		/* The I/O chip and work RAM try to drive the data bus which
	   		   results in both values being combined in random ways when read.
	   		   We return the I/O chip values which seem to have precedence, */
			else if (address <= 0xA1001F)
			{
      			uint8 temp = io_read((address >> 1) & 0x0F);
	  			return (temp << 8 | temp);
			}
      		/* All remaining locations access work RAM */
			else return *(uint16 *)(work_ram + (address & 0xffff));
    }

  return -1;
}

