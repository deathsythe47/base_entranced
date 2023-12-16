// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "w_saber.h"
#include "q_shared.h"
#include "bg_saga.h"

#define	MISSILE_PRESTEP_TIME	50

extern void laserTrapStick( gentity_t *ent, vec3_t endpos, vec3_t normal );
extern void Jedi_Decloak( gentity_t *self );

#include "namespace_begin.h"
extern qboolean FighterIsLanded( Vehicle_t *pVeh, playerState_t *parentPS );
#include "namespace_end.h"

/*
================
G_ReflectMissile

  Reflect the missile roughly back at it's owner
================
*/
#if 0
extern void G_TestLine( vec3_t start, vec3_t end, int color, int time );
#endif

float RandFloat( float min, float max, qboolean breakIntentionally );
void G_ReflectMissile( gentity_t *ent, gentity_t *missile, vec3_t forward, qboolean coneBased ) {
	vec3_t		bounce_dir;
	int			i;
	float		speed;
	qboolean	isOwner, breakRng;

#if 0
	vec3_t eyePoint;
	vec3_t viewTestLine;
	VectorCopy( ent->client->ps.origin, eyePoint );
	eyePoint[2] += ent->client->ps.viewheight;
	VectorCopy( forward, viewTestLine );
	VectorNormalize( viewTestLine );
	VectorScale( viewTestLine, 500, viewTestLine );
	VectorAdd( viewTestLine, eyePoint, viewTestLine );
	G_TestLine( eyePoint, viewTestLine, 0x00ff00, 10000 );
#endif

	breakRng = g_breakRNG.integer & BROKEN_RNG_REFLECT;

	//save the original speed
	speed = VectorNormalize( missile->s.pos.trDelta );

	if ( missile->r.ownerNum == ent->s.number ) {
		// since we're giving boost to our own missile by pushing it, up the velocity
		speed *= 1.5;
		isOwner = qtrue;
	}

	if ( coneBased ) {
		// new behavior: the direction is randomized in a cone centered around the player view, so you can roughly aim
		const float coneAngle = DEG2RAD( Com_Clampi( 0, 360, g_coneReflectAngle.integer ) );
		vec3_t coneDir;
		VectorCopy( forward, coneDir );
		VectorNormalize( coneDir );

		// generate a point on the angled spherical cap centered on the Z axis
		const float phi = RandFloat( 0, 1, breakRng ) * 2 * M_PI;
		bounce_dir[2] = RandFloat( 0, 1, breakRng ) * ( 1 - cos( coneAngle ) ) + cos( coneAngle );
		bounce_dir[0] = sqrt( 1 - bounce_dir[2] * bounce_dir[2] ) * cos( phi );
		bounce_dir[1] = sqrt( 1 - bounce_dir[2] * bounce_dir[2] ) * sin( phi );

		// find the rotation axis and rotation angle for the cone direction
		const vec3_t zAxis = { 0.0f, 0.0f, 1.0f };
		vec3_t rotationAxis;
		CrossProduct( zAxis, coneDir, rotationAxis );
		const float rotationAngle = acos( DotProduct( coneDir, zAxis ) );

		if ( rotationAngle ) {
			// rotate that shit
			const float cr = cos( rotationAngle );
			const float sr = sin( rotationAngle );
			vec3_t zCenteredDirection;
			VectorCopy( bounce_dir, zCenteredDirection );

			// my head
			bounce_dir[0] =
				( ( cr + ( rotationAxis[0] * rotationAxis[0] * ( 1 - cr ) ) ) * zCenteredDirection[0] ) +
				( ( ( rotationAxis[0] * rotationAxis[1] * ( 1 - cr ) ) - ( rotationAxis[2] * sr ) ) * zCenteredDirection[1] ) +
				( ( ( rotationAxis[0] * rotationAxis[2] * ( 1 - cr ) ) + ( rotationAxis[1] * sr ) ) * zCenteredDirection[2] );
			bounce_dir[1] =
				( ( ( rotationAxis[1] * rotationAxis[0] * ( 1 - cr ) ) + ( rotationAxis[2] * sr ) ) * zCenteredDirection[0] ) +
				( ( cr + ( rotationAxis[1] * rotationAxis[1] * ( 1 - cr ) ) ) * zCenteredDirection[1] ) +
				( ( ( rotationAxis[1] * rotationAxis[2] * ( 1 - cr ) ) - ( rotationAxis[0] * sr ) ) * zCenteredDirection[2] );
			bounce_dir[2] =
				( ( ( rotationAxis[2] * rotationAxis[0] * ( 1 - cr ) ) - ( rotationAxis[1] * sr ) ) * zCenteredDirection[0] ) +
				( ( ( rotationAxis[2] * rotationAxis[1] * ( 1 - cr ) ) + ( rotationAxis[0] * sr ) ) * zCenteredDirection[1] ) +
				( ( cr + ( rotationAxis[2] * rotationAxis[2] * ( 1 - cr ) ) ) * zCenteredDirection[2] );
		}

		if (g_gametype.integer == GT_SIEGE && g_siegeReflectionFix.integer) {
			bounce_dir[2] = RandFloat(SIEGEREFLECTIONFIX_MIN, SIEGEREFLECTIONFIX_MAX, breakRng);
		}
	} else {
		// basejka behavior: if owner is present, it's roughly sent back at him
		if ( &g_entities[missile->r.ownerNum] && missile->s.weapon != WP_SABER && missile->s.weapon != G2_MODEL_PART && !isOwner ) {
			// bounce back at them if you can
			VectorSubtract( g_entities[missile->r.ownerNum].r.currentOrigin, missile->r.currentOrigin, bounce_dir );
			VectorNormalize( bounce_dir );
		} else {
			vec3_t missile_dir;

			VectorSubtract( ent->r.currentOrigin, missile->r.currentOrigin, missile_dir );
			VectorCopy( missile->s.pos.trDelta, bounce_dir );
			VectorScale( bounce_dir, DotProduct( forward, missile_dir ), bounce_dir );
			VectorNormalize( bounce_dir );
		}

		for ( i = 0; i < 3; i++ ) {
			float lowerBound, upperBound;
			if (i == 2 && g_gametype.integer == GT_SIEGE && g_siegeReflectionFix.integer) {
				lowerBound = SIEGEREFLECTIONFIX_MIN;
				upperBound = SIEGEREFLECTIONFIX_MAX;
			}
			else {
				lowerBound = -0.2f;
				upperBound = 0.2f;
			}
			bounce_dir[i] += RandFloat( lowerBound, upperBound, breakRng ); // *CHANGE 10a* bigger deflect angles
		}
	}

	VectorNormalize( bounce_dir );
	VectorScale( bounce_dir, speed, missile->s.pos.trDelta );
	missile->s.pos.trTime = level.time; // move a bit on the very first frame
	VectorCopy( missile->r.currentOrigin, missile->s.pos.trBase );

#if 0
	vec3_t reflectionTestLine;
	VectorCopy( missile->s.pos.trDelta, reflectionTestLine );
	VectorNormalize( reflectionTestLine );
	VectorScale( reflectionTestLine, 500, reflectionTestLine );
	VectorAdd( reflectionTestLine, missile->s.pos.trBase, reflectionTestLine );
	G_TestLine( missile->s.pos.trBase, reflectionTestLine, 0x0000ff, 10000 );
#endif

	if ( missile->s.weapon != WP_SABER && missile->s.weapon != G2_MODEL_PART ) {
		// you are mine, now!
		missile->r.ownerNum = ent->s.number;
		missile->isReflected = qtrue;
	}

	if ( missile->s.weapon == WP_ROCKET_LAUNCHER ) {
		// stop homing
		missile->think = 0;
		missile->nextthink = 0;
	}
}

