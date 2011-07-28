// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2010 by The Odamex Team.
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
//		Player related stuff.
//		Bobbing POV/weapon, movement.
//		Pending weapon.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"
#include "s_sound.h"
#include "i_system.h"
#include "i_net.h"
#include <algorithm>

// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP 		32

//
// Movement.
//

// 16 pixels of bob
#define MAXBOB			0x100000

EXTERN_CVAR (sv_allowjump)
EXTERN_CVAR (cl_mouselook)
EXTERN_CVAR (sv_freelook)
EXTERN_CVAR (co_zdoomphys)
EXTERN_CVAR (cl_deathcam)
EXTERN_CVAR (sv_antiwallhack)

extern bool predicting, step_mode;

//
// P_Thrust
// Moves the given origin along a given angle.
//
void P_SideThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle = (angle - ANG90) >> ANGLETOFINESHIFT;

	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

void P_ForwardThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	if ((player->mo->waterlevel || (player->mo->flags2 & MF2_FLY))
		&& player->mo->pitch != 0)
	{
		angle_t pitch = (angle_t)player->mo->pitch >> ANGLETOFINESHIFT;
		fixed_t zpush = FixedMul (move, finesine[pitch]);
		if (player->mo->waterlevel && player->mo->waterlevel < 2 && zpush < 0)
			zpush = 0;
		player->mo->momz -= zpush;
		move = FixedMul (move, finecosine[pitch]);
	}
	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
void P_CalcHeight (player_t *player)
{
	int 		angle;
	fixed_t 	bob;

	// Regular movement bobbing
	// (needs to be calculated for gun swing even if not on ground)
	// OPTIMIZE: tablify angle

	// killough 10/98: Make bobbing depend only on player-applied motion.
	//
	// Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
	// it causes bobbing jerkiness when the player moves from ice to non-ice,
	// and vice-versa.
	
	if ((player->mo->flags2 & MF2_FLY) && !player->mo->onground)
	{
		player->bob = FRACUNIT / 2;
	}	

	if (!player->spectator)
		if (serverside || !predicting)
		{
			player->bob = FixedMul (player->mo->momx, player->mo->momx)
						+ FixedMul (player->mo->momy, player->mo->momy);
			player->bob >>= 2;

			if (player->bob > MAXBOB)
				player->bob = MAXBOB;
		}

    if ((player->cheats & CF_NOMOMENTUM) || !player->mo->onground)
	{
		player->viewz = player->mo->z + VIEWHEIGHT;

		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;

		return;
	}

	angle = (FINEANGLES/20*level.time) & FINEMASK;
	if (!player->spectator)
		bob = FixedMul (player->bob>>(player->mo->waterlevel > 1 ? 2 : 1), finesine[angle]);
	else
		bob = 0;

	// move viewheight
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > VIEWHEIGHT)
		{
			player->viewheight = VIEWHEIGHT;
			player->deltaviewheight = 0;
		}
		else if (player->viewheight < VIEWHEIGHT/2)
		{
			player->viewheight = VIEWHEIGHT/2;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}

		if (player->deltaviewheight)
		{
			player->deltaviewheight += FRACUNIT/4;
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}
	player->viewz = player->mo->z + player->viewheight + bob;

	if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
		player->viewz = player->mo->ceilingz-4*FRACUNIT;
}

//
// P_PlayerLookUpDown
//
void P_PlayerLookUpDown (player_t *p)
{
	ticcmd_t *cmd = &p->cmd;

	// [RH] Look up/down stuff
	if (!sv_freelook)
	{
		p->mo->pitch = 0;
	}
	else
	{
		int look = cmd->ucmd.pitch << 16;

		// The player's view pitch is clamped between -32 and +56 degrees,
		// which translates to about half a screen height up and (more than)
		// one full screen height down from straight ahead when view panning
		// is used.
		if (look)
		{
			if (look == -32768 << 16)
			{ // center view
				p->mo->pitch = 0;
			}
			else if (look > 0)
			{ // look up
				p->mo->pitch -= look;
				if (p->mo->pitch < -ANG(32))
					p->mo->pitch = -ANG(32);
			}
			else
			{ // look down
				p->mo->pitch -= look;
				if (p->mo->pitch > ANG(56))
					p->mo->pitch = ANG(56);
			}
		}
	}
}

