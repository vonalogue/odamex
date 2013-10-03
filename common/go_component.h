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

#ifndef __GO_COMPONENT_H__
#define __GO_COMPONENT_H__

#include "net_type.h"
#include "net_bitstream.h"
#include "vectors.h"
#include "hashtable.h"
#include "m_ostring.h"

// ----------------------------------------------------------------------------
// Forward declarations
// ----------------------------------------------------------------------------

template<typename T, uint16_t SIZE = 8*sizeof(T)> class IntegralComponent;
typedef IntegralComponent<bool, 1> BoolComponent;
typedef IntegralComponent<uint8_t> U8Component;
typedef IntegralComponent<int8_t> S8Component;
typedef IntegralComponent<uint16_t> U16Component;
typedef IntegralComponent<int16_t> S16Component;
typedef IntegralComponent<uint32_t> U32Component;
typedef IntegralComponent<int32_t> S32Component;

// ----------------------------------------------------------------------------

// ============================================================================
//
// GameObjectComponent abstract base interface
//
// Stores a data type for use in GameObjects. GameObjectComponents know
// how to serialize/deserialize from a BitStream and can calculate their own
// size (in bits).
// 
// ============================================================================

class GameObjectComponent
{
public:
	// name of the type (eg., "uint32")
	virtual const OString& getTypeName() const = 0;
	inline void setTypeName(const OString& value) { }

	// name of the attribute (eg., "health")
	inline const OString& getAttributeName() const
		{ return mAttributeName; }
	inline void setAttributeName(const OString& value)
		{ mAttributeName = value; }

	// indicate if this component will always be serialized
	inline bool isRequired() const
		{ return mRequired; }
	inline void setRequired(bool value)
		{ mRequired = value; }

	// indicate if this component will be automatically serialized when changed
	inline bool isReplicatable() const
		{ return mReplicatable; }
	inline void setReplicatable(bool value)
		{ mReplicatable = value; }

	// indicate whether this component contains other components
	virtual inline bool isComposite() const
		{ return false; }

	// return the size of the component (in bits)
	virtual uint16_t size() const = 0;
	// set the component to its default value
	virtual void clear() = 0;

	// read the component from a BitStream
	virtual uint16_t read(BitStream& stream) = 0;
	// write the component to a BitStream
	virtual uint16_t write(BitStream& stream) const = 0;

	// instantiate a new copy of this component
	virtual GameObjectComponent* clone() const = 0;

private:
	OString			mAttributeName;
	bool			mRequired;
	bool			mReplicatable;
};


// ============================================================================
//
// IntegralComponent template implementation
//
// Generic GameObjectComponent class for storing and serializing integral data
// types for use in GameObjects.
// 
// ============================================================================

template<typename T, uint16_t SIZE>
class IntegralComponent : public GameObjectComponent
{
public:
	IntegralComponent() :
		mValue(0) { }
	IntegralComponent(T value) :
		mValue(value) { }
	virtual ~IntegralComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	inline uint16_t size() const
		{ return SIZE; }
	inline void clear()
		{ mValue = 0; }

	inline uint16_t read(BitStream& stream)
		{ set(stream.readBits(SIZE)); return SIZE; }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeBits(mValue, SIZE); return SIZE; }

	inline T get() const
		{ return mValue; }
	inline void set(T value)
		{ mValue = value; }

	inline IntegralComponent<T, SIZE>* clone() const
		{ return new IntegralComponent<T, SIZE>(*this); }

private:
	static const OString	mTypeName;
	T						mValue;
};

// ============================================================================
//
// RangeComponent interface
//
// Stores and efficiently serializes integral values of a specified range for
// use in GameObjects.
// 
// ============================================================================

class RangeComponent : public GameObjectComponent
{
public:
	RangeComponent();
	RangeComponent(int32_t value, int32_t lowerbound = MININT, int32_t upperbound = MAXINT);
	virtual ~RangeComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	uint16_t size() const;
	inline void clear()
		{ mValue = mLowerBound; }

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline int32_t get() const
		{ return mValue; }
	inline void set(int32_t value)
		{ mValue = value; }

	inline RangeComponent* clone() const
		{ return new RangeComponent(*this); }

private:
	static const OString	mTypeName;

	mutable bool			mCachedSizeValid;
	mutable uint16_t		mCachedSize;