void G_DeflectMissile( gentity_t *ent, gentity_t *missile, vec3_t forward ) 
{
	vec3_t	bounce_dir;
	int		i;
	float	speed;
	vec3_t missile_dir;

	//save the original speed
	speed = VectorNormalize( missile->s.pos.trDelta );

	if (ent->client)
	{
		AngleVectors(ent->client->ps.viewangles, missile_dir, 0, 0);
		VectorCopy(missile_dir, bounce_dir);
		VectorScale( bounce_dir, DotProduct( forward, missile_dir ), bounce_dir );
		VectorNormalize( bounce_dir );
	}
	else
	{
		VectorCopy(forward, bounce_dir);
		VectorNormalize(bounce_dir);
	}

	for ( i = 0; i < 3; i++ )
	{
		float lowerBound, upperBound;
		if (i == 2 && g_gametype.integer == GT_SIEGE && g_siegeReflectionFix.integer) {
			lowerBound = SIEGEREFLECTIONFIX_MIN;
			upperBound = SIEGEREFLECTIONFIX_MAX;
		}
		else {
			lowerBound = -1.0f;
			upperBound = 1.0f;
		}
		bounce_dir[i] += RandFloat( lowerBound, upperBound, g_breakRNG.integer & BROKEN_RNG_DEFLECT );// *CHANGE 10b* bigger deflect angles
	}

	VectorNormalize( bounce_dir );
	VectorScale( bounce_dir, speed, missile->s.pos.trDelta );
	missile->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy( missile->r.currentOrigin, missile->s.pos.trBase );
	if ( missile->s.weapon != WP_SABER && missile->s.weapon != G2_MODEL_PART )
	{//you are mine, now!
		missile->r.ownerNum = ent->s.number;
		missile->isReflected = qtrue;
	}
	if ( missile->s.weapon == WP_ROCKET_LAUNCHER )
	{//stop homing
		missile->think = 0;
		missile->nextthink = 0;
	}
}

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;
	gentity_t		*other;
	float coeff;

	other = &g_entities[trace->entityNum];

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );

	if ( ent->flags & FL_BOUNCE_SHRAPNEL ) 
	{
		VectorScale( ent->s.pos.trDelta, 0.25f, ent->s.pos.trDelta );
		ent->s.pos.trType = TR_GRAVITY;

		// check for stop
		if ( trace->plane.normal[2] > 0.7 && ent->s.pos.trDelta[2] < 40 ) //this can happen even on very slightly sloped walls, so changed it from > 0 to > 0.7
		{
			G_SetOrigin( ent, trace->endpos );
			ent->nextthink = level.time + 100;
			return;
		}
	}
	else if ( ent->flags & FL_BOUNCE_HALF ) 
	{
		VectorScale( ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta );
		// check for stop
		if ( trace->plane.normal[2] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 ) 
		{
			G_SetOrigin( ent, trace->endpos );
			return;
		}
	}

	if (ent->s.weapon == WP_THERMAL)
	{ //slight hack for hit sound
		G_Sound(ent, CHAN_BODY, G_SoundIndex(va("sound/weapons/thermal/bounce%i.wav", Q_irand(1, 2))));
	}
	else if (ent->s.weapon == WP_SABER)
	{
		G_Sound(ent, CHAN_BODY, G_SoundIndex(va("sound/weapons/saber/bounce%i.wav", Q_irand(1, 3))));
	}
	else if (ent->s.weapon == G2_MODEL_PART)
	{
		//Limb bounce sound?
	}

	coeff = 1.0f;
	if (((other->s.pos.trType == TR_LINEAR_STOP) ||
	(other->s.pos.trType == TR_NONLINEAR_STOP)) && (VectorLength(trace->plane.normal) != 0.0f)){
		dot = DotProduct( other->s.pos.trDelta, trace->plane.normal );

		if (dot > 0)
			coeff = 0.35f*dot;

		VectorMA( ent->s.pos.trDelta, coeff, trace->plane.normal, ent->s.pos.trDelta);

	}

	VectorMA( ent->r.currentOrigin, /*1.0f*/coeff, trace->plane.normal, ent->r.currentOrigin);

	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;

	if (ent->bounceCount != -5)
	{
		ent->bounceCount--;
	}
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent ) {
	vec3_t		dir;
	vec3_t		origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	// we don't have a valid direction, so just point straight up
	dir[0] = dir[1] = 0;
	dir[2] = 1;

	ent->s.eType = ET_GENERAL;
	G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );

	ent->freeAfterEvent = qtrue;

	ent->takedamage = qfalse;
	// splash damage
	if ( ent->splashDamage ) {
		if( G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius, ent, 
				ent, ent->splashMethodOfDeath ) ) 
		{
			if (ent->parent)
			{
				g_entities[ent->parent->s.number].client->accuracy_hits++;
			}
			else if (ent->activator)
			{
				g_entities[ent->activator->s.number].client->accuracy_hits++;
			}
		}
	}

	trap_LinkEntity( ent );
}

