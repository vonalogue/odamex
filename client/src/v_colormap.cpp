// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: v_colormap.h $
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
//
// Color mapping, translating, and shading definitions
//
//-----------------------------------------------------------------------------

#include "doomtype.h"
#include "v_video.h"
#include "v_palette.h"
#include "v_colormap.h"
#include "w_wad.h"
#include "i_system.h"
#include "g_level.h"
#include <cmath>

// ----------------------------------------------------------------------------
//
// Global variables
//
// ----------------------------------------------------------------------------

static struct FakeCmap
{
	char name[9];
	argb_t blend;
} *fakecmaps;

int numfakecmaps;
int firstfakecmap;
shademap_t realcolormaps;
int lastusedcolormap;

dyncolormap_t NormalLight;


// ----------------------------------------------------------------------------
//
// General functions
//
// ----------------------------------------------------------------------------

//
// V_DoBlending
//
// Blends the colors in an ARGB8888 buffer with an ARGB8888 value.
// This is based (loosely) on the ColorShiftPalette()
// function from the dcolors.c file in the Doom utilities.
///
void V_DoBlending(argb_t* to, const argb_t* from, unsigned int count,
					int tor, int tog, int tob, int toa)
{
	for (unsigned i = 0; i < count; i++, from++)
	{
		unsigned int r = RPART(*from);
		unsigned int g = GPART(*from);
		unsigned int b = BPART(*from);

		int dr = r + ((toa * (tor - r)) >> 8);
		int dg = g + ((toa * (tog - g)) >> 8);
		int db = b + ((toa * (tob - b)) >> 8);

		*to++ = MAKERGB(dr, dg, db);
	}
}

void V_DoBlending(argb_t* to, const argb_t* from, unsigned int count, argb_t blend_value)
{
	unsigned int toa = APART(blend_value);
	unsigned int tor = RPART(blend_value);
	unsigned int tog = GPART(blend_value);
	unsigned int tob = BPART(blend_value);

	V_DoBlending(to, from, count, tor, tog, tob, toa);
}

//
// V_LightWithGamma
//
argb_t V_LightWithGamma(const dyncolormap_t* dyncolormap, argb_t color, int intensity)
{
	argb_t lightcolor;

	if (dyncolormap)
		lightcolor = dyncolormap->color;			// use dynamic lighting if availible
	else
		lightcolor = MAKERGB(0xFF, 0xFF, 0xFF);		// white light

	return V_GammaCorrect(
		(RPART(color) * RPART(lightcolor) * intensity) >> 16,
		(GPART(color) * GPART(lightcolor) * intensity) >> 16,
		(BPART(color) * BPART(lightcolor) * intensity) >> 16);
}


//
// V_FindDynamicColormap
//
// Finds the dynamic colormap that contains shademap
//
dyncolormap_t* V_FindDynamicColormap(const shademap_t* shademap)
{
	if (shademap != &V_GetDefaultPalette()->maps)
	{
		// Find the dynamic colormap by the shademap pointer:
		dyncolormap_t* colormap = &NormalLight;

		do
		{
			if (shademap == colormap->maps.map())
				return colormap;
		} while ( (colormap = colormap->next) );
	}

	return NULL;
}


//
// V_LightScale
//
// NOTE(jsd): Revised inverse logarithmic scale; near-perfect match to COLORMAP lump's scale
// 1 - ((Exp[1] - Exp[a*2 - 1]) / (Exp[1] - Exp[-1]))
//
static int V_LightScale(int value)
{
	static const float e1 = exp(1.0f);
	static const float e1sube0 = e1 - exp(-1.0f);

	float fvalue = 1.0f - (e1 - exp(2.0f * value / 255.0f - 1.0f)) / e1sube0;

	return (int)clamp(255.0f * fvalue, 0.0f, 255.0f);
}


//
// V_BuildLightRamp
//
void V_BuildLightRamp(shademap_t& maps)
{
	for (int i = 0; i < 256; i++)
		maps.ramp[i] = V_LightScale(i);
}


