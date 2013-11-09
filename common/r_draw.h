// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: r_draw.h 1837 2010-09-02 04:21:09Z spleen $
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
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_DRAW__
#define __R_DRAW__

#include "r_intrin.h"
#include "r_defs.h"

extern "C" byte**		ylookup;
extern "C" int*			columnofs;
extern "C" int			viewheight;
extern "C" int			centery;

typedef struct 
{
	int					x;
	int					yl;
	int					yh;
	int					pitch;
	
	fixed_t				iscale;
	fixed_t				texturemid;
	fixed_t				texturefrac;
	fixed_t				textureheight;

	int					color;
	fixed_t				translevel;

	byte*				dest;
	const byte*			source;
	const byte*			mask;

	translationref_t	translation;
	shaderef_t			colormap;
} drawcolumn_t;

extern "C" drawcolumn_t dcol;

typedef struct
{
	int					y;
	int					x1;
	int					x2;
	int					colsize;

	fixed_t				textureheightbits;
	fixed_t				texturewidthbits;

	dsfixed_t			xfrac;
	dsfixed_t			yfrac;
	dsfixed_t			xstep;
	dsfixed_t			ystep;

	int					color;
	fixed_t 			translevel;

	float				iu;
	float				iv;
	float				id;
	float				iustep;
	float				ivstep;
	float				idstep;

	byte*				dest;
	const byte*			source;
	const byte*			mask;

	shaderef_t			colormap;
	shaderef_t			slopelighting[MAXWIDTH];
} drawspan_t;

extern "C" drawspan_t dspan;

// [SL] Determine the screen buffer position to start drawing
inline byte* R_CalculateDestination(const drawcolumn_t& drawcolumn)
{
	return (byte*)(ylookup[drawcolumn.yl] + columnofs[drawcolumn.x]);
}

inline byte* R_CalculateDestination(const drawspan_t& drawspan)
{
	return (byte*)(ylookup[drawspan.y] + columnofs[drawspan.x1]);
}

inline int R_ColumnRangeMinimumHeight(int start, int stop, int* top)
{
	int minheight = viewheight - 1;
	for (int x = start; x <= stop; x++)
		minheight = MIN(minheight, top[x]);

	return MAX(minheight, 0);
}

inline int R_ColumnRangeMaximumHeight(int start, int stop, int* bottom)
{
	int maxheight = 0;
	for (int x = start; x <= stop; x++)
		maxheight = MAX(maxheight, bottom[x]);

	return MIN(maxheight, viewheight - 1);
}


//
// R_DrawColumnRange
//
// Draws a texture to the screen using a cache-friendly 64x64 pixel block
// scheme.
//
template <typename TextureMapper>
inline void R_DrawColumnRange(int start, int stop, int* top, int* bottom,
						const Texture* texture, TextureMapper& mapper, 
						const shaderef_t* colormap_table, void (*drawfunc)(drawcolumn_t&))
{
	const int width = stop - start + 1;
	if (width <= 0)
		return;

	fixed_t texiscale[MAXWIDTH];
	const byte* texdata[MAXWIDTH];

	for (int x = 0; x < width; x++)
	{
		texdata[x] = mapper.getData();
		texiscale[x] = mapper.getIScale();
		mapper.next();
	}

	const int BLOCKBITS = 6;
	const int BLOCKSIZE = (1 << BLOCKBITS);
	const int BLOCKMASK = (BLOCKSIZE - 1);

	// [SL] Render the range of columns in 64x64 pixel blocks, aligned to a grid
	// on the screen. This is to make better use of spatial locality in the cache.
	for (int bx = start; bx <= stop; bx = (bx & ~BLOCKMASK) + BLOCKSIZE)
	{
		int blockstartx = bx;
		int blockstopx = MIN((bx & ~BLOCKMASK) + BLOCKSIZE - 1, stop);

		int miny = R_ColumnRangeMinimumHeight(blockstartx, blockstopx, top);
		int maxy = R_ColumnRangeMaximumHeight(blockstartx, blockstopx, bottom);

		for (int by = miny; by <= maxy; by = (by & ~BLOCKMASK) + BLOCKSIZE)
		{
			int blockstarty = by;
			int blockstopy = (by & ~BLOCKMASK) + BLOCKSIZE - 1;

			for (int x = blockstartx; x <= blockstopx; x++)
			{
				dcol.yl = MAX(top[x], blockstarty);
				dcol.yh = MIN(bottom[x], blockstopy);
				if (dcol.yl <= dcol.yh)
				{
					dcol.x = x;
					dcol.iscale = texiscale[x - start]; 
					dcol.source = texdata[x - start];
					dcol.mask = dcol.source - texture->getData() + texture->getMaskData();
					dcol.dest = R_CalculateDestination(dcol);
					dcol.colormap = colormap_table[x];
					dcol.texturefrac = dcol.texturemid + FixedMul((dcol.yl - centery + 1) << FRACBITS, dcol.iscale);

					drawfunc(dcol);
				}
			}
		}
	}
}

