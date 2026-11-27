// batched spec info: what a demo viewer cycling to any player needs, sent a few times a minute instead of every tick

#include "g_local.h"

#ifdef NEWMOD_SUPPORT

#include "crypto.h"

/*
	Two streams, flushed together:
	- encb: to every player, encrypted with the round's spec info key (revealed at intermission): per-player
	  samples of the spec info token every overlay tick, plus spans and moments.
	- spb: to true spectators, plain: spans and moments only (they get the samples live as snf2).

	A message is one binary buffer, CRC-checked inside (so noise from a wrong key is dropped whole):
		u8 magic, u8 version, u16 length (whole buffer incl. crc), varint base time, records..., u32 crc
	records:
		SB_REC_SAMPLE	u8 client, zigzag (time - base), u8 keys, keys... (see SB_EncodeToken)
		SB_REC_SPAN		u8 kind, u8 client, zigzag start, zigzag end, zigzag due, varint total, varint value, u8 closed
		SB_REC_MOMENT	u8 kind, u8 client, zigzag time; a fall adds zigzag x, y, z (where it began, rounded)
	Times are level-relative (level.time - level.startTime), as fpt's, so a pause doesn't move them.

	A span is sent when it opens and again when it closes: a demo that ends mid-span has the first, one that
	starts mid-span the second. due is when it was scheduled to end (what a countdown counts to), end when
	it actually did (the scheduled end until it closes; 0 = open-ended). The client keys them on (kind,
	client, start) and the later record wins.
*/

#define SB_PROTOCOL			3
#define SB_MAGIC			0xB5
#define SB_VERSION			3
#define SB_MAX_BYTES		832		// 13 key stream blocks; packed it still fits a server command
#define SB_FLUSH_MS			30000
#define SB_NONCE			0x5342	// not a team number, so never the legacy encs key stream

#define SB_REC_SAMPLE		1
#define SB_REC_SPAN			2
#define SB_REC_MOMENT		3

// span kinds: force powers by number (FP_SABER_OFFENSE = rage recovery, as fpt numbers it), then these
#define SB_SPAN_POWERUP		32		// + powerup number
#define SB_SPAN_ROCKETLOCK	64		// value = locked entity

#define SB_MOMENT_GAVE_TH	0
#define SB_MOMENT_GAVE_TE	1
#define SB_MOMENT_FALL		2

static const int sbPowerups[] = { PW_FORCE_BOON, PW_YSALAMIRI, PW_FORCE_ENLIGHTENED_LIGHT, PW_FORCE_ENLIGHTENED_DARK };
#define SB_NUM_POWERUPS		( (int)( sizeof( sbPowerups ) / sizeof( sbPowerups[0] ) ) )

// slots per client: force powers, powerups, rocket lock
#define SB_SLOT_POWERUP		NUM_FORCE_POWERS
#define SB_SLOT_LOCK		( SB_SLOT_POWERUP + SB_NUM_POWERUPS )
#define SB_NUM_SLOTS		( SB_SLOT_LOCK + 1 )

typedef struct {
	qboolean	open;
	int			start, end, due, total, value;
	int			seenAbs, seenRel;	// the server value it was opened from (powerup end, lock start), as is and level-relative
} sbSpan_t;

/*
	whether a server timestamp is a new one. a pause pushes some timestamps along with level.startTime every
	frame (then the level-relative value stays) and leaves others alone (then the absolute one stays), and
	which ones differs between the mods: only a change in both is real
*/
static qboolean SB_NewStamp( const sbSpan_t *s, int stamp ) {
	return (qboolean)( !s->open || ( stamp != s->seenAbs && stamp - level.startTime != s->seenRel ) );
}

typedef struct {
	unsigned char	buf[SB_MAX_BYTES];
	int				len;	// 0 = empty, no header yet
	int				base;
} sbBuf_t;

