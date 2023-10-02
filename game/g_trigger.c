// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

#define LIFT_LAME_DETECTION_DISTANCE 1200

int gTrigFallSound;

int hangarHackTime = 0;

void InitTrigger( gentity_t *self ) {
	if (!VectorCompare (self->s.angles, vec3_origin))
		G_SetMovedir (self->s.angles, self->movedir);

	trap_SetBrushModel( self, self->model );
	self->r.contents = CONTENTS_TRIGGER;		// replaces the -1 from trap_SetBrushModel
	self->r.svFlags = SVF_NOCLIENT;

	if(self->spawnflags & 128)
	{
		self->flags |= FL_INACTIVE;
	}
}

// the wait time has passed, so set back up for another activation
void multi_wait( gentity_t *ent ) {
	ent->nextthink = 0;
}

void trigger_cleared_fire (gentity_t *self);

// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void multi_trigger_run( gentity_t *ent ) 
{
	ent->think = 0;

	G_ActivateBehavior( ent, BSET_USE );

	if ( ent->soundSet && ent->soundSet[0] )
	{
		trap_SetConfigstring( CS_GLOBAL_AMBIENT_SET, ent->soundSet );
	}

	if (ent->genericValue4)
	{ //we want to activate target3 for team1 or target4 for team2
		if (ent->genericValue4 == SIEGETEAM_TEAM1 &&
			ent->target3 && ent->target3[0])
		{
			G_UseTargets2(ent, ent->activator, ent->target3);
		}
		else if (ent->genericValue4 == SIEGETEAM_TEAM2 &&
			ent->target4 && ent->target4[0])
		{
			G_UseTargets2(ent, ent->activator, ent->target4);
		}

		ent->genericValue4 = 0;
	}

	G_UseTargets (ent, ent->activator);
	if ( ent->noise_index ) 
	{
		G_Sound( ent->activator, CHAN_AUTO, ent->noise_index );
	}

	if ( ent->target2 && ent->target2[0] && ent->wait >= 0 )
	{
		ent->think = trigger_cleared_fire;
		ent->nextthink = level.time + ent->speed;
	}
	else if ( ent->wait > 0 ) 
	{
		if ( ent->painDebounceTime != level.time )
		{//first ent to touch it this frame
			ent->nextthink = level.time + ( ent->wait + ent->random * crandom() ) * 1000;
			ent->painDebounceTime = level.time;
		}
	} 
	else if ( ent->wait < 0 )
	{
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->r.contents &= ~CONTENTS_TRIGGER;//so the EntityContact trace doesn't have to be done against me
		ent->think = 0;
		ent->use = 0;
		//Don't remove, Icarus may barf?
	}
	if( ent->activator && ent->activator->client )
	{	// mark the trigger as being touched by the player
		ent->aimDebounceTime = level.time;
	}
}

//determine if the class given is listed in the string using the | formatting
qboolean G_NameInTriggerClassList(char *list, char *str)
{
	char cmp[MAX_STRING_CHARS];
	int i = 0;
	int j;

	while (list[i])
	{
        j = 0;
        while (list[i] && list[i] != '|')
		{
			cmp[j] = list[i];
			i++;
			j++;
		}
		cmp[j] = 0;

		if (!Q_stricmp(str, cmp))
		{ //found it
			return qtrue;
		}
		if (list[i] != '|')
		{ //reached the end and never found it
			return qfalse;
		}
		i++;
	}

	return qfalse;
}

extern qboolean gSiegeRoundBegun;
void SiegeItemRemoveOwner(gentity_t *ent, gentity_t *carrier);
extern void SiegeItemRespawnOnOriginalSpot(gentity_t *ent, gentity_t *carrier, gentity_t *overrideOriginEnt);
void multi_trigger( gentity_t *ent, gentity_t *activator ) 
{
	if (activator && activator->client && activator->client->emoted)
		return;

	qboolean haltTrigger = qfalse;
	short i1, i2;
	if ( ent->think == multi_trigger_run )
	{//already triggered, just waiting to run
		return;
	}

	if (g_gametype.integer == GT_SIEGE &&
		!gSiegeRoundBegun)
	{ //nothing can be used til the round starts.
		return;
	}

	if (g_gametype.integer == GT_SIEGE &&
		activator && activator->client &&
		ent->alliedTeam &&
		activator->client->sess.sessionTeam != ent->alliedTeam)
	{ //this team can't activate this trigger.
		return;
	}

	if (g_gametype.integer == GT_SIEGE &&
		ent->idealclass && ent->idealclass[0])
	{ //only certain classes can activate it
		if (!activator ||
			!activator->client ||
			activator->client->siegeClass < 0)
		{ //no class
			return;
		}
		if (activator->client->sess.sessionTeam == TEAM_RED && g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none"))
		{
			//get generic type of class we are currently(tech, assault, whatever) and return if the idealclass does not match
			i1 = bgSiegeClasses[activator->client->siegeClass].playerClass;
			i2 = bgSiegeClasses[BG_SiegeFindClassIndexByName(ent->idealclass)].playerClass;
			if (i1 != i2)
				return;
		}
		else if (activator->client->sess.sessionTeam == TEAM_BLUE && g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none"))
		{
			//get generic type of class we are currently(tech, assault, whatever) and return if the idealclass does not match
			i1 = bgSiegeClasses[activator->client->siegeClass].playerClass;
			i2 = bgSiegeClasses[BG_SiegeFindClassIndexByName(ent->idealclass)].playerClass;
			if (i1 != i2)
				return;
		}
		else if (!G_NameInTriggerClassList(bgSiegeClasses[activator->client->siegeClass].name, ent->idealclass))
		{ //wasn't in the list
			return;
		}
	}
	if (g_gametype.integer == GT_SIEGE && ent->idealClassType && ent->idealClassType >= CLASSTYPE_ASSAULT && activator->client->sess.sessionTeam == TEAM_RED)
	{
		i1 = bgSiegeClasses[activator->client->siegeClass].playerClass;
		if (i1 + 10 != ent->idealClassType)
		{
			return;
		}
	}
	if (g_gametype.integer == GT_SIEGE && ent->idealClassTypeTeam2 && ent->idealClassTypeTeam2 >= CLASSTYPE_ASSAULT && activator->client->sess.sessionTeam == TEAM_BLUE)
	{
		i1 = bgSiegeClasses[activator->client->siegeClass].playerClass;
		if (i1 + 10 != ent->idealClassTypeTeam2)
		{
			return;
		}
	}
	if (g_gametype.integer == GT_SIEGE && ent->genericValue1)
	{
		haltTrigger = qtrue;

		if (activator && activator->client &&
			activator->client->holdingObjectiveItem &&
			ent->targetname && ent->targetname[0])
		{
			gentity_t *objItem = &g_entities[activator->client->holdingObjectiveItem];

			if (objItem && objItem->inuse)
			{
				if (objItem->goaltarget && objItem->goaltarget[0] &&
					!Q_stricmp(ent->targetname, objItem->goaltarget))
				{
					if (objItem->genericValue7 != activator->client->sess.sessionTeam)
					{ //The carrier of the item is not on the team which disallows objective scoring for it
						if (objItem->target3 && objItem->target3[0])
						{ //if it has a target3, fire it off instead of using the trigger
							if (level.siegeMap == SIEGEMAP_DESERT && !Q_stricmp(objItem->target3, "c3podeliverprint"))
							{
								//droid part on desert
								char *part = NULL;
								if (objItem->model && objItem->model[0])
								{
									if (strstr(objItem->model, "arm"))
									{
										part = "arm";
									}
									else if (strstr(objItem->model, "head"))
									{
										part = "head";
									}
									else if (strstr(objItem->model, "leg"))
									{
										part = "leg";
									}
									else if (strstr(objItem->model, "torso"))
									{
										part = "torso";
									}
								}
								if (part && part[0])
								{
									objItem->siegeItemCarrierTime = 0;
									activator->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_PARTS]++;
									trap_SendServerCommand(-1, va("cp \"Protocol droid %s has been rescued!\n\"", part));
								}
								else
								{
									G_UseTargets2(objItem, objItem, objItem->target3);
								}
							}
							else
							{
								G_UseTargets2(objItem, objItem, objItem->target3);
							}
                            //3-24-03 - want to fire off the target too I guess, if we have one.
							if (ent->targetname && ent->targetname[0])
							{
								haltTrigger = qfalse;
							}
						}
						else
						{
							haltTrigger = qfalse;
						}

						//now that the item has been delivered, it can go away.
						if (objItem->removeFromOwnerOnUse)
						{
							SiegeItemRemoveOwner(objItem, activator);
						}
						if (objItem->despawnOnUse)
						{
							objItem->s.eFlags |= EF_NODRAW;
							objItem->genericValue11 = 0;
							objItem->s.eFlags &= ~EF_RADAROBJECT;
							G_SetOrigin(objItem, objItem->pos1);
						}
						if (objItem->removeFromGameOnUse)
						{
							objItem->nextthink = 0;
							objItem->neverFree = qfalse;
							G_FreeEntity(objItem);
						}
					}
				}
			}
		}
	}
	else if (ent->genericValue1)
	{ //Never activate in non-siege gametype I guess.
		return;
	}

	if (ent->genericValue2)
	{ //has "teambalance" property
		int i = 0;
		int team1ClNum = 0;
		int team2ClNum = 0;
		int owningTeam = ent->genericValue3;
		int newOwningTeam = 0;
		int numEnts = 0;
		int entityList[MAX_GENTITIES];
		gentity_t *cl;

		if (g_gametype.integer != GT_SIEGE)
		{
			return;
		}

		if (!activator->client ||
			(activator->client->sess.sessionTeam != SIEGETEAM_TEAM1 && activator->client->sess.sessionTeam != SIEGETEAM_TEAM2))
		{ //activator must be a valid client to begin with
			return;
		}

		//Count up the number of clients standing within the bounds of the trigger and the number of them on each team
		numEnts = trap_EntitiesInBox( ent->r.absmin, ent->r.absmax, entityList, MAX_GENTITIES );
		while (i < numEnts)
		{
			if (entityList[i] < MAX_CLIENTS)
			{ //only care about clients
				cl = &g_entities[entityList[i]];

				//the client is valid
				if (cl->inuse && cl->client &&
					(cl->client->sess.sessionTeam == SIEGETEAM_TEAM1 || cl->client->sess.sessionTeam == SIEGETEAM_TEAM2) &&
					cl->health > 0 &&
					!(cl->client->ps.eFlags & EF_DEAD))
				{
					//See which team he's on
					if (cl->client->sess.sessionTeam == SIEGETEAM_TEAM1)
					{
						team1ClNum++;
					}
					else
					{
						team2ClNum++;
					}
				}
			}
			i++;
		}

		if (!team1ClNum && !team2ClNum)
		{ //no one in the box? How did we get activated? Oh well.
			return;
		}

		if (team1ClNum == team2ClNum)
		{ //if equal numbers the ownership will remain the same as it is now
			return;
		}

		//decide who owns it now
		if (team1ClNum > team2ClNum)
		{
			newOwningTeam = SIEGETEAM_TEAM1;
		}
		else
		{
			newOwningTeam = SIEGETEAM_TEAM2;
		}

		if (owningTeam == newOwningTeam)
		{ //it's the same one it already was, don't care then.
			return;
		}

		//Set the new owner and set the variable which will tell us to activate a team-specific target
		ent->genericValue3 = newOwningTeam;
		ent->genericValue4 = newOwningTeam;
	}

	if (haltTrigger)
	{ //This is an objective trigger and the activator is not carrying an objective item that matches the targetname.
		return;
	}

	if ( ent->nextthink > level.time ) 
	{
		if( ent->spawnflags & 2048 ) // MULTIPLE - allow multiple entities to touch this trigger in a single frame
		{
			if ( ent->painDebounceTime && ent->painDebounceTime != level.time )
			{//this should still allow subsequent ents to fire this trigger in the current frame
				return;		// can't retrigger until the wait is over
			}
		}
		else
		{
			return;
		}

	}

	// if the player has already activated this trigger this frame
	if( activator && G_IsPlayer(activator) && ent->aimDebounceTime == level.time )
	{
		return;	
	}

	if ( ent->flags & FL_INACTIVE )
	{//Not active at this time
		return;
	}

	if (level.zombies && (level.siegeMap == SIEGEMAP_CARGO) && (!Q_stricmp(ent->target, "commandcenterdooractual") || !Q_stricmp(ent->target, "ccturrets")))
	{
		return;
	}

	if (activator && activator->client && level.siegeMap == SIEGEMAP_NAR && !Q_stricmp(ent->target, "station2breached")) {
		if (activator->client->ps.origin[1] < 8500 && !level.narStationBreached[0]) {
			trap_SendServerCommand(-1, "cp \"Research station 1 breached!\n\"");
			level.narStationBreached[0] = qtrue;
		}
		else if (activator->client->ps.origin[1] >= 8500 && !level.narStationBreached[1]) {
			trap_SendServerCommand(-1, "cp \"Research station 2 breached!\n\"");
			level.narStationBreached[1] = qtrue;
		}
		if (level.narStationBreached[0] && level.narStationBreached[1]) {
			ent->r.contents &= ~CONTENTS_TRIGGER;
			ent->think = 0;
			ent->use = 0;
		}
		return;
	}

	if (ent->recallSiegeItem && VALIDSTRING(ent->target)) {
		int i;
		gentity_t *item = NULL;
		for (i = 0; i < MAX_GENTITIES; i++) {
			gentity_t *maybeItem = &g_entities[i];
			if (VALIDSTRING(maybeItem->classname) && VALIDSTRING(maybeItem->targetname) &&
				!Q_stricmp(maybeItem->classname, "misc_siege_item") && !Q_stricmp(maybeItem->targetname, ent->target)) {
				item = maybeItem;
				break;
			}
		}
		// must be visible, able to be picked up, not being carried by someone
		// commented-out check for being in OG spawn point
		if (item && item->canPickUp && !(item->s.eFlags & EF_NODRAW) && !item->genericValue2 /*&& !VectorInsideBox(item->r.currentOrigin, item->pos1[0], item->pos1[1], item->pos1[2], item->pos1[0], item->pos1[1], item->pos1[2], 64.0f)*/) {
			item->siegeItemCarrierTime = 0;
			if (item->specialIconTreatment)
				item->s.eFlags |= EF_RADAROBJECT;
			gentity_t *overrideOriginEnt = NULL;
			if (VALIDSTRING(ent->recallOrigin)) // see if a custom recall origin entity targetname was specified, too
				overrideOriginEnt = G_Find(NULL, FOFS(targetname), ent->recallOrigin);
			item->genericValue9 = 0; // no longer waiting to respawn
			SiegeItemRespawnOnOriginalSpot(item, NULL, overrideOriginEnt);

			if (VALIDSTRING(ent->recallTarget)) {
				gentity_t *recallTarget = G_Find(NULL, FOFS(targetname), ent->recallTarget);
				while (recallTarget) {
					if (recallTarget->use && !(VALIDSTRING(recallTarget->classname) && !Q_stricmp(recallTarget->classname, "misc_siege_item"))) { // sanity; don't use siege items
						CheckSiegeHelpFromUse(recallTarget->targetname);
						recallTarget->use(recallTarget, activator, activator);
					}
					recallTarget = G_Find(recallTarget, FOFS(targetname), ent->recallTarget);
				}
			}
		}
		return;
	}

	ent->activator = activator;

	if(ent->delay && ent->painDebounceTime < (level.time + ent->delay) )
	{//delay before firing trigger
		ent->think = multi_trigger_run;
		ent->nextthink = level.time + ent->delay;
		ent->painDebounceTime = level.time;
		
	}
	else
	{
		multi_trigger_run (ent);
	}
}

void Use_Multi( gentity_t *ent, gentity_t *other, gentity_t *activator ) 
{
	multi_trigger( ent, activator );
}

qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );
extern void Use_BinaryMover(gentity_t *ent, gentity_t *other, gentity_t *activator);

