// ticktest: scripted scenarios that drive tick-sensitive mechanics directly, measured through [tick] log probes
// (damage probes log the amount handed to G_Damage, before godmode, armor or protect mitigation)
#include "g_local.h"
#include "bg_saga.h"

#define TT					level.tickTest
#define TT_BOT_A			0
#define TT_BOT_B			1
#define TT_BOT_WAIT_MS		15000
#define TT_ROUND_WAIT_MS	15000	// after the bots are in: the round begins 5 s later
#define TT_ROUND_SETTLE_MS	1000
#define TT_SEED				0x7469636b
#define TT_SETTLE_MSEC		1000	// saber scenarios: the photos, boxes, blends and angles settle this long after placement before the clock starts
#define TT_MOVER_CRUSHER	4	// MOVER_CRUSHER in g_mover.c

typedef struct {
	const char	*name;
	qboolean	(*setup)(void);
	void		(*frame)(void);
	void		(*teardown)(void);
	int			bots;
	int			arg;		// scenario-specific parameter (saber matrix cell, duel delay)
	void		(*cmd)(int idx, int c);	// per-command hook, before the pmove of bot idx's command at time c (NULL: per-frame logic only)
} tickTestScenario_t;

extern void AddSpawnField(char *field, char *value);
extern void G_SpawnGEntityFromSpawnVars(qboolean inSubBSP);
extern void health_power_converter_use(gentity_t *self, gentity_t *other, gentity_t *activator);
extern void shield_power_converter_use(gentity_t *self, gentity_t *other, gentity_t *activator);
extern void ammo_generic_power_converter_use(gentity_t *self, gentity_t *other, gentity_t *activator);
extern qboolean G_UseDispenserOn(gentity_t *ent, int dispType, gentity_t *target, qboolean viaCommand);
extern qboolean TryHeal(gentity_t *ent, gentity_t *target);
extern void Blocked_Door(gentity_t *ent, gentity_t *other, gentity_t *blockedBy);
extern void hurt_touch(gentity_t *self, gentity_t *other, trace_t *trace);
extern void target_laser_think(gentity_t *self);
extern void Touch_Multi(gentity_t *self, gentity_t *other, trace_t *trace);
extern void Reached_Train(gentity_t *ent);
extern void Reached_BinaryMover(gentity_t *ent);
extern void Use_BinaryMover_Go(gentity_t *ent);
extern void InitMoverTrData(gentity_t *ent);
extern void CreateLaserTrap(gentity_t *laserTrap, vec3_t start, gentity_t *owner, qboolean tripwire);
extern void laserTrapStick(gentity_t *ent, vec3_t endpos, vec3_t normal);
extern void rocketThink(gentity_t *ent);
extern gentity_t *NPC_SpawnType(gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle);
extern void EWebDisattach(gentity_t *owner, gentity_t *eweb);
extern qboolean TryHealingSomething(gentity_t *ent, gentity_t *target, qboolean doChecks, qboolean viaCommand);
extern void RunControlPoint(gentity_t *self);
extern qboolean gSiegeRoundBegun;
extern void SiegeItemThink(gentity_t *ent);
extern void NPC_Think(gentity_t *self);
extern void P_WorldEffects(gentity_t *ent);
extern void P_DamageFeedback(gentity_t *player);
extern void SaberUpdateSelf(gentity_t *ent);
extern void saberFirstThrown(gentity_t *saberent);
extern void saberBackToOwner(gentity_t *saberent);
extern void DownedSaberThink(gentity_t *saberent);
extern void saberReactivate(gentity_t *saberent, gentity_t *saberOwner);
extern qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride);
extern qboolean G_SaberModelSetup(gentity_t *ent);
extern void SaberGotHit(gentity_t *self, gentity_t *other, trace_t *trace);
extern void WP_SaberRemoveG2Model(gentity_t *saberent);
extern qboolean PM_SaberInParry(int move);
extern qboolean PM_SaberInKnockaway(int move);
extern qboolean PM_SaberInBrokenParry(int move);
extern qboolean PM_SaberInBounce(int move);
extern qboolean PM_SaberInStart(int move);
extern qboolean BG_InKataAnim(int anim);
extern void ClientThink_real(gentity_t *ent);
extern int bg_timesyncBase;

static const char *ttBotNames[2] = { "TickA", "TickB" };
static const char *ttBotTeams[2] = { "red", "blue" };
static const team_t ttBotTeamNums[2] = { TEAM_RED, TEAM_BLUE };

static gentity_t *TT_SpawnSynthetic(const char *classname);
static qboolean TT_BotValid(int idx);
static void TT_RecallSaber(gentity_t *owner);
static int TT_ScenarioArg(void);

static byte ttInuseBefore[MAX_GENTITIES];
static char ttFailText[256];
static int ttBotMove[2][2];		// forwardmove, rightmove of the next scripted command

/*
============================================================================
helpers
============================================================================
*/

static gentity_t *TT_Bot(int idx) {
	return &g_entities[TT.bot[idx]];
}

#define TT_A()	TT_Bot(TT_BOT_A)
#define TT_B()	TT_Bot(TT_BOT_B)

// records why setup or a frame cannot go on; the harness logs it as the scenario's error
static qboolean QDECL TT_Fail(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(ttFailText, sizeof(ttFailText), fmt, ap);
	va_end(ap);
	return qfalse;
}

static void TT_MarkEntities(void) {
	int i;
	for (i = 0; i < MAX_GENTITIES; i++)
		ttInuseBefore[i] = (byte)g_entities[i].inuse;
}

static qboolean TT_IsTracked(int slot) {
	gentity_t *e = &g_entities[TT.spawned[slot]];
	return e->inuse && e->classname == TT.spawnedClass[slot];
}

static void TT_Track(gentity_t *ent) {
	int i;

	if (!ent || !ent->inuse || ent->s.number < MAX_CLIENTS)
		return;

	for (i = 0; i < TT.numSpawned; i++) {
		if (TT.spawned[i] == ent->s.number && TT.spawnedClass[i] == ent->classname)
			return;
	}

	if (TT.numSpawned >= TICKTEST_MAX_SPAWNED) { // drop entries that are already gone
		int kept = 0;
		for (i = 0; i < TT.numSpawned; i++) {
			if (TT_IsTracked(i)) {
				TT.spawned[kept] = TT.spawned[i];
				TT.spawnedClass[kept] = TT.spawnedClass[i];
				kept++;
			}
		}
		TT.numSpawned = kept;
		if (TT.numSpawned >= TICKTEST_MAX_SPAWNED)
			return;
	}

	TT.spawned[TT.numSpawned] = ent->s.number;
	TT.spawnedClass[TT.numSpawned] = ent->classname;
	TT.numSpawned++;
}

// tracks everything spawned since TT_MarkEntities and returns the first one of that classname
static gentity_t *TT_TrackNew(const char *classname) {
	gentity_t *found = NULL;
	int i;

	for (i = MAX_CLIENTS; i < level.num_entities; i++) {
		gentity_t *e = &g_entities[i];

		if (!e->inuse || ttInuseBefore[i] || e->freeAfterEvent || e->s.eType >= ET_EVENTS || !e->classname)
			continue;

		TT_Track(e);
		if (!found && classname && !Q_stricmp(e->classname, classname))
			found = e;
	}

	return found;
}

static void TT_SetEnt(int slot, gentity_t *ent) {
	TT.ent[slot] = ent ? ent->s.number : -1;
}

// the scenario's entity in that slot, or NULL once it is gone
static gentity_t *TT_GetEnt(int slot) {
	gentity_t *e;

	if (TT.ent[slot] < 0)
		return NULL;

	e = &g_entities[TT.ent[slot]];
	if (!e->inuse) {
		TT.ent[slot] = -1;
		return NULL;
	}

	return e;
}

static qboolean TT_OwnedByBot(gentity_t *e) {
	int idx;

	for (idx = 0; idx < 2; idx++) {
		gentity_t *bot;

		if (!TT.haveBot[idx])
			continue;

		bot = TT_Bot(idx);
		if (e->s.eType == ET_MISSILE && e->r.ownerNum == bot->s.number)
			return qtrue;
		if (e->parent == bot && !e->client && (e->s.eType == ET_SPECIAL || (e->classname &&
			(!Q_stricmp(e->classname, "laserTrap") || !Q_stricmp(e->classname, "sentryGun")))))
			return qtrue;
	}

	return qfalse;
}

static void TT_FreeSpawned(void) {
	int i;

	for (i = 0; i < TT.numSpawned; i++) {
		if (TT_IsTracked(i))
			G_FreeEntity(&g_entities[TT.spawned[i]]);
	}
	TT.numSpawned = 0;

	for (i = MAX_CLIENTS; i < level.num_entities; i++) {
		gentity_t *e = &g_entities[i];

		if (!e->inuse || e->isSaberEntity || e->neverFree || e->freeAfterEvent)
			continue;
		if (TT_OwnedByBot(e))
			G_FreeEntity(e);
	}
}

static void TT_SetHealth(gentity_t *ent, int health) {
	ent->health = health;
	ent->client->ps.stats[STAT_HEALTH] = health;
}

static void TT_DrainAmmo(gentity_t *ent) {
	int i;
	for (i = 0; i < AMMO_MAX; i++)
		ent->client->ps.ammo[i] = 0;
}

static void TT_GiveWeapon(gentity_t *ent, int weapon) {
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << weapon);
	ent->client->ps.weapon = weapon;
	ent->s.weapon = weapon;
	ent->client->ps.weaponstate = WEAPON_READY;
	ent->client->ps.weaponTime = 0;
	if (weaponData[weapon].ammoIndex > AMMO_NONE && weaponData[weapon].ammoIndex < AMMO_MAX)
		ent->client->ps.ammo[weaponData[weapon].ammoIndex] = ammoData[weaponData[weapon].ammoIndex].max;
}

static void TT_GivePower(gentity_t *ent, forcePowers_t power, int powerLevel) {
	ent->client->ps.fd.forcePowersKnown |= (1 << power);
	ent->client->ps.fd.forcePowerLevel[power] = powerLevel;
}

// runs the power with no duration, so it stays on until the scenario ends
static void TT_HoldPower(gentity_t *ent, forcePowers_t power, int powerLevel) {
	TT_GivePower(ent, power, powerLevel);
	ent->client->ps.fd.forcePowersActive |= (1 << power);
	ent->client->ps.fd.forcePowerDuration[power] = 0;
}

static void TT_FillForce(gentity_t *ent, int amount) {
	ent->client->ps.fd.forcePower = amount;
}

/*
============================================================================
placement
============================================================================
*/

static vec3_t ttHullMins = { -15, -15, DEFAULT_MINS_2 };
static vec3_t ttHullMaxs = { 15, 15, DEFAULT_MAXS_2 };

static void TT_SetPos(gentity_t *ent, vec3_t origin) {
	trap_UnlinkEntity(ent);
	VectorCopy(origin, ent->client->ps.origin);
	VectorClear(ent->client->ps.velocity);
	BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, qtrue);
	VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);
	VectorCopy(ent->client->ps.origin, ent->s.origin); // target_laser aims at s.origin
	trap_LinkEntity(ent);
}

// like TeleportPlayer without the kill box and the spit-out velocity
static void TT_Place(gentity_t *ent, vec3_t origin, float yaw, float pitch) {
	vec3_t angles;

	VectorSet(angles, pitch, yaw, 0);
	// test bot commands carry zero angles, so the view is held entirely by delta_angles
	ent->client->pers.cmd.angles[0] = ent->client->pers.cmd.angles[1] = ent->client->pers.cmd.angles[2] = 0;
	SetClientViewAngle(ent, angles);
	ent->client->ps.eFlags ^= EF_TELEPORT_BIT;
	TT_SetPos(ent, origin);
}

static void TT_Pin(int idx, vec3_t origin) {
	TT.botPinned[idx] = qtrue;
	VectorCopy(origin, TT.botPin[idx]);
}

// a pinned bot back on its pin, after each of its sub-frame commands (so how far it moves between resets does not depend on sv_fps) and each frame
void TickTest_ApplyPin(int idx) {
	gentity_t *ent;

	if (idx < 0 || idx > 1 || !TT.haveBot[idx] || !TT.botPinned[idx])
		return;

	ent = TT_Bot(idx);
	if (Distance(ent->client->ps.origin, TT.botPin[idx]) > 0.5f || VectorLengthSquared(ent->client->ps.velocity) > 0.0f)
		TT_SetPos(ent, TT.botPin[idx]);
}

static void TT_ApplyPins(void) {
	TickTest_ApplyPin(TT_BOT_A);
	TickTest_ApplyPin(TT_BOT_B);
}

static qboolean TT_HullClear(vec3_t from, vec3_t to) {
	trace_t tr;
	trap_Trace(&tr, from, ttHullMins, ttHullMaxs, to, ENTITYNUM_NONE, MASK_PLAYERSOLID);
	return !tr.startsolid && !tr.allsolid && tr.fraction == 1.0f;
}

static qboolean TT_HasFloor(vec3_t p) {
	trace_t tr;
	vec3_t down;

	VectorCopy(p, down);
	down[2] -= 64;
	trap_Trace(&tr, p, ttHullMins, ttHullMaxs, down, ENTITYNUM_NONE, MASK_PLAYERSOLID);
	return !tr.startsolid && tr.fraction < 1.0f;
}

static qboolean TT_TryLine(vec3_t start, float length, float lateral) {
	int step;

	if (!TT_HullClear(start, start))
		return qfalse;

	for (step = 0; step < 24; step++) {
		vec3_t angles, dir, right, end, p, mid;
		qboolean ok = qtrue;
		int k;

		VectorSet(angles, 0, step * 15.0f, 0);
		AngleVectors(angles, dir, right, NULL);
		VectorMA(start, length, dir, end);
		if (!TT_HullClear(start, end))
			continue;

		for (k = 0; k <= 8 && ok; k++) {
			VectorMA(start, length * k / 8.0f, dir, p);
			if (!TT_HasFloor(p))
				ok = qfalse;
		}

		if (ok && lateral > 0) {
			VectorMA(start, length * 0.5f, dir, mid);
			VectorMA(mid, lateral, right, p);
			if (!TT_HullClear(mid, p))
				ok = qfalse;
			VectorMA(mid, -lateral, right, p);
			if (ok && !TT_HullClear(mid, p))
				ok = qfalse;
		}

		if (ok) {
			VectorCopy(start, TT.origin);
			VectorCopy(dir, TT.dir);
			VectorCopy(right, TT.right);
			TT.yaw = angles[YAW];
			G_LogPrintf("[tick] place %s origin=%.1f %.1f %.1f yaw=%.0f\n", TT.name, start[0], start[1], start[2], TT.yaw);
			return qtrue;
		}
	}

	return qfalse;
}

/*
finds a straight, floored line a player hull fits along, starting at a spawn point (or, failing that,
an item), so scenarios never depend on hand-picked coordinates. the bots are unlinked while it runs.
*/
static qboolean TT_FindLine(float length, float lateral) {
	static const char *spots[] = { "info_player_siegeteam1", "info_player_siegeteam2", "info_player_deathmatch", "team_CTF_redspawn", "team_CTF_bluespawn",
		"team_CTF_redplayer", "team_CTF_blueplayer", "info_player_start", NULL };
	int i;

	for (i = 0; spots[i]; i++) {
		gentity_t *spot = NULL;

		while ((spot = G_Find(spot, FOFS(classname), spots[i])) != NULL) {
			vec3_t start;

			VectorCopy(spot->s.origin, start);
			start[2] += 8;
			if (TT_TryLine(start, length, lateral))
				return qtrue;
		}
	}

	for (i = MAX_CLIENTS; i < level.num_entities; i++) {
		gentity_t *e = &g_entities[i];
		vec3_t start;

		if (!e->inuse || e->s.eType != ET_ITEM || (e->flags & FL_DROPPED_ITEM))
			continue;

		VectorCopy(e->r.currentOrigin, start);
		start[2] += -DEFAULT_MINS_2 + 1;
		if (TT_TryLine(start, length, lateral))
			return qtrue;
	}

	return qfalse;
}

static void TT_Along(float dist, vec3_t out) {
	VectorMA(TT.origin, dist, TT.dir, out);
}

static void TT_DropToFloor(vec3_t p) {
	trace_t tr;
	vec3_t down;

	VectorCopy(p, down);
	down[2] -= 256;
	trap_Trace(&tr, p, NULL, NULL, down, ENTITYNUM_NONE, MASK_SOLID);
	VectorCopy(tr.endpos, p);
}

// A at the start of a clear line facing along it, B `dist` units out facing back at A
static qboolean TT_PlacePair(float dist, float length, float lateral) {
	vec3_t p;

	if (!TT_FindLine(length, lateral))
		return TT_Fail("no clear line of %d units from any spawn point", (int)length);

	TT_Place(TT_A(), TT.origin, TT.yaw, 0);
	if (TT.haveBot[TT_BOT_B]) {
		TT_Along(dist, p);
		TT_Place(TT_B(), p, TT.yaw + 180.0f, 0);
	}
	return qtrue;
}

static qboolean TT_Visible(vec3_t from, int passEnt, gentity_t *target) {
	trace_t tr;
	vec3_t to;

	VectorCopy(target->client ? target->client->ps.origin : target->r.currentOrigin, to);
	trap_Trace(&tr, from, NULL, NULL, to, passEnt, MASK_SHOT);
	return tr.entityNum == target->s.number || (tr.fraction == 1.0f && !tr.startsolid);
}

// trace from a's eye to b's origin
static qboolean TT_LineOfSight(gentity_t *a, gentity_t *b) {
	vec3_t eye;

	VectorCopy(a->client ? a->client->ps.origin : a->r.currentOrigin, eye);
	eye[2] += a->client ? a->client->ps.viewheight : 16;
	return TT_Visible(eye, a->s.number, b);
}

#define TT_REQUIRE_LOS(a, b)	do { if (!TT_LineOfSight((a), (b))) return TT_Fail("no line of sight after placement"); } while (0)

// spawns a map entity from key/value pairs the way the map loader does, so G_Spawn* sees every key
static gentity_t *TT_SpawnMapEntity(const char *classname, vec3_t origin, float yaw, const char **keys) {
	qboolean wasSpawning = level.spawning;
	gentity_t *ent;
	int i;

	level.numSpawnVars = 0;
	level.numSpawnVarChars = 0;
	AddSpawnField("classname", (char *)classname);
	AddSpawnField("origin", va("%f %f %f", origin[0], origin[1], origin[2]));
	AddSpawnField("angles", va("0 %f 0", yaw));
	for (i = 0; keys && keys[i] && keys[i + 1]; i += 2)
		AddSpawnField((char *)keys[i], (char *)keys[i + 1]);

	TT_MarkEntities();
	level.spawning = qtrue;
	G_SpawnGEntityFromSpawnVars(qfalse);
	level.spawning = wasSpawning;
	level.numSpawnVars = 0;
	level.numSpawnVarChars = 0;

	ent = TT_TrackNew(classname);
	if (!ent)
		TT_Fail("%s did not spawn", classname);
	return ent;
}