static sbSpan_t	sbSpans[MAX_CLIENTS][SB_NUM_SLOTS];
static sbSpan_t	sbFall[MAX_CLIENTS];		// only its stamp is used
static sbBuf_t	sbEnc, sbPlain;
static int		sbSeq;
static int		sbFirstRecordTime;	// level.time the oldest unsent record was added, 0 = nothing pending
static int		sbSide[MAX_CLIENTS];		// SB_SIDE_*: which stream a client gets, as of the last frame
static qboolean	sbKeyOut;					// enck went out: nothing more under this key
#define SB_SIDE_NONE	0
#define SB_SIDE_PLAYER	1
#define SB_SIDE_SPEC	2

static int SB_Now( void ) {
	return level.time - level.startTime;
}

static qboolean SB_EncOn( void ) {
	return (qboolean)( g_specInfo.integer && g_specInfoEncrypted.integer && level.specInfoKeyValid && !sbKeyOut );
}

static qboolean SB_PlainOn( void ) {
	return (qboolean)( g_specInfo.integer != 0 );
}

void G_SpecBatch_Init( void ) {
	memset( sbSpans, 0, sizeof( sbSpans ) );
	memset( sbFall, 0, sizeof( sbFall ) );
	memset( &sbEnc, 0, sizeof( sbEnc ) );
	memset( &sbPlain, 0, sizeof( sbPlain ) );
	sbSeq = 0;
	sbFirstRecordTime = 0;
	memset( sbSide, 0, sizeof( sbSide ) );
	sbKeyOut = qfalse;
}

static unsigned int SB_Crc32( const unsigned char *p, int len ) {
	unsigned int crc = 0xFFFFFFFFu;
	int i, k;

	for ( i = 0; i < len; i++ ) {
		crc ^= p[i];
		for ( k = 0; k < 8; k++ ) {
			crc = ( crc >> 1 ) ^ ( 0xEDB88320u & ( 0u - ( crc & 1u ) ) );
		}
	}
	return ~crc;
}

// a record being built: bytes plus an overflow flag
typedef struct {
	unsigned char	b[SB_MAX_BYTES];
	int				len;
	qboolean		overflow;
} sbRec_t;

static void SB_Byte( sbRec_t *r, int v ) {
	if ( r->len >= (int)sizeof( r->b ) ) {
		r->overflow = qtrue;
		return;
	}
	r->b[r->len++] = (unsigned char)( v & 0xFF );
}

static void SB_Varint( sbRec_t *r, unsigned int v ) {
	do {
		SB_Byte( r, (int)( ( v & 0x7F ) | ( v > 0x7F ? 0x80 : 0 ) ) );
		v >>= 7;
	} while ( v );
}

static unsigned int SB_ZigZag( int v ) {
	return ( (unsigned int)v << 1 ) ^ (unsigned int)( v >> 31 );
}

/*
	The spec info token in binary: the keys the client's token parser knows, in this order. dec keys are one
	signed number, hex keys a comma list of non-negative numbers with empty slots, fl four raw characters.
	The client turns it back into the same text and parses it as a token.
*/
typedef enum { SBK_DEC, SBK_HEX, SBK_STR4 } sbKeyType_t;
static const struct { const char *name; sbKeyType_t type; } sbKeys[] = {
	{ "h", SBK_DEC }, { "a", SBK_DEC }, { "f", SBK_DEC }, { "l", SBK_DEC }, { "mh", SBK_DEC }, { "ma", SBK_DEC },
	{ "p", SBK_DEC }, { "fl", SBK_STR4 }, { "w", SBK_HEX }, { "s", SBK_HEX }, { "t", SBK_HEX }, { "r", SBK_HEX },
	{ "d", SBK_HEX }, { "n", SBK_HEX }, { "y", SBK_HEX }, { "ot", SBK_HEX }, { "k", SBK_HEX },
};
#define SB_NUM_KEYS ( (int)( sizeof( sbKeys ) / sizeof( sbKeys[0] ) ) )
#define SB_MAX_LIST	16

