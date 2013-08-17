// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
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
// MessageFields are building-block elements that comprise Messages.
// MessageFields use the Composite Pattern to treat composite MessageFields
// such as MessageFieldGroup the same as primative MessageFields such as
// StringMessageField.
//
// MessageFields are data types that know how to serialize and deserialize
// themselves to and from a BitStream.
//
// MessageFields have a clone operation to create a new instance of themselves.
// This is part of the Prototype Patter and is a mechanism that allows
// a prototype instance of each Message type to be built
//
//-----------------------------------------------------------------------------

#include "net_log.h"
#include "net_common.h"
#include "net_messagefield.h"

// ----------------------------------------------------------------------------

// ============================================================================
//
// RangeMessageField implementation
// 
// ============================================================================

RangeMessageField::RangeMessageField(int32_t lowerbound, int32_t upperbound, int32_t value) :
	mCachedSizeValid(false), mCachedSize(0),	
	mValue(value), mLowerBound(lowerbound), mUpperBound(upperbound)
{
	if (mLowerBound > mUpperBound)
		std::swap(mLowerBound, mUpperBound);
}

//
// RangeMessageField::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t RangeMessageField::size() const
{
	if (!mCachedSizeValid)
	{
		mCachedSize = Net_BitsNeeded(mUpperBound - mLowerBound);
		mCachedSizeValid = true;
	}

	return mCachedSize;
}


//
// RangeMessageField::read
//
uint16_t RangeMessageField::read(BitStream& stream)
{
	mValue = mLowerBound + stream.readBits(size());
	return size();
}


//
// RangeMessageField::write
//
uint16_t RangeMessageField::write(BitStream& stream) const
{
	stream.writeBits(mValue - mLowerBound, size());
	return size();
}


// ============================================================================
//
// BitFieldMessageField implementation
// 
// ============================================================================

BitFieldMessageField::BitFieldMessageField(uint32_t num_fields) :
	mBitField(num_fields)
{ }

BitFieldMessageField::BitFieldMessageField(const BitField& value) :
	mValid(false), mBitField(value)
{ }

uint16_t BitFieldMessageField::read(BitStream& stream)
{
	mBitField.clear();
	for (size_t i = 0; i < mBitField.size(); ++i)
		if (stream.readBit() == true)
			mBitField.set(i);

	return mBitField.size();
}

uint16_t BitFieldMessageField::write(BitStream& stream) const
{
	for (size_t i = 0; i < mBitField.size(); ++i)
		stream.writeBit(mBitField.get(i));

	return mBitField.size();
}


// ============================================================================
//
// Md5SumMessageField implementation
// 
// ============================================================================


Md5SumMessageField::Md5SumMessageField() : 
	mValid(false)
{
	clear();
}

Md5SumMessageField::Md5SumMessageField(const std::string& value)
{
	setFromString(value);
}


//
// Md5SumMessageField::read
//
uint16_t Md5SumMessageField::read(BitStream& stream)
{
	for (size_t i = 0; i < NUMBYTES; ++i)
		mValue[i] = stream.readU8();
	cacheString();

	return NUMBITS;
}


//
// Md5SumMessageField::write
//
uint16_t Md5SumMessageField::write(BitStream& stream) const
{
	for (size_t i = 0; i < NUMBYTES; ++i)
		stream.writeU8(mValue[i]);

	return NUMBITS;
}


//
// Md5SumMessageField::clear
//
// Sets the value to all zeros and indicates that the value is not valid
//
void Md5SumMessageField::clear()
{
	memset(mValue, 0, sizeof(mValue));
	mCachedString.clear();
	mValid = false;
}

//
// Md5SumMessageField::setFromString
//
// Converts a std::string version of a MD5SUM into the compact internal format.
//
void Md5SumMessageField::setFromString(const std::string& value)
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
		mValid = true;
	}
}


//
// Md5SumMessageField::cacheString
//
// Builds a std::string version of the MD5SUM and caches it.
//
void Md5SumMessageField::cacheString()
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
// MessageFieldGroup implementation
// 
// ============================================================================

MessageFieldGroup::MessageFieldGroup() :
	mCachedSizeValid(false), mCachedSize(0), mValid(false)
{ }

MessageFieldGroup::MessageFieldGroup(const MessageFieldGroup& other) :
	mCachedSizeValid(other.mCachedSizeValid), mCachedSize(other.mCachedSize),
	mValid(other.mValid),
	mOptionalIndicator(other.mOptionalIndicator)
{
	for (size_t i = 0; i < other.mOptionalFields.size(); ++i)
		mOptionalFields.push_back(other.mOptionalFields[i]->clone());
	for (size_t i = 0; i < other.mRequiredFields.size(); ++i)
		mRequiredFields.push_back(other.mRequiredFields[i]->clone());
}

MessageFieldGroup::~MessageFieldGroup()
{
	for (size_t i = 0; i < mOptionalFields.size(); ++i)
		delete mOptionalFields[i];
	for (size_t i = 0; i < mRequiredFields.size(); ++i)
		delete mRequiredFields[i];
}


//
// MessageFieldGroup::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t MessageFieldGroup::size() const
{
	if (!mCachedSizeValid)
	{
		mCachedSize = mOptionalIndicator.size();
		for (size_t i = 0; i < mOptionalFields.size(); ++i)
			mCachedSize += mOptionalFields[i]->size();

		for (size_t i = 0; i < mRequiredFields.size(); ++i)
			mCachedSize += mRequiredFields[i]->size();

		mCachedSizeValid = true;
	}

	return mCachedSize;
}


