/****************************************************************************
 *  gx_input.c
 *
 *  Genesis Plus GX input support
 *
 *  Eke-Eke (2008,2009)
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
 ***************************************************************************/

#include "shared.h"
#include "font.h"
#include "gui.h"

/* Analog sticks sensitivity */
#define ANALOG_SENSITIVITY 30

/* Delay before held keys triggering */
/* higher is the value, less responsive is the key update */
#define HELD_DELAY 30

/* Direction & selection update speed when a key is being held */
/* lower is the value, faster is the key update */
#define HELD_SPEED 4

/* Menu request flag */
u8 ConfigRequested = 0;

/* Configurable Genesis keys */
#define KEY_BUTTONA 0
#define KEY_BUTTONB 1
#define KEY_BUTTONC 2
#define KEY_START   3
#define KEY_BUTTONX 4  /* 6-buttons only */
#define KEY_BUTTONY 5  /* 6-buttons only */
#define KEY_BUTTONZ 6  /* 6-buttons only */
#define KEY_MODE    7  /* 6-buttons only */

static const char *keys_name[MAX_KEYS] =
{
  "Button A",
  "Button B",
  "Button C",
  "START Button",
  "Button X",
  "Button Y",
  "Button Z",
  "MODE Button"
};

static int held_cnt = 0;


#ifdef HW_RVL

#define PAD_UP    0   
#define PAD_DOWN  1
#define PAD_LEFT  2
#define PAD_RIGHT 3

/* default directions mapping  */
static u32 wpad_dirmap[3][4] =
{
  {WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT, WPAD_BUTTON_UP, WPAD_BUTTON_DOWN},                                /* WIIMOTE */
  {WPAD_BUTTON_UP, WPAD_BUTTON_DOWN, WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT},                                /* WIIMOTE + NUNCHUK */
  {WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN, WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT} /* CLASSIC */
};
#endif


/***************************************************************************************/
/*   Gamecube PAD support                                                              */
/***************************************************************************************/
static void pad_config(int chan, int max_keys)
{
  int i;
  u16 p,key;
  char msg[30];

  /* reset VSYNC callback */
  VIDEO_SetPostRetraceCallback(NULL);
  VIDEO_Flush();

  /* Check if PAD is connected */
  if (!(PAD_ScanPads() & (1<<chan)))
  {
    /* restore inputs update callback */
    VIDEO_SetPostRetraceCallback(gx_input_UpdateMenu);
    VIDEO_Flush();
    sprintf(msg, "PAD #%d is not connected !", chan+1);
    GUI_WaitPrompt("Error",msg);
    return;
  }

  /* Configure each keys */
  for (i=0; i<max_keys; i++)
  {
    /* remove any pending keys */
    while (PAD_ButtonsHeld(chan))
    {
      VIDEO_WaitVSync();
      PAD_ScanPads();
    }

    /* wait for user input */
    sprintf(msg,"Press key for %s\n(Z to return)",keys_name[i]);
    GUI_MsgBoxUpdate(0,msg);

    key = 0;
    while (!key)
    {
      /* update PAD status */
      VIDEO_WaitVSync();
      PAD_ScanPads();
      p = PAD_ButtonsDown(chan);

      /* find pressed key */
      if (p & PAD_TRIGGER_Z) key = 0xff;
      else if (p & PAD_BUTTON_A) key = PAD_BUTTON_A;
      else if (p & PAD_BUTTON_B) key = PAD_BUTTON_B;
      else if (p & PAD_BUTTON_X) key = PAD_BUTTON_X;
      else if (p & PAD_BUTTON_Y) key = PAD_BUTTON_Y;
      else if (p & PAD_TRIGGER_R) key = PAD_TRIGGER_R;
      else if (p & PAD_TRIGGER_L) key = PAD_TRIGGER_L;
      else if (p & PAD_BUTTON_START) key = PAD_BUTTON_START;
    }

    /* update key mapping */
    if (key !=0xff) config.pad_keymap[chan][i] = key;
    else break;
  }

  /* remove any pending keys */
  while (PAD_ButtonsHeld(chan))
  {
    VIDEO_WaitVSync();
    PAD_ScanPads();
  }

  /* restore inputs update callback */
  VIDEO_SetPostRetraceCallback(gx_input_UpdateMenu);
  VIDEO_Flush();
}