// token: ` "<client> key=value ..."` as BuildSpecInfoPlayerToken writes it. qfalse if it isn't one
static qboolean SB_EncodeToken( sbRec_t *r, const char *token, int *clientOut ) {
	const char	*p = token;
	int			countAt, count = 0, k;

	while ( *p == ' ' || *p == '"' ) {
		p++;
	}
	if ( *p < '0' || *p > '9' ) {
		return qfalse;
	}
	*clientOut = atoi( p );
	while ( *p >= '0' && *p <= '9' ) {
		p++;
	}
	countAt = r->len;
	SB_Byte( r, 0 );

	while ( *p ) {
		char		name[8];
		int			n = 0;
		const char	*value;

		while ( *p == ' ' ) {
			p++;
		}
		if ( !*p || *p == '"' ) {
			break;
		}
		while ( *p && *p != '=' && *p != ' ' && n < (int)sizeof( name ) - 1 ) {
			name[n++] = *p++;
		}
		name[n] = '\0';
		if ( *p != '=' ) {
			return qfalse;
		}
		value = ++p;
		while ( *p && *p != ' ' && *p != '"' ) {
			p++;
		}

		for ( k = 0; k < SB_NUM_KEYS; k++ ) {
			if ( !strcmp( sbKeys[k].name, name ) ) {
				break;
			}
		}
		if ( k == SB_NUM_KEYS ) {
			assert( qfalse );	// a key the codec doesn't know: add it here and in the client
			continue;
		}
		SB_Byte( r, k );
		count++;
		if ( sbKeys[k].type == SBK_DEC ) {
			SB_Varint( r, SB_ZigZag( atoi( value ) ) );
		} else if ( sbKeys[k].type == SBK_STR4 ) {
			int c;
			for ( c = 0; c < 4; c++ ) {
				SB_Byte( r, value + c < p ? (unsigned char)value[c] : 0 );
			}
		} else {
			unsigned int	items[SB_MAX_LIST];
			int				numItems = 0, i;
			const char		*q = value;

			// value+1 per slot, 0 = empty
			while ( q <= p && numItems < SB_MAX_LIST ) {
				const char *e = q;
				while ( e < p && *e != ',' ) {
					e++;
				}
				items[numItems++] = e > q ? (unsigned int)strtoul( q, NULL, 16 ) + 1 : 0;
				if ( e >= p ) {
					break;
				}
				q = e + 1;
			}
			SB_Byte( r, numItems );
			for ( i = 0; i < numItems; i++ ) {
				SB_Varint( r, items[i] );
			}
		}
	}
	if ( r->overflow || count > 255 ) {
		return qfalse;
	}
	r->b[countAt] = (unsigned char)count;
	return qtrue;
}

static void SB_SendBuf( sbBuf_t *sb, qboolean encrypted ) {
	unsigned char	cipher[SB_MAX_BYTES];
	char			packed[Crypto_PackedSizeForBin( SB_MAX_BYTES ) + 1];
	char			cmd[MAX_STRING_CHARS];
	unsigned int	crc;
	int				i, seq = sbSeq;

	if ( sb->len <= 0 ) {
		return;
	}
	if ( sb->len + 4 > SB_MAX_BYTES ) {
		assert( qfalse );
		sb->len = 0;
		return;
	}
	sb->buf[2] = (unsigned char)( ( sb->len + 4 ) & 0xFF );
	sb->buf[3] = (unsigned char)( ( sb->len + 4 ) >> 8 );
	crc = SB_Crc32( sb->buf, sb->len );
	for ( i = 0; i < 4; i++ ) {
		sb->buf[sb->len++] = (unsigned char)( ( crc >> ( i * 8 ) ) & 0xFF );
	}

	if ( encrypted ) {
		if ( Crypto_StreamXor( &level.specInfoKey, (uint64_t)SB_NONCE,
			(uint64_t)seq * ( SB_MAX_BYTES / CRYPTO_STREAM_BLOCK_SIZE ), sb->buf, cipher, (size_t)sb->len ) == CRYPTO_ERROR ) {
			sb->len = 0;
			return;
		}
	} else {
		memcpy( cipher, sb->buf, sb->len );
	}
	if ( !Crypto_Pack7Bit( cipher, (size_t)sb->len, packed, sizeof( packed ) ) ) {
		sb->len = 0;
		return;
	}
	if ( encrypted ) {
		Com_sprintf( cmd, sizeof( cmd ), "kls -1 -1 encb %d %d \"%s\"", SB_PROTOCOL, seq, packed );
	} else {
		Com_sprintf( cmd, sizeof( cmd ), "kls -1 -1 spb %d %d \"%s\"", SB_PROTOCOL, seq, packed );
	}

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gclient_t *cl = &level.clients[i];

		// a lagger's reliable commands pile up unacknowledged: too many and the server drops him
		if ( !g_entities[i].inuse || cl->pers.connected != CON_CONNECTED || ( g_entities[i].r.svFlags & SVF_BOT ) || cl->isLagging ) {
			continue;
		}
		// a client that changed side since the last frame still gets what came in while it was on the old one
		if ( ( cl->sess.sessionTeam == TEAM_SPECTATOR ) != !encrypted && sbSide[i] != ( encrypted ? SB_SIDE_PLAYER : SB_SIDE_SPEC ) ) {
			continue;
		}
		trap_SendServerCommand( i, cmd );
	}
	sb->len = 0;
}

