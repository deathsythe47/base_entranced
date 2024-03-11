// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

#include "kdtree.h"

typedef struct teamgame_s {
	float			last_flag_capture;
	int				last_capture_team;
	flagStatus_t	redStatus;	// CTF
	flagStatus_t	blueStatus;	// CTF
	flagStatus_t	flagStatus;	// One Flag CTF
	int				redTakenTime;
	int				blueTakenTime;
} teamgame_t;

teamgame_t teamgame;

void Team_SetFlagStatus( int team, flagStatus_t status );

void Team_InitGame( void ) {
	memset(&teamgame, 0, sizeof teamgame);

	switch( g_gametype.integer ) {
	case GT_CTF:
	case GT_CTY:
		teamgame.redStatus = teamgame.blueStatus = -1; // Invalid to force update
		Team_SetFlagStatus( TEAM_RED, FLAG_ATBASE );
		Team_SetFlagStatus( TEAM_BLUE, FLAG_ATBASE );
		level.redFlagStealTime = 0;
		level.blueFlagStealTime = 0;
		level.redTeamRunFlaghold = 0;
		level.blueTeamRunFlaghold = 0;
		break;
	default:
		break;
	}
}

int OtherTeam(int team) {
	// huge change
	if (team==TEAM_RED)
		return TEAM_BLUE;
	else if (team==TEAM_BLUE)
		return TEAM_RED;
	return team;
}

