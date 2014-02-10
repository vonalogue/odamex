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

// ============================================================================
//
// ComponentManager class Implementation
//
// ============================================================================

// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------

ComponentManager::ComponentManager() :
	mComponents(ComponentManager::MAX_COMPONENTS),
	mComponentTypes(ComponentManager::MAX_TYPES)
{
	// register built-in component types
	registerComponentType(BoolComponent());
	registerComponentType(U8Component());
	registerComponentType(S8Component());
	registerComponentType(U16Component());
	registerComponentType(S16Component());
	registerComponentType(U32Component());
	registerComponentType(S32Component());
	registerComponentType(RangeComponent());
	registerComponentType(FloatComponent());
	registerComponentType(StringComponent());
	registerComponentType(V2FixedComponent());
	registerComponentType(V3FixedComponent());
	registerComponentType(BitFieldComponent());
	registerComponentType(Md5SumComponent());
}

ComponentManager::~ComponentManager()
{
	clearRegisteredComponentTypes();
}

void ComponentManager::registerComponentType(const Component& prototype)
{
	const OString& type_name = prototype.getTypeName();
	if (mComponentTypes.find(type_name) == mComponentTypes.end())
	{
		// Note: we're creating a new instance of the Component by
		// calling prototype.clone(). Remember to delete it later!
		ComponentTypeRecord rec(type_name, prototype.clone());
		mComponentTypes.insert(std::pair<OString, ComponentTypeRecord>(type_name, rec));
	}
}

void ComponentManager::unregisterComponentType(const OString& type_name)
{
	ComponentTypeStore::iterator it = mComponentTypes.find(type_name);
	if (it != mComponentTypes.end())
	{
		delete it->second.mPrototype;
		mComponentTypes.erase(it);
	}
}

void ComponentManager::clearRegisteredComponentTypes()
{
	for (ComponentTypeStore::iterator it = mComponentTypes.begin(); it != mComponentTypes.end(); ++it)
		delete it->second.mPrototype;
	
	mComponentTypes.clear();
}

ComponentManager::ComponentId ComponentManager::addAttribute(
		const OString& attribute_name, const OString& type_name, ComponentManager::ComponentId parent_id)
{
	ComponentTypeStore::const_iterator it = mComponentTypes.find(type_name);
	if (it != mComponentTypes.end())
	{
		Component* component = it->second.mPrototype->clone();
		component->mManager = this;
		component->setAttributeName(attribute_name);
		ComponentId component_id = mComponents.insert(component);
		mParentToChildrenMap.insert(std::pair<ComponentId, ComponentId>(parent_id, component_id));
		return component_id;
	}

	return 0;
}

void ComponentManager::clearComponents()
{
	for (ComponentStore::iterator it = mComponents.begin(); it != mComponents.end(); ++it)
		delete *it;
	mComponents.clear();
}

VERSION_CONTROL (go_manager_cpp, "$Id$")

