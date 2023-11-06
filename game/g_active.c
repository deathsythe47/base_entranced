// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "bg_saga.h"

extern void Jedi_Cloak( gentity_t *self );
extern void Jedi_Decloak( gentity_t *self );

#include "namespace_begin.h"
qboolean PM_SaberInTransition( int move );
qboolean PM_SaberInStart( int move );
qboolean PM_SaberInReturn( int move );
qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
#include "namespace_end.h"
qboolean saberCheckKnockdown_DuelLoss(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

extern vmCvar_t g_saberLockRandomNess;

#define LAMING_METHOD_ITEM 1
#define LAMING_METHOD_SKIP 2


void P_SetTwitchInfo(gclient_t	*client)
{
	client->ps.painTime = level.time;
	client->ps.painDirection ^= 1;
}

/*
============
G_ResetTrail

Clear out the given client's origin trails (should be called from ClientBegin and when
the teleport bit is toggled)
============
*/
void G_ResetTrail(gentity_t *ent) {
	if (ent - g_entities >= MAX_CLIENTS)
		return;

	int		i;

	// fill up the origin trails with data (assume the current position for the last 1/2 second or so)
	level.unlagged[ent - g_entities].trailHead = MAX_UNLAGGED_TRAILS - 1;
	int now = trap_Milliseconds();
	for (i = level.unlagged[ent - g_entities].trailHead; i >= 0; i--) {
		VectorCopy(ent->r.mins, level.unlagged[ent - g_entities].trail[i].mins);
		VectorCopy(ent->r.maxs, level.unlagged[ent - g_entities].trail[i].maxs);
		VectorCopy(ent->r.currentOrigin, level.unlagged[ent - g_entities].trail[i].currentOrigin);
		//VectorCopy( ent->r.currentAngles, level.unlagged[ent - g_entities].trail[i].currentAngles );

		level.unlagged[ent - g_entities].trail[i].torsoAnim = ent->client->ps.torsoAnim;
		level.unlagged[ent - g_entities].trail[i].torsoTimer = ent->client->ps.torsoTimer;
		level.unlagged[ent - g_entities].trail[i].legsAnim = ent->client->ps.legsAnim;
		level.unlagged[ent - g_entities].trail[i].legsTimer = ent->client->ps.legsTimer;
		level.unlagged[ent - g_entities].trail[i].realAngle = ent->s.apos.trBase[YAW];
		level.unlagged[ent - g_entities].trail[i].time = now;
	}
}


/*
============
G_StoreTrail

Keep track of where the client's been (usually called every ClientThink)
============
*/
void G_StoreTrail(gentity_t *ent) {
	if (ent - g_entities >= MAX_CLIENTS)
		return;

	int		head;

	head = level.unlagged[ent - g_entities].trailHead;

	// increment the head
	level.unlagged[ent - g_entities].trailHead++;
	if (level.unlagged[ent - g_entities].trailHead >= MAX_UNLAGGED_TRAILS) {
		level.unlagged[ent - g_entities].trailHead = 0;
	}
	head = level.unlagged[ent - g_entities].trailHead;

	// store all the collision-detection info and the time
	VectorCopy(ent->r.mins, level.unlagged[ent - g_entities].trail[head].mins);
	VectorCopy(ent->r.maxs, level.unlagged[ent - g_entities].trail[head].maxs);
	VectorCopy(ent->r.currentOrigin, level.unlagged[ent - g_entities].trail[head].currentOrigin);
	//VectorCopy( ent->r.currentAngles, level.unlagged[ent - g_entities].trail[head].currentAngles );

	level.unlagged[ent - g_entities].trail[head].torsoAnim = ent->client->ps.torsoAnim;
	level.unlagged[ent - g_entities].trail[head].torsoTimer = ent->client->ps.torsoTimer;
	level.unlagged[ent - g_entities].trail[head].legsAnim = ent->client->ps.legsAnim;
	level.unlagged[ent - g_entities].trail[head].legsTimer = ent->client->ps.legsTimer;
	level.unlagged[ent - g_entities].trail[head].realAngle = ent->s.apos.trBase[YAW];

	//if (level.unlagged[ent - g_entities].trail[head].currentAngles[0] || level.unlagged[ent - g_entities].trail[head].currentAngles[1] || level.unlagged[ent - g_entities].trail[head].currentAngles[2])
		//Com_Printf("Current angles are %.2f %.2f %.2f\n", level.unlagged[ent - g_entities].trail[head].currentAngles[0], level.unlagged[ent - g_entities].trail[head].currentAngles[1], level.unlagged[ent - g_entities].trail[head].currentAngles[2]);

	level.unlagged[ent - g_entities].trail[head].time = level.lastThinkRealTime[ent - g_entities];

	//Also store their anim info? Since with ghoul2 collision that matters..

	// FOR TESTING ONLY
	//Com_Printf("level.previousTime: %d, level.time: %d, newtime: %d\n", level.previousTime, level.time, newtime);
}


/*
=============
TimeShiftLerp

Used below to interpolate between two previous vectors
Returns a vector "frac" times the distance between "start" and "end"
=============
*/
void TimeShiftLerp(float frac, vec3_t start, vec3_t end, vec3_t result) {
	float	comp = 1.0f - frac;

	result[0] = frac * start[0] + comp * end[0];
	result[1] = frac * start[1] + comp * end[1];
	result[2] = frac * start[2] + comp * end[2];
}

static void TimeShiftAnimLerp(float frac, int anim1, int anim2, int time1, int time2, int *outTime) {
	if (anim1 == anim2 && time2 > time1) {//Only lerp if both anims are same and time2 is after time1.
		*outTime = time1 + (time2 - time1) * frac;
	}
	else
		*outTime = time2;

	//Com_Printf("Timeshift anim lerping: time1 is %i, time 2 is %i, lerped is %i\n", time1, time2, outTime);
}

extern void G_TestLine(vec3_t start, vec3_t end, int color, int time);
static void G_DrawPlayerStick(gentity_t *ent, int color, int duration, int time) {
	//Lets draw a visual of the unlagged hitbox difference?
	//Not sure how to get bounding boxes of g2 parts, so maybe just two stick figures? (lagged stick figure = red, unlagged stick figure = green) and make them both appear for like 5 seconds?
	vec3_t headPos, torsoPos, rHandPos, lHandPos, rArmPos, lArmPos, rKneePos, lKneePos, rFootPos, lFootPos, G2Angles;
	mdxaBone_t	boltMatrix;
	int handLBolt, handRBolt, armRBolt, armLBolt, kneeLBolt, kneeRBolt, footLBolt, footRBolt;

	if (!ent->client)
		return;
	if (ent->localAnimIndex > 1) //Not humanoid
		return;

	//Com_Printf("Drawing player stick model for %s\n", ent->client->pers.netname);

	handLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_hand");
	handRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_hand");
	armLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_arm_elbow");
	armRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_arm_elbow");
	kneeLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*hips_l_knee");
	kneeRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*hips_r_knee");
	footLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
	footRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");  //Shouldnt these always be the same numbers? can just make them constants?.. or pm->g2Bolts_RFoot etc?

	VectorSet(G2Angles, 0, ent->s.apos.trBase[YAW], 0); //Ok so r.currentAngles isnt even used for players ??
	VectorCopy(ent->r.currentOrigin, torsoPos);

	VectorCopy(torsoPos, headPos);
	headPos[2] += ent->r.maxs[2]; //E?
	torsoPos[2] += 8;//idk man
	G_TestLine(headPos, torsoPos, color, duration); //Head -> Torso

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, armRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale); //ent->cmd.serverTime ?.. why does this need time?
	rArmPos[0] = boltMatrix.matrix[0][3];
	rArmPos[1] = boltMatrix.matrix[1][3];
	rArmPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, rArmPos, color, duration); //Torso -> R Arm

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, armLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lArmPos[0] = boltMatrix.matrix[0][3];
	lArmPos[1] = boltMatrix.matrix[1][3];
	lArmPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, lArmPos, color, duration); //Torso -> L Arm

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, handRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	rHandPos[0] = boltMatrix.matrix[0][3];
	rHandPos[1] = boltMatrix.matrix[1][3];
	rHandPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(rArmPos, rHandPos, color, duration); //R Arm  -> R Hand

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, handLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lHandPos[0] = boltMatrix.matrix[0][3];
	lHandPos[1] = boltMatrix.matrix[1][3];
	lHandPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(lArmPos, lHandPos, color, duration); //L Arm -> L Hand

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, kneeRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	rKneePos[0] = boltMatrix.matrix[0][3];
	rKneePos[1] = boltMatrix.matrix[1][3];
	rKneePos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, rKneePos, color, duration); //Torso -> R Knee

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, kneeLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lKneePos[0] = boltMatrix.matrix[0][3];
	lKneePos[1] = boltMatrix.matrix[1][3];
	lKneePos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, lKneePos, color, duration); //Torso -> L Knee

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, footRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	rFootPos[0] = boltMatrix.matrix[0][3];
	rFootPos[1] = boltMatrix.matrix[1][3];
	rFootPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(rKneePos, rFootPos, color, duration); //R Knee -> R Foot

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, footLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lFootPos[0] = boltMatrix.matrix[0][3];
	lFootPos[1] = boltMatrix.matrix[1][3];
	lFootPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(lKneePos, lFootPos, color, duration); //L Knee -> L Foot
}


/*
=================
G_TimeShiftClient

Move a client back to where he was at the specified "time"
=================
*/
void G_TimeShiftClient(gentity_t *ent, int time, qboolean timeshiftAnims) {
	if (ent - g_entities >= MAX_CLIENTS)
		return;

	int		j, k;
	int now = trap_Milliseconds();
	if (time > now) {
		time = now;
	}

#ifdef _DEBUG
	if (g_unlaggedDebug.integer)
		PrintIngame(-1, "G_TimeShiftClient: ent %d, trap_milliseconds is %d, time is %d, trailhead time is %d\n", ent - g_entities, now, time, level.unlagged[ent - g_entities].trail[level.unlagged[ent - g_entities].trailHead].time);
#endif

	// find two entries in the origin trail whose times sandwich "time"
	// assumes no two adjacent trail records have the same timestamp
	j = k = level.unlagged[ent - g_entities].trailHead;
	do {
		if (level.unlagged[ent - g_entities].trail[j].time <= time)
			break;

		k = j;
		j--;
		if (j < 0) {
			j = MAX_UNLAGGED_TRAILS - 1;
		}
	} while (j != level.unlagged[ent - g_entities].trailHead);

	// if we got past the first iteration above, we've sandwiched (or wrapped)
	if (j != k) {
		// make sure it doesn't get re-saved
		if (level.unlagged[ent - g_entities].saved.time != now) {
			// save the current origin and bounding box
			VectorCopy(ent->r.mins, level.unlagged[ent - g_entities].saved.mins);
			VectorCopy(ent->r.maxs, level.unlagged[ent - g_entities].saved.maxs);
			VectorCopy(ent->r.currentOrigin, level.unlagged[ent - g_entities].saved.currentOrigin);
			//VectorCopy( ent->r.currentAngles, level.unlagged[ent - g_entities].saved.currentAngles );

			if (timeshiftAnims) {
				level.unlagged[ent - g_entities].saved.torsoAnim = ent->client->ps.torsoAnim;
				level.unlagged[ent - g_entities].saved.torsoTimer = ent->client->ps.torsoTimer;
				level.unlagged[ent - g_entities].saved.legsAnim = ent->client->ps.legsAnim;
				level.unlagged[ent - g_entities].saved.legsTimer = ent->client->ps.legsTimer;
				level.unlagged[ent - g_entities].saved.realAngle = ent->s.apos.trBase[YAW];
			}

			level.unlagged[ent - g_entities].saved.time = now;
		}

#ifdef _DEBUG
		if (g_unlaggedSkeletonTime.integer) {
			G_DrawPlayerStick(ent, 0x0000ff, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
			//Com_Printf("pre angle is %.2f\n", ent->s.apos.trBase[YAW]);
		}
#endif

		// if we haven't wrapped back to the head, we've sandwiched, so
		// we shift the client's position back to where he was at "time"
		if (j != level.unlagged[ent - g_entities].trailHead)
		{
			float	frac = (float)(level.unlagged[ent - g_entities].trail[k].time - time) / (float)(level.unlagged[ent - g_entities].trail[k].time - level.unlagged[ent - g_entities].trail[j].time);

			// FOR TESTING ONLY
#ifdef _DEBUG
			if (g_unlaggedDebug.integer)
				PrintIngame(-1, "time: %d, fire time: %d, j time: %d, k time: %d, diff: %d\n", now, time, level.unlagged[ent - g_entities].trail[j].time, level.unlagged[ent - g_entities].trail[k].time, level.unlagged[ent - g_entities].trail[k].time - level.unlagged[ent - g_entities].trail[j].time);
#endif

			// interpolate between the two origins to give position at time index "time"
			TimeShiftLerp(frac, level.unlagged[ent - g_entities].trail[k].currentOrigin, level.unlagged[ent - g_entities].trail[j].currentOrigin, ent->r.currentOrigin);
			//ent->r.currentAngles[YAW] = LerpAngle( level.unlagged[ent - g_entities].trail[k].currentAngles[YAW], ent->r.currentAngles[YAW], frac );

			// lerp these too, just for fun (and ducking)
			TimeShiftLerp(frac, level.unlagged[ent - g_entities].trail[k].mins, level.unlagged[ent - g_entities].trail[j].mins, ent->r.mins);
			TimeShiftLerp(frac, level.unlagged[ent - g_entities].trail[k].maxs, level.unlagged[ent - g_entities].trail[j].maxs, ent->r.maxs);

			//Lerp this somehow?
			if (timeshiftAnims) {
				/*
				Com_Printf("Lerp timeshifting client %s. Old anim = %i %i (times %i %i).  New Anim = %i %i (times %i %i).\n",
					ent->client->pers.netname,
					ent->client->ps.torsoAnim, ent->client->ps.legsAnim, ent->client->ps.torsoTimer, ent->client->ps.legsTimer,
					level.unlagged[ent - g_entities].trail[k].torsoAnim, level.unlagged[ent - g_entities].trail[k].legsAnim, level.unlagged[ent - g_entities].trail[k].torsoTimer, level.unlagged[ent - g_entities].trail[k].legsTimer);
				*/

				ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].trail[k].torsoAnim;
				ent->client->ps.legsAnim = level.unlagged[ent - g_entities].trail[k].legsAnim;
				TimeShiftAnimLerp(frac, level.unlagged[ent - g_entities].trail[j].torsoAnim, level.unlagged[ent - g_entities].trail[k].torsoAnim, level.unlagged[ent - g_entities].trail[j].torsoTimer, level.unlagged[ent - g_entities].trail[k].torsoTimer, &ent->client->ps.torsoTimer);
				TimeShiftAnimLerp(frac, level.unlagged[ent - g_entities].trail[j].legsAnim, level.unlagged[ent - g_entities].trail[k].legsAnim, level.unlagged[ent - g_entities].trail[j].legsTimer, level.unlagged[ent - g_entities].trail[k].legsTimer, &ent->client->ps.legsTimer);
				//ent->s.apos.trBase[YAW] = LerpAngle( level.unlagged[ent - g_entities].trail[k].realAngle, ent->s.apos.trBase[YAW], frac ); //Shouldnt this be lerping between k and j instead of k and trbase ?
				ent->s.apos.trBase[YAW] = LerpAngle(level.unlagged[ent - g_entities].trail[k].realAngle, level.unlagged[ent - g_entities].trail[j].realAngle, frac); //Shouldnt this be lerping between k and j instead of k and trbase ?

				//Com_Printf("j angle is %.2f k angle is %.2f frac is %.2f\n", level.unlagged[ent - g_entities].trail[j].realAngle, level.unlagged[ent - g_entities].trail[k].realAngle, frac);
				//Com_Printf("interp angle is %.2f\n", LerpAngle( level.unlagged[ent - g_entities].trail[j].realAngle, level.unlagged[ent - g_entities].trail[k].realAngle, frac ));
			}

			// this will recalculate absmin and absmax
			trap_LinkEntity(ent);
		}
		else {
			// we wrapped, so grab the earliest
			//VectorCopy( level.unlagged[ent - g_entities].trail[k].currentAngles, ent->r.currentAngles );
			VectorCopy(level.unlagged[ent - g_entities].trail[k].currentOrigin, ent->r.currentOrigin);
			VectorCopy(level.unlagged[ent - g_entities].trail[k].mins, ent->r.mins);
			VectorCopy(level.unlagged[ent - g_entities].trail[k].maxs, ent->r.maxs);

			if (timeshiftAnims) {
				/*
				Com_Printf("Timeshifting client %s. Old anim = %i %i (times %i %i).  New Anim = %i %i (times %i %i).\n",
					ent->client->pers.netname,
					ent->client->ps.torsoAnim, ent->client->ps.legsAnim, ent->client->ps.torsoTimer, ent->client->ps.legsTimer,
					level.unlagged[ent - g_entities].trail[k].torsoAnim, level.unlagged[ent - g_entities].trail[k].legsAnim, level.unlagged[ent - g_entities].trail[k].torsoTimer, level.unlagged[ent - g_entities].trail[k].legsTimer);
				*/

				ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].trail[k].torsoAnim;
				ent->client->ps.torsoTimer = level.unlagged[ent - g_entities].trail[k].torsoTimer;
				ent->client->ps.legsAnim = level.unlagged[ent - g_entities].trail[k].legsAnim;
				ent->client->ps.legsTimer = level.unlagged[ent - g_entities].trail[k].legsTimer;
				ent->s.apos.trBase[YAW] = level.unlagged[ent - g_entities].trail[k].realAngle;
			}

			// this will recalculate absmin and absmax
			trap_LinkEntity(ent);
		}

#ifdef _DEBUG
		if (g_unlaggedSkeletonTime.integer) {
			G_DrawPlayerStick(ent, 0x00ff00, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
			//Com_Printf("post angle is %.2f\n", ent->s.apos.trBase[YAW]);
		}
#endif


	}
}

/*
=====================
G_TimeShiftAllClients

Move ALL clients back to where they were at the specified "time",
except for "skip"
=====================
*/
#define UNLAGGED_FACTOR				(0.25)
#define UNLAGGED_MAX_COMPENSATION	(500)
void G_TimeShiftAllClients(int time, gentity_t *skip, qboolean timeshiftAnims) {
	int			i;
	gentity_t *ent;

	if (!skip->client)
		return;
	if (skip->r.svFlags & SVF_BOT)
		return;
	if (skip->s.eType == ET_NPC)
		return;

#ifdef _DEBUG
	if (g_unlaggedDebug.integer)
		PrintIngame(-1, "G_TimeShiftAllClients: time param %d, factor %f, offset %d, final time ", time, g_unlaggedFactor.value, g_unlaggedOffset.integer);
#endif

	int now = trap_Milliseconds();
	if (time > now) {
		time = now;
	}

#ifdef _DEBUG
	if (g_unlaggedFactor.value) {
		float diff = now - time;
		time += (int)round((double)g_unlaggedFactor.value * (double)diff);
	}
#else
	float diff = now - time;
	time += (int)round(UNLAGGED_FACTOR * (double)diff);
#endif

#ifdef _DEBUG
	if (g_unlaggedOffset.integer)
		time += g_unlaggedOffset.integer;
#endif

	if (time > now) {
		time = now;
	}

#ifdef _DEBUG
	if (now - time > g_unlaggedMaxCompensation.integer) {
		time = now - g_unlaggedMaxCompensation.integer;
	}
#else
	if (now - time > UNLAGGED_MAX_COMPENSATION) {
		time = now - UNLAGGED_MAX_COMPENSATION;
	}
#endif

#ifdef _DEBUG
	if (g_unlaggedDebug.integer)
		PrintIngame(-1, "%d, now %d, diff %d\n", time, now, now - time);
#endif

	// for every client
	ent = &g_entities[0];
	for (i = 0; i < MAX_CLIENTS; i++, ent++) {
		if (ent->client && ent->inuse && ent->client->sess.sessionTeam < TEAM_SPECTATOR && ent != skip) {
			G_TimeShiftClient(ent, time, timeshiftAnims);
		}
	}
}


/*
===================
G_UnTimeShiftClient

Move a client back to where he was before the time shift
===================
*/
void G_UnTimeShiftClient(gentity_t *ent, qboolean timeshiftAnims) {
	// if it was saved
	if (level.unlagged[ent - g_entities].saved.time) {
		// move it back
		VectorCopy(level.unlagged[ent - g_entities].saved.mins, ent->r.mins);
		VectorCopy(level.unlagged[ent - g_entities].saved.maxs, ent->r.maxs);
		VectorCopy(level.unlagged[ent - g_entities].saved.currentOrigin, ent->r.currentOrigin);
		//VectorCopy( level.unlagged[ent - g_entities].saved.currentAngles, ent->r.currentAngles );

		if (timeshiftAnims) {
			ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].saved.torsoAnim;
			ent->client->ps.torsoTimer = level.unlagged[ent - g_entities].saved.torsoTimer;
			ent->client->ps.legsAnim = level.unlagged[ent - g_entities].saved.legsAnim;
			ent->client->ps.legsTimer = level.unlagged[ent - g_entities].saved.legsTimer;
			ent->s.apos.trBase[YAW] = level.unlagged[ent - g_entities].saved.realAngle;
		}

		level.unlagged[ent - g_entities].saved.time = 0;

		// this will recalculate absmin and absmax
		trap_LinkEntity(ent);
	}
}

/*
=======================
G_UnTimeShiftAllClients

Move ALL the clients back to where they were before the time shift,
except for "skip"
=======================
*/
void G_UnTimeShiftAllClients(gentity_t *skip, qboolean timeshiftAnims) {
	int			i;
	gentity_t *ent;

	if (!skip->client)
		return;
	if (skip->r.svFlags & SVF_BOT)
		return;
	if (skip->s.eType == ET_NPC)
		return;

	ent = &g_entities[0];
	for (i = 0; i < MAX_CLIENTS; i++, ent++) {
		if (ent->client && ent->inuse && ent->client->sess.sessionTeam < TEAM_SPECTATOR && ent != skip) {
			G_UnTimeShiftClient(ent, timeshiftAnims);
		}
	}
}