static void TT_Every(int slot, int interval, void (*fn)(void)) {
	if (level.time >= TT.timer[slot]) {
		TT.timer[slot] += interval;
		if (TT.timer[slot] <= level.time)
			TT.timer[slot] = level.time + interval;
		fn();
	}
}

/*
============================================================================
stations
============================================================================
*/

static qboolean TT_SetupStation(const char *classname, const char **keys, qboolean holdUse) {
	gentity_t *station;
	vec3_t p;

	if (!TT_PlacePair(128, 256, 0))
		return qfalse;

	TT_Along(224, p);
	station = TT_SpawnMapEntity(classname, p, TT.yaw, keys);
	if (!station)
		return qfalse;

	TT_SetEnt(0, station);
	if (holdUse)
		TT.botButtons[TT_BOT_B] = BUTTON_USE; // what a player at a station holds
	return qtrue;
}

// what TryUse does for a player holding use, once per frame: the use gate, then whether TryUse runs at all
static qboolean TT_UseReaches(gentity_t *user) {
	return G_UseRuns(G_UseGateDue(user));
}

static gentity_t *TT_Station(void) {
	gentity_t *station = TT_GetEnt(0);
	if (!station)
		TT_Fail("station is gone");
	return station;
}

static qboolean TTS_StationHealth(void) {
	static const char *keys[] = { "model", "models/items/power_converter.md3", "count", "100000", NULL };
	return TT_SetupStation("misc_model_health_power_converter", keys, qtrue);
}

static void TTF_StationHealth(void) {
	gentity_t *station = TT_Station(), *b = TT_B();
	if (!station)
		return;
	TT_SetHealth(b, 1);
	health_power_converter_use(station, b, b);
}

// misc_model_shield_power_converter never reads "amount" and gives 0 armor, so the floor unit stands in
static qboolean TTS_StationShield(void) {
	static const char *keys[] = { "count", "100000", NULL };
	return TT_SetupStation("misc_shield_floor_unit", keys, qtrue);
}

// any class with armor will do
static qboolean TTS_StationShieldSiege(void) {
	int i;

	if (!TTS_StationShield())
		return qfalse;
	for (i = 0; i < bgNumSiegeClasses; i++) {
		if (bgSiegeClasses[i].maxarmor > 0)
			break;
	}
	if (i == bgNumSiegeClasses)
		return TT_Fail("no siege class has armor");
	TT_B()->client->siegeClass = i;
	return qtrue;
}

static void TTF_StationShield(void) {
	gentity_t *station = TT_Station(), *b = TT_B();
	if (!station)
		return;
	b->client->ps.stats[STAT_ARMOR] = 0;
	shield_power_converter_use(station, b, b);
}

// B does not hold use in the ammo scenarios: TT_UseReaches evaluates B's use gate itself
static qboolean TTS_StationAmmo(void) {
	static const char *keys[] = { "model", "models/items/power_converter.md3", "nodrain", "1", NULL };
	return TT_SetupStation("misc_model_ammo_power_converter", keys, qfalse);
}

// the converter only refills ammo for weapons the player carries
static void TT_GiveAmmoWeapons(gentity_t *ent) {
	static const int weapons[] = { WP_BLASTER, WP_DISRUPTOR, WP_REPEATER, WP_ROCKET_LAUNCHER, WP_THERMAL, WP_TRIP_MINE, WP_DET_PACK };
	int i;

	for (i = 0; i < (int)(sizeof(weapons) / sizeof(weapons[0])); i++)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << weapons[i]);
}

static qboolean TTS_StationAmmoSiege(void) {
	if (!TTS_StationAmmo())
		return qfalse;
	TT_GiveAmmoWeapons(TT_B());
	return qtrue;
}

static void TTF_StationAmmo(void) {
	gentity_t *station = TT_Station(), *b = TT_B();
	if (!station)
		return;
	TT_DrainAmmo(b);
	if (TT_UseReaches(b))
		station->use(station, b, b); // ammo_generic_power_converter_use in siege
}

static qboolean TTS_StationAmmoGeneric(void) {
	static const char *keys[] = { "nodrain", "1", NULL };
	return TT_SetupStation("misc_ammo_floor_unit", keys, qfalse);
}

static qboolean TTS_StationAmmoGenericSiege(void) {
	if (!TTS_StationAmmoGeneric())
		return qfalse;
	TT_GiveAmmoWeapons(TT_B());
	return qtrue;
}

static void TTF_StationAmmoGeneric(void) {
	gentity_t *station = TT_Station(), *b = TT_B();
	if (!station)
		return;
	TT_DrainAmmo(b);
	if (TT_UseReaches(b))
		ammo_generic_power_converter_use(station, b, b);
}

static qboolean TTS_Recharge(void) {
	static const char *keys[] = { "model", "models/items/power_converter.md3", "count", "10", NULL };
	return TT_SetupStation("misc_model_health_power_converter", keys, qfalse);
}

static qboolean TTS_RechargeShield(void) {
	static const char *keys[] = { "count", "10", NULL };
	return TT_SetupStation("misc_shield_floor_unit", keys, qfalse);
}

// emptied every frame so check_recharge always has a point to give back
static void TTF_Recharge(void) {
	gentity_t *station = TT_Station();
	if (station)
		station->count = 0;
}

/*
============================================================================
dispensers
============================================================================
*/

static qboolean TTS_Medic(void) {
	if (!TT_PlacePair(64, 128, 0))
		return qfalse;
	TT_A()->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_HEALTHDISP);
	return qtrue;
}

static void TTF_Medic(void) {
	TT_SetHealth(TT_B(), 1);
	if (TT_UseReaches(TT_A()))
		G_UseDispenserOn(TT_A(), HI_HEALTHDISP, TT_B(), qfalse);
}

// the medic tapped: use held 100 ms of every 250, the gates told of the key-up frames as ClientThink does
static void TTF_MedicTap(void) {
	TT_SetHealth(TT_B(), 1);
	if ((level.time - TT.startTime) % 250 < 100) {
		if (TT_UseReaches(TT_A()))
			G_UseDispenserOn(TT_A(), HI_HEALTHDISP, TT_B(), qfalse);
	}
	else if (TICKFIX(Input)) {
		G_UseKeyUp(TT_A());
	}
}

static qboolean TTS_TechAmmo(void) {
	if (!TT_PlacePair(64, 128, 0))
		return qfalse;
	TT_A()->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_AMMODISP);
	TT_GiveWeapon(TT_B(), WP_BLASTER);
	return qtrue;
}

static void TTF_TechAmmo(void) {
	gentity_t *b = TT_B();

	if (b->client->ps.weapon != WP_BLASTER) {
		TT_Fail("target lost its blaster");
		return;
	}
	b->client->ps.ammo[weaponData[WP_BLASTER].ammoIndex] = 0;
	if (TT_UseReaches(TT_A()))
		G_UseDispenserOn(TT_A(), HI_AMMODISP, b, qfalse);
}

// TryHeal only works in siege, for a class named in the target's healingclass
static qboolean TTS_Repair(void) {
	gentity_t *a, *target;

	if (g_gametype.integer != GT_SIEGE)
		return TT_Fail("TryHeal only repairs in siege");
	if (!TT_PlacePair(64, 128, 0))
		return qfalse;

	a = TT_A();
	if (a->client->siegeClass < 0)
		return TT_Fail("test bot has no siege class");

	target = TT_SpawnSynthetic("func_breakable");
	target->healingclass = bgSiegeClasses[a->client->siegeClass].name;
	target->healingrate = 250;
	target->maxHealth = 500;
	target->health = 1;
	return qtrue;
}

static void TTF_Repair(void) {
	gentity_t *target = TT_GetEnt(0);

	if (!target) {
		TT_Fail("repair target is gone");
		return;
	}
	if (target->health >= 400)
		target->health = 1;
	if (TT_UseReaches(TT_A()))
		TryHeal(TT_A(), target);
}

// the use key gate alone: one probe per use key tick while held
static qboolean TTS_UseGate(void) {
	return TT_PlacePair(128, 160, 0);
}

static void TTF_UseGate(void) {
	gentity_t *a = TT_A();
	if (G_UseGateDue(a))
		TICKPROBE("use_generic", a->s.number, 1);
}

/*
the use key gate through the bot's own commands, pressed 100 ms of every 300: one probe per use key tick,
v = ms since the previous one (0 for the first)
*/
static void TTF_UseGateRepress(void) {
	gentity_t *a = TT_A();

	TT.botButtons[TT_BOT_A] = (level.time - TT.startTime) % 300 < 100 ? BUTTON_USE : 0;
	if (!TT.timer[2]) { // the deadline left from before the scenario is not a tick
		TT.timer[0] = a->client->ps.useDelay;
		TT.timer[2] = 1;
	}
	else if (a->client->ps.useDelay != TT.timer[0]) {
		TT.timer[0] = a->client->ps.useDelay;
		TICKPROBE("use_repress", a->s.number, TT.timer[1] ? level.time - TT.timer[1] : 0);
		TT.timer[1] = level.time;
	}
}

/*
============================================================================
turrets
============================================================================
*/

static qboolean TTS_Sentry(void) {
	gentity_t *a, *sentry;
	vec3_t eye;

	if (!TT_PlacePair(400, 464, 0))
		return qfalse;

	a = TT_A();
	TT_REQUIRE_LOS(a, TT_B());
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);

	TT_MarkEntities();
	ItemUse_Sentry(a);
	sentry = TT_TrackNew("sentryGun");
	if (!sentry)
		return TT_Fail("sentry did not spawn");

	VectorCopy(sentry->r.currentOrigin, eye);
	eye[2] += 24;
	if (!TT_Visible(eye, sentry->s.number, TT_B()))
		return TT_Fail("no line of sight after placement");

	TT_SetEnt(0, sentry);
	TT.timer[0] = level.time + 1000;
	return qtrue;
}

// turn the gun away so it keeps slewing back at its capped rate
static void TT_KickSentry(void) {
	gentity_t *sentry = TT_GetEnt(0);
	if (sentry)
		sentry->speed = AngleNormalize360(sentry->speed + 180.0f);
}

static void TTF_Sentry(void) {
	gentity_t *sentry = TT_GetEnt(0);

	if (!sentry) {
		TT_Fail("sentry is gone");
		return;
	}
	sentry->count = 40; // ammo; it shuts down when this runs out
	TT_Every(0, 1000, TT_KickSentry);
}

static qboolean TTS_TurretG2(void) {
	static const char *keys[] = { "spawnflags", "2", "radius", "1024", "alliedTeam", "1", NULL };
	gentity_t *turret;
	vec3_t p, eye;

	if (!TT_PlacePair(664, 696, 0))
		return qfalse;

	TT_A()->flags |= FL_NOTARGET;
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);

	TT_Along(64, p);
	turret = TT_SpawnMapEntity("misc_turretG2", p, TT.yaw, keys);
	if (!turret)
		return qfalse;

	VectorCopy(turret->r.currentOrigin, eye);
	eye[2] += 20;
	if (!TT_Visible(eye, turret->s.number, TT_B()))
		return TT_Fail("no line of sight after placement");

	TT_SetEnt(0, turret);
	TT.timer[0] = level.time + 1000;
	return qtrue;
}

static void TT_KickTurretG2(void) {
	gentity_t *turret = TT_GetEnt(0);
	if (turret)
		turret->speed += 30.0f; // pitch
}

static void TTF_TurretG2(void) {
	if (!TT_GetEnt(0)) {
		TT_Fail("turret is gone");
		return;
	}
	TT_Every(0, 1000, TT_KickTurretG2);
}

static gentity_t *TT_TurretTop(void) {
	gentity_t *base = TT_GetEnt(0), *top;

	if (!base)
		return NULL;
	top = &g_entities[base->r.ownerNum];
	return top->inuse ? top : NULL;
}

static qboolean TTS_Turret(void) {
	static const char *keys[] = { "alliedTeam", "1", NULL };
	gentity_t *base, *top;
	vec3_t p;

	if (!TT_PlacePair(696, 728, 0))
		return qfalse;

	TT_A()->flags |= FL_NOTARGET;
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);

	TT_Along(96, p);
	TT_DropToFloor(p);
	base = TT_SpawnMapEntity("misc_turret", p, TT.yaw, keys);
	if (!base)
		return qfalse;

	TT_SetEnt(0, base);
	top = TT_TurretTop();
	if (!top)
		return TT_Fail("turret has no top");
	if (!TT_Visible(top->r.currentOrigin, top->s.number, TT_B()))
		return TT_Fail("no line of sight after placement");

	TT.timer[0] = level.time + 1000;
	return qtrue;
}

static void TT_KickTurret(void) {
	gentity_t *top = TT_TurretTop();

	if (!top)
		return;
	top->s.apos.trBase[YAW] = AngleNormalize180(top->r.currentAngles[YAW] + 90.0f);
	top->s.apos.trBase[PITCH] = top->r.currentAngles[PITCH];
	top->s.apos.trType = TR_STATIONARY;
	top->s.apos.trTime = level.time;
}

static void TTF_Turret(void) {
	if (!TT_TurretTop()) {
		TT_Fail("turret is gone");
		return;
	}
	TT_Every(0, 1000, TT_KickTurret);
}

/*
============================================================================
force powers
============================================================================
*/

static qboolean TT_SetupForceAttack(float dist, forcePowers_t power, int button) {
	gentity_t *a;

	if (!TT_PlacePair(dist, dist + 32, 0))
		return qfalse;

	a = TT_A();
	TT_REQUIRE_LOS(a, TT_B());
	TT_GivePower(a, power, FORCE_LEVEL_3);
	a->client->ps.saberHolstered = 2; // no saber force restrictions
	TT.botButtons[TT_BOT_A] = button;
	return qtrue;
}

static qboolean TTS_Lightning(void) {
	if (!TT_SetupForceAttack(200, FP_LIGHTNING, BUTTON_FORCE_LIGHTNING))
		return qfalse;
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);
	return qtrue;
}

static void TTF_Lightning(void) {
	TT_FillForce(TT_A(), 100);
	TT_SetHealth(TT_B(), 100);
}

static qboolean TTS_LightningProtect(void) {
	gentity_t *b;

	if (!TTS_Lightning())
		return qfalse;

	b = TT_B();
	b->flags &= ~FL_GODMODE; // protect is applied after the godmode check in G_Damage
	TT_HoldPower(b, FP_PROTECT, FORCE_LEVEL_3);
	return qtrue;
}

static void TTF_LightningProtect(void) {
	TT_FillForce(TT_A(), 100);
	TT_FillForce(TT_B(), 100);
	TT_SetHealth(TT_B(), 100);
}

static qboolean TTS_LightningAbsorb(void) {
	if (!TTS_Lightning())
		return qfalse;
	TT_HoldPower(TT_B(), FP_ABSORB, FORCE_LEVEL_3);
	return qtrue;
}

// B is kept below full so every absorbed point is a real gain
static void TTF_LightningAbsorb(void) {
	TT_FillForce(TT_A(), 100);
	TT_FillForce(TT_B(), 50);
}

static qboolean TTS_Drain(void) {
	if (!TT_SetupForceAttack(150, FP_DRAIN, BUTTON_FORCE_DRAIN))
		return qfalse;
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);
	return qtrue;
}

static void TTF_Drain(void) {
	TT_FillForce(TT_A(), 100);
	TT_FillForce(TT_B(), 100);
}

static qboolean TTS_Grip(void) {
	return TT_SetupForceAttack(128, FP_GRIP, BUTTON_FORCEGRIP);
}

static void TTF_Grip(void) {
	TT_FillForce(TT_A(), 100);
}

static qboolean TTS_Rage(void) {
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;
	TT_HoldPower(TT_A(), FP_RAGE, FORCE_LEVEL_3);
	return qtrue;
}

static void TTF_Rage(void) {
	TT_SetHealth(TT_A(), 100);
}

static qboolean TTS_Protect(void) {
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;
	TT_HoldPower(TT_A(), FP_PROTECT, FORCE_LEVEL_3);
	return qtrue;
}

static void TTF_Protect(void) {
	TT_FillForce(TT_A(), 100);
}

static qboolean TTS_Regen(void) {
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;
	TT_FillForce(TT_A(), 0);
	return qtrue;
}

static void TTF_Regen(void) {
	gentity_t *a = TT_A();
	if (a->client->ps.fd.forcePower >= 90)
		TT_FillForce(a, 0);
}

/*
============================================================================
items
============================================================================
*/

static qboolean TT_StartJetpack(void) {
	gentity_t *a = TT_A();

	a->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
	a->client->ps.jetpackFuel = 100;
	Jetpack_On(a);
	if (!a->client->jetPackOn)
		return TT_Fail("jetpack did not turn on");
	TT.botUpmove[TT_BOT_A] = 127;
	return qtrue;
}

static qboolean TTS_Jetpack(void) {
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;
	return TT_StartJetpack();
}

// a class with the short-burst jetpack; if the map has none, A's class gets the flag for the scenario
static qboolean TTS_JetpackShort(void) {
	gentity_t *a;
	int i;

	if (!TT_PlacePair(128, 160, 0))
		return qfalse;

	a = TT_A();
	TT.savedClass = -1;
	for (i = 0; i < bgNumSiegeClasses; i++) {
		if (bgSiegeClasses[i].shortBurstJetpack)
			break;
	}
	if (i < bgNumSiegeClasses) {
		a->client->siegeClass = i;
		G_Printf("ticktest: jetpack_short uses class %s\n", bgSiegeClasses[i].name);
	}
	else {
		if (a->client->siegeClass < 0)
			return TT_Fail("test bot has no siege class");
		TT.savedClass = a->client->siegeClass;
		bgSiegeClasses[TT.savedClass].shortBurstJetpack = qtrue;
		G_Printf("ticktest: jetpack_short lends the short-burst jetpack to class %s\n", bgSiegeClasses[TT.savedClass].name);
	}
	// no force jump: its hold drain is per usercmd, and test bots send one per server frame
	a->client->ps.fd.forcePowersKnown &= ~(1 << FP_LEVITATION);
	a->client->ps.fd.forcePowerLevel[FP_LEVITATION] = FORCE_LEVEL_0;
	return TT_StartJetpack();
}

static void TTT_JetpackShort(void) {
	if (TT.savedClass >= 0)
		bgSiegeClasses[TT.savedClass].shortBurstJetpack = qfalse;
	TT.savedClass = -1;
}

static qboolean TTS_Cloak(void) {
	gentity_t *a;

	if (!TT_PlacePair(128, 160, 0))
		return qfalse;

	a = TT_A();
	a->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);
	a->client->ps.cloakFuel = 100;
	a->client->cloakToggleTime = 0;
	ItemUse_UseCloak(a);
	if (!a->client->ps.powerups[PW_CLOAKED])
		return TT_Fail("cloak did not turn on");
	return qtrue;
}

static void TTF_Nothing(void) {
}

static qboolean TTS_EWeb(void) {
	return TT_PlacePair(128, 160, 48);
}

