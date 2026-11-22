#include "g_local.h"
#include "bg_saga.h"

/*
	multi spec: a spectator watches two players at once, one per half of the screen.

	the spectator stays a free spectator (following nobody), so its snapshot is built for its own
	client number and entities marked SVF_SINGLECLIENT for it reach nobody else. each frame its
	origin is put on the first half's viewpoint; a private portal entity standing at that same spot
	points at the second half's viewpoint, which makes the engine merge what is visible from there
	into the snapshot. the two private entities also carry what a player's own entity state lacks:
	health, armor, force, the weapon in hand, its ammo and the saber stance, and which players that viewpoint
	sees.
*/

#define MS_HALVES			2
#define MS_MAGIC			0x4d56		// trickedentindex of a view entity
#define MS_MAGIC_EXT		0x4d58		// ...of the entity with what only the player's own game knows (below)
#define MS_GAVE_MS			2000		// how long "gave team heal / energize" is shown

// teamowner of a view entity
#define MS_FLAG_HALF		0x01		// which half it describes
#define MS_FLAG_NOPLAYER	0x02		// the viewpoint is not a player
#define MS_FLAG_AUTO		0x04
#define MS_FLAG_GHOST		0x08		// the player is a siege ghost: otherEntityNum2 is the teammate it watches
#define MS_FLAG_FIXED		0x10		// ...a ghost with nobody to watch: the viewpoint is its own spot, angles its view
#define MS_FLAG_VEHLINK		0x20		// the viewpoint player's vehicle has its weapons linked

#define MS_FLAG_ANCHOR_UP	40.0f		// a flag viewpoint sits this far above the flag's origin

// a button steps its half on release, or once held this long. both pressed together inside that time
// is the client's signal to leave, and it says so before either step happens
#define MS_PRESS_MS			200
#define MS_PRECLICK_MS		500			// how long the follow target from before a click is remembered
// an ordinary spectator whose client starts multi spec with both buttons: a follow click waits this long
// (or for its release) so the first button of the pair steps nothing
#define MS_CHORD_MS			150
#define MS_CHORD_NMVER		"1.12.4"	// first client version with the both-buttons toggle
#define MS_RESUME_MS		60000		// how long players named by `multispec on <left> <right>` are waited for

typedef struct {
	qboolean	on;
	int			target[MS_HALVES];		// client number, -1 = none
	qboolean	autoFlag[MS_HALVES];	// ctf: the half tracks its side's attack on the enemy flag
	qboolean	onFlag[MS_HALVES];		// on auto with nobody carrying: the half shows the flag, not target
	gentity_t	*view[MS_HALVES];
	gentity_t	*ext[MS_HALVES];
	int			pressTime[MS_HALVES];	// level.time the half's button went down, 0 = nothing pending
	int			preClick, preClickTime;	// who was followed before the last follow click, and when that was
	int			followPress[MS_HALVES];	// not in multi spec: level.time a follow button went down, 0 = nothing pending
	qboolean	followChord;			// both follow buttons went down together: they step nothing
	int			pending[MS_HALVES];		// players a resume asked for that are not on a team yet, -1 = none
	int			pendingUntil;
} multiSpec_t;

static multiSpec_t ms[MAX_CLIENTS];
static int msGave[MAX_CLIENTS][2];		// level.time until which a client's team heal / team energize notice shows

// what auto shows per half, found once a frame for every spectator
static int		msAutoTime;
static int		msAutoCarrier[MS_HALVES];
static vec3_t	msAutoAnchor[MS_HALVES];

void G_MultiSpec_Init( void ) {
	memset( ms, 0, sizeof( ms ) );
	memset( msGave, 0, sizeof( msGave ) );
	msAutoTime = -1;
}

// a client just used team heal (0) or team energize (1) on somebody
void G_MultiSpec_NoteGave( int clientNum, int which ) {
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS && ( which == 0 || which == 1 ) ) {
		msGave[clientNum][which] = level.time + MS_GAVE_MS;
	}
}

static team_t MS_Side( int half ) {
	if ( g_gametype.integer < GT_TEAM ) {
		return TEAM_FREE;
	}
	return half == 0 ? TEAM_RED : TEAM_BLUE;
}

// a player either half can follow
static qboolean MS_OnTeam( int clientNum ) {
	const gclient_t *cl;

	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return qfalse;
	}
	cl = &level.clients[clientNum];
	return (qboolean)( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR );
}

// ...on the half's side, which only auto and the starting players go by
static qboolean MS_OnSide( int half, int clientNum ) {
	return (qboolean)( MS_OnTeam( clientNum )
		&& ( g_gametype.integer < GT_TEAM || level.clients[clientNum].sess.sessionTeam == MS_Side( half ) ) );
}

/*
	whose eyes a half looks through for its player `t`: the player, or the teammate a siege ghost (dead,
	waiting for the respawn wave) watches. -1 = a ghost with nobody to watch, seen from its own spot
*/
static int MS_Viewpoint( int t, qboolean *ghost ) {
	const int watched = G_MultiSpec_GhostWatching( &level.clients[t], ghost );

	return *ghost ? watched : t;
}