void Touch_Multi(gentity_t *self, gentity_t *other, trace_t *trace)
{
	static qboolean holdEleDoorsHacked = qfalse;
	short		i1, i2;
	if (!other->client)
	{
		return;
	}

	if (other->client->emoted)
		return;

	if (self->flags & FL_INACTIVE)
	{//set by target_deactivate
		return;
	}

	if (self->alliedTeam)
	{
		if (other->client->sess.sessionTeam != self->alliedTeam)
		{
			return;
		}
	}

	if (self->spawnflags & 1)
	{
		if (other->s.eType == ET_NPC)
		{
			return;
		}
	}
	else
	{
		if (self->spawnflags & 16)
		{//NPCONLY
			if (other->NPC == NULL)
			{
				return;
			}
		}

		if (self->NPC_targetname && self->NPC_targetname[0])
		{
			if (other->script_targetname && other->script_targetname[0])
			{
				if (Q_stricmp(self->NPC_targetname, other->script_targetname) != 0)
				{//not the right guy to fire me off
					return;
				}
				if (level.siegeMap == SIEGEMAP_URBAN && other->m_pVehicle && (!other->m_pVehicle->m_pPilot || ((gentity_t *)other->m_pVehicle->m_pPilot)->health <= 0))
				{
					return; // must have pilot
				}
			}
			else
			{
				return;
			}
		}
	}

	if (level.zombies && level.siegeMap == SIEGEMAP_CARGO && VALIDSTRING(self->target) && (!Q_stricmp(self->target, "thecodes") || !Q_stricmp(self->target, "breachcommandcenter") || !Q_stricmp(self->target, "node1breached") || !Q_stricmp(self->target, "breachednode2") || !Q_stricmp(self->target, "ccturrets")))
	{
		return;
	}

	if (!Q_stricmp(self->target, "hangarplatbig1") && !level.hangarCompletedTime && other->client->sess.sessionTeam == TEAM_BLUE && level.siegeMap == SIEGEMAP_HOTH && g_antiHothHangarLiftLame.integer && g_antiHothHangarLiftLame.integer >= 2)
	{
		gentity_t	*entity_list[MAX_GENTITIES], *liftLameTarget;
		vec3_t		liftOrigin;
		int			liftLameTargetsCount, i;
		qboolean	iAmALiftLamer = qfalse;

		VectorCopy(other->client->ps.origin, liftOrigin);
		liftLameTargetsCount = G_RadiusList(liftOrigin, LIFT_LAME_DETECTION_DISTANCE, self, qtrue, entity_list);

		for (i = 0; i < liftLameTargetsCount; i++)
		{
			liftLameTarget = entity_list[i];

			if (!liftLameTarget->client)
			{
				continue;
			}
			if (liftLameTarget == self || !liftLameTarget->takedamage || liftLameTarget->health <= 0 || (liftLameTarget->flags & FL_NOTARGET))
			{
				continue;
			}
			if (liftLameTarget->client->sess.sessionTeam && liftLameTarget->client->sess.sessionTeam != TEAM_RED)
			{
				continue;
			}

			if (liftLameTarget->s.eType == ET_NPC &&
				(liftLameTarget->s.NPC_class == CLASS_VEHICLE || liftLameTarget->s.NPC_class == CLASS_RANCOR || liftLameTarget->s.NPC_class == CLASS_WAMPA))
			{ //don't get mad at vehicles, rancors, or wampas, silly.
				continue;
			}

			if (liftLameTarget->client && liftLameTarget->client->ps.m_iVehicleNum && ((&g_entities[liftLameTarget->client->ps.m_iVehicleNum])->m_pVehicle->m_pVehicleInfo->type == VH_WALKER || (&g_entities[liftLameTarget->client->ps.m_iVehicleNum])->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER))
			{
				continue; //don't target people who are piloting walkers or fighters
			}
			iAmALiftLamer = qtrue;
		}

		if (iAmALiftLamer == qtrue)
		{
			return;
		}
	}
	//d can use lift once, and it must be within 15 seconds of infirmary being breached
	if (!Q_stricmp(self->target, "hangarplatbig1") && level.hangarCompletedTime && (level.hangarLiftUsedByDefense || level.time >= level.hangarCompletedTime + 15000) && other->client->sess.sessionTeam == TEAM_BLUE && level.siegeMap == SIEGEMAP_HOTH && g_antiHothHangarLiftLame.integer && g_antiHothHangarLiftLame.integer >= 4)
	{
		return;
	}

	if (!Q_stricmp(self->target, "medlabplatb") && other->client->ps.origin[2] >= 365 && g_antiHothInfirmaryLiftLame.integer && level.siegeMap == SIEGEMAP_HOTH && level.hangarCompletedTime) {
		return;
	}

	if (holdEleDoorsHacked && other && other->client && other->client->sess.sessionTeam == TEAM_BLUE && !Q_stricmp(self->target, "holdelev") && level.siegeMap == SIEGEMAP_CARGO && other->client->ps.origin[2] >= 600) {
		int i;
		for (i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *player = &g_entities[i];
			if (!player->client || player->client->pers.connected != CON_CONNECTED)
				continue;
			if (player->client->sess.sessionTeam != TEAM_RED)
				continue;
			if (player->health <= 0 || player->client->tempSpectate > level.time)
				continue;
			if (!(player->client->ps.origin[0] <= 2350 &&
				player->client->ps.origin[1] >= 1600 && player->client->ps.origin[1] <= 2400 &&
				player->client->ps.origin[2] <= 350))
				continue;
			return;
		}
	}

	if (self->spawnflags & 2)
	{//FACING
		vec3_t	forward;

		AngleVectors(other->client->ps.viewangles, forward, NULL, NULL);

		if (DotProduct(self->movedir, forward) < 0.5)
		{//Not Within 45 degrees
			return;
		}
	}

	// duo: added siege item check here to prevent bogus hacking/using animations without having the item
	// (the base jka item check takes place after we've already given you an animation)
	if (g_gametype.integer == GT_SIEGE && self->genericValue1) {
		if (!other || !other->client || !other->client->holdingObjectiveItem || !VALIDSTRING(self->targetname))
			return;
		gentity_t *objItem = &g_entities[other->client->holdingObjectiveItem];
		if (!objItem || !objItem->inuse || objItem->genericValue7 == other->client->sess.sessionTeam ||
			!VALIDSTRING(objItem->goaltarget) || Q_stricmp(self->targetname, objItem->goaltarget))
			return;
	}

	if (self->spawnflags & 4)
	{//USE_BUTTON
		if (!(other->client->pers.cmd.buttons & BUTTON_USE))
		{//not pressing use button
			return;
		}

		if ((other->client->ps.weaponTime > 0 && other->client->ps.torsoAnim != BOTH_BUTTON_HOLD && other->client->ps.torsoAnim != BOTH_CONSOLE1) || other->health < 1 ||
			(other->client->ps.pm_flags & PMF_FOLLOW) || other->client->sess.sessionTeam == TEAM_SPECTATOR ||
			other->client->ps.forceHandExtend != HANDEXTEND_NONE)
		{ //player has to be free of other things to use.
			return;
		}

		if (level.siegeMap == SIEGEMAP_CARGO && VALIDSTRING(self->targetname) && !Q_stricmp(self->targetname, "halldoorhack")) {
			gentity_t *door = G_Find(NULL, FOFS(targetname), "halldoor");
			if (door && VALIDSTRING(door->classname) && !Q_stricmp(door->classname, "func_door") && door->moverState != MOVER_1TO2) {
				qboolean locked = (door->spawnflags & 16) ? qtrue : qfalse;
				if (locked)
					door->spawnflags &= ~16; // MOVER_LOCKED
				Use_BinaryMover(door, other, other);
				if (locked)
					door->spawnflags |= 16; // MOVER_LOCKED
			}
		}

		if (level.siegeMap == SIEGEMAP_HOTH && !Q_stricmp(self->target, "sidehack1_relay")) {
			self->genericValue7 = g_fixHoth2ndObj.integer ? 0 : self->genericValue7;
			self->idealclass[0] = g_fixHoth2ndObj.integer ? '\0' : 'I';
		}

		if (self->genericValue7)
		{ 
			if (level.siegeMap == SIEGEMAP_HOTH && !Q_stricmp(self->target, "t712")) {
				self->genericValue7 = g_hothHangarHack.integer & HOTHHANGARHACK_5SECONDS ? 5000 : self->genericValue7;
				if (!Q_stricmpn(level.mapname, "siege_hoth3", 11))
					self->idealClassType = g_hothHangarHack.integer & HOTHHANGARHACK_ANYCLASS ? 0 : CLASSTYPE_TECH;
				else if (self->idealclass)
					self->idealclass[0] = g_hothHangarHack.integer & HOTHHANGARHACK_ANYCLASS ? '\0' : 'I';
			}
			//we have to be holding the use key in this trigger for x milliseconds before firing
			if (g_gametype.integer == GT_SIEGE &&
				self->idealclass && self->idealclass[0])
			{ //only certain classes can activate it
				if (!other ||
					!other->client ||
					other->client->siegeClass < 0)
				{ //no class
					return;
				}

				if (other->client->sess.sessionTeam == TEAM_RED && g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none"))
				{
					//get generic type of class we are currently(tech, assault, whatever) and return if the idealclass does not match
					i1 = bgSiegeClasses[other->client->siegeClass].playerClass;
					i2 = bgSiegeClasses[BG_SiegeFindClassIndexByName(self->idealclass)].playerClass;
					if (i1 != i2)
						if (bgSiegeClasses[other->client->siegeClass].playerClass != BG_SiegeFindClassIndexByName(self->idealclass))
							return;
				}
				else if (other->client->sess.sessionTeam == TEAM_BLUE && g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none"))
				{
					//get generic type of class we are currently(tech, assault, whatever) and return if the idealclass does not match
					i1 = bgSiegeClasses[other->client->siegeClass].playerClass;
					i2 = bgSiegeClasses[BG_SiegeFindClassIndexByName(self->idealclass)].playerClass;
					if (i1 != i2)
						if (bgSiegeClasses[other->client->siegeClass].playerClass != BG_SiegeFindClassIndexByName(self->idealclass))
							return;
				}
				else if (!G_NameInTriggerClassList(bgSiegeClasses[other->client->siegeClass].name, self->idealclass))
				{ //wasn't in the list
					return;
				}
			}
			if (g_gametype.integer == GT_SIEGE && self->idealClassType && self->idealClassType >= CLASSTYPE_ASSAULT && other->client->sess.sessionTeam == TEAM_RED)
			{
				i1 = bgSiegeClasses[other->client->siegeClass].playerClass;
				if (i1 + 10 != self->idealClassType)
				{
					return;
				}
			}
			if (g_gametype.integer == GT_SIEGE && self->idealClassTypeTeam2 && self->idealClassTypeTeam2 >= CLASSTYPE_ASSAULT && other->client->sess.sessionTeam == TEAM_BLUE)
			{
				i1 = bgSiegeClasses[other->client->siegeClass].playerClass;
				if (i1 + 10 != self->idealClassTypeTeam2)
				{
					return;
				}
			}
			if (g_gametype.integer == GT_SIEGE && level.siegeMap == SIEGEMAP_URBAN && VALIDSTRING(self->target) && !Q_stricmp(self->target, "obj2backdoorlockbox") &&
				other && other->client && other->client->siegeClass != -1 && bgSiegeClasses[other->client->siegeClass].playerClass == SPC_JEDI) {
				return;
			}
			if (!G_PointInBounds(other->client->ps.origin, self->r.absmin, self->r.absmax))
			{
				return;
			}
			else if (other->client->isHacking != self->s.number && other->s.number < MAX_CLIENTS)
			{ //start the hack
				other->client->isHacking = self->s.number;
				VectorCopy(other->client->ps.viewangles, other->client->hackingAngles);
				other->client->ps.hackingTime = level.time + self->genericValue7;
				other->client->ps.hackingBaseTime = self->genericValue7;
				if (other->client->ps.hackingBaseTime > 60000)
				{ //don't allow a bit overflow
					other->client->ps.hackingTime = level.time + 60000;
					other->client->ps.hackingBaseTime = 60000;
				}
				return;
			}
			else if (other->client->ps.hackingTime < level.time)
			{ //finished with the hack, reset the hacking values and let it fall through
				other->client->isHacking = 0; //can't hack a client
				other->client->ps.hackingTime = 0;
				// cargo2 non-objective hacks
				if (level.siegeMap == SIEGEMAP_CARGO && VALIDSTRING(self->target)) {
					int numHackedOfNode1AndTop = 0;
					if (!Q_stricmp(self->target, "tophacked") ||
						!Q_stricmp(self->target, "holdeledoors") ||
						!Q_stricmp(self->target, "tunneldoors") ||
						!Q_stricmp(self->target, "topdoor") ||
						!Q_stricmp(self->target, "ccturrets") ||
						!Q_stricmp(self->target, "node1hacked") ||
						!Q_stricmp(self->target, "obj2ramp")) {
						other->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_CARGO2_HACKS]++;
						if (!Q_stricmp(self->target, "tophacked") || !Q_stricmp(self->target, "node1hacked"))
							numHackedOfNode1AndTop++;
						if (!Q_stricmp(self->target, "holdeledoors"))
							holdEleDoorsHacked = qtrue;
					}
					else if (!Q_stricmp(self->target, "node2hacked")) {
						other->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_CARGO2_HACKS] += (2 - numHackedOfNode1AndTop); // can be 2
					}
				}
			}
			else
			{ //hack in progress
				return;
			}
		}
		if (!Q_stricmp(self->target, "hangarplatbig1") && level.siegeMap == SIEGEMAP_HOTH && g_antiHothHangarLiftLame.integer &&
			bgSiegeClasses[other->client->siegeClass].playerClass == 2 && other->client->sess.sessionTeam == TEAM_BLUE && !level.hangarCompletedTime && (g_antiHothHangarLiftLame.integer == 1 || g_antiHothHangarLiftLame.integer == 3))
		{
			if (hangarHackTime && (level.time - hangarHackTime < 2000))
			{
				return;
			}
			if (!G_PointInBounds(other->client->ps.origin, self->r.absmin, self->r.absmax))
			{
				return;
			}
			else if (other->client->isHacking != self->s.number && other->s.number < MAX_CLIENTS)
			{ //start the hack
				other->client->isHacking = self->s.number;
				VectorCopy(other->client->ps.viewangles, other->client->hackingAngles);
				other->client->ps.hackingTime = level.time + 2000;
				other->client->ps.hackingBaseTime = 2000;
				if (other->client->ps.hackingBaseTime > 60000)
				{ //don't allow a bit overflow
					other->client->ps.hackingTime = level.time + 60000;
					other->client->ps.hackingBaseTime = 60000;
				}
				return;
			}
			else if (other->client->ps.hackingTime < level.time)
			{ //finished with the hack, reset the hacking values and let it fall through
				other->client->isHacking = 0; //can't hack a client
				other->client->ps.hackingTime = 0;
				hangarHackTime = level.time;
			}
			else
			{ //hack in progress
				return;
			}
		}
	}

	if (self->spawnflags & 8)
	{//FIRE_BUTTON
		if (!(other->client->pers.cmd.buttons & BUTTON_ATTACK) &&
			!(other->client->pers.cmd.buttons & BUTTON_ALT_ATTACK))
		{//not pressing fire button or altfire button
			return;
		}
	}

	if (self->radius)
	{
		vec3_t	eyeSpot;

		//Only works if your head is in it, but we allow leaning out
		//NOTE: We don't use CalcEntitySpot SPOT_HEAD because we don't want this
		//to be reliant on the physical model the player uses.
		VectorCopy(other->client->ps.origin, eyeSpot);
		eyeSpot[2] += other->client->ps.viewheight;

		if (G_PointInBounds(eyeSpot, self->r.absmin, self->r.absmax))
		{
			if (!(other->client->pers.cmd.buttons & BUTTON_ATTACK) &&
				!(other->client->pers.cmd.buttons & BUTTON_ALT_ATTACK))
			{//not attacking, so hiding bonus
			}
		}
	}

	if (self->spawnflags & 4)
	{//USE_BUTTON
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
	}

	if (self->think == trigger_cleared_fire)
	{//We're waiting to fire our target2 first
		self->nextthink = level.time + self->speed;
		return;
	}

	if (!Q_stricmp(self->target, "hangarplatbig1") && level.hangarCompletedTime && !level.hangarLiftUsedByDefense && other->client->sess.sessionTeam == TEAM_BLUE && level.siegeMap == SIEGEMAP_HOTH)
	{
		level.hangarLiftUsedByDefense = qtrue;
	}

	multi_trigger(self, other);
}