// [RH] Pointers to the different column and span drawers...
extern void (*R_FillColumn)(drawcolumn_t&);
extern void (*R_FillMaskedColumn)(drawcolumn_t&);

extern void (*R_DrawColumn)(drawcolumn_t&);
extern void (*R_DrawMaskedColumn)(drawcolumn_t&);

// The Spectre/Invisibility effect.
extern void (*R_DrawFuzzColumn)(drawcolumn_t&);
extern void (*R_DrawFuzzMaskedColumn)(drawcolumn_t&);

// [RH] Draw translucent column;
extern void (*R_DrawTranslucentColumn)(drawcolumn_t&);
extern void (*R_DrawTranslucentMaskedColumn)(drawcolumn_t&);

// Draw with color translation tables,
//	for player sprite rendering,
//	Green/Red/Blue/Indigo shirts.
extern void (*R_DrawTranslatedColumn)(drawcolumn_t&);
extern void (*R_DrawTranslatedMaskedColumn)(drawcolumn_t&);

// Translated & translucent
extern void (*R_DrawTranslatedTranslucentColumn)(drawcolumn_t&);
extern void (*R_DrawTranslatedTranslucentMaskedColumn)(drawcolumn_t&);

// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.
extern void (*R_DrawSpan)(drawspan_t&);
extern void (*R_DrawSlopeSpan)(drawspan_t&);

extern void (*R_FillSpan)(drawspan_t&);
extern void (*R_FillTranslucentSpan)(drawspan_t&);

// [RH] Initialize the above function pointers
void R_InitColumnDrawers ();

void R_InitDrawers ();

void	R_BlankColumn (drawcolumn_t&);
void	R_BlankSpan (drawspan_t&);

void R_DrawSpanD_c(drawspan_t&);
void R_DrawSlopeSpanD_c(drawspan_t&);

#define SPANJUMP 16
#define INTERPSTEP (0.0625f)


// Vectorizable functions:
void r_dimpatchD_c(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h);

#ifdef __SSE2__
void R_DrawSpanD_SSE2(drawspan_t&);
void R_DrawSlopeSpanD_SSE2(drawspan_t&);
void r_dimpatchD_SSE2(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h);
#endif

#ifdef __MMX__
void R_DrawSpanD_MMX(drawspan_t&);
void R_DrawSlopeSpanD_MMX(drawspan_t&);
void r_dimpatchD_MMX(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h);
#endif

#ifdef __ALTIVEC__
void R_DrawSpanD_ALTIVEC(drawspan_t&);
void R_DrawSlopeSpanD_ALTIVEC(drawspan_t&);
void r_dimpatchD_ALTIVEC(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h);
#endif

// Vectorizable function pointers:
extern void (*R_DrawSpanD)(drawspan_t&);
extern void (*R_DrawSlopeSpanD)(drawspan_t&);
extern void (*r_dimpatchD)(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h);

extern byte*			translationtables;
extern argb_t           translationRGB[MAXPLAYERS+1][16];

enum
{
	TRANSLATION_Shaded,
	TRANSLATION_Players,
	TRANSLATION_PlayersExtra,
	TRANSLATION_Standard,
	TRANSLATION_LevelScripted,
	TRANSLATION_Decals,

	NUM_TRANSLATION_TABLES
};

#define TRANSLATION(a,b)	(((a)<<8)|(b))

const int MAX_ACS_TRANSLATIONS = 32;



// [RH] Double view pixels by detail mode
void R_DetailDouble (void);

void R_InitBuffer(int width, int height);

// Initialize color translation tables,
//	for player rendering etc.
void R_InitTranslationTables (void);
void R_FreeTranslationTables (void);

void R_CopyTranslationRGB (int fromplayer, int toplayer);

// [RH] Actually create a player's translation table.
void R_BuildPlayerTranslation (int player, int color);

// [Nes] Classic player translation table.
void R_BuildClassicPlayerTranslation (int player, int color);


// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder (void);
void R_DrawBorder (int x1, int y1, int x2, int y2);

// [RH] Added for muliresolution support
void R_InitFuzzTable (void);

#endif