/*
	the next player for a half after `from` (-1 = the first, or the last going backwards): the half's side in
	client order, then the other team unless `sideOnly`. without teams never the other half's player.
	`wrapped` (may be NULL) is set when the step went past either end
*/
static int MS_NextTarget( multiSpec_t *m, int half, int from, int dir, qboolean sideOnly, qboolean *wrapped ) {
	int order[MAX_CLIENTS], count = 0, pos = -1, i, pass;

	if ( wrapped ) {
		*wrapped = qfalse;
	}
	for ( pass = 0; pass < ( sideOnly || g_gametype.integer < GT_TEAM ? 1 : 2 ); pass++ ) {
		for ( i = 0; i < level.maxclients; i++ ) {
			if ( !MS_OnTeam( i ) || MS_OnSide( half, i ) != ( pass == 0 ) ) {
				continue;
			}
			if ( g_gametype.integer < GT_TEAM && i == m->target[!half] ) {
				continue;
			}
			if ( i == from ) {
				pos = count;
			}
			order[count++] = i;
		}
	}
	if ( !count ) {
		return -1;
	}
	if ( pos < 0 ) {
		return order[dir > 0 ? 0 : count - 1];
	}
	pos += dir;
	if ( pos < 0 || pos >= count ) {
		if ( wrapped ) {
			*wrapped = qtrue;
		}
		pos = pos < 0 ? count - 1 : 0;
	}
	return order[pos];
}

/*
	ctf auto: the left half belongs to red, so it shows the red player carrying the blue flag, and
	when nobody carries it, the blue flag itself (where it was dropped, else its stand). the right
	half mirrors that. returns the carrier, or -1 with the flag viewpoint in `anchor`.
*/
static int MS_FindAutoTarget( int half, vec3_t anchor ) {
	const int	flagPowerup = half == 0 ? PW_BLUEFLAG : PW_REDFLAG;
	const char	*flagClass = half == 0 ? "team_CTF_blueflag" : "team_CTF_redflag";
	gentity_t	*e = NULL, *stand = NULL;
	int			i;

	for ( i = 0; i < level.maxclients; i++ ) {
		if ( MS_OnSide( half, i ) && level.clients[i].ps.powerups[flagPowerup]
			&& level.clients[i].ps.stats[STAT_HEALTH] > 0 ) {
			return i;
		}
	}
	while ( ( e = G_Find( e, FOFS( classname ), flagClass ) ) != NULL ) {
		if ( e->flags & FL_DROPPED_ITEM ) {
			VectorCopy( e->r.currentOrigin, anchor );
			anchor[2] += MS_FLAG_ANCHOR_UP;
			return -1;
		}
		stand = e;
	}
	if ( stand ) {
		VectorCopy( stand->r.currentOrigin, anchor );
		anchor[2] += MS_FLAG_ANCHOR_UP;
	}
	return -1;
}

static int MS_AutoTarget( int half, vec3_t anchor ) {
	if ( msAutoTime != level.time ) {
		int i;

		for ( i = 0; i < MS_HALVES; i++ ) {
			VectorClear( msAutoAnchor[i] );
			msAutoCarrier[i] = MS_FindAutoTarget( i, msAutoAnchor[i] );
		}
		msAutoTime = level.time;
	}
	VectorCopy( msAutoAnchor[half], anchor );
	return msAutoCarrier[half];
}

static qboolean MS_AutoAvailable( void ) {
	return (qboolean)( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY );
}

// multi spec is for a connected team spectator who follows nobody, while the match runs
static qboolean MS_Allowed( const gentity_t *ent ) {
	const gclient_t *cl = ent->client;

	return (qboolean)( g_multiSpec.integer && ent->inuse && cl && cl->pers.connected == CON_CONNECTED
		&& cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FREE
		&& !level.intermissiontime );
}

qboolean G_MultiSpec_Active( gentity_t *ent ) {
	int n = ent - g_entities;

	// a follow started by other means ends it at the next sweep; its controls apply from now
	return (qboolean)( n >= 0 && n < MAX_CLIENTS && ms[n].on && ent->client
		&& ent->client->sess.spectatorState == SPECTATOR_FREE );
}

static qboolean MS_ViewValid( const gentity_t *v ) {
	return (qboolean)( v && v->inuse && v->classname && !strcmp( v->classname, "multispec_view" ) );
}

// all four of a client's entities, the two view ones first
static gentity_t **MS_Ent( multiSpec_t *m, int i ) {
	return i < MS_HALVES ? &m->view[i] : &m->ext[i - MS_HALVES];
}

/*
	a client's entities (two view, two ext) are spawned once and kept while it stays a spectator, unlinked
	(sent to nobody) while multi spec is off: switching on and off must not use up entity slots
*/
static void MS_ParkViews( multiSpec_t *m ) {
	int i;

	for ( i = 0; i < MS_HALVES * 2; i++ ) {
		gentity_t **e = MS_Ent( m, i );

		if ( MS_ViewValid( *e ) ) {
			trap_UnlinkEntity( *e );
		} else {
			*e = NULL;
		}
	}
}

static void MS_FreeViews( multiSpec_t *m ) {
	int i;

	for ( i = 0; i < MS_HALVES * 2; i++ ) {
		gentity_t **e = MS_Ent( m, i );

		if ( MS_ViewValid( *e ) ) {
			G_FreeEntity( *e );
		}
		*e = NULL;
	}
}