CVAR_FUNC_IMPL (sv_aircontrol)
{
	level.aircontrol = (fixed_t)((float)var * 65536.f);
	G_AirControlChanged ();
}

//
// P_MovePlayer
//
void P_MovePlayer (player_t *player)
{
	ticcmd_t *cmd = &player->cmd;
	AActor *mo = player->mo;
	
	if (player->jumpTics)
		player->jumpTics--;
	
	mo->onground = (mo->z <= mo->floorz) || (mo->flags2 & MF2_ONMOBJ);

	// [RH] Don't let frozen players move
	if (player->cheats & CF_FROZEN)
		return;
		
	// Move around.
	// Reactiontime is used to prevent movement
	//	for a bit after a teleport.
	if (player->mo->reactiontime)
	{
		player->mo->reactiontime--;
		return;
	}

	// [RH] check for jump
	if ((cmd->ucmd.buttons & BT_JUMP) == BT_JUMP)
	{
		if (player->mo->waterlevel >= 2)
		{
			player->mo->momz = 4*FRACUNIT;
		}
		else if (player->mo->flags2 & MF2_FLY)
		{
			player->mo->momz = 3*FRACUNIT;
		}		
		else if (sv_allowjump && player->mo->onground && !player->jumpTics)
		{
			player->mo->momz += 8*FRACUNIT;
			if(!player->spectator)
				S_Sound (player->mo, CHAN_BODY, "*jump1", 1, ATTN_NORM);
				
            player->mo->flags2 &= ~MF2_ONMOBJ;
            player->jumpTics = 18;				
		}
	}
	
	if (co_zdoomphys)
	{
		if (cmd->ucmd.upmove &&
			(player->mo->waterlevel >= 2 || player->mo->flags2 & MF2_FLY))
		{
			player->mo->momz = cmd->ucmd.upmove << 8;
		}		
	}
	else
	{
		if (cmd->ucmd.upmove == -32768)
		{ // Only land if in the air
			if ((player->mo->flags2 & MF2_FLY) && player->mo->waterlevel < 2)
			{
				player->mo->flags2 &= ~MF2_FLY;
				player->mo->flags &= ~MF_NOGRAVITY;
			}
		}
		else if (cmd->ucmd.upmove != 0)
		{
			if (player->mo->waterlevel >= 2 || (player->mo->flags2 & MF2_FLY))
			{
				player->mo->momz = cmd->ucmd.upmove << 8;
			}
			else if (player->mo->waterlevel < 2 && !(player->mo->flags2 & MF2_FLY))
			{
				player->mo->flags2 |= MF2_FLY;
				player->mo->flags |= MF_NOGRAVITY;
				if (player->mo->momz <= -39*FRACUNIT)
				{ // Stop falling scream
					S_StopSound (player->mo, CHAN_VOICE);
				}
			}        
			else if (cmd->ucmd.upmove > 0)
			{
				//P_PlayerUseArtifact (player, arti_fly);
			}
		}		
	}

	// Look left/right
	if(clientside || step_mode)
	{
		mo->angle += cmd->ucmd.yaw << 16;

		// Look up/down stuff
		P_PlayerLookUpDown(player);
	}

	// killough 10/98:
	//
	// We must apply thrust to the player and bobbing separately, to avoid
	// anomalies. The thrust applied to bobbing is always the same strength on
	// ice, because the player still "works just as hard" to move, while the
	// thrust applied to the movement varies with 'movefactor'.

	if (cmd->ucmd.forwardmove | cmd->ucmd.sidemove)
	{
		fixed_t forwardmove, sidemove;
		int bobfactor;
		int friction, movefactor;

		movefactor = P_GetMoveFactor (mo, &friction);
		bobfactor = friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;
		if (!mo->onground && !(mo->flags2 & MF2_FLY) && !mo->waterlevel)
		{
			// [RH] allow very limited movement if not on ground.
			if (co_zdoomphys)
			{
				movefactor = FixedMul (movefactor, level.aircontrol);
				bobfactor = FixedMul (bobfactor, level.aircontrol);
			}
			else
			{
				movefactor >>= 8;
				bobfactor >>= 8;
			}
		}
		forwardmove = (cmd->ucmd.forwardmove * movefactor) >> 8;
		sidemove = (cmd->ucmd.sidemove * movefactor) >> 8;
		
		// [ML] Check for these conditions unless advanced physics is on
		if(co_zdoomphys || 
			(!co_zdoomphys && (mo->onground || (mo->flags2 & MF2_FLY) || mo->waterlevel)))
		{
			if (forwardmove)
			{
				P_ForwardThrust (player, mo->angle, forwardmove);
			}
			if (sidemove)
			{
				P_SideThrust (player, mo->angle, sidemove);
			}
		}

		if (mo->state == &states[S_PLAY])
		{
			P_SetMobjState (player->mo, S_PLAY_RUN1); // denis - fixme - this function might destoy player->mo without setting it to 0
		}

		if (player->cheats & CF_REVERTPLEASE)
		{
			player->cheats &= ~CF_REVERTPLEASE;
			player->camera = player->mo;
		}
	}
}

