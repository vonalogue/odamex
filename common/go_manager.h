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
// ComponentManager class Implementation
//
// ============================================================================

class ComponentManager
{
public:
	// ------------------------------------------------------------------------
	// Types
	// ------------------------------------------------------------------------
	
	typedef uint32_t ComponentId;

	// ------------------------------------------------------------------------
	// iterator
	// ------------------------------------------------------------------------

    class const_iterator : public std::iterator<std::forward_iterator_tag, ComponentManager>
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
		friend class ComponentManager;

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
	ComponentManager();
	virtual ~ComponentManager();


	const OString& getComponentTypeName(ComponentId component_id) const
	{
		return mComponents.get(component_id)->getTypeName();
	}

	template <typename T>
	T* getAttribute(ComponentId parent_id, const OString& attribute_name)
	{
		for (ParentToChildrenMap::iterator it = mParentToChildrenMap.find(parent_id);
				it != mParentToChildrenMap.end(); ++it)
		{
			ComponentId component_id = it->second;
			Component* component = mComponents.get(component_id);
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


	//
	// ComponentManager::getChildren
	//
	const_iterator getChildren(ComponentId parent_id)
	{
		const_iterator result;

		ParentToChildrenMap::const_iterator it = mParentToChildrenMap.find(parent_id);
		while (it != mParentToChildrenMap.end())
		{
			result.mComponentList.push_back(it->second);
			++it;
		}
		
		result.mIt = result.mComponentList.begin();
		return result;
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

	typedef SArray<Component*> ComponentStore;
	ComponentStore					mComponents;


	// ------------------------------------------------------------------------
	// Parent Component to Children Components mapping
	// ------------------------------------------------------------------------

	typedef std::multimap<ComponentId, ComponentId> ParentToChildrenMap;
	ParentToChildrenMap		mParentToChildrenMap;


};


#endif	// __GO_MANAGER_H__
