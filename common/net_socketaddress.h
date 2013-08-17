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
//  Network socket address class
//
//-----------------------------------------------------------------------------

#ifndef __NET_SOCKETADDRESS_H__
#define __NET_SOCKETADDRESS_H__

#include "doomdef.h"
#include "doomtype.h"

// ============================================================================
//
// SocketAddress Interface
//
// ============================================================================

class SocketAddress
{
public:
	SocketAddress();
	SocketAddress(uint8_t oct1, uint8_t oct2, uint8_t oct3, uint8_t oct4, uint16_t port);
	SocketAddress(const SocketAddress& other);
	SocketAddress(const std::string& stradr);
	SocketAddress(const struct sockaddr_in sadr);

	SocketAddress& operator=(const SocketAddress& other);
	bool operator==(const SocketAddress& other) const;
	bool operator!=(const SocketAddress& other) const;
	bool operator<(const SocketAddress& other) const;
	bool operator<=(const SocketAddress& other) const;
	bool operator>(const SocketAddress& other) const;
	bool operator>=(const SocketAddress& other) const;
	operator std::string () const;

	void setIPAddress(uint8_t oct1, uint8_t oct2, uint8_t oct3, uint8_t oct4);
	void setPort(uint16_t port);

	uint32_t getIPAddress() const;
	uint16_t getPort() const;

	void clear();
	bool isValid() const;

	struct sockaddr_in toSockAddr() const;

private:
	uint32_t    mIP;
	uint16_t    mPort;
};

#endif	// __NET_SOCKETADDRESS_H__

