// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "g_ICARUScb.h"
#include "g_nav.h"
#include "bg_saga.h"
#include "sodium.h"

//#include "accounts.h"
#include "jp_engine.h"
#include "g_database.h"

#include "kdtree.h"

level_locals_t	level;

static char* getCurrentTime();

int		eventClearTime = 0;
static int navCalcPathTime = 0;
extern int fatalErrors;


int killPlayerTimer = 0;

typedef struct {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	int			cvarFlags;
	int			modificationCount;  // for tracking changes
	qboolean	trackChange;	    // track this variable, and announce if changed
  qboolean teamShader;        // track and if changed, update shader state
} cvarTable_t;

gentity_t		g_entities[MAX_GENTITIES];
gclient_t		g_clients[MAX_CLIENTS];

qboolean gDuelExit = qfalse;

vmCvar_t	g_trueJedi;

vmCvar_t	g_wasIntermission;

vmCvar_t	g_gametype;
vmCvar_t	g_MaxHolocronCarry;
vmCvar_t	g_ff_objectives;
vmCvar_t	g_autoMapCycle;
vmCvar_t	g_dmflags;
vmCvar_t	g_maxForceRank;
vmCvar_t	g_forceBasedTeams;
vmCvar_t	g_privateDuel;

vmCvar_t	g_duelLifes;
vmCvar_t	g_duelShields;

vmCvar_t	g_chatLimit;
vmCvar_t	g_teamChatLimit;
vmCvar_t	g_voiceChatLimit;

vmCvar_t	g_allowNPC;

vmCvar_t	g_armBreakage;

vmCvar_t	g_saberLocking;
vmCvar_t	g_saberLockFactor;
vmCvar_t	g_saberTraceSaberFirst;

vmCvar_t	d_saberKickTweak;

vmCvar_t	d_powerDuelPrint;

vmCvar_t	d_saberGhoul2Collision;
vmCvar_t	g_saberBladeFaces;
vmCvar_t	d_saberAlwaysBoxTrace;
vmCvar_t	d_saberBoxTraceSize;

vmCvar_t	d_siegeSeekerNPC;

vmCvar_t	g_debugMelee;
vmCvar_t	g_stepSlideFix;

vmCvar_t	g_noSpecMove;

#if 0//#ifdef _DEBUG
vmCvar_t	g_disableServerG2;
#endif

vmCvar_t	d_perPlayerGhoul2;

vmCvar_t	d_projectileGhoul2Collision;

vmCvar_t	g_g2TraceLod;

vmCvar_t	g_optvehtrace;

vmCvar_t	g_locationBasedDamage;

vmCvar_t	g_fixSaberDefense;
// 1 = angle-based blocking for disrupts
// 2 = also fix projectile blocking

vmCvar_t	g_saberDefense1Angle;
vmCvar_t	g_saberDefense2Angle;
vmCvar_t	g_saberDefense3Angle;
vmCvar_t	g_saberDefenseDebug;

vmCvar_t	g_saberHitsToKillSentry;

vmCvar_t	g_allowHighPingDuelist;

vmCvar_t	g_logClientInfo;

vmCvar_t	g_slowmoDuelEnd;

vmCvar_t	g_saberDamageScale;

vmCvar_t	g_useWhileThrowing;

vmCvar_t	g_RMG;

vmCvar_t	g_svfps;
vmCvar_t	g_forceRegenTime;
vmCvar_t	g_spawnInvulnerability;
vmCvar_t	g_forcePowerDisable;
vmCvar_t	g_weaponDisable;
vmCvar_t	g_duelWeaponDisable;
vmCvar_t	g_allowDuelSuicide;
vmCvar_t	g_fraglimitVoteCorrection;
vmCvar_t	g_fraglimit;
vmCvar_t	g_duel_fraglimit;
vmCvar_t	g_timelimit;
vmCvar_t	g_capturelimit;
vmCvar_t	g_capturedifflimit;
vmCvar_t	d_saberInterpolate;
vmCvar_t	g_friendlyFire;
vmCvar_t	g_friendlySaber;
vmCvar_t	g_password;
vmCvar_t	sv_privatepassword;
vmCvar_t	g_needpass;
vmCvar_t	g_maxclients;
vmCvar_t	g_maxGameClients;
vmCvar_t	g_dedicated;
vmCvar_t	g_developer;
vmCvar_t	g_speed;
vmCvar_t	g_gravity;
vmCvar_t	g_cheats;
vmCvar_t	g_knockback;
vmCvar_t	g_forcerespawn;
vmCvar_t	g_siegeRespawn;
vmCvar_t	g_inactivity;
vmCvar_t	g_inactivityKick;
vmCvar_t	g_spectatorInactivity;
vmCvar_t	g_debugMove;
vmCvar_t	g_accounts;
vmCvar_t	g_accountsFile; 
vmCvar_t	g_whitelist;
vmCvar_t	g_dlURL;  
vmCvar_t	cl_allowDownload;
vmCvar_t	g_logrcon;   
vmCvar_t	g_flags_overboarding;
vmCvar_t    g_sexyDisruptor;
vmCvar_t    g_fixSiegeScoring;
vmCvar_t    g_fixFallingSounds;
vmCvar_t    g_nextmapWarning;
vmCvar_t    g_floatingItems;
vmCvar_t    g_rocketSurfing;
vmCvar_t    g_improvedTeamchat;
vmCvar_t    g_enableCloak;
vmCvar_t    g_fixHothBunkerLift;
vmCvar_t    g_fixDesertStationsModels;
vmCvar_t    g_infiniteCharge;
vmCvar_t    g_siegeTiebreakEnd;
vmCvar_t    g_moreTaunts;
vmCvar_t    g_fixRancorCharge;
vmCvar_t    g_autoKorribanFloatingItems;
vmCvar_t    g_autoKorribanSpam;
vmCvar_t	g_forceDTechItems;
vmCvar_t    g_antiHothCodesLiftLame;
vmCvar_t    g_antiHothHangarLiftLame;
vmCvar_t	g_antiHothInfirmaryLiftLame;
vmCvar_t    g_requireMoreCustomTeamVotes;
vmCvar_t	g_antiCallvoteTakeover;
vmCvar_t	g_autoResetCustomTeams;
vmCvar_t    g_fixEweb;
vmCvar_t	g_fixVoiceChat;
vmCvar_t	g_botJumping;
vmCvar_t	g_fixHothDoorSounds;
vmCvar_t	iLikeToDoorSpam;
vmCvar_t	iLikeToMineSpam;
vmCvar_t	iLikeToShieldSpam;
vmCvar_t	autocfg_map;
vmCvar_t	autocfg_unknown;
vmCvar_t	g_swoopKillPoints;
vmCvar_t	g_teamVoteFix;
vmCvar_t	g_antiLaming;
vmCvar_t	g_probation;
vmCvar_t	g_teamOverlayUpdateRate;
vmCvar_t	g_lockdown;
vmCvar_t	siegeStatus;
vmCvar_t	g_hothRebalance;
vmCvar_t	g_hothHangarHack;
vmCvar_t	g_fixShield;
vmCvar_t	g_delayClassUpdate;
vmCvar_t	g_defaultMap;
vmCvar_t	g_multiVoteRNG;
vmCvar_t	g_runoffVote;
vmCvar_t	g_antiSelfMax;
vmCvar_t	g_improvedDisarm;
vmCvar_t	g_flechetteSpread;
vmCvar_t	g_autoSpec;
vmCvar_t	g_intermissionKnockbackNPCs;
vmCvar_t	g_emotes;
vmCvar_t	g_siegeHelp;
vmCvar_t	g_fixHoth2ndObj;
vmCvar_t	g_hothInfirmaryRebalance;
vmCvar_t	g_siegeTimeVisualAid;
vmCvar_t	g_botAimbot;
vmCvar_t	g_botDefaultSiegeClass;
vmCvar_t	g_speedrunRoundOneRestart;
vmCvar_t	g_siegeGhosting;
vmCvar_t	g_fixHothHangarTurrets;
vmCvar_t	g_removeHothHangarTurrets;
vmCvar_t	g_fixLiftkillTraps;
vmCvar_t	g_fixDempSaberThrow;
vmCvar_t	g_fixDodge;
vmCvar_t	g_joinMenuHack;

vmCvar_t	g_preventJoiningLargerTeam;
vmCvar_t	g_lastIntermissionStartTime;
vmCvar_t	g_autoRestartAfterIntermission;

vmCvar_t	g_saberThrowDefenseSmallAngle;
vmCvar_t	g_saberThrowDefenseLargeAngle;

vmCvar_t	g_saberThrowDefenseCompromisedBonus;
vmCvar_t	g_saberThrowDefensePushPullBonus;

vmCvar_t	g_saberThrowDefenseSmallDamage;
vmCvar_t	g_saberThrowDefenseLargeDamage;

vmCvar_t	g_saberThrowDefenseRewind;

vmCvar_t	g_webhookId;
vmCvar_t	g_webhookToken;

vmCvar_t	g_teamOverlayForce;

vmCvar_t	g_improvedHoming;
vmCvar_t	g_improvedHomingThreshold;
vmCvar_t	d_debugImprovedHoming;
vmCvar_t	g_braindeadBots;
vmCvar_t	g_siegeRespawnAutoChange;
vmCvar_t	g_quickPauseChat;
vmCvar_t	g_multiUseGenerators;
vmCvar_t	g_dispenserLifetime;
vmCvar_t	g_techAmmoForAllWeapons;
vmCvar_t	g_damageFixes;
vmCvar_t	g_healWalkerWithAmmoCans;
vmCvar_t	g_unlagged;
vmCvar_t	g_friendlyFreeze;
#ifdef _DEBUG
vmCvar_t	g_unlaggedMaxCompensation;
vmCvar_t	g_unlaggedSkeletonTime;
vmCvar_t	g_unlaggedFactor;
vmCvar_t	g_unlaggedOffset;
vmCvar_t	g_unlaggedDebug;
#endif

vmCvar_t	lastMapName;

vmCvar_t	g_customVotes;
vmCvar_t	g_customVote1_command;
vmCvar_t	g_customVote1_label;
vmCvar_t	g_customVote2_command;
vmCvar_t	g_customVote2_label;
vmCvar_t	g_customVote3_command;
vmCvar_t	g_customVote3_label;
vmCvar_t	g_customVote4_command;
vmCvar_t	g_customVote4_label;
vmCvar_t	g_customVote5_command;
vmCvar_t	g_customVote5_label;
vmCvar_t	g_customVote6_command;
vmCvar_t	g_customVote6_label;
vmCvar_t	g_customVote7_command;
vmCvar_t	g_customVote7_label;
vmCvar_t	g_customVote8_command;
vmCvar_t	g_customVote8_label;
vmCvar_t	g_customVote9_command;
vmCvar_t	g_customVote9_label;
vmCvar_t	g_customVote10_command;
vmCvar_t	g_customVote10_label;

vmCvar_t	g_skillboost1_damageDealtBonus;
vmCvar_t	g_skillboost1_dempDamageDealtBonus;
vmCvar_t	g_skillboost1_damageTakenReduction;
vmCvar_t	g_skillboost1_forceRegenBonus;
vmCvar_t	g_skillboost1_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost1_movementSpeedBonus;
vmCvar_t	g_skillboost1_selfDamageFactorOverride;
vmCvar_t	g_skillboost1_splashRadiusBonus;

vmCvar_t	g_skillboost2_damageDealtBonus;
vmCvar_t	g_skillboost2_dempDamageDealtBonus;
vmCvar_t	g_skillboost2_damageTakenReduction;
vmCvar_t	g_skillboost2_forceRegenBonus;
vmCvar_t	g_skillboost2_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost2_movementSpeedBonus;
vmCvar_t	g_skillboost2_selfDamageFactorOverride;
vmCvar_t	g_skillboost2_splashRadiusBonus;

vmCvar_t	g_skillboost3_damageDealtBonus;
vmCvar_t	g_skillboost3_dempDamageDealtBonus;
vmCvar_t	g_skillboost3_damageTakenReduction;
vmCvar_t	g_skillboost3_forceRegenBonus;
vmCvar_t	g_skillboost3_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost3_movementSpeedBonus;
vmCvar_t	g_skillboost3_selfDamageFactorOverride;
vmCvar_t	g_skillboost3_splashRadiusBonus;

vmCvar_t	g_skillboost4_damageDealtBonus;
vmCvar_t	g_skillboost4_dempDamageDealtBonus;
vmCvar_t	g_skillboost4_damageTakenReduction;
vmCvar_t	g_skillboost4_forceRegenBonus;
vmCvar_t	g_skillboost4_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost4_movementSpeedBonus;
vmCvar_t	g_skillboost4_selfDamageFactorOverride;
vmCvar_t	g_skillboost4_splashRadiusBonus;

vmCvar_t	g_skillboost5_damageDealtBonus;
vmCvar_t	g_skillboost5_dempDamageDealtBonus;
vmCvar_t	g_skillboost5_damageTakenReduction;
vmCvar_t	g_skillboost5_forceRegenBonus;
vmCvar_t	g_skillboost5_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost5_movementSpeedBonus;
vmCvar_t	g_skillboost5_selfDamageFactorOverride;
vmCvar_t	g_skillboost5_splashRadiusBonus;

vmCvar_t	g_skillboost6_damageDealtBonus;
vmCvar_t	g_skillboost6_dempDamageDealtBonus;
vmCvar_t	g_skillboost6_damageTakenReduction;
vmCvar_t	g_skillboost6_forceRegenBonus;
vmCvar_t	g_skillboost6_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost6_movementSpeedBonus;
vmCvar_t	g_skillboost6_selfDamageFactorOverride;
vmCvar_t	g_skillboost6_splashRadiusBonus;

vmCvar_t	g_skillboost7_damageDealtBonus;
vmCvar_t	g_skillboost7_dempDamageDealtBonus;
vmCvar_t	g_skillboost7_damageTakenReduction;
vmCvar_t	g_skillboost7_forceRegenBonus;
vmCvar_t	g_skillboost7_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost7_movementSpeedBonus;
vmCvar_t	g_skillboost7_selfDamageFactorOverride;
vmCvar_t	g_skillboost7_splashRadiusBonus;

vmCvar_t	g_skillboost8_damageDealtBonus;
vmCvar_t	g_skillboost8_dempDamageDealtBonus;
vmCvar_t	g_skillboost8_damageTakenReduction;
vmCvar_t	g_skillboost8_forceRegenBonus;
vmCvar_t	g_skillboost8_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost8_movementSpeedBonus;
vmCvar_t	g_skillboost8_selfDamageFactorOverride;
vmCvar_t	g_skillboost8_splashRadiusBonus;

vmCvar_t	g_skillboost9_damageDealtBonus;
vmCvar_t	g_skillboost9_dempDamageDealtBonus;
vmCvar_t	g_skillboost9_damageTakenReduction;
vmCvar_t	g_skillboost9_forceRegenBonus;
vmCvar_t	g_skillboost9_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost9_movementSpeedBonus;
vmCvar_t	g_skillboost9_selfDamageFactorOverride;
vmCvar_t	g_skillboost9_splashRadiusBonus;

vmCvar_t	g_skillboost10_damageDealtBonus;
vmCvar_t	g_skillboost10_dempDamageDealtBonus;
vmCvar_t	g_skillboost10_damageTakenReduction;
vmCvar_t	g_skillboost10_forceRegenBonus;
vmCvar_t	g_skillboost10_dempMaxFrozenTimeReduction;
vmCvar_t	g_skillboost10_movementSpeedBonus;
vmCvar_t	g_skillboost10_selfDamageFactorOverride;
vmCvar_t	g_skillboost10_splashRadiusBonus;

vmCvar_t	g_senseBoost1_interval;
vmCvar_t	g_senseBoost2_interval;
vmCvar_t	g_senseBoost3_interval;
vmCvar_t	g_senseBoost4_interval;

vmCvar_t	g_classLimits;
vmCvar_t	oAssaultLimit;
vmCvar_t	oHWLimit;
vmCvar_t	oDemoLimit;
vmCvar_t	oTechLimit;
vmCvar_t	oScoutLimit;
vmCvar_t	oJediLimit;
vmCvar_t	dAssaultLimit;
vmCvar_t	dHWLimit;
vmCvar_t	dDemoLimit;
vmCvar_t	dTechLimit;
vmCvar_t	dScoutLimit;
vmCvar_t	dJediLimit;

vmCvar_t	playedPugMaps;
vmCvar_t	desiredPugMaps;
vmCvar_t	g_defaultPugMaps;

vmCvar_t	g_autoStats;

/*vmCvar_t	debug_testHeight1;
vmCvar_t	debug_testHeight2;
vmCvar_t	debug_testHeight3;
vmCvar_t	debug_testHeight4;
vmCvar_t	debug_testHeight5;
vmCvar_t	debug_testHeight6;*/

vmCvar_t	debug_shieldLog;
vmCvar_t	debug_duoTest;

vmCvar_t	siege_r1_obj0;
vmCvar_t	siege_r1_obj1;
vmCvar_t	siege_r1_obj2;
vmCvar_t	siege_r1_obj3;
vmCvar_t	siege_r1_obj4;
vmCvar_t	siege_r1_obj5;
vmCvar_t	siege_r1_obj6;
vmCvar_t	siege_r1_obj7;
vmCvar_t	siege_r1_obj8;
vmCvar_t	siege_r1_obj9;
vmCvar_t	siege_r1_obj10;
vmCvar_t	siege_r1_obj11;
vmCvar_t	siege_r1_obj12;
vmCvar_t	siege_r1_obj13;
vmCvar_t	siege_r1_obj14;
vmCvar_t	siege_r1_obj15;
vmCvar_t	siege_r1_objscompleted;
vmCvar_t	siege_r1_heldformaxat;
vmCvar_t	siege_r1_heldformaxtime;
vmCvar_t	siege_r1_matchid;
vmCvar_t	siege_r2_obj0;
vmCvar_t	siege_r2_obj1;
vmCvar_t	siege_r2_obj2;
vmCvar_t	siege_r2_obj3;
vmCvar_t	siege_r2_obj4;
vmCvar_t	siege_r2_obj5;
vmCvar_t	siege_r2_obj6;
vmCvar_t	siege_r2_obj7;
vmCvar_t	siege_r2_obj8;
vmCvar_t	siege_r2_obj9;
vmCvar_t	siege_r2_obj10;
vmCvar_t	siege_r2_obj11;
vmCvar_t	siege_r2_obj12;
vmCvar_t	siege_r2_obj13;
vmCvar_t	siege_r2_obj14;
vmCvar_t	siege_r2_obj15;
vmCvar_t	siege_r2_objscompleted;
vmCvar_t	siege_r2_heldformaxat;
vmCvar_t	siege_r2_heldformaxtime;

vmCvar_t	vote_map_a;
vmCvar_t	vote_map_b;
vmCvar_t	vote_map_c;
vmCvar_t	vote_map_d;
vmCvar_t	vote_map_e;
vmCvar_t	vote_map_f;
vmCvar_t	vote_map_g;
vmCvar_t	vote_map_h;
vmCvar_t	vote_map_i;
vmCvar_t	vote_map_j;
vmCvar_t	vote_map_k;
vmCvar_t	vote_map_l;
vmCvar_t	vote_map_m;
vmCvar_t	vote_map_n;
vmCvar_t	vote_map_o;
vmCvar_t	vote_map_p;
vmCvar_t	vote_map_q;
vmCvar_t	vote_map_r;
vmCvar_t	vote_map_s;
vmCvar_t	vote_map_t;
vmCvar_t	vote_map_u;
vmCvar_t	vote_map_v;
vmCvar_t	vote_map_w;
vmCvar_t	vote_map_x;
vmCvar_t	vote_map_y;
vmCvar_t	vote_map_z;
vmCvar_t	vote_map_shortname_a;
vmCvar_t	vote_map_shortname_b;
vmCvar_t	vote_map_shortname_c;
vmCvar_t	vote_map_shortname_d;
vmCvar_t	vote_map_shortname_e;
vmCvar_t	vote_map_shortname_f;
vmCvar_t	vote_map_shortname_g;
vmCvar_t	vote_map_shortname_h;
vmCvar_t	vote_map_shortname_i;
vmCvar_t	vote_map_shortname_j;
vmCvar_t	vote_map_shortname_k;
vmCvar_t	vote_map_shortname_l;
vmCvar_t	vote_map_shortname_m;
vmCvar_t	vote_map_shortname_n;
vmCvar_t	vote_map_shortname_o;
vmCvar_t	vote_map_shortname_p;
vmCvar_t	vote_map_shortname_q;
vmCvar_t	vote_map_shortname_r;
vmCvar_t	vote_map_shortname_s;
vmCvar_t	vote_map_shortname_t;
vmCvar_t	vote_map_shortname_u;
vmCvar_t	vote_map_shortname_v;
vmCvar_t	vote_map_shortname_w;
vmCvar_t	vote_map_shortname_x;
vmCvar_t	vote_map_shortname_y;
vmCvar_t	vote_map_shortname_z;

vmCvar_t    g_forceOnNpcs;

#if 0//#ifndef FINAL_BUILD
vmCvar_t	g_debugDamage;
#endif
vmCvar_t	g_debugAlloc;
vmCvar_t	g_debugServerSkel;
vmCvar_t	g_weaponRespawn;
vmCvar_t	g_weaponTeamRespawn;
vmCvar_t	g_adaptRespawn;
vmCvar_t	g_motd;
vmCvar_t	g_synchronousClients;
vmCvar_t	g_forceClientUpdateRate;
vmCvar_t	g_warmup;
vmCvar_t	g_doWarmup;
vmCvar_t	g_restarted;
vmCvar_t	g_log;
vmCvar_t	g_logSync;
vmCvar_t	g_statLog;
vmCvar_t	g_statLogFile;
vmCvar_t	g_blood;
vmCvar_t	g_podiumDist;
vmCvar_t	g_podiumDrop;
vmCvar_t	g_allowVote;
vmCvar_t	g_teamAutoJoin;
vmCvar_t	g_teamForceBalance;
vmCvar_t	g_banIPs;
vmCvar_t    g_getstatusbanIPs;
vmCvar_t	g_filterBan;
vmCvar_t	g_smoothClients;
vmCvar_t	g_defaultBanHoursDuration;

vmCvar_t	g_bouncePadDoubleJump;

#include "namespace_begin.h"
vmCvar_t	pmove_fixed;
vmCvar_t	pmove_msec;
vmCvar_t	pmove_float;
#include "namespace_end.h"

vmCvar_t	g_defaultMapFFA;
vmCvar_t	g_defaultMapDuel;
vmCvar_t	g_defaultMapSiege;
vmCvar_t	g_defaultMapCTF;

vmCvar_t	g_listEntity;
vmCvar_t	g_singlePlayer;
vmCvar_t	g_enableBreath;
vmCvar_t	g_dismember;
vmCvar_t	g_forceDodge;
vmCvar_t	g_timeouttospec;

vmCvar_t	g_saberDmgVelocityScale;
vmCvar_t	g_saberDmgDelay_Idle;
vmCvar_t	g_saberDmgDelay_Wound;

vmCvar_t	g_saberDebugPrint;

vmCvar_t	g_siegeTeamSwitch;

vmCvar_t	bg_fighterAltControl;

#ifdef DEBUG_SABER_BOX
vmCvar_t	g_saberDebugBox;
#endif

//NPC nav debug
vmCvar_t	d_altRoutes;
vmCvar_t	d_patched;

vmCvar_t		g_saberRealisticCombat;
vmCvar_t		g_saberRestrictForce;
vmCvar_t		d_saberSPStyleDamage;
vmCvar_t		g_debugSaberLocks;
vmCvar_t		g_saberLockRandomNess;
// nmckenzie: SABER_DAMAGE_WALLS
vmCvar_t		g_saberWallDamageScale;

vmCvar_t		d_saberStanceDebug;
// ai debug cvars
vmCvar_t		debugNPCAI;			// used to print out debug info about the bot AI
vmCvar_t		debugNPCFreeze;		// set to disable bot ai and temporarily freeze them in place
vmCvar_t		debugNPCAimingBeam;
vmCvar_t		debugBreak;
vmCvar_t		debugNoRoam;
vmCvar_t		d_saberCombat;
vmCvar_t		d_JediAI;
vmCvar_t		d_noGroupAI;
vmCvar_t		d_asynchronousGroupAI;
vmCvar_t		d_slowmodeath;
vmCvar_t		d_noIntermissionWait;

vmCvar_t		g_spskill;

vmCvar_t		g_redTeam;
vmCvar_t		g_blueTeam;

vmCvar_t	g_austrian;

/*

Enhanced mod server cvars

*/

vmCvar_t	g_cleverFakeDetection;
vmCvar_t	g_protectQ3Fill;
vmCvar_t	g_protectQ3FillIPLimit;
vmCvar_t	g_maxIPConnected;
vmCvar_t	g_protectCallvoteHack;
vmCvar_t    g_minimumVotesCount;
vmCvar_t    g_fixPitKills;
vmCvar_t	g_fixGripKills;
vmCvar_t	g_creditAirKills;

vmCvar_t	g_fixForceJumpAnimationLock;
vmCvar_t	g_fixSniperSwitch;
vmCvar_t	g_fixGolanDamage;
vmCvar_t	g_locationBasedDamage_splash;

vmCvar_t	g_balanceSaber;
vmCvar_t	g_balanceSeeing;

vmCvar_t	g_autoSendScores;

vmCvar_t	g_autoGenerateLocations;

vmCvar_t	g_breakRNG;
vmCvar_t	g_siegeReflectionFix;

vmCvar_t	g_randomConeReflection;
vmCvar_t	g_coneReflectAngle;

#ifdef _DEBUG
vmCvar_t	z_debug1;
vmCvar_t	z_debug2;
vmCvar_t	z_debug3;
vmCvar_t	z_debug4;
vmCvar_t	z_debugSiegeTime;
vmCvar_t	z_debugUse;
#endif

vmCvar_t	g_saveCaptureRecords;
vmCvar_t	g_notifyNotLive;
vmCvar_t	g_autoPause999;
vmCvar_t	g_autoPauseDisconnect;

vmCvar_t    g_enforceEvenVotersCount;
vmCvar_t    g_minVotersForEvenVotersCount;

vmCvar_t	g_maxNameLength;

vmCvar_t	g_droppedFlagSpawnProtectionRadius;
vmCvar_t	g_droppedFlagSpawnProtectionDuration;

#ifdef NEWMOD_SUPPORT
vmCvar_t	g_netUnlock;
vmCvar_t	g_nmFlags;
vmCvar_t	g_enableNmAuth;
vmCvar_t	g_specInfo;
#endif

vmCvar_t     g_strafejump_mod;

vmCvar_t	g_antiWallhack;
vmCvar_t	g_wallhackMaxTraces;

vmCvar_t	g_inMemoryDB;

vmCvar_t	g_traceSQL;

//allowing/disabling vote types
vmCvar_t	g_allow_vote_customTeams;
vmCvar_t    g_allow_vote_gametype;
vmCvar_t    g_allow_vote_kick;
vmCvar_t    g_allow_vote_restart;
vmCvar_t    g_allow_vote_map;
vmCvar_t    g_allow_vote_nextmap;
vmCvar_t    g_allow_vote_timelimit;
vmCvar_t    g_allow_vote_fraglimit;
vmCvar_t    g_allow_vote_maprandom;
vmCvar_t    g_allow_vote_nextpug;
vmCvar_t    g_allow_vote_warmup;
vmCvar_t    g_npc_spawn_limit;
vmCvar_t	g_hackLog;
vmCvar_t	g_allow_vote_randomteams;
vmCvar_t	g_allow_vote_randomcapts;
vmCvar_t	g_allow_vote_cointoss;
vmCvar_t	g_allow_vote_q;
vmCvar_t    g_allow_vote_killturrets;
vmCvar_t    g_allow_vote_quickSpawns;
vmCvar_t    g_allow_vote_zombies;
vmCvar_t    g_allow_vote_pug;
vmCvar_t    g_allow_vote_pub;
vmCvar_t    g_allow_vote_forceclass;
vmCvar_t	g_allow_vote_lockteams;

vmCvar_t    g_allow_ready;

vmCvar_t    g_restart_countdown;

vmCvar_t    g_fixboon;
vmCvar_t    g_maxstatusrequests;
vmCvar_t	g_testdebug; //for tmp debug
vmCvar_t	g_rconpassword;

vmCvar_t	g_allowMoveDisable;

vmCvar_t	g_callvotedelay;
vmCvar_t	g_callvotemaplimit;

vmCvar_t	sv_privateclients;
vmCvar_t	sv_passwordlessSpectators;

vmCvar_t	g_autoStart;
vmCvar_t	g_autoStartTimer;
vmCvar_t	g_autoStartAFKThreshold;
vmCvar_t	g_autoStartMinPlayers;

vmCvar_t	g_printCountry;

// nmckenzie: temporary way to show player healths in duels - some iface gfx in game would be better, of course.
// DUEL_HEALTH
vmCvar_t		g_showDuelHealths;

