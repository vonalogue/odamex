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

#ifndef __NET_MESSAGEFIELD_H__
#define __NET_MESSAGEFIELD_H__

#include "net_type.h"
#include "net_bitstream.h"
#include <string>
#include <map>
#include <list>
#include <stack>

// ----------------------------------------------------------------------------
// Forward declarations
// ----------------------------------------------------------------------------

template<typename T, uint16_t SIZE = 8*sizeof(T)> class IntegralMessageField;
typedef IntegralMessageField<bool, 1> BoolMessageField;
typedef IntegralMessageField<uint8_t> U8MessageField;
typedef IntegralMessageField<int8_t> S8MessageField;
typedef IntegralMessageField<uint16_t> U16MessageField;
typedef IntegralMessageField<int16_t> S16MessageField;
typedef IntegralMessageField<uint32_t> U32MessageField;
typedef IntegralMessageField<int32_t> S32MessageField;

class RangeMessageField;
class FloatMessageField;
class StringMessageField;
class BitFieldMessageField;
class Md5SumMessageField;
class MessageFieldGroup;
class MessageFieldArray;

// ----------------------------------------------------------------------------

// ============================================================================
//
// MessageField abstract base interface
//
// Stores a data type for use in concrete Message classes. MessageFields know
// how to serialize/deserialize from a BitStream and can calculate their own
// size.
// 
// ============================================================================

class MessageField
{
public:
	virtual const std::string& getFieldName() const
		{ return mFieldName; }
	virtual void setFieldName(const std::string& name)
		{ mFieldName = name; }

	virtual bool valid() const
		{ return true; }
	virtual uint16_t size() const = 0;
	virtual void clear() = 0;

	virtual uint16_t read(BitStream& stream) = 0;
	virtual uint16_t write(BitStream& stream) const = 0;

	virtual MessageField* clone() const = 0;

private:
	std::string			mFieldName;
};


// ============================================================================
//
// IntegralMessageField template implementation
//
// Generic MessageField class for storing and serializing integral data types
// for use in Messages.
// 
// ============================================================================

template<typename T, uint16_t SIZE>
class IntegralMessageField : public MessageField
{
public:
	IntegralMessageField() :
		mValid(false), mValue(0) { }
	IntegralMessageField(T value) :
		mValid(true), mValue(value) { }
	virtual ~IntegralMessageField() { }

	inline bool valid() const
		{ return mValid; }
	inline uint16_t size() const
		{ return SIZE; }
	inline void clear()
		{ mValue = 0; mValid = false; }

	inline uint16_t read(BitStream& stream)
		{ set(stream.readBits(SIZE)); return SIZE; }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeBits(mValue, SIZE); return SIZE; }

	inline T get() const
		{ return mValue; }
	inline void set(T value)
		{ mValue = value; mValid = true; }

	inline IntegralMessageField<T, SIZE>* clone() const
		{ return new IntegralMessageField<T, SIZE>(*this); }

private:
	bool	mValid;
	T		mValue;
};

// ============================================================================
//
// RangeMessageField implementation
//
// Stores and efficiently serializes integral values of a specified range for
// use in Messages.
// 
// ============================================================================

class RangeMessageField : public MessageField
{
public:
	RangeMessageField();
	RangeMessageField(int32_t value, int32_t lowerbound = MININT, int32_t upperbound = MAXINT);
	virtual ~RangeMessageField() { }

	inline bool valid() const
		{ return mValid; }
	uint16_t size() const;
	inline void clear()
		{ mValue = 0; mValid = false; }

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline int32_t get() const
		{ return mValue; }
	inline void set(int32_t value)
		{ mValue = value; mValid = true; }

	inline RangeMessageField* clone() const
		{ return new RangeMessageField(*this); }

private:
	mutable bool		mCachedSizeValid;
	mutable uint16_t	mCachedSize;

	bool				mValid;
	int32_t				mValue;
	int32_t				mLowerBound;
	int32_t				mUpperBound;
};


// ============================================================================
//
// FloatMessageField implementation
//
// Stores and efficiently serializes floating-point values for use in Messages.
// 
// ============================================================================

class FloatMessageField : public MessageField
{
public:
	FloatMessageField() :
		mValid(false), mValue(0.0f) { }
	FloatMessageField(float value) :
		mValid(true), mValue(value) { }
	virtual ~FloatMessageField() { }

	inline bool valid() const
		{ return mValid; }
	inline uint16_t size() const
		{ return SIZE; }
	inline void clear()
		{ mValue = 0; mValid = false; }

	inline uint16_t read(BitStream& stream)
		{ set(stream.readFloat()); return SIZE; }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeFloat(mValue); return SIZE; }

	inline float get() const
		{ return mValue; }
	inline void set(float value)
		{ mValue = value; mValid = true; }

	inline FloatMessageField* clone() const
		{ return new FloatMessageField(*this); }

private:
	static const uint16_t SIZE = 32;
	bool				mValid;
	float				mValue;
};