static void pad_update(s8 chan, u8 i)
{
  /* PAD status */
  u16 p = PAD_ButtonsHeld(chan);
  s8 x  = PAD_StickX (chan);
  s8 y  = PAD_StickY (chan);

  if (p & PAD_TRIGGER_Z)
  {
    /* Menu Request */
    ConfigRequested = 1;
    return;
  }
  else if ((p & PAD_TRIGGER_L) && (p & PAD_TRIGGER_Z))
  {
    /* Soft RESET */
    set_softreset();
  }

  /* Retrieve current key mapping */
  u16 pad_keymap[MAX_KEYS];
  memcpy(pad_keymap, config.pad_keymap[chan], MAX_KEYS * sizeof(u16));

  /* Generic buttons */
  if (p & pad_keymap[KEY_BUTTONA]) input.pad[i]  |= INPUT_A;
  if (p & pad_keymap[KEY_BUTTONB]) input.pad[i]  |= INPUT_B;
  if (p & pad_keymap[KEY_BUTTONC]) input.pad[i]  |= INPUT_C;
  if (p & pad_keymap[KEY_BUTTONX]) input.pad[i]  |= INPUT_X;
  if (p & pad_keymap[KEY_BUTTONY]) input.pad[i]  |= INPUT_Y;
  if (p & pad_keymap[KEY_BUTTONZ]) input.pad[i]  |= INPUT_Z;
  if (p & pad_keymap[KEY_START])   input.pad[i]  |= INPUT_START;

  /* Analog devices */
  if (input.dev[i] == DEVICE_LIGHTGUN)
  {
    /* Lightgun cursor position (x,y) */
    input.analog[i-4][0] += x / ANALOG_SENSITIVITY;
    input.analog[i-4][1] -= y / ANALOG_SENSITIVITY;
    if (input.analog[i-4][0] < 0) input.analog[i-4][0] = 0;
    else if (input.analog[i-4][0] > bitmap.viewport.w) input.analog[i-4][0] = bitmap.viewport.w;
    if (input.analog[i-4][1] < 0) input.analog[i-4][1] = 0;
    else if (input.analog[i-4][1] > bitmap.viewport.h) input.analog[i-4][1] = bitmap.viewport.h;
  }
  else if ((system_hw == SYSTEM_PICO) && (i == 0))
  {
    /* PEN tablet position (x,y) */
    input.analog[0][0] += x / ANALOG_SENSITIVITY;
    input.analog[0][1] -= y / ANALOG_SENSITIVITY;
    if (input.analog[0][0] < 0x17c) input.analog[0][0] = 0x17c;
    else if (input.analog[0][0] > 0x3c) input.analog[0][0] = 0x3c;
    if (input.analog[0][1] < 0x1fc) input.analog[0][1] = 0x1fc;
    else if (input.analog[0][1] > 0x3f3) input.analog[0][1] = 0x3f3;
  }
  else if (input.dev[i] == DEVICE_MOUSE)
  {
    /* MOUSE relative movement (-255,255) */
    input.analog[2][0] =  (x / ANALOG_SENSITIVITY) * 2;
    input.analog[2][1] =  (y / ANALOG_SENSITIVITY) * 2;
    if (config.invert_mouse) input.analog[2][1] = -input.analog[2][1];
  }

  /* Gamepad device */
  else
  {
    if ((p & PAD_BUTTON_UP)         || (y >  ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_UP;
    else if ((p & PAD_BUTTON_DOWN)  || (y < -ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_DOWN;
    if ((p & PAD_BUTTON_LEFT)       || (x < -ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_LEFT;
    else if ((p & PAD_BUTTON_RIGHT) || (x >  ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_RIGHT;
  }
}


/***************************************************************************************/
/*   Wii WPAD support                                                                  */
/***************************************************************************************/
#ifdef HW_RVL

static s8 WPAD_StickX(WPADData *data,u8 right)
{
  float mag = 0.0;
  float ang = 0.0;

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * sin(M_PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}


static s8 WPAD_StickY(WPADData *data, u8 right)
{
  float mag = 0.0;
  float ang = 0.0;

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* calculate X value (angle need to be converted into radian) */
  if (mag > 1.0) mag = 1.0;
  else if (mag < -1.0) mag = -1.0;
  double val = mag * cos(M_PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}

static void wpad_config(u8 chan, u8 exp, u8 max_keys)
{
  int i;
  char msg[30];
  u32 key,p = 255;

  /* remove inputs update callback */
  VIDEO_SetPostRetraceCallback(NULL);
  VIDEO_Flush();

  /* Check if device is connected */
  WPAD_Probe(chan, &p);
  if (((exp > WPAD_EXP_NONE) && (p != exp)) || (p == 255))
  {
    /* restore inputs update callback */
    VIDEO_SetPostRetraceCallback(gx_input_UpdateMenu);
    VIDEO_Flush();

    if (exp == WPAD_EXP_NONE)     sprintf(msg, "WIIMOTE #%d is not connected !", chan+1);
    if (exp == WPAD_EXP_NUNCHUK)  sprintf(msg, "NUNCHUK #%d is not connected !", chan+1);
    if (exp == WPAD_EXP_CLASSIC)  sprintf(msg, "CLASSIC #%d is not connected !", chan+1);
    GUI_WaitPrompt("Error",msg);
    return;
  }

  /* loop on each mapped keys */
  for (i=0; i<max_keys; i++)
  {
    /* remove any pending buttons */
    while (WPAD_ButtonsHeld(chan))
    {
      WPAD_ScanPads();
      VIDEO_WaitVSync();
    }

    /* wait for user input */
    sprintf(msg,"Press key for %s\n(HOME to return)",keys_name[i]);
    GUI_MsgBoxUpdate(0,msg);

    /* wait for input */
    key = 0;
    while (!key)
    {
      VIDEO_WaitVSync();
      WPAD_ScanPads();
      p = WPAD_ButtonsDown(chan);

      switch (exp)
      {
        /* Wiimote (TODO: add motion sensing !) */
        case WPAD_EXP_NONE:
          if (p & WPAD_BUTTON_HOME) key = 0xff;
          else if (p & WPAD_BUTTON_2) key = WPAD_BUTTON_2;
          else if (p & WPAD_BUTTON_1) key = WPAD_BUTTON_1;
          else if (p & WPAD_BUTTON_B) key = WPAD_BUTTON_B;
          else if (p & WPAD_BUTTON_A) key = WPAD_BUTTON_A;
          else if (p & WPAD_BUTTON_PLUS) key = WPAD_BUTTON_PLUS;
          else if (p & WPAD_BUTTON_MINUS) key = WPAD_BUTTON_MINUS;
          break;
        
        /* Wiimote + Nunchuk (TODO: add motion sensing !) */
        case WPAD_EXP_NUNCHUK:
          if (p & WPAD_BUTTON_HOME) key = 0xff;
          else if (p & WPAD_BUTTON_2) key = WPAD_BUTTON_2;
          else if (p & WPAD_BUTTON_1) key = WPAD_BUTTON_1;
          else if (p & WPAD_BUTTON_B) key = WPAD_BUTTON_B;
          else if (p & WPAD_BUTTON_A) key = WPAD_BUTTON_A;
          else if (p & WPAD_BUTTON_PLUS) key = WPAD_BUTTON_PLUS;
          else if (p & WPAD_BUTTON_MINUS) key = WPAD_BUTTON_MINUS;
          else if (p & WPAD_NUNCHUK_BUTTON_Z) key = WPAD_NUNCHUK_BUTTON_Z;
          else if (p & WPAD_NUNCHUK_BUTTON_C) key = WPAD_NUNCHUK_BUTTON_C;
          break;

        /* Classic Controller */
        case WPAD_EXP_CLASSIC:
          if (p & WPAD_CLASSIC_BUTTON_HOME) key = 0xff;
          else if (p & WPAD_CLASSIC_BUTTON_X) key = WPAD_CLASSIC_BUTTON_X;
          else if (p & WPAD_CLASSIC_BUTTON_A) key = WPAD_CLASSIC_BUTTON_A;
          else if (p & WPAD_CLASSIC_BUTTON_Y) key = WPAD_CLASSIC_BUTTON_Y;
          else if (p & WPAD_CLASSIC_BUTTON_B) key = WPAD_CLASSIC_BUTTON_B;
          else if (p & WPAD_CLASSIC_BUTTON_ZL) key = WPAD_CLASSIC_BUTTON_ZL;
          else if (p & WPAD_CLASSIC_BUTTON_ZR) key = WPAD_CLASSIC_BUTTON_ZR;
          else if (p & WPAD_CLASSIC_BUTTON_PLUS) key = WPAD_CLASSIC_BUTTON_PLUS;
          else if (p & WPAD_CLASSIC_BUTTON_MINUS) key = WPAD_CLASSIC_BUTTON_MINUS;
          else if (p & WPAD_CLASSIC_BUTTON_FULL_L) key = WPAD_CLASSIC_BUTTON_FULL_L;
          else if (p & WPAD_CLASSIC_BUTTON_FULL_R) key = WPAD_CLASSIC_BUTTON_FULL_R;
          break;

        default:
          key = 0xff;
          break;
      }
    }

    /* update key mapping */
    if (key != 0xff) config.wpad_keymap[exp + (chan * 3)][i] = key;
    else break;
  }

  /* remove any pending buttons */
  while (WPAD_ButtonsHeld(chan))
  {
    WPAD_ScanPads();
    VIDEO_WaitVSync();
  }

  /* restore inputs update callback */
  VIDEO_SetPostRetraceCallback(gx_input_UpdateMenu);
  VIDEO_Flush();
}

static float old_x = 0.0;
static float old_y = 0.0;

static void wpad_update(s8 chan, u8 i, u32 exp)
{
  /* WPAD data */
  WPADData *data = WPAD_Data(chan);

  /* WPAD held buttons */
  u32 p = data->btns_h;

  if ((p & WPAD_BUTTON_HOME) || (p & WPAD_CLASSIC_BUTTON_HOME))
  {
    /* Menu Request */
    ConfigRequested = 1;
    return;
  }
  else if (((p & WPAD_BUTTON_PLUS) && (p & WPAD_BUTTON_MINUS)) ||
           ((p & WPAD_CLASSIC_BUTTON_PLUS) && (p & WPAD_CLASSIC_BUTTON_MINUS)))
  {
    /* Soft RESET */
    set_softreset();
  }

  /* Retrieve current key mapping */
  u32 *wpad_keymap = config.wpad_keymap[exp + (3 * chan)];

  /* Generic buttons */
  if (p & wpad_keymap[KEY_BUTTONA]) input.pad[i]  |= INPUT_A;
  if (p & wpad_keymap[KEY_BUTTONB]) input.pad[i]  |= INPUT_B;
  if (p & wpad_keymap[KEY_BUTTONC]) input.pad[i]  |= INPUT_C;
  if (p & wpad_keymap[KEY_BUTTONX]) input.pad[i]  |= INPUT_X;
  if (p & wpad_keymap[KEY_BUTTONY]) input.pad[i]  |= INPUT_Y;
  if (p & wpad_keymap[KEY_BUTTONZ]) input.pad[i]  |= INPUT_Z;
  if (p & wpad_keymap[KEY_START])   input.pad[i]  |= INPUT_START;
  if (p & wpad_keymap[KEY_MODE])    input.pad[i]  |= INPUT_MODE;

  /* Analog sticks */
  s8 x = 0;
  s8 y = 0;
  if (exp != WPAD_EXP_NONE)
  {
    x = WPAD_StickX(data,0);
    y = WPAD_StickY(data,0);
  }

  /* IR structure */
  struct ir_t ir;

  /* Analog devices */
  if (input.dev[i] == DEVICE_LIGHTGUN)
  {
    /* Lightgun cursor position (x,y) */
    if (x || y)
    {
      /* analog stick relative positions */
      input.analog[i-4][0] += x / ANALOG_SENSITIVITY;
      input.analog[i-4][1] -= y / ANALOG_SENSITIVITY;
      if (input.analog[i-4][0] < 0) input.analog[i-4][0] = 0;
      else if (input.analog[i-4][0] > bitmap.viewport.w) input.analog[i-4][0] = bitmap.viewport.w;
      if (input.analog[i-4][1] < 0) input.analog[i-4][1] = 0;
      else if (input.analog[i-4][1] > bitmap.viewport.h) input.analog[i-4][1] = bitmap.viewport.h;
    }

    if (exp != WPAD_EXP_CLASSIC)
    {
      /* IR tracking */
      WPAD_IR(chan, &ir);
      if (ir.valid)
      {
        input.analog[i-4][0] = (ir.x * bitmap.viewport.w) / 640;
        input.analog[i-4][1] = (ir.y * bitmap.viewport.h) / 480;

        /* default button */
        if (p & WPAD_BUTTON_B) input.pad[i]  |= INPUT_B;
      }
    }
  }
  else if ((system_hw == SYSTEM_PICO) && (i == 0))
  {
    /* PEN tablet position (x,y) */
    if (x || y)
    {
      /* analog stick relative positions */
      input.analog[0][0] += x / ANALOG_SENSITIVITY;
      input.analog[0][1] -= y / ANALOG_SENSITIVITY;
      if (input.analog[0][0] < 0x17c) input.analog[0][0] = 0x17c;
      else if (input.analog[0][0] > 0x3c) input.analog[0][0] = 0x3c;
      if (input.analog[0][1] < 0x1fc) input.analog[0][1] = 0x1fc;
      else if (input.analog[0][1] > 0x3f3) input.analog[0][1] = 0x3f3;
    }

    if (exp != WPAD_EXP_CLASSIC)
    {
      /* IR tracking */
      WPAD_IR(chan, &ir);
      if (ir.valid)
      {
        input.analog[0][0] = 0x3c  + (ir.x * (0x17c - 0x3c  + 1)) / 640;
        input.analog[0][1] = 0x1fc + (ir.y * (0x3f3 - 0x1fc + 1)) / 480;

        /* default button */
        if (p & WPAD_BUTTON_B) input.pad[i]  |= INPUT_B;
      }
    }
  }
  else if (input.dev[i] == DEVICE_MOUSE)
  {
    /* Mouse relative movement (-255,255) */
    if (x || y)
    {
      /* analog stick relative positions */
      input.analog[2][0] = (x * 2) / ANALOG_SENSITIVITY;
      input.analog[2][1] = -(y * 2) / ANALOG_SENSITIVITY;
    }

    if (exp != WPAD_EXP_CLASSIC)
    {
      /* IR tracking */
      WPAD_IR(chan, &ir);
      if (ir.valid)
      {
        /* calculate mouse values */
        input.analog[2][0] = (ir.x - old_x);
        input.analog[2][1] = (ir.y - old_y);
        old_x = ir.x;
        old_y = ir.y;
        if (input.analog[2][0] > 255) input.analog[2][0] = 255;
        else if (input.analog[2][0] < -255) input.analog[2][0] = -255;
        if (input.analog[2][1] > 255) input.analog[2][1] = 255;
        else if (input.analog[2][1] < -255) input.analog[2][1] = -255;

        /* default button */
        if (p & WPAD_BUTTON_B) input.pad[i]  |= INPUT_B;
      }
    }

    /* Invert Y coordinate */
    if (!config.invert_mouse) input.analog[2][1] = -input.analog[2][1];
  }

  /* Gamepad device */
  else
  {
    if ((p & wpad_dirmap[exp][PAD_UP])          || (y >  ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_UP;
    else if ((p & wpad_dirmap[exp][PAD_DOWN])   || (y < -ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_DOWN;
    if ((p & wpad_dirmap[exp][PAD_LEFT])        || (x < -ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_LEFT;
    else if ((p & wpad_dirmap[exp][PAD_RIGHT])  || (x >  ANALOG_SENSITIVITY)) input.pad[i] |= INPUT_RIGHT;
  }
}
#endif


/***************************************************************************************/
/*   GX Input interface                                                                */
/***************************************************************************************/
void gx_input_Init(void)
{
  PAD_Init ();

#ifdef HW_RVL
  WPAD_Init();
  WPAD_SetIdleTimeout(60);
  WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
  WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
#endif

  VIDEO_SetPostRetraceCallback(gx_input_UpdateMenu);
  VIDEO_Flush();
}

void gx_input_SetDefault(void)
{
  int i,j;
  u32 exp;

  /* set default key mapping for each type of devices */
  for (i=0; i<4; i++)
  {
    config.pad_keymap[i][KEY_BUTTONA] = PAD_BUTTON_B;
    config.pad_keymap[i][KEY_BUTTONB] = PAD_BUTTON_A;
    config.pad_keymap[i][KEY_BUTTONC] = PAD_BUTTON_X;
    config.pad_keymap[i][KEY_START]   = PAD_BUTTON_START;
    config.pad_keymap[i][KEY_BUTTONX] = PAD_TRIGGER_L;
    config.pad_keymap[i][KEY_BUTTONY] = PAD_BUTTON_Y;
    config.pad_keymap[i][KEY_BUTTONZ] = PAD_TRIGGER_R;
    config.pad_keymap[i][KEY_MODE]    = 0;
  }

#ifdef HW_RVL
  for (i=0; i<4; i++)
  {
    /* Wiimote */
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONA] = WPAD_BUTTON_A;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONB] = WPAD_BUTTON_2;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONC] = WPAD_BUTTON_1;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_START]   = WPAD_BUTTON_PLUS;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONX] = 0;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONY] = 0;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONZ] = 0;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_MODE]    = 0;

    /* Wiimote + Nunchuk */
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONA] = WPAD_NUNCHUK_BUTTON_Z;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONB] = WPAD_BUTTON_A;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONC] = WPAD_BUTTON_B;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_START]   = WPAD_BUTTON_PLUS;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONX] = WPAD_NUNCHUK_BUTTON_C;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONY] = WPAD_BUTTON_1;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONZ] = WPAD_BUTTON_2;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_MODE]    = WPAD_BUTTON_MINUS;

    /* Classic Controller */
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONA] = WPAD_CLASSIC_BUTTON_Y;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONB] = WPAD_CLASSIC_BUTTON_B;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONC] = WPAD_CLASSIC_BUTTON_A;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_START]   = WPAD_CLASSIC_BUTTON_PLUS;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONX] = WPAD_CLASSIC_BUTTON_ZL;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONY] = WPAD_CLASSIC_BUTTON_X;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONZ] = WPAD_CLASSIC_BUTTON_ZR;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_MODE]    = WPAD_CLASSIC_BUTTON_MINUS;
  }