//
// V_BuildDefaultColorAndShademap
//
// Generates NUMCOLORMAPS colormaps and shademaps based on the given palette.
// The colormaps and shademaps will fade to the value level.fadeto, which is
// typically black.
//
// Also generates the invulnerability colormap.
//
void V_BuildDefaultColorAndShademap(const palette_t* pal, shademap_t& maps,
		argb_t lightcolor, argb_t fadecolor)
{
	unsigned int fader = RPART(fadecolor);
	unsigned int fadeg = GPART(fadecolor);
	unsigned int fadeb = BPART(fadecolor);

	unsigned int lightr = RPART(lightcolor);
	unsigned int lightg = GPART(lightcolor);
	unsigned int lightb = BPART(lightcolor);

	V_BuildLightRamp(maps);

	// build normal light mappings
	// [SL] Modified algorithm from RF_BuildLights in dcolors.c
	// from Doom Utilities. Now accomodates level.fadeto.
	for (unsigned int i = 0; i < NUMCOLORMAPS; i++)
	{
		argb_t* shademap = maps.shademap + (i << pal->shadeshift);
		palindex_t* colormap = maps.colormap + (i << pal->shadeshift);

		for (unsigned int c = 0; c < pal->numcolors; c++)
		{
			unsigned int r = RPART(pal->basecolors[c]) * lightr / 255;
			unsigned int g = GPART(pal->basecolors[c]) * lightg / 255;
			unsigned int b = BPART(pal->basecolors[c]) * lightb / 255;

			argb_t color = MAKERGB(
				r + ((fader - r) * i + NUMCOLORMAPS/2) / NUMCOLORMAPS,
				g + ((fadeg - g) * i + NUMCOLORMAPS/2) / NUMCOLORMAPS,
				b + ((fadeb - b) * i + NUMCOLORMAPS/2) / NUMCOLORMAPS);

			shademap[c] = V_GammaCorrect(color);
			colormap[c] = V_BestColor(pal->basecolors, color, pal->numcolors);
		}
	}

	// build special maps (e.g. invulnerability)
	// [SL] Modified algorithm from BuildSpecials in dcolors.c
	// from Doom Utilities.
	argb_t* shademap = maps.shademap + (NUMCOLORMAPS << pal->shadeshift);
	palindex_t* colormap = maps.colormap + (NUMCOLORMAPS << pal->shadeshift);

	for (unsigned int c = 0; c < pal->numcolors; c++)
	{
		int grayint = (int)(255.0f * clamp(1.0f -
						(RPART(pal->basecolors[c]) * 0.00116796875f +
						 GPART(pal->basecolors[c]) * 0.00229296875f +
			 			 BPART(pal->basecolors[c]) * 0.0005625f), 0.0f, 1.0f));

		shademap[c] = V_GammaCorrect(grayint, grayint, grayint);
		colormap[c] = V_BestColor(pal->basecolors, grayint, grayint, grayint, pal->numcolors);
	}
}


//
// V_ForceDefaultColormap
//
void V_ForceDefaultColormap(const char* name)
{
	V_BuildDefaultColorAndShademap(V_GetDefaultPalette(), realcolormaps,
				MAKERGB(255, 255, 255), level.fadeto);

	// allow colormaps in PWAD to override the generated colormap
	const byte* data = (byte*)W_CacheLumpName(name, PU_CACHE);
	memcpy(realcolormaps.colormap, data, (NUMCOLORMAPS + 1) * 256);

	strncpy(fakecmaps[0].name, name, 9); // denis - todo - string limit?
	std::transform(fakecmaps[0].name, fakecmaps[0].name + strlen(fakecmaps[0].name), fakecmaps[0].name, toupper);
	fakecmaps[0].blend = 0;
}


//
// V_SetDefaultColormap
//
void V_SetDefaultColormap(const char* name)
{
	if (strnicmp(fakecmaps[0].name, name, 8) != 0)
		V_ForceDefaultColormap(name);
}

//
// V_ReinitColormap
//
void V_ReinitColormap()
{
	if (fakecmaps)
	{
		if (strlen(fakecmaps[0].name) > 0)
			V_ForceDefaultColormap(fakecmaps[0].name);
		else
			V_ForceDefaultColormap("COLORMAP");
	}
}

