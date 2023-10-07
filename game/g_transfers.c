#include "g_local.h"

#include "cJSON.h"

void G_HandleTransferResult(trsfHandle_t handle, trsfErrorInfo_t* errorInfo, int responseCode, void* data, size_t size) {
	// nothing to do here for now...
}

static void SendMatchMultipart(const char* url, const char* matchid, const char* stats, const char* payload) {
	// send the match result as a multipart discord POST form

	trsfFormPart_t multiPart[2];
	memset(&multiPart, 0, sizeof(multiPart));

	// attach ctfstats as a txt file
	multiPart[0].partName = "file";
	multiPart[0].isFile = qtrue;
	multiPart[0].filename = va("pug_%s.txt", matchid);
	multiPart[0].buf = stats;
	multiPart[0].bufSize = strlen(stats);

	// send the rest of the message as payload
	multiPart[1].partName = "payload_json";
	multiPart[1].buf = payload;
	multiPart[1].bufSize = strlen(payload);

	trap_SendMultipartPOSTRequest(NULL, url, multiPart, 2, NULL, NULL, qfalse);
}

#define DISCORD_WEBHOOK_FORMAT		"https://discordapp.com/api/webhooks/%s/%s"
#define DEMOARCHIVE_MATCH_FORMAT	"https://demos.jactf.com/match.html#rpc=lookup&id=%s"

extern const char* G_GetArenaInfoByMap(const char* map);

