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
// Main common processing for Client/Server functionality 
//
//-----------------------------------------------------------------------------

#include "net_common.h"
#include "net_log.h"
#include "net_interface.h"
#include "net_connection.h"
#include "net_socketaddress.h"

static NetInterface* interface;
static ConnectionId client_connection_id;

// ============================================================================
//
// Network Interface functions
//
// ============================================================================


//
// CS_CloseNetInterface
//
// Closes the network interface. This must be called upon application
// termination.
//
void CS_CloseNetInterface()
{
	if (interface)
	{
		delete interface;
		interface = NULL;
	}
}


//
// CS_OpenNetInterface
//
// Opens a socket on the network interface matching the given IP address 
// at the given preferred port. If the given port is unavailible, up to 16
// sequential port numbers will be tried.
//
// This must be called prior to all other network functions.
//
void CS_OpenNetInterface(const std::string& address, uint16_t port)
{
	if (interface)
		CS_CloseNetInterface();

	interface = new NetInterface(address, port);
}


//
// CS_ServiceNetInterface
//
// Performs regular servicing of the network interface, including retreiving
// incoming packets from the socket, checking for time-outs, and delivering
// packets using latency simulation.
//
// This should be called once every gametic.
//
void CS_ServiceNetInterface()
{
	if (interface)
	{
		interface->service();
	}
}


// ============================================================================
//
// Client-only Connection functions
//
// ============================================================================

//
// CS_CloseConnection
//
// Closes the connection to the remote host.
// This should only be used by the client application.
//
void CS_CloseConnection()
{
	if (interface)
	{
		interface->closeConnection(client_connection_id);
	}
}


//
// CS_OpenConnection
//
// Opens a new connection to the remote host given by socket address string.
// This should only be used by the client application.
// Note that this requires CS_OpenNetInterface to be called prior.
//
void CS_OpenConnection(const std::string& socket_address_string)
{
	if (interface)
	{
		SocketAddress socket_address(socket_address_string);
		client_connection_id = interface->requestConnection(socket_address);
	}
}


//
// CS_IsConnected
//
// Returns true if the client application is currently connected to a remote
// server.
//
bool CS_IsConnected()
{
	return interface && interface->isConnected(client_connection_id);
}

VERSION_CONTROL (cs_main_cpp, "$Id$")

