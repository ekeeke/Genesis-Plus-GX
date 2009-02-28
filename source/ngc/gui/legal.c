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
#include "Background_intro_c1.h"
#include "Background_intro_c2.h"
#include "Background_intro_c3.h"
#include "Background_intro_c4.h"

/* 
 * This is the legal stuff - which must be shown at program startup 
 * Any derivative work MUST include the same textual output.
 *
 * In other words, play nice and give credit where it's due.
 */
void legal ()
{
  int ypos = 64;
  png_texture texture;

  ClearScreen((GXColor)BLACK);
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

  texture.data   = 0;
  texture.width  = 0;
  texture.height = 0;
  texture.format = 0;
  OpenPNGFromMemory(&texture, Background_intro_c4);
  DrawTexture(&texture, (640-texture.width)/2, ypos, texture.width, texture.height);
  ypos += texture.height + 2 * fheight;

  //WriteCentre (ypos, "Press A to continue");
  SetScreen ();
  //WaitButtonA ();
  sleep (2);


  ClearScreen((GXColor)BLACK);
  texture.data   = 0;
  texture.width  = 0;
  texture.height = 0;
  texture.format = 0;
  OpenPNGFromMemory(&texture, Background_intro_c1);
  DrawTexture(&texture, (640-texture.width)/2, (480-texture.height)/2,  texture.width, texture.height);
  SetScreen ();
  sleep (1);

  ClearScreen((GXColor)WHITE);
  texture.data   = 0;
  texture.width  = 0;
  texture.height = 0;
  texture.format = 0;
  OpenPNGFromMemory(&texture, Background_intro_c2);
  DrawTexture(&texture, (640-texture.width)/2, (480-texture.height)/2,  texture.width, texture.height);
  SetScreen ();
  sleep (1);

  ClearScreen((GXColor)BLACK);
  texture.data   = 0;
  texture.width  = 0;
  texture.height = 0;
  texture.format = 0;
  OpenPNGFromMemory(&texture, Background_intro_c3);
  DrawTexture(&texture, (640-texture.width)/2, (480-texture.height)/2,  texture.width, texture.height);
  SetScreen ();
  sleep (2);

}
