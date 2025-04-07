// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "bg_saga.h"

//==========================================================

/*QUAKED target_give (1 0 0) (-8 -8 -8) (8 8 8)
Gives the activator all the items pointed to.
*/
void Use_Target_Give( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	gentity_t	*t;
	trace_t		trace;

	if ( !activator->client ) {
		return;
	}

	if ( !ent->target ) {
		return;
	}

	memset( &trace, 0, sizeof( trace ) );
	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), ent->target)) != NULL ) {
		if ( !t->item ) {
			continue;
		}
		Touch_Item( t, activator, &trace );

		// make sure it isn't going to respawn or show any events
		t->nextthink = 0;
		trap_UnlinkEntity( t );
	}
}

void SP_target_give( gentity_t *ent ) {
	ent->use = Use_Target_Give;
}


//==========================================================

/*QUAKED target_remove_powerups (1 0 0) (-8 -8 -8) (8 8 8)
takes away all the activators powerups.
Used to drop flight powerups into death puts.
*/
void Use_target_remove_powerups( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if( !activator->client ) {
		return;
	}

	if( activator->client->ps.powerups[PW_REDFLAG] ) {
		Team_ReturnFlag( TEAM_RED );
	} else if( activator->client->ps.powerups[PW_BLUEFLAG] ) {
		Team_ReturnFlag( TEAM_BLUE );
	} else if( activator->client->ps.powerups[PW_NEUTRALFLAG] ) {
		Team_ReturnFlag( TEAM_FREE );
	}

	memset( activator->client->ps.powerups, 0, sizeof( activator->client->ps.powerups ) );
}

void SP_target_remove_powerups( gentity_t *ent ) {
	ent->use = Use_target_remove_powerups;
}

extern stringID_table_t WPTable[];
extern stringID_table_t HoldableTable[];
extern stringID_table_t PowerupTable[];
extern stringID_table_t AmmoTable[];
extern stringID_table_t FPTable[];
extern int BG_SiegeTranslateGenericTable(char *buf, stringID_table_t *table, qboolean bitflag);
typedef struct powersList_s {
	byte	levels[NUM_FORCE_POWERS];
} powersList_t;
static powersList_t powersLists[MAX_GENTITIES] = { 0 };
static void TranslateForcePowers(char *buf, powersList_t *powers) { // copy of base jka function without siegeclass_t
	char checkPower[1024];
	char checkLevel[256];
	int l = 0;
	int k = 0;
	int j = 0;
	int i = 0;
	int parsedLevel = 0;
	qboolean allPowers = qfalse;
	qboolean noPowers = qfalse;

	if (!Q_stricmp(buf, "FP_ALL"))
	{ //this is a special case, just give us all the powers on level 3
		allPowers = qtrue;
	}

	if (buf[0] == '0' && !buf[1])
	{ //no powers then
		noPowers = qtrue;
	}

	//First clear out the powers, or in the allPowers case, give us all level 3.
	while (i < NUM_FORCE_POWERS)
	{
		if (allPowers)
		{
			powers->levels[i] = FORCE_LEVEL_3;
		}
		else
		{
			powers->levels[i] = 0;
		}
		i++;
	}

	if (allPowers || noPowers)
	{ //we're done now then.
		return;
	}

	i = 0;
	while (buf[i])
	{ //parse through the list which is seperated by |, and add all the weapons into a bitflag
		if (buf[i] != ' ' && buf[i] != '|')
		{
			j = 0;

			while (buf[i] && buf[i] != ' ' && buf[i] != '|' && buf[i] != ',')
			{
				checkPower[j] = buf[i];
				j++;
				i++;
			}
			checkPower[j] = 0;

			if (buf[i] == ',')
			{ //parse the power level
				i++;
				l = 0;
				while (buf[i] && buf[i] != ' ' && buf[i] != '|')
				{
					checkLevel[l] = buf[i];
					l++;
					i++;
				}
				checkLevel[l] = 0;
				parsedLevel = atoi(checkLevel);

				//keep sane limits on the powers
				if (parsedLevel < 0)
				{
					parsedLevel = 0;
				}
				if (parsedLevel > FORCE_LEVEL_5)
				{
					parsedLevel = FORCE_LEVEL_5;
				}
			}
			else
			{ //if it's not there, assume level 3 I guess.
				parsedLevel = 3;
			}

			if (checkPower[0])
			{ //Got the name, compare it against the weapon table strings.
				k = 0;

				if (!Q_stricmp(checkPower, "FP_JUMP"))
				{ //haqery
					strcpy(checkPower, "FP_LEVITATION");
				}

				while (FPTable[k].id != -1 && FPTable[k].name[0])
				{
					if (!Q_stricmp(checkPower, FPTable[k].name))
					{ //found it, add the weapon into the weapons value
						powers->levels[k] = parsedLevel;
						break;
					}
					k++;
				}
			}
		}

		if (!buf[i])
		{
			break;
		}
		i++;
	}
}

typedef enum gearChange_s {
	GEARCHANGE_ADD = 0,
	GEARCHANGE_REMOVE,
	GEARCHANGE_SET
} gearChange_t;

