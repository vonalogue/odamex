// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
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
// Main common processing for Client/Server functionality 
//
//-----------------------------------------------------------------------------

#ifndef __CS_MAIN_H__
#define __CS_MAIN_H__

#include "doomtype.h"
#include <string>

void CS_OpenNetInterface(const std::string& address, uint16_t port);
void CS_CloseNetInterface();
void CS_ServiceNetInterface();


void CS_OpenConnection(const std::string& socket_address_string);
void CS_CloseConnection();
bool CS_IsConnected();

#endif	// __CS_MAIN_H__