void G_RunStuckMissile( gentity_t *ent )
{
	if ( ent->takedamage )
	{
		if ( ent->s.groundEntityNum >= 0 && ent->s.groundEntityNum < ENTITYNUM_WORLD )
		{
			gentity_t *other = &g_entities[ent->s.groundEntityNum];

			if ( (!VectorCompare( vec3_origin, other->s.pos.trDelta ) && other->s.pos.trType != TR_STATIONARY) || 
				(!VectorCompare( vec3_origin, other->s.apos.trDelta ) && other->s.apos.trType != TR_STATIONARY) )
			{//thing I stuck to is moving or rotating now, kill me
				G_Damage( ent, other, other, NULL, NULL, 99999, 0, MOD_CRUSH );
				return;
			}
		}
	}
	// check think function
	G_RunThink( ent );
}

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout ) {
	vec3_t v, newv;
	float dot;

	VectorSubtract( impact, start, v );
	dot = DotProduct( v, dir );
	VectorMA( v, -2*dot, dir, newv );

	VectorNormalize(newv);
	VectorMA(impact, 8192, newv, endout);
}


//-----------------------------------------------------------------------------
gentity_t *CreateMissile( vec3_t org, vec3_t dir, float vel, int life, 
							gentity_t *owner, qboolean altFire)
//-----------------------------------------------------------------------------
{
	gentity_t	*missile;

	missile = G_Spawn();
	
	missile->nextthink = level.time + life;
	missile->think = G_FreeEntity;
	missile->s.eType = ET_MISSILE;
	missile->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	missile->parent = owner;
	missile->r.ownerNum = owner->s.number;
	missile->isReflected = qfalse;

	if (altFire)
	{
		missile->s.eFlags |= EF_ALT_FIRING;
	}

	missile->s.pos.trType = TR_LINEAR;
	missile->s.pos.trTime = level.time;
	missile->target_ent = NULL;

	SnapVector(org);
	VectorCopy( org, missile->s.pos.trBase );
	VectorScale( dir, vel, missile->s.pos.trDelta );
	VectorCopy( org, missile->r.currentOrigin);
	SnapVector(missile->s.pos.trDelta);

	return missile;
}

void G_MissileBounceEffect( gentity_t *ent, vec3_t org, vec3_t dir )
{
	//FIXME: have an EV_BOUNCE_MISSILE event that checks the s.weapon and does the appropriate effect
	switch( ent->s.weapon )
	{
	case WP_BOWCASTER:
		G_PlayEffectID( G_EffectIndex("bowcaster/deflect"), ent->r.currentOrigin, dir );
		break;
	case WP_BLASTER:
	case WP_BRYAR_PISTOL:
		G_PlayEffectID( G_EffectIndex("blaster/deflect"), ent->r.currentOrigin, dir );
		break;
	default:
		{
			gentity_t *te = G_TempEntity( org, EV_SABER_BLOCK );
			VectorCopy(org, te->s.origin);
			VectorCopy(dir, te->s.angles);
			te->s.eventParm = 0;
			te->s.weapon = 0;//saberNum
			te->s.legsAnim = 0;//bladeNum
		}
		break;
	}
}

qboolean LogAccuracyHitSameTeamOkay(gentity_t *target, gentity_t *attacker) {
	if (!target->takedamage) {
		return qfalse;
	}

	if (target == attacker) {
		return qfalse;
	}

	if (!target->client) {
		return qfalse;
	}

	if (!attacker)
	{
		return qfalse;
	}

	if (!attacker->client) {
		return qfalse;
	}

	if (target->client->ps.stats[STAT_HEALTH] <= 0) {
		return qfalse;
	}

	/*if (OnSameTeam(target, attacker)) {
		return qfalse;
	}*/

	return qtrue;
}

static qboolean CountsForAirshotStat(gentity_t *missile) {
	assert(missile);
	switch (missile->methodOfDeath) {
	case MOD_BOWCASTER:
	case MOD_REPEATER_ALT:
	case MOD_FLECHETTE_ALT_SPLASH:
	case MOD_ROCKET:
	case MOD_THERMAL:
	case MOD_CONC:
		return qtrue;
	case MOD_ROCKET_HOMING:
		return !!!(missile->enemy); // alt rockets can count if they are unhomed
	case MOD_BRYAR_PISTOL_ALT:
		return !!(missile->s.generic1 > 1); // require a little bit of charge
	default:
		return qfalse;
	}
}

qboolean CheckAccuracyAndAirshot(gentity_t *missile, gentity_t *victim, qboolean isSurfedRocket) {
	qboolean hitClient = qfalse;

	if (missile->r.ownerNum >= MAX_CLIENTS)
		return qfalse;
	gentity_t *missileOwner = &g_entities[missile->r.ownerNum];
	if (!missileOwner->inuse || !missileOwner->client)
		return qfalse;

	if (LogAccuracyHitSameTeamOkay(victim, missileOwner) && !missile->isReflected) {
		hitClient = qtrue;

		if (isSurfedRocket && CountsForAirshotStat(missile)) {
			if (victim - g_entities < MAX_CLIENTS) {
				missileOwner->client->lastAiredOtherClientTime[victim - g_entities] = level.time;
				missileOwner->client->lastAiredOtherClientMeansOfDeath[victim - g_entities] = missile->methodOfDeath;
			}
		}
		else if (victim->playerState->groundEntityNum == ENTITYNUM_NONE && CountsForAirshotStat(missile)) {
			// hit while in air; make sure the victim is decently in the air though (not just 1 nanometer from the gorund)
			trace_t tr;
			vec3_t down;
			VectorCopy(victim->r.currentOrigin, down);
			down[2] -= 4096;
			trap_Trace(&tr, victim->r.currentOrigin, victim->r.mins, victim->r.maxs, down, victim - g_entities, MASK_SOLID);
			VectorSubtract(victim->r.currentOrigin, tr.endpos, down);
			float groundDist = VectorLength(down);
			if (groundDist >= AIRSHOT_GROUND_DISTANCE_THRESHOLD) {
				if (victim - g_entities < MAX_CLIENTS) {
					missileOwner->client->lastAiredOtherClientTime[victim - g_entities] = level.time;
					missileOwner->client->lastAiredOtherClientMeansOfDeath[victim - g_entities] = missile->methodOfDeath;
				}
			}
			//PrintIngame(-1, "Ground distance is %0.2f\n", groundDist);
		}
	}

	return hitClient;
}

