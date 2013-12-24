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
//	V_PALETTE
//
//-----------------------------------------------------------------------------


#include <cstring>
#include <math.h>
#include <cstddef>

#include "doomstat.h"
#include "v_video.h"
#include "v_colormap.h"
#include "v_gamma.h"
#include "m_alloc.h"
#include "r_main.h"		// For lighting constants
#include "w_wad.h"
#include "z_zone.h"
#include "i_video.h"
#include "c_dispatch.h"
#include "g_level.h"
#include "st_stuff.h"

/* Reimplement old way of doing red/gold colors, from Chocolate Doom - ML */

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS		1
#define STARTBONUSPALS		9
#define NUMREDPALS			8
#define NUMBONUSPALS		4
// Radiation suit, green shift.
#define RADIATIONPAL		13

EXTERN_CVAR(gammalevel)
EXTERN_CVAR(vid_gammatype)
EXTERN_CVAR(r_painintensity)
EXTERN_CVAR(sv_allowredscreen)

void V_ForceBlend (int blendr, int blendg, int blendb, int blenda);

static int lu_palette;
static int current_palette;
static float current_blend[4];

static palette_t DefPal;
static palette_t* FirstPal;

argb_t IndexedPalette[256];


/* Current color blending values */
int		BlendR, BlendG, BlendB, BlendA;

// ----------------------------------------------------------------------------
//
// Gamma Correction functions
//
// ----------------------------------------------------------------------------

static byte gammatable[256];
static bool gamma_initialized = false;

static DoomGammaStrategy doomgammastrat;
static ZDoomGammaStrategy zdoomgammastrat;
GammaStrategy* gammastrat = &doomgammastrat;


CVAR_FUNC_IMPL(vid_gammatype)
{
	int sanitized_var = clamp(var.value(), 0.0f, 1.0f);
	if (var == sanitized_var)
	{
		if (vid_gammatype == GAMMA_ZDOOM)
			gammastrat = &zdoomgammastrat;
		else
			gammastrat = &doomgammastrat;

		gammalevel.Set(gammalevel);
	}
	else
	{
		var.Set(sanitized_var);
	}
}


//
// V_GammaCorrect
//
// Applies the current gamma correction table to the given color.
//
argb_t V_GammaCorrect(int r, int g, int b)
{
	return MAKERGB(gammatable[r], gammatable[g], gammatable[b]);
}

argb_t V_GammaCorrect(argb_t value)
{
	unsigned int a = APART(value);
	unsigned int r = RPART(value);
	unsigned int g = GPART(value);
	unsigned int b = BPART(value);

	return MAKEARGB(a, gammatable[r], gammatable[g], gammatable[b]);
}

//
// V_GammaCorrectBuffer
//
// Applies the current gamma correction table to an ARGB8888 buffer.
//
void V_GammaCorrectBuffer(argb_t* to, const argb_t* from, unsigned int count)
{
	for (unsigned int i = 0; i < count; i++, from++)
		*to++ = V_GammaCorrect(*from);
}


static void V_UpdateGammaLevel(float level)
{
	static float lastgammalevel = 0.0f;
	static int lasttype = -1;			// ensure this gets set up the first time
	int type = vid_gammatype;

	if (lastgammalevel != level || lasttype != type)
	{
		// Only recalculate the gamma table if the new gamma
		// value is different from the old one.

		lastgammalevel = level;
		lasttype = type;

		gammastrat->generateGammaTable(gammatable, level);
		V_GammaAdjustPalettes();

		if (!screen)
			return;
		if (screen->is8bit())
			V_ForceBlend(BlendR, BlendG, BlendB, BlendA);
		else
			V_RefreshPalettes();
	}
}

CVAR_FUNC_IMPL(gammalevel)
{
	float sanitized_var = clamp(var.value(), gammastrat->min(), gammastrat->max());
	if (var == sanitized_var)
		V_UpdateGammaLevel(var);
	else
		var.Set(sanitized_var);
}