	int32_t					mValue;
	int32_t					mLowerBound;
	int32_t					mUpperBound;
};


// ============================================================================
//
// FloatComponent interface
//
// Stores and efficiently serializes floating-point values for use in
// GameObjects.
// 
// ============================================================================

class FloatComponent : public GameObjectComponent
{
public:
	FloatComponent() :
		mValue(0.0f) { }
	FloatComponent(float value) :
		mValue(value) { }
	virtual ~FloatComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	inline uint16_t size() const
		{ return SIZE; }
	inline void clear()
		{ mValue = 0.0f; }

	inline uint16_t read(BitStream& stream)
		{ set(stream.readFloat()); return SIZE; }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeFloat(mValue); return SIZE; }

	inline float get() const
		{ return mValue; }
	inline void set(float value)
		{ mValue = value; }

	inline FloatComponent* clone() const
		{ return new FloatComponent(*this); }

private:
	static const OString	mTypeName;
	static const uint16_t	SIZE = 32;
	float					mValue;
};


// ============================================================================
//
// StringComponent implementation
//
// Stores and serializes OString values for use in GameObjects.
// 
// ============================================================================

class StringComponent : public GameObjectComponent
{
public:
	StringComponent() { }
	StringComponent(const OString& value) :
		mValue(value) { }
	virtual ~StringComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	inline uint16_t size() const
		{ return 8 * (mValue.length() + 1); }
	inline void clear()
		{ mValue.clear(); }

	inline uint16_t read(BitStream& stream)
		{ set(stream.readString()); return size(); }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeString(mValue); return size(); }

	inline const OString& get() const
		{ return mValue; }
	inline void set(const OString& value)
		{ mValue = value; }

	inline StringComponent* clone() const
		{ return new StringComponent(*this); }

private:
	static const OString	mTypeName;
	OString					mValue;
};

// ============================================================================
//
// V2FixedComponent interface
//
// Stores and serializes v2fixed_t vectors for use in GameObjects.
//
// ============================================================================

class V2FixedComponent : public GameObjectComponent
{
public:
	V2FixedComponent() { }
	V2FixedComponent(const v2fixed_t& value) :
		mValue(value) { }
	virtual ~V2FixedComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	inline uint16_t size() const
		{ return SIZE; }
	inline void clear()
		{ M_ZeroVec2Fixed(&mValue); }

	inline uint16_t read(BitStream& stream)
		{ M_SetVec2Fixed(&mValue, stream.readS16() << 16, stream.readS16() << 16);
		  return size(); }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeS16(mValue.x >> 16); stream.writeS16(mValue.y >> 16);
		  return size(); }

	inline const v2fixed_t& get() const
		{ return mValue; }
	inline void set(const v2fixed_t& value)
		{ mValue = value; }

	inline V2FixedComponent* clone() const
		{ return new V2FixedComponent(*this); }

private:
	static const OString	mTypeName;
	static const uint16_t 	SIZE = 2 * 16;
	v2fixed_t				mValue;
};


// ============================================================================
//
// V3FixedComponent interface
//
// Stores and serializes v3fixed_t vectors for use in GameObjects.
//
// ============================================================================

class V3FixedComponent : public GameObjectComponent
{
public:
	V3FixedComponent() { }
	V3FixedComponent(const v3fixed_t& value) :
		mValue(value) { }
	virtual ~V3FixedComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	inline uint16_t size() const
		{ return SIZE; }
	inline void clear()
		{ M_ZeroVec3Fixed(&mValue); }

	inline uint16_t read(BitStream& stream)
		{ M_SetVec3Fixed(&mValue, stream.readS16() << 16, stream.readS16() << 16, stream.readS16() << 16);
		  return size(); }
	inline uint16_t write(BitStream& stream) const
		{ stream.writeS16(mValue.x >> 16); stream.writeS16(mValue.y >> 16); stream.writeS16(mValue.z >> 16);
		  return size(); }

	inline const v3fixed_t& get() const
		{ return mValue; }
	inline void set(const v3fixed_t& value)
		{ mValue = value; }

	inline V3FixedComponent* clone() const
		{ return new V3FixedComponent(*this); }

private:
	static const OString	mTypeName;
	static const uint16_t 	SIZE = 3 * 16;
	v3fixed_t				mValue;
};