void Use_target_gear(gentity_t *ent, gentity_t *other, gentity_t *activator) {
	if (!activator ||
		!activator->client ||
		activator->health <= 0 ||
		activator->client->sess.sessionTeam == TEAM_SPECTATOR ||
		activator->client->ps.pm_type == PM_INTERMISSION ||
		activator->client->ps.pm_type == PM_DEAD) {
		return;
	}

	gearChange_t gearChange = ent->genericValue1;
	qboolean doWeapons = ent->genericValue2;
	int weapons = ent->genericValue3;
	qboolean doItems = ent->genericValue4;
	int items = ent->genericValue5;
	qboolean doPowerups = ent->genericValue6;
	int powerups = ent->genericValue7;
	qboolean doAmmo = ent->genericValue8;
	int ammoTypes = ent->genericValue9;
	int ammoAmount = ent->genericValue10;
	qboolean doHealth = ent->genericValue11;
	int health = ent->genericValue12;
	qboolean doArmor = ent->genericValue13;
	int armor = ent->genericValue14;
	qboolean doForce = ent->genericValue15;

	if (doWeapons) {
		if (gearChange == GEARCHANGE_ADD)
			activator->client->ps.stats[STAT_WEAPONS] |= weapons;
		else if (gearChange == GEARCHANGE_REMOVE)
			activator->client->ps.stats[STAT_WEAPONS] &= ~weapons;
		else if (gearChange == GEARCHANGE_SET)
			activator->client->ps.stats[STAT_WEAPONS] = weapons;
		if (!activator->client->ps.stats[STAT_WEAPONS])
			activator->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
	}

	if (doItems) {
		if (gearChange == GEARCHANGE_ADD)
			activator->client->ps.stats[STAT_HOLDABLE_ITEMS] |= items;
		else if (gearChange == GEARCHANGE_REMOVE)
			activator->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~items;
		else if (gearChange == GEARCHANGE_SET)
			activator->client->ps.stats[STAT_HOLDABLE_ITEMS] = items;
	}

	if (doPowerups) {
		for (powerup_t pw = PW_NONE; pw < PW_NUM_POWERUPS; pw++) {
			if (powerups & (1 << pw)) {
				if (gearChange == GEARCHANGE_ADD && activator->client->ps.powerups[pw] > level.time)
					activator->client->ps.powerups[pw] += 25000;
				else if (gearChange != GEARCHANGE_REMOVE)
					activator->client->ps.powerups[pw] = level.time + 25000;
				else
					activator->client->ps.powerups[pw] = 0;

				if (pw == PW_FORCE_BOON && gearChange != GEARCHANGE_REMOVE)
					activator->client->pers.teamState.boonPickups++;
			}
		}
	}

	if (doAmmo) {
		for (ammo_t a = AMMO_NONE; a < AMMO_MAX; a++) {
			if (ammoTypes & (1 << a)) {
				if (gearChange == GEARCHANGE_ADD)
					Add_Ammo(activator, a, ammoAmount);
				else if (gearChange == GEARCHANGE_REMOVE)
					Add_Ammo(activator, a, -ammoAmount);
				else if (gearChange == GEARCHANGE_SET)
					activator->client->ps.ammo[a] = ammoAmount;
			}
		}
	}

	if (doForce) {
		powersList_t *powers = &powersLists[ent - g_entities];
		for (forcePowers_t f = FP_HEAL; f < NUM_FORCE_POWERS; f++) {
			if (powers->levels[f]) {
				if (gearChange == GEARCHANGE_ADD)
					activator->client->ps.fd.forcePowerLevel[f] += powers->levels[f];
				if (gearChange == GEARCHANGE_REMOVE)
					activator->client->ps.fd.forcePowerLevel[f] -= powers->levels[f];
				else if (gearChange == GEARCHANGE_SET)
					activator->client->ps.fd.forcePowerLevel[f] = powers->levels[f];
				if (activator->client->ps.fd.forcePowerLevel[f] <= 0) {
					activator->client->ps.fd.forcePowerLevel[f] = 0;
					activator->client->ps.fd.forcePowersKnown &= ~(1 << f);
				}
				else {
					activator->client->ps.fd.forcePowersKnown |= (1 << f);
					if (activator->client->ps.fd.forcePowerLevel[f] > 3)
						activator->client->ps.fd.forcePowerLevel[f] = 3;
				}
			}
			else if (gearChange == GEARCHANGE_SET) {
				activator->client->ps.fd.forcePowerLevel[f] = 0;
				activator->client->ps.fd.forcePowersKnown &= ~(1 << f);
			}
		}
	}

	if (doHealth) {
		if (gearChange == GEARCHANGE_ADD)
			activator->health += health;
		else if (gearChange == GEARCHANGE_REMOVE)
			activator->health -= health;
		else if (gearChange == GEARCHANGE_SET)
			activator->health = health;
		if (g_gametype.integer == GT_SIEGE && activator - g_entities < MAX_CLIENTS && activator->client->siegeClass != -1) {
			if (activator->health > bgSiegeClasses[activator->client->siegeClass].maxhealth)
				activator->health = bgSiegeClasses[activator->client->siegeClass].maxhealth;
		}
		else {
			if (activator->maxHealth && activator->health > activator->maxHealth)
				activator->health = activator->maxHealth;
		}
		if (activator->health < 0)
			activator->health = 0;
		activator->client->ps.stats[STAT_HEALTH] = activator->health;
	}

	if (doArmor) {
		if (gearChange == GEARCHANGE_ADD)
			activator->client->ps.stats[STAT_ARMOR] += armor;
		else if (gearChange == GEARCHANGE_REMOVE)
			activator->client->ps.stats[STAT_ARMOR] -= armor;
		else if (gearChange == GEARCHANGE_SET)
			activator->client->ps.stats[STAT_ARMOR] = armor;
		if (g_gametype.integer == GT_SIEGE && activator - g_entities < MAX_CLIENTS && activator->client->siegeClass != -1) {
			if (activator->client->ps.stats[STAT_ARMOR] > bgSiegeClasses[activator->client->siegeClass].maxarmor)
				activator->client->ps.stats[STAT_ARMOR] = bgSiegeClasses[activator->client->siegeClass].maxarmor;
		}
		else {
			if (activator->client->ps.stats[STAT_ARMOR] > 100)
				activator->client->ps.stats[STAT_ARMOR] = 100;
		}
		if (activator->client->ps.stats[STAT_ARMOR] < 0)
			activator->client->ps.stats[STAT_ARMOR] = 0;
	}
}