void trigger_cleared_fire (gentity_t *self)
{
	G_UseTargets2( self, self->activator, self->target2 );
	self->think = 0;
	// should start the wait timer now, because the trigger's just been cleared, so we must "wait" from this point
	if ( self->wait > 0 ) 
	{
		self->nextthink = level.time + ( self->wait + self->random * crandom() ) * 1000;
	}
}

void RunControlPoint(gentity_t *self) {
	if (g_gametype.integer != GT_SIEGE) {
		G_FreeEntity(self);
		return;
	}

	int *time = &self->genericValue1;
	int *team = &self->genericValue2;
	int *rateModifier = &self->genericValue3; // percentage
	int *blockable = &self->genericValue4;
	int *maxPlayers = &self->genericValue5;
	int *reverseWithNoEnemies = &self->genericValue6;
	int *delayBeforeReverse = &self->genericValue7;
	int *reversible = &self->genericValue8;
	int *target2wait = &self->genericValue9;
	int *target3wait = &self->genericValue10;
	int *target4wait = &self->genericValue11;
	int *lastTarget2 = &self->genericValue12;
	int *lastTarget3 = &self->genericValue13;
	int *lastTarget4 = &self->genericValue14;
	int *progress = &self->genericValue15;
	int *lastThink = &self->genericValue16;
	int *lastProgress = &self->genericValue17;
	int timeSinceLastThink = level.time - *lastThink;
	*lastThink = level.time;
	int timeSinceLastProgress = level.time - *lastProgress;

	assert(*time > 0 && (*team == TEAM_RED || *team == TEAM_BLUE));

	if (self->think == multi_trigger_run) // this shouldn't happen, but we're already triggered and just waiting to fire
		return;

	if (*progress >= *time) { // time to fire
		if (self->delay && self->painDebounceTime < (level.time + self->delay)) { // delay before firing trigger
			self->think = multi_trigger_run;
			self->nextthink = level.time + self->delay;
			self->painDebounceTime = level.time;
			return;
		}
		else { // fire immediately
			multi_trigger_run(self);
			return;
		}
	}

	self->think = RunControlPoint;
	self->nextthink = level.time + 50;

	if (!gSiegeRoundBegun || level.zombies || self->flags & FL_INACTIVE)
		return;

	// count how many people are inside it
	int entityList[MAX_GENTITIES];
	int numEnts = trap_EntitiesInBox(self->r.absmin, self->r.absmax, entityList, MAX_GENTITIES);
	int i, numGoodTeam = 0, numBadTeam = 0;
	for (i = 0; i < numEnts; i++) {
		if (entityList[i] < 0 || entityList[i] >= MAX_CLIENTS)
			continue;
		gentity_t *toucher = &g_entities[entityList[i]];
		if (!toucher->inuse || !toucher->client || toucher->health <= 0 || toucher->client->ps.eFlags & EF_DEAD || toucher->client->tempSpectate >= level.time)
			continue;
		if (toucher->client->sess.sessionTeam == *team)
			numGoodTeam++;
		else if (toucher->client->sess.sessionTeam == OtherTeam(*team))
			numBadTeam++;
	}

#ifdef DEBUG_CONTROLPOINT
	int originalProgress = *progress;
#endif

	if (numGoodTeam > 0) {
		if (blockable && numBadTeam > 0) { // progress blocked by bad guys
			if (VALIDSTRING(self->target4)) {
				if (!*target4wait || !*lastTarget4 || level.time - *lastTarget4 >= *target4wait) {
					G_UseTargets2(self, NULL, self->target4);
					*lastTarget4 = level.time;
				}
			}
			//return;
		}
		else { // increase progress
			if (VALIDSTRING(self->target2)) {
				if (!*target2wait || !*lastTarget2 || level.time - *lastTarget2 >= *target2wait) {
					G_UseTargets2(self, NULL, self->target2);
					*lastTarget2 = level.time;
				}
			}
			*lastProgress = level.time;
			*progress += timeSinceLastThink; // add the first guy's progress
			int numExtraGuys = Com_Clampi(0, *maxPlayers > 0 ? *maxPlayers : MAX_CLIENTS, numGoodTeam - 1);
			for (i = 0; i < numExtraGuys; i++) // add additional progress for extra people
				*progress += (((float)*rateModifier) / 100.0f) * (float)timeSinceLastThink;
			if (*progress >= *time) { // check if we need to fire now, rather than waiting until the next think
				if (self->delay && self->painDebounceTime < (level.time + self->delay)) { // delay before firing trigger
					self->think = multi_trigger_run;
					self->nextthink = level.time + self->delay;
					self->painDebounceTime = level.time;
					return;
				}
				else { // fire immediately
					multi_trigger_run(self);
					return;
				}
			}
		}
	}
	else if (reversible && (numBadTeam > 0 || reverseWithNoEnemies) && (!*delayBeforeReverse || timeSinceLastProgress >= *delayBeforeReverse)) { // decrease progress
		if (VALIDSTRING(self->target3)) {
			if (!*target3wait || !*lastTarget3 || level.time - *lastTarget3 >= *target3wait) {
				G_UseTargets2(self, NULL, self->target3);
				*lastTarget3 = level.time;
			}
		}
		*progress -= timeSinceLastThink; // subtract the first guy's progress (or just progress for nobody being on it, if applicable)
		int numExtraGuys = Com_Clampi(0, *maxPlayers > 0 ? *maxPlayers : MAX_CLIENTS, numBadTeam - 1);
		for (i = 0; i < numExtraGuys; i++) // subtract additional progress for extra people
			*progress -= (((float)*rateModifier) / 100.0f) * (float)timeSinceLastThink;
		if (*progress < 0)
			*progress = 0;
	}

#ifdef DEBUG_CONTROLPOINT
	if (numGoodTeam > 0 || numBadTeam > 0) {
		Com_Printf("%d good guys, %d bad guys; ", numGoodTeam, numBadTeam);
		int difference = *progress - originalProgress;
		if (difference > 0)
			Com_Printf("added %d progress; ", difference);
		else if (difference < 0)
			Com_Printf("removed %d progress; ", abs(difference));
		else
			Com_Printf("no difference; ");
		Com_Printf("progress is now %d\n", *progress);
	}
#endif
}

