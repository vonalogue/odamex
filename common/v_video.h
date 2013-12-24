// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2013 by The Odamex Team.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Gamma correction LUT.
//	Functions to draw textures directly to screen.
//	Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


#ifndef __V_VIDEO_H__
#define __V_VIDEO_H__

#include <string>

#include "doomtype.h"

#include "v_palette.h"

#include "doomdef.h"

#include "r_texture.h"
#include "r_data.h"

extern int CleanXfac, CleanYfac;

extern int DisplayWidth, DisplayHeight, DisplayBits;
extern int SquareWidth;

#define APART(c)				(screen->alphaPart(c))
#define RPART(c)				(screen->redPart(c))
#define GPART(c)				(screen->greenPart(c))
#define BPART(c)				(screen->bluePart(c))

#define MAKERGB(r, g, b)		(screen->makeRGB(r, g, b))
#define MAKEARGB(a, r, g, b)	(screen->makeARGB(a, r, g, b))

//
// VIDEO
//
// [RH] Made screens more implementation-independant:
//
// V_LockScreen() must be called before drawing on a
// screen, and V_UnlockScreen() must be called when
// drawing is finished. As far as ZDoom is concerned,
// there are only two display depths: 8-bit indexed and
// 32-bit RGBA. The video driver is expected to be able
// to convert these to a format supported by the hardware.
// As such, the bits field is only used as an indicator
// of the output display depth and not as the screen's
// display depth (use is8bit for that).
class DCanvas : DObject
{
	DECLARE_CLASS (DCanvas, DObject)
public:
	enum EWrapperCode
	{
		EWrapper_Normal = 0,		// Just draws the texture as is
		EWrapper_Lucent = 1,		// Mixes the texture with the background
		EWrapper_Translated = 2,	// Color translates the texture and draws it
		EWrapper_TlatedLucent = 3,	// Translates the texture and mixes it with the background
		EWrapper_Colored = 4,		// Fills the texture area with a solid color
		EWrapper_ColoredLucent = 5	// Mixes a solid color in the texture area with the background
	};

	DCanvas() :
		buffer(NULL), m_LockCount(0), m_Private(NULL),
		ashift(24), rshift(16), gshift(8), bshift(0)
	{ }

	virtual ~DCanvas ()
	{ }

	int bits;
	byte *buffer;
	int width;
	int height;
	int pitch;
	inline bool is8bit() const { return bits == 8; }

	// [ML] If this is 320x200 or 640x400, the resolutions
	// "protected" from aspect ratio correction.
	inline bool isProtectedRes() const
	{
		return (width == 320 && height == 200) || (width == 640 && height == 400);
	}

	inline void setAlphaShift(byte n)
	{	ashift = n;	}

	inline void setRedShift(byte n)
	{	rshift = n;	}

	inline void setGreenShift(byte n)
	{	gshift = n;	}

	inline void setBlueShift(byte n)
	{	bshift = n;	}

	inline byte getAlphaShift() const
	{	return ashift;	}

	inline byte getRedShift() const
	{	return rshift;	}

	inline byte getGreenShift() const
	{	return gshift;	}

	inline byte getBlueShift() const
	{	return bshift;	}

	inline argb_t alphaPart(argb_t color) const
	{	return (color >> ashift) & 0xFF;	}

	inline argb_t redPart(argb_t color) const
	{	return (color >> rshift) & 0xFF;	}

	inline argb_t greenPart(argb_t color) const
	{	return (color >> gshift) & 0xFF;	}

	inline argb_t bluePart(argb_t color) const
	{	return (color >> bshift) & 0xFF;	}

	inline argb_t makeRGB(byte r, byte g, byte b) const
	{
		return (r << rshift) | (g << gshift) | (b << bshift);
	}

	inline argb_t makeARGB(byte a, byte r, byte g, byte b) const
	{
		return (a << ashift) | (r << rshift) | (g << gshift) | (b << bshift);
	}

	int m_LockCount;
	palette_t *m_Palette;
	void *m_Private;

	// Copy blocks from one canvas to another
	void Blit (int srcx, int srcy, int srcwidth, int srcheight, DCanvas *dest, int destx, int desty, int destwidth, int destheight);
	void CopyRect (int srcx, int srcy, int width, int height, int destx, int desty, DCanvas *destscrn);

