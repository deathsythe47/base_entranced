// Copyright (C) 2000-2002 Raven Software, Inc.
//
/*****************************************************************************
 * name:		bg_saga.c
 *
 * desc:		Siege module, shared for game, cgame, and ui.
 *
 * $Author: osman $ 
 * $Revision: 1.9 $
 *
 *****************************************************************************/
#include "q_shared.h"
#include "bg_saga.h"
#include "bg_weapons.h"
#include "bg_public.h"

#define SIEGECHAR_TAB 9 //perhaps a bit hacky, but I don't think there's any define existing for "tab"

//Could use strap stuff but I don't particularly care at the moment anyway.
#include "namespace_begin.h"

extern int	trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
extern void	trap_FS_Read( void *buffer, int len, fileHandle_t f );
extern void	trap_FS_Write( const void *buffer, int len, fileHandle_t f );
extern void	trap_FS_FCloseFile( fileHandle_t f );
extern int	trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize );

#ifndef QAGAME //cgame, ui
qhandle_t	trap_R_RegisterShaderNoMip( const char *name );
#endif

char		siege_info[MAX_SIEGE_INFO_SIZE];
int			siege_valid = 0;

siegeTeam_t *team1Theme = NULL;
siegeTeam_t *team2Theme = NULL;

siegeClass_t bgSiegeClasses[MAX_SIEGE_CLASSES];
int bgNumSiegeClasses = 0;

siegeTeam_t bgSiegeTeams[MAX_SIEGE_TEAMS];
int bgNumSiegeTeams = 0;

//class flags
stringID_table_t bgSiegeClassFlagNames[] =
{
	ENUM2STRING(CFL_MORESABERDMG),
	ENUM2STRING(CFL_STRONGAGAINSTPHYSICAL),
	ENUM2STRING(CFL_FASTFORCEREGEN),
	ENUM2STRING(CFL_STATVIEWER),
	ENUM2STRING(CFL_HEAVYMELEE),
	ENUM2STRING(CFL_SINGLE_ROCKET),
	ENUM2STRING(CFL_CUSTOMSKEL),
	ENUM2STRING(CFL_EXTRA_AMMO),
	ENUM2STRING(CFL_SPIDERMAN),
	ENUM2STRING(CFL_GRAPPLE),
	ENUM2STRING(CFL_KICK),
	ENUM2STRING(CFL_WONDERWOMAN),
	ENUM2STRING(CFL_SMALLSHIELD),
	ENUM2STRING(CFL_BIGSMALLSHIELD),
	ENUM2STRING(CFL_EXPLOSIVESENTRY),
	ENUM2STRING(CFL_TAKESPRENERFDISRUPTDMG),
	ENUM2STRING(CFL_NODISRUPTFALLOFF),
	{"", -1}
};

//saber stances
stringID_table_t StanceTable[] =
{
	ENUM2STRING(SS_NONE),
	ENUM2STRING(SS_FAST),
	ENUM2STRING(SS_MEDIUM),
	ENUM2STRING(SS_STRONG),
	ENUM2STRING(SS_DESANN),
	ENUM2STRING(SS_TAVION),
	ENUM2STRING(SS_DUAL),
	ENUM2STRING(SS_STAFF),
	{"", 0}
};

stringID_table_t OtherEntTypeTable[] = {
	ENUM2STRING(OTHERENTTYPE_SELF),
	ENUM2STRING(OTHERENTTYPE_ALLY),
	ENUM2STRING(OTHERENTTYPE_ENEMY),
	ENUM2STRING(OTHERENTTYPE_VEHICLE),
	ENUM2STRING(OTHERENTTYPE_OTHER),
	{"", -1}
};

stringID_table_t ModTable[] = {
	ENUM2STRING(MOD_UNKNOWN),
	ENUM2STRING(MOD_STUN_BATON),
	ENUM2STRING(MOD_MELEE),
	ENUM2STRING(MOD_SABER),
	ENUM2STRING(MOD_BRYAR_PISTOL),
	ENUM2STRING(MOD_BRYAR_PISTOL_ALT),
	ENUM2STRING(MOD_BLASTER),
	ENUM2STRING(MOD_TURBLAST),
	ENUM2STRING(MOD_DISRUPTOR),
	ENUM2STRING(MOD_DISRUPTOR_SPLASH),
	ENUM2STRING(MOD_DISRUPTOR_SNIPER),
	ENUM2STRING(MOD_BOWCASTER),
	ENUM2STRING(MOD_REPEATER),
	ENUM2STRING(MOD_REPEATER_ALT),
	ENUM2STRING(MOD_REPEATER_ALT_SPLASH),
	ENUM2STRING(MOD_DEMP2),
	ENUM2STRING(MOD_DEMP2_ALT),
	ENUM2STRING(MOD_FLECHETTE),
	ENUM2STRING(MOD_FLECHETTE_ALT_SPLASH),
	ENUM2STRING(MOD_ROCKET),
	ENUM2STRING(MOD_ROCKET_SPLASH),
	ENUM2STRING(MOD_ROCKET_HOMING),
	ENUM2STRING(MOD_ROCKET_HOMING_SPLASH),
	ENUM2STRING(MOD_THERMAL),
	ENUM2STRING(MOD_THERMAL_SPLASH),
	ENUM2STRING(MOD_TRIP_MINE_SPLASH),
	ENUM2STRING(MOD_TIMED_MINE_SPLASH),
	ENUM2STRING(MOD_DET_PACK_SPLASH),
	ENUM2STRING(MOD_VEHICLE),
	ENUM2STRING(MOD_CONC),
	ENUM2STRING(MOD_CONC_ALT),
	ENUM2STRING(MOD_FORCE_DARK),
	ENUM2STRING(MOD_SENTRY),
	ENUM2STRING(MOD_WATER),
	ENUM2STRING(MOD_SLIME),
	ENUM2STRING(MOD_LAVA),
	ENUM2STRING(MOD_CRUSH),
	ENUM2STRING(MOD_TELEFRAG),
	ENUM2STRING(MOD_FALLING),
	ENUM2STRING(MOD_SUICIDE),
	ENUM2STRING(MOD_TARGET_LASER),
	ENUM2STRING(MOD_TRIGGER_HURT),
	ENUM2STRING(MOD_TEAM_CHANGE),
	ENUM2STRING(MOD_MAX),
	ENUM2STRING(MOD_SPECIAL_SENTRYBOMB),
	{"",  0xFFFFFFFFFFFFFFFFll}
};

//Weapon and force power tables are also used in NPC parsing code and some other places.
stringID_table_t WPTable[] =
{
	{"NULL",WP_NONE},
	ENUM2STRING(WP_NONE),
	// Player weapons
	ENUM2STRING(WP_STUN_BATON),
	ENUM2STRING(WP_MELEE),
	ENUM2STRING(WP_SABER),
	ENUM2STRING(WP_BRYAR_PISTOL),
	{"WP_BLASTER_PISTOL", WP_BRYAR_PISTOL},
	ENUM2STRING(WP_BLASTER),
	ENUM2STRING(WP_DISRUPTOR),
	ENUM2STRING(WP_BOWCASTER),
	ENUM2STRING(WP_REPEATER),
	ENUM2STRING(WP_DEMP2),
	ENUM2STRING(WP_FLECHETTE),
	ENUM2STRING(WP_ROCKET_LAUNCHER),
	ENUM2STRING(WP_THERMAL),
	ENUM2STRING(WP_TRIP_MINE),
	ENUM2STRING(WP_DET_PACK),
	ENUM2STRING(WP_CONCUSSION),
	ENUM2STRING(WP_BRYAR_OLD),
	ENUM2STRING(WP_EMPLACED_GUN),
	ENUM2STRING(WP_TURRET),
	{"", 0}
};

