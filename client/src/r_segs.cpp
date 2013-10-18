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
//		All the clipping: columns, horizontal spans, sky columns.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "m_alloc.h"
#include "m_mempool.h"

#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"

#include "p_local.h"
#include "r_local.h"
#include "r_sky.h"
#include "v_video.h"

#include "m_vectors.h"
#include <math.h>

#include "p_lnspec.h"

// a pool of bytes allocated for sprite clipping arrays
MemoryPool openings(32768);

// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate

static BOOL		segtextured;	// True if any of the segs textures might be visible.
static BOOL		markfloor;		// False if the back side is the same plane.
static BOOL		markceiling;
static bool		didsolidcol;

static texhandle_t	toptexture;
static texhandle_t	bottomtexture;
static texhandle_t	midtexture;
static texhandle_t	maskedmidtexture;

int*			walllights;

//
// regular wall
//
fixed_t			rw_light;		// [RH] Use different scaling for lights
fixed_t			rw_lightstep;

static fixed_t	rw_scale;
static fixed_t	rw_scalestep;
static fixed_t	rw_midtexturemid;
static fixed_t	rw_toptexturemid;
static fixed_t	rw_bottomtexturemid;

extern fixed_t	rw_frontcz1, rw_frontcz2;
extern fixed_t	rw_frontfz1, rw_frontfz2;
extern fixed_t	rw_backcz1, rw_backcz2;
extern fixed_t	rw_backfz1, rw_backfz2;
static bool		rw_hashigh, rw_haslow;

static int walltopf[MAXWIDTH];
static int walltopb[MAXWIDTH];
static int wallbottomf[MAXWIDTH];
static int wallbottomb[MAXWIDTH];

static byte* topcols[MAXWIDTH];
static byte* midcols[MAXWIDTH];
static byte* bottomcols[MAXWIDTH];
static byte** maskedmidcols;

static fixed_t wallscalex[MAXWIDTH];
static int texoffs[MAXWIDTH];

extern fixed_t FocalLengthY;
extern float yfoc;

//
// R_OrthogonalLightnumAdjustment
//
int R_OrthogonalLightnumAdjustment()
{
	// [RH] Only do it if not foggy and allowed
    if (!foggy && !(level.flags & LEVEL_EVENLIGHTING))
	{
		if (curline->linedef->slopetype == ST_HORIZONTAL)
			return -1;
		else if (curline->linedef->slopetype == ST_VERTICAL)
			return 1;
	}
	
	return 0;	// no adjustment for diagonal lines
}

//
// R_FillWallHeightArray
//
// Calculates the wall-texture screen coordinates for a span of columns.
//
static void R_FillWallHeightArray(
	int *array, 
	int start, int stop,
	fixed_t val1, fixed_t val2, 
	float scale1, float scale2)
{
	if (start > stop)
		return;

	float h1 = FIXED2FLOAT(val1 - viewz) * scale1;
	float h2 = FIXED2FLOAT(val2 - viewz) * scale2;
	
	float step = (h2 - h1) / (stop - start + 1);
	float frac = float(centery) - h1;

	for (int i = start; i <= stop; i++)
	{
		array[i] = clamp((int)frac, ceilingclipinitial[0], floorclipinitial[0]);
		frac -= step;
	}
}

//
// R_BlastMaskedSegColumn
//
static inline void R_BlastMaskedSegColumn(void (*drawfunc)(drawcolumn_t&))
{
	if (wallscalex[dcol.x] <= 0)
		return;

	dcol.iscale = 0xffffffffu / unsigned(wallscalex[dcol.x]);
	// TODO: dcol.texturefrac should take y-scaling of textures into account
	dcol.texturefrac = dcol.texturemid + FixedMul((dcol.yl - centery + 1) << FRACBITS, dcol.iscale);

	if (dcol.yl <= dcol.yh)
		drawfunc(dcol);
}

inline void MaskedColumnBlaster()
{
	R_BlastMaskedSegColumn(colfunc);
}


