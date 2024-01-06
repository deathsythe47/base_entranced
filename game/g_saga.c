// Copyright (C) 2000-2002 Raven Software, Inc.
//
/*****************************************************************************
 * name:		g_saga.c
 *
 * desc:		Game-side module for Siege gametype.
 *
 * $Author: Rich Whitehouse $ 
 * $Revision: 1.6 $
 *
 *****************************************************************************/
#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

#define SIEGEITEM_STARTOFFRADAR 8


static char		team1[512];
static char		team2[512];

siegePers_t	g_siegePersistant = {qfalse, 0, 0};

int			imperial_goals_required = 0;
int			imperial_goals_completed = 0;
int			rebel_goals_required = 0;
int			rebel_goals_completed = 0;

int			imperial_time_limit = 0;
int			rebel_time_limit = 0;

int			gImperialCountdown = 0;
int			gRebelCountdown = 0;

int			rebel_attackers = 0;
int			imperial_attackers = 0;

int			winningteam = 0;

qboolean	gSiegeRoundBegun = qfalse;
qboolean	gSiegeRoundEnded = qfalse;
qboolean	gSiegeRoundWinningTeam = 0;
int			gSiegeBeginTime = Q3_INFINITE;

int			g_preroundState = 0; //default to starting as spec (1 is starting ingame)

void LogExit( const char *string );
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);

static char gParseObjectives[MAX_SIEGE_INFO_SIZE];
static char gObjectiveCfgStr[1024];

#ifdef NEWMOD_SUPPORT
#define SIEGETIMER_FAKEOWNER 1023
void UpdateNewmodSiegeTimers(void) {
	if (!g_siegeRespawn.integer || g_siegeRespawn.integer == 1)
		return;
	gentity_t *te = G_TempEntity(vec3_origin, EV_SIEGESPEC);
	te->s.time = level.siegeRespawnCheck;
	te->s.owner = SIEGETIMER_FAKEOWNER;
	te->s.saberInFlight = qtrue;
	te->r.svFlags |= SVF_BROADCAST;
}
#endif

int CurrentSiegeRound(void) {
	if (!g_siegeTeamSwitch.integer)
		return 1;
	if (g_siegePersistant.beatingTime)
		return 2;
	return 1;
}

// returns the first incomplete objective for the specified round
int G_FirstIncompleteObjective(int round) {
	int i;
	for (i = 1; i <= MAX_STATS - 1; i++) {
		if (!trap_Cvar_VariableIntegerValue(va("siege_r%i_obj%i", round, i)))
			return i;
	}
	return 0;
}

// returns the first complete objective for the specified round
int G_FirstCompleteObjective(int round) {
	int i;
	for (i = 1; i <= MAX_STATS - 1; i++) {
		if (trap_Cvar_VariableIntegerValue(va("siege_r%i_obj%i", round, i)))
			return i;
	}
	return 0;
}

// convert milliseconds to time string
void G_ParseMilliseconds(int ms, char *outBuf, size_t outSize) {
	if (!outBuf)
		return;

	int elapsedSeconds, minutes, seconds;

	elapsedSeconds = ((ms + 500) / 1000); //convert milliseconds to seconds
	minutes = (elapsedSeconds / 60) % 60; //find minutes
	seconds = elapsedSeconds % 60; //find seconds

	Q_strncpyz(outBuf, va("%d:%02d", minutes, seconds), outSize);
}

// gets the objective that was completed just before the specified objective or time
int G_PreviousObjective(int objective, int round, int timeOverride) {
	int i, mostRecentObj = 0, mostRecentTime = 0;
	int inputObjTime = timeOverride ? timeOverride : trap_Cvar_VariableIntegerValue(va("siege_r%i_obj%i", round, objective));
	for (i = 1; i <= MAX_STATS - 1; i++) {
		if (i == objective)
			continue;
		int time = trap_Cvar_VariableIntegerValue(va("siege_r%i_obj%i", round, i));
		if (time && time < inputObjTime && time > mostRecentTime) {
			mostRecentObj = i;
			mostRecentTime = time;
		}
	}
	return mostRecentObj;
}

// used for "objective completed in XX:XX"
int G_ObjectiveTimeDifference(int objective, int round) {
	int thisTime = trap_Cvar_VariableIntegerValue(va("siege_r%i_obj%i", round, objective));
	int prevTime = trap_Cvar_VariableIntegerValue(va("siege_r%i_obj%i", round, G_PreviousObjective(objective, round, 0)));

	return abs(thisTime - prevTime);
}

// used for "held at objective for XX:XX"
static int HeldForMaxTime(void) {
	int round = CurrentSiegeRound();
	int heldAtObj = trap_Cvar_VariableIntegerValue(va("siege_r%i_heldformaxat", round));

	if (heldAtObj == 1)
		return abs(level.time - level.siegeRoundStartTime);
	else {
		int firstComplete = G_FirstCompleteObjective(round);
		if (!firstComplete)
			return abs(level.time - level.siegeRoundStartTime);
		else
			return abs(abs(level.time - level.siegeRoundStartTime) - abs(trap_Cvar_VariableIntegerValue(va("siege_r%i_obj%i", round, G_PreviousObjective(heldAtObj, round, abs(level.time - level.siegeRoundStartTime))))));
	}
}

// print time of the objective that was either just completed or you were held for a max at
static void PrintObjStat(int objective, int heldForMax) {
	if (!g_autoStats.integer || !g_siegeTeamSwitch.integer)
		return;

	int round = CurrentSiegeRound();
	int ms = heldForMax ? HeldForMaxTime() : G_ObjectiveTimeDifference(objective, round);

	if (ms == -1)
		return;

	if (heldForMax)
		trap_Cvar_Set(va("siege_r%i_heldformaxtime", round), va("%i", ms));

	char formattedTime[8] = { 0 };
	G_ParseMilliseconds(ms, formattedTime, sizeof(formattedTime));

	G_TeamCommand(TEAM_RED, va("print \"%s "S_COLOR_CYAN"%s"S_COLOR_WHITE".\n\"", heldForMax ? "Held at objective for" : "Objective completed in", formattedTime));
	G_TeamCommand(TEAM_BLUE, va("print \"Objective held for "S_COLOR_CYAN"%s"S_COLOR_WHITE".\n\"", formattedTime));
	G_TeamCommand(TEAM_SPECTATOR, va("print \"Objective %s "S_COLOR_CYAN"%s"S_COLOR_WHITE".\n\"", heldForMax ? "held for" : "completed in", formattedTime));

	// debug message to help investigate bugged times
	if (ms < 0 || ms > level.time - level.siegeRoundStartTime) {
#if 0
		Com_Printf("PRINTOBJSTAT DEBUG: %s %s %s %s %s/%s %s %s %s %s/%s %s %s %s %s/%s %s %s %s %s/%s %s %s %s %s/%s %s %s %s %s/%s %s %s %s %s/%s %s %s\n",
			siege_r1_obj0.string, siege_r1_obj1.string, siege_r1_obj2.string,
			siege_r1_obj3.string, siege_r1_obj4.string, siege_r1_obj5.string,
			siege_r1_obj6.string, siege_r1_obj7.string, siege_r1_obj8.string,
			siege_r1_obj9.string, siege_r1_obj10.string, siege_r1_obj11.string,
			siege_r1_obj12.string, siege_r1_obj13.string, siege_r1_obj14.string,
			siege_r1_obj15.string, siege_r1_objscompleted.string,
			siege_r1_heldformaxat.string, siege_r1_heldformaxtime.string,
			siege_r2_obj0.string, siege_r2_obj1.string, siege_r2_obj2.string,
			siege_r2_obj3.string, siege_r2_obj4.string, siege_r2_obj5.string,
			siege_r2_obj6.string, siege_r2_obj7.string, siege_r2_obj8.string,
			siege_r2_obj9.string, siege_r2_obj10.string, siege_r2_obj11.string,
			siege_r2_obj12.string, siege_r2_obj13.string, siege_r2_obj14.string,
			siege_r2_obj15.string, siege_r2_objscompleted.string,
			siege_r2_heldformaxat.string, siege_r2_heldformaxtime.string);
#endif
	}
}

//go through all classes on a team and register their
//weapons and items for precaching.
void G_SiegeRegisterWeaponsAndHoldables(int team)
{
	siegeTeam_t *stm = BG_SiegeFindThemeForTeam(team);

	if (stm)
	{
		int i = 0;
		siegeClass_t *scl;
		while (i < stm->numClasses)
		{
			scl = stm->classes[i];

			if (scl)
			{
				int j = 0;
				while (j < WP_NUM_WEAPONS)
				{
					if (scl->weapons & (1 << j))
					{ //we use this weapon so register it.
						RegisterItem(BG_FindItemForWeapon(j));
					}
					j++;
				}
				j = 0;
				while (j < HI_NUM_HOLDABLE)
				{
					if (scl->invenItems & (1 << j))
					{ //we use this item so register it.
						RegisterItem(BG_FindItemForHoldable(j));
					}
					j++;
				}
			}
			i++;
		}
	}
}

//tell clients that this team won and print it on their scoreboard for intermission
//or whatever.
void SiegeSetCompleteData(int team)
{
	trap_SetConfigstring(CS_SIEGE_WINTEAM, va("%i", level.siegeMatchWinner ? OtherTeam(level.siegeMatchWinner) : team)); //duo: override the config string so that you don't erroneously see "team 2 won the match!" if both teams were held for a max
}

void InitSiegeMode(void)
{
	vmCvar_t		mapname;
	char			levelname[512];
	char			autolevelname[512];
	char			teamIcon[128];
	char			goalreq[64];
	char			teams[2048];
	char			objective[MAX_SIEGE_INFO_SIZE];
	char			objecStr[8192];
	int				len = 0;
	int				autolen = 0;
	int				i = 0;
	int				objectiveNumTeam1 = 0;
	int				objectiveNumTeam2 = 0;
	fileHandle_t	f;
	fileHandle_t	autofile;
	int				n;

	if (g_gametype.integer != GT_SIEGE)
	{
		goto failure;
	}

	if (!Q_stricmp(g_redTeam.string, "0"))
	{
		trap_Cvar_Set("g_redTeam", "none");
	}
	if (!Q_stricmp(g_blueTeam.string, "0"))
	{
		trap_Cvar_Set("g_blueTeam", "none");
	}

	level.hangarCompletedTime = 0;
	level.hangarLiftUsedByDefense = qfalse;
	level.ccCompleted = qfalse;
	level.objectiveJustCompleted = 0;
	level.totalObjectivesCompleted = 0;
	level.wallCompleted = qfalse;
	level.siegeRoundComplete = qfalse;
	memset(&level.narStationBreached, qfalse, sizeof(level.narStationBreached));
	level.killerOfLastDesertComputer = NULL;

	//reset
	SiegeSetCompleteData(0);

	//get pers data in case it existed from last level
	if (g_siegeTeamSwitch.integer)
	{
		trap_SiegePersGet(&g_siegePersistant);
		if (g_siegePersistant.beatingTime)
		{
			trap_SetConfigstring(CS_SIEGE_TIMEOVERRIDE, va("%i", g_siegePersistant.lastTime));
		}
		else
		{
			trap_SetConfigstring(CS_SIEGE_TIMEOVERRIDE, "0");
		}
	}
	else
	{ //hmm, ok, nothing.
		trap_SetConfigstring(CS_SIEGE_TIMEOVERRIDE, "0");
	}

	imperial_goals_completed = 0;
	rebel_goals_completed = 0;

	trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );

	for (n = 0; n < MAX_CLIENTS; n++)
	{
		if (&g_entities[n])
		{
			g_entities[n].forcedClass = 0;
			g_entities[n].forcedClassTime = 0;
			g_entities[n].funnyClassNumber = 0;
		}
	}

	if (g_autoKorribanFloatingItems.integer)
	{
		if (level.siegeMap == SIEGEMAP_KORRIBAN)
		{
			trap_Cvar_Set("g_floatingItems", "1");
		}
		else
		{
			trap_Cvar_Set("g_floatingItems", "0");
		}
	}

	if (g_autoKorribanSpam.integer)
	{
		if (level.siegeMap == SIEGEMAP_KORRIBAN)
		{
			trap_Cvar_Set("iLikeToDoorSpam", "1");
			trap_Cvar_Set("iLikeToMineSpam", "1");
		}
		else
		{
			trap_Cvar_Set("iLikeToDoorSpam", "0");
			trap_Cvar_Set("iLikeToMineSpam", "0");
		}
	}

	if (autocfg_map.integer)
	{
		Com_sprintf(autolevelname, sizeof(autolevelname), "mapcfgs/%s.cfg\0", mapname.string);

		if (!autolevelname[0])
		{
			G_LogPrintf("g_saga: unknown autolevelname error!\n");
			goto afterauto;
		}

		autolen = trap_FS_FOpenFile(autolevelname, &autofile, FS_READ);

		if (autofile)
		{
			trap_SendConsoleCommand(EXEC_APPEND, va("exec %s\n", autolevelname));
		}
		else
		{
			if (autocfg_unknown.integer)
			{
				Com_sprintf(autolevelname, sizeof(autolevelname), "mapcfgs/unknown.cfg\0", mapname.string);
				autolen = trap_FS_FOpenFile(autolevelname, &autofile, FS_READ);
				if (autofile)
				{
					trap_SendConsoleCommand(EXEC_APPEND, va("exec %s\n", autolevelname));
				}
				else
				{
					G_LogPrintf("Unable to find %s.\n", autolevelname);
				}
			}
			else
			{
				G_LogPrintf("Unable to find %s.\n", autolevelname);
			}
		}
	}

	afterauto:

	Com_sprintf(levelname, sizeof(levelname), "maps/%s.siege\0", mapname.string);

	if (/*!levelname ||*/ !levelname[0])
	{
		goto failure;
	}

	len = trap_FS_FOpenFile(levelname, &f, FS_READ);

	if (!f || len >= MAX_SIEGE_INFO_SIZE)
	{
		goto failure;
	}

	trap_FS_Read(siege_info, len, f);

	trap_FS_FCloseFile(f);

	siege_valid = 1;

	//See if players should be specs or ingame preround
	if (BG_SiegeGetPairedValue(siege_info, "preround_state", teams))
	{
		if (teams[0])
		{
			g_preroundState = atoi(teams);
		}
	}

	if (BG_SiegeGetValueGroup(siege_info, "Teams", teams))
	{
		BG_SiegeGetPairedValue(teams, "team1", team1);
		BG_SiegeGetPairedValue(teams, "team2", team2);
	}
	else
	{
		G_Error("Siege teams not defined");
	}

	if (BG_SiegeGetValueGroup(siege_info, team2, gParseObjectives))
	{
		if (BG_SiegeGetPairedValue(gParseObjectives, "TeamIcon", teamIcon))
		{
			trap_Cvar_Set( "team2_icon", teamIcon);
		}

		if (BG_SiegeGetPairedValue(gParseObjectives, "RequiredObjectives", goalreq))
		{
			rebel_goals_required = atoi(goalreq);
		}
		if (BG_SiegeGetPairedValue(gParseObjectives, "Timed", goalreq))
		{
			rebel_time_limit = atoi(goalreq)*1000;
			level.siegeMaximum = rebel_time_limit;
			if (g_siegeTeamSwitch.integer &&
				g_siegePersistant.beatingTime)
			{
				gRebelCountdown = level.time + g_siegePersistant.lastTime;
			}
			else
			{
				gRebelCountdown = level.time + rebel_time_limit;
			}
		}
		if (BG_SiegeGetPairedValue(gParseObjectives, "attackers", goalreq))
		{
			rebel_attackers = atoi(goalreq);
		}
	}

	if (BG_SiegeGetValueGroup(siege_info, team1, gParseObjectives))
	{

		if (BG_SiegeGetPairedValue(gParseObjectives, "TeamIcon", teamIcon))
		{
			trap_Cvar_Set( "team1_icon", teamIcon);
		}

		if (BG_SiegeGetPairedValue(gParseObjectives, "RequiredObjectives", goalreq))
		{
			imperial_goals_required = atoi(goalreq);
		}
		if (BG_SiegeGetPairedValue(gParseObjectives, "Timed", goalreq))
		{
			if (rebel_time_limit)
			{
				Com_Printf("Tried to set imperial time limit, but there's already a rebel time limit!\nOnly one team can have a time limit.\n");
			}
			else
			{
				imperial_time_limit = atoi(goalreq)*1000;
				level.siegeMaximum = imperial_time_limit;
				if (g_siegeTeamSwitch.integer &&
					g_siegePersistant.beatingTime)
				{
					gImperialCountdown = level.time + g_siegePersistant.lastTime;
				}
				else
				{
					gImperialCountdown = level.time + imperial_time_limit;
				}
			}
		}
		if (BG_SiegeGetPairedValue(gParseObjectives, "attackers", goalreq))
		{
			imperial_attackers = atoi(goalreq);
		}
	}

	//Load the player class types
	BG_SiegeLoadClasses(NULL);

	if (!bgNumSiegeClasses)
	{ //We didn't find any?!
		G_Error("Couldn't find any player classes for Siege");
	}

	//Ok, I'm adding inventory item precaching now, so I'm finally going to optimize this
	//to only do weapons/items for the current teams used on the level.

	//Now load the teams since we have class data.
	BG_SiegeLoadTeams();

	if (!bgNumSiegeTeams)
	{ //React same as with classes.
		G_Error("Couldn't find any player teams for Siege");
	}

	//Get and set the team themes for each team. This will control which classes can be
	//used on each team.
	if (BG_SiegeGetValueGroup(siege_info, team1, gParseObjectives))
	{
		if (BG_SiegeGetPairedValue(gParseObjectives, "UseTeam", goalreq))
		{
			if (g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none"))
			{
				BG_SiegeSetTeamTheme(SIEGETEAM_TEAM1, g_redTeam.string, goalreq);
			}
			else
			{
				BG_SiegeSetTeamTheme(SIEGETEAM_TEAM1, goalreq, goalreq);
			}
		}

		//Now count up the objectives for this team.
		i = 1;
		strcpy(objecStr, va("Objective%i", i));
		while (BG_SiegeGetValueGroup(gParseObjectives, objecStr, objective))
		{
			objectiveNumTeam1++;
			i++;
			strcpy(objecStr, va("Objective%i", i));
		}
	}
	if (BG_SiegeGetValueGroup(siege_info, team2, gParseObjectives))
	{
		if (BG_SiegeGetPairedValue(gParseObjectives, "UseTeam", goalreq))
		{
			if (g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none"))
			{
				BG_SiegeSetTeamTheme(SIEGETEAM_TEAM2, g_blueTeam.string, goalreq);
			}
			else
			{
				BG_SiegeSetTeamTheme(SIEGETEAM_TEAM2, goalreq, goalreq);
			}
		}

		//Now count up the objectives for this team.
		i = 1;
		strcpy(objecStr, va("Objective%i", i));
		while (BG_SiegeGetValueGroup(gParseObjectives, objecStr, objective))
		{
			objectiveNumTeam2++;
			i++;
			strcpy(objecStr, va("Objective%i", i));
		}
	}

	//Set the configstring to show status of all current objectives
	strcpy(gObjectiveCfgStr, "t1");
	while (objectiveNumTeam1 > 0)
	{ //mark them all as not completed since we just initialized
		Q_strcat(gObjectiveCfgStr, 1024, "-0");
		objectiveNumTeam1--;
	}
	//Finished doing team 1's objectives, now do team 2's
	Q_strcat(gObjectiveCfgStr, 1024, "|t2");
	while (objectiveNumTeam2 > 0)
	{
		Q_strcat(gObjectiveCfgStr, 1024, "-0");
		objectiveNumTeam2--;
	}

	//And finally set the actual config string
	trap_SetConfigstring(CS_SIEGE_OBJECTIVES, gObjectiveCfgStr);

	//precache saber data for classes that use sabers on both teams
	BG_PrecacheSabersForSiegeTeam(SIEGETEAM_TEAM1);
	BG_PrecacheSabersForSiegeTeam(SIEGETEAM_TEAM2);

	G_SiegeRegisterWeaponsAndHoldables(SIEGETEAM_TEAM1);
	G_SiegeRegisterWeaponsAndHoldables(SIEGETEAM_TEAM2);

	return;

failure:
	siege_valid = 0;
}

void G_SiegeSetObjectiveComplete(int team, int objective, qboolean failIt)
{
	char *p = NULL;
	int onObjective = 0;

	if (team == SIEGETEAM_TEAM1)
	{
		p = strstr(gObjectiveCfgStr, "t1");
	}
	else if (team == SIEGETEAM_TEAM2)
	{
		p = strstr(gObjectiveCfgStr, "t2");
	}

	if (!p)
	{
		assert(0);
		return;
	}

	//Parse from the beginning of this team's objectives until we get to the desired objective
	//number.
	while (p && *p && *p != '|')
	{
		if (*p == '-')
		{
			onObjective++;
		}

		if (onObjective == objective)
		{ //this is the one we want
			//Move to the next char, the status of this objective
			p++;

			//Now change it from '0' to '1' if we are completeing the objective
			//or vice versa if the objective has been taken away
			if (failIt)
			{
				*p = '0';
			}
			else
			{
				*p = '1';
			}
			break;
		}

		p++;
	}

	//Now re-update the configstring.
	trap_SetConfigstring(CS_SIEGE_OBJECTIVES, gObjectiveCfgStr);
}

//Returns qtrue if objective complete currently, otherwise qfalse
qboolean G_SiegeGetCompletionStatus(int team, int objective)
{
	char *p = NULL;
	int onObjective = 0;

	if (team == SIEGETEAM_TEAM1)
	{
		p = strstr(gObjectiveCfgStr, "t1");
	}
	else if (team == SIEGETEAM_TEAM2)
	{
		p = strstr(gObjectiveCfgStr, "t2");
	}

	if (!p)
	{
		assert(0);
		return qfalse;
	}

	//Parse from the beginning of this team's objectives until we get to the desired objective
	//number.
	while (p && *p && *p != '|')
	{
		if (*p == '-')
		{
			onObjective++;
		}

		if (onObjective == objective)
		{ //this is the one we want
			//Move to the next char, the status of this objective
			p++;

			//return qtrue if it's '1', qfalse if it's anything else
			if (*p == '1')
			{
				return qtrue;
			}
			else
			{
				return qfalse;
			}
			break;
		}

		p++;
	}

	return qfalse;
}

void UseSiegeTarget(gentity_t *other, gentity_t *en, char *target)
{ //actually use the player which triggered the object which triggered the siege objective to trigger the target
	gentity_t		*t;
	gentity_t		*ent;

	if ( !en || !en->client )
	{ //looks like we don't have access to a player, so just use the activating entity
		ent = other;
	}
	else
	{
		ent = en;
	}

	if (!en)
	{
		return;
	}

	if ( !target )
	{
		return;
	}

	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), target)) != NULL )
	{
		if ( t == ent )
		{
			G_Printf ("WARNING: Entity used itself.\n");
		}
		else
		{
			if ( t->use )
			{
				GlobalUse(t, ent, ent);
			}
		}
		if ( !ent->inuse )
		{
			G_Printf("entity was removed while using targets\n");
			return;
		}
	}
}

void SiegeBroadcast_OBJECTIVECOMPLETE(int team, int client, int objective)
{
	gentity_t *te;
	vec3_t nomatter;

	VectorClear(nomatter);

	te = G_TempEntity( nomatter, EV_SIEGE_OBJECTIVECOMPLETE );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = team;
	te->s.weapon = client;
	te->s.trickedentindex = objective;
}

void SiegeBroadcast_ROUNDOVER(int winningteam, int winningclient)
{
	gentity_t *te;
	vec3_t nomatter;

	VectorClear(nomatter);

	te = G_TempEntity( nomatter, EV_SIEGE_ROUNDOVER );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = winningteam;
	te->s.weapon = winningclient;
}

