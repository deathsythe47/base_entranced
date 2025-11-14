// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "G2.h"
#include "bg_saga.h"
#include "g_database.h"

//#include "accounts.h"
#include "sha1.h"
#include "sha3.h"

#define POWER_DUEL_START_HEALTH 150
#define POWER_DUEL_END_HEALTH 90

// g_client.c -- client functions that don't happen every frame

static vec3_t	playerMins = {-15, -15, DEFAULT_MINS_2};
static vec3_t	playerMaxs = {15, 15, DEFAULT_MAXS_2};

extern void EWebDisattach(gentity_t *owner, gentity_t *eweb);

void WP_SaberAddG2Model( gentity_t *saberent, const char *saberModel, qhandle_t saberSkin );
void WP_SaberRemoveG2Model( gentity_t *saberent );
extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );

forcedata_t Client_Force[MAX_CLIENTS];

/*QUAKED info_player_duel (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for duelists in duel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel( gentity_t *ent )
{
	int		i;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_duel1 (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for lone duelists in powerduel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel1( gentity_t *ent )
{
	int		i;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_duel2 (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for paired duelists in powerduel.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_duel2( gentity_t *ent )
{
	int		i;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}

}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for deathmatch games.
The first time a player enters the game, they will be at an 'initial' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_deathmatch( gentity_t *ent ) {
	int		i;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
Targets will be fired when someone spawns in on them.
equivelant to info_player_deathmatch
*/
void SP_info_player_start(gentity_t *ent) {
	ent->classname = "info_player_deathmatch";
	SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_start_red (1 0 0) (-16 -16 -24) (16 16 32) INITIAL
For Red Team DM starts

Targets will be fired when someone spawns in on them.
equivalent to info_player_deathmatch

INITIAL - The first time a player enters the game, they will be at an 'initial' spot.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_start_red(gentity_t *ent) {
	SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_start_blue (1 0 0) (-16 -16 -24) (16 16 32) INITIAL
For Blue Team DM starts

Targets will be fired when someone spawns in on them.
equivalent to info_player_deathmatch

INITIAL - The first time a player enters the game, they will be at an 'initial' spot.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_start_blue(gentity_t *ent) {
	SP_info_player_deathmatch( ent );
}

void SiegePointUse( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	//Toggle the point on/off
	if (self->genericValue1)
	{
		self->genericValue1 = 0;
	}
	else
	{
		self->genericValue1 = 1;
	}
}

/*QUAKED info_player_siegeteam1 (1 0 0) (-16 -16 -24) (16 16 32)
siege start point - team1
name and behavior of team1 depends on what is defined in the
.siege file for this level

startoff - if non-0 spawn point will be disabled until used
idealclass - if specified, this spawn point will be considered
"ideal" for players of this class name. Corresponds to the name
entry in the .scl (siege class) file.
Targets will be fired when someone spawns in on them.
*/
void SP_info_player_siegeteam1(gentity_t *ent) {
	int soff = 0;

	if (g_gametype.integer != GT_SIEGE)
	{ //turn into a DM spawn if not in siege game mode
		ent->classname = "info_player_deathmatch";
		SP_info_player_deathmatch( ent );

		return;
	}

	G_SpawnInt("startoff", "0", &soff);

	if (soff)
	{ //start disabled
		ent->genericValue1 = 0;
	}
	else
	{
		ent->genericValue1 = 1;
	}

	ent->use = SiegePointUse;
}

/*QUAKED info_player_siegeteam2 (0 0 1) (-16 -16 -24) (16 16 32)
siege start point - team2
name and behavior of team2 depends on what is defined in the
.siege file for this level

startoff - if non-0 spawn point will be disabled until used
idealclass - if specified, this spawn point will be considered
"ideal" for players of this class name. Corresponds to the name
entry in the .scl (siege class) file.
Targets will be fired when someone spawns in on them.
*/
void SP_info_player_siegeteam2(gentity_t *ent) {
	int soff = 0;

	if (g_gametype.integer != GT_SIEGE)
	{ //turn into a DM spawn if not in siege game mode
		ent->classname = "info_player_deathmatch";
		SP_info_player_deathmatch( ent );

		return;
	}

	G_SpawnInt("startoff", "0", &soff);

	if (soff)
	{ //start disabled
		ent->genericValue1 = 0;
	}
	else
	{
		ent->genericValue1 = 1;
	}

	ent->use = SiegePointUse;
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32) RED BLUE
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
RED - In a Siege game, the intermission will happen here if the Red (attacking) team wins
BLUE - In a Siege game, the intermission will happen here if the Blue (defending) team wins
*/
void SP_info_player_intermission( gentity_t *ent ) {

}

/*QUAKED info_player_intermission_red (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.

In a Siege game, the intermission will happen here if the Red (attacking) team wins
target - ent to look at
target2 - ents to use when this intermission point is chosen
*/
void SP_info_player_intermission_red( gentity_t *ent ) {

}

/*QUAKED info_player_intermission_blue (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.

In a Siege game, the intermission will happen here if the Blue (defending) team wins
target - ent to look at
target2 - ents to use when this intermission point is chosen
*/
void SP_info_player_intermission_blue( gentity_t *ent ) {

}

#define JMSABER_RESPAWN_TIME 20000 //in case it gets stuck somewhere no one can reach

void ThrowSaberToAttacker(gentity_t *self, gentity_t *attacker)
{
	gentity_t *ent = &g_entities[self->client->ps.saberIndex];
	vec3_t a;
	int altVelocity = 0;

	if (!ent || ent->enemy != self)
	{ //something has gone very wrong (this should never happen)
		//but in case it does.. find the saber manually
#if 0//#ifdef _DEBUG
		Com_Printf("Lost the saber! Attempting to use global pointer..\n");
#endif
		ent = gJMSaberEnt;

		if (!ent)
		{
#if 0//#ifdef _DEBUG
			Com_Printf("The global pointer was NULL. This is a bad thing.\n");
#endif
			return;
		}

#if 0//#ifdef _DEBUG
		Com_Printf("Got it (%i). Setting enemy to client %i.\n", ent->s.number, self->s.number);
#endif

		ent->enemy = self;
		self->client->ps.saberIndex = ent->s.number;
	}

	trap_SetConfigstring ( CS_CLIENT_JEDIMASTER, "-1" );

	if (attacker && attacker->client && self->client->ps.saberInFlight)
	{ //someone killed us and we had the saber thrown, so actually move this saber to the saber location
	  //if we killed ourselves with saber thrown, however, same suicide rules of respawning at spawn spot still
	  //apply.
		gentity_t *flyingsaber = &g_entities[self->client->ps.saberEntityNum];

		if (flyingsaber && flyingsaber->inuse)
		{
			VectorCopy(flyingsaber->s.pos.trBase, ent->s.pos.trBase);
			VectorCopy(flyingsaber->s.pos.trDelta, ent->s.pos.trDelta);
			VectorCopy(flyingsaber->s.apos.trBase, ent->s.apos.trBase);
			VectorCopy(flyingsaber->s.apos.trDelta, ent->s.apos.trDelta);

			VectorCopy(flyingsaber->r.currentOrigin, ent->r.currentOrigin);
			VectorCopy(flyingsaber->r.currentAngles, ent->r.currentAngles);
			altVelocity = 1;
		}
	}

	self->client->ps.saberInFlight = qtrue; //say he threw it anyway in order to properly remove from dead body

	WP_SaberAddG2Model( ent, self->client->saber[0].model, self->client->saber[0].skin );

	ent->s.eFlags &= ~(EF_NODRAW);
	ent->s.modelGhoul2 = 1;
	ent->s.eType = ET_MISSILE;
	ent->enemy = NULL;

	if (!attacker || !attacker->client)
	{
		VectorCopy(ent->s.origin2, ent->s.pos.trBase);
		VectorCopy(ent->s.origin2, ent->s.origin);
		VectorCopy(ent->s.origin2, ent->r.currentOrigin);
		ent->pos2[0] = 0;
		trap_LinkEntity(ent);
		return;
	}

	if (!altVelocity)
	{
		VectorCopy(self->s.pos.trBase, ent->s.pos.trBase);
		VectorCopy(self->s.pos.trBase, ent->s.origin);
		VectorCopy(self->s.pos.trBase, ent->r.currentOrigin);

		VectorSubtract(attacker->client->ps.origin, ent->s.pos.trBase, a);

		VectorNormalize(a);

		ent->s.pos.trDelta[0] = a[0]*256;
		ent->s.pos.trDelta[1] = a[1]*256;
		ent->s.pos.trDelta[2] = 256;
	}

	trap_LinkEntity(ent);
}

void JMSaberThink(gentity_t *ent)
{
	gJMSaberEnt = ent;

	if (ent->enemy)
	{
		if (!ent->enemy->client || !ent->enemy->inuse)
		{ //disconnected?
			VectorCopy(ent->enemy->s.pos.trBase, ent->s.pos.trBase);
			VectorCopy(ent->enemy->s.pos.trBase, ent->s.origin);
			VectorCopy(ent->enemy->s.pos.trBase, ent->r.currentOrigin);
			ent->s.modelindex = G_ModelIndex("models/weapons2/saber/saber_w.glm");
			ent->s.eFlags &= ~(EF_NODRAW);
			ent->s.modelGhoul2 = 1;
			ent->s.eType = ET_MISSILE;
			ent->enemy = NULL;

			ent->pos2[0] = 1;
			ent->pos2[1] = 0; //respawn next think
			trap_LinkEntity(ent);
		}
		else
		{
			ent->pos2[1] = level.time + JMSABER_RESPAWN_TIME;
		}
	}
	else if (ent->pos2[0] && ent->pos2[1] < level.time)
	{
		VectorCopy(ent->s.origin2, ent->s.pos.trBase);
		VectorCopy(ent->s.origin2, ent->s.origin);
		VectorCopy(ent->s.origin2, ent->r.currentOrigin);
		ent->pos2[0] = 0;
		trap_LinkEntity(ent);
	}

	ent->nextthink = level.time + 50;
	G_RunObject(ent);
}

void JMSaberTouch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	int i = 0;

	if (!other || !other->client || other->health < 1)
	{
		return;
	}

	if (self->enemy)
	{
		return;
	}

	if (!self->s.modelindex)
	{
		return;
	}

	if (other->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
	{
		return;
	}

	if (other->client->ps.isJediMaster)
	{
		return;
	}

	self->enemy = other;
	other->client->ps.stats[STAT_WEAPONS] = (1 << WP_SABER);
	other->client->ps.weapon = WP_SABER;
	other->s.weapon = WP_SABER;
	G_AddEvent(other, EV_BECOME_JEDIMASTER, 0);

	// Track the jedi master 
	trap_SetConfigstring ( CS_CLIENT_JEDIMASTER, va("%i", other->s.number ) );

	if (g_spawnInvulnerability.integer)
	{
		other->client->ps.eFlags |= EF_INVULNERABLE;
		other->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
	}

	trap_SendServerCommand( -1, va("cp \"%s %s\n\"", other->client->pers.netname, G_GetStringEdString("MP_SVGAME", "BECOMEJM")) );

	other->client->ps.isJediMaster = qtrue;
	other->client->ps.saberIndex = self->s.number;

	if (other->health < 200 && other->health > 0)
	{ //full health when you become the Jedi Master
		other->client->ps.stats[STAT_HEALTH] = other->health = 200;
	}

	if (other->client->ps.fd.forcePower < 100)
	{
		other->client->ps.fd.forcePower = 100;
	}

	while (i < NUM_FORCE_POWERS)
	{
		other->client->ps.fd.forcePowersKnown |= (1 << i);
		other->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;

		i++;
	}

	self->pos2[0] = 1;
	self->pos2[1] = level.time + JMSABER_RESPAWN_TIME;

	self->s.modelindex = 0;
	self->s.eFlags |= EF_NODRAW;
	self->s.modelGhoul2 = 0;
	self->s.eType = ET_GENERAL;

	G_KillG2Queue(self->s.number);

	return;
}

gentity_t *gJMSaberEnt = NULL;

/*QUAKED info_jedimaster_start (1 0 0) (-16 -16 -24) (16 16 32)
"jedi master" saber spawn point
*/
void SP_info_jedimaster_start(gentity_t *ent)
{
	if (g_gametype.integer != GT_JEDIMASTER)
	{
		gJMSaberEnt = NULL;
		G_FreeEntity(ent);
		return;
	}

	ent->enemy = NULL;

	ent->flags = FL_BOUNCE_HALF;

	ent->s.modelindex = G_ModelIndex("models/weapons2/saber/saber_w.glm");
	ent->s.modelGhoul2 = 1;
	ent->s.g2radius = 20;
	ent->s.eType = ET_MISSILE;
	ent->s.weapon = WP_SABER;
	ent->s.pos.trType = TR_GRAVITY;
	ent->s.pos.trTime = level.time;
	VectorSet( ent->r.maxs, 3, 3, 3 );
	VectorSet( ent->r.mins, -3, -3, -3 );
	ent->r.contents = CONTENTS_TRIGGER;
	ent->clipmask = MASK_SOLID;

	ent->isSaberEntity = qtrue;

	ent->bounceCount = -5;

	ent->physicsObject = qtrue;

	VectorCopy(ent->s.pos.trBase, ent->s.origin2); //remember the spawn spot

	ent->touch = JMSaberTouch;

	trap_LinkEntity(ent);

	ent->think = JMSaberThink;
	ent->nextthink = level.time + 50;
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag( gentity_t *spot ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( spot->s.origin, playerMins, mins );
	VectorAdd( spot->s.origin, playerMaxs, maxs );

	// spawn logic can move player a bit above, we need to be more strict
	maxs[2] += 9;

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];
		//if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
		if ( hit->client) {
			return qtrue;
		}

	}

	return qfalse;
}

qboolean SpotWouldTelefrag2( gentity_t *mover, vec3_t dest ) 
{
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( dest, mover->r.mins, mins );
	VectorAdd( dest, mover->r.maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) 
	{
		hit = &g_entities[touch[i]];
		if ( hit == mover )
		{
			continue;
		}

		if ( hit->r.contents & mover->r.contents )
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
================
SelectNearestDeathmatchSpawnPoint

Find the spot that we DON'T want to use
================
*/
#define	MAX_SPAWN_POINTS	128
gentity_t *SelectNearestDeathmatchSpawnPoint( vec3_t from ) {
	gentity_t	*spot;
	vec3_t		delta;
	float		dist, nearestDist;
	gentity_t	*nearestSpot;

	nearestDist = 999999;
	nearestSpot = NULL;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {

		VectorSubtract( spot->s.origin, from, delta );
		dist = VectorLength( delta );
		if ( dist < nearestDist ) {
			nearestDist = dist;
			nearestSpot = spot;
		}
	}

	return nearestSpot;
}


/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define	MAX_SPAWN_POINTS	128
gentity_t *SelectRandomDeathmatchSpawnPoint( void ) {
	gentity_t	*spot;
	int			count;
	int			selection;
	gentity_t	*spots[MAX_SPAWN_POINTS];

	count = 0;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}
		spots[ count ] = spot;
		count++;
	}

	if ( !count ) {	// no spots that won't telefrag
		return G_Find( NULL, FOFS(classname), "info_player_deathmatch");
	}

	selection = rand() % count;
	return spots[ selection ];
}

/*
===========
SelectRandomFurthestSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *SelectRandomFurthestSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles, team_t team ) {
	gentity_t	*spot;
	vec3_t		delta;
	float		dist;
	float		list_dist[64];
	gentity_t	*list_spot[64];
	int			numSpots, rnd, i, j;

	numSpots = 0;
	spot = NULL;

	//in Team DM, look for a team start spot first, if any
	if ( g_gametype.integer == GT_TEAM 
		&& team != TEAM_FREE 
		&& team != TEAM_SPECTATOR )
	{
		char *classname = NULL;
		if ( team == TEAM_RED )
		{
			classname = "info_player_start_red";
		}
		else
		{
			classname = "info_player_start_blue";
		}
		while ((spot = G_Find (spot, FOFS(classname), classname)) != NULL) {
			if ( SpotWouldTelefrag( spot ) ) {
				continue;
			}
			VectorSubtract( spot->s.origin, avoidPoint, delta );
			dist = VectorLength( delta );
			for (i = 0; i < numSpots; i++) {
				if ( dist > list_dist[i] ) {
					if ( numSpots >= 64 )
						numSpots = 64-1;
					for (j = numSpots; j > i; j--) {
						list_dist[j] = list_dist[j-1];
						list_spot[j] = list_spot[j-1];
					}
					list_dist[i] = dist;
					list_spot[i] = spot;
					numSpots++;
					if (numSpots > 64)
						numSpots = 64;
					break;
				}
			}
			if (i >= numSpots && numSpots < 64) {
				list_dist[numSpots] = dist;
				list_spot[numSpots] = spot;
				numSpots++;
			}
		}
	}

	if ( !numSpots )
	{//couldn't find any of the above
		while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
			if ( SpotWouldTelefrag( spot ) ) {
				continue;
			}
			VectorSubtract( spot->s.origin, avoidPoint, delta );
			dist = VectorLength( delta );
			for (i = 0; i < numSpots; i++) {
				if ( dist > list_dist[i] ) {
					if ( numSpots >= 64 )
						numSpots = 64-1;
					for (j = numSpots; j > i; j--) {
						list_dist[j] = list_dist[j-1];
						list_spot[j] = list_spot[j-1];
					}
					list_dist[i] = dist;
					list_spot[i] = spot;
					numSpots++;
					if (numSpots > 64)
						numSpots = 64;
					break;
				}
			}
			if (i >= numSpots && numSpots < 64) {
				list_dist[numSpots] = dist;
				list_spot[numSpots] = spot;
				numSpots++;
			}
		}
		if (!numSpots) {
			spot = G_Find( NULL, FOFS(classname), "info_player_deathmatch");
			if (!spot)
				G_Error( "Couldn't find a spawn point" );
			VectorCopy (spot->s.origin, origin);
			origin[2] += 9;
			VectorCopy (spot->s.angles, angles);
			return spot;
		}
	}

	// select a random spot from the spawn points furthest away
	rnd = random() * (numSpots / 2);

	VectorCopy (list_spot[rnd]->s.origin, origin);
	origin[2] += 9;
	VectorCopy (list_spot[rnd]->s.angles, angles);

	return list_spot[rnd];
}

gentity_t *SelectDuelSpawnPoint( int team, vec3_t avoidPoint, vec3_t origin, vec3_t angles )
{
	gentity_t	*spot;
	vec3_t		delta;
	float		dist;
	float		list_dist[64];
	gentity_t	*list_spot[64];
	int			numSpots, rnd, i, j;
	char		*spotName;

	if (team == DUELTEAM_LONE)
	{
		spotName = "info_player_duel1";
	}
	else if (team == DUELTEAM_DOUBLE)
	{
		spotName = "info_player_duel2";
	}
	else if (team == DUELTEAM_SINGLE)
	{
		spotName = "info_player_duel";
	}
	else
	{
		spotName = "info_player_deathmatch";
	}
tryAgain:

	numSpots = 0;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), spotName)) != NULL) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}
		VectorSubtract( spot->s.origin, avoidPoint, delta );
		dist = VectorLength( delta );
		for (i = 0; i < numSpots; i++) {
			if ( dist > list_dist[i] ) {
				if ( numSpots >= 64 )
					numSpots = 64-1;
				for (j = numSpots; j > i; j--) {
					list_dist[j] = list_dist[j-1];
					list_spot[j] = list_spot[j-1];
				}
				list_dist[i] = dist;
				list_spot[i] = spot;
				numSpots++;
				if (numSpots > 64)
					numSpots = 64;
				break;
			}
		}
		if (i >= numSpots && numSpots < 64) {
			list_dist[numSpots] = dist;
			list_spot[numSpots] = spot;
			numSpots++;
		}
	}
	if (!numSpots)
	{
		if (Q_stricmp(spotName, "info_player_deathmatch"))
		{ //try the loop again with info_player_deathmatch as the target if we couldn't find a duel spot
			spotName = "info_player_deathmatch";
			goto tryAgain;
		}

		//If we got here we found no free duel or DM spots, just try the first DM spot
		spot = G_Find( NULL, FOFS(classname), "info_player_deathmatch");
		if (!spot)
			G_Error( "Couldn't find a spawn point" );
		VectorCopy (spot->s.origin, origin);
		origin[2] += 9;
		VectorCopy (spot->s.angles, angles);
		return spot;
	}

	// select a random spot from the spawn points furthest away
	rnd = random() * (numSpots / 2);

	VectorCopy (list_spot[rnd]->s.origin, origin);
	origin[2] += 9;
	VectorCopy (list_spot[rnd]->s.angles, angles);

	return list_spot[rnd];
}

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *SelectSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles, team_t team ) {
	return SelectRandomFurthestSpawnPoint( avoidPoint, origin, angles, team );
	}

/*
===========
SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
gentity_t *SelectInitialSpawnPoint( vec3_t origin, vec3_t angles, team_t team ) {
	gentity_t	*spot;

	spot = NULL;
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
		if ( spot->spawnflags & 1 ) {
			break;
		}
	}

	if ( !spot || SpotWouldTelefrag( spot ) ) {
		return SelectSpawnPoint( vec3_origin, origin, angles, team );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*
===========
SelectSpectatorSpawnPoint

============
*/
gentity_t *SelectSpectatorSpawnPoint( vec3_t origin, vec3_t angles ) {
	FindIntermissionPoint();

	VectorCopy( level.intermission_origin, origin );
	VectorCopy( level.intermission_angle, angles );

	return NULL;
}

/*
=======================================================================

BODYQUE

=======================================================================
*/

/*
=======================================================================

BODYQUE

=======================================================================
*/

#define BODY_SINK_TIME		30000//45000

/*
===============
InitBodyQue
===============
*/
void InitBodyQue (void) {
	int		i;
	gentity_t	*ent;

	level.bodyQueIndex = 0;
	for (i=0; i < Com_Clampi(8, 64, g_corpseLimit.integer); i++) {
		ent = G_Spawn();
		ent->classname = "bodyque";
		ent->neverFree = qtrue;
		level.bodyQue[i] = ent;
	}
}

/*
=============
BodySink

After sitting around for five seconds, fall into the ground and dissapear
=============
*/
extern void BodyRid(gentity_t *ent);
void BodySink( gentity_t *ent ) {
	G_AddEvent(ent, EV_BODYFADE, 69);
	ent->think = BodyRid;
	ent->nextthink = level.time + 4000;
	ent->takedamage = qtrue; // duo: changed from qfalse
}

/*
=============
CopyToBodyQue

A player is respawning, so make an entity that looks
just like the existing corpse to leave behind.
=============
*/
static qboolean CopyToBodyQue( gentity_t *ent ) {
	gentity_t		*body;
	int			contents;
	int			islight = 0;

	if (level.intermissiontime)
	{
		return qfalse;
	}

	trap_UnlinkEntity (ent);

	// if client is in a nodrop area, don't leave the body
	contents = trap_PointContents( ent->s.origin, -1 );
	if ( contents & CONTENTS_NODROP ) {
		return qfalse;
	}

	if (ent->client && (ent->client->ps.eFlags & EF_DISINTEGRATION))
	{ //for now, just don't spawn a body if you got disint'd
		return qfalse;
	}

	// grab a body que and cycle to the next one
	body = level.bodyQue[ level.bodyQueIndex ];
	level.bodyQueIndex = (level.bodyQueIndex + 1) % Com_Clampi(8, 64, g_corpseLimit.integer);

	trap_UnlinkEntity (body);
	body->s = ent->s;

	//avoid oddly angled corpses floating around
	body->s.angles[PITCH] = body->s.angles[ROLL] = body->s.apos.trBase[PITCH] = body->s.apos.trBase[ROLL] = 0;

	body->s.g2radius = 100;

	body->s.eType = ET_BODY;
	body->s.eFlags = EF_DEAD;		// clear EF_TALK, etc

	if (ent->client && (ent->client->ps.eFlags & EF_DISINTEGRATION))
	{
		body->s.eFlags |= EF_DISINTEGRATION;
	}

	VectorCopy(ent->client->ps.lastHitLoc, body->s.origin2);

	body->s.powerups = 0;	// clear powerups
	body->s.loopSound = 0;	// clear lava burning
	body->s.loopIsSoundset = qfalse;
	body->s.number = body - g_entities;
	body->timestamp = level.time;
	body->physicsObject = qtrue;
	body->physicsBounce = 0;		// don't bounce
	if ( body->s.groundEntityNum == ENTITYNUM_NONE ) {
		body->s.pos.trType = TR_GRAVITY;
		body->s.pos.trTime = level.time;
		VectorCopy( ent->client->ps.velocity, body->s.pos.trDelta );
	} else {
		body->s.pos.trType = TR_STATIONARY;
	}
	body->s.event = 0;

	body->s.weapon = ent->s.bolt2;

	if (body->s.weapon == WP_SABER && ent->client->ps.saberInFlight)
	{
		body->s.weapon = WP_BLASTER; //lie to keep from putting a saber on the corpse, because it was thrown at death
	}

	//Now doing this through a modified version of the rcg reliable command.
	if (ent->client && ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
	{
		islight = 1;
	}
	trap_SendServerCommand(-1, va("ircg %i %i %i %i", ent->s.number, body->s.number, body->s.weapon, islight));

	if (ent->s.number < MAX_CLIENTS && g_fixReconnectCorpses.integer) {
		// cram some extra information in the corpse's entitystate so that clients can fix the reconnect bug
		*(uint32_t *)&body->s.userInt1 = 0xFFFFFFFF;
		*(uint32_t *)&body->s.userInt2 = ((ent - g_entities) & 1) ? 0xFFFFFFFF : 0;
		*(uint32_t *)&body->s.userInt3 = ((ent - g_entities) & 2) ? 0xFFFFFFFF : 0;
		*(uint32_t *)&body->s.userFloat1 = ((ent - g_entities) & 4) ? 0xFFFFFFFF : 0;
		*(uint32_t *)&body->s.userFloat2 = ((ent - g_entities) & 8) ? 0xFFFFFFFF : 0;
		*(uint32_t *)&body->s.userFloat3 = ((ent - g_entities) & 16) ? 0xFFFFFFFF : 0;
		*(uint32_t *)&body->s.userVec1[0] = islight ? 0xFFFFFFFF : 0;
	}
	else {
		body->s.userInt1 = body->s.userInt2 = body->s.userInt3 = body->s.userFloat1 = body->s.userFloat2 = body->s.userFloat3 = body->s.userVec1[0] = 0;
	}

	body->r.svFlags = ent->r.svFlags | SVF_BROADCAST;
	VectorCopy (ent->r.mins, body->r.mins);
	VectorCopy (ent->r.maxs, body->r.maxs);
	VectorCopy (ent->r.absmin, body->r.absmin);
	VectorCopy (ent->r.absmax, body->r.absmax);

	body->s.torsoAnim = body->s.legsAnim = ent->client->ps.legsAnim;

	body->s.customRGBA[0] = ent->client->ps.customRGBA[0];
	body->s.customRGBA[1] = ent->client->ps.customRGBA[1];
	body->s.customRGBA[2] = ent->client->ps.customRGBA[2];
	body->s.customRGBA[3] = ent->client->ps.customRGBA[3];

	body->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	body->r.contents = CONTENTS_CORPSE;
	body->r.ownerNum = ent->s.number;

	body->nextthink = level.time + BODY_SINK_TIME;
	body->think = BodySink;

	body->die = body_die;

	body->takedamage = qtrue; // duo: removed gib setting this to qfalse bullshit

	VectorCopy ( body->s.pos.trBase, body->r.currentOrigin );
	trap_LinkEntity (body);

	return qtrue;
}

//======================================================================


/*
==================
SetClientViewAngle

==================
*/
void SetClientViewAngle( gentity_t *ent, vec3_t angle ) {
	int			i;

	// set the delta angle
	for (i=0 ; i<3 ; i++) {
		int		cmdAngle;

		cmdAngle = ANGLE2SHORT(angle[i]);
		ent->client->ps.delta_angles[i] = cmdAngle - ent->client->pers.cmd.angles[i];
	}
	VectorCopy( angle, ent->s.angles );
	VectorCopy (ent->s.angles, ent->client->ps.viewangles);
}

void MaintainBodyQueue(gentity_t *ent)
{ //do whatever should be done taking ragdoll and dismemberment states into account.
	qboolean doRCG = qfalse;

	assert(ent && ent->client);
	if (ent->client->tempSpectate > level.time ||
		(ent->client->ps.eFlags2 & EF2_SHIP_DEATH))
	{
		ent->client->noCorpse = qtrue;
	}

	if (!ent->client->noCorpse && !ent->client->ps.fallingToDeath)
	{
		if (!CopyToBodyQue (ent))
		{
			doRCG = qtrue;
		}
	}
	else
	{
		ent->client->noCorpse = qfalse; //clear it for next time
		ent->client->ps.fallingToDeath = qfalse;
		doRCG = qtrue;
	}

	if (doRCG)
	{ //bodyque func didn't manage to call ircg so call this to assure our limbs and ragdoll states are proper on the client.
		trap_SendServerCommand(-1, va("rcg %i", ent->s.clientNum));
	}
}

/*
================
respawn
================
*/
void SiegeRespawn(gentity_t *ent);
void respawn( gentity_t *ent ) {
	if (ent->client->ps.eFlags2&EF2_HELD_BY_MONSTER)
	{
		return;
	}
	MaintainBodyQueue(ent);

	if (gEscaping || g_gametype.integer == GT_POWERDUEL)
	{
		ent->client->sess.sessionTeam = TEAM_SPECTATOR;
		ent->client->sess.spectatorState = SPECTATOR_FREE;
		ent->client->sess.spectatorClient = 0;

		ent->client->pers.teamState.state = TEAM_BEGIN;
		ent->client->sess.spectatorTime = level.time;
		ClientSpawn(ent, qfalse);
		ent->client->iAmALoser = qtrue;
		return;
	}

	trap_UnlinkEntity (ent);

	if (g_gametype.integer == GT_SIEGE)
	{
		if (!g_siegeRespawn.integer || g_siegeRespawn.integer == 1) {
			SiegeRespawn(ent);
			UpdateNewmodSiegeTimers();
			return;
		}
        if ( (ent->client->tempSpectate <= level.time) && (level.siegeRespawnCheck >= level.time) ) {
			int minDel = g_siegeRespawn.integer* 2000;
			if (minDel < 20000)
			{
				minDel = 20000;
			}
			if (g_siegeRespawn.integer >= 10) {
				int killer = ent->client->sess.siegeStats.killer;
				if (level.siegeRespawnCheck >= level.time + ((g_siegeRespawn.integer * 1000) - 4000)) { // check for max
					ent->client->sess.siegeStats.maxed[GetSiegeStatRound()]++;
					if (killer >= 0 && killer < MAX_CLIENTS && killer != ent - g_entities && &g_entities[killer] && g_entities[killer].client) {
						g_entities[killer].client->sess.siegeStats.maxes[GetSiegeStatRound()]++;
						// check for tech max
						if (bgSiegeClasses[ent->client->siegeClass].playerClass == SPC_SUPPORT && ent->client->sess.sessionTeam == TEAM_BLUE && g_entities[killer].client->sess.sessionTeam == TEAM_RED) {
							char map[MAX_QPATH] = { 0 };
							trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
							if (map[0] && !Q_stricmpn(map, "mp/siege_hoth", 13))
								g_entities[killer].client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_HOTH_TECHMAX]++;
							else if (map[0] && !Q_stricmp(map, "siege_narshaddaa"))
								g_entities[killer].client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_NAR_TECHMAX]++;
						}
					}
				}
			}
			ent->client->sess.siegeStats.killer = -1;
			ent->client->tempSpectate = level.time + minDel;
			ent->health = ent->client->ps.stats[STAT_HEALTH] = 1;
			ent->client->ps.weapon = WP_NONE;
			ent->client->ps.stats[STAT_WEAPONS] = 0;
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
			ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
			ent->client->ps.eFlags = 0;
			ent->client->ps.saberMove = LS_NONE;
			ent->takedamage = qfalse;
			if (g_siegeGhosting.integer)
				ent->client->ps.eFlags |= EF_NODRAW; // prevent player model from bugging out for a nanosecond
			trap_LinkEntity(ent);
			// Respawn time.
			if ( ent->s.number < MAX_CLIENTS )
			{
				gentity_t *te = G_TempEntity( ent->client->ps.origin, EV_SIEGESPEC );
				te->s.time = level.siegeRespawnCheck;
				te->s.owner = ent->s.number;
			}
#ifdef NEWMOD_SUPPORT
			UpdateNewmodSiegeTimers();
#endif
			return;
		}
		SiegeRespawn(ent);
	}
	else
	{

		ClientSpawn(ent, qfalse);

		if (level.pause.state == PAUSE_NONE) {
			// add a teleportation effect
			gentity_t *tent;
			tent = G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_IN);
			tent->s.clientNum = ent->s.clientNum;
		}
	}
}

