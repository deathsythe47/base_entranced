#include "g_database.h"
#include "sqlite3.h"
#include "time.h"

#include "g_database_schema.h"

static sqlite3* diskDb = NULL;
static sqlite3* dbPtr = NULL;

static void ErrorCallback( void* ctx, int code, const char* msg ) {
	Com_Printf( "SQL error (code %d): %s\n", code, msg );
}

static int TraceCallback( unsigned int type, void* ctx, void* ptr, void* info ) {
	if ( !ptr || !info ) {
		return 0;
	}

	if ( type == SQLITE_TRACE_STMT ) {
		char* sql = ( char* )info;

		//Com_Printf( "Executing SQL: \n" );

		if ( !Q_stricmpn( sql, "--", 2 ) ) {
			// a comment, which means this is a trigger, log it directly
			Com_Printf( "--------------------------------------------------------------------------------\n" );
			Com_Printf( "%s\n", sql );
			Com_Printf( "--------------------------------------------------------------------------------\n" );
		} else {
			// expand the sql before logging it so we can see parameters
			sqlite3_stmt* stmt = ( sqlite3_stmt* )ptr;
			sql = sqlite3_expanded_sql( stmt );
			Com_Printf( "--------------------------------------------------------------------------------\n" );
			Com_Printf( "%s\n", sql );
			Com_Printf( "--------------------------------------------------------------------------------\n" );
			sqlite3_free( sql );
		}
	} else if ( type == SQLITE_TRACE_PROFILE ) {
		unsigned long long nanoseconds = *( ( unsigned long long* )info );
		unsigned int ms = nanoseconds / 1000000;
		Com_Printf( "Executed in %ums\n", ms );
	}

	return 0;
}

void G_DBLoadDatabase( void *serverDbPtr )
{
	if (!serverDbPtr) {
		Com_Error(ERR_DROP, "Null db pointer from server");
	}

	dbPtr = serverDbPtr;

	// register trace callback if needed
	if ( g_traceSQL.integer ) {
		sqlite3_trace_v2( dbPtr, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, TraceCallback, NULL );
	}

	// more db options
	trap_sqlite3_exec( dbPtr, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL );

	// get version and call the upgrade routine

	char s[16];
	G_DBGetMetadata( "schema_version", s, sizeof( s ) );
	int version = VALIDSTRING( s ) ? atoi( s ) : 0;

	// setup tables if database was just created
	// NOTE: this means g_database_schema.h must always reflect the latest version
	if (!version) {
		trap_sqlite3_exec(dbPtr, sqlCreateTables, 0, 0, 0);
	}

#if 0
	// unused thus far
	if ( !G_DBUpgradeDatabaseSchema( version, dbPtr ) ) {
		// don't let server load if an upgrade failed

		/*if (dbPtr != diskDb) {
			// close in memory db immediately
			sqlite3_close(dbPtr);
			dbPtr = NULL;
		}*/

		Com_Error(ERR_DROP, "Failed to upgrade database, shutting down to avoid data corruption\n");
	}
#endif

	G_DBSetMetadata( "schema_version", DB_SCHEMA_VERSION_STR );

	// optimize the db if needed

	G_DBGetMetadata( "last_optimize", s, sizeof( s ) );

	const time_t currentTime = time( NULL );
	time_t last_optimize = VALIDSTRING( s ) ? strtoll( s, NULL, 10 ) : 0;

	if ( last_optimize + DB_OPTIMIZE_INTERVAL < currentTime ) {
		Com_Printf( "Automatically optimizing database...\n" );

		trap_sqlite3_exec( dbPtr, "PRAGMA optimize;", NULL, NULL, NULL );

		G_DBSetMetadata( "last_optimize", va( "%lld", currentTime ) );
	}

	// if the server is empty, vacuum the db if needed

	if ( !level.numConnectedClients ) {
		G_DBGetMetadata( "last_vacuum", s, sizeof( s ) );

		time_t last_autoclean = VALIDSTRING( s ) ? strtoll( s, NULL, 10 ) : 0;

		if ( last_autoclean + DB_VACUUM_INTERVAL < currentTime ) {
			Com_Printf( "Automatically running vacuum on database...\n" );

			trap_sqlite3_exec( dbPtr, "VACUUM;", NULL, NULL, NULL );

			G_DBSetMetadata( "last_vacuum", va( "%lld", currentTime ) );
		}
	}

	
}

void G_DBUnloadDatabase( void )
{

}

// =========== METADATA ========================================================

const char* const sqlGetMetadata =
"SELECT value FROM metadata WHERE metadata.key = ?1 LIMIT 1;                   ";

const char* const sqlSetMetadata =
"INSERT OR REPLACE INTO metadata (key, value) VALUES( ?1, ?2 );                ";

void G_DBGetMetadata( const char *key,
	char *outValue,
	size_t outValueBufSize )
{
	if (!VALIDSTRING(key) || !outValue || !outValueBufSize) {
		assert(qfalse);
		return;
	}

	sqlite3_stmt* statement;

	outValue[0] = '\0';

	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlGetMetadata, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, key, -1, SQLITE_STATIC );

	rc = trap_sqlite3_step( statement );( statement );
	while ( rc == SQLITE_ROW ) {
		const char *value = ( const char* )sqlite3_column_text( statement, 0 );
		if (VALIDSTRING(value))
			Q_strncpyz( outValue, value, outValueBufSize );

		rc = trap_sqlite3_step( statement );( statement );
	}

	trap_sqlite3_finalize( statement );
}

void G_DBSetMetadata( const char *key,
	const char *value )
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlSetMetadata, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, key, -1, SQLITE_STATIC );
	sqlite3_bind_text( statement, 2, value, -1, SQLITE_STATIC );

	rc = trap_sqlite3_step( statement );( statement );

	trap_sqlite3_finalize( statement );
}

// =========== ACCOUNTS ========================================================

static const char* sqlGetAccountByID =
"SELECT name, created_on, usergroup, flags                                   \n"
"FROM accounts                                                               \n"
"WHERE accounts.account_id = ?1;                                               ";

static const char* sqlGetAccountByName =
"SELECT account_id, name, created_on, usergroup, flags                       \n"
"FROM accounts                                                               \n"
"WHERE accounts.name = ?1;                                                     ";

static const char* sqlCreateAccount =
"INSERT INTO accounts ( name ) VALUES ( ?1 );                                  ";

static const char* sqlDeleteAccount =
"DELETE FROM accounts WHERE accounts.account_id = ?1;                          ";

static const char* sqlGetSessionByID =
"SELECT identifier, info, account_id                                         \n"
"FROM sessions                                                               \n"
"WHERE sessions.identifier = ?1;                                               ";

static const char* sqlGetSessionByIdentifier =
"SELECT session_id, info, account_id                                         \n"
"FROM sessions                                                               \n"
"WHERE sessions.identifier = ?1;                                               ";

static const char* sqlCreateSession =
"INSERT INTO sessions ( identifier, info ) VALUES ( ?1, ?2 );                  ";

static const char* sqlLinkAccountToSession =
"UPDATE sessions                                                             \n"
"SET account_id = ?1                                                         \n"
"WHERE session_id = ?2;                                                        ";

static const char* sqlListSessionIdsForAccount =
"SELECT session_id, identifier, info, referenced                             \n"
"FROM sessions_info                                                          \n"
"WHERE sessions_info.account_id = ?1;                                          ";