BEGIN_COMMAND(bumpgamma)
{
	gammalevel.Set(gammastrat->increment(gammalevel));

	if (gammalevel.value() == gammastrat->min())
	    Printf (PRINT_HIGH, "Gamma correction off\n");
	else
	    Printf (PRINT_HIGH, "Gamma correction level %g\n", gammalevel.value());
}
END_COMMAND(bumpgamma)


//
// V_GammaAdjustPalette
//
static void V_GammaAdjustPalette(palette_t* pal)
{
	if (!(pal->colors && pal->basecolors))
		return;

	if (!gamma_initialized)
		V_UpdateGammaLevel(gammalevel);

	V_GammaCorrectBuffer(pal->colors, pal->basecolors, pal->numcolors);
}


//
// V_GammaAdjustPalettes
//
void V_GammaAdjustPalettes()
{
	V_GammaAdjustPalette(&DefPal);

	for (palette_t* pal = FirstPal; pal; pal = pal->next)
		V_GammaAdjustPalette(pal);
}


// [Russell] - Restore original screen palette from current gamma level
void V_RestoreScreenPalette(void)
{
    if (screen && screen->is8bit())
		V_ForceBlend(BlendR, BlendG, BlendB, BlendA);
}

/****************************/
/* Palette management stuff */
/****************************/

