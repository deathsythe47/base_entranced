// Copyright (C) 1999-2000 Id Software, Inc.
//

// this file holds commands that can be executed by the server console, but not remote clients

#include "g_local.h"
#include "g_database.h"
#include "bg_saga.h"

void Team_ResetFlags( void );
/*
==============================================================================

PACKET FILTERING
 

You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

g_filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/

typedef struct ipFilter_s
{
	unsigned	mask;
	unsigned	compare;
    char        comment[32];
} ipFilter_t;

// VVFIXME - We don't need this at all, but this is the quick way.
#ifdef _XBOX
#define	MAX_IPFILTERS	1
#else
#define	MAX_IPFILTERS	1024
#endif

// getstatus/getinfo bans
static ipFilter_t	getstatusIpFilters[MAX_IPFILTERS];
static int			getstatusNumIPFilters;

/*

ACCOUNT/PASSWORD system

*/

//#define MAX_ACCOUNTS	256
//
//typedef struct {
//	char username[MAX_USERNAME];
//	char password[MAX_PASSWORD];
//	int  inUse;
//	int  toDelete;
//} AccountItem;

//static AccountItem accounts[MAX_ACCOUNTS];
//static int accountsCount;

/*
int validateAccount(const char* username, const char* password, int num){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username)
			&& (!password || !strcmp(accounts[i].password,password)) ){

			if ((accounts[i].inUse > -1) &&
				(((g_entities[ accounts[i].inUse ].client->ps.ping < 999) &&
				g_entities[ accounts[i].inUse ].client->ps.ping != -1)
				&& g_entities[ accounts[i].inUse ].client->pers.connected == CON_CONNECTED
				&& !strcmp(username,g_entities[ accounts[i].inUse ].client->pers.username)
				)){
				//check if that someone isnt 999
				return 2; 
			}

			//if ((accounts[i].inUse > -1))
			//	Com_Printf("data: %i %i %i",
			//		accounts[i].inUse,
			//		g_entities[ num ].client->ps.ping,
			//		g_entities[ num ].client->pers.connected);

			if (accounts[i].toDelete)
				return 1; //this account doesnt exist anymore

			accounts[i].inUse = num;
			return 0;
		}
	}

	return 1;
}
*/

/*
//save updated actual accounts to file
void saveAccounts(){
	fileHandle_t f;
	char entry[64]; // MAX_USERNAME + MAX_PASSWORD + 2 <= 64 !!! 
	int i;

	trap_FS_FOpenFile(g_accountsFile.string, &f, FS_WRITE);

	for(i=0;i<accountsCount;++i){
		if (accounts[i].toDelete)
			continue;

		Com_sprintf(entry,sizeof(entry),"%s:%s ",accounts[i].username,accounts[i].password);
		trap_FS_Write(entry, strlen(entry), f);

		//Com_Printf("Saved account %s %s to file.",accounts[i].username,accounts[i].password);
	}

	trap_FS_FCloseFile(f);
}
*/

/*
void unregisterUser(const char* username){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username)){
			accounts[i].inUse = -1;
			return ;
		}
	}
}
*/

/*
qboolean addAccount(const char* username, const char* password, qboolean updateFile, qboolean checkExists){
	int i;

	if (checkExists){
		for(i=0;i<accountsCount;++i){
			if (!strcmp(accounts[i].username,username) )
				return qfalse; //account with this username already exists
		}
	}

	if (accountsCount >= MAX_ACCOUNTS)
		return qfalse; //too many accounts

	Q_strncpyz(accounts[accountsCount].username,username,MAX_USERNAME);
	Q_strncpyz(accounts[accountsCount].password,password,MAX_PASSWORD);
	accounts[accountsCount].inUse = -1;
	accounts[accountsCount].toDelete = qfalse;
	++accountsCount;

	if (updateFile)
		saveAccounts();

	return qtrue;
}
*/

/*
qboolean changePassword(const char* username, const char* password){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username) ){
			Q_strncpyz(accounts[i].password,password,MAX_PASSWORD);

			G_LogPrintf("Account %s password changed.",username);
			saveAccounts();
		}
			
	}

	return qfalse; //not found

}
*/

/*
void removeAccount(const char* username){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username)){
			accounts[i].toDelete = qtrue;
			saveAccounts();
			G_LogPrintf("Account %s removed.",username);
			return;
		}
	}
}
*/

/*
void loadAccounts(){
	fileHandle_t f;
	int len;
	char buffer[4096];
	char* entryEnd;
	char* rest;
	char* delim;
	int i;

	len = trap_FS_FOpenFile(g_accountsFile.string, &f, FS_READ);

	if (!f || len >= sizeof(buffer))
		return;

	trap_FS_Read(buffer, len, f);
	trap_FS_FCloseFile(f);

	//process
	accountsCount = 0;
	rest = buffer;

	//Com_Printf("Account File Content: %s\n",buffer);

	while(rest - buffer < len){
		entryEnd = strchr(rest,' ');
		if (!entryEnd){
			entryEnd = buffer + len;
		}
		*entryEnd = '\0';

		delim = strchr(rest,':');
		if (!delim){
			//Com_Printf("NOT FOUND delimiter.\n");
			return;
		}
		*delim = '\0';
		++delim;

		addAccount(rest,delim,qfalse,qfalse);

		//Com_Printf("Loaded account %s %s\n",rest,delim);

		rest = entryEnd + 1;
	}

	//go through all clients and mark them as used
	for(i=0;i<MAX_CLIENTS;++i){
		if (!g_entities[i].inuse || !g_entities[i].client)
			continue;

		if (g_entities[i].client->pers.connected != CON_CONNECTED)
			continue;

		validateAccount(g_entities[i].client->pers.username,0,i);
	}

	Com_Printf("Loaded %i accounts.\n",accountsCount);
}
*/


/*
=================
StringToFilter
=================
*/
static qboolean StringToFilter (char *s, char* comment, ipFilter_t *f)
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];
	
	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}
	
	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			G_Printf( "Bad filter address: %s\n", s );
			return qfalse;
		}
		
		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}
	
	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;      
    Q_strncpyz(f->comment,comment ? comment : "none", sizeof(f->comment));

	return qtrue;
}

/*
=================
GetstatusUpdateIPBans
=================
*/
static void GetstatusUpdateIPBans (void)
{
	byte	b[4];
	int		i;
	char	iplist[MAX_INFO_STRING];

	*iplist = 0;
	for (i = 0 ; i < getstatusNumIPFilters ; i++)
	{
		if (getstatusIpFilters[i].compare == 0xffffffff)
			continue;

		*(unsigned *)b = getstatusIpFilters[i].compare;
		Com_sprintf( iplist + strlen(iplist), sizeof(iplist) - strlen(iplist), 
			"%i.%i.%i.%i ", b[0], b[1], b[2], b[3]);
	}

	trap_Cvar_Set( "g_getstatusbanIPs", iplist );
}

/*
=================
G_FilterPacket
=================
*/
qboolean G_FilterPacket(char *from, char* reasonBuffer, int reasonBufferSize)
{
	unsigned int ip = 0;
	getIpFromString(from, &ip);

	if (g_whitelist.integer) {
		return G_DBIsFilteredByWhitelist(ip, reasonBuffer, reasonBufferSize);
	}
	else {
		return G_DBIsFilteredByBlacklist(ip, reasonBuffer, reasonBufferSize);
	}
}

/*
=================
G_FilterPacket
=================
*/
qboolean G_FilterGetstatusPacket (unsigned int ip)
{
	int				i = 0;

	for (i=0 ; i<getstatusNumIPFilters ; i++)
		if ( (ip & getstatusIpFilters[i].mask) == getstatusIpFilters[i].compare)
			return qtrue;

	return qfalse;
}

qboolean getIpFromString( const char* from, unsigned int* ip )
{    
    if ( !(*from) || !(ip) )
    {
        return qfalse;
    }

    qboolean success = qfalse;
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;

    // parse ip address and mask
    if ( sscanf( from, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) == 4  )
    {
        *ip = ((ipA << 24) & 0xFF000000) |
            ((ipB << 16) & 0x00FF0000) |
            ((ipC << 8) & 0x0000FF00) |
            (ipD & 0x000000FF);

        success = qtrue;
    }

    return success;
}

qboolean getIpPortFromString( const char* from, unsigned int* ip, int* port )
{
    if ( !(*from) || !(ip) || !(port) )
    {
        return qfalse;
    }

    qboolean success = qfalse;
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0, ipPort = 0;

    // parse ip address and mask
    if ( sscanf( from, "%d.%d.%d.%d:%d", &ipA, &ipB, &ipC, &ipD, &ipPort ) == 5 )
    {
        *ip = ((ipA << 24) & 0xFF000000) |
            ((ipB << 16) & 0x00FF0000) |
            ((ipC << 8) & 0x0000FF00) |
            (ipD & 0x000000FF);

        *port = ipPort;
        success = qtrue;
    }

    return success;
}

void getStringFromIp( unsigned int ip, char* buffer, int size )
{
    Com_sprintf( buffer, size, "%d.%d.%d.%d",
        (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF );
}

/*
=================
GetstatusAddIP
=================
*/
static void GetstatusAddIP( char *str )
{
	int		i;

	for (i = 0 ; i < getstatusNumIPFilters ; i++)
		if (getstatusIpFilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == getstatusNumIPFilters)
	{
		if (getstatusNumIPFilters == MAX_IPFILTERS)
		{
			G_Printf ("Getstatus IP filter list is full\n");
			return;
		}
		getstatusNumIPFilters++;
	}
	
	if (!StringToFilter (str, 0, &getstatusIpFilters[i]))
		getstatusIpFilters[i].compare = 0xffffffffu;

	GetstatusUpdateIPBans();
}

/*
=================
G_ProcessGetstatusIPBans
=================
*/
void G_ProcessGetstatusIPBans(void) 
{
 	    char *s, *t;
	    char		str[MAX_TOKEN_CHARS];

	Q_strncpyz( str, g_getstatusbanIPs.string, sizeof(str) );

	for (t = s = g_getstatusbanIPs.string; *t; /* */ ) {
		    s = strchr(s, ' ');
		    if (!s)
			    break;
		    while (*s == ' ')
			    *s++ = 0;
		    if (*t)
			GetstatusAddIP( t );
		    t = s;
	    }      
    }



/*
=================
Svcmd_AddIP_f
=================
*/
void Svcmd_AddIP_f (void)
{
    char ip[32];      
    char notes[32];
    char reason[32];
    int hours = 0;
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 ) 
    {
        G_Printf( "Usage:  addip <ip> (mask) (notes) (reason) (hours)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );
        G_Printf( " notes - notes only for admins, defaults to \"\"\n" );
        G_Printf( " reason - reason to be shown to banned player, defaults to \"Unknown\"\n" );
        G_Printf( " hours - duration in hours, defaults to g_defaultBanHoursDuration\n" );

        return;
    }   

    // set defaults
    maskInt = 0xFFFFFFFF;
    Q_strncpyz( notes, "", sizeof( notes ) );
    Q_strncpyz( reason, "Unknown", sizeof( reason ) );
    hours = g_defaultBanHoursDuration.integer;

    // set actuals
    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];
        trap_Argv( 2, mask, sizeof( mask ) );
        getIpFromString( mask, &maskInt );
    }

    if ( trap_Argc() > 3 )
    {
        trap_Argv( 3, notes, sizeof( notes ) );
    }

    if ( trap_Argc() > 4 )
    {
        trap_Argv( 4, reason, sizeof( reason ) );
    }

    if ( trap_Argc() > 5 )
    {
        char hoursStr[16];
        trap_Argv( 5, hoursStr, sizeof( hoursStr ) );
        hours = atoi( hoursStr );        
    }                       

    if ( G_DBAddToBlacklist( ipInt, maskInt, notes, reason, hours ) )
    {
        G_Printf( "Added %s to blacklist successfuly.\n", ip );
        }
    else
    {
        G_Printf( "Failed to add %s to blacklist.\n", ip );
    }         
}

/*
=================
Svcmd_RemoveIP_f
=================
*/
void Svcmd_RemoveIP_f (void)
{
    char ip[32];
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 )
    {
        G_Printf( "Usage:  removeip <ip> (mask)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );

        return;
    }

    // set defaults
    maskInt = 0xFFFFFFFF;

    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];
        trap_Argv( 2, mask, sizeof( mask ) );
        getIpFromString( mask, &maskInt );
    }   

    if ( G_DBRemoveFromBlacklist( ipInt, maskInt ) )
    {
        G_Printf( "Removed %s from blacklist successfuly.\n", ip );
    }
    else
    {
        G_Printf( "Failed to remove %s from blacklist.\n", ip );
    }
}


/*
=================
Svcmd_AddWhiteIP_f
=================
*/
void Svcmd_AddWhiteIP_f( void )
{
    char ip[32];
    char notes[32];
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 )
    {
        G_Printf( "Usage:  addwhiteip <ip> (mask) (notes)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );
        G_Printf( " notes - notes only for admins, defaults to \"\"\n" );

        return;
    }

    // set defaults
    maskInt = 0xFFFFFFFF;
    Q_strncpyz( notes, "", sizeof( notes ) );

    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];
        trap_Argv( 2, mask, sizeof( mask ) );
        getIpFromString( mask, &maskInt );
    }

    if ( trap_Argc() > 3 )
    {
        trap_Argv( 3, notes, sizeof( notes ) );
    }          

    if ( G_DBAddToWhitelist( ipInt, maskInt, notes ) )
    {
        G_Printf( "Added %s to whitelist successfuly.\n", ip );
    }
    else
    {
        G_Printf( "Failed to add %s to whitelist.\n", ip );
    }
}

/*
=================
Svcmd_RemoveWhiteIP_f
=================
*/
void Svcmd_RemoveWhiteIP_f( void )
{
    char ip[32];
    
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 )
    {
        G_Printf( "Usage:  removewhiteip <ip> (mask)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );

        return;
    }

    // set defaults
    maskInt = 0xFFFFFFFF;

    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];    
        trap_Argv( 2, mask, sizeof( mask ) );

        getIpFromString( mask, &maskInt );
    }         

    if ( G_DBRemoveFromWhitelist( ipInt, maskInt ) )
    {
        G_Printf( "Removed %s from whitelist successfuly.\n", ip );
    }
    else
    {
        G_Printf( "Failed to remove %s from whitelist.\n", ip );
    }
}


void (listCallback)( unsigned int ip,
    unsigned int mask,
    const char* notes,
    const char* reason,
    const char* banned_since,
    const char* banned_until )
{
    G_Printf( "%d.%d.%d.%d %d.%d.%d.%d \"%s\" \"%s\" %s %s\n",
        (ip >> 24) & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF,
        (ip) & 0xFF,
        (mask >> 24) & 0xFF,
        (mask >> 16) & 0xFF,
        (mask >> 8) & 0xFF,
        (mask) & 0xFF,
        notes, reason, banned_since, banned_until );
}

/*
=================
Svcmd_Listip_f
=================
*/
void Svcmd_Listip_f (void)
{
    G_Printf( "ip mask notes reason banned_since banned_until\n" );   

    G_DBListBlacklist( listCallback );
}

		

/*
===================
Svcmd_EntityList_f
===================
*/
void	Svcmd_EntityList_f (void) {
	int			e;
	gentity_t		*check;

	check = g_entities+1;
	for (e = 1; e < level.num_entities ; e++, check++) {
		if ( !check->inuse ) {
			continue;
		}
		G_Printf("%3i:", e);
		switch ( check->s.eType ) {
		case ET_GENERAL:
			G_Printf("ET_GENERAL          ");
			break;
		case ET_PLAYER:
			G_Printf("ET_PLAYER           ");
			break;
		case ET_ITEM:
			G_Printf("ET_ITEM             ");
			break;
		case ET_MISSILE:
			G_Printf("ET_MISSILE          ");
			break;
		case ET_MOVER:
			G_Printf("ET_MOVER            ");
			break;
		case ET_BEAM:
			G_Printf("ET_BEAM             ");
			break;
		case ET_PORTAL:
			G_Printf("ET_PORTAL           ");
			break;
		case ET_SPEAKER:
			G_Printf("ET_SPEAKER          ");
			break;
		case ET_PUSH_TRIGGER:
			G_Printf("ET_PUSH_TRIGGER     ");
			break;
		case ET_TELEPORT_TRIGGER:
			G_Printf("ET_TELEPORT_TRIGGER ");
			break;
		case ET_INVISIBLE:
			G_Printf("ET_INVISIBLE        ");
			break;
		case ET_NPC:
			G_Printf("ET_NPC              ");
			break;
		default:
			G_Printf("%3i                 ", check->s.eType);
			break;
		}

		if ( check->classname ) {
			G_Printf("%s", check->classname);
		}
		G_Printf("\n");
	}
}

gclient_t	*ClientForString( const char *s ) {
	gclient_t	*cl;
	int			i;
	int			idnum;

	// numeric values are just slot numbers
	if ( s[0] >= '0' && s[0] <= '9' ) {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			Com_Printf( "Bad client slot: %i\n", idnum );
			return NULL;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			G_Printf( "Client %i is not connected\n", idnum );
			return NULL;
		}
		return cl;
	}

	// check for a name match
	for ( i=0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->pers.netname, s ) ) {
			return cl;
		}
	}

	G_Printf( "User %s is not on the server\n", s );

	return NULL;
}