//
// R_BlastSolidSegColumn
//
static inline void R_BlastSolidSegColumn(void (*drawfunc)(drawcolumn_t&))
{
	if (wallscalex[dcol.x] <= 0)
		return;

	dcol.iscale = 0xffffffffu / unsigned(wallscalex[dcol.x]);
	// TODO: dcol.texturefrac should take y-scaling of textures into account
	dcol.texturefrac = dcol.texturemid + FixedMul((dcol.yl - centery + 1) << FRACBITS, dcol.iscale);

	if (dcol.yl <= dcol.yh)
		drawfunc(dcol);
}

inline void SolidColumnBlaster()
{
	R_BlastSolidSegColumn(colfunc);
}

inline void R_ColumnSetup(int x, int* top, int* bottom, tallpost_t** posts, bool calc_light)
{
	if (calc_light)
	{
		int index = MIN(rw_light >> LIGHTSCALESHIFT, MAXLIGHTSCALE - 1);
		dcol.colormap = basecolormap.with(walllights[index]);
	}

	dcol.yl = MAX(top[x], 0);
	dcol.yh = MIN(bottom[x], viewheight - 1);
	dcol.post = posts[x];
}


static inline int R_ColumnRangeMinimumHeight(int start, int stop, int* top)
{
	int minheight = viewheight - 1;
	for (int x = start; x <= stop; x++)
		minheight = MIN(minheight, top[x]);

	return MAX(minheight, 0);
}

static inline int R_ColumnRangeMaximumHeight(int start, int stop, int* bottom)
{
	int maxheight = 0;
	for (int x = start; x <= stop; x++)
		maxheight = MAX(maxheight, bottom[x]);

	return MIN(maxheight, viewheight - 1);
}

//
// R_RenderColumnRange
//
// Renders a range of columns to the screen.
// Columns are written to the screen buffer in 64x64 tiles for better cache
// usage.
//
void R_RenderColumnRange(int start, int stop, int* top, int* bottom, byte** cols, void (*colblast)(), bool calc_light)
{
	if (start > stop)
		return;

	if (calc_light)
	{
		if (fixedlightlev)
		{
			dcol.colormap = basecolormap.with(fixedlightlev);
			calc_light = false;
		}
		else if (fixedcolormap.isValid())
		{
			dcol.colormap = fixedcolormap;	
			calc_light = false;
		}
		else
		{
			if (!walllights)
				walllights = scalelight[0];
		}
	}

	// pre-calculate the color map number for lighting for each screen column 
	static int light_lookup[MAXWIDTH];
	if (calc_light)
	{
		for (int x = start; x <= stop; x++)
		{
			light_lookup[x] = walllights[MIN(rw_light >> LIGHTSCALESHIFT, MAXLIGHTSCALE - 1)];
			rw_light += rw_lightstep;
		}
	}
		
	#define BLOCKBITS 6
	#define BLOCKSIZE (1 << BLOCKBITS)
	#define BLOCKMASK (BLOCKSIZE - 1)

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
				if (calc_light)
					dcol.colormap = basecolormap.with(light_lookup[x]);

				dcol.x = x;
				dcol.yl = MAX(top[x], blockstarty);
				dcol.yh = MIN(bottom[x], blockstopy);
				dcol.source = cols[x];
				dcol.dest = R_CalculateDestination(dcol);
				colblast();
			}
		}
	}
}


