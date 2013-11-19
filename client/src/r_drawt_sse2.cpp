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


// Direct rendering (32-bit) functions for SSE2 optimization:

void R_DrawSpanD_SSE2(drawspan_t& drawspan)
{
#ifdef RANGECHECK
	if (drawspan.x2 < drawspan.x1 || drawspan.x1 < 0 || drawspan.x2 >= screen->width || drawspan.y > screen->height)
	{
		I_Error ("R_DrawSpan: %i to %i at %i", drawspan.x1, drawspan.x2, drawspan.y);
	}
#endif

	dsfixed_t ufrac = drawspan.xfrac;
	dsfixed_t vfrac = drawspan.yfrac;
	dsfixed_t ustep = drawspan.xstep;
	dsfixed_t vstep = drawspan.ystep;

	const byte* source = drawspan.source;
	argb_t* dest = (argb_t*)drawspan.dest;
	shaderef_t colormap = drawspan.colormap;
	int colsize = drawspan.colsize;
	
	// We do not check for zero spans here?
	int count = drawspan.x2 - drawspan.x1 + 1;

	// [SL] Note that the texture orientation differs from typical Doom span
	// drawers since flats are stored in column major format now. The roles
	// of ufrac and vfrac have been reversed to accomodate this.
	const int umask = ((1 << drawspan.texturewidthbits) - 1) << drawspan.textureheightbits;
	const int vmask = (1 << drawspan.textureheightbits) - 1;
	const int ushift = FRACBITS - drawspan.textureheightbits; 
	const int vshift = FRACBITS;

	// Blit until we align ourselves with a 16-byte offset for SSE2:
	while (((uintptr_t)dest) & 15)
	{
		// Current texture index in u,v.
		const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		*dest = colormap.shade(source[spot]);
		dest += colsize;

		// Next step in u,v.
		ufrac += ustep;
		vfrac += vstep;

		--count;
	}

	// SSE2 optimized and aligned stores for quicker memory writes:
	const __m128i mumask = _mm_set1_epi32(umask);
	const __m128i mvmask = _mm_set1_epi32(vmask);

	__m128i mufrac = _mm_set_epi32(ufrac+ustep*0, ufrac+ustep*1, ufrac+ustep*2, ufrac+ustep*3);
	const __m128i mufracinc = _mm_set1_epi32(ustep*4);
	__m128i mvfrac = _mm_set_epi32(vfrac+vstep*0, vfrac+vstep*1, vfrac+vstep*2, vfrac+vstep*3);
	const __m128i mvfracinc = _mm_set1_epi32(vstep*4);

	ufrac += count & ~3;
	vfrac += count & ~3;

	const int sse_end_count = count & 3;
	while (count > sse_end_count)
	{
//		[SL] The below SSE2 intrinsics are equivalent to the following block:
//		const int spot0 = (((ufrac + ustep*0) >> ushift) & umask) | (((vfrac + vstep*0) >> vshift) & vmask); 
//		const int spot1 = (((ufrac + ustep*1) >> ushift) & umask) | (((vfrac + vstep*1) >> vshift) & vmask); 
//		const int spot2 = (((ufrac + ustep*2) >> ushift) & umask) | (((vfrac + vstep*2) >> vshift) & vmask); 
//		const int spot3 = (((ufrac + ustep*3) >> ushift) & umask) | (((vfrac + vstep*3) >> vshift) & vmask); 

		__m128i u = _mm_and_si128(_mm_srai_epi32(mufrac, ushift), mumask);
		__m128i v = _mm_and_si128(_mm_srai_epi32(mvfrac, vshift), mvmask);
		__m128i spots = _mm_or_si128(u, v);

		// get the color of the pixels at each of the spots
		byte pixel0 = source[((int*)&spots)[0]];
		byte pixel1 = source[((int*)&spots)[1]];
		byte pixel2 = source[((int*)&spots)[2]];
		byte pixel3 = source[((int*)&spots)[3]];

		const __m128i finalColors = _mm_set_epi32(
			colormap.shade(pixel0),
			colormap.shade(pixel1),
			colormap.shade(pixel2),
			colormap.shade(pixel3)
		);

		_mm_store_si128((__m128i*)dest, finalColors);

		// [SL] TODO: does this break with r_detail != 0?
		dest += 4;

		mufrac = _mm_add_epi32(mufrac, mufracinc);
		mvfrac = _mm_add_epi32(mvfrac, mvfracinc);

		count -= 4;
	}

	// blit the remaining 0 - 3 pixels
	while (count > 0)
	{
		// Current texture index in u,v.
		const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		*dest = colormap.shade(source[spot]);
		dest += colsize;

		// Next step in u,v.
		ufrac += ustep;
		vfrac += vstep;
		count--;
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
		while ((((uintptr_t)dest) & 15) && (incount > 0))
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

void r_dimpatchD_SSE2(const DCanvas *const canvas, argb_t color, int alpha, int x1, int y1, int w, int h)
{
	int invAlpha = 256 - alpha;

	int pitch = canvas->pitch / sizeof(argb_t);
	int line_inc = pitch - w;
	argb_t* dest = (argb_t *)canvas->buffer + y1 * pitch + x1;

	// [SL] Calculate how many pixels of each row need to be drawn before dest is
	// aligned to a 16-byte boundary. Note that Odamex ensures the frame buffer's
	// pitch is always a multiple of 16 so the value of align doesn't change
	// for each row.
	const int align = (int)((uintptr_t)dest - ((uintptr_t)dest & ~15)) / sizeof(argb_t);
	const int batches = (w - align) / 4;
	const int remainder = (w - align) & 3;

	// SSE2 temporaries:
	const __m128i upper8mask = _mm_set_epi16(	0, 0xff, 0xff, 0xff,
												0, 0xff, 0xff, 0xff);
	const __m128i blendAlpha = _mm_set_epi16(	0, alpha, alpha, alpha,
												0, alpha, alpha, alpha);
	const __m128i blendInvAlpha = _mm_set_epi16(0, invAlpha, invAlpha, invAlpha,
												0, invAlpha, invAlpha, invAlpha);
	const __m128i blendColor = _mm_set_epi16(	0, RPART(color), GPART(color), BPART(color),
												0, RPART(color), GPART(color), BPART(color));
	const __m128i blendMult = _mm_mullo_epi16(	blendColor, blendAlpha);

	for (int y = 0; y < h; y++)
	{
		// align the destination buffer to 16-byte boundary
		for (int i = 0; i < align; i++)
		{
			*dest = alphablend1a(*dest, color, alpha);
			dest++;
		}

		// SSE2 optimize the bulk in batches of 4 colors:
		for (int i = 0; i < batches; i++)
		{
			const __m128i input = _mm_load_si128((__m128i*)dest);
	
			// Expand the width of each color channel from 8bits to 16 bits
			// by splitting input into two 128bit variables, each
			// containing 2 ARGB values. 16bit color channels are needed to
			// accomodate multiplication.
			__m128i lower = _mm_and_si128(_mm_unpacklo_epi8(input, input), upper8mask);
			__m128i upper = _mm_and_si128(_mm_unpackhi_epi8(input, input), upper8mask);

			// ((input * invAlpha) + (color * Alpha)) >> 8
			lower = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(lower, blendInvAlpha), blendMult), 8);
			upper = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(upper, blendInvAlpha), blendMult), 8);

			// Compress the width of each color channel to 8bits again and store in dest
			_mm_store_si128((__m128i*)dest, _mm_packus_epi16(lower, upper));

			dest += 4;
		}

		// Pick up the remainder:
		for (int i = 0; i < remainder; i++)
		{
			*dest = alphablend1a(*dest, color, alpha);
			dest++;
		}

		dest += line_inc;
	}
}

VERSION_CONTROL (r_drawt_sse2_cpp, "$Id$")

#endif