void SP_target_gear(gentity_t *ent) {
	char *s = NULL;

	G_SpawnString("type", "", &s);
	if (!VALIDSTRING(s)) {
		Com_Printf("Target_gear with no type!\n");
		return;
	}

	gearChange_t *gearChange = (gearChange_t *)&ent->genericValue1;

	if (stristr(s, "add")) {
		*gearChange = GEARCHANGE_ADD;
	} else if (stristr(s, "remove")) {
		*gearChange = GEARCHANGE_REMOVE;
	} else if (stristr(s, "set")) {
		*gearChange = GEARCHANGE_SET;
	} else {
		Com_Printf("Bad target_gear type \"%s\"!\n", s);
		return;
	}

	qboolean *doWeapons = (qboolean *)&ent->genericValue2;
	int *weapons = &ent->genericValue3;
	qboolean *doItems = (qboolean *)&ent->genericValue4;
	int *items = &ent->genericValue5;
	qboolean *doPowerups = (qboolean *)&ent->genericValue6;
	int *powerups = &ent->genericValue7;
	qboolean *doAmmo = (qboolean *)&ent->genericValue8;
	int *ammoTypes = &ent->genericValue9;
	int *ammoAmount = &ent->genericValue10;
	qboolean *doHealth = (qboolean *)&ent->genericValue11;
	int *health = &ent->genericValue12;
	qboolean *doArmor = (qboolean *)&ent->genericValue13;
	int *armor = &ent->genericValue14;
	qboolean *doForce = (qboolean *)&ent->genericValue15;

	G_SpawnString("weapons", "", &s);
	if (VALIDSTRING(s)) {
		*doWeapons = qtrue;
		*weapons = BG_SiegeTranslateGenericTable(s, WPTable, qtrue);
	}

	G_SpawnString("items", "", &s);
	if (VALIDSTRING(s)) {
		*doItems = qtrue;
		*items = BG_SiegeTranslateGenericTable(s, HoldableTable, qtrue);
	}

	G_SpawnString("powerups", "", &s);
	if (VALIDSTRING(s)) {
		*doPowerups = qtrue;
		*powerups = BG_SiegeTranslateGenericTable(s, PowerupTable, qtrue);
	}

	G_SpawnString("ammo", "", &s);
	if (VALIDSTRING(s)) {
		*doAmmo = qtrue;
		*ammoTypes = BG_SiegeTranslateGenericTable(s, AmmoTable, qtrue);
		G_SpawnString("ammoamount", "", &s);
		if (VALIDSTRING(s))
			*ammoAmount = atoi(s);
	}

	G_SpawnString("force", "", &s);
	if (VALIDSTRING(s)) {
		*doForce = qtrue;
		TranslateForcePowers(s, &powersLists[ent - g_entities]);
	}

	G_SpawnString("health", "", &s);
	if (VALIDSTRING(s)) {
		*doHealth = qtrue;
		*health = atoi(s);
	}

	G_SpawnString("armor", "", &s);
	if (VALIDSTRING(s)) {
		*doArmor = qtrue;
		*armor = atoi(s);
	}

	ent->use = Use_target_gear;
}


//==========================================================

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8) NO_RETRIGGER

NO_RETRIGGER - Keeps the delay from resetting the time if it is
activated again while it is counting down to an event.

"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
void Think_Target_Delay( gentity_t *ent )
{
	if (!ent->canContinue)
	{
		ent->canContinue = 1;
		return;
	}
	G_UseTargets( ent, ent->activator );
}

void Use_Target_Delay( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if (ent->nextthink > level.time && (ent->spawnflags & 1))
	{ //Leave me alone, I am thinking.
		return;
	}
	G_ActivateBehavior(ent,BSET_USE);
	ent->nextthink = level.time + ( ent->wait + ent->random * crandom() ) * 1000;
	ent->think = Think_Target_Delay;
	ent->activator = activator;
	ent->canContinue = 1;
}

void SP_target_delay( gentity_t *ent ) {
	// check delay for backwards compatability
	if ( !G_SpawnFloat( "delay", "0", &ent->wait ) ) {
		G_SpawnFloat( "wait", "1", &ent->wait );
	}
	G_SpawnInt("canContinue", "1", &ent->canContinue);
	if ( !ent->wait ) {
		ent->wait = 1;
	}
	if (level.siegeMap == SIEGEMAP_NAR) {
		if (!Q_stricmp(ent->targetname, "obj1") && !Q_stricmp(ent->target, "obj1delayfield")) {
			ent->wait = 12;
		}
		else if (!Q_stricmp(ent->targetname, "objective3") && !Q_stricmp(ent->target, "fieldtobridge")) {
			ent->wait = 12;
		}
	}
	ent->use = Use_Target_Delay;
}

