// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1998-2006 by Randy Heit (ZDoom).
// Copyright (C) 2006-2012 by The Odamex Team.
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
//	Functions for drawing columns into a temporary buffer and then
//	copying them to the screen. On machines with a decent cache, this
//	is faster than drawing them directly to the screen. Will I be able
//	to even understand any of this if I come back to it later? Let's
//	hope so. :-)
//
//-----------------------------------------------------------------------------

#include "SDL_cpuinfo.h"
#include "r_intrin.h"

#ifdef __SSE2__

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <emmintrin.h>

#ifdef _MSC_VER
#define SSE2_ALIGNED(x) _CRT_ALIGN(16) x
#else
#define SSE2_ALIGNED(x) x __attribute__((aligned(16)))
#endif

#include "doomtype.h"
#include "doomdef.h"
#include "i_system.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_things.h"
#include "v_video.h"

// SSE2 alpha-blending functions.
// NOTE(jsd): We can only blend two colors per 128-bit register because we need 16-bit resolution for multiplication.

// Blend 4 colors against 1 color using SSE2:
#define blend4vs1_sse2(input,blendMult,blendInvAlpha,upper8mask) \
	(_mm_packus_epi16( \
		_mm_srli_epi16( \
			_mm_adds_epu16( \
				_mm_mullo_epi16( \
					_mm_and_si128(_mm_unpacklo_epi8(input, input), upper8mask), \
					blendInvAlpha \
				), \
				blendMult \
			), \
			8 \
		), \
		_mm_srli_epi16( \
			_mm_adds_epu16( \
				_mm_mullo_epi16( \
					_mm_and_si128(_mm_unpackhi_epi8(input, input), upper8mask), \
					blendInvAlpha \
				), \
				blendMult \
			), \
			8 \
		) \
	))

// Direct rendering (32-bit) functions for SSE2 optimization:

void R_DrawSpanD_SSE2(drawspan_t& drawspan)
{
#ifdef RANGECHECK
	if (dspan.x2 < dspan.x1 || dspan.x1 < 0 || dspan.x2 >= screen->width || dspan.y > screen->height)
	{
		I_Error ("R_DrawSpan: %i to %i at %i", dspan.x1, dspan.x2, dspan.y);
	}
#endif

	dsfixed_t ufrac = dspan.xfrac;
	dsfixed_t vfrac = dspan.yfrac;
	dsfixed_t ustep = dspan.xstep;
	dsfixed_t vstep = dspan.ystep;

	argb_t* dest = (argb_t*)drawspan.dest;

	// We do not check for zero spans here?
	int count = dspan.x2 - dspan.x1 + 1;

	// NOTE(jsd): Can this just die already?
	assert(dspan.colsize == 1);

	// [SL] Note that the texture orientation differs from typical Doom span
	// drawers since flats are stored in column major format now. The roles
	// of ufrac and vfrac have been reversed to accomodate this.
	const int umask = ((1 << dspan.texturewidthbits) - 1) << dspan.textureheightbits;
	const int vmask = (1 << dspan.textureheightbits) - 1;
	const int ushift = FRACBITS - dspan.textureheightbits; 
	const int vshift = FRACBITS;

	// Blit until we align ourselves with a 16-byte offset for SSE2:
	while (((size_t)dest) & 15)
	{
		// Current texture index in u,v.
		const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		*dest = dspan.colormap.shade(dspan.source[spot]);
		dest += dspan.colsize;

		// Next step in u,v.
		ufrac += ustep;
		vfrac += vstep;

		--count;
	}

	const int rounds = count / 4;
	if (rounds > 0)
	{
		// SSE2 optimized and aligned stores for quicker memory writes:
		for (int i = 0; i < rounds; ++i, count -= 4)
		{
			// TODO(jsd): Consider SSE2 bit twiddling here!
			const int spot0 = (((ufrac + ustep*0) >> ushift) & umask) | (((vfrac + vstep*0) >> vshift) & vmask); 
			const int spot1 = (((ufrac + ustep*1) >> ushift) & umask) | (((vfrac + vstep*1) >> vshift) & vmask); 
			const int spot2 = (((ufrac + ustep*2) >> ushift) & umask) | (((vfrac + vstep*2) >> vshift) & vmask); 
			const int spot3 = (((ufrac + ustep*3) >> ushift) & umask) | (((vfrac + vstep*3) >> vshift) & vmask); 

			const __m128i finalColors = _mm_setr_epi32(
				dspan.colormap.shade(dspan.source[spot0]),
				dspan.colormap.shade(dspan.source[spot1]),
				dspan.colormap.shade(dspan.source[spot2]),
				dspan.colormap.shade(dspan.source[spot3])
			);
			_mm_store_si128((__m128i *)dest, finalColors);
			dest += 4;

			// Next step in u,v.
			ufrac += ustep*4;
			vfrac += vstep*4;
		}
	}

	if (count > 0)
	{
		// Blit the last remainder:
		while (count--)
		{
			// Current texture index in u,v.
			const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 

			// Lookup pixel from flat texture tile,
			//  re-index using light/colormap.
			*dest = dspan.colormap.shade(dspan.source[spot]);
			dest += dspan.colsize;

			// Next step in u,v.
			ufrac += ustep;
			vfrac += vstep;
		}
	}
}