const char *TeamName(int team)  {
	if (team==TEAM_RED)
		return "RED";
	else if (team==TEAM_BLUE)
		return "BLUE";
	else if (team==TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

const char *OtherTeamName(int team) {
	if (team==TEAM_RED)
		return "BLUE";
	else if (team==TEAM_BLUE)
		return "RED";
	else if (team==TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

const char *TeamColorString(int team) {
	if (team==TEAM_RED)
		return S_COLOR_RED;
	else if (team==TEAM_BLUE)
		return S_COLOR_BLUE;
	else if (team==TEAM_SPECTATOR)
		return S_COLOR_YELLOW;
	return S_COLOR_WHITE;
}

//Printing messages to players via this method is no longer done, StringEd stuff is client only.


//plIndex used to print pl->client->pers.netname
//teamIndex used to print team name
void PrintCTFMessage(int plIndex, int teamIndex, int ctfMessage)
{
	gentity_t *te;

	if (plIndex == -1)
	{
		plIndex = MAX_CLIENTS+1;
	}
	if (teamIndex == -1)
	{
		teamIndex = 50;
	}

	te = G_TempEntity(vec3_origin, EV_CTFMESSAGE);
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = ctfMessage;
	te->s.trickedentindex = plIndex;
	if (ctfMessage == CTFMESSAGE_PLAYER_CAPTURED_FLAG)
	{
		if (teamIndex == TEAM_RED)
		{
			te->s.trickedentindex2 = TEAM_BLUE;
		}
		else
		{
			te->s.trickedentindex2 = TEAM_RED;
		}
	}
	else
	{
		te->s.trickedentindex2 = teamIndex;
	}

	if ( ctfMessage == CTFMESSAGE_PLAYER_CAPTURED_FLAG ) {
		// send the capture time as an unused event parameter
		if ( teamIndex == TEAM_RED ) {
			te->s.time = level.redTeamRunFlaghold;
		} else if ( teamIndex == TEAM_BLUE ) {
			te->s.time = level.blueTeamRunFlaghold;
		}
	}
}

/*
==============
AddTeamScore

 used for gametype > GT_TEAM
 for gametype GT_TEAM the level.teamScores is updated in AddScore in g_combat.c
==============
*/
void AddTeamScore(vec3_t origin, int team, int score) {
	gentity_t	*te;

	te = G_TempEntity(origin, EV_GLOBAL_TEAM_SOUND );
	te->r.svFlags |= SVF_BROADCAST;

	if ( team == TEAM_RED ) {
		if ( level.teamScores[ TEAM_RED ] + score == level.teamScores[ TEAM_BLUE ] ) {
			//teams are tied sound
			te->s.eventParm = GTS_TEAMS_ARE_TIED;
		}
		else if ( level.teamScores[ TEAM_RED ] <= level.teamScores[ TEAM_BLUE ] &&
					level.teamScores[ TEAM_RED ] + score > level.teamScores[ TEAM_BLUE ]) {
			// red took the lead sound
			te->s.eventParm = GTS_REDTEAM_TOOK_LEAD;
		}
		else {
			// red scored sound
			te->s.eventParm = GTS_REDTEAM_SCORED;
		}
	}
	else {
		if ( level.teamScores[ TEAM_BLUE ] + score == level.teamScores[ TEAM_RED ] ) {
			//teams are tied sound
			te->s.eventParm = GTS_TEAMS_ARE_TIED;
		}
		else if ( level.teamScores[ TEAM_BLUE ] <= level.teamScores[ TEAM_RED ] &&
					level.teamScores[ TEAM_BLUE ] + score > level.teamScores[ TEAM_RED ]) {
			// blue took the lead sound
			te->s.eventParm = GTS_BLUETEAM_TOOK_LEAD;
		}
		else {
			// blue scored sound
			te->s.eventParm = GTS_BLUETEAM_SCORED;
		}
	}
	level.teamScores[ team ] += score;
}

/*
==============
OnSameTeam
==============
*/
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 ) {
	assert(ent1 && ent2);
	if (ent1->projectileTeam || ent2->projectileTeam) {
		if (ent1->projectileTeam && ent2->projectileTeam) {
			return qfalse; // two projectiles; allow it
		}
		else if (ent1->projectileTeam) {
			if (ent2->client && ent2->client->sess.sessionTeam == ent1->projectileTeam)
				return qtrue;
			if (ent2->teamnodmg == ent1->projectileTeam)
				return qtrue;
		}
		else if (ent2->projectileTeam) {
			if (ent1->client && ent1->client->sess.sessionTeam == ent2->projectileTeam)
				return qtrue;
			if (ent1->teamnodmg == ent2->projectileTeam)
				return qtrue;
		}
	}

	if ( !ent1->client || !ent2->client ) {
		return qfalse;
	}

	if (g_gametype.integer == GT_POWERDUEL)
	{
		if (ent1->client->sess.duelTeam == ent2->client->sess.duelTeam)
		{
			return qtrue;
		}

		return qfalse;
	}

	if (g_gametype.integer == GT_SINGLE_PLAYER)
	{
		qboolean ent1IsBot = qfalse;
		qboolean ent2IsBot = qfalse;

		if (ent1->r.svFlags & SVF_BOT)
		{
			ent1IsBot = qtrue;
		}
		if (ent2->r.svFlags & SVF_BOT)
		{
			ent2IsBot = qtrue;
		}

		if ((ent1IsBot && ent2IsBot) || (!ent1IsBot && !ent2IsBot))
		{
			return qtrue;
		}
		return qfalse;
	}

	if ( g_gametype.integer < GT_TEAM ) {
		return qfalse;
	}

	if (ent1->s.eType == ET_NPC &&
		ent1->s.NPC_class == CLASS_VEHICLE &&
		ent1->client &&
		ent1->client->sess.sessionTeam != TEAM_FREE &&
		ent2->client &&
		ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam)
	{
		return qtrue;
	}
	if (ent2->s.eType == ET_NPC &&
		ent2->s.NPC_class == CLASS_VEHICLE &&
		ent2->client &&
		ent2->client->sess.sessionTeam != TEAM_FREE &&
		ent1->client &&
		ent2->client->sess.sessionTeam == ent1->client->sess.sessionTeam)
	{
		return qtrue;
	}

	if (ent1->client->sess.sessionTeam == TEAM_FREE &&
		ent2->client->sess.sessionTeam == TEAM_FREE &&
		ent1->s.eType == ET_NPC &&
		ent2->s.eType == ET_NPC)
	{ //NPCs don't do normal team rules
		return qfalse;
	}

	if (ent1->s.eType == ET_NPC && ent2->s.eType == ET_PLAYER)
	{
		return qfalse;
	}
	else if (ent1->s.eType == ET_PLAYER && ent2->s.eType == ET_NPC)
	{
		return qfalse;
	}
	if (g_gametype.integer == GT_SIEGE && level.inSiegeCountdown && level.inSiegeCountdown == qtrue && (ent1->client->sess.siegeDesiredTeam != ent2->client->sess.siegeDesiredTeam))
	{
		return qfalse;
	}
	if ( ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam ) {
		return qtrue;
	}

	return qfalse;
}

team_t GetRealTeam(gclient_t *cl) {
	if (!cl)
		return TEAM_SPECTATOR;
	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		return TEAM_FREE; // there aren't really spectators in duel/powerduel; everyone is forced to play
	if (g_gametype.integer == GT_SIEGE) {
		if (cl->sess.sessionTeam == TEAM_RED || cl->sess.sessionTeam == TEAM_BLUE)
			return cl->sess.sessionTeam; // during the round
		if (level.inSiegeCountdown && (cl->sess.siegeDesiredTeam == TEAM_RED || cl->sess.siegeDesiredTeam == TEAM_BLUE))
			return cl->sess.siegeDesiredTeam; // during the countdown
		return TEAM_SPECTATOR; // a true spectator
	}
	return cl->sess.sessionTeam;
}

qboolean OnOppositeTeam(gclient_t *cl1, gclient_t *cl2) {
	team_t t1 = GetRealTeam(cl1);
	team_t t2 = GetRealTeam(cl2);
	if ((t1 == TEAM_RED && t2 == TEAM_BLUE) || (t1 == TEAM_BLUE && t2 == TEAM_RED))
		return qtrue;
	return qfalse;
}


static char ctfFlagStatusRemap[] = { '0', '1', '*', '*', '2' };

void Team_SetFlagStatus( int team, flagStatus_t status ) {
	qboolean modified = qfalse;

	switch( team ) {
	case TEAM_RED:	// CTF
		if( teamgame.redStatus != status ) {
			teamgame.redStatus = status;
			modified = qtrue;
		}
		break;

	case TEAM_BLUE:	// CTF
		if( teamgame.blueStatus != status ) {
			teamgame.blueStatus = status;
			modified = qtrue;
		}
		break;

	case TEAM_FREE:	// One Flag CTF
		if( teamgame.flagStatus != status ) {
			teamgame.flagStatus = status;
			modified = qtrue;
		}
		break;
	}

	if( modified ) {
		char st[4];

		if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY ) {
			st[0] = ctfFlagStatusRemap[teamgame.redStatus];
			st[1] = ctfFlagStatusRemap[teamgame.blueStatus];
			st[2] = 0;
		}

		trap_SetConfigstring( CS_FLAGSTATUS, st );
	}
}

void Team_CheckDroppedItem( gentity_t *dropped ) {
	if ( dropped->item->giTag == PW_REDFLAG ) {
		Team_SetFlagStatus( TEAM_RED, FLAG_DROPPED );
	}
	else if( dropped->item->giTag == PW_BLUEFLAG ) {
		Team_SetFlagStatus( TEAM_BLUE, FLAG_DROPPED );
	}
	else if( dropped->item->giTag == PW_NEUTRALFLAG ) {
		Team_SetFlagStatus( TEAM_FREE, FLAG_DROPPED );
	}
}

/*
================
Team_ForceGesture
================
*/
void Team_ForceGesture(int team) {
	int i;
	gentity_t *ent;

	for (i = 0; i < MAX_CLIENTS; i++) {
		ent = &g_entities[i];
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;
		if (ent->client->sess.sessionTeam != team)
			continue;
		//
		ent->flags |= FL_FORCE_GESTURE;
	}
}

/*
================
Team_FragBonuses

Calculate the bonuses for flag defense, flag carrier defense, etc.
Note that bonuses are not cumulative.  You get one, they are in importance
order.
================
*/
void Team_FragBonuses(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker)
{
	int i;
	gentity_t *ent;
	int flag_pw, enemy_flag_pw;
	int otherteam;
	int tokens;
	gentity_t *flag, *carrier = NULL;
	char *c;
	vec3_t v1, v2;
	int team;

	// no bonus for fragging yourself or team mates
	if (!targ->client || !attacker->client || targ == attacker || OnSameTeam(targ, attacker))
		return;

	team = targ->client->sess.sessionTeam;
	otherteam = OtherTeam(targ->client->sess.sessionTeam);
	if (otherteam < 0)
		return; // whoever died isn't on a team

	// same team, if the flag at base, check to he has the enemy flag
	if (team == TEAM_RED) {
		flag_pw = PW_REDFLAG;
		enemy_flag_pw = PW_BLUEFLAG;
	} else {
		flag_pw = PW_BLUEFLAG;
		enemy_flag_pw = PW_REDFLAG;
	}

	// did the attacker frag the flag carrier?
	tokens = 0;
	if (targ->client->ps.powerups[enemy_flag_pw]) {
		attacker->client->pers.teamState.lastfraggedcarrier = level.time;
		AddScore(attacker, targ->r.currentOrigin, CTF_FRAG_CARRIER_BONUS);

		attacker->client->pers.teamState.fragcarrier++;
		PrintCTFMessage(attacker->s.number, team, CTFMESSAGE_FRAGGED_FLAG_CARRIER);

		// the target had the flag, clear the hurt carrier
		// field on the other team
		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;
			if (ent->inuse && ent->client->sess.sessionTeam == otherteam)
				ent->client->pers.teamState.lasthurtcarrier = 0;
		}
		return;
	}

	// did the attacker frag a head carrier? other->client->ps.generic1
	if (tokens) {
		attacker->client->pers.teamState.lastfraggedcarrier = level.time;
		AddScore(attacker, targ->r.currentOrigin, CTF_FRAG_CARRIER_BONUS * tokens * tokens);

		//*CHANGE 31* longest flag holding time keeping track
		const int thisFlaghold = G_GetAccurateTimerOnTrigger( &targ->client->pers.teamState.flagsince, targ, NULL );

		targ->client->pers.teamState.flaghold += thisFlaghold;

		if ( thisFlaghold > targ->client->pers.teamState.longestFlaghold )
			targ->client->pers.teamState.longestFlaghold = thisFlaghold;

		if ( targ->client->ps.powerups[PW_REDFLAG] ) {
			// carried the red flag, so blue team
			level.blueTeamRunFlaghold += thisFlaghold;
		} else if ( targ->client->ps.powerups[PW_BLUEFLAG] ) {
			// carried the blue flag, so red team
			level.redTeamRunFlaghold += thisFlaghold;
		}

		attacker->client->pers.teamState.fragcarrier++;

		// the target had the flag, clear the hurt carrier
		// field on the other team
		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;
			if (ent->inuse && ent->client->sess.sessionTeam == otherteam)
				ent->client->pers.teamState.lasthurtcarrier = 0;
		}
		return;
	}

	if (targ->client->pers.teamState.lasthurtcarrier &&
		level.time - targ->client->pers.teamState.lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT &&
		!attacker->client->ps.powerups[flag_pw]) {
		// attacker is on the same team as the flag carrier and
		// fragged a guy who hurt our flag carrier
		AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_DANGER_PROTECT_BONUS);

		attacker->client->pers.teamState.carrierdefense++;
		targ->client->pers.teamState.lasthurtcarrier = 0;

		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
		team = attacker->client->sess.sessionTeam;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	if (targ->client->pers.teamState.lasthurtcarrier &&
		level.time - targ->client->pers.teamState.lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT) {
		// attacker is on the same team as the skull carrier and
		AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_DANGER_PROTECT_BONUS);

		attacker->client->pers.teamState.carrierdefense++;
		targ->client->pers.teamState.lasthurtcarrier = 0;

		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
		team = attacker->client->sess.sessionTeam;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	// flag and flag carrier area defense bonuses

	// we have to find the flag and carrier entities

	// find the flag
	switch (attacker->client->sess.sessionTeam) {
	case TEAM_RED:
		c = "team_CTF_redflag";
		break;
	case TEAM_BLUE:
		c = "team_CTF_blueflag";
		break;		
	default:
		return;
	}
	// find attacker's team's flag carrier
	for (i = 0; i < g_maxclients.integer; i++) {
		carrier = g_entities + i;
		if (carrier->inuse && carrier->client->ps.powerups[flag_pw])
			break;
		carrier = NULL;
	}
	flag = NULL;
	while ((flag = G_Find (flag, FOFS(classname), c)) != NULL) {
		if (!(flag->flags & FL_DROPPED_ITEM))
			break;
	}

	if (!flag)
		return; // can't find attacker's flag

	// ok we have the attackers flag and a pointer to the carrier

	// check to see if we are defending the base's flag
	VectorSubtract(targ->r.currentOrigin, flag->r.currentOrigin, v1);
	VectorSubtract(attacker->r.currentOrigin, flag->r.currentOrigin, v2);

	if ( ( ( VectorLength(v1) < CTF_TARGET_PROTECT_RADIUS &&
		trap_InPVS(flag->r.currentOrigin, targ->r.currentOrigin ) ) ||
		( VectorLength(v2) < CTF_TARGET_PROTECT_RADIUS &&
		trap_InPVS(flag->r.currentOrigin, attacker->r.currentOrigin ) ) ) &&
		attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam) {

		// we defended the base flag
		AddScore(attacker, targ->r.currentOrigin, CTF_FLAG_DEFENSE_BONUS);
		attacker->client->pers.teamState.basedefense++;

		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	if (carrier && carrier != attacker) {
		VectorSubtract(targ->r.currentOrigin, carrier->r.currentOrigin, v1);
		VectorSubtract(attacker->r.currentOrigin, carrier->r.currentOrigin, v2);

		if ( ( ( VectorLength(v1) < CTF_ATTACKER_PROTECT_RADIUS &&
			trap_InPVS(carrier->r.currentOrigin, targ->r.currentOrigin ) ) ||
			( VectorLength(v2) < CTF_ATTACKER_PROTECT_RADIUS &&
				trap_InPVS(carrier->r.currentOrigin, attacker->r.currentOrigin ) ) ) &&
			attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam) {
			AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_PROTECT_BONUS);
			attacker->client->pers.teamState.carrierdefense++;

			attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
			attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

			return;
		}
	}
}