void BroadcastObjectiveCompletion(int team, int objective, int final, int client)
{
	if (client != ENTITYNUM_NONE && g_entities[client].client && g_entities[client].client->sess.sessionTeam == team)
	{ //guy who completed this objective gets points, providing he's on the opposing team
		AddScore(&g_entities[client], g_entities[client].client->ps.origin, g_fixSiegeScoring.integer ? SIEGE_POINTS_OBJECTIVECOMPLETED_NEW : SIEGE_POINTS_OBJECTIVECOMPLETED);
	}
	SiegeBroadcast_OBJECTIVECOMPLETE(team, client, objective);
}

void AddSiegeWinningTeamPoints(int team, int winner)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->client && ent->client->sess.sessionTeam == team)
		{
			if (i == winner)
			{
				AddScore(ent, ent->client->ps.origin, g_fixSiegeScoring.integer ? SIEGE_POINTS_TEAMWONROUND_NEW+SIEGE_POINTS_FINALOBJECTIVECOMPLETED_NEW : SIEGE_POINTS_TEAMWONROUND+SIEGE_POINTS_FINALOBJECTIVECOMPLETED);
			}
			else
			{
				AddScore(ent, ent->client->ps.origin, g_fixSiegeScoring.integer ? SIEGE_POINTS_TEAMWONROUND_NEW : SIEGE_POINTS_TEAMWONROUND);
			}
		}

		i++;
	}
}

void SiegeClearSwitchData(void)
{
	memset(&g_siegePersistant, 0, sizeof(g_siegePersistant));
	trap_SiegePersSet(&g_siegePersistant);
	level.siegeRoundStartTime = 0;
	level.antiLamingTime = 0;
}

void SiegeDoTeamAssign(void)
{
	int i = 0;
	gentity_t *ent;

	//yeah, this is great...
	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client &&
			ent->client->pers.connected == CON_CONNECTED)
		{ //a connected client, switch his frickin teams around
			if (ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1)
			{
				ent->client->sess.siegeDesiredTeam = SIEGETEAM_TEAM2;
			}
			else if (ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2)
			{
				ent->client->sess.siegeDesiredTeam = SIEGETEAM_TEAM1;
			}

			if (ent->client->sess.sessionTeam == SIEGETEAM_TEAM1)
			{
				SetTeamQuick(ent, SIEGETEAM_TEAM2, qfalse);
			}
			else if (ent->client->sess.sessionTeam == SIEGETEAM_TEAM2)
			{
				SetTeamQuick(ent, SIEGETEAM_TEAM1, qfalse);
			}
		}
		i++;
	}
}

void SiegeTeamSwitch(int winTeam, int winTime)
{
	trap_SiegePersGet(&g_siegePersistant);
	if (g_siegePersistant.beatingTime)
	{ //was already in "switched" mode, change back
		//announce the winning team.
		//either the first team won again, or the second
		//team beat the time set by the initial team. In any
		//case the winTeam here is the overall winning team.
		SiegeSetCompleteData(winTeam);
		SiegeClearSwitchData();
	}
	else
	{ //go into "beat their time" mode
		g_siegePersistant.beatingTime = qtrue;
        g_siegePersistant.lastTeam = winTeam;
		g_siegePersistant.lastTime = winTime;

		trap_SiegePersSet(&g_siegePersistant);
	}
}

static void ComputeSiegePlayTimes(void) {
	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		if (cl->pers.connected != CON_CONNECTED || (cl->sess.sessionTeam != TEAM_RED && cl->sess.sessionTeam != TEAM_BLUE))
			continue;
		if (cl->sess.sessionTeam == TEAM_RED)
			cl->sess.siegeStats.oTime[GetSiegeStatRound()] = level.time - cl->pers.enterTime;
		else
			cl->sess.siegeStats.dTime[GetSiegeStatRound()] = level.time - cl->pers.enterTime;
	}
}

static int NumPlayersOnTeam(team_t team) {
	assert(team >= TEAM_FREE && team < TEAM_NUM_TEAMS);
	int num = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		if (cl->pers.connected && GetRealTeam(cl) == team)
			num++;
	}
	return num;
}

static CaptureCategoryFlags CaptureFlagsForDefenseHold(gclient_t *client1, gclient_t *client2, CombinedObjNumber objNum) {
	if (!g_saveCaptureRecords.integer || g_gametype.integer != GT_SIEGE ||
		client1->sess.sessionTeam != TEAM_BLUE || (client2 && client2->sess.sessionTeam != TEAM_BLUE) ||
#ifndef _DEBUG
		level.mapCaptureRecords.readonly || !level.isLivePug || g_siegeRespawn.integer != level.worldspawnSiegeRespawnTime)
#else
		0)
#endif
		return 0;

	CaptureCategoryFlags flags = CAPTURERECORDFLAG_DEFENSE;
	
	if (objNum == -1)
		flags |= CAPTURERECORDFLAG_FULLMAP;
	else if (objNum >= 1 && objNum <= MAX_SAVED_OBJECTIVES)
		flags |= (1 << objNum);
	else {
		assert(qfalse);
		return 0;
	}

	int numOnBlue = NumPlayersOnTeam(TEAM_BLUE);
	if (numOnBlue == 2)
		flags |= CAPTURERECORDFLAG_2V2;
	else if (numOnBlue == 3)
		flags |= CAPTURERECORDFLAG_3V3;
	else if (numOnBlue == 4)
		flags |= CAPTURERECORDFLAG_4V4;
	else
		return 0;

	// check that nobody is boosted
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		if (!cl->pers.connected || GetRealTeam(cl) != TEAM_BLUE)
			continue;
		if (cl->sess.skillBoost || cl->sess.senseBoost || cl->sess.aimBoost)
			return 0; // don't allow defense records if someone on defense is boosted
	}

	return flags;
}

static CaptureCategoryFlags CaptureFlagsForRun(gclient_t *client1, gclient_t *client2, CombinedObjNumber objNum) {
	if (!g_saveCaptureRecords.integer || level.mapCaptureRecords.readonly || g_gametype.integer != GT_SIEGE ||
		client1->sess.sessionTeam != TEAM_RED || (client2 && client2->sess.sessionTeam != TEAM_RED))
		return 0;

	CaptureCategoryFlags flags = 0;
	int numOnRed = NumPlayersOnTeam(TEAM_RED), numOnBlue = NumPlayersOnTeam(TEAM_BLUE);
	if (!level.mapCaptureRecords.speedRunModeRuined && g_siegeRespawn.integer == 1 &&
		!client1->runInvalid && !level.siegeTopTimes[client1 - level.clients].attackedByNonTeammate &&
		!level.siegeTopTimes[client1 - level.clients].hasChangedTeams &&
		!numOnBlue && (numOnRed == 1 || numOnRed == 2)) {
		if (!client2)
			flags |= CAPTURERECORDFLAG_SPEEDRUN;
		else { // only speedruns can have co-op
			if ( !client2->runInvalid && !level.siegeTopTimes[client2 - level.clients].attackedByNonTeammate &&
				!level.siegeTopTimes[client2 - level.clients].hasChangedTeams) {
				flags |= CAPTURERECORDFLAG_SPEEDRUN | CAPTURERECORDFLAG_COOP;
			}
			else {
				if (level.isLivePug == ISLIVEPUG_YES)
					flags |= CAPTURERECORDFLAG_LIVEPUG;
				else
					flags |= CAPTURERECORDFLAG_ANYPERCENT;
			}
		}
	}
	else if (level.isLivePug == ISLIVEPUG_YES && g_siegeRespawn.integer == level.worldspawnSiegeRespawnTime)
		flags |= CAPTURERECORDFLAG_LIVEPUG;
	else
		flags |= CAPTURERECORDFLAG_ANYPERCENT;

	qboolean skillboosted = !!(client1->sess.skillBoost || client1->sess.senseBoost || client1->sess.aimBoost || (flags & CAPTURERECORDFLAG_COOP && client2 && (client2->sess.skillBoost || client2->sess.senseBoost)));
	if (skillboosted) { // skillboosted guys can only register any% runs
		flags |= CAPTURERECORDFLAG_ANYPERCENT;
		flags &= ~(CAPTURERECORDFLAG_COOP | CAPTURERECORDFLAG_LIVEPUG | CAPTURERECORDFLAG_SPEEDRUN);
	}

	if (objNum == -1)
		flags |= CAPTURERECORDFLAG_FULLMAP;
	else if (objNum >= 1 && objNum <= MAX_SAVED_OBJECTIVES)
		flags |= (1 << objNum);
	else {
		assert(qfalse);
		return 0;
	}

	if (flags & CAPTURERECORDFLAG_SPEEDRUN) { // only speedruns can have <class>-only or oneshot
		qboolean usedOnlyOneClass;

		if (flags & CAPTURERECORDFLAG_FULLMAP) {
			if (flags & CAPTURERECORDFLAG_COOP) {
				if (level.siegeTopTimes[client1 - level.clients].lastSiegeClassWhileAlive != level.siegeTopTimes[client2 - level.clients].lastSiegeClassWhileAlive)
					usedOnlyOneClass = qfalse;
				else if (level.siegeTopTimes[client1 - level.clients].hasChangedClassTotal)
					usedOnlyOneClass = qfalse;
				else if (level.siegeTopTimes[client1 - level.clients].hasChangedClass)
					usedOnlyOneClass = qfalse;
				else if (level.siegeTopTimes[client2 - level.clients].hasChangedClassTotal)
					usedOnlyOneClass = qfalse;
				else if (level.siegeTopTimes[client2 - level.clients].hasChangedClass)
					usedOnlyOneClass = qfalse;
				else
					usedOnlyOneClass = qtrue;
			}
			else {
				if (level.siegeTopTimes[client1 - level.clients].hasChangedClassTotal)
					usedOnlyOneClass = qfalse;
				else if (level.siegeTopTimes[client1 - level.clients].hasChangedClass)
					usedOnlyOneClass = qfalse;
				else
					usedOnlyOneClass = qtrue;
			}
		}
		else {
			if (flags & CAPTURERECORDFLAG_COOP) {
				if (level.siegeTopTimes[client1 - level.clients].lastSiegeClassWhileAlive != level.siegeTopTimes[client2 - level.clients].lastSiegeClassWhileAlive)
					usedOnlyOneClass = qfalse;
				else if (level.siegeTopTimes[client1 - level.clients].hasChangedClass)
					usedOnlyOneClass = qfalse;
				else if (level.siegeTopTimes[client2 - level.clients].hasChangedClass)
					usedOnlyOneClass = qfalse;
				else
					usedOnlyOneClass = qtrue;
			}
			else {
				if (level.siegeTopTimes[client1 - level.clients].hasChangedClass)
					usedOnlyOneClass = qfalse;
				else
					usedOnlyOneClass = qtrue;
			}
		}

		if (usedOnlyOneClass) {
			switch (bgSiegeClasses[level.siegeTopTimes[client1 - level.clients].lastSiegeClassWhileAlive].playerClass) {
			case SPC_INFANTRY: flags |= CAPTURERECORDFLAG_ASSAULT; break;
			case SPC_HEAVY_WEAPONS: flags |= CAPTURERECORDFLAG_HW; break;
			case SPC_DEMOLITIONIST: flags |= CAPTURERECORDFLAG_DEMO; break;
			case SPC_SUPPORT: flags |= CAPTURERECORDFLAG_TECH; break;
			case SPC_VANGUARD: flags |= CAPTURERECORDFLAG_SCOUT; break;
			case SPC_JEDI: flags |= CAPTURERECORDFLAG_JEDI; break;
			}
		}

		if (flags & CAPTURERECORDFLAG_FULLMAP &&
			!level.siegeTopTimes[client1 - level.clients].hasDied && !(flags & CAPTURERECORDFLAG_COOP && level.siegeTopTimes[client2 - level.clients].hasDied)) {
			flags |= CAPTURERECORDFLAG_ONESHOT;
		}

		if (!(flags & CAPTURERECORDFLAG_COOP))
			flags |= CAPTURERECORDFLAG_SOLO;
	}

	return flags;
}

static qboolean RecordMatchesPlayers(
	int players,
	CaptureRecord *record,
	const char *cuid1,
	const char *cuid2,
	const char *cuid3,
	const char *cuid4,
	unsigned int ipInt1,
	unsigned int ipInt2,
	unsigned int ipInt3,
	unsigned int ipInt4) {
	assert(record && players >= 1 && players <= LOGGED_PLAYERS_PER_OBJ);
	char cuids[LOGGED_PLAYERS_PER_OBJ][CRYPTO_HASH_HEX_SIZE] = { 0 };
	int numCuids = 0;
	if (VALIDSTRING(cuid1)) {
		Q_strncpyz(cuids[0], cuid1, sizeof(cuids[0]));
		numCuids++;
	}
	if (VALIDSTRING(cuid2) && players >= 2) {
		Q_strncpyz(cuids[1], cuid2, sizeof(cuids[1]));
		numCuids++;
	}
	if (VALIDSTRING(cuid3) && players >= 3) {
		Q_strncpyz(cuids[2], cuid3, sizeof(cuids[2]));
		numCuids++;
	}
	if (VALIDSTRING(cuid4) && players >= 4) {
		Q_strncpyz(cuids[3], cuid4, sizeof(cuids[3]));
		numCuids++;
	}
	unsigned int ips[LOGGED_PLAYERS_PER_OBJ] = { 0 };
	ips[0] = ipInt1;
	ips[1] = ipInt2;
	ips[2] = ipInt3;
	ips[3] = ipInt4;
	int numIps = 0, numCuidsInRecord = 0, numIpsInRecord = 0;
	for (int i = 0; i < LOGGED_PLAYERS_PER_OBJ; i++) {
		if (record->recordHolderCuids[i][0])
			numCuidsInRecord++;
		if (record->recordHolderIpInts[i])
			numIpsInRecord++;
		if (ips[i])
			numIps++;
	}

	// if every player has a cuid, and this record has exactly that many cuids, then check for cuids
	if (numCuids == players && numCuidsInRecord == players) { // check for cuid matches
		qboolean alreadyUsed[LOGGED_PLAYERS_PER_OBJ] = { qfalse };
		for (int i = 0; i < players; i++) {
			qboolean matched = qfalse;
			for (int j = 0; j < players; j++) {
				if (record->recordHolderCuids[j][0] && !alreadyUsed[j] && !Q_stricmp(cuids[i], record->recordHolderCuids[j])) {
					matched = qtrue;
					alreadyUsed[j] = qtrue;
					break;
				}
			}
			if (!matched)
				return qfalse;
		}
		return qtrue;
	}
	else if (numIps == players && numIpsInRecord == players) { // check for ip matches
		qboolean alreadyUsed[LOGGED_PLAYERS_PER_OBJ] = { qfalse };
		for (int i = 0; i < players; i++) {
			qboolean matched = qfalse;
			for (int j = 0; j < players; j++) {
				if (!alreadyUsed[j] && record->recordHolderIpInts[j] == ips[i]) {
					matched = qtrue;
					alreadyUsed[j] = qtrue;
					break;
				}
			}
			if (!matched)
				return qfalse;
		}
		return qtrue;
	}
	return qfalse;
}

static int objTimes[MAX_SAVED_OBJECTIVES] = { -1 };

// this function assumes the arrays in currentRecords are sorted by captureTime and date
// returns 0 if this is not a record, or the newly assigned rank otherwise (1 = 1st, 2 = 2nd...)
static int LogCaptureTime(
	unsigned int ipInt1,
	unsigned int ipInt2,
	const char *netname1,
	const char *netname2,
	const char *cuid1,
	const char *cuid2,
	const int client1Id,
	const int client2Id,
	const char *matchId,
	const int totalTime,
	const int maxSpeed1,
	const int avgSpeed1,
	const time_t date,
	const CaptureCategoryFlags flags,
	CaptureRecordsForCategory *recordsPtr,
	CombinedObjNumber objNum,
	unsigned int ipInt3,
	unsigned int ipInt4,
	const char *netname3,
	const char *netname4,
	const char *cuid3,
	const char *cuid4,
	int *tiedOut
	)
{
	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer || !flags) {
		return 0;
	}

	CaptureRecord *recordArray = &recordsPtr->records[0];
	int newIndex;

	int numPlayers = 0;
	if (VALIDSTRING(netname1))
		numPlayers++;
	if (VALIDSTRING(netname2))
		numPlayers++;
	if (VALIDSTRING(netname3))
		numPlayers++;
	if (VALIDSTRING(netname4))
		numPlayers++;

	// we don't want more than one entry per category per player, so first, check if there is already one record for this player
	for (newIndex = 0; newIndex < MAX_SAVED_RECORDS; ++newIndex) {
		if (!recordArray[newIndex].totalTime) {
			continue; // not a valid record
		}

		qboolean match = RecordMatchesPlayers(
			numPlayers,
			&recordArray[newIndex],
			cuid1,
			cuid2,
			cuid3,
			cuid4,
			ipInt1,
			ipInt2,
			ipInt3,
			ipInt4);

		if (match)
			break;
	}

	int goldTime = -1, silverTime = -1, bronzeTime = -1, fourthTime = -1;
	for (int i = 0; i < MAX_SAVED_RECORDS; ++i) {
		if (!recordArray[i].totalTime) {
			continue; // not a valid record
		}
		switch (i) {
		case 0:
			goldTime = recordArray[i].totalTime;
			break;
		case 1:
			if (recordArray[i].totalTime == goldTime) {
			}
			else {
				silverTime = recordArray[i].totalTime;
			}
			break;
		case 2:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else {
				bronzeTime = recordArray[i].totalTime;
			}
			break;
		case 3:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else if (recordArray[i].totalTime == bronzeTime) {
			}
			else {
				fourthTime = recordArray[i].totalTime;
			}
			break;
		default:
			break;
		}
	}

	if (newIndex < MAX_SAVED_RECORDS) {
		// we found an existing record for this player, so just use its index to overwrite it if it's better
		if (totalTime >= recordArray[newIndex].totalTime) {
			return 0; // our existing record is better, so don't save anything to avoid record spam
		}

		// we know we have AT LEAST done better than our current record, so we will save something in any case
		// now, if we didn't already have the top record, check if we did less than the better records
		if (newIndex > 0) {
			const int currentRecordIndex = newIndex;

			for (; newIndex > 0; --newIndex) {
				if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime <= totalTime) {
					break; // this one is better or equal, so use the index after it
				}
			}

			if (newIndex != currentRecordIndex) {
				// we indeed did less than a record which was better than our former record. use its index and shift the array
				memmove(recordArray + newIndex + 1, recordArray + newIndex, (currentRecordIndex - newIndex) * sizeof(*recordArray));
			}
		}
	}
	else {
		// this player doesn't have a record in this category yet, so find an index by comparing times from the worst to the best
		for (newIndex = MAX_SAVED_RECORDS; newIndex > 0; --newIndex) {
			if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime <= totalTime) {
				break; // this one is better or equal, so use the index after it
			}
		}

		// if the worst time is better, the index will point past the array, so don't bother
		if (newIndex >= MAX_SAVED_RECORDS) {
			return 0;
		}

		// shift the array to the right unless this is already the last element
		if (newIndex < MAX_SAVED_RECORDS - 1) {
			memmove(recordArray + newIndex + 1, recordArray + newIndex, (MAX_SAVED_RECORDS - newIndex - 1) * sizeof(*recordArray));
		}
	}

	// overwrite the selected element with the new record
	CaptureRecord *newElement = &recordArray[newIndex];
	if (VALIDSTRING(netname1))
		Q_strncpyz(newElement->recordHolderNames[0], netname1, sizeof(newElement->recordHolderNames[0]));
	else
		newElement->recordHolderNames[0][0] = '\0';
	if (VALIDSTRING(netname2))
		Q_strncpyz(newElement->recordHolderNames[1], netname2, sizeof(newElement->recordHolderNames[1]));
	else
		newElement->recordHolderNames[1][0] = '\0';
	if (VALIDSTRING(netname3))
		Q_strncpyz(newElement->recordHolderNames[2], netname3, sizeof(newElement->recordHolderNames[2]));
	else
		newElement->recordHolderNames[2][0] = '\0';
	if (VALIDSTRING(netname4))
		Q_strncpyz(newElement->recordHolderNames[3], netname4, sizeof(newElement->recordHolderNames[3]));
	else
		newElement->recordHolderNames[3][0] = '\0';

	newElement->recordHolderIpInts[0] = ipInt1;
	newElement->recordHolderIpInts[1] = ipInt2;
	newElement->recordHolderIpInts[2] = ipInt3;
	newElement->recordHolderIpInts[3] = ipInt4;
	newElement->totalTime = totalTime;
	newElement->maxSpeed1 = maxSpeed1;
	newElement->avgSpeed1 = avgSpeed1;
	newElement->date = date;
	newElement->recordHolder1ClientId = client1Id;
	newElement->recordHolder2ClientId = client2Id;

	if (flags & CAPTURERECORDFLAG_FULLMAP)
		memcpy(&(newElement->objTimes), &objTimes[0], sizeof(newElement->objTimes));

	// cuid is optional, empty for clients without one
	if (VALIDSTRING(cuid1))
		Q_strncpyz(newElement->recordHolderCuids[0], cuid1, sizeof(newElement->recordHolderCuids[0]));
	else
		newElement->recordHolderCuids[0][0] = '\0';
	if (VALIDSTRING(cuid2))
		Q_strncpyz(newElement->recordHolderCuids[1], cuid2, sizeof(newElement->recordHolderCuids[1]));
	else
		newElement->recordHolderCuids[1][0] = '\0';
	if (VALIDSTRING(cuid3))
		Q_strncpyz(newElement->recordHolderCuids[2], cuid3, sizeof(newElement->recordHolderCuids[2]));
	else
		newElement->recordHolderCuids[2][0] = '\0';
	if (VALIDSTRING(cuid4))
		Q_strncpyz(newElement->recordHolderCuids[3], cuid4, sizeof(newElement->recordHolderCuids[3]));
	else
		newElement->recordHolderCuids[3][0] = '\0';

	// match id is optional, empty if sv_matchid is not implemented in this OpenJK version
	if (VALIDSTRING(matchId) && strlen(matchId) == SV_MATCHID_LEN - 1) {
		Q_strncpyz(newElement->matchId, matchId, sizeof(newElement->matchId));
	}
	else {
		newElement->matchId[0] = '\0';
	}

	// save the changes later in db
	level.mapCaptureRecords.changed = qtrue;

	if (tiedOut) {
		if (totalTime == goldTime)
			*tiedOut = 1;
		else if (totalTime == silverTime)
			*tiedOut = 2;
		else if (totalTime == bronzeTime)
			*tiedOut = 3;
		else if (totalTime == fourthTime)
			*tiedOut = 4;
	}
	else {
		assert(qfalse);
	}

	return newIndex + 1;
}