// [RH] (Adapted from Q2)
// P_FallingDamage
//
void P_FallingDamage (AActor *ent)
{
	float	delta;
	int		damage;

	if (!ent->player)
		return;		// not a player

	if (ent->flags & MF_NOCLIP)
		return;

	if ((ent->player->oldvelocity[2] < 0)
		&& (ent->momz > ent->player->oldvelocity[2])
		&& (!(ent->flags2 & MF2_ONMOBJ)
			|| !(ent->z <= ent->floorz)))
	{
		delta = (float)ent->player->oldvelocity[2];
	}
	else
	{
		if (!(ent->flags2 & MF2_ONMOBJ))
			return;
		delta = (float)(ent->momz - ent->player->oldvelocity[2]);
	}
	delta = delta*delta * 2.03904313e-11f;

	if (delta < 1)
		return;

	if (delta < 15)
	{
		//ent->s.event = EV_FOOTSTEP;
		return;
	}

	if (delta > 30)
	{
		damage = (int)((delta-30)/2);
		if (damage < 1)
			damage = 1;

		if (0)
			P_DamageMobj (ent, NULL, NULL, damage, MOD_FALLING);
	}
	else
	{
		//ent->s.event = EV_FALLSHORT;
		return;
	}
}

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
#define ANG5	(ANG90/18)

void P_DeathThink (player_t *player)
{
	P_MovePsprites (player);
	player->mo->onground = (player->mo->z <= player->mo->floorz);

	// fall to the ground
	if (player->viewheight > 6*FRACUNIT)
		player->viewheight -= FRACUNIT;

	if (player->viewheight < 6*FRACUNIT)
		player->viewheight = 6*FRACUNIT;

	player->deltaviewheight = 0;
	P_CalcHeight (player);
	
	// adjust the player's view to follow its attacker
	if (cl_deathcam || !clientside)
	{
		if (player->attacker && player->attacker != player->mo)
		{
			angle_t angle = P_PointToAngle (player->mo->x,
									 		player->mo->y,
									 		player->attacker->x,
									 		player->attacker->y);

			angle_t delta = angle - player->mo->angle;

			if (delta < ANG5 || delta > (unsigned)-ANG5)
			{
				// Looking at killer so fade damage flash down.
				player->mo->angle = angle;

				if (player->damagecount && !predicting)
					player->damagecount--;
			}
			else if (delta < ANG180)
				player->mo->angle += ANG5;
			else
				player->mo->angle -= ANG5;
		}
		else if (player->damagecount && !predicting)
			player->damagecount--;
	}
	else if (player->damagecount && !predicting)
		player->damagecount--;

	if(serverside)
	{
		// [Toke - dmflags] Old location of DF_FORCE_RESPAWN
		if (player->ingame() && (player->cmd.ucmd.buttons & BT_USE
								 || (!clientside && level.time >= player->respawn_time))) // forced respawn
			player->playerstate = PST_REBORN;
	}
}

