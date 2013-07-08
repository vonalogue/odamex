// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: r_texture.cpp 3945 2013-07-03 14:32:48Z dr_sean $
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
//	Texture manager to load and convert textures.	
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "tables.h"
#include "r_defs.h"
#include "r_state.h"
#include "w_wad.h"
#include "cmdlib.h"

#include <cstring>
#include <algorithm>
#include <math.h>

#include "r_texture.h"

TextureManager texturemanager;

// ============================================================================
//
// TextureManager
//
// ============================================================================

Texture::Texture()
{
	init(0, 0);
}

void Texture::init(int width, int height)
{
	mWidth = width << FRACBITS;
	mHeight = height << FRACBITS;
	mWidthBits = Log2(width);
	mHeightBits = Log2(height);
	mWidthMask = (1 << (mWidthBits + FRACBITS)) - 1;
	mHeightMask = (1 << (mHeightBits + FRACBITS)) - 1;
	mOffsetX = 0;
	mOffsetY = 0;
	mScaleX = FRACUNIT;
	mScaleY = FRACUNIT;

	mColumnLookup = (byte**)((byte*)this + sizeof(*this));
	mData = (byte*)mColumnLookup + width * sizeof(*mColumnLookup);

	for (int colnum = 0; colnum < width; colnum++)
		mColumnLookup[colnum] = mData + colnum * height;	
}



// ============================================================================
//
// TextureManager
//
// ============================================================================

TextureManager::TextureManager()
{
}

TextureManager::~TextureManager()
{
	clear();
}


//
// TextureManager::clear
//
// Frees all memory used by TextureManager, freeing all Texture objects
// and the supporting lookup structures.
//
void TextureManager::clear()
{
	delete [] mPNameLookup;
	mPNameLookup = NULL;

	for (size_t i = 0; i < mTextureDefinitions.size(); i++)
		delete mTextureDefinitions[i];
	mTextureDefinitions.clear();

	for (std::map<texhandle_t, Texture*>::iterator it = mHandleMap.begin(); it != mHandleMap.end(); ++it)
	{
		Texture* texture = it->second;
		if (texture)
			Z_Free(texture);
	}

	mHandleMap.clear();
}


//
// TextureManager::init
//
// Frees any existing textures and sets up the lookup structures for flats
// and wall textures. This should be called at the start of each map, prior
// to P_SetupLevel.
//
void TextureManager::init()
{
	clear();

	// initialize the FLATS data
	mFirstFlatLumpNum = W_GetNumForName("F_START") + 1;
	mLastFlatLumpNum = W_GetNumForName("F_END") - 1;
	
	// initialize the PNAMES mapping to map an index in PNAMES to a WAD lump number
	readPNamesDirectory();

	// initialize the TEXTURE1 & TEXTURE2 data
	addTextureDirectory("TEXTURE1");
	addTextureDirectory("TEXTURE2");

	// initialize mHandleMap for all possible handles
	unsigned int num_walltextures = mTextureDefinitions.size();
	for (unsigned int texnum = 0; texnum < num_walltextures; texnum++)
	{
		texhandle_t handle = getWallTextureHandle(texnum);
		mHandleMap[handle] = NULL;
	}

	for (unsigned int flatnum = mFirstFlatLumpNum; flatnum <= mLastFlatLumpNum; flatnum++)
	{
		texhandle_t handle = getFlatHandle(flatnum);
		mHandleMap[handle] = NULL;
	}

	generateNotFoundTexture();
}


//
// TextureManager::precache
//
// Loads all of a level's textures into Zone memory.
// Requires that init() and P_SetupLevel be called first.
//
void TextureManager::precache()
{
	// precache all the wall textures
	for (int i = 0; i < numsides; i++)
	{
		if (sides[i].toptexture)
			getTexture(sides[i].toptexture);
		if (sides[i].midtexture)
			getTexture(sides[i].midtexture);
		if (sides[i].bottomtexture)
			getTexture(sides[i].bottomtexture);
	}

	// precache all the floor/ceiling textures
	for (int i = 0; i < numsectors; i++)
	{
		getTexture(sectors[i].ceiling_texhandle);
		getTexture(sectors[i].floor_texhandle);
	}
}


