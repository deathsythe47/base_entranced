// d_debugForcePowers: logs what happened to each force press, plus a detailed dump on the forcebug command

#include "g_local.h"
#include "g_forcedebug.h"
#include "bg_saga.h"

extern int forcePowerNeeded[NUM_FORCE_POWER_LEVELS][NUM_FORCE_POWERS];
extern qboolean G_InGetUpAnim(playerState_t *ps);
extern char *ConcatArgs(int start);
extern qboolean WP_ForcePowerAvailable(gentity_t *self, forcePowers_t forcePower, int overrideAmt);
extern qboolean WP_ForcePowerUsable(gentity_t *self, forcePowers_t forcePower);

#define FD_RING			8192	// usercmds kept per client (30 s at 250 fps)
#define FD_HIST			64
#define FD_DUMP_MS		30000
#define FD_AUTO_MS		3000
#define FD_MAX_LINES	10		// per client per second, outside dumps
#define FD_DUMP_LINES	700

// what happened to a usercmd
enum { FDR_RAN, FDR_PAUSE, FDR_STAMP, FDR_NOTCONN, FDR_INTERMISSION, FDR_GHOST, FDR_SPEC, FDR_OTHER };
static const char *resNames[] = { "ran", "pause", "stamp-not-newer", "not-connected", "intermission", "ghost", "spectator", "returned-early" };

#define FDF_FORCED	1
#define FDF_HELD	2

// force-related buttons, packed
#define FDB_GRIP	1
#define FDB_LIGHT	2
#define FDB_DRAIN	4
#define FDB_FORCE	8
#define FDB_USE		16

enum { FDP_NONE, FDP_OK, FDP_OFF, FDP_REFUSED };

typedef struct {
	int		lt, st;
	short	msec;
	byte	gen, sel, btn, flags, res;
} fdCmd_t;

typedef struct {
	qboolean	active;
	int			gen, power, button;
	const char	*source;
	int			start, lastSeen;
	int			attempts, drops, firstDrop;
	int			result;
	char		reason[64];
	qboolean	stateSet;
	char		state[256];
} fdPress_t;

typedef struct {
	fdCmd_t		ring[FD_RING];
	int			ringHead, ringCount;

	// the usercmd being run
	qboolean	inCmd, reached, cmdHasPress, forcedNext;
	int			cur, cmdMsec, cmdLag, cmdGap, lastSt;
	int			lastGen, lastSel;

	fdPress_t	gp;			// bind press
	fdPress_t	bp[4];		// button presses: grip, lightning, drain, +useforce
	int			lastBtn;

	// one gencmd attempt (the switch in ClientThink_real)
	qboolean	inAttempt;
	int			atFp, atDeb, atSlot, atSlotStamp, atHealth;
	int			startedMask, stoppedMask, btnStarted, btnStopped;

	// last logged force state
	qboolean	stInit;
	int			stKnown, stSel, stLevels[NUM_FORCE_POWERS];

	int			thrStart, thrCount, thrSuppressed;

	char		hist[FD_HIST][320];
	int			histTime[FD_HIST], histHead, histCount;

	struct { int t, from, req; qboolean ok; } cyc[FD_HIST];
	int			cycHead, cycCount;

	int			nextBugMs, nextAutoMs;
	qboolean	autoDumpPending;
} fdClient_t;

static fdClient_t fdc[MAX_CLIENTS];
static int fdNextDumpMs;

// dump lines wait here and are written a few per frame, so a dump never stalls one frame on log and console writes
#define FD_QUEUE_SIZE	(256 * 1024)
#define FD_LINES_PER_FRAME	40
static char fdQueue[FD_QUEUE_SIZE];
static int fdQueueLen, fdQueuePos;
static qboolean fdQueueFull;

static const char *powerNames[NUM_FORCE_POWERS] = {
	"heal", "jump", "speed", "push", "pull", "mindtrick", "grip", "lightning", "rage",
	"protect", "absorb", "teamheal", "teamforce", "drain", "sight", "saberatk", "saberdef", "saberthrow"
};

static const char *PowerName(int p) {
	if (p >= 0 && p < NUM_FORCE_POWERS)
		return powerNames[p];
	return va("none(%d)", p);
}

// power and debounce slot of a force gencmd; -1 if it isn't one
static int GenPower(int gen, int *slot) {
	int s = -1, p = -1;
	switch (gen) {
	case GENCMD_FORCE_HEAL:				p = FP_HEAL; s = GENCMD_DELAY_HEAL; break;
	case GENCMD_FORCE_SPEED:			p = FP_SPEED; s = GENCMD_DELAY_SPEED; break;
	case GENCMD_FORCE_THROW:			p = FP_PUSH; break;
	case GENCMD_FORCE_PULL:				p = FP_PULL; break;
	case GENCMD_FORCE_DISTRACT:			p = FP_TELEPATHY; s = GENCMD_DELAY_TRICK; break;
	case GENCMD_FORCE_RAGE:				p = FP_RAGE; s = GENCMD_DELAY_RAGE; break;
	case GENCMD_FORCE_PROTECT:			p = FP_PROTECT; s = GENCMD_DELAY_PROTECT; break;
	case GENCMD_FORCE_ABSORB:			p = FP_ABSORB; s = GENCMD_DELAY_ABSORB; break;
	case GENCMD_FORCE_HEALOTHER:		p = FP_TEAM_HEAL; break;
	case GENCMD_FORCE_FORCEPOWEROTHER:	p = FP_TEAM_FORCE; break;
	case GENCMD_FORCE_SEEING:			p = FP_SEE; s = GENCMD_DELAY_SEEING; break;
	}
	if (slot)
		*slot = s;
	return p;
}

