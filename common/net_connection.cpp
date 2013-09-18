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
// Connection establishment and management	
//
//-----------------------------------------------------------------------------

#include "net_type.h"
#include "net_common.h"
#include "net_log.h"
#include "net_interface.h"
#include "net_connection.h"

#include "i_system.h"
#include "md5.h"

#include <stdarg.h>

EXTERN_CVAR(net_maxrate)

// ============================================================================
//
// Connection class Implementation
//
// ============================================================================

static const uint32_t CONNECTION_TIMEOUT = 4*ONE_SECOND;
static const uint32_t NEGOTIATION_TIMEOUT = 2*ONE_SECOND;
static const uint32_t NEGOTIATION_ATTEMPTS = 4;
static const uint32_t TOKEN_TIMEOUT = 4*ONE_SECOND;
static const uint32_t TERMINATION_TIMEOUT = 4*ONE_SECOND;

// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------

Connection::Connection(const ConnectionId& connection_id, NetInterface* interface, const SocketAddress& adr) :
	mConnectionId(connection_id),
	mInterface(interface), mRemoteAddress(adr), mRemoteAddressString(std::string(adr)),
	mCreationTS(Net_CurrentTime()),
	mTimeOutTS(Net_CurrentTime() + CONNECTION_TIMEOUT),
	mConnectionAttempt(0), mConnectionAttemptTimeOutTS(0),
	mToken(0), mTokenTimeOutTS(0),
	mSequence(rand()),
	mLastAckSequenceValid(false), mRecvSequenceValid(false),
	mRecvHistory(ACKNOWLEDGEMENT_COUNT),
	mSentPacketCount(0), mLostPacketCount(0), mRecvPacketCount(0)
{
	resetState();
}


Connection::~Connection()
{ }


ConnectionId Connection::getConnectionId() const
{
	return mConnectionId;
}


NetInterface* Connection::getInterface() const
{
	return mInterface;
}


const SocketAddress& Connection::getRemoteAddress() const
{
	return mRemoteAddress;
}


const char* Connection::getRemoteAddressCString() const
{
	return mRemoteAddressString.c_str();
}


bool Connection::isConnected() const
{
	return mState == CONN_CONNECTED;
}


bool Connection::isNegotiating() const
{
	return mState & CONN_NEGOTIATING;
}


bool Connection::isTerminated() const
{
	return mState & CONN_TERMINATED;
}


bool Connection::isTimedOut() const
{
	return 	Net_CurrentTime() >= mTimeOutTS;
}

//
// Connection::openConnection
//
// Sends the remote host a request to establish a connection.
//
void Connection::openConnection()
{
	Net_Printf("Requesting connection to server %s...\n", getRemoteAddressCString());
	clientRequest();
}


//
// Connection::closeConnection
//
// Sends the remote host a signal that the connection is ending and
// updates this connection's internal state to prevent further sending or
// receiving over this connection.
//
void Connection::closeConnection()
{
	Net_LogPrintf("Connection::closeConnection: terminating connection to %s.", getRemoteAddressCString());
	BitStream stream;
	stream.writeU32(TERMINATION_SEQUENCE);
	sendPacket(stream);

	mState = CONN_TERMINATED;
	mTimeOutTS = Net_CurrentTime() + TERMINATION_TIMEOUT;
}


//
// Connection::sendPacket
//
// Updates statistics and hands the packet off to NetInterface to send
//
void Connection::sendPacket(BitStream& stream)
{
	if (mState == CONN_TERMINATED)
		return;

	mInterface->sendPacket(mConnectionId, stream);
	++mSentPacketCount;
}


//
// Connection::processPacket
//
// Inspects the type of an incoming packet and then hands the packet off to
// the appropriate parsing function.
//
void Connection::processPacket(BitStream& stream)
{
	if (mState == CONN_TERMINATED)
		return;

	// inspect the packet type
	PacketType packet_type = checkPacketType(stream);

	if (packet_type == NEGOTIATION_PACKET)
		parseNegotiationPacket(stream);
	else if (packet_type == GAME_PACKET)
		parseGamePacket(stream);
	else
		Net_Error("Connection::processPacket: Unknown packet type from %s.", getRemoteAddressCString());

	mTimeOutTS = Net_CurrentTime() + CONNECTION_TIMEOUT;
	++mRecvPacketCount;
}


