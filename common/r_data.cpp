// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1998-2006 by Randy Heit (ZDoom).
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
// Revision 1.3  1997/01/29 20:10
// DESCRIPTION:
//		Preparation of data for rendering,
//		generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------

#include "r_texture.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_alloc.h"

#include "m_swap.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"


#include "r_data.h"

#include "v_palette.h"
#include "v_video.h"

#include <ctype.h>
#include <cstddef>

#include <algorithm>

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

int 			firstflat;
int 			lastflat;
int				numflats;

int 			firstspritelump;
int 			lastspritelump;
int				numspritelumps;

//
// R_CalculateNewPatchSize
//
// Helper function for converting raw patches that use post_t into patches
// that use tallpost_t. Returns the lump size of the converted patch.
//
size_t R_CalculateNewPatchSize(patch_t *patch, size_t length)
{
	if (!patch)
		return 0;

	// sanity check to see if the postofs array fits in the patch lump
	if (length < patch->width() * sizeof(unsigned int))
		return 0;

	int numposts = 0, numpixels = 0;
	unsigned int *postofs = (unsigned int *)((byte*)patch + 8);

	for (int i = 0; i < patch->width(); i++)
	{
		size_t ofs = LELONG(postofs[i]);

		// check that the offset is valid
		if (ofs >= length)
			return 0;

		post_t *post = (post_t*)((byte*)patch + ofs);

		while (post->topdelta != 0xFF)
		{
			if (ofs + post->length >= length)
				return 0;		// patch is corrupt

			numposts++;
			numpixels += post->length;
			post = (post_t*)((byte*)post + post->length + 4);
		}
	}

	// 8 byte patch header
	// 4 * width bytes for column offset table
	// 4 bytes per post for post header
	// 1 byte per pixel
	// 2 bytes per column for termination
	return 8 + 4 * patch->width() + 4 * numposts + numpixels + 2 * patch->width();
}

//
// R_ConvertPatch
//
// Converts a patch that uses post_t posts into a patch that uses tallpost_t.
//
void R_ConvertPatch(patch_t *newpatch, patch_t *rawpatch)
{
	if (!rawpatch || !newpatch)
		return;

	memcpy(newpatch, rawpatch, 8);		// copy the patch header

	unsigned *rawpostofs = (unsigned*)((byte*)rawpatch + 8);
	unsigned *newpostofs = (unsigned*)((byte*)newpatch + 8);

	unsigned curofs = 8 + 4 * rawpatch->width();	// keep track of the column offset

	for (int i = 0; i < rawpatch->width(); i++)
	{
		int abs_offset = 0;
			
		newpostofs[i] = LELONG(curofs);		// write the new offset for this column
		post_t *rawpost = (post_t*)((byte*)rawpatch + LELONG(rawpostofs[i]));
		tallpost_t *newpost = (tallpost_t*)((byte*)newpatch + curofs);

		while (rawpost->topdelta != 0xFF)
		{
			// handle DeePsea tall patches where topdelta is treated as a relative
			// offset instead of an absolute offset
			if (rawpost->topdelta <= abs_offset)
				abs_offset += rawpost->topdelta;
			else
				abs_offset = rawpost->topdelta;
				
			// watch for column overruns
			int length = rawpost->length;
			if (abs_offset + length > rawpatch->height())
				length = rawpatch->height() - abs_offset;

			// copy the pixels in the post
			memcpy(newpost->data(), (byte*)(rawpost) + 3, length);
				
			newpost->topdelta = abs_offset;
			newpost->length = length;

			curofs += newpost->length + 4;
			rawpost = (post_t*)((byte*)rawpost + rawpost->length + 4);
			newpost = newpost->next();
		}

		newpost->writeend();
		curofs += 2;
	}
}

//
// R_GetPatchColumn
//
tallpost_t* R_GetPatchColumn(int lumpnum, int colnum)
{
	patch_t* patch = W_CachePatch(lumpnum, PU_CACHE);
	return (tallpost_t*)((byte*)patch + LELONG(patch->columnofs[colnum]));
}

//
// R_GetPatchColumnData
//
byte* R_GetPatchColumnData(int lumpnum, int colnum)
{
	return R_GetPatchColumn(lumpnum, colnum)->data();
}

//
// R_InitTextures
// Initializes the texture list
//	with the textures from the world map.
//
void R_InitTextures (void)
{
	texturemanager.startup();
}


