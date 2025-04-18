// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"



/*
===============================================================================

PUSHMOVE

===============================================================================
*/

void MatchTeam(gentity_t *teamLeader, int moverState, int time);

typedef struct {
	gentity_t	*ent;
	vec3_t	origin;
	vec3_t	angles;
	float	deltayaw;
} pushed_t;
pushed_t	pushed[MAX_GENTITIES], *pushed_p;

#define MOVER_START_ON		1
#define MOVER_FORCE_ACTIVATE	2
#define MOVER_CRUSHER		4
#define MOVER_TOGGLE		8
#define MOVER_LOCKED		16
#define MOVER_GOODIE		32
#define MOVER_PLAYER_USE	64
#define MOVER_INACTIVE		128

int	BMS_START = 0;
int	BMS_MID = 1;
int	BMS_END = 2;

/*
-------------------------
G_PlayDoorLoopSound
-------------------------
*/

void G_PlayDoorLoopSound(gentity_t *ent)
{
	if (!ent->soundSet || !ent->soundSet[0])
	{
		return;
	}

	ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);
	ent->s.loopIsSoundset = qtrue;
	ent->s.loopSound = BMS_MID;
}

/*
-------------------------
G_PlayDoorSound
-------------------------
*/

void G_PlayDoorSound(gentity_t *ent, int type)
{
	vmCvar_t	mapname;
	trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);

	if (!Q_stricmpn(ent->team, "bunkerfront", 11) && !Q_stricmpn(mapname.string, "mp/siege_hoth", 13) && g_fixHothDoorSounds.integer)
	{
		ent->s.soundSetIndex = G_SoundSetIndex("impdoor1");
		G_AddEvent(ent, EV_PLAYDOORSOUND, type);
		return;
	}

	if (!ent->soundSet || !ent->soundSet[0])
	{
		return;
	}

	ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);

	G_AddEvent(ent, EV_PLAYDOORSOUND, type);
}

/*
============
G_TestEntityPosition

============
*/
gentity_t	*G_TestEntityPosition(gentity_t *ent) {
	trace_t	tr;
	int		mask;

	if (ent->clipmask) {
		mask = ent->clipmask;
	}
	else {
		mask = MASK_SOLID;
	}
	if (ent->client) {
		vec3_t vMax;
		VectorCopy(ent->r.maxs, vMax);
		if (vMax[2] < 1)
		{
			vMax[2] = 1;
		}
		trap_Trace(&tr, ent->client->ps.origin, ent->r.mins, vMax, ent->client->ps.origin, ent->s.number, mask);
	}
	else {
		trap_Trace(&tr, ent->s.pos.trBase, ent->r.mins, ent->r.maxs, ent->s.pos.trBase, ent->s.number, mask);
	}

	if (tr.startsolid)
		return &g_entities[tr.entityNum];

	return NULL;
}

/*
================
G_CreateRotationMatrix
================
*/
void G_CreateRotationMatrix(vec3_t angles, vec3_t matrix[3]) {
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/*
================
G_TransposeMatrix
================
*/
void G_TransposeMatrix(vec3_t matrix[3], vec3_t transpose[3]) {
	int i, j;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			transpose[i][j] = matrix[j][i];
		}
	}
}

/*
================
G_RotatePoint
================
*/
void G_RotatePoint(vec3_t point, vec3_t matrix[3]) {
	vec3_t tvec;

	VectorCopy(point, tvec);
	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
}

/*
==================
G_TryPushingEntity

Returns qfalse if the move is blocked
==================
*/
qboolean	G_TryPushingEntity(gentity_t *check, gentity_t *pusher, vec3_t move, vec3_t amove, gentity_t **blocker) {
	vec3_t		matrix[3], transpose[3];
	vec3_t		org, org2, move2;
	gentity_t	*block, *confirmedBlocker = NULL;
	*blocker = NULL;

	// fix issue with horizontal "doors" being way too easily greenable with a detpack placed on top
	qboolean isCargoTrollDetpack = qfalse;
	if (level.siegeMap == SIEGEMAP_CARGO && check && !Q_stricmp(check->classname, "detpack") &&
		check->r.currentOrigin[0] >= 7034 && check->r.currentOrigin[0] <= 7322 &&
		check->r.currentOrigin[1] >= -456 && check->r.currentOrigin[1] <= -110) {
		isCargoTrollDetpack = qtrue;
	}

	//This was only serverside not to mention it was never set.
	if (pusher->s.apos.trType != TR_STATIONARY//rotating
		&& (pusher->spawnflags & 16) //IMPACT
		&& Q_stricmp("func_rotating", pusher->classname) == 0)
	{//just blow the fuck out of them
		G_Damage(check, pusher, pusher, NULL, NULL, pusher->damage, DAMAGE_NO_KNOCKBACK, MOD_CRUSH);
		return qtrue;
	}

	// save off the old position
	if (pushed_p > &pushed[MAX_GENTITIES]) {
		G_Error("pushed_p > &pushed[MAX_GENTITIES]");
	}
	pushed_p->ent = check;
	VectorCopy(check->s.pos.trBase, pushed_p->origin);
	VectorCopy(check->s.apos.trBase, pushed_p->angles);
	if (check->client) {
		pushed_p->deltayaw = check->client->ps.delta_angles[YAW];
		VectorCopy(check->client->ps.origin, pushed_p->origin);
	}
	pushed_p++;

	if (isCargoTrollDetpack && pusher->moverState == MOVER_2TO1) {
		check->s.groundEntityNum = ENTITYNUM_WORLD; // don't push the detpack back while it closes; consider it stuck to the floor from now on
	}
	else {
		// try moving the contacted entity 
		// figure movement due to the pusher's amove
		G_CreateRotationMatrix(amove, transpose);
		G_TransposeMatrix(transpose, matrix);
		if (check->client) {
			VectorSubtract(check->client->ps.origin, pusher->r.currentOrigin, org);
		}
		else {
			VectorSubtract(check->s.pos.trBase, pusher->r.currentOrigin, org);
		}
		VectorCopy(org, org2);
		G_RotatePoint(org2, matrix);
		VectorSubtract(org2, org, move2);
		// add movement
		VectorAdd(check->s.pos.trBase, move, check->s.pos.trBase);
		VectorAdd(check->s.pos.trBase, move2, check->s.pos.trBase);
		if (check->client) {
			VectorAdd(check->client->ps.origin, move, check->client->ps.origin);
			VectorAdd(check->client->ps.origin, move2, check->client->ps.origin);
			// make sure the client's view rotates when on a rotating mover
			check->client->ps.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
		}

		// may have pushed them off an edge
		if (check->s.groundEntityNum != pusher->s.number) {
			check->s.groundEntityNum = ENTITYNUM_NONE;
		}

		block = G_TestEntityPosition(check);
		if (block) {
			confirmedBlocker = block;
		}
		else {
			// pushed ok
			if (check->client) {
				VectorCopy(check->client->ps.origin, check->r.currentOrigin);
			}
			else {
				VectorCopy(check->s.pos.trBase, check->r.currentOrigin);
			}
			trap_LinkEntity(check);
			return qtrue;
		}
	}

	if (isCargoTrollDetpack) {
		return qtrue; // push the detpack
	}
	else {
		if (check->takedamage && !check->client && check->s.weapon && check->r.ownerNum < MAX_CLIENTS &&
			check->health < 500)
		{
			if (check->health > 0)
			{
				G_Damage(check, pusher, pusher, vec3_origin, check->r.currentOrigin, 999, 0, MOD_UNKNOWN);
			}
			return qfalse;
		}
	}

	// if it is ok to leave in the old position, do it
	// this is only relevent for riding entities, not pushed
	// Sliding trapdoors can cause this.
	VectorCopy((pushed_p - 1)->origin, check->s.pos.trBase);
	if (check->client) {
		VectorCopy((pushed_p - 1)->origin, check->client->ps.origin);
	}
	VectorCopy((pushed_p - 1)->angles, check->s.apos.trBase);
	block = G_TestEntityPosition(check);
	if (!block) {
		check->s.groundEntityNum = -1;
		pushed_p--;
		return qtrue;
	}

	// blocked
	if (confirmedBlocker)
		*blocker = confirmedBlocker;
	return qfalse;
}


void G_ExplodeMissile(gentity_t *ent);

/*
============
G_MoverPush

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
If qfalse is returned, *obstacle will be the blocking entity
============
*/
extern void NPC_RemoveBody(gentity_t *self);

qboolean G_MoverPush(gentity_t *pusher, vec3_t move, vec3_t amove, gentity_t **obstacle, gentity_t **blockedBy) {
	int			i, e;
	gentity_t	*check, *blocker = NULL;
	vec3_t		mins, maxs;
	pushed_t	*p;
	int			entityList[MAX_GENTITIES];
	int			listedEntities;
	vec3_t		totalMins, totalMaxs;

	*obstacle = NULL;
	*blockedBy = NULL;

	// mins/maxs are the bounds at the destination
	// totalMins / totalMaxs are the bounds for the entire move
	if (pusher->r.currentAngles[0] || pusher->r.currentAngles[1] || pusher->r.currentAngles[2]
		|| amove[0] || amove[1] || amove[2]) {
		float		radius;

		radius = RadiusFromBounds(pusher->r.mins, pusher->r.maxs);
		for (i = 0; i < 3; i++) {
			mins[i] = pusher->r.currentOrigin[i] + move[i] - radius;
			maxs[i] = pusher->r.currentOrigin[i] + move[i] + radius;
			totalMins[i] = mins[i] - move[i];
			totalMaxs[i] = maxs[i] - move[i];
		}
	}
	else {
		for (i = 0; i<3; i++) {
			mins[i] = pusher->r.absmin[i] + move[i];
			maxs[i] = pusher->r.absmax[i] + move[i];
		}

		VectorCopy(pusher->r.absmin, totalMins);
		VectorCopy(pusher->r.absmax, totalMaxs);
		for (i = 0; i<3; i++) {
			if (move[i] > 0) {
				totalMaxs[i] += move[i];
			}
			else {
				totalMins[i] += move[i];
			}
		}
	}

	// unlink the pusher so we don't get it in the entityList
	trap_UnlinkEntity(pusher);

	listedEntities = trap_EntitiesInBox(totalMins, totalMaxs, entityList, MAX_GENTITIES);

	// move the pusher to it's final position
	VectorAdd(pusher->r.currentOrigin, move, pusher->r.currentOrigin);
	VectorAdd(pusher->r.currentAngles, amove, pusher->r.currentAngles);
	trap_LinkEntity(pusher);

	// see if any solid entities are inside the final position
	for (e = 0; e < listedEntities; e++) {
		check = &g_entities[entityList[e]];

		// only push items and players
		if ( /*check->s.eType != ET_ITEM &&*/ check->s.eType != ET_PLAYER && check->s.eType != ET_NPC && !check->physicsObject) {
			continue;
		}

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->s.groundEntityNum != pusher->s.number) {
			// see if the ent needs to be tested
			if (check->r.absmin[0] >= maxs[0]
				|| check->r.absmin[1] >= maxs[1]
				|| check->r.absmin[2] >= maxs[2]
				|| check->r.absmax[0] <= mins[0]
				|| check->r.absmax[1] <= mins[1]
				|| check->r.absmax[2] <= mins[2]) {
				continue;
			}
			// see if the ent's bbox is inside the pusher's final position
			// this does allow a fast moving object to pass through a thin entity...
			gentity_t *touchingEnt = G_TestEntityPosition(check);
			if (!touchingEnt || touchingEnt != pusher) {
				continue;
			}
		}

		// the entity needs to be pushed
		if (G_TryPushingEntity(check, pusher, move, amove, &blocker)) {
			continue;
		}

		if (pusher->damage && check->client && (pusher->spawnflags & 32))
		{
			G_Damage(check, pusher, pusher, NULL, NULL, pusher->damage, 0, MOD_CRUSH);
			continue;
		}

		if (check->s.eType == ET_PLAYER && check->health < 1)
		{ //whatever, just crush it
			G_Damage(check, pusher, pusher, NULL, NULL, 999, 0, MOD_CRUSH);
			continue;
		}

		if (check->s.eType == ET_BODY)
		{ //remove dead bodies
			G_FreeEntity(check);
			continue;
		}

		if ((check->r.contents & CONTENTS_TRIGGER) && check->s.weapon == G2_MODEL_PART)
		{//keep limbs from blocking elevators.  Kill the limb and keep moving.
			G_FreeEntity(check);
			continue;
		}

		if (check->s.eFlags & EF_DROPPEDWEAPON)
		{//keep dropped weapons from blocking elevators.  Kill the weapon and keep moving.
			G_FreeEntity(check);
			continue;
		}

		if (check->s.eType == ET_NPC //an NPC
			&& check->health <= 0 //NPC is dead
			&& !(check->flags & FL_NOTARGET))  //NPC isn't still spawned or in no target mode.
		{//dead npcs should be removed now!
			NPC_RemoveBody(check);
			continue;
		}

		if (check->methodOfDeath && (check->methodOfDeath == MOD_THERMAL || check->methodOfDeath == MOD_THERMAL_SPLASH))
		{
			continue; //dets can't block lifts
		}

		// the move was blocked an entity

		// bobbing entities are instant-kill and never get blocked
		if (pusher->s.pos.trType == TR_SINE || pusher->s.apos.trType == TR_SINE) {
			G_Damage(check, pusher, pusher, NULL, NULL, 99999, 0, MOD_CRUSH);
			continue;
		}

		// save off the obstacle so we can call the block function (crush, etc)
		*obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p = pushed_p - 1; p >= pushed; p--) {
			VectorCopy(p->origin, p->ent->s.pos.trBase);
			VectorCopy(p->angles, p->ent->s.apos.trBase);
			if (p->ent->client) {
				p->ent->client->ps.delta_angles[YAW] = p->deltayaw;
				VectorCopy(p->origin, p->ent->client->ps.origin);
			}
			trap_LinkEntity(p->ent);
		}

		if (blocker)
			*blockedBy = blocker;

		return qfalse;
	}

	return qtrue;
}


