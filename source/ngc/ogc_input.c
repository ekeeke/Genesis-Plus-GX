/******************************************************************************
 *
 *  SMS Plus GX - Sega Master System / GameGear Emulator
 *
 *  SMS Plus - Sega Master System / GameGear Emulator
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ***************************************************************************/

#include "shared.h"
#include "font.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

/* configurable keys */
#define KEY_BUTTONA 0   
#define KEY_BUTTONB 1
#define KEY_BUTTONC 2
#define KEY_START   3
#define KEY_MENU    4
#define KEY_BUTTONX 5  // 6-buttons only
#define KEY_BUTTONY 6  // 6-buttons only
#define KEY_BUTTONZ 7  // 6-buttons only

int ConfigRequested = 1;

static const char *keys_name[MAX_KEYS] =
{
  "Button A",
  "Button B",
  "Button C",
  "Button START ",
  "Menu",
  "Button X",
  "Button Y",
  "Button Z",
};

/* gamepad available buttons */
static const u16 pad_keys[8] =
{
  PAD_TRIGGER_Z,
  PAD_TRIGGER_R,
  PAD_TRIGGER_L,
  PAD_BUTTON_A,
  PAD_BUTTON_B,
  PAD_BUTTON_X,
  PAD_BUTTON_Y,
  PAD_BUTTON_START,
};

#ifdef HW_RVL

/* directional buttons mapping  */
#define PAD_UP    0   
#define PAD_DOWN  1
#define PAD_LEFT  2
#define PAD_RIGHT 3

static u32 wpad_dirmap[3][4] =
{
  {WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT, WPAD_BUTTON_UP, WPAD_BUTTON_DOWN},                                // WIIMOTE only
  {WPAD_BUTTON_UP, WPAD_BUTTON_DOWN, WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT},                                // WIIMOTE + NUNCHUK
  {WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN, WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT} // CLASSIC
};

/* wiimote/expansion available buttons */
static u32 wpad_keys[20] =
{
  WPAD_BUTTON_2,
  WPAD_BUTTON_1,
  WPAD_BUTTON_B,
  WPAD_BUTTON_A,
/*  WPAD_BUTTON_MINUS,  used for mode */
  WPAD_BUTTON_HOME,
  WPAD_BUTTON_PLUS,
  WPAD_NUNCHUK_BUTTON_Z,
  WPAD_NUNCHUK_BUTTON_C,
  WPAD_CLASSIC_BUTTON_ZR,
  WPAD_CLASSIC_BUTTON_X,
  WPAD_CLASSIC_BUTTON_A,
  WPAD_CLASSIC_BUTTON_Y,
  WPAD_CLASSIC_BUTTON_B,
  WPAD_CLASSIC_BUTTON_ZL,
  WPAD_CLASSIC_BUTTON_FULL_R,
  WPAD_CLASSIC_BUTTON_PLUS,
  WPAD_CLASSIC_BUTTON_HOME,
  WPAD_CLASSIC_BUTTON_MINUS,
  WPAD_CLASSIC_BUTTON_FULL_L,
};
#endif


/*******************************
  gamepad support
*******************************/
static void pad_config(int num, int padtype)
{
  int i,j,max;
  u16 p;
  u8 quit;
  char msg[30];

  u32 pad = PAD_ScanPads() & (1<<num);
  if (!pad)
  {
    sprintf(msg, "PAD #%d is not connected !", num+1);
    WaitPrompt(msg);
    return;
  }

  /* configure keys */
  max = (padtype == DEVICE_6BUTTON) ? MAX_KEYS : (MAX_KEYS - 3);
  for (i=0; i<max; i++)
  {
    /* remove any pending keys */
    while (PAD_ButtonsHeld(num))
    {
      VIDEO_WaitVSync();
      PAD_ScanPads();
    }

    ClearScreen();
    sprintf(msg,"Press key for %s",keys_name[i]);
    WriteCentre(254, msg);
    SetScreen();

    /* check buttons state */
    quit = 0;
    while (quit == 0)
    {
      VIDEO_WaitVSync();
      PAD_ScanPads();
      p = PAD_ButtonsDown(num);

      for (j=0; j<8; j++)
      {
        if (p & pad_keys[j])
        {
           config.pad_keymap[num][i] = pad_keys[j];
           quit = 1;
           j = 9;   /* exit loop */
        }
      }
    }
  }
}