void G_MultiSpec_Stop( gentity_t *ent, qboolean resumeFollow ) {
	multiSpec_t	*m = &ms[ent - g_entities];
	int			left = m->target[0];

	if ( !m->on ) {
		return;
	}
	m->on = qfalse;
	m->pressTime[0] = m->pressTime[1] = 0;
	MS_ParkViews( m );
	if ( resumeFollow && ent->client && ent->client->sess.sessionTeam == TEAM_SPECTATOR
		&& MS_OnTeam( left ) ) {
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		ent->client->sess.spectatorClient = left;
	}
}

// players named in a resume request go back to their halves, once they are on a team
static void MS_PlacePending( multiSpec_t *m ) {
	int i;

	for ( i = 0; i < MS_HALVES; i++ ) {
		const int c = m->pending[i];

		if ( c < 0 ) {
			continue;
		}
		if ( level.time > m->pendingUntil || c >= level.maxclients || level.clients[c].pers.connected != CON_CONNECTED ) {
			m->pending[i] = -1;
			continue;
		}
		if ( !MS_OnTeam( c ) || ( g_gametype.integer < GT_TEAM && c == m->target[!i] ) ) {
			continue;		// not on a team yet (a siege round's countdown)
		}
		m->pending[i] = -1;
		m->target[i] = c;
		m->autoFlag[i] = qfalse;
	}
}

static void MS_Start( gentity_t *ent, const int *resume ) {
	multiSpec_t	*m = &ms[ent - g_entities];
	gclient_t	*cl = ent->client;
	int			was = -1, i;

	if ( cl->pers.connected != CON_CONNECTED || level.intermissiontime ) {
		return;
	}
	if ( cl->sess.sessionTeam != TEAM_SPECTATOR || cl->tempSpectate >= level.time ) {
		trap_SendServerCommand( ent - g_entities, "print \"Multi spec is for spectators.\n\"" );
		return;
	}
	for ( i = 0; i < MS_HALVES * 2; i++ ) {
		gentity_t **e = MS_Ent( m, i );

		if ( !MS_ViewValid( *e ) ) {
			*e = NULL;
			if ( level.num_entities > ENTITYNUM_MAX_NORMAL - 8 ) {
				// an entity is needed and running out would end the server
				trap_SendServerCommand( ent - g_entities, "print \"Multi spec is not available right now.\n\"" );
				return;
			}
		}
	}
	if ( cl->sess.spectatorState == SPECTATOR_FOLLOW ) {
		was = cl->sess.spectatorClient;
		StopFollowing( ent );
	}
	if ( m->preClickTime && level.time - m->preClickTime <= MS_PRECLICK_MS ) {
		was = m->preClick;		// entered by pressing both buttons: the first one already stepped the follow
	}
	m->preClickTime = 0;
	m->pressTime[0] = m->pressTime[1] = 0;
	m->followPress[0] = m->followPress[1] = 0;
	m->followChord = qfalse;
	m->target[0] = m->target[1] = -1;
	m->autoFlag[0] = m->autoFlag[1] = MS_AutoAvailable();
	for ( i = 0; i < MS_HALVES; i++ ) {
		// the player being watched keeps their half; the other half starts on the other side
		m->target[i] = MS_OnSide( i, was ) && was != m->target[!i] ? was : MS_NextTarget( m, i, -1, 1, qtrue, NULL );
	}
	m->pending[0] = resume ? resume[0] : -1;
	m->pending[1] = resume ? resume[1] : -1;
	m->pendingUntil = level.time + MS_RESUME_MS;
	MS_PlacePending( m );
	/*
		the engine fills a snapshot in entity number order, adds what the second viewpoint sees when it
		reaches the portal entity, and drops whatever comes after a full snapshot. the portal is the
		highest of the four, so the other three are never the ones dropped
	*/
	{
		gentity_t **last = &m->view[1];

		for ( i = 0; i < MS_HALVES * 2; i++ ) {
			gentity_t **e = MS_Ent( m, i );

			if ( !*e ) {
				*e = G_Spawn();
			}
			if ( *e > *last ) {
				last = e;
			}
		}
		if ( last != &m->view[1] ) {
			gentity_t *swap[2];

			swap[0] = *last;
			swap[1] = m->view[1];
			*last = swap[1];
			m->view[1] = swap[0];
			for ( i = 0; i < 2; i++ ) {
				// nothing of the old role stays
				memset( &swap[i]->s, 0, sizeof( swap[i]->s ) );
				swap[i]->s.number = swap[i] - g_entities;
			}
		}
	}
	for ( i = 0; i < MS_HALVES; i++ ) {
		gentity_t *v = m->view[i];

		v->classname = "multispec_view";
		v->s.eType = ET_INVISIBLE;
		v->s.trickedentindex = MS_MAGIC;
		v->r.singleClient = ent - g_entities;
		// the first half's entity is simply always sent. the second must pass the real pvs test,
		// which is what makes the engine look through it
		v->r.svFlags = SVF_SINGLECLIENT | ( i == 0 ? SVF_BROADCAST : SVF_PORTAL );
		VectorSet( v->r.mins, -4, -4, -4 );
		VectorSet( v->r.maxs, 4, 4, 4 );
		m->view[i] = v;

		v = m->ext[i];
		v->classname = "multispec_view";
		v->s.eType = ET_INVISIBLE;
		v->s.trickedentindex = MS_MAGIC_EXT;
		v->s.teamowner = i ? MS_FLAG_HALF : 0;
		v->r.singleClient = ent - g_entities;
		v->r.svFlags = SVF_SINGLECLIENT | SVF_BROADCAST;
		VectorSet( v->r.mins, -4, -4, -4 );
		VectorSet( v->r.maxs, 4, 4, 4 );
		m->ext[i] = v;
	}
	m->on = qtrue;
}