/*
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player_state_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback( gentity_t *player ) {
	gclient_t	*client;
	float	count;
	vec3_t	angles;

	client = player->client;
	if ( client->ps.pm_type == PM_DEAD ) {
		return;
	}

	// total points of damage shot at the player this frame
	count = client->damage_blood + client->damage_armor;
	if ( count == 0 ) {
		return;		// didn't take any damage
	}

	if ( count > 255 ) {
		count = 255;
	}

	// send the information to the client

	// world damage (falling, slime, etc) uses a special code
	// to make the blend blob centered instead of positional
	if ( client->damage_fromWorld ) {
		client->ps.damagePitch = 255;
		client->ps.damageYaw = 255;

		client->damage_fromWorld = qfalse;
	} else {
		vectoangles( client->damage_from, angles );
		client->ps.damagePitch = angles[PITCH]/360.0 * 256;
		client->ps.damageYaw = angles[YAW]/360.0 * 256;

		//cap them since we can't send negative values in here across the net
		if (client->ps.damagePitch < 0)
		{
			client->ps.damagePitch = 0;
		}
		if (client->ps.damageYaw < 0)
		{
			client->ps.damageYaw = 0;
		}
	}

	// play an apropriate pain sound
	if ( (level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && !(player->s.eFlags & EF_DEAD) ) {

		// don't do more than two pain sounds a second
		// nmckenzie: also don't make him loud and whiny if he's only getting nicked.
		if ( level.time - client->ps.painTime < 500 || count < 10) {
			return;
		}
		P_SetTwitchInfo(client);
		player->pain_debounce_time = level.time + 700;

		int percent;
		if (g_gametype.integer == GT_SIEGE && client->siegeClass != -1) { // get accurate health percentages for siege classes whose starting/max HP isn't 100
			siegeClass_t *scl = &bgSiegeClasses[client->siegeClass];
			if (scl && scl->maxhealth > 0)
				percent = (int)(100.0f * ((double)player->health / (double)scl->maxhealth));
		}
		else {
			percent = player->health;
		}
		// disguise HP amounts before sending them to clients
		if (percent < 25) //0-24
			percent = 24;
		else if (percent < 50) //25-49
			percent = 49;
		else if (percent < 75) //50-74
			percent = 74;
		else //75 +
			percent = 100;
		G_AddEvent(player, EV_PAIN, percent);

		client->ps.damageEvent++;

		if (client->damage_armor && !client->damage_blood)
		{
			client->ps.damageType = 1; //pure shields
		}
		else if (client->damage_armor)
		{
			client->ps.damageType = 2; //shields and health
		}
		else
		{
			client->ps.damageType = 0; //pure health
		}
	}


	client->ps.damageCount = count;

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_knockback = 0;
}



/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects( gentity_t *ent ) {
	qboolean	envirosuit;
	int			waterlevel;

	if ( ent->client->noclip ) {
		ent->client->airOutTime = level.time + 12000;	// don't need air
		return;
	}

	waterlevel = ent->waterlevel;

	envirosuit = ent->client->ps.powerups[PW_BATTLESUIT] > level.time;

	//
	// check for drowning
	//
	if ( waterlevel == 3 ) {
		// envirosuit give air
		if ( envirosuit ) {
			ent->client->airOutTime = level.time + 10000;
		}

		// if out of air, start drowning
		if ( ent->client->airOutTime < level.time) {
			// drown!
			ent->client->airOutTime += 1000;
			if ( ent->health > 0 ) {
				// take more damage the longer underwater
				ent->damage += 2;
				if (ent->damage > 15)
					ent->damage = 15;

				// play a gurp sound instead of a normal pain sound
				if (ent->health <= ent->damage) {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex(/*"*drown.wav"*/"sound/player/gurp1.wav"));
				} else if (rand()&1) {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex("sound/player/gurp1.wav"));
				} else {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex("sound/player/gurp2.wav"));
				}

				// don't play a normal pain sound
				ent->pain_debounce_time = level.time + 200;

				G_Damage (ent, NULL, NULL, NULL, NULL, 
					ent->damage, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	} else {
		ent->client->airOutTime = level.time + 12000;
		ent->damage = 2;
	}

	//
	// check for sizzle damage (move to pmove?)
	//
	if (waterlevel && 
		(ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		if (ent->health > 0
			&& ent->pain_debounce_time <= level.time	) {

			if ( envirosuit ) {
				G_AddEvent( ent, EV_POWERUP_BATTLESUIT, 0 );
			} else {
				if (ent->watertype & CONTENTS_LAVA) {
					G_Damage (ent, NULL, NULL, NULL, NULL, 
						30*waterlevel, 0, MOD_LAVA);
				}

				if (ent->watertype & CONTENTS_SLIME) {
					G_Damage (ent, NULL, NULL, NULL, NULL, 
						10*waterlevel, 0, MOD_SLIME);
				}
			}
		}
	}
}





//==============================================================
extern void G_ApplyKnockback( gentity_t *targ, vec3_t newDir, float knockback );
void DoImpact( gentity_t *self, gentity_t *other, qboolean damageSelf )
{
	float magnitude, my_mass;
	vec3_t	velocity;
	int cont;
	qboolean easyBreakBrush = qtrue;

	if( self->client )
	{
		VectorCopy( self->client->ps.velocity, velocity );
		if( !self->mass )
		{
			my_mass = 10;
		}
		else
		{
			my_mass = self->mass;
		}
	}
	else 
	{
		VectorCopy( self->s.pos.trDelta, velocity );
		if ( self->s.pos.trType == TR_GRAVITY )
		{
			velocity[2] -= 0.25f * g_gravity.value;
		}
		if( !self->mass )
		{
			my_mass = 1;
		}
		else if ( self->mass <= 10 )
		{
			my_mass = 10;
		}
		else
		{
			my_mass = self->mass;
		}
	}

	magnitude = VectorLength( velocity ) * my_mass / 10;

	if ( !other->noTouchBreak && (other->material == MAT_GLASS 
		|| other->material == MAT_GLASS_METAL 
		|| other->material == MAT_GRATE1
		|| ((other->flags&FL_BBRUSH)&&(other->spawnflags&8))
		|| (other->r.svFlags&SVF_GLASS_BRUSH) ))
	{
		easyBreakBrush = qtrue;
	}

	qboolean definitelyBreak;
	if (easyBreakBrush && g_gametype.integer == GT_SIEGE && level.siegeMap == SIEGEMAP_CARGO && self - g_entities != CARGO_FANGRATING_NUM) {
		definitelyBreak = qtrue;
	}
	else {
		definitelyBreak = qfalse;
	}

	if ( !self->client || self->client->ps.lastOnGround+300<level.time || ( self->client->ps.lastOnGround+100 < level.time && easyBreakBrush ) || definitelyBreak )
	{
		vec3_t dir1, dir2;
		float force = 0, dot;

		if ( easyBreakBrush )
			magnitude *= 2;

		//damage them
		if ( magnitude >= 100 && other->s.number < ENTITYNUM_WORLD )
		{
			VectorCopy( velocity, dir1 );
			VectorNormalize( dir1 );
			if( VectorCompare( other->r.currentOrigin, vec3_origin ) )
			{//a brush with no origin
				VectorCopy ( dir1, dir2 );
			}
			else
			{
				VectorSubtract( other->r.currentOrigin, self->r.currentOrigin, dir2 );
				VectorNormalize( dir2 );
			}

			dot = DotProduct( dir1, dir2 );

			if ( dot >= 0.2 )
			{
				force = dot;
			}
			else
			{
				force = 0;
			}

			force *= (magnitude/50);

			cont = trap_PointContents( other->r.absmax, other->s.number );
			if( (cont&CONTENTS_WATER) )//|| (self.classname=="barrel"&&self.aflag))//FIXME: or other watertypes
			{
				force /= 3;							//water absorbs 2/3 velocity
			}

			if( ( force >= 1 && other->s.number >= MAX_CLIENTS ) || force >= 10)
			{

				if ( other->r.svFlags & SVF_GLASS_BRUSH )
				{
					other->splashRadius = (float)(self->r.maxs[0] - self->r.mins[0])/4.0f;
				}
				if ( other->takedamage )
				{
					G_Damage( other, self, self, velocity, self->r.currentOrigin, force, DAMAGE_NO_ARMOR, MOD_CRUSH);//FIXME: MOD_IMPACT
				}
				else
				{
					G_ApplyKnockback( other, dir2, force );
				}
			}
		}

		if ( damageSelf && self->takedamage )
		{
			//Now damage me
			//FIXME: more lenient falling damage, especially for when driving a vehicle
			if ( self->client && self->client->ps.fd.forceJumpZStart )
			{//we were force-jumping
				if ( self->r.currentOrigin[2] >= self->client->ps.fd.forceJumpZStart )
				{//we landed at same height or higher than we landed
					magnitude = 0;
				}
				else
				{//FIXME: take off some of it, at least?
					magnitude = (self->client->ps.fd.forceJumpZStart-self->r.currentOrigin[2])/3;
				}
			}
			//if(self.classname!="monster_mezzoman"&&self.netname!="spider")//Cats always land on their feet
				if( ( magnitude >= 100 + self->health && self->s.number >= MAX_CLIENTS && self->s.weapon != WP_SABER ) || ( magnitude >= 700 ) )//&& self.safe_time < level.time ))//health here is used to simulate structural integrity
				{
					if ( (self->s.weapon == WP_SABER) && self->client && self->client->ps.groundEntityNum < ENTITYNUM_NONE && magnitude < 1000 )
					{//players and jedi take less impact damage
						//allow for some lenience on high falls
						magnitude /= 2;
						}
					magnitude /= 40;
					magnitude = magnitude - force/2;//If damage other, subtract half of that damage off of own injury
					if ( magnitude >= 1 )
					{
						G_Damage( self, NULL, NULL, NULL, self->r.currentOrigin, magnitude/2, DAMAGE_NO_ARMOR, MOD_FALLING );//FIXME: MOD_IMPACT
					}
				}
		}
	}
}

void Client_CheckImpactBBrush( gentity_t *self, gentity_t *other )
{
	if ( !other || !other->inuse )
	{
		return;
	}
	if (!self || !self->inuse || !self->client ||
		self->client->tempSpectate >= level.time ||
		self->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //hmm.. let's not let spectators ram into breakables.
		return;
	}

	if ( !other->noTouchBreak && (other->material == MAT_GLASS 
		|| other->material == MAT_GLASS_METAL 
		|| other->material == MAT_GRATE1
		|| ((other->flags&FL_BBRUSH)&&(other->spawnflags&8/*THIN*/))
		|| ((other->flags&FL_BBRUSH)&&(other->health<=10))
		|| (other->r.svFlags&SVF_GLASS_BRUSH))
		&& !(self->client->ps.pm_flags&PMF_STUCK_TO_WALL)) //breaking glass bug
	{//clients only do impact damage against easy-break breakables
		DoImpact( self, other, qfalse );
	}
}


/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( gentity_t *ent ) {
	if (ent->client && ent->client->isHacking)
	{ //loop hacking sound
		ent->client->ps.loopSound = level.snd_hack;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->client && ent->client->isMedHealed > level.time)
	{ //loop healing sound
		ent->client->ps.loopSound = level.snd_medHealed;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->client && ent->client->isMedSupplied > level.time)
	{ //loop supplying sound
		ent->client->ps.loopSound = level.snd_medSupplied;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->waterlevel && (ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		ent->client->ps.loopSound = level.snd_fry;
		ent->s.loopIsSoundset = qfalse;
	} else {
		ent->client->ps.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
	}
}



//==============================================================

/*
==============
ClientImpacts
==============
*/
void ClientImpacts( gentity_t *ent, pmove_t *pm ) {
	int		i, j;
	trace_t	trace;
	gentity_t	*other;

	memset( &trace, 0, sizeof( trace ) );
	for (i=0 ; i<pm->numtouch ; i++) {
		for (j=0 ; j<i ; j++) {
			if (pm->touchents[j] == pm->touchents[i] ) {
				break;
			}
		}
		if (j != i) {
			continue;	// duplicated
		}
		other = &g_entities[ pm->touchents[i] ];

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, other, &trace );
		}

		if ( !other->touch ) {
			continue;
		}

		other->touch( other, ent, &trace );
	}

}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void	G_TouchTriggers( gentity_t *ent ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs;
	static vec3_t	range = { 40, 40, 52 };

	if ( !ent->client ) {
		return;
	}

	// dead clients don't activate triggers!
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	VectorSubtract( ent->client->ps.origin, range, mins );
	VectorAdd( ent->client->ps.origin, range, maxs );

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	// can't use ent->r.absmin, because that has a one unit pad
	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

	for ( i=0 ; i<num ; i++ ) {
		hit = &g_entities[touch[i]];

		if ( !hit->touch && !ent->touch ) {
			continue;
		}
		if ( !( hit->r.contents & CONTENTS_TRIGGER )
            && (hit->s.eType != ET_SPECIAL) )
        {
			continue;
		}

		// ignore most entities if a spectator
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			if ( hit->s.eType != ET_TELEPORT_TRIGGER &&
				// this is ugly but adding a new ET_? type will
				// most likely cause network incompatibilities
				hit->touch != Touch_DoorTrigger) {
				continue;
			}
		}

		// use seperate code for determining if an item is picked up
		// so you don't have to actually contact its bounding box
		if ( hit->s.eType == ET_ITEM ) {
			if ( !BG_PlayerTouchesItem( &ent->client->ps, &hit->s, level.time ) ) {
				continue;
			}
		} else {
			if ( !trap_EntityContact( mins, maxs, hit ) ) {
				continue;
			}
		}

		memset( &trace, 0, sizeof(trace) );

		if ( hit->touch ) {
			hit->touch (hit, ent, &trace);
		}

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, hit, &trace );
		}
	}

	// if we didn't touch a jump pad this pmove frame
	if ( ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount ) {
		ent->client->ps.jumppad_frame = 0;
		ent->client->ps.jumppad_ent = 0;
	}
}


