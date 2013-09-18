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

#include "net_socketaddress.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#ifdef _XBOX
		#include <xtl.h>
	#else
		#include <windows.h>
		#include <winsock2.h>
		#include <ws2tcpip.h>
	#endif // !_XBOX
#else
	#ifdef GEKKO // Wii/GC
		#include <network.h>
	#else
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <arpa/inet.h>
		#include <netdb.h>
		#include <sys/ioctl.h>
	#endif // GEKKO

	#include <sys/types.h>
	#include <errno.h>
	#include <unistd.h>
	#include <sys/time.h>
#endif // WIN32

#ifndef _WIN32
	typedef int SOCKET;
	#ifndef GEKKO
		#define SOCKET_ERROR -1
		#define INVALID_SOCKET -1
	#endif // !GEKKO

	#define closesocket close
	#define ioctlsocket ioctl
	#define Sleep(x)	usleep (x * 1000)
#endif

#ifdef _XBOX
	#include "i_xbox.h"
#endif

#ifdef GEKKO
	#include "i_wii.h"
#endif

// ============================================================================
//
// SocketAddress Implementation
//
// ============================================================================

SocketAddress::SocketAddress() :
	mIP(0), mPort(0), mIsSockDirty(true), mIsStringDirty(true)
{
}

SocketAddress::SocketAddress(uint8_t oct1, uint8_t oct2, uint8_t oct3, uint8_t oct4, uint16_t port) :
	mIP((oct1 << 24) | (oct2 << 16) | (oct3 << 8) | oct4),
	mPort(port), mIsSockDirty(true), mIsStringDirty(true)
{
}

SocketAddress::SocketAddress(const SocketAddress& other)
{
	mIP = other.mIP;
	mPort = other.mPort;
	mIsSockDirty = mIsStringDirty = true;
}

SocketAddress::SocketAddress(const std::string& stradr)
{
	struct hostent *h;
	struct sockaddr_in sadr;
	char *colon;

	memset(&sadr, 0, sizeof(sadr));
	sadr.sin_family = AF_INET;

	char copy[256];
	strncpy(copy, stradr.c_str(), sizeof(copy) - 1);
	copy[sizeof(copy) - 1] = 0;

	// strip off a trailing :port if present
	for (colon = copy; *colon; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			sadr.sin_port = htons(atoi(colon+1));
		}
	}

	if (!(h = gethostbyname(copy)))
		return;

	*(int *)&sadr.sin_addr = *(int *)h->h_addr_list[0];

	mIP = ntohl(sadr.sin_addr.s_addr);
	mPort = ntohs(sadr.sin_port);
	mIsSockDirty = mIsStringDirty = true;
}

SocketAddress::SocketAddress(const struct sockaddr_in sadr)
{
	mIP = ntohl(sadr.sin_addr.s_addr);
	mPort = ntohs(sadr.sin_port);
	mIsSockDirty = mIsStringDirty = true;
}

SocketAddress& SocketAddress::operator=(const SocketAddress& other)
{
	if (&other != this)
	{
		mIP = other.mIP;
		mPort = other.mPort;
		mIsSockDirty = mIsStringDirty = true;
	}
   
	return *this;
}

bool SocketAddress::operator==(const SocketAddress& other) const
{
	return mIP == other.mIP && mPort == other.mPort;
}

bool SocketAddress::operator!=(const SocketAddress& other) const
{
	return !(operator==(other));
}

bool SocketAddress::operator<(const SocketAddress& other) const
{
	return (mIP < other.mIP) || (mIP == other.mIP && mPort < other.mPort);
}

bool SocketAddress::operator<=(const SocketAddress& other) const
{
	return (mIP < other.mIP) || (mIP == other.mIP && mPort <= other.mPort);
}

bool SocketAddress::operator>(const SocketAddress& other) const
{
	return !(operator<=(other));
}

bool SocketAddress::operator>=(const SocketAddress& other) const
{
	return !(operator<(other));
}

void SocketAddress::setIPAddress(uint32_t ip)
{
	mIP = ip;
	mIsSockDirty = mIsStringDirty = true;
}

void SocketAddress::setIPAddress(uint8_t oct1, uint8_t oct2, uint8_t oct3, uint8_t oct4)
{
	mIP = (oct1 << 24) | (oct2 << 16) | (oct3 << 8) | (oct4);
	mIsSockDirty = mIsStringDirty = true;
}

void SocketAddress::setPort(uint16_t port)
{
	mPort = port;
	mIsSockDirty = mIsStringDirty = true;
}

uint32_t SocketAddress::getIPAddress() const
{
	return mIP;
}

uint16_t SocketAddress::getPort() const
{
	return mPort;
}

void SocketAddress::clear()
{
	mIP = mPort = 0;
	mIsSockDirty = mIsStringDirty = true;
}

bool SocketAddress::isValid() const
{
	return mIP > 0 && mPort > 0;
}

const struct sockaddr_in& SocketAddress::getSockAddrIn() const
{
	if (mIsSockDirty)
	{
		memset(&mSockAddrIn, 0, sizeof(mSockAddrIn));
		mSockAddrIn.sin_family = AF_INET;
		mSockAddrIn.sin_addr.s_addr = htonl(mIP);
		mSockAddrIn.sin_port = htons(mPort);
		mIsSockDirty = false;
	}

	return mSockAddrIn;
}

const std::string& SocketAddress::getString() const
{
	if (mIsStringDirty)
	{
		char buf[22];
		sprintf(buf, "%u.%u.%u.%u:%u",
				(mIP >> 24) & 0xFF,
				(mIP >> 16) & 0xFF,
				(mIP >> 8 ) & 0xFF,
				(mIP      ) & 0xFF,
				mPort);

		mString = buf;
		mIsStringDirty = false;
	}

	return mString;
}

const char* SocketAddress::getCString() const
{
	return getString().c_str();
}

VERSION_CONTROL (net_socketaddress_cpp, "$Id$")