// the e-web needs the owner standing on the ground, so it is set up once A has landed
static void TTF_EWeb(void) {
	gentity_t *a = TT_A();

	if (TT.timer[1]) {
		if (!TT_GetEnt(0))
			TT_Fail("e-web is gone");
		return;
	}

	if (a->client->ps.groundEntityNum != ENTITYNUM_NONE && level.time - TT.startTime >= 500) {
		ItemUse_UseEWeb(a);
		if (a->client->ewebIndex) {
			TT_Track(&g_entities[a->client->ewebIndex]);
			TT_SetEnt(0, &g_entities[a->client->ewebIndex]);
			TT.timer[1] = 1;
			return;
		}
	}
	if (level.time - TT.startTime > 2000)
		TT_Fail("e-web could not be placed");
}

static qboolean TTS_ExphysItem(void) {
	return TT_PlacePair(256, 288, 0);
}

// toss a medpack whenever the last one is gone and the toss debounce allows
static void TTF_ExphysItem(void) {
	gentity_t *a = TT_A(), *item;

	if (TT_GetEnt(0))
		return;
	if (a->client->tossableItemDebounce > level.time)
		return;

	TT_MarkEntities();
	ItemUse_UseDisp(a, HI_HEALTHDISP);
	item = TT_TrackNew("item_medpak_instant");
	if (!item) {
		if (level.time - TT.startTime > 2000)
			TT_Fail("medpack did not spawn");
		return;
	}
	TT_SetEnt(0, item);
}

static qboolean TTS_Shield(void) {
	gentity_t *a, *shield;
	gitem_t *item = BG_FindItemForHoldable(HI_SHIELD);
	vec3_t p;

	if (!item)
		return TT_Fail("no shield item");
	if (!TT_FindLine(400, 0))
		return TT_Fail("no clear line of %d units from any spawn point", 400);

	a = TT_A();
	if (TT.haveBot[TT_BOT_B])
		TT_Place(TT_B(), TT.origin, TT.yaw, 0);
	TT_Along(150, p);
	TT_Place(a, p, TT.yaw, 0);

	TT_MarkEntities();
	ItemUse_Shield(a);
	shield = TT_TrackNew(item->classname);
	if (!shield)
		return TT_Fail("no room to place the shield");
	TT_SetEnt(0, shield);

	// the owner touching it makes it go non-solid and lose health
	TT_Along(64, p);
	TT_Place(a, p, TT.yaw, 0);
	return qtrue;
}

static void TTF_Shield(void) {
	if (!TT_GetEnt(0))
		TT_Fail("shield is gone");
}

// A's seeker drone shoots at frozen B in front of A
static qboolean TTS_Seeker(void) {
	gentity_t *a;

	if (!TT_PlacePair(256, 288, 0))
		return qfalse;

	a = TT_A();
	TT_REQUIRE_LOS(a, TT_B());
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);

	ItemUse_Seeker(a);
	if (!(a->client->ps.eFlags & EF_SEEKERDRONE))
		return TT_Fail("seeker did not activate");
	return qtrue;
}

static void TTF_Seeker(void) {
	gentity_t *a = TT_A();

	if (!(a->client->ps.eFlags & EF_SEEKERDRONE)) {
		TT_Fail("seeker is gone");
		return;
	}
	a->client->ps.droneExistTime = level.time + 30000; // it stops shooting in its last 5 s
}

// B stands in synthetic lava: P_WorldEffects and P_DamageFeedback run in ClientEndFrame's order.
// B is mortal here, since the pain debounce only re-arms on real pain
static qboolean TTS_Lava(void) {
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;
	TT_B()->flags &= ~FL_GODMODE;
	return qtrue;
}

static void TTF_Lava(void) {
	gentity_t *b = TT_B();
	int waterlevel = b->waterlevel, watertype = b->watertype;

	TT_SetHealth(b, 100);
	b->client->ps.stats[STAT_ARMOR] = 0;
	b->waterlevel = 1;
	b->watertype = CONTENTS_LAVA;
	P_WorldEffects(b);
	P_DamageFeedback(b);
	b->waterlevel = waterlevel;
	b->watertype = watertype;
}

// lava with the match paused for 3 s from 5 s in (pause.time keeps it from counting down on its own); the
// lava_pause probe marks the window (v 1 start, 0 end), and the sizzle may land at most once inside it
static void TTF_LavaPaused(void) {
	int t = level.time - TT.startTime;

	if (!TT.timer[0] && t >= 5000) {
		TT.timer[0] = 1;
		level.pause.state = PAUSE_PAUSED;
		level.pause.time = level.time + 3000 + 5000 + 1000;
		TICKPROBE("lava_pause", ENTITYNUM_WORLD, 1);
	}
	else if (TT.timer[0] == 1 && t >= 8000) {
		TT.timer[0] = 2;
		level.pause.state = PAUSE_UNPAUSING; // the game's own unpause runs next frame and clears the pause angles
		level.pause.time = level.time;
		TICKPROBE("lava_pause", ENTITYNUM_WORLD, 0);
	}
	TTF_Lava();
}

static void TTT_LavaPaused(void) {
	if (TT.timer[0] == 1) { // ended inside the pause window
		level.pause.state = PAUSE_UNPAUSING;
		level.pause.time = level.time;
	}
}

/*
B is killed every 5 s and respawns by holding attack; probes the respawn delay and the spawn invulnerability.
respawn waves are off for the scenario (g_siegeRespawn 0), so respawn() spawns B at once instead of holding it
for a wave
*/
static qboolean TTS_Respawn(void) {
	TT.savedCvar = trap_Cvar_VariableIntegerValue("g_siegeRespawn"); // before anything that can fail: teardown restores it
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;
	trap_Cvar_Set("g_siegeRespawn", "0");
	g_siegeRespawn.integer = 0;
	TT.timer[0] = level.time + 500;
	return qtrue;
}

static void TTF_Respawn(void) {
	gentity_t *b = TT_B();

	if (TT.timer[1]) { // dead, waiting to respawn
		if (b->health > 0 && b->client->ps.pm_type != PM_DEAD) {
			TICKPROBE("respawn_delay", b->s.number, level.time - TT.timer[1]);
			TT.timer[1] = 0;
			TT.timer[2] = level.time;
			TT.botButtons[TT_BOT_B] = 0;
			b->flags |= FL_GODMODE;
		}
		return;
	}

	if (TT.timer[2] && !(b->client->ps.eFlags & EF_INVULNERABLE)) {
		TICKPROBE("invuln_end", b->s.number, level.time - TT.timer[2]);
		TT.timer[2] = 0;
	}

	if (level.time >= TT.timer[0]) {
		TT.timer[0] += 5000;
		TT.timer[2] = 0;
		b->flags &= ~FL_GODMODE;
		G_Damage(b, NULL, NULL, NULL, NULL, 9999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		if (b->health > 0) {
			TT_Fail("B did not die");
			return;
		}
		TT.timer[1] = level.time;
		TT.botButtons[TT_BOT_B] = BUTTON_ATTACK;
	}
}

static void TTT_Respawn(void) {
	if (TT.savedCvar >= 0)
		trap_Cvar_Set("g_siegeRespawn", va("%d", TT.savedCvar));
	TT.savedCvar = -1;
}

// a 1 s powerup on A every 2 s; probes how long it lasted
static qboolean TTS_Powerup(void) {
	return TT_PlacePair(128, 160, 0);
}

static void TT_GivePowerup(void) {
	TT_A()->client->ps.powerups[PW_SPEEDBURST] = level.time + 1000;
	TT.timer[1] = level.time;
}

static void TTF_Powerup(void) {
	gentity_t *a = TT_A();

	if (TT.timer[1] && !a->client->ps.powerups[PW_SPEEDBURST]) {
		TICKPROBE("powerup_end", a->s.number, level.time - TT.timer[1]);
		TT.timer[1] = 0;
	}
	TT_Every(0, 2000, TT_GivePowerup);
}

/*
============================================================================
weapons
============================================================================
*/

static qboolean TTS_Rocket(void) {
	gentity_t *a;

	if (!TT_PlacePair(1500, 1532, 0))
		return qfalse;

	a = TT_A();
	TT_REQUIRE_LOS(a, TT_B());
	TT_GiveWeapon(a, WP_ROCKET_LAUNCHER);
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);
	return qtrue;
}

// keeps one homing rocket in flight at B
static void TTF_Rocket(void) {
	gentity_t *a = TT_A(), *rocket = TT_GetEnt(0);

	if (rocket && rocket->s.eType == ET_MISSILE)
		return;

	TT_GiveWeapon(a, WP_ROCKET_LAUNCHER);
	a->client->ps.rocketLockIndex = TT.bot[TT_BOT_B];
	a->client->ps.rocketLockTime = level.time - 2000;
	TT_MarkEntities();
	FireWeapon(a, qtrue);
	rocket = TT_TrackNew("rocket_proj");
	if (!rocket || rocket->think != rocketThink) {
		TT_Fail("homing rocket did not launch");
		return;
	}
	TT_SetEnt(0, rocket);
}

// mines are stuck straight onto the floor at TT.point, as touchLaserTrap does on impact
static void TT_LayMine(qboolean tripwire) {
	vec3_t up = { 0, 0, 1 };
	gentity_t *mine;

	TT_MarkEntities();
	mine = G_Spawn();
	CreateLaserTrap(mine, TT.point, TT_A(), tripwire);
	mine->setTime = level.time;
	if (tripwire)
		mine->count = 1;
	trap_LinkEntity(mine);
	laserTrapStick(mine, TT.point, up);
	TT_TrackNew(NULL);
	TT_SetEnt(0, mine);
}

static qboolean TTS_Tripwire(void) {
	vec3_t p;

	if (!TT_PlacePair(50, 400, 0))
		return qfalse;

	TT_Along(200, TT.point);
	TT_DropToFloor(TT.point);
	TT.point[2] += 1;
	TT_LayMine(qtrue);
	TT.timer[2] = 0;

	TT_Along(50, p);
	TT_Pin(TT_BOT_B, p);
	return qtrue;
}

// every 2 s: re-lay the mine if it went off, then walk B across the beam at 600 u/s after it has armed
static void TTF_Tripwire(void) {
	int elapsed = level.time - TT.startTime, cycle = elapsed / 2000, t = elapsed % 2000;
	float along;
	vec3_t p;

	if (cycle != TT.timer[2]) {
		TT.timer[2] = cycle;
		if (!TT_GetEnt(0))
			TT_LayMine(qtrue);
	}

	along = 50 + Com_Clamp(0, 300, (t - 1200) * 0.6f);
	TT_Along(along, p);
	TT_Pin(TT_BOT_B, p);
}

static qboolean TTS_Prox(void) {
	vec3_t p;

	if (!TT_PlacePair(100, 700, 0))
		return qfalse;

	TT_Along(600, TT.point);
	TT_DropToFloor(TT.point);
	TT.point[2] += 1;
	TT_LayMine(qfalse);
	TT.timer[2] = 0;

	TT_Along(100, p);
	TT_Pin(TT_BOT_B, p);
	return qtrue;
}

// every 4 s: re-lay the mine if it went off, then walk B at it at 600 u/s once it has armed
static void TTF_Prox(void) {
	int elapsed = level.time - TT.startTime, cycle = elapsed / 4000, t = elapsed % 4000;
	float along;
	vec3_t p;

	if (cycle != TT.timer[2]) {
		TT.timer[2] = cycle;
		if (!TT_GetEnt(0))
			TT_LayMine(qfalse);
	}

	along = 100 + Com_Clamp(0, 460, (t - 2200) * 0.6f);
	TT_Along(along, p);
	TT_Pin(TT_BOT_B, p);
}

static qboolean TTS_Thermal(void) {
	if (!TT_PlacePair(1000, 1032, 0)) // B well out of the thermal's path
		return qfalse;
	TT_GiveWeapon(TT_A(), WP_THERMAL);
	TT.timer[0] = level.time;
	return qtrue;
}

// one thermal at a time, thrown half a second after the last one went off
static void TTF_Thermal(void) {
	gentity_t *a = TT_A(), *thermal;

	if (TT.ent[0] >= 0) {
		if (TT_GetEnt(0))
			return;
		TT.timer[0] = level.time + 500;
	}
	if (level.time < TT.timer[0])
		return;

	TT_GiveWeapon(a, WP_THERMAL);
	a->client->ps.weaponChargeTime = level.time; // minimum charge: it lands near A
	TT_MarkEntities();
	FireWeapon(a, qfalse);
	thermal = TT_TrackNew("thermal_detonator");
	if (!thermal) {
		TT_Fail("thermal did not spawn");
		return;
	}
	TT_SetEnt(0, thermal);
}

static qboolean TTS_Demp2(void) {
	gentity_t *a;

	if (!TT_PlacePair(300, 332, 0))
		return qfalse;

	a = TT_A();
	TT_REQUIRE_LOS(a, TT_B());
	TT_GiveWeapon(a, WP_DEMP2);
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);
	TT.timer[0] = level.time;
	return qtrue;
}

// an alt shock at B every 1.5 s on a fixed schedule, so the step rate does not depend on the tick
static void TTF_Demp2(void) {
	gentity_t *a = TT_A(), *shock;

	if (level.time < TT.timer[0] || TT_GetEnt(0))
		return;
	TT.timer[0] += 1500;

	TT_GiveWeapon(a, WP_DEMP2);
	a->client->ps.weaponChargeTime = level.time - 1500; // two charge units
	TT_MarkEntities();
	FireWeapon(a, qtrue);
	shock = TT_TrackNew("demp2_alt_proj");
	if (!shock) {
		TT_Fail("demp2 alt did not spawn");
		return;
	}
	TT_SetEnt(0, shock);
}

static qboolean TTS_Bounce(void) {
	vec3_t end;
	trace_t tr;

	if (!TT_PlacePair(300, 332, 0)) // B out of the way
		return qfalse;

	// fired straight down from A's eye at the floor
	VectorCopy(TT_A()->client->ps.origin, TT.point);
	TT.point[2] += TT_A()->client->ps.viewheight;
	VectorCopy(TT.point, end);
	end[2] -= 1024;
	trap_Trace(&tr, TT.point, NULL, NULL, end, TT_A()->s.number, MASK_SHOT);
	if (tr.startsolid || tr.fraction == 1.0f || (tr.surfaceFlags & SURF_NOIMPACT))
		return TT_Fail("no floor under A");

	TT.timer[0] = level.time;
	return qtrue;
}

// a bouncing bolt at the floor every second on a fixed schedule, one bounce each
static void TTF_Bounce(void) {
	gentity_t *bolt;
	vec3_t down;

	if (level.time < TT.timer[0] || TT_GetEnt(0))
		return;
	TT.timer[0] += 1000;

	TT_MarkEntities();
	VectorSet(down, 0, 0, -1);
	bolt = CreateMissile(TT.point, down, 1500, 900, TT_A(), qfalse);
	bolt->classname = "blaster_proj";
	bolt->s.weapon = WP_BLASTER;
	bolt->damage = 0;
	bolt->methodOfDeath = MOD_BLASTER;
	bolt->clipmask = MASK_SHOT;
	bolt->flags |= FL_BOUNCE;
	bolt->bounceCount = 1;
	TT_TrackNew(NULL);
	TT_SetEnt(0, bolt);
}

/*
============================================================================
map hazards
============================================================================
*/

static gentity_t *TT_SpawnSynthetic(const char *classname) {
	gentity_t *ent = G_Spawn();
	ent->classname = (char *)classname;
	TT_Track(ent);
	TT_SetEnt(0, ent);
	return ent;
}

static qboolean TTS_Crusher(void) {
	gentity_t *door;

	if (!TT_PlacePair(64, 128, 0))
		return qfalse;

	door = TT_SpawnSynthetic("func_door");
	door->damage = 2;
	door->spawnflags = TT_MOVER_CRUSHER;
	return qtrue;
}

// a mover calls its blocked function every frame something is in the way
static void TTF_Crusher(void) {
	gentity_t *door = TT_GetEnt(0);
	if (door)
		Blocked_Door(door, TT_B(), NULL);
}

static qboolean TTS_Hurt(void) {
	gentity_t *hurt;

	if (!TT_PlacePair(64, 128, 0))
		return qfalse;

	hurt = TT_SpawnSynthetic("trigger_hurt");
	hurt->damage = 5;
	hurt->r.contents = CONTENTS_TRIGGER;
	hurt->r.svFlags = SVF_NOCLIENT;
	return qtrue;
}

// the engine calls touch every frame a client is inside the trigger
static void TTF_Hurt(void) {
	gentity_t *hurt = TT_GetEnt(0);

	TT_SetHealth(TT_B(), 100); // trigger_hurt damage ignores godmode
	if (hurt)
		hurt_touch(hurt, TT_B(), NULL);
}

static qboolean TTS_Laser(void) {
	gentity_t *laser;
	vec3_t p;

	if (!TT_PlacePair(300, 332, 0))
		return qfalse;

	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);
	TT_Along(32, p);
	laser = TT_SpawnSynthetic("target_laser");
	G_SetOrigin(laser, p);
	VectorCopy(p, laser->s.origin);
	laser->s.eType = ET_BEAM;
	laser->enemy = TT_B();
	laser->damage = 2;
	laser->activator = laser;
	laser->think = target_laser_think;
	laser->nextthink = level.time;
	trap_LinkEntity(laser);

	if (!TT_Visible(p, laser->s.number, TT_B()))
		return TT_Fail("no line of sight after placement");
	return qtrue;
}

static qboolean TT_SetupTrigger(float wait) {
	gentity_t *trigger;

	if (!TT_PlacePair(64, 128, 0))
		return qfalse;

	trigger = TT_SpawnSynthetic("trigger_multiple");
	trigger->wait = wait;
	trigger->touch = Touch_Multi;
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->r.svFlags = SVF_NOCLIENT;
	return qtrue;
}

static qboolean TTS_Trigger(void) {
	return TT_SetupTrigger(0);
}

static qboolean TTS_TriggerWait(void) {
	return TT_SetupTrigger(0.5f);
}

static void TTF_Trigger(void) {
	gentity_t *trigger = TT_GetEnt(0);
	if (trigger)
		Touch_Multi(trigger, TT_B(), NULL);
}

static gentity_t *TT_PathCorner(vec3_t origin) {
	gentity_t *corner = TT_SpawnSynthetic("path_corner");
	G_SetOrigin(corner, origin);
	VectorCopy(origin, corner->s.origin);
	return corner;
}

/*
a func_train looping between two path_corners with no waits, well above the bot so it pushes nothing. its 333 ms
legs are off the 50 ms grid, so each leg's start shows whether it waits for the frame that noticed the last one end
*/
static qboolean TTS_Mover(void) {
	gentity_t *train, *a, *b;
	vec3_t p;

	VectorCopy(TT_A()->client->ps.origin, p);
	p[2] += 1024;
	a = TT_PathCorner(p);
	p[0] += 100;
	b = TT_PathCorner(p);
	a->nextTrain = b;
	b->nextTrain = a;

	train = TT_SpawnSynthetic("func_train");
	train->s.eType = ET_MOVER;
	train->speed = 300;
	train->reached = Reached_Train;
	train->nextTrain = a;
	train->s.pos.trType = TR_STATIONARY;
	G_SetOrigin(train, a->s.origin);
	Reached_Train(train); // starts the first leg, as Think_SetupTrainTargets does
	return qtrue;
}