//
// R_RenderSolidSegRange
//
// Clips each of the three possible seg tiers of the column (top, mid, and bottom),
// sets the appropriate drawcolumn variables and calls R_RenderColumnRange for each
// tier to render the range of columns.
//
// The clipping of the seg tiers also vertically clips the ceiling and floor
// planes.
//
void R_RenderSolidSegRange(int start, int stop)
{
	static int lower[MAXWIDTH];
	int count = stop - start + 1;
	int initial_light = rw_light;

	if (start > stop)
		return;

	int columnmethod = 2;

	// clip the front of the walls to the ceiling and floor
	for (int x = start; x <= stop; x++)
	{
		walltopf[x] = MAX(walltopf[x], ceilingclip[x]);
		wallbottomf[x] = MIN(wallbottomf[x], floorclip[x]);
	}

	// mark ceiling-plane areas
	if (markceiling)
	{
		for (int x = start; x <= stop; x++)
		{
			int top = MAX(ceilingclip[x], 0);
			int bottom = MIN(MIN(walltopf[x], floorclip[x]) - 1, viewheight - 1);

			if (top <= bottom)
			{
				ceilingplane->top[x] = top;
				ceilingplane->bottom[x] = bottom;
			}
		}
	}

	// mark floor-plane areas
	if (markfloor)
	{
		for (int x = start; x <= stop; x++)
		{
			int top = MAX(MAX(wallbottomf[x], ceilingclip[x]), 0);
			int bottom = MIN(floorclip[x] - 1, viewheight - 1);

			if (top <= bottom)
			{
				floorplane->top[x] = top;
				floorplane->bottom[x] = bottom;
			}
		}
	}

	if (midtexture)		// 1-sided line
	{
		// draw the middle wall tier
		for (int x = start; x <= stop; x++)
			lower[x] = wallbottomf[x] - 1;

		rw_light = initial_light;

		const Texture* texture = texturemanager.getTexture(midtexture);
		dcol.textureheight = texture->getHeight();

		dcol.texturemid = rw_midtexturemid;

		R_RenderColumnRange(start, stop, walltopf, lower, midcols, SolidColumnBlaster, true);

		// indicate that no further drawing can be done in this column
		memcpy(ceilingclip + start, floorclipinitial + start, count * sizeof(*ceilingclip));
		memcpy(floorclip + start, ceilingclipinitial + start, count * sizeof(*floorclip));
	}
	else			// 2-sided line
	{
		if (toptexture)
		{
			// draw the upper wall tier
			rw_light = initial_light;

			for (int x = start; x <= stop; x++)
			{
				walltopb[x] = MAX(MIN(walltopb[x], floorclip[x]), walltopf[x]);
				lower[x] = walltopb[x] - 1;
			}

			const Texture* texture = texturemanager.getTexture(toptexture);
			dcol.textureheight = texture->getHeight();
			dcol.texturemid = rw_toptexturemid;

			R_RenderColumnRange(start, stop, walltopf, lower, topcols, SolidColumnBlaster, true);

			memcpy(ceilingclip + start, walltopb + start, count * sizeof(*ceilingclip));
		}
		else if (markceiling)
		{
			// no upper wall
			memcpy(ceilingclip + start, walltopf + start, count * sizeof(*ceilingclip));
		}

		if (bottomtexture)
		{
			// draw the lower wall tier
			rw_light = initial_light;

			for (int x = start; x <= stop; x++)
			{
				wallbottomb[x] = MIN(MAX(wallbottomb[x], ceilingclip[x]), wallbottomf[x]);
				lower[x] = wallbottomf[x] - 1;
			}

			const Texture* texture = texturemanager.getTexture(bottomtexture);
			dcol.textureheight = texture->getHeight();
			dcol.texturemid = rw_bottomtexturemid;

			R_RenderColumnRange(start, stop, wallbottomb, lower, bottomcols, SolidColumnBlaster, true);

			memcpy(floorclip + start, wallbottomb + start, count * sizeof(*floorclip));
		}
		else if (markfloor)
		{
			// no lower wall
			memcpy(floorclip + start, wallbottomf + start, count * sizeof(*floorclip));
		}

		if (maskedmidtexture)
		{
			memcpy(maskedmidcols + start, midcols + start, sizeof(*maskedmidcols) * (stop - start + 1));
		}
	}

	for (int x = start; x <= stop; x++)
	{
		// cph - if we completely blocked further sight through this column,
		// add this info to the solid columns array
		if ((markceiling || markfloor) && (floorclip[x] <= ceilingclip[x]))
		{
			solidcol[x] = 1;
			didsolidcol = true;
		}
	}
}