/*QUAKED target_delay_cancel (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM x x x x INACTIVE
This doesn't perform any actions except fire its targets.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them

INACTIVE  Can't be used until activated

wait - set to -1 to use it only once
*/
void target_delay_cancel_use(gentity_t *self, gentity_t *other, gentity_t *activator) {
	qboolean ranscript = qfalse;
	if ((self->spawnflags & 1) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_RED) {
		return;
	}
	if ((self->spawnflags & 2) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_BLUE) {
		return;
	}

	if (self->flags & FL_INACTIVE)
	{//set by target_deactivate
		return;
	}

	ranscript = G_ActivateBehavior(self, BSET_USE);
	if (self->wait == -1)
	{//never use again
		if (ranscript)
		{//crap, can't remove!
			self->use = NULL;
		}
		else
		{//remove
			self->think = G_FreeEntity;
			self->nextthink = level.time + FRAMETIME;
		}
	}
	if (self->spawnflags & 4) {
		gentity_t	*ent;

		ent = G_PickTarget(self->target);
		if (ent && ent->use) {
			TargetDelayCancelUse(ent, self, activator);
		}
		return;
	}
	G_DelayCancelTargets(self, activator);
}

void SP_target_delay_cancel(gentity_t *self) {
	self->use = target_delay_cancel_use;
	if (self->spawnflags & 128)
	{
		self->flags |= FL_INACTIVE;
	}
}

//==========================================================

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8)
"count" number of points to add, default 1

The activator is given this many points.
*/
void Use_Target_Score (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	AddScore( activator, ent->r.currentOrigin, ent->count );
}

void SP_target_score( gentity_t *ent ) {
	if ( !ent->count ) {
		ent->count = 1;
	}
	ent->use = Use_Target_Score;
}


//==========================================================

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) redteam blueteam private
"message"	text to print
"wait"		don't fire off again if triggered within this many milliseconds ago
If "private", only the activator gets the message.  If no checks, all clients get the message.
*/
void Use_Target_Print (gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	if (!ent || !ent->inuse)
	{
		Com_Printf("ERROR: Bad ent in Use_Target_Print");
		return;
	}

	if (ent->wait)
	{
		if (ent->genericValue14 >= level.time)
		{
			return;
		}
		ent->genericValue14 = level.time + ent->wait;
	}

#if 0//#ifndef FINAL_BUILD
	/*
	if (!ent || !ent->inuse)
	{
		Com_Error(ERR_DROP, "Bad ent in Use_Target_Print");
	}
	else if (!activator || !activator->inuse)
	{
		Com_Error(ERR_DROP, "Bad activator in Use_Target_Print");
	}

	if (ent->genericValue15 > level.time)
	{
		Com_Printf("TARGET PRINT ERRORS:\n");
		if (activator && activator->classname && activator->classname[0])
		{
			Com_Printf("activator classname: %s\n", activator->classname);
		}
		if (activator && activator->target && activator->target[0])
		{
			Com_Printf("activator target: %s\n", activator->target);
		}
		if (activator && activator->targetname && activator->targetname[0])
		{
			Com_Printf("activator targetname: %s\n", activator->targetname);
		}
		if (ent->targetname && ent->targetname[0])
		{
			Com_Printf("print targetname: %s\n", ent->targetname);
		}
		Com_Error(ERR_DROP, "target_print used in quick succession, fix it! See the console for details.");
	}
	ent->genericValue15 = level.time + 5000;
	*/
#endif

	const char *message = ent->message;

	if (g_gametype.integer == GT_SIEGE && level.siegeMap == SIEGEMAP_CARGO) {
		static qboolean topHacked = qfalse, node1Hacked = qfalse;
		qboolean topOrNode1HackedThisFrame = qfalse;
		if (!Q_stricmp(ent->targetname, "node2hacked")) {
			RemoveIconsWithTargetname("node2hackicon");
			RemoveIconsWithTargetname("node1hackicon");
			RemoveIconsWithTargetname("powernode2sideicon");
			SetUsableFromTargetname("node2greenhacktexture", qtrue);
			SetUsableFromTargetname("node2redhacktexture", qfalse);
			SetUsableFromTargetname("node1greenhacktexture", qtrue);
			SetUsableFromTargetname("node1redhacktexture", qfalse);
			SetUsableFromTargetname("topgreenhacktexture", qtrue);
			SetUsableFromTargetname("topredhacktexture", qfalse);
			if (topHacked && !node1Hacked)
				message = "Tunnel doors unlocked!";
			else if (node1Hacked && !topHacked)
				message = "Node 2 vent lock overridden!";
			else
				message = "Tunnel and node 2 vent unlocked!";
		}
		else if (!Q_stricmp(ent->targetname, "tophacked")) {
			topHacked = qtrue;
			topOrNode1HackedThisFrame = qtrue;
			SetUsableFromTargetname("topgreenhacktexture", qtrue);
			SetUsableFromTargetname("topredhacktexture", qfalse);
			RemoveIconsWithTargetname("powernode2sideicon");
		}
		else if (!Q_stricmp(ent->targetname, "node1hacked")) {
			node1Hacked = qtrue;
			topOrNode1HackedThisFrame = qtrue;
			SetUsableFromTargetname("node1greenhacktexture", qtrue);
			SetUsableFromTargetname("node1redhacktexture", qfalse);
			RemoveIconsWithTargetname("node1hackicon");
		}

		if (topOrNode1HackedThisFrame && topHacked && node1Hacked) {
			RemoveIconsWithTargetname("node2hackicon");
			SetUsableFromTargetname("topgreenhacktexture", qtrue);
			SetUsableFromTargetname("topredhacktexture", qfalse);
			SetUsableFromTargetname("node2greenhacktexture", qtrue);
			SetUsableFromTargetname("node2redhacktexture", qfalse);
			SetUsableFromTargetname("node1greenhacktexture", qtrue);
			SetUsableFromTargetname("node1redhacktexture", qfalse);
		}
	}

	G_ActivateBehavior(ent,BSET_USE);
	if ( ( ent->spawnflags & 4 ) ) 
	{//private, to one client only
		if (!activator || !activator->inuse)
		{
			Com_Printf("ERROR: Bad activator in Use_Target_Print");
		}
		if ( activator && activator->client )
		{//make sure there's a valid client ent to send it to
			if (ent->message[0] == '@' && ent->message[1] != '@')
			{
				trap_SendServerCommand( activator-g_entities, va("cps \"%s\"", message ));
			}
			else
			{
				trap_SendServerCommand( activator-g_entities, va("cp \"%s\"", message));
			}
		}
		//NOTE: change in functionality - if there *is* no valid client ent, it won't send it to anyone at all
		return;
	}

	if ( ent->spawnflags & 3 ) {
		if ( ent->spawnflags & 1 ) {
			if (ent->message[0] == '@' && ent->message[1] != '@')
			{
				G_TeamCommand( TEAM_RED, va("cps \"%s\"", message) );
			}
			else
			{
				G_TeamCommand( TEAM_RED, va("cp \"%s\"", message) );
			}
		}
		if ( ent->spawnflags & 2 ) {
			if (ent->message[0] == '@' && ent->message[1] != '@')
			{
				G_TeamCommand( TEAM_BLUE, va("cps \"%s\"", message) );
			}
			else
			{
				G_TeamCommand( TEAM_BLUE, va("cp \"%s\"", message) );
			}
		}
		return;
	}

	if (ent->message[0] == '@' && ent->message[1] != '@')
	{
		trap_SendServerCommand( -1, va("cps \"%s\"", message));
	}
	else
	{
		trap_SendServerCommand( -1, va("cp \"%s\"", message));
	}
}