/*
================
Team_CheckHurtCarrier

Check to see if attacker hurt the flag carrier.  Needed when handing out bonuses for assistance to flag
carrier defense.
================
*/
void Team_CheckHurtCarrier(gentity_t *targ, gentity_t *attacker)
{
	int flag_pw;

	if (!targ->client || !attacker->client)
		return;

	if (targ->client->sess.sessionTeam == TEAM_RED)
		flag_pw = PW_BLUEFLAG;
	else
		flag_pw = PW_REDFLAG;

	// flags
	if (targ->client->ps.powerups[flag_pw] &&
		targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
		attacker->client->pers.teamState.lasthurtcarrier = level.time;

	// skulls
	if (targ->client->ps.generic1 &&
		targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
		attacker->client->pers.teamState.lasthurtcarrier = level.time;
}


gentity_t *Team_ResetFlag( int team ) {
	char *c;
	gentity_t *ent, *rent = NULL;

	switch (team) {
	case TEAM_RED:
		c = "team_CTF_redflag";
		break;
	case TEAM_BLUE:
		c = "team_CTF_blueflag";
		break;
	case TEAM_FREE:
		c = "team_CTF_neutralflag";
		break;
	default:
		return NULL;
	}

	ent = NULL;
	while ((ent = G_Find (ent, FOFS(classname), c)) != NULL) {
		if (ent->flags & FL_DROPPED_ITEM)
			G_FreeEntity(ent);
		else {
			rent = ent;
			RespawnItem(ent);
		}
	}

	// team is the color of the flag being reset, so reset the flaghold of the opposite team
	if ( team == TEAM_RED ) {
		level.redFlagStealTime = 0;
		level.blueTeamRunFlaghold = 0;
	} else {
		level.blueFlagStealTime = 0;
		level.redTeamRunFlaghold = 0;
	}

	Team_SetFlagStatus( team, FLAG_ATBASE );

	return rent;
}

void Team_ResetFlags( void ) {
	if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY ) {
		Team_ResetFlag( TEAM_RED );
		Team_ResetFlag( TEAM_BLUE );
	}
}

void Team_ReturnFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_LogPrintf ("Warning:  NULL passed to Team_ReturnFlagSound\n");
		return;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_RED_RETURN;
	}
	else {
		te->s.eventParm = GTS_BLUE_RETURN;
	}
	te->r.svFlags |= SVF_BROADCAST;
}