// like LogCaptureTime, but for defense
static int LogDefenseCaptureTime(
	unsigned int ipInt1,
	unsigned int ipInt2,
	const char *netname1,
	const char *netname2,
	const char *cuid1,
	const char *cuid2,
	const int client1Id,
	const int client2Id,
	const char *matchId,
	const int totalTime,
	const int frags,
	const float kpm,
	const time_t date,
	const CaptureCategoryFlags flags,
	CaptureRecordsForCategory *recordsPtr,
	CombinedObjNumber objNum,
	unsigned int ipInt3,
	unsigned int ipInt4,
	const char *netname3,
	const char *netname4,
	const char *cuid3,
	const char *cuid4,
	int *tiedOut
)
{
	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer || !flags) {
		return 0;
	}

	CaptureRecord *recordArray = &recordsPtr->records[0];
	int newIndex;

	int numPlayers = 0;
	if (VALIDSTRING(netname1))
		numPlayers++;
	if (VALIDSTRING(netname2))
		numPlayers++;
	if (VALIDSTRING(netname3))
		numPlayers++;
	if (VALIDSTRING(netname4))
		numPlayers++;

	// we don't want more than one entry per category per player, so first, check if there is already one record for this player
	for (newIndex = 0; newIndex < MAX_SAVED_RECORDS; ++newIndex) {
		if (!recordArray[newIndex].totalTime) {
			continue; // not a valid record
		}

		qboolean match = RecordMatchesPlayers(
			numPlayers,
			&recordArray[newIndex],
			cuid1,
			cuid2,
			cuid3,
			cuid4,
			ipInt1,
			ipInt2,
			ipInt3,
			ipInt4);

		if (match)
			break;
	}

	int goldTime = -1, silverTime = -1, bronzeTime = -1, fourthTime = -1;
	int goldObjs = -1, silverObjs = -1, bronzeObjs = -1, fourthObjs = -1;
	for (int i = 0; i < MAX_SAVED_RECORDS; ++i) {
		if (!recordArray[i].totalTime) {
			continue; // not a valid record
		}
		switch (i) {
		case 0:
			goldTime = recordArray[i].totalTime;
			goldObjs = recordArray[i].objectivesCompleted;
			break;
		case 1:
			if (recordArray[i].totalTime == goldTime) {
			}
			else {
				silverTime = recordArray[i].totalTime;
				silverObjs = recordArray[i].objectivesCompleted;
			}
			break;
		case 2:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else {
				bronzeTime = recordArray[i].totalTime;
				bronzeObjs = recordArray[i].objectivesCompleted;
			}
			break;
		case 3:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else if (recordArray[i].totalTime == bronzeTime) {
			}
			else {
				fourthTime = recordArray[i].totalTime;
				fourthObjs = recordArray[i].objectivesCompleted;
			}
			break;
		default:
			break;
		}
	}

	if (newIndex < MAX_SAVED_RECORDS) {
		// we found an existing record for this player, so just use its index to overwrite it if it's better
		if (totalTime < recordArray[newIndex].totalTime || (!(flags & CAPTURERECORDFLAG_FULLMAP) && totalTime == recordArray[newIndex].totalTime)) {
			return 0; // our existing record is better (fullmap or individual obj), or it is the same (individual obj only), so don't save anything to avoid record spam
		}

		if (flags & CAPTURERECORDFLAG_FULLMAP && totalTime == recordArray[newIndex].totalTime) {
			if (level.totalObjectivesCompleted >= recordArray[newIndex].objectivesCompleted)
				return 0; // fullmap record with the same time AND same or more number of of objectives completed
		}

		// we know we have AT LEAST done better than our current record, so we will save something in any case
		// now, if we didn't already have the top record, check if we did better than the better records
		if (newIndex > 0) {
			const int currentRecordIndex = newIndex;

			if (flags & CAPTURERECORDFLAG_FULLMAP) {
				for (; newIndex > 0; --newIndex) {
					if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime > totalTime) {
						break; // this one is better or equal, so use the index after it
					}
					else if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime == totalTime &&
						recordArray[newIndex - 1].objectivesCompleted <= level.totalObjectivesCompleted) {
						break; // this one is the same time but fewer or equal objectives completed, so use the index after it
					}
				}
			}
			else {
				for (; newIndex > 0; --newIndex) {
					if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime >= totalTime) {
						break; // this one is better or equal, so use the index after it
					}
				}
			}

			if (newIndex != currentRecordIndex) {
				// we indeed did better than a record which was better than our former record. use its index and shift the array
				memmove(recordArray + newIndex + 1, recordArray + newIndex, (currentRecordIndex - newIndex) * sizeof(*recordArray));
			}
		}
	}
	else {
		// this player doesn't have a record in this category yet, so find an index by comparing times from the worst to the best
		if (flags & CAPTURERECORDFLAG_FULLMAP) {
			for (newIndex = MAX_SAVED_RECORDS; newIndex > 0; --newIndex) {
				if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime > totalTime) {
					break; // this one is better, so use the index after it
				}
				else if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime == totalTime &&
					recordArray[newIndex - 1].objectivesCompleted <= level.totalObjectivesCompleted) {
					break; // this one is the same time but fewer or equal objectives completed, so use the index after it
				}
			}
		}
		else {
			for (newIndex = MAX_SAVED_RECORDS; newIndex > 0; --newIndex) {
				if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime >= totalTime) {
					break; // this one is better or equal, so use the index after it
				}
			}
		}

		// if the worst time is better, the index will point past the array, so don't bother
		if (newIndex >= MAX_SAVED_RECORDS) {
			return 0;
		}

		// shift the array to the right unless this is already the last element
		if (newIndex < MAX_SAVED_RECORDS - 1) {
			memmove(recordArray + newIndex + 1, recordArray + newIndex, (MAX_SAVED_RECORDS - newIndex - 1) * sizeof(*recordArray));
		}
	}

	// overwrite the selected element with the new record
	CaptureRecord *newElement = &recordArray[newIndex];
	if (VALIDSTRING(netname1))
		Q_strncpyz(newElement->recordHolderNames[0], netname1, sizeof(newElement->recordHolderNames[0]));
	else
		newElement->recordHolderNames[0][0] = '\0';
	if (VALIDSTRING(netname2))
		Q_strncpyz(newElement->recordHolderNames[1], netname2, sizeof(newElement->recordHolderNames[1]));
	else
		newElement->recordHolderNames[1][0] = '\0';
	if (VALIDSTRING(netname3))
		Q_strncpyz(newElement->recordHolderNames[2], netname3, sizeof(newElement->recordHolderNames[2]));
	else
		newElement->recordHolderNames[2][0] = '\0';
	if (VALIDSTRING(netname4))
		Q_strncpyz(newElement->recordHolderNames[3], netname4, sizeof(newElement->recordHolderNames[3]));
	else
		newElement->recordHolderNames[3][0] = '\0';

	newElement->recordHolderIpInts[0] = ipInt1;
	newElement->recordHolderIpInts[1] = ipInt2;
	newElement->recordHolderIpInts[2] = ipInt3;
	newElement->recordHolderIpInts[3] = ipInt4;
	newElement->totalTime = totalTime;
	newElement->frags = frags;
	if (flags & CAPTURERECORDFLAG_FULLMAP)
		newElement->objectivesCompleted = level.totalObjectivesCompleted;
	else
		newElement->objectivesCompleted = 0;
	newElement->avgSpeed1 = newElement->maxSpeed1 = 0;
	newElement->kpm = kpm;
	newElement->date = date;
	newElement->recordHolder1ClientId = client1Id;
	newElement->recordHolder2ClientId = client2Id;

	if (flags & CAPTURERECORDFLAG_FULLMAP)
		memcpy(&(newElement->objTimes), &objTimes[0], sizeof(newElement->objTimes));

	// cuid is optional, empty for clients without one
	if (VALIDSTRING(cuid1))
		Q_strncpyz(newElement->recordHolderCuids[0], cuid1, sizeof(newElement->recordHolderCuids[0]));
	else
		newElement->recordHolderCuids[0][0] = '\0';
	if (VALIDSTRING(cuid2))
		Q_strncpyz(newElement->recordHolderCuids[1], cuid2, sizeof(newElement->recordHolderCuids[1]));
	else
		newElement->recordHolderCuids[1][0] = '\0';
	if (VALIDSTRING(cuid3))
		Q_strncpyz(newElement->recordHolderCuids[2], cuid3, sizeof(newElement->recordHolderCuids[2]));
	else
		newElement->recordHolderCuids[2][0] = '\0';
	if (VALIDSTRING(cuid4))
		Q_strncpyz(newElement->recordHolderCuids[3], cuid4, sizeof(newElement->recordHolderCuids[3]));
	else
		newElement->recordHolderCuids[3][0] = '\0';

	// match id is optional, empty if sv_matchid is not implemented in this OpenJK version
	if (VALIDSTRING(matchId) && strlen(matchId) == SV_MATCHID_LEN - 1) {
		Q_strncpyz(newElement->matchId, matchId, sizeof(newElement->matchId));
	}
	else {
		newElement->matchId[0] = '\0';
	}

	// save the changes later in db
	level.mapCaptureRecords.changed = qtrue;

	if (tiedOut) {
		if (totalTime == goldTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != goldObjs))
			*tiedOut = 1;
		else if (totalTime == silverTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != silverObjs))
			*tiedOut = 2;
		else if (totalTime == bronzeTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != bronzeObjs))
			*tiedOut = 3;
		else if (totalTime == fourthTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != fourthObjs))
			*tiedOut = 4;
	}
	else {
		assert(qfalse);
	}

	return newIndex + 1;
}

// similar to LogCaptureTime, but used to check the ranking for a broader category without saving anything
static int CheckRanking(
	unsigned int ipInt1,
	unsigned int ipInt2,
	const char *cuid1,
	const char *cuid2,
	const int totalTime,
	const CaptureCategoryFlags flags,
	const CaptureCategoryFlags specificFlags,
	CaptureRecordsForCategory *recordsPtr,
	int *tiedOut) {
	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer || !flags) {
		return 0;
	}

	CaptureRecord *recordArray = &recordsPtr->records[0];

	int goldTime = -1, silverTime = -1, bronzeTime = -1, fourthTime = -1;
	for (int i = 0; i < MAX_SAVED_RECORDS; ++i) {
		if (!recordArray[i].totalTime) {
			continue; // not a valid record
		}
		switch (i) {
		case 0:
			goldTime = recordArray[i].totalTime;
			break;
		case 1:
			if (recordArray[i].totalTime == goldTime) {
			}
			else {
				silverTime = recordArray[i].totalTime;
			}
			break;
		case 2:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else {
				bronzeTime = recordArray[i].totalTime;
			}
			break;
		case 3:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else if (recordArray[i].totalTime == bronzeTime) {
			}
			else {
				fourthTime = recordArray[i].totalTime;
			}
			break;
		default:
			break;
		}
	}

	int newIndex;

	// we don't want more than one entry per category per player, so first, check if there is already one record for this player
	for (newIndex = 0; newIndex < MAX_SAVED_RECORDS; ++newIndex) {
		if (!recordArray[newIndex].totalTime) {
			continue; // not a valid record
		}

		// don't mix the two types of lookup
		if (!(flags & CAPTURERECORDFLAG_COOP | CAPTURERECORDFLAG_SPEEDRUN)) { // solo type
			if (VALIDSTRING(cuid1)) { // if we have a cuid, use that to find an existing record
				if (VALIDSTRING(recordArray[newIndex].recordHolderCuids[0]) && !Q_stricmp(cuid1, recordArray[newIndex].recordHolderCuids[0]) && recordArray[newIndex].flags == specificFlags)
					break;
			}
			else { // fall back to the whois accuracy...
				if (ipInt1 == recordArray[newIndex].recordHolderIpInts[0] && recordArray[newIndex].flags == specificFlags)
					break;
			}
		}
		else { // co-op type
			if (VALIDSTRING(cuid1) && VALIDSTRING(cuid2)) { // if we have a cuid, use that to find an existing record
				if (VALIDSTRING(recordArray[newIndex].recordHolderCuids[0]) && !Q_stricmp(cuid1, recordArray[newIndex].recordHolderCuids[0]) &&
					VALIDSTRING(recordArray[newIndex].recordHolderCuids[1]) && !Q_stricmp(cuid2, recordArray[newIndex].recordHolderCuids[1]) &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
			else { // fall back to the whois accuracy...
				if (ipInt1 == recordArray[newIndex].recordHolderIpInts[0] && ipInt2 == recordArray[newIndex].recordHolderIpInts[1] &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
		}
	}

	if (newIndex < MAX_SAVED_RECORDS) {
		// we found an existing record for this player, so just use its index to overwrite it if it's better
		if (totalTime >= recordArray[newIndex].totalTime) {
			return 0; // our existing record is better, so don't save anything to avoid record spam
		}

		// we know we have AT LEAST done better than our current record, so we will save something in any case
		// now, if we didn't already have the top record, check if we did less than the better records
		if (newIndex > 0) {
			const int currentRecordIndex = newIndex;

			for (; newIndex > 0; --newIndex) {
				if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime <= totalTime) {
					break; // this one is better or equal, so use the index after it
				}
			}
		}
	}
	else {
		// this player doesn't have a record in this category yet, so find an index by comparing times from the worst to the best
		for (newIndex = MAX_SAVED_RECORDS; newIndex > 0; --newIndex) {
			if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime <= totalTime) {
				break; // this one is better or equal, so use the index after it
			}
		}

		// if the worst time is better, the index will point past the array, so don't bother
		if (newIndex >= MAX_SAVED_RECORDS) {
			return 0;
		}
	}

	if (tiedOut) {
		if (totalTime == goldTime)
			*tiedOut = 1;
		else if (totalTime == silverTime)
			*tiedOut = 2;
		else if (totalTime == bronzeTime)
			*tiedOut = 3;
		else if (totalTime == fourthTime)
			*tiedOut = 4;
	}
	else {
		assert(qfalse);
	}

	return newIndex + 1;
}

// like CheckRanking, but for defense only
static int CheckDefenseRanking(
	unsigned int ipInt1,
	unsigned int ipInt2,
	unsigned int ipInt3,
	unsigned int ipInt4,
	const char *cuid1,
	const char *cuid2,
	const char *cuid3,
	const char *cuid4,
	const int numDefenders,
	const int totalTime,
	const CaptureCategoryFlags flags,
	const CaptureCategoryFlags specificFlags,
	CaptureRecordsForCategory *recordsPtr,
	int *tiedOut) {
	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer || !flags || numDefenders < 2 || numDefenders > 4) {
		return 0;
	}

	CaptureRecord *recordArray = &recordsPtr->records[0];

	int goldTime = -1, silverTime = -1, bronzeTime = -1, fourthTime = -1;
	int goldObjs = -1, silverObjs = -1, bronzeObjs = -1, fourthObjs = -1;
	for (int i = 0; i < MAX_SAVED_RECORDS; ++i) {
		if (!recordArray[i].totalTime) {
			continue; // not a valid record
		}
		switch (i) {
		case 0:
			goldTime = recordArray[i].totalTime;
			goldObjs = recordArray[i].objectivesCompleted;
			break;
		case 1:
			if (recordArray[i].totalTime == goldTime) {
			}
			else {
				silverTime = recordArray[i].totalTime;
				silverObjs = recordArray[i].objectivesCompleted;
			}
			break;
		case 2:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else {
				bronzeTime = recordArray[i].totalTime;
				bronzeObjs = recordArray[i].objectivesCompleted;
			}
			break;
		case 3:
			if (recordArray[i].totalTime == goldTime) {
			}
			else if (recordArray[i].totalTime == silverTime) {
			}
			else if (recordArray[i].totalTime == bronzeTime) {
			}
			else {
				fourthTime = recordArray[i].totalTime;
				fourthObjs = recordArray[i].objectivesCompleted;
			}
			break;
		default:
			break;
		}
	}

	int newIndex;

	// we don't want more than one entry per category per player, so first, check if there is already one record for this player
	for (newIndex = 0; newIndex < MAX_SAVED_RECORDS; ++newIndex) {
		if (!recordArray[newIndex].totalTime) {
			continue; // not a valid record
		}

		if (numDefenders == 4) {
			if (VALIDSTRING(cuid1) && VALIDSTRING(cuid2) && VALIDSTRING(cuid3) && VALIDSTRING(cuid4)) { // if we have a cuid, use that to find an existing record
				if (VALIDSTRING(recordArray[newIndex].recordHolderCuids[0]) && !Q_stricmp(cuid1, recordArray[newIndex].recordHolderCuids[0]) &&
					VALIDSTRING(recordArray[newIndex].recordHolderCuids[1]) && !Q_stricmp(cuid2, recordArray[newIndex].recordHolderCuids[1]) &&
					VALIDSTRING(recordArray[newIndex].recordHolderCuids[2]) && !Q_stricmp(cuid2, recordArray[newIndex].recordHolderCuids[2]) &&
					VALIDSTRING(recordArray[newIndex].recordHolderCuids[3]) && !Q_stricmp(cuid2, recordArray[newIndex].recordHolderCuids[3]) &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
			else { // fall back to the whois accuracy...
				if (ipInt1 == recordArray[newIndex].recordHolderIpInts[0] && ipInt2 == recordArray[newIndex].recordHolderIpInts[1] &&
					ipInt3 == recordArray[newIndex].recordHolderIpInts[2] && ipInt4 == recordArray[newIndex].recordHolderIpInts[3] &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
		}
		else if (numDefenders == 3) {
			if (VALIDSTRING(cuid1) && VALIDSTRING(cuid2) && VALIDSTRING(cuid3)) { // if we have a cuid, use that to find an existing record
				if (VALIDSTRING(recordArray[newIndex].recordHolderCuids[0]) && !Q_stricmp(cuid1, recordArray[newIndex].recordHolderCuids[0]) &&
					VALIDSTRING(recordArray[newIndex].recordHolderCuids[1]) && !Q_stricmp(cuid2, recordArray[newIndex].recordHolderCuids[1]) &&
					VALIDSTRING(recordArray[newIndex].recordHolderCuids[2]) && !Q_stricmp(cuid2, recordArray[newIndex].recordHolderCuids[2]) &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
			else { // fall back to the whois accuracy...
				if (ipInt1 == recordArray[newIndex].recordHolderIpInts[0] && ipInt2 == recordArray[newIndex].recordHolderIpInts[1] &&
					ipInt3 == recordArray[newIndex].recordHolderIpInts[2] &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
		}
		else { // 2
			if (VALIDSTRING(cuid1) && VALIDSTRING(cuid2)) { // if we have a cuid, use that to find an existing record
				if (VALIDSTRING(recordArray[newIndex].recordHolderCuids[0]) && !Q_stricmp(cuid1, recordArray[newIndex].recordHolderCuids[0]) &&
					VALIDSTRING(recordArray[newIndex].recordHolderCuids[1]) && !Q_stricmp(cuid2, recordArray[newIndex].recordHolderCuids[1]) &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
			else { // fall back to the whois accuracy...
				if (ipInt1 == recordArray[newIndex].recordHolderIpInts[0] && ipInt2 == recordArray[newIndex].recordHolderIpInts[1] &&
					recordArray[newIndex].flags == specificFlags)
					break;
			}
		}
	}

	if (newIndex < MAX_SAVED_RECORDS) {
		// we found an existing record for this player, so just use its index to overwrite it if it's better
		if (totalTime <= recordArray[newIndex].totalTime) {
			return 0; // our existing record is better, so don't save anything to avoid record spam
		}

		// we know we have AT LEAST done better than our current record, so we will save something in any case
		// now, if we didn't already have the top record, check if we did less than the better records
		if (newIndex > 0) {
			const int currentRecordIndex = newIndex;

			for (; newIndex > 0; --newIndex) {
				if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime >= totalTime) {
					break; // this one is better or equal, so use the index after it
				}
			}
		}
	}
	else {
		// this player doesn't have a record in this category yet, so find an index by comparing times from the worst to the best
		for (newIndex = MAX_SAVED_RECORDS; newIndex > 0; --newIndex) {
			if (recordArray[newIndex - 1].totalTime && recordArray[newIndex - 1].totalTime >= totalTime) {
				break; // this one is better or equal, so use the index after it
			}
		}

		// if the worst time is better, the index will point past the array, so don't bother
		if (newIndex >= MAX_SAVED_RECORDS) {
			return 0;
		}
	}

	if (tiedOut) {
		if (totalTime == goldTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != goldObjs))
			*tiedOut = 1;
		else if (totalTime == silverTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != silverObjs))
			*tiedOut = 2;
		else if (totalTime == bronzeTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != bronzeObjs))
			*tiedOut = 3;
		else if (totalTime == fourthTime && !(flags & CAPTURERECORDFLAG_FULLMAP && level.totalObjectivesCompleted != fourthObjs))
			*tiedOut = 4;
	}
	else {
		assert(qfalse);
	}

	return newIndex + 1;
}

void SpeedRunModeRuined(const char *reason) {
	if (level.mapCaptureRecords.speedRunModeRuined)
		return;
	level.mapCaptureRecords.speedRunModeRuined = qtrue;
#if 0
	Com_Printf("^1Speed run mode ruined! Reason: %s^7\n", reason);
#endif
}

void LivePugRuined(const char *reason, qboolean announce) {
	if (level.isLivePug == ISLIVEPUG_NO) // prevent spamming people with messages; once is enough
		return;

	Com_Printf(va("Pug is not live (reason: %s)\n", reason)); // print the reason to the server console
	level.isLivePug = ISLIVEPUG_NO;

	if (!g_notifyNotLive.integer || !announce)
		return;

	trap_SendServerCommand(-1, va("print \"^1Pug is not live:^7 %s^7\n\"", reason));
	trap_SendServerCommand(-1, va("cp \"^1Pug is not live:^7\n%s^7\n\"", reason));
}

static char *RankString(int rank, int tied) {
	static char rankString[32] = { 0 };
	if (tied) {
		switch (tied) {
		case 1: Com_sprintf(rankString, sizeof(rankString), "^3Rank: tied for GOLD^7"); break; // gold gets "Rank:" in gold, too
		case 2: Com_sprintf(rankString, sizeof(rankString), "^5Rank: ^9tied for SILVER^7"); break;
		case 3: Com_sprintf(rankString, sizeof(rankString), "^5Rank: ^8tied for BRONZE^7"); break;
		default: Com_sprintf(rankString, sizeof(rankString), "^5Rank: ^1tied for %d^7", tied);
		}
	}
	else {
		switch (rank) {
		case 1: Com_sprintf(rankString, sizeof(rankString), "^3Rank: GOLD^7"); break; // gold gets "Rank:" in gold too
		case 2: Com_sprintf(rankString, sizeof(rankString), "^5Rank: ^9SILVER^7"); break;
		case 3: Com_sprintf(rankString, sizeof(rankString), "^5Rank: ^8BRONZE^7"); break;
		default: Com_sprintf(rankString, sizeof(rankString), "^5Rank: ^1%d^7", rank);
		}
	}
	return rankString;
}

// if fullmap completion, objective == -1
static void CheckTopTimes(int timeInMilliseconds, CombinedObjNumber objective, int clientNumOverride) {
	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer || level.endedWithEndMatchCommand) {
		// reset EVERYONE's speed/displacement stats
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gclient_t *cl = &level.clients[i];
			cl->pers.displacement = cl->pers.displacementSamples = cl->pers.topSpeed = 0;
		}
		return;
	}

	if (objective >= 1 && objective <= MAX_SAVED_OBJECTIVES)
		objTimes[objective - 1] = timeInMilliseconds;

	gclient_t *clients[4] = { NULL };
	if (clientNumOverride >= 0 && clientNumOverride < MAX_CLIENTS) {
		clients[0] = &level.clients[clientNumOverride];
	}
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		if (cl->pers.connected != CON_CONNECTED)
			continue;
		if (cl->sess.sessionTeam != TEAM_RED)
			continue;
		if (g_entities[i].inuse && (g_entities[i].r.svFlags & SVF_BOT))
			return;
		if (clients[0] == cl)
			continue;
		if (!clients[0]) {
			clients[0] = cl;
		} else if (!clients[1]) {
			clients[1] = cl;
		} else if (!clients[2]) {
			clients[2] = cl;
		} else if (!clients[3]) {
			clients[3] = cl;
		} else {
			break;
		}
	}
	if (!clients[0])
		return; // nobody is on red...?

	const CaptureCategoryFlags flags = CaptureFlagsForRun(clients[0], clients[1], objective);
	if (!flags)
		return; // invalid
	if (!(flags & CAPTURERECORDFLAG_LIVEPUG)) {
		if (!(flags & CAPTURERECORDFLAG_COOP | CAPTURERECORDFLAG_SPEEDRUN))
			clients[1] = NULL; // sanity check: non-coopspeedruns cannot have clients[1]
		clients[2] = clients[3] = NULL; // in any case, non-livepugs can only have 2 players at most
	}

	char matchId[SV_MATCHID_LEN];
	trap_Cvar_VariableStringBuffer("sv_matchid", matchId, sizeof(matchId)); // this requires a custom OpenJK build

	// store per-obj speeds for calculating the entire-map speeds later
	float *disp[4] = { NULL }, *topSpeed[4] = { NULL };
	int *samples[4] = { NULL };
	if (flags & CAPTURERECORDFLAG_FULLMAP) { // full map
		for (int i = 0; i < 4; i++) {
			gclient_t *cl = clients[i];
			if (!cl)
				break;
			disp[i] = &cl->pers.totalDisplacement;
			topSpeed[i] = &cl->pers.totalTopSpeed;
			samples[i] = &cl->pers.totalDisplacementSamples;
		}
	}
	else { // an individual obj
		for (int i = 0; i < 4; i++) {
			gclient_t *cl = clients[i];
			if (!cl)
				break;
			cl->pers.totalDisplacement += cl->pers.displacement;
			cl->pers.totalDisplacementSamples += cl->pers.displacementSamples;
			if (cl->pers.topSpeed > cl->pers.totalTopSpeed)
				cl->pers.totalTopSpeed = cl->pers.topSpeed;
			disp[i] = &cl->pers.displacement;
			topSpeed[i] = &cl->pers.topSpeed;
			samples[i] = &cl->pers.displacementSamples;
		}
	}

	int maxSpeed = 0, avgSpeed = 0;
	if (clients[1]) { // multiple players
		int samplesBetweenAllPlayers = 0;
		float displacementBetweenAllPlayers = 0.0f;
		int numPlayersWithSamples = 0;
		for (int i = 0; i < 4; i++) {
			if (!clients[i])
				break;
			int thisGuyMaxSpeed = (int)(*(topSpeed[i]) + 0.5f);
			if (thisGuyMaxSpeed > maxSpeed)
				maxSpeed = thisGuyMaxSpeed;
			if (*(samples[i])) {
				samplesBetweenAllPlayers += *(samples[i]);
				displacementBetweenAllPlayers += *(disp[i]);
				numPlayersWithSamples++;
			}
		}
		if (numPlayersWithSamples) {
			float mean = (displacementBetweenAllPlayers * g_svfps.value) / samplesBetweenAllPlayers;
			avgSpeed = (int)(mean + 0.5f);
		}
		else {
			assert(qfalse);
		}
	}
	else { // just a single player
		maxSpeed = (int)(*(topSpeed[0]) + 0.5f);
		if (*(samples[0]))
			avgSpeed = (*(disp[0]) * g_svfps.value) / *(samples[0]);
		else
			avgSpeed = 0;
	}

	// reset EVERYONE's speed/displacement stats
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		cl->pers.displacement = cl->pers.displacementSamples = cl->pers.topSpeed = 0;
	}

	CaptureRecordsForCategory *recordsPtr = CaptureRecordsForCategoryFromFlags(flags);
	if (!recordsPtr) {
		assert(qfalse);
		return; // ???
	}

#ifdef _DEBUG
	if (z_debugSiegeTime.string[0])
		timeInMilliseconds = z_debugSiegeTime.integer;
#endif

	// for speedruns, check whether we registered a rank for a broader (more prestigious) category
	// e.g. if we performed a "speedrun fullmap co-op assault one-shot" run, check if we also ranked for simply "speedrun fullmap"
	int broadRank = 0;
	CaptureCategoryFlags broadFlags = flags;
	int broadRankTied = 0;
	if (flags & CAPTURERECORDFLAG_SPEEDRUN) {
		broadFlags &= ~CAPTURERECORDFLAG_SOLO;
		broadFlags &= ~CAPTURERECORDFLAG_COOP;
		broadFlags &= ~CAPTURERECORDFLAG_ONESHOT;
		broadFlags &= ~CAPTURERECORDFLAG_ASSAULT;
		broadFlags &= ~CAPTURERECORDFLAG_HW;
		broadFlags &= ~CAPTURERECORDFLAG_DEMO;
		broadFlags &= ~CAPTURERECORDFLAG_TECH;
		broadFlags &= ~CAPTURERECORDFLAG_SCOUT;
		broadFlags &= ~CAPTURERECORDFLAG_JEDI;
		if (broadFlags != flags) { // sanity check; make sure we are actually checking something different
			CaptureRecordsForCategory broadRecords = { 0 };
			G_DBLoadCaptureRecords(level.mapCaptureRecords.mapname, broadFlags, qfalse, &broadRecords);
			broadRank = CheckRanking(clients[0]->sess.ip,
				clients[1] ? clients[1]->sess.ip : 0,
				clients[0]->sess.auth == AUTHENTICATED ? clients[0]->sess.cuidHash : "",
				clients[1] && clients[1]->sess.auth == AUTHENTICATED ? clients[1]->sess.cuidHash : "",
				timeInMilliseconds, broadFlags, flags, &broadRecords, &broadRankTied);
		}
	}

	int newRankTied = 0;
	const int recordRank = LogCaptureTime(clients[0]->sess.ip,
		clients[1] ? clients[1]->sess.ip : 0,
		clients[0]->pers.netname,
		clients[1] ? clients[1]->pers.netname : "",
		clients[0]->sess.auth == AUTHENTICATED ? clients[0]->sess.cuidHash : "",
		clients[1] && clients[1]->sess.auth == AUTHENTICATED ? clients[1]->sess.cuidHash : "",
		clients[0] - level.clients,
		clients[1] ? clients[1] - level.clients : -1,
		matchId, timeInMilliseconds, maxSpeed, avgSpeed,
		time(NULL), flags, recordsPtr, objective,
		clients[2] ? clients[2]->sess.ip : 0,
		clients[3] ? clients[3]->sess.ip : 0,
		clients[2] ? clients[2]->pers.netname : "",
		clients[3] ? clients[3]->pers.netname : "",
		clients[2] && clients[2]->sess.auth == AUTHENTICATED ? clients[2]->sess.cuidHash : "",
		clients[3] && clients[3]->sess.auth == AUTHENTICATED ? clients[3]->sess.cuidHash : "",
		&newRankTied);

	// we only print multiple names if it's two people in a coopspeedrun
	// i.e. in a live pug, just print the name of the person who actually did the obj
	char combinedNameString[64] = { 0 };
	Q_strncpyz(combinedNameString, clients[0]->pers.netname, sizeof(combinedNameString));
	if (clients[1] && flags & (CAPTURERECORDFLAG_COOP | CAPTURERECORDFLAG_SPEEDRUN))
		Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9 & ^7%s", clients[1]->pers.netname));

	int mins, secs, millis;
	PartitionedTimer(timeInMilliseconds, &mins, &secs, &millis);
	char timeString[10] = { 0 };
	if (mins > 9) // 12:59.123
		Com_sprintf(timeString, sizeof(timeString), "%d:%02d.%03d", mins, secs, millis);
	else if (mins > 0) // 2:59.123
		Com_sprintf(timeString, sizeof(timeString), " %d:%02d.%03d", mins, secs, millis);
	else if (secs > 9) // 59.123
		Com_sprintf(timeString, sizeof(timeString), "   %d.%03d", secs, millis);
	else // 9.123
		Com_sprintf(timeString, sizeof(timeString), "    %d.%03d", secs, millis);

	char *topSpeedPhrase;
	if (clients[2]) {
		if (flags & (CAPTURERECORDFLAG_COOP | CAPTURERECORDFLAG_SPEEDRUN))
			topSpeedPhrase = "Co-op t"; // coopspeedrun: "Co-op top speed"
		else
			topSpeedPhrase = "Team t"; // live pug: "Team top speed"
	}
	else {
		topSpeedPhrase = "T"; // everything else: "Top speed"
	}

	if (recordRank) {
		// we just did a new capture record, broadcast it

		if (broadRank) { // if they ranked in a broader category, print that, with the exact category as an afterthought

			// more swag colors if you got gold
			if (broadRankTied == 1 || (!broadRankTied && broadRank == 1)) {
				trap_SendServerCommand(-1, va("print \"^3New offensive record by ^7%s^3!    %s    ^3Type: ^7%s    ^3%sop speed: ^7%d    ^3Avg: ^7%d    ^3Time: ^7%s\n\"",
					combinedNameString, RankString(broadRank, broadRankTied), GetLongNameForRecordFlags(level.mapname, broadFlags, qfalse), topSpeedPhrase, maxSpeed, avgSpeed, timeString));
			}
			else {
				trap_SendServerCommand(-1, va("print \"^5New offensive record by ^7%s^5!    %s    ^5Type: ^7%s    ^5%sop speed: ^7%d    ^5Avg: ^7%d    ^5Time: ^7%s\n\"",
					combinedNameString, RankString(broadRank, broadRankTied), GetLongNameForRecordFlags(level.mapname, broadFlags, qfalse), topSpeedPhrase, maxSpeed, avgSpeed, timeString));
			}

			// more swag colors if you got gold
			if (newRankTied == 1 || (!newRankTied && recordRank == 1))
				trap_SendServerCommand(-1, va("print \"^3Also set offensive record for ^7%s (%s)\n\"", GetLongNameForRecordFlags(level.mapname, flags, qtrue), RankString(recordRank, newRankTied)));
			else
				trap_SendServerCommand(-1, va("print \"^7Also set offensive record for %s ^7(%s)\n\"", GetLongNameForRecordFlags(level.mapname, flags, qtrue), RankString(recordRank, newRankTied)));

		}
		else { // just print the exact category they ranked in

			// more swag colors if you got gold
			if (newRankTied == 1 || (!newRankTied && recordRank == 1)) {
				trap_SendServerCommand(-1, va("print \"^3New offensive record by ^7%s^3!    %s    ^3Type: ^7%s    ^3%sop speed: ^7%d    ^3Avg: ^7%d    ^3Time: ^7%s\n\"",
					combinedNameString, RankString(recordRank, newRankTied), GetLongNameForRecordFlags(level.mapname, flags, qtrue), topSpeedPhrase, maxSpeed, avgSpeed, timeString));
			}
			else {
				trap_SendServerCommand(-1, va("print \"^5New offensive record by ^7%s^5!    %s    ^5Type: ^7%s    ^5%sop speed: ^7%d    ^5Avg: ^7%d    ^5Time: ^7%s\n\"",
					combinedNameString, RankString(recordRank, newRankTied), GetLongNameForRecordFlags(level.mapname, flags, qtrue), topSpeedPhrase, maxSpeed, avgSpeed, timeString));
			}

		}

		// update the in-memory db so that toptimes can reflect this change
		G_DBSaveCaptureRecords(&level.mapCaptureRecords);
	}
	else if (level.isLivePug == ISLIVEPUG_NO && !(flags & CAPTURERECORDFLAG_LIVEPUG)) {
		// we didn't make a new record, but that was still a valid run. show everyone what time they did
		// don't print this for pugs, though
		trap_SendServerCommand(-1, va("print \"^7No offensive record beaten.    ^5Type: ^7%s    ^5%sop speed: ^7%d    ^5Avg: ^7%d    ^5Time: ^7%s\n\"",
			GetLongNameForRecordFlags(level.mapname, flags, qtrue), topSpeedPhrase, maxSpeed, avgSpeed, timeString));
	}
}

// if fullmap, objective == -1
static void CheckDefenseRecords(int timeInMilliseconds, CombinedObjNumber objective) {
	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer || level.endedWithEndMatchCommand) {
		// reset EVERYONE's frag since obj start stats
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gclient_t *cl = &level.clients[i];
			cl->pers.fragsSinceObjStart = 0;
		}
		return;
	}

	if (objective >= 1 && objective <= MAX_SAVED_OBJECTIVES)
		objTimes[objective - 1] = timeInMilliseconds;

	gclient_t *clients[4] = { NULL };
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		if (cl->pers.connected != CON_CONNECTED)
			continue;
		if (cl->sess.sessionTeam != TEAM_BLUE)
			continue;
		if (g_entities[i].inuse && (g_entities[i].r.svFlags & SVF_BOT))
			return;
		if (clients[0] == cl)
			continue;
		if (!clients[0]) {
			clients[0] = cl;
		}
		else if (!clients[1]) {
			clients[1] = cl;
		}
		else if (!clients[2]) {
			clients[2] = cl;
		}
		else if (!clients[3]) {
			clients[3] = cl;
		}
		else {
			break;
		}
	}
	if (!clients[0])
		return; // nobody is on blue...?

	const CaptureCategoryFlags flags = CaptureFlagsForDefenseHold(clients[0], clients[1], objective);
	if (!flags)
		return; // invalid
	if (flags & CAPTURERECORDFLAG_2V2)
		clients[2] = clients[3] = NULL; // sanity check; only 2 players in 2v2
	else if (flags & CAPTURERECORDFLAG_3V3)
		clients[3] = NULL; // sanity check; only 3 players in 3v3

	char matchId[SV_MATCHID_LEN];
	trap_Cvar_VariableStringBuffer("sv_matchid", matchId, sizeof(matchId)); // this requires a custom OpenJK build

	// store per-obj frags for calculating the entire-map speeds later
	int *frags[4] = { NULL };
	if (flags & CAPTURERECORDFLAG_FULLMAP) { // full map
		for (int i = 0; i < 4; i++) {
			gclient_t *cl = clients[i];
			if (!cl)
				break;
			frags[i] = &cl->sess.siegeStats.dKills[GetSiegeStatRound()];
		}
	}
	else { // an individual obj
		for (int i = 0; i < 4; i++) {
			gclient_t *cl = clients[i];
			if (!cl)
				break;
			frags[i] = &cl->pers.fragsSinceObjStart;
		}
	}

	int teamFrags = 0;
	float teamKpm = 0.0f;
	for (int i = 0; i < 4; i++) {
		if (!clients[i])
			break;
		int thisGuyFrags = *frags[i];
		teamFrags += thisGuyFrags;
	}
	if (teamFrags)
		teamKpm = ((float)teamFrags / (timeInMilliseconds ? (float)timeInMilliseconds : 1.0f)) * 60000.0f; // avoid division by zero
	else
		teamKpm = 0.0f;

	// reset EVERYONE's per-obj frag stats
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		cl->pers.fragsSinceObjStart = 0;
	}

	CaptureRecordsForCategory *recordsPtr = CaptureRecordsForCategoryFromFlags(flags);
	if (!recordsPtr) {
		assert(qfalse);
		return; // ???
	}

