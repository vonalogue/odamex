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
#include "hashtable.h"
#include "sarray.h"
#include <map>
#include <algorithm>
#include <utility>
#include <list>
#include "m_ostring.h"

#include "go_component.h"


// ============================================================================
//
// GameObjectManager class Implementation
//
// ============================================================================

class GameObjectManager
{
public:
	// ------------------------------------------------------------------------
	// Types
	// ------------------------------------------------------------------------
	
	typedef uint32_t ComponentId;

	// ------------------------------------------------------------------------
	// iterator
	// ------------------------------------------------------------------------

    class const_iterator : public std::iterator<std::forward_iterator_tag, GameObjectManager>
	{
	public:
		bool operator== (const const_iterator& other)
		{
			return (mIt == mComponentList.end() && other.mComponentList.empty()) ||
					mIt == other.mIt;
		}

		bool operator!= (const const_iterator& other)
		{
			return mIt != other.mIt;
		}

		const ComponentId& operator* ()
		{
			return *mIt;
		}

		const ComponentId* operator-> ()
		{
			return &(*mIt);
		}

		const_iterator& operator++ ()
		{
			++mIt;
			return *this;
		}

		const_iterator operator++ (int)
		{
			const_iterator temp(*this);
			++mIt;
			return temp;
		}

	private:
		friend class GameObjectManager;

		void addComponent(ComponentId id)
		{
			mComponentList.push_back(id);
		}

		void removeComponent(ComponentId id)
		{
			std::list<ComponentId>::iterator it = std::find(mComponentList.begin(), mComponentList.end(), id);
			if (it != mComponentList.end())
				mComponentList.erase(it);
		}

		std::list<ComponentId>						mComponentList;
		std::list<ComponentId>::const_iterator		mIt;
	};

	// ------------------------------------------------------------------------
	// Public functions
	// ------------------------------------------------------------------------
	GameObjectManager();
	virtual ~GameObjectManager();

	void registerComponentType(const GameObjectComponent& prototype);
	void unregisterComponentType(const OString& type_name);
	void clearRegisteredComponentTypes();


	const OString& getComponentTypeName(ComponentId component_id) const
	{
		return mComponents.get(component_id)->getTypeName();
	}

	template <typename T>
	T* getAttribute(ComponentId parent_id, const OString& attribute_name)
	{
		for (CompositeMap::iterator it = mCompositeMap.find(parent_id); it != mCompositeMap.end(); ++it)
		{
			ComponentId component_id = it->second;
			GameObjectComponent* component = mComponents.get(component_id);
			if (component->getAttributeName() == attribute_name)
				return dynamic_cast<T*>(component);	
		}
		return NULL;	// not found
	}

	template <typename T>
	T* getAttribute(ComponentId component_id)
	{
		ComponentStore::iterator it = mComponents.find(component_id);
		if (it != mComponents.end())
			return dynamic_cast<T*>(*it);
		return NULL;	// not found
	}

	ComponentId addAttribute(const OString& attribute_name, const OString& type_name, ComponentId parent_id);

	void clearComponents();


	const_iterator getChildren(ComponentId parent_id)
	{
		return begin();	
	}

	const_iterator begin() const
	{
		const_iterator it;
		return it;
	}

	const_iterator end() const
	{
		return const_iterator();
	}

private:

	// ------------------------------------------------------------------------
	// Component Storage
	// ------------------------------------------------------------------------

	static const uint32_t MAX_COMPONENTS = 65536;

	typedef SArray<GameObjectComponent*> ComponentStore;
	ComponentStore					mComponents;


	// ------------------------------------------------------------------------
	// Component to Parent Component mapping
	// ------------------------------------------------------------------------

	typedef std::multimap<ComponentId, ComponentId> CompositeMap;
	CompositeMap				mCompositeMap;

	// ------------------------------------------------------------------------
	// Component Type Information
	// ------------------------------------------------------------------------

	static const uint32_t MAX_TYPES = 64;
	
	struct ComponentTypeRecord
	{
		ComponentTypeRecord() :
			mPrototype(NULL), mComposite(false)
		{ }

		ComponentTypeRecord(const OString& type_name, const GameObjectComponent* prototype) :
			mTypeName(type_name), mPrototype(prototype), mComposite(prototype->isComposite())
		{ }

		OString						mTypeName;
		const GameObjectComponent*	mPrototype;
		bool						mComposite;
	};

	typedef OHashTable<OString, ComponentTypeRecord> ComponentTypeStore;
	ComponentTypeStore			mComponentTypes;
};

#endif	// __GO_MANAGER_H__