void Team_TakeFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_TakeFlagSound\n");
		return;
	}

	// only play sound when the flag was at the base
	// or not picked up the last 10 seconds
	switch(team) {
		case TEAM_RED:
			if( teamgame.blueStatus != FLAG_ATBASE ) {
				if (teamgame.blueTakenTime > level.time - 10000)
					return;
			}
			teamgame.blueTakenTime = level.time;
			break;

		case TEAM_BLUE:	// CTF
			if( teamgame.redStatus != FLAG_ATBASE ) {
				if (teamgame.redTakenTime > level.time - 10000)
					return;
			}
			teamgame.redTakenTime = level.time;
			break;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_RED_TAKEN;
	}
	else {
		te->s.eventParm = GTS_BLUE_TAKEN;
	}
	te->r.svFlags |= SVF_BROADCAST;
}

void Team_CaptureFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_CaptureFlagSound\n");
		return;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_BLUE_CAPTURE;
	}
	else {
		te->s.eventParm = GTS_RED_CAPTURE;
	}
	te->r.svFlags |= SVF_BROADCAST;
}

void Team_ReturnFlag( int team ) {
	Team_ReturnFlagSound(Team_ResetFlag(team), team);
	if( team == TEAM_FREE ) {
	}
	else { //flag should always have team in normal CTF
		PrintCTFMessage(-1, team, CTFMESSAGE_FLAG_RETURNED);
	}
}

void Team_FreeEntity( gentity_t *ent ) {
	if( ent->item->giTag == PW_REDFLAG ) {
		Team_ReturnFlag( TEAM_RED );
	}
	else if( ent->item->giTag == PW_BLUEFLAG ) {
		Team_ReturnFlag( TEAM_BLUE );
	}
	else if( ent->item->giTag == PW_NEUTRALFLAG ) {
		Team_ReturnFlag( TEAM_FREE );
	}
}

/*
==============
Team_DroppedFlagThink

Automatically set in Launch_Item if the item is one of the flags

Flags are unique in that if they are dropped, the base flag must be respawned when they time out
==============
*/
void Team_DroppedFlagThink(gentity_t *ent) {
	int		team = TEAM_FREE;

	if( ent->item->giTag == PW_REDFLAG ) {
		team = TEAM_RED;
	}
	else if( ent->item->giTag == PW_BLUEFLAG ) {
		team = TEAM_BLUE;
	}
	else if( ent->item->giTag == PW_NEUTRALFLAG ) {
		team = TEAM_FREE;
	}

	Team_ReturnFlagSound( Team_ResetFlag( team ), team );
	PrintCTFMessage(-1, team, CTFMESSAGE_FLAG_RETURNED);
	// Reset Flag will delete this entity
}


/*
==============
Team_DroppedFlagThink
==============
*/
static vec3_t	minFlagRange = { 50, 36, 36 };
static vec3_t	maxFlagRange = { 44, 36, 36 };

int Team_TouchEnemyFlag( gentity_t *ent, gentity_t *other, int team );
extern void PartitionedTimer( const int time, int *mins, int *secs, int *millis );