//
// P_PlayerThink
//
void P_PlayerThink (player_t *player)
{
	ticcmd_t *cmd;
	weapontype_t newweapon;

	// [RH] Error out if player doesn't have an mobj, but just make
	//		it a warning if the player trying to spawn is a bot
	if (!player->mo)
		I_Error ("No player %d start\n", player->id);
		
	client_t *cl = &player->client;
	
	player->xviewshift = 0;		// [RH] Make sure view is in right place

	// fixme: do this in the cheat code
	if (player->cheats & CF_NOCLIP)
		player->mo->flags |= MF_NOCLIP;
	else
		player->mo->flags &= ~MF_NOCLIP;
		
	if (player->cheats & CF_FLY)
		player->mo->flags |= MF_NOGRAVITY, player->mo->flags2 |= MF2_FLY;
	else
		player->mo->flags &= ~MF_NOGRAVITY, player->mo->flags2 &= ~MF2_FLY;

	// chain saw run forward
	cmd = &player->cmd;
	if (player->mo->flags & MF_JUSTATTACKED)
	{
		cmd->ucmd.yaw = 0;
		cmd->ucmd.forwardmove = 0xc800/2;
		cmd->ucmd.sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}

	if (player->playerstate == PST_DEAD)
	{
		P_DeathThink(player);
		return;
	}

	if(serverside)
	{
		P_MovePlayer (player);

		P_CalcHeight (player);
	}

	if (player->mo->subsector && (player->mo->subsector->sector->special || player->mo->subsector->sector->damage))
		P_PlayerInSpecialSector (player);

	// Check for weapon change.

	// A special event has no other buttons.
	if (cmd->ucmd.buttons & BT_SPECIAL)
		cmd->ucmd.buttons = 0;

	if ((cmd->ucmd.buttons & BT_CHANGE) || cmd->ucmd.impulse >= 50)
	{
		// [RH] Support direct weapon changes
		if (cmd->ucmd.impulse) {
			newweapon = (weapontype_t)(cmd->ucmd.impulse - 50);
		} else {
			// The actual changing of the weapon is done
			//	when the weapon psprite can do it
			//	(read: not in the middle of an attack).
			newweapon = (weapontype_t)((cmd->ucmd.buttons&BT_WEAPONMASK)>>BT_WEAPONSHIFT);

			if (newweapon == wp_fist
				&& player->weaponowned[wp_chainsaw]
				&& !(player->readyweapon == wp_chainsaw
					 && player->powers[pw_strength]))
			{
				newweapon = wp_chainsaw;
			}

			if (gamemode == commercial
				&& newweapon == wp_shotgun
				&& player->weaponowned[wp_supershotgun]
				&& player->readyweapon != wp_supershotgun)
			{
				newweapon = wp_supershotgun;
			}
		}

		if ((newweapon >= 0 && newweapon < NUMWEAPONS)
			&& player->weaponowned[newweapon]
			&& newweapon != player->readyweapon)
		{
			// NEVER go to plasma or BFG in shareware,
			if ((newweapon != wp_plasma && newweapon != wp_bfg)
			|| (gamemode != shareware) )
			{
				player->pendingweapon = newweapon;
						
				if (serverside)
				{	// [ML] From Zdaemon .99: use changeweapon here
					MSG_WriteMarker	(&cl->reliablebuf, svc_changeweapon);
					MSG_WriteByte (&cl->reliablebuf, (byte)player->pendingweapon);	
				}
			}
		}
	}

	// check for use
	if (cmd->ucmd.buttons & BT_USE)
	{
		if (!player->usedown)
		{
			P_UseLines (player);
			player->usedown = true;
		}
	}
	else
		player->usedown = false;

	// cycle psprites
	P_MovePsprites (player);

	// Counters, time dependent power ups.

	// Strength counts up to diminish fade.
	if (player->powers[pw_strength])
		player->powers[pw_strength]++;

	if (player->powers[pw_invulnerability])
		player->powers[pw_invulnerability]--;

	if (player->powers[pw_invisibility])
		if (! --player->powers[pw_invisibility] )
			player->mo->flags &= ~MF_SHADOW;

	if (player->powers[pw_infrared])
		player->powers[pw_infrared]--;

	if (player->powers[pw_ironfeet])
		player->powers[pw_ironfeet]--;

	if (player->damagecount)
		player->damagecount--;

	if (player->bonuscount)
		player->bonuscount--;

	// Handling colormaps.
	if (displayplayer().powers[pw_invulnerability])
	{
		if (displayplayer().powers[pw_invulnerability] > 4*32
			|| (displayplayer().powers[pw_invulnerability]&8) )
			displayplayer().fixedcolormap = INVERSECOLORMAP;
		else
			displayplayer().fixedcolormap = 0;
	}
	else if (player->powers[pw_infrared])
	{
		if (player->powers[pw_infrared] > 4*32
			|| (player->powers[pw_infrared]&8) )
		{
			// almost full bright
			player->fixedcolormap = 1;
		}
		else
			player->fixedcolormap = 0;
	}
	else
		player->fixedcolormap = 0;

	// Handle air supply
	if (player->mo->waterlevel < 3 || player->powers[pw_ironfeet])
	{
		player->air_finished = level.time + 10*TICRATE;
	}
	else if (player->air_finished <= level.time && !(level.time & 31))
	{
		P_DamageMobj (player->mo, NULL, NULL, 2 + 2*((level.time-player->air_finished)/TICRATE), MOD_WATER, DMG_NO_ARMOR);
	}
}