static void pad_update(s8 num, u8 i)
{
  /* get PAD status */
  s8 x  = PAD_StickX (num);
  s8 y  = PAD_StickY (num);
  u16 p = PAD_ButtonsHeld(num);
  u8 sensitivity = 60;
 
  /* get current key config */
  u16 pad_keymap[MAX_KEYS];
  memcpy(pad_keymap, config.pad_keymap[num], MAX_KEYS * sizeof(u16));

  /* SOFTRESET */
  if ((p & PAD_TRIGGER_L) && (p & PAD_TRIGGER_Z))
  {
    set_softreset();
  }

  /* BUTTONS */
  if (p & pad_keymap[KEY_BUTTONA]) input.pad[i]  |= INPUT_A;
  if (p & pad_keymap[KEY_BUTTONB]) input.pad[i]  |= INPUT_B;
  if (p & pad_keymap[KEY_BUTTONC]) input.pad[i]  |= INPUT_C;
  if (p & pad_keymap[KEY_BUTTONX]) input.pad[i]  |= INPUT_X;
  if (p & pad_keymap[KEY_BUTTONY]) input.pad[i]  |= INPUT_Y;
  if (p & pad_keymap[KEY_BUTTONZ]) input.pad[i]  |= INPUT_Z;

  /* MODE/START */
  if ((p & PAD_BUTTON_START) && (p & PAD_TRIGGER_Z)) input.pad[i]  |= INPUT_MODE;
  else if (p & pad_keymap[KEY_START]) input.pad[i]  |= INPUT_START;

  /* MENU */
  if (p & pad_keymap[KEY_MENU])
  {
    ConfigRequested = 1;
  }

  /* LIGHTGUN screen position (x,y) */
  if (input.dev[i] == DEVICE_LIGHTGUN)
  {
    input.analog[i-4][0] += x / sensitivity;
    input.analog[i-4][1] += y / sensitivity;
    if (input.analog[i-4][0] < 0) input.analog[i-4][0] = 0;
    else if (input.analog[i-4][0] > bitmap.viewport.w) input.analog[i-4][0] = bitmap.viewport.w;
    if (input.analog[i-4][1] < 0) input.analog[i-4][1] = 0;
    else if (input.analog[i-4][1] > bitmap.viewport.h) input.analog[i-4][1] = bitmap.viewport.h;
  }

  /* PEN tablet position (x,y) */
  else if ((system_hw == SYSTEM_PICO) && (i == 0))
  {
    input.analog[0][0] += x / sensitivity;
    input.analog[0][1] += y / sensitivity;
    if (input.analog[0][0] < 0x17c) input.analog[0][0] = 0x17c;
    else if (input.analog[0][0] > 0x3c) input.analog[0][0] = 0x3c;
    if (input.analog[0][1] < 0x1fc) input.analog[0][1] = 0x1fc;
    else if (input.analog[0][1] > 0x3f3) input.analog[0][1] = 0x3f3;
  }

  /* MOUSE quantity of movement (-256,256) */
  else if (input.dev[i] == DEVICE_MOUSE)
  {
    input.analog[2][0] = x * 2;
    input.analog[2][1] = y * 2;
    if (config.invert_mouse) input.analog[2][1] = 0 - input.analog[2][1];
  }

  /* GAMEPAD directional buttons */
  else
  {
    if ((p & PAD_BUTTON_UP)         || (y >  sensitivity)) input.pad[i] |= INPUT_UP;
    else if ((p & PAD_BUTTON_DOWN)  || (y < -sensitivity)) input.pad[i] |= INPUT_DOWN;
    if ((p & PAD_BUTTON_LEFT)       || (x < -sensitivity)) input.pad[i] |= INPUT_LEFT;
    else if ((p & PAD_BUTTON_RIGHT) || (x >  sensitivity)) input.pad[i] |= INPUT_RIGHT;
  }
}