#ifdef _DEBUG
	if (z_debugSiegeTime.string[0])
		timeInMilliseconds = z_debugSiegeTime.integer;
#endif

	// check whether we registered a rank for a broader (more prestigious) category
	// e.g. if we performed a 2v2 hold, check if we also ranked for simply a defense hold (with an indeterminate number of defenders)
	int broadRank = 0;
	CaptureCategoryFlags broadFlags = flags;
	int broadRankTied = 0;
	broadFlags &= ~CAPTURERECORDFLAG_2V2;
	broadFlags &= ~CAPTURERECORDFLAG_3V3;
	broadFlags &= ~CAPTURERECORDFLAG_4V4;
	int numDefenders = 0;
	for (int i = 0; i < 4; i++) {
		if (clients[i])
			numDefenders++;
	}
	if (broadFlags != flags) { // sanity check; make sure we are actually checking something different
		CaptureRecordsForCategory broadRecords = { 0 };
		G_DBLoadCaptureRecords(level.mapCaptureRecords.mapname, broadFlags, qfalse, &broadRecords);
		broadRank = CheckDefenseRanking(clients[0]->sess.ip,
			clients[1] ? clients[1]->sess.ip : 0,
			clients[2] ? clients[2]->sess.ip : 0,
			clients[3] ? clients[3]->sess.ip : 0,
			clients[0]->sess.auth == AUTHENTICATED ? clients[0]->sess.cuidHash : "",
			clients[1] && clients[1]->sess.auth == AUTHENTICATED ? clients[1]->sess.cuidHash : "",
			clients[2] && clients[2]->sess.auth == AUTHENTICATED ? clients[2]->sess.cuidHash : "",
			clients[3] && clients[3]->sess.auth == AUTHENTICATED ? clients[3]->sess.cuidHash : "",
			numDefenders, timeInMilliseconds, broadFlags, flags, &broadRecords, &broadRankTied);
	}

	int newRankTied = 0;
	const int recordRank = LogDefenseCaptureTime(clients[0]->sess.ip,
		clients[1] ? clients[1]->sess.ip : 0,
		clients[0]->pers.netname,
		clients[1] ? clients[1]->pers.netname : "",
		clients[0]->sess.auth == AUTHENTICATED ? clients[0]->sess.cuidHash : "",
		clients[1] && clients[1]->sess.auth == AUTHENTICATED ? clients[1]->sess.cuidHash : "",
		clients[0] - level.clients,
		clients[1] ? clients[1] - level.clients : -1,
		matchId, timeInMilliseconds, teamFrags, teamKpm,
		time(NULL), flags, recordsPtr, objective,
		clients[2] ? clients[2]->sess.ip : 0,
		clients[3] ? clients[3]->sess.ip : 0,
		clients[2] ? clients[2]->pers.netname : "",
		clients[3] ? clients[3]->pers.netname : "",
		clients[2] && clients[2]->sess.auth == AUTHENTICATED ? clients[2]->sess.cuidHash : "",
		clients[3] && clients[3]->sess.auth == AUTHENTICATED ? clients[3]->sess.cuidHash : "",
		&newRankTied);

	// print the name of everyone on defense
	char combinedNameString[256] = { 0 };
	Q_strncpyz(combinedNameString, clients[0]->pers.netname, sizeof(combinedNameString));
	if (clients[1] && clients[2] && clients[3])
		Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9, ^7%s^9, ^7%s,^9 & ^7%s", clients[1]->pers.netname, clients[2]->pers.netname, clients[3]->pers.netname));
	else if (clients[1] && clients[2])
		Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9, ^7%s^9, & ^7%s", clients[1]->pers.netname, clients[2]->pers.netname));
	else if (clients[1])
		Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9 & ^7%s", clients[1]->pers.netname));

	int mins, secs, millis;
	PartitionedTimer(timeInMilliseconds, &mins, &secs, &millis);
	char timeString[10] = { 0 };
	if (mins > 9) // 12:59.123
		Com_sprintf(timeString, sizeof(timeString), "%d:%02d.%03d", mins, secs, millis);
	else if (mins > 0) // 2:59.123
		Com_sprintf(timeString, sizeof(timeString), " %d:%02d.%03d", mins, secs, millis);
	else if (secs > 9) // 59.123
		Com_sprintf(timeString, sizeof(timeString), "   %d.%03d", secs, millis);
	else // 9.123
		Com_sprintf(timeString, sizeof(timeString), "    %d.%03d", secs, millis);

	if (recordRank) {
		// we just did a new capture record, broadcast it

		if (broadRank) { // if they ranked in a broader category, print that, with the exact category as an afterthought

			// more swag colors if you got gold
			if (broadRankTied == 1 || (!broadRankTied && broadRank == 1)) {
				trap_SendServerCommand(-1, va("print \"^3New defensive record by ^7%s^3!    %s    ^3Type: ^7%s    ^3Frags: ^7%d    ^3Kpm: ^7%0.1f    %s^3Time: ^7%s\n\"",
					combinedNameString, RankString(broadRank, broadRankTied), GetLongNameForRecordFlags(level.mapname, broadFlags, qfalse), teamFrags, teamKpm, broadFlags & CAPTURERECORDFLAG_FULLMAP ? va("^3Objs completed: ^7%d   ", level.totalObjectivesCompleted) : "", timeString));
			}
			else {
				trap_SendServerCommand(-1, va("print \"^5New defensive record by ^7%s^5!    %s    ^5Type: ^7%s    ^5Frags: ^7%d    ^5Kpm: ^7%0.1f    %s^5Time: ^7%s\n\"",
					combinedNameString, RankString(broadRank, broadRankTied), GetLongNameForRecordFlags(level.mapname, broadFlags, qfalse), teamFrags, teamKpm, broadFlags & CAPTURERECORDFLAG_FULLMAP ? va("^5Objs completed: ^7%d   ", level.totalObjectivesCompleted) : "", timeString));
			}

			// more swag colors if you got gold
			if (newRankTied == 1 || (!newRankTied && recordRank == 1))
				trap_SendServerCommand(-1, va("print \"^3Also set defensive record for ^7%s (%s)\n\"", GetLongNameForRecordFlags(level.mapname, flags, qtrue), RankString(recordRank, newRankTied)));
			else
				trap_SendServerCommand(-1, va("print \"^7Also set defensive record for %s ^7(%s)\n\"", GetLongNameForRecordFlags(level.mapname, flags, qtrue), RankString(recordRank, newRankTied)));

		}
		else { // just print the exact category they ranked in

			// more swag colors if you got gold
			if (newRankTied == 1 || (!newRankTied && recordRank == 1)) {
				trap_SendServerCommand(-1, va("print \"^3New defensive record by ^7%s^3!    %s    ^3Type: ^7%s    ^3Frags: ^7%d    ^3Kpm: ^7%0.1f    %s^3Time: ^7%s\n\"",
					combinedNameString, RankString(recordRank, newRankTied), GetLongNameForRecordFlags(level.mapname, flags, qtrue), teamFrags, teamKpm, flags & CAPTURERECORDFLAG_FULLMAP ? va("^3Objs completed: ^7%d   ", level.totalObjectivesCompleted) : "", timeString));
			}
			else {
				trap_SendServerCommand(-1, va("print \"^5New defensive record by ^7%s^5!    %s    ^5Type: ^7%s    ^5Frags: ^7%d    ^5Kpm: ^7%0.1f    %s^5Time: ^7%s\n\"",
					combinedNameString, RankString(recordRank, newRankTied), GetLongNameForRecordFlags(level.mapname, flags, qtrue), teamFrags, teamKpm, flags & CAPTURERECORDFLAG_FULLMAP ? va("^5Objs completed: ^7%d   ", level.totalObjectivesCompleted) : "", timeString));
			}

		}

		// update the in-memory db so that toptimes can reflect this change
		G_DBSaveCaptureRecords(&level.mapCaptureRecords);
	}
}

extern void PrintStatsTo(gentity_t *ent, const char *type);
void G_SiegeRoundComplete(int winningteam, int winningclient, qboolean completedEntireMap)
{
	ComputeSiegePlayTimes();

	vec3_t nomatter;
	char teamstr[1024];
	int originalWinningClient = winningclient;

	level.antiLamingTime = 0;
	level.siegeRoundComplete = qtrue;
	int totalRoundTime = 0;

	if (level.siegeStage == SIEGESTAGE_ROUND1) {
		level.siegeStage = SIEGESTAGE_ROUND1POSTGAME;
		int realTime = 0;
		if (g_siegeTeamSwitch.integer && (imperial_time_limit || rebel_time_limit)) {
			if (imperial_time_limit)
				realTime = imperial_time_limit - (gImperialCountdown - level.time);
			else if (rebel_time_limit)
				realTime = rebel_time_limit - (gRebelCountdown - level.time);
			if (realTime < 1)
				realTime = 1;
		}
		totalRoundTime = realTime ? realTime : abs(level.time - level.siegeRoundStartTime);
		trap_Cvar_Set("siege_r1_total", va("%i", realTime));
		trap_Cvar_Set("siege_r1_matchid", Cvar_VariableString("sv_matchid"));
	}
	else if (level.siegeStage == SIEGESTAGE_ROUND2) {
		level.siegeStage = SIEGESTAGE_ROUND2POSTGAME;
		if (!level.siegeMatchWinner)
			level.siegeMatchWinner = OtherTeam(winningteam);
		totalRoundTime = abs(level.time - level.siegeRoundStartTime);
		trap_Cvar_Set("siege_r2_total", va("%i", abs(level.time - level.siegeRoundStartTime)));
	}

	level.siegeRoundStartTime = 0;

	if (winningclient != ENTITYNUM_NONE && g_entities[winningclient].client &&
		g_entities[winningclient].client->sess.sessionTeam != winningteam)
	{ //this person just won the round for the other team..
		winningclient = ENTITYNUM_NONE;
	}

	if (winningteam == TEAM_RED && completedEntireMap)
		CheckTopTimes(totalRoundTime, -1, winningclient >= 0 && winningclient < MAX_CLIENTS ? winningclient : -1);
	CheckDefenseRecords(totalRoundTime, -1);

	VectorClear(nomatter);

	SiegeBroadcast_ROUNDOVER(winningteam, winningclient);

	AddSiegeWinningTeamPoints(winningteam, winningclient);

	//Instead of exiting like this, fire off a target, and let it handle things.
	//Can be a script or whatever the designer wants.
   	if (winningteam == SIEGETEAM_TEAM1)
	{
		Com_sprintf(teamstr, sizeof(teamstr), team1);
	}
	else
	{
		Com_sprintf(teamstr, sizeof(teamstr), team2);
	}

	trap_SetConfigstring(CS_SIEGE_STATE, va("3|%i", level.time)); //ended
	gSiegeRoundBegun = qfalse;
	gSiegeRoundEnded = qtrue;
	gSiegeRoundWinningTeam = winningteam;

	if (BG_SiegeGetValueGroup(siege_info, teamstr, gParseObjectives))
	{
		if (!BG_SiegeGetPairedValue(gParseObjectives, "roundover_target", teamstr))
		{ //didn't find the name of the thing to target upon win, just logexit now then.
			LogExit( "Objectives completed" );
			//return;
			//returning here breaks round 2 timer if there's no target_siege_end
		}
		
		if (originalWinningClient && originalWinningClient == ENTITYNUM_NONE) //add null check
		{ //oh well, just find something active and use it then.
            int i = 0;
			gentity_t *ent;

			while (i < MAX_CLIENTS)
			{
				ent = &g_entities[i];

				if (ent->inuse)
				{ //sure, you'll do.
                    originalWinningClient = ent->s.number;
					break;
				}

				i++;
			}
		}
		G_UseTargets2(&g_entities[originalWinningClient], &g_entities[originalWinningClient], teamstr);
	}

	if (level.isLivePug != ISLIVEPUG_YES && !level.mapCaptureRecords.speedRunModeRuined && g_speedrunRoundOneRestart.integer) {
		SiegeClearSwitchData(); // if map is completed in speedrun mode, just restart round 1 afterwards and don't switch teams
	}
	else if (g_siegeTeamSwitch.integer &&
		(imperial_time_limit || rebel_time_limit))
	{ //handle stupid team switching crap
		int time = 0;
		if (imperial_time_limit)
		{
			time = imperial_time_limit-(gImperialCountdown-level.time);
		}
		else if (rebel_time_limit)
		{
			time = rebel_time_limit-(gRebelCountdown-level.time);
		}

		if (time < 1)
		{
			time = 1;
		}
		SiegeTeamSwitch(winningteam, time);
	}
	else
	{ //assure it's clear for next round
		SiegeClearSwitchData();
	}

	level.intermissionNeededTime = trap_Milliseconds();
}