int Team_TouchOurFlag( gentity_t *ent, gentity_t *other, int team ) {
	int			i;
	gentity_t	*player;
	gclient_t	*cl = other->client;
	int			enemy_flag;
	vec3_t		mins, maxs;
	int num, j, enemyTeam;
	int	touch[MAX_GENTITIES];
	gentity_t*	enemy;
	float enemyDist, dist;

	if (cl->sess.sessionTeam == TEAM_RED) {
		enemy_flag = PW_BLUEFLAG;
	} else {
		enemy_flag = PW_REDFLAG;
	}

	if ( ent->flags & FL_DROPPED_ITEM ) {
		// hey, its not home.  return it by teleporting it back
		PrintCTFMessage(other->s.number, team, CTFMESSAGE_PLAYER_RETURNED_FLAG);

		AddScore(other, ent->r.currentOrigin, CTF_RECOVERY_BONUS);
		other->client->pers.teamState.flagrecovery++;
		other->client->pers.teamState.lastreturnedflag = level.time;

		//ResetFlag will remove this entity!  We must return zero
		Team_ReturnFlagSound(Team_ResetFlag(team), team);



		return 0;
	}

	// the flag is at home base.  if the player has the enemy
	// flag, he's just won!
	if (!cl->ps.powerups[enemy_flag])
		return 0; // We don't have the flag

	if (level.intermissionQueued) {
		return 0;
	}

	VectorSubtract( ent->s.pos.trBase, minFlagRange, mins );
	VectorAdd( ent->s.pos.trBase, maxFlagRange, maxs );

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	dist = Distance(ent->s.pos.trBase, other->client->ps.origin);
		
	if (other->client->sess.sessionTeam == TEAM_RED){
		enemyTeam = TEAM_BLUE;
	} else {
		enemyTeam = TEAM_RED;
	}	

	for ( j=0 ; j<num ; j++ ) {
		enemy = (g_entities + touch[j]);

		if (!enemy || !enemy->inuse || !enemy->client){
			continue;
		}

		//check if its alive
		if (enemy->health < 1)
			continue;		// dead people can't pickup

		//ignore specs
		if (enemy->client->sess.sessionTeam == TEAM_SPECTATOR)
			continue;

		//check if this is enemy
		if ((enemy->client->sess.sessionTeam != TEAM_RED && enemy->client->sess.sessionTeam != TEAM_BLUE) ||
			enemy->client->sess.sessionTeam != enemyTeam){
			continue;
		}
			
		//check if enemy is closer to our flag than us
		enemyDist = Distance(ent->s.pos.trBase,enemy->client->ps.origin);
		if (enemyDist < dist){
			return Team_TouchEnemyFlag( ent, enemy, team );
		}
	}

	//*CHANGE 31* longest flag holding time keeping track
	const int thisFlaghold = G_GetAccurateTimerOnTrigger( &other->client->pers.teamState.flagsince, other, ent );

	other->client->pers.teamState.flaghold += thisFlaghold;
	if ( thisFlaghold > other->client->pers.teamState.longestFlaghold )
		other->client->pers.teamState.longestFlaghold = thisFlaghold;

	// team is the color of the flagstand on which we are capturing, so we add time to the ACTUAL team (not the opposite)
	int initialStealTime;
	if ( team == TEAM_RED ) {
		initialStealTime = level.blueFlagStealTime;
		level.redTeamRunFlaghold += thisFlaghold;
	} else {
		initialStealTime = level.redFlagStealTime;
		level.blueTeamRunFlaghold += thisFlaghold;
	}

	PrintCTFMessage( other->s.number, team, CTFMESSAGE_PLAYER_CAPTURED_FLAG );

	cl->ps.powerups[enemy_flag] = 0;
	
	teamgame.last_flag_capture = level.time;
	teamgame.last_capture_team = team;

	// Increase the team's score
	AddTeamScore(ent->s.pos.trBase, other->client->sess.sessionTeam, 1);
	//rww - don't really want to do this now. Mainly because performing a gesture disables your upper torso animations until it's done and you can't fire

	other->client->pers.teamState.captures++;
	other->client->rewardTime = level.time + REWARD_SPRITE_TIME;
	other->client->ps.persistant[PERS_CAPTURES]++;

	// other gets another 10 frag bonus
	AddScore(other, ent->r.currentOrigin, CTF_CAPTURE_BONUS);

	Team_CaptureFlagSound( ent, team );

	// Ok, let's do the player loop, hand out the bonuses
	for (i = 0; i < g_maxclients.integer; i++) {
		player = &g_entities[i];
		if (!player->inuse || player == other)
			continue;

		if (player->client->sess.sessionTeam !=
			cl->sess.sessionTeam) {
			player->client->pers.teamState.lasthurtcarrier = -5;
		} else if (player->client->sess.sessionTeam ==
			cl->sess.sessionTeam) {
			AddScore(player, ent->r.currentOrigin, CTF_TEAM_BONUS);
			// award extra points for capture assists
			if (player->client->pers.teamState.lastreturnedflag + 
				CTF_RETURN_FLAG_ASSIST_TIMEOUT > level.time) {
				AddScore (player, ent->r.currentOrigin, CTF_RETURN_FLAG_ASSIST_BONUS);
				other->client->pers.teamState.assists++;

				player->client->ps.persistant[PERS_ASSIST_COUNT]++;
				player->client->rewardTime = level.time + REWARD_SPRITE_TIME;

			} else if (player->client->pers.teamState.lastfraggedcarrier + 
				CTF_FRAG_CARRIER_ASSIST_TIMEOUT > level.time) {
				AddScore(player, ent->r.currentOrigin, CTF_FRAG_CARRIER_ASSIST_BONUS);
				other->client->pers.teamState.assists++;
				player->client->ps.persistant[PERS_ASSIST_COUNT]++;
				player->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}
		}
	}
	Team_ResetFlags();

	// reset the speed stats for this run
	other->client->pers.fastcapTopSpeed = 0;
	other->client->pers.fastcapDisplacement = 0;
	other->client->pers.fastcapDisplacementSamples = 0;

	CalculateRanks();

	return 0; // Do not respawn this automatically
}

int Team_TouchEnemyFlag( gentity_t *ent, gentity_t *other, int team ) {
	gclient_t *cl = other->client;
	vec3_t		mins, maxs;
	int num, j, ourFlag;
	int	touch[MAX_GENTITIES];
	gentity_t*	enemy;
	float enemyDist, dist;

	//fix capture condition v2
	VectorSubtract( ent->s.pos.trBase, minFlagRange, mins );
	VectorAdd( ent->s.pos.trBase, maxFlagRange, maxs );

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	dist = Distance(ent->s.pos.trBase, other->client->ps.origin);

	if (other->client->sess.sessionTeam == TEAM_RED){
		ourFlag   = PW_REDFLAG;
	} else {
		ourFlag   = PW_BLUEFLAG;
	}		

	for(j = 0; j < num; ++j){
		enemy = (g_entities + touch[j]);

		if (!enemy || !enemy->inuse || !enemy->client){
			continue;
		}

		//ignore specs
		if (enemy->client->sess.sessionTeam == TEAM_SPECTATOR)
			continue;

		//check if its alive
		if (enemy->health < 1)
			continue;		// dead people can't pick up items

		//lets check if he has our flag
		if (!enemy->client->ps.powerups[ourFlag])
			continue;

		//check if enemy is closer to our flag than us
		enemyDist = Distance(ent->s.pos.trBase,enemy->client->ps.origin);
		if (enemyDist < dist){
			return Team_TouchOurFlag( ent, enemy, team );
		}
		}		

	PrintCTFMessage(other->s.number, team, CTFMESSAGE_PLAYER_GOT_FLAG);

	if (team == TEAM_RED)
		cl->ps.powerups[PW_REDFLAG] = INT_MAX; // flags never expire
	else
		cl->ps.powerups[PW_BLUEFLAG] = INT_MAX; // flags never expire

	if (!(ent->flags & FL_DROPPED_ITEM)){ 
		// initial flag steal, so reset the team flaghold times
		// team is the color of the STOLEN flag, so we reset the run flaghold of the opposite team
		if ( team == TEAM_RED ) {
			level.redFlagStealTime = level.time;
			level.blueTeamRunFlaghold = 0;
		} else {
			level.blueFlagStealTime = level.time;
			level.redTeamRunFlaghold = 0;
		}
	} else {
		// this guy picked up a dropped flag, thus his run is invalid
		//other->client->runInvalid = qtrue;
	}

	// reset the speed stats for this run
	other->client->pers.fastcapTopSpeed = 0;
	other->client->pers.fastcapDisplacement = 0;
	other->client->pers.fastcapDisplacementSamples = 0;

	// picking up the flag removes invulnerability
	cl->ps.eFlags &= ~EF_INVULNERABLE;

	G_ResetAccurateTimerOnTrigger( &cl->pers.teamState.flagsince, other, ent );

	Team_SetFlagStatus( team, FLAG_TAKEN );
	AddScore(other, ent->r.currentOrigin, CTF_FLAG_BONUS);
	Team_TakeFlagSound( ent, team );

	return -1; // Do not respawn this automatically, but do delete it if it was FL_DROPPED
}