/*
================
G_MissileImpact
================
*/
void WP_SaberBlockNonRandom( gentity_t *self, vec3_t hitloc, qboolean missileBlock );
void WP_flechette_alt_blow( gentity_t *ent );
void G_MissileImpact( gentity_t *ent, trace_t *trace ) {
	gentity_t		*other;
	qboolean		hitClient = qfalse;
	qboolean		isKnockedSaber = qfalse;

	other = &g_entities[trace->entityNum];

	// check for bounce
	if ( !other->takedamage &&
		(ent->bounceCount > 0 || ent->bounceCount == -5 ) &&
		( ent->flags & ( FL_BOUNCE | FL_BOUNCE_HALF ) ) ) {
		G_BounceMissile( ent, trace );
		G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
		return;
	}
	else if (ent->neverFree && ent->s.weapon == WP_SABER && (ent->flags & FL_BOUNCE_HALF))
	{ //this is a knocked-away saber
		if (ent->bounceCount > 0 || ent->bounceCount == -5)
		{
			G_BounceMissile( ent, trace );
			G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
			return;
		}

		isKnockedSaber = qtrue;
	}

	// I would glom onto the FL_BOUNCE code section above, but don't feel like risking breaking something else
	if ( (!other->takedamage && (ent->bounceCount > 0 || ent->bounceCount == -5) && ( ent->flags&(FL_BOUNCE_SHRAPNEL) ) ) || ((trace->surfaceFlags&SURF_FORCEFIELD)&&!ent->splashDamage&&!ent->splashRadius&&(ent->bounceCount > 0 || ent->bounceCount == -5)) ) 
	{
		G_BounceMissile( ent, trace );

		if ( ent->bounceCount < 1 )
		{
			ent->flags &= ~FL_BOUNCE_SHRAPNEL;
		}
		return;
	}

	if ((other->r.contents & CONTENTS_LIGHTSABER) && !isKnockedSaber)
	{ //hit this person's saber, so..
		gentity_t *otherOwner = &g_entities[other->r.ownerNum];

		if (otherOwner->takedamage && otherOwner->client && otherOwner->client->ps.duelInProgress &&
			otherOwner->client->ps.duelIndex != ent->r.ownerNum)
		{
			goto killProj;
		}
		else if (otherOwner->takedamage && otherOwner->client && otherOwner->client->sess.siegeDuelInProgress &&
			otherOwner->client->sess.siegeDuelIndex != ent->r.ownerNum)
		{
			goto killProj;
		}
	}
	else if (!isKnockedSaber)
	{
		if (other->takedamage && other->client && other->client->ps.duelInProgress &&
			other->client->ps.duelIndex != ent->r.ownerNum)
		{
			goto killProj;
		}
		else if (other->takedamage && other->client && other->client->sess.siegeDuelInProgress &&
			other->client->sess.siegeDuelIndex != ent->r.ownerNum)
		{
			goto killProj;
		}
	}

	if (g_gametype.integer == GT_SIEGE && other && other->client && other->client->ps.weapon == WP_MELEE && other->client->siegeClass != -1 && bgSiegeClasses[other->client->siegeClass].classflags & (1 << CFL_WONDERWOMAN)) {
		if (ent->methodOfDeath != MOD_REPEATER_ALT &&
			ent->methodOfDeath != MOD_ROCKET &&
			ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
			ent->methodOfDeath != MOD_ROCKET_HOMING &&
			ent->methodOfDeath != MOD_THERMAL &&
			ent->methodOfDeath != MOD_THERMAL_SPLASH &&
			ent->methodOfDeath != MOD_TRIP_MINE_SPLASH &&
			ent->methodOfDeath != MOD_TIMED_MINE_SPLASH &&
			ent->methodOfDeath != MOD_DET_PACK_SPLASH &&
			ent->methodOfDeath != MOD_VEHICLE &&
			ent->methodOfDeath != MOD_CONC &&
			ent->methodOfDeath != MOD_CONC_ALT &&
			ent->methodOfDeath != MOD_SABER &&
			ent->methodOfDeath != MOD_TURBLAST)
		{
			vec3_t fwd;

			if (trace)
			{
				VectorCopy(trace->plane.normal, fwd);
			}
			else
			{ //oh well
				AngleVectors(other->r.currentAngles, fwd, NULL, NULL);
			}

			G_DeflectMissile(other, ent, fwd);
			G_MissileBounceEffect(ent, ent->r.currentOrigin, fwd);
			return;
		}
	}

	if (other->flags & FL_DMG_BY_HEAVY_WEAP_ONLY)
	{
		if (ent->methodOfDeath != MOD_REPEATER_ALT &&
			ent->methodOfDeath != MOD_ROCKET &&
			ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
			ent->methodOfDeath != MOD_ROCKET_HOMING &&
			ent->methodOfDeath != MOD_THERMAL &&
			ent->methodOfDeath != MOD_THERMAL_SPLASH &&
			ent->methodOfDeath != MOD_TRIP_MINE_SPLASH &&
			ent->methodOfDeath != MOD_TIMED_MINE_SPLASH &&
			ent->methodOfDeath != MOD_DET_PACK_SPLASH &&
			ent->methodOfDeath != MOD_VEHICLE &&
			ent->methodOfDeath != MOD_CONC &&
			ent->methodOfDeath != MOD_CONC_ALT &&
			ent->methodOfDeath != MOD_SABER &&
			ent->methodOfDeath != MOD_TURBLAST)
		{
			vec3_t fwd;

			if (trace)
			{
				VectorCopy(trace->plane.normal, fwd);
			}
			else
			{ //oh well
				AngleVectors(other->r.currentAngles, fwd, NULL, NULL);
			}

			G_DeflectMissile(other, ent, fwd);
			G_MissileBounceEffect(ent, ent->r.currentOrigin, fwd);
			return;
		}
	}

	if ((other->flags & FL_SHIELDED) &&
		ent->s.weapon != WP_ROCKET_LAUNCHER &&
		ent->s.weapon != WP_THERMAL &&
		ent->s.weapon != WP_TRIP_MINE &&
		ent->s.weapon != WP_DET_PACK &&
		ent->s.weapon != WP_DEMP2 &&
		ent->s.weapon != WP_EMPLACED_GUN &&
		ent->methodOfDeath != MOD_REPEATER_ALT &&
		ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH && 
		ent->methodOfDeath != MOD_TURBLAST &&
		ent->methodOfDeath != MOD_VEHICLE &&
		ent->methodOfDeath != MOD_CONC &&
		ent->methodOfDeath != MOD_CONC_ALT &&
		!(ent->dflags&DAMAGE_HEAVY_WEAP_CLASS) )
	{
		vec3_t fwd;

		if (other->client)
		{
			AngleVectors(other->client->ps.viewangles, fwd, NULL, NULL);
		}
		else
		{
			AngleVectors(other->r.currentAngles, fwd, NULL, NULL);
		}

		// duo: fix killing yourself off the walker
		if (other->s.NPC_class == CLASS_VEHICLE && other->m_pVehicle && other->m_pVehicle->m_pVehicleInfo && other->m_pVehicle->m_pVehicleInfo->type == VH_WALKER) {
			ent->damage = 0;
			ent->splashDamage = 0;
		}

		G_DeflectMissile(other, ent, fwd);
		G_MissileBounceEffect(ent, ent->r.currentOrigin, fwd);
		return;
	}

	vec3_t overrideSpot;
	qboolean overrideSpotSet = qfalse;
	if (g_fixSaberDefense.integer >= 2) {
		VectorMA(ent->s.pos.trBase, -0.1f, ent->s.pos.trDelta, overrideSpot);
		overrideSpotSet = qtrue;
	}

	if (other->takedamage && other->client &&
		ent->s.weapon != WP_ROCKET_LAUNCHER &&
		ent->s.weapon != WP_THERMAL &&
		ent->s.weapon != WP_TRIP_MINE &&
		ent->s.weapon != WP_DET_PACK &&
		ent->s.weapon != WP_DEMP2 &&
		ent->methodOfDeath != MOD_REPEATER_ALT &&
		ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
		ent->methodOfDeath != MOD_CONC &&
		ent->methodOfDeath != MOD_CONC_ALT &&
		other->client->ps.saberBlockTime < level.time &&
		!isKnockedSaber &&
		WP_SaberCanBlock(other, ent, ent->r.currentOrigin, 0, 0, qtrue, 0, overrideSpotSet ? (vec_t*)&overrideSpot : NULL))
	{ //only block one projectile per 200ms (to prevent giant swarms of projectiles being blocked)
		vec3_t fwd;
		gentity_t *te;
		int otherDefLevel = other->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE];

		te = G_TempEntity( ent->r.currentOrigin, EV_SABER_BLOCK );
		VectorCopy(ent->r.currentOrigin, te->s.origin);
		VectorCopy(trace->plane.normal, te->s.angles);
		te->s.eventParm = 0;
		te->s.weapon = 0;//saberNum
		te->s.legsAnim = 0;//bladeNum

		/*if (other->client->ps.velocity[2] > 0 ||
			other->client->pers.cmd.forwardmove ||
			other->client->pers.cmd.rightmove)
			*/
		if (other->client->ps.velocity[2] > 0 ||
			other->client->pers.cmd.forwardmove < 0) //now we only do it if jumping or running backward. Should be able to full-on charge.
		{
			otherDefLevel -= 1;
			if (otherDefLevel < 0)
			{
				otherDefLevel = 0;
			}
		}

		AngleVectors(other->client->ps.viewangles, fwd, NULL, NULL);
		if (otherDefLevel == FORCE_LEVEL_1)
		{
			//if def is only level 1, instead of deflecting the shot it should just die here
		}
		else if (otherDefLevel == FORCE_LEVEL_2)
		{
			G_DeflectMissile(other, ent, fwd);
		}
		else
		{
			G_ReflectMissile(other, ent, fwd, g_randomConeReflection.integer & CONE_REFLECT_SDEF);
		}
		other->client->ps.saberBlockTime = level.time + (350 - (otherDefLevel*100)); 

		//For jedi AI
		other->client->ps.saberEventFlags |= SEF_DEFLECTED;

		if (otherDefLevel == FORCE_LEVEL_3)
		{
			other->client->ps.saberBlockTime = 0; //^_^
		}

		if (otherDefLevel == FORCE_LEVEL_1)
		{
			goto killProj;
		}
		return;
	}
	else if ((other->r.contents & CONTENTS_LIGHTSABER) && !isKnockedSaber)
	{ //hit this person's saber, so..
		gentity_t *otherOwner = &g_entities[other->r.ownerNum];

		if (otherOwner->takedamage && otherOwner->client &&
			ent->s.weapon != WP_ROCKET_LAUNCHER &&
			ent->s.weapon != WP_THERMAL &&
			ent->s.weapon != WP_TRIP_MINE &&
			ent->s.weapon != WP_DET_PACK &&
			ent->s.weapon != WP_DEMP2 &&
			ent->methodOfDeath != MOD_REPEATER_ALT &&
			ent->methodOfDeath != MOD_FLECHETTE_ALT_SPLASH &&
			ent->methodOfDeath != MOD_CONC &&
			ent->methodOfDeath != MOD_CONC_ALT )
		{ //for now still deflect even if saberBlockTime >= level.time because it hit the actual saber
			vec3_t fwd;
			gentity_t *te;
			int otherDefLevel = otherOwner->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE];

			//in this case, deflect it even if we can't actually block it because it hit our saber
			if (otherOwner->client && otherOwner->client->ps.weaponTime <= 0)
			{
				WP_SaberBlockNonRandom(otherOwner, ent->r.currentOrigin, qtrue);
			}

			te = G_TempEntity( ent->r.currentOrigin, EV_SABER_BLOCK );
			VectorCopy(ent->r.currentOrigin, te->s.origin);
			VectorCopy(trace->plane.normal, te->s.angles);
			te->s.eventParm = 0;
			te->s.weapon = 0;//saberNum
			te->s.legsAnim = 0;//bladeNum

			if (otherOwner->client->ps.velocity[2] > 0 ||
				otherOwner->client->pers.cmd.forwardmove < 0) //now we only do it if jumping or running backward. Should be able to full-on charge.
			{
				otherDefLevel -= 1;
				if (otherDefLevel < 0)
				{
					otherDefLevel = 0;
				}
			}

			AngleVectors(otherOwner->client->ps.viewangles, fwd, NULL, NULL);

			if (otherDefLevel == FORCE_LEVEL_1)
			{
				//if def is only level 1, instead of deflecting the shot it should just die here
			}
			else if (otherDefLevel == FORCE_LEVEL_2)
			{
				G_DeflectMissile(otherOwner, ent, fwd);
			}
			else
			{
				G_ReflectMissile(otherOwner, ent, fwd, g_randomConeReflection.integer & CONE_REFLECT_SDEF);
			}
			otherOwner->client->ps.saberBlockTime = level.time + (350 - (otherDefLevel*100));

			//For jedi AI
			otherOwner->client->ps.saberEventFlags |= SEF_DEFLECTED;

			if (otherDefLevel == FORCE_LEVEL_3)
			{
				otherOwner->client->ps.saberBlockTime = 0; //^_^
			}

			if (otherDefLevel == FORCE_LEVEL_1)
			{
				goto killProj;
			}
			return;
		}
	}

	// check for sticking
	if ( !other->takedamage && ( ent->s.eFlags & EF_MISSILE_STICK ) ) 
	{
		laserTrapStick( ent, trace->endpos, trace->plane.normal );
		G_AddEvent( ent, EV_MISSILE_STICK, 0 );
		return;
	}

	// impact damage
	if (other->takedamage && !isKnockedSaber) {
		// FIXME: wrong damage direction?
		if ( ent->damage ) {
			vec3_t	velocity;
			qboolean didDmg = qfalse;

			hitClient = CheckAccuracyAndAirshot(ent, other, qfalse);
			BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
			if ( VectorLength( velocity ) == 0 ) {
				velocity[2] = 1;	// stepped on a grenade
			}

			if ((ent->s.weapon == WP_BOWCASTER || ent->s.weapon == WP_FLECHETTE ||
				ent->s.weapon == WP_ROCKET_LAUNCHER))
			{
				if (ent->s.weapon == WP_FLECHETTE && (ent->s.eFlags & EF_ALT_FIRING)) 
				{
					if (g_fixGolanDamage.integer) { // do audio/visual explosion effect and apply single-target damage directly to the target
						if (ent->think == WP_flechette_alt_blow) {
							// effect
							ent->takedamage = qfalse;
							vec3_t v;
							v[0] = ent->s.time == -2 ? 0 : 1;
							v[1] = 0;
							v[2] = 0;
							gentity_t *te = G_PlayEffect(EFFECT_EXPLOSION_FLECHETTE, ent->r.currentOrigin, v);
							ent->think = G_FreeEntity;
							ent->nextthink = level.time;

							if (g_fixGolanDamage.integer == 1) { // g_fixGolanDamage 1 == knockback everyone at this stage unless target is a living player
								if (other - g_entities >= MAX_CLIENTS) {
									// splash knockback without damage if not living player
									if (ent->activator)
										G_RadiusDamageKnockbackOnly(ent->r.currentOrigin, ent->activator, ent->splashDamage, ent->splashRadius, ent, ent, ent->methodOfDeath/*MOD_LT_SPLASH*/);
								}

								// single-target damage with knockback
								G_Damage(other, ent, &g_entities[ent->r.ownerNum], velocity,
									/*ent->s.origin*/ent->r.currentOrigin, ent->damage,
									DAMAGE_HALF_ABSORB, ent->methodOfDeath);
							}
							else if (g_fixGolanDamage.integer == 2) { // g_fixGolanDamage 2 == knockback everyone at this stage
								// splash knockback without damage
								if (ent->activator)
									G_RadiusDamageKnockbackOnly(ent->r.currentOrigin, ent->activator, ent->splashDamage, ent->splashRadius, ent, ent, ent->methodOfDeath/*MOD_LT_SPLASH*/);

								// single-target damage with knockback
								G_Damage(other, ent, &g_entities[ent->r.ownerNum], velocity,
									/*ent->s.origin*/ent->r.currentOrigin, ent->damage,
									DAMAGE_HALF_ABSORB, ent->methodOfDeath);
							}
							else { // g_fixGolanDamage 3 == only knockback the target at this stage
								// single-target damage with knockback
								G_Damage(other, ent, &g_entities[ent->r.ownerNum], velocity,
									/*ent->s.origin*/ent->r.currentOrigin, ent->damage,
									DAMAGE_HALF_ABSORB, ent->methodOfDeath);
							}
						}
					}
					else { // legacy behavior
						/* fix: there are rare situations where flechette did
						explode by timeout AND by impact in the very same frame, then here
						ent->think was set to G_FreeEntity, so the folowing think
						did invalidate this entity, BUT it would be reused later in this
						function for explosion event. This, then, would set ent->freeAfterEvent
						to qtrue, so event later, when reusing this entity by using G_InitEntity(),
						it would have this freeAfterEvent set AND this would in case of dropped
						item erase it from game immeadiately. THIS for example caused
						very rare flag dissappearing bug.	 */
						if (ent->think == WP_flechette_alt_blow)
							ent->think(ent);
					}
				}
				else
				{
					G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
						/*ent->s.origin*/ent->r.currentOrigin, ent->damage, 
						DAMAGE_HALF_ABSORB, ent->methodOfDeath);
					didDmg = qtrue;
				}
			}
			else
			{
				G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
					/*ent->s.origin*/ent->r.currentOrigin, ent->damage, 
					0, ent->methodOfDeath);
				didDmg = qtrue;
			}

			if (didDmg && other && other->client)
			{ //What I'm wondering is why this isn't in the NPC pain funcs. But this is what SP does, so whatever.
				class_t	npc_class = other->client->NPC_class;

				// If we are a robot and we aren't currently doing the full body electricity...
				if ( npc_class == CLASS_SEEKER || npc_class == CLASS_PROBE || npc_class == CLASS_MOUSE ||
					   npc_class == CLASS_GONK || npc_class == CLASS_R2D2 || npc_class == CLASS_R5D2 || npc_class == CLASS_REMOTE ||
					   npc_class == CLASS_MARK1 || npc_class == CLASS_MARK2 || //npc_class == CLASS_PROTOCOL ||//no protocol, looks odd
					   npc_class == CLASS_INTERROGATOR || npc_class == CLASS_ATST || npc_class == CLASS_SENTRY )
				{
					// special droid only behaviors
					if ( other->client->ps.electrifyTime < level.time + 100 )
					{
						// ... do the effect for a split second for some more feedback
						other->client->ps.electrifyTime = level.time + 450;
					}
					//FIXME: throw some sparks off droids,too
				}
			}
		}

		if ( ent->s.weapon == WP_DEMP2 )
		{//a hit with demp2 decloaks people, disables ships
			if ( other && other->client && other->client->NPC_class == CLASS_VEHICLE )
			{//hit a vehicle
				if ( other->m_pVehicle //valid vehicle ent
					&& other->m_pVehicle->m_pVehicleInfo//valid stats
					&& (other->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER//always affect speeders
						||(other->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER && ent->classname && Q_stricmp("vehicle_proj", ent->classname ) == 0) )//only vehicle ion weapons affect a fighter in this manner
					&& !FighterIsLanded( other->m_pVehicle , &other->client->ps )//not landed
					&& !(other->spawnflags&2) )//and not suspended
				{//vehicles hit by "ion cannons" lose control
					if (!(!g_friendlyFreeze.integer && g_gametype.integer >= GT_TEAM &&
						ent->parent && ent->parent - g_entities < MAX_CLIENTS && ent->parent->client &&
						other->m_pVehicle->m_pPilot && (gentity_t *)other->m_pVehicle->m_pPilot - g_entities < MAX_CLIENTS && ((gentity_t *)(other->m_pVehicle->m_pPilot))->client &&
						((gentity_t *)(other->m_pVehicle->m_pPilot))->client->sess.sessionTeam == ent->parent->client->sess.sessionTeam) &&
						!(!g_friendlyFreeze.integer && g_gametype.integer >= GT_TEAM && ent->parent && ent->parent->client && other->teamnodmg == ent->parent->client->sess.sessionTeam)) {
						if (other->client->ps.electrifyTime > level.time)
						{//add onto it
							//FIXME: extern the length of the "out of control" time?
							other->client->ps.electrifyTime += Q_irand(200, 500);
							if (other->client->ps.electrifyTime > level.time + 4000)
							{//cap it
								other->client->ps.electrifyTime = level.time + 4000;
							}
						}
						else
						{//start it
							//FIXME: extern the length of the "out of control" time?
							other->client->ps.electrifyTime = level.time + Q_irand(200, 500);
						}
					}
				}
			}
			else if ( other && other->client && other->client->ps.powerups[PW_CLOAKED] )
			{
				if (!(!g_friendlyFreeze.integer && g_gametype.integer >= GT_TEAM && ent->client && other && other->client &&
					ent->client->sess.sessionTeam == other->client->sess.sessionTeam)) {
					Jedi_Decloak(other);
					if (ent->methodOfDeath == MOD_DEMP2_ALT)
					{//direct hit with alt disables cloak forever
						//permanently disable the saboteur's cloak
						other->client->cloakToggleTime = Q3_INFINITE;
					}
					else
					{//temp disable
						other->client->cloakToggleTime = level.time + Q_irand(3000, 10000);
					}
				}
			}
		}
	}