/*
===================
Svcmd_ForceTeam_f

forceteam <player> <team>
===================
*/
void	Svcmd_ForceTeam_f( void ) {
	char buffer[64];
	gentity_t* found = NULL;
	char		str[MAX_TOKEN_CHARS];

	if (trap_Argc() < 3)
	{
		Com_Printf("usage: forceteam [name or client number] [team] <seconds> (name can be just part of name, colors don't count)\n"); //bad number of arguments
		return;
	}

	trap_Argv(1, buffer, sizeof(buffer));
	found = G_FindClient(buffer);

	if (!found || !found->client)
	{
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n",buffer);
		return;
	}
	trap_Argv( 2, str, sizeof( str ) );

	if ((!Q_stricmp(str, "r") || !Q_stricmp(str, "red")) && found->client->sess.sessionTeam == TEAM_RED)
	{
		goto afterTeamChange;
	}
	else if ((!Q_stricmp(str, "b") || !Q_stricmp(str, "blue")) && found->client->sess.sessionTeam == TEAM_BLUE)
	{
		goto afterTeamChange;
	}

	if (!found->client->sess.canJoin)
	{
		found->client->sess.canJoin = qtrue; // Admins can force passwordless spectators on a team
		SetTeam(found, str, qtrue);
		found->client->sess.canJoin = qfalse;
	}
	else
	{
		SetTeam(found, str, qtrue);
	}

	if (g_gametype.integer == GT_SIEGE)
	{
		if (found->client->sess.siegeDesiredTeam == TEAM_RED)
			trap_SendServerCommand(-1, va("print \"%s was forceteamed to ^1red team.\n\"", found->client->pers.netname));
		else if (found->client->sess.siegeDesiredTeam == TEAM_BLUE)
			trap_SendServerCommand(-1, va("print \"%s was forceteamed to ^4blue team.\n\"", found->client->pers.netname));
		else
			trap_SendServerCommand(-1, va("print \"%s was forceteamed to spectator.\n\"", found->client->pers.netname));
	}
	else
	{
		if (found->client->sess.sessionTeam == TEAM_RED)
			trap_SendServerCommand(-1, va("print \"%s was forceteamed to ^1red team.\n\"", found->client->pers.netname));
		else if (found->client->sess.sessionTeam == TEAM_BLUE)
			trap_SendServerCommand(-1, va("print \"%s was forceteamed to ^4blue team.\n\"", found->client->pers.netname));
		else
			trap_SendServerCommand(-1, va("print \"%s was forceteamed to spectator.\n\"", found->client->pers.netname));
	}

	afterTeamChange:

	if (trap_Argc() < 4)
	{
		return;
	}

	trap_Argv(3, str, sizeof(str));
	if (!str || !str[0] || !atoi(str))
	{
		return;
	}

	//specify the time
	found->forcedTeamTime = level.time + (atoi(str) * 1000);
}

/*
===================
Svcmd_ForceClass_f

forceclass <player> <first letter of class>
===================
*/
extern void SetSiegeClass(gentity_t *ent, char* className);




void	Svcmd_UnForceClass_f()
{
	char buffer[64];
	gentity_t* found = NULL;

	if (g_gametype.integer != GT_SIEGE)
	{
		Com_Printf("Must be in siege gametype.\n");
		return;
	}

	if (trap_Argc() < 2)
	{
		Com_Printf("usage: unforceclass [name or client number]\n"); //bad number of arguments
		return;
	}

	trap_Argv(1, buffer, sizeof(buffer));

	found = G_FindClient(buffer);

	if (!found || !found->client)
	{
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", buffer);
		return;
	}

	found->forcedClass = 0;
	found->forcedClassTime = 0;
	found->funnyClassNumber = 0;

	trap_SendServerCommand(-1, va("print \"%s was ^1un^7forceclassed.\n\"", found->client->pers.netname));
}

void	Svcmd_ForceClass_f(int specifiedClientNum, char *specifiedClassLetter) {
	char buffer[64];
	gentity_t* found = NULL;
	char		enteredClass[MAX_TOKEN_CHARS];
	char		desiredClassLetter[16];
	char		desiredClassName[16];
	stupidSiegeClassNum_t destinedClassNumber;
	int			funnyClassNumber;
	siegeClass_t* siegeClass = 0;

	if (g_gametype.integer != GT_SIEGE)
	{
		Com_Printf("Must be in siege gametype.\n");
		return;
	}

	if ((!specifiedClassLetter || !specifiedClassLetter[0]) && trap_Argc() < 3)
	{
		Com_Printf("usage: forceclass [name or client number] [first letter of class name]\n"); //bad number of arguments
		return;
	}

	if (specifiedClassLetter && specifiedClassLetter[0])
	{
		if (&g_entities[specifiedClientNum] && g_entities[specifiedClientNum].client && level.clients[specifiedClientNum].pers.connected != CON_DISCONNECTED)
		{
			//he's still here
			found = &g_entities[specifiedClientNum];
			Com_sprintf(enteredClass, sizeof(enteredClass), "%s", specifiedClassLetter);
		}
		else
		{
			//maybe he disconnected or something
			return;
		}
	}
	else
	{
		trap_Argv(1, buffer, sizeof(buffer));
		found = G_FindClient(buffer);

		if (!found || !found->client)
		{
			Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", buffer);
			return;
		}
		if (found->client->sess.sessionTeam != TEAM_RED && found->client->sess.sessionTeam != TEAM_BLUE)
		{
			Com_Printf("Client %s"S_COLOR_WHITE" is not in-game.\n", buffer);
			return;
		}
		trap_Argv(2, enteredClass, sizeof(enteredClass));
	}
	if (!Q_stricmp(enteredClass, "a") || !Q_stricmp(enteredClass, "h") || !Q_stricmp(enteredClass, "t") || !Q_stricmp(enteredClass, "d") || !Q_stricmp(enteredClass, "j") || !Q_stricmp(enteredClass, "s"))
	{
		strcpy(desiredClassLetter, enteredClass);
		if (!Q_stricmp(desiredClassLetter, "a"))
		{
			Com_sprintf(desiredClassName, sizeof(desiredClassName), "assault");
			destinedClassNumber = SSCN_ASSAULT;
			funnyClassNumber = 0;
		}
		else if (!Q_stricmp(desiredClassLetter, "h"))
		{
			Com_sprintf(desiredClassName, sizeof(desiredClassName), "HW");
			destinedClassNumber = SSCN_HW;
			funnyClassNumber = 5;
		}
		else if (!Q_stricmp(desiredClassLetter, "t"))
		{
			Com_sprintf(desiredClassName, sizeof(desiredClassName), "tech");
			destinedClassNumber = SSCN_TECH;
			funnyClassNumber = 2;
		}
		else if (!Q_stricmp(desiredClassLetter, "d"))
		{
			Com_sprintf(desiredClassName, sizeof(desiredClassName), "demo");
			destinedClassNumber = SSCN_DEMO;
			funnyClassNumber = 4;
		}
		else if (!Q_stricmp(desiredClassLetter, "j"))
		{
			Com_sprintf(desiredClassName, sizeof(desiredClassName), "jedi");
			destinedClassNumber = SSCN_JEDI;
			funnyClassNumber = 3;
		}
		else if (!Q_stricmp(desiredClassLetter, "s"))
		{
			Com_sprintf(desiredClassName, sizeof(desiredClassName), "scout");
			destinedClassNumber = SSCN_SCOUT;
			funnyClassNumber = 1;
		}
	}
	else
	{
		Com_Printf("usage: forceteam [name or client number] [first letter of class name]\n");
		return;
	}

	if (level.inSiegeCountdown && found->client->sess.sessionTeam == TEAM_SPECTATOR && found->client->sess.siegeDesiredTeam && (found->client->sess.siegeDesiredTeam == TEAM_RED || found->client->sess.siegeDesiredTeam == TEAM_BLUE))
	{
		siegeClass = BG_SiegeGetClass(found->client->sess.siegeDesiredTeam, destinedClassNumber);
	}
	else
	{
		siegeClass = BG_SiegeGetClass(found->client->sess.sessionTeam, destinedClassNumber);
	}

	if (!siegeClass)
	{
		Com_Printf("usage: forceclass [name or client number] [first letter of class name]\n");
		return;
	}

	if (level.inSiegeCountdown || found->client->sess.sessionTeam == TEAM_RED || found->client->sess.sessionTeam == TEAM_BLUE)
	{
		if ((found->client->sess.sessionTeam == TEAM_RED || found->client->sess.sessionTeam == TEAM_BLUE) && bgSiegeClasses[found->client->siegeClass].playerClass == funnyClassNumber)
		{
		}
		else
		{
			SetSiegeClass(found, siegeClass->name);
		}
	}
	found->forcedClass = destinedClassNumber;
	found->forcedClassTime = level.time + 60000;
	found->funnyClassNumber = funnyClassNumber;
	

	trap_SendServerCommand(-1, va("print \"%s was forceclassed to ^5%s^7.\n\"", found->client->pers.netname, desiredClassName));
}

void Svcmd_ResetFlags_f(){
	gentity_t*	ent;
	int i;

	for (i = 0; i < 32; ++i ){
		ent = g_entities+i;
		
		if (!ent->inuse || !ent->client )
			continue;

		ent->client->ps.powerups[PW_BLUEFLAG] = 0;
		ent->client->ps.powerups[PW_REDFLAG] = 0;
	}
	Team_ResetFlags();
}

void G_ChangePlayerReadiness(gclient_t *cl, qboolean ready, qboolean announce) {
	if (!cl || cl - level.clients < 0 || cl - level.clients >= MAX_CLIENTS)
		return;

	cl->pers.ready = ready;
	cl->pers.readyTime = level.time;
	if (announce) {
		if (cl->pers.ready) {
			trap_SendServerCommand(-1, va("print \"%s%s "S_COLOR_GREEN"is ready\n\"", NM_SerializeUIntToColor(cl - level.clients), cl->pers.netname));
			trap_SendServerCommand(cl - level.clients, va("cp \""S_COLOR_GREEN"You are ready\""));
		}
		else {
			trap_SendServerCommand(-1, va("print \"%s%s "S_COLOR_RED"is NOT ready\n\"", NM_SerializeUIntToColor(cl - level.clients), cl->pers.netname));
			trap_SendServerCommand(cl - level.clients, va("cp \""S_COLOR_RED"You are NOT ready\""));
		}
	}
}

/*
===================
Svcmd_ForceReady_f

forceready <player>
===================
*/
void	Svcmd_ForceReady_f(void) {
	gentity_t *found = NULL;
	char		str[MAX_TOKEN_CHARS];

	if (trap_Argc() != 2) {
		Com_Printf("Usage: forceready <clientnumber> (use -1 for all clients, -2 for all ingame players)\n"); //bad number of arguments
		return;
	}

	trap_Argv(1, str, sizeof(str));
	int argInt = atoi(str), affectedPlayers = 0;
	if (argInt < 0) { // all players or all ingame players
		int i;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (level.clients[i].pers.connected == CON_DISCONNECTED)
				continue;
			if (GetRealTeam(&level.clients[i]) == TEAM_SPECTATOR && argInt == -2)
				continue;
			G_ChangePlayerReadiness(&level.clients[i], qtrue, qfalse);
			affectedPlayers++;
		}
		if (affectedPlayers > 0)
			trap_SendServerCommand(-1, va("print \"All %d %splayers "S_COLOR_GREEN"are ready"S_COLOR_WHITE"\n\"", affectedPlayers, argInt == -2 ? "ingame " : ""));
		else
			Com_Printf("No valid %splayers found.\n", argInt == -2 ? "ingame " : "");
	}
	else { // one specific player
		found = G_FindClient(str);
		if (!found || !found->client) {
			Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
			return;
		}

		G_ChangePlayerReadiness(found->client, qtrue, qtrue);
	}
}

/*
===================
Svcmd_ForceUnReady_f

forceunready <player>
===================
*/
void	Svcmd_ForceUnReady_f(void) {
	gentity_t *found = NULL;
	char		str[MAX_TOKEN_CHARS];

	if (trap_Argc() != 2) {
		Com_Printf("Usage: forceunready <clientnumber> (use -1 for all clients, -2 for all ingame players)\n"); //bad number of arguments
		return;
	}

	trap_Argv(1, str, sizeof(str));
	int argInt = atoi(str), affectedPlayers = 0;
	if (argInt < 0) { // all players or all ingame players
		int i;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (level.clients[i].pers.connected == CON_DISCONNECTED)
				continue;
			if (GetRealTeam(&level.clients[i]) == TEAM_SPECTATOR && argInt == -2)
				continue;
			G_ChangePlayerReadiness(&level.clients[i], qfalse, qfalse);
			affectedPlayers++;
		}
		if (affectedPlayers > 0)
			trap_SendServerCommand(-1, va("print \"All %d %splayers "S_COLOR_RED"are NOT ready"S_COLOR_WHITE"\n\"", affectedPlayers, argInt == -2 ? "ingame " : ""));
		else
			Com_Printf("No valid %splayers found.\n", argInt == -2 ? "ingame " : "");
	}
	else { // one specific player
		found = G_FindClient(str);
		if (!found || !found->client) {
			Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
			return;
		}

		G_ChangePlayerReadiness(found->client, qfalse, qtrue);
	}
}

// whitelist
// g_lockdown helper command
// status == displays list of clients with their client number, name, unique, id and whitelist status
// add/del == whitelists/unwhitelists a player from being affected by g_lockdown (adds/removes his unique id in whitelist.txt and level.whitelistedUniqueIds[])
#define WHITELIST_ERROR	"Usage: whitelist < status | add | del > [partial name or client number]\nToggle server lockdown with g_lockdown.\n"
void Svcmd_Whitelist_f(void) {
	if (trap_Argc() < 2) {
		Com_Printf(WHITELIST_ERROR);
		return;
	}

	qboolean add;
	char commandArg[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, commandArg, sizeof(commandArg));
	if (!Q_stricmp(commandArg, "status")) { // print the status of each player
		// lockdown status
		Com_Printf("Server lockdown is currently ^5%s^7 (toggle with g_lockdown).\n", g_lockdown.integer ? "enabled" : "disabled");

		// print the column names
		int i;
		unsigned int longestName = 4;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (!&g_entities[i] || !g_entities[i].client || g_entities[i].client->pers.connected == CON_DISCONNECTED || g_entities[i].r.svFlags & SVF_BOT)
				continue;
			unsigned int len = strlen(g_entities[i].client->pers.netnameClean);
			if (len > longestName) // fixme: color codes/drawstrlen
				longestName = len;
		}
		char nameSpacingStr[64] = { 0 };
		while (strlen(nameSpacingStr) < longestName - 4)
			nameSpacingStr[strlen(nameSpacingStr)] = ' ';
		Com_Printf("#  Name%s  Whitelisted?  Unique ID             Newmod CUID\n", nameSpacingStr);

		// print each player's line
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (!&g_entities[i] || !g_entities[i].client || g_entities[i].client->pers.connected == CON_DISCONNECTED || g_entities[i].r.svFlags & SVF_BOT)
				continue;

			// get name
			char nameString[64] = { 0 };
			Q_strncpyz(nameString, g_entities[i].client->pers.netnameClean, sizeof(nameString));
			while (strlen(nameString) < longestName + 2)
				nameString[strlen(nameString)] = ' ';

			// get unique id
			char uniqueString[MAX_STRING_CHARS] = { 0 };
			Q_strncpyz(uniqueString, level.clientUniqueIds[i] ? va("%llu", level.clientUniqueIds[i]) : "Unknown", sizeof(uniqueString));
			while (strlen(uniqueString) < 20 + 2)
				uniqueString[strlen(uniqueString)] = ' ';

			// get cuid, if available
			char cuidString[MAX_STRING_CHARS] = { 0 };
			Q_strncpyz(cuidString, g_entities[i].client->sess.auth == AUTHENTICATED ? va("%s", g_entities[i].client->sess.cuidHash) : "", sizeof(cuidString));

			// print this player's line
			Com_Printf("%i%s^7%s%s%s%s"S_COLOR_WHITE"\n",
				i, // client number
				i >= 10 ? " " : "  ", // spacing
				nameString, // name
				G_ClientIsWhitelisted(i) ? S_COLOR_GREEN"Yes           "S_COLOR_WHITE : S_COLOR_RED"No            "S_COLOR_WHITE, // whitelisted status
				uniqueString, // unique id
				cuidString // cuid, if available
				);
		}
		return;
	}
	else if (!Q_stricmp(commandArg, "add") && trap_Argc() >= 3)
		add = qtrue;
	else if (!Q_stricmp(commandArg, "del") && trap_Argc() >= 3)
		add = qfalse;
	else {
		Com_Printf(WHITELIST_ERROR);
		return;
	}

	// add or remove a player
	// find the player
	gentity_t	*found = NULL;
	int			clientNum;
	char playerArg[MAX_STRING_CHARS] = { 0 };
	trap_Argv(2, playerArg, sizeof(playerArg));
	found = G_FindClient(playerArg);
	if (!found || !found->client) {
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", playerArg);
		Com_Printf(WHITELIST_ERROR);
		return;
	}
	clientNum = found - g_entities;
	if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
		Com_Printf("Invalid client.\n");
		Com_Printf(WHITELIST_ERROR);
		return;
	}

	char *name = found->client->pers.netname;
	unsigned long long id = level.clientUniqueIds[clientNum];
	qboolean newmod = found->client->sess.auth == AUTHENTICATED ? qtrue : qfalse;
	char *cuid = newmod ? found->client->sess.cuidHash : "";
	int currentStatus = G_DBPlayerLockdownWhitelisted(id, cuid);
	qboolean whiteId = currentStatus & LOCKDOWNWHITELISTED_ID ? qtrue : qfalse;
	qboolean whiteCuid = currentStatus & LOCKDOWNWHITELISTED_CUID ? qtrue : qfalse;

	if (add) {
		found->client->sess.whitelistStatus = WHITELIST_WHITELISTED;
		// add the player to the whitelist
		if (newmod && whiteId && whiteCuid || !newmod && whiteId) {
			Com_Printf("%s"S_COLOR_WHITE" is already whitelisted.\n", name);
		}
		else if (newmod && whiteId && !whiteCuid) {
			G_DBStorePlayerInLockdownWhitelist(0, cuid, name);
			Com_Printf("%s"S_COLOR_WHITE" was previously whitelisted by unique ID, but not CUID. Added their CUID to the whitelist.\n", name);
		}
		else if (newmod && !whiteId && whiteCuid) {
			G_DBStorePlayerInLockdownWhitelist(id, "", name);
			Com_Printf("%s"S_COLOR_WHITE" was previously whitelisted by CUID, but not unique ID. Added their unique ID to the whitelist.\n", name);
		}
		else {
			G_DBStorePlayerInLockdownWhitelist(id, cuid, name);
			Com_Printf("%s"S_COLOR_WHITE" was added to the whitelist.\n", name);
		}
	}
	else {
		found->client->sess.whitelistStatus = WHITELIST_NOTWHITELISTED;
		// remove the player from the whitelist
		if (currentStatus) {
			G_DBRemovePlayerFromLockdownWhitelist(id, cuid);
			Com_Printf("%s"S_COLOR_WHITE" was removed from the whitelist.\n", name);
		}
		else {
			Com_Printf("%s"S_COLOR_WHITE" is not currently whitelisted, so they couldn't be removed.\n", name);
		}
	}
}