//
// V_BestColor
//
// (borrowed from Quake2 source: utils3/qdata/images.c)
// [SL] Also nearly identical to BestColor in dcolors.c in Doom utilites
//
palindex_t V_BestColor(const argb_t *palette, int r, int g, int b, int numcolors)
{
	int bestdistortion = 256*256*4;
	int bestcolor = 0;		/// let any color go to 0 as a last resort

	for (int i = 0; i < numcolors; i++)
	{
		int dr = r - RPART(palette[i]);
		int dg = g - GPART(palette[i]);
		int db = b - BPART(palette[i]);
		int distortion = dr*dr + dg*dg + db*db;
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

palindex_t V_BestColor(const argb_t *palette, argb_t color, int numcolors)
{
	return V_BestColor(palette, RPART(color), GPART(color), BPART(color), numcolors);
}

static bool V_InternalCreatePalette(palette_t* palette, const char* name, byte *colors,
							unsigned numcolors, unsigned flags)
{
	if (numcolors == 0)
		return false;
	else if (numcolors > 256)
		numcolors = 256;

	strncpy(palette->name.name, name, 8);
	palette->flags = flags;
	palette->usecount = 1;
	palette->maps.colormap = NULL;
	palette->maps.shademap = NULL;

	M_Free(palette->basecolors);

	palette->basecolors = (argb_t *)Malloc(numcolors * 2 * sizeof(argb_t));
	palette->colors = palette->basecolors + numcolors;
	palette->numcolors = numcolors;

	palette->shadeshift = Log2(numcolors);

	for (unsigned int i = 0; i < numcolors; i++, colors += 3)
		palette->basecolors[i] = MAKERGB(colors[0], colors[1], colors[2]);

	V_GammaAdjustPalette(palette);

	return true;
}

palette_t* V_InitPalettes(const char* name)
{
	current_palette = -1;
	current_blend[0] = current_blend[1] = current_blend[2] = current_blend[3] = 255.0f;

    lu_palette = W_GetNumForName("PLAYPAL");
	byte* colors = (byte*)W_CacheLumpName(name, PU_CACHE);
	if (colors)
	{
		unsigned int flags = PALETTEF_SHADE | PALETTEF_BLEND | PALETTEF_DEFAULT;
		if (V_InternalCreatePalette(&DefPal, name, colors, 256, flags))
			return &DefPal;
	}

	return NULL;
}

palette_t* V_GetDefaultPalette()
{
	return &DefPal;
}

//
// V_FreePalette
//
// input:	palette: the palette to free
//
// This function decrements the palette's usecount and frees it
// when it hits zero.
//
void V_FreePalette(palette_t* palette)
{
	if (!(--palette->usecount))
	{
		if (!(palette->flags & PALETTEF_DEFAULT))
		{
			if (!palette->prev)
				FirstPal = palette->next;
			else
				palette->prev->next = palette->next;

			M_Free(palette->basecolors);
			M_Free(palette->colormapsbase);
			M_Free(palette);
		}
	}
}


//
// V_RefreshPalette
//
static void V_RefreshPalette(palette_t* pal)
{
	if (pal->flags & PALETTEF_SHADE)
	{
		if (pal->maps.colormap && pal->maps.colormap - pal->colormapsbase >= 256)
		{
			M_Free(pal->maps.colormap);
		}

		pal->colormapsbase = (byte *)Realloc(pal->colormapsbase, (NUMCOLORMAPS + 1) * 256 + 255);
		pal->maps.colormap = (byte *)(((ptrdiff_t)(pal->colormapsbase) + 255) & ~0xff);
		pal->maps.shademap = (argb_t *)Realloc(pal->maps.shademap, (NUMCOLORMAPS + 1)*256*sizeof(argb_t) + 255);

		V_BuildDefaultColorAndShademap(pal, pal->maps, MAKERGB(255, 255, 255), level.fadeto);
	}

	if (pal == &DefPal)
	{
		NormalLight.maps = shaderef_t(&DefPal.maps, 0);
		NormalLight.color = MAKERGB(255,255,255);
		NormalLight.fade = level.fadeto;
	}
}


//
// V_RefreshPalettes
//
void V_RefreshPalettes()
{
	V_RefreshPalette(&DefPal);

	for (palette_t* pal = FirstPal; pal; pal = pal->next)
		V_RefreshPalette(pal);
}


//
// V_AddBlend
//
// [RH] This is from Q2.
//
void V_AddBlend(float r, float g, float b, float a, float* v_blend)
{
	float a2, a3;

	if (a <= 0.0f)
		return;
	a2 = v_blend[3] + (1.0f - v_blend[3]) * a;	// new total alpha
	a3 = v_blend[3] / a2;		// fraction of color from old

	v_blend[0] = v_blend[0] * a3 + r*(1.0f - a3);
	v_blend[1] = v_blend[1] * a3 + g*(1.0f - a3);
	v_blend[2] = v_blend[2] * a3 + b*(1.0f - a3);
	v_blend[3] = a2;
}

void V_SetBlend (int blendr, int blendg, int blendb, int blenda)
{
	// Don't do anything if the new blend is the same as the old
	if ((blenda == 0 && BlendA == 0) ||
		(blendr == BlendR &&
		 blendg == BlendG &&
		 blendb == BlendB &&
		 blenda == BlendA))
		return;

	V_ForceBlend(blendr, blendg, blendb, blenda);
}

void V_ForceBlend (int blendr, int blendg, int blendb, int blenda)
{
	BlendR = blendr;
	BlendG = blendg;
	BlendB = blendb;
	BlendA = blenda;

	// blend the palette for 8-bit mode
	// shademap_t::shade takes care of blending
	// [SL] actually, an alpha overlay is drawn on top of the rendered screen
	// in R_RenderPlayerView
	if (screen->is8bit())
	{
		V_DoBlending(IndexedPalette, DefPal.colors, DefPal.numcolors,
					gammatable[BlendR], gammatable[BlendG], gammatable[BlendB], BlendA);
		I_SetPalette(IndexedPalette);
	}
}

BEGIN_COMMAND (testblend)
{
	int color;
	float amt;

	if (argc < 3)
	{
		Printf (PRINT_HIGH, "testblend <color> <amount>\n");
	}
	else
	{
		std::string colorstring = V_GetColorStringByName (argv[1]);

		if (colorstring.length())
			color = V_GetColorFromString (NULL, colorstring.c_str());
		else
			color = V_GetColorFromString (NULL, argv[1]);

		amt = (float)atof (argv[2]);
		if (amt > 1.0f)
			amt = 1.0f;
		else if (amt < 0.0f)
			amt = 0.0f;
		//V_SetBlend (RPART(color), GPART(color), BPART(color), (int)(amt * 256.0f));
		BaseBlendR = RPART(color);
		BaseBlendG = GPART(color);
		BaseBlendB = BPART(color);
		BaseBlendA = amt;
	}
}
END_COMMAND (testblend)

BEGIN_COMMAND (testfade)
{
	if (argc < 2)
	{
		Printf (PRINT_HIGH, "testfade <color>\n");
	}
	else
	{
		std::string colorstring = V_GetColorStringByName(argv[1]);
		if (!colorstring.empty())
			level.fadeto = V_GetColorFromString(NULL, colorstring.c_str());
		else
			level.fadeto = V_GetColorFromString(NULL, argv[1]);

		V_RefreshPalettes();
		NormalLight.maps = shaderef_t(&DefPal.maps, 0);
	}
}
END_COMMAND (testfade)

/****** Colorspace Conversion Functions ******/

// Code from http://www.cs.rit.edu/~yxv4997/t_convert.html

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//				if s == 0, then h = -1 (undefined)

// Green Doom guy colors:
// RGB - 0: {    .46  1 .429 } 7: {    .254 .571 .206 } 15: {    .0317 .0794 .0159 }
// HSV - 0: { 116.743 .571 1 } 7: { 112.110 .639 .571 } 15: { 105.071  .800 .0794 }
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v)
{
	float min, max, delta, foo;

	if (r == g && g == b) {
		*h = 0;
		*s = 0;
		*v = r;
		return;
	}

	foo = r < g ? r : g;
	min = (foo < b) ? foo : b;
	foo = r > g ? r : g;
	max = (foo > b) ? foo : b;

	*v = max;									// v

	delta = max - min;

	*s = delta / max;							// s

	if (r == max)
		*h = (g - b) / delta;					// between yellow & magenta
	else if (g == max)
		*h = 2 + (b - r) / delta;				// between cyan & yellow
	else
		*h = 4 + (r - g) / delta;				// between magenta & cyan

	*h *= 60;									// degrees
	if (*h < 0)
		*h += 360;
}

void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v)
{
	int i;
	float f, p, q, t;

	if (s == 0) {
		// achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;									// sector 0 to 5
	i = (int)floor (h);
	f = h - i;									// factorial part of h
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));

	switch (i) {
		case 0:		*r = v; *g = t; *b = p; break;
		case 1:		*r = q; *g = v; *b = p; break;
		case 2:		*r = p; *g = v; *b = t; break;
		case 3:		*r = p; *g = q; *b = v; break;
		case 4:		*r = t; *g = p; *b = v; break;
		default:	*r = v; *g = p; *b = q; break;
	}
}

