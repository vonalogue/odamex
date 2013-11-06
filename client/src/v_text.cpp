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

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_swap.h"

#include "doomstat.h"

EXTERN_CVAR(hud_scaletext)

static Texture* console_font[256];

extern patch_t *hu_font[HU_FONTSIZE];

extern byte *Ranges;

int V_TextScaleXAmount()
{
	if (hud_scaletext < 1.0f)
		return 1;

	return int(hud_scaletext);
}

int V_TextScaleYAmount()
{
	if (hud_scaletext < 1.0f)
		return 1;

	return int(hud_scaletext);
}

//
// V_LoadConsoleFont
//
// Reads the CONCHARS lump from disk and converts it into an array with a
// separate Texture for each character in the font.
//
void V_LoadConsoleFont()
{
	static const char lumpname[] = "CONCHARS";
	texhandle_t conchars_handle = texturemanager.getHandle(lumpname, Texture::TEX_PATCH);
	if (conchars_handle == TextureManager::NOT_FOUND_TEXTURE_HANDLE)
		I_Error("Unable to load %s lump.", lumpname);
	const Texture* conchars_texture = texturemanager.getTexture(conchars_handle);

	const int charwidth = 8, charheight = 8;
	const int numcolumns = conchars_texture->getWidth() / charwidth;
	const int numrows = conchars_texture->getHeight() / charheight;

	for (int row = 0; row < numrows; row++)
	{
		for (int column = 0; column < numcolumns; column++)
		{	
			int charnum = row * numrows + column;
			texhandle_t texhandle = texturemanager.createSpecialUseHandle();			
			Texture* texture = texturemanager.createTexture(texhandle, charwidth, charheight);
			Z_ChangeTag(texture, PU_STATIC);	// don't allow texture to be purged from cache
			console_font[charnum] = texture;

			R_CopySubimage(texture, conchars_texture, column * charwidth, row * charheight, charwidth, charheight);
		}
	}
}

//
// V_PrintStr
// Print a line of text using the console font
//

extern "C" void STACK_ARGS PrintChar1P (DWORD *charimg, byte *dest, int screenpitch);
extern "C" void STACK_ARGS PrintChar2P_MMX (DWORD *charimg, byte *dest, int screenpitch);

static void V_PrintCharacter(const DCanvas* canvas, int& x, int& y, char c, fixed_t scale)
{
	byte charnum = (byte)c;
	if (charnum == '\t')
		charnum = ' ';

	const Texture* texture = console_font[charnum];

	int charwidth = (texture->getWidth() * scale) >> FRACBITS;
	int charheight = (texture->getHeight() * scale) >> FRACBITS;

	if (c == '\t')
	{
		// convert tabs into 4 spaces
		for (int i = 0; i < 4; i++)
		{
			canvas->DrawTextureStretched(texture, x, y, charwidth, charheight);
			x += charwidth;
		}
	}
	else
	{
		canvas->DrawTextureStretched(texture, x, y, charwidth, charheight);
		x += charwidth;
	}
}


void DCanvas::PrintStr(int x, int y, const char *s, int count) const
{
	if (!buffer)
		return;

	while (*s != 0 && count--)
	{
		V_PrintCharacter(this, x, y, *s, FRACUNIT);
		s++;
	}
}

//
// V_PrintStr2
// Same as V_PrintStr but doubles the size of every character.
//
void DCanvas::PrintStr2(int x, int y, const char *s, int count) const
{
	if (!buffer)
		return;

	while (*s != 0 && count--)
	{
		V_PrintCharacter(this, x, y, *s, 2 * FRACUNIT);
		s++;
	}
}

//
// V_DrawText
//
// Write a string using the hu_font
//

void DCanvas::TextWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const
{
	int 		w;
	const byte *ch;
	int 		c;
	int 		cx;
	int 		cy;
	int			boldcolor;

	if (normalcolor > NUM_TEXT_COLORS)
		normalcolor = CR_RED;
	boldcolor = normalcolor ? normalcolor - 1 : NUM_TEXT_COLORS - 1;

	V_ColorMap = translationref_t(Ranges + normalcolor * 256);

	ch = string;
	cx = x;
	cy = y;

	while (1)
	{
		c = *ch++;
		if (!c)
			break;

		if (c == 0x8a)
		{
			int newcolor = toupper(*ch++);

			if (newcolor == 0)
			{
				return;
			}
			else if (newcolor == '-')
			{
				newcolor = normalcolor;
			}
			else if (newcolor >= 'A' && newcolor < 'A' + NUM_TEXT_COLORS)
			{
				newcolor -= 'A';
			}
			else if (newcolor == '+')
			{
				newcolor = boldcolor;
			}
			else
			{
				continue;
			}
			V_ColorMap = translationref_t(Ranges + newcolor * 256);
			continue;
		}

		if (c == '\n')
		{
			cx = x;
			cy += 9;
			continue;
		}

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE)
		{
			cx += 4;
			continue;
		}

		w = hu_font[c]->width();
		if (cx+w > width)
			break;

		DrawWrapper (drawer, hu_font[c], cx, cy);
		cx+=w;
	}
}

