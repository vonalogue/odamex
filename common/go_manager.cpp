// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2015 by The Odamex Team.
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
// The ComponentManager class handles the creation of new Components based
// on a prototype.
//
//-----------------------------------------------------------------------------

#include "go_manager.h"
#include "go_component.h"
#include "net_log.h"


// ============================================================================
//
// ComponentManager class Implementation
//
// ============================================================================

// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------

ComponentManager::ComponentManager() :
	mComponents(ComponentManager::MAX_COMPONENTS)
{ }

ComponentManager::~ComponentManager()
{ }

void ComponentManager::clearComponents()
{
	for (ComponentStore::iterator it = mComponents.begin(); it != mComponents.end(); ++it)
		delete *it;
	mComponents.clear();
}

VERSION_CONTROL (go_manager_cpp, "$Id$")