// both streams go out together, so nobody changing team between them loses or doubles a record
void G_SpecBatch_Flush( void ) {
	SB_SendBuf( &sbEnc, qtrue );
	SB_SendBuf( &sbPlain, qfalse );
	sbSeq++;
	sbFirstRecordTime = 0;
}

static qboolean SB_Fits( const sbBuf_t *sb, int recLen ) {
	// header (worst case) + record + crc
	return (qboolean)( ( sb->len ? sb->len : 4 + 5 ) + recLen + 4 <= SB_MAX_BYTES );
}

static void SB_Append( sbBuf_t *sb, const sbRec_t *r ) {
	if ( !sb->len ) {
		sbRec_t h;

		memset( &h, 0, sizeof( h ) );
		SB_Byte( &h, SB_MAGIC );
		SB_Byte( &h, SB_VERSION );
		SB_Byte( &h, 0 );
		SB_Byte( &h, 0 );
		sb->base = SB_Now();
		SB_Varint( &h, (unsigned int)sb->base );
		memcpy( sb->buf, h.b, h.len );
		sb->len = h.len;
	}
	memcpy( sb->buf + sb->len, r->b, r->len );
	sb->len += r->len;
	if ( !sbFirstRecordTime ) {
		sbFirstRecordTime = level.time;
	}
}

// a record for one stream or both
static void SB_Add( const sbRec_t *r, qboolean toEnc, qboolean toPlain ) {
	toEnc = (qboolean)( toEnc && SB_EncOn() );
	toPlain = (qboolean)( toPlain && SB_PlainOn() );
	if ( r->overflow || ( !toEnc && !toPlain ) ) {
		return;
	}
	if ( ( toEnc && !SB_Fits( &sbEnc, r->len ) ) || ( toPlain && !SB_Fits( &sbPlain, r->len ) ) ) {
		G_SpecBatch_Flush();
		if ( !SB_Fits( &sbEnc, r->len ) ) {
			return;
		}
	}
	if ( toEnc ) {
		SB_Append( &sbEnc, r );
	}
	if ( toPlain ) {
		SB_Append( &sbPlain, r );
	}
}

static void SB_EmitSpan( int kind, int clientNum, const sbSpan_t *s, qboolean closed ) {
	sbRec_t r;

	memset( &r, 0, sizeof( r ) );
	SB_Byte( &r, SB_REC_SPAN );
	SB_Byte( &r, kind );
	SB_Byte( &r, clientNum );
	SB_Varint( &r, SB_ZigZag( s->start ) );
	SB_Varint( &r, SB_ZigZag( s->end ) );
	SB_Varint( &r, SB_ZigZag( s->due ) );
	SB_Varint( &r, (unsigned int)( s->total > 0 ? s->total : 0 ) );
	SB_Varint( &r, (unsigned int)( s->value > 0 ? s->value : 0 ) );
	SB_Byte( &r, closed ? 1 : 0 );
	SB_Add( &r, qtrue, qtrue );
}

static int SB_SlotKind( int slot ) {
	if ( slot < SB_SLOT_POWERUP ) {
		return slot;
	}
	if ( slot < SB_SLOT_LOCK ) {
		return SB_SPAN_POWERUP + sbPowerups[slot - SB_SLOT_POWERUP];
	}
	return SB_SPAN_ROCKETLOCK;
}

