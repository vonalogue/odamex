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
// Connection establishment and management	
//
//-----------------------------------------------------------------------------

#ifndef __NET_CONNECTION_H__
#define __NET_CONNECTION_H__

#include "doomdef.h"

#include "net_type.h"
#include "net_socketaddress.h"
#include "net_interface.h"
#include "net_packet.h"

#include <vector>

class MessageManager;


// ============================================================================
//
// Connection base class Interface
//
// Connection classes represent a coupling between two hosts on a network.
// The class is responsible for maintaining the flow of information between
// the hosts. This can mean measuring bandwidth usage, flow control,
// congestion avoidance, detecting when the link is dead (timed out) and more.
//
// ============================================================================

class Connection
{
public:
	Connection(const ConnectionId& id, NetInterface* interface, const SocketAddress& adr);
	virtual ~Connection();

	ConnectionId getConnectionId() const;
	NetInterface* getInterface() const;
	const SocketAddress& getRemoteAddress() const;
	const char* getRemoteAddressCString() const;

	virtual bool isConnected() const;
	virtual bool isNegotiating() const;
	virtual bool isTerminated() const;
	virtual bool isTimedOut() const;

	virtual void openConnection();
	virtual void closeConnection();

	virtual void service();
	virtual void sendPacket(BitStream& stream);
	virtual void processPacket(BitStream& stream);

	virtual void registerMessageManager(MessageManager* manager);

private:
	enum ConnectionState_t
	{
		CONN_LISTENING					= 0x01,
		CONN_REQUESTING					= 0x02,
		CONN_OFFERING					= 0x04,
		CONN_NEGOTIATING				= 0x07,
		CONN_ACCEPTING					= 0x08,
		CONN_CONNECTED					= 0x80,
		CONN_TERMINATED					= 0xFF
	};

	ConnectionId			mConnectionId;
	NetInterface*			mInterface;
	SocketAddress			mRemoteAddress;

	ConnectionState_t		mState;
	dtime_t					mCreationTS;
	dtime_t					mTimeOutTS;

	uint32_t				mConnectionAttempt;
	dtime_t					mConnectionAttemptTimeOutTS;

	uint32_t				mToken;
	dtime_t					mTokenTimeOutTS;

	// ------------------------------------------------------------------------
	// Sequence numbers and receipt acknowledgement
	//-------------------------------------------------------------------------
	static const uint8_t ACKNOWLEDGEMENT_COUNT = 32;

	// sequence number to be sent in the next packet
	Packet::PacketSequenceNumber	mSequence;
	// most recent sequence number the remote host has acknowledged receiving
	Packet::PacketSequenceNumber	mLastAckSequence;
	// set to true during connection negotiation
	bool							mLastAckSequenceValid;

	// most recent sequence number the remote host has sent
	Packet::PacketSequenceNumber	mRecvSequence;
	// set to true during connection negotiation
	bool							mRecvSequenceValid;
	// bitfield representing all of the recently received sequence numbers
	BitField						mRecvHistory;


	// ------------------------------------------------------------------------
	// Statistical tracking
	// ------------------------------------------------------------------------
	void remoteHostReceivedPacket(const Packet::PacketSequenceNumber seq);
	void remoteHostLostPacket(const Packet::PacketSequenceNumber seq);

	uint32_t				mSentPacketCount;
	uint32_t				mLostPacketCount;
	uint32_t				mRecvPacketCount;

	double					mAvgRoundTripTime;
	double					mAvgJitterTime;
	double					mAvgLostPacketPercentage;

	std::queue<dtime_t>		mPacketSendTimes;

	// ------------------------------------------------------------------------
	// Packet composition and parsing
	// ------------------------------------------------------------------------
	typedef std::vector<MessageManager*> MessageManagerList;
	MessageManagerList					mMessageManagers;

	Packet::PacketType checkPacketType(BitStream& stream);

	void composePacketHeader(const Packet::PacketType& type, BitStream& stream);
	void parsePacketHeader(BitStream& stream);

	void composeGamePacket(BitStream& stream);
	void parseGamePacket(BitStream& stream);
	void parseNegotiationPacket(BitStream& stream);	

	// ------------------------------------------------------------------------
	// Connection establishment
	// ------------------------------------------------------------------------
	void resetState();

	bool clientRequest();
	bool serverProcessRequest(BitStream& stream);
	bool serverOffer();
	bool clientProcessOffer(BitStream& stream);
	bool clientAccept();
	bool serverProcessAcceptance(BitStream& stream);
	bool serverConfirmAcceptance();
	bool clientProcessConfirmation(BitStream& stream);

};


#endif	// __NET_CONNECTION_H__


