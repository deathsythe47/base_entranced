// leave this line at the top of all AI_xxxx.cpp files for PCH reasons...
#include "g_headers.h"

	    
#include "b_local.h"

extern void G_GetBoltPosition( gentity_t *self, int boltIndex, vec3_t pos, int modelIndex );

// These define the working combat range for these suckers
#define MIN_DISTANCE		128
#define MIN_DISTANCE_SQR	( MIN_DISTANCE * MIN_DISTANCE )

#define MAX_DISTANCE		1024
#define MAX_DISTANCE_SQR	( MAX_DISTANCE * MAX_DISTANCE )

// g_fixRancor: sight, hearing and chase range
#define FIXRANCOR_RANGE		2048
#define RANCOR_RANGE		( g_fixRancor.integer ? FIXRANCOR_RANGE : MAX_DISTANCE )

#define LSTATE_CLEAR		0
#define LSTATE_WAITING		1

void Rancor_SetBolts( gentity_t *self )
{
	if ( self && self->client )
	{
		renderInfo_t *ri = &self->client->renderInfo;
		ri->handRBolt = trap_G2API_AddBolt( self->ghoul2, 0, "*r_hand" );
		ri->handLBolt = trap_G2API_AddBolt( self->ghoul2, 0, "*l_hand" );
		ri->headBolt = trap_G2API_AddBolt( self->ghoul2, 0, "*head_eyes" );
		ri->torsoBolt = trap_G2API_AddBolt( self->ghoul2, 0, "jaw_bone" );
	}
}

/*
-------------------------
NPC_Rancor_Precache
-------------------------
*/
void NPC_Rancor_Precache( void )
{
	int i;
	for ( i = 1; i < 3; i ++ )
	{
		G_SoundIndex( va("sound/chars/rancor/snort_%d.wav", i) );
	}
	G_SoundIndex( "sound/chars/rancor/swipehit.wav" );
	G_SoundIndex( "sound/chars/rancor/chomp.wav" );
}


/*
-------------------------
Rancor_Idle
-------------------------
*/
void Rancor_Idle( void )
{
	NPCInfo->localState = LSTATE_CLEAR;

	//If we have somewhere to go, then do that
	if ( UpdateGoal() )
	{
		ucmd.buttons &= ~BUTTON_WALKING;
		NPC_MoveToGoal( qtrue );
	}
}


