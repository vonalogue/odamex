// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: v_font.h $
//
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
//
// Bitmapped font routines
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "v_video.h"
#include "r_texture.h"
#include "z_zone.h"

#include "v_font.h"

// ----------------------------------------------------------------------------
//
// OFont base class implementation
//
// ----------------------------------------------------------------------------

void OFont::printCharacter(const DCanvas* canvas, int& x, int& y, char c) const
{
	byte charnum = (byte)c;
	if (charnum == '\t')
		charnum = ' ';

	const Texture* texture = mCharacters[charnum];
	if (texture == NULL)
		return;

	int xoffs = texture->getOffsetX();
	int yoffs = texture->getOffsetY();

	if (c == '\t')
	{
		// convert tabs into 4 spaces
		for (int i = 0; i < 4; i++)
		{
			canvas->DrawTranslatedTexture(texture, x + xoffs, y + yoffs);
			x += texture->getWidth();
		}
	}
	else
	{
		canvas->DrawTranslatedTexture(texture, x + xoffs, y + yoffs);
		x += texture->getWidth();
	}
}

void OFont::printText(const DCanvas* canvas, int x, int y, int color, const char* str) const
{
	// TODO: change V_ColorMap to reflect color
//	V_ColorMap = translationref_t(Ranges + CR_ORANGE * 256);

	while (*str)
	{
		printCharacter(canvas, x, y, *str);
		str++;
	}
}

int OFont::getTextWidth(char c) const
{
	return mCharacters[(byte)c]->getWidth();
}

int OFont::getTextWidth(const char* str) const
{
	int width = 0;

	while (*str)
	{
		if (*str == '\t')
			width += 4 * mCharacters[' ']->getWidth();
		else
			width += mCharacters[(byte)*str]->getWidth();
		str++;
	}

	return width;
}

// ----------------------------------------------------------------------------
//
// ConCharsFont implementation
//
// ----------------------------------------------------------------------------

ConCharsFont::ConCharsFont(fixed_t scale)
{
	static const char lumpname[] = "CONCHARS";
	texhandle_t conchars_handle = texturemanager.getHandle(lumpname, Texture::TEX_PATCH);
	if (conchars_handle == TextureManager::NOT_FOUND_TEXTURE_HANDLE)
		I_Error("Unable to load %s lump.", lumpname);
	const Texture* conchars_texture = texturemanager.getTexture(conchars_handle);

	const int numcolumns = conchars_texture->getWidth() / charwidth;
	const int numrows = conchars_texture->getHeight() / charheight;

	for (int row = 0; row < numrows; row++)
	{
		for (int column = 0; column < numcolumns; column++)
		{	
			// scaled size of characters
			int dest_charwidth = (charwidth * scale) >> FRACBITS;
			int dest_charheight = (charheight * scale) >> FRACBITS;

			int charnum = row * numrows + column;
			texhandle_t texhandle = texturemanager.createSpecialUseHandle();			
			Texture* texture = texturemanager.createTexture(texhandle, dest_charwidth, dest_charheight);
			Z_ChangeTag(texture, PU_STATIC);	// don't allow texture to be purged from cache
			mCharacters[charnum] = texture;

			int x1 = column * charwidth;
			int x2 = x1 + charwidth - 1;
			int y1 = row * charheight;
			int y2 = y1 + charheight - 1;
			R_CopySubimage(texture, conchars_texture, x1, y1, x2, y2);
		}
	}
}


// ----------------------------------------------------------------------------
//
// HudFont implementation
//
// ----------------------------------------------------------------------------

HudFont::HudFont(fixed_t scale)
{
	static const char* tplate = "STCFN%.3d";
	char name[12];

	// initialize all Texture pointers to NULL
	memset(mCharacters, 0, sizeof(*mCharacters) * 256);

	for (int charnum = '!'; charnum <= '_'; charnum++)
	{
		sprintf(name, tplate, charnum);
		texhandle_t source_texhandle = texturemanager.getHandle(name, Texture::TEX_PATCH);
		const Texture* source_texture = texturemanager.getTexture(source_texhandle);

		int dest_charwidth = (source_texture->getWidth() * scale) >> FRACBITS;
		int dest_charheight = (source_texture->getHeight() * scale) >> FRACBITS;
		texhandle_t dest_texhandle = texturemanager.createSpecialUseHandle();
		Texture* dest_texture = texturemanager.createTexture(dest_texhandle, dest_charwidth, dest_charheight);
		R_CopySubimage(dest_texture, source_texture, 0, 0, source_texture->getWidth() - 1, source_texture->getHeight() - 1);

		Z_ChangeTag(dest_texture, PU_STATIC);	// don't allow texture to be purged from cache
		mCharacters[charnum] = dest_texture;
	}

	for (int charnum = 'a'; charnum <= 'z'; charnum++)
		mCharacters[charnum] = mCharacters[charnum - 32];	

	// add a character for ' ' (based on dimensions of the T character)
	int spacewidth = mCharacters['T']->getWidth() / 2;
	int spaceheight = mCharacters['T']->getHeight();
	texhandle_t space_texhandle = texturemanager.createSpecialUseHandle();
	Texture* space_texture = texturemanager.createTexture(space_texhandle, spacewidth, spaceheight);
	Z_ChangeTag(space_texture, PU_STATIC);
	mCharacters[' '] = space_texture;


	// add blank textures for the characters not present in the font
	int blankwidth = mCharacters['T']->getWidth() / 2;
	int blankheight = mCharacters['T']->getHeight();
	texhandle_t blank_texhandle = texturemanager.createSpecialUseHandle();
	Texture* blank_texture = texturemanager.createTexture(blank_texhandle, blankwidth, blankheight);
	Z_ChangeTag(blank_texture, PU_STATIC);

	for (int charnum = 0; charnum < 256; charnum++)
		if (mCharacters[charnum] == NULL)
			mCharacters[charnum] = blank_texture;	

	mHeight = mCharacters['T']->getHeight();
}