static fdClient_t *FD_Get(gentity_t *ent) {
	int n;
	if (!d_debugForcePowers.integer || !ent || !ent->client)
		return NULL;
	n = ent - g_entities;
	if (n < 0 || n >= MAX_CLIENTS || (ent->r.svFlags & SVF_BOT) || ent->client->sess.sessionTeam == TEAM_SPECTATOR)
		return NULL;
	return &fdc[n];
}

static void FD_LogRaw(gentity_t *ent, const char *text) {
	G_LogPrintf("FP: %d %s %s\n", (int)(ent - g_entities), ent->client->pers.netname, text);
}

static void FD_QueueLine(gentity_t *ent, const char *text) {
	char line[1000];
	int len;

	if (fdQueueFull)
		return;
	Com_sprintf(line, sizeof(line), "FP: %d %s %s", (int)(ent - g_entities), ent->client->pers.netname, text);
	len = strlen(line) + 1;
	if (fdQueueLen + len + 64 > FD_QUEUE_SIZE) {
		Q_strncpyz(line, "FP: dump truncated (queue full)", sizeof(line));
		len = strlen(line) + 1;
		fdQueueFull = qtrue;
	}
	memcpy(fdQueue + fdQueueLen, line, len);
	fdQueueLen += len;
}

static qboolean FD_QueueBusy(void) {
	return fdQueuePos < fdQueueLen;
}

// from G_RunFrame
void FD_RunFrame(void) {
	int k;
	for (k = 0; k < FD_LINES_PER_FRAME && fdQueuePos < fdQueueLen; k++) {
		const char *line = fdQueue + fdQueuePos;
		G_LogPrintf("%s\n", line);
		fdQueuePos += strlen(line) + 1;
	}
	if (fdQueuePos >= fdQueueLen)
		fdQueuePos = fdQueueLen = 0, fdQueueFull = qfalse;
}

// throttled line
static void FD_Log(gentity_t *ent, fdClient_t *f, const char *fmt, ...) {
	va_list ap;
	char buf[900];

	if (level.time - f->thrStart >= 1000 || level.time < f->thrStart) {
		if (f->thrSuppressed)
			FD_LogRaw(ent, va("suppressed %d lines", f->thrSuppressed));
		f->thrStart = level.time;
		f->thrCount = f->thrSuppressed = 0;
	}
	if (f->thrCount >= FD_MAX_LINES) {
		f->thrSuppressed++;
		return;
	}
	f->thrCount++;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	FD_LogRaw(ent, buf);
}

static void PowerList(int mask, char *out, int size, const int *levels) {
	int i;
	out[0] = '\0';
	for (i = 0; i < NUM_FORCE_POWERS; i++) {
		if (!(mask & (1 << i)))
			continue;
		if (out[0])
			Q_strcat(out, size, ",");
		Q_strcat(out, size, levels ? va("%s%d", powerNames[i], levels[i]) : powerNames[i]);
	}
	if (!out[0])
		Q_strncpyz(out, "-", size);
}

#define ACTIVE_MASK(ps) ((ps)->fd.forcePowersActive & ((1 << NUM_FORCE_POWERS) - 1))

static void FD_StateString(gentity_t *ent, fdClient_t *f, int power, qboolean fromCmd, char *out, int size) {
	gclient_t *cl = ent->client;
	playerState_t *ps = &cl->ps;
	char act[128];
	char cost[32] = "";
	int lag = fromCmd ? f->cmdLag : level.time - cl->pers.cmd.serverTime;

	PowerList(ACTIVE_MASK(ps), act, sizeof(act), NULL);
	if (power >= 0 && power < NUM_FORCE_POWERS)
		Com_sprintf(cost, sizeof(cost), "/%d", forcePowerNeeded[ps->fd.forcePowerLevel[power]][power]);
	Com_sprintf(out, size, "| fp %d%s act %s sel %s hp %d ping %d lag %d gap %d held %d",
		ps->fd.forcePower, cost, act, PowerName(ps->fd.forcePowerSelected), cl->ps.stats[STAT_HEALTH], ps->ping,
		lag, fromCmd ? f->cmdGap : level.time - cl->lastRealCmdTime, cl->pers.numHeldCmds);
}

static const char *ToggleOffWindow(gentity_t *ent, int p) {
	playerState_t *ps = &ent->client->ps;
	switch (p) {
	case FP_SPEED: case FP_SEE: case FP_PROTECT: case FP_ABSORB: case FP_RAGE: case FP_TELEPATHY:
		if (ps->forceAllowDeactivateTime >= level.time)
			return va("already-active(cannot-turn-off-for-%dms)", ps->forceAllowDeactivateTime - level.time);
		return "already-active";
	}
	return "already-active";
}

static qboolean ObjectiveBlocks(gentity_t *ent, int p) {
	gclient_t *cl = ent->client;
	if (!cl->holdingObjectiveItem || cl->holdingObjectiveItem >= MAX_GENTITIES || !g_entities[cl->holdingObjectiveItem].genericValue15)
		return qfalse;
	if (g_gametype.integer == GT_SIEGE && cl->siegeClass != -1 && (bgSiegeClasses[cl->siegeClass].forceWithItem & (1 << p)))
		return qfalse;
	return qtrue;
}