void Svcmd_LockTeams_f( void ) {
	char	str[MAX_TOKEN_CHARS];
	int		n = -1;

	if ( trap_Argc() < 2 ) {
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	// could do some advanced parsing, but since we only have to handle a few cases...
	if ( !Q_stricmp( str, "0" ) || !Q_stricmpn( str, "r", 1 ) ) {
		n = 0;
	} else if (!Q_stricmp(str, "1s")) {
		n = 2;
	} else if (!Q_stricmp(str, "2s")) {
		n = 4;
	} else if (!Q_stricmp(str, "3s")) {
		n = 6;
	} else if ( !Q_stricmp( str, "4s" ) ) {
		n = 8;
	} else if ( !Q_stricmp( str, "5s" ) ) {
		n = 10;
	} else if ( !Q_stricmp( str, "6s" ) ) {
		n = 12;
	} else if ( !Q_stricmp( str, "7s" ) ) {
		n = 14;
	}

	if ( n < 0 ) {
		return;
	}

	if ( !n ) {
		trap_SendServerCommand( -1, "print \""S_COLOR_GREEN"Teams were unlocked.\n\"");
	} else {
		trap_SendServerCommand( -1, va( "print \""S_COLOR_RED"Teams were locked to %i vs %i.\n\"", n / 2, n / 2 ) );
	}

	trap_Cvar_Set("g_maxgameclients", va("%i", n));
}

void Svcmd_Cointoss_f(void) 
{
	int cointoss = rand() % 2;

	trap_SendServerCommand(-1, va("cp \""S_COLOR_YELLOW"%s"S_COLOR_WHITE"!\"", cointoss ? "Heads" : "Tails"));
	trap_SendServerCommand(-1, va("print \"Coin Toss result: "S_COLOR_YELLOW"%s\n\"", cointoss ? "Heads" : "Tails"));
}

void Svcmd_Skillboost_f(void) {
	gentity_t	*found = NULL;
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	if (trap_Argc() <= 2) {
		Com_Printf("Usage:   skillboost [client num or partial name] [level from 1 to 5]     (use level 0 to reset).\n");
		int i;
		qboolean wrotePreface = qfalse;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (&g_entities[i] && g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.skillBoost) {
				if (!wrotePreface) {
					Com_Printf("Currently skillboosted players:\n");
					wrotePreface = qtrue;
				}
				Com_Printf("^7%s^7 has a level ^5%d^7 skillboost.\n",
					g_entities[i].client->pers.netname, g_entities[i].client->sess.skillBoost);
			}
		}
		if (!wrotePreface)
			Com_Printf("No players are currently skillboosted.\n");
		return;
	}

	// find the player
	trap_Argv(1, str, sizeof(str));
	found = G_FindClient(str);
	if (!found || !found->client) {
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
		Com_Printf("Usage:   skillboost [client num or partial name] [level from 1 to 5]     (use level 0 to reset).\n");
		return;
	}

	cl = found->client;

	trap_Argv(2, str, sizeof(str));
	int newValue = Com_Clampi(0, MAX_SKILLBOOST, atoi(str));

	// notify everyone
	if (!newValue) {
		if (!found->client->sess.skillBoost)
			Com_Printf("Client '%s'^7 already has a skillboost of zero.\n", found->client->pers.netname);
		else
			trap_SendServerCommand(-1, va("print \"^7%s^7's skillboost was reset to zero.\n\"", found->client->pers.netname));
	}
	else
		trap_SendServerCommand(-1, va("print \"^7%s^7 was given a level ^5%d^7 skillboost.\n\"",
			found->client->pers.netname, newValue));

	found->client->sess.skillBoost = newValue;
}

void Svcmd_Senseboost_f(void) {
	gentity_t	*found = NULL;
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	if (trap_Argc() <= 2) {
		Com_Printf("Usage:   senseboost [client num or partial name] [level from 1 to 5]     (use level 0 to reset).\n");
		int i;
		qboolean wrotePreface = qfalse;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (&g_entities[i] && g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.senseBoost) {
				if (!wrotePreface) {
					Com_Printf("Currently senseboosted players:\n");
					wrotePreface = qtrue;
				}
				Com_Printf("^7%s^7 has a level ^5%d^7 senseboost.\n",
					g_entities[i].client->pers.netname, g_entities[i].client->sess.senseBoost);
			}
		}
		if (!wrotePreface)
			Com_Printf("No players are currently senseboosted.\n");
		return;
	}

	// find the player
	trap_Argv(1, str, sizeof(str));
	found = G_FindClient(str);
	if (!found || !found->client) {
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
		Com_Printf("Usage:   senseboost [client num or partial name] [level from 1 to 5]     (use level 0 to reset).\n");
		return;
	}

	cl = found->client;

	trap_Argv(2, str, sizeof(str));
	int newValue = Com_Clampi(0, 5, atoi(str));

	// notify everyone
	if (!newValue) {
		if (!found->client->sess.senseBoost)
			Com_Printf("Client '%s'^7 already has a senseboost of zero.\n", found->client->pers.netname);
		else
			trap_SendServerCommand(-1, va("print \"^7%s^7's senseboost was reset to zero.\n\"", found->client->pers.netname));
	}
	else
		trap_SendServerCommand(-1, va("print \"^7%s^7 was given a level ^5%d^7 senseboost.\n\"",
			found->client->pers.netname, newValue));

	found->client->sess.senseBoost = newValue;
}

void Svcmd_Aimboost_f(void) {
	gentity_t *found = NULL;
	gclient_t *cl;
	char		str[MAX_TOKEN_CHARS];

	if (trap_Argc() <= 2) {
		Com_Printf("Usage:   aimboost [client num or partial name] [level from 1 to 5]     (use level 0 to reset).\n");
		int i;
		qboolean wrotePreface = qfalse;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (&g_entities[i] && g_entities[i].client && g_entities[i].client->pers.connected != CON_DISCONNECTED && g_entities[i].client->sess.aimBoost) {
				if (!wrotePreface) {
					Com_Printf("Currently aimboosted players:\n");
					wrotePreface = qtrue;
				}
				Com_Printf("^7%s^7 has a level ^5%d^7 aimboost.\n",
					g_entities[i].client->pers.netname, g_entities[i].client->sess.aimBoost);
			}
		}
		if (!wrotePreface)
			Com_Printf("No players are currently aimboosted.\n");
		return;
	}

	// find the player
	trap_Argv(1, str, sizeof(str));
	found = G_FindClient(str);
	if (!found || !found->client) {
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
		Com_Printf("Usage:   aimboost [client num or partial name] [level from 1 to 5]     (use level 0 to reset).\n");
		return;
	}

	cl = found->client;

	trap_Argv(2, str, sizeof(str));
	int newValue = Com_Clampi(0, 5, atoi(str));

	// notify everyone
	if (!newValue) {
		if (!found->client->sess.aimBoost)
			Com_Printf("Client '%s'^7 already has an aimboost of zero.\n", found->client->pers.netname);
		else
			trap_SendServerCommand(-1, va("print \"^7%s^7's aimboost was disabled.\n\"", found->client->pers.netname));
	}
	else
		trap_SendServerCommand(-1, va("print \"^7%s^7 was given a level ^5%d^7 aimboost.\n\"",
			found->client->pers.netname, newValue));

	found->client->sess.aimBoost = newValue;
}

void Svcmd_ShadowMute_f(void) {
	gentity_t	*found = NULL;
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	// find the player
	trap_Argv(1, str, sizeof(str));
	found = G_FindClient(str);
	if (!found || !found->client) {
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
		return;
	}
	cl = found->client;

	cl->sess.shadowMuted = !cl->sess.shadowMuted;

	if (cl->sess.shadowMuted) {
		G_Printf("Client %d (%s"S_COLOR_WHITE") is now shadowmuted.\n", cl - level.clients, cl->pers.netname);
	}
	else {
		G_Printf("Client %d (%s"S_COLOR_WHITE") is no longer shadowmuted.\n", cl - level.clients, cl->pers.netname);
	}
}

void Svcmd_ForceName_f(void) {
	gentity_t	*found = NULL;
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];
	char		durationStr[4];
	int			duration = 700;

	if (trap_Argc() <= 2)
	{
		Com_Printf("Usage:  rename [name or client number] [forced name] <optional duration in seconds>\nExample:  rename pad lamer 60\n");
		return;
	}

	// find the player
	trap_Argv(1, str, sizeof(str));
	found = G_FindClient(str);
	if (!found || !found->client)
	{
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
		Com_Printf("Usage:  rename [name or client number] [forced name] <optional duration in seconds>\nExample:  rename pad lamer 60\n");
		return;
	}

	cl = found->client;

	trap_Argv( 2, str, sizeof( str ) );

	G_LogPrintf("Client number %i from %s %s renamed to %s by admin.\n", found->s.number, found->client->sess.ipString, found->client->pers.netname, str);
	trap_SendServerCommand(-1, va("print \"%s^7 was renamed by admin to %s\n\"", found->client->pers.netname, str));
	if ( trap_Argc() > 2 ) {
		trap_Argv( 3, durationStr, sizeof( durationStr ) );
		duration = atoi( durationStr ) * 1000;
	}

	SetNameQuick( &g_entities[cl - level.clients], str, duration );
}

#ifdef _WIN32
#define FS_RESTART_ADDR 0x416800
#else
#define FS_RESTART_ADDR 0x81313B4
#endif

#include "jp_engine.h"

//#ifdef _WIN32
	void (*FS_Restart)( int checksumFeed ) = (void (*)( int  ))FS_RESTART_ADDR;
//#else
//#endif

extern void G_LoadArenas( void );
void Svcmd_FSReload_f(){

#ifdef _WIN32
	int feed = rand();
	FS_Restart(feed);
#else
	//LINUX NOT YET SUPPORTED
	int feed = rand();
	FS_Restart(feed);

#endif

	G_LoadArenas();
}


#ifndef _WIN32
#include <sys/utsname.h>
#endif

void Svcmd_Osinfo_f(){

#ifdef _WIN32
	Com_Printf("System: Windows\n");
#else
	struct utsname buf;
	uname(&buf);
	Com_Printf("System: Linux\n");
	Com_Printf("System name: %s\n",buf.sysname);
	Com_Printf("Node: %s\n",buf.nodename);
	Com_Printf("Release: %s\n",buf.release);
	Com_Printf("Version: %s\n",buf.version);
	Com_Printf("Machine: %s\n",buf.machine);
#endif




}

void Svcmd_ClientBan_f(){
	char clientId[4];
    char ip[16];
	int id;

	trap_Argv(1,clientId,sizeof(clientId));
	id = atoi(clientId);

	if (id < 0 || id >= MAX_CLIENTS)
		return;

	if (g_entities[id].client->pers.connected == CON_DISCONNECTED)
		return;

	if (g_entities[id].r.svFlags & SVF_BOT)
		return;


    getStringFromIp( g_entities[id].client->sess.ip, ip, sizeof( ip ) );

    trap_SendConsoleCommand( EXEC_APPEND, va( "addip %s\n", ip ) );
	trap_SendConsoleCommand(EXEC_APPEND, va("clientkick %i\n",id));

}

/*
void Svcmd_AddAccount_f(){
	char username[MAX_USERNAME];
	char password[MAX_PASSWORD];

	if (trap_Argc() < 3){
		Com_Printf("Too few arguments. Usage: accountadd [username] [password]\n");
		return;
	}

	trap_Argv(1,username,MAX_USERNAME);
	trap_Argv(2,password,MAX_PASSWORD);

	if (addAccount(username,password,qtrue,qtrue))
		G_LogPrintf("Account %s added.",username);
}
*/

/*
void Svcmd_RemoveAccount_f(){
	char username[MAX_USERNAME];

	if (trap_Argc() < 2){
		Com_Printf("Too few arguments. Usage: accountremove [username]\n");
		return;
	}

	trap_Argv(1,username,MAX_USERNAME);

	removeAccount(username);
}
*/

/*
void Svcmd_ChangeAccount_f(){
	char username[MAX_USERNAME];
	char password[MAX_PASSWORD];

	if (trap_Argc() < 3){
		Com_Printf("Too few arguments. Usage: accountchange [username] [newpassword]\n");
		return;
	}

	trap_Argv(1,username,MAX_USERNAME);
	trap_Argv(2,password,MAX_PASSWORD);

	changePassword(username,password);
}
*/

/*
void Svcmd_ReloadAccount_f(){
	loadAccounts();
}
*/

#define Q_IsColorStringStats(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE && *((p)+1) <= '7' && *((p)+1) >= '0' )
int CG_DrawStrlenStats( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorStringStats( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

void Svcmd_AccountPrint_f(){
	int i;

	Com_Printf("id nick                             username                                 ip\n");

	for(i=0;i<level.maxclients;++i){
		if (!g_entities[i].inuse || !g_entities[i].client)
			continue;

		Com_Printf("%2i %-*s^7 %-*s %+*s\n",
			i,
			32 + strlen(g_entities[i].client->pers.netname) - CG_DrawStrlenStats(g_entities[i].client->pers.netname),g_entities[i].client->pers.netname,
			16 + strlen(g_entities[i].client->sess.username) - CG_DrawStrlenStats(g_entities[i].client->sess.username),g_entities[i].client->sess.username,
			26 + strlen(g_entities[i].client->sess.ipString) - CG_DrawStrlenStats(g_entities[i].client->sess.ipString),g_entities[i].client->sess.ipString
			);

	}

}

/*
int accCompare(const void* index1, const void* index2){
	return stricmp(accounts[*(int *)index1].username,accounts[*(int *)index2].username);
}
*/

/*
void Svcmd_AccountPrintAll_f(){
	int i;
	int sorted[MAX_ACCOUNTS];

	for(i=0;i<accountsCount;++i){
		sorted[i] = i;
	}
	qsort(sorted, accountsCount, sizeof(int),accCompare);


	for(i=0;i<accountsCount;++i){
		Com_Printf("%s\n",accounts[sorted[i]].username);
	}
	Com_Printf("%i accounts listed.\n",accountsCount);
}
*/

qboolean LongMapNameFromChar(char c, char *outFileName, size_t outFileNameSize, char *outPrettyName, size_t outPrettyNameSize) {
	char lowered = tolower(c);
	if (lowered < 'a' || lowered > 'z')
		return qfalse;

	// get the filename
	char fileName[MAX_STRING_CHARS] = { 0 };
	trap_Cvar_VariableStringBuffer(va("vote_map_%c", lowered), fileName, sizeof(fileName));
	if (!fileName[0])
		return qfalse;

	if (outFileName && outFileNameSize > 0)
		Q_strncpyz(outFileName, fileName, outFileNameSize);

	if (!outPrettyName || outPrettyNameSize <= 0)
		return qtrue;

	// now get the brief, readable name
	char prettyName[MAX_STRING_CHARS] = { 0 };
	trap_Cvar_VariableStringBuffer(va("vote_map_shortname_%c", lowered), prettyName, sizeof(prettyName));
	if (prettyName[0]) {
		Q_CleanStr(prettyName);
		Q_strncpyz(outPrettyName, prettyName, outPrettyNameSize);
	}
	else { // fallback; see if we can get it from the map's arena info
		extern const char *G_GetArenaInfoByMap(const char *map);
		const char *arenaInfo = G_GetArenaInfoByMap(fileName);
		if (VALIDSTRING(arenaInfo)) {
			char *nameFromArenaInfo = Info_ValueForKey(arenaInfo, "longname");
			if (VALIDSTRING(nameFromArenaInfo)) {
				Q_strncpyz(prettyName, nameFromArenaInfo, sizeof(prettyName));
				Q_CleanStr(prettyName);
				Q_strncpyz(outPrettyName, prettyName, outPrettyNameSize);
				return qtrue;
			}
		}
		// if we got here, we couldn't get it from the arena info either; just use the filename
		Q_strncpyz(outPrettyName, fileName, outPrettyNameSize);
	}
	return qtrue;
}

static char CharFromMapName(char *s) {
	if (!s || !strlen(s))
		return '\0';
	for (char c = 'a'; c <= 'z'; c++) {
		char buf[MAX_STRING_CHARS] = { 0 };
		trap_Cvar_VariableStringBuffer(va("vote_map_%c", c), buf, sizeof(buf));
		if (!Q_stricmpn(s, buf, strlen(s)))
			return c;
	}
	return '\0';
}

extern void fixVoters();
static char reinstateVotes[MAX_CLIENTS] = { 0 }, removedVotes[MAX_CLIENTS] = { 0 };
// starts a multiple choices vote using some of the binary voting logic
static void StartMultiMapVote( const int numMaps, const char *listOfMaps ) {
	if ( level.voteTime ) {
		// stop the current vote because we are going to replace it
		level.voteTime = 0;
		g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	}

	G_LogPrintf( "A multi map vote was started: %s\n", listOfMaps );

	// start a "fake vote" so that we can use most of the logic that already exists
	Com_sprintf( level.voteString, sizeof( level.voteString ), "map_multi_vote %s", listOfMaps );
	Q_strncpyz( level.voteDisplayString, S_COLOR_RED"Vote for a map in console", sizeof( level.voteDisplayString ) );
	level.voteTime = level.time;
#ifdef _DEBUG
	level.voteTime += 15000; // for testing
#else
	if (g_runoffVote.integer)
		level.voteTime -= 15000; // shorter vote time for runoff rounds
#endif
	level.voteYes = 0;
	level.voteNo = 0;
	// we don't set lastVotingClient since this isn't a "normal" vote
	level.multiVoting = qtrue;
	level.inRunoff = qfalse;
	level.multiVoteChoices = numMaps;
	memset( &( level.multiVotes ), 0, sizeof( level.multiVotes ) );

	// now reinstate votes, if needed
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!reinstateVotes[i] || level.clients[i].pers.connected != CON_CONNECTED ||
			(g_gametype.integer != GT_DUEL && g_gametype.integer != GT_POWERDUEL && level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) ||
			g_entities[i].r.svFlags & SVF_BOT || G_ClientIsOnProbation(i) || (g_lockdown.integer && G_ClientIsWhitelisted(i)))
			continue;
		int index = strchr(level.multiVoteMapChars, reinstateVotes[i]) - level.multiVoteMapChars;
		if (index >= 0 && index < level.multiVoteChoices) {
			level.multiVotes[i] = (strchr(level.multiVoteMapChars, reinstateVotes[i]) - level.multiVoteMapChars) + 1;
			G_LogPrintf("Client %d (%s) auto-voted for choice %c\n", i, level.clients[i].pers.netname, reinstateVotes[i]);
			level.voteYes++;
			trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			level.clients[i].mGameFlags |= PSG_VOTED;
		}
	}
	memset(&reinstateVotes, 0, sizeof(reinstateVotes));

	fixVoters();

	trap_SetConfigstring( CS_VOTE_TIME, va( "%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
	trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
}

typedef struct {
	char listOfMaps[MAX_STRING_CHARS];
	char printMessage[MAX_CLIENTS][MAX_STRING_CHARS]; // everyone gets a unique message
	qboolean announceMultiVote;
	int numSelected;
} MapSelectionContext;
static unsigned long long runoffSurvivors = 0llu, runoffLosers = 0llu;

extern const char *G_GetArenaInfoByMap( const char *map );

static void mapSelectedCallback( void *context, char *mapname ) {
	MapSelectionContext *selection = ( MapSelectionContext* )context;

	// build a whitespace separated list ready to be passed as arguments in voteString if needed
	if ( VALIDSTRING( selection->listOfMaps ) ) {
		Q_strcat( selection->listOfMaps, sizeof( selection->listOfMaps ), " " );
		Q_strcat( selection->listOfMaps, sizeof( selection->listOfMaps ), mapname );
	} else {
		Q_strncpyz( selection->listOfMaps, mapname, sizeof( selection->listOfMaps ) );
	}

	selection->numSelected++;

	if ( selection->announceMultiVote ) {
		if ( selection->numSelected == 1 ) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (runoffSurvivors & (1llu << (unsigned long long)i)) {
					Q_strncpyz(selection->printMessage[i], va("Runoff vote: waiting for other players to vote...\nMaps still in contention:"), sizeof(selection->printMessage[i]));
				}
				else if (runoffLosers & (1llu << (unsigned long long)i)) {
					char map[MAX_QPATH] = { 0 };
					if (removedVotes[i] && LongMapNameFromChar(removedVotes[i], NULL, 0, map, sizeof(map)))
						Q_strncpyz(selection->printMessage[i], va("Runoff vote:\n"S_COLOR_RED"%s"S_COLOR_RED" was eliminated\n"S_COLOR_YELLOW"Please vote again"S_COLOR_WHITE, map), sizeof(selection->printMessage[i]));
					else
						Q_strncpyz(selection->printMessage[i], va("Runoff vote: "S_COLOR_YELLOW"please vote again"S_COLOR_WHITE), sizeof(selection->printMessage[i]));
				}
				else
					Q_strncpyz(selection->printMessage[i], va("%sote for a map%s:", level.inRunoff ? "Runoff v" : "V", g_multiVoteRNG.integer ? " to increase its probability" : ""), sizeof(selection->printMessage[i]));
			}
		}

		// try to print the full map name to players (if a full name doesn't appear, map has no .arena or isn't installed)
		char *mapDisplayName = NULL;
		const char *arenaInfo = G_GetArenaInfoByMap( mapname );

		if ( arenaInfo ) {
			mapDisplayName = Info_ValueForKey( arenaInfo, "longname" );
			Q_CleanStr( mapDisplayName );
		}

		if ( !VALIDSTRING( mapDisplayName ) ) {
			mapDisplayName = mapname;
		}

		char mapChar = CharFromMapName(mapname);
		if (mapChar) {
			level.multiVoteMapChars[selection->numSelected - 1] = mapChar;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (runoffSurvivors & (1llu << (unsigned long long)i)) {
					Q_strcat(selection->printMessage[i], sizeof(selection->printMessage[i]),
						va("\n"S_COLOR_WHITE"%c - %s", mapChar, mapDisplayName)
					);
				}
				else {
					Q_strcat(selection->printMessage[i], sizeof(selection->printMessage[i]),
						va("\n"S_COLOR_CYAN"/vote %c "S_COLOR_WHITE" - %s", mapChar, mapDisplayName)
					);
				}
			}
		}
		else {
			assert(MAX_PUGMAPS < 10 && selection->numSelected < 10);
			level.multiVoteMapChars[selection->numSelected - 1] = (char)selection->numSelected + '0';
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (runoffSurvivors & (1llu << (unsigned long long)i)) {
					Q_strcat(selection->printMessage[i], sizeof(selection->printMessage[i]),
						va("\n"S_COLOR_WHITE"%d - %s", selection->numSelected, mapDisplayName)
					);
				}
				else {
					Q_strcat(selection->printMessage[i], sizeof(selection->printMessage[i]),
						va("\n"S_COLOR_CYAN"/vote %d "S_COLOR_WHITE" - %s", selection->numSelected, mapDisplayName)
					);
				}
			}
		}
	}
}

