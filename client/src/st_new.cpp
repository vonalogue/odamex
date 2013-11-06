// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1998-2006 by Randy Heit (ZDoom 1.22).
// Copyright (C) 2000-2006 by Sergey Makovkin (CSDoom .62).
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
//	Additional status bar stuff
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "cl_demo.h"
#include "d_items.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "z_zone.h"
#include "i_system.h"
#include "m_swap.h"
#include "st_stuff.h"
#include "hu_drawers.h"
#include "hu_elements.h"
#include "hu_stuff.h"
#include "c_cvars.h"
#include "p_ctf.h"
#include "cl_vote.h"

static int		widestnum, numheight;

static texhandle_t		medi;
static texhandle_t		armors[2];
static texhandle_t		ammos[4];
static texhandle_t		bigammos[4];
static texhandle_t		flagiconteam;
static texhandle_t		flagiconbhome;
static texhandle_t		flagiconrhome;
static texhandle_t		flagiconbtakenbyb;
static texhandle_t		flagiconbtakenbyr;
static texhandle_t		flagiconrtakenbyb;
static texhandle_t		flagiconrtakenbyr;
static texhandle_t		flagicongtakenbyb;
static texhandle_t		flagicongtakenbyr;
static texhandle_t		flagiconbdropped;
static texhandle_t		flagiconrdropped;
static texhandle_t		line_leftempty;
static texhandle_t		line_leftfull;
static texhandle_t		line_centerempty;
static texhandle_t		line_centerleft;
static texhandle_t		line_centerright;
static texhandle_t		line_centerfull;
static texhandle_t		line_rightempty;
static texhandle_t		line_rightfull;
static int		NameUp = -1;

extern patch_t	*sttminus;
extern patch_t	*tallnum[10];
extern patch_t	*faces[];
extern int		st_faceindex;
extern patch_t	*keys[NUMCARDS+NUMCARDS/2];
extern byte		*Ranges;
extern flagdata CTFdata[NUMFLAGS];

extern NetDemo netdemo;

int V_TextScaleXAmount();
int V_TextScaleYAmount();

EXTERN_CVAR (hud_scale)
EXTERN_CVAR (hud_timer)
EXTERN_CVAR (hud_targetcount)
EXTERN_CVAR (st_scale)
EXTERN_CVAR (sv_fraglimit)

//
// ST_LoadSprite
//
// Loads a status bar sprite and prevents it from being freed when the
// Zone memory manager is low on memory.
// 
static texhandle_t ST_LoadSprite(const char* name)
{
	texhandle_t texhandle = texturemanager.getHandle(name, Texture::TEX_SPRITE);
	if (texhandle != TextureManager::NOT_FOUND_TEXTURE_HANDLE)
	{
		const Texture* texture = texturemanager.getTexture(texhandle);
		Z_ChangeTag(texture, PU_STATIC);
	}
	return texhandle;
}

//
// ST_UnloadSprite
//
static void ST_UnloadSprite(texhandle_t texhandle)
{
	if (texhandle == TextureManager::NOT_FOUND_TEXTURE_HANDLE ||
		texhandle == TextureManager::NO_TEXTURE_HANDLE)
		return;

	const Texture* texture = texturemanager.getTexture(texhandle);
	Z_ChangeTag(texture, PU_CACHE);
}

void ST_unloadNew (void)
{
	ST_UnloadSprite(medi);

	ST_UnloadSprite(flagiconteam);
	ST_UnloadSprite(flagiconbhome);
	ST_UnloadSprite(flagiconrhome);
	ST_UnloadSprite(flagiconbtakenbyb);
	ST_UnloadSprite(flagiconbtakenbyr);
	ST_UnloadSprite(flagiconrtakenbyb);
	ST_UnloadSprite(flagiconrtakenbyr);
	ST_UnloadSprite(flagicongtakenbyb);
	ST_UnloadSprite(flagicongtakenbyr);
	ST_UnloadSprite(flagiconbdropped);
	ST_UnloadSprite(flagiconrdropped);

	ST_UnloadSprite(line_leftempty);
	ST_UnloadSprite(line_leftfull);
	ST_UnloadSprite(line_centerempty);
	ST_UnloadSprite(line_centerleft);
	ST_UnloadSprite(line_centerright);
	ST_UnloadSprite(line_centerfull);
	ST_UnloadSprite(line_rightempty);
	ST_UnloadSprite(line_rightfull);

	for (int i = 0; i < 2; i++)
		ST_UnloadSprite(armors[i]);

	for (int i = 0; i < 4; i++)
	{
		ST_UnloadSprite(ammos[i]);
		ST_UnloadSprite(bigammos[i]);
	}
}


