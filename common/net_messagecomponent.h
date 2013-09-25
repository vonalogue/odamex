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
// MessageComponents are building-block elements that comprise Messages.
// MessageComponents use the Composite Pattern to treat composite MessageComponents
// such as MessageComponentGroup the same as primative MessageComponents such as
// StringMessageComponent.
//
// MessageComponents are data types that know how to serialize and deserialize
// themselves to and from a BitStream.
//
// MessageComponents have a clone operation to create a new instance of themselves.
// This is part of the Prototype Patter and is a mechanism that allows
// a prototype instance of each Message type to be built
//
//-----------------------------------------------------------------------------

#ifndef __NET_MESSAGECOMPONENT_H__
#define __NET_MESSAGECOMPONENT_H__

#include "net_type.h"
#include "net_bitstream.h"
#include <string>
#include "hashtable.h"
#include <list>

// ----------------------------------------------------------------------------
// Forward declarations
// ----------------------------------------------------------------------------

template<typename T, uint16_t SIZE = 8*sizeof(T)> class IntegralMessageComponent;
typedef IntegralMessageComponent<bool, 1> BoolMessageComponent;
typedef IntegralMessageComponent<uint8_t> U8MessageComponent;
typedef IntegralMessageComponent<int8_t> S8MessageComponent;
typedef IntegralMessageComponent<uint16_t> U16MessageComponent;
typedef IntegralMessageComponent<int16_t> S16MessageComponent;
typedef IntegralMessageComponent<uint32_t> U32MessageComponent;
typedef IntegralMessageComponent<int32_t> S32MessageComponent;

class RangeMessageComponent;
class FloatMessageComponent;
class StringMessageComponent;
class BitFieldMessageComponent;
class Md5SumMessageComponent;
class MessageComponentGroup;
class MessageComponentArray;

// ----------------------------------------------------------------------------

// ============================================================================
//
// MessageComponent abstract base interface
//
// Stores a data type for use in concrete Message classes. MessageComponents know
// how to serialize/deserialize from a BitStream and can calculate their own
// size.
// 
// ============================================================================

class MessageComponent
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

	virtual MessageComponent* clone() const = 0;

private:
	std::string			mFieldName;
};


// ============================================================================
//
// IntegralMessageComponent template implementation
//
// Generic MessageComponent class for storing and serializing integral data types
// for use in Messages.
// 
// ============================================================================

template<typename T, uint16_t SIZE>
class IntegralMessageComponent : public MessageComponent
{
public:
	IntegralMessageComponent() :
		mValid(false), mValue(0) { }
	IntegralMessageComponent(T value) :
		mValid(true), mValue(value) { }
	virtual ~IntegralMessageComponent() { }

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

	inline IntegralMessageComponent<T, SIZE>* clone() const
		{ return new IntegralMessageComponent<T, SIZE>(*this); }

private:
	bool	mValid;
	T		mValue;
};

// ============================================================================
//
// RangeMessageComponent implementation
//
// Stores and efficiently serializes integral values of a specified range for
// use in Messages.
// 
// ============================================================================

class RangeMessageComponent : public MessageComponent
{
public:
	RangeMessageComponent();
	RangeMessageComponent(int32_t value, int32_t lowerbound = MININT, int32_t upperbound = MAXINT);
	virtual ~RangeMessageComponent() { }

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

	inline RangeMessageComponent* clone() const
		{ return new RangeMessageComponent(*this); }

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
// FloatMessageComponent implementation
//
// Stores and efficiently serializes floating-point values for use in Messages.
// 
// ============================================================================

class FloatMessageComponent : public MessageComponent
{
public:
	FloatMessageComponent() :
		mValid(false), mValue(0.0f) { }
	FloatMessageComponent(float value) :
		mValid(true), mValue(value) { }
	virtual ~FloatMessageComponent() { }

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

	inline FloatMessageComponent* clone() const
		{ return new FloatMessageComponent(*this); }

private:
	static const uint16_t SIZE = 32;
	bool				mValid;
	float				mValue;
};


// ============================================================================
//
// StringMessageComponent implementation
//
// Stores and serializes std::string values for use in Messages.
// 
// ============================================================================

class StringMessageComponent : public MessageComponent
{
public:
	StringMessageComponent() :
		mValid(false) { }
	StringMessageComponent(const std::string& value) :
		mValid(true), mValue(value) { }
	virtual ~StringMessageComponent() { }

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

