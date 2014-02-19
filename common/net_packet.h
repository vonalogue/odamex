// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2014 by The Odamex Team.
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
// Packet class 
//
//-----------------------------------------------------------------------------

#ifndef __NET_PACKET_H__
#define __NET_PACKET_H__

#include "doomdef.h"

#include "net_type.h"
#include "net_bitstream.h"

// ============================================================================
//
// Packet class interface
//
// The Packet class wraps the raw BitStream that is used to read and write
// data for network transmission. It includes several variables in a header
// followed by the payload.
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


	// ---------------------------------------------------------------------------
	// constructors
	// ---------------------------------------------------------------------------
	Packet() :
		mRecvHistory(32)
	{ }


	//
	// Packet::getSize
	//
	// Returns the size of the packet in bits, including the header.
	//	
	uint16_t getSize() const
	{
		return HEADER_SIZE + mPayload.bitsWritten();
	}


	//
	// Packet::writePacketData
	//
	// Writes the packet contents to the given buffer. The caller indicates
	// the maximum size buffer can hold (in bits) with the size parameter.
	//
	// Returns the number of bits written to the buffer.
	//
	uint16_t writePacketData(uint8_t* buf, uint16_t size)
	{
		if (size > getSize())
			return 0;

		BitStream header;
		header.writeBits(mType, 1);
		mSequence.write(header);
		mRecvSequence.write(header);
		mRecvHistory.write(header);

		header.readBlob(buf, HEADER_SIZE);
		mPayload.readBlob(buf + HEADER_SIZE, mPayload.bitsWritten());
		return getSize();
	}


	//
	// Packet::readPacketData
	//
	// Reads the contents from the given buffer and constructs a packet
	// by parsing the header and separating the payload.
	//
	// Returns the number of bits read from the buffer.
	//
	uint16_t readPacketData(const uint8_t* buf, uint16_t size)
	{
		BitStream header;
		header.writeBlob(buf, HEADER_SIZE);
		mType = static_cast<Packet::PacketType>(header.readBits(1));
		mSequence.read(header);
		mRecvSequence.read(header);
		mRecvHistory.read(header);

		mPayload.clear();
		mPayload.writeBlob(buf + HEADER_SIZE, size - HEADER_SIZE);
		return size;
	}

	void setSequence(const Packet::PacketSequenceNumber value)
	{
		mSequence = value;
	}

	Packet::PacketSequenceNumber getSequence() const
	{
		return mSequence;
	}

	void setRecvSequence(const Packet::PacketSequenceNumber value)
	{
		mRecvSequence = value;
	}

	Packet::PacketSequenceNumber getRecvSequence() const
	{
		return mRecvSequence;
	}

	void setRecvHistory(const BitField& value)
	{
		mRecvHistory = value;
	}

	const BitField& getRecvHistory() const
	{
		return mRecvHistory;
	}


private:
	static const uint16_t HEADER_SIZE = 9*8;	// must be byte aligned

	PacketType				mType;
	PacketSequenceNumber	mSequence;
	PacketSequenceNumber	mRecvSequence;
	BitField				mRecvHistory;

	BitStream				mPayload;
};

#endif	// __NET_PACKET_H__