dyncolormap_t* GetSpecialLights(int lr, int lg, int lb, int fr, int fg, int fb)
{
	argb_t lightcolor = MAKERGB(lr, lg, lb);
	argb_t fadecolor= MAKERGB(fr, fg, fb);
	dyncolormap_t* colormap = &NormalLight;

	// Bah! Simple linear search because I want to get this done.
	while (colormap)
	{
		if (lightcolor == colormap->color && fadecolor == colormap->fade)
			return colormap;
		else
			colormap = colormap->next;
	}

	// Not found. Create it.
	colormap = (dyncolormap_t*)Z_Malloc(sizeof(*colormap), PU_LEVEL, 0);
	shademap_t* maps = (shademap_t*)Z_Malloc(sizeof(shademap_t), PU_LEVEL, 0);
	maps->colormap = (byte*)Z_Malloc(NUMCOLORMAPS*256*sizeof(byte)+3+255, PU_LEVEL, 0);
	maps->colormap = (byte*)(((ptrdiff_t)maps->colormap + 255) & ~0xff);
	maps->shademap = (argb_t*)Z_Malloc(NUMCOLORMAPS*256*sizeof(argb_t)+3+255, PU_LEVEL, 0);
	maps->shademap = (argb_t*)(((ptrdiff_t)maps->shademap + 255) & ~0xff);

	colormap->maps = shaderef_t(maps, 0);
	colormap->color = lightcolor;
	colormap->fade = fadecolor;
	colormap->next = NormalLight.next;
	NormalLight.next = colormap;

	V_BuildDefaultColorAndShademap(&DefPal, *maps, lightcolor, fadecolor);

	return colormap;
}