/*******************************
  wiimote support
*******************************/
#ifdef HW_RVL

#define PI 3.14159265f

static s8 WPAD_StickX(u8 chan,u8 right)
{
  float mag = 0.0;
  float ang = 0.0;
  WPADData *data = WPAD_Data(chan);

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
  double val = mag * sin(PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}


static s8 WPAD_StickY(u8 chan, u8 right)
{
  float mag = 0.0;
  float ang = 0.0;
  WPADData *data = WPAD_Data(chan);

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
  double val = mag * cos(PI * ang/180.0f);
 
  return (s8)(val * 128.0f);
}

static void wpad_config(u8 num, u8 exp, u8 padtype)
{
  int i,j,max;
  u8 quit;
  char msg[30];
  u32 current = 255;

  /* check wiimote status */
  WPAD_Probe(num, &current);
  if (((exp > WPAD_EXP_NONE) && (current != exp)) || (current == 255))
  {
    if (exp == WPAD_EXP_NONE)     sprintf(msg, "WIIMOTE #%d is not connected !", num+1);
    if (exp == WPAD_EXP_NUNCHUK)  sprintf(msg, "NUNCHUK #%d is not connected !", num+1);
    if (exp == WPAD_EXP_CLASSIC)  sprintf(msg, "CLASSIC #%d is not connected !", num+1);
    WaitPrompt(msg);
    return;
  }

  /* index for wpad_keymap */
  u8 index = exp + (num * 3);

  /* loop on each mapped keys */
  max = (padtype == DEVICE_6BUTTON) ? MAX_KEYS : (MAX_KEYS - 3);
  for (i=0; i<max; i++)
  {
    /* remove any pending buttons */
    while (WPAD_ButtonsHeld(num))
    {
      WPAD_ScanPads();
      VIDEO_WaitVSync();
    }

    /* user information */
    ClearScreen();
    sprintf(msg,"Press key for %s",keys_name[i]);
    WriteCentre(254, msg);
    SetScreen();

    /* wait for input */
    quit = 0;
    while (quit == 0)
    {
      WPAD_ScanPads();

      /* get buttons */
      for (j=0; j<20; j++)
      {
        if (WPAD_ButtonsDown(num) & wpad_keys[j])
        {
          config.wpad_keymap[index][i]  = wpad_keys[j];
          quit = 1;
          j = 20;    /* leave loop */
        }
      }
    } /* wait for input */ 
  } /* loop for all keys */

  /* removed any pending buttons */
  while (WPAD_ButtonsHeld(num))
  {
    WPAD_ScanPads();
    VIDEO_WaitVSync();
  }
}

static void wpad_update(s8 num, u8 i, u32 exp)
{
  /* get buttons status */
  u32 p = WPAD_ButtonsHeld(num);

  /* get analog sticks values */
  u8 sensitivity = 60;
  s8 x = 0;
  s8 y = 0;
  if (exp != WPAD_EXP_NONE)
  {
    x = WPAD_StickX(num,0);
    y = WPAD_StickY(num,0);
  }

  /* retrieve current key mapping */
  u32 *wpad_keymap = config.wpad_keymap[exp + (3 * num)];

  /* BUTTONS */
  if (p & wpad_keymap[KEY_BUTTONA]) input.pad[i]  |= INPUT_A;
  if (p & wpad_keymap[KEY_BUTTONB]) input.pad[i]  |= INPUT_B;
  if (p & wpad_keymap[KEY_BUTTONC]) input.pad[i]  |= INPUT_C;
  if (p & wpad_keymap[KEY_BUTTONX]) input.pad[i]  |= INPUT_X;
  if (p & wpad_keymap[KEY_BUTTONY]) input.pad[i]  |= INPUT_Y;
  if (p & wpad_keymap[KEY_BUTTONZ]) input.pad[i]  |= INPUT_Z;
  if (p & wpad_keymap[KEY_START])   input.pad[i]  |= INPUT_START;

  /* MODE Button (FIXED) */
  if (((exp == WPAD_EXP_CLASSIC) && (p & WPAD_CLASSIC_BUTTON_MINUS)) ||
      ((exp != WPAD_EXP_CLASSIC) && (p & WPAD_BUTTON_MINUS)))
    input.pad[i]  |= INPUT_MODE;

  /* LIGHTGUN screen position (X,Y) */
  if (input.dev[i] == DEVICE_LIGHTGUN)
  {
    if (x || y)
    {
      /* analog stick */
      input.analog[i-4][0] += x / sensitivity;
      input.analog[i-4][1] += y / sensitivity;
      if (input.analog[i-4][0] < 0) input.analog[i-4][0] = 0;
      else if (input.analog[i-4][0] > bitmap.viewport.w) input.analog[i-4][0] = bitmap.viewport.w;
      if (input.analog[i-4][1] < 0) input.analog[i-4][1] = 0;
      else if (input.analog[i-4][1] > bitmap.viewport.h) input.analog[i-4][1] = bitmap.viewport.h;
    }

    if (exp != WPAD_EXP_CLASSIC)
    {
      /* wiimote IR */
      struct ir_t ir;
      WPAD_IR(num, &ir);
      if (ir.valid)
      {
        input.analog[i-4][0] = (ir.x * bitmap.viewport.w) / 640;
        input.analog[i-4][1] = (ir.y * bitmap.viewport.h) / 480;
      }
    }
  }

  /* PEN tablet position (x,y) */
  else if ((system_hw == SYSTEM_PICO) && (i == 0))
  {
    if (x || y)
    {
      /* analog stick */
      input.analog[0][0] += x / sensitivity;
      input.analog[0][1] += y / sensitivity;
      if (input.analog[0][0] < 0x17c) input.analog[0][0] = 0x17c;
      else if (input.analog[0][0] > 0x3c) input.analog[0][0] = 0x3c;
      if (input.analog[0][1] < 0x1fc) input.analog[0][1] = 0x1fc;
      else if (input.analog[0][1] > 0x3f3) input.analog[0][1] = 0x3f3;
    }

    if (exp != WPAD_EXP_CLASSIC)
    {
      /* wiimote IR */
      struct ir_t ir;
      WPAD_IR(num, &ir);
      if (ir.valid)
      {
        input.analog[0][0] = 0x3c  + (ir.x * (0x17c - 0x3c  + 1)) / 640;
        input.analog[0][1] = 0x1fc + (ir.y * (0x3f3 - 0x1fc + 1)) / 480;
      }
    }
  }

  /* MOUSE quantity of movement (-256,256) */
  else if (input.dev[i] == DEVICE_MOUSE)
  {
    /* analog stick */
    input.analog[2][0] = x * 2;
    input.analog[2][1] = y * 2;
    if (config.invert_mouse) input.analog[2][1] = 0 - input.analog[2][1];
  }

  /* GAMEPAD directional buttons */
  else
  {
    if ((p & wpad_dirmap[exp][PAD_UP])          || (y >  sensitivity)) input.pad[i] |= INPUT_UP;
    else if ((p & wpad_dirmap[exp][PAD_DOWN])   || (y < -sensitivity)) input.pad[i] |= INPUT_DOWN;
    if ((p & wpad_dirmap[exp][PAD_LEFT])        || (x < -sensitivity)) input.pad[i] |= INPUT_LEFT;
    else if ((p & wpad_dirmap[exp][PAD_RIGHT])  || (x >  sensitivity)) input.pad[i] |= INPUT_RIGHT;
  }

  /* SOFTRESET */
  if (((p & WPAD_CLASSIC_BUTTON_PLUS) && (p & WPAD_CLASSIC_BUTTON_MINUS)) ||
      ((p & WPAD_BUTTON_PLUS) && (p & WPAD_BUTTON_MINUS)))
  {
    set_softreset();
  }

  /* MENU */
  if (p & wpad_keymap[KEY_MENU])
  {
    ConfigRequested = 1;
  }
}
#endif

/*****************************************************************
                Generic input handlers 
******************************************************************/

void ogc_input__init(void)
{
  PAD_Init ();

#ifdef HW_RVL
  WPAD_Init();
	WPAD_SetIdleTimeout(60);
  WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
  WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
#endif
}

void ogc_input__set_defaults(void)
{
  int i;

  /* set default key mapping for each type of devices */
  for (i=0; i<4; i++)
  {
    config.pad_keymap[i][KEY_BUTTONA] = PAD_BUTTON_B;
    config.pad_keymap[i][KEY_BUTTONB] = PAD_BUTTON_A;
    config.pad_keymap[i][KEY_BUTTONC] = PAD_BUTTON_X;
    config.pad_keymap[i][KEY_START]   = PAD_BUTTON_START;
    config.pad_keymap[i][KEY_MENU]    = PAD_TRIGGER_Z;
    config.pad_keymap[i][KEY_BUTTONX] = PAD_TRIGGER_L;
    config.pad_keymap[i][KEY_BUTTONY] = PAD_BUTTON_Y;
    config.pad_keymap[i][KEY_BUTTONZ] = PAD_TRIGGER_R;
  }

#ifdef HW_RVL
  for (i=0; i<4; i++)
  {
    /* WIIMOTE */
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONA] = WPAD_BUTTON_A;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONB] = WPAD_BUTTON_2;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONC] = WPAD_BUTTON_1;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_START]   = WPAD_BUTTON_PLUS;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_MENU]    = WPAD_BUTTON_HOME;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONX] = 0;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONY] = 0;
    config.wpad_keymap[i*3 + WPAD_EXP_NONE][KEY_BUTTONZ] = 0;

    /* WIIMOTE + NUNCHUK */
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONA] = WPAD_NUNCHUK_BUTTON_Z;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONB] = WPAD_BUTTON_A;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONC] = WPAD_BUTTON_B;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_START]   = WPAD_BUTTON_PLUS;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_MENU]    = WPAD_BUTTON_HOME;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONX] = WPAD_NUNCHUK_BUTTON_C;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONY] = WPAD_BUTTON_1;
    config.wpad_keymap[i*3 + WPAD_EXP_NUNCHUK][KEY_BUTTONZ] = WPAD_BUTTON_2;

    /* CLASSIC CONTROLLER */
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONA] = WPAD_CLASSIC_BUTTON_Y;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONB] = WPAD_CLASSIC_BUTTON_B;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONC] = WPAD_CLASSIC_BUTTON_A;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_START]   = WPAD_CLASSIC_BUTTON_PLUS;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_MENU]    = WPAD_CLASSIC_BUTTON_HOME;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONX] = WPAD_CLASSIC_BUTTON_ZL;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONY] = WPAD_CLASSIC_BUTTON_X;
    config.wpad_keymap[i*3 + WPAD_EXP_CLASSIC][KEY_BUTTONZ] = WPAD_CLASSIC_BUTTON_ZR;
  }
