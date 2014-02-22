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
// Packet abstraction 
//
//-----------------------------------------------------------------------------

#include "doomdef.h"

#include "net_type.h"
#include "net_common.h"
#include "net_bitstream.h"

#include "net_packet.h"

// ============================================================================
//
// Packet class implementation
//
// ============================================================================

// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------


Packet::Packet() :
	mRecvHistory(32),
	mCorrupted(false)
{ }


//
// Packet::clear
//
void Packet::clear()
{
	mSequence = 0;
	mRecvSequence = 0;
	mRecvHistory.clear();
	mCorrupted = false;
	mPayload.clear();
}


//
// Packet::getSize
//
// Returns the size of the packet in bits, including the header.
//	
uint16_t Packet::getSize() const
{
	return HEADER_SIZE + (mPayload.bytesWritten() << 3) + TRAILER_SIZE;
}


//
// Packet::isCorrupted
//
// Returns true if the packet failed validation.
//
bool Packet::isCorrupted() const
{
	return mCorrupted;
}


//
// Packet::writePacketData
//
// Writes the packet contents to the given buffer. The caller indicates
// the maximum size buffer can hold (in bits) with the size parameter.
//
// Returns the number of bits written to the buffer.
//
uint16_t Packet::writePacketData(uint8_t* buf, uint16_t size) const
{
	if (size > getSize())
		return 0;

	BitStream header;
	header.writeBits(mType, 1);
	mSequence.write(header);
	mRecvSequence.write(header);
	mRecvHistory.write(header);

	header.readBlob(buf, HEADER_SIZE);

	memcpy(buf + (HEADER_SIZE >> 3), mPayload.getRawData(), mPayload.bytesWritten());

	// calculate CRC32 for the packet header & payload
	BitStream trailer;
	const uint32_t datasize = getSize() - TRAILER_SIZE;
	uint32_t crcvalue = Net_CRC32(buf, datasize >> 3);

	// write the calculated CRC32 value to the buffer
	trailer.writeU32(crcvalue);
	trailer.readBlob(buf + datasize, TRAILER_SIZE);	

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
uint16_t Packet::readPacketData(const uint8_t* buf, uint16_t size)
{
	clear();

	BitStream header;
	header.writeBlob(buf, HEADER_SIZE);
	mType = static_cast<Packet::PacketType>(header.readBits(1));
	mSequence.read(header);
	mRecvSequence.read(header);
	mRecvHistory.read(header);

	const uint32_t payloadsize = size - HEADER_SIZE - TRAILER_SIZE;
	mPayload.writeBlob(buf + HEADER_SIZE, payloadsize);

	// calculate CRC32 for the packet header & payload
	const uint32_t datasize = size - TRAILER_SIZE;
	uint32_t crcvalue = Net_CRC32(buf, datasize >> 3);

	// verify the calculated CRC32 value matches the value in the packet
	BitStream trailer;
	trailer.writeBlob(buf + datasize, TRAILER_SIZE);
	mCorrupted = (trailer.readU32() != crcvalue);

	return size;
}


void Packet::setSequence(const Packet::PacketSequenceNumber value)
{
	mSequence = value;
}

Packet::PacketSequenceNumber Packet::getSequence() const
{
	return mSequence;
}

void Packet::setRecvSequence(const Packet::PacketSequenceNumber value)
{
	mRecvSequence = value;
}

Packet::PacketSequenceNumber Packet::getRecvSequence() const
{
	return mRecvSequence;
}

void Packet::setRecvHistory(const BitField& value)
{
	mRecvHistory = value;
}

const BitField& Packet::getRecvHistory() const
{
	return mRecvHistory;
}


BitStream& Packet::getPayload()
{
	return mPayload;
}


// ============================================================================
//
// PacketFactory class implementation
//
// ============================================================================

bool PacketFactory::mInitialized = false;
PacketFactory::PacketStore* PacketFactory::mPackets = NULL;

void PacketFactory::startup()
{
	if (!mInitialized)
	{
		mPackets = new PacketStore(128);
		mInitialized = true;
	}
}


void PacketFactory::shutdown()
{
	if (mInitialized)
	{
		delete mPackets;
		mPackets = NULL;
		mInitialized = false;
	}
}

//
// PacketFactory::createPacket
//
// Returns an instance of an unused Packet from the memory pool.
//
Packet* PacketFactory::createPacket()
{
	SArrayId id = mPackets->insert();
	Packet* packet = &mPackets->get(id);
	packet->clear();
	return packet;
}


VERSION_CONTROL (net_packet_cpp, "$Id$")

