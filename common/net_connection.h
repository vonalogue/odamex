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
// Connection establishment and management	
//
//-----------------------------------------------------------------------------

#ifndef __NET_CONNECTION_H__
#define __NET_CONNECTION_H__

#include "doomdef.h"

#include "net_type.h"
#include "net_socketaddress.h"
#include "net_interface.h"
#include "net_messagecomponent.h"

#include <vector>

class MessageManager;

typedef SequenceNumber<16> PacketSequenceNumber;


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
	std::string				mRemoteAddressString;

	ConnectionState_t		mState;
	uint32_t				mCreationTS;
	uint32_t				mTimeOutTS;

	uint32_t				mConnectionAttempt;
	uint32_t				mConnectionAttemptTimeOutTS;

	uint32_t				mToken;
	uint32_t				mTokenTimeOutTS;

	// ------------------------------------------------------------------------
	// Sequence numbers and receipt acknowledgement
	//-------------------------------------------------------------------------
	static const uint8_t ACKNOWLEDGEMENT_COUNT = 32;

	// sequence number to be sent in the next packet
	PacketSequenceNumber	mSequence;
	// most recent sequence number the remote host has acknowledged receiving
	PacketSequenceNumber	mLastAckSequence;
	// set to true during connection negotiation
	bool					mLastAckSequenceValid;

	// most recent sequence number the remote host has sent
	PacketSequenceNumber	mRecvSequence;
	// set to true during connection negotiation
	bool					mRecvSequenceValid;
	// bitfield representing all of the recently received sequence numbers
	BitField				mRecvHistory;


	// ------------------------------------------------------------------------
	// Statistical tracking
	// ------------------------------------------------------------------------
	uint32_t				mSentPacketCount;
	uint32_t				mLostPacketCount;
	uint32_t				mRecvPacketCount;

	// ------------------------------------------------------------------------
	// Packet composition and parsing
	// ------------------------------------------------------------------------
	typedef enum
	{
		GAME_PACKET				= 0,
		NEGOTIATION_PACKET		= 1
	} PacketType;

	typedef BoolMessageComponent PacketTypeMessageComponent;

	std::vector<MessageManager*>		mMessageManagers;

	PacketType checkPacketType(BitStream& stream);

	void composePacketHeader(const PacketType& type, BitStream& stream);
	void parsePacketHeader(BitStream& stream);

	void composeGamePacket(BitStream& stream);
	void parseGamePacket(BitStream& stream);
	void parseNegotiationPacket(BitStream& stream);	

	void notifyReceived(const PacketSequenceNumber& seq);
	void notifyLost(const PacketSequenceNumber& seq);

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