/*
=================
G_MoverTeam
=================
*/
void G_MoverTeam(gentity_t *ent) {
	vec3_t		move, amove;
	gentity_t	*part, *obstacle = NULL, *blockedBy = NULL;
	vec3_t		origin, angles;

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
	pushed_p = pushed;
	for (part = ent; part; part = part->teamchain) {
		// get current position
		BG_EvaluateTrajectory(&part->s.pos, level.time, origin);
		BG_EvaluateTrajectory(&part->s.apos, level.time, angles);
		VectorSubtract(origin, part->r.currentOrigin, move);
		VectorSubtract(angles, part->r.currentAngles, amove);
		if (!VectorCompare(move, vec3_origin)
			|| !VectorCompare(amove, vec3_origin))
		{//actually moved
			if (!G_MoverPush(part, move, amove, &obstacle, &blockedBy)) {
				break;	// move was blocked
			}
		}
	}

	if (part) {
		// go back to the previous position
		for (part = ent; part; part = part->teamchain) {
			part->s.pos.trTime += level.time - level.previousTime;
			part->s.apos.trTime += level.time - level.previousTime;
			BG_EvaluateTrajectory(&part->s.pos, level.time, part->r.currentOrigin);
			BG_EvaluateTrajectory(&part->s.apos, level.time, part->r.currentAngles);
			trap_LinkEntity(part);
		}

		// if the pusher has a "blocked" function, call it
		if (ent->blocked) {
			ent->blocked(ent, obstacle, blockedBy);
		}
		return;
	}

	// the move succeeded
	for (part = ent; part; part = part->teamchain) {
		// call the reached function if time is at or past end point
		if (part->s.pos.trType == TR_LINEAR_STOP ||
			part->s.pos.trType == TR_NONLINEAR_STOP) {
			if (level.time >= part->s.pos.trTime + part->s.pos.trDuration) {
				if (part->reached) {
					part->reached(part);
				}
			}
		}
	}
}

/*
================
G_RunMover

================
*/
void G_RunMover(gentity_t *ent) {
	// if not a team captain, don't do anything, because
	// the captain will handle everything
	if (ent->flags & FL_TEAMSLAVE) {
		return;
	}

	// if stationary at one of the positions, don't move anything
	if (ent->s.pos.trType != TR_STATIONARY || ent->s.apos.trType != TR_STATIONARY) {
		//OSP: pause
		if (level.pause.state == PAUSE_NONE)
			G_MoverTeam(ent);
		else
			ent->s.pos.trTime += level.time - level.previousTime;
	}

	// check think function
	G_RunThink(ent);
}

/*
============================================================================

GENERAL MOVERS

Doors, plats, and buttons are all binary (two position) movers
Pos1 is "at rest", pos2 is "activated"
============================================================================
*/

/*
CalcTeamDoorCenter

Finds all the doors of a team and returns their center position
*/

void CalcTeamDoorCenter(gentity_t *ent, vec3_t center)
{
	vec3_t		slavecenter;
	gentity_t	*slave;

	//Start with our center
	VectorAdd(ent->r.mins, ent->r.maxs, center);
	VectorScale(center, 0.5, center);
	for (slave = ent->teamchain; slave; slave = slave->teamchain)
	{
		//Find slave's center
		VectorAdd(slave->r.mins, slave->r.maxs, slavecenter);
		VectorScale(slavecenter, 0.5, slavecenter);
		//Add that to our own, find middle
		VectorAdd(center, slavecenter, center);
		VectorScale(center, 0.5, center);
	}
}

/*
===============
SetMoverState
===============
*/
void SetMoverState(gentity_t *ent, moverState_t moverState, int time) {
	vec3_t			delta;
	float			f;

	ent->moverState = moverState;

	ent->s.pos.trTime = time;

	if (ent->s.pos.trDuration <= 0)
	{//Don't allow divide by zero!
		ent->s.pos.trDuration = 1;
	}

	switch (moverState) {
	case MOVER_POS1:
		VectorCopy(ent->pos1, ent->s.pos.trBase);
		ent->s.pos.trType = TR_STATIONARY;
		break;
	case MOVER_POS2:
		VectorCopy(ent->pos2, ent->s.pos.trBase);
		ent->s.pos.trType = TR_STATIONARY;
		break;
	case MOVER_1TO2:
		VectorCopy(ent->pos1, ent->s.pos.trBase);
		VectorSubtract(ent->pos2, ent->pos1, delta);
		f = 1000.0 / ent->s.pos.trDuration;
		VectorScale(delta, f, ent->s.pos.trDelta);
		if (ent->alt_fire)
		{
			ent->s.pos.trType = TR_LINEAR_STOP;
		}
		else
		{
			ent->s.pos.trType = TR_NONLINEAR_STOP;
		}
		break;
	case MOVER_2TO1:
		VectorCopy(ent->pos2, ent->s.pos.trBase);
		VectorSubtract(ent->pos1, ent->pos2, delta);
		f = 1000.0 / ent->s.pos.trDuration;
		VectorScale(delta, f, ent->s.pos.trDelta);
		if (ent->alt_fire)
		{
			ent->s.pos.trType = TR_LINEAR_STOP;
		}
		else
		{
			ent->s.pos.trType = TR_NONLINEAR_STOP;
		}
		break;
	}
	BG_EvaluateTrajectory(&ent->s.pos, level.time, ent->r.currentOrigin);
	trap_LinkEntity(ent);
}

/*
================
MatchTeam

All entities in a mover team will move from pos1 to pos2
in the same amount of time
================
*/
void MatchTeam(gentity_t *teamLeader, int moverState, int time) {
	gentity_t		*slave;

	for (slave = teamLeader; slave; slave = slave->teamchain) {
		SetMoverState(slave, (moverState_t)moverState, time);
	}
}



/*
================
ReturnToPos1
================
*/
void ReturnToPos1(gentity_t *ent) {
	ent->think = 0;
	ent->nextthink = 0;
	ent->s.time = level.time;

	MatchTeam(ent, MOVER_2TO1, level.time);

	// starting sound
	G_PlayDoorLoopSound(ent);
	G_PlayDoorSound(ent, BMS_START);	//??
}


/*
================
Reached_BinaryMover
================
*/

void Reached_BinaryMover(gentity_t *ent)
{
	// stop the looping sound
	ent->s.loopSound = 0;
	ent->s.loopIsSoundset = qfalse;

	if (ent->moverState == MOVER_1TO2)
	{//reached open
		vec3_t	doorcenter;

		// reached pos2
		SetMoverState(ent, MOVER_POS2, level.time);

		CalcTeamDoorCenter(ent, doorcenter);

		// play sound
		G_PlayDoorSound(ent, BMS_END);

		if (ent->wait < 0)
		{//Done for good
			ent->think = 0;
			ent->nextthink = 0;
			ent->use = 0;
		}
		else
		{
			// return to pos1 after a delay
			ent->think = ReturnToPos1;
			if (ent->spawnflags & 8)
			{//Toggle, keep think, wait for next use?
				ent->nextthink = -1;
			}
			else
			{
				ent->nextthink = level.time + ent->wait;
			}
		}

		// fire targets
		if (!ent->activator)
		{
			ent->activator = ent;
		}
		G_UseTargets2(ent, ent->activator, ent->opentarget);
	}
	else if (ent->moverState == MOVER_2TO1)
	{//closed
		vec3_t	doorcenter;

		// reached pos1
		SetMoverState(ent, MOVER_POS1, level.time);

		CalcTeamDoorCenter(ent, doorcenter);

		// play sound
		G_PlayDoorSound(ent, BMS_END);

		// close areaportals
		if (ent->teammaster == ent || !ent->teammaster)
		{
			trap_AdjustAreaPortalState(ent, qfalse);
		}
		G_UseTargets2(ent, ent->activator, ent->closetarget);
	}
	else
	{
		G_Error("Reached_BinaryMover: bad moverState");
	}
}


/*
================
Use_BinaryMover_Go
================
*/
void Use_BinaryMover_Go(gentity_t *ent)
{
	int		total;
	int		partial;
	gentity_t	*activator = ent->activator;

	ent->activator = activator;

	if (ent->moverState == MOVER_POS1)
	{
		vec3_t	doorcenter;

		// start moving 50 msec later, becase if this was player
		// triggered, level.time hasn't been advanced yet
		MatchTeam(ent, MOVER_1TO2, level.time + 50);

		CalcTeamDoorCenter(ent, doorcenter);

		// starting sound
		G_PlayDoorLoopSound(ent);
		G_PlayDoorSound(ent, BMS_START);
		ent->s.time = level.time;

		// open areaportal
		if (ent->teammaster == ent || !ent->teammaster) {
			trap_AdjustAreaPortalState(ent, qtrue);
		}
		G_UseTargets(ent, ent->activator);
		return;
	}

	// if all the way up, just delay before coming down
	if (ent->moverState == MOVER_POS2) {
		//have to do this because the delay sets our think to Use_BinaryMover_Go
		ent->think = ReturnToPos1;
		if (ent->spawnflags & 8)
		{//TOGGLE doors don't use wait!
			ent->nextthink = level.time + FRAMETIME;
		}
		else
		{
			ent->nextthink = level.time + ent->wait;
		}
		G_UseTargets2(ent, ent->activator, ent->target2);
		return;
	}

	// only partway down before reversing
	if (ent->moverState == MOVER_2TO1)
	{
		if (ent->s.pos.trType == TR_NONLINEAR_STOP)
		{
			vec3_t curDelta;
			float fPartial;
			total = ent->s.pos.trDuration - 50;
			VectorSubtract(ent->r.currentOrigin, ent->pos1, curDelta);
			fPartial = VectorLength(curDelta) / VectorLength(ent->s.pos.trDelta);
			VectorScale(ent->s.pos.trDelta, fPartial, curDelta);
			fPartial /= ent->s.pos.trDuration;
			fPartial /= 0.001f;
			fPartial = acos(fPartial);
			fPartial = RAD2DEG(fPartial);
			fPartial = (90.0f - fPartial) / 90.0f*ent->s.pos.trDuration;
			partial = total - floor(fPartial);
		}
		else
		{
			total = ent->s.pos.trDuration;
			partial = level.time - ent->s.pos.trTime;
		}

		if (partial > total) {
			partial = total;
		}
		ent->s.pos.trTime = level.time - (total - partial);

		MatchTeam(ent, MOVER_1TO2, ent->s.pos.trTime);

		G_PlayDoorSound(ent, BMS_START);

		return;
	}

	// only partway up before reversing
	if (ent->moverState == MOVER_1TO2)
	{
		if (ent->s.pos.trType == TR_NONLINEAR_STOP)
		{
			vec3_t curDelta;
			float fPartial;
			total = ent->s.pos.trDuration - 50;
			VectorSubtract(ent->r.currentOrigin, ent->pos2, curDelta);
			fPartial = VectorLength(curDelta) / VectorLength(ent->s.pos.trDelta);
			VectorScale(ent->s.pos.trDelta, fPartial, curDelta);
			fPartial /= ent->s.pos.trDuration;
			fPartial /= 0.001f;
			fPartial = acos(fPartial);
			fPartial = RAD2DEG(fPartial);
			fPartial = (90.0f - fPartial) / 90.0f*ent->s.pos.trDuration;
			partial = total - floor(fPartial);
		}
		else
		{
			total = ent->s.pos.trDuration;
			partial = level.time - ent->s.pos.trTime;
		}
		if (partial > total) {
			partial = total;
		}

		ent->s.pos.trTime = level.time - (total - partial);
		MatchTeam(ent, MOVER_2TO1, ent->s.pos.trTime);

		G_PlayDoorSound(ent, BMS_START);

		return;
	}
}