/*
a linear door (333 ms legs, no wait) opened as soon as it is shut and reversed 150 ms into each opening, so every
arrival is the end of a leg re-anchored mid-move
*/
static qboolean TTS_MoverReverse(void) {
	gentity_t *door;

	door = TT_SpawnSynthetic("func_door");
	VectorCopy(TT_A()->client->ps.origin, door->pos1);
	door->pos1[2] += 1024;
	VectorCopy(door->pos1, door->pos2);
	door->pos2[0] += 100;
	door->s.eType = ET_MOVER;
	door->speed = 300;
	door->alt_fire = qtrue; // TR_LINEAR_STOP
	door->moverState = MOVER_POS1;
	door->reached = Reached_BinaryMover;
	G_SetOrigin(door, door->pos1); // before InitMoverTrData, which it would undo
	InitMoverTrData(door);
	trap_LinkEntity(door);
	return qtrue;
}

static void TTF_MoverReverse(void) {
	gentity_t *door = TT_GetEnt(0);

	if (!door)
		return;
	if (door->moverState == MOVER_POS1)
		Use_BinaryMover_Go(door);
	else if (door->moverState == MOVER_1TO2 && level.time >= door->s.pos.trTime + 150)
		Use_BinaryMover_Go(door); // reverses partway
}

/*
============================================================================
npc and saber
============================================================================
*/

static qboolean TTS_Npc(void) {
	gentity_t *a, *npc;

	if (!TT_PlacePair(500, 532, 0))
		return qfalse;

	a = TT_A();
	TT_REQUIRE_LOS(a, TT_B());
	a->flags |= FL_NOTARGET;
	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);

	TT_MarkEntities();
	npc = NPC_SpawnType(a, "stormtrooper", NULL, qfalse);
	TT_TrackNew(NULL);
	if (!npc)
		return TT_Fail("npc did not spawn");
	TT_SetEnt(0, npc);
	return qtrue;
}

static void TTF_Npc(void) {
	gentity_t *npc = TT_GetEnt(0), *b = TT_B();

	if (!npc || npc->s.eType != ET_NPC) {
		TT_Fail("npc is gone");
		return;
	}
	if (npc->NPC && npc->health > 0 && npc->enemy != b)
		G_SetEnemy(npc, b);
}

static void TTT_Npc(void) {
	gentity_t *npc = TT_GetEnt(0);
	if (npc && npc->s.eType == ET_NPC)
		G_FreeEntity(npc);
}

static qboolean TT_SetupSaber(float dist) {
	gentity_t *a, *b;

	if (!TT_PlacePair(dist, dist + 32, 0))
		return qfalse;

	a = TT_A();
	b = TT_B();
	TT_GiveWeapon(a, WP_SABER);
	TT_GiveWeapon(b, WP_SABER);
	a->client->ps.saberHolstered = 0;
	b->client->ps.saberHolstered = 0;
	TT_GivePower(a, FP_SABER_OFFENSE, FORCE_LEVEL_3);
	a->client->ps.fd.saberAnimLevelBase = a->client->ps.fd.saberAnimLevel = a->client->ps.fd.saberDrawAnimLevel = SS_STRONG;
	TT.botButtons[TT_BOT_A] = BUTTON_ATTACK;
	TT_Pin(TT_BOT_B, b->client->ps.origin);
	return qtrue;
}

static qboolean TTS_Saber(void) {
	return TT_SetupSaber(64);
}

/*
with d_saberSPStyleDamage only real blade-on-blade contact reaches the clash branches, and an idle
defender rarely puts its blade in the way, so B is kept cycling through parry poses
*/
static qboolean TTS_SaberClash(void) {
	if (!TT_SetupSaber(80))
		return qfalse;
	TT_GivePower(TT_B(), FP_SABER_DEFENSE, FORCE_LEVEL_3);
	return qtrue;
}

static int ttSavedStance;

/*
A throws its saber (throw level 3) toward B every 2.5 s and lets go of alt attack 850 ms into the throw. B is 1000
units out, beyond the saber's reach in that time, so every return is the timed one on release: a hit would start it
from the per-frame held-blade contact check (damage accounting) at a frame-rate dependent moment
*/
static qboolean TT_SetupSaberThrow(float dist) {
	gentity_t *a, *b;

	if (!TT_PlacePair(dist, dist + 32, 0))
		return qfalse;

	a = TT_A();
	b = TT_B();
	TT_REQUIRE_LOS(a, b);
	TT_GiveWeapon(a, WP_SABER);
	// the class sabers may not be throwable and a staff stance kicks on alt attack: a single saber, stance unforced
	TT.savedClass = -1;
	if (a->client->siegeClass >= 0) {
		TT.savedClass = a->client->siegeClass;
		ttSavedStance = bgSiegeClasses[TT.savedClass].saberStance;
		bgSiegeClasses[TT.savedClass].saberStance = 0;
	}
	G_SetSaber(a, 0, "Kyle", qtrue);
	G_SetSaber(a, 1, "none", qtrue);
	G_SaberModelSetup(a);
	a->client->ps.fd.saberAnimLevelBase = a->client->ps.fd.saberAnimLevel = a->client->ps.fd.saberDrawAnimLevel = SS_MEDIUM;
	a->client->ps.saberHolstered = 0;
	TT_GivePower(a, FP_SABER_OFFENSE, FORCE_LEVEL_3);
	TT_GivePower(a, FP_SABERTHROW, FORCE_LEVEL_3);
	TT_GiveWeapon(b, WP_SABER);
	b->client->ps.saberHolstered = 2;
	TT_Pin(TT_BOT_B, b->client->ps.origin);
	return qtrue;
}

static qboolean TTS_SaberThrow(void) {
	return TT_SetupSaberThrow(1000);
}

// probe-only: B 400 units out, in reach, so the throw hits it and returns from the held-blade contact check
static qboolean TTS_SaberThrowHit(void) {
	return TT_SetupSaberThrow(400);
}

static void TTF_SaberThrow(void) {
	gclient_t *cl = TT_A()->client;
	qboolean hold;

	TT_FillForce(TT_A(), 100);
	// released 850 ms into the throw, between two 100 ms thinks, so the frame the throw started on cannot move the
	// return to another think
	if (cl->ps.saberInFlight)
		hold = level.time - cl->ps.saberDidThrowTime < 850;
	else
		hold = (level.time - TT.startTime) % 2500 < 1000;
	TT.botButtons[TT_BOT_A] = hold ? BUTTON_ALT_ATTACK : 0;
}

// back to the class stance and sabers, as ClientUserinfoChanged sets them
static void TTT_SaberThrow(void) {
	gentity_t *a = TT_A();
	siegeClass_t *scl;

	if (TT.savedClass >= 0)
		bgSiegeClasses[TT.savedClass].saberStance = ttSavedStance;
	TT.savedClass = -1;
	if (!TT_BotValid(TT_BOT_A) || a->client->siegeClass < 0)
		return;
	scl = &bgSiegeClasses[a->client->siegeClass];
	G_SetSaber(a, 0, scl->saber1[0] ? scl->saber1 : "Kyle", qtrue);
	G_SetSaber(a, 1, scl->saber2[0] ? scl->saber2 : "none", qtrue);
	G_SaberModelSetup(a);
}

/*
============================================================================
saber matrix

A swings from the ready pose at B on every chance it gets: one swing per return to ready, so each
saber_swing line opens one swing. B's saber is off, so every contact is blade on body. cells are
saber_<stance>_<swing>_<distance>; the saberduel scenarios put A's medium swings against B's saber
============================================================================
*/

#define TT_SABER_REACH		64
#define TT_SABER_TIP		72	// the ctf matrix's tip (its farthest medium l2r/tl2br hit at sv_fps 20), kept for comparison
#define TT_LOCK_MASH		100		// attack press period while saber locked

typedef struct {
	int			style;
	const char	*saber1, *saber2;
} ttSaberStance_t;

// the dual_N sabers are staffs
static const ttSaberStance_t ttSaberStances[] = {
	{ SS_FAST,		"single_1",	"none" },
	{ SS_MEDIUM,	"single_1",	"none" },
	{ SS_STRONG,	"single_1",	"none" },
	{ SS_DUAL,		"single_1",	"single_1" },
	{ SS_STAFF,		"dual_1",	"none" },
};

typedef struct {
	int			forwardmove, rightmove, buttons;
} ttSaberSwing_t;

// movement picks the attack out of the ready pose; attack + alt attack standing still is the stance's kata
static const ttSaberSwing_t ttSaberSwings[] = {
	{ 127,	127,	BUTTON_ATTACK },						// tl2br
	{ 0,	127,	BUTTON_ATTACK },						// l2r
	{ 0,	0,		BUTTON_ATTACK },						// t2b
	{ 0,	0,		BUTTON_ATTACK | BUTTON_ALT_ATTACK },	// special
};

static const float ttSaberDists[] = { TT_SABER_REACH, TT_SABER_TIP };

#define TT_SWING_TL2BR	0
#define TT_SWING_L2R	1

typedef struct {
	const ttSaberSwing_t	*swing;
	int						count;
	int						prevMove;
	qboolean				pressed;	// the swing buttons went out since the frame hook last looked
	qboolean				logMove;	// the attack the swing became is not logged yet
	qboolean				armed;
	int						armedAt;	// no press before this
	int						readySince;	// command time since which LS_READY && weaponTime <= 0 has held (-1: not ready)
	int			pressAt;		// command time of the latest press (simulated clients arm the duel follower from it)
} ttSwinger_t;

static ttSwinger_t ttSwing[2];

// what a cell lifts for its run: the class stance restriction and swing damage multiplier, and the session stance
static int ttCellClass[2];
static int ttCellClassStance[2];
static float ttCellClassSwingDmg[2];
static int ttStyleWas[2][4];	// sess.saberLevel, saberAnimLevel, saberAnimLevelBase, saberDrawAnimLevel
static qboolean ttStyleSaved[2];

static void TT_LiftClass(int idx) {
	int cls = TT_Bot(idx)->client->siegeClass;

	if (cls < 0 || (idx == TT_BOT_B && ttCellClass[TT_BOT_A] == cls))
		return;
	ttCellClass[idx] = cls;
	ttCellClassStance[idx] = bgSiegeClasses[cls].saberStance;
	ttCellClassSwingDmg[idx] = bgSiegeClasses[cls].swingDmg;
	G_Printf("ticktest: bot %d class %s, saberstance %d, swingdmg %g (lifted for the cell)\n", idx,
		bgSiegeClasses[cls].name, ttCellClassStance[idx], ttCellClassSwingDmg[idx]);
	bgSiegeClasses[cls].saberStance = 0;
	bgSiegeClasses[cls].swingDmg = 1.0f;
}

// back to the class stance rules and sabers, as ClientUserinfoChanged sets them; the harness respawns the bots after this
static void TTT_SaberCell(void) {
	int idx;

	for (idx = 0; idx < 2; idx++) {
		gentity_t *ent = TT_Bot(idx);
		siegeClass_t *scl;

		if (ttCellClass[idx] >= 0) {
			bgSiegeClasses[ttCellClass[idx]].saberStance = ttCellClassStance[idx];
			bgSiegeClasses[ttCellClass[idx]].swingDmg = ttCellClassSwingDmg[idx];
			ttCellClass[idx] = -1;
		}
		if (!TT_BotValid(idx))
			continue;
		if (ttStyleSaved[idx]) {
			ent->client->sess.saberLevel = ttStyleWas[idx][0];
			ent->client->ps.fd.saberAnimLevel = ttStyleWas[idx][1];
			ent->client->ps.fd.saberAnimLevelBase = ttStyleWas[idx][2];
			ent->client->ps.fd.saberDrawAnimLevel = ttStyleWas[idx][3];
		}
		if (ent->client->siegeClass < 0)
			continue;
		scl = &bgSiegeClasses[ent->client->siegeClass];
		G_SetSaber(ent, 0, scl->saber1[0] ? scl->saber1 : "Kyle", qtrue);
		G_SetSaber(ent, 1, scl->saber2[0] ? scl->saber2 : "none", qtrue);
		G_SaberModelSetup(ent);
	}
	memset(ttStyleSaved, 0, sizeof(ttStyleSaved));
	memset(ttSwing, 0, sizeof(ttSwing));
}

// the class sabers and stance are set directly: in siege a respawn keeps the sabers and userinfo never sets them
static void TT_ArmSaber(int idx, const ttSaberStance_t *st, qboolean lit) {
	gentity_t *ent = TT_Bot(idx);
	gclient_t *cl = ent->client;

	TT_LiftClass(idx);
	if (!ttStyleSaved[idx]) {
		ttStyleWas[idx][0] = cl->sess.saberLevel;
		ttStyleWas[idx][1] = cl->ps.fd.saberAnimLevel;
		ttStyleWas[idx][2] = cl->ps.fd.saberAnimLevelBase;
		ttStyleWas[idx][3] = cl->ps.fd.saberDrawAnimLevel;
		ttStyleSaved[idx] = qtrue;
	}
	TT_GiveWeapon(ent, WP_SABER);
	TT_GivePower(ent, FP_SABER_OFFENSE, FORCE_LEVEL_3);
	if (lit)
		TT_GivePower(ent, FP_SABER_DEFENSE, FORCE_LEVEL_3);
	G_SetSaber(ent, 0, (char *)st->saber1, qtrue);
	G_SetSaber(ent, 1, (char *)st->saber2, qtrue);
	G_SaberModelSetup(ent);
	if (st->style != SS_DUAL && st->style != SS_STAFF)
		cl->sess.saberLevel = st->style;
	cl->ps.fd.saberAnimLevelBase = cl->ps.fd.saberAnimLevel = cl->ps.fd.saberDrawAnimLevel = st->style;
	cl->ps.saberHolstered = lit ? 0 : 2;
	cl->ps.saberMove = LS_READY;
}

static void TT_PinOnFloor(int idx) {
	gentity_t *ent = TT_Bot(idx);
	trace_t tr;
	vec3_t down;

	VectorCopy(ent->client->ps.origin, down);
	down[2] -= 64;
	trap_Trace(&tr, ent->client->ps.origin, ttHullMins, ttHullMaxs, down, ent->s.number, MASK_PLAYERSOLID);
	if (!tr.startsolid && !tr.allsolid)
		TT_SetPos(ent, tr.endpos);
	TT_Pin(idx, ent->client->ps.origin);
}

static qboolean TT_SetupSaberPair(const ttSaberStance_t *st, float dist, qboolean bLit) {
	gentity_t *a, *b;

	ttCellClass[TT_BOT_A] = ttCellClass[TT_BOT_B] = -1; // first, so the teardown is safe whatever fails below
	memset(ttSwing, 0, sizeof(ttSwing));
	if (!TT_PlacePair(dist, dist + 32, 0))
		return qfalse;

	a = TT_A();
	b = TT_B();
	TT_REQUIRE_LOS(a, b);
	TT_ArmSaber(TT_BOT_A, st, qtrue);
	TT_ArmSaber(TT_BOT_B, &ttSaberStances[1], bLit);
	// both pinned on the floor (placement leaves them in the air), so walking never adds the velocity lead
	TT_PinOnFloor(TT_BOT_A);
	TT_PinOnFloor(TT_BOT_B);
	ttSwing[TT_BOT_A].readySince = ttSwing[TT_BOT_B].readySince = -1;
	ttSwing[TT_BOT_A].armed = qtrue;
	return qtrue;
}

/*
grid-tick brain for the saber scenarios. with one command per frame (cmdMsec 0) every command decides, from the
state the previous command's pmove left: the per-frame bot, whose one-frame reaction is 50 ms at sv_fps 20. with
sub-frame commands only the commands on the 50 ms grid decide, from the state that held at c - 50, so the bot keeps
that 20 Hz timing at any sv_fps (at 20 Hz both forms name the same time)
*/
// tt_phase: with sub-frame commands, every command and decision this many ms after the 50 ms grid instead of on it
static int ttPhase;
static int ttHuman;				// tt_hseed: the bots are simulated clients (TT_ClientNextPacket)
static int ttDecideOrigin[2];		// a simulated client decides when its own 50 ms clock, from here, ticks
static int ttPrevCmd[2];			// serverTime of the bot's previous command

static qboolean TT_Aligned(int idx, int c) {
	if (ttHuman) // the first command of each of the client's own 50 ms steps
		return ( c - ttDecideOrigin[idx] ) / TICK_BASE_MSEC != ( ttPrevCmd[idx] - ttDecideOrigin[idx] ) / TICK_BASE_MSEC;
	return !TT.cmdMsec || ( c - ttPhase ) % TICK_BASE_MSEC == 0;
}

// time of the state a decision for the command at c is taken from
static int TT_ObsTime(int idx, int c) {
	return TT.cmdMsec ? c - TICK_BASE_MSEC : TT_Bot(idx)->client->ps.commandTime;
}

/*
an aligned decision holds for the whole grid step, as the 50 ms command carrying it does on a 20 Hz server: the
buttons and movement stay on the commands up to the next aligned one (pmove reads a command without the attack
button as a release that clears weaponTime). a forced parry pose is set once: the pmove consuming it holds the parry
by weaponTime, and setting it again would restart that hold
*/

/*
per command, before its pmove: readiness is tracked from the state every command sees; the aligned commands decide
whether the swing buttons and movement go out, from the observation time (the first such command starts the swing;
a refused press repeats on the next)
*/
static void TT_CmdSwing(int idx, int c) {
	playerState_t *ps = &TT_Bot(idx)->client->ps;
	ttSwinger_t *s = &ttSwing[idx];
	int obs = TT_ObsTime(idx, c);
	qboolean press;

	// the frame hook first looks at the end of the frame after arming; the arming frame's state is not observed
	if (ps->commandTime > TT.startTime) {
		if (ps->saberMove == LS_READY && ps->weaponTime <= 0) {
			if (s->readySince < 0)
				s->readySince = ps->commandTime;
		}
		else {
			s->readySince = -1;
		}
	}

	if (!TT_Aligned(idx, c))
		return;

	if (ps->saberLockTime > obs) { // nobody would push otherwise
		TT.botButtons[idx] = ((obs - TT.startTime) / TT_LOCK_MASH) % 2 ? BUTTON_ATTACK : 0;
		ttBotMove[idx][0] = ttBotMove[idx][1] = 0;
		return;
	}

	press = s->armed && s->armedAt <= obs && s->readySince >= 0 && s->readySince <= obs;
	if (press) {
		s->pressed = qtrue;
		s->pressAt = c;
	}
	TT.botButtons[idx] = press ? s->swing->buttons : 0;
	ttBotMove[idx][0] = press ? s->swing->forwardmove : 0;
	ttBotMove[idx][1] = press ? s->swing->rightmove : 0;
}

// a held attack button goes out on every command
static void TT_CmdHoldAttack(int idx, int c) {
	TT.botButtons[idx] = BUTTON_ATTACK;
}