qboolean G_DBGetAccountByID( const int id,
	account_t* account )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetAccountByID, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, id );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );( statement );

	if ( rc == SQLITE_ROW ) {
		const char* name = ( const char* )sqlite3_column_text( statement, 0 );
		const int created_on = sqlite3_column_int( statement, 1 );
		const int flags = sqlite3_column_int( statement, 3 );

		account->id = id;
		Q_strncpyz( account->name, name, sizeof( account->name ) );
		account->creationDate = created_on;
		account->flags = flags;

		if ( sqlite3_column_type( statement, 2 ) != SQLITE_NULL ) {
			const char* usergroup = ( const char* )sqlite3_column_text( statement, 2 );
			Q_strncpyz( account->group, usergroup, sizeof( account->group ) );
		}
		else {
			account->group[0] = '\0';
		}

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return found;
}

qboolean G_DBGetAccountByName( const char* name,
	account_t* account )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetAccountByName, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, name, -1, 0 );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );( statement );

	if ( rc == SQLITE_ROW ) {
		const int account_id = sqlite3_column_int( statement, 0 );
		const char* name = ( const char* )sqlite3_column_text( statement, 1 );
		const int created_on = sqlite3_column_int( statement, 2 );
		const int flags = sqlite3_column_int( statement, 4 );

		account->id = account_id;
		Q_strncpyz( account->name, name, sizeof( account->name ) );
		account->creationDate = created_on;
		account->flags = flags;

		if ( sqlite3_column_type( statement, 3 ) != SQLITE_NULL ) {
			const char* usergroup = ( const char* )sqlite3_column_text( statement, 3 );
			Q_strncpyz( account->group, usergroup, sizeof( account->group ) );
		} else {
			account->group[0] = '\0';
		}

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return found;
}

void G_DBCreateAccount( const char* name )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlCreateAccount, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, name, -1, 0 );

	trap_sqlite3_step( statement );( statement );

	trap_sqlite3_finalize( statement );
}

qboolean G_DBDeleteAccount( account_t* account )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlDeleteAccount, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, account->id );

	trap_sqlite3_step( statement );( statement );

	qboolean success = sqlite3_changes( dbPtr ) != 0;

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBGetSessionByID( const int id,
	session_t* session )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetSessionByID, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, id );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );( statement );

	if ( rc == SQLITE_ROW ) {
		const sessionIdentifier_t identifier = sqlite3_column_int64( statement, 0 );
		const char* info = ( const char* )sqlite3_column_text( statement, 1 );

		session->id = id;
		session->identifier = identifier;
		Q_strncpyz( session->info, info, sizeof( session->info ) );

		if ( sqlite3_column_type( statement, 2 ) != SQLITE_NULL ) {
			const int account_id = sqlite3_column_int( statement, 2 );
			session->accountId = account_id;
		} else {
			session->accountId = ACCOUNT_ID_UNLINKED;
		}

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return found;
}

qboolean G_DBGetSessionByIdentifier( const sessionIdentifier_t identifier,
	session_t* session )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetSessionByIdentifier, -1, &statement, 0 );

	sqlite3_bind_int64( statement, 1, identifier );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );( statement );

	if ( rc == SQLITE_ROW ) {
		const int session_id = sqlite3_column_int( statement, 0 );
		const char* info = ( const char* )sqlite3_column_text( statement, 1 );
		
		session->id = session_id;
		session->identifier = identifier;
		Q_strncpyz( session->info, info, sizeof( session->info ) );

		if ( sqlite3_column_type( statement, 2 ) != SQLITE_NULL ) {
			const int account_id = sqlite3_column_int( statement, 2 );
			session->accountId = account_id;
		} else {
			session->accountId = ACCOUNT_ID_UNLINKED;
		}

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return found;
}

void G_DBCreateSession( const sessionIdentifier_t identifier,
	const char* info )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlCreateSession, -1, &statement, 0 );

	sqlite3_bind_int64( statement, 1, identifier );
	sqlite3_bind_text( statement, 2, info, -1, 0 );

	trap_sqlite3_step( statement );( statement );

	trap_sqlite3_finalize( statement );
}

void G_DBLinkAccountToSession( session_t* session,
	account_t* account )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlLinkAccountToSession, -1, &statement, 0 );

	if ( account ) {
		sqlite3_bind_int( statement, 1, account->id );
	} else {
		sqlite3_bind_null( statement, 1 );
	}
	
	sqlite3_bind_int( statement, 2, session->id );

	trap_sqlite3_step( statement );( statement );

	// link in the struct too if successful
	if ( sqlite3_changes( dbPtr ) != 0 ) {
		session->accountId = account ? account->id : ACCOUNT_ID_UNLINKED;
	}

	trap_sqlite3_finalize( statement );
}

void G_DBUnlinkAccountFromSession( session_t* session )
{
	G_DBLinkAccountToSession( session, NULL );
}

void G_DBListSessionsForAccount( account_t* account,
	DBListAccountSessionsCallback callback,
	void* ctx )
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlListSessionIdsForAccount, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, account->id );

	rc = trap_sqlite3_step( statement );( statement );
	while ( rc == SQLITE_ROW ) {
		session_t session;

		const int session_id = sqlite3_column_int( statement, 0 );
		const sqlite3_int64 identifier = sqlite3_column_int64( statement, 1 );
		const char* info = ( const char* )sqlite3_column_text( statement, 2 );
		const qboolean referenced = !!sqlite3_column_int( statement, 3 );

		session.id = session_id;
		session.identifier = identifier;
		Q_strncpyz( session.info, info, sizeof( session.info ) );
		session.accountId = account->id;

		callback( ctx, &session, !referenced );

		rc = trap_sqlite3_step( statement );( statement );
	}

	trap_sqlite3_finalize( statement );
}

// =========== NICKNAMES =======================================================

const char* const sqlAddName =
"INSERT INTO nicknames (ip_int, name, duration)                              \n"
"VALUES (?,?,?)                                                                ";

const char* const sqlAddNameNM =
"INSERT INTO nicknames (ip_int, name, duration, cuid_hash2)                  \n"
"VALUES (?,?,?,?)                                                              ";

const char* const sqlGetAliases =
"SELECT name, SUM( duration ) AS time                                        \n"
"FROM nicknames                                                              \n"
"WHERE nicknames.ip_int & ?2 = ?1 & ?2                                       \n"
"GROUP BY name                                                               \n"
"ORDER BY time DESC                                                          \n"
"LIMIT ?3                                                                      ";

const char* const sqlGetNMAliases =
"SELECT name, SUM( duration ) AS time                                        \n"
"FROM nicknames                                                              \n"
"WHERE nicknames.cuid_hash2 = ?1                                             \n"
"GROUP BY name                                                               \n"
"ORDER BY time DESC                                                          \n"
"LIMIT ?2                                                                      ";

const char* const sqlCountNMAliases =
"SELECT COUNT(*) FROM ("
"SELECT name, SUM( duration ) AS time                                        \n"
"FROM nicknames                                                              \n"
"WHERE nicknames.cuid_hash2 = ?1                                             \n"
"GROUP BY name                                                               \n"
"ORDER BY time DESC                                                          \n"
"LIMIT ?2                                                                    \n"
")                                                                             ";