void player_s::Serialize (FArchive &arc)
{
	size_t i;

	if (arc.IsStoring ())
	{ // saving to archive
		arc << id
			<< playerstate
			<< cmd
			<< userinfo
			<< viewz
			<< viewheight
			<< deltaviewheight
			<< bob
			<< health
			<< armorpoints
			<< armortype
			<< backpack
			<< fragcount
			<< (int)readyweapon
			<< (int)pendingweapon
			<< attackdown
			<< usedown
			<< cheats
			<< refire
			<< killcount
			<< itemcount
			<< secretcount			
			<< damagecount
			<< bonuscount
			/*<< attacker->netid*/
			<< extralight
			<< fixedcolormap
			<< xviewshift
			<< jumpTics			
			<< respawn_time
			<< air_finished;
		for (i = 0; i < NUMPOWERS; i++)
			arc << powers[i];
		for (i = 0; i < NUMCARDS; i++)
			arc << cards[i];
		for (i = 0; i < NUMWEAPONS; i++)
			arc << weaponowned[i];
		for (i = 0; i < NUMAMMO; i++)
			arc << ammo[i] << maxammo[i];
		for (i = 0; i < NUMPSPRITES; i++)
			arc << psprites[i];
		for (i = 0; i < 3; i++)
			arc << oldvelocity[i];
	}
	else
	{ // Restoring from archive
		userinfo_t dummyuserinfo;

		arc >> id
			>> playerstate
			>> cmd
			>> dummyuserinfo // Q: Would it be better to restore the userinfo from the archive?
			>> viewz
			>> viewheight
			>> deltaviewheight
			>> bob
			>> health
			>> armorpoints
			>> armortype
			>> backpack
			>> fragcount
			>> (int&)readyweapon
			>> (int&)pendingweapon
			>> attackdown
			>> usedown
			>> cheats
			>> refire
			>> killcount
			>> itemcount
			>> secretcount			
			>> damagecount
			>> bonuscount
			/*>> attacker->netid*/
			>> extralight
			>> fixedcolormap
			>> xviewshift
			>> jumpTics			
			>> respawn_time
			>> air_finished;
		for (i = 0; i < NUMPOWERS; i++)
			arc >> powers[i];
		for (i = 0; i < NUMCARDS; i++)
			arc >> cards[i];
		for (i = 0; i < NUMWEAPONS; i++)
			arc >> weaponowned[i];
		for (i = 0; i < NUMAMMO; i++)
			arc >> ammo[i] >> maxammo[i];
		for (i = 0; i < NUMPSPRITES; i++)
			arc >> psprites[i];
		for (i = 0; i < 3; i++)
			arc >> oldvelocity[i];

		camera = mo;

		if (&consoleplayer() != this)
			userinfo = dummyuserinfo;
	}
}


//
// P_WriteMobjSpawnUpdate
//
void P_WriteMobjSpawnUpdate(buf_t &buf, AActor *mo)
{
	if (!mo)
		return;

	MSG_WriteMarker(&buf, svc_spawnmobj);
	MSG_WriteLong(&buf, mo->x);
	MSG_WriteLong(&buf, mo->y);
	MSG_WriteLong(&buf, mo->z);
	MSG_WriteLong(&buf, mo->angle);

	MSG_WriteShort(&buf, mo->type);
	MSG_WriteShort(&buf, mo->netid);
	MSG_WriteByte(&buf, mo->rndindex);
	MSG_WriteShort(&buf, (mo->state - states)); // denis - sending state fixes monster ghosts appearing under doors

	if(mo->flags & MF_MISSILE || mobjinfo[mo->type].flags & MF_MISSILE) // denis - check type as that is what the client will be spawning
	{
		MSG_WriteShort (&buf, mo->target ? mo->target->netid : 0);
		MSG_WriteShort (&buf, mo->netid);
		MSG_WriteLong (&buf, mo->angle);
		MSG_WriteLong (&buf, mo->momx);
		MSG_WriteLong (&buf, mo->momy);
		MSG_WriteLong (&buf, mo->momz);
	}
	else
	{
		if(mo->flags & MF_AMBUSH || mo->flags & MF_DROPPED)
		{
			MSG_WriteMarker(&buf, svc_mobjinfo);
			MSG_WriteShort(&buf, mo->netid);
			MSG_WriteLong(&buf, mo->flags);
		}
	}

	// animating corpses
	if((mo->flags & MF_CORPSE) && mo->state - states != S_GIBS)
	{
		MSG_WriteMarker (&buf, svc_corpse);
		MSG_WriteShort (&buf, mo->netid);
		MSG_WriteByte (&buf, mo->frame);
		MSG_WriteByte (&buf, mo->tics);
	}
}