//
// Connection::service
//
void Connection::service()
{
	// check if we should try to negotiate again
	if (isNegotiating() && mConnectionAttempt > 0 &&
		mConnectionAttempt <= NEGOTIATION_ATTEMPTS &&
		Net_CurrentTime() >= mConnectionAttemptTimeOutTS)
	{
		Net_Printf("Re-requesting connection to server %s...", getRemoteAddressCString());
		clientRequest();
	}

	// compose a new game packet and send it
	if (isConnected())
	{
		// TODO: determine packet_size using flow-control methods
		const uint16_t packet_size = 200*8;

		BitStream stream(packet_size);
		composeGamePacket(stream);
		sendPacket(stream);
	}
}


//
// Connection::registerMessageManager
//
// Registers a MessageManager object that will handle composing or parsing the
// payload portion of outgoing or incoming game packets. The composing/parsing
// functions will be called order in which they are registered.
//
/*
void Connection::registerMessageManager(MessageManager* manager)
{
	if (manager != NULL)
		mMessageManagers.push_back(manager);
}
*/

// ----------------------------------------------------------------------------
// Private connection negotiation functions
// ----------------------------------------------------------------------------

//
// Connection::resetState
//
// Sets the state of the negotiation back to its default state. This is used
// when there is an error in the negotiation process and the negotiation needs
// to be retried.
//
void Connection::resetState()
{
	mState = CONN_LISTENING;
	mLastAckSequenceValid = false;
	mRecvSequenceValid = false;
	mRecvHistory.clear();
}


//
// Connection::clientRequest
//
// Contact the server and initiate a game connection.  Send the server a hash
// of the password.
//
bool Connection::clientRequest()
{
	++mConnectionAttempt;
	mConnectionAttemptTimeOutTS = Net_CurrentTime() + NEGOTIATION_TIMEOUT;

	Net_LogPrintf("Connection::clientRequest: sending connection request packet to %s.", getRemoteAddressCString());
	mState = CONN_REQUESTING;

	BitStream stream;
	stream.writeU32(CONNECTION_SEQUENCE);
	stream.writeU32(GAME_CHALLENGE);
	stream.writeU8(GAMEVER);

	sendPacket(stream);
	return true;
}


//
// Connection::serverProcessRequest
//
// Handle a client's request to initiate a connection.
//
bool Connection::serverProcessRequest(BitStream& stream)
{
	Net_LogPrintf("Connection::serverProcessRequest: processing connection request packet from %s.", getRemoteAddressCString());

	if (stream.readU32() != CONNECTION_SEQUENCE)
	{
		//terminate(TERM_BADHANDSHAKE);
		return false;
	}
	if (stream.readU32() != GAME_CHALLENGE)
	{
		//terminate(TERM_BADHANDSHAKE);
		return false;
	}
	if (stream.readU8() != GAMEVER)
	{
		//terminate(TERM_VERSIONMISMATCH);
		return false;
	}

	return true;
}


//
// Connection::serverOffer
//
//
bool Connection::serverOffer()
{
	Net_LogPrintf("Connection::serverOffer: sending connection offer packet to %s.", getRemoteAddressCString());

	BitStream stream;
	composePacketHeader(NEGOTIATION_PACKET, stream);

	// denis - each launcher reply contains a random token so that
	// the server will only allow connections with a valid token
	// in order to protect itself from ip spoofing
	mToken = rand() * time(0);
	mTokenTimeOutTS = Net_CurrentTime() + TOKEN_TIMEOUT;
	stream.writeU32(mToken);

	// TODO: Server gives a list of supported compression types as a bitfield
	// enum {
	//		COMPR_NONE = 0,
	//		COMPR_LZO = 1,
	//		COMPR_HUFFMAN = 2
	// } CompressionType;
	//
	// BitField<3> compression_availible;
	// compression_availible.write(stream);

	// TODO: Server gives a list of supported credential types as a bitfield
	// enum {
	//		CRED_NONE = 0,
	//		CRED_PASSWORD = 1,
	//		CRED_LOCALACCT = 2,
	//		CRED_MASTERACCT = 3
	// } CredentialType;
	//
	// BitField<3> credential_type;
	// credential_availible.write(stream);

	sendPacket(stream);
	return true;
}	