BEGIN_COMMAND (testcolor)
{
	if (argc < 2)
	{
		Printf (PRINT_HIGH, "testcolor <color>\n");
	}
	else
	{
		std::string colorstring = V_GetColorStringByName(argv[1]);
		argb_t color;

		if (!colorstring.empty())
			color = V_GetColorFromString(NULL, colorstring.c_str());
		else
			color = V_GetColorFromString(NULL, argv[1]);

		V_BuildDefaultColorAndShademap(&DefPal, *(shademap_t*)NormalLight.maps.map(), color, level.fadeto);
	}
}
END_COMMAND (testcolor)

//
// V_DoPaletteEffects
//
// Handles changing the palette or the BlendR/G/B/A globals based on damage
// the player has taken, any power-ups, or environment such as deep water.
//
void V_DoPaletteEffects()
{
	player_t* plyr = &displayplayer();

	if (screen->is8bit())
	{
		int		palette;

		float cnt = (float)plyr->damagecount;
		if (!multiplayer || sv_allowredscreen)
			cnt *= r_painintensity;

		// slowly fade the berzerk out
		if (plyr->powers[pw_strength])
			cnt = MAX(cnt, 12.0f - float(plyr->powers[pw_strength] >> 6));

		if (cnt)
		{
			palette = ((int)cnt + 7) >> 3;

			if (gamemode == retail_chex)
				palette = RADIATIONPAL;
			else
			{
				if (palette >= NUMREDPALS)
					palette = NUMREDPALS-1;

				palette += STARTREDPALS;

				if (palette < 0)
					palette = 0;
			}
		}
		else if (plyr->bonuscount)
		{
			palette = (plyr->bonuscount+7)>>3;

			if (palette >= NUMBONUSPALS)
				palette = NUMBONUSPALS-1;

			palette += STARTBONUSPALS;
		}
		else if (plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet] & 8)
			palette = RADIATIONPAL;
		else
			palette = 0;

		if (palette != current_palette)
		{
			current_palette = palette;
			byte* pal = (byte *)W_CacheLumpNum(lu_palette, PU_CACHE) + palette * 768;
			I_SetOldPalette(pal);
		}
	}
	else
	{
		float blend[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float cnt;

		V_AddBlend(BaseBlendR / 255.0f, BaseBlendG / 255.0f, BaseBlendB / 255.0f, BaseBlendA, blend);

		// 32bpp gets color blending approximated to the original palettes:
		if (plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet] & 8)
			V_AddBlend(0.0f, 1.0f, 0.0f, 0.125f, blend);

		if (plyr->bonuscount)
		{
			cnt = (float)(plyr->bonuscount << 3);
			V_AddBlend(0.8431f, 0.7294f, 0.2706f, cnt > 128 ? 0.5f : cnt / 255.0f, blend);
		}

		// NOTE(jsd): rewritten to better match 8bpp behavior
		// 0 <= damagecount <= 100
		cnt = (float)plyr->damagecount*3.5f;
		if (!multiplayer || sv_allowredscreen)
			cnt *= r_painintensity;

		// slowly fade the berzerk out
		if (plyr->powers[pw_strength])
			cnt = MAX(cnt, 128.0f - float((plyr->powers[pw_strength]>>3) & (~0x1f)));
	
		cnt = clamp(cnt, 0.0f, 237.0f);

		if (cnt > 0.0f)
			V_AddBlend (1.0f, 0.0f, 0.0f, (cnt + 18.0f) / 255.0f, blend);

		V_AddBlend(plyr->BlendR, plyr->BlendG, plyr->BlendB, plyr->BlendA, blend);

		if (memcmp(blend, current_blend, sizeof(blend)))
	        memcpy(current_blend, blend, sizeof(blend));

		V_SetBlend ((int)(blend[0] * 255.0f), (int)(blend[1] * 255.0f),
					(int)(blend[2] * 255.0f), (int)(blend[3] * 256.0f));

/*
		argb_t blendcolor = MAKERGB(BlendR, BlendG, BlendB);

		for (unsigned int c = 0; c < DefPal.numcolors; c++)
			DefPal.basecolors[c] = rt_blend2(DefPal.basecolors[c], 255 - BlendA, blendcolor, BlendA);
		V_RefreshPalette(&DefPal);
*/
	}
}

VERSION_CONTROL (v_palette_cpp, "$Id$")