//
// TextureManager::readPNamesDirectory
//
void TextureManager::readPNamesDirectory()
{
	int lumpnum = W_GetNumForName("PNAMES");
	size_t lumplen = W_LumpLength(lumpnum);

	byte* lumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, lumpdata);

	int num_pname_mappings = LELONG(*((int*)(lumpdata + 0)));
	mPNameLookup = new int[num_pname_mappings];

	for (int i = 0; i < num_pname_mappings; i++)
	{
		mPNameLookup[i] = W_CheckNumForName((char*)(lumpdata + 4 + 8 * i));

		// killough 4/17/98:
		// Some wads use sprites as wall patches, so repeat check and
		// look for sprites this time, but only if there were no wall
		// patches found. This is the same as allowing for both, except
		// that wall patches always win over sprites, even when they
		// appear first in a wad. This is a kludgy solution to the wad
		// lump namespace problem.

		if (mPNameLookup[i] == -1)
			mPNameLookup[i] = W_CheckNumForName((char*)(lumpdata + 4 + 8 * i), ns_sprites);
	}

	delete [] lumpdata;
}


//
// TextureManager::generateNotFoundTexture
//
// Generates a checkerboard texture with 32x32 squares. This texture will be
// used whenever a texture is requested but not found in the WAD file.
//
void TextureManager::generateNotFoundTexture()
{
	const int width = 64;
	const int height = 64;

	texhandle_t handle = NOT_FOUND_TEXTURE_HANDLE;
	Texture** owner = &mHandleMap[handle];
	size_t texture_size = Texture::calculateSize(width, height);
	Texture* texture = (Texture*)Z_Malloc(texture_size, PU_STATIC, (void**)owner);
	texture->init(width, height);

	byte color1 = 0x40, color2 = 0x80;

	for (int y = 0; y < height / 2; y++)
	{
		memset(texture->mData + y * width + 0, color1, width / 2);
		memset(texture->mData + y * width + width / 2, color2, width / 2);
	}
	for (int y = height / 2; y < height; y++)
	{
		memset(texture->mData + y * width + 0, color2, width / 2);
		memset(texture->mData + y * width + width / 2, color1, width / 2);
	}
}


//
// TextureManager::addTextureDirectory
//
// Requires that the PNAMES lump has been read and processed.
//
void TextureManager::addTextureDirectory(const char* lumpname)
{
	int lumpnum = W_CheckNumForName(lumpname);
	if (lumpnum == -1)
	{
		if (iequals("TEXTURE1", lumpname))
			I_Error("R_InitTextures: TEXTURE1 lump not found");
		return;
	}

	size_t lumplen = W_LumpLength(lumpnum);
	if (lumplen == 0)
		return;

	byte* rawlumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, rawlumpdata);

	int numtextures = LELONG(*((int*)(rawlumpdata + 0)));
	int* texoffs = (int*)(rawlumpdata + 4);

	// keep track of the number of texture errors
	int errors = 0;

	for (int i = 0; i < numtextures; i++)
	{
		maptexture_t* mtexdef = (maptexture_t*)((byte*)rawlumpdata + LELONG(texoffs[i]));
		
		size_t texdefsize = sizeof(texdef_t) + sizeof(texdefpatch_t) * (SAFESHORT(mtexdef->patchcount) - 1);
		texdef_t* texdef = (texdef_t*)(new byte[texdefsize]); 	
		mTextureDefinitions.push_back(texdef);

		texdef->width = SAFESHORT(mtexdef->width);
		texdef->height = SAFESHORT(mtexdef->height);
		texdef->patchcount = SAFESHORT(mtexdef->patchcount);

		std::string texname(mtexdef->name, 9);
		std::transform(texname.begin(), texname.end(), texname.begin(), toupper);
		strncpy(texdef->name, mtexdef->name, 9);
		std::transform(texdef->name, texdef->name + strlen(texdef->name), texdef->name, toupper);

		mappatch_t* mpatch = &mtexdef->patches[0];
		texdefpatch_t* patch = &texdef->patches[0];

		for (int j = 0; j < texdef->patchcount; j++, mpatch++, patch++)
		{
			patch->originx = LESHORT(mpatch->originx);
			patch->originy = LESHORT(mpatch->originy);
			patch->patch = mPNameLookup[LESHORT(mpatch->patch)];
			if (patch->patch == -1)
			{
				Printf(PRINT_HIGH, "R_InitTextures: Missing patch in texture %s\n", texdef->name);
				errors++;
			}
		}

		mTextureNameTranslationMap[texname] = mTextureDefinitions.size() - 1;
	}

	delete [] rawlumpdata;
}