//
// Connection::clientProcessOffer
//
//
bool Connection::clientProcessOffer(BitStream& stream)
{
	Net_LogPrintf("Connection::clientProcessOffer: processing connection offer packet from %s.", getRemoteAddressCString());

	parsePacketHeader(stream);
	mRecvSequenceValid = true;
	
	mToken = stream.readU32();	// save the token

	// TODO: process the server's list of supported compression types. select
	// one and record it so it can be sent in the clientAccept packet.
	//
	// BitField<3> compression_availible;
	// compression_availible.read(stream);

	// TODO: process the server's list of supported credential types. select
	// one and record it so it can be sent in the clientAccept packet along
	// with the relevant credential info.
	//
	// BitField<3> credential_type;
	// credential_availible.write(stream);

	// write the connection password
	std::string password_hash;
	if (!getInterface()->getPassword().empty())
		password_hash = MD5SUM(getInterface()->getPassword());
	stream.writeString(password_hash);

	return true;
}


//
// Connection::clientAccept
//
//
bool Connection::clientAccept()
{
	Net_LogPrintf("Connection::clientAccept: sending connection acceptance packet to %s.", getRemoteAddressCString());

	BitStream stream;
	composePacketHeader(NEGOTIATION_PACKET, stream);

	// confirm server token
	stream.writeU32(mToken);			

	sendPacket(stream);
	return true;
}


//
// Connection::serverProcessAcceptance
//
//
bool Connection::serverProcessAcceptance(BitStream& stream)
{
	Net_LogPrintf("Connection::serverProcessAcceptance: processing connection acceptance packet from %s.", getRemoteAddressCString());

	parsePacketHeader(stream);
	mRecvSequenceValid = true;
	mLastAckSequenceValid = true;
	
	// server verifies the token it received from the client matches
	// what it had sent earlier
	uint32_t token = stream.readU32();
	if (token != mToken || Net_CurrentTime() >= mTokenTimeOutTS)
	{
		//terminate(Connection::TERM_BADHANDSHAKE);
		return false;
	}

	// Read and verify the supplied password.
	std::string password_hash = stream.readString();
	if (!getInterface()->getPassword().empty())
	{
		if (password_hash.compare(MD5SUM(getInterface()->getPassword())) != 0)
		{
			//terminate(TERM_BADPASSWORD);
			return false;
		}
	}	

	return true;
}


//
// Connection::serverConfirmAcceptance
//
//
bool Connection::serverConfirmAcceptance()
{
	Net_LogPrintf("Connection::serverConfirmAcceptance: sending connection confirmation packet to %s.", getRemoteAddressCString());

	BitStream stream;
	composePacketHeader(NEGOTIATION_PACKET, stream);


	// TODO: maybe tell the client its ClientId, etc

	sendPacket(stream);
	return true;
}


//
// Connection::clientProcessConfirmation
//
//
bool Connection::clientProcessConfirmation(BitStream& stream)
{
	Net_LogPrintf("Connection::clientProcessConfirmation: processing connection confirmation packet from %s.", getRemoteAddressCString());

	parsePacketHeader(stream);
	mLastAckSequenceValid = true;

	return true;
}