/*
B parries whenever it is back in the ready pose: the pose is forced right before an aligned command's pmove, which
turns the pending saberBlocked into the parry move
*/
static void TT_CmdForceParry(int idx, int c, int pose, qboolean unlessLocked) {
	playerState_t *ps = &TT_Bot(idx)->client->ps;

	if (TT_Aligned(idx, c) && ps->saberMove == LS_READY && !ps->saberBlocked && (!unlessLocked || ps->saberLockTime <= TT_ObsTime(idx, c)))
		ps->saberBlocked = pose;
}

static void TTC_Saber(int idx, int c) {
	if (idx == TT_BOT_A)
		TT_CmdHoldAttack(idx, c);
}

static void TTC_SaberClash(int idx, int c) {
	static const int poses[] = { BLOCKED_TOP, BLOCKED_UPPER_RIGHT, BLOCKED_UPPER_LEFT };

	if (idx == TT_BOT_A)
		TT_CmdHoldAttack(idx, c);
	else
		TT_CmdForceParry(idx, c, poses[((TT_ObsTime(idx, c) - TT.startTime) / 600) % 3], qfalse);
}

/*
per frame, after the frame's pmoves: a swing starts when the bot leaves the ready pose on our press, and the
state fixes here are the ones the next frame's skeleton update must see. the press itself is TT_CmdSwing's
*/
// the command time of the frame's latest aligned command: with sub-frame commands a frame can be a few ms after it
static int TT_CmdTimeNow(void) {
	return TT.cmdMsec ? level.time - ( level.time - ttPhase ) % TICK_BASE_MSEC : level.time;
}

static void TT_DriveSwing(int idx) {
	gentity_t *ent = TT_Bot(idx);
	playerState_t *ps = &ent->client->ps;
	ttSwinger_t *s = &ttSwing[idx];
	int move = ps->saberMove;

	if (ps->saberInFlight)
		TT_RecallSaber(ent);
	TT_FillForce(ent, 100);

	if (s->pressed && s->prevMove == LS_READY && move != LS_READY && !PM_SaberInParry(move) &&
		!PM_SaberInKnockaway(move) && !PM_SaberInBrokenParry(move) && !PM_SaberInBounce(move)) {
		s->count++;
		s->armed = qfalse;
		s->logMove = qtrue;
		TICKPROBE("saber_swing", ent->s.number, s->count);
	}
	s->pressed = qfalse;
	if (s->logMove && !PM_SaberInStart(move)) {
		TICKPROBE("saber_move", ent->s.number, move);
		TICKPROBE("saber_style", ent->s.number, ps->fd.saberAnimLevel);
		s->logMove = qfalse;
	}
	s->prevMove = move;

	// a finished kata leaves its torso anim up, which refuses the next kata; idling would put the stance back
	if (ps->saberLockTime <= level.time && move == LS_READY && ps->torsoTimer <= 0 && BG_InKataAnim(ps->torsoAnim))
		ps->torsoAnim = ps->legsAnim;
}

static qboolean TTS_SaberCell(void) {
	int cell = TT_ScenarioArg();
	float dist = ttSaberDists[cell % 10];

	if (!TT_SetupSaberPair(&ttSaberStances[cell / 100], dist, qfalse))
		return qfalse;
	ttSwing[TT_BOT_A].swing = &ttSaberSwings[(cell / 10) % 10];
	return qtrue;
}

static void TTF_SaberCell(void) {
	ttSwing[TT_BOT_A].armed = qtrue;
	TT_DriveSwing(TT_BOT_A);
}

static void TTC_SaberCell(int idx, int c) {
	if (idx == TT_BOT_A)
		TT_CmdSwing(idx, c);
}

// B parries upper right, into A's tl2br, whenever it is back in the ready pose
static qboolean TTS_SaberDuelBlock(void) {
	if (!TT_SetupSaberPair(&ttSaberStances[1], TT_SABER_REACH, qtrue))
		return qfalse;
	ttSwing[TT_BOT_A].swing = &ttSaberSwings[TT_SWING_TL2BR];
	return qtrue;
}

static void TTF_SaberDuelBlock(void) {
	gentity_t *b = TT_B();

	ttSwing[TT_BOT_A].armed = qtrue;
	TT_DriveSwing(TT_BOT_A);
	if (b->client->ps.saberInFlight)
		TT_RecallSaber(b);
	TT_FillForce(b, 100);
}

static void TTC_SaberDuelBlock(int idx, int c) {
	if (idx == TT_BOT_A)
		TT_CmdSwing(idx, c);
	else
		TT_CmdForceParry(idx, c, BLOCKED_UPPER_RIGHT, qtrue);
}

/*
both swing: with a delay of 0 or more B's swing is held until that long after A's latest swing started, with a
negative one B leads and A follows
*/
static qboolean TT_SetupSaberDuelSwing(int swing) {
	if (!TT_SetupSaberPair(&ttSaberStances[1], TT_SABER_REACH, qtrue))
		return qfalse;
	ttSwing[TT_BOT_A].swing = ttSwing[TT_BOT_B].swing = &ttSaberSwings[swing];
	if (TT_ScenarioArg() < 0)
		ttSwing[TT_BOT_A].armed = qfalse;
	return qtrue;
}

static qboolean TTS_SaberDuelSwing(void) {
	return TT_SetupSaberDuelSwing(TT_SWING_TL2BR);
}

// tl2br against tl2br never meets blade on blade here; level l2r swings cross in front of both bots
static qboolean TTS_SaberDuelL2R(void) {
	return TT_SetupSaberDuelSwing(TT_SWING_L2R);
}

static void TTF_SaberDuelSwing(void) {
	int delay = TT_ScenarioArg();
	int lead = delay >= 0 ? TT_BOT_A : TT_BOT_B, follow = lead == TT_BOT_A ? TT_BOT_B : TT_BOT_A;
	int before = ttSwing[lead].count;

	ttSwing[lead].armed = qtrue;
	TT_DriveSwing(lead);
	if (ttSwing[lead].count != before && !ttHuman) {
		ttSwing[follow].armed = qtrue;
		ttSwing[follow].armedAt = TT_CmdTimeNow() + abs(delay);
	}
	TT_DriveSwing(follow);
}

/*
simulated clients: the follower is armed by the command carrying the leader's press, not by the frame that sees
the swing start, so when it may answer does not depend on the frame rate
*/
static void TTC_SaberDuelSwing(int idx, int c) {
	int delay = TT_ScenarioArg(), lead = delay >= 0 ? TT_BOT_A : TT_BOT_B;

	TT_CmdSwing(idx, c);
	if (ttHuman && idx == lead && ttSwing[idx].pressed && ttSwing[idx].pressAt == c) {
		ttSwing[lead == TT_BOT_A ? TT_BOT_B : TT_BOT_A].armed = qtrue;
		ttSwing[lead == TT_BOT_A ? TT_BOT_B : TT_BOT_A].armedAt = c + abs(delay);
	}
}

/*
============================================================================
saber movement scenarios

what a real player does that the standing bots never do: turn the view while swinging, spin at cl_yawspeed 3000
(3000 degrees per second), strafe, swing at a target that strafes or spins, and send commands in irregular bursts.
every input is a function of the command time since the clock started, so a run is the same at every sv_fps and
on every repeat; they are meant for sub-frame commands (cmdmsec 10, a 100 fps client), the 20 Hz reference too.
the arg says what moves: TT_MV_* bits
============================================================================
*/

#define TT_MV_LAG_A			1		// A's commands arrive in bursts (TT_LAG_*)
#define TT_MV_TURN_A		2		// A's view swings left and right while it swings (TT_TURN_*)
#define TT_MV_SPIN_A		4		// A holds attack and spins at TT_SPIN_RATE (the spin tactic)
#define TT_MV_STRAFE_A		8		// A strafes left and right (TT_STRAFE_HALF)
#define TT_MV_STRAFE_B		16		// B strafes left and right
#define TT_MV_SPIN_B		32		// B spins at TT_SPIN_RATE
#define TT_MV_DUEL			64		// both lit and swinging, B follows A's swings

#define TT_SPIN_RATE		3000	// degrees per second: +left with cl_yawspeed 3000
#define TT_TURN_AMP			60		// a mouse swing: +-60 degrees at 750 degrees per second
#define TT_TURN_PERIOD		320
#define TT_STRAFE_HALF		400		// ms per strafe direction; the first leg is half as long, so the strafe stays centered
#define TT_LAG_PHASE		7		// first packet this long after the clock starts, off the 50 ms grid
#define TT_LAG_CYCLE		300		// packet gaps 40, 90, 20, 150 ms, repeating

static float ttBotYaw[2];			// view yaw offset of the next scripted command, degrees
static qboolean ttBotLag[2];
static qboolean ttMvUnpin[2];
static int ttMvSeen;				// A's swing count the duel follower was last released for

/*
a swing is counted per command, not per frame: within one burst of lagged commands a bot can finish a swing and
start the next without a frame seeing it back in the ready pose. the press's own pmove has run when the next
command is decided, so that command sees whether the press started a swing
*/
static void TT_MvCountSwing(int idx) {
	gentity_t *ent = TT_Bot(idx);
	ttSwinger_t *s = &ttSwing[idx];
	int move = ent->client->ps.saberMove;

	if (s->pressed) {
		if (s->prevMove == LS_READY && move != LS_READY && !PM_SaberInParry(move) && !PM_SaberInKnockaway(move) &&
			!PM_SaberInBrokenParry(move) && !PM_SaberInBounce(move)) {
			s->count++;
			s->armed = qfalse;
			TICKPROBE("saber_swing", ent->s.number, s->count);
		}
		s->pressed = qfalse;
	}
	s->prevMove = move;
}

// TT_DriveSwing's upkeep without its per-frame swing count
static void TT_MvUpkeep(int idx) {
	gentity_t *ent = TT_Bot(idx);
	playerState_t *ps = &ent->client->ps;

	if (ps->saberInFlight)
		TT_RecallSaber(ent);
	TT_FillForce(ent, 100);
	if (ps->saberLockTime <= level.time && ps->saberMove == LS_READY && ps->torsoTimer <= 0 && BG_InKataAnim(ps->torsoAnim))
		ps->torsoAnim = ps->legsAnim;
}

static float TT_TurnYaw(int t) {
	int u = t % TT_TURN_PERIOD, q = TT_TURN_PERIOD / 4;
	float rate = 4.0f * TT_TURN_AMP / TT_TURN_PERIOD;	// degrees per ms

	if (u < q)
		return rate * u;
	if (u < 3 * q)
		return TT_TURN_AMP - rate * (u - q);
	return -TT_TURN_AMP + rate * (u - 3 * q);
}

static float TT_SpinYaw(int t) {
	return (float)(((long long)t * TT_SPIN_RATE) % 360000) / 1000.0f;
}

static int TT_StrafeMove(int t) {
	return ((t + TT_STRAFE_HALF / 2) / TT_STRAFE_HALF) % 2 ? -127 : 127;
}

// the latest packet arrival of a lagged bot at or before now; a packet carries the commands up to its arrival
static int TT_LagArrival(int now) {
	static const int offsets[] = { 0, 40, 130, 150 };
	int k, base, rem, i, off = -1;

	k = now - TT.startTime - ttPhase - TT_LAG_PHASE;
	if (k < 0)
		return TT.startTime;
	base = k - k % TT_LAG_CYCLE;
	rem = k % TT_LAG_CYCLE;
	for (i = 0; i < 4; i++) {
		if (offsets[i] <= rem)
			off = offsets[i];
	}
	return TT.startTime + ttPhase + TT_LAG_PHASE + base + off;
}

// the newest command time a test bot thinks with in this frame
int TickTest_CmdLimit(int idx) {
	if (idx < 0 || idx > 1 || !ttBotLag[idx])
		return level.time;
	return TT_LagArrival(level.time);
}

/*
cmdArrival: at the end of a frame the bots think with every command up to the next frame, as a real client's
commands are thought between frames, each stamped with its arrival (a lagged packet's commands all arrive with it).
the commands keep their times, so the run is the same at every sv_fps
*/
/*
tt_hseed (with arrival): each bot is a simulated client, with a frame rate, packet rate, latency, jitter and loss drawn
from its own generator (never the game's random streams). a command carries the client's clock, the latency behind the
server's; a packet is sent on the first client frame its packet interval allows and arrives latency plus jitter later,
carrying every command since the last packet that got through. everything is drawn in a fixed order, so a seed gives
the same commands and arrivals at every sv_fps
*/
#define TT_CLIENT_CMDS	64

typedef struct {
	unsigned	rng;
	float		frameMsec, packetMsec, nextFrame, nextSend;	// the clock from the scenario start, so its rounding is the same at every sv_fps
	int			latency, jitter, lossPermille;
	int			lastCmd;
	int			arrival, numCmds, cmds[TT_CLIENT_CMDS];	// the next packet that gets through
} ttClient_t;

static ttClient_t ttClient[2];
static int ttClientFps[2], ttClientLatency[2];	// a scenario's fixed client profile, 0 drawn

static unsigned TT_ClientRand(ttClient_t *cl) {
	cl->rng ^= cl->rng << 13;
	cl->rng ^= cl->rng >> 17;
	cl->rng ^= cl->rng << 5;
	return cl->rng;
}

static int TT_ClientRange(ttClient_t *cl, int lo, int hi) {
	return lo + (int)( TT_ClientRand(cl) % (unsigned)( hi - lo + 1 ) );
}

static void TT_ClientNextPacket(ttClient_t *cl) {
	int prev = cl->arrival;

	cl->numCmds = 0;
	for (;;) {
		float sent;

		do { // client frames up to the one that may send
			int c = TT.startTime + (int)cl->nextFrame - cl->latency;

			sent = cl->nextFrame;
			if (c <= cl->lastCmd)
				c = cl->lastCmd + 1;
			cl->lastCmd = c;
			if (cl->numCmds < TT_CLIENT_CMDS)
				cl->cmds[cl->numCmds++] = c;
			cl->nextFrame += cl->frameMsec + TT_ClientRange(cl, -500, 500) / 1000.0f;
		} while (sent < cl->nextSend);
		cl->nextSend = sent + cl->packetMsec;
		if (TT_ClientRange(cl, 0, 999) < cl->lossPermille && cl->numCmds < TT_CLIENT_CMDS / 2)
			continue; // lost: the next packet repeats its commands
		cl->arrival = TT.startTime + (int)sent + cl->latency + TT_ClientRange(cl, 0, cl->jitter);
		if (cl->arrival < prev)
			cl->arrival = prev; // the engine drops commands older than ones already run, so arrivals never go back
		return;
	}
}

static void TT_ClientInit(int idx, qboolean badLink) {
	static const int fps[] = { 60, 125, 144, 333 }, packets[] = { 30, 60, 100 };
	ttClient_t *cl = &ttClient[idx];
	int f, pk;

	memset(cl, 0, sizeof(*cl));
	cl->rng = (unsigned)ttHuman * 2654435761u ^ (unsigned)( idx + 1 ) * 0x9e3779b9u;
	if (!cl->rng)
		cl->rng = 1;
	f = fps[TT_ClientRange(cl, 0, 3)];
	pk = packets[TT_ClientRange(cl, 0, 2)];
	if (badLink)
		pk = 30;
	if (ttClientFps[idx])
		f = ttClientFps[idx];
	if (pk > f)
		pk = f;
	cl->frameMsec = 1000.0f / f;
	cl->packetMsec = 1000.0f / pk;
	cl->latency = TT_ClientRange(cl, 10, 80);
	if (ttClientLatency[idx])
		cl->latency = ttClientLatency[idx];
	cl->jitter = badLink ? 20 : TT_ClientRange(cl, 0, 20);
	cl->lossPermille = badLink ? 50 : TT_ClientRange(cl, 0, 50);
	// the first command's clock is at or after the start, so every input script starts at its own time 0
	cl->nextFrame = cl->latency + TT_ClientRange(cl, 0, 999) / 1000.0f * cl->frameMsec;
	cl->nextSend = cl->nextFrame;
	cl->lastCmd = TT.startTime;
	ttDecideOrigin[idx] = TT.startTime + TT_ClientRange(cl, 0, TICK_BASE_MSEC - 1);
	ttPrevCmd[idx] = TT.startTime;
	TT_ClientNextPacket(cl);
	G_LogPrintf("[tick] client idx=%d fps=%d packets=%d latency=%d jitter=%d loss=%d decide=%d\n", idx, f, pk, cl->latency, cl->jitter,
		cl->lossPermille, ttDecideOrigin[idx] - TT.startTime);
}

// the simulated clients' packets that arrive before the next frame, in arrival order, each command stamped with its packet's arrival
static void TT_ClientThinks(int bots) {
	int next = level.time + G_FrameMsec();

	for (;;) {
		int idx = -1, i, k;

		for (i = 0; i < bots; i++) {
			if (ttClient[i].arrival < next && ( idx < 0 || ttClient[i].arrival < ttClient[idx].arrival ))
				idx = i;
		}
		if (idx < 0)
			break;
		for (k = 0; k < ttClient[idx].numCmds; k++) {
			gentity_t *ent = TT_Bot(idx);
			usercmd_t run = ent->client->pers.cmd; // put back when held: the command run last

			TT.cmdStamp = Com_Clampi(level.time, next - 1, ttClient[idx].arrival);
			ent->client->pers.cmd.serverTime = ttClient[idx].cmds[k];
			if (G_HoldCmd(ent)) {
				ent->client->pers.cmd = run;
				continue;
			}
			TickTest_BotThink(TT.bot[idx], &ent->client->pers.cmd);
			ClientThink_real(ent);
			TickTest_ApplyPin(idx);
		}
		TT_ClientNextPacket(&ttClient[idx]);
	}
	TT.cmdStamp = 0;
	TT_ApplyPins();
}

static void TT_ArrivalThinks(int bots) {
	int next = level.time + G_FrameMsec(), idx;

	if (ttHuman) {
		TT_ClientThinks(bots);
		return;
	}

	for (idx = 0; idx < bots; idx++) {
		gentity_t *ent = TT_Bot(idx);
		int u, t = TT.botNextCmd[idx], steps = 0;

		// each packet arriving before the next frame, in order, with its own arrival (unlagged: one per command)
		for (u = level.time; u < next; u++) {
			int limit = ttBotLag[idx] ? TT_LagArrival(u) : u;

			if (limit != u)
				continue;
			for (; t <= limit && steps < 32; t += TT.cmdMsec, steps++) {
				usercmd_t run = ent->client->pers.cmd; // put back when held: the command run last

				TT.cmdStamp = ttBotLag[idx] ? limit : Com_Clampi(level.time, next - 1, t);
				ent->client->pers.cmd.serverTime = t;
				if (G_HoldCmd(ent)) {
					ent->client->pers.cmd = run;
					continue;
				}
				TickTest_BotThink(TT.bot[idx], &ent->client->pers.cmd);
				ClientThink_real(ent);
				TickTest_ApplyPin(idx);
			}
		}
		TT.cmdStamp = 0;
		TT.botNextCmd[idx] = t;
	}
	TT_ApplyPins();
}