void G_ValidateSiegeClassForTeam(gentity_t *ent, int team)
{
	siegeClass_t *scl;
	siegeTeam_t *stm;
	int newClassIndex = -1;
	if (ent->client->siegeClass == -1)
	{ //uh.. sure.
		return;
	}

	scl = &bgSiegeClasses[ent->client->siegeClass];

	stm = BG_SiegeFindThemeForTeam(team);
	if (stm)
	{
		int i = 0;

		while (i < stm->numClasses)
		{ //go through the team and see its valid classes, can we find one that matches our current player class?
			if (stm->classes[i])
			{
				if (!Q_stricmp(scl->name, stm->classes[i]->name))
				{ //the class we're using is already ok for this team.
					return;
				}
				if (stm->classes[i]->playerClass == scl->playerClass ||
					newClassIndex == -1)
				{
					newClassIndex = i;
				}
			}
			i++;
		}

		if (newClassIndex != -1)
		{ //ok, let's find it in the global class array
			ent->client->siegeClass = BG_SiegeFindClassIndexByName(stm->classes[newClassIndex]->name);
			strcpy(ent->client->sess.siegeClass, stm->classes[newClassIndex]->name);
		}
	}
}

//bypass most of the normal checks in SetTeam
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin)
{
	char userinfo[MAX_INFO_STRING];

	trap_GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );

	if (g_gametype.integer == GT_SIEGE)
	{
		G_ValidateSiegeClassForTeam(ent, team);
		if (team == TEAM_BLUE) {
			SpeedRunModeRuined("SetTeamQuick: blue");
		}
		else if (!level.mapCaptureRecords.speedRunModeRuined && team == TEAM_RED &&
			(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
			level.siegeRoundStartTime && level.time - level.siegeRoundStartTime >= 10000) {
			SpeedRunModeRuined("SetTeamQuick: joined red late"); // joined red after 10sec into the round
		}
	}

	ent->client->sess.sessionTeam = team;

	if (team == TEAM_SPECTATOR)
	{
		ent->client->sess.spectatorState = SPECTATOR_FREE;
		Info_SetValueForKey(userinfo, "team", "s");
	}
	else
	{
		ent->client->sess.spectatorState = SPECTATOR_NOT;
		if (team == TEAM_RED)
		{
			Info_SetValueForKey(userinfo, "team", "r");
		}
		else if (team == TEAM_BLUE)
		{
			Info_SetValueForKey(userinfo, "team", "b");
		}
		else
		{
			Info_SetValueForKey(userinfo, "team", "?");
		}
	}

	trap_SetUserinfo( ent->s.number, userinfo );

	ent->client->sess.spectatorClient = 0;

	ent->client->pers.teamState.state = TEAM_BEGIN;

	ClientUserinfoChanged( ent->s.number );

	if (doBegin)
	{
		ClientBegin( ent->s.number, qfalse );
	}
}

extern void SetSiegeClass(gentity_t *ent, char* className);

// account for raven being inconsistent
static int GetStupidClassNumber(int in) {
	switch (in) {
	case SPC_INFANTRY:		return 1;
	case SPC_HEAVY_WEAPONS:	return 2;
	case SPC_DEMOLITIONIST:	return 3;
	case SPC_VANGUARD:		return 4;
	case SPC_SUPPORT:		return 5;
	case SPC_JEDI:			return 6;
	}
	return 1;
}

static char *classNames[] = { "Assault", "Scout", "Tech", "Jedi", "Demolitions", "Heavy Weapons", "another class" };
void G_ChangePlayerFromExceededClass(gentity_t *ent) {
	if (!g_classLimits.integer)
		return;
	int current = bgSiegeClasses[ent->client->siegeClass].playerClass, lastLegit = level.lastLegitClass[ent - g_entities];

	// first, see if we can change back to our previous class
	siegeClass_t *new = BG_SiegeGetClass(ent->client->sess.siegeDesiredTeam, GetStupidClassNumber(lastLegit));
	if (new && lastLegit != -1 && !G_WouldExceedClassLimit(ent->client->sess.siegeDesiredTeam, lastLegit, qtrue, -1, NULL)) {
		trap_SendServerCommand(ent - g_entities, va("print \"The class you selected is full. You were automatically changed back to %s.\n\"", classNames[Com_Clampi(SPC_INFANTRY, SPC_MAX, new->playerClass)]));
		SetSiegeClass(ent, new->name);
		return;
	}
	else { // we were unable to switch back to our previous class; try to switch to something else
		int i, classPreferenceOrder[] = { SPC_INFANTRY, SPC_HEAVY_WEAPONS, SPC_DEMOLITIONIST, SPC_SUPPORT, SPC_VANGUARD, SPC_JEDI };
		for (i = 0; i < 6; i++) {
			new = BG_SiegeGetClass(ent->client->sess.siegeDesiredTeam, GetStupidClassNumber(classPreferenceOrder[i]));
			if (!new || new->playerClass == current || G_WouldExceedClassLimit(ent->client->sess.siegeDesiredTeam, new->playerClass, qtrue, -1, NULL))
				continue;
			// we found a valid target to switch to
			trap_SendServerCommand(ent - g_entities, va("print \"The class you selected is full. You were automatically changed to %s.\n\"", classNames[Com_Clampi(SPC_INFANTRY, SPC_MAX, new->playerClass)]));
			SetSiegeClass(ent, new->name);
			return;
		}
	}
	// morons playing game larger than 6v6 with limit of 1 on each class...
	Com_Error(ERR_DROP, "G_ChangePlayerFromExceededClass: unable to find a class that wouldn't exceed limit!");
}

static void CheckForClassesExceedingLimits(team_t team) {
	assert(team == TEAM_RED || team == TEAM_BLUE);
	int i;
	for (i = 0; i < SPC_MAX; i++) {
		int tries = 0;
		while (G_WouldExceedClassLimit(team, i, qfalse, -1, NULL)) {
			// this class's limit is exceeded; switch one player at a time to another class until the limit is no longer exceeded.

			if (tries > MAX_CLIENTS) // avoid infinite loop
				Com_Error(ERR_DROP, va("CheckForClassesExceedingLimits: recursive error for team %s, class %s)!", team == TEAM_RED ? "red" : "blue", classNames[i]));
			tries++;
			
			// method #1: the (dead) player who changed to it most recently gets switched off. should cover most real-world cases.
			int j, mostRecentChange = -1, mostRecentSwitcher = -1;
			for (j = 0; j < MAX_CLIENTS; j++) {
				gentity_t *dude = &g_entities[j];
				if (!dude || !dude->client || dude->client->sess.siegeDesiredTeam != team || bgSiegeClasses[dude->client->siegeClass].playerClass != i || level.tryChangeClass[j].class != i || level.tryChangeClass[j].team != team || (!level.inSiegeCountdown && dude->health > 0 && !(dude->client->tempSpectate > level.time)))
					continue;
				if (level.tryChangeClass[j].time > mostRecentChange) {
					mostRecentChange = level.tryChangeClass[j].time;
					mostRecentSwitcher = j;
				}
			}
			if (mostRecentSwitcher != -1) {
				G_ChangePlayerFromExceededClass(&g_entities[mostRecentSwitcher]);
				continue;
			}

			// method #2: the dead player who changed teams most recently gets switched off.
			int mostRecentTeamChange = -1, mostRecentTeamChanger = -1;
			for (j = 0; j < MAX_CLIENTS; j++) {
				gentity_t *dude = &g_entities[j];
				if (!dude || !dude->client || dude->client->sess.siegeDesiredTeam != team || bgSiegeClasses[dude->client->siegeClass].playerClass != i || (!level.inSiegeCountdown && dude->health > 0 && !(dude->client->tempSpectate > level.time)) || !level.teamChangeTime[j])
					continue;
				if (level.teamChangeTime[j] > mostRecentTeamChange) {
					mostRecentTeamChange = level.teamChangeTime[j];
					mostRecentTeamChanger = j;
				}
			}
			if (mostRecentTeamChanger != -1) {
				G_ChangePlayerFromExceededClass(&g_entities[mostRecentTeamChanger]);
				continue;
			}

			// method #3: the alive player who changed teams most recently gets switched off.
			mostRecentTeamChange = -1, mostRecentTeamChanger = -1;
			for (j = 0; j < MAX_CLIENTS; j++) {
				gentity_t *dude = &g_entities[j];
				if (!dude || !dude->client || dude->client->sess.siegeDesiredTeam != team || bgSiegeClasses[dude->client->siegeClass].playerClass != i || !level.teamChangeTime[j])
					continue;
				if (level.teamChangeTime[j] > mostRecentTeamChange) {
					mostRecentTeamChange = level.teamChangeTime[j];
					mostRecentTeamChanger = j;
				}
			}
			if (mostRecentTeamChanger != -1) {
				G_ChangePlayerFromExceededClass(&g_entities[mostRecentTeamChanger]);
				continue;
			}

			// fallback/sanity check; just pick someone.
			int selectedGuy = -1;
			for (j = 0; j < MAX_CLIENTS; j++) {
				gentity_t *dude = &g_entities[j];
				if (!dude || !dude->client || dude->client->sess.siegeDesiredTeam != team || bgSiegeClasses[dude->client->siegeClass].playerClass != i)
					continue;
				selectedGuy = j;
				break;
			}
			if (selectedGuy != -1) {
				G_ChangePlayerFromExceededClass(&g_entities[selectedGuy]);
				continue;
			}
		}
	}

	// sanity check; make sure that we actually resolved everything (i.e., that we didn't just shift everyone around to other limited classes)
	for (i = 0; i < SPC_MAX; i++) {
		if (G_WouldExceedClassLimit(team, i, qfalse, -1, NULL))
			Com_Error(ERR_DROP, va("CheckForClassesExceedingLimits: unable to resolve class limits for team %s, class %s!", team == TEAM_RED ? "red" : "blue", classNames[i]));
	}
}

void SiegeRespawn(gentity_t *ent)
{
	gentity_t *tent;

	if (g_classLimits.integer)
		CheckForClassesExceedingLimits(ent->client->sess.siegeDesiredTeam);

	level.lastLegitClass[ent - g_entities] = bgSiegeClasses[ent->client->siegeClass].playerClass;

	qboolean needUpdateInfo = g_delayClassUpdate.integer && (Q_stricmp(ent->client->sess.spawnedSiegeClass, ent->client->sess.siegeClass) || (ent->client->siegeClass != -1 && Q_stricmp(ent->client->sess.spawnedSiegeModel, bgSiegeClasses[ent->client->siegeClass].forcedModel))) ? qtrue : qfalse;

	if (ent->client->sess.siegeClass[0])
		Q_strncpyz(ent->client->sess.spawnedSiegeClass, ent->client->sess.siegeClass, sizeof(ent->client->sess.spawnedSiegeClass));
	else
		memset(&ent->client->sess.spawnedSiegeClass, 0, sizeof(ent->client->sess.spawnedSiegeClass));

	{
		siegeClass_t *scl = BG_SiegeFindClassByName(ent->client->sess.spawnedSiegeClass);
		if (scl && scl->forcedModel[0])
			Q_strncpyz(ent->client->sess.spawnedSiegeModel, scl->forcedModel, sizeof(ent->client->sess.spawnedSiegeModel));
		else
			memset(&ent->client->sess.spawnedSiegeModel, 0, sizeof(ent->client->sess.spawnedSiegeModel));

		if (scl)
			ent->client->sess.spawnedSiegeMaxHealth = scl->maxhealth;
	}

	if (((g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none")) || (g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none"))) &&
		g_joinMenuHack.integer && g_joinMenuHack.integer != 2) {
		if (ent->client->siegeClass != -1) {
			siegeClass_t *scl = &bgSiegeClasses[ent->client->siegeClass];
			if (!Q_stricmp(level.mapname, "mp/siege_hoth")) {
				if (scl->playerClass == SPC_INFANTRY && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Imperial Snowtrooper", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_HEAVY_WEAPONS && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rocket Trooper", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_DEMOLITIONIST && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Imperial Demolitionist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_SUPPORT && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Imperial Tech", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_VANGUARD && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Imperial Sniper", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_JEDI && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Jedi Invader", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_INFANTRY && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Infantry", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_HEAVY_WEAPONS && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Wookie", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_DEMOLITIONIST && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Demolitionist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_SUPPORT && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Tech", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_VANGUARD && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Sniper", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_JEDI && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Guardian", sizeof(ent->client->sess.spawnedSiegeClass));
			}
			else if (!Q_stricmp(level.mapname, "mp/siege_desert")) {
				if (scl->playerClass == SPC_INFANTRY && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Bounty Hunter", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_HEAVY_WEAPONS && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Merc Assault Specialist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_DEMOLITIONIST && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Merc Demolitionist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_SUPPORT && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Smuggler", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_VANGUARD && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Merc Sniper", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_JEDI && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Side Marauder", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_INFANTRY && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Assault Specialist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_HEAVY_WEAPONS && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Wookie", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_DEMOLITIONIST && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Demolitionist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_SUPPORT && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Tech", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_VANGUARD && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Rebel Sniper", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_JEDI && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Duelist", sizeof(ent->client->sess.spawnedSiegeClass));
			}
			else if (!Q_stricmp(level.mapname, "mp/siege_korriban")) {
				if (scl->playerClass == SPC_INFANTRY && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Side Mauler", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_HEAVY_WEAPONS && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Jedi Destroyer", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_DEMOLITIONIST && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Jedi Demolitionist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_SUPPORT && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Jedi Tech", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_VANGUARD && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Jedi Interceptor", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_JEDI && ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Dark Jedi Duelist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_INFANTRY && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Warrior", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_HEAVY_WEAPONS && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Knight", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_DEMOLITIONIST && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Demolitionist", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_SUPPORT && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Healer", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_VANGUARD && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Scout", sizeof(ent->client->sess.spawnedSiegeClass));
				else if (scl->playerClass == SPC_JEDI && ent->client->sess.siegeDesiredTeam == TEAM_RED)
					Q_strncpyz(ent->client->sess.spawnedSiegeClass, "Jedi Lightsaber Master", sizeof(ent->client->sess.spawnedSiegeClass));
			}
		}
	}

	if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam)
	{
		SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, qtrue);
	}
	else
	{
		ClientSpawn(ent, needUpdateInfo);
		// add a teleportation effect
		tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = ent->s.clientNum;
	}
}

void SiegeBeginRound(int entNum)
{ //entNum is just used as something to fire targets from.
	char targname[1024];
	memset(&level.lastLegitClass, -1, sizeof(level.lastLegitClass));

	if (!g_preroundState)
	{ //if players are not ingame on round start then respawn them now
		int i = 0;
		gentity_t *ent;
		level.inSiegeCountdown = qfalse;
		level.siegeRoundComplete = qfalse;
		if (!g_siegeTeamSwitch.integer)
			level.siegeStage = SIEGESTAGE_NONE;
		else if (CurrentSiegeRound() == 1)
			level.siegeStage = SIEGESTAGE_ROUND1;
		else
			level.siegeStage = SIEGESTAGE_ROUND2;
		//respawn everyone now
		level.siegeRespawnCheck = level.time + g_siegeRespawn.integer * 1000 - SIEGE_ROUND_BEGIN_TIME - 200;

#ifdef NEWMOD_SUPPORT
		UpdateNewmodSiegeTimers();
#endif

		// determine who needs to be spawned
		qboolean spawnEnt[MAX_CLIENTS] = { qfalse };
		int numBeingSpawnedOnRedTeam = 0;
		for (i = 0; i < MAX_CLIENTS; i++) {
			ent = &g_entities[i];

			if (ent->inuse && ent->client) {
				if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && !(ent->client->ps.pm_flags & PMF_FOLLOW))
					spawnEnt[i] = qtrue; //not a spec, just respawn them
				else if (ent->client->sess.sessionTeam == TEAM_SPECTATOR &&
					(ent->client->sess.siegeDesiredTeam == TEAM_RED || ent->client->sess.siegeDesiredTeam == TEAM_BLUE))
					spawnEnt[i] = qtrue; //spectator but has a desired team
			}

			if (spawnEnt[i]) {
				if (ent->client->sess.siegeDesiredTeam == TEAM_RED)
					numBeingSpawnedOnRedTeam++;
				else if (ent->client->sess.siegeDesiredTeam == TEAM_BLUE)
					SpeedRunModeRuined("SiegeBeginRound: blue");
			}
		}
		if (numBeingSpawnedOnRedTeam > 2)
			SpeedRunModeRuined("SiegeBeginRound: >2 red");

		// switch people off classes that are exceeding their limits
		if (g_classLimits.integer) {
			CheckForClassesExceedingLimits(TEAM_RED);
			CheckForClassesExceedingLimits(TEAM_BLUE);
		}

		// spawn everyone who needs to be spawned
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (spawnEnt[i]) {
				SiegeRespawn(&g_entities[i]);
				level.clients[i].sess.siegeFollowing.wasFollowing = qfalse;
			}
		}

		if (g_autoSpec.integer) {
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (spawnEnt[i])
					continue;
				gclient_t *cl = &level.clients[i];
				if (cl->pers.connected == CON_CONNECTED && cl->sess.siegeFollowing.wasFollowing &&
					cl->sess.siegeFollowing.followingClientNum >= 0 && cl->sess.siegeFollowing.followingClientNum < MAX_CLIENTS &&
					spawnEnt[cl->sess.siegeFollowing.followingClientNum] && level.clients[cl->sess.siegeFollowing.followingClientNum].pers.connected == CON_CONNECTED) {
					cl->sess.spectatorClient = cl->sess.siegeFollowing.followingClientNum;
					cl->sess.spectatorState = SPECTATOR_FOLLOW;
				}
				else {
					cl->sess.siegeFollowing.wasFollowing = qfalse;
				}
			}
		}
	}

	//Now check if there's something to fire off at the round start, if so do it.
	if (BG_SiegeGetPairedValue(siege_info, "roundbegin_target", targname))
	{
		if (targname[0])
		{
			G_UseTargets2(&g_entities[entNum], &g_entities[entNum], targname);
		}
	}

	if (!Q_stricmp(level.mapname, "siege_codes"))
	{
		//hacky fix to remove icons for useless ammo generators that shouldn't be in the map
		SetIconFromClassname("misc_ammo_floor_unit", 2, qfalse);
		SetIconFromClassname("misc_ammo_floor_unit", 3, qfalse);
	}
	else if (!Q_stricmp(level.mapname, "siege_narshaddaa"))
	{
		SetIconFromClassname("misc_ammo_floor_unit", 2, qfalse);
		SetIconFromClassname("misc_ammo_floor_unit", 3, qfalse);
		SetIconFromClassname("misc_shield_floor_unit", 2, qfalse);
		SetIconFromClassname("misc_shield_floor_unit", 3, qfalse);
	}
	else if (!Q_stricmp(level.mapname, "mp/siege_desert"))
	{
		SetIconFromClassname("misc_ammo_floor_unit", 2, qfalse);
		SetIconFromClassname("misc_ammo_floor_unit", 3, qfalse);
		SetIconFromClassname("misc_ammo_floor_unit", 5, qfalse);
		SetIconFromClassname("misc_shield_floor_unit", 2, qfalse);
		SetIconFromClassname("misc_shield_floor_unit", 3, qfalse);
		SetIconFromClassname("misc_shield_floor_unit", 5, qfalse);
		SetIconFromClassname("misc_model_health_power_converter", 2, qfalse);
		SetIconFromClassname("misc_model_health_power_converter", 3, qfalse);
	}

	level.antiLamingTime = 0;


	// if round 1, reset both round 1 and round 2 cvars
	// if round 2, reset only round 2 cvars
	int i, j, currentRound = CurrentSiegeRound();
	for (i = 2; i >= currentRound; i--) {
		trap_Cvar_Set(va("siege_r%i_objscompleted", currentRound), "");
		trap_Cvar_Set(va("siege_r%i_heldformaxat", currentRound), "");
		trap_Cvar_Set(va("siege_r%i_heldformaxtime", currentRound), "");
		trap_Cvar_Set(va("siege_r%i_total", currentRound), "");
		for (j = 1; j <= MAX_STATS - 1; j++) {
			trap_Cvar_Set(va("siege_r%i_obj%i", i, j), "");
		}
		for (j = 0; j < MAX_CLIENTS; j++) {
			level.clients[j].sess.siegeStats.caps[i - 1] = 0;
			level.clients[j].sess.siegeStats.saves[i - 1] = 0;
			level.clients[j].sess.siegeStats.oKills[i - 1] = 0;
			level.clients[j].sess.siegeStats.dKills[i - 1] = 0;
			level.clients[j].sess.siegeStats.oDeaths[i - 1] = 0;
			level.clients[j].sess.siegeStats.dDeaths[i - 1] = 0;
			level.clients[j].sess.siegeStats.oDamageDealt[i - 1] = 0;
			level.clients[j].sess.siegeStats.oDamageTaken[i - 1] = 0;
			level.clients[j].sess.siegeStats.dDamageDealt[i - 1] = 0;
			level.clients[j].sess.siegeStats.dDamageTaken[i - 1] = 0;
			level.clients[j].sess.siegeStats.killer = -1;
			level.clients[j].sess.siegeStats.maxes[i - 1] = 0;
			level.clients[j].sess.siegeStats.maxed[i - 1] = 0;
			level.clients[j].sess.siegeStats.selfkills[i - 1] = 0;
			level.clients[j].sess.siegeStats.oTime[i - 1] = 0;
			level.clients[j].sess.siegeStats.dTime[i - 1] = 0;
			memset(&level.clients[j].sess.siegeStats.mapSpecific[i - 1], 0, sizeof(level.clients[j].sess.siegeStats.mapSpecific[i - 1]));
			level.clients[j].pers.fragsSinceObjStart = 0;
		}
	}

	level.siegeMatchWinner = SIEGEMATCHWINNER_NONE;
	level.endedWithEndMatchCommand = qfalse; // sanity check; probably not needed
	trap_SetConfigstring(CS_SIEGE_STATE, va("0|%i", level.time)); //we're ready to g0g0g0

	// if no ghost camera in world spawn, try to figure one out based on first objective offense spawns
	if (!level.gotGhostCameraFromWorldspawn) {
		if (level.siegeMap == SIEGEMAP_HOTH) {
			static vec3_t cameraPoint = { -438, 17, 496 };
			VectorCopy(cameraPoint, level.ghostCameraOrigin);
			level.ghostCameraYaw = 180;
		}
		else if (level.siegeMap == SIEGEMAP_CARGO && Q_stricmp(level.mapname, "siege_cargobarge")) {
			static vec3_t cameraPoint = { 1820, 989, 168 };
			VectorCopy(cameraPoint, level.ghostCameraOrigin);
			level.ghostCameraYaw = 160;
		}
		else if (level.siegeMap == SIEGEMAP_NAR) {
			static vec3_t cameraPoint = { 141, 2298, 24 };
			VectorCopy(cameraPoint, level.ghostCameraOrigin);
			level.ghostCameraYaw = -90;
		}
		else if (level.siegeMap == SIEGEMAP_URBAN) {
			static vec3_t cameraPoint = { 1254, -2582, 450 };
			VectorCopy(cameraPoint, level.ghostCameraOrigin);
			level.ghostCameraYaw = 0;
		}
		else {

			gentity_t *camera = NULL;

#if 0
			// try to get an initial offense spawnpoint
			for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *ent = &g_entities[i];
				if (!ent->inuse || !ent->genericValue1 || !VALIDSTRING(ent->classname) || Q_stricmp(ent->classname, "info_player_siegeteam1"))
					continue;
				camera = ent;
				break;
			}
#endif

			if (!camera) {
				// use the spectator spawnpoint
				for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
					gentity_t *ent = &g_entities[i];
					if (!ent->inuse || !VALIDSTRING(ent->classname) || Q_stricmp(ent->classname, "info_player_deathmatch"))
						continue;
					camera = ent;
					break;
				}
			}

			if (camera) {
				VectorCopy(camera->r.currentOrigin, level.ghostCameraOrigin);
				//level.ghostCameraOrigin[2] += 100; // bump it up a little higher
				//level.ghostCameraYaw = camera->s.angles[YAW] + 180; // flip the yaw so they can't see anything idk
			}
		}
	}

	if (g_autoRestartAfterIntermission.integer && g_lastIntermissionStartTime.string[0] && Q_isanumber(g_lastIntermissionStartTime.string)) {
		qboolean intermissionOccurredRecently = !!(((int)time(NULL)) - g_lastIntermissionStartTime.integer < 60);
		if (intermissionOccurredRecently) {
			int restartTime = Com_Clampi(5, 30, g_autoRestartAfterIntermission.integer);
			trap_SendConsoleCommand(EXEC_APPEND, va("map_restart %d\n", restartTime));
			trap_Cvar_Set("g_lastIntermissionStartTime", "");
		}
	}
}