/*
============
G_MoverTouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void G_MoverTouchPushTriggers( gentity_t *ent, vec3_t oldOrg ) 
{
	int			i, num;
	float		step, stepSize, dist;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs, dir, size, checkSpot;
	const vec3_t	range = { 40, 40, 52 };

	// non-moving movers don't hit triggers!
	if ( !VectorLengthSquared( ent->s.pos.trDelta ) ) 
	{
		return;
	}

	VectorSubtract( ent->r.mins, ent->r.maxs, size );
	stepSize = VectorLength( size );
	if ( stepSize < 1 )
	{
		stepSize = 1;
	}

	VectorSubtract( ent->r.currentOrigin, oldOrg, dir );
	dist = VectorNormalize( dir );
	for ( step = 0; step <= dist; step += stepSize )
	{
		VectorMA( ent->r.currentOrigin, step, dir, checkSpot );
		VectorSubtract( checkSpot, range, mins );
		VectorAdd( checkSpot, range, maxs );

		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

		// can't use ent->r.absmin, because that has a one unit pad
		VectorAdd( checkSpot, ent->r.mins, mins );
		VectorAdd( checkSpot, ent->r.maxs, maxs );

		for ( i=0 ; i<num ; i++ ) 
		{
			hit = &g_entities[touch[i]];

			if ( hit->s.eType != ET_PUSH_TRIGGER )
			{
				continue;
			}

			if ( hit->touch == NULL ) 
			{
				continue;
			}

			if ( !( hit->r.contents & CONTENTS_TRIGGER ) ) 
			{
				continue;
			}


			if ( !trap_EntityContact( mins, maxs, hit ) ) 
			{
				continue;
			}

			memset( &trace, 0, sizeof(trace) );

			if ( hit->touch != NULL ) 
			{
				hit->touch(hit, ent, &trace);
			}
		}
	}
}

/*
=================
SpectatorThink
=================
*/
void SpectatorThink( gentity_t *ent, usercmd_t *ucmd ) {
	pmove_t	pm;
	gclient_t	*client;

	client = ent->client;

	if ( client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		client->ps.pm_type = PM_SPECTATOR;

		//hmm, shouldn't have an anim if you're a spectator, make sure
		//it gets cleared.
		client->ps.legsAnim = 0;
		client->ps.legsTimer = 0;
		client->ps.torsoAnim = 0;
		client->ps.torsoTimer = 0;

		// set up for pmove
		memset (&pm, 0, sizeof(pm));
		pm.ps = &client->ps;
		pm.cmd = *ucmd;
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;	// spectators can fly through bodies
		pm.trace = trap_Trace;
		pm.pointcontents = trap_PointContents;

		if (g_siegeGhosting.integer && g_gametype.integer == GT_SIEGE && (client->sess.sessionTeam == TEAM_RED || client->sess.sessionTeam == TEAM_BLUE)) {
			pm.noSpecMove = 1;
			client->ps.speed = 0;
			client->ps.basespeed = 0;
		}
		else {
			pm.noSpecMove = g_noSpecMove.integer;
			client->ps.speed = 400;	// faster than normal
			client->ps.basespeed = 400;
		}

		pm.animations = NULL;
		pm.nonHumanoid = qfalse;

		//Set up bg entity data
		pm.baseEnt = (bgEntity_t *)g_entities;
		pm.entSize = sizeof(gentity_t);

		// perform a pmove
		Pmove (&pm);

		// save results of pmove
		VectorCopy( client->ps.origin, ent->s.origin );

		if (ent->client->tempSpectate < level.time)
		{
			G_TouchTriggers( ent );
		}

		trap_UnlinkEntity( ent );
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;

	if (client->tempSpectate < level.time)
	{
		// attack button cycles through spectators
		if ( ( client->buttons & BUTTON_ATTACK ) && ! ( client->oldbuttons & BUTTON_ATTACK ) ) {
			Cmd_FollowCycle_f( ent, 1 );
		} else if ( client->sess.spectatorState == SPECTATOR_FOLLOW && 
			(client->buttons & BUTTON_ALT_ATTACK) && !(client->oldbuttons & BUTTON_ALT_ATTACK) ) {
			Cmd_FollowCycle_f( ent, -1 );
		} else if ( ucmd->generic_cmd == GENCMD_SABERATTACKCYCLE ) {
			// saberattackcycle cycles flag carriers
			Cmd_FollowFlag_f( ent );
		} else if ((client->buttons & BUTTON_USE) && !(client->oldbuttons & BUTTON_USE)) {
			Cmd_FollowTarget_f(ent);
		}

		if (client->sess.spectatorState == SPECTATOR_FOLLOW && (ucmd->upmove > 0))
		{ //jump now removes you from follow mode
			StopFollowing(ent);
		}
	}
}

// returns global time in ms, this should persist between server restarts
int getGlobalTime()
{
    return (int)time(0);
}

static qboolean IsInputting(const gclient_t *client, qboolean checkPressingButtons, qboolean checkMovingMouse, qboolean checkPressingChatButton) {
	if (!client) {
		assert(qfalse);
		return qfalse;
	}

	if (checkPressingButtons) {
		if (client->pers.cmd.forwardmove || client->pers.cmd.rightmove || client->pers.cmd.upmove ||
			client->pers.cmd.buttons & BUTTON_ATTACK || client->pers.cmd.buttons & BUTTON_ALT_ATTACK || client->pers.cmd.generic_cmd) {
			return qtrue;
		}
	}

	if (checkMovingMouse) {
		if ((short)client->pers.cmd.angles[0] != (short)client->pers.lastCmd.angles[0] ||
			(short)client->pers.cmd.angles[1] != (short)client->pers.lastCmd.angles[1] ||
			(short)client->pers.cmd.angles[2] != (short)client->pers.lastCmd.angles[2]) {
			return qtrue;
		}
	}

	if (checkPressingChatButton) {
		if ((client->pers.cmd.buttons ^ client->pers.lastCmd.buttons) & BUTTON_TALK) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
=================
ClientInactivityTimer

Returns qfalse if the spectator is dropped
=================
*/
qboolean CheckSpectatorInactivityTimer(gclient_t *client)
{
	if (IsInputting(client, qtrue, qtrue, qtrue))
		client->pers.lastInputTime = getGlobalTime();

    if (!g_spectatorInactivity.integer)
    {
        // give everyone some time, so if the operator sets g_inactivity during
        // gameplay, everyone isn't kicked

        client->sess.inactivityTime = getGlobalTime() + 60;
        client->inactivityWarning = qfalse;
    }
    else if (IsInputting(client, qtrue, qtrue, qtrue) || client->sess.spectatorState == SPECTATOR_FOLLOW)
    {
        client->sess.inactivityTime = getGlobalTime() + g_spectatorInactivity.integer;
        
        client->inactivityWarning = qfalse;
    }
    else if (!client->pers.localClient)
    {
        if (getGlobalTime() > client->sess.inactivityTime)
        {
            // kick them..
            trap_DropClient(client - level.clients, "dropped due to inactivity");
        }

        if ((getGlobalTime() > (client->sess.inactivityTime - 10)) && (!client->inactivityWarning))
        {
            client->inactivityWarning = qtrue;
            trap_SendServerCommand(client - level.clients, "cp \"Ten seconds until inactivity drop!\n\"");
        }
    }

    // remember current commands for next cycle diff
    client->pers.lastCmd = client->pers.cmd;

    return qtrue;
}

/*
=================
ClientInactivityTimer

Returns qfalse if the client is dropped/force specced
=================
*/
qboolean CheckPlayerInactivityTimer(gclient_t *client)
{
	if (IsInputting(client, qtrue, qtrue, qtrue))
		client->pers.lastInputTime = getGlobalTime();

	qboolean active = qtrue;

	qboolean didSomething;
	if (client->pers.cmd.forwardmove ||
		client->pers.cmd.rightmove ||
		client->pers.cmd.upmove) {
		didSomething = qtrue;
		if (!level.movedAtStart[client - level.clients] && (level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
			level.siegeRoundStartTime && level.time - level.siegeRoundStartTime < LIVEPUG_CHECK_TIME && client->sess.sessionTeam != TEAM_SPECTATOR) {
			level.movedAtStart[client - level.clients] = qtrue;
		}
	}
	else if (IsInputting(client, qtrue, qtrue, qtrue)) {
		didSomething = qtrue;
	}
	else {
		didSomething = qfalse;
	}

    if (!g_inactivity.integer)
    {
        // give everyone some time, so if the operator sets g_inactivity during
        // gameplay, everyone isn't kicked
        client->sess.inactivityTime = getGlobalTime() + 60;
        
        client->inactivityWarning = qfalse;
    } 
    else if (didSomething)
    {
        client->sess.inactivityTime = getGlobalTime() + g_inactivity.integer;
        client->inactivityWarning = qfalse;
    }
    else if (!client->pers.localClient)
    {
        if (getGlobalTime() > client->sess.inactivityTime)
        {
            if (g_inactivityKick.integer)
            {
                // kick them..
                trap_DropClient(client - level.clients, "dropped due to inactivity");
            }
            else
            {
                // just force them to spec
                trap_SendServerCommand(-1, va("print \"%s "S_COLOR_WHITE"forced to spectators due to inactivity\n\"", client->pers.netname));
                trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i spectator\n", client - level.clients));
                client->sess.inactivityTime = getGlobalTime() + 1;
            }

            active = qfalse;
        } 
        else if ((getGlobalTime() > (client->sess.inactivityTime - 10)) && (!client->inactivityWarning))
        {
            client->inactivityWarning = qtrue;
            trap_SendServerCommand(client - level.clients, "cp \"Ten seconds until inactivity drop!\n\"");
        }
    }

    // remember current commands for next cycle diff
    client->pers.lastCmd = client->pers.cmd;

    return active;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
void ClientTimerActions( gentity_t *ent, int msec ) {
	gclient_t	*client;

	client = ent->client;
	client->timeResidual += msec;

	while ( client->timeResidual >= 1000 ) 
	{
		client->timeResidual -= 1000;

		// count down health when over max
		if ( ent->health > client->ps.stats[STAT_MAX_HEALTH] && !client->sess.siegeDuelInProgress) {
			ent->health--;
		}

		// count down armor when over max
		int maxArmor;
		if (g_gametype.integer == GT_SIEGE && client->siegeClass != -1)
			maxArmor = bgSiegeClasses[client->siegeClass].maxarmor;
		else
			maxArmor = client->ps.stats[STAT_MAX_HEALTH];
		if ( client->ps.stats[STAT_ARMOR] > maxArmor ) {
			client->ps.stats[STAT_ARMOR]--;
		}
	}
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink( gclient_t *client ) {
	client->ps.eFlags &= ~EF_TALK;
	client->ps.eFlags &= ~EF_FIRING;

	// the level will exit when everyone wants to or after timeouts

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = client->pers.cmd.buttons;
	if ( client->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) & ( client->oldbuttons ^ client->buttons ) ) {
		// this used to be an ^1 but once a player says ready, it should stick
		client->readyToExit = 1;
	}
}

extern void NPC_SetAnim(gentity_t	*ent,int setAnimParts,int anim,int setAnimFlags);
void G_VehicleAttachDroidUnit( gentity_t *vehEnt )
{
	if ( vehEnt && vehEnt->m_pVehicle && vehEnt->m_pVehicle->m_pDroidUnit != NULL )
	{
		gentity_t *droidEnt = (gentity_t *)vehEnt->m_pVehicle->m_pDroidUnit;
		mdxaBone_t boltMatrix;
		vec3_t	fwd;

		trap_G2API_GetBoltMatrix(vehEnt->ghoul2, 0, vehEnt->m_pVehicle->m_iDroidUnitTag, &boltMatrix, vehEnt->r.currentAngles, vehEnt->r.currentOrigin, level.time,
			NULL, vehEnt->modelScale);
		BG_GiveMeVectorFromMatrix(&boltMatrix, ORIGIN, droidEnt->r.currentOrigin);
		BG_GiveMeVectorFromMatrix(&boltMatrix, NEGATIVE_Y, fwd);
		vectoangles( fwd, droidEnt->r.currentAngles );
		
		if ( droidEnt->client )
		{
			VectorCopy( droidEnt->r.currentAngles, droidEnt->client->ps.viewangles );
			VectorCopy( droidEnt->r.currentOrigin, droidEnt->client->ps.origin );
		}

		G_SetOrigin( droidEnt, droidEnt->r.currentOrigin );
		trap_LinkEntity( droidEnt );
		
		if ( droidEnt->NPC )
		{
			NPC_SetAnim( droidEnt, SETANIM_BOTH, BOTH_STAND2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
		}
	}
}

//called gameside only from pmove code (convenience)
void G_CheapWeaponFire(int entNum, int ev)
{
	gentity_t *ent = &g_entities[entNum];
	
	if (!ent->inuse || !ent->client)
	{
		return;
	}

	switch (ev)
	{
		case EV_FIRE_WEAPON:
			if (ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER &&
				ent->client && ent->client->ps.m_iVehicleNum)
			{ //a speeder with a pilot
				gentity_t *rider = &g_entities[ent->client->ps.m_iVehicleNum-1];
				if (rider->inuse && rider->client)
				{ //pilot is valid...
                    if (rider->client->ps.weapon != WP_MELEE &&
						(rider->client->ps.weapon != WP_SABER || !rider->client->ps.saberHolstered))
					{ //can only attack on speeder when using melee or when saber is holstered
						break;
					}
				}
			}

			FireWeapon( ent, qfalse );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;
		case EV_ALT_FIRE:
			FireWeapon( ent, qtrue );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;
	}
}

/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
#include "namespace_begin.h"
qboolean BG_InKnockDownOnly( int anim );
#include "namespace_end.h"

void ClientEvents( gentity_t *ent, int oldEventSequence ) {
	int		i;
	int		event;
	gclient_t *client;
	int		damage;

	client = ent->client;

	if ( oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS ) {
		oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
	}
	for ( i = oldEventSequence ; i < client->ps.eventSequence ; i++ ) {
		event = client->ps.events[ i & (MAX_PS_EVENTS-1) ];

		switch ( event ) {
		case EV_FALL:
		case EV_ROLL:
			{
				int delta = client->ps.eventParms[ i & (MAX_PS_EVENTS-1) ];
				qboolean knockDownage = qfalse;

				if (ent->client && ent->client->ps.fallingToDeath)
				{
					break;
				}

				if ( ent->s.eType != ET_PLAYER )
				{
					break;		// not in the player model
				}
				
				if ( g_dmflags.integer & DF_NO_FALLING )
				{
					break;
				}

				if (BG_InKnockDownOnly(ent->client->ps.legsAnim))
				{
					if (delta <= 14)
					{
						break;
					}
					knockDownage = qtrue;
				}
				else
				{
					if (delta <= 44)
					{
						break;
					}
				}

				if (knockDownage)
				{
					damage = delta*1; //you suffer for falling unprepared. A lot. Makes throws and things useful, and more realistic I suppose.
				}
				else
				{
					if (g_gametype.integer == GT_SIEGE &&
						delta > 60)
					{ //longer falls hurt more
						damage = delta*1; //good enough for now, I guess
					}
					else
					{
						damage = delta*0.16; //good enough for now, I guess
					}
				}

				//VectorSet (dir, 0, 0, 1);
				ent->pain_debounce_time = level.time + 200;	// no normal pain sound
				G_Damage (ent, NULL, NULL, NULL, NULL, damage, DAMAGE_NO_ARMOR, MOD_FALLING);

				if (ent->health < 1)
				{
					G_Sound(ent, CHAN_AUTO, G_SoundIndex( "sound/player/fallsplat.wav" ));
				}
			}
			break;
		case EV_FIRE_WEAPON:
			if (ent && ent->client && ent->client->emoted)
				break;
			FireWeapon( ent, qfalse );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		case EV_ALT_FIRE:
			if (ent && ent->client && ent->client->emoted)
				break;
			FireWeapon( ent, qtrue );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		case EV_SABER_ATTACK:
			if (ent && ent->client && ent->client->emoted)
				break;
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		//rww - Note that these must be in the same order (ITEM#-wise) as they are in holdable_t
		case EV_USE_ITEM1: //seeker droid
			ItemUse_Seeker(ent);
			break;
		case EV_USE_ITEM2: //shield
			ItemUse_Shield(ent);
			break;
		case EV_USE_ITEM3: //medpack
			ItemUse_MedPack(ent);
			break;
		case EV_USE_ITEM4: //big medpack
			ItemUse_MedPack_Big(ent);
			break;
		case EV_USE_ITEM5: //binoculars
			ItemUse_Binoculars(ent);
			break;
		case EV_USE_ITEM6: //sentry gun
			ItemUse_Sentry(ent);
			break;
		case EV_USE_ITEM7: //jetpack
			ItemUse_Jetpack(ent);
			break;
		case EV_USE_ITEM8: //health disp
			break;
		case EV_USE_ITEM9: //ammo disp
			break;
		case EV_USE_ITEM10: //eweb
			ItemUse_UseEWeb(ent);
			break;
		case EV_USE_ITEM11: //cloak
			ItemUse_UseCloak(ent);
			break;
		default:
			break;
		}
	}

}

/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents( playerState_t *ps ) {
	gentity_t *t;
	int event, seq;
	int extEvent, number;

	// if there are still events pending
	if ( ps->entityEventSequence < ps->eventSequence ) {
		// create a temporary entity for this event which is sent to everyone
		// except the client who generated the event
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		// set external event to zero before calling BG_PlayerStateToEntityState
		extEvent = ps->externalEvent;
		ps->externalEvent = 0;
		// create temporary entity for event
		t = G_TempEntity( ps->origin, event );
		number = t->s.number;
		BG_PlayerStateToEntityState( ps, &t->s, qtrue );
		t->s.number = number;
		t->s.eType = ET_EVENTS + event;
		t->s.eFlags |= EF_PLAYER_EVENT;
		t->s.otherEntityNum = ps->clientNum;
		// send to everyone except the client who generated the event
		t->r.svFlags |= SVF_NOTSINGLECLIENT;
		t->r.singleClient = ps->clientNum;
		// set back external event
		ps->externalEvent = extEvent;
	}
}

/*
==================
G_UpdateClientBroadcasts

Determines whether this client should be broadcast to any other clients.  
A client is broadcast when another client is using force sight or is
==================
*/

static qboolean SE_RenderIsVisible( const gentity_t *self, const vec3_t startPos, const vec3_t testOrigin, qboolean reversedCheck ) {
	trace_t results;

	trap_Trace( &results, startPos, NULL, NULL, testOrigin, self - g_entities, MASK_SOLID );
	++level.wallhackTracesDone;

	if ( results.fraction < 1.0f ) {
		if ( ( results.surfaceFlags & SURF_FORCEFIELD )
			|| ( results.surfaceFlags & MATERIAL_MASK ) == MATERIAL_GLASS
			|| ( results.surfaceFlags & MATERIAL_MASK ) == MATERIAL_SHATTERGLASS ) {
			// FIXME: This is a quick hack to render people and things through glass and force fields, but will also take
			// effect even if there is another wall between them (and double glass) - which is bad, of course, but
			// nothing i can prevent right now.
			if ( reversedCheck || SE_RenderIsVisible( self, testOrigin, startPos, qtrue ) ) {
				return qtrue;
			}
		}

		return qfalse;
	}

	return qtrue;
}

static qboolean SE_RenderPlayerChecks( const gentity_t *self, const vec3_t playerOrigin, vec3_t playerPoints[9] ) {
	trace_t results;
	int i;

	for ( i = 0; i < 9; i++ ) {
		if ( trap_PointContents( playerPoints[i], self - g_entities ) == CONTENTS_SOLID ) {
			trap_Trace( &results, playerOrigin, NULL, NULL, playerPoints[i], self - g_entities, MASK_SOLID );
			++level.wallhackTracesDone;
			VectorCopy( results.endpos, playerPoints[i] );
		}
	}

	return qtrue;
}


qboolean SE_IsPlayerCrouching( const gentity_t *ent ) {
	const playerState_t *ps = &ent->client->ps;

	// FIXME: This is no proper way to determine if a client is actually in a crouch position, we want to do this in
	// order to properly hide a client from the enemy when he is crouching behind an obstacle and could not possibly
	// be seen.

	if ( !ent->inuse || !ps ) {
		return qfalse;
	}

	if ( ps->forceHandExtend == HANDEXTEND_KNOCKDOWN ) {
		return qtrue;
	}

	if ( ps->pm_flags & PMF_DUCKED ) {
		return qtrue;
	}

	switch ( ps->legsAnim ) {
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	default:
		break;
	}

	switch ( ps->torsoAnim ) {
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	default:
		break;
	}

	return qfalse;
}

static qboolean SE_RenderPlayerPoints( qboolean isCrouching, const vec3_t playerAngles, const vec3_t playerOrigin, vec3_t playerPoints[9] ) {
	int isHeight = isCrouching ? 32 : 56;
	vec3_t	forward, right, up;

	AngleVectors( playerAngles, forward, right, up );

	VectorMA( playerOrigin, 32.0f, up, playerPoints[0] );
	VectorMA( playerOrigin, 64.0f, forward, playerPoints[1] );
	VectorMA( playerPoints[1], 64.0f, right, playerPoints[1] );
	VectorMA( playerOrigin, 64.0f, forward, playerPoints[2] );
	VectorMA( playerPoints[2], -64.0f, right, playerPoints[2] );
	VectorMA( playerOrigin, -64.0f, forward, playerPoints[3] );
	VectorMA( playerPoints[3], 64.0f, right, playerPoints[3] );
	VectorMA( playerOrigin, -64.0f, forward, playerPoints[4] );
	VectorMA( playerPoints[4], -64.0f, right, playerPoints[4] );

	VectorMA( playerPoints[1], isHeight, up, playerPoints[5] );
	VectorMA( playerPoints[2], isHeight, up, playerPoints[6] );
	VectorMA( playerPoints[3], isHeight, up, playerPoints[7] );
	VectorMA( playerPoints[4], isHeight, up, playerPoints[8] );

	return qtrue;
}

static void GetCameraPosition( const gentity_t *self, vec3_t cameraOrigin ) {
	vec3_t forward;
	int thirdPersonRange = 80, thirdPersonVertOffset = 16;

	AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
	VectorNormalize( forward );

	//Get third person position.  
	VectorCopy( self->client->ps.origin, cameraOrigin );
	VectorMA( cameraOrigin, -thirdPersonRange, forward, cameraOrigin );
	cameraOrigin[2] += 24 + thirdPersonVertOffset;

	if ( SE_IsPlayerCrouching( self ) ) {
		cameraOrigin[2] -= 32;
	}
}

static qboolean SE_NetworkPlayer( const gentity_t *self, const gentity_t *other ) {
	int i;
	vec3_t firstPersonPos, thirdPersonPos, targPos[9];
	unsigned int contents;

	GetCameraPosition( self, thirdPersonPos );

	VectorCopy( self->client->ps.origin, firstPersonPos );
	firstPersonPos[2] += 24;

	if ( SE_IsPlayerCrouching( self ) ) {
		firstPersonPos[2] -= 32;
	}

	contents = trap_PointContents( firstPersonPos, self - g_entities );

	// translucent, we should probably just network them anyways
	if ( contents & ( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME ) ) {
		return qtrue;
	}

	// entirely in an opaque surface, no point networking them.
	if ( contents & ( CONTENTS_SOLID | CONTENTS_TERRAIN | CONTENTS_OPAQUE ) ) {
		return qfalse;
	}

	// plot their bbox pointer into targPos[]
	SE_RenderPlayerPoints( SE_IsPlayerCrouching( other ), other->client->ps.viewangles, other->client->ps.origin, targPos );
	SE_RenderPlayerChecks( self, other->client->ps.origin, targPos );

	for ( i = 0; i < 9; i++ ) {
		if ( SE_RenderIsVisible( self, thirdPersonPos, targPos[i], qfalse ) ) {
			return qtrue;
		}

		if ( SE_RenderIsVisible( self, firstPersonPos, targPos[i], qfalse ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

// Tracing non-players seems to have a bad effect, we know players are limited to 32 per frame, however other gentities
// that are being added are not! It's stupid to actually add traces for it, even with a limited form i used before of 2
// traces per object. There are to many too track and simply networking them takes less FPS either way
static qboolean G_EntityOccluded( const gentity_t *self, const gentity_t *other ) {
	// This is a non-player object, just send it (see above).
	if ( !other->inuse || other->s.number >= level.maxclients ) {
		return qtrue;
	}

	// If this player is me, or my spectee, we will always draw and don't trace.
	if ( self == other ) {
		return qtrue;
	}

	if ( self->client->ps.zoomMode ) { // 0.0
		return qtrue;
	}
	
	// Not rendering; this player's traces did not appear in my screen.
	if ( !SE_NetworkPlayer( self, other ) ) {
		return qtrue;
	}

	return qfalse;
}

#define MAX_JEDI_MASTER_DISTANCE	( ( float )( 2500 * 2500 ) )
#define MAX_JEDI_MASTER_FOV			100.0f
#define MAX_FORCE_SIGHT_DISTANCE	( ( float )( 1500 * 1500 ) )
#define MAX_FORCE_SIGHT_FOV			100.0f

void G_UpdateClientBroadcasts( gentity_t *self ) {
	int i;
	gentity_t *other;

	// we are always sent to ourselves
	// we are always sent to other clients if we are in their PVS
	// if we are not in their PVS, we must set the broadcastClients bit field
	// if we do not wish to be sent to any particular entity, we must set the broadcastClients bit field and the
	//	SVF_BROADCASTCLIENTS bit flag
	self->r.broadcastClients[0] = 0;
	self->r.broadcastClients[1] = 0;

	if ( g_antiWallhack.integer ) {
		self->r.svFlags |= SVF_BROADCASTCLIENTS;
	} else {
		self->r.svFlags &= ~SVF_BROADCASTCLIENTS;
	}

	for ( i = 0, other = g_entities; i < MAX_CLIENTS; i++, other++ ) {
		qboolean send = qfalse;
		float dist;
		vec3_t angles;

		if ( !other->inuse || other->client->pers.connected != CON_CONNECTED ) {
			// no need to compute visibility for non-connected clients
			continue;
		}

		if ( other == self ) {
			// we are always sent to ourselves anyway, this is purely an optimisation
			continue;
		}

		if (other->client->sess.sessionTeam == TEAM_SPECTATOR) {
			send = qtrue;
		}

		VectorSubtract( self->client->ps.origin, other->client->ps.origin, angles );
		dist = VectorLengthSquared( angles );
		vectoangles( angles, angles );

		// broadcast jedi master to everyone if we are in distance/field of view
		if ( g_gametype.integer == GT_JEDIMASTER && self->client->ps.isJediMaster ) {
			if ( dist < MAX_JEDI_MASTER_DISTANCE && InFieldOfVision( other->client->ps.viewangles, MAX_JEDI_MASTER_FOV, angles ) ) {
				send = qtrue;
			}
		}

		// broadcast this client to everyone using force sight, regardless of distance/field of view
		if ( ( other->client->ps.fd.forcePowersActive & ( 1 << FP_SEE ) ) ) {
			send = qtrue;
		}

		// always broadcast if we reached the max traces
		if ( g_wallhackMaxTraces.integer ) {
			if ( level.wallhackTracesDone > g_wallhackMaxTraces.integer ) {
				send = qtrue;
			}
		}

		if ( !send && g_antiWallhack.integer ) {
			if ( other->client->ps.pm_type != PM_SPECTATOR // always send everyone to spectators
				&& ( ( g_antiWallhack.integer == 1 && !other->client->sess.whTrustToggle ) // not whitelisted
				|| ( g_antiWallhack.integer >= 2 && other->client->sess.whTrustToggle ) ) // blacklisted
				&& G_EntityOccluded( self, other ) ) {
				continue;
			} else {
				send = qtrue;
			}
		}

		if ( send ) {
			self->r.broadcastClients[i / 32] |= ( 1 << ( i % 32 ) );
		}
	}

	trap_LinkEntity( self );
}

void G_AddPushVecToUcmd( gentity_t *self, usercmd_t *ucmd )
{
	vec3_t	forward, right, moveDir;
	float	pushSpeed, fMove, rMove;

	if ( !self->client )
	{
		return;
	}
	pushSpeed = VectorLengthSquared(self->client->pushVec);
	if(!pushSpeed)
	{//not being pushed
		return;
	}

	AngleVectors(self->client->ps.viewangles, forward, right, NULL);
	VectorScale(forward, ucmd->forwardmove/127.0f * self->client->ps.speed, moveDir);
	VectorMA(moveDir, ucmd->rightmove/127.0f * self->client->ps.speed, right, moveDir);
	//moveDir is now our intended move velocity

	VectorAdd(moveDir, self->client->pushVec, moveDir);
	self->client->ps.speed = VectorNormalize(moveDir);
	//moveDir is now our intended move velocity plus our push Vector

	fMove = 127.0 * DotProduct(forward, moveDir);
	rMove = 127.0 * DotProduct(right, moveDir);
	ucmd->forwardmove = floor(fMove);//If in the same dir , will be positive
	ucmd->rightmove = floor(rMove);//If in the same dir , will be positive

	if ( self->client->pushVecTime < level.time )
	{
		VectorClear( self->client->pushVec );
	}
}

qboolean G_StandingAnim( int anim )
{//NOTE: does not check idles or special (cinematic) stands
	switch ( anim )
	{
	case BOTH_STAND1:
	case BOTH_STAND2:
	case BOTH_STAND3:
	case BOTH_STAND4:
		return qtrue;
		break;
	}
	return qfalse;
}

qboolean G_ActionButtonPressed(int buttons)
{
	if (buttons & BUTTON_ATTACK)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_USE_HOLDABLE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_GESTURE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_USE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCEGRIP)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_ALT_ATTACK)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCEPOWER)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCE_LIGHTNING)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCE_DRAIN)
	{
		return qtrue;
	}

	return qfalse;
}

void G_CheckClientIdle( gentity_t *ent, usercmd_t *ucmd ) 
{
	vec3_t viewChange;
	qboolean actionPressed;
	int buttons;

	if ( !ent || !ent->client || ent->health <= 0 || ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		ent->client->sess.sessionTeam == TEAM_SPECTATOR || (ent->client->ps.pm_flags & PMF_FOLLOW))
	{
		return;
	}

	buttons = ucmd->buttons;

	if (ent->r.svFlags & SVF_BOT)
	{ //they press use all the time..
		buttons &= ~BUTTON_USE;
	}
	actionPressed = G_ActionButtonPressed(buttons);

	VectorSubtract(ent->client->ps.viewangles, ent->client->idleViewAngles, viewChange);
	if ( !VectorCompare( vec3_origin, ent->client->ps.velocity ) 
		|| actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove 
		|| !G_StandingAnim( ent->client->ps.legsAnim ) 
		|| (ent->health+ent->client->ps.stats[STAT_ARMOR]) != ent->client->idleHealth
		|| VectorLength(viewChange) > 10
		|| ent->client->ps.legsTimer > 0
		|| ent->client->ps.torsoTimer > 0
		|| ent->client->ps.weaponTime > 0
		|| ent->client->ps.weaponstate == WEAPON_CHARGING
		|| ent->client->ps.weaponstate == WEAPON_CHARGING_ALT
		|| ent->client->ps.zoomMode
		|| (ent->client->ps.weaponstate != WEAPON_READY && ent->client->ps.weapon != WP_SABER)
		|| ent->client->ps.forceHandExtend != HANDEXTEND_NONE
		|| ent->client->ps.saberBlocked != BLOCKED_NONE
		|| ent->client->ps.saberBlocking >= level.time
		|| ent->client->ps.weapon == WP_MELEE
		|| (ent->client->ps.weapon != ent->client->pers.cmd.weapon && ent->s.eType != ET_NPC))
	{//FIXME: also check for turning?
		qboolean brokeOut = qfalse;

		if ( !VectorCompare( vec3_origin, ent->client->ps.velocity ) 
			|| actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove 
			|| (ent->health+ent->client->ps.stats[STAT_ARMOR]) != ent->client->idleHealth
			|| ent->client->ps.zoomMode
			|| (ent->client->ps.weaponstate != WEAPON_READY && ent->client->ps.weapon != WP_SABER)
			|| (ent->client->ps.weaponTime > 0 && ent->client->ps.weapon == WP_SABER)
			|| ent->client->ps.weaponstate == WEAPON_CHARGING
			|| ent->client->ps.weaponstate == WEAPON_CHARGING_ALT
			|| ent->client->ps.forceHandExtend != HANDEXTEND_NONE
			|| ent->client->ps.saberBlocked != BLOCKED_NONE
			|| ent->client->ps.saberBlocking >= level.time
			|| ent->client->ps.weapon == WP_MELEE
			|| (ent->client->ps.weapon != ent->client->pers.cmd.weapon && ent->s.eType != ET_NPC))
		{
			//if in an idle, break out
			switch ( ent->client->ps.legsAnim )
			{
			case BOTH_STAND1IDLE1:
			case BOTH_STAND2IDLE1:
			case BOTH_STAND2IDLE2:
			case BOTH_STAND3IDLE1:
			case BOTH_STAND5IDLE1:
				ent->client->ps.legsTimer = 0;
				brokeOut = qtrue;
				break;
			}
			switch ( ent->client->ps.torsoAnim )
			{
			case BOTH_STAND1IDLE1:
			case BOTH_STAND2IDLE1:
			case BOTH_STAND2IDLE2:
			case BOTH_STAND3IDLE1:
			case BOTH_STAND5IDLE1:
				ent->client->ps.torsoTimer = 0;
				ent->client->ps.weaponTime = 0;
				ent->client->ps.saberMove = LS_READY;
				brokeOut = qtrue;
				break;
			}
		}
		//
		ent->client->idleHealth = (ent->health+ent->client->ps.stats[STAT_ARMOR]);
		VectorCopy(ent->client->ps.viewangles, ent->client->idleViewAngles);
		if ( ent->client->idleTime < level.time )
		{
			ent->client->idleTime = level.time;
		}

		if (brokeOut &&
			(ent->client->ps.weaponstate == WEAPON_CHARGING || ent->client->ps.weaponstate == WEAPON_CHARGING_ALT))
		{
			ent->client->ps.torsoAnim = TORSO_RAISEWEAP1;
		}
	}
	else if ( level.time - ent->client->idleTime > 5000 )
	{//been idle for 5 seconds
		int	idleAnim = -1;
		switch ( ent->client->ps.legsAnim )
		{
		case BOTH_STAND1:
			idleAnim = BOTH_STAND1IDLE1;
			break;
		case BOTH_STAND2:
			idleAnim = BOTH_STAND2IDLE1;
			break;
		case BOTH_STAND3:
			idleAnim = BOTH_STAND3IDLE1;
			break;
		case BOTH_STAND5:
			idleAnim = BOTH_STAND5IDLE1;
			break;
		}

		if (idleAnim == BOTH_STAND2IDLE1 && Q_irand(1, 10) <= 5)
		{
			idleAnim = BOTH_STAND2IDLE2;
		}

		if ( idleAnim != -1 && idleAnim > 0 && idleAnim < MAX_ANIMATIONS )
		{
			G_SetAnim(ent, ucmd, SETANIM_BOTH, idleAnim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);

			//don't idle again after this anim for a while
			ent->client->idleTime = level.time + ent->client->ps.legsTimer + Q_irand( 0, 2000 );
		}
	}
}

void NPC_Accelerate( gentity_t *ent, qboolean fullWalkAcc, qboolean fullRunAcc )
{
	if ( !ent->client || !ent->NPC )
	{
		return;
	}

	if ( !ent->NPC->stats.acceleration )
	{//No acceleration means just start and stop
		ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
	}
	//FIXME:  in cinematics always accel/decel?
	else if ( ent->NPC->desiredSpeed <= ent->NPC->stats.walkSpeed )
	{//Only accelerate if at walkSpeeds
		if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration )
		{
			ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed )
		{
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
		else if ( fullWalkAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration )
		{//decelerate even when walking
			ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
		{//stop on a dime
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
	}
	else//  if ( ent->NPC->desiredSpeed > ent->NPC->stats.walkSpeed )
	{//Only decelerate if at runSpeeds
		if ( fullRunAcc && ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration )
		{//Accelerate to runspeed
			ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed )
		{//accelerate instantly
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
		else if ( fullRunAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration )
		{
			ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
		{
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
	}
}

/*
-------------------------
NPC_GetWalkSpeed
-------------------------
*/

static int NPC_GetWalkSpeed( gentity_t *ent )
{
	int	walkSpeed = 0;

	if ( ( ent->client == NULL ) || ( ent->NPC == NULL ) )
		return 0;

	switch ( ent->client->playerTeam )
	{
	case NPCTEAM_PLAYER:	//To shutup compiler, will add entries later (this is stub code)
	default:
		walkSpeed = ent->NPC->stats.walkSpeed;
		break;
	}

	return walkSpeed;
}

/*
-------------------------
NPC_GetRunSpeed
-------------------------
*/
static int NPC_GetRunSpeed( gentity_t *ent )
{
	int	runSpeed = 0;

	if ( ( ent->client == NULL ) || ( ent->NPC == NULL ) )
		return 0;

	// team no longer indicates species/race.  Use NPC_class to adjust speed for specific npc types
	switch( ent->client->NPC_class)
	{
	case CLASS_PROBE:	// droid cases here to shut-up compiler
	case CLASS_GONK:
	case CLASS_R2D2:
	case CLASS_R5D2:
	case CLASS_MARK1:
	case CLASS_MARK2:
	case CLASS_PROTOCOL:
	case CLASS_ATST: // hmm, not really your average droid
	case CLASS_MOUSE:
	case CLASS_SEEKER:
	case CLASS_REMOTE:
		runSpeed = ent->NPC->stats.runSpeed;
		break;

	default:
		runSpeed = ent->NPC->stats.runSpeed*1.3f; //rww - seems to slow in MP for some reason.
		break;
	}

	return runSpeed;
}

//Seems like a slightly less than ideal method for this, could it be done on the client?
extern qboolean FlyingCreature( gentity_t *ent );
void G_CheckMovingLoopingSounds( gentity_t *ent, usercmd_t *ucmd )
{
	if ( ent->client )
	{
		if ( (ent->NPC&&!VectorCompare( vec3_origin, ent->client->ps.moveDir ))//moving using moveDir
			|| ucmd->forwardmove || ucmd->rightmove//moving using ucmds
			|| (ucmd->upmove&&FlyingCreature( ent ))//flier using ucmds to move
			|| (FlyingCreature( ent )&&!VectorCompare( vec3_origin, ent->client->ps.velocity )&&ent->health>0))//flier using velocity to move
		{
			switch( ent->client->NPC_class )
			{
			case CLASS_R2D2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/r2d2/misc/r2_move_lp.wav" );
				break;
			case CLASS_R5D2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/r2d2/misc/r2_move_lp2.wav" );
				break;
			case CLASS_MARK2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/mark2/misc/mark2_move_lp" );
				break;
			case CLASS_MOUSE:
				ent->s.loopSound = G_SoundIndex( "sound/chars/mouse/misc/mouse_lp" );
				break;
			case CLASS_PROBE:
				ent->s.loopSound = G_SoundIndex( "sound/chars/probe/misc/probedroidloop" );
				break;
			default:
				break;
			}
		}
		else
		{//not moving under your own control, stop loopSound
			if ( ent->client->NPC_class == CLASS_R2D2 || ent->client->NPC_class == CLASS_R5D2 
					|| ent->client->NPC_class == CLASS_MARK2 || ent->client->NPC_class == CLASS_MOUSE 
					|| ent->client->NPC_class == CLASS_PROBE )
			{
				ent->s.loopSound = 0;
			}
		}
	}
}

void G_HeldByMonster( gentity_t *ent, usercmd_t **ucmd )
{
	if ( ent 
		&& ent->client
		&& ent->client->ps.hasLookTarget )//NOTE: lookTarget is an entity number, so this presumes that client 0 is NOT a Rancor...
	{
		gentity_t *monster = &g_entities[ent->client->ps.lookTarget];
		if ( monster && monster->client )
		{
			//take the monster's waypoint as your own
			ent->waypoint = monster->waypoint;
			if ( monster->s.NPC_class == CLASS_RANCOR )
			{//only possibility right now, may add Wampa and Sand Creature later
				BG_AttachToRancor( monster->ghoul2, //ghoul2 info
					monster->r.currentAngles[YAW],
					monster->r.currentOrigin,
					level.time,
					NULL,
					monster->modelScale,
					(monster->client->ps.eFlags2&EF2_GENERIC_NPC_FLAG),
					ent->client->ps.origin,
					ent->client->ps.viewangles,
					NULL );
			}
			VectorClear( ent->client->ps.velocity );
			G_SetOrigin( ent, ent->client->ps.origin );
			SetClientViewAngle( ent, ent->client->ps.viewangles );
			G_SetAngles( ent, ent->client->ps.viewangles );
			trap_LinkEntity( ent );//redundant?
		}
	}
	// don't allow movement, weapon switching, and most kinds of button presses
	(*ucmd)->forwardmove = 0;
	(*ucmd)->rightmove = 0;
	(*ucmd)->upmove = 0;
}

void G_SetTauntAnim( gentity_t *ent, int taunt )
{
	if (taunt == TAUNT_MEDITATE &&
		(ent->client->pers.cmd.upmove ||
		ent->client->pers.cmd.forwardmove ||
		ent->client->pers.cmd.rightmove))
	{ //hack, don't do while moving
		//just apply this annoying restriction to meditate
		return;
	}
	if ( taunt != TAUNT_TAUNT && !g_moreTaunts.integer)
	{//normal taunt always allowed
		if ( g_gametype.integer != GT_DUEL
			&& g_gametype.integer != GT_POWERDUEL )
		{//no taunts unless in Duel
			return;
		}
	}
	if (g_gametype.integer != GT_DUEL && g_gametype.integer != GT_POWERDUEL && taunt == TAUNT_MEDITATE)
	{
		if (g_emotes.integer) {
			if (level.isLivePug != ISLIVEPUG_NO)
				ent->client->emoted = qtrue; // apply emote restrictions to meditate during pugs
		}
		else {
			return; // no meditating in non-duel gametypes unless emotes are enabled
		}
	}
	// *CHANGE 65* fix - release rocket lock, old bug
		BG_ClearRocketLock(&ent->client->ps);

	if ( ent->client->ps.torsoTimer < 1
		&& ent->client->ps.forceHandExtend == HANDEXTEND_NONE 
		&& ent->client->ps.legsTimer < 1
		&& ent->client->ps.weaponTime < 1 
		&& ent->client->ps.saberLockTime < level.time )
	{
		int anim = -1;
		switch ( taunt )
		{
		case TAUNT_TAUNT:
			if ( ent->client->ps.weapon != WP_SABER )
			{
				anim = BOTH_ENGAGETAUNT;
			}
			else if ( ent->client->saber[0].tauntAnim != -1 )
			{
				anim = ent->client->saber[0].tauntAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].tauntAnim != -1 )
			{
				anim = ent->client->saber[1].tauntAnim;
			}
			else
			{
				switch ( ent->client->ps.fd.saberAnimLevel )
				{
				case SS_FAST:
				case SS_TAVION:
					if ( ent->client->ps.saberHolstered == 1 
						&& ent->client->saber[1].model 
						&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
					{//turn off second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOff );
					}
					else if ( ent->client->ps.saberHolstered == 0 && ent->client->ps.weapon == WP_SABER)
					{//turn off first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOff );
					}
					ent->client->ps.saberHolstered = 2;
					anim = BOTH_GESTURE1;
					break;
				case SS_MEDIUM:
				case SS_STRONG:
				case SS_DESANN:
					anim = BOTH_ENGAGETAUNT;
					break;
				case SS_DUAL:
					if ( ent->client->ps.saberHolstered == 1 
						&& ent->client->saber[1].model 
						&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
					{//turn on second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn );
					}
					else if ( ent->client->ps.saberHolstered == 2 && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_DUAL_TAUNT;
					break;
				case SS_STAFF:
					if ( ent->client->ps.saberHolstered > 0 && ent->client->ps.weapon == WP_SABER)
					{//turn on all blades
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_STAFF_TAUNT;
					break;
				}
			}
			break;
		case TAUNT_BOW:
			if ( ent->client->saber[0].bowAnim != -1 )
			{
				anim = ent->client->saber[0].bowAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].bowAnim != -1 )
			{
				anim = ent->client->saber[1].bowAnim;
			}
			else
			{
				anim = BOTH_BOW;
			}
			if ( ent->client->ps.saberHolstered == 1 
				&& ent->client->saber[1].model 
				&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
			{//turn off second saber
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOff );
			}
			else if ( ent->client->ps.saberHolstered == 0 && ent->client->ps.weapon == WP_SABER)
			{//turn off first
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOff );
			}
			ent->client->ps.saberHolstered = 2;
			break;
		case TAUNT_MEDITATE:
			if ( ent->client->saber[0].meditateAnim != -1 )
			{
				anim = ent->client->saber[0].meditateAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].meditateAnim != -1 )
			{
				anim = ent->client->saber[1].meditateAnim;
			}
			else
			{
				anim = BOTH_MEDITATE;
			}
			if ( ent->client->ps.saberHolstered == 1 
				&& ent->client->saber[1].model 
				&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
			{//turn off second saber
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOff );
			}
			else if ( ent->client->ps.saberHolstered == 0 && ent->client->ps.weapon == WP_SABER)
			{//turn off first
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOff );
			}
			ent->client->ps.saberHolstered = 2;
			break;
		case TAUNT_FLOURISH:
				if ( ent->client->ps.saberHolstered == 1 
					&& ent->client->saber[1].model 
					&& ent->client->saber[1].model[0] )
				{//turn on second saber
					G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn);
				}
				else if ( ent->client->ps.saberHolstered == 2 && ent->client->ps.weapon == WP_SABER)
				{//turn on first
					G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
				}
				ent->client->ps.saberHolstered = 0;
				if ( ent->client->saber[0].flourishAnim != -1 )
				{
					anim = ent->client->saber[0].flourishAnim;
				}
				else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].flourishAnim != -1 )
				{
					anim = ent->client->saber[1].flourishAnim;
				}
				else
				{
					switch ( ent->client->ps.fd.saberAnimLevel )
					{
					case SS_FAST:
					case SS_TAVION:
						anim = BOTH_SHOWOFF_FAST;
						break;
					case SS_MEDIUM:
						anim = BOTH_SHOWOFF_MEDIUM;
						break;
					case SS_STRONG:
					case SS_DESANN:
						anim = BOTH_SHOWOFF_STRONG;
						break;
					case SS_DUAL:
						anim = BOTH_SHOWOFF_DUAL;
						break;
					case SS_STAFF:
						anim = BOTH_SHOWOFF_STAFF;
						break;
					}
				}
			break;
		case TAUNT_GLOAT:
			if ( ent->client->saber[0].gloatAnim != -1 )
			{
				anim = ent->client->saber[0].gloatAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].gloatAnim != -1 )
			{
				anim = ent->client->saber[1].gloatAnim;
			}
			else
			{
				switch ( ent->client->ps.fd.saberAnimLevel )
				{
				case SS_FAST:
				case SS_TAVION:
					anim = BOTH_VICTORY_FAST;
					break;
				case SS_MEDIUM:
					anim = BOTH_VICTORY_MEDIUM;
					break;
				case SS_STRONG:
				case SS_DESANN:
					if ( ent->client->ps.saberHolstered && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_STRONG;
					break;
				case SS_DUAL:
					if ( ent->client->ps.saberHolstered == 1 
						&& ent->client->saber[1].model 
						&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
					{//turn on second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn );
					}
					else if ( ent->client->ps.saberHolstered == 2 && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_DUAL;
					break;
				case SS_STAFF:
					if ( ent->client->ps.saberHolstered && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_STAFF;
					break;
				}
			}
			break;
		}
		if ( anim != -1 )
		{
			if ( ent->client->ps.groundEntityNum != ENTITYNUM_NONE && !(ent->client->ps.m_iVehicleNum)) //taunt anim inside a vehicle can bug weapons
			{
				ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
				ent->client->ps.forceDodgeAnim = anim;
				ent->client->ps.forceHandExtendTime = level.time + BG_AnimLength(ent->localAnimIndex, (animNumber_t)anim);
			}
			if ( taunt == TAUNT_TAUNT || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
			{
				//just use classic sound
				G_AddEvent( ent, EV_TAUNT, taunt );
			}
			else if (taunt == TAUNT_GLOAT || taunt == TAUNT_FLOURISH)
			{
				//generate dank taunt sounds for clients that support them

				//determine the cool taunt sound randomly
				int rng = 0;
				while (rng >= 0 && rng <= TAUNT_GLOAT) { // make sure the final number isn't <= 4
					rng = Q_irand(0, 255);
					rng &= ~3; // zero the lower two bits
					if (taunt == TAUNT_FLOURISH)
						rng |= 1; // set the lowest bit for flourish
				}
				G_AddEvent(ent, EV_TAUNT, rng);
				//non-supporting clients will just use the normal taunt sound anyway;
				//no need to differentiate between users of different mods here.
			}
		}
	}
}

void SiegeGhostUpdate(int sendClientNum, qboolean forceUpdate) {
	if (g_gametype.integer != GT_SIEGE || !g_siegeGhosting.integer)
		return;

	if (sendClientNum < 0 || sendClientNum >= MAX_CLIENTS) {
		assert(qfalse);
		return;
	}

	static int lastUpdate[MAX_CLIENTS] = { 0 };
	if (!forceUpdate && !(level.time - lastUpdate[sendClientNum] > 200))
		return;
	lastUpdate[sendClientNum] = level.time;

	gclient_t *client = &level.clients[sendClientNum];
	gentity_t *followee = NULL;

	if (client->sess.sessionTeam == TEAM_SPECTATOR) {
		if (client->sess.spectatorState == SPECTATOR_FOLLOW && client->sess.spectatorClient < MAX_CLIENTS && g_entities[client->sess.spectatorClient].inuse &&
			g_entities[client->sess.spectatorClient].client && g_entities[client->sess.spectatorClient].client->fakeSpec) {
			followee = &g_entities[g_entities[client->sess.spectatorClient].client->fakeSpecClient];
		}
	}
	else if (client->fakeSpec) {
		followee = &g_entities[client->fakeSpecClient];
	}

	// we probably don't need to bother validating the followee here

	if (followee) { // send some info to constitute a fake hud

#define GHOSTDATA_MISCELLANEOUSINTEGER_HASSENSE2OR3	(1 << 0)
		int miscellaneousInteger = 0;
		if (followee->client->ps.fd.forcePowerLevel[FP_SEE] >= FORCE_LEVEL_2)
			miscellaneousInteger |= GHOSTDATA_MISCELLANEOUSINTEGER_HASSENSE2OR3;

		trap_SendServerCommand(sendClientNum, va("kls -1 -1 sgho %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			followee - g_entities,
			followee->health >= 0 ? followee->health : 0,
			followee->health >= 0 ? followee->client->ps.stats[STAT_ARMOR] : 0,
			followee->client->ps.stats[STAT_MAX_HEALTH],
			followee->client->ps.fd.forcePowersKnown ? followee->client->ps.fd.forcePower : -1,
			followee->client->ps.weapon,
			weaponData[followee->client->ps.weapon].energyPerShot ? Com_Clampi(0, 999, followee->client->ps.ammo[weaponData[followee->client->ps.weapon].ammoIndex]) : -1,
			followee->client->ps.fd.saberAnimLevel,
			followee->client->ps.emplacedIndex,
			followee->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK) ? followee->client->ps.jetpackFuel : -1,
			followee->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK) ? followee->client->ps.cloakFuel : -1,
			followee->client->ps.fd.forcePowersActive,
			followee->client->ps.fd.forceRageRecoveryTime,
			followee->client->ps.m_iVehicleNum && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle ? followee->client->ps.m_iVehicleNum : /*-1 */ 0,
			followee->client->ps.m_iVehicleNum && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle ? g_entities[followee->client->ps.m_iVehicleNum].health : -1,
			followee->client->ps.m_iVehicleNum && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle ? g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle->m_iTurboTime : -1,
			followee->client->ps.m_iVehicleNum && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle->m_pVehicleInfo && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle->m_pVehicleInfo->weapon[0].ID ? g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle->weaponStatus[0].ammo : -1,
			followee->client->ps.m_iVehicleNum && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle->m_pVehicleInfo && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle->m_pVehicleInfo->weapon[1].ID ? g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle->weaponStatus[1].ammo : -1,
			followee->client->ps.m_iVehicleNum && g_entities[followee->client->ps.m_iVehicleNum].m_pVehicle ? g_entities[followee->client->ps.m_iVehicleNum].playerState->stats[STAT_ARMOR] : -1,
			miscellaneousInteger
		));
	}
	else { // send a non-update
		trap_SendServerCommand(sendClientNum, "kls -1 -1 sgho");
	}
}

void ForceSiegeGhostUpdateForFollowers(int followedClientNum) {
	assert(followedClientNum >= 0 && followedClientNum < MAX_CLIENTS);

	for (int i = 0; i < MAX_CLIENTS; i++) { // send an update to anyone following this guy
		if (i == followedClientNum)
			continue;

		gentity_t *thisGuy = &g_entities[i];
		if (thisGuy->inuse && thisGuy->client && thisGuy->client->sess.sessionTeam == TEAM_SPECTATOR &&
			thisGuy->client->sess.spectatorState == SPECTATOR_FOLLOW && thisGuy->client->sess.spectatorClient == followedClientNum) {
			SiegeGhostUpdate(thisGuy - g_entities, qtrue);
		}
	}
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
qboolean IsMindTrickedByAnyone(int targetClientNum);
void ClientThink_real( gentity_t *ent ) {
	gclient_t	*client;
	pmove_t		pm;
	int			oldEventSequence;
	int			msec;
	int			forcePowerNum;
	usercmd_t	*ucmd;
	qboolean	isNPC = qfalse;
	qboolean	controlledByPlayer = qfalse;
	qboolean	killJetFlags = qtrue;

	client = ent->client;

	if (ent - g_entities < MAX_CLIENTS)
		level.lastThinkRealTime[ent - g_entities] = trap_Milliseconds();

	if (ent->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	// don't think if the client is not yet connected (and thus not yet spawned in)
	if (client->pers.connected != CON_CONNECTED && !isNPC) {
		return;
	}

	// This code was moved here from clientThink to fix a problem with g_synchronousClients 
	// being set to 1 when in vehicles. 
	if ( ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		if (g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

			if (veh->m_pVehicle &&
				veh->m_pVehicle->m_pPilot == (bgEntity_t *)ent)
			{ //only take input from the pilot...
				veh->client->ps.commandTime = ent->client->ps.commandTime;
				memcpy(&veh->m_pVehicle->m_ucmd, &ent->client->pers.cmd, sizeof(usercmd_t));
				if ( veh->m_pVehicle->m_ucmd.buttons & BUTTON_TALK )
				{ //forced input if "chat bubble" is up
					veh->m_pVehicle->m_ucmd.buttons = BUTTON_TALK;
					veh->m_pVehicle->m_ucmd.forwardmove = 0;
					veh->m_pVehicle->m_ucmd.rightmove = 0;
					veh->m_pVehicle->m_ucmd.upmove = 0;
				}
			}
		}
	}

	if (!(client->ps.pm_flags & PMF_FOLLOW))
	{
		if (g_gametype.integer == GT_SIEGE &&
			client->siegeClass != -1 &&
			bgSiegeClasses[client->siegeClass].saberStance)
		{ //the class says we have to use this stance set.
			if (!(bgSiegeClasses[client->siegeClass].saberStance & (1 << client->ps.fd.saberAnimLevel)))
			{ //the current stance is not in the bitmask, so find the first one that is.
				int i = SS_FAST;

				while (i < SS_NUM_SABER_STYLES)
				{
					if (bgSiegeClasses[client->siegeClass].saberStance & (1 << i))
					{
						if (i == SS_DUAL 
							&& client->ps.saberHolstered == 1 )
						{//one saber should be off, adjust saberAnimLevel accordinly
							client->ps.fd.saberAnimLevelBase = i;
							client->ps.fd.saberAnimLevel = SS_FAST;
							client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
						}
						else if ( i == SS_STAFF
							&& client->ps.saberHolstered == 1 
							&& client->saber[0].singleBladeStyle != SS_NONE)
						{//one saber or blade should be off, adjust saberAnimLevel accordinly
							client->ps.fd.saberAnimLevelBase = i;
							client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
							client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
						}
						else
						{
							client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = i;
							client->ps.fd.saberDrawAnimLevel = i;
						}
						break;
					}

					i++;
				}
			}
		}
		else if (client->saber[0].model[0] && client->saber[1].model[0])
		{ //with two sabs always use akimbo style
			if ( client->ps.saberHolstered == 1 )
			{//one saber should be off, adjust saberAnimLevel accordinly
				client->ps.fd.saberAnimLevelBase = SS_DUAL;
				client->ps.fd.saberAnimLevel = SS_FAST;
				client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
			}
			else
			{
				if ( !WP_SaberStyleValidForSaber( &client->saber[0], &client->saber[1], client->ps.saberHolstered, client->ps.fd.saberAnimLevel ) )
				{//only use dual style if the style we're trying to use isn't valid
					client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = SS_DUAL;
				}
				client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
			}
		}
		else
		{
			if (client->saber[0].stylesLearned == (1<<SS_STAFF) )
			{ //then *always* use the staff style
				client->ps.fd.saberAnimLevelBase = SS_STAFF;
			}
			if ( client->ps.fd.saberAnimLevelBase == SS_STAFF )
			{//using staff style
				if ( client->ps.saberHolstered == 1 
					&& client->saber[0].singleBladeStyle != SS_NONE)
				{//one blade should be off, adjust saberAnimLevel accordinly
					client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
					client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
				}
				else
				{
					client->ps.fd.saberAnimLevel = SS_STAFF;
					client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
				}
			}
		}
	}

	// mark the time, so the connection sprite can be removed
	ucmd = &ent->client->pers.cmd;

    if ( client && (client->ps.eFlags2&EF2_HELD_BY_MONSTER) && (!(ent->client->ps.pm_flags & PMF_FOLLOW)) )
	{
		G_HeldByMonster( ent, &ucmd );
	}

	// sanity check the command time to prevent speedup cheating
	if ( ucmd->serverTime > level.time + 200 ) {
		ucmd->serverTime = level.time + 200;
	}
	if ( ucmd->serverTime < level.time - 1000 ) {
		ucmd->serverTime = level.time - 1000;
	} 

	if (isNPC && (ucmd->serverTime - client->ps.commandTime) < 1)
	{
		ucmd->serverTime = client->ps.commandTime + 100;
	}

	msec = ucmd->serverTime - client->ps.commandTime;
	// following others may result in bad times, but we still want
	// to check for follow toggles
	if ( msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		return;
	}

	if ( msec > 200 ) {
		msec = 200;
	}

	if ( pmove_msec.integer < 3 ) {
		trap_Cvar_Set("pmove_msec", "3");
	}
	else if (pmove_msec.integer > 50) {
		trap_Cvar_Set("pmove_msec", "50");
	}

	if ( pmove_fixed.integer || client->pers.pmoveFixed ) {
		ucmd->serverTime = ((ucmd->serverTime + pmove_msec.integer-1) / pmove_msec.integer) * pmove_msec.integer;
	}

	//
	// check for exiting intermission
	//
	if ( level.intermissiontime ) 
	{
		if ( ent->s.number < MAX_CLIENTS
			|| client->NPC_class == CLASS_VEHICLE )
		{//players and vehicles do nothing in intermissions
			ClientIntermissionThink( client );
			return;
		}
	}

	if (g_siegeGhosting.integer && g_gametype.integer == GT_SIEGE && client->tempSpectate > level.time && client->ps.clientNum < MAX_CLIENTS && !(g_entities[client->ps.clientNum].r.svFlags & SVF_BOT)) {
		// dead siege spec; try to follow a teammate if anyone else is still alive
		client->oldbuttons = client->buttons;
		client->buttons = ucmd->buttons;
		int offset = client->fakeSpecClient;
		int direction = 1;
		qboolean tryChange = qfalse;

		if ((client->buttons & BUTTON_ATTACK) && !(client->oldbuttons & BUTTON_ATTACK)) { // mouse1
			tryChange = qtrue;
		}
		else if ((client->buttons & BUTTON_ALT_ATTACK) && !(client->oldbuttons & BUTTON_ALT_ATTACK)) { // mouse2
			tryChange = qtrue;
			direction = -1;
		}

		gentity_t *followee = NULL;

		// try to keep following the person we were already following
		if (client->fakeSpec) {
			followee = &g_entities[client->fakeSpecClient];
			if (!followee->inuse || followee == ent || !followee->client || followee->client->pers.connected != CON_CONNECTED ||
				followee->client->sess.sessionTeam != ent->client->sess.sessionTeam || followee->client->tempSpectate >= level.time) {
				followee = NULL; // the followee has died or disconnected or something
			}
		}

		// try to cycle to someone else
		if (tryChange || !followee) {
			offset %= MAX_CLIENTS;
			int limit = direction == 1 ? MAX_CLIENTS : -MAX_CLIENTS;
			for (int i = 0; i != limit; i += direction) {
				int j = (i + offset) % MAX_CLIENTS;
				if (j < 0)
					j = MAX_CLIENTS - j;
				followee = &g_entities[j];
				if (!followee->inuse || !followee->client || followee->client->pers.connected != CON_CONNECTED || followee == ent || (client->fakeSpec && client->fakeSpecClient == j) ||
					followee->client->sess.sessionTeam != ent->client->sess.sessionTeam || followee->client->tempSpectate >= level.time) {
					followee = NULL;
					continue;
				}
				break;
			}
			if (!followee && client->fakeSpec)
				followee = &g_entities[client->fakeSpecClient]; // couldn't find anyone; just keep following the person we were already following
		}
		else {
			// just keep following the person we were already following
			if (client->fakeSpec)
				followee = &g_entities[client->fakeSpecClient];
		}

		if (followee && (!followee->inuse || followee == ent || !followee->client || followee->client->pers.connected != CON_CONNECTED ||
			followee->client->sess.sessionTeam != ent->client->sess.sessionTeam || followee->client->tempSpectate >= level.time)) {
			followee = NULL; // the followee has died or disconnected or something
		}

		qboolean changed = qfalse;
		if (followee) { // we have someone to follow
			if (!client->fakeSpec)
				G_MuteSound(ent - g_entities, CHAN_VOICE); // mute death sounds at first, so they don't teleport to the follow target
			if (!client->fakeSpec || client->fakeSpecClient != followee->client->ps.clientNum)
				changed = qtrue;
			client->fakeSpec = qtrue;
			client->fakeSpecClient = followee->client->ps.clientNum;

			pmove_t	pm;

			client->ps.pm_type = PM_FLOAT; // mega hack
			client->ps.speed = 0;
			client->ps.basespeed = 0;
			client->ps.legsAnim = 0;
			client->ps.legsTimer = 0;
			client->ps.torsoAnim = 0;
			client->ps.torsoTimer = 0;
			memset(&pm, 0, sizeof(pm));
			pm.ps = &client->ps;
			pm.cmd = *ucmd;
			pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
			pm.trace = trap_Trace;
			pm.pointcontents = trap_PointContents;
			pm.noSpecMove = 1;
			pm.animations = NULL;
			pm.nonHumanoid = qfalse;
			pm.baseEnt = (bgEntity_t *)g_entities;
			pm.entSize = sizeof(gentity_t);
			Pmove(&pm);
			VectorCopy(followee->client->ps.origin, client->ps.origin);
			if (changed) {
				client->ps.eFlags ^= EF_TELEPORT_BIT; // toggle the teleport bit so the client knows to not lerp
			}
			SetClientViewAngle(ent, followee->client->ps.viewangles);
			VectorCopy(client->ps.origin, ent->s.origin);
			ent->client->ps.zoomFov = followee->client->ps.zoomFov;
			ent->client->ps.zoomLocked = followee->client->ps.zoomLocked;
			ent->client->ps.zoomLockTime = followee->client->ps.zoomLockTime;
			ent->client->ps.zoomMode = followee->client->ps.zoomMode;
			ent->client->ps.zoomTime = followee->client->ps.zoomTime;
			ent->client->ps.holocronBits = followee->client->ps.holocronBits;
			ent->client->ps.rocketLockIndex = followee->client->ps.rocketLockIndex;
			ent->client->ps.rocketLockTime = followee->client->ps.rocketLockTime;
			ent->client->ps.hackingBaseTime = followee->client->ps.hackingBaseTime;
			ent->client->ps.hackingTime = followee->client->ps.hackingTime;
			ent->client->ps.standheight = followee->client->ps.standheight;
			ent->client->ps.crouchheight = followee->client->ps.crouchheight;
			ent->client->ps.viewheight = followee->client->ps.viewheight;
			ent->client->ps.pm_flags = followee->client->ps.pm_flags;
			ent->client->ps.stats[STAT_DEAD_YAW] = followee->client->ps.stats[STAT_DEAD_YAW];
			trap_UnlinkEntity(ent);

			SiegeGhostUpdate(ent - g_entities, changed); // force it if they just started ghosting or changed ghost targets
			if (changed)
				ForceSiegeGhostUpdateForFollowers(ent - g_entities);
		}
		else { // nobody available to follow
			SiegeGhostUpdate(ent - g_entities, client->fakeSpec); // force it if they just became a free ghost
			if (client->fakeSpec)
				ForceSiegeGhostUpdateForFollowers(ent - g_entities);
			client->fakeSpec = qfalse;

			// spectators don't do much
			if (client->sess.sessionTeam == TEAM_SPECTATOR || client->tempSpectate > level.time) {
				CheckSpectatorInactivityTimer(client);
				if (client->sess.spectatorState == SPECTATOR_SCOREBOARD) {
					return;
				}
				if (g_siegeGhosting.integer == 2) { // force you to a specific camera spot if everyone is dead
					float distToCamera = Distance(client->ps.origin, level.ghostCameraOrigin);
					if (distToCamera > 50)
						client->ps.eFlags ^= EF_TELEPORT_BIT; // don't predict
					VectorCopy(level.ghostCameraOrigin, client->ps.origin);
					VectorCopy(client->ps.origin, ent->s.origin);
					vec3_t angs;
					angs[PITCH] = 0;
					angs[YAW] = level.ghostCameraYaw;
					angs[ROLL] = 0;
					SetClientViewAngle(ent, angs);
					VectorClear(client->ps.velocity);
					ent->client->ps.zoomFov = ent->client->ps.zoomLocked = ent->client->ps.zoomLockTime = ent->client->ps.zoomMode = ent->client->ps.zoomTime = 0;
					ent->client->ps.rocketLockIndex = ent->client->ps.rocketLockTime = ent->client->ps.holocronBits = 0;
					ent->client->ps.hackingBaseTime = ent->client->ps.hackingTime = 0;
					ent->client->ps.standheight = DEFAULT_MAXS_2;
					ent->client->ps.crouchheight = CROUCH_VIEWHEIGHT;
					ent->client->ps.viewheight = DEFAULT_VIEWHEIGHT;
					ent->client->ps.pm_flags = 0;
					ent->client->ps.stats[STAT_DEAD_YAW] = 0;
				}
				SpectatorThink(ent, ucmd);
			}
		}
		return;
	}
	else {
		// spectators don't do much
		if (client->sess.sessionTeam == TEAM_SPECTATOR || client->tempSpectate > level.time) {
			CheckSpectatorInactivityTimer(client);
			if (client->sess.spectatorState == SPECTATOR_SCOREBOARD) {
				return;
			}
			if (client->sess.sessionTeam == TEAM_SPECTATOR)
				SiegeGhostUpdate(ent - g_entities, qfalse); // periodically update specs if they are following ghosts
			SpectatorThink(ent, ucmd);
			return;
		}
	}

    if (ent->s.eType != ET_NPC && ent - g_entities < MAX_CLIENTS && client)
    {
        // check for inactivity timer, but never drop the local client of a non-dedicated server
        if (!CheckPlayerInactivityTimer(client)) {
            return;
        }
    }

	if (ent && ent->client && (ent->client->ps.eFlags & EF_INVULNERABLE))
	{
		if (ent->client->invulnerableTimer <= level.time)
		{
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
		}
	}

	//Check if we should have a fullbody push effect around the player
	if (client->pushEffectTime > level.time)
	{
		client->ps.eFlags |= EF_BODYPUSH;
	}
	else if (client->pushEffectTime)
	{
		client->pushEffectTime = 0;
		client->ps.eFlags &= ~EF_BODYPUSH;
	}

	if (client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK))
	{
		client->ps.eFlags |= EF_JETPACK;
	}
	else
	{
		client->ps.eFlags &= ~EF_JETPACK;
	}

	// duo: force people to stop holding their mouse1/mouse2 button(s) after they get forced off an emplaced gun
	// fixes e.g. blowing yourself up if firing emplaced gun with rocket launcher equipped and then knockbacked off the emplaced gun
	if (ent && client && ent->health > 0 && client->ps.pm_type != PM_DEAD) {
		if (client->ps.emplacedIndex) {
			// we are currently using emplaced; note this for the next think
			client->usingEmplaced = qtrue;
			client->forcingEmplacedNoAttack = qfalse;
		}
		else {
			if (client->usingEmplaced && (ucmd->buttons & BUTTON_ATTACK || ucmd->buttons & BUTTON_ALT_ATTACK)) {
				// we were using emplaced on the last think, are no longer using it, and are still holding down mouse buttons
				// cancel their mouse button, and continue doing this on subsequent thinks until the mouse button is actually physically released
				client->usingEmplaced = qfalse;
				client->forcingEmplacedNoAttack = qtrue;
				ucmd->buttons &= ~(BUTTON_ATTACK | BUTTON_ALT_ATTACK);
			}
			else if (client->forcingEmplacedNoAttack) {
				if (!(ucmd->buttons & BUTTON_ATTACK) && !(ucmd->buttons & BUTTON_ALT_ATTACK))
					client->forcingEmplacedNoAttack = qfalse; // they actually physically released mouse button; stop canceling attack
				else
					ucmd->buttons &= ~(BUTTON_ATTACK | BUTTON_ALT_ATTACK); // they have not yet released; cancel the attack
			}
			client->usingEmplaced = qfalse;
		}
	}
	else if (client) { // sanity check to clear these when dead
		client->usingEmplaced = qfalse;
		client->forcingEmplacedNoAttack = qfalse;
	}

    //OSP: pause
    if ( level.pause.state != PAUSE_NONE ) {
        ucmd->buttons = 0;
        ucmd->forwardmove = 0;
        ucmd->rightmove = 0;
        ucmd->upmove = 0;
        ucmd->generic_cmd = 0;
        client->ps.pm_type = PM_FREEZE;
    } else if ( client->noclip ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.eFlags & EF_DISINTEGRATION ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
		client->ps.pm_type = PM_DEAD;
	} else {
		if (client->ps.forceGripChangeMovetype)
		{
			client->ps.pm_type = client->ps.forceGripChangeMovetype;
		}
		else
		{
			if (client->jetPackOn)
			{
				client->ps.pm_type = PM_JETPACK;
				client->ps.eFlags |= EF_JETPACK_ACTIVE;
				killJetFlags = qfalse;
			}
			else
			{
				client->ps.pm_type = PM_NORMAL;
			}
		}
	}

	if (killJetFlags)
	{
		client->ps.eFlags &= ~EF_JETPACK_ACTIVE;
		client->ps.eFlags &= ~EF_JETPACK_FLAMING;
	}

	vmCvar_t	mapname;
	trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);

	if (level.zombies && ent->client->jetPackOn) {
		if (level.siegeMap == SIEGEMAP_CARGO) {
			if (ent->client->ps.origin[0] >= 1838 && ent->client->ps.origin[0] <= 3261 && ent->client->ps.origin[1] >= 1719 && ent->client->ps.origin[1] <= 3422)
				ent->client->jetPackOn = qfalse;
		}
	}

#define	SLOWDOWN_DIST	128.0f
#define	MIN_NPC_SPEED	16.0f

	if (client->bodyGrabIndex != ENTITYNUM_NONE)
	{
		gentity_t *grabbed = &g_entities[client->bodyGrabIndex];

		if (!grabbed->inuse || grabbed->s.eType != ET_BODY ||
			(grabbed->s.eFlags & EF_DISINTEGRATION) ||
			(grabbed->s.eFlags & EF_NODRAW))
		{
			if (grabbed->inuse && grabbed->s.eType == ET_BODY)
			{
				grabbed->s.ragAttach = 0;
			}
			client->bodyGrabIndex = ENTITYNUM_NONE;
		}
		else
		{
			mdxaBone_t rhMat;
			vec3_t rhOrg, tAng;
			vec3_t bodyDir;
			float bodyDist;

			ent->client->ps.forceHandExtend = HANDEXTEND_DRAGGING;

			if (ent->client->ps.forceHandExtendTime < level.time + 500)
			{
				ent->client->ps.forceHandExtendTime = level.time + 1000;
			}

			VectorSet(tAng, 0, ent->client->ps.viewangles[YAW], 0);
			trap_G2API_GetBoltMatrix(ent->ghoul2, 0, 0, &rhMat, tAng, ent->client->ps.origin, level.time,
				NULL, ent->modelScale); //0 is always going to be right hand bolt
			BG_GiveMeVectorFromMatrix(&rhMat, ORIGIN, rhOrg);

			VectorSubtract(rhOrg, grabbed->r.currentOrigin, bodyDir);
			bodyDist = VectorLength(bodyDir);

			if (bodyDist > 40.0f)
			{ //can no longer reach
				grabbed->s.ragAttach = 0;
				client->bodyGrabIndex = ENTITYNUM_NONE;
			}
			else if (bodyDist > 24.0f)
			{
				bodyDir[2] = 0; //don't want it floating
				VectorAdd(grabbed->epVelocity, bodyDir, grabbed->epVelocity);
				G_Sound(grabbed, CHAN_AUTO, G_SoundIndex("sound/player/roll1.wav"));
			}
		}
	}
	else if (ent->client->ps.forceHandExtend == HANDEXTEND_DRAGGING)
	{
		ent->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
	}

	if (ent->NPC && ent->s.NPC_class != CLASS_VEHICLE) //vehicles manage their own speed
	{
		//FIXME: swoop should keep turning (and moving forward?) for a little bit?
		if ( ent->NPC->combatMove == qfalse )
		{
			//if ( !(ucmd->buttons & BUTTON_USE) )
			if (1)
			{//Not leaning
				qboolean Flying = (ucmd->upmove && (ent->client->ps.eFlags2&EF2_FLYING));
				qboolean Climbing = (ucmd->upmove && ent->watertype&CONTENTS_LADDER );

				if ( ucmd->forwardmove || ucmd->rightmove || Flying )
				{
					{//In - Formation NPCs set thier desiredSpeed themselves
						if ( ucmd->buttons & BUTTON_WALKING )
						{
							ent->NPC->desiredSpeed = NPC_GetWalkSpeed( ent );
						}
						else//running
						{
							ent->NPC->desiredSpeed = NPC_GetRunSpeed( ent );
						}

						if ( ent->NPC->currentSpeed >= 80 && !controlledByPlayer )
						{//At higher speeds, need to slow down close to stuff
							//Slow down as you approach your goal
						//	if ( ent->NPC->distToGoal < SLOWDOWN_DIST && client->race != RACE_BORG && !(ent->NPC->aiFlags&NPCAI_NO_SLOWDOWN) )//128
							if ( ent->NPC->distToGoal < SLOWDOWN_DIST && !(ent->NPC->aiFlags&NPCAI_NO_SLOWDOWN) )//128
							{
								if ( ent->NPC->desiredSpeed > MIN_NPC_SPEED )
								{
									float slowdownSpeed = ((float)ent->NPC->desiredSpeed) * ent->NPC->distToGoal / SLOWDOWN_DIST;

									ent->NPC->desiredSpeed = ceil(slowdownSpeed);
									if ( ent->NPC->desiredSpeed < MIN_NPC_SPEED )
									{//don't slow down too much
										ent->NPC->desiredSpeed = MIN_NPC_SPEED;
									}
								}
							}
						}
					}
				}
				else if ( Climbing )
				{
					ent->NPC->desiredSpeed = ent->NPC->stats.walkSpeed;
				}
				else
				{//We want to stop
					ent->NPC->desiredSpeed = 0;
				}

				NPC_Accelerate( ent, qfalse, qfalse );

				if ( ent->NPC->currentSpeed <= 24 && ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
				{//No-one walks this slow
					client->ps.speed = ent->NPC->currentSpeed = 0;//Full stop
					ucmd->forwardmove = 0;
					ucmd->rightmove = 0;
				}
				else
				{
					if ( ent->NPC->currentSpeed <= ent->NPC->stats.walkSpeed )
					{//Play the walkanim
						ucmd->buttons |= BUTTON_WALKING;
					}
					else
					{
						ucmd->buttons &= ~BUTTON_WALKING;
					}

					if ( ent->NPC->currentSpeed > 0 )
					{//We should be moving
						if ( Climbing || Flying )
						{
							if ( !ucmd->upmove )
							{//We need to force them to take a couple more steps until stopped
								ucmd->upmove = ent->NPC->last_ucmd.upmove;
							}
						}
						else if ( !ucmd->forwardmove && !ucmd->rightmove )
						{//We need to force them to take a couple more steps until stopped
							ucmd->forwardmove = ent->NPC->last_ucmd.forwardmove;
							ucmd->rightmove = ent->NPC->last_ucmd.rightmove;
						}
					}

					client->ps.speed = ent->NPC->currentSpeed;
					//rwwFIXMEFIXME: do this and also check for all real client
					if (1)
					{
						//Slow down on turns - don't orbit!!!
						float turndelta = 0; 
						// if the NPC is locked into a Yaw, we want to check the lockedDesiredYaw...otherwise the NPC can't walk backwards, because it always thinks it trying to turn according to desiredYaw
						//if( client->renderInfo.renderFlags & RF_LOCKEDANGLE ) // yeah I know the RF_ flag is a pretty ugly hack...
						if (0) //rwwFIXMEFIXME: ...
						{	
							turndelta = (180 - fabs( AngleDelta( ent->r.currentAngles[YAW], ent->NPC->lockedDesiredYaw ) ))/180;
						}
						else
						{
							turndelta = (180 - fabs( AngleDelta( ent->r.currentAngles[YAW], ent->NPC->desiredYaw ) ))/180;
						}
												
						if ( turndelta < 0.75f )
						{
							client->ps.speed = 0;
						}
						else if ( ent->NPC->distToGoal < 100 && turndelta < 1.0 )
						{//Turn is greater than 45 degrees or closer than 100 to goal
							client->ps.speed = floor(((float)(client->ps.speed))*turndelta);
						}
					}
				}
			}
		}
		else
		{	
			ent->NPC->desiredSpeed = ( ucmd->buttons & BUTTON_WALKING ) ? NPC_GetWalkSpeed( ent ) : NPC_GetRunSpeed( ent );

			client->ps.speed = ent->NPC->desiredSpeed;
		}

		if (ucmd->buttons & BUTTON_WALKING)
		{ //sort of a hack I guess since MP handles walking differently from SP (has some proxy cheat prevention methods)
			if (ucmd->forwardmove > 64)
			{
				ucmd->forwardmove = 64;	
			}
			else if (ucmd->forwardmove < -64)
			{
				ucmd->forwardmove = -64;
			}
			
			if (ucmd->rightmove > 64)
			{
				ucmd->rightmove = 64;
			}
			else if ( ucmd->rightmove < -64)
			{
				ucmd->rightmove = -64;
			}

		}
		client->ps.basespeed = client->ps.speed;
	}
	else if (!client->ps.m_iVehicleNum &&
		(!ent->NPC || ent->s.NPC_class != CLASS_VEHICLE)) //if riding a vehicle it will manage our speed and such
	{
		if (client->sess.siegeDuelInProgress)
		{
			client->ps.speed = client->ps.basespeed = (g_speed.value * 1.25);
		}
		else
		{
			// set speed
			client->ps.speed = g_speed.value;
			if (client->sess.skillBoost) {
				switch (client->sess.skillBoost) {
				case 1:		client->ps.speed += (client->ps.speed * g_skillboost1_movementSpeedBonus.value);		break;
				case 2:		client->ps.speed += (client->ps.speed * g_skillboost2_movementSpeedBonus.value);		break;
				case 3:		client->ps.speed += (client->ps.speed * g_skillboost3_movementSpeedBonus.value);		break;
				case 4:		client->ps.speed += (client->ps.speed * g_skillboost4_movementSpeedBonus.value);		break;
				case 5:		client->ps.speed += (client->ps.speed * g_skillboost5_movementSpeedBonus.value);		break;
				case 6:		client->ps.speed += (client->ps.speed * g_skillboost6_movementSpeedBonus.value);		break;
				case 7:		client->ps.speed += (client->ps.speed * g_skillboost7_movementSpeedBonus.value);		break;
				case 8:		client->ps.speed += (client->ps.speed * g_skillboost8_movementSpeedBonus.value);		break;
				case 9:		client->ps.speed += (client->ps.speed * g_skillboost9_movementSpeedBonus.value);		break;
				case 10:		client->ps.speed += (client->ps.speed * g_skillboost10_movementSpeedBonus.value);		break;
				}
			}

			//Check for a siege class speed multiplier
			if (g_gametype.integer == GT_SIEGE &&
				client->siegeClass != -1)
			{
				client->ps.speed *= bgSiegeClasses[client->siegeClass].speed;
			}

			if (client->bodyGrabIndex != ENTITYNUM_NONE)
			{ //can't go nearly as fast when dragging a body around
				client->ps.speed *= 0.2f;
			}

			client->ps.basespeed = client->ps.speed;

			if (client->holdingObjectiveItem && client->sess.sessionTeam == TEAM_RED && g_entities[client->holdingObjectiveItem].speedMultiplier && g_entities[client->holdingObjectiveItem].speedMultiplier != 1)
			{
				//speed multiplier for siege items (team red)
				client->ps.speed *= g_entities[client->holdingObjectiveItem].speedMultiplier;
				client->ps.basespeed = client->ps.speed;
			}
			else if (client->holdingObjectiveItem && client->sess.sessionTeam == TEAM_BLUE && g_entities[client->holdingObjectiveItem].speedMultiplierTeam2 &&
				g_entities[client->holdingObjectiveItem].speedMultiplierTeam2 != 1)
			{
				//speed multiplier for siege items (team blue)
				client->ps.speed *= g_entities[client->holdingObjectiveItem].speedMultiplierTeam2;
				client->ps.basespeed = client->ps.speed;
			}
		}
	}

	if ( !ent->NPC || !(ent->NPC->aiFlags&NPCAI_CUSTOM_GRAVITY) )
	{//use global gravity
		if (ent->NPC && ent->s.NPC_class == CLASS_VEHICLE &&
			ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->gravity)
		{ //use custom veh gravity
			client->ps.gravity = ent->m_pVehicle->m_pVehicleInfo->gravity;
		}
		else
		{
			if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE)
			{ //in space, so no gravity...
				client->ps.gravity = 1.0f;
				if (ent->s.number < MAX_CLIENTS)
				{
					VectorScale(client->ps.velocity, 0.8f, client->ps.velocity);
				}
			}
			else
			{
				if (client->ps.eFlags2 & EF2_SHIP_DEATH)
				{ //float there
					VectorClear(client->ps.velocity);
					client->ps.gravity = 1.0f;
				}
				else
				{
					client->ps.gravity = g_gravity.value;
				}
			}
		}
	}

	if (g_antiLaming.integer && ent->client && !ent->NPC && ent->client->sess.sessionTeam == TEAM_RED && g_gametype.integer == GT_SIEGE)
	{
		qboolean	isLaming = qfalse;
		int lamingMethod = 0;
		if (ent->client->holdingObjectiveItem > 0)
		{
			if (!Q_stricmpn(mapname.string, "mp/siege_hoth", 13))
			{
				if (ent->client->ps.origin[0] >= 300 && ent->client->ps.origin[1] <= 1132)
				{
					//laming the codes
					isLaming = qtrue;
				}
			}
			else if (!Q_stricmp(mapname.string, "mp/siege_korriban"))
			{
				if (ent->client->ps.origin[1] <= 1945)
				{
					//laming the crystals
					isLaming = qtrue;
				}
				else if (level.totalObjectivesCompleted && level.totalObjectivesCompleted >= 4 && ent->client->ps.origin[1] <= 5650)
				{
					//laming the scepter
					isLaming = qtrue;
				}
			}
			else if (!Q_stricmp(mapname.string, "mp/siege_desert"))
			{
				if (ent->client->ps.origin[0] >= -3700)
				{
					//laming the parts
					isLaming = qtrue;
				}
			}

			if (isLaming)
			{
				lamingMethod = LAMING_METHOD_ITEM;
			}
		}
		else
		{
			if (!Q_stricmpn(mapname.string, "mp/siege_hoth", 13))
			{
				if (level.totalObjectivesCompleted <= 2 && ent->client->ps.origin[0] >= -691 && ent->client->ps.origin[0] < 157 &&
					ent->client->ps.origin[1] >= 1592 && ent->client->ps.origin[1] <= 2457 && ent->client->ps.origin[2] >= 300)
				{
					//got to codes area before gen was destroyed
					isLaming = qtrue;
				}
				else if (level.totalObjectivesCompleted <= 3 && ent->client->ps.origin[0] >= -2712 && ent->client->ps.origin[0] < -1027 &&
					ent->client->ps.origin[1] >= -1052 && ent->client->ps.origin[1] <= 569 && ent->client->ps.origin[2] >= -212)
				{
					//got to hangar before codes were delivered
					isLaming = qtrue;
				}
			}
			else if (!Q_stricmp(mapname.string, "mp/siege_desert"))
			{
				/*if (level.totalObjectivesCompleted <= 2 && ent->client->ps.origin[0] >= -3470 && ent->client->ps.origin[0] <= -3336 &&
					ent->client->ps.origin[1] >= -1124 && ent->client->ps.origin[1] <= -477 && ent->client->ps.origin[2] >= 325)
				{
					//butlered through D spawn doors at arena before arena obj has been completed
					isLaming = qtrue;
				}*/
				//removed this because the doors don't even open at that point
				//and butlering through them after completing the arena is debatable as to whether or not it's actually laming
				if (!level.totalObjectivesCompleted && ent->client->ps.origin[0] >= 6200 && ent->client->ps.origin[0] <= 6506 && ent->client->ps.origin[1] >= 988 &&
					ent->client->ps.origin[1] <= 1150 && ent->client->ps.origin[2] >= -215 && ent->client->ps.origin[2] <= 127)
				{
					//jumped through the hole in the wall
					isLaming = qtrue;
				}
			}

			if (isLaming)
			{
				lamingMethod = LAMING_METHOD_SKIP;
			}
		}

		if (isLaming && (!level.antiLamingTime || level.antiLamingTime <= level.time))
		{
			//we are a lamer confirmed, now let's check the cvar to determine our punishment.
			G_LogPrintf("Client %i (%s) from %s %s for %s.", ent->s.number, ent->client->pers.netname, ent->client->sess.ipString, g_antiLaming.integer == 1 ? "killed" : "kicked", lamingMethod == LAMING_METHOD_ITEM ? "laming an item" : "objective skipping");
			trap_SendConsoleCommand(EXEC_APPEND, va("svsay %s^7 was automatically %s for %s.\n", ent->client->pers.netname, g_antiLaming.integer == 1 ? "killed" : "kicked", lamingMethod == LAMING_METHOD_ITEM ? "laming an item" : "objective skipping"));

			if (g_antiLaming.integer == 1)
			{
				//soft punishment, kill them
				G_Damage(ent, ent, ent, NULL, ent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
			}
			else
			{
				//hard punishment, kick them
				trap_SendConsoleCommand(EXEC_APPEND, va("clientkick %i\n", ent->s.number));
			}

			level.antiLamingTime = level.time + 5000; //give 5 seconds for people to pick up the item without being punished.
			//also ensures that we don't spam the server if the troll manages to somehow trip the laming detection continuously.

		}
	}

	if (ent->client->sess.siegeDuelInProgress)
	{
		gentity_t *duelAgainst = &g_entities[ent->client->sess.siegeDuelIndex];

		if (ent->client->sess.sessionTeam != TEAM_RED && ent->client->sess.sessionTeam != TEAM_BLUE)
		{
			ent->client->sess.siegeDuelInProgress = 0;
		}

		if (ent->client->sess.siegeDuelTime < level.time)
		{
			//pull out the pistols
			client->ps.stats[STAT_WEAPONS] = (1 << WP_BRYAR_PISTOL);
			client->ps.weapon = WP_BRYAR_PISTOL;
			duelAgainst->client->ps.stats[STAT_WEAPONS] = (1 << WP_BRYAR_PISTOL);
			duelAgainst->client->ps.weapon = WP_BRYAR_PISTOL;
			if (ent->client->sess.siegeDuelTime > 0 || duelAgainst->client->sess.siegeDuelTime > 0)
			{
				ent->client->sess.siegeDuelTime = 0;
				duelAgainst->client->sess.siegeDuelTime = 0;
				G_LocalSound(ent, CHAN_ANNOUNCER, G_SoundIndex("sound/chars/protocol/misc/40mom038.wav")); //simulate clientside announcer voice "you may begin"
				G_LocalSound(duelAgainst, CHAN_ANNOUNCER, G_SoundIndex("sound/chars/protocol/misc/40mom038.wav")); //simulate clientside announcer voice "you may begin"
				trap_SendServerCommand(ent - g_entities, va("cp \"Fight!\n\""));
				trap_SendServerCommand(duelAgainst - g_entities, va("cp \"Fight!\n\""));
			}
		}


		if (!duelAgainst || !duelAgainst->client || !duelAgainst->inuse ||
			duelAgainst->client->sess.siegeDuelIndex != ent->s.number ||
			(duelAgainst && duelAgainst->client && duelAgainst->client->sess.sessionTeam != TEAM_RED && duelAgainst->client->sess.sessionTeam != TEAM_BLUE))
		{
			//duelAgainst guy disconnected or something
			ent->client->sess.siegeDuelInProgress = 0;
			duelAgainst->client->sess.siegeDuelInProgress = 0;

			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
			{
				ent->client->ps.fd.forcePower = 100;
				//reset force powers
				if (ent->client->siegeClass != -1)
				{
					for (forcePowerNum = 0; forcePowerNum < NUM_FORCE_POWERS; forcePowerNum++)
					{
						ent->client->ps.fd.forcePowerLevel[forcePowerNum] = bgSiegeClasses[ent->client->siegeClass].forcePowerLevels[forcePowerNum];
						if (!ent->client->ps.fd.forcePowerLevel[forcePowerNum])
						{
							ent->client->ps.fd.forcePowersKnown &= ~(1 << forcePowerNum);
						}
						else
						{
							ent->client->ps.fd.forcePowersKnown |= (1 << forcePowerNum);
						}
					}
				}
				if (ent->client->siegeClass != -1)
				{
					//valid siege class
					ent->client->ps.stats[STAT_HEALTH] = ent->health = bgSiegeClasses[ent->client->siegeClass].maxhealth;
					ent->client->ps.stats[STAT_ARMOR] = bgSiegeClasses[ent->client->siegeClass].startarmor;
				}
				else
				{
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 100;
					ent->client->ps.stats[STAT_ARMOR] = 0;
				}

				if (g_spawnInvulnerability.integer)
				{
					ent->client->ps.eFlags |= EF_INVULNERABLE;
					ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
				}

				//add back weapons
				ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;

				if (ent->client->siegeClass != -1)
				{
					//valid siege class
					ent->client->ps.speed = g_speed.value;
					ent->client->ps.speed *= bgSiegeClasses[client->siegeClass].speed;
					if (ent->client->sess.skillBoost) {
						switch (client->sess.skillBoost) {
						case 1:		client->ps.speed += (client->ps.speed * g_skillboost1_movementSpeedBonus.value);		break;
						case 2:		client->ps.speed += (client->ps.speed * g_skillboost2_movementSpeedBonus.value);		break;
						case 3:		client->ps.speed += (client->ps.speed * g_skillboost3_movementSpeedBonus.value);		break;
						case 4:		client->ps.speed += (client->ps.speed * g_skillboost4_movementSpeedBonus.value);		break;
						case 5:		client->ps.speed += (client->ps.speed * g_skillboost5_movementSpeedBonus.value);		break;
						case 6:		client->ps.speed += (client->ps.speed * g_skillboost6_movementSpeedBonus.value);		break;
						case 7:		client->ps.speed += (client->ps.speed * g_skillboost7_movementSpeedBonus.value);		break;
						case 8:		client->ps.speed += (client->ps.speed * g_skillboost8_movementSpeedBonus.value);		break;
						case 9:		client->ps.speed += (client->ps.speed * g_skillboost9_movementSpeedBonus.value);		break;
						case 10:		client->ps.speed += (client->ps.speed * g_skillboost10_movementSpeedBonus.value);		break;
						}
					}
					ent->client->ps.basespeed = client->ps.speed;
				}
				else
				{
					ent->client->ps.speed = g_speed.value;
					if (ent->client->sess.skillBoost) {
						switch (client->sess.skillBoost) {
						case 1:		client->ps.speed += (client->ps.speed * g_skillboost1_movementSpeedBonus.value);		break;
						case 2:		client->ps.speed += (client->ps.speed * g_skillboost2_movementSpeedBonus.value);		break;
						case 3:		client->ps.speed += (client->ps.speed * g_skillboost3_movementSpeedBonus.value);		break;
						case 4:		client->ps.speed += (client->ps.speed * g_skillboost4_movementSpeedBonus.value);		break;
						case 5:		client->ps.speed += (client->ps.speed * g_skillboost5_movementSpeedBonus.value);		break;
						case 6:		client->ps.speed += (client->ps.speed * g_skillboost6_movementSpeedBonus.value);		break;
						case 7:		client->ps.speed += (client->ps.speed * g_skillboost7_movementSpeedBonus.value);		break;
						case 8:		client->ps.speed += (client->ps.speed * g_skillboost8_movementSpeedBonus.value);		break;
						case 9:		client->ps.speed += (client->ps.speed * g_skillboost9_movementSpeedBonus.value);		break;
						case 10:		client->ps.speed += (client->ps.speed * g_skillboost10_movementSpeedBonus.value);		break;
						}
					}
					ent->client->ps.basespeed = client->ps.speed;
				}
			}
		}
		else if (duelAgainst->health < 1 || duelAgainst->client->ps.stats[STAT_HEALTH] < 1 || duelAgainst->client->tempSpectate >= level.time)
		{
			ent->client->sess.siegeDuelInProgress = 0;
			duelAgainst->client->sess.siegeDuelInProgress = 0;

			//Winner gets full health.. providing he's still alive
			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
			{
				ent->client->ps.fd.forcePower = 100;
				//reset force powers
				if (ent->client->siegeClass != -1)
				{
					for (forcePowerNum = 0; forcePowerNum < NUM_FORCE_POWERS; forcePowerNum++)
					{
						ent->client->ps.fd.forcePowerLevel[forcePowerNum] = bgSiegeClasses[ent->client->siegeClass].forcePowerLevels[forcePowerNum];
						if (!ent->client->ps.fd.forcePowerLevel[forcePowerNum])
						{
							ent->client->ps.fd.forcePowersKnown &= ~(1 << forcePowerNum);
						}
						else
						{
							ent->client->ps.fd.forcePowersKnown |= (1 << forcePowerNum);
						}
					}
				}
				if (ent->client->siegeClass != -1)
				{
					//valid siege class
					ent->client->ps.stats[STAT_HEALTH] = ent->health = bgSiegeClasses[ent->client->siegeClass].maxhealth;
					ent->client->ps.stats[STAT_ARMOR] = bgSiegeClasses[ent->client->siegeClass].startarmor;
				}
				else
				{
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 100;
					ent->client->ps.stats[STAT_ARMOR] = 0;
				}

				if (g_spawnInvulnerability.integer)
				{
					ent->client->ps.eFlags |= EF_INVULNERABLE;
					ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
				}

				//add back weapons
				ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;
				 
				if (ent->client->siegeClass != -1)
				{
					//valid siege class
					ent->client->ps.speed = g_speed.value;
					ent->client->ps.speed *= bgSiegeClasses[client->siegeClass].speed;
					if (ent->client->sess.skillBoost) {
						switch (client->sess.skillBoost) {
						case 1:		client->ps.speed += (client->ps.speed * g_skillboost1_movementSpeedBonus.value);		break;
						case 2:		client->ps.speed += (client->ps.speed * g_skillboost2_movementSpeedBonus.value);		break;
						case 3:		client->ps.speed += (client->ps.speed * g_skillboost3_movementSpeedBonus.value);		break;
						case 4:		client->ps.speed += (client->ps.speed * g_skillboost4_movementSpeedBonus.value);		break;
						case 5:		client->ps.speed += (client->ps.speed * g_skillboost5_movementSpeedBonus.value);		break;
						case 6:		client->ps.speed += (client->ps.speed * g_skillboost6_movementSpeedBonus.value);		break;
						case 7:		client->ps.speed += (client->ps.speed * g_skillboost7_movementSpeedBonus.value);		break;
						case 8:		client->ps.speed += (client->ps.speed * g_skillboost8_movementSpeedBonus.value);		break;
						case 9:		client->ps.speed += (client->ps.speed * g_skillboost9_movementSpeedBonus.value);		break;
						case 10:		client->ps.speed += (client->ps.speed * g_skillboost10_movementSpeedBonus.value);		break;
						}
					}
					ent->client->ps.basespeed = client->ps.speed;
				}
				else
				{
					ent->client->ps.speed = g_speed.value;
					if (ent->client->sess.skillBoost) {
						switch (client->sess.skillBoost) {
						case 1:		client->ps.speed += (client->ps.speed * g_skillboost1_movementSpeedBonus.value);		break;
						case 2:		client->ps.speed += (client->ps.speed * g_skillboost2_movementSpeedBonus.value);		break;
						case 3:		client->ps.speed += (client->ps.speed * g_skillboost3_movementSpeedBonus.value);		break;
						case 4:		client->ps.speed += (client->ps.speed * g_skillboost4_movementSpeedBonus.value);		break;
						case 5:		client->ps.speed += (client->ps.speed * g_skillboost5_movementSpeedBonus.value);		break;
						case 6:		client->ps.speed += (client->ps.speed * g_skillboost6_movementSpeedBonus.value);		break;
						case 7:		client->ps.speed += (client->ps.speed * g_skillboost7_movementSpeedBonus.value);		break;
						case 8:		client->ps.speed += (client->ps.speed * g_skillboost8_movementSpeedBonus.value);		break;
						case 9:		client->ps.speed += (client->ps.speed * g_skillboost9_movementSpeedBonus.value);		break;
						case 10:		client->ps.speed += (client->ps.speed * g_skillboost10_movementSpeedBonus.value);		break;
						}
					}
					ent->client->ps.basespeed = client->ps.speed;
				}

				//Private duel announcements are now made globally because we only want one duel at a time.
				if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
				{
					trap_SendServerCommand(-1, va("cp \"^7Duel winner: %s\n\"", ent->client->pers.netname));
					trap_SendServerCommand(-1, va("print \"^7Duel winner: %s\n\"", ent->client->pers.netname));
				}
				else
				{ //it was a draw, because we both managed to die in the same frame
					//uhh, will this actually get called? we already checked to see if ent was alive earlier...just copying basejka here
					trap_SendServerCommand(-1, va("cp \"The duel is a draw. Both players died at the exact same time. First death announced was not actually earlier!\n\""));
					trap_SendServerCommand(-1, va("print \"The duel is a draw. Both players died at the exact same time. First death announced was not actually earlier!\n\""));
				}
			}
		}


	}


	if (ent->client->ps.duelInProgress)
	{
		gentity_t *duelAgainst = &g_entities[ent->client->ps.duelIndex];

		//Keep the time updated, so once this duel ends this player can't engage in a duel for another
		//10 seconds. This will give other people a chance to engage in duels in case this player wants
		//to engage again right after he's done fighting and someone else is waiting.
		ent->client->ps.fd.privateDuelTime = level.time + 10000;

		if (ent->client->ps.duelTime < level.time)
		{
			//Bring out the sabers
			if (duelAgainst 
				&& duelAgainst->client 
				&& duelAgainst->inuse )
			{
				if (g_gametype.integer == GT_CTF){
					if (((duelAgainst->client->ps.weapon == WP_BRYAR_PISTOL)
						||(duelAgainst->client->ps.weapon == WP_SABER)
						||(duelAgainst->client->ps.weapon == WP_MELEE))
					&& duelAgainst->client->ps.duelTime){
						G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 2);
						duelAgainst->client->ps.duelTime = 0;
						duelAgainst->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
						duelAgainst->client->ps.weapon = WP_BRYAR_PISTOL ;
						duelAgainst->client->ps.weaponTime = 0;
						duelAgainst->client->ps.weaponstate = 0;
					}
				} else {
					if (duelAgainst->client->ps.weapon == WP_SABER 
						&& duelAgainst->client->ps.saberHolstered 
						&& duelAgainst->client->ps.duelTime){

						duelAgainst->client->ps.saberHolstered = 0;

						if (duelAgainst->client->saber[0].soundOn)
						{
							G_Sound(duelAgainst, CHAN_AUTO, duelAgainst->client->saber[0].soundOn);
						}
						if (duelAgainst->client->saber[1].soundOn)
						{
							G_Sound(duelAgainst, CHAN_AUTO, duelAgainst->client->saber[1].soundOn);
						}

						G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 2);

						duelAgainst->client->ps.duelTime = 0;
					}
				}
			}
		} else if (g_gametype.integer != GT_CTF) {
			client->ps.speed = 0;
			client->ps.basespeed = 0;
			ucmd->forwardmove = 0;
			ucmd->rightmove = 0;
			ucmd->upmove = 0;
		}

		if (!duelAgainst || !duelAgainst->client || !duelAgainst->inuse ||
            duelAgainst->client->ps.duelIndex != ent->s.number)
		{
			ent->client->ps.duelInProgress = 0;
			G_AddEvent(ent, EV_PRIVATE_DUEL, 0);

			//remove melee, add saber, if necessary
			if (g_gametype.integer == GT_CTF){
				ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;
			}
		}
		else if (duelAgainst->health < 1 || duelAgainst->client->ps.stats[STAT_HEALTH] < 1)
		{
			ent->client->ps.duelInProgress = 0;
			duelAgainst->client->ps.duelInProgress = 0;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
			G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 0);

			//Winner gets full health.. providing he's still alive
			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
			{
				if (ent->health < ent->client->ps.stats[STAT_MAX_HEALTH])
				{
					ent->client->ps.stats[STAT_HEALTH] = ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
					ent->client->ps.stats[STAT_ARMOR] = 25;
				}

				if (g_spawnInvulnerability.integer)
				{
					ent->client->ps.eFlags |= EF_INVULNERABLE;
					ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
				}

				//remove melee, add saber, if necessary
				if (g_gametype.integer == GT_CTF){
					ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;
				}
			}

			//Private duel announcements are now made globally because we only want one duel at a time.
			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
			{
				trap_SendServerCommand( -1, va("cp \"%s^7 %s %s!\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER"), duelAgainst->client->pers.netname) );
				trap_SendServerCommand( -1, va("print \"%s^7 %s %s!\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER"), duelAgainst->client->pers.netname) );
			}
			else
			{ //it was a draw, because we both managed to die in the same frame
				trap_SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELTIE")) );
				trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELTIE")) );
			}
		}
		else
		{
			vec3_t vSub;
			float subLen = 0;
			int duelDistLimit = (g_gametype.integer == GT_CTF ) ? 2048 : 1024;

			VectorSubtract(ent->client->ps.origin, duelAgainst->client->ps.origin, vSub);
			subLen = VectorLength(vSub);

			if (subLen >= duelDistLimit)
			{
				ent->client->ps.duelInProgress = 0;
				duelAgainst->client->ps.duelInProgress = 0;

				G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
				G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 0);

				if (g_gametype.integer == GT_CTF){
					ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;
					duelAgainst->client->ps.stats[STAT_WEAPONS] = duelAgainst->client->preduelWeaps;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
					duelAgainst->client->ps.stats[STAT_HEALTH] = duelAgainst->health = duelAgainst->client->ps.stats[STAT_MAX_HEALTH];
					ent->client->ps.stats[STAT_ARMOR] = 25;
					duelAgainst->client->ps.stats[STAT_ARMOR] = 25;
				}

				trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELSTOP")) );
			}
		}
	}

	if (ent->client->doingThrow > level.time)
	{
		gentity_t *throwee = &g_entities[ent->client->throwingIndex];

		if (!throwee->inuse || !throwee->client || throwee->health < 1 ||
			throwee->client->sess.sessionTeam == TEAM_SPECTATOR ||
			(throwee->client->ps.pm_flags & PMF_FOLLOW) ||
			throwee->client->throwingIndex != ent->s.number)
		{
			ent->client->doingThrow = 0;
			ent->client->ps.forceHandExtend = HANDEXTEND_NONE;

			if (throwee->inuse && throwee->client)
			{
				throwee->client->ps.heldByClient = 0;
				throwee->client->beingThrown = 0;

				if (throwee->client->ps.forceHandExtend != HANDEXTEND_POSTTHROWN)
				{
					throwee->client->ps.forceHandExtend = HANDEXTEND_NONE;
				}
			}
		}
	}

	if (ent->client->beingThrown > level.time)
	{
		gentity_t *thrower = &g_entities[ent->client->throwingIndex];

		if (!thrower->inuse || !thrower->client || thrower->health < 1 ||
			thrower->client->sess.sessionTeam == TEAM_SPECTATOR ||
			(thrower->client->ps.pm_flags & PMF_FOLLOW) ||
			thrower->client->throwingIndex != ent->s.number)
		{
			ent->client->ps.heldByClient = 0;
			ent->client->beingThrown = 0;

			if (ent->client->ps.forceHandExtend != HANDEXTEND_POSTTHROWN)
			{
				ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}

			if (thrower->inuse && thrower->client)
			{
				thrower->client->doingThrow = 0;
				thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
		}
		else if (thrower->inuse && thrower->client && thrower->ghoul2 &&
			trap_G2_HaveWeGhoul2Models(thrower->ghoul2))
		{
#if 0
			int lHandBolt = trap_G2API_AddBolt(thrower->ghoul2, 0, "*l_hand");
			int pelBolt = trap_G2API_AddBolt(thrower->ghoul2, 0, "pelvis");


			if (lHandBolt != -1 && pelBolt != -1)
#endif
			{
				float pDif = 40.0f;
				vec3_t boltOrg, pBoltOrg;
				vec3_t tAngles;
				vec3_t vDif;
				vec3_t entDir, otherAngles;
				vec3_t fwd, right;

				//Always look at the thrower.
				VectorSubtract( thrower->client->ps.origin, ent->client->ps.origin, entDir );
				VectorCopy( ent->client->ps.viewangles, otherAngles );
				otherAngles[YAW] = vectoyaw( entDir );
				SetClientViewAngle( ent, otherAngles );

				VectorCopy(thrower->client->ps.viewangles, tAngles);
				tAngles[PITCH] = tAngles[ROLL] = 0;

				//Get the direction between the pelvis and position of the hand
#if 0
				mdxaBone_t boltMatrix, pBoltMatrix;

				trap_G2API_GetBoltMatrix(thrower->ghoul2, 0, lHandBolt, &boltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				boltOrg[0] = boltMatrix.matrix[0][3];
				boltOrg[1] = boltMatrix.matrix[1][3];
				boltOrg[2] = boltMatrix.matrix[2][3];

				trap_G2API_GetBoltMatrix(thrower->ghoul2, 0, pelBolt, &pBoltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				pBoltOrg[0] = pBoltMatrix.matrix[0][3];
				pBoltOrg[1] = pBoltMatrix.matrix[1][3];
				pBoltOrg[2] = pBoltMatrix.matrix[2][3];
#else //above tends to not work once in a while, for various reasons I suppose.
				VectorCopy(thrower->client->ps.origin, pBoltOrg);
				AngleVectors(tAngles, fwd, right, 0);
				boltOrg[0] = pBoltOrg[0] + fwd[0]*8 + right[0]*pDif;
				boltOrg[1] = pBoltOrg[1] + fwd[1]*8 + right[1]*pDif;
				boltOrg[2] = pBoltOrg[2];
#endif
				VectorSubtract(ent->client->ps.origin, boltOrg, vDif);
				if (VectorLength(vDif) > 32.0f && (thrower->client->doingThrow - level.time) < 4500)
				{ //the hand is too far away, and can no longer hold onto us, so escape.
					ent->client->ps.heldByClient = 0;
					ent->client->beingThrown = 0;
					thrower->client->doingThrow = 0;

					thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
					G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

					ent->client->ps.forceDodgeAnim = 2;
					ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
					ent->client->ps.forceHandExtendTime = level.time + 500;
					ent->client->ps.velocity[2] = 400;
					G_PreDefSound(ent->client->ps.origin, PDSOUND_FORCEJUMP);
				}
				else if ((client->beingThrown - level.time) < 4000)
				{ //step into the next part of the throw, and go flying back
					float vScale = 400.0f;
					ent->client->ps.forceHandExtend = HANDEXTEND_POSTTHROWN;
					ent->client->ps.forceHandExtendTime = level.time + 1200;
					ent->client->ps.forceDodgeAnim = 0;

					thrower->client->ps.forceHandExtend = HANDEXTEND_POSTTHROW;
					thrower->client->ps.forceHandExtendTime = level.time + 200;

					ent->client->ps.heldByClient = 0;

					ent->client->ps.heldByClient = 0;
					ent->client->beingThrown = 0;
					thrower->client->doingThrow = 0;

					AngleVectors(thrower->client->ps.viewangles, vDif, 0, 0);
					ent->client->ps.velocity[0] = vDif[0]*vScale;
					ent->client->ps.velocity[1] = vDif[1]*vScale;
					ent->client->ps.velocity[2] = 400;

					G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain100.wav") );
					G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*jump1.wav") );

					//Set the thrower as the "other killer", so if we die from fall/impact damage he is credited.
					ent->client->ps.otherKiller = thrower->s.number;
					ent->client->ps.otherKillerTime = level.time + 8000;
					ent->client->ps.otherKillerDebounceTime = level.time + 100;
				}
				else
				{ //see if we can move to be next to the hand.. if it's not clear, break the throw.
					vec3_t intendedOrigin;
					trace_t tr;
					trace_t tr2;

					VectorSubtract(boltOrg, pBoltOrg, vDif);
					VectorNormalize(vDif);

					VectorClear(ent->client->ps.velocity);
					intendedOrigin[0] = pBoltOrg[0] + vDif[0]*pDif;
					intendedOrigin[1] = pBoltOrg[1] + vDif[1]*pDif;
					intendedOrigin[2] = thrower->client->ps.origin[2];

					trap_Trace(&tr, intendedOrigin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, ent->clipmask);
					trap_Trace(&tr2, ent->client->ps.origin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, CONTENTS_SOLID);

					if (tr.fraction == 1.0 && !tr.startsolid && tr2.fraction == 1.0 && !tr2.startsolid)
					{
						VectorCopy(intendedOrigin, ent->client->ps.origin);
						
						if ((client->beingThrown - level.time) < 4800)
						{
							ent->client->ps.heldByClient = thrower->s.number+1;
						}
					}
					else
					{ //if the guy can't be put here then it's time to break the throw off.
						ent->client->ps.heldByClient = 0;
						ent->client->beingThrown = 0;
						thrower->client->doingThrow = 0;

						thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
						G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

						ent->client->ps.forceDodgeAnim = 2;
						ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
						ent->client->ps.forceHandExtendTime = level.time + 500;
						ent->client->ps.velocity[2] = 400;
						G_PreDefSound(ent->client->ps.origin, PDSOUND_FORCEJUMP);
					}
				}
			}
		}
	}
	else if (ent->client->ps.heldByClient)
	{
		ent->client->ps.heldByClient = 0;
	}

	// set up for pmove
	oldEventSequence = client->ps.eventSequence;

	memset (&pm, 0, sizeof(pm));

	if ( ent->flags & FL_FORCE_GESTURE ) {
		ent->flags &= ~FL_FORCE_GESTURE;
		ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
	}


	if (ent->client && ent->client->ps.fallingToDeath &&
		(level.time - FALL_FADE_TIME) > ent->client->ps.fallingToDeath)
	{ //die!
		if (ent->health > 0)
		{
			gentity_t *otherKiller = ent;
			if (ent->client->ps.otherKillerTime > level.time &&
				ent->client->ps.otherKiller != ENTITYNUM_NONE)
			{
				otherKiller = &g_entities[ent->client->ps.otherKiller];

				if (!otherKiller->inuse)
				{
					otherKiller = ent;
				}
			}
			G_Damage(ent, otherKiller, otherKiller, NULL, ent->client->ps.origin, 9999, DAMAGE_NO_PROTECTION, MOD_FALLING);

			G_MuteSound(ent->s.number, CHAN_VOICE); //stop screaming, because you are dead!
		}
	}

	if (ent->client->ps.otherKillerTime > level.time &&
		ent->client->ps.groundEntityNum != ENTITYNUM_NONE &&
		ent->client->ps.otherKillerDebounceTime < level.time)
	{
		ent->client->ps.otherKillerTime = 0;
		ent->client->ps.otherKiller = ENTITYNUM_NONE;
	}
	else if (ent->client->ps.otherKillerTime > level.time &&
		ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		if (ent->client->ps.otherKillerDebounceTime < (level.time + 100))
		{
			ent->client->ps.otherKillerDebounceTime = level.time + 100;
		}
	}

	//NOTE: can't put USE here *before* PMove!!
	if ( ent->client->ps.useDelay > level.time 
		&& ent->client->ps.m_iVehicleNum )
	{//when in a vehicle, debounce the use...
		ucmd->buttons &= ~BUTTON_USE;
	}

	//FIXME: need to do this before check to avoid walls and cliffs (or just cliffs?)
	G_AddPushVecToUcmd( ent, ucmd );

	//play/stop any looping sounds tied to controlled movement
	G_CheckMovingLoopingSounds( ent, ucmd );

	pm.ps = &client->ps;
	pm.cmd = *ucmd;
	if ( pm.ps->pm_type == PM_DEAD ) {
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	}
	else if ( ent->r.svFlags & SVF_BOT ) {
		pm.tracemask = MASK_PLAYERSOLID | CONTENTS_MONSTERCLIP;
	}
	else {
		pm.tracemask = MASK_PLAYERSOLID;
	}
	pm.trace = trap_Trace;
	pm.pointcontents = trap_PointContents;
	pm.debugLevel = g_debugMove.integer;
	pm.noFootsteps = ( g_dmflags.integer & DF_NO_FOOTSTEPS ) > 0;

	pm.pmove_fixed = pmove_fixed.integer | client->pers.pmoveFixed;
	pm.pmove_msec = pmove_msec.integer;
	pm.pmove_float = pmove_float.integer;

	pm.animations = bgAllAnims[ent->localAnimIndex].anims;

	//rww - bgghoul2
	pm.ghoul2 = NULL;

#if 0//#ifdef _DEBUG
	if (g_disableServerG2.integer)
	{

	}
	else
#endif
	if (ent->ghoul2)
	{
		if (ent->localAnimIndex > 1)
		{ //if it isn't humanoid then we will be having none of this.
			pm.ghoul2 = NULL;
		}
		else
		{
			pm.ghoul2 = ent->ghoul2;
			pm.g2Bolts_LFoot = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
			pm.g2Bolts_RFoot = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");
		}
	}

	//point the saber data to the right place
#if 0
	k = 0;
	while (k < MAX_SABERS)
	{
		if (ent->client->saber[k].model[0])
		{
			pm.saber[k] = &ent->client->saber[k];
		}
		else
		{
			pm.saber[k] = NULL;
		}
		k++;
	}
#endif

	//I'll just do this every frame in case the scale changes in realtime (don't need to update the g2 inst for that)
	VectorCopy(ent->modelScale, pm.modelScale);
	//rww end bgghoul2

	pm.gametype = g_gametype.integer;
	pm.debugMelee = g_debugMelee.integer;
	pm.stepSlideFix = g_stepSlideFix.integer;

	pm.noSpecMove = g_noSpecMove.integer;

	pm.nonHumanoid = (ent->localAnimIndex > 0);

	VectorCopy( client->ps.origin, client->oldOrigin );

	//Set up bg entity data
	pm.baseEnt = (bgEntity_t *)g_entities;
	pm.entSize = sizeof(gentity_t);

	if (ent->client->ps.saberLockTime > level.time)
	{
		gentity_t *blockOpp = &g_entities[ent->client->ps.saberLockEnemy];

		if (blockOpp && blockOpp->inuse && blockOpp->client)
		{
			vec3_t lockDir, lockAng;

			VectorSubtract( blockOpp->r.currentOrigin, ent->r.currentOrigin, lockDir );
			vectoangles(lockDir, lockAng);
			SetClientViewAngle( ent, lockAng );
		}

		if ( ent->client->ps.saberLockHitCheckTime < level.time )
		{//have moved to next frame since last lock push
			ent->client->ps.saberLockHitCheckTime = level.time;//so we don't push more than once per server frame
			if ( ( ent->client->buttons & BUTTON_ATTACK ) && ! ( ent->client->oldbuttons & BUTTON_ATTACK ) )
			{
				if ( ent->client->ps.saberLockHitIncrementTime < level.time )
				{//have moved to next frame since last saberlock attack button press
					int lockHits = 0;
					ent->client->ps.saberLockHitIncrementTime = level.time;//so we don't register an attack key press more than once per server frame
					//NOTE: FP_SABER_OFFENSE level already taken into account in PM_SaberLocked
					if ( (ent->client->ps.fd.forcePowersActive&(1<<FP_RAGE)) )
					{//raging: push harder
						lockHits = 1+ent->client->ps.fd.forcePowerLevel[FP_RAGE];
					}
					else
					{//normal attack
						switch ( ent->client->ps.fd.saberAnimLevel )
						{
						case SS_FAST:
							lockHits = 1;
							break;
						case SS_MEDIUM:
						case SS_TAVION:
						case SS_DUAL:
						case SS_STAFF:
							lockHits = 2;
							break;
						case SS_STRONG:
						case SS_DESANN:
							lockHits = 3;
							break;
						}
					}
					if ( ent->client->ps.fd.forceRageRecoveryTime > level.time 
						&& Q_irand( 0, 1 ) )
					{//finished raging: weak
						lockHits -= 1;
					}
					lockHits += ent->client->saber[0].lockBonus;
					if ( ent->client->saber[1].model
						&& ent->client->saber[1].model[0]
						&& !ent->client->ps.saberHolstered )
					{
						lockHits += ent->client->saber[1].lockBonus;
					}
					ent->client->ps.saberLockHits += lockHits;
					if ( g_saberLockRandomNess.integer )
					{
						ent->client->ps.saberLockHits += Q_irand( 0, g_saberLockRandomNess.integer );
						if ( ent->client->ps.saberLockHits < 0 )
						{
							ent->client->ps.saberLockHits = 0;
						}
					}
				}
			}
			if ( ent->client->ps.saberLockHits > 0 )
			{
				if ( !ent->client->ps.saberLockAdvance )
				{
					ent->client->ps.saberLockHits--;
				}
				ent->client->ps.saberLockAdvance = qtrue;
			}
		}
	}
	else
	{
		ent->client->ps.saberLockFrame = 0;
		//check for taunt
		if ( (pm.cmd.generic_cmd == GENCMD_ENGAGE_DUEL) && (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) )
		{//already in a duel, make it a taunt command
			pm.cmd.buttons |= BUTTON_GESTURE;
		}
	}

	if (ent->s.number >= MAX_CLIENTS)
	{
		VectorCopy(ent->r.mins, pm.mins);
		VectorCopy(ent->r.maxs, pm.maxs);
#if 1
		if (ent->s.NPC_class == CLASS_VEHICLE &&
			ent->m_pVehicle)
		{
			if (ent->m_pVehicle->m_pPilot)
			{ //vehicles want to use their last pilot ucmd I guess
				if ((level.time - ent->m_pVehicle->m_ucmd.serverTime) > 2000)
				{ //Previous owner disconnected, maybe
					ent->m_pVehicle->m_ucmd.serverTime = level.time;
					ent->client->ps.commandTime = level.time - 100;
					msec = 100;
				}

				memcpy(&pm.cmd, &ent->m_pVehicle->m_ucmd, sizeof(usercmd_t));

				//no veh can strafe
				pm.cmd.rightmove = 0;
				//no crouching or jumping!
				pm.cmd.upmove = 0;

				//NOTE: button presses were getting lost!
				assert(g_entities[ent->m_pVehicle->m_pPilot->s.number].client);
				pm.cmd.buttons = (g_entities[ent->m_pVehicle->m_pPilot->s.number].client->pers.cmd.buttons&(BUTTON_ATTACK | BUTTON_ALT_ATTACK));
			}
			if (ent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER)
			{
				if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{//ATST crushes anything underneath it
					gentity_t	*under = &g_entities[ent->client->ps.groundEntityNum];

					if (under && under->health && under->takedamage)
					{
						vec3_t	down = { 0,0,-1 };
						gentity_t	*attacker = ent;

						if (ent->m_pVehicle->m_pPilot)
						{
							attacker = &g_entities[ent->m_pVehicle->m_pPilot->s.number];
						}

						//FIXME: we'll be doing traces down from each foot, so we'll have a real impact origin
						G_Damage(under, attacker, attacker, down, under->r.currentOrigin, 100, 0, MOD_CRUSH);
					}
				}
			}
		}
#endif
	}

	Pmove (&pm);

	if ( pm.cmd.forwardmove ) {
		ent->client->usedForwardOrBackward = qtrue;
	}

	if ( pm.cmd.upmove ) {
		ent->client->jumpedOrCrouched = qtrue;
	}

	if (g_strafejump_mod.integer){
		ent->r.contents &= ~CONTENTS_BODY;
	}

	if (ent->client->solidHack)
	{
		if (ent->client->solidHack > level.time)
		{ //whee!
			ent->r.contents = 0;
		}
		else
		{
			ent->r.contents = CONTENTS_BODY;
			ent->client->solidHack = 0;
		}
	}
	
	if ( ent->NPC )
	{
		VectorCopy( ent->client->ps.viewangles, ent->r.currentAngles );
	}

	if (pm.checkDuelLoss)
	{
		if (pm.checkDuelLoss > 0 && (pm.checkDuelLoss <= MAX_CLIENTS || (pm.checkDuelLoss < (MAX_GENTITIES-1) && g_entities[pm.checkDuelLoss-1].s.eType == ET_NPC) ) )
		{
			gentity_t *clientLost = &g_entities[pm.checkDuelLoss-1];

			if (clientLost && clientLost->inuse && clientLost->client && Q_irand(0, 40) > clientLost->health)
			{
				vec3_t attDir;
				VectorSubtract(ent->client->ps.origin, clientLost->client->ps.origin, attDir);
				VectorNormalize(attDir);

				VectorClear(clientLost->client->ps.velocity);
				clientLost->client->ps.forceHandExtend = HANDEXTEND_NONE;
				clientLost->client->ps.forceHandExtendTime = 0;

				gGAvoidDismember = 1;
				G_Damage(clientLost, ent, ent, attDir, clientLost->client->ps.origin, 9999, DAMAGE_NO_PROTECTION, MOD_SABER);

				if (clientLost->health < 1)
				{
					gGAvoidDismember = 2;
					G_CheckForDismemberment(clientLost, ent, clientLost->client->ps.origin, 999, (clientLost->client->ps.legsAnim), qfalse);
				}

				gGAvoidDismember = 0;
			}
			else if (clientLost && clientLost->inuse && clientLost->client &&
				clientLost->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN && clientLost->client->ps.saberEntityNum)
			{ //if we didn't knock down it was a circle lock. So as punishment, make them lose their saber and go into a proper anim
				saberCheckKnockdown_DuelLoss(&g_entities[clientLost->client->ps.saberEntityNum], clientLost, ent);
			}
		}

		pm.checkDuelLoss = 0;
	}

	if (pm.cmd.generic_cmd &&
		(pm.cmd.generic_cmd != ent->client->lastGenCmd || ent->client->lastGenCmdTime < level.time))
	{
		ent->client->lastGenCmd = pm.cmd.generic_cmd;
		if (pm.cmd.generic_cmd != GENCMD_FORCE_THROW &&
			pm.cmd.generic_cmd != GENCMD_FORCE_PULL)
		{ //these are the only two where you wouldn't care about a delay between
			ent->client->lastGenCmdTime = level.time + 300; //default 100ms debounce between issuing the same command.
		}

		switch(pm.cmd.generic_cmd)
		{
		case 0:
			break;
		case GENCMD_SABERSWITCH:
			Cmd_ToggleSaber_f(ent);
			break;
		case GENCMD_ENGAGE_DUEL:
			if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
			{//already in a duel, made it a taunt command
			}
			else
			{
				if (g_gametype.integer == GT_SIEGE)
				{
					Cmd_SiegeDuel_f(ent);
				}
				else
				{
					Cmd_EngageDuel_f(ent);
				}
			}
			break;
		case GENCMD_FORCE_HEAL:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceHeal(ent);
			break;
		case GENCMD_FORCE_SPEED:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceSpeed(ent, 0);
			break;
		case GENCMD_FORCE_THROW:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceThrow(ent, qfalse);
			break;
		case GENCMD_FORCE_PULL:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceThrow(ent, qtrue);
			break;
		case GENCMD_FORCE_DISTRACT:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceTelepathy(ent);
			break;
		case GENCMD_FORCE_RAGE:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceRage(ent);
			break;
		case GENCMD_FORCE_PROTECT:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceProtect(ent);
			break;
		case GENCMD_FORCE_ABSORB:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceAbsorb(ent);
			break;
		case GENCMD_FORCE_HEALOTHER:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceTeamHeal(ent);
			break;
		case GENCMD_FORCE_FORCEPOWEROTHER:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceTeamForceReplenish(ent);
			break;
		case GENCMD_FORCE_SEEING:
			if (ent && ent->client && ent->client->emoted)
				break;
			ForceSeeing(ent);
			break;
		case GENCMD_USE_SEEKER:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)) &&
				G_ItemUsable(&ent->client->ps, HI_SEEKER) )
			{
				ItemUse_Seeker(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_SEEKER, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER);
			}
			break;
		case GENCMD_USE_FIELD:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)) &&
				G_ItemUsable(&ent->client->ps, HI_SHIELD) )
			{
				if (g_gametype.integer == GT_SIEGE && !iLikeToShieldSpam.integer && !G_ShieldSpamAllowed(ent - g_entities) && (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE) && !level.canShield[ent->client->sess.sessionTeam]) {
					trap_SendServerCommand(ent - g_entities, "print \"You are not allowed to place a shield at the moment.\n\"");
				}
				else {
					ItemUse_Shield(ent);
					G_AddEvent(ent, EV_USE_ITEM0 + HI_SHIELD, 0);
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SHIELD);
					if (g_gametype.integer == GT_SIEGE && (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE)) {
						if (level.canShield[ent->client->sess.sessionTeam] == CANSHIELD_YES)
							level.canShield[ent->client->sess.sessionTeam] = CANSHIELD_NO;
						else if (level.canShield[ent->client->sess.sessionTeam] == CANSHIELD_YO_NOTPLACED)
							level.canShield[ent->client->sess.sessionTeam] = CANSHIELD_YO_PLACED;
					}
				}
			}
			break;
		case GENCMD_USE_BACTA:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) &&
				G_ItemUsable(&ent->client->ps, HI_MEDPAC) )
			{
				ItemUse_MedPack(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_MEDPAC, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC);
			}
			break;
		case GENCMD_USE_BACTABIG:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) &&
				G_ItemUsable(&ent->client->ps, HI_MEDPAC_BIG) )
			{
				ItemUse_MedPack_Big(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_MEDPAC_BIG, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC_BIG);
			}
			break;
		case GENCMD_USE_ELECTROBINOCULARS:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) &&
				G_ItemUsable(&ent->client->ps, HI_BINOCULARS) )
			{
				ItemUse_Binoculars(ent);
				if (ent->client->ps.zoomMode == 0)
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 1);
				}
				else
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 2);
				}
			}
			break;
		case GENCMD_ZOOM:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) &&
				G_ItemUsable(&ent->client->ps, HI_BINOCULARS) )
			{
				ItemUse_Binoculars(ent);
				if (ent->client->ps.zoomMode == 0)
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 1);
				}
				else
				{
					G_AddEvent(ent, EV_USE_ITEM0+HI_BINOCULARS, 2);
				}
			}
			break;
		case GENCMD_USE_SENTRY:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)) &&
				G_ItemUsable(&ent->client->ps, HI_SENTRY_GUN) )
			{
				ItemUse_Sentry(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_SENTRY_GUN, 0);
				if (ent - g_entities >= 0 && ent - g_entities < MAX_CLIENTS && g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1 && bgSiegeClasses[ent->client->siegeClass].maxSentries > 0 && level.sentriesUsedThisLife[ent - g_entities] < bgSiegeClasses[ent->client->siegeClass].maxSentries)
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);
				else
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SENTRY_GUN);
			}
			break;
		case GENCMD_USE_JETPACK:
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) &&
				G_ItemUsable(&ent->client->ps, HI_JETPACK) )
			{
				ItemUse_Jetpack(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_JETPACK, 0);
			}
			break;
		case GENCMD_USE_HEALTHDISP:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_HEALTHDISP)) &&
				G_ItemUsable(&ent->client->ps, HI_HEALTHDISP) )
			{
				G_AddEvent(ent, EV_USE_ITEM0+HI_HEALTHDISP, 0);
			}
			break;
		case GENCMD_USE_AMMODISP:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_AMMODISP)) &&
				G_ItemUsable(&ent->client->ps, HI_AMMODISP) )
			{
				G_AddEvent(ent, EV_USE_ITEM0+HI_AMMODISP, 0);
			}
			break;
		case GENCMD_USE_EWEB:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)) &&
				G_ItemUsable(&ent->client->ps, HI_EWEB) )
			{
				ItemUse_UseEWeb(ent);
				G_AddEvent(ent, EV_USE_ITEM0+HI_EWEB, 0);
			}
			break;
		case GENCMD_USE_CLOAK:
			if (ent && ent->client && ent->client->emoted)
				break;
			if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) &&
				G_ItemUsable(&ent->client->ps, HI_CLOAK) )
			{
				if ( ent->client->ps.powerups[PW_CLOAKED] )
				{//decloak
					Jedi_Decloak( ent );
				}
				else
				{//cloak
					Jedi_Cloak( ent );
				}
			}
			break;
		case GENCMD_SABERATTACKCYCLE:
			Cmd_SaberAttackCycle_f(ent);
			break;
		case GENCMD_TAUNT:
			G_SetTauntAnim( ent, TAUNT_TAUNT );
			break;
		case GENCMD_BOW:
			G_SetTauntAnim( ent, TAUNT_BOW );
			break;
		case GENCMD_MEDITATE:
			G_SetTauntAnim( ent, TAUNT_MEDITATE );
			break;
		case GENCMD_FLOURISH:
			G_SetTauntAnim( ent, TAUNT_FLOURISH );
			break;
		case GENCMD_GLOAT:
			G_SetTauntAnim( ent, TAUNT_GLOAT );
			break;
		default:
			break;
		}
	}

	// save results of pmove
	if ( ent->client->ps.eventSequence != oldEventSequence ) {
		ent->eventTime = level.time;
	}
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qfalse );
		//rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qfalse );
	}

	if (isNPC)
	{
		ent->s.eType = ET_NPC;
	}

	SendPendingPredictableEvents( &ent->client->ps );

	if ( !( ent->client->ps.eFlags & EF_FIRING ) ) {
		client->fireHeld = qfalse;		// for grapple
	}

	// use the snapped origin for linking so it matches client predicted versions
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );

	if (ent->s.eType != ET_NPC ||
		ent->s.NPC_class != CLASS_VEHICLE ||
		!ent->m_pVehicle ||
		!ent->m_pVehicle->m_iRemovedSurfaces)
	{ //let vehicles that are getting broken apart do their own crazy sizing stuff
		VectorCopy (pm.mins, ent->r.mins);
		VectorCopy (pm.maxs, ent->r.maxs);
	}

	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;

	// execute client events
    //OSP: pause
    if ( level.pause.state == PAUSE_NONE )
            ClientEvents( ent, oldEventSequence );

	if ( pm.useEvent )
	{
		//TODO: Use
	}
	if ((ent->client->pers.cmd.buttons & BUTTON_USE) && ent->client->ps.useDelay < level.time)
	{
		TryUse(ent);
		ent->client->ps.useDelay = level.time + 100;
	}

	// link entity now, after any personal teleporters have been used
	trap_LinkEntity (ent);

	if ( !ent->client->noclip ) {
		G_TouchTriggers( ent );
	}

	// NOTE: now copy the exact origin over otherwise clients can be snapped into solid
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );

	//test for solid areas in the AAS file

	// touch other objects
	ClientImpacts( ent, &pm );

	// save results of triggers and client events
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// Did we kick someone in our pmove sequence?
	if (client->ps.forceKickFlip)
	{
		gentity_t *faceKicked = &g_entities[client->ps.forceKickFlip-1];

		if (faceKicked && faceKicked->client && (!OnSameTeam(ent, faceKicked) || g_friendlyFire.integer) &&
			(!faceKicked->client->ps.duelInProgress || !faceKicked->client->sess.siegeDuelInProgress || faceKicked->client->ps.duelIndex == ent->s.number) &&
			(!ent->client->ps.duelInProgress || !ent->client->sess.siegeDuelInProgress || ent->client->ps.duelIndex == faceKicked->s.number))
		{
			if ( faceKicked && faceKicked->client && faceKicked->health && faceKicked->takedamage )
			{//push them away and do pain
				vec3_t oppDir;
				int strength = (int)VectorNormalize2( client->ps.velocity, oppDir );

				strength *= 0.05;

				VectorScale( oppDir, -1, oppDir );

				G_Damage( faceKicked, ent, ent, oppDir, client->ps.origin, strength, DAMAGE_NO_ARMOR, MOD_MELEE );

				if ( faceKicked->client->ps.weapon != WP_SABER ||
					 faceKicked->client->ps.fd.saberAnimLevel != FORCE_LEVEL_3 ||
					 (!BG_SaberInAttack(faceKicked->client->ps.saberMove) && !PM_SaberInStart(faceKicked->client->ps.saberMove) && !PM_SaberInReturn(faceKicked->client->ps.saberMove) && !PM_SaberInTransition(faceKicked->client->ps.saberMove)) )
				{
					if (faceKicked->health > 0 &&
						faceKicked->client->ps.stats[STAT_HEALTH] > 0 &&
						faceKicked->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN)
					{
						if (BG_KnockDownable(&faceKicked->client->ps) && Q_irand(1, 10) <= 3)
						{ //only actually knock over sometimes, but always do velocity hit
							faceKicked->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
							faceKicked->client->ps.forceHandExtendTime = level.time + 1100;
							faceKicked->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
						}

						faceKicked->client->ps.otherKiller = ent->s.number;
						faceKicked->client->ps.otherKillerTime = level.time + 5000;
						faceKicked->client->ps.otherKillerDebounceTime = level.time + 100;

						faceKicked->client->ps.velocity[0] = oppDir[0]*(strength*40);
						faceKicked->client->ps.velocity[1] = oppDir[1]*(strength*40);
						faceKicked->client->ps.velocity[2] = 200;
					}
				}

				G_Sound( faceKicked, CHAN_AUTO, G_SoundIndex( va("sound/weapons/melee/punch%d", Q_irand(1, 4)) ) );
			}
		}

		client->ps.forceKickFlip = 0;
	}

	// check for respawning
	if ( client->ps.stats[STAT_HEALTH] <= 0 
		&& !(client->ps.eFlags2&EF2_HELD_BY_MONSTER)//can't respawn while being eaten
		&& ent->s.eType != ET_NPC ) {
		// wait for the attack button to be pressed
		if ( level.time > client->respawnTime && !gDoSlowMoDuel ) {
			// forcerespawn is to prevent users from waiting out powerups
			int forceRes = g_forcerespawn.integer;

			if (g_gametype.integer == GT_POWERDUEL)
			{
				forceRes = 1;
			}
			else if (g_gametype.integer == GT_SIEGE &&
				g_siegeRespawn.integer)
			{ //wave respawning on
				forceRes = 1;
			}

			if ( forceRes > 0 && 
				( level.time - client->respawnTime ) > forceRes * 1000 ) {
				respawn( ent );
				return;
			}
		
			// pressing attack or use is the normal respawn method
			if ( ucmd->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) ) {
				respawn( ent );
			}
		}
		else if (gDoSlowMoDuel)
		{
			client->respawnTime = level.time + 1000;
		}
		return;
	}

	// perform once-a-second actions
    //OSP: pause
    if ( level.pause.state == PAUSE_NONE )
            ClientTimerActions( ent, msec );

	G_UpdateClientBroadcasts ( ent );

	//try some idle anims on ent if getting no input and not moving for some time
	G_CheckClientIdle( ent, ucmd );

	// This code was moved here from clientThink to fix a problem with g_synchronousClients 
	// being set to 1 when in vehicles. 
	if ( ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		//run it
		if (g_entities[ent->client->ps.m_iVehicleNum].inuse && g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			ClientThink(ent->client->ps.m_iVehicleNum, &g_entities[ent->client->ps.m_iVehicleNum].m_pVehicle->m_ucmd);
		}
		else
		{ //vehicle no longer valid?
			ent->client->ps.m_iVehicleNum = 0;
		}
	}

	// note the last class they were when alive
	if (g_gametype.integer == GT_SIEGE && ent - g_entities < MAX_CLIENTS && ent->client->sess.sessionTeam == TEAM_RED &&
		client->ps.stats[STAT_HEALTH] > 0 && level.time > client->tempSpectate) {
		// note the last class you had while alive
		level.siegeTopTimes[ent - g_entities].lastSiegeClassWhileAlive = client->siegeClass;

		// have you changed class at this obj?
		if (level.time >= level.forceCheckClassTime) {
			if (!level.siegeTopTimes[ent - g_entities].classOnFile) {
				level.siegeTopTimes[ent - g_entities].classOnFile = client->siegeClass + 2; // make sure not to set to 0, since index 0 is an actual class
			}
			else if (level.siegeTopTimes[ent - g_entities].classOnFile != client->siegeClass + 2) {
				level.siegeTopTimes[ent - g_entities].hasChangedClass = qtrue;
			}
		}

		// have you changed class ever?
		if (!level.siegeTopTimes[ent - g_entities].classOnFileTotal) {
			level.siegeTopTimes[ent - g_entities].classOnFileTotal = client->siegeClass + 2; // make sure not to set to 0, since index 0 is an actual class
		}
		else if (level.siegeTopTimes[ent - g_entities].classOnFileTotal != client->siegeClass + 2) {
			level.siegeTopTimes[ent - g_entities].hasChangedClassTotal = qtrue;
		}
	}

	if (g_gametype.integer == GT_SIEGE && ent - g_entities < MAX_CLIENTS && ent->client->sess.senseBoost &&
		ent->health > 0 && ent->client->sess.sessionTeam != TEAM_SPECTATOR && ent->client->siegeClass != -1
		&& !bgSiegeClasses[ent->client->siegeClass].forcePowerLevels[FP_SEE]) {
		int interval = 0;
		switch (ent->client->sess.senseBoost) {
		case 1: interval = g_senseBoost1_interval.integer; break;
		case 2: interval = g_senseBoost2_interval.integer; break;
		case 3: interval = g_senseBoost3_interval.integer; break;
		case 4: interval = g_senseBoost4_interval.integer; break;
		case 5: interval = 0; break; // just constant
		}
		if ((!interval || level.time % interval <= 1000) && !IsMindTrickedByAnyone(ent - g_entities)) {
			ent->client->ps.fd.forcePowerLevel[FP_SEE] = 3;
			ent->client->ps.fd.forcePowersActive |= (1 << FP_SEE);
		}
		else {
			ent->client->ps.fd.forcePowerLevel[FP_SEE] = 0;
			ent->client->ps.fd.forcePowersActive &= ~(1 << FP_SEE);
		}
	}

	G_StoreTrail(ent);
}