killProj:

	if (!ent->inuse){
		G_LogPrintf("ERROR: entity %i non-used, checkpoint 5\n",ent-g_entities);
	}

	// is it cheaper in bandwidth to just remove this ent and create a new
	// one, rather than changing the missile into the explosion?

	if ( other->takedamage && other->client && !isKnockedSaber ) {
		G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
		ent->s.otherEntityNum = other->s.number;
	} else if( trace->surfaceFlags & SURF_METALSTEPS ) {
		G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ) );
	} else if (ent->s.weapon != G2_MODEL_PART && !isKnockedSaber) {
		G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
	}

	if (!isKnockedSaber)
	{
		ent->freeAfterEvent = qtrue;

		// change over to a normal entity right at the point of impact
		ent->s.eType = ET_GENERAL;
	}

	SnapVectorTowards( trace->endpos, ent->s.pos.trBase );	// save net bandwidth

	G_SetOrigin( ent, trace->endpos );

	ent->takedamage = qfalse;
	// splash damage (doesn't apply to person directly hit)
	if ( ent->splashDamage ) {
		if( G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius, 
			other, ent, ent->splashMethodOfDeath ) ) {
			if( !hitClient 
				&& g_entities[ent->r.ownerNum].client 
				&& !ent->isReflected) {
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
			}
		}
	}

	if (ent->s.weapon == G2_MODEL_PART)
	{
		ent->freeAfterEvent = qfalse; //it will free itself
	}

	trap_LinkEntity( ent );
}

