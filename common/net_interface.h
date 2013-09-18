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
// Abstraction for low-level network mangement 
//
//-----------------------------------------------------------------------------

#ifndef __NET_INTERFACE_H__
#define __NET_INTERFACE_H__

#include "net_type.h"
#include "net_socketaddress.h"
#include "hashtable.h"
#include <map>
#include <queue>
#include <string>

#ifdef _WIN32
	#ifdef _XBOX
		#include <xtl.h>
	#else
		#include <winsock2.h>
	#endif	// ! _XBOX
#else
	typedef int SOCKET;
#endif

class Connection;


// ============================================================================
//
// // Type Definitions
//
// ============================================================================

static const uint8_t CONNECTION_ID_BITS = 9;		// allows for 0 - 512
typedef SequenceNumber<CONNECTION_ID_BITS> ConnectionId;
typedef IdGenerator<CONNECTION_ID_BITS> ConnectionIdGenerator;

//
// Function object to generate hash keys from SocketAddress objects
//
template <> struct hashfunc<const SocketAddress>
{   size_t operator()(const SocketAddress& adr) const { return (adr.getIPAddress() ^ adr.getPort()); } };

// ============================================================================
//
// Connection management signals
//
// ============================================================================

static const uint32_t CONNECTION_SEQUENCE = 0xFFFFFFFF;
static const uint32_t TERMINATION_SEQUENCE = 0xAAAAAAAA;
static const uint32_t GAME_CHALLENGE = 0x00ABABAB;

// ============================================================================
//
// NetInterface Interface
//
// 
// ============================================================================

typedef enum
{
	HOST_CLIENT,
	HOST_SERVER
} HostType_t;

class NetInterface
{
public:
	NetInterface(const std::string& address, uint16_t desired_port);
	~NetInterface();

	HostType_t getHostType() const;
	const SocketAddress& getLocalAddress() const;
	const std::string& getPassword() const;
	void setPassword(const std::string& password);

	ConnectionId requestConnection(const SocketAddress& adr);
	void closeConnection(const ConnectionId& connection_id);
	void closeAllConnections();
	bool isConnected(const ConnectionId& connection_id) const;
	Connection* findConnection(const ConnectionId& connection_id);
	const Connection* findConnection(const ConnectionId& connection_id) const;
	void sendPacket(const ConnectionId& connection_id, BitStream& stream);
	void service();

	typedef enum
	{
		TERM_RECVTERM,
		TERM_QUIT,
		TERM_TIMEOUT,
		TERM_KICK,
		TERM_BAN,
		TERM_BADPASSWORD,
		TERM_BADHANDSHAKE,
		TERM_PARSEERROR,
		TERM_VERSIONMISMATCH,
		TERM_UNSPECIFIED		// should always be last
	} TermType_t;
	
private:
	// ------------------------------------------------------------------------
	// Low-level socket interaction
	// ------------------------------------------------------------------------

	void openInterface(const std::string& address, uint16_t desired_port);
	void closeInterface();
	void openSocket(uint32_t desired_ip, uint16_t desired_port);
	void closeSocket();
	bool socketSend(const SocketAddress& dest, const uint8_t* databuf, uint16_t size);
	bool socketRecv(SocketAddress& source, uint8_t* data, uint16_t& size);

	HostType_t				mHostType;
	bool					mInitialized;
	SocketAddress			mLocalAddress;
	SOCKET					mSocket;

	// ------------------------------------------------------------------------
	// Connection handling
	// ------------------------------------------------------------------------

	ConnectionIdGenerator		mConnectionIdGenerator;

//	typedef HashTable<SocketAddress, Connection*> ConnectionAddressTable;
//	typedef HashTable<ConnectionId, Connection*> ConnectionIdTable;
	typedef std::map<SocketAddress, Connection*> ConnectionAddressTable;
	typedef std::map<ConnectionId, Connection*> ConnectionIdTable;

	ConnectionAddressTable		mConnectionsByAddress;
	ConnectionIdTable			mConnectionsById;

	Connection* findConnection(const SocketAddress& adr);
	const Connection* findConnection(const SocketAddress& adr) const;
	Connection* createNewConnection(const SocketAddress& remote_address);

	void processPacket(const SocketAddress& remote_address, BitStream& stream);

	std::string				mPassword;
};

#endif	// __NET_INTERFACE_H__