// bk001129 - made static to avoid aliasing
static cvarTable_t		gameCvarTable[] = {
	// don't override the cheat state set by the system
	{ &g_cheats, "sv_cheats", "", 0, 0, qfalse },

	{ &g_debugMelee, "g_debugMelee", "0", CVAR_SERVERINFO, 0, qfalse  },
	{ &g_stepSlideFix, "g_stepSlideFix", "1", CVAR_SERVERINFO, 0, qtrue  },

	{ &g_noSpecMove, "g_noSpecMove", "0", CVAR_SERVERINFO, 0, qtrue },

	// noset vars
	{ NULL, "gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ NULL, "gamedate", __DATE__ , CVAR_ROM, 0, qfalse  },
	//TODO: autogenerate gameversion
	{ NULL, "gameversion", GAMEVERSION_VALUE , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ &g_restarted, "g_restarted", "0", CVAR_ROM, 0, qfalse  },
	{ NULL, "sv_mapname", "", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ &siegeStatus, "siegeStatus", "", CVAR_ROM | CVAR_SERVERINFO, 0, qfalse },
	{ &g_wasIntermission, "g_wasIntermission", "0", CVAR_ROM, 0, qfalse  },

	// latched vars
	{ &g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qfalse  },
	{ &g_MaxHolocronCarry, "g_MaxHolocronCarry", "3", CVAR_LATCH, 0, qfalse  },

	{ &g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse },
	{ &g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qtrue  },

	{ &g_trueJedi, "g_jediVmerc", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qtrue },

	// change anytime vars
	{ &g_ff_objectives, "g_ff_objectives", "0", /*CVAR_SERVERINFO |*/ CVAR_CHEAT | CVAR_NORESTART, 0, qtrue },

	{ &g_autoMapCycle, "g_autoMapCycle", "0", CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_autoStats, "g_autoStats", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_dmflags, "dmflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },

	{ &g_maxForceRank, "g_maxForceRank", "6", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse  },
	{ &g_forceBasedTeams, "g_forceBasedTeams", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse  },
	{ &g_privateDuel, "g_privateDuel", "1", CVAR_ARCHIVE, 0, qtrue  },

	{ &g_duelLifes, "g_duelLifes", "100", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_duelShields, "g_duelShields", "25", CVAR_ARCHIVE, 0, qtrue  },

	{ &g_chatLimit, "g_chatLimit", "0", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_teamChatLimit, "g_teamChatLimit", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_voiceChatLimit, "g_voiceChatLimit", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_allowNPC, "g_allowNPC", "1", CVAR_ARCHIVE, 0, qtrue  },

	{ &g_armBreakage, "g_armBreakage", "0", 0, 0, qtrue  },

	{ &g_saberLocking, "g_saberLocking", "1", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_saberLockFactor, "g_saberLockFactor", "2", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_saberTraceSaberFirst, "g_saberTraceSaberFirst", "0", CVAR_ARCHIVE, 0, qtrue  },

	{ &d_saberKickTweak, "d_saberKickTweak", "1", 0, 0, qtrue  },

	{ &d_powerDuelPrint, "d_powerDuelPrint", "0", 0, qtrue },

	{ &d_saberGhoul2Collision, "d_saberGhoul2Collision", "1", CVAR_CHEAT, 0, qtrue },
	{ &g_saberBladeFaces, "g_saberBladeFaces", "1", 0, 0, qtrue },

	{ &d_saberAlwaysBoxTrace, "d_saberAlwaysBoxTrace", "0", CVAR_CHEAT, 0, qtrue },
	{ &d_saberBoxTraceSize, "d_saberBoxTraceSize", "0", CVAR_CHEAT, 0, qtrue },

	{ &d_siegeSeekerNPC, "d_siegeSeekerNPC", "0", CVAR_CHEAT, 0, qtrue },

#if 0//#ifdef _DEBUG
	{ &g_disableServerG2, "g_disableServerG2", "0", 0, 0, qtrue },
#endif

	{ &d_perPlayerGhoul2, "d_perPlayerGhoul2", "0", CVAR_CHEAT, 0, qtrue },

	{ &d_projectileGhoul2Collision, "d_projectileGhoul2Collision", "1", CVAR_CHEAT, 0, qtrue },

	{ &g_g2TraceLod, "g_g2TraceLod", "3", 0, 0, qtrue },

	{ &g_optvehtrace, "com_optvehtrace", "0", 0, 0, qtrue },

	{ &g_locationBasedDamage, "g_locationBasedDamage", "3", 0, 0, qtrue },

	{ &g_allowHighPingDuelist, "g_allowHighPingDuelist", "1", 0, 0, qtrue },

	{ &g_fixSaberDefense, "g_fixSaberDefense", "2", 0, 0, qtrue },
	{ &g_saberDefense1Angle, "g_saberDefense1Angle", "6", 0, 0, qtrue },
	{ &g_saberDefense2Angle, "g_saberDefense2Angle", "30", 0, 0, qtrue },
	{ &g_saberDefense3Angle, "g_saberDefense3Angle", "73", 0, 0, qtrue },
	{ &g_saberDefenseDebug, "g_saberDefenseDebug", "0", 0, 0, qtrue },

	{ &g_saberHitsToKillSentry, "g_saberHitsToKillSentry", "2", 0, 0, qtrue },

	{ &g_logClientInfo, "g_logClientInfo", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_slowmoDuelEnd, "g_slowmoDuelEnd", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_saberDamageScale, "g_saberDamageScale", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_useWhileThrowing, "g_useWhileThrowing", "1", 0, 0, qtrue },

	{ &g_RMG, "RMG", "0", 0, 0, qtrue },

	{ &g_svfps, "sv_fps", "30", CVAR_SERVERINFO, 0, qtrue },

	{ &g_forceRegenTime, "g_forceRegenTime", "200", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue },

	{ &g_spawnInvulnerability, "g_spawnInvulnerability", "3000", CVAR_ARCHIVE, 0, qtrue },

	{ &g_forcePowerDisable, "g_forcePowerDisable", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_weaponDisable, "g_weaponDisable", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_duelWeaponDisable, "g_duelWeaponDisable", "1", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_allowDuelSuicide, "g_allowDuelSuicide", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_fraglimitVoteCorrection, "g_fraglimitVoteCorrection", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_fraglimit, "fraglimit", "20", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_duel_fraglimit, "duel_fraglimit", "10", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_capturelimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_capturedifflimit, "capturedifflimit", "0", CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },

    { &g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, qfalse },
	{ &g_forceClientUpdateRate, "g_forceClientUpdateRate", "200", CVAR_ARCHIVE, 0, qfalse },

	{ &d_saberInterpolate, "d_saberInterpolate", "0", CVAR_CHEAT, 0, qtrue },

	{ &g_friendlyFire, "g_friendlyFire", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_friendlySaber, "g_friendlySaber", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_teamAutoJoin, "g_teamAutoJoin", "0", CVAR_ARCHIVE },
	{ &g_teamForceBalance, "g_teamForceBalance", "0", CVAR_ARCHIVE },

	{ &g_warmup, "g_warmup", "20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_doWarmup, "g_doWarmup", "0", 0, 0, qtrue },
	{ &g_log, "g_log", "games.log", CVAR_ARCHIVE, 0, qfalse },
	{ &g_logSync, "g_logSync", "0", CVAR_ARCHIVE, 0, qfalse },

	{ &g_statLog, "g_statLog", "0", CVAR_ARCHIVE, 0, qfalse },
	{ &g_statLogFile, "g_statLogFile", "statlog.log", CVAR_ARCHIVE, 0, qfalse },

	{ &g_password, "g_password", "", CVAR_USERINFO, 0, qfalse },
	{ &sv_privatepassword, "sv_privatepassword", "", CVAR_TEMP, 0, qfalse },


	{ &g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, 0, qfalse },
	{ &g_getstatusbanIPs, "g_getstatusbanIPs", "", CVAR_ARCHIVE, 0, qfalse },

	{ &g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, 0, qfalse },

	{ &g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse },

	{ &g_dedicated, "dedicated", "0", 0, 0, qfalse },

	{ &g_developer, "developer", "0", 0, 0, qfalse },

	{ &g_speed, "g_speed", "250", CVAR_SERVERINFO, 0, qtrue },
	{ &g_gravity, "g_gravity", "760", CVAR_SERVERINFO, 0, qtrue },
	{ &g_knockback, "g_knockback", "1000", 0, 0, qtrue },
	{ &g_weaponRespawn, "g_weaponrespawn", "5", 0, 0, qtrue },
	{ &g_weaponTeamRespawn, "g_weaponTeamRespawn", "5", 0, 0, qtrue },
	{ &g_adaptRespawn, "g_adaptrespawn", "1", 0, 0, qtrue },		// Make weapons respawn faster with a lot of players.
	{ &g_forcerespawn, "g_forcerespawn", "60", 0, 0, qtrue },		// One minute force respawn.  Give a player enough time to reallocate force.
	{ &g_siegeRespawn, "g_siegeRespawn", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue }, //siege respawn wave time
	{ &g_inactivity, "g_inactivity", "0", 0, 0, qtrue },
	{ &g_inactivityKick, "g_inactivityKick", "1", 0, 0, qtrue },
	{ &g_spectatorInactivity, "g_spectatorInactivity", "0", 0, 0, qtrue },

	{ &g_debugMove, "g_debugMove", "0", 0, 0, qfalse },


#if 0//#ifndef FINAL_BUILD
	{ &g_debugDamage, "g_debugDamage", "0", 0, 0, qfalse },
#endif
	{ &g_debugAlloc, "g_debugAlloc", "0", 0, 0, qfalse },
	{ &g_debugServerSkel, "g_debugServerSkel", "0", CVAR_CHEAT, 0, qfalse },
	{ &g_motd, "g_motd", "", 0, 0, qfalse },
	{ &g_blood, "com_blood", "1", 0, 0, qfalse },

	{ &g_podiumDist, "g_podiumDist", "80", 0, 0, qfalse },
	{ &g_podiumDrop, "g_podiumDrop", "70", 0, 0, qfalse },

	{ &g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_listEntity, "g_listEntity", "0", 0, 0, qfalse },

	{ &g_singlePlayer, "ui_singlePlayerActive", "", 0, 0, qfalse, qfalse  },

	{ &g_enableBreath, "g_enableBreath", "0", 0, 0, qtrue, qfalse },
	{ &g_smoothClients, "g_smoothClients", "1", 0, 0, qfalse },
	{ &pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qtrue },
	{ &pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qtrue },
	{ &pmove_float, "pmove_float", "1", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qtrue },

	{ &g_defaultMapFFA, "g_defaultMapFFA", "", CVAR_ARCHIVE, 0, qtrue },
	{ &g_defaultMapDuel, "g_defaultMapDuel", "", CVAR_ARCHIVE, 0, qtrue },
	{ &g_defaultMapSiege, "g_defaultMapSiege", "", CVAR_ARCHIVE, 0, qtrue },
	{ &g_defaultMapCTF, "g_defaultMapCTF", "", CVAR_ARCHIVE, 0, qtrue },

	{ &g_dismember, "g_dismember", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_forceDodge, "g_forceDodge", "1", 0, 0, qtrue },

	{ &g_timeouttospec, "g_timeouttospec", "70", CVAR_ARCHIVE, 0, qfalse },

	{ &g_saberDmgVelocityScale, "g_saberDmgVelocityScale", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberDmgDelay_Idle, "g_saberDmgDelay_Idle", "350", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberDmgDelay_Wound, "g_saberDmgDelay_Wound", "0", CVAR_ARCHIVE, 0, qtrue },

#if 0//#ifndef FINAL_BUILD
	{ &g_saberDebugPrint, "g_saberDebugPrint", "0", CVAR_CHEAT, 0, qfalse  },
#endif
	{ &g_debugSaberLocks, "g_debugSaberLocks", "0", CVAR_CHEAT, 0, qfalse },
	{ &g_saberLockRandomNess, "g_saberLockRandomNess", "2", CVAR_CHEAT, 0, qfalse },
	// nmckenzie: SABER_DAMAGE_WALLS
		{ &g_saberWallDamageScale, "g_saberWallDamageScale", "0.4", 0, 0, qfalse },

		{ &d_saberStanceDebug, "d_saberStanceDebug", "0", 0, 0, qfalse },

		{ &g_siegeTeamSwitch, "g_siegeTeamSwitch", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, qtrue },

		{ &bg_fighterAltControl, "bg_fighterAltControl", "0", CVAR_SERVERINFO, 0, qtrue },

	#ifdef DEBUG_SABER_BOX
		{ &g_saberDebugBox, "g_saberDebugBox", "0", CVAR_CHEAT, 0, qfalse },
	#endif

		{ &d_altRoutes, "d_altRoutes", "0", CVAR_CHEAT, 0, qfalse },
		{ &d_patched, "d_patched", "0", CVAR_CHEAT, 0, qfalse },

		{ &g_saberRealisticCombat, "g_saberRealisticCombat", "0", CVAR_CHEAT },
		{ &g_saberRestrictForce, "g_saberRestrictForce", "0", CVAR_CHEAT },
		{ &d_saberSPStyleDamage, "d_saberSPStyleDamage", "1", CVAR_CHEAT },

		{ &debugNoRoam, "d_noroam", "0", CVAR_CHEAT },
		{ &debugNPCAimingBeam, "d_npcaiming", "0", CVAR_CHEAT },
		{ &debugBreak, "d_break", "0", CVAR_CHEAT },
		{ &debugNPCAI, "d_npcai", "0", CVAR_CHEAT },
		{ &debugNPCFreeze, "d_npcfreeze", "0", CVAR_CHEAT },
		{ &d_JediAI, "d_JediAI", "0", CVAR_CHEAT },
		{ &d_noGroupAI, "d_noGroupAI", "0", CVAR_CHEAT },
		{ &d_asynchronousGroupAI, "d_asynchronousGroupAI", "0", CVAR_CHEAT },

	//0 = never (BORING)
	//1 = kyle only
	//2 = kyle and last enemy jedi
	//3 = kyle and any enemy jedi
	//4 = kyle and last enemy in a group
	//5 = kyle and any enemy
	//6 = also when kyle takes pain or enemy jedi dodges player saber swing or does an acrobatic evasion

	{ &d_slowmodeath, "d_slowmodeath", "0", CVAR_CHEAT },

	{ &d_saberCombat, "d_saberCombat", "0", CVAR_CHEAT },

	{ &g_spskill, "g_npcspskill", "0", CVAR_ARCHIVE | CVAR_INTERNAL },

	{ &g_redTeam, "g_redTeam", "none", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qtrue },
	{ &g_blueTeam, "g_blueTeam", "none", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qtrue },

		//mainly for debugging with bots while I'm not around (want the server to
		//cycle through levels naturally)
	{ &d_noIntermissionWait, "d_noIntermissionWait", "0", CVAR_CHEAT, 0, qfalse },

	{ &g_austrian, "g_austrian", "0", CVAR_ARCHIVE, 0, qfalse  },
	// nmckenzie:
	// DUEL_HEALTH
		{ &g_showDuelHealths, "g_showDuelHealths", "0", CVAR_SERVERINFO },

		// *CHANGE 12* allowing/disabling cvars
	{ &g_protectQ3Fill,	"g_protectQ3Fill"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_protectQ3FillIPLimit,	"g_protectQ3FillIPLimit"	, "3"	, CVAR_ARCHIVE, 0, qtrue },


	{ &g_protectCallvoteHack,	"g_protectCallvoteHack"	, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_maxIPConnected,	"g_maxIPConnected"	, "0"	, CVAR_ARCHIVE, 0, qtrue },

		// *CHANGE 10* anti q3fill
	{ &g_cleverFakeDetection,	"g_cleverFakeDetection"	, "forcepowers"	, CVAR_ARCHIVE, 0, qtrue },


	{ &g_fixPitKills,	"g_fixPitKills"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixGripKills,	"g_fixGripKills", "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_creditAirKills,	"g_creditAirKills", "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_fixForceJumpAnimationLock,	"g_fixForceJumpAnimationLock"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixSniperSwitch,	"g_fixSniperSwitch"		, "0"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixGolanDamage,	"g_fixGolanDamage"		, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_locationBasedDamage_splash,	"g_locationBasedDamage_splash"		, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_balanceSaber, "g_balanceSaber", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_balanceSeeing, "g_balanceSeeing", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_autoSendScores, "g_autoSendScores", "2000", CVAR_ARCHIVE, 0, qtrue },

	{ &g_autoGenerateLocations, "g_autoGenerateLocations", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_breakRNG, "g_breakRNG", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_siegeReflectionFix, "g_siegeReflectionFix", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_randomConeReflection , "g_randomConeReflection", "-1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_coneReflectAngle , "g_coneReflectAngle", "30", CVAR_ARCHIVE, 0, qtrue },

#ifdef _DEBUG
	{ &z_debug1, "z_debug1", "", 0, 0, qtrue },
	{ &z_debug2, "z_debug2", "", 0, 0, qtrue },
	{ &z_debug3, "z_debug3", "", 0, 0, qtrue },
	{ &z_debug4, "z_debug4", "", 0, 0, qtrue },
	{ &z_debugSiegeTime, "z_debugSiegeTime", "", 0, 0, qtrue },
	{ &z_debugUse, "z_debugUse", "1", 0, 0, qtrue },
#endif

	{ &g_saveCaptureRecords, "g_saveCaptureRecords", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_notifyNotLive, "g_notifyNotLive", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoPause999, "g_autoPause999", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoPauseDisconnect, "g_autoPauseDisconnect", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_minimumVotesCount, "g_minimumVotesCount", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_enforceEvenVotersCount, "g_enforceEvenVotersCount", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_minVotersForEvenVotersCount, "g_minVotersForEvenVotersCount", "7", CVAR_ARCHIVE, 0, qtrue },

	{ &g_maxNameLength, "g_maxNameLength", "35", CVAR_ARCHIVE, 0, qtrue },

	{ &g_droppedFlagSpawnProtectionRadius, "g_droppedFlagSpawnProtectionRadius", "1024", CVAR_ARCHIVE, 0, qtrue },
	{ &g_droppedFlagSpawnProtectionDuration, "g_droppedFlagSpawnProtectionDuration", "10000", CVAR_ARCHIVE, 0, qtrue },

#ifdef NEWMOD_SUPPORT
	{ &g_netUnlock, "g_netUnlock", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_nmFlags, "g_nmFlags", "0", CVAR_ROM | CVAR_SERVERINFO, 0, qfalse },
	{ &g_enableNmAuth, "g_enableNmAuth", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_specInfo, "g_specInfo", "1", CVAR_ARCHIVE, 0, qtrue },
#endif
	{ &g_strafejump_mod,	"g_strafejump_mod"	, "0"	, CVAR_ARCHIVE, 0, qtrue },


	{ &g_antiWallhack,	"g_antiWallhack"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_wallhackMaxTraces,	"g_wallhackMaxTraces"	, "1000"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_inMemoryDB, "g_inMemoryDB", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },

	{ &g_traceSQL, "g_traceSQL", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },

    { &g_restart_countdown, "g_restart_countdown", "0", CVAR_ARCHIVE, 0, qtrue }, 

	{ &g_allow_vote_customTeams,	"g_allow_vote_customTeams"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_gametype,	"g_allow_vote_gametype"	, "1023"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_kick,	"g_allow_vote_kick"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_restart,	"g_allow_vote_restart"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_map,	"g_allow_vote_map"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_nextmap,	"g_allow_vote_nextmap"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_timelimit,	"g_allow_vote_timelimit"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_fraglimit,	"g_allow_vote_fraglimit"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_maprandom, "g_allow_vote_maprandom", "4", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_nextpug, "g_allow_vote_nextpug", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_warmup, "g_allow_vote_warmup", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_randomteams, "g_allow_vote_randomteams", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_randomcapts, "g_allow_vote_randomcapts", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_cointoss, "g_allow_vote_cointoss", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_forceclass, "g_allow_vote_forceclass", "1", CVAR_ARCHIVE, 0, qfalse },

	{ &g_allow_ready, "g_allow_ready", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_allow_vote_q, "g_allow_vote_q", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_allow_vote_killturrets, "g_allow_vote_killturrets", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_zombies, "g_allow_vote_zombies", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_pug, "g_allow_vote_pug", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_pub, "g_allow_vote_pub", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_lockteams, "g_allow_vote_lockteams", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_quickSpawns, "g_allow_vote_quickSpawns", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_hackLog,	"g_hackLog"	, "hacks.log"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_npc_spawn_limit,	"g_npc_spawn_limit"	, "100"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_accounts,	"g_accounts"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_accountsFile,	"g_accountsFile"	, "accounts.txt"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_whitelist, "g_whitelist", "0", CVAR_ARCHIVE, 0, qtrue },


	{ &g_dlURL,	"g_dlURL"	, ""	, CVAR_SYSTEMINFO, 0, qtrue },

	{ &cl_allowDownload,	"cl_allowDownload"	, "0"	, CVAR_SYSTEMINFO, 0, qfalse },
	{ &g_fixboon,	"g_fixboon"	, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_allowMoveDisable, "g_allowMoveDisable", "-1", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qtrue },

	{ &g_flags_overboarding, "g_flags_overboarding", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_sexyDisruptor, "g_sexyDisruptor", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixSiegeScoring, "g_fixSiegeScoring", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixFallingSounds, "g_fixFallingSounds", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_nextmapWarning, "g_nextmapWarning", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_floatingItems, "g_floatingItems", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_rocketSurfing, "g_rocketSurfing", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_improvedTeamchat, "g_improvedTeamchat", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_enableCloak, "g_enableCloak", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixHothBunkerLift, "g_fixHothBunkerLift", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixDesertStationsModels, "g_fixDesertStationsModels", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_infiniteCharge, "g_infiniteCharge", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_siegeTiebreakEnd, "g_siegeTiebreakEnd", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_moreTaunts, "g_moreTaunts", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixRancorCharge, "g_fixRancorCharge", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoKorribanFloatingItems, "g_autoKorribanFloatingItems", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoKorribanSpam, "g_autoKorribanSpam", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_forceDTechItems, "g_forceDTechItems", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_antiHothCodesLiftLame, "g_antiHothCodesLiftLame", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_antiHothHangarLiftLame, "g_antiHothHangarLiftLame", "4", CVAR_ARCHIVE, 0, qtrue },
	{ &g_antiHothInfirmaryLiftLame, "g_antiHothInfirmaryLiftLame", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_requireMoreCustomTeamVotes, "g_requireMoreCustomTeamVotes", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_antiCallvoteTakeover, "g_antiCallvoteTakeover", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoResetCustomTeams, "g_autoResetCustomTeams", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixEweb, "g_fixEweb", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixVoiceChat, "g_fixVoiceChat", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_botJumping, "g_botJumping", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixHothDoorSounds, "g_fixHothDoorSounds", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &iLikeToDoorSpam, "iLikeToDoorSpam", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &iLikeToMineSpam, "iLikeToMineSpam", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &iLikeToShieldSpam, "iLikeToShieldSpam", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &autocfg_map, "autocfg_map", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &autocfg_unknown, "autocfg_unknown", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_swoopKillPoints, "g_swoopKillPoints", "0", CVAR_ARCHIVE, 0, qfalse },
	{ &g_teamVoteFix, "g_teamVoteFix", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_antiLaming, "g_antiLaming", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_probation, "g_probation", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixHoth2ndObj, "g_fixHoth2ndObj", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_hothInfirmaryRebalance, "g_hothInfirmaryRebalance", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_siegeTimeVisualAid , "g_siegeTimeVisualAid", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_botAimbot, "g_botAimbot", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_botDefaultSiegeClass, "g_botDefaultSiegeClass", "scout", CVAR_ARCHIVE, 0, qtrue },
	{ &g_speedrunRoundOneRestart, "g_speedrunRoundOneRestart", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_siegeGhosting, "g_siegeGhosting", "2", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixHothHangarTurrets, "g_fixHothHangarTurrets", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_removeHothHangarTurrets, "g_removeHothHangarTurrets", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixDempSaberThrow, "g_fixDempSaberThrow", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixDodge, "g_fixDodge", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_joinMenuHack, "g_joinMenuHack", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_preventJoiningLargerTeam, "g_preventJoiningLargerTeam", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_lastIntermissionStartTime, "g_lastIntermissionStartTime", "", CVAR_TEMP | CVAR_ROM, 0, qfalse },
	{ &g_autoRestartAfterIntermission, "g_autoRestartAfterIntermission", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_saberThrowDefenseSmallAngle, "g_saberThrowDefenseSmallAngle", "30", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberThrowDefenseLargeAngle, "g_saberThrowDefenseLargeAngle", "80", CVAR_ARCHIVE, 0, qtrue },

	{ &g_saberThrowDefenseCompromisedBonus, "g_saberThrowDefenseCompromisedBonus", "0.2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberThrowDefensePushPullBonus, "g_saberThrowDefensePushPullBonus", "0.2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberThrowDefenseSmallDamage, "g_saberThrowDefenseSmallDamage", "0.4", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberThrowDefenseLargeDamage, "g_saberThrowDefenseLargeDamage", "0.6", CVAR_ARCHIVE, 0, qtrue },

	{ &g_saberThrowDefenseRewind, "g_saberThrowDefenseRewind", "0.5", CVAR_ARCHIVE, 0, qtrue },

	{ &g_webhookId, "g_webhookId", "", CVAR_ARCHIVE, 0, qfalse },
	{ &g_webhookToken, "g_webhookToken", "", CVAR_ARCHIVE, 0, qfalse },

	{ &g_teamOverlayForce, "g_teamOverlayForce", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_teamOverlayUpdateRate, "g_teamOverlayUpdateRate", "250", CVAR_ARCHIVE, 0, qtrue },
	{ &g_delayClassUpdate, "g_delayClassUpdate", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_defaultMap, "g_defaultMap", "", CVAR_ARCHIVE, 0, qtrue },
	{ &g_multiVoteRNG, "g_multiVoteRNG", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_runoffVote, "g_runoffVote", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_antiSelfMax, "g_antiSelfMax", "1", CVAR_ARCHIVE, 0 , qtrue },
	{ &g_improvedDisarm, "g_improvedDisarm", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_flechetteSpread, "g_flechetteSpread", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoSpec, "g_autoSpec", "1", CVAR_ARCHIVE, 0 , qtrue },
	{ &g_intermissionKnockbackNPCs, "g_intermissionKnockbackNPCs", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_emotes, "g_emotes", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_siegeHelp, "g_siegeHelp", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_improvedHoming, "g_improvedHoming", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_improvedHomingThreshold, "g_improvedHomingThreshold", "500", CVAR_ARCHIVE, 0, qtrue },
	{ &d_debugImprovedHoming, "d_debugImprovedHoming", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_braindeadBots, "g_braindeadBots", "0", CVAR_ARCHIVE, 0 , qtrue },
	{ &g_siegeRespawnAutoChange, "g_siegeRespawnAutoChange", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_quickPauseChat, "g_quickPauseChat", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_multiUseGenerators, "g_multiUseGenerators", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_dispenserLifetime, "g_dispenserLifetime", "25", CVAR_ARCHIVE, 0, qtrue }, // up from 20
	{ &g_techAmmoForAllWeapons, "g_techAmmoForAllWeapons", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_damageFixes, "g_damageFixes", "-1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_healWalkerWithAmmoCans, "g_healWalkerWithAmmoCans", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_friendlyFreeze, "g_friendlyFreeze", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixLiftkillTraps, "g_fixLiftkillTraps", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &lastMapName, "lastMapName", "", CVAR_ARCHIVE | CVAR_ROM, 0, qtrue },

	{ &g_customVotes, "g_customVotes", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote1_command, "g_customVote1_command", "map_restart", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote1_label, "g_customVote1_label", "Restart Map", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote2_command, "g_customVote2_command", "nextmap", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote2_label, "g_customVote2_label", "Restart Match (Round 1)", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote3_command, "g_customVote3_command", "newpug", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote3_label, "g_customVote3_label", "New Pug Map Rotation", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote4_command, "g_customVote4_command", "nextpug", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote4_label, "g_customVote4_label", "Next Pug Map", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote5_command, "g_customVote5_command", "randomteams 2 2", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote5_label, "g_customVote5_label", "Random Teams: 2v2", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote6_command, "g_customVote6_command", "randomteams 3 3", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote6_label, "g_customVote6_label", "Random Teams: 3v3", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote7_command, "g_customVote7_command", "randomteams 4 4", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote7_label, "g_customVote7_label", "Random Teams: 4v4", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote8_command, "g_customVote8_command", "shuffleteams", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote8_label, "g_customVote8_label", "Shuffle Teams", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote9_command, "g_customVote9_command", "pause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote9_label, "g_customVote9_label", "Pause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote10_command, "g_customVote10_command", "unpause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote10_label, "g_customVote10_label", "Unpause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_skillboost1_damageDealtBonus, "g_skillboost1_damageDealtBonus", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_damageDealtBonus, "g_skillboost2_damageDealtBonus", "0.15", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_damageDealtBonus, "g_skillboost3_damageDealtBonus", "0.20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_damageDealtBonus, "g_skillboost4_damageDealtBonus", "0.35", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_damageDealtBonus, "g_skillboost5_damageDealtBonus", "0.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_damageDealtBonus, "g_skillboost6_damageDealtBonus", "0.75", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_damageDealtBonus, "g_skillboost7_damageDealtBonus", "1.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_damageDealtBonus, "g_skillboost8_damageDealtBonus", "1.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_damageDealtBonus, "g_skillboost9_damageDealtBonus", "2.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_damageDealtBonus, "g_skillboost10_damageDealtBonus", "3.00", CVAR_ARCHIVE, 0, qtrue },

	{ &g_skillboost1_dempDamageDealtBonus, "g_skillboost1_dempDamageDealtBonus", "0.0333", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_dempDamageDealtBonus, "g_skillboost2_dempDamageDealtBonus", "0.0667", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_dempDamageDealtBonus, "g_skillboost3_dempDamageDealtBonus", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_dempDamageDealtBonus, "g_skillboost4_dempDamageDealtBonus", "0.125", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_dempDamageDealtBonus, "g_skillboost5_dempDamageDealtBonus", "0.15", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_dempDamageDealtBonus, "g_skillboost6_dempDamageDealtBonus", "0.20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_dempDamageDealtBonus, "g_skillboost7_dempDamageDealtBonus", "0.30", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_dempDamageDealtBonus, "g_skillboost8_dempDamageDealtBonus", "0.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_dempDamageDealtBonus, "g_skillboost9_dempDamageDealtBonus", "0.75", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_dempDamageDealtBonus, "g_skillboost10_dempDamageDealtBonus", "1.00", CVAR_ARCHIVE, 0, qtrue },

	{ &g_skillboost1_damageTakenReduction, "g_skillboost1_damageTakenReduction", "0.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_damageTakenReduction, "g_skillboost2_damageTakenReduction", "0.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_damageTakenReduction, "g_skillboost3_damageTakenReduction", "0.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_damageTakenReduction, "g_skillboost4_damageTakenReduction", "0.025", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_damageTakenReduction, "g_skillboost5_damageTakenReduction", "0.05", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_damageTakenReduction, "g_skillboost6_damageTakenReduction", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_damageTakenReduction, "g_skillboost7_damageTakenReduction", "0.15", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_damageTakenReduction, "g_skillboost8_damageTakenReduction", "0.20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_damageTakenReduction, "g_skillboost9_damageTakenReduction", "0.30", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_damageTakenReduction, "g_skillboost10_damageTakenReduction", "0.40", CVAR_ARCHIVE, 0, qtrue },

	{ &g_skillboost1_forceRegenBonus, "g_skillboost1_forceRegenBonus", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_forceRegenBonus, "g_skillboost2_forceRegenBonus", "0.15", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_forceRegenBonus, "g_skillboost3_forceRegenBonus", "0.20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_forceRegenBonus, "g_skillboost4_forceRegenBonus", "0.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_forceRegenBonus, "g_skillboost5_forceRegenBonus", "1.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_forceRegenBonus, "g_skillboost6_forceRegenBonus", "1.25", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_forceRegenBonus, "g_skillboost7_forceRegenBonus", "1.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_forceRegenBonus, "g_skillboost8_forceRegenBonus", "2.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_forceRegenBonus, "g_skillboost9_forceRegenBonus", "3.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_forceRegenBonus, "g_skillboost10_forceRegenBonus", "4.00", CVAR_ARCHIVE, 0, qtrue },

	{ &g_skillboost1_dempMaxFrozenTimeReduction, "g_skillboost1_dempMaxFrozenTimeReduction", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_dempMaxFrozenTimeReduction, "g_skillboost2_dempMaxFrozenTimeReduction", "0.15", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_dempMaxFrozenTimeReduction, "g_skillboost3_dempMaxFrozenTimeReduction", "0.20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_dempMaxFrozenTimeReduction, "g_skillboost4_dempMaxFrozenTimeReduction", "0.30", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_dempMaxFrozenTimeReduction, "g_skillboost5_dempMaxFrozenTimeReduction", "0.45", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_dempMaxFrozenTimeReduction, "g_skillboost6_dempMaxFrozenTimeReduction", "0.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_dempMaxFrozenTimeReduction, "g_skillboost7_dempMaxFrozenTimeReduction", "0.60", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_dempMaxFrozenTimeReduction, "g_skillboost8_dempMaxFrozenTimeReduction", "0.70", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_dempMaxFrozenTimeReduction, "g_skillboost9_dempMaxFrozenTimeReduction", "0.80", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_dempMaxFrozenTimeReduction, "g_skillboost10_dempMaxFrozenTimeReduction", "0.90", CVAR_ARCHIVE, 0, qtrue },

	{ &g_skillboost1_movementSpeedBonus, "g_skillboost1_movementSpeedBonus", "0.05", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_movementSpeedBonus, "g_skillboost2_movementSpeedBonus", "0.075", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_movementSpeedBonus, "g_skillboost3_movementSpeedBonus", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_movementSpeedBonus, "g_skillboost4_movementSpeedBonus", "0.15", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_movementSpeedBonus, "g_skillboost5_movementSpeedBonus", "0.20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_movementSpeedBonus, "g_skillboost6_movementSpeedBonus", "0.30", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_movementSpeedBonus, "g_skillboost7_movementSpeedBonus", "0.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_movementSpeedBonus, "g_skillboost8_movementSpeedBonus", "0.75", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_movementSpeedBonus, "g_skillboost9_movementSpeedBonus", "1.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_movementSpeedBonus, "g_skillboost10_movementSpeedBonus", "1.50", CVAR_ARCHIVE, 0, qtrue },


	{ &g_skillboost1_selfDamageFactorOverride, "g_skillboost1_selfDamageFactorOverride", "1.25", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_selfDamageFactorOverride, "g_skillboost2_selfDamageFactorOverride", "1.125", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_selfDamageFactorOverride, "g_skillboost3_selfDamageFactorOverride", "1.00", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_selfDamageFactorOverride, "g_skillboost4_selfDamageFactorOverride", "0.75", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_selfDamageFactorOverride, "g_skillboost5_selfDamageFactorOverride", "0.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_selfDamageFactorOverride, "g_skillboost6_selfDamageFactorOverride", "0.40", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_selfDamageFactorOverride, "g_skillboost7_selfDamageFactorOverride", "0.30", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_selfDamageFactorOverride, "g_skillboost8_selfDamageFactorOverride", "0.20", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_selfDamageFactorOverride, "g_skillboost9_selfDamageFactorOverride", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_selfDamageFactorOverride, "g_skillboost10_selfDamageFactorOverride", "0.05", CVAR_ARCHIVE, 0, qtrue },

	{ &g_skillboost1_splashRadiusBonus, "g_skillboost1_splashRadiusBonus", "0.05", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost2_splashRadiusBonus, "g_skillboost2_splashRadiusBonus", "0.10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost3_splashRadiusBonus, "g_skillboost3_splashRadiusBonus", "0.15", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost4_splashRadiusBonus, "g_skillboost4_splashRadiusBonus", "0.25", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost5_splashRadiusBonus, "g_skillboost5_splashRadiusBonus", "0.40", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost6_splashRadiusBonus, "g_skillboost6_splashRadiusBonus", "0.50", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost7_splashRadiusBonus, "g_skillboost7_splashRadiusBonus", "0.60", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost8_splashRadiusBonus, "g_skillboost8_splashRadiusBonus", "0.70", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost9_splashRadiusBonus, "g_skillboost9_splashRadiusBonus", "0.80", CVAR_ARCHIVE, 0, qtrue },
	{ &g_skillboost10_splashRadiusBonus, "g_skillboost10_splashRadiusBonus", "1.00", CVAR_ARCHIVE, 0, qtrue },

	{ &g_senseBoost1_interval, "g_senseBoost1_interval", "10100", CVAR_ARCHIVE, 0 , qtrue },
	{ &g_senseBoost2_interval, "g_senseBoost2_interval", "7100", CVAR_ARCHIVE, 0 , qtrue },
	{ &g_senseBoost3_interval, "g_senseBoost3_interval", "4100", CVAR_ARCHIVE, 0 , qtrue },
	{ &g_senseBoost4_interval, "g_senseBoost4_interval", "2100", CVAR_ARCHIVE, 0 , qtrue },
	// level 5 is just constant

	{ &g_lockdown, "g_lockdown", "0", 0, 0, qtrue },
	{ &g_hothRebalance, "g_hothRebalance", "3", CVAR_ARCHIVE, 0, qtrue },
	{ &g_hothHangarHack, "g_hothHangarHack", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixShield, "g_fixShield", "1", CVAR_ARCHIVE, 0, qtrue },
	/*{ &debug_testHeight1, "debug_testHeight1", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &debug_testHeight2, "debug_testHeight2", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &debug_testHeight3, "debug_testHeight3", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &debug_testHeight4, "debug_testHeight4", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &debug_testHeight5, "debug_testHeight5", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &debug_testHeight6, "debug_testHeight6", "0", CVAR_ARCHIVE, 0, qtrue },*/

	{ &g_classLimits, "g_classLimits", "1", 0, 0, qtrue },
	{ &oAssaultLimit, "oAssaultLimit", "0", 0, 0, qtrue },
	{ &oHWLimit, "oHWLimit", "0", 0, 0, qtrue },
	{ &oDemoLimit, "oDemoLimit", "0", 0, 0, qtrue },
	{ &oTechLimit, "oTechLimit", "0", 0, 0, qtrue },
	{ &oScoutLimit, "oScoutLimit", "0", 0, 0, qtrue },
	{ &oJediLimit, "oJediLimit", "0", 0, 0, qtrue },
	{ &dAssaultLimit, "dAssaultLimit", "0", 0, 0, qtrue },
	{ &dHWLimit, "dHWLimit", "0", 0, 0, qtrue },
	{ &dDemoLimit, "dDemoLimit", "0", 0, 0, qtrue },
	{ &dTechLimit, "dTechLimit", "0", 0, 0, qtrue },
	{ &dScoutLimit, "dScoutLimit", "0", 0, 0, qtrue },
	{ &dJediLimit, "dJediLimit", "0", 0, 0, qtrue },

	{ &playedPugMaps, "playedPugMaps", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &desiredPugMaps, "desiredPugMaps", "", CVAR_ARCHIVE | CVAR_ROM, 0 , qfalse },
	{ &g_defaultPugMaps, "g_defaultPugMaps", "hncub", CVAR_ARCHIVE, 0, qtrue },

	{ &debug_shieldLog, "debug_shieldLog", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &debug_duoTest, "debug_duoTest", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &siege_r1_obj0, "siege_r1_obj0", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj1, "siege_r1_obj1", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj2, "siege_r1_obj2", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj3, "siege_r1_obj3", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj4, "siege_r1_obj4", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj5, "siege_r1_obj5", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj6, "siege_r1_obj6", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj7, "siege_r1_obj7", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj8, "siege_r1_obj8", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj9, "siege_r1_obj9", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj10, "siege_r1_obj10", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj11, "siege_r1_obj11", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj12, "siege_r1_obj12", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj13, "siege_r1_obj13", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj14, "siege_r1_obj14", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_obj15, "siege_r1_obj15", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_objscompleted, "siege_r1_objscompleted", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_heldformaxat, "siege_r1_heldformaxat", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_heldformaxtime, "siege_r1_heldformaxtime", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r1_matchid, "siege_r1_matchid", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj0, "siege_r2_obj0", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj1, "siege_r2_obj1", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj2, "siege_r2_obj2", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj3, "siege_r2_obj3", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj4, "siege_r2_obj4", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj5, "siege_r2_obj5", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj6, "siege_r2_obj6", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj7, "siege_r2_obj7", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj8, "siege_r2_obj8", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj9, "siege_r2_obj9", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj10, "siege_r2_obj10", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj11, "siege_r2_obj11", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj12, "siege_r2_obj12", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj13, "siege_r2_obj13", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj14, "siege_r2_obj14", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_obj15, "siege_r2_obj15", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_objscompleted, "siege_r2_objscompleted", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_heldformaxat, "siege_r2_heldformaxat", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },
	{ &siege_r2_heldformaxtime, "siege_r2_heldformaxtime", "", CVAR_ARCHIVE | CVAR_ROM, 0, qfalse },

	{ &vote_map_a, "vote_map_a", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_b, "vote_map_b", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_c, "vote_map_c", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_d, "vote_map_d", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_e, "vote_map_e", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_f, "vote_map_f", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_g, "vote_map_g", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_h, "vote_map_h", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_i, "vote_map_i", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_j, "vote_map_j", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_k, "vote_map_k", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_l, "vote_map_l", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_m, "vote_map_m", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_n, "vote_map_n", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_o, "vote_map_o", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_p, "vote_map_p", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_q, "vote_map_q", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_r, "vote_map_r", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_s, "vote_map_s", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_t, "vote_map_t", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_u, "vote_map_u", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_v, "vote_map_v", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_w, "vote_map_w", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_x, "vote_map_x", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_y, "vote_map_y", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_z, "vote_map_z", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_a, "vote_map_shortname_a", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_b, "vote_map_shortname_b", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_c, "vote_map_shortname_c", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_d, "vote_map_shortname_d", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_e, "vote_map_shortname_e", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_f, "vote_map_shortname_f", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_g, "vote_map_shortname_g", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_h, "vote_map_shortname_h", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_i, "vote_map_shortname_i", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_j, "vote_map_shortname_j", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_k, "vote_map_shortname_k", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_l, "vote_map_shortname_l", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_m, "vote_map_shortname_m", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_n, "vote_map_shortname_n", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_o, "vote_map_shortname_o", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_p, "vote_map_shortname_p", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_q, "vote_map_shortname_q", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_r, "vote_map_shortname_r", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_s, "vote_map_shortname_s", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_t, "vote_map_shortname_t", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_u, "vote_map_shortname_u", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_v, "vote_map_shortname_v", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_w, "vote_map_shortname_w", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_x, "vote_map_shortname_x", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_y, "vote_map_shortname_y", "", CVAR_ARCHIVE, 0, qtrue },
	{ &vote_map_shortname_z, "vote_map_shortname_z", "", CVAR_ARCHIVE, 0, qtrue },

	{ &g_forceOnNpcs, "g_forceOnNpcs", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_maxstatusrequests,	"g_maxstatusrequests"	, "50"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_testdebug,	"g_testdebug"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	
	{ &g_logrcon,	"g_logrcon"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_rconpassword,	"rconpassword"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },

	{ &g_callvotedelay,	"g_callvotedelay"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },
    { &g_callvotemaplimit,	"g_callvotemaplimit"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },
    
    { &sv_privateclients, "sv_privateclients", "0", CVAR_ARCHIVE | CVAR_SERVERINFO },
	{ &sv_passwordlessSpectators, "sv_passwordlessSpectators", "0", CVAR_ARCHIVE | CVAR_SERVERINFO },
    { &g_defaultBanHoursDuration, "g_defaultBanHoursDuration", "9999999", CVAR_ARCHIVE | CVAR_INTERNAL },      

	{ &g_autoStart, "g_autoStart", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoStartTimer, "g_autoStartTimer", "10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoStartAFKThreshold, "g_autoStartAFKThreshold", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoStartMinPlayers, "g_autoStartMinPlayers", "4", CVAR_ARCHIVE, 0, qtrue },

	{ &g_bouncePadDoubleJump, "g_bouncePadDoubleJump", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_unlagged, "g_unlagged", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
#ifdef _DEBUG
	{ &g_unlaggedMaxCompensation, "g_unlaggedMaxCompensation", "500", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedSkeletonTime, "g_unlaggedSkeletonTime", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedFactor, "g_unlaggedFactor", "0.25", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedOffset, "g_unlaggedOffset", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedDebug, "g_unlaggedDebug", "0", CVAR_ARCHIVE, 0, qtrue },
#endif

	{ &g_printCountry, "g_printCountry", "1", CVAR_ARCHIVE, 0, qtrue },
};

// bk001129 - made static to avoid aliasing
static int gameCvarTableSize = sizeof( gameCvarTable ) / sizeof( gameCvarTable[0] );


void G_InitGame					( int levelTime, int randomSeed, int restart, void *serverDbPtr );
void G_RunFrame					( int levelTime );
void G_ShutdownGame				( int restart );
void CheckExitRules				( void );
void G_ROFF_NotetrackCallback	( gentity_t *cent, const char *notetrack);

extern stringID_table_t setTable[];

qboolean G_ParseSpawnVars( qboolean inSubBSP );
void G_SpawnGEntityFromSpawnVars( qboolean inSubBSP );


qboolean NAV_ClearPathToPoint( gentity_t *self, vec3_t pmins, vec3_t pmaxs, vec3_t point, int clipmask, int okToHitEntNum );
qboolean NPC_ClearLOS2( gentity_t *ent, const vec3_t end );
int NAVNEW_ClearPathBetweenPoints(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int ignore, int clipmask);
qboolean NAV_CheckNodeFailedForEnt( gentity_t *ent, int nodeNum );
qboolean G_EntIsUnlockedDoor( int entityNum );
qboolean G_EntIsDoor( int entityNum );
qboolean G_EntIsBreakable( int entityNum );
qboolean G_EntIsRemovableUsable( int entNum );
void CP_FindCombatPointWaypoints( void );

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
#include "namespace_begin.h"
#if defined(__linux__) && !defined(__GCC__)
extern "C" {
#endif

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  ) {
	switch ( command ) {
	case GAME_INIT:
		G_InitGame( arg0, arg1, arg2, (void *)arg3 );
		return 0;
	case GAME_SHUTDOWN:
		G_ShutdownGame( arg0 );
		return 0;
	case GAME_CLIENT_CONNECT:
		return (int)ClientConnect( arg0, arg1, arg2 );
	case GAME_CLIENT_THINK:
		ClientThink( arg0, NULL );
		return 0;
	case GAME_CLIENT_USERINFO_CHANGED:
		ClientUserinfoChanged( arg0 );
		return 0;
	case GAME_CLIENT_DISCONNECT:
		ClientDisconnect( arg0 );
		return 0;
	case GAME_CLIENT_BEGIN:
		ClientBegin( arg0, qtrue );
		return 0;
	case GAME_CLIENT_COMMAND:
		ClientCommand( arg0 );
		return 0;
	case GAME_RUN_FRAME:
		G_RunFrame( arg0 );
		return 0;
	case GAME_CONSOLE_COMMAND:
		return ConsoleCommand();
	case BOTAI_START_FRAME:
		return BotAIStartFrame( arg0 );
	case GAME_ROFF_NOTETRACK_CALLBACK:
		G_ROFF_NotetrackCallback( &g_entities[arg0], (const char *)arg1 );
		return 0;
	case GAME_SPAWN_RMG_ENTITY:
		if (G_ParseSpawnVars(qfalse))
		{
			G_SpawnGEntityFromSpawnVars(qfalse);
		}
		return 0;

	//rww - begin icarus callbacks
	case GAME_ICARUS_PLAYSOUND:
		{
			T_G_ICARUS_PLAYSOUND *sharedMem = (T_G_ICARUS_PLAYSOUND *)gSharedBuffer;
			return Q3_PlaySound(sharedMem->taskID, sharedMem->entID, sharedMem->name, sharedMem->channel);
		}
	case GAME_ICARUS_SET:
		{
			T_G_ICARUS_SET *sharedMem = (T_G_ICARUS_SET *)gSharedBuffer;
			return Q3_Set(sharedMem->taskID, sharedMem->entID, sharedMem->type_name, sharedMem->data);
		}
	case GAME_ICARUS_LERP2POS:
		{
			T_G_ICARUS_LERP2POS *sharedMem = (T_G_ICARUS_LERP2POS *)gSharedBuffer;
			if (sharedMem->nullAngles)
			{
				Q3_Lerp2Pos(sharedMem->taskID, sharedMem->entID, sharedMem->origin, NULL, sharedMem->duration);
			}
			else
			{
				Q3_Lerp2Pos(sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->angles, sharedMem->duration);
			}
		}
		return 0;
	case GAME_ICARUS_LERP2ORIGIN:
		{
			T_G_ICARUS_LERP2ORIGIN *sharedMem = (T_G_ICARUS_LERP2ORIGIN *)gSharedBuffer;
			Q3_Lerp2Origin(sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_LERP2ANGLES:
		{
			T_G_ICARUS_LERP2ANGLES *sharedMem = (T_G_ICARUS_LERP2ANGLES *)gSharedBuffer;
			Q3_Lerp2Angles(sharedMem->taskID, sharedMem->entID, sharedMem->angles, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_GETTAG:
		{
			T_G_ICARUS_GETTAG *sharedMem = (T_G_ICARUS_GETTAG *)gSharedBuffer;
			return Q3_GetTag(sharedMem->entID, sharedMem->name, sharedMem->lookup, sharedMem->info);
		}
	case GAME_ICARUS_LERP2START:
		{
			T_G_ICARUS_LERP2START *sharedMem = (T_G_ICARUS_LERP2START *)gSharedBuffer;
			Q3_Lerp2Start(sharedMem->entID, sharedMem->taskID, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_LERP2END:
		{
			T_G_ICARUS_LERP2END *sharedMem = (T_G_ICARUS_LERP2END *)gSharedBuffer;
			Q3_Lerp2End(sharedMem->entID, sharedMem->taskID, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_USE:
		{
			T_G_ICARUS_USE *sharedMem = (T_G_ICARUS_USE *)gSharedBuffer;
			Q3_Use(sharedMem->entID, sharedMem->target);
		}
		return 0;
	case GAME_ICARUS_KILL:
		{
			T_G_ICARUS_KILL *sharedMem = (T_G_ICARUS_KILL *)gSharedBuffer;
			Q3_Kill(sharedMem->entID, sharedMem->name);
		}
		return 0;
	case GAME_ICARUS_REMOVE:
		{
			T_G_ICARUS_REMOVE *sharedMem = (T_G_ICARUS_REMOVE *)gSharedBuffer;
			Q3_Remove(sharedMem->entID, sharedMem->name);
		}
		return 0;
	case GAME_ICARUS_PLAY:
		{
			T_G_ICARUS_PLAY *sharedMem = (T_G_ICARUS_PLAY *)gSharedBuffer;
			Q3_Play(sharedMem->taskID, sharedMem->entID, sharedMem->type, sharedMem->name);
		}
		return 0;
	case GAME_ICARUS_GETFLOAT:
		{
			T_G_ICARUS_GETFLOAT *sharedMem = (T_G_ICARUS_GETFLOAT *)gSharedBuffer;
			return Q3_GetFloat(sharedMem->entID, sharedMem->type, sharedMem->name, &sharedMem->value);
		}
	case GAME_ICARUS_GETVECTOR:
		{
			T_G_ICARUS_GETVECTOR *sharedMem = (T_G_ICARUS_GETVECTOR *)gSharedBuffer;
			return Q3_GetVector(sharedMem->entID, sharedMem->type, sharedMem->name, sharedMem->value);
		}
	case GAME_ICARUS_GETSTRING:
		{
			T_G_ICARUS_GETSTRING *sharedMem = (T_G_ICARUS_GETSTRING *)gSharedBuffer;
			int r;
			char *crap = NULL; //I am sorry for this -rww
			char **morecrap = &crap; //and this
			r = Q3_GetString(sharedMem->entID, sharedMem->type, sharedMem->name, morecrap);

			if (crap)
			{ //success!
				strcpy(sharedMem->value, crap);
			}

			return r;
		}
	case GAME_ICARUS_SOUNDINDEX:
		{
			T_G_ICARUS_SOUNDINDEX *sharedMem = (T_G_ICARUS_SOUNDINDEX *)gSharedBuffer;
			G_SoundIndex(sharedMem->filename);
		}
		return 0;
	case GAME_ICARUS_GETSETIDFORSTRING:
		{
			T_G_ICARUS_GETSETIDFORSTRING *sharedMem = (T_G_ICARUS_GETSETIDFORSTRING *)gSharedBuffer;
			return GetIDForString(setTable, sharedMem->string);
		}
	//rww - end icarus callbacks

	case GAME_NAV_CLEARPATHTOPOINT:
		return NAV_ClearPathToPoint(&g_entities[arg0], (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5);
	case GAME_NAV_CLEARLOS:
		return NPC_ClearLOS2(&g_entities[arg0], (const float *)arg1);
	case GAME_NAV_CLEARPATHBETWEENPOINTS:
		return NAVNEW_ClearPathBetweenPoints((float *)arg0, (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5);
	case GAME_NAV_CHECKNODEFAILEDFORENT:
		return NAV_CheckNodeFailedForEnt(&g_entities[arg0], arg1);
	case GAME_NAV_ENTISUNLOCKEDDOOR:
		return G_EntIsUnlockedDoor(arg0);
	case GAME_NAV_ENTISDOOR:
		return G_EntIsDoor(arg0);
	case GAME_NAV_ENTISBREAKABLE:
		return G_EntIsBreakable(arg0);
	case GAME_NAV_ENTISREMOVABLEUSABLE:
		return G_EntIsRemovableUsable(arg0);
	case GAME_NAV_FINDCOMBATPOINTWAYPOINTS:
		CP_FindCombatPointWaypoints();
		return 0;
	case GAME_GETITEMINDEXBYTAG:
		return BG_GetItemIndexByTag(arg0, arg1);

	// base_enhanced

	case GAME_TRANSFER_RESULT:
		G_HandleTransferResult((trsfHandle_t)arg0, (trsfErrorInfo_t*)arg1, arg2, (void*)arg3, (size_t)arg4);
		return 0;
	case GAME_RCON_COMMAND:
		G_LogRconCommand((const char*)arg0, (const char*)arg1);
		return 0;
	case GAME_STATUS:
		G_Status();
		return 0;
	}

	return -1;
}
#if defined(__linux__) && !defined(__GCC__)
}
#endif
#include "namespace_end.h"

#define BUFFER_TEMP_LEN 2048
#define BUFFER_REAL_LEN 1024
void QDECL G_Printf( const char *fmt, ... ) {
	va_list		argptr;
	static char		text[4096] = { 0 };

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	text[4096-1] = '\0';

	trap_Printf( text );
}

void QDECL G_Error( const char *fmt, ... ) {
	va_list		argptr;
	static char		text[BUFFER_TEMP_LEN] = { 0 };

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	text[BUFFER_REAL_LEN-1] = '\0';

	trap_Error( text );
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=1, e=g_entities+i ; i < level.num_entities ; i++,e++ ){
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		if (e->r.contents==CONTENTS_TRIGGER)
			continue;//triggers NEVER link up in teams!
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < level.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

}

void G_RemapTeamShaders( void ) {
#if 0
	char string[1024];
	float f = level.time * 0.001;
	Com_sprintf( string, sizeof(string), "team_icon/%s_red", g_redteam.string );
	AddRemap("textures/ctf2/redteam01", string, f); 
	AddRemap("textures/ctf2/redteam02", string, f); 
	Com_sprintf( string, sizeof(string), "team_icon/%s_blue", g_blueteam.string );
	AddRemap("textures/ctf2/blueteam01", string, f); 
	AddRemap("textures/ctf2/blueteam02", string, f); 
	trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
#endif
}


/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		trap_Cvar_Register( cv->vmCvar, cv->cvarName,
			cv->defaultString, cv->cvarFlags );
		if ( cv->vmCvar )
			cv->modificationCount = cv->vmCvar->modificationCount;

		if (cv->teamShader) {
			remapped = qtrue;
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}

	// check some things
	if ( g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE ) {
		G_Printf( "g_gametype %i is out of range, defaulting to 0\n", g_gametype.integer );
		trap_Cvar_Set( "g_gametype", "0" );
	}
	else if (g_gametype.integer == GT_HOLOCRON)
	{
		G_Printf( "This gametype is not supported.\n" );
		trap_Cvar_Set( "g_gametype", "0" );
	}
	else if (g_gametype.integer == GT_JEDIMASTER)
	{
		G_Printf( "This gametype is not supported.\n" );
		trap_Cvar_Set( "g_gametype", "0" );
	}
	else if (g_gametype.integer == GT_CTY)
	{
		G_Printf( "This gametype is not supported.\n" );
		trap_Cvar_Set( "g_gametype", "0" );
	}

	level.warmupModificationCount = g_warmup.modificationCount;
}

/*
=================
G_UpdateCvars
=================
*/
void G_UpdateCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		if ( cv->vmCvar ) {
			trap_Cvar_Update( cv->vmCvar );

			if ( cv->modificationCount != cv->vmCvar->modificationCount ) {
				cv->modificationCount = cv->vmCvar->modificationCount;

				if ( cv->trackChange ) {
					trap_SendServerCommand( -1, va("print \"Server: %s changed to %s\n\"", 
						cv->cvarName, cv->vmCvar->string ) );
				}

				if (cv->teamShader) {
					remapped = qtrue;
				}				
			}
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}
}

void GetPlayerCounts(qboolean includeBots, qboolean realTeam, int *numRedOut, int *numBlueOut, int *numSpecOut, int *numFreeOut) {
	int numRed = 0, numBlue = 0, numSpec = 0, numFree = 0;
	for (gclient_t *cl = &level.clients[0]; cl - level.clients < MAX_CLIENTS; cl++) {
		if (cl->pers.connected != CON_CONNECTED)
			continue;
		if (!includeBots && g_entities[cl - level.clients].r.svFlags & SVF_BOT)
			continue;
		team_t playerTeam;
		if (realTeam)
			playerTeam = GetRealTeam(cl);
		else
			playerTeam = cl->sess.sessionTeam;
		switch (playerTeam) {
		case TEAM_RED: numRed++; break;
		case TEAM_BLUE: numBlue++; break;
		case TEAM_SPECTATOR: numSpec++; break;
		default: numFree++; break;
		}
	}
	if (numRedOut)
		*numRedOut = numRed;
	if (numBlueOut)
		*numBlueOut = numBlue;
	if (numSpecOut)
		*numSpecOut = numSpec;
	if (numFreeOut)
		*numFreeOut = numFree;
}

static isLivePug_t CheckLivePug(char **reasonOut) {
	if (!level.wasRestarted || level.isLivePug == ISLIVEPUG_NO) {
		*reasonOut = "not restarted or already confirmed not live";
		return ISLIVEPUG_NO;
	}
#ifndef _DEBUG
	if (g_cheats.integer || g_siegeRespawn.integer != level.worldspawnSiegeRespawnTime) {
		*reasonOut = "cheats enabled or non-standard respawn time";
		return ISLIVEPUG_NO;
	}
#endif
	if (g_speed.integer != 250 || g_forceRegenTime.integer != 200 || !pmove_float.integer || g_saberDamageScale.value != 1.0f || g_gravity.integer != 760 || (level.siegeMap == SIEGEMAP_KORRIBAN && !(g_knockback.integer == 1000 || !g_knockback.integer)) || g_knockback.integer != 1000) {
		*reasonOut = "non-standard speed, force regen time, pmove_float, saber damage scale, gravity, or knockback";
		return ISLIVEPUG_NO;
	}

	if ((g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none") && Q_stricmp(g_redTeam.string, "0")) ||
		(g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none") && Q_stricmp(g_blueTeam.string, "0"))) {
		*reasonOut = "custom classes in use";
		return ISLIVEPUG_NO;
	}

	int numRed = 0, numBlue = 0;
	for (gclient_t *cl = &level.clients[0]; cl - level.clients < MAX_CLIENTS; cl++) {
		if (cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR)
			continue;
#ifndef _DEBUG
		if (g_entities[cl - level.clients].r.svFlags & SVF_BOT)
			continue;
#endif
		if (cl->sess.sessionTeam == TEAM_RED) {
			numRed++;
		}
		else if (cl->sess.sessionTeam == TEAM_BLUE) {
			numBlue++;
			SpeedRunModeRuined("CheckLivePug: blue team");
		}
	}

	if (numRed != numBlue || numRed < LIVEPUG_MINIMUM_PLAYERS || numBlue < LIVEPUG_MINIMUM_PLAYERS) {
		*reasonOut = "unequal, or too low # players on teams";
		return ISLIVEPUG_NO;
	}

	enum {
		CLIENTNUMAFK_MULTIPLE = -2,
		CLIENTNUMAFK_NONE = -1
	} clientNumAfk = CLIENTNUMAFK_NONE;
	enum {
		CLIENTNUMSELFKILLED_MULTIPLE = -2,
		CLIENTNUMSELFKILLED_NONE = -1
	} clientNumSelfkilled = CLIENTNUMSELFKILLED_NONE;
	for (gclient_t *cl = &level.clients[0]; cl - level.clients < MAX_CLIENTS; cl++) {
		if (cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR)
			continue;
#ifndef _DEBUG
		if (g_entities[cl - level.clients].r.svFlags & SVF_BOT)
			continue;
#endif

		if (!level.movedAtStart[cl - level.clients]) {
			if (clientNumAfk == CLIENTNUMAFK_NONE) {
				clientNumAfk = cl - level.clients;
			}
			else {
				clientNumAfk = CLIENTNUMAFK_MULTIPLE;
				break;
			}
		}
		if (level.selfKilledAtStart[cl - level.clients]) {
			if (clientNumSelfkilled == CLIENTNUMSELFKILLED_NONE) {
				clientNumSelfkilled = cl - level.clients;
			}
			else {
				clientNumSelfkilled = CLIENTNUMSELFKILLED_MULTIPLE;
				break;
			}
		}
	}

	if (clientNumAfk != CLIENTNUMAFK_NONE) {
		if (clientNumAfk == CLIENTNUMAFK_MULTIPLE)
			LivePugRuined("Multiple players are AFK", qtrue);
		else
			LivePugRuined(va("%s^7 is AFK", level.clients[clientNumAfk].pers.netname), qtrue);
		*reasonOut = "AFK";
		return ISLIVEPUG_NO;
	}

	if (clientNumSelfkilled != CLIENTNUMSELFKILLED_NONE) {
		if (clientNumSelfkilled == CLIENTNUMSELFKILLED_MULTIPLE)
			LivePugRuined("Multiple players selfkilled", qtrue);
		else
			LivePugRuined(va("%s^7 selfkilled", level.clients[clientNumSelfkilled].pers.netname), qtrue);
		*reasonOut = "selfkill";
		return ISLIVEPUG_NO;
	}

	return ISLIVEPUG_YES;
}

// this function is called periodically ONLY if the pug is already considered live
static void CheckBadTeamNumbers(void) {
	static qboolean lastCheckWasBadTeamNumbers = qfalse;
	if (!level.teamBalanceCheckTime) {
		level.teamBalanceCheckTime = level.time + LIVEPUG_TEAMBALANCE_CHECK_INTERVAL;
	}
	else if (level.time >= level.teamBalanceCheckTime) {
		int numRed = 0, numBlue = 0;
		GetPlayerCounts(IsDebugBuild, qfalse, &numRed, &numBlue, NULL, NULL);
		if (numRed != numBlue || numRed < LIVEPUG_MINIMUM_PLAYERS || numBlue < LIVEPUG_MINIMUM_PLAYERS) {
			if (lastCheckWasBadTeamNumbers) {
				// failed two consecutive checks; now the pug is no longer live
				// might as well announce it, even though people will likely already realize it
				if (numRed < LIVEPUG_MINIMUM_PLAYERS || numBlue < LIVEPUG_MINIMUM_PLAYERS)
					LivePugRuined("Not enough players ingame", qtrue);
				else
					LivePugRuined("Unequal #s of players on each team", qtrue);
			}
			else {
				// warn the idiots that teams are uneven before it's too late
				lastCheckWasBadTeamNumbers = qtrue;
				trap_SendServerCommand(-1, "cp \"^3Warning:^7 unequal #s of players\n\"");
				trap_SendServerCommand(-1, "print \"^3Warning:^7 unequal #s of players\n\"");
				level.teamBalanceCheckTime = level.time + LIVEPUG_TEAMBALANCE_FAILEDCHECK_INTERVAL; // longer delay
			}
		}
		else {
			lastCheckWasBadTeamNumbers = qfalse;
			level.teamBalanceCheckTime = level.time + LIVEPUG_TEAMBALANCE_CHECK_INTERVAL;
		}
	}
}

void initMatch(){
	int i;
	gentity_t	*ent;

	//G_LogPrintf("initMatch()\n");

	if (!level.wasRestarted)
		return;

	level.initialConditionsMatch = qfalse;

	if (g_gametype.integer != GT_CTF || !g_accounts.integer)
		return;

	//fill roster
	level.initialBlueCount = 0;
	level.initialRedCount = 0;

	for(i=0;i<level.maxclients;++i){
		ent = &g_entities[i];

		if (!ent->inuse || !ent->client)
			continue;
		
		if (ent->client->sess.sessionTeam == TEAM_RED){		
			level.initialRedRoster[level.initialRedCount].clientNum = i;
			strncpy(level.initialRedRoster[level.initialRedCount].username,
				ent->client->sess.username,MAX_USERNAME_SIZE);

			++level.initialRedCount;
		} else if (ent->client->sess.sessionTeam == TEAM_BLUE){		
			level.initialBlueRoster[level.initialBlueCount].clientNum = i;
			strncpy(level.initialBlueRoster[level.initialBlueCount].username,
				ent->client->sess.username,MAX_USERNAME_SIZE);
			++level.initialBlueCount;
		}

	}

	if (level.initialBlueCount < 4 || level.initialRedCount < 4)
		return;
		
	level.initialConditionsMatch = qtrue;

}

// probation; substitute for banning troublemakers
// removes ability to do vote, call votes, use /tell, receive /tell messages, join game without forceteam
static void InitProbation(void) {
	int currentProbationUniqueId = 0;
	for (currentProbationUniqueId = 0; currentProbationUniqueId < MAX_PROBATION_UNIQUEIDS; currentProbationUniqueId++)
		level.probationUniqueIds[currentProbationUniqueId] = 0;
	fileHandle_t probFile;
	int probationLen = trap_FS_FOpenFile("probation.txt", &probFile, FS_READ);

	if (!probFile || probationLen < 20)
		return;
	if (probationLen >= MAX_PROBATION_LEN) {
		G_LogPrintf("probation.txt is too long! Must be under %i bytes.\n", MAX_PROBATION_LEN);
		return;
	}

	char probationBuf[MAX_PROBATION_LEN] = { 0 };
	trap_FS_Read(probationBuf, probationLen, probFile);
	trap_FS_FCloseFile(probFile);

	if (!probationBuf || !probationBuf[0])
		return;

	int pos = 0;
	char *probPtr = probationBuf;
	int startPos;
	int currentLen = 1;
	currentProbationUniqueId = 0;
	while (1) {
		if (currentProbationUniqueId >= MAX_PROBATION_UNIQUEIDS) {
			G_LogPrintf("Too many probation IDs!\n");
			goto doneProbation;
		}
		else if (*probPtr && *probPtr == ' ')
			goto keepGoing;
		else if (!pos || *probPtr && *(probPtr - 1) && *(probPtr - 1) == '\n') {
			startPos = pos;
			currentLen = 1;
		}
		else if (pos && *probPtr && *probPtr == '\n' && *(probPtr - 1) == '\r')
			goto keepGoing;
		else if (*probPtr && *probPtr != '\r' && *probPtr != '\n')
			currentLen++;
		else if (currentLen >= 15 && currentLen <= 20 && (!*probPtr || *probPtr == '\n' || *probPtr == '\r')) {
			char uniqueStr[32] = { 0 };
			char *copyMe = &probationBuf[startPos];
			Q_strncpyz(uniqueStr, copyMe, currentLen + 1);
			level.probationUniqueIds[currentProbationUniqueId] = strtoull(uniqueStr, NULL, 10);
			G_LogPrintf("Unique ID %llu is under probation\n", level.probationUniqueIds[currentProbationUniqueId]);
			currentProbationUniqueId++;
			if (!*probPtr)
				goto doneProbation;
		}
		else if (!*probPtr)
			goto doneProbation;
	keepGoing:;
		pos++;
		probPtr++;
	}
doneProbation:;
	if (currentProbationUniqueId)
		G_LogPrintf("%i total unique IDs are under probation\n", currentProbationUniqueId);
}

qboolean G_ClientIsWhitelisted(int clientNum) {
	if (clientNum < 0 || clientNum >= MAX_CLIENTS || !&level.clients[clientNum] || !&level.clients[clientNum].sess)
		return qfalse;

	gclient_t *client = &level.clients[clientNum];

	if (client->sess.whitelistStatus == WHITELIST_NOTWHITELISTED)
		return qfalse; // status is already known to not be whitelisted; don't spam db calls
	if (client->sess.whitelistStatus == WHITELIST_WHITELISTED)
		return qtrue; // status is already known to be whitelisted; don't spam db calls

	// status is unknown; check it in the db now
	unsigned long long id = level.clientUniqueIds[clientNum];
	char *cuid = client->sess.auth == AUTHENTICATED ? client->sess.cuidHash : "";
	if (G_DBPlayerLockdownWhitelisted(id, cuid)) {
		// according to the db, he is whitelisted
		client->sess.whitelistStatus = WHITELIST_WHITELISTED;
		return qtrue;
	}

	// couldn't find a match in the db
	client->sess.whitelistStatus = WHITELIST_NOTWHITELISTED;
	return qfalse;
}

qboolean G_ClientIsOnProbation(int clientNum) {
	if (clientNum < 0 || clientNum >= MAX_CLIENTS || !&level || !level.clientUniqueIds[clientNum])
		return qfalse;

	int i;
	for (i = 0; i < MAX_PROBATION_UNIQUEIDS; i++) {
		if (level.probationUniqueIds[i] && level.probationUniqueIds[i] == level.clientUniqueIds[clientNum])
			return qtrue;
	}
	return qfalse;
}

static void InitializeMapName(void) {
	trap_Cvar_VariableStringBuffer("mapname", level.mapname, sizeof(level.mapname));
	Q_strlwr(level.mapname);
	Q_strncpyz(level.mapCaptureRecords.mapname, level.mapname, sizeof(level.mapCaptureRecords.mapname));
	if (!Q_stricmpn(level.mapname, "mp/siege_hoth", 13) || !Q_stricmpn(level.mapname, "siege_hoth", 10)) {
		level.siegeMap = SIEGEMAP_HOTH;
		level.numSiegeObjectivesOnMapCombined = 6;
	} else if (!Q_stricmp(level.mapname, "mp/siege_desert")) {
		level.siegeMap = SIEGEMAP_DESERT;
		level.numSiegeObjectivesOnMapCombined = 5;
	} else if (!Q_stricmp(level.mapname, "mp/siege_korriban")) {
		level.siegeMap = SIEGEMAP_KORRIBAN;
		level.numSiegeObjectivesOnMapCombined = 4;
	} else if (!Q_stricmp(level.mapname, "siege_narshaddaa")) {
		level.siegeMap = SIEGEMAP_NAR;
		level.numSiegeObjectivesOnMapCombined = 5;
	} else if (stristr(level.mapname, "siege_urban")) {
		level.siegeMap = SIEGEMAP_URBAN;
		level.numSiegeObjectivesOnMapCombined = 5;
	} else if (stristr(level.mapname, "siege_cargobarge3") || stristr(level.mapname, "siege_cargobarge2")) {
		level.siegeMap = SIEGEMAP_CARGO;
		level.numSiegeObjectivesOnMapCombined = 6;
	} else if (stristr(level.mapname, "mp/siege_bespin")) {
		level.siegeMap = SIEGEMAP_BESPIN;
		level.numSiegeObjectivesOnMapCombined = 6;
	} else if (stristr(level.mapname, "siege_ansion")) {
		level.siegeMap = SIEGEMAP_ANSION;
		level.numSiegeObjectivesOnMapCombined = 5;
	} else if (stristr(level.mapname, "siege_imperial")){
		level.siegeMap = SIEGEMAP_IMPERIAL;
		level.numSiegeObjectivesOnMapCombined = 5;
	} else if (stristr(level.mapname, "duel")) {
		level.siegeMap = SIEGEMAP_DUEL;
	} else {
		level.siegeMap = SIEGEMAP_UNKNOWN;
		if (!Q_stricmp(level.mapname, "siege_cargobarge"))
			level.numSiegeObjectivesOnMapCombined = 5;
		else if (!Q_stricmp(level.mapname, "mp/siege_eat_shower"))
			level.numSiegeObjectivesOnMapCombined = 6;
		else if (!Q_stricmp(level.mapname, "mp/siege_destroyer"))
			level.numSiegeObjectivesOnMapCombined = 2;
		else if (!Q_stricmp(level.mapname, "mp/siege_alzocIII"))
			level.numSiegeObjectivesOnMapCombined = 5;
	}
}

char gSharedBuffer[MAX_G_SHARED_BUFFER_SIZE];

#include "namespace_begin.h"
void WP_SaberLoadParms( void );
void BG_VehicleLoadParms( void );
#include "namespace_end.h"

static void LoadPepper(void) {
	fileHandle_t f = 0;
	int len = trap_FS_FOpenFile("pepper.bin", &f, FS_READ);

	if (len != PEPPER_CHARS) {
		if (f)
			trap_FS_FCloseFile(f);

		trap_FS_FOpenFile("pepper.bin", &f, FS_WRITE);

		if (!f) {
			Com_Error(ERR_FATAL, "Unable to acquire file handle on pepper");
			return;
		}

		unsigned char arr[PEPPER_CHARS];
		randombytes_buf(arr, PEPPER_CHARS);
		memcpy(level.pepper, arr, PEPPER_CHARS);
		trap_FS_Write(arr, PEPPER_CHARS, f);
	}
	else {
		trap_FS_Read(&level.pepper, len, f);
	}

	trap_FS_FCloseFile(f);
}

/*
============
G_InitGame

============
*/
extern void RemoveAllWP(void);
extern void BG_ClearVehicleParseParms(void);
extern void G_LoadArenas( void );
extern void G_LoadVoteMapsPools(void);
extern void InitUnhandledExceptionFilter();

void G_InitGame( int levelTime, int randomSeed, int restart, void *serverDbPtr ) {
	int					i;
	vmCvar_t	mapname;
	vmCvar_t	ckSum;

	InitUnhandledExceptionFilter();
	InitializeMapName();

	// if we changed siege maps, reset back to round 1
	char lastMapName[MAX_QPATH] = { 0 };
	trap_Cvar_VariableStringBuffer("lastMapName", lastMapName, sizeof(lastMapName));
	int gametype = trap_Cvar_VariableIntegerValue("g_gametype");
	int siegeTeamSwitch = trap_Cvar_VariableIntegerValue("g_siegeTeamSwitch");
	if (gametype == GT_SIEGE && siegeTeamSwitch && lastMapName[0] && Q_stricmp(lastMapName, level.mapname))
		SiegeClearSwitchData();
	trap_Cvar_Set("lastMapName", level.mapname);

#ifdef _XBOX
	if(restart) {
		BG_ClearVehicleParseParms();
		RemoveAllWP();
	}
#endif

	//Init RMG to 0, it will be autoset to 1 if there is terrain on the level.
	trap_Cvar_Set("RMG", "0");
	g_RMG.integer = 0;

	//Clean up any client-server ghoul2 instance attachments that may still exist exe-side
	trap_G2API_CleanEntAttachments();

	BG_InitAnimsets(); //clear it out

	B_InitAlloc(); //make sure everything is clean

	trap_SV_RegisterSharedMemory(gSharedBuffer);

	//Load external vehicle data
	BG_VehicleLoadParms();

	G_Printf ("------- Game Initialization -------\n");
	G_Printf ("gamename: %s\n", GAMEVERSION);
	G_Printf ("gamedate: %s\n", __DATE__);

	srand( randomSeed );

	G_RegisterCvars();

	G_ProcessGetstatusIPBans();

	G_InitMemory();

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;
	level.startTime = levelTime;

	InitializeMapName();

	level.snd_fry = G_SoundIndex("sound/player/fry.wav");	// FIXME standing in lava / slime

	level.snd_hack = G_SoundIndex("sound/player/hacking.wav");
	level.snd_medHealed = G_SoundIndex("sound/player/supp_healed.wav");
	level.snd_medSupplied = G_SoundIndex("sound/player/supp_supplied.wav");

	InitProbation();

#ifndef _XBOX
	if ( g_log.string[0] ) {
		char	serverinfo[MAX_INFO_STRING];

		trap_GetServerinfo( serverinfo, sizeof( serverinfo ) );

		if ( g_logSync.integer ) {
			trap_FS_FOpenFile( g_log.string, &level.logFile, FS_APPEND_SYNC );
		} else {
			trap_FS_FOpenFile( g_log.string, &level.logFile, FS_APPEND );
		}

		if (g_hackLog.string[0]){
			trap_FS_FOpenFile( g_hackLog.string, &level.hackLogFile, FS_APPEND_SYNC );

			if ( !level.hackLogFile ) {
				G_Printf( "WARNING: Couldn't open crash logfile: %s\n", g_hackLog.string );
			}
		}	

		if ( !level.logFile ) {
			G_Printf( "WARNING: Couldn't open logfile: %s\n", g_log.string );
		} else {
			G_LogPrintf("------------------------------------------------------------\n" );
			G_LogPrintf("Timestamp: %s\n", getCurrentTime() ); //contains \n already :S
			G_LogPrintf("InitGame: %s\n", serverinfo );
		}

		if (g_logrcon.integer){
			trap_FS_FOpenFile( "rcon.log", &level.rconLogFile, FS_APPEND_SYNC );
			if (!level.rconLogFile){
				G_Printf( "WARNING: Couldn't open rcon logfile: %s\n", "rcon.log" );
			}
		}

		// accounts system
		//if (g_accounts.integer && db_log.integer){
		//	trap_FS_FOpenFile( "db.log", &level.DBLogFile, FS_APPEND_SYNC ); //remove SYNC in release

		//	if (!level.DBLogFile){
		//		G_Printf( "WARNING: Couldn't open dbfile: %s\n", "db.log" );
		//	} else {
		//		int	min, tens, sec;
		//		sec = level.time / 1000;

		//		min = sec / 60;
		//		sec -= min * 60;
		//		tens = sec / 10;
		//		sec -= tens * 10;

		//		G_DBLog("------------------------------------------------------------\n" );
		//		G_DBLog("InitGame: %3i:%i%i %s\n", min, tens, sec, serverinfo );
		//	}
		//}

	} else {
		G_Printf( "Not logging to disk.\n" );
	}
#endif

	if (restart && !g_wasIntermission.integer)
		level.wasRestarted = qtrue;
	trap_Cvar_Set("g_wasIntermission", "0");

#ifdef NEWMOD_SUPPORT
	level.nmAuthEnabled = qfalse;

	if (Crypto_Init(G_Printf) == CRYPTO_ERROR) {
		Com_Error(ERR_FATAL, "Crypto_Init failed!");
		return;
	}

	if ( g_enableNmAuth.integer ) {
		if ( Crypto_LoadKeysFromFiles( &level.publicKey, PUBLIC_KEY_FILENAME, &level.secretKey, SECRET_KEY_FILENAME ) != CRYPTO_ERROR ) {
			// got the keys, all is good
			G_Printf( "Loaded crypto key files from disk successfully\n" );
			level.nmAuthEnabled = qtrue;
		} else if ( Crypto_GenerateKeys( &level.publicKey, &level.secretKey ) != CRYPTO_ERROR ) {
			// either first run or file couldnt be read. whatever, we got a key pair for this session
			G_Printf( "Generated new crypto key pair successfully:\nPUBLIC=%s\nSECRET=%s\n", level.publicKey.keyHex, level.secretKey.keyHex );
			level.nmAuthEnabled = qtrue;

			// save them to disk for the next time
			Crypto_SaveKeysToFiles( &level.publicKey, PUBLIC_KEY_FILENAME, &level.secretKey, SECRET_KEY_FILENAME );
		}
	}

	if ( !level.nmAuthEnabled ) {
		G_Printf( S_COLOR_RED"Newmod auth support is not active. Some functionality will be unavailable for these clients.\n" );
	}
#endif

	LoadPepper();

	// accounts system
	//initDB();

	////load accounts
	//if (g_accounts.integer){
	//	loadAccounts();
	//}

	G_LogWeaponInit();

	G_InitWorldSession();

	// initialize all entities for this game
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]) );
	level.gentities = g_entities;

	// initialize all clients for this game
	level.maxclients = g_maxclients.integer;
	memset( g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]) );
	level.clients = g_clients;

	// set client fields on player ents
	for ( i=0 ; i<level.maxclients ; i++ ) {
		g_entities[i].client = level.clients + i;
	}

	// read accounts and sessions cache
	qboolean resetAccountsCache = !G_ReadAccountsCache();

	G_ReadSessionData(restart ? qtrue : qfalse, resetAccountsCache);

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	level.num_entities = MAX_CLIENTS;

	// let the server system know where the entites are
	trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	//Load sabers.cfg data
	WP_SaberLoadParms();

	NPC_InitGame();

	TIMER_Clear();
	//
	//ICARUS INIT START

	trap_ICARUS_Init();

	//ICARUS INIT END
	//

	// reserve some spots for dead player bodies
	InitBodyQue();

	ClearRegisteredItems();

	//make sure saber data is loaded before this! (so we can precache the appropriate hilts)
	InitSiegeMode();

	trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
	trap_Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

	navCalculatePaths	= ( trap_Nav_Load( mapname.string, ckSum.integer ) == qfalse );

	G_DBLoadDatabase(serverDbPtr);

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString(qfalse);

	// general initialization
	G_FindTeams();

	// clean intermission configstring flag
	trap_SetConfigstring( CS_INTERMISSION, "0" );

	// make sure we have flags for CTF, etc
	if( g_gametype.integer >= GT_TEAM ) {
		G_CheckTeamItems();
	}
	else if ( g_gametype.integer == GT_JEDIMASTER )
	{
		trap_SetConfigstring ( CS_CLIENT_JEDIMASTER, "-1" );
	}

	if (g_gametype.integer == GT_POWERDUEL)
	{
		trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1|-1") );
	}
	else
	{
		trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1") );
	}
// nmckenzie: DUEL_HEALTH: Default.
	trap_SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("-1|-1|!") );
	trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("-1") );

	SaveRegisteredItems();

	if( g_gametype.integer == GT_SINGLE_PLAYER || trap_Cvar_VariableIntegerValue( "com_buildScript" ) ) {
		G_ModelIndex( SP_PODIUM_MODEL );
		G_SoundIndex( "sound/player/gurp1.wav" );
		G_SoundIndex( "sound/player/gurp2.wav" );
	}

	G_LoadArenas(); //*CHANGE 93* loading map list not dependant on bot_enable cvar
    G_InitVoteMapsLimit();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( restart );
	}

	G_RemapTeamShaders();

	if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
	{
		G_LogPrintf("Duel Tournament Begun: kill limit %d, win limit: %d\n", g_fraglimit.integer, g_duel_fraglimit.integer );
	}

	if ( navCalculatePaths )
	{//not loaded - need to calc paths
		navCalcPathTime = level.time + START_TIME_NAV_CALC;//make sure all ents are in and linked
	}
	else
	{//loaded
		//FIXME: if this is from a loadgame, it needs to be sure to write this 
		//out whenever you do a savegame since the edges and routes are dynamic...
		//OR: always do a navigator.CheckBlockedEdges() on map startup after nav-load/calc-paths
		//navigator.pathsCalculated = qtrue;//just to be safe?  Does this get saved out?  No... assumed
		trap_Nav_SetPathsCalculated(qtrue);
		//need to do this, because combatpoint waypoints aren't saved out...?
		CP_FindCombatPointWaypoints();
		navCalcPathTime = 0;

	}

	if (g_gametype.integer == GT_SIEGE)
	{ //just get these configstrings registered now...
		int i = 0;
		while (i < MAX_CUSTOM_SIEGE_SOUNDS)
		{
			if (!bg_customSiegeSoundNames[i])
			{
				break;
			}
			G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
			i++;
		}
	}

    if ( BG_IsLegacyEngine() )
    {
		PatchEngine();
	}

    //level.db.levelId = G_LogDbLogLevelStart(restart);

	InitializeSiegeHelpMessages();

	G_BroadcastServerFeatureList(-1);

#ifdef _DEBUG
	Com_Printf("Build date: %s %s\n", __DATE__, __TIME__);
#endif
	G_InitVchats();
}



/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame( int restart ) {
	int i = 0;
	gentity_t *ent;

	G_CleanAllFakeClients(); //get rid of dynamically allocated fake client structs.

	BG_ClearAnimsets(); //free all dynamic allocations made through the engine

	while (i < MAX_GENTITIES)
	{ //clean up all the ghoul2 instances
		ent = &g_entities[i];

		if (ent->ghoul2 && trap_G2_HaveWeGhoul2Models(ent->ghoul2))
		{
			trap_G2API_CleanGhoul2Models(&ent->ghoul2);
			ent->ghoul2 = NULL;
		}
		if (ent->client)
		{
			int j = 0;

			while (j < MAX_SABERS)
			{
				if (ent->client->weaponGhoul2[j] && trap_G2_HaveWeGhoul2Models(ent->client->weaponGhoul2[j]))
				{
					trap_G2API_CleanGhoul2Models(&ent->client->weaponGhoul2[j]);
				}
				j++;
			}
		}
		i++;
	}
	if (g2SaberInstance && trap_G2_HaveWeGhoul2Models(g2SaberInstance))
	{
		trap_G2API_CleanGhoul2Models(&g2SaberInstance);
		g2SaberInstance = NULL;
	}
	if (precachedKyle && trap_G2_HaveWeGhoul2Models(precachedKyle))
	{
		trap_G2API_CleanGhoul2Models(&precachedKyle);
		precachedKyle = NULL;
	}

	trap_ICARUS_Shutdown ();	//Shut ICARUS down

	TAG_Init();	//Clear the reference tags

	G_LogWeaponOutput();

	if ( level.logFile ) {
		G_LogPrintf("ShutdownGame:\n" );
		G_LogPrintf("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.logFile );
	}

	if ( level.hackLogFile ) {
		trap_FS_FCloseFile( level.hackLogFile );
	}

	if ( level.DBLogFile ) {
		G_DBLog("ShutdownGame:\n" );
		G_DBLog("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.DBLogFile );
	}

	if ( level.rconLogFile ) {
		trap_FS_FCloseFile( level.rconLogFile );
	}

	// write all the client session data so we can get it back
	G_WriteSessionData();

	// save accounts and sessions cache
	G_SaveAccountsCache();

	trap_ROFF_Clean();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}

	B_CleanupAlloc(); //clean up all allocations made with B_Alloc

	kd_free( level.locations.enhanced.lookupTree );

	// accounts system
	//cleanDB();

	ListClear(&level.mapCaptureRecords.captureRecordsList);
	ListClear(&level.siegeHelpList);

    //G_LogDbLogLevelEnd(level.db.levelId);

	G_DBUnloadDatabase();

	UnpatchEngine();
}



//===================================================================

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and bg_*.c can link

void QDECL Com_Error ( int level, const char *error, ... ) {
	va_list		argptr;
	char		text[1024] = { 0 };
	
	G_LogPrintf("Com_Error (%i): %s", level, error);
	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);
	
	G_Error( "%s", text);
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[4096] = { 0 };

	va_start (argptr, msg);
	vsnprintf (text, sizeof(text), msg, argptr);
	va_end (argptr);

	G_Printf ("%s", text);
}

#endif

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void ) {
	int			i;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 2 ) {
		return;
	}

	nextInLine = NULL;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if (!g_allowHighPingDuelist.integer && client->ps.ping >= 999)
		{ //don't add people who are lagging out if cvar is not set to allow it.
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD || 
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorTime < nextInLine->sess.spectatorTime ) {
			nextInLine = client;
		}
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" , qfalse);
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[1];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" , qfalse);
}

void G_PowerDuelCount(int *loners, int *doubles, qboolean countSpec)
{
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS)
	{
		cl = g_entities[i].client;

		if (g_entities[i].inuse && cl && (countSpec || cl->sess.sessionTeam != TEAM_SPECTATOR))
		{
			if (cl->sess.duelTeam == DUELTEAM_LONE)
			{
				(*loners)++;
			}
			else if (cl->sess.duelTeam == DUELTEAM_DOUBLE)
			{
				(*doubles)++;
			}
		}
		i++;
	}
}

qboolean g_duelAssigning = qfalse;
void AddPowerDuelPlayers( void )
{
	int			i;
	int			loners = 0;
	int			doubles = 0;
	int			nonspecLoners = 0;
	int			nonspecDoubles = 0;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 3 )
	{
		return;
	}

	nextInLine = NULL;

	G_PowerDuelCount(&nonspecLoners, &nonspecDoubles, qfalse);
	if (nonspecLoners >= 1 && nonspecDoubles >= 2)
	{ //we have enough people, stop
		return;
	}

	//Could be written faster, but it's not enough to care I suppose.
	G_PowerDuelCount(&loners, &doubles, qtrue);

	if (loners < 1 || doubles < 2)
	{ //don't bother trying to spawn anyone yet if the balance is not even set up between spectators
		return;
	}

	//Count again, with only in-game clients in mind.
	loners = nonspecLoners;
	doubles = nonspecDoubles;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_FREE)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
		{
			continue;
		}

		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD || 
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorTime < nextInLine->sess.spectatorTime ) {
			nextInLine = client;
		}
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" , qfalse);

	//Call recursively until everyone is in
	AddPowerDuelPlayers();
}

qboolean g_dontFrickinCheck = qfalse;

void RemovePowerDuelLosers(void)
{
	int remClients[3];
	int remNum = 0;
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS && remNum < 3)
	{
		cl = &level.clients[i];

		if (cl->pers.connected == CON_CONNECTED)
		{
			if ((cl->ps.stats[STAT_HEALTH] <= 0 || cl->iAmALoser) &&
				(cl->sess.sessionTeam != TEAM_SPECTATOR || cl->iAmALoser))
			{ //he was dead or he was spectating as a loser
                remClients[remNum] = cl->ps.clientNum;
				remNum++;
			}
		}

		i++;
	}

	if (!remNum)
	{ //Time ran out or something? Oh well, just remove the main guy.
		remClients[remNum] = level.sortedClients[0];
		remNum++;
	}

	i = 0;
	while (i < remNum)
	{ //set them all to spectator
		SetTeam( &g_entities[ remClients[i] ], "s" ,qfalse);
		i++;
	}

	g_dontFrickinCheck = qfalse;

	//recalculate stuff now that we have reset teams.
	CalculateRanks();
}

void RemoveDuelDrawLoser(void)
{
	int clFirst = 0;
	int clSec = 0;
	int clFailure = 0;

	if ( level.clients[ level.sortedClients[0] ].pers.connected != CON_CONNECTED )
	{
		return;
	}
	if ( level.clients[ level.sortedClients[1] ].pers.connected != CON_CONNECTED )
	{
		return;
	}

	clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
	clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];

	if (clFirst > clSec)
	{
		clFailure = 1;
	}
	else if (clSec > clFirst)
	{
		clFailure = 0;
	}
	else
	{
		clFailure = 2;
	}

	if (clFailure != 2)
	{
		SetTeam( &g_entities[ level.sortedClients[clFailure] ], "s" ,qfalse);
	}
	else
	{ //we could be more elegant about this, but oh well.
		SetTeam( &g_entities[ level.sortedClients[1] ], "s" ,qfalse);
	}
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[0];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" ,qfalse);
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void ) {
	int			clientNum;

	if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
		level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
		level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
		level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
	{
		int clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
		int clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];
		int clFailure = 0;
		int clSuccess = 0;

		if (clFirst > clSec)
		{
			clFailure = 1;
			clSuccess = 0;
		}
		else if (clSec > clFirst)
		{
			clFailure = 0;
			clSuccess = 1;
		}
		else
		{
			clFailure = 2;
			clSuccess = 2;
		}

		if (clFailure != 2)
		{
			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
		else
		{
			clSuccess = 0;
			clFailure = 1;

			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
	else
	{
		clientNum = level.sortedClients[0];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );

			trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );
		}

		clientNum = level.sortedClients[1];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b ) {
	gclient_t	*ca, *cb;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	if (g_gametype.integer == GT_POWERDUEL)
	{
		//sort single duelists first
		if (ca->sess.duelTeam == DUELTEAM_LONE && ca->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return -1;
		}
		if (cb->sess.duelTeam == DUELTEAM_LONE && cb->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return 1;
		}

		//others will be auto-sorted below but above spectators.
	}

	// sort special clients last
	if ( ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorClient < 0 ) {
		return 1;
	}
	if ( cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorClient < 0  ) {
		return -1;
	}

	// then connecting clients
	if ( ca->pers.connected == CON_CONNECTING ) {
		return 1;
	}
	if ( cb->pers.connected == CON_CONNECTING ) {
		return -1;
	}


	// then spectators
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ca->sess.spectatorTime < cb->sess.spectatorTime ) {
			return -1;
		}
		if ( ca->sess.spectatorTime > cb->sess.spectatorTime ) {
			return 1;
		}
		return 0;
	}
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR ) {
		return 1;
	}
	if ( cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		return -1;
	}

	// then sort by score
	if ( ca->ps.persistant[PERS_SCORE]
		> cb->ps.persistant[PERS_SCORE] ) {
		return -1;
	}
	if ( ca->ps.persistant[PERS_SCORE]
		< cb->ps.persistant[PERS_SCORE] ) {
		return 1;
	}
	return 0;
}

qboolean gQueueScoreMessage = qfalse;
int gQueueScoreMessageTime = 0;

//A new duel started so respawn everyone and make sure their stats are reset
qboolean G_CanResetDuelists(void)
{
	int i;
	gentity_t *ent;

	i = 0;
	while (i < 3)
	{ //precheck to make sure they are all respawnable
		ent = &g_entities[level.sortedClients[i]];

		if (!ent->inuse || !ent->client || ent->health <= 0 ||
			ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
			ent->client->sess.duelTeam <= DUELTEAM_FREE)
		{
			return qfalse;
		}
		i++;
	}

	return qtrue;
}

qboolean g_noPDuelCheck = qfalse;
void G_ResetDuelists(void)
{
	int i;
	gentity_t *ent;
	gentity_t *tent;

	i = 0;
	while (i < 3)
	{
		ent = &g_entities[level.sortedClients[i]];

		g_noPDuelCheck = qtrue;
		player_die(ent, ent, ent, 999, MOD_SUICIDE);
		g_noPDuelCheck = qfalse;
		trap_UnlinkEntity (ent);
		ClientSpawn(ent, qfalse);

		// add a teleportation effect
		tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = ent->s.clientNum;
		i++;
	}
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void ) {
	int		i;
	int		rank;
	int		score;
	int		newScore;
	gclient_t	*cl;

	level.follow1 = -1;
	level.follow2 = -1;
	level.numConnectedClients = 0;
	level.numNonSpectatorClients = 0;
	level.numPlayingClients = 0;
	for ( i = 0; i < 2; i++ ) {
		level.numteamVotingClients[i] = 0;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			level.sortedClients[level.numConnectedClients] = i;
			level.numConnectedClients++;

			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
			{
				if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR)
				{
					level.numNonSpectatorClients++;
				}
			
				// decide if this should be auto-followed
				if ( level.clients[i].pers.connected == CON_CONNECTED )
				{
					if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || level.clients[i].iAmALoser)
					{
						level.numPlayingClients++;
					}
					if ( !(g_entities[i].r.svFlags & SVF_BOT) )
					{
						if ( level.clients[i].sess.sessionTeam == TEAM_RED )
							level.numteamVotingClients[0]++;
						else if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							level.numteamVotingClients[1]++;
					}
					if ( level.follow1 == -1 ) {
						level.follow1 = i;
					} else if ( level.follow2 == -1 ) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	//NOTE: for now not doing this either. May use later if appropriate.

	qsort( level.sortedClients, level.numConnectedClients, 
		sizeof(level.sortedClients[0]), SortRanks );

	// set the rank value for all clients that are connected and not spectators
	if ( g_gametype.integer >= GT_TEAM ) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for ( i = 0;  i < level.numConnectedClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			if ( level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	} else {	
		rank = -1;
		score = 0;
		for ( i = 0;  i < level.numPlayingClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			newScore = cl->ps.persistant[PERS_SCORE];
			if ( i == 0 || newScore != score ) {
				rank = i;
				// assume we aren't tied until the next client is checked
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank;
			} else {
				// we are tied with the previous client
				level.clients[ level.sortedClients[i-1] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
			score = newScore;
			if ( g_gametype.integer == GT_SINGLE_PLAYER && level.numPlayingClients == 1 ) {
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
		}
	}

	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
	if ( g_gametype.integer >= GT_TEAM ) {
		trap_SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
		trap_SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
	} else {
		if ( level.numConnectedClients == 0 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else if ( level.numConnectedClients == 1 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] ) );
		}

		if (g_gametype.integer != GT_DUEL || g_gametype.integer != GT_POWERDUEL)
		{ //when not in duel, use this configstring to pass the index of the player currently in first place
			if ( level.numConnectedClients >= 1 )
			{
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", level.sortedClients[0] ) );
			}
			else
			{
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	// see if it is time to end the level
	CheckExitRules();

	// if we are at the intermission or in multi-frag Duel game mode, send the new info to everyone
	if ( level.intermissiontime || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL ) {
		gQueueScoreMessage = qtrue;
		gQueueScoreMessageTime = level.time + 500;
		//rww - Made this operate on a "queue" system because it was causing large overflows
	}
}

int getOrder(int* topArray, int value, qboolean biggerIsBetter){
	if (biggerIsBetter){
		if (value >= topArray[0]) //bettter than best
			return 0;
		else if (value >= topArray[1])
			return 1;
		else if (value >= topArray[2])
			return 2;
	} else {
		if (value <= topArray[0]) //bettter than best
			return 0;
		else if (value <= topArray[1])
			return 1;
		else if (value <= topArray[2])
			return 2;
	}

	return 3;
}

/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[ i ].pers.connected == CON_CONNECTED && !level.clients[i].isLagging ) {
			DeathmatchScoreboardMessage( g_entities + i );
		}
	}
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
void MoveClientToIntermission( gentity_t *ent ) {
	// take out of follow mode if needed
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		StopFollowing( ent );
	}


	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, ent->client->ps.origin );
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pm_type = PM_INTERMISSION;

	// clean up powerup info
	memset( ent->client->ps.powerups, 0, sizeof(ent->client->ps.powerups) );

	ent->client->ps.eFlags = 0;
	ent->s.eFlags = 0;
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->s.loopSound = 0;
	ent->s.loopIsSoundset = qfalse;
	ent->s.event = 0;
	ent->r.contents = 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
extern qboolean	gSiegeRoundBegun;
extern qboolean	gSiegeRoundEnded;
extern qboolean	gSiegeRoundWinningTeam;
void FindIntermissionPoint( void ) {
	gentity_t	*ent = NULL;
	gentity_t	*target;
	vec3_t		dir;

	// find the intermission spot
	if ( g_gametype.integer == GT_SIEGE
		&& level.intermissiontime
		&& level.intermissiontime <= level.time
		&& gSiegeRoundEnded )
	{
	   	if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM1)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_red");
			if ( ent && ent->target2 ) 
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	   	else if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM2)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_blue");
			if ( ent && ent->target2 ) 
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	}
	if ( !ent )
	{
		ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	}
	if ( !ent ) {	// the map creator forgot to put in an intermission point...
		SelectSpawnPoint ( vec3_origin, level.intermission_origin, level.intermission_angle, TEAM_SPECTATOR );
	} else {
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);
		// if it has a target, look towards it
		if ( ent->target ) {
			target = G_PickTarget( ent->target );
			if ( target ) {
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}

}

qboolean DuelLimitHit(void);

/*
==================
BeginIntermission
==================
*/
//ghost debug

extern void PrintStatsTo( gentity_t *ent, const char *type, char* outputBuffer, size_t outSize );

void BeginIntermission(void) {
	int			i;
	gentity_t* client;

	if (level.intermissiontime) {
		return;		// already active
	}

	trap_Cvar_Set("g_wasIntermission", "1");

	// if in tournement mode, change the wins / losses
	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) {
		trap_SetConfigstring(CS_CLIENT_DUELWINNER, "-1");

		if (g_gametype.integer != GT_POWERDUEL)
		{
			AdjustTournamentScores();
		}
		if (DuelLimitHit())
		{
			gDuelExit = qtrue;
		}
		else
		{
			gDuelExit = qfalse;
		}
	}

	//*CHANGE 32* printing tops on intermission
	if (g_gametype.integer == GT_CTF) {//NYI
	}

	level.intermissiontime = level.time;
	FindIntermissionPoint();

	//what the? Well, I don't want this to happen.

	// move all clients to the intermission point
	for (i = 0; i < level.maxclients; i++) {
		client = g_entities + i;
		if (!client->inuse)
			continue;
		// respawn if dead
		if (client->health <= 0) {
			if (g_gametype.integer != GT_POWERDUEL ||
				!client->client ||
				client->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ //don't respawn spectators in powerduel or it will mess the line order all up
				respawn(client);
			}
		}
		MoveClientToIntermission(client);
	}

	// send the current scoring to all clients
	SendScoreboardMessageToAllClients();

	char statsBuf[4096] = { 0 };
	if (g_autoStats.integer) {
		if (g_gametype.integer == GT_SIEGE) {
			PrintStatsTo(NULL, "obj", statsBuf, sizeof(statsBuf));
			PrintStatsTo(NULL, "general", statsBuf, sizeof(statsBuf));
			if (level.siegeMap != SIEGEMAP_UNKNOWN && level.siegeMap != SIEGEMAP_IMPERIAL && level.siegeMap != SIEGEMAP_DUEL)
				PrintStatsTo(NULL, "map", statsBuf, sizeof(statsBuf));
			Q_StripColor(statsBuf);
		}
		else if (g_gametype.integer == GT_CTF) {
			PrintStatsTo(NULL, "general", statsBuf, sizeof(statsBuf));
			PrintStatsTo(NULL, "force", statsBuf, sizeof(statsBuf));
			Q_StripColor(statsBuf);
		}
	}

	if (level.numTeamTicks) {
		float avgRed = (float)level.numRedPlayerTicks / (float)level.numTeamTicks;
		float avgBlue = (float)level.numBluePlayerTicks / (float)level.numTeamTicks;

		int avgRedInt = (int)lroundf(avgRed);
		int avgBlueInt = (int)lroundf(avgBlue);

		// post scoreboard to webhook if:
		// * live pug (was restarted, etc)
		// * round 2
		// * the average rounded integer number of players in each team is equal
		// * the sum of these average integers is >= 4 (at least 2s)
		// * both averages are within +/- 0.1 of their rounded values
		// (accounts for subs, ragequits, random joins... 0.1 represents 2 mins of a 20 mins pug)
		if (
#ifndef _DEBUG
			level.pugWasLiveAtSomePoint &&
#endif
			level.siegeStage >= SIEGESTAGE_ROUND2 &&
			avgRedInt == avgBlueInt &&
			avgRedInt + avgBlueInt >= 4 &&
			fabs(avgRed - round(avgRed)) < 0.1f &&
			fabs(avgBlue - round(avgBlue)) < 0.1f)
		{
			G_PostScoreboardToWebhook(statsBuf);
		}
	}

	char timeBuf[MAX_STRING_CHARS] = { 0 };
	Com_sprintf(timeBuf, sizeof(timeBuf), "%d", (int)time(NULL));
	trap_Cvar_Set("g_lastIntermissionStartTime", timeBuf);
}

qboolean DuelLimitHit(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		if ( g_duel_fraglimit.integer && cl->sess.wins >= g_duel_fraglimit.integer )
		{
			return qtrue;
		}
	}

	return qfalse;
}

void DuelResetWinsLosses(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		cl->sess.wins = 0;
		cl->sess.losses = 0;
	}
}

/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar 

=============
*/
extern void SiegeDoTeamAssign(void); //g_saga.c
extern siegePers_t g_siegePersistant; //g_saga.c
void ExitLevel (void) {
	int		i;
	gclient_t *cl;

	// if we are running a tournement map, kick the loser to spectator status,
	// which will automatically grab the next spectator and restart
	if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL ) {
		if (!DuelLimitHit())
		{
			if ( !level.restarted ) {
				trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
				level.restarted = qtrue;
				level.changemap = NULL;
				level.intermissiontime = 0;
			}
			return;	
		}

		DuelResetWinsLosses();
	}


	if (g_gametype.integer == GT_SIEGE &&
		g_siegeTeamSwitch.integer &&
		g_siegePersistant.beatingTime)
	{ //restart same map...
		trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
	}
	else
	{
		trap_SendConsoleCommand( EXEC_APPEND, "vstr nextmap\n" );
	}
	level.changemap = NULL;
	level.intermissiontime = 0;

	if (g_gametype.integer == GT_SIEGE && g_siegeTeamSwitch.integer && (!g_speedrunRoundOneRestart.integer || !(level.isLivePug != ISLIVEPUG_YES && !level.mapCaptureRecords.speedRunModeRuined)))
	{ //switch out now, unless map was completed under speedrun conditions
		SiegeDoTeamAssign();
	}

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED] = 0;
	level.teamScores[TEAM_BLUE] = 0;
	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;
	}

	// we need to do this here before chaning to CON_CONNECTING
	G_WriteSessionData();

	// save accounts and sessions cache
	G_SaveAccountsCache();

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for (i=0 ; i< g_maxclients.integer ; i++) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			level.clients[i].pers.connected = CON_CONNECTING;
		}
	}

}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024] = { 0 };
	int			/*t,*/ min, sec, len;
	static time_t rawtime;
	static struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = gmtime( &rawtime );

	sec = (level.time - level.startTime)/1000;
	min = sec/60;
	sec %= 60;

	len = sprintf(string,"[%02i:%02i:%02i;%i;%i:%02i] ",
		timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec /*real time UTC*/, 
		level.time /*server time (ms)*/,
		min,sec /*game time*/);


	va_start( argptr, fmt );
	vsnprintf( string +len , sizeof(string) - len, fmt,argptr );
	va_end( argptr );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + len );
	}

	if ( !level.logFile ) {
		return;
	}

	trap_FS_Write( string, strlen( string ), level.logFile );
}