stringID_table_t FPTable[] =
{
	ENUM2STRING(FP_HEAL),
	ENUM2STRING(FP_LEVITATION),
	ENUM2STRING(FP_SPEED),
	ENUM2STRING(FP_PUSH),
	ENUM2STRING(FP_PULL),
	ENUM2STRING(FP_TELEPATHY),
	ENUM2STRING(FP_GRIP),
	ENUM2STRING(FP_LIGHTNING),
	ENUM2STRING(FP_RAGE),
	ENUM2STRING(FP_PROTECT),
	ENUM2STRING(FP_ABSORB),
	ENUM2STRING(FP_TEAM_HEAL),
	ENUM2STRING(FP_TEAM_FORCE),
	ENUM2STRING(FP_DRAIN),
	ENUM2STRING(FP_SEE),
	ENUM2STRING(FP_SABER_OFFENSE),
	ENUM2STRING(FP_SABER_DEFENSE),
	ENUM2STRING(FP_SABERTHROW),
	{"",	-1}
};

stringID_table_t HoldableTable[] =
{
	ENUM2STRING(HI_NONE),

	ENUM2STRING(HI_SEEKER),
	ENUM2STRING(HI_SHIELD),
	ENUM2STRING(HI_MEDPAC),
	ENUM2STRING(HI_MEDPAC_BIG),
	ENUM2STRING(HI_BINOCULARS),
	ENUM2STRING(HI_SENTRY_GUN),
	ENUM2STRING(HI_JETPACK),
	ENUM2STRING(HI_HEALTHDISP),
	ENUM2STRING(HI_AMMODISP),
	ENUM2STRING(HI_EWEB),
	ENUM2STRING(HI_CLOAK),
	{"", -1}
};

stringID_table_t PowerupTable[] =
{
	ENUM2STRING(PW_NONE),
	ENUM2STRING(PW_QUAD),
	ENUM2STRING(PW_BATTLESUIT),
	ENUM2STRING(PW_PULL),
	ENUM2STRING(PW_REDFLAG),
	ENUM2STRING(PW_BLUEFLAG),
	ENUM2STRING(PW_NEUTRALFLAG),
	ENUM2STRING(PW_SHIELDHIT),
	ENUM2STRING(PW_SPEEDBURST),
	ENUM2STRING(PW_DISINT_4),
	ENUM2STRING(PW_SPEED),
	ENUM2STRING(PW_CLOAKED),
	ENUM2STRING(PW_FORCE_ENLIGHTENED_LIGHT),
	ENUM2STRING(PW_FORCE_ENLIGHTENED_DARK),
	ENUM2STRING(PW_FORCE_BOON),
	ENUM2STRING(PW_YSALAMIRI),

	{"", -1}
};

stringID_table_t AmmoTable[] = 
{
	ENUM2STRING(AMMO_NONE),
	ENUM2STRING(AMMO_FORCE),
	ENUM2STRING(AMMO_BLASTER),
	ENUM2STRING(AMMO_POWERCELL),
	ENUM2STRING(AMMO_METAL_BOLTS),
	ENUM2STRING(AMMO_ROCKETS),
	ENUM2STRING(AMMO_EMPLACED),
	ENUM2STRING(AMMO_THERMAL),
	ENUM2STRING(AMMO_TRIPMINE),
	ENUM2STRING(AMMO_DETPACK),
	{"", -1}
};

// this crap was done inconsistently so i have to resort to this
stupidSiegeClassNum_t SiegeClassEnumToStupidClassNumber(siegePlayerClassFlags_t scl) {
	switch (scl) {
	case SPC_INFANTRY:			return SSCN_ASSAULT;
	case SPC_HEAVY_WEAPONS:		return SSCN_HW;
	case SPC_DEMOLITIONIST:		return SSCN_DEMO;
	case SPC_VANGUARD:			return SSCN_SCOUT;
	case SPC_SUPPORT:			return SSCN_TECH;
	case SPC_JEDI:				return SSCN_JEDI;
	default:					return -1;
	}
}


//======================================
//Parsing functions
//======================================
void BG_SiegeStripTabs(char *buf)
{
	int i = 0;
	int i_r = 0;

	while (buf[i])
	{
		if (buf[i] != SIEGECHAR_TAB)
		{ //not a tab, just stick it in
			buf[i_r] = buf[i];
		}
		else
		{ //If it's a tab, convert it to a space.
			buf[i_r] = ' ';
		}

		i_r++;
		i++;
	}

	buf[i_r] = '\0';
}

