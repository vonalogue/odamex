// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2014 by The Odamex Team.
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
// ComponentTypeDatabase class Implementation
//
// ============================================================================

// ----------------------------------------------------------------------------
// Construction / Destruction functions
// ----------------------------------------------------------------------------

//
// ComponentTypeDatabase::ComponentTypeDatabase
//
ComponentTypeDatabase::ComponentTypeDatabase() :
	mTypes(128)
{ }


//
// ComponentTypeDatabase::~ComponentTypeDatabase
//
// 
ComponentTypeDatabase::~ComponentTypeDatabase()
{
	clearTypes();
}


// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------

//
// ComponentTypeDatabase::instance
//
// Returns an instance of the ComponentTypeDatabase Singleton.
//
ComponentTypeDatabase* ComponentTypeDatabase::instance()
{
	static ComponentTypeDatabase database;
	return &database;
}


//
// ComponentTypeDatabase::clearTypes
//
void ComponentTypeDatabase::clearTypes()
{
	for (TypeRecordTable::iterator it = mTypes.begin(); it != mTypes.end(); ++it)
		delete it->second.mPrototype;

	mTypes.clear();
}


//
// ComponentTypeDatabase::registerType
//
void ComponentTypeDatabase::registerType(
			const OString& type_name, const OString& type_parent_name,
			const Component& prototype)
{
	// check to see if the type is already registered
	TypeRecordTable::const_iterator it = mTypes.find(type_name);
	if (it != mTypes.end())
	{
		Net_Error("ComponentTypeDatabase::registerType: Attempting to register component " \
				"type %s more than once!\n", type_name.c_str());
		return;
	}

	mTypes.insert(std::make_pair(type_name, ComponentTypeRecord(type_name, type_parent_name, prototype)));
}


//
// ComponentTypeDatabase::unregisterType
//
void ComponentTypeDatabase::unregisterType(const OString& type_name)
{
	TypeRecordTable::iterator it = mTypes.find(type_name);
	if (it == mTypes.end())
	{
		Net_Error("ComponentTypeDatabase::unregisterType: Attempting to unregister component " \
				"type %s which has not been registered!\n", type_name.c_str());
		return;
	}

	mTypes.erase(it);
}


//
// ComponentTypeDatabase::buildComponent
//
Component* ComponentTypeDatabase::buildComponent(const OString& type_name) const
{
	TypeRecordTable::const_iterator it = mTypes.find(type_name);
	if (it == mTypes.end())
	{
		Net_Error("ComponentTypeDatabase::buildComponent: Attempting to build a component of " \
				"type %s which has not been registered!\n", type_name.c_str());
		return NULL;
	}

	return it->second.mPrototype->clone();
}
	

//
// ComponentTypeDatabase::descendant
//
// Returns true if the type type_name is descended from the type
// type_parent_name.
//
// TODO: Store a tree or graph containing the parent/child relationships
// for more efficient lookups.
//
bool ComponentTypeDatabase::descendant(const OString& type_name, const OString& type_parent_name) const
{
	TypeRecordTable::const_iterator it = mTypes.find(type_name);
	if (it == mTypes.end())
	{
		Net_Error("ComponentTypeDatabase::descendant: Unable to find component type %s!\n", type_name.c_str());
		return false;
	}

	static const OString BaseTypeName("base");

	if (type_parent_name == BaseTypeName)
		return true;

	while (it->second.mTypeParentName != BaseTypeName)
	{
		if (it->second.mTypeParentName == type_parent_name)
			return true;
		it = mTypes.find(it->second.mTypeParentName);
	}

	return false;
}

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