/*
================
G_RunMissile
================
*/
extern void WP_flechette_alt_blow( gentity_t *ent ); //debug
void G_RunMissile( gentity_t *ent ) {
	vec3_t		origin, groundSpot;
	trace_t		tr;
	int			passent = ENTITYNUM_NONE;
	qboolean	isKnockedSaber = qfalse;
	const int adjustedClipMask = g_fixSaberDefense.integer >= 2 ? (ent->clipmask & ~CONTENTS_LIGHTSABER) : ent->clipmask;

	VectorSet(groundSpot, 0, 0, 0);

	//prevent golan-spamming the lift
	if (level.hangarCompletedTime && ent->s.weapon == WP_FLECHETTE && !iLikeToDoorSpam.integer)
	{
		if (ent->r.currentOrigin[0] >= -1216 && ent->r.currentOrigin[0] <= -996 && ent->r.currentOrigin[1] >= -128 && ent->r.currentOrigin[1] <= 142 && ent->r.currentOrigin[2] <= 120)
		{
			for (int n = 32; n < MAX_GENTITIES; n++)
			{
				if (&g_entities[n] && !Q_stricmp(g_entities[n].targetname, "hangarplatbig1"))
				{
					if (g_entities[n].moverState == MOVER_POS1 || g_entities[n].moverState == MOVER_1TO2)
					{
						//lift is at the bottom or in the process of going up
						if (&g_entities[ent->r.ownerNum] && g_entities[ent->r.ownerNum].r.currentOrigin[2] >= 184)
						{
							//this height detection will make sure the "spammer" isn't on the lift.
							//prevents you from getting fucked by golan-spam detection when you are simply fighting someone who next to you on the lift
							//(the idea is that you are considered spamming if you are not on the lift, firing golans into the lift)
							ent->think = G_FreeEntity;
							ent->nextthink = level.time;
							return;
						}
					}
				}
			}
		}
	}

	if (ent->neverFree && ent->s.weapon == WP_SABER && (ent->flags & FL_BOUNCE_HALF))
	{
		isKnockedSaber = qtrue;
		ent->s.pos.trType = TR_GRAVITY;
	}

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// if this missile bounced off an invulnerability sphere
	if ( ent->target_ent ) {
		passent = ent->target_ent->s.number;
	}
	else {
		// ignore interactions with the missile owner
		if ( (ent->r.svFlags&SVF_OWNERNOTSHARED) 
			&& (ent->s.eFlags&EF_JETPACK_ACTIVE) )
		{//A vehicle missile that should be solid to its owner
			//I don't care about hitting my owner
			passent = ent->s.number;
		}
		else
		{
			passent = ent->r.ownerNum;
		}
	}
	// trace a line from the previous position to the current position
	if (d_projectileGhoul2Collision.integer)
	{
		trap_G2Trace(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, adjustedClipMask, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);

		if (tr.fraction != 1.0 && tr.entityNum < ENTITYNUM_WORLD)
		{
			gentity_t *g2Hit = &g_entities[tr.entityNum];

			if (g2Hit->inuse && g2Hit->client && g2Hit->ghoul2)
			{ //since we used G2TRFLAG_GETSURFINDEX, tr.surfaceFlags will actually contain the index of the surface on the ghoul2 model we collided with.
				g2Hit->client->g2LastSurfaceHit = tr.surfaceFlags;
				g2Hit->client->g2LastSurfaceTime = level.time;
			}

			if (g2Hit->ghoul2)
			{
				tr.surfaceFlags = 0; //clear the surface flags after, since we actually care about them in here.
			}
		}
		}
		else
		{
			trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, adjustedClipMask);
		}

	if ( tr.startsolid || tr.allsolid ) {
		// make sure the tr.entityNum is set to the entity we're stuck in
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, adjustedClipMask);
		tr.fraction = 0;
	}
	else {
		VectorCopy( tr.endpos, ent->r.currentOrigin );
	}

	if (ent->passThroughNum && tr.entityNum == (ent->passThroughNum-1))
	{
		VectorCopy( origin, ent->r.currentOrigin );
		trap_LinkEntity( ent );
		goto passthrough;
	}

	trap_LinkEntity( ent );

	if (ent->s.weapon == G2_MODEL_PART && !ent->bounceCount)
	{
		vec3_t lowerOrg;
		trace_t trG;

		VectorCopy(ent->r.currentOrigin, lowerOrg);
		lowerOrg[2] -= 1;
		trap_Trace( &trG, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, lowerOrg, passent, adjustedClipMask);

		VectorCopy(trG.endpos, groundSpot);

		if (!trG.startsolid && !trG.allsolid && trG.entityNum == ENTITYNUM_WORLD)
		{
			ent->s.groundEntityNum = trG.entityNum;
		}
		else
		{
			ent->s.groundEntityNum = ENTITYNUM_NONE;
		}
	}

	if ( tr.fraction != 1) {
		// never explode or bounce on sky
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			// If grapple, reset owner
			if (ent->parent && ent->parent->client && ent->parent->client->hook == ent) {
				ent->parent->client->hook = NULL;
			}

			if ((ent->s.weapon == WP_SABER && ent->isSaberEntity) || isKnockedSaber)
			{
				G_RunThink( ent );
				return;
			}
			else if (ent->s.weapon != G2_MODEL_PART)
			{
				G_FreeEntity( ent );
				return;
			}
		}

