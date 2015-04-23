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

Packet::PacketDataStore Packet::mPacketData(128);

// ----------------------------------------------------------------------------
// Construction / Destruction
// ----------------------------------------------------------------------------

Packet::Packet() :
	mId(0), mData(NULL)
{ }

Packet::Packet(const Packet& other) :
	mId(other.mId), mData(other.mData)
{
	if (mData)
		mData->mRefCount++;
}

Packet::~Packet()
{
	if (mData)
	{
		mData->mRefCount--;
		if (mData->mRefCount == 0)
			mPacketData.erase(mId);
	}
}

Packet Packet::operator=(const Packet& other)
{
	if (mId != other.mId)
	{
		if (mData)
		{
			mData->mRefCount--;
			if (mData->mRefCount == 0)
				mPacketData.erase(mId);
		}
		
		mId = other.mId;
		mData = other.mData;
		if (mData)
			mData->mRefCount++;
	}

	return *this;
}
	

// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------

//
// Packet::clear
//
void Packet::clear()
{
	mData->clear();
}


//
// Packet::getSize
//
// Returns the size of the packet in bits, including the header.
//	
uint16_t Packet::getSize() const
{
	return HEADER_SIZE + (mData->mPayload.bytesWritten() << 3) + TRAILER_SIZE;
}


//
// Packet::isCorrupted
//
// Returns true if the packet failed validation.
//
bool Packet::isCorrupted() const
{
	return mData->mCorrupted;
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
	const uint16_t packet_size = getSize();
	if (size < packet_size)
		return 0;

	BitStream header;
	header.writeBits(mData->mType, 1);
	mData->mSequence.write(header);
	mData->mRecvSequence.write(header);
	mData->mRecvHistory.write(header);

	header.readBlob(buf, HEADER_SIZE);

	memcpy(buf + BitsToBytes(HEADER_SIZE), mData->mPayload.getRawData(), mData->mPayload.bytesWritten());

	// calculate CRC32 for the packet header & payload
	BitStream trailer;
	const uint32_t data_size = BitsToBytes(packet_size - TRAILER_SIZE);
	uint32_t crcvalue = Net_CRC32(buf, data_size);

	// write the calculated CRC32 value to the buffer
	trailer.writeU32(crcvalue);
	trailer.readBlob(buf + data_size, TRAILER_SIZE);	

	return packet_size;
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
	mData->clear();

	BitStream header;
	header.writeBlob(buf, HEADER_SIZE);
	mData->mType = static_cast<Packet::PacketType>(header.readBits(1));
	mData->mSequence.read(header);
	mData->mRecvSequence.read(header);
	mData->mRecvHistory.read(header);

	const uint32_t payload_size = size - HEADER_SIZE - TRAILER_SIZE;
	mData->mPayload.writeBlob(buf + BitsToBytes(HEADER_SIZE), payload_size);

	// calculate CRC32 for the packet header & payload
	const uint32_t data_size = BitsToBytes(size - TRAILER_SIZE);
	uint32_t crcvalue = Net_CRC32(buf, data_size);

	// verify the calculated CRC32 value matches the value in the packet
	BitStream trailer;
	trailer.writeBlob(buf + data_size, TRAILER_SIZE);
	mData->mCorrupted = (trailer.readU32() != crcvalue);

	return size;
}


void Packet::setType(const Packet::PacketType value)
{
	mData->mType = value;
}

Packet::PacketType Packet::getType() const
{
	return mData->mType;
}

void Packet::setSequence(const Packet::PacketSequenceNumber value)
{
	mData->mSequence = value;
}

Packet::PacketSequenceNumber Packet::getSequence() const
{
	return mData->mSequence;
}

void Packet::setRecvSequence(const Packet::PacketSequenceNumber value)
{
	mData->mRecvSequence = value;
}

Packet::PacketSequenceNumber Packet::getRecvSequence() const
{
	return mData->mRecvSequence;
}

BitStream& Packet::getPayload()
{
	return mData->mPayload;
}


//
// Packet::dump
//
// Writes the contents of the packet to stdout in hexidecimal.
//
void Packet::dump() const
{
	printf("Packet Contents\n");
	printf("---------------\n");

	char ack_history[33];
	ack_history[32] = 0;
	for (int i = 0; i < 32; i++)
		ack_history[i] = mData->mRecvHistory.get(i) ? '1' : '0';
	 
	printf("Seq: %u, Ack: %u, %s\n", mData->mSequence.getInteger(),
				mData->mRecvSequence.getInteger(), ack_history);

	const uint8_t* buf = mData->mPayload.getRawData();
	uint16_t buflen = mData->mPayload.bytesWritten();

	int i, j;
	for (i = 0; i < buflen; i += 16)
	{
		printf("%06x: ", i);
		for (j = 0; j < 16; j++) 
			if (i + j < buflen)
				printf("%02x ", buf[i + j]);
			else
				printf("   ");
		printf(" ");
		for (j = 0; j < 16; j++) 
			if (i + j < buflen)
				printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
		printf("\n");
	}
}


// ============================================================================
//
// PacketFactory class implementation
//
// ============================================================================


//
// PacketFactory::createPacket
//
// Returns an instance of an unused Packet from the memory pool.
//
Packet PacketFactory::createPacket()
{
	Packet packet;
	packet.mId = Packet::mPacketData.insert();
	packet.mData = &(Packet::mPacketData.get(packet.mId));
	packet.mData->clear();	
	packet.mData->mRefCount = 1;
	
	return packet;
}


VERSION_CONTROL (net_packet_cpp, "$Id$")