#endif

 /* set default device assigantion */
 for (i=0; i<MAX_DEVICES; i++)
 {
#ifdef HW_RVL
    config.input[i].device = (i < 4) ? 1 : 0;
#else
    config.input[i].device = (i < 4) ? 0 : -1;
#endif
    config.input[i].port = i % 4;
  }
}

void ogc_input__update(void)
{
  int i;
  int num = 0;

  /* update inputs */
  PAD_ScanPads();

#ifdef HW_RVL
  WPAD_ScanPads();
  if (WPAD_ButtonsHeld(0) & WPAD_BUTTON_HOME)
  {
    /* do additional check here to prevent bad controller configuration */
    ConfigRequested = 1;
    return;
  }
#endif

  for (i=0; i<MAX_DEVICES; i++)
  {
    input.pad[i] = 0;

    if (input.dev[i] != NO_DEVICE)
    {
      if (config.input[num].device == 0)
        pad_update(config.input[num].port, i);

#ifdef HW_RVL
      else if (config.input[num].device > 0)
        wpad_update(config.input[num].port,i, config.input[num].device - 1);
#endif
      num ++;
    }
  }
}

void ogc_input__config(u8 num, u8 type, u8 padtype)
{
  switch (type)
  {
    case 0:
      pad_config(num, padtype);
      break;
    
    default:
#ifdef HW_RVL
      wpad_config(num,type-1, padtype);
#endif
      break;
  }
}