void G_DBLogNickname( unsigned int ipInt,
	const char* name,
	int duration,
	const char* cuidHash )
{
	sqlite3_stmt* statement;

	// prepare insert statement
	trap_sqlite3_prepare_v2( dbPtr, VALIDSTRING( cuidHash ) ? sqlAddNameNM : sqlAddName, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ipInt );
	sqlite3_bind_text( statement, 2, name, -1, 0 );
	sqlite3_bind_int( statement, 3, duration );
	if ( VALIDSTRING( cuidHash ) )
		sqlite3_bind_text( statement, 4, cuidHash, -1, 0 );

	trap_sqlite3_step( statement );( statement );

	trap_sqlite3_finalize( statement );
}

void G_DBListAliases( unsigned int ipInt,
	unsigned int ipMask,
	int limit,
	ListAliasesCallback callback,
	void* context,
	const char* cuidHash )
{
	sqlite3_stmt* statement;
	int rc;
	const char* name;
	int duration;
	if ( VALIDSTRING( cuidHash ) ) { // newmod user; check for cuid matches first before falling back to checking for unique id matches
		int numNMFound = 0;
		rc = trap_sqlite3_prepare_v2( dbPtr, sqlCountNMAliases, -1, &statement, 0 );
		sqlite3_bind_text( statement, 1, cuidHash, -1, 0 );
		sqlite3_bind_int( statement, 2, limit );

		rc = trap_sqlite3_step( statement );( statement );
		while ( rc == SQLITE_ROW ) {
			numNMFound = sqlite3_column_int( statement, 0 );
			rc = trap_sqlite3_step( statement );( statement );
		}
		trap_sqlite3_reset( statement );

		if ( numNMFound ) { // we found some cuid matches; let's use these
			rc = trap_sqlite3_prepare_v2( dbPtr, sqlGetNMAliases, -1, &statement, 0 );
			sqlite3_bind_text( statement, 1, cuidHash, -1, 0 );
			sqlite3_bind_int( statement, 2, limit );

			rc = trap_sqlite3_step( statement );( statement );
			while ( rc == SQLITE_ROW ) {
				name = ( const char* )sqlite3_column_text( statement, 0 );
				duration = sqlite3_column_int( statement, 1 );

				callback( context, name, duration );

				rc = trap_sqlite3_step( statement );( statement );
			}
			trap_sqlite3_finalize( statement );
		}
		else { // didn't find any cuid matches; use the old unique id method
			rc = trap_sqlite3_prepare_v2( dbPtr, sqlGetAliases, -1, &statement, 0 );
			sqlite3_bind_int( statement, 1, ipInt );
			sqlite3_bind_int( statement, 2, ipMask );
			sqlite3_bind_int( statement, 3, limit );

			rc = trap_sqlite3_step( statement );( statement );
			while ( rc == SQLITE_ROW ) {
				name = ( const char* )sqlite3_column_text( statement, 0 );
				duration = sqlite3_column_int( statement, 1 );

				callback( context, name, duration );

				rc = trap_sqlite3_step( statement );( statement );
			}
			trap_sqlite3_finalize( statement );
		}
	}
	else { // non-newmod; just use the old unique id method
		sqlite3_stmt* statement;
		// prepare insert statement
		int rc = trap_sqlite3_prepare_v2( dbPtr, sqlGetAliases, -1, &statement, 0 );

		sqlite3_bind_int( statement, 1, ipInt );
		sqlite3_bind_int( statement, 2, ipMask );
		sqlite3_bind_int( statement, 3, limit );

		rc = trap_sqlite3_step( statement );( statement );
		while ( rc == SQLITE_ROW ) {
			name = ( const char* )sqlite3_column_text( statement, 0 );
			duration = sqlite3_column_int( statement, 1 );

			callback( context, name, duration );

			rc = trap_sqlite3_step( statement );( statement );
		}

		trap_sqlite3_finalize( statement );
	}
}

// =========== FASTCAPS ========================================================

const char* const sqlAddSiegeFastcapV2 =
"INSERT INTO siegefastcaps (                                                    "
"    mapname, flags, rank, desc, player1_name, player1_ip_int,                  "
"    player1_cuid_hash2, max_speed, avg_speed,                                  "
"    player2_name, player2_ip_int, player2_cuid_hash2,                          "
"    obj1_time, obj2_time, obj3_time, obj4_time, obj5_time,                     "
"    obj6_time, obj7_time, obj8_time, obj9_time, total_time, date,              "
"    match_id, player1_client_id, player2_client_id,                            "
"    player3_name, player3_ip_int, player3_cuid_hash2,                          "
"    player4_name, player4_ip_int, player4_cuid_hash2)                          "
"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)       ";

const char* const sqlremoveSiegeFastcaps =
"DELETE FROM siegefastcaps WHERE mapname = ?1 AND flags = ?2                    ";

const char* const sqlGetSiegeFastcapsStrict =
"SELECT flags, player1_name, player1_ip_int,                                    "
"    player1_cuid_hash2, max_speed, avg_speed,                                  "
"    player2_name, player2_ip_int, player2_cuid_hash2,                          "
"    obj1_time, obj2_time, obj3_time, obj4_time, obj5_time,                     "
"    obj6_time, obj7_time, obj8_time, obj9_time, total_time, date,              "
"    match_id, player1_client_id, player2_client_id,                            "
"    player3_name, player3_ip_int, player3_cuid_hash2,                          "
"    player4_name, player4_ip_int, player4_cuid_hash2                           "
"FROM siegefastcaps                                                             "
"WHERE siegefastcaps.mapname = ?1 AND siegefastcaps.flags = ?2                  "
"ORDER BY total_time                                                            "
"LIMIT ?3                                                                       ";

const char* const sqlGetSiegeFastcapsRelaxed =
"SELECT flags, player1_name, player1_ip_int,                                    "
"    player1_cuid_hash2, max_speed, avg_speed,                                  "
"    player2_name, player2_ip_int, player2_cuid_hash2,                          "
"    obj1_time, obj2_time, obj3_time, obj4_time, obj5_time,                     "
"    obj6_time, obj7_time, obj8_time, obj9_time, total_time, date,              "
"    match_id, player1_client_id, player2_client_id,                            "
"    player3_name, player3_ip_int, player3_cuid_hash2,                          "
"    player4_name, player4_ip_int, player4_cuid_hash2                          "
"FROM siegefastcaps                                                             "
"WHERE siegefastcaps.mapname = ?1 AND (siegefastcaps.flags & ?2) = ?2           "
"ORDER BY total_time                                                            "
"LIMIT ?3                                                                       ";

const char *const sqlGetSiegeFastcapsStrictDescending =
"SELECT flags, player1_name, player1_ip_int,                                    "
"    player1_cuid_hash2, max_speed, avg_speed,                                  "
"    player2_name, player2_ip_int, player2_cuid_hash2,                          "
"    obj1_time, obj2_time, obj3_time, obj4_time, obj5_time,                     "
"    obj6_time, obj7_time, obj8_time, obj9_time, total_time, date,              "
"    match_id, player1_client_id, player2_client_id,                            "
"    player3_name, player3_ip_int, player3_cuid_hash2,                          "
"    player4_name, player4_ip_int, player4_cuid_hash2                           "
"FROM siegefastcaps                                                             "
"WHERE siegefastcaps.mapname = ?1 AND siegefastcaps.flags = ?2                  "
"ORDER BY total_time DESC                                                       "
"LIMIT ?3                                                                       ";