void SP_target_print( gentity_t *ent ) {
	if (ent->wait >= 0 && ent->wait <= 999)
	{
		ent->wait = 500;
	}
	ent->use = Use_Target_Print;
}


//==========================================================


/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off global activator
"noise"		wav file to play

A global sound will play full volume throughout the level.
Activator sounds will play on the player that activated the target.
Global and activator sounds can't be combined with looping.
Normal sounds play each time the target is used.
Looped sounds will be toggled by use functions.
Multiple identical looping sounds will just increase volume without any speed cost.
"wait" : Seconds between auto triggerings, 0 = don't auto trigger
"random"	wait variance, default is 0
*/
void Use_Target_Speaker (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	G_ActivateBehavior(ent,BSET_USE);

	if (ent->spawnflags & 3) {	// looping sound toggles
		if (ent->s.loopSound)
		{
			ent->s.loopSound = 0;	// turn it off
			ent->s.loopIsSoundset = qfalse;
			ent->s.trickedentindex = 1;
		}
		else
		{
			ent->s.loopSound = ent->noise_index;	// start it
			ent->s.loopIsSoundset = qfalse;
			ent->s.trickedentindex = 0;
		}
	}else {	// normal sound
		if ( ent->spawnflags & 8 ) {
			G_AddEvent( activator, EV_GENERAL_SOUND, ent->noise_index );
		} else if (ent->spawnflags & 4) {
			G_AddEvent( ent, EV_GLOBAL_SOUND, ent->noise_index );
		} else {
			G_AddEvent( ent, EV_GENERAL_SOUND, ent->noise_index );
		}
	}
}

void SP_target_speaker( gentity_t *ent ) {
	char	buffer[MAX_QPATH];
	char	*s;

	G_SpawnFloat( "wait", "0", &ent->wait );
	G_SpawnFloat( "random", "0", &ent->random );

	if ( G_SpawnString ( "soundSet", "", &s ) )
	{	// this is a sound set
		ent->s.soundSetIndex = G_SoundSetIndex(s);
		ent->s.eFlags = EF_PERMANENT;
		VectorCopy( ent->s.origin, ent->s.pos.trBase );
		trap_LinkEntity (ent);
		return;
	}

	if ( !G_SpawnString( "noise", "NOSOUND", &s ) ) {
		G_Error( "target_speaker without a noise key at %s", vtos( ent->s.origin ) );
	}

	// force all client reletive sounds to be "activator" speakers that
	// play on the entity that activates it
	if ( s[0] == '*' ) {
		ent->spawnflags |= 8;
	}

	Q_strncpyz( buffer, s, sizeof(buffer) );

	ent->noise_index = G_SoundIndex(buffer);

	// a repeating speaker can be done completely client side
	ent->s.eType = ET_SPEAKER;
	ent->s.eventParm = ent->noise_index;
	ent->s.frame = ent->wait * 10;
	ent->s.clientNum = ent->random * 10;


	// check for prestarted looping sound
	if ( ent->spawnflags & 1 ) {
		ent->s.loopSound = ent->noise_index;
		ent->s.loopIsSoundset = qfalse;
	}

	ent->use = Use_Target_Speaker;

	if (ent->spawnflags & 4) {
		ent->r.svFlags |= SVF_BROADCAST;
	}

	VectorCopy( ent->s.origin, ent->s.pos.trBase );

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	trap_LinkEntity( ent );
}



//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON
When triggered, fires a laser.  You can either set a target or a direction.
*/
void target_laser_think (gentity_t *self) {
	vec3_t	end;
	trace_t	tr;
	vec3_t	point;

	// if pointed at another entity, set movedir to point at it
	if ( self->enemy ) {
		VectorMA (self->enemy->s.origin, 0.5, self->enemy->r.mins, point);
		VectorMA (point, 0.5, self->enemy->r.maxs, point);
		VectorSubtract (point, self->s.origin, self->movedir);
		VectorNormalize (self->movedir);
	}

	// fire forward and see what we hit
	VectorMA (self->s.origin, 2048, self->movedir, end);

	trap_Trace( &tr, self->s.origin, NULL, NULL, end, self->s.number, CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE);

	if ( tr.entityNum ) {
		// hurt it if we can
		G_Damage ( &g_entities[tr.entityNum], self, self->activator, self->movedir, 
			tr.endpos, self->damage, DAMAGE_NO_KNOCKBACK, MOD_TARGET_LASER);
	}

	VectorCopy (tr.endpos, self->s.origin2);

	trap_LinkEntity( self );
	self->nextthink = level.time + FRAMETIME;
}