void G_MultiSpec_Cycle( gentity_t *ent, int half, int dir ) {
	multiSpec_t	*m = &ms[ent - g_entities];
	int			next;
	qboolean	wrapped;

	if ( !m->on || half < 0 || half >= MS_HALVES ) {
		return;
	}
	if ( !MS_AutoAvailable() ) {
		next = MS_NextTarget( m, half, m->target[half], dir, qfalse, NULL );
		if ( next >= 0 ) {
			m->target[half] = next;
		}
		return;
	}
	// auto is one stop of the cycle, between the other team's last player and the side's first
	if ( m->autoFlag[half] ) {
		next = MS_NextTarget( m, half, -1, dir, qfalse, NULL );
		if ( next >= 0 ) {
			m->target[half] = next;
			m->autoFlag[half] = qfalse;
		}
		return;
	}
	next = MS_NextTarget( m, half, m->target[half], dir, qfalse, &wrapped );
	if ( next < 0 || wrapped ) {
		m->autoFlag[half] = qtrue;
	} else {
		m->target[half] = next;
	}
}

/*
	multispec on [<left> <right>]|off, multispec left|right [next|prev], multispec auto [left|right].
	on with two client numbers (-1 = none) resumes those players in the left and right halves
*/
void G_MultiSpec_Cmd( gentity_t *ent ) {
	char	arg[16], arg2[16];
	int		half = -1;

	if ( !g_multiSpec.integer || !ent->client ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	trap_Argv( 2, arg2, sizeof( arg2 ) );
	if ( !Q_stricmp( arg, "on" ) ) {
		multiSpec_t *m = &ms[ent - g_entities];

		if ( trap_Argc() >= 4 ) {
			char	arg3[16];
			int		resume[MS_HALVES];

			trap_Argv( 3, arg3, sizeof( arg3 ) );
			resume[0] = atoi( arg2 );
			resume[1] = atoi( arg3 );
			if ( !m->on ) {
				MS_Start( ent, resume );
			} else {
				// already on: the players asked for are still placed
				m->pending[0] = resume[0];
				m->pending[1] = resume[1];
				m->pendingUntil = level.time + MS_RESUME_MS;
			}
		} else if ( !m->on ) {
			MS_Start( ent, NULL );
		}
	} else if ( !Q_stricmp( arg, "off" ) ) {
		G_MultiSpec_Stop( ent, qtrue );
	} else if ( !Q_stricmp( arg, "auto" ) ) {
		multiSpec_t *m = &ms[ent - g_entities];

		if ( m->on && MS_AutoAvailable() ) {
			if ( Q_stricmp( arg2, "right" ) ) {
				m->autoFlag[0] = qtrue;
			}
			if ( Q_stricmp( arg2, "left" ) ) {
				m->autoFlag[1] = qtrue;
			}
		}
	} else if ( !Q_stricmp( arg, "left" ) ) {
		half = 0;
	} else if ( !Q_stricmp( arg, "right" ) ) {
		half = 1;
	}
	if ( half >= 0 ) {
		G_MultiSpec_Cycle( ent, half, Q_stricmp( arg2, "prev" ) ? 1 : -1 );
	}
}

// attack steps the left half to its next player, alt attack the right half
void G_MultiSpec_Buttons( gentity_t *ent, int buttons, int oldbuttons ) {
	static const int	bits[MS_HALVES] = { BUTTON_ATTACK, BUTTON_ALT_ATTACK };
	multiSpec_t			*m = &ms[ent - g_entities];
	int					i;

	for ( i = 0; i < MS_HALVES; i++ ) {
		if ( ( buttons & bits[i] ) && !( oldbuttons & bits[i] ) ) {
			m->pressTime[i] = level.time;
		} else if ( m->pressTime[i] && ( !( buttons & bits[i] ) || level.time - m->pressTime[i] >= MS_PRESS_MS ) ) {
			// a press older than a few frames was left over while the buttons were not being read (frozen)
			const qboolean stale = (qboolean)( level.time - m->pressTime[i] > 4 * MS_PRESS_MS );

			m->pressTime[i] = 0;
			if ( !stale ) {
				G_MultiSpec_Cycle( ent, i, 1 );
			}
		}
	}
}

// an ordinary spectator is about to step its follow with a button: remember where it was
void G_MultiSpec_NoteFollowClick( gentity_t *ent ) {
	multiSpec_t *m = &ms[ent - g_entities];

	if ( m->preClickTime && level.time - m->preClickTime <= MS_PRECLICK_MS ) {
		return;		// the second button of a pair
	}
	m->preClick = ent->client->sess.spectatorState == SPECTATOR_FOLLOW ? ent->client->sess.spectatorClient : -1;
	m->preClickTime = level.time;
}

// the client turns multi spec on and off with both attack buttons
static qboolean MS_UsesChord( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	return (qboolean)( g_multiSpec.integer && cl && cl->sess.sessionTeam == TEAM_SPECTATOR
		&& CompareVersions( cl->sess.nmVer, MS_CHORD_NMVER ) >= 0 );
}

/*
an ordinary spectator's follow clicks, for a client that uses both buttons to start multi spec: a click
steps the follow on release or once held MS_CHORD_MS, and both pressed together step nothing.
returns the buttons whose stock follow handling is skipped
*/
int G_MultiSpec_FollowButtons( gentity_t *ent, int buttons, int oldbuttons ) {
	static const int	bits[MS_HALVES] = { BUTTON_ATTACK, BUTTON_ALT_ATTACK };
	multiSpec_t			*m = &ms[ent - g_entities];
	int					i;

	if ( !MS_UsesChord( ent ) ) {
		m->followPress[0] = m->followPress[1] = 0;
		m->followChord = qfalse;
		return 0;
	}
	for ( i = 0; i < MS_HALVES; i++ ) {
		if ( ( buttons & bits[i] ) && !( oldbuttons & bits[i] ) ) {
			m->followPress[i] = level.time ? level.time : 1;
		}
	}
	if ( m->followPress[0] && m->followPress[1] ) {
		m->followChord = qtrue;
	}
	for ( i = 0; i < MS_HALVES; i++ ) {
		if ( m->followPress[i] && ( !( buttons & bits[i] ) || level.time - m->followPress[i] >= MS_CHORD_MS ) ) {
			// a press older than a few frames was left over while the buttons were not being read
			const qboolean stale = (qboolean)( level.time - m->followPress[i] > 4 * MS_CHORD_MS );

			m->followPress[i] = 0;
			if ( !m->followChord && !stale ) {
				if ( i == 0 ) {
					Cmd_FollowCycle_f( ent, 1 );
				} else if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
					Cmd_FollowCycle_f( ent, -1 );
				}
			}
		}
	}
	if ( !m->followPress[0] && !m->followPress[1] ) {
		m->followChord = qfalse;
	}
	return BUTTON_ATTACK | BUTTON_ALT_ATTACK;
}