void ST_initNew (void)
{
	int widest = 0;

	// denis - todo - security - these patches have unchecked dimensions
	// ie, if a patch has a 0 width/height, it may cause a divide by zero
	// somewhere else in the code. we download wads, so this is an issue!

	for (int i = 0; i < 10; i++) {
		if (tallnum[i]->width() > widest)
			widest = tallnum[i]->width();
	}

	medi = ST_LoadSprite("MEDIA0");

	armors[0] = ST_LoadSprite("ARM1A0");
	armors[1] = ST_LoadSprite("ARM2A0");

	ammos[0] = ST_LoadSprite("CLIPA0");
	ammos[1] = ST_LoadSprite("SHELA0");
	ammos[2] = ST_LoadSprite("CELLA0");
	ammos[3] = ST_LoadSprite("ROCKA0");

	bigammos[0] = ST_LoadSprite("AMMOA0");
	bigammos[1] = ST_LoadSprite("SBOXA0");
	bigammos[2] = ST_LoadSprite("CELPA0");
	bigammos[3] = ST_LoadSprite("BROKA0");
	
	flagiconteam		= ST_LoadSprite("FLAGIT");
	flagiconbhome		= ST_LoadSprite("FLAGIC2B");
	flagiconrhome		= ST_LoadSprite("FLAGIC2R");
	flagiconbtakenbyb	= ST_LoadSprite("FLAGI3BB");
	flagiconbtakenbyr	= ST_LoadSprite("FLAGI3BR");
	flagiconrtakenbyb	= ST_LoadSprite("FLAGI3RB");
	flagiconrtakenbyr	= ST_LoadSprite("FLAGI3RR");
	flagiconbdropped	= ST_LoadSprite("FLAGIC4B");
	flagiconrdropped	= ST_LoadSprite("FLAGIC4R");

	widestnum = widest;
	numheight = tallnum[0]->height();

	if (multiplayer && (sv_gametype == GM_COOP || demoplayback || !netgame) && level.time)
		NameUp = level.time + 2*TICRATE;

	line_leftempty		= ST_LoadSprite("ODABARLE");
	line_leftfull		= ST_LoadSprite("ODABARLF");
	line_centerempty	= ST_LoadSprite("ODABARCE");
	line_centerleft		= ST_LoadSprite("ODABARCL");
	line_centerright	= ST_LoadSprite("ODABARCR");
	line_centerfull		= ST_LoadSprite("ODABARCF");
	line_rightempty		= ST_LoadSprite("ODABARRE");
	line_rightfull		= ST_LoadSprite("ODABARRF");
}

void ST_DrawNum (int x, int y, DCanvas *scrn, int num)
{
	char digits[8], *d;

	if (num < 0)
	{
		if (hud_scale)
		{
			scrn->DrawLucentPatchCleanNoMove (sttminus, x, y);
			x += CleanXfac * sttminus->width();
		}
		else
		{
			scrn->DrawLucentPatch (sttminus, x, y);
			x += sttminus->width();
		}
		num = -num;
	}

	sprintf (digits, "%d", num);

	d = digits;
	while (*d)
	{
		if (*d >= '0' && *d <= '9')
		{
			if (hud_scale)
			{
				scrn->DrawLucentPatchCleanNoMove (tallnum[*d - '0'], x, y);
				x += CleanXfac * tallnum[*d - '0']->width();
			}
			else
			{
				scrn->DrawLucentPatch (tallnum[*d - '0'], x, y);
				x += tallnum[*d - '0']->width();
			}
		}
		d++;
	}
}

void ST_DrawNumRight (int x, int y, DCanvas *scrn, int num)
{
	int d = abs(num);
	int xscale = hud_scale ? CleanXfac : 1;

	do {
		x -= tallnum[d%10]->width() * xscale;
	} while (d /= 10);

	if (num < 0)
		x -= sttminus->width() * xscale;

	ST_DrawNum (x, y, scrn, num);
}

