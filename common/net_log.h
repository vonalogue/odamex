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
//  
// Message logging for the network subsystem
//
//-----------------------------------------------------------------------------

#ifndef __NET_LOG_H__
#define __NET_LOG_H__

#define __NET_DEBUG__

void Net_Printf(const char *str, ...);
void Net_Error(const char *str, ...);
void Net_Warning(const char *str, ...);

#ifdef __NET_DEBUG__
void Net_LogPrintf(const char *str, ...);
#else
#define Net_LogPrintf(fmt, ...) { }
#endif	// __NET_DEBUG__

#endif	// __NET_LOG_H__