void target_laser_on (gentity_t *self)
{
	if (!self->activator)
		self->activator = self;
	target_laser_think (self);
}

void target_laser_off (gentity_t *self)
{
	trap_UnlinkEntity( self );
	self->nextthink = 0;
}

void target_laser_use (gentity_t *self, gentity_t *other, gentity_t *activator)
{
	self->activator = activator;
	if ( self->nextthink > 0 )
		target_laser_off (self);
	else
		target_laser_on (self);
}

void target_laser_start (gentity_t *self)
{
	gentity_t *ent;

	self->s.eType = ET_BEAM;

	if (self->target) {
		ent = G_Find (NULL, FOFS(targetname), self->target);
		if (!ent) {
			G_Printf ("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
		}
		self->enemy = ent;
	} else {
		G_SetMovedir (self->s.angles, self->movedir);
	}

	self->use = target_laser_use;
	self->think = target_laser_think;

	if ( !self->damage ) {
		self->damage = 1;
	}

	if (self->spawnflags & 1)
		target_laser_on (self);
	else
		target_laser_off (self);
}

void SP_target_laser (gentity_t *self)
{
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->nextthink = level.time + FRAMETIME;
}


//==========================================================

void target_teleporter_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	gentity_t	*dest;

	if (!activator->client)
		return;

	G_ActivateBehavior(self,BSET_USE);

	dest = 	G_PickTarget( self->target );
	if (!dest) {
		G_Printf ("Couldn't find teleporter destination\n");
		return;
	}

	TeleportPlayer( activator, dest->s.origin, dest->s.angles );
}

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8)
The activator will be teleported away.
*/
void SP_target_teleporter( gentity_t *self ) {
	if (!self->targetname)
		G_Printf("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

	self->use = target_teleporter_use;
}

//==========================================================


/*QUAKED target_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM x x x x INACTIVE
This doesn't perform any actions except fire its targets.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them

  INACTIVE  Can't be used until activated

wait - set to -1 to use it only once
*/
void target_relay_use (gentity_t *self, gentity_t *other, gentity_t *activator) {
	qboolean ranscript = qfalse;
	if ( ( self->spawnflags & 1 ) && activator->client 
		&& activator->client->sess.sessionTeam != TEAM_RED ) {
		return;
	}
	if ( ( self->spawnflags & 2 ) && activator->client 
		&& activator->client->sess.sessionTeam != TEAM_BLUE ) {
		return;
	}

	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	ranscript = G_ActivateBehavior( self, BSET_USE );
	if ( self->wait == -1 )
	{//never use again
		if ( ranscript )
		{//crap, can't remove!
			self->use = NULL;
		}
		else
		{//remove
			self->think = G_FreeEntity;
			self->nextthink = level.time + FRAMETIME;
		}
	}

	if ( self->spawnflags & 4 ) {
		gentity_t	*ent;

		ent = G_PickTarget( self->target );
		if ( ent && ent->use ) {
			GlobalUse( ent, self, activator );
		}
		return;
	}
	G_UseTargets (self, activator);
}

void SP_target_relay (gentity_t *self) {
	// duo: new "setteamallow" key -- this target_relay will set the teamallow (known as alliedTeam in the gentity_t struct)
	// which means we can target a door to enable a certain team to use it but not actually unlock it
	G_SpawnInt("setteamallow", "0", &self->genericValue17);
	if (self->genericValue17 < TEAM_FREE || self->genericValue17 > TEAM_BLUE)
		self->genericValue17 = TEAM_FREE;

	// duo: new "justopen" key -- when targeting a LOCKED door, this target_relay will simply open it once instead of unlocking it
	G_SpawnInt("justopen", "0", &self->genericValue16);

	self->use = target_relay_use;
	if ( self->spawnflags&128 )
	{
		self->flags |= FL_INACTIVE;
	}
}

void target_icontoggle_use(gentity_t *self, gentity_t *other, gentity_t *activator) {
	assert(self);
	if (!self || !VALIDSTRING(self->target) || self->flags & FL_INACTIVE)
		return;

	qboolean enable = !!(self->spawnflags & 1);
	qboolean disable = !!(self->spawnflags & 2);
	if (enable && disable) // ?
		enable = disable = qfalse;

	gentity_t *targ;
	targ = G_Find(NULL, FOFS(targetname), self->target);

	while (targ) {
		if (!enable && (targ->s.eFlags & EF_RADAROBJECT || disable))
			targ->s.eFlags &= ~EF_RADAROBJECT;
		else
			targ->s.eFlags |= EF_RADAROBJECT;
		targ = G_Find(targ, FOFS(targetname), self->target);
	}
}

void SP_target_icontoggle(gentity_t *self) {
	self->use = target_icontoggle_use;
	if (self->spawnflags & 128)
		self->flags |= FL_INACTIVE;
}


//==========================================================

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8)
Kills the activator.
*/
void target_kill_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	G_ActivateBehavior(self,BSET_USE);
	G_Damage ( activator, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
}

void SP_target_kill( gentity_t *self ) {
	self->use = target_kill_use;
}

/*QUAKED target_position (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
*/
void SP_target_position( gentity_t *self ){
	G_SetOrigin( self, self->s.origin );
}