const char *const sqlGetSiegeFastcapsRelaxedDescending =
"SELECT flags, player1_name, player1_ip_int,                                    "
"    player1_cuid_hash2, max_speed, avg_speed,                                  "
"    player2_name, player2_ip_int, player2_cuid_hash2,                          "
"    obj1_time, obj2_time, obj3_time, obj4_time, obj5_time,                     "
"    obj6_time, obj7_time, obj8_time, obj9_time, total_time, date,              "
"    match_id, player1_client_id, player2_client_id,                            "
"    player3_name, player3_ip_int, player3_cuid_hash2,                          "
"    player4_name, player4_ip_int, player4_cuid_hash2                           "
"FROM siegefastcaps                                                             "
"WHERE siegefastcaps.mapname = ?1 AND (siegefastcaps.flags & ?2) = ?2           "
"ORDER BY total_time DESC                                                       "
"LIMIT ?3                                                                       ";

const char* const sqlListBestSiegeFastcaps =
"SELECT flags, mapname, player1_name, player1_ip_int, player1_cuid_hash2,       "
"player2_name, player2_ip_int, player2_cuid_hash2,                              "
"player3_name, player3_ip_int, player3_cuid_hash2,                              "
"player4_name, player4_ip_int, player4_cuid_hash2,                              "
"MIN( total_time ) AS best_time, date                                           "
"FROM siegefastcaps                                                             "
"WHERE (siegefastcaps.flags & ?1) = ?1                                          "
"GROUP BY mapname                                                               "
"ORDER BY mapname ASC, date ASC                                                 "
"LIMIT ?2                                                                       "
"OFFSET ?3                                                                      ";

const char* const sqlListLatestSiegeFastcaps =
"SELECT flags, mapname, player1_name, player1_ip_int, player1_cuid_hash2,       "
"player2_name, player2_ip_int, player2_cuid_hash2,                              "
"player3_name, player3_ip_int, player3_cuid_hash2,                              "
"player4_name, player4_ip_int, player4_cuid_hash2,                              "
"total_time, date, rank                                                         "
"FROM siegefastcaps                                                             "
"WHERE (siegefastcaps.flags & ?1) = ?1                                          "
"ORDER BY date DESC                                                             "
"LIMIT ?2                                                                       "
"OFFSET ?3                                                                      ";

// strict == qtrue:  only get records where flags in db is exactly equal to flags parameter
// strict == qfalse: only get records where flags in db contains (bitwise AND) flags parameter
void G_DBLoadCaptureRecords(const char *mapname, CaptureCategoryFlags flags, qboolean strict, CaptureRecordsForCategory *out) {
#ifdef _DEBUG
	if (!flags) {
		Com_Printf("DEBUG MESSAGE: G_LogDbLoadCaptureRecords: flags 0\n");
	}
#endif
	memset(out, 0, sizeof(*out));

	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer) {
		return;
	}

	extern const char *G_GetArenaInfoByMap(const char *map);
	const char *arenaInfo = G_GetArenaInfoByMap(mapname);

	if (VALIDSTRING(arenaInfo)) {
		const char *mapFlags = Info_ValueForKey(arenaInfo, "b_e_flags");

		// this flag disables toptimes on this map
		// TODO: if I ever make more flags, make an actual define in some header file...
		if (VALIDSTRING(mapFlags) && atoi(mapFlags) & 1) {
			return;
		}
	}

	int loaded = 0;

	sqlite3_stmt* statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, flags & CAPTURERECORDFLAG_DEFENSE ? (strict ? sqlGetSiegeFastcapsStrictDescending : sqlGetSiegeFastcapsRelaxedDescending) : (strict ? sqlGetSiegeFastcapsStrict : sqlGetSiegeFastcapsRelaxed), -1, &statement, 0);

	sqlite3_bind_text(statement, 1, mapname, -1, 0);
	sqlite3_bind_int(statement, 2, flags);
	sqlite3_bind_int(statement, 3, MAX_SAVED_RECORDS);

	rc = trap_sqlite3_step( statement );(statement);

	int j = 0;
	while (rc == SQLITE_ROW && j < MAX_SAVED_RECORDS) {
		CaptureRecord *record = &out->records[j];
		int k = 0;
		record->flags = sqlite3_column_int(statement, k++);
		const char *player1_name = (const char*)sqlite3_column_text(statement, k++);
		if (VALIDSTRING(player1_name))
			Q_strncpyz(record->recordHolderNames[0], player1_name, sizeof(record->recordHolderNames[0]));
		record->recordHolderIpInts[0] = sqlite3_column_int(statement, k++);
		const char *player1_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		if (VALIDSTRING(player1_cuid_hash2))
			Q_strncpyz(record->recordHolderCuids[0], player1_cuid_hash2, sizeof(record->recordHolderCuids[0]));
		if (flags & CAPTURERECORDFLAG_DEFENSE) {
			record->frags = sqlite3_column_int(statement, k++);
			record->objectivesCompleted = sqlite3_column_int(statement, k++);
			record->maxSpeed1 = 0; // unused in defense records
			record->avgSpeed1 = 0; // unused in defense records
		}
		else {
			record->maxSpeed1 = sqlite3_column_int(statement, k++);
			record->avgSpeed1 = sqlite3_column_int(statement, k++);
			record->frags = 0; // unused in offense records
			record->objectivesCompleted = 0; // unused in offense records
		}

		qboolean secondPlayer = qfalse;
		const char *player2_name = (const char*)sqlite3_column_text(statement, k++);
		if (VALIDSTRING(player2_name)) {
			secondPlayer = qtrue;
			Q_strncpyz(record->recordHolderNames[1], player2_name, sizeof(record->recordHolderNames[1]));
			record->recordHolderIpInts[1] = sqlite3_column_int(statement, k++);
			const char *player2_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
			if (VALIDSTRING(player2_cuid_hash2))
				Q_strncpyz(record->recordHolderCuids[1], player2_cuid_hash2, sizeof(record->recordHolderCuids[1]));
		}
		else {
			k += 2;
		}

		if (flags & CAPTURERECORDFLAG_FULLMAP) { // round time includes times for each obj, too
			for (int l = 0; l < MAX_SAVED_OBJECTIVES; l++) {
				record->objTimes[l] = sqlite3_column_int(statement, k++);
			}
		}
		else {
			k += MAX_SAVED_OBJECTIVES;
		}

		record->totalTime = sqlite3_column_int(statement, k++);

		if (flags & CAPTURERECORDFLAG_DEFENSE) {
			if (record->frags)
				record->kpm = ((float)record->frags / (record->totalTime ? (float)record->totalTime : 1.0f)) * 60000.0f; // avoid division by zero
			else
				record->kpm = 0.0f;
		}

		record->date = sqlite3_column_int64(statement, k++);
		const char *match_id = (const char*)sqlite3_column_text(statement, k++);
		if (VALIDSTRING(match_id))
			Q_strncpyz(record->matchId, match_id, sizeof(record->matchId));
		record->recordHolder1ClientId = sqlite3_column_int(statement, k++);
		if (secondPlayer)
			record->recordHolder2ClientId = sqlite3_column_int(statement, k++);
		else
			k++;

		if (flags & CAPTURERECORDFLAG_LIVEPUG || flags & CAPTURERECORDFLAG_DEFENSE) { // live pugs and defense records can have up to 4 players logged
			const char *player3_name = sqlite3_column_text(statement, k++);
			if (VALIDSTRING(player3_name)) {
				Q_strncpyz(record->recordHolderNames[2], player3_name, sizeof(record->recordHolderNames[2]));
				record->recordHolderIpInts[2] = sqlite3_column_int(statement, k++);
				const char *player3_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
				if (VALIDSTRING(player3_cuid_hash2))
					Q_strncpyz(record->recordHolderCuids[2], player3_cuid_hash2, sizeof(record->recordHolderCuids[2]));
				const char *player4_name = sqlite3_column_text(statement, k++);
				if (VALIDSTRING(player4_name)) {
					Q_strncpyz(record->recordHolderNames[3], player4_name, sizeof(record->recordHolderNames[2]));
					record->recordHolderIpInts[3] = sqlite3_column_int(statement, k++);
					const char *player4_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
					if (VALIDSTRING(player4_cuid_hash2))
						Q_strncpyz(record->recordHolderCuids[3], player4_cuid_hash2, sizeof(record->recordHolderCuids[3]));
				}
				else {
					k += 2;
				}
			}
			else {
				k += 5;
			}
		}
		else {
			k += 6;
		}

		rc = trap_sqlite3_step( statement );(statement);
		++loaded;
		++j;
	}
	trap_sqlite3_finalize(statement);

	// write the remaining global fields
	//recordsToLoad->enabled = qtrue;