// ============================================================================
//
// StringMessageField implementation
//
// Stores and serializes std::string values for use in Messages.
// 
// ============================================================================

class StringMessageField : public MessageField
{
public:
	StringMessageField() :
		mValid(false) { }
	StringMessageField(const std::string& value) :
		mValid(true), mValue(value) { }
	virtual ~StringMessageField() { }

	inline bool valid() const
		{ return mValid; }
	inline uint16_t size() const
		{ return 8 * (mValue.length() + 1); }
	inline void clear()
		{ mValue.clear(); mValid = false;}

	inline uint16_t read(BitStream& stream)
		{ set(stream.readString()); return size(); }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeString(mValue); return size(); }

	inline const std::string& get() const
		{ return mValue; }
	inline void set(const std::string& value)
		{ mValue = value; mValid = true; }

	inline StringMessageField* clone() const
		{ return new StringMessageField(*this); }

private:
	bool				mValid;
	std::string			mValue;
};


// ============================================================================
//
// BitFieldMessageField implementation
//
// Stores and serializes BitField values for use in Messages.
// 
// ============================================================================

class BitFieldMessageField : public MessageField
{
public:
	BitFieldMessageField(uint32_t num_fields = 32);
	BitFieldMessageField(const BitField& value);
	virtual ~BitFieldMessageField() { }

	inline bool valid() const
		{ return mValid; }
	inline uint16_t size() const
		{ return mBitField.size(); }
	inline void clear()
		{ mBitField.clear(); mValid = false; }

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline const BitField& get() const
		{ return mBitField; }
	inline void set(const BitField& value)
		{ mBitField = value; mValid = true; }

	inline BitFieldMessageField* clone() const
		{ return new BitFieldMessageField(*this); }

private:
	bool				mValid;
	BitField			mBitField;
};


// ============================================================================
//
// Md5SumMessageField implementation
//
// Stores and efficiently serializes hexidecimal strings that store MD5SUM
// hashes.
// 
// ============================================================================

class Md5SumMessageField : public MessageField
{
public:
	Md5SumMessageField();
	Md5SumMessageField(const std::string& value);
	virtual ~Md5SumMessageField() { }

	inline bool valid() const
		{ return mValid; }
	inline uint16_t size() const
		{ return NUMBITS; }
	void clear();

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline const std::string& get() const
		{ return mCachedString; }
	inline void set(const std::string& value)
		{ setFromString(value); }

	inline Md5SumMessageField* clone() const
		{ return new Md5SumMessageField(*this); }

private:
	void setFromString(const std::string& value);
	void cacheString();

	static const size_t NUMBITS = 128;
	static const size_t NUMBYTES = NUMBITS / 8;
	bool			mValid;
	uint8_t			mValue[NUMBYTES];
	std::string		mCachedString;	
};


// ============================================================================
//
// CompositeMessageField abstract base class
//
// Provides an interface for storing a collection of MessageField objects.
// 
// ============================================================================

class CompositeMessageField : public MessageField
{
public:
	
};

// ============================================================================
//
// MessageFieldGroup interface
//
// Stores and serializes a composite list of required and optional
// MessageFields for use in Messages.
// 
// ============================================================================

class MessageFieldGroup : public CompositeMessageField
{
public:
	MessageFieldGroup();
	MessageFieldGroup(const MessageFieldGroup& other);
	virtual ~MessageFieldGroup();

	inline bool valid() const
		{ return mValid; }
	uint16_t size() const;
	void clear();

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline MessageFieldGroup* clone() const
		{ return new MessageFieldGroup(*this); }

	virtual void addField(MessageField* field, bool optional);

private:
	mutable bool			mCachedSizeValid;
	mutable uint16_t		mCachedSize;

	bool					mValid;

	typedef std::map<std::string, MessageField*> NameTable;
	NameTable				mNameTable;

	typedef std::vector<MessageField*> FieldArray;

	BitFieldMessageField	mOptionalIndicator;
	FieldArray				mOptionalFields;
	FieldArray				mRequiredFields;
};


// ============================================================================
//
// MessageFieldArray interface
//
// Stores and serializes an homogeneous dynamic array of MessageFields type for
// use in Messages.
// 
// ============================================================================

class MessageFieldArray : public CompositeMessageField
{
public:
	MessageFieldArray(uint32_t mincnt = 0, uint32_t maxcnt = 65535);
	MessageFieldArray(const MessageFieldArray& other);
	virtual ~MessageFieldArray();
	MessageFieldArray& operator=(const MessageFieldArray& other);

	inline bool valid() const
		{ return mValid; }
	uint16_t size() const;
	void clear();

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline MessageFieldArray* clone() const
		{ return new MessageFieldArray(*this); }
	
private:
	mutable bool		mCachedSizeValid;
	mutable uint16_t	mCachedSize;

	bool				mValid;

	uint32_t			mMinCount;
	uint32_t			mMaxCount;
	RangeMessageField	mCountField;

	typedef std::vector<MessageField*> FieldArray;
	FieldArray			mFields;
};

#endif	// __NET_MESSAGEFIELD_H__


