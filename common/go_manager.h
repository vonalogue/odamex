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

#ifndef __GO_MANAGER_H__
#define __GO_MANAGER_H__

#include "doomtype.h"

#include <string>
#include "hashtable.h"

typedef uint32_t GameObjectId;
typedef uint32_t GameObjectComponentId;

class GameObjectComponent;

// ============================================================================
//
// GameObjectManager class Implementation
//
// ============================================================================

class GameObjectManager
{
public:

	// ------------------------------------------------------------------------
	// Public functions
	// ------------------------------------------------------------------------
	GameObjectManager();
	virtual ~GameObjectManager();


	template <class T>
	T& getComponent(const GameObjectId& object_id, const GameObjectComponentId& component_id)
	{
		ComponentIdToComponentMap::const_iterator it = mComponentIdToComponentMap.find(component_id);
		if (it != mComponentIdToComponentMap.end())
			return static_cast<T>(it->second);

		// TODO: handle not found errors
		return T();
	}

	void registerComponentType(const std::string& name, const GameObjectComponent& prototype);
	void unregisterComponentType(const std::string& name);
	void clearRegisteredComponentTypes();

private:
	typedef HashTable<GameObjectId, GameObjectComponentId> ObjectIdToComponentIdMap;
	typedef HashTable<GameObjectComponentId, GameObjectComponent*> ComponentIdToComponentMap;
	
	ObjectIdToComponentIdMap		mObjectIdToComponentIdMap;
	ComponentIdToComponentMap		mComponentIdToComponentMap;


	typedef HashTable<std::string, GameObjectComponent*> ComponentPrototypeMap;
	ComponentPrototypeMap			mComponentPrototypeMap;

};

#endif	// __GO_MANAGER_H__