u16 ogc_input__getMenuButtons(void)
{
  /* gamecube pad */
  PAD_ScanPads();
  u16 p = PAD_ButtonsDown(0);
  s8 x  = PAD_StickX(0);
  s8 y  = PAD_StickY(0);
  if (x > 70) p |= PAD_BUTTON_RIGHT;
  else if (x < -70) p |= PAD_BUTTON_LEFT;
	if (y > 60) p |= PAD_BUTTON_UP;
  else if (y < -60) p |= PAD_BUTTON_DOWN;

#ifdef HW_RVL
  /* wiimote support */
  struct ir_t ir;
  u32 exp;

  WPAD_ScanPads();
  if (WPAD_Probe(0, &exp) == WPAD_ERR_NONE)
  {
    u32 q = WPAD_ButtonsDown(0);
    x = WPAD_StickX(0, 0);
    y = WPAD_StickY(0, 0);

    /* default directions */
    WPAD_IR(0, &ir);
    if (ir.valid)
    {
      /* Wiimote is pointed toward screen */
      if ((q & WPAD_BUTTON_UP) || (y > 70))         p |= PAD_BUTTON_UP;
      else if ((q & WPAD_BUTTON_DOWN) || (y < -70)) p |= PAD_BUTTON_DOWN;
      if ((q & WPAD_BUTTON_LEFT) || (x < -60))      p |= PAD_BUTTON_LEFT;
      else if ((q & WPAD_BUTTON_RIGHT) || (x > 60)) p |= PAD_BUTTON_RIGHT;
    }
    else
    {
      /* Wiimote is used horizontally */
      if ((q & WPAD_BUTTON_RIGHT) || (y > 70))         p |= PAD_BUTTON_UP;
      else if ((q & WPAD_BUTTON_LEFT) || (y < -70)) p |= PAD_BUTTON_DOWN;
      if ((q & WPAD_BUTTON_UP) || (x < -60))      p |= PAD_BUTTON_LEFT;
      else if ((q & WPAD_BUTTON_DOWN) || (x > 60)) p |= PAD_BUTTON_RIGHT;
    }

    /* default keys */
    if (q & WPAD_BUTTON_MINUS)  p |= PAD_TRIGGER_L;
    if (q & WPAD_BUTTON_PLUS)   p |= PAD_TRIGGER_R;
    if (q & WPAD_BUTTON_A)      p |= PAD_BUTTON_A;
    if (q & WPAD_BUTTON_B)      p |= PAD_BUTTON_B;
    if (q & WPAD_BUTTON_2)      p |= PAD_BUTTON_A;
    if (q & WPAD_BUTTON_1)      p |= PAD_BUTTON_B;
    if (q & WPAD_BUTTON_HOME)   p |= PAD_TRIGGER_Z;

    /* classic controller expansion */
    if (exp == WPAD_EXP_CLASSIC)
    {
      if (q & WPAD_CLASSIC_BUTTON_UP)         p |= PAD_BUTTON_UP;
      else if (q & WPAD_CLASSIC_BUTTON_DOWN)  p |= PAD_BUTTON_DOWN;
      if (q & WPAD_CLASSIC_BUTTON_LEFT)       p |= PAD_BUTTON_LEFT;
      else if (q & WPAD_CLASSIC_BUTTON_RIGHT) p |= PAD_BUTTON_RIGHT;

      if (q & WPAD_CLASSIC_BUTTON_FULL_L) p |= PAD_TRIGGER_L;
      if (q & WPAD_CLASSIC_BUTTON_FULL_R) p |= PAD_TRIGGER_R;
      if (q & WPAD_CLASSIC_BUTTON_A)      p |= PAD_BUTTON_A;
      if (q & WPAD_CLASSIC_BUTTON_B)      p |= PAD_BUTTON_B;
      if (q & WPAD_CLASSIC_BUTTON_HOME)   p |= PAD_TRIGGER_Z;
    }
  }
#endif

  return p;
}