int Pickup_Team( gentity_t *ent, gentity_t *other ) {
	int team;
	gclient_t *cl = other->client;

	// figure out what team this flag is
	if( strcmp(ent->classname, "team_CTF_redflag") == 0 ) {
		team = TEAM_RED;
	}
	else if( strcmp(ent->classname, "team_CTF_blueflag") == 0 ) {
		team = TEAM_BLUE;
	}
	else if( strcmp(ent->classname, "team_CTF_neutralflag") == 0  ) {
		team = TEAM_FREE;
	}
	else {
//		PrintMsg ( other, "Don't know what team the flag is on.\n");
		return 0;
	}
	// GT_CTF
	if( team == cl->sess.sessionTeam) {
		return Team_TouchOurFlag( ent, other, team );
	}
	return Team_TouchEnemyFlag( ent, other, team );
}

/*
===========
Team_GetLocation

Report a location for the player. May use different systems as defined in G_LinkLocations.

Returns the configstring index of the location, or 0 if no location could be found.
If locationBuffer is not NULL, the location string will be directly written there.
============
*/
int Team_GetLocation( gentity_t *ent, char *locationBuffer, size_t locationBufferSize ) {
	vec3_t origin;
	VectorCopy( ent->r.currentOrigin, origin );

	if ( locationBuffer ) {
		locationBuffer[0] = '\0';
	}

	if ( level.locations.enhanced.numUnique ) {
		// using enhanced locations
		int		resultIndex = 0;
		void	*nearest;

		// we should always have at most 1 result
		nearest = kd_nearestf( level.locations.enhanced.lookupTree, origin );

		if ( nearest && kd_res_size( nearest ) == 1 ) {
			enhancedLocation_t *loc = ( enhancedLocation_t* )kd_res_item_data( nearest );

			if ( loc ) {
				if ( locationBuffer && ent->client ) {
					// we aren't writing to configstrings here, so we can format the team dynamically
					if ( loc->teamowner ) {
						if ( ent->client->ps.persistant[PERS_TEAM] == loc->teamowner ) {
							Com_sprintf( locationBuffer, locationBufferSize, "Our %s", loc->message );
						} else {
							Com_sprintf( locationBuffer, locationBufferSize, "Enemy %s", loc->message );
						}
					} else {
						Q_strncpyz( locationBuffer, loc->message, locationBufferSize );
					}
				}

				resultIndex = loc->cs_index;
			}
		}

		kd_res_free( nearest );

		return resultIndex;
	} else if ( level.locations.legacy.num ) {
		// using legacy locations
		legacyLocation_t	*loc, *best;
		vec_t				bestlen, len;
		int					i;

		best = NULL;
		bestlen = 3 * 8192.0*8192.0;

		for ( i = 0; i < level.locations.legacy.num; i++ ) {
			loc = &level.locations.legacy.data[i];

			len = DistanceSquared( origin, loc->origin );

			if ( len > bestlen ) {
				continue;
			}

			if ( !trap_InPVS( origin, loc->origin ) ) {
				continue;
			}

			bestlen = len;
			best = loc;
		}

		if ( best ) {
			if ( locationBuffer ) {
				if ( best->count ) {
					Com_sprintf( locationBuffer, locationBufferSize, "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, best->count + '0', best->message );
				} else {
					Com_sprintf( locationBuffer, locationBufferSize, "%s", best->message );
				}
			}

			return best->cs_index;
		}
	}

	return 0;
}

/*---------------------------------------------------------------------------*/

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define	MAX_TEAM_SPAWN_POINTS	32
gentity_t *SelectRandomTeamSpawnPoint( int teamstate, team_t team, int siegeClass ) {
	gentity_t	*spot;
	int			count;
	int			selection;
	gentity_t	*spots[MAX_TEAM_SPAWN_POINTS];
	char		*classname;
	qboolean	mustBeEnabled = qfalse;

	if (g_gametype.integer == GT_SIEGE)
	{
		if (team == SIEGETEAM_TEAM1)
		{
			classname = "info_player_siegeteam1";
		}
		else
		{
			classname = "info_player_siegeteam2";
		}

		mustBeEnabled = qtrue; //siege spawn points need to be "enabled" to be used (because multiple spawnpoint sets can be placed at once)
	}
	else
	{
		if (teamstate == TEAM_BEGIN) {
			if (team == TEAM_RED)
				classname = "team_CTF_redplayer";
			else if (team == TEAM_BLUE)
				classname = "team_CTF_blueplayer";
			else
				return NULL;
		} else {
			if (team == TEAM_RED)
				classname = "team_CTF_redspawn";
			else if (team == TEAM_BLUE)
				classname = "team_CTF_bluespawn";
			else
				return NULL;
		}
	}
	count = 0;

	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), classname)) != NULL) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}

		if (mustBeEnabled && !spot->genericValue1)
		{ //siege point that's not enabled, can't use it
			continue;
		}

		spots[ count ] = spot;
		if (++count == MAX_TEAM_SPAWN_POINTS)
			break;
	}

	// in ctf, rule out spawns that are too close to recently dropped flags if enabled
	if ( g_gametype.integer == GT_CTF && count && g_droppedFlagSpawnProtectionRadius.integer > 0 ) {
		int i;

		// look for any dropped flag here and remove matching spawns instead of
		// not adding them in the loop above (saves building an array of dropped flags,
		// since there can theoretically be more than 2)
		for ( i = 0; i < level.num_entities; ++i ) {
			gentity_t* ent = &g_entities[i];

			if ( !ent->inuse || !ent->item ) {
				continue;
			}

			if ( ent->item->giTag != PW_REDFLAG &&
				ent->item->giTag != PW_BLUEFLAG &&
				ent->item->giTag != PW_NEUTRALFLAG ) {
				continue;
			}

			if ( !( ent->flags & FL_DROPPED_ITEM ) ) {
				continue;
			}

			// this is a dropped flag

			// hack: nextthink is always level.time + 30000 at the time the flag was dropped
			int flagDroppedTime = ent->nextthink - 30000;

			// g_droppedFlagSpawnProtectionDuration is the time in ms during which dropped flags prevent nearby spawns
			// (minimum 1s, maximum 30s which is the time it takes the flag to respawn)
			if ( level.time > flagDroppedTime + Com_Clampi( 1000, 30000, g_droppedFlagSpawnProtectionDuration.integer ) ) {
				continue;
			}

			// this dropped flag still prevents spawns -- check all spawns against it			
			const float radiusSquared = ( float )( g_droppedFlagSpawnProtectionRadius.integer * g_droppedFlagSpawnProtectionRadius.integer );

			int j = 0;
			while ( j < count ) {
				if ( DistanceSquared( spots[j]->s.origin, ent->r.currentOrigin ) > radiusSquared ) {
					++j;
					continue;
				}

				// this spawn is close enough, remove it from the array of candidates

				if ( j + 1 < count ) { // if there are more spots, shift the array
					memmove( spots + j, spots + j + 1, ( count - j - 1 ) * sizeof( gentity_t* ) );
				}

				--count;
			}
		}
	}

	if ( !count ) {	// no valid spot
		return G_Find( NULL, FOFS(classname), classname);
	}

	if (g_gametype.integer == GT_SIEGE && siegeClass >= 0 &&
		bgSiegeClasses[siegeClass].name[0])
	{ //out of the spots found, see if any have an idealclass to match our class name
		int classCount = 0;
		int i = 0;

        while (i < count)
		{
			if (spots[i] && spots[i]->idealclass && spots[i]->idealclass[0] &&
				!Q_stricmp(spots[i]->idealclass, bgSiegeClasses[siegeClass].name))
			{ //this spot's idealclass matches the class name
				classCount++;
			}
			i++;
		}

		if (classCount > 0)
		{ //found at least one
			selection = rand() % classCount;
			return spots[ selection ];
		}
	}

	selection = rand() % count;
	return spots[ selection ];
}