/**
 * Draw a bar on the screen.
 *
 * @param normalcolor Bar color.  Uses text colors (i.e. CR_RED).
 * @param value Value of the bar.
 * @param total Maximum value of the bar.
 * @param x Unscaled leftmost X position to draw from.
 * @param y Unscaled topmost Y position to draw from.
 * @param width Width of the bar in unscaled pixels.
 * @param reverse If true, the bar is drawn with the 'baseline' on the right.
 * @param cutleft True if you want the left end of the bar to not have a cap.
 * @param cutright True if you want the right end of the bar to not have a cap.
 */
void ST_DrawBar (int normalcolor, unsigned int value, unsigned int total,
				 int x, int y, int width, bool reverse = false,
				 bool cutleft = false, bool cutright = false) {
	int xscale = hud_scale ? CleanXfac : 1;

	if (normalcolor > NUM_TEXT_COLORS || normalcolor == CR_GREY) {
		normalcolor = CR_RED;
	}

	if (width < (4 * xscale)) {
		width = 4 * xscale;
	}
	width -= (width % (2 * xscale));

	int bar_width = width / (2 * xscale);

	int bar_filled;
	if (value == 0)				// Bar is forced empty.
		bar_filled = 0;
	else if (value >= total)	// Bar is forced full.
		bar_filled = bar_width;
	else
	{
		bar_filled = (value * bar_width) / total;
		if (bar_filled == 0)	// Bar is prevented from being empty.
			bar_filled = 1;
		else if (bar_filled >= bar_width)	// Bar is prevented from being full.
			bar_filled = bar_width - 1;
	}

	V_ColorMap = translationref_t(Ranges + normalcolor * 256);
	for (int i = 0; i < bar_width; i++)
	{
		texhandle_t line_texhandle;

		if (!reverse)
		{
			if (i == 0 && !cutleft)
			{
				if (bar_filled == 0)
					line_texhandle = line_leftempty;
				else
					line_texhandle = line_leftfull;
			}
			else if (i == bar_width - 1 && !cutright)
			{
				if (bar_filled == bar_width)
					line_texhandle = line_rightfull;
				else
					line_texhandle = line_rightempty;
			}
			else
			{
				if (i == bar_filled - 1)
					line_texhandle = line_centerleft;
				else if (i < bar_filled)
					line_texhandle = line_centerfull;
				else
					line_texhandle = line_centerempty;
			}
		}
		else
		{
			if (i == 0 && !cutleft)
			{
				if (bar_filled == bar_width)
					line_texhandle = line_leftfull;
				else
					line_texhandle = line_leftempty;
			}
			else if (i == bar_width - 1 && !cutright)
			{
				if (bar_filled == 0)
					line_texhandle = line_rightempty;
				else
					line_texhandle = line_rightfull;
			}
			else
			{
				if (i == (bar_width - bar_filled))
					line_texhandle = line_centerright;
				else if (i >= (bar_width - bar_filled))
					line_texhandle = line_centerfull;
				else
					line_texhandle = line_centerempty;
			}
		}

		const Texture* line_texture = texturemanager.getTexture(line_texhandle);

		int xi = x + (i * xscale * 2);
		if (hud_scale)
			screen->DrawTranslatedTextureCleanNoMove(line_texture, xi, y);
		else
			screen->DrawTranslatedTexture(line_texture, xi, y);
	}
}