void SiegeCheckTimers(void)
{
	int i=0;
	gentity_t *ent;
	int numTeam1 = 0;
	int numTeam2 = 0;

	if (g_gametype.integer != GT_SIEGE)
	{
		return;
	}

	if (level.intermissiontime)
	{
		return;
	}

	if (gSiegeRoundEnded)
	{
		return;
	}

	if (!gSiegeRoundBegun)
	{ //check if anyone is active on this team - if not, keep the timer set up.
		i = 0;

		while (i < MAX_CLIENTS)
		{
			ent = &g_entities[i];

			if (ent && ent->inuse && ent->client &&
				ent->client->pers.connected == CON_CONNECTED &&
				ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1)
			{
				numTeam1++;
			}
			i++;
		}

		i = 0;

		while (i < MAX_CLIENTS)
		{
			ent = &g_entities[i];

			if (ent && ent->inuse && ent->client &&
				ent->client->pers.connected == CON_CONNECTED &&
				ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2)
			{
				numTeam2++;
			}
			i++;
		}

		if (g_siegeTeamSwitch.integer &&
			g_siegePersistant.beatingTime)
		{
			gImperialCountdown = level.time + g_siegePersistant.lastTime;
			gRebelCountdown = level.time + g_siegePersistant.lastTime;
		}
		else
		{
			gImperialCountdown = level.time + imperial_time_limit;
			gRebelCountdown = level.time + rebel_time_limit;
		}
	}

	if (imperial_time_limit)
	{ //team1
		if (gImperialCountdown < level.time) //they were held for 20 minutes, so let's notate this differently
		{
			int round = CurrentSiegeRound();
			trap_Cvar_Set(va("siege_r%i_heldformaxat", round), va("%i", G_FirstIncompleteObjective(round)));
			if (round == 2 && siege_r1_heldformaxat.integer) { //round 2 was held for max, and round 1 was previously held for max
				if (siege_r2_objscompleted.integer > siege_r1_objscompleted.integer)
					level.siegeMatchWinner = SIEGEMATCHWINNER_ROUND2OFFENSE;
				else if (siege_r1_objscompleted.integer > siege_r2_objscompleted.integer)
					level.siegeMatchWinner = SIEGEMATCHWINNER_ROUND1OFFENSE;
				else
					level.siegeMatchWinner = SIEGEMATCHWINNER_TIE;
			}
			PrintObjStat(0, qtrue);
			G_SiegeRoundComplete(SIEGETEAM_TEAM2, ENTITYNUM_NONE, qfalse);
			imperial_time_limit = 0;
			return;
		}
	}

	if (rebel_time_limit)
	{ //team2
		if (gRebelCountdown < level.time)//they were held for 20 minutes, so let's notate this differently
		{
			int round = CurrentSiegeRound();
			trap_Cvar_Set(va("siege_r%i_heldformaxat", round), va("%i", G_FirstIncompleteObjective(round)));
			if (round == 2 && siege_r1_heldformaxat.integer) { //round 2 was held for max, and round 1 was previously held for max
				if (siege_r2_objscompleted.integer > siege_r1_objscompleted.integer)
					level.siegeMatchWinner = SIEGEMATCHWINNER_ROUND2OFFENSE;
				else if (siege_r1_objscompleted.integer > siege_r2_objscompleted.integer)
					level.siegeMatchWinner = SIEGEMATCHWINNER_ROUND1OFFENSE;
				else
					level.siegeMatchWinner = SIEGEMATCHWINNER_TIE;
			}
			PrintObjStat(0, qtrue);
			G_SiegeRoundComplete(SIEGETEAM_TEAM1, ENTITYNUM_NONE, qfalse);
			rebel_time_limit = 0;
			return;
		}
	}

	if (!gSiegeRoundBegun)
	{
		static qboolean forcedInfoReload = qfalse;
		if (!numTeam1 && !numTeam2)
		{ //don't have people on both teams yet.
			memset(&level.lastLegitClass, -1, sizeof(level.lastLegitClass));
			memset(&level.tryChangeClass, -1, sizeof(level.tryChangeClass));
			gSiegeBeginTime = level.time + SIEGE_ROUND_BEGIN_TIME;
			trap_SetConfigstring(CS_SIEGE_STATE, "1"); //"waiting for players on both teams"
			level.inSiegeCountdown = qfalse;
			if (!g_siegeTeamSwitch.integer)
				level.siegeStage = SIEGESTAGE_NONE;
			else if (CurrentSiegeRound() == 1)
				level.siegeStage = SIEGESTAGE_PREROUND1;
			else
				level.siegeStage = SIEGESTAGE_PREROUND2;
		}
		else if (gSiegeBeginTime < level.time)
		{ //mark the round as having begun
			level.siegeRoundStartTime = gSiegeBeginTime;
			if (level.wasRestarted) {
				int numRed = 0, numBlue = 0;
				GetPlayerCounts(qtrue, qtrue, &numRed, &numBlue, NULL, NULL);
				if (numRed && numBlue) {
					if (g_siegeRespawnAutoChange.integer)
						trap_Cvar_Set("g_siegeRespawn", va("%d", level.worldspawnSiegeRespawnTime));
					SpeedRunModeRuined("Round started with players on both teams");
				}
				else {
					if (g_siegeRespawnAutoChange.integer)
						trap_Cvar_Set("g_siegeRespawn", "1");
					LivePugRuined("Round started without players on both teams", qfalse);
				}
			}
			else {
				if (g_siegeRespawnAutoChange.integer)
					trap_Cvar_Set("g_siegeRespawn", "1");
				LivePugRuined("Round started without having been restarted", qfalse);
			}
			gSiegeRoundBegun = qtrue;
			level.inSiegeCountdown = qtrue;
			level.siegeRoundComplete = qfalse;
			SiegeBeginRound(i); //perform any round start tasks
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.skillBoost) {
					trap_SendServerCommand(-1, va("print \"^7%s^7 has a level ^5%d^7 skillboost.\n\"",
						g_entities[i].client->pers.netname, g_entities[i].client->sess.skillBoost));
				}
				if (g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.senseBoost) {
					trap_SendServerCommand(-1, va("print \"^7%s^7 has a level ^5%d^7 senseboost.\n\"",
						g_entities[i].client->pers.netname, g_entities[i].client->sess.senseBoost));
				}
				if (g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.aimBoost) {
					trap_SendServerCommand(-1, va("print \"^7%s^7 has a level ^5%d^7 aimboost.\n\"",
						g_entities[i].client->pers.netname, g_entities[i].client->sess.aimBoost));
				}
			}
			level.canShield[TEAM_RED] = CANSHIELD_YES;
			level.canShield[TEAM_BLUE] = CANSHIELD_YES;
			forcedInfoReload = qfalse;
		}
		else if (gSiegeBeginTime > (level.time + SIEGE_ROUND_BEGIN_TIME))
		{
			memset(&level.lastLegitClass, -1, sizeof(level.lastLegitClass));
			memset(&level.tryChangeClass, -1, sizeof(level.tryChangeClass));
			gSiegeBeginTime = level.time + SIEGE_ROUND_BEGIN_TIME;
			level.inSiegeCountdown = qtrue;
			level.siegeRoundComplete = qfalse;
			if (!g_siegeTeamSwitch.integer)
				level.siegeStage = SIEGESTAGE_NONE;
			else if (CurrentSiegeRound() == 1)
				level.siegeStage = SIEGESTAGE_PREROUND1;
			else
				level.siegeStage = SIEGESTAGE_PREROUND2;
		}
		else
		{
			if (level.wasRestarted) {
				static int ticksCountedDown = 0;
				ticksCountedDown++;
				if (ticksCountedDown >= g_svfps.integer * 4) {
					int numRed = 0, numBlue = 0;
					GetPlayerCounts(qtrue, qtrue, &numRed, &numBlue, NULL, NULL);
					if (numRed && numBlue) {
						if (g_siegeRespawnAutoChange.integer)
							trap_Cvar_Set("g_siegeRespawn", va("%d", level.worldspawnSiegeRespawnTime));
						SpeedRunModeRuined("Countdown ticking down with players on both teams");
					}
					else {
						if (g_siegeRespawnAutoChange.integer)
							trap_Cvar_Set("g_siegeRespawn", "1");
						LivePugRuined("Countdown ticking down without players on both teams", qfalse);
					}
				}
			}
			else {
				if (g_siegeRespawnAutoChange.integer)
					trap_Cvar_Set("g_siegeRespawn", "1");
				LivePugRuined("Countdown ticking down without having been restarted", qfalse);
			}
			memset(&level.lastLegitClass, -1, sizeof(level.lastLegitClass));
			trap_SetConfigstring(CS_SIEGE_STATE, va("2|%i", gSiegeBeginTime - SIEGE_ROUND_BEGIN_TIME)); //getting ready to begin
			level.inSiegeCountdown = qtrue;
			level.siegeRoundComplete = qfalse;
			if (!g_siegeTeamSwitch.integer)
				level.siegeStage = SIEGESTAGE_NONE;
			else if (CurrentSiegeRound() == 1)
				level.siegeStage = SIEGESTAGE_PREROUND1;
			else
				level.siegeStage = SIEGESTAGE_PREROUND2;
			if (!forcedInfoReload && g_delayClassUpdate.integer) { // it's now countdown, so force a resend of userinfo to hide classes for people already joined
				forcedInfoReload = qtrue;
				for (i = 0; i < MAX_CLIENTS; i++) {
					if (level.clients[i].pers.connected && !(g_entities[i].r.svFlags & SVF_BOT))
						ClientUserinfoChanged(i);
				}
			}
		}
	}
}

void SiegeObjectiveCompleted(int team, int objective, int final, int client) {

	int goals_completed, goals_required;

	trap_Cvar_Set(va("siege_r%i_obj%i", CurrentSiegeRound(), objective), va("%i", level.time - level.siegeRoundStartTime));
	if (objective >= 1 && objective <= MAX_SAVED_OBJECTIVES)
		level.objIsComplete[objective - 1] = qtrue;
	PrintObjStat(objective, 0);

	if (objective == 5 && level.siegeMap == SIEGEMAP_HOTH)
	{
		level.hangarCompletedTime = level.time;
	}

	if (objective == 5 && level.siegeMap == SIEGEMAP_CARGO) {
		level.ccCompleted = qtrue;
		for (int i = 0; i < MAX_GENTITIES; i++) { // re-lock 2nd obj inner doors
			gentity_t *ent = &g_entities[i];
			if (!Q_stricmp(ent->target, "2ndobjdoordummy") && !Q_stricmp(ent->classname, "func_door")) {
				ent->spawnflags |= 16; // MOVER_LOCKED
				ent->s.eFlags |= EF_SHADER_ANIM;
				ent->s.frame = 0;
			}
			else if (!Q_stricmp(ent->targetname, "ccreached")) {
				if (ent->use) {
					CheckSiegeHelpFromUse(ent->targetname);
					ent->use(ent, NULL, NULL);
				}
			}
		}
	}

	if (objective == 2 && level.siegeMap == SIEGEMAP_DESERT && level.killerOfLastDesertComputer != NULL && level.killerOfLastDesertComputer->client && level.killerOfLastDesertComputer->client->sess.sessionTeam == TEAM_RED)
	{
		client = level.killerOfLastDesertComputer->s.number;
	}

	if (level.siegeMap == SIEGEMAP_URBAN)
	{
		if (objective == 2) {
			int i;
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *icon = &g_entities[i];
				if (VALIDSTRING(icon->targetname) && !Q_stricmp(icon->targetname, "hackicon") && VALIDSTRING(icon->classname) && !Q_stricmp(icon->classname, "info_siege_radaricon")) {
					icon->s.eFlags &= ~EF_RADAROBJECT;
					break;
				}
			}
		}
		if (objective < 5) {
			int i;
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *sentry = &g_entities[i];
				if (VALIDSTRING(sentry->classname) && !Q_stricmp(sentry->classname, "sentryGun") && level.time - sentry->genericValue8 < (TURRET_LIFETIME - 3000))
						sentry->genericValue8 = level.time - (TURRET_LIFETIME - 3000); // explode in 3 seconds
			}
		}
	}

	if (level.siegeMap == SIEGEMAP_CARGO)
	{
		if (level.totalObjectivesCompleted != 3) {
			int i;
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *sentry = &g_entities[i];
				if (VALIDSTRING(sentry->classname) && !Q_stricmp(sentry->classname, "sentryGun") && level.time - sentry->genericValue8 < (TURRET_LIFETIME - 3000))
					sentry->genericValue8 = level.time - (TURRET_LIFETIME - 3000); // explode in 3 seconds
			}
		}
	}

	if (level.siegeMap == SIEGEMAP_ANSION && objective == 4) { // remove datapad
		gentity_t *item = G_Find(NULL, FOFS(targetname), "alphaisdeadyay");
		if (item) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].holdingObjectiveItem == item - g_entities) {
					level.clients[i].holdingObjectiveItem = 0;
					break;
				}
			}
			G_FreeEntity(item);
		}
	}

	if (client >= 0 && client < MAX_CLIENTS && &level.clients[client] && level.clients[client].pers.connected == CON_CONNECTED)
	{
		level.clients[client].pers.teamState.captures++;
		level.clients[client].rewardTime = level.time + REWARD_SPRITE_TIME;
		level.clients[client].ps.persistant[PERS_CAPTURES]++;
	}

	if (objective) {
		if (client >= 0 && client < MAX_CLIENTS && g_entities[client].client) {
			G_LogPrintf("Objective %i completed by client %i (%s)\n", objective, client, g_entities[client].client->pers.netname);
			g_entities[client].client->sess.siegeStats.caps[GetSiegeStatRound()]++;
		}
		else {
			G_LogPrintf("Objective %i completed by client %i\n", objective, client);
		}
		level.canShield[TEAM_RED] = CANSHIELD_YO_NOTPLACED;
		level.canShield[TEAM_BLUE] = CANSHIELD_YO_NOTPLACED;
	}

	if (gSiegeRoundEnded)
	{
		return;
	}

	//Update the configstring status
	G_SiegeSetObjectiveComplete(team, objective, qfalse);

	if (final != -1)
	{
		if (team == SIEGETEAM_TEAM1)
		{
			imperial_goals_completed++;
		}
		else
		{
			rebel_goals_completed++;
		}
	}

	if (team == SIEGETEAM_TEAM1)
	{
		goals_completed = imperial_goals_completed;
		goals_required = imperial_goals_required;
	}
	else
	{
		goals_completed = rebel_goals_completed;
		goals_required = rebel_goals_required;
	}

	// check top times for this objective
	int ms = G_ObjectiveTimeDifference(objective, CurrentSiegeRound());
	CombinedObjNumber topTimesObjNum = objective;
	// duoTODO: clean this crap up
	if (level.siegeMap == SIEGEMAP_CARGO || level.siegeMap == SIEGEMAP_NAR) {
		if (objective == 3 || objective == 4) {
			static int firstStationTime = -1;
			if (firstStationTime == -1) { // first one destroyed
				firstStationTime = ms;
				topTimesObjNum = 0; // don't run toptimes on this obj
			}
			else { // second one destroyed
				ms += firstStationTime;
				topTimesObjNum = 3;
			}
		}
		else if (objective > 4) {
			topTimesObjNum--;
		}
	}
	else if (level.siegeMap == SIEGEMAP_ANSION || !Q_stricmp(level.mapCaptureRecords.mapname, "mp/siege_alzocIII")) {
		if (objective == 2 || objective == 3) {
			static int firstStationTime = -1;
			if (firstStationTime == -1) { // first one destroyed
				firstStationTime = ms;
				topTimesObjNum = 0; // don't run toptimes on this obj
			}
			else { // second one destroyed
				ms += firstStationTime;
				topTimesObjNum = 2;
			}
		}
		else if (objective > 3) {
			topTimesObjNum--;
		}
	}
	else if (!Q_stricmp(level.mapCaptureRecords.mapname, "mp/siege_destroyer")) {
		if (objective < 6) { // this janky map has FIVE objectives combined into one, wtf
			static int crystalCaptureNumber = 1;
			static int crystalsTime = 0;
			if (crystalCaptureNumber++ <= 4) { // first/second/third/fourth one captured
				crystalsTime += ms;
				topTimesObjNum = 0; // don't run toptimes on this obj
			}
			else { // fifth one captured
				ms += crystalsTime;
				topTimesObjNum = 1;
			}
		}
		else {
			topTimesObjNum = 2;
		}
	}
	else if (level.siegeMap == SIEGEMAP_URBAN) {
		if (objective == 5 || objective == 6) {
			static int firstStationTime = -1;
			if (firstStationTime == -1) { // first one destroyed
				firstStationTime = ms;
				topTimesObjNum = 0; // don't run toptimes on this obj
			}
			else { // second one destroyed
				ms += firstStationTime;
				topTimesObjNum = 5;
			}
		}
	}
	else if (level.siegeMap == SIEGEMAP_KORRIBAN) {
		if (objective == 2 || objective == 3 || objective == 4) {
			static int crystalCaptureNumber = 1;
			static int crystalsTime = 0;
			if (crystalCaptureNumber++ <= 2) { // first/second one captured
				crystalsTime += ms;
				topTimesObjNum = 0; // don't run toptimes on this obj
			}
			else { // third one captured
				ms += crystalsTime;
				topTimesObjNum = 2;
			}
		}
		else if (objective > 4) {
			topTimesObjNum -= 2;
		}
	}
	else if (level.siegeMap == SIEGEMAP_IMPERIAL) {
		if (objective <= 2) {
			static int firstStationTime = -1;
			if (firstStationTime == -1) { // first one destroyed
				firstStationTime = ms;
				topTimesObjNum = 0; // don't run toptimes on this obj
			}
			else { // second one destroyed
				ms += firstStationTime;
				topTimesObjNum = 1;
			}
		}
		else {
			topTimesObjNum -= 1;
		}
	}
	else if (level.worldspawnHasCombinedObjs && objective >= 1 && objective <= MAX_SAVED_OBJECTIVES) {
		qboolean gotIt = qfalse;
		for (int i = 0; i < MAX_SAVED_OBJECTIVES; i++) {
			if (level.worldspawnCombinedObjs[i] == (1 << (objective - 1))) {
				// this combined obj is exactly the obj we just completed
#if 0
				Com_Printf("Obj complete exactly matching a combined obj; running top times on combined obj %d\n", i + 1);
#endif
				topTimesObjNum = i + 1;
				gotIt = qtrue;
				break;
			}
		}
		if (!gotIt) {
			static int savedTime = 0;
			for (int i = 0; i < MAX_SAVED_OBJECTIVES; i++) {
				if (level.worldspawnCombinedObjs[i] & (1 << (objective - 1))) {
					// this combined obj includes the obj we just completed
					qboolean combinedObjHasAllObjsCompleted = qtrue;
					for (int j = 0; j < MAX_SAVED_OBJECTIVES; j++) {
						if (level.worldspawnCombinedObjs[i] & (1 << j) && !level.objIsComplete[j]) {
							combinedObjHasAllObjsCompleted = qfalse;
							break;
						}
					}
					if (combinedObjHasAllObjsCompleted) {
						// the combined obj has had all sub-objs completed; run top times
#if 0
						Com_Printf("Obj complete and all sub-objs complete; running toptimes on combined obj %d with time savedTime %d + ms %d == %d\n", i + 1, savedTime, ms, savedTime + ms);
#endif
						topTimesObjNum = i + 1;
						ms += savedTime;
						savedTime = 0;
					}
					else {
						// don't run top times yet; the combined obj has not yet had all sub-objs completed
#if 0
						Com_Printf("Obj complete but not all sub-objs of combined obj; saving obj time: %d += %d == %d\n", savedTime, ms, savedTime + ms);
#endif
						savedTime += ms;
						topTimesObjNum = 0;
					}
					break;
				}
			}
		}
	}

	if (topTimesObjNum >= 1 && topTimesObjNum <= MAX_SAVED_OBJECTIVES) {
		CheckTopTimes(ms, topTimesObjNum, client >= 0 && client < MAX_CLIENTS ? client : -1);
		CheckDefenseRecords(ms, topTimesObjNum);
		level.mapCaptureRecords.lastCombinedObjCompleted = topTimesObjNum;
	}

	if (final == 1 || goals_completed >= goals_required)
	{
		SiegeBroadcast_OBJECTIVECOMPLETE(team, client, objective);
		G_SiegeRoundComplete(team, client, qtrue);
	}
	else if (g_siegeTiebreakEnd.integer && g_siegePersistant.beatingTime && g_siegeTeamSwitch.integer && siege_r2_objscompleted.integer >= siege_r1_objscompleted.integer) {
		SiegeBroadcast_OBJECTIVECOMPLETE(team, client, objective);
		G_SiegeRoundComplete(team, client, qfalse);
	}
	else
	{
		BroadcastObjectiveCompletion(team, objective, final, client);
	}

	// reset "has changed class" bool for each obj
	for (int i = 0; i < MAX_CLIENTS; i++) {
		level.siegeTopTimes[i].hasChangedClass = qfalse;
		level.siegeTopTimes[i].classOnFile = 0;
	}
	level.forceCheckClassTime = level.time + 500;
}

