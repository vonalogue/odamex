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
// Message logging for the network subsystem
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "net_log.h"
#include "i_system.h"

static const size_t printbuf_size = 4096;
static char printbuf[printbuf_size];

//
// Net_LogPrintf
//
// Prints a debugging message to stderr if network debugging has been enabled
// at compile-time.
//

#ifdef __NET_DEBUG__
void Net_LogPrintf(const char* str, ...)
{
	static const char* prefix = "LOG";

	va_list args;
	va_start(args, str);

	vsnprintf(printbuf, printbuf_size, str, args);
	fprintf(stderr, "%s: %s\n", prefix, printbuf);

	va_end(args);
	fflush(stderr);
}
#endif	// __NET_DEBUG__

void Net_Warning(const char* str, ...)
{
	static const char* prefix = "WARNING";

	va_list args;
	va_start(args, str);

	vsnprintf(printbuf, printbuf_size, str, args);
	fprintf(stderr, "%s: %s\n", prefix, printbuf);

	va_end(args);
	fflush(stderr);
}

void Net_Error(const char* str, ...)
{
	va_list args;
	va_start(args, str);

	vsnprintf(printbuf, printbuf_size, str, args);
	va_end(args);

	I_Error(printbuf);
}

void Net_Printf(const char* str, ...)
{
	va_list args;
	va_start(args, str);

	vsnprintf(printbuf, printbuf_size, str, args);
	va_end(args);

	Printf(PRINT_HIGH, printbuf);
	Printf(PRINT_HIGH, "\n");
}


VERSION_CONTROL (net_log_cpp, "$Id$")

