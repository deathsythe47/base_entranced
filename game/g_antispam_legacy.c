// the anti-spam verdict as it was before g_antispam.c, kept only so the two can be compared on live shots
//
// g_antiSpamShadow 1: print both verdicts to the shooter on every shot, this legacy verdict decides
// g_antiSpamShadow 2: same, but the g_antispam.c verdict decides
//
// the legacy code below is unchanged except that it no longer refunds ammo itself, takes the aim direction as a
// parameter, and keeps its door bookkeeping in this file instead of on every entity
// to retire it, delete this file and the G_AntiSpamShadow_Verdict call in g_antispam.c

#include "g_local.h"
#include <time.h>

#define	DISTANCE_FROM_ENEMY_TO_DOOR_FOR_DOORSPAM	512

extern int BotMindTricked(int botClient, int enemyClient);
extern qboolean InFOV(gentity_t *ent, gentity_t *from, int hFOV, int vFOV);

static qboolean legacyDoorFound[MAX_GENTITIES];
static qboolean legacyFoundNeverReturnDoor;

static qboolean Legacy_OnValidMapForAntiSpam(qboolean doorSpam)
{
	vmCvar_t	mapname;
	trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);

	if (!Q_stricmp(mapname.string, "siege_codes") || !Q_stricmpn(mapname.string, "mp/siege_crystals", 17))
		return qfalse;

	if (doorSpam && (level.siegeMap == SIEGEMAP_URBAN || level.siegeMap == SIEGEMAP_ANSION)) // workaround
		return qfalse;

	return qtrue;
}

