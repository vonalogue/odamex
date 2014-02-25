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
// Message logging for the network subsystem
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <algorithm>

#include "net_log.h"
#include "i_system.h"
#include "c_dispatch.h"

static const size_t printbuf_size = 4096;
static char printbuf[printbuf_size];

typedef OHashTable<OString, LogChannel*> LogChannelTable;
static LogChannelTable log_channels;

const OString LogChan_Interface		= "interface";
const OString LogChan_Connection	= "connection";

// ============================================================================
//
// LogChannel class implementation
//
// ============================================================================

//
// LogChannel Constructor
//
// Initializes a LogChannel object with the specified channel name, file name
// for the log's output, and initial status (enabled or disabled). Instead of
// an actual file name, the caller may pass "stdout" or "stderr" to use
// the operating system's standard output or standard error output facilities.
//
LogChannel::LogChannel(const OString& name, const OString& filename, bool enabled) :
		mName(name), mEnabled(enabled), mStream(NULL)
{
	setFilename(filename);
}


//
// LogChannel Destructor
//
LogChannel::~LogChannel()
{
	close();
}

//
// LogChannel::close
//
// Closes the log file.
//
void LogChannel::close()
{
	if (mStream != stdout && mStream != stderr && mStream != NULL)
		fclose(mStream);
}


//
// LogChannel::setFilename
//
// Closes the existing log file and opens the specified file name for writing.
//
void LogChannel::setFilename(const OString& filename)
{
	close();	

	if (filename.empty() || filename.iequals("stdout"))
		mStream = stdout;
	else if (filename.iequals("stderr"))
		mStream = stderr;
	else
		mStream = fopen(filename.c_str(), "w");
}


//
// LogChannel::write
//
// Writes a text string to the log channel (provided it is enabled).
//
void LogChannel::write(const char* str)
{
	if (mStream != NULL && mEnabled)
	{
		// ensure we're writing to the end of the file even if multiple
		// LogChannels are writting to the same file
		fseek(mStream, 0, SEEK_END);
		fprintf(mStream, "%s", str);
		fflush(mStream);
	}
}
	

// ============================================================================


//
// Net_LogStartup
//
// Initializes the network logging system.
//
void Net_LogStartup()
{
}


//
// Net_LogShutdown
//
// Free the LogChannel pointers stored in log_channels.
//
void STACK_ARGS Net_LogShutdown()
{
	for (LogChannelTable::iterator it = log_channels.begin(); it != log_channels.end(); ++it)
		delete it->second;
	log_channels.clear();
}


//
// Net_PrintLogChannelNames
//
// Prints a list of the log channel names to the console.
//
void Net_PrintLogChannelNames()
{
	Printf(PRINT_HIGH, "Networking Subsystem Log Channels:\n");
	
	std::vector<OString> sorted_channel_names;

	for (LogChannelTable::const_iterator it = log_channels.begin(); it != log_channels.end(); ++it)
		sorted_channel_names.push_back(it->first);

	std::sort(sorted_channel_names.begin(), sorted_channel_names.end());

	for (std::vector<OString>::const_iterator it = sorted_channel_names.begin();
		it != sorted_channel_names.end(); ++it)
	{
		const OString& channel_name = *it;
		Printf(PRINT_HIGH, "%s\n", channel_name.c_str());
	}
}

BEGIN_COMMAND(net_logchannames)
{
	Net_PrintLogChannelNames();
}
END_COMMAND(net_logchannames)


//
// Net_EnableLogChannel
//
void Net_EnableLogChannel(const OString& channel_name)
{
	// look up the LogChannel object by name
	LogChannelTable::iterator it = log_channels.find(channel_name);
	if (it != log_channels.end())
	{
		LogChannel* channel = it->second;
		channel->setEnabled(true);
	}
}

//
// Net_DisableLogChannel
//
void Net_DisableLogChannel(const OString& channel_name)
{
	// look up the LogChannel object by name
	LogChannelTable::iterator it = log_channels.find(channel_name);
	if (it != log_channels.end())
	{
		LogChannel* channel = it->second;
		channel->setEnabled(false);
	}
}


//
// Net_SetLogChannelDestination
//
void Net_SetLogChannelDestination(const OString& channel_name, const OString& destination)
{
	// look up the LogChannel object by name
	LogChannelTable::iterator it = log_channels.find(channel_name);
	if (it != log_channels.end())
	{
		LogChannel* channel = it->second;
		channel->setFilename(destination);
	}
}


BEGIN_COMMAND(net_logchandest)
{
	if (argc != 3)
	{
		Printf(PRINT_HIGH, "Usage: %s channel_name file_name\n", argv[0]);
	}
	else
	{
		Net_SetLogChannelDestination(argv[1], argv[2]);
	}	
}
END_COMMAND(net_logchandest)

//
// Net_LogPrintf2
//
// Prints a message to the specified log channel.
// This function should only be used by the Net_LogPrintf macro and not called
// directly as the macro automatically passes the calling function's name.
//
void Net_LogPrintf2(const OString& channel_name, const char* func_name, const char* str, ...)
{
#ifdef __NET_DEBUG__
	// look up the LogChannel object by name
	LogChannelTable::iterator it = log_channels.find(channel_name);
	if (it == log_channels.end())
	{
		// create a new LogChannel object if one doesn't already exist with this channel name
		LogChannel* new_channel = new LogChannel(channel_name);
		it = log_channels.insert(std::make_pair(channel_name, new_channel)).first;
	}

	LogChannel* channel = it->second;
	if (channel->isEnabled())
	{
		const size_t bufsize = 4096;
		char output[bufsize];

		// prepend the channel name & name of the calling function to the output
		sprintf(output, "[%s] %s: ", channel_name.c_str(), func_name);	

		va_list args;
		va_start(args, str);

		vsprintf(output + strlen(output), str, args);
		va_end(args);

		channel->write(output);
		channel->write("\n");
	}
#endif	// __NET_DEBUG__
}


void Net_Warning(const char* str, ...)
{
	static const char* prefix = "WARNING";

	va_list args;
	va_start(args, str);

	vsnprintf(printbuf, printbuf_size, str, args);
	fprintf(stdout, "%s: %s\n", prefix, printbuf);

	va_end(args);
	fflush(stdout);
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