void R_DrawSlopeSpanD_SSE2(drawspan_t& drawspan)
{
	int count = dspan.x2 - dspan.x1 + 1;
	if (count <= 0)
		return;

#ifdef RANGECHECK 
	if (dspan.x2 < dspan.x1 || dspan.x1 < 0 || dspan.x2 >= screen->width || dspan.y > screen->height)
	{
		I_Error ("R_DrawSlopeSpan: %i to %i at %i", dspan.x1, dspan.x2, dspan.y);
	}
#endif

	// [SL] Note that the texture orientation differs from typical Doom span
	// drawers since flats are stored in column major format now. The roles
	// of ufrac and vfrac have been reversed to accomodate this.
	const int umask = ((1 << dspan.texturewidthbits) - 1) << dspan.textureheightbits;
	const int vmask = (1 << dspan.textureheightbits) - 1;
	const int ushift = FRACBITS - dspan.textureheightbits; 
	const int vshift = FRACBITS;

	float iu = dspan.iu, iv = dspan.iv;
	float ius = dspan.iustep, ivs = dspan.ivstep;
	float id = dspan.id, ids = dspan.idstep;
	
	// framebuffer	
	argb_t *dest = (argb_t *)drawspan.dest;
	
	// texture data
	byte *src = (byte *)dspan.source;

	assert (dspan.colsize == 1);
	const int colsize = dspan.colsize;
	int ltindex = 0;		// index into the lighting table

	// Blit the bulk in batches of SPANJUMP columns:
	while (count >= SPANJUMP)
	{
		const float mulstart = 65536.0f / id;
		id += ids * SPANJUMP;
		const float mulend = 65536.0f / id;

		const float ustart = iu * mulstart;
		const float vstart = iv * mulstart;

		fixed_t ufrac = (fixed_t)ustart;
		fixed_t vfrac = (fixed_t)vstart;

		iu += ius * SPANJUMP;
		iv += ivs * SPANJUMP;

		const float uend = iu * mulend;
		const float vend = iv * mulend;

		fixed_t ustep = (fixed_t)((uend - ustart) * INTERPSTEP);
		fixed_t vstep = (fixed_t)((vend - vstart) * INTERPSTEP);

		int incount = SPANJUMP;

		// Blit up to the first 16-byte aligned position:
		while ((((size_t)dest) & 15) && (incount > 0))
		{
			const shaderef_t &colormap = dspan.slopelighting[ltindex++];
	
			const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 
			*dest = colormap.shade(src[spot]);

			dest += colsize;
			ufrac += ustep;
			vfrac += vstep;
			incount--;
		}

		if (incount > 0)
		{
			const int rounds = incount >> 2;
			if (rounds > 0)
			{
				for (int i = 0; i < rounds; ++i, incount -= 4)
				{
					const int spot0 = (((ufrac + ustep*0) >> ushift) & umask) | (((vfrac + vstep*0) >> vshift) & vmask); 
					const int spot1 = (((ufrac + ustep*1) >> ushift) & umask) | (((vfrac + vstep*1) >> vshift) & vmask); 
					const int spot2 = (((ufrac + ustep*2) >> ushift) & umask) | (((vfrac + vstep*2) >> vshift) & vmask); 
					const int spot3 = (((ufrac + ustep*3) >> ushift) & umask) | (((vfrac + vstep*3) >> vshift) & vmask); 

					const __m128i finalColors = _mm_setr_epi32(
						dspan.slopelighting[ltindex+0].shade(src[spot0]),
						dspan.slopelighting[ltindex+1].shade(src[spot1]),
						dspan.slopelighting[ltindex+2].shade(src[spot2]),
						dspan.slopelighting[ltindex+3].shade(src[spot3])
					);
					_mm_store_si128((__m128i *)dest, finalColors);

					dest += 4;
					ltindex += 4;

					ufrac += ustep * 4;
					vfrac += vstep * 4;
				}
			}
		}

		if (incount > 0)
		{
			while(incount--)
			{
				const shaderef_t &colormap = dspan.slopelighting[ltindex++];
				const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 
				*dest = colormap.shade(src[spot]);
				dest += colsize;

				ufrac += ustep;
				vfrac += vstep;
			}
		}

		count -= SPANJUMP;
	}

	// Remainder:
	assert(count < SPANJUMP);
	if (count > 0)
	{
		const float mulstart = 65536.0f / id;
		id += ids * count;
		const float mulend = 65536.0f / id;

		const float ustart = iu * mulstart;
		const float vstart = iv * mulstart;

		fixed_t ufrac = (fixed_t)ustart;
		fixed_t vfrac = (fixed_t)vstart;

		iu += ius * count;
		iv += ivs * count;

		const float uend = iu * mulend;
		const float vend = iv * mulend;

		fixed_t ustep = (fixed_t)((uend - ustart) / count);
		fixed_t vstep = (fixed_t)((vend - vstart) / count);

		int incount = count;
		while (incount--)
		{
			const shaderef_t &colormap = dspan.slopelighting[ltindex++];
			const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 
			*dest = colormap.shade(src[spot]);
			dest += colsize;
			ufrac += ustep;
			vfrac += vstep;
		}
	}
}