// read-only guess at the first check that refused power p, in the order the code checks
static const char *FD_WhyRefused(gentity_t *ent, int p) {
	gclient_t *cl = ent->client;
	playerState_t *ps = &cl->ps;

	if (p < 0 || p >= NUM_FORCE_POWERS)
		return "no-power-selected";
	if (cl->emoted && p != FP_LEVITATION)
		return "emoted";
	if (BG_HasYsalamiri(g_gametype.integer, ps))
		return "ysalamiri";
	if (ent->health <= 0 || ps->stats[STAT_HEALTH] <= 0 || (ps->eFlags & EF_DEAD))
		return "dead";
	if (ps->pm_flags & PMF_FOLLOW)
		return "following";
	if (cl->sess.sessionTeam == TEAM_SPECTATOR)
		return "spectator";
	if (cl->tempSpectate >= level.time)
		return "ghost";
	if (ps->forceRestricted || ps->trueNonJedi)
		return "non-jedi";
	if (ps->weapon == WP_EMPLACED_GUN)
		return "emplaced-gun";
	if (ps->m_iVehicleNum)
		return "vehicle";
	if (cl->sess.siegeDuelInProgress)
		return "siege-duel";
	if (ps->duelInProgress && p != FP_SABER_OFFENSE && p != FP_SABER_DEFENSE && p != FP_LEVITATION && (!ps->saberLockFrame || p != FP_PUSH))
		return "duel";
	if ((ps->saberLockFrame || ps->saberLockTime > level.time) && p != FP_PUSH)
		return "saber-lock";
	if (ps->fallingToDeath)
		return "falling-to-death";
	if (!BG_CanUseFPNow(g_gametype.integer, ps, level.time, p))
		return "cannot-use-now(broken-arm?)";
	if (!(ps->fd.forcePowersKnown & (1 << p)))
		return "not-known";
	if ((ps->fd.forcePowersActive & (1 << p)) && p != FP_LEVITATION)
		return ToggleOffWindow(ent, p);
	if (!ps->fd.forcePowerLevel[p])
		return "level-0";
	if (!WP_ForcePowerAvailable(ent, p, 0))
		return va("not-enough-fp(%d<%d)", ps->fd.forcePower, forcePowerNeeded[ps->fd.forcePowerLevel[p]][p]);
	if (!WP_ForcePowerUsable(ent, p))
		return "saber-or-wall-restriction";

	switch (p) {
	case FP_HEAL:
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
			return "health-full";
		break;
	case FP_TEAM_HEAL:
	case FP_TEAM_FORCE:
		if (ps->fd.forcePowerDebounce[p] >= level.time)
			return va("cooldown(%dms)", ps->fd.forcePowerDebounce[p] - level.time);
		return "no-eligible-teammate";
	case FP_RAGE:
		if (ps->fd.forceRageRecoveryTime >= level.time)
			return va("rage-recovery(%dms)", ps->fd.forceRageRecoveryTime - level.time);
		if (ent->health < 10)
			return "health-below-10";
		break;
	case FP_PUSH:
	case FP_PULL:
		if (ps->forceHandExtend != HANDEXTEND_NONE && (ps->forceHandExtend != HANDEXTEND_KNOCKDOWN || !G_InGetUpAnim(ps)))
			return va("hand-busy(%d)", ps->forceHandExtend);
		if (!g_useWhileThrowing.integer && ps->saberInFlight)
			return "saber-thrown";
		if (ps->weaponTime > 0)
			return va("weapon-busy(%dms)", ps->weaponTime);
		if (ps->powerups[PW_DISINT_4] > level.time)
			return va("push-cooldown(%dms)", ps->powerups[PW_DISINT_4] - level.time);
		break;
	case FP_TELEPATHY:
	case FP_GRIP:
	case FP_LIGHTNING:
	case FP_DRAIN:
		if (ps->forceHandExtend != HANDEXTEND_NONE)
			return va("hand-busy(%d)", ps->forceHandExtend);
		if (ps->weaponTime > 0)
			return va("weapon-busy(%dms)", ps->weaponTime);
		if (p == FP_GRIP && ps->fd.forceGripUseTime > level.time)
			return va("cooldown(%dms)", ps->fd.forceGripUseTime - level.time);
		if (ps->fd.forcePowerDebounce[p] > level.time)
			return va("cooldown(%dms)", ps->fd.forcePowerDebounce[p] - level.time);
		if (p == FP_TELEPATHY && ObjectiveBlocks(ent, p))
			return "holding-objective";
		if (p == FP_TELEPATHY || p == FP_GRIP)
			return "no-target";
		break;
	case FP_SPEED:
		if (ObjectiveBlocks(ent, p))
			return "holding-objective";
		break;
	}
	return "unknown";
}

static void FD_History(fdClient_t *f, const char *line) {
	Q_strncpyz(f->hist[f->histHead], line, sizeof(f->hist[0]));
	f->histTime[f->histHead] = level.time;
	f->histHead = (f->histHead + 1) % FD_HIST;
	if (f->histCount < FD_HIST)
		f->histCount++;
}

static void FD_Dump(gentity_t *ent, fdClient_t *f, int windowMs, const char *why);