static qboolean TTS_SaberMove(void) {
	int arg = TT_ScenarioArg();

	// medium stance against B at reach; B's saber is off (every contact is blade on body) unless it duels
	if (!TT_SetupSaberPair(&ttSaberStances[1], TT_SABER_REACH, (arg & TT_MV_DUEL) ? qtrue : qfalse))
		return qfalse;
	ttSwing[TT_BOT_A].swing = &ttSaberSwings[TT_SWING_TL2BR];
	if (arg & TT_MV_DUEL)
		ttSwing[TT_BOT_B].swing = &ttSaberSwings[TT_SWING_TL2BR];
	ttMvSeen = 0;
	// a strafer stays pinned through the settle, where bots think once per frame, and is let go by its first command
	ttMvUnpin[TT_BOT_A] = (arg & TT_MV_STRAFE_A) ? qtrue : qfalse;
	ttMvUnpin[TT_BOT_B] = (arg & TT_MV_STRAFE_B) ? qtrue : qfalse;
	ttBotLag[TT_BOT_A] = (arg & TT_MV_LAG_A) ? qtrue : qfalse;
	return qtrue;
}

static void TTF_SaberMove(void) {
	int arg = TT_ScenarioArg();

	TT_MvUpkeep(TT_BOT_A);
	ttSwing[TT_BOT_A].armed = qtrue;
	if (arg & TT_MV_DUEL) { // saberduel_swing's: B's swing is released with A's latest
		if (ttSwing[TT_BOT_A].count != ttMvSeen) {
			ttMvSeen = ttSwing[TT_BOT_A].count;
			if (!ttHuman) { // simulated clients are armed by the command that showed A's swing (TTC_SaberMove)
				ttSwing[TT_BOT_B].armed = qtrue;
				ttSwing[TT_BOT_B].armedAt = TT_CmdTimeNow();
			}
		}
		TT_MvUpkeep(TT_BOT_B);
	}
}

static void TTC_SaberMove(int idx, int c) {
	int arg = TT_ScenarioArg(), t = c - TT.startTime - ttPhase;

	if (ttMvUnpin[idx]) {
		TT.botPinned[idx] = qfalse;
		ttMvUnpin[idx] = qfalse;
	}
	if (idx == TT_BOT_A) {
		if (arg & TT_MV_SPIN_A) { // attack and a strafe held: level l2r swings, the blade swept round by the spin (A stays pinned)
			TT_CmdHoldAttack(idx, c);
			ttBotMove[idx][0] = 0;
			ttBotMove[idx][1] = 127;
			ttBotYaw[idx] = TT_SpinYaw(t);
		}
		else {
			int before = ttSwing[idx].count;

			TT_MvCountSwing(idx);
			if (ttSwing[idx].count != before && ttHuman && (arg & TT_MV_DUEL)) { // on this command, not the frame that sees it, so the follower's timing is the same at every rate
				ttSwing[TT_BOT_B].armed = qtrue;
				ttSwing[TT_BOT_B].armedAt = c;
			}
			TT_CmdSwing(idx, c);
		}
		if (arg & TT_MV_TURN_A)
			ttBotYaw[idx] = TT_TurnYaw(t);
		if (arg & TT_MV_STRAFE_A) { // the strafe picks the swing direction, as it does for a player
			ttBotMove[idx][0] = 0;
			ttBotMove[idx][1] = TT_StrafeMove(t);
		}
		return;
	}
	if (arg & TT_MV_DUEL) {
		TT_MvCountSwing(idx);
		TT_CmdSwing(idx, c);
	}
	if (arg & TT_MV_SPIN_B)
		ttBotYaw[idx] = TT_SpinYaw(t);
	if (arg & TT_MV_STRAFE_B) {
		ttBotMove[idx][0] = 0;
		ttBotMove[idx][1] = TT_StrafeMove(t);
	}
}

/*
============================================================================
simulated players (saberhsim_*, meant for tt_hseed with arrival)

two pilots fight as people do: they close to a preferred range and back off, circle-strafe, aim at the opponent with
a human turn rate and drag the view through their swings, attack in varied directions with varied press lengths,
chain, jump and lunge, and react to the opponent's swings after a reaction time (block, step back or counter). a pilot
sees its opponent only as sampled on its own commands, delayed by its round trip, so what it perceives does not depend
on sv_fps; its decisions draw from its own generator, never the game's or its network's
============================================================================
*/
#define TT_HS_AGGRO		1		// A attacks often and drags hard; B leans to defense
#define TT_HS_SPIN		2		// A spins with attack held when in range; B leans to defense
#define TT_HS_CHASE		4		// B runs and never attacks; A chases
#define TT_HS_SPECIALS	8		// A lunges and jump-attacks often
#define TT_HS_MIXED		16		// A a 60 fps client at 80 ms, B a 333 fps client at 10 ms
#define TT_HS_LOSSY		32		// both on bad links (30 packets per second, 20 ms jitter, 5% loss)
#define TT_HS_HIST		128

typedef struct {
	int			time;
	vec3_t		origin;
	int			saberMove;
} ttSeen_t;

typedef struct {
	unsigned	rng;
	float		baseYaw, yaw, dragRate;
	vec3_t		home;
	int			range, turnRate, reaction, attackMin, attackMax, blockPct, backPct, specialPct;
	int			lastCmd, lastSeenMove, reactAt, reactKind, reactUntil;
	int			strafeDir, strafeUntil, pressUntil, pressFwd, pressRight, pressUp, nextAttack, dragUntil, spinUntil, jumpUntil;
	int			numSeen, seenHead;
	ttSeen_t	seen[TT_HS_HIST];
} ttPilot_t;

static ttPilot_t ttPilot[2];

static int TT_HsRange(ttPilot_t *pl, int lo, int hi) {
	pl->rng ^= pl->rng << 13;
	pl->rng ^= pl->rng >> 17;
	pl->rng ^= pl->rng << 5;
	return lo + (int)( pl->rng % (unsigned)( hi - lo + 1 ) );
}

static void TT_HsInit(int idx) {
	ttPilot_t *pl = &ttPilot[idx];
	int arg = TT_ScenarioArg(), seed = ttHuman ? ttHuman : 1;
	gentity_t *ent = TT_Bot(idx);

	memset(pl, 0, sizeof(*pl));
	pl->rng = (unsigned)seed * 0x85ebca6bu ^ (unsigned)( idx + 1 ) * 0xc2b2ae35u ^ 0x5bd1e995u;
	if (!pl->rng)
		pl->rng = 1;
	pl->baseYaw = ent->client->ps.viewangles[YAW];
	VectorCopy(ent->client->ps.origin, pl->home);
	pl->range = TT_HsRange(pl, 50, 90);
	pl->turnRate = TT_HsRange(pl, 500, 1000);
	pl->reaction = TT_HsRange(pl, 180, 300);
	pl->attackMin = TT_HsRange(pl, 120, 300);
	pl->attackMax = pl->attackMin + TT_HsRange(pl, 100, 400);
	pl->blockPct = TT_HsRange(pl, 30, 60);
	pl->backPct = TT_HsRange(pl, 10, 30);
	pl->specialPct = TT_HsRange(pl, 2, 8);
	if (idx == TT_BOT_A && ( arg & TT_HS_AGGRO )) {
		pl->attackMin = 80;
		pl->attackMax = 250;
	}
	if (idx == TT_BOT_B && ( arg & ( TT_HS_AGGRO | TT_HS_SPIN ) )) {
		pl->blockPct = 70;
		pl->backPct = 20;
	}
	if (idx == TT_BOT_A && ( arg & TT_HS_SPECIALS ))
		pl->specialPct = 35;
	if (idx == TT_BOT_A && ( arg & TT_HS_CHASE ))
		pl->range = 45;
	pl->lastSeenMove = LS_READY;
	TT.botPinned[idx] = qfalse;
}

// the opponent as this pilot saw it at command time c, its round trip and a snapshot interval ago
static const ttSeen_t *TT_HsSeen(int idx, int c) {
	ttPilot_t *pl = &ttPilot[idx];
	gentity_t *opp = TT_Bot(!idx);
	ttSeen_t *sn = &pl->seen[pl->seenHead];
	int delay = ( ttHuman ? 2 * ttClient[idx].latency : 0 ) + TICK_BASE_MSEC, i;

	pl->seenHead = ( pl->seenHead + 1 ) % TT_HS_HIST;
	sn = &pl->seen[pl->seenHead];
	sn->time = c;
	VectorCopy(opp->client->ps.origin, sn->origin);
	sn->saberMove = opp->client->ps.saberMove;
	if (pl->numSeen < TT_HS_HIST)
		pl->numSeen++;
	for (i = 0; i < pl->numSeen; i++) {
		const ttSeen_t *old = &pl->seen[( pl->seenHead - i + TT_HS_HIST ) % TT_HS_HIST];

		if (old->time <= c - delay || i == pl->numSeen - 1)
			return old;
	}
	return sn;
}

// floor under the point dist ahead of ent in the direction the movement keys walk it (straight down for no keys)
static qboolean TT_HsFloorAhead(gentity_t *ent, float yaw, int fwd, int right, float dist) {
	vec3_t f, r, ahead, down, ang;
	trace_t tr;

	VectorSet(ang, 0, yaw, 0);
	AngleVectors(ang, f, r, NULL);
	VectorScale(f, fwd, ahead);
	VectorMA(ahead, right, r, ahead);
	ahead[2] = 0;
	VectorNormalize(ahead);
	VectorMA(ent->client->ps.origin, dist, ahead, ahead);
	VectorCopy(ahead, down);
	down[2] -= 80;
	trap_Trace(&tr, ahead, NULL, NULL, down, ent->s.number, MASK_PLAYERSOLID);
	return tr.fraction < 1.0f && !tr.startsolid;
}

static void TTC_Hsim(int idx, int c) {
	static const int dirs[8][2] = { { 127, 127 }, { 127, -127 }, { 0, 127 }, { 0, -127 }, { 127, 0 }, { -127, 127 }, { -127, -127 }, { -127, 0 } };
	ttPilot_t *pl = &ttPilot[idx];
	gentity_t *me = TT_Bot(idx);
	playerState_t *ps = &me->client->ps;
	const ttSeen_t *sn;
	int arg = TT_ScenarioArg(), dt, fwd = 0, right = 0, up = 0, buttons = 0;
	qboolean runner = idx == TT_BOT_B && ( arg & TT_HS_CHASE ), blocking = qfalse;
	vec3_t to;
	float dist, want, turn;

	if (!pl->rng)
		TT_HsInit(idx);
	dt = pl->lastCmd ? Com_Clampi(0, 100, c - pl->lastCmd) : 0;
	pl->lastCmd = c;
	VectorSubtract(pl->home, ps->origin, to);
	if (to[2] > 48 || sqrt(to[0] * to[0] + to[1] * to[1]) > 200) { // fell or was knocked off the ground around the placement: back to it
		TT_SetPos(me, pl->home);
		TICKPROBE("hsim_reset", me->s.number, 1);
	}
	TT_MvCountSwing(idx);
	sn = TT_HsSeen(idx, c);
	VectorSubtract(sn->origin, ps->origin, to);
	to[2] = 0;
	dist = VectorLength(to);
	want = vectoyaw(to) - pl->baseYaw;
	VectorSubtract(pl->home, ps->origin, to);
	to[2] = 0;
	if (VectorLength(to) > 160.0f) { // the arena is the clear ground around the placement: head back toward it
		want = vectoyaw(to) - pl->baseYaw;
		dist = 9999;
	}

	// the opponent's swing as perceived; the reaction comes a reaction time later
	if (!runner && sn->saberMove != LS_READY && pl->lastSeenMove == LS_READY && sn->saberMove >= LS_A_TL2BR && sn->saberMove <= LS_A_T2B)
		pl->reactAt = c + pl->reaction;
	pl->lastSeenMove = sn->saberMove;
	if (pl->reactAt && c >= pl->reactAt) {
		int r = TT_HsRange(pl, 0, 99);

		pl->reactKind = r < pl->blockPct ? 1 : r < pl->blockPct + pl->backPct ? 2 : 3;
		pl->reactUntil = c + TT_HsRange(pl, 250, 400);
		pl->reactAt = 0;
		if (pl->reactKind == 3)
			pl->nextAttack = c; // counter
	}
	if (pl->reactUntil > c && pl->reactKind != 3) {
		blocking = pl->reactKind == 1;
		fwd = pl->reactKind == 2 ? -127 : 0;
	}

	// aim: track the perceived opponent, drag through a swing, or spin
	turn = AngleSubtract(want, pl->yaw);
	if (pl->spinUntil > c)
		pl->yaw += TT_SPIN_RATE * dt / 1000.0f;
	else if (pl->dragUntil > c)
		pl->yaw += pl->dragRate * dt / 1000.0f;
	else {
		float step = ( blocking ? 2 : 1 ) * pl->turnRate * dt / 1000.0f;

		pl->yaw += turn > step ? step : turn < -step ? -step : turn;
	}
	pl->yaw = AngleNormalize360(pl->yaw);

	// footwork
	if (c >= pl->strafeUntil) {
		int r = TT_HsRange(pl, 0, 4);

		pl->strafeDir = r < 2 ? -1 : r < 4 ? 1 : 0;
		pl->strafeUntil = c + TT_HsRange(pl, 150, 700);
		if (TT_HsRange(pl, 0, 99) < 10)
			pl->jumpUntil = c + 80;
	}
	if (!blocking && pl->reactUntil <= c) {
		if (runner)
			fwd = dist < 160 ? -127 : 0;
		else if (dist > pl->range + 15)
			fwd = 127;
		else if (dist < pl->range - 15)
			fwd = -127;
	}
	if (!blocking && ( runner || dist <= pl->range + 40 )) // closing in is a straight walk; circling starts in range
		right = ( runner && !pl->strafeDir ? 1 : pl->strafeDir ) * 127;
	if (pl->jumpUntil > c)
		up = 127;

	// attacks
	if (ps->saberLockFrame) { // a lock is mashed
		buttons = ( c / 100 ) % 2 ? BUTTON_ATTACK : 0;
		fwd = right = up = 0;
	}
	else if (pl->spinUntil > c) {
		buttons = BUTTON_ATTACK;
		fwd = 0;
		right = 127;
	}
	else if (pl->pressUntil > c) {
		buttons = BUTTON_ATTACK;
		fwd = pl->pressFwd;
		right = pl->pressRight;
		up = pl->pressUp;
		ttSwing[idx].pressed = qtrue;
	}
	else if (!runner && !blocking && dist < pl->range + 40 && c >= pl->nextAttack && ps->saberMove == LS_READY) {
		int d = TT_HsRange(pl, 0, 7);

		if (idx == TT_BOT_A && ( arg & TT_HS_SPIN ) && TT_HsRange(pl, 0, 99) < 50) {
			pl->spinUntil = c + TT_HsRange(pl, 500, 1500);
			pl->nextAttack = pl->spinUntil + TT_HsRange(pl, pl->attackMin, pl->attackMax);
		}
		else {
			pl->pressUntil = c + TT_HsRange(pl, 40, 250);
			if (TT_HsRange(pl, 0, 99) < 25) // held on into a chain
				pl->pressUntil += TT_HsRange(pl, 400, 900);
			pl->pressFwd = dirs[d][0];
			pl->pressRight = dirs[d][1];
			pl->pressUp = 0;
			if (TT_HsRange(pl, 0, 99) < pl->specialPct) { // a lunge (crouched) or a jump attack, straight ahead
				pl->pressFwd = 127;
				pl->pressRight = 0;
				pl->pressUp = TT_HsRange(pl, 0, 1) ? 127 : -127;
			}
			if (TT_HsRange(pl, 0, 99) < ( idx == TT_BOT_A && ( arg & TT_HS_AGGRO ) ? 80 : 50 )) { // the mouse drags the swing across
				pl->dragUntil = c + TT_HsRange(pl, 100, 180);
				pl->dragRate = TT_HsRange(pl, 300, 800) * ( TT_HsRange(pl, 0, 1) ? 1.0f : -1.0f );
			}
			pl->nextAttack = pl->pressUntil + TT_HsRange(pl, pl->attackMin, pl->attackMax);
			buttons = BUTTON_ATTACK;
			fwd = pl->pressFwd;
			right = pl->pressRight;
			up = pl->pressUp;
			ttSwing[idx].pressed = qtrue;
		}
	}

	// never off a ledge: a walk needs floor 100 u ahead in its direction (otherwise it brakes), a jump 120 u
	if (( fwd || right ) && !TT_HsFloorAhead(me, pl->baseYaw + pl->yaw, fwd, right, 100)) {
		fwd = -fwd;
		right = -right;
	}
	if (up > 0 && !TT_HsFloorAhead(me, pl->baseYaw + pl->yaw, fwd, right, 120))
		up = 0;
	TT.botButtons[idx] = buttons;
	ttBotMove[idx][0] = fwd;
	ttBotMove[idx][1] = right;
	TT.botUpmove[idx] = up;
	ttBotYaw[idx] = pl->yaw;
}

static qboolean TTS_Hsim(void) {
	int arg = TT_ScenarioArg(), idx;

	if (!TT_SetupSaberPair(&ttSaberStances[1], 96, qtrue))
		return qfalse;
	memset(ttPilot, 0, sizeof(ttPilot));
	for (idx = 0; idx < 2; idx++) {
		ttClientFps[idx] = ttClientLatency[idx] = 0;
		ttBotLag[idx] = ( arg & TT_HS_LOSSY ) ? qtrue : qfalse;
	}
	if (arg & TT_HS_MIXED) {
		ttClientFps[TT_BOT_A] = 60;
		ttClientLatency[TT_BOT_A] = 80;
		ttClientFps[TT_BOT_B] = 333;
		ttClientLatency[TT_BOT_B] = 10;
	}
	return qtrue;
}

static void TTF_Hsim(void) {
	TT_MvUpkeep(TT_BOT_A);
	TT_MvUpkeep(TT_BOT_B);
}

static void TTT_Hsim(void) {
	ttClientFps[0] = ttClientFps[1] = ttClientLatency[0] = ttClientLatency[1] = 0;
	TTT_SaberCell();
}

#define TT_HSIM(name, arg) \
	{ "saberhsim_" name, TTS_Hsim, TTF_Hsim, TTT_Hsim, 2, (arg), TTC_Hsim }

#define TT_MOVE(name, arg) \
	{ "sabermove_" name, TTS_SaberMove, TTF_SaberMove, TTT_SaberCell, 2, (arg), TTC_SaberMove }

#define TT_CELL(s, si, m, mi, d, di) \
	{ "saber_" s "_" m "_" d, TTS_SaberCell, TTF_SaberCell, TTT_SaberCell, 2, (si) * 100 + (mi) * 10 + (di), TTC_SaberCell }