//
// R_InitFlats
//
void R_InitFlats (void)
{
	firstflat = W_GetNumForName ("F_START") + 1;
	lastflat = W_GetNumForName ("F_END") - 1;

	if(firstflat >= lastflat)
		I_Error("no flats");

	numflats = lastflat - firstflat + 1;
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//	so the sprite does not need to be cached completely
//	just for having the header info ready during rendering.
//
void R_InitSpriteLumps (void)
{
	firstspritelump = W_GetNumForName ("S_START") + 1;
	lastspritelump = W_GetNumForName ("S_END") - 1;

	numspritelumps = lastspritelump - firstspritelump + 1;

	if(firstspritelump > lastspritelump)
		I_Error("no sprite lumps");

	// [RH] Rather than maintaining separate spritewidth, spriteoffset,
	//		and spritetopoffset arrays, this data has now been moved into
	//		the sprite frame definition and gets initialized by
	//		R_InstallSpriteLump(), so there really isn't anything to do here.
}


static struct FakeCmap {
	char name[9];
	unsigned int blend;
} *fakecmaps;
size_t numfakecmaps;
int firstfakecmap;
shademap_t realcolormaps;
int lastusedcolormap;

void R_ForceDefaultColormap(const char *name)
{
	byte *data = (byte *)W_CacheLumpName (name, PU_CACHE);

	memcpy (realcolormaps.colormap, data, (NUMCOLORMAPS+1)*256);

#if 0
	// Setup shademap to mirror colormapped colors:
	for (int m = 0; m < (NUMCOLORMAPS+1); ++m)
		for (int c = 0; c < 256; ++c)
			realcolormaps.shademap[m*256+c] = V_Palette.shade(realcolormaps.colormap[m*256+c]);
#else
	BuildDefaultShademap (GetDefaultPalette(), realcolormaps);
#endif

	strncpy (fakecmaps[0].name, name, 9); // denis - todo - string limit?
	std::transform(fakecmaps[0].name, fakecmaps[0].name + strlen(fakecmaps[0].name), fakecmaps[0].name, toupper);
	fakecmaps[0].blend = 0;
}

void R_SetDefaultColormap (const char *name)
{
	if (strnicmp (fakecmaps[0].name, name, 8))
	{
		R_ForceDefaultColormap(name);
	}
}

void R_ReinitColormap()
{
	if (fakecmaps == NULL)
		return;

	const char *name = fakecmaps[0].name;

	if (name[0] == 0)
		name = "COLORMAP";

	R_ForceDefaultColormap(name);
}

//
// R_InitColormaps
//
void R_InitColormaps (void)
{
	// [RH] Try and convert BOOM colormaps into blending values.
	//		This is a really rough hack, but it's better than
	//		not doing anything with them at all (right?)
	int lastfakecmap = W_CheckNumForName ("C_END");
	firstfakecmap = W_CheckNumForName ("C_START");

	if (firstfakecmap == -1 || lastfakecmap == -1)
		numfakecmaps = 1;
	else
	{
		if(firstfakecmap > lastfakecmap)
			I_Error("no fake cmaps");

		numfakecmaps = lastfakecmap - firstfakecmap;
	}

	realcolormaps.colormap = (byte *)Z_Malloc (256*(NUMCOLORMAPS+1)*numfakecmaps,PU_STATIC,0);
	realcolormaps.shademap = (argb_t *)Z_Malloc (256*sizeof(argb_t)*(NUMCOLORMAPS+1)*numfakecmaps,PU_STATIC,0);
	fakecmaps = (FakeCmap *)Z_Malloc (sizeof(*fakecmaps) * numfakecmaps, PU_STATIC, 0);

	fakecmaps[0].name[0] = 0;
	R_ForceDefaultColormap ("COLORMAP");

	if (numfakecmaps > 1)
	{
		int i;
		size_t j;
		palette_t *pal = GetDefaultPalette ();
		shaderef_t defpal = shaderef_t(&pal->maps, 0);

		for (i = ++firstfakecmap, j = 1; j < numfakecmaps; i++, j++)
		{
			if (W_LumpLength (i) >= (NUMCOLORMAPS+1)*256)
			{
				int k, r, g, b;
				byte *map = (byte *)W_CacheLumpNum (i, PU_CACHE);

				byte  *colormap = realcolormaps.colormap+(NUMCOLORMAPS+1)*256*j;
				argb_t *shademap = realcolormaps.shademap+(NUMCOLORMAPS+1)*256*j;

				// Copy colormap data:
				memcpy (colormap, map, (NUMCOLORMAPS+1)*256);

				if(pal->basecolors)
				{
					r = RPART(pal->basecolors[*map]);
					g = GPART(pal->basecolors[*map]);
					b = BPART(pal->basecolors[*map]);

					W_GetLumpName (fakecmaps[j].name, i);
					for (k = 1; k < 256; k++) {
						r = (r + RPART(pal->basecolors[map[k]])) >> 1;
						g = (g + GPART(pal->basecolors[map[k]])) >> 1;
						b = (b + BPART(pal->basecolors[map[k]])) >> 1;
					}
					// NOTE(jsd): This alpha value is used for 32bpp in water areas.
					fakecmaps[j].blend = MAKEARGB (64, r, g, b);

					// Set up shademap for the colormap:
					for (k = 0; k < 256; ++k)
					{
						argb_t c = pal->basecolors[map[0]];
						shademap[k] = alphablend1a(c, MAKERGB(r,g,b), j * (256 / numfakecmaps));
					}
				}
				else
				{
					// Set up shademap for the colormap:
					for (k = 0; k < 256; ++k)
					{
						shademap[k] = defpal.shade(colormap[k]);
					}
				}
			}
		}
	}
}

// [RH] Returns an index into realcolormaps. Multiply it by
//		256*(NUMCOLORMAPS+1) to find the start of the colormap to use.
//		WATERMAP is an exception and returns a blending value instead.
int R_ColormapNumForName (const char *name)
{
	int lump, blend = 0;

	if (strnicmp (name, "COLORMAP", 8))
	{	// COLORMAP always returns 0
		if (-1 != (lump = W_CheckNumForName (name, ns_colormaps)) )
			blend = lump - firstfakecmap + 1;
		else if (!strnicmp (name, "WATERMAP", 8))
			blend = MAKEARGB (128,0,0x4f,0xa5);
	}

	return blend;
}

unsigned int R_BlendForColormap (int map)
{
	return APART(map) ? map :
		   (unsigned)map < numfakecmaps ? fakecmaps[map].blend : 0;
}

//
// R_InitData
// Locates all the lumps
//	that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
	R_InitColormaps ();
	R_InitTextures ();
	R_InitFlats ();
	R_InitSpriteLumps ();

	// haleyjd 01/28/10: also initialize tantoangle_acc table
	Table_InitTanToAngle ();
}

