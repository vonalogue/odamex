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

static bool		markfloor;		// False if the back side is the same plane.
static bool		markceiling;
static bool		didsolidcol;

static texhandle_t	toptexture;
static texhandle_t	bottomtexture;
static texhandle_t	midtexture;
static texhandle_t	maskedmidtexture;

int*			walllights;

//
// regular wall
//

static fixed_t	rw_midtexturemid;
static fixed_t	rw_toptexturemid;
static fixed_t	rw_bottomtexturemid;

static bool		rw_hashigh, rw_haslow;

static int walltopf[MAXWIDTH];
static int walltopb[MAXWIDTH];
static int wallbottomf[MAXWIDTH];
static int wallbottomb[MAXWIDTH];

extern fixed_t FocalLengthY;
extern float yfoc;

// [SL] global seg texture mapping parameters for the current seg_t
static drawseg_t rw;

// [SL] global vertices fro the current seg_t
static wall_t rw_wall;

//
// Texture mapping functor for wall segs
// Used with R_DrawColumnRange
//
// [SL] Quick note on texture mapping: we can not linearly interpolate along the length of the seg
// as it will yield evenly spaced texels instead of correct perspective (taking depth Z into account).
// We also can not linearly interpolate Z, but we can linearly interpolate 1/Z (scale), so we linearly
// interpolate the texture coordinates u / Z and then divide by 1/Z to get the correct u for each column.
//
class SegTextureMapper
{
public:
	SegTextureMapper(const drawseg_t* ds, const Texture* texture) :
		mInvZ(ds->invz1), mInvZStep(ds->invzstep), mUInvZ(ds->uinvz1), mUInvZStep(ds->uinvzstep)
	{
		// use 2.30 fixed-point format for mInvFocY
		mInvFocY = FixedDiv<0, 16, 30>(1, FocalLengthY);

		mHeight = texture->getHeight();
		mWidthMask = texture->getWidth() - 1; 
		mData = texture->getData();

		calc();
	}

	inline void next()
	{
		mInvZ += mInvZStep;
		mUInvZ += mUInvZStep;
		calc();
	}

	inline const byte* getData() const
	{
		// TODO: take texture x-scaling into account
		return mData + mHeight * ((mU >> FRACBITS) & mWidthMask);
	}

	inline fixed_t getIScale() const
	{
		// TODO: take texture y-scaling into account
		return mIScale;
	}

private:
	inline void calc()
	{
		// use 16.16 fixed-point format for z
		int32_t z = FixedDiv<0, 28, 16>(1, mInvZ);
		mU = FixedMul<20, 16, 16>(mUInvZ, z);
		mIScale = FixedMul<16, 30, 16>(z, mInvFocY);
	}

	int32_t			mInvZ;
	int32_t			mInvZStep;
	fixed_t			mUInvZ;
	fixed_t			mUInvZStep;

	int32_t			mInvFocY;

	fixed_t			mU;
	fixed_t			mIScale;

	unsigned int	mHeight;
	unsigned int	mWidthMask;
	const byte*		mData;
};

//
// R_HasMaskedMidTexture
//
bool R_HasMaskedMidTexture(const seg_t* line)
{
	return line->backsector && line->sidedef->midtexture;
}


static fixed_t R_CalculateMidTextureMid(const drawseg_t* ds)
{
	const texhandle_t texhandle = ds->curline->sidedef->midtexture;
	fixed_t result;

	if (ds->curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		// bottom of texture at bottom
		const Texture* texture = texturemanager.getTexture(texhandle);
		fixed_t texheight = FixedMul(texture->getHeight() << FRACBITS, texture->getScaleY());
		result = P_FloorHeight(ds->frontsector) - viewz + texheight;
	}
	else
	{
		// top of texture at top
		result = P_CeilingHeight(ds->frontsector) - viewz;
	}

	result += ds->curline->sidedef->rowoffset;
	return result;
}

static fixed_t R_CalculateTopTextureMid(const drawseg_t* ds)
{
	const texhandle_t texhandle = ds->curline->sidedef->toptexture;
	fixed_t result;

	if (ds->curline->linedef->flags & ML_DONTPEGTOP)
	{
		// top of texture at top
		result = P_CeilingHeight(ds->frontsector) - viewz;
	}
	else
	{
		// bottom of texture
		fixed_t texheight = 0;
		if (texhandle)
			texheight = texturemanager.getTexture(texhandle)->getHeight() << FRACBITS;
		result = P_CeilingHeight(ds->backsector) - viewz + texheight;
	}

	result += ds->curline->sidedef->rowoffset;
	return result;
}