// which players a client standing at this viewpoint as `viewer` would be sent, the way the engine decides it
static int MS_VisibleMask( const vec3_t eye, int viewer ) {
	int i, mask = 0;

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *e = &g_entities[i];

		if ( !e->inuse || !e->client ) {
			continue;
		}
		if ( i == viewer ) {
			mask |= 1 << i;
			continue;
		}
		if ( !e->r.linked || ( e->r.svFlags & SVF_NOCLIENT ) ) {
			continue;
		}
		if ( viewer >= 0 && ( e->r.broadcastClients[1] & ( 1 << viewer ) ) ) {
			continue;
		}
		if ( ( e->r.svFlags & SVF_BROADCAST ) || ( viewer >= 0 && ( e->r.broadcastClients[0] & ( 1 << viewer ) ) )
			|| trap_InPVS( eye, e->r.currentOrigin ) ) {
			mask |= 1 << i;
		}
	}
	return mask;
}

/*
	the vehicle the viewpoint player is in: hull + 1 (0 = none), shields, both weapons' ammo (the second in
	time2's high half). returns the view flags it adds
*/
static int MS_SetVehicle( gentity_t *v, const gclient_t *tc ) {
	gentity_t *veh;

	v->s.trickedentindex2 = v->s.trickedentindex3 = v->s.trickedentindex4 = 0;
	if ( !tc || tc->ps.m_iVehicleNum <= 0 || tc->ps.m_iVehicleNum >= ENTITYNUM_MAX_NORMAL ) {
		return 0;
	}
	veh = &g_entities[tc->ps.m_iVehicleNum];
	if ( !veh->inuse || !veh->client || !veh->m_pVehicle || !veh->m_pVehicle->m_pVehicleInfo ) {
		return 0;
	}
	v->s.trickedentindex2 = Com_Clampi( 0, 0xfffe, veh->client->ps.stats[STAT_HEALTH] ) + 1;
	v->s.trickedentindex3 = Com_Clampi( 0, 0xffff, veh->client->ps.stats[STAT_ARMOR] );
	v->s.trickedentindex4 = Com_Clampi( 0, 0xffff, veh->client->ps.ammo[0] );
	v->s.time2 = (int)( ( (unsigned)v->s.time2 & 0xffffu ) | ( (unsigned)Com_Clampi( 0, 0xffff, veh->client->ps.ammo[1] ) << 16 ) );
	return veh->client->ps.vehWeaponsLinked ? MS_FLAG_VEHLINK : 0;
}

