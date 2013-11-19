// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
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


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "v_text.h"
#include "v_font.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_swap.h"

#include "doomstat.h"

EXTERN_CVAR(hud_scaletext)
EXTERN_CVAR(con_scaletext)

OFont* console_font;
OFont* hud_font;
OFont* menu_font;
OFont* doom_font;

extern byte *Ranges;

void V_UnloadConsoleFont()
{
	delete console_font;
	console_font = NULL;
};

void V_LoadConsoleFont()
{
	if (console_font)
		V_UnloadConsoleFont();
	console_font = new ConCharsFont(con_scaletext * FRACUNIT);
//	console_font = new TrueTypeFont("FONT_SM", hud_scaletext * 8);
}

void V_UnloadHudFont()
{
	delete hud_font;
	hud_font = NULL;
}

void V_LoadHudFont()
{
	if (hud_font)
		V_UnloadHudFont();
	hud_font = new SmallDoomFont(hud_scaletext * FRACUNIT);

	unsigned int ttf_stylemask =
			TrueTypeFont::TTF_TEXTURE |
			TrueTypeFont::TTF_OUTLINE;

//	hud_font = new TrueTypeFont("FONT_SM", hud_scaletext * 8, ttf_stylemask);
}

void V_UnloadMenuFont()
{
	delete menu_font;
	menu_font = NULL;
}

void V_LoadMenuFont()
{
	if (menu_font)
		V_UnloadMenuFont();
//	menu_font = new SmallDoomFont(MIN(CleanXfac, CleanYfac) * FRACUNIT);

	unsigned int ttf_stylemask =
			TrueTypeFont::TTF_TEXTURE |
			TrueTypeFont::TTF_OUTLINE;
	menu_font = new TrueTypeFont("FONT_SM", MIN(CleanXfac, CleanYfac) * 8, ttf_stylemask);
}

void V_UnloadDoomFont()
{
	delete doom_font;
	doom_font = NULL;
}

void V_LoadDoomFont()
{
	if (doom_font)
		V_UnloadDoomFont();
	doom_font = new SmallDoomFont(MIN(CleanXfac, CleanYfac) * FRACUNIT);
}

extern "C" void STACK_ARGS PrintChar1P (DWORD *charimg, byte *dest, int screenpitch);
extern "C" void STACK_ARGS PrintChar2P_MMX (DWORD *charimg, byte *dest, int screenpitch);

void DCanvas::TextWrapper(EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const
{
	if (normalcolor > NUM_TEXT_COLORS)
		normalcolor = CR_RED;
	int boldcolor = normalcolor ? normalcolor - 1 : NUM_TEXT_COLORS - 1;

	V_ColorMap = translationref_t(Ranges + normalcolor * 256);

	hud_font->printText(this, x, y, normalcolor, (const char*)string);
}

void DCanvas::TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper(drawer, normalcolor, x, y, string, CleanXfac, CleanYfac);
}

void DCanvas::TextSWrapper(EWrapperCode drawer, int normalcolor, int x, int y, 
							const byte *string, int scalex, int scaley) const
{ 
	if (normalcolor > NUM_TEXT_COLORS)
		normalcolor = CR_RED;
	int boldcolor = normalcolor ? normalcolor - 1 : NUM_TEXT_COLORS - 1;

	V_ColorMap = translationref_t(Ranges + normalcolor * 256);

	hud_font->printText(this, x, y, normalcolor, (const char*)string);
}

//
// Break long lines of text into multiple lines no longer than maxwidth pixels
//
static void breakit(brokenlines_t *line, const byte *start, const byte *string, const OFont* font)
{
	// Leave out trailing white space
	while (string > start && isspace (*(string - 1)))
		string--;

	line->string = new char[string - start + 1];
	strncpy (line->string, (char *)start, string - start);
	line->string[string - start] = 0;
	line->width = font->getTextWidth((const char*)line->string);
}

brokenlines_t *V_BreakLines(const OFont* font, int maxwidth, const byte *string)
{
	brokenlines_t lines[128];	// Support up to 128 lines (should be plenty)

	const byte *space = NULL, *start = string;
	int i, c, w, nw;
	BOOL lastWasSpace = false;

	i = w = 0;

	while ( (c = *string++) )
	{
		if (c == 0x8a)
		{
			if (*string)
				string++;
			continue;
		}

		if (isspace(c))
		{
			if (!lastWasSpace)
			{
				space = string - 1;
				lastWasSpace = true;
			}
		}
		else
			lastWasSpace = false;

		nw = hud_font->getTextWidth(c);
		
		// Time to break the line
		if (w + nw > maxwidth || c == '\n' - HU_FONTSTART)
		{	
			if (!space)
				space = string - 1;

			breakit(&lines[i], start, space, font);

			i++;
			w = 0;
			lastWasSpace = false;
			start = space;
			space = NULL;

			while (*start && isspace (*start) && *start != '\n')
				start++;

			if (*start == '\n')
				start++;
			else
			{
				while (*start && isspace (*start))
					start++;
			}

			string = start;
		}
		else
		{
			w += nw;
		}
	}

	if (string - start > 1)
	{
		const byte *s = start;

		while (s < string)
		{
			if (!isspace (*s++))
			{
				breakit(&lines[i++], start, string, font);
				break;
			}
		}
	}

	{
		// Make a copy of the broken lines and return them
		brokenlines_t *broken = new brokenlines_t[i+1];

		memcpy (broken, lines, sizeof(brokenlines_t) * i);
		broken[i].string = NULL;
		broken[i].width = -1;

		return broken;
	}
}

void V_FreeBrokenLines (brokenlines_t *lines)
{
	if (lines)
	{
		int i = 0;

		while (lines[i].width != -1)
		{
			delete[] lines[i].string;
			lines[i].string = NULL;
			i++;
		}
		delete[] lines;
	}
}


VERSION_CONTROL (v_text_cpp, "$Id$")