/*
================
TeamCount

Returns number of players on a team
================
*/
team_t TeamCount( int ignoreClientNum, int team ) {
	int		i;
	int		count = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( i == ignoreClientNum ) {
			continue;
		}
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( level.clients[i].sess.sessionTeam == team ) {
			count++;
		}
		else if (g_gametype.integer == GT_SIEGE &&
            level.clients[i].sess.siegeDesiredTeam == team)
		{
			count++;
		}
	}

	return count;
}

/*
================
TeamLeader

Returns the client number of the team leader
================
*/
int TeamLeader( int team ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( level.clients[i].sess.sessionTeam == team ) {
			if ( level.clients[i].sess.teamLeader )
				return i;
		}
	}

	return -1;
}


/*
================
PickTeam

================
*/
team_t PickTeam( int ignoreClientNum ) {
	int		counts[TEAM_NUM_TEAMS];

	counts[TEAM_BLUE] = TeamCount( ignoreClientNum, TEAM_BLUE );
	counts[TEAM_RED] = TeamCount( ignoreClientNum, TEAM_RED );

	if ( counts[TEAM_BLUE] > counts[TEAM_RED] ) {
		return TEAM_RED;
	}
	if ( counts[TEAM_RED] > counts[TEAM_BLUE] ) {
		return TEAM_BLUE;
	}
	// equal team count, so join the team with the lowest score
	if ( level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ) {
		return TEAM_RED;
	}
	return TEAM_BLUE;
}

#if 0
static qboolean IsEmpty(const char* name){
	const char* ptr;
	char        ch, nextch;

	ptr = name;

	while(*ptr){
		ch = *(ptr++);
		nextch = *ptr;

		if (ch == Q_COLOR_ESCAPE &&
			nextch <= '9' && nextch >= '0'){
			++ptr;
			continue;
		}

		if (ch>=0 && isspace(ch)){
			continue;
		}
		//special ISO 8859-1 A0 letter test, ugly i know :S
		if (ch == -96) {
			continue;
		}

		//we found proper character
		return qfalse;
	}


	return qtrue;
}
#endif

static qboolean IsSuffixed( const char *cleanName, int *suffixNum ) {
	char tmp[MAX_NETNAME], *s, *bracketL, *bracketR;
	int len;

	len = strlen( cleanName );

	// no left bracket at all, can't be suffixed
	if ( !(bracketL = strrchr( cleanName, '(' )) )
		return qfalse;

	//	0 1 2 3 4 5 6 7 8 9
	//	h,e,l,l,o, ,(,0,2,)
	//	            ^
	//	strlen = 10
	// the bracket is coming too early, it can't be a clientNum suffix
	if ( cleanName - bracketL < len - 3 ) {
#if 0//#ifdef _DEBUG
		Com_Printf( "DuplicateNames: bracket came too early, not removing suffix. [cleanName - bracketL](%d) < "
			"[len - 3]()\n", cleanName - bracketL, len - 3 );
#endif
		return qfalse;
	}

	// no right bracket, can't be a clientNum suffix
	if ( !(bracketR = strchr( bracketL, ')' )) ) {
		return qfalse;
	}

	// found the left bracket, let's get the number between here and the next bracket
	for ( s = bracketL; s < bracketR; s++ ) {
		// if it's not a number, discard it - the name is not suffixed
		if ( !isdigit( *s ) )
			return qfalse;

		tmp[s-bracketL] = *s;
	}

	if ( suffixNum )
		*suffixNum = atoi( tmp );

	return qtrue;
}

// add the "(02)" suffix, truncating if needed
static void AddSuffix( char *name, size_t nameLen, int clientNum ) {
	size_t len = strlen( name );
	const char *suffix = va( " ^7(^5%02i^7)", clientNum );
	const size_t suffixLen = strlen( suffix );
	if ( len + suffixLen < nameLen ) {
		Q_strcat( name, nameLen, suffix );
	}
	else {
		// not enough space, we'll have to truncate
		char *s = &name[nameLen - suffixLen - 1];
		Q_strncpyz( s, suffix, suffixLen+1 );
	}
}

// check if the specified client is trying to use a name that is already in use and add a clientNum suffix if so
qboolean CheckDuplicateName( int clientNum ) {
	char *name = g_entities[clientNum].client->pers.netname;
	const char *cleanName = g_entities[clientNum].client->pers.netnameClean;
	gentity_t *ent;
	int i;
	int suffixNum;

	// remove any potential suffix if it's not ours
	while ( IsSuffixed( cleanName, &suffixNum ) ) {
		// if the last suffix on our name is ours, everything is in order
		if ( clientNum == suffixNum )
			return qfalse;

		// remove this suffix and continue
#if 0//#ifdef _DEBUG
		G_DebugPrint(WL_WARNING, "Removing invalid name suffix on \"%s\"\n", G_PrintClient( clientNum ) );
#endif
		char *s = strrchr( name, '(' );
		if ( s-1 > name )
			s--;
		*s = '\0';
#if 0//#ifdef _DEBUG
		G_DebugPrint(WL_WARNING, "New name is \"%s\"\n", s );
#endif
	}

	// empty name? fall back to "Padawan"
	if ( strlen( name ) == 0u ) {
#if 0//#ifdef _DEBUG
		G_DebugPrint(WL_WARNING, "Falling back to default name for \"%s\"\n", G_PrintClient( clientNum ) );
#endif
		Q_strncpyz( name, DEFAULT_NAME, MAX_NETNAME );
	}

	// now check if someone else is using this name
	for ( i = 0, ent = g_entities; i<level.maxclients; i++, ent++ ) {
		if ( i == clientNum || !ent->inuse || ent->client->pers.connected == CON_DISCONNECTED ) {
			continue;
		}

		//TODO: perhaps do an advanced check replacing chars such as 'O' for '0'
		//	we are, after all, checking for impersonation
		if ( !Q_stricmp( ent->client->pers.netnameClean, cleanName ) ) {
			// we attempted to use this person's name, so add a suffix to ours
#if 0//#ifdef _DEBUG
			G_DebugPrint(WL_WARNING, "Adding suffix on name for \"%s\" because \"%s\" == \"%s\"\n", G_PrintClient( clientNum ),
				ent->client->pers.netnameClean, cleanName );
#endif
			AddSuffix( name, MAX_NETNAME, clientNum );
			return qtrue;
		}
	}

	return qfalse;
}

#define IsControlChar( c )		( c && *c && ( ( *c > 0 && *c < 32 ) || *c == 127 ) ) // control and DEL
#define IsWhitespace( c )		( c && *c && ( *c == 32 || *c == 255 ) ) // whitespace and nbsp
#define IsForbiddenChar( c )	( c && *c && *c == -84 ) // box character
#define IsVisibleChar( c )		( !IsControlChar( c ) && !IsWhitespace( c ) && !Q_IsColorString( c ) )

// Name normalizing and cleaning function that aims at making unique name representations so that
// you can just compare two strings to compare two names. The end goal is that if two names are VISUALLY
// equivalent, then they are also strictly equivalent internally, unlike basejka where, for example,
// ^1^7Padawan and Padawan display similarly while not being strictly equivalent. This makes it much
// easier to find duplicate names (simple strcmp) and to store names in databases in general.
// colorlessSize can be used as a smart way to limit the length of names, which is way too high in
// basejka and breaks scoreboard, while not limiting its color potential (it is basically the max name
// length when you don't count colors, so use MAX_NAME_LENGTH for basejka behavior, but 24 is recommended)
// -----------------------------------------------------------------------------------------------------------
// "player" -> "^7player": all names are ALWAYS prefixed with a color
// "*player" -> "^7*player": appending a color fixes the * problem for names in console by itself
// "longplayername" && colorlessSize = 4 -> "^7long"
// "     player     " -> "^7player": all enclosing spaces are stripped
// "pla     yer" -> "^7pla   yer": max 3 consecutive spaces
// "^1pla^2   yer" -> "^1pla   ^2yer": reports color codes before visible chars
// "^1^2play^3er^4" -> "^2play^3er": invisible color chars are stripped
// "^1    player    ^2" -> "^1player": strips colored invisible whitespaces
// "   ^7" -> "^7Padawan": empty names default to Padawan
// "play\ner" -> "^7player": filters control chars
// -----------------------------------------------------------------------------------------------------------
void NormalizeName( const char *in, char *out, int outSize, int colorlessSize ) {
	int i = 0, spaces = 0;
	char *p, pendingColorCode = '\0';
	qboolean metVisibleChar = qfalse;

	--outSize; // room for null byte

	for ( p = va( S_COLOR_WHITE"%s", in ); p && *p && i < outSize && colorlessSize > 0; ++p ) {
		// always ignore control chars
		if ( IsControlChar( p ) || IsForbiddenChar( p ) ) {
			continue;
		}

		// there could be spaces after the color, store it to write it later
		// this also makes colors override each other
		if ( Q_IsColorString( p ) ) {
			pendingColorCode = *++p;
			continue;
		}

		spaces += IsWhitespace( p ) ? 1 : -spaces;

		// basejka behaviour: no more than 3 consecutive spaces
		if ( spaces > 3 ) {
			continue;
		}

		if ( !metVisibleChar && IsVisibleChar( p ) ) {
			metVisibleChar = qtrue;
		}

		// everything after this will be actual writing: don't do it unless we have met the first visible char
		if ( !metVisibleChar ) {
			continue;
		}

		// only write pending colors before a char to prevent ambiguous spreading of color codes
		if ( pendingColorCode && IsVisibleChar( p ) && i < outSize - 3 ) { // 3 because we need an actual char after it
			*out++ = Q_COLOR_ESCAPE;
			*out++ = pendingColorCode;
			pendingColorCode = '\0';
			i += 2;
		}

		// write the current char
		*out++ = *p;
		++i; --colorlessSize;
	}

	// write null bytes after the last non whitespace
	spaces = Com_Clampi( 0, 3, spaces ); // no more than 3 chars are written
	*( out - spaces ) = '\0';

	// no char was written, use the default name
	if ( !i ) {
		Com_sprintf( out, outSize, DEFAULT_NAME );
	}
}

/*
===========
ClientCheckName
============
*/
static qboolean ClientCleanName( const char *in, char *out, int outSize ) {
	int		len, colorlessLen;
	char	ch;
	char	*p;
	int		spaces;

	//save room for trailing null byte
	outSize--;

	len = 0;
	colorlessLen = 0;
	p = out;
	*p = 0;
	spaces = 0;

	while( 1 ) {
		if (colorlessLen >= g_maxNameLength.integer) {
			break;
		}

		ch = *in++;
		if( !ch ) {
			break;
		}

		// don't allow leading spaces
		//TODO: check for leading spaces in visual name
		//so something like "^1 " will be treated properly
		if( !*p && ch == ' ' ) {
			continue;
		}

		// check colors
		if( ch == Q_COLOR_ESCAPE ) {
			// solo trailing carat is not a color prefix
			if( !*in ) {
				break;
			}

			// make sure room in dest for both chars
			if( len > outSize - 2 ) {
				break;
			}

			*out++ = ch;
			*out++ = *in++;
			len += 2;
			continue;
		}

		// don't allow too many consecutive spaces
		if( ch == ' ' ) {
			spaces++;
			if( spaces > 3 ) {
				continue;
			}
		}
		else {
			spaces = 0;
		}

		if( len > outSize - 1 ) {
			break;
		}

		*out++ = ch;
		++colorlessLen;
		len++;
	}
	*out = 0;

	//TODO: fix this, names like " ^7"
	//are still possible

	// strip trailing space
	len = strlen( out );
	while ( out[len-1] == ' ' ) {
		out[--len] = '\0';
	}

	// don't allow empty names
	if( *p == 0 || colorlessLen == 0 ) {
		Q_strncpyz( p, "Padawan", outSize );
		return qtrue;
	}

	return qfalse;
}

#if 0//#ifdef _DEBUG
void G_DebugWrite(const char *path, const char *text)
{
	fileHandle_t f;

	trap_FS_FOpenFile( path, &f, FS_APPEND );
	trap_FS_Write(text, strlen(text), f);
	trap_FS_FCloseFile(f);
}
#endif

qboolean G_SaberModelSetup(gentity_t *ent)
{
	int i = 0;
	qboolean fallbackForSaber = qtrue;

	while (i < MAX_SABERS)
	{
		if (ent->client->saber[i].model[0])
		{
			//first kill it off if we've already got it
			if (ent->client->weaponGhoul2[i])
			{
				trap_G2API_CleanGhoul2Models(&(ent->client->weaponGhoul2[i]));
			}
			trap_G2API_InitGhoul2Model(&ent->client->weaponGhoul2[i], ent->client->saber[i].model, 0, 0, -20, 0, 0);

			if (ent->client->weaponGhoul2[i])
			{
				int j = 0;
				char *tagName;
				int tagBolt;

				if (ent->client->saber[i].skin)
				{
					trap_G2API_SetSkin(ent->client->weaponGhoul2[i], 0, ent->client->saber[i].skin, ent->client->saber[i].skin);
				}

				if (ent->client->saber[i].saberFlags & SFL_BOLT_TO_WRIST)
				{
					trap_G2API_SetBoltInfo(ent->client->weaponGhoul2[i], 0, 3+i);
				}
				else
				{ // bolt to right hand for 0, or left hand for 1
					trap_G2API_SetBoltInfo(ent->client->weaponGhoul2[i], 0, i);
				}

				//Add all the bolt points
				while (j < ent->client->saber[i].numBlades)
				{
					tagName = va("*blade%i", j+1);
					tagBolt = trap_G2API_AddBolt(ent->client->weaponGhoul2[i], 0, tagName);

					if (tagBolt == -1)
					{
						if (j == 0)
						{ //guess this is an 0ldsk3wl saber
							tagBolt = trap_G2API_AddBolt(ent->client->weaponGhoul2[i], 0, "*flash");
							fallbackForSaber = qfalse;
							break;
						}

						if (tagBolt == -1)
						{
							assert(0);
							break;

						}
					}
					j++;

					fallbackForSaber = qfalse; //got at least one custom saber so don't need default
				}

				//Copy it into the main instance
				trap_G2API_CopySpecificGhoul2Model(ent->client->weaponGhoul2[i], 0, ent->ghoul2, i+1); 
			}
		}
		else
		{
			break;
		}

		i++;
	}

	return fallbackForSaber;
}