//
// TextureManager::getFlatHandle
//
// Returns the handle for the flat with the given WAD lump number.
//
texhandle_t TextureManager::getFlatHandle(unsigned int lumpnum)
{
	const unsigned int flatcount = mLastFlatLumpNum - mFirstFlatLumpNum + 1;
	const unsigned int flatnum = lumpnum - mFirstFlatLumpNum;

	// flatnum > number of flats in the WAD file?
	if (flatnum >= flatcount)
		return NOT_FOUND_TEXTURE_HANDLE;

	if (W_LumpLength(lumpnum) == 0)
		return NOT_FOUND_TEXTURE_HANDLE;

	return (texhandle_t)flatnum | FLAT_HANDLE_MASK;
}


//
// TextureManager::cacheFlat
//
// Loads a flat with the specified handle from the WAD file and composes
// a Texture object.
//
void TextureManager::cacheFlat(texhandle_t handle)
{
	// should we check that the handle is valid for a flat?

	unsigned int lumpnum = (handle & ~FLAT_HANDLE_MASK) + mFirstFlatLumpNum;
	unsigned int lumplen = W_LumpLength(lumpnum);

	int width, height;	

	if (lumplen == 64 * 64)
		width = height = 64;
	else if (lumplen == 128 * 128)
		width = height = 128;
	else if (lumplen == 256 * 256)
		width = height = 256;
	else
		width = height = Log2(sqrt(lumplen));	// probably not pretty... 

	Texture** owner = &mHandleMap[handle];
	size_t texture_size = Texture::calculateSize(width, height);
	Texture* texture = (Texture*)Z_Malloc(texture_size, PU_CACHE, (void**)owner);

	texture->init(width, height);

	byte *rawlumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, rawlumpdata);

#if 0
	// copy the row-major flat lump to into column-major
	byte* dest = texture->mData;

	for (int x = 0; x < width; x++)
	{
		const byte* source = rawlumpdata + x;
		
		for (int y = 0; y < height; y++)
		{
			*dest = *source;
			source += width;
			dest++;
		}
	}
#else
	// copy the row-major flat into row-major for now...
	// visplane span rendering functions will need to be modified to use
	// column-major format...
	memcpy(texture->mData, rawlumpdata, width * height);
#endif
	
	delete [] rawlumpdata;
}


//
// TextureManager::getWallTextureHandle
//
// Returns the handle for the wall texture with the given WAD lump number.
//
texhandle_t TextureManager::getWallTextureHandle(unsigned int texdef_handle)
{
	// flatnum > number of flats in the WAD file?
	if (texdef_handle >= mTextureDefinitions.size())
		return NOT_FOUND_TEXTURE_HANDLE;

	return (texhandle_t)texdef_handle | WALLTEXTURE_HANDLE_MASK;
}


