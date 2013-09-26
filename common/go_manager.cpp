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
// The GameObjectManager class handles the creation of new GameObjects based
// on a prototype.
//
//-----------------------------------------------------------------------------

#include "go_manager.h"
#include "go_component.h"

// ============================================================================
//
// GameObjectManager class Implementation
//
// ============================================================================

// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------

GameObjectManager::GameObjectManager()
{
	// register built-in component types
	registerComponentType("bool",		BoolComponent());
	registerComponentType("u8",			U8Component());
	registerComponentType("s8",			S8Component());
	registerComponentType("u16",		U16Component());
	registerComponentType("s16",		S16Component());
	registerComponentType("u32",		U32Component());
	registerComponentType("s32",		S32Component());
	registerComponentType("range",		RangeComponent());
	registerComponentType("float",		FloatComponent());
	registerComponentType("string",		StringComponent());
	registerComponentType("v2fixed",	V2FixedComponent());
	registerComponentType("v3fixed",	V3FixedComponent());
	registerComponentType("bitfield",	BitFieldComponent());
	registerComponentType("md5sum",		Md5SumComponent());
}

GameObjectManager::~GameObjectManager()
{
	clearRegisteredComponentTypes();
}

void GameObjectManager::registerComponentType(const std::string& name, const GameObjectComponent& prototype)
{
	if (mComponentPrototypeMap.find(name) == mComponentPrototypeMap.end())
	{	
		// Note: we're creating a new instance of the GameObjectComponent
		// remember to delete it later!
		mComponentPrototypeMap.insert(std::pair<std::string, GameObjectComponent*>(name, prototype.clone()));
	}
}

void GameObjectManager::unregisterComponentType(const std::string& name)
{
	ComponentPrototypeMap::iterator it = mComponentPrototypeMap.find(name);
	if (it != mComponentPrototypeMap.end())
	{
		delete it->second;
		mComponentPrototypeMap.erase(it);
	}
}

void GameObjectManager::clearRegisteredComponentTypes()
{
	for (ComponentPrototypeMap::iterator it = mComponentPrototypeMap.begin();
		it != mComponentPrototypeMap.end();
		++it)
	{
		delete it->second;
	}
	
	mComponentPrototypeMap.clear();
}

VERSION_CONTROL (go_manager_cpp, "$Id$")