void R_RenderMaskedSegRange(drawseg_t* ds, int x1, int x2)
{
	curline = ds->curline;

	// killough 4/11/98: draw translucent 2s normal textures
	// [RH] modified because we don't use user-definable
	//		translucency maps
	if (curline->linedef->lucency < 240)
	{
		R_SetLucentDrawFuncs();
		dcol.translevel = curline->linedef->lucency << 8;
	}
	else
	{
		R_ResetDrawFuncs();
	}

	frontsector = curline->frontsector;
	backsector = curline->backsector;

	// TODO: texturetranslation[]
	const Texture* texture = texturemanager.getTexture(curline->sidedef->_midtexture);
	fixed_t texheight = texture->getHeight();

	fixed_t scalefrac = ds->scale1 + (x1 - ds->x1) * ds->scalestep;
	fixed_t scalestep = ds->scalestep;

	static int top[MAXWIDTH];
	static int bottom[MAXWIDTH];

	if (curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		dcol.texturemid = MAX(P_FloorHeight(frontsector), P_FloorHeight(backsector))
						+ texheight - viewz + curline->sidedef->rowoffset;

		for (int x = x1; x <= x2; x++)
		{
			top[x] = MAX((centeryfrac - FixedMul(dcol.texturemid, scalefrac)) >> FRACBITS, ds->sprtopclip[x]);
			bottom[x] = ds->sprbottomclip[x] - 1;

			wallscalex[x] = scalefrac;
			scalefrac += scalestep;
		}

	}
	else
	{
		dcol.texturemid = MIN(P_CeilingHeight(frontsector), P_CeilingHeight(backsector))
						- viewz + curline->sidedef->rowoffset;

		for (int x = x1; x <= x2; x++)
		{
			top[x] = ds->sprtopclip[x];
			bottom[x] = MIN((centeryfrac - FixedMul(dcol.texturemid - texheight, scalefrac)) >> FRACBITS, ds->sprbottomclip[x]) - 1;

			wallscalex[x] = scalefrac;
			scalefrac += scalestep;
		}
	}

	if ((bottom[x1] < 0 && bottom[x2] < 0) ||
		(top[x1] >= viewheight && top[x2] >= viewheight))
		return;

	maskedmidcols = ds->maskedmidcols;
	dcol.color = (dcol.color + 4) & 0xFF;	// color if using r_drawflat
	dcol.textureheight = 256*FRACUNIT;

	basecolormap = frontsector->floorcolormap->maps;	// [RH] Set basecolormap

	// killough 4/13/98: get correct lightlevel for 2s normal textures
	sector_t tempsec;
	int lightnum = (R_FakeFlat(frontsector, &tempsec, NULL, NULL, false)
			->lightlevel >> LIGHTSEGSHIFT) + (foggy ? 0 : extralight);

	lightnum += R_OrthogonalLightnumAdjustment();

	walllights = lightnum >= LIGHTLEVELS ? scalelight[LIGHTLEVELS-1] :
		lightnum <  0 ? scalelight[0] : scalelight[lightnum];

	rw_lightstep = ds->lightstep;
	rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

	// draw the columns
	R_RenderColumnRange(x1, x2, top, bottom, maskedmidcols, MaskedColumnBlaster, true);
}


static fixed_t R_LineLength(fixed_t px1, fixed_t py1, fixed_t px2, fixed_t py2)
{
	float dx = FIXED2FLOAT(px2 - px1);
	float dy = FIXED2FLOAT(py2 - py1);

	return FLOAT2FIXED(sqrt(dx*dx + dy*dy));
}

