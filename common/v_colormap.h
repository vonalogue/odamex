// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: v_colormap.h $
//
// Copyright (C) 1998-2006 by Randy Heit (ZDoom 1.22).
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
//
// Color mapping, translating, and shading definitions
//
//-----------------------------------------------------------------------------

#ifndef __V_COLORMAP_H__
#define __V_COLORMAP_H__

#include "doomtype.h"
#include "doomdef.h"

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS			32

// ----------------------------------------------------------------------------
//
// extern variables and functions
//
// ----------------------------------------------------------------------------

extern argb_t translationRGB[MAXPLAYERS + 1][16];


// ----------------------------------------------------------------------------
//
// translationref_t
//
// ----------------------------------------------------------------------------

class translationref_t
{
public:
	translationref_t(const palindex_t* table = NULL, int player_id = -1) :
		m_table(table), m_player_id(player_id)
	{ }

	translationref_t(const translationref_t &other) :
		m_table(other.m_table), m_player_id(other.m_player_id)
	{ }

	forceinline operator bool() const
	{
		return m_table != NULL;
	}

	forceinline palindex_t tlate(const palindex_t c) const
	{
		#if DEBUG
		if (m_table == NULL)
			throw CFatalError("translationref_t::tlate() called with NULL m_table");
		#endif

		return m_table[c];
	}

	forceinline int getPlayerID() const
	{
		return m_player_id;
	}

	forceinline const byte* getTable() const
	{
		return m_table;
	}

private:
	const palindex_t*	m_table;
	int					m_player_id;
};


// ----------------------------------------------------------------------------
//
// shademap_t
//
// ----------------------------------------------------------------------------

struct shademap_t
{
	palindex_t*		colormap;	// Colormap for 8-bit
	argb_t*			shademap;	// ARGB8888 values for 32-bit
	byte			ramp[256];	// Light fall-off as a function of distance
								// Light levels: 0 = black, 255 = full bright.
								// Distance:     0 = near,  255 = far.
};


// ----------------------------------------------------------------------------
//
// shaderef_t
//
// ----------------------------------------------------------------------------

struct dyncolormap_t;
dyncolormap_t* V_FindDynamicColormap(const shademap_t* shademap);
argb_t V_LightWithGamma(const dyncolormap_t* dyncolormap, argb_t color, int intensity);

// This represents a clean reference to a map of both 8-bit colors and 32-bit shades.

class shaderef_t
{
public:
	shaderef_t() :
		m_colormap(NULL), m_shademap(NULL), m_dyncolormap(NULL),
		m_colors(NULL), m_mapnum(-1) 
	{ }

	shaderef_t(const shaderef_t &other) :
		m_colormap(other.m_colormap), m_shademap(other.m_shademap), m_dyncolormap(other.m_dyncolormap),
		m_colors(other.m_colors), m_mapnum(other.m_mapnum)
	{ }

	shaderef_t(const shademap_t* const colors, const int mapnum) :
		m_colormap(NULL), m_shademap(NULL), m_dyncolormap(NULL),
		m_colors(colors), m_mapnum(mapnum)
	{
		#if DEBUG
		// NOTE(jsd): Arbitrary value picked here because we don't record the
		// max number of colormaps for dynamic ones... or do we?
		if (m_mapnum >= 8192)
		{
			char tmp[100];
			sprintf_s(tmp, "32bpp: shaderef_t::shaderef_t() called with mapnum = %d, which looks too large", m_mapnum);
			throw CFatalError(tmp);
		}
		#endif

		if (m_colors != NULL)
		{
			if (m_colors->colormap != NULL)
				m_colormap = m_colors->colormap + (256 * m_mapnum);
			else
				m_colormap = NULL;

			if (m_colors->shademap != NULL)
				m_shademap = m_colors->shademap + (256 * m_mapnum);
			else
				m_shademap = NULL;

			m_dyncolormap = V_FindDynamicColormap(m_colors);
		}
	}

	forceinline bool operator==(const shaderef_t &other) const
	{
		return m_colors == other.m_colors && m_mapnum == other.m_mapnum;
	}

	forceinline bool isValid() const
	{
		return m_colors != NULL;
	}

	forceinline shaderef_t with(const int mapnum) const
	{
		return shaderef_t(m_colors, m_mapnum + mapnum);
	}

	forceinline palindex_t index(const palindex_t c) const
	{
		#if DEBUG
		if (m_colors == NULL)
			throw CFatalError("shaderef_t::index(): Bad shaderef_t");
		if (m_colors->colormap == NULL)
			throw CFatalError("shaderef_t::index(): colormap == NULL!");
		#endif

		return m_colormap[c];
	}

	forceinline argb_t shade(const palindex_t c) const
	{
		#if DEBUG
		if (m_colors == NULL)
			throw CFatalError("shaderef_t::shade(): Bad shaderef_t");
		if (m_colors->shademap == NULL)
			throw CFatalError("shaderef_t::shade(): shademap == NULL!");
		#endif

		return m_shademap[c];
	}

	forceinline const shademap_t* map() const
	{
		return m_colors;
	}

	forceinline const int mapnum() const
	{
		return m_mapnum;
	}

	forceinline const byte ramp() const
	{
		if (m_mapnum >= NUMCOLORMAPS)
			return 0;

		int index = clamp(m_mapnum * 256 / NUMCOLORMAPS, 0, 255);
		return m_colors->ramp[index];
	}

	forceinline argb_t tlate(const translationref_t& translation, const palindex_t c) const
	{
		int pid = translation.getPlayerID();

		// Not a player color translation:
		if (pid == -1)
			return shade(translation.tlate(c));

		// Special effect:
		if (m_mapnum >= NUMCOLORMAPS)
			return shade(translation.tlate(c));

		// Is a player color translation, but not a player color index:
		if (!(c >= 0x70 && c < 0x80))
			return shade(c);

		// Find the shading for the custom player colors:
		byte intensity = 255 - ramp();
		argb_t t = translationRGB[pid][c - 0x70];

		return V_LightWithGamma(m_dyncolormap, t, intensity);
	}


public:
	mutable const palindex_t*		m_colormap;		// Computed colormap pointer
	mutable const argb_t*			m_shademap;		// Computed shademap pointer
	mutable const dyncolormap_t*	m_dyncolormap;	// Auto-discovered dynamic colormap

private:
	const shademap_t*	m_colors;	// The color/shade map to use
	int					m_mapnum;	// Which index into the color/shade map to use
};



// ----------------------------------------------------------------------------
//
// dyncolormap_t
//
// ----------------------------------------------------------------------------

struct dyncolormap_t
{
	shaderef_t		maps;
	unsigned int	color;
	unsigned int	fade;
	dyncolormap_t*	next;
};



#endif	// __V_COLORMAP_H__