	inline StringMessageComponent* clone() const
		{ return new StringMessageComponent(*this); }

private:
	bool				mValid;
	std::string			mValue;
};


// ============================================================================
//
// BitFieldMessageComponent implementation
//
// Stores and serializes BitField values for use in Messages.
// 
// ============================================================================

class BitFieldMessageComponent : public MessageComponent
{
public:
	BitFieldMessageComponent(uint32_t num_fields = 32);
	BitFieldMessageComponent(const BitField& value);
	virtual ~BitFieldMessageComponent() { }

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

	inline BitFieldMessageComponent* clone() const
		{ return new BitFieldMessageComponent(*this); }

private:
	bool				mValid;
	BitField			mBitField;
};


// ============================================================================
//
// Md5SumMessageComponent implementation
//
// Stores and efficiently serializes hexidecimal strings that store MD5SUM
// hashes.
// 
// ============================================================================

class Md5SumMessageComponent : public MessageComponent
{
public:
	Md5SumMessageComponent();
	Md5SumMessageComponent(const std::string& value);
	virtual ~Md5SumMessageComponent() { }

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

	inline Md5SumMessageComponent* clone() const
		{ return new Md5SumMessageComponent(*this); }

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
// MessageComponentArray interface
//
// Stores and serializes an homogeneous dynamic array of MessageComponents type for
// use in Messages.
// 
// ============================================================================

class MessageComponentArray : public MessageComponent
{
public:
	MessageComponentArray(uint32_t mincnt = 0, uint32_t maxcnt = 65535);
	MessageComponentArray(const MessageComponentArray& other);
	virtual ~MessageComponentArray();
	MessageComponentArray& operator=(const MessageComponentArray& other);

	inline bool valid() const
		{ return mValid; }
	uint16_t size() const;
	void clear();

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline MessageComponentArray* clone() const
		{ return new MessageComponentArray(*this); }
	
private:
	mutable bool		mCachedSizeValid;
	mutable uint16_t	mCachedSize;

	bool				mValid;

	uint32_t			mMinCount;
	uint32_t			mMaxCount;
	RangeMessageComponent	mCountField;

	typedef std::vector<MessageComponent*> FieldArray;
	FieldArray			mFields;
};

// ============================================================================
//
// CompositeMessageComponent abstract base class
//
// Provides an interface for storing a collection of MessageComponent objects.
// 
// ============================================================================

class CompositeMessageComponent : public MessageComponent
{
public:
	
};

// ============================================================================
//
// MessageComponentGroup interface
//
// Stores and serializes a composite list of required and optional
// MessageComponents for use in Messages.
// 
// ============================================================================

class MessageComponentGroup : public CompositeMessageComponent
{
public:
	MessageComponentGroup();
	MessageComponentGroup(const MessageComponentGroup& other);
	virtual ~MessageComponentGroup();

	inline bool valid() const
		{ return mValid; }
	uint16_t size() const;
	void clear();

	virtual uint16_t read(BitStream& stream);
	virtual uint16_t write(BitStream& stream) const;

	inline MessageComponentGroup* clone() const
		{ return new MessageComponentGroup(*this); }

	bool hasField(const std::string& name) const;

	virtual void addField(MessageComponent* field, bool optional);

private:
	mutable bool			mCachedSizeValid;
	mutable uint16_t		mCachedSize;

	bool					mValid;

	typedef HashTable<std::string, MessageComponent*> NameTable;
	NameTable				mNameTable;

	typedef std::vector<MessageComponent*> FieldArray;

	BitFieldMessageComponent	mOptionalIndicator;
	FieldArray				mOptionalFields;
	FieldArray				mRequiredFields;
};


// ============================================================================
//
// Message interface
//
// Wrapper for composite MessageComponents
//
// ============================================================================

typedef enum
{
	NM_NoOp					= 0,		// does nothing
    NM_Replication          = 1,
    NM_Ticcmd               = 2,
    NM_LoadMap              = 10,
    NM_ClientStatus         = 11,
    NM_Chat                 = 20,
    NM_Obituary             = 21,
} MessageType;

class Message : public MessageComponent
{
public:
	virtual ~Message() { }

	const MessageType getMessageType() const
		{	return mMessageType;	}

	inline bool valid() const { return true; }
	virtual uint16_t size() const { return 0; }
	virtual void clear() { }

	virtual uint16_t read(BitStream& stream) { return 0; } 
	virtual uint16_t write(BitStream& stream) const { return 0; }

	Message* clone() const
		{ return new Message(*this); }

private:
	MessageType				mMessageType;	
};

#endif	// __NET_MESSAGECOMPONENT_H__