//
// R_PrepWall
//
// Prepares a lineseg for rendering. It fills the walltopf, wallbottomf,
// walltopb, and wallbottomb arrays with the top and bottom pixel heights
// of the wall for the span from start to stop.
//
// It also fills in the wallscalex and texoffs arrays with the vertical
// scaling for each column and the horizontal texture offset for each column
// respectively.
//
void R_PrepWall(fixed_t px1, fixed_t py1, fixed_t px2, fixed_t py2, fixed_t dist1, fixed_t dist2, int start, int stop)
{
	int width = stop - start + 1;
	if (width <= 0)
		return;

	// Also these are masking the global toptexture/midtexture/bottomtexture
	// declared in this file
	const Texture* toptexture = NULL;
	const Texture* midtexture = NULL;
	const Texture* bottomtexture = NULL;

	// TODO: texturetranslation[]
	if (curline->sidedef->_toptexture)
		toptexture = texturemanager.getTexture(curline->sidedef->_toptexture);
	if (curline->sidedef->_midtexture)
		midtexture = texturemanager.getTexture(curline->sidedef->_midtexture);
	if (curline->sidedef->_bottomtexture)
		bottomtexture = texturemanager.getTexture(curline->sidedef->_bottomtexture);

	// determine which vertex of the linedef should be used for texture alignment
	vertex_t *v1;
	if (curline->linedef->sidenum[0] == curline->sidedef - sides)
		v1 = curline->linedef->v1;
	else
		v1 = curline->linedef->v2;

	// clipped lineseg length
	fixed_t seglen = R_LineLength(px1, py1, px2, py2);

	// distance from lineseg start to start of clipped lineseg
	fixed_t segoffs = R_LineLength(v1->x, v1->y, px1, py1) + curline->sidedef->textureoffset;

	const fixed_t mindist = NEARCLIP;
	const fixed_t maxdist = 16384*FRACUNIT;
	dist1 = clamp(dist1, mindist, maxdist);
	dist2 = clamp(dist2, mindist, maxdist);

	// calculate texture coordinates at the line's endpoints
	float scale1 = yfoc / FIXED2FLOAT(dist1);
	float scale2 = yfoc / FIXED2FLOAT(dist2);

	// [SL] Quick note on texture mapping: we can not linearly interpolate along the length of the seg
	// as it will yield evenly spaced texels instead of correct perspective (taking depth Z into account).
	// We also can not linearly interpolate Z, but we can linearly interpolate 1/Z (scale), so we linearly
	// interpolate the texture coordinates u / Z and then divide by 1/Z to get the correct u for each column.

	float scalestep = (scale2 - scale1) / width;
	float uinvzstep = FIXED2FLOAT(seglen) * scale2 / width;

	// determine which texture posts will be used for each screen
	// column in this range and calculate the scaling factor for
	// each column.

	float uinvz = 0.0f;
	float curscale = scale1;
	for (int x = start; x <= stop; x++)
	{
		wallscalex[x] = FLOAT2FIXED(curscale);

		fixed_t colfrac = segoffs + FLOAT2FIXED(uinvz / curscale);
		texoffs[x] = colfrac;
		
		if (toptexture)
		{
			fixed_t colnum = FixedMul(colfrac, toptexture->getScaleX()); 
			topcols[x] = toptexture->getColumnDataTiled(colnum);
		}
		if (midtexture)
		{
			fixed_t colnum = FixedMul(colfrac, midtexture->getScaleX()); 
			midcols[x] = midtexture->getColumnDataTiled(colnum);
		}
		if (bottomtexture)
		{
			fixed_t colnum = FixedMul(colfrac, bottomtexture->getScaleX()); 
			bottomcols[x] = bottomtexture->getColumnDataTiled(colnum);
		}

		uinvz += uinvzstep;
		curscale += scalestep;
	}

	// get the z coordinates of the line's vertices on each side of the line
	rw_frontcz1 = P_CeilingHeight(px1, py1, frontsector);
	rw_frontfz1 = P_FloorHeight(px1, py1, frontsector);
	rw_frontcz2 = P_CeilingHeight(px2, py2, frontsector);
	rw_frontfz2 = P_FloorHeight(px2, py2, frontsector);

	// calculate the upper and lower heights of the walls in the front
	R_FillWallHeightArray(walltopf, start, stop, rw_frontcz1, rw_frontcz2, scale1, scale2);
	R_FillWallHeightArray(wallbottomf, start, stop, rw_frontfz1, rw_frontfz2, scale1, scale2);

	rw_hashigh = rw_haslow = false;

	if (backsector)
	{
		rw_backcz1 = P_CeilingHeight(px1, py1, backsector);
		rw_backfz1 = P_FloorHeight(px1, py1, backsector);
		rw_backcz2 = P_CeilingHeight(px2, py2, backsector);
		rw_backfz2 = P_FloorHeight(px2, py2, backsector);

		// calculate the upper and lower heights of the walls in the back
		R_FillWallHeightArray(walltopb, start, stop, rw_backcz1, rw_backcz2, scale1, scale2);
		R_FillWallHeightArray(wallbottomb, start, stop, rw_backfz1, rw_backfz2, scale1, scale2);
	
		const fixed_t tolerance = FRACUNIT/2;

		// determine if an upper texture is showing
		rw_hashigh	= (P_CeilingHeight(curline->v1->x, curline->v1->y, frontsector) - tolerance >
					   P_CeilingHeight(curline->v1->x, curline->v1->y, backsector)) ||
					  (P_CeilingHeight(curline->v2->x, curline->v2->y, frontsector) - tolerance>
					   P_CeilingHeight(curline->v2->x, curline->v2->y, backsector));

		// determine if a lower texture is showing
		rw_haslow	= (P_FloorHeight(curline->v1->x, curline->v1->y, frontsector) + tolerance <
					   P_FloorHeight(curline->v1->x, curline->v1->y, backsector)) ||
					  (P_FloorHeight(curline->v2->x, curline->v2->y, frontsector) + tolerance <
					   P_FloorHeight(curline->v2->x, curline->v2->y, backsector));

		// hack to allow height changes in outdoor areas (sky hack)
		// copy back ceiling height array to front ceiling height array
		if (frontsector->ceiling_texhandle == sky1flathandle && 
			backsector->ceiling_texhandle == sky1flathandle)
			memcpy(walltopf+start, walltopb+start, width*sizeof(*walltopb));
	}

	rw_scalestep = FLOAT2FIXED(scalestep);
}