static char* getCurrentTime()
{
	time_t rawtime;
	struct tm *timeinfo;
	static char buffer[128];

	time ( &rawtime );
	timeinfo = gmtime ( &rawtime );

	strftime(buffer, sizeof(buffer), "UTC %a %b %d %H:%M:%S %Y", timeinfo);
	return buffer;
}

//CRASH LOG
void QDECL G_HackLog( const char *fmt, ... ) {
	va_list		argptr;
	static char		string[2048] = { 0 };
	int len;

	if ( !level.hackLogFile && !level.logFile) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsnprintf( string+len, sizeof(string) - len, fmt, argptr );
	va_end( argptr );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + len );
	}
		
	if (level.hackLogFile){
		trap_FS_Write( string, strlen( string ), level.hackLogFile );
	}

}

//DB-ACCOUNTS log
void QDECL G_DBLog( const char *fmt, ... ) {
	va_list		argptr;
	static char		string[2048] = { 0 };
	int len;

	if ( !level.DBLogFile ) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsnprintf( string+len, sizeof(string) - len, fmt, argptr );
	va_end( argptr );

	trap_FS_Write( string, strlen( string ), level.DBLogFile );
}

//RCON log
void QDECL G_RconLog( const char *fmt, ... ) {
	va_list		argptr;
	static char		string[2048] = { 0 };
	int len;

	if ( !level.rconLogFile) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsnprintf( string+len, sizeof(string) - len, fmt, argptr );
	va_end( argptr );

	trap_FS_Write( string, strlen( string ), level.rconLogFile );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string ) {
	int				i, numSorted;
	gclient_t		*cl;
	G_LogPrintf( "Exit: %s\n", string );

	level.didLogExit = qtrue;
	level.intermissionNeededTime = 0;
	level.intermissionQueued = level.time;


	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap_SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( g_gametype.integer >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		if ((cl->ps.powerups[PW_BLUEFLAG] || cl->ps.powerups[PW_REDFLAG])){
			const int thisFlaghold = G_GetAccurateTimerOnTrigger( &cl->pers.teamState.flagsince, &g_entities[level.sortedClients[i]], NULL );

			cl->pers.teamState.flaghold += thisFlaghold;

			if ( thisFlaghold > cl->pers.teamState.longestFlaghold )
				cl->pers.teamState.longestFlaghold = thisFlaghold;

			if ( cl->ps.powerups[PW_REDFLAG] ) {
				// carried the red flag, so blue team
				level.blueTeamRunFlaghold += thisFlaghold;
			} else if ( cl->ps.powerups[PW_BLUEFLAG] ) {
				// carried the blue flag, so red team
				level.redTeamRunFlaghold += thisFlaghold;
			}
		}

		if ( cl->ps.fd.forcePowersActive & ( 1 << FP_PROTECT ) ) {
			if ( cl->pers.protsince && cl->pers.protsince < level.time ) {
				cl->pers.protTimeUsed += level.time - cl->pers.protsince;
			}
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  client: %i %s\n", cl->ps.persistant[PERS_SCORE], ping, level.sortedClients[i],	cl->pers.netname );
	}

	// reset the lock on teams if the game ended normally
	trap_Cvar_Set( "g_maxgameclients", "0" );

}

qboolean gDidDuelStuff = qfalse; //gets reset on game reinit

/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void ) {
	int			ready, notReady;
	int			i;
	gclient_t	*cl;
	int			readyMask;

	// see which players are ready
	ready = 0;
	notReady = 0;
	readyMask = 0;
	for (i=0 ; i< g_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT ) {
			continue;
		}

		if ( cl->readyToExit ) {
			ready++;
			if ( i < 16 ) {
				readyMask |= 1 << i;
			}
		} else if (cl->sess.sessionTeam != TEAM_SPECTATOR && !(g_gametype.integer == GT_CTF && cl->sess.sessionTeam == TEAM_FREE)) {
			notReady++;
		}
	}

	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) && !gDidDuelStuff &&
		(level.time > level.intermissiontime + 2000) )
	{
		gDidDuelStuff = qtrue;

		if ( g_austrian.integer && g_gametype.integer != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Results:\n");
			G_LogPrintf("winner: %s, score: %d, wins/losses: %d/%d\n", 
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
			G_LogPrintf("loser: %s, score: %d, wins/losses: %d/%d\n", 
				level.clients[level.sortedClients[1]].pers.netname,
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[1]].sess.wins,
				level.clients[level.sortedClients[1]].sess.losses );
		}
		// if we are running a tournement map, kick the loser to spectator status,
		// which will automatically grab the next spectator and restart
		if (!DuelLimitHit())
		{
			if (g_gametype.integer == GT_POWERDUEL)
			{
				RemovePowerDuelLosers();
				AddPowerDuelPlayers();
			}
			else
			{
				if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
					level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
					level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
					level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
				{
					RemoveDuelDrawLoser();
				}
				else
				{
					RemoveTournamentLoser();
				}
				AddTournamentPlayer();
			}

			if ( g_austrian.integer )
			{
				if (g_gametype.integer == GT_POWERDUEL)
				{
					G_LogPrintf("Power Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n", 
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						level.clients[level.sortedClients[2]].pers.netname,
						level.clients[level.sortedClients[2]].sess.wins,
						level.clients[level.sortedClients[2]].sess.losses,
						g_fraglimit.integer );
				}
				else
				{
					G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d, kill limit: %d\n", 
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						g_fraglimit.integer );
				}
			}
			
			if (g_gametype.integer == GT_POWERDUEL)
			{
				if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
				{
					trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
					trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}			
			}
			else
			{
				if (level.numPlayingClients >= 2)
				{
					trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
					trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}
			}

			return;	
		}

		if ( g_austrian.integer && g_gametype.integer != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Tournament Winner: %s wins/losses: %d/%d\n", 
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
		}

		if (g_gametype.integer == GT_POWERDUEL)
		{
			RemovePowerDuelLosers();
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{
				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
		else
		{
			//this means we hit the duel limit so reset the wins/losses
			//but still push the loser to the back of the line, and retain the order for
			//the map change
			if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
				level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
				level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
			{
				RemoveDuelDrawLoser();
			}
			else
			{
				RemoveTournamentLoser();
			}

			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	if ((g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) && !gDuelExit)
	{ //in duel, we have different behaviour for between-round intermissions
		if ( level.time > level.intermissiontime + 4000 )
		{ //automatically go to next after 4 seconds
			ExitLevel();
			return;
		}

		for (i=0 ; i< g_maxclients.integer ; i++)
		{ //being in a "ready" state is not necessary here, so clear it for everyone
		  //yes, I also thinking holding this in a ps value uniquely for each player
		  //is bad and wrong, but it wasn't my idea.
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED )
			{
				continue;
			}
			cl->ps.stats[STAT_CLIENTS_READY] = 0;
		}
		return;
	}

	// copy the readyMask to each player's stats so
	// it can be displayed on the scoreboard
	for (i=0 ; i< g_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
	}

	// never exit in less than five seconds
	if ( level.time < level.intermissiontime + 5000 ) {
		return;
	}

    // always exit at most after fifteen seconds
    if ( level.time > level.intermissiontime + 15000 )
    {
        ExitLevel();
        return;
    }

	if (d_noIntermissionWait.integer)
	{ //don't care who wants to go, just go.
		ExitLevel();
		return;
	}

	// if nobody wants to go, clear timer
	if ( !ready ) {
		level.readyToExit = qfalse;
		return;
	}

	// if everyone wants to go, go now
	if ( !notReady ) {
		ExitLevel();
		return;
	}

	// the first person to ready starts the ten second timeout
	if ( !level.readyToExit ) {
		level.readyToExit = qtrue;
		level.exitTime = level.time;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if ( level.time < level.exitTime + 10000 ) {
		return;
	}

	ExitLevel();
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void ) {
	int		a, b;

	if (level.numPlayingClients < 2 ) {
		return qfalse;
	}
	
	if ( g_gametype.integer >= GT_TEAM ) {
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
	b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

	return a == b;
}

int findClient(const char* username){
	int i;
	gentity_t* ent;

	for(i=0;i<level.maxclients;++i){
		ent = &g_entities[i];

		if (!ent->inuse || !ent->client)
			continue;

		if (!Q_stricmpn(ent->client->sess.username,username,MAX_USERNAME_SIZE)){
			//usernames match
			return i;
		}
	}

	return -1; //not found
}

//match report, for db accounts system
//void reportMatch(){
//	int playersLeft;
//	int reported;
//	static char buffer[1024];
//	char mapname[128];
//	gclient_t*	client;
//	char* str;
//	char* dest;
//	int i;
//	int len;
//
//	//G_LogPrintf("reportMatch()\n");
//
//	if (!level.initialConditionsMatch)
//		return;
//
//	//conditions match, lets send roster to database
//	//format: 
//    // red1:red2:...:redm blue1:blue2:...:bluen redscore bluescore map wasOvertime
//	//example:
//	// sil:sal:ramses:norb jax:lando:fp:rst 11 8 mp/ctf4 0
//
//	//report players from original roster list, don't count newly joined
//	//red players roster report
//
//	dest = buffer;
//
//	//report REDs
//	strncpy(dest,"reds=",5);
//	dest += 5;
//
//	reported = 0;
//	playersLeft = 0;
//	for(i=0;i<level.initialRedCount;++i){
//		client = &g_clients[level.initialRedRoster[i].clientNum];
//
//		if (strncmp(client->sess.username,level.initialRedRoster[i].username,MAX_USERNAME_SIZE)){
//			//username doesnt match, try to find this player somewhere else,
//			//he might possibly reconnect with another number
//			int num = findClient(level.initialRedRoster[i].username);	
//
//			if (num == -1) //not found
//				continue; //TODO: include for check of how long time before end did player left
//
//			//ok user found
//			client = &g_clients[num];			
//		}
//
//		if ((client->sess.sessionTeam != TEAM_RED)
//			|| (client->pers.connected == CON_DISCONNECTED)){
//			//this guy left his team
//			++playersLeft;
//			continue; //TODO: include for check of how long time before end did player left
//		}
//
//		if (reported){
//			*dest = ':';
//			++dest;
//		}
//
//		len = strlen(client->sess.username);
//		strncpy(dest,client->sess.username,len);
//		dest += len;
//		++reported;
//	}
//
//	strncpy(dest,"&blues=",7);
//	dest += 7;
//
//	//report BLUEs
//	reported = 0;
//	//playersLeft = 0;
//	for(i=0;i<level.initialBlueCount;++i){
//		client = &g_clients[level.initialBlueRoster[i].clientNum];
//
//		if (strncmp(client->sess.username,level.initialBlueRoster[i].username,MAX_USERNAME_SIZE)){
//			//username doesnt match, try to find this player somewhere else,
//			//he might possibly reconnect with another number
//			int num = findClient(level.initialBlueRoster[i].username);	
//
//			if (num == -1) //not found in present players
//				continue; //TODO: include for check of how long time before end did player left
//
//			//ok user found
//			client = &g_clients[num];	
//		}
//
//		if ((client->sess.sessionTeam != TEAM_BLUE)
//			|| (client->pers.connected == CON_DISCONNECTED)){
//			//this guy left his team
//			++playersLeft;
//			continue; //TODO: include for check of how long time before end did player left
//		}
//
//		if (reported){
//			*dest = ':';
//			++dest;
//		}
//
//		len = strlen(client->sess.username);
//		strncpy(dest,client->sess.username,len);
//		dest += len;
//		++reported;
//	}
//
//	//more than 2 players left original teams, dont report...
//	if (playersLeft > 2)
//		return;
//
//	//report scores
//	strncpy(dest,"&redscore=",10);
//	dest += 10;
//
//	str = va("%i",level.teamScores[TEAM_RED]); //reds
//	len = strlen(str);
//	strncpy(dest,str,len); 
//	dest += len;
//
//	strncpy(dest,"&bluescore=",11);
//	dest += 11;
//
//	str = va("%i",level.teamScores[TEAM_BLUE]); //blues
//	len = strlen(str);
//	strncpy(dest,str,len); 
//	dest += len;
//
//	strncpy(dest,"&map=",5);
//	dest += 5;
//
//	//report map name
//	trap_Cvar_VariableStringBuffer("mapname",mapname,sizeof(mapname));
//	len = strlen(mapname);
//	strncpy(dest,mapname,len);
//	dest += len;
//
//	strncpy(dest,"&ot=",4);
//	dest += 4;
//
//	//report whether this game went overtime
//	if (level.overtime){
//		*dest = '1';
//	} else {
//		*dest = '0';
//	}
//	++dest;
//
//	*dest = '\0';
//
//	sendMatchResult(buffer);
//
//}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
qboolean g_endPDuel = qfalse;
void CheckExitRules( void ) {
 	int			i;
	gclient_t	*cl;
	char *sKillLimit;
	qboolean printLimit = qtrue;
	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if ( level.intermissiontime ) {
		CheckIntermissionExit ();
		return;
	}

	if (gDoSlowMoDuel)
	{ //don't go to intermission while in slow motion
		return;
	}

	if (gEscaping)
	{
		int i = 0;
		int numLiveClients = 0;

		while (i < MAX_CLIENTS)
		{
			if (g_entities[i].inuse && g_entities[i].client && g_entities[i].health > 0)
			{
				if (g_entities[i].client->sess.sessionTeam != TEAM_SPECTATOR &&
					!(g_entities[i].client->ps.pm_flags & PMF_FOLLOW))
				{
					numLiveClients++;
				}
			}

			i++;
		}
		if (gEscapeTime < level.time)
		{
			gEscaping = qfalse;
			LogExit( "Escape time ended." );
			return;
		}
		if (!numLiveClients)
		{
			gEscaping = qfalse;
			LogExit( "Everyone failed to escape." );
			return;
		}
	}

	if ( level.intermissionQueued ) {
		if ( level.time - level.intermissionQueued >= INTERMISSION_DELAY_TIME ) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}
				return;
			}

	// check for sudden death
	if (g_gametype.integer != GT_SIEGE)
	{
		
		if ( ScoreIsTied() ) {

			//log we came overtime
			if ( level.time - level.startTime >= g_timelimit.integer*60000 ){
				level.overtime = qtrue;
			}

			// always wait for sudden death
			if ((g_gametype.integer != GT_DUEL) || !g_timelimit.integer)
			{
				if (g_gametype.integer != GT_POWERDUEL)
				{
					return;
				}
			}
		}
	}

	if (g_gametype.integer != GT_SIEGE)
	{
		if ( g_timelimit.integer && !level.warmupTime ) {
			if ( level.time - level.startTime >= g_timelimit.integer*60000 ) {
				trap_SendServerCommand( -1, va("print \"%s.\n\"",G_GetStringEdString("MP_SVGAME", "TIMELIMIT_HIT")));
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Timelimit hit (1)\n");
				}
				LogExit( "Timelimit hit." );

				if (g_gametype.integer == GT_CTF && g_accounts.integer){
					// match log - accounts system
					//reportMatch();
				}

				return;
			}
		}
	}

	if (g_gametype.integer == GT_POWERDUEL && level.numPlayingClients >= 3)
	{
		if (g_endPDuel)
		{
			g_endPDuel = qfalse;
			LogExit("Powerduel ended.");
		}

		return;
	}

	if ( level.numPlayingClients < 2 ) {
		return;
	}

	if (g_gametype.integer == GT_DUEL ||
		g_gametype.integer == GT_POWERDUEL)
	{
		if (g_fraglimit.integer > 1)
		{
			sKillLimit = "Kill limit hit.";
		}
		else
		{
			sKillLimit = "";
			printLimit = qfalse;
		}
	}
	else
	{
		sKillLimit = "Kill limit hit.";
	}
	if ( g_gametype.integer < GT_SIEGE && g_fraglimit.integer ) {
		if ( level.teamScores[TEAM_RED] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, va("print \"Red %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (1)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, va("print \"Blue %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (2)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		for ( i=0 ; i< g_maxclients.integer ; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam != TEAM_FREE ) {
				continue;
			}

			if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) && g_duel_fraglimit.integer && cl->sess.wins >= g_duel_fraglimit.integer )
			{
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Duel limit hit (1)\n");
				}
				LogExit( "Duel limit hit." );
				gDuelExit = qtrue;
				trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " hit the win limit.\n\"",
					cl->pers.netname ) );
				return;
			}

			if ( cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer ) {
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Kill limit (3)\n");
				}
				LogExit( sKillLimit );
				gDuelExit = qfalse;
				if (printLimit)
				{
					trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s.\n\"",
													cl->pers.netname,
													G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")
													) 
											);
				}
				return;
			}
		}
	}

    if (g_gametype.integer >= GT_CTF && g_capturedifflimit.integer) {

        if (level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE] >= g_capturedifflimit.integer)
        {
            trap_SendServerCommand(-1, va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")));
            trap_SendServerCommand(-1, va("print \"%s ^7- Team ^1RED ^7leads by ^2%i^7.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT"), g_capturedifflimit.integer));
            LogExit("Capture difference limit hit.");
				return;
			}

        if (level.teamScores[TEAM_BLUE] - level.teamScores[TEAM_RED] >= g_capturedifflimit.integer) {
            trap_SendServerCommand(-1, va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")));
            trap_SendServerCommand(-1, va("print \"%s ^7- Team ^4BLUE ^7leads by ^2%i^7.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT"), g_capturedifflimit.integer));
            LogExit("Capture difference limit hit.");
            return;
		}
	}

	if ( g_gametype.integer >= GT_CTF && g_capturelimit.integer ) {

		if ( level.teamScores[TEAM_RED] >= g_capturelimit.integer ) 
		{
			trap_SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")));
			trap_SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_capturelimit.integer ) {
			trap_SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")));
			trap_SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}
	}

}