void r_dimpatchD_SSE2(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h)
{
	int x, y, i;
	argb_t *line;
	int invAlpha = 256 - alpha;

	int dpitch = cvs->pitch / sizeof(argb_t);
	line = (argb_t *)cvs->buffer + y1 * dpitch;

	int batches = w / 4;
	int remainder = w & 3;

	// SSE2 temporaries:
	const __m128i upper8mask = _mm_set_epi16(0, 0xff, 0xff, 0xff, 0, 0xff, 0xff, 0xff);
	const __m128i blendAlpha = _mm_set_epi16(0, alpha, alpha, alpha, 0, alpha, alpha, alpha);
	const __m128i blendInvAlpha = _mm_set_epi16(0, invAlpha, invAlpha, invAlpha, 0, invAlpha, invAlpha, invAlpha);
	const __m128i blendColor = _mm_set_epi16(0, RPART(color), GPART(color), BPART(color), 0, RPART(color), GPART(color), BPART(color));
	const __m128i blendMult = _mm_mullo_epi16(blendColor, blendAlpha);

	for (y = y1; y < y1 + h; y++)
	{
		// SSE2 optimize the bulk in batches of 4 colors:
		for (i = 0, x = x1; i < batches; ++i, x += 4)
		{
			const __m128i input = _mm_setr_epi32(line[x + 0], line[x + 1], line[x + 2], line[x + 3]);
			_mm_storeu_si128((__m128i *)&line[x], blend4vs1_sse2(input, blendMult, blendInvAlpha, upper8mask));
		}

		if (remainder)
		{
			// Pick up the remainder:
			for (; x < x1 + w; x++)
			{
				line[x] = alphablend1a(line[x], color, alpha);
			}
		}

		line += dpitch;
	}
}

VERSION_CONTROL (r_drawt_sse2_cpp, "$Id$")

#endif