//
// V_InitColormaps
//
void V_InitColormaps()
{
	// [RH] Try and convert BOOM colormaps into blending values.
	//		This is a really rough hack, but it's better than
	//		not doing anything with them at all (right?)
	int lastfakecmap = W_CheckNumForName("C_END");
	firstfakecmap = W_CheckNumForName("C_START");

	if (firstfakecmap == -1 || lastfakecmap == -1)
		numfakecmaps = 1;
	else
	{
		if (firstfakecmap > lastfakecmap)
			I_Error("no fake cmaps");

		numfakecmaps = lastfakecmap - firstfakecmap;
	}

	realcolormaps.colormap =
			(byte*)Z_Malloc(256 * (NUMCOLORMAPS + 1) * numfakecmaps, PU_STATIC, 0);
	realcolormaps.shademap =
			(argb_t*)Z_Malloc(256 * sizeof(argb_t) * (NUMCOLORMAPS + 1) * numfakecmaps, PU_STATIC, 0);
	fakecmaps = (FakeCmap*)Z_Malloc(sizeof(*fakecmaps) * numfakecmaps, PU_STATIC, 0);

	fakecmaps[0].name[0] = 0;
	V_ForceDefaultColormap("COLORMAP");

	if (numfakecmaps > 1)
	{
		palette_t* pal = V_GetDefaultPalette();
		shaderef_t defpal = shaderef_t(&pal->maps, 0);

		for (int i = ++firstfakecmap, j = 1; j < numfakecmaps; i++, j++)
		{
			if (W_LumpLength(i) >= (NUMCOLORMAPS + 1) * 256)
			{
				const byte* map = (byte*)W_CacheLumpNum(i, PU_CACHE);

				byte* colormap = realcolormaps.colormap + (NUMCOLORMAPS + 1) * 256 * j;
				argb_t* shademap = realcolormaps.shademap + (NUMCOLORMAPS + 1) * 256 * j;

				// Copy colormap data:
				memcpy(colormap, map, (NUMCOLORMAPS + 1) * 256);

				if (pal->basecolors)
				{
					unsigned int r = RPART(pal->basecolors[*map]);
					unsigned int g = GPART(pal->basecolors[*map]);
					unsigned int b = BPART(pal->basecolors[*map]);

					W_GetLumpName(fakecmaps[j].name, i);
					for (int k = 1; k < 256; k++)
					{
						r = (r + RPART(pal->basecolors[map[k]])) >> 1;
						g = (g + GPART(pal->basecolors[map[k]])) >> 1;
						b = (b + BPART(pal->basecolors[map[k]])) >> 1;
					}

					// NOTE(jsd): This alpha value is used for 32bpp in water areas.
					fakecmaps[j].blend = MAKEARGB(64, r, g, b);

					// Set up shademap for the colormap:
					for (int k = 0; k < 256; ++k)
					{
						argb_t c = pal->basecolors[map[0]];
						shademap[k] = alphablend1a(c, MAKERGB(r, g, b), j * (256 / numfakecmaps));
					}
				}
				else
				{
					// Set up shademap for the colormap:
					for (int k = 0; k < 256; ++k)
						shademap[k] = defpal.shade(colormap[k]);
				}
			}
		}
	}
}


//
// V_ColormapNumForName
//
// [RH] Returns an index into realcolormaps. Multiply it by
//		256*(NUMCOLORMAPS+1) to find the start of the colormap to use.
//		WATERMAP is an exception and returns a blending value instead.
int V_ColormapNumForName(const char* name)
{
	if (strnicmp(name, "COLORMAP", 8) != 0)
	{
		int lump = W_CheckNumForName(name, ns_colormaps);

		if (lump != -1)
			return lump - firstfakecmap + 1;
		else if (strnicmp(name, "WATERMAP", 8) == 0)
			return MAKEARGB(128, 0, 0x4F, 0xA5);
	}

	return 0;
}

//
// V_BlendForColormap
//
argb_t V_BlendForColormap(int map)
{
	return APART(map) ? map : map < numfakecmaps ? fakecmaps[map].blend : 0;
}


VERSION_CONTROL (v_colormap_cpp, "$Id: $")
