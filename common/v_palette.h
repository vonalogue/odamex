// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: v_palette.h 1788 2010-08-24 04:42:57Z russellrice $
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
//	V_PALETTE
//
//-----------------------------------------------------------------------------

#ifndef __V_PALETTE_H__
#define __V_PALETTE_H__

#include "doomtype.h"
#include "r_defs.h"

struct palette_t
{
	shademap_t		maps;
	byte*			colormapsbase;
	argb_t*			colors;		// gamma corrected colors
	argb_t*			basecolors;	// non-gamma corrected colors
	unsigned		numcolors;
	unsigned		flags;
	unsigned		shadeshift;
};

// Generate shading ramps for lighting
#define PALETTEB_SHADE		(0)
#define PALETTEF_SHADE		(1<<PALETTEB_SHADE)

// Apply blend color specified in V_SetBlend()
#define PALETTEB_BLEND		(1)
#define PALETTEF_BLEND		(1<<PALETTEB_SHADE)

// Default palette when none is specified (Do not set directly!)
#define PALETTEB_DEFAULT	(30)
#define PALETTEF_DEFAULT	(1<<PALETTEB_DEFAULT)

argb_t V_GammaCorrect(int r, int g, int b);
argb_t V_GammaCorrect(argb_t value);
void V_GammaCorrectBuffer(argb_t* to, const argb_t* from, unsigned int count);

// Alpha blend between two RGB colors with only dest alpha value
// 0 <=   toa <= 256
argb_t alphablend1a(const argb_t from, const argb_t to, const int toa);

palindex_t V_BestColor(const argb_t *palette, int r, int g, int b, int numcolors);
palindex_t V_BestColor(const argb_t *palette, argb_t color, int numcolors);

// InitPalettes()
//	input: name:  the name of the default palette lump
//				  (normally GAMEPAL)
//
// Returns a pointer to the default palette.
palette_t* V_InitPalettes(const char* name);

void V_RestorePalettes();

// GetDefaultPalette()
//
//	Returns the palette created through V_InitPalettes()
palette_t* V_GetDefaultPalette();

//
// V_RestoreScreenPalette
//
// Restore original screen palette from current gamma level
void V_RestoreScreenPalette(void);

void V_FreePalette(palette_t* palette);

void V_RefreshPalettes();
void V_GammaAdjustPalettes();

// V_SetBlend()
//	input: blendr: red component of blend
//		   blendg: green component of blend
//		   blendb: blue component of blend
//		   blenda: alpha component of blend
//
// Applies the blend to all palettes with PALETTEF_BLEND flag
void V_SetBlend (int blendr, int blendg, int blendb, int blenda);

// V_ForceBlend()
//
// Normally, V_SetBlend() does nothing if the new blend is the
// same as the old. This function will performing the blending
// even if the blend hasn't changed.
void V_ForceBlend (int blendr, int blendg, int blendb, int blenda);

void V_DoPaletteEffects();

// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);

dyncolormap_t *GetSpecialLights (int lr, int lg, int lb, int fr, int fg, int fb);

#endif //__V_PALETTE_H__