gentity_t *SelectSiegeSpeedrunSpawnPoint(void) {
	static int lastTimeFirstSpotUsed = 0;
	static gentity_t *firstSpot = NULL;
	gentity_t	*spot = NULL;
	gentity_t	*nearestSpot = NULL, *secondNearestSpot = NULL;
	float		nearestDistance = 999999.0f, secondNearestDistance = 999999.0f;
	int			count = 0;

	// see if there's an objective we can measure the distance to
	gentity_t *obj = NULL;
	qboolean gotObj = qfalse;
	while ((obj = G_Find(obj, FOFS(classname), "info_siege_objective")) != NULL) {
		if (obj->objective != level.totalObjectivesCompleted + 1)
			continue;
		gotObj = qtrue;
		break;
	}

	if (gotObj) {
		while ((spot = G_Find(spot, FOFS(classname), "info_player_siegeteam1")) != NULL) {
			// allow telefrags to prevent cheesing respawn spots by intentionally blocking them
			/*if (SpotWouldTelefrag(spot))
				continue;*/

			if (!spot->genericValue1)
				continue; // must be enabled

			vec3_t diff;
			VectorSubtract(spot->s.origin, obj->s.origin, diff);
			diff[2] = 0.0f;
			float distance = VectorLength(diff);
			if (distance < nearestDistance) {
				nearestDistance = distance;
				nearestSpot = spot;
			}
		}
		while ((spot = G_Find(spot, FOFS(classname), "info_player_siegeteam1")) != NULL) {
			if (!spot->genericValue1)
				continue; // must be enabled
			if (spot == nearestSpot)
				continue;

			vec3_t diff;
			VectorSubtract(spot->s.origin, obj->s.origin, diff);
			float distance = VectorLength(diff);
			if (distance < secondNearestDistance) {
				secondNearestDistance = distance;
				secondNearestSpot = spot;
			}
		}
		if (nearestSpot && (level.time - lastTimeFirstSpotUsed > 500 || firstSpot != nearestSpot)) { // take the spot nearest the obj if it hasn't been used in the last 500ms, or the obj changed
			firstSpot = nearestSpot;
			lastTimeFirstSpotUsed = level.time;
			return nearestSpot;
		}
		if (secondNearestSpot)
			return secondNearestSpot;
	}
	else {
		while ((spot = G_Find(spot, FOFS(classname), "info_player_siegeteam1")) != NULL) {
			// allow telefrags to prevent cheesing respawn spots by intentionally blocking them
			/*if (SpotWouldTelefrag(spot))
				continue;*/

		if (!spot->genericValue1)
			continue; // must be enabled

			if (!count++) {
				if (level.time - lastTimeFirstSpotUsed > 0 || firstSpot != spot) { // take the first spot if it hasn't been used this tick
					firstSpot = spot;
					lastTimeFirstSpotUsed = level.time;
					return spot;
				}
				continue;
			}
			return spot; // take the second spot
		}
	}

	// if we got here, somehow there was no valid spot???
	assert(qfalse);
	SpeedRunModeRuined("SelectSiegeSpeedrunSpawnPoint: no valid spawnpoint");
	return G_Find(NULL, FOFS(classname), "info_player_siegeteam1");
}