//
// P_IsTeammate
//
bool P_IsTeammate(player_t &a, player_t &b)
{
	// same player isn't own teammate
	if(&a == &b)
		return false;

	if (sv_gametype == GM_TEAMDM || sv_gametype == GM_CTF)
	{
		if (a.userinfo.team == b.userinfo.team)
			return true;
		else
			return false;
	}
	else if (sv_gametype == GM_COOP)
		return true;

	else 
		return false;
}

//
// P_WriteAwarenessUpdate
//
bool P_WriteAwarenessUpdate(buf_t &buf, player_t &player, AActor *mo, bool always_write = false)
{
	bool ok = false;

	if (!mo)
		return false;

	if(player.mo == mo)
		ok = true;
	else if(!mo->player)
		ok = true;
	else if (mo->flags & MF_SPECTATOR)      // GhostlyDeath -- Spectating things
		ok = false;
	else if(player.mo && mo->player && mo->player->spectator)
		ok = false;
	else if(player.mo && mo->player && P_IsTeammate(player, *mo->player))
		ok = true;

	else if(player.mo && mo->player && !sv_antiwallhack)
		ok = true;
	else if (	player.mo && mo->player && sv_antiwallhack &&
				player.spectator)	// GhostlyDeath -- Spectators MUST see players to F12 properly
		ok = true;
//  [SL] 2011-07-07 - TODO: FIX THIS
/*	else if(player.mo && mo->player && sv_antiwallhack && 
		((HasBehavior && P_CheckSightEdges2(player.mo, mo, 5)) || (!HasBehavior && P_CheckSightEdges(player.mo, mo, 5))))
		ok = true; */

	std::vector<size_t>::iterator a = std::find(mo->players_aware.begin(), mo->players_aware.end(), player.id);
	bool previously_ok = (a != mo->players_aware.end());

	if (always_write)
	{
		previously_ok = false;
		ok = true;
	}
	
	if(!ok && previously_ok)
	{
		mo->players_aware.erase(a);

		MSG_WriteMarker (&buf, svc_removemobj);
		MSG_WriteShort (&buf, mo->netid);

		return true;
	}
	else if(!previously_ok && ok)
	{
		mo->players_aware.push_back(player.id);

		if(!mo->player || mo->player->playerstate != PST_LIVE)
		{
			P_WriteMobjSpawnUpdate(buf, mo);
		}
		else
		{
			MSG_WriteMarker (&buf, svc_spawnplayer);
			MSG_WriteByte (&buf, mo->player->id);
			MSG_WriteShort (&buf, mo->netid);
			MSG_WriteLong (&buf, mo->angle);
			MSG_WriteLong (&buf, mo->x);
			MSG_WriteLong (&buf, mo->y);
			MSG_WriteLong (&buf, mo->z);
		}

		return true;
	}

	return false;
}

//
// P_WriteUserInfo
//
void P_WriteUserInfo (buf_t &buf, player_t &player)
{
	player_t *p = &player;

	MSG_WriteMarker	(&buf, svc_userinfo);
	MSG_WriteByte	(&buf, p->id);
	MSG_WriteString (&buf, p->userinfo.netname);
	MSG_WriteByte	(&buf, p->userinfo.team);
	MSG_WriteLong	(&buf, p->userinfo.gender);
	MSG_WriteLong	(&buf, p->userinfo.color);
	MSG_WriteString	(&buf, skins[p->userinfo.skin].name);  // [Toke - skins]
	MSG_WriteShort	(&buf, time(NULL) - p->JoinTime);
}