//
// R_StoreWallRange
// A wall segment will be drawn
//	between start and stop pixels (inclusive).
//
void R_StoreWallRange(int start, int stop)
{
#ifdef RANGECHECK
	if (start >= viewwidth || start > stop)
		I_FatalError ("Bad R_StoreWallRange: %i to %i", start , stop);
#endif

	int count = stop - start + 1;
	if (count <= 0)
		return;

	R_ReallocDrawSegs();	// don't overflow and crash

	sidedef = curline->sidedef;
	linedef = curline->linedef;

	// mark the segment as visible for auto map
	linedef->flags |= ML_MAPPED;

	ds_p->x1 = start;
	ds_p->x2 = stop;
	ds_p->curline = curline;

	// calculate scale at both ends and step
	ds_p->scale1 = rw_scale = wallscalex[start];
	ds_p->scale2 = wallscalex[stop];
	ds_p->scalestep = rw_scalestep;

	ds_p->light = rw_light = rw_scale * lightscalexmul;
 	ds_p->lightstep = rw_lightstep = rw_scalestep * lightscalexmul;

	// calculate texture boundaries
	//	and decide if floor / ceiling marks are needed
	midtexture = toptexture = bottomtexture = maskedmidtexture = 0;
	ds_p->maskedmidcols = NULL;

	if (!backsector)
	{
		// single sided line
		// TODO: texturetranslation[]
		midtexture = sidedef->_midtexture;

		// a single sided line is terminal, so it must mark ends
		markfloor = markceiling = true;

		if (linedef->flags & ML_DONTPEGBOTTOM)
		{
			// bottom of texture at bottom
			const Texture* texture = texturemanager.getTexture(midtexture);
			fixed_t texheight = FixedMul(texture->getHeight(), texture->getScaleY());
			rw_midtexturemid = P_FloorHeight(frontsector) - viewz + texheight;
		}
		else
		{
			// top of texture at top
			rw_midtexturemid = P_CeilingHeight(frontsector) - viewz;
		}

		rw_midtexturemid += sidedef->rowoffset;

		ds_p->silhouette = SIL_BOTH;
		ds_p->sprtopclip = viewheightarray;
		ds_p->sprbottomclip = negonearray;
	}
	else
	{
		// two sided line
		ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
		ds_p->silhouette = 0;

		extern bool doorclosed;	
		if (doorclosed)
		{
			// clip all sprites behind this closed door (or otherwise solid line)
			ds_p->silhouette = SIL_BOTH;
			ds_p->sprtopclip = viewheightarray;
			ds_p->sprbottomclip = negonearray;
		}
		else
		{
			// determine sprite clipping for non-solid line segs	
			if (rw_frontfz1 > rw_backfz1 || rw_frontfz2 > rw_backfz2 || 
				rw_backfz1 > viewz || rw_backfz2 > viewz || 
				!P_IsPlaneLevel(&backsector->floorplane))	// backside sloping?
				ds_p->silhouette |= SIL_BOTTOM;

			if (rw_frontcz1 < rw_backcz1 || rw_frontcz2 < rw_backcz2 ||
				rw_backcz1 < viewz || rw_backcz2 < viewz || 
				!P_IsPlaneLevel(&backsector->ceilingplane))	// backside sloping?
				ds_p->silhouette |= SIL_TOP;
		}

		if (doorclosed)
		{
			markceiling = markfloor = true;
		}
		else if (spanfunc == R_FillSpan)
		{
			markfloor = markceiling = (frontsector != backsector);
		}
		else
		{
			markfloor =
				  !P_IdenticalPlanes(&backsector->floorplane, &frontsector->floorplane)
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->floor_texhandle != frontsector->floor_texhandle

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->floor_xoffs != frontsector->floor_xoffs
				|| (backsector->floor_yoffs + backsector->base_floor_yoffs) != 
				   (frontsector->floor_yoffs + frontsector->base_floor_yoffs)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through deep water
				|| frontsector->heightsec

				// killough 4/17/98: draw floors if different light levels
				|| backsector->floorlightsec != frontsector->floorlightsec

				// [RH] Add checks for colormaps
				|| backsector->floorcolormap != frontsector->floorcolormap

				|| backsector->floor_xscale != frontsector->floor_xscale
				|| backsector->floor_yscale != frontsector->floor_yscale

				|| (backsector->floor_angle + backsector->base_floor_angle) !=
				   (frontsector->floor_angle + frontsector->base_floor_angle)
				;

			markceiling = 
				  !P_IdenticalPlanes(&backsector->ceilingplane, &frontsector->ceilingplane)
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->ceiling_texhandle != frontsector->ceiling_texhandle

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->ceiling_xoffs != frontsector->ceiling_xoffs
				|| (backsector->ceiling_yoffs + backsector->base_ceiling_yoffs) !=
				   (frontsector->ceiling_yoffs + frontsector->base_ceiling_yoffs)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through fake ceilings
				|| (frontsector->heightsec && frontsector->ceiling_texhandle != sky1flathandle)

				// killough 4/17/98: draw ceilings if different light levels
				|| backsector->ceilinglightsec != frontsector->ceilinglightsec

				// [RH] Add check for colormaps
				|| backsector->ceilingcolormap != frontsector->ceilingcolormap

				|| backsector->ceiling_xscale != frontsector->ceiling_xscale
				|| backsector->ceiling_yscale != frontsector->ceiling_yscale

				|| (backsector->ceiling_angle + backsector->base_ceiling_angle) !=
				   (frontsector->ceiling_angle + frontsector->base_ceiling_angle)
				;
				
			// Sky hack
			markceiling = markceiling &&
				(frontsector->ceiling_texhandle != sky1flathandle ||
				backsector->ceiling_texhandle != sky1flathandle);
		}

		if (rw_hashigh)
		{
			// TODO: texturetranslation[]
			toptexture = sidedef->_toptexture;
			if (linedef->flags & ML_DONTPEGTOP)
			{
				// top of texture at top
				rw_toptexturemid = P_CeilingHeight(frontsector) - viewz;
			}
			else
			{
				// bottom of texture
				fixed_t texheight = 0;
				if (toptexture)
					texheight = texturemanager.getTexture(toptexture)->getHeight();
				rw_toptexturemid = P_CeilingHeight(backsector) - viewz + texheight;
			}

			rw_toptexturemid += sidedef->rowoffset;
		}

		if (rw_haslow)
		{
			// TODO: texturetranslation[]
			bottomtexture = sidedef->_bottomtexture;
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{
				// bottom of texture at bottom, top of texture at top
				rw_bottomtexturemid = P_CeilingHeight(frontsector) - viewz;
			}
			else
			{
				// top of texture at top
				rw_bottomtexturemid = P_FloorHeight(backsector) - viewz;
			}
		
			rw_bottomtexturemid += sidedef->rowoffset;
		}

		// TODO: texturetranslation[]
		maskedmidtexture = sidedef->_midtexture;
		if (maskedmidtexture)
		{
			// masked midtexture
			ds_p->maskedmidcols = maskedmidcols = openings.alloc<byte*>(count) - start;
			markfloor = markceiling = true;
		}

		// [SL] additional fix for sky hack
		if (frontsector->ceiling_texhandle == sky1flathandle &&
			backsector->ceiling_texhandle == sky1flathandle)
			toptexture = 0;
	}

	// [SL] 2012-01-24 - Horizon line extends to infinity by scaling the wall
	// height to 0
	if (curline->linedef->special == Line_Horizon)
	{
		rw_scale = ds_p->scale1 = ds_p->scale2 = rw_scalestep = ds_p->light = rw_light = 0;
		midtexture = toptexture = bottomtexture = maskedmidtexture = 0;

		for (int n = start; n <= stop; n++)
			walltopf[n] = wallbottomf[n] = centery;
	}

	segtextured = (midtexture | toptexture) | (bottomtexture | maskedmidtexture);

	if (segtextured)
	{
		// calculate light table
		//	use different light tables
		//	for horizontal / vertical / diagonal
		// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
		if (!fixedcolormap.isValid())
		{
			int lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)
					+ (foggy ? 0 : extralight);

			lightnum += R_OrthogonalLightnumAdjustment();

			if (lightnum < 0)
				walllights = scalelight[0];
			else if (lightnum >= LIGHTLEVELS)
				walllights = scalelight[LIGHTLEVELS-1];
			else
				walllights = scalelight[lightnum];
		}
	}

	// if a floor / ceiling plane is on the wrong side
	//	of the view plane, it is definitely invisible
	//	and doesn't need to be marked.

	// killough 3/7/98: add deep water check
	if (frontsector->heightsec == NULL ||
		(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		// above view plane?
		if (P_FloorHeight(viewx, viewy, frontsector) >= viewz)       
			markfloor = false;
		// below view plane?
		if (P_CeilingHeight(viewx, viewy, frontsector) <= viewz &&
			frontsector->ceiling_texhandle != sky1flathandle)   
			markceiling = false;	
	}

	// render it
	if (markceiling && ceilingplane)
		ceilingplane = R_CheckPlane(ceilingplane, start, stop);
	else
		markceiling = false;

	if (markfloor && floorplane)
		floorplane = R_CheckPlane(floorplane, start, stop);
	else
		markfloor = false;

	didsolidcol = false;

	R_RenderSolidSegRange(start, stop);

	// [SL] save full clipping info for masked midtextures
	// cph - if a column was made solid by this wall, we _must_ save full clipping info
	if (maskedmidtexture || (backsector && didsolidcol))
		ds_p->silhouette = SIL_BOTH;

    // save sprite & masked seg clipping info
	if ((ds_p->silhouette & SIL_TOP) && ds_p->sprtopclip == NULL)
	{
		ds_p->sprtopclip = openings.alloc<int>(count) - start;
		memcpy(ds_p->sprtopclip + start, ceilingclip + start, count * sizeof(*ds_p->sprtopclip));
	}

	if ((ds_p->silhouette & SIL_BOTTOM) && ds_p->sprbottomclip == NULL)
	{
		ds_p->sprbottomclip = openings.alloc<int>(count) - start;
		memcpy(ds_p->sprbottomclip + start, floorclip + start, count * sizeof(*ds_p->sprbottomclip));
	}

	ds_p++;
}


void R_ClearOpenings()
{
	openings.clear();
}

VERSION_CONTROL (r_segs_cpp, "$Id$")