void InitControlPoint(gentity_t *ent) {
	G_SpawnInt("time", "1000", &ent->genericValue1);
	G_SpawnInt("team", "1", &ent->genericValue2);
	G_SpawnInt("ratemodifier", "0", &ent->genericValue3);
	G_SpawnInt("blockable", "0", &ent->genericValue4);
	G_SpawnInt("maxplayers", "1", &ent->genericValue5);
	G_SpawnInt("reversewithnoenemies", "0", &ent->genericValue6);
	G_SpawnInt("delaybeforereverse", "0", &ent->genericValue7);
	G_SpawnInt("reversible", "1", &ent->genericValue8);
	G_SpawnInt("target2wait", "0", &ent->genericValue9);
	G_SpawnInt("target3wait", "0", &ent->genericValue10);
	G_SpawnInt("target4wait", "0", &ent->genericValue11);

	G_SpawnInt("delay", "0", &ent->delay);

	if ((ent->wait > 0) && (ent->random >= ent->wait)) {
		ent->random = ent->wait - FRAMETIME;
		Com_Printf(S_COLOR_YELLOW"trigger_multiple has random >= wait\n");
	}

	ent->delay *= 1000;//1 = 1 msec, 1000 = 1 sec
	if (!ent->speed && ent->target2 && ent->target2[0])
	{
		ent->speed = 1000;
	}
	else
	{
		ent->speed *= 1000;
	}

	ent->touch = NULL;
	ent->use = NULL;
	ent->think = RunControlPoint;
	ent->nextthink = level.time + 50;

	InitTrigger(ent);
	trap_LinkEntity(ent);
}


/*QUAKED trigger_multiple (.1 .5 .1) ? CLIENTONLY FACING USE_BUTTON FIRE_BUTTON NPCONLY x x INACTIVE MULTIPLE
CLIENTONLY - only a player can trigger this by touch
FACING - Won't fire unless triggering ent's view angles are within 45 degrees of trigger's angles (in addition to any other conditions)
USE_BUTTON - Won't fire unless player is in it and pressing use button (in addition to any other conditions)
FIRE_BUTTON - Won't fire unless player/NPC is in it and pressing fire button (in addition to any other conditions)
NPCONLY - only non-player NPCs can trigger this by touch
INACTIVE - Start off, has to be activated to be touchable/usable
MULTIPLE - multiple entities can touch this trigger in a single frame *and* if needed, the trigger can have a wait of > 0

"wait"		Seconds between triggerings, 0 default, number < 0 means one time only.
"random"	wait variance, default is 0
"delay"		how many seconds to wait to fire targets after tripped
"hiderange" As long as NPC's head is in this trigger, NPCs out of this hiderange cannot see him.  If you set an angle on the trigger, they're only hidden from enemies looking in that direction.  the player's crouch viewheight is 36, his standing viewheight is 54.  So a trigger thast should hide you when crouched but not standing should be 48 tall.
"target2"	The trigger will fire this only when the trigger has been activated and subsequently 'cleared'( once any of the conditions on the trigger have not been satisfied).  This will not fire the "target" more than once until the "target2" is fired (trigger field is 'cleared')
"speed"		How many seconds to wait to fire the target2, default is 1
"noise"		Sound to play when the trigger fires (plays at activator's origin)
"NPC_targetname"  Only the NPC with this NPC_targetname fires this trigger

Variable sized repeatable trigger.  Must be targeted at one or more entities.
so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"team" - If set, only this team can trip this trigger
	0 - any
	1 - red
	2 - blue

"soundSet"	Ambient sound set to play when this trigger is activated

usetime		-	If specified (in milliseconds) along with the USE_BUTTON flag, will
				require a client to hold the use key for x amount of ms before firing.

Applicable only during Siege gametype:
teamuser	-	if 1, team 2 can't use this. If 2, team 1 can't use this.
siegetrig	-	if non-0, can only be activated by players carrying a misc_siege_item
				which is associated with this trigger by the item's goaltarget value.
teambalance	-	if non-0, is "owned" by the last team that activated. Can only be activated
				by the other team if the number of players on the other team inside	the
				trigger outnumber the number of players on the owning team inside the
				trigger.
target3		-	fire when activated by team1
target4		-	fire when activated by team2

idealclass	-	Can only be used by this class/these classes. You can specify use by
				multiple classes with the use of |, e.g.:
				"Imperial Medic|Imperial Assassin|Imperial Demolitionist"
*/

