/****************************************************************************
 * Super Street Fighter 2 - The New Challengers
 *
 * This is the ROM mapper for this game.
 * Called from mem68k.c
 ***************************************************************************/

/* Function prototypes */
extern void ssf2bankrom (int address, unsigned char data);

/* global variables */
extern int SSF2TNC;
#ifdef NGC
extern char *shadow_rom;
#else
extern char shadow_rom[0x500000];
#endif