int BG_SiegeGetValueGroup(char *buf, char *group, char *outbuf)
{
	int i = 0;
	int j;
	char checkGroup[4096];
	qboolean isGroup;
	int parseGroups = 0;

	while (buf[i])
	{
		if (buf[i] != ' ' && buf[i] != '{' && buf[i] != '}' && buf[i] != '\n' && buf[i] != '\r' && buf[i] != SIEGECHAR_TAB)
		{ //we're on a valid character
			if (buf[i] == '/' &&
				buf[i+1] == '/')
			{ //this is a comment, so skip over it
				while (buf[i] && buf[i] != '\n' && buf[i] != '\r' && buf[i] != SIEGECHAR_TAB)
				{
					i++;
				}
			}
			else
			{ //parse to the next space/endline/eos and check this value against our group value.
				j = 0;

				while (buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\r' && buf[i] != SIEGECHAR_TAB && buf[i] != '{' && buf[i])
				{
					if (buf[i] == '/' && buf[i+1] == '/')
					{ //hit a comment, break out.
						break;
					}

					checkGroup[j] = buf[i];
					j++;
					i++;
				}
				checkGroup[j] = 0;

				//Make sure this is a group as opposed to a globally defined value.
				if (buf[i] == '/' && buf[i+1] == '/')
				{ //stopped on a comment, so first parse to the end of it.
                    while (buf[i] && buf[i] != '\n' && buf[i] != '\r')
					{
						i++;
					}
					while (buf[i] == '\n' || buf[i] == '\r')
					{
						i++;
					}
				}

				if (!buf[i])
				{
					Com_Error(ERR_DROP, "Unexpected EOF while looking for group '%s'", group);
				}

				isGroup = qfalse;

				while ((buf[i] && buf[i] == ' ') || buf[i] == SIEGECHAR_TAB || buf[i] == '\n' || buf[i] == '\r')
				{ //parse to the next valid character
					i++;
				}

				if (buf[i] == '{')
				{ //if the next valid character is an opening bracket, then this is indeed a group
					isGroup = qtrue;
				}

				//Is this the one we want?
				if (isGroup && !Q_stricmp(checkGroup, group))
				{ //guess so. Parse until we hit the { indicating the beginning of the group.
					while (buf[i] != '{' && buf[i])
					{
						i++;
					}

					if (buf[i])
					{ //We're at the start of the group now, so parse to the closing bracket.
						j = 0;

						parseGroups = 0;

						while ((buf[i] != '}' || parseGroups) && buf[i])
						{
							if (buf[i] == '{')
							{ //increment for the opening bracket.
								parseGroups++;
							}
							else if (buf[i] == '}')
							{ //decrement for the closing bracket
								parseGroups--;
							}

							if (parseGroups < 0)
							{ //Syntax error, I guess.
								Com_Error(ERR_DROP, "Found a closing bracket without an opening bracket while looking for group '%s'", group);
							}

							if ((buf[i] != '{' || parseGroups > 1) &&
								(buf[i] != '}' || parseGroups > 0))
							{ //don't put the start and end brackets for this group into the output buffer
								outbuf[j] = buf[i];
								j++;
							}

							if (buf[i] == '}' && !parseGroups)
							{ //Alright, we can break out now.
								break;
							}

							i++;
						}
						outbuf[j] = 0;

						//Verify that we ended up on the closing bracket.
						if (buf[i] != '}')
						{
							Com_Error(ERR_DROP, "Group '%s' is missing a closing bracket", group);
						}

						//Strip the tabs so we're friendly for value parsing.
						BG_SiegeStripTabs(outbuf);

						return 1; //we got it, so return 1.
					}
					else
					{
						Com_Error(ERR_DROP, "Error parsing group in file, unexpected EOF before opening bracket while looking for group '%s'", group);
					}
				}
				else if (!isGroup)
				{ //if it wasn't a group, parse to the end of the line
					while (buf[i] && buf[i] != '\n' && buf[i] != '\r')
					{
						i++;
					}
				}
				else
				{ //this was a group but we not the one we wanted to find, so parse by it.
					parseGroups = 0;

					while (buf[i] && (buf[i] != '}' || parseGroups))
					{
						if (buf[i] == '{')
						{
							parseGroups++;
						}
						else if (buf[i] == '}')
						{
							parseGroups--;
						}

						if (parseGroups < 0)
						{ //Syntax error, I guess.
							Com_Error(ERR_DROP, "Found a closing bracket without an opening bracket while looking for group '%s'", group);
						}

						if (buf[i] == '}' && !parseGroups)
						{ //Alright, we can break out now.
							break;
						}

						i++;
					}

					if (buf[i] != '}')
					{
						Com_Error(ERR_DROP, "Found an opening bracket without a matching closing bracket while looking for group '%s'", group);
					}

					i++;
				}
			}
		}
		else if (buf[i] == '{')
		{ //we're in a group that isn't the one we want, so parse to the end.
			parseGroups = 0;

			while (buf[i] && (buf[i] != '}' || parseGroups))
			{
				if (buf[i] == '{')
				{
					parseGroups++;
				}
				else if (buf[i] == '}')
				{
					parseGroups--;
				}

				if (parseGroups < 0)
				{ //Syntax error, I guess.
					Com_Error(ERR_DROP, "Found a closing bracket without an opening bracket while looking for group '%s'", group);
				}

				if (buf[i] == '}' && !parseGroups)
				{ //Alright, we can break out now.
					break;
				}

				i++;
			}

			if (buf[i] != '}')
			{
				Com_Error(ERR_DROP, "Found an opening bracket without a matching closing bracket while looking for group '%s'", group);
			}
		}

		if (!buf[i])
		{
			break;
		}
		i++;
	}

	return 0; //guess we never found it.
}

int BG_SiegeGetPairedValue(char *buf, char *key, char *outbuf)
{
	int i = 0;
	int j;
	int k;
	char checkKey[4096];

	while (buf[i])
	{
		if (buf[i] != ' ' && buf[i] != '{' && buf[i] != '}' && buf[i] != '\n' && buf[i] != '\r')
		{ //we're on a valid character
			if (buf[i] == '/' &&
				buf[i+1] == '/')
			{ //this is a comment, so skip over it
				while (buf[i] && buf[i] != '\n' && buf[i] != '\r')
				{
					i++;
				}
			}
			else
			{ //parse to the next space/endline/eos and check this value against our key value.
				j = 0;

				while (buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\r' && buf[i] != SIEGECHAR_TAB && buf[i])
				{
					if (buf[i] == '/' && buf[i+1] == '/')
					{ //hit a comment, break out.
						break;
					}

					checkKey[j] = buf[i];
					j++;
					i++;
				}
				checkKey[j] = 0;

				k = i;

				while (buf[k] && (buf[k] == ' ' || buf[k] == '\n' || buf[k] == '\r'))
				{
					k++;
				}

				if (buf[k] == '{')
				{ //this is not the start of a value but rather of a group. We don't want to look in subgroups so skip over the whole thing.
					int openB = 0;

					while (buf[i] && (buf[i] != '}' || openB))
					{
						if (buf[i] == '{')
						{
							openB++;
						}
						else if (buf[i] == '}')
						{
							openB--;
						}

						if (openB < 0)
						{
							Com_Error(ERR_DROP, "Unexpected closing bracket (too many) while parsing to end of group '%s'", checkKey);
						}

						if (buf[i] == '}' && !openB)
						{ //this is the end of the group
							break;
						}
						i++;
					}

					if (buf[i] == '}')
					{
						i++;
					}
				}
				else
				{
					//Is this the one we want?
					if (buf[i] != '/' || buf[i+1] != '/')
					{ //make sure we didn't stop on a comment, if we did then this is considered an error in the file.
						if (!Q_stricmp(checkKey, key))
						{ //guess so. Parse along to the next valid character, then put that into the output buffer and return 1.
							while ((buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\r' || buf[i] == SIEGECHAR_TAB) && buf[i])
							{
								i++;
							}

							if (buf[i])
							{ //We're at the start of the value now.
								qboolean parseToQuote = qfalse;

								if (buf[i] == '\"')
								{ //if the value is in quotes, then stop at the next quote instead of ' '
									i++;
									parseToQuote = qtrue;
								}

								j = 0;
								while ( ((!parseToQuote && buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\r') || (parseToQuote && buf[i] != '\"')) )
								{
									if (buf[i] == '/' &&
										buf[i+1] == '/')
									{ //hit a comment after the value? This isn't an ideal way to be writing things, but we'll support it anyway.
										break;
									}
									outbuf[j] = buf[i];
									j++;
									i++;

									if (!buf[i])
									{
										if (parseToQuote)
										{
											Com_Error(ERR_DROP, "Unexpected EOF while looking for endquote, error finding paired value for '%s'", key);
										}
										else
										{
											Com_Error(ERR_DROP, "Unexpected EOF while looking for space or endline, error finding paired value for '%s'", key);
										}
									}
								}
								outbuf[j] = 0;

								return 1; //we got it, so return 1.
							}
							else
							{
								Com_Error(ERR_DROP, "Error parsing file, unexpected EOF while looking for valud '%s'", key);
							}
						}
						else
						{ //if that wasn't the desired key, then make sure we parse to the end of the line, so we don't mistake a value for a key
							while (buf[i] && buf[i] != '\n')
							{
								i++;
							}
						}
					}
					else
					{
						Com_Error(ERR_DROP, "Error parsing file, found comment, expected value for '%s'", key);
					}
				}
			}
		}

		if (!buf[i])
		{
			break;
		}
		i++;
	}

	return 0; //guess we never found it.
}
//======================================
//End parsing functions
//======================================


//======================================
//Class loading functions
//======================================
void BG_SiegeTranslateForcePowers(char *buf, siegeClass_t *siegeClass)
{
	char checkPower[1024];
	char checkLevel[256];
	int l = 0;
	int k = 0;
	int j = 0;
	int i = 0;
	int parsedLevel = 0;
	qboolean allPowers = qfalse;
	qboolean noPowers = qfalse;

	if (!Q_stricmp(buf, "FP_ALL"))
	{ //this is a special case, just give us all the powers on level 3
		allPowers = qtrue;
	}

	if (buf[0] == '0' && !buf[1])
	{ //no powers then
		noPowers = qtrue;
	}

	//First clear out the powers, or in the allPowers case, give us all level 3.
	while (i < NUM_FORCE_POWERS)
	{
		if (allPowers)
		{
			siegeClass->forcePowerLevels[i] = FORCE_LEVEL_3;
		}
		else
		{
			siegeClass->forcePowerLevels[i] = 0;
		}
		i++;
	}

	if (allPowers || noPowers)
	{ //we're done now then.
		return;
	}

	i = 0;
	while (buf[i])
	{ //parse through the list which is seperated by |, and add all the weapons into a bitflag
		if (buf[i] != ' ' && buf[i] != '|')
		{
			j = 0;

			while (buf[i] && buf[i] != ' ' && buf[i] != '|' && buf[i] != ',')
			{
				checkPower[j] = buf[i];
				j++;
				i++;
			}
			checkPower[j] = 0;

			if (buf[i] == ',')
			{ //parse the power level
				i++;
				l = 0;
				while (buf[i] && buf[i] != ' ' && buf[i] != '|')
				{
					checkLevel[l] = buf[i];
					l++;
					i++;
				}
				checkLevel[l] = 0;
				parsedLevel = atoi(checkLevel);

				//keep sane limits on the powers
				if (parsedLevel < 0)
				{
					parsedLevel = 0;
				}
				if (parsedLevel > FORCE_LEVEL_5)
				{
					parsedLevel = FORCE_LEVEL_5;
				}
			}
			else
			{ //if it's not there, assume level 3 I guess.
				parsedLevel = 3;
			}

			if (checkPower[0])
			{ //Got the name, compare it against the weapon table strings.
				k = 0;

				if (!Q_stricmp(checkPower, "FP_JUMP"))
				{ //haqery
                    strcpy(checkPower, "FP_LEVITATION");
				}

				while (FPTable[k].id != -1 && FPTable[k].name[0])
				{
					if (!Q_stricmp(checkPower, FPTable[k].name))
					{ //found it, add the weapon into the weapons value
						siegeClass->forcePowerLevels[k] = parsedLevel;
						break;
					}
					k++;
				}
			}
		}

		if (!buf[i])
		{
			break;
		}
		i++;
	}
}

//Used for the majority of generic val parsing stuff. buf should be the value string,
//table should be the appropriate string/id table. If bitflag is qtrue then the
//values are accumulated into a bitflag. If bitflag is qfalse then the first value
//is returned as a directly corresponding id and no further parsing is done.
int BG_SiegeTranslateGenericTable(char *buf, stringID_table_t *table, qboolean bitflag)
{
	int items = 0;
	char checkItem[1024];
	int i = 0;
	int j = 0;
	int k = 0;

	if (buf[0] == '0' && !buf[1])
	{ //special case, no items.
		return 0;
	}

	while (buf[i])
	{ //Using basically the same parsing method as we do for weapons and forcepowers.
		if (buf[i] != ' ' && buf[i] != '|')
		{
			j = 0;

			while (buf[i] && buf[i] != ' ' && buf[i] != '|')
			{
				checkItem[j] = buf[i];
				j++;
				i++;
			}
			checkItem[j] = 0;

			if (checkItem[0])
			{
				k = 0;

                while (table[k].name && table[k].name[0])
				{ //go through the list and check the parsed flag name against the hardcoded names
					if (!Q_stricmp(checkItem, table[k].name))
					{ //Got it, so add the value into our items value.
						if (bitflag)
						{
							items |= (1 << table[k].id);
						}
						else
						{ //return the value directly then.
							return table[k].id;
						}
						break;
					}
					k++;
				}
			}
		}

		if (!buf[i])
		{
			break;
		}

		i++;
	}
	return items;
}

// duo: same as above but with 64-bit support
long long BG_SiegeTranslateGenericTable64(char *buf, stringID_table_t *table, qboolean bitflag)
{
	long long items = 0ll;
	char checkItem[1024];
	long long i = 0ll;
	long long j = 0ll;
	long long k = 0ll;

	if (buf[0] == '0' && !buf[1])
	{ //special case, no items.
		return 0;
	}

	while (buf[i])
	{ //Using basically the same parsing method as we do for weapons and forcepowers.
		if (buf[i] != ' ' && buf[i] != '|')
		{
			j = 0ll;

			while (buf[i] && buf[i] != ' ' && buf[i] != '|')
			{
				checkItem[j] = buf[i];
				j++;
				i++;
			}
			checkItem[j] = 0ll;

			if (checkItem[0])
			{
				k = 0ll;

				while (table[k].name && table[k].name[0])
				{ //go through the list and check the parsed flag name against the hardcoded names
					if (!Q_stricmp(checkItem, table[k].name))
					{ //Got it, so add the value into our items value.
						if (bitflag)
						{
							items |= (1ll << (long long)table[k].id);
						}
						else
						{ //return the value directly then.
							return (long long)table[k].id;
						}
						break;
					}
					k++;
				}
			}
		}

		if (!buf[i])
		{
			break;
		}

		i++;
	}
	return items;
}

char *classTitles[SPC_MAX] =
{
"infantry",			// SPC_INFANTRY
"vanguard",			// SPC_VANGUARD
"support",			// SPC_SUPPORT
"jedi_general",		// SPC_JEDI
"demolitionist",	// SPC_DEMOLITIONIST
"heavy_weapons",	// SPC_HEAVY_WEAPONS
};

typedef enum {
	SIEGEMAP_UNKNOWN = 0,
	SIEGEMAP_HOTH,
	SIEGEMAP_DESERT,
	SIEGEMAP_KORRIBAN,
	SIEGEMAP_NAR,
	SIEGEMAP_CARGO,
	SIEGEMAP_URBAN,
	SIEGEMAP_BESPIN,
	SIEGEMAP_ANSION
} siegeMap_t;
extern siegeMap_t GetSiegeMap(void);

extern vmCvar_t g_hothRebalance;
void BG_SiegeParseClassFile(const char *filename, siegeClassDesc_t *descBuffer)
{
	fileHandle_t f;
	int len;
	int i;
	char classInfo[4096];
	char parseBuf[4096];

	len = trap_FS_FOpenFile(filename, &f, FS_READ);

	if (!f || len >= 4096)
	{
		return;
	}

	trap_FS_Read(classInfo, len, f);

	trap_FS_FCloseFile(f);

	classInfo[len] = 0;

#if 0
	//first get the description if we have a buffer for it
	if (descBuffer)
	{
		if (!BG_SiegeGetPairedValue(classInfo, "description", descBuffer->desc))
		{
			strcpy(descBuffer->desc, "DESCRIPTION UNAVAILABLE");
		}

		//Hit this assert?  Memory has already been trashed.  Increase
		//SIEGE_CLASS_DESC_LEN.
		assert(strlen(descBuffer->desc) < SIEGE_CLASS_DESC_LEN);
	}
#endif

	siegeClass_t *scl = &bgSiegeClasses[bgNumSiegeClasses];
	// duo: added to send to clients
	if (BG_SiegeGetPairedValue(classInfo, "description", parseBuf));
		Q_strncpyz(scl->description, parseBuf, sizeof(scl->description));

	BG_SiegeGetValueGroup(classInfo, "ClassInfo", classInfo);

	//Parse name
	if (BG_SiegeGetPairedValue(classInfo, "name", parseBuf))
	{
		strcpy(scl->name, parseBuf);
	}
	else
	{
		Com_Error(ERR_DROP, "Siege class without name entry");
	}

	//Parse forced model
	if (BG_SiegeGetPairedValue(classInfo, "model", parseBuf))
	{
		strcpy(scl->forcedModel, parseBuf);
	}
	else
	{ //It's ok if there isn't one, it's optional.
		scl->forcedModel[0] = 0;
	}

	//Parse forced skin
	if (BG_SiegeGetPairedValue(classInfo, "skin", parseBuf))
	{
		strcpy(scl->forcedSkin, parseBuf);
	}
	else
	{ //It's ok if there isn't one, it's optional.
	  // duo: set to "default" if none found
		Q_strncpyz(scl->forcedSkin, "default", sizeof(scl->forcedSkin));
		//bgSiegeClasses[bgNumSiegeClasses].forcedSkin[0] = 0;
	}

	//Parse first saber
	if (BG_SiegeGetPairedValue(classInfo, "saber1", parseBuf))
	{
		strcpy(scl->saber1, parseBuf);
	}
	else
	{ //It's ok if there isn't one, it's optional.
		scl->saber1[0] = 0;
	}

	//Parse second saber
	if (BG_SiegeGetPairedValue(classInfo, "saber2", parseBuf))
	{
		strcpy(scl->saber2, parseBuf);
	}
	else
	{ //It's ok if there isn't one, it's optional.
		scl->saber2[0] = 0;
	}

	//Parse forced saber stance
	if (BG_SiegeGetPairedValue(classInfo, "saberstyle", parseBuf))
	{
		scl->saberStance = BG_SiegeTranslateGenericTable(parseBuf, StanceTable, qtrue);
	}
	else
	{ //It's ok if there isn't one, it's optional.
		scl->saberStance = 0;
	}

	//Parse forced saber color
	if (BG_SiegeGetPairedValue(classInfo, "sabercolor", parseBuf))
	{
		scl->forcedSaberColor = atoi(parseBuf);
		scl->hasForcedSaberColor = qtrue;
	}
	else
	{ //It's ok if there isn't one, it's optional.
		scl->hasForcedSaberColor = qfalse;
	}

	//Parse forced saber2 color
	if (BG_SiegeGetPairedValue(classInfo, "saber2color", parseBuf))
	{
		scl->forcedSaber2Color = atoi(parseBuf);
		scl->hasForcedSaber2Color = qtrue;
	}
	else
	{ //It's ok if there isn't one, it's optional.
		scl->hasForcedSaber2Color = qfalse;
	}

	//Parse weapons
	if (BG_SiegeGetPairedValue(classInfo, "weapons", parseBuf))
	{
		scl->weapons = BG_SiegeTranslateGenericTable(parseBuf, WPTable, qtrue);
	}
	else
	{
		Com_Error(ERR_DROP, "Siege class without weapons entry");
	}

	// hoth rebalancing
	if (!strcmp(scl->name, "Rocket Trooper") && g_hothRebalance.integer & (1 << 1))
		scl->weapons |= (1 << WP_BLASTER);

	if (!(scl->weapons & (1 << WP_SABER)))
	{ //make sure it has melee if there's no saber
		scl->weapons |= (1 << WP_MELEE);

		//always give them this too if they are not a saber user
	}

	//Parse forcepowers
	if (BG_SiegeGetPairedValue(classInfo, "forcepowers", parseBuf))
	{
		BG_SiegeTranslateForcePowers(parseBuf, scl);
	}
	else
	{ //fine, clear out the powers.
		i = 0;
		while (i < NUM_FORCE_POWERS)
		{
			scl->forcePowerLevels[i] = 0;
			i++;
		}
	}

	// hoth rebalancing
	if (!strcmp(scl->name, "Jedi Guardian") && g_hothRebalance.integer & (1 << 2))
		scl->forcePowerLevels[FP_HEAL] = 3;

	//Parse classflags
	if (BG_SiegeGetPairedValue(classInfo, "classflags", parseBuf))
	{
		scl->classflags = BG_SiegeTranslateGenericTable(parseBuf, bgSiegeClassFlagNames, qtrue);
	}
	else
	{ //fine, we'll 0 it.
		scl->classflags = 0;
	}

	if (BG_SiegeGetPairedValue(classInfo, "audiomindtrick", parseBuf))
		scl->audioMindTrick = atoi(parseBuf) ? qtrue : qfalse;
	else
		scl->audioMindTrick = qfalse;

	if (BG_SiegeGetPairedValue(classInfo, "saberoffdamageboost", parseBuf))
		scl->saberOffDamageBoost = atoi(parseBuf) ? qtrue : qfalse;
	else
		scl->saberOffDamageBoost = qfalse;

	if (BG_SiegeGetPairedValue(classInfo, "saberoffextradamage", parseBuf))
		scl->saberOffExtraDamage = atof(parseBuf);
	else
		scl->saberOffExtraDamage = 0;

	if (BG_SiegeGetPairedValue(classInfo, "shortburstjetpack", parseBuf))
		scl->shortBurstJetpack = atoi(parseBuf) ? qtrue : qfalse;
	else
		scl->shortBurstJetpack = qfalse;

	if (BG_SiegeGetPairedValue(classInfo, "chargingdempremovesspawnshield", parseBuf))
		scl->chargingDempRemovesSpawnShield = atoi(parseBuf) ? qtrue : qfalse;
	else
		scl->chargingDempRemovesSpawnShield = qfalse;

	if (BG_SiegeGetPairedValue(classInfo, "pull1IfSaberInAir", parseBuf))
		scl->pull1IfSaberInAir = atoi(parseBuf) ? qtrue : qfalse;
	else
		scl->pull1IfSaberInAir = qfalse;

	if (BG_SiegeGetPairedValue(classInfo, "detkilldelay", parseBuf))
		scl->detKillDelay = atoi(parseBuf);
	else
		scl->detKillDelay = 0;

	//Parse maxhealth
	if (BG_SiegeGetPairedValue(classInfo, "maxhealth", parseBuf))
	{
		scl->maxhealth = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 100 then.
		scl->maxhealth = 100;
	}

	//Parse starthealth
	if (BG_SiegeGetPairedValue(classInfo, "starthealth", parseBuf))
	{
		scl->starthealth = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 100 then.
		scl->starthealth = scl->maxhealth;
	}

	//Parse ammoblaster
	if (BG_SiegeGetPairedValue(classInfo, "ammoblaster", parseBuf))
	{
		scl->ammoblaster = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->ammoblaster = 0;
	}

	//Parse ammopowercell
	if (BG_SiegeGetPairedValue(classInfo, "ammopowercell", parseBuf))
	{
		scl->ammopowercell = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->ammopowercell = 0;
	}

	//Parse ammometallicbolts
	if (BG_SiegeGetPairedValue(classInfo, "ammometallicbolts", parseBuf))
	{
		scl->ammometallicbolts = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->ammometallicbolts = 0;
	}

	//Parse ammorockets
	if (BG_SiegeGetPairedValue(classInfo, "ammorockets", parseBuf))
	{
		scl->ammorockets = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->ammorockets = 0;
	}

	//Parse ammothermals
	if (BG_SiegeGetPairedValue(classInfo, "ammothermals", parseBuf))
	{
		scl->ammothermals = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->ammothermals = 0;
	}

	//Parse ammotripmines
	if (BG_SiegeGetPairedValue(classInfo, "ammotripmines", parseBuf))
	{
		scl->ammotripmines = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->ammotripmines = 0;
	}

	//Parse ammodetpacks
	if (BG_SiegeGetPairedValue(classInfo, "ammodetpacks", parseBuf))
	{
		scl->ammodetpacks = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->ammodetpacks = 0;
	}

	//Parse dispensehealthpaks
	if (BG_SiegeGetPairedValue(classInfo, "dispensehealthpaks", parseBuf))
	{
		scl->dispenseHealthpaks = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->dispenseHealthpaks = 0;
	}

	if (BG_SiegeGetPairedValue(classInfo, "jetpackfreezeimmunity", parseBuf))
	{
		scl->jetpackFreezeImmunity = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->jetpackFreezeImmunity = 0;
	}

	//Parse maxsentries
	if (BG_SiegeGetPairedValue(classInfo, "maxsentries", parseBuf))
	{
		scl->maxSentries = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->maxSentries = 0;
	}
	
	memset(&scl->incomingDamageParam, 0, sizeof(scl->incomingDamageParam));
	memset(&scl->outgoingDamageParam, 0, sizeof(scl->outgoingDamageParam));
	for (i = 0; i < MAX_SPECIALDAMAGEPARAMETERS; i++) {
		//Parse special dmg params
		specialDamageParam_t *sdp = &scl->incomingDamageParam[i];
		if (!BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_mods", i + 1), parseBuf)) {
			memset(sdp, 0, sizeof(specialDamageParam_t));
			continue;
		}
		if (stristr(parseBuf, "-1"))
			sdp->mods = 0xFFFFFFFFFFFFFFFFll;
		else
			sdp->mods = BG_SiegeTranslateGenericTable64(parseBuf, ModTable, qtrue);

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_otherenttype", i + 1), parseBuf))
			sdp->otherEntType = BG_SiegeTranslateGenericTable(parseBuf, OtherEntTypeTable, qtrue);
		else
			sdp->otherEntType = -1;

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_minDmg", i + 1), parseBuf))
			sdp->damageMin = atoi(parseBuf);
		else
			sdp->damageMin = 0x80000000;

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_maxDmg", i + 1), parseBuf))
			sdp->damageMax = atoi(parseBuf);
		else
			sdp->damageMax = 0x7FFFFFFF;

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_dmgMult", i + 1), parseBuf))
			sdp->damageMultiplier = atof(parseBuf);
		else
			sdp->damageMultiplier = 1.0f;

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_knockbackMult", i + 1), parseBuf))
			sdp->knockbackMultiplier = atof(parseBuf);
		else
			sdp->knockbackMultiplier = 1.0f;

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_negativedmgok", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->negativeDamageOk = qtrue;
			else
				sdp->negativeDamageOk = qfalse;
		}
		else {
			sdp->negativeDamageOk = qfalse;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_freeze", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->freeze = FREEZE_YES;
			else if (stristr(parseBuf, "no") || *parseBuf == '0')
				sdp->freeze = FREEZE_NO;
			else
				sdp->freeze = FREEZE_DEFAULT;
		}
		else {
			sdp->freeze = FREEZE_DEFAULT;
		}

		if (sdp->freeze == FREEZE_YES) {
			if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_freezemin", i + 1), parseBuf))
				sdp->freezeMin = atoi(parseBuf);
			else
				sdp->freezeMin = -1;

			if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_freezemax", i + 1), parseBuf))
				sdp->freezeMax = atoi(parseBuf);
			else
				sdp->freezeMax = -1;

			if (sdp->freezeMin < 0 || sdp->freezeMax <= 0 || sdp->freezeMin > sdp->freezeMax)
				sdp->freezeMin = sdp->freezeMax = -1;
		}
		else {
			sdp->freezeMin = sdp->freezeMax = -1;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_onlyknockback", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->onlyKnockback = qtrue;
			else
				sdp->onlyKnockback = qfalse;
		}
		else {
			sdp->onlyKnockback = qfalse;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_jedisplashdmgreduction", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_YES;
			else if (stristr(parseBuf, "no") || *parseBuf == '0')
				sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_NO;
			else
				sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_DEFAULT;
		}
		else {
			sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_DEFAULT;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("incomingdmg%d_nonjedisaberdmgincrease", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_YES;
			else if (stristr(parseBuf, "no") || *parseBuf == '0')
				sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_NO;
			else
				sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_DEFAULT;
		}
		else {
			sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_DEFAULT;
		}

#ifdef _DEBUG
		Com_Printf("Parsed incoming dmg parm for %s with mods %llu, otherEntType %d, minDmg %d, maxDmg %d, dmgMult %.3f, knockbackMult %.3f, freeze %d (%d to %d), negativeDmgOk %d, onlyKnockback %d, jedi splash dmg reduction %d, non-jedi saber dmg increase %d\n",
			scl->name, sdp->mods, sdp->otherEntType, sdp->damageMin, sdp->damageMax, sdp->damageMultiplier, sdp->knockbackMultiplier, sdp->freeze, sdp->freezeMin, sdp->freezeMax, sdp->negativeDamageOk, sdp->onlyKnockback, sdp->jediSplashDamageReduction, sdp->nonJediSaberDamageIncrease);
#endif
	}

	for (i = 0; i < MAX_SPECIALDAMAGEPARAMETERS; i++) {
		//Parse special dmg params
		specialDamageParam_t *sdp = &scl->outgoingDamageParam[i];
		if (!BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_mods", i + 1), parseBuf)) {
			memset(sdp, 0, sizeof(specialDamageParam_t));
			continue;
		}
		if (stristr(parseBuf, "-1"))
			sdp->mods = 0xFFFFFFFFFFFFFFFFll;
		else
			sdp->mods = BG_SiegeTranslateGenericTable64(parseBuf, ModTable, qtrue);

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_otherenttype", i + 1), parseBuf))
			sdp->otherEntType = BG_SiegeTranslateGenericTable(parseBuf, OtherEntTypeTable, qtrue);
		else
			sdp->otherEntType = -1;

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_minDmg", i + 1), parseBuf))
			sdp->damageMin = atoi(parseBuf);
		else
			sdp->damageMin = 0x80000000;

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_maxDmg", i + 1), parseBuf))
			sdp->damageMax = atoi(parseBuf);
		else
			sdp->damageMax = 0x7FFFFFFF;

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_dmgMult", i + 1), parseBuf))
			sdp->damageMultiplier = atof(parseBuf);
		else
			sdp->damageMultiplier = 1.0f;

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_knockbackMult", i + 1), parseBuf))
			sdp->knockbackMultiplier = atof(parseBuf);
		else
			sdp->knockbackMultiplier = sdp->damageMultiplier;

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_negativedmgok", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->negativeDamageOk = qtrue;
			else
				sdp->negativeDamageOk = qfalse;
		}
		else {
			sdp->negativeDamageOk = qfalse;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_freeze", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->freeze = FREEZE_YES;
			else if (stristr(parseBuf, "no") || *parseBuf == '2')
				sdp->freeze = FREEZE_NO;
			else
				sdp->freeze = FREEZE_DEFAULT;
		}
		else {
			sdp->freeze = FREEZE_DEFAULT;
		}

		if (sdp->freeze == FREEZE_YES) {
			if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_freezemin", i + 1), parseBuf))
				sdp->freezeMin = atoi(parseBuf);
			else
				sdp->freezeMin = -1;

			if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_freezemax", i + 1), parseBuf))
				sdp->freezeMax = atoi(parseBuf);
			else
				sdp->freezeMax = -1;

			if (sdp->freezeMin < 0 || sdp->freezeMax <= 0 || sdp->freezeMin > sdp->freezeMax)
				sdp->freezeMin = sdp->freezeMax = -1;
		}
		else {
			sdp->freezeMin = sdp->freezeMax = -1;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_onlyknockback", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->onlyKnockback = qtrue;
			else
				sdp->onlyKnockback = qfalse;
		}
		else {
			sdp->onlyKnockback = qfalse;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_jedisplashdmgreduction", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_YES;
			else if (stristr(parseBuf, "no") || *parseBuf == '0')
				sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_NO;
			else
				sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_DEFAULT;
		}
		else {
			sdp->jediSplashDamageReduction = JEDISPLASHDMGREDUCTION_DEFAULT;
		}

		if (BG_SiegeGetPairedValue(classInfo, va("outgoingdmg%d_nonjedisaberdmgincrease", i + 1), parseBuf)) {
			if (stristr(parseBuf, "yes") || *parseBuf == '1')
				sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_YES;
			else if (stristr(parseBuf, "no") || *parseBuf == '0')
				sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_NO;
			else
				sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_DEFAULT;
		}
		else {
			sdp->nonJediSaberDamageIncrease = NONJEDISABERDMGINCREASE_DEFAULT;
		}

#ifdef _DEBUG
		Com_Printf("Parsed outgoing dmg parm for %s with mods %llu, otherEntType %d, minDmg %d, maxDmg %d, dmgMult %.3f, knockbackMult %.3f, freeze %d (%d to %d), negativeDmgOk %d, onlyKnockback %d, jedi splash dmg reduction %d, non-jedi saber dmg increase %d\n",
			scl->name, sdp->mods, sdp->otherEntType, sdp->damageMin, sdp->damageMax, sdp->damageMultiplier, sdp->knockbackMultiplier, sdp->freeze, sdp->freezeMin, sdp->freezeMax, sdp->negativeDamageOk, sdp->onlyKnockback, sdp->jediSplashDamageReduction, sdp->nonJediSaberDamageIncrease);
#endif
	}

	//Parse startarmor
	if (BG_SiegeGetPairedValue(classInfo, "maxarmor", parseBuf))
	{
		scl->maxarmor = atoi(parseBuf);
	}
	else
	{ //It's alright, just default to 0 then.
		scl->maxarmor = 0;
	}

	//Parse startarmor
	if (BG_SiegeGetPairedValue(classInfo, "startarmor", parseBuf))
	{
		scl->startarmor = atoi(parseBuf);
		if (!scl->maxarmor)
		{ //if they didn't specify a damn max armor then use this.
			scl->maxarmor = scl->startarmor;
		}
	}
	else
	{ //default to maxarmor.
		scl->startarmor = scl->maxarmor;
	}

	//Parse speed (this is a multiplier value)
	if (BG_SiegeGetPairedValue(classInfo, "speed", parseBuf))
	{
		scl->speed = atof(parseBuf);
	}
	else
	{ //It's alright, just default to 1 then.
		scl->speed = 1.0f;
	}

	//Parse shader for ui to use
	if (BG_SiegeGetPairedValue(classInfo, "uishader", parseBuf))
	{
#ifdef QAGAME
		scl->uiPortraitShader = 0;
		//memset(bgSiegeClasses[bgNumSiegeClasses].uiPortrait,0,sizeof(bgSiegeClasses[bgNumSiegeClasses].uiPortrait));
		Q_strncpyz(scl->uiPortrait, parseBuf, sizeof(scl->uiPortrait)); // duo: added to send to clients
#elif defined CGAME
		scl->uiPortraitShader = 0;
		memset(scl->uiPortrait,0,sizeof(scl->uiPortrait));
#else //ui
		scl->uiPortraitShader = trap_R_RegisterShaderNoMip(parseBuf);
		memcpy(scl->uiPortrait,parseBuf,sizeof(scl->uiPortrait));
#endif
	}
	else
	{ //I guess this is an essential.. we don't want to render bad shaders or anything.
		Com_Error(ERR_DROP, "Siege class without uishader entry");
	}

	//Parse shader for ui to use
	if (BG_SiegeGetPairedValue(classInfo, "class_shader", parseBuf))
	{
#ifdef QAGAME
		scl->classShader = 0;
		Q_strncpyz(scl->classShaderBuf, parseBuf, sizeof(scl->classShaderBuf)); // duo: added to send to clients
#else //cgame, ui
		scl->classShader = trap_R_RegisterShaderNoMip(parseBuf);
		assert( scl->classShader );
		if ( !scl->classShader )
		{
			Com_Printf( "ERROR: could not find class_shader %s for class %s\n", parseBuf, scl->name );
		}
		// A very hacky way to determine class . . . 
		else
#endif
		{
			// Find the base player class based on the icon name - very bad, I know.
			int titleLength,i,arrayTitleLength;
			char *holdBuf;

			titleLength = strlen(parseBuf);
			for (i=0;i<SPC_MAX;i++)
			{
				// Back up 
				arrayTitleLength = strlen(classTitles[i]);
				if (arrayTitleLength>titleLength)	// Too long
				{
					break;
				}

				holdBuf = parseBuf + ( titleLength - arrayTitleLength);
				if (!strcmp(holdBuf,classTitles[i]))
				{
					scl->playerClass = i;
					break;
				}
			}

			// In case the icon name doesn't match up
			if (i>=SPC_MAX)
			{
				scl->playerClass = SPC_INFANTRY;
			}
		}
	}
	else
	{ //No entry!  Bad bad bad
		Com_Printf( "ERROR: no class_shader defined for class %s\n", scl->name );
	}

	//Parse holdable items to use
	if (BG_SiegeGetPairedValue(classInfo, "holdables", parseBuf))
	{
		scl->invenItems = BG_SiegeTranslateGenericTable(parseBuf, HoldableTable, qtrue);
	}
	else
	{ //Just don't start out with any then.
		scl->invenItems = 0;
	}

	// hoth rebalancing
	if (!strcmp(scl->name, "Imperial Snowtrooper") && g_hothRebalance.integer & (1 << 0))
		scl->invenItems |= (1 << HI_MEDPAC_BIG);
	else if (!strcmp(scl->name, "Rocket Trooper") && g_hothRebalance.integer & (1 << 1))
		scl->invenItems |= (1 << HI_MEDPAC);

	//Parse powerups to use
	if (BG_SiegeGetPairedValue(classInfo, "powerups", parseBuf))
	{
		scl->powerups = BG_SiegeTranslateGenericTable(parseBuf, PowerupTable, qtrue);
	}
	else
	{ //Just don't start out with any then.
		scl->powerups = 0;
	}

	//A successful read.
	bgNumSiegeClasses++;
}

// Count the number of like base classes
int BG_SiegeCountBaseClass(const int team, const short classIndex)
{
	int count = 0,i;
	siegeTeam_t *stm;

	stm = BG_SiegeFindThemeForTeam(team);
	if (!stm)
	{
		return(0);

	}

	for (i=0;i<stm->numClasses;i++)
	{

		if (stm->classes[i]->playerClass == classIndex)
		{
			count++;
		}
	}
	return(count);
}

char *BG_GetUIPortraitFile(const int team, const short classIndex, const short cntIndex)
{
	int count = 0,i;
	siegeTeam_t *stm;

	stm = BG_SiegeFindThemeForTeam(team);
	if (!stm)
	{
		return(0);

	}

	// Loop through all the classes for this team
	for (i=0;i<stm->numClasses;i++)
	{
		// does it match the base class?
		if (stm->classes[i]->playerClass == classIndex)
		{
			if (count==cntIndex)
			{
				return(stm->classes[i]->uiPortrait);
			}
			++count;
		}
	}

	return(0);
}

int BG_GetUIPortrait(const int team, const short classIndex, const short cntIndex)
{
	int count = 0,i;
	siegeTeam_t *stm;

	stm = BG_SiegeFindThemeForTeam(team);
	if (!stm)
	{
		return(0);

	}

	// Loop through all the classes for this team
	for (i=0;i<stm->numClasses;i++)
	{
		// does it match the base class?
		if (stm->classes[i]->playerClass == classIndex)
		{
			if (count==cntIndex)
			{
				return(stm->classes[i]->uiPortraitShader);
			}
			++count;
		}
	}

	return(0);
}

// This is really getting ugly - looking to get the base class (within a class) based on the index passed in
siegeClass_t *BG_GetClassOnBaseClass(const int team, const short classIndex, const short cntIndex)
{
	int count = 0,i;
	siegeTeam_t *stm;

	stm = BG_SiegeFindThemeForTeam(team);
	if (!stm)
	{
		return(0);
	}

	// Loop through all the classes for this team
	for (i=0;i<stm->numClasses;i++)
	{
		// does it match the base class?
		if (stm->classes[i]->playerClass == classIndex)
		{
			if (count==cntIndex)
			{
				return(stm->classes[i]);
			}
			++count;
		}
	}

	return(0);
}

void BG_SiegeLoadClasses(siegeClassDesc_t *descBuffer)
{
	int numFiles;
	int filelen;
	char filelist[MAX_SIEGE_CLASS_FILELIST];
	char filename[MAX_QPATH];
	char* fileptr;
	int i;

	bgNumSiegeClasses = 0;

	numFiles = trap_FS_GetFileList("ext_data/Siege/Classes", ".scl", filelist, MAX_SIEGE_CLASS_FILELIST );
	fileptr = filelist;

	for (i = 0; i < numFiles; i++, fileptr += filelen+1)
	{
		filelen = strlen(fileptr);
		strcpy(filename, "ext_data/Siege/Classes/");
		strcat(filename, fileptr);

		if (descBuffer)
		{
			BG_SiegeParseClassFile(filename, &descBuffer[i]);
		}
		else
		{
			BG_SiegeParseClassFile(filename, NULL);
		}
	}
}
//======================================
//End class loading functions
//======================================


//======================================
//Team loading functions
//======================================
siegeClass_t *BG_SiegeFindClassByName(const char *classname)
{
	int i = 0;

	while (i < bgNumSiegeClasses)
	{
		if (!Q_stricmp(bgSiegeClasses[i].name, classname))
		{ //found it
			return &bgSiegeClasses[i];
		}
		i++;
	}

	return NULL;
}

siegeClass_t* BG_SiegeGetClass(int team, stupidSiegeClassNum_t classNumber)
{
	siegeTeam_t* siegeTeam = 0;
	if (team == SIEGETEAM_TEAM1)
	{
		siegeTeam = team1Theme;
	}
	else if (team == SIEGETEAM_TEAM2)
	{
		siegeTeam = team2Theme;
	}
	else
	{
		// spec, ignore
		return 0;
	}

	if (!siegeTeam)
	{
		return 0;
	}

	if ( (classNumber < 1) || (classNumber > siegeTeam->numClasses) )
	{
		return 0;
	}

	return siegeTeam->classes[classNumber - 1];
}

void BG_SiegeParseTeamFile(const char *filename)
{
	fileHandle_t f;
	int len;
	char teamInfo[2048];
	char parseBuf[1024];
	char lookString[256];
	int i = 1;
	qboolean success = qtrue;

	len = trap_FS_FOpenFile(filename, &f, FS_READ);

	if (!f || len >= 2048)
	{
		return;
	}

	trap_FS_Read(teamInfo, len, f);

	trap_FS_FCloseFile(f);

	teamInfo[len] = 0;

	if (BG_SiegeGetPairedValue(teamInfo, "name", parseBuf))
	{
		strcpy(bgSiegeTeams[bgNumSiegeTeams].name, parseBuf);
	}
	else
	{
		Com_Error(ERR_DROP, "Siege team with no name definition");
	}

//I don't entirely like doing things this way but it's the easiest way.
#ifdef CGAME
	if (BG_SiegeGetPairedValue(teamInfo, "FriendlyShader", parseBuf))
	{
		bgSiegeTeams[bgNumSiegeTeams].friendlyShader = trap_R_RegisterShaderNoMip(parseBuf);
	}
#else
	bgSiegeTeams[bgNumSiegeTeams].friendlyShader = 0;
#endif

	bgSiegeTeams[bgNumSiegeTeams].numClasses = 0;

	if (BG_SiegeGetValueGroup(teamInfo, "Classes", teamInfo))
	{
		while (success && i < MAX_SIEGE_CLASSES)
		{ //keep checking for group values named class# up to MAX_SIEGE_CLASSES until we can't find one.
			strcpy(lookString, va("class%i", i));

			success = BG_SiegeGetPairedValue(teamInfo, lookString, parseBuf);

			if (!success)
			{
				break;
			}

			bgSiegeTeams[bgNumSiegeTeams].classes[bgSiegeTeams[bgNumSiegeTeams].numClasses] = BG_SiegeFindClassByName(parseBuf);

			if (!bgSiegeTeams[bgNumSiegeTeams].classes[bgSiegeTeams[bgNumSiegeTeams].numClasses])
			{
				Com_Error(ERR_DROP, "Invalid class specified: '%s'", parseBuf);
			}

			bgSiegeTeams[bgNumSiegeTeams].numClasses++;

			i++;
		}
	}

	if (!bgSiegeTeams[bgNumSiegeTeams].numClasses)
	{
		Com_Error(ERR_DROP, "Team defined with no allowable classes\n");
	}

	//If we get here then it was a success, so increment the team number
	bgNumSiegeTeams++;
}

void BG_SiegeLoadTeams(void)
{
	int numFiles;
	int filelen;
	char filelist[MAX_SIEGE_TEAM_FILELIST];
	char filename[MAX_QPATH];
	char* fileptr;
	int i;

	bgNumSiegeTeams = 0;

	numFiles = trap_FS_GetFileList("ext_data/Siege/Teams", ".team", filelist, MAX_SIEGE_TEAM_FILELIST );
	fileptr = filelist;

	for (i = 0; i < numFiles; i++, fileptr += filelen+1)
	{
		filelen = strlen(fileptr);
		strcpy(filename, "ext_data/Siege/Teams/");
		strcat(filename, fileptr);
		BG_SiegeParseTeamFile(filename);
	}
}
//======================================
//End team loading functions
//======================================


//======================================
//Misc/utility functions
//======================================
siegeTeam_t *BG_SiegeFindThemeForTeam(int team)
{
	if (team == SIEGETEAM_TEAM1)
	{
		return team1Theme;
	}
	else if (team == SIEGETEAM_TEAM2)
	{
		return team2Theme;
	}

    return NULL;
}

#ifndef UI_EXPORTS //only for game/cgame
//precache all the sabers for the active classes for the team
extern qboolean WP_SaberParseParms( const char *SaberName, saberInfo_t *saber ); //bg_saberLoad.cpp
extern int BG_ModelCache(const char *modelName, const char *skinName); //bg_misc.c

void BG_PrecacheSabersForSiegeTeam(int team)
{
	siegeTeam_t *t;
	saberInfo_t saber;
	char *saberName;
	int sNum;

	t = BG_SiegeFindThemeForTeam(team);

	if (t)
	{
		int i = 0;

		while (i < t->numClasses)
		{
			sNum = 0;

			while (sNum < MAX_SABERS)
			{
				switch (sNum)
				{
				case 0:
					saberName = &t->classes[i]->saber1[0];
					break;
				case 1:
					saberName = &t->classes[i]->saber2[0];
					break;
				default:
					saberName = NULL;
					break;
				}

				if (saberName && saberName[0])
				{
					WP_SaberParseParms(saberName, &saber);
					if (!Q_stricmp(saberName, saber.name))
					{ //found the matching saber
						if (saber.model[0])
						{
							BG_ModelCache(saber.model, NULL);
						}
					}
				}

				sNum++;
			}

			i++;
		}
	}
}
#endif

qboolean BG_SiegeCheckClassLegality(int team, char *classname)
{
	siegeTeam_t **teamPtr = NULL;
	int i = 0;

	if (team == SIEGETEAM_TEAM1)
	{
		teamPtr = &team1Theme;
	}
	else if (team == SIEGETEAM_TEAM2)
	{
		teamPtr = &team2Theme;
	}
	else
	{ //spectator? Whatever, you're legal then.
		return qtrue;
	}

	if (!teamPtr || !(*teamPtr))
	{ //Well, guess the class is ok, seeing as there is no team theme to begin with.
		return qtrue;
	}

	//See if the class is listed on the team
	while (i < (*teamPtr)->numClasses)
	{
		if (!Q_stricmp(classname, (*teamPtr)->classes[i]->name))
		{ //found it, so it's alright
			return qtrue;
		}
		i++;
	}

	//Didn't find it, so copy the name of the first valid class over it.
	strcpy(classname, (*teamPtr)->classes[0]->name);

	return qfalse;
}

siegeTeam_t *BG_SiegeFindTeamForTheme(char *themeName)
{
	int i = 0;

	while (i < bgNumSiegeTeams)
	{
		if (bgSiegeTeams[i].name &&
			!Q_stricmp(bgSiegeTeams[i].name, themeName))
		{ //this is what we're looking for
			return &bgSiegeTeams[i];
		}

		i++;
	}
	return NULL;
}

void BG_SiegeSetTeamTheme(int team, char *themeName, char *backup)
{
	siegeTeam_t **teamPtr = NULL;

	if (team == SIEGETEAM_TEAM1)
	{
		teamPtr = &team1Theme;
	}
	else
	{
		teamPtr = &team2Theme;
	}
	if (!BG_SiegeFindTeamForTheme(themeName))
	{
		(*teamPtr) = BG_SiegeFindTeamForTheme(backup);
	}
	else
	{
		(*teamPtr) = BG_SiegeFindTeamForTheme(themeName);
	}
}

int BG_SiegeFindClassIndexByName(const char *classname)
{
	int i = 0;

	while (i < bgNumSiegeClasses)
	{
		if (!Q_stricmp(bgSiegeClasses[i].name, classname))
		{ //found it
			return i;
		}
		i++;
	}

	return -1;
}
//======================================
//End misc/utility functions
//======================================

#include "namespace_end.h"
