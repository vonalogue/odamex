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
// Components are building-block elements that comprise GameObjects.
// Components use the Composite Pattern to treat composite components
// such as ComponentGroup the same as primative Components such as
// StringComponent.
//
// Components are data types that know how to serialize and deserialize
// themselves to and from a BitStream.
//
// Components have a clone operation to create a new instance of themselves.
// This is part of the Prototype Patter and is a mechanism that allows
// a prototype instance of each Message type to be built
//
//-----------------------------------------------------------------------------

#include "net_log.h"
#include "net_common.h"
#include "go_component.h"
#include "hashtable.h"
#include "m_ostring.h"

// All component classes are considered to be derived initially from
// the component type "base" as the root class.
static const OString BaseTypeName("base");

// ============================================================================
//
// IntegralComponent implementation
//
// ============================================================================

template<>
const OString BoolComponent::mTypeName("bool");
template<>
const OString BoolComponent::mTypeParentName(BaseTypeName);

template<>
const OString U8Component::mTypeName("u8");
template<>
const OString U8Component::mTypeParentName(BaseTypeName);

template<>
const OString S8Component::mTypeName("s8");
template<>
const OString S8Component::mTypeParentName(BaseTypeName);

template<>
const OString U16Component::mTypeName("u16");
template<>
const OString U16Component::mTypeParentName(BaseTypeName);

template<>
const OString S16Component::mTypeName("s16");
template<>
const OString S16Component::mTypeParentName(BaseTypeName);

template<>
const OString U32Component::mTypeName("u32");
template<>
const OString U32Component::mTypeParentName(BaseTypeName);

template<>
const OString S32Component::mTypeName("s32");
template<>
const OString S32Component::mTypeParentName(BaseTypeName);


// ============================================================================
//
// RangeComponent implementation
// 
// ============================================================================

const OString RangeComponent::mTypeName("range");
const OString RangeComponent::mTypeParentName(BaseTypeName);

RangeComponent::RangeComponent() :
	mCachedSizeValid(false), mCachedSize(0),
	mValue(0), mLowerBound(MININT), mUpperBound(MAXINT)
{ }

RangeComponent::RangeComponent(int32_t lowerbound, int32_t upperbound, int32_t value) :
	mCachedSizeValid(false), mCachedSize(0),	
	mValue(value), mLowerBound(lowerbound), mUpperBound(upperbound)
{
	if (mLowerBound > mUpperBound)
		std::swap(mLowerBound, mUpperBound);
}

//
// RangeComponent::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t RangeComponent::size() const
{
	if (!mCachedSizeValid)
	{
		mCachedSize = Net_BitsNeeded(mUpperBound - mLowerBound);
		mCachedSizeValid = true;
	}

	return mCachedSize;
}


//
// RangeComponent::read
//
uint16_t RangeComponent::read(BitStream& stream)
{
	mValue = mLowerBound + stream.readBits(size());
	return size();
}


//
// RangeComponent::write
//
uint16_t RangeComponent::write(BitStream& stream) const
{
	stream.writeBits(mValue - mLowerBound, size());
	return size();
}


// ============================================================================
//
// FloatComponent implementation
//
// ============================================================================

const OString FloatComponent::mTypeName("float");
const OString FloatComponent::mTypeParentName(BaseTypeName);


// ============================================================================
//
// StringComponent implementation
//
// ============================================================================

const OString StringComponent::mTypeName("string");
const OString StringComponent::mTypeParentName(BaseTypeName);


// ============================================================================
//
// V2FixedComponent implementation
// 
// ============================================================================

const OString V2FixedComponent::mTypeName("v2fixed");
const OString V2FixedComponent::mTypeParentName(BaseTypeName);


// ============================================================================
//
// V3FixedComponent implementation
// 
// ============================================================================

const OString V3FixedComponent::mTypeName("v3fixed");
const OString V3FixedComponent::mTypeParentName(BaseTypeName);


// ============================================================================
//
// BitFieldComponent implementation
// 
// ============================================================================

template <uint32_t N>
const OString BitFieldComponent<N>::mTypeName("bitfield" + N);

template <uint32_t N>
const OString BitFieldComponent<N>::mTypeParentName(BaseTypeName);

template <uint32_t N>
BitFieldComponent<N>::BitFieldComponent()
{ }

template <uint32_t N>
BitFieldComponent<N>::BitFieldComponent(const BitField<N>& value) :
	mBitField(value)
{ }

