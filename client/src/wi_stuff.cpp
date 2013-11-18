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
//		Intermission screens.
//
//-----------------------------------------------------------------------------


#include <ctype.h>
#include <stdio.h>

#include "z_zone.h"
#include "m_random.h"
#include "m_swap.h"
#include "i_system.h"
#include "i_video.h"
#include "w_wad.h"
#include "g_game.h"
#include "g_level.h"
#include "r_local.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "v_video.h"
#include "wi_stuff.h"
#include "c_console.h"
#include "hu_stuff.h"
#include "v_palette.h"
#include "gi.h"
#include "v_font.h"
#include "v_text.h"

void WI_unloadData(void);

//
// Data needed to add textures to full screen intermission pics.
// Textures are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//


//
// Different vetween registered DOOM (1994) and
//	Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//	release (aka DOOM II), which had 32 maps
//	in one episode. So there.
#define NUMEPISODES 	4
#define NUMMAPS 		9


// GLOBAL LOCATIONS
#define WI_TITLEY				2
#define WI_SPACINGY 			33

// Single Player
#define SP_STATSX		50
#define SP_STATSY		50
#define SP_TIMEX		16
#define SP_TIMEY		168

// NET GAME STUFF
#define NG_STATSY				50
#define NG_STATSX				(32 + star->getWidth() / 2 + 32*!dofrags)

#define NG_SPACINGX 			64


typedef enum
{
	ANIM_ALWAYS,
    ANIM_RANDOM,
	ANIM_LEVEL
} animenum_t;

typedef struct
{
    int		x;
    int		y;

} point_t;

//
// Animation.
// There is another anim_t used in the TextureManager class.
//
typedef struct
{
    animenum_t	type;

    // period in tics between animations
    int		period;

    // number of animation frames
    int		nanims;

    // location of animation
    point_t	loc;

    // ALWAYS: n/a,
    // RANDOM: period deviation (<256),
    // LEVEL: level
    int		data1;

    // ALWAYS: n/a,
    // RANDOM: random base period,
    // LEVEL: n/a
    int		data2;

    // actual graphics for frames of animations
	const Texture*	textures[3];

    // following must be initialized to zero before use!

    // next value of bcnt (used in conjunction with period)
    int		nexttic;

    // last drawn animation frame
    int		lastdrawn;

    // next frame number to animate
    int		ctr;

    // used by RANDOM and LEVEL when animating
    int		state;

} anim_t;

static point_t lnodes[NUMEPISODES][NUMMAPS] =
{
    // Episode 0 World Map
    {
	{ 185, 164 },	// location of level 0 (CJ)
	{ 148, 143 },	// location of level 1 (CJ)
	{ 69, 122 },	// location of level 2 (CJ)
	{ 209, 102 },	// location of level 3 (CJ)
	{ 116, 89 },	// location of level 4 (CJ)
	{ 166, 55 },	// location of level 5 (CJ)
	{ 71, 56 },	// location of level 6 (CJ)
	{ 135, 29 },	// location of level 7 (CJ)
	{ 71, 24 }	// location of level 8 (CJ)
    },

    // Episode 1 World Map should go here
    {
	{ 254, 25 },	// location of level 0 (CJ)
	{ 97, 50 },	// location of level 1 (CJ)
	{ 188, 64 },	// location of level 2 (CJ)
	{ 128, 78 },	// location of level 3 (CJ)
	{ 214, 92 },	// location of level 4 (CJ)
	{ 133, 130 },	// location of level 5 (CJ)
	{ 208, 136 },	// location of level 6 (CJ)
	{ 148, 140 },	// location of level 7 (CJ)
	{ 235, 158 }	// location of level 8 (CJ)
    },

    // Episode 2 World Map should go here
    {
	{ 156, 168 },	// location of level 0 (CJ)
	{ 48, 154 },	// location of level 1 (CJ)
	{ 174, 95 },	// location of level 2 (CJ)
	{ 265, 75 },	// location of level 3 (CJ)
	{ 130, 48 },	// location of level 4 (CJ)
	{ 279, 23 },	// location of level 5 (CJ)
	{ 198, 48 },	// location of level 6 (CJ)
	{ 140, 25 },	// location of level 7 (CJ)
	{ 281, 136 }	// location of level 8 (CJ)
    }

};

//
// Animation locations for episode 0 (1).
// Using textures saves a lot of space,
//  as they replace 320x200 full screen frames.
//
static anim_t epsd0animinfo[] =
{
    { ANIM_ALWAYS, TICRATE/3, 3, { 224, 104 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 184, 160 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 112, 136 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 72, 112 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 88, 96 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 64, 48 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 192, 40 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 136, 16 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 80, 16 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 64, 24 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 }
};

static anim_t epsd1animinfo[] =
{
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 1, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 2, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 3, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 4, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 5, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 6, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 7, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 3, { 192, 144 }, 8, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  },
    { ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 8, 0, { NULL, NULL, NULL }, 0, 0, 0, 0  }
};