#define TT_CELL_D(s, si, m, mi)		TT_CELL(s, si, m, mi, "reach", 0), TT_CELL(s, si, m, mi, "tip", 1)
#define TT_CELL_M(s, si) \
	TT_CELL_D(s, si, "tl2br", 0), TT_CELL_D(s, si, "l2r", 1), TT_CELL_D(s, si, "t2b", 2), TT_CELL_D(s, si, "special", 3)

/*
============================================================================
siege
============================================================================
*/

static qboolean TT_RequireRound(void) {
	if (!gSiegeRoundBegun)
		return TT_Fail("siege round has not begun");
	return qtrue;
}

// TryHealingSomething only heals teammates: A briefly counts as B's team for the call
static qboolean TTS_PassiveHeal(void) {
	if (!TT_RequireRound())
		return qfalse;
	if (!TT_PlacePair(64, 128, 0))
		return qfalse;
	TT_A()->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_HEALTHDISP);
	return qtrue;
}

static void TTF_PassiveHeal(void) {
	gentity_t *a = TT_A(), *b = TT_B();

	TT_SetHealth(b, 1);
	int team = a->projectileTeam;

	a->projectileTeam = b->client->sess.sessionTeam;
	if (TT_UseReaches(a))
		TryHealingSomething(a, b, qtrue, qfalse);
	a->projectileTeam = team;
}

// the use_heal command path, sent every frame: B hurt and out of blaster ammo, A with both dispensers
static qboolean TTS_UseHealCmd(void) {
	if (!TT_RequireRound())
		return qfalse;
	if (!TT_PlacePair(64, 128, 0))
		return qfalse;
	TT_A()->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_HEALTHDISP) | (1 << HI_AMMODISP);
	TT_GiveWeapon(TT_B(), WP_BLASTER);
	return qtrue;
}

static void TTF_UseHealCmd(void) {
	gentity_t *a = TT_A(), *b = TT_B();

	TT_SetHealth(b, 1);
	b->client->ps.ammo[weaponData[WP_BLASTER].ammoIndex] = 0;
	int team = a->projectileTeam;

	a->projectileTeam = b->client->sess.sessionTeam;
	TryHealingSomething(a, b, qtrue, qtrue); // what Cmd_UseHealing_f calls on its traced target
	a->projectileTeam = team;
}

// use_heal_cmd with the command sent every 60 ms, as a bound key repeats, instead of every frame
static void TTF_UseHealCmd60(void) {
	TT_Every(0, 60, TTF_UseHealCmd);
}

// use_heal sent every frame while use is held: the command first, as the engine runs client commands before usercmds
static void TTF_UseHealBoth(void) {
	gentity_t *a = TT_A(), *b = TT_B();

	TT_SetHealth(b, 1);
	int team = a->projectileTeam;

	a->projectileTeam = b->client->sess.sessionTeam;
	TryHealingSomething(a, b, qtrue, qtrue);
	if (TT_UseReaches(a))
		TryHealingSomething(a, b, qtrue, qfalse);
	a->projectileTeam = team;
}

#define TT_WAVE_SECONDS		10

// the harness holds waves off; this one starts them at once and lasts long enough for two intervals
static qboolean TTS_Wave(void) {
	TT.savedCvar = trap_Cvar_VariableIntegerValue("g_siegeRespawn"); // before anything that can fail: teardown restores it
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;
	trap_Cvar_Set("g_siegeRespawn", va("%d", TT_WAVE_SECONDS));
	g_siegeRespawn.integer = TT_WAVE_SECONDS; // the first wave re-arms before the cvar table is refreshed
	level.siegeRespawnCheck = level.time;
	if (TT.endTime < TT.startTime + 2 * TT_WAVE_SECONDS * 1000 + 1000)
		TT.endTime = TT.startTime + 2 * TT_WAVE_SECONDS * 1000 + 1000;
	return qtrue;
}

static void TTT_Wave(void) {
	if (TT.savedCvar >= 0)
		trap_Cvar_Set("g_siegeRespawn", va("%d", TT.savedCvar));
	TT.savedCvar = -1;
}

static gentity_t *TT_SpawnSiegeItem(float dist, float height, const char **keys) {
	gentity_t *item;
	vec3_t p;

	TT_Along(dist, p);
	TT_DropToFloor(p);
	p[2] += height;
	item = TT_SpawnMapEntity("misc_siege_item", p, TT.yaw, keys);
	if (item)
		TT_SetEnt(0, item);
	return item;
}

// dropped from a height so SiegeItemThink runs the physics
static qboolean TTS_ExphysSiegeItem(void) {
	static const char *keys[] = { "model", "models/items/power_converter.md3", "canpickup", "0", "usephysics", "1", NULL };
	gentity_t *item;

	if (!TT_PlacePair(128, 320, 0))
		return qfalse;
	item = TT_SpawnSiegeItem(256, 64, keys);
	if (!item)
		return qfalse;
	if (!item->genericValue1 || item->think == NULL)
		return TT_Fail("siege item does not run physics");
	return qtrue;
}

static void TTF_SiegeItem(void) {
	if (!TT_GetEnt(0))
		TT_Fail("siege item is gone");
}

static qboolean TTS_SiegeItemRecharge(void) {
	static const char *keys[] = { "model", "models/items/power_converter.md3", "canpickup", "0", "usephysics", "0",
		"health", "500", "showhealth", "1", "health_chargeamt", "10", "health_chargerate", "100", NULL };
	gentity_t *item;

	if (!TT_PlacePair(128, 320, 0))
		return qfalse;
	item = TT_SpawnSiegeItem(256, 32, keys);
	if (!item)
		return qfalse;
	if (!item->genericValue12 || !item->maxHealth)
		return TT_Fail("siege item has no health recharge");
	item->health = 1;
	return qtrue;
}

static void TTF_SiegeItemRecharge(void) {
	gentity_t *item = TT_GetEnt(0);

	if (!item) {
		TT_Fail("siege item is gone");
		return;
	}
	if (item->health >= 400)
		item->health = 1;
}

// a brush trigger around B, so the point-in-bounds checks pass without a map entity
static gentity_t *TT_SpawnBoxTrigger(gentity_t *around, float size) {
	gentity_t *trigger = TT_SpawnSynthetic("trigger_multiple");
	vec3_t org;

	VectorCopy(around->client->ps.origin, org);
	G_SetOrigin(trigger, org);
	VectorSet(trigger->r.mins, -size, -size, -size);
	VectorSet(trigger->r.maxs, size, size, size);
	VectorAdd(org, trigger->r.mins, trigger->r.absmin);
	VectorAdd(org, trigger->r.maxs, trigger->r.absmax);
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->r.svFlags = SVF_NOCLIENT;
	return trigger;
}

#define TT_HACK_MS		2000

static qboolean TTS_Hack(void) {
	gentity_t *trigger;

	if (!TT_RequireRound())
		return qfalse;
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;

	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);
	trigger = TT_SpawnBoxTrigger(TT_B(), 48);
	trigger->spawnflags = 4; // USE_BUTTON
	trigger->genericValue7 = TT_HACK_MS;
	trigger->wait = 0;
	trigger->touch = Touch_Multi;
	TT.botButtons[TT_BOT_B] = BUTTON_USE;
	return qtrue;
}

// the engine calls touch every frame B stands in it
static void TTF_Hack(void) {
	gentity_t *trigger = TT_GetEnt(0);

	if (!trigger) {
		TT_Fail("hack trigger is gone");
		return;
	}
	Touch_Multi(trigger, TT_B(), NULL);
}

static void TTT_Hack(void) {
	if (TT_BotValid(TT_BOT_B)) {
		TT_B()->client->isHacking = 0;
		TT_B()->client->ps.hackingTime = 0;
	}
}

// a control point for B's team with B inside and A outside; progress never reaches the trigger time
static qboolean TTS_ControlPoint(void) {
	gentity_t *cp;

	if (!TT_RequireRound())
		return qfalse;
	if (!TT_PlacePair(128, 160, 0))
		return qfalse;

	TT_Pin(TT_BOT_B, TT_B()->client->ps.origin);
	cp = TT_SpawnBoxTrigger(TT_B(), 48);
	cp->genericValue1 = 1000000000;							// time
	cp->genericValue2 = TT_B()->client->sess.sessionTeam;	// team
	cp->genericValue5 = 1;									// maxplayers
	cp->genericValue8 = 1;									// reversible
	cp->genericValue16 = level.time;						// last think
	cp->genericValue17 = level.time;						// last progress
	cp->think = RunControlPoint;
	cp->nextthink = level.time + 50;
	return qtrue;
}

static void TTF_ControlPoint(void) {
	if (!TT_GetEnt(0))
		TT_Fail("control point is gone");
}

/*
============================================================================
scenario table
============================================================================
*/

static const tickTestScenario_t ttScenarios[] = {
	{ "station_health",			TTS_StationHealth,		TTF_StationHealth,		NULL,		2 },
	{ "station_shield",			TTS_StationShieldSiege,		TTF_StationShield,		NULL,		2 },
	{ "station_ammo",			TTS_StationAmmoSiege,	TTF_StationAmmo,		NULL,		2 },
	{ "station_ammo_generic",	TTS_StationAmmoGenericSiege,TTF_StationAmmoGeneric,	NULL,		2 },
	{ "recharge",				TTS_Recharge,			TTF_Recharge,			NULL,		2 },
	{ "recharge_shield",		TTS_RechargeShield,		TTF_Recharge,			NULL,		2 },
	{ "medic",					TTS_Medic,				TTF_Medic,				NULL,		2 },
	{ "techammo",				TTS_TechAmmo,			TTF_TechAmmo,			NULL,		2 },
	{ "repair",					TTS_Repair,				TTF_Repair,				NULL,		2 },
	{ "usegate",				TTS_UseGate,			TTF_UseGate,			NULL,		1 },
	{ "usegate_repress",		TTS_UseGate,			TTF_UseGateRepress,		NULL,		1 },
	{ "medic_tap",				TTS_Medic,				TTF_MedicTap,			NULL,		2 },
	{ "sentry",					TTS_Sentry,				TTF_Sentry,				NULL,		2 },
	{ "turretg2",				TTS_TurretG2,			TTF_TurretG2,			NULL,		2 },
	{ "turret",					TTS_Turret,				TTF_Turret,				NULL,		2 },
	{ "lightning",				TTS_Lightning,			TTF_Lightning,			NULL,		2 },
	{ "lightning_protect",		TTS_LightningProtect,	TTF_LightningProtect,	NULL,		2 },
	{ "lightning_absorb",		TTS_LightningAbsorb,	TTF_LightningAbsorb,	NULL,		2 },
	{ "drain",					TTS_Drain,				TTF_Drain,				NULL,		2 },
	{ "grip",					TTS_Grip,				TTF_Grip,				NULL,		2 },
	{ "rage",					TTS_Rage,				TTF_Rage,				NULL,		1 },
	{ "protect",				TTS_Protect,			TTF_Protect,			NULL,		1 },
	{ "regen",					TTS_Regen,				TTF_Regen,				NULL,		1 },
	{ "jetpack",				TTS_Jetpack,			TTF_Nothing,			NULL,		1 },
	{ "cloak",					TTS_Cloak,				TTF_Nothing,			NULL,		1 },
	{ "rocket",					TTS_Rocket,				TTF_Rocket,				NULL,		2 },
	{ "tripwire",				TTS_Tripwire,			TTF_Tripwire,			NULL,		2 },
	{ "prox",					TTS_Prox,				TTF_Prox,				NULL,		2 },
	{ "thermal",				TTS_Thermal,			TTF_Thermal,			NULL,		1 },
	{ "demp2",					TTS_Demp2,				TTF_Demp2,				NULL,		2 },
	{ "bounce",					TTS_Bounce,				TTF_Bounce,				NULL,		2 },
	{ "crusher",				TTS_Crusher,			TTF_Crusher,			NULL,		2 },
	{ "hurt",					TTS_Hurt,				TTF_Hurt,				NULL,		2 },
	{ "laser",					TTS_Laser,				TTF_Nothing,			NULL,		2 },
	{ "exphys_eweb",			TTS_EWeb,				TTF_EWeb,				NULL,		1 },
	{ "exphys_item",			TTS_ExphysItem,			TTF_ExphysItem,			NULL,		1 },
	{ "shield",					TTS_Shield,				TTF_Shield,				NULL,		1 },
	{ "seeker",					TTS_Seeker,				TTF_Seeker,				NULL,		2 },
	{ "lava",					TTS_Lava,				TTF_Lava,				NULL,		2 },
	{ "lava_paused",			TTS_Lava,				TTF_LavaPaused,			TTT_LavaPaused,	2 },
	{ "respawn",				TTS_Respawn,			TTF_Respawn,			TTT_Respawn,	2 },
	{ "powerup",				TTS_Powerup,			TTF_Powerup,			NULL,		1 },
	{ "trigger",				TTS_Trigger,			TTF_Trigger,			NULL,		2 },
	{ "trigger_wait",			TTS_TriggerWait,		TTF_Trigger,			NULL,		2 },
	{ "mover",					TTS_Mover,				TTF_Nothing,			NULL,		1 },
	{ "mover_reverse",			TTS_MoverReverse,		TTF_MoverReverse,		NULL,		1 },
	{ "npc",					TTS_Npc,				TTF_Npc,				TTT_Npc,	2 },
	{ "saber",					TTS_Saber,				TTF_Nothing,			NULL,		2,	0,	TTC_Saber },
	{ "saber_clash",			TTS_SaberClash,			TTF_Nothing,			NULL,		2,	0,	TTC_SaberClash },
	{ "saberthrow",				TTS_SaberThrow,			TTF_SaberThrow,			TTT_SaberThrow,	2 },
	{ "saberthrow_hit",			TTS_SaberThrowHit,		TTF_SaberThrow,			TTT_SaberThrow,	2 },
	TT_CELL_M("fast", 0),
	TT_CELL_M("medium", 1),
	TT_CELL_M("strong", 2),
	TT_CELL_M("dual", 3),
	TT_CELL_M("staff", 4),
	{ "saberduel_block",		TTS_SaberDuelBlock,		TTF_SaberDuelBlock,		TTT_SaberCell,	2,	0,	TTC_SaberDuelBlock },
	{ "saberduel_swing",		TTS_SaberDuelSwing,		TTF_SaberDuelSwing,		TTT_SaberCell,	2,	0,	TTC_SaberDuelSwing },
	{ "saberduel_l2r",		TTS_SaberDuelL2R,		TTF_SaberDuelSwing,		TTT_SaberCell,	2,	-50, TTC_SaberDuelSwing },
	{ "passive_heal",			TTS_PassiveHeal,		TTF_PassiveHeal,		NULL,		2 },
	{ "use_heal_cmd",			TTS_UseHealCmd,			TTF_UseHealCmd,			NULL,		2 },
	{ "use_heal_cmd60",			TTS_UseHealCmd,			TTF_UseHealCmd60,		NULL,		2 },
	{ "use_heal_both",			TTS_PassiveHeal,		TTF_UseHealBoth,		NULL,		2 },
	{ "wave",					TTS_Wave,				TTF_Nothing,			TTT_Wave,	1 },
	{ "exphys_siegeitem",		TTS_ExphysSiegeItem,	TTF_SiegeItem,			NULL,		1 },
	{ "siegeitem_recharge",		TTS_SiegeItemRecharge,	TTF_SiegeItemRecharge,	NULL,		1 },
	{ "hack",					TTS_Hack,				TTF_Hack,				TTT_Hack,	2 },
	{ "controlpoint",			TTS_ControlPoint,		TTF_ControlPoint,		NULL,		2 },
	{ "jetpack_short",			TTS_JetpackShort,		TTF_Nothing,			TTT_JetpackShort,	1 },
	TT_MOVE("spin",			TT_MV_SPIN_A),
	TT_MOVE("spin_lag",		TT_MV_SPIN_A | TT_MV_LAG_A),
	TT_MOVE("turn",			TT_MV_TURN_A),
	TT_MOVE("turn_lag",		TT_MV_TURN_A | TT_MV_LAG_A),
	TT_MOVE("lag",			TT_MV_LAG_A),
	TT_MOVE("strafe",		TT_MV_STRAFE_A),
	TT_MOVE("target_strafe",	TT_MV_STRAFE_B),
	TT_MOVE("target_spin",	TT_MV_SPIN_B),
	TT_MOVE("duel_turn",		TT_MV_DUEL | TT_MV_TURN_A),
	TT_MOVE("duel_strafe",	TT_MV_DUEL | TT_MV_STRAFE_A),
	TT_HSIM("duel",			0),
	TT_HSIM("aggro",		TT_HS_AGGRO),
	TT_HSIM("spinner",		TT_HS_SPIN),
	TT_HSIM("chase",		TT_HS_CHASE),
	TT_HSIM("specials",		TT_HS_SPECIALS),
	TT_HSIM("mixed",		TT_HS_MIXED),
	TT_HSIM("lossy",		TT_HS_LOSSY),
};

static const int ttNumScenarios = sizeof(ttScenarios) / sizeof(ttScenarios[0]);

static int TT_ScenarioArg(void) {
	return ttScenarios[TT.scenario].arg;
}

/*
============================================================================
bots
============================================================================
*/

static int TT_FindBotByName(const char *name) {
	int i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *e = &g_entities[i];
		char clean[MAX_NETNAME];

		if (!e->inuse || !e->client || !(e->r.svFlags & SVF_BOT) || e->client->pers.connected == CON_DISCONNECTED)
			continue;

		Q_strncpyz(clean, e->client->pers.netname, sizeof(clean));
		Q_CleanStr(clean);
		if (!Q_stricmp(clean, name))
			return i;
	}

	return -1;
}

qboolean TickTest_IsBot(int clientNum) {
	int idx;

	for (idx = 0; idx < 2; idx++) {
		if (TT.haveBot[idx] && TT.bot[idx] == clientNum)
			return qtrue;
	}
	return qfalse;
}

int TickTest_BotIndex(int clientNum) {
	int idx;

	for (idx = 0; idx < 2; idx++) {
		if (TT.haveBot[idx] && TT.bot[idx] == clientNum)
			return idx;
	}
	return -1;
}

// the command a test bot is about to think with: the scenario's per-command hook decides for it first
void TickTest_BotThink(int clientNum, usercmd_t *cmd) {
	const tickTestScenario_t *sc = &ttScenarios[TT.scenario];
	int idx = TickTest_BotIndex(clientNum);

	if (idx >= 0 && idx < sc->bots && TT.phase == TICKTEST_RUNNING && sc->cmd)
		sc->cmd(idx, cmd->serverTime);
	if (idx >= 0 && idx < 2)
		ttPrevCmd[idx] = cmd->serverTime;
	TickTest_BotCmd(clientNum, cmd);
}