extern void CountPlayersIngame( int *total, int *ingame );

void Svcmd_MapRandom_f()
{
	char pool[64];
	int mapsToRandomize;

	if (trap_Argc() < 2) {
		return;
	}

    trap_Argv( 1, pool, sizeof( pool ) );

	if ( trap_Argc() < 3 ) {
		// default to 1 map if no argument
		mapsToRandomize = 1;
	} else {
		// get it from the argument then
		char mapsToRandomizeBuf[64];
		trap_Argv( 2, mapsToRandomizeBuf, sizeof( mapsToRandomizeBuf ) );

		mapsToRandomize = atoi( mapsToRandomizeBuf );
		if ( mapsToRandomize < 1 ) mapsToRandomize = 1; // we don't want negative/zero maps. the upper limit is set in callvote code so rcon can do whatever
	}

	if ( mapsToRandomize > 1 ) {
		int total, ingame;
		CountPlayersIngame( &total, &ingame );
		if ( ingame < 2 ) { // how? this should have been handled in callvote code, maybe some stupid admin directly called it with nobody in game..?
							// or it could also be caused by people joining spec before map_random passes... so inform them, i guess
			G_Printf( "Not enough people in game to start a multi vote, a single map will be randomized\n" );
			mapsToRandomize = 1;
		}
	}

	char currentMap[MAX_MAP_NAME];
    trap_Cvar_VariableStringBuffer( "mapname", currentMap, sizeof( currentMap ) );
	
	MapSelectionContext context;
	memset( &context, 0, sizeof( context ) );

	if ( mapsToRandomize > 1 ) { // if we are randomizing more than one map, there will be a second vote
		if ( level.multiVoting && level.voteTime ) {
			return; // theres already a multi vote going on, retard
		}

		if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
			return; // another vote just passed and is waiting to execute, don't interrupt it...
		}

		context.announceMultiVote = qtrue;
	}
	memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
	if ( G_DBSelectMapsFromPool( pool, currentMap, mapsToRandomize, mapSelectedCallback, &context ) )
    {
		if ( context.numSelected != mapsToRandomize ) {
			G_Printf( "Could not randomize this many maps! Expected %d, but randomized %d\n", mapsToRandomize, context.numSelected );
		}

		// print in console and do a global prioritized center print
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!level.inRunoff) // don't spam the console for non-first rounds
				trap_SendServerCommand(i, va("print \"%s\n\"", context.printMessage[i]));
		}
		G_UniqueTickedCenterPrint( &context.printMessage, sizeof(context.printMessage[0]), 15000, qtrue ); // give them 15s to see the options

		if ( context.numSelected > 1 ) {
			// we are going to need another vote for this...
			StartMultiMapVote( context.numSelected, context.listOfMaps );
		} else {
			// we want 1 map, this means listOfMaps only contains 1 randomized map. Just change to it straight away.
			trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", context.listOfMaps ) );
		}

		return;
    }

    G_Printf( "Map pool '%s' not found\n", pool );
}

void Svcmd_RandomPugMap_f()
{
	char maps[MAX_PUGMAPS][64] = { 0 };
	int mapsToRandomize = 0;

	if (trap_Argc() < 2) {
		return;
	}

	char theArg[64];
	trap_Argv(1, theArg, sizeof(theArg));
	int i, len = strlen(theArg);
	for (i = 0; i < MAX_PUGMAPS && i < len; i++) {
		if (!LongMapNameFromChar(theArg[i], maps[i], sizeof(maps[i]), NULL, 0)) {
			Com_Printf("Unrecognized map '%c'\n", tolower(theArg[i]));
			return;
		}
		mapsToRandomize++;
	}

	if (mapsToRandomize < 1) {
		Com_Printf("Invalid maps to be randomized.\n");
		return;
	}

	int total, ingame;
	CountPlayersIngame(&total, &ingame);
	if (ingame < 2) { // how? this should have been handled in callvote code, maybe some stupid admin directly called it with nobody in game..?
					  // or it could also be caused by people joining spec before map_random passes... so inform them, i guess
		Com_Printf("Not enough people in game to start a multi vote; a single map will be randomized.\n");
		mapsToRandomize = 1;
	}

	MapSelectionContext context;
	memset(&context, 0, sizeof(context));

	if (mapsToRandomize > 1) { // if we are randomizing more than one map, there will be a second vote
		if (level.multiVoting && level.voteTime) {
			return; // theres already a multi vote going on, retard
		}

		if (level.voteExecuteTime && level.voteExecuteTime < level.time) {
			return; // another vote just passed and is waiting to execute, don't interrupt it...
		}

		context.announceMultiVote = qtrue;
	}
	memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
	for (i = 0; i < mapsToRandomize; i++)
		mapSelectedCallback(&context, maps[i]);

	// print in console and do a global prioritized center print
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!level.inRunoff) // don't spam the console for non-first rounds
			trap_SendServerCommand(i, va("print \"%s\n\"", context.printMessage[i]));
	}
	G_UniqueTickedCenterPrint(&context.printMessage, sizeof(context.printMessage[0]), 15000, qtrue); // give them 15s to see the options

	if (context.numSelected > 1) {
		// we are going to need another vote for this...
		StartMultiMapVote(context.numSelected, context.listOfMaps);
	}
	else {
		// we want 1 map, this means listOfMaps only contains 1 randomized map. Just change to it straight away.
		trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", context.listOfMaps));
	}
	LivePugRuined("Map voting", qfalse); // should be obvious enough; doesn't need announcement
}

void Svcmd_NewPug_f(void) {
	char arg[MAX_PUGMAPS + 1] = { 0 };
	if (trap_Argc() < 2) { // no arg
		if (g_defaultPugMaps.string[0])
			Q_strncpyz(arg, g_defaultPugMaps.string, sizeof(arg));
		else
			Q_strncpyz(arg, "hncu", sizeof(arg));
	}
	else {
		trap_Argv(1, arg, sizeof(arg));
		if (!arg[0]) {
			if (g_defaultPugMaps.string[0])
				Q_strncpyz(arg, g_defaultPugMaps.string, sizeof(arg));
			else
				Q_strncpyz(arg, "hncu", sizeof(arg));
		}
	}

	int i, len = strlen(arg);
	for (i = 0; i < MAX_PUGMAPS && i < len; i++) {
		if (!LongMapNameFromChar(arg[i], NULL, 0, NULL, 0)) {
			Com_Printf("Unrecognized map '%c'\n", tolower(arg[i]));
			return;
		}
	}

	if (!level.inRunoff) { // only change the cvars for the first vote; otherwise weird things will happen
		trap_SendServerCommand(-1, va("print \"Starting a new pug%s.\n\"", strlen(playedPugMaps.string) ? "; clearing list of played maps" : ""));
		trap_Cvar_Set("playedPugMaps", "");
		trap_Cvar_Set("desiredPugMaps", arg);
	}
	trap_SendConsoleCommand(EXEC_APPEND, va("randompugmap %s\n", arg));
}

void Svcmd_NextPug_f(void) {
	char arg[MAX_PUGMAPS + 1] = { 0 }, maps[MAX_PUGMAPS + 1] = { 0 }, currentMap[64];
	trap_Cvar_VariableStringBuffer("mapname", currentMap, sizeof(currentMap));
	char played[MAX_STRING_CHARS], thisMapChar = CharFromMapName(currentMap);
	Q_strncpyz(played, playedPugMaps.string, sizeof(played));
	if (desiredPugMaps.string[0])
		Q_strncpyz(maps, desiredPugMaps.string, sizeof(maps));
	else
		Q_strncpyz(maps, "hncub", sizeof(maps));

	// add the current map to the cvar
	if (thisMapChar && !strchr(played, thisMapChar)) {
		Q_strcat(played, sizeof(played), va("%c", thisMapChar));
		trap_Cvar_Set("playedPugMaps", played);
		trap_Cvar_Update(&playedPugMaps);
	}

	int i;
	for (i = 0; i < sizeof(maps); i++) {
		if (maps[i] && !strchr(playedPugMaps.string, maps[i]))
			Q_strcat(arg, sizeof(arg), va("%c", maps[i]));
	}

	if (!arg[0]) { // all maps have been played
		Svcmd_NewPug_f();
		return;
	}

	trap_SendConsoleCommand(EXEC_APPEND, va("randompugmap %s", arg));
}

void Svcmd_KillTurrets_f(qboolean announce)
{
#ifndef _DEBUG
	level.mapCaptureRecords.readonly = qtrue;
	LivePugRuined("Killturrets", qfalse); // should be obvious enough; doesn't need announcement
#endif
	int i = 0;
	gentity_t* ent;
	while (i < level.num_entities)
	{
		ent = &g_entities[i];
		if (ent->s.eType != ET_NPC && ent->s.eType != ET_PLAYER && ent->s.weapon)
		{
			if (ent->s.weapon == WP_EMPLACED_GUN)
			{
				G_RadiusDamage(ent->r.currentOrigin, NULL, 6666, 128, ent, NULL, MOD_SUICIDE); //kill it in style
			}
			else if (ent->s.weapon == WP_TURRET)
			{
				G_FreeEntity(ent); //boring, just remove it
			}
		}
		i++;
	}
	if (announce)
	{
		trap_SendServerCommand(-1, va("print \"Turrets destroyed.\n\""));
	}
}

void Svcmd_GreenDoors_f(qboolean announce)
{
	level.mapCaptureRecords.readonly = qtrue;
	LivePugRuined("Greendoors", qfalse); // should be obvious enough; doesn't need announcement
	gentity_t *doorent;
	int i = 0;

	for (i = 0; i < MAX_GENTITIES; i++)
	{
		doorent = &g_entities[i];
		if (doorent->blocked && doorent->blocked == Blocked_Door && doorent->spawnflags && doorent->spawnflags & 16)
		{
			UnLockDoors(doorent);
		}
	}
	if (announce)
	{
		trap_SendServerCommand(-1, va("print \"Doors greened.\n\""));
	}
}

