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
// Miscellaneous definitions and functions common to the networking subsystem.
//
//-----------------------------------------------------------------------------

#ifndef __NET_TYPE_H__
#define __NET_TYPE_H__

#include "doomtype.h"
#include <cstring>
#include <queue>

#include "net_bitstream.h"


// ============================================================================
//
// BitField
//
// Efficiently stores a boolean value for up to N different fields.
//
// ============================================================================

class BitField
{
public:
	BitField(uint16_t size) : mSize(size), mByteCount((size + 7) >> 3)
	{
		mData = new uint8_t[mByteCount];
		clear();
	}

	BitField(const BitField& other) : mSize(other.mSize), mByteCount(other.mByteCount)
	{
		mData = new uint8_t[mByteCount];
		memcpy(mData, other.mData, sizeof(mData));
	}

	~BitField()
	{
		delete [] mData;
	}

	BitField& operator=(const BitField& other)
	{
		if (&other != this)
		{
			if (mByteCount != other.mByteCount)
			{
				delete [] mData;
				mData = new uint8_t[other.mByteCount];
			}
			mSize = other.mSize;
			mByteCount = other.mByteCount;
			memcpy(mData, other.mData, sizeof(mData));
		}
		return *this;
	}

	uint16_t size() const
	{
		return mSize;
	}

	void clear()
	{
		memset(mData, 0, sizeof(mData));
	}

	bool empty() const
	{
		for (uint32_t i = 0; i < mByteCount; ++i)
			if (mData[i] != 0)
				return false;
		return true;
	}

	void set(uint32_t flag)
	{
		if (flag < mSize)
			mData[flag >> 3] |= (1 << (flag & 0x7));
	}

	void unset(uint32_t flag)
	{
		if (flag < mSize)
			mData[flag >> 3] &= ~(1 << (flag & 0x7));
	}

	bool get(uint32_t flag) const
	{
		if (flag < mSize)
			return (mData[flag >> 3] & (1 << (flag & 0x7))) != 0;
		return false;
	}

	void read(BitStream& stream)
	{
		clear();
		for (uint32_t i = 0; i < mSize; i++)
			if (stream.readBit() == 1)
				set(i);
	}

	void write(BitStream& stream) const
	{
		for (uint32_t i = 0; i < mSize; i++)
			stream.writeBit(get(i));
	}

	bool operator== (const BitField& other) const
	{
		if (mByteCount != other.mByteCount)
			return false;

		for (uint32_t i = 0; i < mByteCount; ++i)
			if (mData[i] != other.mData[i])
				return false;
		return true;
	}

	bool operator!= (const BitField& other) const
	{
		return (operator==(other) == false);
	}

	BitField operator| (const BitField& other) const
	{
		BitField result(*this);
		result |= other;
		return result;
	}

	BitField operator& (const BitField& other) const
	{
		BitField result(*this);
		result &= other;
		return result;
	}

	BitField operator^ (const BitField& other) const
	{
		BitField result(*this);
		result ^= other;
		return result;
	}

	BitField operator~ () const
	{
		BitField result(mSize);
		for (uint32_t i = 0; i < mByteCount; ++i)
			result.mData[i] = ~mData[i];
		return result;
	}

	BitField& operator|= (const BitField& other)
	{
		expand(other);
		for (uint32_t i = 0; i < mByteCount; ++i)
			mData[i] |= other.mData[i];
		return *this;
	}

	BitField& operator&= (const BitField& other)
	{
		expand(other);
		for (uint32_t i = 0; i < mByteCount; ++i)
			mData[i] &= other.mData[i];
		return *this;
	}

	BitField& operator^= (const BitField& other)
	{
		expand(other);
		for (uint32_t i = 0; i < mByteCount; ++i)
			mData[i] ^= other.mData[i];
		return *this;
	}

	BitField operator>> (uint32_t cnt) const
	{
		BitField result(*this);
		result >>= cnt;
		return result;
	}