/*
==================
G_CheckClientTimeouts

Checks whether a client has exceded any timeouts and act accordingly
==================
*/
void G_CheckClientTimeouts ( gentity_t *ent )
{
	// Only timeout supported right now is the timeout to spectator mode
	if ( !g_timeouttospec.integer )
	{
		return;
	}

	// Already a spectator, no need to boot them to spectator
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		return;
	}

	// See how long its been since a command was received by the client and if its 
	// longer than the timeout to spectator then force this client into spectator mode
	if ( level.time - ent->client->pers.cmd.serverTime > g_timeouttospec.integer * 1000 )
	{
		SetTeam ( ent, "spectator" ,qtrue);
	}
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
void ClientThink( int clientNum,usercmd_t *ucmd ) {
	gentity_t *ent;

	ent = g_entities + clientNum;
	if (clientNum < MAX_CLIENTS)
	{
		trap_GetUsercmd( clientNum, &ent->client->pers.cmd );
	}

	// mark the time we got info, so we can display the
	// phone jack if they don't get any for a while
	ent->client->lastCmdTime = level.time;
	ent->client->lastRealCmdTime = level.time;

	if (ucmd)
	{
		ent->client->pers.cmd = *ucmd;
	}

	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		ClientThink_real( ent );
	}
	// vehicles are clients and when running synchronous they still need to think here
	// so special case them.
	else if ( clientNum >= MAX_CLIENTS ) {
		ClientThink_real( ent );
	}
}


