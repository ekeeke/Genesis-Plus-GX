#ifndef _IO_H_
#define _IO_H_

#define REGION_USA			0x80
#define REGION_JAPAN_NTSC	0x00
#define REGION_EUROPE		0xC0
#define REGION_JAPAN_PAL	0x40

/* Global variables */
extern uint8 io_reg[0x10];
extern uint8 region_code;
extern uint8 pad_type;

/* Function prototypes */
extern void io_reset(void);
extern void io_write(int offset, int value);
extern int io_read(int offset);

#endif /* _IO_H_ */