/*
	the ext entity: what only the viewpoint player's own game is told, for its half's hud. tc NULL = the
	viewpoint is not a player (everything zero, MS_FLAG_NOPLAYER).

	| field | content |
	| time, time2, emplacedOwner | level.time speed, protect, absorb end (0 = not running) |
	| surfacesOn, surfacesOff, apos.trTime | ...rage, sense, mind trick end |
	| apos.trDuration | level.time rage recovery ends |
	| genericenemyindex | force level of speed, rage, sense, mind trick, two bits each from bit 0; bits 8-11 the rocket lock's progress (the four holocron bits above the force powers) |
	| forcePowersActive, pos.trTime | level.time the "gave team heal" / "gave team energize" notice ends |
	| bolt2, pos.trDuration | rocket lock target (ENTITYNUM_NONE = none) and the level.time it began |
	| boneAngles1[0] | ms spent falling to death, 1-4000 (0 = not falling) |
	| boneAngles2[0..2], boneAngles3[0] | seconds left of force enlightenment light, dark, force boon, ysalamiri (0 = none) |
	| health, maxhealth | weapons owned, bits 0-9 and 10-19 |
	| boneAngles4[0..2], angles2[0..2] | medal counts: impressive, excellent, gauntlet, defend, assist, captures |
	| boneAngles1[1..2], boneAngles3[1..2], origin2[0..2] | ammo: blaster, power cell, metal bolts, rockets, thermals, trip mines, det packs |
*/
static void MS_UpdateExt( multiSpec_t *m, int half, const vec3_t specEye, const gclient_t *tc ) {
	static const int	fp[6] = { FP_SPEED, FP_PROTECT, FP_ABSORB, FP_RAGE, FP_SEE, FP_TELEPATHY };
	static const int	pw[4] = { PW_FORCE_ENLIGHTENED_LIGHT, PW_FORCE_ENLIGHTENED_DARK, PW_FORCE_BOON, PW_YSALAMIRI };
	gentity_t			*v = m->ext[half];
	int					end[6] = { 0 }, left[4] = { 0 }, i;
	int					n;

	if ( !MS_ViewValid( v ) ) {
		return;
	}
	v->s.teamowner = ( half ? MS_FLAG_HALF : 0 ) | ( tc ? 0 : MS_FLAG_NOPLAYER );
	if ( tc ) {
		n = tc - level.clients;
		for ( i = 0; i < 6; i++ ) {
			if ( ( tc->ps.fd.forcePowersActive & ( 1 << fp[i] ) ) && tc->ps.fd.forcePowerDuration[fp[i]] > level.time ) {
				end[i] = tc->ps.fd.forcePowerDuration[fp[i]];
			}
		}
		for ( i = 0; i < 4; i++ ) {
			if ( tc->ps.powerups[pw[i]] > level.time ) {
				left[i] = Com_Clampi( 1, 999, ( tc->ps.powerups[pw[i]] - level.time ) / 1000 + 1 );
			}
		}
	}
	v->s.time = end[0];
	v->s.time2 = end[1];
	v->s.emplacedOwner = end[2];
	v->s.surfacesOn = end[3];
	v->s.surfacesOff = end[4];
	v->s.apos.trTime = end[5];
	v->s.apos.trDuration = ( tc && tc->ps.fd.forceRageRecoveryTime > level.time ) ? tc->ps.fd.forceRageRecoveryTime : 0;
	v->s.genericenemyindex = !tc ? 0 : ( tc->ps.fd.forcePowerLevel[FP_SPEED] & 3 ) | ( ( tc->ps.fd.forcePowerLevel[FP_RAGE] & 3 ) << 2 )
		| ( ( tc->ps.fd.forcePowerLevel[FP_SEE] & 3 ) << 4 ) | ( ( tc->ps.fd.forcePowerLevel[FP_TELEPATHY] & 3 ) << 6 )
		| ( ( ( tc->ps.holocronBits >> NUM_FORCE_POWERS ) & 15 ) << 8 );
	v->s.forcePowersActive = ( tc && msGave[n][0] > level.time ) ? msGave[n][0] : 0;
	v->s.bolt2 = ( tc && tc->ps.rocketLockIndex >= 0 && tc->ps.rocketLockIndex < ENTITYNUM_NONE ) ? tc->ps.rocketLockIndex : ENTITYNUM_NONE;
	v->s.boneAngles1[0] = ( tc && tc->ps.fallingToDeath ) ? Com_Clampi( 1, 4000, level.time - tc->ps.fallingToDeath ) : 0;
	v->s.boneAngles2[0] = left[0];
	v->s.boneAngles2[1] = left[1];
	v->s.boneAngles2[2] = left[2];
	v->s.boneAngles3[0] = left[3];
	v->s.health = tc ? ( tc->ps.stats[STAT_WEAPONS] & 1023 ) : 0;
	v->s.maxhealth = tc ? ( ( tc->ps.stats[STAT_WEAPONS] >> 10 ) & 1023 ) : 0;
	v->s.boneAngles4[0] = tc ? Com_Clampi( 0, 4000, tc->ps.persistant[PERS_IMPRESSIVE_COUNT] ) : 0;
	v->s.boneAngles4[1] = tc ? Com_Clampi( 0, 4000, tc->ps.persistant[PERS_EXCELLENT_COUNT] ) : 0;
	v->s.boneAngles4[2] = tc ? Com_Clampi( 0, 4000, tc->ps.persistant[PERS_GAUNTLET_FRAG_COUNT] ) : 0;
	v->s.angles2[0] = tc ? Com_Clampi( 0, 4000, tc->ps.persistant[PERS_DEFEND_COUNT] ) : 0;
	v->s.angles2[1] = tc ? Com_Clampi( 0, 4000, tc->ps.persistant[PERS_ASSIST_COUNT] ) : 0;
	v->s.angles2[2] = tc ? Com_Clampi( 0, 4000, tc->ps.persistant[PERS_CAPTURES] ) : 0;
	// ammo, for the weapon bar
	v->s.boneAngles1[1] = tc ? Com_Clampi( 0, 4000, tc->ps.ammo[AMMO_BLASTER] ) : 0;
	v->s.boneAngles1[2] = tc ? Com_Clampi( 0, 4000, tc->ps.ammo[AMMO_POWERCELL] ) : 0;
	v->s.boneAngles3[1] = tc ? Com_Clampi( 0, 4000, tc->ps.ammo[AMMO_METAL_BOLTS] ) : 0;
	v->s.boneAngles3[2] = tc ? Com_Clampi( 0, 4000, tc->ps.ammo[AMMO_ROCKETS] ) : 0;
	v->s.origin2[0] = tc ? Com_Clampi( 0, 4000, tc->ps.ammo[AMMO_THERMAL] ) : 0;
	v->s.origin2[1] = tc ? Com_Clampi( 0, 4000, tc->ps.ammo[AMMO_TRIPMINE] ) : 0;
	v->s.origin2[2] = tc ? Com_Clampi( 0, 4000, tc->ps.ammo[AMMO_DETPACK] ) : 0;
	G_SetOrigin( v, (float *)specEye );
	// after G_SetOrigin, which clears the trajectory's times
	v->s.pos.trTime = ( tc && msGave[n][1] > level.time ) ? msGave[n][1] : 0;
	v->s.pos.trDuration = ( tc && v->s.bolt2 != ENTITYNUM_NONE ) ? tc->ps.rocketLockTime : 0;
	trap_LinkEntity( v );
}