//
// TextureManager::cacheWallTexture
//
// Composes a wall texture from a set of patches loaded from the WAD file.
//
void TextureManager::cacheWallTexture(texhandle_t handle)
{
	// should we check that the handle is valid for a wall texture?

	texdef_t* texdef = mTextureDefinitions[handle & ~WALLTEXTURE_HANDLE_MASK];

	int width = texdef->width;
	int height = texdef->height;

	Texture** owner = &mHandleMap[handle];
	size_t texture_size = Texture::calculateSize(width, height);
	Texture* texture = (Texture*)Z_Malloc(texture_size, PU_CACHE, (void**)owner);
	texture->init(width, height);

	// TODO: remove this once proper masking is in place
	memset(texture->mData, 0, width * height);

	// compose the texture out of a set of patches
	for (int i = 0; i < texdef->patchcount; i++)
	{
		texdefpatch_t* texdefpatch = &texdef->patches[i];

		unsigned int lumplen = W_LumpLength(texdefpatch->patch);
		byte* rawlumpdata = new byte[lumplen];
		W_ReadLump(texdefpatch->patch, rawlumpdata);

		const patch_t* patch = (patch_t*)rawlumpdata;
		const int* colofs = (int*)(rawlumpdata + 8);

		int x1 = MAX(texdefpatch->originx, 0);
		int x2 = MIN(texdefpatch->originx + patch->width() - 1, width - 1);

		for (int x = x1; x <= x2; x++)
		{
			int abstopdelta = 0;

			post_t* post = (post_t*)(rawlumpdata + colofs[x - texdefpatch->originx]);
			while (post->topdelta != 0xFF)
			{
				// handle DeePsea tall patches where topdelta is treated as a relative
				// offset instead of an absolute offset
				if (post->topdelta <= abstopdelta)
					abstopdelta += post->topdelta;
				else
					abstopdelta = post->topdelta;

				int y1 = MAX(texdefpatch->originy + abstopdelta, 0);
				int y2 = MIN(texdefpatch->originy + abstopdelta + post->length - 1, height - 1);

				if (y1 <= y2)
				{
					byte* dest = texture->mData + x * height + y1;
					const byte* source = (byte*)post + 3;
					memcpy(dest, source, y2 - y1 + 1);
				}
			
				post = (post_t*)((byte*)post + post->length + 4);
			}
		}

		delete [] rawlumpdata;
	}
}


//
// TextureManager::getHandle
//
// Returns the handle for the texture that matches the supplied name.
//
texhandle_t TextureManager::getHandle(const char* name, Texture::TextureSourceType type)
{
	if (name[0] == '-')
		return NOT_FOUND_TEXTURE_HANDLE;

	texhandle_t handle = NOT_FOUND_TEXTURE_HANDLE;

	if (type == Texture::TEX_FLAT)
	{
		int lumpnum = W_CheckNumForName(name, ns_flats);
		if (lumpnum >= 0)
			handle = getFlatHandle(lumpnum);
	}
	else if (type == Texture::TEX_PATCH)
	{
	}
	else if (type == Texture::TEX_WALLTEXTURE)
	{
		std::string texname(name, 9);
		std::transform(texname.begin(), texname.end(), texname.begin(), toupper);

		std::map<std::string, unsigned int>::iterator it = mTextureNameTranslationMap.find(texname);
		if (it != mTextureNameTranslationMap.end())
			handle = getWallTextureHandle(it->second);
	}

	return handle;
}


//
// TextureManager::getTexture
//
// Returns the Texture for the appropriate handle. If the Texture is not
// currently cached, it will be loaded from the disk and cached.
//
const Texture* TextureManager::getTexture(texhandle_t handle) 
{
	Texture* texture = mHandleMap[handle];
	if (!texture)
	{
		DPrintf("cache miss 0x%x\n", handle);
		if (handle & FLAT_HANDLE_MASK)
			cacheFlat(handle);
//		else if (handle & PATCH_HANDLE_MASK)
//			cachePatch(handle);
		else if (handle & WALLTEXTURE_HANDLE_MASK)
			cacheWallTexture(handle);

		texture = mHandleMap[handle];
	}
	else
	{
		DPrintf("cache hit 0x%x\n", handle);
	}

	return texture;
}

VERSION_CONTROL (r_texture_cpp, "$Id: r_texture.cpp 3945 2013-07-03 14:32:48Z dr_sean $")