void G_RunClient( gentity_t *ent ) {
	// force a client think if it's been too long since their last real one
	if (!(ent->r.svFlags & SVF_BOT) && g_forceClientUpdateRate.integer && ent->client->lastCmdTime < level.time - g_forceClientUpdateRate.integer &&
		ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		trap_GetUsercmd(ent - g_entities, &ent->client->pers.cmd);
		ent->client->lastCmdTime = level.time;
		ent->client->pers.cmd.serverTime = level.time;
		ent->client->pers.cmd.forwardmove = ent->client->pers.cmd.rightmove = ent->client->pers.cmd.upmove = 0;
		//ent->client->pers.cmd.buttons = 0;

		ClientThink_real(ent);
		return;
	}

	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		return;
	}
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink_real( ent );
}


/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame( gentity_t *ent ) {
	gclient_t	*cl;

	if (ent->s.eType == ET_NPC)
	{
		assert(0);
		return;
	}

	// if we are doing a chase cam or a remote view, grab the latest info
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		int		clientNum;

		clientNum = ent->client->sess.spectatorClient;

		// team follow1 and team follow2 go to whatever clients are playing
		if ( clientNum == -1 ) {
			clientNum = level.follow1;
		} else if ( clientNum == -2 ) {
			clientNum = level.follow2;
		}
		if ( clientNum >= 0 ) {
			cl = &level.clients[ clientNum ];
			if ( cl->pers.connected == CON_CONNECTED && 
				cl->sess.sessionTeam != TEAM_SPECTATOR ) {
				int originalPing = ent->client->ps.ping;

				ent->client->ps.eFlags = cl->ps.eFlags;
				ent->client->ps = cl->ps;

				if (ent->client->sess.isInkognito || (g_lockdown.integer && !G_ClientIsWhitelisted(ent - g_entities))
					|| originalPing == -1
					|| originalPing == 999){
					ent->client->ps.ping = originalPing;
					ent->client->ps.persistant[PERS_SCORE] = 0;
				}
				ent->client->ps.zoomMode = cl->ps.zoomMode;
				ent->client->ps.pm_flags |= PMF_FOLLOW;
				return;
			} else {
				// drop them to free spectators unless they are dedicated camera followers
				if ( ent->client->sess.spectatorClient >= 0 ) {
					ent->client->ps.zoomMode = 0;
					ent->client->sess.spectatorState = SPECTATOR_FREE;
					ClientBegin( ent->client - level.clients, qtrue );
				}
			}
		}
	}
    else
    {
		int time_delta = level.time - level.previousTime;
		qboolean increasedEnterTime = qfalse;
		if (level.intermissiontime) {
			ent->client->pers.enterTime += time_delta;
			increasedEnterTime = qtrue;
		}
        if (level.pause.state != PAUSE_NONE) {
            if (!increasedEnterTime) // don't do it twice
				ent->client->pers.enterTime += time_delta;
            ent->client->sess.inactivityTime += (time_delta / 1000);
        }
    }

	if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
		ent->client->sess.siegeFollowing.wasFollowing = qtrue;
		ent->client->sess.siegeFollowing.followingClientNum = ent->client->sess.spectatorClient;
	}
	else if (g_gametype.integer != GT_SIEGE) {
		ent->client->sess.siegeFollowing.wasFollowing = qfalse;
	}
	else if ((level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
		level.siegeRoundStartTime && level.time - level.siegeRoundStartTime >= 100) {
		ent->client->sess.siegeFollowing.wasFollowing = qfalse;
	}

	if ( ent->client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		ent->client->ps.pm_flags |= PMF_SCOREBOARD;
	} else {
		ent->client->ps.pm_flags &= ~PMF_SCOREBOARD;
	}
}