#endif

  /* Default player inputs */
  for (i=0; i<MAX_DEVICES; i++)
  {
    config.input[i].device  = -1;
    config.input[i].port    = i%4;
    config.input[i].padtype = 0;
  }

#ifdef HW_RVL
  /* Autodetect Wii Controllers */
  VIDEO_WaitVSync();
  for (i=0; i<4; i++)
  {
    /* try to autodetect connected controller */
    exp = 255;
    WPAD_Probe(i, &exp);
    if (exp <= WPAD_EXP_CLASSIC)
    {
      /* set expansion controller (or wiimote if no expansion) as default */
      config.input[i].device = exp + 1;
      config.input[i].port = i;
    }
    else
    {
      /* look for unassigned wiimotes */
      for (j=0; j<i; j++)
      {
        /* classic controller is already assigned, which means wiimote is not used */
        if (config.input[j].device == (WPAD_EXP_CLASSIC + 1))
        {
          /* assign wiimote  */
          config.input[i].device = 1;
          config.input[i].port = j;
        }
      }
    }
  }
#endif

  /* Autodetect Gamecube Controllers */
  VIDEO_WaitVSync();
  exp = PAD_ScanPads();
  for (i=0; i<4; i++)
  {
    /* check if Gamecube Controller is connected */
    if (exp & (1 << i))
    {
      for (j=0; j<MAX_DEVICES; j++)
      {
        /* look for the first unassigned player */
        if (config.input[j].device == -1)
        {
          config.input[j].device  = 0;
          config.input[j].port    = i;
        }
      }
    }
  }

  /* default emulated inputs */
  input.system[0] = SYSTEM_GAMEPAD;
  input.system[1] = (config.input[1].device != -1) ? SYSTEM_GAMEPAD : NO_SYSTEM;
}