#ifdef _DEBUG
	G_Printf("Loaded %d capture time records from database %s flags %d (%s)\n", loaded, strict ? "matching" : "containing", flags, GetLongNameForRecordFlags(mapname, flags, qfalse));
#endif
}

void G_DBListAllMapsCaptureRecords(CaptureCategoryFlags flags, int limit, int offset, ListAllMapsCapturesCallback callback, void *context) {
	if (!flags) {
		assert(qfalse);
		return;
	}
	sqlite3_stmt* statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListBestSiegeFastcaps, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, flags);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	rc = trap_sqlite3_step( statement );(statement);
	while (rc == SQLITE_ROW) {
		int k = 0;
		const int thisRecordFlags = sqlite3_column_int(statement, k++);
		const char *mapname = (const char*)sqlite3_column_text(statement, k++);
		const char *player1_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player1_ip_int = sqlite3_column_int(statement, k++);
		const char *player1_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const char *player2_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player2_ip_int = sqlite3_column_int(statement, k++);
		const char *player2_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const char *player3_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player3_ip_int = sqlite3_column_int(statement, k++);
		const char *player3_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const char *player4_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player4_ip_int = sqlite3_column_int(statement, k++);
		const char *player4_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const int best_time = sqlite3_column_int(statement, k++);
		const time_t date = sqlite3_column_int64(statement, k++);

		callback(context, mapname, flags, thisRecordFlags, player1_name, player1_ip_int, player1_cuid_hash2, player2_name, player2_ip_int, player2_cuid_hash2, player3_name, player3_ip_int, player3_cuid_hash2, player4_name, player4_ip_int, player4_cuid_hash2, best_time, date);
		rc = trap_sqlite3_step( statement );(statement);
	}

	trap_sqlite3_finalize(statement);
}

void G_DBListLatestCaptureRecords(CaptureCategoryFlags flags, int limit, int offset, ListLastestCapturesCallback callback, void *context) {
	sqlite3_stmt* statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListLatestSiegeFastcaps, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, flags);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	rc = trap_sqlite3_step( statement );(statement);
	while (rc == SQLITE_ROW) {
		int k = 0;
		const int thisRecordFlags = sqlite3_column_int(statement, k++);
		const char *mapname = (const char*)sqlite3_column_text(statement, k++);
		const char *player1_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player1_ip_int = sqlite3_column_int(statement, k++);
		const char *player1_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const char *player2_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player2_ip_int = sqlite3_column_int(statement, k++);
		const char *player2_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const char *player3_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player3_ip_int = sqlite3_column_int(statement, k++);
		const char *player3_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const char *player4_name = (const char*)sqlite3_column_text(statement, k++);
		const unsigned int player4_ip_int = sqlite3_column_int(statement, k++);
		const char *player4_cuid_hash2 = (const char*)sqlite3_column_text(statement, k++);
		const int best_time = sqlite3_column_int(statement, k++);
		const time_t date = sqlite3_column_int64(statement, k++);
		const int rank = sqlite3_column_int(statement, k++);

		callback(context, mapname, flags, thisRecordFlags, player1_name, player1_ip_int, player1_cuid_hash2, player2_name, player2_ip_int, player2_cuid_hash2, player3_name, player3_ip_int, player3_cuid_hash2, player4_name, player4_ip_int, player4_cuid_hash2, best_time, date, rank);
		rc = trap_sqlite3_step( statement );(statement);
	}

	trap_sqlite3_finalize(statement);
}

static qboolean SaveCapturesForCategory(CaptureRecordsForCategory *recordsForCategory, void *mapname) {
	if (!recordsForCategory->flags) {
		assert(qfalse);
		return qtrue;
	}

	{
		// first, delete all of the old records for this category on this map, even those that didn't change
		sqlite3_stmt* statement;
		trap_sqlite3_prepare_v2(dbPtr, sqlremoveSiegeFastcaps, -1, &statement, 0);
		sqlite3_bind_text(statement, 1, mapname, -1, 0);
		sqlite3_bind_int(statement, 2, recordsForCategory->flags);
		int rc = trap_sqlite3_step( statement );(statement);
		if (rc != SQLITE_DONE)
			Com_Printf("SaveCapturesForCategory: error deleting\n");
		trap_sqlite3_finalize(statement);
	}

	int saved = 0;
	for (int j = 0; j < MAX_SAVED_RECORDS; ++j) {
		CaptureRecord *record = &recordsForCategory->records[j];
		if (!record->totalTime)
			continue; // not a valid record

		sqlite3_stmt *statement;
		trap_sqlite3_prepare_v2(dbPtr, sqlAddSiegeFastcapV2, -1, &statement, 0);

		int k = 1;
		sqlite3_bind_text(statement, k++, mapname, -1, 0);
		sqlite3_bind_int(statement, k++, recordsForCategory->flags);
		sqlite3_bind_int(statement, k++, j + 1); // rank
		sqlite3_bind_text(statement, k++, GetLongNameForRecordFlags(level.mapCaptureRecords.mapname, recordsForCategory->flags, qtrue), -1, 0);

		sqlite3_bind_text(statement, k++, record->recordHolderNames[0], -1, 0);
		sqlite3_bind_int(statement, k++, record->recordHolderIpInts[0]);
		if (VALIDSTRING(record->recordHolderCuids[0]))
			sqlite3_bind_text(statement, k++, record->recordHolderCuids[0], -1, 0);
		else
			sqlite3_bind_null(statement, k++);
		if (recordsForCategory->flags & CAPTURERECORDFLAG_DEFENSE) {
			sqlite3_bind_int(statement, k++, record->frags);
			sqlite3_bind_int(statement, k++, recordsForCategory->flags & CAPTURERECORDFLAG_FULLMAP ? record->objectivesCompleted : 0);
		}
		else {
			sqlite3_bind_int(statement, k++, record->maxSpeed1);
			sqlite3_bind_int(statement, k++, record->avgSpeed1);
		}

		if (VALIDSTRING(record->recordHolderNames[1])) {
			sqlite3_bind_text(statement, k++, record->recordHolderNames[1], -1, 0);
			sqlite3_bind_int(statement, k++, record->recordHolderIpInts[1]);
			if (VALIDSTRING(record->recordHolderCuids[1]))
				sqlite3_bind_text(statement, k++, record->recordHolderCuids[1], -1, 0);
			else
				sqlite3_bind_null(statement, k++);
		}
		else {
			for (int i = 0; i < 3; i++)
				sqlite3_bind_null(statement, k++);
		}

		for (int l = 0; l < MAX_SAVED_OBJECTIVES; l++) {
			if (recordsForCategory->flags & CAPTURERECORDFLAG_FULLMAP && record->objTimes[l] != -1)
				sqlite3_bind_int(statement, k++, record->objTimes[l]);
			else
				sqlite3_bind_null(statement, k++);
		}

		sqlite3_bind_int(statement, k++, record->totalTime);
		sqlite3_bind_int64(statement, k++, record->date);

		if (VALIDSTRING(record->matchId))
			sqlite3_bind_text(statement, k++, record->matchId, -1, 0);
		else
			sqlite3_bind_null(statement, k++);

		sqlite3_bind_int(statement, k++, record->recordHolder1ClientId);
		if (recordsForCategory->flags & CAPTURERECORDFLAG_COOP | CAPTURERECORDFLAG_SPEEDRUN)
			sqlite3_bind_int(statement, k++, record->recordHolder2ClientId);
		else
			sqlite3_bind_null(statement, k++);

		if (VALIDSTRING(record->recordHolderNames[1]) && VALIDSTRING(record->recordHolderNames[2])) {
			sqlite3_bind_text(statement, k++, record->recordHolderNames[2], -1, 0);
			sqlite3_bind_int(statement, k++, record->recordHolderIpInts[2]);
			if (VALIDSTRING(record->recordHolderCuids[2]))
				sqlite3_bind_text(statement, k++, record->recordHolderCuids[2], -1, 0);
			else
				sqlite3_bind_null(statement, k++);

			if (VALIDSTRING(record->recordHolderNames[3])) {
				sqlite3_bind_text(statement, k++, record->recordHolderNames[3], -1, 0);
				sqlite3_bind_int(statement, k++, record->recordHolderIpInts[3]);
				if (VALIDSTRING(record->recordHolderCuids[3]))
					sqlite3_bind_text(statement, k++, record->recordHolderCuids[3], -1, 0);
				else
					sqlite3_bind_null(statement, k++);
			}
			else {
				for (int i = 0; i < 3; i++)
					sqlite3_bind_null(statement, k++);
			}
		}
		else {
			for (int i = 0; i < 6; i++)
				sqlite3_bind_null(statement, k++);
		}

		int rc = trap_sqlite3_step( statement );(statement);
		if (rc != SQLITE_DONE)
			Com_Printf("SaveCapturesForCategory: error saving\n");
		trap_sqlite3_finalize(statement);
		++saved;
	}

#ifdef _DEBUG
	G_Printf("Saved %d fastcap records with flags %d (%s) to database\n", saved, recordsForCategory->flags, GetLongNameForRecordFlags(mapname, recordsForCategory->flags, qtrue));
#endif

	return qtrue;
}