// [AM] Draw the state of voting
void ST_voteDraw (int y) {
	vote_state_t vote_state;
	if (!VoteState::instance().get(vote_state)) {
		return;
	}

	int xscale = hud_scale ? CleanXfac : 1;
	int yscale = hud_scale ? CleanYfac : 1;

	// Vote Result/Countdown
	std::ostringstream buffer;
	std::string result_string;
	EColorRange result_color;

	switch (vote_state.result) {
	case VOTE_YES:
		result_string = "VOTE PASSED";
		result_color = CR_GREEN;
		break;
	case VOTE_NO:
		result_string = "VOTE FAILED";
		result_color = CR_RED;
		break;
	case VOTE_INTERRUPT:
		result_string = "VOTE INTERRUPTED";
		result_color = CR_TAN;
		break;
	case VOTE_ABANDON:
		result_string = "VOTE ABANDONED";
		result_color = CR_TAN;
		break;
	case VOTE_UNDEC:
		buffer << "VOTE NOW: " << vote_state.countdown;
		result_string = buffer.str();
		if (vote_state.countdown <= 5 && (I_MSTime() % 1000) < 500) {
			result_color = CR_BRICK;
		} else {
			result_color = CR_GOLD;
		}
		break;
	default:
		return;
	}

	size_t x1, x2;
	x1 = (screen->width - V_StringWidth(result_string.c_str()) * xscale) >> 1;
	if (hud_scale) {
		screen->DrawTextClean(result_color, x1, y, result_string.c_str());
	} else {
		screen->DrawText(result_color, x1, y, result_string.c_str());
	}

	// Votestring - Break lines
	brokenlines_t *votestring = V_BreakLines(320, vote_state.votestring.c_str());
	for (byte i = 0;i < 4;i++) {
		if (votestring[i].width == -1) {
			break;
		}

		x2 = (screen->width - votestring[i].width * xscale) >> 1;
		y += yscale * 8;

		if (hud_scale) {
			screen->DrawTextClean(CR_GREY, x2, y, votestring[i].string);
		} else {
			screen->DrawText(CR_GREY, x2, y, votestring[i].string);
		}
	}
	V_FreeBrokenLines(votestring);

	if (vote_state.result == VOTE_ABANDON) {
		return;
	}

	// Voting Bar
	y += yscale * 8;

	ST_DrawBar(CR_RED, vote_state.no, vote_state.no_needed,
			   (screen->width >> 1) - xscale * 40, y, xscale * 40,
			   true, false, true);
	ST_DrawBar(CR_GREEN, vote_state.yes, vote_state.yes_needed,
			   (screen->width >> 1), y, xscale * 40, false, true);
}

