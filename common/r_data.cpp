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

#include "v_video.h"

#include <ctype.h>
#include <cstddef>

int 			firstspritelump;
int 			lastspritelump;
int				numspritelumps;



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


//
// R_InitData
// Locates all the lumps
//	that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
	R_InitSpriteLumps ();

	Table_InitTables();
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


VERSION_CONTROL (r_data_cpp, "$Id$")