//
// Connection::parseNegotiationPacket
//
// Handles incoming connection setup/tear-down data for this connection.
//
void Connection::parseNegotiationPacket(BitStream& stream)
{
	if (stream.peekU32() == TERMINATION_SEQUENCE)
	{
		mState = CONN_TERMINATED;
		closeConnection();
	}

	if (mState == CONN_TERMINATED)
		return;

	// Once negotiation is successful, we should no longer receive
	// negotiation packets. If we do, it's an indication that the remote
	// host lost our last packet and is retrying the from the beginning.
	if (mState == CONN_CONNECTED)
		resetState();

	if (mState == CONN_ACCEPTING)
	{
		if (clientProcessConfirmation(stream))
		{
			mState = CONN_CONNECTED;
			Net_LogPrintf("Connection::processNegotiationPacket: negotiation with %s successful.", getRemoteAddressCString());
		}
		else
		{
			resetState();
		}
	}

	// server offering connection to client?
	if (mState == CONN_OFFERING)
	{
		if (serverProcessAcceptance(stream))
		{
			// server confirms connected status to client
			serverConfirmAcceptance();
			mState = CONN_CONNECTED;
			Net_LogPrintf("Connection::processNegotiationPacket: negotiation with %s successful.", getRemoteAddressCString());
		}
		else
		{
			resetState();
		}
	}

	// client requesting connection to server?
	if (mState == CONN_REQUESTING)
	{
		if (clientProcessOffer(stream))
		{
			// client accepts connection to server
			clientAccept();
			mState = CONN_ACCEPTING;
		}
		else
		{
			resetState();
		}
	}

	// server listening for requests?
	if (mState == CONN_LISTENING)
	{
		if (serverProcessRequest(stream))
		{
			// server offers connection to client
			serverOffer();
			mState = CONN_OFFERING;
		}
		else
		{
			resetState();
		}
	}
}


//
// Connection::checkPacketType
//
// Determines the type of a packet by examining the header.
//
Connection::PacketType Connection::checkPacketType(BitStream& stream)
{
	if (stream.peekU32() == CONNECTION_SEQUENCE ||
		stream.peekU32() == TERMINATION_SEQUENCE)
		return NEGOTIATION_PACKET;

	if (stream.peekU8() & 0x80)
		return NEGOTIATION_PACKET;

	return GAME_PACKET;
}


//
// Connection::parsePacketHeader
//
// Reads an incoming packet's header, recording the packet sequence number so
// it can be acknowledged in the next outgoing packet. Also notifies the
// registered MessageManagers of any outgoing packet sequence numbers the
// remote host has acknowledged receiving or lost. Notification is guaranteed
// to be handled in order of sequence number.
//
void Connection::parsePacketHeader(BitStream& stream)
{
	PacketTypeMessageComponent packettype;
	packettype.read(stream);		// read and ignore since we already peeked at it
	
	// read the remote host's sequence number and store it so we can
	// acknowledge receipt in the next outgoing packet.
	PacketSequenceNumber in_seq;
	in_seq.read(stream);

	Net_LogPrintf("Connection::parsePacketHeader: reading packet header from %s, sequence %u.", getRemoteAddressCString(), in_seq.getInteger());

	if (mRecvSequenceValid)
	{
		// drop out-of-order packets
		if (in_seq <= mRecvSequence)
		{
			Net_LogPrintf("Connection::parsePacketHeader: dropping out-of-order packet from %s (sequence %u, last sequence %u).",
					getRemoteAddressCString(), in_seq.getInteger(), mRecvSequence.getInteger());
		
			stream.clear();
			return;
		}

		// Record the received sequence number and update the bitfield that
		// stores acknowledgement for the N previously received sequence numbers.
	
		uint32_t diff = PacketSequenceNumber(in_seq - mRecvSequence).getInteger();
		if (diff > 0)
		{
			// shift the window of received sequence numbers to make room
			mRecvHistory <<= diff;
			// add the previous sequence received
			mRecvHistory.set(diff - 1);
		}
	}

	mRecvSequence = in_seq;

	// read the list of packets the remote host has acknowledge receiving
	PacketSequenceNumber ack_seq;
	ack_seq.read(stream);

	BitFieldMessageComponent ack_history_field(ACKNOWLEDGEMENT_COUNT);
	ack_history_field.read(stream);
	const BitField& ack_history = ack_history_field.get();

	if (mLastAckSequenceValid)
	{
		// sanity check (acknowledging a packet we haven't sent yet?!?!)
		if (ack_seq >= mSequence)
		{
			Net_Warning("Connection::parsePacketHeader: Remote host %s acknowledges an unknown packet.", getRemoteAddressCString());
			stream.clear();
			return;
		}

		// sanity check (out of order acknowledgement)
		if (ack_seq < mLastAckSequence)
		{
			Net_Warning("Connection::parsePacketHeader: Out-of-order acknowledgement from remote host %s.", getRemoteAddressCString());
			stream.clear();
			return;
		}

		for (PacketSequenceNumber seq = mLastAckSequence + 1; seq <= ack_seq; ++seq)
		{
			if (seq < ack_seq - ACKNOWLEDGEMENT_COUNT)
			{
				// seq is too old to fit in the ack_history window
				// consider it lost
				notifyLost(seq);
				++mLostPacketCount;
			}
			else if (seq == ack_seq)
			{
				// seq is the most recent acknowledgement in the header
				notifyReceived(seq);
			}
			else
			{
				// seq's delivery status is indicated by a bit in the
				// ack_history bitfield
				uint32_t bit = PacketSequenceNumber(ack_seq - seq - 1).getInteger();
				if (ack_history.get(bit))
				{
					notifyReceived(seq);
				}
				else
				{
					notifyLost(seq);
					++mLostPacketCount;
				}
			}
		}
	}

	mLastAckSequence = ack_seq;
}


