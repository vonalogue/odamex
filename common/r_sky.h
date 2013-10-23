// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: r_sky.h 1788 2010-08-24 04:42:57Z russellrice $
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
//	Sky rendering.
//
//-----------------------------------------------------------------------------


#ifndef __R_SKY_H__
#define __R_SKY_H__

#include "c_cvars.h"

// SKY, store the number for name.
extern char SKYFLATNAME[8];

extern texhandle_t		sky1flathandle;
extern texhandle_t		sky2flathandle;

extern texhandle_t sky1texhandle;
extern texhandle_t sky2texhandle;

EXTERN_CVAR (r_stretchsky)

// Called whenever the sky changes.
bool R_IsSkyFlat(texhandle_t handle);
void R_InitSkyMap		();

void R_RenderSkyRange(visplane_t* pl);

#endif //__R_SKY_H__

