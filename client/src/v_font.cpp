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
#include "w_wad.h"

#include "v_font.h"

#include "SDL_ttf.h"

extern byte *Ranges;

//
// V_LoadDoomFontChar
//
// Reads a font character sprite from the WAD file and creates a scaled
// Texture from the sprite.
//
static texhandle_t V_LoadDoomFontChar(const char* name, fixed_t scale)
{
	texhandle_t source_texhandle = texturemanager.getHandle(name, Texture::TEX_PATCH);
	const Texture* source_texture = texturemanager.getTexture(source_texhandle);

	int dest_charwidth = (source_texture->getWidth() * scale) >> FRACBITS;
	int dest_charheight = (source_texture->getHeight() * scale) >> FRACBITS;

	texhandle_t dest_texhandle = texturemanager.createCustomHandle();
	Texture* dest_texture = texturemanager.createTexture(dest_texhandle, dest_charwidth, dest_charheight);
	R_CopySubimage(dest_texture, source_texture,
			0, 0, dest_charwidth - 1, dest_charheight - 1,
			0, 0, source_texture->getWidth() - 1, source_texture->getHeight() - 1);

	texturemanager.freeTexture(source_texhandle);

	// translate the characters to red so that they can be
	// easily translated later
	byte* data = dest_texture->getData();
	for (int i = 0; i < dest_charwidth * dest_charheight; i++)
	{
		if (data[i] >= 0x50 && data[i] <= 0x5F)
			data[i] += (0xB0 - 0x50);
	}

	return dest_texhandle;
}

//
// V_LoadBlankDoomFontChar
//
// Creates a scaled blank texture given the unscaled width & height.
//
static texhandle_t V_LoadBlankDoomFontChar(int width, int height, fixed_t scale)
{
	int dest_charwidth = (width * scale) >> FRACBITS;
	int dest_charheight = (height * scale) >> FRACBITS;

	texhandle_t dest_texhandle = texturemanager.createCustomHandle();
	texturemanager.createTexture(dest_texhandle, dest_charwidth, dest_charheight);

	return dest_texhandle;
}


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
	else if (charnum == '\n')
		return;

	const Texture* texture = mCharacters[charnum];
	if (texture == NULL)
		return;

	if (c == '\t')
	{
		// convert tabs into 4 spaces
		for (int i = 0; i < 4; i++)
		{
			canvas->DrawTranslatedTexture(texture, x, y);
			x += getTextWidth((char)charnum);
		}
	}
	else
	{
		canvas->DrawTranslatedTexture(texture, x, y);
		x += getTextWidth((char)charnum);
	}
}

void OFont::printText(const DCanvas* canvas, int x, int y, int color, const char* str) const
{
	V_ColorMap = translationref_t(Ranges + color * 256);

	while (*str)
	{
		printCharacter(canvas, x, y, *str);
		str++;
	}
}

int OFont::getTextWidth(char c) const
{
	const Texture* texture = mCharacters[(int)c];
	if (texture)
		return texture->getWidth() - texture->getOffsetX();
	else
		return 0;
}

int OFont::getTextWidth(const char* str) const
{
	int width = 0;

	while (*str)
	{
		if (*str == '\t')
			width += 4 * getTextWidth(' ');
		else
			width += getTextWidth(*str);
		str++;
	}

	return width;
}

int OFont::getTextHeight(char c) const
{
	const Texture* texture = mCharacters[(int)c];
	if (texture)
		return texture->getHeight() - texture->getOffsetY();
	else
		return 0;
}

int OFont::getTextHeight(const char* str) const
{
	int height = 0;
	while (*str)
	{
		height = MAX(height, getTextHeight(*str));
		str++;
	}

	return height;
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

	// initialize lookups
	for (int i = 0; i < 256; i++)
	{
		mCharacters[i] = NULL;
		mCharacterHandles[i] = TextureManager::NO_TEXTURE_HANDLE;
	}

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

			mCharacterHandles[charnum] = texturemanager.createCustomHandle();

			Texture* texture = texturemanager.createTexture(mCharacterHandles[charnum], dest_charwidth, dest_charheight);
			mCharacters[charnum] = texture;

			int x1 = column * charwidth;
			int x2 = x1 + charwidth - 1;
			int y1 = row * charheight;
			int y2 = y1 + charheight - 1;
			R_CopySubimage(texture, conchars_texture,
					0, 0, dest_charwidth - 1, dest_charheight - 1,
					x1, y1, x2, y2);

			// translate the characters to red so that they can be
			// easily translated later
			byte* data = texture->getData();
			for (int i = 0; i < dest_charwidth * dest_charheight; i++)
			{
				if (data[i] >= 0x50 && data[i] <= 0x5F)
					data[i] += (0xB0 - 0x50);
			}
		}
	}

	texturemanager.freeTexture(conchars_handle);

	// base font height on the letter T
	mHeight = mCharacters['T']->getHeight();
}