qboolean DoRunoff(void) {
	if (!g_runoffVote.integer && !level.inRunoff)
		return qfalse;

	runoffSurvivors = runoffLosers = 0llu;

	// count the votes
	qboolean gotAnyVote = qfalse;
	int numVotesForMap[64] = { 0 };
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		int voteId = level.multiVotes[i];
		if (voteId > 0 && voteId <= level.multiVoteChoices) {
			numVotesForMap[voteId - 1]++;
			gotAnyVote = qtrue;
		}
	}

	if (!gotAnyVote)
		return qfalse; // nobody has voted whatsoever; don't do anything

	// determine the highest and lowest numbers
	int lowestNumVotes = 0x7FFFFFFF, highestNumVotes = 0;
	for (int i = 0; i < level.multiVoteChoices; i++) {
		if (numVotesForMap[i] > 0 && numVotesForMap[i] < lowestNumVotes)
			lowestNumVotes = numVotesForMap[i];
		if (numVotesForMap[i] > highestNumVotes)
			highestNumVotes = numVotesForMap[i];
	}

	// determine which maps are tied for highest/lowest/zero
	int numMapsTiedForFirst = 0, numMapsNOTTiedForFirst = 0, numMapsTiedForLast = 0;
	for (int i = 0; i < level.multiVoteChoices; i++) {
		if (numVotesForMap[i] == highestNumVotes)
			numMapsTiedForFirst++;
		else if (numVotesForMap[i] > 0 && numVotesForMap[i] < highestNumVotes) {
			numMapsNOTTiedForFirst++;
			if (numVotesForMap[i] == lowestNumVotes) {
				numMapsTiedForLast++;
			}
		}
	}

	if (!numMapsNOTTiedForFirst && numMapsTiedForFirst == 2) {
		// it's a tie between two maps; let rng decide
		return qfalse;
	}

	if (numMapsTiedForFirst == 1 && numMapsNOTTiedForFirst <= 1) {
		// exactly one map is in first place AND exactly one map is in last place
		// or exactly one map got votes
		// let the normal routine pick the first place map as winner
		return qfalse;
	}

	// if we got here, at least one map will be eliminated
	// determine which maps made the cut and which will be eliminated
	char newMapChars[MAX_PUGMAPS + 1] = { 0 }, choppingBlock[MAX_PUGMAPS + 1] = { 0 };
	for (int i = 0; i < level.multiVoteChoices; i++) {
		if (numVotesForMap[i] <= 0) { // this map has exactly 0 votes; remove it
			continue;
		}
		else if (highestNumVotes == lowestNumVotes) { // a tie; add this one to the chopping block
			char buf[2] = { 0 };
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(choppingBlock, sizeof(choppingBlock), buf);
		}
		else if (numVotesForMap[i] >= highestNumVotes) { // this map is tied for first; add it
			char buf[2] = { 0 };
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(newMapChars, sizeof(newMapChars), buf);
		}
		else if (numVotesForMap[i] == lowestNumVotes) { // this map is non-zero and tied for last
			if (numMapsTiedForLast <= 1) { // only one map is tied for last; remove it
				continue;
			}
			else { // multiple maps are tied for last; add this one to the chopping block
				char buf[2] = { 0 };
				buf[0] = level.multiVoteMapChars[i];
				Q_strcat(choppingBlock, sizeof(choppingBlock), buf);
			}
		}
		else { // this map is non-zero, not in first place, but not in last place; add it
			char buf[2] = { 0 };
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(newMapChars, sizeof(newMapChars), buf);
		}
	}

	// if needed, randomly remove one map from the chopping block and add the others
	int numOnChoppingBlock = strlen(choppingBlock);
	if (numOnChoppingBlock > 0) {
		for (int i = numOnChoppingBlock - 1; i >= 1; i--) { // fisher-yates
			int j = rand() % (i + 1);
			int temp = choppingBlock[i];
			choppingBlock[i] = choppingBlock[j];
			choppingBlock[j] = temp;
		}
		choppingBlock[numOnChoppingBlock - 1] = '\0';
		Q_strcat(newMapChars, sizeof(newMapChars), choppingBlock);
	}

	// re-order the remaining maps to match the original order
	// and note which maps got eliminated (including ones with zero votes)
	int numOldMaps = strlen(level.multiVoteMapChars);
	char temp[MAX_PUGMAPS + 1] = { 0 }, eliminatedMaps[MAX_PUGMAPS + 1] = { 0 };
	for (int i = 0; i < numOldMaps; i++) {
		char *p = strchr(newMapChars, level.multiVoteMapChars[i]), buf[2] = { 0 };
		if (VALIDSTRING(p)) { // this map made the cut
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(temp, sizeof(temp), buf);
		}
		else { // this map was eliminated
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(eliminatedMaps, sizeof(eliminatedMaps), buf);
			G_LogPrintf("Choice %c was eliminated from contention.\n", buf[0]);
		}
	}
	if (temp[0])
		memcpy(newMapChars, temp, sizeof(newMapChars));
	int numRemainingMaps = strlen(newMapChars);

	// determine which people will be happy or sad
	// mark still-valid choices as needing to be reinstated in the next round
	memset(&reinstateVotes, 0, sizeof(reinstateVotes));
	memset(&removedVotes, 0, sizeof(removedVotes));
	int numRunoffLosers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		int voteId = level.multiVotes[i];
		if (level.clients[i].pers.connected == CON_CONNECTED && voteId > 0 && voteId <= level.multiVoteChoices) {
			if (!strchr(newMapChars, level.multiVoteMapChars[voteId - 1])) { // this guy's map was removed
				level.clients[i].mGameFlags &= ~PSG_VOTED;
				runoffLosers |= (1llu << (unsigned long long)i);
				removedVotes[i] = level.multiVoteMapChars[level.multiVotes[i] - 1];
				numRunoffLosers++;
				G_LogPrintf("Client %d (%s) had their \"%c\" vote removed.\n", i, level.clients[i].pers.netname, level.multiVoteMapChars[voteId - 1]);
			}
			else { // this guy's map survived; his vote will be reinstated
				reinstateVotes[i] = level.multiVoteMapChars[level.multiVotes[i] - 1];
				runoffSurvivors |= (1llu << (unsigned long long)i);
			}
		}
		if (numRemainingMaps > 1)
			level.multiVotes[i] = 0; // if there will be another round, reset everyone's vote (some will be reinstated anyway)
	}

	// notify players which map(s) got removed
	if (numRemainingMaps > 1) {
		int numEliminatedMaps = strlen(eliminatedMaps);
		if (numEliminatedMaps > 0) {
			// create the string containing the eliminated map names
			char removedMapNamesStr[MAX_STRING_CHARS] = { 0 };
			for (int i = 0; i < numEliminatedMaps; i++) {
				char buf[MAX_QPATH] = { 0 };
				if (LongMapNameFromChar(eliminatedMaps[i], NULL, 0, buf, sizeof(buf))) {
					if (numEliminatedMaps == 2 && i == 1) {
						Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), " and ");
					}
					else if (numEliminatedMaps > 2 && i > 0) {
						Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), ", ");
						if (i == numEliminatedMaps - 1)
							Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), "and ");
					}
					Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), buf);
				}
			}
			Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), numEliminatedMaps == 1 ? " was" : " were");
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].pers.connected != CON_CONNECTED || g_entities[i].r.svFlags & SVF_BOT)
					continue;
				if (runoffLosers & (1llu << (unsigned long long)i)) {
					if (numRunoffLosers == 1)
						trap_SendServerCommand(i, va("print \"%s eliminated from contention. You may re-vote.\n\"", removedMapNamesStr));
					else
						trap_SendServerCommand(i, va("print \"%s eliminated from contention. You and %d other player%s may re-vote.\n\"", removedMapNamesStr, numRunoffLosers - 1, numRunoffLosers - 1 == 1 ? "" : "s"));
				}
				else {
					trap_SendServerCommand(i, va("print \"%s eliminated from contention. %d player%s may re-vote.\n\"", removedMapNamesStr, numRunoffLosers, numRunoffLosers == 1 ? "" : "s"));
				}
			}
		}
		// start the next round of voting
		level.multiVoting = qfalse;
		level.inRunoff = qtrue;
		trap_SendConsoleCommand(EXEC_APPEND, va("newpug \"%s\"\n", newMapChars));
		return qtrue;
	}
	else {
		// everything was eliminated except one map
		// let the normal routine select it as the winner
		return qfalse;
	}
}

// allocates an array of length numChoices containing the sorted results from level.multiVoteChoices
// don't forget to FREE THE RESULT
int* BuildVoteResults( const int numChoices, int *numVotes, int *highestVoteCount ) {
	int i;
	int *voteResults = calloc( numChoices, sizeof( *voteResults ) ); // voteResults[i] = how many votes for the i-th choice

	if ( numVotes ) *numVotes = 0;
	if ( highestVoteCount ) *highestVoteCount = 0;

	for ( i = 0; i < MAX_CLIENTS; ++i ) {
		int voteId = level.multiVotes[i];

		if ( voteId > 0 && voteId <= numChoices ) {
			// one more valid vote...
			if ( numVotes ) ( *numVotes )++;
			voteResults[voteId - 1]++;

			if ( highestVoteCount && voteResults[voteId - 1] > *highestVoteCount ) {
				*highestVoteCount = voteResults[voteId - 1];
			}
		} else if ( voteId ) { // shouldn't happen since we check that in /vote...
			G_LogPrintf( "Invalid multi vote id for client %d: %d\n", i, voteId );
		}
	}

	return voteResults;
}

static void PickRandomMultiMap( const int *voteResults, const int numChoices, const int numVotingClients,
	const int numVotes, const int highestVoteCount, char *out, size_t outSize ) {
	int i;

	if ( highestVoteCount >= ( numVotingClients / 2 ) + 1 ) {
		// one map has a >50% majority, find it and pass it
		for ( i = 0; i < numChoices; ++i ) {
			if ( voteResults[i] == highestVoteCount ) {
				trap_Argv( i + 1, out, outSize );
				return;
			}
		}
	}

	// now, since the amount of votes is pretty low (always <= MAX_CLIENTS) we can just build a uniformly distributed function
	// this isn't the fastest but it is more readable
	int *udf;
	int random;

	if ( !numVotes ) {
		// if nobody voted, just give all maps a weight of 1, ie the same probability
		udf = malloc( sizeof( *udf ) * numChoices );

		for ( i = 0; i < numChoices; ++i ) {
			udf[i] = i + 1;
		}

		random = rand() % numChoices;
	} else {
		if (!g_multiVoteRNG.integer) {
			// check if there is a single map with the most votes, in which case we simply choose that one
			int numWithHighestVoteCount = 0, mapWithHighestVoteCount = -1;
			for (i = 0; i < numChoices; i++) {
				if (voteResults[i] == highestVoteCount) {
					numWithHighestVoteCount++;
					mapWithHighestVoteCount = i;
				}
			}
			if (numWithHighestVoteCount == 1 && mapWithHighestVoteCount != -1) {
				trap_Argv(mapWithHighestVoteCount + 1, out, outSize);
				return;
			}
		}

		// make it an array where each item appears as many times as they were voted, thus giving weight (0 votes = 0%)
		int items = numVotes, currentItem = 0;
		udf = malloc( sizeof( *udf ) * items );

		for ( i = 0; i < numChoices; ++i ) {
			if (g_multiVoteRNG.integer) {
				if (highestVoteCount - voteResults[i] > (numVotingClients / 4)) {
					// rule out very low vote counts relatively to the highest one and the max voting clients
					items -= voteResults[i];
					udf = realloc(udf, sizeof(*udf) * items);
					continue;
				}
			}
			else {
				if (voteResults[i] < highestVoteCount) {
					// rng is disabled; simply rule out vote counts that are not tied for most votes
					items -= voteResults[i];
					udf = realloc(udf, sizeof(*udf) * items);
					continue;
				}
			}

			int j;

			for ( j = 0; j < voteResults[i]; ++j ) {
				udf[currentItem++] = i + 1;
			}
		}

		random = rand() % items;
	}

	// since the array is uniform, we can just pick an index to have a weighted map pick
	trap_Argv( udf[random], out, outSize );
	free( udf );
}

void Svcmd_MapMultiVote_f() {
	if ( !level.multiVoting || !level.multiVoteChoices ) {
		return; // this command should only be run by the callvote code
	}

	if ( trap_Argc() - 1 != level.multiVoteChoices ) { // wtf? this should never happen...
		G_LogPrintf( "MapMultiVote failed: argc=%d != multiVoteChoices=%d\n", trap_Argc() - 1, level.multiVoteChoices );
		return;
	}

	// get the results and pick a map
	int numVotes, highestVoteCount;
	int *voteResults = BuildVoteResults( level.multiVoteChoices, &numVotes, &highestVoteCount );

	char selectedMapname[MAX_MAP_NAME];
	PickRandomMultiMap( voteResults, level.multiVoteChoices, level.numVotingClients, numVotes, highestVoteCount, selectedMapname, sizeof( selectedMapname ) );

	// build a string to show the idiots what they voted for
	int i;
	char resultString[MAX_STRING_CHARS] = S_COLOR_WHITE"Map voting results:";
	for ( i = 0; i < level.multiVoteChoices; ++i ) {
		char mapname[MAX_MAP_NAME];
		trap_Argv( 1 + i, mapname, sizeof( mapname ) );

		// get the full name of the map
		char *mapDisplayName = NULL;
		const char *arenaInfo = G_GetArenaInfoByMap( mapname );

		if ( arenaInfo ) {
			mapDisplayName = Info_ValueForKey( arenaInfo, "longname" );
			Q_CleanStr( mapDisplayName );
		}

		if ( !VALIDSTRING( mapDisplayName ) ) {
			mapDisplayName = &mapname[0];
		}

		Q_strcat( resultString, sizeof( resultString ), va( "\n%s%s - %d vote%s",
			!Q_stricmp(mapname, selectedMapname) ? S_COLOR_GREEN : S_COLOR_WHITE, // the selected map is green
			mapDisplayName, voteResults[i], voteResults[i] != 1 ? "s" : "" )
		);
	}

	free( voteResults );

	// do both a console print and global prioritized center print
	if ( VALIDSTRING( resultString ) ) {
		trap_SendServerCommand( -1, va( "print \"%s\n\"", resultString ) );
		G_GlobalTickedCenterPrint( resultString, 3000, qtrue ); // show it for 3s, the time before map changes
	}

	// re-use the vote timer so that there's a small delay before map change
	Q_strncpyz( level.voteString, va( "map %s", selectedMapname ), sizeof( level.voteString ) );
	level.voteExecuteTime = level.time + 3000;
	level.multiVoting = qfalse;
	level.inRunoff = qfalse;
	level.multiVoteChoices = 0;
	memset(&level.multiVotes, 0, sizeof( level.multiVotes ) );
	memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
}

void Svcmd_SpecAll_f() {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (!g_entities[i].inuse || !g_entities[i].client) {
            continue;
        }

        if ((g_entities[i].client->sess.sessionTeam == TEAM_BLUE) || (g_entities[i].client->sess.sessionTeam == TEAM_RED)) {
			SetTeam(&g_entities[i], "s",qtrue);
        }
    }
	trap_SendServerCommand(-1, va("print \"All players were forced to spectator.\n\""));
	LivePugRuined("Specall", qfalse); // should be obvious enough; doesn't need announcement
}

void Svcmd_RandomCapts_f() {
    int ingame[32], spectator[32], i, numberOfIngamePlayers = 0, numberOfSpectators = 0, randNum1, randNum2;

	// TODO: ignore passwordless specs
    for (i = 0; i < level.maxclients; i++) {
        if (!g_entities[i].inuse || !g_entities[i].client) {
            continue;
        }

        if (g_entities[i].client->sess.sessionTeam == TEAM_SPECTATOR) {
            spectator[numberOfSpectators] = i;
            numberOfSpectators++;
        }
        else if ((g_entities[i].client->sess.sessionTeam == TEAM_BLUE) || (g_entities[i].client->sess.sessionTeam == TEAM_RED)) {
            ingame[numberOfIngamePlayers] = i;
            numberOfIngamePlayers++;
        }
    }

    if (numberOfIngamePlayers + numberOfSpectators < 2) {
        trap_SendServerCommand(-1, "print \"^1Not enough players on the server.\n\"");
        return;
    }

    if (numberOfIngamePlayers == 0) {
        randNum1 = rand() % numberOfSpectators;
        randNum2 = (randNum1 + 1 + (rand() % (numberOfSpectators - 1))) % numberOfSpectators;

        trap_SendServerCommand(-1, va("print \"^7The captain for team ^1RED ^7 is: %s\n\"", g_entities[spectator[randNum1]].client->pers.netname));
        trap_SendServerCommand(-1, va("print \"^7The captain for team ^4BLUE ^7 is: %s\n\"", g_entities[spectator[randNum2]].client->pers.netname));
    }
    else if (numberOfIngamePlayers == 1) {
        randNum1 = rand() % numberOfSpectators;

        if (g_entities[ingame[0]].client->sess.sessionTeam == TEAM_RED) {
            trap_SendServerCommand(-1, va("print \"^7The captain for team ^4BLUE ^7 is: %s\n\"", g_entities[spectator[randNum1]].client->pers.netname));
        }
        else if (g_entities[ingame[0]].client->sess.sessionTeam == TEAM_BLUE) {
            trap_SendServerCommand(-1, va("print \"^7The captain for team ^1RED ^7 is: %s\n\"", g_entities[spectator[randNum1]].client->pers.netname));
        }
    }
    else if (numberOfIngamePlayers == 2) {
        if (g_entities[ingame[0]].client->sess.sessionTeam != TEAM_RED) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i r\n", ingame[0]));
            SetTeam(&g_entities[ingame[0]], "red",qtrue);
        }
        if (g_entities[ingame[1]].client->sess.sessionTeam != TEAM_BLUE) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i b\n", ingame[1]));
            SetTeam(&g_entities[ingame[1]], "blue",qtrue);
        }
    }
    else {
        randNum1 = rand() % numberOfIngamePlayers;
        randNum2 = (randNum1 + 1 + (rand() % (numberOfIngamePlayers - 1))) % numberOfIngamePlayers;

        if (g_entities[ingame[randNum1]].client->sess.sessionTeam != TEAM_RED) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i r\n", ingame[randNum1]));
            SetTeam(&g_entities[ingame[randNum1]], "red", qtrue);
        }
        if (g_entities[ingame[randNum2]].client->sess.sessionTeam != TEAM_BLUE) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i b\n", ingame[randNum2]));
            SetTeam(&g_entities[ingame[randNum2]], "blue", qtrue);
        }

		int i;
		for (i = 0; i < numberOfIngamePlayers; i++) {
            if ((i == randNum1) || (i == randNum2)) {
                continue;
            }

            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i s\n", ingame[i]));
            SetTeam(&g_entities[ingame[i]], "spectator", qtrue);
        }
    }
}
extern void SiegeClearSwitchData(void);
void Svcmd_SiegeRestart_f() {
	SiegeClearSwitchData(); //gives a way for the server to reset siege to round 1
	trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
}
extern siegePers_t g_siegePersistant;
void Svcmd_ForceRound2_f() {
	char count[32];
	int mins = 0;
	int secs = 0;
	int time;
	if (g_gametype.integer != GT_SIEGE)
	{
		G_Printf("Must be in siege gametype.\n");
		return;
	}
	trap_Argv(1, count, sizeof(count));
	if (!count[0]) {
		trap_Printf("Usage: forceround2 <mm:ss> (e.g. 'forceround2 7:30' for 7 minutes, 30 seconds)\n");
		return;
	}
	if (!atoi(count) || atoi(count) <= 0 || !(strstr(count, ":") != NULL) || strstr(count, ".") != NULL || strstr(count, "-") != NULL || strstr(count, ",") != NULL)
	{
		trap_Printf("Usage: forceround2 <mm:ss> (e.g. 'forceround2 7:30' for 7 minutes, 30 seconds)\n");
		return;
	}
	sscanf(count, "%d:%d", &mins, &secs);
	time = (mins * 60) + secs;
	if (!mins || secs > 59 || secs < 0 || mins < 0 || time <= 0)
	{
		trap_Printf("Invalid time.\n");
		return;
	}
	if (time > 3600)
	{
		trap_Printf("Time must be under 60 minutes.\n");
		return;
	}
	trap_Cvar_Set("g_siegeObjStorage", "none");
	trap_Cvar_Set("g_objscompleted_old", "0");
	g_siegePersistant.beatingTime = qtrue;
	g_siegePersistant.lastTeam = 1;
	g_siegePersistant.lastTime = time * 1000;
	trap_SiegePersSet(&g_siegePersistant);
	trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
	trap_SendServerCommand(-1, va("print \"Round 2 beginning with countdown of %s.\n\"", count));
}

void Svcmd_RemovePassword_f()
{
	trap_Cvar_Set("g_password", "");
	trap_Printf("g_password has been cleared. The server no longer requires a password.\n");
}

