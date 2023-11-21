// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

#include "cJSON.h"

#include "menudef.h"			// for the voice chats

//rww - for getting bot commands...
int AcceptBotCommand(char *cmd, gentity_t *pl);
//end rww

#include "namespace_begin.h"
void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );
#include "namespace_end.h"

void Cmd_NPC_f( gentity_t *ent );
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[1400];
	int			stringlength;
	int			i, j;
	gclient_t	*cl;
	int			numSorted, accuracy;
	int         statsMix;

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;

	numSorted = level.numConnectedClients;
	
	if (numSorted > MAX_CLIENT_SCORE_SEND)
	{
		numSorted = MAX_CLIENT_SCORE_SEND;
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			//test


			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

			if (ping == 0 && !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT))
				ping = 1;

		}

		if( cl->accuracy_shots ) {
			accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
		}
		else {
			accuracy = 0;
		}

		//lower 16 bits say average return time
		//higher 16 bits say how many times did player get the flag
		statsMix = cl->pers.teamState.th;
		statsMix |= ((cl->pers.teamState.te) << 16);

		// first 16 bits are unused
		// next 10 bits are real ping
		// next 5 bits are followed client #
		// final bit is whether you are following someone or not
		int specMix = 0; // this was previously scoreFlags
		if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW &&
			!cl->sess.isInkognito && !(g_lockdown.integer && !G_ClientIsWhitelisted(level.sortedClients[i]))) {
			specMix = 0b1;
			specMix |= ((cl->sess.spectatorClient & 0b11111) << 1);
		}
		int realPing;
		if (cl->pers.connected != CON_CONNECTED)
			realPing = -1;
		else if (cl->isLagging)
			realPing = 999;
		else
			realPing = Com_Clampi(-1, 999, cl->realPing);
		specMix |= ((realPing & 0b1111111111) << 6);

		Com_sprintf (entry, sizeof(entry),
			" %i %i %i %i %i %i %i %i %i %i %i %i %i %i", level.sortedClients[i],
			cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime)/60000,
			specMix, g_entities[level.sortedClients[i]].s.powerups, accuracy, 
			
			cl->pers.teamState.fragcarrier, //this can be replaced
			                                       //but only here!
												   //server uses this value
			/*
			//sending number where
			//lower 16 bits say average return time
			//higher 16 bits say how many times did player get the flag

			statsMix,
			*/

			cl->pers.teamState.flaghold,
			cl->pers.teamState.flagrecovery, 
			cl->ps.persistant[PERS_DEFEND_COUNT], 
			cl->ps.persistant[PERS_ASSIST_COUNT], 

			statsMix,
			/*
			perfect, //this can be replaced
			         //but how can we manipulate with it on client side?
			*/
			cl->ps.persistant[PERS_CAPTURES]);
		j = strlen(entry);
		if (stringlength + j > 1022)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	//still want to know the total # of clients
	i = level.numConnectedClients;

	trap_SendServerCommand( ent-g_entities, va("scores %i %i %i%s", i, 
		level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
		string ) );
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}

typedef struct version_s {
	int		major;
	int		minor;
	int		revision;
} version_t;

static void VersionStringToNumbers(const char *s, version_t *out) {
	assert(VALIDSTRING(s) && out);
	out->major = out->minor = out->revision = 0;

	if (!VALIDSTRING(s) || *s < '0' || *s > '9' || strlen(s) >= 16)
		return;

	char *r = (char *)s;
	for (int i = 0; i < 3; i++) {
		char temp[16] = { 0 }, *w;
		for (w = temp; *r && *r >= '0' && *r <= '9'; r++, w++) // loop through all number chars, copying to the temporary buffer
			*w = *r;
		switch (i) { // set the number
		case 0:		out->major = atoi(temp);		break;
		case 1:		out->minor = atoi(temp);		break;
		case 2:		out->revision = atoi(temp);		break;
		}
		while (*r < '0' || *r > '9') {
			if (!*r)
				return;
			r++;
		}
	}
}

// returns 1 if verStr is greater, 0 if same, -1 if verStr is older
int CompareVersions(const char *verStr, const char *compareToStr) {
	if (!VALIDSTRING(verStr) && !VALIDSTRING(compareToStr))
		return 0;
	if (!VALIDSTRING(verStr))
		return -1;
	if (!VALIDSTRING(compareToStr))
		return 1;

	version_t ver, compareTo;
	VersionStringToNumbers(verStr, &ver);
	VersionStringToNumbers(compareToStr, &compareTo);

	if (ver.major > compareTo.major)
		return 1;
	else if (ver.major < compareTo.major)
		return -1;

	// same major
	if (ver.minor > compareTo.minor)
		return 1;
	else if (ver.minor < compareTo.minor)
		return -1;

	// same major and same minor
	if (ver.revision > compareTo.revision)
		return 1;
	else if (ver.revision < compareTo.revision)
		return -1;

	// equal
	return 0;
}

void Cmd_Unlagged_f(gentity_t *ent) {
	if (!g_unlagged.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"This server has completely disabled unlagged support.\n\"");
		return;
	}

	// only allow the command to be used by the unascended
	if (ent->client->sess.auth != INVALID && CompareVersions(ent->client->sess.nmVer, "1.5.6") >= 0) {
		trap_SendServerCommand(ent - g_entities, "print \"Invalid command. You must use the ^5cg_unlagged^7 cvar instead.\n\"");
		return;
	}

	if (g_unlagged.integer == 2) { // special testing mode, don't let people use it with the command
		trap_SendServerCommand(ent - g_entities, "print \"Invalid command.\n\"");
		return;
	}

	ent->client->sess.unlagged ^= UNLAGGED_COMMAND;

	if (ent->client->sess.unlagged & UNLAGGED_COMMAND) {
		char msg[MAX_STRING_CHARS] = "print \"Unlagged ^2enabled^7.";
		if (!ent->client->sess.basementNeckbeardsTriggered) {
			if (ent->client->sess.auth != INVALID)
				Q_strcat(msg, sizeof(msg), " For the best unlagged experience, update to Newmod 1.6.0 and enable cg_unlagged.");
			else
				Q_strcat(msg, sizeof(msg), " For the best unlagged experience, use Newmod and enable cg_unlagged.");
			ent->client->sess.basementNeckbeardsTriggered = qtrue;
		}
		Q_strcat(msg, sizeof(msg), "\n\"");
		trap_SendServerCommand(ent - g_entities, msg);
		Com_Printf("Client %d (%s^7) enabled unlagged\n", ent - g_entities, ent->client->pers.netname);
	}
	else {
		trap_SendServerCommand(ent - g_entities, "print \"Unlagged ^1disabled^7.\n\"");
		Com_Printf("Client %d (%s^7) disabled unlagged\n", ent - g_entities, ent->client->pers.netname);
	}
}

/*
==================
CheatsOk
==================
*/
qboolean	CheatsOk( gentity_t *ent ) {
	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return qfalse;
	}
	if ( ent->health <= 0 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")));
		return qfalse;
	}
	return qtrue;
}


/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = trap_Argc();
	for ( i = start ; i < c ; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/*
==================
SanitizeString

Remove case and control characters
==================
*/
void SanitizeString( char *in, char *out ) {
	while ( *in ) {
		if ( *in == 27 ) {
			in += 2;		// skip color code
			continue;
		}
		if ( *in < 32 ) {
			in++;
			continue;
		}
		*out++ = tolower( (unsigned char) *in++ );
	}

	*out = 0;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, char *s ) {
	gclient_t	*cl;
	int			idnum;
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			trap_SendServerCommand( to-g_entities, va("print \"Bad client slot: %i\n\"", idnum));
			return -1;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected != CON_CONNECTED ) {
			trap_SendServerCommand( to-g_entities, va("print \"Client %i is not active\n\"", idnum));
			return -1;
		}
		return idnum;
	}

	// check for a name match
	SanitizeString( s, s2 );
	for ( idnum=0,cl=level.clients ; idnum < level.maxclients ; idnum++,cl++ ) {
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) {
			return idnum;
		}
	}

	trap_SendServerCommand( to-g_entities, va("print \"User %s is not on the server\n\"", s));
	return -1;
}

extern void Svcmd_KillTurrets_f(qboolean announce);

void Cmd_KillTurrets_f(gentity_t *ent)
{
	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}
	Svcmd_KillTurrets_f(qtrue);
}

extern void Blocked_Door(gentity_t *ent, gentity_t *other, gentity_t *blockedBy);
extern void UnLockDoors(gentity_t *const ent);

void Cmd_GreenDoors_f(gentity_t *ent)
{
	gentity_t *doorent;
	int i = 0;

	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}

	for (i = 0; i < MAX_GENTITIES; i++)
	{
		doorent = &g_entities[i];
		if (doorent->blocked && doorent->blocked == Blocked_Door && doorent->spawnflags && doorent->spawnflags & 16)
		{
			UnLockDoors(doorent);
		}
	}
}

void Cmd_DuoTest_f(gentity_t *ent)
{
	int i = 0;

	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}

	Svcmd_KillTurrets_f(qfalse);
	Cmd_GreenDoors_f(ent);
	trap_Cvar_Set("g_siegeRespawn", "1");
	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		ent->client->ps.stats[STAT_WEAPONS] = (1 << (LAST_USEABLE_WEAPON + 1)) - (1 << WP_NONE);
		for (i = 0; i < MAX_WEAPONS; i++)
		{
			ent->client->ps.ammo[i] = 999;
		}
		for (i = 0; i < NUM_FORCE_POWERS; i++)
		{
			ent->client->ps.fd.forcePowerLevel[i] = 3;
			ent->client->ps.fd.forcePowersKnown |= (1 << i);
		}
	}
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (gentity_t *cmdent, int baseArg)
{
	char		name[MAX_TOKEN_CHARS];
	gentity_t	*ent;
	gitem_t		*it;
	int			i;
	qboolean	give_all;
	gentity_t		*it_ent;
	trace_t		trace;
	char		arg[MAX_TOKEN_CHARS];

	if ( !CheatsOk( cmdent ) ) {
		return;
	}

	if (baseArg)
	{
		char otherindex[MAX_TOKEN_CHARS];

		trap_Argv( 1, otherindex, sizeof( otherindex ) );

		if (!otherindex[0])
		{
			Com_Printf("giveother requires that the second argument be a client index number.\n");
			return;
		}

		i = atoi(otherindex);

		if (i < 0 || i >= MAX_CLIENTS)
		{
			Com_Printf("%i is not a client index\n", i);
			return;
		}

		ent = &g_entities[i];

		if (!ent->inuse || !ent->client)
		{
			Com_Printf("%i is not an active client\n", i);
			return;
		}
	}
	else
	{
		ent = cmdent;
	}

	trap_Argv( 1+baseArg, name, sizeof( name ) );

	if (Q_stricmp(name, "all") == 0)
		give_all = qtrue;
	else
		give_all = qfalse;

	if (give_all)
	{
		i = 0;
		while (i < HI_NUM_HOLDABLE)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << i);
			i++;
		}
		i = 0;
	}

	if (give_all || Q_stricmp( name, "health") == 0)
	{
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			ent->health = atoi(arg);
			if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH]) {
				ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
			}
		}
		else {
			ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "force") == 0)
	{
		if (trap_Argc() == 3 + baseArg) {
			trap_Argv(2 + baseArg, arg, sizeof(arg));
			ent->client->ps.fd.forcePower = Com_Clampi(0, 999, atoi(arg));
		}
		else {
			ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		ent->client->ps.stats[STAT_WEAPONS] = (1 << (LAST_USEABLE_WEAPON+1))  - ( 1 << WP_NONE );
		if (!give_all)
			return;
	}
	
	if ( !give_all && Q_stricmp(name, "weaponnum") == 0 )
	{
		trap_Argv( 2+baseArg, arg, sizeof( arg ) );
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << atoi(arg));
		return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		int num = 999;
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			num = atoi(arg);
		}
		for ( i = 0 ; i < MAX_WEAPONS ; i++ ) {
			ent->client->ps.ammo[i] = num;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			ent->client->ps.stats[STAT_ARMOR] = atoi(arg);
			if (g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1) {
				int maxArmor = bgSiegeClasses[ent->client->siegeClass].maxarmor;
				if (maxArmor && ent->client->ps.stats[STAT_ARMOR] > maxArmor)
					ent->client->ps.stats[STAT_ARMOR] = maxArmor;
			}
		} else {
			if (g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1) {
				int maxArmor = bgSiegeClasses[ent->client->siegeClass].maxarmor;
				if (maxArmor)
					ent->client->ps.stats[STAT_ARMOR] = maxArmor;
			}
			else {
				ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
			}
		}

		if (!give_all)
			return;
	}

	// spawn a specific item right on the player
	if ( !give_all ) {
		it = BG_FindItem (name);
		if (!it) {
			return;
		}

		it_ent = G_Spawn();
		VectorCopy( ent->r.currentOrigin, it_ent->s.origin );
		it_ent->classname = it->classname;
		G_SpawnItem (it_ent, it);
		FinishSpawningItem(it_ent );
		memset( &trace, 0, sizeof( trace ) );
		Touch_Item (it_ent, ent, &trace);
		if (it_ent->inuse) {
			G_FreeEntity( it_ent );
		}
	}

}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (gentity_t *ent)
{
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	if ( ent->client->noclip ) {
		msg = "noclip OFF\n";
	} else {
		msg = "noclip ON\n";
	}
	ent->client->noclip = !ent->client->noclip;

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent ) {
	if ( !CheatsOk( ent ) ) {
		return;
	}

	// doesn't work in single player
	if ( g_gametype.integer != 0 ) {
		trap_SendServerCommand( ent-g_entities, 
			"print \"Must be in g_gametype 0 for levelshot\n\"" );
		return;
	}

	BeginIntermission();
	trap_SendServerCommand( ent-g_entities, "clientLevelShot" );
}


/*
==================
Cmd_TeamTask_f

From TA.
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap_Argc() != 2 ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap_GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap_SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}

static qboolean HasDetpackInWorld(gentity_t *ent) {
	qboolean hasDetpack = qfalse;
	for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
		gentity_t *detpack = &g_entities[i];
		if (!detpack->classname || strcmp(detpack->classname, "detpack") || detpack->r.ownerNum != ent - g_entities || !detpack->inuse)
			continue;
		if (ent->client->siegeClass != -1 && bgSiegeClasses[ent->client->siegeClass].detKillDelay &&
			level.time - detpack->siegeItemSpawnTime < bgSiegeClasses[ent->client->siegeClass].detKillDelay)
			continue; // don't count detpacks which are not valid for being detkilled yet (due to delay for the player's class)
		return qtrue;
	}
	return qfalse;
}

static void SendAntiSelfMaxMessage(int clientNum, qboolean isClassChange) {
	if (clientNum >= MAX_CLIENTS) {
		assert(qfalse);
		return;
	}

	gentity_t *selfkiller = &g_entities[clientNum];
	if (!selfkiller->client || selfkiller->client->sess.sessionTeam == TEAM_SPECTATOR) // sanity check (should never happen)
		return;

	if (isClassChange)
		PrintIngame(clientNum, "You were blocked from changing class to prevent a self-max.\n");
	else
		PrintIngame(clientNum, "You were blocked from selfkilling to prevent a self-max.\n");

	static int lastMessageTime[MAX_CLIENTS][MAX_CLIENTS] = { 0 };

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisGuy = &g_entities[i];
		if (!thisGuy->client || thisGuy->client->pers.connected == CON_DISCONNECTED || !thisGuy->inuse || i == clientNum)
			continue;

		if (lastMessageTime[clientNum][i] && level.time - lastMessageTime[clientNum][i] < 2000)
			continue; // don't spam

		qboolean sendYouTriedToSelfkillMessage = qfalse, sendTeammateTriedToSelfkillMessage = qfalse;
		if (thisGuy->client->sess.sessionTeam == TEAM_SPECTATOR) {
			if (thisGuy->client->sess.spectatorState == SPECTATOR_FOLLOW && thisGuy->client->sess.spectatorClient == clientNum) {
				sendYouTriedToSelfkillMessage = qtrue; // this guy is a spec following the selfkiller directly
			}
			else if (thisGuy->client->sess.spectatorState == SPECTATOR_FOLLOW && thisGuy->client->sess.spectatorClient != clientNum &&
				thisGuy->client->sess.spectatorClient < MAX_CLIENTS && g_entities[thisGuy->client->sess.spectatorClient].inuse &&
				g_entities[thisGuy->client->sess.spectatorClient].client && g_entities[thisGuy->client->sess.spectatorClient].client->sess.sessionTeam == selfkiller->client->sess.sessionTeam) {
				sendTeammateTriedToSelfkillMessage = qtrue; // this guy is a spec following a teammate of the selfkiller
			}
		}
		else if (thisGuy->client->sess.sessionTeam == selfkiller->client->sess.sessionTeam) {
			sendTeammateTriedToSelfkillMessage = qtrue; // this guy is a teammate of the selfkiller
		}

		if (sendYouTriedToSelfkillMessage) {
			lastMessageTime[clientNum][i] = level.time;
			if (isClassChange)
				PrintIngame(i, "You were blocked from changing class to prevent a self-max.\n");
			else
				PrintIngame(i, "You were blocked from selfkilling to prevent a self-max.\n");
		}
		else if (sendTeammateTriedToSelfkillMessage) {
			lastMessageTime[clientNum][i] = level.time;
			if (isClassChange)
				PrintIngame(i, "%s%s^7 was blocked from changing class to prevent a self-max.\n", NM_SerializeUIntToColor(clientNum), selfkiller->client->pers.netname);
			else
				PrintIngame(i, "%s%s^7 was blocked from selfkilling to prevent a self-max.\n", NM_SerializeUIntToColor(clientNum), selfkiller->client->pers.netname);
		}
	}
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent ) {
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}
	if (ent->health <= 0) {
		return;
	}

    if ( ent->client->tempSpectate > level.time )
    {
        return;
    }

    //OSP: pause
    if ( level.pause.state != PAUSE_NONE )
            return;

	if ((g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) &&
		level.numPlayingClients > 1 && !level.warmupTime)
	{
		if (!g_allowDuelSuicide.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "ATTEMPTDUELKILL")) );
			return;
		}
	}

	if (g_gametype.integer == GT_SIEGE && g_antiSelfMax.integer && g_siegeRespawn.integer >= 10 && (level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2)) {
		int timeSinceRespawn = (level.time + (g_siegeRespawn.integer * 1000)) - level.siegeRespawnCheck;
		if (timeSinceRespawn < 3000 && !HasDetpackInWorld(ent)) { // players cannot sk within 3 seconds AFTER the spawn wave, unless they have a detpack in the world
			return;
		}
		if (ent->client->sess.skillBoost || g_antiSelfMax.integer) {
			int oneSecBeforeRespawn = (g_siegeRespawn.integer - 1) * 1000;
			if (timeSinceRespawn >= oneSecBeforeRespawn && !HasDetpackInWorld(ent)) { // cannot sk within 1 second BEFORE the spawn wave, too
				SendAntiSelfMaxMessage(ent - g_entities, qfalse);
				return;
			}
		}
	}

	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
	player_die (ent, ent, ent, 100000, MOD_SUICIDE);
}

gentity_t *G_GetDuelWinner(gclient_t *client)
{
	gclient_t *wCl;
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		wCl = &level.clients[i];
		
		if (wCl && wCl != client && /*wCl->ps.clientNum != client->ps.clientNum &&*/
			wCl->pers.connected == CON_CONNECTED && wCl->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return &g_entities[wCl->ps.clientNum];
		}
	}

	return NULL;
}

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	char buffer[512];
	client->ps.fd.forceDoInit = 1; //every time we change teams make sure our force powers are set right

	*buffer = '\0';

	if ( client->sess.sessionTeam == TEAM_RED ) {
		Q_strncpyz(buffer, 
			va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEREDTEAM")),
			sizeof(buffer));

	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		Q_strncpyz(buffer, 
			va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBLUETEAM")),
			sizeof(buffer));

	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		Q_strncpyz(buffer, 
			va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")),
			sizeof(buffer));

	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		{
			//NOTE: Just doing a vs. once it counts two players up
		}
		else
		{
			Q_strncpyz(buffer, 
				va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
				client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")),
				sizeof(buffer));
		}
	}

	if (*buffer){
		trap_SendServerCommand( -1, va("cp \"%s\n\"",buffer) );
		trap_SendServerCommand( -1, va("print \"%s\n\"",buffer) );
	}

	G_LogPrintf ( "setteam:  %i %s %s\n",
				  client - &level.clients[0],
				  TeamName ( oldTeam ),
				  TeamName ( client->sess.sessionTeam ) );
}

qboolean G_PowerDuelCheckFail(gentity_t *ent)
{
	int			loners = 0;
	int			doubles = 0;

	if (!ent->client || ent->client->sess.duelTeam == DUELTEAM_FREE)
	{
		return qtrue;
	}

	G_PowerDuelCount(&loners, &doubles, qfalse);

	if (ent->client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
	{
		return qtrue;
	}

	if (ent->client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
	{
		return qtrue;
	}

	return qfalse;
}

/*
=================
SetTeam
=================
*/
qboolean g_dontPenalizeTeam = qfalse;
qboolean g_preventTeamBegin = qfalse;
void SetTeam( gentity_t *ent, char *s, qboolean forceteamed ) {
	int					team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;

	//base enhanced fix, sometimes we come here with invalid 
	//entity sloty and this procedure then creates fake player
	if (!ent->inuse){
		return;
	}

	//
	// see what change is requested
	//
	client = ent->client;

	clientNum = client - level.clients;
	specClient = 0;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_SCOREBOARD;
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmpn( s, "s", 1 ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( g_gametype.integer >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmpn( s, "r", 1 ) || !Q_stricmpn(s, "o", 1) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmpn( s, "b", 1 ) || !Q_stricmpn(s, "d", 1)) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			//For now, don't do this. The legalize function will set powers properly now.
				team = PickTeam( clientNum );
			//}
		}

		if ( g_teamForceBalance.integer && !g_trueJedi.integer ) {
			int		counts[TEAM_NUM_TEAMS];

			counts[TEAM_BLUE] = TeamCount( ent-g_entities, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( ent-g_entities, TEAM_RED );

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				{
					trap_SendServerCommand( ent->client->ps.clientNum, 
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
				}
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				{
					trap_SendServerCommand( ent->client->ps.clientNum, 
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
				}
				return; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}
	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	//pending server commands overflow might cause that this
	//client is no longer valid, lets check it for it here and few other places
	if (!ent->inuse){
		return;
	}

	if (g_lockdown.integer && !(ent->r.svFlags & SVF_BOT) && !G_ClientIsWhitelisted(ent - g_entities) && !forceteamed) {
		return;
	}

	if (g_probation.integer >= 2 && G_ClientIsOnProbation(ent - g_entities) && !forceteamed) {
		trap_SendServerCommand(ent - g_entities, va("print \"You are on probation and cannot switch teams without being forceteamed by an admin.\n\""));
		return;
	}

	if (ent->forcedTeamTime > level.time && !forceteamed)
	{
		//raped
		trap_SendServerCommand(ent - g_entities, va("print \"You have been forceteamed recently. Please wait %i seconds before changing teams again.\n\"", (ent->forcedTeamTime - level.time + 500) / 1000));
		return;
	}

	if (g_gametype.integer == GT_SIEGE)
	{
		qboolean teamChanged = qfalse;
		if (forceteamed == qfalse && (ent->health <= 0 ||  (client->tempSpectate >= level.time &&
			team == TEAM_SPECTATOR) || ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER))
		{ //sorry, can't do that.
			return;
		}

		if (level.zombies && forceteamed == qfalse && team == TEAM_SPECTATOR && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			trap_SendServerCommand(ent - g_entities, "print \"You cannot join the spectators during zombies.\n\"");
			return;
		}

		// Only check one way, so you can join spec back if you were forced as a passwordless spectator
		if (team != TEAM_SPECTATOR && !client->sess.canJoin && !forceteamed) {
			trap_SendServerCommand(ent - g_entities,
				"cp \"^7You may not join due to incorrect/missing password\n^7If you know the password, just use /password\n\"");
			trap_SendServerCommand(ent - g_entities,
				"print \"^7You may not join due to incorrect/missing password\n^7If you know the password, just use /password\n\"");
			return;
		}

		if (client->sess.siegeDesiredTeam != team)
		{
			teamChanged = qtrue;
		}

		if (g_preventJoiningLargerTeam.integer && g_gametype.integer >= GT_TEAM &&
			!forceteamed && !(ent->r.svFlags & SVF_BOT) && teamChanged && (team == TEAM_RED || team == TEAM_BLUE)) {
			int numRed = 0, numBlue = 0;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *thisEnt = &g_entities[i];
				if (thisEnt == ent || !thisEnt->inuse || !thisEnt->client)
					continue;
				if (thisEnt->client->sess.siegeDesiredTeam == TEAM_RED)
					++numRed;
				else if (thisEnt->client->sess.siegeDesiredTeam == TEAM_BLUE)
					++numBlue;
			}
			if ((team == TEAM_RED && numRed > numBlue) || (team == TEAM_BLUE && numBlue > numRed)) {
				trap_SendServerCommand(ent - g_entities,
					"cp \"^7Please join the other team to help balance the teams.\n\"");
				trap_SendServerCommand(ent - g_entities,
					"print \"^7Please join the other team to help balance the teams.\n\"");
				return;
			}
		}

		client->sess.siegeDesiredTeam = team;

		if (client->sess.sessionTeam != TEAM_SPECTATOR && team != TEAM_SPECTATOR) {
			//not a spectator now, and not switching to spec, so you have to wait til you die.
			//trap_SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time you respawn.\n\"") );
			qboolean doBegin;
			if (ent->client->tempSpectate >= level.time)
			{
				doBegin = qfalse;
			}
			else
			{
				doBegin = qtrue;
			}

			if (doBegin)
			{
				// Kill them so they automatically respawn in the team they wanted.
				if (ent->health > 0)
				{
					ent->flags &= ~FL_GODMODE;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
					player_die( ent, ent, ent, 100000, MOD_TEAM_CHANGE ); 
				}
			}

			if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam)
			{
				SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, qfalse);
				if (teamChanged)
				{
					BroadcastTeamChange(client, client->sess.sessionTeam);
				}
			}

			// since we switched teams, update their siege help
			SendSiegeHelpForClient(clientNum, team == TEAM_SPECTATOR ? TEAM_SPECTATOR : client->sess.siegeDesiredTeam);
			return;
		}

		// since we switched teams, update their siege help
		SendSiegeHelpForClient(clientNum, team == TEAM_SPECTATOR ? TEAM_SPECTATOR : client->sess.siegeDesiredTeam);

		if (!teamChanged)
		{
			return;
		}
	}

	// override decision if limiting the players
	if ( (g_gametype.integer == GT_DUEL)
		&& level.numNonSpectatorClients >= 2 )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( (g_gametype.integer == GT_POWERDUEL)
		&& (level.numPlayingClients >= 3 || G_PowerDuelCheckFail(ent)) )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( g_maxGameClients.integer > 0 && 
		level.numNonSpectatorClients >= g_maxGameClients.integer )
	{
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	oldTeam = client->sess.sessionTeam;
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	// Only check one way, so you can join spec back if you were forced as a passwordless spectator
	if (team != TEAM_SPECTATOR && !client->sess.canJoin && !forceteamed) {
		trap_SendServerCommand( ent - g_entities,
			"cp \"^7You may not join due to incorrect/missing password\n^7If you know the password, just use /password\n\"" );
		trap_SendServerCommand(ent - g_entities,
			"print \"^7You may not join due to incorrect/missing password\n^7If you know the password, just use /password\n\"");
		return;
	}

	//
	// execute the team change
	//

	memset(&client->sess.spawnedSiegeClass, 0, sizeof(client->sess.spawnedSiegeClass));
	memset(&client->sess.spawnedSiegeModel, 0, sizeof(client->sess.spawnedSiegeModel));
	client->sess.spawnedSiegeMaxHealth = 0;

	// if the player was dead leave the body
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		MaintainBodyQueue(ent);
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		g_dontPenalizeTeam = qtrue;
		player_die (ent, ent, ent, 100000, MOD_TEAM_CHANGE);
		g_dontPenalizeTeam = qfalse;
	}
	// they go to the end of the line for tournements
	if ( team == TEAM_SPECTATOR ) {
		if ( (g_gametype.integer != GT_DUEL) || (oldTeam != TEAM_SPECTATOR) )	{//so you don't get dropped to the bottom of the queue for changing skins, etc.
			client->sess.spectatorTime = level.time;
		}
	}

	if (!level.mapCaptureRecords.speedRunModeRuined && g_gametype.integer == GT_SIEGE && ent - g_entities < MAX_CLIENTS &&
		(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
		(team == TEAM_BLUE || (team == TEAM_RED && level.siegeRoundStartTime && level.time - level.siegeRoundStartTime >= 10000))) {
		// joining a non-spectator team after 10s+ into the round breaks speedrun mode
		level.siegeTopTimes[ent - g_entities].hasChangedTeams = qtrue;
		SpeedRunModeRuined("SetTeam: joined non-spectator team mid-game");
	}

	client->sess.sessionTeam = team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	//pending server commands overflow might cause that this
	//client is no longer valid, lets check it for it here and few other places
	if (!ent->inuse){
		return;
	}

	level.teamChangeTime[ent - g_entities] = level.time;
	BroadcastTeamChange( client, oldTeam );
    // G_LogDbLogLevelEvent( level.db.levelId, level.time - level.startTime, levelEventTeamChanged, client->sess.sessionId, oldTeam, team, 0, 0 );

	//make a disappearing effect where they were before teleporting them to the appropriate spawn point,
	//if we were not on the spec team
	if (oldTeam != TEAM_SPECTATOR)
	{
		gentity_t *tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = clientNum;
	}

	// get and distribute relevent paramters
	ClientUserinfoChanged( clientNum );

	//pending server commands overflow might cause that this
	//client is no longer valid, lets check it for it here and few other places
	if (!ent->inuse){
		return;
	}

	if (!g_preventTeamBegin)
	{
		ClientBegin( clientNum, qfalse );
	}

	//vote delay
	if (team != oldTeam && oldTeam == TEAM_SPECTATOR){
		client->lastJoinedTime = level.time;
	}
}

void SetNameQuick( gentity_t *ent, char *s, int renameDelay ) {
	char	userinfo[MAX_INFO_STRING];

	trap_GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
	Info_SetValueForKey(userinfo, "name", s);
	trap_SetUserinfo( ent->s.number, userinfo );

	ent->client->pers.netnameTime = level.time - 1; // bypass delay
	ClientUserinfoChanged( ent->s.number );
	// TODO: display something else than "5 seconds" to the player
	ent->client->pers.netnameTime = level.time + ( renameDelay < 0 ? 0 : renameDelay );
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void StopFollowing( gentity_t *ent ) {
	vec3_t origin, viewangles;
	int commandTime;
	int ping;

	// save necessary values
	commandTime = ent->client->ps.commandTime;
	ping = ent->client->ps.ping;
	VectorCopy(ent->client->ps.origin,origin);
	VectorCopy(ent->client->ps.viewangles,viewangles);

	// clean spec's playerstate
	memset(&ent->client->ps,0,sizeof(ent->client->ps));

	// set necessary values
	ent->client->ps.commandTime = commandTime;
	ent->client->ps.ping = ping;
	VectorCopy(origin,ent->client->ps.origin);
	VectorCopy(viewangles,ent->client->ps.viewangles);

	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;	
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;	
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.clientNum = ent - g_entities;
	ent->client->ps.stats[STAT_HEALTH] = 100;
	ent->client->ps.cloakFuel = 100;
	ent->client->ps.jetpackFuel = 100;
	ent->client->ps.crouchheight = CROUCH_MAXS_2;
	ent->client->ps.standheight = DEFAULT_MAXS_2;

	SiegeGhostUpdate(ent - g_entities, qtrue); // force a ghost update since we changed follow targets
}

#ifdef NEWMOD_SUPPORT
static char *SiegeClassInfoForcePowersString(siegeClass_t *scl) {
	if (!scl) {
		assert(qfalse);
		return NULL;
	}

	static char forceBuf[64] = { 0 };
	memset(&forceBuf, 0, sizeof(forceBuf));
	for (forcePowers_t f = FP_HEAL; f < NUM_FORCE_POWERS; f++) {
		if (scl->forcePowerLevels[f] > 0) {
			char c;
			switch (f) {
			case FP_HEAL:			c = 'h';	break;
			case FP_LEVITATION:		c = 'j';	break;
			case FP_SPEED:			c = 's';	break;
			case FP_PUSH:			c = 'x';	break;
			case FP_PULL:			c = 'u';	break;
			case FP_TELEPATHY:		c = 'm';	break;
			case FP_GRIP:			c = 'g';	break;
			case FP_LIGHTNING:		c = 'l';	break;
			case FP_RAGE:			c = 'r';	break;
			case FP_PROTECT:		c = 'p';	break;
			case FP_ABSORB:			c = 'a';	break;
			case FP_TEAM_HEAL:		c = 't';	break;
			case FP_TEAM_FORCE:		c = 'e';	break;
			case FP_DRAIN:			c = 'd';	break;
			case FP_SEE:			c = 'v';	break;
			case FP_SABER_OFFENSE:	c = 'o';	break;
			case FP_SABER_DEFENSE:	c = 'f';	break;
			case FP_SABERTHROW:		c = 'w';	break;
			}
			Q_strcat(forceBuf, sizeof(forceBuf), va("%c%d", c, Com_Clampi(1, 3, scl->forcePowerLevels[f])));
		}
	}
	return forceBuf;
}

static char *SiegeClassInfoClassShaderString(siegeClass_t *scl) {
	if (!scl || !scl->classShaderBuf[0])
		return NULL;

	// save some bytes by just sending an abbreviated form
	if (!Q_stricmp(scl->classShaderBuf, "gfx/mp/c_icon_infantry"))
		return "a";
	else if (!Q_stricmp(scl->classShaderBuf, "gfx/mp/c_icon_heavy_weapons"))
		return "h";
	else if (!Q_stricmp(scl->classShaderBuf, "gfx/mp/c_icon_demolitionist"))
		return "d";
	else if (!Q_stricmp(scl->classShaderBuf, "gfx/mp/c_icon_support"))
		return "t";
	else if (!Q_stricmp(scl->classShaderBuf, "gfx/mp/c_icon_vanguard"))
		return "s";
	else if (!Q_stricmp(scl->classShaderBuf, "gfx/mp/c_icon_jedi_general"))
		return "j";
	else
		return scl->classShaderBuf;
}

static char *FilterChars(char *s) {
	if (!VALIDSTRING(s))
		return s;

	for (char *p = s; *p; p++) { // filter chars that might break parsing
		if (*p == '"')
			*p = '\'';
		else if (*p == '=')
			*p = ':';
	}

	return s;
}

#define AddIntegerToClassInfo(key, value)						\
	do {														\
		if (value) {											\
			Q_strcat(s, size, va(" \"%s=%d\"", key, value));	\
		}														\
	} while (0)

#define AddFloatToClassInfo(key, value)							\
	do {														\
		if (value) {											\
			Q_strcat(s, size, va(" \"%s=%.3f\"", key, value));	\
		}														\
	} while (0)

#define AddStringToClassInfo(key, string)						\
	do {														\
		if (VALIDSTRING(string)) {								\
			Q_strcat(s, size, va(" \"%s=%s\"", key, string));	\
		}														\
	} while (0)

void Cmd_Sci_f(gentity_t *ent) {
	if (g_gametype.integer != GT_SIEGE)
		return;

	char arg[MAX_STRING_CHARS];
	trap_Argv(1, arg, sizeof(arg));
	team_t desiredTeam = atoi(arg);
	if (desiredTeam != TEAM_RED && desiredTeam != TEAM_BLUE)
		return;

	static qboolean initialized = qfalse;
	static char string[2][6][MAX_STRING_CHARS] = { 0 };

	if (!initialized) {
		initialized = qtrue;
		for (team_t team = TEAM_RED; team <= TEAM_BLUE; team++) {
			for (int i = 0; i < 6; i++) {
				siegeClass_t *scl = BG_SiegeGetClass(team, i + 1);
				if (!scl)
					continue;

				char *s = string[team - 1][i];
				size_t size = sizeof(string[0][0]);
				Com_sprintf(s, size, "kls -1 -1 sci %d %d", team, i);

				AddStringToClassInfo("nm", FilterChars(scl->name));
				AddIntegerToClassInfo("ss", scl->saberStance);
				AddIntegerToClassInfo("wp", scl->weapons);
				AddStringToClassInfo("s1", FilterChars(scl->saber1));
				AddStringToClassInfo("s2", FilterChars(scl->saber2));
				AddIntegerToClassInfo("in", scl->invenItems);
				char *forcePowersString = SiegeClassInfoForcePowersString(scl);
				AddStringToClassInfo("fp", forcePowersString);
				AddIntegerToClassInfo("cf", scl->classflags);
				AddIntegerToClassInfo("mh", scl->maxhealth);
				AddIntegerToClassInfo("sh", scl->starthealth);
				AddIntegerToClassInfo("ab", scl->ammoblaster);
				AddIntegerToClassInfo("ap", scl->ammopowercell);
				AddIntegerToClassInfo("am", scl->ammometallicbolts);
				AddIntegerToClassInfo("ar", scl->ammorockets);
				AddIntegerToClassInfo("ah", scl->ammothermals);
				AddIntegerToClassInfo("at", scl->ammotripmines);
				AddIntegerToClassInfo("ad", scl->ammodetpacks);
				AddIntegerToClassInfo("ma", scl->maxarmor);
				AddIntegerToClassInfo("sa", scl->startarmor);
				AddFloatToClassInfo("sp", scl->speed);
				AddStringToClassInfo("ps", FilterChars(scl->uiPortrait));
				char *classShaderString = SiegeClassInfoClassShaderString(scl);
				AddStringToClassInfo("cs", FilterChars(classShaderString));
				AddStringToClassInfo("de", FilterChars(scl->description));

				if (strlen(s) >= (MAX_STRING_CHARS - 1) && *(s + (MAX_STRING_CHARS - 1)) != '\"') {
					G_LogPrintf("SiegeClassInfo warning: class %d on team %d (%s)'s info is too long!", i, team, scl->name);
					*s = '\0';
				}
			}
		}
	}

	for (int i = 0; i < 6; i++) {
		if (string[desiredTeam - 1][i][0]) {
#ifdef _DEBUG
			Com_Printf("%s\n", string[desiredTeam - 1][i]);
#endif
			trap_SendServerCommand(ent - g_entities, string[desiredTeam - 1][i]);
		}
	}
}

static void SendNewmodClassChange(int clientNum, qboolean switchedToFullClass, int limit, qboolean startedAsSpec, int current) {
	assert(g_gametype.integer == GT_SIEGE);
	assert(clientNum >= 0 && clientNum < MAX_CLIENTS);

	static int lastClass[MAX_CLIENTS] = { -1 }, lastTeam[MAX_CLIENTS] = { -1 };

	gclient_t *changedCl = &level.clients[clientNum];

	char *changedToStr;
	if (changedCl->siegeClass == -1) {
		changedToStr = "?";
	}
	else {
		switch (bgSiegeClasses[changedCl->siegeClass].playerClass) {
		case SPC_INFANTRY:		changedToStr = "Assault";		break;
		case SPC_VANGUARD:		changedToStr = "Scout";			break;
		case SPC_SUPPORT:		changedToStr = "Tech";			break;
		case SPC_JEDI:			changedToStr = "Jedi";			break;
		case SPC_DEMOLITIONIST:	changedToStr = "Demolitions";	break;
		case SPC_HEAVY_WEAPONS:	changedToStr = "Heavy Weapons";	break;
		default:				changedToStr = "?";				break;
		}
	}

	team_t team;
	if (changedCl->sess.sessionTeam == TEAM_RED || changedCl->sess.sessionTeam == TEAM_BLUE) {
		team = changedCl->sess.sessionTeam;
	}
	else if (changedCl->sess.siegeDesiredTeam == TEAM_RED || changedCl->sess.siegeDesiredTeam == TEAM_BLUE) {
		team = changedCl->sess.siegeDesiredTeam;
	}
	else {
		// debug print to help me find weird edge cases
		G_LogPrintf("SendNewmodClassChange: failed to determine team for client %d! Misc. data: %d %d %d %d %d %d\n", clientNum, switchedToFullClass, limit, startedAsSpec, current, changedCl->sess.sessionTeam, changedCl->sess.siegeDesiredTeam);
		assert(qfalse);
		return; // ???
	}

	// if already ingame, don't broadcast the exact same thing consecutively (e.g. someone pressing "red jedi" bind while already a red jedi)
	if (!startedAsSpec && changedCl->siegeClass == lastClass[clientNum] && team == lastTeam[clientNum])
		return;

	lastClass[clientNum] = changedCl->siegeClass;
	lastTeam[clientNum] = team;

	// example: client 3 (on red team) switched to hw but it's full
	// sclc 3 1 "Heavy Weapons" "lim=1" "cur=2"
	char buf[MAX_STRING_CHARS];
	Q_strncpyz(buf, va("kls -1 -1 sclc %d %d \"%s\"%s", clientNum, team, changedToStr, switchedToFullClass ? va(" \"lim=%d\" \"cur=%d\"", limit, current) : ""), sizeof(buf));

	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *sendCl = &level.clients[i];
		if (sendCl->pers.connected != CON_CONNECTED)
			continue;
		// if client i is the changer himself, or is a spec with no desired team,
		// skip all the other checks because he gets to see this change
		if (!(i == clientNum || (sendCl->sess.sessionTeam == TEAM_SPECTATOR && sendCl->sess.siegeDesiredTeam != TEAM_RED && sendCl->sess.siegeDesiredTeam != TEAM_BLUE))) {
			if ((team == TEAM_RED && sendCl->sess.sessionTeam == TEAM_BLUE) ||
				(team == TEAM_BLUE && sendCl->sess.sessionTeam == TEAM_RED))
				continue; // on the opposite team
			if ((team == TEAM_RED && sendCl->sess.siegeDesiredTeam == TEAM_BLUE) ||
				(team == TEAM_BLUE && sendCl->sess.siegeDesiredTeam == TEAM_RED))
				continue; // in countdown and destined for the opposite team
		}
		trap_SendServerCommand(i, buf);
#if 0
		Com_Printf("Sent to client %d: %s\n", i, buf);
#endif
	}
}
#endif

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent ) {
	char		s[MAX_TOKEN_CHARS];

	int oldTeam = ent->client->sess.sessionTeam;
	int oldDesiredTeam = ent->client->sess.siegeDesiredTeam;

	if ( trap_Argc() != 2 ) {		
		switch ( oldTeam ) {
		case TEAM_BLUE:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")) );
			break;
		case TEAM_RED:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")) );
			break;
		case TEAM_FREE:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")) );
			break;
		case TEAM_SPECTATOR:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")) );
			break;
		}
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if (gEscaping)
	{
		return;
	}

	// if they are playing a tournement game, count as a loss
	if ( g_gametype.integer == GT_DUEL
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {//in a tournament game
		//disallow changing teams
		trap_SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Duel\n\"" );
		return;
		//FIXME: why should this be a loss???
	}

	if (g_gametype.integer == GT_POWERDUEL)
	{ //don't let clients change teams manually at all in powerduel, it will be taken care of through automated stuff
		trap_SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Power Duel\n\"" );
		return;
	}

	trap_Argv( 1, s, sizeof( s ) );

	
	SetTeam( ent, s , qfalse);

	if (oldTeam != ent->client->sess.sessionTeam) { // *CHANGE 16* team change actually happened
		ent->client->switchTeamTime = level.time + 5000;
	}

#ifdef NEWMOD_SUPPORT
	// just go ahead and broadcast for a team change with /team command, regardless of full class
	if (g_gametype.integer == GT_SIEGE &&
		(ent->client->sess.siegeDesiredTeam == TEAM_RED || ent->client->sess.siegeDesiredTeam == TEAM_BLUE) &&
		(oldTeam != ent->client->sess.sessionTeam || oldDesiredTeam != ent->client->sess.siegeDesiredTeam)) {
		SendNewmodClassChange(ent - g_entities, qfalse, 0, qtrue, 0);
	}
#endif
}

/*
=================
Cmd_DuelTeam_f
=================
*/
void Cmd_DuelTeam_f(gentity_t *ent)
{
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	if (g_gametype.integer != GT_POWERDUEL)
	{ //don't bother doing anything if this is not power duel
		return;
	}

	if ( trap_Argc() != 2 )
	{ //No arg so tell what team we're currently on.
		oldTeam = ent->client->sess.duelTeam;
		switch ( oldTeam )
		{
		case DUELTEAM_FREE:
			trap_SendServerCommand( ent-g_entities, va("print \"None\n\"") );
			break;
		case DUELTEAM_LONE:
			trap_SendServerCommand( ent-g_entities, va("print \"Single\n\"") );
			break;
		case DUELTEAM_DOUBLE:
			trap_SendServerCommand( ent-g_entities, va("print \"Double\n\"") );
			break;
		default:
			break;
		}
		return;
	}

	if ( ent->client->switchDuelTeamTime > level.time )
	{ //debounce for changing
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	trap_Argv( 1, s, sizeof( s ) );

	oldTeam = ent->client->sess.duelTeam;

	if (!Q_stricmp(s, "free"))
	{
		ent->client->sess.duelTeam = DUELTEAM_FREE;
	}
	else if (!Q_stricmp(s, "single"))
	{
		ent->client->sess.duelTeam = DUELTEAM_LONE;
	}
	else if (!Q_stricmp(s, "double"))
	{
		ent->client->sess.duelTeam = DUELTEAM_DOUBLE;
	}
	else
	{
		trap_SendServerCommand( ent-g_entities, va("print \"'%s' not a valid duel team.\n\"", s) );
	}

	if (oldTeam == ent->client->sess.duelTeam)
	{ //didn't actually change, so don't care.
		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //ok..die
		int curTeam = ent->client->sess.duelTeam;
		ent->client->sess.duelTeam = oldTeam;
		G_Damage(ent, ent, ent, NULL, ent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		ent->client->sess.duelTeam = curTeam;
	}
	//reset wins and losses
	ent->client->sess.wins = 0;
	ent->client->sess.losses = 0;

	//get and distribute relevent paramters
	ClientUserinfoChanged( ent->s.number );

	ent->client->switchDuelTeamTime = level.time + 5000;
}

int G_TeamForSiegeClass(const char *clName)
{
	int i = 0;
	int team = SIEGETEAM_TEAM1;
	siegeTeam_t *stm = BG_SiegeFindThemeForTeam(team);
	siegeClass_t *scl;

	if (!stm)
	{
		return 0;
	}

	while (team <= SIEGETEAM_TEAM2)
	{
		scl = stm->classes[i];

		if (scl && scl->name[0])
		{
			if (!Q_stricmp(clName, scl->name))
			{
				return team;
			}
		}

		i++;
		if (i >= MAX_SIEGE_CLASSES || i >= stm->numClasses)
		{
			if (team == SIEGETEAM_TEAM2)
			{
				break;
			}
			team = SIEGETEAM_TEAM2;
			stm = BG_SiegeFindThemeForTeam(team);
			i = 0;
		}
	}

	return 0;
}

// if the proposed class change would not violate any limits, returns 0
// else, returns the limit of the class
// can optionally write the current number of the class if currentOut is specified
int G_WouldExceedClassLimit(int team, int classType, qboolean hypothetical, int clientNum, int *currentOut) {
	assert(clientNum < MAX_CLIENTS);
	if (!g_classLimits.integer)
		return 0;

	// no class limits for speedruns, as long as < 3 on red team and nobody on blue team
	// teams have to be checked here because speedrun mode is still preserved during countdown even with "bad" teams, and
	// only becomes ruined at the moment the round proper starts
	// checking teams here allows warnings to be printed during the countdown
	if (g_saveCaptureRecords.integer && !level.mapCaptureRecords.speedRunModeRuined && level.isLivePug != ISLIVEPUG_YES && team == TEAM_RED) {
		int numRed = 0, numBlue = 0;
		GetPlayerCounts(IsDebugBuild, qtrue, &numRed, &numBlue, NULL, NULL);
		if (numRed < 3 && !numBlue)
			return 0;
	}

	int limit;
	if (team == TEAM_RED) {
		switch (classType) {
		case SPC_INFANTRY:		limit = oAssaultLimit.integer;	break;
		case SPC_HEAVY_WEAPONS:	limit = oHWLimit.integer;		break;
		case SPC_DEMOLITIONIST:	limit = oDemoLimit.integer;		break;
		case SPC_SUPPORT:		limit = oTechLimit.integer;		break;
		case SPC_VANGUARD:		limit = oScoutLimit.integer;	break;
		case SPC_JEDI:			limit = oJediLimit.integer;		break;
		default:				return 0;
		}
	}
	else if (team == TEAM_BLUE) {
		switch (classType) {
		case SPC_INFANTRY:		limit = dAssaultLimit.integer;	break;
		case SPC_HEAVY_WEAPONS:	limit = dHWLimit.integer;		break;
		case SPC_DEMOLITIONIST:	limit = dDemoLimit.integer;		break;
		case SPC_SUPPORT:		limit = dTechLimit.integer;		break;
		case SPC_VANGUARD:		limit = dScoutLimit.integer;	break;
		case SPC_JEDI:			limit = dJediLimit.integer;		break;
		default:				return 0;
		}
	}
	else
		return 0;

	if (limit <= 0)
		return 0;

	int current = G_SiegeClassCount(team, classType, qfalse, clientNum >= 0 ? clientNum : -1);
	if (hypothetical)
		current++;

	if (current > limit) {
		if (currentOut)
			*currentOut = current;
		return limit;
	}

	return 0;
}

void *G_SiegeClassFromName(char *s) {
	if (!VALIDSTRING(s))
		return NULL;

	static void *c;
	int i;

	for (i = 0; i < MAX_SIEGE_CLASSES; i++) {
		c = (void *)&bgSiegeClasses[i];
		if (c && !Q_stricmp(s, ((siegeClass_t *)c)->name))
			return c;
	}

	return NULL;
}

void SetSiegeClass(gentity_t *ent, char* className)
{
	qboolean doDelay = qtrue;
	qboolean startedAsSpec = qfalse;
	int team = 0;
	int preScore;
	int limit = 0;
	qboolean switchedToFullClass = qfalse;
	int current = 0;

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		startedAsSpec = qtrue;
	}

	team = G_TeamForSiegeClass(className);

	if (!team)
	{ //not a valid class name
		return;
	}

	if (level.zombies)
	{
		if (team == TEAM_BLUE && bgSiegeClasses[BG_SiegeFindClassIndexByName(className)].playerClass == SPC_JEDI)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"You cannot play as defense jedi in zombies.\n\""));
			return;
		}
		if (team == TEAM_RED && bgSiegeClasses[BG_SiegeFindClassIndexByName(className)].playerClass != SPC_JEDI)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"You cannot play as offense gunners in zombies.\n\""));
			return;
		}
	}
	
	if (g_classLimits.integer) {
		siegeClass_t *scl = (siegeClass_t *)G_SiegeClassFromName(className);
		limit = scl ? G_WouldExceedClassLimit(team, scl->playerClass, qtrue, ent - g_entities, &current) : 0;
		if (scl && limit) {
			if (level.inSiegeCountdown || ent->client->sess.sessionTeam == team) { // countdown, or ingame and already on the desired team
				trap_SendServerCommand(ent - g_entities, va("print \"The class you selected is full. You will be switched if there %s more than %i.\n\"", limit == 1 ? "is" : "are", limit));
				level.tryChangeClass[ent - g_entities].class = scl->playerClass;
				level.tryChangeClass[ent - g_entities].team = team;
				level.tryChangeClass[ent - g_entities].time = level.time;
				doDelay = qfalse;
				switchedToFullClass = qtrue;
			}
			else {
				trap_SendServerCommand(ent - g_entities, va("print \"The class you selected is full (limit: %i).\n\"", limit));
				return;
			}
		}
	}

	if (ent->client->sess.sessionTeam != team)
	{ //try changing it then
		g_preventTeamBegin = qtrue;
		if (team == TEAM_RED)
		{
			if (level.zombies && ent->client->sess.sessionTeam == TEAM_BLUE)
			{
				SetTeam(ent, "red", qtrue);
			}
			else
			{
				SetTeam(ent, "red", qfalse);
			}
		}
		else if (team == TEAM_BLUE)
		{
			SetTeam(ent, "blue", qfalse);
		}
		g_preventTeamBegin = qfalse;

		if (ent->client->sess.sessionTeam != team)
		{ //failed, oh well
			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR ||
				ent->client->sess.siegeDesiredTeam != team)
			{
				//trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSTEAM")));
				return;
			}
		}
	}

	//preserve 'is score
	preScore = ent->client->ps.persistant[PERS_SCORE];

	//Make sure the class is valid for the team
	BG_SiegeCheckClassLegality(team, className);

	//Set the session data
	strcpy(ent->client->sess.siegeClass, className);

	// get and distribute relevent paramters
	ClientUserinfoChanged(ent->s.number);

	if (ent->client->tempSpectate < level.time)
	{
		// Kill him (makes sure he loses flags, etc)
		if (ent->health > 0 && !startedAsSpec)
		{
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
			player_die(ent, ent, ent, 100000, MOD_SUICIDE);
		}

		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || startedAsSpec)
		{ //respawn them instantly.
			ClientBegin(ent->s.number, qfalse);
		}
	}

#ifdef NEWMOD_SUPPORT
	SendNewmodClassChange(ent->s.number, switchedToFullClass, limit, startedAsSpec, limit ? current : 0);
#endif

	//set it back after we do all the stuff
	ent->client->ps.persistant[PERS_SCORE] = preScore;

	if (doDelay) {
		if (g_delayClassUpdate.integer)
			ent->client->switchClassTime = level.time + 1000;
		else
			ent->client->switchClassTime = level.time + 5000;
	}
}

/*
=================
Cmd_SiegeClass_f
=================
*/
void Cmd_SiegeClass_f(gentity_t *ent)
{
	char className[64];
	int	timeRemaining = 0;

	if (g_gametype.integer != GT_SIEGE)
	{ //classes are only valid for this gametype
		return;
	}

	if (!ent->client)
	{
		return;
	}

	if (trap_Argc() < 1)
	{
		return;
	}

	if (ent->client->switchClassTime > level.time && level.inSiegeCountdown != qtrue)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"Please wait %i seconds before switching classes.\n\"", (ent->client->switchClassTime - level.time + 500) / 1000));
		return;
	}

	trap_Argv(1, className, sizeof(className));

	if (ent->forcedClassTime > level.time)
	{
		if (bgSiegeClasses[BG_SiegeFindClassIndexByName(className)].playerClass != ent->funnyClassNumber)
		{
			timeRemaining = ((ent->forcedClassTime - level.time + 500) / 1000);
			trap_SendServerCommand(ent - g_entities, va("print \"You are currently being forced to a class. You will be able to change classes in %i seconds.\n\"", timeRemaining));
			return;
		}
	}

	if (g_gametype.integer == GT_SIEGE && g_antiSelfMax.integer && g_siegeRespawn.integer >= 10 && (level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
		ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		int timeSinceRespawn = (level.time + (g_siegeRespawn.integer * 1000)) - level.siegeRespawnCheck;
		if (timeSinceRespawn < 3000 && !HasDetpackInWorld(ent)) { // players cannot sk within 3 seconds AFTER the spawn wave, unless they have a detpack in the world
			return;
		}
		if ((ent->client->sess.skillBoost || g_antiSelfMax.integer) && ent->health > 0 && !(ent->client->tempSpectate >= level.time)) {
			int oneSecBeforeRespawn = (g_siegeRespawn.integer - 1) * 1000;
			if (timeSinceRespawn >= oneSecBeforeRespawn && !HasDetpackInWorld(ent)) { // cannot sk within 1 second BEFORE the spawn wave, too
				SendAntiSelfMaxMessage(ent - g_entities, qtrue);
				return;
			}
		}
	}

	// quick and dirty hack to make the join menu work for altered classes on base maps
	// basically, the base jka join menu will send /siegeclass "Imperial Snowtrooper" if you try to change to offense HW on hoth,
	// regardless of what g_redTeam is currently set to. ordinarily this will do nothing if the offense HW class isn't actually Imperial Snowtrooper.
	// this hack effectively turns that command into /class h

	if (((g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none")) || (g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none"))) && g_joinMenuHack.integer) {
		stupidSiegeClassNum_t classNum = 0;
		team_t teamNum = 0;
		if (!Q_stricmp(level.mapname, "mp/siege_hoth")) {
			if (!Q_stricmp(className, "Imperial Snowtrooper")) {
				classNum = SSCN_ASSAULT;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Rocket Trooper")) {
				classNum = SSCN_HW;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Imperial Demolitionist")) {
				classNum = SSCN_DEMO;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Imperial Sniper")) {
				classNum = SSCN_SCOUT;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Imperial Tech")) {
				classNum = SSCN_TECH;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Dark Jedi Invader")) {
				classNum = SSCN_JEDI;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Rebel Infantry")) {
				classNum = SSCN_ASSAULT;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Wookie")) {
				classNum = SSCN_HW;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Rebel Demolitionist")) {
				classNum = SSCN_DEMO;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Rebel Sniper")) {
				classNum = SSCN_SCOUT;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Rebel Tech")) {
				classNum = SSCN_TECH;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Jedi Guardian")) {
				classNum = SSCN_JEDI;
				teamNum = TEAM_BLUE;
			}
		}
		else if (!Q_stricmp(level.mapname, "mp/siege_desert")) {
			if (!Q_stricmp(className, "Bounty Hunter")) {
				classNum = SSCN_ASSAULT;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Merc Assault Specialist")) {
				classNum = SSCN_HW;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Merc Demolitionist")) {
				classNum = SSCN_DEMO;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Merc Sniper")) {
				classNum = SSCN_SCOUT;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Smuggler")) {
				classNum = SSCN_TECH;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Dark Side Marauder")) {
				classNum = SSCN_JEDI;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Rebel Assault Specialist")) {
				classNum = SSCN_ASSAULT;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Wookie")) {
				classNum = SSCN_HW;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Rebel Demolitionist")) {
				classNum = SSCN_DEMO;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Rebel Sniper")) {
				classNum = SSCN_SCOUT;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Rebel Tech")) {
				classNum = SSCN_TECH;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Jedi Duelist")) {
				classNum = SSCN_JEDI;
				teamNum = TEAM_RED;
			}
		}
		else if (!Q_stricmp(level.mapname, "mp/siege_korriban")) {
			if (!Q_stricmp(className, "Dark Side Mauler")) {
				classNum = SSCN_ASSAULT;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Dark Jedi Destroyer")) {
				classNum = SSCN_HW;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Dark Jedi Demolitionist")) {
				classNum = SSCN_DEMO;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Dark Jedi Interceptor")) {
				classNum = SSCN_SCOUT;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Dark Jedi Tech")) {
				classNum = SSCN_TECH;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Dark Jedi Duelist")) {
				classNum = SSCN_JEDI;
				teamNum = TEAM_BLUE;
			}
			else if (!Q_stricmp(className, "Jedi Warrior")) {
				classNum = SSCN_ASSAULT;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Jedi Knight")) {
				classNum = SSCN_HW;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Jedi Demolitionist")) {
				classNum = SSCN_DEMO;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Jedi Scout")) {
				classNum = SSCN_SCOUT;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Jedi Healer")) {
				classNum = SSCN_TECH;
				teamNum = TEAM_RED;
			}
			else if (!Q_stricmp(className, "Jedi Lightsaber Master")) {
				classNum = SSCN_JEDI;
				teamNum = TEAM_RED;
			}
		}

		if (classNum && teamNum) {
			siegeClass_t *siegeClass = BG_SiegeGetClass(teamNum, classNum);

			if (siegeClass) {
				SetSiegeClass(ent, siegeClass->name);
				return;
			}
		}
	}

	SetSiegeClass(ent, className);
}

void Cmd_Join_f(gentity_t *ent)
{
	char className[8];
	char input[8];
	char desiredTeam[8];
	int desiredTeamNumber;
	int classNumber = 0;
	siegeClass_t* siegeClass = 0;
	int	timeRemaining = 0;

	if (!ent || !ent->client)
	{
		return;
	}

	if (trap_Argc() != 2)
	{
		trap_SendServerCommand(ent - g_entities, "print \"Usage: join <team letter><first letter of class name> (no spaces)  example: '^5join rj^7' for red jedi)\n\"");
		return;
	}

	trap_Argv(1, input, sizeof(input));
	if (!input[0] || (input[1] && input[2]))
	{
		trap_SendServerCommand(ent - g_entities, "print \"Usage: join <team letter><first letter of class name> (no spaces)  example: '^5join rj^7' for red jedi)\n\"");
		return;
	}

	if (!input[1]) {
		switch (tolower(input[0])) {
		case 'r': case 'o': case 'b': case 'd': case 's':
			Cmd_Team_f(ent);
			break;
		default:
			trap_SendServerCommand(ent - g_entities, "print \"Usage: join <team letter><first letter of class name> (no spaces)  example: '^5join rj^7' for red jedi)\n\"");
			break;
		}
		return;
	}

	if (g_gametype.integer != GT_SIEGE)
		return;

	desiredTeam[0] = input[0];
	className[0] = input[1];

	if (desiredTeam[0] == 'r' || desiredTeam[0] == 'R' || desiredTeam[0] == 'o' || desiredTeam[0] == 'O')
	{
		desiredTeamNumber = 1;
	}
	else if(desiredTeam[0] == 'b' || desiredTeam[0] == 'B' || desiredTeam[0] == 'd' || desiredTeam[0] == 'D')
	{
		desiredTeamNumber = 2;
	}
	else
	{
		trap_SendServerCommand(ent - g_entities, "print \"Usage: join <team letter><first letter of class name> (no spaces)  example: '^5join rj^7' for red jedi)\n\"");
		return;
	}

	if (ent->client->switchClassTime > level.time && level.inSiegeCountdown != qtrue)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"Please wait %i seconds before switching classes.\n\"", (ent->client->switchClassTime - level.time + 500) / 1000));
		return;
	}

	if (ent->forcedClassTime > level.time)
	{
		if (bgSiegeClasses[BG_SiegeFindClassIndexByName(className)].playerClass != ent->funnyClassNumber)
		{
			timeRemaining = ((ent->forcedClassTime - level.time + 500) / 1000);
			trap_SendServerCommand(ent - g_entities, va("print \"You are currently being forced to a class. You will be able to change classes in %i seconds.\n\"", timeRemaining));
			return;
		}
	}

	if (g_antiSelfMax.integer && g_siegeRespawn.integer >= 10 && (level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
		ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		int timeSinceRespawn = (level.time + (g_siegeRespawn.integer * 1000)) - level.siegeRespawnCheck;
		if (timeSinceRespawn < 3000 && !HasDetpackInWorld(ent)) { // players cannot sk within 3 seconds AFTER the spawn wave, unless they have a detpack in the world
			return;
		}
		if ((ent->client->sess.skillBoost || g_antiSelfMax.integer) && ent->health > 0 && !(ent->client->tempSpectate >= level.time)) {
			int oneSecBeforeRespawn = (g_siegeRespawn.integer - 1) * 1000;
			if (timeSinceRespawn >= oneSecBeforeRespawn && !HasDetpackInWorld(ent)) { // cannot sk within 1 second BEFORE the spawn wave, too
				SendAntiSelfMaxMessage(ent - g_entities, qtrue);
				return;
			}
		}
	}

	if ((className[0] >= '0') && (className[0] <= '9'))
	{
		classNumber = atoi(className);
		trap_SendServerCommand(ent - g_entities, va("print \"Changing to %sclass %i^7\n\"", desiredTeamNumber == 1 ? "^1" : "^4", classNumber));
	}
	else
	{
		// funny way for pro siegers
		switch (tolower(className[0]))
		{
		case 'a':
			classNumber = 1;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to %sAssault^7\n\"", desiredTeamNumber == 1 ? "^1" : "^4"));
			break;
		case 'h':
			classNumber = 2;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to %sHeavy Weapons^7\n\"", desiredTeamNumber == 1 ? "^1" : "^4"));
			break;
		case 'd':
			classNumber = 3;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to %sDemolitions^7\n\"", desiredTeamNumber == 1 ? "^1" : "^4"));
			break;
		case 's':
			classNumber = 4;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to %sScout^7\n\"", desiredTeamNumber == 1 ? "^1" : "^4"));
			break;
		case 't':
			classNumber = 5;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to %sTech^7\n\"", desiredTeamNumber == 1 ? "^1" : "^4"));
			break;
		case 'j':
			classNumber = 6;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to %sJedi^7\n\"", desiredTeamNumber == 1 ? "^1" : "^4"));
			break;
		default:
			trap_SendServerCommand(ent - g_entities, "print \"Usage: join <team letter><first letter of class name> (no spaces)  example: '^5join rj^7' for red jedi)\n\"");
			return;
		}

	}

	siegeClass = BG_SiegeGetClass(desiredTeamNumber, classNumber);

	if (!siegeClass)
	{
		trap_SendServerCommand(ent - g_entities, "print \"Usage: join <team letter><first letter of class name> (no spaces)  example: '^5join rj^7' for red jedi)\n\"");
		return;
	}

	SetSiegeClass(ent, siegeClass->name);
}

void Cmd_Class_f(gentity_t *ent)
{
	char className[16];
	stupidSiegeClassNum_t classNumber = 0;
	siegeClass_t* siegeClass = 0;
	int	timeRemaining = 0;

	if (!ent || !ent->client || g_gametype.integer != GT_SIEGE)
	{
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		if (!(level.inSiegeCountdown && ent->client->sess.siegeDesiredTeam && (ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1 || ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2)))
		{
			return;
		}
	}

	if (trap_Argc() < 1)
	{
		trap_SendServerCommand(ent - g_entities, "print \"Usage: class <number> or class <first letter of class name> (e.g. '^5class a^7' for assault)\n\"");
		return;
	}

	if (ent->client->switchClassTime > level.time && level.inSiegeCountdown != qtrue)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"Please wait %i seconds before switching classes.\n\"", (ent->client->switchClassTime - level.time + 500) / 1000));
		return;
	}

	trap_Argv(1, className, sizeof(className));

	if (ent->forcedClassTime > level.time)
	{
		if (bgSiegeClasses[BG_SiegeFindClassIndexByName(className)].playerClass != ent->funnyClassNumber)
		{
			timeRemaining = ((ent->forcedClassTime - level.time + 500) / 1000);
			trap_SendServerCommand(ent - g_entities, va("print \"You are currently being forced to a class. You will be able to change classes in %i seconds.\n\"", timeRemaining));
			return;
		}
	}

	if (g_antiSelfMax.integer && g_siegeRespawn.integer >= 10 && (level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2)) {
		int timeSinceRespawn = (level.time + (g_siegeRespawn.integer * 1000)) - level.siegeRespawnCheck;
		if (timeSinceRespawn < 3000 && !HasDetpackInWorld(ent)) { // players cannot sk within 3 seconds AFTER the spawn wave, unless they have a detpack in the world
			return;
		}
		if ((ent->client->sess.skillBoost || g_antiSelfMax.integer) && ent->health > 0 && !(ent->client->tempSpectate >= level.time)) {
			int oneSecBeforeRespawn = (g_siegeRespawn.integer - 1) * 1000;
			if (timeSinceRespawn >= oneSecBeforeRespawn && !HasDetpackInWorld(ent)) { // cannot sk within 1 second BEFORE the spawn wave, too
				SendAntiSelfMaxMessage(ent - g_entities, qtrue);
				return;
			}
		}
	}

	if ((className[0] >= '0') && (className[0] <= '9'))
	{
		classNumber = atoi(className);
		trap_SendServerCommand(ent - g_entities, va("print \"Changing to class %i\n\"", classNumber));
	}
	else
	{
		// funny way for pro siegers
		switch (tolower(className[0]))
		{
		case 'a':
			classNumber = SSCN_ASSAULT;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to Assault\n\""));
			break;
		case 'h':
			classNumber = SSCN_HW;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to Heavy Weapons\n\""));
			break;
		case 'd':
			classNumber = SSCN_DEMO;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to Demolitions\n\""));
			break;
		case 's':
			classNumber = SSCN_SCOUT;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to Scout\n\""));
			break;
		case 't':
			classNumber = SSCN_TECH;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to Tech\n\""));
			break;
		case 'j':
			classNumber = SSCN_JEDI;
			trap_SendServerCommand(ent - g_entities, va("print \"Changing to Jedi\n\""));
			break;
		default:
			trap_SendServerCommand(ent - g_entities, "print \"Usage: class <number> or class <first letter of class name> (e.g. '^5class a^7' for assault)\n\"");
			return;
		}

	}

	if (level.inSiegeCountdown && ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.siegeDesiredTeam && (ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1 || ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2))
	{
		siegeClass = BG_SiegeGetClass(ent->client->sess.siegeDesiredTeam, classNumber);
	}
	else
	{
		siegeClass = BG_SiegeGetClass(ent->client->sess.sessionTeam, classNumber);
	}
	
	if (!siegeClass)
	{
		trap_SendServerCommand(ent - g_entities, "print \"Usage: class <number> or class <first letter of class name> (e.g. '^5class a^7' for assault)\n\"");
		return;
	}

	SetSiegeClass(ent, siegeClass->name);
}

/*
=================
Cmd_ForceChanged_f
=================
*/
void Cmd_ForceChanged_f( gentity_t *ent )
{
	char fpChStr[1024];
	const char *buf;
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //if it's a spec, just make the changes now
		//No longer print it, as the UI calls this a lot.
		WP_InitForcePowers( ent );
		goto argCheck;
	}

	buf = G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED");

	strcpy(fpChStr, buf);

	trap_SendServerCommand( ent-g_entities, va("print \"%s%s\n\n\"", S_COLOR_GREEN, fpChStr) );

	ent->client->ps.fd.forceDoInit = 1;
argCheck:
	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
	{ //If this is duel, don't even bother changing team in relation to this.
		return;
	}

	if (trap_Argc() > 1)
	{
		char	arg[MAX_TOKEN_CHARS];

		trap_Argv( 1, arg, sizeof( arg ) );

		if (/*arg &&*/ arg[0])
		{ //if there's an arg, assume it's a combo team command from the UI.
			Cmd_Team_f(ent);
		}
	}
}

extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );
qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride)
{
	char truncSaberName[64];
	int i = 0;

	if (!siegeOverride &&
		g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		(
		 bgSiegeClasses[ent->client->siegeClass].saberStance ||
		 bgSiegeClasses[ent->client->siegeClass].saber1[0] ||
		 bgSiegeClasses[ent->client->siegeClass].saber2[0]
		))
	{ //don't let it be changed if the siege class has forced any saber-related things
        return qfalse;
	}

	while (saberName[i] && i < 64-1)
	{
        truncSaberName[i] = saberName[i];
		i++;
	}
	truncSaberName[i] = 0;

	if ( saberNum == 0 && (Q_stricmp( "none", truncSaberName ) == 0 || Q_stricmp( "remove", truncSaberName ) == 0) )
	{ //can't remove saber 0 like this
        strcpy(truncSaberName, "Kyle");
	}

	//Set the saber with the arg given. If the arg is
	//not a valid sabername defaults will be used.
	WP_SetSaber(ent->s.number, ent->client->saber, saberNum, truncSaberName);

	if (!ent->client->saber[0].model[0])
	{
		assert(0); //should never happen!
		strcpy(ent->client->sess.saberType, "none");
	}
	else
	{
		strcpy(ent->client->sess.saberType, ent->client->saber[0].name);
	}

	if (!ent->client->saber[1].model[0])
	{
		strcpy(ent->client->sess.saber2Type, "none");
	}
	else
	{
		strcpy(ent->client->sess.saber2Type, ent->client->saber[1].name);
	}

    if ( g_gametype.integer != GT_SIEGE )
    {
        if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
        {
            WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
            ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
        }
    }

	return qtrue;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	//if either g_followSpectator is turned off or targeted spectator is already following someoneelse
	if  (/*(!g_followSpectator.integer || level.clients[ i ].sess.spectatorState == SPECTATOR_FOLLOW)
		&&*/ level.clients[ i ].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		//WTF???
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" , qfalse);
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {\
		//WTF???
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" , qfalse);
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	if ( dir != 1 && dir != -1 ) {
		G_Error( "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	clientnum = ent->client->sess.spectatorClient;
	if (clientnum < 0) clientnum += level.maxclients;
	original = clientnum;
	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if (/*(!g_followSpectator.integer || level.clients[ clientnum ].sess.spectatorState == SPECTATOR_FOLLOW)
			&&*/ level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		SiegeGhostUpdate(ent - g_entities, qtrue); // force a ghost update since we changed follow targets
		return;
	} while ( clientnum != original );

	// leave it where it was
}

void Cmd_FollowFlag_f( gentity_t *ent ) 
{
	int		clientnum;
	int		original;

	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" , qfalse);
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	clientnum = ent->client->sess.spectatorClient;
	if (clientnum < 0) clientnum += level.maxclients;
	original = clientnum;
	do {
		++clientnum;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if (/*(!g_followSpectator.integer || level.clients[ clientnum ].sess.spectatorState == SPECTATOR_FOLLOW)
			&&*/ level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		if (level.clients[ clientnum ].ps.powerups[PW_REDFLAG] || level.clients[ clientnum ].ps.powerups[PW_BLUEFLAG]){
			// this is good, we can use it
			ent->client->sess.spectatorClient = clientnum;
			ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
			SiegeGhostUpdate(ent - g_entities, qtrue); // force a ghost update since we changed follow targets
			return;		
		}
	} while ( clientnum != original );
}

void Cmd_FollowTarget_f(gentity_t *ent) {
	// first set them to spectator
	if (ent->client->sess.spectatorState == SPECTATOR_NOT) {
		SetTeam(ent, "spectator", qfalse);
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	// check who is eligible to be followed
	qboolean valid[MAX_CLIENTS] = { qfalse }, gotValid = qfalse;
	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (i == ent->s.number || !g_entities[i].inuse || level.clients[i].pers.connected != CON_CONNECTED || level.clients[i].sess.sessionTeam == TEAM_SPECTATOR)
			continue;
		if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW && ent->client->sess.spectatorClient == i)
			continue; // already following this guy
		if (level.clients[i].ps.stats[STAT_HEALTH] <= 0 || level.clients[i].ps.pm_type == PM_SPECTATOR || level.clients[i].tempSpectate >= level.time)
			continue; // this guy is dead
		valid[i] = qtrue;
		gotValid = qtrue;
	}
	if (!gotValid)
		return; // nobody to follow

	// check for aiming directly at someone
	trace_t tr;
	vec3_t start, end, forward;
	VectorCopy(ent->client->ps.origin, start);
	AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
	VectorMA(start, 16384, forward, end);
	start[2] += ent->client->ps.viewheight;
	trap_G2Trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
	if (tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS && valid[tr.entityNum]) {
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		ent->client->sess.spectatorClient = tr.entityNum;
		return;
	}

	// see who was closest to where we aimed
	float closestDistance = -1;
	int closestPlayer = -1;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!valid[i])
			continue;
		vec3_t difference;
		VectorSubtract(level.clients[i].ps.origin, tr.endpos, difference);
		if (difference && (closestDistance == -1 || VectorLength(difference) < closestDistance)) {
			closestDistance = VectorLength(difference);
			closestPlayer = i;
		}
	}

	if (closestDistance != -1 && closestPlayer != -1) {
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		ent->client->sess.spectatorClient = closestPlayer;
		SiegeGhostUpdate(ent - g_entities, qtrue); // force a ghost update since we changed follow targets
	}
}

/*
==================
G_Say
==================
*/
#define EC		"\x19"

static qboolean ChatLimitExceeded(gentity_t *ent, int mode) {
	if (!ent || !ent->client)
		return qfalse;

	int *sentTime, *sentCount, *limit;

	if ( mode == -1 && ent->client->sess.canJoin ) { // -1 is voice chat
		sentTime = &ent->client->pers.voiceChatSentTime;
		sentCount = &ent->client->pers.voiceChatSentCount;
		limit = &g_voiceChatLimit.integer;
	} else if (mode == SAY_TEAM && ent->client->sess.canJoin && GetRealTeam(ent->client) != TEAM_SPECTATOR) { // an in-game player using teamchat
		sentTime = &ent->client->pers.teamChatSentTime;
		sentCount = &ent->client->pers.teamChatSentCount;
		limit = &g_teamChatLimit.integer;
	} else { // using all chat OR private chat (or anything for passwordless)
		sentTime = &ent->client->pers.chatSentTime;
		sentCount = &ent->client->pers.chatSentCount;
		limit = &g_chatLimit.integer;
	}

	qboolean exceeded = qfalse;

	if ( !ent->client->sess.canJoin ) { // for passwordless specs, apply a more strict anti spam with a permanent lock, just like sv_floodprotect
		exceeded = level.time - *sentTime < 1000;
	} else if ( *limit > 0 && *sentTime && (level.time - *sentTime) < 1000 ) { // we are in tracking second for current user, check limit
		exceeded = *sentCount >= *limit;
		++*sentCount;
	} else { // this is the first and only message that has been sent in the last second
		*sentCount = 1;
	}

	// in any case, reset the timer so people have to unpress their spam binds while blocked to be unblocked
	*sentTime = level.time;

	return exceeded;
}

static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, char *locMsg )
{
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) ) {
		return;
	}
	if ((other->client->sess.ignoreFlags & (1<<(ent-g_entities)))
		&& (ent!=other)	){
		return;
	}

	// if this guy is shadow muted, don't let anyone see his messages except himself and other shadow muted clients
	if (ent->client->sess.shadowMuted && ent != other && !other->client->sess.shadowMuted) {
		return;
	}

	if (((ent->client->sess.sessionTeam == TEAM_SPECTATOR && (!level.inSiegeCountdown || (ent->client->sess.siegeDesiredTeam != SIEGETEAM_TEAM1 && ent->client->sess.siegeDesiredTeam != SIEGETEAM_TEAM2)) || level.intermissiontime) && g_improvedTeamchat.integer) || level.zombies && ent->client->sess.sessionTeam == TEAM_BLUE)
	{
		//a spectator during a live game, or a spectator without a desired team during the countdown
		//or, it's intermission (for anyone, regardless of team) or zombies mode on blue team
		//remove location from message (useless for spectator teamchat)
		trap_SendServerCommand(other - g_entities, va("%s \"%s%c%c%s\" \"%i\"",
			mode == SAY_TEAM ? "tchat" : "chat",
			name, Q_COLOR_ESCAPE, color, message, ent - g_entities));
	}
	else
	{
		if (locMsg && !(g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY)) // duo: never send location in teamchat for ctf
		{
			trap_SendServerCommand(other - g_entities, va("%s \"%s\" \"%s\" \"%c\" \"%s\" \"%i\"",
				mode == SAY_TEAM ? "ltchat" : "lchat",
				name, locMsg, color, message, ent - g_entities));
		}
		else
		{
			trap_SendServerCommand(other - g_entities, va("%s \"%s%c%c%s\" \"%i\"",
				mode == SAY_TEAM ? "tchat" : "chat",
				name, Q_COLOR_ESCAPE, color, message, ent - g_entities));
		}
	}
}

char* NM_SerializeUIntToColor( const unsigned int n ) {
	static char result[32] = { 0 };
	char buf[32] = { 0 };
	int i;

	Com_sprintf( buf, sizeof( buf ), "%o", n );
	result[0] = '\0';

	for ( i = 0; buf[i] != '\0'; ++i ) {
		Q_strcat( result, sizeof( result ), va( "%c%c", Q_COLOR_ESCAPE, buf[i] ) );
	}

	return &result[0];
}

static void WriteTextForToken( gentity_t *ent, const char token, char *buffer, size_t bufferSize ) {
	buffer[0] = '\0';

	if ( !ent || !ent->client ) {
		return;
	}

	gclient_t *cl = ent->client;
	int value;
	char *s;

	switch ( token ) {
	case 'h': case 'H':
		if (g_gametype.integer == GT_SIEGE && cl->tempSpectate > level.time)
			value = 0;
		else
			value = Com_Clampi(0, 999, cl->ps.stats[STAT_HEALTH]);
		Com_sprintf( buffer, bufferSize, "%d", value );
		break;
	case 'a': case 'A':
		if (g_gametype.integer == GT_SIEGE && cl->tempSpectate > level.time)
			value = 0;
		else
			value = Com_Clampi(0, 999, cl->ps.stats[STAT_ARMOR]);
		Com_sprintf( buffer, bufferSize, "%d", value );
		break;
	case 'f': case 'F':
		if (!cl->ps.fd.forcePowersKnown || (g_gametype.integer == GT_SIEGE && cl->tempSpectate > level.time))
			value = 0;
		else
			value = Com_Clampi(0, 999, cl->ps.fd.forcePower);
		Com_sprintf( buffer, bufferSize, "%d", value );
		break;
	case 'm': case 'M':
		if (!weaponData[cl->ps.weapon].energyPerShot && !weaponData[cl->ps.weapon].energyPerShot || cl->ps.stats[STAT_HEALTH] <= 0 || (g_gametype.integer == GT_SIEGE && cl->tempSpectate > level.time))
			value = 0;
		else
			value = Com_Clampi(0, 999, cl->ps.ammo[weaponData[cl->ps.weapon].ammoIndex]);
		Com_sprintf( buffer, bufferSize, "%d", value );
		break;
	case 'l': case 'L':
		Team_GetLocation( ent, buffer, bufferSize );
		break;
	case 'w': case 'W':
		switch (cl->ps.weapon) {
		case WP_STUN_BATON:			s = "Stun Baton";			break;
		case WP_MELEE:				s = "Melee";				break;
		case WP_SABER:				s = "Saber";				break;
		case WP_BRYAR_PISTOL:	case WP_BRYAR_OLD:				s = "Pistol";	break;
		case WP_BLASTER:			s = "Blaster";				break;
		case WP_DISRUPTOR:			s = "Disruptor";			break;
		case WP_BOWCASTER:			s = "Bowcaster";			break;
		case WP_REPEATER:			s = "Repeater";				break;
		case WP_DEMP2:				s = "Demp";					break;
		case WP_FLECHETTE:			s = "Golan";				break;
		case WP_ROCKET_LAUNCHER:	s = "Rockets";	break;
		case WP_THERMAL:			s = "Thermals";				break;
		case WP_TRIP_MINE:			s = "Mines";				break;
		case WP_DET_PACK:			s = "Detpacks";				break;
		case WP_CONCUSSION:			s = "Concussion";			break;
		case WP_EMPLACED_GUN:		s = "Emplaced";				break;
		default:					s = "Jesus";				break;
		}
		Com_sprintf(buffer, bufferSize, s);
		break;
	default: return;
	}
}

#define TEAM_CHAT_TOKEN_CHAR '$'

static void TokenizeTeamChat( gentity_t *ent, char *dest, const char *src, size_t destsize ) {
	const char *p;
	char tokenText[64];
	int i = 0;

	if ( !ent || !ent->client ) {
		return;
	}

	for ( p = src; p && *p && destsize > 1; ++p ) {
		if ( *p == TEAM_CHAT_TOKEN_CHAR ) {
			WriteTextForToken( ent, *++p, tokenText, sizeof( tokenText ) );

			if ( !tokenText[0] && ( *p == 'b' || *p == 'B' ) && *++p ) { // special case for boon: write text after if i have it
				int len, offset = 0;
				char *s = strchr( p, TEAM_CHAT_TOKEN_CHAR );

				if ( s && *s ) {
					len = s - p;
				} else {
					len = strlen( p );
					offset = 1; // not terminated by a $, so point back to the char before
				}

				if ( !len ) { // retard terminated it immediately
					--p;
					continue;
				}

				if ( !ent->client->ps.powerups[PW_FORCE_BOON] ) {
					p += ( len - offset );
					continue;
				}

				destsize -= len;

				if ( destsize > 0 ) {
					Q_strcat( dest + i, len + 1, p );
					i += len;
					p += ( len - offset );
					continue;
				} else {
					break;
				}
			}

			if ( VALIDSTRING( tokenText ) ) {
				int len = strlen( tokenText );
				destsize -= len;

				if ( destsize > 0 ) {
					Q_strcat( dest + i, len + 1, tokenText );
					i += len;
					continue;
				} else {
					break; // don't write it at all if there is no room
				}
			}; 

			--p; // this token is invalid, go back to write the $ sign
		}

		dest[i++] = *p;
		--destsize;
	}

	dest[i] = '\0';
}

static qboolean ChatMessageShouldPause(const char *s) {
	if (!VALIDSTRING(s)) {
		assert(qfalse);
		return qfalse;
	}

	size_t len = strlen(s);

	if (!Q_stricmpn(s, "pause", 5) && len <= 6)
		return qtrue;

	if (!Q_stricmpn(s, "need to pause", 13) && len <= 14) // lg brain
		return qtrue;

	if (!Q_stricmpn(s, "sec", 3) && len <= 4) { // stricter requirement since this is so short
		const unsigned char c = (unsigned) *(s + 3);
		if (!c || isspace(c) || ispunct(c) || c == '\\' || c == '\'' || c == '~' || c == '`')
			return qtrue;
		return qfalse;
	}

	if (!Q_stricmpn(s, "1sec", 4) && len <= 5)
		return qtrue;

	if (!Q_stricmpn(s, "1 sec", 5) && len <= 6)
		return qtrue;

	return qfalse;
}

void Cmd_CallVote_f(gentity_t *ent, int pause);
void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText ) {
	int			j;
	gentity_t	*other;
	int			color;
	char		name[64];
	// don't let text be too long for malicious reasons
	char		text[MAX_SAY_TEXT] = { 0 };
	char		location[64];
	char		*locMsg = NULL;
	short		myClass;
	qboolean	doNotSendLocation = qfalse;

	// Check chat limit regardless of g_chatLimit since it now encapsulates
	// passwordless specs chat limit
	if ( ChatLimitExceeded( ent, mode ) ) {
		return;
	}

	if (g_lockdown.integer && !(ent->r.svFlags & SVF_BOT) && !G_ClientIsWhitelisted(ent - g_entities)) {
		return;
	}

	if ( g_gametype.integer < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	if (g_probation.integer && G_ClientIsOnProbation(ent - g_entities) && mode == SAY_TEAM && ent->client->sess.sessionTeam == TEAM_SPECTATOR && !(ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.siegeDesiredTeam && (ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1 || ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2))) {
		mode = SAY_ALL;
		trap_SendServerCommand(ent - g_entities, va("print \"You are on probation; your spec teamchat has been redirected into allchat.\n\""));
	}

	// allow typing "pause" in the chat to quickly call a pause vote
	if (level.isLivePug != ISLIVEPUG_NO && ent->client->sess.sessionTeam != TEAM_SPECTATOR && mode != SAY_TELL && ChatMessageShouldPause(chatText) && g_quickPauseChat.integer) { // allow a small typo at the end
		// allow setting g_quickPauseChat to 2 for callvote-only mode
		if (g_quickPauseChat.integer != 2) {
			// instapause
			level.pause.state = PAUSE_PAUSED;
			level.pause.time = level.time + 300000;
			NoteClientsOnLiftAtPause();
			PrintIngame(-1, "Pause requested by %s^7.\n", ent->client->pers.netname);
			Com_Printf("Pausing upon chat request by %s^7.\n", ent->client->pers.netname);
		}
		else {
			// just call a vote
			Cmd_CallVote_f(ent, PAUSE_PAUSED);
		}
	}
	else if (level.isLivePug != ISLIVEPUG_NO && ent->client->sess.sessionTeam != TEAM_SPECTATOR && mode != SAY_TELL && !Q_stricmpn(chatText, "unpause", 7) && strlen(chatText) <= 8 && g_quickPauseChat.integer) { // allow a small typo at the end
		Cmd_CallVote_f(ent, PAUSE_UNPAUSING);
	}

	switch ( mode ) {
	default:
	case SAY_ALL:
		G_LogPrintf( "say: %i %s: %s\n", ent-g_entities, ent->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_GREEN;
		break;
	case SAY_TEAM:
		G_LogPrintf( "sayteam: %i %s: %s\n", ent-g_entities, ent->client->pers.netname, chatText );
		if (Team_GetLocation(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
		color = COLOR_CYAN;
		break;
	case SAY_TELL:
        G_LogPrintf( "tell: %i %i %s to %s: %s\n", ent-g_entities, target-g_entities, 
			ent->client->pers.netname, target->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );

		if (target && g_gametype.integer >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocation(ent, location, sizeof(location)))
		{
			
			locMsg = location;
		}

		color = COLOR_MAGENTA;
		break;
	}

	if (mode == SAY_TEAM && ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		TokenizeTeamChat(ent, text, chatText, sizeof(text));
	}
	else {
		Q_strncpyz(text, chatText, sizeof(text));
	}

	if (g_improvedTeamchat.integer && g_gametype.integer == GT_SIEGE && mode == SAY_TEAM)
	{
		if (level.inSiegeCountdown)
		{
			//in siege countdown
			if (ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.siegeDesiredTeam && (ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1 || ent->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2))
			{
				//we are in spec (in countdown) with a desired team/class...put our class type as our "location"
				myClass = bgSiegeClasses[ent->client->siegeClass].playerClass;
				switch (myClass) {
				case 0:	locMsg = "Assault";			break;
				case 1:	locMsg = "Scout";			break;
				case 2:	locMsg = "Tech";			break;
				case 3:	locMsg = "Jedi";			break;
				case 4:	locMsg = "Demolitions";		break;
				case 5:	locMsg = "Heavy Weapons";	break;
				default:
					doNotSendLocation = qtrue;
					break;
				}
			}
		}
		else
		{
			//not in siege countdown
			if (ent->client->sess.sessionTeam == SIEGETEAM_TEAM1 || ent->client->sess.sessionTeam == SIEGETEAM_TEAM2)
			{
				if (ent->client->tempSpectate > level.time || ent->health <= 0)
				{
					//ingame and dead
					Com_sprintf(text, sizeof(text), "^0(DEAD) %c%c%s", Q_COLOR_ESCAPE, color, text);
				}
				else if (g_improvedTeamchat.integer >= 2 && !level.intermissiontime)
				{
					if (ent->client->ps.stats[STAT_ARMOR])
					{
						//we have armor
						Com_sprintf(text, sizeof(text), "^7(%i/%i) %c%c%s", ent->health, ent->client->ps.stats[STAT_ARMOR], Q_COLOR_ESCAPE, color, text);
					}
					else
					{
						//no armor
						Com_sprintf(text, sizeof(text), "^7(%i) %c%c%s", ent->health, Q_COLOR_ESCAPE, color, text);
					}
				}
			}
		}
	}

	if ( target ) {
		G_SayTo(ent, target, mode, color, name, text, locMsg);
		return;
	}

	// echo the text to the console
	// dont echo, it makes duplicated messages, already echoed with logprintf

	// send it to all the apropriate clients
	for (j = 0; j < level.maxclients; j++)
	{
		other = &g_entities[j];
		if (doNotSendLocation)
		{
			G_SayTo(ent, other, mode, color, name, text, NULL);
		}
		else
		{
			G_SayTo(ent, other, mode, color, name, text, locMsg);
		}
	}
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f(gentity_t *ent, char *override) {
	char buffer[MAX_TOKEN_CHARS] = { 0 };
	char firstArg[MAX_STRING_CHARS] = { 0 };
	char *space;
	gentity_t* found = NULL;
	char		*p;
	int			len;

	if (g_probation.integer && G_ClientIsOnProbation(ent - g_entities)) {
		trap_SendServerCommand(ent - g_entities, va("print \"You are on probation and cannot use private chat.\n\""));
		return;
	}

	if (!override && trap_Argc() < 3)
	{
		trap_SendServerCommand(ent - g_entities,
			"print \"usage: tell <name or client number> <message> (name can be just part of name, colors don't count. use ^5/whois^7 to see client numbers)  \n\"");
		return;
	}

	if (override && !override[0])
	{
		trap_SendServerCommand(ent - g_entities,
			"print \"usage: @[name or client number] [message] (name can be just part of name, colors don't count. use ^5/whois^7 to see client numbers)  \n\"");
		return;
	}

	if (override && override[0])
	{
		space = strchr(override, ' ');
		if (space == NULL || !space[1])
		{
			trap_SendServerCommand(ent - g_entities,
				"print \"usage: @[name or client number] [message] (name can be just part of name, colors don't count. use ^5/whois^7 to see client numbers)  \n\"");
			return;
		}
		Q_strncpyz(firstArg, override, sizeof(firstArg));
		firstArg[strlen(firstArg) - strlen(space)] = '\0';
		found = G_FindClient(firstArg);
	}
	else
	{
		trap_Argv(1, buffer, sizeof(buffer));
		found = G_FindClient(buffer);
	}

	if (!found || !found->client)
	{
		trap_SendServerCommand(
			ent - g_entities,
			va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"",
				override && override[0] ? firstArg : buffer));
		return;
	}

	if (g_probation.integer && G_ClientIsOnProbation(found - g_entities)) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s ^7is on probation and cannot receive private messages.\n\"", found->client->pers.netname));
		return;
	}

	if (override && override[0])
	{
		p = space + 1; //need to remove the space itself
	}
	else
	{
		p = ConcatArgs(2);
	}

	/* *CHANGE 4* anti say aaaaaa for whispering */
	len = strlen(p);
	if (len > 150)
	{
		p[149] = 0;

		if (len > 255) { //report only real threats
			G_HackLog("Too long message: Client num %d (%s) from %s tried to send too long (%i chars) message (truncated: %s).\n",
				ent->client->pers.clientNum, ent->client->pers.netname, ent->client->sess.ipString, len, p);
		}
	}

	G_Say(ent, found, SAY_TELL, p);
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if (ent != found && !(ent->r.svFlags & SVF_BOT) /*UNCOMMENT, JUST FOR DEBUG NOW*/) {
		G_SayTo(ent, ent, SAY_TELL, COLOR_MAGENTA, va("--> "EC"[%s%c%c"EC"]"EC": ", found->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE), p, NULL);
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent, int mode, qboolean arg0 ) {
	char		*p;
	int len;

	if ( trap_Argc () < 2 && !arg0 ) {
		return;
	}

	if (arg0)
	{
		p = ConcatArgs( 0 );
	}
	else
	{
		p = ConcatArgs( 1 );
	}

	if ( p && strlen(p) > 2 && p[0] == '@' && p[1] == '@' )
	{
		//redirect this as a private message
		Cmd_Tell_f( ent, p + 2 );
		return;
	}

	/* *CHANGE 3* Anti say aaaaaaaaaaa */
	len = strlen(p);
	if ( len > 150 )
	{
		p[149] = 0 ;

		if (len > 255){ //report only real threats
			G_HackLog("Too long message: Client num %d (%s) from %s tried to send too long (%i chars) message (truncated: %s).\n",
                    ent->client->pers.clientNum, ent->client->pers.netname,	ent->client->sess.ipString,	len, p);
		}
	}

	G_Say( ent, NULL, mode, p );
}

//siege voice command
static void Cmd_VoiceCommand_f(gentity_t *ent)
{
	gentity_t *te;
	static char arg[MAX_TOKEN_CHARS];
	char *s;
	int i = 0;
	int n = 0;

	if (g_gametype.integer < GT_TEAM)
	{
		return;
	}

	if (trap_Argc() < 2)
	{
		// list available arguments to help users bind stuff (otherwise the only way is to use assets/code)
		
		char voiceCmdList[MAX_STRING_CHARS];
		Q_strncpyz( voiceCmdList, S_COLOR_WHITE"Available voice_cmd commands:", sizeof( voiceCmdList ) );

		while ( i < MAX_CUSTOM_SIEGE_SOUNDS ) {
			if ( !bg_customSiegeSoundNames[i] ) {
				break;
			}

			if ( i % 8 == 0 ) { // line break every 8 commands
				Q_strcat( voiceCmdList, sizeof( voiceCmdList ), "\n    "S_COLOR_YELLOW );
			}

			Q_strcat( voiceCmdList, sizeof( voiceCmdList ), bg_customSiegeSoundNames[i] + 1 ); // skip the * character
			Q_strcat( voiceCmdList, sizeof( voiceCmdList ), " " );

			++i;
		}

		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", voiceCmdList ) );
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOICECHATASSPEC")) );
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	if (arg[0] == '*')
	{ //hmm.. don't expect a * to be prepended already. maybe someone is trying to be sneaky.
		return;
	}

	s = va("*%s", arg);

	//now, make sure it's a valid sound to be playing like this.. so people can't go around
	//screaming out death sounds or whatever.
	while (i < MAX_CUSTOM_SIEGE_SOUNDS)
	{
		if (!bg_customSiegeSoundNames[i])
		{
			break;
		}
		if (!Q_stricmp(bg_customSiegeSoundNames[i], s))
		{ //it matches this one, so it's ok
			break;
		}
		i++;
	}

	if (i == MAX_CUSTOM_SIEGE_SOUNDS || !bg_customSiegeSoundNames[i])
	{ //didn't find it in the list
		return;
	}

	if (ChatLimitExceeded(ent, -1)) {
		return;
	}

	// always allow "air support" and "emplaced guns" binds to be heard by enemies so you can gloat after mad airs or taunt corner securers
	qboolean airSupport = (!Q_stricmp(s, "*spot_air") || !Q_stricmp(s, "*spot_emplaced")) ? qtrue : qfalse;

	for (n = 0; n < level.maxclients; n++) {
		if (level.clients[n].pers.connected == CON_DISCONNECTED ||
			g_entities[n].r.svFlags & SVF_BOT)
			continue;

		if (g_fixVoiceChat.integer && level.clients[n].sess.sessionTeam == OtherTeam(ent->client->sess.sessionTeam) && !airSupport)
			continue;

		te = G_TempEntity(vec3_origin, EV_VOICECMD_SOUND);
		te->s.groundEntityNum = ent->s.number;
		te->s.eventParm = G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
#ifdef NEWMOD_SUPPORT
		// send location to teammates
		if (!(g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY) && !level.zombies && level.clients[n].sess.sessionTeam != OtherTeam(ent->client->sess.sessionTeam)) {
			int loc = Team_GetLocation(ent, NULL, 0);
			if (loc)
				te->s.powerups = loc;
		}
#endif
		te->r.svFlags |= SVF_SINGLECLIENT;
		te->r.svFlags |= SVF_BROADCAST;
		te->r.singleClient = n;
	}
}


static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};

void Cmd_GameCommand_f( gentity_t *ent ) {
	int		player;
	int		order;
	static char	str[MAX_TOKEN_CHARS];

	trap_Argv( 1, str, sizeof( str ) );
	player = atoi( str );
	trap_Argv( 2, str, sizeof( str ) );
	order = atoi( str );

	if ((player >= level.maxclients && player < MAX_CLIENTS)
		||  order == sizeof(gc_orders)/sizeof(char *)){
		G_HackLog("Client %i (%s) from %s is attempting GameCommand crash.\n",
			ent-g_entities, ent->client->pers.netname,ent->client->sess.ipString);
	}

	// real version, use in release
	if ( player < 0 || player >= level.maxclients ) {
		return;
	}

	if ( order < 0 || order >= sizeof(gc_orders)/sizeof(char *) ) {
		return;
	}
	G_Say( ent, &g_entities[player], SAY_TELL, gc_orders[order] );
	G_Say( ent, ent, SAY_TELL, gc_orders[order] );
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	if (!ent->client)
		return;

	trap_SendServerCommand( ent - g_entities, va( "print \"Origin: %s ; Yaw: %.2f degrees\n\"", vtos( ent->client->ps.origin ), ent->client->ps.viewangles[YAW] ) );
}

void Cmd_TestVis_f(gentity_t *ent)
{
	if (!ent->client)
		return;

	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}

	char buffer[64];
	gentity_t *found = NULL;

	trap_Argv(1, buffer, sizeof(buffer));
	found = G_FindClient(buffer);

	if (!found || !found->client)
	{
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", buffer);
		return;
	}

	if (G_ClientCanBeSeenByClient(found, ent))
	{
		trap_SendServerCommand(ent - g_entities, va("print \"%s^7 is ^2visible!^7\n\"", found->client->pers.netname));
	}
	else
	{
		trap_SendServerCommand(ent - g_entities, va("print \"%s^7 is ^1not^7 visible.\n\"", found->client->pers.netname));
	}

}

/*
==================
Cmd_TargetInfo_f
Debug command to show information about whatever we are aiming at
==================
*/
void Cmd_TargetInfo_f(gentity_t *ent)
{

	if (!ent->client)
		return;

	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}

	gentity_t *infoEnt;

	if (trap_Argc() > 1) {
		char entNumBuf[5];
		trap_Argv(1, entNumBuf, sizeof(entNumBuf));
		int entNum = atoi(entNumBuf);
		if (entNum < 0 || entNum >= MAX_GENTITIES - 1) {
			trap_SendServerCommand(ent - g_entities, "print \"Invalid entity.\nUsage:  targetinfo <optional entity number>\n\"");
			return;
		}
		infoEnt = &g_entities[entNum];
		if (!infoEnt->inuse) {
			trap_SendServerCommand(ent - g_entities, "print \"Invalid entity.\nUsage:  targetinfo <optional entity number>\n\"");
			return;
		}
	}
	else {
		trace_t tr;
		vec3_t tfrom, tto, fwd;
		VectorCopy(ent->client->ps.origin, tfrom);
		tfrom[2] += ent->client->ps.viewheight;
		AngleVectors(ent->client->ps.viewangles, fwd, NULL, NULL);
		tto[0] = tfrom[0] + fwd[0] * 999999;
		tto[1] = tfrom[1] + fwd[1] * 999999;
		tto[2] = tfrom[2] + fwd[2] * 999999;

		trap_Trace(&tr, tfrom, NULL, NULL, tto, ent->s.number, MASK_PLAYERSOLID);

		infoEnt = &g_entities[tr.entityNum];
	}

	trap_SendServerCommand(ent - g_entities, va("print \"^%iI^%in^%if^%io^7 for entity %i (%s)\n\"", Q_irand(1, 7), Q_irand(1, 7), Q_irand(1, 7), Q_irand(1, 7), infoEnt->s.number, infoEnt->client ? "^2client^7" : "^1!client^7"));

	if (VALIDSTRING(infoEnt->classname)) {
		trap_SendServerCommand(ent - g_entities, va("print \"classname == %s\n\"", infoEnt->classname));
		if (!Q_stricmp(infoEnt->classname, "func_door")) {
			char *moverStateStr = NULL;
			switch (infoEnt->moverState) {
			case MOVER_POS1: moverStateStr = "MOVER_POS1"; break;
			case MOVER_POS2: moverStateStr = "MOVER_POS2"; break;
			case MOVER_1TO2: moverStateStr = "MOVER_1TO2"; break;
			case MOVER_2TO1: moverStateStr = "MOVER_2TO1"; break;
			default: moverStateStr = "?"; break;
			}
			trap_SendServerCommand(ent - g_entities, va("print \"moverState == %d (%s)\n\"", infoEnt->moverState, moverStateStr));
		}
	}

	if (VALIDSTRING(infoEnt->targetname))
		trap_SendServerCommand(ent - g_entities, va("print \"targetname: %s\n\"", infoEnt->targetname));

	if (VALIDSTRING(infoEnt->target))
		trap_SendServerCommand(ent - g_entities, va("print \"target: %s\n\"", infoEnt->target));

	trap_SendServerCommand(ent - g_entities, va("print \"health: %d (max: %d)\n\"", infoEnt->health, infoEnt->maxHealth));
	trap_SendServerCommand(ent - g_entities, va("print \"eFlags: %d (eFlags2: %d)\n\"", infoEnt->s.eFlags, infoEnt->s.eFlags2));

	if (infoEnt->client && infoEnt->client->ewebIndex)
		trap_SendServerCommand(ent - g_entities, va("print \"ewebIndex == %i\n\"", infoEnt->client->ewebIndex));

	if (infoEnt->client && infoEnt->client->ps.emplacedIndex)
		trap_SendServerCommand(ent - g_entities, va("print \"emplacedIndex == %i\n\"", infoEnt->client->ps.emplacedIndex));

	if (infoEnt->s.weapon) {
		char *weaponStr;
		switch (infoEnt->s.weapon) {
		case WP_STUN_BATON: weaponStr = "Stun Baton"; break;
		case WP_MELEE: weaponStr = "Melee"; break;
		case WP_SABER: weaponStr = "Saber"; break;
		case WP_BRYAR_PISTOL: weaponStr = "Pistol"; break;
		case WP_BLASTER: weaponStr = "Blaster"; break;
		case WP_DISRUPTOR: weaponStr = "Disruptor"; break;
		case WP_BOWCASTER: weaponStr = "Bowcaster"; break;
		case WP_REPEATER: weaponStr = "Repeater"; break;
		case WP_DEMP2: weaponStr = "Demp"; break;
		case WP_FLECHETTE: weaponStr = "Golan"; break;
		case WP_ROCKET_LAUNCHER: weaponStr = "Rocket Launcher"; break;
		case WP_THERMAL: weaponStr = "Thermals"; break;
		case WP_TRIP_MINE: weaponStr = "Mines"; break;
		case WP_DET_PACK: weaponStr = "Detpacks"; break;
		case WP_CONCUSSION: weaponStr = "Conc"; break;
		case WP_BRYAR_OLD: weaponStr = "Old Bryar Pistol"; break;
		case WP_EMPLACED_GUN: weaponStr = "Emplaced"; break;
		case WP_TURRET: weaponStr = "Turret"; break;
		default: weaponStr = "?"; break;
		}
		trap_SendServerCommand(ent - g_entities, va("print \"weapon == %i (%s)\n\"", infoEnt->s.weapon, weaponStr));
	}

	if (infoEnt->r.contents) {
		char contentsBuf[MAX_STRING_CHARS] = { 0 };
		if (infoEnt->r.contents & CONTENTS_SOLID)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%ssolid", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_LAVA)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%slava", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_WATER)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%swater", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_FOG)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sfog", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_PLAYERCLIP)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%splayerclip", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_MONSTERCLIP)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%smonsterclip", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_BOTCLIP)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sbotclip", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_SHOTCLIP)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sshotclip", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_BODY)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sbody", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_CORPSE)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%scorpse", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_TRIGGER)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%strigger", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_NODROP)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%snodrop", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_TERRAIN)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sterrain", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_LADDER)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sladder", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_ABSEIL)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sabseil", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_OPAQUE)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sopaque", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_OUTSIDE)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%soutside", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_INSIDE)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sinside", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_SLIME)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sslime", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_LIGHTSABER)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%slightsaber", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_TELEPORTER)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%steleporter", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_ITEM)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sitem", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_NOSHOT)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%snoshot", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_DETAIL)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%sdetail", contentsBuf[0] ? " " : ""));
		if (infoEnt->r.contents & CONTENTS_TRANSLUCENT)
			Q_strcat(contentsBuf, sizeof(contentsBuf), va("%stranslucent", contentsBuf[0] ? " " : ""));

		trap_SendServerCommand(ent - g_entities, va("print \"contents: %d / 0x%08X (%s)\n\"", infoEnt->r.contents, infoEnt->r.contents, contentsBuf));
	}
}

#define MAX_CHANGES_CHUNKS		4
#define CHANGES_CHUNK_SIZE		1000
#define MAX_CHANGES_SIZE		(MAX_CHANGES_CHUNKS * CHANGES_CHUNK_SIZE)
void Cmd_Changes_f(gentity_t *ent) {
	static char changes[MAX_CHANGES_SIZE] = { 0 }, map[MAX_CVAR_VALUE_STRING] = { 0 };
	static qboolean lookedForChanges = qfalse;
	static size_t len = 0;

	if (!lookedForChanges) {
		lookedForChanges = qtrue;
		fileHandle_t f;
		vmCvar_t	mapname;
		trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);
		Q_strncpyz(map, mapname.string, sizeof(map));
		len = trap_FS_FOpenFile(va("maps/%s.changes", mapname.string), &f, FS_READ);
		if (f) {
			trap_FS_Read(changes, len, f);
			trap_FS_FCloseFile(f);
			if (len >= MAX_CHANGES_SIZE)
				G_LogPrintf("Warning: changelog for map %s is too long (%d chars, should be less than %d)\n", map, len, sizeof(changes));
		}
	}

	if (!changes[0] || !len) {
		trap_SendServerCommand(ent - g_entities, va("print \"No changelog could be found for %s.\n\"", map));
		return;
	}

	trap_SendServerCommand(ent - g_entities, va("print \"Changelog for %s"S_COLOR_WHITE":\n\"", map));

	char fixed[MAX_CHANGES_SIZE] = { 0 };
	char *r = changes, *w = fixed;
	int remaining = MAX_CHANGES_SIZE - 1;
	while (*r) {
		if (*r == '%' && r - changes && isdigit(*(r - 1)) && remaining > 8) {
			Q_strncpyz(w, " percent", remaining);
			remaining -= 8;
			w += 8;
		}
		else if (*r == '%' && remaining > 7) {
			Q_strncpyz(w, "percent", remaining);
			remaining -= 7;
			w += 7;
		}
		else {
			*w = *r;
			remaining--;
			w++;
		}
		r++;
	}
	len = strlen(fixed);

	if (len <= CHANGES_CHUNK_SIZE) { // no chunking necessary
		trap_SendServerCommand(ent - g_entities, va("print \"%s"S_COLOR_WHITE"\n\"", fixed));
	}
	else { // chunk it
		int i, chunks = len / CHANGES_CHUNK_SIZE;
		if (len % CHANGES_CHUNK_SIZE > 0)
			chunks++;
		for (i = 0; i < chunks && i < MAX_CHANGES_CHUNKS; i++) {
			char thisChunk[CHANGES_CHUNK_SIZE + 1];
			Q_strncpyz(thisChunk, fixed + (i * CHANGES_CHUNK_SIZE), sizeof(thisChunk));
			if (thisChunk[0])
				trap_SendServerCommand(ent - g_entities, va("print \"%s%s\"", thisChunk, i == chunks - 1 ? S_COLOR_WHITE"\n" : "")); // add ^7 and line break for last one
		}
	}
}

void Cmd_SkillBoost_f(gentity_t *ent) {
	qboolean wrotePreface = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.skillBoost) {
			if (!wrotePreface) {
				trap_SendServerCommand(ent - g_entities, "print \"Currently skillboosted players:\n\"");
				wrotePreface = qtrue;
			}
			trap_SendServerCommand(ent - g_entities, va("print \"^7%s^7 has a level ^5%d^7 skillboost.\n\"",
				g_entities[i].client->pers.netname, g_entities[i].client->sess.skillBoost));
		}
	}
	if (!wrotePreface) {
		trap_SendServerCommand(ent - g_entities, "print \"No players are currently skillboosted.\n\"");
	}
}

void Cmd_SenseBoost_f(gentity_t *ent) {
	qboolean wrotePreface = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.senseBoost) {
			if (!wrotePreface) {
				trap_SendServerCommand(ent - g_entities, "print \"Currently senseboosted players:\n\"");
				wrotePreface = qtrue;
			}
			trap_SendServerCommand(ent - g_entities, va("print \"^7%s^7 has a level ^5%d^7 senseboost.\n\"",
				g_entities[i].client->pers.netname, g_entities[i].client->sess.senseBoost));
		}
	}
	if (!wrotePreface) {
		trap_SendServerCommand(ent - g_entities, "print \"No players are currently senseboosted.\n\"");
	}
}

void Cmd_AimBoost_f(gentity_t *ent) {
	qboolean wrotePreface = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.aimBoost) {
			if (!wrotePreface) {
				trap_SendServerCommand(ent - g_entities, "print \"Currently aimboosted players:\n\"");
				wrotePreface = qtrue;
			}
			trap_SendServerCommand(ent - g_entities, va("print \"^7%s^7 has a level ^5%d^7 aimboost.\n\"",
				g_entities[i].client->pers.netname, g_entities[i].client->sess.aimBoost));
		}
	}
	if (!wrotePreface) {
		trap_SendServerCommand(ent - g_entities, "print \"No players are currently aimboosted.\n\"");
	}
}

void Cmd_PugMaps_f(gentity_t *ent) {
	qboolean gotOne = qfalse;
	for (char c = 'a'; c <= 'z'; c++) {
		char fileName[MAX_QPATH] = { 0 }, prettyName[MAX_QPATH] = { 0 };
		if (LongMapNameFromChar(c, fileName, sizeof(fileName), prettyName, sizeof(prettyName))) {
			trap_SendServerCommand(ent - g_entities, va("print \"%sMap ^3%c^7: %s (%s):\n\"", gotOne ? "" : "List of map letters for /callvote newpug:\n", c, prettyName, fileName));
			gotOne = qtrue;
		}
	}
	if (!gotOne)
		trap_SendServerCommand(ent - g_entities, "print \"This server is not properly configured for pug map voting.\n\"");
}

void Cmd_Anim_f(gentity_t *ent) {
	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time || ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		ent->client->ps.pm_type == PM_SPECTATOR || ent->client->ps.pm_type == PM_INTERMISSION)
		return;

	if (trap_Argc() < 2) {
		trap_SendServerCommand(ent - g_entities, va("print \"Usage: anim <animation number between 1 and %d> [optional separate animation number for legs] [optional time in milliseconds]\n\"", MAX_ANIMATIONS - 1));
		return;
	}

	char buf[8] = { 0 };
	trap_Argv(1, buf, sizeof(buf));
	if (!buf[0]) {
		trap_SendServerCommand(ent - g_entities, va("print \"Usage: anim <animation number between 1 and %d> [optional separate animation number for legs] [optional time in milliseconds]\n\"", MAX_ANIMATIONS - 1));
		return;
	}

	int animNum = atoi(buf);
	if (animNum < 0 || animNum >= MAX_ANIMATIONS) {
		trap_SendServerCommand(ent - g_entities, va("print \"Usage: anim <animation number between 1 and %d> [optional separate animation number for legs] [optional time in milliseconds]\n\"", MAX_ANIMATIONS - 1));
		return;
	}

	int secondAnimNum = 0;
	if (trap_Argc() >= 3) {
		trap_Argv(2, buf, sizeof(buf));
		secondAnimNum = atoi(buf);
		if (secondAnimNum < 0 || secondAnimNum >= MAX_ANIMATIONS) {
			trap_SendServerCommand(ent - g_entities, va("print \"Usage: anim <animation number between 1 and %d> [optional separate animation number for legs] [optional time in milliseconds]\n\"", MAX_ANIMATIONS - 1));
			return;
		}
	}
	int time = 1000; // default to one second
	if (trap_Argc() >= 4) {
		trap_Argv(3, buf, sizeof(buf));
		time = atoi(buf);
		if (time < 0)
			return;
		else if (time > 60000)
			time = 60000; // limit to one minute i guess
	}

	if (trap_Argc() == 2 || !secondAnimNum) {
		ent->client->ps.torsoAnim = animNum;
		ent->client->ps.legsAnim = animNum;
	}
	else {
		ent->client->ps.torsoAnim = animNum;
		ent->client->ps.legsAnim = secondAnimNum;
	}

	ent->client->ps.torsoTimer = time;
	ent->client->ps.legsTimer = time;
}

void Cmd_GetAnim_f(gentity_t *ent) {
	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time || ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		ent->client->ps.pm_type == PM_SPECTATOR || ent->client->ps.pm_type == PM_INTERMISSION)
		return;

	trap_SendServerCommand(ent - g_entities, va("print \"torsoAnim is %d, legsAnim is %d\n\"", ent->client->ps.torsoAnim, ent->client->ps.legsAnim));
}

extern void GlassDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

/*
==================
Cmd_KillTarget_f
Debug command to kill whatever we are aiming at
==================
*/
void Cmd_KillTarget_f(gentity_t *ent)
{

	if (!ent || !ent->client)
		return;

	if (!g_cheats.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}

	trace_t tr;
	vec3_t tfrom, tto, fwd;
	gentity_t *traceEnt;

	VectorCopy(ent->client->ps.origin, tfrom);
	tfrom[2] += ent->client->ps.viewheight;
	AngleVectors(ent->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0] * 999999;
	tto[1] = tfrom[1] + fwd[1] * 999999;
	tto[2] = tfrom[2] + fwd[2] * 999999;

	trap_Trace(&tr, tfrom, NULL, NULL, tto, ent->s.number, MASK_PLAYERSOLID);

	if (tr.entityNum && tr.entityNum == ENTITYNUM_WORLD)
	{
		return;
	}

	traceEnt = &g_entities[tr.entityNum];

	if (!traceEnt)
	{
		return;
	}

	if (traceEnt->classname && traceEnt->classname[0] && !Q_stricmp(traceEnt->classname, "func_glass"))
	{
		//need a special exception for glass; just damaging it seems to crash the server
		GlassDie(traceEnt, traceEnt, traceEnt, 99999, MOD_ROCKET);
		return;
	}

	G_Damage(traceEnt, ent, ent, NULL, NULL, 99999, 0, MOD_ROCKET);

}

static const char *gameNames[] = {
	"Free For All",
	"Holocron FFA",
	"Jedi Master",
	"Duel",
	"Power Duel",
	"Single Player",
	"Team FFA",
	"Siege",
	"Capture the Flag",
	"Capture the Ysalamiri"
};

/*
==================
G_ClientNumberFromName

Finds the client number of the client with the given name
==================
*/
int G_ClientNumberFromName ( const char* name )
{
	static char		s2[MAX_STRING_CHARS];
	static char		n2[MAX_STRING_CHARS];
	int			i;
	gclient_t*	cl;

	// check for a name match
	SanitizeString( (char*)name, s2 );
	for ( i=0, cl=level.clients ; i < level.numConnectedClients ; i++, cl++ ) 
	{
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) 
		{
			return i;
		}
	}

	return -1;
}

/*
==================
SanitizeString2

Rich's revised version of SanitizeString
==================
*/
void SanitizeString2( char *in, char *out )
{
	int i = 0;
	int r = 0;

	while (in[i])
	{
		if (i >= MAX_NAME_LENGTH-1)
		{ //the ui truncates the name here..
			break;
		}

		if (in[i] == '^')
		{
			if (in[i+1] >= 48 && //'0'
				in[i+1] <= 57) //'9'
			{ //only skip it if there's a number after it for the color
				i += 2;
				continue;
			}
			else
			{ //just skip the ^
				i++;
				continue;
			}
		}

		if (in[i] < 32)
		{
			i++;
			continue;
		}

		out[r] = in[i];
		r++;
		i++;
	}
	out[r] = 0;
}

/*
==================
G_ClientNumberFromStrippedName

Same as above, but strips special characters out of the names before comparing.
==================
*/
int G_ClientNumberFromStrippedName ( const char* name )
{
	static char		s2[MAX_STRING_CHARS];
	static char		n2[MAX_STRING_CHARS];
	int			i;
	gclient_t*	cl;

	// check for a name match
	SanitizeString2( (char*)name, s2 );
	for ( i=0, cl=level.clients ; i < level.numConnectedClients ; i++, cl++ ) 
	{
		SanitizeString2( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) 
		{
			return i;
		}
	}

	return -1;
}

//MapPool pools[64];
//int poolNum = 0;
//
//void G_LoadMapPool(const char* filename)
//{
//	int				len;
//	fileHandle_t	f;
//	int             map;
//	static char		buf[MAX_POOLS_TEXT];
//	char			*line;
//
//	if (poolNum == MAX_MAPS_IN_POOL)
//		return;
//
//	if (strlen(filename) > MAX_MAP_POOL_ID)
//		return;
//
//	// open map pool file
//	len = trap_FS_FOpenFile(filename, &f, FS_READ);
//	if (!f) {
//		trap_Printf(va(S_COLOR_RED "file not found: %s\n", filename));
//		return;
//	}
//
//	if (len >= MAX_POOLS_TEXT) {
//		trap_Printf(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_ARENAS_TEXT));
//		trap_FS_FCloseFile(f);
//		return;
//	}
//
//	trap_FS_Read(buf, len, f);
//	buf[len] = 0;
//	trap_FS_FCloseFile(f);
//
//	// store file name as an id
//	COM_StripExtension(filename, pools[poolNum].id);
//
//	line = strtok(buf, "\r\n");
//	if (!line)
//	{
//		Com_Printf("Pool %s is corrupted\n", filename);
//		return;
//	}
//
//	// long name for votes	
//	Q_strncpyz(pools[poolNum].longname, line, MAX_MAP_POOL_LONGNAME);
//
//	// cycle through all arenas
//	map = 0;
//	while ( (line = strtok(0, "\r\n")) )
//	{
//		const char* error;
//		Q_strncpyz(pools[poolNum].maplist[map], line, MAX_MAP_NAME);
//
//		// check the arena validity (exists and supports current gametype)
//		// TBD optimize arena search, hash map perhaps
//		error = G_DoesMapSupportGametype(pools[poolNum].maplist[map], g_gametype.integer);
//		if (!error)
//		{
//			++map;
//		}
//		else
//		{
//		}
//	}
//
//	pools[poolNum].mapsCount = map;
//
//	Com_Printf("Loaded map pool %s, maps %i\n", filename, pools[poolNum].mapsCount);
//
//	++poolNum;
//}

//void G_LoadVoteMapsPools()
//{
//	int			numdirs;
//	char		filename[128];
//	char		dirlist[4096];
//	char*		dirptr;
//	int			dirlen;
//	int         i;
//
//	// load all .pool files
//	numdirs = trap_FS_GetFileList("", ".pool", dirlist, sizeof(dirlist));
//	dirptr = dirlist;
//
//	for (i = 0; i < numdirs; i++, dirptr += dirlen + 1)
//	{
//		dirlen = strlen(dirptr);
//		strcpy(filename, dirptr);
//		G_LoadMapPool(filename);
//	}
//
//}

qboolean TryingToDoCallvoteTakeover(gentity_t *ent)
{
	int i = 0;
	int numEligible = 0;
	int numTotal = 0;

	for (i = 0; i < level.maxclients; i++)
	{
		if (level.clients[i].pers.connected != CON_DISCONNECTED && !(g_entities[i].r.svFlags & SVF_BOT)) //connected player who is not a bot
		{
			if (G_ClientIsOnProbation(i) || g_lockdown.integer && !G_ClientIsWhitelisted(i))
			{
				numTotal++; //you are not probation, so you are not eligible
			}
			else if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL || level.clients[i].sess.sessionTeam != TEAM_SPECTATOR) //everyone is eligible in duel gametypes
			{
				numEligible++; //you are not a spectator, so you are eligible
				numTotal++;
			}
			else
			{
				numTotal++; //you are a spectator, so you are not eligible
			}
		}
	}
	if (numTotal >= 6 && numEligible < 2)
	{
		return qtrue;
	}
	return qfalse;

}

void fixTeamVoters(gentity_t *ent)
{
	int i;
	int numRedTeamers = 0;
	int numBlueTeamers = 0;

	if (!ent || !ent->client)
	{
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_RED)
	{
		level.numRequiredTeamVotes[0] = 0;

		for (i = 0; i < level.maxclients; i++)
		{
			if (level.clients[i].sess.sessionTeam == TEAM_RED)
			{
				level.clients[i].mGameFlags &= ~PSG_TEAMVOTEDRED;
				level.clients[i].mGameFlags &= ~PSG_CANTEAMVOTERED;
			}
		}

		for (i = 0; i < level.maxclients; i++)
		{
			if (level.clients[i].pers.connected != CON_DISCONNECTED) {
				if (level.clients[i].sess.sessionTeam == TEAM_RED)
				{
					if (!(g_entities[i].r.svFlags & SVF_BOT) && !G_ClientIsOnProbation(i) && !(g_lockdown.integer && !G_ClientIsWhitelisted(i)))
					{
						level.clients[i].mGameFlags |= PSG_CANTEAMVOTERED;
						numRedTeamers++;
					}
				}
			}
		}

		ent->client->mGameFlags |= PSG_TEAMVOTEDRED;
		if (numRedTeamers >= 3)
		{
			level.numRequiredTeamVotes[0] = numRedTeamers - 1; //require everyone but the troll himself to vote yes.
			level.numRequiredTeamVotesNo[0] = 2;
		}
		else
		{
			level.numRequiredTeamVotes[0] = numRedTeamers; //require 100% yes votes in 2v2 (or 1v1...)
			level.numRequiredTeamVotesNo[0] = 1;
		}
	}
	else
	{
		ent->client->mGameFlags |= PSG_TEAMVOTEDBLUE;
		level.numRequiredTeamVotes[1] = 0;

		for (i = 0; i < level.maxclients; i++)
		{
			if (level.clients[i].sess.sessionTeam == TEAM_BLUE)
			{
				level.clients[i].mGameFlags &= ~PSG_TEAMVOTEDBLUE;
				level.clients[i].mGameFlags &= ~PSG_CANTEAMVOTEBLUE;
			}
		}

		for (i = 0; i < level.maxclients; i++)
		{
			if (level.clients[i].pers.connected != CON_DISCONNECTED) {
				if (level.clients[i].sess.sessionTeam == TEAM_BLUE)
				{
					if (!(g_entities[i].r.svFlags & SVF_BOT) && !G_ClientIsOnProbation(i) && !(g_lockdown.integer && !G_ClientIsWhitelisted(i)))
					{
						level.clients[i].mGameFlags |= PSG_CANTEAMVOTEBLUE;
						numBlueTeamers++;
					}
				}
			}
		}

		ent->client->mGameFlags |= PSG_TEAMVOTEDBLUE;
		if (numBlueTeamers >= 3)
		{
			level.numRequiredTeamVotes[1] = numBlueTeamers - 1; //require everyone but the troll himself to vote yes.
			level.numRequiredTeamVotesNo[1] = 2;
		}
		else
		{
			level.numRequiredTeamVotes[1] = numBlueTeamers; //require 100% yes votes in 2v2 (or 1v1...)
			level.numRequiredTeamVotesNo[1] = 1;
		}
	}
}

void fixVoters(){
	int i;

	level.numVotingClients = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		level.clients[i].mGameFlags &= ~PSG_VOTED;
        level.clients[i].mGameFlags &= ~PSG_CANVOTE;
	}

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
			{
                /*
				// decide if this should be auto-followed     
				if ( level.clients[i].pers.connected == CON_CONNECTED )
				{
                */
				if (!(g_entities[i].r.svFlags & SVF_BOT) && !G_ClientIsOnProbation(i) && !(g_lockdown.integer && !G_ClientIsWhitelisted(i)))
					{
						level.clients[i].mGameFlags |= PSG_CANVOTE;
						level.numVotingClients++;
					}
                    /*
				}
                */
			}
		}
	}
}

void CountPlayersIngame( int *total, int *ingame ) {
	gentity_t *ent;
	int i;

	*total = *ingame = 0;

	for ( i = 0; i < level.maxclients; i++ ) { // count clients that are connected and who are not bots
		ent = &g_entities[i];

		if ( !ent || !ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || ent->r.svFlags & SVF_BOT ) {
			continue;
		}

		( *total )++;

		if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
			( *ingame )++;
		}
	}
}

/*
==================
Cmd_CallVote_f
==================
*/
const char *G_GetArenaInfoByMap( const char *map );
int G_GetArenaNumber( const char *map );

static int      g_votedCounts[MAX_ARENAS];

void Cmd_CallVote_f( gentity_t *ent, int pause ) {
	int		i;
	static char	arg1[MAX_STRING_TOKENS];
	char	arg2[MAX_CVAR_VALUE_STRING];
	char*		mapName = 0;
	const char*	arenaInfo;
    int argc;

	if (g_lockdown.integer && !G_ClientIsWhitelisted(ent - g_entities)) {
		return;
	}

	if (g_probation.integer && G_ClientIsOnProbation(ent - g_entities)) {
		trap_SendServerCommand(ent - g_entities, va("print \"You are on probation and cannot call votes.\n\""));
		return;
	}

	if ( !g_allowVote.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		return;
	}

	if ( level.voteTime || level.voteExecuteTime >= level.time ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEINPROGRESS")) );
		return;
	}

	//check for delays
	if (level.startTime + g_callvotedelay.integer*1000 > level.time){
		trap_SendServerCommand( ent-g_entities, 
			va("print \"Cannot call vote %i seconds after map start.\n\"",g_callvotedelay.integer)  );
		return;
	}

	if (ent->client->lastCallvoteTime + g_callvotedelay.integer*1000 > level.time){
		trap_SendServerCommand( ent-g_entities, 
			va("print \"Cannot call vote %i seconds after previous attempt.\n\"",g_callvotedelay.integer)  );
		return;
	}

	if (ent->client->lastJoinedTime + g_callvotedelay.integer*1000 > level.time){
		trap_SendServerCommand( ent-g_entities, 
			va("print \"Cannot call vote %i seconds after joining the game.\n\"",g_callvotedelay.integer)  );
		return;
	}

	if (g_gametype.integer != GT_DUEL &&
		g_gametype.integer != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam != TEAM_RED && ent->client->sess.sessionTeam != TEAM_BLUE ) {
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
			return;
		}
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	trap_Argv( 2, arg2, sizeof( arg2 ) );
    argc = trap_Argc();

	if (pause) {
		Q_strncpyz(arg1, pause == PAUSE_PAUSED ? "pause" : "unpause", sizeof(arg1));
		arg2[0] = '\0';
	}

	// *CHANGE 8a* anti callvote bug
	if ((g_protectCallvoteHack.integer && (strchr( arg1, '\n' ) || strchr( arg2, '\n' ) ||	strchr( arg1, '\r' ) || strchr( arg2, '\r' ))) ) {
		//lets replace line breaks with ; for better readability
		int j, len;
		len = strlen(arg1);
		for(j = 0; j < len; ++j){
			if(arg1[j]=='\n' || arg1[j]=='\r') 
				arg1[j] = ';';
		}

		len = strlen(arg2);
		for(j = 0; j < len; ++j){
			if(arg2[j]=='\n' || arg2[j]=='\r') 
				arg2[j] = ';';
		}

		G_HackLog("Callvote hack: Client num %d (%s) from %s tries to hack via callvote (callvote %s \"%s\").\n",
                ent->client->pers.clientNum, 
				ent->client->pers.netname,
				ent->client->sess.ipString,
                arg1, arg2);
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if( strchr( arg1, ';'  ) || strchr( arg2, ';'  )){
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if ( !Q_stricmp( arg1, "map_restart" ) ) {
	} else if ( !Q_stricmp( arg1, "nextmap" ) ) {
	} else if ( !Q_stricmp( arg1, "map" ) ) {
	} else if ( !Q_stricmp( arg1, "map_random" ) ) {
	} else if ( !Q_stricmp( arg1, "randompugmap" ) ) {
	} else if ( !Q_stricmp( arg1, "newpug" ) ) {
	} else if ( !Q_stricmp( arg1, "nextpug" ) ) {
	} else if ( !Q_stricmp( arg1, "g_gametype" ) ) {
	} else if ( !Q_stricmp( arg1, "kick" ) ) {
	} else if ( !Q_stricmp( arg1, "clientkick" ) ) {
	} else if ( !Q_stricmp( arg1, "g_doWarmup" ) ) {
	} else if ( !Q_stricmp( arg1, "timelimit" ) ) {
	} else if ( !Q_stricmp( arg1, "fraglimit" ) ) {
	} else if ( !Q_stricmp( arg1, "resetflags" ) ) { //flag reset when they disappear (debug)
	} else if ( !Q_stricmp( arg1, "q" ) ) { //general question vote :)
	} else if ( !Q_stricmp( arg1, "pause" ) ) {
	} else if ( !Q_stricmp( arg1, "unpause" ) ) { 
	} else if ( !Q_stricmp( arg1, "endmatch" ) ) {
	} else if ( !Q_stricmp ( arg1, "lockteams") ) {
	} else if ( !Q_stricmp( arg1, "cointoss")) {
    } else if ( !Q_stricmp( arg1, "randomcapts")) {
    } else if ( !Q_stricmp( arg1, "randomteams")) {
	} else if ( !Q_stricmp( arg1, "shuffleteams")) {
	} else if ( !Q_stricmp( arg1, "killturrets")) {
	} else if (!Q_stricmpn(arg1, "quickspawn", 10) || !Q_stricmpn(arg1, "quickrespawn", 12) ||
		!Q_stricmpn(arg1, "fastspawn", 9) || !Q_stricmpn(arg1, "fastrespawn", 11) || !Q_stricmpn(arg1, "respawn", 7)) {
	} else if ( !Q_stricmp( arg1, "zombies")) {
	} else if (!Q_stricmp(arg1, "pug")) {
	} else if (!Q_stricmp(arg1, "pub")) {
	} else if (!Q_stricmp(arg1, "g_redTeam")) {
	} else if (!Q_stricmp(arg1, "g_blueTeam")) {
	} else if (!Q_stricmp(arg1, "forceround2")) {
	} else if ( !Q_stricmp( arg1, "ffa" ) ) {
	} else if ( !Q_stricmp( arg1, "duel" ) ) {
	} else if ( !Q_stricmp( arg1, "powerduel" ) ) {
	} else if ( !Q_stricmp( arg1, "tffa" ) ) {
	} else if ( !Q_stricmp( arg1, "siege" ) ) {
	} else if ( !Q_stricmp( arg1, "siege_restart" ) ) {
	} else if ( !Q_stricmp( arg1, "ctf" ) ) {
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Vote commands are: map_restart, nextmap, map <mapname>, g_gametype <n>, "
			"kick <player>, clientkick <clientnum>, g_doWarmup, timelimit <time>, fraglimit <frags>, cointoss, forceround2, killturrets, quickspawns,"
			"resetflags, q <question>, pause, unpause, endmatch, randomcapts, randomteams <numRedPlayers> <numBluePlayers>, shuffleteams <numRedPlayers> <numBluePlayers>, g_redTeam, g_blueTeam, zombies, lockTeams <numPlayers>, newpug, nextpug, randompugmap\n\"" );
		return;
	}

	// if there is still a vote to be executed
	if ( level.voteExecuteTime ) {
		level.voteExecuteTime = 0;

		/*
		if (!Q_stricmpn(level.voteString,"map_restart",11)){
			trap_Cvar_Set("g_wasRestarted", "1");
		}
		*/

		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );
	}

	// if calling a nextmap vote on a siege server where nextmap is set to "map_restart 0"
	// (basically any pug server, since it doesn't have a standard map rotation setup),
	// redirect this vote to the new siege_restart vote
	// the purpose of this is that showing the words "Next Map" is misleading in such a context
	if (g_gametype.integer == GT_SIEGE && !Q_stricmp(arg1, "nextmap")) {
		char nextmapBuf[MAX_QPATH] = { 0 };
		trap_Cvar_VariableStringBuffer("nextmap", nextmapBuf, sizeof(nextmapBuf));
		if (nextmapBuf[0] && !Q_stricmp(nextmapBuf, "map_restart 0")) {
			Q_strncpyz(arg1, "siege_restart", sizeof(arg1));
			arg2[0] = '\0';
		}
	}

	// allow e.g. /callvote ctf as an alias of /callvote g_gametype 8
	if (!Q_stricmp(arg1, "ffa")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "0", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "duel")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "3", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "powerduel")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "4", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "tffa")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "6", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "siege")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "7", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "ctf")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "8", sizeof(arg2));
	}

	// special case for g_gametype, check for bad values
	if ( !Q_stricmp( arg1, "g_gametype" ) )
	{
		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_gametype.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Voting for any gametype is disabled.\n\"" );
			return;
		}

		i = atoi( arg2 );

		if( i == GT_SINGLE_PLAYER || i < GT_FFA || i >= GT_MAX_GAME_TYPE) {
			trap_SendServerCommand( ent-g_entities, "print \"Invalid gametype.\n\"" );
			return;
		}

		//check for specific gametype
		if (!(g_allow_vote_gametype.integer & (1 << i))){
			trap_SendServerCommand( ent-g_entities, "print \"Voting for this gametype is disabled.\n\"" );
			return;
		}

		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}

		level.votingGametype = qtrue;
		level.votingGametypeTo = i;

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, gameNames[i] );
	}
	else if ( !Q_stricmp( arg1, "map" ) ) 
	{
		// special case for map changes, we want to reset the nextmap setting
		// this allows a player to change maps, but not upset the map rotation
		char	s[MAX_STRING_CHARS];
		const char    *result; //only constant strings are used
        int arenaNumber;

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_map.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote map is disabled.\n\"" );
			return;
		}

		// force map name to lowercase
		char *p = arg2;
		for (; *p; ++p)
			*p = tolower(*p);

		// redirect hoth vote to hoth3 if the server has it
		if (!Q_stricmp(arg2, "mp/siege_hoth")) {
			static qboolean serverHasHoth3 = qfalse;
			if (serverHasHoth3) {
				Q_strncpyz(arg2, "siege_hoth3_v2", sizeof(arg2));
			}
			else {
				fileHandle_t f;
				trap_FS_FOpenFile("maps/siege_hoth3_v2.bsp", &f, FS_READ);
				if (f) {
					trap_FS_FCloseFile(f);
					serverHasHoth3 = qtrue;
					Q_strncpyz(arg2, "siege_hoth3_v2", sizeof(arg2));
				}
				else {
					serverHasHoth3 = qfalse;
					static qboolean serverHasHoth2 = qfalse;
					if (serverHasHoth2) {
						Q_strncpyz(arg2, "mp/siege_hoth2", sizeof(arg2));
					}
					else {
						fileHandle_t f;
						trap_FS_FOpenFile("maps/mp/siege_hoth2.bsp", &f, FS_READ);
						if (f) {
							trap_FS_FCloseFile(f);
							serverHasHoth2 = qtrue;
							Q_strncpyz(arg2, "mp/siege_hoth2", sizeof(arg2));
						}
						else {
							serverHasHoth2 = qfalse;
						}
					}
				}
			}
		}

#if 0
		if (!Q_stricmp(arg2, "siege_urban") || !Q_stricmp(arg2, "mp/siege_urban"))
			Q_strncpyz(arg2, GetNewestMapVersion(SIEGEMAP_URBAN), sizeof(arg2));
		else if (!Q_stricmp(arg2, "siege_ansion") || !Q_stricmp(arg2, "mp/siege_ansion"))
			Q_strncpyz(arg2, GetNewestMapVersion(SIEGEMAP_ANSION), sizeof(arg2));
		else if (!Q_stricmp(arg2, "siege_cargobarge3") || !Q_stricmp(arg2, "mp/siege_cargobarge3"))
			Q_strncpyz(arg2, GetNewestMapVersion(SIEGEMAP_CARGO), sizeof(arg2));
		else if (!Q_stricmp(arg2, "siege_bespin") || !Q_stricmp(arg2, "mp/siege_bespin"))
			Q_strncpyz(arg2, GetNewestMapVersion(SIEGEMAP_BESPIN), sizeof(arg2));
		else if (!Q_stricmp(arg2, "siege_imperial") || !Q_stricmp(arg2, "mp/siege_imperial"))
			Q_strncpyz(arg2, GetNewestMapVersion(SIEGEMAP_IMPERIAL), sizeof(arg2));
#endif

		result = G_DoesMapSupportGametype(arg2, trap_Cvar_VariableIntegerValue("g_gametype"));
		if (result)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", result) );
			return;
		}

		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (*s) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"; set nextmap \"%s\"", arg1, arg2, s );
		} else {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		}
		
        arenaNumber = G_GetArenaNumber(arg2);
		arenaInfo	= G_GetArenaInfoByMap(arg2);
		if (arenaInfo)
		{
			mapName = Info_ValueForKey(arenaInfo, "longname");
		}

		if (!mapName || !mapName[0])
		{
			mapName = "ERROR";
		}   
        
        if ( (arenaNumber >= 0) && g_callvotemaplimit.integer)
        {
            if ( (g_votedCounts[arenaNumber] >= g_callvotemaplimit.integer))
            {
                trap_SendServerCommand( ent-g_entities, "print \"Map has been callvoted too many times, try another map.\n\"" );
                return;
            } 
            g_votedCounts[arenaNumber]++;
        }    
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "map %s", mapName);
	}
	else if (!Q_stricmp(arg1, "map_random"))
	{
		if (!g_allow_vote_maprandom.integer){
			trap_SendServerCommand(ent - g_entities, "print \"Vote map_random is disabled.\n\"");
			return;
		}

		// the cvar value is the amount of maps to randomize up to 5 (so 1 is the old, instant random map behavior)
		int mapsToRandomize = Com_Clampi( 1, 5, g_allow_vote_maprandom.integer );

		int total, ingame;
		CountPlayersIngame( &total, &ingame );

		if ( ingame < 2 ) {
			mapsToRandomize = 1; // not enough clients for a multi vote, just randomize 1 map right away
		}

        PoolInfo poolInfo;
        if ( !G_DBFindPool( arg2, &poolInfo ) )
        {
            trap_SendServerCommand( ent - g_entities, "print \"Pool not found.\n\"" );
            return;
        }

		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}
        
		if ( mapsToRandomize >= 2 ) {
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Randomize %d Maps: %s", mapsToRandomize, poolInfo.long_name );
		} else {
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Random Map: %s", poolInfo.long_name );
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s %d", arg1, arg2, mapsToRandomize );

		//// find the pool
		//for (i = 0; i < poolNum; ++i)
		//{
		//	if (!Q_stricmpn(pools[i].id, arg2, MAX_MAP_POOL_ID))
		//	{
		//		found = qtrue;
		//		break;
		//	}
		//}

		//if (!found)
		//{
		//	trap_SendServerCommand(ent - g_entities, "print \"Pool not found.\n\"");
		//	return;
		//}

		//Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Random Map: %s", pools[i].longname);
		//Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
	}
	else if (!Q_stricmp(arg1, "randompugmap")) {
		if (!g_allow_vote_maprandom.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote maprandom is disabled.\n\"");
			return;
		}

		if (trap_Argc() != 3 || !arg2[0]) {
			trap_SendServerCommand(ent - g_entities, "print \"Usage: callvote randompugmap <letters of map names, without spaces>\n\"");
			return;
		}

		int total, ingame;
		CountPlayersIngame(&total, &ingame);
		if (ingame < 2) {
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}
		
		int len = strlen(arg2), valid = 0;
		char eligibleMaps[MAX_PUGMAPS + 1] = { 0 }, mapsString[MAX_STRING_CHARS] = { 0 };
		for (i = 0; i < MAX_PUGMAPS && i < len; i++) {
			char possibleMap[64] = { 0 }, possibleMapPrettyName[64] = { 0 };
			if (LongMapNameFromChar(arg2[i], possibleMap, sizeof(possibleMap), possibleMapPrettyName, sizeof(possibleMapPrettyName))) {
				Q_strcat(eligibleMaps, sizeof(eligibleMaps), va("%c", arg2[i]));
				if (i > 0)
					Q_strcat(mapsString, sizeof(mapsString), ", ");
				Q_strcat(mapsString, sizeof(mapsString), possibleMapPrettyName);
				valid++;
			}
			else {
				trap_SendServerCommand(ent - g_entities, va("print \"Invalid map name '%c'.\nEligible maps: h = Hoth, n = Nar Shaddaa, c = Cargo Barge, u = Urban, b = Bespin, a = Ansion, z = Alzoc III, e = Eat Shower, d = Desert, k = Korriban\n\"", arg2[i]));
				return;
			}
		}

		if (valid < 2) {
			trap_SendServerCommand(ent - g_entities, "print \"Must specify at least two valid maps.\n\"");
			return;
		}

		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s pug map: %s", g_multiVoteRNG.integer ? "Random" : "Multivote for", mapsString);
		Com_sprintf(level.voteString, sizeof(level.voteString), "randompugmap %s", eligibleMaps);
	}
	else if (!Q_stricmp(arg1, "newpug")) {
		if (!g_allow_vote_nextpug.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote newpug is disabled.\n\"");
			return;
		}

		int total, ingame;
		CountPlayersIngame(&total, &ingame);
		if (ingame < 2) {
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}

		char maps[MAX_PUGMAPS + 1] = { 0 };
		if (arg2[0]) {
			Q_strncpyz(maps, arg2, sizeof(maps));
		}
		else {
			if (g_defaultPugMaps.string[0])
				Q_strncpyz(maps, g_defaultPugMaps.string, sizeof(maps));
			else
				Q_strncpyz(maps, "hncu", sizeof(maps));
		}

		char thisMap[64] = { 0 }, mapsString[MAX_STRING_CHARS] = { 0 };
		int len = strlen(maps);
		for (i = 0; i < MAX_PUGMAPS && i < len; i++) {
			if (LongMapNameFromChar(maps[i], NULL, 0, thisMap, sizeof(thisMap))) {
				if (i > 0)
					Q_strcat(mapsString, sizeof(mapsString), ", ");
				Q_strcat(mapsString, sizeof(mapsString), thisMap);
			}
			else {
				trap_SendServerCommand(ent - g_entities, va("print \"Invalid map '%c'; unable to call vote.\n\"", maps[i]));
				return;
			}
		}

		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "New %spug map rotation (%s)", g_multiVoteRNG.integer ? "random " : "", mapsString);
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, maps);
	}
	else if (!Q_stricmp(arg1, "nextpug")) {
		if (!g_allow_vote_nextpug.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote nextpug is disabled.\n\"");
			return;
		}

		int total, ingame;
		CountPlayersIngame(&total, &ingame);
		if (ingame < 2) {
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}

		char maps[MAX_PUGMAPS + 1] = { 0 }, eligibleMaps[MAX_PUGMAPS + 1] = { 0 }, currentMap[64];
		if (desiredPugMaps.string[0])
			Q_strncpyz(maps, desiredPugMaps.string, sizeof(maps));
		else
			Q_strncpyz(maps, "hncub", sizeof(maps));
		trap_Cvar_VariableStringBuffer("mapname", currentMap, sizeof(currentMap));
		int i, len = strlen(maps);
		for (i = 0; i < MAX_PUGMAPS && i < len; i++) {
			if (!strchr(playedPugMaps.string, maps[i])) {
				char possibleMap[64] = { 0 };
				LongMapNameFromChar(maps[i], possibleMap, sizeof(possibleMap), NULL, 0);
				if (Q_stricmp(currentMap, possibleMap))
					Q_strcat(eligibleMaps, sizeof(eligibleMaps), va("%c", maps[i]));
			}
		}
		len = strlen(eligibleMaps);
		if (!len) { // all maps have been played
			trap_SendServerCommand(ent - g_entities, "print \"All maps have been played; call a newpug vote to start a new pug map rotation.\n\"");
			return;
		}
		else if (len == 1) { // only one map left
			char fileName[64] = { 0 }, prettyName[64] = { 0 };
			if (LongMapNameFromChar(eligibleMaps[0], fileName, sizeof(fileName), prettyName, sizeof(prettyName))) {
				Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Final pug map (%s)", prettyName);
				Com_sprintf(level.voteString, sizeof(level.voteString), arg1);
			}
			else {
				trap_SendServerCommand(ent - g_entities, va("print \"Invalid map '%c'; unable to call vote.\n\"", eligibleMaps[0]));
				return;
			}
		}
		else { // multiple maps are eligible
			char mapsString[MAX_STRING_CHARS] = { 0 };
			for (i = 0; i < MAX_PUGMAPS && i < len; i++) {
				char thisMap[64];
				if (LongMapNameFromChar(eligibleMaps[i], NULL, 0, thisMap, sizeof(thisMap))) {
					if (i > 0)
						Q_strcat(mapsString, sizeof(mapsString), ", ");
					Q_strcat(mapsString, sizeof(mapsString), thisMap);
				}
				else {
					trap_SendServerCommand(ent - g_entities, va("print \"Invalid map '%c'; unable to call vote.\n\"", eligibleMaps[i]));
					return;
				}
			}
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Next %spug map (%s)", g_multiVoteRNG.integer ? "random " : "", mapsString);
			Com_sprintf(level.voteString, sizeof(level.voteString), arg1);
		}
	}
	else if ( !Q_stricmp ( arg1, "clientkick" ) )
	{
		int n = atoi ( arg2 );

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_kick.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote kick is disabled.\n\"" );
			return;
		}

		if ( n < 0 || n >= MAX_CLIENTS )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Invalid client number %d.\n\"", n ) );
			return;
		}

		if ( g_entities[n].client->pers.connected == CON_DISCONNECTED )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"There is no client with the client number %d.\n\"", n ) );
			return;
		}
			
		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}

		Com_sprintf ( level.voteString, sizeof(level.voteString ), "%s %i", arg1, n );
		Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s%s", NM_SerializeUIntToColor(n), g_entities[n].client->pers.netname );
	}
	else if ( !Q_stricmp ( arg1, "kick" ) )
	{
		int clientid = G_ClientNumberFromName ( arg2 );

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_kick.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote kick is disabled.\n\"" );
			return;
		}

		if ( clientid == -1 )
		{
			clientid = G_ClientNumberFromStrippedName(arg2);

			if (clientid == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"there is no client named '%s' currently on the server.\n\"", arg2 ) );
				return;
			}
		}

		Com_sprintf ( level.voteString, sizeof(level.voteString ), "clientkick %d", clientid );
		Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s%s", NM_SerializeUIntToColor(clientid), g_entities[clientid].client->pers.netname );
	}
	else if ( !Q_stricmp( arg1, "nextmap" ) ) 
	{
		char	s[MAX_STRING_CHARS];

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_nextmap.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote nextmap is disabled.\n\"" );
			return;
		}

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (!*s) {
			trap_SendServerCommand( ent-g_entities, "print \"nextmap not set.\n\"" );
			return;
		}
		extern siegePers_t g_siegePersistant;
		if (g_gametype.integer == GT_SIEGE && g_siegeTeamSwitch.integer && g_siegePersistant.beatingTime && g_nextmapWarning.integer)
		{
			trap_SendServerCommand(-1, va("cp \"Currently round 2,\nvote will reset timer going up\n\""));
			trap_SendServerCommand(-1, va("print \"Currently round 2, vote will reset timer going up\n\""));
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap");
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	} 
	else if ( !Q_stricmp( arg1, "timelimit" )) 
	{
		int n = atoi(arg2);

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_timelimit.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote timelimit is disabled.\n\"" );
			return;
		}

		if (n < 0)
			n = 0;

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%i\"", arg1, n );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );

	} 
	else if ( !Q_stricmp( arg1, "fraglimit" )) 
	{
		int n = atoi(arg2);

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_fraglimit.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote fraglimit is disabled.\n\"" );
			return;
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );

	} 
	else if ( !Q_stricmp( arg1, "map_restart" )) 
	{
		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_restart.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote map_restart is disabled.\n\"" );
			return;
		}
		
		const int n = Com_Clampi(0, 10, g_restart_countdown.integer);

		Com_sprintf(level.voteString, sizeof(level.voteString), "%s \"%i\"", arg1, n);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);

	} 
	else if (!Q_stricmp(arg1, "siege_restart"))
	{
		if (g_gametype.integer != GT_SIEGE) {
			trap_SendServerCommand(ent - g_entities, "print \"Not in siege gametype.\n\"");
			return;
		}

	// *CHANGE 22* - vote disabling
	if (!g_allow_vote_restart.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"Vote map_restart is disabled.\n\"");
		return;
	}

	extern siegePers_t g_siegePersistant;
	if (g_siegeTeamSwitch.integer && g_siegePersistant.beatingTime && g_nextmapWarning.integer) {
		Q_strncpyz(level.voteString, "siege_restart", sizeof(level.voteString));
		Q_strncpyz(level.voteDisplayString, "Restart Match in Round 1", sizeof(level.voteDisplayString));
	}
	else {
		Q_strncpyz(level.voteString, "map_restart \"0\"", sizeof(level.voteString));
		Q_strncpyz(level.voteDisplayString, "map_restart \"0\"", sizeof(level.voteDisplayString));
	}
	}
	else if (!Q_stricmp(arg1, "g_redTeam"))
	{
		if (!g_allow_vote_customTeams.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Custom teams is disabled.\n\"");
			return;
		}
		if (trap_Argc() != 3) {
			trap_SendServerCommand(ent - g_entities, va("print \"usage: g_redTeam <teamName> (e.g. 'g_redTeam Siege3_Jedi')\n\""));
			trap_SendServerCommand(ent - g_entities, va("print \"Use 'g_redTeam 0' or 'g_redTeam none' to reset classes.\n\""));
			return;
		}
		if (!Q_stricmp(arg2, "none") || !Q_stricmp(arg2, "0")) //reset the classes
		{
			Com_sprintf(level.voteString, sizeof(level.voteString), "g_redTeam none");
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Reset ^1Red^7 Team Classes");
		}
		else
		{
			if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
				return;
			}
			Com_sprintf(level.voteString, sizeof(level.voteString), "g_redTeam %s", arg2);
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Custom ^1Red^7 Team Classes: %s", arg2);
		}
	}
	else if (!Q_stricmp(arg1, "g_blueTeam"))
	{
		if (!g_allow_vote_customTeams.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Custom teams is disabled.\n\"");
			return;
		}
		if (trap_Argc() != 3) {
			trap_SendServerCommand(ent - g_entities, va("print \"usage: g_blueTeam <teamName> (e.g. 'g_blueTeam Siege3_DarkJedi')\n\""));
			trap_SendServerCommand(ent - g_entities, va("print \"Use 'g_blueTeam 0' or 'g_blueTeam none' to reset classes.\n\""));
			return;
		}
		if (!Q_stricmp(arg2, "none") || !Q_stricmp(arg2, "0")) //reset the classes
		{
			Com_sprintf(level.voteString, sizeof(level.voteString), "g_blueTeam none");
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Reset ^4Blue^7 Team Classes");
		}
		else
		{
			if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
				return;
			}
			Com_sprintf(level.voteString, sizeof(level.voteString), "g_blueTeam %s", arg2);
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Custom ^4Blue^7 Team Classes: %s", arg2);
		}
	}
	else if (!Q_stricmp(arg1, "cointoss"))
	{
		//disable this vote
		if (!g_allow_vote_cointoss.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Coin toss is disabled.\n\"");
			return;
		}
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Coin Toss");
	}
    else if (!Q_stricmp(arg1, "randomcapts"))
    {
		//disable this vote
		if (!g_allow_vote_randomcapts.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Random capts is disabled.\n\"");
			return;
		}
        Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Random capts");
    }
    else if (!Q_stricmp(arg1, "randomteams") || !Q_stricmp(arg1, "shuffleteams"))
    {
		qboolean shuffle = tolower(*arg1) == 's' ? qtrue : qfalse;

		//disable this vote
		if (!g_allow_vote_randomteams.integer) {
			trap_SendServerCommand(ent - g_entities, va("print \"%s teams is disabled.\n\"", shuffle ? "Shuffle" : "Random"));
			return;
		}

		// make sure this person isn't a troublemaker
		for (i = 2; i <= trap_Argc(); i++) {
			char arg[32];
			trap_Argv(i, arg, sizeof(arg));
			if (strchr(arg, ';')) {
				trap_SendServerCommand(ent - g_entities, "print \"Sorry, semicolons are not allowed.\n\"");
				trap_SendServerCommand(ent - g_entities, va("print \"Usage: callvote %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n\"", shuffle ? "shuffle" : "random"));
				return;
			}
			else if (strchr(arg, '\n'))
				return;
			else if (strchr(arg, '\r'))
				return;
		}

		// get the number of players we want on each team
        int args = trap_Argc() - 2, team1Count = 0, team2Count = 0;
		if (args >= 2) {
			char count[2];
			trap_Argv(2, count, sizeof(count));
			team1Count = atoi(count);
			trap_Argv(3, count, sizeof(count));
			team2Count = atoi(count);
		}
		else if (shuffle) { // no arguments; get numbers based on current ingame player counts
			int total = 0;
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].pers.connected == CON_DISCONNECTED)
					continue;
				if (level.clients[i].sess.sessionTeam == TEAM_RED || level.clients[i].sess.sessionTeam == TEAM_BLUE)
					total++;
			}
			team1Count = team2Count = (total / 2); // e.g. 9 players in game will result in a 4v4 vote
		}
		if (team1Count <= 0 || team2Count <= 0) {
			trap_SendServerCommand(ent - g_entities, "print \"Both teams need at least 1 player.\n\"");
			trap_SendServerCommand(ent - g_entities, va("print \"Usage: callvote %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n\"", shuffle ? "shuffle" : "random"));
			return;
		}
		else if (team1Count > MAX_CLIENTS || team2Count > MAX_CLIENTS) {
			return;
		}

		if (args >= 3 && g_allow_vote_randomteams.integer < 2) {
			trap_SendServerCommand(ent - g_entities, va("print \"%s teams with separating players is disabled.\n\"", shuffle ? "Shuffle" : "Random"));
			trap_SendServerCommand(ent - g_entities, va("print \"Usage: callvote %steams <# red players> <# blue players>\n\"", shuffle ? "shuffle" : "random"));
			return;
		}

#define MAX_RANDOMTEAMS_VOTE_PAIRS	8
		if (args < 4) { // if they didn't want to separate some players, then we're done here
			Com_sprintf(level.voteString, sizeof(level.voteString), "%s %d %d", shuffle ? "shuffleteams" : "randomteams", team1Count, team2Count);
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s Teams - %d vs %d", shuffle ? "Shuffle" : "Random", team1Count, team2Count);
		}
		else {
			if (args > (2 + (MAX_RANDOMTEAMS_VOTE_PAIRS * 2))) { // allow max of 8 pairs, i guess
				trap_SendServerCommand(ent - g_entities, "print \"Too many arguments.\n\"");
				return;
			}

			char msg[4096] = { 0 };
			Q_strncpyz(msg, va("%s Teams - %d vs %d, separating", shuffle ? "Shuffle" : "Random", team1Count, team2Count), sizeof(msg));
			if (args <= 6)
				Q_strcat(msg, sizeof(msg), " ");
			else if (args > 6)
				Q_strcat(msg, sizeof(msg), ":\n");
			int pairs[MAX_RANDOMTEAMS_VOTE_PAIRS][2];
			memset(&pairs, 0llu, sizeof(pairs));
			for (i = 0; i < MAX_RANDOMTEAMS_VOTE_PAIRS * 2 && args >= i + 4; i += 2) {
				// get the first guy in this pair
				char name[2][MAX_NAME_LENGTH];
				trap_Argv(i + 4, name[0], sizeof(name[0]));
				gentity_t *firstGuy = G_FindClient(name[0]);
				if (!firstGuy) {
					trap_SendServerCommand(ent - g_entities, va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"", name[0]));
					trap_SendServerCommand(ent - g_entities, va("print \"Usage: callvote %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n\"", shuffle ? "shuffle" : "random"));
					return;
				}

				// get the second guy in this pair
				trap_Argv(i + 5, name[1], sizeof(name[1]));
				gentity_t *secondGuy = G_FindClient(name[1]);
				if (!secondGuy) {
					trap_SendServerCommand(ent - g_entities, va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"", name[1]));
					trap_SendServerCommand(ent - g_entities, va("print \"Usage: callvote %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n\"", shuffle ? "shuffle" : "random"));
					return;
				}
				else if (secondGuy == firstGuy) {
					trap_SendServerCommand(ent - g_entities, va("print \"Client %s"S_COLOR_WHITE" is the same person as client %s"S_COLOR_WHITE".\n\"", name[1], name[0]));
					trap_SendServerCommand(ent - g_entities, va("print \"Usage: callvote %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n\"", shuffle ? "shuffle" : "random"));
					return;
				}
				else if (i > 0) { // check that this pair hasn't already been used earlier in the vote call
					for (int j = 0; j < i / 2; j++) {
						if ((pairs[j][0] == firstGuy - g_entities && pairs[j][1] == secondGuy - g_entities) ||
							(pairs[j][0] == secondGuy - g_entities && pairs[j][1] == firstGuy - g_entities)) {
							trap_SendServerCommand(ent - g_entities, "print \"You entered the same pair of players more than once.\n\"");
							trap_SendServerCommand(ent - g_entities, va("print \"Usage: callvote %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n\"", shuffle ? "shuffle" : "random"));
							return;
						}
					}
				}
				if (args >= 6 && i > 0)
					Q_strcat(msg, sizeof(msg), "\n");
				Q_strcat(msg, sizeof(msg), va("%s"S_COLOR_WHITE" and %s"S_COLOR_WHITE, firstGuy->client->pers.netname, secondGuy->client->pers.netname));
				pairs[i / 2][0] = firstGuy - g_entities;
				pairs[i / 2][1] = secondGuy - g_entities;
			}
			Q_strncpyz(level.voteString, ConcatArgs(1), sizeof(level.voteString));
			Q_strncpyz(level.voteDisplayString, msg, sizeof(level.voteDisplayString));
		}
    }
	else if (!Q_stricmp(arg1, "forceround2"))
	{
		char count[32];
		int mins = 0;
		int secs = 0;
		int time;
		if (g_gametype.integer != GT_SIEGE)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Must be in siege gametype.\n\"");
			return;
		}
		trap_Argv(2, count, sizeof(count));
		if (!count[0]) {
			trap_SendServerCommand(ent - g_entities, "print \"Usage: forceround2 <mm:ss> (e.g. 'forceround2 7:30' for 7 minutes, 30 seconds)\n\"");
			return;
		}
		if (!atoi(count) || atoi(count) <= 0 || !(strstr(count, ":") != NULL) || strstr(count, ".") != NULL || strstr(count, "-") != NULL || strstr(count, ",") != NULL)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Usage: forceround2 <mm:ss> (e.g. 'forceround2 7:30' for 7 minutes, 30 seconds)\n\"");
			return;
		}
		sscanf(count, "%d:%d", &mins, &secs);
		time = (mins * 60) + secs;
		if (!mins || secs > 59 || secs < 0 || mins < 0 || time <= 0)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Invalid time.\n\"");
			return;
		}
		if (time > 3600)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Time must be under 60 minutes.\n\"");
			return;
		}
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Force round 2 siege with %i:%i timer", mins, secs);
	}
	else if (!Q_stricmp(arg1, "killturrets"))
	{
		//disable this vote
		if (!g_allow_vote_killturrets.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Kill turrets is disabled.\n\"");
			return;
		}
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Kill Turrets");

	}
	else if (!Q_stricmpn(arg1, "quickspawn", 10) || !Q_stricmpn(arg1, "quickrespawn", 12) ||
	!Q_stricmpn(arg1, "fastspawn", 9) || !Q_stricmpn(arg1, "fastrespawn", 11) || !Q_stricmpn(arg1, "respawn", 7)) {
	//disable this vote
	if (!g_allow_vote_quickSpawns.integer || g_gametype.integer != GT_SIEGE) {
		trap_SendServerCommand(ent - g_entities, "print \"Quick spawns is disabled.\n\"");
		return;
	}
	if (!g_siegeRespawn.integer || g_siegeRespawn.integer == 1) {
		trap_SendServerCommand(ent - g_entities, "print \"Quick spawns are already enabled.\n\"");
		return;
	}
	Com_sprintf(level.voteString, sizeof(level.voteString), "g_siegeRespawn 1");
	Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Quick Spawns");
	}
	else if (!Q_stricmp(arg1, "zombies"))
	{
		//disable this vote
		if (!g_allow_vote_zombies.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Zombies is disabled.\n\"");
			return;
		}
		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}
		Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), level.zombies ? "Disable Zombies Mode" : "Enable Zombies Mode");

	}
	else if (!Q_stricmp(arg1, "pug"))
	{
		//disable this vote
		if (!g_allow_vote_pug.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Pug vote is disabled.\n\"");
			return;
		}
		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}
		Com_sprintf(level.voteString, sizeof(level.voteString), "exec pug.cfg", arg1);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Pug Server Mode");

	}
	else if (!Q_stricmp(arg1, "pub"))
	{
		//disable this vote
		if (!g_allow_vote_pub.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Pub vote is disabled.\n\"");
			return;
		}
		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}
		Com_sprintf(level.voteString, sizeof(level.voteString), "exec pub.cfg", arg1);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Public Server Mode");

	}
	else if ( !Q_stricmp( arg1, "resetflags" )) 
	{
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", arg1 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Reset Flags" );
	}
	else if ( !Q_stricmp( arg1, "q" )) 
	{
		if (!g_allow_vote_q.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Ask question is disabled.\n\"");
			return;
		}

		if (!arg2[0] || isspace(arg2[0])) {
			trap_SendServerCommand(ent - g_entities, "print \"You must specify a poll question.\n\"");
			return;
		}

		char		*questionstring;
		questionstring = ConcatArgs(2);
		if (strlen(questionstring) > 234)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Question is too long.\n\"");
			return;
		}
		int i;
		int length = strlen(questionstring);
		for (i = 0; i < length; i++)
		{
			if (questionstring[i] == '\t' || questionstring[i] == '\n' || questionstring[i] == '\v' || questionstring[i] == '\f' || questionstring[i] == '\r')
			{
				G_HackLog("Client from %s is sending an illegal poll question.\n", ent->client->sess.ipString);
				questionstring[i] = ' ';
			}
		}
		char normalizedPoll[256] = { 0 };
		NormalizeName(questionstring, normalizedPoll, sizeof(normalizedPoll), sizeof(normalizedPoll));
		PurgeStringedTrolling(normalizedPoll, normalizedPoll, sizeof(normalizedPoll));
		Com_sprintf( level.voteString, sizeof( level.voteString ), "svsay Poll Result ^2YES^7: %s", normalizedPoll );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Poll: %s", normalizedPoll );
		trap_SendServerCommand(-1, va("print \"Poll: %s\n\"", normalizedPoll));
	} 
	else if ( !Q_stricmp( arg1, "pause" )) 
	{
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s 300", arg1);
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Pause Game" );

	}
	else if ( !Q_stricmp( arg1, "unpause" )) 
	{
		if (level.pause.state == PAUSE_NONE) {
			trap_SendServerCommand(ent - g_entities, "print \"The game is not currently paused.\n\"");
			return;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", arg1 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Unpause Game" );
	}
	else if ( !Q_stricmp( arg1, "endmatch" )) 
	{
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", arg1 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "End Match" );
	}
	else if ( !Q_stricmp( arg1, "lockteams" ) )
	{
		//disable this vote
		if (!g_allow_vote_lockteams.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Lock teams vote is disabled.\n\"");
			return;
		}
		if (g_antiCallvoteTakeover.integer && TryingToDoCallvoteTakeover(ent) == qtrue)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"At least two players must be in-game to call this vote.\n\""));
			return;
		}
		// hacky param whitelist but we aren't going to do any parsing anyway
		if ( argc >= 3 && ( !Q_stricmp( arg2, "0" ) || !Q_stricmpn( arg2, "r", 1 )
			|| !Q_stricmp(arg2, "1s") || !Q_stricmp(arg2, "2s") || !Q_stricmp(arg2, "3s") || !Q_stricmp( arg2, "4s" ) || !Q_stricmp( arg2, "5s" ) || !Q_stricmp(arg2, "6s") || !Q_stricmp(arg2, "7s")) ) { //i'm lazy, fuck off.
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
			
			if ( !Q_stricmp( arg2, "0" ) || !Q_stricmpn( arg2, "r", 1 ) ) {
				Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Unlock Teams" );
			} else {
				Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Lock Teams - %s", arg2 );
			}
		} else {
			trap_SendServerCommand( ent - g_entities, "print \"usage: /callvote lockteams 1s/2s/3s/4s/5s/6s/7s/reset\n\"" );
			return;
		}
	}
	else if (!Q_stricmp(arg1, "g_doWarmup"))
	{
		int n = atoi(arg2);

        if (!g_allow_vote_warmup.integer){
            trap_SendServerCommand(ent - g_entities, "print \"Vote warmup is disabled.\n\"");
            return;
        }                

		Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), n ? "Enable Warmup" : "Disable Warmup");
	}
	else
	{
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	}

	trap_SendServerCommand( -1, va("print \"%s%s^7 %s\n\"", NM_SerializeUIntToColor(ent - g_entities), ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLCALLEDVOTE")) );

	// log the vote
	G_LogPrintf("Client %i (%s) called a vote: %s %s\n",
		ent-g_entities, ent->client->pers.netname, arg1, arg2);

	// start the voting, the caller autoamtically votes yes
	level.voteTime = level.time;
	level.voteYes = 1;
	level.voteNo = 0;
	level.lastVotingClient = ent-g_entities;
	level.multiVoting = qfalse;
	level.inRunoff = qfalse;
	level.multiVoteChoices = 0;
	memset( &( level.multiVotes ), 0, sizeof( level.multiVotes ) );

	fixVoters();

	ent->client->mGameFlags |= PSG_VOTED;

	trap_SetConfigstring( CS_VOTE_TIME, va("%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );	
	trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );	
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent ) {
	char		msg[64];

	if (g_lockdown.integer && !G_ClientIsWhitelisted(ent - g_entities)) {
		return;
	}

	if (g_probation.integer && G_ClientIsOnProbation(ent - g_entities)) {
		trap_SendServerCommand(ent - g_entities, va("print \"You are on probation and cannot vote.\n\""));
		return;
	}

	if (!level.voteTime && ent->client->sess.sessionTeam == TEAM_RED && level.teamVoteTime[0]) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")));
		trap_SendServerCommand(ent - g_entities, "print \"Use ^5/teamvote [yes/no]^7 to vote on teamvotes.\n\"");
		return;
	}
	if (!level.voteTime && ent->client->sess.sessionTeam == TEAM_BLUE && level.teamVoteTime[1]) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")));
		trap_SendServerCommand(ent - g_entities, "print \"Use ^5/teamvote [yes/no]^7 to vote on teamvotes.\n\"");
		return;
	}
	if ( !level.voteTime ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_VOTED ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEALREADY")) );
		return;
	}
	if ( !(ent->client->mGameFlags & PSG_CANVOTE) ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", "You were not in-game when the vote was called. You cannot vote.") );
		return;
	}

	if (g_gametype.integer != GT_DUEL &&
		g_gametype.integer != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
			return;
		}
	}

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( !level.multiVoting ) {
		// not a special multi vote, use legacy behavior
		if ( tolower(msg[0]) == 'y' ) {
			G_LogPrintf( "Client %i (%s) voted YES\n", ent - g_entities, ent->client->pers.netname );
			level.voteYes++;
			trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
		} else if ( tolower(msg[0]) == 'n' ) {
			G_LogPrintf( "Client %i (%s) voted NO\n", ent - g_entities, ent->client->pers.netname );
			level.voteNo++;
			trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
		} else {
			trap_SendServerCommand(ent - g_entities, "print \"Invalid choice, please use /vote y or /vote n.\n\"");
			return;
		}
	} else {
		// multi map vote, only allow voting for valid choice ids
		int voteId = -1;
		char *choiceStr;
		if (isdigit(*msg)) { // number
			char choiceBuf[MAX_QPATH] = { 0 };
			Q_strncpyz(choiceBuf, va("%c", *msg), sizeof(choiceBuf));
			choiceStr = choiceBuf;
			int i;
			for (i = 0; i < MAX_PUGMAPS; i++) {
				if (*msg == level.multiVoteMapChars[i]) {
					voteId = i + 1;
					break;
				}
			}
		}
		else if (tolower(*msg) >= 'a' && tolower(*msg) <= 'z') { // letter representing a map name
			char prettyName[MAX_QPATH] = { 0 };
			if (LongMapNameFromChar(tolower(*msg), NULL, 0, prettyName, sizeof(prettyName))) {
				choiceStr = prettyName;
				int i;
				for (i = 0; i < MAX_PUGMAPS; i++) {
					if (tolower(*msg) == tolower(level.multiVoteMapChars[i])) {
						voteId = i + 1;
						break;
					}
				}
			}
			else {
				voteId = -1; // this will make it invalid below
			}
		}

		if ( voteId <= 0 || voteId > level.multiVoteChoices ) {
			qboolean thereIsANumberMap = qfalse;
			int i;
			for (i = 0; i < MAX_PUGMAPS; i++) {
				if (level.multiVoteMapChars[i] >= '0' && level.multiVoteMapChars[i] <= '9') {
					thereIsANumberMap = qtrue;
					break;
				}
			}
			if (thereIsANumberMap)
				trap_SendServerCommand( ent - g_entities, va( "print \"Invalid choice, please use /vote [letter] or /vote [number from 1-%d] in the console\n\"", level.multiVoteChoices ) );
			else
				trap_SendServerCommand(ent - g_entities, va("print \"Invalid choice, please use /vote [letter] in the console\n\"", level.multiVoteChoices));
			return;
		}

		// we maintain an internal array of choice ids, and only use voteYes to show how many people voted
		G_LogPrintf( "Client %i (%s) voted for choice %s\n", ent - g_entities, ent->client->pers.netname, choiceStr );
		level.multiVotes[ent - g_entities] = voteId;
		level.voteYes++;
		trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
	}

	trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "PLVOTECAST" ) ) );
	ent->client->mGameFlags |= PSG_VOTED;

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

static void Cmd_Ready_f(gentity_t *ent) {
	const char *publicMsg = NULL;

	if (!ent || !ent->client)
	{
		//???
		return;
	}

	if (g_lockdown.integer && !G_ClientIsWhitelisted(ent - g_entities))
		return;

	if (!g_allow_ready.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"Ready is disabled.\n\"");
		return;
	}

	if (level.restarted)
		return;

	if (ent->client->pers.readyTime > level.time - 2000)
		return;

	if (!ent->client->sess.canJoin) {
		trap_SendServerCommand(ent->client->ps.clientNum,
			va("cp \"^7You may not join due to incorrect/missing password\n^7If you know the password, just use /password\n\""));
		trap_SendServerCommand(ent - g_entities,
			"print \"^7You may not join due to incorrect/missing password\n^7If you know the password, just use /password\n\"");
		return;
	}

	// if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
    //     return;

	G_ChangePlayerReadiness(ent->client, !ent->client->pers.ready, qtrue);
}


/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f(gentity_t *ent) {
	int		i, team, cs_offset;
	static char	arg1[MAX_STRING_TOKENS];
	static char	arg2[MAX_STRING_TOKENS];
	char buffer[64];
	gentity_t* found = NULL;
	char		enteredClass[MAX_STRING_TOKENS];
	char		desiredClassLetter[16];
	char		desiredClassName[16];

	team = ent->client->sess.sessionTeam;
	if (team == TEAM_RED)
		cs_offset = 0;
	else if (team == TEAM_BLUE)
		cs_offset = 1;
	else
		return;

	if (g_lockdown.integer && !G_ClientIsWhitelisted(ent - g_entities)) {
		return;
	}

	if (g_probation.integer && G_ClientIsOnProbation(ent - g_entities)) {
		trap_SendServerCommand(ent - g_entities, va("print \"You are on probation and cannot call teamvotes.\n\""));
		return;
	}

	if (!g_allowVote.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")));
		return;
	}

	if (!g_allow_vote_forceclass.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"Forceclass vote is disabled.\n\"");
		return;
	}

	if (level.teamVoteTime[cs_offset]) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADY")));
		return;
	}
	if (ent->client->pers.teamVoteCount >= MAX_VOTE_COUNT) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MAXTEAMVOTES")));
		return;
	}
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")));
		return;
	}

	// make sure it is a valid command to vote on
	trap_Argv(1, arg1, sizeof(arg1));
	arg2[0] = '\0';
	for (i = 2; i < trap_Argc(); i++) {
		if (i > 2)
			strcat(arg2, " ");
		trap_Argv(i, &arg2[strlen(arg2)], sizeof(arg2) - strlen(arg2));
	}

	// *CHANGE 8b* anti callvote bug
	if ((g_protectCallvoteHack.integer && (strchr(arg1, '\n') || strchr(arg2, '\n') || strchr(arg1, '\r') || strchr(arg2, '\r')))) {
		//lets replace line breaks with ; for better readability
		int len;
		for (len = 0; len < (int)strlen(arg1); ++len)
			if (arg1[len] == '\n' || arg1[len] == '\r') arg1[len] = ';';
		for (len = 0; len < (int)strlen(arg2); ++len)
			if (arg2[len] == '\n' || arg2[len] == '\r') arg2[len] = ';';

		G_HackLog("Callvote hack: Client num %d (%s) from %s tries to hack via callteamvote (callteamvote %s \"%s\").\n",
			ent->client->pers.clientNum,
			ent->client->pers.netname,
			ent->client->sess.ipString,
			arg1, arg2);
		trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
		return;
	}

	if (strchr(arg1, ';') || strchr(arg2, ';')) {
		trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
		return;
	}
#if 0
	if (!Q_stricmp(arg1, "leader")) {
		char netname[MAX_NETNAME], leader[MAX_NETNAME];

		if (!arg2[0]) {
			i = ent->client->ps.clientNum;
		}
		else {
			// numeric values are just slot numbers
			for (i = 0; i < 3; i++) {
				if (!arg2[i] || arg2[i] < '0' || arg2[i] > '9')
					break;
			}
			if (i >= 3 || !arg2[i]) {
				i = atoi(arg2);
				if (i < 0 || i >= level.maxclients) {
					trap_SendServerCommand(ent - g_entities, va("print \"Bad client slot: %i\n\"", i));
					return;
				}

				if (!g_entities[i].inuse) {
					trap_SendServerCommand(ent - g_entities, va("print \"Client %i is not active\n\"", i));
					return;
				}
			}
			else {
				Q_strncpyz(leader, arg2, sizeof(leader));
				Q_CleanStr(leader);
				for (i = 0; i < level.maxclients; i++) {
					if (level.clients[i].pers.connected == CON_DISCONNECTED)
						continue;
					if (level.clients[i].sess.sessionTeam != team)
						continue;
					Q_strncpyz(netname, level.clients[i].pers.netname, sizeof(netname));
					Q_CleanStr(netname);
					if (!Q_stricmp(netname, leader)) {
						break;
					}
				}
				if (i >= level.maxclients) {
					trap_SendServerCommand(ent - g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2));
					return;
				}
			}
		}
		Com_sprintf(arg2, sizeof(arg2), "%d", i);
	}
#endif
	if (!Q_stricmp(arg1, "forceclass"))
	{
		if (g_gametype.integer != GT_SIEGE)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Must be in siege gametype.\n\"");
			return;
		}

		if (trap_Argc() < 3)
		{
			trap_SendServerCommand(ent - g_entities, "print \"usage: forceclass [name or client number] [first letter of class]\n\""); //bad number of arguments
			return;
		}

		trap_Argv(2, buffer, sizeof(buffer));
		found = G_FindTeammateClient(buffer, ent);

		if (!found || !found->client)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"", buffer));
			return;
		}
		if (ent->client->sess.sessionTeam == TEAM_RED && !found->client->sess.sessionTeam == TEAM_RED)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Target player must be on your team.\n\"");
			return;
		}
		else if (found->client->sess.sessionTeam == TEAM_BLUE && !found->client->sess.sessionTeam == TEAM_BLUE)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Target player must be on your team.\n\"");
			return;
		}
		trap_Argv(3, enteredClass, sizeof(enteredClass));
		if (!Q_stricmp(enteredClass, "a") || !Q_stricmp(enteredClass, "h") || !Q_stricmp(enteredClass, "t") || !Q_stricmp(enteredClass, "d") || !Q_stricmp(enteredClass, "j") || !Q_stricmp(enteredClass, "s"))
		{
			strcpy(desiredClassLetter, enteredClass);
			if (!Q_stricmp(desiredClassLetter, "a") || !Q_stricmp(desiredClassLetter, "assault"))
				Com_sprintf(desiredClassName, sizeof(desiredClassName), "assault");
			else if (!Q_stricmp(desiredClassLetter, "h") || !Q_stricmp(desiredClassLetter, "hw"))
				Com_sprintf(desiredClassName, sizeof(desiredClassName), "HW");
			else if (!Q_stricmp(desiredClassLetter, "t") || !Q_stricmp(desiredClassLetter, "tech"))
				Com_sprintf(desiredClassName, sizeof(desiredClassName), "tech");
			else if (!Q_stricmp(desiredClassLetter, "d") || !Q_stricmp(desiredClassLetter, "demo"))
				Com_sprintf(desiredClassName, sizeof(desiredClassName), "demo");
			else if (!Q_stricmp(desiredClassLetter, "j") || !Q_stricmp(desiredClassLetter, "jedi"))
				Com_sprintf(desiredClassName, sizeof(desiredClassName), "jedi");
			else if (!Q_stricmp(desiredClassLetter, "s") || !Q_stricmp(desiredClassLetter, "scout"))
				Com_sprintf(desiredClassName, sizeof(desiredClassName), "scout");
		}
		else
		{
			trap_SendServerCommand(ent - g_entities, "print \"usage: forceclass [name or client number] [first letter of class]\n\"");
			return;
		}
	}
	else if (!Q_stricmp(arg1, "unforceclass"))
	{
		if (g_gametype.integer != GT_SIEGE)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Must be in siege gametype.\n\"");
			return;
		}

		if (trap_Argc() < 2)
		{
			trap_SendServerCommand(ent - g_entities, "print \"usage: unforceclass [name or client number]\n\""); //bad number of arguments
			return;
		}

		trap_Argv(2, buffer, sizeof(buffer));
		found = G_FindTeammateClient(buffer, ent);

		if (!found || !found->client)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"", buffer));
			return;
		}
		if (ent->client->sess.sessionTeam == TEAM_RED && found->client->sess.sessionTeam != TEAM_RED)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Target player must be on your team.\n\"");
			return;
		}
		else if (found->client->sess.sessionTeam == TEAM_BLUE && found->client->sess.sessionTeam != TEAM_BLUE)
		{
			trap_SendServerCommand(ent - g_entities, "print \"Target player must be on your team.\n\"");
			return;
		}
	}
	else
	{
		trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
		trap_SendServerCommand(ent - g_entities, "print \"Team vote commands are: forceclass <player> <class>, unforceclass <player>.\n\"");
		return;
	}
	if (!Q_stricmp(arg1, "forceclass"))
	{
		Com_sprintf(level.teamVoteString[cs_offset], sizeof(level.teamVoteString[cs_offset]), "Force to %s: %s", desiredClassName, found->client->pers.netname);
	}
	else if (!Q_stricmp(arg1, "unforceclass"))
	{
		Com_sprintf(level.teamVoteString[cs_offset], sizeof(level.teamVoteString[cs_offset]), "^1Un^7forceclass: %s", found->client->pers.netname);
	}

	for (i = 0; i < level.maxclients; i++)
	{
		if (level.clients[i].pers.connected == CON_DISCONNECTED)
			continue;
		if (level.clients[i].sess.sessionTeam == team)
			trap_SendServerCommand(i, va("print \"%s called a team vote.\n\"", ent->client->pers.netname));
	}

	// start the voting, the caller autoamtically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	if (!Q_stricmp(arg1, "forceclass"))
	{
		Com_sprintf(level.teamVoteCommand[cs_offset], sizeof(level.teamVoteCommand[cs_offset]), "forceclass %i %s", found->s.number, desiredClassLetter);
	}
	else if (!Q_stricmp(arg1, "unforceclass"))
	{
		Com_sprintf(level.teamVoteCommand[cs_offset], sizeof(level.teamVoteCommand[cs_offset]), "unforceclass %i", found->s.number);
	}

	fixTeamVoters(ent);

	trap_SetConfigstring(CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset]));
	trap_SetConfigstring(CS_TEAMVOTE_STRING + cs_offset, level.teamVoteString[cs_offset]);
	trap_SetConfigstring(CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset]));
	trap_SetConfigstring(CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset]));

	if (g_teamVoteFix.integer)
	{
		G_TeamCommand(team, va("cp \"%s called a team vote.\n\"", ent->client->pers.netname));
		G_TeamCommand(team, va("print \"Team Vote: %s <Yes: %i  No: %i>\n\"", level.teamVoteString[cs_offset], level.teamVoteYes[cs_offset], level.teamVoteNo[cs_offset]));
	}
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent ) {
	int			team, cs_offset;
	char		msg[64];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if (g_lockdown.integer && !G_ClientIsWhitelisted(ent - g_entities)) {
		return;
	}

	if (g_probation.integer && G_ClientIsOnProbation(ent - g_entities)) {
		trap_SendServerCommand(ent - g_entities, va("print \"You are on probation and cannot teamvote.\n\""));
		return;
	}

	if ( !level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOTEAMVOTEINPROG")) );
		return;
	}
	if (ent->client->sess.sessionTeam == TEAM_RED && !(ent->client->mGameFlags & PSG_CANTEAMVOTERED)) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", "You were not in-game and in this team when the vote was called. You cannot vote."));
		return;
	}
	if (ent->client->sess.sessionTeam == TEAM_BLUE && !(ent->client->mGameFlags & PSG_CANTEAMVOTEBLUE)) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", "You were not in-game and in this team when the vote was called. You cannot vote."));
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
		return;
	}
	if (ent->client->sess.sessionTeam == TEAM_RED && ent->client->mGameFlags & PSG_TEAMVOTEDRED) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")));
		return;
	}
	if (ent->client->sess.sessionTeam == TEAM_BLUE && ent->client->mGameFlags & PSG_TEAMVOTEDBLUE) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")));
		return;
	}

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( tolower(msg[0]) == 'y' ) {
		G_LogPrintf("Client %i (%s) teamvoted YES\n", ent - g_entities, ent->client->pers.netname);
		level.teamVoteYes[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	} else if ( tolower(msg[0]) == 'n' ) {
		G_LogPrintf("Client %i (%s) teamvoted NO\n", ent - g_entities, ent->client->pers.netname);
		level.teamVoteNo[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );	
	} else {
		trap_SendServerCommand(ent - g_entities, "print \"Invalid choice, please use /teamvote y or /teamvote n.\n\"");
		return;
	}

	// if we got here, the vote attempt was successful

	trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLTEAMVOTECAST")));

	if (ent->client->sess.sessionTeam == TEAM_RED)
	{
		ent->client->mGameFlags |= PSG_TEAMVOTEDRED;
	}
	else
	{
		ent->client->mGameFlags |= PSG_TEAMVOTEDBLUE;
	}

	if (g_teamVoteFix.integer)
	{
		G_TeamCommand(team, va("print \"Team Vote: %s <Yes: %i  No: %i>\n\"", level.teamVoteString[cs_offset], level.teamVoteYes[cs_offset], level.teamVoteNo[cs_offset]));
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}
	if ( trap_Argc() != 5 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap_Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	TeleportPlayer( ent, origin, angles );
}

void Cmd_Ignore_f( gentity_t *ent )
{			   
	return;
	char buffer[64];
	gentity_t* found = NULL;
	int id = 0;

	if ( !ent->client )
	{
		return;
	}

	if (trap_Argc() < 2)
	{
		trap_SendServerCommand( ent-g_entities, 
			"print \"usage: ignore [name or client number]  (name can be just part of name, colors don't count. use ^5/whois^7 to see client numbers)  \n\"");
		return;
	}

	trap_Argv(1,buffer,sizeof(buffer));		

	if ( atoi(buffer) == -1)
	{
		if (ent->client->sess.ignoreFlags == 0xFFFFFFFF)
		{
			trap_SendServerCommand( ent-g_entities, 
				va("print \"All Clients messages are now "S_COLOR_GREEN"allowed"S_COLOR_WHITE".\n\""));
			ent->client->sess.ignoreFlags = 0;
		} 
		else 
		{
			trap_SendServerCommand( ent - g_entities, 
				va( "print \"All Clients messages are now "S_COLOR_RED"ignored"S_COLOR_WHITE".\n\"" ) );
			ent->client->sess.ignoreFlags = 0xFFFFFFFF;
		}

		return;
	} 

	found = G_FindClient( buffer );	  
	if ( !found || !found->client )
	{
		trap_SendServerCommand(
			ent - g_entities,
			va( "print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"",
			buffer ) );
		return;
	}

	id = found - g_entities;  
	if ( ent->client->sess.ignoreFlags & (1 << id) )
	{
		trap_SendServerCommand( ent - g_entities,
			va( "print \"Client '%s"S_COLOR_WHITE"' messages are now "S_COLOR_GREEN"allowed"S_COLOR_WHITE".\n\"",
			found->client->pers.netname ) );
	}
	else
	{
		trap_SendServerCommand( ent - g_entities,
			va( "print \"Client '%s"S_COLOR_WHITE"' messages are now "S_COLOR_RED"ignored"S_COLOR_WHITE".\n\"",
			found->client->pers.netname ) );
	}
	ent->client->sess.ignoreFlags ^= (1 << id);

}

#define MST (-7)
#define UTC (0)
#define CCT (+8)

void Cmd_WhoIs_f( gentity_t* ent );

void Cmd_TestCmd_f( gentity_t *ent ) {
}

/*
=================
Cmd_TopTimes_f
=================
*/

static void FormatLocalDateFromEpoch( time_t now, char* buf, size_t bufSize, time_t epochSecs ) {
	struct tm * timeinfo;
	timeinfo = localtime( &epochSecs );

	int diff = now - epochSecs;
	int minutesAgo = diff / 60;
	int hoursAgo = diff / 60 / 60;
	int secondsAgo = diff;
	if (hoursAgo < 24) {
		if (minutesAgo < 1)
			Com_sprintf(buf, bufSize, "%d second%s ago", secondsAgo, secondsAgo == 1 ? "" : "s");
		else if (hoursAgo < 1)
			Com_sprintf(buf, bufSize, "%d minute%s ago", minutesAgo, minutesAgo == 1 ? "" : "s");
		else
			Com_sprintf(buf, bufSize, "%d hour%s ago", hoursAgo, hoursAgo == 1 ? "" : "s");
	}
	else {
		strftime(buf, bufSize, "%d %b %y %I:%M %p", timeinfo);
	}
}

// if one parameter is NULL, its value is added to the next non NULL parameter
void PartitionedTimer( const int time, int *mins, int *secs, int *millis ) {
	div_t qr;
	int pMins, pSecs, pMillis;

	qr = div( time, 1000 );
	pMillis = qr.rem;
	qr = div( qr.quot, 60 );
	pSecs = qr.rem;
	pMins = qr.quot;

	if ( mins ) {
		*mins = pMins;
	} else {
		pSecs += pMins * 60;
	}

	if ( secs ) {
		*secs = pSecs;
	} else {
		pMillis += pSecs * 1000;
	}

	if ( millis ) {
		*millis = pMillis;
	}
}

typedef struct {
	const char *mapname;
	const char *altMapname;
	qboolean matchMapNameExactly;
	int numObjs;
	char *objName[MAX_SAVED_OBJECTIVES];
} FancyObjNameData;

static const FancyObjNameData fancyObjNameData[] = {
	{ "siege_cargobarge3", "siege_cargobarge2", qfalse, 6,
		{ "HoldLock", "CommArray", "Nodes", "CC", "Codes", "Escape" } },
	{ "siege_urban", NULL, qfalse, 5,
		{"Crack", "LiquorStore", "Prostitute", "Bike", "Witnesses" } },
	{ "mp/siege_hoth", "siege_hoth", qfalse, 6,
		{ "Trench", "Bridge", "ShieldGen", "Codes", "Hangar", "CC" } },
	{ "siege_narshaddaa", NULL, qtrue, 5,
		{ "Entrance", "Checkpoint", "Stations", "Bridge", "Codes" } },
	{ "siege_ansion", NULL, qfalse, 5,
		{ "Forcefield", "Stations", "NPCs", "FirstCodes", "SecondCodes"} },
	{ "mp/siege_desert", NULL, qtrue, 5,
		{ "Wall", "Stations", "Arena", "Tower", "Parts" } },
	{ "mp/siege_korriban", NULL, qtrue, 4,
		{ "Gate", "Crystals", "Scepter", "Coffin" } },
	{ "siege_cargobarge", NULL, qtrue, 5,
		{ "Hold", "CommArray", "Nodes", "CC", "Codes" } },
	{ "mp/siege_eat_shower", NULL, qtrue, 6,
		{ "Water", "Button", "Consoles", "FirstTP", "SecondTP", "Codes" } },
	{ "mp/siege_destroyer", NULL, qtrue, 2,
		{ "Defenses", "TractorBeam" } },
	{ "mp/siege_bespin", NULL, qfalse, 6,
		{ "Door", "Consoles", "PowerGen", "Codes", "Lift", "TwinPod" } },
	{ "mp/siege_imperial", NULL, qfalse, 5,
        { "ShieldCooling", "Consoles", "Codes", "Lift", "Bridge" } },
	{ NULL }
};

char *ProperObjectiveName(const char *mapname, CombinedObjNumber obj) {
	// try to get it from worldspawn if it's the current map
	if (obj >= 1 && obj <= MAX_SAVED_OBJECTIVES && level.combinedObjName[obj - 1][0] && !Q_stricmp(mapname, level.mapname))
		return level.combinedObjName[obj - 1];

	// try to get it from past worldspawn data
	static char buf[32] = { 0 };
	memset(&buf, 0, sizeof(buf));
	G_DBGetMetadata(va("combinedobjname_%s_%d", mapname, obj), buf, sizeof(buf));
	if (buf[0])
		return buf;

	// try to get it from the hardcoded data
	for (const FancyObjNameData *data = &fancyObjNameData[0]; VALIDSTRING(data->mapname); data++) {
		if (data->matchMapNameExactly) {
			if (Q_stricmp(data->mapname, mapname) &&
				!(VALIDSTRING(data->altMapname) && !Q_stricmp(data->altMapname, mapname))) {
				continue;
			}
		}
		else {
			if (Q_stricmpn(data->mapname, mapname, strlen(data->mapname)) &&
				!(VALIDSTRING(data->altMapname) && !Q_stricmpn(data->altMapname, mapname, strlen(data->altMapname)))) {
				continue;
			}
		}
		if (obj > 0 && obj <= data->numObjs) {
			assert(VALIDSTRING(data->objName[obj - 1]));
			return data->objName[obj - 1];
		}
		assert(qfalse);
		break;
	}
	return va("Obj %d", obj);
}

#define RECORDTYPE_LONGNAME_BUFFERS		(8)
#define RECORDTYPE_LONGNAME_BUFSIZE		(64)
const char *GetLongNameForRecordFlags(const char *mapname, CaptureCategoryFlags flags, qboolean forceSoloIfSpeedrunAndNonCoop) {
	static char string[RECORDTYPE_LONGNAME_BUFFERS][RECORDTYPE_LONGNAME_BUFSIZE];	// in case va is called by nested functions
	static int index = -1;
	char *buf = (char *)&string[++index % RECORDTYPE_LONGNAME_BUFFERS];
	const int size = RECORDTYPE_LONGNAME_BUFSIZE;
	memset(buf, 0, RECORDTYPE_LONGNAME_BUFSIZE);

	if (flags & CAPTURERECORDFLAG_DEFENSE) {
		Q_strcat(buf, size, "Defense");
		if (flags & CAPTURERECORDFLAG_2V2)
			Q_strcat(buf, size, " 2v2");
		else if (flags & CAPTURERECORDFLAG_3V3)
			Q_strcat(buf, size, " 3v3");
		else if (flags & CAPTURERECORDFLAG_4V4)
			Q_strcat(buf, size, " 4v4");
		if (flags & CAPTURERECORDFLAG_FULLMAP) {
			Q_strcat(buf, size, " FullMap");
		}
		else {
			for (int i = 0; i < MAX_SAVED_OBJECTIVES; i++) {
				if (flags & (1 << i)) {
					Q_strcat(buf, size, va(" %s", ProperObjectiveName(mapname, i)));
					break;
				}
			}
		}
	}
	else if (flags & CAPTURERECORDFLAG_ANYPERCENT || flags & CAPTURERECORDFLAG_LIVEPUG || flags & CAPTURERECORDFLAG_DEFENSE) {
		Q_strncpyz(buf, flags & CAPTURERECORDFLAG_LIVEPUG ? "LivePug" : "Any'/.", size);
		if (flags & CAPTURERECORDFLAG_FULLMAP) {
			Q_strcat(buf, size, " FullMap");
		}
		else {
			for (int i = 0; i < MAX_SAVED_OBJECTIVES; i++) {
				if (flags & (1 << i)) {
					Q_strcat(buf, size, va(" %s", ProperObjectiveName(mapname, i)));
					break;
				}
			}
		}
	}
	else if (flags & CAPTURERECORDFLAG_SPEEDRUN) {
		Q_strcat(buf, size, "Speedrun");
		if (flags & CAPTURERECORDFLAG_COOP)
			Q_strcat(buf, size, " Co-op");
		else if (flags & CAPTURERECORDFLAG_SOLO || forceSoloIfSpeedrunAndNonCoop)
			Q_strcat(buf, size, " Solo");

		if (flags & CAPTURERECORDFLAG_ASSAULT)
			Q_strcat(buf, size, " Assault");
		else if (flags & CAPTURERECORDFLAG_HW)
			Q_strcat(buf, size, " HW");
		else if (flags & CAPTURERECORDFLAG_DEMO)
			Q_strcat(buf, size, " Demo");
		else if (flags & CAPTURERECORDFLAG_TECH)
			Q_strcat(buf, size, " Tech");
		else if (flags & CAPTURERECORDFLAG_SCOUT)
			Q_strcat(buf, size, " Scout");
		else if (flags & CAPTURERECORDFLAG_JEDI)
			Q_strcat(buf, size, " Jedi");
		if (flags & CAPTURERECORDFLAG_FULLMAP) {
			Q_strcat(buf, size, " FullMap");
			if (flags & CAPTURERECORDFLAG_ONESHOT)
				Q_strcat(buf, size, " OneShot");
		}
		else {
			for (int i = 0; i < MAX_SAVED_OBJECTIVES; i++) {
				if (flags & (1 << i)) {
					Q_strcat(buf, size, va(" %s", ProperObjectiveName(mapname, i)));
					break;
				}
			}
		}
	}
	else {
		return "?"; // dafuq? shouldn't happen
	}

	return buf;
}

#define OBJ_NAME_PARTIALMATCH_LENGTH	(4)
// note that it's possible for this function to return categories that are speedrun but neither co-op nor solo
// this is intentional for the printing of stats, despite the fact that such runs are impossible to specifically generate ingame
CaptureCategoryFlags GetRecordFlagsForString(const char *mapname, char *s, qboolean *displayBothSpeedRunAndLivePugVariants, qboolean mustBeFullMap, qboolean forceSoloIfSpeedrunAndNonCoop) {
	if (displayBothSpeedRunAndLivePugVariants)
		*displayBothSpeedRunAndLivePugVariants = qfalse;
	if (!VALIDSTRING(s))
		return 0;

	qboolean manuallySpecifiedMainCategory;
	CaptureCategoryFlags flags = 0;
	if (stristr(s, "def")) {
		flags |= CAPTURERECORDFLAG_DEFENSE;
		manuallySpecifiedMainCategory = qtrue;
		if (stristr(s, "2v") || stristr(s, "2s"))
			flags |= CAPTURERECORDFLAG_2V2;
		else if (stristr(s, "3v") || stristr(s, "3s"))
			flags |= CAPTURERECORDFLAG_3V3;
		else if (stristr(s, "4v") || stristr(s, "4s"))
			flags |= CAPTURERECORDFLAG_4V4;
	}
	else if (stristr(s, "speed") || stristr(s, "run")) {
		flags |= CAPTURERECORDFLAG_SPEEDRUN;
		manuallySpecifiedMainCategory = qtrue;
	}
	else if (stristr(s, "live") || stristr(s, "pug")) {
		flags |= CAPTURERECORDFLAG_LIVEPUG;
		manuallySpecifiedMainCategory = qtrue;
	}
	else if (stristr(s, "any") || stristr(s, "perc") || stristr(s, "pct")) {
		flags |= CAPTURERECORDFLAG_ANYPERCENT;
		manuallySpecifiedMainCategory = qtrue;
	}
	else {
		flags |= CAPTURERECORDFLAG_SPEEDRUN; // default to speedruns
		manuallySpecifiedMainCategory = qfalse;
	}
	
	// check for partially matching an obj name, e.g. "stat" for "stations"
	qboolean gotObjNumber = qfalse;
	if (mustBeFullMap) {
		flags |= CAPTURERECORDFLAG_FULLMAP;
		gotObjNumber = qtrue;
	}
	else {
		// try to get it from worldspawn if it's the current map
		if (!Q_stricmp(mapname, level.mapname)) {
			for (int i = 0; i < MAX_SAVED_OBJECTIVES; i++) {
				if (level.combinedObjName[i][0]) {
					char buf[OBJ_NAME_PARTIALMATCH_LENGTH + 1];
					Q_strncpyz(buf, level.combinedObjName[i], sizeof(buf));
					RemoveSpaces(buf);
					if (stristr(s, buf)) {
						flags |= (1 << (i + 1));
						gotObjNumber = qtrue;
						break;
					}
				}
			}
		}

		if (!gotObjNumber) {
			// try to get it from past worldspawn data
			for (int i = 0; i < MAX_SAVED_OBJECTIVES; i++) {
				char buf[OBJ_NAME_PARTIALMATCH_LENGTH] = { 0 };
				G_DBGetMetadata(va("combinedobjname_%s_%d", mapname, i + 1), buf, sizeof(buf));
				if (buf[0]) {
					RemoveSpaces(buf);
					if (stristr(s, buf)) {
						flags |= (1 << (i + 1));
						gotObjNumber = qtrue;
						break;
					}
				}
			}
		}

		if (!gotObjNumber) {
			// try to get it from the hardcoded data
			for (const FancyObjNameData *data = &fancyObjNameData[0]; VALIDSTRING(data->mapname); data++) {
				if (data->matchMapNameExactly) {
					if (Q_stricmp(data->mapname, mapname) &&
						!(VALIDSTRING(data->altMapname) && !Q_stricmp(data->altMapname, mapname))) {
						continue;
					}
				}
				else {
					if (Q_stricmpn(data->mapname, mapname, strlen(data->mapname)) &&
						!(VALIDSTRING(data->altMapname) && !Q_stricmpn(data->altMapname, mapname, strlen(data->altMapname)))) {
						continue;
					}
				}
				for (CombinedObjNumber i = 1; i <= data->numObjs; i++) {
					assert(VALIDSTRING(data->objName[i - 1]));
					char buf[OBJ_NAME_PARTIALMATCH_LENGTH + 1];
					Q_strncpyz(buf, data->objName[i - 1], sizeof(buf));
					RemoveSpaces(buf);
					if (stristr(s, buf)) {
						flags |= (1 << i);
						gotObjNumber = qtrue;
						break;
					}
				}
				break;
			}
		}
	}

	qboolean isSubsetOfSpeedrunsOnly = qfalse;
	if (!gotObjNumber) {
		if (flags & CAPTURERECORDFLAG_SPEEDRUN && (stristr(s, "1shot") || stristr(s, "shot"))) {
			flags |= CAPTURERECORDFLAG_FULLMAP | CAPTURERECORDFLAG_ONESHOT;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
		else if (stristr(s, "full") || stristr(s, "map"))
			flags |= CAPTURERECORDFLAG_FULLMAP;
		else if (strchr(s, '1') && !(level.numSiegeObjectivesOnMapCombined && 1 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ1;
		else if (strchr(s, '2') && !(flags & CAPTURERECORDFLAG_DEFENSE && (stristr(s, "2v") || stristr(s, "2s"))) && !(level.numSiegeObjectivesOnMapCombined && 2 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ2;
		else if (strchr(s, '3') && !(flags & CAPTURERECORDFLAG_DEFENSE && (stristr(s, "3v") || stristr(s, "3s"))) && !(level.numSiegeObjectivesOnMapCombined && 3 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ3;
		else if (strchr(s, '4') && !(flags & CAPTURERECORDFLAG_DEFENSE && (stristr(s, "4v") || stristr(s, "4s"))) && !(level.numSiegeObjectivesOnMapCombined && 4 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ4;
		else if (strchr(s, '5') && !(level.numSiegeObjectivesOnMapCombined && 5 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ5;
		else if (strchr(s, '6') && !(level.numSiegeObjectivesOnMapCombined && 6 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ6;
		else if (strchr(s, '7') && !(level.numSiegeObjectivesOnMapCombined && 7 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ7;
		else if (strchr(s, '8') && !(level.numSiegeObjectivesOnMapCombined && 8 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ8;
		else if (strchr(s, '9') && !(level.numSiegeObjectivesOnMapCombined && 9 > level.numSiegeObjectivesOnMapCombined))
			flags |= CAPTURERECORDFLAG_OBJ9;
		else
			flags |= CAPTURERECORDFLAG_FULLMAP;
	}

	// <class>-only, co-op, and one-shot are subsets of ONLY speedruns; they can't coexist with livepug or any%
	if (flags & CAPTURERECORDFLAG_SPEEDRUN) {
		if (stristr(s, "assa")) {
			flags |= CAPTURERECORDFLAG_ASSAULT;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
		else if (stristr(s, "hw")) {
			flags |= CAPTURERECORDFLAG_HW;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
		else if (stristr(s, "demo")) {
			flags |= CAPTURERECORDFLAG_DEMO;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
		else if (stristr(s, "tech")) {
			flags |= CAPTURERECORDFLAG_TECH;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
		else if (stristr(s, "scout")) {
			flags |= CAPTURERECORDFLAG_SCOUT;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
		else if (stristr(s, "jedi")) {
			flags |= CAPTURERECORDFLAG_JEDI;
			isSubsetOfSpeedrunsOnly = qtrue;
		}

		if (stristr(s, "coop") || stristr(s, "co-op")) {
			flags |= CAPTURERECORDFLAG_COOP;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
		else if (forceSoloIfSpeedrunAndNonCoop || stristr(s, "solo")) {
			flags |= CAPTURERECORDFLAG_SOLO;
			isSubsetOfSpeedrunsOnly = qtrue;
		}
	}

	/*
	if:
		- we didn't manually specify speedrun/livepug/any%
		- we aren't using a subset of speedruns such as <class>-only or co-op
	then:
		- we can display the requested category in both speedrun AND livepug variants
	*/
	// if:we didn't manually specify speedrun/livepug/any%, we aren't using
	if (!manuallySpecifiedMainCategory && !isSubsetOfSpeedrunsOnly && displayBothSpeedRunAndLivePugVariants) {
		flags &= ~CAPTURERECORDFLAG_SOLO; // if they didn't specify solo, allow co-op to be shown so that we can also show live pug
		*displayBothSpeedRunAndLivePugVariants = qtrue;
	}

	return flags;
}

static qboolean FlagsMatch(genericNode_t *node, void *userData) {
	const CaptureRecordsForCategory *candidate = (const CaptureRecordsForCategory *)node;
	const CaptureCategoryFlags flags = *((CaptureCategoryFlags *)userData);
	if (candidate->flags == flags)
		return qtrue;
	return qfalse;
}

CaptureRecordsForCategory *CaptureRecordsForCategoryFromFlags(CaptureCategoryFlags flags) {
	CaptureRecordsForCategory *records = ListFind(&level.mapCaptureRecords.captureRecordsList, FlagsMatch, &flags, NULL);
	if (records)
		return records;
	records = ListAdd(&level.mapCaptureRecords.captureRecordsList, sizeof(CaptureRecordsForCategory));
	G_DBLoadCaptureRecords(level.mapCaptureRecords.mapname, flags, qtrue, records);
	records->flags = flags;
	return records;
}

static void copyTopNameCallback( void* context, const char* name, int duration ) {
	Q_strncpyz( ( char* )context, name, MAX_NETNAME );
}

typedef struct {
	int entNum;
	qboolean hasPrinted;
} BestTimeContext;

#define TOPTIMES_NAME_BUFFER_SIZE								(256)
#define TOPTIMES_NAME_MAX_PRINTABLE_CHARS						(45)
#define TOPTIMES_2NAMES_MAX_PRINTABLE_CHARS_PER_NAME			(21)
#define TOPTIMES_3NAMES_MAX_PRINTABLE_CHARS_PER_NAME			(14)
#define TOPTIMES_4NAMES_MAX_PRINTABLE_CHARS_PER_NAME			(10)

// returns a combined string of player names, e.g.
// ^1friend^7
// ^1friend^9 & ^2buddy^7
// ^1friend^9, ^2buddy^9, ^3pal^7
// ^1friend^9, ^2buddy^9, ^3pal^9, ^4guy^7
static char *CombineNames(const char *inName1, const char *inName2, const char *inName3, const char *inName4, qboolean padSpaces) {
	static char combinedNameString[256] = { 0 };
	memset(&combinedNameString, 0, sizeof(combinedNameString));
	char name[LOGGED_PLAYERS_PER_OBJ][64] = { 0 };
	int players = 0;
	if (VALIDSTRING(inName1)) {
		Q_strncpyz(name[0], inName1, sizeof(name[0]));
		players++;
	}
	else {
		assert(qfalse);
		return "";
	}
	if (VALIDSTRING(inName2)) {
		Q_strncpyz(name[1], inName2, sizeof(name[1]));
		players++;
		if (VALIDSTRING(inName3)) {
			Q_strncpyz(name[2], inName3, sizeof(name[2]));
			players++;
			if (VALIDSTRING(inName4)) {
				Q_strncpyz(name[3], inName4, sizeof(name[3]));
				players++;
			}
		}
	}

	int freeChars = TOPTIMES_NAME_MAX_PRINTABLE_CHARS;
	switch (players) {
	case 1:
		Q_strncpyz(combinedNameString, ChopString(inName1, TOPTIMES_NAME_MAX_PRINTABLE_CHARS), sizeof(combinedNameString));
		if (padSpaces) { // pad the name string with spaces here because printf padding will ignore colors
			int spacesToAdd = TOPTIMES_NAME_MAX_PRINTABLE_CHARS - Q_PrintStrlen(combinedNameString);
			for (int j = 0; j < sizeof(combinedNameString) && spacesToAdd > 0; ++j) {
				if (!combinedNameString[j]) {
					combinedNameString[j] = ' '; // replace null terminators with spaces
					--spacesToAdd;
				}
			}
			combinedNameString[sizeof(combinedNameString) - 1] = '\0'; // make sure it's still null terminated
		}
		return combinedNameString;
	case 2: freeChars -= strlen(" & "); break;
	case 3: freeChars -= strlen(", , "); break;
	case 4: freeChars -= strlen(", , , "); break;
	default: assert(qfalse); return "";
	}

	// allocate chars for each name
	// e.g. for one player with a really long name and one with a short name,
	// if the total length is acceptable, than the really long name player
	// can take up more than 50% of the available digits
	int allocated[LOGGED_PLAYERS_PER_OBJ] = { 0 };
	qboolean good[LOGGED_PLAYERS_PER_OBJ] = { qfalse };
	for (int i = 0; freeChars > 0; i = (i + 1) % players) {
		int len = Q_PrintStrlen(name[i]);
		if (allocated[i] < len) {
			allocated[i]++;
			freeChars--;
		}
		else {
			good[i] = qtrue;
			qboolean allGood = qtrue;
			for (int j = 0; j < players; j++) {
				if (!good[j]) {
					allGood = qfalse;
					break;
				}
			}
			if (allGood)
				break;
		}
	}

	Q_strncpyz(combinedNameString, ChopString(name[0], allocated[0]), sizeof(combinedNameString));
	if (players == 2) {
		Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9 & ^7%s", ChopString(name[1], allocated[1])));
	}
	else {
		Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9, ^7%s", ChopString(name[1], allocated[1])));
		if (players >= 4) {
			Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9, ^7%s", ChopString(name[2], allocated[2])));
			Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9, & ^7%s", ChopString(name[3], allocated[3])));
		}
		else {
			Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9, & ^7%s", ChopString(name[2], allocated[2])));
		}
	}
	Q_strcat(combinedNameString, sizeof(combinedNameString), "^7");

	if (padSpaces) { // pad the name string with spaces here because printf padding will ignore colors
		int spacesToAdd = TOPTIMES_NAME_MAX_PRINTABLE_CHARS - Q_PrintStrlen(combinedNameString);
		for (int j = 0; j < sizeof(combinedNameString) && spacesToAdd > 0; ++j) {
			if (!combinedNameString[j]) {
				combinedNameString[j] = ' '; // replace null terminators with spaces
				--spacesToAdd;
			}
		}
		combinedNameString[sizeof(combinedNameString) - 1] = '\0'; // make sure it's still null terminated
	}

	return combinedNameString;
}

static void printBestTimeCallback( void *context, const char *mapname, const CaptureCategoryFlags flags, const CaptureCategoryFlags thisRecordFlags,
	const char *recordHolderName1, unsigned int recordHolderIpInt1, const char *recordHolderCuid1,
	const char *recordHolderName2, unsigned int recordHolderIpInt2, const char *recordHolderCuid2,
	const char *recordHolderName3, unsigned int recordHolderIpInt3, const char *recordHolderCuid3,
	const char *recordHolderName4, unsigned int recordHolderIpInt4, const char *recordHolderCuid4,
	int bestTime, time_t bestTimeDate ) {
	BestTimeContext* thisContext = ( BestTimeContext* )context;

	// if we are printing the current map, since we only save new records at the end of the round, check if we beat the top time during this session
	// and print that one instead (the record in DB will stay outdated until round ends)
	if ( !Q_stricmp( mapname, level.mapCaptureRecords.mapname ) ) {
		CaptureRecordsForCategory *recordsPtr = CaptureRecordsForCategoryFromFlags(flags);
		const CaptureRecord *currentRecord = &recordsPtr->records[0];

		if ( currentRecord->totalTime && currentRecord->totalTime < bestTime ) {
			recordHolderName1 = currentRecord->recordHolderNames[0];
			recordHolderName2 = currentRecord->recordHolderNames[1];
			recordHolderName3 = currentRecord->recordHolderNames[2];
			recordHolderName4 = currentRecord->recordHolderNames[3];
			recordHolderIpInt1 = currentRecord->recordHolderIpInts[0];
			recordHolderIpInt2 = currentRecord->recordHolderIpInts[1];
			recordHolderIpInt3 = currentRecord->recordHolderIpInts[2];
			recordHolderIpInt4 = currentRecord->recordHolderIpInts[3];
			recordHolderCuid1 = currentRecord->recordHolderCuids[0];
			recordHolderCuid2 = currentRecord->recordHolderCuids[1];
			recordHolderCuid3 = currentRecord->recordHolderCuids[2];
			recordHolderCuid4 = currentRecord->recordHolderCuids[3];
			bestTime = currentRecord->totalTime;
			bestTimeDate = currentRecord->date;
		}
	}

	if ( !thisContext->hasPrinted ) {
		// first time printing, show a header
		trap_SendServerCommand( thisContext->entNum, va( "print \""S_COLOR_WHITE"Records for the "S_COLOR_CYAN"%s "S_COLOR_WHITE"category:\n^5%-26s  %-9s  %-45s  %-18s  %-38s\n\"", GetLongNameForRecordFlags( mapname, flags, qfalse ), "Map", "Time", "Name", "Date", "Category" ) );
	}

	char identifiers[LOGGED_PLAYERS_PER_OBJ][MAX_NETNAME + 1] = { 0 };
	G_DBListAliases(recordHolderIpInt1, ( unsigned int )0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[0], recordHolderCuid1);
	if (VALIDSTRING(recordHolderName2)) {
		G_DBListAliases(recordHolderIpInt2, (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[1], recordHolderCuid2);
		if (VALIDSTRING(recordHolderName3)) {
			G_DBListAliases(recordHolderIpInt3, (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[2], recordHolderCuid3);
			if (VALIDSTRING(recordHolderName4))
				G_DBListAliases(recordHolderIpInt4, (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[3], recordHolderCuid4);
		}
	}

	// no name in db for this guy, use the one we stored
	if ( !VALIDSTRING( identifiers[0] ) )
		Q_strncpyz( identifiers[0], recordHolderName1, sizeof(identifiers[0]) );
	if ( VALIDSTRING(recordHolderName2) && !VALIDSTRING( identifiers[1] ) )
		Q_strncpyz( identifiers[1], recordHolderName2, sizeof( identifiers[1] ) );
	if (VALIDSTRING(recordHolderName3) && !VALIDSTRING(identifiers[2]))
		Q_strncpyz(identifiers[2], recordHolderName3, sizeof(identifiers[2]));
	if (VALIDSTRING(recordHolderName4) && !VALIDSTRING(identifiers[3]))
		Q_strncpyz(identifiers[3], recordHolderName4, sizeof(identifiers[3]));

	char combinedNameString[TOPTIMES_NAME_BUFFER_SIZE] = { 0 };
	Q_strncpyz(combinedNameString, CombineNames(identifiers[0], identifiers[1], identifiers[2], identifiers[3], qtrue), sizeof(combinedNameString));

	int mins, secs, millis;
	PartitionedTimer( bestTime, &mins, &secs, &millis );

	char timeString[10] = { 0 };
	if (mins > 9) // 12:59.123
		Com_sprintf(timeString, sizeof(timeString), "%d:%02d.%03d", mins, secs, millis);
	else if (mins > 0) // 2:59.123
		Com_sprintf(timeString, sizeof(timeString), " %d:%02d.%03d", mins, secs, millis);
	else if (secs > 9) // 59.123
		Com_sprintf(timeString, sizeof(timeString), "   %d.%03d", secs, millis);
	else // 9.123
		Com_sprintf(timeString, sizeof(timeString), "    %d.%03d", secs, millis);

	char *dateColor;
	char date[19] = { 0 };
	time_t now = time( NULL );
	if (now - bestTimeDate < 60 * 60 * 24) {
		dateColor = "^2";
		FormatLocalDateFromEpoch(now, date, sizeof(date), bestTimeDate);
	}
	else {
		dateColor = "^7";
		FormatLocalDateFromEpoch(now, date, sizeof(date), bestTimeDate);
	}

	trap_SendServerCommand( thisContext->entNum, va(
		"print \"^7%-26s^7  ^5%-9s^7  ^7%s  %s%-18s  ^7%-38s\n\"", mapname, timeString, combinedNameString, dateColor, date, GetLongNameForRecordFlags(mapname, thisRecordFlags, qtrue)) );

	thisContext->hasPrinted = qtrue;
}

static void printLatestTimesCallback(void *context, const char *mapname, const CaptureCategoryFlags flags, const CaptureCategoryFlags thisRecordFlags,
	const char *recordHolderName1, unsigned int recordHolderIpInt1, const char *recordHolderCuid1,
	const char *recordHolderName2, unsigned int recordHolderIpInt2, const char *recordHolderCuid2,
	const char *recordHolderName3, unsigned int recordHolderIpInt3, const char *recordHolderCuid3,
	const char *recordHolderName4, unsigned int recordHolderIpInt4, const char *recordHolderCuid4,
	int bestTime, time_t bestTimeDate, int rank) {
	BestTimeContext* thisContext = (BestTimeContext*)context;

	if (!thisContext->hasPrinted) {
		// first time printing, show a header
		if (flags)
			trap_SendServerCommand(thisContext->entNum, va("print \""S_COLOR_WHITE"Latest records for the "S_COLOR_CYAN"%s "S_COLOR_WHITE"category:\n^5# %-26s  %-9s  %-45s  %-18s  %-38s\n\"", GetLongNameForRecordFlags(mapname, flags, qfalse), "Map", "Time", "Name", "Date", "Category"));
		else
			trap_SendServerCommand(thisContext->entNum, va("print \""S_COLOR_WHITE"Latest records:\n^5# %-26s  %-9s  %-45s  %-18s  %-38s\n\"", "Map", "Time", "Name", "Date", "Category"));
	}

	char identifiers[LOGGED_PLAYERS_PER_OBJ][MAX_NETNAME + 1] = { 0 };
	G_DBListAliases(recordHolderIpInt1, (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[0], recordHolderCuid1);
	if (VALIDSTRING(recordHolderName2)) {
		G_DBListAliases(recordHolderIpInt2, (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[1], recordHolderCuid2);
		if (VALIDSTRING(recordHolderName3)) {
			G_DBListAliases(recordHolderIpInt3, (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[2], recordHolderCuid3);
			if (VALIDSTRING(recordHolderName4))
				G_DBListAliases(recordHolderIpInt4, (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &identifiers[3], recordHolderCuid4);
		}
	}

	// no name in db for this guy, use the one we stored
	if (!VALIDSTRING(identifiers[0]))
		Q_strncpyz(identifiers[0], recordHolderName1, sizeof(identifiers[0]));
	if (VALIDSTRING(recordHolderName2) && !VALIDSTRING(identifiers[1]))
		Q_strncpyz(identifiers[1], recordHolderName2, sizeof(identifiers[1]));
	if (VALIDSTRING(recordHolderName3) && !VALIDSTRING(identifiers[2]))
		Q_strncpyz(identifiers[2], recordHolderName3, sizeof(identifiers[2]));
	if (VALIDSTRING(recordHolderName4) && !VALIDSTRING(identifiers[3]))
		Q_strncpyz(identifiers[3], recordHolderName4, sizeof(identifiers[3]));

	char combinedNameString[TOPTIMES_NAME_BUFFER_SIZE] = { 0 };
	Q_strncpyz(combinedNameString, CombineNames(identifiers[0], identifiers[1], identifiers[2], identifiers[3], qtrue), sizeof(combinedNameString));

	int mins, secs, millis;
	PartitionedTimer(bestTime, &mins, &secs, &millis);

	char timeString[10] = { 0 };
	if (mins > 9) // 12:59.123
		Com_sprintf(timeString, sizeof(timeString), "%d:%02d.%03d", mins, secs, millis);
	else if (mins > 0) // 2:59.123
		Com_sprintf(timeString, sizeof(timeString), " %d:%02d.%03d", mins, secs, millis);
	else if (secs > 9) // 59.123
		Com_sprintf(timeString, sizeof(timeString), "   %d.%03d", secs, millis);
	else // 9.123
		Com_sprintf(timeString, sizeof(timeString), "    %d.%03d", secs, millis);

	char *dateColor;
	char date[19] = { 0 };
	time_t now = time(NULL);
	if (now - bestTimeDate < 60 * 60 * 24) {
		dateColor = "^2";
		FormatLocalDateFromEpoch(now, date, sizeof(date), bestTimeDate);
	}
	else {
		dateColor = "^7";
		FormatLocalDateFromEpoch(now, date, sizeof(date), bestTimeDate);
	}
	char *rankString;
	if (rank >= 1 && rank <= MAX_SAVED_RECORDS) {
		switch (rank) {
		case 1: rankString = "^31"; break;
		case 2: rankString = "^92"; break;
		case 3: rankString = "^83"; break;
		default: rankString = va("^1%d", rank); break;
		}
	}
	else {
		assert(qfalse);
		Com_Printf("DEBUG MESSAGE: printLatestTimesCallback: bad rank %d\n", rank);
		rankString = " ";
	}

	trap_SendServerCommand(thisContext->entNum, va(
		"print \"^7%s ^7%-26s  ^5%-9s^7  ^7%s  %s%-18s  ^7%-38s\n\"", rankString, mapname, timeString, combinedNameString, dateColor, date, GetLongNameForRecordFlags(mapname, thisRecordFlags, qtrue)));

	thisContext->hasPrinted = qtrue;
}

#define MAPLIST_MAPS_PER_PAGE		(15)
#define MAX_CATEGORIES_TO_PRINT		(4)
#define DEMOARCHIVE_BASE_MATCH_URL	"https://demos.jactf.com/match.html#rpc=lookup&id=%s"
#define TOPTIMES_HELPMSG			"For [category], enter any combination of terms (such as obj1, obj2, map, pug, defense, jedi, tech, solo, coop, speedrun, anypercent, oneshot) without spaces.\nExamples:    map     solo     obj5pug     defenseobj3     obj2anypercent    codesjedi    coopscoutoneshot"

static qboolean PrintCategory(CaptureCategoryFlags flags, gentity_t *ent) {
	if (!flags)
		return qfalse;
	CaptureRecordsForCategory localRecords = { 0 };
	G_DBLoadCaptureRecords(level.mapCaptureRecords.mapname, flags, qfalse, &localRecords);
	if (!localRecords.records[0].totalTime) // there is no first record for that category
		return qfalse;

	const char* categoryName = GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, flags, qfalse);

	// prepend a newline and print the header
	if (flags & CAPTURERECORDFLAG_DEFENSE && flags & CAPTURERECORDFLAG_FULLMAP)
		trap_SendServerCommand(ent - g_entities, va("print \"\n"S_COLOR_WHITE"Records for the "S_COLOR_YELLOW"%s "S_COLOR_WHITE"category on "S_COLOR_YELLOW"%s"S_COLOR_WHITE":\n"S_COLOR_CYAN"%s: %-45s  %-9s  %-4s  %-5s  %-14s  %-18s  %-38s\n\"", categoryName, level.mapCaptureRecords.mapname, "#", "Name", "Time", "Frags", "Kpm", "Objs completed", "Date", "Category"));
	else if (flags & CAPTURERECORDFLAG_DEFENSE)
		trap_SendServerCommand(ent - g_entities, va("print \"\n"S_COLOR_WHITE"Records for the "S_COLOR_YELLOW"%s "S_COLOR_WHITE"category on "S_COLOR_YELLOW"%s"S_COLOR_WHITE":\n"S_COLOR_CYAN"%s: %-45s  %-9s  %-4s  %-5s  %-18s  %-38s\n\"", categoryName, level.mapCaptureRecords.mapname, "#", "Name", "Time", "Frags", "Kpm", "Date", "Category"));
	else
		trap_SendServerCommand(ent - g_entities, va("print \"\n"S_COLOR_WHITE"Records for the "S_COLOR_YELLOW"%s "S_COLOR_WHITE"category on "S_COLOR_YELLOW"%s"S_COLOR_WHITE":\n"S_COLOR_CYAN"%s: %-45s  %-9s  %-4s  %-3s  %-18s  %-38s\n\"", categoryName, level.mapCaptureRecords.mapname, "#", "Name", "Time", "TopSpd", "AvgSpd", "Date", "Category"));

	// print each record for this category as a row
	int goldTime = -1, silverTime = -1, bronzeTime = -1, fourthTime = -1;
	int goldObjs = -1, silverObjs = -1, bronzeObjs = -1, fourthObjs = -1;
	for (int i = 0; i < MAX_SAVED_RECORDS; ++i) {
		CaptureRecord *record = &localRecords.records[i];

		if (!record->totalTime) {
			continue;
		}

		const char *overrideRankColor = NULL;
		switch (i) {
		case 0:
			goldTime = record->totalTime;
			overrideRankColor = "^3";
			goldObjs = record->objectivesCompleted;
			break;
		case 1:
			if (record->totalTime == goldTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != goldObjs)) {
				overrideRankColor = "^3";
			}
			else {
				silverTime = record->totalTime;
				overrideRankColor = "^9";
				silverObjs = record->objectivesCompleted;
			}
			break;
		case 2:
			if (record->totalTime == goldTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != goldObjs)) {
				overrideRankColor = "^3";
			}
			else if (record->totalTime == silverTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != silverObjs)) {
				overrideRankColor = "^9";
			}
			else {
				bronzeTime = record->totalTime;
				overrideRankColor = "^8";
				bronzeObjs = record->objectivesCompleted;
			}
			break;
		case 3:
			if (record->totalTime == goldTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != goldObjs)) {
				overrideRankColor = "^3";
			}
			else if (record->totalTime == silverTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != silverObjs)) {
				overrideRankColor = "^9";
			}
			else if (record->totalTime == bronzeTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != bronzeObjs)) {
				overrideRankColor = "^8";
			}
			else {
				fourthTime = record->totalTime;
				overrideRankColor = "^1";
				fourthObjs = record->objectivesCompleted;
			}
			break;
		case 4:
			if (record->totalTime == goldTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != goldObjs)) {
				overrideRankColor = "^3";
			}
			else if (record->totalTime == silverTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != silverObjs)) {
				overrideRankColor = "^9";
			}
			else if (record->totalTime == bronzeTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != bronzeObjs)) {
				overrideRankColor = "^8";
			}
			else if (record->totalTime == fourthTime && !(flags & CAPTURERECORDFLAG_FULLMAP && record->objectivesCompleted != fourthObjs)) {
				overrideRankColor = "^1";
			}
			else {
				overrideRankColor = "^1";
			}
			break;
		}

		char nameStrings[LOGGED_PLAYERS_PER_OBJ][64] = { 0 };
		G_DBListAliases(record->recordHolderIpInts[0], (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &nameStrings[0], record->recordHolderCuids[0]);
		for (int i = 1; i < LOGGED_PLAYERS_PER_OBJ; i++) {
			if (record->recordHolderCuids[i][0])
				G_DBListAliases(record->recordHolderIpInts[i], (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &nameStrings[i], record->recordHolderCuids[i]);
		}

		// no name in db for this guy, use the one we stored
		if (!VALIDSTRING(nameStrings[0]))
			Q_strncpyz(nameStrings[0], record->recordHolderNames[0], sizeof(nameStrings[0]));
		for (int i = 1; i < LOGGED_PLAYERS_PER_OBJ; i++) {
			if (!VALIDSTRING(nameStrings[i]) && VALIDSTRING(record->recordHolderNames[i]))
				Q_strncpyz(nameStrings[i], record->recordHolderNames[i], sizeof(nameStrings[i]));
		}

		char combinedNameString[TOPTIMES_NAME_BUFFER_SIZE] = { 0 };
		Q_strncpyz(combinedNameString, CombineNames(nameStrings[0], nameStrings[1], nameStrings[2], nameStrings[3], qtrue), sizeof(combinedNameString));

		int mins, secs, millis;
		PartitionedTimer(record->totalTime, &mins, &secs, &millis);

		char timeString[10] = { 0 };
		if (mins > 9) // 12:59.123
			Com_sprintf(timeString, sizeof(timeString), "%d:%02d.%03d", mins, secs, millis);
		else if (mins > 0) // 2:59.123
			Com_sprintf(timeString, sizeof(timeString), " %d:%02d.%03d", mins, secs, millis);
		else if (secs > 9) // 59.123
			Com_sprintf(timeString, sizeof(timeString), "   %d.%03d", secs, millis);
		else // 9.123
			Com_sprintf(timeString, sizeof(timeString), "    %d.%03d", secs, millis);

		char *dateColor;
		char date[19] = { 0 };
		time_t now = time(NULL);
		if (now - record->date < 60 * 60 * 24) {
			dateColor = "^2";
			FormatLocalDateFromEpoch(now, date, sizeof(date), record->date);
		}
		else {
			dateColor = "^7";
			FormatLocalDateFromEpoch(now, date, sizeof(date), record->date);
		}

		const char *rankColor;
		if (VALIDSTRING(overrideRankColor)) {
			rankColor = overrideRankColor;
		}
		else {
			switch (i + 1) {
			case 1: rankColor = S_COLOR_YELLOW; break;
			case 2: rankColor = S_COLOR_GREY; break;
			case 3: rankColor = S_COLOR_ORANGE; break;
			default: rankColor = S_COLOR_RED; break;
			}
		}

		if (flags & CAPTURERECORDFLAG_DEFENSE && flags & CAPTURERECORDFLAG_FULLMAP) {
			trap_SendServerCommand(ent - g_entities, va(
				"print \"%s%d: ^7%s  ^7%-9s  ^7%-5d  ^7%-5.1f  %-14d  %s%-18s  ^7%-38s\n\"",
				rankColor, i + 1, combinedNameString, timeString, Com_Clampi(0, 99999999, record->frags), Com_Clamp(0.0f, 999.9f, record->kpm), Com_Clampi(0, 999, record->objectivesCompleted), dateColor, date, GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, record->flags, qtrue)));
		}
		else if (flags & CAPTURERECORDFLAG_DEFENSE) {
			trap_SendServerCommand(ent - g_entities, va(
				"print \"%s%d: ^7%s  ^7%-9s  ^7%-5d  ^7%-5.1f  %s%-18s  ^7%-38s\n\"",
				rankColor, i + 1, combinedNameString, timeString, Com_Clampi(0, 99999999, record->frags), Com_Clamp(0.0f, 999.9f, record->kpm), dateColor, date, GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, record->flags, qtrue)));
		}
		else {
			trap_SendServerCommand(ent - g_entities, va(
				"print \"%s%d: ^7%s  ^7%-9s  ^7%-6d  ^7%-6d  %s%-18s  ^7%-38s\n\"",
				rankColor, i + 1, combinedNameString, timeString, Com_Clampi(1, 99999999, record->maxSpeed1), Com_Clampi(1, 9999999, record->avgSpeed1), dateColor, date, GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, record->flags, qtrue)));
		}
	}
	return qtrue;
}

// sends the url for a demo to someone
static qboolean GetDemoURL(CaptureCategoryFlags flags, int rank, gentity_t *ent) {
	if (!flags || rank < 1 || rank > MAX_SAVED_RECORDS) {
		assert(qfalse);
		return qfalse;
	}
	CaptureRecordsForCategory localRecords = { 0 };
	G_DBLoadCaptureRecords(level.mapCaptureRecords.mapname, flags, qfalse, &localRecords);
	if (!localRecords.records[0].totalTime) // there is no first record for that category
		return qfalse;

	CaptureRecord *record = &localRecords.records[rank - 1];
	if (!record->totalTime || !record->matchId[0])
		return qfalse;

	const char* categoryName = GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, flags, qfalse); // the category they entered
	const char* thisCategoryName = GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, flags, qtrue); // the actual category (may be different)
	char *rankString;
	switch (rank) {
	case 1: rankString = "^3GOLD^7 rank"; break;
	case 2: rankString = "^9SILVER^7 rank"; break;
	case 3: rankString = "^8BRONZE^7 rank"; break;
	case 4: rankString = "Rank ^14^7"; break;
	case 5: rankString = "Rank ^15^7"; break;
	default: assert(qfalse); return qfalse;
	}

	char nameStrings[LOGGED_PLAYERS_PER_OBJ][64] = { 0 };
	G_DBListAliases(record->recordHolderIpInts[0], (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &nameStrings[0], record->recordHolderCuids[0]);
	for (int i = 1; i < LOGGED_PLAYERS_PER_OBJ; i++) {
		if (record->recordHolderCuids[i][0])
			G_DBListAliases(record->recordHolderIpInts[i], (unsigned int)0xFFFFFFFF, 1, copyTopNameCallback, &nameStrings[i], record->recordHolderCuids[i]);
	}

	// no name in db for this guy, use the one we stored
	if (!VALIDSTRING(nameStrings[0]))
		Q_strncpyz(nameStrings[0], record->recordHolderNames[0], sizeof(nameStrings[0]));
	for (int i = 1; i < LOGGED_PLAYERS_PER_OBJ; i++) {
		if (!VALIDSTRING(nameStrings[i]) && VALIDSTRING(record->recordHolderNames[i]))
			Q_strncpyz(nameStrings[i], record->recordHolderNames[i], sizeof(nameStrings[i]));
	}

	char combinedNameString[256] = { 0 };
	Q_strcat(combinedNameString, sizeof(combinedNameString), nameStrings[0]);
	if (nameStrings[1][0]) {
		for (int i = 1; i < LOGGED_PLAYERS_PER_OBJ; i++) {
			if (nameStrings[i][0]) {
				Q_strcat(combinedNameString, sizeof(combinedNameString), va("^9, %s", nameStrings[i]));
				if (Q_stricmp(nameStrings[i], record->recordHolderNames[0]))
					Q_strcat(combinedNameString, sizeof(combinedNameString), va("^7 (as %s^7)", record->recordHolderNames[i]));
			}
			else {
				break;
			}
		}
	}
	else {
		Q_strcat(combinedNameString, sizeof(combinedNameString), "^7");
	}

	int mins, secs, millis;
	PartitionedTimer(record->totalTime, &mins, &secs, &millis);

	char timeString[10] = { 0 };
	if (mins > 9) // 12:59.123
		Com_sprintf(timeString, sizeof(timeString), "%d:%02d.%03d", mins, secs, millis);
	else if (mins > 0) // 2:59.123
		Com_sprintf(timeString, sizeof(timeString), "%d:%02d.%03d", mins, secs, millis);
	else if (secs > 9) // 59.123
		Com_sprintf(timeString, sizeof(timeString), "%d.%03d", secs, millis);
	else // 9.123
		Com_sprintf(timeString, sizeof(timeString), "%d.%03d", secs, millis);

	char date[19] = { 0 };
	time_t now = time(NULL);
	if (now - record->date < 60 * 60 * 24)
		FormatLocalDateFromEpoch(now, date, sizeof(date), record->date);
	else
		FormatLocalDateFromEpoch(now, date, sizeof(date), record->date);

	trap_SendServerCommand(ent - g_entities, va(
		"print \"%s demo%s for ^5%s^7: %s in %s (^5%s^7), recorded %s\n\"",
		rankString, nameStrings[1][0] ? "s" : "", categoryName, combinedNameString, timeString, thisCategoryName, date));

	trap_SendServerCommand(ent - g_entities, va("chat \"^1Demo%s link^7\x19: " DEMOARCHIVE_BASE_MATCH_URL "\"\n", nameStrings[1][0] ? "s" : "", record->matchId));

	return qtrue;
}

void Cmd_TopTimes_f( gentity_t *ent ) {
	if ( !ent || !ent->client || g_gametype.integer != GT_SIEGE) {
		return;
	}

	if (!g_saveCaptureRecords.integer) {
		trap_SendServerCommand( ent - g_entities, "print \"Capture records are disabled.\n\"" );
		return;
	}

	if (!level.mapCaptureRecords.speedRunModeRuined)
		trap_SendServerCommand(ent - g_entities, "print \"^5Current run conditions: ^3Speedrun^7\n\"");
	else if (level.isLivePug == ISLIVEPUG_YES)
		trap_SendServerCommand(ent - g_entities, "print \"^5Current run conditions: ^2Live Pug^7\n\"");
	else
		trap_SendServerCommand(ent - g_entities, "print \"^5Current run conditions: ^1Any'/.^7\n\"");

	if ( level.mapCaptureRecords.readonly ) {
		trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_YELLOW"WARNING: Server settings are non-standard, new times won't be recorded!\n\"" );
	}

	CaptureCategoryFlags flags = 0;
	qboolean manuallySpecifiedFlags = qfalse, displayBothSpeedRunAndLivePugVariants = qfalse;

	if ( trap_Argc() > 1 ) {
		char buf[64];
		trap_Argv( 1, buf, sizeof( buf ) );

		if ( !Q_stricmp( buf, "maplist" ) ) {
			int page = 1;
			if ( trap_Argc() > 2 ) {
				trap_Argv( 2, buf, sizeof( buf ) );

				if ( Q_isanumber( buf ) ) { // is the 2nd argument a page number?...
					page = Com_Clampi(1, 999, atoi( buf ));
				} else { // 2nd argument isn't a page number, so it must be a category
					// prevent maplist with flags pertaining to a specific obj...
					// what's the point of getting the fastest obj 1 time on multiple maps?
					flags = GetRecordFlagsForString( NULL, buf, NULL, qtrue, qfalse );
					if (!flags) {
						trap_SendServerCommand( ent - g_entities, va("print \"Invalid category. Usage: /toptimes maplist [category]\n%s\n\"", TOPTIMES_HELPMSG));
						return;
					}

					if ( trap_Argc() > 3 ) { // 3rd argument must be the page number
						trap_Argv( 3, buf, sizeof( buf ) );
						page = Com_Clampi(1, 999, atoi(buf));
					}
				}
			}

			if (!flags) {
				flags = CAPTURERECORDFLAG_SPEEDRUN | CAPTURERECORDFLAG_FULLMAP; // default to entire-map speedruns
				trap_SendServerCommand(ent - g_entities, "print \"No category specified; defaulting to Speedrun FullMap.\n\"");
			}
			BestTimeContext context;
			context.entNum = ent - g_entities;
			context.hasPrinted = qfalse;
			G_DBListAllMapsCaptureRecords( flags, MAPLIST_MAPS_PER_PAGE, ( page - 1 ) * MAPLIST_MAPS_PER_PAGE, printBestTimeCallback, &context );
			
			if ( context.hasPrinted )
				trap_SendServerCommand( ent - g_entities, va( "print \"Viewing page %d.\nUsage: /toptimes maplist [category] [page]\n%s\n\"", page, TOPTIMES_HELPMSG) );
			else
				trap_SendServerCommand( ent - g_entities, va("print \"There aren't this many records! Try a lower page number.\nUsage: /toptimes maplist [category] [page]\n%s\n\"", TOPTIMES_HELPMSG));

			return;
		}
		else if (!Q_stricmp(buf, "rules") || !Q_stricmp(buf, "help") || !Q_stricmp(buf, "info")) {
			char *text =
				"^2Live pug^7 runs are for live pugs (even teams, etc.)\n"
				"^4Defensive^7 records are only recorded during live pugs.\n\n"
				"^3Speedrun^7 runs require there to be nobody on blue team, nobody extra on red, and no changing teams after the start of the round.\n"
				"While these conditions are intact, for consistent timings, you will spawn only at predetermined spawn points.\n"
				"This category contains several sub-categories:\n"
				"    ^5Co-op^7 runs require having a single teammate.\n"
				"    ^5<class>-Only^7 runs require playing only one class for the entire obj/map.\n"
				"    ^5One-shot^7 runs require completing the entire map in one life (no dying/selfkilling/changing class).\n\n"
				"^1Any'/.^7 runs are any that don't qualify for either of these two categories, or were done with skillboost.\n"
				"Objectives that can be completed simultaneously (e.g. stations) are combined.\n";
			trap_SendServerCommand(ent - g_entities, va("print \"%s\"", text));
			return;
		}
		else if (!Q_stricmp(buf, "latest")) {
			int page = 1;
			if (trap_Argc() > 2) {
				trap_Argv(2, buf, sizeof(buf));

				if (Q_isanumber(buf)) { // is the 2nd argument a page number?...
					page = Com_Clampi(1, 999, atoi(buf));
				}
				else { // 2nd argument isn't a page number, so it must be a category
				 // prevent maplist with flags pertaining to a specific obj...
				 // what's the point of getting the fastest obj 1 time on multiple maps?
					flags = GetRecordFlagsForString(NULL, buf, NULL, qtrue, qfalse);
					if (!flags) {
						trap_SendServerCommand(ent - g_entities, va("print \"Invalid category. Usage: /toptimes latest [category]\n%s\n\"", TOPTIMES_HELPMSG));
						return;
					}

					if (trap_Argc() > 3) { // 3rd argument must be the page number
						trap_Argv(3, buf, sizeof(buf));
						page = Com_Clampi(1, 999, atoi(buf));
					}
				}
			}

			BestTimeContext context;
			context.entNum = ent - g_entities;
			context.hasPrinted = qfalse;
			G_DBListLatestCaptureRecords(flags, MAPLIST_MAPS_PER_PAGE, (page - 1) * MAPLIST_MAPS_PER_PAGE, printLatestTimesCallback, &context);

			if (context.hasPrinted)
				trap_SendServerCommand(ent - g_entities, va("print \"Viewing page %d.\nUsage: /toptimes latest [category] [page]\n%s\n\"", page, TOPTIMES_HELPMSG));
			else
				trap_SendServerCommand(ent - g_entities, va("print \"There aren't this many records! Try a lower page number.\nUsage: /toptimes latest [category] [page]\n%s\n\"", TOPTIMES_HELPMSG));

			return;
		}
		else if (!Q_stricmp(buf, "demo")) {
			if (trap_Argc() < 4) {
				trap_SendServerCommand(ent - g_entities, va("print \"Usage: /toptimes demo [category] [rank #]\n%s\n\"", TOPTIMES_HELPMSG));
				return;
			}
			trap_Argv(2, buf, sizeof(buf));
			flags = GetRecordFlagsForString(level.mapCaptureRecords.mapname, buf, &displayBothSpeedRunAndLivePugVariants, qfalse, qfalse);
			if (!flags) {
				trap_SendServerCommand(ent - g_entities, va("print \"Invalid category. Usage: /toptimes demo [category] [rank #]\n%s\n\"", TOPTIMES_HELPMSG));
				return;
			}
			trap_Argv(3, buf, sizeof(buf));
			int desiredRank = 0;
			if (Q_isanumber(buf))
				desiredRank = atoi(buf);
			else if (tolower(buf[0]) == 'g')
				desiredRank = 1; // gold
			else if (tolower(buf[0]) == 's')
				desiredRank = 2; // silver
			else if (tolower(buf[0]) == 'b')
				desiredRank = 3; // bronze
			if (desiredRank < 1 || desiredRank > MAX_SAVED_RECORDS) {
				trap_SendServerCommand(ent - g_entities, va("print \"Invalid rank number (must be between 1 through %d). Usage: /toptimes demo [category] [rank #]\n%s\n\"", MAX_SAVED_RECORDS, TOPTIMES_HELPMSG));
				return;
			}
			if (!GetDemoURL(flags, desiredRank, ent)) {
				const char* categoryName = GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, flags, qfalse);
				trap_SendServerCommand(ent - g_entities, va("print \"Unable to find a demo for rank %d in category %s.\nUsage: /toptimes demo [category] [rank #]\n%s\n\"", desiredRank, categoryName, TOPTIMES_HELPMSG));
			}
			return;

		}
		else {
			// they typed an arg, but it wasn't "maplist" or "rules"; assume it's a category on the current map
			flags = GetRecordFlagsForString(level.mapCaptureRecords.mapname, buf, &displayBothSpeedRunAndLivePugVariants, qfalse, qfalse);
			if (!flags) {
				trap_SendServerCommand(ent - g_entities, va("print \"Invalid category. Usage: /toptimes maplist [category]\n%s\n\"", TOPTIMES_HELPMSG));
				return;
			}
			manuallySpecifiedFlags = qtrue;
		}
	}

	// determine which categories to print
	CaptureCategoryFlags categoriesToPrint[MAX_CATEGORIES_TO_PRINT] = { 0 };
	if (manuallySpecifiedFlags) { // the user specified a specific category they want to see
		if (displayBothSpeedRunAndLivePugVariants) { // if applicable, display both the speedrun and livepug variants of the specified category
			categoriesToPrint[0] = (flags | CAPTURERECORDFLAG_SPEEDRUN) & ~CAPTURERECORDFLAG_LIVEPUG;
			categoriesToPrint[1] = (flags | CAPTURERECORDFLAG_LIVEPUG) & ~CAPTURERECORDFLAG_SPEEDRUN;
		}
		else { // otherwise, just show the exact specified category
			categoriesToPrint[0] = flags;
		}
	}
	else { // no specific category specified; show four: current obj (both speedrun and livepug) and fullmap (both speedrun and livepug)
		CombinedObjNumber currentObjNum = level.mapCaptureRecords.lastCombinedObjCompleted + 1;
		if (!level.intermissiontime && currentObjNum >= 1 && currentObjNum <= MAX_SAVED_OBJECTIVES && !(level.numSiegeObjectivesOnMapCombined && currentObjNum > level.numSiegeObjectivesOnMapCombined)) { // double-check that the current obj number is valid
			categoriesToPrint[0] = (1 << currentObjNum) | CAPTURERECORDFLAG_SPEEDRUN;
			categoriesToPrint[1] = (1 << currentObjNum) | CAPTURERECORDFLAG_LIVEPUG;
			categoriesToPrint[2] = CAPTURERECORDFLAG_FULLMAP | CAPTURERECORDFLAG_SPEEDRUN;
			categoriesToPrint[3] = CAPTURERECORDFLAG_FULLMAP | CAPTURERECORDFLAG_LIVEPUG;
		}
		else { // intermission or invalid obj number somehow; just print fullmap
			categoriesToPrint[0] = CAPTURERECORDFLAG_FULLMAP | CAPTURERECORDFLAG_SPEEDRUN;
			categoriesToPrint[1] = CAPTURERECORDFLAG_FULLMAP | CAPTURERECORDFLAG_LIVEPUG;
		}
	}

	// do the printing
	qboolean atLeastOneCategoryValid = qfalse;
	for (int i = 0; i < MAX_CATEGORIES_TO_PRINT; i++) {
		if (!categoriesToPrint[i])
			continue;
		if (PrintCategory(categoriesToPrint[i], ent))
			atLeastOneCategoryValid = qtrue;
	}

	// confirm that we printed at least one
	if (!atLeastOneCategoryValid) {
		if (manuallySpecifiedFlags) {
			const char* categoryName = GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, flags, qfalse);
			trap_SendServerCommand(ent - g_entities, va("print \"No records for the ^3%s^7 category on this map yet!\n\"", categoryName));
		}
		else {
			trap_SendServerCommand(ent - g_entities, "print \"No speedrun or live pug records for the current objective/map yet!\n\"");
		}
	}

	// display a little extra help message so they know other possibilities with this command
	trap_SendServerCommand( ent - g_entities, va("print \"To view records for a specific category: /toptimes [category]\n%s\nFor a list of records on all maps: /toptimes maplist [category]\nFor rules: /toptimes rules\nTo get the demo URL for a record: /toptimes demo [category] [rank #]\nTo view the latest records: /toptimes latest\n\"", TOPTIMES_HELPMSG));
}

#define AddDesc(s)							\
	do {									\
		Q_strcat(desc, sizeof(desc), s);	\
	} while (0)

#define MAX_CLASSES_CHUNKS		4
#define CLASSES_CHUNK_SIZE		1000
#define MAX_CLASSES_SIZE		(MAX_CLASSES_CHUNKS * CLASSES_CHUNK_SIZE)

static char *GenerateSiegeClassDescription(siegeClass_t *scl) {
	if (!scl) {
		assert(qfalse);
		return "";
	}
	static char desc[MAX_CLASSES_SIZE] = { 0 };
	memset(&desc, 0, sizeof(desc));
	switch (scl->playerClass) {
	case 0:	AddDesc("^5Assault:^7");	break;
	case 1:	AddDesc("^5Scout:^7");		break;
	case 2:	AddDesc("^5Tech:^7");		break;
	case 3:	AddDesc("^5Jedi:^7");		break;
	case 4:	AddDesc("^5Demo:^7");		break;
	case 5:	AddDesc("^5HW:^7");			break;
	}
	if (scl->starthealth == scl->maxhealth)
		AddDesc(va(" ^1%d^7 HP", scl->starthealth));
	else
		AddDesc(va(" ^1%d/%d^7 HP", scl->starthealth, scl->maxhealth));

	if (scl->startarmor || scl->maxarmor) {
		if (scl->startarmor == scl->maxarmor)
			AddDesc(va(", ^2%d^7 Armor", scl->startarmor));
		else
			AddDesc(va(", ^2%d/%d^7 Armor", scl->startarmor, scl->maxarmor));
	}

	AddDesc(va(", %.02fx Speed", scl->speed));

	for (weapon_t w = WP_STUN_BATON; w < WP_NUM_WEAPONS; w++) {
		if (scl->weapons & (1 << w)) {
			switch (w) {
			case WP_STUN_BATON:			AddDesc(", Stun Baton");			break;
			case WP_MELEE:				AddDesc(", Melee");				break;
			case WP_BRYAR_PISTOL:		AddDesc(", Pistol");				break;
			case WP_BRYAR_OLD:			AddDesc(", Bryar");				break;
			case WP_BLASTER:			AddDesc(", Blaster");				break;
			case WP_DISRUPTOR:			AddDesc(", Disruptor");			break;
			case WP_BOWCASTER:			AddDesc(", Bowcaster");			break;
			case WP_REPEATER:			AddDesc(", Repeater");			break;
			case WP_DEMP2:				AddDesc(", Demp");				break;
			case WP_FLECHETTE:			AddDesc(", Golan");				break;
			case WP_ROCKET_LAUNCHER:	AddDesc(", Rockets");				break;
			case WP_THERMAL:			AddDesc(", Thermals");			break;
			case WP_TRIP_MINE:			AddDesc(", Mines");				break;
			case WP_DET_PACK:			AddDesc(", Detpacks");			break;
			case WP_CONCUSSION:			AddDesc(", Concussion");			break;
			}
		}
	}

	if (scl->weapons & (1 << WP_SABER)) {
		if (scl->saber2[0] || scl->saberStance & (1 << SS_DUAL)) {
			AddDesc(", Dual Sabers");
		}
		else if (scl->saberStance & (1 << SS_STAFF)) {
			AddDesc(", Staff");
		}
		else {
			AddDesc(", Saber");
			qboolean gotStyle = qfalse;
			for (saber_styles_t style = SS_FAST; style <= SS_TAVION; style++) {
				if (scl->saberStance & (1 << style)) {
					AddDesc(gotStyle ? "/" : " (");
					switch (style) {
					case SS_FAST:	AddDesc("Blue");	break;
					case SS_MEDIUM:	AddDesc("Yellow");	break;
					case SS_STRONG:	AddDesc("Red");		break;
					case SS_DESANN:	AddDesc("Desann");	break;
					case SS_TAVION:	AddDesc("Tavion");	break;
					}
					gotStyle = qtrue;
				}
			}
			if (gotStyle)
				AddDesc(")");
		}
	}

	for (forcePowers_t fp = FP_HEAL; fp < NUM_FORCE_POWERS; fp++) {
		if (scl->forcePowerLevels[fp]) {
			switch (fp) {
			case FP_HEAL:			AddDesc(", ^5Heal");				break;
			case FP_LEVITATION:		AddDesc(", ^5Jump");				break;
			case FP_SPEED:			AddDesc(", ^5Speed");				break;
			case FP_PUSH:			AddDesc(", ^5Push");				break;
			case FP_PULL:			AddDesc(", ^5Pull");				break;
			case FP_TELEPATHY:		AddDesc(", ^5Mind Trick");		break;
			case FP_GRIP:			AddDesc(", ^5Grip");				break;
			case FP_LIGHTNING:		AddDesc(", ^5Lightning");			break;
			case FP_RAGE:			AddDesc(", ^5Rage");				break;
			case FP_PROTECT:		AddDesc(", ^5Protect");			break;
			case FP_ABSORB:			AddDesc(", ^5Absorb");			break;
			case FP_TEAM_HEAL:		AddDesc(", ^5Team Heal");			break;
			case FP_TEAM_FORCE:		AddDesc(", ^5Team Energize");		break;
			case FP_DRAIN:			AddDesc(", ^5Drain");				break;
			case FP_SEE:			AddDesc(", ^5Sense");				break;
			case FP_SABER_OFFENSE:	AddDesc(", ^5Offense");			break;
			case FP_SABER_DEFENSE:	AddDesc(", ^5Defense");			break;
			case FP_SABERTHROW:		AddDesc(", ^5Throw");				break;
			}
			AddDesc(va(" %d^7", scl->forcePowerLevels[fp]));
		}
	}

	for (siegeClassFlags_t cfl = CFL_MORESABERDMG; cfl < NUM_CLASSFLAGS; cfl++) {
		if (scl->classflags & (1 << cfl)) {
			switch (cfl) {
			case CFL_MORESABERDMG:			AddDesc(", ^3More Saber Dmg^7");					break;
			case CFL_STRONGAGAINSTPHYSICAL:	AddDesc(", ^3Strong Against Physical^7");			break;
			case CFL_FASTFORCEREGEN:		AddDesc(", ^3Fast Force Regen^7");					break;
			case CFL_STATVIEWER:			AddDesc(", ^3Sees Allied HP/Armor/Ammo Bars^7");		break;
			case CFL_HEAVYMELEE:			AddDesc(", ^3Heavy Melee^7");						break;
			case CFL_SINGLE_ROCKET:			AddDesc(", ^3Single Rocket^7");						break;
			case CFL_CUSTOMSKEL:			AddDesc(", ^3Custom Skeleton^7");					break;
			case CFL_EXTRA_AMMO:			AddDesc(", ^3Double Ammo^7");						break;
			case CFL_SPIDERMAN:				AddDesc(", ^3Perma-Wallgrab^7");						break;
			case CFL_GRAPPLE:				AddDesc(", ^3Melee Grapple^7");						break;
			case CFL_KICK:					AddDesc(", ^3Melee Kick^7");							break;
			case CFL_WONDERWOMAN:			AddDesc(", ^3Wonder Woman^7");						break;
			case CFL_SMALLSHIELD:			AddDesc(", ^3Small Shield^7");						break;
			}
		}
	}

	if (scl->invenItems & (1 << HI_HEALTHDISP) && scl->invenItems & (1 << HI_AMMODISP))
		AddDesc(", ^3Health/Ammo Dispensing^7");
	else if (scl->invenItems & (1 << HI_HEALTHDISP))
		AddDesc(", ^3Health Dispensing^7");
	else if (scl->invenItems & (1 << HI_AMMODISP))
		AddDesc(", ^3Ammo Dispensing^7");

	if (scl->jetpackFreezeImmunity)
		AddDesc(", ^3Jetpack Freeze Immunity^7");
	if (scl->dispenseHealthpaks)
		AddDesc(", ^3Dispenses Health Packs^7");
	if (scl->audioMindTrick)
		AddDesc(", ^3Audio Mind Trick^7");
	if (scl->saberOffDamageBoost)
		AddDesc(", ^3Saber-Off Damage Boost^7");
	if (scl->shortBurstJetpack)
		AddDesc(", ^3Short-Burst Jetpack^7");
	if (scl->saberOffDamageBoost)
		AddDesc(", ^3Charging Demp Removes Spawn Shield^7");
	if (scl->pull1IfSaberInAir)
		AddDesc(", ^3Pull Level 1 If Saber In Air^7");
	if (scl->detKillDelay)
		AddDesc(va(", ^3%d ms Detkill Delay^7", scl->detKillDelay));

	if (scl->ammoblaster)
		AddDesc(va(", ^8%d Blaster Ammo^7", scl->ammoblaster));
	if (scl->ammopowercell)
		AddDesc(va(", ^8%d Powercell Ammo^7", scl->ammopowercell));
	if (scl->ammometallicbolts)
		AddDesc(va(", ^8%d Metallic Bolt Ammo^7", scl->ammometallicbolts));
	if (scl->ammorockets)
		AddDesc(va(", ^8%d Rocket Ammo^7", scl->ammorockets));
	if (scl->ammothermals)
		AddDesc(va(", ^8%d Thermal Ammo^7", scl->ammothermals));
	if (scl->ammotripmines)
		AddDesc(va(", ^8%d Mine Ammo^7", scl->ammotripmines));
	if (scl->ammodetpacks)
		AddDesc(va(", ^8%d Detpack Ammo^7", scl->ammodetpacks));

	for (holdable_t item = HI_SEEKER; item < HI_NUM_HOLDABLE; item++) {
		if (scl->invenItems & (1 << item)) {
			switch (item) {
			case HI_SEEKER:		AddDesc(", ^2Seeker^7");		break;
			case HI_SHIELD:		AddDesc(", ^2Shield^7");		break;
			case HI_MEDPAC:		AddDesc(", ^2Bacta^7");			break;
			case HI_MEDPAC_BIG:	AddDesc(", ^2Big Bacta^7");		break;
			case HI_BINOCULARS:	AddDesc(", ^2Binoculars^7");	break;
			case HI_JETPACK:	AddDesc(", ^2Jetpack^7");		break;
			case HI_EWEB:		AddDesc(", ^2Eweb^7");			break;
			case HI_CLOAK:		AddDesc(", ^2Cloak^7");			break;
			}
		}
	}

	if (scl->invenItems & (1 << HI_SENTRY_GUN)) {
		if (scl->maxSentries)
			AddDesc(va(", ^2%d Sentries^7", scl->maxSentries));
		else
			AddDesc(", ^2Sentry^7");
	}

	for (powerup_t pw = PW_QUAD; pw < PW_NUM_POWERUPS; pw++) {
		if (scl->powerups & (1 << pw)) {
			switch (pw) {
			case PW_FORCE_ENLIGHTENED_LIGHT:AddDesc(", ^6Light Enlightenment^7");	break;
			case PW_FORCE_ENLIGHTENED_DARK:	AddDesc(", ^6Dark Enlightenment^7");	break;
			case PW_FORCE_BOON:				AddDesc(", ^6Boon^7");					break;
			case PW_YSALAMIRI:				AddDesc(", ^6Ysalamiri^7");				break;
			}
		}
	}

	for (specialDamageParam_t *param = &scl->incomingDamageParam[0]; param - scl->incomingDamageParam < MAX_SPECIALDAMAGEPARAMETERS; param++) {
		if (!param->mods)
			continue;
		AddDesc(", ^1Incoming dmg modifier^7 (");
		if (param->mods == 0xFFFFFFFFFFFFFFFFll) {
			AddDesc("All Weapons");
		}
		else {
			qboolean gotMod = qfalse;
			for (meansOfDeath_t mod = MOD_STUN_BATON; mod <= MOD_SPECIAL_SENTRYBOMB; mod++) {
				if (param->mods & (1ll << (long long)mod)) {
					if (gotMod)
						AddDesc("/");
					switch (mod) {
					case MOD_STUN_BATON:			AddDesc("Stun Baton");				break;
					case MOD_MELEE:					AddDesc("Melee");					break;
					case MOD_SABER:					AddDesc("Saber");					break;
					case MOD_BRYAR_PISTOL:			AddDesc("Pistol Primary");			break;
					case MOD_BRYAR_PISTOL_ALT:		AddDesc("Pistol Altfire");			break;
					case MOD_BLASTER:				AddDesc("E11");						break;
					case MOD_TURBLAST:				AddDesc("Turbolaser");				break;
					case MOD_DISRUPTOR:				AddDesc("Disruptor Primary");		break;
					case MOD_DISRUPTOR_SNIPER:		AddDesc("Disruptor Altfire");		break;
					case MOD_BOWCASTER:				AddDesc("Bowcaster");				break;
					case MOD_REPEATER:				AddDesc("Repeater Primary");		break;
					case MOD_REPEATER_ALT:			AddDesc("Repeater Altfire Direct");	break;
					case MOD_REPEATER_ALT_SPLASH:	AddDesc("Repeater Altfire Splash");	break;
					case MOD_DEMP2:					AddDesc("Demp Primary");			break;
					case MOD_DEMP2_ALT:				AddDesc("Demp Altfire");			break;
					case MOD_FLECHETTE:				AddDesc("Golan Primary");			break;
					case MOD_FLECHETTE_ALT_SPLASH:	AddDesc("Golan Altfire");			break;
					case MOD_ROCKET:				AddDesc("Rocket Direct");			break;
					case MOD_ROCKET_SPLASH:			AddDesc("Rocket Splash");			break;
					case MOD_ROCKET_HOMING:			AddDesc("Rocket Altfire Direct");	break;
					case MOD_ROCKET_HOMING_SPLASH:	AddDesc("Rocket Altfire Splash");	break;
					case MOD_THERMAL:				AddDesc("Thermal Direct");			break;
					case MOD_THERMAL_SPLASH:		AddDesc("Thermal Splash");			break;
					case MOD_TRIP_MINE_SPLASH:		AddDesc("Lasermine");				break;
					case MOD_TIMED_MINE_SPLASH:		AddDesc("Proximity Mine");			break;
					case MOD_DET_PACK_SPLASH:		AddDesc("Detpack");					break;
					case MOD_VEHICLE:				AddDesc("Vehicle");					break;
					case MOD_CONC:					AddDesc("Concussion Primary");		break;
					case MOD_CONC_ALT:				AddDesc("Concussion Altfire");		break;
					case MOD_FORCE_DARK:			AddDesc("Dark Force");				break;
					case MOD_SENTRY:				AddDesc("Sentry");					break;
					case MOD_WATER:					AddDesc("Water");					break;
					case MOD_SLIME:					AddDesc("Slime");					break;
					case MOD_LAVA:					AddDesc("Lava");					break;
					case MOD_CRUSH:					AddDesc("Crush");					break;
					case MOD_TELEFRAG:				AddDesc("Telefrag");				break;
					case MOD_FALLING:				AddDesc("Falling");					break;
					case MOD_SUICIDE:				AddDesc("Suicide");					break;
					case MOD_TARGET_LASER:			AddDesc("Laser");					break;
					case MOD_TRIGGER_HURT:			AddDesc("Trigger");					break;
					case MOD_SPECIAL_SENTRYBOMB:	AddDesc("Sentry-Bomb");				break;
					}
					gotMod = qtrue;
				}
			}
		}

		if (param->otherEntType == -1) {
			AddDesc(", All Attacker Types");
		}
		else {
			for (int oet = OTHERENTTYPE_SELF; oet <= OTHERENTTYPE_ENEMY; oet++) {
				if (param->otherEntType & (1 << oet)) {
					switch (oet) {
					case OTHERENTTYPE_SELF:		AddDesc(", Self");	break;
					case OTHERENTTYPE_ALLY:		AddDesc(", Allies");	break;
					case OTHERENTTYPE_ENEMY:	AddDesc(", Enemies");	break;
					case OTHERENTTYPE_VEHICLE:	AddDesc(", Vehicles"); break;
					case OTHERENTTYPE_OTHER:	AddDesc(", Other");	break;
					}
				}
			}
		}

		if (param->damageMin == param->damageMax) {
			if (param->damageMin)
				AddDesc(va(", %d Dmg", param->damageMin));
			else
				AddDesc(", No Dmg");
		}
		else if (param->damageMin != 0x80000000) {
			AddDesc(va(", %d Min Dmg", param->damageMin));
		}
		else if (param->damageMax != 0x7FFFFFFF) {
			AddDesc(va(", %d Max Dmg", param->damageMax));
		}

		if (param->damageMultiplier != 1.0f) {
			if (param->damageMultiplier)
				AddDesc(va(", %0.2fx Dmg", param->damageMultiplier));
			else
				AddDesc(", No Dmg");
		}
		if (param->knockbackMultiplier != 1.0f) {
			if (param->knockbackMultiplier)
				AddDesc(va(", %0.2fx Knockback", param->knockbackMultiplier));
			else
				AddDesc(", No Knockback");
		}
		if (param->negativeDamageOk)
			AddDesc(", Can Heal");

		if (param->freeze == FREEZE_NO)
			AddDesc(", No Freezing");
		else if (param->freeze == FREEZE_YES)
			AddDesc(", Causes Freezing");

		if (param->onlyKnockback)
			AddDesc(", Only Knockback");

		if (param->jediSplashDamageReduction == JEDISPLASHDMGREDUCTION_YES)
			AddDesc(", Jedi Splash Damage Reduction");
		else if (param->jediSplashDamageReduction == JEDISPLASHDMGREDUCTION_NO)
			AddDesc(", No Jedi Splash Damage Reduction");

		if (param->nonJediSaberDamageIncrease == NONJEDISABERDMGINCREASE_YES)
			AddDesc(", Non-Jedi Saber Damage Increase");
		else if (param->nonJediSaberDamageIncrease == NONJEDISABERDMGINCREASE_NO)
			AddDesc(", No Non-Jedi Saber Damage Increase");

		AddDesc(")");
	}
	for (specialDamageParam_t *param = &scl->outgoingDamageParam[0]; param - scl->outgoingDamageParam < MAX_SPECIALDAMAGEPARAMETERS; param++) {
		if (!param->mods)
			continue;
		AddDesc(", ^1Outgoing dmg modifier^7 (");
		if (param->mods == 0xFFFFFFFFFFFFFFFFll) {
			AddDesc("All Damage Sources");
		}
		else {
			qboolean gotMod = qfalse;
			for (meansOfDeath_t mod = MOD_STUN_BATON; mod <= MOD_SPECIAL_SENTRYBOMB; mod++) {
				if (param->mods & (1ll << (long long)mod)) {
					if (gotMod)
						AddDesc("/");
					switch (mod) {
					case MOD_STUN_BATON:			AddDesc("Stun Baton");				break;
					case MOD_MELEE:					AddDesc("Melee");					break;
					case MOD_SABER:					AddDesc("Saber");					break;
					case MOD_BRYAR_PISTOL:			AddDesc("Pistol Primary");			break;
					case MOD_BRYAR_PISTOL_ALT:		AddDesc("Pistol Altfire");			break;
					case MOD_BLASTER:				AddDesc("E11");						break;
					case MOD_TURBLAST:				AddDesc("Turbolaser");				break;
					case MOD_DISRUPTOR:				AddDesc("Disruptor Primary");		break;
					case MOD_DISRUPTOR_SNIPER:		AddDesc("Disruptor Altfire");		break;
					case MOD_BOWCASTER:				AddDesc("Bowcaster");				break;
					case MOD_REPEATER:				AddDesc("Repeater Primary");		break;
					case MOD_REPEATER_ALT:			AddDesc("Repeater Altfire Direct");	break;
					case MOD_REPEATER_ALT_SPLASH:	AddDesc("Repeater Altfire Splash");	break;
					case MOD_DEMP2:					AddDesc("Demp Primary");			break;
					case MOD_DEMP2_ALT:				AddDesc("Demp Altfire");			break;
					case MOD_FLECHETTE:				AddDesc("Golan Primary");			break;
					case MOD_FLECHETTE_ALT_SPLASH:	AddDesc("Golan Altfire");			break;
					case MOD_ROCKET:				AddDesc("Rocket Direct");			break;
					case MOD_ROCKET_SPLASH:			AddDesc("Rocket Splash");			break;
					case MOD_ROCKET_HOMING:			AddDesc("Rocket Altfire Direct");	break;
					case MOD_ROCKET_HOMING_SPLASH:	AddDesc("Rocket Altfire Splash");	break;
					case MOD_THERMAL:				AddDesc("Thermal Direct");			break;
					case MOD_THERMAL_SPLASH:		AddDesc("Thermal Splash");			break;
					case MOD_TRIP_MINE_SPLASH:		AddDesc("Lasermine");				break;
					case MOD_TIMED_MINE_SPLASH:		AddDesc("Proximity Mine");			break;
					case MOD_DET_PACK_SPLASH:		AddDesc("Detpack");					break;
					case MOD_VEHICLE:				AddDesc("Vehicle");					break;
					case MOD_CONC:					AddDesc("Concussion Primary");		break;
					case MOD_CONC_ALT:				AddDesc("Concussion Altfire");		break;
					case MOD_FORCE_DARK:			AddDesc("Dark Force");				break;
					case MOD_SENTRY:				AddDesc("Sentry");					break;
					case MOD_WATER:					AddDesc("Water");					break;
					case MOD_SLIME:					AddDesc("Slime");					break;
					case MOD_LAVA:					AddDesc("Lava");					break;
					case MOD_CRUSH:					AddDesc("Crush");					break;
					case MOD_TELEFRAG:				AddDesc("Telefrag");				break;
					case MOD_FALLING:				AddDesc("Falling");					break;
					case MOD_SUICIDE:				AddDesc("Suicide");					break;
					case MOD_TARGET_LASER:			AddDesc("Laser");					break;
					case MOD_TRIGGER_HURT:			AddDesc("Trigger");					break;
					case MOD_SPECIAL_SENTRYBOMB:	AddDesc("Sentry-Bomb");				break;
					}
					gotMod = qtrue;
				}
			}
		}

		if (param->otherEntType == -1) {
			AddDesc(", All Target Types");
		}
		else {
			for (int oet = OTHERENTTYPE_SELF; oet <= OTHERENTTYPE_ENEMY; oet++) {
				if (param->otherEntType & (1 << oet)) {
					switch (oet) {
					case OTHERENTTYPE_SELF:		AddDesc(", Self");	break;
					case OTHERENTTYPE_ALLY:		AddDesc(", Allies");	break;
					case OTHERENTTYPE_ENEMY:	AddDesc(", Enemies");	break;
					case OTHERENTTYPE_VEHICLE:	AddDesc(", Vehicles"); break;
					case OTHERENTTYPE_OTHER:	AddDesc(", Other");	break;
					}
				}
			}
		}

		if (param->damageMin == param->damageMax) {
			if (param->damageMin)
				AddDesc(va(", %d Dmg", param->damageMin));
			else
				AddDesc(", No Dmg");
		}
		else if (param->damageMin != 0x80000000) {
			AddDesc(va(", %d Min Dmg", param->damageMin));
		}
		else if (param->damageMax != 0x7FFFFFFF) {
			AddDesc(va(", %d Max Dmg", param->damageMax));
		}

		if (param->damageMultiplier != 1.0f) {
			if (param->damageMultiplier)
				AddDesc(va(", %0.2fx Dmg", param->damageMultiplier));
			else
				AddDesc(", No Dmg");
		}
		if (param->knockbackMultiplier != 1.0f) {
			if (param->knockbackMultiplier)
				AddDesc(va(", %0.2fx Knockback", param->knockbackMultiplier));
			else
				AddDesc(", No Knockback");
		}
		if (param->negativeDamageOk)
			AddDesc(", Can Heal");

		if (param->freeze == FREEZE_NO) {
			AddDesc(", No Freezing");
		}
		else if (param->freeze == FREEZE_YES) {
			if (param->freeze == FREEZE_YES && param->freezeMin >= 0 && param->freezeMax > 0 && param->freezeMin <= param->freezeMax) {
				if (param->freezeMin == param->freezeMax)
					AddDesc(va(", Causes %dms Freezing", param->freezeMin));
				else
					AddDesc(va(", Causes %d-%dms Freezing", param->freezeMin, param->freezeMax));
			}
			else {
				AddDesc(", Causes Freezing");
			}
		}

		if (param->onlyKnockback)
			AddDesc(", Only Knockback");

		if (param->jediSplashDamageReduction == JEDISPLASHDMGREDUCTION_YES)
			AddDesc(", Jedi Splash Damage Reduction");
		else if (param->jediSplashDamageReduction == JEDISPLASHDMGREDUCTION_NO)
			AddDesc(", No Jedi Splash Damage Reduction");

		if (param->nonJediSaberDamageIncrease == NONJEDISABERDMGINCREASE_YES)
			AddDesc(", Non-Jedi Saber Damage Increase");
		else if (param->nonJediSaberDamageIncrease == NONJEDISABERDMGINCREASE_NO)
			AddDesc(", No Non-Jedi Saber Damage Increase");

		AddDesc(")");
	}

	return desc;
}

void Cmd_Classes_f(gentity_t *ent) {
	if (g_gametype.integer != GT_SIEGE)
		return;
	trap_SendServerCommand(ent - g_entities, va("print \"Classes for %s:\n\"", level.mapname));
	qboolean printedOne = qfalse;
	for (team_t team = TEAM_RED; team <= TEAM_BLUE; team++) {
		trap_SendServerCommand(ent - g_entities, va("print \"%s:\n\"", team == TEAM_RED ? "^1Offense^7" : "\n^4Defense^7")); // extra line break for second team

		for (stupidSiegeClassNum_t clNum = SSCN_ASSAULT; clNum <= SSCN_JEDI; clNum++) {
			siegeClass_t *scl = BG_SiegeGetClass(team, clNum);
			if (!scl)
				continue;
			char *desc = GenerateSiegeClassDescription(scl);
			if (!VALIDSTRING(desc)) {
				assert(qfalse);
				continue;
			}
			size_t len = strlen(desc);
			if (len <= CLASSES_CHUNK_SIZE) { // no chunking necessary
				trap_SendServerCommand(ent - g_entities, va("print \"%s^7\n\"", desc));
			}
			else { // chunk it
				int chunks = len / CLASSES_CHUNK_SIZE;
				if (len % CLASSES_CHUNK_SIZE > 0)
					chunks++;
				for (int i = 0; i < chunks && i < MAX_CLASSES_CHUNKS; i++) {
					char thisChunk[CLASSES_CHUNK_SIZE + 1];
					Q_strncpyz(thisChunk, desc + (i * CLASSES_CHUNK_SIZE), sizeof(thisChunk));
					if (thisChunk[0])
						trap_SendServerCommand(ent - g_entities, va("print \"%s%s\"", thisChunk, i == chunks - 1 ? "^7\n" : "")); // add ^7 and line break for last one
				}
			}
			printedOne = qtrue;
		}

	}
	if (!printedOne)
		trap_SendServerCommand(ent - g_entities, "print \"No classes found!\n\"");
}

#define NUM_EMOTES				(LEGS_TURN180)
#define NUM_EMOTE_COMBINATIONS	(NUM_EMOTES * NUM_EMOTES)
#define RANDOM_EMOTE_LEN		(5)
#define EMOTE_DEFAULT_TIME		(5000)
#include "xxhash.h"
extern qboolean PM_InKnockDown(playerState_t *ps);
void Cmd_Emote_f(gentity_t *ent) {
	if (!g_emotes.integer ||
		ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		ent->client->tempSpectate >= level.time ||
		ent->client->ps.pm_type == PM_DEAD ||
		ent->client->ps.pm_type == PM_SPECTATOR ||
		ent->client->ps.pm_type == PM_INTERMISSION ||
		level.intermissiontime ||
		ent->client->ps.weaponTime ||
		ent->client->ps.m_iVehicleNum ||
		ent->client->ps.pm_flags & PMF_STUCK_TO_WALL ||
		ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER ||
		ent->client->ps.fd.forceGripBeingGripped > level.time ||
		PM_InKnockDown(&ent->client->ps) ||
		ent->client->ps.saberLockTime >= level.time ||
		ent->client->ps.torsoTimer >= EMOTE_DEFAULT_TIME ||
		ent->client->ps.legsTimer >= EMOTE_DEFAULT_TIME ||
		ent->client->ps.pm_flags & PMF_FOLLOW ||
		ent->client->ps.emplacedIndex ||
		ent->client->ps.saberInFlight ||
		ent->client->ps.weaponstate == WEAPON_CHARGING ||
		ent->client->ps.weaponstate == WEAPON_CHARGING_ALT ||
		ent->flags & FL_NOTARGET ||
		ent->client->sess.siegeDuelInProgress ||
		ent->client->ps.duelInProgress ||
		ent->client->holdingObjectiveItem ||
		level.pause.state != PAUSE_NONE)
		return;

	char buf[MAX_STRING_CHARS] = { 0 };
	if (trap_Argc() >= 2)
		trap_Argv(1, buf, sizeof(buf));
	unsigned int hash;
	int torsoTimer = -1, legsTimer = -1;
	if (buf[0]) { // manually-specified animation string
		// allow manually-specified duration with a second argument
		if (trap_Argc() >= 3) {
			char durationBuf[8] = { 0 };
			trap_Argv(2, durationBuf, sizeof(durationBuf));
			int durationFromArg = atoi(durationBuf);
			if (durationFromArg >= 1000) {
				torsoTimer = durationFromArg;
				legsTimer = durationFromArg;
			}
			else {
				trap_SendServerCommand(ent - g_entities, va("print \"Usage: emote [keyword] [duration in milliseconds]  (duration must be at least 1000 ms)\n\""));
				return;
			}
		}
		hash = XXH32(buf, strlen(buf), 0x69420) % (unsigned)NUM_EMOTE_COMBINATIONS;
	}
	else { // no arg == generate random animations
		hash = rand() & 0xff;
		hash |= (rand() & 0xff) << 8;
		hash |= (rand() & 0xff) << 16;
		hash &= (unsigned)NUM_EMOTE_COMBINATIONS;
	}

	if (level.isLivePug != ISLIVEPUG_NO) {
		if (g_gametype.integer == GT_SIEGE && level.isLivePug != ISLIVEPUG_NO && g_siegeRespawn.integer >= 5 &&
			level.siegeRespawnCheck > level.time && level.siegeRespawnCheck - level.time <= 3000 && level.siegeRespawnCheck - level.time > 1000) {
			// if timer is almost up, just kill them immediately
			ent->client->emoted = qfalse;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
			ent->flags &= ~FL_GODMODE;
			player_die(ent, ent, ent, 100000, MOD_SUICIDE);
			return;
		}
		// set the emoted bool so that they will automatically sk + can't do certain things
		ent->client->emoted = qtrue;
	}


	unsigned int torsoAnim = hash / (unsigned)NUM_EMOTES;
	unsigned int legsAnim = hash % (unsigned)NUM_EMOTES;

	// regenerate the hash for some bugged ones
	while (legsAnim >= (unsigned)BOTH_ROLL_F && legsAnim <= (unsigned)BOTH_ROLL_R) {
		hash = XXH32(&hash, sizeof(hash), 0x69420) % (unsigned)NUM_EMOTE_COMBINATIONS;
		torsoAnim = hash / (unsigned)NUM_EMOTES;
		legsAnim = hash % (unsigned)NUM_EMOTES;
	}

	ent->client->ps.torsoAnim = (signed)torsoAnim;
	ent->client->ps.legsAnim = (signed)legsAnim;

	if (torsoTimer == -1 || legsTimer == -1) {
		torsoTimer = BG_AnimLength(ent->localAnimIndex, ent->client->ps.torsoAnim);
		legsTimer = BG_AnimLength(ent->localAnimIndex, ent->client->ps.legsAnim);
	}

	ent->client->ps.torsoTimer = Com_Clampi(1000, 10000, torsoTimer);
	ent->client->ps.legsTimer = Com_Clampi(1000, 10000, legsTimer);

	BG_ClearRocketLock(&ent->client->ps);
}

#ifdef NEWMOD_SUPPORT
static qboolean StringIsOnlyNumbers(const char *s) {
	if (!VALIDSTRING(s))
		return qfalse;
	for (const char *p = s; *p; p++) {
		if (*p < '0' || *p > '9')
			return qfalse;
	}
	return qtrue;
}

static XXH32_hash_t GetVchatHash(const char *modName, const char *msg, const char *fileName) {
	char buf[MAX_TOKEN_CHARS] = { 0 };
	Com_sprintf(buf, sizeof(buf), "%s%s%s%s", level.mapname, modName, msg, fileName);
	return XXH32(buf, strlen(buf), 0);
}

typedef enum vchatType_s {
	VCHATTYPE_TEAMWORK = 0,
	VCHATTYPE_GENERAL,
	VCHATTYPE_MEME
} vchatType_t;

#define VCHAT_ESCAPE_CHAR '$'
void Cmd_Vchat_f(gentity_t *sender) {
	char hashBuf[MAX_TOKEN_CHARS] = { 0 };
	trap_Argv(1, hashBuf, sizeof(hashBuf));
	if (!hashBuf[0] || !StringIsOnlyNumbers(hashBuf))
		return;
	XXH32_hash_t hash = (XXH32_hash_t)strtoul(hashBuf, NULL, 10);

	char *s = ConcatArgs(2);
	if (!VALIDSTRING(s) || !strchr(s, VCHAT_ESCAPE_CHAR))
		return;

	qboolean teamOnly = qfalse;
	vchatType_t type = VCHATTYPE_MEME;
	char modName[MAX_QPATH] = { 0 }, fileName[MAX_QPATH] = { 0 }, msg[200] = { 0 };
	// parse each argument
	for (char *r = s; *r; r++) {
		// skip to the next argument
		while (*r && *r != VCHAT_ESCAPE_CHAR)
			r++;
		if (!*r++)
			break;

		// copy this argument into a buffer
		char buf[MAX_STRING_CHARS] = { 0 }, *w = buf;
		while (*r && *r != VCHAT_ESCAPE_CHAR && w - buf < sizeof(buf) - 1)
			*w++ = *r++;
		if (!buf[0])
			break;

		if (!Q_stricmpn(buf, "mod=", 4) && buf[4])
			Q_strncpyz(modName, buf + 4, sizeof(modName));
		else if (!Q_stricmpn(buf, "file=", 5) && buf[5])
			Q_strncpyz(fileName, buf + 5, sizeof(fileName));
		else if (!Q_stricmpn(buf, "msg=", 4) && buf[4])
			Q_strncpyz(msg, buf + 4, sizeof(msg));
		else if (!Q_stricmpn(buf, "team=", 5) && buf[5])
			teamOnly = !!atoi(buf + 5);
		else if (!Q_stricmpn(buf, "t=", 2) && buf[2])
			type = atoi(buf + 2);

		if (!*r)
			break; // we reached the end of the line

		// the for loop will increment r here, taking us past the current escape char
	}

	if (!modName[0] || !fileName[0] || !msg[0])
		return;

	if (ChatLimitExceeded(sender, teamOnly ? SAY_TEAM : -1))
		return;

	Q_CleanString(modName, STRIP_COLOR);
	Q_CleanString(fileName, STRIP_COLOR);
#if 1
	Q_CleanString(msg, STRIP_COLOR);
#endif
	
	XXH32_hash_t expectedHash = GetVchatHash(modName, msg, fileName);
	if (hash != expectedHash)
		return;

	// convert global teamwork vchats and teamwork vchats among specs to memes
	team_t senderTeam = GetRealTeam(sender->client);
	if (type == VCHATTYPE_TEAMWORK && (!teamOnly || senderTeam == TEAM_SPECTATOR || senderTeam == TEAM_FREE))
		type = VCHATTYPE_MEME;

	G_LogPrintf("vchat: %d %s (%s/%s): %s/%s: %s\n",
		sender - g_entities,
		sender->client->pers.netname,
		teamOnly ? "team" : "global",
		type == VCHATTYPE_TEAMWORK ? "teamwork" : (type == VCHATTYPE_GENERAL ? "general" : "meme"),
		modName,
		fileName,
		msg);

	int senderLocation = 0;
	char chatLocation[64] = { 0 };
	char chatMessage[200] = { 0 };
	Q_strncpyz(chatMessage, msg, sizeof(chatMessage));
#if 1
	// get his location (disable this section to never send locations)
	if (g_gametype.integer >= GT_TEAM)
		senderLocation = Team_GetLocation(sender, chatLocation, sizeof(chatLocation));
#endif

	char chatSenderName[64] = { 0 };
	if (teamOnly) {
		Com_sprintf(chatSenderName, sizeof(chatSenderName), EC"(%s%c%c"EC")"EC": ", sender->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
#if 1 // disable this to disable all the extra siege stuff
		if (g_gametype.integer == GT_SIEGE) {
			if (level.inSiegeCountdown) {
				if (sender->client->siegeClass != -1 && sender->client->sess.sessionTeam == TEAM_SPECTATOR && sender->client->sess.siegeDesiredTeam &&
					(sender->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1 || sender->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2)) {
					// we are in spec (in countdown) with a desired team/class...put our class type as our "location"
					switch (bgSiegeClasses[sender->client->siegeClass].playerClass) {
					case 0:	Q_strncpyz(chatLocation, "Assault", sizeof(chatLocation));			break;
					case 1:	Q_strncpyz(chatLocation, "Scout", sizeof(chatLocation));			break;
					case 2:	Q_strncpyz(chatLocation, "Tech", sizeof(chatLocation));				break;
					case 3:	Q_strncpyz(chatLocation, "Jedi", sizeof(chatLocation));				break;
					case 4:	Q_strncpyz(chatLocation, "Demolitions", sizeof(chatLocation));		break;
					case 5:	Q_strncpyz(chatLocation, "Heavy Weapons", sizeof(chatLocation));	break;
					default:	senderLocation = 0;	chatLocation[0] = '\0';						break;
					}
				}
			}
			else { // not in siege countdown
				if (sender->client->sess.sessionTeam == SIEGETEAM_TEAM1 || sender->client->sess.sessionTeam == SIEGETEAM_TEAM2) {
					if (sender->client->tempSpectate > level.time || sender->health <= 0) { // ingame and dead
						Com_sprintf(chatMessage, sizeof(chatMessage), "^0(DEAD) ^5%s", chatMessage);
					}
					else if (g_improvedTeamchat.integer >= 2 && !level.intermissiontime) {
						if (sender->client->ps.stats[STAT_ARMOR])
							Com_sprintf(chatMessage, sizeof(chatMessage), "^7(%i/%i) ^5%s", sender->health, sender->client->ps.stats[STAT_ARMOR], chatMessage);
						else
							Com_sprintf(chatMessage, sizeof(chatMessage), "^7(%i) ^5%s", sender->health, chatMessage);
					}
				}
			}

			if (((sender->client->sess.sessionTeam == TEAM_SPECTATOR &&
				(!level.inSiegeCountdown ||
				(sender->client->sess.siegeDesiredTeam != SIEGETEAM_TEAM1 && sender->client->sess.siegeDesiredTeam != SIEGETEAM_TEAM2)) ||
				level.intermissiontime) && g_improvedTeamchat.integer) ||
				(level.zombies && sender->client->sess.sessionTeam == TEAM_BLUE)) {
				senderLocation = 0;
				chatLocation[0] = '\0';
			}
		}
#endif
	}
	else {
		Com_sprintf(chatSenderName, sizeof(chatSenderName), "%s%c%c"EC": ", sender->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
	}

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gclient_t *cl = &level.clients[i];
		if (cl->pers.connected != CON_CONNECTED)
			continue;

		if (sender->client != cl && cl->sess.ignoreFlags & (1 << (sender - g_entities))) {
			// this recipient has ignored the sender
			continue;
		}

		if (sender->client != cl && sender->client->sess.shadowMuted && !cl->sess.shadowMuted) {
			// the sender is shadowmuted and this recipient is not shadowmuted
			continue;
		}

		int locationToSend = 0;
		if (teamOnly) {
			team_t senderTeam = GetRealTeam(sender->client);
			team_t recipientTeam = GetRealTeam(cl);
			if (senderTeam != recipientTeam) { // it's a teamchat and this guy isn't on our team...
				if (recipientTeam == TEAM_SPECTATOR && cl->ps.persistant[PERS_TEAM] == senderTeam) {
					// but he is speccing our team, so it's okay
				}
				else {
					continue; // he is truly on a different team; don't send it to him
				}
			}
			locationToSend = senderLocation;
		}

		char *command;
		if (type == VCHATTYPE_MEME) {
			command = va("kls -1 -1 vcht \"cl=%d\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\"%s",
				sender - g_entities,
				modName,
				fileName,
				msg,
				type,
				teamOnly,
				teamOnly && locationToSend ? va(" \"loc=%d\"", locationToSend) : "");
		}
		else {
			if (teamOnly && locationToSend) {
				command = va("ltchat \"%s\" \"%s\" \"5\" \"%s\" \"%d\" \"vchat\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\" \"loc=%d\"",
					chatSenderName,
					chatLocation,
					chatMessage,
					sender - g_entities,
					modName,
					fileName,
					msg,
					type,
					teamOnly, // team only parameter is sent anyway so clients can display with team styling
					locationToSend);
			}
			else {
				if (teamOnly) {
					command = va("tchat \"%s^5%s\" \"%d\" \"vchat\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\"",
						chatSenderName,
						chatMessage,
						sender - g_entities,
						modName,
						fileName,
						msg,
						type,
						teamOnly); // team only parameter is sent anyway so clients can display with team styling);
				}
				else {
					command = va("chat \"%s^2%s\" \"%d\" \"vchat\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\"",
						chatSenderName,
						chatMessage,
						sender - g_entities,
						modName,
						fileName,
						msg,
						type,
						teamOnly); // team only parameter is sent anyway so clients can display with team styling);
				}
			}
		}
		trap_SendServerCommand(i, command);
	}
}
#endif

void Cmd_UsePack_f(gentity_t *ent) {
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)))
		return;

	if (ent->client->jetPackOn || ent->client->ps.groundEntityNum == ENTITYNUM_NONE) {
		ItemUse_Jetpack(ent);
	}
}

void Cmd_UseDispenser_f(gentity_t *ent) {
	if (!TryTossHealthPack(ent, qtrue))
		TryTossAmmoPack(ent, qtrue);
}

#if 0
void Cmd_UseHealing_f(gentity_t *ent) {
	trace_t		trace;
	vec3_t		src, dest, vf;
	vec3_t		viewspot;

	VectorCopy(ent->client->ps.origin, viewspot);
	viewspot[2] += ent->client->ps.viewheight;

	VectorCopy(viewspot, src);
	AngleVectors(ent->client->ps.viewangles, vf, NULL, NULL);

	VectorMA(src, USE_DISTANCE, vf, dest);

	//Trace ahead to find a valid target
	trap_Trace(&trace, src, vec3_origin, vec3_origin, dest, ent->s.number, MASK_OPAQUE | CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_ITEM | CONTENTS_CORPSE);

	//fixed bug when using client with slot 0, it skips needed part
	if (trace.fraction == 1.0f || trace.entityNum == ENTITYNUM_NONE)
	{
		return;
	}

	TryHealingSomething(ent, &g_entities[trace.entityNum], qtrue);
}
#endif

void Cmd_UseAnyBacta_f(gentity_t *ent) {
#if 0
	if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) && G_ItemUsable(&ent->client->ps, HI_MEDPAC_BIG) &&
		(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) && G_ItemUsable(&ent->client->ps, HI_MEDPAC)) {
		int health = ent->client->ps.stats[STAT_HEALTH], maxHealth;
		if (g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1 && bgSiegeClasses[ent->client->siegeClass].maxhealth)
			maxHealth = bgSiegeClasses[ent->client->siegeClass].maxhealth;
		else
			maxHealth = 100;
		if (maxHealth - health >= 50) {
			ItemUse_MedPack_Big(ent);
			G_AddEvent(ent, EV_USE_ITEM0 + HI_MEDPAC_BIG, 0);
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC_BIG);
		}
		else {
			ItemUse_MedPack(ent);
			G_AddEvent(ent, EV_USE_ITEM0 + HI_MEDPAC, 0);
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC);
		}
	}
	else {
#endif
		if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) && G_ItemUsable(&ent->client->ps, HI_MEDPAC_BIG)) {
			ItemUse_MedPack_Big(ent);
			G_AddEvent(ent, EV_USE_ITEM0 + HI_MEDPAC_BIG, 0);
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC_BIG);
		}
		else if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) && G_ItemUsable(&ent->client->ps, HI_MEDPAC)) {
			ItemUse_MedPack(ent);
			G_AddEvent(ent, EV_USE_ITEM0 + HI_MEDPAC, 0);
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC);
		}
#if 0
	}
#endif
}

/*
=================
Cmd_Stats_f
=================
*/
void Cmd_Stats_f( gentity_t *ent ) {

	}

int G_ItemUsable(playerState_t *ps, int forcedUse)
{
	vec3_t fwd, fwdorg, dest, pos;
	vec3_t yawonly;
	vec3_t mins, maxs;
	vec3_t trtest;
	trace_t tr;

	//is client alive???
	if (ps->stats[STAT_HEALTH] <= 0){
		return 0;
	}

	if (ps->m_iVehicleNum)
	{
		return 0;
	}
	
	if (ps->pm_flags & PMF_USE_ITEM_HELD)
	{ //force to let go first
		return 0;
	}

	if (!forcedUse)
	{
		forcedUse = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;
	}

	if (!BG_IsItemSelectable(ps, forcedUse))
	{
		return 0;
	}

	if (&g_entities[ps->clientNum] && g_entities[ps->clientNum].client->sess.siegeDuelInProgress)
	{
		return 0;
	}

	switch (forcedUse)
	{
	case HI_MEDPAC:
	case HI_MEDPAC_BIG:
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
		{
			return 0;
		}

		if (ps->stats[STAT_HEALTH] <= 0)
		{
			return 0;
		}

		return 1;
	case HI_SEEKER:
		if (ps->eFlags & EF_SEEKERDRONE)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SEEKER_ALREADYDEPLOYED);
			return 0;
		}

		return 1;
	case HI_SENTRY_GUN:
		if (ps->fd.sentryDeployed)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_ALREADYPLACED);
			return 0;
		}

		yawonly[ROLL] = 0;
		yawonly[PITCH] = 0;
		yawonly[YAW] = ps->viewangles[YAW];

		VectorSet( mins, -8, -8, 0 );
		VectorSet( maxs, 8, 8, 24 );

		AngleVectors(yawonly, fwd, NULL, NULL);

		fwdorg[0] = ps->origin[0] + fwd[0]*64;
		fwdorg[1] = ps->origin[1] + fwd[1]*64;
		fwdorg[2] = ps->origin[2] + fwd[2]*64;

		trtest[0] = fwdorg[0] + fwd[0]*16;
		trtest[1] = fwdorg[1] + fwd[1]*16;
		trtest[2] = fwdorg[2] + fwd[2]*16;

		trap_Trace(&tr, ps->origin, mins, maxs, trtest, ps->clientNum, MASK_PLAYERSOLID);

		if ((tr.fraction != 1 && tr.entityNum != ps->clientNum) || tr.startsolid || tr.allsolid)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_NOROOM);
			return 0;
		}

		return 1;
	case HI_SHIELD:
		mins[0] = -8;
		mins[1] = -8;
		mins[2] = 0;

		maxs[0] = 8;
		maxs[1] = 8;
		maxs[2] = 8;

		AngleVectors (ps->viewangles, fwd, NULL, NULL);
		fwd[2] = 0;
		VectorMA(ps->origin, 64, fwd, dest);
		trap_Trace(&tr, ps->origin, mins, maxs, dest, ps->clientNum, MASK_SHOT );
		if (tr.fraction > 0.9 && !tr.startsolid && !tr.allsolid)
		{
			VectorCopy(tr.endpos, pos);
			VectorSet( dest, pos[0], pos[1], pos[2] - 4096 );
			trap_Trace( &tr, pos, mins, maxs, dest, ps->clientNum, MASK_SOLID );
			if ( !tr.startsolid && !tr.allsolid )
			{
				return 1;
			}
		}
		G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SHIELD_NOROOM);
		return 0;
	case HI_JETPACK: //do something?
		return 1;
	case HI_HEALTHDISP:
		return 1;
	case HI_AMMODISP:
		return 1;
	case HI_EWEB:
		return 1;
	case HI_CLOAK:
		return 1;
	default:
		return 1;
	}
}

void saberKnockDown(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

void Cmd_ToggleSaber_f(gentity_t *ent)
{
	if (ent->client->ps.fd.forceGripCripple)
	{ //if they are being gripped, don't let them unholster their saber
		if (ent->client->ps.saberHolstered)
		{
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		if (ent->client->ps.saberEntityNum)
		{ //turn it off in midair
			saberKnockDown(&g_entities[ent->client->ps.saberEntityNum], ent, ent);
		}
		return;
	}

	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.saberLockTime >= level.time)
	{
		return;
	}

	if (ent->client && ent->client->ps.weaponTime < 1)
	{
		if (ent->client->ps.saberHolstered == 2)
		{
			ent->client->ps.saberHolstered = 0;
			if (ent->client->saber[0].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			}
			if (ent->client->saber[1].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
			}
		}
		else
		{
			ent->client->ps.saberHolstered = 2;
			if (ent->client->saber[0].soundOff)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
			}
			if (ent->client->saber[1].soundOff &&
				ent->client->saber[1].model[0])
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
			}
			//prevent anything from being done for 400ms after holster
			ent->client->ps.weaponTime = 400;
		}
	}
}

extern vmCvar_t		d_saberStanceDebug;

extern qboolean WP_SaberCanTurnOffSomeBlades( saberInfo_t *saber );
void Cmd_SaberAttackCycle_f(gentity_t *ent)
{
	int selectLevel = 0;
	qboolean usingSiegeStyle = qfalse;
	
	if ( !ent || !ent->client )
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER) {
		if (TryTossHealthPack(ent, qtrue))
			return;
		if (TryTossAmmoPack(ent, qtrue))
			return;
	}

	if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
	{ //no cycling for akimbo
		if ( WP_SaberCanTurnOffSomeBlades( &ent->client->saber[1] ) )
		{//can turn second saber off 
			if ( ent->client->ps.saberHolstered == 1 )
			{//have one holstered
				//unholster it
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				ent->client->ps.saberHolstered = 0;
				//g_active should take care of this, but...
				ent->client->ps.fd.saberAnimLevel = SS_DUAL;
			}
			else if ( ent->client->ps.saberHolstered == 0 )
			{//have none holstered
				if ( (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
				{//can't turn it off manually
				}
				else if ( ent->client->saber[1].bladeStyle2Start > 0
					&& (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
				{//can't turn it off manually
				}
				else
				{
					//turn it off
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
					ent->client->ps.saberHolstered = 1;
					//g_active should take care of this, but...
					ent->client->ps.fd.saberAnimLevel = SS_FAST;
				}
			}

			if (d_saberStanceDebug.integer)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle dual saber blade.\n\"") );
			}
			return;
		}
	}
	else if (ent->client->saber[0].numBlades > 1
		&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[0] ) )
	{ //use staff stance then.
		if ( ent->client->ps.saberHolstered == 1 )
		{//second blade off
			if ( ent->client->ps.saberInFlight )
			{//can't turn second blade back on if it's in the air, you naughty boy!
				if (d_saberStanceDebug.integer)
				{
					trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade in air.\n\"") );
				}
				return;
			}
			//turn it on
			G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			ent->client->ps.saberHolstered = 0;
			//g_active should take care of this, but...
			if ( ent->client->saber[0].stylesForbidden )
			{//have a style we have to use
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
				if ( ent->client->ps.weaponTime <= 0 )
				{ //not busy, set it now
					ent->client->ps.fd.saberAnimLevel = selectLevel;
				}
				else
				{ //can't set it now or we might cause unexpected chaining, so queue it
					ent->client->saberCycleQueue = selectLevel;
				}
			}
		}
		else if ( ent->client->ps.saberHolstered == 0 )
		{//both blades on
			if ( (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
			{//can't turn it off manually
			}
			else if ( ent->client->saber[0].bladeStyle2Start > 0
				&& (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
			{//can't turn it off manually
			}
			else
			{
				//turn second one off
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				ent->client->ps.saberHolstered = 1;
				//g_active should take care of this, but...
				if ( ent->client->saber[0].singleBladeStyle != SS_NONE )
				{
					if ( ent->client->ps.weaponTime <= 0 )
					{ //not busy, set it now
						ent->client->ps.fd.saberAnimLevel = ent->client->saber[0].singleBladeStyle;
					}
					else
					{ //can't set it now or we might cause unexpected chaining, so queue it
						ent->client->saberCycleQueue = ent->client->saber[0].singleBladeStyle;
					}
				}
			}
		}
		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade.\n\"") );
		}
		return;
	}

	if (ent->client->saberCycleQueue)
	{ //resume off of the queue if we haven't gotten a chance to update it yet
		selectLevel = ent->client->saberCycleQueue;
	}
	else
	{
		selectLevel = ent->client->ps.fd.saberAnimLevel;
	}

	if (g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		bgSiegeClasses[ent->client->siegeClass].saberStance)
	{ //we have a flag of useable stances so cycle through it instead
		int i = selectLevel+1;

		usingSiegeStyle = qtrue;

		while (i != selectLevel)
		{ //cycle around upward til we hit the next style or end up back on this one
			if (i >= SS_NUM_SABER_STYLES)
			{ //loop back around to the first valid
				i = SS_FAST;
			}

			if (bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << i))
			{ //we can use this one, select it and break out.
				selectLevel = i;
				break;
			}
			i++;
		}

		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle given class stance.\n\"") );
		}
	}
	else
	{
		int maxLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
		selectLevel++;

		if ( g_balanceSaber.integer & SB_OFFENSE ) {
			if ( g_balanceSaber.integer & SB_OFFENSE_TAV_DES ) {
				maxLevel += 2; // level 2 gives SS_DESANN, level 3 gives SS_TAVION
			} else {
				maxLevel = SS_STRONG; // all stances are available regardless of the level
			}
		}

		if ( selectLevel > maxLevel )
		{
			selectLevel = FORCE_LEVEL_1;
		}
		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle stance normally.\n\"") );
		}
	}

	if ( !usingSiegeStyle )
	{
		//make sure it's valid, change it if not
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
	}

	if (ent->client->ps.weaponTime <= 0)
	{ //not busy, set it now
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = selectLevel;
	}
	else
	{ //can't set it now or we might cause unexpected chaining, so queue it
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = selectLevel;
	}
}

qboolean G_OtherPlayersDueling(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && (ent->client->ps.duelInProgress || ent->client->sess.siegeDuelInProgress))
		{
			return qtrue;
		}
		i++;
	}

	return qfalse;
}

#ifdef NEWMOD_SUPPORT
static void Cmd_Svauth_f( gentity_t *ent ) {
	if ( trap_Argc() < 2 ) {
		return;
	}

	if ( !ent || !ent->client ) {
		return;
	}

	if ( !( ent->client->sess.auth > PENDING && ent->client->sess.auth < AUTHENTICATED ) ) {
		return;
	}

	do {
		char encryptedSvauth[CRYPTO_CIPHER_HEX_SIZE];
		trap_Argv( 1, encryptedSvauth, sizeof( encryptedSvauth ) );

		char decryptedSvauth[CRYPTO_CIPHER_RAW_SIZE];
		if ( Crypto_Decrypt( &level.publicKey, &level.secretKey, encryptedSvauth, decryptedSvauth, sizeof( decryptedSvauth ) ) ) {
			G_HackLog( S_COLOR_RED"Failed to decrypt svauth command from client %d\n", ent - g_entities );
			break;
		}

		char *s;

		if ( ent->client->sess.auth == CLANNOUNCE ) {
			int clientKeys[2];

			s = Info_ValueForKey( decryptedSvauth, "ck1" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses ck1\n", ent - g_entities );
				break;
			}
			clientKeys[0] = atoi( s );

			s = Info_ValueForKey( decryptedSvauth, "ck2" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses ck2\n", ent - g_entities );
				break;
			}
			clientKeys[1] = atoi( s );

#define RandomConfirmationKey()	( ( rand() << 16 ) ^ rand() ^ trap_Milliseconds() )
			ent->client->sess.serverKeys[0] = RandomConfirmationKey();
			ent->client->sess.serverKeys[1] = RandomConfirmationKey();

			trap_SendServerCommand( ent - g_entities, va( "kls -1 -1 \"clauth\" %d %d %d",
				clientKeys[0] ^ clientKeys[1], ent->client->sess.serverKeys[0], ent->client->sess.serverKeys[1] ) );
			ent->client->sess.auth++;
#ifdef _DEBUG
			G_Printf( "Got keys %d ^ %d = %d from client %d, sent %d and %d\n",
				clientKeys[0], clientKeys[1], clientKeys[0] ^ clientKeys[1], ent - g_entities, ent->client->sess.serverKeys[0], ent->client->sess.serverKeys[1]
			);
#endif
			return;
		} else if ( ent->client->sess.auth == CLAUTH ) {
			int serverKeysXor;

			s = Info_ValueForKey( decryptedSvauth, "skx" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses skx\n", ent - g_entities );
				break;
			}
			serverKeysXor = atoi( s );

			s = Info_ValueForKey( decryptedSvauth, "cid" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses cid\n", ent - g_entities );
				break;
			}

			if ( ( ent->client->sess.serverKeys[0] ^ ent->client->sess.serverKeys[1] ) != serverKeysXor ) {
				G_HackLog( S_COLOR_RED"Client %d failed the server key check!\n", ent - g_entities );
				break;
			}

			Crypto_Hash( s, ent->client->sess.cuidHash, sizeof( ent->client->sess.cuidHash ) );
			G_Printf( "Newmod client %d authenticated successfully (cuid hash: %s)\n", ent - g_entities, ent->client->sess.cuidHash );
			ent->client->sess.auth++;
			ClientUserinfoChanged( ent - g_entities );

			return;
		}
	} while ( 0 );

	// if we got here, auth failed
	ent->client->sess.auth = INVALID;
	G_LogPrintf( "Client %d failed newmod authentication (at state: %d)\n", ent - g_entities, ent->client->sess.auth );
}

static void Cmd_VchatDl_f( gentity_t* ent ) {
	// no longer used
}

static char vchatListArgs[MAX_STRING_CHARS] = { 0 };

// use -1 for everyone
void SendVchatList(int clientNum) {
	if (!VALIDSTRING(vchatListArgs))
		return;

	trap_SendServerCommand(clientNum, va("kls -1 -1 vchl %s", vchatListArgs));
}

#define VCHATPACK_CONFIG_FILE	"vchatpacks.config"

void G_InitVchats() {
	fileHandle_t f;
	int len = trap_FS_FOpenFile(VCHATPACK_CONFIG_FILE, &f, FS_READ);

	memset(vchatListArgs, 0, sizeof(vchatListArgs));

	if (!f || len <= 0) {
		Com_Printf(VCHATPACK_CONFIG_FILE" not found, vchats will be unavailable\n");
		return;
	}

	char* buf = (char*)malloc(len + 1);
	trap_FS_Read(buf, len, f);
	buf[len] = '\0';
	trap_FS_FCloseFile(f);

	char parseBuf[MAX_STRING_CHARS] = { 0 };
	int numParsed = 0;

	cJSON* root = cJSON_Parse(buf);
	if (root) {
		cJSON* download_base = cJSON_GetObjectItemCaseSensitive(root, "download_base");
		if (cJSON_IsString(download_base) && VALIDSTRING(download_base->valuestring) && !strchr(download_base->valuestring, ' ')) {
			Q_strcat(parseBuf, sizeof(parseBuf), va("\"%s\"", download_base->valuestring));

			cJSON* packs = cJSON_GetObjectItemCaseSensitive(root, "packs");
			if (cJSON_IsArray(packs)) {
				int i;
				for (i = 0; i < cJSON_GetArraySize(packs); ++i) {
					cJSON* pack = cJSON_GetArrayItem(packs, i);

					cJSON* pack_name = cJSON_GetObjectItemCaseSensitive(pack, "pack_name");
					cJSON* file_name = cJSON_GetObjectItemCaseSensitive(pack, "file_name");
					cJSON* version = cJSON_GetObjectItemCaseSensitive(pack, "version");

					if (cJSON_IsString(pack_name) && VALIDSTRING(pack_name->valuestring) && !strchr(pack_name->valuestring, ' ') &&
						cJSON_IsString(file_name) && VALIDSTRING(file_name->valuestring) && !strchr(file_name->valuestring, ' ') &&
						cJSON_IsNumber(version) && version > 0)
					{
						Q_strcat(parseBuf, sizeof(parseBuf), va(" %s %s %d", pack_name->valuestring, file_name->valuestring, version->valueint));
						++numParsed;
					}
				}
			}
		}
	}

	cJSON_Delete(root);
	free(buf);

	if (!numParsed) {
		Com_Printf("WARNING: No vchat pack parsed in "VCHATPACK_CONFIG_FILE", invalid format?\n");
		return;
	}

	Com_Printf("Parsed %d vchat packs from "VCHATPACK_CONFIG_FILE"\n", numParsed);

	Q_strncpyz(vchatListArgs, parseBuf, sizeof(vchatListArgs));
	SendVchatList(-1);
}

void Cmd_VchatList_f(gentity_t* ent) {
	SendVchatList(ent - g_entities);
}
#endif

typedef struct
{
	int entNum;

} AliasesContext;

void listAliasesCallback(void* context,
	const char* name,
	int duration)
{
	AliasesContext* thisContext = (AliasesContext*)context;
	trap_SendServerCommand(thisContext->entNum, va("print \"  %s"S_COLOR_WHITE" (%i).\n\"", name, duration));
}

void singleAliasCallback(void* context,
	const char* name,
	int duration)
{
	AliasesContext* thisContext = (AliasesContext*)context;
	trap_SendServerCommand(thisContext->entNum, va("print \""S_COLOR_WHITE"* %s"S_COLOR_WHITE"\"", name));
}

void Cmd_WhoIs_f( gentity_t* ent ) {
	int clientNum = -1;
	if (trap_Argc() >= 2) {
		char buf[64] = { 0 };
		trap_Argv(1, buf, sizeof(buf));
		gentity_t *found = found = G_FindClient(buf);

		if (!found || !found->client) {
			trap_SendServerCommand(ent - g_entities,
				va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"", buf));
			return;
		}
		clientNum = found - g_entities;
	}

	Table *t = Table_Initialize(qfalse);

	for (int i = 0; i < level.maxclients; i++) {
		if (!level.clients[i].pers.connected || (clientNum != -1 && i != clientNum))
			continue;
		Table_DefineRow(t, &level.clients[i]);
	}

	Table_DefineColumn(t, "#", TableCallback_ClientNum, qfalse, 2);
	Table_DefineColumn(t, "Name", TableCallback_Name, qtrue, g_maxNameLength.integer);
	Table_DefineColumn(t, "Alias", TableCallback_Alias, qtrue, g_maxNameLength.integer);
	Table_DefineColumn(t, "Country", TableCallback_Country, qtrue, 64);

	char buf[4096] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf));
	Table_Destroy(t);

	PrintIngame(ent - g_entities, buf);
}
#define MAX_STATS			16
#define STATS_ROW_SEPARATOR	"-"

typedef enum {
	STAT_NONE = 0, // types after this are RIGHT aligned
	STAT_BLANK, // only serves as caption, no value
	STAT_FLOAT,
	STAT_INT,
	STAT_INT_LOWERBETTER,
	STAT_DURATION,
	STAT_DURATION_LOWERBETTER,
	STAT_DURATION_PAIR1,
	STAT_DURATION_PAIR1_LOWERBETTER,
	STAT_OBJ_DURATION, // times with value of 0 are hidden, unless they have red color, which prints held time + "(DNF)" (did not finish)
	STAT_INT_PAIR1,
	STAT_INT_PAIR1_LOWERBETTER,
	STAT_LEFT_ALIGNED, // types after this are LEFT aligned
	STAT_DURATION_PAIR2,
	STAT_DURATION_PAIR2_LOWERBETTER,
	STAT_INT_PAIR2,
	STAT_INT_PAIR2_LOWERBETTER
} StatType;

typedef struct {
	char *cols[MAX_STATS]; // column names, strlen gives the width so use spaces for larger cols
	StatType types[MAX_STATS]; // type of data
} StatsDesc;

typedef struct {
	union {
		int		iValue;
		float	fValue;
	};
	char *forceColor;
} Stat;

#define FORMAT_FLOAT( i )				va( "%0.1f", *(float *)&i ) /*hackerman*/
#define FORMAT_INT( i )					va( "%d", i )
#define FORMAT_PAIRED_INT( i )			va( "%d"S_COLOR_WHITE"/", i )
#define FORMAT_MINS_SECS( m, s )		va( "%d:%02d", m, s )
#define FORMAT_PAIRED_MINS_SECS( m,s )	va( "%d:%02d"S_COLOR_WHITE"/", m, s )
#define FORMAT_SECS( s )				va( "0:%02d", s )
#define FORMAT_PAIRED_SECS( s )			va( "0:%02d"S_COLOR_WHITE"/", s )

static char* GetFormattedValue( int value, StatType type ) {
	switch ( type ) {
	case STAT_FLOAT: return FORMAT_FLOAT( value );
	case STAT_INT: case STAT_INT_LOWERBETTER: return FORMAT_INT( value );
	case STAT_INT_PAIR1: case STAT_INT_PAIR1_LOWERBETTER: return FORMAT_PAIRED_INT( value );
	case STAT_INT_PAIR2: case STAT_INT_PAIR2_LOWERBETTER: return FORMAT_INT( value );
	case STAT_DURATION: case STAT_DURATION_PAIR1: case STAT_DURATION_PAIR1_LOWERBETTER: case STAT_DURATION_PAIR2: case STAT_DURATION_PAIR2_LOWERBETTER: case STAT_DURATION_LOWERBETTER: case STAT_OBJ_DURATION: {
		int secs = value / 1000;
		int mins = secs / 60;

		// more or less than a minute?
		if ( value >= 60000 ) {
			secs %= 60;
			if (type == STAT_DURATION_PAIR1 || type == STAT_DURATION_PAIR1_LOWERBETTER)
				return FORMAT_PAIRED_MINS_SECS( mins, secs );
			else
				return FORMAT_MINS_SECS( mins, secs );
		} else {
			if (type == STAT_DURATION_PAIR1 || type == STAT_DURATION_PAIR1_LOWERBETTER)
				return FORMAT_PAIRED_SECS( secs );
			else
				return FORMAT_SECS( secs );
		}
	}
	default: return "0"; // should never happen
	}
}

#define TypeIsLowerBetter(t) (t == STAT_DURATION_LOWERBETTER || t == STAT_DURATION_PAIR1_LOWERBETTER || t == STAT_DURATION_PAIR2_LOWERBETTER || t == STAT_INT_LOWERBETTER || t == STAT_INT_PAIR1_LOWERBETTER || t == STAT_INT_PAIR2_LOWERBETTER)
#define TypeIsPair1(t) (t == STAT_DURATION_PAIR1 || t == STAT_DURATION_PAIR1_LOWERBETTER || t == STAT_INT_PAIR1 || t == STAT_INT_PAIR1_LOWERBETTER)
#define GetStatColor( s, b ) ( b && b == s ? S_COLOR_GREEN : S_COLOR_WHITE )
#define GetStatColorZeroOkay( s, b ) ( b == s ? S_COLOR_GREEN : S_COLOR_WHITE )

static void PrintClientStats( const int id, const char *name, StatsDesc desc, Stat *stats, Stat *bestStats, const int nameCols, char* outputBuffer, size_t outSize ) {
	int i, nameLen = 0;
	char s[MAX_STRING_CHARS];

	nameLen = Q_PrintStrlen( name );

	Com_sprintf( s, sizeof( s ), name );

	// fill up the gaps left by the bigger names
	if ( nameLen < nameCols ) {
		for ( i = 0; i < nameCols - nameLen; ++i )
			Q_strcat( s, sizeof( s ), " " );
	}

	// write all formatted stats
	for ( i = 0; i < MAX_STATS; ++i ) {
		if (desc.types[i] == STAT_NONE)
			continue;
		char overrideStr[MAX_STRING_CHARS] = { 0 };
		if (desc.types[i] == STAT_OBJ_DURATION && VALIDSTRING(stats[i].forceColor) && !Q_stricmp(stats[i].forceColor, S_COLOR_RED)) {
			G_ParseMilliseconds(stats[i].iValue, overrideStr, sizeof(overrideStr));
			Q_strncpyz(overrideStr, va("%s (DNF)", overrideStr), sizeof(overrideStr));
		}
		Q_strcat( s, sizeof( s ), va( desc.types[i] > STAT_LEFT_ALIGNED ? "%s%-*s" : " %s%*s",
			VALIDSTRING(stats[i].forceColor) ? stats[i].forceColor : (TypeIsLowerBetter(desc.types[i]) ? GetStatColorZeroOkay(stats[i].iValue, bestStats[i].iValue) : (desc.types[i] == STAT_FLOAT ? GetStatColor(stats[i].fValue, bestStats[i].fValue) : GetStatColor( stats[i].iValue, bestStats[i].iValue ))), // green if the best, white otherwise
			Q_PrintStrlen( desc.cols[i] ) + ( TypeIsPair1(desc.types[i]) ? 3 : 0 ), // add 3 for the ^7/ of PAIR1 types
			desc.types[i] == STAT_OBJ_DURATION ? (overrideStr[0] ? overrideStr : (stats[i].iValue ? GetFormattedValue(stats[i].iValue, desc.types[i]) : "")) : GetFormattedValue( stats[i].iValue, desc.types[i] ) ) // string-ified version of the type, will contain the slash for PAIR1
			);
	}

	trap_SendServerCommand( id, va( "print \"%s\n\"", s ) );
	if (outputBuffer) Q_strcat(outputBuffer, outSize, va("%s\n", s));
}

// prints colored lines as a visual indicator of what times you got each obj
static void PrintSiegeObjGraphics(const int clientNum) {
	if (g_gametype.integer != GT_SIEGE || level.siegeStage < SIEGESTAGE_ROUND1POSTGAME || !g_siegeTimeVisualAid.integer)
		return;

	// print a line for each round
	int currentRound = level.siegeStage >= SIEGESTAGE_ROUND2POSTGAME ? 2 : 1;
	int maximumTime = level.siegeMaximum;
	int maximumTicks = (maximumTime / 10000) + 1;
	int highestRoundTime = 1;
	for (int round = 1; round <= currentRound; round++) {
		char thisLine[8192] = { 0 };
		int index = 0;
		int thisRoundTime = 0;
		Com_sprintf(thisLine, sizeof(thisLine), "Round %d  ", round);
		index = strlen(thisLine);

		int heldForMaxAt = trap_Cvar_VariableIntegerValue(va("siege_r%i_heldformaxat", round));
		int heldForMaxTime = 0, incomplete = 0;
		if (heldForMaxAt) {
			incomplete = G_FirstIncompleteObjective(round);
			heldForMaxTime = trap_Cvar_VariableIntegerValue(va("siege_r%i_heldformaxtime", round));
		}
		
		int totalTicks = 0;
		// print the lines for each obj adjacent to one another
		for (int obj = 0; obj < MAX_STATS && obj < level.numSiegeObjectivesOnMap; ++obj) {
			// determine how long of a line to draw
			int objTime;
			if (incomplete && obj + 1 == incomplete && heldForMaxTime) {
				objTime = heldForMaxTime;
			}
			else {
				objTime = G_ObjectiveTimeDifference(obj + 1, round);
			}
			thisRoundTime += objTime;
			int ticks = (objTime + 5000) / 10000; // round to the nearest 10 second increment (each tick represents 10 seconds)

			// make sure the total number of ticks doesn't go past the logical maximum
			if (totalTicks + ticks > maximumTicks) {
				ticks = maximumTicks - totalTicks;
			}

			if (ticks < 1) { // always guarantee a minimum of 1 tick
				ticks = 1;
			}

			// draw the line
			thisLine[index++] = '^';
			thisLine[index++] = (unsigned char)('0' + ((obj + 1) % 10));
			totalTicks += ticks;
			while (ticks) {
				if (incomplete && obj + 1 == incomplete && heldForMaxTime && ticks == 1)
					thisLine[index++] = 'X'; // show 'X' as the last digit to show that they actually complete the obj
				else
					thisLine[index++] = '-';
				ticks--;
			}

			if (incomplete && obj + 1 == incomplete && heldForMaxTime)
				break; // no more objs because they didn't finish this one
		}
		Q_strcat(thisLine, sizeof(thisLine), "^7\n");
		PrintIngame(clientNum, thisLine);

		if (thisRoundTime > highestRoundTime)
			highestRoundTime = thisRoundTime;
	}

	// print a line labeling the minutes
	// we go 1 minute over what they did (e.g. for 13:37 we show up to minute 14),
	// but we never go over the max (e.g. for 20:00 we just show up to minute 20)
	char timeLine[2048] = { 0 };
	int highestRoundMinutes = (highestRoundTime / (60 * 1000)) + 1;
	int maximumMinutes = (maximumTime / 1000) / 60;
	Com_sprintf(timeLine, sizeof(timeLine), "^9Time^7     ");
	for (int minute = 0; minute <= highestRoundMinutes && minute <= maximumMinutes; minute++) {
		// alternate between white and grey
		char *minuteStr = va("%s%d", minute & 1 ? "^9" : "^7", minute);
		int minuteStrLen = Q_PrintStrlen(minuteStr);
		Q_strcat(timeLine, sizeof(timeLine), minuteStr);

		// add spaces until the next minute
		for (int numSpacesToPrint = 6 - minuteStrLen; numSpacesToPrint > 0; numSpacesToPrint--)
			Q_strcat(timeLine, sizeof(timeLine), " ");
	}
	Q_strcat(timeLine, sizeof(timeLine), "^7\n\n");
	PrintIngame(clientNum, timeLine);
}

static void PrintTeamStats(const int id, const team_t team, const char teamColor, StatsDesc desc, void(*fillCallback)(gclient_t *, Stat *), qboolean printHeader, char *outputBuffer, size_t outSize) {
	int i, j, nameLen = 0, maxNameLen = 0;
	Stat stats[MAX_CLIENTS][MAX_STATS], bestStats[MAX_STATS];
	memset(&stats, 0, sizeof(stats));
	memset(&bestStats, 0, sizeof(bestStats));
	char header[MAX_STRING_CHARS], separator[MAX_STRING_CHARS];
	gclient_t *client;
	team_t otherTeam;
	int siegeRound = level.siegeStage >= SIEGESTAGE_ROUND2POSTGAME ? 2 : 1;
	char *firstColumnTitle = "NAME";

	otherTeam = OtherTeam( team );

	if (desc.types[0] == STAT_OBJ_DURATION) { // siege obj stats
		firstColumnTitle = "ROUND";
		if (siegeRound == 2) { // only highlight faster times in round 2 (or else all times will be highlighted in round 1)
			for (j = 0; j < MAX_STATS; ++j) {
				if (desc.types[j] == STAT_NONE)
					continue;
				bestStats[j].iValue = 0x7FFFFFFF;
			}
		}
		for (i = 1; i <= siegeRound; i++) {
			client = g_entities[i].client;
			(*fillCallback)(client, &stats[i][0]);
			// compare them to the best stats and sum to the total stats
			for (j = 0; j < MAX_STATS; ++j) {
				if (desc.types[j] == STAT_NONE)
					continue;
				if (j != MAX_STATS - 1 && j + 1 > level.numSiegeObjectivesOnMap) { // unused objectives are not put in the table
					desc.types[j] = STAT_NONE;
					continue;
				}
				else
					desc.types[j] = STAT_OBJ_DURATION;
				nameLen = Q_PrintStrlen(desc.cols[j]);
				if (nameLen > maxNameLen) // highlight faster times
					maxNameLen = nameLen;
				if (bestStats[j].iValue > stats[i][j].iValue)
					bestStats[j].iValue = stats[i][j].iValue;
			}
		}
		// in round 2, force green highlight if you completed an objective that the other round didn't
		if (siegeRound == 2) {
			for (j = 0; j < MAX_STATS; ++j) {
				if (desc.types[j] == STAT_NONE)
					continue;
				if (!stats[1][j].iValue && stats[2][j].iValue)
					bestStats[j].iValue = stats[2][j].iValue;
				else if (!stats[2][j].iValue && stats[1][j].iValue)
					bestStats[j].iValue = stats[1][j].iValue;
			}
			// sanity check: the winning team's time should always be in green, but nobody should be green if it's a tie
			if (level.siegeMatchWinner == SIEGEMATCHWINNER_ROUND1OFFENSE || level.siegeMatchWinner == SIEGEMATCHWINNER_ROUND2OFFENSE) {
				stats[level.siegeMatchWinner][MAX_STATS - 1].forceColor = S_COLOR_GREEN;
				stats[OtherTeam(level.siegeMatchWinner)][MAX_STATS - 1].forceColor = S_COLOR_WHITE;
			}
			else if (level.siegeMatchWinner == SIEGEMATCHWINNER_TIE) {
				stats[1][MAX_STATS - 1].forceColor = S_COLOR_WHITE;
				stats[2][MAX_STATS - 1].forceColor = S_COLOR_WHITE;
			}
		}
		// if you were held for a max, color it red and add "(DNF)" with the time you were held for
		for (i = 1; i <= siegeRound; i++) {
			int heldForMaxAt = trap_Cvar_VariableIntegerValue(va("siege_r%i_heldformaxat", i));
			if (heldForMaxAt) {
				int incomplete = G_FirstIncompleteObjective(i);
				stats[i][incomplete - 1].iValue = trap_Cvar_VariableIntegerValue(va("siege_r%i_heldformaxtime", i));
				stats[i][incomplete - 1].forceColor = S_COLOR_RED;
			}
		}
	}
	else { // normal stats
		for (j = 0; j < MAX_STATS; ++j) {
			if (TypeIsLowerBetter(desc.types[j]))
				bestStats[j].iValue = 0x7FFFFFFF;
		}
		for (i = 0; i < level.maxclients; ++i) {
			if (!g_entities[i].inuse || !g_entities[i].client || g_entities[i].client->pers.connected != CON_CONNECTED) {
				continue;
			}

			client = g_entities[i].client;

			// count both teams so the columns have the same size
			if (g_entities[i].client->sess.sessionTeam != team && g_entities[i].client->sess.sessionTeam != otherTeam) {
				continue;
			}

			nameLen = Q_PrintStrlen(client->pers.netname);
			if (nameLen > maxNameLen)
				maxNameLen = nameLen;

			// only fill stats for the current team
			if (client->sess.sessionTeam != team) {
				continue;
			}

			(*fillCallback)(client, &stats[i][0]);

			// compare them to the best stats and sum to the total stats
			for (j = 0; j < MAX_STATS; ++j) {
				if (desc.types[j] == STAT_NONE)
					continue;
				if (desc.types[j] == STAT_FLOAT) {
					if (bestStats[j].fValue < stats[i][j].fValue)
						bestStats[j].fValue = stats[i][j].fValue;
				}
				else if (TypeIsLowerBetter(desc.types[j])) {
					if (bestStats[j].iValue > stats[i][j].iValue)
						bestStats[j].iValue = stats[i][j].iValue;
				}
				else {
					if (bestStats[j].iValue < stats[i][j].iValue)
						bestStats[j].iValue = stats[i][j].iValue;
				}
			}
		}
	}

	// make sure there is room for the first column title alone
	if (maxNameLen < Q_PrintStrlen(firstColumnTitle))
		maxNameLen = Q_PrintStrlen(firstColumnTitle);

	Com_sprintf(header, sizeof(header), S_COLOR_CYAN"%s", firstColumnTitle);
	for ( i = 0; i < maxNameLen - Q_PrintStrlen(firstColumnTitle); ++i )
		Q_strcat( header, sizeof( header ), " " );

	Com_sprintf( separator, sizeof( separator ), "^%c", teamColor );

	for ( i = 0; i < maxNameLen; ++i )
		Q_strcat( separator, sizeof( separator ), STATS_ROW_SEPARATOR );

	// prepare header and dotted separators
	for ( j = 0; j < MAX_STATS; ++j ) {
		if (desc.types[j] == STAT_NONE)
			continue;
		// left aligned names should follow a slash, so no space
		if ( desc.types[j] < STAT_LEFT_ALIGNED ) {
			Q_strcat( header, sizeof( header ), " " );
			Q_strcat( separator, sizeof( separator ), " " );
		}

		Q_strcat( header, sizeof( header ), desc.cols[j] );

		// only for PAIR1 types, append a slash in the name
		if ( TypeIsPair1(desc.types[j]) ) {
			Q_strcat( header, sizeof( header ), "/" );
		}

		// generate the dotted delimiter, adding a char for the slash of PAIR1 types
		for ( i = 0; i < Q_PrintStrlen( desc.cols[j] ) + ( TypeIsPair1(desc.types[j]) ? 1 : 0 ); ++i ) {
			Q_strcat( separator, sizeof( separator ), STATS_ROW_SEPARATOR );
		}
	}

	if ( printHeader ) {
		trap_SendServerCommand( id, va( "print \"%s\n%s\n\"", header, separator ) );
		if (outputBuffer) Q_strcat(outputBuffer, outSize, va("%s\n%s\n", header, separator));
	} else {
		trap_SendServerCommand( id, va( "print \"%s\n\"", separator ) );
		if (outputBuffer) Q_strcat(outputBuffer, outSize, va("%s\n", separator));
	}

	if (desc.types[0] == STAT_OBJ_DURATION) { // siege obj stats
		for (i = 1; i <= siegeRound; i++) {
			client = &level.clients[level.sortedClients[i]];
			PrintClientStats(id, va("Round %i", i), desc, stats[i], bestStats, maxNameLen, outputBuffer, outSize);
		}
	}
	else { // normal stats
		for (i = 0; i < level.numConnectedClients; i++) {
			client = &level.clients[level.sortedClients[i]];

			if (!client || client->sess.sessionTeam != team)
				continue;

			PrintClientStats(id, client->pers.netname, desc, stats[level.sortedClients[i]], bestStats, maxNameLen, outputBuffer, outSize);
		}
	}
}

#define FillValueInt(v)		do { values[i++].iValue = v; } while (0)
#define FillValueFloat(v)	do { values[i++].fValue = v; } while (0)

static const StatsDesc CtfStatsDesc = {
	{
		"SCORE", "CAP", "ASS", "DEF", "ACC", "FCKIL", "RET", "BOON", "TTLHOLD", "MAXHOLD",
		"SAVES", "DMGDLT", "DMGTKN", "TOPSPD", "AVGSPD"
	},
	{
		STAT_INT, STAT_INT, STAT_INT, STAT_INT, STAT_INT, STAT_INT, STAT_INT, STAT_INT, STAT_DURATION, STAT_DURATION,
		STAT_INT, STAT_INT, STAT_INT, STAT_INT, STAT_INT
	}
};

static void FillCtfStats(gclient_t *cl, Stat *values) {
	int i = 0;
	FillValueInt(cl->ps.persistant[PERS_SCORE]);
	FillValueInt(cl->ps.persistant[PERS_CAPTURES]);
	FillValueInt(cl->ps.persistant[PERS_ASSIST_COUNT]);
	FillValueInt(cl->ps.persistant[PERS_DEFEND_COUNT]);
	FillValueInt(cl->accuracy_shots ? cl->accuracy_hits * 100 / cl->accuracy_shots : 0);
	FillValueInt(cl->pers.teamState.fragcarrier);
	FillValueInt(cl->pers.teamState.flagrecovery);
	FillValueInt(cl->pers.teamState.boonPickups);
	FillValueInt(cl->pers.teamState.flaghold);
	FillValueInt(cl->pers.teamState.longestFlaghold);
	FillValueInt(cl->pers.teamState.saves);
	FillValueInt(cl->pers.damageCaused);
	FillValueInt(cl->pers.damageTaken);
	int maxSpeed = (int)(cl->pers.topSpeed + 0.5f);
	int avgSpeed;

	if (cl->pers.displacementSamples) {
		avgSpeed = (int)floorf(((cl->pers.displacement * g_svfps.value) / cl->pers.displacementSamples) + 0.5f);
	}
	else {
		avgSpeed = maxSpeed;
	}

	FillValueInt(maxSpeed);
	FillValueInt(avgSpeed);
}

static const StatsDesc ForceStatsDesc = {
	{
		"PUSH", "PULL", "HEALED", "NRGSED ALLY", "ENEMY", "ABSRBD", "PROTDMG", "PROTTIME"
	},
	{
		STAT_INT_PAIR1, STAT_INT_PAIR2, STAT_INT, STAT_INT_PAIR1, STAT_INT_PAIR2, STAT_INT, STAT_INT, STAT_DURATION
	}
};

static void FillForceStats( gclient_t *cl, Stat *values ) {
	int i = 0;
	FillValueInt(cl->pers.push);
	FillValueInt(cl->pers.pull);
	FillValueInt(cl->pers.healed);
	FillValueInt(cl->pers.energizedAlly);
	FillValueInt(cl->pers.energizedEnemy);
	FillValueInt(cl->pers.absorbed);
	FillValueInt(cl->pers.protDmgAvoided);
	FillValueInt(cl->pers.protTimeUsed);
}

static const StatsDesc ObjStatsDesc = {
	{
		"OBJECTIVE 1", "OBJECTIVE 2", "OBJECTIVE 3", "OBJECTIVE 4", "OBJECTIVE 5", "OBJECTIVE 6",
		"OBJECTIVE 7", "OBJECTIVE 8", "OBJECTIVE 9", "OBJECTIVE 10", "OBJECTIVE 11",
		"OBJECTIVE 12", "OBJECTIVE 13", "OBJECTIVE 14", "OBJECTIVE 15", "TOTAL",
	},
	{
		STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION,
		STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION,
		STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION,
		STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION, STAT_OBJ_DURATION
	}
};

static void FillObjStats(gclient_t *cl, Stat *values) {
	int i, roundNum = cl - level.clients;
	for (i = 0; i < MAX_STATS - 1; i++) {
		values[i].iValue = G_ObjectiveTimeDifference(i + 1, roundNum);
	}
	values[MAX_STATS - 1].iValue = trap_Cvar_VariableIntegerValue(va("siege_r%i_total", roundNum));
}

static const StatsDesc SiegeGeneralDesc = {
	{
		"CAP", "SAVE", "OFFKIL", "DMGDEALT", "OKPM", "OFFDTH", "DMGTKN",
		"DEFKIL", "DMGDEALT", "DKPM", "DEFDTH", "DMGTKN",
		"MAXES", "GOTMAXED", "SK",
	},
	{
		STAT_INT, STAT_INT, STAT_INT_PAIR1, STAT_INT_PAIR2, STAT_FLOAT, STAT_INT_PAIR1_LOWERBETTER, STAT_INT_PAIR2_LOWERBETTER,
		STAT_INT_PAIR1, STAT_INT_PAIR2, STAT_FLOAT, STAT_INT_PAIR1_LOWERBETTER, STAT_INT_PAIR2_LOWERBETTER,
		STAT_INT, STAT_INT_LOWERBETTER, STAT_INT
	}
};

static void FillSiegeGeneralStats(gclient_t *cl, Stat *values) {
	int i = 0;
	FillValueInt(cl->sess.siegeStats.caps[0] + cl->sess.siegeStats.caps[1]);
	FillValueInt(cl->sess.siegeStats.saves[0] + cl->sess.siegeStats.saves[1]);
	FillValueInt(cl->sess.siegeStats.oKills[0] + cl->sess.siegeStats.oKills[1]);
	FillValueInt(cl->sess.siegeStats.oDamageDealt[0] + cl->sess.siegeStats.oDamageDealt[1]);

	float oKills = (float)(cl->sess.siegeStats.oKills[0] + cl->sess.siegeStats.oKills[1]);
	float oTime = (float)(cl->sess.siegeStats.oTime[0] + cl->sess.siegeStats.oTime[1]);
	if (oTime) {
#ifdef SCOREBOARDTIME_BASED_KPM
		FillValueFloat(oKills / ((int)oTime / 60000));
#else
		FillValueFloat((oKills / oTime) * 60000.0f);
#endif
	}
	else {
		FillValueFloat(0.0f);
	}

	FillValueInt(cl->sess.siegeStats.oDeaths[0] + cl->sess.siegeStats.oDeaths[1]);
	FillValueInt(cl->sess.siegeStats.oDamageTaken[0] + cl->sess.siegeStats.oDamageTaken[1]);

	FillValueInt(cl->sess.siegeStats.dKills[0] + cl->sess.siegeStats.dKills[1]);
	FillValueInt(cl->sess.siegeStats.dDamageDealt[0] + cl->sess.siegeStats.dDamageDealt[1]);

	float dKills = (float)(cl->sess.siegeStats.dKills[0] + cl->sess.siegeStats.dKills[1]);
	float dTime = (float)(cl->sess.siegeStats.dTime[0] + cl->sess.siegeStats.dTime[1]);
	if (dTime) {
#ifdef SCOREBOARDTIME_BASED_KPM
		FillValueFloat(dKills / ((int)dTime / 60000));
#else
		FillValueFloat((dKills / dTime) * 60000.0f);
#endif
	}
	else {
		FillValueFloat(0.0f);
	}

	FillValueInt(cl->sess.siegeStats.dDeaths[0] + cl->sess.siegeStats.dDeaths[1]);
	FillValueInt(cl->sess.siegeStats.dDamageTaken[0] + cl->sess.siegeStats.dDamageTaken[1]);

	FillValueInt(cl->sess.siegeStats.maxes[0] + cl->sess.siegeStats.maxes[1]);
	FillValueInt(cl->sess.siegeStats.maxed[0] + cl->sess.siegeStats.maxed[1]);
	FillValueInt(cl->sess.siegeStats.selfkills[0] + cl->sess.siegeStats.selfkills[1]);
}

static const StatsDesc HothDesc = {
	{
		"GENDMG", "CODESTIME", "CCDMG", "TECHMAX", "KILL",
		"ATSTKILL", "ATSTDMG", "SHIELD", "UPTIME",
		"OFFFREEZE", "OFFGOTFROZEN", "DEFFREEZE", "DEFGOTFROZEN",
		"OASSIST", "DASSIST"
	},
	{
		STAT_INT, STAT_DURATION, STAT_INT, STAT_INT_PAIR1, STAT_INT_PAIR2,
		STAT_INT, STAT_INT, STAT_INT, STAT_DURATION,
		STAT_DURATION, STAT_DURATION_LOWERBETTER, STAT_DURATION, STAT_DURATION_LOWERBETTER,
		STAT_INT, STAT_INT
	}
};

static const StatsDesc DesertDesc = {
	{
		"WALLDMG", "STATION1DMG", "STATION2DMG", "STATION3DMG", "GATEDMG",
		"PARTS", "PARTSTIME",
		"OFFFREEZE", "OFFGOTFROZEN", "DEFFREEZE", "DEFGOTFROZEN",
		"OASSIST", "DASSIST"
	},
	{
		STAT_INT, STAT_INT, STAT_INT, STAT_INT, STAT_INT,
		STAT_INT, STAT_DURATION,
		STAT_DURATION, STAT_DURATION_LOWERBETTER, STAT_DURATION, STAT_DURATION_LOWERBETTER,
		STAT_INT, STAT_INT
	}
};

static const StatsDesc KorriDesc = {
	{
		"TEMPGATEDMG", "BLUETIME", "GREENTIME", "REDTIME",
		"SCEPTGATEDMG", "ALTGATEDMG", "SCEPTTIME", "COFFINDMG",
		"MINESTHROWN"
	},
	{
		STAT_INT, STAT_DURATION, STAT_DURATION, STAT_DURATION,
		STAT_INT, STAT_INT, STAT_DURATION, STAT_INT,
		STAT_INT
	}
};

static const StatsDesc NarDesc = {
	{
		"STATION1DMG", "STATION2DMG", "CODESTIME", "TECHMAX", "KILL",
		"SHIELDS", "SHIELDUPTIME",
		"OFFFREEZE", "OFFGOTFROZEN", "DEFFREEZE", "DEFGOTFROZEN",
		"OASSIST", "DASSIST"
	},
	{
		STAT_INT, STAT_INT, STAT_DURATION, STAT_INT_PAIR1, STAT_INT_PAIR2,
		STAT_INT, STAT_DURATION,
		STAT_DURATION, STAT_DURATION_LOWERBETTER, STAT_DURATION, STAT_DURATION_LOWERBETTER,
		STAT_INT, STAT_INT
	}
};

static const StatsDesc Cargo2Desc = {
	{
		"ARRAYDMG", "NODE1DMG", "NODE2DMG", "CODESTIME", "HACKS",
		"OFFFREEZE", "OFFGOTFROZEN", "DEFFREEZE", "DEFGOTFROZEN",
		"OASSIST", "DASSIST"
	},
	{
		STAT_INT, STAT_INT, STAT_INT, STAT_DURATION, STAT_INT,
		STAT_DURATION, STAT_DURATION_LOWERBETTER, STAT_DURATION, STAT_DURATION_LOWERBETTER,
		STAT_INT, STAT_INT
	}
};

static const StatsDesc BespinDesc = {
	{
		"LOCKDMG", "PANELDMG", "GENDMG", "CODESTIME", "PODDMG",
		"OFFFREEZE", "OFFGOTFROZEN", "DEFFREEZE", "DEFGOTFROZEN",
		"OASSIST", "DASSIST"
	},
	{
		STAT_INT, STAT_INT, STAT_INT, STAT_DURATION, STAT_INT,
		STAT_DURATION, STAT_DURATION_LOWERBETTER, STAT_DURATION, STAT_DURATION_LOWERBETTER,
		STAT_INT, STAT_INT
	}
};

static const StatsDesc UrbanDesc = {
	{
		"MONEYTIME", "BLUEDMG", "REDDMG",
		"OFFFREEZE", "OFFGOTFROZEN", "DEFFREEZE", "DEFGOTFROZEN",
		"OASSIST", "DASSIST"
	},
	{
		STAT_DURATION, STAT_INT, STAT_INT,
		STAT_DURATION, STAT_DURATION_LOWERBETTER, STAT_DURATION, STAT_DURATION_LOWERBETTER,
		STAT_INT, STAT_INT
	}
};

static const StatsDesc AnsionDesc = {
	{
		"CODESTIME"
	},
	{
		STAT_DURATION
	}
};

static void FillMapSpecificStats(gclient_t *cl, Stat *values) {
	int i;
	for (i = 0; i < MAX_STATS; i++) {
		values[i].iValue = cl->sess.siegeStats.mapSpecific[0][i] + cl->sess.siegeStats.mapSpecific[1][i];
	}
}

#define ColorForTeam( team )		( team == TEAM_BLUE ? COLOR_BLUE : COLOR_RED )
#define ScoreTextForTeam( team )	( team == TEAM_BLUE ? S_COLOR_BLUE"BLUE" : S_COLOR_RED"RED" )

void PrintStatsTo( gentity_t *ent, const char *type, char* outputBuffer, size_t outSize ) {
	qboolean winningIngame = qfalse, losingIngame = qfalse;
	int id = ent ? ( ent - g_entities ) : -1, i;
	const StatsDesc *desc;
	void( *callback )( gclient_t*, Stat* );

	if (g_gametype.integer == GT_SIEGE && !g_siegeTeamSwitch.integer) // not supported
		return;

	if ( !VALIDSTRING( type ) ) {
		return;
	}

	if ( g_gametype.integer != GT_CTF && g_gametype.integer != GT_SIEGE ) {
		if ( id != -1 ) {
			trap_SendServerCommand( id, "print \""S_COLOR_WHITE"Gametype is not CTF or Siege. Statistics aren't generated.\n\"" );
		}

		return;
	}

	team_t winningTeam = g_gametype.integer == GT_SIEGE ? TEAM_RED : level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ? TEAM_RED : TEAM_BLUE;
	team_t losingTeam = OtherTeam( winningTeam );

	if (!(g_gametype.integer == GT_SIEGE && !Q_stricmp(type, "obj"))) {
		for (i = 0; i < level.maxclients; i++) {
			if (!g_entities[i].inuse || !g_entities[i].client) {
				continue;
			}

			if (g_entities[i].client->sess.sessionTeam == winningTeam) {
				winningIngame = qtrue;
			}
			else if (g_entities[i].client->sess.sessionTeam == losingTeam) {
				losingIngame = qtrue;
			}

			if (winningIngame && losingIngame) {
				break;
			}
		}

		if (!winningIngame && !losingIngame) {
			if (id != -1) {
				//trap_SendServerCommand(id, "print \""S_COLOR_WHITE"Nobody is playing. Statistics aren't generated.\n\"");
			}

			return;
		}
	}

	if ( g_gametype.integer == GT_CTF && !Q_stricmp( type, "general" ) ) {
		desc = &CtfStatsDesc;
		callback = &FillCtfStats;

		// for general stats, also print the score
		const char* s = va("%s: "S_COLOR_WHITE"%d    %s: "S_COLOR_WHITE"%d\n\n",
			ScoreTextForTeam(winningTeam), level.teamScores[winningTeam],
			ScoreTextForTeam(losingTeam), level.teamScores[losingTeam]);
		trap_SendServerCommand( id, va( "print \"%s\"", s) );
		if (outputBuffer) Q_strcat(outputBuffer, outSize, s);
	} else if ( !Q_stricmp( type, "force" ) ) {
		desc = &ForceStatsDesc;
		callback = &FillForceStats;
	} else if ( g_gametype.integer == GT_SIEGE && !Q_stricmp( type, "obj" ) ) {
		if (level.siegeStage < SIEGESTAGE_ROUND1POSTGAME)
			return;
		trap_SendServerCommand(id, "print \"\n\"");
		desc = &ObjStatsDesc;
		callback = &FillObjStats;
	} else if (g_gametype.integer == GT_SIEGE && !Q_stricmp(type, "general")) {
#ifndef _DEBUG
		if (level.siegeStage != SIEGESTAGE_ROUND1POSTGAME && level.siegeStage != SIEGESTAGE_ROUND2POSTGAME && id >= 0 && id < MAX_CLIENTS && &g_entities[id].client && g_entities[id].client->sess.sessionTeam != TEAM_SPECTATOR)
			return;
#endif
		desc = &SiegeGeneralDesc;
		callback = &FillSiegeGeneralStats;
	} else if (g_gametype.integer == GT_SIEGE && !Q_stricmp(type, "map")) {
#ifndef _DEBUG
		if (level.siegeStage != SIEGESTAGE_ROUND1POSTGAME && level.siegeStage != SIEGESTAGE_ROUND2POSTGAME && id >= 0 && id < MAX_CLIENTS && &g_entities[id].client && g_entities[id].client->sess.sessionTeam != TEAM_SPECTATOR)
			return;
#endif
		callback = &FillMapSpecificStats;
		switch (level.siegeMap) {
		case SIEGEMAP_HOTH:			desc = &HothDesc;		break;
		case SIEGEMAP_DESERT:		desc = &DesertDesc;		break;
		case SIEGEMAP_KORRIBAN:		desc = &KorriDesc;		break;
		case SIEGEMAP_NAR:			desc = &NarDesc;		break;
		case SIEGEMAP_CARGO:		desc = &Cargo2Desc;		break;
		case SIEGEMAP_URBAN:		desc = &UrbanDesc;		break;
		case SIEGEMAP_BESPIN:		desc = &BespinDesc;		break;
		case SIEGEMAP_ANSION:		desc = &AnsionDesc;		break;
		default:					return;
		}
	} else {
		if ( id != -1 ) {
			if (g_gametype.integer == GT_SIEGE)
				trap_SendServerCommand(id, va("print \""S_COLOR_WHITE"Unknown type \"%s"S_COLOR_WHITE"\". Usage: "S_COLOR_CYAN"/stats <obj | general | map>\n\"", type));
			else
				trap_SendServerCommand( id, va( "print \""S_COLOR_WHITE"Unknown type \"%s"S_COLOR_WHITE"\". Usage: "S_COLOR_CYAN"/ctfstats <general | force>\n\"", type ) );
		}

		return;
	}

	// print the winning team first, and don't print stats of teams that have no players
	if (desc == &ObjStatsDesc) {
		PrintTeamStats(id, 0, COLOR_WHITE, *desc, callback, qtrue, outputBuffer, outSize);
	}
	else {
		if (winningIngame) PrintTeamStats(id, winningTeam, ColorForTeam(winningTeam), *desc, callback, qtrue, outputBuffer, outSize);
		if (losingIngame) PrintTeamStats(id, losingTeam, ColorForTeam(losingTeam), *desc, callback, !winningIngame, outputBuffer, outSize);
	}
	trap_SendServerCommand( id, "print \"\n\"" );
	if (outputBuffer) Q_strcat(outputBuffer, outSize, "\n");

	if (desc == &ObjStatsDesc)
		PrintSiegeObjGraphics(id);
}

void Cmd_PrintStats_f( gentity_t *ent ) {
	if ( trap_Argc() < 2 ) { // display all types if none is specified, i guess
		if (g_gametype.integer == GT_SIEGE) {
			PrintStatsTo(ent, "obj", NULL, 0);
			PrintStatsTo(ent, "general", NULL, 0);
			if (level.siegeMap != SIEGEMAP_UNKNOWN && level.siegeMap != SIEGEMAP_IMPERIAL && level.siegeMap != SIEGEMAP_DUEL)
				PrintStatsTo(ent, "map", NULL, 0);
		}
		else {
			PrintStatsTo(ent, "general", NULL, 0);
			PrintStatsTo(ent, "force", NULL, 0);
		}
	} else {
		char subcmd[MAX_STRING_CHARS] = { 0 };
		trap_Argv( 1, subcmd, sizeof( subcmd ) );
		PrintStatsTo( ent, subcmd, NULL, 0 );
	}
}

void ServerCfgColor(char *string, int integer, gentity_t *ent)
{
	if (integer && integer == 1)
	{
		Com_sprintf(string, 64, "%s:^2", string);
	}
	else if (integer && (integer > 1 || integer < 0))
	{
		Com_sprintf(string, 64, "%s:^5", string);
	}
	else
	{
		Com_sprintf(string, 64, "%s:^1", string);
	}
	trap_SendServerCommand(ent - g_entities, va("print \"%s %i\n\"", string, integer));
}

void Cmd_Help_f(gentity_t *ent)
{
	trap_SendServerCommand(ent - g_entities, va("print \"^6base_entranced version: build " MODBUILDNUMBER " - https://github.com/deathsythe47/base_entranced\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^2/WHOIS:^7   You can list everyone's client numbers and their most-used alias with ^5/whois^7. See a history of someone's most-used aliases with ^5/whois <name/id>^7. Partial player names or slot numbers are okay.\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^2/TELL:^7   You can send private chats to another player with ^5/tell <player> <message>.^7 Partial player names or slot numbers are okay.\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"Example: ^5/tell pad enemy weak^7 will send Padawan a message saying 'enemy weak'\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^2SIMPLIFIED PRIVATE MESSAGING:^7   Instead of using /tell, you can send private chats to another player simply by pressing your chat bind and typing two @ symbols, e.g. ^5@@<player> <message>.^7 Partial player names or slot numbers are okay.\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^2/IGNORE:^7   Use ^5/ignore <name/id>^7 to stop seeing chats from a player. Partial player names or slot numbers are okay.\n\""));
	if (g_gametype.integer == GT_SIEGE)
	{
		if (g_saveCaptureRecords.integer)
			trap_SendServerCommand(ent - g_entities, va("print \"^2/TOPTIMES:^7   Use ^5/toptimes^7 to view a list of fastest obj/map completion records.\n\""));
		trap_SendServerCommand(ent - g_entities, va("print \"^2/CLASS:^7   Use ^5/class <first letter of class name>^7 to change classes. For example, ^5/class a^7 for assault.\n\""));
		trap_SendServerCommand(ent - g_entities, va("print \"^2/JOIN:^7   Use ^5/join <team letter><first letter of class name>^7 (no spaces) to join as a specific team and class. For example, '^5join rj^7' for red jedi)\n\""));
		trap_SendServerCommand(ent - g_entities, va("print \"^2/CLASSES:^7   Use ^5/classes^7 to display all class loadouts for the current map.\n\""));
	}
	trap_SendServerCommand(ent - g_entities, va("print \"^2/SERVERSTATUS2:^7   You can use ^5/serverstatus2^7 to display a list of server cvars that are not displayed by the ordinary /serverstatus command.\n\""));
	trap_SendServerCommand(ent - g_entities, "print \"^2/INKOGNITO:^7   You can use ^5/inkognito^7 to hide whom you are following on the scoreboard.\n\"");
	if (g_allow_ready.integer)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^2/READY:^7   You can use ^5/ready^7 to yourself to toggle yourself eligible/ineligible for selection by the random team generator(used with vote or rcon).\n\""));
	}
	if (g_privateDuel.integer)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^2/ENGAGE_DUEL^7:   You can challenge another captain to a pistol duel if both of you use ^5/engage_duel^7.\n\""));
	}
	if (g_allowVote.integer)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^2VOTES:^7   There are many new things you can call votes for. Type ^5/callvote^7 to see a list.\n\""));
		trap_SendServerCommand(ent - g_entities, va("print \"To see a list of eligible maps for voting with /callvote newpug, use ^5/pugmaps^7.\n\""));
	}
	if (g_emotes.integer) {
		trap_SendServerCommand(ent - g_entities, va("print \"^2/EMOTE^7:   You can do a custom animation with ^5/emote [optional keyword] [optional duration in ms]^7. You will automatically SK at the end of the current spawn cycle, and can no longer attack or sustain damage for that spawn.\n\""));
		if (g_moreTaunts.integer)
			trap_SendServerCommand(ent - g_entities, va("print \"^2MORE TAUNTS:^7   This server allows for extra taunts. Use ^5/gloat^7, ^5/flourish^7, ^5/bow^7, or ^5/meditate^7. Meditate uses similar rules as emotes (see above).\n\""));
	}
	else {
		if (g_moreTaunts.integer)
			trap_SendServerCommand(ent - g_entities, va("print \"^2MORE TAUNTS:^7   This server allows for extra taunts. Use ^5/gloat^7, ^5/flourish^7, ^5/bow^7, or ^5/meditate^7. Meditate causes you to automatically SK at the end of the current spawn cycle, and you can no longer attack or sustain damage for that spawn.\n\""));
	}
	if (g_allow_vote_forceclass.integer && g_allowVote.integer)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^2TEAMVOTES: FORCECLASS AND UNFORCECLASS:^7   You can call special team-only votes with ^5/callteamvote^7. You can force teammates to play a certain class.\n\""));
		trap_SendServerCommand(ent - g_entities, va("print \"Use ^5forceclass <name> <first letter of class name>^7 or ^5unforceclass^7. For example, ^5/callteamvote forceclass pad a\n\""));
		trap_SendServerCommand(ent - g_entities, va("print \"Use ^5/teamvote yes^7 and ^5/teamvote no^7 to vote on these special teamvotes.\n\""));
	}
	trap_SendServerCommand(ent - g_entities, va("print \"^2WEAPON SPAWN PREFERENCE:^7   You can specify an order of preferred weapons that you would like to be holding when you spawn by using ^5/setu prefer <15 letters>\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"L=melee,S=saber,P=pistol,Y=bryar,E=E11,U=disruptor,B=bowcaster,I=repeater,D=demp,G=golan,R=rocket,C=conc,T=thermals,M=mines,K=detpacks\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"Example: ^5/setu prefer RCTIGDUEBSMKYPL\n\""));
	if (g_cheats.integer)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^2/NPC SPAWNLIST:^7   Use ^5/npc spawnlist^7 to show a list of possible NPC spawns. Ingame console has limited scrolling; read /base/qconsole.log to see more.\n\""));
	}
	trap_SendServerCommand(ent - g_entities, va("print \"^2CHAT TOKENS:^7   You can dynamically include some stats in your chat messages by writing these tokens:\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^5$H^7 (health), ^5$A^7 (armor), ^5$F^7 (force), ^5$M^7 (ammo)\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^2MAP CHANGELOG:^7   You can view the changelog to the current map (if available) by using ^5/changes^7.\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^2SKILLBOOST:^7   You can see which players are skillboosted by typing ^5/skillboost^7.\n\""));
	trap_SendServerCommand(ent - g_entities, va("print \"^2/USE_PACK, USE_DISPENSER, USE_ANYBACTA:^7   New commands to use jetpack, ammo dispenser, or any bacta you have\n\""));
}

#define PrintCvar(cvar)		do { Com_sprintf(string, sizeof(string), #cvar); ServerCfgColor(string, cvar.integer, ent); } while (0)
void Cmd_ServerStatus2_f(gentity_t *ent)
{
	char string[128] = { 0 };


	PrintCvar(autocfg_map);
	PrintCvar(autocfg_unknown);
	PrintCvar(g_allow_ready);
	PrintCvar(g_allow_vote_cointoss);
	PrintCvar(g_allow_vote_customTeams);
	PrintCvar(g_allow_vote_forceclass);
	PrintCvar(g_allow_vote_fraglimit);
	PrintCvar(g_allow_vote_gametype);
	PrintCvar(g_allow_vote_kick);
	PrintCvar(g_allow_vote_killturrets);
	PrintCvar(g_allow_vote_quickSpawns);
	PrintCvar(g_allow_vote_lockteams);
	PrintCvar(g_allow_vote_map);
	PrintCvar(g_allow_vote_maprandom);
	PrintCvar(g_allow_vote_nextpug);
	PrintCvar(g_allow_vote_nextmap);
	PrintCvar(g_allow_vote_pub);
	PrintCvar(g_allow_vote_pug);
	PrintCvar(g_allow_vote_q);
	PrintCvar(g_allow_vote_randomcapts);
	PrintCvar(g_allow_vote_randomteams);
	PrintCvar(g_allow_vote_restart);
	PrintCvar(g_allow_vote_timelimit);
	PrintCvar(g_allow_vote_warmup);
	PrintCvar(g_allow_vote_zombies);
	PrintCvar(g_antiCallvoteTakeover);
	PrintCvar(g_antiHothCodesLiftLame);
	PrintCvar(g_antiHothHangarLiftLame);
	PrintCvar(g_antiHothInfirmaryLiftLame);
	PrintCvar(g_antiSelfMax);
	PrintCvar(g_autoKorribanFloatingItems);
	PrintCvar(g_autoKorribanSpam);
	PrintCvar(g_autoPause999);
	PrintCvar(g_autoPauseDisconnect);
	PrintCvar(g_autoSpec);
	PrintCvar(g_autoStats);
	PrintCvar(g_antiLaming);
	PrintCvar(g_autoResetCustomTeams);
	PrintCvar(g_damageFixes);
	PrintCvar(g_botAimbot);
	PrintCvar(g_botJumping);
	PrintCvar(g_breakRNG);
	PrintCvar(g_coneReflectAngle);
	PrintCvar(g_dismember);
	PrintCvar(g_dispenserLifetime);
	PrintCvar(g_emotes);
	PrintCvar(g_enableCloak);
	PrintCvar(g_siegeTiebreakEnd);
	PrintCvar(g_fixDempSaberThrow);
	PrintCvar(g_fixDodge);
	PrintCvar(g_fixEweb);
	PrintCvar(g_fixFallingSounds);
	PrintCvar(g_fixGripKills);
	PrintCvar(g_fixHothBunkerLift);
	PrintCvar(g_fixHothDoorSounds);
	PrintCvar(g_fixHothHangarTurrets);
	PrintCvar(g_fixLiftkillTraps);
	PrintCvar(g_fixPitKills);
	PrintCvar(g_fixRancorCharge);
	PrintCvar(g_fixShield);
	PrintCvar(g_fixSiegeScoring);
	PrintCvar(g_fixVoiceChat);
	PrintCvar(g_flechetteSpread);
	PrintCvar(g_floatingItems);
	PrintCvar(g_forceDTechItems);
	PrintCvar(g_friendlyFreeze);
	PrintCvar(g_healWalkerWithAmmoCans);
	PrintCvar(g_hothRebalance);
	PrintCvar(g_hothHangarHack);
	PrintCvar(g_improvedDisarm);
	PrintCvar(g_improvedTeamchat);
	PrintCvar(g_infiniteCharge);
	PrintCvar(g_intermissionKnockbackNPCs);
	PrintCvar(g_unlagged);
#ifdef _DEBUG
	PrintCvar(g_unlaggedDebug);
	PrintCvar(g_unlaggedFactor);
	PrintCvar(g_unlaggedMaxCompensation);
	PrintCvar(g_unlaggedOffset);
	PrintCvar(g_unlaggedSkeletonTime);
#endif
	PrintCvar(g_knockback);
	PrintCvar(g_locationBasedDamage);
	PrintCvar(g_moreTaunts);
	PrintCvar(g_multiUseGenerators);
	PrintCvar(g_multiVoteRNG);
	PrintCvar(g_nextmapWarning);
	PrintCvar(g_notifyNotLive);
	PrintCvar(g_quickPauseChat);
	PrintCvar(g_randomConeReflection);
	PrintCvar(g_requireMoreCustomTeamVotes);
	PrintCvar(g_removeHothHangarTurrets);
	PrintCvar(g_rocketSurfing);
	PrintCvar(g_runoffVote);
	PrintCvar(g_saberDamageScale);
	PrintCvar(g_saberHitsToKillSentry);
	PrintCvar(g_sexyDisruptor);
	PrintCvar(g_siegeHelp);
	PrintCvar(g_siegeReflectionFix);
	PrintCvar(g_specInfo);
	PrintCvar(g_swoopKillPoints);
	PrintCvar(g_techAmmoForAllWeapons);
	PrintCvar(iLikeToDoorSpam);
	PrintCvar(iLikeToMineSpam);
	PrintCvar(iLikeToShieldSpam);
	PrintCvar(g_improvedHoming);
	PrintCvar(g_improvedHomingThreshold);
	trap_SendServerCommand(ent - g_entities, va("print \"If the cvar you are looking for is not listed here, use regular ^5/serverstatus^7 command instead\n\""));
}

void Cmd_SiegeDuel_f(gentity_t *ent)
{
	trace_t tr;
	vec3_t forward, fwdOrg;
	int duelrange = 2048;
	int i;
	int n;
	int numPlayersInGame = 0;

	if (!g_privateDuel.integer)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"Private duels are not enabled on this server.\n\""));
		return;
	}

	if (ent->client->ps.saberInFlight)
	{
		return;
	}

	if (ent->client->sess.siegeDuelInProgress)
	{
		//already dueling
		return;
	}

	if (ent->health <= 0 || ent->client->tempSpectate >= level.time)
	{
		//no dueling while dead
		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_RED && ent->client->sess.sessionTeam != TEAM_BLUE)
	{
		//no spec dueling, lol
		return;
	}

	if (ent->client->ps.m_iVehicleNum)
	{
		//no challenging duels while riding in a vehicle...
		return;
	}

	if (ent->client->sess.siegeDuelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.emplacedIndex || ent->client->ewebIndex)
	{
		//no challenging people when you're ewebing/emplaced gunning, silly.
		return;
	}
	
	if (ent->client->jetPackOn)
	{
		//no challenging while jetpacking
		return;
	}

	for (i = 0; i < level.maxclients; i++)
	{
		if (level.clients[i].pers.connected != CON_DISCONNECTED &&
			(level.clients[i].sess.sessionTeam == TEAM_RED || level.clients[i].sess.sessionTeam == TEAM_BLUE) &&
			!(g_entities[i].r.svFlags & SVF_BOT)) //connected player who is ingame and not a bot
		{
			if (level.clients[i].sess.siegeDuelInProgress)
			{
				//????????????????????
				trap_SendServerCommand(ent - g_entities, "print \"There is already a duel in progress.\n\"");
				return;
			}
			numPlayersInGame++;
		}
	}

	if (numPlayersInGame != 2)
	{
		trap_SendServerCommand(ent - g_entities, "print \"There must be exactly two players in-game to start a duel.\n\"");
		return;
	}

	AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);

	fwdOrg[0] = ent->client->ps.origin[0] + forward[0] * duelrange;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1] * duelrange;
	fwdOrg[2] = (ent->client->ps.origin[2] + ent->client->ps.viewheight) + forward[2] * duelrange;

	trap_Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID);

	if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS)
	{
		gentity_t *challenged = &g_entities[tr.entityNum];

		if (!challenged || !challenged->client || !challenged->inuse ||
			challenged->health < 1 || challenged->client->ps.stats[STAT_HEALTH] < 1 || challenged->client->ps.m_iVehicleNum || challenged->client->ps.emplacedIndex || challenged->client->ewebIndex ||
			challenged->client->ps.saberInFlight || challenged->client->tempSpectate >= level.time ||
			(challenged->client->sess.sessionTeam != TEAM_RED && challenged->client->sess.sessionTeam != TEAM_BLUE))
		{
			return;
		}

		if (OnSameTeam(ent, challenged))
		{
			return;
		}

		if (challenged->client->jetPackOn)
		{
			//no challenging while jetpacking
			return;
		}

		if (challenged->client->sess.siegeDuelIndex == ent->s.number && challenged->client->sess.siegeDuelTime >= level.time)
		{

			trap_SendServerCommand(-1, va("print \"%s^7 %s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname));
			trap_SendServerCommand(ent - g_entities, va("cp \"Get ready...\n\""));
			trap_SendServerCommand(challenged - g_entities, va("cp \"Get ready...\n\""));
			ent->client->sess.siegeDuelInProgress = 1;
			challenged->client->sess.siegeDuelInProgress = 1;
			//this will define that they are dueling

			ent->client->sess.siegeDuelTime = level.time + 3000;
			challenged->client->sess.siegeDuelTime = level.time + 3000;
			//"get ready" phase

			ent->client->ps.stats[STAT_ARMOR] = 0;
			challenged->client->ps.stats[STAT_ARMOR] = 0;

			ent->client->ps.fd.forcePower = 100;
			challenged->client->ps.fd.forcePower = 100;

			//only pistol in captain duel
			ent->client->preduelWeaps = ent->client->ps.stats[STAT_WEAPONS];
			challenged->client->preduelWeaps = challenged->client->ps.stats[STAT_WEAPONS];

			ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
			challenged->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
			ent->client->ps.weapon = WP_MELEE;
			challenged->client->ps.weapon = WP_MELEE;
			//give them melee; then switch to pistol after (like a quick draw thing)

			//enforce 100 hp in siege
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 100;
			challenged->client->ps.stats[STAT_HEALTH] = challenged->health = 100;

			//automatically kill turrets for siege
			Svcmd_KillTurrets_f(qfalse);
			//give everyone 125% speed
			ent->client->ps.speed = ent->client->ps.basespeed = challenged->client->ps.speed = challenged->client->ps.basespeed = (g_speed.value * 1.25);

			//need to remove all force powers for clientside prediction. it's not enough to simply disable them from being used.
			for (n = 0; n < NUM_FORCE_POWERS; n++)
			{
				challenged->client->ps.fd.forcePowerLevel[n] = ent->client->ps.fd.forcePowerLevel[n] = 0;
				ent->client->ps.fd.forcePowersKnown &= ~(1 << n);
				challenged->client->ps.fd.forcePowersKnown &= ~(1 << n);
			}

		}
		else
		{
			//Print the message that a player has been challenged in private, only announce the actual duel initiation in private
			trap_SendServerCommand(challenged - g_entities, va("cp \"%s^7 %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")));
			trap_SendServerCommand(ent - g_entities, va("cp \"%s %s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname));
		}

		ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
		ent->client->ps.forceHandExtendTime = level.time + 1000;

		ent->client->sess.siegeDuelIndex = challenged->s.number; //duelIndex
		ent->client->sess.siegeDuelTime = level.time + 5000; //duelTime
	}
}

void Cmd_EngageDuel_f(gentity_t *ent)
{
	trace_t tr;
	vec3_t forward, fwdOrg;
	int duelrange = (g_gametype.integer == GT_CTF) ? 1024 : 256;

	if (!g_privateDuel.integer)
	{
		return;
	}

	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
	{ //rather pointless in this mode..
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_CTF) 
	{ //no private dueling in team modes, except captain duel in ctf
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (g_gametype.integer == GT_CTF ){
		if (ent->client->ps.weapon != WP_BRYAR_PISTOL
			&& ent->client->ps.weapon != WP_SABER
			&& ent->client->ps.weapon != WP_MELEE){
			return; //pistol duel in ctf
		}
	} else {
		if (ent->client->ps.weapon != WP_SABER) {
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		return;
	}

	if (ent->client->ps.duelInProgress)
	{
		return;
	}

	//New: Don't let a player duel if he just did and hasn't waited 10 seconds yet (note: If someone challenges him, his duel timer will reset so he can accept)
	if (ent->client->ps.fd.privateDuelTime > level.time)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_JUSTDID")) );
		return;
	}

	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );

	fwdOrg[0] = ent->client->ps.origin[0] + forward[0]*duelrange;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1]*duelrange;
	fwdOrg[2] = (ent->client->ps.origin[2]+ent->client->ps.viewheight) + forward[2]*duelrange;

	trap_Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID);

	if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS)
	{
		gentity_t *challenged = &g_entities[tr.entityNum];

		if (!challenged || !challenged->client || !challenged->inuse ||
			challenged->health < 1 || challenged->client->ps.stats[STAT_HEALTH] < 1 ||
			 challenged->client->ps.duelInProgress ||
			challenged->client->ps.saberInFlight)
		{
			return;
		}

		if (g_gametype.integer == GT_CTF ){
			if ((challenged->client->ps.weapon != WP_SABER)
				&& (challenged->client->ps.weapon != WP_BRYAR_PISTOL)
				&& (challenged->client->ps.weapon != WP_MELEE)){ 
				return;
			}
		} else {
			if (challenged->client->ps.weapon != WP_SABER){
				return;
			}
		}

		if (g_gametype.integer >= GT_TEAM && OnSameTeam(ent, challenged))
		{
			return;
		}

		//auto accept for testing, remove in release!
		if (challenged->client->ps.duelIndex == ent->s.number && challenged->client->ps.duelTime >= level.time)
		{
			int lifes, shields;

			trap_SendServerCommand(-1, va("print \"%s^7 %s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname) );

			ent->client->ps.duelInProgress = qtrue;
			challenged->client->ps.duelInProgress = qtrue;

			ent->client->ps.duelTime = level.time + 2000;
			challenged->client->ps.duelTime = level.time + 2000;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 1);
			G_AddEvent(challenged, EV_PRIVATE_DUEL, 1);

			//set default duel values for life and shield
			shields = g_duelShields.integer;

			if (shields > 100)
				shields = 100;

			ent->client->ps.stats[STAT_ARMOR] = shields;
			challenged->client->ps.stats[STAT_ARMOR] = shields;

			ent->client->ps.fd.forcePower = 100;
			challenged->client->ps.fd.forcePower = 100;

			if (g_gametype.integer == GT_CTF){
				//only meele and blaster in captain duel
				ent->client->preduelWeaps = ent->client->ps.stats[STAT_WEAPONS];
				challenged->client->preduelWeaps = challenged->client->ps.stats[STAT_WEAPONS];

				ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
				challenged->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
				ent->client->ps.weapon = WP_MELEE;
				challenged->client->ps.weapon = WP_MELEE;
			}

			lifes = g_duelLifes.integer;

			if (lifes < 1)
				lifes = 1;
			else if (lifes > ent->client->ps.stats[STAT_MAX_HEALTH])
				lifes = ent->client->ps.stats[STAT_MAX_HEALTH];

			ent->client->ps.stats[STAT_HEALTH] = ent->health = lifes;
			challenged->client->ps.stats[STAT_HEALTH] = challenged->health = lifes;
			
			//remove weapons and powerups here for ctf captain duel, 
			//disable saber and all forces except jump
			//watchout for flags etc
			if (g_gametype.integer == GT_CTF){
				int i;
				//return flags
				if ( ent->client->ps.powerups[PW_NEUTRALFLAG] ) {		
					Team_ReturnFlag( TEAM_FREE );
					ent->client->ps.powerups[PW_NEUTRALFLAG] = 0;
				}
				else if ( ent->client->ps.powerups[PW_REDFLAG] ) {		
					Team_ReturnFlag( TEAM_RED );
					ent->client->ps.powerups[PW_REDFLAG] = 0;
				}
				else if ( ent->client->ps.powerups[PW_BLUEFLAG] ) {	
					Team_ReturnFlag( TEAM_BLUE );
					ent->client->ps.powerups[PW_BLUEFLAG] = 0;
				}
				if ( challenged->client->ps.powerups[PW_NEUTRALFLAG] ) {		
					Team_ReturnFlag( TEAM_FREE );
					challenged->client->ps.powerups[PW_NEUTRALFLAG] = 0;
				}
				else if ( challenged->client->ps.powerups[PW_REDFLAG] ) {		
					Team_ReturnFlag( TEAM_RED );
					challenged->client->ps.powerups[PW_REDFLAG] = 0;
				}
				else if ( challenged->client->ps.powerups[PW_BLUEFLAG] ) {	
					Team_ReturnFlag( TEAM_BLUE );
					challenged->client->ps.powerups[PW_BLUEFLAG] = 0;
				}

				//remove other powerups
				for(i=0;i<PW_NUM_POWERUPS;++i){
					ent->client->ps.powerups[i] = 0;
					challenged->client->ps.powerups[i] = 0;
				}
			}


			//Holster their sabers now, until the duel starts (then they'll get auto-turned on to look cool)

			if (!ent->client->ps.saberHolstered)
			{
				if (ent->client->saber[0].soundOff)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				}
				if (ent->client->saber[1].soundOff &&
					ent->client->saber[1].model[0])
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
				}
				ent->client->ps.weaponTime = 400;
				ent->client->ps.saberHolstered = 2;
			}
			if (!challenged->client->ps.saberHolstered)
			{
				if (challenged->client->saber[0].soundOff)
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[0].soundOff);
				}
				if (challenged->client->saber[1].soundOff &&
					challenged->client->saber[1].model[0])
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[1].soundOff);
				}
				challenged->client->ps.weaponTime = 400;
				challenged->client->ps.saberHolstered = 2;
			}
		}
		else
		{
			//Print the message that a player has been challenged in private, only announce the actual duel initiation in private
			trap_SendServerCommand( challenged-g_entities, va("cp \"%s^7 %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")) );
			trap_SendServerCommand( ent-g_entities, va("cp \"%s %s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname) );
		}

		challenged->client->ps.fd.privateDuelTime = 0; //reset the timer in case this player just got out of a duel. He should still be able to accept the challenge.

		ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
		ent->client->ps.forceHandExtendTime = level.time + 1000;

		ent->client->ps.duelIndex = challenged->s.number;
		ent->client->ps.duelTime = level.time + 5000;
	}
}

#if 0//#ifndef FINAL_BUILD
extern stringID_table_t animTable[MAX_ANIMATIONS+1];

void Cmd_DebugSetSaberMove_f(gentity_t *self)
{
	int argNum = trap_Argc();
	char arg[MAX_STRING_CHARS];

	if (argNum < 2)
	{
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	self->client->ps.saberMove = atoi(arg);
	self->client->ps.saberBlocked = BLOCKED_BOUNCE_MOVE;

	if (self->client->ps.saberMove >= LS_MOVE_MAX)
	{
		self->client->ps.saberMove = LS_MOVE_MAX-1;
	}

	Com_Printf("Anim for move: %s\n", animTable[saberMoveData[self->client->ps.saberMove].animToUse].name);
}

void Cmd_DebugSetBodyAnim_f(gentity_t *self, int flags)
{
	int argNum = trap_Argc();
	char arg[MAX_STRING_CHARS];
	int i = 0;

	if (argNum < 2)
	{
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	while (i < MAX_ANIMATIONS)
	{
		if (!Q_stricmp(arg, animTable[i].name))
		{
			break;
		}
		i++;
	}

	if (i == MAX_ANIMATIONS)
	{
		Com_Printf("Animation '%s' does not exist\n", arg);
		return;
	}

	G_SetAnim(self, NULL, SETANIM_BOTH, i, flags, 0);

	Com_Printf("Set body anim to %s\n", arg);
}
#endif

void StandardSetBodyAnim(gentity_t *self, int anim, int flags)
{
	G_SetAnim(self, NULL, SETANIM_BOTH, anim, flags, 0);
}

void DismembermentTest(gentity_t *self);

void Bot_SetForcedMovement(int bot, int forward, int right, int up);

#if 0//#ifndef FINAL_BUILD
extern void DismembermentByNum(gentity_t *self, int num);
extern void G_SetVehDamageFlags( gentity_t *veh, int shipSurf, int damageLevel );
#endif

static int G_ClientNumFromNetname(char *name)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client &&
			!Q_stricmp(ent->client->pers.netname, name))
		{
			return ent->s.number;
		}
		i++;
	}

	return -1;
}

qboolean TryGrapple(gentity_t *ent)
{
	if (ent->client->ps.weaponTime > 0)
	{ //weapon busy
		return qfalse;
	}
	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //force power or knockdown or something
		return qfalse;
	}
	if (ent->client->grappleState)
	{ //already grappling? but weapontime should be > 0 then..
		return qfalse;
	}

	if (ent->client->ps.weapon != WP_SABER && ent->client->ps.weapon != WP_MELEE)
	{
		return qfalse;
	}

	if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
	{
		Cmd_ToggleSaber_f(ent);
		if (!ent->client->ps.saberHolstered)
		{ //must have saber holstered
			return qfalse;
		}
	}

	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_GRAB, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	if (ent->client->ps.torsoAnim == BOTH_KYLE_GRAB)
	{ //providing the anim set succeeded..
		ent->client->ps.torsoTimer += 500; //make the hand stick out a little longer than it normally would
		if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim)
		{
			ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		return qtrue;
	}

	return qfalse;
}

#if 0//#ifndef FINAL_BUILD
qboolean saberKnockOutOfHand(gentity_t *saberent, gentity_t *saberOwner, vec3_t velocity);
#endif

/*
=================
ClientCommand
=================
*/
void ClientCommand( int clientNum ) {
	gentity_t *ent;
	char	cmd[MAX_TOKEN_CHARS];

	ent = g_entities + clientNum;
	if ( !ent->client ) {
		return;		// not fully in game yet
	}


	trap_Argv( 0, cmd, sizeof( cmd ) );

	//rww - redirect bot commands
	if (strstr(cmd, "bot_") && AcceptBotCommand(cmd, ent))
	{
		return;
	}
	//end rww

	if (Q_stricmp (cmd, "say") == 0 || !Q_stricmp(cmd, "sm_say")) {
		Cmd_Say_f (ent, SAY_ALL, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0 || !Q_stricmp(cmd, "sm_say_team")) {
		if (g_gametype.integer < GT_TEAM)
		{ //not a team game, just refer to regular say.
			Cmd_Say_f (ent, SAY_ALL, qfalse);
		}
		else
		{
			Cmd_Say_f (ent, SAY_TEAM, qfalse);
		}
		return;
	}
	if (Q_stricmp (cmd, "tell") == 0) {
		Cmd_Tell_f ( ent, NULL );
		return;
	}

	if (Q_stricmp(cmd, "voice_cmd") == 0)
	{
		Cmd_VoiceCommand_f(ent);
		return;
	}

	if (Q_stricmp (cmd, "score") == 0) {
		Cmd_Score_f (ent);
		return;
	}

	if (!Q_stricmp(cmd, "unlagged")) {
		Cmd_Unlagged_f(ent);
		return;
	}

	if (Q_stricmp(cmd, "inkognito") == 0) {
		if (ent && ent->client){
			ent->client->sess.isInkognito = ent->client->sess.isInkognito ? qfalse : qtrue;

			trap_SendServerCommand( ent-g_entities, 
			va("print \"Inkognito %s\n\"", ent->client->sess.isInkognito ? "ON" : "OFF"));
#ifdef NEWMOD_SUPPORT
			trap_SendServerCommand(ent - g_entities, va("kls -1 -1 \"inko\" \"%d\"", ent->client->sess.isInkognito));
#endif
		}
		return;
	}

	// ignore all other commands when at intermission
	if (level.intermissiontime) {
		if (!Q_stricmp(cmd, "forcechanged"))
		{ //special case: still update force change
			Cmd_ForceChanged_f(ent);
		}
		else if (!Q_stricmp(cmd, "ctfstats") || !Q_stricmp(cmd, "stats") || !Q_stricmp(cmd, "siegestats"))
		{ // special case: we want people to read their other stats as suggested
			Cmd_PrintStats_f(ent);
		}
#ifdef NEWMOD_SUPPORT
		else if (Q_stricmp(cmd, "svauth") == 0 && ent->client->sess.auth > PENDING && ent->client->sess.auth < AUTHENTICATED) {
			Cmd_Svauth_f(ent);
		}
		else if (!Q_stricmp(cmd, "sci")) {
			Cmd_Sci_f(ent);
		}
#endif
		else if (Q_stricmp(cmd, "changes") == 0)
			Cmd_Changes_f(ent);
		else if (!Q_stricmp(cmd, "skillboost"))
			Cmd_SkillBoost_f(ent);
		else if (!Q_stricmp(cmd, "senseboost"))
			Cmd_SenseBoost_f(ent);
		else if (!Q_stricmp(cmd, "aimboost"))
			Cmd_AimBoost_f(ent);
		else if (!Q_stricmp(cmd, "pugmaps"))
			Cmd_PugMaps_f(ent);
		else if (Q_stricmp(cmd, "whois") == 0)
			Cmd_WhoIs_f(ent);
		else if (Q_stricmp(cmd, "ignore") == 0)
			Cmd_Ignore_f(ent);
		else if (Q_stricmp(cmd, "serverstatus2") == 0)
			Cmd_ServerStatus2_f(ent);
		else if (Q_stricmp(cmd, "info") == 0 || Q_stricmp(cmd, "help") == 0 || Q_stricmp(cmd, "rules") == 0)
			Cmd_Help_f(ent);
		else if (!Q_stricmp(cmd, "toptimes") || !Q_stricmp(cmd, "fastcaps"))
			Cmd_TopTimes_f(ent);
		else if (!Q_stricmp(cmd, "classes"))
			Cmd_Classes_f(ent);
		else if (!Q_stricmp(cmd, "emote"))
			Cmd_Emote_f(ent);
#ifdef NEWMOD_SUPPORT
		else if (!Q_stricmp(cmd, "vchat"))
			Cmd_Vchat_f(ent);
		else if (!Q_stricmp(cmd, "vchatdl"))
			Cmd_VchatDl_f(ent);
		else if (!Q_stricmp(cmd, "vchl"))
			Cmd_VchatList_f(ent);
#endif
		else
			trap_SendServerCommand( clientNum, va("print \"%s (%s) \n\"", G_GetStringEdString("MP_SVGAME", "CANNOT_TASK_INTERMISSION"), cmd ) );
		return;
	}

	if (Q_stricmp(cmd, "give") == 0)
	{
		Cmd_Give_f(ent, 0);
	}
	else if (Q_stricmp(cmd, "greendoors") == 0)
	{
		Cmd_GreenDoors_f(ent);
	}
	else if (Q_stricmp(cmd, "killturrets") == 0)
	{
		Cmd_KillTurrets_f(ent);
	}
	else if (Q_stricmp(cmd, "duotest") == 0)
	{
		Cmd_DuoTest_f(ent);
	}
	else if (Q_stricmp(cmd, "giveother") == 0)
	{ //for debugging pretty much
		Cmd_Give_f(ent, 1);
	}
	else if (Q_stricmp(cmd, "t_use") == 0 && CheatsOk(ent))
	{ //debug use map object
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			gentity_t *targ;

			trap_Argv(1, sArg, sizeof(sArg));
			targ = G_Find(NULL, FOFS(targetname), sArg);

			while (targ)
			{
				if (targ->use)
				{
					CheckSiegeHelpFromUse(targ->targetname);
					targ->use(targ, ent, ent);
				}
				targ = G_Find(targ, FOFS(targetname), sArg);
			}
		}
	}
	else if (Q_stricmp(cmd, "god") == 0)
		Cmd_God_f(ent);
	else if (Q_stricmp(cmd, "notarget") == 0)
		Cmd_Notarget_f(ent);
	else if (Q_stricmp(cmd, "noclip") == 0)
		Cmd_Noclip_f(ent);
	else if (Q_stricmp(cmd, "NPC") == 0 && CheatsOk(ent))
	{
		Cmd_NPC_f(ent);
	}
	else if (Q_stricmp(cmd, "kill") == 0)
		Cmd_Kill_f(ent);
	else if (Q_stricmp(cmd, "levelshot") == 0)
		Cmd_LevelShot_f(ent);
	else if (Q_stricmp(cmd, "follow") == 0)
		Cmd_Follow_f(ent);
	else if (Q_stricmp(cmd, "follownext") == 0)
		Cmd_FollowCycle_f(ent, 1);
	else if (Q_stricmp(cmd, "followprev") == 0)
		Cmd_FollowCycle_f(ent, -1);
	else if (Q_stricmp(cmd, "followflag") == 0)
		Cmd_FollowFlag_f(ent);
	else if (Q_stricmp(cmd, "followtarget") == 0)
		Cmd_FollowTarget_f(ent);
	else if (Q_stricmp(cmd, "team") == 0)
		Cmd_Team_f(ent);
	else if (Q_stricmp(cmd, "duelteam") == 0)
		Cmd_DuelTeam_f(ent);
	else if (Q_stricmp(cmd, "siegeclass") == 0)
		Cmd_SiegeClass_f(ent);
	else if (Q_stricmp(cmd, "class") == 0)
		Cmd_Class_f(ent);
	else if (Q_stricmp(cmd, "join") == 0)
		Cmd_Join_f(ent);
	else if (Q_stricmp(cmd, "forcechanged") == 0)
		Cmd_ForceChanged_f(ent);
	else if (Q_stricmp(cmd, "where") == 0)
		Cmd_Where_f(ent);
	else if (Q_stricmp(cmd, "targetinfo") == 0)
		Cmd_TargetInfo_f(ent);
	else if (Q_stricmp(cmd, "changes") == 0)
		Cmd_Changes_f(ent);
	else if (!Q_stricmp(cmd, "skillboost"))
		Cmd_SkillBoost_f(ent);
	else if (!Q_stricmp(cmd, "senseboost"))
		Cmd_SenseBoost_f(ent);
	else if (!Q_stricmp(cmd, "aimboost"))
		Cmd_AimBoost_f(ent);
	else if (!Q_stricmp(cmd, "pugmaps"))
		Cmd_PugMaps_f(ent);
	else if (!Q_stricmp(cmd, "anim"))
		Cmd_Anim_f(ent);
	else if (!Q_stricmp(cmd, "getanim"))
		Cmd_GetAnim_f(ent);
	else if (Q_stricmp(cmd, "killtarget") == 0)
		Cmd_KillTarget_f(ent);
	else if (Q_stricmp(cmd, "callvote") == 0)
		Cmd_CallVote_f(ent, PAUSE_NONE);
	else if (!Q_stricmp(cmd, "pause"))
		Cmd_CallVote_f(ent, PAUSE_PAUSED); // allow "pause" command as alias for "callvote pause"
	else if (!Q_stricmp(cmd, "unpause"))
		Cmd_CallVote_f(ent, PAUSE_UNPAUSING); // allow "unpause" command as alias for "callvote unpause"
	else if (Q_stricmp (cmd, "vote") == 0)
		Cmd_Vote_f (ent);
	else if (Q_stricmp(cmd, "callteamvote") == 0)
		Cmd_CallTeamVote_f(ent);
	else if (Q_stricmp(cmd, "teamvote") == 0)
		Cmd_TeamVote_f(ent);
	else if (Q_stricmp(cmd, "ready") == 0)
		Cmd_Ready_f(ent);
    else if ( Q_stricmp( cmd, "whois" ) == 0 )
        Cmd_WhoIs_f( ent );
	else if (Q_stricmp(cmd, "ctfstats") == 0 || Q_stricmp(cmd, "stats") == 0 || Q_stricmp(cmd, "siegestats") == 0)
		Cmd_PrintStats_f(ent);
	else if (Q_stricmp( cmd, "help" ) == 0 || Q_stricmp( cmd, "rules" ) == 0)
		Cmd_Help_f( ent );
	else if (Q_stricmp (cmd, "gc") == 0)
		Cmd_GameCommand_f( ent );
	else if (Q_stricmp (cmd, "setviewpos") == 0)
		Cmd_SetViewpos_f( ent );
	else if (Q_stricmp (cmd, "ignore") == 0)
		Cmd_Ignore_f( ent );
	else if (Q_stricmp(cmd, "testcmd") == 0)
		Cmd_TestCmd_f(ent);
	else if (Q_stricmp(cmd, "serverstatus2") == 0)
		Cmd_ServerStatus2_f(ent);
	else if (Q_stricmp(cmd, "info") == 0)//stupid openjk broke my command
		Cmd_Help_f(ent);
	else if (Q_stricmp(cmd, "testvis") == 0)
		Cmd_TestVis_f(ent);
	else if (Q_stricmp (cmd, "testcmd") == 0)
		Cmd_TestCmd_f( ent );
	else if ( !Q_stricmp(cmd, "toptimes") || !Q_stricmp( cmd, "fastcaps" ) )
		Cmd_TopTimes_f( ent );
	else if (!Q_stricmp(cmd, "classes"))
		Cmd_Classes_f(ent);
	else if (!Q_stricmp(cmd, "emote"))
		Cmd_Emote_f(ent);
	
#ifdef NEWMOD_SUPPORT
	else if (!Q_stricmp(cmd, "vchat"))
		Cmd_Vchat_f(ent);
	else if (!Q_stricmp(cmd, "vchatdl"))
		Cmd_VchatDl_f(ent);
	else if (!Q_stricmp(cmd, "vchl"))
		Cmd_VchatList_f(ent);
#endif
	else if (!Q_stricmp(cmd, "use_pack"))
		Cmd_UsePack_f(ent);
	else if (!Q_stricmp(cmd, "use_dispenser"))
		Cmd_UseDispenser_f(ent);
#if 0
	else if (!Q_stricmp(cmd, "use_healing"))
		Cmd_UseHealing_f(ent);
#endif
	else if (!Q_stricmp(cmd, "use_anybacta"))
		Cmd_UseAnyBacta_f(ent);
#ifdef NEWMOD_SUPPORT
	else if ( Q_stricmp( cmd, "svauth" ) == 0 && ent->client->sess.auth > PENDING && ent->client->sess.auth < AUTHENTICATED )
		Cmd_Svauth_f( ent );
	else if (!Q_stricmp(cmd, "sci"))
		Cmd_Sci_f(ent);
#endif
		
	//for convenient powerduel testing in release
	else if (Q_stricmp(cmd, "killother") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int entNum = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );

			entNum = G_ClientNumFromNetname(sArg);

			if (entNum >= 0 && entNum < MAX_GENTITIES)
			{
				gentity_t *kEnt = &g_entities[entNum];

				if (kEnt->inuse && kEnt->client)
				{
					kEnt->flags &= ~FL_GODMODE;
					kEnt->client->ps.stats[STAT_HEALTH] = kEnt->health = -999;
					player_die (kEnt, kEnt, kEnt, 100000, MOD_SUICIDE);
				}
			}
		}
	}
#if 0//#ifdef _DEBUG
	else if (Q_stricmp(cmd, "relax") == 0 && CheatsOk( ent ))
	{
		if (ent->client->ps.eFlags & EF_RAG)
		{
			ent->client->ps.eFlags &= ~EF_RAG;
		}
		else
		{
			ent->client->ps.eFlags |= EF_RAG;
		}
	}
	else if (Q_stricmp(cmd, "holdme") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int entNum = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );

			entNum = atoi(sArg);

			if (entNum >= 0 &&
				entNum < MAX_GENTITIES)
			{
				gentity_t *grabber = &g_entities[entNum];

				if (grabber->inuse && grabber->client && grabber->ghoul2)
				{
					if ( G_IsPlayer(grabber) )
					{ //switch cl 0 and entitynum_none, so we can operate on the "if non-0" concept
						ent->client->ps.ragAttach = ENTITYNUM_NONE;
					}
					else
					{
						ent->client->ps.ragAttach = grabber->s.number;
					}
				}
			}
		}
		else
		{
			ent->client->ps.ragAttach = 0;
		}
	}
	else if (Q_stricmp(cmd, "limb_break") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int breakLimb = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );
			if (!Q_stricmp(sArg, "right"))
			{
				breakLimb = BROKENLIMB_RARM;
			}
			else if (!Q_stricmp(sArg, "left"))
			{
				breakLimb = BROKENLIMB_LARM;
			}

			G_BreakArm(ent, breakLimb);
		}
	}
	else if (Q_stricmp(cmd, "headexplodey") == 0 && CheatsOk( ent ))
	{
		Cmd_Kill_f (ent);
		if (ent->health < 1)
		{
			DismembermentTest(ent);
		}
	}
	else if (Q_stricmp(cmd, "debugstupidthing") == 0 && CheatsOk( ent ))
	{
		int i = 0;
		gentity_t *blah;
		while (i < MAX_GENTITIES)
		{
			blah = &g_entities[i];
			if (blah->inuse && blah->classname && blah->classname[0] && !Q_stricmp(blah->classname, "NPC_Vehicle"))
			{
				Com_Printf("Found it.\n");
			}
			i++;
		}
	}
	else if (Q_stricmp(cmd, "arbitraryprint") == 0 && CheatsOk( ent ))
	{
		trap_SendServerCommand( -1, va("cp \"Blah blah blah\n\""));
	}
	else if (Q_stricmp(cmd, "handcut") == 0 && CheatsOk( ent ))
	{
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		if (trap_Argc() > 1)
		{
			trap_Argv( 1, sarg, sizeof( sarg ) );

			if (sarg[0])
			{
				bCl = atoi(sarg);

				if (bCl >= 0 && bCl < MAX_GENTITIES)
				{
					gentity_t *hEnt = &g_entities[bCl];

					if (hEnt->client)
					{
						if (hEnt->health > 0)
						{
							gGAvoidDismember = 1;
							hEnt->flags &= ~FL_GODMODE;
							hEnt->client->ps.stats[STAT_HEALTH] = hEnt->health = -999;
							player_die (hEnt, hEnt, hEnt, 100000, MOD_SUICIDE);
						}
						gGAvoidDismember = 2;
						G_CheckForDismemberment(hEnt, ent, hEnt->client->ps.origin, 999, hEnt->client->ps.legsAnim, qfalse);
						gGAvoidDismember = 0;
					}
				}
			}
		}
	}
	else if (Q_stricmp(cmd, "loveandpeace") == 0 && CheatsOk( ent ))
	{
		trace_t tr;
		vec3_t fPos;

		AngleVectors(ent->client->ps.viewangles, fPos, 0, 0);

		fPos[0] = ent->client->ps.origin[0] + fPos[0]*40;
		fPos[1] = ent->client->ps.origin[1] + fPos[1]*40;
		fPos[2] = ent->client->ps.origin[2] + fPos[2]*40;

		trap_Trace(&tr, ent->client->ps.origin, 0, 0, fPos, ent->s.number, ent->clipmask);

		if (tr.entityNum < MAX_CLIENTS && tr.entityNum != ent->s.number)
		{

			gentity_t *other = &g_entities[tr.entityNum];

			if (other && other->inuse && other->client)
			{
				vec3_t entDir;
				vec3_t otherDir;
				vec3_t entAngles;
				vec3_t otherAngles;

				if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
				{
					Cmd_ToggleSaber_f(ent);
				}

				if (other->client->ps.weapon == WP_SABER && !other->client->ps.saberHolstered)
				{
					Cmd_ToggleSaber_f(other);
				}

				if ((ent->client->ps.weapon != WP_SABER || ent->client->ps.saberHolstered) &&
					(other->client->ps.weapon != WP_SABER || other->client->ps.saberHolstered))
				{
					VectorSubtract( other->client->ps.origin, ent->client->ps.origin, otherDir );
					VectorCopy( ent->client->ps.viewangles, entAngles );
					entAngles[YAW] = vectoyaw( otherDir );
					SetClientViewAngle( ent, entAngles );

					StandardSetBodyAnim(ent, /*BOTH_KISSER1LOOP*/BOTH_STAND1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;

					VectorSubtract( ent->client->ps.origin, other->client->ps.origin, entDir );
					VectorCopy( other->client->ps.viewangles, otherAngles );
					otherAngles[YAW] = vectoyaw( entDir );
					SetClientViewAngle( other, otherAngles );

					StandardSetBodyAnim(other, /*BOTH_KISSEE1LOOP*/BOTH_STAND1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
					other->client->ps.saberMove = LS_NONE;
					other->client->ps.saberBlocked = 0;
					other->client->ps.saberBlocking = 0;
				}
			}
		}
	}
#endif
	else if (Q_stricmp(cmd, "thedestroyer") == 0 && CheatsOk( ent ) && ent && ent->client && ent->client->ps.saberHolstered && ent->client->ps.weapon == WP_SABER)
	{
		Cmd_ToggleSaber_f(ent);

		if (!ent->client->ps.saberHolstered)
		{
		}
	}
	//begin bot debug cmds
	else if (Q_stricmp(cmd, "debugBMove_Forward") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, arg, -1, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Back") == 0 && CheatsOk(ent))
	{
		int arg = -4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, arg, -1, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Right") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, arg, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Left") == 0 && CheatsOk(ent))
	{
		int arg = -4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, arg, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Up") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, -1, arg);
	}
	//end bot debug cmds
#if 0//#ifndef FINAL_BUILD
	else if (Q_stricmp(cmd, "debugSetSaberMove") == 0)
	{
		Cmd_DebugSetSaberMove_f(ent);
	}
	else if (Q_stricmp(cmd, "debugSetBodyAnim") == 0)
	{
		Cmd_DebugSetBodyAnim_f(ent, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD);
	}
	else if (Q_stricmp(cmd, "debugDismemberment") == 0)
	{
		Cmd_Kill_f (ent);
		if (ent->health < 1)
		{
			char	arg[MAX_STRING_CHARS];
			int		iArg = 0;

			if (trap_Argc() > 1)
			{
				trap_Argv( 1, arg, sizeof( arg ) );

				if (arg[0])
				{
					iArg = atoi(arg);
				}
			}

			DismembermentByNum(ent, iArg);
		}
	}
	else if (Q_stricmp(cmd, "debugDropSaber") == 0)
	{
		if (ent->client->ps.weapon == WP_SABER &&
			ent->client->ps.saberEntityNum &&
			!ent->client->ps.saberInFlight)
		{
			saberKnockOutOfHand(&g_entities[ent->client->ps.saberEntityNum], ent, vec3_origin);
		}
	}
	else if (Q_stricmp(cmd, "debugKnockMeDown") == 0)
	{
		if (BG_KnockDownable(&ent->client->ps))
		{
			ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
			ent->client->ps.forceDodgeAnim = 0;
			if (trap_Argc() > 1)
			{
				ent->client->ps.forceHandExtendTime = level.time + 1100;
				ent->client->ps.quickerGetup = qfalse;
			}
			else
			{
				ent->client->ps.forceHandExtendTime = level.time + 700;
				ent->client->ps.quickerGetup = qtrue;
			}
		}
	}
	else if (Q_stricmp(cmd, "debugSaberSwitch") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client)
		{
			Cmd_ToggleSaber_f(targ);
		}
	}
	else if (Q_stricmp(cmd, "debugIKGrab") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client && ent->s.number != targ->s.number)
		{
			targ->client->ps.heldByClient = ent->s.number+1;
		}
	}
	else if (Q_stricmp(cmd, "debugIKBeGrabbedBy") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client && ent->s.number != targ->s.number)
		{
			ent->client->ps.heldByClient = targ->s.number+1;
		}
	}
	else if (Q_stricmp(cmd, "debugIKRelease") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client)
		{
			targ->client->ps.heldByClient = 0;
		}
	}
	else if (Q_stricmp(cmd, "debugThrow") == 0)
	{
		trace_t tr;
		vec3_t tTo, fwd;

		if (ent->client->ps.weaponTime > 0 || ent->client->ps.forceHandExtend != HANDEXTEND_NONE ||
			ent->client->ps.groundEntityNum == ENTITYNUM_NONE || ent->health < 1)
		{
			return;
		}

		AngleVectors(ent->client->ps.viewangles, fwd, 0, 0);
		tTo[0] = ent->client->ps.origin[0] + fwd[0]*32;
		tTo[1] = ent->client->ps.origin[1] + fwd[1]*32;
		tTo[2] = ent->client->ps.origin[2] + fwd[2]*32;

		trap_Trace(&tr, ent->client->ps.origin, 0, 0, tTo, ent->s.number, MASK_PLAYERSOLID);

		if (tr.fraction != 1)
		{
			gentity_t *other = &g_entities[tr.entityNum];

			if (other->inuse && other->client && other->client->ps.forceHandExtend == HANDEXTEND_NONE &&
				other->client->ps.groundEntityNum != ENTITYNUM_NONE && other->health > 0 &&
				(int)ent->client->ps.origin[2] == (int)other->client->ps.origin[2])
			{
				float pDif = 40.0f;
				vec3_t entAngles, entDir;
				vec3_t otherAngles, otherDir;
				vec3_t intendedOrigin;
				vec3_t boltOrg, pBoltOrg;
				vec3_t tAngles, vDif;
				vec3_t fwd, right;
				trace_t tr;
				trace_t tr2;

				VectorSubtract( other->client->ps.origin, ent->client->ps.origin, otherDir );
				VectorCopy( ent->client->ps.viewangles, entAngles );
				entAngles[YAW] = vectoyaw( otherDir );
				SetClientViewAngle( ent, entAngles );

				ent->client->ps.forceHandExtend = HANDEXTEND_PRETHROW;
				ent->client->ps.forceHandExtendTime = level.time + 5000;

				ent->client->throwingIndex = other->s.number;
				ent->client->doingThrow = level.time + 5000;
				ent->client->beingThrown = 0;

				VectorSubtract( ent->client->ps.origin, other->client->ps.origin, entDir );
				VectorCopy( other->client->ps.viewangles, otherAngles );
				otherAngles[YAW] = vectoyaw( entDir );
				SetClientViewAngle( other, otherAngles );

				other->client->ps.forceHandExtend = HANDEXTEND_PRETHROWN;
				other->client->ps.forceHandExtendTime = level.time + 5000;

				other->client->throwingIndex = ent->s.number;
				other->client->beingThrown = level.time + 5000;
				other->client->doingThrow = 0;

				//Doing this now at a stage in the throw, isntead of initially.

				G_EntitySound( other, CHAN_VOICE, G_SoundIndex("*pain100.wav") );
				G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
				G_Sound(other, CHAN_AUTO, G_SoundIndex( "sound/movers/objects/objectHit.wav" ));

				//see if we can move to be next to the hand.. if it's not clear, break the throw.
				VectorClear(tAngles);
				tAngles[YAW] = ent->client->ps.viewangles[YAW];
				VectorCopy(ent->client->ps.origin, pBoltOrg);
				AngleVectors(tAngles, fwd, right, 0);
				boltOrg[0] = pBoltOrg[0] + fwd[0]*8 + right[0]*pDif;
				boltOrg[1] = pBoltOrg[1] + fwd[1]*8 + right[1]*pDif;
				boltOrg[2] = pBoltOrg[2];

				VectorSubtract(boltOrg, pBoltOrg, vDif);
				VectorNormalize(vDif);

				VectorClear(other->client->ps.velocity);
				intendedOrigin[0] = pBoltOrg[0] + vDif[0]*pDif;
				intendedOrigin[1] = pBoltOrg[1] + vDif[1]*pDif;
				intendedOrigin[2] = other->client->ps.origin[2];

				trap_Trace(&tr, intendedOrigin, other->r.mins, other->r.maxs, intendedOrigin, other->s.number, other->clipmask);
				trap_Trace(&tr2, ent->client->ps.origin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, CONTENTS_SOLID);

				if (tr.fraction == 1.0 && !tr.startsolid && tr2.fraction == 1.0 && !tr2.startsolid)
				{
					VectorCopy(intendedOrigin, other->client->ps.origin);
				}
				else
				{ //if the guy can't be put here then it's time to break the throw off.
					vec3_t oppDir;
					int strength = 4;

					other->client->ps.heldByClient = 0;
					other->client->beingThrown = 0;
					ent->client->doingThrow = 0;

					ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
					G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

					other->client->ps.forceHandExtend = HANDEXTEND_NONE;
					VectorSubtract(other->client->ps.origin, ent->client->ps.origin, oppDir);
					VectorNormalize(oppDir);
					other->client->ps.velocity[0] = oppDir[0]*(strength*40);
					other->client->ps.velocity[1] = oppDir[1]*(strength*40);
					other->client->ps.velocity[2] = 150;

					VectorSubtract(ent->client->ps.origin, other->client->ps.origin, oppDir);
					VectorNormalize(oppDir);
					ent->client->ps.velocity[0] = oppDir[0]*(strength*40);
					ent->client->ps.velocity[1] = oppDir[1]*(strength*40);
					ent->client->ps.velocity[2] = 150;
				}
			}
		}
	}
#endif
#ifdef VM_MEMALLOC_DEBUG
	else if (Q_stricmp(cmd, "debugTestAlloc") == 0)
	{ //rww - small routine to stress the malloc trap stuff and make sure nothing bad is happening.
		char *blah;
		int i = 1;
		int x;

		//stress it. Yes, this will take a while. If it doesn't explode miserably in the process.
		while (i < 32768)
		{
			x = 0;

			trap_TrueMalloc((void **)&blah, i);
			if (!blah)
			{ //pointer is returned null if allocation failed
				trap_SendServerCommand( -1, va("print \"Failed to alloc at %i!\n\"", i));
				break;
			}
			while (x < i)
			{ //fill the allocated memory up to the edge
				if (x+1 == i)
				{
					blah[x] = 0;
				}
				else
				{
					blah[x] = 'A';
				}
				x++;
			}
			trap_TrueFree((void **)&blah);
			if (blah)
			{ //should be nullified in the engine after being freed
				trap_SendServerCommand( -1, va("print \"Failed to free at %i!\n\"", i));
				break;
			}

			i++;
		}

		trap_SendServerCommand( -1, "print \"Finished allocation test\n\"");
	}
#endif
#if 0//#ifndef FINAL_BUILD
	else if (Q_stricmp(cmd, "debugShipDamage") == 0)
	{
		char	arg[MAX_STRING_CHARS];
		char	arg2[MAX_STRING_CHARS];
		int		shipSurf, damageLevel;

		trap_Argv( 1, arg, sizeof( arg ) );
		trap_Argv( 2, arg2, sizeof( arg2 ) );
		shipSurf = SHIPSURF_FRONT+atoi(arg);
		damageLevel = atoi(arg2);

		G_SetVehDamageFlags( &g_entities[ent->s.m_iVehicleNum], shipSurf, damageLevel );
	}
#endif
	else
	{
		if (Q_stricmp(cmd, "addbot") == 0)
		{ //because addbot isn't a recognized command unless you're the server, but it is in the menus regardless
			trap_SendServerCommand( clientNum, va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "ONLY_ADD_BOTS_AS_SERVER")));
		}
		else
		{
			trap_SendServerCommand( clientNum, va("print \"unknown cmd %s\n\"", cmd ) );
		}
	}
}

void G_InitVoteMapsLimit()
{
    memset(g_votedCounts, 0, sizeof(g_votedCounts));
}