//
// MessageFieldGroup::read
//
// Reads a group of required and optional MessageFields from a BitStream.
// Returns the number of bits read.
//
uint16_t MessageFieldGroup::read(BitStream& stream)
{
	uint16_t read_size = 0;
	clear();

	const BitField& bitfield = mOptionalIndicator.get();

	// read the optional fields
	read_size += mOptionalIndicator.read(stream);
	for (size_t i = 0; i < mOptionalFields.size(); ++i)
	{
		if (bitfield.get(i) == true)
			read_size += mOptionalFields[i]->read(stream);
	}

	// read the required fields
	for (size_t i = 0; i < mRequiredFields.size(); ++i)
		read_size += mRequiredFields[i]->read(stream);

	return read_size;
}


//
// MessageFieldGroup::write
//
// Writes a group of required and optional MessageFields from a BitStream.
// Returns the number of bits written.
//
uint16_t MessageFieldGroup::write(BitStream& stream) const
{
	uint16_t write_size = 0;
	const BitField& bitfield = mOptionalIndicator.get();

	// write the optional fields
	write_size += mOptionalIndicator.write(stream);

	for (size_t i = 0; i < mOptionalFields.size(); ++i)
	{
		if (bitfield.get(i) == true)
			write_size += mOptionalFields[i]->write(stream);
	}

	// write the required fields
	for (size_t i = 0; i < mRequiredFields.size(); ++i)
		write_size += mRequiredFields[i]->write(stream);

	return write_size;
}


//
// MessageFieldGroup::clear
//
// Clears all of the MessageFields in this group.
//
void MessageFieldGroup::clear()
{
	// clear the optional fields
	mOptionalIndicator.clear();
	for (size_t i = 0; i < mOptionalFields.size(); ++i)
		mOptionalFields[i]->clear();

	// clear the required fields
	for (size_t i = 0; i < mRequiredFields.size(); ++i)
		mRequiredFields[i]->clear();

	mCachedSizeValid = false;
}


//
// MessageFieldGroup::addField
//
// Adds a field to the container.
//
void MessageFieldGroup::addField(MessageField* field, bool optional)
{
	if (field == NULL)
		return;

	// check if a field with this name already exists
	NameTable::const_iterator itr = mNameTable.find(field->getFieldName());
	if (itr != mNameTable.end())
	{
		Net_Warning("MessageFieldGroup::addField: field name %s already exists.\n", field->getFieldName().c_str());
		return;
	}

	// add it to the name table
	mNameTable.insert(std::pair<std::string, MessageField*>(field->getFieldName(), field));

	if (optional)
	{
		// TODO: increase the size of the optional field bitfield
		mOptionalFields.push_back(field);
	}
	else
	{
		mRequiredFields.push_back(field);
	}
}

// ============================================================================
//
// MessageFieldArray implementation
// 
// ============================================================================

MessageFieldArray::MessageFieldArray(uint32_t mincnt, uint32_t maxcnt) :
	mCachedSizeValid(false), mCachedSize(0), mValid(mincnt == 0),
	mMinCount(mincnt), mMaxCount(maxcnt), mCountField(0, mincnt, maxcnt)
{ }

MessageFieldArray::MessageFieldArray(const MessageFieldArray& other) :
	mCachedSizeValid(other.mCachedSizeValid), mCachedSize(other.mCachedSize),
	mValid(other.mValid),
	mMinCount(other.mMinCount), mMaxCount(other.mMaxCount),
	mCountField(other.mCountField)
{
	for (size_t i = 0; i < other.mFields.size(); ++i)
		mFields.push_back(other.mFields[i]->clone());
}

MessageFieldArray::~MessageFieldArray()
{
	for (size_t i = 0; i < mFields.size(); ++i)
		delete mFields[i];
}


MessageFieldArray& MessageFieldArray::operator=(const MessageFieldArray& other)
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
		mValid = other.mValid;
		mMinCount = other.mMinCount;
		mMaxCount = other.mMaxCount;
		mCountField = other.mCountField;
	}

	return *this;
}

//
// MessageFieldArray::size
//
// Returns the total size of the message fields in bits. The size is cached for
// fast retrieval in the future;
//
uint16_t MessageFieldArray::size() const
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
// MessageFieldArray::read
//
// Reads an array of message fields from a BitStream. Returns the number of
// bits read.
//
uint16_t MessageFieldArray::read(BitStream& stream)
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
// MessageFieldArray::write
//
// Writes an array of message fields from a BitStream. Returns the number of
// bits written
//
uint16_t MessageFieldArray::write(BitStream& stream) const
{
	uint16_t write_size = 0;
	
	if (mMaxCount - mMinCount > 0)
		write_size += mCountField.write(stream);

	for (int32_t i = 0; i < mCountField.get(); ++i)
		write_size += mFields[i]->write(stream);

	return write_size;
}


//
// MessageFieldArray::clear
//
// Clears all of the repeated MessageFields .
//
void MessageFieldArray::clear()
{
	mCountField.set(0);
	for (size_t i = 0; i < mFields.size(); ++i)
		mFields[i]->clear();

	mCachedSizeValid = false;
}


VERSION_CONTROL (net_messagefield_cpp, "$Id$")