// ============================================================================
//
// BitFieldComponent implementation
//
// Stores and serializes BitField values for use in GameObjects.
// 
// ============================================================================

class BitFieldComponent : public GameObjectComponent
{
public:
	BitFieldComponent(uint32_t num_fields = 32);
	BitFieldComponent(const BitField& value);
	virtual ~BitFieldComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	inline uint16_t size() const
		{ return mBitField.size(); }
	inline void clear()
		{ mBitField.clear(); }

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline const BitField& get() const
		{ return mBitField; }
	inline void set(const BitField& value)
		{ mBitField = value; }

	inline BitFieldComponent* clone() const
		{ return new BitFieldComponent(*this); }

private:
	static const OString	mTypeName;
	BitField				mBitField;
};


// ============================================================================
//
// Md5SumComponent implementation
//
// Stores and efficiently serializes hexidecimal strings that store MD5SUM
// hashes.
// 
// ============================================================================

class Md5SumComponent : public GameObjectComponent
{
public:
	Md5SumComponent();
	Md5SumComponent(const OString& value);
	virtual ~Md5SumComponent() { }

	virtual const OString& getTypeName() const
		{ return mTypeName; }

	inline uint16_t size() const
		{ return NUMBITS; }
	void clear();

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline const OString& get() const
		{ return mCachedString; }
	inline void set(const OString& value)
		{ setFromString(value); }

	inline Md5SumComponent* clone() const
		{ return new Md5SumComponent(*this); }

private:
	void setFromString(const OString& value);
	void cacheString();

	static const OString	mTypeName;
	static const size_t 	NUMBITS = 128;
	static const size_t 	NUMBYTES = NUMBITS / 8;
	uint8_t					mValue[NUMBYTES];
	OString					mCachedString;	
};



// ============================================================================
//
// GameObjectComposite abstract base class interface
//
// ============================================================================
class GameObjectComposite : public GameObjectComponent
{
public:
	inline bool isComposite() const
		{ return true; }
};


// ============================================================================
//
// GameObjectComponentArray interface
//
// Stores and serializes an homogeneous dynamic array of GameObjectComponents
// type for use in GameObjects.
// 
// ============================================================================

class GameObjectComponentArray : public GameObjectComposite
{
public:
	GameObjectComponentArray(uint32_t mincnt = 0, uint32_t maxcnt = 65535);
	GameObjectComponentArray(const GameObjectComponentArray& other);
	virtual ~GameObjectComponentArray();
	GameObjectComponentArray& operator=(const GameObjectComponentArray& other);

	virtual const OString& getTypeName() const
		{ return mTypeName; }
	virtual void setTypeName(const OString& type_name)
		{ mTypeName = type_name; }

	inline bool isComposite() const
		{ return true; }

	uint16_t size() const;
	void clear();

	uint16_t read(BitStream& stream);
	uint16_t write(BitStream& stream) const;

	inline GameObjectComponentArray* clone() const
		{ return new GameObjectComponentArray(*this); }
	
private:
	OString				mTypeName;

	mutable bool		mCachedSizeValid;
	mutable uint16_t	mCachedSize;


	uint32_t			mMinCount;
	uint32_t			mMaxCount;
	RangeComponent		mCountField;

	typedef std::vector<GameObjectComponent*> FieldArray;
	FieldArray			mFields;
};

// ============================================================================
//
// GameObjectComponentGroup interface
//
// Stores and serializes a composite list of required and optional
// GameObjectComponents for use in GameObjects.
// 
// ============================================================================

class GameObjectComponentGroup : public GameObjectComposite
{
public:
	GameObjectComponentGroup();
	GameObjectComponentGroup(const GameObjectComponentGroup& other);
	virtual ~GameObjectComponentGroup();

	virtual const OString& getTypeName() const
		{ return mTypeName; }
	virtual void setTypeName(const OString& type_name)
		{ mTypeName = type_name; }

	inline bool isComposite() const
		{ return true; }

	uint16_t size() const;
	void clear();

	virtual uint16_t read(BitStream& stream);
	virtual uint16_t write(BitStream& stream) const;

	inline GameObjectComponentGroup* clone() const
		{ return new GameObjectComponentGroup(*this); }

private:
	OString						mTypeName;

	mutable bool				mCachedSizeValid;
	mutable uint16_t			mCachedSize;
};

#endif	// __GO_COMPONENT_H__