/*
===========
SetupGameGhoul2Model

There are two ghoul2 model instances per player (actually three).  One is on the clientinfo (the base for the client side 
player, and copied for player spawns and for corpses).  One is attached to the centity itself, which is the model acutally 
animated and rendered by the system.  The final is the game ghoul2 model.  This is animated by pmove on the server, and
is used for determining where the lightsaber should be, and for per-poly collision tests.
===========
*/
void *g2SaberInstance = NULL;

#include "namespace_begin.h"
qboolean BG_IsValidCharacterModel(const char *modelName, const char *skinName);
qboolean BG_ValidateSkinForTeam( const char *modelName, char *skinName, int team, float *colors );
void BG_GetVehicleModelName(char *modelname);
#include "namespace_end.h"

void SetupGameGhoul2Model(gentity_t *ent, char *modelname, char *skinName)
{
	int handle;
	char		afilename[MAX_QPATH];
#if 0
	char		/**GLAName,*/ *slash;
#endif
	char		GLAName[MAX_QPATH];
	vec3_t	tempVec = {0,0,0};

	// First things first.  If this is a ghoul2 model, then let's make sure we demolish this first.
	if (ent->ghoul2 && trap_G2_HaveWeGhoul2Models(ent->ghoul2))
	{
		trap_G2API_CleanGhoul2Models(&(ent->ghoul2));
	}

	//rww - just load the "standard" model for the server"
	if (!precachedKyle)
	{
		int defSkin;

		Com_sprintf( afilename, sizeof( afilename ), "models/players/kyle/model.glm" );
		handle = trap_G2API_InitGhoul2Model(&precachedKyle, afilename, 0, 0, -20, 0, 0);

		if (handle<0)
		{
			return;
		}

		defSkin = trap_R_RegisterSkin("models/players/kyle/model_default.skin");
		trap_G2API_SetSkin(precachedKyle, 0, defSkin, defSkin);
	}

	if (precachedKyle && trap_G2_HaveWeGhoul2Models(precachedKyle))
	{
		if (d_perPlayerGhoul2.integer || ent->s.number >= MAX_CLIENTS ||
			G_PlayerHasCustomSkeleton(ent))
		{ //rww - allow option for perplayer models on server for collision and bolt stuff.
			char modelFullPath[MAX_QPATH];
			char truncModelName[MAX_QPATH];
			char skin[MAX_QPATH];
			char vehicleName[MAX_QPATH];
			int skinHandle = 0;
			int i = 0;
			char *p;

			// If this is a vehicle, get it's model name.
			if ( ent->client->NPC_class == CLASS_VEHICLE )
			{
				strcpy(vehicleName, modelname);
				BG_GetVehicleModelName(modelname);
				strcpy(truncModelName, modelname);
				skin[0] = 0;
				if ( ent->m_pVehicle
					&& ent->m_pVehicle->m_pVehicleInfo
					&& ent->m_pVehicle->m_pVehicleInfo->skin
					&& ent->m_pVehicle->m_pVehicleInfo->skin[0] )
				{
					skinHandle = trap_R_RegisterSkin(va("models/players/%s/model_%s.skin", modelname, ent->m_pVehicle->m_pVehicleInfo->skin));
				}
				else
				{
					skinHandle = trap_R_RegisterSkin(va("models/players/%s/model_default.skin", modelname));
				}
			}
			else
			{
				if (skinName && skinName[0])
				{
					strcpy(skin, skinName);
					strcpy(truncModelName, modelname);
				}
				else
				{
					strcpy(skin, "default");

					strcpy(truncModelName, modelname);
					p = Q_strrchr(truncModelName, '/');

					if (p)
					{
						*p = 0;
						p++;

						while (p && *p)
						{
							skin[i] = *p;
							i++;
							p++;
						}
						skin[i] = 0;
						i = 0;
					}

					if (!BG_IsValidCharacterModel(truncModelName, skin))
					{
						strcpy(truncModelName, "kyle");
						strcpy(skin, "default");
					}

					if ( g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_SIEGE && !g_trueJedi.integer )
					{
						BG_ValidateSkinForTeam( truncModelName, skin, ent->client->sess.sessionTeam, NULL );
					}
					else if (g_gametype.integer == GT_SIEGE)
					{ //force skin for class if appropriate
						if (ent->client->siegeClass != -1)
						{
							siegeClass_t *scl = &bgSiegeClasses[ent->client->siegeClass];
							if (scl->forcedSkin[0])
							{
								strcpy(skin, scl->forcedSkin);
							}
						}
					}
				}
			}

			if (skin[0])
			{
				char *useSkinName;

				if (strchr(skin, '|'))
				{//three part skin
					useSkinName = va("models/players/%s/|%s", truncModelName, skin);
				}
				else
				{
					useSkinName = va("models/players/%s/model_%s.skin", truncModelName, skin);
				}

				skinHandle = trap_R_RegisterSkin(useSkinName);
			}

			strcpy(modelFullPath, va("models/players/%s/model.glm", truncModelName));
			handle = trap_G2API_InitGhoul2Model(&ent->ghoul2, modelFullPath, 0, skinHandle, -20, 0, 0);

			if (handle<0)
			{ //Huh. Guess we don't have this model. Use the default.

				if (ent->ghoul2 && trap_G2_HaveWeGhoul2Models(ent->ghoul2))
				{
					trap_G2API_CleanGhoul2Models(&(ent->ghoul2));
				}
				ent->ghoul2 = NULL;
				trap_G2API_DuplicateGhoul2Instance(precachedKyle, &ent->ghoul2);
			}
			else
			{
				trap_G2API_SetSkin(ent->ghoul2, 0, skinHandle, skinHandle);

				GLAName[0] = 0;
				trap_G2API_GetGLAName( ent->ghoul2, 0, GLAName);

				if (!GLAName[0] || (!strstr(GLAName, "players/_humanoid/") && ent->s.number < MAX_CLIENTS && !G_PlayerHasCustomSkeleton(ent)))
				{ //a bad model
					trap_G2API_CleanGhoul2Models(&(ent->ghoul2));
					ent->ghoul2 = NULL;
					trap_G2API_DuplicateGhoul2Instance(precachedKyle, &ent->ghoul2);
				}

				if (ent->s.number >= MAX_CLIENTS)
				{
					ent->s.modelGhoul2 = 1; //so we know to free it on the client when we're removed.

					if (skin[0])
					{ //append it after a *
						strcat( modelFullPath, va("*%s", skin) );
					}

					if ( ent->client->NPC_class == CLASS_VEHICLE )
					{ //vehicles are tricky and send over their vehicle names as the model (the model is then retrieved based on the vehicle name)
						ent->s.modelindex = G_ModelIndex(vehicleName);
					}
					else
					{
						ent->s.modelindex = G_ModelIndex(modelFullPath);
					}
				}
			}
		}
		else
		{
			trap_G2API_DuplicateGhoul2Instance(precachedKyle, &ent->ghoul2);
		}
	}
	else
	{
		return;
	}

	//Attach the instance to this entity num so we can make use of client-server
	//shared operations if possible.
	trap_G2API_AttachInstanceToEntNum(ent->ghoul2, ent->s.number, qtrue);

	// The model is now loaded.

	GLAName[0] = 0;

	if (!BGPAFtextLoaded)
	{
		if (BG_ParseAnimationFile("models/players/_humanoid/animation.cfg", bgHumanoidAnimations, qtrue) == -1)
		{
			Com_Printf( "Failed to load humanoid animation file\n");
			return;
		}
	}

	if (ent->s.number >= MAX_CLIENTS || G_PlayerHasCustomSkeleton(ent))
	{
		ent->localAnimIndex = -1;

		GLAName[0] = 0;
		trap_G2API_GetGLAName(ent->ghoul2, 0, GLAName);

		if (GLAName[0] &&
			!strstr(GLAName, "players/_humanoid/") /*&&
			!strstr(GLAName, "players/rockettrooper/")*/)
		{ //it doesn't use humanoid anims.
			char *slash = Q_strrchr( GLAName, '/' );
			if ( slash )
			{
				strcpy(slash, "/animation.cfg");

				ent->localAnimIndex = BG_ParseAnimationFile(GLAName, NULL, qfalse);
			}
		}
		else
		{ //humanoid index.
			if (strstr(GLAName, "players/rockettrooper/"))
			{
				ent->localAnimIndex = 1;
			}
			else
			{
				ent->localAnimIndex = 0;
			}
		}

		if (ent->localAnimIndex == -1)
		{
			Com_Error(ERR_DROP, "NPC had an invalid GLA\n");
		}
	}
	else
	{
		GLAName[0] = 0;
		trap_G2API_GetGLAName(ent->ghoul2, 0, GLAName);

		if (strstr(GLAName, "players/rockettrooper/"))
		{
			ent->localAnimIndex = 1;
		}
		else
		{
			ent->localAnimIndex = 0;
		}
	}

	if (ent->s.NPC_class == CLASS_VEHICLE &&
		ent->m_pVehicle)
	{ //do special vehicle stuff
		char strTemp[128];
		int i;

		// Setup the default first bolt
		i = trap_G2API_AddBolt( ent->ghoul2, 0, "model_root" );

		// Setup the droid unit.
		ent->m_pVehicle->m_iDroidUnitTag = trap_G2API_AddBolt( ent->ghoul2, 0, "*droidunit" );

		// Setup the Exhausts.
		for ( i = 0; i < MAX_VEHICLE_EXHAUSTS; i++ )
		{
			Com_sprintf( strTemp, 128, "*exhaust%i", i + 1 );
			ent->m_pVehicle->m_iExhaustTag[i] = trap_G2API_AddBolt( ent->ghoul2, 0, strTemp );
		}

		// Setup the Muzzles.
		for ( i = 0; i < MAX_VEHICLE_MUZZLES; i++ )
		{
			Com_sprintf( strTemp, 128, "*muzzle%i", i + 1 );
			ent->m_pVehicle->m_iMuzzleTag[i] = trap_G2API_AddBolt( ent->ghoul2, 0, strTemp );
			if ( ent->m_pVehicle->m_iMuzzleTag[i] == -1 )
			{//ergh, try *flash?
				Com_sprintf( strTemp, 128, "*flash%i", i + 1 );
				ent->m_pVehicle->m_iMuzzleTag[i] = trap_G2API_AddBolt( ent->ghoul2, 0, strTemp );
			}
		}

		// Setup the Turrets.
		for ( i = 0; i < MAX_VEHICLE_TURRET_MUZZLES; i++ )
		{
			if ( ent->m_pVehicle->m_pVehicleInfo->turret[i].gunnerViewTag )
			{
				ent->m_pVehicle->m_iGunnerViewTag[i] = trap_G2API_AddBolt( ent->ghoul2, 0, ent->m_pVehicle->m_pVehicleInfo->turret[i].gunnerViewTag );
			}
			else
			{
				ent->m_pVehicle->m_iGunnerViewTag[i] = -1;
			}
		}
	}
	
	if (ent->client->ps.weapon == WP_SABER || ent->s.number < MAX_CLIENTS)
	{ //a player or NPC saber user
		trap_G2API_AddBolt(ent->ghoul2, 0, "*r_hand");
		trap_G2API_AddBolt(ent->ghoul2, 0, "*l_hand");

		//rhand must always be first bolt. lhand always second. Whichever you want the
		//jetpack bolted to must always be third.
		trap_G2API_AddBolt(ent->ghoul2, 0, "*chestg");

		//claw bolts
		trap_G2API_AddBolt(ent->ghoul2, 0, "*r_hand_cap_r_arm");
		trap_G2API_AddBolt(ent->ghoul2, 0, "*l_hand_cap_l_arm");

		trap_G2API_SetBoneAnim(ent->ghoul2, 0, "model_root", 0, 12, BONE_ANIM_OVERRIDE_LOOP, 1.0f, level.time, -1, -1);
		trap_G2API_SetBoneAngles(ent->ghoul2, 0, "upper_lumbar", tempVec, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, NULL, 0, level.time);
		trap_G2API_SetBoneAngles(ent->ghoul2, 0, "cranium", tempVec, BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, POSITIVE_X, NULL, 0, level.time);

		if (!g2SaberInstance)
		{
			trap_G2API_InitGhoul2Model(&g2SaberInstance, "models/weapons2/saber/saber_w.glm", 0, 0, -20, 0, 0);

			if (g2SaberInstance)
			{
				// indicate we will be bolted to model 0 (ie the player) on bolt 0 (always the right hand) when we get copied
				trap_G2API_SetBoltInfo(g2SaberInstance, 0, 0);
				// now set up the gun bolt on it
				trap_G2API_AddBolt(g2SaberInstance, 0, "*blade1");
			}
		}

		if (G_SaberModelSetup(ent))
		{
			if (g2SaberInstance)
			{
				trap_G2API_CopySpecificGhoul2Model(g2SaberInstance, 0, ent->ghoul2, 1); 
			}
		}
	}

	if (ent->s.number >= MAX_CLIENTS)
	{ //some extra NPC stuff
		if (trap_G2API_AddBolt(ent->ghoul2, 0, "lower_lumbar") == -1)
		{ //check now to see if we have this bone for setting anims and such
			ent->noLumbar = qtrue;
		}
	}
}

unsigned long long int Q_strtoull(const char *str, char **endptr, int base) {
#ifdef WIN32
	return _strtoui64( str, endptr, base );
#else
	return strtoull( str, endptr, base );
#endif
}

qboolean PasswordMatches(const char *s) {
	return !strcmp(g_password.string, s) ||
		(sv_privatepassword.string[0] && !strcmp(sv_privatepassword.string, s));
}

// replaces "@@@" in a string with "   "
void PurgeStringedTrolling(char *in, char *out, int outSize) {
	if (!in || !in[0])
		return; // ???

	char fixed[MAX_STRING_CHARS] = { 0 };
	Q_strncpyz(fixed, in, sizeof(fixed));

	while (strstr(fixed, "@@@")) {
		char *at = strstr(fixed, "@@@");
		if (at && at[0]) {
			int i;
			for (i = 0; i < 3; i++)
				at[i] = ' '; // replace it with a space
		}
	}

	Q_strncpyz(out, fixed, outSize);
}

/*
===============
G_SendConfigstring

Creates and sends the server command necessary to update the CS index for the
given client
===============
*/
// if overrideCs is qtrue, inStr will *be* the new cs at that index; otherwise, it will simply be appended
static void G_SendConfigstring( int clientNum, int configstringNum, char *inStr, qboolean overrideCs )
{
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS )
		return;

	int maxChunkSize = MAX_STRING_CHARS - 24, len;
	char configstring[16384] = { 0 };
	if (overrideCs) {
		Q_strncpyz(configstring, inStr, sizeof(configstring));
	}
	else {
		trap_GetConfigstring(configstringNum, configstring, sizeof(configstring));

		if (!configstring[0]) {
			return; // fix: sometimes cs is empty, this prevents unpinning them client side
		}

		if (VALIDSTRING(inStr)) {
			Q_strcat(configstring, sizeof(configstring), inStr);
		}
	}

	len = strlen( configstring );

	if ( len >= maxChunkSize ) {
		int		sent = 0;
		int		remaining = len;
		char	*cmd;
		char	buf[MAX_STRING_CHARS];

		while ( remaining > 0 ) {
			if ( sent == 0 )
				cmd = "bcs0";
			else if ( remaining < maxChunkSize )
				cmd = "bcs2";
			else
				cmd = "bcs1";
			Q_strncpyz( buf, &configstring[sent], maxChunkSize );

			trap_SendServerCommand( clientNum, va( "%s %i \"%s\"\n", cmd, configstringNum, buf ) );

			sent += ( maxChunkSize - 1 );
			remaining -= ( maxChunkSize - 1 );
		}
	} else { // standard cs, just send it
		trap_SendServerCommand( clientNum, va( "cs %i \"%s\"\n", configstringNum, configstring ) );
	}
}

/*
===========
ClientUserInfoChanged

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call trap_SetUserinfo
if desired.
============
*/

qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride);
void G_ValidateSiegeClassForTeam(gentity_t *ent, int team);
void ClientUserinfoChanged( int clientNum, char *debugMsg ) {
	gentity_t *ent;
	int		teamTask, teamLeader, team, health;
	char	*s;
	char	model[MAX_QPATH];
	char	forcePowers[MAX_QPATH];
	char	oldname[MAX_STRING_CHARS];
	gclient_t	*client;
	char	c1[MAX_INFO_STRING];
	char	c2[MAX_INFO_STRING];
	char	userinfo[MAX_INFO_STRING];
	char	className[MAX_QPATH]; //name of class type to use in siege
	char	saberName[MAX_QPATH];
	char	saber2Name[MAX_QPATH];
	char	*value;
	int		maxHealth;
	qboolean	modelChanged = qfalse;

	ent = g_entities + clientNum;
	client = ent->client;

	trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

	// check for malformed or illegal info strings
	if ( !Info_Validate(userinfo) ) {
		strcpy (userinfo, "\\name\\badinfo");
	}

	//special check
	if ( (s = strchr(userinfo, '\n')) 
		|| (s = strchr(userinfo, '\r'))){
		G_HackLog("Client %i is sending illegal userinfo at index %i (%s).\n",
			clientNum,userinfo-s,userinfo);
		strcpy(userinfo, "\\name\\badinfo");
	}

	// check for local client
	s = Info_ValueForKey( userinfo, "ip" );
	if ( !strcmp( s, "localhost" ) ) {
		client->pers.localClient = qtrue;
	}

	// check the item prediction
	s = Info_ValueForKey( userinfo, "cg_predictItems" );
	if ( !atoi( s ) ) {
		client->pers.predictItemPickup = qfalse;
	} else {
		client->pers.predictItemPickup = qtrue;
	}

	char *nmVer = Info_ValueForKey(userinfo, "nm_ver");
	if (VALIDSTRING(nmVer))
		Q_strncpyz(client->sess.nmVer, nmVer, sizeof(client->sess.nmVer));
	else
		client->sess.nmVer[0] = '\0';

	s = Info_ValueForKey(userinfo, "nm_flags");
	if (VALIDSTRING(s)) {
		client->sess.disableShittySaberMoves = 0;
		if (strchr(s, 'a')) { client->sess.disableShittySaberMoves |= (1 << SHITTYSABERMOVE_BUTTERFLY); }
		if (strchr(s, 'b')) { client->sess.disableShittySaberMoves |= (1 << SHITTYSABERMOVE_STAB); }
		if (strchr(s, 'c')) { client->sess.disableShittySaberMoves |= (1 << SHITTYSABERMOVE_DFA); }
		if (strchr(s, 'd')) { client->sess.disableShittySaberMoves |= (1 << SHITTYSABERMOVE_LUNGE); }
		if (strchr(s, 'e')) { client->sess.disableShittySaberMoves |= (1 << SHITTYSABERMOVE_CARTWHEEL); }
		if (strchr(s, 'f')) { client->sess.disableShittySaberMoves |= (1 << SHITTYSABERMOVE_KATA); }
		if (strchr(s, 'g')) { client->sess.disableShittySaberMoves |= (1 << SHITTYSABERMOVE_KICK); }

		if (strchr(s, 'u')) {
			client->sess.unlagged |= UNLAGGED_CLIENTINFO;
			client->sess.unlagged &= ~UNLAGGED_COMMAND;
		}
		else {
			client->sess.unlagged &= ~UNLAGGED_CLIENTINFO;
		}
	}
	else {
		client->sess.unlagged &= ~UNLAGGED_CLIENTINFO;
		client->sess.disableShittySaberMoves = 0;
	}

	// passwordless spectators - check for password change
	s = Info_ValueForKey( userinfo, "password" );
	if (!client->sess.canJoin) {
		if (PasswordMatches(s)) {
			client->sess.canJoin = qtrue;

			trap_SendServerCommand(ent - g_entities,
				va("print \"^2You can now join the game.\n\""));
		}
	}

	// set name
	qboolean whitelisted = qtrue;
	if (g_lockdown.integer && !(ent->r.svFlags & SVF_BOT) && !G_ClientIsWhitelisted(clientNum)) {
		Q_strncpyz(oldname, va("Client %i", clientNum), sizeof(oldname));
		s = oldname;
		whitelisted = qfalse;
	}
	else {
		PurgeStringedTrolling(client->pers.netname, client->pers.netname, sizeof(client->pers.netname));
		Q_strncpyz(oldname, client->pers.netname, sizeof(oldname));
		s = Info_ValueForKey(userinfo, "name");
	}
	NormalizeName(s, client->pers.netname, sizeof(client->pers.netname), g_maxNameLength.integer);
	PurgeStringedTrolling(client->pers.netname, client->pers.netname, sizeof(client->pers.netname));
	//ClientCleanName(s, client->pers.netname, sizeof(client->pers.netname));

	Q_strncpyz(client->pers.netnameClean, client->pers.netname, sizeof(client->pers.netnameClean));
	Q_CleanString(client->pers.netnameClean, STRIP_COLOUR);

	if (CheckDuplicateName(clientNum)) {
		Q_strncpyz(client->pers.netnameClean, client->pers.netname, sizeof(client->pers.netnameClean));
		Q_CleanString(client->pers.netnameClean, STRIP_COLOUR);
	}

	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
			Q_strncpyz( client->pers.netname, "scoreboard", sizeof(client->pers.netname) );
		}
	}

	if ( whitelisted && (client->pers.connected == CON_CONNECTED ||	(client->pers.connected == CON_CONNECTING && oldname[0]!='\0'))) {
		if ( strcmp( oldname, client->pers.netname ) ) 
		{
			if ( client->pers.netnameTime > level.time  )
			{
				trap_SendServerCommand( clientNum, va("print \"You must wait %i seconds before changing your name again.\n\"", ((client->pers.netnameTime - level.time) / 1000)) );
				
				Info_SetValueForKey( userinfo, "name", oldname );
				trap_SetUserinfo( clientNum, userinfo );	
				strcpy ( client->pers.netname, oldname );
			}
			else
			{		
				trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s %s\n\"", oldname, G_GetStringEdString("MP_SVGAME", "PLRENAME"), client->pers.netname) );
				G_LogPrintf("Client num %i from %s renamed from '%s' to '%s'\n", clientNum,client->sess.ipString,
					oldname, client->pers.netname);
				client->pers.netnameTime = level.time + 700; //change time limit from 5s to 1s

                G_DBLogNickname( client->sess.ip, oldname, getGlobalTime() - client->sess.nameChangeTime, client->sess.auth == AUTHENTICATED ? client->sess.cuidHash : "");

                client->sess.nameChangeTime = getGlobalTime();

				//make heartbeat soon - accounts system
				//if (nextHeartBeatTime > level.time + 5000){
				//	nextHeartBeatTime = level.time + 5000;
				//	//G_DBLog("D1: nextHeartBeatTime: %i\n",nextHeartBeatTime);
				//}
			}
		}
	}

	// set model
	Q_strncpyz( model, Info_ValueForKey (userinfo, "model"), sizeof( model ) );

	if (d_perPlayerGhoul2.integer)
	{
		if (Q_stricmp(model, client->modelname))
		{
			strcpy(client->modelname, model);
			modelChanged = qtrue;
		}
	}

	//Get the skin RGB based on his userinfo
	value = Info_ValueForKey (userinfo, "char_color_red");
	if (value)
	{
		client->ps.customRGBA[0] = atoi(value);
	}
	else
	{
		client->ps.customRGBA[0] = 255;
	}

	value = Info_ValueForKey (userinfo, "char_color_green");
	if (value)
	{
		client->ps.customRGBA[1] = atoi(value);
	}
	else
	{
		client->ps.customRGBA[1] = 255;
	}

	value = Info_ValueForKey (userinfo, "char_color_blue");
	if (value)
	{
		client->ps.customRGBA[2] = atoi(value);
	}
	else
	{
		client->ps.customRGBA[2] = 255;
	}

	client->ps.customRGBA[3]=255;

	Q_strncpyz( forcePowers, Info_ValueForKey (userinfo, "forcepowers"), sizeof( forcePowers ) );

	// bots set their team a few frames later
	if (g_gametype.integer >= GT_TEAM && g_entities[clientNum].r.svFlags & SVF_BOT) {
		s = Info_ValueForKey( userinfo, "team" );
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			team = PickTeam( clientNum );
		}
	}
	else {
		team = client->sess.sessionTeam;
	}

	// detect assetsless clients (jkchat users)
	if (strcmp(client->pers.netname, "^7elo BOT") && !client->sess.nmVer[0] && !Q_stricmp(forcePowers, "7-1-032330000000001333") && !Q_stricmp(model, "kyle/default") &&
		!Q_stricmp(Info_ValueForKey(userinfo, "rate"), "25000") && !Q_stricmp(Info_ValueForKey(userinfo, "snaps"), "40")
#if 0
		&& !Q_stricmp(Info_ValueForKey(userinfo, "engine"), "jkclient") && !Q_stricmp(Info_ValueForKey(userinfo, "assets"), "0")
#endif
		) {

		client->sess.clientType = CLIENT_TYPE_JKCHAT;
	}
	else {
		client->sess.clientType = CLIENT_TYPE_NORMAL;
	}

	//Set the siege class
	if (g_gametype.integer == GT_SIEGE)
	{
		strcpy(className, client->sess.siegeClass);

		//This function will see if the given class is legal for the given team.
		//If not className will be filled in with the first legal class for this team.
		//Now that the team is legal for sure, we'll go ahead and get an index for it.
		client->siegeClass = BG_SiegeFindClassIndexByName(className);
		if (client->siegeClass == -1)
		{ //ok, get the first valid class for the team you're on then, I guess.
			BG_SiegeCheckClassLegality(team, className);
			strcpy(client->sess.siegeClass, className);
			client->siegeClass = BG_SiegeFindClassIndexByName(className);
		}
		else
		{ //otherwise, make sure the class we are using is legal.
			G_ValidateSiegeClassForTeam(ent, team);
			strcpy(className, client->sess.siegeClass);
		}

		if (((g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none")) || (g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none"))) &&
			g_joinMenuHack.integer && g_joinMenuHack.integer != 2) {
			if (client->siegeClass != -1) {
				siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];
				if (!Q_stricmp(level.mapname, "mp/siege_hoth")) {
					if (scl->playerClass == SPC_INFANTRY && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Imperial Snowtrooper", sizeof(className));
					else if (scl->playerClass == SPC_HEAVY_WEAPONS && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Rocket Trooper", sizeof(className));
					else if (scl->playerClass == SPC_DEMOLITIONIST && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Imperial Demolitionist", sizeof(className));
					else if (scl->playerClass == SPC_SUPPORT && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Imperial Tech", sizeof(className));
					else if (scl->playerClass == SPC_VANGUARD && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Imperial Sniper", sizeof(className));
					else if (scl->playerClass == SPC_JEDI && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Dark Jedi Invader", sizeof(className));
					else if (scl->playerClass == SPC_INFANTRY && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Rebel Infantry", sizeof(className));
					else if (scl->playerClass == SPC_HEAVY_WEAPONS && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Wookie", sizeof(className));
					else if (scl->playerClass == SPC_DEMOLITIONIST && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Rebel Demolitionist", sizeof(className));
					else if (scl->playerClass == SPC_SUPPORT && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Rebel Tech", sizeof(className));
					else if (scl->playerClass == SPC_VANGUARD && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Rebel Sniper", sizeof(className));
					else if (scl->playerClass == SPC_JEDI && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Jedi Guardian", sizeof(className));
				}
				else if (!Q_stricmp(level.mapname, "mp/siege_desert")) {
					if (scl->playerClass == SPC_INFANTRY && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Bounty Hunter", sizeof(className));
					else if (scl->playerClass == SPC_HEAVY_WEAPONS && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Merc Assault Specialist", sizeof(className));
					else if (scl->playerClass == SPC_DEMOLITIONIST && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Merc Demolitionist", sizeof(className));
					else if (scl->playerClass == SPC_SUPPORT && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Smuggler", sizeof(className));
					else if (scl->playerClass == SPC_VANGUARD && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Merc Sniper", sizeof(className));
					else if (scl->playerClass == SPC_JEDI && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Dark Side Marauder", sizeof(className));
					else if (scl->playerClass == SPC_INFANTRY && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Rebel Assault Specialist", sizeof(className));
					else if (scl->playerClass == SPC_HEAVY_WEAPONS && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Wookie", sizeof(className));
					else if (scl->playerClass == SPC_DEMOLITIONIST && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Rebel Demolitionist", sizeof(className));
					else if (scl->playerClass == SPC_SUPPORT && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Rebel Tech", sizeof(className));
					else if (scl->playerClass == SPC_VANGUARD && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Rebel Sniper", sizeof(className));
					else if (scl->playerClass == SPC_JEDI && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Jedi Duelist", sizeof(className));
				}
				else if (!Q_stricmp(level.mapname, "mp/siege_korriban")) {
					if (scl->playerClass == SPC_INFANTRY && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Dark Side Mauler", sizeof(className));
					else if (scl->playerClass == SPC_HEAVY_WEAPONS && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Dark Jedi Destroyer", sizeof(className));
					else if (scl->playerClass == SPC_DEMOLITIONIST && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Dark Jedi Demolitionist", sizeof(className));
					else if (scl->playerClass == SPC_SUPPORT && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Dark Jedi Tech", sizeof(className));
					else if (scl->playerClass == SPC_VANGUARD && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Dark Jedi Interceptor", sizeof(className));
					else if (scl->playerClass == SPC_JEDI && client->sess.siegeDesiredTeam == TEAM_BLUE)
						Q_strncpyz(className, "Dark Jedi Duelist", sizeof(className));
					else if (scl->playerClass == SPC_INFANTRY && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Jedi Warrior", sizeof(className));
					else if (scl->playerClass == SPC_HEAVY_WEAPONS && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Jedi Knight", sizeof(className));
					else if (scl->playerClass == SPC_DEMOLITIONIST && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Jedi Demolitionist", sizeof(className));
					else if (scl->playerClass == SPC_SUPPORT && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Jedi Healer", sizeof(className));
					else if (scl->playerClass == SPC_VANGUARD && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Jedi Scout", sizeof(className));
					else if (scl->playerClass == SPC_JEDI && client->sess.siegeDesiredTeam == TEAM_RED)
						Q_strncpyz(className, "Jedi Lightsaber Master", sizeof(className));
				}
			}
		}

		//Set the sabers if the class dictates
		if (client->siegeClass != -1)
		{
			siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];

			if (scl->saber1[0])
			{
				G_SetSaber(ent, 0, scl->saber1, qtrue);
			}
			else
			{ //default I guess
				G_SetSaber(ent, 0, "Kyle", qtrue);
			}
			if (scl->saber2[0])
			{
				G_SetSaber(ent, 1, scl->saber2, qtrue);
			}
			else
			{ //no second saber then
				G_SetSaber(ent, 1, "none", qtrue);
			}

			//make sure the saber models are updated
			G_SaberModelSetup(ent);

			if (scl->forcedModel[0])
			{ //be sure to override the model we actually use
				strcpy(model, scl->forcedModel);
				if (d_perPlayerGhoul2.integer)
				{
					if (Q_stricmp(model, client->modelname))
					{
						strcpy(client->modelname, model);
						modelChanged = qtrue;
					}
				}
			}

			//force them to use their class model on the server, if the class dictates
			if (G_PlayerHasCustomSkeleton(ent))
			{
				if (Q_stricmp(model, client->modelname) || ent->localAnimIndex == 0)
				{
					strcpy(client->modelname, model);
					modelChanged = qtrue;
				}
			}
		}
	}
	else
	{
		strcpy(className, "none");
	}

	//Set the saber name
	strcpy(saberName, client->sess.saberType);
	strcpy(saber2Name, client->sess.saber2Type);

	// set max health
	if (g_gametype.integer == GT_SIEGE && client->siegeClass != -1)
	{
		siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];
		maxHealth = 100;

		if (scl->maxhealth)
		{
			maxHealth = scl->maxhealth;
		}

		health = maxHealth;
	}
	else
	{
		maxHealth = 100;
		health = 100; 
	}
	client->pers.maxHealth = health;
	if ( client->pers.maxHealth < 1 || client->pers.maxHealth > maxHealth ) {
		client->pers.maxHealth = 100;
	}
	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;

	if (g_gametype.integer >= GT_TEAM) {
		client->pers.teamInfo = qtrue;
	} else {
		s = Info_ValueForKey( userinfo, "teamoverlay" );
		if ( ! *s || atoi( s ) != 0 ) {
			client->pers.teamInfo = qtrue;
		} else {
			client->pers.teamInfo = qfalse;
		}
	}

	// team task (0 = none, 1 = offence, 2 = defence)
	teamTask = atoi(Info_ValueForKey(userinfo, "teamtask"));
	// team Leader (1 = leader, 0 is normal player)
	teamLeader = client->sess.teamLeader;

	// colors
	strcpy(c1, Info_ValueForKey( userinfo, "color1" ));
	strcpy(c2, Info_ValueForKey( userinfo, "color2" ));

	// send over a subset of the userinfo keys so other clients can
	// print scoreboards, display models, and play custom sounds
	unsigned long long int totalHash = 0;
	if ( ent->r.svFlags & SVF_BOT ) {
		s = va("n\\%s\\t\\%i\\model\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\skill\\%s\\tt\\%d\\tl\\%d\\siegeclass\\%s\\st\\%s\\st2\\%s\\dt\\%i\\sdt\\%i",
			client->pers.netname, team, model,  c1, c2, 
			client->pers.maxHealth, client->sess.wins, client->sess.losses,
			Info_ValueForKey( userinfo, "skill" ), teamTask, teamLeader, className, saberName, saber2Name, client->sess.duelTeam, client->sess.siegeDesiredTeam );
	} else {
		// compute and send a uniqueid
		uint32_t guidHash = 0u;
		SHA1Context ctx;
		// openjk clients send over a (theoretically) consistent guid so we can track them across ip changes
		value = Info_ValueForKey( userinfo, "ja_guid" );
		if ( value && *value ) {
			// cool, use it, but also use ip in case this devious person reset their guid
			SHA1Reset( &ctx );
			SHA1Input( &ctx, (unsigned char *)value, (unsigned int)strlen(value) );
			if ( SHA1Result( &ctx ) == 1 ) {
				guidHash = ctx.Message_Digest[0];
				G_LogPrintf( "Client %d (OpenJK) reports guid %d (userinfo %s)\n", clientNum, guidHash, userinfo );
			}
		} else {
			// not an openjk client, so we should be able to force their cvars
			value = Info_ValueForKey( userinfo, "sex" );
			if ( value && *value && Q_isanumber( value ) ) {
				guidHash = atoi( value );
				G_LogPrintf( "Client %d reports guid %d (userinfo %s)\n", clientNum, guidHash, userinfo );
			} else {
				char previnfo[16384] = { 0 };
				qboolean prevGuid = qfalse;
				trap_GetConfigstring( CS_PLAYERS+clientNum, previnfo, sizeof( previnfo ) );
				if ( *previnfo ) {
					value = Info_ValueForKey( previnfo, "id" );
					if ( value && *value ) {
						// player previously had an id but the guid didn't stick, try to set it again
						char *endptr = NULL;
						uint64_t prevHash = Q_strtoull( value, &endptr, 10 );
						if ( *endptr == '\0' ) {
							if ( prevHash == ULLONG_MAX && errno == ERANGE ) {
								// parsed, but value was out of range
								G_LogPrintf( "Client %d's previous id was too long: %s\n", clientNum, value );
							} else {
								guidHash = prevHash & 0xFFFFFFFF;
								prevGuid = qtrue;
							}
						}
					}
				}
				if ( prevGuid ) {
					G_LogPrintf( "Reassigning previously assigned guid %d to client %d (userinfo %s)\n", guidHash, clientNum, userinfo );
				} else {
					guidHash = rand();
					G_LogPrintf( "Assigning random guid %d to client %d (userinfo %s)\n", guidHash, clientNum, userinfo );
				}

				// they only need to see it once for it to be set
				G_SendConfigstring( clientNum, CS_SYSTEMINFO, va( "\\sex\\%d", guidHash ), qfalse );
				G_SendConfigstring( clientNum, CS_SYSTEMINFO, NULL, qfalse );
			}
		}

		sha3_context c;
		sha3_Init512(&c);
		uint32_t ip = getIpFromString(client->sess.ipString, &ip);
		sha3_Update(&c, &ip, sizeof(ip));
		sha3_Update(&c, &level.pepper, PEPPER_CHARS);
		uint32_t ipHash;
		memcpy(&ipHash, sha3_Finalize(&c), sizeof(ipHash));
		uint64_t totalHash = ((uint64_t)ipHash) << 32 | (((uint64_t)guidHash) & 0xFFFFFFFF);

		level.clientUniqueIds[clientNum] = totalHash;
		G_LogPrintf( "Client %d (%s) has unique id %llu%s\n", clientNum, client->pers.netname, totalHash, G_ClientIsWhitelisted(clientNum) ? " (whitelisted)" : " (NOT whitelisted)");
		if (G_ClientIsOnProbation(clientNum)) {
			G_LogPrintf("Client %d (%s) is under probation\n", clientNum, client->pers.netname);
			if (!g_probation.integer)
				G_LogPrintf("g_probation is currently set to zero, so this client will be treated normally\n");
			else if (g_probation.integer >= 2)
				G_LogPrintf("g_probation is currently set >= 2, so this client will be treated under full probation\n");
			else
				G_LogPrintf("g_probation is currently non-zero but < 2, so this client will be treated under partial probation(can join without being forceteamed).\n");
		}
		if (g_gametype.integer == GT_SIEGE)
		{ //more crap to send
			s = va("n\\%s\\t\\%i\\model\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\tt\\%d\\tl\\%d\\siegeclass\\%s\\st\\%s\\st2\\%s\\dt\\%i\\sdt\\%i\\id\\%llu",
				client->pers.netname, client->sess.sessionTeam, model, c1, c2, 
				client->pers.maxHealth, client->sess.wins, client->sess.losses, teamTask, teamLeader, className, saberName, saber2Name, client->sess.duelTeam,
				client->sess.siegeDesiredTeam, totalHash);
		}
		else
		{
			s = va("n\\%s\\t\\%i\\model\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\tt\\%d\\tl\\%d\\st\\%s\\st2\\%s\\dt\\%i\\id\\%llu",
				client->pers.netname, client->sess.sessionTeam, model, c1, c2, 
				client->pers.maxHealth, client->sess.wins, client->sess.losses, teamTask, teamLeader, saberName, saber2Name, client->sess.duelTeam, totalHash);
		}
#ifdef NEWMOD_SUPPORT
		if ( client->sess.auth == AUTHENTICATED && client->sess.cuidHash[0] ) {
			s = va( "%s\\cid\\%s", s, client->sess.cuidHash );
		}
#endif
	}

	if (g_logClientInfo.integer) {
		static char lastClientInfo[MAX_CLIENTS][MAX_STRING_CHARS] = { 0 };
		if (!lastClientInfo[clientNum][0] || Q_stricmp(lastClientInfo[clientNum], s)) {
			G_LogPrintf("ClientUserinfoChanged: %i %s debug %s\n", clientNum, s, VALIDSTRING(debugMsg) ? debugMsg : "");
			Q_strncpyz(lastClientInfo[clientNum], s, sizeof(lastClientInfo[0]));
		}
	}

	if (g_gametype.integer == GT_SIEGE && g_delayClassUpdate.integer && !(ent->r.svFlags & SVF_BOT)) {
		trap_SetConfigstringNoUpdate(CS_PLAYERS + clientNum, s); // set it on the server without updating everyone (if possible)
		qboolean pregame = (level.inSiegeCountdown || level.siegeStage == SIEGESTAGE_PREROUND1 || level.siegeStage == SIEGESTAGE_PREROUND2) ? qtrue : qfalse;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (level.clients[i].pers.connected == CON_DISCONNECTED || g_entities[i].r.svFlags & SVF_BOT) {
				client->sess.lastInfoSent[i][0] = '\0'; // client i isn't valid anymore; clear out lastInfoSent from me to him
				continue;
			}

			if (OnOppositeTeam(client, &level.clients[i]) && (level.inSiegeCountdown || client->sess.spawnedSiegeClass[0] && client->sess.spawnedSiegeModel[0] && Q_stricmp(className, client->sess.spawnedSiegeClass))) {
				// this guy doesn't get to see our real class yet
				s = va("n\\%s\\t\\%i\\model\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\tt\\%d\\tl\\%d\\siegeclass\\%s\\st\\%s\\st2\\%s\\dt\\%i\\sdt\\%i\\id\\%llu",
					client->pers.netname, client->sess.sessionTeam, pregame ? "kyle" : client->sess.spawnedSiegeModel, c1, c2,
					pregame ? 100 : client->sess.spawnedSiegeMaxHealth, client->sess.wins, client->sess.losses, teamTask, teamLeader, pregame ? "" : client->sess.spawnedSiegeClass, pregame ? "Kyle" : saberName, pregame ? "none" : saber2Name, client->sess.duelTeam,
					client->sess.siegeDesiredTeam, totalHash);
			}
			else {
				// teammate or otherwise allowed to see real class
				s = va("n\\%s\\t\\%i\\model\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\tt\\%d\\tl\\%d\\siegeclass\\%s\\st\\%s\\st2\\%s\\dt\\%i\\sdt\\%i\\id\\%llu",
					client->pers.netname, client->sess.sessionTeam, model, c1, c2,
					client->pers.maxHealth, client->sess.wins, client->sess.losses, teamTask, teamLeader, className, saberName, saber2Name, client->sess.duelTeam,
					client->sess.siegeDesiredTeam, totalHash);
			}

#ifdef NEWMOD_SUPPORT
			if (client->sess.auth == AUTHENTICATED && client->sess.cuidHash[0]) {
				s = va("%s\\cid\\%s", s, client->sess.cuidHash);
			}
#endif

			// if the engine supports setting user info without immediately updating,
			// then we don't need to send another one to overwrite the one sent above
			if (qtrue) {
				if (client->sess.lastInfoSent[i][0] && !Q_stricmp(s, client->sess.lastInfoSent[i]))
					continue; // client i already got this exact info from us last time; don't bother
				Q_strncpyz(client->sess.lastInfoSent[i], s, sizeof(client->sess.lastInfoSent[i]));
			}

			// send whatever to client i
			G_SendConfigstring(i, CS_PLAYERS + clientNum, s, qtrue);
		}
	}
	else {
		// crappy server or gametype; just do normal behavior
		trap_SetConfigstring(CS_PLAYERS + clientNum, s);
	}

	if (modelChanged) //only going to be true for allowable server-side custom skeleton cases
	{ //update the server g2 instance if appropriate
		char *modelname = Info_ValueForKey (userinfo, "model");
		SetupGameGhoul2Model(ent, modelname, NULL);

		if (ent->ghoul2 && ent->client)
		{
			ent->client->renderInfo.lastG2 = NULL; //update the renderinfo bolts next update.
		}

		client->torsoAnimExecute = client->legsAnimExecute = -1;
		client->torsoLastFlip = client->legsLastFlip = qfalse;
	}
}

static int connectingClients = 0;

static qboolean isBaseMap( const char *s ) {
	       if ( !Q_stricmp( s, "mp/ctf1" ) ) {
	} else if ( !Q_stricmp( s, "mp/ctf2" ) ) {
	} else if ( !Q_stricmp( s, "mp/ctf3" ) ) {
	} else if ( !Q_stricmp( s, "mp/ctf4" ) ) {
	} else if ( !Q_stricmp( s, "mp/ctf5" ) ) {
	} else if ( !Q_stricmp( s, "mp/ffa1" ) ) {
	} else if ( !Q_stricmp( s, "mp/ffa2" ) ) {
	} else if ( !Q_stricmp( s, "mp/ffa3" ) ) {
	} else if ( !Q_stricmp( s, "mp/ffa4" ) ) {
	} else if ( !Q_stricmp( s, "mp/ffa5" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel1" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel2" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel3" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel4" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel5" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel6" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel7" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel8" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel9" ) ) {
	} else if ( !Q_stricmp( s, "mp/duel10" ) ) {
	} else if ( !Q_stricmp( s, "mp/siege_desert" ) ) {
	} else if ( !Q_stricmp( s, "mp/siege_hoth" ) ) {
	} else if ( !Q_stricmp( s, "mp/siege_korriban" ) ) {
	} else {
		return qfalse;
	}

	return qtrue;
}

extern void SetSiegeClass(gentity_t *ent, char *className);

/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/
char *ClientConnect(int clientNum, qboolean firstTime, qboolean isBot) {
	char *value;
	gclient_t *client;
	char		userinfo[MAX_INFO_STRING];
	gentity_t *ent;
	gentity_t *te;
	char		cleverFakeDetection[24];
	char        ipString[24] = { 0 };
	unsigned int ip = 0;
	int         port = 0;
	char		username[MAX_USERNAME_SIZE];
	static char reason[64];
	static char currentMap[MAX_MAP_NAME];
	int			sv_allowDownload;
	qboolean	hasSmod;
	qboolean	canJoinLater = qtrue;

	trap_Cvar_VariableStringBuffer("g_cleverFakeDetection", cleverFakeDetection, 24);
	ent = &g_entities[clientNum];
	if (firstTime && clientNum < MAX_CLIENTS) {
		level.clients[clientNum].sess.siegeFollowing.wasFollowing = qfalse;
	}

	trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

	// check to see if they are on the banned IP list
	value = Info_ValueForKey(userinfo, "ip");
	if (G_FilterPacket(value, reason, sizeof(reason)))
	{
		G_LogPrintf("Filtered client (%s) attempts to connect.\n", value);
		return reason;
	}

	*username = '\0';
	if (!(ent->r.svFlags & SVF_BOT) && !isBot) {
		if (firstTime) {
			Q_strncpyz(ipString, Info_ValueForKey(userinfo, "ip"), sizeof(ipString));
			ip = 0;
			getIpPortFromString(ipString, &ip, &port);
		}


		//if (isDBLoaded){ //accounts system

		//	if (firstTime){
		//		char* delimitator;
		//		char* reason;
		//		int status;

		//		value = Info_ValueForKey (userinfo, "password");

		//		delimitator = strchr(value,':');
		//		if (!delimitator){
		//			return "Invalid format. Use username:password.";
		//		}

		//		*delimitator = '\0'; //seperate username and password
		//		++delimitator;
		//		//value now points to username
		//		//delimitator now points to password

		//		status = getAuthorizationStatus(value, delimitator, ip, &reason);
		//		if (status == STATUS_WAITING){
		//			//Com_Printf("%s is NOT YET authorized.\n",value);
		//			//not yet authorized
		//			//enqueue
		//			enqueueAuthorization(value,delimitator);

		//			return "Authorization in progress...";
		//		} else if (status == STATUS_DENIED) {
		//			if (!reason){
		//				G_LogPrintf("DB_ERROR: no reason for denial.\n");
		//			}

		//			G_DBLog("Authorization denied for %s (%s). Reason:%s.\n",
		//				value,ipString,reason);
		//			//Com_Printf("%s is DENIED.\n",value);
		//			return va("Denied: %s",reason);				
		//		}

		//		G_DBLog("Authorization approved for %s (%s).\n",value,ipString);

		//		//authorized
		//		//Q_strncpyz(username,value,sizeof(username));
		//		//init session data should do that for us now

		//		//send heartbeat soon
		//		if (nextHeartBeatTime > level.time + 2000){
		//			nextHeartBeatTime = level.time + 2000;
		//			//G_DBLog("D2: nextHeartBeatTime: %i\n",nextHeartBeatTime);
		//		}

		//	}


		//} else

		if (g_needpass.integer || sv_passwordlessSpectators.integer) {// check for standard password
			static char sTemp[1024];

			value = Info_ValueForKey(userinfo, "password");

			if (!PasswordMatches(value))
			{
				if (!sv_passwordlessSpectators.integer) {
					Q_strncpyz(sTemp, G_GetStringEdString("MP_SVGAME", "INVALID_ESCAPE_TO_MAIN"), sizeof(sTemp));
					return sTemp;
				}
				else {
					// We allow passwordless clients, but don't let them join teams later
					canJoinLater = qfalse;
				}
			}
		}
	}

	//if ( !( ent->r.svFlags & SVF_BOT ) && !isBot && g_needpass.integer ) {
	//	// check for a password
	//	value = Info_ValueForKey (userinfo, "password");
	//	if ( g_password.string[0] && Q_stricmp( g_password.string, "none" ) &&
	//		strcmp( g_password.string, value) != 0) {
	//		static char sTemp[1024];
	//		Q_strncpyz(sTemp, G_GetStringEdString("MP_SVGAME","INVALID_ESCAPE_TO_MAIN"), sizeof (sTemp) );
	//		return sTemp;// return "Invalid password";
	//	}
	//}

	if (!(ent->r.svFlags & SVF_BOT) && !isBot && firstTime) {
		/* *CHANGE 40* anti q3 unban protection */
		if (!ip) {
			G_HackLog("Possible Q3unban: Someone is trying to connect with invalid IP (userinfo: %s).\n", userinfo);
			return "Invalid IP.";
		}

		/* *CHANGE 11* anti q3fill protections (also maxIPConnecting) */
		if (g_protectQ3Fill.integer) {
			gentity_t *otherEnt;
			int i, n;

			if ((strcmp(cleverFakeDetection, "none") != 0 && strcmp(cleverFakeDetection, "") != 0 && strcmp(Info_ValueForKey(userinfo, cleverFakeDetection), "") == 0)
				|| strcmp(Info_ValueForKey(userinfo, "cl_guid"), "") != 0)
			{
				// client doesn't have JKA specific info -> fake !
				G_HackLog("Possible Q3Fill attack: Client from %s is sending fake player.\n", ipString);
				return "Incorrect userinfo(C): fake player ?";
			}

			//additional anti q3fill test - is already someone from this ip connecting/connected?
			//we should accept only limits greater than 1 because someone can lost connection
			//and attempt to reconnecting
			if (g_protectQ3FillIPLimit.integer > 1)
			{
				otherEnt = g_entities;
				for (i = 0, n = 0; i < level.maxclients; ++i, ++otherEnt)
				{
					if ((otherEnt->client->pers.connected != CON_DISCONNECTED) &&
						(ip == otherEnt->client->sess.ip))
					{
						++n;
						if (n >= g_protectQ3FillIPLimit.integer)
						{
							G_HackLog("Possible Q3Fill attack: Client from %s is connecting more than %i times.\n", ipString, g_protectQ3FillIPLimit.integer);
							return "Reached limit for given IP, please try later.";
						}
					}
				}
			}

		}

		if (g_maxIPConnected.integer) {
			gentity_t *otherEnt;
			int i, n;

			otherEnt = g_entities;
			for (i = 0, n = 0; i < level.maxclients; ++i, ++otherEnt) {
				if ((otherEnt->client->pers.connected == CON_CONNECTING || (otherEnt->client->pers.connected == CON_CONNECTED))
					&& ip == otherEnt->client->sess.ip) {
					++n;
					if (n >= g_maxIPConnected.integer) {
						G_HackLog("Possible Q3Fill attack: Client from %s is connected more than %i times.\n", ipString, g_maxIPConnected.integer);
						return "Too many players with same IP already in game.";
					}
				}
			}
		}

#if 0
#ifdef NEWMOD_SUPPORT
		{
			// first newmod verification
			char *nm_ver = Info_ValueForKey(userinfo, "nm_ver");

			if (*nm_ver) {
				// Version filtering should be done here
				if (!strcmp(nm_ver, "1.0.0")) {
					return va("Newmod %s is banned, please update.", nm_ver);
				}
			}
		}
#endif
#endif
	}

	if (isBot) {
		if (!G_BotConnect(clientNum, !firstTime)) {
			return "BotConnectfailed";
		}

		ent->r.svFlags |= SVF_BOT;
		ent->inuse = qtrue;

		if (g_gametype.integer == GT_SIEGE) {
			int defaultSiegeClassNum = SSCN_SCOUT;
			if (g_botDefaultSiegeClass.string[0]) {
				switch (tolower((unsigned char)g_botDefaultSiegeClass.string[0])) {
				case 'a': defaultSiegeClassNum = SSCN_ASSAULT; break;
				case 'h': defaultSiegeClassNum = SSCN_HW; break;
				case 'd': defaultSiegeClassNum = SSCN_DEMO; break;
				case 't': defaultSiegeClassNum = SSCN_TECH; break;
				case 's': defaultSiegeClassNum = SSCN_SCOUT; break;
				case 'j': defaultSiegeClassNum = SSCN_JEDI; break;
				default: defaultSiegeClassNum = Q_irand(SSCN_ASSAULT, SSCN_JEDI); break; // random
				}
			}
			siegeClass_t *scl = BG_SiegeGetClass(ent->client->sess.siegeDesiredTeam, defaultSiegeClassNum);
			if (scl && VALIDSTRING(scl->name))
				SetSiegeClass(ent, scl->name);
		}
	}

	// force auto dl on non-openjk clients
	trap_Cvar_VariableStringBuffer("mapname", currentMap, sizeof(currentMap));
	sv_allowDownload = trap_Cvar_VariableIntegerValue("sv_allowDownload");
	hasSmod = Info_ValueForKey(userinfo, "smod_ver")[0] != '\0'; // TODO: move to session data if we need it somewhere else

	// fixme: default sv_allowdownload 1 is actually broken with this, since cl_allowDownload will always
	// be forced to 0. maybe remove SYSTEMINFO flags at runtime?
	if (sv_allowDownload >= 2 && !isBaseMap(currentMap)) {
		// If the client doesn't have SMod, download in all cases. With SMod, enabling basejka auto dl
		// disables fast autodl, so unless there is already someone connecting, don't enable it for them.
		// Ideally the check should be done client side, g_dlURL should override cl_allowDownload
		// SMod clients can still set cl_allowDownload 1 themselves if map is missing on g_dlURL
		if (!hasSmod || (hasSmod && connectingClients)) {
			trap_Cvar_Set("cl_allowDownload", "1");
			++connectingClients;
		}
	}

	// they can connect
	ent->client = level.clients + clientNum;
	client = ent->client;

	//assign the pointer for bg entity access
	ent->playerState = &ent->client->ps;

	clientSession_t	sessOld = client->sess;
	memset(client, 0, sizeof(*client));
	level.lastLegitClass[clientNum] = -1;
	memset(&level.tryChangeClass[clientNum], -1, sizeof(level.tryChangeClass[clientNum]));
	level.teamChangeTime[clientNum] = 0;
	client->sess = sessOld;

	if (firstTime) {
		value = Info_ValueForKey(userinfo, "qport");
		if (VALIDSTRING(value))
			client->sess.qport = atoi(value);
		else
			client->sess.qport = 0;
	}

	// country detection
	if (isBot) {
		client->sess.country[0] = '\0';
	}
	else if (firstTime) {
		client->sess.country[0] = '\0';
		trap_GetCountry(ipString, client->sess.country, sizeof(client->sess.country));
	}

	client->pers.connected = CON_CONNECTING;

	ent->forcedClass = 0;
	ent->forcedClassTime = 0;
	ent->funnyClassNumber = 0;

	// *CHANGE 8b* added clientNum to persistant data
	client->pers.clientNum = clientNum;

	// passwordless clients
	client->sess.canJoin = canJoinLater;

	// read or initialize the session data
	if ( firstTime || level.newSession ) 
    {
		G_InitSessionData( client, userinfo, isBot, firstTime );
	}

	//clean all players ignore flags for connecting guy
	if (firstTime){
		int i;
		for(i=0;i<level.maxclients;++i){
			if (!g_entities[i].inuse || !g_entities[i].client)
				continue;

			if (g_entities[i].client->sess.ignoreFlags == 0xFFFFFFFF){
				continue;
			}

			g_entities[i].client->sess.ignoreFlags &= ~(1 << clientNum);
		}
	}

	if (g_gametype.integer == GT_SIEGE &&
		(firstTime || level.newSession))
	{ //if this is the first time then auto-assign a desired siege team and show briefing for that team
		client->sess.siegeDesiredTeam = 0;
		//don't just show it - they'll see it if they switch to a team on purpose.
	}


	if (g_gametype.integer == GT_SIEGE && client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		if (firstTime || level.newSession)
		{ //start as spec
			client->sess.siegeDesiredTeam = client->sess.sessionTeam;
				if (!isBot)
				 {
				client->sess.sessionTeam = TEAM_SPECTATOR;
				}
		}
	}
	else if (g_gametype.integer == GT_POWERDUEL && client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		client->sess.sessionTeam = TEAM_SPECTATOR;
	}

	// get and distribute relevent paramters
	if (firstTime)
		G_LogPrintf( "ClientConnect: %i (%s) from %s%s\n", clientNum, Info_ValueForKey(userinfo, "name") , Info_ValueForKey(userinfo, "ip"), client->sess.country[0] ? va(" (%s)", client->sess.country) : "");
	ClientUserinfoChanged( clientNum, "2");

	// don't do the "xxx connected" messages if they were caried over from previous level
	if ( firstTime ) {
		if (g_printCountry.integer && client->sess.country[0])
			trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s (from %s)\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLCONNECT"), client->sess.country) );
		else
			trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " %s\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLCONNECT")));
	}

    if ( firstTime )
    {
        //int sessionId = G_LogDbLogSessionStart( client->sess.ip, client->sess.port, clientNum );
        //client->sess.sessionId = sessionId;
        ent->client->sess.nameChangeTime = getGlobalTime();
    }

	if ((g_gametype.integer >= GT_TEAM) &&
		(client->sess.sessionTeam != TEAM_SPECTATOR) &&
		firstTime)
	{
		BroadcastTeamChange( client, -1 );
	}

	// count current clients and rank for scoreboard
	CalculateRanks();

	te = G_TempEntity( vec3_origin, EV_CLIENTJOIN );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = clientNum;

	return NULL;
}

#ifdef NEWMOD_SUPPORT

void G_BroadcastServerFeatureList( int clientNum ) {
	static char commandListCmd[MAX_TOKEN_CHARS] =
		"kls -1 -1 cmds "
		"whois \"Displays current players' most-used names\" "
		"rules \"Displays server rules\" "
		"mappool \"Lists server map pools and their content\" "
		"ready \"Marks yourself as ready to be part of random teams\" "
		"join \"Changes Siege class on a specified team\" "
		"class \"Changes Siege class on current team\" "
		"classes \"Displays Siege class loadouts for the current map\" "
		"toptimes \"Displays fast capture records\" "
		"emote \"Performs a unique animation\" "
		"skillboost \"Displays which players are skillboosted\" "
		"senseboost \"Displays which players are senseboosted\" "
		"pugmaps \"Displays a list of maps you can vote for with /callvote newpug\" "
		"inkognito \"Hides whom you are following on the scoreboard\" "
		"serverstatus2 \"View additional server settings not listed in serverstatus\"";

	if (clientNum != -1) // we call this function with -1 clientNum to just do the global stuff
		trap_SendServerCommand(clientNum, commandListCmd);

	static qboolean didGlobal = qfalse;
	if (didGlobal)
		return;

	didGlobal = qtrue;

	static char featureListConfigString[MAX_TOKEN_CHARS] =
		"sfl "
		"oid "
		"esrw "
		"swpr "
		"tsgm "
		"sclb "
		"fqs "
		"ccid "
		"accs "
		"tnt2 "
		"cobt "
		"sccc "
		"isd2 "
		"sci "
		"vch2 "
		"vchl "
		"sgfl "
		"femp "
		"fsb "
		"fdfa ";

	// shlp is dependent on whether we have it for this map
	if (g_gametype.integer == GT_SIEGE) {
		if (!level.siegeHelpInitialized)
			InitializeSiegeHelpMessages();
		if (level.siegeHelpValid)
			Q_strcat(featureListConfigString, sizeof(featureListConfigString), "shlp ");
	}

	if (g_improvedHoming.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "rhom ");

	if (g_unlagged.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "unlg ");

	if (g_teamOverlayForce.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "tolf ");


	if (g_siegeGhosting.integer == 2)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "sgh2 ");
	else if (g_siegeGhosting.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "sgho ");

	if (g_allowMoveDisable.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "amd ");

	if (g_fixForceJumpAnimationLock.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "fjal ");

	if (g_fixVehicleTurbo.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "fvt ");

	if (g_fixSniperSwitch.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "fss ");

	if (g_fixReconnectCorpses.integer)
		Q_strcat(featureListConfigString, sizeof(featureListConfigString), "frc ");

	trap_SetConfigstring(CS_SERVERFEATURELIST, featureListConfigString);

	static char locationsListConfigString[MAX_TOKEN_CHARS] = { 0 };
	if (level.locations.enhanced.numUnique && !*locationsListConfigString) {
		Q_strncpyz(locationsListConfigString, "locs", sizeof(locationsListConfigString));
		for (int i = 0; i < level.locations.enhanced.numUnique; ++i) {
			Q_strcat(locationsListConfigString, sizeof(locationsListConfigString), va(" \"%s\" %d", level.locations.enhanced.data[i].message, level.locations.enhanced.data[i].teamowner));
		}
	}
	if (level.locations.enhanced.numUnique)
		trap_SetConfigstring(CS_ENHANCEDLOCATIONS, locationsListConfigString);

	qboolean gotCustomVote = qfalse;
	static char customVotesStr[MAX_TOKEN_CHARS] = "cuvo ";
	if (g_customVotes.integer) {
		for (int i = 0; i < MAX_CUSTOM_VOTES; i++) {
			// get the command
			char cmdBuf[256] = { 0 };
			trap_Cvar_VariableStringBuffer(va("g_customVote%d_command", i + 1), cmdBuf, sizeof(cmdBuf));
			if (!cmdBuf[0] || !Q_stricmp(cmdBuf, "0"))
				continue;
			Q_strstrip(cmdBuf, ";\"\n\r", NULL); // remove bad chars
			if (!cmdBuf[0])
				continue;

			// get the label
			char labelBuf[256] = { 0 };
			trap_Cvar_VariableStringBuffer(va("g_customVote%d_label", i + 1), labelBuf, sizeof(labelBuf));
			if (!labelBuf[0] || !Q_stricmp(labelBuf, "0"))
				continue;
			Q_strstrip(labelBuf, "\"\n\r", NULL); // remove bad chars
			if (!labelBuf[0])
				continue;

			// append to the overall string
			if (gotCustomVote)
				Q_strcat(customVotesStr, sizeof(customVotesStr), " "); // prepend a space if not the first one
			Q_strcat(customVotesStr, sizeof(customVotesStr), va("\"%s\" \"%s\"", cmdBuf, labelBuf));
			gotCustomVote = qtrue;
		}
	}
	trap_SetConfigstring(CS_CUSTOMVOTES, gotCustomVote ? customVotesStr : "");

	static char customObituariesString[MAX_TOKEN_CHARS] = "cobt ";
	Q_strcat(customObituariesString, sizeof(customObituariesString), va(
		"\"was sentry-bombed by\" %d "
		"\"sentry-bombed $\" %d ",
		CUSTOMOBITUARY_GENERIC_SENTRYBOMBED,
		CUSTOMOBITUARY_GENERIC_SENTRYBOMBED_SELF));
	if (level.siegeMap == SIEGEMAP_CARGO) {
		Q_strcat(customObituariesString, sizeof(customObituariesString), va(
			"\"was minced by\" %d "
			"\"minced $\" %d ",
			CUSTOMOBITUARY_CARGO_CHOPPED, CUSTOMOBITUARY_CARGO_CHOPPED_SELF));
	}
	else if (level.siegeMap == SIEGEMAP_URBAN) {
		Q_strcat(customObituariesString, sizeof(customObituariesString), va(
			"\"was dumpstered by\" %d "
			"\"dumpstered $\" %d "
			"\"was burned to a crisp by\" %d "
			"\"burned $ to a crisp\" %d ",
			CUSTOMOBITUARY_URBAN_DUMPSTERED, CUSTOMOBITUARY_URBAN_DUMPSTERED_SELF,
			CUSTOMOBITUARY_URBAN_BURNED, CUSTOMOBITUARY_URBAN_BURNED_SELF));
	}
	else if (level.siegeMap == SIEGEMAP_ANSION) {
		Q_strcat(customObituariesString, sizeof(customObituariesString), va(
			"\"was poisoned by\" %d "
			"\"poisoned $\" %d ",
			CUSTOMOBITUARY_ANSION_POISONED, CUSTOMOBITUARY_ANSION_POISONED_SELF));
	}
	trap_SetConfigstring(CS_CUSTOMOBITUARIES, customObituariesString);
}
#endif

#include "namespace_begin.h"
void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );
#include "namespace_end.h"

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
extern qboolean	gSiegeRoundBegun;
extern qboolean	gSiegeRoundEnded;
extern qboolean g_dontPenalizeTeam; //g_cmds.c
extern vmCvar_t g_testdebug;
extern int getGlobalTime();
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);
void ClientBegin( int clientNum, qboolean allowTeamReset ) {
	gentity_t	*ent;
	gclient_t	*client;
	gentity_t	*tent;
	int			flags, i;
	char		userinfo[MAX_INFO_VALUE], *modelname;

	memset(&level.siegeTopTimes[clientNum], 0, sizeof(level.siegeTopTimes[0]));

	ent = g_entities + clientNum;
#ifdef NEWMOD_SUPPORT
	UpdateNewmodSiegeTimers();
#endif
	if ((ent->r.svFlags & SVF_BOT) && g_gametype.integer >= GT_TEAM)
	{
		if (allowTeamReset)
		{
			const char *team = "Red";

			ent->client->sess.sessionTeam = PickTeam(-1);
			trap_GetUserinfo(clientNum, userinfo, MAX_INFO_STRING);

			if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			{
				ent->client->sess.sessionTeam = TEAM_RED;
			}

			if (ent->client->sess.sessionTeam == TEAM_RED)
			{
				team = "Red";
			}
			else
			{
				team = "Blue";
			}

			Info_SetValueForKey( userinfo, "team", team );

			trap_SetUserinfo( clientNum, userinfo );

			ent->client->ps.persistant[PERS_TEAM] = ent->client->sess.sessionTeam;

			ClientBegin(clientNum, qfalse);
			ClientUserinfoChanged(clientNum, "3");
			return;
		}
	}

	client = level.clients + clientNum;

	if ( ent->r.linked ) {
		trap_UnlinkEntity( ent );
	}
	G_InitGentity( ent );
	ent->touch = 0;
	ent->pain = 0;
	ent->client = client;

	//assign the pointer for bg entity access
	ent->playerState = &ent->client->ps;

	//RestoreDisconnectedPlayerData(ent);

	client->pers.connected = CON_CONNECTED;
	client->pers.enterTime = level.time;
	client->pers.teamState.state = TEAM_BEGIN;
	client->accuracy_hits = 0;
	client->accuracy_shots = 0;

	// save eflags around this, because changing teams will
	// cause this to happen with a valid entity, and we
	// want to make sure the teleport bit is set right
	// so the viewpoint doesn't interpolate through the
	// world to the new position
	flags = client->ps.eFlags;

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		if (ent->client->ps.fd.forcePowersActive & (1 << i))
		{
			WP_ForcePowerStop(ent, i);
		}
		i++;
	}

	i = TRACK_CHANNEL_1;

	while (i < NUM_TRACK_CHANNELS)
	{
		if (ent->client->ps.fd.killSoundEntIndex[i-50] && ent->client->ps.fd.killSoundEntIndex[i-50] < MAX_GENTITIES && ent->client->ps.fd.killSoundEntIndex[i-50] > 0)
		{
			G_MuteSound(ent->client->ps.fd.killSoundEntIndex[i-50], CHAN_VOICE);
		}
		i++;
	}
	i = 0;

	memset( &client->ps, 0, sizeof( client->ps ) );
	client->ps.eFlags = flags;

	client->ps.hasDetPackPlanted = qfalse;

	//first-time force power initialization
	WP_InitForcePowers( ent );

	//init saber ent
	WP_SaberInitBladeData( ent );

	//pending server commands overflow might cause that this
	//client is no longer valid, lets check it for it here and few other places
	if (!ent->inuse){
		return;
	}

	// First time model setup for that player.
	trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );
	modelname = Info_ValueForKey (userinfo, "model");
	SetupGameGhoul2Model(ent, modelname, NULL);

	if (ent->ghoul2 && ent->client)
	{
		ent->client->renderInfo.lastG2 = NULL; //update the renderinfo bolts next update.
	}

	if (g_gametype.integer == GT_POWERDUEL && client->sess.sessionTeam != TEAM_SPECTATOR &&
		client->sess.duelTeam == DUELTEAM_FREE)
	{
		SetTeam(ent, "s", qfalse);
	}
	else
	{
		if (g_gametype.integer == GT_SIEGE && (!gSiegeRoundBegun || gSiegeRoundEnded))
		{
			SetTeamQuick(ent, TEAM_SPECTATOR, qfalse);
			if (client->siegeClass == -1)
			{
				//duo: bugfix for being in spec with siegeDesiredTeam defaulting to 0 (TEAM_RED)
				client->sess.siegeDesiredTeam = TEAM_SPECTATOR;
			}
		}
        
		if ((ent->r.svFlags & SVF_BOT) &&
			g_gametype.integer != GT_SIEGE)
		{
			char *saberVal = Info_ValueForKey(userinfo, "saber1");
			char *saber2Val = Info_ValueForKey(userinfo, "saber2");

			if (!saberVal || !saberVal[0])
			{ //blah, set em up with a random saber
				int r = rand()%50;
				char sab1[1024];
				char sab2[1024];

				if (r <= 17)
				{
					strcpy(sab1, "Katarn");
					strcpy(sab2, "none");
				}
				else if (r <= 34)
				{
					strcpy(sab1, "Katarn");
					strcpy(sab2, "Katarn");
				}
				else
				{
					strcpy(sab1, "dual_1");
					strcpy(sab2, "none");
				}
				G_SetSaber(ent, 0, sab1, qfalse);
				G_SetSaber(ent, 0, sab2, qfalse);
				Info_SetValueForKey( userinfo, "saber1", sab1 );
				Info_SetValueForKey( userinfo, "saber2", sab2 );
				trap_SetUserinfo( clientNum, userinfo );
			}
			else
			{
				G_SetSaber(ent, 0, saberVal, qfalse);
			}

			if (saberVal && saberVal[0] &&
				(!saber2Val || !saber2Val[0]))
			{
				G_SetSaber(ent, 0, "none", qfalse);
				Info_SetValueForKey( userinfo, "saber2", "none" );
				trap_SetUserinfo( clientNum, userinfo );
			}
			else
			{
				G_SetSaber(ent, 0, saber2Val, qfalse);
			}
		}

        // update inactivity timer for proper team, must be done before client spawn
        if (client->sess.sessionTeam == TEAM_SPECTATOR){
            client->sess.inactivityTime = getGlobalTime() + 1 + g_spectatorInactivity.integer;
        }
        else {
            client->sess.inactivityTime = getGlobalTime() + 1 + g_inactivity.integer;
        }

		// locate ent at a spawn point
		ClientSpawn( ent, qfalse );
	}

	if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
		if (level.pause.state == PAUSE_NONE) {
			// send event
			tent = G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_IN);
			tent->s.clientNum = ent->s.clientNum;
		}

		if ( /*g_gametype.integer != GT_DUEL ||*/ g_gametype.integer == GT_POWERDUEL ) {
			trap_SendServerCommand( -1, va("print \"%s%s" S_COLOR_WHITE " %s\n\"", NM_SerializeUIntToColor(client - level.clients), client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLENTER")) );
		}
	}

	G_LogPrintf( "ClientBegin: %i (%s)\n", clientNum, ent->client->pers.netname );

	// count current clients and rank for scoreboard
	CalculateRanks();

	// client connected, update auto dl if he was the last one
	if ( connectingClients > 0 ) {
		--connectingClients;
		trap_Cvar_Set( "cl_allowDownload", connectingClients ? "1" : "0" );
	}

	G_ClearClientLog(clientNum);

#ifdef NEWMOD_SUPPORT
	G_BroadcastServerFeatureList( clientNum );
	UpdateNewmodSiegeClassLimits(clientNum);
	if (ent->client->sess.isInkognito)
		trap_SendServerCommand(ent - g_entities, "kls -1 -1 \"inko\" \"1\""); // only update if enabled

	if ( ent->client->sess.auth == PENDING ) {
		trap_SendServerCommand( clientNum, va( "kls -1 -1 \"clannounce\" %d \"%s\"", NM_AUTH_PROTOCOL, level.publicKey.keyHex ) );
		ent->client->sess.auth++;
#ifdef _DEBUG
		G_LogPrintf( "Sent clannounce packet to client %d\n", clientNum );
#endif
	}
#endif
}

static qboolean AllForceDisabled(int force)
{
	int i;

	if (force)
	{
		for (i=0;i<NUM_FORCE_POWERS;i++)
		{
			if (!(force & (1<<i)))
			{
				return qfalse;
			}
		}

		return qtrue;
	}

	return qfalse;
}

//Convenient interface to set all my limb breakage stuff up -rww
void G_BreakArm(gentity_t *ent, int arm)
{
	int anim = -1;

	assert(ent && ent->client);

	if (ent->s.NPC_class == CLASS_VEHICLE || ent->localAnimIndex > 1)
	{ //no broken limbs for vehicles and non-humanoids
		return;
	}

	if (!arm)
	{ //repair him
		ent->client->ps.brokenLimbs = 0;
		return;
	}

	if (ent->client->ps.fd.saberAnimLevel == SS_STAFF)
	{ //I'm too lazy to deal with this as well for now.
		return;
	}

	if (arm == BROKENLIMB_LARM)
	{
		if (ent->client->saber[1].model[0] &&
			ent->client->ps.weapon == WP_SABER &&
			!ent->client->ps.saberHolstered &&
			ent->client->saber[1].soundOff)
		{ //the left arm shuts off its saber upon being broken
			G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
		}
	}

	ent->client->ps.brokenLimbs = 0; //make sure it's cleared out
	ent->client->ps.brokenLimbs |= (1 << arm); //this arm is now marked as broken

	//Do a pain anim based on the side. Since getting your arm broken does tend to hurt.
	if (arm == BROKENLIMB_LARM)
	{
		anim = BOTH_PAIN2;
	}
	else if (arm == BROKENLIMB_RARM)
	{
		anim = BOTH_PAIN3;
	}

	if (anim == -1)
	{
		return;
	}

	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);

	//This could be combined into a single event. But I guess limbs don't break often enough to
	//worry about it.
	G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain25.wav") );
	//FIXME: A nice bone snapping sound instead if possible
	G_Sound(ent, CHAN_AUTO, G_SoundIndex( va("sound/player/bodyfall_human%i.wav", Q_irand(1, 3)) ));
}

//Update the ghoul2 instance anims based on the playerstate values
#include "namespace_begin.h"
qboolean BG_SaberStanceAnim( int anim );
qboolean PM_RunningAnim( int anim );
#include "namespace_end.h"
void G_UpdateClientAnims(gentity_t *self, float animSpeedScale)
{
	static int f;
	static int torsoAnim;
	static int legsAnim;
	static int firstFrame, lastFrame;
	static int aFlags;
	static float animSpeed, lAnimSpeedScale;
	qboolean setTorso = qfalse;

	torsoAnim = (self->client->ps.torsoAnim);
	legsAnim = (self->client->ps.legsAnim);

	if (self->client->ps.saberLockFrame)
	{
		trap_G2API_SetBoneAnim(self->ghoul2, 0, "model_root", self->client->ps.saberLockFrame, self->client->ps.saberLockFrame+1, BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND, animSpeedScale, level.time, -1, 150);
		trap_G2API_SetBoneAnim(self->ghoul2, 0, "lower_lumbar", self->client->ps.saberLockFrame, self->client->ps.saberLockFrame+1, BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND, animSpeedScale, level.time, -1, 150);
		trap_G2API_SetBoneAnim(self->ghoul2, 0, "Motion", self->client->ps.saberLockFrame, self->client->ps.saberLockFrame+1, BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND, animSpeedScale, level.time, -1, 150);
		return;
	}

	if (self->localAnimIndex > 1 &&
		bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame == 0 &&
		bgAllAnims[self->localAnimIndex].anims[legsAnim].numFrames == 0)
	{ //We'll allow this for non-humanoids.
		goto tryTorso;
	}

	if (self->client->legsAnimExecute != legsAnim || self->client->legsLastFlip != self->client->ps.legsFlip)
	{
		animSpeed = 50.0f / bgAllAnims[self->localAnimIndex].anims[legsAnim].frameLerp;
		lAnimSpeedScale = (animSpeed *= animSpeedScale);

		if (bgAllAnims[self->localAnimIndex].anims[legsAnim].loopFrames != -1)
		{
			aFlags = BONE_ANIM_OVERRIDE_LOOP;
		}
		else
		{
			aFlags = BONE_ANIM_OVERRIDE_FREEZE;
		}

		if (animSpeed < 0)
		{
			lastFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame;
			firstFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame + bgAllAnims[self->localAnimIndex].anims[legsAnim].numFrames;
		}
		else
		{
			firstFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame;
			lastFrame = bgAllAnims[self->localAnimIndex].anims[legsAnim].firstFrame + bgAllAnims[self->localAnimIndex].anims[legsAnim].numFrames;
		}

		aFlags |= BONE_ANIM_BLEND; //since client defaults to blend. Not sure if this will make much difference if any on server position, but it's here just for the sake of matching them.

		trap_G2API_SetBoneAnim(self->ghoul2, 0, "model_root", firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
		self->client->legsAnimExecute = legsAnim;
		self->client->legsLastFlip = self->client->ps.legsFlip;
	}

tryTorso:
	if (self->localAnimIndex > 1 &&
		bgAllAnims[self->localAnimIndex].anims[torsoAnim].firstFrame == 0 &&
		bgAllAnims[self->localAnimIndex].anims[torsoAnim].numFrames == 0)

	{ //If this fails as well just return.
		return;
	}
	else if (self->s.number >= MAX_CLIENTS &&
		self->s.NPC_class == CLASS_VEHICLE)
	{ //we only want to set the root bone for vehicles
		return;
	}

	if ((self->client->torsoAnimExecute != torsoAnim || self->client->torsoLastFlip != self->client->ps.torsoFlip) &&
		!self->noLumbar)
	{
		aFlags = 0;
		animSpeed = 0;

		f = torsoAnim;

		BG_SaberStartTransAnim(self->s.number, self->client->ps.fd.saberAnimLevel, self->client->ps.weapon, f, &animSpeedScale, self->client->ps.brokenLimbs);

		animSpeed = 50.0f / bgAllAnims[self->localAnimIndex].anims[f].frameLerp;
		lAnimSpeedScale = (animSpeed *= animSpeedScale);

		if (bgAllAnims[self->localAnimIndex].anims[f].loopFrames != -1)
		{
			aFlags = BONE_ANIM_OVERRIDE_LOOP;
		}
		else
		{
			aFlags = BONE_ANIM_OVERRIDE_FREEZE;
		}

		aFlags |= BONE_ANIM_BLEND; //since client defaults to blend. Not sure if this will make much difference if any on client position, but it's here just for the sake of matching them.

		if (animSpeed < 0)
		{
			lastFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame;
			firstFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame + bgAllAnims[self->localAnimIndex].anims[f].numFrames;
		}
		else
		{
			firstFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame;
			lastFrame = bgAllAnims[self->localAnimIndex].anims[f].firstFrame + bgAllAnims[self->localAnimIndex].anims[f].numFrames;
		}

		trap_G2API_SetBoneAnim(self->ghoul2, 0, "lower_lumbar", firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, /*firstFrame why was it this before?*/-1, 150);

		self->client->torsoAnimExecute = torsoAnim;
		self->client->torsoLastFlip = self->client->ps.torsoFlip;
		
		setTorso = qtrue;
	}

	if (setTorso &&
		self->localAnimIndex <= 1)
	{ //only set the motion bone for humanoids.
		trap_G2API_SetBoneAnim(self->ghoul2, 0, "Motion", firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
	}

#if 0 //disabled for now
	if (self->client->ps.brokenLimbs != self->client->brokenLimbs ||
		setTorso)
	{
		if (self->localAnimIndex <= 1 && self->client->ps.brokenLimbs &&
			(self->client->ps.brokenLimbs & (1 << BROKENLIMB_LARM)))
		{ //broken left arm
			char *brokenBone = "lhumerus";
			animation_t *armAnim;
			int armFirstFrame;
			int armLastFrame;
			int armFlags = 0;
			float armAnimSpeed;

			armAnim = &bgAllAnims[self->localAnimIndex].anims[ BOTH_DEAD21 ];
			self->client->brokenLimbs = self->client->ps.brokenLimbs;

			armFirstFrame = armAnim->firstFrame;
			armLastFrame = armAnim->firstFrame+armAnim->numFrames;
			armAnimSpeed = 50.0f / armAnim->frameLerp;
			armFlags = (BONE_ANIM_OVERRIDE_LOOP|BONE_ANIM_BLEND);

			trap_G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, armFirstFrame, armLastFrame, armFlags, armAnimSpeed, level.time, -1, 150);
		}
		else if (self->localAnimIndex <= 1 && self->client->ps.brokenLimbs &&
			(self->client->ps.brokenLimbs & (1 << BROKENLIMB_RARM)))
		{ //broken right arm
			char *brokenBone = "rhumerus";
			char *supportBone = "lhumerus";

			self->client->brokenLimbs = self->client->ps.brokenLimbs;

			//Only put the arm in a broken pose if the anim is such that we
			//want to allow it.
			if ((//self->client->ps.weapon == WP_MELEE ||
				self->client->ps.weapon != WP_SABER ||
				BG_SaberStanceAnim(self->client->ps.torsoAnim) ||
				PM_RunningAnim(self->client->ps.torsoAnim)) &&
				(!self->client->saber[1].model[0] || self->client->ps.weapon != WP_SABER))
			{
				int armFirstFrame;
				int armLastFrame;
				int armFlags = 0;
				float armAnimSpeed;
				animation_t *armAnim;

				if (self->client->ps.weapon == WP_MELEE ||
					self->client->ps.weapon == WP_SABER ||
					self->client->ps.weapon == WP_BRYAR_PISTOL)
				{ //don't affect this arm if holding a gun, just make the other arm support it
					armAnim = &bgAllAnims[self->localAnimIndex].anims[ BOTH_ATTACK2 ];

					armFirstFrame = armAnim->firstFrame+armAnim->numFrames;
					armLastFrame = armAnim->firstFrame+armAnim->numFrames;
					armAnimSpeed = 50.0f / armAnim->frameLerp;
					armFlags = (BONE_ANIM_OVERRIDE_LOOP|BONE_ANIM_BLEND);

					trap_G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, armFirstFrame, armLastFrame, armFlags, armAnimSpeed, level.time, -1, 150);
				}
				else
				{ //we want to keep the broken bone updated for some cases
					trap_G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
				}

				if (self->client->ps.torsoAnim != BOTH_MELEE1 &&
					self->client->ps.torsoAnim != BOTH_MELEE2 &&
					(self->client->ps.torsoAnim == TORSO_WEAPONREADY2 || self->client->ps.torsoAnim == BOTH_ATTACK2 || self->client->ps.weapon < WP_BRYAR_PISTOL))
				{
					//Now set the left arm to "support" the right one
					armAnim = &bgAllAnims[self->localAnimIndex].anims[ BOTH_STAND2 ];
					armFirstFrame = armAnim->firstFrame;
					armLastFrame = armAnim->firstFrame+armAnim->numFrames;
					armAnimSpeed = 50.0f / armAnim->frameLerp;
					armFlags = (BONE_ANIM_OVERRIDE_LOOP|BONE_ANIM_BLEND);

					trap_G2API_SetBoneAnim(self->ghoul2, 0, supportBone, armFirstFrame, armLastFrame, armFlags, armAnimSpeed, level.time, -1, 150);
				}
				else
				{ //we want to keep the support bone updated for some cases
					trap_G2API_SetBoneAnim(self->ghoul2, 0, supportBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
				}
			}
			else
			{ //otherwise, keep it set to the same as the torso
				trap_G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
				trap_G2API_SetBoneAnim(self->ghoul2, 0, supportBone, firstFrame, lastFrame, aFlags, lAnimSpeedScale, level.time, -1, 150);
			}
		}
		else if (self->client->brokenLimbs)
		{ //remove the bone now so it can be set again
			char *brokenBone = NULL;
			int broken = 0;

			//Warning: Don't remove bones that you've added as bolts unless you want to invalidate your bolt index
			//(well, in theory, I haven't actually run into the problem)
			if (self->client->brokenLimbs & (1<<BROKENLIMB_LARM))
			{
				brokenBone = "lhumerus";
				broken |= (1<<BROKENLIMB_LARM);
			}
			else if (self->client->brokenLimbs & (1<<BROKENLIMB_RARM))
			{ //can only have one arm broken at once.
				brokenBone = "rhumerus";
				broken |= (1<<BROKENLIMB_RARM);

				//want to remove the support bone too then
				trap_G2API_SetBoneAnim(self->ghoul2, 0, "lhumerus", 0, 1, 0, 0, level.time, -1, 0);
				trap_G2API_RemoveBone(self->ghoul2, "lhumerus", 0);
			}

			assert(brokenBone);

			//Set the flags and stuff to 0, so that the remove will succeed
			trap_G2API_SetBoneAnim(self->ghoul2, 0, brokenBone, 0, 1, 0, 0, level.time, -1, 0);

			//Now remove it
			trap_G2API_RemoveBone(self->ghoul2, brokenBone, 0);
			self->client->brokenLimbs &= ~broken;
		}
	}
#endif
}

static qboolean SaberStyleIsValidNew(gentity_t *ent, int style, int forceLevel) { // this is only intended for non-siege gametypes
	if (style == SS_STAFF || style == SS_DUAL) {
		if (style == SS_DUAL && ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
			return qtrue;
		if (style == SS_STAFF && ent->client->saber[0].saberFlags & SFL_TWO_HANDED)
			return qtrue;
		return qfalse;
	}

	if (!(g_balanceSaber.integer & SB_OFFENSE)) { // base mode
		if (style > forceLevel)
			return qfalse;
	} else if ( !( g_balanceSaber.integer & SB_OFFENSE_TAV_DES ) ) { // balanced/fixed mode, level 1+ allows fast, medium, and strong
		if (style == SS_DESANN || style == SS_TAVION)
			return qfalse;
	} else { // fun mode, level 2 allows desann stance and level 3 allows tavion stance
		if (forceLevel == 1 && (style == SS_DESANN || style == SS_TAVION)) // level 1, we only get normal styles
			return qfalse;
		if (forceLevel == 2 && style == SS_TAVION) // level 2, we get desann stance
			return qfalse;
	}


	return qtrue;
}

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/
extern qboolean WP_HasForcePowers( const playerState_t *ps );
void ClientSpawn(gentity_t *ent, qboolean forceUpdateInfo) {
	int					index;
	vec3_t				spawn_origin, spawn_angles;
	gclient_t			*client;
	int					i;
	clientPersistant_t	saved;
	clientSession_t		savedSess;
	int					persistant[MAX_PERSISTANT];
	gentity_t			*spawnPoint;
	int					flags, gameFlags;
	int					savedPing;
	int					accuracy_hits, accuracy_shots;
	int					eventSequence;
	char				userinfo[MAX_INFO_STRING];
	forcedata_t			savedForce;
	int					saveSaberNum = ENTITYNUM_NONE;
	int					wDisable = 0;
	int					savedSiegeIndex = 0;
	int					maxHealth;
	saberInfo_t			saberSaved[MAX_SABERS];
	int					l = 0;
	void				*g2WeaponPtrs[MAX_SABERS];
	char				*value;
	char				*saber;
	qboolean			changedSaber = qfalse;
	qboolean			inSiegeWithClass = qfalse;
	int					n;
	int					favweapon[16];
	qboolean			foundweapon = qfalse;
	int					numgoodweapons = 0;

	index = ent - g_entities;
	client = ent->client;

	client->fakeSpec = qfalse;
	ForceSiegeGhostUpdateForFollowers(ent - g_entities);

	if (index >= 0 && index < MAX_CLIENTS && client) {
		client->emoted = qfalse;
		if (g_gametype.integer == GT_SIEGE) {
			if (client->sess.sessionTeam == TEAM_BLUE) {
				level.siegeTopTimes[ent - g_entities].hasChangedTeams = qtrue;
				SpeedRunModeRuined("ClientSpawn: spawned on blue");
				client->sess.siegeFollowing.wasFollowing = qfalse;
			}
			else if (client->sess.sessionTeam == TEAM_RED) {
				client->sess.siegeFollowing.wasFollowing = qfalse;
			}
			int numOnRedTeam = 0;
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.sessionTeam == TEAM_RED)
					numOnRedTeam++;
			}
			if (numOnRedTeam > 2)
				SpeedRunModeRuined("ClientSpawn: spawned with >2 on red");
		}
		level.sentriesUsedThisLife[index] = 0;
	}

	if (ent->s.number < MAX_CLIENTS) {
		//duo: this stuff should be reset on spawn...
		ent->m_pVehicle = NULL;
		ent->s.owner = ENTITYNUM_NONE;
		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && !(ent->client->ps.pm_flags & PMF_FOLLOW))
			ent->s.eFlags &= ~EF_NODRAW;
	}

	if (ent->client->sess.siegeDuelInProgress)
	{
		ent->client->sess.siegeDuelInProgress = 0;
	}

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap_GetUserinfo( index, userinfo, sizeof(userinfo) );
	if (g_gametype.integer != GT_SIEGE) { // duo: we don't care what sabers you want in siege
		while (l < MAX_SABERS)
		{
			switch (l)
			{
			case 0:
				saber = &ent->client->sess.saberType[0];
				break;
			case 1:
				saber = &ent->client->sess.saber2Type[0];
				break;
			default:
				saber = NULL;
				break;
			}

			value = Info_ValueForKey(userinfo, va("saber%i", l + 1));
			if (saber &&
				value &&
				(Q_stricmp(value, saber) || !saber[l] || !ent->client->saber[l].model[0]))
			{ //doesn't match up (or our session saber is BS), we want to try setting it
				if (G_SetSaber(ent, l, value, qfalse))
				{
					//if (Q_stricmp(value, saber))
					{
						changedSaber = qtrue;
					}
				}
				else if (!saber[l] || !ent->client->saber[l].model[0])
				{ //Well, we still want to say they changed then (it means this is siege and we have some overrides)
					changedSaber = qtrue;
				}
			}
			l++;
		}
	}

	if (forceUpdateInfo)
		ClientUserinfoChanged(ent->s.number, "4");

	if (changedSaber)
	{ //make sure our new info is sent out to all the other clients, and give us a valid stance
		if (!forceUpdateInfo)
			ClientUserinfoChanged( ent->s.number, "5");

		//make sure the saber models are updated
		G_SaberModelSetup(ent);

		l = 0;
		while (l < MAX_SABERS)
		{ //go through and make sure both sabers match the userinfo
			switch (l)
			{
			case 0:
				saber = &ent->client->sess.saberType[0];
				break;
			case 1:
				saber = &ent->client->sess.saber2Type[0];
				break;
			default:
				saber = NULL;
				break;
			}

			value = Info_ValueForKey (userinfo, va("saber%i", l+1));

			if (Q_stricmp(value, saber))
			{ //they don't match up, force the user info
				Info_SetValueForKey(userinfo, va("saber%i", l+1), saber);
				trap_SetUserinfo( ent->s.number, userinfo );
			}
			l++;
		}

		if (ent->client->saber[0].model[0] &&
			ent->client->saber[1].model[0])
		{ //dual
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_DUAL;
		}
		else if ((ent->client->saber[0].saberFlags&SFL_TWO_HANDED))
		{ //staff
			ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_STAFF;
		}
		else
		{
			if (ent->client->sess.saberLevel < SS_FAST)
			{
				ent->client->sess.saberLevel = SS_FAST;
			}
			else if (g_gametype.integer == GT_SIEGE && ent->client->sess.saberLevel > SS_STRONG)
			{
				if (ent->client->sess.saberLevel == SS_DESANN || ent->client->sess.saberLevel == SS_TAVION)
				{ // allow spawning with desann/tavion stance if the class has it
					if (ent->client->siegeClass != -1 && !(bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << ent->client->sess.saberLevel)))
					{
						ent->client->sess.saberLevel = SS_STRONG;
					}
				}
				else
				{
					ent->client->sess.saberLevel = SS_STRONG;
				}
			}
			else if (g_gametype.integer != GT_SIEGE && ent->client->sess.saberLevel > SS_STRONG && !SaberStyleIsValidNew(ent, ent->client->sess.saberLevel, ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE]))
			{
				ent->client->sess.saberLevel = SS_STRONG;
			}
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel;
			if (g_gametype.integer != GT_SIEGE && !(g_balanceSaber.integer & SB_OFFENSE) && ent->client->ps.fd.saberAnimLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE])
			{
				ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
			}
		}
		if ( g_gametype.integer != GT_SIEGE )
		{
			//let's just make sure the styles we chose are cool
			if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) && !SaberStyleIsValidNew(ent, ent->client->ps.fd.saberAnimLevel, ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE]))
			{
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
				ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
			}
		}
	}
	l = 0;

	if (client->ps.fd.forceDoInit)
	{ //force a reread of force powers
		WP_InitForcePowers( ent );
		client->ps.fd.forceDoInit = 0;
	}

	if (ent->client->ps.fd.saberAnimLevel != SS_STAFF &&
		ent->client->ps.fd.saberAnimLevel != SS_DUAL &&
		ent->client->ps.fd.saberAnimLevel == ent->client->ps.fd.saberDrawAnimLevel &&
		ent->client->ps.fd.saberAnimLevel == ent->client->sess.saberLevel)
	{
		if (ent->client->sess.saberLevel < SS_FAST)
		{
			ent->client->sess.saberLevel = SS_FAST;
		}
		else if (g_gametype.integer == GT_SIEGE && ent->client->sess.saberLevel > SS_STRONG)
		{
			if (ent->client->sess.saberLevel == SS_DESANN || ent->client->sess.saberLevel == SS_TAVION)
			{ // allow spawning with desann/tavion stance if the class has it
				if (ent->client->siegeClass != -1 && !(bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << ent->client->sess.saberLevel)))
				{
					ent->client->sess.saberLevel = SS_STRONG;
				}
			}
			else
			{
				ent->client->sess.saberLevel = SS_STRONG;
			}
		}
		else if (g_gametype.integer != GT_SIEGE && ent->client->sess.saberLevel > SS_STRONG && !SaberStyleIsValidNew(ent, ent->client->sess.saberLevel, ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE]))
		{
			ent->client->sess.saberLevel = SS_STRONG;
		}
		ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel;

		if (g_gametype.integer != GT_SIEGE &&
			(ent->client->ps.fd.saberAnimLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE])
			&& ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE]
			&& !(g_balanceSaber.integer & SB_OFFENSE)
		    /*make sure we actually have sabers, this is fix for yellow stance going to blue after changing forcepowers to no saber cfg*/
			)
		{
			ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
		}
	}

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		spawnPoint = SelectSpectatorSpawnPoint ( 
						spawn_origin, spawn_angles);
	} else if (g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY) {
		// all base oriented team games use the CTF spawn points
		spawnPoint = SelectCTFSpawnPoint ( 
						client->sess.sessionTeam, 
						client->pers.teamState.state, 
						spawn_origin, spawn_angles);
	}
	else if (g_gametype.integer == GT_SIEGE)
	{
		spawnPoint = SelectSiegeSpawnPoint (
						client->siegeClass,
						client->sess.sessionTeam, 
						client->pers.teamState.state, 
						spawn_origin, spawn_angles);
	}
	else {
		do {
			if (g_gametype.integer == GT_POWERDUEL)
			{
				spawnPoint = SelectDuelSpawnPoint(client->sess.duelTeam, client->ps.origin, spawn_origin, spawn_angles);
			}
			else if (g_gametype.integer == GT_DUEL)
			{	// duel 
				spawnPoint = SelectDuelSpawnPoint(DUELTEAM_SINGLE, client->ps.origin, spawn_origin, spawn_angles);
			}
			else
			{
				// the first spawn should be at a good looking spot
				if ( !client->pers.initialSpawn && client->pers.localClient ) {
					client->pers.initialSpawn = qtrue;
					spawnPoint = SelectInitialSpawnPoint( spawn_origin, spawn_angles, client->sess.sessionTeam );
				} else {
					// don't spawn near existing origin if possible
					spawnPoint = SelectSpawnPoint ( 
						client->ps.origin, 
						spawn_origin, spawn_angles, client->sess.sessionTeam );
				}
			}

			// Tim needs to prevent bots from spawning at the initial point
			// on q3dm0...
			if ( ( spawnPoint->flags & FL_NO_BOTS ) && ( ent->r.svFlags & SVF_BOT ) ) {
				continue;	// try again
			}
			// just to be symetric, we have a nohumans option...
			if ( ( spawnPoint->flags & FL_NO_HUMANS ) && !( ent->r.svFlags & SVF_BOT ) ) {
				continue;	// try again
			}

			break;

		} while ( 1 );
	}
	client->pers.teamState.state = TEAM_ACTIVE;

	// toggle the teleport bit so the client knows to not lerp
	// and never clear the voted flag
	flags = ent->client->ps.eFlags & (EF_TELEPORT_BIT );
	flags ^= EF_TELEPORT_BIT;
	gameFlags = ent->client->mGameFlags & ( PSG_VOTED | PSG_TEAMVOTED | PSG_CANVOTE | PSG_TEAMVOTEDRED | PSG_TEAMVOTEDBLUE | PSG_CANTEAMVOTERED | PSG_CANTEAMVOTEBLUE);

	// clear everything but the persistant data

	G_ResetTrail(ent);

	saved = client->pers;
	savedSess = client->sess;
	savedPing = client->ps.ping;
	accuracy_hits = client->accuracy_hits;
	accuracy_shots = client->accuracy_shots;
	for ( i = 0 ; i < MAX_PERSISTANT ; i++ ) {
		persistant[i] = client->ps.persistant[i];
	}
	eventSequence = client->ps.eventSequence;

	savedForce = client->ps.fd;

	saveSaberNum = client->ps.saberEntityNum;

	savedSiegeIndex = client->siegeClass;

	l = 0;
	while (l < MAX_SABERS)
	{
		saberSaved[l] = client->saber[l];
		g2WeaponPtrs[l] = client->weaponGhoul2[l];
		l++;
	}

	i = 0;
	while (i < HL_MAX)
	{
		ent->locationDamage[i] = 0;
		i++;
	}

	memset (client, 0, sizeof(*client)); // bk FIXME: Com_Memset?
	client->bodyGrabIndex = ENTITYNUM_NONE;

	//Get the skin RGB based on his userinfo
	value = Info_ValueForKey (userinfo, "char_color_red");
	if (value)
	{
		client->ps.customRGBA[0] = atoi(value);
	}
	else
	{
		client->ps.customRGBA[0] = 255;
	}

	value = Info_ValueForKey (userinfo, "char_color_green");
	if (value)
	{
		client->ps.customRGBA[1] = atoi(value);
	}
	else
	{
		client->ps.customRGBA[1] = 255;
	}

	value = Info_ValueForKey (userinfo, "char_color_blue");
	if (value)
	{
		client->ps.customRGBA[2] = atoi(value);
	}
	else
	{
		client->ps.customRGBA[2] = 255;
	}

	client->ps.customRGBA[3]=255;

	client->siegeClass = savedSiegeIndex;

	l = 0;
	while (l < MAX_SABERS)
	{
		client->saber[l] = saberSaved[l];
		client->weaponGhoul2[l] = g2WeaponPtrs[l];
		l++;
	}

	//or the saber ent num
	client->ps.saberEntityNum = saveSaberNum;
	client->saberStoredIndex = saveSaberNum;

	client->ps.fd = savedForce;

	client->ps.duelIndex = ENTITYNUM_NONE;

	//spawn with 100
	client->ps.jetpackFuel = 100;
	client->ps.cloakFuel = 100;

	client->pers = saved;
	client->sess = savedSess;
	client->ps.ping = savedPing;
	client->accuracy_hits = accuracy_hits;
	client->accuracy_shots = accuracy_shots;
	client->lastkilled_client = -1;

	for ( i = 0 ; i < MAX_PERSISTANT ; i++ ) {
		client->ps.persistant[i] = persistant[i];
	}
	client->ps.eventSequence = eventSequence;
	// increment the spawncount so the client will detect the respawn
	client->ps.persistant[PERS_SPAWN_COUNT]++;
	client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;

	client->airOutTime = level.time + 12000;

	// set max health
	if (g_gametype.integer == GT_SIEGE && client->siegeClass != -1)
	{
		siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];
		maxHealth = 100;

		if (scl->maxhealth)
		{
			maxHealth = scl->maxhealth;
		}
	}
	else
	{
		maxHealth = 100;
	}
	client->pers.maxHealth = maxHealth;
	if ( client->pers.maxHealth < 1 || client->pers.maxHealth > maxHealth ) {
		client->pers.maxHealth = 100;
	}
	// clear entity values
	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
	client->ps.eFlags = flags;
	client->mGameFlags = gameFlags;

	ent->s.groundEntityNum = ENTITYNUM_NONE;
	ent->client = &level.clients[index];
	ent->playerState = &ent->client->ps;
	ent->takedamage = qtrue;
	ent->inuse = qtrue;
	ent->classname = "player";
	ent->r.contents = CONTENTS_BODY;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags = 0;
	
	VectorCopy (playerMins, ent->r.mins);
	VectorCopy (playerMaxs, ent->r.maxs);
	client->ps.crouchheight = CROUCH_MAXS_2;
	client->ps.standheight = DEFAULT_MAXS_2;

	client->ps.clientNum = index;

	//give default weapons
	client->ps.stats[STAT_WEAPONS] = ( 1 << WP_NONE );

	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
	{
		wDisable = g_duelWeaponDisable.integer;
	}
	else
	{
		wDisable = g_weaponDisable.integer;
	}

	if ( g_gametype.integer != GT_HOLOCRON 
		&& g_gametype.integer != GT_JEDIMASTER 
		&& !HasSetSaberOnly()
		&& !AllForceDisabled( g_forcePowerDisable.integer )
		&& g_trueJedi.integer )
	{
		if ( g_gametype.integer >= GT_TEAM && (client->sess.sessionTeam == TEAM_BLUE || client->sess.sessionTeam == TEAM_RED) )
		{//In Team games, force one side to be merc and other to be jedi
			if ( level.numPlayingClients > 0 )
			{//already someone in the game
				int		i, forceTeam = TEAM_SPECTATOR;
				for ( i = 0 ; i < level.maxclients ; i++ ) 
				{
					if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
						continue;
					}
					if ( level.clients[i].sess.sessionTeam == TEAM_BLUE || level.clients[i].sess.sessionTeam == TEAM_RED ) 
					{//in-game
						if ( WP_HasForcePowers( &level.clients[i].ps ) )
						{//this side is using force
							forceTeam = level.clients[i].sess.sessionTeam;
						}
						else
						{//other team is using force
							if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							{
								forceTeam = TEAM_RED;
							}
							else
							{
								forceTeam = TEAM_BLUE;
							}
						}
						break;
					}
				}
				if ( WP_HasForcePowers( &client->ps ) && client->sess.sessionTeam != forceTeam )
				{//using force but not on right team, switch him over
					const char *teamName = TeamName( forceTeam );
					SetTeam( ent, (char *)teamName , qfalse);
					return;
				}
			}
		}

		if ( WP_HasForcePowers( &client->ps ) )
		{
			client->ps.trueNonJedi = qfalse;
			client->ps.trueJedi = qtrue;
			//make sure they only use the saber
			client->ps.weapon = WP_SABER;
			client->ps.stats[STAT_WEAPONS] = (1 << WP_SABER);
		}
		else
		{//no force powers set
			client->ps.trueNonJedi = qtrue;
			client->ps.trueJedi = qfalse;
			if (!wDisable || !(wDisable & (1 << WP_BRYAR_PISTOL)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
			if (!wDisable || !(wDisable & (1 << WP_BLASTER)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BLASTER );
			}
			if (!wDisable || !(wDisable & (1 << WP_BOWCASTER)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BOWCASTER );
			}
			client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
			client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
			client->ps.ammo[AMMO_POWERCELL] = ammoData[AMMO_POWERCELL].max;
			client->ps.weapon = WP_BRYAR_PISTOL;
		}
	}
	else
	{//jediVmerc is incompatible with this gametype, turn it off!
		trap_Cvar_Set( "g_jediVmerc", "0" );
		if (g_gametype.integer == GT_HOLOCRON)
		{
			//always get free saber level 1 in holocron
			client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER );	//these are precached in g_items, ClearRegisteredItems()
		}
		else
		{
			if (client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE])
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER );	//these are precached in g_items, ClearRegisteredItems()
			}
			else
			{ //if you don't have saber attack rank then you don't get a saber
				client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
			}
		}

		if (g_gametype.integer != GT_SIEGE)
		{
			if (!wDisable || !(wDisable & (1 << WP_BRYAR_PISTOL)))
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
			else if (g_gametype.integer == GT_JEDIMASTER)
			{
				client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
		}

		if (g_gametype.integer == GT_JEDIMASTER)
		{
			client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
			client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
		}

		if (client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
		{
			client->ps.weapon = WP_SABER;
		}
		else if (client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL))
		{
			client->ps.weapon = WP_BRYAR_PISTOL;
		}
		else
		{
			client->ps.weapon = WP_MELEE;
		}
	}

	vmCvar_t	mapname;
	trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);

	if (g_gametype.integer == GT_SIEGE && client->siegeClass != -1 &&
		client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //well then, we will use a custom weaponset for our class
		int m = 0;

		client->ps.stats[STAT_WEAPONS] = bgSiegeClasses[client->siegeClass].weapons;
		if (!g_enableTrollItems.integer) {
			client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_NONE);
			client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_TURRET);
			client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_EMPLACED_GUN);
			if (client->sess.sessionTeam == TEAM_BLUE && client->ps.stats[STAT_WEAPONS] & (1 << WP_BOWCASTER) &&
				Q_stricmp(mapname.string, "siege_sillyroom")) {
				client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);
				client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BOWCASTER);
			}
		}

		if (Q_stricmp(mapname.string, "mp/siege_korriban") && g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none") && g_forceDTechItems.integer && client->sess.sessionTeam == TEAM_BLUE && bgSiegeClasses[client->siegeClass].playerClass == 2 &&
			((g_forceDTechItems.integer && (g_forceDTechItems.integer == 4 || g_forceDTechItems.integer == 6) ||
				(!Q_stricmpn(mapname.string, "mp/siege_hoth", 13) && g_forceDTechItems.integer && (g_forceDTechItems.integer == 1 || g_forceDTechItems.integer == 3 || g_forceDTechItems.integer == 7)))))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);
		}

		if (client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
		{
			client->ps.weapon = WP_SABER;
		}
		else if (client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL))
		{
			client->ps.weapon = WP_BRYAR_PISTOL;
		}
		else
		{
			client->ps.weapon = WP_MELEE;
		}
		inSiegeWithClass = qtrue;

		if ( g_gametype.integer == GT_SIEGE
			&& client->siegeClass != -1
			&& (bgSiegeClasses[client->siegeClass].classflags & (1 << CFL_EXTRA_AMMO)) )
		{//double ammo
			client->ps.eFlags |= EF_DOUBLE_AMMO;
		}


		value = Info_ValueForKey(userinfo, "prefer");
		if (value && strlen(value) == 15)
		{
			for (n = 0; n < 15; n++)
			{
				if (value[n] == 's' || value[n] == 'S')
				{
					favweapon[n] = WP_SABER;
					numgoodweapons++;
				}
				else if (value[n] == 'l' || value[n] == 'L')
				{
					favweapon[n] = WP_MELEE;
					numgoodweapons++;
				}
				else if (value[n] == 'p' || value[n] == 'P')
				{
					favweapon[n] = WP_BRYAR_PISTOL;
					numgoodweapons++;
				}
				else if (value[n] == 'y' || value[n] == 'Y')
				{
					favweapon[n] = WP_BRYAR_OLD;
					numgoodweapons++;
				}
				else if (value[n] == 'e' || value[n] == 'E')
				{
					favweapon[n] = WP_BLASTER;
					numgoodweapons++;
				}
				else if (value[n] == 'u' || value[n] == 'U')
				{
					favweapon[n] = WP_DISRUPTOR;
					numgoodweapons++;
				}
				else if (value[n] == 'b' || value[n] == 'B')
				{
					favweapon[n] = WP_BOWCASTER;
					numgoodweapons++;
				}
				else if (value[n] == 'i' || value[n] == 'I')
				{
					favweapon[n] = WP_REPEATER;
					numgoodweapons++;
				}
				else if (value[n] == 'd' || value[n] == 'D')
				{
					favweapon[n] = WP_DEMP2;
					numgoodweapons++;
				}
				else if (value[n] == 'g' || value[n] == 'G')
				{
					favweapon[n] = WP_FLECHETTE;
					numgoodweapons++;
				}
				else if (value[n] == 'r' || value[n] == 'R')
				{
					favweapon[n] = WP_ROCKET_LAUNCHER;
					numgoodweapons++;
				}
				else if (value[n] == 'c' || value[n] == 'C')
				{
					favweapon[n] = WP_CONCUSSION;
					numgoodweapons++;
				}
				else if (value[n] == 't' || value[n] == 'T')
				{
					favweapon[n] = WP_THERMAL;
					numgoodweapons++;
				}
				else if (value[n] == 'm' || value[n] == 'M')
				{
					favweapon[n] = WP_TRIP_MINE;
					numgoodweapons++;
				}
				else if (value[n] == 'k' || value[n] == 'K')
				{
					favweapon[n] = WP_DET_PACK;
					numgoodweapons++;
				}
				else if (value[n] == '\t' || value[n] == '\n' || value[n] == '\v' || value[n] == '\f' || value[n] == '\r') //detect sneaky mofos
				{
					G_HackLog("Client from %s is sending illegal preferred weapons list.\n", client->sess.ipString);
					numgoodweapons = 0;
					continue;
				}
				else
				{
					numgoodweapons = 0;
					continue;
				}
			}
		}
		else
		{
			numgoodweapons = 0;
		}


		while (m < WP_NUM_WEAPONS)
		{
			if (client->ps.stats[STAT_WEAPONS] & (1 << m))
			{
				if (numgoodweapons == 15)
				{
					for (n = 0; n < 15 && foundweapon == qfalse; n++)
					{
						if (foundweapon == qfalse && client->ps.stats[STAT_WEAPONS] & (1 << favweapon[n]))
						{
							client->ps.weapon = favweapon[n];
							foundweapon = qtrue;
						}
					}
				}
				if (foundweapon == qfalse && client->ps.weapon != WP_SABER)
				{ //try to find the highest ranking weapon we have
					if (m > client->ps.weapon)
					{
						client->ps.weapon = m;
					}
				}

				if (m >= WP_BRYAR_PISTOL)
				{ //Max his ammo out for all the weapons he has.
					if (g_gametype.integer == GT_SIEGE && m == WP_ROCKET_LAUNCHER && !bgSiegeClasses[ent->client->siegeClass].ammorockets && !(bgSiegeClasses[ent->client->siegeClass].classflags & (1 << CFL_SINGLE_ROCKET)))
					{
						if (bgSiegeClasses[ent->client->siegeClass].classflags & (1 << CFL_EXTRA_AMMO) && !Q_stricmp(mapname.string, "siege_cargobarge"))
							//siege_cargobarge (the original one) needs a manual override due to oink being dumb
							Add_Ammo(ent, AMMO_ROCKETS, 20);
						else
							Add_Ammo(ent, AMMO_ROCKETS, 10);
					}
					else
					{
						Add_Max_Ammo(ent, weaponData[m].ammoIndex);
					}
				}
			}
			m++;
		}
	}

	if (g_gametype.integer == GT_SIEGE &&
		client->siegeClass != -1 &&
		client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //use class-specified inventory
		client->ps.stats[STAT_HOLDABLE_ITEMS] = bgSiegeClasses[client->siegeClass].invenItems;
		if (!g_enableTrollItems.integer) {
			client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_CLOAK);
			client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_BINOCULARS);
		}
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
		if (Q_stricmp(mapname.string, "mp/siege_korriban") && g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none") && g_forceDTechItems.integer && client->sess.sessionTeam == TEAM_BLUE && bgSiegeClasses[client->siegeClass].playerClass == 2 &&
			((g_forceDTechItems.integer == 5 || g_forceDTechItems.integer == 6 || g_forceDTechItems.integer == 7)
				|| (!Q_stricmpn(mapname.string, "mp/siege_hoth", 13) && (g_forceDTechItems.integer == 2 || g_forceDTechItems.integer == 3))))
		{
			client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);
		}
		if (!Q_stricmp(mapname.string, "siege_cargobarge2") && client->sess.sessionTeam == TEAM_BLUE && bgSiegeClasses[client->siegeClass].playerClass == SPC_SUPPORT && (!g_blueTeam.string || !g_blueTeam.string[0] || g_blueTeam.string[0] == '0' || !Q_stricmpn(g_blueTeam.string, "none", 4))) {
			client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC); //duo: hacky fix to remove unintentional bacta from cargo2 d tech without me having to update server pk3
		}
	}
	else
	{
		client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

	if (g_gametype.integer == GT_SIEGE &&
		client->siegeClass != -1 &&
		bgSiegeClasses[client->siegeClass].powerups &&
		client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //this class has some start powerups
		i = 0;
		while (i < PW_NUM_POWERUPS)
		{
			if (bgSiegeClasses[client->siegeClass].powerups & (1 << i))
			{
				client->ps.powerups[i] = Q3_INFINITE;
			}
			i++;
		}
	}

	if ( client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		client->ps.stats[STAT_WEAPONS] = 0;
		client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

// nmckenzie: DESERT_SIEGE... or well, siege generally.  This was over-writing the max value, which was NOT good for siege.
	if ( inSiegeWithClass == qfalse )
	{
		client->ps.ammo[AMMO_BLASTER] = 100; 
	}

	client->ps.rocketLockIndex = ENTITYNUM_NONE;
	client->ps.rocketLockTime = 0;

	//rww - Set here to initialize the circling seeker drone to off.
	//A quick note about this so I don't forget how it works again:
	//ps.genericEnemyIndex is kept in sync between the server and client.
	//When it gets set then an entitystate value of the same name gets
	//set along with an entitystate flag in the shared bg code. Which
	//is why a value needs to be both on the player state and entity state.
	//(it doesn't seem to just carry over the entitystate value automatically
	//because entity state value is derived from player state data or some
	//such)
	client->ps.genericEnemyIndex = -1;

	client->ps.isJediMaster = qfalse;

	if (client->ps.fallingToDeath)
	{
		client->ps.fallingToDeath = 0;
		client->noCorpse = qtrue;
	}

	//Do per-spawn force power initialization
	WP_SpawnInitForcePowers( ent );

	// health will count down towards max_health
	if (g_gametype.integer == GT_SIEGE &&
		client->siegeClass != -1 &&
		bgSiegeClasses[client->siegeClass].starthealth)
	{ //class specifies a start health, so use it
		ent->health = client->ps.stats[STAT_HEALTH] = bgSiegeClasses[client->siegeClass].starthealth;
	}
	else if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
	{//only start with 100 health in Duel
		if ( g_gametype.integer == GT_POWERDUEL && client->sess.duelTeam == DUELTEAM_LONE )
		{
			if ( g_duel_fraglimit.integer )
			{
				
				ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] =
					POWER_DUEL_START_HEALTH - ((POWER_DUEL_START_HEALTH - POWER_DUEL_END_HEALTH) * (float)client->sess.wins / (float)g_duel_fraglimit.integer);
			}
			else
			{
				ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] = 150;
			}
		}
		else
		{
			ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] = 100;
		}
	}
	else if (client->ps.stats[STAT_MAX_HEALTH] <= 100)
	{
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] * 1.25;
	}
	else if (client->ps.stats[STAT_MAX_HEALTH] < 125)
	{
		ent->health = client->ps.stats[STAT_HEALTH] = 125;
	}
	else
	{
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH];
	}

	// Start with a small amount of armor as well.
	if (g_gametype.integer == GT_SIEGE &&
		client->siegeClass != -1 /*&&
		bgSiegeClasses[client->siegeClass].startarmor*/)
	{ //class specifies a start armor amount, so use it
		client->ps.stats[STAT_ARMOR] = bgSiegeClasses[client->siegeClass].startarmor;
	}
	else if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
	{//no armor in duel
		client->ps.stats[STAT_ARMOR] = 0;
	}
	else
	{
		client->ps.stats[STAT_ARMOR] = client->ps.stats[STAT_MAX_HEALTH] * 0.25;
	}

	G_SetOrigin( ent, spawn_origin );
	VectorCopy( spawn_origin, client->ps.origin );

	// the respawned flag will be cleared after the attack and jump keys come up
	client->ps.pm_flags |= PMF_RESPAWNED;

	trap_GetUsercmd( client - level.clients, &ent->client->pers.cmd );
	SetClientViewAngle( ent, spawn_angles );

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {

	} else {
		G_KillBox( ent );
		trap_LinkEntity (ent);

		// force the base weapon up
		if (client->ps.weapon <= WP_NONE)
		{
			client->ps.weapon = WP_BRYAR_PISTOL;
		}

		client->ps.torsoTimer = client->ps.legsTimer = 0;

		if (client->ps.weapon == WP_SABER)
		{
			G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_STAND1TO2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS, 0);
		}
		else
		{
			G_SetAnim(ent, NULL, SETANIM_TORSO, TORSO_RAISEWEAP1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS, 0);
			client->ps.legsAnim = WeaponReadyAnim[client->ps.weapon];
		}
		client->ps.weaponstate = WEAPON_RAISING;
		client->ps.weaponTime = client->ps.torsoTimer;
	}

	// don't allow full run speed for a bit
	client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	client->ps.pm_time = 100;

	client->respawnTime = level.time;
	client->latched_buttons = 0;

	if ( level.intermissiontime ) {
		MoveClientToIntermission( ent );
	} else {
		// fire the targets of the spawn point
		G_UseTargets( spawnPoint, ent );

		// select the highest weapon number available, after any
		// spawn given items have fired
	}

	//set teams for NPCs to recognize
	if (g_gametype.integer == GT_SIEGE)
	{ //Imperial (team1) team is allied with "enemy" NPCs in this mode
		if (client->sess.sessionTeam == SIEGETEAM_TEAM1)
		{
			client->playerTeam = ent->s.teamowner = NPCTEAM_ENEMY;
			client->enemyTeam = NPCTEAM_PLAYER;
		}
		else
		{
			client->playerTeam = ent->s.teamowner = NPCTEAM_PLAYER;
			client->enemyTeam = NPCTEAM_ENEMY;
		}
	}
	else
	{
		client->playerTeam = ent->s.teamowner = NPCTEAM_PLAYER;
		client->enemyTeam = NPCTEAM_ENEMY;
	}

	//Disabled. At least for now. Not sure if I'll want to do it or not eventually.

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	client->ps.commandTime = level.time - 100;
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink( ent-g_entities, NULL );

	// positively link the client, even if the command times are weird
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );
		VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
		trap_LinkEntity( ent );
	}

	if (g_spawnInvulnerability.integer)
	{
		ent->client->ps.eFlags |= EF_INVULNERABLE;
		ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
	}

	// run the presend to set anything else, follow spectators wait
	// until all clients have been reconnected after map_restart
	if ( ent->client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		ClientEndFrame( ent );
	}

	// clear entity state values
	BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );

	//ghost bug fix
	ent->s.number = index;

	//rww - make sure client has a valid icarus instance
	trap_ICARUS_FreeEnt( ent );
	trap_ICARUS_InitEnt( ent );

	if (!Q_stricmp(mapname.string, "siege_narshaddaa") && client->sess.sessionTeam == SIEGETEAM_TEAM2 && !iLikeToMineSpam.integer)
	{
		level.antiSpawnSpamTime = level.time + 6000; //disable spamming d spawn at nar 3rd obj for 6 seconds after spawn
	}
}