#if 0 //will get stomped with missile impact event...
		if (ent->s.weapon > WP_NONE && ent->s.weapon < WP_NUM_WEAPONS &&
			(tr.entityNum < MAX_CLIENTS || g_entities[tr.entityNum].s.eType == ET_NPC))
		{ //player or NPC, try making a mark on him

			//ok, let's try adding it to the missile ent instead (tempents bad!)
			G_AddEvent(ent, EV_GHOUL2_MARK, 0);

			//copy current pos to s.origin, and current projected traj to origin2
			VectorCopy(ent->r.currentOrigin, ent->s.origin);
			BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->s.origin2 );

			//the index for whoever we are hitting
			ent->s.otherEntityNum = tr.entityNum;

			if (VectorCompare(ent->s.origin, ent->s.origin2))
			{
				ent->s.origin2[2] += 2.0f; //whatever, at least it won't mess up.
			}
		}
#else
		if (ent->s.weapon > WP_NONE && ent->s.weapon < WP_NUM_WEAPONS &&
			(tr.entityNum < MAX_CLIENTS || g_entities[tr.entityNum].s.eType == ET_NPC))
		{ //player or NPC, try making a mark on him
			//copy current pos to s.origin, and current projected traj to origin2
			VectorCopy(ent->r.currentOrigin, ent->s.origin);
			BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->s.origin2 );

			if (VectorCompare(ent->s.origin, ent->s.origin2))
			{
				ent->s.origin2[2] += 2.0f; //whatever, at least it won't mess up.
			}
		}