//
// P_WriteHiddenMobjUpdate
//
void P_WriteHiddenMobjUpdate (buf_t &buf, player_t &pl)
{
	// denis - todo - throttle this
	AActor *mo;
	TThinkerIterator<AActor> iterator;

	if (!pl.mo)
		return;

	int updated = 0;

	while(!pl.to_spawn.empty())
	{
		mo = pl.to_spawn.front();

		pl.to_spawn.pop();

		if(mo && !mo->WasDestroyed())
			updated += P_WriteAwarenessUpdate(buf, pl, mo);

		if(updated > 16)
			break;
	}

	while ( (mo = iterator.Next() ) )
	{
		if (serverside)
			updated += P_WriteAwarenessUpdate(buf, pl, mo, false);
		else
			updated += P_WriteAwarenessUpdate(buf, pl, mo, true);

		if(updated > 16)
			break;
	}
}


//
// P_WriteSectorsUpdate
// Update doors, floors, ceilings etc... that have at some point moved
//
void P_WriteSectorsUpdate(buf_t &buf)
{
	for (int s=0; s<numsectors; s++)
	{
		sector_t* sec = &sectors[s];

		if (sec->moveable)
		{
			MSG_WriteMarker (&buf, svc_sector);
			MSG_WriteShort (&buf, s);
			MSG_WriteShort (&buf, sec->floorheight>>FRACBITS);
			MSG_WriteShort (&buf, sec->ceilingheight>>FRACBITS);
			MSG_WriteShort (&buf, sec->floorpic);
			MSG_WriteShort (&buf, sec->ceilingpic);
		}
	}
}

extern int TEAMpoints[NUMFLAGS];
bool SV_SendPacket(player_t &p);

//
// P_WriteClientFullUpdate
//
void P_WriteClientFullUpdate (buf_t &buf, player_t &pl)
{
	size_t	i;

	// send player's info to the client
	for (i=0; i < players.size(); i++)
	{
		if (!players[i].ingame())
			continue;

		if(players[i].mo)
		{
			if (serverside)
				P_WriteAwarenessUpdate(buf, pl, players[i].mo, false);
			else
				P_WriteAwarenessUpdate(buf, pl, players[i].mo, true);
		}

		P_WriteUserInfo(buf, players[i]);

		if (buf.cursize >= 600 && serverside && multiplayer)
			if (!SV_SendPacket(pl))
				return;
	}

	// update frags/points/spectate
	for (i = 0; i < players.size(); i++)
	{
		if (!players[i].ingame())
			continue;

		MSG_WriteMarker(&buf, svc_updatefrags);
		MSG_WriteByte(&buf, players[i].id);
		if(sv_gametype != GM_COOP)
		{
			MSG_WriteShort(&buf, players[i].fragcount);
		}
		else
		{
			MSG_WriteShort(&buf, players[i].killcount);
		}
		MSG_WriteShort(&buf, players[i].deathcount);
		MSG_WriteShort(&buf, players[i].points);

		MSG_WriteMarker (&buf, svc_spectate);
		MSG_WriteByte (&buf, players[i].id);
		MSG_WriteByte (&buf, players[i].spectator);
	}

	// [deathz0r] send team frags/captures if teamplay is enabled
	if(sv_gametype == GM_TEAMDM || sv_gametype == GM_CTF)
	{
		MSG_WriteMarker (&buf, svc_teampoints);
		for (i = 0; i < NUMTEAMS; i++)
			MSG_WriteShort (&buf, TEAMpoints[i]);
	}

	P_WriteHiddenMobjUpdate(buf, pl);

	// update flags
	// TODO: Implement this!
	//	if(sv_gametype == GM_CTF)
	//		CTF_Connect(pl);

	// update sectors
	P_WriteSectorsUpdate(buf);
	if (buf.cursize >= 600 && serverside && multiplayer)
		if(!SV_SendPacket(pl))
			return;

	// update switches
	for (int l=0; l<numlines; l++)
	{
		unsigned state = 0, time = 0;
		if(P_GetButtonInfo(&lines[l], state, time) || lines[l].wastoggled)
		{
			MSG_WriteMarker (&buf, svc_switch);
			MSG_WriteLong (&buf, l);
			MSG_WriteByte (&buf, lines[l].wastoggled);
			MSG_WriteByte (&buf, state);
			MSG_WriteLong (&buf, time);
		}
	}

	if (serverside && multiplayer)
		SV_SendPacket(pl);
}


VERSION_CONTROL (p_user_cpp, "$Id$")

