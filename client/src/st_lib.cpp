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
//		The status bar widget code.
//		[RH] Widget coordinates are now relative to status bar
//			 instead of the game screen.
//
//-----------------------------------------------------------------------------



#include <ctype.h>

#include "doomdef.h"

#include "z_zone.h"
#include "v_video.h"

#include "m_swap.h"

#include "i_system.h"

#include "w_wad.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"

#include "c_cvars.h"
#include "m_swap.h"


void ST_DrawTexture(const Texture* texture, int x0, int y0);

// in AM_map.c
extern BOOL			automapactive;

void STlib_init(void)
{
}

// [RH] Routines to stretch status bar graphics depending on st_scale cvar.
EXTERN_CVAR (st_scale)

// ?
void
STlib_initNum
( st_number_t*			n,
  int					x,
  int					y,
  const Texture** 		textures,
  int*					num,
  bool*					on,
  int					width,
  const Texture*		minus_texture )
{
	n->x				= x;
	n->y				= y;
	n->oldnum			= 0;
	n->width			= width;
	n->num				= num;
	n->on				= on;
	n->textures			= textures;
	n->minus_texture	= minus_texture;
}


//
// A fairly efficient way to draw a number
//	based on differences from the old number.
// Note: worth the trouble?
//
void STlib_drawNum (st_number_t *n, bool refresh)
{
	int 		numdigits = n->width;
	int 		num = *n->num;

	int 		w = n->textures[0]->getWidth();
	int 		x = n->x;

	n->oldnum = *n->num;

	bool neg = num < 0;

	if (neg)
	{
		if (numdigits == 2 && num < -9)
			num = -9;
		else if (numdigits == 3 && num < -99)
			num = -99;

		num = -num;
	}

	// if non-number, do not draw it
	if (num == 1994)
		return;

	x = n->x;

	// in the special case of 0, you draw 0
	if (!num)
		ST_DrawTexture(n->textures[0], x - w, n->y);

	// draw the new number
	while (num && numdigits--)
	{
		x -= w;
		ST_DrawTexture(n->textures[num % 10], x, n->y);
		num /= 10;
	}

	// draw a minus sign if necessary
	if (neg)
		ST_DrawTexture(n->minus_texture, x - 8, n->y);
}


//
void
STlib_updateNum
( st_number_t*			n,
  bool					refresh )
{
	if (*n->on)
		STlib_drawNum(n, refresh);
}


//
void
STlib_initPercent
( st_percent_t* 		p,
  int					x,
  int					y,
  const Texture** 		textures,
  int*					num,
  bool*					on,
  const Texture*		minus_texture,
  const Texture*		percent_texture )
{
	STlib_initNum(&p->n, x, y, textures, num, on, 3, minus_texture);
	p->texture = percent_texture;
}




void
STlib_updatePercent
( st_percent_t* 		per,
  bool					refresh )
{
	if (refresh && *per->n.on)
		ST_DrawTexture(per->texture, per->n.x, per->n.y);

	STlib_updateNum(&per->n, refresh);
}



void
STlib_initMultIcon
( st_multicon_t*		i,
  int					x,
  int					y,
  const Texture** 		textures,
  int*					inum,
  bool*					on )
{
	i->x		= x;
	i->y		= y;
	i->oldinum	= -1;
	i->inum 	= inum;
	i->on		= on;
	i->textures	= textures;
}



void
STlib_updateMultIcon
( st_multicon_t*		mi,
  bool					refresh )
{
	if (*mi->on
		&& (mi->oldinum != *mi->inum || refresh)
		&& (*mi->inum!=-1))
	{
		ST_DrawTexture(mi->textures[*mi->inum], mi->x, mi->y);
		mi->oldinum = *mi->inum;
	}
}



void
STlib_initBinIcon
( st_binicon_t* 		b,
  int					x,
  int					y,
  const Texture*		texture,
  bool*					val,
  bool*					on )
{
	b->x		= x;
	b->y		= y;
	b->oldval	= 0;
	b->val		= val;
	b->on		= on;
	b->texture	= texture;
}



void
STlib_updateBinIcon
( st_binicon_t* 		bi,
  bool					refresh )
{
	if (*bi->on
		&& ((bi->oldval != *bi->val) || refresh))
	{
		ST_DrawTexture(bi->texture, bi->x, bi->y);
	}

}

VERSION_CONTROL (st_lib_cpp, "$Id$")