static void FD_Finalize(gentity_t *ent, fdClient_t *f, fdPress_t *p) {
	char line[600], res[128];
	qboolean dropped = qfalse;

	if (!p->active)
		return;
	p->active = qfalse;

	switch (p->result) {
	case FDP_OK:		Q_strncpyz(res, "ok", sizeof(res)); break;
	case FDP_OFF:		Q_strncpyz(res, "off", sizeof(res)); break;
	case FDP_REFUSED:	Com_sprintf(res, sizeof(res), "refused:%s", p->reason); break;
	default:
		if (p->drops) {
			Com_sprintf(res, sizeof(res), "dropped:%s", resNames[p->firstDrop]);
			if (p->firstDrop == FDR_GHOST && p->power == FP_SPEED && g_preActivateSpeedWhileDead.integer)
				Q_strcat(res, sizeof(res), "(kept-for-next-spawn)");
			dropped = qtrue;
		}
		else {
			Com_sprintf(res, sizeof(res), "unresolved:%s", p->reason[0] ? p->reason : "no-attempt");
		}
		break;
	}
	if (p->result != FDP_NONE && p->drops)
		Q_strcat(res, sizeof(res), va(" (after %d dropped:%s)", p->drops, resNames[p->firstDrop]));
	if (p->attempts > 1)
		Q_strcat(res, sizeof(res), va(" x%d", p->attempts));
	if (!p->stateSet)
		FD_StateString(ent, f, p->power, qfalse, p->state, sizeof(p->state));

	Com_sprintf(line, sizeof(line), "press %s %s %s %s", PowerName(p->power), p->source, res, p->state);
	FD_History(f, line);
	FD_Log(ent, f, "%s", line);

	// only drops without a game reason get the automatic dump; written at the end of the frame, when the usercmd
	// being run has its result
	if (dropped && (p->firstDrop == FDR_STAMP || p->firstDrop == FDR_OTHER || p->firstDrop == FDR_NOTCONN))
		f->autoDumpPending = qtrue;
}

static void FD_OpenPress(gentity_t *ent, fdPress_t *p, int gen, int power, int button, const char *source) {
	memset(p, 0, sizeof(*p));
	p->active = qtrue;
	p->gen = gen;
	p->power = power;
	p->button = button;
	p->source = source;
	p->start = p->lastSeen = level.time;
	p->firstDrop = -1;
}

void FD_ClientReset(int clientNum) {
	fdClient_t *f;
	if (clientNum < 0 || clientNum >= MAX_CLIENTS)
		return;
	f = &fdc[clientNum];
	memset(f, 0, sizeof(*f));
	f->lastGen = f->lastSel = -1;
}

void FD_MarkForcedThink(gentity_t *ent) {
	fdClient_t *f = FD_Get(ent);
	if (f)
		f->forcedNext = qtrue;
}

void FD_CmdBegin(gentity_t *ent) {
	fdClient_t *f = FD_Get(ent);
	gclient_t *cl;
	usercmd_t *u;
	fdCmd_t *c;
	int st, gen, power, sel, btn = 0;

	if (!f)
		return;
	cl = ent->client;
	u = &cl->pers.cmd;

	st = u->serverTime;
	if (st > level.time + 200)
		st = level.time + 200;
	if (st < level.time - 1000)
		st = level.time - 1000;
	f->cmdMsec = st - cl->ps.commandTime;
	f->cmdLag = level.time - u->serverTime;
	f->cmdGap = f->lastSt ? u->serverTime - f->lastSt : 0;
	f->lastSt = u->serverTime;

	if (u->buttons & BUTTON_FORCEGRIP) btn |= FDB_GRIP;
	if (u->buttons & BUTTON_FORCE_LIGHTNING) btn |= FDB_LIGHT;
	if (u->buttons & BUTTON_FORCE_DRAIN) btn |= FDB_DRAIN;
	if (u->buttons & BUTTON_FORCEPOWER) btn |= FDB_FORCE;
	if (u->buttons & BUTTON_USE) btn |= FDB_USE;

	f->cur = f->ringHead;
	c = &f->ring[f->ringHead];
	c->lt = level.time;
	c->st = u->serverTime;
	c->msec = (short)Com_Clampi(-32000, 32000, f->cmdMsec);
	c->gen = u->generic_cmd;
	c->sel = u->forcesel;
	c->btn = btn;
	c->flags = (f->forcedNext ? FDF_FORCED : 0) | (level.cmdStamp ? FDF_HELD : 0);
	c->res = FDR_OTHER;
	f->ringHead = (f->ringHead + 1) % FD_RING;
	if (f->ringCount < FD_RING)
		f->ringCount++;

	// a bind rides on every usercmd until the client's next packet, so a press is the edge where generic_cmd changes
	gen = u->generic_cmd;
	power = GenPower(gen, NULL);
	if (gen != f->lastGen) {
		FD_Finalize(ent, f, &f->gp);
		if (power >= 0)
			FD_OpenPress(ent, &f->gp, gen, power, -1, f->forcedNext ? "forced-think" : "bind");
	}
	else if (power >= 0 && !f->gp.active && f->forcedNext) {
		FD_OpenPress(ent, &f->gp, gen, power, -1, "forced-think-replay");
	}
	if (f->gp.active && f->gp.gen == gen)
		f->gp.lastSeen = level.time;
	f->lastGen = gen;
	f->cmdHasPress = f->gp.active && f->gp.gen == gen && power >= 0;

	// selection requests (force cycling happens on the client; this is what it asked for)
	sel = u->forcesel;
	if (sel != f->lastSel) {
		qboolean ok = sel < NUM_FORCE_POWERS && (cl->ps.fd.forcePowersKnown & (1 << sel));
		int i = f->cycHead;
		f->cyc[i].t = level.time;
		f->cyc[i].from = cl->ps.fd.forcePowerSelected;
		f->cyc[i].req = sel;
		f->cyc[i].ok = ok;
		f->cycHead = (f->cycHead + 1) % FD_HIST;
		if (f->cycCount < FD_HIST)
			f->cycCount++;
		if (!ok) {
			char known[256];
			PowerList(cl->ps.fd.forcePowersKnown, known, sizeof(known), NULL);
			FD_Log(ent, f, "select %s ignored: %s | known %s sel %s", PowerName(sel),
				sel < NUM_FORCE_POWERS ? "not-known" : "client-sent-no-selection", known, PowerName(cl->ps.fd.forcePowerSelected));
		}
		f->lastSel = sel;
	}

	f->forcedNext = qfalse;
	f->inCmd = qtrue;
	f->reached = qfalse;
	f->startedMask = f->stoppedMask = 0;
}