void UnLockDoors(gentity_t *const ent)
{
	//noise?
	//go through and unlock the door and all the slaves
	gentity_t	*slave = ent;
	do
	{	// want to allow locked toggle doors, so keep the targetname
		if (!(slave->spawnflags & MOVER_TOGGLE) && !slave->remainUsableIfUnlocked)
		{
			slave->targetname = NULL;//not usable ever again
		}
		slave->spawnflags &= ~MOVER_LOCKED;
		slave->s.frame = 1;//second stage of anim
		slave = slave->teamchain;
	} while (slave);
}
void LockDoors(gentity_t *const ent)
{
	//noise?
	//go through and lock the door and all the slaves
	gentity_t	*slave = ent;
	do
	{
		slave->spawnflags |= MOVER_LOCKED;
		slave->s.frame = 0;//first stage of anim
		slave = slave->teamchain;
	} while (slave);
}
/*
================
Use_BinaryMover
================
*/
void Use_BinaryMover(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	if (!ent->use)
	{//I cannot be used anymore, must be a door with a wait of -1 that's opened.
		return;
	}

	// only the master should be used
	if (ent->flags & FL_TEAMSLAVE)
	{
		Use_BinaryMover(ent->teammaster, other, activator);
		return;
	}

	if (ent->flags & FL_INACTIVE)
	{
		return;
	}

	// setteamallow
	if (other && other->genericValue17) {
		ent->alliedTeam = other->genericValue17;
		return;
	}

	if (ent->spawnflags & MOVER_LOCKED)
	{//a locked door, unlock it
		if (other && other->genericValue16) {
			// justopen: open the locked door once instead of unlocking it
			if (ent->moverState == MOVER_1TO2)
				return; // already opening

			if (other->genericValue16 == 2) { // only open the locked door if we're on red team (if it becomes unlocked, anyone will be able to open it)
				if (!(activator && activator->client && activator->client->sess.sessionTeam == TEAM_RED))
					return;
				// fall through to open the door
			}
			else if (other->genericValue16 == 3) { // only open the locked door if we're on blue team (if it becomes unlocked, anyone will be able to open it)
				if (!(activator && activator->client && activator->client->sess.sessionTeam == TEAM_BLUE))
					return;
				// fall through to open the door
			}
			else {
				// fall through to open the door
			}
		}
		else {
			UnLockDoors(ent); // unlock the door and go no further (do not open it)
			return;
		}
	}

	G_ActivateBehavior(ent, BSET_USE);

	ent->enemy = other;
	ent->activator = activator;
	if (ent->delay)
	{
		ent->think = Use_BinaryMover_Go;
		ent->nextthink = level.time + ent->delay;
	}
	else
	{
		Use_BinaryMover_Go(ent);
	}
}



/*
================
InitMover

"pos1", "pos2", and "speed" should be set before calling,
so the movement delta can be calculated
================
*/
void InitMoverTrData(gentity_t *ent)
{
	vec3_t		move;
	float		distance;

	ent->s.pos.trType = TR_STATIONARY;
	VectorCopy(ent->pos1, ent->s.pos.trBase);

	// calculate time to reach second position from speed
	VectorSubtract(ent->pos2, ent->pos1, move);
	distance = VectorLength(move);
	if (!ent->speed)
	{
		ent->speed = 100;
	}
	VectorScale(move, ent->speed, ent->s.pos.trDelta);
	ent->s.pos.trDuration = distance * 1000 / ent->speed;
	if (ent->s.pos.trDuration <= 0)
	{
		ent->s.pos.trDuration = 1;
	}
}

void InitMover(gentity_t *ent)
{
	float		light;
	vec3_t		color;
	qboolean	lightSet, colorSet;

	// if the "model2" key is set, use a seperate model
	// for drawing, but clip against the brushes
	if (ent->model2)
	{
		if (strstr(ent->model2, ".glm"))
		{ //for now, not supported in MP.
			ent->s.modelindex2 = 0;
		}
		else
		{
			ent->s.modelindex2 = G_ModelIndex(ent->model2);
		}
	}

	// if the "color" or "light" keys are set, setup constantLight
	lightSet = G_SpawnFloat("light", "100", &light);
	colorSet = G_SpawnVector("color", "1 1 1", color);
	if (lightSet || colorSet) {
		int		r, g, b, i;

		r = color[0] * 255;
		if (r > 255) {
			r = 255;
		}
		g = color[1] * 255;
		if (g > 255) {
			g = 255;
		}
		b = color[2] * 255;
		if (b > 255) {
			b = 255;
		}
		i = light / 4;
		if (i > 255) {
			i = 255;
		}
		ent->s.constantLight = r | (g << 8) | (b << 16) | (i << 24);
	}

	ent->use = Use_BinaryMover;
	ent->reached = Reached_BinaryMover;

	ent->moverState = MOVER_POS1;
	ent->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	if (ent->spawnflags & MOVER_INACTIVE)
	{// Make it inactive
		ent->flags |= FL_INACTIVE;
	}
	if (ent->spawnflags & MOVER_PLAYER_USE)
	{//Can be used by the player's BUTTON_USE
		ent->r.svFlags |= SVF_PLAYER_USABLE;
	}
	ent->s.eType = ET_MOVER;
	VectorCopy(ent->pos1, ent->r.currentOrigin);
	trap_LinkEntity(ent);

	InitMoverTrData(ent);
}


/*
===============================================================================

DOOR

A use can be triggered either by a touch function, by being shot, or by being
targeted by another entity.

===============================================================================
*/

/*
================
Blocked_Door
================
*/
void Blocked_Door(gentity_t *ent, gentity_t *other, gentity_t *blockedBy)
{
	static qboolean fixedHothBridge = qfalse;
	if (level.siegeMap == SIEGEMAP_HOTH && !fixedHothBridge && !Q_stricmp(ent->target, "droptheclip") && !Q_stricmp(ent->targetname, "bridge")) {
		ent->damage = 9999;
		ent->spawnflags |= MOVER_CRUSHER;
		fixedHothBridge = qtrue;
	}

	// fix issue with horizontal "doors" being way too easily greenable with a detpack placed on top
	if (level.siegeMap == SIEGEMAP_CARGO && other && !Q_stricmp(other->classname, "detpack") &&
		other->r.currentOrigin[0] >= 7034 && other->r.currentOrigin[0] <= 7322 &&
		other->r.currentOrigin[1] >= -456 && other->r.currentOrigin[1] <= -110) {
		return;
	}

	if (g_fixLiftkillTraps.integer && blockedBy &&
		(!Q_stricmp(blockedBy->classname, "sentryGun") || !Q_stricmp(blockedBy->classname, "item_shield")) &&
		other && other - g_entities < MAX_CLIENTS) {
		G_Damage(blockedBy, ent, ent, NULL, NULL, 999999, 0, MOD_CRUSH);
	}
	else {
		if (ent->damage) {
			// duo: properly credit liftkills
			if (blockedBy && other && other - g_entities < MAX_CLIENTS) {
				if (blockedBy - g_entities < MAX_CLIENTS)
					G_Damage(other, blockedBy, blockedBy, NULL, NULL, ent->damage, 0, MOD_CRUSH); // killed by player
				else if (blockedBy->parent && blockedBy->parent - g_entities < MAX_CLIENTS)
					G_Damage(other, blockedBy->parent, blockedBy->parent, NULL, NULL, ent->damage, 0, MOD_CRUSH); // killed by something owned by a player
				else
					G_Damage(other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH);
			}
			else {
				G_Damage(other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH);
			}
		}
	}


	if (ent->spawnflags & MOVER_CRUSHER) {
		return;		// crushers don't reverse
	}

	// reverse direction
	Use_BinaryMover(ent, ent, other);
}


/*
================
Touch_DoorTriggerSpectator
================
*/
static void Touch_DoorTriggerSpectator(gentity_t *ent, gentity_t *other, trace_t *trace) {
	int i, axis, doormargin;
	trace_t tr;
	vec3_t pMins, pMaxs;
	vec3_t origin, dir, angles;

	axis = ent->count;
	VectorClear(dir);
	doormargin = (ent->r.absmax[axis] - ent->r.absmin[axis]) / 20;
	if (fabs(other->s.origin[axis] - ent->r.absmax[axis]) <
		fabs(other->s.origin[axis] - ent->r.absmin[axis])) {
		origin[axis] = ent->r.absmin[axis] - doormargin;
		dir[axis] = -1;
	}
	else {
		origin[axis] = ent->r.absmax[axis] + doormargin;
		dir[axis] = 1;
	}
	for (i = 0; i < 3; i++) {
		if (i == axis) continue;
		origin[i] = (ent->r.absmin[i] + ent->r.absmax[i]) * 0.5;
	}

	vectoangles(dir, angles);

	VectorSet(pMins, -15.0f, -15.0f, DEFAULT_MINS_2);
	VectorSet(pMaxs, 15.0f, 15.0f, DEFAULT_MAXS_2);
	trap_Trace(&tr, origin, pMins, pMaxs, origin, other->s.number, other->clipmask);
	if (!tr.startsolid &&
		!tr.allsolid &&
		tr.fraction == 1.0f &&
		tr.entityNum == ENTITYNUM_NONE)
	{
		TeleportPlayer(other, origin, angles);
	}
}

/*
================
Touch_DoorTrigger
================
*/
void Touch_DoorTrigger(gentity_t *ent, gentity_t *other, trace_t *trace)
{
	gentity_t *relockEnt = NULL;

	if (other->client && other->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		// if the door is not open and not opening
		if (ent->parent->moverState != MOVER_1TO2 &&
			ent->parent->moverState != MOVER_POS2)
		{
			Touch_DoorTriggerSpectator(ent, other, trace);
		}
		return;
	}

	if (!ent->genericValue14 &&
		(!ent->parent || !ent->parent->genericValue14))
	{
		if (other->client && other->s.number >= MAX_CLIENTS &&
			other->s.eType == ET_NPC && other->s.NPC_class == CLASS_VEHICLE)
		{ //doors don't open for vehicles
			return;
		}

		if (other->client && other->s.number < MAX_CLIENTS &&
			other->client->ps.m_iVehicleNum)
		{ //can't open a door while on a vehicle
			return;
		}
	}

	if (ent->flags & FL_INACTIVE)
	{
		return;
	}

	if (ent->parent->spawnflags & MOVER_LOCKED)
	{//don't even try to use the door if it's locked
		if (level.siegeMap == SIEGEMAP_URBAN && VALIDSTRING(ent->parent->targetname) &&
			((level.totalObjectivesCompleted < 1 && !Q_stricmp(ent->parent->targetname, "obj1to2")) ||
			(level.totalObjectivesCompleted < 2 && !Q_stricmp(ent->parent->targetname, "obj2to3")) ||
				(level.totalObjectivesCompleted < 3 && !Q_stricmp(ent->parent->targetname, "obj3to4"))))
		{
			return;
		}
		else if (!ent->parent->alliedTeam //we don't have a "teamallow" team
			|| !other->client //we do have a "teamallow" team, but this isn't a client
			||( other->client->sess.sessionTeam != ent->parent->alliedTeam && !other->client->sess.siegeDuelInProgress))//it is a client, but it's not on the right team
		{
			return;
		}
		else
		{//temporarily unlock us while we call Use_BinaryMover (so it doesn't unlock all the doors in this team)
			if (ent->parent->flags & FL_TEAMSLAVE)
			{
				relockEnt = ent->parent->teammaster;
			}
			else
			{
				relockEnt = ent->parent;
			}
			if (relockEnt != NULL)
			{
				relockEnt->spawnflags &= ~MOVER_LOCKED;
			}
		}
	}

	if (ent->parent->moverState != MOVER_1TO2)
	{//Door is not already opening
	 //if ( ent->parent->moverState == MOVER_POS1 || ent->parent->moverState == MOVER_2TO1 )
	 //{//only check these if closed or closing

	 //If door is closed, opening or open, check this
		Use_BinaryMover(ent->parent, ent, other);
	}
	if (relockEnt != NULL)
	{//re-lock us
		relockEnt->spawnflags |= MOVER_LOCKED;
	}
}

/*
======================
Think_SpawnNewDoorTrigger

All of the parts of a door have been spawned, so create
a trigger that encloses all of them
======================
*/
void Think_SpawnNewDoorTrigger(gentity_t *ent)
{
	gentity_t		*other;
	vec3_t		mins, maxs;
	int			i, best;

	// set all of the slaves as shootable
	if (ent->takedamage)
	{
		for (other = ent; other; other = other->teamchain)
		{
			other->takedamage = qtrue;
		}
	}

	// find the bounds of everything on the team
	VectorCopy(ent->r.absmin, mins);
	VectorCopy(ent->r.absmax, maxs);

	for (other = ent->teamchain; other; other = other->teamchain) {
		AddPointToBounds(other->r.absmin, mins, maxs);
		AddPointToBounds(other->r.absmax, mins, maxs);
	}

	// find the thinnest axis, which will be the one we expand
	best = 0;
	for (i = 1; i < 3; i++) {
		if (maxs[i] - mins[i] < maxs[best] - mins[best]) {
			best = i;
		}
	}
	maxs[best] += 120;
	mins[best] -= 120;

	// create a trigger with this size
	other = G_Spawn();
	VectorCopy(mins, other->r.mins);
	VectorCopy(maxs, other->r.maxs);
	other->parent = ent;
	other->r.contents = CONTENTS_TRIGGER;
	other->touch = Touch_DoorTrigger;
	trap_LinkEntity(other);
	other->classname = "trigger_door";
	// remember the thinnest axis
	other->count = best;

	MatchTeam(ent, ent->moverState, level.time);
}

void Think_MatchTeam(gentity_t *ent)
{
	MatchTeam(ent, ent->moverState, level.time);
}

qboolean G_EntIsDoor(int entityNum)
{
	gentity_t *ent;

	if (entityNum < 0 || entityNum >= ENTITYNUM_WORLD)
	{
		return qfalse;
	}

	ent = &g_entities[entityNum];
	if (ent && !Q_stricmp("func_door", ent->classname))
	{//blocked by a door
		return qtrue;
	}
	return qfalse;
}