void SP_trigger_multiple( gentity_t *ent ) 
{
	int isControlPoint;
	G_SpawnInt("controlpoint", "0", &isControlPoint);
	if (isControlPoint) {
		InitControlPoint(ent);
		return;
	}

	// duo: new "setteamallow" key -- this trigger will set the teamallow (known as alliedTeam in the gentity_t struct)
	// which means we can target a door to enable a certain team to use it but not actually unlock it
	G_SpawnInt("setteamallow", "0", &ent->genericValue17);
	if (ent->genericValue17 < TEAM_FREE || ent->genericValue17 > TEAM_BLUE)
		ent->genericValue17 = TEAM_FREE;

	// duo: new "justopen" key -- when targeting a LOCKED door, this trigger will simply open it once instead of unlocking it
	G_SpawnInt("justopen", "0", &ent->genericValue16);

	char	*s;
	if ( G_SpawnString( "noise", "", &s ) ) 
	{
		if (s && s[0])
		{
			ent->noise_index = G_SoundIndex(s);
		}
		else
		{
			ent->noise_index = 0;
		}
	}

	if (G_SpawnString("idealclasstype", "", &s))
	{
		if (s && s[0])
		{
			if (!Q_stricmp(s, "a"))
			{
				ent->idealClassType = CLASSTYPE_ASSAULT;
			}
			else if (!Q_stricmp(s, "h"))
			{
				ent->idealClassType = CLASSTYPE_HW;
			}
			else if (!Q_stricmp(s, "d"))
			{
				ent->idealClassType = CLASSTYPE_DEMO;
			}
			else if (!Q_stricmp(s, "s"))
			{
				ent->idealClassType = CLASSTYPE_SCOUT;
			}
			else if (!Q_stricmp(s, "t"))
			{
				ent->idealClassType = CLASSTYPE_TECH;
			}
			else if (!Q_stricmp(s, "j"))
			{
				ent->idealClassType = CLASSTYPE_JEDI;
			}
		}
	}

	if (G_SpawnString("idealclasstypeteam2", "", &s))
	{
		if (s && s[0])
		{
			if (!Q_stricmp(s, "a"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_ASSAULT;
			}
			else if (!Q_stricmp(s, "h"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_HW;
			}
			else if (!Q_stricmp(s, "d"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_DEMO;
			}
			else if (!Q_stricmp(s, "s"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_SCOUT;
			}
			else if (!Q_stricmp(s, "t"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_TECH;
			}
			else if (!Q_stricmp(s, "j"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_JEDI;
			}
		}
	}

	G_SpawnInt("usetime", "0", &ent->genericValue7);

	//For siege gametype
	G_SpawnInt("siegetrig", "0", &ent->genericValue1);
    G_SpawnInt("teambalance", "0", &ent->genericValue2);

	G_SpawnInt("delay", "0", &ent->delay);

	if ( (ent->wait > 0) && (ent->random >= ent->wait) ) {
		ent->random = ent->wait - FRAMETIME;
		Com_Printf(S_COLOR_YELLOW"trigger_multiple has random >= wait\n");
	}

	ent->delay *= 1000;//1 = 1 msec, 1000 = 1 sec
	if ( !ent->speed && ent->target2 && ent->target2[0] )
	{
		ent->speed = 1000;
	}
	else
	{
		ent->speed *= 1000;
	}

	if (level.siegeMap == SIEGEMAP_DESERT && !Q_stricmp(ent->targetname, "partdeliverzone"))
	{
		ent->wait = 0;
		//bugfix for final objective getting broken when two parts are delivered within 1 second of each other
	}

	static char recalltargets[MAX_GENTITIES][32] = { 0 };
	static char recallorigins[MAX_GENTITIES][32] = { 0 };
	G_SpawnInt("recallsiegeitem", "0", &ent->recallSiegeItem);
	if (ent->recallSiegeItem) {
		s = NULL;
		if (G_SpawnString("recalltarget", "", &s) && VALIDSTRING(s)) {
			if (strlen(s) >= sizeof(recalltargets[0]))
				G_LogPrintf("misc_siege_item: recalltarget '%s' is too long! (should be %d digits or less)\n", s, sizeof(recalltargets[0]) - 1);
			Q_strncpyz(recalltargets[ent - g_entities], s, sizeof(recalltargets[0]));
			ent->recallTarget = recalltargets[ent - g_entities];
		}
		if (G_SpawnString("recallorigin", "", &s) && VALIDSTRING(s)) {
			if (strlen(s) >= sizeof(recallorigins[0]))
				G_LogPrintf("misc_siege_item: recalltarget '%s' is too long! (should be %d digits or less)\n", s, sizeof(recallorigins[0]) - 1);
			Q_strncpyz(recallorigins[ent - g_entities], s, sizeof(recallorigins[0]));
			ent->recallOrigin = recallorigins[ent - g_entities];
		}
	}
	else {
		ent->recallTarget = ent->recallOrigin = "";
	}

	ent->touch = Touch_Multi;
	ent->use   = Use_Multi;

	if ( ent->team && ent->team[0] )
	{
		ent->alliedTeam = atoi(ent->team);
		ent->team = NULL;
	}

	InitTrigger( ent );
	trap_LinkEntity (ent);
}

/*QUAKED trigger_once (.5 1 .5) ? CLIENTONLY FACING USE_BUTTON FIRE_BUTTON x x x INACTIVE MULTIPLE
CLIENTONLY - only a player can trigger this by touch
FACING - Won't fire unless triggering ent's view angles are within 45 degrees of trigger's angles (in addition to any other conditions)
USE_BUTTON - Won't fire unless player is in it and pressing use button (in addition to any other conditions)
FIRE_BUTTON - Won't fire unless player/NPC is in it and pressing fire button (in addition to any other conditions)
INACTIVE - Start off, has to be activated to be touchable/usable
MULTIPLE - multiple entities can touch this trigger in a single frame *and* if needed, the trigger can have a wait of > 0

"random"	wait variance, default is 0
"delay"		how many seconds to wait to fire targets after tripped
Variable sized repeatable trigger.  Must be targeted at one or more entities.
so, the basic time between firing is a random time between
(wait - random) and (wait + random)
"noise"		Sound to play when the trigger fires (plays at activator's origin)
"NPC_targetname"  Only the NPC with this NPC_targetname fires this trigger

"team" - If set, only this team can trip this trigger
	0 - any
	1 - red
	2 - blue

"soundSet"	Ambient sound set to play when this trigger is activated

usetime		-	If specified (in milliseconds) along with the USE_BUTTON flag, will
				require a client to hold the use key for x amount of ms before firing.

Applicable only during Siege gametype:
teamuser - if 1, team 2 can't use this. If 2, team 1 can't use this.
siegetrig - if non-0, can only be activated by players carrying a misc_siege_item
			which is associated with this trigger by the item's goaltarget value.

idealclass	-	Can only be used by this class/these classes. You can specify use by
				multiple classes with the use of |, e.g.:
				"Imperial Medic|Imperial Assassin|Imperial Demolitionist"
*/
void SP_trigger_once( gentity_t *ent ) 
{
	int isControlPoint;
	G_SpawnInt("controlpoint", "0", &isControlPoint);
	if (isControlPoint) {
		InitControlPoint(ent);
		return;
	}

	// duo: new "setteamallow" key -- this trigger will set the teamallow (known as alliedTeam in the gentity_t struct)
	// which means we can target a door to enable a certain team to use it but not actually unlock it
	G_SpawnInt("setteamallow", "0", &ent->genericValue17);
	if (ent->genericValue17 < TEAM_FREE || ent->genericValue17 > TEAM_BLUE)
		ent->genericValue17 = TEAM_FREE;

	// duo: new "justopen" key -- when targeting a LOCKED door, this trigger will simply open it once instead of unlocking it
	G_SpawnInt("justopen", "0", &ent->genericValue16);

	char	*s;
	if ( G_SpawnString( "noise", "", &s ) ) 
	{
		if (s && s[0])
		{
			ent->noise_index = G_SoundIndex(s);
		}
		else
		{
			ent->noise_index = 0;
		}
	}

	if (G_SpawnString("idealclasstype", "", &s))
	{
		if (s && s[0])
		{
			if (!Q_stricmp(s, "a"))
			{
				ent->idealClassType = CLASSTYPE_ASSAULT;
			}
			else if (!Q_stricmp(s, "h"))
			{
				ent->idealClassType = CLASSTYPE_HW;
			}
			else if (!Q_stricmp(s, "d"))
			{
				ent->idealClassType = CLASSTYPE_DEMO;
			}
			else if (!Q_stricmp(s, "s"))
			{
				ent->idealClassType = CLASSTYPE_SCOUT;
			}
			else if (!Q_stricmp(s, "t"))
			{
				ent->idealClassType = CLASSTYPE_TECH;
			}
			else if (!Q_stricmp(s, "j"))
			{
				ent->idealClassType = CLASSTYPE_JEDI;
			}
		}
	}

	if (G_SpawnString("idealclasstypeteam2", "", &s))
	{
		if (s && s[0])
		{
			if (!Q_stricmp(s, "a"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_ASSAULT;
			}
			else if (!Q_stricmp(s, "h"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_HW;
			}
			else if (!Q_stricmp(s, "d"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_DEMO;
			}
			else if (!Q_stricmp(s, "s"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_SCOUT;
			}
			else if (!Q_stricmp(s, "t"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_TECH;
			}
			else if (!Q_stricmp(s, "j"))
			{
				ent->idealClassTypeTeam2 = CLASSTYPE_JEDI;
			}
		}
	}

	G_SpawnInt("usetime", "0", &ent->genericValue7);
	if (level.siegeMap == SIEGEMAP_CARGO && ent->genericValue7 > 5000 && !Q_stricmp(ent->target, "holdeledoors"))
		ent->genericValue7 = 5000;

	//For siege gametype
	G_SpawnInt("siegetrig", "0", &ent->genericValue1);

	G_SpawnInt("delay", "0", &ent->delay);

	ent->wait = -1;

	ent->touch = Touch_Multi;
	ent->use   = Use_Multi;

	if ( ent->team && ent->team[0] )
	{
		ent->alliedTeam = atoi(ent->team);
		ent->team = NULL;
	}

	ent->delay *= 1000;//1 = 1 msec, 1000 = 1 sec

	InitTrigger( ent );
	trap_LinkEntity (ent);
}

/*
======================================================================
trigger_lightningstrike -rww
======================================================================
*/
//lightning strike trigger lightning strike event
void Do_Strike(gentity_t *ent)
{
	trace_t localTrace;
	vec3_t strikeFrom;
	vec3_t strikePoint;
	vec3_t fxAng;
	
	//maybe allow custom fx direction at some point?
	VectorSet(fxAng, 90.0f, 0.0f, 0.0f);

	//choose a random point to strike within the bounds of the trigger
	strikePoint[0] = flrand(ent->r.absmin[0], ent->r.absmax[0]);
	strikePoint[1] = flrand(ent->r.absmin[1], ent->r.absmax[1]);

	//consider the bottom mins the ground level
	strikePoint[2] = ent->r.absmin[2];

	//set the from point
	strikeFrom[0] = strikePoint[0];
	strikeFrom[1] = strikePoint[1];
	strikeFrom[2] = ent->r.absmax[2]-4.0f;

	//now trace for damaging stuff, and do the effect
	trap_Trace(&localTrace, strikeFrom, NULL, NULL, strikePoint, ent->s.number, MASK_PLAYERSOLID);
	VectorCopy(localTrace.endpos, strikePoint);

	if (localTrace.startsolid || localTrace.allsolid)
	{ //got a bad spot, think again next frame to try another strike
		ent->nextthink = level.time;
		return;
	}

	if (ent->radius)
	{ //do a radius damage at the end pos
		G_RadiusDamage(strikePoint, ent, ent->damage, ent->radius, ent, NULL, MOD_SUICIDE);
	}
	else
	{ //only damage individuals
		gentity_t *trHit = &g_entities[localTrace.entityNum];

		if (trHit->inuse && trHit->takedamage)
		{ //damage it then
			G_Damage(trHit, ent, ent, NULL, trHit->r.currentOrigin, ent->damage, 0, MOD_SUICIDE);
		}
	}

	G_PlayEffectID(ent->genericValue2, strikeFrom, fxAng);
}

//lightning strike trigger think loop
void Think_Strike(gentity_t *ent)
{
	if (ent->genericValue1)
	{ //turned off currently
		return;
	}

	ent->nextthink = level.time + ent->wait + Q_irand(0, ent->random);
	Do_Strike(ent);
}

//lightning strike trigger use event function
void Use_Strike( gentity_t *ent, gentity_t *other, gentity_t *activator ) 
{
	ent->genericValue1 = !ent->genericValue1;

	if (!ent->genericValue1)
	{ //turn it back on
		ent->nextthink = level.time;
	}
}

/*QUAKED trigger_lightningstrike (.1 .5 .1) ? START_OFF
START_OFF - start trigger disabled

"lightningfx"	effect to use for lightning, MUST be specified
"wait"			Seconds between strikes, 1000 default
"random"		wait variance, default is 2000
"dmg"			damage on strike (default 50)
"radius"		if non-0, does a radius damage at the lightning strike
				impact point (using this value as the radius). otherwise
				will only do line trace damage. default 0.

use to toggle on and off
*/
void SP_trigger_lightningstrike( gentity_t *ent ) 
{
	char *s;

	ent->use = Use_Strike;
	ent->think = Think_Strike;
	ent->nextthink = level.time + 500;

	G_SpawnString("lightningfx", "", &s);
	if (!s || !s[0])
	{
		Com_Error(ERR_DROP, "trigger_lightningstrike with no lightningfx");
	}

	//get a configstring index for it
	ent->genericValue2 = G_EffectIndex(s);

	if (ent->spawnflags & 1)
	{ //START_OFF
		ent->genericValue1 = 1;
	}

	if (!ent->wait)
	{ //default 1000
		ent->wait = 1000;
	}
	if (!ent->random)
	{ //default 2000
		ent->random = 2000;
	}
	if (!ent->damage)
	{ //default 50
		ent->damage = 50;
	}

	InitTrigger( ent );
	trap_LinkEntity (ent);
}


/*
==============================================================================

trigger_always

==============================================================================
*/

void trigger_always_think( gentity_t *ent ) {
	G_UseTargets(ent, ent);
	G_FreeEntity( ent );
}

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always (gentity_t *ent) {
	// we must have some delay to make sure our use targets are present
	ent->nextthink = level.time + 300;
	ent->think = trigger_always_think;
}


/*
==============================================================================

trigger_push

==============================================================================
*/
//trigger_push
#define PUSH_LINEAR		4
#define PUSH_RELATIVE	16
#define PUSH_MULTIPLE	2048
//target_push
#define PUSH_CONSTANT	2

void trigger_push_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	if ( !(self->spawnflags&PUSH_LINEAR) )
	{//normal throw
		if ( !other->client ) {
			return;
		}
		BG_TouchJumpPad( &other->client->ps, &self->s );
		return;
	}

	//linear
	if( level.time < self->painDebounceTime + self->wait  ) // normal 'wait' check
	{
		if( self->spawnflags & PUSH_MULTIPLE ) // MULTIPLE - allow multiple entities to touch this trigger in one frame
		{
			if ( self->painDebounceTime && level.time > self->painDebounceTime ) // if we haven't reached the next frame continue to let ents touch the trigger
			{
				return;
			}
		}
		else // only allowing one ent per frame to touch trigger
		{
				return;
			}
		}

	if ( !other->client ) {
		if ( other->s.pos.trType != TR_STATIONARY && other->s.pos.trType != TR_LINEAR_STOP && other->s.pos.trType != TR_NONLINEAR_STOP && VectorLengthSquared( other->s.pos.trDelta ) )
		{//already moving
			VectorCopy( other->r.currentOrigin, other->s.pos.trBase );
			VectorCopy( self->s.origin2, other->s.pos.trDelta );
			other->s.pos.trTime = level.time;
		}
		return;
	}

	if ( other->client->ps.pm_type != PM_NORMAL 
		&& other->client->ps.pm_type != PM_DEAD 
		&& other->client->ps.pm_type != PM_FREEZE ) 
	{
		return;
	}
	
	if ( (self->spawnflags&PUSH_RELATIVE) )
	{//relative, dir to it * speed
		vec3_t dir;
		VectorSubtract( self->s.origin2, other->r.currentOrigin, dir );
		if ( self->speed )
		{
			VectorNormalize( dir );
			VectorScale( dir, self->speed, dir );
		}
		VectorCopy( dir, other->client->ps.velocity );
	}
	else if ( (self->spawnflags&PUSH_LINEAR) )
	{//linear dir * speed
		VectorScale( self->s.origin2, self->speed, other->client->ps.velocity );
	}
	else
	{
		VectorCopy( self->s.origin2, other->client->ps.velocity );
	}

	if ( self->wait == -1 )
	{
		self->touch = NULL;
	}
	else if ( self->wait > 0 )
	{
		self->painDebounceTime = level.time;
		
	}
}


/*
=================
AimAtTarget

Calculate origin2 so the target apogee will be hit
=================
*/
void AimAtTarget( gentity_t *self ) {
	gentity_t	*ent;
	vec3_t		origin;
	float		height, gravity, time, forward;
	float		dist;

	VectorAdd( self->r.absmin, self->r.absmax, origin );
	VectorScale ( origin, 0.5f, origin );

	ent = G_PickTarget( self->target );
	if ( !ent ) {
		G_FreeEntity( self );
		return;
	}

	if ( self->classname && !Q_stricmp( "trigger_push", self->classname ) )
	{
		if ( (self->spawnflags&PUSH_RELATIVE) )
		{//relative, not an arc or linear
			VectorCopy( ent->r.currentOrigin, self->s.origin2 );
			return;
		}
		else if ( (self->spawnflags&PUSH_LINEAR) )
		{//linear, not an arc
			VectorSubtract( ent->r.currentOrigin, origin, self->s.origin2 );
			VectorNormalize( self->s.origin2 );
			return;
		}
	}

	if ( self->classname && !Q_stricmp( "target_push", self->classname ) )
	{
		if( self->spawnflags & PUSH_CONSTANT )
		{
			VectorSubtract ( ent->s.origin, self->s.origin, self->s.origin2 );
			VectorNormalize( self->s.origin2);
			VectorScale (self->s.origin2, self->speed, self->s.origin2);
			return;
		}
	}

	height = ent->s.origin[2] - origin[2];
	gravity = g_gravity.value;
	time = sqrt( height / ( .5 * gravity ) );
	if ( !time ) {
		G_FreeEntity( self );
		return;
	}

	// set s.origin2 to the push velocity
	VectorSubtract ( ent->s.origin, origin, self->s.origin2 );
	self->s.origin2[2] = 0;
	dist = VectorNormalize( self->s.origin2);

	forward = dist / time;
	VectorScale( self->s.origin2, forward, self->s.origin2 );

	self->s.origin2[2] = time * gravity;
}


/*QUAKED trigger_push (.5 .5 .5) ? x x LINEAR x RELATIVE x x INACTIVE MULTIPLE
Must point at a target_position, which will be the apex of the leap.
This will be client side predicted, unlike target_push

LINEAR - Instead of tossing the client at the target_position, it will push them towards it.  Must set a "speed" (see below)
RELATIVE - instead of pushing you in a direction that is always from the center of the trigger to the target_position, it pushes *you* toward the target position, relative to your current location (can use with "speed"... if don't set a speed, it will use the distance from you to the target_position)
INACTIVE - not active until targeted by a target_activate
MULTIPLE - multiple entities can touch this trigger in a single frame *and* if needed, the trigger can have a wait of > 0

wait - how long to wait between pushes: -1 = push only once
speed - when used with the LINEAR spawnflag, pushes the client toward the position at a constant speed (default is 1000)
*/
void SP_trigger_push( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	self->r.svFlags &= ~SVF_NOCLIENT;

	// make sure the client precaches this sound
	G_SoundIndex("sound/weapons/force/jump.wav");

	self->s.eType = ET_PUSH_TRIGGER;
	
	if ( !(self->spawnflags&2) )
	{//start on
		self->touch = trigger_push_touch;
	}

	if ( self->spawnflags & 4 )
	{//linear
		self->speed = 1000;
	}

	self->think = AimAtTarget;
	self->nextthink = level.time + FRAMETIME;
	trap_LinkEntity (self);
}

void Use_target_push( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( !activator->client ) {
		return;
	}

	if ( activator->client->ps.pm_type != PM_NORMAL && activator->client->ps.pm_type != PM_FLOAT ) {
		return;
	}

	G_ActivateBehavior(self,BSET_USE);

	VectorCopy (self->s.origin2, activator->client->ps.velocity);

	// play fly sound every 1.5 seconds
	if ( activator->fly_sound_debounce_time < level.time ) {
		activator->fly_sound_debounce_time = level.time + 1500;
		if (self->noise_index)
		{
			G_Sound( activator, CHAN_AUTO, self->noise_index );
		}
	}
}

/*QUAKED target_push (.5 .5 .5) (-8 -8 -8) (8 8 8) bouncepad CONSTANT
CONSTANT will push activator in direction of 'target' at constant 'speed'

Pushes the activator in the direction.of angle, or towards a target apex.
"speed"		defaults to 1000
if "bouncepad", play bounce noise instead of none
*/
void SP_target_push( gentity_t *self ) {
	if (!self->speed) {
		self->speed = 1000;
	}
	G_SetMovedir (self->s.angles, self->s.origin2);
	VectorScale (self->s.origin2, self->speed, self->s.origin2);

	if ( self->spawnflags & 1 ) {
		self->noise_index = G_SoundIndex("sound/weapons/force/jump.wav");
	} else {
		self->noise_index = 0;	
	}
	if ( self->target ) {
		VectorCopy( self->s.origin, self->r.absmin );
		VectorCopy( self->s.origin, self->r.absmax );
		self->think = AimAtTarget;
		self->nextthink = level.time + FRAMETIME;
	}
	self->use = Use_target_push;
}

/*
==============================================================================

trigger_teleport

==============================================================================
*/

void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {
	gentity_t	*dest;

	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	if ( !other->client ) {
		return;
	}
	if ( other->client->ps.pm_type == PM_DEAD ) {
		return;
	}
	// Spectators only?
	if ( ( self->spawnflags & 1 ) && 
		other->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		return;
	}


	dest = 	G_PickTarget( self->target );
	if (!dest) {
		G_Printf ("Couldn't find teleporter destination\n");
		return;
	}

	TeleportPlayer( other, dest->s.origin, dest->s.angles );
}


/*QUAKED trigger_teleport (.5 .5 .5) ? SPECTATOR
Allows client side prediction of teleportation events.
Must point at a target_position, which will be the teleport destination.

If spectator is set, only spectators can use this teleport
Spectator teleporters are not normally placed in the editor, but are created
automatically near doors to allow spectators to move through them
*/
void SP_trigger_teleport( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	// unless is a spectator trigger
	if ( self->spawnflags & 1 ) {
		self->r.svFlags |= SVF_NOCLIENT;
	} else {
		self->r.svFlags &= ~SVF_NOCLIENT;
	}

	// make sure the client precaches this sound
	G_SoundIndex("sound/weapons/force/speed.wav");

	self->s.eType = ET_TELEPORT_TRIGGER;
	self->touch = trigger_teleporter_touch;

	trap_LinkEntity (self);
}


/*
==============================================================================

trigger_hurt

==============================================================================
*/

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF CAN_TARGET SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.
It does dmg points of damage each server frame
Targeting the trigger will toggle its on / off state.

CAN_TARGET		if you target it, it will toggle on and off
SILENT			supresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"team"			team (1 or 2) to allow hurting (if none then hurt anyone) only applicable for siege
"dmg"			default 5 (whole numbers only)
If dmg is set to -1 this brush will use the fade-kill method

*/
void hurt_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if (activator && activator->inuse && activator->client)
	{
		self->activator = activator;
	}
	else
	{
		self->activator = NULL;
	}

	if (level.siegeMap == SIEGEMAP_URBAN && VALIDSTRING(self->targetname) && !Q_stricmp(self->targetname, "barrelsdead"))
		self->activator = NULL;

	G_ActivateBehavior(self,BSET_USE);

	if ( self->r.linked ) {
		trap_UnlinkEntity( self );
	} else {
		trap_LinkEntity( self );
	}
}

void hurt_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	int		dflags;

	if (g_gametype.integer == GT_SIEGE && self->team && self->team[0])
	{
		int team = atoi(self->team);

		if (other->inuse && other->s.number < MAX_CLIENTS && other->client &&
			other->client->sess.sessionTeam != team)
		{ //real client don't hurt
			return;
		}
		else if (other->inuse && other->client && other->s.eType == ET_NPC &&
			other->s.NPC_class == CLASS_VEHICLE && other->s.teamowner != team)
		{ //vehicle owned by team don't hurt
			return;
		}
	}

	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	if ( !other->takedamage ) {
		return;
	}

	if ( self->timestamp > level.time ) {
		return;
	}

	if (self->damage == -1 && other && other->client && other->health < 1)
	{
		other->client->ps.fallingToDeath = 0;
		respawn(other);
		return;
	}

	if (self->damage == -1 && other && other->client && other->client->ps.fallingToDeath)
	{
		return;
	}

	if ( self->spawnflags & 16 ) {
		self->timestamp = level.time + 1000;
	} else {
		self->timestamp = level.time + FRAMETIME;
	}

	if (self->spawnflags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;

	// duo: never allow slow fade-to-black fall deaths in siege due to the annoying amount of time they take up and the fact that we are on a respawn timer
	if (g_gametype.integer == GT_SIEGE && self->damage == -1)
		self->damage = 99999;

	if (self->damage == -1 && other && other->client)
	{
		if (other->client->ps.otherKillerTime > level.time)
		{ //we're as good as dead, so if someone pushed us into this then remember them
			other->client->ps.otherKillerTime = level.time + 20000;
			other->client->ps.otherKillerDebounceTime = level.time + 10000;
		}
		other->client->ps.fallingToDeath = level.time;

		//rag on the way down, this flag will automatically be cleared for us on respawn
		other->client->ps.eFlags |= EF_RAG;

		//make sure his jetpack is off
		Jetpack_Off(other);

		if (other->NPC)
		{ //kill it now
			vec3_t vDir;

			VectorSet(vDir, 0, 1, 0);
			G_Damage(other, other, other, vDir, other->client->ps.origin, Q3_INFINITE, 0, MOD_FALLING);
		}
		else
		{
			G_EntitySound(other, CHAN_VOICE, G_SoundIndex("*falling1.wav"));
		}

		self->timestamp = 0; //do not ignore others
	}
	else	
	{
		int dmg = self->damage;

		if (dmg == -1)
		{ //so fall-to-blackness triggers destroy evertyhing
			dmg = 99999;
			self->timestamp = 0;
		}
		if (self->activator && self->activator->inuse && self->activator->client)
		{
			G_Damage (other, self->activator, self->activator, NULL, NULL, dmg, dflags|DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT);
		}
		else
		{
			G_Damage (other, self, self, NULL, NULL, dmg, dflags|DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT);
		}
		if (g_fixFallingSounds.integer == 1 && dmg >= 100)
		{
			G_EntitySound(other, CHAN_VOICE, G_SoundIndex("*falling1.wav"));
		}
	}
}

void SP_trigger_hurt( gentity_t *self ) {
	InitTrigger (self);

	gTrigFallSound = G_SoundIndex("*falling1.wav");

	self->noise_index = G_SoundIndex( "sound/weapons/force/speed.wav" );
	self->touch = hurt_touch;

	if ( !self->damage ) {
		self->damage = 5;
	}

	self->r.contents = CONTENTS_TRIGGER;

	if ( self->spawnflags & 2 ) {
		self->use = hurt_use;
	}

	// link in to the world if starting active
	if ( ! (self->spawnflags & 1) ) {
		trap_LinkEntity (self);
	}
	else if (self->r.linked)
	{
		trap_UnlinkEntity(self);
	}
}

#define	INITIAL_SUFFOCATION_DELAY	500 //.5 seconds
void space_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	if (!other || !other->inuse || !other->client )
		//NOTE: we need vehicles to know this, too...
		//|| other->s.number >= MAX_CLIENTS)
	{
		return;
	}

	if ( other->s.number < MAX_CLIENTS//player
		&& other->client->ps.m_iVehicleNum//in a vehicle
		&& other->client->ps.m_iVehicleNum >= MAX_CLIENTS )
	{//a player client inside a vehicle
		gentity_t *veh = &g_entities[other->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle &&
			veh->m_pVehicle->m_pVehicleInfo->hideRider)
		{ //if they are "inside" a vehicle, then let that protect them from THE HORRORS OF SPACE.
			other->client->inSpaceSuffocation = 0;
			other->client->inSpaceIndex = ENTITYNUM_NONE;
			return;
		}
	}

	if (!G_PointInBounds(other->client->ps.origin, self->r.absmin, self->r.absmax))
	{ //his origin must be inside the trigger
		return;
	}

	if (!other->client->inSpaceIndex ||
		other->client->inSpaceIndex == ENTITYNUM_NONE)
	{ //freshly entering space
		other->client->inSpaceSuffocation = level.time + INITIAL_SUFFOCATION_DELAY;
	}

	other->client->inSpaceIndex = self->s.number;
}

/*QUAKED trigger_space (.5 .5 .5) ? 
causes human clients to suffocate and have no gravity.

*/
void SP_trigger_space(gentity_t *self)
{
	InitTrigger(self);
	self->r.contents = CONTENTS_TRIGGER;
	
	self->touch = space_touch;

    trap_LinkEntity(self);
}

void shipboundary_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	gentity_t *ent;

	if (!other || !other->inuse || !other->client ||
		other->s.number < MAX_CLIENTS ||
		!other->m_pVehicle)
	{ //only let vehicles touch
		return;
	}

	if ( other->client->ps.hyperSpaceTime && level.time - other->client->ps.hyperSpaceTime < HYPERSPACE_TIME )
	{//don't interfere with hyperspacing ships
		return;
	}

	ent = G_Find (NULL, FOFS(targetname), self->target);
	if (!ent || !ent->inuse)
	{ //this is bad
		G_Error("trigger_shipboundary has invalid target '%s'\n", self->target);
		return;
	}

	if (!other->client->ps.m_iVehicleNum || other->m_pVehicle->m_iRemovedSurfaces)
	{ //if a vehicle touches a boundary without a pilot in it or with parts missing, just blow the thing up
		G_Damage(other, other, other, NULL, other->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		return;
	}

	//make sure this sucker is linked so the prediction knows where to go
	trap_LinkEntity(ent);

	other->client->ps.vehTurnaroundIndex = ent->s.number;
	other->client->ps.vehTurnaroundTime = level.time + (self->genericValue1*2);

	//keep up the detailed checks for another 2 seconds
	self->genericValue7 = level.time + 2000;
}

void shipboundary_think(gentity_t *ent)
{
	int			iEntityList[MAX_GENTITIES];
	int			numListedEntities;
	int			i = 0;
	gentity_t	*listedEnt;

	ent->nextthink = level.time + 100;

	if (ent->genericValue7 < level.time)
	{ //don't need to be doing this check, no one has touched recently
		return;
	}

	numListedEntities = trap_EntitiesInBox( ent->r.absmin, ent->r.absmax, iEntityList, MAX_GENTITIES );
	while (i < numListedEntities)
	{
		listedEnt = &g_entities[iEntityList[i]];
		if (listedEnt->inuse && listedEnt->client && listedEnt->client->ps.m_iVehicleNum)
		{
            if (listedEnt->s.eType == ET_NPC &&
				listedEnt->s.NPC_class == CLASS_VEHICLE)
			{
				Vehicle_t *pVeh = listedEnt->m_pVehicle;
				if (pVeh && pVeh->m_pVehicleInfo->type == VH_FIGHTER)
				{
                    shipboundary_touch(ent, listedEnt, NULL);
				}
			}
		}
		i++;
	}
}

/*QUAKED trigger_shipboundary (.5 .5 .5) ? 
causes vehicle to turn toward target and travel in that direction for a set time when hit.

"target"		name of entity to turn toward (can be info_notnull, or whatever).
"traveltime"	time to travel in this direction

*/
void SP_trigger_shipboundary(gentity_t *self)
{
	InitTrigger(self);
	self->r.contents = CONTENTS_TRIGGER;
	
	if (!self->target || !self->target[0])
	{
		G_Error("trigger_shipboundary without a target.");
	}
	G_SpawnInt("traveltime", "0", &self->genericValue1);

	if (!self->genericValue1)
	{
		G_Error("trigger_shipboundary without traveltime.");
	}

	self->think = shipboundary_think;
	self->nextthink = level.time + 500;
	self->touch = shipboundary_touch;

    trap_LinkEntity(self);
}

void hyperspace_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	gentity_t *ent;

	if (!other || !other->inuse || !other->client ||
		other->s.number < MAX_CLIENTS ||
		!other->m_pVehicle)
	{ //only let vehicles touch
		return;
	}

	if ( other->client->ps.hyperSpaceTime && level.time - other->client->ps.hyperSpaceTime < HYPERSPACE_TIME )
	{//already hyperspacing, just keep us moving
		if ( (other->client->ps.eFlags2&EF2_HYPERSPACE) )
		{//they've started the hyperspace but haven't been teleported yet
			float timeFrac = ((float)(level.time-other->client->ps.hyperSpaceTime))/HYPERSPACE_TIME;
			if ( timeFrac >= HYPERSPACE_TELEPORT_FRAC )
			{//half-way, now teleport them!
				vec3_t	diff, fwd, right, up, newOrg;
				float	fDiff, rDiff, uDiff;
				//take off the flag so we only do this once
				other->client->ps.eFlags2 &= ~EF2_HYPERSPACE;
				//Get the offset from the local position
				ent = G_Find (NULL, FOFS(targetname), self->target);
				if (!ent || !ent->inuse)
				{ //this is bad
					G_Error("trigger_hyperspace has invalid target '%s'\n", self->target);
					return;
				}
				VectorSubtract( other->client->ps.origin, ent->s.origin, diff );
				AngleVectors( ent->s.angles, fwd, right, up );
				fDiff = DotProduct( fwd, diff );
				rDiff = DotProduct( right, diff );
				uDiff = DotProduct( up, diff );
				//Now get the base position of the destination
				ent = G_Find (NULL, FOFS(targetname), self->target2);
				if (!ent || !ent->inuse)
				{ //this is bad
					G_Error("trigger_hyperspace has invalid target2 '%s'\n", self->target2);
					return;
				}
				VectorCopy( ent->s.origin, newOrg );
				//finally, add the offset into the new origin
				AngleVectors( ent->s.angles, fwd, right, up );
				VectorMA( newOrg, fDiff, fwd, newOrg );
				VectorMA( newOrg, rDiff, right, newOrg );
				VectorMA( newOrg, uDiff, up, newOrg );
				//now put them in the offset position, facing the angles that position wants them to be facing
				TeleportPlayer( other, newOrg, ent->s.angles );
				if ( other->m_pVehicle && other->m_pVehicle->m_pPilot )
				{//teleport the pilot, too
					TeleportPlayer( (gentity_t*)other->m_pVehicle->m_pPilot, newOrg, ent->s.angles );
					//FIXME: and the passengers?
				}
				//make them face the new angle
				VectorCopy( ent->s.angles, other->client->ps.hyperSpaceAngles );
				//sound
				G_Sound( other, CHAN_LOCAL, G_SoundIndex( "sound/vehicles/common/hyperend.wav" ) );
			}
		}
		return;
	}
	else
	{
		ent = G_Find (NULL, FOFS(targetname), self->target);
		if (!ent || !ent->inuse)
		{ //this is bad
			G_Error("trigger_hyperspace has invalid target '%s'\n", self->target);
			return;
		}

		if (!other->client->ps.m_iVehicleNum || other->m_pVehicle->m_iRemovedSurfaces)
		{ //if a vehicle touches a boundary without a pilot in it or with parts missing, just blow the thing up
			G_Damage(other, other, other, NULL, other->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
			return;
		}
		//other->client->ps.hyperSpaceIndex = ent->s.number;
		VectorCopy( ent->s.angles, other->client->ps.hyperSpaceAngles );
		other->client->ps.hyperSpaceTime = level.time;
	}
}

/*QUAKED trigger_hyperspace (.5 .5 .5) ? 
Ship will turn to face the angles of the first target_position then fly forward, playing the hyperspace effect, then pop out at a relative point around the target

"target"		whatever position the ship teleports from in relation to the target_position specified here, that's the relative position the ship will spawn at around the target2 target_position
"target2"		name of target_position to teleport the ship to (will be relative to it's origin)
*/
void SP_trigger_hyperspace(gentity_t *self)
{
	//register the hyperspace end sound (start sounds are customized)
	G_SoundIndex( "sound/vehicles/common/hyperend.wav" );

	InitTrigger(self);
	self->r.contents = CONTENTS_TRIGGER;
	
	if (!self->target || !self->target[0])
	{
		G_Error("trigger_hyperspace without a target.");
	}
	if (!self->target2 || !self->target2[0])
	{
		G_Error("trigger_hyperspace without a target2.");
	}
	
	self->delay = Distance( self->r.absmax, self->r.absmin );//my size

	self->touch = hyperspace_touch;

    trap_LinkEntity(self);

}
/*
==============================================================================

timer

==============================================================================
*/


/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
This should be renamed trigger_timer...
Repeatedly fires its targets.
Can be turned on or off by using.

"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0
so, the basic time between firing is a random time between
(wait - random) and (wait + random)

*/
void func_timer_think( gentity_t *self ) {
	G_UseTargets (self, self->activator);
	// set time before next firing
	self->nextthink = level.time + 1000 * ( self->wait + crandom() * self->random );
}

void func_timer_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	self->activator = activator;

	G_ActivateBehavior(self,BSET_USE);

	// if on, turn it off
	if ( self->nextthink ) {
		self->nextthink = 0;
		return;
	}

	// turn it on
	func_timer_think (self);
}

void SP_func_timer( gentity_t *self ) {
	G_SpawnFloat( "random", "1", &self->random);
	G_SpawnFloat( "wait", "1", &self->wait );

	self->use = func_timer_use;
	self->think = func_timer_think;

	if ( self->random >= self->wait ) {
		self->random = self->wait - 1;//NOTE: was - FRAMETIME, but FRAMETIME is in msec (100) and these numbers are in *seconds*!
		G_Printf( "func_timer at %s has random >= wait\n", vtos( self->s.origin ) );
	}

	if ( self->spawnflags & 1 ) {
		self->nextthink = level.time + FRAMETIME;
		self->activator = self;
	}

	self->r.svFlags = SVF_NOCLIENT;
}

gentity_t *asteroid_pick_random_asteroid( gentity_t *self )
{
	int			t_count = 0, pick;
	gentity_t	*t = NULL;

	while ( (t = G_Find (t, FOFS(targetname), self->target)) != NULL )
	{
		if (t != self)
		{
			t_count++;
		}
	}

	if(!t_count)
	{
		return NULL;
	}

	if(t_count == 1)
	{
		return t;
	}

	//FIXME: need a seed
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
		
		if(t_count == pick)
		{
			return t;
		}
	}
	return NULL;
}

int asteroid_count_num_asteroids( gentity_t *self )
{
	int	i, count = 0;

	for ( i = MAX_CLIENTS; i < ENTITYNUM_WORLD; i++ )
	{
		if ( !g_entities[i].inuse )
		{
			continue;
		}
		if ( g_entities[i].r.ownerNum == self->s.number )
		{
			count++;
		}
	}
	return count;
}

extern void SP_func_rotating (gentity_t *ent);
extern void Q3_Lerp2Origin( int taskID, int entID, vec3_t origin, float duration );
void asteroid_field_think(gentity_t *self)
{
	int numAsteroids = asteroid_count_num_asteroids( self );

	self->nextthink = level.time + 500;

	if ( numAsteroids < self->count )
	{
		//need to spawn a new asteroid
		gentity_t *newAsteroid = G_Spawn();
		if ( newAsteroid )
		{
			vec3_t startSpot, endSpot, startAngles;
			float dist, speed = flrand( self->speed * 0.25f, self->speed * 2.0f );
			int	capAxis, axis, time = 0;
			gentity_t *copyAsteroid = asteroid_pick_random_asteroid( self );
			if ( copyAsteroid )
			{
				newAsteroid->model = copyAsteroid->model;
				newAsteroid->model2 = copyAsteroid->model2;
				newAsteroid->health = copyAsteroid->health;
				newAsteroid->spawnflags = copyAsteroid->spawnflags;
				newAsteroid->mass = copyAsteroid->mass;
				newAsteroid->damage = copyAsteroid->damage;
				newAsteroid->speed = copyAsteroid->speed;

				G_SetOrigin( newAsteroid, copyAsteroid->s.origin );
				G_SetAngles( newAsteroid, copyAsteroid->s.angles );
				newAsteroid->classname = "func_rotating";

				SP_func_rotating( newAsteroid );

				newAsteroid->genericValue15 = copyAsteroid->genericValue15;
				newAsteroid->s.iModelScale = copyAsteroid->s.iModelScale;
				newAsteroid->maxHealth = newAsteroid->health;
				G_ScaleNetHealth(newAsteroid);
				newAsteroid->radius = copyAsteroid->radius;
				newAsteroid->material = copyAsteroid->material;

				//keep track of it
				newAsteroid->r.ownerNum = self->s.number;

				//move it
				capAxis = Q_irand( 0, 2 );
				for ( axis = 0; axis < 3; axis++ )
				{
					if ( axis == capAxis )
					{
						if ( Q_irand( 0, 1 ) )
						{
							startSpot[axis] = self->r.mins[axis];
							endSpot[axis] = self->r.maxs[axis];
						}
						else
						{
							startSpot[axis] = self->r.maxs[axis];
							endSpot[axis] = self->r.mins[axis];
						}
					}
					else
					{
						startSpot[axis] = self->r.mins[axis]+(flrand(0,1.0f)*(self->r.maxs[axis]-self->r.mins[axis]));
						endSpot[axis] = self->r.mins[axis]+(flrand(0,1.0f)*(self->r.maxs[axis]-self->r.mins[axis]));
					}
				}
				//FIXME: maybe trace from start to end to make sure nothing is in the way?  How big of a trace?

				G_SetOrigin( newAsteroid, startSpot );
				dist = Distance( endSpot, startSpot );
				time = ceil(dist/speed)*1000;
				Q3_Lerp2Origin( -1, newAsteroid->s.number, endSpot, time );

				//spin it
				startAngles[0] = flrand( -360, 360 );
				startAngles[1] = flrand( -360, 360 );
				startAngles[2] = flrand( -360, 360 );
				G_SetAngles( newAsteroid, startAngles );
				newAsteroid->s.apos.trDelta[0] = flrand( -100, 100 );
				newAsteroid->s.apos.trDelta[1] = flrand( -100, 100 );
				newAsteroid->s.apos.trDelta[2] = flrand( -100, 100 );
				newAsteroid->s.apos.trTime = level.time;
				newAsteroid->s.apos.trType = TR_LINEAR;

				//remove itself when done
				newAsteroid->think = G_FreeEntity;
				newAsteroid->nextthink = level.time+time;

				//think again sooner if need even more
				if ( numAsteroids+1 < self->count )
				{//still need at least one more
					//spawn it in 100ms
					self->nextthink = level.time + 100;
				}
			}
		}
	}
}

/*QUAKED trigger_asteroid_field (.5 .5 .5) ? 
speed - how fast, on average, the asteroid moves
count - how many asteroids, max, to have at one time
target - target this at func_rotating asteroids
*/
void SP_trigger_asteroid_field(gentity_t *self)
{
	trap_SetBrushModel( self, self->model );
	self->r.contents = 0;

	if ( !self->count )
	{
		self->health = 20;
	}

	if ( !self->speed )
	{
		self->speed = 10000;
	}

	self->think = asteroid_field_think;
	self->nextthink = level.time + 100;

    trap_LinkEntity(self);
}