void G_PostScoreboardToWebhook(const char* stats) {
	if (!VALIDSTRING(g_webhookId.string) || !VALIDSTRING(g_webhookToken.string) || g_gametype.integer != GT_SIEGE || level.siegeStage < SIEGESTAGE_ROUND2) {
		return;
	}

	// get the time string for each team
	char round1OffenseTimeString[64] = { 0 };
	G_ParseMilliseconds(atoi(Cvar_VariableString("siege_r1_total")), round1OffenseTimeString, sizeof(round1OffenseTimeString));
	char round2OffenseTimeString[64] = { 0 };
	G_ParseMilliseconds(atoi(Cvar_VariableString("siege_r2_total")), round2OffenseTimeString, sizeof(round2OffenseTimeString));

	int msgColor;
	char round1OffenseString[64] = { 0 }, round2OffenseString[64] = { 0 };
	if (level.siegeMatchWinner == SIEGEMATCHWINNER_ROUND1OFFENSE) {
		msgColor = 255;

		const int round1MissingObjs = siege_r1_objscompleted.string[0] && siege_r1_objscompleted.integer < level.numSiegeObjectivesOnMap ? level.numSiegeObjectivesOnMap - siege_r1_objscompleted.integer : 0;
		if (round1MissingObjs > 0)
			Com_sprintf(round1OffenseString, sizeof(round1OffenseString), "WIN (-%d obj) :trophy:", round1MissingObjs);
		else
			Com_sprintf(round1OffenseString, sizeof(round1OffenseString), "WIN (%s) :trophy:", round1OffenseTimeString);

		const int round2MissingObjs = level.numSiegeObjectivesOnMap - level.totalObjectivesCompleted;
		if (round2MissingObjs > 0)
			Com_sprintf(round2OffenseString, sizeof(round2OffenseString), "LOSE (-%d obj)", round2MissingObjs);
		else
			Com_sprintf(round2OffenseString, sizeof(round2OffenseString), "LOSE");
	}
	else if (level.siegeMatchWinner == SIEGEMATCHWINNER_ROUND2OFFENSE) {
		msgColor = 16711680;
		const int round1MissingObjs = siege_r1_objscompleted.string[0] && siege_r1_objscompleted.integer < level.numSiegeObjectivesOnMap ? level.numSiegeObjectivesOnMap - siege_r1_objscompleted.integer : 0;
		if (round1MissingObjs > 0)
			Com_sprintf(round1OffenseString, sizeof(round1OffenseString), "LOSE (-%d obj)", round1MissingObjs);
		else
			Com_sprintf(round1OffenseString, sizeof(round1OffenseString), "LOSE (%s)", round1OffenseTimeString);

		Com_sprintf(round2OffenseString, sizeof(round2OffenseString), "WIN (%s) :trophy:", round2OffenseTimeString);
	}
	else { // draw
		msgColor = -1;
		trap_Cvar_Update(&siege_r2_heldformaxat);
		if (siege_r1_heldformaxat.string[0] && siege_r1_heldformaxat.integer > 0 &&
			siege_r2_heldformaxat.string[0] && siege_r2_heldformaxat.integer == siege_r1_heldformaxat.integer) {
			const int missingObjs = level.numSiegeObjectivesOnMap - level.totalObjectivesCompleted;
			Com_sprintf(round1OffenseString, sizeof(round1OffenseString), "DRAW (-%d obj) :handshake:", missingObjs);
			Com_sprintf(round2OffenseString, sizeof(round2OffenseString), "DRAW (-%d obj) :handshake:", missingObjs);
		}
		else {
			Com_sprintf(round1OffenseString, sizeof(round1OffenseString), "DRAW (%s) :handshake:", round1OffenseTimeString);
			Com_sprintf(round2OffenseString, sizeof(round2OffenseString), "DRAW (%s) :handshake:", round2OffenseTimeString);
		}
	}

	// build a list of players in each team
	int numSorted = level.numConnectedClients;
	char redTeam[256] = { 0 }, blueTeam[256] = { 0 };
	int i;
	for (i = 0; i < numSorted; i++) {
		gclient_t* cl = &level.clients[level.sortedClients[i]];

		if (!cl || cl->pers.connected != CON_CONNECTED) {
			continue;
		}

		char* buf;
		size_t bufSize;
		switch (cl->sess.sessionTeam) {
			case TEAM_RED: buf = redTeam; bufSize = sizeof(redTeam); break;
			case TEAM_BLUE: buf = blueTeam; bufSize = sizeof(blueTeam); break;
			default: buf = NULL; bufSize = 0;
		}

		if (buf) {
			if (VALIDSTRING(buf)) Q_strcat(buf, bufSize, "\n");
			char name[MAX_NETNAME];
			Q_strncpyz(name, cl->pers.netname, sizeof(name));
			Q_strcat(buf, bufSize, Q_CleanStr(name));
		}
	}

	if (!VALIDSTRING(redTeam) || !VALIDSTRING(blueTeam)) {
		return;
	}

	// get a clean string of the server name
	char serverName[64];
	trap_Cvar_VariableStringBuffer("sv_hostname", serverName, sizeof(serverName));
	Q_CleanStr(serverName);

	// get match id for demo link if possible
	char matchId[SV_MATCHID_LEN];
	trap_Cvar_VariableStringBuffer("sv_matchid", matchId, sizeof(matchId));

	char round1MatchId[SV_MATCHID_LEN];
	trap_Cvar_VariableStringBuffer("siege_r1_matchid", round1MatchId, sizeof(round1MatchId));

	// get map str
	char mapStr[256] = { 0 };
	char mapname[MAX_QPATH];
	trap_Cvar_VariableStringBuffer("mapname", mapname, sizeof(mapname));
	if (VALIDSTRING(mapname)) {
		Q_strncpyz(mapStr, mapname, sizeof(mapStr));

		const char* arenaInfo = G_GetArenaInfoByMap(mapname);
		if (arenaInfo) {
			char* mapLongName = Info_ValueForKey(arenaInfo, "longname");
			if (VALIDSTRING(mapLongName)) {
				Com_sprintf(mapStr, sizeof(mapStr), "%s (%s)", mapLongName, mapname);
			}
		}
	}
	Q_CleanStr(mapStr);

	// build the json string to post to discord
	cJSON* root = cJSON_CreateObject();
	if (root) {
		{
			if (VALIDSTRING(serverName)) {
				cJSON_AddStringToObject(root, "username", serverName);
			}

			cJSON* embeds = cJSON_AddArrayToObject(root, "embeds");

			{
				cJSON* msg = cJSON_CreateObject();
				if (msgColor >= 0) {
					cJSON_AddNumberToObject(msg, "color", msgColor);
				}
				cJSON* fields = cJSON_AddArrayToObject(msg, "fields");

				{

					{
						cJSON* redField = cJSON_CreateObject();
						cJSON_AddStringToObject(redField, "name", round1OffenseString);
						cJSON_AddStringToObject(redField, "value", blueTeam);
						cJSON_AddBoolToObject(redField, "inline", cJSON_True);
						cJSON_AddItemToArray(fields, redField);
					}
					{
						cJSON* blueField = cJSON_CreateObject();
						cJSON_AddStringToObject(blueField, "name", round2OffenseString);
						cJSON_AddStringToObject(blueField, "value", redTeam);
						cJSON_AddBoolToObject(blueField, "inline", cJSON_True);
						cJSON_AddItemToArray(fields, blueField);
					}
					{
						if (VALIDSTRING(matchId)) {
							if (VALIDSTRING(round1MatchId)) {
								// we have both
								cJSON *link1Field = cJSON_CreateObject();
								cJSON_AddStringToObject(link1Field, "name", "Demoarchive links");
								cJSON_AddStringToObject(link1Field, "value", va(DEMOARCHIVE_MATCH_FORMAT"\n"DEMOARCHIVE_MATCH_FORMAT"\n(May not work until uploaded)", round1MatchId, matchId));
								cJSON_AddItemToArray(fields, link1Field);
							}
							else {
								// we only have round 2 somehow
								cJSON *linkField = cJSON_CreateObject();
								cJSON_AddStringToObject(linkField, "name", "Demoarchive link");
								cJSON_AddStringToObject(linkField, "value", va(DEMOARCHIVE_MATCH_FORMAT"\n(May not work until uploaded)", matchId));
								cJSON_AddItemToArray(fields, linkField);
							}
						}
					}
					{
						if (VALIDSTRING(mapStr)) {
							cJSON* mapField = cJSON_CreateObject();
							cJSON_AddStringToObject(mapField, "name", "Map");
							cJSON_AddStringToObject(mapField, "value", mapStr);
							cJSON_AddBoolToObject(mapField, "inline", cJSON_False);
							cJSON_AddItemToArray(fields, mapField);
						}
					}
				}

				cJSON_AddItemToArray(embeds, msg);
			}
		}

		// generate the request and send it
		char *requestString = cJSON_PrintUnformatted(root);

		const char* url = va(DISCORD_WEBHOOK_FORMAT, g_webhookId.string, g_webhookToken.string);

		if (VALIDSTRING(stats)) {
			// if we have stats, send the request as a multipart and attach stats as a file
			SendMatchMultipart(url, matchId, stats, requestString);
		} else {
			// otherwise, just send a simple json POST request
			trap_SendPOSTRequest(NULL, url, requestString, "application/json", "application/json", qfalse);
		}

		free(requestString);
	}
	cJSON_Delete(root);
}