static qboolean DisconnectedPlayerMatches(genericNode_t *node, void *userData) {
	disconnectedPlayerData_t *existing = (disconnectedPlayerData_t *)node;
	disconnectedPlayerData_t *find = (disconnectedPlayerData_t *)userData;

	if (!existing || !find)
		return qfalse;

	if (existing->ip == find->ip)
		return qtrue;

	if (!Q_stricmp(existing->cuidHash, find->cuidHash))
		return qtrue;

	return qfalse;
}

extern gentity_t *EWeb_Create(gentity_t *spawner);

qboolean RestoreDisconnectedPlayerData(gentity_t *ent) {
	if (!ent || !ent->client)
		return qfalse;

	if (!ent->inuse || ent->client->pers.connected != CON_CONNECTED || (ent->r.svFlags & SVF_BOT) || ent->client->sess.clientType != CLIENT_TYPE_NORMAL)
		return qfalse;
	
	disconnectedPlayerData_t findMe = { 0 };
	findMe.ip = ent->client->sess.ip;
	if (ent->client->sess.cuidHash[0])
		memcpy(findMe.cuidHash, ent->client->sess.cuidHash, sizeof(findMe.cuidHash));
	disconnectedPlayerData_t *data = ListFind(&level.disconnectedPlayerList, DisconnectedPlayerMatches, &findMe, NULL);

	if (!data)
		return qfalse;

	PrintIngame(-1, "Restoring %s^7's pre-disconnect state.\n", ent->client->pers.netname);
	ent->client->sess.canJoin = qtrue;
	SetTeam(ent, data->team == TEAM_RED ? "r" : "b", qtrue);

	int commandTime = ent->client->ps.commandTime;
	int pm_time = ent->client->ps.pm_time;

	const int clientNum = ent - g_entities;
	memcpy(&ent->client->ps, &data->ps, sizeof(playerState_t));
	ent->client->ps.clientNum = clientNum;

	ent->client->ps.commandTime = ent->playerState->commandTime = commandTime;
	ent->client->ps.pm_time = ent->playerState->pm_time = pm_time;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = data->health;
	ent->client->ps.stats[STAT_ARMOR] = data->armor;

	Q_strncpyz(ent->client->sess.siegeClass, data->siegeClassStr, sizeof(ent->client->sess.siegeClass));
	Q_strncpyz(ent->client->sess.spawnedSiegeClass, data->spawnedSiegeClass, sizeof(ent->client->sess.spawnedSiegeClass));
	Q_strncpyz(ent->client->sess.spawnedSiegeModel, data->spawnedSiegeModel, sizeof(ent->client->sess.spawnedSiegeModel));
	ent->client->sess.spawnedSiegeMaxHealth = data->spawnedSiegeMaxHealth;

	ent->client->siegeClass = data->siegeClass;
	ent->client->isHacking = data->isHacking;
	memcpy(ent->client->hackingAngles, data->hackingAngles, sizeof(ent->client->hackingAngles));
	ent->client->ewebHealth = data->ewebHealth;
	ent->client->ewebIndex = data->ewebIndex;
	ent->client->ewebTime = data->ewebTime;
	ent->client->tempSpectate = data->tempSpectate;
	ent->client->saberIgniteTime = data->saberIgniteTime;
	ent->client->saberUnigniteTime = data->saberUnigniteTime;
	ent->client->saberBonusTime = data->saberBonusTime;
	ent->client->pushOffWallTime = data->pushOffWallTime;
	ent->client->usingEmplaced = data->usingEmplaced;
	ent->client->forcingEmplacedNoAttack = data->forcingEmplacedNoAttack;
	memcpy(ent->client->saberThrowDamageTime, data->saberThrowDamageTime, sizeof(ent->client->saberThrowDamageTime));
	ent->client->homingLockTime = data->homingLockTime;
	ent->client->homingLockTarget = data->homingLockTarget;
	ent->client->fakeSpec = data->fakeSpec;
	ent->client->fakeSpecClient = data->fakeSpecClient;
	memcpy(ent->client->lastAiredOtherClientTime, data->lastAiredOtherClientTime, sizeof(ent->client->lastAiredOtherClientTime));
	memcpy(ent->client->lastAiredOtherClientMeansOfDeath, data->lastAiredOtherClientMeansOfDeath, sizeof(ent->client->lastAiredOtherClientMeansOfDeath));
	ent->client->isMedHealed = data->isMedHealed;
	ent->client->isMedHealingSomeone = data->isMedHealingSomeone;
	ent->client->isMedSupplied = data->isMedSupplied;
	ent->client->isMedSupplyingSomeone = data->isMedSupplyingSomeone;
	ent->client->jetPackTime = data->jetPackTime;
	ent->client->jetPackOn = data->jetPackOn;
	ent->client->jetPackToggleTime = data->jetPackToggleTime;
	ent->client->jetPackDebRecharge = data->jetPackDebRecharge;
	ent->client->jetPackDebReduce = data->jetPackDebReduce;
	ent->client->cloakToggleTime = data->cloakToggleTime;
	ent->client->cloakDebRecharge = data->cloakDebRecharge;
	ent->client->cloakDebReduce = data->cloakDebReduce;
	ent->client->airOutTime = data->airOutTime;
	ent->client->invulnerableTimer = data->invulnerableTimer;
	ent->client->saberKnockedTime = data->saberKnockedTime;
	ent->client->triesToSelfkillDuringPause = data->triesToSelfkillDuringPause;
	ent->client->triesToChangeClassDuringPause = data->triesToChangeClassDuringPause;
	Q_strncpyz(ent->client->triesToChangeClassDuringPauseToThisClassName, data->triesToChangeClassDuringPauseToThisClassName, sizeof(ent->client->triesToChangeClassDuringPauseToThisClassName));
	ent->client->triesToChangeClassDuringPauseToThisClass = data->triesToChangeClassDuringPauseToThisClass;
	
	BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, qfalse);

	// eweb fuckery
	if (ent->client->ewebIndex) {
		gentity_t *eweb = EWeb_Create(ent);
		if (eweb) {
			ent->client->ewebIndex = eweb->s.number;
			ent->client->ps.emplacedIndex = eweb->s.number;
		}
		else {
			ent->client->ewebIndex = 0;
			ent->client->ps.emplacedIndex = 0;
		}
	}

	// vehicle fuckery
	if (ent->client->ps.m_iVehicleNum && ent->client->ps.m_iVehicleNum != ENTITYNUM_NONE) {
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];
		if (veh->inuse && veh->m_pVehicle) {
			const int oldBoarding = veh->m_pVehicle->m_iBoarding;
			ent->client->ps.m_iVehicleNum = 0;
			veh->m_pVehicle->m_iBoarding = 0;
			veh->m_pVehicle->m_pVehicleInfo->Board(veh->m_pVehicle, (bgEntity_t *)ent);
			veh->m_pVehicle->m_iBoarding = oldBoarding;
			ent->client->ps.m_iVehicleNum = veh - g_entities; // sanity check, probably not necessary
		}
		else { // some how the vehicle is gone? take away our vehicle
			ent->client->ps.m_iVehicleNum = 0;
		}
	}

	// siege item fuckery
	if (ent->client->holdingObjectiveItem >= MAX_CLIENTS && ent->client->holdingObjectiveItem < ENTITYNUM_WORLD) {
		gentity_t *siegeItem = &g_entities[ent->client->holdingObjectiveItem];
		if (siegeItem && siegeItem->inuse && siegeItem->touch) { // touch the item we dropped
			siegeItem->genericValue2 = 1;
			siegeItem->genericValue8 = ent->s.number;
			siegeItem->genericValue9 = 0;

			// see if we need to kill a "pick up this item" siege help message
			qboolean needUpdate = qfalse;
			iterator_t iter = { 0 };
			ListIterate(&level.siegeHelpList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				siegeHelp_t *help = IteratorNext(&iter);
				if (help->ended)
					continue;
				if (help->started && help->item[0] && (!Q_stricmp(siegeItem->targetname, help->item) || !Q_stricmp(siegeItem->goaltarget, help->item))) {
					help->forceHideItem = qtrue;
					needUpdate = qtrue;
				}
			}
			if (needUpdate)
				level.siegeHelpMessageTime = -SIEGE_HELP_INTERVAL;
			UpdateNewmodSiegeItems();
			siegeItem->genericValue5 = 1;
			if (siegeItem->specialIconTreatment)
				siegeItem->s.eFlags &= ~EF_RADAROBJECT;
			siegeItem->s.time2 = 0xFFFFFFFF;
		}
		else { // somehow the item is gone? take away our item i guess
			ent->client->holdingObjectiveItem = 0;
		}
	}

	ListRemove(&level.disconnectedPlayerList, data);

	return qtrue;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.