static void FD_Drop(gentity_t *ent, fdClient_t *f, int res) {
	fdPress_t *p = &f->gp;
	p->drops++;
	if (p->firstDrop < 0)
		p->firstDrop = res;
	if (!p->stateSet) {
		FD_StateString(ent, f, p->power, qtrue, p->state, sizeof(p->state));
		p->stateSet = qtrue;
	}
}

void FD_PreGenCmd(gentity_t *ent, const usercmd_t *cmd) {
	fdClient_t *f = FD_Get(ent);
	gclient_t *cl;
	int slot;

	if (!f || !f->inCmd)
		return;
	cl = ent->client;
	f->reached = qtrue;
	if (level.pause.state != PAUSE_NONE) {
		f->ring[f->cur].res = FDR_PAUSE;
		if (f->cmdHasPress)
			FD_Drop(ent, f, FDR_PAUSE);
		return;
	}
	f->ring[f->cur].res = FDR_RAN;
	if (!f->cmdHasPress || GenPower(cmd->generic_cmd, &slot) < 0)
		return;
	f->inAttempt = qtrue;
	f->atFp = cl->ps.fd.forcePower;
	f->atDeb = cl->ps.fd.forcePowerDebounce[f->gp.power];
	f->atHealth = cl->ps.stats[STAT_HEALTH];
	f->atSlot = slot;
	f->atSlotStamp = slot >= 0 ? cl->genCmdDebounce[slot] : 0;
	f->startedMask = f->stoppedMask = 0;
	if (!f->gp.stateSet) {
		FD_StateString(ent, f, f->gp.power, qtrue, f->gp.state, sizeof(f->gp.state));
		f->gp.stateSet = qtrue;
	}
}

void FD_PostGenCmd(gentity_t *ent) {
	fdClient_t *f = FD_Get(ent);
	gclient_t *cl;
	fdPress_t *p;
	int bit, result;
	const char *reason = "";

	if (!f || !f->inAttempt)
		return;
	f->inAttempt = qfalse;
	cl = ent->client;
	p = &f->gp;
	bit = 1 << p->power;

	if ((f->stoppedMask & bit) && !(f->startedMask & bit))
		result = FDP_OFF;
	else if ((f->startedMask & bit) || cl->ps.fd.forcePower < f->atFp || cl->ps.fd.forcePowerDebounce[p->power] != f->atDeb ||
		(p->power == FP_HEAL && cl->ps.stats[STAT_HEALTH] > f->atHealth))
		result = FDP_OK;
	else {
		result = FDP_REFUSED;
		if (cl->emoted)
			reason = "emoted";
		else if (f->atSlot >= 0 && cl->genCmdDebounce[f->atSlot] == f->atSlotStamp)
			reason = va("debounce(%dms-since-last)", level.time - f->atSlotStamp);
		else
			reason = FD_WhyRefused(ent, p->power);
	}

	p->attempts++;
	if (result == FDP_OK || result == FDP_OFF) {
		if (p->result != FDP_OK && p->result != FDP_OFF)
			p->result = result;
	}
	else if (p->result == FDP_NONE) {
		p->result = result;
		Q_strncpyz(p->reason, reason, sizeof(p->reason));
	}
}

void FD_CmdEnd(gentity_t *ent) {
	fdClient_t *f = FD_Get(ent);
	gclient_t *cl;
	int res;

	if (!f || !f->inCmd)
		return;
	f->inCmd = qfalse;
	f->inAttempt = qfalse;
	if (f->reached)
		return;
	cl = ent->client;
	if (cl->pers.connected != CON_CONNECTED)
		res = FDR_NOTCONN;
	else if (f->cmdMsec < 1 && cl->sess.spectatorState != SPECTATOR_FOLLOW)
		res = FDR_STAMP;
	else if (level.intermissiontime)
		res = FDR_INTERMISSION;
	else if (cl->tempSpectate > level.time)
		res = FDR_GHOST;
	else if (cl->sess.sessionTeam == TEAM_SPECTATOR)
		res = FDR_SPEC;
	else
		res = FDR_OTHER;
	f->ring[f->cur].res = res;
	if (!f->cmdHasPress)
		return;
	if (res == FDR_STAMP && f->gp.power == FP_SPEED && (f->startedMask & (1 << FP_SPEED))) {
		// g_preActivateSpeedWhileDead ran it anyway
		f->gp.attempts++;
		if (f->gp.result != FDP_OK && f->gp.result != FDP_OFF)
			f->gp.result = FDP_OK;
	}
	FD_Drop(ent, f, res);
}

void FD_PowerStarted(gentity_t *ent, int power) {
	fdClient_t *f = FD_Get(ent);
	if (!f || power < 0 || power >= NUM_FORCE_POWERS)
		return;
	f->startedMask |= 1 << power;
	f->btnStarted |= 1 << power;
}