ConCharsFont::~ConCharsFont()
{
	for (int i = 0; i < 256; i++)
	{
		if (mCharacterHandles[i] != TextureManager::NO_TEXTURE_HANDLE)
			texturemanager.freeTexture(mCharacterHandles[i]);
	}
}


// ----------------------------------------------------------------------------
//
// SmallDoomFont implementation
//
// ----------------------------------------------------------------------------

SmallDoomFont::SmallDoomFont(fixed_t scale)
{
	char name[12];

	// initialize lookups
	for (int i = 0; i < 256; i++)
	{
		mCharacters[i] = NULL;
		mCharacterHandles[i] = TextureManager::NO_TEXTURE_HANDLE;
	}

	// load the glyphs
	for (int charnum = '!'; charnum <= '_'; charnum++)
	{
		sprintf(name, "STCFN%.3d", charnum);
		mCharacterHandles[charnum] = V_LoadDoomFontChar(name, scale);
		mCharacters[charnum] = texturemanager.getTexture(mCharacterHandles[charnum]);
	}

	// lowercase glyphs are the same as uppercase
	for (int charnum = 'a'; charnum <= 'z'; charnum++)
	{
		mCharacters[charnum] = mCharacters[charnum - 32];	
		mCharacterHandles[charnum] = mCharacterHandles[charnum - 32];
	}

	// add blank glyphs for any not present in the font
	texhandle_t blank_texhandle = V_LoadBlankDoomFontChar(4, 7, scale);
	const Texture* blank_texture = texturemanager.getTexture(blank_texhandle);

	for (int charnum = 0; charnum < 256; charnum++)
	{
		if (mCharacters[charnum] == NULL)
		{
			mCharacterHandles[charnum] = blank_texhandle;
			mCharacters[charnum] = blank_texture;
		}
	}

	mHeight = mCharacters['T']->getHeight() + (scale >> FRACBITS);
}

SmallDoomFont::~SmallDoomFont()
{
	for (int i = 0; i < 256; i++)
	{
		if (mCharacterHandles[i] != TextureManager::NO_TEXTURE_HANDLE)
			texturemanager.freeTexture(mCharacterHandles[i]);
	}
}


// ----------------------------------------------------------------------------
//
// LargeDoomFont implementation
//
// ----------------------------------------------------------------------------

LargeDoomFont::LargeDoomFont(fixed_t scale)
{
	char name[12];

	// initialize lookups
	for (int i = 0; i < 256; i++)
	{
		mCharacters[i] = NULL;
		mCharacterHandles[i] = TextureManager::NO_TEXTURE_HANDLE;
	}

	// load the glyphs
	for (int charnum = '!'; charnum <= 'Z'; charnum++)
	{
		sprintf(name, "FONTB%02u", charnum - 32);
		mCharacterHandles[charnum] = V_LoadDoomFontChar(name, scale);
		mCharacters[charnum] = texturemanager.getTexture(mCharacterHandles[charnum]);
	}

	// lowercase glyphs are the same as uppercase
	for (int charnum = 'a'; charnum <= 'z'; charnum++)
	{
		mCharacters[charnum] = mCharacters[charnum - 32];	
		mCharacterHandles[charnum] = mCharacterHandles[charnum - 32];
	}

	// add blank glyphs for any not present in the font
	texhandle_t blank_texhandle = V_LoadBlankDoomFontChar(12, 12, scale);
	const Texture* blank_texture = texturemanager.getTexture(blank_texhandle);

	for (int charnum = 0; charnum < 256; charnum++)
	{
		if (mCharacters[charnum] == NULL)
		{
			mCharacterHandles[charnum] = blank_texhandle;
			mCharacters[charnum] = blank_texture;
		}
	}

	mHeight = mCharacters['T']->getHeight() + (scale >> FRACBITS);
}

LargeDoomFont::~LargeDoomFont()
{
	for (int i = 0; i < 256; i++)
	{
		if (mCharacterHandles[i] != TextureManager::NO_TEXTURE_HANDLE)
			texturemanager.freeTexture(mCharacterHandles[i]);
	}
}


// ----------------------------------------------------------------------------
//
// TrueTypeFont implementation
//
// ----------------------------------------------------------------------------

inline bool is_solid(const Texture* texture, int x, int y)
{
	return *(texture->getMaskData() + x * texture->getHeight() + y) != 0;
}