template <uint32_t N>
uint16_t BitFieldComponent<N>::read(BitStream& stream)
{
	mBitField.clear();
	for (size_t i = 0; i < mBitField.size(); ++i)
		if (stream.readBit() == true)
			mBitField.set(i);

	return mBitField.size();
}

template <uint32_t N>
uint16_t BitFieldComponent<N>::write(BitStream& stream) const
{
	for (size_t i = 0; i < mBitField.size(); ++i)
		stream.writeBit(mBitField.get(i));

	return mBitField.size();
}


// ============================================================================
//
// Md5SumComponent implementation
// 
// ============================================================================

const OString Md5SumComponent::mTypeName("md5sum");
const OString Md5SumComponent::mTypeParentName(BaseTypeName);

Md5SumComponent::Md5SumComponent() 
{
	clear();
}

Md5SumComponent::Md5SumComponent(const OString& value)
{
	setFromString(value);
}


//
// Md5SumComponent::read
//
uint16_t Md5SumComponent::read(BitStream& stream)
{
	for (size_t i = 0; i < NUMBYTES; ++i)
		mValue[i] = stream.readU8();
	cacheString();

	return NUMBITS;
}


//
// Md5SumComponent::write
//
uint16_t Md5SumComponent::write(BitStream& stream) const
{
	for (size_t i = 0; i < NUMBYTES; ++i)
		stream.writeU8(mValue[i]);

	return NUMBITS;
}


//
// Md5SumComponent::clear
//
// Sets the value to all zeros
//
void Md5SumComponent::clear()
{
	memset(mValue, 0, sizeof(mValue));
	mCachedString.clear();
}

//
// Md5SumComponent::setFromString
//
// Converts a OString version of a MD5SUM into the compact internal format.
//
void Md5SumComponent::setFromString(const OString& value)
{
	clear();

	if (value.length() == 2*NUMBYTES)
	{
		const char* pch = value.c_str();
		for (size_t i = 0; i < NUMBYTES; ++i)
		{
			for (size_t j = 0; j < 2; j++, pch++)
			{
				mValue[i] <<= 4;

				if (*pch >= '0' && *pch <= '9')
					mValue[i] += *pch - '0';
				else if (*pch >= 'a' && *pch <= 'f')
					mValue[i] += *pch - 'a' + 10;
				else if (*pch >= 'A' && *pch <= 'F')
					mValue[i] += *pch - 'A' + 10;
				else
				{
					clear();
					return;
				}
			}
		}

		mCachedString = value;
	}
}


//
// Md5SumComponent::cacheString
//
// Builds a OString version of the MD5SUM and caches it.
//
void Md5SumComponent::cacheString()
{
	static const char hextable[] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F' };

	const size_t bufsize = 2*NUMBYTES + 1;
	char buf[bufsize];
	char* pch = buf;

	for (size_t i = 0; i < NUMBYTES; i++)
	{
		*(pch++) = hextable[mValue[i] >> 4];
		*(pch++) = hextable[mValue[i] & 0xF];
	}	

	buf[bufsize - 1] = '\0';
	mCachedString = buf;
}



// ============================================================================
//
// ComponentArray implementation
// 
// ============================================================================

ComponentArray::ComponentArray(uint32_t mincnt, uint32_t maxcnt) :
	mCachedSizeValid(false), mCachedSize(0), 
	mMinCount(mincnt), mMaxCount(maxcnt), mCountField(0, mincnt, maxcnt)
{ }

ComponentArray::ComponentArray(const ComponentArray& other) :
	mCachedSizeValid(other.mCachedSizeValid), mCachedSize(other.mCachedSize),
	mMinCount(other.mMinCount), mMaxCount(other.mMaxCount),
	mCountField(other.mCountField)
{
	for (size_t i = 0; i < other.mFields.size(); ++i)
		mFields.push_back(other.mFields[i]->clone());
}

ComponentArray::~ComponentArray()
{
	for (size_t i = 0; i < mFields.size(); ++i)
		delete mFields[i];
}


ComponentArray& ComponentArray::operator=(const ComponentArray& other)
{
	if (&other != this)
	{
		for (size_t i = 0; i < mFields.size(); ++i)
			delete mFields[i];
		mFields.clear();

		for (size_t i = 0; i < other.mFields.size(); ++i)
			mFields.push_back(other.mFields[i]->clone());

		mCachedSizeValid = other.mCachedSizeValid;
		mCachedSize = other.mCachedSize;
		mMinCount = other.mMinCount;
		mMaxCount = other.mMaxCount;
		mCountField = other.mCountField;
	}

	return *this;
}