// replaces a test bot's command with the scenario's: scripted buttons and movement, view held by delta_angles
qboolean TickTest_BotCmd(int clientNum, usercmd_t *cmd) {
	int idx = TickTest_BotIndex(clientNum), serverTime;
	gclient_t *cl;

	if (idx < 0 || !g_entities[clientNum].client)
		return qfalse;

	cl = g_entities[clientNum].client;
	serverTime = cmd->serverTime;
	memset(cmd, 0, sizeof(*cmd));
	cmd->serverTime = serverTime;
	if (TT.phase == TICKTEST_RUNNING) { // a settling saber scenario's bots stand still: the scenario's inputs start with its clock
		cmd->buttons = TT.botButtons[idx];
		cmd->upmove = (signed char)TT.botUpmove[idx];
		cmd->forwardmove = (signed char)ttBotMove[idx][0];
		cmd->rightmove = (signed char)ttBotMove[idx][1];
		cmd->angles[YAW] = ANGLE2SHORT(ttBotYaw[idx]);
	}
	cmd->weapon = (byte)cl->ps.weapon;
	cmd->forcesel = (byte)cl->ps.fd.forcePowerSelected;
	return qtrue;
}

// waits for the bots this scenario needs to be connected on their teams, asking for them once
static qboolean TT_BotsReady(int needed) {
	qboolean ready = qtrue;
	int idx;

	for (idx = 0; idx < needed; idx++) {
		int num = TT_FindBotByName(ttBotNames[idx]);
		gentity_t *e;

		if (num < 0) {
			TT.haveBot[idx] = qfalse;
			if (!TT.botRequested[idx]) {
				trap_SendConsoleCommand(EXEC_APPEND, va("addbot Desann 1 %s 0 %s\n", ttBotTeams[idx], ttBotNames[idx]));
				TT.botRequested[idx] = qtrue;
			}
			ready = qfalse;
			continue;
		}

		e = &g_entities[num];
		if (e->client->pers.connected != CON_CONNECTED) {
			ready = qfalse;
			continue;
		}
		if (e->client->sess.sessionTeam != ttBotTeamNums[idx]) {
			if (level.time >= TT.botTeamRetry[idx]) {
				SetTeam(e, (char *)ttBotTeams[idx], qtrue);
				TT.botTeamRetry[idx] = level.time + 1000;
			}
			ready = qfalse;
			continue;
		}
		if (e->client->siegeClass < 0) { // assigned once the bot has its team
			ready = qfalse;
			continue;
		}

		if (!TT.haveBot[idx] || TT.bot[idx] != num) {
			TT.bot[idx] = num;
			TT.haveBot[idx] = qtrue;
			TT.botClass[idx] = e->client->siegeClass;
			TT.botKnown[idx] = e->client->ps.fd.forcePowersKnown;
			memcpy(TT.botLevels[idx], e->client->ps.fd.forcePowerLevel, sizeof(TT.botLevels[idx]));
		}
	}

	return ready;
}

static qboolean TT_BotValid(int idx) {
	gentity_t *e;

	if (!TT.haveBot[idx])
		return qfalse;
	e = TT_Bot(idx);
	return e->inuse && e->client && e->client->pers.connected == CON_CONNECTED &&
		e->client->sess.sessionTeam == ttBotTeamNums[idx];
}

// a thrown or knocked-away saber goes back in the owner's hand as a catch would (ClientSpawn leaves it alone)
static void TT_RecallSaber(gentity_t *owner) {
	int i;

	for (i = MAX_CLIENTS; i < level.num_entities; i++) {
		gentity_t *e = &g_entities[i];

		if (!e->inuse || e->r.ownerNum != owner->s.number)
			continue;
		if (e->think != saberFirstThrown && e->think != saberBackToOwner && e->think != DownedSaberThink)
			continue;

		if (e->think == DownedSaberThink)
			saberReactivate(e, owner); // off the ground first, as a pull-back does
		owner->client->ps.saberInFlight = qfalse;
		memset(owner->client->saberThrowDamageTime, 0, sizeof(owner->client->saberThrowDamageTime));
		owner->client->ps.saberEntityState = 0;
		owner->client->ps.saberEntityNum = e->s.number;
		e->touch = SaberGotHit;
		e->think = SaberUpdateSelf;
		e->genericValue5 = 0;
		e->nextthink = level.time;
		e->r.svFlags |= SVF_NOCLIENT;
		e->s.loopSound = 0;
		e->s.loopIsSoundset = qfalse;
		WP_SaberRemoveG2Model(e);
	}
}

// back to a fresh spawn with the force setup the bot joined with
static void TT_ResetBot(int idx) {
	gentity_t *ent;
	gclient_t *cl;
	int i;

	if (!TT_BotValid(idx))
		return;

	ent = TT_Bot(idx);
	cl = ent->client;

	// ClientSpawn keeps the force data, so stop everything first
	for (i = 0; i < NUM_FORCE_POWERS; i++) {
		if (cl->ps.fd.forcePowersActive & (1 << i))
			WP_ForcePowerStop(ent, (forcePowers_t)i);
		cl->ps.fd.forcePowerDuration[i] = 0;
		cl->ps.fd.forcePowerDebounce[i] = 0;
	}
	cl->ps.fd.forcePowersActive = 0;
	cl->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;
	cl->ps.fd.forceGripBeingGripped = 0;
	cl->ps.fd.sentryDeployed = qfalse;
	TT_RecallSaber(ent);
	cl->siegeClass = TT.botClass[idx]; // a scenario may have lent it another class
	cl->isHacking = 0;
	cl->ps.hackingTime = 0;

	ClientSpawn(ent, qfalse);

	ent->flags |= FL_GODMODE;
	ent->flags &= ~FL_NOTARGET;
	ent->enemy = NULL; // survives a respawn, and NPCs hold fire at someone whose enemy is a saberist
	cl->ps.eFlags &= ~EF_INVULNERABLE;
	cl->invulnerableTimer = 0;
	cl->ps.fd.forcePowersKnown = TT.botKnown[idx];
	memcpy(cl->ps.fd.forcePowerLevel, TT.botLevels[idx], sizeof(cl->ps.fd.forcePowerLevel));
	cl->ps.fd.forcePower = cl->ps.fd.forcePowerMax;
	cl->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;
	cl->ps.forceGripChangeMovetype = PM_NORMAL;

	TT.botButtons[idx] = 0;
	TT.botUpmove[idx] = 0;
	ttBotMove[idx][0] = ttBotMove[idx][1] = 0;
	ttBotYaw[idx] = 0;
	ttBotLag[idx] = qfalse;
	TT.botPinned[idx] = qfalse;
}

/*
============================================================================
scenario flow
============================================================================
*/

static void TT_Teardown(void) {
	const tickTestScenario_t *sc = &ttScenarios[TT.scenario];
	int idx, respawn;

	bg_timesyncBase = 0;
	ttPhase = 0;
	ttHuman = 0;

	if (sc->teardown)
		sc->teardown();

	// waves were held off for the scenario: back to the normal next-wave deadline
	respawn = trap_Cvar_VariableIntegerValue("g_siegeRespawn");
	respawn = respawn > 1 ? respawn * 1000 : 0;
	if (level.siegeRespawnCheck > level.time + respawn)
		level.siegeRespawnCheck = level.time + respawn;

	if (TT_BotValid(TT_BOT_A) && TT_A()->client->ewebIndex) {
		gentity_t *eweb = &g_entities[TT_A()->client->ewebIndex];
		EWebDisattach(TT_A(), eweb);
		G_FreeEntity(eweb);
	}

	TT_FreeSpawned();
	for (idx = 0; idx < 2; idx++)
		TT_ResetBot(idx);

	memset(TT.ent, -1, sizeof(TT.ent));
	TT.phase = TICKTEST_IDLE;
}

static void TT_Error(const char *text) {
	G_LogPrintf("[tick] scenario %s error %s\n", TT.name, text);
	G_Printf("ticktest: %s: %s\n", TT.name, text);
	TT_Teardown();
}

static void TT_Begin(void) {
	const tickTestScenario_t *sc = &ttScenarios[TT.scenario];
	int idx;

	memset(TT.ent, -1, sizeof(TT.ent));
	memset(TT.timer, 0, sizeof(TT.timer));
	TT.numSpawned = 0;
	ttFailText[0] = '\0';

	for (idx = 0; idx < 2; idx++) {
		TT_ResetBot(idx);
		if (TT_BotValid(idx))
			trap_UnlinkEntity(TT_Bot(idx)); // out of the way of the line search; placement links them again
	}

	// fixed random sequence, so repeated runs at one sv_fps match; tt_seed picks another one for spread measurements
	{
		int seed = trap_Cvar_VariableIntegerValue("tt_seed");

		if (!seed)
			seed = TT_SEED;
		srand(seed);
		Rand_Init(seed);
	}

	TT.phase = TICKTEST_RUNNING;
	TT.startTime = level.time;
	TT.endTime = level.time + TT.duration;
	level.siegeRespawnCheck = TT.endTime + 1000; // no respawn wave inside a scenario unless it asks for one
	// commands sit on absolute multiples of cmdMsec, so the 50 ms grid times are command times at any sv_fps (level.time
	// is one at 20 Hz, where this is level.time + cmdMsec); this frame's command has been thought
	for (idx = 0; idx < 2; idx++)
		TT.botNextCmd[idx] = TT.cmdMsec ? (level.time / TT.cmdMsec + 1) * TT.cmdMsec : level.time;

	if (!sc->setup()) {
		TT_Error(ttFailText[0] ? ttFailText : "setup failed");
		return;
	}

	// a saber scenario's bots were just placed: the first photos after a placement interpolate across it and the blends,
	// boxes and angles need a few steps, all of it rate-dependent, and a duel inherits its first steps. so the clock
	// starts once that has settled (TT_Start); every other scenario starts now, as before
	if (!Q_stricmpn(sc->name, "saber", 5)) {
		TT.phase = TICKTEST_SETTLE;
		TT.settleEnd = level.time + TT_SETTLE_MSEC;
		return;
	}

	G_LogPrintf("[tick] scenario %s start t=%d fps=%d frame=%d\n", TT.name, level.time, g_svfps.integer, level.time - level.previousTime);
}

// the settled saber scenario's clock starts: the random sequence, the command schedule and the start log as TT_Begin's
static void TT_Start(void) {
	int idx;

	{
		int seed = trap_Cvar_VariableIntegerValue("tt_seed");

		if (!seed)
			seed = TT_SEED;
		srand(seed);
		Rand_Init(seed);
	}

	TT.phase = TICKTEST_RUNNING;
	TT.startTime = level.time;
	TT.endTime = level.time + TT.duration;
	level.siegeRespawnCheck = TT.endTime + 1000;
	ttPhase = TT.cmdMsec ? Com_Clampi(0, TICK_BASE_MSEC - 1, trap_Cvar_VariableIntegerValue("tt_phase")) : 0;
	for (idx = 0; idx < 2; idx++)
		TT.botNextCmd[idx] = TT.cmdMsec ? (level.time / TT.cmdMsec + 1) * TT.cmdMsec + ttPhase % TT.cmdMsec : level.time;
	// pmove's saber chain rolls are seeded by the command time: counted from the start they repeat on every run and rate
	bg_timesyncBase = trap_Cvar_VariableIntegerValue("tt_timesync") ? TT.startTime + ttPhase : 0;
	ttHuman = TT.cmdArrival && TT.cmdMsec ? trap_Cvar_VariableIntegerValue("tt_hseed") : 0;
	if (ttHuman) {
		for (idx = 0; idx < 2; idx++)
			TT_ClientInit(idx, ttBotLag[idx]);
	}

	G_LogPrintf("[tick] scenario %s start t=%d fps=%d frame=%d\n", TT.name, level.time, g_svfps.integer, level.time - level.previousTime);
}

// end of every server frame while the harness owns the bots
void TickTest_RunFrame(void) {
	const tickTestScenario_t *sc;
	int idx;

	if (!TT.active || TT.phase == TICKTEST_IDLE)
		return;

	sc = &ttScenarios[TT.scenario];

	if (TT.phase == TICKTEST_WAIT_BOTS) {
		if (TT_BotsReady(2)) { // a siege round only begins with both teams manned
			// start on a frame at a 50 ms grid time, as every 20 Hz frame is one, so the run's phase against the grid
			// does not depend on when the command arrived (at 30 Hz such a frame comes every 1650 ms)
			if (gSiegeRoundBegun && level.time - level.siegeRoundStartTime >= TT_ROUND_SETTLE_MS
				&& (level.time - level.previousTime == TICK_BASE_MSEC || !(level.time % TICK_BASE_MSEC)))
				TT_Begin();
			else if (level.time - TT.waitStart > TT_BOT_WAIT_MS + TT_ROUND_WAIT_MS)
				TT_Error(va("siege round did not begin within %d s", (TT_BOT_WAIT_MS + TT_ROUND_WAIT_MS) / 1000));
		}
		else if (level.time - TT.waitStart > TT_BOT_WAIT_MS) {
			for (idx = 0; idx < 2; idx++) {
				if (!TT.haveBot[idx])
					TT.botRequested[idx] = qfalse; // the next ticktest asks again
			}
			TT_Error(va("test bots did not join within %d s", TT_BOT_WAIT_MS / 1000));
		}
		return;
	}

	for (idx = 0; idx < sc->bots; idx++) {
		if (!TT_BotValid(idx)) {
			TT.botRequested[idx] = qfalse; // ask for it again next time
			TT_Error(va("test bot %s left the game", ttBotNames[idx]));
			return;
		}
	}

	if (!gSiegeRoundBegun || level.siegeRoundComplete || level.intermissiontime) {
		TT_Error("siege round ended");
		return;
	}

	if (TT.phase == TICKTEST_SETTLE) {
		TT_ApplyPins();
		// the clock starts on a frame at a 50 ms grid time, as TT_Begin did
		if (level.time >= TT.settleEnd && (level.time - level.previousTime == TICK_BASE_MSEC || !(level.time % TICK_BASE_MSEC))) {
			TT_Start();
			if (TT.cmdArrival && TT.cmdMsec)
				TT_ArrivalThinks(sc->bots);
		}
		return;
	}

	if (level.time >= TT.endTime) {
		G_LogPrintf("[tick] scenario %s end t=%d\n", TT.name, level.time);
		G_Printf("ticktest: %s done\n", TT.name);
		TT_Teardown();
		return;
	}

	sc->frame();
	if (ttFailText[0]) {
		TT_Error(ttFailText);
		return;
	}

	TT_ApplyPins();
	if (TT.cmdArrival && TT.cmdMsec)
		TT_ArrivalThinks(sc->bots);
}


/*
the map's own physics siege items and npcs think every frame and fire the same probes the scenarios
measure, so their thinks are suspended while the harness runs
*/
static void TT_QuietMap(void) {
	int i;

	TT.numQuiet = 0;
	for (i = MAX_CLIENTS; i < level.num_entities && TT.numQuiet < TICKTEST_MAX_SPAWNED; i++) {
		gentity_t *e = &g_entities[i];

		if (!e->inuse)
			continue;
		if (!(e->think == SiegeItemThink && e->genericValue1) && !(e->think == NPC_Think && e->NPC))
			continue;

		TT.quiet[TT.numQuiet] = i;
		TT.quietThink[TT.numQuiet] = e->think;
		TT.quietClass[TT.numQuiet] = e->classname;
		TT.quietNPC[TT.numQuiet] = e->NPC;
		TT.quietFreetime[TT.numQuiet] = e->freetime;
		TT.numQuiet++;
		e->think = NULL;
		e->nextthink = 0;
	}
}

static void TT_WakeMap(void) {
	int i;

	for (i = 0; i < TT.numQuiet; i++) {
		gentity_t *e = &g_entities[TT.quiet[i]];

		// only the same entity: a slot freed and reused since (freetime moves on) is left alone
		if (e->inuse && !e->think && e->classname == TT.quietClass[i] && e->NPC == TT.quietNPC[i] &&
			e->freetime == TT.quietFreetime[i]) {
			e->think = TT.quietThink[i];
			e->nextthink = level.time;
		}
	}
	TT.numQuiet = 0;
}

static void TT_Stop(void) {
	int idx;

	if (TT.active && TT.phase != TICKTEST_IDLE) {
		G_LogPrintf("[tick] scenario %s error stopped\n", TT.name);
		if (TT.phase == TICKTEST_RUNNING || TT.phase == TICKTEST_SETTLE)
			TT_Teardown();
	}

	// by name, so a bot that joined but was never taken over goes too
	for (idx = 0; idx < 2; idx++) {
		int num = TT_FindBotByName(ttBotNames[idx]);

		if (num >= 0)
			trap_DropClient(num, "was removed by ticktest stop");
		else if (TT.botRequested[idx]) // an addbot may still be queued: kick after it runs
			trap_SendConsoleCommand(EXEC_APPEND, va("kick %s\n", ttBotNames[idx]));
	}

	if (TT.active)
		TT_WakeMap();
	memset(&TT, 0, sizeof(TT));
	G_Printf("ticktest: stopped\n");
}

// ticktest <scenario> [seconds=20] [cmdmsec=0] | stop | list
void Svcmd_TickTest_f(void) {
	char arg[MAX_TOKEN_CHARS];
	int i, seconds = 20;

	trap_Argv(1, arg, sizeof(arg));

	if (!Q_stricmp(arg, "list")) {
		for (i = 0; i < ttNumScenarios; i++)
			G_Printf("%s\n", ttScenarios[i].name);
		return;
	}

	if (!Q_stricmp(arg, "stop")) {
		TT_Stop();
		return;
	}

	if (!arg[0]) {
		G_Printf("usage: ticktest <scenario> [seconds=20] [cmdmsec=0] [arrival=0] | stop | list\n");
		return;
	}

	if (!trap_Cvar_VariableIntegerValue("sv_cheats")) {
		G_Printf("ticktest requires sv_cheats 1\n");
		return;
	}

	if (TT.active && TT.phase != TICKTEST_IDLE) {
		G_Printf("ticktest: scenario %s is still running\n", TT.name);
		return;
	}

	for (i = 0; i < ttNumScenarios; i++) {
		if (!Q_stricmp(arg, ttScenarios[i].name))
			break;
	}
	if (i == ttNumScenarios) {
		G_Printf("ticktest: unknown scenario %s (ticktest list)\n", arg);
		return;
	}

	if (trap_Argc() > 2) {
		trap_Argv(2, arg, sizeof(arg));
		seconds = Com_Clampi(1, 3600, atoi(arg));
	}
	// sub-frame bot commands: one per cmdmsec of level time instead of one per frame (0: per frame)
	TT.cmdMsec = 0;
	if (trap_Argc() > 3) {
		trap_Argv(3, arg, sizeof(arg));
		TT.cmdMsec = Com_Clampi(0, TICK_BASE_MSEC, atoi(arg));
	}
	TT.cmdArrival = qfalse;
	if (trap_Argc() > 4) {
		trap_Argv(4, arg, sizeof(arg));
		TT.cmdArrival = atoi(arg) ? qtrue : qfalse;
	}
	TT.cmdStamp = 0;

	if (!TT.active) {
		memset(TT.ent, -1, sizeof(TT.ent));
		TT.savedClass = -1;
		TT.savedCvar = -1;
		TT_QuietMap();
	}
	TT.active = qtrue;
	TT.scenario = i;
	TT.name = ttScenarios[i].name;
	TT.duration = seconds * 1000;
	TT.phase = TICKTEST_WAIT_BOTS;
	TT.waitStart = level.time;
}
