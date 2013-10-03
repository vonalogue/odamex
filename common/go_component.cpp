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
// GameObjectComponents are building-block elements that comprise GameObjects.
// GameObjectComponents use the Composite Pattern to treat composite components
// such as GameObjectComponentGroup the same as primative GameObjectComponents
// such as StringComponent.
//
// GameObjectComponents are data types that know how to serialize and deserialize
// themselves to and from a BitStream.
//
// GameObjectComponents have a clone operation to create a new instance of themselves.
// This is part of the Prototype Patter and is a mechanism that allows
// a prototype instance of each Message type to be built
//
//-----------------------------------------------------------------------------

#include "net_log.h"
#include "net_common.h"
#include "go_component.h"
#include "hashtable.h"
#include "m_ostring.h"


// ============================================================================
//
// IntegralComponent implementation
//
// ============================================================================

template<>
const OString BoolComponent::mTypeName("bool");

template<>
const OString U8Component::mTypeName("u8");

template<>
const OString S8Component::mTypeName("s8");

template<>
const OString U16Component::mTypeName("u16");

template<>
const OString S16Component::mTypeName("s16");

template<>
const OString U32Component::mTypeName("u32");

template<>
const OString S32Component::mTypeName("s32");


// ============================================================================
//
// RangeComponent implementation
// 
// ============================================================================

const OString RangeComponent::mTypeName("range");

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


// ============================================================================
//
// StringComponent implementation
//
// ============================================================================

const OString StringComponent::mTypeName("string");


// ============================================================================
//
// V2FixedComponent implementation
// 
// ============================================================================

const OString V2FixedComponent::mTypeName("v2fixed");


// ============================================================================
//
// V3FixedComponent implementation
// 
// ============================================================================

const OString V3FixedComponent::mTypeName("v3fixed");


// ============================================================================
//
// BitFieldComponent implementation
// 
// ============================================================================

const OString BitFieldComponent::mTypeName("bitfield");

BitFieldComponent::BitFieldComponent(uint32_t num_fields) :
	mBitField(num_fields)
{ }

BitFieldComponent::BitFieldComponent(const BitField& value) :
	mBitField(value)
{ }

uint16_t BitFieldComponent::read(BitStream& stream)
{
	mBitField.clear();
	for (size_t i = 0; i < mBitField.size(); ++i)
		if (stream.readBit() == true)
			mBitField.set(i);

	return mBitField.size();
}

uint16_t BitFieldComponent::write(BitStream& stream) const
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
// GameObjectComponentArray implementation
// 
// ============================================================================

GameObjectComponentArray::GameObjectComponentArray(uint32_t mincnt, uint32_t maxcnt) :
	mCachedSizeValid(false), mCachedSize(0), 
	mMinCount(mincnt), mMaxCount(maxcnt), mCountField(0, mincnt, maxcnt)
{ }

GameObjectComponentArray::GameObjectComponentArray(const GameObjectComponentArray& other) :
	mCachedSizeValid(other.mCachedSizeValid), mCachedSize(other.mCachedSize),
	mMinCount(other.mMinCount), mMaxCount(other.mMaxCount),
	mCountField(other.mCountField)
{
	for (size_t i = 0; i < other.mFields.size(); ++i)
		mFields.push_back(other.mFields[i]->clone());
}

GameObjectComponentArray::~GameObjectComponentArray()
{
	for (size_t i = 0; i < mFields.size(); ++i)
		delete mFields[i];
}


GameObjectComponentArray& GameObjectComponentArray::operator=(const GameObjectComponentArray& other)
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
// GameObjectComponentArray::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t GameObjectComponentArray::size() const
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
// GameObjectComponentArray::read
//
// Reads an array of GameObjectComponents from a BitStream. Returns the
// number of bits read.
//
uint16_t GameObjectComponentArray::read(BitStream& stream)
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
// GameObjectComponentArray::write
//
// Writes an array of GameObjectComponents into a BitStream. Returns the
// number of bits written.
//
uint16_t GameObjectComponentArray::write(BitStream& stream) const
{
	uint16_t write_size = 0;
	
	if (mMaxCount - mMinCount > 0)
		write_size += mCountField.write(stream);

	for (int32_t i = 0; i < mCountField.get(); ++i)
		write_size += mFields[i]->write(stream);

	return write_size;
}


//
// GameObjectComponentArray::clear
//
// Clears all of the repeated GameObjectComponents.
//
void GameObjectComponentArray::clear()
{
	mCountField.set(0);
	for (size_t i = 0; i < mFields.size(); ++i)
		mFields[i]->clear();

	mCachedSizeValid = false;
}


// ============================================================================
//
// GameObjectComponentGroup implementation
// 
// ============================================================================

GameObjectComponentGroup::GameObjectComponentGroup() :
	mCachedSizeValid(false), mCachedSize(0)
{ }

GameObjectComponentGroup::GameObjectComponentGroup(const GameObjectComponentGroup& other) :
	mCachedSizeValid(other.mCachedSizeValid), mCachedSize(other.mCachedSize)
{
}

GameObjectComponentGroup::~GameObjectComponentGroup()
{
}


//
// GameObjectComponentGroup::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t GameObjectComponentGroup::size() const
{
	if (!mCachedSizeValid)
	{
		mCachedSize = 0;
		mCachedSizeValid = true;
	}

	return mCachedSize;
}


//
// GameObjectComponentGroup::read
//
// Reads a group of required and optional GameObjectComponents from a BitStream.
// Returns the number of bits read.
//
uint16_t GameObjectComponentGroup::read(BitStream& stream)
{
	uint16_t read_size = 0;
	clear();
	return read_size;
}


//
// GameObjectComponentGroup::write
//
// Writes a group of required and optional GameObjectComponents from a BitStream.
// Returns the number of bits written.
//
uint16_t GameObjectComponentGroup::write(BitStream& stream) const
{
	uint16_t write_size = 0;
	return write_size;
}


//
// GameObjectComponentGroup::clear
//
// Clears all of the GameObjectComponents in this group.
//
void GameObjectComponentGroup::clear()
{
	mCachedSizeValid = false;
}


VERSION_CONTROL (go_component_cpp, "$Id$")