/*QUAKED target_location (0 0.5 0) (-8 -8 -8) (8 8 8)
Set "message" to the name of this location.
Set "count" to 0-7 for color.
0:white 1:red 2:green 3:yellow 4:blue 5:cyan 6:magenta 7:white

Closest target_location in sight used for the location, if none
in site, closest in distance
*/
void SP_target_location( gentity_t *self ) {
	if ( self->targetname && self->targetname[0] ) {
		SP_target_position( self );
		return;
	} else {
		static qboolean didwarn = qfalse;

		if ( !VALIDSTRING( self->message ) ) {
			G_Printf( "target_location with no message at %s\n", vtos( self->s.origin ) );
			G_FreeEntity( self );
			return;
		}

		if ( level.locations.legacy.num >= MAX_LOCATIONS ) {
			if ( !didwarn ) {
				G_Printf( "Maximum target_locations hit (%d)! Remaining locations will be removed.\n", MAX_LOCATIONS );
				didwarn = qtrue;
			}

			G_FreeEntity( self );
			return;
		}

		VectorCopy( self->s.origin, level.locations.legacy.data[level.locations.legacy.num].origin );
		Q_strncpyz( level.locations.legacy.data[level.locations.legacy.num].message, self->message, sizeof( level.locations.legacy.data[level.locations.legacy.num].message ) );
		level.locations.legacy.data[level.locations.legacy.num].count = Com_Clampi( 0, 9, self->count );

		level.locations.legacy.num++;

		G_FreeEntity( self );
	}
}

/*QUAKED target_counter (1.0 0 0) (-4 -4 -4) (4 4 4) x x x x x x x INACTIVE
Acts as an intermediary for an action that takes multiple inputs.

INACTIVE cannot be used until used by a target_activate

target2 - what the counter should fire each time it's incremented and does NOT reach it's count

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.

bounceCount - number of times the counter should reset to it's full count when it's done
*/
extern void G_DebugPrint( int level, const char *format, ... );
void target_counter_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( self->count == 0 )
	{
		return;
	}
	
	self->count--;

	if ( activator )
	{
		G_DebugPrint( WL_VERBOSE, "target_counter %s used by %s (%d/%d)\n", self->targetname, activator->targetname, (self->genericValue1-self->count), self->genericValue1 );
	}

	if ( self->count )
	{
		if ( self->target2 )
		{
			G_UseTargets2( self, activator, self->target2 );
		}
		return;
	}
	
	G_ActivateBehavior( self,BSET_USE );

	if ( self->spawnflags & 128 )
	{
		self->flags |= FL_INACTIVE;
	}

	self->activator = activator;
	G_UseTargets( self, activator );

	if ( self->count == 0 )
	{
		if ( self->bounceCount == 0 )
		{
			return;
		}
		self->count = self->genericValue1;
		if ( self->bounceCount > 0 )
		{//-1 means bounce back forever
			self->bounceCount--; 
		}
	}
}

void SP_target_counter (gentity_t *self)
{
	self->wait = -1;
	if (!self->count)
	{
		self->count = 2;
	}
	//if ( self->bounceCount > 0 )//let's always set this anyway
	{//we will reset when we use up our count, remember our initial count
		self->genericValue1 = self->count;
	}

	self->use = target_counter_use;
}

/*QUAKED target_random (.5 .5 .5) (-4 -4 -4) (4 4 4) USEONCE
Randomly fires off only one of it's targets each time used

USEONCE	set to never fire again
*/

void target_random_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int			t_count = 0, pick;
	gentity_t	*t = NULL;

	G_ActivateBehavior(self,BSET_USE);

	if(self->spawnflags & 1)
	{
		self->use = 0;
	}

	while ( (t = G_Find (t, FOFS(targetname), self->target)) != NULL )
	{
		if (t != self)
		{
			t_count++;
		}
	}

	if(!t_count)
	{
		return;
	}

	if(t_count == 1)
	{
		G_UseTargets (self, activator);
		return;
	}

	//FIXME: need a seed
	if (self->spawnflags & 2) {
		int seed = trap_Milliseconds() % 256;
		pick = (int)((Q_random(&seed)*t_count) + 1);
	}
	else
		pick = Q_irand(1, t_count);
	t_count = 0;
	while ( (t = G_Find (t, FOFS(targetname), self->target)) != NULL )
	{
		if (t != self)
		{
			t_count++;
		}
		else
		{
			continue;
		}
		
		if (t == self)
		{
		}
		else if(t_count == pick)
		{
			if (t->use != NULL)	// check can be omitted
			{
				GlobalUse(t, self, activator);
				return;
			}
		}

		if (!self->inuse)
		{
			Com_Printf("entity was removed while using targets\n");
			return;
		}
	}
}

void SP_target_random (gentity_t *self)
{
	self->use = target_random_use;
}