static fixed_t R_CalculateBottomTextureMid(const drawseg_t* ds)
{
	fixed_t result;

	if (ds->curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		// bottom of texture at bottom, top of texture at top
		result = P_CeilingHeight(ds->frontsector) - viewz;
	}
	else
	{
		// top of texture at top
		result = P_FloorHeight(ds->backsector) - viewz;
	}

	result += ds->curline->sidedef->rowoffset;
	return result;
}


//
// R_SelectColormapTable
//
// Generates a colormap table for each column in the range, or returns the
// appropriate table for fixed-colormap or fixed-lightlev.
//
static inline const shaderef_t* R_SelectColormapTable(const drawseg_t* ds)
{
	if (fixedlightlev)
		return fixed_light_colormap_table;
	else if (fixedcolormap.isValid())
		return fixed_colormap_table;

	static shaderef_t colormap_table[MAXWIDTH];

	// calculate light table
	//	use different light tables
	//	for horizontal / vertical / diagonal
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
	int adjustment = 0;
	if (!foggy && !(level.flags & LEVEL_EVENLIGHTING))
	{
		if (ds->curline->linedef->slopetype == ST_HORIZONTAL)
			adjustment = -1;
		else if (ds->curline->linedef->slopetype == ST_VERTICAL)
			adjustment = 1;
	}

	int lightnum = (ds->frontsector->lightlevel >> LIGHTSEGSHIFT)
				+ (foggy ? 0 : extralight) + adjustment;

	walllights = scalelight[clamp(lightnum, 0, LIGHTLEVELS - 1)];	

	int light = ds->light;
	int lightstep = ds->lightstep;

	for (int x = ds->x1; x <= ds->x2; x++)
	{
		colormap_table[x] = basecolormap.with(walllights[MIN(light >> LIGHTSCALESHIFT, MAXLIGHTSCALE - 1)]);
		light += lightstep;
	}

	return colormap_table; 
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
// R_RenderSolidSegRange
//
// Clips each of the three possible seg tiers of the column (top, mid, and bottom),
// sets the appropriate drawcolumn variables and calls R_RenderColumnRange for each
// tier to render the range of columns.
//
// The clipping of the seg tiers also vertically clips the ceiling and floor
// planes.
//
void R_RenderSolidSegRange(const drawseg_t* ds)
{
	int start = ds->x1;
	int stop = ds->x2;
	int count = stop - start + 1;

	static int lower[MAXWIDTH];

	// generate the light table
	const shaderef_t* colormap_table = R_SelectColormapTable(ds);

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

	if (midtexture)	// 1-sided line
	{
		// draw the middle wall tier
		for (int x = start; x <= stop; x++)
			lower[x] = wallbottomf[x] - 1;

		const Texture* texture = texturemanager.getTexture(midtexture);
		dcol.textureheight = texture->getHeight() << FRACBITS;
		dcol.texturemid = rw_midtexturemid;

		SegTextureMapper mapper(ds, texture);
		R_DrawColumnRange<SegTextureMapper>(start, stop, walltopf, lower,
						texture, mapper, colormap_table, colfunc);

		// indicate that no further drawing can be done in this column
		memcpy(ceilingclip + start, floorclipinitial + start, count * sizeof(*ceilingclip));
		memcpy(floorclip + start, ceilingclipinitial + start, count * sizeof(*floorclip));
	}
	else			// 2-sided line
	{
		if (toptexture)
		{
			// draw the upper wall tier
			for (int x = start; x <= stop; x++)
			{
				walltopb[x] = MAX(MIN(walltopb[x], floorclip[x]), walltopf[x]);
				lower[x] = walltopb[x] - 1;
			}

			const Texture* texture = texturemanager.getTexture(toptexture);
			dcol.textureheight = texture->getHeight() << FRACBITS;
			dcol.texturemid = rw_toptexturemid;

			SegTextureMapper mapper(ds, texture);
			R_DrawColumnRange<SegTextureMapper>(start, stop, walltopf, lower,
						texture, mapper, colormap_table, colfunc);

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
			for (int x = start; x <= stop; x++)
			{
				wallbottomb[x] = MIN(MAX(wallbottomb[x], ceilingclip[x]), wallbottomf[x]);
				lower[x] = wallbottomf[x] - 1;
			}

			const Texture* texture = texturemanager.getTexture(bottomtexture);
			dcol.textureheight = texture->getHeight() << FRACBITS;
			dcol.texturemid = rw_bottomtexturemid;

			SegTextureMapper mapper(ds, texture);
			R_DrawColumnRange<SegTextureMapper>(start, stop, wallbottomb, lower,
						texture, mapper, colormap_table, colfunc);

			memcpy(floorclip + start, wallbottomb + start, count * sizeof(*floorclip));
		}
		else if (markfloor)
		{
			// no lower wall
			memcpy(floorclip + start, wallbottomf + start, count * sizeof(*floorclip));
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

void R_RenderMaskedSegRange(const drawseg_t* ds, int start, int stop)
{
	// killough 4/11/98: draw translucent 2s normal textures
	// [RH] modified because we don't use user-definable
	//		translucency maps
	if (ds->curline->linedef->lucency < 240)
	{
		R_SetLucentDrawFuncs();
		dcol.translevel = ds->curline->linedef->lucency << 8;
	}
	else
	{
		R_ResetDrawFuncs();
	}

	side_t* sidedef = ds->curline->sidedef;
	line_t* linedef = ds->curline->linedef;

	frontsector = ds->frontsector;
	backsector = ds->backsector;

	const Texture* texture = texturemanager.getTexture(sidedef->midtexture);
	fixed_t texheight = texture->getHeight() << FRACBITS;

	fixed_t scalefrac = ds->scale1;
	fixed_t scalestep = ds->scalestep;

	static int top[MAXWIDTH];
	static int bottom[MAXWIDTH];

	if (linedef->flags & ML_DONTPEGBOTTOM)
	{
		dcol.texturemid = MAX(P_FloorHeight(ds->frontsector), P_FloorHeight(ds->backsector))
						+ texheight - viewz + sidedef->rowoffset;

		for (int x = start; x <= stop; x++)
		{
			int top1 = (centeryfrac - FixedMul(dcol.texturemid, scalefrac)) >> FRACBITS;
			int top2 = ds->sprtopclip[x];
			top[x] = MAX(top1, top2);
			int bottom1 = ds->sprbottomclip[x];
			int bottom2 = top1 + (FixedMul(texheight, scalefrac) >> FRACBITS);
			bottom[x] = MIN(bottom1, bottom2) - 1;
			scalefrac += scalestep;
		}
	}
	else
	{
		dcol.texturemid = MIN(P_CeilingHeight(ds->frontsector), P_CeilingHeight(ds->backsector))
						- viewz + sidedef->rowoffset;

		for (int x = start; x <= stop; x++)
		{
			int bottom1 = (centeryfrac - FixedMul(dcol.texturemid - texheight, scalefrac)) >> FRACBITS;
			int bottom2 = ds->sprbottomclip[x];
			bottom[x] = MIN(bottom1, bottom2) - 1;
			int top1 = bottom1 - (FixedMul(texheight, scalefrac) >> FRACBITS);
			int top2 = ds->sprtopclip[x];
			top[x] = MAX(top1, top2); 
			scalefrac += scalestep;
		}
	}

	// clip entire drawseg if it is above or below the screen
	if ((bottom[start] < 0 && bottom[stop] < 0) || (top[start] >= viewheight && top[stop] >= viewheight))
		return;

	dcol.color = (dcol.color + 4) & 0xFF;	// color if using r_drawflat

	basecolormap = ds->frontsector->floorcolormap->maps;	// [RH] Set basecolormap

	// generate the light table
	const shaderef_t* colormap_table = R_SelectColormapTable(ds);

	SegTextureMapper mapper(ds, texture);
	R_DrawColumnRange<SegTextureMapper>(start, stop, top, bottom,
				texture, mapper, colormap_table, maskedcolfunc);
}


//
// R_PrepWall
//
// Prepares a lineseg for rendering. It fills the walltopf, wallbottomf,
// walltopb, and wallbottomb arrays with the top and bottom pixel heights
// of the wall for the span from start to stop.
//
void R_PrepWall(const drawseg_t* ds, const wall_t* wall)
{
	int width = ds->x2 - ds->x1 + 1;
	if (width <= 0)
		return;

	// calculate texture coordinates at the line's endpoints
	float scale1 = FIXED2FLOAT(ds->scale1);
	float scale2 = FIXED2FLOAT(ds->scale2);

	rw = *ds;
	rw_wall = *wall;

	// calculate the upper and lower heights of the walls in the front
	R_FillWallHeightArray(walltopf, ds->x1, ds->x2, wall->frontc1.z, wall->frontc2.z, scale1, scale2);
	R_FillWallHeightArray(wallbottomf, ds->x1, ds->x2, wall->frontf1.z, wall->frontf2.z, scale1, scale2);

	rw_hashigh = rw_haslow = false;

	if (wall->twosided)
	{
		// calculate the upper and lower heights of the walls in the back
		R_FillWallHeightArray(walltopb, ds->x1, ds->x2, wall->backc1.z, wall->backc2.z, scale1, scale2);
		R_FillWallHeightArray(wallbottomb, ds->x1, ds->x2, wall->backf1.z, wall->backf2.z, scale1, scale2);
	
		const fixed_t tolerance = FRACUNIT/2;

		// determine if an upper texture is showing
		rw_hashigh	= (P_CeilingHeight(ds->curline->v1->x, ds->curline->v1->y, ds->frontsector) - tolerance >
					   P_CeilingHeight(ds->curline->v1->x, ds->curline->v1->y, ds->backsector)) ||
					  (P_CeilingHeight(ds->curline->v2->x, ds->curline->v2->y, ds->frontsector) - tolerance>
					   P_CeilingHeight(ds->curline->v2->x, ds->curline->v2->y, ds->backsector));

		// determine if a lower texture is showing
		rw_haslow	= (P_FloorHeight(ds->curline->v1->x, ds->curline->v1->y, ds->frontsector) + tolerance <
					   P_FloorHeight(ds->curline->v1->x, ds->curline->v1->y, ds->backsector)) ||
					  (P_FloorHeight(ds->curline->v2->x, ds->curline->v2->y, ds->frontsector) + tolerance <
					   P_FloorHeight(ds->curline->v2->x, ds->curline->v2->y, ds->backsector));

		// hack to allow height changes in outdoor areas (sky hack)
		// copy back ceiling height array to front ceiling height array
		if (ds->frontsector->ceiling_texhandle == sky1flathandle && ds->backsector->ceiling_texhandle == sky1flathandle)
			memcpy(walltopf+ds->x1, walltopb+ds->x1, width*sizeof(*walltopb));
	}
}



//
// R_StoreWallRange
// A wall segment will be drawn
//	between start and stop pixels (inclusive).
//
void R_StoreWallRange(drawseg_t* ds, int start, int stop)
{
#ifdef RANGECHECK
	if (start >= viewwidth || start > stop)
		I_FatalError ("Bad R_StoreWallRange: %i to %i", start , stop);
#endif

	int width = stop - start + 1;
	if (width <= 0)
		return;

	side_t* sidedef = rw.curline->sidedef;
	line_t* linedef = rw.curline->linedef;

	// mark the segment as visible for auto map
	linedef->flags |= ML_MAPPED;

	ds->curline = rw.curline;
	ds->frontsector = rw.frontsector;
	ds->backsector = rw.backsector;

	// clip the rw drawseg to the range between start and stop
	ds->x1 = start;
	ds->x2 = stop;

	int clipx1 = ds->x1 - rw.x1;
	int clipx2 = rw.x2 - ds->x2;

	if (clipx1 || clipx2)
	{
		if (ds->x2 > ds->x1)
		{
			const int32_t stepfactor = FixedDiv<0, 0, 30>(1, ds->x2 - ds->x1);

			ds->invz1 = rw.invz1 + clipx1 * rw.invzstep;
			ds->invz2 = rw.invz2 - clipx2 * rw.invzstep;
			ds->invzstep = FixedMul<28, 30, 28>(ds->invz2 - ds->invz1, stepfactor); 

			ds->scale1 = rw.scale1 + clipx1 * rw.scalestep;
			ds->scale2 = rw.scale2 - clipx2 * rw.scalestep;
			ds->scalestep = FixedMul<16, 30, 16>(ds->scale2 - ds->scale1, stepfactor);

			ds->uinvz1 = rw.uinvz1 + clipx1 * rw.uinvzstep;
			ds->uinvz2 = rw.uinvz2 - clipx2 * rw.uinvzstep;
			ds->uinvzstep = FixedMul<20, 30, 20>(ds->uinvz2 - ds->uinvz1, stepfactor); 
		}
		else
		{
			ds->invz1 = ds->invz2 = rw.invz1 + clipx1 * rw.invzstep;
			ds->invzstep = 0;

			ds->scale1 = ds->scale2 = rw.scale1 + clipx1 * rw.scalestep;
			rw.scalestep = 0;

			ds->uinvz1 = ds->uinvz2 = rw.uinvz1 + clipx1 * rw.uinvzstep;
			ds->uinvzstep = 0;
		}
	}
	else
	{
		ds->invz1 = rw.invz1;
		ds->invz2 = rw.invz2;
		ds->invzstep = rw.invzstep;

		ds->scale1 = rw.scale1;
		ds->scale2 = rw.scale2;
		ds->scalestep = rw.scalestep;

		ds->uinvz1 = rw.uinvz1;
		ds->uinvz2 = rw.uinvz2;
		ds->uinvzstep = rw.uinvzstep;
	}

	ds->light = ds->scale1 * lightscalexmul;
 	ds->lightstep = ds->scalestep * lightscalexmul;

	// calculate texture boundaries
	//	and decide if floor / ceiling marks are needed
	midtexture = toptexture = bottomtexture = maskedmidtexture = 0; 

	if (!ds->backsector)
	{
		// single sided line
		midtexture = sidedef->midtexture;

		// a single sided line is terminal, so it must mark ends
		markfloor = markceiling = true;

		rw_midtexturemid = R_CalculateMidTextureMid(ds);

		ds->silhouette = SIL_BOTH;
		ds->sprtopclip = viewheightarray;
		ds->sprbottomclip = negonearray;
	}
	else
	{
		// two sided line
		ds->sprtopclip = ds->sprbottomclip = NULL;
		ds->silhouette = 0;

		extern bool doorclosed;	
		if (doorclosed)
		{
			// clip all sprites behind this closed door (or otherwise solid line)
			ds->silhouette = SIL_BOTH;
			ds->sprtopclip = viewheightarray;
			ds->sprbottomclip = negonearray;
		}
		else
		{
			// determine sprite clipping for non-solid line segs	
			if (rw_wall.frontf1.z > rw_wall.backf1.z || rw_wall.frontf2.z > rw_wall.backf2.z || 
				rw_wall.backf1.z > viewz || rw_wall.backf2.z > viewz || 
				!P_IsPlaneLevel(&ds->backsector->floorplane))	// backside sloping?
				ds->silhouette |= SIL_BOTTOM;

			if (rw_wall.frontc1.z < rw_wall.backc1.z || rw_wall.frontc2.z < rw_wall.backc2.z ||
				rw_wall.backc1.z < viewz || rw_wall.backc2.z < viewz || 
				!P_IsPlaneLevel(&ds->backsector->ceilingplane))	// backside sloping?
				ds->silhouette |= SIL_TOP;
		}

		if (doorclosed)
		{
			markceiling = markfloor = true;
		}
		else if (spanfunc == R_FillSpan)
		{
			markfloor = markceiling = (ds->frontsector != ds->backsector);
		}
		else
		{
			markfloor =
				  !P_IdenticalPlanes(&ds->backsector->floorplane, &ds->frontsector->floorplane)
				|| ds->backsector->lightlevel != ds->frontsector->lightlevel
				|| ds->backsector->floor_texhandle != ds->frontsector->floor_texhandle

				// killough 3/7/98: Add checks for (x,y) offsets
				|| ds->backsector->floor_xoffs != ds->frontsector->floor_xoffs
				|| (ds->backsector->floor_yoffs + ds->backsector->base_floor_yoffs) != 
				   (ds->frontsector->floor_yoffs + ds->frontsector->base_floor_yoffs)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through deep water
				|| ds->frontsector->heightsec

				// killough 4/17/98: draw floors if different light levels
				|| ds->backsector->floorlightsec != ds->frontsector->floorlightsec

				// [RH] Add checks for colormaps
				|| ds->backsector->floorcolormap != ds->frontsector->floorcolormap

				|| ds->backsector->floor_xscale != ds->frontsector->floor_xscale
				|| ds->backsector->floor_yscale != ds->frontsector->floor_yscale

				|| (ds->backsector->floor_angle + ds->backsector->base_floor_angle) !=
				   (ds->frontsector->floor_angle + ds->frontsector->base_floor_angle)
				;

			markceiling = 
				  !P_IdenticalPlanes(&ds->backsector->ceilingplane, &ds->frontsector->ceilingplane)
				|| ds->backsector->lightlevel != ds->frontsector->lightlevel
				|| ds->backsector->ceiling_texhandle != ds->frontsector->ceiling_texhandle

				// killough 3/7/98: Add checks for (x,y) offsets
				|| ds->backsector->ceiling_xoffs != ds->frontsector->ceiling_xoffs
				|| (ds->backsector->ceiling_yoffs + ds->backsector->base_ceiling_yoffs) !=
				   (ds->frontsector->ceiling_yoffs + ds->frontsector->base_ceiling_yoffs)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through fake ceilings
				|| (ds->frontsector->heightsec && ds->frontsector->ceiling_texhandle != sky1flathandle)

				// killough 4/17/98: draw ceilings if different light levels
				|| ds->backsector->ceilinglightsec != ds->frontsector->ceilinglightsec

				// [RH] Add check for colormaps
				|| ds->backsector->ceilingcolormap != ds->frontsector->ceilingcolormap

				|| ds->backsector->ceiling_xscale != ds->frontsector->ceiling_xscale
				|| ds->backsector->ceiling_yscale != ds->frontsector->ceiling_yscale

				|| (ds->backsector->ceiling_angle + ds->backsector->base_ceiling_angle) !=
				   (ds->frontsector->ceiling_angle + ds->frontsector->base_ceiling_angle)
				;
				
			// Sky hack
			markceiling = markceiling &&
				(ds->frontsector->ceiling_texhandle != sky1flathandle ||
				ds->backsector->ceiling_texhandle != sky1flathandle);
		}

		if (rw_hashigh)
		{
			toptexture = sidedef->toptexture;
			rw_toptexturemid = R_CalculateTopTextureMid(ds);
		}

		if (rw_haslow)
		{
			bottomtexture = sidedef->bottomtexture;
			rw_bottomtexturemid = R_CalculateBottomTextureMid(ds);
		}

		maskedmidtexture = sidedef->midtexture;
		if (maskedmidtexture)
		{
			markfloor = markceiling = true;
		}

		// [SL] additional fix for sky hack
		if (ds->frontsector->ceiling_texhandle == sky1flathandle &&
			ds->backsector->ceiling_texhandle == sky1flathandle)
			toptexture = 0; 
	}

	// [SL] 2012-01-24 - Horizon line extends to infinity by scaling the wall
	// height to 0
	if (rw.curline->linedef->special == Line_Horizon)
	{
		ds->scale1 = ds->scale2 = ds->light = 0;
		midtexture = toptexture = bottomtexture = maskedmidtexture = 0;

		for (int n = start; n <= stop; n++)
			walltopf[n] = wallbottomf[n] = centery;
	}

	// if a floor / ceiling plane is on the wrong side
	//	of the view plane, it is definitely invisible
	//	and doesn't need to be marked.

	// killough 3/7/98: add deep water check
	if (ds->frontsector->heightsec == NULL ||
		(ds->frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		// above view plane?
		if (P_FloorHeight(viewx, viewy, ds->frontsector) >= viewz)       
			markfloor = false;
		// below view plane?
		if (P_CeilingHeight(viewx, viewy, ds->frontsector) <= viewz &&
			ds->frontsector->ceiling_texhandle != sky1flathandle)   
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

	R_RenderSolidSegRange(ds);

	// [SL] save full clipping info for masked midtextures
	// cph - if a column was made solid by this wall, we _must_ save full clipping info
	if (maskedmidtexture || (ds->backsector && didsolidcol))
		ds->silhouette = SIL_BOTH;

    // save sprite & masked seg clipping info
	if ((ds->silhouette & SIL_TOP) && ds->sprtopclip == NULL)
	{
		ds->sprtopclip = openings.alloc<int>(width) - start;
		memcpy(ds->sprtopclip + start, ceilingclip + start, width * sizeof(*ds->sprtopclip));
	}

	if ((ds->silhouette & SIL_BOTTOM) && ds->sprbottomclip == NULL)
	{
		ds->sprbottomclip = openings.alloc<int>(width) - start;
		memcpy(ds->sprbottomclip + start, floorclip + start, width * sizeof(*ds->sprbottomclip));
	}

	if (maskedmidtexture)
	{
		ds->maskedcoldrawn = openings.alloc<byte>(width) - start;
		memset(ds->maskedcoldrawn + start, 0, width * sizeof(*ds->maskedcoldrawn));
	}
}


void R_ClearOpenings()
{
	openings.clear();
}



VERSION_CONTROL (r_segs_cpp, "$Id$")

