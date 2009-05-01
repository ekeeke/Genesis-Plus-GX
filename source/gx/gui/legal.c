/****************************************************************************
 *  legal.c
 *
 *  legal informations screen
 *
 *  code by Softdev (2006), Eke-Eke (2007,2008)
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
#include "menu.h"

#include "intro_pcm.h"
#include <asndlib.h>

/* 
 * This is the legal stuff - which must be shown at program startup 
 * Any derivative work MUST include the same textual output.
 *
 * In other words, play nice and give credit where it's due.
 */
void legal ()
{
  int ypos = 64;
  gx_texture *texture;

  gxClearScreen((GXColor)BLACK);

  WriteCentre (ypos, "Genesis Plus Sega Mega Drive Emulator (v1.2a)");
  ypos += fheight;
  WriteCentre (ypos, "(C) 1999 - 2003 Charles MacDonald");
  ypos += fheight;
  WriteCentre (ypos, "This is free software, and you are welcome to");
  ypos += fheight;
  WriteCentre (ypos, "redistribute it under the conditions of the");
  ypos += fheight;
  WriteCentre (ypos, "GNU GENERAL PUBLIC LICENSE Version 2");
  ypos += fheight;
  WriteCentre (ypos, "Additionally, the developers of this port");
  ypos += fheight;
  WriteCentre (ypos, "disclaims all copyright interests in the ");
  ypos += fheight;
  WriteCentre (ypos, "Nintendo Gamecube Porting code.");
  ypos += fheight;
  WriteCentre (ypos, "You are free to use it as you wish.");
  ypos += 2*fheight;

  texture= gxTextureOpenPNG(Bg_intro_c4_png,0);
  if (texture)
  {
    gxDrawTexture(texture, (640-texture->width)/2, ypos, texture->width, texture->height,255);
    ypos += texture->height + 2 * fheight;
    if (texture->data) free(texture->data);
    free(texture);
  }

  gxSetScreen ();
  sleep (1);
  GXColor color = {0x66,0x99,0xcc,0xff};
  FONT_writeCenter("Press any button to skip intro",fheight,0,640,ypos,color);
  gxSetScreen ();
  int count = 100;
  while (count > 0)
  {
    count--;
    VIDEO_WaitVSync();
    if (m_input.keys) return;
  }

  gxClearScreen((GXColor)BLACK);
  texture = gxTextureOpenPNG(Bg_intro_c1_png,0);
  if (texture)
  {
    gxDrawTexture(texture, (640-texture->width)/2, (480-texture->height)/2,  texture->width, texture->height,255);
    if (texture->data) free(texture->data);
    free(texture);
  }

  gxSetScreen ();
  sleep (1);

  gxClearScreen((GXColor)WHITE);
  texture = gxTextureOpenPNG(Bg_intro_c2_png,0);
  if (texture)
  {
    gxDrawTexture(texture, (640-texture->width)/2, (480-texture->height)/2,  texture->width, texture->height,255);
    if (texture->data) free(texture->data);
    free(texture);
  }

  gxSetScreen ();
  sleep (1);

  gxClearScreen((GXColor)BLACK);
  texture = gxTextureOpenPNG(Bg_intro_c3_png,0);
  if (texture)
  {
    gxDrawTexture(texture, (640-texture->width)/2, (480-texture->height)/2,  texture->width, texture->height,255);
    if (texture->data) free(texture->data);
    free(texture);
  }

  gxSetScreen ();
  ASND_Init();
  ASND_Pause(0);
  int voice = ASND_GetFirstUnusedVoice();
  ASND_SetVoice(voice,VOICE_MONO_16BIT,44100,0,(u8 *)intro_pcm,intro_pcm_size,200,200,NULL);
  sleep (2);
  ASND_Pause(1);
  ASND_End();
}
