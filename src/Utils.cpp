/*
 * This file is part of ClanBomber;
 * you can get it at "http://www.nongnu.org/clanbomber".
 *
 * Copyright (C) 2009, 2010 Rene Lopez <rsl@members.fsf.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "Utils.h"
#include "SDL_image.h"
#include "Resources.h"
#include <locale.h>

#include "config.h"
#include "gettext.h"

extern SDL_Surface *primary;

void CB_BlitSurface(SDL_Surface *sSurface, int x, int y)
{
  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  SDL_BlitSurface(sSurface, NULL, primary, &rect);
}

/* DSTF_TOPLEFT
 * DSTF_TOPRIGHT
 * DSTL_CENTER
 * DSTF_TOPCENTER
 */

void CB_RenderText(TTF_Font *font, const std::wstring &text, int x, int y)
{
  //if(text.c_str() == NULL) {
  //  std::wcout << "Texto nullo" << endl;
  //  return;
  //}
  SDL_Color color = { 0xFF, 0x00, 0x00};//Default color for text is white
  char *utf8text = SDL_iconv_string("", "wchar_t", (char*) text.c_str(), (text.length() + 1) * sizeof(wchar_t));
  SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, utf8text, color);
  CB_BlitSurface(textSurface, x, y);
  SDL_FreeSurface(textSurface);
  SDL_free(utf8text);
}

void CB_RenderText(TTF_Font *font, const char *text, int x, int y)
{
  SDL_Color color = { 0xFF, 0x00, 0x00};//Default color for text is white
  //SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, color);
  SDL_Surface *textSurface = TTF_RenderUTF8_Solid(font, text, color);
  CB_BlitSurface(textSurface, x, y);
  SDL_FreeSurface(textSurface);
}

void CB_RenderText(TTF_Font *font, const std::string &text, int x, int y)
{
  CB_RenderText(font, text.c_str(), x, y);
}

void CB_RenderTextCenter(TTF_Font *font, const char *text, int x, int y)
{
  SDL_Color color = { 0x00, 0xFF, 0x00};//Default color for text is white
  //SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, color);
  SDL_Surface *textSurface = TTF_RenderUTF8_Solid(font, text, color);
  if(textSurface == NULL) //nedded beacause when empty string TTF_RenderText_Solid returns NULL
    return;
  x -= textSurface->w/2;
  CB_BlitSurface(textSurface, x, y);
  SDL_FreeSurface(textSurface);
}

void CB_RenderTextCenter(TTF_Font *font, const std::string &text, int x, int y)
{
  CB_RenderTextCenter(font, text.c_str(), x, y);
}

void CB_RenderTextRight(TTF_Font *font, const char *text, int x, int y)
{
  SDL_Color color = { 0x00, 0x00, 0xFF};//Default color for text is white
  //SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, color);
  SDL_Surface *textSurface = TTF_RenderUTF8_Solid(font, text, color);
  x -= textSurface->w;
  CB_BlitSurface(textSurface, x, y);
  SDL_FreeSurface(textSurface);
}

void CB_RenderTextRight(TTF_Font *font, const std::string &text, int x, int y)
{
  CB_RenderTextRight(font, text.c_str(), x, y);
}

void CB_FillScreen(Uint8 r, Uint8 g, Uint8 b)
{
  SDL_FillRect(primary, NULL, SDL_MapRGB(primary->format, r, g, b) );
}

void CB_FillRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b)
{
  SDL_Rect rect = {x, y, w, h};
  SDL_FillRect(primary, &rect, SDL_MapRGB(primary->format, r, g, b) );
}

void CB_FillRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
  SDL_Rect rect = {x, y, w, h};
  SDL_Surface *tmpSurface = SDL_CreateRGBSurface(SDL_SRCALPHA,
						 w,
						 h,
						 32,
						 0, 0, 0,
						 0);
  SDL_SetAlpha(tmpSurface, SDL_SRCALPHA, a);
  SDL_FillRect(tmpSurface, NULL, SDL_MapRGB(tmpSurface->format, r, g, b) );
  CB_BlitSurface(tmpSurface, x, y);
  SDL_FreeSurface(tmpSurface);
}

void CB_WaitForKeypress()
{
  SDL_Event event;
  do {
    SDL_WaitEvent(&event);
  }while(event.type != SDL_KEYDOWN);
}