/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

void G_RemoveDuelist(int team)
{
	int i = 0;
	gentity_t *ent;
	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client && ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
			ent->client->sess.duelTeam == team)
		{
			SetTeam(ent, "s",qfalse);
		}
        i++;
	}
}

/*
=============
CheckTournament

Once a frame, check for changes in tournement player state
=============
*/
int g_duelPrintTimer = 0;
void CheckTournament( void ) {
	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart

	if (g_gametype.integer == GT_POWERDUEL)
	{
		if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
		{
			trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
		}
	}
	else
	{
		if (level.numPlayingClients >= 2)
		{
			trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
		}
	}

	if ( g_gametype.integer == GT_DUEL )
	{
		// pull in a spectator if needed
		if ( level.numPlayingClients < 2 && !level.intermissiontime && !level.intermissionQueued ) {
			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
			}
		}

		if (level.numPlayingClients >= 2)
		{
// nmckenzie: DUEL_HEALTH
			if ( g_showDuelHealths.integer >= 1 )
			{
				playerState_t *ps1, *ps2;
				ps1 = &level.clients[level.sortedClients[0]].ps;
				ps2 = &level.clients[level.sortedClients[1]].ps;
				trap_SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("%i|%i|!", 
					ps1->stats[STAT_HEALTH], ps2->stats[STAT_HEALTH]));
			}
		}

		//rww - It seems we have decided there will be no warmup in duel.
		//if (!g_warmup.integer)
		{ //don't care about any of this stuff then, just add people and leave me alone
			level.warmupTime = 0;
			return;
		}