	BitField operator<< (uint32_t cnt) const
	{
		BitField result(*this);
		result <<= cnt;
		return result;	
	}

	BitField& operator>>= (uint32_t cnt)
	{
		if (cnt >= mSize)
		{
			clear();
			return *this;
		}

		const uint32_t byteshift = cnt >> 3;
		const uint32_t bitshift = cnt & 0x07;
		for (int i = 0; i < (int)mByteCount - (int)byteshift - 1; ++i)
			mData[i] = (mData[i + byteshift] >> bitshift) | (mData[i + byteshift + 1] << (8 - bitshift));
		mData[mByteCount - byteshift - 1] = mData[mByteCount - 1] >> bitshift;
		memset(mData + mByteCount - byteshift, 0, byteshift);

		return *this;
	}

	BitField& operator<<= (uint32_t cnt)
	{
		if (cnt >= mSize)
		{
			clear();
			return *this;
		}

		const uint32_t byteshift = cnt >> 3;
		const uint32_t bitshift = cnt & 0x07;
		for (int i = (int)mByteCount - 1; i > (int)byteshift; --i)
			mData[i] = (mData[i - byteshift] << bitshift) | (mData[i - byteshift - 1] >> (8 - bitshift));
		mData[byteshift] = mData[0] << bitshift;
		memset(mData, 0, byteshift);

		return *this;
	}

private:
	void expand(const BitField& other)
	{
		if (mByteCount < other.mByteCount)
		{
			uint8_t* newdata = new uint8_t[other.mByteCount];
			memset(newdata + mByteCount, 0, other.mByteCount - mByteCount);
			memcpy(newdata, mData, mByteCount);
			delete [] mData;
			mData = newdata;
			mSize = other.mSize;
			mByteCount = other.mByteCount;
		}
	}

	uint16_t mSize;
	uint16_t mByteCount;
	uint8_t* mData;
};


// ============================================================================
//
// SequenceNumber
//
// Sequence number data type, which handles integer wrap-around and
// allows for an arbitrary N-bit sequence number (up to 32 bits).
//
// ============================================================================

template <uint8_t N = 32>
class SequenceNumber
{
public:
	SequenceNumber(uint32_t seq = 0) :
		mSequence(seq & SEQUENCE_MASK)
	{ }

	void setInteger(uint32_t seq)
	{
		mSequence = seq & SEQUENCE_MASK;
	}

	uint32_t getInteger() const
	{
		return mSequence;
	}

	void read(BitStream& stream)
	{
		mSequence = stream.readBits(SEQUENCE_BITS);
	}

	void write(BitStream& stream) const
	{
		stream.writeBits(mSequence, SEQUENCE_BITS);
	}

	bool operator== (const SequenceNumber<N>& other) const
	{
		return mSequence == other.mSequence;
	}

	bool operator!= (const SequenceNumber<N>& other) const
	{
		return mSequence != other.mSequence;
	}

	bool operator> (const SequenceNumber<N>& other) const
	{
		static const uint32_t MIDPOINT = (1 << (SEQUENCE_BITS - 1));

		return	(mSequence > other.mSequence && (mSequence - other.mSequence) <= MIDPOINT) ||
				(mSequence < other.mSequence && (other.mSequence - mSequence) > MIDPOINT);
	}

	bool operator>= (const SequenceNumber<N>& other) const
	{
		return operator==(other) || operator>(other);
	}

	bool operator< (const SequenceNumber<N>& other) const
	{
		return !(operator>=(other));
	}

	bool operator<= (const SequenceNumber<N>& other) const
	{
		return !(operator>(other));
	}

	SequenceNumber<N> operator+ (const SequenceNumber<N>& other) const
	{
		SequenceNumber<N> result(*this);
		return result += other;
	}

	SequenceNumber<N> operator- (const SequenceNumber<N>& other) const
	{
		SequenceNumber<N> result(*this);
		return result -= other;
	}