void FD_PowerStopped(gentity_t *ent, int power, int wasActive) {
	fdClient_t *f = FD_Get(ent);
	if (!f || power < 0 || power >= NUM_FORCE_POWERS || !(wasActive & (1 << power)))
		return;
	f->stoppedMask |= 1 << power;
	f->btnStopped |= 1 << power;
}

static const int btnBits[4] = { FDB_GRIP, FDB_LIGHT, FDB_DRAIN, FDB_FORCE };
static const char *btnNames[4] = { "button-grip", "button-lightning", "button-drain", "button-useforce" };

// once per frame from WP_ForcePowersUpdate: the previous frame's results for open button presses, then new press edges
void FD_ButtonsFrame(gentity_t *ent, const usercmd_t *ucmd) {
	fdClient_t *f = FD_Get(ent);
	gclient_t *cl;
	int k, btn = 0;

	if (!f)
		return;
	cl = ent->client;
	if (ucmd->buttons & BUTTON_FORCEGRIP) btn |= FDB_GRIP;
	if (ucmd->buttons & BUTTON_FORCE_LIGHTNING) btn |= FDB_LIGHT;
	if (ucmd->buttons & BUTTON_FORCE_DRAIN) btn |= FDB_DRAIN;
	if (ucmd->buttons & BUTTON_FORCEPOWER) btn |= FDB_FORCE;

	for (k = 0; k < 4; k++) {
		fdPress_t *p = &f->bp[k];
		int bit;
		if (!p->active)
			continue;
		bit = (p->power >= 0 && p->power < NUM_FORCE_POWERS) ? 1 << p->power : 0;
		if (bit && (f->btnStarted & bit)) {
			p->result = FDP_OK;
			p->attempts++;
			FD_Finalize(ent, f, p);
			continue;
		}
		if (bit && (f->btnStopped & bit)) {
			p->result = FDP_OFF;
			p->attempts++;
			FD_Finalize(ent, f, p);
			continue;
		}
		if (!p->stateSet) { // first frame after the edge with nothing started
			Q_strncpyz(p->reason, FD_WhyRefused(ent, p->power), sizeof(p->reason));
			FD_StateString(ent, f, p->power, qfalse, p->state, sizeof(p->state));
			p->stateSet = qtrue;
		}
		if (!(btn & btnBits[k]) || level.time - p->start > 1500) {
			p->result = FDP_REFUSED;
			p->attempts++;
			FD_Finalize(ent, f, p);
		}
	}
	f->btnStarted = f->btnStopped = 0;

	for (k = 0; k < 4; k++) {
		int power;
		if (!(btn & btnBits[k]) || (f->lastBtn & btnBits[k]))
			continue;
		power = k == 0 ? FP_GRIP : k == 1 ? FP_LIGHTNING : k == 2 ? FP_DRAIN : cl->ps.fd.forcePowerSelected;
		if (k == 3 && power == FP_LEVITATION)
			continue;
		FD_Finalize(ent, f, &f->bp[k]);
		FD_OpenPress(ent, &f->bp[k], 0, power, k, btnNames[k]);
	}
	f->lastBtn = btn;
}

// once per client frame: closes bind presses no longer seen, logs force state the server changed
void FD_EndFrame(gentity_t *ent) {
	fdClient_t *f = FD_Get(ent);
	playerState_t *ps;
	int k;
	qboolean levelsChanged = qfalse;

	if (!f)
		return;
	ps = &ent->client->ps;
	if (f->gp.active && level.time - f->gp.lastSeen > 250)
		FD_Finalize(ent, f, &f->gp);
	for (k = 0; k < 4; k++) {
		if (f->bp[k].active && level.time - f->bp[k].start > 2500) {
			if (!f->bp[k].reason[0])
				Q_strncpyz(f->bp[k].reason, "force-update-not-running", sizeof(f->bp[k].reason));
			FD_Finalize(ent, f, &f->bp[k]);
		}
	}

	for (k = 0; k < NUM_FORCE_POWERS; k++) {
		if (ps->fd.forcePowerLevel[k] != f->stLevels[k])
			levelsChanged = qtrue;
	}
	if (!f->stInit || ps->fd.forcePowersKnown != f->stKnown || levelsChanged) {
		char known[256];
		PowerList(ps->fd.forcePowersKnown, known, sizeof(known), ps->fd.forcePowerLevel);
		FD_Log(ent, f, "state known %s sel %s team %d class %s", known, PowerName(ps->fd.forcePowerSelected),
			ent->client->sess.sessionTeam, ent->client->siegeClass >= 0 ? bgSiegeClasses[ent->client->siegeClass].name : "-");
		f->stSel = ps->fd.forcePowerSelected;
	}
	else if (ps->fd.forcePowerSelected != f->stSel) {
		// a change the client asked for is only kept for forcebug; one the server made is logged
		if (ps->fd.forcePowerSelected != f->lastSel)
			FD_Log(ent, f, "selection set by server %s -> %s (client asks %s)", PowerName(f->stSel),
				PowerName(ps->fd.forcePowerSelected), PowerName(f->lastSel));
		f->stSel = ps->fd.forcePowerSelected;
	}
	if (f->autoDumpPending) {
		int ms = trap_Milliseconds();
		f->autoDumpPending = qfalse;
		if (ms >= f->nextAutoMs && !FD_QueueBusy()) {
			f->nextAutoMs = ms + 10000;
			FD_Dump(ent, f, FD_AUTO_MS, "auto (press dropped)");
		}
	}

	f->stInit = qtrue;
	f->stKnown = ps->fd.forcePowersKnown;
	for (k = 0; k < NUM_FORCE_POWERS; k++)
		f->stLevels[k] = ps->fd.forcePowerLevel[k];
}