//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
// [RH] Rewrote this using Lee Killough's code in BOOM as an example.

void R_PrecacheLevel (void)
{
	if (demoplayback)
		return;

	texturemanager.precache();

	// Precache sprites.
	byte* hitlist = new byte[numsprites];
	memset (hitlist, 0, numsprites);

	AActor *actor;
	TThinkerIterator<AActor> iterator;

	while ( (actor = iterator.Next ()) )
		hitlist[actor->sprite] = 1;

	for (int i = numsprites - 1; i >= 0; i--)
	{
		if (hitlist[i])
			R_CacheSprite(sprites + i);
	}

	delete[] hitlist;
}

// Utility function,
//	called by R_PointToAngle.
unsigned int SlopeDiv (unsigned int num, unsigned int den)
{
	unsigned int ans;

	if (den < 512)
		return SLOPERANGE;

	ans = (num << 3) / (den >> 8);

	return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

// [ML] 11/4/06: Moved here from v_video.cpp
// [ML] 12/6/11: Moved from v_draw.cpp, not sure where to put it now...
/*
===============
BestColor
(borrowed from Quake2 source: utils3/qdata/images.c)
===============
*/
byte BestColor (const argb_t *palette, const int r, const int g, const int b, const int numcolors)
{
	int		i;
	int		dr, dg, db;
	int		bestdistortion, distortion;
	int		bestcolor;

//
// let any color go to 0 as a last resort
//
	bestdistortion = 256*256*4;
	bestcolor = 0;

	for (i = 0; i < numcolors; i++)
	{
		dr = r - RPART(palette[i]);
		dg = g - GPART(palette[i]);
		db = b - BPART(palette[i]);
		distortion = dr*dr + dg*dg + db*db;
		if (distortion < bestdistortion)
		{
			if (!distortion)
				return i;		// perfect match

			bestdistortion = distortion;
			bestcolor = i;
		}
	}

	return bestcolor;
}

byte BestColor2 (const argb_t *palette, const argb_t color, const int numcolors)
{
	return BestColor(palette, RPART(color), GPART(color), BPART(color), numcolors);
}

VERSION_CONTROL (r_data_cpp, "$Id$")