void gx_input_Config(u8 chan, u8 type, u8 max)
{
  switch (type)
  {
    case 0:
      pad_config(chan, max);
      break;
    
    default:
#ifdef HW_RVL
      wpad_config(chan,type-1, max);
#endif
      break;
  }
}

void gx_input_UpdateEmu(void)
{
  int i;
  int player = 0;

  /* update controllers */
  PAD_ScanPads();
#ifdef HW_RVL
  WPAD_ScanPads();
#endif

#ifdef HW_RVL
  if (SYS_ResetButtonDown())
  {
    /* Soft RESET */
    set_softreset();
  }
#endif

  for (i=0; i<MAX_DEVICES; i++)
  {
    /* clear key status */
    input.pad[i] = 0;

    /* update inputs */
    if (input.dev[i] != NO_DEVICE)
    {
      if (config.input[player].device == 0)
        pad_update(config.input[player].port, i);

#ifdef HW_RVL
      else if (config.input[player].device > 0)
        wpad_update(config.input[player].port,i, config.input[player].device - 1);
#endif

      player ++;
    }
  }
}

/* Menu inputs update function (done by Video Interrupt callback) */
void gx_input_UpdateMenu(u32 cnt)
{
  /* PAD status update */
  PAD_ScanPads();

  /* PAD pressed keys */
  s16 pp = PAD_ButtonsDown(0);

  /* PAD held keys (direction/selection) */
  s16 hp = PAD_ButtonsHeld(0) & (PAD_BUTTON_UP|PAD_BUTTON_DOWN|PAD_BUTTON_LEFT|PAD_BUTTON_RIGHT|PAD_BUTTON_A);

  /* PAD analog sticks (handled as PAD held direction keys) */
  s8 x  = PAD_StickX(0);
  s8 y  = PAD_StickY(0);
  if (x > ANALOG_SENSITIVITY)       hp |= PAD_BUTTON_RIGHT;
  else if (x < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_LEFT;
  else if (y > ANALOG_SENSITIVITY)  hp |= PAD_BUTTON_UP;
  else if (y < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_DOWN;

#ifdef HW_RVL
  /* WPAD status update */
  WPAD_ScanPads();
  WPADData *data = WPAD_Data(0);

  /* WPAD pressed keys */
  u32 pw = data->btns_d;

  /* WPAD held keys (direction/selection) */
  u32 hw = data->btns_h & (WPAD_BUTTON_UP|WPAD_BUTTON_DOWN|WPAD_BUTTON_LEFT|WPAD_BUTTON_RIGHT|WPAD_BUTTON_A|WPAD_BUTTON_2);

  /* WPAD analog sticks (handled as PAD held direction keys) */
  x = WPAD_StickX(data, 0);
  y = WPAD_StickY(data, 0);
  if (x > ANALOG_SENSITIVITY)       hp |= PAD_BUTTON_RIGHT;
  else if (x < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_LEFT;
  else if (y > ANALOG_SENSITIVITY)  hp |= PAD_BUTTON_UP;
  else if (y < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_DOWN;
#endif

  /* check if any direction/selection key is being held or just being pressed/released */
#ifdef HW_RVL
  if (pp||pw) held_cnt = 0;
  else if (hp||hw) held_cnt++;
  else held_cnt = 0;
#else
  if (pp) held_cnt = 0;
  else if (hp) held_cnt++;
  else held_cnt = 0;
#endif

  /* initial delay (prevents triggering to start immediately) */
  if (held_cnt > HELD_DELAY)
  {
    /* key triggering */
    pp |= hp;
#ifdef HW_RVL
    pw |= hw;
#endif

    /* delay until next triggering (adjusts direction/selection update speed) */
    held_cnt -= HELD_SPEED;
  }

#ifdef HW_RVL
  /* Wiimote direction keys */
  WPAD_IR(0, &m_input.ir);
  if (m_input.ir.valid)
  {
    /* Wiimote is handled vertically */
    if (pw & WPAD_BUTTON_UP)         pp |= PAD_BUTTON_UP;
    else if (pw & WPAD_BUTTON_DOWN)  pp |= PAD_BUTTON_DOWN;
    else if (pw & WPAD_BUTTON_LEFT)  pp |= PAD_BUTTON_LEFT;
    else if (pw & WPAD_BUTTON_RIGHT) pp |= PAD_BUTTON_RIGHT;
  }
  else
  {
    /* Wiimote is handled horizontally */
    if (pw & WPAD_BUTTON_UP)         pp |= PAD_BUTTON_LEFT;
    else if (pw & WPAD_BUTTON_DOWN)  pp |= PAD_BUTTON_RIGHT;
    else if (pw & WPAD_BUTTON_LEFT)  pp |= PAD_BUTTON_DOWN;
    else if (pw & WPAD_BUTTON_RIGHT) pp |= PAD_BUTTON_UP;
  }

  /* Classic Controller direction keys */
  if (pw & WPAD_CLASSIC_BUTTON_UP)          pp |= PAD_BUTTON_UP;
  else if (pw & WPAD_CLASSIC_BUTTON_DOWN)   pp |= PAD_BUTTON_DOWN;
  else if (pw & WPAD_CLASSIC_BUTTON_LEFT)   pp |= PAD_BUTTON_LEFT;
  else if (pw & WPAD_CLASSIC_BUTTON_RIGHT)  pp |= PAD_BUTTON_RIGHT;

  /* WPAD buttons */
  if (pw & WPAD_BUTTON_MINUS)           pp |= PAD_TRIGGER_L;
  if (pw & WPAD_BUTTON_PLUS)            pp |= PAD_TRIGGER_R;
  if (pw & WPAD_BUTTON_A)               pp |= PAD_BUTTON_A;
  if (pw & WPAD_BUTTON_B)               pp |= PAD_BUTTON_B;
  if (pw & WPAD_BUTTON_2)               pp |= PAD_BUTTON_A;
  if (pw & WPAD_BUTTON_1)               pp |= PAD_BUTTON_B;
  if (pw & WPAD_BUTTON_HOME)            pp |= PAD_TRIGGER_Z;
  if (pw & WPAD_CLASSIC_BUTTON_FULL_L)  pp |= PAD_TRIGGER_L;
  if (pw & WPAD_CLASSIC_BUTTON_FULL_R)  pp |= PAD_TRIGGER_R;
  if (pw & WPAD_CLASSIC_BUTTON_A)       pp |= PAD_BUTTON_A;
  if (pw & WPAD_CLASSIC_BUTTON_B)       pp |= PAD_BUTTON_B;
  if (pw & WPAD_CLASSIC_BUTTON_HOME)    pp |= PAD_TRIGGER_Z;
#endif

  /* Update menu inputs */
  m_input.keys = pp;
}
