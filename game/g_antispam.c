// anti doorspam and anti minespam (iLikeToDoorSpam 0 / iLikeToMineSpam 0)
//
// doorspam: a defender fires an explosive at a closed door while an attacker they cannot see is close behind it
// minespam: a defender places or throws a mine at an attacker in front of them
// a shot judged to be spam is suppressed and its ammo refunded; nothing else happens to the shooter

#include "g_local.h"

extern int BotMindTricked(int botClient, int enemyClient);
extern int Get_Max_Ammo(gentity_t *ent, ammo_t ammoIndex);
extern qboolean InFOV(gentity_t *ent, gentity_t *from, int hFOV, int vFOV);

#define DOOR_VICTIM_DISTANCE	512		// an unseen attacker this close behind a closed door makes it doorspam
#define VICTIM_HEIGHT_ABOVE		296		// about codes main room floor to top of bunker
#define VICTIM_HEIGHT_BELOW		116		// about the high part of the ravine to the low part
#define HEIGHT_UNLIMITED		99999
#define MAX_FAN_DOORS			64
#define ANY						999999.0f	// unbounded box axis

typedef struct {
	vec3_t	mins, maxs;		// inclusive
} antiSpamBox_t;

// siege_cargobarge2 v1.1+ and siege_cargobarge3; doorspam is only policed inside these
static const antiSpamBox_t CARGO_2ND_OBJ			= { { 1846, 1719, -ANY },	{ 3269, 3422, ANY } };
static const antiSpamBox_t CARGO_STATION1			= { { 6678, 62, -ANY },		{ 7277, 708, ANY } };
static const antiSpamBox_t CARGO_STATION2			= { { 6935, -1318, -ANY },	{ 7579, -588, ANY } };
static const antiSpamBox_t CARGO_VENT_SHIELD_ROOM	= { { 6496, -1313, -ANY },	{ 6799, -877, ANY } };
static const antiSpamBox_t CARGO_HALLWAY_NEAR_CC	= { { 6081, -299, -ANY },	{ 6564, 1047, ANY } };

static const antiSpamBox_t HOTH_WALKER_SPAWN_OBJ1	= { { 6549, -1394, -ANY },	{ 8204, 762, ANY } };
static const antiSpamBox_t HOTH_WALKER_SPAWN_OBJ23	= { { 2287, -1083, -ANY },	{ 4113, 549, ANY } };
static const antiSpamBox_t HOTH_WALKER_AREA_OBJ4	= { { -917, -3729, -ANY },	{ 8219, 1635, ANY } };
static const antiSpamBox_t HOTH_INFIRMARY_LIFT_TOP	= { { -1440, -230, 161 },	{ -1136, 155, 200 } };
static const antiSpamBox_t HOTH_HANGAR				= { { -3825, -570, -295 },	{ -1258, 755, 38 } };
static const antiSpamBox_t HOTH_HANGAR_LIFT_SHAFT	= { { -1216, -128, -ANY },	{ -996, 142, 120 } };
static const vec3_t HOTH_SHORT_LIFT					= { -2224, -321, 484 };

static const antiSpamBox_t NAR_STATION1_OBJ_ROOM	= { { -1660, 7119, -ANY },	{ -989, 7639, ANY } };
static const antiSpamBox_t NAR_STATION1				= { { -1660, 7119, -ANY },	{ -989, 8280, ANY } };
static const antiSpamBox_t NAR_OUTSIDE_D_SPAWN		= { { -912, 7983, -ANY },	{ -263, 8607, ANY } };	// third objective
static const antiSpamBox_t NAR_D_SPAWN				= { { -1287, 8257, -ANY },	{ -545, 8709, ANY } };

// state of one verdict
typedef struct {
	const antiSpamShot_t	*shot;
	gentity_t				*ent;
	float					*origin;
	vec3_t					eye;
	float					victimHeightMin, victimHeightMax;	// world z window a victim must be in
	float					doorVictimDistance;
	int						numDoors;
	vec3_t					eyeToDoor[MAX_FAN_DOORS];			// from the first ray that hit each closed door
	int						enemyInHangar;						// -1 until looked up
	const char				*reason;
} antiSpamCtx_t;

static qboolean NotSpam(antiSpamCtx_t *ctx, const char *reason) {
	ctx->reason = reason;
	return qfalse;
}