namespace hud {

// [AM] Draw CTF scoreboard
void drawCTF() {
	if (sv_gametype != GM_CTF) {
		return;
	}

	player_t *plyr = &consoleplayer();
	int xscale = hud_scale ? CleanXfac : 1;
	int yscale = hud_scale ? CleanYfac : 1;

	texhandle_t flagblue_texhandle = flagiconbhome;
	texhandle_t flagred_texhandle = flagiconrhome;

	switch (CTFdata[it_blueflag].state)
	{
		case flag_carried:
			if (CTFdata[it_blueflag].flagger)
			{
				player_t &player = idplayer(CTFdata[it_blueflag].flagger);
				if (player.userinfo.team == TEAM_BLUE)
					flagblue_texhandle = flagiconbtakenbyb;
				else if (player.userinfo.team == TEAM_RED)
					flagblue_texhandle = flagiconbtakenbyr;
			}
			break;
		case flag_dropped:
			flagblue_texhandle = flagiconbdropped;
			break;
		default:
			break;
	}

	switch (CTFdata[it_redflag].state)
	{
		case flag_carried:
			if (CTFdata[it_redflag].flagger)
			{
				player_t &player = idplayer(CTFdata[it_redflag].flagger);
				if (player.userinfo.team == TEAM_BLUE)
					flagred_texhandle = flagiconrtakenbyb;
				else if (player.userinfo.team == TEAM_RED)
					flagred_texhandle = flagiconrtakenbyr;
			}
			break;
		case flag_dropped:
			flagred_texhandle = flagiconrdropped;
			break;
		default:
			break;
	}

	// Draw base flag patches
	const Texture* flagblue_texture = texturemanager.getTexture(flagblue_texhandle);
	const Texture* flagred_texture = texturemanager.getTexture(flagred_texhandle);

	hud::DrawTexture(4, 61, hud_scale,
	               hud::X_RIGHT, hud::Y_BOTTOM,
	               hud::X_RIGHT, hud::Y_BOTTOM,
	               flagblue_texture);
	hud::DrawTexture(4, 43, hud_scale,
	               hud::X_RIGHT, hud::Y_BOTTOM,
	               hud::X_RIGHT, hud::Y_BOTTOM,
	               flagred_texture);

	// Draw team border
	const Texture* flagborder_texture = texturemanager.getTexture(flagiconteam);

	switch (plyr->userinfo.team)
	{
		case TEAM_BLUE:
			hud::DrawTexture(4, 61, hud_scale,
			               hud::X_RIGHT, hud::Y_BOTTOM,
			               hud::X_RIGHT, hud::Y_BOTTOM,
			               flagborder_texture);
			break;
		case TEAM_RED:
			hud::DrawTexture(4, 43, hud_scale,
			               hud::X_RIGHT, hud::Y_BOTTOM,
			               hud::X_RIGHT, hud::Y_BOTTOM,
			               flagborder_texture);
			break;
		default:
			break;
	}

	// Draw team scores
	ST_DrawNumRight(screen->width - 24 * xscale, screen->height - (62 + 16) * yscale,
	                screen, TEAMpoints[TEAM_BLUE]);
	ST_DrawNumRight(screen->width - 24 * xscale, screen->height - (44 + 16) * yscale,
	                screen, TEAMpoints[TEAM_RED]);
}

// [AM] Draw netdemo state
// TODO: This is ripe for commonizing, but I _need_ to get this done soon.
void drawNetdemo() {
	if (!(netdemo.isPlaying() || netdemo.isPaused())) {
		return;
	}

	int xscale = hud_scale ? CleanXfac : 1;
	int yscale = hud_scale ? CleanYfac : 1;

	// Draw demo elapsed time
	hud::DrawText(2, 47, hud_scale,
	              hud::X_LEFT, hud::Y_BOTTOM,
	              hud::X_LEFT, hud::Y_BOTTOM,
	              hud::NetdemoElapsed().c_str(), CR_GREY);

	// Draw map number/total
	hud::DrawText(74, 47, hud_scale,
	              hud::X_LEFT, hud::Y_BOTTOM,
	              hud::X_RIGHT, hud::Y_BOTTOM,
	              hud::NetdemoMaps().c_str(), CR_BRICK);

	// Draw the bar.
	// TODO: Once status bar notches have been implemented, put map
	//       change times in as notches.
	int color = CR_GOLD;
	if (netdemo.isPlaying()) {
		color = CR_GREEN;
	}
	ST_DrawBar(color, netdemo.calculateTimeElapsed(), netdemo.calculateTotalTime(),
	           2 * xscale, screen->height - 46 * yscale, 72 * xscale);
}

// [ML] 9/29/2011: New fullscreen HUD, based on Ralphis's work
void OdamexHUD() {
	player_t *plyr = &displayplayer();

	// TODO: I can probably get rid of these invocations once I put a
	//       copy of ST_DrawNumRight into the hud namespace. -AM
	unsigned int y, xscale, yscale;
	xscale = hud_scale ? CleanXfac : 1;
	yscale = hud_scale ? CleanYfac : 1;
	y = screen->height - (numheight + 4) * yscale;

	// Draw Armor if the player has any
	if (plyr->armortype && plyr->armorpoints)
	{
		texhandle_t armor_texhandle = armors[plyr->armortype - 1];
		const Texture* armor_texture = texturemanager.getTexture(armor_texhandle);

		// Draw Armor type.  Vertically centered against armor number.
		hud::DrawTextureScaled(48 + 2 + 10, 32, 20, 20, hud_scale,
			                     hud::X_LEFT, hud::Y_BOTTOM,
			                     hud::X_CENTER, hud::Y_MIDDLE,
			                     armor_texture);
		ST_DrawNumRight(48 * xscale, y - 20 * yscale, screen, plyr->armorpoints);
	}

	// Draw Doomguy.  Vertically scaled to an area two pixels above and
	// below the health number, and horizontally centered below the armor.
	hud::DrawPatchScaled(48 + 2 + 10, 2, 20, 20, hud_scale,
	                     hud::X_LEFT, hud::Y_BOTTOM,
	                     hud::X_CENTER, hud::Y_BOTTOM,
	                     faces[st_faceindex]);
	ST_DrawNumRight(48 * xscale, y, screen, plyr->health);

	// Draw Ammo
	ammotype_t ammotype = weaponinfo[plyr->readyweapon].ammotype;
	if (ammotype < NUMAMMO)
	{
		// Use big ammo if the player has a backpack.
		texhandle_t ammo_texhandle = plyr->backpack ? bigammos[ammotype] : ammos[ammotype];
		const Texture* ammo_texture = texturemanager.getTexture(ammo_texhandle);

		// Draw ammo.  We have a 16x16 box to the right of the ammo where the
		// ammo type is drawn.
		// TODO: This "scale only if bigger than bounding box" can
		//       probably be commonized, along with "scale only if
		//       smaller than bounding box".
		if (ammo_texture->getWidth() > 16 || ammo_texture->getHeight() > 16)
		{
			hud::DrawTextureScaled(12, 12, 16, 16, hud_scale,
			                     hud::X_RIGHT, hud::Y_BOTTOM,
			                     hud::X_CENTER, hud::Y_MIDDLE,
			                     ammo_texture);
		}
		else
		{
			hud::DrawTexture(12, 12, hud_scale,
			               hud::X_RIGHT, hud::Y_BOTTOM,
			               hud::X_CENTER, hud::Y_MIDDLE,
			               ammo_texture);
		}
		ST_DrawNumRight(screen->width - 24 * xscale, y, screen, plyr->ammo[ammotype]);
	}

	int color;
	std::string str;

	// Draw warmup state or timer
	str = hud::Warmup(color);
	if (!str.empty()) {
		hud::DrawText(0, 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	} else if (hud_timer) {
		str = hud::Timer(color);
		hud::DrawText(0, 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	}

	// Draw other player name, if spying
	str = hud::SpyPlayerName(color);
	hud::DrawText(0, 12, hud_scale,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              str.c_str(), color);

	// Draw targeted player names.
	hud::EATargets(0, 20, hud_scale,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               1, hud_targetcount);

	// Draw stat lines.  Vertically aligned with the bottom of the armor
	// number on the other side of the screen.
	str = hud::PersonalSpread(color);
	hud::DrawText(4, 32, hud_scale,
	              hud::X_RIGHT, hud::Y_BOTTOM,
	              hud::X_RIGHT, hud::Y_BOTTOM,
	              str.c_str(), color);
	str = hud::PersonalScore(color);
	hud::DrawText(4, 24, hud_scale,
	              hud::X_RIGHT, hud::Y_BOTTOM,
	              hud::X_RIGHT, hud::Y_BOTTOM,
	              str.c_str(), color);

	// Draw keys in coop
	if (sv_gametype == GM_COOP) {
		for (byte i = 0;i < NUMCARDS;i++) {
			if (plyr->cards[i]) {
				hud::DrawPatch(4 + (i * 10), 24, hud_scale,
				               hud::X_RIGHT, hud::Y_BOTTOM,
				               hud::X_RIGHT, hud::Y_BOTTOM,
				               keys[i]);
			}
		}
	}

	// Draw CTF scoreboard
	hud::drawCTF();

	// Draw Netdemo info
	hud::drawNetdemo();
}

// [AM] Spectator HUD.
void SpectatorHUD() {
	int color;
	std::string str;

	// Draw warmup state or timer
	str = hud::Warmup(color);
	if (!str.empty()) {
		hud::DrawText(0, 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	} else if (hud_timer) {
		str = hud::Timer(color);
		hud::DrawText(0, 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	}

	// Draw other player name, if spying
	str = hud::SpyPlayerName(color);
	hud::DrawText(0, 12, hud_scale,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              str.c_str(), color);

	// Draw help text if there's no other player name
	if (str.empty()) {
		hud::DrawText(0, 12, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::HelpText().c_str(), CR_GREY);
	}

	// Draw targeted player names.
	hud::EATargets(0, 20, hud_scale,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               1, 0);

	// Draw CTF scoreboard
	hud::drawCTF();

	// Draw Netdemo info
	hud::drawNetdemo();
}

// [AM] Original ZDoom HUD
void ZDoomHUD() {
	player_t *plyr = &displayplayer();
	int y, i;
	ammotype_t ammotype = weaponinfo[plyr->readyweapon].ammotype;
	int xscale = hud_scale ? CleanXfac : 1;
	int yscale = hud_scale ? CleanYfac : 1;

	y = screen->height - (numheight + 4) * yscale;

	// Draw health
	const Texture* medi_texture = texturemanager.getTexture(medi);
	if (hud_scale)
		screen->DrawLucentTextureCleanNoMove(medi_texture, 20 * CleanXfac, screen->height - 2*CleanYfac);
	else
		screen->DrawLucentTexture(medi_texture, 20, screen->height - 2);
	ST_DrawNum (40 * xscale, y, screen, plyr->health);

	// Draw armor
	if (plyr->armortype && plyr->armorpoints)
	{
		texhandle_t armor_texhandle = armors[plyr->armortype - 1];
		const Texture* armor_texture = texturemanager.getTexture(armor_texhandle);

		if (hud_scale)
			screen->DrawLucentTextureCleanNoMove(armor_texture, 20 * CleanXfac, y - 4*CleanYfac);
		else
			screen->DrawLucentTexture(armor_texture, 20, y - 4);

		int texheight = armor_texture->getHeight();
		ST_DrawNum(40 * xscale, y - (texheight + 3) * yscale, screen, plyr->armorpoints);
	}

	// Draw ammo
	if (ammotype < NUMAMMO)
	{
		texhandle_t ammo_texhandle = ammos[weaponinfo[plyr->readyweapon].ammotype];
		const Texture* ammo_texture = texturemanager.getTexture(ammo_texhandle);

		if (hud_scale)
			screen->DrawLucentTextureCleanNoMove(ammo_texture, screen->width - 14 * CleanXfac, screen->height - 4 * CleanYfac);
		else
			screen->DrawLucentTexture(ammo_texture, screen->width - 14, screen->height - 4);
		ST_DrawNumRight (screen->width - 25 * xscale, y, screen, plyr->ammo[ammotype]);
	}

	// Draw top-right info. (Keys/Frags/Score)
    if (sv_gametype == GM_CTF)
    {
		hud::drawCTF();
    }
	else if (sv_gametype != GM_COOP)
	{
		// Draw frags (in DM)
		ST_DrawNumRight (screen->width - (2 * xscale), 2 * yscale, screen, plyr->fragcount);
	}
	else
	{
		// Draw keys (not DM)
		y = CleanYfac;
		for (i = 0; i < 6; i++)
		{
			if (plyr->cards[i])
			{
				if (hud_scale)
					screen->DrawLucentPatchCleanNoMove (keys[i], screen->width - 10*CleanXfac, y);
				else
					screen->DrawLucentPatch (keys[i], screen->width - 10, y);
				y += (8 + (i < 3 ? 0 : 2)) * yscale;
			}
		}
	}

	int color;
	std::string str;

	// Draw warmup state or timer
	str = hud::Warmup(color);
	if (!str.empty()) {
		hud::DrawText(0, 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	} else if (hud_timer) {
		str = hud::Timer(color);
		hud::DrawText(0, 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	}

	// Draw other player name, if spying
	str = hud::SpyPlayerName(color);
	hud::DrawText(0, 12, hud_scale,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              str.c_str(), color);

	// Draw targeted player names.
	hud::EATargets(0, 20, hud_scale,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               1, 0);

	// Draw Netdemo info
	hud::drawNetdemo();
}

// [AM] HUD drawn with the Doom Status Bar.
void DoomHUD() {
	int color;
	std::string str;

	// ST_Y is the number of pixels of viewable space, taking into account the
	// status bar.  We need to convert this into scaled pixels as best we can.
	int st_y;
	if (hud_scale) {
		st_y = (screen->height - ST_Y) / CleanYfac;
	} else {
		st_y = screen->height - ST_Y;
	}

	// Draw warmup state or timer
	str = hud::Warmup(color);
	if (!str.empty()) {
		hud::DrawText(0, st_y + 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	} else if (hud_timer) {
		str = hud::Timer(color);
		hud::DrawText(0, st_y + 4, hud_scale,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              hud::X_CENTER, hud::Y_BOTTOM,
		              str.c_str(), color);
	}

	// Draw other player name, if spying
	str = hud::SpyPlayerName(color);
	hud::DrawText(0, st_y + 12, hud_scale,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              hud::X_CENTER, hud::Y_BOTTOM,
	              str.c_str(), color);

	// Draw targeted player names.
	hud::EATargets(0, st_y + 20, hud_scale,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               hud::X_CENTER, hud::Y_BOTTOM,
	               1, hud_targetcount);

	// Draw Netdemo info
	hud::drawNetdemo();
}

}

VERSION_CONTROL (st_new_cpp, "$Id$")
