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
//	V_DRAW
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>

#include "doomtype.h"
#include "v_video.h"
#include "r_main.h"
#include "m_swap.h"

#include "i_system.h"

#include "cmdlib.h"

#include "r_draw.h"

// [RH] Stretch values for V_DrawTextureClean()
int CleanXfac, CleanYfac;

EXTERN_CVAR(hud_transparency)

translationref_t V_ColorMap;
int V_ColorFill;

// Palette lookup table for direct modes
shaderef_t V_Palette;


/********************************/
/*                              */
/* The texture drawing wrappers */
/*                              */
/********************************/

void DCanvas::DrawWrapper(EWrapperCode drawer, const Texture* texture, int x0, int y0) const
{
	DrawSWrapper(drawer, texture, x0, y0, texture->getWidth(), texture->getHeight());
}

void DCanvas::DrawSWrapper(EWrapperCode drawer, const Texture* texture, int x0, int y0,
							int destwidth, int destheight) const
{
	if (!texture)
		return;

	int texwidth = texture->getWidth();
	int texheight = texture->getHeight();

	if (destwidth <= 0 || destheight <= 0 || texwidth == 0 || texheight == 0)
		return;

	// [AM] Adding 1 to the inc variables leads to fewer weird scaling
	//      artifacts since it forces col to roll over to the next real number
	//      a column-of-real-pixels sooner.
	fixed_t xinc = (texwidth << FRACBITS) / destwidth + 1;
	fixed_t yinc = (texheight << FRACBITS) / destheight + 1;
	fixed_t xmul = (destwidth << FRACBITS) / texwidth;
	fixed_t ymul = (destheight << FRACBITS) / texheight;

	x0 -= (texture->getOffsetX() * xmul) >> FRACBITS;
	y0 -= (texture->getOffsetY() * ymul) >> FRACBITS;

	#ifdef RANGECHECK
	if (x0 < 0 || x0 + destwidth > width || y0 < 0 || y0 + destheight > height)
	{
	  // Printf(PRINT_HIGH, "Texture at %d,%d exceeds LFB\n", x,y );
	  // No I_Error abort - what is up with TNT.WAD?
	  DPrintf ("DCanvas::DrawSWrapper: bad texture (ignored) (%i, %i) - (%i, %i)\n",
			x0, y0, x0 + destwidth, y0 + destheight);
	  return;
	}
	#endif

	if (this == screen)
		V_MarkRect(x0, y0, destwidth, destheight);
	
	// choose the drawing function based on 'drawer'
	typedef void (*ColumnDrawFunc)(drawcolumn_t&);
	ColumnDrawFunc DrawFuncs[6] =
	{
		R_DrawMaskedColumn,
		R_DrawTranslucentMaskedColumn,
		R_DrawTranslatedMaskedColumn,
		R_DrawTranslatedTranslucentMaskedColumn,
		R_FillMaskedColumn,
		R_FillMaskedColumn
	};
	ColumnDrawFunc drawfunc = DrawFuncs[drawer];

	int colstep = is8bit() ? 1 : 4;
	dcol.pitch = pitch / colstep;
	dcol.dest = buffer + y0 * pitch + x0 * colstep;
	dcol.source = texture->getData();
	dcol.mask = texture->getMaskData();
	dcol.colormap = V_Palette;
	dcol.translation = V_ColorMap;

	dcol.color = (int)V_ColorFill;

	dcol.translevel = 0xFFFF * hud_transparency;
	dcol.yl = y0;
	dcol.yh = y0 + destheight - 1;
	dcol.iscale = yinc;
	dcol.texturefrac = 0;
	dcol.textureheight = texheight << FRACBITS;

	for (fixed_t colnum = 0; colnum < destwidth * xinc; colnum += xinc)
	{
		int pixel_offset = texheight * (colnum >> FRACBITS);
		dcol.source = texture->getData() + pixel_offset;
		dcol.mask = texture->getMaskData() + pixel_offset;
		drawfunc(dcol);

		dcol.dest += colstep;
	} 
}

void DCanvas::DrawIWrapper(EWrapperCode drawer, const Texture* texture, int x0, int y0) const
{
	if (width == 320 && height == 200)
		DrawWrapper(drawer, texture, x0, y0);
	else
		DrawSWrapper(drawer, texture,
			 (width * x0) / 320, (height * y0) / 200,
			 (width * texture->getWidth()) / 320, (height * texture->getHeight()) / 200);
}

void DCanvas::DrawCWrapper(EWrapperCode drawer, const Texture* texture, int x0, int y0) const
{
	if (CleanXfac == 1 && CleanYfac == 1)
		DrawWrapper(drawer, texture, (x0 - 160) + (width / 2), (y0 - 100) + (height / 2));
	else
		DrawSWrapper(drawer, texture,
			(x0 - 160) * CleanXfac + (width / 2), (y0 - 100) * CleanYfac + (height / 2),
			texture->getWidth() * CleanXfac, texture->getHeight() * CleanYfac);
}

void DCanvas::DrawCNMWrapper(EWrapperCode drawer, const Texture* texture, int x0, int y0) const
{
	if (CleanXfac == 1 && CleanYfac == 1)
		DrawWrapper(drawer, texture, x0, y0);
	else
		DrawSWrapper(drawer, texture, x0, y0, texture->getWidth() * CleanXfac, texture->getHeight() * CleanYfac);
}


/********************************/
/*								*/
/* Other miscellaneous routines */
/*								*/
/********************************/