static qboolean Spam(antiSpamCtx_t *ctx, const char *reason) {
	ctx->reason = reason;
	return qtrue;
}

static qboolean InBox(const antiSpamBox_t *box, const vec3_t point) {
	int i;

	for (i = 0; i < 3; i++) {
		if (point[i] < box->mins[i] || point[i] > box->maxs[i])
			return qfalse;
	}

	return qtrue;
}

static qboolean SegmentTouchesBox(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs) {
	float	enter = 0.0f, leave = 1.0f;
	int		i;

	for (i = 0; i < 3; i++) {
		float delta = end[i] - start[i];
		float t1, t2;

		if (delta == 0.0f) {
			if (start[i] < mins[i] || start[i] > maxs[i])
				return qfalse;
			continue;
		}

		t1 = (mins[i] - start[i]) / delta;
		t2 = (maxs[i] - start[i]) / delta;
		if (t1 > t2) {
			float swap = t1;
			t1 = t2;
			t2 = swap;
		}

		if (t1 > enter)
			enter = t1;
		if (t2 < leave)
			leave = t2;
		if (enter > leave)
			return qfalse;
	}

	return qtrue;
}

/*
==================
ray fan

the shot is judged by what a fan of rays around the aim direction hits first
offsets are added to the aim point in world axes, so the fan is much taller than it is wide
==================
*/

typedef struct {
	int			sideExtent, sideStep;
	int			top, bottom, heightStep;
	qboolean	needsDoor;		// nothing to learn from the fan unless a closed door is in it
} rayFan_t;

static const rayFan_t DEFENSE_FAN		= { 1024, 512, 4096, -4096, 512, qfalse };
static const rayFan_t DEFENSE_DOOR_FAN	= { 1024, 512, 4096, -4096, 512, qtrue };
static const rayFan_t NAR_OFFENSE_FAN	= { 512, 512, 2048, -4096, 256, qfalse };

typedef enum {
	RAYHIT_NOTHING,
	RAYHIT_LEGIT_TARGET,	// aiming at this is never spam
	RAYHIT_CLOSED_DOOR
} rayHit_t;

typedef rayHit_t (*rayClassifier_t)(const gentity_t *hit);

static qboolean IsClassname(const gentity_t *ent, const char *classname) {
	return ent->classname && !Q_stricmp(ent->classname, classname);
}

// npcs on offense count as players here
static qboolean IsLivingAttacker(const gentity_t *ent) {
	return ent->client && ent->client->sess.sessionTeam == TEAM_RED && ent->health > 0 && ent->takedamage &&
		!(ent->client->tempSpectate >= level.time) && !(ent->flags & FL_NOTARGET);
}

// a closed door that will open for someone and close again
static qboolean IsClosedDoor(const gentity_t *ent) {
	return IsClassname(ent, "func_door") && ent->moverState == MOVER_POS1 && ent->wait >= 0 &&
		!(ent->spawnflags & (MOVER_CRUSHER | MOVER_TOGGLE | MOVER_LOCKED));
}

static rayHit_t ClassifyForDoorspam(const gentity_t *hit) {
	if (IsLivingAttacker(hit) || IsClassname(hit, "item_shield"))
		return RAYHIT_LEGIT_TARGET;
	if (IsClosedDoor(hit))
		return RAYHIT_CLOSED_DOOR;
	return RAYHIT_NOTHING;
}

static rayHit_t ClassifyForMinespam(const gentity_t *hit) {
	return IsClassname(hit, "item_shield") ? RAYHIT_LEGIT_TARGET : RAYHIT_NOTHING;
}

// func_breakable is the door lock
static rayHit_t ClassifyForNarOffense(const gentity_t *hit) {
	return (IsClassname(hit, "item_shield") || IsClassname(hit, "func_breakable")) ? RAYHIT_LEGIT_TARGET : RAYHIT_NOTHING;
}

typedef struct {
	vec3_t	mins, maxs;
} rayCandidate_t;