gentity_t *G_FindDoorTrigger(gentity_t *ent)
{
	gentity_t *owner = NULL;
	gentity_t *door = ent;
	if (door->flags & FL_TEAMSLAVE)
	{//not the master door, get the master door
		while (door->teammaster && (door->flags&FL_TEAMSLAVE))
		{
			door = door->teammaster;
		}
	}
	if (door->targetname)
	{//find out what is targeting it
	 //FIXME: if ent->targetname, check what kind of trigger/ent is targetting it?  If a normal trigger (active, etc), then it's okay?
		while ((owner = G_Find(owner, FOFS(target), door->targetname)) != NULL)
		{
			if (owner && (owner->r.contents&CONTENTS_TRIGGER))
			{
				return owner;
			}
		}
		owner = NULL;
		while ((owner = G_Find(owner, FOFS(target2), door->targetname)) != NULL)
		{
			if (owner && (owner->r.contents&CONTENTS_TRIGGER))
			{
				return owner;
			}
		}
	}

	owner = NULL;
	while ((owner = G_Find(owner, FOFS(classname), "trigger_door")) != NULL)
	{
		if (owner->parent == door)
		{
			return owner;
		}
	}

	return NULL;
}

qboolean G_EntIsUnlockedDoor(int entityNum)
{
	if (entityNum < 0 || entityNum >= ENTITYNUM_WORLD)
	{
		return qfalse;
	}

	if (G_EntIsDoor(entityNum))
	{
		gentity_t *ent = &g_entities[entityNum];
		gentity_t *owner = NULL;
		if (ent->flags & FL_TEAMSLAVE)
		{//not the master door, get the master door
			while (ent->teammaster && (ent->flags&FL_TEAMSLAVE))
			{
				ent = ent->teammaster;
			}
		}
		if (ent->targetname)
		{//find out what is targetting it
			owner = NULL;
			//FIXME: if ent->targetname, check what kind of trigger/ent is targetting it?  If a normal trigger (active, etc), then it's okay?
			while ((owner = G_Find(owner, FOFS(target), ent->targetname)) != NULL)
			{
				if (!Q_stricmp("trigger_multiple", owner->classname))//FIXME: other triggers okay too?
				{
					if (!(owner->flags & FL_INACTIVE))
					{
						return qtrue;
					}
				}
			}
			owner = NULL;
			while ((owner = G_Find(owner, FOFS(target2), ent->targetname)) != NULL)
			{
				if (!Q_stricmp("trigger_multiple", owner->classname))//FIXME: other triggers okay too?
				{
					if (!(owner->flags & FL_INACTIVE))
					{
						return qtrue;
					}
				}
			}
			return qfalse;
		}
		else
		{//check the door's auto-created trigger instead
			owner = G_FindDoorTrigger(ent);
			if (owner && (owner->flags&FL_INACTIVE))
			{//owning auto-created trigger is inactive
				return qfalse;
			}
		}
		if (!(ent->flags & FL_INACTIVE) && //assumes that the reactivate trigger isn't right next to the door!
			!ent->health &&
			!(ent->spawnflags & MOVER_PLAYER_USE) &&
			!(ent->spawnflags & MOVER_FORCE_ACTIVATE) &&
			!(ent->spawnflags & MOVER_LOCKED))
			//FIXME: what about MOVER_GOODIE?
		{
			return qtrue;
		}
	}
	return qfalse;
}


/*QUAKED func_door (0 .5 .8) ? START_OPEN FORCE_ACTIVATE CRUSHER TOGGLE LOCKED x PLAYER_USE INACTIVE
START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
FORCE_ACTIVATE	Can only be activated by a force push or pull
CRUSHER		?
TOGGLE		wait in both the start and end states for a trigger event - does NOT work on Trek doors.
LOCKED		Starts locked, with the shader animmap at the first frame and inactive.  Once used, the animmap changes to the second frame and the door operates normally.  Note that you cannot use the door again after this.
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

"target"     Door fires this when it starts moving from it's closed position to its open position.
"opentarget" Door fires this after reaching its "open" position
"target2"    Door fires this when it starts moving from it's open position to its closed position.
"closetarget" Door fires this after reaching its "closed" position
"model2"	.md3 model to also draw
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"delay"		when used, how many seconds to wait before moving - default is none
"lip"		lip remaining at end of move (8 default)
"dmg"		damage to inflict when blocked (2 default, set to negative for no damage)
"color"		constantLight color
"light"		constantLight radius
"health"	if set, the door must be shot open
"linear"	set to 1 and it will move linearly rather than with acceleration (default is 0)
"teamallow" even if locked, this team can always open and close it just by walking up to it
0 - none (locked to everyone)
1 - red
2 - blue
"vehopen"	if non-0, vehicles/players riding vehicles can open
*/
void SP_func_door(gentity_t *ent)
{
	vec3_t	abs_movedir;
	float	distance;
	vec3_t	size;
	float	lip;

	G_SpawnInt("vehopen", "0", &ent->genericValue14);
	G_SpawnInt("remainusableifunlocked", "0", &ent->remainUsableIfUnlocked);

	ent->blocked = Blocked_Door;

	// default speed of 400
	if (!ent->speed)
		ent->speed = 400;

	// default wait of 2 seconds
	if (!ent->wait)
		ent->wait = 2;
	ent->wait *= 1000;

	ent->delay *= 1000;

	// default lip of 8 units
	G_SpawnFloat("lip", "8", &lip);

	// default damage of 2 points
	G_SpawnInt("dmg", "2", &ent->damage);
	if (ent->damage < 0)
	{
		ent->damage = 0;
	}

	G_SpawnInt("teamallow", "0", &ent->alliedTeam);

	// first position at start
	VectorCopy(ent->s.origin, ent->pos1);

	// calculate second position
	trap_SetBrushModel(ent, ent->model);
	G_SetMovedir(ent->s.angles, ent->movedir);
	abs_movedir[0] = fabs(ent->movedir[0]);
	abs_movedir[1] = fabs(ent->movedir[1]);
	abs_movedir[2] = fabs(ent->movedir[2]);
	VectorSubtract(ent->r.maxs, ent->r.mins, size);
	distance = DotProduct(abs_movedir, size) - lip;
	VectorMA(ent->pos1, distance, ent->movedir, ent->pos2);

	// if "start_open", reverse position 1 and 2
	if (ent->spawnflags & 1)
	{
		vec3_t	temp;

		VectorCopy(ent->pos2, temp);
		VectorCopy(ent->s.origin, ent->pos2);
		VectorCopy(temp, ent->pos1);
	}

	if (level.siegeMap == SIEGEMAP_HOTH && g_hothCodesAntirush.integer) {
		char *teamStr = NULL;
		G_SpawnString("team", "", &teamStr);
		if (!Q_stricmp(teamStr, "doorgroup1")) {
			ent->spawnflags |= MOVER_LOCKED;
			ent->alliedTeam = TEAM_BLUE;
			ent->isHothChokepointDoor = qtrue;
		}
	}
	else if (level.siegeMap == SIEGEMAP_NAR) {
		if (!Q_stricmp(ent->targetname, "imphack")) {
			ent->targetname = G_NewString("fixeddoorobj2to3");
		}
		else if (!Q_stricmp(ent->targetname, "objective3")) {
			ent->targetname = G_NewString("fixeddoorobj3to4");
		}
		else if (!Q_stricmp(ent->targetname, "objswitch") || ent - g_entities == 194) {
			ent->targetname = G_NewString("fixeddoorobj5to6");
			ent->spawnflags |= MOVER_LOCKED;
			ent->alliedTeam = TEAM_BLUE;
		}
		else if (!(ent->spawnflags & MOVER_LOCKED) && !VALIDSTRING(ent->targetname) && (ent - g_entities == 235 || ent - g_entities == 236 || ent - g_entities == 237)) {
			ent->targetname = G_NewString("fixeddoorobj2to3");
			ent->spawnflags |= MOVER_LOCKED;
			ent->alliedTeam = TEAM_BLUE;
		}
	}

	if (ent->spawnflags & MOVER_LOCKED)
	{//a locked door, set up as locked until used directly
		ent->s.eFlags |= EF_SHADER_ANIM;//use frame-controlled shader anim
		ent->s.frame = 0;//first stage of anim
	}
	InitMover(ent);

	ent->nextthink = level.time + FRAMETIME;

	if (!(ent->flags&FL_TEAMSLAVE))
	{
		int health;

		G_SpawnInt("health", "0", &health);

		if (health)
		{
			ent->takedamage = qtrue;
		}

		if (!(ent->spawnflags&MOVER_LOCKED) && (ent->targetname || health || ent->spawnflags & MOVER_PLAYER_USE || ent->spawnflags & MOVER_FORCE_ACTIVATE))
		{
			// non touch/shoot doors
			ent->think = Think_MatchTeam;

			if (ent->spawnflags & MOVER_FORCE_ACTIVATE)
			{ //so we know it's push/pullable on the client
				ent->s.bolt1 = 1;
			}
		}
		else
		{//locked doors still spawn a trigger
			ent->think = Think_SpawnNewDoorTrigger;
		}
	}

}

/*
===============================================================================

PLAT

===============================================================================
*/

/*
==============
Touch_Plat

Don't allow decent if a living player is on it
===============
*/
void Touch_Plat(gentity_t *ent, gentity_t *other, trace_t *trace) {
	if (!other->client || other->client->ps.stats[STAT_HEALTH] <= 0) {
		return;
	}

	// delay return-to-pos1 by one second
	if (ent->moverState == MOVER_POS2) {
		ent->nextthink = level.time + 1000;
	}
}

/*
==============
Touch_PlatCenterTrigger

If the plat is at the bottom position, start it going up
===============
*/
void Touch_PlatCenterTrigger(gentity_t *ent, gentity_t *other, trace_t *trace) {
	if (!other->client) {
		return;
	}
	if (other->client->emoted)
		return;

	if (ent->parent->moverState == MOVER_POS1) {
		Use_BinaryMover(ent->parent, ent, other);
	}
}


/*
==============
Touch_PlatCenterTrigger_Hoth

Require use button for Hoth bunker plat
===============
*/
void Touch_PlatCenterTrigger_Hoth(gentity_t *ent, gentity_t *other, trace_t *trace) {
	if (!other->client) {
		return;
	}
	
	if (other->client->emoted)
		return;

	if (g_fixHothBunkerLift.integer)
	{
		if (!(other->client->pers.cmd.buttons & BUTTON_USE))
		{//not pressing use button
			return;
		}

		if (ent->parent->nextthink >= level.time || ent->parent->moverState != MOVER_POS1)
		{
			return; //prevents spamming lift button if holding +use
		}

		if ((other->client->ps.weaponTime > 0 && other->client->ps.torsoAnim != BOTH_BUTTON_HOLD && other->client->ps.torsoAnim != BOTH_CONSOLE1) || other->health < 1 ||
			(other->client->ps.pm_flags & PMF_FOLLOW) || other->client->sess.sessionTeam == TEAM_SPECTATOR ||
			other->client->ps.forceHandExtend != HANDEXTEND_NONE)
		{ //player has to be free of other things to use.
			return;
		}

		if (other->client->ps.torsoAnim != BOTH_BUTTON_HOLD &&
			other->client->ps.torsoAnim != BOTH_CONSOLE1)
		{
			G_SetAnim(other, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		}
		else
		{
			other->client->ps.torsoTimer = 500;
		}
		other->client->ps.weaponTime = other->client->ps.torsoTimer;

		ent->parent->nextthink = level.time + 500;
		ent->parent->think = Use_BinaryMover_Go;
	}
	else
	{
		Use_BinaryMover(ent->parent, ent, other);
	}

	
}

/*
==============
Touch_PlatCenterTrigger_HothCodes

Anti-liftlame for hoth codes plat
===============
*/
void Touch_PlatCenterTrigger_HothCodes(gentity_t *ent, gentity_t *other, trace_t *trace) {
	if (!other->client) {
		return;
	}
	if (other->client->emoted)
		return;

	if (g_antiHothCodesLiftLame.integer && other->client->sess.sessionTeam == TEAM_BLUE) {
		int i;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (&g_entities[i] && g_entities[i].client && g_entities[i].client->holdingObjectiveItem > 0 && g_entities[i].client->sess.sessionTeam == TEAM_RED &&
				g_entities[i].health > 0 && !(g_entities[i].client->ps.fd.forcePowersKnown & (1 << FP_LEVITATION)) && !(g_entities[i].client->tempSpectate > level.time) &&
				g_entities[i].client->ps.origin[0] >= -3820 && g_entities[i].client->ps.origin[0] <= -2711 &&
				g_entities[i].client->ps.origin[1] >= 1750 && g_entities[i].client->ps.origin[1] <= 2300 &&
				g_entities[i].client->ps.origin[2] < -256) {
				return;
			}
		}
	}

	if (ent->parent->moverState == MOVER_POS1) {
		Use_BinaryMover(ent->parent, ent, other);
	}

}