void G_DBSaveCaptureRecords(CaptureRecordsContext *context)
{
	if (g_gametype.integer != GT_SIEGE || !g_saveCaptureRecords.integer || !context->changed) {
		return;
	}

	ListForEach(&context->captureRecordsList, SaveCapturesForCategory, context->mapname);
}

// =========== LOCKDOWN WHITELIST ==============================================

const char* const sqlCheckNMPlayerWhitelisted =
"SELECT COUNT(*) FROM playerwhitelist WHERE cuid_hash2 = ?1";

const char* const sqlCheckPlayerWhitelisted =
"SELECT COUNT(*) FROM playerwhitelist WHERE unique_id = ?1";

const char* const sqlAddUniqueIDAndCuidToWhitelist =
"INSERT INTO playerwhitelist (unique_id, cuid_hash2, name)                     "
"VALUES (?1,?2,?3)                                                             ";

const char* const sqlAddCuidToWhitelist =
"INSERT INTO playerwhitelist (cuid_hash2, name)                                 "
"VALUES (?1,?2)                                                                 ";

const char* const sqlAddUniqueIDToWhitelist =
"INSERT INTO playerwhitelist (unique_id, name)                                    "
"VALUES (?1,?2)                                                                   ";

const char* const sqlDeleteNMPlayerFromWhitelist =
"DELETE FROM playerwhitelist WHERE unique_id = ?1 OR cuid_hash2 = ?2";

const char* const sqlDeletePlayerFromWhitelist =
"DELETE FROM playerwhitelist WHERE unique_id = ?1";

int G_DBPlayerLockdownWhitelisted(unsigned long long uniqueID, const char* cuidHash) {
	sqlite3_stmt* statement;
	int rc;
	int whitelistedFlags = 0;

	if (VALIDSTRING(cuidHash)) {
		// check for cuid whitelist
		rc = trap_sqlite3_prepare_v2(dbPtr, sqlCheckNMPlayerWhitelisted, -1, &statement, 0);
		sqlite3_bind_text(statement, 1, cuidHash, -1, 0);
		rc = trap_sqlite3_step( statement );(statement);
		while (rc == SQLITE_ROW) {
			if (sqlite3_column_int(statement, 0) > 0)
				whitelistedFlags |= LOCKDOWNWHITELISTED_CUID;
			rc = trap_sqlite3_step( statement );(statement);
		}
		trap_sqlite3_reset(statement);
	}

	// check for ip whitelist
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlCheckPlayerWhitelisted, -1, &statement, 0);
	sqlite3_bind_int64(statement, 1, (signed long long)uniqueID);
	rc = trap_sqlite3_step( statement );(statement);
	while (rc == SQLITE_ROW) {
		if (sqlite3_column_int(statement, 0) > 0)
			whitelistedFlags |= LOCKDOWNWHITELISTED_ID;
		rc = trap_sqlite3_step( statement );(statement);
	}
	trap_sqlite3_finalize(statement);

	return whitelistedFlags;
}

void G_DBStorePlayerInLockdownWhitelist(unsigned long long uniqueID, const char* cuidHash, const char* name) {
	sqlite3_stmt* statement;
	int rc;

	if (uniqueID && VALIDSTRING(cuidHash)) {
		rc = trap_sqlite3_prepare_v2(dbPtr, sqlAddUniqueIDAndCuidToWhitelist, -1, &statement, 0);
		sqlite3_bind_int64(statement, 1, (signed long long)uniqueID);
		sqlite3_bind_text(statement, 2, cuidHash, -1, 0);
		sqlite3_bind_text(statement, 3, name, -1, 0);
		rc = trap_sqlite3_step( statement );(statement);
	}
	else if (uniqueID) {
		rc = trap_sqlite3_prepare_v2(dbPtr, sqlAddUniqueIDToWhitelist, -1, &statement, 0);
		sqlite3_bind_int64(statement, 1, (signed long long)uniqueID);
		sqlite3_bind_text(statement, 2, name, -1, 0);
		rc = trap_sqlite3_step( statement );(statement);
	}
	else if (VALIDSTRING(cuidHash)) {
		rc = trap_sqlite3_prepare_v2(dbPtr, sqlAddCuidToWhitelist, -1, &statement, 0);
		sqlite3_bind_text(statement, 1, cuidHash, -1, 0);
		sqlite3_bind_text(statement, 2, name, -1, 0);
		rc = trap_sqlite3_step( statement );(statement);
	}

	trap_sqlite3_finalize(statement);
}