//
// V_CopyRect
//
void DCanvas::CopyRect (int srcx, int srcy, int _width, int _height,
						int destx, int desty, DCanvas *destscrn)
{
	#ifdef RANGECHECK
	// [AM] Properly crop the copy.  All of these comparison checks (except
	//      the very last two) used to be done every tic anyway, now we attempt
	//      to do something intelligent about it.

	// Source coordinates OOB means we offset our destination coordinates by
	// the same ammount, effectively giving it a whitespace border.
	if (srcx < 0)
	{
		destx -= srcx;
		srcx = 0;
	}
	if (srcy < 0)
	{
		desty -= srcy;
		srcy = 0;
	}
	// Rectangle going outside of the source buffer's width and height
	// means we reduce the size of the rectangle until it fits into the source
	// buffer.
	if (srcx + _width > this->width)
	{
		_width = this->width - srcx;
	}
	if (srcy + _height > this->height)
	{
		_height = this->height - srcy;
	}
	// Destination coordinates OOB means we offset our source coordinates by
	// the same amount, effectively cutting off the top or left hand corner.
	if (destx < 0)
	{
		srcx -= destx;
		destx = 0;
	}
	if (desty < 0)
	{
		srcy -= desty;
		desty = 0;
	}
	// Rectangle going outside of the destination buffer's width and height
	// means we reduce the size of the rectangle (again?) so it fits into the
	// destination buffer.
	if (destx + _width > destscrn->width)
	{
		_width = destscrn->width - destx;
	}
	if (desty + _height > destscrn->height)
	{
		_height = destscrn->height - desty;
	}
	// If rectangle width or height is 0 or less, our blit is useless.
	if (_width <= 0 || _height <= 0)
	{
		DPrintf("DCanvas::CopyRect: Bad copy (ignored)\n");
		return;
	}
	#endif

	V_MarkRect (destx, desty, _width, _height);
	Blit (srcx, srcy, _width, _height, destscrn, destx, desty, _width, _height);
}


//
// DCanvas::DrawBlock
//
// Draw a linear block of pixels into the view buffer.
//
void DCanvas::DrawBlock (int x, int y, int _width, int _height, const byte *src) const
{
	byte *dest;

#ifdef RANGECHECK
	if (x < 0 ||x + _width > width || y < 0 || y + _height > height)
		I_Error ("Bad DCanvas::DrawBlock");
#endif

	V_MarkRect (x, y, _width, _height);

	x <<= (is8bit()) ? 0 : 2;
	_width <<= (is8bit()) ? 0 : 2;

	dest = buffer + y*pitch + x;

	while (_height--)
	{
		memcpy (dest, src, _width);
		src += _width;
		dest += pitch;
	};
}


//
// DCanvas::GetBlock
//
// Gets a linear block of pixels from the view buffer.
//
void DCanvas::GetBlock (int x, int y, int _width, int _height, byte *dest) const
{
	const byte *src;

#ifdef RANGECHECK
	if (x < 0 ||x + _width > width || y < 0 || y + _height > height)
		I_Error ("Bad DCanvas::GetBlock");
#endif

	x <<= (is8bit()) ? 0 : 2;
	_width <<= (is8bit()) ? 0 : 2;

	src = buffer + y*pitch + x;

	while (_height--)
	{
		memcpy (dest, src, _width);
		src += pitch;
		dest += _width;
	}
}


//
// DCanvas::GetTransposedBlock
//
// Gets a transposed block of pixels from the view buffer.
//

template<typename PIXEL_T>
static inline void V_GetTransposedBlockGeneric(const DCanvas* canvas, int x, int y, int width, int height, byte* destbuffer)
{
	const int pitch = canvas->pitch / sizeof(PIXEL_T);
	const PIXEL_T* source = (PIXEL_T*)canvas->buffer + y * pitch + x;
	PIXEL_T* dest = (PIXEL_T*)destbuffer;

	for (int col = x; col < x + width; col++)
	{
		const PIXEL_T* sourceptr = source++;
		for (int row = y; row < y + height; row++)
		{
			*dest++ = *sourceptr;
			sourceptr += pitch;
		}
	}
}

void DCanvas::GetTransposedBlock(int x, int y, int _width, int _height, byte* destbuffer) const
{
#ifdef RANGECHECK
	if (x < 0 ||x + _width > width || y < 0 || y + _height > height)
		I_Error ("Bad V_GetTransposedBlock");
#endif

	if (is8bit())
		V_GetTransposedBlockGeneric<palindex_t>(this, x, y, _width, _height, destbuffer);
	else
		V_GetTransposedBlockGeneric<argb_t>(this, x, y, _width, _height, destbuffer);
}

int V_GetColorFromString (const argb_t *palette, const char *cstr)
{
	int c[3], i, p;
	char val[5];
	const char *s, *g;

	val[4] = 0;
	for (s = cstr, i = 0; i < 3; i++) {
		c[i] = 0;
		while ((*s <= ' ') && (*s != 0))
			s++;
		if (*s) {
			p = 0;
			while (*s > ' ') {
				if (p < 4) {
					val[p++] = *s;
				}
				s++;
			}
			g = val;
			while (p < 4) {
				val[p++] = *g++;
			}
			c[i] = ParseHex (val);
		}
	}
	if (palette)
		return BestColor (palette, c[0]>>8, c[1]>>8, c[2]>>8, 256);
	else
		return ((c[0] << 8) & 0xff0000) |
			   ((c[1])      & 0x00ff00) |
			   ((c[2] >> 8));
}

VERSION_CONTROL (v_draw_cpp, "$Id$")