/*
================
SpawnPlatTrigger

Spawn a trigger in the middle of the plat's low position
Elevator cars require that the trigger extend through the entire low position,
not just sit on top of it.
================
*/
void SpawnPlatTrigger(gentity_t *ent, qboolean isHothBunkerLift, qboolean isHothCodesLift) {
	gentity_t	*trigger;
	vec3_t	tmin, tmax;

	// the middle trigger will be a thin trigger just
	// above the starting position
	trigger = G_Spawn();
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->parent = ent;

	if (isHothBunkerLift)
	{
		trigger->touch = Touch_PlatCenterTrigger_Hoth;
	}
	else if (isHothCodesLift)
	{
		trigger->touch = Touch_PlatCenterTrigger_HothCodes;
	}
	else
	{
		trigger->touch = Touch_PlatCenterTrigger;
	}

	tmin[0] = ent->pos1[0] + ent->r.mins[0] + 33;
	tmin[1] = ent->pos1[1] + ent->r.mins[1] + 33;
	tmin[2] = ent->pos1[2] + ent->r.mins[2];

	tmax[0] = ent->pos1[0] + ent->r.maxs[0] - 33;
	tmax[1] = ent->pos1[1] + ent->r.maxs[1] - 33;
	tmax[2] = ent->pos1[2] + ent->r.maxs[2] + 8;

	if (tmax[0] <= tmin[0]) {
		tmin[0] = ent->pos1[0] + (ent->r.mins[0] + ent->r.maxs[0]) *0.5;
		tmax[0] = tmin[0] + 1;
	}
	if (tmax[1] <= tmin[1]) {
		tmin[1] = ent->pos1[1] + (ent->r.mins[1] + ent->r.maxs[1]) *0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy(tmin, trigger->r.mins);
	VectorCopy(tmax, trigger->r.maxs);

	trap_LinkEntity(trigger);
}


/*QUAKED func_plat (0 .5 .8) ? x x x x x x PLAYER_USE INACTIVE
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

Plats are always drawn in the extended position so they will light correctly.

"lip"		default 8, protrusion above rest position
"height"	total height of movement, defaults to model height
"speed"		overrides default 200.
"dmg"		overrides default 2
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_plat(gentity_t *ent) {
	float		lip, height;
	vmCvar_t    mapname;

	VectorClear(ent->s.angles);

	G_SpawnFloat("speed", "200", &ent->speed);
	G_SpawnInt("dmg", "2", &ent->damage);
	G_SpawnFloat("wait", "1", &ent->wait);
	G_SpawnFloat("lip", "8", &lip);

	ent->wait = 1000;

	// create second position
	trap_SetBrushModel(ent, ent->model);

	if (!G_SpawnFloat("height", "0", &height)) {
		height = (ent->r.maxs[2] - ent->r.mins[2]) - lip;
	}

	// pos1 is the rest (bottom) position, pos2 is the top
	VectorCopy(ent->s.origin, ent->pos2);
	VectorCopy(ent->pos2, ent->pos1);
	ent->pos1[2] -= height;

	InitMover(ent);

	// touch function keeps the plat from returning while
	// a live player is standing on it
	ent->touch = Touch_Plat;

	ent->blocked = Blocked_Door;

	ent->parent = ent;	// so it can be treated as a door

						// spawn the trigger if one hasn't been custom made

	if (!ent->targetname)
	{
		trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);
		if (!Q_stricmpn(mapname.string, "mp/siege_hoth", 13) && ent->damage == 9999) //hacktastic
		{
			if (height == 568)
				SpawnPlatTrigger(ent, qtrue, qfalse);
			else if (height == 154)
				SpawnPlatTrigger(ent, qfalse, qtrue);
			else
				SpawnPlatTrigger(ent, qfalse, qfalse);
		}
		else
		{
			SpawnPlatTrigger(ent, qfalse, qfalse);
		}
	}
}

/*
===============================================================================

BUTTON

===============================================================================
*/

/*
==============
Touch_Button

===============
*/
void Touch_Button(gentity_t *ent, gentity_t *other, trace_t *trace) {
	if (!other->client) {
		return;
	}

	if (ent->moverState == MOVER_POS1) {
		Use_BinaryMover(ent, other, other);
	}
}


/*QUAKED func_button (0 .5 .8) ? x x x x x x PLAYER_USE INACTIVE
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"model2"	.md3 model to also draw
"angle"		determines the opening direction
"target"	all entities with a matching targetname will be used
"speed"		override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_button(gentity_t *ent) {
	vec3_t		abs_movedir;
	float		distance;
	vec3_t		size;
	float		lip;

	if (!ent->speed) {
		ent->speed = 40;
	}

	if (!ent->wait) {
		ent->wait = 1;
	}
	ent->wait *= 1000;

	// first position
	VectorCopy(ent->s.origin, ent->pos1);

	// calculate second position
	trap_SetBrushModel(ent, ent->model);

	G_SpawnFloat("lip", "4", &lip);

	G_SetMovedir(ent->s.angles, ent->movedir);
	abs_movedir[0] = fabs(ent->movedir[0]);
	abs_movedir[1] = fabs(ent->movedir[1]);
	abs_movedir[2] = fabs(ent->movedir[2]);
	VectorSubtract(ent->r.maxs, ent->r.mins, size);
	distance = abs_movedir[0] * size[0] + abs_movedir[1] * size[1] + abs_movedir[2] * size[2] - lip;
	VectorMA(ent->pos1, distance, ent->movedir, ent->pos2);

	if (ent->health) {
		// shootable button
		ent->takedamage = qtrue;
	}
	else {
		// touchable button
		ent->touch = Touch_Button;
	}

	InitMover(ent);
}




/*
===============================================================================

TRAIN

===============================================================================
*/


#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

/*
===============
Think_BeginMoving

The wait time at a corner has completed, so start moving again
===============
*/
void Think_BeginMoving(gentity_t *ent) {
	G_PlayDoorSound(ent, BMS_START);
	G_PlayDoorLoopSound(ent);
	ent->s.pos.trTime = level.time;
	ent->s.pos.trType = TR_LINEAR_STOP;
}

/*
===============
Reached_Train
===============
*/
void Reached_Train(gentity_t *ent) {
	gentity_t		*next;
	float			speed;
	vec3_t			move;
	float			length;

	// copy the apropriate values
	next = ent->nextTrain;
	if (!next || !next->nextTrain) {
		return;		// just stop
	}

	// fire all other targets
	G_UseTargets(next, NULL);

	// set the new trajectory
	ent->nextTrain = next->nextTrain;
	VectorCopy(next->s.origin, ent->pos1);
	VectorCopy(next->nextTrain->s.origin, ent->pos2);

	// if the path_corner has a speed, use that
	if (next->speed) {
		speed = next->speed;
	}
	else {
		// otherwise use the train's speed
		speed = ent->speed;
	}
	if (speed < 1) {
		speed = 1;
	}

	// calculate duration
	VectorSubtract(ent->pos2, ent->pos1, move);
	length = VectorLength(move);

	ent->s.pos.trDuration = length * 1000 / speed;

	// start it going
	SetMoverState(ent, MOVER_1TO2, level.time);

	G_PlayDoorSound(ent, BMS_END);

	// if there is a "wait" value on the target, don't start moving yet
	if (next->wait) {
		ent->s.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
		ent->nextthink = level.time + next->wait * 1000;
		ent->think = Think_BeginMoving;
		ent->s.pos.trType = TR_STATIONARY;
	}
	else
	{
		G_PlayDoorLoopSound(ent);
	}
}

/*
===============
Think_SetupTrainTargets

Link all the corners together
===============
*/
void Think_SetupTrainTargets(gentity_t *ent) {
	gentity_t		*path, *next, *start;

	ent->nextTrain = G_Find(NULL, FOFS(targetname), ent->target);
	if (!ent->nextTrain) {
		Com_Printf("func_train at %s with an unfound target\n",
			vtos(ent->r.absmin));
		//Free me?`
		return;
	}

	//FIXME: this can go into an infinite loop if last path_corner doesn't link to first
	//path_corner, like so:
	// t1---->t2---->t3
	//         ^      |
	//          \_____|
	start = NULL;
	for (path = ent->nextTrain; path != start; path = next) {
		if (!start) {
			start = path;
		}

		if (!path->target) {
			//			gi.Printf( "Train corner at %s without a target\n",
			//				vtos(path->s.origin) );
			//end of path
			break;
		}

		// find a path_corner among the targets
		// there may also be other targets that get fired when the corner
		// is reached
		next = NULL;
		do {
			next = G_Find(next, FOFS(targetname), path->target);
			if (!next) {
				//end of path
				break;
			}
		} while (strcmp(next->classname, "path_corner"));

		if (next)
		{
			path->nextTrain = next;
		}
		else
		{
			break;
		}
	}

	if (!ent->targetname || (ent->spawnflags & 1) /*start on*/)
	{
		// start the train moving from the first corner
		Reached_Train(ent);
	}
	else
	{
		G_SetOrigin(ent, ent->s.origin);
	}
}


/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8)
Train path corners.
Target: next path corner and other targets to fire
"speed" speed to move to the next corner
"wait" seconds to wait before behining move to next corner
*/
void SP_path_corner(gentity_t *self) {
	if (!self->targetname) {
		G_Printf("path_corner with no targetname at %s\n", vtos(self->s.origin));
		G_FreeEntity(self);
		return;
	}
	// path corners don't need to be linked in
}



/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS ? ? CRUSH_THROUGH PLAYER_USE INACTIVE
A train is a mover that moves between path_corner target points.
Trains MUST HAVE AN ORIGIN BRUSH.
The train spawns at the first target it is pointing at.
CRUSH_THROUGH spawnflag combined with a dmg value will make the train pass through
entities and damage them on contact as well.
"model2"	.md3 model to also draw
"speed"		default 100
"dmg"		default	2
"target"	next path corner
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_train(gentity_t *self) {
	VectorClear(self->s.angles);

	if (self->spawnflags & TRAIN_BLOCK_STOPS) {
		self->damage = 0;
	}
	else {
		if (!self->damage) {
			self->damage = 2;
		}
	}

	if (!self->speed) {
		self->speed = 100;
	}

	if (!self->target) {
		G_Printf("func_train without a target at %s\n", vtos(self->r.absmin));
		G_FreeEntity(self);
		return;
	}

	trap_SetBrushModel(self, self->model);
	InitMover(self);

	self->reached = Reached_Train;

	// start trains on the second frame, to make sure their targets have had
	// a chance to spawn
	self->nextthink = level.time + FRAMETIME;
	self->think = Think_SetupTrainTargets;
}

/*
===============================================================================

STATIC

===============================================================================
*/

void func_static_use(gentity_t *self, gentity_t *other, gentity_t *activator);
/*QUAKED func_static (0 .5 .8) ? F_PUSH F_PULL SWITCH_SHADER CRUSHER IMPACT x PLAYER_USE INACTIVE BROADCAST
F_PUSH		Will be used when you Force-Push it
F_PULL		Will be used when you Force-Pull it
SWITCH_SHADER	Toggle the shader anim frame between 1 and 2 when used
CRUSHER		Make it do damage when it's blocked
IMPACT		Make it do damage when it hits any entity
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used
BROADCAST   don't ever use this, it's evil

A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"model2"	.md3 model to also draw
"model2scale"	percent of normal scale (on all x y and z axii) to scale the model2 if there is one. 100 is normal scale, min is 1 (100 times smaller than normal), max is 1000 (ten times normal).
"color"		constantLight color
"light"		constantLight radius
"dmg"		how much damage to do when it crushes (use with CRUSHER spawnflag)
"linear" set to 1 and it will move linearly rather than with acceleration (default is 0)
*/
void SP_func_static(gentity_t *ent)
{
	int		test;
	trap_SetBrushModel(ent, ent->model);

	VectorCopy(ent->s.origin, ent->pos1);
	VectorCopy(ent->s.origin, ent->pos2);

	InitMover(ent);

	ent->use = func_static_use;
	ent->reached = 0;

	G_SetOrigin(ent, ent->s.origin);
	G_SetAngles(ent, ent->s.angles);

	if (ent->spawnflags & 2048)
	{								   // yes this is very very evil, but for now (pre-alpha) it's a solution
		ent->r.svFlags |= SVF_BROADCAST; // I need to rotate something that is huge and it's touching too many area portals...
	}

	if (ent->spawnflags & 4/*SWITCH_SHADER*/)
	{
		ent->s.eFlags |= EF_SHADER_ANIM;//use frame-controlled shader anim
		ent->s.frame = 0;//first stage of anim
	}

	if ((ent->spawnflags & 1) || (ent->spawnflags & 2))
	{ //so we know it's push/pullable on the client
		ent->s.bolt1 = 1;
	}

#ifdef _XBOX
	int	tempModelScale;
	G_SpawnInt("model2scale", "0", &tempModelScale);
	ent->s.iModelScale = tempModelScale;
#else
	G_SpawnInt("model2scale", "0", &ent->s.iModelScale);
#endif
	if (ent->s.iModelScale < 0)
	{
		ent->s.iModelScale = 0;
	}
	else if (ent->s.iModelScale > 1023)
	{
		ent->s.iModelScale = 1023;
	}

	G_SpawnInt("hyperspace", "0", &test);
	if (test)
	{
		ent->r.svFlags |= SVF_BROADCAST; // I need to rotate something that is huge and it's touching too many area portals...
		ent->s.eFlags2 |= EF2_HYPERSPACE;
	}

	trap_LinkEntity(ent);

	if (level.mBSPInstanceDepth)
	{	// this means that this guy will never be updated, moved, changed, etc.
		ent->s.eFlags = EF_PERMANENT;
	}
}

void func_static_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self, BSET_USE);

	if (self->spawnflags & 4/*SWITCH_SHADER*/)
	{
		self->s.frame = self->s.frame ? 0 : 1;//toggle frame
	}
	G_UseTargets(self, activator);
}

/*
===============================================================================

ROTATING

===============================================================================
*/

