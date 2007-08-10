
#ifndef _MEM68K_H_
#define _MEM68K_H_

/* Function prototypes */
unsigned int m68k_read_bus_8 (unsigned int address);
unsigned int m68k_read_bus_16 (unsigned int address);
void m68k_unused_w (unsigned int address, unsigned int value);
void m68k_lockup_w_8 (unsigned int address, unsigned int value);
void m68k_lockup_w_16 (unsigned int address, unsigned int value);
unsigned int m68k_lockup_r_8 (unsigned int address);
unsigned int m68k_lockup_r_16 (unsigned int address);

#endif /* _MEM68K_H_ */