/*
===========
SelectCTFSpawnPoint

============
*/
gentity_t *SelectCTFSpawnPoint ( team_t team, int teamstate, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;

	spot = SelectRandomTeamSpawnPoint ( teamstate, team, -1 );

	if (!spot) {
		return SelectSpawnPoint( vec3_origin, origin, angles, team );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*
===========
SelectSiegeSpawnPoint

============
*/
gentity_t *SelectSiegeSpawnPoint ( int siegeClass, team_t team, int teamstate, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;
	
	if (team == TEAM_RED && !level.mapCaptureRecords.speedRunModeRuined) {
		spot = SelectSiegeSpeedrunSpawnPoint();
	}
	else if (team == TEAM_BLUE) {
		SpeedRunModeRuined("SelectSiegeSpawnPoint: blue");
		spot = SelectRandomTeamSpawnPoint(teamstate, team, siegeClass);
	}
	else {
		spot = SelectRandomTeamSpawnPoint(teamstate, team, siegeClass);
	}

	if (!spot) {
		SpeedRunModeRuined("SelectSiegeSpawnPoint: !spot");
		return SelectSpawnPoint( vec3_origin, origin, angles, team );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*---------------------------------------------------------------------------*/

static int QDECL SortClients( const void *a, const void *b ) {
	return *(int *)a - *(int *)b;
}


/*
==================
TeamplayLocationsMessage

Format:
	clientNum location health armor weapon powerups

==================
*/
void TeamplayInfoMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[8192];
	int			stringlength;
	int			i, j;
	gentity_t	*player;
	int			cnt;
	int			h, a, p;
	int			clients[TEAM_MAXOVERLAY];

	if ( ! ent->client->pers.teamInfo )
		return;

	// figure out what client should be on the display
	// we are limited to 8, but we want to use the top eight players
	// but in client order (so they don't keep changing position on the overlay)
	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++) {
		player = g_entities + level.sortedClients[i];
		if (player->inuse && player->client->sess.sessionTeam == 
			ent->client->ps.persistant[PERS_TEAM] ) {
			clients[cnt++] = level.sortedClients[i];
		}
	}

	// We have the top eight players, sort them by clientNum
	qsort( clients, cnt, sizeof( clients[0] ), SortClients );

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;

	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++) {
		player = g_entities + i;
		if (player->inuse && player->client->sess.sessionTeam == 
			ent->client->ps.persistant[PERS_TEAM] ) {
			
			// duo: fix incorrectly showing that dead tempspeccing siege players have 1 hp
			h = g_gametype.integer == GT_SIEGE && player->client->tempSpectate >= level.time ? 0 : player->client->ps.stats[STAT_HEALTH];
			a = player->client->ps.stats[STAT_ARMOR];

			if (h < 0) h = 0;

			if (g_teamOverlayForce.integer) {
				// modified behavior: compatible client mods can display everything properly; incompatible clients will see force in place of armor
				// armor slot = force power points
				// powerups slot = armor in the upper 8 (unused) bits; actual powerups in the lower bits as normal
				a = player->client->ps.fd.forcePowersKnown ? player->client->ps.fd.forcePower : -1;
				byte clampedArmor = (byte)Com_Clampi(0, 255, player->client->ps.stats[STAT_ARMOR]);
				p = (int)(clampedArmor << 24);
				p |= player->s.powerups;
			}
			else {
				// normal behavior
				// armor slot = armor
				// powerups slot = powerups
				a = player->client->ps.stats[STAT_ARMOR];
				if (a < 0) a = 0;
				p = player->s.powerups;
			}

			

			Com_sprintf (entry, sizeof(entry),
				" %i %i %i %i %i %i", 
//				level.sortedClients[i], player->client->pers.teamState.location, h, a, 
				i, level.zombies && player->client->sess.sessionTeam == TEAM_BLUE ? 0 : player->client->pers.teamState.location, h, a,
				player->client->ps.weapon, p);
			j = strlen(entry);
			if (stringlength + j > sizeof(string))
				break;
			strcpy (string + stringlength, entry);
			stringlength += j;
			cnt++;
		}
	}

	trap_SendServerCommand( ent-g_entities, va("tinfo %i %s", cnt, string) );
}

void CheckTeamStatus(void) {
	int i;
	gentity_t *ent;

    //OSP: pause
    if ( level.pause.state != PAUSE_NONE )
            return;

	if (level.intermissiontime)
		return; // don't spam teamoverlay updates during intermission

	int updateRate = Com_Clampi(1, 1000, g_teamOverlayUpdateRate.integer);
	if (level.time - level.lastTeamLocationTime > updateRate) {

		level.lastTeamLocationTime = level.time;

		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;

			if ( !ent->client )
			{
				continue;
			}

			if ( ent->client->pers.connected != CON_CONNECTED ) {
				continue;
			}

			if (ent->inuse && (ent->client->sess.sessionTeam == TEAM_RED ||	ent->client->sess.sessionTeam == TEAM_BLUE)) {
				ent->client->pers.teamState.location = Team_GetLocation( ent, NULL, 0 );
			}
		}

		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;

			if ( !ent->client || ent->client->pers.connected != CON_CONNECTED ) {
				continue;
			}

			if (ent->inuse && (ent->client->ps.persistant[PERS_TEAM] == TEAM_RED ||	ent->client->ps.persistant[PERS_TEAM] == TEAM_BLUE) && !ent->client->isLagging) {
				TeamplayInfoMessage( ent );
			}
		}
	}
}

/*-----------------------------------------------------------------*/

/*QUAKED team_CTF_redplayer (1 0 0) (-16 -16 -16) (16 16 32)
Only in CTF games.  Red players spawn here at game start.
*/
void SP_team_CTF_redplayer( gentity_t *ent ) {
}


/*QUAKED team_CTF_blueplayer (0 0 1) (-16 -16 -16) (16 16 32)
Only in CTF games.  Blue players spawn here at game start.
*/
void SP_team_CTF_blueplayer( gentity_t *ent ) {
}


/*QUAKED team_CTF_redspawn (1 0 0) (-16 -16 -24) (16 16 32)
potential spawning position for red team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_redspawn(gentity_t *ent) {
}

/*QUAKED team_CTF_bluespawn (0 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for blue team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_bluespawn(gentity_t *ent) {
}