void DCanvas::TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper(drawer, normalcolor, x, y, string, CleanXfac, CleanYfac);
}

void DCanvas::TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, 
							const byte *string, int scalex, int scaley) const
{
	int 		w;
	const byte *ch;
	int 		c;
	int 		cx;
	int 		cy;
	int			boldcolor;

	if (normalcolor > NUM_TEXT_COLORS)
		normalcolor = CR_RED;
	boldcolor = normalcolor ? normalcolor - 1 : NUM_TEXT_COLORS - 1;

	V_ColorMap = translationref_t(Ranges + normalcolor * 256);

	ch = string;
	cx = x;
	cy = y;

	while (1)
	{
		c = *ch++;
		if (!c)
			break;

		if (c == 0x8a)
		{
			int newcolor = toupper(*ch++);

			if (newcolor == 0)
			{
				return;
			}
			else if (newcolor == '-')
			{
				newcolor = normalcolor;
			}
			else if (newcolor >= 'A' && newcolor < 'A' + NUM_TEXT_COLORS)
			{
				newcolor -= 'A';
			}
			else if (newcolor == '+')
			{
				newcolor = boldcolor;
			}
			else
			{
				continue;
			}
			V_ColorMap = translationref_t(Ranges + newcolor * 256);
			continue;
		}

		if (c == '\n')
		{
			cx = x;
			cy += 9 * scalex;
			continue;
		}

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE)
		{
			cx += 4 * scaley;
			continue;
		}

		w = hu_font[c]->width() * scalex;
		if (cx+w > width)
			break;

        DrawSWrapper (drawer, hu_font[c], cx, cy,
                        hu_font[c]->width() * scalex,
                        hu_font[c]->height() * scaley);

		cx+=w;
	}
}

//
// Find string width from hu_font chars
//
int V_StringWidth (const byte *string)
{
	int w = 0, c;
	
	if(!string)
		return 0;

	while (*string)
	{
		if (*string == 0x8a)
		{
			if (*(++string))
				string++;
			continue;
		}
		else
		{
			c = toupper((*string++) & 0x7f) - HU_FONTSTART;
			if (c < 0 || c >= HU_FONTSIZE)
			{
				w += 4;
			}
			else
			{
				w += hu_font[c]->width();
			}
		}
	}

	return w;
}

//
// Break long lines of text into multiple lines no longer than maxwidth pixels
//
static void breakit (brokenlines_t *line, const byte *start, const byte *string)
{
	// Leave out trailing white space
	while (string > start && isspace (*(string - 1)))
		string--;

	line->string = new char[string - start + 1];
	strncpy (line->string, (char *)start, string - start);
	line->string[string - start] = 0;
	line->width = V_StringWidth (line->string);
}

brokenlines_t *V_BreakLines (int maxwidth, const byte *string)
{
	brokenlines_t lines[128];	// Support up to 128 lines (should be plenty)

	const byte *space = NULL, *start = string;
	int i, c, w, nw;
	BOOL lastWasSpace = false;

	i = w = 0;

	while ( (c = *string++) ) {
		if (c == 0x8a) {
			if (*string)
				string++;
			continue;
		}

		if (isspace(c)) {
			if (!lastWasSpace) {
				space = string - 1;
				lastWasSpace = true;
			}
		} else
			lastWasSpace = false;

		c = toupper (c & 0x7f) - HU_FONTSTART;

		if (c < 0 || c >= HU_FONTSIZE)
			nw = 4;
		else
			nw = hu_font[c]->width();

		if (w + nw > maxwidth || c == '\n' - HU_FONTSTART) {	// Time to break the line
			if (!space)
				space = string - 1;

			breakit (&lines[i], start, space);

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
				while (*start && isspace (*start))
					start++;
			string = start;
		} else
			w += nw;
	}

	if (string - start > 1) {
		const byte *s = start;

		while (s < string) {
			if (!isspace (*s++)) {
				breakit (&lines[i++], start, string);
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