static qboolean Legacy_CheckIfIAmAFilthySpammer(gentity_t *ent, qboolean checkDoorspam, qboolean removeMissileIfIAmASpammer, gentity_t *missile, weapon_t weaponBeingUsed, qboolean altFire, qboolean trackDoorspamStatusOfProj, float range, qboolean refundMyAmmo, const vec3_t forward)
{
	vec3_t		start, end;
	vec3_t		distanceToDoor[64];
	trace_t		tr;
	gentity_t	*traceEnt;
	int			ignore;
	int			x, z;
	int			foundDoorsIndex[64];
	int			n;

	gentity_t	*entity_list[MAX_GENTITIES], *potentialSpamVictim, *enemyInStationList[MAX_GENTITIES], *enemyInStation1List[MAX_GENTITIES], *potentialEnemyInStation, *potentialEnemyInStation1;
	vec3_t		throwerOrigin, distanceToVictim, distanceBetweenMeAndVictim, distanceBetweenDoorAndVictim;
	int			possibleTargets;
	int			possibleEnemiesInStation;
	int			possibleEnemiesInStation1;
	int			numConfirmedEnemiesInStation = 0;
	int			numConfirmedEnemiesInStation1 = 0;
	qboolean	iAmADirtyFuckingSpammer = qfalse;
	qboolean	thereIsAWalkerOrProtector = qfalse;
	qboolean	thereIsADoor = qfalse;
	qboolean	thereIsAnEnemyBetweenMeAndDoor = qfalse;
	qboolean	thereIsAnEnemyBehindDoor = qfalse;
	qboolean	overrideDefinitelyNotSpam = qfalse;
	int			sizeOfConeOfProhibitedSpam, i;
	int			heightAdjustment, xAdjustment, yAdjustment;
	float		originalend0, originalend1, originalend2;
	float		heightLowerBound, heightUpperBound;
	int			numberOfDoorsFound = 0;
	int			doorspamDistanceCheck = DISTANCE_FROM_ENEMY_TO_DOOR_FOR_DOORSPAM;
	//qboolean	aimingAtCargoHallDoor = qfalse;
	vmCvar_t	mapname;
	trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);

	if (!ent || !ent->client || !Legacy_OnValidMapForAntiSpam(checkDoorspam) || g_gametype.integer != GT_SIEGE)
	{
		return qfalse;
	}

	if (ent->client->sess.sessionTeam == TEAM_BLUE)
	{
		heightUpperBound = (ent->client->ps.origin[2] + 296); //approximately height distance from codes main room floor to top of bunker
		heightLowerBound = (ent->client->ps.origin[2] - 116); //approximately height distance from high part of ravine to low part of ravine (outside codes delivery room)

		if (checkDoorspam)
		{
			memset(&tr, 0, sizeof(tr)); //to shut the compiler up
			VectorCopy(ent->client->ps.origin, start);
			start[2] += ent->client->ps.viewheight;//By eyes

			if (level.siegeMap == SIEGEMAP_CARGO)
			{
				if (ent->client->ps.origin[0] >= 1846 && ent->client->ps.origin[0] <= 3269 && ent->client->ps.origin[1] >= 1719 && ent->client->ps.origin[1] <= 3422)
				{
					//allow for more height fudging in the 2nd obj of cargo2 v1.1+
					heightUpperBound = (ent->client->ps.origin[2] + 296); //approximately height distance from codes main room floor to top of bunker
					heightLowerBound = (ent->client->ps.origin[2] - 99999); //huge
					doorspamDistanceCheck = 1024;
					//trap_SendServerCommand(-1, va("print \"Debug: 2nd obj\n\""));
				}
				else if (ent->client->ps.origin[0] >= 6678 && ent->client->ps.origin[0] <= 7277 && ent->client->ps.origin[1] >= 62 && ent->client->ps.origin[1] <= 708)
				{
					//cargo2 v1.1+, station 1
					heightUpperBound = (ent->client->ps.origin[2] + 99999); //huge
					heightLowerBound = (ent->client->ps.origin[2] - 99999); //huge
					//trap_SendServerCommand(-1, va("print \"Debug: station 1\n\""));
					//trap_SendServerCommand(-1, va("print \"^%iIn station 1 obj room, ^%ichecking for enemies.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
					VectorCopy(ent->client->ps.origin, throwerOrigin);
					possibleEnemiesInStation1 = G_RadiusList(throwerOrigin, 3000, ent, qtrue, enemyInStation1List);

					for (n = 0; n < possibleEnemiesInStation1; n++)
					{
						potentialEnemyInStation1 = enemyInStation1List[n];

						if (!potentialEnemyInStation1 || !potentialEnemyInStation1->client)
						{
							continue; //??? uhh...this should never happen, but whatever
						}

						if (potentialEnemyInStation1->client->sess.sessionTeam && potentialEnemyInStation1->client->sess.sessionTeam != TEAM_RED)
						{
							continue; //must be on offense
						}

						if (potentialEnemyInStation1 == ent || !potentialEnemyInStation1->takedamage || potentialEnemyInStation1->health <= 0 || potentialEnemyInStation1->client->tempSpectate >= level.time || potentialEnemyInStation1->flags & FL_NOTARGET || potentialEnemyInStation1->s.eType == ET_NPC || potentialEnemyInStation1->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
						{
							continue; //miscellaneous checks
						}

						if (potentialEnemyInStation1->client->ps.origin[0] >= 6678 && potentialEnemyInStation1->client->ps.origin[0] <= 7277 && potentialEnemyInStation1->client->ps.origin[1] >= 62 && potentialEnemyInStation1->client->ps.origin[1] <= 708)
						{
							//there is an enemy in the station 1
							numConfirmedEnemiesInStation1++;
						}
					}
					if (numConfirmedEnemiesInStation1)
					{
						//there are enemies in the Station1, so "spamming" is okay
						//trap_SendServerCommand(-1, va("print \"^%iEnemies in station 1, ^%ispam allowed.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
						return qfalse;
					}
					else
					{
						//trap_SendServerCommand(-1, va("print \"^%iNo enemies detected in station 1, ^%iproceeding normally.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
						//proceed normally
					}
				}
				else if (ent->client->ps.origin[0] >= 6935 && ent->client->ps.origin[0] <= 7579 && ent->client->ps.origin[1] >= -1318 && ent->client->ps.origin[1] <= -588)
				{
					//cargo2 v1.1+, station 2
					//trap_SendServerCommand(-1, va("print \"Debug: station 2\n\""));
				}
				else if (ent->client->ps.origin[0] >= 6496 && ent->client->ps.origin[0] <= 6799 && ent->client->ps.origin[1] >= -1313 && ent->client->ps.origin[1] <= -877)
				{
					//cargo2 v1.1+, vent shield room thing
					//trap_SendServerCommand(-1, va("print \"Debug: vent shield room thing\n\""));
				}
				else if (ent->client->ps.origin[0] >= 6081 && ent->client->ps.origin[0] <= 6564 && ent->client->ps.origin[1] >= -299 && ent->client->ps.origin[1] <= 1047 && level.ccCompleted == qtrue)
				{
					//cargo2 v1.1+, hallway near cc
					//trap_SendServerCommand(-1, va("print \"Debug: hallway near cc\n\""));
				}
				else
				{
					//no other areas are considered for doorspam detection
					//trap_SendServerCommand(-1, va("print \"^1Debug: other area, returning qfalse\n\""));
					return qfalse;
				}
			}
			
			if (!Q_stricmp(mapname.string, "mp/siege_eat_shower") && weaponBeingUsed == WP_ROCKET_LAUNCHER)
			{
				if (level.objectiveJustCompleted == 3)
				{
					//we're on the 4th obj of this map, so always allow rocket spam.
					return qfalse;
				}
			}
			if ((level.siegeMap == SIEGEMAP_HOTH || level.siegeMap == SIEGEMAP_DESERT || level.siegeMap == SIEGEMAP_NAR) && !level.totalObjectivesCompleted)
			{
				return qfalse;
			}
			if (level.siegeMap == SIEGEMAP_HOTH && level.totalObjectivesCompleted == 5 && ent->client->ps.origin[2] < 455)
				return qfalse;
			if (level.siegeMap == SIEGEMAP_KORRIBAN)
			{
				return qfalse;
			}
			//start[0] += debug_testHeight1.integer;
			//start[1] += debug_testHeight2.integer;
			//start[2] += debug_testHeight3.integer;

			VectorMA(start, range, forward, end);
			ignore = ent->s.number;
			originalend0 = end[0];
			originalend1 = end[1];
			originalend2 = end[2];
			//end[0] += debug_testHeight4.integer;
			//end[1] += debug_testHeight5.integer;
			//end[2] += debug_testHeight6.integer;

			//trap_SendServerCommand(-1, va("print \"start: %f, %f, %f   end: %f, %f, %f\n\"", start[0], start[1], start[2], end[0], end[1], end[2]));

			for (yAdjustment = 1024; yAdjustment >= -1024; yAdjustment -= 512)
			{
				end[0] = originalend0;
				end[0] += yAdjustment;
				for (xAdjustment = 1024; xAdjustment >= -1024; xAdjustment -= 512)
				{
					end[1] = originalend1;
					end[1] += xAdjustment;
					for (heightAdjustment = 4096; heightAdjustment >= -4096; heightAdjustment -= 512) //check many different heights to detect doorspam when aiming slightly above or below the door.
					{
						end[2] = originalend2;
						end[2] += heightAdjustment;
						//G_PlayEffectID(G_EffectIndex("disruptor/wall_impact.efx"), end, start);
						trap_G2Trace(&tr, start, NULL, NULL, end, ignore, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
						traceEnt = &g_entities[tr.entityNum];
						//if (traceEnt->classname)
							//trap_SendServerCommand(-1, va("print \"^%iDebug: classname == %s\n\"", Q_irand(1, 7), traceEnt->classname));
						if (traceEnt && traceEnt->client && traceEnt->client->sess.sessionTeam == TEAM_RED &&
							traceEnt->health > 0 && traceEnt->takedamage && !(traceEnt->client->tempSpectate >= level.time) && !(traceEnt->flags & FL_NOTARGET))
						{
							//we are aiming at a player
							//trap_SendServerCommand(-1, va("print \"We are aiming at a player, returning qfalse\n\""));
							overrideDefinitelyNotSpam = qtrue;
						}
						else if (traceEnt && tr.entityNum < ENTITYNUM_WORLD && traceEnt->classname && !Q_stricmp(traceEnt->classname, "item_shield"))
						{
							//we are aiming at a shield
							//trap_SendServerCommand(-1, va("print \"We are aiming at a shield, returning qfalse\n\""));
							overrideDefinitelyNotSpam = qtrue;
						}
						else if (traceEnt && tr.entityNum < ENTITYNUM_WORLD && traceEnt->classname && !Q_stricmp(traceEnt->classname, "func_door") && traceEnt->moverState == MOVER_POS1 && !(traceEnt->wait && traceEnt->wait == -1) && !(traceEnt->spawnflags & 4) && !(traceEnt->spawnflags & 8) && !(traceEnt->spawnflags & 16) && legacyDoorFound[tr.entityNum] != qtrue)
						{
							//we are aiming directly at a closed door
							thereIsADoor = qtrue;
							numberOfDoorsFound++;
							VectorSubtract(start, tr.endpos, distanceToDoor[numberOfDoorsFound]);
							//if (trackDoorspamStatusOfProj && missile)
							//{
							//	missile->closedDoorWeWereFiredAt = tr.entityNum;
							//}
							legacyDoorFound[tr.entityNum] = qtrue;
							foundDoorsIndex[numberOfDoorsFound] = tr.entityNum;
							if (traceEnt->wait < 0)
								legacyFoundNeverReturnDoor = qtrue;
							//goto foundTheDoor;
						}
					}
				}
			}

			//foundTheDoor:

			//if (thereIsADoor)
			//{
				//trap_SendServerCommand(-1, va("print \"^%iDebug: ^%iaiming at a door\n\"", Q_irand(1, 7), Q_irand(1,7)));
			//}

			for (z = 1; z <= numberOfDoorsFound; z++)
			{
				if (foundDoorsIndex[z])
				{
					legacyDoorFound[foundDoorsIndex[z]] = qfalse;
				}
			}

			if (overrideDefinitelyNotSpam)
			{
				return qfalse;
			}
		}
		else
		{
			//you are always allowed to minespam in the walker spawn areas
			if (level.siegeMap == SIEGEMAP_HOTH)
			{
				if (ent->client->ps.origin[0] >= 6549 && ent->client->ps.origin[0] <= 8204 && ent->client->ps.origin[1] >= -1394 && ent->client->ps.origin[1] <= 762)
				{
					//trap_SendServerCommand(-1, va("print \"^%iFirst walker spawn point, ^%ispam allowed.\n\"", Q_irand(1,7), Q_irand(1, 7)));
					return qfalse; //first obj walker spawn
				}
				if (ent->client->ps.origin[0] >= 2287 && ent->client->ps.origin[0] <= 4113 && ent->client->ps.origin[1] >= -1083 && ent->client->ps.origin[1] <= 549)
				{
					//trap_SendServerCommand(-1, va("print \"^%iSecond walker spawn point, ^%ispam allowed.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
					return qfalse; //second/third obj walker spawn
				}
				if (ent->client->ps.origin[0] >= -1440 && ent->client->ps.origin[0] <= -1136 && ent->client->ps.origin[1] >= -230 && ent->client->ps.origin[1] <= 155 &&
					ent->client->ps.origin[2] >= 161 && ent->client->ps.origin[2] <= 200)
				{
					//trap_SendServerCommand(-1, va("print \"^%iNear top of lift, ^%iincreased height detection.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
					heightLowerBound = (ent->client->ps.origin[2] - 9999); //we're standing near the top of the infirmary lift, so increase lower height detection(to detect throwing mines down on people)
				}
				else if (level.totalObjectivesCompleted == 5 && ent->client->ps.origin[2] >= 470 && ent->client->ps.origin[1] >= -615) {
					// fix for spamming the short lift at short
					vec3_t comparisonPoint = { -2224, -321, 484 };
					float dist = DistanceHorizontal(ent->client->ps.origin, comparisonPoint);
					if (dist <= 768)
						heightLowerBound = 180;
				}
			}
			else if (level.siegeMap == SIEGEMAP_NAR) //nar station 1 obj room exception. mine placement is okay if you are in the obj room and there are no enemies in the station
			{
				if (ent->client->ps.origin[0] >= -1660 && ent->client->ps.origin[0] <= -989 && ent->client->ps.origin[1] >= 7119 && ent->client->ps.origin[1] <= 7639)
				{
					//trap_SendServerCommand(-1, va("print \"^%iIn station 1 obj room, ^%ichecking for enemies.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
					VectorCopy(ent->client->ps.origin, throwerOrigin);
					possibleEnemiesInStation = G_RadiusList(throwerOrigin, 3000, ent, qtrue, enemyInStationList);

					for (n = 0; n < possibleEnemiesInStation; n++)
					{
						potentialEnemyInStation = enemyInStationList[n];

						if (!potentialEnemyInStation || !potentialEnemyInStation->client)
						{
							continue; //??? uhh...this should never happen, but whatever
						}

						if (potentialEnemyInStation->client->sess.sessionTeam && potentialEnemyInStation->client->sess.sessionTeam != TEAM_RED)
						{
							continue; //must be on offense
						}

						if (potentialEnemyInStation == ent || !potentialEnemyInStation->takedamage || potentialEnemyInStation->health <= 0 || potentialEnemyInStation->client->tempSpectate >= level.time || potentialEnemyInStation->flags & FL_NOTARGET || potentialEnemyInStation->s.eType == ET_NPC || potentialEnemyInStation->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
						{
							continue; //miscellaneous checks
						}

						if (potentialEnemyInStation->client->ps.origin[0] >= -1660 && potentialEnemyInStation->client->ps.origin[0] <= -989 && potentialEnemyInStation->client->ps.origin[1] >= 7119 && potentialEnemyInStation->client->ps.origin[1] <= 8280)
						{
							//there is an enemy in the station
							numConfirmedEnemiesInStation++;
						}
					}
					if (!numConfirmedEnemiesInStation)
					{
						//there are no enemies in the station, so it's okay to place mines
						//trap_SendServerCommand(-1, va("print \"^%iNo enemies in station, ^%ispam allowed.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
						return qfalse;
					}
					else
					{
						//trap_SendServerCommand(-1, va("print \"^%iEnemies detected in station, ^%iproceeding normally.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
						//proceed normally
					}
				}
			}
			memset(&tr, 0, sizeof(tr)); //to shut the compiler up
			VectorCopy(ent->client->ps.origin, start);
			start[2] += ent->client->ps.viewheight;//By eyes

			//start[0] += debug_testHeight1.integer;
			//start[1] += debug_testHeight2.integer;
			//start[2] += debug_testHeight3.integer;

			VectorMA(start, range, forward, end);
			ignore = ent->s.number;
			originalend0 = end[0];
			originalend1 = end[1];
			originalend2 = end[2];
			//end[0] += debug_testHeight4.integer;
			//end[1] += debug_testHeight5.integer;
			//end[2] += debug_testHeight6.integer;

			//trap_SendServerCommand(-1, va("print \"start: %f, %f, %f   end: %f, %f, %f\n\"", start[0], start[1], start[2], end[0], end[1], end[2]));

			for (yAdjustment = 1024; yAdjustment >= -1024; yAdjustment -= 512)
			{
				end[0] = originalend0;
				end[0] += yAdjustment;
				for (xAdjustment = 1024; xAdjustment >= -1024; xAdjustment -= 512)
				{
					end[1] = originalend1;
					end[1] += xAdjustment;
					for (heightAdjustment = 4096; heightAdjustment >= -4096; heightAdjustment -= 512) //check many different heights to detect doorspam when aiming slightly above or below the door.
					{
						end[2] = originalend2;
						end[2] += heightAdjustment;
						//G_PlayEffectID(G_EffectIndex("disruptor/wall_impact.efx"), end, start);
						trap_G2Trace(&tr, start, NULL, NULL, end, ignore, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
						traceEnt = &g_entities[tr.entityNum];
						//if (traceEnt->classname)
						//trap_SendServerCommand(-1, va("print \"^%iDebug: classname == %s\n\"", Q_irand(1, 7), traceEnt->classname));
						if (traceEnt && tr.entityNum < ENTITYNUM_WORLD && traceEnt->classname && !Q_stricmp(traceEnt->classname, "item_shield"))
						{
							//we are aiming at a shield
							//trap_SendServerCommand(-1, va("print \"We are aiming at a shield, returning qfalse\n\""));
							return qfalse;
						}
					}
				}
			}
		}


		VectorCopy(ent->client->ps.origin, throwerOrigin);
		//possibleTargets = G_RadiusList(throwerOrigin, 9999, ent, qtrue, entity_list);
		possibleTargets = G_RadiusList(throwerOrigin, range, ent, qtrue, entity_list);

		for (i = 0; i < possibleTargets; i++)
		{
			potentialSpamVictim = entity_list[i];

			if (!potentialSpamVictim)
			{
				continue; //??? uhh...this should never happen, but whatever
			}

			if (potentialSpamVictim->s.eType && potentialSpamVictim->s.eType == ET_NPC && potentialSpamVictim->m_pVehicle && (potentialSpamVictim->m_pVehicle->m_pVehicleInfo->type == VH_WALKER || potentialSpamVictim->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER))
			{
				if (level.siegeMap == SIEGEMAP_HOTH) {
					if (level.totalObjectivesCompleted <= 2) { //allow walker to trigger "allow spam" for first 3 objs
						thereIsAWalkerOrProtector = qtrue;
						continue;
					}
					if (level.totalObjectivesCompleted >= 4) //don't allow walker to trigger "allow spam" for later objs
						continue;
					if (level.totalObjectivesCompleted == 3) //allow walker to trigger "allow spam" if you're near it i guess
					{
						if (ent->client->ps.origin[0] >= -917 && ent->client->ps.origin[0] <= 8219 &&
							ent->client->ps.origin[1] >= -3729 && ent->client->ps.origin[1] <= 1635) {
							thereIsAWalkerOrProtector = qtrue;
						}
						continue;
					}
				}
				else
					thereIsAWalkerOrProtector = qtrue;//it's okay to spam if there's a walker or fighter nearby (regardless of angle)
					continue;
			}

			if (potentialSpamVictim->client && &potentialSpamVictim->client->ps && potentialSpamVictim->client->ps.m_iVehicleNum && &g_entities[potentialSpamVictim->client->ps.m_iVehicleNum]
				&& (&g_entities[potentialSpamVictim->client->ps.m_iVehicleNum])->m_pVehicle && (&g_entities[potentialSpamVictim->client->ps.m_iVehicleNum])->m_pVehicle->m_pVehicleInfo
				&& (&g_entities[potentialSpamVictim->client->ps.m_iVehicleNum])->m_pVehicle->m_pVehicleInfo->type && ((&g_entities[potentialSpamVictim->client->ps.m_iVehicleNum])->m_pVehicle->m_pVehicleInfo->type == VH_WALKER || (&g_entities[potentialSpamVictim->client->ps.m_iVehicleNum])->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER))
			{
				if (level.siegeMap == SIEGEMAP_HOTH) {
					if (level.totalObjectivesCompleted <= 2) { //allow walker to trigger "allow spam" for first 3 objs
						thereIsAWalkerOrProtector = qtrue;
						continue;
					}
					if (level.totalObjectivesCompleted >= 4) //don't allow walker to trigger "allow spam" for later objs
						continue;
					if (level.totalObjectivesCompleted == 3) //allow walker to trigger "allow spam" if you're near it i guess
					{
						if (ent->client->ps.origin[0] >= -917 && ent->client->ps.origin[0] <= 8219 &&
							ent->client->ps.origin[1] >= -3729 && ent->client->ps.origin[1] <= 1635) {
							thereIsAWalkerOrProtector = qtrue;
						}
						continue;
					}
				}
				else
					thereIsAWalkerOrProtector = qtrue;//it's okay to spam if there's a walker or fighter nearby (regardless of angle)
				continue;
			}

			/*if (potentialSpamVictim && potentialSpamVictim->classname && !Q_stricmp(potentialSpamVictim->classname, "item_shield"))
			{
				continue; //it's okay to spam if there's a shield nearby (regardless of angle)
			}*/

			if (!potentialSpamVictim->client)
			{
				continue;
			}

			if (potentialSpamVictim->client->sess.sessionTeam && potentialSpamVictim->client->sess.sessionTeam != TEAM_RED)
			{
				continue; //spam victim must be on offense
			}

			if (ent->client->ps.origin[2] < 470 && potentialSpamVictim->client->ps.origin[2] >= 470 && level.siegeMap == SIEGEMAP_HOTH) {
				continue; // fix for attackers in cc/short area triggering anti spam for defenders down below
			}

			VectorSubtract(throwerOrigin, potentialSpamVictim->client->ps.origin, distanceToVictim); //DUOFIXME: add more graduated angles for extreme distances (e.g very small angle for long-range rocket use)
			if (VectorLength(distanceToVictim) < 600)
			{
				sizeOfConeOfProhibitedSpam = 70;//reduced from 75
			}
			else if (VectorLength(distanceToVictim) >= 600 && VectorLength(distanceToVictim) < 900)
			{
				sizeOfConeOfProhibitedSpam = 50; //reduced from 60. restrictive cone shouldn't be too draconian if we are rather far away...ease up on the restriction a little
				//trap_SendServerCommand(-1, va("print \"Debug: using reduced cone size of %i\n\"", sizeOfConeOfProhibitedSpam));
			}
			else if (VectorLength(distanceToVictim) >= 900 && VectorLength(distanceToVictim) < 1800)
			{
				sizeOfConeOfProhibitedSpam = 35; //reduced from 45. restrictive cone shouldn't be too draconian if we are rather far away...ease up on the restriction a little
				//trap_SendServerCommand(-1, va("print \"Debug: using reduced cone size of %i\n\"", sizeOfConeOfProhibitedSpam));
			}
			else
			{
				sizeOfConeOfProhibitedSpam = 20; //reduced from 30. restrictive cone shouldn't be too draconian if we are rather far away...ease up on the restriction a little
				//trap_SendServerCommand(-1, va("print \"Debug: using reduced cone size of %i\n\"", sizeOfConeOfProhibitedSpam));
			}

			if (!InFOV(potentialSpamVictim, ent, sizeOfConeOfProhibitedSpam, 170))
			{
				continue; //make sure the potential spam victim is within our vision cone
				//we'll use a large number for vertical FOV check here, then just check height using ps.origin[2] later
			}

			if (potentialSpamVictim->client->ps.origin[2] < heightLowerBound || potentialSpamVictim->client->ps.origin[2] > heightUpperBound)
			{
				continue; //don't prevent people from "spamming" if the enemy in question is too far away vertically.
			}

			if (potentialSpamVictim == ent || !potentialSpamVictim->takedamage || potentialSpamVictim->health <= 0 || potentialSpamVictim->client->tempSpectate >= level.time || potentialSpamVictim->flags & FL_NOTARGET || potentialSpamVictim->s.eType == ET_NPC || potentialSpamVictim->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
			{
				continue; //miscellaneous checks
			}

			if (BotMindTricked(ent->s.number, potentialSpamVictim->s.number)) //don't count people who have us mind tricked. this would be a dead giveaway that the MTer is nearby.
			{
				continue; //pretend like MTer is not there
			}

			if (potentialSpamVictim->client->ps.fd.forcePowersActive && potentialSpamVictim->client->ps.fd.forcePowersActive & (1 << FP_PROTECT))
			{
				thereIsAWalkerOrProtector = qtrue;//it's okay to spam if the enemy is using protect (I guess)
				continue;
			}

			//if we got to this line, there is at least one eligible enemy. we still have some more stuff to check, though...

			if (checkDoorspam) //we're checking for door spam and we have at least one possible enemy. let's make he's behind a door, though.
			{
				if (!thereIsADoor)
				{
					iAmADirtyFuckingSpammer = qfalse; //we are checking for doorspam and there is no door; therefore, we are not doorspamming.
				}
				else //there is a door, so let's check if an enemy is behind it
				{
					VectorSubtract(start, potentialSpamVictim->client->ps.origin, distanceBetweenMeAndVictim);
					for (x = 1; x <= numberOfDoorsFound; x++)
					{
						if (VectorLength(distanceBetweenMeAndVictim) > VectorLength(distanceToDoor[x]))
						{
							//enemy is behind door
							//trap_SendServerCommand(-1, va("print \"Debug: distanceBetweenMeAndVictim %f ^2GREATER THAN^7 distanceBetweenMeAndDoor %f\n\"", VectorLength(distanceBetweenMeAndVictim), VectorLength(distanceBetweenMeAndDoor)));
							VectorSubtract(distanceToDoor[x], distanceBetweenMeAndVictim, distanceBetweenDoorAndVictim);
							if (VectorLength(distanceBetweenDoorAndVictim) < doorspamDistanceCheck)
							{
								//enemy is behind door, and is rather close to the door. this is doorspam.
								//trap_SendServerCommand(-1, va("print \"Debug: distanceBetweenDoorAndVictim %f ^2LESS THAN^7 512\n\"", VectorLength(distanceBetweenDoorAndVictim)));
								//let's check that we can't see him first before we say it's doorspam
								if (!G_ClientCanBeSeenByClient(potentialSpamVictim, ent))
								{
									//trap_SendServerCommand(-1, va("print \"Debug: Client ^1cannot^7 be seen, so it's doorspam\n\""));
									thereIsAnEnemyBehindDoor = qtrue;
								}
								//else
								//{
									//trap_SendServerCommand(-1, va("print \"Debug: Client ^2can^7 be seen, so it's not doorspam\n\""));
								//}
							}
							else
							{
								//enemy is behind door, but is rather far from the door. not doorspam.
								//trap_SendServerCommand(-1, va("print \"Debug: distanceBetweenDoorAndVictim %f ^1GREATER THAN^7 512\n\"", VectorLength(distanceBetweenDoorAndVictim)));
							}
						}
						else
						{
							//trap_SendServerCommand(-1, va("print \"Debug: distanceBetweenMeAndVictim %f ^1LESS THAN^7 distanceBetweenMeAndDoor %f\n\"", VectorLength(distanceBetweenMeAndVictim), VectorLength(distanceBetweenMeAndDoor)));
							if (level.siegeMap == SIEGEMAP_HOTH && ent->client->ps.origin[0] >= -3825 && ent->client->ps.origin[0] <= -1258 &&
								ent->client->ps.origin[1] >= -570 && ent->client->ps.origin[1] <= 755 && ent->client->ps.origin[2] >= -295 && ent->client->ps.origin[2] <= 38)
							{
								//we are in the hangar on hoth
								possibleEnemiesInStation1 = G_RadiusList(throwerOrigin, 9999, ent, qtrue, enemyInStation1List);

								for (n = 0; n < possibleEnemiesInStation1; n++)
								{
									potentialEnemyInStation1 = enemyInStation1List[n];

									if (!potentialEnemyInStation1 || !potentialEnemyInStation1->client)
									{
										continue; //??? uhh...this should never happen, but whatever
									}

									if (potentialEnemyInStation1->client->sess.sessionTeam && potentialEnemyInStation1->client->sess.sessionTeam != TEAM_RED)
									{
										continue; //must be on offense
									}

									if (potentialEnemyInStation1 == ent || !potentialEnemyInStation1->takedamage || potentialEnemyInStation1->health <= 0 || potentialEnemyInStation1->client->tempSpectate >= level.time || potentialEnemyInStation1->flags & FL_NOTARGET || potentialEnemyInStation1->s.eType == ET_NPC || potentialEnemyInStation1->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
									{
										continue; //miscellaneous checks
									}

									if (potentialEnemyInStation1->client->ps.origin[0] >= -3825 && potentialEnemyInStation1->client->ps.origin[0] <= -1258 &&
										potentialEnemyInStation1->client->ps.origin[1] >= -570 && potentialEnemyInStation1->client->ps.origin[1] <= 755 &&
										potentialEnemyInStation1->client->ps.origin[2] >= -295 && potentialEnemyInStation1->client->ps.origin[2] <= 38)
									{
										//there is an enemy in the hangar
										numConfirmedEnemiesInStation1++;
									}
								}
								if (numConfirmedEnemiesInStation1)
								{
									//there are enemies in the hangar, so "doorspamming" is okay
									//trap_SendServerCommand(-1, va("print \"^%iEnemies in hangar, ^%ispam allowed.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
									thereIsAnEnemyBetweenMeAndDoor = qtrue; //enemy is not behind the door
								}
								else
								{
									//trap_SendServerCommand(-1, va("print \"^%iNo enemies detected in hangar, ^%iproceeding normally.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
									thereIsAnEnemyBetweenMeAndDoor = qfalse; //enemy is behind the door
								}
							}
							else
							{
								thereIsAnEnemyBetweenMeAndDoor = qtrue; //enemy is not behind the door
							}
						}
					}
				}
			}
			else
			{
				iAmADirtyFuckingSpammer = qtrue; //there is at least one eligible enemy and we aren't checking for doorspam
			}
		}

		//if (thereIsAnEnemyBehindDoor)
			//trap_SendServerCommand(-1, va("print \"There is at least 1 enemy behind the %i doors found\n\"", numberOfDoorsFound));
		//if (thereIsAnEnemyBetweenMeAndDoor)
			//trap_SendServerCommand(-1, va("print \"There is at least 1 enemy between you and the %i doors found\n\"", numberOfDoorsFound));


#if 0
	//ALTERNATE METHOD
	//don't check for enemies -- just never allow shooting at doors ever (unless someone is in front of us or there's a walker)
		if (checkDoorspam && thereIsADoor && !thereIsAnEnemyBetweenMeAndDoor && !thereIsAWalkerOrProtector)
		{
			if (removeMissileIfIAmASpammer && missile)
				G_FreeEntity(missile); //optionally delete the missile from the game
			return qtrue;
	}
#endif


		if ((checkDoorspam && thereIsADoor && thereIsAnEnemyBehindDoor && !thereIsAnEnemyBetweenMeAndDoor && !thereIsAWalkerOrProtector && !overrideDefinitelyNotSpam) || (!checkDoorspam && iAmADirtyFuckingSpammer && !thereIsAWalkerOrProtector && !overrideDefinitelyNotSpam))
		{
			//we are a spammer confirmed. return qtrue so we can punish this douchebag
			if (removeMissileIfIAmASpammer && missile)
				G_FreeEntity(missile); //optionally delete the missile from the game
			return qtrue;
		}
		else
		{
			return qfalse;
		}


		return qfalse; //?
	}



	else if (ent->client->sess.sessionTeam == TEAM_RED)
	{
		if (weaponBeingUsed != WP_TRIP_MINE)
		{
			return qfalse; //only affecting some weapons for now
		}

		if (level.siegeMap != SIEGEMAP_NAR)
		{
			return qfalse; //only on nar
		}

		if (!(ent->client->ps.origin[0] >= -912 && ent->client->ps.origin[0] <= -263 && ent->client->ps.origin[1] >= 7983 && ent->client->ps.origin[1] <= 8607))
		{
			return qfalse;
		}
		//trap_SendServerCommand(-1, va("print \"^%iPassed weapon, map, ^%iand origin check.\n\"", Q_irand(1, 7), Q_irand(1, 7)));

		//if we got to this point, we are on offense, using mines, and in the little area outside d spawn at 3rd objective of nar.
		//now let's check for enemies.

		memset(&tr, 0, sizeof(tr)); //to shut the compiler up
		VectorCopy(ent->client->ps.origin, start);
		start[2] += ent->client->ps.viewheight;//By eyes

		//start[0] += debug_testHeight1.integer;
		//start[1] += debug_testHeight2.integer;
		//start[2] += debug_testHeight3.integer;

		VectorMA(start, range, forward, end);
		ignore = ent->s.number;
		originalend0 = end[0];
		originalend1 = end[1];
		originalend2 = end[2];
		//end[0] += debug_testHeight4.integer;
		//end[1] += debug_testHeight5.integer;
		//end[2] += debug_testHeight6.integer;

		//trap_SendServerCommand(-1, va("print \"start: %f, %f, %f   end: %f, %f, %f\n\"", start[0], start[1], start[2], end[0], end[1], end[2]));

		for (yAdjustment = 512; yAdjustment >= -512; yAdjustment -= 512)
		{
			end[0] = originalend0;
			end[0] += yAdjustment;
			for (xAdjustment = 512; xAdjustment >= -512; xAdjustment -= 512)
			{
				end[1] = originalend1;
				end[1] += xAdjustment;
				for (heightAdjustment = 2048; heightAdjustment >= -4096; heightAdjustment -= 256) //check many different heights slightly above or below.
				{
					end[2] = originalend2;
					end[2] += heightAdjustment;
					//G_PlayEffectID(G_EffectIndex("disruptor/wall_impact.efx"), end, start);
					trap_G2Trace(&tr, start, NULL, NULL, end, ignore, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
					traceEnt = &g_entities[tr.entityNum];
					//if (traceEnt->classname)
					//trap_SendServerCommand(-1, va("print \"^%iDebug: classname == %s\n\"", Q_irand(1, 7), traceEnt->classname));
					if (traceEnt && tr.entityNum < ENTITYNUM_WORLD && traceEnt->classname && !Q_stricmp(traceEnt->classname, "item_shield"))
					{
						//we are aiming at a shield
						//trap_SendServerCommand(-1, va("print \"We are aiming at a shield, returning qfalse\n\""));
						return qfalse;
					}
					else if (traceEnt && tr.entityNum < ENTITYNUM_WORLD && traceEnt->classname && !Q_stricmp(traceEnt->classname, "func_breakable"))
					{
						//we are aiming at a door lock
						//trap_SendServerCommand(-1, va("print \"We are aiming at a func_breakable (door lock), returning qfalse\n\""));
						return qfalse;
					}
				}
			}
		}

		//if we got to this point, we are not aiming at a shield.
		//now let's check for enemies.

		VectorCopy(ent->client->ps.origin, throwerOrigin);
		//possibleTargets = G_RadiusList(throwerOrigin, 9999, ent, qtrue, entity_list);
		possibleTargets = G_RadiusList(throwerOrigin, range, ent, qtrue, entity_list);

		for (i = 0; i < possibleTargets; i++)
		{
			potentialSpamVictim = entity_list[i];

			if (!potentialSpamVictim)
			{
				continue; //??? uhh...this should never happen, but whatever
			}

			if (!potentialSpamVictim->client)
			{
				continue;
			}

			if (potentialSpamVictim->client->sess.sessionTeam && potentialSpamVictim->client->sess.sessionTeam != TEAM_BLUE)
			{
				continue; //spam victim must be on defense
			}

			if (!InFOV(potentialSpamVictim, ent, 60, 170))
			{
				//trap_SendServerCommand(-1, va("print \"^%iEnemy is not^%i within vision cone, continuing.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
				continue; //make sure the potential spam victim is within our vision cone
			}

			if (!(potentialSpamVictim->client->ps.origin[0] >= -1287 && potentialSpamVictim->client->ps.origin[0] <= -545 && potentialSpamVictim->client->ps.origin[1] >= 8257 && potentialSpamVictim->client->ps.origin[1] <= 8709))
			{
				//trap_SendServerCommand(-1, va("print \"^%iEnemy is not^%i within origin constraints, continuing.\n\"", Q_irand(1, 7), Q_irand(1, 7)));
				continue;
			}

			if (potentialSpamVictim == ent || !potentialSpamVictim->takedamage || potentialSpamVictim->health <= 0 || potentialSpamVictim->client->tempSpectate >= level.time || potentialSpamVictim->flags & FL_NOTARGET || potentialSpamVictim->s.eType == ET_NPC || potentialSpamVictim->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
			{
				continue; //miscellaneous checks
			}

			thereIsAnEnemyBetweenMeAndDoor = qtrue;
		}

		if (thereIsAnEnemyBetweenMeAndDoor && level.time <= level.antiSpawnSpamTime)
		{
			//trap_SendServerCommand(-1, va("print \"^%iSpam ^%iconfirmed, ^2returning qtrue\n\"", Q_irand(1, 7), Q_irand(1, 7)));
			//we are a spammer confirmed. return qtrue so we can punish this douchebag
			if (removeMissileIfIAmASpammer && missile)
				G_FreeEntity(missile); //optionally delete the missile from the game
			return qtrue;
		}
		//trap_SendServerCommand(-1, va("print \"^%iSpam ^%idisproven, ^1returning qfalse\n\"", Q_irand(1, 7), Q_irand(1, 7)));
		return qfalse;
	}



	return qfalse; //?
}

qboolean G_AntiSpamShadow_Verdict(const antiSpamShot_t *shot, qboolean newVerdict) {
	qboolean	legacyVerdict;
	const char	*result;

	if (!g_antiSpamShadow.integer)
		return newVerdict;

	legacyFoundNeverReturnDoor = qfalse;
	legacyVerdict = Legacy_CheckIfIAmAFilthySpammer(shot->ent, shot->kind == ANTISPAM_DOOR, qfalse, NULL, shot->weapon,
		shot->altFire, qfalse, shot->range, qfalse, shot->forward);

	// never-return doors no longer count as doors, so that difference is intended
	if (legacyVerdict == newVerdict)
		result = "^2match";
	else if (legacyVerdict && !newVerdict && legacyFoundNeverReturnDoor)
		result = "^3expected difference, never-return door";
	else
		result = "^1MISMATCH";

	if (shot->ent && shot->ent->client) {
		trap_SendServerCommand(shot->ent - g_entities, va("print \"antispam shadow: legacy %s^7, new %s^7 (%s^7)\n\"",
			legacyVerdict ? "^1blocked" : "^2allowed", newVerdict ? "^1blocked" : "^2allowed", result));
	}

	return g_antiSpamShadow.integer == 1 ? legacyVerdict : newVerdict;
}

/*
==================
antispamfuzz <shots>

server console test, needs g_antiSpamDebug and some bots on both teams
scatters the players around doors and the map specific spots, shuffles the objective state, and compares
the two verdicts for a random shot each time; everything it touches is put back afterwards
==================
*/

#define FUZZ_COUNT(a)	(sizeof(a) / sizeof((a)[0]))

typedef struct {
	siegeMap_t	map;
	float		x, y, zMin, zMax;
} fuzzSpot_t;

static const fuzzSpot_t fuzzSpots[] = {
	{ SIEGEMAP_CARGO, 2550, 2570, -300, 600 },		// 2nd obj
	{ SIEGEMAP_CARGO, 6980, 385, -300, 600 },		// station 1
	{ SIEGEMAP_CARGO, 7250, -950, -300, 600 },		// station 2
	{ SIEGEMAP_CARGO, 6650, -1100, -300, 600 },		// vent shield room
	{ SIEGEMAP_CARGO, 6320, 370, -300, 600 },		// hallway near cc
	{ SIEGEMAP_HOTH, 7370, -300, -400, 700 },		// first walker spawn
	{ SIEGEMAP_HOTH, 3200, -270, -400, 700 },		// second walker spawn
	{ SIEGEMAP_HOTH, -1290, -40, 100, 260 },		// top of infirmary lift
	{ SIEGEMAP_HOTH, -2224, -321, 150, 560 },		// short lift
	{ SIEGEMAP_HOTH, -2540, 90, -295, 500 },		// hangar
	{ SIEGEMAP_NAR, -1320, 7380, -400, 600 },		// station 1 obj room
	{ SIEGEMAP_NAR, -1320, 7900, -400, 600 },		// station 1
	{ SIEGEMAP_NAR, -590, 8290, -400, 600 },		// outside d spawn
	{ SIEGEMAP_NAR, -900, 8480, -400, 600 },		// d spawn
};

typedef struct {
	vec3_t	origin, currentOrigin, viewangles, eyeAngles, eyePoint;
	int		forcePowersActive;
} fuzzSavedClient_t;

static void Fuzz_MoveClient(gentity_t *ent, const vec3_t origin, const vec3_t angles) {
	VectorCopy(origin, ent->client->ps.origin);
	VectorCopy(origin, ent->r.currentOrigin);
	VectorCopy(angles, ent->client->ps.viewangles);
	VectorCopy(angles, ent->client->renderInfo.eyeAngles);
	VectorCopy(origin, ent->client->renderInfo.eyePoint);
	ent->client->renderInfo.eyePoint[2] += ent->client->ps.viewheight;
	trap_LinkEntity(ent);
}

static void Fuzz_PointNear(const vec3_t anchor, float spread, float heightSpread, vec3_t out) {
	out[0] = anchor[0] + flrand(-spread, spread);
	out[1] = anchor[1] + flrand(-spread, spread);
	out[2] = anchor[2] + flrand(-heightSpread, heightSpread);
}

void G_AntiSpamShadow_Fuzz(void) {
	static const struct { antiSpamKind_t kind; weapon_t weapon; qboolean altFire; float range; } shots[] = {
		{ ANTISPAM_DOOR, WP_BOWCASTER, qtrue, SPAM_DISTANCE_BOWCASTER },
		{ ANTISPAM_DOOR, WP_REPEATER, qtrue, SPAM_DISTANCE_BLOB },
		{ ANTISPAM_DOOR, WP_FLECHETTE, qtrue, SPAM_DISTANCE_GOLAN },
		{ ANTISPAM_DOOR, WP_ROCKET_LAUNCHER, qfalse, SPAM_DISTANCE_ROCKET },
		{ ANTISPAM_DOOR, WP_THERMAL, qfalse, SPAM_DISTANCE_THERMAL },
		{ ANTISPAM_DOOR, WP_CONCUSSION, qfalse, SPAM_DISTANCE_CONC },
		{ ANTISPAM_MINE, WP_TRIP_MINE, qtrue, SPAM_DISTANCE_MINES },
		{ ANTISPAM_MINE, WP_TRIP_MINE, qfalse, SPAM_DISTANCE_PRIMARY_MINES },
	};
	fuzzSavedClient_t	saved[MAX_CLIENTS];
	int					players[MAX_CLIENTS], doors[MAX_GENTITIES];
	gentity_t			*walker = NULL;
	vec3_t				walkerOrigin;
	int					numPlayers = 0, numDoors = 0, numSpots = 0;
	int					numShots, numMismatched = 0, numExpected = 0, numSpam = 0;
	int					savedObjs = level.totalObjectivesCompleted, savedJustCompleted = level.objectiveJustCompleted;
	int					savedSpawnSpamTime = level.antiSpawnSpamTime;
	qboolean			savedCc = level.ccCompleted;
	clock_t				legacyClock = 0, newClock = 0, start;
	const char			*reasons[32];
	int					reasonCounts[32] = { 0 }, numReasons = 0;
	char				arg[16];
	int					i, shotNum;

	trap_Argv(1, arg, sizeof(arg));
	numShots = atoi(arg);
	if (!g_antiSpamDebug.integer || numShots <= 0) {
		G_Printf("usage: antispamfuzz <shots> (needs g_antiSpamDebug)\n");
		return;
	}

	for (i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];

		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || ent->health <= 0)
			continue;
		if (ent->client->sess.sessionTeam != TEAM_RED && ent->client->sess.sessionTeam != TEAM_BLUE)
			continue;

		VectorCopy(ent->client->ps.origin, saved[i].origin);
		VectorCopy(ent->r.currentOrigin, saved[i].currentOrigin);
		VectorCopy(ent->client->ps.viewangles, saved[i].viewangles);
		VectorCopy(ent->client->renderInfo.eyeAngles, saved[i].eyeAngles);
		VectorCopy(ent->client->renderInfo.eyePoint, saved[i].eyePoint);
		saved[i].forcePowersActive = ent->client->ps.fd.forcePowersActive;
		players[numPlayers++] = i;
	}

	for (i = MAX_CLIENTS; i < level.num_entities; i++) {
		if (g_entities[i].inuse && g_entities[i].classname && !Q_stricmp(g_entities[i].classname, "func_door"))
			doors[numDoors++] = i;
		if (g_entities[i].inuse && g_entities[i].s.eType == ET_NPC && g_entities[i].m_pVehicle && g_entities[i].m_pVehicle->m_pVehicleInfo->type == VH_WALKER)
			walker = &g_entities[i];
	}

	for (i = 0; i < (int)FUZZ_COUNT(fuzzSpots); i++) {
		if (fuzzSpots[i].map == level.siegeMap)
			numSpots++;
	}

	if (walker)
		VectorCopy(walker->r.currentOrigin, walkerOrigin);

	if (numPlayers < 2 || !numDoors) {
		G_Printf("antispamfuzz: need at least two living players and a door\n");
		return;
	}

	for (shotNum = 0; shotNum < numShots; shotNum++) {
		antiSpamShot_t	shot;
		gentity_t		*shooter = &g_entities[players[Q_irand(0, numPlayers - 1)]];
		gentity_t		*door = NULL;
		moverState_t	savedDoorState = MOVER_POS1;
		vec3_t			anchor, side, origin, angles, toAnchor;
		const char		*reason = "";
		qboolean		legacyVerdict, newVerdict;
		int				pick = Q_irand(0, (int)FUZZ_COUNT(shots) - 1);

		// only defense is policed, apart from mines thrown at the defense spawn on nar
		qboolean narOffense = (level.siegeMap == SIEGEMAP_NAR && Q_irand(0, 3) == 0);

		for (i = 0; i < 100 && shooter->client->sess.sessionTeam != (narOffense ? TEAM_RED : TEAM_BLUE); i++)
			shooter = &g_entities[players[Q_irand(0, numPlayers - 1)]];
		if (narOffense)
			pick = (int)FUZZ_COUNT(shots) - 1 - Q_irand(0, 1);

		level.totalObjectivesCompleted = Q_irand(0, 9) ? Q_irand(1, 6) : 0;
		level.objectiveJustCompleted = Q_irand(0, 6);
		level.ccCompleted = Q_irand(0, 1);
		level.antiSpawnSpamTime = level.time + Q_irand(-3000, 3000);

		// anchor the scene on a door or on one of this map's spots
		if (narOffense) {
			VectorSet(anchor, -900, 8480, shooter->client->ps.origin[2]);
			VectorSet(origin, -590 + flrand(-350, 350), 8290 + flrand(-330, 330), anchor[2]);
			VectorSubtract(origin, anchor, side);
			VectorNormalize(side);
		}
		else if (numSpots && Q_irand(0, 2) == 0) {
			int want = Q_irand(0, numSpots - 1);

			for (i = 0; i < (int)FUZZ_COUNT(fuzzSpots); i++) {
				if (fuzzSpots[i].map == level.siegeMap && !want--)
					break;
			}
			VectorSet(anchor, fuzzSpots[i].x, fuzzSpots[i].y, flrand(fuzzSpots[i].zMin, fuzzSpots[i].zMax));
			VectorSet(side, 1, 0, 0);
			Fuzz_PointNear(anchor, 250, 0, origin);
		}
		else {
			trace_t tr;
			int		tries;

			door = &g_entities[doors[Q_irand(0, numDoors - 1)]];
			VectorAdd(door->r.absmin, door->r.absmax, anchor);
			VectorScale(anchor, 0.5f, anchor);
			savedDoorState = door->moverState;
			if (Q_irand(0, 5))
				door->moverState = MOVER_POS1;

			// stand somewhere with a clear line to the door
			for (tries = 0; tries < 8; tries++) {
				float yaw = flrand(0, 2 * M_PI);

				VectorSet(side, cos(yaw), sin(yaw), 0);
				VectorMA(anchor, flrand(80, 1400), side, origin);
				origin[2] = door->r.absmin[2] + 25 + (Q_irand(0, 3) ? 0 : flrand(-150, 300));
				trap_Trace(&tr, anchor, NULL, NULL, origin, door->s.number, MASK_SOLID);
				if (tr.fraction == 1.0f)
					break;
			}
		}

		// most of the others go behind the anchor, some in front of it, a few are protected
		for (i = 0; i < numPlayers; i++) {
			gentity_t	*other = &g_entities[players[i]];
			vec3_t		otherOrigin;

			if (other == shooter || Q_irand(0, 3) == 0)
				continue;

			VectorMA(anchor, Q_irand(0, 3) ? -flrand(40, 700) : flrand(40, 700), side, otherOrigin);
			otherOrigin[0] += flrand(-150, 150);
			otherOrigin[1] += flrand(-150, 150);
			otherOrigin[2] = origin[2] + (Q_irand(0, 3) ? flrand(-30, 30) : flrand(-400, 400));
			VectorSet(angles, 0, flrand(-180, 180), 0);
			Fuzz_MoveClient(other, otherOrigin, angles);
			if (Q_irand(0, 25) == 0)
				other->client->ps.fd.forcePowersActive |= (1 << FP_PROTECT);
		}

		// now and then a walker is part of the scene
		if (walker) {
			if (Q_irand(0, 9) == 0)
				Fuzz_PointNear(anchor, 1500, 100, walker->r.currentOrigin);
			else
				VectorCopy(walkerOrigin, walker->r.currentOrigin);
			trap_LinkEntity(walker);
		}

		// the shooter mostly aims at the anchor
		VectorSubtract(anchor, origin, toAnchor);
		toAnchor[2] -= shooter->client->ps.viewheight;
		vectoangles(toAnchor, angles);
		if (Q_irand(0, 5)) {
			angles[PITCH] = AngleNormalize180(angles[PITCH] + flrand(-12, 12));
			angles[YAW] = AngleNormalize180(angles[YAW] + flrand(-25, 25));
		}
		else {
			angles[PITCH] = flrand(-89, 89);
			angles[YAW] = flrand(-180, 180);
		}
		angles[ROLL] = 0;
		Fuzz_MoveClient(shooter, origin, angles);

		shot.ent = shooter;
		shot.kind = shots[pick].kind;
		shot.weapon = shots[pick].weapon;
		shot.altFire = shots[pick].altFire;
		shot.range = shots[pick].range;
		AngleVectors(angles, shot.forward, NULL, NULL);

		legacyFoundNeverReturnDoor = qfalse;
		start = clock();
		legacyVerdict = Legacy_CheckIfIAmAFilthySpammer(shooter, shot.kind == ANTISPAM_DOOR, qfalse, NULL, shot.weapon,
			shot.altFire, qfalse, shot.range, qfalse, shot.forward);
		legacyClock += clock() - start;

		start = clock();
		newVerdict = G_AntiSpam_IsSpam(&shot, &reason);
		newClock += clock() - start;

		for (i = 0; i < numReasons && reasons[i] != reason; i++)
			;
		if (i == numReasons && numReasons < (int)FUZZ_COUNT(reasons))
			reasons[numReasons++] = reason;
		if (i < (int)FUZZ_COUNT(reasons))
			reasonCounts[i]++;

		numSpam += legacyVerdict;

		if (legacyVerdict != newVerdict) {
			if (legacyVerdict && !newVerdict && legacyFoundNeverReturnDoor) {
				numExpected++;
			}
			else if (++numMismatched <= 20) {
				G_Printf("antispamfuzz MISMATCH: team %d kind %d weapon %d objs %d origin %.1f %.1f %.1f angles %.1f %.1f legacy %d new %d\n",
					shooter->client->sess.sessionTeam, shot.kind, shot.weapon, level.totalObjectivesCompleted,
					origin[0], origin[1], origin[2], angles[PITCH], angles[YAW], legacyVerdict, newVerdict);
			}
		}

		if (door)
			door->moverState = savedDoorState;
		for (i = 0; i < numPlayers; i++)
			g_entities[players[i]].client->ps.fd.forcePowersActive = saved[players[i]].forcePowersActive;
	}

	for (i = 0; i < numPlayers; i++) {
		gentity_t *ent = &g_entities[players[i]];
		fuzzSavedClient_t *s = &saved[players[i]];

		Fuzz_MoveClient(ent, s->origin, s->viewangles);
		VectorCopy(s->currentOrigin, ent->r.currentOrigin);
		VectorCopy(s->eyeAngles, ent->client->renderInfo.eyeAngles);
		VectorCopy(s->eyePoint, ent->client->renderInfo.eyePoint);
		ent->client->ps.fd.forcePowersActive = s->forcePowersActive;
		trap_LinkEntity(ent);
	}

	if (walker) {
		VectorCopy(walkerOrigin, walker->r.currentOrigin);
		trap_LinkEntity(walker);
	}

	level.totalObjectivesCompleted = savedObjs;
	level.objectiveJustCompleted = savedJustCompleted;
	level.ccCompleted = savedCc;
	level.antiSpawnSpamTime = savedSpawnSpamTime;

	for (i = 0; i < numReasons; i++)
		G_Printf("antispamfuzz: %6d  %s\n", reasonCounts[i], reasons[i]);

	G_Printf("antispamfuzz: map %s, %d players, %d doors, %d shots, %d legacy spam verdicts, %d mismatched, %d expected (never-return door), legacy %.0fms, new %.0fms\n",
		level.mapname, numPlayers, numDoors, numShots, numSpam, numMismatched, numExpected,
		legacyClock * 1000.0 / CLOCKS_PER_SEC, newClock * 1000.0 / CLOCKS_PER_SEC);
}