void func_rotating_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	if (self->s.apos.trType == TR_LINEAR)
	{
		self->s.apos.trType = TR_STATIONARY;
		// stop the sound if it stops moving
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
		// play stop sound too?
		if (self->soundSet && self->soundSet[0])
		{
			self->s.soundSetIndex = G_SoundSetIndex(self->soundSet);
			G_AddEvent(self, EV_BMODEL_SOUND, BMS_END);
		}
	}
	else
	{
		if (self->soundSet && self->soundSet[0])
		{
			self->s.soundSetIndex = G_SoundSetIndex(self->soundSet);
			G_AddEvent(self, EV_BMODEL_SOUND, BMS_START);
			self->s.loopSound = BMS_MID;
			self->s.loopIsSoundset = qtrue;
		}
		self->s.apos.trType = TR_LINEAR;
	}
}

/*QUAKED func_rotating (0 .5 .8) ? START_ON RADAR X_AXIS Y_AXIS IMPACT x PLAYER_USE INACTIVE
RADAR - shows up on Radar in Siege mode (good for asteroids when you're in a ship)
IMPACT - Kills anything on impact when rotating (10000 points of damage, unless specified otherwise)

You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"model2"		.md3 model to also draw
"model2scale"	percent of normal scale (on all x y and z axii) to scale the model2 if there is one. 100 is normal scale, min is 1 (100 times smaller than normal), max is 1000 (ten times normal).
"speed"			determines how fast it moves; default value is 100.
"dmg"			damage to inflict when blocked (2 default)
"color"			constantLight color
"light"			constantLight radius
"spinangles"	instead of using "speed", you can set use this to set rotation on all 3 axes (pitch yaw and roll)

==================================================
"health"	default is 0

If health is set, the following key/values are available:

"numchunks" Multiplies the number of chunks spawned.  Chunk code tries to pick a good volume of chunks, but you can alter this to scale the number of spawned chunks. (default 1)  (.5) is half as many chunks, (2) is twice as many chunks
"chunksize"	scales up/down the chunk size by this number (default is 1)

"showhealth" if non-0, will display the health bar on the hud when the crosshair is over this ent (in siege)
"teamowner" in siege this will specify which team this thing is "owned" by. To that team the crosshair will
highlight green, to the other team it will highlight red.

Damage: default is none
"splashDamage" - damage to do
"splashRadius" - radius for above damage

"team" - If set, only this team can trip this trigger
0 - any
1 - red
2 - blue

Don't know if these work:
"color"		constantLight color
"light"		constantLight radius

"material" - default is "0 - MAT_METAL" - choose from this list:
0 = MAT_METAL		(basic blue-grey scorched-DEFAULT)
1 = MAT_GLASS
2 = MAT_ELECTRICAL	(sparks only)
3 = MAT_ELEC_METAL	(METAL2 chunks and sparks)
4 =	MAT_DRK_STONE	(brown stone chunks)
5 =	MAT_LT_STONE	(tan stone chunks)
6 =	MAT_GLASS_METAL (glass and METAL2 chunks)
7 = MAT_METAL2		(electronic type of metal)
8 = MAT_NONE		(no chunks)
9 = MAT_GREY_STONE	(grey colored stone)
10 = MAT_METAL3		(METAL and METAL2 chunk combo)
11 = MAT_CRATE1		(yellow multi-colored crate chunks)
12 = MAT_GRATE1		(grate chunks--looks horrible right now)
13 = MAT_ROPE		(for yavin_trial, no chunks, just wispy bits )
14 = MAT_CRATE2		(red multi-colored crate chunks)
15 = MAT_WHITE_METAL (white angular chunks for Stu, NS_hideout )
16 = MAT_SNOWY_ROCK	 (mix of gray and brown rocks)

Applicable only during Siege gametype:
teamnodmg - if 1, team 1 can't damage this. If 2, team 2 can't damage this.

*/
void SP_func_breakable(gentity_t *self);
void SP_func_rotating(gentity_t *ent) {
	vec3_t spinangles;
	if (ent->health)
	{
		int sav_spawnflags = ent->spawnflags;
		ent->spawnflags = 0;
		SP_func_breakable(ent);
		ent->spawnflags = sav_spawnflags;
	}
	else
	{
		trap_SetBrushModel(ent, ent->model);
		InitMover(ent);

		VectorCopy(ent->s.origin, ent->s.pos.trBase);
		VectorCopy(ent->s.pos.trBase, ent->r.currentOrigin);
		VectorCopy(ent->s.apos.trBase, ent->r.currentAngles);

		trap_LinkEntity(ent);
	}

#ifdef _XBOX
	int	tempModelScale;
	G_SpawnInt("model2scale", "0", &tempModelScale);
	ent->s.iModelScale = tempModelScale;
#else
	G_SpawnInt("model2scale", "0", &ent->s.iModelScale);
#endif
	if (ent->s.iModelScale < 0)
	{
		ent->s.iModelScale = 0;
	}
	else if (ent->s.iModelScale > 1023)
	{
		ent->s.iModelScale = 1023;
	}

	if (G_SpawnVector("spinangles", "0 0 0", spinangles))
	{
		ent->speed = VectorLength(spinangles);
		// set the axis of rotation
		VectorCopy(spinangles, ent->s.apos.trDelta);
	}
	else
	{
		if (!ent->speed) {
			ent->speed = 100;
		}
		// set the axis of rotation
		if (ent->spawnflags & 4) {
			ent->s.apos.trDelta[2] = ent->speed;
		}
		else if (ent->spawnflags & 8) {
			ent->s.apos.trDelta[0] = ent->speed;
		}
		else {
			ent->s.apos.trDelta[1] = ent->speed;
		}
	}
	ent->s.apos.trType = TR_LINEAR;

	if (!ent->damage) {
		if ((ent->spawnflags & 16))//IMPACT
		{
			ent->damage = 10000;
		}
		else
		{
			ent->damage = 2;
		}
	}
	if ((ent->spawnflags & 2))//RADAR
	{//show up on Radar at close range and play impact sound when close...?  Range based on my size
		ent->s.speed = Distance(ent->r.absmin, ent->r.absmax)*0.5f;
		ent->s.eFlags |= EF_RADAROBJECT;
	}
}


/*
===============================================================================

BOBBING

===============================================================================
*/


/*QUAKED func_bobbing (0 .5 .8) ? X_AXIS Y_AXIS x x x x PLAYER_USE INACTIVE
Normally bobs on the Z axis
"model2"	.md3 model to also draw
"height"	amplitude of bob (32 default)
"speed"		seconds to complete a bob cycle (4 default)
"phase"		the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_bobbing(gentity_t *ent) {
	float		height;
	float		phase;

	G_SpawnFloat("speed", "4", &ent->speed);
	G_SpawnFloat("height", "32", &height);
	G_SpawnInt("dmg", "2", &ent->damage);
	G_SpawnFloat("phase", "0", &phase);

	trap_SetBrushModel(ent, ent->model);
	InitMover(ent);

	VectorCopy(ent->s.origin, ent->s.pos.trBase);
	VectorCopy(ent->s.origin, ent->r.currentOrigin);

	ent->s.pos.trDuration = ent->speed * 1000;
	ent->s.pos.trTime = ent->s.pos.trDuration * phase;
	ent->s.pos.trType = TR_SINE;

	// set the axis of bobbing
	if (ent->spawnflags & 1) {
		ent->s.pos.trDelta[0] = height;
	}
	else if (ent->spawnflags & 2) {
		ent->s.pos.trDelta[1] = height;
	}
	else {
		ent->s.pos.trDelta[2] = height;
	}
}

/*
===============================================================================

PENDULUM

===============================================================================
*/


/*QUAKED func_pendulum (0 .5 .8) ? x x x x x x PLAYER_USE INACTIVE
You need to have an origin brush as part of this entity.
Pendulums always swing north / south on unrotated models.  Add an angles field to the model to allow rotation in other directions.
Pendulum frequency is a physical constant based on the length of the beam and gravity.
"model2"	.md3 model to also draw
"speed"		the number of degrees each way the pendulum swings, (30 default)
"phase"		the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_pendulum(gentity_t *ent) {
	float		freq;
	float		length;
	float		phase;
	float		speed;

	G_SpawnFloat("speed", "30", &speed);
	G_SpawnInt("dmg", "2", &ent->damage);
	G_SpawnFloat("phase", "0", &phase);

	trap_SetBrushModel(ent, ent->model);

	// find pendulum length
	length = fabs(ent->r.mins[2]);
	if (length < 8) {
		length = 8;
	}

	freq = 1 / (M_PI * 2) * sqrt(g_gravity.value / (3 * length));

	ent->s.pos.trDuration = (1000 / freq);

	InitMover(ent);

	VectorCopy(ent->s.origin, ent->s.pos.trBase);
	VectorCopy(ent->s.origin, ent->r.currentOrigin);

	VectorCopy(ent->s.angles, ent->s.apos.trBase);

	ent->s.apos.trDuration = 1000 / freq;
	ent->s.apos.trTime = ent->s.apos.trDuration * phase;
	ent->s.apos.trType = TR_SINE;
	ent->s.apos.trDelta[2] = speed;
}

/*
===============================================================================

BREAKABLE BRUSH

===============================================================================
*/
//---------------------------------------------------
static void CacheChunkEffects(material_t material)
{
	switch (material)
	{
	case MAT_GLASS:
		G_EffectIndex("chunks/glassbreak");
		break;
	case MAT_GLASS_METAL:
		G_EffectIndex("chunks/glassbreak");
		G_EffectIndex("chunks/metalexplode");
		break;
	case MAT_ELECTRICAL:
	case MAT_ELEC_METAL:
		G_EffectIndex("chunks/sparkexplode");
		break;
	case MAT_METAL:
	case MAT_METAL2:
	case MAT_METAL3:
	case MAT_CRATE1:
	case MAT_CRATE2:
		G_EffectIndex("chunks/metalexplode");
		break;
	case MAT_GRATE1:
		G_EffectIndex("chunks/grateexplode");
		break;
	case MAT_DRK_STONE:
	case MAT_LT_STONE:
	case MAT_GREY_STONE:
	case MAT_WHITE_METAL: // what is this crap really supposed to be??
	case MAT_SNOWY_ROCK:
		G_EffectIndex("chunks/rockbreaklg");
		G_EffectIndex("chunks/rockbreakmed");
		break;
	case MAT_ROPE:
		G_EffectIndex("chunks/ropebreak");
		break;
	}
}

void G_MiscModelExplosion(vec3_t mins, vec3_t maxs, int size, material_t chunkType)
{
	gentity_t *te;
	vec3_t mid;

	VectorAdd(mins, maxs, mid);
	VectorScale(mid, 0.5f, mid);

	te = G_TempEntity(mid, EV_MISC_MODEL_EXP);

	VectorCopy(maxs, te->s.origin2);
	VectorCopy(mins, te->s.angles2);
	te->s.time = size;
	te->s.eventParm = chunkType;
}

void G_Chunks(int owner, vec3_t origin, const vec3_t normal, const vec3_t mins, const vec3_t maxs,
	float speed, int numChunks, material_t chunkType, int customChunk, float baseScale)
{
	gentity_t *te = G_TempEntity(origin, EV_DEBRIS);

	//Now it's time to cram everything horribly into the entitystate of an event entity.
	te->s.owner = owner;
	VectorCopy(origin, te->s.origin);
	VectorCopy(normal, te->s.angles);
	VectorCopy(maxs, te->s.origin2);
	VectorCopy(mins, te->s.angles2);
	te->s.speed = speed;
	te->s.eventParm = numChunks;
	te->s.trickedentindex = chunkType;
	te->s.modelindex = customChunk;
	te->s.apos.trBase[0] = baseScale;
}

//--------------------------------------
void funcBBrushDieGo(gentity_t *self)
{
	vec3_t		org, dir, up;
	gentity_t	*attacker = self->enemy;
	float		scale;
	int			i, numChunks, size = 0;
	material_t	chunkType = self->material;

	// if a missile is stuck to us, blow it up so we don't look dumb
	for (i = 0; i < MAX_GENTITIES; i++)
	{
		if (g_entities[i].s.groundEntityNum == self->s.number && (g_entities[i].s.eFlags & EF_MISSILE_STICK))
		{
			G_Damage(&g_entities[i], self, self, NULL, NULL, 99999, 0, MOD_CRUSH); //?? MOD?
		}
	}

	//So chunks don't get stuck inside me
	self->s.solid = 0;
	self->r.contents = 0;
	self->clipmask = 0;
	trap_LinkEntity(self);

	VectorSet(up, 0, 0, 1);

	if (self->target && attacker != NULL)
	{
		G_UseTargets(self, attacker);
	}

	VectorSubtract(self->r.absmax, self->r.absmin, org);// size

	numChunks = random() * 6 + 18;

	// This formula really has no logical basis other than the fact that it seemed to be the closest to yielding the results that I wanted.
	// Volume is length * width * height...then break that volume down based on how many chunks we have
	scale = sqrt(sqrt(org[0] * org[1] * org[2])) * 1.75f;

	if (scale > 48)
	{
		size = 2;
	}
	else if (scale > 24)
	{
		size = 1;
	}

	scale = scale / numChunks;

	if (self->radius > 0.0f)
	{
		// designer wants to scale number of chunks, helpful because the above scale code is far from perfect
		//	I do this after the scale calculation because it seems that the chunk size generally seems to be very close, it's just the number of chunks is a bit weak
		numChunks *= self->radius;
	}

	VectorMA(self->r.absmin, 0.5, org, org);
	VectorAdd(self->r.absmin, self->r.absmax, org);
	VectorScale(org, 0.5f, org);

	if (attacker != NULL && attacker->client)
	{
		VectorSubtract(org, attacker->r.currentOrigin, dir);
		VectorNormalize(dir);
	}
	else
	{
		VectorCopy(up, dir);
	}

	if (!(self->spawnflags & 2048)) // NO_EXPLOSION
	{
		// we are allowed to explode
		G_MiscModelExplosion(self->r.absmin, self->r.absmax, size, chunkType);
	}

	if (self->genericValue15)
	{ //a custom effect to play
		vec3_t ang;
		VectorSet(ang, 0.0f, 1.0f, 0.0f);
		G_PlayEffectID(self->genericValue15, org, ang);
	}

	if (self->splashDamage > 0 && self->splashRadius > 0)
	{
		gentity_t *te;
		//explode
		G_RadiusDamage(org, self, self->splashDamage, self->splashRadius, self, NULL, MOD_UNKNOWN);

		te = G_TempEntity(org, EV_GENERAL_SOUND);
		te->s.eventParm = G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");
	}

	//FIXME: base numChunks off size?
	G_Chunks(self->s.number, org, dir, self->r.absmin, self->r.absmax, 300, numChunks, chunkType, 0, (scale*self->mass));

	trap_AdjustAreaPortalState(self, qtrue);
	self->think = G_FreeEntity;
	self->nextthink = level.time + 50;
}

void funcBBrushDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	self->takedamage = qfalse;//stop chain reaction runaway loops

	self->enemy = attacker;

	if (self->delay)
	{
		self->think = funcBBrushDieGo;
		self->nextthink = level.time + floor(self->delay * 1000.0f);
		return;
	}

	funcBBrushDieGo(self);
}

void funcBBrushUse(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self, BSET_USE);
	if (self->spawnflags & 64)
	{//Using it doesn't break it, makes it use it's targets
		if (self->target && self->target[0])
		{
			G_UseTargets(self, activator);
		}
	}
	else
	{
		funcBBrushDie(self, other, activator, self->health, MOD_UNKNOWN);
	}
}

void funcBBrushPain(gentity_t *self, gentity_t *attacker, int damage)
{
	if (self->painDebounceTime > level.time)
	{
		return;
	}

	if (self->paintarget && self->paintarget[0])
	{
		if (!self->activator)
		{
			if (attacker && attacker->inuse && attacker->client)
			{
				G_UseTargets2(self, attacker, self->paintarget);
			}
		}
		else
		{
			G_UseTargets2(self, self->activator, self->paintarget);
		}
	}

	G_ActivateBehavior(self, BSET_PAIN);

	if (self->material == MAT_DRK_STONE
		|| self->material == MAT_LT_STONE
		|| self->material == MAT_GREY_STONE
		|| self->material == MAT_SNOWY_ROCK)
	{
		vec3_t	org, dir;
		float	scale;
		int		numChunks;
		VectorSubtract(self->r.absmax, self->r.absmin, org);// size
															// This formula really has no logical basis other than the fact that it seemed to be the closest to yielding the results that I wanted.
															// Volume is length * width * height...then break that volume down based on how many chunks we have
		scale = VectorLength(org) / 100.0f;
		VectorMA(self->r.absmin, 0.5, org, org);
		VectorAdd(self->r.absmin, self->r.absmax, org);
		VectorScale(org, 0.5f, org);
		if (attacker != NULL && attacker->client)
		{
			VectorSubtract(attacker->r.currentOrigin, org, dir);
			VectorNormalize(dir);
		}
		else
		{
			VectorSet(dir, 0, 0, 1);
		}
		numChunks = Q_irand(1, 3);
		if (self->radius > 0.0f)
		{
			// designer wants to scale number of chunks, helpful because the above scale code is far from perfect
			//	I do this after the scale calculation because it seems that the chunk size generally seems to be very close, it's just the number of chunks is a bit weak
			numChunks = ceil(numChunks*self->radius);
		}
		G_Chunks(self->s.number, org, dir, self->r.absmin, self->r.absmax, 300, numChunks, self->material, 0, (scale*self->mass));
	}

	if (self->wait == -1)
	{
		self->pain = 0;
		return;
	}

	self->painDebounceTime = level.time + self->wait;
}

static void InitBBrush(gentity_t *ent)
{
	float		light;
	vec3_t		color;
	qboolean	lightSet, colorSet;

	VectorCopy(ent->s.origin, ent->pos1);

	trap_SetBrushModel(ent, ent->model);

	ent->die = funcBBrushDie;

	ent->flags |= FL_BBRUSH;

	//This doesn't have to be an svFlag, can just be a flag.
	//And it might not be needed anyway.

	// if the "model2" key is set, use a seperate model
	// for drawing, but clip against the brushes
	if (ent->model2 && ent->model2[0])
	{
		ent->s.modelindex2 = G_ModelIndex(ent->model2);
	}

	// if the "color" or "light" keys are set, setup constantLight
	lightSet = G_SpawnFloat("light", "100", &light);
	colorSet = G_SpawnVector("color", "1 1 1", color);
	if (lightSet || colorSet)
	{
		int		r, g, b, i;

		r = color[0] * 255;
		if (r > 255) {
			r = 255;
		}
		g = color[1] * 255;
		if (g > 255) {
			g = 255;
		}
		b = color[2] * 255;
		if (b > 255) {
			b = 255;
		}
		i = light / 4;
		if (i > 255) {
			i = 255;
		}
		ent->s.constantLight = r | (g << 8) | (b << 16) | (i << 24);
	}

	if (ent->spawnflags & 128)
	{//Can be used by the player's BUTTON_USE
		ent->r.svFlags |= SVF_PLAYER_USABLE;
	}

	ent->s.eType = ET_MOVER;
	trap_LinkEntity(ent);

	ent->s.pos.trType = TR_STATIONARY;
	VectorCopy(ent->pos1, ent->s.pos.trBase);
}

void funcBBrushTouch(gentity_t *ent, gentity_t *other, trace_t *trace)
{
	// allow touching cargo vents to destroy them
	if (g_gametype.integer != GT_SIEGE || level.siegeMap != SIEGEMAP_CARGO)
		return;
	if (!other || !other->client || !ent || !(ent->spawnflags & 8))
		return;
	if (ent - g_entities == CARGO_FANGRATING_NUM1 || ent - g_entities == CARGO_FANGRATING_NUM2)
		return;
	if (other->client->emoted)
		return;
	G_Damage(ent, NULL, NULL, NULL, ent->r.currentOrigin, 999, DAMAGE_NO_ARMOR, MOD_FALLING);
}

/*QUAKED func_breakable (0 .8 .5) ? INVINCIBLE IMPACT CRUSHER THIN SABERONLY HEAVY_WEAP USE_NOT_BREAK PLAYER_USE NO_EXPLOSION
INVINCIBLE - can only be broken by being used
IMPACT - does damage on impact
CRUSHER - won't reverse movement when hit an obstacle
THIN - can be broken by impact damage, like glass
SABERONLY - only takes damage from sabers
HEAVY_WEAP - only takes damage by a heavy weapon, like an emplaced gun or AT-ST gun.
USE_NOT_BREAK - Using it doesn't make it break, still can be destroyed by damage
PLAYER_USE - Player can use it with the use button
NO_EXPLOSION - Does not play an explosion effect, though will still create chunks if specified

When destroyed, fires it's trigger and chunks and plays sound "noise" or sound for type if no noise specified

"targetname" entities with matching target will fire it
"paintarget" target to fire when hit (but not destroyed)
"wait"		how long minimum to wait between firing paintarget each time hit
"delay"		When killed or used, how long (in seconds) to wait before blowing up (none by default)
"model2"	.md3 model to also draw
"target"	all entities with a matching targetname will be used when this is destoryed
"health"	default is 10
"numchunks" Multiplies the number of chunks spawned.  Chunk code tries to pick a good volume of chunks, but you can alter this to scale the number of spawned chunks. (default 1)  (.5) is half as many chunks, (2) is twice as many chunks
"chunksize"	scales up/down the chunk size by this number (default is 1)
"playfx"	path of effect to play on death

"showhealth" if non-0, will display the health bar on the hud when the crosshair is over this ent (in siege)
"teamowner" in siege this will specify which team this thing is "owned" by. To that team the crosshair will
highlight green, to the other team it will highlight red.

Damage: default is none
"splashDamage" - damage to do
"splashRadius" - radius for above damage

"team" - If set, only this team can trip this trigger
0 - any
1 - red
2 - blue

Don't know if these work:
"color"		constantLight color
"light"		constantLight radius

"material" - default is "0 - MAT_METAL" - choose from this list:
0 = MAT_METAL		(basic blue-grey scorched-DEFAULT)
1 = MAT_GLASS
2 = MAT_ELECTRICAL	(sparks only)
3 = MAT_ELEC_METAL	(METAL2 chunks and sparks)
4 =	MAT_DRK_STONE	(brown stone chunks)
5 =	MAT_LT_STONE	(tan stone chunks)
6 =	MAT_GLASS_METAL (glass and METAL2 chunks)
7 = MAT_METAL2		(electronic type of metal)
8 = MAT_NONE		(no chunks)
9 = MAT_GREY_STONE	(grey colored stone)
10 = MAT_METAL3		(METAL and METAL2 chunk combo)
11 = MAT_CRATE1		(yellow multi-colored crate chunks)
12 = MAT_GRATE1		(grate chunks--looks horrible right now)
13 = MAT_ROPE		(for yavin_trial, no chunks, just wispy bits )
14 = MAT_CRATE2		(red multi-colored crate chunks)
15 = MAT_WHITE_METAL (white angular chunks for Stu, NS_hideout )
16 = MAT_SNOWY_ROCK	 (mix of gray and brown rocks)

Applicable only during Siege gametype:
teamnodmg - if 1, team 1 can't damage this. If 2, team 2 can't damage this.

*/
void SP_func_breakable(gentity_t *self)
{
	int t;
	char *s = NULL;
	vmCvar_t mapname;

	G_SpawnString("playfx", "", &s);

	trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);

	//it's hackkkkkkin time
	if (level.siegeMap == SIEGEMAP_HOTH && !Q_stricmp(self->healingclass, "Rebel Tech")) {
		self->healingteam = 2;
	}
	else if (level.siegeMap == SIEGEMAP_DESERT && !Q_stricmp(self->healingclass, "Rebel Smuggler")) {
		self->healingteam = 2;
	}
	else if (level.siegeMap == SIEGEMAP_NAR && !Q_stricmp(self->healingclass, "Merc Technical Specialist")) {
		self->healingteam = 2;
	}
	else if (level.siegeMap == SIEGEMAP_CARGO && VALIDSTRING(self->healingclass)) {
		self->healingclass = NULL;
		self->healingrate = 0;
	}

	if (level.siegeMap == SIEGEMAP_NAR && !Q_stricmpn(self->target, "lastobj_lockdoor", 16)) {
		self->health = 600;
		self->healingteam = TEAM_BLUE;
		self->healingrate = 250;
		self->healingclass = G_NewString("Merc Technical Specialist");
		self->healingsound = G_NewString("sound/ambience/doomgiver/doom_bridge.wav");
		G_SoundIndex(self->healingsound);
	}

	if (s && s[0])
	{ //should we play a special death effect?
		self->genericValue15 = G_EffectIndex(s);
	}
	else
	{
		self->genericValue15 = 0;
	}

	if (!(self->spawnflags & 1))
	{
		if (!self->health)
		{
			self->health = 10;
		}
	}

	G_SpawnInt("material", "0", (int *)&self->material);

	if (g_fixHoth2ndObj.integer && g_gametype.integer == GT_SIEGE && level.siegeMap == SIEGEMAP_HOTH && self->material == 16) {
		if (self->health == 800) { // remove inner rocks
			G_FreeEntity(self);
			return;
		}
		if (self->health == 1500) { // nerf outer rocks
			self->health = 100;
		}
	}

	G_SpawnInt("showhealth", "0", &t);

	if (t)
	{ //a non-0 maxhealth value will mean we want to show the health on the hud
		self->maxHealth = self->health;
		G_ScaleNetHealth(self);
	}

	//NOTE: g_spawn.c does this automatically now

	if (level.siegeMap == SIEGEMAP_ANSION && self->spawnflags & 48) {
		self->flags |= (FL_DMG_BY_SABER_ONLY | FL_DMG_BY_HEAVY_WEAP_ONLY);
	}
	else {
		if (self->spawnflags & 16) // saber only
			self->flags |= FL_DMG_BY_SABER_ONLY;
		else if (self->spawnflags & 32)
			self->flags |= FL_DMG_BY_HEAVY_WEAP_ONLY;
	}

	if (self->health)
	{
		self->takedamage = qtrue;
	}

	G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");//precaching
	G_SpawnFloat("radius", "1", &self->radius); // used to scale chunk code if desired by a designer

	G_SpawnInt("splashDamage", "0", &self->splashDamage);
	G_SpawnInt("splashRadius", "0", &self->splashRadius);
	int noTouchBreak;
	G_SpawnInt("noTouchBreak", "0", &noTouchBreak);
	self->noTouchBreak = !!noTouchBreak;

	CacheChunkEffects(self->material);

	self->use = funcBBrushUse;

	//if ( self->paintarget )
	{
		self->pain = funcBBrushPain;
	}

	self->touch = funcBBrushTouch;

	if (self->team && self->team[0] && g_gametype.integer == GT_SIEGE &&
		!self->teamnodmg)
	{
		self->teamnodmg = atoi(self->team);
	}
	self->team = NULL;
	if (!self->model) {
		G_Error("func_breakable with NULL model\n");
	}
	InitBBrush(self);

	if (!self->radius)
	{//numchunks multiplier
		self->radius = 1.0f;
	}
	if (!self->mass)
	{//chunksize multiplier
		self->mass = 1.0f;
	}
	self->genericValue4 = 1; //so damage sys knows it's a bbrush
	if (!Q_stricmp(mapname.string, "siege_cargobarge") && self->wait == 5)
	{
		self->wait = 5000; //fix oink's derp...
	}
}