	// Draw a linear block of pixels into the view buffer.
	void DrawBlock (int x, int y, int width, int height, const byte *src) const;

	// Reads a linear block of pixels from the view buffer.
	void GetBlock (int x, int y, int width, int height, byte *dest) const;

	// Reads a transposed block of pixels from the view buffer
	void GetTransposedBlock(int x, int y, int _width, int _height, byte* dest) const;

	// Darken a rectangle of th canvas
	void Dim (int x, int y, int width, int height, const char* color, float amount) const;
	void Dim (int x, int y, int width, int height) const;

	// Fill an area with a 64x64 flat texture
	void FlatFill(const Texture* texture, int left, int top, int right, int bottom) const;

	// Set an area to a specified color
	void Clear (int left, int top, int right, int bottom, int color) const;

	// Access control
	void Lock ();
	void Unlock ();

	// Output some text with wad heads-up font
	inline void DrawText (int normalcolor, int x, int y, const byte *string) const;
	inline void DrawTextLuc (int normalcolor, int x, int y, const byte *string) const;
	inline void DrawTextClean (int normalcolor, int x, int y, const byte *string) const;		// Does not adjust x and y
	inline void DrawTextCleanLuc (int normalcolor, int x, int y, const byte *string) const;		// ditto
	inline void DrawTextCleanMove (int normalcolor, int x, int y, const byte *string) const;	// This one does
	inline void DrawTextStretched (int normalcolor, int x, int y, const byte *string, int scalex, int scaley) const;
	inline void DrawTextStretchedLuc (int normalcolor, int x, int y, const byte *string, int scalex, int scaley) const;