void siegeTriggerUse(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	char			teamstr[64];
	char			objectivestr[64];
	char			desiredobjective[MAX_SIEGE_INFO_SIZE];
	int				clUser = ENTITYNUM_NONE;
	int				final = 0;
	int				i = 0;
	int				n;
	int				x;

	if (!siege_valid)
	{
		return;
	}

	if (level.siegeRoundComplete) // no completing objs while the round is already over
		return;

	if (level.siegeMap == SIEGEMAP_HOTH)
	{
		if (ent->objective <= 5)
		{
			for (n = 0; n < MAX_GENTITIES; n++)
			{
				if (&g_entities[n] && g_entities[n].objective && g_entities[n].objective == (ent->objective + 1))
				{
					//upon completion of a non-final objective, find the next objective and turn on its radar icon (fix for poor mapping practice on Hoth)
					g_entities[n].s.eFlags |= EF_RADAROBJECT;
				}
			}
		}
	}
	else if (level.siegeMap == SIEGEMAP_NAR)
	{
		if (ent->objective == 1 || ent->objective == 4 || ent->objective == 5)
		{
			for (n = 0; n < MAX_GENTITIES; n++)
			{
				if (&g_entities[n] && g_entities[n].objective && g_entities[n].objective == (ent->objective + 1))
				{
					//upon completion of a non-final objective, find the next objective and turn on its radar icon (fix for poor mapping practice on Nar)
					g_entities[n].s.eFlags |= EF_RADAROBJECT;
				}
			}
			if (ent->objective == 5)
			{
				//enable codes icon
				SetIconFromClassname("misc_siege_item", 1, qtrue);
			}
		}
		else if (ent->objective == 2)
		{
			for (n = 0; n < MAX_GENTITIES; n++)
			{
				if (&g_entities[n] && g_entities[n].objective && (g_entities[n].objective == 3 || g_entities[n].objective == 4))
				{
					//activate both station icons for nar, since these two objs can be completed in any order
					g_entities[n].s.eFlags |= EF_RADAROBJECT;
				}
			}
		}
	}
	else if (level.siegeMap == SIEGEMAP_DESERT)
	{
		if (ent->objective == 1)
		{
		}
		else if (ent->objective == 2 && !level.wallCompleted) //objective 2 is targeted to enable its icon
		{
			ent->s.eFlags |= EF_RADAROBJECT;
			return;
		}
		else if (ent->objective == 2 && level.wallCompleted) //objective 2 is targeted because it has actually been completed
		{
		}
		else if (!(ent->s.eFlags & EF_RADAROBJECT))
		{	//toggle radar on and exit if it is not showing up already
			//we still need to support the default behavior for this map (targeting inactive siege objs simply causes the icon to activate rather than completing the obj)
			ent->s.eFlags |= EF_RADAROBJECT;
			return;
		}
	}
	else if (level.siegeMap == SIEGEMAP_CARGO)
	{
		if (ent->objective == 6)
		{
			for (x = 0; x < MAX_GENTITIES; x++)
			{
				if (&g_entities[x] && g_entities[x].classname && g_entities[x].classname[0] && !Q_stricmp(g_entities[x].classname, "info_siege_radaricon") &&
					g_entities[x].targetname && g_entities[x].targetname[0] && Q_stricmp(g_entities[x].targetname, "wedidthenewhack") && Q_stricmp(g_entities[x].targetname, "ventcentergens"))
				{
					//remove all the now-useless hack icons once we reach the last obj
					g_entities[x].s.eFlags &= ~EF_RADAROBJECT;
					G_FreeEntity(&g_entities[x]);
				}
			}
		}
		if (!(ent->s.eFlags & EF_RADAROBJECT))
		{ //toggle radar on and exit if it is not showing up already
			ent->s.eFlags |= EF_RADAROBJECT;
			return;
		}
	}
	else
	{
		if (!(ent->s.eFlags & EF_RADAROBJECT))
		{ //toggle radar on and exit if it is not showing up already
			ent->s.eFlags |= EF_RADAROBJECT;
			return;
		}
	}

	level.totalObjectivesCompleted++;
	level.previousObjectiveCompleted = level.objectiveJustCompleted;
	level.objectiveJustCompleted = ent->objective;
	char *roundCvar = va("siege_r%i_objscompleted", CurrentSiegeRound());
	trap_Cvar_Set(roundCvar, va("%i", trap_Cvar_VariableIntegerValue(roundCvar) + 1));

	if (level.siegeMap == SIEGEMAP_HOTH)
	{
		if (ent->objective == 1)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 5, qtrue);
			SetIconFromClassname("misc_shield_floor_unit", 2, qtrue);
		}
		else if (ent->objective == 3)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 5, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 2, qfalse);
			SetIconFromClassname("misc_ammo_floor_unit", 4, qtrue);
			SetIconFromClassname("misc_shield_floor_unit", 1, qtrue);
		}
		else if (ent->objective == 4)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 4, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 1, qfalse);
		}
		else if (ent->objective == 5)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 1, qtrue);
			SetIconFromClassname("misc_ammo_floor_unit", 2, qtrue);
			SetIconFromClassname("misc_ammo_floor_unit", 3, qtrue);
		}
	}
	else if (level.siegeMap == SIEGEMAP_NAR)
	{
		if (ent->objective == 1)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 3, qtrue);
			SetIconFromClassname("misc_shield_floor_unit", 2, qtrue);
		}
		else if (ent->objective == 2)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 1, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 1, qfalse);
		}
		else if ((ent->objective == 3 || ent->objective == 4) && level.totalObjectivesCompleted == 4)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 1, qfalse);
			SetIconFromClassname("misc_ammo_floor_unit", 3, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 2, qfalse);
		}
		else if (ent->objective == 5)
		{
			SetIconFromClassname("misc_siege_icon", 1, qtrue);
			SetIconFromClassname("misc_ammo_floor_unit", 2, qtrue);
			SetIconFromClassname("misc_shield_floor_unit", 3, qtrue);
		}
	}
	else if (level.siegeMap == SIEGEMAP_DESERT)
	{
		if (ent->objective == 1)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 1, qfalse);
			SetIconFromClassname("misc_ammo_floor_unit", 4, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 1, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 4, qfalse);
			SetIconFromClassname("misc_model_health_power_converter", 1, qfalse);
			SetIconFromClassname("misc_ammo_floor_unit", 2, qtrue);
			SetIconFromClassname("misc_ammo_floor_unit", 3, qtrue);
			SetIconFromClassname("misc_shield_floor_unit", 2, qtrue);
			SetIconFromClassname("misc_shield_floor_unit", 3, qtrue);
			SetIconFromClassname("misc_model_health_power_converter", 2, qtrue);
		}
		else if (ent->objective == 2)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 2, qfalse);
			SetIconFromClassname("misc_ammo_floor_unit", 3, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 2, qfalse);
			SetIconFromClassname("misc_shield_floor_unit", 3, qfalse);
			SetIconFromClassname("misc_model_health_power_converter", 2, qfalse);
			for (x = 0; x < MAX_GENTITIES; x++) //get rid of the wall icons if they are still up
			{
				if (&g_entities[x] && !Q_stricmp(g_entities[x].classname, "info_siege_radaricon") && !Q_stricmpn(g_entities[x].targetname, "walldestroy", 11))
				{
					g_entities[x].s.eFlags &= ~EF_RADAROBJECT;
				}
			}
		}
		else if (ent->objective == 3)
		{
			SetIconFromClassname("misc_ammo_floor_unit", 5, qtrue);
			SetIconFromClassname("misc_shield_floor_unit", 5, qtrue);
			SetIconFromClassname("misc_model_health_power_converter", 3, qtrue);
		}
	}



	if (activator) { //activator will hopefully be the person who triggered this event
		if (activator->s.NPC_class == CLASS_VEHICLE) { // if a vehicle captured the objective, the pilot should be credited for the objective
			if (activator->m_pVehicle && activator->m_pVehicle->m_pPilot) {
				clUser = activator->m_pVehicle->m_pPilot->s.number; // we have a pilot
			}
			else if (activator->lastPilot && activator->lastPilot - g_entities >= 0 && activator->lastPilot - g_entities < MAX_CLIENTS &&
				activator->lastPilot->client && activator->lastPilot->client->pers.connected == CON_CONNECTED) {
				clUser = activator->lastPilot->s.number; // the vehicle has no pilot, but it previously had a pilot (maybe the vehicle's own momentum carried it in)
			}
		}
		else if (activator->client) {
			clUser = activator->s.number;
		}
	}

	if (ent->side == SIEGETEAM_TEAM1)
	{
		Com_sprintf(teamstr, sizeof(teamstr), team1);
	}
	else
	{
		Com_sprintf(teamstr, sizeof(teamstr), team2);
	}

	if (BG_SiegeGetValueGroup(siege_info, teamstr, gParseObjectives))
	{
		Com_sprintf(objectivestr, sizeof(objectivestr), "Objective%i", ent->objective);

		if (BG_SiegeGetValueGroup(gParseObjectives, objectivestr, desiredobjective))
		{
			if (BG_SiegeGetPairedValue(desiredobjective, "final", teamstr))
			{
				final = atoi(teamstr);
			}

			if (BG_SiegeGetPairedValue(desiredobjective, "target", teamstr))
			{
				while (teamstr[i])
				{
					if (teamstr[i] == '\r' ||
						teamstr[i] == '\n')
					{
						teamstr[i] = '\0';
					}

					i++;
				}
				UseSiegeTarget(other, activator, teamstr);
			}

			if (ent->target && ent->target[0])
			{ //use this too
				UseSiegeTarget(other, activator, ent->target);
			}

			SiegeObjectiveCompleted(ent->side, ent->objective, final, clUser);
		}
	}

	if (ent->objective == 1 && level.siegeMap == SIEGEMAP_DESERT)
	{
		//second obj icon should always be disabled
		SetIconFromClassname("info_siege_objective", 2, qfalse);
		level.wallCompleted = qtrue;
	}

	ent->genericValue17 = qtrue; // shitty hack to mark it as complete
}

/*QUAKED info_siege_objective (1 0 1) (-16 -16 -24) (16 16 32) ? x x STARTOFFRADAR
STARTOFFRADAR - start not displaying on radar, don't display until used.

"objective" - specifies the objective to complete upon activation
"side" - set to 1 to specify an imperial goal, 2 to specify rebels
"icon" - icon that represents the objective on the radar
*/
void SP_info_siege_objective(gentity_t *ent)
{
	char* s;

	if (!siege_valid || g_gametype.integer != GT_SIEGE)
	{
		G_FreeEntity(ent);
		return;
	}

	ent->use = siegeTriggerUse;
	G_SpawnInt( "objective", "0", &ent->objective);
	G_SpawnInt( "side", "0", &ent->side);

	if (!ent->objective || !ent->side)
	{ //j00 fux0red something up
		G_FreeEntity(ent);
		G_Printf("ERROR: info_siege_objective without an objective or side value\n");
		return;
	}

	if (!level.numSiegeObjectivesOnMap || ent->objective > level.numSiegeObjectivesOnMap)
		level.numSiegeObjectivesOnMap = ent->objective;

	if (level.siegeMap == SIEGEMAP_HOTH)
	{
		if (ent->objective == 1)
		{
			//objective 1 on hoth
			ent->s.eFlags |= EF_RADAROBJECT;
		}
		else
		{
			//objectives 2-6 on hoth
			ent->s.eFlags &= ~EF_RADAROBJECT;
		}
	}
	else if (level.siegeMap == SIEGEMAP_NAR)
	{
		if (ent->objective == 1)
		{
			//objective 1 on nar
			ent->s.eFlags |= EF_RADAROBJECT;
		}
		else
		{
			//objectives 2-6 on nar
			ent->s.eFlags &= ~EF_RADAROBJECT;
		}
	}
	else if (level.siegeMap == SIEGEMAP_DESERT)
	{
		//they all start off radar
		//(obj 1 uses separate icons for each (left/right) wall segment
		ent->s.eFlags &= ~EF_RADAROBJECT;
	}
	else
	{
		//all other maps
		//Set it up to be drawn on radar
		if (!(ent->spawnflags & SIEGEITEM_STARTOFFRADAR))
		{
			ent->s.eFlags |= EF_RADAROBJECT;
		}
	}

	//All clients want to know where it is at all times for radar
	ent->r.svFlags |= SVF_BROADCAST;

	G_SpawnString( "icon", "", &s );
	
	if (s && s[0])
	{ 
		// We have an icon, so index it now.  We are reusing the genericenemyindex
		// variable rather than adding a new one to the entity state.
		ent->s.genericenemyindex = G_IconIndex(s);
	}

	ent->s.brokenLimbs = ent->side;
	ent->s.frame = ent->objective;
	trap_LinkEntity(ent);
}


void SiegeIconUse(gentity_t *ent, gentity_t *other, gentity_t *activator)
{ //toggle it on and off
	if (ent->s.eFlags & EF_RADAROBJECT)
	{
		ent->s.eFlags &= ~EF_RADAROBJECT;
		ent->r.svFlags &= ~SVF_BROADCAST;
	}
	else
	{
		ent->s.eFlags |= EF_RADAROBJECT;
		ent->r.svFlags |= SVF_BROADCAST;
	}
}

/*QUAKED info_siege_radaricon (1 0 1) (-16 -16 -24) (16 16 32) ? 
Used to arbitrarily display radar icons at placed location. Can be used
to toggle on and off.

"icon" - icon that represents the objective on the radar
"startoff" - if 1 start off
*/
void SP_info_siege_radaricon (gentity_t *ent)
{
	char* s;
	int i;

	if (!siege_valid || g_gametype.integer != GT_SIEGE)
	{
		G_FreeEntity(ent);
		return;
	}

	G_SpawnInt("startoff", "0", &i);

	if (!i)
	{ //start on then
		ent->s.eFlags |= EF_RADAROBJECT;
		ent->r.svFlags |= SVF_BROADCAST;
	}

	G_SpawnString( "icon", "", &s );
	if (!s || !s[0])
	{ //that's the whole point of the entity
        Com_Error(ERR_DROP, "misc_siege_radaricon without an icon");
		return;
	}

	ent->classname = "info_siege_radaricon";

	ent->use = SiegeIconUse;

	ent->s.genericenemyindex = G_IconIndex(s);

	trap_LinkEntity(ent);
}

void decompTriggerUse(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	int final = 0;
	char teamstr[1024];
	char objectivestr[64];
	char desiredobjective[MAX_SIEGE_INFO_SIZE];

	if (gSiegeRoundEnded)
	{
		return;
	}

	if (!G_SiegeGetCompletionStatus(ent->side, ent->objective))
	{ //if it's not complete then there's nothing to do here
		return;
	}

	//Update the configstring status
	G_SiegeSetObjectiveComplete(ent->side, ent->objective, qtrue);

	//Find out if this objective counts toward the final objective count
   	if (ent->side == SIEGETEAM_TEAM1)
	{
		Com_sprintf(teamstr, sizeof(teamstr), team1);
	}
	else
	{
		Com_sprintf(teamstr, sizeof(teamstr), team2);
	}

	if (BG_SiegeGetValueGroup(siege_info, teamstr, gParseObjectives))
	{
		Com_sprintf(objectivestr, sizeof(objectivestr), "Objective%i", ent->objective);

		if (BG_SiegeGetValueGroup(gParseObjectives, objectivestr, desiredobjective))
		{
			if (BG_SiegeGetPairedValue(desiredobjective, "final", teamstr))
			{
				final = atoi(teamstr);
			}
		}
	}

	//Subtract the goal num if applicable
	if (final != -1)
	{
		if (ent->side == SIEGETEAM_TEAM1)
		{
			imperial_goals_completed--;
		}
		else
		{
			rebel_goals_completed--;
		}
	}
}

/*QUAKED info_siege_decomplete (1 0 1) (-16 -16 -24) (16 16 32)
"objective" - specifies the objective to decomplete upon activation
"side" - set to 1 to specify an imperial (team1) goal, 2 to specify rebels (team2)
*/
void SP_info_siege_decomplete (gentity_t *ent)
{
	if (!siege_valid || g_gametype.integer != GT_SIEGE)
	{
		G_FreeEntity(ent);
		return;
	}

	ent->use = decompTriggerUse;
	G_SpawnInt( "objective", "0", &ent->objective);
	G_SpawnInt( "side", "0", &ent->side);

	if (!ent->objective || !ent->side)
	{ //j00 fux0red something up
		G_FreeEntity(ent);
		G_Printf("ERROR: info_siege_objective_decomplete without an objective or side value\n");
		return;
	}
}

void siegeEndUse(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	LogExit("Round ended");
}

/*QUAKED target_siege_end (1 0 1) (-16 -16 -24) (16 16 32)
Do a logexit for siege when used.
*/
void SP_target_siege_end (gentity_t *ent)
{
	if (!siege_valid || g_gametype.integer != GT_SIEGE)
	{
		G_FreeEntity(ent);
		return;
	}

	ent->use = siegeEndUse;
}

void SiegeItemRemoveOwner(gentity_t *ent, gentity_t *carrier)
{
	ent->genericValue2 = 0; //Remove picked-up flag
	ent->genericValue8 = ENTITYNUM_NONE; //Mark entity carrying us as none

	if (carrier)
	{
		carrier->client->holdingObjectiveItem = 0; //The carrier is no longer carrying us
		if (ent->genericValue15)
		{
			carrier->client->ps.fd.forcePowerRegenDebounceTime = 0; //start regenerating force immediately
		}
		carrier->r.svFlags &= ~SVF_BROADCAST;
#ifdef NEWMOD_SUPPORT
		UpdateNewmodSiegeItems();
#endif
	}
}

static void SiegeItemRespawnEffect(gentity_t *ent, vec3_t newOrg)
{
	ent->siegeItemSpawnTime = level.time;
	vec3_t upAng;

	if (ent->target5 && ent->target5[0])
	{
		G_UseTargets2(ent, ent, ent->target5);
	}

	if (!ent->genericValue10)
	{ //no respawn effect
		return;
	}

	VectorSet(upAng, 0, 0, 1);

	//Play it once on the current origin, and once on the origin we're respawning to.
	G_PlayEffectID(ent->genericValue10, ent->r.currentOrigin, upAng);
	G_PlayEffectID(ent->genericValue10, newOrg, upAng);
}

void SiegeItemRespawnOnOriginalSpot(gentity_t *ent, gentity_t *carrier, gentity_t *overrideOriginEnt)
{
	if (overrideOriginEnt) {
		SiegeItemRespawnEffect(ent, overrideOriginEnt->r.currentOrigin);
		G_SetOrigin(ent, overrideOriginEnt->r.currentOrigin);
	}
	else {
		SiegeItemRespawnEffect(ent, ent->pos1);
		G_SetOrigin(ent, ent->pos1);
	}

	SiegeItemRemoveOwner(ent, carrier);
	
	// Stop the item from flashing on the radar
	ent->s.time2 = 0;
}

void SiegeItemThink(gentity_t *ent)
{
	gentity_t *carrier = NULL;

	if (ent->genericValue12)
	{ //recharge health
		if (ent->health > 0 && ent->health < ent->maxHealth && ent->genericValue14 < level.time)
		{
            ent->genericValue14 = level.time + ent->genericValue13;
			ent->health += ent->genericValue12;
			if (ent->health > ent->maxHealth)
			{
				ent->health = ent->maxHealth;
			}
		}
	}

	if (ent->genericValue8 != ENTITYNUM_NONE)
	{ //Just keep sticking it on top of the owner. We need it in the same PVS as him so it will render bolted onto him properly.
		carrier = &g_entities[ent->genericValue8];

		if (carrier->inuse && carrier->client)
		{
			if (ent->specialIconTreatment)
			{
				ent->s.eFlags &= ~EF_RADAROBJECT;
			}
			VectorCopy(carrier->client->ps.origin, ent->r.currentOrigin);
			trap_LinkEntity(ent);
		}
	}
	else if (ent->genericValue1)
	{ //this means we want to run physics on the object
		G_RunExPhys(ent, ent->radius, ent->mass, ent->random, qfalse, NULL, 0);
	}

	//Bolt us to whoever is carrying us if a client
	if (ent->genericValue8 < MAX_CLIENTS)
	{
		ent->s.boltToPlayer = ent->genericValue8+1;
	}
	else
	{
		ent->s.boltToPlayer = 0;
	}


	if (carrier)
	{
		gentity_t *carrier = &g_entities[ent->genericValue8];

		//This checking can be a bit iffy on the death stuff, but in theory we should always
		//get a think in before the default minimum respawn time is exceeded.
		if (!carrier->inuse || !carrier->client ||
			(carrier->client->sess.sessionTeam != SIEGETEAM_TEAM1 && carrier->client->sess.sessionTeam != SIEGETEAM_TEAM2) ||
			(carrier->client->ps.pm_flags & PMF_FOLLOW))
		{ //respawn on the original spot
			ent->siegeItemCarrierTime = 0;
			if (ent->specialIconTreatment)
			{
				ent->s.eFlags |= EF_RADAROBJECT;
			}
			SiegeItemRespawnOnOriginalSpot(ent, NULL, NULL);
		}
		else if (carrier->health < 1)
		{ //The carrier died so pop out where he is (unless in nodrop).
			ent->siegeItemCarrierTime = 0;
			if (VALIDSTRING(ent->target6)) {
				if (level.siegeMap == SIEGEMAP_DESERT && !Q_stricmp(ent->target6, "c3podroppedprint") && VALIDSTRING(ent->model)) { // droid parts on desert get unique messages
					char *part = "part";
					if (stristr(ent->model, "arm")) part = "arm";
					else if (stristr(ent->model, "head")) part = "head";
					else if (stristr(ent->model, "leg")) part = "leg";
					else if (stristr(ent->model, "torso")) part = "torso";
					trap_SendServerCommand(-1, va("cp \"Protocol droid %s has been dropped!\n\"", part));
				}
				else
				{
					G_UseTargets2(ent, ent, ent->target6);
				}
			}
			if (ent->specialIconTreatment)
			{
				ent->s.eFlags |= EF_RADAROBJECT;
			}
			if ( trap_PointContents(carrier->client->ps.origin, carrier->s.number) & CONTENTS_NODROP )
			{ //In nodrop land, go back to the original spot.
				SiegeItemRespawnOnOriginalSpot(ent, carrier, NULL);
				if (!ent->genericValue16) //hacky...
				{
					ent->s.eFlags |= EF_NODRAW;
					ent->genericValue11 = 0;
					ent->s.eFlags &= ~EF_RADAROBJECT;
				}

				// see if we need to respawn a "pick up this item" siege help message
				qboolean needUpdate = qfalse;
				iterator_t iter = { 0 };
				ListIterate(&level.siegeHelpList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					siegeHelp_t *help = IteratorNext(&iter);
					if (help->ended)
						continue;
					if (help->started && help->item[0] && (!Q_stricmp(ent->targetname, help->item) || !Q_stricmp(ent->goaltarget, help->item))) {
						help->forceHideItem = qtrue;
						needUpdate = qtrue;
					}
				}
				if (needUpdate)
					level.siegeHelpMessageTime = -SIEGE_HELP_INTERVAL;
			}
			else
			{
				G_SetOrigin(ent, carrier->client->ps.origin);
				ent->epVelocity[0] = Q_irand(-80, 80);
				ent->epVelocity[1] = Q_irand(-80, 80);
				ent->epVelocity[2] = Q_irand(40, 80);

				//We're in a nonstandard place, so if we go this long without being touched,
				//assume we may not be reachable and respawn on the original spot.
				ent->genericValue9 = level.time + ent->genericValue17;

				SiegeItemRemoveOwner(ent, carrier);
			}
		}
		// update siege item carry time
		else if (ent->siegeItemCarrierTime && carrier->client && carrier - g_entities >= 0 && carrier - g_entities < MAX_CLIENTS) {
			if (level.siegeMap == SIEGEMAP_HOTH)
				carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_HOTH_CODESTIME] += (level.time - ent->siegeItemCarrierTime);
			else if (level.siegeMap == SIEGEMAP_DESERT)
				carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_PARTSTIME] += (level.time - ent->siegeItemCarrierTime);
			else if (level.siegeMap == SIEGEMAP_KORRIBAN && VALIDSTRING(ent->goaltarget)) {
				if (!Q_stricmp(ent->goaltarget, "bluecrystaldelivery"))
					carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_BLUETIME] += (level.time - ent->siegeItemCarrierTime);
				else if (!Q_stricmp(ent->goaltarget, "greencrystaldelivery"))
					carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_GREENTIME] += (level.time - ent->siegeItemCarrierTime);
				else if (!Q_stricmp(ent->goaltarget, "redcrystaldelivery"))
					carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_REDTIME] += (level.time - ent->siegeItemCarrierTime);
				else if (!Q_stricmp(ent->goaltarget, "staffplace"))
					carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_SCEPTERTIME] += (level.time - ent->siegeItemCarrierTime);
			}
			else if (level.siegeMap == SIEGEMAP_NAR)
				carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_NAR_CODESTIME] += (level.time - ent->siegeItemCarrierTime);
			else if (level.siegeMap == SIEGEMAP_CARGO)
				carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_CARGO2_CODESTIME] += (level.time - ent->siegeItemCarrierTime);
			else if (level.siegeMap == SIEGEMAP_BESPIN)
				carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_BESPIN_CODESTIME] += (level.time - ent->siegeItemCarrierTime);
			else if (level.siegeMap == SIEGEMAP_URBAN)
				carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_URBAN_MONEYTIME] += (level.time - ent->siegeItemCarrierTime);
			else if (level.siegeMap == SIEGEMAP_ANSION)
				carrier->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_ANSION_CODESTIME] += (level.time - ent->siegeItemCarrierTime);
			ent->siegeItemCarrierTime = level.time;
		}
	}
	else
		ent->siegeItemCarrierTime = 0;

	if (ent->genericValue9 && ent->genericValue9 < level.time && ent->genericValue17 != -1)
	{ //time to respawn on the original spot then
		ent->siegeItemCarrierTime = 0;
		if (ent->specialIconTreatment)
		{
			ent->s.eFlags |= EF_RADAROBJECT;
		}
		SiegeItemRespawnEffect(ent, ent->pos1);
		G_SetOrigin(ent, ent->pos1);
		if (!ent->genericValue16) //hacky...
		{
			ent->s.eFlags |= EF_NODRAW;
			ent->genericValue11 = 0;
			ent->s.eFlags &= ~EF_RADAROBJECT;
		}
		ent->genericValue9 = 0;
		
		// stop flashing on radar
		ent->s.time2 = 0;

		// see if we need to respawn a "pick up this item" siege help message
		qboolean needUpdate = qfalse;
		iterator_t iter = { 0 };
		ListIterate(&level.siegeHelpList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			siegeHelp_t *help = IteratorNext(&iter);
			if (help->ended)
				continue;
			if (help->started && help->item[0] && (!Q_stricmp(ent->targetname, help->item) || !Q_stricmp(ent->goaltarget, help->item))) {
				help->forceHideItem = qfalse;
				needUpdate = qtrue;
			}
		}
		if (needUpdate)
			level.siegeHelpMessageTime = -SIEGE_HELP_INTERVAL;
	}

	ent->nextthink = level.time + FRAMETIME/2;
}

