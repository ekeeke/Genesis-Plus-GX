
#ifndef _MEMZ80_H_
#define _MEMZ80_H_

/* Function prototypes */
unsigned int cpu_readmem16 (unsigned int address);
void cpu_writemem16 (unsigned int address, unsigned int data);
unsigned int cpu_readport16 (unsigned int port);
void cpu_writeport16 (unsigned int port, unsigned int data);
void z80_unused_w (int address, int data);
int z80_unused_r (int address);
void z80_lockup_w (int address, int data);
int z80_lockup_r (int address);
int z80_vdp_r (int address);
void z80_vdp_w (int address, int data);

#endif /* _MEMZ80_H_ */