void Svcmd_Zombies_f()
{
	level.mapCaptureRecords.readonly = qtrue;
	LivePugRuined("Zombies", qfalse); // should be obvious enough; doesn't need announcement
	if (level.zombies)
	{
		return;
	}
	else
	{
		level.zombies = qtrue;
		Svcmd_GreenDoors_f(qfalse);
		trap_Cvar_Set("g_siegeRespawn", "5");
		Svcmd_KillTurrets_f(qfalse);
		trap_Cvar_Set("oJediLimit", "0");
		trap_Cvar_Set("dAssaultLimit", "0");
		trap_Cvar_Set("dHWLimit", "0");
		trap_Cvar_Set("dDemoLimit", "0");
		trap_Cvar_Set("dTechLimit", "0");
		trap_Cvar_Set("dScoutLimit", "0");
		if (level.siegeMap == SIEGEMAP_CARGO) {
			int i;
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *ent = &g_entities[i];
				if (ent && VALIDSTRING(ent->classname) && !Q_stricmp(ent->classname, "func_usable") && ent->targetname && ent->targetname[0] && !Q_stricmp(ent->targetname, "ccshield")) {
					//disable cc shields on cargo2
					G_FreeEntity(ent);
				}
			}
		}
		else if (level.siegeMap == SIEGEMAP_NAR) {
			int i;
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *ent = &g_entities[i];
				if (ent && VALIDSTRING(ent->classname) && !Q_stricmp(ent->classname, "func_usable") && ent->targetname && ent->targetname[0] && (!Q_stricmp(ent->targetname, "fieldtobridge") || !Q_stricmp(ent->targetname, "obj1delayfield"))) {
					//disable breach prints and anti-rush shields on nar shaddaa
					G_FreeEntity(ent);
				}
			}
		}
		else if (level.siegeMap == SIEGEMAP_URBAN) {
			int i;
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *ent = &g_entities[i];
				if (!ent || !VALIDSTRING(ent->classname))
					continue;
				if (!Q_stricmp(ent->classname, "misc_siege_item")) {
					G_FreeEntity(ent);
				}
				else if (!Q_stricmp(ent->classname, "info_player_siegeteam1") && Q_stricmp(ent->targetname, "zomspawno")) {
					G_FreeEntity(ent);
				}
				else if (!Q_stricmp(ent->classname, "info_player_siegeteam2") && Q_stricmp(ent->targetname, "zomspawnd")) {
					G_FreeEntity(ent);
				}
				else if (!Q_stricmp(ent->classname, "trigger_once")) {
					G_FreeEntity(ent);
				}
				else if (!Q_stricmp(ent->classname, "trigger_multiple") && VALIDSTRING(ent->targetname) && Q_stristrclean(ent->targetname, "telehack")) {
					G_FreeEntity(ent);
				}
				else if (!Q_stricmp(ent->classname, "func_usable") && !(VALIDSTRING(ent->targetname) && (!Q_stricmp(ent->targetname, "doorshack") || !Q_stricmp(ent->targetname, "retreat")))) {
					G_FreeEntity(ent);
				}
				else if (!Q_stricmp(ent->classname, "func_wall")) {
					G_FreeEntity(ent);
				}
				else if (!Q_stricmp(ent->classname, "func_breakable")) {
					if (ent->spawnflags & 1 || ent->spawnflags & 8 || ent->spawnflags & 2048 || ent->maxHealth == 150 || (VALIDSTRING(ent->targetname) && !Q_stricmp(ent->targetname, "obj2backdoorlockbox"))) {
						G_FreeEntity(ent);
					}
					else {
						ent->health = 999999;
						ent->maxHealth = 999999;
						ent->paintarget = NULL;
					}
				}
				else if (!Q_stricmp(ent->classname, "target_print") && VALIDSTRING(ent->targetname) && !Q_stricmpn(ent->targetname, "breach", 6)) {
					ent->message = NULL;
				}
				else if (ent->client && ent->NPC && ent->s.NPC_class == CLASS_LIZARD) {
					ent->health = 999999;
					ent->maxHealth = 999999;
					ent->paintarget = NULL;
				}
			}
		}
	}

	trap_SendServerCommand(-1, va("print \"Zombies mode has been %s^7\n\"", level.zombies ? "^2enabled^7. Zombies will automatically be disabled again on map restart." : "^1disabled^7. Please restart the map to fully disable zombies."));
}

void Svcmd_RandomTeams_f(qboolean shuffle) {
	int args = trap_Argc() - 1;
    if (args < 2) {
		Com_Printf("Usage: %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n", shuffle ? "shuffle" : "random");
        return;
    }

	LivePugRuined("Randomteams", qfalse); // should be obvious enough; doesn't need announcement
	char count[2];
    trap_Argv(1, count, sizeof(count));
    int team1Count = atoi(count);

    trap_Argv(2, count, sizeof(count));
    int team2Count = atoi(count);

    if ((team1Count <= 0) || (team2Count <= 0)) {
		Com_Printf("Both teams need at least 1 player.\n");
		Com_Printf("Usage: %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n", shuffle ? "shuffle" : "random");
        return;
    }

	// gather up everyone who's ready
	int oldRedPlayers = 0, oldBluePlayers = 0;
	int numberOfReadyPlayers = 0, numberOfOtherPlayers = 0;
	int otherPlayers[MAX_CLIENTS], readyPlayers[MAX_CLIENTS];
    for (int i = 0; i < level.maxclients; i++) {
        if (!g_entities[i].inuse || !g_entities[i].client)
            continue;

		if (g_entities[i].client->sess.sessionTeam == TEAM_RED)
			oldRedPlayers |= (1llu << (unsigned long long)i);
		else if (g_entities[i].client->sess.sessionTeam == TEAM_BLUE)
			oldBluePlayers |= (1llu << (unsigned long long)i);

        if (!g_entities[i].client->pers.ready) {
            otherPlayers[numberOfOtherPlayers] = i;
            numberOfOtherPlayers++;
            continue;
        }

        readyPlayers[numberOfReadyPlayers] = i;
        numberOfReadyPlayers++;
    }
	if (numberOfReadyPlayers < team1Count + team2Count) {
		trap_SendServerCommand(-1, va("print \"^1Not enough ready players on the server: %d\n\"", numberOfReadyPlayers));
		return;
	}

	int separatedPlayers[MAX_CLIENTS / 2][2] = { 0 };
	char separatedPlayersMessages[MAX_CLIENTS / 2][128] = { 0 };
	if (args >= 4) {
		for (int i = 0; i < MAX_CLIENTS / 2 && args >= i + 4; i += 2) {
			// get the first guy in this pair
			char name[2][MAX_NAME_LENGTH];
			trap_Argv(i + 3, name[0], sizeof(name[0]));
			gentity_t *firstGuy = G_FindClient(name[0]);
			if (!firstGuy) {
				Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", name[0]);
				Com_Printf("Usage: %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n", shuffle ? "shuffle" : "random");
				return;
			}
			else if (!firstGuy->client->pers.ready) {
				Com_Printf("%s"S_COLOR_WHITE" is not ready.\n", firstGuy->client->pers.netname);
				Com_Printf("Usage: %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n", shuffle ? "shuffle" : "random");
				return;
			}

			// get the second guy in this pair
			trap_Argv(i + 4, name[1], sizeof(name[1]));
			gentity_t *secondGuy = G_FindClient(name[1]);
			if (!secondGuy) {
				Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", name[1]);
				Com_Printf("Usage: %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n", shuffle ? "shuffle" : "random");
				return;
			}
			else if (!secondGuy->client->pers.ready) {
				Com_Printf("%s"S_COLOR_WHITE" is not ready.\n", secondGuy->client->pers.netname);
				Com_Printf("Usage: %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n", shuffle ? "shuffle" : "random");
				return;
			}
			else if (secondGuy == firstGuy) {
				Com_Printf("Client %s"S_COLOR_WHITE" is the same person as client %s"S_COLOR_WHITE".\n", name[1], name[0]);
				Com_Printf("Usage: %steams <# red players> <# blue players> [player A to separate] [player B to separate] ...\n", shuffle ? "shuffle" : "random");
				return;
			}

			// mark them to be separated
			separatedPlayers[i / 2][0] = firstGuy - g_entities;
			separatedPlayers[i / 2][1] = secondGuy - g_entities;
			Q_strncpyz(separatedPlayersMessages[i / 2], va("%s"S_COLOR_WHITE" and %s"S_COLOR_WHITE, firstGuy->client->pers.netname, secondGuy->client->pers.netname), sizeof(separatedPlayersMessages[0]));
		}
	}

	// randomize
	int tries = 0;
	unsigned long long newRedPlayers, newBluePlayers;
	int redCaptain = -1, blueCaptain = -1;
	qboolean success = qfalse;

continueOuterLoop:
	while (tries < 16384) {
		for (int i = numberOfReadyPlayers - 1; i >= 1; i--) { // fisher-yates
			int j = rand() % (i + 1);
			int temp = readyPlayers[i];
			readyPlayers[i] = readyPlayers[j];
			readyPlayers[j] = temp;
		}

		newRedPlayers = newBluePlayers = 0llu;
		redCaptain = blueCaptain = -1;

		for (int i = 0; i < team1Count; i++) {
			newRedPlayers |= (1llu << (unsigned long long)readyPlayers[i]);
			if (redCaptain == -1)
				redCaptain = readyPlayers[i];
		}
		for (int i = team1Count; i < team1Count + team2Count; i++) {
			newBluePlayers |= (1llu << (unsigned long long)readyPlayers[i]);
			if (blueCaptain == -1)
				blueCaptain = readyPlayers[i];
		}

		if (args >= 4) { // make sure certain players are separated, if needed
			for (int i = 0; i < MAX_CLIENTS / 2 && separatedPlayers[i][0] != separatedPlayers[i][1]; i++) {
				if (newRedPlayers & (1llu << (unsigned long long)separatedPlayers[i][0]) &&
					newRedPlayers & (1llu << (unsigned long long)separatedPlayers[i][1])) {
					tries++;
					goto continueOuterLoop;
				}
				else if (newBluePlayers & (1llu << (unsigned long long)separatedPlayers[i][0]) &&
					newBluePlayers & (1llu << (unsigned long long)separatedPlayers[i][1])) {
					tries++;
					goto continueOuterLoop;
				}
			}
		}

		if (shuffle) { // make sure the teams are shuffled, if needed
			if (!oldRedPlayers || !oldBluePlayers) {
				success = qtrue;
				break; // we didn't actually have teams before, so just take the first result
			}

			if (numberOfReadyPlayers > 2 && (newRedPlayers == oldBluePlayers || newBluePlayers == oldRedPlayers)) {
				tries++;
				continue; // one of the teams was simply the other old team; try again
			}

			if (newRedPlayers == oldRedPlayers || newBluePlayers == oldBluePlayers) {
				tries++;
				continue; // at least one of the teams was the same as before; try again
			}
		}

		success = qtrue;
		break; // use it
	}

	// print the public message
	if (success) {
		if (args <= 2) {
			trap_SendServerCommand(-1, va("print \"%sing teams for %d vs %d.\n\"", shuffle ? "Shuffl" : "Randomiz", team1Count, team2Count));
		}
		else {
#define MAX_RANDOMTEAMS_CHUNKS		4
#define RANDOMTEAMS_CHUNK_SIZE		1000
#define MAX_RANDOMTEAMS_SIZE		(MAX_RANDOMTEAMS_CHUNKS * RANDOMTEAMS_CHUNK_SIZE)
			char msg[MAX_RANDOMTEAMS_SIZE] = { 0 };
			Q_strncpyz(msg, va("%sing teams for %d vs %d, separating ", shuffle ? "Shuffl" : "Randomiz", team1Count, team2Count), sizeof(msg));
			for (int i = 0; i < MAX_CLIENTS / 2 && separatedPlayersMessages[i][0]; i++) {
				if (i > 0)
					Q_strcat(msg, sizeof(msg), "; ");
				Q_strcat(msg, sizeof(msg), separatedPlayersMessages[i]);
			}
			Q_strcat(msg, sizeof(msg), ".");

			int len = strlen(msg);
			if (len <= RANDOMTEAMS_CHUNK_SIZE) { // no chunking necessary
				trap_SendServerCommand(-1, va("print \"%s"S_COLOR_WHITE"\n\"", msg));
			}
			else { // chunk it
				int i, chunks = len / RANDOMTEAMS_CHUNK_SIZE;
				if (len % RANDOMTEAMS_CHUNK_SIZE > 0)
					chunks++;
				for (i = 0; i < chunks && i < MAX_RANDOMTEAMS_CHUNKS; i++) {
					char thisChunk[RANDOMTEAMS_CHUNK_SIZE + 1];
					Q_strncpyz(thisChunk, msg + (i * RANDOMTEAMS_CHUNK_SIZE), sizeof(thisChunk));
					if (thisChunk[0])
						trap_SendServerCommand(-1, va("print \"%s%s\"", thisChunk, i == chunks - 1 ? S_COLOR_WHITE".\n" : "")); // add ^7 and line break for last one
				}
			}
		}
	}
	else {
		trap_SendServerCommand(-1, va("print \"The proposed team %s is impossible to achieve.\n\"", shuffle ? "shuffle" : "randomization"));
		return;
	}

	// move everyone where they need to be
	for (unsigned long long i = 0llu; i < 32llu; i++) {
		if (!g_entities[i].inuse || !g_entities[i].client)
			continue;
		if (newRedPlayers & (1llu << i)) {
			if (g_entities[i].client->sess.sessionTeam != TEAM_RED)
				SetTeam(&g_entities[i], "red", qtrue);
		}
		else if (newBluePlayers & (1llu << i)) {
			if (g_entities[i].client->sess.sessionTeam != TEAM_BLUE)
				SetTeam(&g_entities[i], "blue", qtrue);
		}
		else {
			if (g_entities[i].client->sess.sessionTeam != TEAM_SPECTATOR)
				SetTeam(&g_entities[i], "spectator", qtrue);
		}
	}

	// unready everyone if autostart is enabled
	if (g_autoStart.integer) {
		if (GetCurrentRestartCountdown())
			level.randomizedTeams = qtrue; // to cancel the countdown
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent || !ent->inuse || !ent->client)
				continue;
			ent->client->pers.ready = qfalse;
		}
	}

#if 0
	if (redCaptain != -1)
		trap_SendServerCommand(-1, va("print \"^2The captain in team ^1RED ^2is^7: %s\n\"", g_entities[redCaptain].client->pers.netname));
	if (blueCaptain != -1)
		trap_SendServerCommand(-1, va("print \"^2The captain in team ^4BLUE ^2is^7: %s\n\"", g_entities[blueCaptain].client->pers.netname));
#endif
}

typedef struct
{
	int entNum;

} AliasesContext;

void listAliasesCallbackServer(void* context,
	const char* name,
	int duration)
{
	AliasesContext* thisContext = (AliasesContext*)context;
	G_Printf("%s"S_COLOR_WHITE" (%i).\n", name, duration);
}

//void singleAliasCallbackServer(void* context,
//	const char* name,
//	int duration)
//{
//	AliasesContext* thisContext = (AliasesContext*)context;
//	G_Printf("%s"S_COLOR_WHITE"\n", name);
//}

static void Svcmd_WhoIs_f(void)
{
	char buffer[64];
	gentity_t* found = NULL;
	AliasesContext context;

	context.entNum = -1;

	if (trap_Argc() < 2)
	{
		G_Printf("usage: whois [name or client number]  (name can be just part of name, colors don't count. use ^5/rcon status^7 to see client numbers)  \n");
		return;
	}

	trap_Argv(1, buffer, sizeof(buffer));
	found = G_FindClient(buffer);

	if (!found || !found->client)
	{
		G_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", buffer);
		return;
	}

	G_Printf("Aliases for client %i (%s"S_COLOR_WHITE").\n", found - g_entities, found->client->pers.netname);

	unsigned int maskInt = 0xFFFFFFFF;

	if (trap_Argc() > 2)
	{
		char mask[20];
		trap_Argv(2, mask, sizeof(mask));
		maskInt = 0;
		getIpFromString(mask, &maskInt);
	}

	G_DBListAliases(found->client->sess.ip, maskInt, 3, listAliasesCallbackServer, &context, found->client->sess.auth == AUTHENTICATED ? found->client->sess.cuidHash : 0);
}

void Svcmd_ClientDesc_f( void ) {
	int i;

	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED && !( &g_entities[i] && g_entities[i].r.svFlags & SVF_BOT ) ) {
			char description[MAX_STRING_CHARS] = { 0 }, userinfo[MAX_INFO_STRING] = { 0 };
			trap_GetUserinfo( i, userinfo, sizeof( userinfo ) );

#ifdef NEWMOD_SUPPORT
			if ( *Info_ValueForKey( userinfo, "nm_ver" ) ) {
				// running newmod
				Q_strcat( description, sizeof( description ), "Newmod " );
				Q_strcat( description, sizeof( description ), Info_ValueForKey( userinfo, "nm_ver" ) );

				if ( level.clients[i].sess.auth == AUTHENTICATED ) {
					Q_strcat( description, sizeof( description ), S_COLOR_GREEN" (confirmed) "S_COLOR_WHITE );

					if ( level.clients[i].sess.cuidHash[0] ) {
						// valid cuid
						Q_strcat( description, sizeof( description ), va( "(cuid hash: "S_COLOR_CYAN"%s"S_COLOR_WHITE")", level.clients[i].sess.cuidHash ) );
					} else {
						// invalid cuid, should not happen
						Q_strcat( description, sizeof( description ), S_COLOR_RED"(invalid cuid!)"S_COLOR_WHITE );
					}
				} else if ( level.clients[i].sess.auth < AUTHENTICATED ) {
					Q_strcat( description, sizeof( description ), va( S_COLOR_YELLOW" (authing: %d)"S_COLOR_WHITE, level.clients[i].sess.auth ) );
				} else {
					Q_strcat( description, sizeof( description ), S_COLOR_RED" (auth failed)"S_COLOR_WHITE );
				}

				Q_strcat( description, sizeof( description ), ", " );
#else
			if ( *Info_ValueForKey( userinfo, "nm_ver" ) ) {
				// running newmod
				Q_strcat( description, sizeof( description ), "Newmod " );
				Q_strcat( description, sizeof( description ), Info_ValueForKey( userinfo, "nm_ver" ) );
				Q_strcat( description, sizeof( description ), ", " );
#endif
			} else if ( *Info_ValueForKey( userinfo, "smod_ver" ) ) {
				// running smod
				Q_strcat( description, sizeof( description ), "SMod " );
				Q_strcat( description, sizeof( description ), Info_ValueForKey( userinfo, "smod_ver" ) );
				Q_strcat( description, sizeof( description ), ", " );
			} else {
				// running another cgame mod
				Q_strcat( description, sizeof( description ), "Unknown mod, " );
			}

			if ( *Info_ValueForKey( userinfo, "ja_guid" ) ) {
				// running an openjk engine or fork
				Q_strcat( description, sizeof( description ), "OpenJK or derivate" );
			} else {
				Q_strcat( description, sizeof( description ), "Jamp or other" );
			}

			if (g_unlagged.integer && level.clients[i].sess.unlagged) {
				Q_strcat(description, sizeof(description), " (unlagged)");
			}

			if (sv_passwordlessSpectators.integer && !level.clients[i].sess.canJoin) {
				Q_strcat(description, sizeof(description), " (^3no password^7)");
			}
			
			if ( ( g_antiWallhack.integer == 1 && !level.clients[i].sess.whTrustToggle )
				|| ( g_antiWallhack.integer >= 2 && level.clients[i].sess.whTrustToggle ) ) {
				// anti wallhack is enabled for this player
				Q_strcat( description, sizeof( description ), " (^3anti WH enabled^7)" );
			}

			if ( level.clients[i].sess.shadowMuted ) {
				Q_strcat( description, sizeof( description ), " (^3shadow muted^7)" );
			}

			G_Printf( "Client %i (%s"S_COLOR_WHITE"): %s\n", i, level.clients[i].pers.netname, description );
		}
	}
}

