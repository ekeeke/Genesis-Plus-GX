
#ifndef _SOUND_H_
#define _SOUND_H_

typedef struct
{
  int running;
  int enable;
  int count;
  int base;
  int index;
} t_timer;

/* Global variables */
extern uint8 fm_reg[2][0x100];
extern uint8 fm_latch[2];
extern uint8 fm_status;
extern t_timer timer[2];

/* Function prototypes */
void sound_init (void);
void fm_reset (void);
void fm_restore(void);
void fm_write (int address, int data);
int fm_read (int address);
void fm_update_timers (int inc);
void psg_write (int data);

#endif /* _SOUND_H_ */