// due: the scheduled end (0 = open-ended)
static void SB_Open( int clientNum, int slot, int start, int due, int total, int value, int stamp ) {
	sbSpan_t *s = &sbSpans[clientNum][slot];

	s->open = qtrue;
	s->start = start;
	s->end = due;
	s->due = due;
	s->total = total;
	s->value = value;
	s->seenAbs = stamp;
	s->seenRel = stamp - level.startTime;
	SB_EmitSpan( SB_SlotKind( slot ), clientNum, s, qfalse );
}

// end: when it actually ended (level-relative)
static void SB_Close( int clientNum, int slot, int end ) {
	sbSpan_t *s = &sbSpans[clientNum][slot];

	if ( !s->open ) {
		return;
	}
	s->open = qfalse;
	if ( end < s->end || !s->end ) {
		s->end = end;
	}
	if ( s->end < s->start ) {	// the pause's time base snap can put now before a span that just opened
		s->end = s->start;
	}
	SB_EmitSpan( SB_SlotKind( slot ), clientNum, s, qtrue );
}

/*
	From StartForceTimer, with the same duration fpt sends: a timed force power (or FP_SABER_OFFENSE for rage
	recovery) starting (duration > 0) or stopping (0). Starting rage recovery ends rage and vice versa.
*/
void G_SpecBatch_PowerTimer( int clientNum, int forcePower, int duration ) {
	const int now = SB_Now();

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || forcePower < 0 || forcePower >= NUM_FORCE_POWERS ) {
		return;
	}
	SB_Close( clientNum, forcePower, now );
	if ( duration > 0 ) {
		if ( forcePower == FP_SABER_OFFENSE ) {
			SB_Close( clientNum, FP_RAGE, now );
		} else if ( forcePower == FP_RAGE ) {
			SB_Close( clientNum, FP_SABER_OFFENSE, now );
		}
		SB_Open( clientNum, forcePower, now, now + duration, duration, 0, 0 );
	}
}

void G_SpecBatch_NoteGave( int clientNum, int which ) {
	sbRec_t r;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}
	memset( &r, 0, sizeof( r ) );
	SB_Byte( &r, SB_REC_MOMENT );
	SB_Byte( &r, which ? SB_MOMENT_GAVE_TE : SB_MOMENT_GAVE_TH );
	SB_Byte( &r, clientNum );
	SB_Varint( &r, SB_ZigZag( SB_Now() ) );
	SB_Add( &r, qtrue, qtrue );
}

/*
	The pause code moved level.startTime by `snap` without moving anything else: every level-relative time of
	a running span is now `snap` too late. End each one here and continue it as a new span from now.
*/
void G_SpecBatch_TimeBaseSnapped( int snap ) {
	int c, slot;

	for ( c = 0; c < MAX_CLIENTS; c++ ) {
		for ( slot = 0; slot < SB_NUM_SLOTS; slot++ ) {
			sbSpan_t s = sbSpans[c][slot];

			if ( !s.open ) {
				continue;
			}
			SB_Close( c, slot, SB_Now() );
			// a lock's total is when it began, level-relative too
			SB_Open( c, slot, SB_Now(), s.due ? s.due - snap : 0, slot == SB_SLOT_LOCK ? s.total - snap : s.total, s.value, s.seenAbs );
		}
	}
}

// every included player's spec info token, on the overlay tick: one sample each, encrypted stream only
void G_SpecBatch_Sample( int clientNum, const char *token ) {
	sbRec_t	body, r;
	int		tokenClient = -1;

	if ( !SB_EncOn() ) {
		return;
	}
	memset( &body, 0, sizeof( body ) );
	if ( !SB_EncodeToken( &body, token, &tokenClient ) || tokenClient != clientNum ) {
		return;
	}
	// type, client and the time (relative to the buffer's base, so the flush that makes room comes first)
	if ( !SB_Fits( &sbEnc, body.len + 7 ) ) {
		G_SpecBatch_Flush();
	}
	memset( &r, 0, sizeof( r ) );
	SB_Byte( &r, SB_REC_SAMPLE );
	SB_Byte( &r, clientNum );
	SB_Varint( &r, SB_ZigZag( sbEnc.len ? SB_Now() - sbEnc.base : 0 ) );
	if ( r.len + body.len > (int)sizeof( r.b ) ) {
		return;
	}
	memcpy( r.b + r.len, body.b, body.len );
	r.len += body.len;
	SB_Add( &r, qtrue, qfalse );
}