static const char *BtnString(int btn) {
	static char s[32];
	s[0] = '\0';
	if (btn & FDB_GRIP) Q_strcat(s, sizeof(s), "G");
	if (btn & FDB_LIGHT) Q_strcat(s, sizeof(s), "L");
	if (btn & FDB_DRAIN) Q_strcat(s, sizeof(s), "D");
	if (btn & FDB_FORCE) Q_strcat(s, sizeof(s), "F");
	if (btn & FDB_USE) Q_strcat(s, sizeof(s), "U");
	if (!s[0]) Q_strncpyz(s, "-", sizeof(s));
	return s;
}

static const char *GenString(int gen) {
	int p = GenPower(gen, NULL);
	if (!gen)
		return "-";
	if (p >= 0)
		return powerNames[p];
	return va("gen%d", gen);
}

// ordinary cadence collapses into one line; any change in force input or outcome, or a timing gap, starts a new one
static qboolean FD_SameRun(const fdCmd_t *a, const fdCmd_t *b) {
	return a->gen == b->gen && a->sel == b->sel && a->btn == b->btn && a->res == b->res &&
		a->msec > 0 && a->msec <= 40 && b->msec > 0 && b->msec <= 40;
}

static void FD_Dump(gentity_t *ent, fdClient_t *f, int windowMs, const char *why) {
	gclient_t *cl = ent->client;
	playerState_t *ps = &cl->ps;
	char buf[900], known[256], act[128], userinfo[MAX_INFO_STRING];
	int i, k, lines = 0, from, n;

	FD_QueueLine(ent, va("dump begin: %s, last %d s", why, windowMs / 1000));

	PowerList(ps->fd.forcePowersKnown, known, sizeof(known), ps->fd.forcePowerLevel);
	PowerList(ACTIVE_MASK(ps), act, sizeof(act), NULL);
	FD_QueueLine(ent, va("dump state: team %d class %s hp %d/%d fp %d known %s sel %s active %s",
		cl->sess.sessionTeam, cl->siegeClass >= 0 ? bgSiegeClasses[cl->siegeClass].name : "-",
		ps->stats[STAT_HEALTH], ps->stats[STAT_MAX_HEALTH], ps->fd.forcePower, known,
		PowerName(ps->fd.forcePowerSelected), act));
	buf[0] = '\0';
	for (k = 0; k < NUM_FORCE_POWERS; k++) {
		if (ps->fd.forcePowerDuration[k] > level.time)
			Q_strcat(buf, sizeof(buf), va(" %s-runs-%dms", powerNames[k], ps->fd.forcePowerDuration[k] - level.time));
		if (ps->fd.forcePowerDebounce[k] > level.time)
			Q_strcat(buf, sizeof(buf), va(" %s-debounce-%dms", powerNames[k], ps->fd.forcePowerDebounce[k] - level.time));
	}
	FD_QueueLine(ent, va("dump timers:%s", buf[0] ? buf : " -"));
	FD_QueueLine(ent, va("dump gencmd debounce (ms since last accepted): heal %d speed %d trick %d rage %d protect %d absorb %d sight %d",
		level.time - cl->genCmdDebounce[GENCMD_DELAY_HEAL], level.time - cl->genCmdDebounce[GENCMD_DELAY_SPEED],
		level.time - cl->genCmdDebounce[GENCMD_DELAY_TRICK], level.time - cl->genCmdDebounce[GENCMD_DELAY_RAGE],
		level.time - cl->genCmdDebounce[GENCMD_DELAY_PROTECT], level.time - cl->genCmdDebounce[GENCMD_DELAY_ABSORB],
		level.time - cl->genCmdDebounce[GENCMD_DELAY_SEEING]));
	FD_QueueLine(ent, va("dump flags: needrelease %d emoted %d tempspec %d siegeduel %d duel %d saberlock %d vehicle %d emplaced %d ysalamiri %d objective %d handextend %d weapontime %d pushcooldown %d gripping %d gripped %d fjDidJump %d pause %d",
		ps->fd.forceButtonNeedRelease, cl->emoted, cl->tempSpectate > level.time ? cl->tempSpectate - level.time : 0,
		cl->sess.siegeDuelInProgress, ps->duelInProgress, (ps->saberLockFrame || ps->saberLockTime > level.time),
		ps->m_iVehicleNum, ps->weapon == WP_EMPLACED_GUN, BG_HasYsalamiri(g_gametype.integer, ps), cl->holdingObjectiveItem,
		ps->forceHandExtend, ps->weaponTime, ps->powerups[PW_DISINT_4] > level.time ? ps->powerups[PW_DISINT_4] - level.time : 0,
		ps->fd.forceGripEntityNum != ENTITYNUM_NONE ? ps->fd.forceGripEntityNum : -1, ps->fd.forceGripBeingGripped > level.time,
		cl->fjDidJump, level.pause.state));

	trap_GetUserinfo(ent - g_entities, userinfo, sizeof(userinfo));
	FD_QueueLine(ent, va("dump client: nm_ver %s ja_guid %s ping %d packet-interval %.1f jitter %.1f cmd-latency-low %.1f outliers %d last-real-cmd %dms-ago held %d tickfix %d input %d client %d saberorder %d",
#ifdef NEWMOD_SUPPORT
		cl->sess.nmVer[0] ? cl->sess.nmVer : "-",
#else
		"?",
#endif
		Info_ValueForKey(userinfo, "ja_guid")[0] ? "yes" : "no", ps->ping, cl->pers.packetIntervalAvg, cl->pers.packetJitterAvg,
		cl->pers.cmdLatencyLow, cl->pers.cmdLatencyOutliers, level.time - cl->lastRealCmdTime, cl->pers.numHeldCmds,
		trap_Cvar_VariableIntegerValue("g_tickFix"), trap_Cvar_VariableIntegerValue("g_tickFixInput"),
		trap_Cvar_VariableIntegerValue("g_tickFixClient"), trap_Cvar_VariableIntegerValue("g_tickFixSaberOrder")));

	// presses and their results
	n = f->histCount;
	for (i = 0; i < n; i++) {
		int idx = (f->histHead - n + i + FD_HIST) % FD_HIST;
		if (level.time - f->histTime[idx] > windowMs || level.time < f->histTime[idx])
			continue;
		FD_QueueLine(ent, va("dump press -%.2fs %s", (level.time - f->histTime[idx]) / 1000.0f, f->hist[idx]));
	}

	// selection requests
	n = f->cycCount;
	for (i = 0; i < n; i++) {
		int idx = (f->cycHead - n + i + FD_HIST) % FD_HIST;
		if (level.time - f->cyc[idx].t > windowMs || level.time < f->cyc[idx].t)
			continue;
		FD_QueueLine(ent, va("dump select -%.2fs %s -> %s %s", (level.time - f->cyc[idx].t) / 1000.0f,
			PowerName(f->cyc[idx].from), PowerName(f->cyc[idx].req), f->cyc[idx].ok ? "accepted" : "ignored"));
	}

	// usercmds, oldest first; when there are too many runs the oldest are left out
	n = f->ringCount;
	from = (f->ringHead - n + FD_RING) % FD_RING;
	{
		int runs = 0, skip;
		for (i = 0; i < n; ) {
			const fdCmd_t *a = &f->ring[(from + i) % FD_RING];
			int j = i + 1;
			if (level.time - a->lt > windowMs || level.time < a->lt) {
				i++;
				continue;
			}
			while (j < n && FD_SameRun(a, &f->ring[(from + j) % FD_RING]))
				j++;
			runs++;
			i = j;
		}
		skip = runs > FD_DUMP_LINES ? runs - FD_DUMP_LINES : 0;
		if (skip)
			FD_QueueLine(ent, va("dump cmd ... %d older runs left out", skip));
		lines = -skip;
	}
	for (i = 0; i < n; ) {
		const fdCmd_t *a = &f->ring[(from + i) % FD_RING], *last = a;
		int j = i + 1, minMs = a->msec, maxMs = a->msec, forced = !!(a->flags & FDF_FORCED), held = !!(a->flags & FDF_HELD);
		if (level.time - a->lt > windowMs || level.time < a->lt) {
			i++;
			continue;
		}
		while (j < n) {
			const fdCmd_t *b = &f->ring[(from + j) % FD_RING];
			if (!FD_SameRun(a, b))
				break;
			if (b->msec < minMs) minMs = b->msec;
			if (b->msec > maxMs) maxMs = b->msec;
			if (b->flags & FDF_FORCED) forced++;
			if (b->flags & FDF_HELD) held++;
			last = b;
			j++;
		}
		if (++lines <= 0) {
			i = j;
			continue;
		}
		FD_QueueLine(ent, va("dump cmd -%.3fs..-%.3fs n %d msec %d..%d st %d..%d btn %s gen %s sel %s forced %d held %d %s",
			(level.time - a->lt) / 1000.0f, (level.time - last->lt) / 1000.0f, j - i, minMs, maxMs, a->st, last->st,
			BtnString(a->btn), GenString(a->gen), PowerName(a->sel), forced, held, resNames[a->res]));
		i = j;
	}
	FD_QueueLine(ent, "dump end (usercmds the engine discarded never reach this log: they show as gaps in st)");
}