void G_DBRemovePlayerFromLockdownWhitelist(unsigned long long uniqueID, const char* cuidHash) {
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, VALIDSTRING(cuidHash) ? sqlDeleteNMPlayerFromWhitelist : sqlDeletePlayerFromWhitelist, -1, &statement, 0);

	sqlite3_bind_int64(statement, 1, (signed long long)uniqueID);
	if (VALIDSTRING(cuidHash))
		sqlite3_bind_text(statement, 2, cuidHash, -1, 0);

	rc = trap_sqlite3_step( statement );(statement);

	trap_sqlite3_finalize(statement);
}

// =========== WHITELIST =======================================================

const char* const sqlIsIpWhitelisted =
"SELECT COUNT(*)                                                             \n"
"FROM ip_whitelist                                                           \n"
"WHERE( ip_int & mask_int ) = (? & mask_int)                                   ";

const char* const sqlAddToWhitelist =
"INSERT INTO ip_whitelist (ip_int, mask_int, notes)                          \n"
"VALUES (?,?,?)                                                                ";

const char* const sqlremoveFromWhitelist =
"DELETE FROM ip_whitelist                                                    \n"
"WHERE ip_int = ?                                                            \n"
"AND mask_int = ?                                                              ";

qboolean G_DBIsFilteredByWhitelist( unsigned int ip,
	char* reasonBuffer,
	int reasonBufferSize )
{
	qboolean filtered = qfalse;

	// check if ip is on white list
	sqlite3_stmt* statement;

	// prepare whitelist check statement
	trap_sqlite3_prepare_v2( dbPtr, sqlIsIpWhitelisted, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );

	trap_sqlite3_step( statement );( statement );
	int count = sqlite3_column_int( statement, 0 );

	if ( count == 0 )
	{
		Q_strncpyz( reasonBuffer, "IP address not on whitelist", reasonBufferSize );
		filtered = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return filtered;
}

qboolean G_DBAddToWhitelist( unsigned int ip,
	unsigned int mask,
	const char* notes )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlAddToWhitelist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	sqlite3_bind_text( statement, 3, notes, -1, 0 );

	rc = trap_sqlite3_step( statement );( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBRemoveFromWhitelist( unsigned int ip,
	unsigned int mask )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlremoveFromWhitelist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	rc = trap_sqlite3_step( statement );( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

	return success;
}

// =========== BLACKLIST =======================================================

const char* const sqlIsIpBlacklisted =
"SELECT reason                                                               \n"
"FROM ip_blacklist                                                           \n"
"WHERE( ip_int & mask_int ) = (? & mask_int)                                 \n"
"AND banned_until >= datetime('now')                                           ";

const char* const sqlListBlacklist =
"SELECT ip_int, mask_int, notes, reason, banned_since, banned_until          \n"
"FROM ip_blacklist                                                             ";

const char* const sqlAddToBlacklist =
"INSERT INTO ip_blacklist (ip_int,                                           \n"
"mask_int, notes, reason, banned_since, banned_until)                        \n"
"VALUES (?,?,?,?,datetime('now'),datetime('now','+'||?||' hours'))             ";

const char* const sqlRemoveFromBlacklist =
"DELETE FROM ip_blacklist                                                    \n"
"WHERE ip_int = ?                                                            \n"
"AND mask_int = ?                                                              ";

qboolean G_DBIsFilteredByBlacklist( unsigned int ip,
	char* reasonBuffer,
	int reasonBufferSize )
{
	qboolean filtered = qfalse;

	sqlite3_stmt* statement;

	// prepare blacklist check statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlIsIpBlacklisted, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );

	rc = trap_sqlite3_step( statement );( statement );

	// blacklisted => we forbid it
	if ( rc == SQLITE_ROW )
	{
		const char* reason = ( const char* )sqlite3_column_text( statement, 0 );
		const char* prefix = "Banned: ";
		int prefixSize = strlen( prefix );

		Q_strncpyz( reasonBuffer, prefix, reasonBufferSize );
		Q_strncpyz( &reasonBuffer[prefixSize], reason, reasonBufferSize - prefixSize );
		filtered = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return filtered;
}

void G_DBListBlacklist( BlackListCallback callback )
{
	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlListBlacklist, -1, &statement, 0 );

	rc = trap_sqlite3_step( statement );( statement );
	while ( rc == SQLITE_ROW )
	{
		unsigned int ip = sqlite3_column_int( statement, 0 );
		unsigned int mask = sqlite3_column_int( statement, 1 );
		const char* notes = ( const char* )sqlite3_column_text( statement, 2 );
		const char* reason = ( const char* )sqlite3_column_text( statement, 3 );
		const char* banned_since = ( const char* )sqlite3_column_text( statement, 4 );
		const char* banned_until = ( const char* )sqlite3_column_text( statement, 5 );

		callback( ip, mask, notes, reason, banned_since, banned_until );

		rc = trap_sqlite3_step( statement );( statement );
	}

	trap_sqlite3_finalize( statement );
}

qboolean G_DBAddToBlacklist( unsigned int ip,
	unsigned int mask,
	const char* notes,
	const char* reason,
	int hours )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlAddToBlacklist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	sqlite3_bind_text( statement, 3, notes, -1, 0 );
	sqlite3_bind_text( statement, 4, reason, -1, 0 );

	sqlite3_bind_int( statement, 5, hours );

	rc = trap_sqlite3_step( statement );( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBRemoveFromBlacklist( unsigned int ip,
	unsigned int mask )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlRemoveFromBlacklist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	rc = trap_sqlite3_step( statement );( statement );

	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

	return success;
}

// =========== MAP POOLS =======================================================

const char* const sqlListPools =
"SELECT pool_id, short_name, long_name                                       \n"
"FROM pools                                                                    ";

const char* const sqlListMapsInPool =
"SELECT long_name, pools.pool_id, mapname, weight                            \n"
"FROM pools                                                                  \n"
"JOIN pool_has_map                                                           \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ? AND mapname <> ?                                         ";

const char* const sqlFindPool =
"SELECT pools.pool_id, long_name                                             \n"
"FROM pools                                                                  \n"
"JOIN                                                                        \n"
"pool_has_map                                                                \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ?                                                          ";

const char* const sqlCreatePool =
"INSERT INTO pools (short_name, long_name)                                   \n"
"VALUES (?,?)                                                                  ";

const char* const sqlDeleteAllMapsInPool =
"DELETE FROM pool_has_map                                                    \n"
"WHERE pool_id                                                               \n"
"IN                                                                          \n"
"( SELECT pools.pool_id                                                      \n"
"FROM pools                                                                  \n"
"JOIN pool_has_map                                                           \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ? )                                                        ";

const char* const sqlDeletePool =
"DELETE FROM pools                                                           \n"
"WHERE short_name = ?;                                                         ";

const char* const sqlAddMapToPool =
"INSERT INTO pool_has_map (pool_id, mapname, weight)                         \n"
"SELECT pools.pool_id, ?, ?                                                  \n"
"FROM pools                                                                  \n"
"WHERE short_name = ?                                                          ";

const char* const sqlRemoveMapToPool =
"DELETE FROM pool_has_map                                                    \n"
"WHERE pool_id                                                               \n"
"IN                                                                          \n"
"( SELECT pools.pool_id                                                      \n"
"FROM pools                                                                  \n"
"JOIN pool_has_map                                                           \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ? )                                                      \n"
"AND mapname = ? ;                                                             ";

void G_DBListPools( ListPoolCallback callback,
	void* context )
{
	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlListPools, -1, &statement, 0 );

	rc = trap_sqlite3_step( statement );( statement );
	while ( rc == SQLITE_ROW )
	{
		int pool_id = sqlite3_column_int( statement, 0 );
		const char* short_name = ( const char* )sqlite3_column_text( statement, 1 );
		const char* long_name = ( const char* )sqlite3_column_text( statement, 2 );

		callback( context, pool_id, short_name, long_name );

		rc = trap_sqlite3_step( statement );( statement );
	}

	trap_sqlite3_finalize( statement );
}

void G_DBListMapsInPool( const char* short_name,
	const char* ignore,
	ListMapsPoolCallback callback,
	void* context,
	char *longNameOut,
	size_t longNameOutSize)
{
	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlListMapsInPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, ignore, -1, 0 ); // ignore map, we 

	rc = trap_sqlite3_step( statement );( statement );
	while ( rc == SQLITE_ROW )
	{
		const char* long_name = ( const char* )sqlite3_column_text( statement, 0 );
		if (VALIDSTRING(long_name) && longNameOut && longNameOutSize)
			Q_strncpyz(longNameOut, long_name, longNameOutSize);
		int pool_id = sqlite3_column_int( statement, 1 );
		const char* mapname = ( const char* )sqlite3_column_text( statement, 2 );
		int weight = sqlite3_column_int( statement, 3 );
		if ( weight < 1 ) weight = 1;

		if ( Q_stricmp( mapname, ignore ) ) {
			callback( context, long_name, pool_id, mapname, weight );
		}

		rc = trap_sqlite3_step( statement );( statement );
	}

	trap_sqlite3_finalize( statement );
}