#if 0
		// if we don't have two players, go back to "waiting for players"
		if ( level.numPlayingClients != 2 ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return;
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			if ( level.numPlayingClients == 2 ) {
				// fudge by -1 to account for extra delays
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;

				if (level.warmupTime < (level.time + 3000))
				{ //rww - this is an unpleasent hack to keep the level from resetting completely on the client (this happens when two map_restarts are issued rapidly)
					level.warmupTime = level.time + 3000;
				}
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			}
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap_Cvar_Set( "g_restarted", "1" );
			trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
#endif
	}
	else if (g_gametype.integer == GT_POWERDUEL)
	{
		if (level.numPlayingClients < 2)
		{ //hmm, ok, pull more in.
			g_dontFrickinCheck = qfalse;
		}

		if (level.numPlayingClients > 3)
		{ //umm..yes..lets take care of that then.
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone > 1)
			{
				G_RemoveDuelist(DUELTEAM_LONE);
			}
			else if (dbl > 2)
			{
				G_RemoveDuelist(DUELTEAM_DOUBLE);
			}
		}
		else if (level.numPlayingClients < 3)
		{ //hmm, someone disconnected or something and we need em
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
			else if (dbl < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
		}

		// pull in a spectator if needed
		if (level.numPlayingClients < 3 && !g_dontFrickinCheck)
		{
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 &&
				G_CanResetDuelists())
			{
				gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
				te->r.svFlags |= SVF_BROADCAST;
				//this is really pretty nasty, but..
				te->s.otherEntityNum = level.sortedClients[0];
				te->s.otherEntityNum2 = level.sortedClients[1];
				te->s.groundEntityNum = level.sortedClients[2];

				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				G_ResetDuelists();

				g_dontFrickinCheck = qtrue;
			}
			else if (level.numPlayingClients > 0 ||
				level.numConnectedClients > 0)
			{
				if (g_duelPrintTimer < level.time)
				{ //print once every 10 seconds
					int lone = 0, dbl = 0;

					G_PowerDuelCount(&lone, &dbl, qtrue);
					if (lone < 1)
					{
						trap_SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMORESINGLE")) );
					}
					else
					{
						trap_SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMOREPAIRED")) );
					}
					g_duelPrintTimer = level.time + 10000;
				}
			}

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{ //pulled in a needed person
				if (G_CanResetDuelists())
				{
					gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
					te->r.svFlags |= SVF_BROADCAST;
					//this is really pretty nasty, but..
					te->s.otherEntityNum = level.sortedClients[0];
					te->s.otherEntityNum2 = level.sortedClients[1];
					te->s.groundEntityNum = level.sortedClients[2];

					trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );

					if ( g_austrian.integer )
					{
						G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n", 
							level.clients[level.sortedClients[0]].pers.netname,
							level.clients[level.sortedClients[0]].sess.wins,
							level.clients[level.sortedClients[0]].sess.losses,
							level.clients[level.sortedClients[1]].pers.netname,
							level.clients[level.sortedClients[1]].sess.wins,
							level.clients[level.sortedClients[1]].sess.losses,
							level.clients[level.sortedClients[2]].pers.netname,
							level.clients[level.sortedClients[2]].sess.wins,
							level.clients[level.sortedClients[2]].sess.losses,
							g_fraglimit.integer );
					}
					//FIXME: This seems to cause problems. But we'd like to reset things whenever a new opponent is set.
				}
			}
		}
		else
		{ //if you have proper num of players then don't try to add again
			g_dontFrickinCheck = qtrue;
		}

		level.warmupTime = 0;
		return;
	}

}

void G_KickAllBots(void)
{
	int i;
	char netname[36];
	gclient_t	*cl;

	for ( i=0 ; i< g_maxclients.integer ; i++ )
	{
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED )
		{
			continue;
		}
		if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) )
		{
			continue;
		}
		strcpy(netname, cl->pers.netname);
		Q_CleanStr(netname);
		trap_SendConsoleCommand( EXEC_INSERT, va("kick \"%s\"\n", netname) );
	}
}

/*
==================
CheckVote
==================
*/
extern void SiegeClearSwitchData(void);
extern int* BuildVoteResults( const int numChoices, int *numVotes, int *highestVoteCount );

qboolean IsVoteForCustomClasses(char *string)
{
	if (!g_requireMoreCustomTeamVotes.integer) //only do all this crap if this integer is non-zero
	{
		return qfalse;
	}
	if (!Q_stricmpn(string, "g_red", 5)) //custom red team vote
	{
		if (string[10] == 'n' && string[11] == 'o' && string[12] == 'n' && string[13] == 'e') //"none" argument
		{
			return qfalse;
		}
		else if (string[10] == '0') //"0" argument
		{
			return qfalse;
		}
		else
		{
			return qtrue;
		}
	}
	else if (!Q_stricmpn(string, "g_blue", 6)) //custom blue team vote
	{
		if (string[11] == 'n' && string[12] == 'o' && string[13] == 'n' && string[14] == 'e') //"none" argument
		{
			return qfalse;
		}
		else if (string[11] == '0') //"0" argument
		{
			return qfalse;
		}
		else
		{
			return qtrue;
		}
	}
	else
	{
		return qfalse;
	}
}

int FindRequiredCustomTeamYesVoters(int numVotingClients)
{
	int i;
	if (numVotingClients <= 3)
	{
		return numVotingClients;
	}
	for (i = 1; i <= numVotingClients; i++)
	{
		if ((double)i / numVotingClients >= 0.75) //vote will only pass if we get >=75% yes votes
		{
			return i;
		}
	}
	return numVotingClients; //this should never happen
}

int FindRequiredCustomTeamNoVoters(int numVotingClients)
{
	int i;
	if (numVotingClients <= 3)
	{
		return 1;
	}
	for (i = 1; i <= numVotingClients; i++)
	{
		if ((double)i / numVotingClients >= 0.75)
		{
			return (numVotingClients - i + 1); //vote fails if we get >25% no votes
		}
	}
	return 1; //this should never happen
}