#endif

		G_MissileImpact( ent, &tr );

		if (tr.entityNum == ent->s.otherEntityNum)
		{ //if the impact event other and the trace ent match then it's ok to do the g2 mark
			ent->s.trickedentindex = 1;
		}

		if ( ent->s.eType != ET_MISSILE && ent->s.weapon != G2_MODEL_PART )
		{
			return;		// exploded
		}
	}

passthrough:
	if ( ent->s.pos.trType == TR_STATIONARY && (ent->s.eFlags&EF_MISSILE_STICK) )
	{//stuck missiles should check some special stuff
		G_RunStuckMissile( ent );
		return;
	}

	if (ent->s.weapon == G2_MODEL_PART)
	{
		if (ent->s.groundEntityNum == ENTITYNUM_WORLD)
		{
			ent->s.pos.trType = TR_LINEAR;
			VectorClear(ent->s.pos.trDelta);
			ent->s.pos.trTime = level.time;

			VectorCopy(groundSpot, ent->s.pos.trBase);
			VectorCopy(groundSpot, ent->r.currentOrigin);

			if (ent->s.apos.trType != TR_STATIONARY)
			{
				ent->s.apos.trType = TR_STATIONARY;
				ent->s.apos.trTime = level.time;

				ent->s.apos.trBase[ROLL] = 0;
				ent->s.apos.trBase[PITCH] = 0;
			}
		}
	}

	// check think function after bouncing
	G_RunThink( ent );
}


//=============================================================================




