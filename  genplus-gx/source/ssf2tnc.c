/****************************************************************************
 * Super Street Fighter 2 - The New Challengers
 *
 * This is the ROM mapper for this game.
 * Called from mem68k.c
 ***************************************************************************/
#include "shared.h"

#ifdef NGC
char *shadow_rom = (char *) 0x8000;
#else
char shadow_rom[0x500000];
#endif

int SSF2TNC = 0;
static int oldaddr = 0;
static uint8 olddata = 0;

void ssf2bankrom (int address, unsigned char data)
{
	/* Banking performed on odd addresses only */
    if (!(address & 1)) return;

	/*
	 * Whilst debugging the ARAM stuff, I noticed that this ROM
	 * requested the same address/data on subsequent calls.
	 * This is just a little speedup, which seems to make the
	 * sound etc in the intro much less choppy.
	 *
	 * Happy dance anyone?
	 */
	if ((oldaddr == address) && (olddata == data)) return;

    if ((address > 0xa130f2) && (address < 0xa13100))
    {
		switch (address & 0xf)
	    {
			case 0x3: /* 080000-0FFFFF */
#ifdef NGC
			  ARAMFetch (cart_rom + 0x080000, shadow_rom + (data * 0x80000), 0x80000);
#else
			  memcpy (cart_rom + 0x080000, shadow_rom + (data * 0x80000), 0x80000);
#endif
	          break;
            
			case 0x5: /* 100000 - 17FFFF */
#ifdef NGC
	          ARAMFetch (cart_rom + 0x100000, shadow_rom + (data * 0x80000), 0x80000);
#else
	          memcpy (cart_rom + 0x100000, shadow_rom + (data * 0x80000), 0x80000);
#endif
	          break;

	        case 0x7: /* 180000 - 1FFFFF */
#ifdef NGC
	          ARAMFetch (cart_rom + 0x180000, shadow_rom + ( data * 0x80000), 0x80000);
#else
	          memcpy (cart_rom + 0x180000, shadow_rom + ( data * 0x80000), 0x80000);
#endif
	          break;

	        case 0x9: /* 200000 - 27FFFF */
#ifdef NGC
	          ARAMFetch (cart_rom + 0x200000, shadow_rom + ( data * 0x80000), 0x80000);
#else
	          memcpy (cart_rom + 0x200000, shadow_rom + ( data * 0x80000), 0x80000);
#endif
	          break;

	        case 0xb: /* 280000 - 2FFFFF */
#ifdef NGC
	          ARAMFetch (cart_rom + 0x280000, shadow_rom + ( data * 0x80000), 0x80000);
#else
	          memcpy (cart_rom + 0x280000, shadow_rom + ( data * 0x80000), 0x80000);
#endif
	          break;

	        case 0xd: /* 300000 - 37FFFF */
#ifdef NGC
	          ARAMFetch (cart_rom + 0x300000, shadow_rom + ( data * 0x80000), 0x80000);
#else
	          memcpy (cart_rom + 0x300000, shadow_rom + ( data * 0x80000), 0x80000);
#endif
	          break;

	        case 0xf: /* 380000 - 3FFFFF */
#ifdef NGC
	          ARAMFetch (cart_rom + 0x380000, shadow_rom + ( data * 0x80000), 0x80000);
#else
	          memcpy (cart_rom + 0x380000, shadow_rom + ( data * 0x80000), 0x80000);
#endif
	          break;
        }
    }

    oldaddr = address;
    olddata = data;
}