void CheckVote( void ) {
	if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
		level.voteExecuteTime = 0;

		/*
		if (!Q_stricmpn(level.voteString,"map_restart",11)){
			trap_Cvar_Set("g_wasRestarted", "1");
		}
		*/

        if (!Q_stricmpn(level.voteString, "clientkick", 10)){
            int id = atoi(&level.voteString[11]);

            if ((id < sv_privateclients.integer) && !(g_entities[id].r.svFlags & SVF_BOT))
            {
                if (g_entities[id].client->sess.sessionTeam != TEAM_SPECTATOR)
                {       
                    trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i s\n", id));
                }

                trap_SendServerCommand(-1, va("print \"%s^1 may not be kicked.^7\n\"", g_entities[id].client->pers.netname));
				trap_SendServerCommand(id, "cp \"^1There's a reason people voted to kick you.\nPlease reconsider your behavior.\nPeople aren't happy with it.^7\n\"");
				trap_SendServerCommand(id, "print \"^1There's a reason people voted to kick you.\nPlease reconsider your behavior.\nPeople aren't not happy with it.^7\n\"");
                return;
            }
        }

		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );

		if (level.votingGametype)
		{
			if (g_fraglimitVoteCorrection.integer)
			{ //This means to auto-correct fraglimit when voting to and from duel.
				const int currentGT = trap_Cvar_VariableIntegerValue("g_gametype");
				const int currentFL = trap_Cvar_VariableIntegerValue("fraglimit");
				const int currentTL = trap_Cvar_VariableIntegerValue("timelimit");

				if ((level.votingGametypeTo == GT_DUEL || level.votingGametypeTo == GT_POWERDUEL) && currentGT != GT_DUEL && currentGT != GT_POWERDUEL)
				{
					if (currentFL > 3 || !currentFL)
					{ //if voting to duel, and fraglimit is more than 3 (or unlimited), then set it down to 3
						trap_SendConsoleCommand(EXEC_APPEND, "fraglimit 3\n");
					}
					if (currentTL)
					{ //if voting to duel, and timelimit is set, make it unlimited
						trap_SendConsoleCommand(EXEC_APPEND, "timelimit 0\n");
					}
				}
				else if ((level.votingGametypeTo != GT_DUEL && level.votingGametypeTo != GT_POWERDUEL) &&
					(currentGT == GT_DUEL || currentGT == GT_POWERDUEL))
				{
					if (currentFL && currentFL < 20)
					{ //if voting from duel, an fraglimit is less than 20, then set it up to 20
						trap_SendConsoleCommand(EXEC_APPEND, "fraglimit 20\n");
					}
				}
			}

			level.votingGametype = qfalse;
			level.votingGametypeTo = 0;
		}
	}
	if ( !level.voteTime ) {
		return;
	}
	if ( !level.multiVoting ) {
		// normal behavior for basejka voting
		if ( level.time - level.voteTime >= VOTE_TIME ) {
			if ( ( g_minimumVotesCount.integer ) && ( level.numVotingClients % 2 == 0 ) && ( level.voteYes > level.voteNo ) && ( level.voteYes + level.voteNo >= g_minimumVotesCount.integer ) ) {
				trap_SendServerCommand( -1, va( "print \"%s\n\"",
					G_GetStringEdString( "MP_SVGAME", "VOTEPASSED" ) ) );
		// log the vote
            G_LogPrintf("Vote passed. (Yes:%i No:%i All:%i g_minimumVotesCount:%i)\n", level.voteYes, level.voteNo, level.numVotingClients, g_minimumVotesCount.integer);
			//special fix for siege status
			if (!Q_strncmp(level.voteString, "vstr nextmap", sizeof(level.voteString))) {
				SiegeClearSwitchData(); //clear siege to round 1 on nextmap vote
				LivePugRuined("Vote", qfalse);
			}
			if (!Q_stricmpn(level.voteString, "map", 3) && !(!Q_stricmpn(level.voteString, "map_", 4))) {
				SiegeClearSwitchData(); //clear siege to round 1 on map change vote
				if (g_autoResetCustomTeams.integer) //reset custom teams
				{
					trap_Cvar_Set("g_redTeam", "none");
					trap_Cvar_Set("g_blueTeam", "none");
				}
				LivePugRuined("Vote", qfalse);
			}
			if (!Q_stricmpn(level.voteString, "map_restart", 11)) {
				LivePugRuined("Vote", qfalse);
			}
			if (!Q_stricmp(level.voteString, "g_siegeRespawn 1")) {
				LivePugRuined("Vote", qfalse);
			}
			if (!Q_stricmpn(level.voteString, "g_gametype", 10))
			{
				LivePugRuined("Vote", qfalse);
				trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
				if (trap_Cvar_VariableIntegerValue("g_gametype") != level.votingGametypeTo)
				{ //If we're voting to a different game type, be sure to refresh all the map stuff
					const char *nextMap = G_GetDefaultMap(level.votingGametypeTo);

					if (level.votingGametypeTo == GT_SIEGE)
					{ //ok, kick all the bots, cause the aren't supported!
						G_KickAllBots();
						//just in case, set this to 0 too... I guess...maybe?
						//trap_Cvar_Set("bot_minplayers", "0");
					}

					if (nextMap && nextMap[0])
					{
						trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", nextMap));
					}
				}
				else
				{ //otherwise, just leave the map until a restart
					G_RefreshNextMap(level.votingGametypeTo, qfalse);
				}
			}

			// set the delay
			if (!Q_stricmpn(level.voteString, "pause", 5))
				level.voteExecuteTime = level.time; // allow pause votes to take affect immediately
			else
				level.voteExecuteTime = level.time + 3000;

        } else {
            trap_SendServerCommand(-1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEFAILED")));
			if (!Q_stricmpn(level.voteString, "svsay Poll", 10))
			{
				trap_SendConsoleCommand(EXEC_APPEND, va("svsay Poll Result ^1NO^7: %s\n", level.voteDisplayString + 6));
			}
		// log the vote
		G_LogPrintf("Vote timed out. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients);
        }
		}
		else {
			if ((g_enforceEvenVotersCount.integer) && (level.numVotingClients % 2 == 1)) {
				if ((g_minVotersForEvenVotersCount.integer > 4) && (level.numVotingClients >= g_minVotersForEvenVotersCount.integer)) {
					if (level.voteYes < level.numVotingClients / 2 + 2) {
						return;
					}
				}
			}

			if (!IsVoteForCustomClasses(level.voteString) && level.voteYes > level.numVotingClients / 2)
			{
				trap_SendServerCommand(-1, va("print \"%s\n\"",
					G_GetStringEdString("MP_SVGAME", "VOTEPASSED")));

				// log the vote
				G_LogPrintf("Vote passed. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients);
				if (!Q_strncmp(level.voteString, "vstr nextmap", sizeof(level.voteString))) {
					SiegeClearSwitchData(); //clear siege to round 1 on nextmap vote
					LivePugRuined("Vote", qfalse);
				}
				if (!Q_stricmpn(level.voteString, "map", 3) && !(!Q_stricmpn(level.voteString, "map_", 4))) {
					SiegeClearSwitchData(); //clear siege to round 1 on map change vote
					if (g_autoResetCustomTeams.integer) //reset custom teams
					{
						trap_Cvar_Set("g_redTeam", "none");
						trap_Cvar_Set("g_blueTeam", "none");
					}
					LivePugRuined("Vote", qfalse);
				}
				if (!Q_stricmpn(level.voteString, "map_restart", 11)) {
					LivePugRuined("Vote", qfalse);
				}
				if (!Q_stricmp(level.voteString, "g_siegeRespawn 1")) {
					LivePugRuined("Vote", qfalse);
				}
				if (!Q_stricmpn(level.voteString, "g_gametype", 10))
				{
					trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
					if (trap_Cvar_VariableIntegerValue("g_gametype") != level.votingGametypeTo)
					{ //If we're voting to a different game type, be sure to refresh all the map stuff
						const char *nextMap = G_GetDefaultMap(level.votingGametypeTo);

						if (level.votingGametypeTo == GT_SIEGE)
						{ //ok, kick all the bots, cause the aren't supported!
							G_KickAllBots();
							//just in case, set this to 0 too... I guess...maybe?
							//trap_Cvar_Set("bot_minplayers", "0");
						}

						if (nextMap && nextMap[0])
						{
							trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", nextMap));
						}
					}
					else
					{ //otherwise, just leave the map until a restart
						G_RefreshNextMap(level.votingGametypeTo, qfalse);
					}
				}

				// set the delay
				if (!Q_stricmpn(level.voteString, "pause", 5))
					level.voteExecuteTime = level.time; // allow pause votes to take affect immediately
				else
					level.voteExecuteTime = level.time + 3000;
			}
			else if (IsVoteForCustomClasses(level.voteString) && level.voteYes >= FindRequiredCustomTeamYesVoters(level.numVotingClients))
			{
				trap_SendServerCommand(-1, va("print \"%s\n\"",
					G_GetStringEdString("MP_SVGAME", "VOTEPASSED")));

				// log the vote
				G_LogPrintf("Vote passed. (Yes:%i No:%i All:%i) - 75 percent yes votes required\n", level.voteYes, level.voteNo, level.numVotingClients);
				level.voteExecuteTime = level.time + 3000;
			}
			else if (!IsVoteForCustomClasses(level.voteString) && level.voteNo >= (level.numVotingClients + 1) / 2)
			{
				// same behavior as a timeout
				trap_SendServerCommand(-1, va("print \"%s\n\"",
					G_GetStringEdString("MP_SVGAME", "VOTEFAILED")));
				if (!Q_stricmpn(level.voteString, "svsay Poll", 10))
				{
					trap_SendConsoleCommand(EXEC_APPEND, va("svsay Poll Result ^1NO^7: %s\n", level.voteDisplayString + 6));
				}

				// log the vote
				G_LogPrintf("Vote failed. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients);
			}
			else if (IsVoteForCustomClasses(level.voteString) && level.voteNo >= FindRequiredCustomTeamNoVoters(level.numVotingClients))
			{
				// same behavior as a timeout
				trap_SendServerCommand(-1, va("print \"%s\n\"",
					G_GetStringEdString("MP_SVGAME", "VOTEFAILED")));
				if (!Q_stricmpn(level.voteString, "svsay Poll", 10))
				{
					trap_SendConsoleCommand(EXEC_APPEND, va("svsay Poll Result ^1NO^7: %s\n", level.voteDisplayString + 6));
				}

				// log the vote
				G_LogPrintf("Vote failed. (Yes:%i No:%i All:%i) - 75 percent yes votes required\n", level.voteYes, level.voteNo, level.numVotingClients);
			}
			else
			{
				// still waiting for a majority
				return;
			}
		}

		g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	} else {
		// special handler for multiple choices voting
		int numVotes, highestVoteCount;
		int *voteResults = BuildVoteResults( level.multiVoteChoices, &numVotes, &highestVoteCount );
		free( voteResults );

		// the vote ends when a map has >50% majority, when everyone voted, or when the vote timed out
		if (level.time - level.voteTime >= VOTE_TIME && !DoRunoff()) {
			G_LogPrintf("Multi vote ended due to time (%d voters)\n", numVotes);
			level.voteExecuteTime = level.time; // in this special case, execute it now. the delay is done in the svcmd
			level.inRunoff = qfalse;
		}
		else if ( highestVoteCount >= ( ( level.numVotingClients / 2 ) + 1 )) {
			G_LogPrintf( "Multi vote ended due to majority vote (%d voters)\n", numVotes );
			level.voteExecuteTime = level.time; // in this special case, execute it now. the delay is done in the svcmd
			level.inRunoff = qfalse;
		}
		else if (numVotes >= level.numVotingClients && !DoRunoff()) {
			G_LogPrintf("Multi vote ended due to everyone voted, no majority, and no runoff (%d voters)\n", numVotes);
			level.voteExecuteTime = level.time; // in this special case, execute it now. the delay is done in the svcmd
			level.inRunoff = qfalse;
		}
		else {
			return;
		}

	}

	level.voteTime = 0;
	trap_SetConfigstring( CS_VOTE_TIME, "" );

}

void CheckReady(void) 
{
    int i = 0, readyCount = 0;
    int botsCount = 0;
    gentity_t *ent = NULL;
    unsigned readyMask = 0;
    static qboolean restarting = qfalse;

    if ((g_gametype.integer == GT_POWERDUEL) || (g_gametype.integer == GT_DUEL))
        return;

    // for original functionality of /ready
    // if (!g_doWarmup.integer || !level.numPlayingClients || restarting || level.intermissiontime)
    if (restarting || level.intermissiontime)
        return;

    for (i = 0, ent = g_entities; i < level.maxclients; i++, ent++)
    {
        // for original functionality of /ready
        // if (!ent->inuse || ent->client->pers.connected == CON_DISCONNECTED || ent->client->sess.sessionTeam == TEAM_SPECTATOR)
        if (!ent->inuse || ent->client->pers.connected == CON_DISCONNECTED)
            continue;

        if (ent->client->pers.ready)
        {
            readyCount++;
            if (i < 16)
                readyMask |= (1 << i);
        }

        if (ent->r.svFlags & SVF_BOT)
            ++botsCount;
    }

    // update ready flags for clients' scoreboards
    for (i = 0, ent = g_entities; i < level.maxclients; i++, ent++)
    {
        if (!ent->inuse || ent->client->pers.connected == CON_DISCONNECTED)
            continue;


        ent->client->ps.stats[STAT_CLIENTS_READY] = readyMask;
    }

    // allow this for original functionality of /ready
    // check if all conditions to start the match have been met
    // {
    //    int		counts[TEAM_NUM_TEAMS];
    //    qboolean	conditionsMet = qtrue;
    //
    //    counts[TEAM_BLUE] = TeamCount(-1, TEAM_BLUE);
    //    counts[TEAM_RED] = TeamCount(-1, TEAM_RED);
    //
    //    // eat least 1 player in each team
    //    if (counts[TEAM_RED] < 1 || counts[TEAM_BLUE] < 1 || counts[TEAM_RED] != counts[TEAM_BLUE])
    //    {
    //        conditionsMet = qfalse;
    //    }
    //
    //    // all players are ready
    //    if (readyCount < counts[TEAM_BLUE] + counts[TEAM_RED] - botsCount)
    //    {
    //        conditionsMet = qfalse;
    //    }
    //
    //    if (conditionsMet)
    //    {
    //        trap_Cvar_Set("g_restarted", "1");
    //        trap_Cvar_Set("g_wasRestarted", "1");
    //        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 5\n");
    //        restarting = qtrue;
    //        return;
    //    }
    //    else
    //    {
    //
    //    }
    // }
}

/*
==================
PrintTeam
==================
*/
void PrintTeam(int team, char *message) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		trap_SendServerCommand( i, message );
	}
}

/*
==================
SetLeader
==================
*/
void SetLeader(int team, int client) {
	int i;

	if ( level.clients[client].pers.connected == CON_DISCONNECTED ) {
		PrintTeam(team, va("print \"%s is not connected\n\"", level.clients[client].pers.netname) );
		return;
	}
	if (level.clients[client].sess.sessionTeam != team) {
		PrintTeam(team, va("print \"%s is not on the team anymore\n\"", level.clients[client].pers.netname) );
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader) {
			level.clients[i].sess.teamLeader = qfalse;
			ClientUserinfoChanged(i);
		}
	}
	level.clients[client].sess.teamLeader = qtrue;
	ClientUserinfoChanged( client );
	PrintTeam(team, va("print \"%s %s\n\"", level.clients[client].pers.netname, G_GetStringEdString("MP_SVGAME", "NEWTEAMLEADER")) );
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( int team ) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader)
			break;
	}
	if (i >= level.maxclients) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			if (!(g_entities[i].r.svFlags & SVF_BOT)) {
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			level.clients[i].sess.teamLeader = qtrue;
			break;
		}
	}
}

/*
==================
CheckTeamVote
==================
*/
void CheckTeamVote( int team ) {
	int cs_offset;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		return;
	}
	if ( level.time - level.teamVoteTime[cs_offset] >= VOTE_TIME )
	{
		trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED")) );
	}
	else
	{
		if ( level.teamVoteYes[cs_offset] >= level.numRequiredTeamVotes[cs_offset] ) {
			// execute the command, then remove the vote
			trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEPASSED")) );
			//
			if ( !Q_stricmpn( "forceclass", level.teamVoteCommand[cs_offset], 10) || !Q_stricmpn("unforceclass", level.teamVoteCommand[cs_offset], 12))
			{
				trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.teamVoteCommand[cs_offset]));
			}
		} else if ( level.teamVoteNo[cs_offset] >= level.numRequiredTeamVotesNo[cs_offset] ) {
			// same behavior as a timeout
			trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED")) );
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.teamVoteTime[cs_offset] = 0;
	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );

}


/*
==================
CheckCvars
==================
*/
void CheckCvars( void ) {
	static int lastMod = -1;
	
	if ( g_password.modificationCount != lastMod ) {
		char password[MAX_INFO_STRING];
		char *c = password;
		lastMod = g_password.modificationCount;
		
		strcpy( password, g_password.string );
		while( *c )
		{
			if ( *c == '%' )
			{
				*c = '.';
			}
			c++;
		}
		trap_Cvar_Set("g_password", password );

		if ( (*g_password.string && Q_stricmp( g_password.string, "none" ))
			/*|| isDBLoaded */ // accounts system
			) 
		{
			trap_Cvar_Set( "g_needpass", !sv_passwordlessSpectators.integer ? "1" : "0" );
		} else {
			trap_Cvar_Set( "g_needpass", "0" );
			// Disabled since no password is set
			// TODO: auto update of g_needpass
			trap_Cvar_Set( "sv_passwordlessSpectators", "0" );
		}
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
extern void proxMineThink(gentity_t *ent);
extern void SiegeItemThink( gentity_t *ent );
extern void pas_think(gentity_t *ent);
extern void thermalThinkStandard(gentity_t *ent);
extern void thermalThinkPrimaryAntiSpam(gentity_t *ent);
extern void ShieldThink(gentity_t *ent);
extern void ShieldGoSolid(gentity_t *ent);
extern void turretG2_base_think(gentity_t *self);
extern void turret_base_think(gentity_t *self);

void G_RunThink(gentity_t *ent) {
	int	thinktime;

	//OSP: pause
	//      If paused, push nextthink
	if (level.pause.state != PAUSE_NONE) {
		int dt = level.time - level.previousTime;
		if (ent - g_entities >= g_maxclients.integer && ent->nextthink > level.time)
			ent->nextthink += dt;

		// special case, mines need update here
		if (ent->think == proxMineThink && ent->genericValue15 > level.time)
			ent->genericValue15 += dt;

		// another special case, siege items need respawn timer update
		if (ent->think == SiegeItemThink && ent->genericValue9 > level.time)
			ent->genericValue9 += dt;

		// more special cases, sentry
		if (ent->think == pas_think)
			ent->genericValue8 += dt;

		// another special case, thermal primaries
		if (ent->think == thermalThinkStandard || ent->think == thermalThinkPrimaryAntiSpam)
			ent->genericValue5 += dt;
		
		// siege item carry time stat
		if (ent->think == SiegeItemThink && ent->siegeItemCarrierTime)
			ent->siegeItemCarrierTime += dt;

		// siege shield uptime stat, siege item spawn floating timer, detkill timer
		if (ent->siegeItemSpawnTime)
			ent->siegeItemSpawnTime += dt;

		// turrets
		if (ent->think == turretG2_base_think || ent->think == turret_base_think) {
			ent->bounceCount += dt;
			ent->last_move_time += dt;
			ent->attackDebounceTime += dt;
			ent->painDebounceTime += dt;
			ent->setTime += dt;
			ent->fly_sound_debounce_time += dt;
			ent->aimDebounceTime += dt;
			ent->s.apos.trType = TR_STATIONARY;
		}
	}

	thinktime = ent->nextthink;
	if (thinktime <= 0) {
		goto runicarus;
	}
	if (thinktime > level.time) {
		goto runicarus;
	}
	

	ent->nextthink = 0;
	if (!ent->think) {
		goto runicarus;
	}
	ent->think (ent);

runicarus:
	if ( ent->inuse )
	{
		trap_ICARUS_MaintainTaskManager(ent->s.number);
	}
}

int g_LastFrameTime = 0;
int g_TimeSinceLastFrame = 0;

qboolean gDoSlowMoDuel = qfalse;
int gSlowMoDuelTime = 0;

//#define _G_FRAME_PERFANAL

void NAV_CheckCalcPaths( void )
{	
	if ( navCalcPathTime && navCalcPathTime < level.time )
	{//first time we've ever loaded this map...
		vmCvar_t	mapname;
		vmCvar_t	ckSum;

		trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		trap_Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

		//clear all the failed edges
		trap_Nav_ClearAllFailedEdges();

		//Calculate all paths
		NAV_CalculatePaths( mapname.string, ckSum.integer );
		
		trap_Nav_CalculatePaths(qfalse);

#if 0//#ifndef FINAL_BUILD
		if ( fatalErrors )
		{
			Com_Printf( S_COLOR_RED"Not saving .nav file due to fatal nav errors\n" );
		}
		else 
#endif
#ifndef _XBOX
		if ( trap_Nav_Save( mapname.string, ckSum.integer ) == qfalse )
		{
			Com_Printf("Unable to save navigations data for map \"%s\" (checksum:%d)\n", mapname.string, ckSum.integer );
		}
#endif
		navCalcPathTime = 0;
	}
}

//so shared code can get the local time depending on the side it's executed on
#include "namespace_begin.h"
int BG_GetTime(void)
{
	return level.time;
}
#include "namespace_end.h"


#ifdef NEWMOD_SUPPORT
#include "cJSON.h"

/*
reads the siege help messages from maps/map_name_goes_here.siegehelp
example file contents:


{
   "messages":[
	  {
		 "start":[""],
		 "end":["obj2"],
		 "size":"large",
		 "origin":[4360, -1220, 92],
		 "redMsg":"Complete obj 1",
		 "blueMsg":"Complete obj 2"
	  },
	  {
		 "start":["obj1completed"],
		 "end":["teleporttimedout", "obj2completed"],
		 "size":"small",
		 "origin":[3257, 2746, 424],
		 "constraints":[[null, 3900], [null, null], [null, null]],
		 "blueMsg":"Teleport"
	  },
	  {
		 "start":["obj1completed"],
		 "end":["obj2completed"],
		 "item":"codes_targetname_or_goaltarget",
		 "size":"large",
		 "origin":[6070, 1910, 179],
		 "redMsg":"Get codes",
		 "blueMsg":"Defend codes spawn"
	  },
   ]
}


*/
void InitializeSiegeHelpMessages(void) {
	if (!g_siegeHelp.integer)
		return;
	level.siegeHelpInitialized = qtrue;
	fileHandle_t f = 0;
	char mapname[64];
	trap_Cvar_VariableStringBuffer("mapname", mapname, sizeof(mapname));
	int len = trap_FS_FOpenFile(va("maps/%s.siegehelp", mapname), &f, FS_READ);
	if (!f) { // idk
		Com_Printf("No .siegehelp file was found for %s.\n", mapname);
		return;
	}
	if (len < 4) { // sanity check i guess
		Com_Printf("^1No valid siege help messages found. The siegehelp file was probably configured incorrectly.^7\n");
		trap_FS_FCloseFile(f);
		return;
	}

	char *buf = malloc(len + 1);
	memset(buf, 0, len + 1);
	if (f) {
		trap_FS_Read(buf, len, f);
		trap_FS_FCloseFile(f);
	}

	cJSON *root = cJSON_Parse(buf);
	if (!root) {
		Com_Printf("^1No valid siege help messages found. The siegehelp file was probably configured incorrectly. Check commas, etc.^7\n");
		Com_DebugPrintf("Error pointer: %s\n", cJSON_GetErrorPtr());
		free(buf);
		return;
	}
	cJSON *messages = cJSON_GetObjectItem(root, "messages");
	if (!messages) {
		Com_Printf("^1No valid siege help messages found. The siegehelp file was probably configured incorrectly. Check commas, etc.^7\n");
		cJSON_Delete(root);
		free(buf);
		return;
	}

	int numHelps = 0;
	for (int i = 0; i < cJSON_GetArraySize(messages); i++) {
		cJSON *thisMessageInArray = cJSON_GetArrayItem(messages, i);
		if (!thisMessageInArray)
			//break;
			continue;
		siegeHelp_t *help = ListAdd(&level.siegeHelpList, sizeof(siegeHelp_t));

		cJSON *start = cJSON_GetObjectItem(thisMessageInArray, "start");
		if (start) {
			for (int j = 0; j < MAX_SIEGEHELP_TARGETNAMES; j++) {
				cJSON *thisStart = cJSON_GetArrayItem(start, j);
				if (thisStart && cJSON_IsString(thisStart) && VALIDSTRING(thisStart->valuestring))
					Q_strncpyz(help->start[j], thisStart->valuestring, sizeof(help->start[j]));
			}
		}

		cJSON *end = cJSON_GetObjectItem(thisMessageInArray, "end");
		if (end) {
			for (int j = 0; j < MAX_SIEGEHELP_TARGETNAMES; j++) {
				cJSON *thisEnd = cJSON_GetArrayItem(end, j);
				if (thisEnd && cJSON_IsString(thisEnd) && VALIDSTRING(thisEnd->valuestring))
					Q_strncpyz(help->end[j], thisEnd->valuestring, sizeof(help->end[j]));
			}
		}

		cJSON *size = cJSON_GetObjectItem(thisMessageInArray, "size");
		if (size && cJSON_IsString(size) && VALIDSTRING(size->valuestring) && !Q_stricmpn(size->valuestring, "small", 5))
			help->smallSize = qtrue;

		// can either be the item's targetname or goaltarget
		cJSON *item = cJSON_GetObjectItem(thisMessageInArray, "item");
		if (item && cJSON_IsString(item) && VALIDSTRING(item->valuestring))
			Q_strncpyz(help->item, item->valuestring, sizeof(help->item));

		cJSON *redMsg = cJSON_GetObjectItem(thisMessageInArray, "redMsg");
		if (redMsg && cJSON_IsString(redMsg) && VALIDSTRING(redMsg->valuestring)) {
			Q_strncpyz(help->redMsg, redMsg->valuestring, sizeof(help->redMsg));
			char *checkQuotes = help->redMsg;
			while (checkQuotes = strchr(checkQuotes, '"')) // make sure idiots didn't write quotation marks
				*checkQuotes = '\'';
		}

		cJSON *blueMsg = cJSON_GetObjectItem(thisMessageInArray, "blueMsg");
		if (blueMsg && cJSON_IsString(blueMsg) && VALIDSTRING(blueMsg->valuestring)) {
			Q_strncpyz(help->blueMsg, blueMsg->valuestring, sizeof(help->blueMsg));
			char *checkQuotes = help->blueMsg;
			while (checkQuotes = strchr(checkQuotes, '"')) // make sure idiots didn't write quotation marks
				*checkQuotes = '\'';
		}
		if (!help->redMsg[0] && !help->blueMsg[0]) {
			Com_Printf("^1InitializeSiegeHelpMessages: error: message has empty redMsg and empty blueMsg!^7\n");
			ListRemove(&level.siegeHelpList, help);
			continue;
		}

		cJSON *origin = cJSON_GetObjectItem(thisMessageInArray, "origin");
		if (origin) {
			for (int j = 0; j < 3; j++) { // one coord for each of x, y, and z
				cJSON *thisCoord = cJSON_GetArrayItem(origin, j);
				if (thisCoord && cJSON_IsNumber(thisCoord))
					help->origin[j] = (int)thisCoord->valuedouble;
			}
		}

		cJSON *constraints = cJSON_GetObjectItem(thisMessageInArray, "constraints");
		if (constraints) {
			for (int j = 0; j < 3; j++) { // one set of constraints for each of x, y, and z
				cJSON *thisConstraint = cJSON_GetArrayItem(constraints, j);
				if (thisConstraint) {
					for (int k = 0; k < 2; k++) { // one lower bound and one upper bound for each set of constraints
						cJSON *thisConstraintBound = cJSON_GetArrayItem(thisConstraint, k);
						if (thisConstraintBound && cJSON_IsNumber(thisConstraintBound)) {
							help->constraints[j][k] = (int)thisConstraintBound->valuedouble;
							help->constrained[j][k] = qtrue;
						}
					}
					if (help->constrained[j][0] && help->constrained[j][1] && help->constraints[j][0] > help->constraints[j][1]) {
						// make sure they are in the correct order: low, high
						int temp = help->constraints[j][0];
						help->constraints[j][0] = help->constraints[j][1];
						help->constraints[j][1] = temp;
					}
				}
			}
		}

		if (!help->start[0][0])
			help->started = qtrue; // no start targetname; it just defaults to started at the beginning of the level

		numHelps++;
	}

	if (numHelps) {
		Com_Printf("Parsed %d siege help messages.\n", numHelps);
		level.siegeHelpValid = qtrue;
	}
	else {
		Com_Printf("^1No valid siege help messages found. The siegehelp file was probably configured incorrectly.^7\n");
	}

	free(buf);
}

// gets the current siege help messages and writes them to the supplied buffers
static void GetSiegeHelpMessages(char *redOut, size_t redOutSize, char *blueOut, size_t blueOutSize, char *bothOut, size_t bothOutSize) {
	assert(redOut && redOutSize && blueOut && blueOutSize && bothOut && bothOutSize && g_gametype.integer == GT_SIEGE);
	*redOut = *blueOut = *bothOut = '\0';
	char redTeamHelpBuf[MAX_STRING_CHARS] = { 0 }, blueTeamHelpBuf[MAX_STRING_CHARS] = { 0 }, bothTeamsHelpBuf[MAX_STRING_CHARS] = { 0 };
	Q_strncpyz(redTeamHelpBuf, "kls -1 -1 shlp", sizeof(redTeamHelpBuf));
	Q_strncpyz(blueTeamHelpBuf, "kls -1 -1 shlp", sizeof(blueTeamHelpBuf));
	Q_strncpyz(bothTeamsHelpBuf, "kls -1 -1 shlp", sizeof(bothTeamsHelpBuf));

	if (!level.zombies) {
		iterator_t iter = { 0 };
		ListIterate(&level.siegeHelpList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			siegeHelp_t *help = IteratorNext(&iter);
			if (help->ended || !help->started)
				continue;
			if (help->forceHideItem)
				continue;
			assert(help->redMsg[0] || help->blueMsg[0]);

			// message
			char thisRed[MAX_STRING_CHARS] = { 0 }, thisBlue[MAX_STRING_CHARS] = { 0 }, thisBoth[MAX_STRING_CHARS] = { 0 };
			if (help->redMsg[0]) {
				Q_strcat(thisRed, sizeof(thisRed), va("\"r=%s\"", help->redMsg));
				Q_strcat(thisBoth, sizeof(thisBoth), va("\"r=%s\"", help->redMsg));
			}
			if (help->blueMsg[0]) {
				Q_strcat(thisBlue, sizeof(thisBlue), va("\"b=%s\"", help->blueMsg));
				Q_strcat(thisBoth, sizeof(thisBoth), va("%s\"b=%s\"", help->redMsg[0] ? " " : "", help->blueMsg)); // extra space if red too
			}

			// small size?
			if (help->smallSize) {
				Q_strcat(thisBoth, sizeof(thisBoth), " \"s=1\"");
				if (help->redMsg[0])
					Q_strcat(thisRed, sizeof(thisRed), " \"s=1\"");
				if (help->blueMsg[0])
					Q_strcat(thisBlue, sizeof(thisBlue), " \"s=1\"");
			}

			// constraints?
			for (int j = 0; j < 3; j++) {
				for (int k = 0; k < 2; k++) {
					if (help->constrained[j][k]) {
						Q_strcat(thisBoth, sizeof(thisBoth), va(" \"c%d%d=%d\"", j, k, help->constraints[j][k]));
						if (help->redMsg[0])
							Q_strcat(thisRed, sizeof(thisRed), va(" \"c%d%d=%d\"", j, k, help->constraints[j][k]));
						if (help->blueMsg[0])
							Q_strcat(thisBlue, sizeof(thisBlue), va(" \"c%d%d=%d\"", j, k, help->constraints[j][k]));
					}
				}
			}

			// coordinates (these should be last)
			Q_strcat(thisBoth, sizeof(thisBoth), va(" \"x=%d\" \"y=%d\" \"z=%d\"", help->origin[0], help->origin[1], help->origin[2]));
			if (help->redMsg[0])
				Q_strcat(thisRed, sizeof(thisRed), va(" \"x=%d\" \"y=%d\" \"z=%d\"", help->origin[0], help->origin[1], help->origin[2]));
			if (help->blueMsg[0])
				Q_strcat(thisBlue, sizeof(thisBlue), va(" \"x=%d\" \"y=%d\" \"z=%d\"", help->origin[0], help->origin[1], help->origin[2]));

			// copy this one into the main strings
			if (strlen(bothTeamsHelpBuf) + strlen(thisBoth) < MAX_STRING_CHARS - 1)
				Q_strcat(bothTeamsHelpBuf, sizeof(bothTeamsHelpBuf), thisBoth);
			else
				Com_Printf("Warning: siege both teams help message does not fit! (\"%s^7\")\n", thisBoth);

			if (help->redMsg[0]) {
				if (strlen(redTeamHelpBuf) + strlen(thisRed) < MAX_STRING_CHARS - 1)
					Q_strcat(redTeamHelpBuf, sizeof(redTeamHelpBuf), thisRed);
				else
					Com_Printf("Warning: siege red help message does not fit! (\"%s^7\")\n", thisRed);
			}

			if (help->blueMsg[0]) {
				if (strlen(blueTeamHelpBuf) + strlen(thisBlue) < MAX_STRING_CHARS - 1)
					Q_strcat(blueTeamHelpBuf, sizeof(blueTeamHelpBuf), thisBlue);
				else
					Com_Printf("Warning: siege blue help message does not fit! (\"%s^7\")\n", thisBlue);
			}
		}
	}

	Q_strncpyz(redOut, redTeamHelpBuf, redOutSize);
	Q_strncpyz(blueOut, blueTeamHelpBuf, blueOutSize);
	Q_strncpyz(bothOut, bothTeamsHelpBuf, bothOutSize);
}

// sends current siege help to a client (-1 for everyone)
void SendSiegeHelpForClient(int client, team_t teamOverride) {
	assert(client >= -1 && client < MAX_CLIENTS);
	if (g_gametype.integer != GT_SIEGE || !g_siegeHelp.integer)
		return;
	if (!level.siegeHelpInitialized)
		InitializeSiegeHelpMessages();
	if (level.siegeStage != SIEGESTAGE_ROUND1 && level.siegeStage != SIEGESTAGE_ROUND2)
		return;
	if (!level.siegeHelpValid)
		return;
	char red[MAX_STRING_CHARS], blue[MAX_STRING_CHARS], both[MAX_STRING_CHARS];

	GetSiegeHelpMessages(&red[0], sizeof(red), &blue[0], sizeof(blue), &both[0], sizeof(both));

	if (client == -1) { // everyone
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (level.clients[i].pers.connected != CON_CONNECTED || level.clients[i].isLagging)
				continue;
			if (level.clients[i].sess.siegeDesiredTeam == TEAM_RED) {
				trap_SendServerCommand(i, red);
				//Com_DebugPrintf("Sent red help to client %d: %s^7\n", i, red);
			}
			else if (level.clients[i].sess.siegeDesiredTeam == TEAM_BLUE) {
				trap_SendServerCommand(i, blue);
				//Com_DebugPrintf("Sent blue help to client %d: %s^7\n", i, blue);
			}
			else {
				trap_SendServerCommand(i, both);
				//Com_DebugPrintf("Sent both help to client %d: %s^7\n", i, both);
			}
		}
	}
	else if (level.clients[client].pers.connected == CON_CONNECTED && !level.clients[client].isLagging) { // a specific client
		team_t team = teamOverride ? teamOverride : level.clients[client].sess.siegeDesiredTeam;
		if (team == TEAM_RED) {
			trap_SendServerCommand(client, red);
			//Com_DebugPrintf("Sent red help to client %d: %s^7\n", client, red);
		}
		else if (team == TEAM_BLUE) {
			trap_SendServerCommand(client, blue);
			//Com_DebugPrintf("Sent blue help to client %d: %s^7\n", client, blue);
		}
		else {
			trap_SendServerCommand(client, both);
			//Com_DebugPrintf("Sent both help to client %d: %s^7\n", client, both);
		}
	}
}

// sends siege help messages periodically
static void RunSiegeHelpMessages(void) {
	if (g_gametype.integer != GT_SIEGE || !g_siegeHelp.integer)
		return;
	if (level.siegeStage != SIEGESTAGE_ROUND1 && level.siegeStage != SIEGESTAGE_ROUND2)
		return;
	if (level.siegeHelpMessageTime && trap_Milliseconds() < level.siegeHelpMessageTime)
		return;

	// send help to everyone
	SendSiegeHelpForClient(-1, 0);

	// set the time for this function to run again
	level.siegeHelpMessageTime = trap_Milliseconds() + SIEGE_HELP_INTERVAL;
}

// use some bullshit unused fields to put homing data in the client's playerstate
static void SetHomingInPlayerstate(gclient_t *cl, int stage) {
	assert(cl);
	assert(stage >= 0 && stage <= 10);
	for (int i = 0; i < 4; i++) {
		if (stage & (1 << i))
			cl->ps.holocronBits |= (1 << (NUM_FORCE_POWERS + i));
		else
			cl->ps.holocronBits &= ~(1 << (NUM_FORCE_POWERS + i));
	}
}

static void RunImprovedHoming(void) {
	if (!g_improvedHoming.integer)
		return;

	float lockTimeInterval = ((g_gametype.integer == GT_SIEGE) ? 2400.0f : 1200.0f) / 16.0f;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		gclient_t *cl = &level.clients[i];
		if (cl->pers.connected != CON_CONNECTED) {
			cl->homingLockTime = 0;
			SetHomingInPlayerstate(cl, 0);
			continue;
		}

		// check if they are following someone
		gentity_t *lockEnt = ent;
		gclient_t *lockCl = cl;
		if (cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW &&
			cl->sess.spectatorClient >= 0 && cl->sess.spectatorClient < MAX_CLIENTS) {
			gclient_t *followed = &level.clients[cl->sess.spectatorClient];
			if (followed->pers.connected != CON_CONNECTED) {
				lockCl->homingLockTime = 0;
				SetHomingInPlayerstate(cl, 0);
				continue;
			}
			lockCl = followed;
			lockEnt = &g_entities[lockCl - level.clients];
		}

		if (lockCl->sess.sessionTeam == TEAM_SPECTATOR || lockEnt->health <= 0 || lockCl->ps.pm_type == PM_SPECTATOR || lockCl->ps.pm_type == PM_INTERMISSION) {
			lockCl->homingLockTime = 0;
			SetHomingInPlayerstate(cl, 0);
			continue;
		}

		// check that the rocketeer has a lock
		if (lockCl->ps.rocketLockIndex == ENTITYNUM_NONE) {
			SetHomingInPlayerstate(cl, 0);
			continue;
		}

		// get the lock stage
		float rTime = lockCl->ps.rocketLockTime == -1 ? lockCl->ps.rocketLastValidTime : lockCl->ps.rocketLockTime;
		int dif = Com_Clampi(0, 10, (level.time - rTime) / lockTimeInterval);
		if (lockCl->ps.m_iVehicleNum) {
			gentity_t *veh = &g_entities[lockCl->ps.m_iVehicleNum];
			if (veh->m_pVehicle) {
				vehWeaponInfo_t *vehWeapon = NULL;
				if (lockCl->ps.weaponstate == WEAPON_CHARGING_ALT) {
					if (veh->m_pVehicle->m_pVehicleInfo->weapon[1].ID > VEH_WEAPON_BASE &&veh->m_pVehicle->m_pVehicleInfo->weapon[1].ID < MAX_VEH_WEAPONS)
						vehWeapon = &g_vehWeaponInfo[veh->m_pVehicle->m_pVehicleInfo->weapon[1].ID];
				}
				else if (veh->m_pVehicle->m_pVehicleInfo->weapon[0].ID > VEH_WEAPON_BASE &&veh->m_pVehicle->m_pVehicleInfo->weapon[0].ID < MAX_VEH_WEAPONS) {
					vehWeapon = &g_vehWeaponInfo[veh->m_pVehicle->m_pVehicleInfo->weapon[0].ID];
				}
				if (vehWeapon) {//we are trying to lock on with a valid vehicle weapon, so use *its* locktime, not the hard-coded one
					if (!vehWeapon->iLockOnTime) { //instant lock-on
						dif = 10;
					}
					else {//use the custom vehicle lockOnTime
						lockTimeInterval = (vehWeapon->iLockOnTime / 16.0f);
						dif = Com_Clampi(0, 10, (level.time - rTime) / lockTimeInterval);
					}
				}
			}
		}

		// set the homing stage in their playerstate and start the special serverside timer
		SetHomingInPlayerstate(cl, dif);
		if (dif == 10) {
			lockCl->homingLockTime = level.time;
			lockCl->homingLockTarget = lockCl->ps.rocketLockIndex;
		}
	}
}
#endif

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void ClearNPCGlobals( void );
void AI_UpdateGroups( void );
void ClearPlayerAlertEvents( void );
void SiegeCheckTimers(void);
void WP_SaberStartMissileBlockCheck( gentity_t *self, usercmd_t *ucmd );
extern void Jedi_Decloak( gentity_t *self );
qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );

extern unsigned getstatus_LastIP;
extern int getstatus_TimeToReset;
extern int getstatus_Counter;
extern int getstatus_LastReportTime;
extern int getstatus_UniqueIPCount;
extern int gImperialCountdown;
extern int gRebelCountdown;

void UpdateSiegeStatus()
{
	//this would enable people to see what's going on in the server without connecting, similar to how you can see the current map name, etc from the serverinfo.
	char	string[128];
	int i, numConnectedPlayers = 0, numRedPlayers = 0, numBluePlayers = 0, numDuelingPlayers = 0;

	vmCvar_t mapname;
	trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);

	if (!mapname.string || !mapname.string[0] || !level.siegeRoundStartTime)
	{
		Com_sprintf(string, 128, "");
		trap_Cvar_Set("siegeStatus", va("%s", string)); //update it
		return;
	}

	for (i = 0; i < level.maxclients; i++)
	{
		if (level.clients[i].pers.connected != CON_DISCONNECTED) //connected player
		{
			numConnectedPlayers++;
			if (level.clients[i].sess.siegeDuelInProgress)
			{
				numDuelingPlayers++;
			}
			if (level.clients[i].sess.sessionTeam == TEAM_RED || level.inSiegeCountdown && level.clients[i].sess.sessionTeam == TEAM_SPECTATOR && level.clients[i].sess.siegeDesiredTeam == TEAM_RED)
			{
				numRedPlayers++;
			}
			else if (level.clients[i].sess.sessionTeam == TEAM_BLUE || level.inSiegeCountdown && level.clients[i].sess.sessionTeam == TEAM_SPECTATOR && level.clients[i].sess.siegeDesiredTeam == TEAM_BLUE)
			{
				numBluePlayers++;
			}
		}
	}

	if (!numConnectedPlayers || (!numRedPlayers && !numBluePlayers))
	{
		Com_sprintf(string, 128, "Awaiting players");
		trap_Cvar_Set("siegeStatus", va("%s", string)); //update it
		return;
	}

	if (level.inSiegeCountdown)
	{
		Com_sprintf(string, 128, "%iv%i: Countdown", numRedPlayers, numBluePlayers);
		trap_Cvar_Set("siegeStatus", va("%s", string)); //update it
		return;
	}

	if (level.intermissiontime || level.siegeRoundComplete)
	{
		//siegeRoundComplete is needed because intermissiontime isn't non-zero until a few moments after last obj is completed.
		//for example, if siegestatus is updated IMMEDIATELY after finishing round, but before intermissiontime is set, you can get some weird time/obj values.
		Com_sprintf(string, 128, "%iv%i: Intermission", numRedPlayers, numBluePlayers);
		trap_Cvar_Set("siegeStatus", va("%s", string)); //update it
		return;
	}

	if (numDuelingPlayers && numDuelingPlayers == 2)
	{
		Com_sprintf(string, 128, "Captain duel in progress");
		trap_Cvar_Set("siegeStatus", va("%s", string)); //update it
		return;
	}

	Com_sprintf(string, 128, "%iv%i", numRedPlayers, numBluePlayers);

	if (!g_siegeTeamSwitch.integer || !g_siegePersistant.beatingTime)
	{
		//round 1
		Com_sprintf(string, 128, "%s: Round 1", string);
	}
	else
	{
		//round 2
		Com_sprintf(string, 128, "%s: Round 2", string);
	}

	if (Q_stricmp(mapname.string, "siege_sillyroom") && Q_stricmp(mapname.string, "siege_codes") && Q_stricmpn(mapname.string, "mp/siege_crystals", 17))
	{
		if (level.objectiveJustCompleted == 0)
		{
			Com_sprintf(string, 128, "%s, 1st obj", string);
		}
		else if (level.objectiveJustCompleted == 1)
		{
			Com_sprintf(string, 128, "%s, 2nd obj", string);
		}
		else if (level.objectiveJustCompleted == 2)
		{
			Com_sprintf(string, 128, "%s, 3rd obj", string);
		}
		else
		{
			Com_sprintf(string, 128, "%s, %ith obj", string, level.objectiveJustCompleted + 1);
		}
	}

	int elapsedSeconds;
	int minutes;
	int seconds;

	if (!g_siegeTeamSwitch.integer || !g_siegePersistant.beatingTime)
	{
		elapsedSeconds = (((level.time - level.siegeRoundStartTime) + 500) / 1000); //convert milliseconds to seconds
	}
	else
	{
		elapsedSeconds = (((g_siegePersistant.lastTime - (level.time - level.siegeRoundStartTime)) + 500) / 1000); //convert milliseconds to seconds
	}
	minutes = (elapsedSeconds / 60) % 60; //find minutes
	seconds = elapsedSeconds % 60; //find seconds

	if (seconds >= 10) //seconds is double-digit
	{
		Com_sprintf(string, 128, "%s (%i:%i)", string, minutes, seconds); //convert to string as-is
	}
	else //seconds is single-digit
	{
		Com_sprintf(string, 128, "%s (%i:0%i)", string, minutes, seconds); //convert to string, and also add a zero if seconds is single-digit
	}

	trap_Cvar_Set("siegeStatus", va("%s", string)); //update it
}
#ifdef NEWMOD_SUPPORT
#define SIEGEITEM_UPDATE_INTERVAL	1000
void UpdateNewmodSiegeItems(void) {
	if (g_gametype.integer == GT_SIEGE && (level.siegeStage == SIEGESTAGE_PREROUND1 || level.siegeStage == SIEGESTAGE_PREROUND2))
		return;

	// loop through each team (red and red-followers only gets red carriers, blue and blue-followers only gets blue carriers, freespecs get all carriers)
	for (team_t currentTeam = TEAM_RED; currentTeam <= TEAM_SPECTATOR; currentTeam++) {
		int modelIndices[MAX_CLIENTS];
		qboolean foundAny = qfalse;
		for (int i = 0; i < MAX_CLIENTS; i++) { // get list of clients with siege items
			if (!(&g_entities[i] && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED && g_entities[i].client->holdingObjectiveItem &&
				g_entities[i].health > 0 && g_entities[i].client->tempSpectate < level.time && g_entities[i].client->ps.pm_type != PM_SPECTATOR &&
				(currentTeam != TEAM_SPECTATOR && g_entities[i].client->sess.sessionTeam == currentTeam || currentTeam == TEAM_SPECTATOR && (g_entities[i].client->sess.sessionTeam == TEAM_RED || g_entities[i].client->sess.sessionTeam == TEAM_BLUE)) &&
				&g_entities[g_entities[i].client->holdingObjectiveItem])) {
				modelIndices[i] = -1;
				continue;
			}
			foundAny = qtrue;
			modelIndices[i] = g_entities[g_entities[i].client->holdingObjectiveItem].s.modelindex;
		}

		char command[MAX_STRING_CHARS] = { 0 };
		Q_strncpyz(command, "si", sizeof(command));

		if (!foundAny) { // didn't find any; send empty message
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.sessionTeam == currentTeam) {
					trap_SendServerCommand(i, va("lchat %s", command));
					//Com_Printf("Sent no siege item to client %i\n", i);
				}
			}
			continue;
		}

		foundAny = qfalse;
		for (int i = 0; i < MAX_CLIENTS; i++) { // build the string we will send out
			if (modelIndices[i] >= 0) { // this player has an item; add him to the string as "clientNumber modelIndex"
				Q_strncpyz(command, va("%s %i %i", command, i, modelIndices[i]), sizeof(command));
				foundAny = qtrue;
			}
		}

		if (!foundAny)
			continue;

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.sessionTeam == currentTeam) {
				trap_SendServerCommand(i, va("kls -1 -1 %s", command)); // send it
				//Com_Printf("Sent siege item command to client %i: %s\n", i, command);
			}
		}
	}
}

#define MAX_SPECINFO_PLAYERS_PER_TEAM	8
#define MAX_SPECINFO_PLAYERS			(MAX_SPECINFO_PLAYERS_PER_TEAM * 2)
void CheckSpecInfo(void) {
	if (!g_specInfo.integer)
		return;

	static int lastUpdate = 0;
	int updateRate = Com_Clampi(1, 1000, g_teamOverlayUpdateRate.integer);
	if (lastUpdate && level.time - lastUpdate <= updateRate)
		return;

	if (g_gametype.integer == GT_SIEGE && (level.siegeStage == SIEGESTAGE_PREROUND1 || level.siegeStage == SIEGESTAGE_PREROUND2))
		return;

	// see if anyone is spec
	int i, numPlayers[3] = { 0 };
	qboolean gotRecipient = qfalse, include[MAX_CLIENTS] = { qfalse }, sendTo[MAX_CLIENTS] = { qfalse };
	for (i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		gclient_t *cl = &level.clients[i];
		if (!ent->inuse || cl->pers.connected != CON_CONNECTED /*|| ent->r.svFlags & SVF_BOT*/)
			continue;
		if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
			if (!cl->isLagging) {
				sendTo[i] = qtrue;
				gotRecipient = qtrue;
			}
		}
		else {
			if (g_gametype.integer < GT_TEAM && cl->sess.sessionTeam == TEAM_FREE && numPlayers[TEAM_FREE] < MAX_SPECINFO_PLAYERS) {
				include[i] = qtrue;
				numPlayers[TEAM_FREE]++;
			}
			else if (g_gametype.integer >= GT_TEAM && cl->sess.sessionTeam != TEAM_FREE && numPlayers[cl->sess.sessionTeam] < MAX_SPECINFO_PLAYERS_PER_TEAM) {
				include[i] = qtrue;
				numPlayers[cl->sess.sessionTeam]++;
			}
		}
	}
	if (!gotRecipient)
		return;
	lastUpdate = level.time;

	// build the spec info string
	char totalString[MAX_STRING_CHARS] = { 0 };
	Q_strncpyz(totalString, "kls -1 -1 snf2", sizeof(totalString));
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!include[i])
			continue;
		gentity_t *ent = &g_entities[i];
		gclient_t *cl = &level.clients[i];

		char playerString[MAX_STRING_CHARS] = { 0 };
		Q_strncpyz(playerString, va(" \"%d", i), sizeof(playerString));
		if (ent->health > 0 && !(g_gametype.integer == GT_SIEGE && ent->client->tempSpectate && ent->client->tempSpectate >= level.time))
			Q_strcat(playerString, sizeof(playerString), va(" h=%d", ent->health));
		if (cl->ps.stats[STAT_ARMOR] > 0 && ent->health > 0  && !(g_gametype.integer == GT_SIEGE && ent->client->tempSpectate && ent->client->tempSpectate >= level.time))
			Q_strcat(playerString, sizeof(playerString), va(" a=%d", cl->ps.stats[STAT_ARMOR]));
		Q_strcat(playerString, sizeof(playerString), va(" f=%d", !cl->ps.fd.forcePowersKnown ? -1 : cl->ps.fd.forcePower));
		Q_strcat(playerString, sizeof(playerString), va(" l=%d", g_gametype.integer < GT_TEAM ? Team_GetLocation(ent, NULL, 0) : cl->pers.teamState.location));
		if (g_gametype.integer == GT_SIEGE && cl->siegeClass != -1 && bgSiegeClasses[cl->siegeClass].maxhealth != 100)
			Q_strcat(playerString, sizeof(playerString), va(" mh=%d", bgSiegeClasses[cl->siegeClass].maxhealth));
		if (g_gametype.integer == GT_SIEGE && cl->siegeClass != -1 && bgSiegeClasses[cl->siegeClass].maxarmor != 100)
			Q_strcat(playerString, sizeof(playerString), va(" ma=%d", bgSiegeClasses[cl->siegeClass].maxarmor));
		if (ent->s.powerups)
			Q_strcat(playerString, sizeof(playerString), va(" p=%d", ent->s.powerups));
		Q_strcat(playerString, sizeof(playerString), "\"");

		Q_strcat(totalString, sizeof(totalString), playerString);
	}

	int len = strlen(totalString);
	if (len >= 1000) {
		G_LogPrintf("Warning: specinfo string is very long! (%d digits)\n", len);
	}

	// send it to specs
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!sendTo[i])
			continue;
		trap_SendServerCommand(i, totalString);
	}
}

static int GetMaxArmor(team_t team, stupidSiegeClassNum_t classNum) {
	assert(team == TEAM_RED || team == TEAM_BLUE);
	siegeClass_t *found = BG_SiegeGetClass(team, classNum);
	if (found)
		return found->maxarmor;
	else
		return 100;
}

void UpdateNewmodSiegeClassLimits(int clientNum) {
	if (g_gametype.integer != GT_SIEGE)
		return;
	assert(clientNum >= -1 && clientNum < MAX_CLIENTS);
	char *limitsString = va("kls -1 -1 scli \"%s\"", level.classLimits[0] ? level.classLimits : "0");
	static char armorString[MAX_STRING_CHARS] = { 0 };
	if (!armorString[0]) {
		Q_strncpyz(armorString, va("kls -1 -1 scma \"oa=%d oh=%d od=%d ot=%d os=%d oj=%d da=%d dh=%d dd=%d dt=%d ds=%d dj=%d\"",
			GetMaxArmor(TEAM_RED, SSCN_ASSAULT), GetMaxArmor(TEAM_RED, SSCN_HW), GetMaxArmor(TEAM_RED, SSCN_DEMO),
			GetMaxArmor(TEAM_RED, SSCN_TECH), GetMaxArmor(TEAM_RED, SSCN_SCOUT), GetMaxArmor(TEAM_RED, SSCN_JEDI),
			GetMaxArmor(TEAM_BLUE, SSCN_ASSAULT), GetMaxArmor(TEAM_BLUE, SSCN_HW), GetMaxArmor(TEAM_BLUE, SSCN_DEMO),
			GetMaxArmor(TEAM_BLUE, SSCN_TECH), GetMaxArmor(TEAM_BLUE, SSCN_SCOUT), GetMaxArmor(TEAM_BLUE, SSCN_JEDI)),
			sizeof(armorString));
	}
	if (clientNum == -1) { // everyone
		int i;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (!g_entities[i].inuse || level.clients[i].pers.connected != CON_CONNECTED)
				continue;
			trap_SendServerCommand(i, limitsString);
			trap_SendServerCommand(i, armorString);
		}
	}
	else { // single client
		if (!g_entities[clientNum].inuse || level.clients[clientNum].pers.connected != CON_CONNECTED)
			return;
		trap_SendServerCommand(clientNum, limitsString);
		trap_SendServerCommand(clientNum, armorString);
	}
}

static void CheckNewmodSiegeClassLimits(void) {
	// generate the current string
	char newString[MAX_STRING_CHARS];
	if (g_gametype.integer != GT_SIEGE || !g_classLimits.integer) {
		Q_strncpyz(newString, "0", sizeof(newString));
	}
	else {
		Q_strncpyz(newString, va("oa=%d oh=%d od=%d ot=%d os=%d oj=%d da=%d dh=%d dd=%d dt=%d ds=%d dj=%d",
			oAssaultLimit.integer, oHWLimit.integer, oDemoLimit.integer, oTechLimit.integer, oScoutLimit.integer, oJediLimit.integer,
			dAssaultLimit.integer, dHWLimit.integer, dDemoLimit.integer, dTechLimit.integer, dScoutLimit.integer, dJediLimit.integer),
			sizeof(newString));
	}

	// compare to the old string
	if (!level.classLimits[0] || Q_stricmp(level.classLimits, newString)) {
		Q_strncpyz(level.classLimits, newString, sizeof(level.classLimits));
		UpdateNewmodSiegeClassLimits(-1);
	}
}
#endif

extern void WP_AddToClientBitflags(gentity_t *ent, int entNum);

int GetCurrentRestartCountdown(void) {
	char buf[32] = { 0 };
	trap_GetConfigstring(CS_WARMUP, buf, sizeof(buf));
	if (buf[0]) {
		int num = atoi(buf);
		if (num > 0)
			return num;
	}
	return 0;
}

#define AUTORESTART_AFK_MIN				(1)
#define AUTORESTART_AFK_MAX				(30)
#define AUTORESTART_AFK_DEFAULT			(5)

#define AUTORESTART_COUNTDOWN_MIN		(3)
#define AUTORESTART_COUNTDOWN_MAX		(30)
#define AUTORESTART_COUNTDOWN_DEFAULT	(10)

#define AUTORESTART_MINPLAYERS_MIN		(2)
#define AUTORESTART_MINPLAYERS_MAX		(MAX_CLIENTS)
#define AUTORESTART_MINPLAYERS_DEFAULT	(4) // 2v2

static void RunAutoRestart(void) {
	if (g_gametype.integer != GT_SIEGE)
		return;

	const int currentCountdown = GetCurrentRestartCountdown();
	static int autoCountdown = 0;

	if (!g_autoStart.integer || level.intermissiontime || level.randomizedTeams) {
		level.randomizedTeams = qfalse;
		if (currentCountdown && autoCountdown) {
			// edge case: the g_autoStart cvar was disabled or intermission was reached while the auto countdown was active
			// cancel the auto countdown
			trap_SendConsoleCommand(EXEC_APPEND, "map_restart -1\n");
			autoCountdown = 0;
		}
		return;
	}

	if (autoCountdown && !currentCountdown) {
		// edge case: an admin used rcon map_restart -1 to cancel the auto countdown
		// set the g_autoStart cvar to 0 to prevent instantly spamming auto countdown
		trap_Cvar_Set("g_autoStart", "0");
		autoCountdown = 0;
		return;
	}

	if (currentCountdown && autoCountdown && abs(currentCountdown - autoCountdown) > 200) {
		// edge case: a countdown was started elsewhere (map_restart vote/rcon, etc) while the auto countdown was active
		// e.g., auto countdown was ticking down with 2 seconds left and then ski used rcon map_restart 300
		// disable special auto countdown checks (afk detection, etc) so that this function treats it as a manual countdown
		autoCountdown = 0;
		return;
	}

	int numRed = 0, numBlue = 0;
	enum {
		CLIENTNUMAFK_MULTIPLE = -2,
		CLIENTNUMAFK_NONE = -1
	} clientNumAfk = CLIENTNUMAFK_NONE;
	enum {
		CLIENTNUMNOTREADY_MULTIPLE = -2,
		CLIENTNUMNOTREADY_NONE = -1
	} clientNumNotReady = CLIENTNUMNOTREADY_NONE;
	int now = getGlobalTime();
	int afkThreshold = g_autoStartAFKThreshold.integer;
	if (afkThreshold <= 0)
		afkThreshold = AUTORESTART_AFK_DEFAULT;
	afkThreshold = Com_Clampi(AUTORESTART_AFK_MIN, AUTORESTART_AFK_MAX, afkThreshold);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED)
			continue;
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->sess.sessionTeam == TEAM_FREE)
			continue;

		if (ent->client->sess.sessionTeam == TEAM_RED)
			numRed++;
		else
			numBlue++;

		if (!ent->client->pers.ready) {
			if (clientNumNotReady == CLIENTNUMNOTREADY_NONE)
				clientNumNotReady = i;
			else
				clientNumNotReady = CLIENTNUMNOTREADY_MULTIPLE;
		}
		if (now - ent->client->pers.lastInputTime > afkThreshold) {
			if (clientNumAfk == CLIENTNUMAFK_NONE)
				clientNumAfk = i;
			else
				clientNumAfk = CLIENTNUMAFK_MULTIPLE;
		}
	}

	int minPlayers = g_autoStartMinPlayers.integer;
	if (minPlayers <= 0)
		minPlayers = AUTORESTART_MINPLAYERS_DEFAULT;
	minPlayers = Com_Clampi(AUTORESTART_MINPLAYERS_MIN, AUTORESTART_MINPLAYERS_MAX, minPlayers);
	if (currentCountdown) {
		if (!autoCountdown) {
			// there is currently a countdown, but it wasn't from here (map_restart vote/rcon, etc); don't do anything at all.
			return;
		}
		char cancelReason[256] = { 0 };
		if (numRed + numBlue < minPlayers)
			Q_strncpyz(cancelReason, "Not enough players ingame", sizeof(cancelReason));
		else if (clientNumNotReady == CLIENTNUMNOTREADY_MULTIPLE)
			Q_strncpyz(cancelReason, "Multiple players are not ready", sizeof(cancelReason));
		else if (clientNumNotReady >= 0 && clientNumNotReady < MAX_CLIENTS)
			Q_strncpyz(cancelReason, va("%s^7 is not ready", level.clients[clientNumNotReady].pers.netname), sizeof(cancelReason));
		else if (numRed != numBlue)
			Q_strncpyz(cancelReason, "Uneven # of players", sizeof(cancelReason));
		else if (clientNumAfk == CLIENTNUMAFK_MULTIPLE)
			Q_strncpyz(cancelReason, "Multiple players are AFK", sizeof(cancelReason));
		else if (clientNumAfk >= 0 && clientNumAfk < MAX_CLIENTS)
			Q_strncpyz(cancelReason, va("%s^7 is AFK", level.clients[clientNumAfk].pers.netname), sizeof(cancelReason));

		if (cancelReason[0]) {
			/*
			there is currently an auto countdown, but ANY ONE of the following is true:
			- someone has unreadied
			- someone has become afk
			- the teams have become uneven
			- there are now less than 8 players ingame

			cancel the auto countdown
			*/
			trap_SendConsoleCommand(EXEC_APPEND, "map_restart -1\n");
			autoCountdown = 0;
			trap_SendServerCommand(-1, va("print \"^1Auto-start cancelled:^7 %s^7\n\"", cancelReason));
			trap_SendServerCommand(-1, va("cp \"^1Auto-start cancelled:^7\n%s^7\n\"", cancelReason));
		}
	}
	else {
		if (numRed == numBlue && numRed + numBlue >= minPlayers && clientNumNotReady == CLIENTNUMNOTREADY_NONE && clientNumAfk == CLIENTNUMAFK_NONE) {
			/*
			there is NOT currently a countdown, and ALL of the following are true:
			- everyone is ready
			- everyone is present
			- there are at least 8 players ingame
			- the teams are even

			start the auto countdown
			*/
			int seconds = g_autoStartTimer.integer;
			if (seconds <= 0)
				seconds = AUTORESTART_COUNTDOWN_DEFAULT;
			seconds = Com_Clampi(AUTORESTART_COUNTDOWN_MIN, AUTORESTART_COUNTDOWN_MAX, seconds);
			trap_SendConsoleCommand(EXEC_NOW, va("map_restart %d\n", seconds));
			autoCountdown = level.time + (seconds * 1000);
		}
	}
}

void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;
#ifdef _G_FRAME_PERFANAL
	int			iTimer_ItemRun = 0;
	int			iTimer_ROFF = 0;
	int			iTimer_ClientEndframe = 0;
	int			iTimer_GameChecks = 0;
	int			iTimer_Queues = 0;
	void		*timer_ItemRun;
	void		*timer_ROFF;
	void		*timer_ClientEndframe;
	void		*timer_GameChecks;
	void		*timer_Queues;
#endif
	static int lastMsgTime = 0;

#ifdef NEWMOD_SUPPORT
	for (i = 0; i < MAX_CLIENTS; i++)
		level.clients[i].realPing = level.clients[i].ps.ping;

	if (g_gametype.integer == GT_SIEGE) {
		if (!level.siegeItemUpdateTime || level.siegeItemUpdateTime <= level.time) {
			level.siegeItemUpdateTime = level.time + SIEGEITEM_UPDATE_INTERVAL;
			UpdateNewmodSiegeItems();
		}
	}
#endif

	if (g_gametype.integer == GT_SIEGE) {
		if (!level.siegeStatusUpdateTime || level.siegeStatusUpdateTime <= level.time)
		{
			//level.siegeStatusUpdateTime = level.time + 1000; //update every second (debug)
			level.siegeStatusUpdateTime = level.time + 15000; //update every 15 seconds
			UpdateSiegeStatus();
		}
	}
	else
	{
		vmCvar_t	siegeStatus;
		trap_Cvar_Register(&siegeStatus, "siegeStatus", "", CVAR_ROM | CVAR_SERVERINFO);
		if (Q_stricmp(siegeStatus.string, ""))
		{
			trap_Cvar_Set("siegeStatus", "");
		}
	}
#ifdef _DEBUG
	if ( g_antiWallhack.integer && g_wallhackMaxTraces.integer && level.wallhackTracesDone ) {
#if 0
		G_LogPrintf( "Last frame's WH check terminated with %d traces done\n", level.wallhackTracesDone );
#endif
		if ( level.wallhackTracesDone > g_wallhackMaxTraces.integer ) {
			G_LogPrintf( "Last frame's WH check terminated prematurely with %d traces (limit: %d)\n", level.wallhackTracesDone, g_wallhackMaxTraces.integer );
		}
	}
#endif

#ifdef NEWMOD_SUPPORT
	static qboolean flagsSet = qfalse; // make sure it gets set at least once
	static int saberFlags, netUnlock;
	if (!flagsSet || saberFlags != g_balanceSaber.integer || netUnlock != g_netUnlock.integer) {
		saberFlags = g_balanceSaber.integer;
		netUnlock = g_netUnlock.integer;

		int sendFlags = 0;
		if (saberFlags & SB_KICK)
			sendFlags |= NMF_KICK;
		if (saberFlags & SB_BACKFLIP)
			sendFlags |= NMF_BACKFLIP;
		if (netUnlock)
			sendFlags |= NMF_NETUNLOCK;

		trap_Cvar_Set("g_nmFlags", va("%i", sendFlags));
		flagsSet = qtrue;
	}