void Svcmd_WhTrustToggle_f( void ) {
	gentity_t	*ent;
	char		str[MAX_TOKEN_CHARS];
	char		*s;

	if ( !g_antiWallhack.integer ) {
		G_Printf( "Anti Wallhack is not enabled.\n" );
		return;
	}

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	ent = G_FindClient( str );

	if ( !ent ) {
		G_Printf( "Player not found.\n" );
		return;
	}

	ent->client->sess.whTrustToggle = !ent->client->sess.whTrustToggle;

	if ( g_antiWallhack.integer >= 2 ) {
		if ( ent->client->sess.whTrustToggle ) {
			s = "is now blacklisted (wallhack check is ACTIVE for him)";
		} else {
			s = "is no longer blacklisted (wallhack check is INACTIVE for him)";
		}
	} else {
		if ( ent->client->sess.whTrustToggle ) {
			s = "is now whitelisted (wallhack check is INACTIVE for him)";
		} else {
			s = "is no longer whitelisted (wallhack check is ACTIVE) for him";
		}
	}

	G_Printf( "Player %d (%s"S_COLOR_WHITE") %s\n", ent - g_entities, ent->client->pers.netname, s );
}

void Svcmd_FastCapsRemove_f( void ) {
#if 1
	G_Printf("Not yet implemented.\n");
#else
	if ( !level.mapCaptureRecords.enabled ) {
		G_Printf( "Capture records are disabled.\n" );
		return;
	}

	if ( trap_Argc() < 3 ) {
		G_Printf( "Usage: fastcaps_remove <type> <rank>\n" );
		return;
	}

	int type, rank;
	char str[16];

	trap_Argv( 1, str, sizeof( str ) );
	type = atoi( str );

	if ( !Q_isanumber( str ) || type < 0 || type >= CAPTURE_RECORD_NUM_TYPES ) {
		G_Printf( "Invalid type.\n" );
		return;
	}

	trap_Argv( 2, str, sizeof( str ) );
	rank = atoi( str );

	if ( !Q_isanumber( str ) || rank < 1 || rank > MAX_SAVED_RECORDS ) { // rank = index + 1
		G_Printf( "Invalid rank.\n" );
		return;
	}

	CaptureRecord *recordArray = &level.mapCaptureRecords.records[type][0];

	if ( !recordArray[rank - 1].totalTime ) {
		G_Printf( "This record is not set yet.\n" );
		return;
	}

	// shift the array to the left unless this is the last element
	if ( rank < MAX_SAVED_RECORDS ) {
		memmove( recordArray + rank - 1, recordArray + rank, ( MAX_SAVED_RECORDS - rank ) * sizeof( *recordArray ) );
	}

	// always set the last element to 0 so it gets shifted as a zero element with the others later
	memset( recordArray + MAX_SAVED_RECORDS - 1, 0, sizeof( *recordArray ) );

	// save the changes later in db
	level.mapCaptureRecords.changed = qtrue;

	G_Printf( "Rank %d deleted from category %d successfully.\n", rank, type );
#endif
}

void Svcmd_PoolCreate_f()
{
    char short_name[64];
    char long_name[64];      

    if ( trap_Argc() < 3 )
    {
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, long_name, sizeof( long_name ) );

    if ( !G_DBPoolCreate( short_name, long_name ) )
    {
        G_Printf( "Could not create pool '%s'.\n", short_name );
    }
}

void Svcmd_PoolDelete_f()
{
    char short_name[64];

    if ( trap_Argc() < 2 )
    {
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );

    if ( !G_DBPoolDelete( short_name ))
    {
        G_Printf( "Failed to delete pool '%s'.\n", short_name );
    }


}

void Svcmd_PoolMapAdd_f()
{
    char short_name[64];
    char mapname[64];
    int  weight;

    if ( trap_Argc() < 3 )
    {
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, mapname, sizeof( mapname ) );

    if ( trap_Argc() > 3 )
    {
        char weightStr[16];
        trap_Argv( 3, weightStr, sizeof( weightStr ) );

        weight = atoi( weightStr );
		if ( weight < 1 ) weight = 1;
    }
    else
    {
        weight = 1;
    } 

    if ( !G_DBPoolMapAdd( short_name, mapname, weight ) )
    {
        G_Printf( "Could not add map to pool '%s'.\n", short_name );
    }  
}

void Svcmd_PoolMapRemove_f()
{
    char short_name[64];
    char mapname[64];

    if ( trap_Argc() < 3 )
    {
        return;
    } 

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, mapname, sizeof( mapname ) );

    if ( !G_DBPoolMapRemove( short_name, mapname ) )
    {
        G_Printf( "Could not remove map from pool '%s'.\n", short_name );
    }

}

#ifdef DBCLEAN
extern void G_LogDbClean(void);
static void Svcmd_CleanDB_f(void) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (level.clients[i].pers.connected) {
			Com_Printf("Server must be empty to clean db. Try using rcon remotely with /rconaddress from the main menu.\n");
			return;
		}
	}
	static qboolean attempted = qfalse;
	if (!attempted) {
		attempted = qtrue;
		Com_Printf("Warning: cleaning the db can take a long time (up to a minute or longer). The server may appear to be unresponsive until cleaning is complete. If you are sure you want to clean the db, please enter /cleandb again. This message is only shown once.\n");
		return;
	}
	G_LogDbUnload();
	G_LogDbClean();
	G_LogDbLoad();
}
#endif

#ifdef DBHOTSWAP
extern void G_LogDbHotswap(void);
static void Svcmd_HotSwapDB_f(void) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (level.clients[i].pers.connected) {
			Com_Printf("Server must be empty to hotswap db. Try using rcon remotely with /rconaddress from the main menu.\n");
			return;
		}
	}
	static qboolean attempted = qfalse;
	if (!attempted) {
		attempted = qtrue;
		Com_Printf("Are you sure you want to do this? Enter the command once again to continue.\n");
		return;
	}
	G_LogDbHotswap();
}
#endif

static void Svcmd_NotLive_f(void) {
	if (level.isLivePug == ISLIVEPUG_NO) {
		Com_Printf("This match is already not a live pug.\n");
		return;
	}
	LivePugRuined("Admin override", qtrue);
}

char	*ConcatArgs( int start );

typedef struct {
	char format[MAX_STRING_CHARS];
} SessionPrintCtx;

static void FormatAccountSessionList(void *ctx, const sessionReference_t sessionRef, const qboolean temporary) {
	SessionPrintCtx* out = (SessionPrintCtx*)ctx;

#ifdef NEWMOD_SUPPORT
	qboolean isNewmodSession = G_SessionInfoHasString(sessionRef.ptr, "cuid_hash2");
#endif

	char sessFlags[16] = { 0 };
	if (sessionRef.online) Q_strcat(sessFlags, sizeof(sessFlags), S_COLOR_GREEN"|");
	if (temporary) Q_strcat(sessFlags, sizeof(sessFlags), S_COLOR_YELLOW"|");
#ifdef NEWMOD_SUPPORT
	if (isNewmodSession) Q_strcat(sessFlags, sizeof(sessFlags), S_COLOR_CYAN"|");
#endif

	Q_strcat(out->format, sizeof(out->format), va(S_COLOR_WHITE"Session ID %d ", sessionRef.ptr->id));
	if (VALIDSTRING(sessFlags)) Q_strcat(out->format, sizeof(out->format), va("%s ", sessFlags));
	Q_strcat(out->format, sizeof(out->format), va(S_COLOR_WHITE"(identifier: %llX)\n", sessionRef.ptr->identifier));
}


void Svcmd_Account_f( void ) {
#ifndef SESSIONS_ACCOUNTS_SYSTEM
	return;
#endif
	qboolean printHelp = qfalse;

	if (trap_Argc() > 1) {
		char s[64];
		trap_Argv(1, s, sizeof(s));

		if (!Q_stricmp(s, "create")) {

			if (trap_Argc() < 3) {
				G_Printf("Usage: account create <username>\n");
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv(2, username, sizeof(username));

			// sanitize input
			char *p;
			for (p = username; *p; ++p) {
				if (p == username) {
					if (!Q_isalpha(*p)) {
						G_Printf("Username must start with an alphabetic character\n");
						return;
					}
				}
				else if (!*(p + 1)) {
					if (!Q_isalphanumeric(*p)) {
						G_Printf("Username must end with an alphanumeric character\n");
						return;
					}
				}
				else {
					if (!Q_isalphanumeric(*p) && *p != '-' && *p != '_') {
						G_Printf("Valid characters are: a-z A-Z 0-9 - _\n");
						return;
					}
				}
			}

			accountReference_t acc;

			if (G_CreateAccount(username, &acc)) {
				G_Printf("Account '%s' created successfully\n", acc.ptr->name);
			}
			else {
				if (acc.ptr) {
					char timestamp[32];
					G_FormatLocalDateFromEpoch(timestamp, sizeof(timestamp), acc.ptr->creationDate);
					G_Printf("Account '%s' was already created on %s\n", acc.ptr->name, timestamp);
				}
				else {
					G_Printf("Failed to create account!\n");
				}
			}

		}
		else if (!Q_stricmp(s, "delete")) {

			if (trap_Argc() < 3) {
				G_Printf("Usage: account delete <username>\n");
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv(2, username, sizeof(username));

			accountReference_t acc = G_GetAccountByName(username, qfalse);

			if (!acc.ptr) {
				G_Printf("No account exists with this name\n");
				return;
			}

			if (G_DeleteAccount(acc.ptr)) {
				G_Printf("Deleted account '%s' successfully\n", acc.ptr->name);
			}
			else {
				G_Printf("Failed to delete account!\n");
			}

		}
		else if (!Q_stricmp(s, "info")) {

			if (trap_Argc() < 3) {
				G_Printf("Usage: account info <username>\n");
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv(2, username, sizeof(username));

			accountReference_t acc = G_GetAccountByName(username, qfalse);

			if (!acc.ptr) {
				G_Printf("Account '%s' does not exist\n", username);
				return;
			}

			SessionPrintCtx sessionsPrint = { 0 };
			G_ListSessionsForAccount(acc.ptr, FormatAccountSessionList, &sessionsPrint);

			char timestamp[32];
			G_FormatLocalDateFromEpoch(timestamp, sizeof(timestamp), acc.ptr->creationDate);

			// TODO: pages

			G_Printf(
				S_COLOR_YELLOW"Account Name: "S_COLOR_WHITE"%s\n"
				S_COLOR_YELLOW"Account ID: "S_COLOR_WHITE"%d\n"
				S_COLOR_YELLOW"Created on: "S_COLOR_WHITE"%s\n"
				S_COLOR_YELLOW"Group: "S_COLOR_WHITE"%s\n"
				S_COLOR_YELLOW"Flags: "S_COLOR_WHITE"%d\n"
				"\n",
				acc.ptr->name,
				acc.ptr->id,
				timestamp,
				acc.ptr->group,
				acc.ptr->flags
			);

			if (VALIDSTRING(sessionsPrint.format)) {

#ifdef NEWMOD_SUPPORT
				G_Printf("Sessions tied to this account: ( "S_COLOR_GREEN"| = online "S_COLOR_YELLOW"| = temporary "S_COLOR_CYAN"| = newmod "S_COLOR_WHITE")\n");
#else
				G_Printf("Sessions tied to this account: ( "S_COLOR_GREEN"| - online "S_COLOR_YELLOW"| - temporary "S_COLOR_WHITE")\n");
#endif
				G_Printf("%s", sessionsPrint.format);

			}
			else {
				G_Printf("No session tied to this account yet.\n");
			}

		}
		else if (!Q_stricmp(s, "help")) {
			printHelp = qtrue;
		}
		else {
			G_Printf("Invalid subcommand.\n");
			printHelp = qtrue;
		}
	}
	else {
		printHelp = qtrue;
	}

	if (printHelp) {
		G_Printf(
			"Valid subcommands:\n"
			"account create <username>: Creates a new account with the given name\n"
			"account delete <username>: Deletes the given account and unlinks all associated sessions\n"
			"account info <username>: Prints various information for the given account name\n"
			"account help: Prints this message\n"
		);
	}
}

void Svcmd_Session_f( void ) {
#ifndef SESSIONS_ACCOUNTS_SYSTEM
	return;
#endif
	qboolean printHelp = qfalse;

	if (trap_Argc() > 1) {
		char s[64];
		trap_Argv(1, s, sizeof(s));

		if (!Q_stricmp(s, "whois")) {

			qboolean printed = qfalse;

			int i;
			for (i = 0; i < level.maxclients; ++i) {
				gclient_t *client = &level.clients[i];

				if (client->pers.connected != CON_DISCONNECTED &&
					!(&g_entities[i] && g_entities[i].r.svFlags & SVF_BOT))
				{
					char* color;
					switch (level.clients[i].sess.sessionTeam) {
					case TEAM_RED: color = S_COLOR_RED; break;
					case TEAM_BLUE: color = S_COLOR_BLUE; break;
					case TEAM_FREE: color = S_COLOR_YELLOW; break;
					default: color = S_COLOR_WHITE;
					}

					printed = qtrue;

					if (!client->session) {
						G_Printf("%sClient %d "S_COLOR_WHITE"(%s"S_COLOR_WHITE"): Uninitialized session\n", color, i, client->pers.netname);
						continue;
					}

					char line[MAX_STRING_CHARS];
					line[0] = '\0';

					Q_strcat(line, sizeof(line), va("%sClient %d "S_COLOR_WHITE"(%s"S_COLOR_WHITE"): Session ID %d (identifier: %llX)",
						color, i, client->pers.netname, client->session->id, client->session->identifier));

					if (client->account) {
						Q_strcat(line, sizeof(line), va(" linked to Account ID %d '%s'", client->account->id, client->account->name));
					}

					Q_strcat(line, sizeof(line), "\n");

#ifdef _DEBUG
					Q_strcat(line, sizeof(line), va("=> Session cache num %d (ptr: 0x%lx)", client->sess.sessionCacheNum, (uintptr_t)client->session));

					if (client->account) {
						Q_strcat(line, sizeof(line), va(" ; Account cache num %d (ptr: 0x%lx)", client->sess.accountCacheNum, (uintptr_t)client->account));
					}

					Q_strcat(line, sizeof(line), "\n");
#endif

					G_Printf(line);
				}
			}

			if (!printed) {
				G_Printf("No player online\n");
			}

		}
		else if (!Q_stricmp(s, "latest")) {

			// TODO

		}
		else if (!Q_stricmp(s, "info")) {

			// TODO

		}
		else if (!Q_stricmp(s, "link")) {

			if (trap_Argc() < 4) {
				G_Printf("Usage: session link <session id> <account name>\n");
				return;
			}

			trap_Argv(2, s, sizeof(s));
			const int sessionId = atoi(s);
			if (sessionId <= 0) {
				G_Printf("Session ID must be a number > 1\n");
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv(3, username, sizeof(username));

			sessionReference_t sess = G_GetSessionByID(sessionId, qfalse);

			if (!sess.ptr) {
				G_Printf("No session found with this ID\n");
				return;
			}

			if (sess.ptr->accountId > 0) {
				accountReference_t acc = G_GetAccountByID(sess.ptr->accountId, qfalse);
				if (acc.ptr) G_Printf("This session is already linked to account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id);
				return;
			}

			accountReference_t acc = G_GetAccountByName(username, qfalse);

			if (!acc.ptr) {
				G_Printf("No account found with this name\n");
				return;
			}

			if (G_LinkAccountToSession(sess.ptr, acc.ptr)) {
				G_Printf("Session successfully linked to account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id);
			}
			else {
				G_Printf("Failed to link session to this account!\n");
			}

		}
		else if (!Q_stricmp(s, "linkingame")) {

			if (trap_Argc() < 4) {
				G_Printf("Usage: session linkingame <client id> <account name>\n");
				return;
			}

			trap_Argv(2, s, sizeof(s));
			const int clientId = atoi(s);
			if (!IN_CLIENTNUM_RANGE(clientId) ||
				!g_entities[clientId].inuse ||
				level.clients[clientId].pers.connected != CON_CONNECTED) {
				G_Printf("You must enter a valid client ID of a connected client\n");
				return;
			}

			gclient_t *client = &level.clients[clientId];

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv(3, username, sizeof(username));

			if (!client->session) {
				return;
			}

			if (client->account) {
				G_Printf("This session is already linked to account '%s' (id: %d)\n", client->account->name, client->account->id);
				return;
			}

			accountReference_t acc = G_GetAccountByName(username, qfalse);

			if (!acc.ptr) {
				G_Printf("No account found with this name\n");
				return;
			}

			if (G_LinkAccountToSession(client->session, acc.ptr)) {
				G_Printf("Client session successfully linked to account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id);
			}
			else {
				G_Printf("Failed to link client session to this account!\n");
			}

		}
		else if (!Q_stricmp(s, "unlink")) {

			if (trap_Argc() < 3) {
				G_Printf("Usage: session unlink <session id>\n");
				return;
			}

			trap_Argv(2, s, sizeof(s));
			const int sessionId = atoi(s);
			if (sessionId <= 0) {
				G_Printf("Session ID must be a number > 1\n");
				return;
			}

			sessionReference_t sess = G_GetSessionByID(sessionId, qfalse);

			if (!sess.ptr) {
				G_Printf("No session found with this ID\n");
				return;
			}

			if (sess.ptr->accountId <= 0) {
				G_Printf("This session is not linked to any account\n");
				return;
			}

			accountReference_t acc = G_GetAccountByID(sess.ptr->accountId, qfalse);

			if (!acc.ptr) {
				return;
			}

			if (G_UnlinkAccountFromSession(sess.ptr)) {
				G_Printf("Session successfully unlinked from account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id);
			}
			else {
				G_Printf("Failed to unlink session from this account!\n");
			}

		}
		else if (!Q_stricmp(s, "unlinkingame")) {

			if (trap_Argc() < 3) {
				G_Printf("Usage: session unlinkingame <client id>\n");
				return;
			}

			trap_Argv(2, s, sizeof(s));
			const int clientId = atoi(s);
			if (!IN_CLIENTNUM_RANGE(clientId) ||
				!g_entities[clientId].inuse ||
				level.clients[clientId].pers.connected != CON_CONNECTED) {
				G_Printf("You must enter a valid client ID of a connected client\n");
				return;
			}

			gclient_t *client = &level.clients[clientId];

			if (!client->session) {
				return;
			}

			if (!client->account) {
				G_Printf("This client's session is not linked to any account\n");
				return;
			}

			// save a reference so we can print the name even after unlinking
			accountReference_t acc;
			acc.ptr = client->account;
			acc.online = qtrue;

			if (G_UnlinkAccountFromSession(client->session)) {
				G_Printf("Client session successfully unlinked from account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id);
			}
			else {
				G_Printf("Failed to unlink Client session from this account!\n");
			}

		}
		else if (!Q_stricmp(s, "help")) {
			printHelp = qtrue;
		}
		else {
			G_Printf("Invalid subcommand.\n");
			printHelp = qtrue;
		}
	}
	else {
		printHelp = qtrue;
	}

	if (printHelp) {
		G_Printf(
			"Valid subcommands:\n"
			"session whois: Lists the session currently in use by all in-game players\n"
			"session latest: Lists the latest unassigned sessions\n"
			"session info <session id>: Prints detailed information for the given session ID\n"
			"session link <session id> <account name>: Links the given session ID to an existing account\n"
			"session linkingame <client id> <account name>: Shortcut command to link an in-game client's session to an existing account\n"
			"session unlink <session id>: Unlinks the account associated to the given session ID\n"
			"session unlinkingame <client id>: Shortcut command to unlink the account associated to an in-game client's session\n"
			"session help: Prints this message\n"
		);
	}
}

/*
=================
ConsoleCommand

=================
*/
void LogExit( const char *string );

qboolean	ConsoleCommand( void ) {
	char	cmd[MAX_TOKEN_CHARS];

	trap_Argv( 0, cmd, sizeof( cmd ) );

	if ( Q_stricmp (cmd, "entitylist") == 0 ) {
		Svcmd_EntityList_f();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "forceteam") == 0 ) {
		Svcmd_ForceTeam_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "forceclass") == 0) {
		Svcmd_ForceClass_f(0, NULL);
		return qtrue;
	}

	if (Q_stricmp(cmd, "unforceclass") == 0) {
		Svcmd_UnForceClass_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "forceready") == 0) {
		Svcmd_ForceReady_f();
		return qtrue;
	}
	if (Q_stricmp(cmd, "forceunready") == 0 || Q_stricmp(cmd, "unforceready") == 0) {
		Svcmd_ForceUnReady_f();
		return qtrue;
	}
	if (Q_stricmp(cmd, "skillboost") == 0) {
		Svcmd_Skillboost_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "senseboost") == 0) {
		Svcmd_Senseboost_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "aimboost") == 0) {
		Svcmd_Aimboost_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "game_memory") == 0) {
		Svcmd_GameMem_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addbot") == 0) {
		Svcmd_AddBot_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "botlist") == 0) {
		Svcmd_BotList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addip") == 0) {
		Svcmd_AddIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "removeip") == 0) {
		Svcmd_RemoveIP_f();
		return qtrue;
	}

    if ( Q_stricmp( cmd, "addwhiteip" ) == 0 )
    {
        Svcmd_AddWhiteIP_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "removewhiteip" ) == 0 )
    {
        Svcmd_RemoveWhiteIP_f();
        return qtrue;
    }    

	if (Q_stricmp (cmd, "listip") == 0) {
        Svcmd_Listip_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "svprint") == 0) {
		trap_SendServerCommand( -1, va("cp \"%s\n\"", ConcatArgs(1) ) );
		return qtrue;
	}

	if (Q_stricmp (cmd, "resetflags") == 0) {
		Svcmd_ResetFlags_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "fsreload") == 0) {
		Svcmd_FSReload_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "osinfo") == 0) {
		Svcmd_Osinfo_f();
		return qtrue;
	}	

	if (Q_stricmp (cmd, "clientban") == 0) {
		Svcmd_ClientBan_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "map_random") == 0) {
		Svcmd_MapRandom_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "randompugmap") == 0) {
		Svcmd_RandomPugMap_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "newpug") == 0) {
		Svcmd_NewPug_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "nextpug") == 0) {
		Svcmd_NextPug_f();
		return qtrue;
	}

	if ( Q_stricmp( cmd, "map_multi_vote" ) == 0 ) {
		Svcmd_MapMultiVote_f();
		return qtrue;
	}

    if ( Q_stricmp( cmd, "pool_create" ) == 0 )
    {
        Svcmd_PoolCreate_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_delete" ) == 0 )
    {
        Svcmd_PoolDelete_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_map_add" ) == 0 )
    {
        Svcmd_PoolMapAdd_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_map_remove" ) == 0 )
    {
        Svcmd_PoolMapRemove_f();
        return qtrue;
    }

	if (Q_stricmp(cmd, "siege_restart") == 0)
	{
		Svcmd_SiegeRestart_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "forceRound2") == 0)
	{
		Svcmd_ForceRound2_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "removePassword") == 0)
	{
		Svcmd_RemovePassword_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "zombies") == 0)
	{
		Svcmd_Zombies_f();
		return qtrue;
	}

	//if (Q_stricmp (cmd, "accountadd") == 0) {
	//	Svcmd_AddAccount_f();
	//	return qtrue;
	//}

	//if (Q_stricmp (cmd, "accountremove") == 0) {
	//	Svcmd_RemoveAccount_f();
	//	return qtrue;
	//}	

	//if (Q_stricmp (cmd, "accountchange") == 0) {
	//	Svcmd_ChangeAccount_f();
	//	return qtrue;
	//}	

	//if (Q_stricmp (cmd, "accountreload") == 0) {
	//	Svcmd_ReloadAccount_f();
	//	return qtrue;
	//}	

	if (Q_stricmp (cmd, "accountlist") == 0) {
		Svcmd_AccountPrint_f();
		return qtrue;
	}	
 
    //OSP: pause
    if ( !Q_stricmp( cmd, "pause" ) )
    {
		char durationStr[4];
		trap_Argv(1,durationStr,sizeof(durationStr));

		// clamp between 5 seconds and 300 seconds
		int duration = atoi(durationStr);
		if (duration < 5 || duration > 300)
			duration = 300;

        level.pause.state = PAUSE_PAUSED;
		level.pause.time = level.time + duration*1000;
		DoPauseStartChecks();

        return qtrue;
    } 

    //OSP: unpause
    if ( !Q_stricmp( cmd, "unpause" ) )
    {
		if ( level.pause.state == PAUSE_PAUSED ) {
            level.pause.state = PAUSE_UNPAUSING;
			//level.pause.time = level.time + japp_unpauseTime.integer*1000;
            level.pause.time = level.time + 3*1000;
        }

        return qtrue;
    } 

    if ( !Q_stricmp( cmd, "endmatch" ) )
    {
		level.endedWithEndMatchCommand = qtrue;
#ifdef NEWMOD_SUPPORT
		trap_SendServerCommand(-1, "lchat \"em\"");
#endif
		G_LogPrintf("Match forced to end.\n");
		trap_SendServerCommand( -1,  va("print \"Match forced to end.\n\""));
		// duo: fix siege winners if match is forced to end in round 2 and one team completed more objectives than the other
		if (g_gametype.integer == GT_SIEGE && level.siegeStage == SIEGESTAGE_ROUND2) {
			trap_Cvar_Set("siege_r2_heldformaxat", va("%i", G_FirstIncompleteObjective(2)));
			int r1objs = trap_Cvar_VariableIntegerValue("siege_r1_objscompleted");
			int r2objs = trap_Cvar_VariableIntegerValue("siege_r2_objscompleted");
			if (r1objs > r2objs) {
				level.siegeMatchWinner = SIEGEMATCHWINNER_ROUND1OFFENSE;
				G_SiegeRoundComplete(TEAM_BLUE, ENTITYNUM_NONE, qfalse);
			}
			else if (r2objs > r1objs) {
				level.siegeMatchWinner = SIEGEMATCHWINNER_ROUND2OFFENSE;
				G_SiegeRoundComplete(TEAM_RED, ENTITYNUM_NONE, qtrue);
			}
			else {
				level.siegeMatchWinner = SIEGEMATCHWINNER_TIE;
				G_SiegeRoundComplete(TEAM_BLUE, ENTITYNUM_NONE, qfalse);
			}
		}
		else
			LogExit("Match forced to end.");
        return qtrue;
    } 

	if (!Q_stricmp(cmd, "whitelist")) {
		Svcmd_Whitelist_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "lockteams" ) )
	{
		Svcmd_LockTeams_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "cointoss"))
	{
		Svcmd_Cointoss_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "rename"))
	{
		Svcmd_ForceName_f();
		return qtrue;
	}

    if (!Q_stricmp(cmd, "specall")) {
        Svcmd_SpecAll_f();
        return qtrue;
    }

	if (!Q_stricmp(cmd, "killturrets")) {
		Svcmd_KillTurrets_f(qtrue);
		return qtrue;
	}

    if (!Q_stricmp(cmd, "randomcapts")) {
        Svcmd_RandomCapts_f();
        return qtrue;
    }

    if (!Q_stricmp(cmd, "randomteams")) {
        Svcmd_RandomTeams_f(qfalse);
        return qtrue;
    }

	if (!Q_stricmp(cmd, "shuffleteams")) {
		Svcmd_RandomTeams_f(qtrue);
		return qtrue;
	}

	if (!Q_stricmp(cmd, "whois")) {
		Svcmd_WhoIs_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "greendoors")) {
		Svcmd_GreenDoors_f(qtrue);
		return qtrue;
	}
	
	if ( !Q_stricmp( cmd, "clientdesc" ) ) {
		Svcmd_ClientDesc_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "whTrustToggle" ) ) {
		Svcmd_WhTrustToggle_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "fastcaps_remove" ) ) {
		Svcmd_FastCapsRemove_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "shadowmute" ) ) {
		Svcmd_ShadowMute_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "account")) {
		Svcmd_Account_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "session")) {
		Svcmd_Session_f();
		return qtrue;
	}