TrueTypeFont::TrueTypeFont(const char* lumpname, int size, unsigned int stylemask)
{
	memset(mCharacters, 0, sizeof(*mCharacters) * 256);

	if (TTF_Init() == -1)
		return;

	// read the TTF from the odamex.wad and store it in rawlumpdata
	int lumpnum = W_CheckNumForName(lumpname);
	if (lumpnum == -1)
	{
		Printf(PRINT_HIGH, "Unable to locate TrueType font %s!\n", lumpname);
		return;
	}
	
	// initialize lookups
	for (int i = 0; i < 256; i++)
	{
		mCharacters[i] = NULL;
		mCharacterHandles[i] = TextureManager::NO_TEXTURE_HANDLE;
	}

	size_t lumplen = W_LumpLength(lumpnum);
	byte* rawlumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, rawlumpdata);

	// open the TTF for usage 
	SDL_RWops* rw = SDL_RWFromMem(rawlumpdata, lumplen);
	TTF_Font* font = TTF_OpenFontRW(rw, 0, size);
	SDL_Color font_color = { 0, 0, 0 };

	texhandle_t background_texhandle = texturemanager.getHandle("FONTBACK", Texture::TEX_PATCH);
	const Texture* background_texture = texturemanager.getTexture(background_texhandle);

	for (int charnum = ' '; charnum < '~'; charnum++)
	{
		const char str[2] = { char(charnum), 0 };
		SDL_Surface* surface = TTF_RenderText_Solid(font, str, font_color);

		if (surface == NULL)
			continue;

		int width = surface->w;
		int height = surface->h;

		mCharacterHandles[charnum] = texturemanager.createCustomHandle();
		Texture* texture = texturemanager.createTexture(mCharacterHandles[charnum], width, height);

		if (stylemask & TTF_TEXTURE)
		{
			// load a texture to use for the background of the text
			R_CopySubimage(texture, background_texture,
					0, 0, width - 1, height - 1,
					0, 0, width - 1, height - 1);
		}
		else if (stylemask & TTF_GRADIENT)
		{
			// TODO
		}
		else
		{
			// set the background to a solid
			memset(texture, 0xB0, width * height * sizeof(byte));
		}

		SDL_PixelFormat* format = surface->format;
		if (format->BytesPerPixel == 1)
		{
			int pitch = surface->pitch;
			byte* source = (byte*)surface->pixels;
			byte* dest_mask = texture->getMaskData();

			for (int x = 0; x < width; x++)
			{
				for (int y = 0; y < height; y++)
				{
					byte pixel = source[pitch * y + x];
					*dest_mask = (pixel != 0);
					dest_mask++;
				}
			}
		}
		else if (format->BytesPerPixel == 4)
		{
			int pitch = surface->pitch;
			argb_t* source = (argb_t*)surface->pixels;
			byte* dest_mask = texture->getMaskData();

			for (int x = 0; x < width; x++)
			{
				for (int y = 0; y < height; y++)
				{
					argb_t pixel = source[pitch * y + x];
					byte alpha = (byte)(((pixel & format->Amask) >> format->Ashift) << format->Aloss);
					*dest_mask = (alpha != 0);
					dest_mask++;
				}
			}
		}

		SDL_FreeSurface(surface);

		if (stylemask & TTF_OUTLINE)
		{
			for (int x = 0; x < width; x++)
			{
				for (int y = 0; y < height; y++)
				{
					if (is_solid(texture, x, y))
					{
						bool edge = false;
						if (x == 0 || (x > 0 && !is_solid(texture, x - 1, y)))
							edge = true;
						else if (x == width - 1 || (x < width - 1 && !is_solid(texture, x + 1, y)))
							edge = true;
						else if (y == 0 || (y > 0 && !is_solid(texture, x, y - 1)))
							edge = true;
						else if (y == height - 1 || (y < height - 1 && !is_solid(texture, x, y + 1)))
							edge = true;

						if (edge)
							*(texture->getData() + x * height + y) = 0xBF;
					}
				}
			}
		}

		mCharacters[charnum] = texture;
	}
	
	texturemanager.freeTexture(background_texhandle);

	TTF_CloseFont(font);
	SDL_RWclose(rw);
	delete [] rawlumpdata;
	TTF_Quit();

	// base the font height on the letter T
	mHeight = mCharacters['T']->getHeight();
}

TrueTypeFont::~TrueTypeFont()
{
	for (int i = 0; i < 256; i++)
	{
		if (mCharacterHandles[i] != TextureManager::NO_TEXTURE_HANDLE)
			texturemanager.freeTexture(mCharacterHandles[i]);
	}
}