static qboolean SB_Tracked( const gentity_t *ent ) {
	return (qboolean)( ent->inuse && ent->client && ent->client->pers.connected == CON_CONNECTED
		&& ent->client->sess.sessionTeam != TEAM_SPECTATOR );
}

// every server frame: spans the game state opens and closes by itself, the end of a force power that stopped
// without StartForceTimer, and the 30 s flush
void G_SpecBatch_Frame( void ) {
	const int	now = SB_Now();
	int			c, i;

	if ( !SB_EncOn() && !SB_PlainOn() ) {
		// nothing is sent meanwhile: what runs now is unknown when it comes back on
		memset( sbSpans, 0, sizeof( sbSpans ) );
		memset( sbFall, 0, sizeof( sbFall ) );
		return;
	}
	if ( level.intermissiontime ) {
		return;
	}

	// a client that changed side: what is pending for its old side goes out now, while it still counts there
	{
		int			side[MAX_CLIENTS];
		qboolean	changed = qfalse;

		for ( c = 0; c < MAX_CLIENTS; c++ ) {
			const gentity_t *ent = &g_entities[c];

			side[c] = SB_SIDE_NONE;
			if ( ent->inuse && ent->client && ent->client->pers.connected == CON_CONNECTED && !( ent->r.svFlags & SVF_BOT ) ) {
				side[c] = ent->client->sess.sessionTeam == TEAM_SPECTATOR ? SB_SIDE_SPEC : SB_SIDE_PLAYER;
			}
			if ( side[c] != sbSide[c] && sbSide[c] != SB_SIDE_NONE && side[c] != SB_SIDE_NONE ) {
				changed = qtrue;
			}
		}
		if ( changed ) {
			G_SpecBatch_Flush();
		}
		memcpy( sbSide, side, sizeof( sbSide ) );
	}

	for ( c = 0; c < MAX_CLIENTS; c++ ) {
		gentity_t		*ent = &g_entities[c];
		playerState_t	*ps;

		if ( !SB_Tracked( ent ) ) {
			for ( i = 0; i < SB_NUM_SLOTS; i++ ) {
				SB_Close( c, i, now );
			}
			sbFall[c].open = qfalse;
			continue;
		}
		ps = &ent->client->ps;

		// force powers that lost their active bit (death, respawn, a drain) and rage recovery that ran out
		for ( i = 0; i < NUM_FORCE_POWERS; i++ ) {
			const sbSpan_t *s = &sbSpans[c][i];

			if ( !s->open ) {
				continue;
			}
			if ( i == FP_SABER_OFFENSE ) {
				if ( ps->fd.forceRageRecoveryTime <= level.time || now >= s->end ) {
					SB_Close( c, i, now < s->end ? now : s->end );
				}
			} else if ( !( ps->fd.forcePowersActive & ( 1 << i ) ) ) {
				SB_Close( c, i, now );
			}
		}

		// powerups: open on pickup with their end, reopen when the end changes
		for ( i = 0; i < SB_NUM_POWERUPS; i++ ) {
			const int	slot = SB_SLOT_POWERUP + i;
			const int	until = ps->powerups[sbPowerups[i]];
			sbSpan_t	*s = &sbSpans[c][slot];

			if ( until > level.time ) {
				if ( SB_NewStamp( s, until ) ) {
					SB_Close( c, slot, now );
					SB_Open( c, slot, now, until - level.startTime, until - level.time, 0, until );
				}
			} else if ( s->open ) {
				SB_Close( c, slot, now );
			}
		}

		/*
			rocket lock: value = locked entity | progress << 16 (the improved homing's 0-10, from the unused
			holocron bits; 0 elsewhere), total = when the lock began. a new span whenever either changes
		*/
		{
			sbSpan_t	*s = &sbSpans[c][SB_SLOT_LOCK];
			const int	target = ps->rocketLockIndex;

			if ( s->open && target >= 0 && target < ENTITYNUM_NONE && ps->rocketLockTime == -1 ) {
				// the trace lost the target for a moment (up to 500 ms): the lock holds, as pmove puts it back
			} else if ( target >= 0 && target < ENTITYNUM_NONE && ps->rocketLockTime > 0 ) {
				const int value = target | ( ( ( ps->holocronBits >> NUM_FORCE_POWERS ) & 15 ) << 16 );

				if ( s->value != value || SB_NewStamp( s, (int)ps->rocketLockTime ) ) {
					SB_Close( c, SB_SLOT_LOCK, now );
					SB_Open( c, SB_SLOT_LOCK, now, 0, (int)ps->rocketLockTime - level.startTime, value, (int)ps->rocketLockTime );
				}
			} else if ( s->open ) {
				SB_Close( c, SB_SLOT_LOCK, now );
			}
		}

		// falling into a pit: the moment it began
		if ( ps->fallingToDeath && SB_NewStamp( &sbFall[c], ps->fallingToDeath ) ) {
			sbRec_t r;

			sbFall[c].open = qtrue;
			sbFall[c].seenAbs = ps->fallingToDeath;
			sbFall[c].seenRel = ps->fallingToDeath - level.startTime;
			memset( &r, 0, sizeof( r ) );
			SB_Byte( &r, SB_REC_MOMENT );
			SB_Byte( &r, SB_MOMENT_FALL );
			SB_Byte( &r, c );
			SB_Varint( &r, SB_ZigZag( ps->fallingToDeath - level.startTime ) );
			SB_Varint( &r, SB_ZigZag( (int)floorf( ps->origin[0] + 0.5f ) ) );
			SB_Varint( &r, SB_ZigZag( (int)floorf( ps->origin[1] + 0.5f ) ) );
			SB_Varint( &r, SB_ZigZag( (int)floorf( ps->origin[2] + 0.5f ) ) );
			SB_Add( &r, qtrue, qtrue );
		} else if ( !ps->fallingToDeath ) {
			sbFall[c].open = qfalse;
		}
	}

	if ( sbFirstRecordTime && level.time - sbFirstRecordTime >= SB_FLUSH_MS ) {
		G_SpecBatch_Flush();
	}
}

