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
// Packet abstraction 
//
//-----------------------------------------------------------------------------

#ifndef __NET_PACKET_H__
#define __NET_PACKET_H__

#include "doomdef.h"

#include "net_type.h"
#include "net_common.h"
#include "net_bitstream.h"

#include "sarray.h"

class PacketFactory;

// ============================================================================
//
// Packet class interface
//
// The Packet class wraps the raw BitStream that is used to read and write
// data for network transmission. It includes several variables in a header
// followed by the payload and trailer. The trailer includes a 32-bit CRC to
// ensure validity of the payload and header.
//
// ============================================================================

class Packet
{
public:
	// ---------------------------------------------------------------------------
	// typedefs
	// ---------------------------------------------------------------------------
	typedef SequenceNumber<16> PacketSequenceNumber;

	typedef enum
	{
		GAME_PACKET				= 0,
		NEGOTIATION_PACKET		= 1
	} PacketType;

	Packet(const Packet&);
	~Packet();
	Packet operator=(const Packet&);

	// ---------------------------------------------------------------------------
	// status functions
	// ---------------------------------------------------------------------------
	void clear();

	uint16_t getSize() const;

	bool isCorrupted() const;

	uint16_t writePacketData(uint8_t* buf, uint16_t size) const;
	uint16_t readPacketData(const uint8_t* buf, uint16_t size);

	void dump() const;

	// ---------------------------------------------------------------------------
	// accessors / mutators functions
	// ---------------------------------------------------------------------------
	void setType(const Packet::PacketType value);
	Packet::PacketType getType() const;

	void setSequence(const Packet::PacketSequenceNumber value);
	Packet::PacketSequenceNumber getSequence() const;

	void setRecvSequence(const Packet::PacketSequenceNumber value);
	Packet::PacketSequenceNumber getRecvSequence() const;

	void setRecvHistory(const BitField<32>& value)
	{	mData->mRecvHistory = value;	}

	const BitField<32>& getRecvHistory() const
	{	return mData->mRecvHistory;		}

	BitStream& getPayload();

private:
	friend class PacketFactory;

	static const uint16_t HEADER_SIZE = 9*8;	// must be byte aligned
	static const uint16_t TRAILER_SIZE = 32;

	struct PacketData
	{
		PacketData() : mRefCount(0), mCorrupted(false) { }
		
		void clear() {	mCorrupted = false; mPayload.clear();	}

		int						mRefCount;

		PacketType				mType;
		PacketSequenceNumber	mSequence;
		PacketSequenceNumber	mRecvSequence;
		BitField<32>			mRecvHistory;
		bool					mCorrupted;

		BitStream				mPayload;
	};

	// Private constructor so that only PacketFactory can build a Packet
	Packet();
	
	SArrayId					mId;
	PacketData*					mData;		// cached lookup to mPacketData.get(mId)

	// ------------------------------------------------------------------------
	// PacketData storage
	// ------------------------------------------------------------------------
	typedef SArray<Packet::PacketData> PacketDataStore;
	static PacketDataStore mPacketData;
};



// ============================================================================
//
// PacketFactory
//
// Singleton class to allocate memory for Packets. The PacketFactory allocates
// a large memory pool and stores packets internally, handing out pointers
// to a Packet instance in the PacketFactory's memory pool. 
//
// ============================================================================

class PacketFactory
{
public:
	static Packet createPacket();

private:
	friend class Packet;

	// Private constructor for Singleton
	// Don't allow copy constructor or assignment operator
	PacketFactory() { }
	PacketFactory(const PacketFactory&);
	PacketFactory& operator=(const PacketFactory&);

	~PacketFactory() { }
};


#endif	// __NET_PACKET_H__