static anim_t epsd2animinfo[] =
{
    { ANIM_ALWAYS, TICRATE/3, 3, { 104, 168 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 40, 136 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 160, 96 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 104, 80 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/3, 3, { 120, 32 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 },
    { ANIM_ALWAYS, TICRATE/4, 3, { 40, 0 }, 0, 0, { NULL, NULL, NULL }, 0, 0, 0, 0 }
};

static int NUMANIMS[NUMEPISODES] =
{
	sizeof(epsd0animinfo)/sizeof(anim_t),
	sizeof(epsd1animinfo)/sizeof(anim_t),
	sizeof(epsd2animinfo)/sizeof(anim_t)
};

static anim_t *anims[NUMEPISODES] =
{
	epsd0animinfo,
	epsd1animinfo,
	epsd2animinfo
};

// [RH] Map name -> index mapping
static char names[NUMEPISODES][NUMMAPS][8] = {
	{ "E1M1", "E1M2", "E1M3", "E1M4", "E1M5", "E1M6", "E1M7", "E1M8", "E1M9" },
	{ "E2M1", "E2M2", "E2M3", "E2M4", "E2M5", "E2M6", "E2M7", "E2M8", "E2M9" },
	{ "E3M1", "E3M2", "E3M3", "E3M4", "E3M5", "E3M6", "E3M7", "E3M8", "E3M9" }
};

//
// GENERAL DATA
//

//
// Locally used stuff.
//
#define FB (screen)


// in seconds
#define SHOWNEXTLOCDELAY		4
//#define SHOWLASTLOCDELAY		SHOWNEXTLOCDELAY


// used to accelerate or skip a stage
static bool				acceleratestage;

// wbs->pnum
static unsigned			me;

 // specifies current state
static stateenum_t		state;

// contains information passed into intermission
static wbstartstruct_t* wbs;

static std::vector<wbplayerstruct_t> plrs;	// = wbs->plyr
static std::vector<int> cnt_kills_c;	// = cnt_kills
static std::vector<int> cnt_items_c;	// = cnt_items
static std::vector<int> cnt_secret_c;	// = cnt_secret
static std::vector<int> cnt_frags_c;	// = cnt_frags
static int dofrags;
static int ng_state;

// used for general timing
static int				cnt;

// used for timing of background animation
static int				bcnt;

/*struct count_t
{
	int cnt_kills, cnt_items, cnt_secret, cnt_frags;
};

static std::vector<count_t> stats;
static std::vector<int> dm_totals;*/

// Since classic is used for singleplayer only...
static int			cnt_kills;
static int			cnt_items;
static int			cnt_secret;
static int			cnt_time;
static int			cnt_par;
static int			cnt_pause;


//
//		GRAPHICS
//

// You Are Here graphic
static const Texture*	yah[2];

// splat
static const Texture* 	splat;

// %, : graphics
static const Texture*	percent;
static const Texture*	colon;

// 0-9 graphic
static const Texture* 	num[10];

// minus sign
static const Texture* 	wiminus;

// "Finished!" graphics
static const Texture* 	finished; //(Removed) Dan - Causes GUI Issues |FIX-ME|
// [Nes] Re-added for singleplayer

// "Entering" graphic
static const Texture* 	entering;

 // "Kills", "Items", "Secrets"
static const Texture*	kills;
static const Texture*	secret;
static const Texture*	items;
static const Texture*	frags;
static const Texture*	scrt;

// Time sucks.
static const Texture*	timepatch;
static const Texture*	par;
static const Texture*	sucks;

// "Total", your face, your dead face
static const Texture* 	total;
static const Texture* 	star;
static const Texture* 	bstar;

static const Texture* 	face_bg;		// [RH] Only one

static Texture*	background_tex;

 // Name graphics of each level (centered)
static const Texture*	lnames[2];

static OFont* wi_font = NULL;



// [RH] Info to dynamically generate the level name graphics
static int				lnamewidths[2];
static const char*		lnametexts[2];

EXTERN_CVAR (sv_maxplayers)
EXTERN_CVAR (wi_newintermission)

//
// CODE
//


//Draws "<Levelname> Finished!"
void WI_drawLF (void)
{
	int y = WI_TITLEY;

	if (lnames[0] != TextureManager::NOT_FOUND_TEXTURE_HANDLE)
	{
		// draw <LevelName> centered
		const Texture* texture = lnames[0];
		FB->DrawTextureClean(texture, (320 - texture->getWidth()) / 2, y);
		y += (5 * texture->getHeight()) / 4;
	}
	else
	{
		// [RH] draw a dynamic title string
		const char* str = lnametexts[0];
		int str_width = wi_font->getTextWidth(str) / CleanXfac;
		int str_height = wi_font->getTextHeight(str) / CleanYfac;
	
		int realx = CleanXfac * (160 - str_width) / 2;
		int realy = CleanYfac * y;
		wi_font->printText(FB, realx, realy, CR_WHITE, str);
		y += str_height;
	}

	// draw "Finished!" centered
	FB->DrawTextureClean(finished, (320 - finished->getWidth()) / 2, y);
}



// Draws "Entering <LevelName>"
void WI_drawEL (void)
{
	int y = WI_TITLEY;

	// draw "Entering"
	FB->DrawTextureClean(entering, (320 - entering->getWidth()) / 2, y);
	y += (5 * entering->getHeight()) / 4;

	if (lnames[1] != TextureManager::NOT_FOUND_TEXTURE_HANDLE)
	{
		// draw <LevelName> centered
		const Texture* texture = lnames[1];
		FB->DrawTextureClean(texture, (320 - texture->getWidth()) / 2, y);
	}
	else
	{
		// [RH] draw a dynamic title string
		const char* str = lnametexts[1];
		int str_width = wi_font->getTextWidth(str) / CleanXfac;
		int str_height = wi_font->getTextHeight(str) / CleanYfac;
	
		int realx = CleanXfac * (160 - str_width) / 2;
		int realy = CleanYfac * y;
		wi_font->printText(FB, realx, realy, CR_WHITE, str);
		y += str_height;
	}
}

int WI_MapToIndex (char *map)
{
	int i;

	for (i = 0; i < NUMMAPS; i++)
	{
		if (!strnicmp (names[wbs->epsd][i], map, 8))
			break;
	}
	return i;
}


// ====================================================================
// WI_drawOnLnode
// Purpose: Draw textures at a location based on episode/map
// Args:    n   -- index to map# within episode
//          c[] -- array of textures to be drawn
//          numtextures -- haleyjd 04/12/03: bug fix - number of textures
// Returns: void
//
// draw stuff at a location by episode/map#
//
// [Russell] - Modified for odamex, fixes a crash with certain pwads at
// intermission change
void WI_drawOnLnode(int n, const Texture* c[], int numtextures)
{
	int 	i;
	bool 	fits = false;

	i = 0;
	do
	{
		int left = lnodes[wbs->epsd][n].x - c[i]->getOffsetX();
		int top = lnodes[wbs->epsd][n].y - c[i]->getOffsetY();
		int right = left + c[i]->getWidth();
		int bottom = top + c[i]->getHeight();

		if (left >= 0 && right < FB->width &&
            top >= 0 && bottom < FB->height)
		{
			fits = true;
		}
		else
		{
			i++;
		}
	} while (!fits && i != numtextures); // haleyjd: bug fix

	if (fits && i < numtextures) // haleyjd: bug fix
	{
		FB->DrawTextureIndirect(c[i], lnodes[wbs->epsd][n].x, lnodes[wbs->epsd][n].y);
	}
	else
	{
		// DEBUG
		DPrintf("Could not place texture on level %d", n+1);
	}
}



void WI_initAnimatedBack (void)
{
	int i;
	anim_t *a;

	if ((gameinfo.flags & GI_MAPxx) || wbs->epsd > 2)
		return;

	for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
	{
		a = &anims[wbs->epsd][i];

		// init variables
		a->ctr = -1;

		// specify the next time to draw it
		if (a->type == ANIM_ALWAYS)
			a->nexttic = bcnt + 1 + (M_Random()%a->period);
		else if (a->type == ANIM_LEVEL)
			a->nexttic = bcnt + 1;
	}

}

void WI_updateAnimatedBack (void)
{
	if ((gameinfo.flags & GI_MAPxx) || wbs->epsd > 2)
		return;

	for (int i = 0; i < NUMANIMS[wbs->epsd]; i++)
	{
		anim_t* a = &anims[wbs->epsd][i];

		if (bcnt == a->nexttic)
		{
			switch (a->type)
			{
			  case ANIM_ALWAYS:
				if (++a->ctr >= a->nanims)
					a->ctr = 0;
				a->nexttic = bcnt + a->period;
				break;

			  case ANIM_RANDOM:
				  a->ctr++;
				  if (a->ctr == a->nanims)
				  {
					  a->ctr = -1;
					  a->nexttic = bcnt+a->data2+(M_Random()%a->data1);
				  }
					  else a->nexttic = bcnt + a->period;
				  break;

			  case ANIM_LEVEL:
				// gawd-awful hack for level anims

				if (!(state == StatCount && i == 7)
					&& (WI_MapToIndex (wbs->next) + 1) == a->data1)
				{
					a->ctr++;
					if (a->ctr == a->nanims)
						a->ctr--;
					a->nexttic = bcnt + a->period;
				}

				break;
			}
		}
	}
}

void WI_drawAnimatedBack (void)
{
	int i;
	anim_t *a;

	if (gamemode != commercial && gamemode != commercial_bfg && wbs->epsd <= 2)
	{
		for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
		{
			a = &anims[wbs->epsd][i];

			if (a->ctr >= 0)
			{
				const Texture* source_texture = a->textures[a->ctr];
				int width = source_texture->getWidth();
				int height = source_texture->getHeight();

				R_CopySubimage(background_tex, source_texture,
						a->loc.x, a->loc.y,	a->loc.x + width - 1, a->loc.y + height - 1,
						0, 0, width - 1, height - 1); 
			}
		}
	}

	FB->DrawTextureFullScreen(background_tex);
}

int WI_drawNum (int n, int x, int y, int digits)
{
    int		fontwidth = num[0]->getWidth();
    int		neg;
    int		temp;

	if (digits < 0)
	{
		if (!n)
		{
			// make variable-length zeros 1 digit long
			digits = 1;
		}
		else
		{
			// figure out # of digits in #
			digits = 0;
			temp = n;

			while (temp)
			{
				temp /= 10;
				digits++;
			}
		}
	}

	// if non-number, do not draw it
	if (n == 1994)
		return 0;

	neg = n < 0;
	if (neg)
		n = -n;

	// draw the new number
	while (digits--)
	{
		x -= fontwidth;
		FB->DrawTextureClean(num[ n % 10 ], x, y);
		n /= 10;
	}

	// draw a minus sign if necessary
	if (neg)
		FB->DrawTextureClean(wiminus, x-=8, y);

    return x;
}

#include "hu_stuff.h"

void WI_drawPercent (int p, int x, int y, int b = 0)
{
	if (p < 0)
		return;

	FB->DrawTextureClean(percent, x, y);
	if (b == 0)
		WI_drawNum(p, x, y, -1);
	else
		WI_drawNum(p * 100 / b, x, y, -1);
}

void WI_drawTime (int t, int x, int y)
{
	int		div;
	int		n;

	if (t < 0)
		return;

	if (t <= 61*59)
	{
		div = 1;

		do
		{
			n = (t / div) % 60;
			x = WI_drawNum(n, x, y, 2) - colon->getWidth();
			div *= 60;

			// draw
			if (div==60 || t / div)
				FB->DrawTextureClean(colon, x, y);
		} while (t / div);
	}
	else
	{
		// "sucks"
		FB->DrawTextureClean(sucks, x - sucks->getWidth(), y);
	}
}

void WI_End (void)
{
	WI_unloadData();
}

void WI_initNoState (void)
{
	state = NoState;
	acceleratestage = 0;
	cnt = 10;
}

void WI_updateNoState (void)
{
	WI_updateAnimatedBack();

	// denis - let the server decide when to load the next map
	if(serverside)
	{
		if (!--cnt)
		{
			WI_End();
			G_WorldDone();
		}
	}
}

static BOOL snl_pointeron = false;

void WI_initShowNextLoc (void)
{
	state = ShowNextLoc;
	acceleratestage = 0;
	cnt = SHOWNEXTLOCDELAY * TICRATE;

	WI_initAnimatedBack();
}

void WI_updateShowNextLoc (void)
{
	WI_updateAnimatedBack();

	if(serverside)
	{
		if (!--cnt || acceleratestage)
			WI_initNoState();
		else
			snl_pointeron = (cnt & 31) < 20;
	}
}

void WI_drawShowNextLoc (void)
{
	int i;

	// draw animated background
	WI_drawAnimatedBack ();

	if (gamemode != commercial && gamemode != commercial_bfg )
	{
		if (wbs->epsd > 2)
		{
			WI_drawEL();
			return;
		}

		// draw a splat on taken cities.
		for (i=0; i < NUMMAPS; i++) {
			if (FindLevelInfo (names[wbs->epsd][i])->flags & LEVEL_VISITED)
				WI_drawOnLnode(i, &splat, 1);
		}

		// draw flashing ptr
		if (snl_pointeron)
			WI_drawOnLnode(WI_MapToIndex (wbs->next), yah, 2);
	}

	// draws which level you are entering..
	WI_drawEL();

}

void WI_drawNoState (void)
{
	snl_pointeron = true;
	WI_drawShowNextLoc();
}

int WI_fragSum (player_t &player)
{
	return player.fragcount;
}

void WI_drawDeathmatchStats()
{
	// draw animated background
	WI_drawAnimatedBack();
	WI_drawLF();

	// [RH] Draw heads-up scores display
	HU_DrawScores(&idplayer(me));
}

void WI_initNetgameStats()
{
	state = StatCount;
	acceleratestage = 0;
	ng_state = 1;

	cnt_pause = TICRATE;

	cnt_kills_c.clear();
	cnt_items_c.clear();
	cnt_secret_c.clear();
	cnt_frags_c.clear();

	for (Players::iterator it = players.begin();it != players.end();++it)
	{
		if (!(it->ingame()))
			continue;

		cnt_kills_c.push_back(0);
		cnt_items_c.push_back(0);
		cnt_secret_c.push_back(0);
		cnt_frags_c.push_back(0);

		dofrags += WI_fragSum(*it);
	}

	dofrags = !!dofrags;

	WI_initAnimatedBack();
}

void WI_updateNetgameStats()
{
	unsigned int i;
	int fsum;
	BOOL stillticking;

	WI_updateAnimatedBack();

	if (acceleratestage && ng_state != 10)
	{
		acceleratestage = 0;

		i = 0;
		for (Players::iterator it = players.begin();it != players.end();++it,++i)
		{
			if (!(it->ingame()))
				continue;

			cnt_kills_c[i] = plrs[i].skills;
			cnt_items_c[i] = plrs[i].sitems;
			cnt_secret_c[i] = plrs[i].ssecret;

			if (dofrags)
				cnt_frags_c[i] = WI_fragSum(*it);
		}
		S_Sound (CHAN_INTERFACE, "weapons/rocklx", 1, ATTN_NONE);
		ng_state = 10;
	}

	if (ng_state == 2)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		i = 0;
		for (Players::iterator it = players.begin();it != players.end();++it,++i)
		{
			if (!(it->ingame()))
				continue;

			cnt_kills_c[i] += 2;

			if (cnt_kills_c[i] > plrs[i].skills)
				cnt_kills_c[i] = plrs[i].skills;
			else
				stillticking = true;
		}

		if (!stillticking)
		{
			S_Sound (CHAN_INTERFACE, "weapons/rocklx", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 4)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		i = 0;
		for (Players::iterator it = players.begin();it != players.end();++it,++i)
		{
			if (!(it->ingame()))
				continue;

			cnt_items_c[i] += 2;
			if (cnt_items_c[i] > plrs[i].sitems)
				cnt_items_c[i] = plrs[i].sitems;
			else
				stillticking = true;
		}
		if (!stillticking)
		{
			S_Sound (CHAN_INTERFACE, "weapons/rocklx", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 6)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		i = 0;
		for (Players::iterator it = players.begin();it != players.end();++it,++i)
		{
			if (!(it->ingame()))
				continue;

			cnt_secret_c[i] += 2;

			if (cnt_secret_c[i] > plrs[i].ssecret)
				cnt_secret_c[i] = plrs[i].ssecret;
			else
				stillticking = true;
		}

		if (!stillticking)
		{
			S_Sound (CHAN_INTERFACE, "weapons/rocklx", 1, ATTN_NONE);
			ng_state += 1 + 2*!dofrags;
		}
	}
	else if (ng_state == 8)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		i = 0;
		for (Players::iterator it = players.begin();it != players.end();++it,++i)
		{
			if (!(it->ingame()))
				continue;

			cnt_frags_c[i] += 1;

			if (cnt_frags_c[i] >= (fsum = WI_fragSum(*it)))
				cnt_frags_c[i] = fsum;
			else
				stillticking = true;
		}

		if (!stillticking)
		{
			S_Sound (CHAN_INTERFACE, "player/male/death1", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_INTERFACE, "weapons/shotgr", 1, ATTN_NONE);
			if ( (gameinfo.flags & GI_MAPxx) )
				WI_initNoState();
			else
				WI_initShowNextLoc();
		}
	}
	else if (ng_state & 1)
	{
		if (!--cnt_pause)
		{
			ng_state++;
			cnt_pause = TICRATE;
		}
	}
}

void WI_drawNetgameStats(void)
{
	unsigned int x, y;
	short pwidth = percent->getWidth();

	// draw animated background
	WI_drawAnimatedBack();

	WI_drawLF();

	// draw stat titles (top line)
	x = NG_STATSX + 1*NG_SPACINGX - kills->getWidth();
	y = NG_STATSY;
	FB->DrawTextureClean(kills, x, y);

	x = NG_STATSX + 2*NG_SPACINGX - items->getWidth();
	FB->DrawTextureClean(items, x, y);

	x = NG_STATSX + 3*NG_SPACINGX - scrt->getWidth();
	FB->DrawTextureClean(scrt, x, y);

	if (dofrags)
	{
		x = NG_STATSX + 4*NG_SPACINGX - frags->getWidth();
		FB->DrawTextureClean(frags, x, y);
	}

	// draw stats
	y = NG_STATSY + kills->getHeight();

	for (Players::iterator it = players.begin();it != players.end();++it)
	{
		// [RH] Quick hack: Only show the first four players.
		if (it->id > 4)
			break;

		byte i = (it->id) - 1;

		if (!it->ingame())
			continue;

		x = NG_STATSX;
		// [RH] Only use one graphic for the face backgrounds
		V_ColorMap = translationref_t(translationtables + i * 256, i);
        FB->DrawTranslatedTextureClean(face_bg, x - face_bg->getWidth(), y);

		if (i == me)
			FB->DrawTextureClean(star, x - face_bg->getWidth(), y);

		x += NG_SPACINGX;
		WI_drawPercent(cnt_kills_c[i], x - pwidth, y + 10, wbs->maxkills);	x += NG_SPACINGX;
		WI_drawPercent(cnt_items_c[i], x - pwidth, y + 10, wbs->maxitems);	x += NG_SPACINGX;
		WI_drawPercent(cnt_secret_c[i], x - pwidth, y + 10, wbs->maxsecret); x += NG_SPACINGX;

		if (dofrags)
			WI_drawNum(cnt_frags_c[i], x, y + 10, -1);

		y += WI_SPACINGY;
	}
}

static int	sp_state;

void WI_initStats(void)
{
    state = StatCount;
    acceleratestage = 0;
    sp_state = 1;
    cnt_kills = cnt_items = cnt_secret = -1;
    cnt_time = cnt_par = -1;
    cnt_pause = TICRATE;

    WI_initAnimatedBack();
}

void WI_updateStats(void)
{

    WI_updateAnimatedBack();

    if (acceleratestage && sp_state != 10)
    {
	acceleratestage = 0;
	cnt_kills = (wminfo.maxkills) ? (level.killed_monsters * 100) / wminfo.maxkills : 0;
	cnt_items = (wminfo.maxitems) ? (level.found_items * 100) / wminfo.maxitems : 0;
	cnt_secret = (wminfo.maxsecret) ? (level.found_secrets * 100) / wminfo.maxsecret : 0;
	cnt_time = (plrs[me].stime) ? plrs[me].stime / TICRATE : level.time / TICRATE;
	cnt_par = wminfo.partime / TICRATE;
	S_Sound (CHAN_INTERFACE, "world/barrelx", 1, ATTN_NONE);
	sp_state = 10;
    }

    if (sp_state == 2)
    {
	cnt_kills += 2;

	if (!(bcnt&3))
	    S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

	if (!wminfo.maxkills || cnt_kills >= (level.killed_monsters * 100) / wminfo.maxkills)
	{
	    cnt_kills = (wminfo.maxkills) ? (level.killed_monsters * 100) / wminfo.maxkills : 0;
	    S_Sound (CHAN_INTERFACE, "world/barrelx", 1, ATTN_NONE);
	    sp_state++;
	}
    }
    else if (sp_state == 4)
    {
	cnt_items += 2;

	if (!(bcnt&3))
	    S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

	if (!wminfo.maxitems || cnt_items >= (level.found_items * 100) / wminfo.maxitems)
	{
	    cnt_items = (wminfo.maxitems) ? (level.found_items * 100) / wminfo.maxitems : 0;
	    S_Sound (CHAN_INTERFACE, "world/barrelx", 1, ATTN_NONE);
	    sp_state++;
	}
    }
    else if (sp_state == 6)
    {
	cnt_secret += 2;

	if (!(bcnt&3))
	    S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

	if (!wminfo.maxsecret || cnt_secret >= (level.found_secrets * 100) / wminfo.maxsecret)
	{
	    cnt_secret = (wminfo.maxsecret) ? (level.found_secrets * 100) / wminfo.maxsecret : 0;
	    S_Sound (CHAN_INTERFACE, "world/barrelx", 1, ATTN_NONE);
	    sp_state++;
	}
    }

    else if (sp_state == 8)
    {
	if (!(bcnt&3))
	    S_Sound (CHAN_INTERFACE, "weapons/pistol", 1, ATTN_NONE);

	cnt_time += 3;

	if (cnt_time >= level.time / TICRATE)
	    cnt_time = level.time / TICRATE;

	cnt_par += 3;

	if (cnt_par >= wminfo.partime / TICRATE)
	{
	    cnt_par = wminfo.partime / TICRATE;

	    if (cnt_time >= level.time / TICRATE)
	    {
		S_Sound (CHAN_INTERFACE, "world/barrelx", 1, ATTN_NONE);
		sp_state++;
	    }
	}
    }
    else if (sp_state == 10)
    {
	if (acceleratestage)
	{
	    S_Sound (CHAN_INTERFACE, "weapons/shotgr", 1, ATTN_NONE);

	    if ((gameinfo.flags & GI_MAPxx))
		WI_initNoState();
	    else
		WI_initShowNextLoc();
	}
    }
    else if (sp_state & 1)
    {
	if (!--cnt_pause)
	{
	    sp_state++;
	    cnt_pause = TICRATE;
	}
    }

}

void WI_drawStats (void)
{
    // line height
    int lh = (3 * num[0]->getHeight()) / 2;

    // draw animated background
    WI_drawAnimatedBack();
    WI_drawLF();

    FB->DrawTextureClean(kills, SP_STATSX, SP_STATSY);
    WI_drawPercent(cnt_kills, 320 - SP_STATSX, SP_STATSY);

    FB->DrawTextureClean(items, SP_STATSX, SP_STATSY+lh);
    WI_drawPercent(cnt_items, 320 - SP_STATSX, SP_STATSY+lh);

    FB->DrawTextureClean(secret, SP_STATSX, SP_STATSY+2*lh);
    WI_drawPercent(cnt_secret, 320 - SP_STATSX, SP_STATSY+2*lh);

    FB->DrawTextureClean(timepatch, SP_TIMEX, SP_TIMEY);
    WI_drawTime(cnt_time, 160 - SP_TIMEX, SP_TIMEY);

	if ((gameinfo.flags & GI_MAPxx) || wbs->epsd < 3)
    {
    	FB->DrawTextureClean(par, SP_TIMEX + 160, SP_TIMEY);
    	WI_drawTime(cnt_par, 320 - SP_TIMEX, SP_TIMEY);
    }

}

void WI_checkForAccelerate(void)
{
	if (!serverside)
		return;

	// check for button presses to skip delays
	for (Players::iterator it = players.begin();it != players.end();++it)
	{
		if (it->ingame())
		{
			player_t *player = &*it;

			if (player->cmd.buttons & BT_ATTACK)
			{
				if (!player->attackdown)
					acceleratestage = 1;
				player->attackdown = true;
			}
			else
				player->attackdown = false;
			if (player->cmd.buttons & BT_USE)
			{
				if (!player->usedown)
					acceleratestage = 1;
				player->usedown = true;
			}
			else
				player->usedown = false;
		}
	}
}

// Updates stuff each tick
void WI_Ticker (void)
{
	// counter for general background animation
	bcnt++;

	if (bcnt == 1)
	{
		// intermission music
		if ((gameinfo.flags & GI_MAPxx))
			S_ChangeMusic ("d_dm2int", true);
		else
			S_ChangeMusic ("d_inter", true);
	}

    WI_checkForAccelerate();

	switch (state)
	{
		case StatCount:
			if (multiplayer && sv_maxplayers > 1)
			{
				if (sv_gametype == 0 && !wi_newintermission && sv_maxplayers < 5)
					WI_updateNetgameStats();
				else
					WI_updateNoState();
			}
			else
				WI_updateStats();
			break;

		case ShowNextLoc:
			WI_updateShowNextLoc();
			break;

		case NoState:
			WI_updateNoState();
			break;
	}
}


//
// WI_LoadSprite
//
// Loads an intermission sprite and prevents it from being freed when the
// Zone memory manager is low on memory.
// 
static const Texture* WI_LoadSprite(const char* name)
{
	texhandle_t texhandle = texturemanager.getHandle(name, Texture::TEX_SPRITE);
	const Texture* texture = texturemanager.getTexture(texhandle);
	Z_ChangeTag(texture, PU_STATIC);
	return texture;
}

//
// WI_UnloadSprite
//
// Allows an intermission sprite to be freed by the Zone memory manager
//
static void WI_UnloadSprite(const Texture* texture)
{
	if (texture)
		Z_ChangeTag(texture, PU_CACHE);
}


void WI_loadData (void)
{
	wi_font = new LargeDoomFont(CleanXfac * FRACUNIT);

	char name[9];

	if ((gameinfo.flags & GI_MAPxx) ||
		((gameinfo.flags & GI_MENUHACK_RETAIL) && wbs->epsd >= 3))
		strcpy(name, "INTERPIC");
	else
		sprintf(name, "WIMAP%d", wbs->epsd);

	// background
	// TODO: don't cast away const!
	background_tex = (Texture*)WI_LoadSprite(name);

	for (int i = 0; i < 2; i++)
	{
		const char* lname = (i == 0 ? wbs->lname0 : wbs->lname1);
		lnames[i] = WI_LoadSprite(lname);
	}

	if (gamemode != commercial && gamemode != commercial_bfg)
	{
		// you are here
		yah[0] = WI_LoadSprite("WIURHO");

		// you are here (alt.)
		yah[1] = WI_LoadSprite("WIURH1");

		// splat
		splat = WI_LoadSprite("WISPLAT");

		if (wbs->epsd < 3)
		{
			for (int j = 0; j < NUMANIMS[wbs->epsd]; j++)
			{
				anim_t* a = &anims[wbs->epsd][j];
				for (int i = 0; i < a->nanims; i++)
				{
					// MONDO HACK!
					if (wbs->epsd != 1 || j != 8)
					{
						// animations
						sprintf(name, "WIA%d%.2d%.2d", wbs->epsd, j, i);
						a->textures[i] = WI_LoadSprite(name);
					}
					else
					{
						// HACK ALERT!
						a->textures[i] = anims[1][4].textures[i];
					}
				}
			}
		}
	}

	// numbers 0-9
	for (int i = 0; i < 10; i++)
    {
		sprintf(name, "WINUM%d", i);
		num[i] = WI_LoadSprite(name);
    }

	// "-"
    wiminus = WI_LoadSprite("WIMINUS");

	// "%"
    percent = WI_LoadSprite("WIPCNT");

	// ":"
    colon = WI_LoadSprite("WICOLON");

	// "finished"
	finished = WI_LoadSprite("WIF"); // (Removed) Dan - Causes GUI Issues |FIX-ME|

	// "entering"
	entering = WI_LoadSprite("WIENTER");

	// "kills"
    kills = WI_LoadSprite("WIOSTK");

	// "items"
    items = WI_LoadSprite("WIOSTI");

    // "scrt"
    scrt = WI_LoadSprite("WIOSTS");

	// "secret"
    secret = WI_LoadSprite("WISCRT2");

	// "frgs"
	frags = WI_LoadSprite("WIFRGS");

	// "time"
    timepatch = WI_LoadSprite("WITIME");

    // "sucks"
    sucks =WI_LoadSprite("WISUCKS");

    // "par"
    par = WI_LoadSprite("WIPAR");

	// "total"
	total = WI_LoadSprite("WIMSTT");

	// your face
	star = WI_LoadSprite("STFST01");

	// dead face
	bstar = WI_LoadSprite("STFDEAD0");

	// background for player face (translatable)
	face_bg = WI_LoadSprite("STPBANY");
}

void WI_unloadData (void)
{
	delete wi_font;
	wi_font = NULL;

	WI_UnloadSprite(background_tex);

	WI_UnloadSprite(yah[0]);
	WI_UnloadSprite(yah[1]);
	WI_UnloadSprite(splat);
	if (wbs->epsd < 3)
	{
		for (int j = 0; j < NUMANIMS[wbs->epsd]; j++)
		{
			anim_t* a = &anims[wbs->epsd][j];
			for (int i = 0; i < a->nanims; i++)
				WI_UnloadSprite(a->textures[i]);
		}
	}

	for (int i = 0; i < 10; i++)
		WI_UnloadSprite(num[i]);

	WI_UnloadSprite(wiminus);
	WI_UnloadSprite(percent);
	WI_UnloadSprite(colon);
	WI_UnloadSprite(finished);
	WI_UnloadSprite(entering);
	WI_UnloadSprite(kills);
	WI_UnloadSprite(items);
	WI_UnloadSprite(scrt);
	WI_UnloadSprite(secret);
	WI_UnloadSprite(frags);
	WI_UnloadSprite(timepatch);
	WI_UnloadSprite(sucks);
	WI_UnloadSprite(par);
	WI_UnloadSprite(total);
	WI_UnloadSprite(star);
	WI_UnloadSprite(bstar);

	WI_UnloadSprite(face_bg);
}

void WI_Drawer (void)
{
	// If the background texture has been freed, then we really shouldn't
	// be in here. (But it happens anyway.)
	if (background_tex)
	{
		switch (state)
		{
		case StatCount:
			if (multiplayer && sv_maxplayers > 1)
			{
				if (sv_gametype == 0 && !wi_newintermission && sv_maxplayers < 5)
					WI_drawNetgameStats();
				else
					WI_drawDeathmatchStats();
			}
			else
				WI_drawStats();
			break;

		case ShowNextLoc:
			WI_drawShowNextLoc();
			break;

		default:
			WI_drawNoState();
			break;
		}
	}
}


void WI_initVariables (wbstartstruct_t *wbstartstruct)
{
	wbs = wbstartstruct;

	acceleratestage = 0;
	cnt = bcnt = 0;
	me = wbs->pnum;
	plrs = wbs->plyr;
}

void WI_Start (wbstartstruct_t *wbstartstruct)
{
	WI_initVariables (wbstartstruct);
	WI_loadData ();
	WI_initStats();
	WI_initNetgameStats();

	V_SetBlend (0,0,0,0);
	S_StopAllChannels ();
 	SN_StopAllSequences ();
}

VERSION_CONTROL (wi_stuff_cpp, "$Id$")


