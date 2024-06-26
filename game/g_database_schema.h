#pragma once

const char* const sqlCreateTables =
"CREATE TABLE IF NOT EXISTS [metadata] (                                     \n"
"  [key] TEXT COLLATE NOCASE PRIMARY KEY NOT NULL,                           \n"
"  [value] TEXT DEFAULT NULL                                                 \n"
");                                                                          \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS [accounts] (                                     \n"
"    [account_id] INTEGER NOT NULL,                                          \n"
"    [name] TEXT COLLATE NOCASE NOT NULL,                                    \n"
"    [created_on] INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),          \n"
"    [usergroup] TEXT DEFAULT NULL,                                          \n"
"    [flags] INTEGER NOT NULL DEFAULT 0,                                     \n"
"    PRIMARY KEY ( [account_id] ),                                           \n"
"    UNIQUE ( [name] )                                                       \n"
");                                                                          \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS [sessions] (                                     \n"
"    [session_id] INTEGER NOT NULL,                                          \n"
"    [identifier] INTEGER NOT NULL,                                          \n"
"    [info] TEXT NOT NULL,                                                   \n"
"    [account_id] INTEGER DEFAULT NULL,                                      \n"
"    PRIMARY KEY ( [session_id] ),                                           \n"
"    UNIQUE ( [identifier] ),                                                \n"
"    FOREIGN KEY ( [account_id] )                                            \n"
"        REFERENCES accounts ( [account_id] )                                \n"
"        ON DELETE SET NULL                                                  \n"
");                                                                          \n"
"                                                                            \n"
"CREATE VIEW IF NOT EXISTS sessions_info AS                                  \n"
"SELECT                                                                      \n"
"    session_id,                                                             \n"
"    identifier,                                                             \n"
"    info,                                                                   \n"
"    account_id,                                                             \n"
"    CASE WHEN (                                                             \n"
"	     sessions.account_id IS NOT NULL                                     \n"
//"	     OR EXISTS( SELECT 1 FROM test WHERE test.session_id = sessions.session_id )\n"
"    )                                                                       \n"
"    THEN 1 ELSE 0 END referenced                                            \n"
"FROM sessions;                                                              \n"
"                                                                            \n"
"CREATE TRIGGER IF NOT EXISTS sessions_info_delete_trigger                   \n"
"INSTEAD OF DELETE ON sessions_info                                          \n"
"BEGIN                                                                       \n"
"    DELETE FROM sessions WHERE sessions.session_id = OLD.session_id;        \n"
"END;                                                                        \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS nicknames (                                      \n"
"    [ip_int] INTEGER,                                                       \n"
"    [name] TEXT,                                                            \n"
"    [duration] INTEGER,                                                     \n"
"    [cuid_hash2] TEXT                                                       \n"
");                                                                          \n"
"                                                                            \n"
"CREATE INDEX IF NOT EXISTS nicknames_ip_int_idx                             \n"
"ON nicknames ( ip_int );                                                    \n"
"                                                                            \n"
"CREATE INDEX IF NOT EXISTS nicknames_cuid_hash2_idx                         \n"
"ON nicknames ( cuid_hash2 );                                                \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS playerwhitelist(                                 \n"
"    [unique_id] BIGINT,                                                     \n"
"    [cuid_hash2] TEXT,                                                      \n"
"    [name] TEXT);                                                           \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS siegefastcaps (                                  \n"
"    [fastcap_id] INTEGER PRIMARY KEY AUTOINCREMENT,                         \n"
"    [mapname] TEXT,                                                         \n"
"    [flags] INTEGER,                                                        \n"
"    [rank] INTEGER,                                                         \n"
"    [desc] TEXT,                                                            \n"
"    [player1_name] TEXT,                                                    \n"
"    [player1_ip_int] INTEGER,                                               \n"
"    [player1_cuid_hash2] TEXT,                                              \n"
"    [max_speed] INTEGER,                                                    \n"
"    [avg_speed] INTEGER,                                                    \n"
"    [player2_name] TEXT,                                                    \n"
"    [player2_ip_int] INTEGER,                                               \n"
"    [player2_cuid_hash2] TEXT,                                              \n"
"    [obj1_time] INTEGER,                                                    \n"
"    [obj2_time] INTEGER,                                                    \n"
"    [obj3_time] INTEGER,                                                    \n"
"    [obj4_time] INTEGER,                                                    \n"
"    [obj5_time] INTEGER,                                                    \n"
"    [obj6_time] INTEGER,                                                    \n"
"    [obj7_time] INTEGER,                                                    \n"
"    [obj8_time] INTEGER,                                                    \n"
"    [obj9_time] INTEGER,                                                    \n"
"    [total_time] INTEGER,                                                   \n"
"    [date] INTEGER,                                                         \n"
"    [match_id] TEXT,                                                        \n"
"    [player1_client_id] INTEGER,                                            \n"
"    [player2_client_id] INTEGER,                                            \n"
"    [player3_name] TEXT,                                                    \n"
"    [player3_ip_int] INTEGER,                                               \n"
"    [player3_cuid_hash2] TEXT,                                              \n"
"    [player4_name] TEXT,                                                    \n"
"    [player4_ip_int] INTEGER,                                               \n"
"    [player4_cuid_hash2] TEXT);                                             \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS ip_whitelist (                                   \n"
"    [ip_int] INTEGER PRIMARY KEY,                                           \n"
"    [mask_int] INTEGER,                                                     \n"
"    [notes] TEXT);                                                          \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS ip_blacklist (                                   \n"
"    [ip_int] INTEGER PRIMARY KEY,                                           \n"
"    [mask_int] INTEGER,                                                     \n"
"    [notes] TEXT,                                                           \n"
"    [reason] TEXT,                                                          \n"
"    [banned_since] DATETIME,                                                \n"
"    [banned_until] DATETIME);                                               \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS pools (                                          \n"
"    [pool_id] INTEGER PRIMARY KEY AUTOINCREMENT,                            \n"
"    [short_name] TEXT,                                                      \n"
"    [long_name] TEXT );                                                     \n"
"                                                                            \n"
"CREATE TABLE IF NOT EXISTS pool_has_map (                                   \n"
"    [pool_id] INTEGER REFERENCES [pools]([pool_id]) ON DELETE RESTRICT,     \n"
"    [mapname] TEXT,                                                         \n"
"    [weight] INTEGER);                                                        ";