//
// Connection::composePacketHeader
//
// Writes the header portion of a new packet. The type of the packet is
// written first, followed by the outgoing sequence number and then the
// acknowledgement of receipt for recent packets.
//
void Connection::composePacketHeader(const PacketType& type, BitStream& stream)
{
	Net_LogPrintf("Connection::composePacketHeader: writing packet header to %s, sequence %u.", getRemoteAddressCString(), mSequence.getInteger());

	PacketTypeMessageComponent packettype;
	packettype.set(type);
	packettype.write(stream);

	mSequence.write(stream);
	mRecvSequence.write(stream);

	BitFieldMessageComponent ack_bitfield(mRecvHistory);
	ack_bitfield.write(stream);

	++mSequence;
}

//
// Connection::composeGamePacket
//
// Writes the header for a new game packet and calls the registered
// composition callbacks to write game data to the packet.
//
void Connection::composeGamePacket(BitStream& stream)
{
	Net_LogPrintf("Connection::composeGamePacket: composing packet to %s.", getRemoteAddressCString());
	stream.clear();
	composePacketHeader(GAME_PACKET, stream);

	// call the registered packet composition functions to compose the payload
/*
	uint16_t size_left = stream.writeSize();
	for (std::list<MessageManager*>::iterator itr = mMessageManagers.begin(); itr != mMessageManagers.end(); ++itr)
	{
		MessageManager* composer = *itr;
		size_left -= composer->write(stream, size_left, mSequence);
	}
*/
}

//
// Connection::parseGamePacket
//
// Handles acknowledgement of packets
//
void Connection::parseGamePacket(BitStream& stream)
{
	Net_LogPrintf("Connection::processGamePacket: processing packet from %s.", getRemoteAddressCString());

	parsePacketHeader(stream);

/*
	// call the registered packet parser functions to parse the payload
	uint16_t size_left = stream.readSize();
	for (std::list<MessageManager*>::iterator itr = mMessageManagers.begin(); itr != mMessageManagers.end(); ++itr)
	{
		MessageManager* parser = *itr;
		size_left -= parser->read(stream, size_left);
	}
*/
}


//
// Connection::notifyReceived
//
// Notifies all of the registered MessageManagers that a packet with the
// specified sequence number was successfully received by the remote host.
//
void Connection::notifyReceived(const PacketSequenceNumber& seq)
{
	Net_LogPrintf("Connection::notifyReceived: Remote host received packet %u.", seq.getInteger());

	if (isConnected() == false)
		return;

/*
	for (std::list<MessageManager*>::iterator itr = mMessageManagers.begin(); itr != mMessageManagers.end(); ++itr)
	{
		MessageManager* manager = *itr;
		manager->notifyReceived(seq);
	}
*/
}


//
// Connection::notifyLost
//
// Notifies all of the registered MessageManagers that a packet with the
// specified sequence number was not received by the remote host.
//
void Connection::notifyLost(const PacketSequenceNumber& seq)
{
	Net_LogPrintf("Connection::notifyLost: Remote host did not receive packet %u.", seq.getInteger());

	if (isConnected() == false)
		return;

/*
	for (std::list<MessageManager*>::iterator itr = mMessageManagers.begin(); itr != mMessageManagers.end(); ++itr)
	{
		MessageManager* manager = *itr;
		manager->notifyLost(seq);
	}
*/
}

VERSION_CONTROL (net_connection_cpp, "$Id$")

