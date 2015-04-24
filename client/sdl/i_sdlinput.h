// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_sdlinput.h 5302 2015-04-23 01:53:13Z dr_sean $
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
//
//-----------------------------------------------------------------------------

#ifndef __I_SDLINPUT_H__
#define __I_SDLINPUT_H__

#include "doomtype.h"
#include <SDL.h>
#include "i_input.h"

#include "d_event.h"
#include <queue>


class ISDL12MouseInputDevice : public IInputDevice
{
public:
	ISDL12MouseInputDevice();
	virtual ~ISDL12MouseInputDevice() { }

	virtual bool paused() const
	{	return mActive == false;		}

	virtual void pause();
	virtual void resume();
	virtual void reset();

	virtual void gatherEvents();

	virtual bool hasEvent() const
	{	return !mEvents.empty();	}

	virtual void getEvent(event_t* ev);

	virtual void flushEvents();

private:
	void center();

	bool			mActive;

	typedef std::queue<SDL_Event> EventQueue;
	EventQueue		mEvents;
};

#endif	// __I_SDLINPUT_H__