static void MS_UpdateView( gentity_t *spec, multiSpec_t *m, int half, const vec3_t specEye, const vec3_t flagAnchor ) {
	gentity_t	*v = m->view[half];
	int			t = m->target[half];
	vec3_t		eye;
	int			flags = ( half ? MS_FLAG_HALF : 0 ) | ( m->autoFlag[half] ? MS_FLAG_AUTO : 0 );
	int			vp = -1;
	qboolean	ghost = qfalse;

	if ( !MS_ViewValid( v ) ) {
		return;
	}
	v->s.otherEntityNum2 = ENTITYNUM_NONE;
	VectorClear( v->s.angles );
	v->s.weapon = 0;

	if ( flagAnchor ) {
		// auto, and nobody carries the flag: the viewpoint is the flag
		VectorCopy( flagAnchor, eye );
		flags |= MS_FLAG_NOPLAYER;
		v->s.otherEntityNum = ENTITYNUM_NONE;
		v->s.health = v->s.maxhealth = 0;
		v->s.time = 0;
		v->s.time2 = 0xffff;
		v->s.genericenemyindex = MS_VisibleMask( eye, -1 );
		MS_SetVehicle( v, NULL );
	} else if ( MS_OnTeam( t ) && ( vp = MS_Viewpoint( t, &ghost ) ) < 0 ) {
		// a ghost with nobody alive to watch: what the ghost itself sees
		const gclient_t *gc = &level.clients[t];

		VectorCopy( gc->ps.origin, eye );
		eye[2] += gc->ps.viewheight;
		VectorCopy( gc->ps.viewangles, v->s.angles );
		flags |= MS_FLAG_GHOST | MS_FLAG_FIXED;
		v->s.otherEntityNum = t;
		v->s.health = v->s.maxhealth = 0;
		v->s.time = 0;
		v->s.time2 = 0xffff;
		v->s.genericenemyindex = MS_VisibleMask( eye, -1 );
		MS_SetVehicle( v, NULL );
	} else if ( MS_OnTeam( t ) ) {
		// the half's player, or for a ghost the teammate it watches: eyes and vitals are theirs
		gclient_t	*tc = &level.clients[vp];
		int			maxHealth = tc->ps.stats[STAT_MAX_HEALTH], maxArmor = 100;
		int			force = tc->ps.fd.forcePowersKnown ? tc->ps.fd.forcePower : 255;
		int			weapon = tc->ps.weapon;

		if ( g_gametype.integer == GT_SIEGE && tc->siegeClass != -1 ) {
			maxHealth = bgSiegeClasses[tc->siegeClass].maxhealth;
			maxArmor = bgSiegeClasses[tc->siegeClass].maxarmor;
		}
		VectorCopy( tc->ps.origin, eye );
		eye[2] += tc->ps.viewheight;
		if ( ghost ) {
			flags |= MS_FLAG_GHOST;
			v->s.otherEntityNum2 = vp;
		}
		v->s.otherEntityNum = t;
		v->s.health = Com_Clampi( 0, 1023, tc->ps.stats[STAT_HEALTH] );
		v->s.maxhealth = Com_Clampi( 0, 1023, maxHealth );
		v->s.time = Com_Clampi( 0, 1023, tc->ps.stats[STAT_ARMOR] ) | ( Com_Clampi( 0, 1023, maxArmor ) << 10 )
			| ( Com_Clampi( 0, 255, force ) << 20 );
		v->s.time2 = ( weapon > WP_NONE && weapon < WP_NUM_WEAPONS )
			? Com_Clampi( 0, 0xfffe, tc->ps.ammo[weaponData[weapon].ammoIndex] ) : 0xffff;
		// a pilot hidden inside its vehicle sends no entity of its own to read these from:
		// the weapon in the low five bits, the saber stance above them
		v->s.weapon = ( weapon & 31 ) | ( ( tc->ps.fd.saberDrawAnimLevel & 7 ) << 5 );
		v->s.genericenemyindex = MS_VisibleMask( eye, vp );
		flags |= MS_SetVehicle( v, tc );
	} else {
		// nobody to show on this side: look from where the spectator already is
		VectorCopy( specEye, eye );
		flags |= MS_FLAG_NOPLAYER;
		v->s.otherEntityNum = ENTITYNUM_NONE;
		v->s.health = v->s.maxhealth = 0;
		v->s.time = 0;
		v->s.time2 = 0xffff;
		v->s.genericenemyindex = MS_VisibleMask( eye, -1 );
		MS_SetVehicle( v, NULL );
	}
	v->s.teamowner = flags;
	VectorCopy( eye, v->s.origin2 );
	G_SetOrigin( v, (float *)specEye );
	trap_LinkEntity( v );
	MS_UpdateExt( m, half, specEye, ( flags & MS_FLAG_NOPLAYER ) || vp < 0 ? NULL : &level.clients[vp] );
}