void SiegeItemTouch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	short i1;
	if (!other || !other->inuse ||
		!other->client || other->s.eType == ET_NPC)
	{
		if (trace && trace->startsolid && (g_floatingItems.integer || self->siegeItemSpawnTime && level.time - self->siegeItemSpawnTime <= 1000)) // duo: allow items to float for 1sec after spawning, even with g_floatingItems set to 0
		{ //let me out! (ideally this should not happen, but such is life)
			vec3_t escapePos;
			VectorCopy(self->r.currentOrigin, escapePos);
			escapePos[2] += 1.0f;

			//I hope you weren't stuck in the ceiling.
			G_SetOrigin(self, escapePos);
		}
		return;
	}

	if (other->health < 1)
	{ //dead people can't pick us up.
		return;
	}

	if (other->client->holdingObjectiveItem)
	{ //this guy's already carrying a siege item
		return;
	}

	if ( other->client->ps.pm_type == PM_SPECTATOR )
	{//spectators don't pick stuff up
		return;
	}

	if (self->genericValue2)
	{ //Am I already picked up?
		return;
	}

	if (self->genericValue6 == other->client->sess.sessionTeam)
	{ //Set to not be touchable by players on this team.
		return;
	}

	if (!gSiegeRoundBegun)
	{ //can't pick it up if round hasn't started yet
		return;
	}

	if (other->client->sess.siegeDuelInProgress)
	{
		//people in a duel can't pick us up
		return;
	}

	if (self->s.eFlags & EF_NODRAW)
	{
		return;
	}

	if (level.zombies && level.siegeMap == SIEGEMAP_CARGO)
	{
		return;
	}

	if (self->idealClassType && self->idealClassType >= CLASSTYPE_ASSAULT && other->client->sess.sessionTeam == TEAM_RED)
	{
		i1 = bgSiegeClasses[other->client->siegeClass].playerClass;
		if (i1 + 10 != self->idealClassType)
		{
			return;
		}
	}

	if (self->idealClassTypeTeam2 && self->idealClassTypeTeam2 >= CLASSTYPE_ASSAULT && other->client->sess.sessionTeam == TEAM_BLUE)
	{
		i1 = bgSiegeClasses[other->client->siegeClass].playerClass;
		if (i1 + 10 != self->idealClassTypeTeam2)
		{
			return;
		}
	}

	if (other->client->emoted)
		return;

	if (self->noise_index)
	{ //play the pickup noise.
		G_Sound(other, CHAN_AUTO, self->noise_index);
	}

	if (level.siegeMap != SIEGEMAP_UNKNOWN && level.siegeMap != SIEGEMAP_IMPERIAL && level.siegeMap != SIEGEMAP_DUEL)
		self->siegeItemCarrierTime = level.time;
	else
		self->siegeItemCarrierTime = 0;

	self->genericValue2 = 1; //Mark it as picked up.

	other->client->holdingObjectiveItem = self->s.number;
	other->r.svFlags |= SVF_BROADCAST; //broadcast player while he carries this
	self->genericValue8 = other->s.number; //Keep the index so we know who is "carrying" us

	self->genericValue9 = 0; //So it doesn't think it has to respawn.
	
	// see if we need to kill a "pick up this item" siege help message
	qboolean needUpdate = qfalse;
	iterator_t iter = { 0 };
	ListIterate(&level.siegeHelpList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		siegeHelp_t *help = IteratorNext(&iter);
		if (help->ended)
			continue;
		if (help->started && help->item[0] && (!Q_stricmp(self->targetname, help->item) || !Q_stricmp(self->goaltarget, help->item))) {
			help->forceHideItem = qtrue;
			needUpdate = qtrue;
		}
	}
	if (needUpdate)
		level.siegeHelpMessageTime = -SIEGE_HELP_INTERVAL;

#ifdef NEWMOD_SUPPORT
	UpdateNewmodSiegeItems();
#endif

	if (self->target2 && self->target2[0] && (!self->genericValue4 || !self->genericValue5))
	{ //fire the target for pickup, if it's set to fire every time, or set to only fire the first time and the first time has not yet occured.
		if (level.siegeMap == SIEGEMAP_DESERT && !Q_stricmp(self->target2, "c3postolenprint"))
		{
			//droid part on desert
			char *part;
			if (self->model && self->model[0])
			{
				if (strstr(self->model, "arm"))
				{
					part = "arm";
				}
				else if (strstr(self->model, "head"))
				{
					part = "head";
				}
				else if (strstr(self->model, "leg"))
				{
					part = "leg";
				}
				else if (strstr(self->model, "torso"))
				{
					part = "torso";
				}
			}
			if (part && part[0])
			{
				trap_SendServerCommand(-1, va("cp \"Protocol droid %s has been taken!\n\"", part));
			}
			else
			{
				G_UseTargets2(self, self, self->target2);
			}
		}
		else
		{
			G_UseTargets2(self, self, self->target2);
		}
		self->genericValue5 = 1; //mark it as having been picked up
	}	
	if (self->specialIconTreatment)
	{
		self->s.eFlags &= ~EF_RADAROBJECT;
	}
	// time2 set to -1 will blink the item on the radar indefinately
	self->s.time2 = 0xFFFFFFFF;

}

void SiegeItemPain(gentity_t *self, gentity_t *attacker, int damage)
{
	// Time 2 is used to pulse the radar icon to show its under attack
	self->s.time2 = level.time;
}

void SiegeItemDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	if (level.siegeMap == SIEGEMAP_DESERT && self->model && self->model[0] && strstr(self->model, "vjun/control_station"))
	{
		//killed a desert computer; note the client so we can give him points
		if (attacker && attacker->client && attacker->s.number < MAX_CLIENTS && attacker->client->sess.sessionTeam == TEAM_RED)
		{
			level.killerOfLastDesertComputer = attacker;
		}
		else
		{
			level.killerOfLastDesertComputer = NULL;
		}
	}

	self->takedamage = qfalse; //don't die more than once

	if (self->genericValue3)
	{ //An indexed effect to play on death
		vec3_t upAng;

		VectorSet(upAng, 0, 0, 1);
		G_PlayEffectID(self->genericValue3, self->r.currentOrigin, upAng);
	}

	self->neverFree = qfalse;
	self->think = G_FreeEntity;
	self->nextthink = level.time;

	//Fire off the death target if we've got one.
	if (self->target4 && self->target4[0])
	{
		G_UseTargets2(self, self, self->target4);
	}
}

void SiegeItemUse(gentity_t *ent, gentity_t *other, gentity_t *activator)
{ //once used, become active
	if (ent->spawnflags & SIEGEITEM_STARTOFFRADAR)
	{ //start showing on radar
		ent->s.eFlags |= EF_RADAROBJECT;

		if (!(ent->s.eFlags & EF_NODRAW))
		{ //we've nothing else to do here
			return;
		}
	}
	else
	{ //make sure it's showing up
		ent->s.eFlags |= EF_RADAROBJECT;
	}

	if (ent->genericValue11 || !ent->takedamage)
	{ //We want to be able to walk into it to pick it up then.
		ent->r.contents = CONTENTS_TRIGGER;
		ent->clipmask = CONTENTS_SOLID|CONTENTS_TERRAIN;
		if (ent->genericValue11)
		{
			ent->touch = SiegeItemTouch;
		}
	}
	else
	{ //Make it solid.
		ent->r.contents = MASK_PLAYERSOLID;
		ent->clipmask = MASK_PLAYERSOLID;
	}

	ent->think = SiegeItemThink;
	ent->nextthink = level.time + FRAMETIME/2;

	//take off nodraw
	ent->s.eFlags &= ~EF_NODRAW;

	if (ent->paintarget && ent->paintarget[0])
	{ //want to be on this guy's origin now then
		gentity_t *targ = G_Find (NULL, FOFS(targetname), ent->paintarget);
		
		if (targ && targ->inuse)
		{
			G_SetOrigin(ent, targ->r.currentOrigin);
			trap_LinkEntity(ent);
		}
	}
}

/*QUAKED misc_siege_item (1 0 1) (-16 -16 -24) (16 16 32) ? x x STARTOFFRADAR
STARTOFFRADAR - start not displaying on radar, don't display until used.

"model"				Name of model to use for the object
"mins"				Actual mins of the object. Careful not to place it into a solid,
					as these new mins will not be reflected visually in the editor.
					Default value is "-16 -16 -24".
"maxs"				Same as above for maxs. Default value is "16 16 32".
"targetname"		If it has a targetname, it will only spawn upon being used.
"target2"			Target to fire upon pickup. If none, nothing will happen.
"pickuponlyonce"	If non-0, target2 will only be fired on the first pickup. If the item is
					dropped and picked up again later, the target will not be fired off on
					the sequential pickup. Default value is 1.
"target3"			Target to fire upon delivery of the item to the goal point.
					If none, nothing will happen. (but you should always want something to happen)
"health"			If > 0, object can be damaged and will die once health reaches 0. Default is 0.
"showhealth"		if health > 0, will show a health meter for this item
"teamowner"			Which team owns this item, used only for deciding what color to make health meter
"target4"			Target to fire upon death, if damageable. Default is none.
"deathfx"			Effect to play on death, if damageable. Default is none.
"canpickup"			If non-0, item can be picked up. Otherwise it will just be solid and sit on the
					ground. Default is 1.
"pickupsound"		Sound to play on pickup, if any.
"goaltarget"		Must be the targetname of a trigger_multi/trigger_once. Once a player carrying
					this object is brought inside the specified trigger, then that trigger will be
					allowed to fire. Ideally it will target a siege objective or something like that.
"usephysics"		If non-0, run standard physics on the object. Default is 1.
"mass"				If usephysics, this will be the factored object mass. Default is 0.09.
"gravity"			If usephysics, this will be the factored gravitational pull. Default is 3.0.
"bounce"			If usephysics, this will be the factored bounce amount. Default is 1.3.
"teamnotouch"		If 1 don't let team 1 pickup, if 2 don't let team 2. By default both teams
					can pick this object up and carry it around until death.
"teamnocomplete"	Same values as above, but controls if this object can be taken into the objective
					area by said team.
"respawnfx"			Plays this effect when respawning (e.g. it is left in an unknown area too long
					and goes back to the original spot). If this is not specified there will be
					no effect. (expected format is .efx file)
"paintarget"		plop self on top of this guy's origin when we are used (only applies if the siege
					item has a targetname)
"noradar"			if non-0 this thing will not show up on radar

"forcelimit"		if non-0, while carrying this item, the carrier's force powers will be crippled.
"target5"			target to fire when respawning.
"target6"			target to fire when dropped by someone carrying this item.

"icon"				icon that represents the gametype item on the radar

health charge things only work with showhealth 1 on siege items that take damage.
"health_chargeamt"	if non-0 will recharge this much health every...
"health_chargerate"	...this many milliseconds
*/
void SP_misc_siege_item (gentity_t *ent)
{
	int		canpickup;
	int		noradar;
	char	*s;

	if (!siege_valid || g_gametype.integer != GT_SIEGE)
	{
		G_FreeEntity(ent);
		return;
	}

	if (!ent->model || !ent->model[0])
	{
		G_Error("You must specify a model for misc_siege_item types.");
	}

	G_SpawnInt("canpickup", "1", &canpickup);
	ent->canPickUp = canpickup;
	G_SpawnInt("usephysics", "1", &ent->genericValue1);
	G_SpawnInt("autorespawn", "1", &ent->genericValue16);
	G_SpawnInt("respawntime", "20000", &ent->genericValue17);
	ent->siegeItemCarrierTime = 0;

	ent->classname = "misc_siege_item";

	if (ent->genericValue1)
	{ //if we're using physics we want lerporigin smoothing
		ent->s.eFlags |= EF_CLIENTSMOOTH;
	}

	G_SpawnInt("noradar", "0", &noradar);

	if (level.siegeMap == SIEGEMAP_NAR)
	{
		//start off with icon hidden for nar shaddaa codes
		ent->s.eFlags &= ~EF_RADAROBJECT;
	}
	else
	{
		//Want it to always show up as a goal object on radar
		if (!noradar && !(ent->spawnflags & SIEGEITEM_STARTOFFRADAR))
		{
			ent->s.eFlags |= EF_RADAROBJECT;
		}
	}

	//All clients want to know where it is at all times for radar
	ent->r.svFlags |= SVF_BROADCAST;

	G_SpawnInt("pickuponlyonce", "1", &ent->genericValue4);

	G_SpawnInt("teamnotouch", "0", &ent->genericValue6);
	G_SpawnInt("teamnocomplete", "0", &ent->genericValue7);
	G_SpawnInt("hideiconwhilecarried", "0", &ent->specialIconTreatment);
	G_SpawnInt("removeFromOwnerOnUse", "1", &ent->removeFromOwnerOnUse);
	G_SpawnInt("removeFromGameOnUse", "1", &ent->removeFromGameOnUse);
	G_SpawnInt("despawnOnUse", "0", &ent->despawnOnUse);
	//Get default physics values.
	G_SpawnFloat("mass", "0.09", &ent->mass);
	G_SpawnFloat("gravity", "3.0", &ent->radius);
	G_SpawnFloat("bounce", "1.3", &ent->random);

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

	G_SpawnString( "pickupsound", "", &s );

	if (s && s[0])
	{ //We have a pickup sound, so index it now.
		ent->noise_index = G_SoundIndex(s);
	}

	G_SpawnString( "deathfx", "", &s );

	if (s && s[0])
	{ //We have a death effect, so index it now.
		ent->genericValue3 = G_EffectIndex(s);
	}

	G_SpawnString( "respawnfx", "", &s );

	if (s && s[0])
	{ //We have a respawn effect, so index it now.
		ent->genericValue10 = G_EffectIndex(s);
	}

	G_SpawnString( "icon", "", &s );
	
	if (s && s[0])
	{ 
		// We have an icon, so index it now.  We are reusing the genericenemyindex
		// variable rather than adding a new one to the entity state.
		ent->s.genericenemyindex = G_IconIndex(s);
	}
	
	ent->s.modelindex = G_ModelIndex(ent->model);
	G_SpawnInt("modelscale", "100", &ent->s.iModelScale);
	ent->s.iModelScale = Com_Clampi(0, 1023, ent->s.iModelScale);

	//Is the model a ghoul2 model?
	if (!Q_stricmp(&ent->model[strlen(ent->model) - 4], ".glm"))
	{ //apparently so.
        ent->s.modelGhoul2 = 1;
	}

	ent->s.eType = ET_GENERAL;

	//Set the mins/maxs with default values.
	G_SpawnVector("mins", "-16 -16 -24", ent->r.mins);
	G_SpawnVector("maxs", "16 16 32", ent->r.maxs);

	VectorCopy(ent->s.origin, ent->pos1); //store off the initial origin for respawning
	G_SetOrigin(ent, ent->s.origin);

	VectorCopy(ent->s.angles, ent->r.currentAngles);
	VectorCopy(ent->s.angles, ent->s.apos.trBase);

	G_SpawnInt("forcelimit", "0", &ent->genericValue15);

	G_SpawnFloat("speedMultiplier", "1", &ent->speedMultiplier);
	G_SpawnFloat("speedMultiplierTeam2", "1", &ent->speedMultiplierTeam2);

	if (ent->health > 0)
	{ //If it has health, it can be killed.
		int t;

		ent->pain = SiegeItemPain;
		ent->die = SiegeItemDie;
		ent->takedamage = qtrue;

		G_SpawnInt( "showhealth", "0", &t );
		if (t)
		{ //a non-0 maxhealth value will mean we want to show the health on the hud
			ent->maxHealth = ent->health;
			G_ScaleNetHealth(ent);

			G_SpawnInt( "health_chargeamt", "0", &ent->genericValue12);
			G_SpawnInt( "health_chargerate", "0", &ent->genericValue13);
		}
	}
	else
	{ //Otherwise no.
		ent->takedamage = qfalse;
	}

	if (ent->spawnflags & SIEGEITEM_STARTOFFRADAR)
	{
		ent->use = SiegeItemUse;
	}
	else if (ent->targetname && ent->targetname[0])
	{
		ent->s.eFlags |= EF_NODRAW; //kind of hacky, but whatever
		ent->genericValue11 = canpickup;
        ent->use = SiegeItemUse;
		ent->s.eFlags &= ~EF_RADAROBJECT;
	}

	if ( (!ent->targetname || !ent->targetname[0]) ||
		 (ent->spawnflags & SIEGEITEM_STARTOFFRADAR) )
	{
		if (canpickup || !ent->takedamage)
		{ //We want to be able to walk into it to pick it up then.
			ent->r.contents = CONTENTS_TRIGGER;
			ent->clipmask = CONTENTS_SOLID|CONTENTS_TERRAIN;
			if (canpickup)
			{
				ent->touch = SiegeItemTouch;
			}
		}
		else
		{ //Make it solid.
			ent->r.contents = MASK_PLAYERSOLID;
			ent->clipmask = MASK_PLAYERSOLID;
		}

		ent->think = SiegeItemThink;
		ent->nextthink = level.time + FRAMETIME/2;
	}

	ent->genericValue8 = ENTITYNUM_NONE; //initialize the carrier to none

	ent->neverFree = qtrue; //never free us unless we specifically request it.

	trap_LinkEntity(ent);
}

//sends extra data about other client's in this client's PVS
//used for support guy etc.
//with this formatting:
//sxd 16,999,999,999|17,999,999,999
//assumed max 2 chars for cl num, 3 chars per ammo/health/maxhealth, even a single string full of
//info for all 32 clients should not get much past 450 bytes, which is well within a
//reasonable range. We don't need to send anything about the max ammo or current weapon, because
//currentState.weapon can be checked for the ent in question on the client. -rww
extern int Get_Max_Ammo(gentity_t *ent, ammo_t ammoIndex);
void G_SiegeClientExData(gentity_t *msgTarg)
{
	gentity_t *ent;
	int count = 0;
	int i = 0;
	char str[MAX_STRING_CHARS];
	char scratch[MAX_STRING_CHARS];

    if ( level.pause.state != PAUSE_NONE )
            return;

	while (i < level.num_entities && count < MAX_EXDATA_ENTS_TO_SEND)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client && msgTarg->s.number != ent->s.number &&
			ent->s.eType == ET_PLAYER && msgTarg->client->ps.persistant[PERS_TEAM] == ent->client->ps.persistant[PERS_TEAM] &&
			trap_InPVS(msgTarg->client->ps.origin, ent->client->ps.origin))
		{ //another client in the same pvs, send his jive
            if (count)
			{ //append a seperating space if we are not the first in the list
				Q_strcat(str, sizeof(str), " ");
			}
			else
			{ //otherwise create the prepended chunk
				strcpy(str, "sxd ");
			}

#define EXTENDEDSIEGEDATA_PROTOCOL	(1)

			//append the stats
			Com_sprintf(scratch, sizeof(scratch), "%i|%i|%i|%i" /*begin new stuff:*/ "|z=%i|b=%i|mb=%i|p=%i|mp=%i|m=%i|mm=%i|r=%i|mr=%i|h=%i|mh=%i|t=%i|mt=%i|d=%i|md=%i", ent->s.number, ent->client->ps.stats[STAT_HEALTH],
				ent->client->ps.stats[STAT_MAX_HEALTH], ent->client->ps.ammo[weaponData[ent->client->ps.weapon].ammoIndex],
				EXTENDEDSIEGEDATA_PROTOCOL,
				ent->client->ps.ammo[AMMO_BLASTER], Get_Max_Ammo(ent, AMMO_BLASTER), 
				ent->client->ps.ammo[AMMO_POWERCELL], Get_Max_Ammo(ent, AMMO_POWERCELL),
				ent->client->ps.ammo[AMMO_METAL_BOLTS], Get_Max_Ammo(ent, AMMO_METAL_BOLTS),
				ent->client->ps.ammo[AMMO_ROCKETS], Get_Max_Ammo(ent, AMMO_ROCKETS),
				ent->client->ps.ammo[AMMO_THERMAL], Get_Max_Ammo(ent, AMMO_THERMAL),
				ent->client->ps.ammo[AMMO_TRIPMINE], Get_Max_Ammo(ent, AMMO_TRIPMINE),
				ent->client->ps.ammo[AMMO_DETPACK], Get_Max_Ammo(ent, AMMO_DETPACK));
			Q_strcat(str, sizeof(str), scratch);
			count++;
		}
		i++;
	}

	if (!count)
	{ //nothing to send
		return;
	}

	//send the string to him
	trap_SendServerCommand(msgTarg-g_entities, str);
}