void NoteClientsOnLiftAtPause(void) {
	for (int i = 0; i < MAX_CLIENTS; i++)
		level.clients[i].pers.onLiftDuringPause = NULL;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];

		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED)
			continue;
		if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE || ent->client->ps.groundEntityNum == ENTITYNUM_WORLD)
			continue;
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			continue;
		if (ent->client->ps.pm_type == PM_SPECTATOR)
			continue;
		
		gentity_t *groundEnt = &g_entities[ent->client->ps.groundEntityNum];

		if (groundEnt->client)
			continue;
		if (!groundEnt->blocked)
			continue;

		ent->client->pers.onLiftDuringPause = groundEnt;
	}
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEdFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
void ClientEndFrame( gentity_t *ent ) {
	int			i;
	qboolean isNPC = qfalse;

	if (ent->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	if (ent->client && ent - g_entities < MAX_CLIENTS) {
		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
			ent->client->sess.siegeFollowing.wasFollowing = qfalse;
		}
		else if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
			ent->client->sess.siegeFollowing.wasFollowing = qtrue;
			ent->client->sess.siegeFollowing.followingClientNum = ent->client->sess.spectatorClient;
		}
	}

#ifdef NEWMOD_SUPPORT
	if (ent - g_entities < MAX_CLIENTS && ent->client) {
		// add the EF_CONNECTION flag for non-specs if we haven't gotten commands recently
		if (level.time - ent->client->lastRealCmdTime > 1000) {
			ent->client->isLagging = qtrue;
			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
				ent->client->ps.eFlags |= EF_CONNECTION;

				// auto-pause if someone goes 999 for a few seconds during a live pug
				if (g_gametype.integer == GT_SIEGE && level.isLivePug == ISLIVEPUG_YES && g_autoPause999.integer && level.pause.state != PAUSE_PAUSED &&
					level.time - ent->client->lastRealCmdTime >= (Com_Clampi(1, 10, g_autoPause999.integer) * 1000) &&
					(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
					level.siegeRoundStartTime && level.time - level.siegeRoundStartTime >= LIVEPUG_AUTOPAUSE_TIME) {
					level.pause.state = PAUSE_PAUSED;
					level.pause.time = level.time + 300000;
					NoteClientsOnLiftAtPause();
					Q_strncpyz(level.pause.reason, va("%s^7 is 999\n", ent->client->pers.netname), sizeof(level.pause.reason));
				}
			}
		}
		else {
			if (ent->client->ps.eFlags & EF_CONNECTION || ent->client->isLagging) { // he was lagging (or vid_restarted) but isn't anymore; send this stuff again just to be sure
				// try not to send these post-lag messages too rapidly
				static int lastTimePostLagMessagesSent[MAX_CLIENTS] = { 0 };
				int now = trap_Milliseconds();
				if (!lastTimePostLagMessagesSent[ent - g_entities] || now - lastTimePostLagMessagesSent[ent - g_entities] >= 3000) {
					G_BroadcastServerFeatureList(ent - g_entities);
					UpdateNewmodSiegeClassLimits(ent - g_entities);
					if (ent->client->sess.isInkognito)
						trap_SendServerCommand(ent - g_entities, "kls -1 -1 \"inko\" \"1\""); // only update if enabled
					lastTimePostLagMessagesSent[ent - g_entities] = now;
				}
			}
			ent->client->isLagging = qfalse;
			ent->client->ps.eFlags &= ~EF_CONNECTION;
		}
	}
