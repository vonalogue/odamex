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
//	Sky rendering. The DOOM sky is a texture map like any
//	wall, wrapping around. 1024 columns equal 360 degrees.
//	The default sky map is 256 columns and repeats 4 times
//	on a 320 screen.
//	
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "doomstat.h"
#include "m_fixed.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_main.h"
#include "c_cvars.h"
#include "g_level.h"
#include "r_sky.h"
#include "gi.h"
#include "w_wad.h"
#include "r_texture.h"

EXTERN_CVAR(sv_freelook)
EXTERN_CVAR(cl_mouselook)
EXTERN_CVAR(r_skypalette)


//
// sky mapping
//

texhandle_t		sky1flathandle;
texhandle_t		sky2flathandle;

texhandle_t		sky1texhandle;
texhandle_t		sky2texhandle;


fixed_t		skytexturemid;
fixed_t		skyscale;
int			skystretch;
fixed_t		skyiscale;

int			sky1shift,		sky2shift;
fixed_t		sky1pos=0,		sky1speed=0;
fixed_t		sky2pos=0,		sky2speed=0;

CVAR_FUNC_IMPL(r_stretchsky)
{
	R_InitSkyMap ();
}

char SKYFLATNAME[8] = "F_SKY1";

extern "C" int detailxshift, detailyshift;
extern fixed_t freelookviewheight;

static byte* skycols[MAXWIDTH];

//
//
// R_InitSkyMap
//
// Called whenever the view size changes.
//
// [ML] 5/11/06 - Remove sky2 stuffs
// [ML] 3/16/10 - Bring it back!

void R_InitSkyMap ()
{
	// [SL] 2011-11-30 - Don't run if we don't know what sky texture to use
	if (gamestate != GS_LEVEL)
		return;

	const Texture* sky1texture = texturemanager.getTexture(sky1texhandle);
	const Texture* sky2texture = NULL;
	if (sky2texhandle)
	{
		sky2texture = texturemanager.getTexture(sky2texhandle);
		if (sky1texture->getHeight() != sky2texture->getHeight())
		{
			Printf (PRINT_HIGH,"\x1f+Both sky textures must be the same height.\x1f-\n");
			sky2texhandle = sky1texhandle;
			sky2texture = sky1texture;
		}
	}

	skystretch = 0;

	if (sky1texture->getHeight() <= 128*FRACUNIT)
	{
		skytexturemid = 200/2*FRACUNIT;
		skystretch = (r_stretchsky == 1) || (r_stretchsky == 2 && sv_freelook && cl_mouselook);
	}
	else
	{
		skytexturemid = 199 * FRACUNIT;
	}
	
	if (viewwidth && viewheight)
	{
		skyiscale = (200*FRACUNIT) / (((freelookviewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift));
		skyscale = ((((freelookviewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift)) << FRACBITS) /(200);

		skyiscale = FixedMul (skyiscale, FixedDiv (FieldOfView, 2048));
		skyscale = FixedMul (skyscale, FixedDiv (2048, FieldOfView));
	}

	// The DOOM sky map is 256*128*4 maps.
	// The Heretic sky map is 256*200*4 maps.
	sky1shift = 22+skystretch-16;
	sky2shift = 22+skystretch-16;	
	if (sky1texture->getWidth() >= 128*FRACUNIT)
		sky1shift -= skystretch;
	if (sky2texture->getWidth() >= 128*FRACUNIT)
		sky2shift -= skystretch;
}

//
// R_BlastSkyColumn
//
static inline void R_BlastSkyColumn(void (*drawfunc)(drawcolumn_t&))
{
	if (dcol.yl <= dcol.yh)
	{
		dcol.texturefrac = dcol.texturemid + (dcol.yl - centery + 1) * dcol.iscale;
		drawfunc(dcol);
	}
}

inline void SkyColumnBlaster()
{
	R_BlastSkyColumn(colfunc);
}

//
// R_IsSkyFlat
//
bool R_IsSkyFlat(texhandle_t handle)
{
	return handle == sky1flathandle
		|| handle == sky2flathandle
		|| handle & PL_SKYTRANSFERLINE_MASK;
}
			

//
// R_RenderSkyRange
//
// [RH] Can handle parallax skies. Note that the front sky is *not* masked in
// in the normal convention for patches, but uses color 0 as a transparent
// color.
// [ML] 5/11/06 - Removed sky2
//
void R_RenderSkyRange(visplane_t* pl)
{
	if (pl->minx > pl->maxx)
		return;

	int columnmethod = 2;

	texhandle_t skytexhandle = sky1texhandle;
	fixed_t front_offset = 0;
	angle_t skyflip = 0;

	if (pl->texhandle == sky1flathandle)
	{
		// use sky1
		skytexhandle = sky1texhandle;
	}
	else if (pl->texhandle == sky2flathandle)
	{
		// use sky2
		skytexhandle = sky2texhandle;
	}
	else if (pl->texhandle & PL_SKYTRANSFERLINE_MASK)
	{
		// MBF's linedef-controlled skies
		short linenum = (pl->texhandle & ~PL_SKYTRANSFERLINE_MASK) - 1;
		if (linenum < numlines)
		{
			const line_t* line = &lines[linenum];

			// Sky transferred from first sidedef
			const side_t* side = *line->sidenum + sides;

			// Texture comes from upper texture of reference sidedef
			// TODO: texturetranslation[]
			skytexhandle = side->_toptexture;

			// Horizontal offset is turned into an angle offset,
			// to allow sky rotation as well as careful positioning.
			// However, the offset is scaled very small, so that it
			// allows a long-period of sky rotation.
			front_offset = (-side->textureoffset) >> 6;

			// Vertical offset allows careful sky positioning.
			skytexturemid = side->rowoffset - 28*FRACUNIT;

			// We sometimes flip the picture horizontally.
			//
			// Doom always flipped the picture, so we make it optional,
			// to make it easier to use the new feature, while to still
			// allow old sky textures to be used.
			skyflip = line->args[2] ? 0u : ~0u;
		}
	}

	R_ResetDrawFuncs();

	palette_t *pal = GetDefaultPalette();

	const Texture* texture = texturemanager.getTexture(skytexhandle);

	dcol.iscale = skyiscale >> skystretch;
	dcol.texturemid = skytexturemid;
	dcol.textureheight = texture->getHeight();
	skyplane = pl;

	// set up the appropriate colormap for the sky
	if (fixedlightlev)
	{
		dcol.colormap = shaderef_t(&pal->maps, fixedlightlev);
	}
	else if (fixedcolormap.isValid() && r_skypalette)
	{
		dcol.colormap = fixedcolormap;
	}
	else
	{
		// [SL] 2011-06-28 - Emulate vanilla Doom's handling of skies
		// when the player has the invulnerability powerup
		dcol.colormap = shaderef_t(&pal->maps, 0);
	}

	static shaderef_t colormap_table[MAXWIDTH];
	for (int x = pl->minx; x <= pl->maxx; x++)
		colormap_table[x] = dcol.colormap;

	// determine which texture posts will be used for each screen
	// column in this range.
	for (int x = pl->minx; x <= pl->maxx; x++)
	{
		int colnum = (((viewangle + xtoviewangle[x]) ^ skyflip) >> sky1shift) + front_offset;
		skycols[x] = texture->getColumnDataTiled(colnum);
	}

	R_DrawColumnRange(pl->minx, pl->maxx, (int*)pl->top, (int*)pl->bottom, skycols,
						colormap_table, SkyColumnBlaster);
				
	R_ResetDrawFuncs();
}

VERSION_CONTROL (r_sky_cpp, "$Id$")