// the entities in the fan that a ray could usefully hit
// a ray that touches none of their boxes is not worth tracing
static int FindRayCandidates(const antiSpamCtx_t *ctx, const rayFan_t *fan, const vec3_t aimPoint,
	rayClassifier_t classify, rayCandidate_t *candidates, qboolean *foundDoor) {
	vec3_t	mins, maxs;
	int		touched[MAX_GENTITIES];
	int		numTouched, numCandidates = 0;
	int		i, axis;

	for (axis = 0; axis < 3; axis++) {
		float low = aimPoint[axis] + (axis == 2 ? fan->bottom : -fan->sideExtent);
		float high = aimPoint[axis] + (axis == 2 ? fan->top : fan->sideExtent);

		mins[axis] = minimum(ctx->eye[axis], low) - 2;
		maxs[axis] = maximum(ctx->eye[axis], high) + 2;
	}

	*foundDoor = qfalse;
	numTouched = trap_EntitiesInBox(mins, maxs, touched, MAX_GENTITIES);

	for (i = 0; i < numTouched; i++) {
		const gentity_t	*other = &g_entities[touched[i]];
		rayCandidate_t	*candidate;
		rayHit_t		hit;

		if (other == ctx->ent)
			continue;

		hit = classify(other);
		if (hit == RAYHIT_NOTHING)
			continue;
		if (hit == RAYHIT_CLOSED_DOOR)
			*foundDoor = qtrue;

		// cover both the linked box and the box at the current origin
		candidate = &candidates[numCandidates++];
		for (axis = 0; axis < 3; axis++) {
			candidate->mins[axis] = other->r.absmin[axis];
			candidate->maxs[axis] = other->r.absmax[axis];
			if (!other->r.bmodel) {
				candidate->mins[axis] = minimum(candidate->mins[axis], other->r.currentOrigin[axis] + other->r.mins[axis]);
				candidate->maxs[axis] = maximum(candidate->maxs[axis], other->r.currentOrigin[axis] + other->r.maxs[axis]);
			}
			candidate->mins[axis] -= 2;
			candidate->maxs[axis] += 2;
		}
	}

	return numCandidates;
}

static void RememberDoor(antiSpamCtx_t *ctx, int *doorNums, int doorNum, const vec3_t hitPoint) {
	int i;

	for (i = 0; i < ctx->numDoors; i++) {
		if (doorNums[i] == doorNum)
			return;
	}

	if (ctx->numDoors == MAX_FAN_DOORS)
		return;

	doorNums[ctx->numDoors] = doorNum;
	VectorSubtract(ctx->eye, hitPoint, ctx->eyeToDoor[ctx->numDoors]);
	ctx->numDoors++;
}