qboolean G_EntIsBreakable(int entityNum)
{
	gentity_t *ent;

	if (entityNum < 0 || entityNum >= ENTITYNUM_WORLD)
	{
		return qfalse;
	}

	ent = &g_entities[entityNum];
	if ((ent->r.svFlags & SVF_GLASS_BRUSH))
	{
		return qtrue;
	}
	if (!Q_stricmp("func_breakable", ent->classname))
	{
		return qtrue;
	}

	if (!Q_stricmp("misc_model_breakable", ent->classname))
	{
		return qtrue;
	}
	if (!Q_stricmp("misc_maglock", ent->classname))
	{
		return qtrue;
	}

	return qfalse;
}

/*
===============================================================================

GLASS

===============================================================================
*/
void GlassDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	gentity_t *te;
	vec3_t dif;

	if (self->genericValue5)
	{ //was already destroyed, do not retrigger it
		return;
	}

	self->genericValue5 = 1;

	dif[0] = (self->r.absmax[0] + self->r.absmin[0]) / 2;
	dif[1] = (self->r.absmax[1] + self->r.absmin[1]) / 2;
	dif[2] = (self->r.absmax[2] + self->r.absmin[2]) / 2;

	G_UseTargets(self, attacker);

	self->splashRadius = 40; // ?? some random number, maybe it's ok?

	te = G_TempEntity(dif, EV_GLASS_SHATTER);
	te->s.genericenemyindex = self->s.number;
	VectorCopy(self->pos1, te->s.origin);
	VectorCopy(self->pos2, te->s.angles);
	te->s.trickedentindex = (int)self->splashRadius;
	te->s.pos.trTime = (int)self->genericValue3;

	G_FreeEntity(self);
}

void GlassDie_Old(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	gentity_t *te;
	vec3_t dif;

	dif[0] = (self->r.absmax[0] + self->r.absmin[0]) / 2;
	dif[1] = (self->r.absmax[1] + self->r.absmin[1]) / 2;
	dif[2] = (self->r.absmax[2] + self->r.absmin[2]) / 2;

	G_UseTargets(self, attacker);

	te = G_TempEntity(dif, EV_GLASS_SHATTER);
	te->s.genericenemyindex = self->s.number;
	VectorCopy(self->r.maxs, te->s.origin);
	VectorCopy(self->r.mins, te->s.angles);

	G_FreeEntity(self);
}

void GlassPain(gentity_t *self, gentity_t *attacker, int damage)
{
	//Make "cracking" sound?
}

void GlassUse(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	vec3_t temp1, temp2;

	//no direct object to blame for the break, so fill the values with whatever
	VectorAdd(self->r.mins, self->r.maxs, temp1);
	VectorScale(temp1, 0.5f, temp1);

	VectorAdd(other->r.mins, other->r.maxs, temp2);
	VectorScale(temp2, 0.5f, temp2);

	VectorSubtract(temp1, temp2, self->pos2);
	VectorCopy(temp1, self->pos1);

	VectorNormalize(self->pos2);
	VectorScale(self->pos2, 390, self->pos2);

	GlassDie(self, other, activator, 100, MOD_UNKNOWN);
}

void GlassInstaBreakOnTouch(gentity_t *ent, gentity_t *other, trace_t *trace)
{
	// allow touching cargo glass to destroy it
	if (g_gametype.integer != GT_SIEGE || level.siegeMap != SIEGEMAP_CARGO)
		return;
	if (!other || !other->client || !ent)
		return;
	if (other->client->emoted)
		return;
	gentity_t *te;
	vec3_t dif;

	if (ent->genericValue5)
	{ //was already destroyed, do not retrigger it
		return;
	}

	ent->genericValue5 = 1;

	dif[0] = (ent->r.absmax[0] + ent->r.absmin[0]) / 2;
	dif[1] = (ent->r.absmax[1] + ent->r.absmin[1]) / 2;
	dif[2] = (ent->r.absmax[2] + ent->r.absmin[2]) / 2;

	G_UseTargets(ent, other);

	ent->splashRadius = 40; // ?? some random number, maybe it's ok?

	te = G_TempEntity(dif, EV_GLASS_SHATTER);
	te->s.genericenemyindex = ent->s.number;
	VectorCopy(ent->pos1, te->s.origin);
	VectorCopy(ent->pos2, te->s.angles);
	te->s.trickedentindex = (int)ent->splashRadius;
	te->s.pos.trTime = (int)ent->genericValue3;

	G_FreeEntity(ent);
}

/*QUAKED func_glass (0 .5 .8) ? x x x x x x PLAYER_USE INACTIVE
Breakable glass
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius
"maxshards"	Max number of shards to spawn on glass break
*/
void SP_func_glass(gentity_t *ent) {
	trap_SetBrushModel(ent, ent->model);
	InitMover(ent);

	ent->r.svFlags = SVF_GLASS_BRUSH;

	VectorCopy(ent->s.origin, ent->s.pos.trBase);
	VectorCopy(ent->s.origin, ent->r.currentOrigin);
	if (!ent->health)
	{
		ent->health = 1;
	}

	G_SpawnInt("maxshards", "0", &ent->genericValue3);

	ent->genericValue1 = 0;

	ent->genericValue4 = 1;

	ent->moverState = MOVER_POS1;

	if (ent->spawnflags & 1)
	{
		ent->takedamage = qfalse;
	}
	else
	{
		ent->takedamage = qtrue;
	}

	ent->die = GlassDie;
	ent->use = GlassUse;
	ent->pain = GlassPain;
	if (g_gametype.integer == GT_SIEGE && level.siegeMap == SIEGEMAP_CARGO)
		ent->touch = GlassInstaBreakOnTouch;
}

void func_usable_use(gentity_t *self, gentity_t *other, gentity_t *activator);

extern gentity_t	*G_TestEntityPosition(gentity_t *ent);
void func_wait_return_solid(gentity_t *self)
{
	//once a frame, see if it's clear.
	self->clipmask = CONTENTS_BODY;
	if (!(self->spawnflags & 16) || G_TestEntityPosition(self) == NULL)
	{
		trap_SetBrushModel(self, self->model);
		InitMover(self);
		VectorCopy(self->s.origin, self->s.pos.trBase);
		VectorCopy(self->s.origin, self->r.currentOrigin);
		self->r.svFlags &= ~SVF_NOCLIENT;
		self->s.eFlags &= ~EF_NODRAW;
		self->use = func_usable_use;
		self->clipmask = 0;
		if (self->target2 && self->target2[0])
		{
			G_UseTargets2(self, self->activator, self->target2);
		}
	}
	else
	{
		self->clipmask = 0;
		self->think = func_wait_return_solid;
		self->nextthink = level.time + FRAMETIME;
	}
}

void func_usable_think(gentity_t *self)
{
	if (self->spawnflags & 8)
	{
		self->r.svFlags |= SVF_PLAYER_USABLE;	//Replace the usable flag
		self->use = func_usable_use;
		self->think = 0;
	}
}

qboolean G_EntIsRemovableUsable(int entNum)
{
	gentity_t *ent = &g_entities[entNum];
	if (ent->classname && !Q_stricmp("func_usable", ent->classname))
	{
		if (!(ent->s.eFlags&EF_SHADER_ANIM) && !(ent->spawnflags & 8) && ent->targetname)
		{//not just a shader-animator and not ALWAYS_ON, so it must be removable somehow
			return qtrue;
		}
	}
	return qfalse;
}

void func_usable_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{//Toggle on and off
	G_ActivateBehavior(self, BSET_USE);
	if (self->s.eFlags & EF_SHADER_ANIM)
	{//animate shader when used
		self->s.frame++;//inc frame
		if (self->s.frame > self->genericValue5)
		{//wrap around
			self->s.frame = 0;
		}
		if (self->target && self->target[0])
		{
			G_UseTargets(self, activator);
		}
	}
	else if (self->spawnflags & 8)
	{//ALWAYS_ON
	 //Remove the ability to use the entity directly
		self->r.svFlags &= ~SVF_PLAYER_USABLE;
		//also remove ability to call any use func at all!
		self->use = 0;

		if (self->target && self->target[0])
		{
			G_UseTargets(self, activator);
		}

		if (self->wait)
		{
			self->think = func_usable_think;
			self->nextthink = level.time + (self->wait * 1000);
		}

		return;
	}
	else if (!self->count)
	{//become solid again
		self->count = 1;
		func_wait_return_solid(self);
	}
	else
	{
		self->s.solid = 0;
		self->r.contents = 0;
		self->clipmask = 0;
		self->r.svFlags |= SVF_NOCLIENT;
		self->s.eFlags |= EF_NODRAW;
		self->count = 0;

		if (self->target && self->target[0])
		{
			G_UseTargets(self, activator);
		}
		self->think = 0;
		self->nextthink = -1;
	}
}

void func_usable_pain(gentity_t *self, gentity_t *attacker, int damage)
{
	GlobalUse(self, attacker, attacker);
}

void func_usable_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	self->takedamage = qfalse;
	GlobalUse(self, inflictor, attacker);
}

/*QUAKED func_usable (0 .5 .8) ? STARTOFF x x ALWAYS_ON x x PLAYER_USE INACTIVE
START_OFF - the wall will not be there
ALWAYS_ON - Doesn't toggle on and off when used, just runs usescript and fires target

A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"targetname" - When used, will toggle on and off
"target"	Will fire this target every time it is toggled OFF
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius
"usescript" script to run when turned on
"deathscript"  script to run when turned off
"wait"		amount of time before the object is usable again (only valid with ALWAYS_ON flag)
"health"	if it has health, it will be used whenever shot at/killed - if you want it to only be used once this way, set health to 1
"endframe"	Will make it animate to next shader frame when used, not turn on/off... set this to number of frames in the shader, minus 1

Applicable only during Siege gametype:
teamuser - if 1, team 2 can't use this. If 2, team 1 can't use this.

*/

void SP_func_usable(gentity_t *self)
{
	trap_SetBrushModel(self, self->model);
	InitMover(self);
	VectorCopy(self->s.origin, self->s.pos.trBase);
	VectorCopy(self->s.origin, self->r.currentOrigin);
	VectorCopy(self->s.origin, self->pos1);

	G_SpawnInt("endframe", "0", &self->genericValue5);

	if (self->model2 && self->model2[0])
	{
		if (strstr(self->model2, ".glm"))
		{ //for now, not supported in MP.
			self->s.modelindex2 = 0;
		}
		else
		{
			self->s.modelindex2 = G_ModelIndex(self->model2);
		}
	}

	self->count = 1;
	if (self->spawnflags & 1)
	{
		self->s.solid = 0;
		self->r.contents = 0;
		self->clipmask = 0;
		self->r.svFlags |= SVF_NOCLIENT;
		self->s.eFlags |= EF_NODRAW;
		self->count = 0;
	}

	self->use = func_usable_use;

	if (self->health)
	{
		self->takedamage = qtrue;
		self->die = func_usable_die;
		self->pain = func_usable_pain;
	}

	if (self->genericValue5 > 0)
	{
		self->s.frame = 0;
		self->s.eFlags |= EF_SHADER_ANIM;
		self->s.time = self->genericValue5 + 1;
	}

	trap_LinkEntity(self);
}


/*
===============================================================================

WALL

===============================================================================
*/

//static -slc
void use_wall(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(ent, BSET_USE);

	// Not there so make it there
	if (!(ent->r.contents & CONTENTS_SOLID))
	{
		ent->r.svFlags &= ~SVF_NOCLIENT;
		ent->s.eFlags &= ~EF_NODRAW;
		ent->r.contents = CONTENTS_SOLID;
		if (!(ent->spawnflags & 1))
		{//START_OFF doesn't effect area portals
			trap_AdjustAreaPortalState(ent, qfalse);
		}
	}
	// Make it go away
	else
	{
		ent->r.contents = 0;
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
		if (!(ent->spawnflags & 1))
		{//START_OFF doesn't effect area portals
			trap_AdjustAreaPortalState(ent, qtrue);
		}
	}
}

#define FUNC_WALL_OFF	1

/*QUAKED func_wall (0 .5 .8) ? START_OFF x x x x x PLAYER_USE INACTIVE
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius

START_OFF - the wall will not be there
*/
void SP_func_wall(gentity_t *ent)
{
	trap_SetBrushModel(ent, ent->model);

	VectorCopy(ent->s.origin, ent->pos1);
	VectorCopy(ent->s.origin, ent->pos2);

	InitMover(ent);
	VectorCopy(ent->s.origin, ent->s.pos.trBase);
	VectorCopy(ent->s.origin, ent->r.currentOrigin);

	// it must be START_OFF
	if (ent->spawnflags & FUNC_WALL_OFF)
	{
		ent->r.contents = 0;
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
	}

	ent->use = use_wall;

	trap_LinkEntity(ent);

}