//
// ComponentArray::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t ComponentArray::size() const
{
	if (!mCachedSizeValid)
	{
		mCachedSize = 0;
	
		if (mMaxCount - mMinCount > 0)
			mCachedSize += mCountField.size();

		for (int32_t i = 0; i < mCountField.get(); ++i)
			mCachedSize += mFields[i]->size();

		mCachedSizeValid = true;
	}

	return mCachedSize;
}


//
// ComponentArray::read
//
// Reads an array of Components from a BitStream. Returns the
// number of bits read.
//
uint16_t ComponentArray::read(BitStream& stream)
{
	uint16_t read_size = 0;
	clear();

	if (mMaxCount - mMinCount > 0)
		read_size += mCountField.read(stream);

	for (int32_t i = 0; i < mCountField.get(); ++i)
	{
				

	}

	return read_size;
}


//
// ComponentArray::write
//
// Writes an array of Components into a BitStream. Returns the
// number of bits written.
//
uint16_t ComponentArray::write(BitStream& stream) const
{
	uint16_t write_size = 0;
	
	if (mMaxCount - mMinCount > 0)
		write_size += mCountField.write(stream);

	for (int32_t i = 0; i < mCountField.get(); ++i)
		write_size += mFields[i]->write(stream);

	return write_size;
}


//
// ComponentArray::clear
//
// Clears all of the repeated Components.
//
void ComponentArray::clear()
{
	mCountField.set(0);
	for (size_t i = 0; i < mFields.size(); ++i)
		mFields[i]->clear();

	mCachedSizeValid = false;
}


// ============================================================================
//
// ComponentGroup implementation
// 
// ============================================================================

ComponentGroup::ComponentGroup() :
	mCachedSizeValid(false), mCachedSize(0)
{ }

ComponentGroup::ComponentGroup(const ComponentGroup& other) :
	mCachedSizeValid(other.mCachedSizeValid), mCachedSize(other.mCachedSize)
{
}

ComponentGroup::~ComponentGroup()
{
}


//
// ComponentGroup::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t ComponentGroup::size() const
{
	if (!mCachedSizeValid)
	{
		mCachedSize = 0;
		mCachedSizeValid = true;
	}

	return mCachedSize;
}


//
// ComponentGroup::read
//
// Reads a group of required and optional Components from a BitStream.
// Returns the number of bits read.
//
uint16_t ComponentGroup::read(BitStream& stream)
{
	uint16_t read_size = 0;
	clear();
	return read_size;
}


//
// ComponentGroup::write
//
// Writes a group of required and optional Components from a BitStream.
// Returns the number of bits written.
//
uint16_t ComponentGroup::write(BitStream& stream) const
{
	uint16_t write_size = 0;
	return write_size;
}


//
// ComponentGroup::clear
//
// Clears all of the Components in this group.
//
void ComponentGroup::clear()
{
	mCachedSizeValid = false;
}



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
// Sets the initial size of the mTypes hash table to hold 128 elements.
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

// ----------------------------------------------------------------------------
//
// Non-member functions
//
// ----------------------------------------------------------------------------

//
// RegisterBuiltInComponentType
//
// Helper function for RegisterBuiltInComponentTypes.
//
template <typename T>
static void RegisterBuiltInComponentType()
{
	const T prototype;
	ComponentTypeDatabase::instance()->registerType(
		prototype.getTypeName(), prototype.getTypeParentName(), prototype);
}


//
// RegisterBuiltInComponentTypes
//
// Registers all of the built-in component types with the ComponentTypeDatabase.
//
void RegisterBuiltInComponentTypes()
{
	RegisterBuiltInComponentType<BoolComponent>();
	RegisterBuiltInComponentType<U8Component>();
	RegisterBuiltInComponentType<S8Component>();
	RegisterBuiltInComponentType<U16Component>();
	RegisterBuiltInComponentType<S16Component>();
	RegisterBuiltInComponentType<U32Component>();
	RegisterBuiltInComponentType<S32Component>();
	RegisterBuiltInComponentType<RangeComponent>();
	RegisterBuiltInComponentType<FloatComponent>();
	RegisterBuiltInComponentType<StringComponent>();
	RegisterBuiltInComponentType<BitFieldComponent<32> >();
	RegisterBuiltInComponentType<Md5SumComponent>();
	RegisterBuiltInComponentType<V2FixedComponent>();
	RegisterBuiltInComponentType<V3FixedComponent>();
}



VERSION_CONTROL (go_component_cpp, "$Id$")