#ifdef DBCLEAN
	if (!Q_stricmp(cmd, "cleandb")) {
		Svcmd_CleanDB_f();
		return qtrue;
	}
#endif

#ifdef DBHOTSWAP
	if (!Q_stricmp(cmd, "hotswapdb")) {
		Svcmd_HotSwapDB_f();
		return qtrue;
	}
#endif

	if (!Q_stricmp(cmd, "notlive")) {
		Svcmd_NotLive_f();
		return qtrue;
	}

	//if (Q_stricmp (cmd, "accountlistall") == 0) {
	//	Svcmd_AccountPrintAll_f();
	//	return qtrue;
	//}	

	if (!Q_stricmp(cmd, "session")) {
		Svcmd_Session_f();
		return qtrue;
	}

	if (g_dedicated.integer) {
		if (Q_stricmp (cmd, "say") == 0) {
			trap_SendServerCommand( -1, va("print \"server: %s\n\"", ConcatArgs(1) ) );
			return qtrue;
		}
	}

	return qfalse;
}

void G_LogRconCommand(const char* ipFrom, const char* command) {
	// log it to disk if needed
	if (g_logrcon.integer) {
		G_RconLog("rcon from {%s}: %s", ipFrom, command);
	}
}

static char *GetUptime(void) {
	static char buf[256] = { 0 };
	time_t currTime;

	time(&currTime);

	int secs = difftime(currTime, trap_Cvar_VariableIntegerValue("sv_startTime"));
	int mins = secs / 60;
	int hours = mins / 60;
	int days = hours / 24;

	secs %= 60;
	mins %= 60;
	hours %= 24;
	//days %= 365;

	buf[0] = '\0';

	if (days)
		Com_sprintf(buf, sizeof(buf), "%id%ih%im%is", days, hours, mins, secs);
	if (hours)
		Com_sprintf(buf, sizeof(buf), "%ih%im%is", hours, mins, secs);
	else if (mins)
		Com_sprintf(buf, sizeof(buf), "%im%is", mins, secs);
	else
		Com_sprintf(buf, sizeof(buf), "%is", secs);

	return buf;
}

const char *GetGametypeString(int gametype) {
	switch (gametype) {
	case GT_FFA: return "Free For All";
	case GT_HOLOCRON: return "Holocron";
	case GT_JEDIMASTER: return "Jedi Master";
	case GT_DUEL: return "Duel";
	case GT_POWERDUEL: return "Power Duel";
	case GT_SINGLE_PLAYER: return "Cooperative";
	case GT_TEAM: return "Team Free For All";
	case GT_SIEGE: return "Siege";
	case GT_CTF: return "Capture The Flag";
	case GT_CTY: return "Capture The Ysalimiri";
	default: return "Unknown Gametype";
	}
}

extern char *ProperObjectiveName(const char *mapname, CombinedObjNumber obj);
void G_Status(void) {
	int humans = 0, bots = 0;
	for (int i = 0; i < level.maxclients; i++) {
		if (level.clients[i].pers.connected) {
			if (g_entities[i].r.svFlags & SVF_BOT)
				bots++;
			else
				humans++;
		}
	}

#if defined(_WIN32)
#define STATUS_OS "Windows"
#elif defined(__linux__)
#define STATUS_OS "Linux"
#elif defined(MACOS_X)
#define STATUS_OS "OSX"
#else
#define STATUS_OS "Unknown OS"
#endif

	char *dedicatedStr;
	switch (trap_Cvar_VariableIntegerValue("dedicated")) {
	case 1: dedicatedStr = "LAN dedicated"; break;
	case 2: dedicatedStr = "Public dedicated"; break;
	default: dedicatedStr = "Listen"; break;
	}

	int days = level.time / 86400000;
	int hours = (level.time % 86400000) / 3600000;
	int mins = (level.time % 3600000) / 60000;
	int secs = (level.time % 60000) / 1000;
	char matchTime[64] = { 0 };
	if (days)
		Com_sprintf(matchTime, sizeof(matchTime), "%i day%s, %i:%02i:%02i", days, days > 1 ? "s" : "", hours, mins, secs);
	else if (hours)
		Com_sprintf(matchTime, sizeof(matchTime), "%i:%02i:%02i", hours, mins, secs);
	else
		Com_sprintf(matchTime, sizeof(matchTime), "%i:%02i", mins, secs);

	Com_Printf("Hostname:   %s^7\n", Cvar_VariableString("sv_hostname"));
	Com_Printf("UDP/IP:     %s:%i, %s, %s\n", Cvar_VariableString("net_ip"), trap_Cvar_VariableIntegerValue("net_port"), STATUS_OS, dedicatedStr);
	Com_Printf("Uptime:     %s\n", GetUptime());
	Com_Printf("Map:        %s (%s)\n", Cvar_VariableString("mapname"), GetGametypeString(g_gametype.integer));
	Com_Printf("Players:    %i%s / %i%s max\n",
		humans, bots ? va(" human%s + %d bot%s", humans > 1 ? "s" : "", bots, bots > 1 ? "s" : "") : "", level.maxclients - sv_privateclients.integer, sv_privateclients.integer ? va("+%i", sv_privateclients.integer) : "");
	Com_Printf("Match time: %s\n", matchTime);
	if (g_gametype.integer == GT_TEAM || g_gametype.integer == GT_CTF) {
	Com_Printf("Score:      ^1%d^7 - ^4%d^7\n", level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]);
	}
	else if (g_gametype.integer == GT_SIEGE) {
		char siegeStr[1024] = { 0 }, *siegeStageStr;
		switch (level.siegeStage) {
		case SIEGESTAGE_PREROUND1: siegeStageStr = "Round 1 countdown"; break;
		case SIEGESTAGE_ROUND1: siegeStageStr = "Round 1"; break;
		case SIEGESTAGE_ROUND1POSTGAME: siegeStageStr = "Round 1 postgame"; break;
		case SIEGESTAGE_PREROUND2: siegeStageStr = "Round 2 countdown"; break;
		case SIEGESTAGE_ROUND2: siegeStageStr = "Round 2"; break;
		case SIEGESTAGE_ROUND2POSTGAME: siegeStageStr = "Round 2 postgame"; break;
		default: siegeStageStr = "";
		}
		int numRed = 0, numBlue = 0;
		GetPlayerCounts(qtrue, qtrue, &numRed, &numBlue, NULL, NULL);
		if (numRed && numBlue) {
			Com_sprintf(siegeStr, sizeof(siegeStr), "%s%s: %d vs %d%s",
				level.isLivePug == ISLIVEPUG_NO && level.siegeStage != SIEGESTAGE_PREROUND1 && level.siegeStage != SIEGESTAGE_PREROUND2 ? "(Not live) " : "",
				siegeStageStr,
				numRed,
				numBlue,
				(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) ? va(", %s", ProperObjectiveName(level.mapname, level.mapCaptureRecords.lastCombinedObjCompleted + 1)) : "");
	Com_Printf("Siege:      %s\n", siegeStr);
		}
	}
	if (g_cheats.integer)
	Com_Printf("^3Cheats enabled^7\n");
	Com_Printf("\n");

	if (humans + bots == 0) {
		Com_Printf("No players are currently connected.\n\n");
		return;
	}

	Table *t = Table_Initialize(qtrue);

	for (int i = 0; i < level.maxclients; i++) {
		if (!level.clients[i].pers.connected)
			continue;
		Table_DefineRow(t, &level.clients[i]);
	}

	Table_DefineColumn(t, "#", TableCallback_ClientNum, qfalse, 2);
	Table_DefineColumn(t, "Name", TableCallback_Name, qtrue, g_maxNameLength.integer);
	Table_DefineColumn(t, "Alias", TableCallback_Alias, qtrue, g_maxNameLength.integer);
	Table_DefineColumn(t, "Country", TableCallback_Country, qtrue, 64);
	Table_DefineColumn(t, "IP", TableCallback_IP, qtrue, 21);
	Table_DefineColumn(t, "Qport", TableCallback_Qport, qfalse, 5);
	Table_DefineColumn(t, "Mod", TableCallback_Mod, qtrue, 64);
	Table_DefineColumn(t, "Ping", TableCallback_Ping, qfalse, 4);
	Table_DefineColumn(t, "Score", TableCallback_Score, qfalse, 5);
	Table_DefineColumn(t, "Shadowmuted", TableCallback_Shadowmuted, qtrue, 64);
	Table_DefineColumn(t, "Boost", TableCallback_Boost, qfalse, 2);

	char buf[4096] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf));
	Table_Destroy(t);

	Q_strcat(buf, sizeof(buf), "^7\n");
	Com_Printf(buf);
}