This should NOT be called directly by any game logic,
call trap_DropClient(), which will call this and do
server system housekeeping.
============
*/
void ClientDisconnect( int clientNum ) {
	gentity_t	*ent;
	gentity_t	*tent;
	int			i;

	// cleanup if we are kicking a bot that
	// hasn't spawned yet
	G_RemoveQueuedBotBegin( clientNum );

	ent = g_entities + clientNum;
	if ( !ent->client ) {
		return;
	}

	memset(&level.siegeTopTimes[clientNum], 0, sizeof(level.siegeTopTimes[0]));

	// auto-pause if someone disconnects during a live pug
	if (g_autoPauseDisconnect.integer && g_gametype.integer == GT_SIEGE && (ent->client->sess.siegeDesiredTeam == TEAM_RED || ent->client->sess.siegeDesiredTeam == TEAM_BLUE) && level.isLivePug == ISLIVEPUG_YES &&
		(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) && level.siegeRoundStartTime && level.time - level.siegeRoundStartTime >= LIVEPUG_AUTOPAUSE_TIME) {

		// only reset the pause time back up to maximum if we're not already paused
		if (level.pause.state != PAUSE_PAUSED)
			level.pause.time = level.time + 300000;

		// always update the reason string
		Q_strncpyz(level.pause.reason, va("%s^7 disconnected\n", ent->client->pers.netname), sizeof(level.pause.reason));

		level.pause.state = PAUSE_PAUSED;
		DoPauseStartChecks();

		if (g_autoPauseDisconnect.integer != 1 && ent->health > 0 && !ent->client->ps.fallingToDeath) {
			disconnectedPlayerData_t findMe = { 0 };
			findMe.ip = ent->client->sess.ip;
			if (ent->client->sess.cuidHash[0])
				memcpy(findMe.cuidHash, ent->client->sess.cuidHash, sizeof(findMe.cuidHash));
			disconnectedPlayerData_t *data = ListFind(&level.disconnectedPlayerList, DisconnectedPlayerMatches, &findMe, NULL);
			if (!data)
				data = ListAdd(&level.disconnectedPlayerList, sizeof(disconnectedPlayerData_t));
			data->ip = ent->client->sess.ip;
			if (ent->client->sess.cuidHash[0])
				memcpy(data->cuidHash, ent->client->sess.cuidHash, sizeof(data->cuidHash));

			data->team = ent->client->sess.siegeDesiredTeam;
			data->health = ent->health;
			data->armor = ent->client->ps.stats[STAT_ARMOR];

			Q_strncpyz(data->siegeClassStr, ent->client->sess.siegeClass, sizeof(data->siegeClassStr));
			Q_strncpyz(data->spawnedSiegeClass, ent->client->sess.spawnedSiegeClass, sizeof(data->spawnedSiegeClass));
			Q_strncpyz(data->spawnedSiegeModel, ent->client->sess.spawnedSiegeModel, sizeof(data->spawnedSiegeModel));
			data->spawnedSiegeMaxHealth = ent->client->sess.spawnedSiegeMaxHealth;

			data->siegeClass = ent->client->siegeClass;
			memcpy(&data->ps, &ent->client->ps, sizeof(playerState_t));
			data->isHacking = ent->client->isHacking;
			memcpy(data->hackingAngles, ent->client->hackingAngles, sizeof(data->hackingAngles));
			data->ewebHealth = ent->client->ewebHealth;
			data->ewebIndex = ent->client->ewebIndex;
			data->ewebTime = ent->client->ewebTime;
			data->tempSpectate = ent->client->tempSpectate;
			data->saberIgniteTime = ent->client->saberIgniteTime;
			data->saberUnigniteTime = ent->client->saberUnigniteTime;
			data->saberBonusTime = ent->client->saberBonusTime;
			data->pushOffWallTime = ent->client->pushOffWallTime;
			data->usingEmplaced = ent->client->usingEmplaced;
			data->forcingEmplacedNoAttack = ent->client->forcingEmplacedNoAttack;
			memcpy(data->saberThrowDamageTime, ent->client->saberThrowDamageTime, sizeof(data->saberThrowDamageTime));
			data->homingLockTime = ent->client->homingLockTime;
			data->homingLockTarget = ent->client->homingLockTarget;
			data->fakeSpec = ent->client->fakeSpec;
			data->fakeSpecClient = ent->client->fakeSpecClient;
			memcpy(data->lastAiredOtherClientTime, ent->client->lastAiredOtherClientTime, sizeof(data->lastAiredOtherClientTime));
			memcpy(data->lastAiredOtherClientMeansOfDeath, ent->client->lastAiredOtherClientMeansOfDeath, sizeof(data->lastAiredOtherClientMeansOfDeath));
			data->isMedHealed = ent->client->isMedHealed;
			data->isMedHealingSomeone = ent->client->isMedHealingSomeone;
			data->isMedSupplied = ent->client->isMedSupplied;
			data->isMedSupplyingSomeone = ent->client->isMedSupplyingSomeone;
			data->jetPackTime = ent->client->jetPackTime;
			data->jetPackOn = ent->client->jetPackOn;
			data->jetPackToggleTime = ent->client->jetPackToggleTime;
			data->jetPackDebRecharge = ent->client->jetPackDebRecharge;
			data->jetPackDebReduce = ent->client->jetPackDebReduce;
			data->cloakToggleTime = ent->client->cloakToggleTime;
			data->cloakDebRecharge = ent->client->cloakDebRecharge;
			data->cloakDebReduce = ent->client->cloakDebReduce;
			data->airOutTime = ent->client->airOutTime;
			data->invulnerableTimer = ent->client->invulnerableTimer;
			data->saberKnockedTime = ent->client->saberKnockedTime;
			data->triesToSelfkillDuringPause = ent->client->triesToSelfkillDuringPause;
			data->triesToChangeClassDuringPause = ent->client->triesToChangeClassDuringPause;
			Q_strncpyz(data->triesToChangeClassDuringPauseToThisClassName, ent->client->triesToChangeClassDuringPauseToThisClassName, sizeof(data->triesToChangeClassDuringPauseToThisClassName));
			data->triesToChangeClassDuringPauseToThisClass = ent->client->triesToChangeClassDuringPauseToThisClass;
		}
	}

	if (clientNum < MAX_CLIENTS) {
		ent->client->sess.siegeFollowing.wasFollowing = qfalse;
	}

    G_DBLogNickname( ent->client->sess.ip, ent->client->pers.netname, getGlobalTime() - ent->client->sess.nameChangeTime, ent->client->sess.auth == AUTHENTICATED ? ent->client->sess.cuidHash : "");

    ent->client->sess.nameChangeTime = getGlobalTime();

    //G_LogDbLogSessionEnd( ent->client->sess.sessionId );

	// client disconnected, update auto dl if he was the last one
	if (connectingClients > 0) {
		--connectingClients;
		trap_Cvar_Set("cl_allowDownload", connectingClients ? "1" : "0");
	}

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		if (ent->client->ps.fd.forcePowersActive & (1 << i))
		{
			WP_ForcePowerStop(ent, i);
		}
		i++;
	}

	i = TRACK_CHANNEL_1;

	while (i < NUM_TRACK_CHANNELS)
	{
		if (ent->client->ps.fd.killSoundEntIndex[i-50] && ent->client->ps.fd.killSoundEntIndex[i-50] < MAX_GENTITIES && ent->client->ps.fd.killSoundEntIndex[i-50] > 0)
		{
			G_MuteSound(ent->client->ps.fd.killSoundEntIndex[i-50], CHAN_VOICE);
		}
		i++;
	}
	i = 0;

	if (ent->client->ps.m_iVehicleNum)
	{ //tell it I'm getting off
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{ 
			veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
		}
	}

	// stop any following clients
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].sess.sessionTeam == TEAM_SPECTATOR
			&& level.clients[i].sess.spectatorState == SPECTATOR_FOLLOW
			&& level.clients[i].sess.spectatorClient == clientNum ) {
			StopFollowing( &g_entities[i] );
		}
	}

	// send effect if they were completely connected
	if ( ent->client->pers.connected == CON_CONNECTED && ent->client->sess.sessionTeam != TEAM_SPECTATOR && level.pause.state == PAUSE_NONE) {
		tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = ent->s.clientNum;

		// They don't get to take powerups with them!
		// Especially important for stuff like CTF flags
		TossClientItems( ent );
	}

	G_LogPrintf( "ClientDisconnect: %i (%s) from %s\n", clientNum, ent->client->pers.netname, ent->client->sess.ipString);

	// if we are playing in tourney mode, give a win to the other player and clear his frags for this round
	if ( (g_gametype.integer == GT_DUEL )
		&& !level.intermissiontime
		&& !level.warmupTime ) {
		if ( level.sortedClients[1] == clientNum ) {
			level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] = 0;
			level.clients[ level.sortedClients[0] ].sess.wins++;
			ClientUserinfoChanged( level.sortedClients[0], "6");
		}
		else if ( level.sortedClients[0] == clientNum ) {
			level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] = 0;
			level.clients[ level.sortedClients[1] ].sess.wins++;
			ClientUserinfoChanged( level.sortedClients[1], "7");
		}
	}

	if (ent->ghoul2 && trap_G2_HaveWeGhoul2Models(ent->ghoul2))
	{
		trap_G2API_CleanGhoul2Models(&ent->ghoul2);
	}
	i = 0;
	while (i < MAX_SABERS)
	{
		if (ent->client->weaponGhoul2[i] && trap_G2_HaveWeGhoul2Models(ent->client->weaponGhoul2[i]))
		{
			trap_G2API_CleanGhoul2Models(&ent->client->weaponGhoul2[i]);
		}
		i++;
	}

	trap_UnlinkEntity (ent);
	ent->s.modelindex = 0;
	ent->inuse = qfalse;
	ent->classname = "disconnected";
	ent->client->pers.connected = CON_DISCONNECTED;
	ent->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
	ent->client->sess.sessionTeam = TEAM_FREE;
	ent->client->sess.skillBoost = 0;
	ent->client->sess.senseBoost = 0;
	memset(&ent->client->sess.spawnedSiegeClass, 0, sizeof(ent->client->sess.spawnedSiegeClass));
	memset(&ent->client->sess.spawnedSiegeModel, 0, sizeof(ent->client->sess.spawnedSiegeModel));
	ent->client->sess.spawnedSiegeMaxHealth = 0;
	ent->client->sess.whitelistStatus = WHITELIST_UNKNOWN;
	ent->r.contents = 0;
	level.clientUniqueIds[clientNum] = 0;

	trap_SetConfigstring( CS_PLAYERS + clientNum, "");

	CalculateRanks();

	if ( ent->r.svFlags & SVF_BOT ) {
		BotAIShutdownClient( clientNum, qfalse );
	} 
	// // accounts system
	//else {
	//	int freeSlot;
	//	//send heartbeat soon
	//	nextHeartBeatTime = level.time;// + 2000; //+1000
	//	//G_DBLog("D3: nextHeartBeatTime: %i\n",nextHeartBeatTime);
	//	isNextHeartBeatForced = qtrue;

	//	//we should add this client to authorized for shord
	//	//period of time, so reconnecting is easy
	//	freeSlot = getFreeSlotIndex();
	//	strncpy(authorized[freeSlot].username,ent->client->sess.username,MAX_USERNAME_SIZE);
	//	authorized[freeSlot].password[0] = '\0';
	//	authorized[freeSlot].status = STATUS_AUTHORIZED;
	//	authorized[freeSlot].expire = level.time + 2*1000; //2 seconds should be enough
	//	//authorized[freeSlot].isRecconect = qtrue;
	//	authorized[freeSlot].reconnectIp = ent->client->sess.ip;
	//	
	//}

	G_ClearClientLog(clientNum);

}   