qboolean Rancor_CheckRoar( gentity_t *self )
{
	if ( !self->wait )
	{//haven't ever gotten mad yet
		self->wait = 1;//do this only once
		self->client->ps.eFlags2 |= EF2_ALERTED;
		if ( g_fixRancor.integer )
		{//no first-sight roar: get straight to work
			return qfalse;
		}
		NPC_SetAnim( self, SETANIM_BOTH, BOTH_STAND1TO2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
		TIMER_Set( self, "rageTime", self->client->ps.legsTimer );
		return qtrue;
	}
	return qfalse;
}
/*
-------------------------
Rancor_PickTarget

g_fixRancor targeting: anyone valid within FIXRANCOR_RANGE in the same area (PVS) counts, seen or only heard.
priority: visible and in front of me > visible behind or beside me > heard only; nearest wins within a tier
-------------------------
*/
static gentity_t *Rancor_PickTarget( void )
{
	int			entNums[MAX_GENTITIES];
	vec3_t		mins, maxs;
	int			i, num;
	gentity_t	*best = NULL;
	int			bestTier = 3;
	float		bestDistSq = 0;
	const float	rangeSq = (float)FIXRANCOR_RANGE * (float)FIXRANCOR_RANGE;

	for ( i = 0; i < 3; i++ )
	{
		mins[i] = NPC->r.currentOrigin[i] - FIXRANCOR_RANGE;
		maxs[i] = NPC->r.currentOrigin[i] + FIXRANCOR_RANGE;
	}
	num = trap_EntitiesInBox( mins, maxs, entNums, MAX_GENTITIES );

	for ( i = 0; i < num; i++ )
	{
		gentity_t *ent = &g_entities[entNums[i]];
		float distSq;
		int tier;

		if ( ent == NPC || !ent->client || !NPC_ValidEnemy( ent ) )
			continue;
		if ( (ent->s.eFlags & EF_NODRAW) || ent->client->tempSpectate >= level.time )
			continue;
		if ( G_RancorBoxActive( NPC ) && !G_RancorBoxEnemyAllowed( ent->r.currentOrigin ) )
			continue;
		distSq = DistanceSquared( ent->r.currentOrigin, NPC->r.currentOrigin );
		if ( distSq > rangeSq )
			continue;
		if ( !trap_InPVS( ent->r.currentOrigin, NPC->r.currentOrigin ) )
			continue;

		if ( NPC_ClearLOS4( ent ) )
			tier = InFOV( ent, NPC, 90, 60 ) ? 0 : 1;
		else
			tier = 2;//heard

		if ( tier < bestTier || (tier == bestTier && distSq < bestDistSq) )
		{
			best = ent;
			bestTier = tier;
			bestDistSq = distSq;
		}
	}
	return best;
}

// g_fixRancor: for two seconds after losing sight of its enemy it still knows where he is (so it doesn't lose people
// who duck behind a pillar), and keeps going straight for him; after that, normal pathing and retargeting apply
#define RANCOR_ENEMY_MEMORY_MS	2000

static qboolean Rancor_CanSenseEnemy( void )
{
	if ( NPC_ClearLOS4( NPC->enemy ) )
	{
		NPC->rancorEnemySeenTime = level.time;
		NPC->rancorEnemySeenNum = NPC->enemy->s.number + 1;
		return qtrue;
	}
	return (qboolean)( g_fixRancor.integer && NPC->rancorEnemySeenNum == NPC->enemy->s.number + 1 &&
		level.time - NPC->rancorEnemySeenTime <= RANCOR_ENEMY_MEMORY_MS );
}

static qboolean Rancor_RememberingHiddenEnemy( void )
{
	return (qboolean)( g_fixRancor.integer && NPC->enemy && NPC->rancorEnemySeenNum == NPC->enemy->s.number + 1 &&
		level.time - NPC->rancorEnemySeenTime <= RANCOR_ENEMY_MEMORY_MS && !NPC_ClearLOS4( NPC->enemy ) );
}

// g_korriRancorBox: every goal is clamped into the box so pathing never takes it out, and at the edge it just
// turns to face the goal
#define RANCORBOX_GOAL_INSET	16.0f

static qboolean Rancor_MoveToGoal( qboolean tryStraight )
{
	vec3_t clamped;

	if ( !G_RancorBoxActive( NPC ) || !NPCInfo->goalEntity )
		return NPC_MoveToGoal( tryStraight );

	G_RancorBoxClamp( NPCInfo->goalEntity->r.currentOrigin, RANCORBOX_GOAL_INSET, clamped );
	if ( clamped[0] == NPCInfo->goalEntity->r.currentOrigin[0] && clamped[1] == NPCInfo->goalEntity->r.currentOrigin[1] )
		return NPC_MoveToGoal( tryStraight );

	if ( sqrt( (clamped[0] - NPC->r.currentOrigin[0]) * (clamped[0] - NPC->r.currentOrigin[0]) + (clamped[1] - NPC->r.currentOrigin[1]) * (clamped[1] - NPC->r.currentOrigin[1]) ) < 24.0f )
	{//already at the edge nearest the goal: stand and face it
		vec3_t dir, angles;
		VectorSubtract( NPCInfo->goalEntity->r.currentOrigin, NPC->r.currentOrigin, dir );
		dir[2] = 0;
		if ( VectorNormalize( dir ) >= 1.0f )
		{
			vectoangles( dir, angles );
			NPCInfo->desiredPitch = 0.0f;
			NPCInfo->desiredYaw = AngleNormalize360( angles[YAW] );
		}
		ucmd.forwardmove = ucmd.rightmove = 0;
		return qtrue;
	}

	NPC_SetMoveGoal( NPC, clamped, 16, qtrue, -1, NULL );
	return NPC_MoveToGoal( tryStraight );
}

/*
-------------------------
Rancor_Patrol
-------------------------
*/
void Rancor_Patrol( void )
{
	NPCInfo->localState = LSTATE_CLEAR;

	//If we have somewhere to go, then do that
	if ( UpdateGoal() )
	{
		ucmd.buttons &= ~BUTTON_WALKING;
		Rancor_MoveToGoal( qtrue );
	}
	else
	{
		if ( TIMER_Done( NPC, "patrolTime" ))
		{
			TIMER_Set( NPC, "patrolTime", crandom() * 5000 + 5000 );
		}
	}

	if ( g_fixRancor.integer )
	{
		gentity_t *target = Rancor_PickTarget();
		if ( target )
			G_SetEnemy( NPC, target );
		if ( !NPC->enemy )
		{
			Rancor_Idle();
			return;
		}
	}
	else if ( NPC_CheckEnemyExt( qtrue ) == qfalse )
	{
		Rancor_Idle();
		return;
	}
	Rancor_CheckRoar( NPC );
	TIMER_Set( NPC, "lookForNewEnemy", Q_irand( 5000, 15000 ) );
}
 
/*
-------------------------
Rancor_Move
-------------------------
*/
// g_fixRancor: when pathing finds no route, NPC_MoveToGoal only turns to face the goal and the rancor stands still.
// walk straight at the enemy instead and let collision slide it along walls
static void Rancor_MoveStraight( void )
{
	vec3_t dir, angles, goal;

	VectorCopy( NPC->enemy->r.currentOrigin, goal );
	if ( G_RancorBoxActive( NPC ) )
		G_RancorBoxClamp( NPC->enemy->r.currentOrigin, RANCORBOX_GOAL_INSET, goal );
	VectorSubtract( goal, NPC->r.currentOrigin, dir );
	dir[2] = 0;
	if ( VectorNormalize( dir ) < (G_RancorBoxActive( NPC ) ? 24.0f : 1.0f) )
		return;
	vectoangles( dir, angles );
	NPCInfo->desiredPitch = 0.0f;
	NPCInfo->desiredYaw = AngleNormalize360( angles[YAW] );
	NPC->client->ps.speed = NPCInfo->stats.runSpeed;
	ucmd.buttons &= ~BUTTON_WALKING;
	ucmd.forwardmove = 127;
}

void Rancor_Move( qboolean visible )
{
	if ( NPCInfo->localState != LSTATE_WAITING )
	{
		NPCInfo->goalEntity = NPC->enemy;
		if ( !Rancor_MoveToGoal( qtrue ) )
		{
			NPCInfo->consecutiveBlockedMoves++;
			if ( g_fixRancor.integer )
				Rancor_MoveStraight();
		}
		else
		{
			NPCInfo->consecutiveBlockedMoves = 0;
		}
		NPCInfo->goalRadius = RANCOR_RANGE;	// just get us within combat range
	}
}

//---------------------------------------------------------
extern void G_Knockdown( gentity_t *victim );
extern void G_Dismember( gentity_t *ent, gentity_t *enemy, vec3_t point, int limbType, float limbRollBase, float limbPitchBase, int deathAnim, qboolean postDeath );
extern float NPC_EntRangeFromBolt( gentity_t *targEnt, int boltIndex );
extern int NPC_GetEntsNearBolt( int *radiusEnts, float radius, int boltIndex, vec3_t boltOrg );

// extra victims grabbed in the same swipe as self->activator, still validly held by self
static int Rancor_ExtraVictims( gentity_t *self, gentity_t **out )
{
	int i, n = 0;
	for ( i = 0; i < self->numRancorExtraVictims; i++ )
	{
		gentity_t *v = &g_entities[self->rancorExtraVictims[i]];
		if ( v->inuse && v->client && (v->client->ps.eFlags2&EF2_HELD_BY_MONSTER) && v->client->heldByRancor == self->s.number + 1 )
			out[n++] = v;
	}
	return n;
}

// the held victim (activator) followed by every extra victim
static int Rancor_AllVictims( gentity_t *self, gentity_t **out )
{
	if ( !self->activator )
		return 0;
	out[0] = self->activator;
	return 1 + Rancor_ExtraVictims( self, out + 1 );
}

static void Rancor_ReleaseVictim( gentity_t *self, gentity_t *victim )
{
	if ( victim->client )
	{
		victim->client->ps.eFlags2 &= ~EF2_HELD_BY_MONSTER;
		victim->client->heldByRancor = 0;
		if ( victim == self->activator )
		{
			victim->client->ps.hasLookTarget = qfalse;
			victim->client->ps.lookTarget = ENTITYNUM_NONE;
		}
		victim->client->ps.viewangles[ROLL] = 0;
		SetClientViewAngle( victim, victim->client->ps.viewangles );
		victim->r.currentAngles[PITCH] = victim->r.currentAngles[ROLL] = 0;
		G_SetAngles( victim, victim->r.currentAngles );
	}
	if ( victim->health <= 0 )
	{
		//if ( victim->s.number )
		{//never free player
			if ( self->count == 1 )
			{//in my hand, just drop them
				if ( victim->client )
				{
					victim->client->ps.legsTimer = victim->client->ps.torsoTimer = 0;
					//FIXME: ragdoll?
				}
			}
			else
			{
				if ( victim->client )
				{
					victim->client->ps.eFlags |= EF_NODRAW;//so his corpse doesn't drop out of me...
				}
			}
		}
	}
	else
	{
		if ( victim->NPC )
		{//start thinking again
			victim->NPC->nextBStateThink = level.time;
		}
		//clear their anim and let them fall
		victim->client->ps.legsTimer = victim->client->ps.torsoTimer = 0;
	}
	if ( self->enemy == victim )
	{
		self->enemy = NULL;
	}
}

void Rancor_DropVictim( gentity_t *self )
{
	//FIXME: if Rancor dies, it should drop its victim.
	//FIXME: if Rancor is removed, it must remove its victim.
	if ( self->activator )
	{
		gentity_t *extras[MAX_CLIENTS];
		int i, n = Rancor_ExtraVictims( self, extras );
		Rancor_ReleaseVictim( self, self->activator );
		for ( i = 0; i < n; i++ )
			Rancor_ReleaseVictim( self, extras[i] );
		self->activator = NULL;
	}
	self->numRancorExtraVictims = 0;
	self->count = 0;//drop him
}

static qboolean Rancor_CanGrab( gentity_t *ent )
{
	return (qboolean)( ent->client->NPC_class != CLASS_RANCOR
		&& ent->client->NPC_class != CLASS_GALAKMECH
		&& ent->client->NPC_class != CLASS_ATST
		&& ent->client->NPC_class != CLASS_GONK
		&& ent->client->NPC_class != CLASS_R2D2
		&& ent->client->NPC_class != CLASS_R5D2
		&& ent->client->NPC_class != CLASS_MARK1
		&& ent->client->NPC_class != CLASS_MARK2
		&& ent->client->NPC_class != CLASS_MOUSE
		&& ent->client->NPC_class != CLASS_PROBE
		&& ent->client->NPC_class != CLASS_SEEKER
		&& ent->client->NPC_class != CLASS_REMOTE
		&& ent->client->NPC_class != CLASS_SENTRY
		&& ent->client->NPC_class != CLASS_INTERROGATOR
		&& ent->client->NPC_class != CLASS_VEHICLE );
}

// set up a freshly grabbed victim (both the main one and extras)
static void Rancor_GrabVictimCommon( gentity_t *radiusEnt )
{
	radiusEnt->client->ps.eFlags2 |= EF2_HELD_BY_MONSTER;
	if ( g_fixRancor.integer && radiusEnt->client )
	{//scream in terror as the rancor picks you up -- no more regular pain sounds until you die
		G_EntitySound( radiusEnt, CHAN_VOICE, G_SoundIndex("*falling1.wav") );
	}
	if ( radiusEnt->health > 0 && radiusEnt->pain )
	{//do pain on enemy
		radiusEnt->pain( radiusEnt, NPC, 100 );
	}
	else if ( radiusEnt->client )
	{
		radiusEnt->client->ps.forceHandExtend = HANDEXTEND_NONE;
		radiusEnt->client->ps.forceHandExtendTime = 0;
		NPC_SetAnim( radiusEnt, SETANIM_BOTH, BOTH_SWIM_IDLE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
	}
}

void Rancor_Swing( qboolean tryGrab )
{
	int			radiusEntNums[128];
	int			numEnts;
	const float	radius = 88;
	const float	radiusSquared = (radius*radius);
	int			i;
	vec3_t		boltOrg;

	numEnts = NPC_GetEntsNearBolt( radiusEntNums, radius, NPC->client->renderInfo.handRBolt, boltOrg );

	for ( i = 0; i < numEnts; i++ )
	{
		gentity_t *radiusEnt = &g_entities[radiusEntNums[i]];
		if ( !radiusEnt->inuse )
		{
			continue;
		}
		
		if ( radiusEnt == NPC )
		{//Skip the rancor ent
			continue;
		}
		
		if ( radiusEnt->client == NULL )
		{//must be a client
			continue;
		}

		if ( (radiusEnt->client->ps.eFlags2&EF2_HELD_BY_MONSTER) )
		{//can't be one already being held
			continue;
		}
		
		if ( DistanceSquared( radiusEnt->r.currentOrigin, boltOrg ) <= radiusSquared )
		{
			if ( tryGrab 
				&& NPC->count != 1 //don't have one in hand or in mouth already - FIXME: allow one in hand and any number in mouth!
				&& Rancor_CanGrab( radiusEnt ) )
			{//grab
				if ( NPC->count == 2 )
				{//have one in my mouth, remove him
					TIMER_Remove( NPC, "clearGrabbed" );
					Rancor_DropVictim( NPC );
				}
				NPC->enemy = radiusEnt;//make him my new best friend
				NPC->rancorHasBitten = qfalse;//g_fixRancor: exactly one quick bite before the kill on this victim
				//FIXME: this makes it so that the victim can't hit us with shots!  Just use activator or something
				radiusEnt->client->ps.hasLookTarget = qtrue;
				radiusEnt->client->ps.lookTarget = NPC->s.number;
				radiusEnt->client->heldByRancor = NPC->s.number + 1;
				radiusEnt->client->heldByRancorSlot = 0;
				NPC->activator = radiusEnt;//remember him
				NPC->count = 1;//in my hand
				NPC->numRancorExtraVictims = 0;
				//wait to attack
				TIMER_Set( NPC, "attacking", NPC->client->ps.legsTimer + Q_irand(500, 2500) );
				Rancor_GrabVictimCommon( radiusEnt );
			}
			else if ( tryGrab
				&& NPC->count == 1 && NPC->activator
				&& NPC->client->ps.legsAnim == BOTH_ATTACK2
				&& NPC->numRancorExtraVictims < MAX_CLIENTS
				&& Rancor_CanGrab( radiusEnt ) )
			{//same swipe, already holding someone: grab this one too, next to him in my hand
				// no lookTarget, so clients draw them at the server origin G_HeldByMonster offsets for them
				radiusEnt->client->ps.hasLookTarget = qfalse;//a stale saber lookTarget would make clients attach him to that player
				radiusEnt->client->ps.lookTarget = ENTITYNUM_NONE;
				radiusEnt->client->heldByRancor = NPC->s.number + 1;
				radiusEnt->client->heldByRancorSlot = NPC->numRancorExtraVictims + 1;
				NPC->rancorExtraVictims[NPC->numRancorExtraVictims++] = radiusEnt->s.number;
				Rancor_GrabVictimCommon( radiusEnt );
			}
			else
			{//smack
				vec3_t pushDir;
				vec3_t angs;

				G_Sound( radiusEnt, CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/swipehit.wav" ) );
				//actually push the enemy
				VectorCopy( NPC->client->ps.viewangles, angs );
				angs[YAW] += flrand( 25, 50 );
				angs[PITCH] = flrand( -25, -15 );
				AngleVectors( angs, pushDir, NULL, NULL );
				if ( radiusEnt->client->NPC_class != CLASS_RANCOR
					&& radiusEnt->client->NPC_class != CLASS_ATST )
				{
					G_Damage( radiusEnt, NPC, NPC, vec3_origin, radiusEnt->r.currentOrigin, Q_irand( 25, 40 ), DAMAGE_NO_ARMOR|DAMAGE_NO_KNOCKBACK, MOD_MELEE );
					G_Throw( radiusEnt, pushDir, 250 );
					if ( radiusEnt->health > 0 )
					{//do pain on enemy
						G_Knockdown( radiusEnt );
					}
				}
			}
		}
	}
}

void Rancor_Smash( void )
{
	int			radiusEntNums[128];
	int			numEnts;
	const float	radius = 128;
	const float	halfRadSquared = ((radius/2)*(radius/2));
	const float	radiusSquared = (radius*radius);
	float		distSq;
	int			i;
	vec3_t		boltOrg;

	AddSoundEvent( NPC, NPC->r.currentOrigin, 512, AEL_DANGER, qfalse );

	numEnts = NPC_GetEntsNearBolt( radiusEntNums, radius, NPC->client->renderInfo.handLBolt, boltOrg );

	for ( i = 0; i < numEnts; i++ )
	{
		gentity_t *radiusEnt = &g_entities[radiusEntNums[i]];
		if ( !radiusEnt->inuse )
		{
			continue;
		}
		
		if ( radiusEnt == NPC )
		{//Skip the rancor ent
			continue;
		}
		
		if ( radiusEnt->client == NULL )
		{//must be a client
			continue;
		}

		if ( (radiusEnt->client->ps.eFlags2&EF2_HELD_BY_MONSTER) )
		{//can't be one being held
			continue;
		}
		
		distSq = DistanceSquared( radiusEnt->r.currentOrigin, boltOrg );
		if ( distSq <= radiusSquared )
		{
			G_Sound( radiusEnt, CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/swipehit.wav" ) );
			if ( distSq < halfRadSquared )
			{//close enough to do damage, too
				G_Damage( radiusEnt, NPC, NPC, vec3_origin, radiusEnt->r.currentOrigin, Q_irand( 10, 25 ), DAMAGE_NO_ARMOR|DAMAGE_NO_KNOCKBACK, MOD_MELEE );
			}
			if ( radiusEnt->health > 0 
				&& radiusEnt->client
				&& radiusEnt->client->NPC_class != CLASS_RANCOR
				&& radiusEnt->client->NPC_class != CLASS_ATST )
			{
				if ( distSq < halfRadSquared 
					|| radiusEnt->client->ps.groundEntityNum != ENTITYNUM_NONE )
				{//within range of my fist or withing ground-shaking range and not in the air
					G_Knockdown( radiusEnt );
				}
			}
		}
	}
}

void Rancor_Bite( void )
{
	int			radiusEntNums[128];
	int			numEnts;
	const float	radius = 100;
	const float	radiusSquared = (radius*radius);
	int			i;
	vec3_t		boltOrg;

	numEnts = NPC_GetEntsNearBolt( radiusEntNums, radius, NPC->client->renderInfo.crotchBolt, boltOrg );//was gutBolt?

	for ( i = 0; i < numEnts; i++ )
	{
		gentity_t *radiusEnt = &g_entities[radiusEntNums[i]];
		if ( !radiusEnt->inuse )
		{
			continue;
		}
		
		if ( radiusEnt == NPC )
		{//Skip the rancor ent
			continue;
		}
		
		if ( radiusEnt->client == NULL )
		{//must be a client
			continue;
		}

		if ( (radiusEnt->client->ps.eFlags2&EF2_HELD_BY_MONSTER) )
		{//can't be one already being held
			continue;
		}
		
		if ( DistanceSquared( radiusEnt->r.currentOrigin, boltOrg ) <= radiusSquared )
		{
			G_Damage( radiusEnt, NPC, NPC, vec3_origin, radiusEnt->r.currentOrigin, Q_irand( 15, 30 ), DAMAGE_NO_ARMOR|DAMAGE_NO_KNOCKBACK, MOD_MELEE );
			if ( radiusEnt->health <= 0 && radiusEnt->client )
			{//killed them, chance of dismembering
				if ( !Q_irand( 0, 1 ) )
				{//bite something off
					int hitLoc = Q_irand( G2_MODELPART_HEAD, G2_MODELPART_RLEG );
					if ( hitLoc == G2_MODELPART_HEAD )
					{
						NPC_SetAnim( radiusEnt, SETANIM_BOTH, BOTH_DEATH17, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
					}
					else if ( hitLoc == G2_MODELPART_WAIST )
					{
						NPC_SetAnim( radiusEnt, SETANIM_BOTH, BOTH_DEATHBACKWARD2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
					}
					//FIXME: the limb should just disappear, cuz I ate it
					G_Dismember( radiusEnt, NPC, radiusEnt->r.currentOrigin, hitLoc, 90, 0, radiusEnt->client->ps.torsoAnim, qtrue);
				}
			}
			G_Sound( radiusEnt, CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/chomp.wav" ) );
		}
	}
}
//------------------------------
extern void TossClientItems( gentity_t *self );
void Rancor_Attack( float distance, qboolean doCharge )
{
	gentity_t *victimList[MAX_CLIENTS + 1];
	int victims, v;

	if ( !TIMER_Exists( NPC, "attacking" ) )
	{
		if ( NPC->count == 2 && NPC->activator )
		{
		}
		else if ( NPC->count == 1 && NPC->activator )
		{//holding enemy
			// stock: a coin flip each time, quick bite or eat, g_fixRancor: exactly one.
			if ( NPC->activator->health > 0 && ( g_fixRancor.integer ? !NPC->rancorHasBitten : Q_irand( 0, 1 ) ) )
			{//quick bite
				NPC->rancorHasBitten = qtrue;
				NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_ATTACK1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
				TIMER_Set( NPC, "attack_dmg", 450 );
			}
			else
			{//full eat
				NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_ATTACK3, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
				TIMER_Set( NPC, "attack_dmg", 900 );
				//Make victim scream in fright
				victims = Rancor_AllVictims( NPC, victimList );
				for ( v = 0; v < victims; v++ )
				{
					gentity_t *victim = victimList[v];
					if ( victim->health > 0 && victim->client )
					{
						G_AddEvent( victim, Q_irand(EV_DEATH1, EV_DEATH3), 0 );
						NPC_SetAnim( victim, SETANIM_TORSO, BOTH_FALLDEATH1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
						if ( victim->NPC )
						{//no more thinking for you
							TossClientItems( NPC );
							victim->NPC->nextBStateThink = Q3_INFINITE;
						}
					}
				}
			}
		}
		else if ( NPC->enemy->health > 0 && doCharge )
		{//charge
			if (!g_fixRancorCharge.integer || NPC_MoveToGoal(qtrue))
			{
				vec3_t	fwd, yawAng;
				VectorSet( yawAng, 0, NPC->client->ps.viewangles[YAW], 0 );
				AngleVectors( yawAng, fwd, NULL, NULL );
				VectorScale( fwd, distance*1.5f, NPC->client->ps.velocity );
				NPC->client->ps.velocity[2] = 150;
				NPC->client->ps.groundEntityNum = ENTITYNUM_NONE;

				NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_MELEE2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
				TIMER_Set( NPC, "attack_dmg", 1250 );
			}
		}
		else if ( !Q_irand(0, 1) )
		{//smash
			NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_MELEE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
			TIMER_Set( NPC, "attack_dmg", 1000 );
		}
		else
		{//try to grab
			NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_ATTACK2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
			NPC->rancorSwipeGrab = !Q_irand( 0, 2 );//g_fixRancor: one roll for everyone in this swipe
			TIMER_Set( NPC, "attack_dmg", 1000 );
		}

		TIMER_Set( NPC, "attacking", NPC->client->ps.legsTimer + random() * 200 );
	}

	// Need to do delayed damage since the attack animations encapsulate multiple mini-attacks

	if ( TIMER_Done2( NPC, "attack_dmg", qtrue ) )
	{
		vec3_t shakePos;
		switch ( NPC->client->ps.legsAnim )
		{
		case BOTH_MELEE1:
			Rancor_Smash();
			G_GetBoltPosition( NPC, NPC->client->renderInfo.handLBolt, shakePos, 0 );
			G_ScreenShake( shakePos, NULL, 4.0f, 1000, qfalse );
			break;
		case BOTH_MELEE2:
			Rancor_Bite();
			TIMER_Set( NPC, "attack_dmg2", 450 );
			break;
		case BOTH_ATTACK1:
			if ( NPC->count == 1 && NPC->activator )
			{
				victims = Rancor_AllVictims( NPC, victimList );
				for ( v = 0; v < victims; v++ )
				{
					gentity_t *victim = victimList[v];
					G_Damage( victim, NPC, NPC, vec3_origin, victim->r.currentOrigin, Q_irand( 25, 40 ), DAMAGE_NO_ARMOR|DAMAGE_NO_KNOCKBACK, MOD_MELEE );
					if ( victim->health <= 0 )
					{//killed him
						//make it look like we bit his head off
						G_Dismember( victim, NPC, victim->r.currentOrigin, G2_MODELPART_HEAD, 90, 0, victim->client->ps.torsoAnim, qtrue);
						victim->client->ps.forceHandExtend = HANDEXTEND_NONE;
						victim->client->ps.forceHandExtendTime = 0;
						NPC_SetAnim( victim, SETANIM_BOTH, BOTH_SWIM_IDLE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
					}
					G_Sound( victim, CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/chomp.wav" ) );
				}
			}
			break;
		case BOTH_ATTACK2:
			//try to grab
			Rancor_Swing( qtrue );
			break;
		case BOTH_ATTACK3:
			if ( NPC->count == 1 && NPC->activator )
			{
				victims = Rancor_AllVictims( NPC, victimList );
				for ( v = 0; v < victims; v++ )
				{
					gentity_t *victim = victimList[v];
					//cut in half
					if ( victim->client )
					{
						G_Dismember( victim, NPC, victim->r.currentOrigin, G2_MODELPART_WAIST, 90, 0, victim->client->ps.torsoAnim, qtrue);
					}
					//KILL
					G_Damage( victim, NPC, NPC, vec3_origin, victim->r.currentOrigin, (v || !NPC->enemy ? victim->health : NPC->enemy->health)+10, DAMAGE_NO_PROTECTION|DAMAGE_NO_ARMOR|DAMAGE_NO_KNOCKBACK|DAMAGE_NO_HIT_LOC, MOD_MELEE );//, HL_NONE );//
					if ( victim->client )
					{
						victim->client->ps.forceHandExtend = HANDEXTEND_NONE;
						victim->client->ps.forceHandExtendTime = 0;
						NPC_SetAnim( victim, SETANIM_BOTH, BOTH_SWIM_IDLE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
					}
					G_Sound( victim, CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/swipehit.wav" ) );
					G_AddEvent( victim, EV_JUMP, victim->health );
				}
				TIMER_Set( NPC, "attack_dmg2", 1350 );
			}
			break;
		}
	}
	else if ( TIMER_Done2( NPC, "attack_dmg2", qtrue ) )
	{
		switch ( NPC->client->ps.legsAnim )
		{
		case BOTH_MELEE1:
			break;
		case BOTH_MELEE2:
			Rancor_Bite();
			break;
		case BOTH_ATTACK1:
			break;
		case BOTH_ATTACK2:
			break;
		case BOTH_ATTACK3:
			if ( NPC->count == 1 && NPC->activator )
			{//swallow victim
				victims = Rancor_AllVictims( NPC, victimList );
				for ( v = 0; v < victims; v++ )
				{
					gentity_t *victim = victimList[v];
					G_Sound( victim, CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/chomp.wav" ) );
					//FIXME: sometimes end up with a live one in our mouths?
					//just make sure they're dead
					if ( victim->health > 0 )
					{
						//cut in half
						G_Dismember( victim, NPC, victim->r.currentOrigin, G2_MODELPART_WAIST, 90, 0, victim->client->ps.torsoAnim, qtrue);
						//KILL
						G_Damage( victim, NPC, NPC, vec3_origin, victim->r.currentOrigin, (v || !NPC->enemy ? victim->health : NPC->enemy->health)+10, DAMAGE_NO_PROTECTION|DAMAGE_NO_ARMOR|DAMAGE_NO_KNOCKBACK|DAMAGE_NO_HIT_LOC, MOD_MELEE );//, HL_NONE );
						victim->client->ps.forceHandExtend = HANDEXTEND_NONE;
						victim->client->ps.forceHandExtendTime = 0;
						NPC_SetAnim( victim, SETANIM_BOTH, BOTH_SWIM_IDLE1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
						G_AddEvent( victim, EV_JUMP, victim->health );
					}
					if ( victim->client )
					{//*sigh*, can't get tags right, just remove them?
						victim->client->ps.eFlags |= EF_NODRAW;
					}
				}
				NPC->count = 2;
				TIMER_Set( NPC, "clearGrabbed", 2600 );
			}
			break;
		}
	}
	else if ( NPC->client->ps.legsAnim == BOTH_ATTACK2 )
	{
		if ( NPC->client->ps.legsTimer >= 1200 && NPC->client->ps.legsTimer <= 1350 )
		{
			if ( g_fixRancor.integer ? !NPC->rancorSwipeGrab : Q_irand( 0, 2 ) )
			{
				Rancor_Swing( qfalse );
			}
			else
			{
				Rancor_Swing( qtrue );
			}
		}
		else if ( NPC->client->ps.legsTimer >= 1100 && NPC->client->ps.legsTimer <= 1550 )
		{
			Rancor_Swing( qtrue );
		}
	}

	// Just using this to remove the attacking flag at the right time
	TIMER_Done2( NPC, "attacking", qtrue );
}

//----------------------------------
void Rancor_Combat( void )
{
	if ( NPC->count )
	{//holding my enemy
		if ( TIMER_Done2( NPC, "takingPain", qtrue ))
		{
			NPCInfo->localState = LSTATE_CLEAR;
		}
		else
		{
			Rancor_Attack( 0, qfalse );
		}
		NPC_UpdateAngles( qtrue, qtrue );
		return;
	}
	// If we cannot see our target or we have somewhere to go, then do that
	if ( !Rancor_CanSenseEnemy() )//|| UpdateGoal( ))
	{
		NPCInfo->combatMove = qtrue;
		NPCInfo->goalEntity = NPC->enemy;
		NPCInfo->goalRadius = MIN_DISTANCE;

		if ( !Rancor_MoveToGoal( qtrue ) )
		{//couldn't go after him?  Look for a new one
			TIMER_Set( NPC, "lookForNewEnemy", 0 );
			NPCInfo->consecutiveBlockedMoves++;
		}
		else 
		{
			NPCInfo->consecutiveBlockedMoves = 0;
		}
		return;
	}

	// Sometimes I have problems with facing the enemy I'm attacking, so force the issue so I don't look dumb
	NPC_FaceEnemy( qtrue );

	{
		float	distance;
		qboolean	advance;
		qboolean	doCharge;

		distance	= Distance( NPC->r.currentOrigin, NPC->enemy->r.currentOrigin );	
		advance = (qboolean)( distance > (NPC->r.maxs[0]+MIN_DISTANCE) ? qtrue : qfalse  );
		doCharge = qfalse;

		if ( advance )
		{//have to get closer
			vec3_t	yawOnlyAngles;
			VectorSet( yawOnlyAngles, 0, NPC->r.currentAngles[YAW], 0 );
			if ( NPC->enemy->health > 0
				&& fabs(distance-250) <= 80 
				&& InFOV3( NPC->enemy->r.currentOrigin, NPC->r.currentOrigin, yawOnlyAngles, 30, 30 ) )
			{
				if ( !g_fixRancor.integer && !Q_irand( 0, 9 ) )
				{//go for the charge (g_fixRancor: never lunges)
					doCharge = qtrue;
					advance = qfalse;
				}
			}
		}

		if (( advance /*|| NPCInfo->localState == LSTATE_WAITING*/ ) && TIMER_Done( NPC, "attacking" )) // waiting monsters can't attack
		{
			if ( TIMER_Done2( NPC, "takingPain", qtrue ))
			{
				NPCInfo->localState = LSTATE_CLEAR;
			}
			else
			{
				Rancor_Move( 1 );
			}
		}
		else
		{
			Rancor_Attack( distance, doCharge );
		}
	}
}

/*
-------------------------
NPC_Rancor_Pain
-------------------------
*/
//void NPC_Rancor_Pain( gentity_t *self, gentity_t *inflictor, gentity_t *other, const vec3_t point, int damage, int mod,int hitLoc ) 
void NPC_Rancor_Pain( gentity_t *self, gentity_t *attacker, int damage ) 
{
	qboolean hitByRancor = qfalse;
	if ( attacker&&attacker->client&&attacker->client->NPC_class==CLASS_RANCOR )
	{
		hitByRancor = qtrue;
	}
	if ( attacker 
		&& attacker->inuse 
		&& attacker != self->enemy
		&& !(attacker->flags&FL_NOTARGET) )
	{
		if ( !self->count )
		{
			if ( (G_IsPlayer(attacker) &&!Q_irand(0,3))
				|| !self->enemy
				|| self->enemy->health == 0
				|| (self->enemy->client&&self->enemy->client->NPC_class == CLASS_RANCOR)
				|| (self->NPC && self->NPC->consecutiveBlockedMoves>=10 && DistanceSquared( attacker->r.currentOrigin, self->r.currentOrigin ) < DistanceSquared( self->enemy->r.currentOrigin, self->r.currentOrigin )) ) 
			{//if my enemy is dead (or attacked by player) and I'm not still holding/eating someone, turn on the attacker
				//FIXME: if can't nav to my enemy, take this guy if I can nav to him
				G_SetEnemy( self, attacker );
				TIMER_Set( self, "lookForNewEnemy", Q_irand( 5000, 15000 ) );
				if ( hitByRancor )
				{//stay mad at this Rancor for 2-5 secs before looking for attacker enemies
					TIMER_Set( self, "rancorInfight", Q_irand( 2000, 5000 ) );
				}

			}
		}
	}
	if ( (hitByRancor|| (self->count==1&&self->activator&&!Q_irand(0,4)) || Q_irand( 0, 200 ) < damage )//hit by rancor, hit while holding live victim, or took a lot of damage
		&& self->client->ps.legsAnim != BOTH_STAND1TO2
		&& TIMER_Done( self, "takingPain" ) )
	{
		if ( !Rancor_CheckRoar( self ) )
		{
			if ( self->client->ps.legsAnim != BOTH_MELEE1
				&& self->client->ps.legsAnim != BOTH_MELEE2
				&& self->client->ps.legsAnim != BOTH_ATTACK2 )
			{//cant interrupt one of the big attack anims
				/*
				if ( self->count != 1 
					|| attacker == self->activator
					|| (self->client->ps.legsAnim != BOTH_ATTACK1&&self->client->ps.legsAnim != BOTH_ATTACK3) )
				*/
				{//if going to bite our victim, only victim can interrupt that anim
					if ( self->health > 100 || hitByRancor )
					{
						TIMER_Remove( self, "attacking" );

						VectorCopy( self->NPC->lastPathAngles, self->s.angles );

						if ( self->count == 1 )
						{
							NPC_SetAnim( self, SETANIM_BOTH, BOTH_PAIN2, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
						}
						else
						{
							NPC_SetAnim( self, SETANIM_BOTH, BOTH_PAIN1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD );
						}
						TIMER_Set( self, "takingPain", self->client->ps.legsTimer+Q_irand(0, 500) );

						if ( self->NPC )
						{
							self->NPC->localState = LSTATE_WAITING;
						}
					}
				}
			}
		}
	}
}

void Rancor_CheckDropVictim( void )
{
	vec3_t mins;
	vec3_t maxs;
	vec3_t start; 
	vec3_t end; 
	trace_t	trace;

	VectorSet( mins, NPC->activator->r.mins[0]-1, NPC->activator->r.mins[1]-1, 0 );
	VectorSet( maxs, NPC->activator->r.maxs[0]+1, NPC->activator->r.maxs[1]+1, 1 );
	VectorSet( start, NPC->activator->r.currentOrigin[0], NPC->activator->r.currentOrigin[1], NPC->activator->r.absmin[2] ); 
	VectorSet( end, NPC->activator->r.currentOrigin[0], NPC->activator->r.currentOrigin[1], NPC->activator->r.absmax[2]-1 ); 

	trap_Trace( &trace, start, mins, maxs, end, NPC->activator->s.number, NPC->activator->clipmask );
	if ( !trace.allsolid && !trace.startsolid && trace.fraction >= 1.0f )
	{
		Rancor_DropVictim( NPC );
	}
}

//if he's stepping on things then crush them -rww
void Rancor_Crush(void)
{
	gentity_t *crush;

	if (!NPC ||
		!NPC->client ||
		NPC->client->ps.groundEntityNum >= ENTITYNUM_WORLD)
	{ //nothing to crush
		return;
	}

	crush = &g_entities[NPC->client->ps.groundEntityNum];
	if (crush->inuse && crush->client && !crush->localAnimIndex)
	{ //a humanoid, smash them good.
		G_Damage(crush, NPC, NPC, NULL, NPC->r.currentOrigin, 200, 0, MOD_CRUSH);
	}
}

/*
-------------------------
NPC_BSRancor_Default
-------------------------
*/
static void Rancor_Think( void )
{
	// g_fixRancor: twice the sight range and turn speed, and 10% more move speed, than its .npc stats
	if ( !NPC->rancorStockYawSpeed )
	{
		NPC->rancorStockYawSpeed = NPCInfo->stats.yawSpeed;
		NPC->rancorStockVisrange = NPCInfo->stats.visrange;
		NPC->rancorStockRunSpeed = NPCInfo->stats.runSpeed;
		NPC->rancorStockWalkSpeed = NPCInfo->stats.walkSpeed;
	}
	NPCInfo->stats.yawSpeed = g_fixRancor.integer ? NPC->rancorStockYawSpeed * 2 : NPC->rancorStockYawSpeed;
	NPCInfo->stats.visrange = g_fixRancor.integer ? FIXRANCOR_RANGE : NPC->rancorStockVisrange;
	NPCInfo->stats.runSpeed = g_fixRancor.integer ? (int)(NPC->rancorStockRunSpeed * 1.1f + 0.5f) : NPC->rancorStockRunSpeed;
	// and no walking: whenever it moves it moves at full speed, without slowing down near its goal
	NPCInfo->stats.walkSpeed = g_fixRancor.integer ? NPCInfo->stats.runSpeed : NPC->rancorStockWalkSpeed;
	if ( g_fixRancor.integer )
		NPCInfo->aiFlags |= NPCAI_NO_SLOWDOWN;
	else
		NPCInfo->aiFlags &= ~NPCAI_NO_SLOWDOWN;

	AddSightEvent( NPC, NPC->r.currentOrigin, RANCOR_RANGE, AEL_DANGER_GREAT, 50 );

	Rancor_Crush();

	NPC->client->ps.eFlags2 &= ~(EF2_USE_ALT_ANIM|EF2_GENERIC_NPC_FLAG);
	if ( NPC->count )
	{//holding someone
		NPC->client->ps.eFlags2 |= EF2_USE_ALT_ANIM;
		if ( NPC->count == 2 )
		{//in my mouth
			NPC->client->ps.eFlags2 |= EF2_GENERIC_NPC_FLAG;
		}
	}
	else
	{
		NPC->client->ps.eFlags2 &= ~(EF2_USE_ALT_ANIM|EF2_GENERIC_NPC_FLAG);
	}

	if ( TIMER_Done2( NPC, "clearGrabbed", qtrue ) )
	{
		Rancor_DropVictim( NPC );
	}
	else if ( NPC->client->ps.legsAnim == BOTH_PAIN2 
		&& NPC->count == 1 
		&& NPC->activator )
	{
		if ( !Q_irand( 0, 3 ) )
		{
			Rancor_CheckDropVictim();
		}
	}
	if ( !TIMER_Done( NPC, "rageTime" ) )
	{//do nothing but roar first time we see an enemy
		AddSoundEvent( NPC, NPC->r.currentOrigin, RANCOR_RANGE, AEL_DANGER_GREAT, qfalse );
		NPC_FaceEnemy( qtrue );
		return;
	}
	if ( NPC->enemy )
	{
		if ( TIMER_Done(NPC,"angrynoise") )
		{
			G_Sound( NPC, CHAN_AUTO, G_SoundIndex( va("sound/chars/rancor/misc/anger%d.wav", Q_irand(1, 3))) );

			TIMER_Set( NPC, "angrynoise", Q_irand( 5000, 10000 ) );
		}
		else
		{
			AddSoundEvent( NPC, NPC->r.currentOrigin, 512, AEL_DANGER_GREAT, qfalse );
		}
		if ( NPC->count == 2 && NPC->client->ps.legsAnim == BOTH_ATTACK3 )
		{//we're still chewing our enemy up
			NPC_UpdateAngles( qtrue, qtrue );
			return;
		}
		//else, if he's in our hand, we eat, else if he's on the ground, we keep attacking his dead body for a while
		if( NPC->enemy->client && NPC->enemy->client->NPC_class == CLASS_RANCOR )
		{//got mad at another Rancor, look for a valid enemy
			if ( TIMER_Done( NPC, "rancorInfight" ) )
			{
				NPC_CheckEnemyExt( qtrue );
			}
		}
		else if ( !NPC->count )
		{
			if ( ValidEnemy( NPC->enemy ) == qfalse )
			{
				TIMER_Remove( NPC, "lookForNewEnemy" );//make them look again right now
				if ( g_fixRancor.integer )
				{//no hanging around a corpse: go find somebody else right away
					NPC->lastEnemy = NPC->enemy;
					NPC->enemy = NULL;
					Rancor_Patrol();
					NPC_UpdateAngles( qtrue, qtrue );
					return;
				}
				if ( !NPC->enemy->inuse || level.time - NPC->enemy->s.time > Q_irand( 10000, 15000 ) )
				{//it's been a while since the enemy died, or enemy is completely gone, get bored with him
					NPC->enemy = NULL;
					Rancor_Patrol();
					NPC_UpdateAngles( qtrue, qtrue );
					return;
				}
			}
			if ( TIMER_Done( NPC, "lookForNewEnemy" ) && !Rancor_RememberingHiddenEnemy() )
			{//(don't trade him for someone else while we still know where he went)
				gentity_t *newEnemy, *sav_enemy = NPC->enemy;//FIXME: what about NPC->lastEnemy?
				NPC->enemy = NULL;
				if ( g_fixRancor.integer )
					newEnemy = Rancor_PickTarget();
				else
					newEnemy = NPC_CheckEnemy( NPCInfo->confusionTime < level.time, qfalse, qfalse );
				NPC->enemy = sav_enemy;
				if ( newEnemy && newEnemy != sav_enemy )
				{//picked up a new enemy!
					NPC->lastEnemy = NPC->enemy;
					G_SetEnemy( NPC, newEnemy );
					//hold this one for at least 5-15 seconds
					TIMER_Set( NPC, "lookForNewEnemy", Q_irand( 5000, 15000 ) );
				}
				else
				{//look again in 2-5 secs
					TIMER_Set( NPC, "lookForNewEnemy", Q_irand( 2000, 5000 ) );
				}
			}
		}
		Rancor_Combat();
	}
	else 
	{
		if ( TIMER_Done(NPC,"idlenoise") )
		{
			G_Sound( NPC, CHAN_AUTO, G_SoundIndex( va("sound/chars/rancor/snort_%d.wav", Q_irand(1, 2))) );

			TIMER_Set( NPC, "idlenoise", Q_irand( 2000, 4000 ) );
			AddSoundEvent( NPC, NPC->r.currentOrigin, 384, AEL_DANGER, qfalse );
		}
		if ( NPCInfo->scriptFlags & SCF_LOOK_FOR_ENEMIES )
		{
			Rancor_Patrol();
		}
		else
		{
			Rancor_Idle();
		}
	}

	NPC_UpdateAngles( qtrue, qtrue );
}

// g_korriRancorBox: out of the box (spawned there, teleported, pushed), nothing else matters: run straight back in
static void Rancor_ReturnToBox( void )
{
	vec3_t goal, dir, angles;

	G_RancorBoxClamp( NPC->r.currentOrigin, RANCORBOX_GOAL_INSET, goal );
	NPC->client->ps.speed = NPCInfo->stats.runSpeed;
	ucmd.buttons &= ~BUTTON_WALKING;
	NPC_SetMoveGoal( NPC, goal, 16, qtrue, -1, NULL );
	if ( !NPC_MoveToGoal( qtrue ) )
	{//no route, go straight and let collision slide it
		VectorSubtract( goal, NPC->r.currentOrigin, dir );
		dir[2] = 0;
		if ( VectorNormalize( dir ) >= 1.0f )
		{
			vectoangles( dir, angles );
			NPCInfo->desiredPitch = 0.0f;
			NPCInfo->desiredYaw = AngleNormalize360( angles[YAW] );
			ucmd.forwardmove = 127;
		}
	}
	NPC_UpdateAngles( qtrue, qtrue );
}

// last line of defense, whatever chose the move: if this frame's move (or its momentum) would take it past an edge,
// drop that part of it. moving back toward the box is always allowed
static void Rancor_BoxBackstop( void )
{
	vec3_t	mins, maxs, fwd, right, angles, w;
	float	speed, step, f, r;
	int		a;

	G_RancorBoxLimits( mins, maxs );
	speed = NPC->client->ps.speed;
	if ( VectorLength( NPC->client->ps.velocity ) > speed )
		speed = VectorLength( NPC->client->ps.velocity );
	step = speed * 0.1f;//this think plus the extra no-think frame

	VectorSet( angles, 0, SHORT2ANGLE( ucmd.angles[YAW] + NPC->client->ps.delta_angles[YAW] ), 0 );
	AngleVectors( angles, fwd, right, NULL );
	VectorClear( w );
	VectorMA( w, ucmd.forwardmove / 127.0f, fwd, w );
	VectorMA( w, ucmd.rightmove / 127.0f, right, w );

	for ( a = 0; a < 2; a++ )
	{
		float o = NPC->r.currentOrigin[a];
		if ( w[a] > 0 && o + w[a] * step > maxs[a] )
			w[a] = 0;
		else if ( w[a] < 0 && o + w[a] * step < mins[a] )
			w[a] = 0;

		if ( NPC->client->ps.velocity[a] > 0 && o + NPC->client->ps.velocity[a] * 0.1f > maxs[a] )
			NPC->client->ps.velocity[a] = 0;
		else if ( NPC->client->ps.velocity[a] < 0 && o + NPC->client->ps.velocity[a] * 0.1f < mins[a] )
			NPC->client->ps.velocity[a] = 0;
	}

	f = DotProduct( w, fwd ) * 127.0f;
	r = DotProduct( w, right ) * 127.0f;
	ucmd.forwardmove = (signed char)Com_Clampi( -127, 127, (int)f );
	ucmd.rightmove = (signed char)Com_Clampi( -127, 127, (int)r );
}

void NPC_BSRancor_Default( void )
{
	if ( !G_RancorBoxActive( NPC ) )
	{
		Rancor_Think();
		return;
	}

	if ( !G_RancorBoxContainsXY( NPC->r.currentOrigin ) )
	{
		Rancor_ReturnToBox();
	}
	else
	{
		if ( NPC->enemy && !NPC->count && !G_RancorBoxEnemyAllowed( NPC->enemy->r.currentOrigin ) )
		{//too far outside the box: forget him
			NPC->lastEnemy = NPC->enemy;
			NPC->enemy = NULL;
			NPC_ClearGoal();
		}
		Rancor_Think();
	}
	Rancor_BoxBackstop();
}