	inline void DrawText (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextLuc (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextClean (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextCleanLuc (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextCleanMove (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextStretched (int normalcolor, int x, int y, const char *string, int scalex, int scaley) const;
	inline void DrawTextStretchedLuc (int normalcolor, int x, int y, const char *string, int scalex, int scaley) const;

	inline void DrawTexture(const Texture* texture, int x, int y) const;
	inline void DrawTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const;
	inline void DrawTextureDirect(const Texture* texture, int x, int y) const;
	inline void DrawTextureIndirect(const Texture* texture, int x, int y) const;
	inline void DrawTextureClean(const Texture* texture, int x, int y) const;
	inline void DrawTextureCleanNoMove(const Texture* texture, int x, int y) const;

	void DrawTextureFullScreen(const Texture* texture) const;

	inline void DrawLucentTexture(const Texture* texture, int x, int y) const;
	inline void DrawLucentTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const;
	inline void DrawLucentTextureDirect(const Texture* texture, int x, int y) const;
	inline void DrawLucentTextureIndirect(const Texture* texture, int x, int y) const;
	inline void DrawLucentTextureClean(const Texture* texture, int x, int y) const;
	inline void DrawLucentTextureCleanNoMove(const Texture* texture, int x, int y) const;

	inline void DrawTranslatedTexture(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const;
	inline void DrawTranslatedTextureDirect(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedTextureIndirect(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedTextureClean(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedTextureCleanNoMove(const Texture* texture, int x, int y) const;

	inline void DrawTranslatedLucentTexture(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedLucentTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const;
	inline void DrawTranslatedLucentTextureDirect(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedLucentTextureIndirect(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedLucentTextureClean(const Texture* texture, int x, int y) const;
	inline void DrawTranslatedLucentTextureCleanNoMove(const Texture* texture, int x, int y) const;

	inline void DrawColoredTexture(const Texture* texture, int x, int y) const;
	inline void DrawColoredTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const;
	inline void DrawColoredTextureDirect(const Texture* texture, int x, int y) const;
	inline void DrawColoredTextureIndirect(const Texture* texture, int x, int y) const;
	inline void DrawColoredTextureClean(const Texture* texture, int x, int y) const;
	inline void DrawColoredTextureCleanNoMove(const Texture* texture, int x, int y) const;

	inline void DrawColoredLucentTexture(const Texture* texture, int x, int y) const;
	inline void DrawColoredLucentTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const;
	inline void DrawColoredLucentTextureDirect(const Texture* texture, int x, int y) const;
	inline void DrawColoredLucentTextureIndirect(const Texture* texture, int x, int y) const;
	inline void DrawColoredLucentTextureClean(const Texture* texture, int x, int y) const;
	inline void DrawColoredLucentTextureCleanNoMove(const Texture* texture, int x, int y) const;

protected:
	void TextWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const;
	void TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const;
	void TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string, int scalex, int scaley) const;

	void DrawWrapper(EWrapperCode drawer, const Texture* texture, int x, int y) const;
	void DrawSWrapper(EWrapperCode drawer, const Texture* texture, int x, int y, int destwidth, int destheight) const;
	void DrawIWrapper(EWrapperCode drawer, const Texture* texture, int x, int y) const;
	void DrawCWrapper(EWrapperCode drawer, const Texture* texture, int x, int y) const;
	void DrawCNMWrapper(EWrapperCode drawer, const Texture* texture, int x, int y) const;

	byte ashift;
	byte rshift;
	byte gshift;
	byte bshift;
};

inline void DCanvas::DrawText (int normalcolor, int x, int y, const byte *string) const
{
	TextWrapper (EWrapper_Translated, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextLuc (int normalcolor, int x, int y, const byte *string) const
{
	TextWrapper (EWrapper_TlatedLucent, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextClean (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanLuc (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (EWrapper_TlatedLucent, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanMove (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor,
		(x - 160) + width / 2,
		(y - 100) + height / 2,
		string);
}
inline void DCanvas::DrawTextStretched (int normalcolor, int x, int y, const byte *string, int scalex, int scaley) const
{
	TextSWrapper (EWrapper_Translated, normalcolor, x, y, string, scalex, scaley);
}

inline void DCanvas::DrawTextStretchedLuc (int normalcolor, int x, int y, const byte *string, int scalex, int scaley) const
{
	TextSWrapper (EWrapper_TlatedLucent, normalcolor, x, y, string, scalex, scaley);
}

inline void DCanvas::DrawText (int normalcolor, int x, int y, const char *string) const
{
	TextWrapper (EWrapper_Translated, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextLuc (int normalcolor, int x, int y, const char *string) const
{
	TextWrapper (EWrapper_TlatedLucent, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextClean (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanLuc (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (EWrapper_TlatedLucent, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanMove (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor,
		(x - 160) * CleanXfac + width / 2,
		(y - 100) * CleanYfac + height / 2,
		(const byte *)string);
}
inline void DCanvas::DrawTextStretched (int normalcolor, int x, int y, const char *string, int scalex, int scaley) const
{
	TextSWrapper (EWrapper_Translated, normalcolor, x, y, (const byte *)string, scalex, scaley);
}
inline void DCanvas::DrawTextStretchedLuc (int normalcolor, int x, int y, const char *string, int scalex, int scaley) const
{
	TextSWrapper (EWrapper_TlatedLucent, normalcolor, x, y, (const byte *)string, scalex, scaley);
}

inline void DCanvas::DrawTexture(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_Normal, texture, x, y);
}
inline void DCanvas::DrawTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const
{
	DrawSWrapper(EWrapper_Normal, texture, x, y, dw, dh);
}
inline void DCanvas::DrawTextureDirect(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_Normal, texture, x, y);
}
inline void DCanvas::DrawTextureIndirect(const Texture* texture, int x, int y) const
{
	DrawIWrapper(EWrapper_Normal, texture, x, y);
}
inline void DCanvas::DrawTextureClean(const Texture* texture, int x, int y) const
{
	DrawCWrapper(EWrapper_Normal, texture, x, y);
}
inline void DCanvas::DrawTextureCleanNoMove(const Texture* texture, int x, int y) const
{
	DrawCNMWrapper(EWrapper_Normal, texture, x, y);
}

inline void DCanvas::DrawLucentTexture(const Texture *texture, int x, int y) const
{
	DrawWrapper(EWrapper_Lucent, texture, x, y);
}
inline void DCanvas::DrawLucentTextureStretched(const Texture *texture, int x, int y, int dw, int dh) const
{
	DrawSWrapper(EWrapper_Lucent, texture, x, y, dw, dh);
}
inline void DCanvas::DrawLucentTextureDirect(const Texture *texture, int x, int y) const
{
	DrawWrapper(EWrapper_Lucent, texture, x, y);
}
inline void DCanvas::DrawLucentTextureIndirect(const Texture *texture, int x, int y) const
{
	DrawIWrapper(EWrapper_Lucent, texture, x, y);
}
inline void DCanvas::DrawLucentTextureClean(const Texture *texture, int x, int y) const
{
	DrawCWrapper(EWrapper_Lucent, texture, x, y);
}
inline void DCanvas::DrawLucentTextureCleanNoMove(const Texture *texture, int x, int y) const
{
	DrawCNMWrapper(EWrapper_Lucent, texture, x, y);
}

inline void DCanvas::DrawTranslatedTexture(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_Translated, texture, x, y);
}
inline void DCanvas::DrawTranslatedTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const
{
	DrawSWrapper(EWrapper_Translated, texture, x, y, dw, dh);
}
inline void DCanvas::DrawTranslatedTextureDirect(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_Translated, texture, x, y);
}
inline void DCanvas::DrawTranslatedTextureIndirect(const Texture* texture, int x, int y) const
{
	DrawIWrapper(EWrapper_Translated, texture, x, y);
}
inline void DCanvas::DrawTranslatedTextureClean(const Texture* texture, int x, int y) const
{
	DrawCWrapper(EWrapper_Translated, texture, x, y);
}
inline void DCanvas::DrawTranslatedTextureCleanNoMove(const Texture* texture, int x, int y) const
{
	DrawCNMWrapper(EWrapper_Translated, texture, x, y);
}

inline void DCanvas::DrawTranslatedLucentTexture(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_TlatedLucent, texture, x, y);
}
inline void DCanvas::DrawTranslatedLucentTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const
{
	DrawSWrapper(EWrapper_TlatedLucent, texture, x, y, dw, dh);
}
inline void DCanvas::DrawTranslatedLucentTextureDirect(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_TlatedLucent, texture, x, y);
}
inline void DCanvas::DrawTranslatedLucentTextureIndirect(const Texture* texture, int x, int y) const
{
	DrawIWrapper(EWrapper_TlatedLucent, texture, x, y);
}
inline void DCanvas::DrawTranslatedLucentTextureClean(const Texture* texture, int x, int y) const
{
	DrawCWrapper(EWrapper_TlatedLucent, texture, x, y);
}
inline void DCanvas::DrawTranslatedLucentTextureCleanNoMove(const Texture* texture, int x, int y) const
{
	DrawCNMWrapper(EWrapper_TlatedLucent, texture, x, y);
}

inline void DCanvas::DrawColoredTexture(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_Colored, texture, x, y);
}
inline void DCanvas::DrawColoredTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const
{
	DrawSWrapper(EWrapper_Colored, texture, x, y, dw, dh);
}
inline void DCanvas::DrawColoredTextureDirect(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_Colored, texture, x, y);
}
inline void DCanvas::DrawColoredTextureIndirect(const Texture* texture, int x, int y) const
{
	DrawIWrapper(EWrapper_Colored, texture, x, y);
}
inline void DCanvas::DrawColoredTextureClean(const Texture* texture, int x, int y) const
{
	DrawCWrapper(EWrapper_Colored, texture, x, y);
}
inline void DCanvas::DrawColoredTextureCleanNoMove(const Texture* texture, int x, int y) const
{
	DrawCNMWrapper(EWrapper_Colored, texture, x, y);
}

inline void DCanvas::DrawColoredLucentTexture(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_ColoredLucent, texture, x, y);
}
inline void DCanvas::DrawColoredLucentTextureStretched(const Texture* texture, int x, int y, int dw, int dh) const
{
	DrawSWrapper(EWrapper_ColoredLucent, texture, x, y, dw, dh);
}
inline void DCanvas::DrawColoredLucentTextureDirect(const Texture* texture, int x, int y) const
{
	DrawWrapper(EWrapper_ColoredLucent, texture, x, y);
}
inline void DCanvas::DrawColoredLucentTextureIndirect(const Texture* texture, int x, int y) const
{
	DrawIWrapper(EWrapper_ColoredLucent, texture, x, y);
}
inline void DCanvas::DrawColoredLucentTextureClean(const Texture* texture, int x, int y) const
{
	DrawCWrapper(EWrapper_ColoredLucent, texture, x, y);
}
inline void DCanvas::DrawColoredLucentTextureCleanNoMove(const Texture* texture, int x, int y) const
{
	DrawCNMWrapper(EWrapper_ColoredLucent, texture, x, y);
}

// This is the screen updated by I_FinishUpdate.
extern	DCanvas *screen;

extern	DBoundingBox 	dirtybox;

// Translucency tables
extern argb_t Col2RGB8[65][256];
extern palindex_t RGB32k[32][32][32];

// Allocates buffer screens, call before R_Init.
void V_Init (void);

void V_Close (void);

// The color to fill with for #4 and #5 above
extern int V_ColorFill;

// The color map for #1 and #2 above
extern translationref_t V_ColorMap;

// The palette lookup table to be used with for direct modes
extern shaderef_t V_Palette;

void V_MarkRect (int x, int y, int width, int height);

// Returns the closest color to the one desired. String
// should be of the form "rr gg bb".
int V_GetColorFromString (const argb_t *palette, const char *colorstring);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
std::string V_GetColorStringByName (const char *name);


bool V_SetResolution (int width, int height, int bpp);
bool V_UsePillarBox();
bool V_UseLetterBox();
bool V_UseWidescreen();


// ----------------------------------------------------------------------------
//
// Shaders
//
// ----------------------------------------------------------------------------

//
// rt_blend2
//
// Performs alpha-blending between two colors.
//
template<typename PIXEL_T>
static forceinline PIXEL_T rt_blend2(const PIXEL_T bg, const int bga, const PIXEL_T fg, const int fga);

template <>
forceinline palindex_t rt_blend2(const palindex_t bg, const int bga, const palindex_t fg, const int fga)
{
	// Crazy 8bpp alpha-blending using lookup tables and bit twiddling magic
	argb_t bgARGB = Col2RGB8[bga >> 2][bg];
	argb_t fgARGB = Col2RGB8[fga >> 2][fg];

	argb_t mix = (fgARGB + bgARGB) | 0x1f07c1f;
	return RGB32k[0][0][mix & (mix >> 15)];
}

template <>
forceinline argb_t rt_blend2(const argb_t bg, const int bga, const argb_t fg, const int fga)
{
	return MAKERGB(
		(RPART(bg) * bga + RPART(fg) * fga) >> 8,
		(GPART(bg) * bga + GPART(fg) * fga) >> 8,
		(BPART(bg) * bga + BPART(fg) * fga) >> 8
	);
}


//
// rt_rawcolor
//
// Performs no colormapping and only uses the default palette.
//
template<typename PIXEL_T>
static forceinline PIXEL_T rt_rawcolor(const shaderef_t &pal, const byte c);

template <>
forceinline palindex_t rt_rawcolor<palindex_t>(const shaderef_t &pal, const byte c)
{
	return (c);
}

template <>
forceinline argb_t rt_rawcolor<argb_t>(const shaderef_t &pal, const byte c)
{
	return pal.shade(c);
}


//
// rt_mapcolor
//
// Performs color mapping.
//
template<typename PIXEL_T>
static forceinline PIXEL_T rt_mapcolor(const shaderef_t &pal, const byte c);

template <>
forceinline palindex_t rt_mapcolor<palindex_t>(const shaderef_t &pal, const byte c)
{
	return pal.index(c);
}

template <>
forceinline argb_t rt_mapcolor<argb_t>(const shaderef_t &pal, const byte c)
{
	return pal.shade(c);
}


//
// rt_tlatecolor
//
// Performs color mapping and color translation.
//
template<typename PIXEL_T>
static forceinline PIXEL_T rt_tlatecolor
			(const shaderef_t &pal, const translationref_t &translation, const byte c);

template <>
forceinline palindex_t rt_tlatecolor<palindex_t>
			(const shaderef_t &pal, const translationref_t &translation, const byte c)
{
	return translation.tlate(pal.index(c));
}

template <>
forceinline argb_t rt_tlatecolor<argb_t>
			(const shaderef_t &pal, const translationref_t &translation, const byte c)
{
	return pal.tlate(translation, c);
}



// Alpha blend between two RGB colors with only dest alpha value
// 0 <=   toa <= 255
forceinline argb_t alphablend1a(const argb_t from, const argb_t to, const int toa)
{
	const int fr = RPART(from);
	const int fg = GPART(from);
	const int fb = BPART(from);

	const int dr = RPART(to) - fr;
	const int dg = GPART(to) - fg;
	const int db = BPART(to) - fb;

	return MAKERGB(
		fr + ((dr * toa) >> 8),
		fg + ((dg * toa) >> 8),
		fb + ((db * toa) >> 8)
	);
}

#endif // __V_VIDEO_H__