void Cmd_ForceBug_f(gentity_t *ent) {
	fdClient_t *f;
	int ms = trap_Milliseconds(), k;
	char note[256];

	if (!ent || !ent->client)
		return;
	if (!d_debugForcePowers.integer) {
		PrintIngame(ent - g_entities, "Force debug logging is off on this server.\n");
		return;
	}
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
		PrintIngame(ent - g_entities, "forcebug is only for players in the game.\n");
		return;
	}
	f = FD_Get(ent);
	if (!f)
		return;
	if (ms < f->nextBugMs) {
		PrintIngame(ent - g_entities, "You can use forcebug again in %d seconds.\n", (f->nextBugMs - ms + 999) / 1000);
		return;
	}
	if (ms < fdNextDumpMs || FD_QueueBusy()) {
		PrintIngame(ent - g_entities, "Someone else just used forcebug. Try again in %d seconds.\n", fdNextDumpMs > ms ? (fdNextDumpMs - ms + 999) / 1000 : 1);
		return;
	}
	f->nextBugMs = ms + 30000;
	fdNextDumpMs = ms + 5000;

	Q_strncpyz(note, trap_Argc() > 1 ? ConcatArgs(1) : "", sizeof(note));
	for (k = 0; note[k]; k++) {
		if (note[k] == '\n' || note[k] == '\r')
			note[k] = ' ';
	}
	FD_QueueLine(ent, va("FORCEBUG reported: \"%s\"", note));
	FD_Finalize(ent, f, &f->gp);
	for (k = 0; k < 4; k++)
		FD_Finalize(ent, f, &f->bp[k]);
	FD_Dump(ent, f, FD_DUMP_MS, "forcebug");
	PrintIngame(ent - g_entities, "Force info logged. Thanks!\n");
}