int	numNewICARUSEnts = 0;
void scriptrunner_run (gentity_t *self)
{
	if ( self->count != -1 )
	{
		if ( self->count <= 0 )
		{
			self->use = 0;
			self->behaviorSet[BSET_USE] = NULL;
			return;
		}
		else
		{
			--self->count;
		}
	}

	if (self->behaviorSet[BSET_USE])
	{
		if ( self->spawnflags & 1 )
		{
			if ( !self->activator )
			{
				if (g_developer.integer)
				{
					Com_Printf("target_scriptrunner tried to run on invalid entity!\n");
				}
				return;
			}

			//if ( !self->activator->sequencer || !self->activator->taskManager )
			if (!trap_ICARUS_IsInitialized(self->s.number))
			{//Need to be initialized through ICARUS
				if ( !self->activator->script_targetname || !self->activator->script_targetname[0] )
				{
					//We don't have a script_targetname, so create a new one
					self->activator->script_targetname = va( "newICARUSEnt%d", numNewICARUSEnts++ );
				}

				if ( trap_ICARUS_ValidEnt( self->activator ) )
				{
					trap_ICARUS_InitEnt( self->activator );
				}
				else
				{
					if (g_developer.integer)
					{
						Com_Printf("target_scriptrunner tried to run on invalid ICARUS activator!\n");
					}
					return;
				}
			}

			if (g_developer.integer)
			{
				Com_Printf( "target_scriptrunner running %s on activator %s\n", self->behaviorSet[BSET_USE], self->activator->targetname );
			}
			trap_ICARUS_RunScript( self->activator, va( "%s/%s", Q3_SCRIPT_DIR, self->behaviorSet[BSET_USE] ) );
		}
		else
		{
			if ( g_developer.integer && self->activator )
			{
				Com_Printf( "target_scriptrunner %s used by %s\n", self->targetname, self->activator->targetname );
			}
			G_ActivateBehavior( self, BSET_USE );
		}
	}

	if ( self->wait )
	{
		self->nextthink = level.time + self->wait;
	}
}

void target_scriptrunner_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	if ( self->nextthink > level.time )
	{
		return;
	}

	self->activator = activator;
	self->enemy = other;
	if ( self->delay )
	{//delay before firing scriptrunner
		self->think = scriptrunner_run;
		self->nextthink = level.time + self->delay;
	}
	else
	{
		scriptrunner_run (self);
	}
}

/*QUAKED target_scriptrunner (1 0 0) (-4 -4 -4) (4 4 4) runonactivator x x x x x x INACTIVE
--- SPAWNFLAGS ---
runonactivator - Will run the script on the entity that used this or tripped the trigger that used this
INACTIVE - start off

----- KEYS ------
Usescript - Script to run when used
count - how many times to run, -1 = infinite.  Default is once
wait - can't be used again in this amount of seconds (Default is 1 second if it's multiple-use)
delay - how long to wait after use to run script

*/
void SP_target_scriptrunner( gentity_t *self )
{
	float v;
	if ( self->spawnflags & 128 )
	{
		self->flags |= FL_INACTIVE;
	}

	if ( !self->count )
	{
		self->count = 1;//default 1 use only
	}

	// FIXME: this is a hack... because delay is read in as an int, so I'm bypassing that because it's too late in the project to change it and I want to be able to set less than a second delays
	// no one should be setting a radius on a scriptrunner, if they are this would be bad, take this out for the next project
	v = 0.0f;
	G_SpawnFloat( "delay", "0", &v );
	self->delay = v * 1000;//sec to ms
	self->wait *= 1000;//sec to ms

	G_SetOrigin( self, self->s.origin );
	self->use = target_scriptrunner_use;
}

void G_SetActiveState(char *targetstring, qboolean actState)
{
	gentity_t	*target = NULL;
	while( NULL != (target = G_Find(target, FOFS(targetname), targetstring)) )
	{
		target->flags = actState ? (target->flags&~FL_INACTIVE) : (target->flags|FL_INACTIVE);
	}
}

#define ACT_ACTIVE		qtrue
#define ACT_INACTIVE	qfalse

void target_activate_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self,BSET_USE);

	G_SetActiveState(self->target, ACT_ACTIVE);
}

void target_deactivate_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self,BSET_USE);

	G_SetActiveState(self->target, ACT_INACTIVE);
}

//FIXME: make these apply to doors, etc too?
/*QUAKED target_activate (1 0 0) (-4 -4 -4) (4 4 4)
Will set the target(s) to be usable/triggerable
*/
void SP_target_activate( gentity_t *self )
{
	G_SetOrigin( self, self->s.origin );
	self->use = target_activate_use;
}

/*QUAKED target_deactivate (1 0 0) (-4 -4 -4) (4 4 4)
Will set the target(s) to be non-usable/triggerable
*/
void SP_target_deactivate( gentity_t *self )
{
	G_SetOrigin( self, self->s.origin );
	self->use = target_deactivate_use;
}

void target_level_change_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self,BSET_USE);

	trap_SendConsoleCommand(EXEC_NOW, va("map %s", self->message));
}

/*QUAKED target_level_change (1 0 0) (-4 -4 -4) (4 4 4)
"mapname" - Name of map to change to
*/
void SP_target_level_change( gentity_t *self )
{
	char *s;

	G_SpawnString( "mapname", "", &s );
	self->message = G_NewString(s);

	if ( !self->message || !self->message[0] )
	{
		G_Error( "target_level_change with no mapname!\n");
		return;
	}

	G_SetOrigin( self, self->s.origin );
	self->use = target_level_change_use;
}

void target_play_music_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self,BSET_USE);
	trap_SetConfigstring( CS_MUSIC, self->message );
}

/*QUAKED target_play_music (1 0 0) (-4 -4 -4) (4 4 4)
target_play_music
Plays the requested music files when this target is used.

"targetname"
"music"		music WAV or MP3 file ( music/introfile.mp3 [optional]  music/loopfile.mp3 )

If an intro file and loop file are specified, the intro plays first, then the looping
portion will start and loop indefinetly.  If no introfile is entered, only the loopfile
will play.
*/
void SP_target_play_music( gentity_t *self )
{
	char *s;

	G_SetOrigin( self, self->s.origin );
	if (!G_SpawnString( "music", "", &s ))
	{
		G_Error( "target_play_music without a music key at %s", vtos( self->s.origin ) );
	}

	self->message = G_NewString(s);

	self->use = target_play_music_use;
}
