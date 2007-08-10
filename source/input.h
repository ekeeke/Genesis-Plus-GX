#ifndef _INPUT_H_
#define _INPUT_H_
#include "types.h"

/* Input devices */
#ifdef NGC
#define MAX_INPUTS          (4)     
#else
#define MAX_INPUTS          (8)     
#endif
#define MAX_DEVICES         (8)
#define DEVICE_3BUTTON      (0x00)	/* 3-button gamepad */
#define DEVICE_6BUTTON      (0x01)	/* 6-button gamepad */
#define DEVICE_MOUSE	    (0x02)	/* Sega Mouse (not supported) */
#define DEVICE_LIGHTGUN     (0x03)	/* Sega Menacer */
#define DEVICE_2BUTTON      (0x04)	/* 2-button gamepad (not supported) */
#define NO_DEVICE		    (0x0F)	/* unconnected */

/* Input bitmasks */
#define INPUT_MODE      (0x00000800)
#define INPUT_Z         (0x00000400)
#define INPUT_Y         (0x00000200)
#define INPUT_X         (0x00000100)
#define INPUT_START     (0x00000080)
#define INPUT_C         (0x00000040)
#define INPUT_B         (0x00000020)
#define INPUT_A         (0x00000010)
#define INPUT_LEFT      (0x00000008)
#define INPUT_RIGHT     (0x00000004)
#define INPUT_DOWN      (0x00000002)
#define INPUT_UP        (0x00000001)

/* System bitmasks */
#define SYSTEM_GAMEPAD    (0)	/* Single Gamepad */
#define SYSTEM_TEAMPLAYER (1)	/* Sega TeamPlayer (1~8 players) */
#define SYSTEM_WAYPLAY    (2)	/* EA 4-Way Play (1~4 players) */
#define SYSTEM_MENACER    (3)	/* SEGA Menacer Lightgun */
#define NO_SYSTEM         (4)	/* Unconnected Port*/

typedef struct
{
  uint8 dev[MAX_DEVICES];	/* Can be any of the DEVICE_* values */
  uint32 pad[MAX_DEVICES];	/* Can be any of the INPUT_* bitmasks */
  uint8 system[2];			/* Can be any of the SYSTEM_* bitmasks (PORTA & PORTB) */
  uint8 max;				/* maximum number of connected devices */
} t_input;

/* global variables */
extern t_input input;
extern uint8 j_cart;

/* Function prototypes */
extern void input_reset (int padtype);
extern void input_update(void);
extern void input_raz(void);
extern void lightgun_set (void);
extern uint8 lightgun_read (void);
extern uint8 gamepad_read (uint8 i);
extern void gamepad_write (uint8 i, uint8 data);
extern uint8 multitap_read (uint8 port);
extern void multitap_write (uint8 port, uint8 data);

#endif