#endif

	// saber off damage boost
	if (g_gametype.integer == GT_SIEGE) {
		static int lastTime = 0;
		static qboolean saberOn[MAX_CLIENTS] = { qfalse }, notified[MAX_CLIENTS] = { qfalse };
		if (lastTime) {
			for (i = 0; i < MAX_CLIENTS; i++) {
				gclient_t *cl = &level.clients[i];
				if (cl->pers.connected != CON_CONNECTED ||
					cl->siegeClass == -1 ||
					!bgSiegeClasses[cl->siegeClass].saberOffDamageBoost ||
					cl->sess.sessionTeam == TEAM_SPECTATOR ||
					cl->ps.stats[STAT_HEALTH] <= 0 ||
					cl->tempSpectate > level.time ||
					cl->ps.weapon != WP_SABER) {
					cl->saberIgniteTime = 0;
					cl->saberUnigniteTime = 0;
					cl->saberBonusTime = 0;
					saberOn[i] = qfalse;
					notified[i] = qfalse;
					continue;
				}
				if (cl->ps.saberHolstered) {
					cl->saberUnigniteTime += (level.time - lastTime);
					if (cl->saberUnigniteTime >= 3000 && !notified[i]) {
						int j;
						for (j = 0; j < MAX_CLIENTS; j++) {
							if (level.clients[j].pers.connected != CON_CONNECTED)
								continue;
							if (j != i && !(level.clients[j].sess.sessionTeam == TEAM_SPECTATOR && level.clients[j].sess.spectatorState == SPECTATOR_FOLLOW && level.clients[j].sess.spectatorClient == i))
								continue;
							gentity_t *te = G_TempEntity(cl->ps.origin, EV_TEAM_POWER);
							te->s.eventParm = 2;
							te->s.saberInFlight = 1; // tell client this is a special te
							te->r.svFlags |= SVF_SINGLECLIENT;
							te->r.svFlags |= SVF_BROADCAST;
							te->r.singleClient = j;
							WP_AddToClientBitflags(te, i);
						}
						notified[i] = qtrue;
					}
					saberOn[i] = qfalse;
				}
				else {
					if (!saberOn[i]) {
						cl->saberIgniteTime = level.time;
						if (cl->saberUnigniteTime >= 3000) {
							cl->saberBonusTime = level.time;
						}
					}
					saberOn[i] = qtrue;
					notified[i] = qfalse;
					cl->saberUnigniteTime = 0;
				}
			}
		}
		lastTime = level.time;
	}

	level.wallhackTracesDone = 0; // reset the traces for the next ClientThink wave

	UpdateGlobalCenterPrint( levelTime );

	// check for modified physics and disable capture times if non standard
	if (g_saveCaptureRecords.integer && !level.mapCaptureRecords.readonly && level.time > 1000 ) { // wat. it seems that sv_cheats = 1 on first frame... so don't check until 1000ms i guess
		if ( !pmove_float.integer ) {
			G_Printf( S_COLOR_YELLOW"pmove_float is not enabled. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
#ifndef _DEBUG
		} else if (g_cheats.integer != 0) {
			G_Printf(S_COLOR_YELLOW"Cheats are enabled. Capture records won't be tracked during this map.\n");
			level.mapCaptureRecords.readonly = qtrue;
#endif
		} else if ( g_svfps.integer != 30 ) {
			G_Printf( S_COLOR_YELLOW"Server FPS is not standard. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
		} else if ( g_speed.value != 250 ) {
			G_Printf( S_COLOR_YELLOW"Speed is not standard. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
		} else if ( g_gravity.value != 760 ) {
			G_Printf( S_COLOR_YELLOW"Gravity is not standard. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
		} else if ((level.siegeMap == SIEGEMAP_KORRIBAN && !(g_knockback.integer == 1000 || !g_knockback.integer)) || g_knockback.integer != 1000) {
			G_Printf(S_COLOR_YELLOW"Knockback is not standard. Capture records won't be tracked during this map.\n");
			level.mapCaptureRecords.readonly = qtrue;
		} else if ( g_saberDamageScale.value != 1.0f) {
			G_Printf( S_COLOR_YELLOW"Saber damage scale is not standard. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
		} else if ( g_forceRegenTime.value != 200 ) {
			G_Printf( S_COLOR_YELLOW"Force regen is not standard. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
		} else if ((g_redTeam.string[0] && Q_stricmp(g_redTeam.string, "none") && Q_stricmp(g_redTeam.string, "0")) ||
			(g_blueTeam.string[0] && Q_stricmp(g_blueTeam.string, "none") && Q_stricmp(g_blueTeam.string, "0"))) {
			G_Printf( S_COLOR_YELLOW"Custom classes are in use. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
#if 0
		} else if ( g_siegeRespawn.integer != 20 ) {
			G_Printf( S_COLOR_YELLOW"Respawn time is not standard. Capture records won't be tracked during this map.\n" );
			level.mapCaptureRecords.readonly = qtrue;
#endif
		} else if (level.siegeMap == SIEGEMAP_HOTH && (g_hothHangarHack.integer != HOTHHANGARHACK_5SECONDS || g_hothRebalance.integer != 3)) {
			G_Printf(S_COLOR_YELLOW"Hoth settings are not standard. Capture records won't be tracked during this map.\n");
			level.mapCaptureRecords.readonly = qtrue;
		}
	}

	if (g_gametype.integer == GT_SIEGE && g_siegeRespawn.integer && level.siegeRespawnCheck < level.time) { // check for a respawn wave
		int i;
		qboolean hasAliveTech[3] = { qfalse };
		for (i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *clEnt = &g_entities[i];
			if (!clEnt->inuse || !clEnt->client || clEnt->client->sess.sessionTeam == TEAM_SPECTATOR || clEnt->client->sess.sessionTeam == TEAM_FREE)
				continue;

			if ((clEnt->client->tempSpectate > level.time) || (clEnt->health <= 0 && level.time > clEnt->client->respawnTime)) {
				respawn(clEnt);
				clEnt->client->tempSpectate = 0;
			}
			else if (clEnt->client->siegeClass != -1 && bgSiegeClasses[clEnt->client->siegeClass].invenItems & (1 << HI_SHIELD)) {
				hasAliveTech[clEnt->client->sess.sessionTeam] = qtrue;
			}
		}

		for (i = TEAM_RED; i <= TEAM_BLUE; i++) {
			if (level.canShield[i] == CANSHIELD_YO_NOTPLACED) {	
				level.canShield[i] = CANSHIELD_YES; // respawn wave hit and this team didn't place yo shield ==> can shield
			}
			else if (level.canShield[i] == CANSHIELD_YO_PLACED) {
				if (hasAliveTech[i])
					level.canShield[i] = CANSHIELD_NO; // respawn wave hit and this team placed yo shield but didn't die ==> cannot shield
				else
					level.canShield[i] = CANSHIELD_YES; // respawn wave hit and this team placed yo shield and died ==> can shield
			}
		}

		if (!g_siegeRespawn.integer || g_siegeRespawn.integer == 1)
			level.siegeRespawnCheck = level.time;
		else
			level.siegeRespawnCheck = level.time + g_siegeRespawn.integer * 1000;

#ifdef NEWMOD_SUPPORT
		UpdateNewmodSiegeTimers();
#endif

	}

	if (gDoSlowMoDuel)
	{
		if (level.restarted)
		{
			char buf[128];
			float tFVal = 0;

			trap_Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

			tFVal = atof(buf);

			trap_Cvar_Set("timescale", "1");
			if (tFVal == 1.0f)
			{
				gDoSlowMoDuel = qfalse;
			}
		}
		else
		{
			float timeDif = (level.time - gSlowMoDuelTime); //difference in time between when the slow motion was initiated and now
			float useDif = 0; //the difference to use when actually setting the timescale

			if (timeDif < 150)
			{
				trap_Cvar_Set("timescale", "0.1f");
			}
			else if (timeDif < 1150)
			{
				useDif = (timeDif/1000); //scale from 0.1 up to 1
				if (useDif < 0.1)
				{
					useDif = 0.1;
				}
				if (useDif > 1.0)
				{
					useDif = 1.0;
				}
				trap_Cvar_Set("timescale", va("%f", useDif));
			}
			else
			{
				char buf[128];
				float tFVal = 0;

				trap_Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

				tFVal = atof(buf);

				trap_Cvar_Set("timescale", "1");
				if (timeDif > 1500 && tFVal == 1.0f)
				{
					gDoSlowMoDuel = qfalse;
				}
			}
		}
	}

	// if we are waiting for the level to restart, do nothing
	if ( level.restarted ) {
		return;
	}

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;

	//OSP: pause
	if (level.pause.state != PAUSE_NONE)
	{
		static int lastCSTime = 0;
		int dt = level.time - level.previousTime;

		// compensate for timelimit and warmup time
		if (level.warmupTime > 0)
			level.warmupTime += dt;
		level.startTime += dt;

#if 0
		// disabled these two lines of sil code because of fucking with siege respawn checks
		// could probably try to account for it, but more trouble than it's worth
		// the only downside of removing this appears to be that the white timer could flicker
		
		// floor start time to avoid time flipering
		if ((level.time - level.startTime) % 1000 >= 500)
			level.startTime += (level.time - level.startTime) % 1000;
#endif

		// initial CS update time, needed!
		if (!lastCSTime)
			lastCSTime = level.time;

		// client needs to do the same, just adjust the configstrings periodically
		// i can't see a way around this mess without requiring a client mod.
		if (lastCSTime < level.time - 500) {
			lastCSTime += 500;
			trap_SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));
			if (level.warmupTime > 0)
				trap_SetConfigstring(CS_WARMUP, va("%i", level.warmupTime));
		}

		if (level.teamBalanceCheckTime)
			level.teamBalanceCheckTime += dt;

		if (g_gametype.integer == GT_SIEGE)
		{
			static int accumulatedDt = 0;

			level.siegeRoundStartTime += dt;

			// siege timer adjustment
			accumulatedDt += dt;

			if (accumulatedDt >= 1000)
			{
				char siegeState[128];
				int round, startTime;
				trap_GetConfigstring(CS_SIEGE_STATE, siegeState, sizeof(siegeState));
				sscanf(siegeState, "%i|%i", &round, &startTime);
				startTime += accumulatedDt;
				trap_SetConfigstring(CS_SIEGE_STATE, va("%i|%i", round, startTime));

				accumulatedDt = 0;
			}

			level.siegeRespawnCheck += dt;

#ifdef NEWMOD_SUPPORT
			UpdateNewmodSiegeTimers();
#endif

			// siege objectives timers adjustments
			if (gImperialCountdown)
			{
				gImperialCountdown += dt;
			}

			if (gRebelCountdown)
			{
				gRebelCountdown += dt;
			}

		}
	}
	if (level.pause.state == PAUSE_PAUSED)
	{
		if (lastMsgTime < level.time - 1000) {
			int pauseSecondsRemaining = (int)ceilf((level.pause.time - level.time) / 1000.0f);
			if (level.pause.reason[0])
				trap_SendServerCommand(-1, va("cp \"The match has been auto-paused. (%s%d^7)\n%s\n\"",
					pauseSecondsRemaining <= 10 ? S_COLOR_RED : S_COLOR_WHITE, pauseSecondsRemaining, level.pause.reason));
			else
				trap_SendServerCommand(-1, va("cp \"The match has been paused. (%s%d^7)\n\"",
					pauseSecondsRemaining <= 10 ? S_COLOR_RED : S_COLOR_WHITE, pauseSecondsRemaining));
			lastMsgTime = level.time;
		}

		//if ( level.time > level.pause.time - (japp_unpauseTime.integer*1000) )
		if (level.time > level.pause.time - (5 * 1000)) // 5 seconds
			level.pause.state = PAUSE_UNPAUSING;
	}
	if (level.pause.state == PAUSE_UNPAUSING)
	{
		level.pause.reason[0] = '\0';
		if (lastMsgTime < level.time - 500) {
			int pauseSecondsRemaining = (int)ceilf((level.pause.time - level.time) / 1000.0f);
			trap_SendServerCommand(-1, va("cp \"^1MATCH IS UNPAUSING^7\nin %d^7...\n\"", pauseSecondsRemaining));
			lastMsgTime = level.time;
		}

		if (level.time > level.pause.time) {
			level.pause.state = PAUSE_NONE;
			level.pause.unpauseTime = level.time;
			trap_SendServerCommand(-1, "cp \"^2Go!^7\n\"");
		}
	}

	if (g_allowNPC.integer)
	{
	}

	AI_UpdateGroups();

	if (g_allowNPC.integer)
	{
		if ( d_altRoutes.integer )
		{
			trap_Nav_CheckAllFailedEdges();
		}
		trap_Nav_ClearCheckedNodes();

		//remember last waypoint, clear current one
		for ( i = 0; i < level.num_entities ; i++) 
		{
			ent = &g_entities[i];

			if ( !ent->inuse )
				continue;

			if ( ent->waypoint != WAYPOINT_NONE 
				&& ent->noWaypointTime < level.time )
			{
				ent->lastWaypoint = ent->waypoint;
				ent->waypoint = WAYPOINT_NONE;
			}
			if ( d_altRoutes.integer )
			{
				trap_Nav_CheckFailedNodes( ent );
			}
		}

		//Look to clear out old events
		ClearPlayerAlertEvents();
	}

	g_TimeSinceLastFrame = (level.time - g_LastFrameTime);

	// get any cvar changes
	G_UpdateCvars();



#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_ItemRun);
#endif
	//
	// go through all allocated objects
	//
	ent = &g_entities[0];
	for (i=0 ; i<level.num_entities ; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}

		// clear events that are too old
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	
				if ( ent->client ) {
					ent->client->ps.externalEvent = 0;
					// predicted events should never be set to zero
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				if (ent->s.eFlags & EF_SOUNDTRACKER)
				{ //don't trigger the event again..
					ent->s.event = 0;
					ent->s.eventParm = 0;
					ent->s.eType = 0;
					ent->eventTime = 0;
				}
				else
				{
					G_FreeEntity( ent );
					continue;
				}
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap_UnlinkEntity( ent );
			}
		}

		// temporary entities don't think
		if ( ent->freeAfterEvent ) {
			continue;
		}

		if ( !ent->r.linked && ent->neverFree ) {
			continue;
		}

		if ( ent->s.eType == ET_MISSILE ) {
            //OSP: pause
            if ( level.pause.state == PAUSE_NONE )
                    G_RunMissile( ent );
            else
            {// During a pause, gotta keep track of stuff in the air
                    ent->s.pos.trTime += level.time - level.previousTime;
                    G_RunThink( ent );
            }
			continue;
		}

		if ( ent->s.eType == ET_ITEM || ent->physicsObject ) {
#if 0 //use if body dragging enabled?
			if (ent->s.eType == ET_BODY)
			{ //special case for bodies
				float grav = 3.0f;
				float mass = 0.14f;
				float bounce = 1.15f;

				G_RunExPhys(ent, grav, mass, bounce, qfalse, NULL, 0);
			}
			else
			{
				G_RunItem( ent );
			}
#else
			G_RunItem( ent );
#endif
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) {
			G_RunMover( ent );
			continue;
		}

		if ( i < MAX_CLIENTS ) 
		{
			G_CheckClientTimeouts ( ent );
			
			if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE)
			{ //we're in space, check for suffocating and for exiting
                gentity_t *spacetrigger = &g_entities[ent->client->inSpaceIndex];

				if (!spacetrigger->inuse ||
					!G_PointInBounds(ent->client->ps.origin, spacetrigger->r.absmin, spacetrigger->r.absmax))
				{ //no longer in space then I suppose
                    ent->client->inSpaceIndex = 0;					
				}
				else
				{ //check for suffocation
                    if (ent->client->inSpaceSuffocation < level.time)
					{ //suffocate!
						if (ent->health > 0 && ent->takedamage)
						{ //if they're still alive..
							G_Damage(ent, spacetrigger, spacetrigger, NULL, ent->client->ps.origin, Q_irand(50, 70), DAMAGE_NO_ARMOR, MOD_SUICIDE);

							if (ent->health > 0)
							{ //did that last one kill them?
								//play the choking sound
								G_EntitySound(ent, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )));

								//make them grasp their throat
								ent->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
								ent->client->ps.forceHandExtendTime = level.time + 2000;
							}
						}

						ent->client->inSpaceSuffocation = level.time + Q_irand(100, 200);
					}
				}
			}

			if (ent->client->isHacking && level.pause.state == PAUSE_NONE)
			{ //hacking checks
				gentity_t *hacked = &g_entities[ent->client->isHacking];
				vec3_t angDif;

				VectorSubtract(ent->client->ps.viewangles, ent->client->hackingAngles, angDif);

				//keep him in the "use" anim
				if (ent->client->ps.torsoAnim != BOTH_CONSOLE1)
				{
					G_SetAnim(ent, NULL, SETANIM_TORSO, BOTH_CONSOLE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
				}
				else
				{
					ent->client->ps.torsoTimer = 500;
				}
				ent->client->ps.weaponTime = ent->client->ps.torsoTimer;

				if (!(ent->client->pers.cmd.buttons & BUTTON_USE) && !(level.pause.unpauseTime && abs(level.time - level.pause.unpauseTime) < 100))
				{ //have to keep holding use
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!hacked || !hacked->inuse)
				{ //shouldn't happen, but safety first
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!G_PointInBounds( ent->client->ps.origin, hacked->r.absmin, hacked->r.absmax ))
				{ //they stepped outside the thing they're hacking, so reset hacking time
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!(ent->r.svFlags & SVF_BOT) && VectorLength(angDif) > 10.0f && !(level.pause.unpauseTime && abs(level.time - level.pause.unpauseTime) < 100) &&
					!ent->client->sess.skillBoost)
				{ //must remain facing generally the same angle as when we start
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
			}
			else if (ent->client->isHacking) { // allow them to spin around without penalty if hacking during pause
				VectorCopy(ent->client->ps.viewangles, ent->client->hackingAngles);
			}

#define JETPACK_DEFUEL_RATE		200 //approx. 20 seconds of idle use from a fully charged fuel amt
#define JETPACK_REFUEL_RATE		150 //seems fair
			if (ent->client->jetPackOn && level.pause.state == PAUSE_NONE) { //using jetpack, drain fuel
				if (ent->client->jetPackDebReduce < level.time) {
					if (g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1 && bgSiegeClasses[ent->client->siegeClass].shortBurstJetpack)
						ent->client->ps.jetpackFuel -= 3;
					else
						ent->client->ps.jetpackFuel -= (ent->client->pers.cmd.upmove > 0) ? 2 : 1; // take more if they're thrusting
					
					if (ent->client->ps.jetpackFuel <= 0) { // turn it off
						ent->client->ps.jetpackFuel = 0;
						Jetpack_Off(ent);
					}
					if (g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1 && bgSiegeClasses[ent->client->siegeClass].shortBurstJetpack)
						ent->client->jetPackDebReduce = level.time + 14;
					else
						ent->client->jetPackDebReduce = level.time + JETPACK_DEFUEL_RATE;
				}
			}
			else if (ent->client->ps.jetpackFuel < 100 && level.pause.state == PAUSE_NONE) { //recharge jetpack
				if (ent->client->jetPackDebRecharge < level.time) {
					ent->client->ps.jetpackFuel++;
					if (g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1 && bgSiegeClasses[ent->client->siegeClass].shortBurstJetpack)
						ent->client->jetPackDebRecharge = level.time + (JETPACK_REFUEL_RATE / 2);
					else
						ent->client->jetPackDebRecharge = level.time + JETPACK_REFUEL_RATE;
				}
			}

#define CLOAK_DEFUEL_RATE		200 //approx. 20 seconds of idle use from a fully charged fuel amt
#define CLOAK_REFUEL_RATE		150 //seems fair
			if (ent->client->ps.powerups[PW_CLOAKED] && level.pause.state == PAUSE_NONE)
			{ //using cloak, drain battery
				if (ent->client->cloakDebReduce < level.time)
				{
					ent->client->ps.cloakFuel--;
					
					if (ent->client->ps.cloakFuel <= 0)
					{ //turn it off
						ent->client->ps.cloakFuel = 0;
						Jedi_Decloak(ent);
					}
					ent->client->cloakDebReduce = level.time + CLOAK_DEFUEL_RATE;
				}
			}
			else if (ent->client->ps.cloakFuel < 100)
			{ //recharge cloak
				if (ent->client->cloakDebRecharge < level.time)
				{
					ent->client->ps.cloakFuel++;
					ent->client->cloakDebRecharge = level.time + CLOAK_REFUEL_RATE;
				}
			}

			if (g_gametype.integer == GT_SIEGE && !ent->client->isLagging &&
				((ent->client->siegeClass != -1 && bgSiegeClasses[ent->client->siegeClass].classflags & (1<<CFL_STATVIEWER)) ||
					(ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.spectatorState == SPECTATOR_FOLLOW &&
						ent->client->sess.spectatorClient >= 0 && ent->client->sess.spectatorClient < MAX_CLIENTS &&
						g_entities[ent->client->sess.spectatorClient].client && g_entities[ent->client->sess.spectatorClient].client->sess.sessionTeam != TEAM_SPECTATOR &&
						g_entities[ent->client->sess.spectatorClient].client->siegeClass != -1 && bgSiegeClasses[g_entities[ent->client->sess.spectatorClient].client->siegeClass].classflags & (1 << CFL_STATVIEWER))))
			{ //see if it's time to send this guy an update of extended info
				if (ent->client->siegeEDataSend < level.time)
				{
					int updateRate = Com_Clampi(1, 1000, g_teamOverlayUpdateRate.integer);
                    G_SiegeClientExData(ent);
					if (ent->client->isMedHealingSomeone || ent->client->isMedSupplyingSomeone || (ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.spectatorState == SPECTATOR_FOLLOW &&
						ent->client->sess.spectatorClient >= 0 && ent->client->sess.spectatorClient < MAX_CLIENTS &&
						g_entities[ent->client->sess.spectatorClient].client && g_entities[ent->client->sess.spectatorClient].client->sess.sessionTeam != TEAM_SPECTATOR &&
						g_entities[ent->client->sess.spectatorClient].client->siegeClass != -1 && bgSiegeClasses[g_entities[ent->client->sess.spectatorClient].client->siegeClass].classflags & (1 << CFL_STATVIEWER) &&
						(g_entities[ent->client->sess.spectatorClient].client->isMedHealingSomeone || g_entities[ent->client->sess.spectatorClient].client->isMedSupplyingSomeone))) {
						ent->client->siegeEDataSend = level.time + (updateRate < 100 ? updateRate : 100); // faster if you are actively healing someone
					}
					else {
						ent->client->siegeEDataSend = level.time + updateRate;
					}
				}
			}

            if ( level.pause.state == PAUSE_NONE
                    && !level.intermissiontime
                    && !(ent->client->ps.pm_flags & PMF_FOLLOW)
                    && ent->client->sess.sessionTeam != TEAM_SPECTATOR )
			{
				WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
				WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
				WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);

				if (level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) {
					if (ent->client->sess.sessionTeam == TEAM_RED)
						level.numRedPlayerTicks++;
					else if (ent->client->sess.sessionTeam == TEAM_BLUE)
						level.numBluePlayerTicks++;
				}

				if (ent->client->ps.pm_type != PM_SPECTATOR && ent->health > 0) {
					// this client is in game and not dead, update speed stats

					float xyspeed = 0;
					if ( ent->client->ps.m_iVehicleNum ) {
						gentity_t *currentVeh = &g_entities[ent->client->ps.m_iVehicleNum];

						if ( currentVeh->client ) {
							xyspeed = sqrt( currentVeh->client->ps.velocity[0] * currentVeh->client->ps.velocity[0] + currentVeh->client->ps.velocity[1] * currentVeh->client->ps.velocity[1] );
						}
					} else {
						xyspeed = sqrt( ent->client->ps.velocity[0] * ent->client->ps.velocity[0] + ent->client->ps.velocity[1] * ent->client->ps.velocity[1] );
					}

					ent->client->pers.displacement += xyspeed / g_svfps.value;
					ent->client->pers.displacementSamples++;

					if ( xyspeed > ent->client->pers.topSpeed ) {
						ent->client->pers.topSpeed = xyspeed;
					}
				}
			}

			if (g_allowNPC.integer)
			{
				//This was originally intended to only be done for client 0.
				//Make sure it doesn't slow things down too much with lots of clients in game.
			}

			trap_ICARUS_MaintainTaskManager(ent->s.number);

			G_RunClient( ent );
			continue;
		}
		else if (ent->s.eType == ET_NPC)
		{
			int j;
			// turn off any expired powerups
			for ( j = 0 ; j < MAX_POWERUPS ; j++ ) {
				if ( ent->client->ps.powerups[ j ] < level.time ) {
					ent->client->ps.powerups[ j ] = 0;
				}
			}

			WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
			WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
			WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);
		}

		G_RunThink( ent );

		if (g_allowNPC.integer)
		{
			ClearNPCGlobals();
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_ItemRun = trap_PrecisionTimer_End(timer_ItemRun);
#endif

	if (level.pause.state == PAUSE_NONE && !level.intermissiontime && (level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2)) {
		level.numTeamTicks++;
	}

	SiegeCheckTimers();

#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_ROFF);
#endif
	trap_ROFF_UpdateEntities();
#ifdef _G_FRAME_PERFANAL
	iTimer_ROFF = trap_PrecisionTimer_End(timer_ROFF);
#endif



#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_ClientEndframe);
#endif
	// perform final fixups on the players
	ent = &g_entities[0];
	for (i=0 ; i < level.maxclients ; i++, ent++ ) {
		if ( ent->inuse ) {
			ClientEndFrame( ent );
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_ClientEndframe = trap_PrecisionTimer_End(timer_ClientEndframe);
#endif



#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_GameChecks);
#endif
	// see if it is time to do a tournement restart
	CheckTournament();

	// see if it is time to end the level
	CheckExitRules();

	// update to team status?
	CheckTeamStatus();

#ifdef NEWMOD_SUPPORT
	// send extra data to specs
	CheckSpecInfo();
#endif

	// cancel vote if timed out
	CheckVote();

	// check for allready during warmup
	CheckReady();

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	// for tracking changes
	CheckCvars();

	//account processing - accounts system
	//processDatabase();	

#ifdef HOOK_GETSTATUS_FIX
	//check getstatus flood report timers
	if (getstatus_TimeToReset &&
		(level.time >= getstatus_TimeToReset)){
		//reset the counter
		//allow only one report per minute
		if ((getstatus_Counter >= g_maxstatusrequests.integer)
			&& (!getstatus_LastReportTime || (level.time - getstatus_LastReportTime > 60*1000))){
			G_HackLog("Too many getstatus requests detected! (%i Hz, %i.%i.%i.%i, unique:%i)\n",
				getstatus_Counter,
				(unsigned char)(getstatus_LastIP),
				(unsigned char)(getstatus_LastIP>>8),
				(unsigned char)(getstatus_LastIP>>16),
				(unsigned char)(getstatus_LastIP>>24),
				getstatus_UniqueIPCount);
			getstatus_LastReportTime = level.time;
		}

		getstatus_Counter = 0;
		getstatus_UniqueIPCount = 0;
		//turn off timer until next getstatus occur
		getstatus_TimeToReset = 0;
	}
#endif

	static qboolean checkedLivePug = qfalse;
	if (g_gametype.integer == GT_SIEGE && level.isLivePug != ISLIVEPUG_NO && /*it might have already been non-lived by admin command*/
		(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) && level.siegeRoundStartTime) {
		if (!checkedLivePug && level.time - level.siegeRoundStartTime >= LIVEPUG_CHECK_TIME) {

			// check whether live pug conditions are met
			char *notLiveReason = NULL;
			level.isLivePug = CheckLivePug(&notLiveReason);
			if (level.isLivePug == ISLIVEPUG_YES) {
				level.pugWasLiveAtSomePoint = qtrue;
				Com_Printf("Pug is live\n");
				SpeedRunModeRuined("G_RunFrame: CheckLivePug");
			}
			else {
				Com_Printf("Pug is not live (reason: %s)\n", notLiveReason);
			}
			checkedLivePug = qtrue;
		}

		// check periodically whether team #s are correct
		if (level.isLivePug == ISLIVEPUG_YES && !level.pause.state) {
			CheckBadTeamNumbers();
		}
	}

    // report time wrapping 20 minutes ahead
    #define SERVER_WRAP_RESTART_TIME 0x70000000
    if ( enginePatched && (level.time >= SERVER_WRAP_RESTART_TIME - 20*60*1000 ) )
    {
        static int nextWrappingReportTime = 0;
        if ( level.time >= nextWrappingReportTime )
        {
             int remaining = (SERVER_WRAP_RESTART_TIME - level.time) / 1000;

             G_LogPrintf("Restarting server in %i seconds due to time wrap\n",remaining);
			 trap_SendServerCommand( -1, 
                    va("cp \"Restarting server in %i seconds due to time wrap\n\"", remaining) );

             // report every 1 second last minute, otherwise every 1 minute
             if ( level.time >= SERVER_WRAP_RESTART_TIME - 60*1000 )
             {                     
                nextWrappingReportTime = level.time + 1000;
             }
             else
             {
                nextWrappingReportTime = level.time + 60*1000;
             }
        }
    }  

	if (g_listEntity.integer) {
		for (i = 0; i < MAX_GENTITIES; i++) {
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		trap_Cvar_Set("g_listEntity", "0");
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_GameChecks = trap_PrecisionTimer_End(timer_GameChecks);
#endif

#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_Queues);
#endif
	//At the end of the frame, send out the ghoul2 kill queue, if there is one
	G_SendG2KillQueue();

	if (gQueueScoreMessage)
	{
		if (gQueueScoreMessageTime < level.time)
		{
			SendScoreboardMessageToAllClients();

			gQueueScoreMessageTime = 0;
			gQueueScoreMessage = 0;
		}
	}
	else if ( g_autoSendScores.integer )
	{
		// if we don't have a message queued, do it now
		gQueueScoreMessage = qtrue;
		gQueueScoreMessageTime = g_autoSendScores.integer;

		if ( gQueueScoreMessageTime < 500 ) {
			gQueueScoreMessageTime = 500;
		}

		gQueueScoreMessageTime += level.time;
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_Queues = trap_PrecisionTimer_End(timer_Queues);
#endif



#ifdef _G_FRAME_PERFANAL
	Com_Printf("---------------\nItemRun: %i\nROFF: %i\nClientEndframe: %i\nGameChecks: %i\nQueues: %i\n---------------\n",
		iTimer_ItemRun,
		iTimer_ROFF,
		iTimer_ClientEndframe,
		iTimer_GameChecks,
		iTimer_Queues);
#endif

	if (g_maxGameClients.integer)
	{
		//automatically unlock the teams if they are locked while 0 people are ingame
		int playersInGame = 0;
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (level.clients[i].pers.connected != CON_DISCONNECTED && (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR && level.clients[i].sess.siegeDesiredTeam != TEAM_SPECTATOR)))
			{
				playersInGame++;
			}
		}
		if (!playersInGame)
		{
			trap_Cvar_Set("g_maxGameClients", "0");
			trap_SendServerCommand(-1, va("print \"Teams automatically unlocked due to lack of in-game players.\n\""));
		}
	}
	
	char currentMap[MAX_QPATH] = { 0 };
	trap_Cvar_VariableStringBuffer("mapname", currentMap, sizeof(currentMap));
	if (g_defaultMap.string[0] && !(currentMap[0] && !Q_stricmp(currentMap, g_defaultMap.string))) {
		qboolean anyoneConnected = qfalse;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (level.clients[i].pers.connected != CON_DISCONNECTED) {
				anyoneConnected = qtrue;
				break;
			}
		}
		if (anyoneConnected) {
			level.nobodyHereTime = 0;
		}
		else {
			if (!level.nobodyHereTime) {
				level.nobodyHereTime = level.time;
			}
			else if (level.time - level.nobodyHereTime >= 60000) {
				G_LogPrintf(va("60 seconds passed with nobody connected, reverting to g_defaultMap %s.\n", g_defaultMap.string));
				trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", g_defaultMap.string));
			}
		}
	}

	if (level.siegeMap == SIEGEMAP_URBAN) {
		static gentity_t *swoop = NULL, *icon = NULL;
		if (!swoop) {
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				if (g_entities[i].m_pVehicle && g_entities[i].m_pVehicle->m_pVehicleInfo && g_entities[i].m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER && VALIDSTRING(g_entities[i].m_pVehicle->m_pVehicleInfo->name) && !Q_stricmp(g_entities[i].m_pVehicle->m_pVehicleInfo->name, "swoop_urban")) {
					swoop = &g_entities[i];
					break;
				}
			}
		}
		if (!icon) {
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				if (VALIDSTRING(g_entities[i].targetname) && !Q_stricmp(g_entities[i].targetname, "swoopicon") && VALIDSTRING(g_entities[i].classname) && !Q_stricmp(g_entities[i].classname, "info_siege_radaricon")) {
					icon = &g_entities[i];
					break;
				}
			}
		}
		if (swoop && icon && level.totalObjectivesCompleted == 3 && !level.zombies) {
			icon->s.eFlags |= EF_RADAROBJECT;
			icon->r.svFlags |= SVF_BROADCAST;
			icon->s.origin[0] = swoop->r.currentOrigin[0];
			icon->s.origin[1] = swoop->r.currentOrigin[1];
			icon->s.origin[2] = swoop->r.currentOrigin[2];
			G_SetOrigin(icon, swoop->r.currentOrigin);
		}
		else {
			if (icon) {
				icon->s.eFlags &= ~EF_RADAROBJECT;
				icon->r.svFlags &= ~SVF_BROADCAST;
			}
		}
	}

#ifdef NEWMOD_SUPPORT
	CheckNewmodSiegeClassLimits();
#endif

	static qboolean didInitialHothStuff = qfalse;
	if (!didInitialHothStuff && g_gametype.integer == GT_SIEGE && level.siegeMap == SIEGEMAP_HOTH &&
		(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
		level.siegeRoundStartTime && level.time - level.siegeRoundStartTime >= 5000) {
		didInitialHothStuff = qtrue;

		// fix inconsistent atst spawning on hoth by always spawning it 5 seconds after round start
		gentity_t *atst = G_Find(NULL, FOFS(targetname), "atst_1");
		if (atst && !Q_stricmp(atst->classname, "npc_vehicle")) {
			atst->delay = 0;
			GlobalUse(atst, NULL, NULL);
			atst->delay = 10000;
		}

		// remove infirmary lock to short
		if (g_hothInfirmaryRebalance.integer) {
			for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *lock = &g_entities[i];
				if (!lock->inuse)
					continue;
				if (VALIDSTRING(lock->target) && !strcmp(lock->target, "medlabplatb_lockdoor")) {
					lock->die(lock, lock, lock, 999999, MOD_UNKNOWN);
					break;
				}
			}
		}
	}

	if (g_gametype.integer == GT_SIEGE && level.intermissionNeededTime && !level.didLogExit) {
		int now = trap_Milliseconds();
		if (now - level.intermissionNeededTime > 1000) {
			LogExit("Siege intermission override");
		}
	}

	if (g_emotes.integer && g_gametype.integer == GT_SIEGE && level.isLivePug != ISLIVEPUG_NO && g_siegeRespawn.integer >= 5 &&
		level.siegeRespawnCheck > level.time &&level.siegeRespawnCheck - level.time <= 3000 && level.siegeRespawnCheck - level.time > 1000) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gclient_t *killCl = &level.clients[i];
			if (!killCl->emoted || killCl->pers.connected != CON_CONNECTED ||
				killCl->ps.stats[STAT_HEALTH] <= 0 || killCl->tempSpectate > level.time ||
				killCl->ps.pm_type == PM_DEAD)
				continue;
			killCl->emoted = qfalse;
			gentity_t *killEnt = &g_entities[i];
			killCl->ps.stats[STAT_HEALTH] = killEnt->health = -999;
			killEnt->flags &= ~FL_GODMODE;
			player_die(killEnt, killEnt, killEnt, 100000, MOD_SUICIDE);
		}
	}

	if (g_gametype.integer == GT_SIEGE) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gclient_t *cl = &level.clients[i];
			cl->ps.stats[STAT_SIEGEFLAGS] = 0;
			if (cl->pers.connected != CON_CONNECTED)
				continue;
			if (cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW &&
				cl->sess.spectatorClient >= 0 && cl->sess.spectatorClient < MAX_CLIENTS) {
				gclient_t *followed = &level.clients[cl->sess.spectatorClient];
				if (followed->pers.connected != CON_CONNECTED)
					continue;
				if (followed->siegeClass == -1)
					continue;
				if (bgSiegeClasses[followed->siegeClass].classflags & (1 << CFL_SPIDERMAN))
					cl->ps.stats[STAT_SIEGEFLAGS] |= (1 << SIEGEFLAG_SPIDERMAN);
				if (bgSiegeClasses[followed->siegeClass].classflags & (1 << CFL_GRAPPLE))
					cl->ps.stats[STAT_SIEGEFLAGS] |= (1 << SIEGEFLAG_GRAPPLE);
				if (bgSiegeClasses[followed->siegeClass].classflags & (1 << CFL_KICK))
					cl->ps.stats[STAT_SIEGEFLAGS] |= (1 << SIEGEFLAG_KICK);
			}
			else {
				if (bgSiegeClasses[cl->siegeClass].classflags & (1 << CFL_SPIDERMAN))
					cl->ps.stats[STAT_SIEGEFLAGS] |= (1 << SIEGEFLAG_SPIDERMAN);
				if (bgSiegeClasses[cl->siegeClass].classflags & (1 << CFL_GRAPPLE))
					cl->ps.stats[STAT_SIEGEFLAGS] |= (1 << SIEGEFLAG_GRAPPLE);
				if (bgSiegeClasses[cl->siegeClass].classflags & (1 << CFL_KICK))
					cl->ps.stats[STAT_SIEGEFLAGS] |= (1 << SIEGEFLAG_KICK);
			}
		}
	}

#ifdef NEWMOD_SUPPORT
	RunSiegeHelpMessages();
	RunImprovedHoming();
#endif

	RunAutoRestart();

	// clear out onLiftDuringPause if we've been unpaused for more than 2 ticks
	int tickTime = 1000 / g_svfps.integer;
	if (level.pause.unpauseTime && level.pause.state == PAUSE_NONE && level.time - level.pause.unpauseTime > tickTime * 2) {
		for (int i = 0; i < MAX_CLIENTS; i++)
			level.clients[i].pers.onLiftDuringPause = NULL;
	}

	level.frameStartTime = trap_Milliseconds(); // accurate timer

	g_LastFrameTime = level.time;
}

const char *G_GetStringEdString(char *refSection, char *refName)
{
	//Well, it would've been lovely doing it the above way, but it would mean mixing
	//languages for the client depending on what the server is. So we'll mark this as
	//a stringed reference with @@@ and send the refname to the client, and when it goes
	//to print it will get scanned for the stringed reference indication and dealt with
	//properly.
	static char text[1024]={0};
	Com_sprintf(text, sizeof(text), "@@@%s", refName);
	return text;
}