#endif

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		SpectatorClientEndFrame( ent );
		return;
	}

	// turn off any expired powerups
    for ( i=0; i<MAX_POWERUPS; i++ )
    {// turn off any expired powerups

            //OSP: pause
            //      If we're paused, update powerup timers accordingly.
            //      Make sure we dont let stuff like CTF flags expire.
            if ( ent->client->ps.powerups[i] == 0 )
                    continue;

            if ( level.pause.state != PAUSE_NONE && ent->client->ps.powerups[i] != INT_MAX )
                    ent->client->ps.powerups[i] += level.time - level.previousTime;

            if ( ent->client->ps.powerups[i] < level.time )
                    ent->client->ps.powerups[i] = 0;
    }

	int time_delta = level.time - level.previousTime;
	qboolean increasedEnterTime = qfalse;
	if (level.intermissiontime) {
		ent->client->pers.enterTime += time_delta;
		increasedEnterTime = qtrue;
	}
    if ( level.pause.state != PAUSE_NONE ) {
		//OSP: pause
		//      If we're paused, make sure other timers stay in sync
        int i;

		if (ent->client->ps.hackingTime)
			ent->client->ps.hackingTime += time_delta;
        ent->client->airOutTime += time_delta;
        ent->client->sess.inactivityTime += (time_delta / 1000);
		if (!increasedEnterTime) // don't do it twice
			ent->client->pers.enterTime += time_delta;
        ent->client->pers.teamState.lastreturnedflag += time_delta;
        ent->client->pers.teamState.lasthurtcarrier += time_delta;
        ent->client->pers.teamState.lastfraggedcarrier += time_delta;
		ent->client->pers.teamState.flagsince.startTime += time_delta; // base_enhanced
		ent->client->pers.protsince += time_delta; // force stats
        ent->client->respawnTime += time_delta;
		if (ent->client->invulnerableTimer)
			ent->client->invulnerableTimer += time_delta;
        ent->pain_debounce_time += time_delta;
        ent->client->ps.fd.forcePowerRegenDebounceTime += time_delta;
		if (ent->client->tempSpectate)
		{
			ent->client->tempSpectate += time_delta;
		}
		// update force powers durations
		for(i=0;i < NUM_FORCE_POWERS;++i) {
			if (ent->client->ps.fd.forcePowerDuration[i])
				ent->client->ps.fd.forcePowerDuration[i] += time_delta;
		}
			
    }


	// save network bandwidth
#if 0
	if ( !g_synchronousClients->integer && (ent->client->ps.pm_type == PM_NORMAL || ent->client->ps.pm_type == PM_JETPACK || ent->client->ps.pm_type == PM_FLOAT) ) {
		// FIXME: this must change eventually for non-sync demo recording
		VectorClear( ent->client->ps.viewangles );
	}
#endif

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if ( level.intermissiontime ) {
		if ( ent->s.number < MAX_CLIENTS
			|| ent->client->NPC_class == CLASS_VEHICLE )
		{//players and vehicles do nothing in intermissions
			return;
		}
	}

	// burn from lava, etc
	P_WorldEffects (ent);

	// apply all the damage taken this frame
	P_DamageFeedback (ent);

	ent->client->ps.stats[STAT_HEALTH] = ent->health;	// FIXME: get rid of ent->health...

	G_SetClientSound (ent);

	// set the latest infor
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qfalse );
		//rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qfalse );
	}

	if (isNPC)
	{
		ent->s.eType = ET_NPC;
	}

	SendPendingPredictableEvents( &ent->client->ps );

}