// from SpectatorClientEndFrame, after every player has moved this frame
void G_MultiSpec_EndFrame( gentity_t *ent ) {
	multiSpec_t	*m = &ms[ent - g_entities];
	gclient_t	*cl = ent->client;
	vec3_t		specEye;
	int			i;

	if ( !m->on ) {
		return;
	}
	if ( !MS_Allowed( ent ) ) {
		G_MultiSpec_Stop( ent, qfalse );	// joined a team, followed someone by other means, match over
		return;
	}
	vec3_t		anchor[MS_HALVES];
	qboolean	onFlag[MS_HALVES] = { qfalse, qfalse };

	if ( !MS_AutoAvailable() ) {
		m->autoFlag[0] = m->autoFlag[1] = qfalse;
	}
	MS_PlacePending( m );
	for ( i = 0; i < MS_HALVES; i++ ) {
		if ( m->autoFlag[i] ) {
			int carrier;

			VectorClear( anchor[i] );
			carrier = MS_AutoTarget( i, anchor[i] );
			if ( carrier >= 0 ) {
				m->target[i] = carrier;
			} else {
				onFlag[i] = qtrue;
			}
		}
		m->onFlag[i] = onFlag[i];
		if ( !MS_OnTeam( m->target[i] ) ) {
			m->target[i] = MS_NextTarget( m, i, -1, 1, qfalse, NULL );
		}
	}

	// ride the first half's viewpoint, eye for eye, so its visibility is the spectator's own
	if ( onFlag[0] ) {
		VectorCopy( anchor[0], cl->ps.origin );
		cl->ps.viewheight = 0;
		VectorClear( cl->ps.velocity );
		VectorCopy( cl->ps.origin, ent->s.origin );
	} else if ( MS_OnTeam( m->target[0] ) ) {
		qboolean	ghost;
		const int	vp = MS_Viewpoint( m->target[0], &ghost );
		gclient_t	*tc = &level.clients[vp >= 0 ? vp : m->target[0]];

		VectorCopy( tc->ps.origin, cl->ps.origin );
		cl->ps.viewheight = tc->ps.viewheight;
		VectorClear( cl->ps.velocity );
		VectorCopy( cl->ps.origin, ent->s.origin );
	}
	VectorCopy( cl->ps.origin, specEye );
	specEye[2] += cl->ps.viewheight;

	for ( i = 0; i < MS_HALVES; i++ ) {
		MS_UpdateView( ent, m, i, specEye, onFlag[i] ? anchor[i] : NULL );
	}
}

void G_MultiSpec_ClientGone( int clientNum ) {
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
		MS_FreeViews( &ms[clientNum] );
		memset( &ms[clientNum], 0, sizeof( ms[clientNum] ) );
		msGave[clientNum][0] = msGave[clientNum][1] = 0;
	}
}

/*
	once a frame: ends multi spec for anybody it is no longer for (a team join, a follow, a drop).
	somebody who is no longer a spectator gives its entities back
*/
void G_MultiSpec_Sweep( void ) {
	int i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gentity_t	*ent = &g_entities[i];
		multiSpec_t	*m = &ms[i];

		if ( m->on && !MS_Allowed( ent ) ) {
			G_MultiSpec_Stop( ent, qfalse );
		}
		if ( !m->on && ( m->view[0] || m->view[1] || m->ext[0] || m->ext[1] )
			&& ( !ent->client || ent->client->pers.connected != CON_CONNECTED
				|| ent->client->sess.sessionTeam != TEAM_SPECTATOR ) ) {
			MS_FreeViews( m );
		}
	}
}

/*
	what lists that show who a spectator watches say about a multi spec spectator: the players its halves
	show, left half first (`shown`, returns how many, 0-2; a half on auto watching a flag shows nobody),
	and whether any half is on auto
*/
int G_MultiSpec_ListState( int clientNum, int shown[MS_HALVES], qboolean *anyAuto ) {
	multiSpec_t	*m;
	int			i, n = 0;

	*anyAuto = qfalse;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !ms[clientNum].on ) {
		return 0;
	}
	m = &ms[clientNum];
	for ( i = 0; i < MS_HALVES; i++ ) {
		if ( m->autoFlag[i] ) {
			*anyAuto = qtrue;
			if ( m->onFlag[i] ) {
				continue;
			}
		}
		if ( MS_OnTeam( m->target[i] ) ) {
			shown[n++] = m->target[i];
		}
	}
	return n;
}