	SequenceNumber<N>& operator= (const SequenceNumber<N>& other)
	{
		mSequence = other.mSequence;
		return *this;
	}

	SequenceNumber<N>& operator+= (const SequenceNumber<N>& other)
	{
		mSequence = (mSequence + other.mSequence) & SEQUENCE_MASK;
		return *this;
	}

	SequenceNumber<N>& operator-= (const SequenceNumber<N>& other)
	{
		mSequence = (mSequence - other.mSequence) & SEQUENCE_MASK;
		return *this;
	}

	SequenceNumber<N>& operator++ ()
	{
		mSequence = (mSequence + 1) & SEQUENCE_MASK;
		return *this;
	}

	SequenceNumber<N>& operator-- ()
	{
		mSequence = (mSequence - 1) & SEQUENCE_MASK;
		return *this;
	}

	SequenceNumber<N> operator++ (int)
	{
		SequenceNumber<N> result(*this);
		operator++();
		return result;
	}

	SequenceNumber<N> operator-- (int)
	{
		SequenceNumber<N> result(*this);
		operator--();
		return result;
	}

private:
	static const uint8_t	SEQUENCE_BITS = N;	// must be <= 32
	static const uint32_t	SEQUENCE_MASK = (1ULL << N) - 1;
	static const uint64_t	SEQUENCE_UNIT = (1ULL << N);

	uint32_t				mSequence;
};


// ============================================================================
//
// IdGenerator
//
// Hands out unique IDs of the type SequenceNumber. The range for the IDs
// generated includes 0 through 2^N - 1. Although the IDs generated are in
// order at first, they are not guaranteed of being so since the order in
// which IDs are freed eventually impacts the IDs generated.
//
// Note that there is no error checking made when generating a new ID if
// there are no free IDs and there is no checking to prevent freeing
// an ID twice. Be careful to not use values of N larger than about 16.
//
// ============================================================================

template <uint8_t N>
class IdGenerator
{
public:
	IdGenerator()
	{
		clear();
	}

	virtual ~IdGenerator()
	{ }

	virtual bool empty() const
	{
		return mFreeIds.empty();
	}

	virtual void clear()
	{
		while (!mFreeIds.empty())
			mFreeIds.pop();

		for (uint32_t i = 0; i < mNumIds; i++)
			mFreeIds.push(i);
	}

	virtual SequenceNumber<N> generate()
	{
		// TODO: Add check for mFreeIds.empty()
		SequenceNumber<N> id = mFreeIds.front();
		mFreeIds.pop();
		return id;
	}

	virtual void free(const SequenceNumber<N>& id)
	{
		// TODO: Add check to ensure id isn't already free
		mFreeIds.push(id);
	}

private:
	static const uint32_t			mNumIds = (1 << N);
	std::queue<SequenceNumber<N> >	mFreeIds;
};


// ============================================================================
//
// SequentialIdGenerator
//
// Hands out unique IDs of the type SequenceNumber. The range for the IDs
// generated includes 0 through 2^N - 1. The IDs generated are guaranteed to
// be in sequential order. This imposes the constraint that IDs must also be
// freed in sequential order.
//
// ============================================================================

template <uint8_t N>
class SequentialIdGenerator : public IdGenerator<N>
{
public:
	SequentialIdGenerator()
	{
		clear();
	}

	virtual ~SequentialIdGenerator()
	{ }

	virtual bool empty() const
	{
		return mNextId == mFirstId;
	}

	virtual void clear()
	{
		mNextId = 0;
	}

	virtual SequenceNumber<N> generate()
	{
		// TODO: throw exception if there are no free IDs
		return mNextId++;
	}

	virtual void free(const SequenceNumber<N>& id)
	{
		// TODO: throw exception if empty or id != mFirstId
		++mFirstId;
	}

private:
	static const uint32_t		mNumIds = (1 << N);
	SequenceNumber<N>			mNextId;
	SequenceNumber<N>			mFirstId;
};

#endif	// __NET_TYPE_H__