void CB_BatchBlit(SDL_Surface *src, SDL_Rect *srcRects, SDL_Rect *destRects, int num)
{
  for(int i = 0; i < num; i++)
    SDL_BlitSurface(src, &srcRects[i], primary, &destRects[i]);
}

void CB_FillRects(SDL_Rect *rects, int num, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
  for(int i = 0; i < num; i++) {
    if (a == 0xFF) {
      CB_FillRect(rects[i].x, rects[i].y, rects[i].w, rects[i].h, r, g, b);
    }
    else {
      CB_FillRect(rects[i].x, rects[i].y, rects[i].w, rects[i].h, r, g, b, a);
    }
  }
}

int CB_EnterText(std::string &new_string)
{
  SDL_Event event;
  //std::string str = "intruso";
  std::string str = new_string;
  int cursor = str.length();
  int cursorx = 0;
  
  SDL_EnableUNICODE(1);
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
  
  while(SDL_WaitEvent(&event)) {
    if(event.type == SDL_KEYDOWN) {
      switch(event.key.keysym.sym) {
      case SDLK_RETURN:
	new_string = str;
	SDL_EnableUNICODE(0);
	SDL_EnableKeyRepeat(0, 0);
	return 1;
      case SDLK_ESCAPE:
	SDL_EnableUNICODE(0);
	SDL_EnableKeyRepeat(0, 0);
	return -1;
      case SDLK_BACKSPACE:
	if(cursor > 0) {
	  str = str.substr(0, cursor - 1)+ str.substr(cursor, str.length());
	  cursor--;
	}
	break;
      case SDLK_DELETE:
	if(cursor < str.length()) {
	  str = str.substr(0, cursor)+ str.substr(cursor + 1, str.length());
	}
	break;
      case SDLK_LEFT:
	if(cursor > 0)
	  cursor--;
	break;
      case SDLK_UP:
      case SDLK_HOME:
      case SDLK_PAGEUP:
	cursor = 0;
	break;
      case SDLK_DOWN:
      case SDLK_END:
      case SDLK_PAGEDOWN:
	cursor = str.length();
	break;
      case SDLK_RIGHT:
	if(cursor < str.length())
	  cursor++;
	break;
      default:
	if(event.key.keysym.mod & KMOD_CTRL) {
	  printf("Control --- %u\n", event.key.keysym.unicode);
	  switch(event.key.keysym.unicode) {
	  case 1: //^A Start of header
	    cursor = 0;
	    break;
	  case 2: //^B
	    if(cursor > 0)
	      cursor--;
	    break;
	  case 5: //^E Enquiry
	    cursor = str.length();
	    break;
	  case 6: //^F
	    if(cursor < str.length())
	      cursor++;
	  }
	  /*switch(event.key.keysym.sym) {
	  case SDLK_a:
	    cursor = 0;
	    break;
	  case SDLK_e:
	    cursor = str.length();
	  }*/
	}
	else if(event.key.keysym.unicode >= 32  &&
	   event.key.keysym.unicode != 127) { // 32=spc 127=delete
	  char temp[2] = {0,0};
	  printf("%s -- %d CHAR: %c\n", str.c_str(), cursor, event.key.keysym.unicode);
	  temp[0] = event.key.keysym.unicode;
	  //str += temp;
	  str = str.substr(0, cursor) + temp + str.substr(cursor, str.length()-cursor);
	  cursor++;
	}
	else
	  printf("Esto no es un caracter");
      }
    }
    CB_FillRect(0, 0, 800, 40, 70, 0 , 0);
    //TODO Add error return in Font.getSize
    /*if(Resources::Font_big()->getSize(str.substr(0, cursor), &cursorx, NULL)
       == -1) {
      printf("Error");
      }*/
    Resources::Font_big()->render(str, 0, 0, cbe::FontAlignment_0topleft);
    CB_FillRect(cursorx, 0, 2, Resources::Font_big()->getHeight(), 0xFF, 0xFF,
                0xFF);
    CB_Flip();
  }
  
  SDL_EnableUNICODE(0);
  SDL_EnableKeyRepeat(0, 0);

  return 0;
}

void CB_Locale()
{
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, CB_LOCALEDIR);
  textdomain(PACKAGE);
}

void CB_Flip()
{
  SDL_Flip(primary);
}