qboolean G_DBFindPool( const char* short_name,
	PoolInfo* poolInfo )
{
	qboolean found = qfalse;

	sqlite3_stmt* statement;

	// prepare blacklist check statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlFindPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );

	rc = trap_sqlite3_step( statement );( statement );

	// blacklisted => we forbid it
	if ( rc == SQLITE_ROW )
	{
		int pool_id = sqlite3_column_int( statement, 0 );
		const char* long_name = ( const char* )sqlite3_column_text( statement, 1 );

		Q_strncpyz( poolInfo->long_name, long_name, sizeof( poolInfo->long_name ) );
		poolInfo->pool_id = pool_id;

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return found;
}

typedef struct CumulativeMapWeight {
	char mapname[MAX_MAP_NAME];
	int cumulativeWeight; // the sum of all previous weights in the list and the current one
	struct CumulativeMapWeight *next;
} CumulativeMapWeight;

static void BuildCumulativeWeight( void** context,
	const char* long_name,
	int pool_id,
	const char* mapname,
	int mapWeight )
{
	CumulativeMapWeight **cdf = ( CumulativeMapWeight** )context;

	// first, create the current node using parameters
	CumulativeMapWeight *currentNode = malloc( sizeof( *currentNode ) );
	Q_strncpyz( currentNode->mapname, mapname, sizeof( currentNode->mapname ) );
	currentNode->cumulativeWeight = mapWeight;
	currentNode->next = NULL;

	if ( !*cdf ) {
		// this is the first item, just assign it
		*cdf = currentNode;
	}
	else {
		// otherwise, add it to the end of the list
		CumulativeMapWeight *n = *cdf;

		while ( n->next ) {
			n = n->next;
		}

		currentNode->cumulativeWeight += n->cumulativeWeight; // add the weight of the previous node
		n->next = currentNode;
	}
}

qboolean G_DBSelectMapsFromPool( const char* short_name,
	const char* ignoreMap,
	const int mapsToRandomize,
	MapSelectedCallback callback,
	void* context )
{
	PoolInfo poolInfo;
	if ( G_DBFindPool( short_name, &poolInfo ) )
	{
		// fill the cumulative density function of the map pool
		CumulativeMapWeight *cdf = NULL;
		G_DBListMapsInPool( short_name, ignoreMap, BuildCumulativeWeight, ( void * )cdf, NULL, 0 );

		if ( cdf ) {
			CumulativeMapWeight *n = cdf;
			int i, numMapsInList = 0;

			while ( n ) {
				++numMapsInList; // if we got here, we have at least 1 map, so this will always be at least 1
				n = n->next;
			}

			// pick as many maps as needed from the pool while there are enough in the list
			for ( i = 0; i < mapsToRandomize && numMapsInList > 0; ++i ) {
				// first, get a random number based on the highest cumulative weight
				n = cdf;
				while ( n->next ) {
					n = n->next;
				}
				const int random = rand() % n->cumulativeWeight;

				// now, pick the map
				n = cdf;
				while ( n ) {
					if ( random < n->cumulativeWeight ) {
						break; // got it
					}
					n = n->next;
				}

				// let the caller do whatever they want with the map we picked
				callback( context, n->mapname );

				// delete the map from the cdf for further iterations
				CumulativeMapWeight *nodeToDelete = n;
				int weightToDelete;

				// get the node right before the node to delete and find the weight to delete
				if ( nodeToDelete == cdf ) {
					n = NULL;
					weightToDelete = cdf->cumulativeWeight; // the head's weight is equal to its cumulative weight
				}
				else {
					n = cdf;
					while ( n->next && n->next != nodeToDelete ) {
						n = n->next;
					}
					weightToDelete = nodeToDelete->cumulativeWeight - n->cumulativeWeight;
				}

				// relink the list
				if ( !n ) {
					cdf = cdf->next; // we are deleting the head, so just rebase it (may be NULL if this was the last remaining element)
				}
				else {
					n->next = nodeToDelete->next; // may be NULL if we are deleting the last element in the list
				}
				--numMapsInList;

				// rebuild the cumulative weights
				n = nodeToDelete->next;
				while ( n ) {
					n->cumulativeWeight -= weightToDelete;
					n = n->next;
				}

				free( nodeToDelete );
			}

			// free the remaining resources
			n = cdf;
			while ( n ) {
				CumulativeMapWeight *nodeToDelete = n;
				n = n->next;
				free( nodeToDelete );
			}

			return qtrue;
		}
	}

	return qfalse;
}

qboolean G_DBPoolCreate( const char* short_name,
	const char* long_name )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlCreatePool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, long_name, -1, 0 );

	rc = trap_sqlite3_step( statement );( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolDeleteAllMaps( const char* short_name )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlDeleteAllMapsInPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );

	rc = trap_sqlite3_step( statement );( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolDelete( const char* short_name )
{
	qboolean success = qfalse;

	if ( G_DBPoolDeleteAllMaps( short_name ) )
	{
		sqlite3_stmt* statement;
		// prepare insert statement
		int rc = trap_sqlite3_prepare_v2( dbPtr, sqlDeletePool, -1, &statement, 0 );

		sqlite3_bind_text( statement, 1, short_name, -1, 0 );

		rc = trap_sqlite3_step( statement );( statement );
		if ( rc == SQLITE_DONE )
		{
			int changes = sqlite3_changes( dbPtr );
			if ( changes != 0 )
			{
				success = qtrue;
			}
		}

		trap_sqlite3_finalize( statement );
	}

	return success;
}

qboolean G_DBPoolMapAdd( const char* short_name,
	const char* mapname,
	int weight )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlAddMapToPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, mapname, -1, 0 );
	sqlite3_bind_int( statement, 2, weight );
	sqlite3_bind_text( statement, 3, short_name, -1, 0 );

	rc = trap_sqlite3_step( statement );( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolMapRemove( const char* short_name,
	const char* mapname )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlRemoveMapToPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, mapname, -1, 0 );

	rc = trap_sqlite3_step( statement );( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

	return success;
}