// a player whose disconnected state was restored: his running timed powers and rage recovery, from now
void G_SpecBatch_Restored( int clientNum ) {
	static const int timed[] = { FP_SPEED, FP_TELEPATHY, FP_PROTECT, FP_ABSORB, FP_RAGE, FP_SEE };
	const playerState_t	*ps;
	int					i;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !g_entities[clientNum].client ) {
		return;
	}
	ps = &g_entities[clientNum].client->ps;
	for ( i = 0; i < (int)( sizeof( timed ) / sizeof( timed[0] ) ); i++ ) {
		if ( ( ps->fd.forcePowersActive & ( 1 << timed[i] ) ) && ps->fd.forcePowerDuration[timed[i]] > level.time ) {
			G_SpecBatch_PowerTimer( clientNum, timed[i], ps->fd.forcePowerDuration[timed[i]] - level.time );
		}
	}
	if ( ps->fd.forceRageRecoveryTime > level.time ) {
		G_SpecBatch_PowerTimer( clientNum, FP_SABER_OFFENSE, ps->fd.forceRageRecoveryTime - level.time );
	}
}

// intermission: close what is running and send everything before the key goes out
void G_SpecBatch_Intermission( void ) {
	int c, slot;

	for ( c = 0; c < MAX_CLIENTS; c++ ) {
		for ( slot = 0; slot < SB_NUM_SLOTS; slot++ ) {
			const sbSpan_t *s = &sbSpans[c][slot];

			// a running timer keeps its scheduled end: intermission freezes it, it didn't stop. an open-ended one ends now
			if ( s->open ) {
				sbSpans[c][slot].open = qfalse;
				if ( !s->due ) {
					sbSpans[c][slot].end = SB_Now();
				}
				SB_EmitSpan( SB_SlotKind( slot ), c, s, qtrue );
			}
		}
	}
	G_SpecBatch_Flush();
	sbKeyOut = qtrue;	// a level that goes on after intermission (no map change) must not use a revealed key
}

#endif
