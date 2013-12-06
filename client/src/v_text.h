// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1998-2006 by Randy Heit (ZDoom 1.22).
// Copyright (C) 2006-2013 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	V_TEXT
//
//-----------------------------------------------------------------------------


#ifndef __V_TEXT_H__
#define __V_TEXT_H__

#include "doomtype.h"
#include "v_font.h"

struct brokenlines_s {
	int width;
	char *string;
};
typedef struct brokenlines_s brokenlines_t;

enum EColorRange
{
	CR_BRICK,
	CR_TAN,
	CR_GRAY,
	CR_GREY = CR_GRAY,
	CR_GREEN,
	CR_BROWN,
	CR_GOLD,
	CR_RED,
	CR_BLUE,
	CR_ORANGE,
	CR_WHITE,
	CR_YELLOW,

	// [AM] Extended ZDoom colors.  Not all of these actually work yet.
	CR_UNTRANSLATED,
	CR_BLACK,
	CR_LIGHTBLUE,
	CR_CREAM,
	CR_OLIVE,
	CR_DARKGREEN,
	CR_DARKRED,
	CR_DARKBROWN,
	CR_PURPLE,
	CR_DARKGRAY,
	CR_DARKGREY = CR_DARKGRAY,
	CR_CYAN,
	NUM_TEXT_COLORS
};

#define TEXTCOLOR_ESCAPE	'\x8a'

#define TEXTCOLOR_BRICK		"\x8aA"
#define TEXTCOLOR_TAN		"\x8aB"
#define TEXTCOLOR_GRAY		"\x8aC"
#define TEXTCOLOR_GREY		"\x8aC"
#define TEXTCOLOR_GREEN		"\x8aD"
#define TEXTCOLOR_BROWN		"\x8aE"
#define TEXTCOLOR_GOLD		"\x8aF"
#define TEXTCOLOR_RED		"\x8aG"
#define TEXTCOLOR_BLUE		"\x8aH"
#define TEXTCOLOR_ORANGE	"\x8aI"
#define TEXTCOLOR_WHITE		"\x8aJ"
#define TEXTCOLOR_YELLOW	"\x8aK"

// [AM] Extended ZDoom colors.  Not all of these actually work yet.
#define TEXTCOLOR_UNTRANSLATED	"\x8aL"
#define TEXTCOLOR_BLACK		"\x8aM"
#define TEXTCOLOR_LIGHTBLUE	"\x8aN"
#define TEXTCOLOR_CREAM		"\x8aO"
#define TEXTCOLOR_OLIVE		"\x8aP"
#define TEXTCOLOR_DARKGREEN	"\x8aQ"
#define TEXTCOLOR_DARKRED	"\x8aR"
#define TEXTCOLOR_DARKBROWN	"\x8aS"
#define TEXTCOLOR_PURPLE	"\x8aT"
#define TEXTCOLOR_DARKGRAY	"\x8aU"
#define TEXTCOLOR_DARKGREY	"\x8aU"
#define TEXTCOLOR_CYAN		"\x8aV"

#define TEXTCOLOR_NORMAL	"\x8a-"
#define TEXTCOLOR_BOLD		"\x8a+"

brokenlines_t *V_BreakLines(const OFont* font, int maxwidth, const byte *str);
inline brokenlines_t *V_BreakLines(const OFont* font, int maxwidth, const char *str)
{
	return V_BreakLines(font, maxwidth, (const byte *)str);
}

void V_FreeBrokenLines(brokenlines_t *lines);

void V_LoadConsoleFont();
void V_UnloadConsoleFont();
void V_LoadHudFont();
void V_UnloadHudFont();
void V_LoadMenuFont();
void V_UnloadMenuFont();
void V_LoadDoomFont();
void V_UnloadDoomFont();

void V_LoadFonts();
void V_UnloadFonts();
bool V_FontsLoaded();

extern OFont* console_font;
extern OFont* hud_font;
extern OFont* menu_font;
extern OFont* doom_font;

#endif //__V_TEXT_H__