// returns qtrue if the shooter is aiming at something that makes the shot legitimate
// closed doors are collected into ctx; the ray order decides which hit point represents a door
static qboolean FanHitsLegitTarget(antiSpamCtx_t *ctx, const rayFan_t *fan, rayClassifier_t classify) {
	rayCandidate_t	candidates[MAX_GENTITIES];
	int				doorNums[MAX_FAN_DOORS];
	int				numCandidates;
	qboolean		foundDoor;
	vec3_t			aimPoint, end;
	int				side0, side1, height, i;

	VectorMA(ctx->eye, ctx->shot->range, ctx->shot->forward, aimPoint);

	numCandidates = FindRayCandidates(ctx, fan, aimPoint, classify, candidates, &foundDoor);
	if (!numCandidates || (fan->needsDoor && !foundDoor))
		return qfalse;

	for (side0 = fan->sideExtent; side0 >= -fan->sideExtent; side0 -= fan->sideStep) {
		end[0] = aimPoint[0] + side0;
		for (side1 = fan->sideExtent; side1 >= -fan->sideExtent; side1 -= fan->sideStep) {
			end[1] = aimPoint[1] + side1;
			for (height = fan->top; height >= fan->bottom; height -= fan->heightStep) {
				trace_t	tr;

				end[2] = aimPoint[2] + height;

				for (i = 0; i < numCandidates; i++) {
					if (SegmentTouchesBox(ctx->eye, end, candidates[i].mins, candidates[i].maxs))
						break;
				}
				if (i == numCandidates)
					continue;

				trap_G2Trace(&tr, ctx->eye, NULL, NULL, end, ctx->ent->s.number, MASK_SHOT,
					G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
				if (tr.entityNum >= ENTITYNUM_WORLD)
					continue;

				switch (classify(&g_entities[tr.entityNum])) {
				case RAYHIT_LEGIT_TARGET:
					return qtrue;
				case RAYHIT_CLOSED_DOOR:
					RememberDoor(ctx, doorNums, tr.entityNum, tr.endpos);
					break;
				default:
					break;
				}
			}
		}
	}

	return qfalse;
}

/*
==================
victims
==================
*/

// a living player on the enemy team; never an npc, a spectator or someone in a rancor's hand
static qboolean IsLiveEnemyPlayer(const gentity_t *shooter, const gentity_t *other, team_t enemyTeam) {
	if (!other || !other->client)
		return qfalse;
	if (other->client->sess.sessionTeam && other->client->sess.sessionTeam != enemyTeam)
		return qfalse;
	if (other == shooter || !other->takedamage || other->health <= 0 || other->client->tempSpectate >= level.time)
		return qfalse;
	if ((other->flags & FL_NOTARGET) || other->s.eType == ET_NPC || (other->client->ps.eFlags2 & EF2_HELD_BY_MONSTER))
		return qfalse;

	return qtrue;
}

static qboolean EnemyInBox(const antiSpamCtx_t *ctx, float radius, const antiSpamBox_t *box) {
	gentity_t	*nearby[MAX_GENTITIES];
	int			numNearby = G_RadiusList(ctx->origin, radius, ctx->ent, qtrue, nearby);
	int			i;

	for (i = 0; i < numNearby; i++) {
		if (IsLiveEnemyPlayer(ctx->ent, nearby[i], TEAM_RED) && InBox(box, nearby[i]->client->ps.origin))
			return qtrue;
	}

	return qfalse;
}

static qboolean IsWalkerOrFighterVehicle(const gentity_t *vehicle) {
	return vehicle->m_pVehicle && vehicle->m_pVehicle->m_pVehicleInfo &&
		(vehicle->m_pVehicle->m_pVehicleInfo->type == VH_WALKER || vehicle->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER);
}

// the vehicle itself or anyone riding one, on either team
static qboolean IsWalkerOrFighter(const gentity_t *other) {
	if (other->s.eType == ET_NPC && IsWalkerOrFighterVehicle(other))
		return qtrue;

	return other->client && other->client->ps.m_iVehicleNum && IsWalkerOrFighterVehicle(&g_entities[other->client->ps.m_iVehicleNum]);
}

// anything goes with a walker or fighter around, except on hoth once the walker is no longer the fight
static qboolean WalkerAllowsSpam(const antiSpamCtx_t *ctx) {
	if (level.siegeMap != SIEGEMAP_HOTH || level.totalObjectivesCompleted <= 2)
		return qtrue;
	if (level.totalObjectivesCompleted == 3)
		return InBox(&HOTH_WALKER_AREA_OBJ4, ctx->origin);

	return qfalse;
}

// the farther away the victim, the more directly the shooter has to be facing them
static int ProhibitedConeForDistance(float distance) {
	if (distance < 600)
		return 70;
	if (distance < 900)
		return 50;
	if (distance < 1800)
		return 35;
	return 20;
}

static qboolean IsInFrontOfShooter(const antiSpamCtx_t *ctx, gentity_t *victim) {
	vec3_t	toVictim;
	float	*victimOrigin = victim->client->ps.origin;

	// attackers up in cc/short do not count against defenders down below
	if (level.siegeMap == SIEGEMAP_HOTH && ctx->origin[2] < 470 && victimOrigin[2] >= 470)
		return qfalse;

	// the vertical half of the cone is left wide open; height is checked by world z instead
	VectorSubtract(ctx->origin, victimOrigin, toVictim);
	if (!InFOV(victim, ctx->ent, ProhibitedConeForDistance(VectorLength(toVictim)), 170))
		return qfalse;

	return victimOrigin[2] >= ctx->victimHeightMin && victimOrigin[2] <= ctx->victimHeightMax;
}

typedef enum {
	DOORVICTIM_IRRELEVANT,
	DOORVICTIM_UNSEEN_BEHIND_DOOR,
	DOORVICTIM_IN_THE_OPEN
} doorVictim_t;

static doorVictim_t ClassifyDoorVictim(antiSpamCtx_t *ctx, gentity_t *victim) {
	vec3_t			eyeToVictim, doorToVictim;
	float			victimDistance;
	int				canBeSeen = -1;
	doorVictim_t	result = DOORVICTIM_IRRELEVANT;
	int				i;

	VectorSubtract(ctx->eye, victim->client->ps.origin, eyeToVictim);
	victimDistance = VectorLength(eyeToVictim);

	for (i = 0; i < ctx->numDoors; i++) {
		if (victimDistance > VectorLength(ctx->eyeToDoor[i])) {
			VectorSubtract(ctx->eyeToDoor[i], eyeToVictim, doorToVictim);
			if (VectorLength(doorToVictim) >= ctx->doorVictimDistance)
				continue;

			if (canBeSeen == -1)
				canBeSeen = G_ClientCanBeSeenByClient(victim, ctx->ent);
			if (!canBeSeen)
				result = DOORVICTIM_UNSEEN_BEHIND_DOOR;
			continue;
		}

		// the victim is on our side of this door
		// in the hoth hangar that only counts if the attackers have actually made it into the hangar
		if (level.siegeMap == SIEGEMAP_HOTH && InBox(&HOTH_HANGAR, ctx->origin)) {
			if (ctx->enemyInHangar == -1)
				ctx->enemyInHangar = EnemyInBox(ctx, 9999, &HOTH_HANGAR);
			if (!ctx->enemyInHangar)
				continue;
		}

		return DOORVICTIM_IN_THE_OPEN;
	}

	return result;
}

static qboolean DefenseVictimsMakeItSpam(antiSpamCtx_t *ctx) {
	gentity_t	*nearby[MAX_GENTITIES];
	int			numNearby = G_RadiusList(ctx->origin, ctx->shot->range, ctx->ent, qtrue, nearby);
	qboolean	foundVictim = qfalse;
	int			i;

	for (i = 0; i < numNearby; i++) {
		gentity_t *other = nearby[i];

		if (IsWalkerOrFighter(other)) {
			if (WalkerAllowsSpam(ctx))
				return NotSpam(ctx, "walker or fighter nearby");
			continue;
		}

		if (!IsLiveEnemyPlayer(ctx->ent, other, TEAM_RED) || !IsInFrontOfShooter(ctx, other))
			continue;

		// pretend a mind tricker is not there, or the refused shot gives them away
		if (BotMindTricked(ctx->ent->s.number, other->s.number))
			continue;

		if (other->client->ps.fd.forcePowersActive & (1 << FP_PROTECT))
			return NotSpam(ctx, "victim is using protect");

		if (ctx->shot->kind == ANTISPAM_MINE) {
			foundVictim = qtrue;
			continue;
		}

		switch (ClassifyDoorVictim(ctx, other)) {
		case DOORVICTIM_IN_THE_OPEN:
			return NotSpam(ctx, "an attacker is on this side of the door");
		case DOORVICTIM_UNSEEN_BEHIND_DOOR:
			foundVictim = qtrue;
			break;
		default:
			break;
		}
	}

	if (!foundVictim)
		return NotSpam(ctx, "no victim");

	return Spam(ctx, ctx->shot->kind == ANTISPAM_MINE ? "attacker in front of the mine" : "unseen attacker close behind a closed door");
}

/*
==================
defense
==================
*/

// returns qtrue if this spot or map state is exempt from doorspam detection
static qboolean DefenseDoorspamIsExempt(antiSpamCtx_t *ctx) {
	if (level.siegeMap == SIEGEMAP_CARGO) {
		if (InBox(&CARGO_2ND_OBJ, ctx->origin)) {
			ctx->victimHeightMin = ctx->origin[2] - HEIGHT_UNLIMITED;
			ctx->doorVictimDistance = 1024;
		}
		else if (InBox(&CARGO_STATION1, ctx->origin)) {
			ctx->victimHeightMin = ctx->origin[2] - HEIGHT_UNLIMITED;
			ctx->victimHeightMax = ctx->origin[2] + HEIGHT_UNLIMITED;
			if (EnemyInBox(ctx, 3000, &CARGO_STATION1))
				return qtrue;
		}
		else if (!InBox(&CARGO_STATION2, ctx->origin) && !InBox(&CARGO_VENT_SHIELD_ROOM, ctx->origin) &&
			!(InBox(&CARGO_HALLWAY_NEAR_CC, ctx->origin) && level.ccCompleted)) {
			return qtrue;
		}
	}

	// rockets are needed for the fourth objective
	if (!Q_stricmp(level.mapname, "mp/siege_eat_shower") && ctx->shot->weapon == WP_ROCKET_LAUNCHER && level.objectiveJustCompleted == 3)
		return qtrue;

	if ((level.siegeMap == SIEGEMAP_HOTH || level.siegeMap == SIEGEMAP_DESERT || level.siegeMap == SIEGEMAP_NAR) && !level.totalObjectivesCompleted)
		return qtrue;

	// final objective, below the command center
	if (level.siegeMap == SIEGEMAP_HOTH && level.totalObjectivesCompleted == 5 && ctx->origin[2] < 455)
		return qtrue;

	return level.siegeMap == SIEGEMAP_KORRIBAN;
}

// returns qtrue if this spot is exempt from minespam detection
static qboolean DefenseMinespamIsExempt(antiSpamCtx_t *ctx) {
	if (level.siegeMap == SIEGEMAP_HOTH) {
		if (InBox(&HOTH_WALKER_SPAWN_OBJ1, ctx->origin) || InBox(&HOTH_WALKER_SPAWN_OBJ23, ctx->origin))
			return qtrue;

		// catch mines thrown down the infirmary lift and the short lift
		if (InBox(&HOTH_INFIRMARY_LIFT_TOP, ctx->origin))
			ctx->victimHeightMin = ctx->origin[2] - 9999;
		else if (level.totalObjectivesCompleted == 5 && ctx->origin[2] >= 470 && ctx->origin[1] >= -615 &&
			DistanceHorizontal(ctx->origin, HOTH_SHORT_LIFT) <= 768)
			ctx->victimHeightMin = 180;		// world z of the bottom of the short lift
	}
	else if (level.siegeMap == SIEGEMAP_NAR) {
		// mining the station 1 objective room is fine until attackers are in the station
		if (InBox(&NAR_STATION1_OBJ_ROOM, ctx->origin) && !EnemyInBox(ctx, 3000, &NAR_STATION1))
			return qtrue;
	}

	return qfalse;
}

static qboolean DefenseShotIsSpam(antiSpamCtx_t *ctx) {
	ctx->victimHeightMax = ctx->origin[2] + VICTIM_HEIGHT_ABOVE;
	ctx->victimHeightMin = ctx->origin[2] - VICTIM_HEIGHT_BELOW;

	if (ctx->shot->kind == ANTISPAM_DOOR) {
		if (DefenseDoorspamIsExempt(ctx))
			return NotSpam(ctx, "doorspam is not policed here");
		if (FanHitsLegitTarget(ctx, &DEFENSE_DOOR_FAN, ClassifyForDoorspam))
			return NotSpam(ctx, "aiming at an attacker or a shield");
		if (!ctx->numDoors)
			return NotSpam(ctx, "not aiming at a closed door");
	}
	else {
		if (DefenseMinespamIsExempt(ctx))
			return NotSpam(ctx, "minespam is not policed here");
		if (FanHitsLegitTarget(ctx, &DEFENSE_FAN, ClassifyForMinespam))
			return NotSpam(ctx, "aiming at a shield");
	}

	return DefenseVictimsMakeItSpam(ctx);
}

/*
==================
offense

only mines thrown at the defense spawn from just outside it at the third objective of nar,
and only for a few seconds after a defender spawns
==================
*/

static qboolean OffenseShotIsSpam(antiSpamCtx_t *ctx) {
	gentity_t	*nearby[MAX_GENTITIES];
	int			numNearby, i;

	if (ctx->shot->weapon != WP_TRIP_MINE || level.siegeMap != SIEGEMAP_NAR || !InBox(&NAR_OUTSIDE_D_SPAWN, ctx->origin))
		return NotSpam(ctx, "offense is not policed here");

	if (level.time > level.antiSpawnSpamTime)
		return NotSpam(ctx, "no defender spawned recently");

	if (FanHitsLegitTarget(ctx, &NAR_OFFENSE_FAN, ClassifyForNarOffense))
		return NotSpam(ctx, "aiming at a shield or the door lock");

	numNearby = G_RadiusList(ctx->origin, ctx->shot->range, ctx->ent, qtrue, nearby);
	for (i = 0; i < numNearby; i++) {
		gentity_t *other = nearby[i];

		if (IsLiveEnemyPlayer(ctx->ent, other, TEAM_BLUE) && InFOV(other, ctx->ent, 60, 170) && InBox(&NAR_D_SPAWN, other->client->ps.origin))
			return Spam(ctx, "defender in their spawn");
	}

	return NotSpam(ctx, "no victim");
}

/*
==================
entry points
==================
*/

static qboolean IsPolicedOnThisMap(antiSpamKind_t kind) {
	if (g_gametype.integer != GT_SIEGE)
		return qfalse;

	if (!Q_stricmp(level.mapname, "siege_codes") || !Q_stricmpn(level.mapname, "mp/siege_crystals", 17))
		return qfalse;

	if (kind == ANTISPAM_DOOR && (level.siegeMap == SIEGEMAP_URBAN || level.siegeMap == SIEGEMAP_ANSION))
		return qfalse;

	return qtrue;
}

// has no side effects
qboolean G_AntiSpam_IsSpam(const antiSpamShot_t *shot, const char **reason) {
	antiSpamCtx_t	ctx;
	qboolean		spam;

	memset(&ctx, 0, sizeof(ctx));
	ctx.shot = shot;
	ctx.ent = shot->ent;
	ctx.doorVictimDistance = DOOR_VICTIM_DISTANCE;
	ctx.enemyInHangar = -1;

	if (!shot->ent || !shot->ent->client || !IsPolicedOnThisMap(shot->kind)) {
		spam = NotSpam(&ctx, "not policed on this map");
	}
	else {
		ctx.origin = shot->ent->client->ps.origin;
		VectorCopy(ctx.origin, ctx.eye);
		ctx.eye[2] += shot->ent->client->ps.viewheight;

		if (shot->ent->client->sess.sessionTeam == TEAM_BLUE)
			spam = DefenseShotIsSpam(&ctx);
		else if (shot->ent->client->sess.sessionTeam == TEAM_RED)
			spam = OffenseShotIsSpam(&ctx);
		else
			spam = NotSpam(&ctx, "not on a team");
	}

	if (reason)
		*reason = ctx.reason;

	return spam;
}

static void RefundAmmo(gentity_t *ent, weapon_t weapon, qboolean altFire) {
	int ammoIndex = weaponData[weapon].ammoIndex;
	int maxAmmo = Get_Max_Ammo(ent, ammoIndex);

	ent->client->ps.ammo[ammoIndex] += altFire ? weaponData[weapon].altEnergyPerShot : weaponData[weapon].energyPerShot;
	if (ent->client->ps.ammo[ammoIndex] > maxAmmo)
		ent->client->ps.ammo[ammoIndex] = maxAmmo;
}

// call before spawning the projectile; returns qtrue if the shot must not happen
// forward is the shooter's aim direction
qboolean G_AntiSpam_BlockShot(gentity_t *ent, antiSpamKind_t kind, weapon_t weapon, qboolean altFire, float range, const vec3_t forward) {
	antiSpamShot_t	shot;
	const char		*reason;
	qboolean		spam;

	if (kind == ANTISPAM_DOOR ? iLikeToDoorSpam.integer : iLikeToMineSpam.integer)
		return qfalse;

	shot.ent = ent;
	shot.kind = kind;
	shot.weapon = weapon;
	shot.altFire = altFire;
	shot.range = range;
	VectorCopy(forward, shot.forward);

	spam = G_AntiSpam_IsSpam(&shot, &reason);
	spam = G_AntiSpamShadow_Verdict(&shot, spam);

	if (g_antiSpamDebug.integer && ent && ent->client)
		trap_SendServerCommand(ent - g_entities, va("print \"antispam: %s (%s)\n\"", spam ? "^1blocked^7" : "^2allowed^7", reason));

	if (spam)
		RefundAmmo(ent, weapon, altFire);

	return spam;
}

// after the hoth hangar objective, explosives fired into the hangar lift shaft from above are removed
// while the lift is at the bottom or on its way up; someone riding the lift is low enough not to count
qboolean G_AntiSpam_ProjectileInHothLiftShaft(const gentity_t *projectile) {
	int i;

	if (!level.hangarCompletedTime || !InBox(&HOTH_HANGAR_LIFT_SHAFT, projectile->r.currentOrigin))
		return qfalse;

	if (g_entities[projectile->r.ownerNum].r.currentOrigin[2] < 184)
		return qfalse;

	for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
		const gentity_t *lift = &g_entities[i];

		if (!Q_stricmp(lift->targetname, "hangarplatbig1") && (lift->moverState == MOVER_POS1 || lift->moverState == MOVER_1TO2))
			return qtrue;
	}

	return qfalse;
}
