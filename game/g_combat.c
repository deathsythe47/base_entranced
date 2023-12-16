// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_combat.c

//#include "g_local.h"
#include "b_local.h"
#include "bg_saga.h"

extern int G_ShipSurfaceForSurfName( const char *surfaceName );
extern qboolean G_FlyVehicleDestroySurface( gentity_t *veh, int surface );
extern void G_VehicleSetDamageLocFlags( gentity_t *veh, int impactDir, int deathPoint );
extern void G_VehUpdateShields( gentity_t *targ );
extern void G_LetGoOfWall( gentity_t *ent );
extern void BG_ClearRocketLock( playerState_t *ps );
//rww - pd
void BotDamageNotification(gclient_t *bot, gentity_t *attacker);
//end rww

void ThrowSaberToAttacker(gentity_t *self, gentity_t *attacker);

void ObjectDie (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	if(self->target)
	{
		G_UseTargets(self, attacker);
	}

	//remove my script_targetname
	G_FreeEntity( self );
}

qboolean G_HeavyMelee( gentity_t *attacker )
{
	if (g_gametype.integer == GT_SIEGE 
		&& attacker 
		&& attacker->client
		&& attacker->client->siegeClass != -1 
		&& (bgSiegeClasses[attacker->client->siegeClass].classflags & (1<<CFL_HEAVYMELEE)) )
	{
		return qtrue;
	}
	return qfalse;
}

int G_GetHitLocation(gentity_t *target, vec3_t ppoint)
{
	vec3_t			point, point_dir;
	vec3_t			forward, right, up;
	vec3_t			tangles, tcenter;
	float			udot, fdot, rdot;
	int				Vertical, Forward, Lateral;
	int				HitLoc;

	// Get target forward, right and up.
	if(target->client)
	{
		// Ignore player's pitch and roll.
		VectorSet(tangles, 0, target->r.currentAngles[YAW], 0);
	}

	AngleVectors(tangles, forward, right, up);

	// Get center of target.
	VectorAdd(target->r.absmin, target->r.absmax, tcenter);
	VectorScale(tcenter, 0.5, tcenter);

	// Get radius width of target.

	// Get impact point.
	if(ppoint && !VectorCompare(ppoint, vec3_origin))
	{
		VectorCopy(ppoint, point);
	}
	else
	{
		return HL_NONE;
	}

	VectorSubtract(point, tcenter, point_dir);
	VectorNormalize(point_dir);

	// Get bottom to top (vertical) position index
	udot = DotProduct(up, point_dir);
	if(udot>.800)
	{
		Vertical = 4;
	}
	else if(udot>.400)
	{
		Vertical = 3;
	}
	else if(udot>-.333)
	{
		Vertical = 2;
	}
	else if(udot>-.666)
	{
		Vertical = 1;
	}
	else
	{
		Vertical = 0;
	}

	// Get back to front (forward) position index.
	fdot = DotProduct(forward, point_dir);
	if(fdot>.666)
	{
		Forward = 4;
	}
	else if(fdot>.333)
	{
		Forward = 3;
	}
	else if(fdot>-.333)
	{
		Forward = 2;
	}
	else if(fdot>-.666)
	{
		Forward = 1;
	}
	else
	{
		Forward = 0;
	}

	// Get left to right (lateral) position index.
	rdot = DotProduct(right, point_dir);
	if(rdot>.666)
	{
		Lateral = 4;
	}
	else if(rdot>.333)
	{
		Lateral = 3;
	}
	else if(rdot>-.333)
	{
		Lateral = 2;
	}
	else if(rdot>-.666)
	{
		Lateral = 1;
	}
	else
	{
		Lateral = 0;
	}

	HitLoc = Vertical * 25 + Forward * 5 + Lateral;

	if(HitLoc <= 10)
	{
		// Feet.
		if ( rdot > 0 )
		{
			return HL_FOOT_RT;
		}
		else
		{
			return HL_FOOT_LT;
		}
	}
	else if(HitLoc <= 50)
	{
		// Legs.
		if ( rdot > 0 )
		{
			return HL_LEG_RT;
		}
		else
		{
			return HL_LEG_LT;
		}
	}
	else if(HitLoc == 56||HitLoc == 60||HitLoc == 61||HitLoc == 65||HitLoc == 66||HitLoc == 70)
	{
		// Hands.
		if ( rdot > 0 )
		{
			return HL_HAND_RT;
		}
		else
		{
			return HL_HAND_LT;
		}
	}
	else if(HitLoc == 83||HitLoc == 87||HitLoc == 88||HitLoc == 92||HitLoc == 93||HitLoc == 97)
	{
		// Arms.
		if ( rdot > 0 )
		{
			return HL_ARM_RT;
		}
		else
		{
			return HL_ARM_LT;
		}
	}
	else if((HitLoc >= 107 && HitLoc <= 109)||(HitLoc >= 112 && HitLoc <= 114)||(HitLoc >= 117 && HitLoc <= 119))
	{
		// Head.
		return HL_HEAD;
	}
	else
	{
		if(udot < 0.3)
		{
			return HL_WAIST;
		}
		else if(fdot < 0)
		{
			if(rdot > 0.4)
			{
				return HL_BACK_RT;
			}
			else if(rdot < -0.4)
			{
				return HL_BACK_LT;
			}
			else if(fdot < 0)
			{
				return HL_BACK;
			}
		}
		else
		{
			if(rdot > 0.3)
			{
				return HL_CHEST_RT;
			}
			else if(rdot < -0.3)
			{
				return HL_CHEST_LT;
			}
			else if(fdot < 0)
			{
				return HL_CHEST;
			}
		}
	}
	return HL_NONE;
}

void ExplodeDeath( gentity_t *self ) 
{
	vec3_t		forward;

	self->takedamage = qfalse;//stop chain reaction runaway loops

	self->s.loopSound = 0;
	self->s.loopIsSoundset = qfalse;

	VectorCopy( self->r.currentOrigin, self->s.pos.trBase );

	AngleVectors(self->s.angles, forward, NULL, NULL);

	if(self->splashDamage > 0 && self->splashRadius > 0)
	{
		gentity_t *attacker = self;
		if ( self->parent )
		{
			attacker = self->parent;
		}
		G_RadiusDamage( self->r.currentOrigin, attacker, self->splashDamage, self->splashRadius, 
				attacker, NULL, MOD_UNKNOWN );
	}

	ObjectDie( self, self, self, 20, 0 );
}


/*
============
ScorePlum
============
*/
void ScorePlum( gentity_t *ent, vec3_t origin, int score ) {
	gentity_t *plum;

	plum = G_TempEntity( origin, EV_SCOREPLUM );
	// only send this temp entity to a single client
	plum->r.svFlags |= SVF_SINGLECLIENT;
	plum->r.singleClient = ent->s.number;
	//
	plum->s.otherEntityNum = ent->s.number;
	plum->s.time = score;
}

/*
============
AddScore

Adds score to both the client and his team
============
*/
extern qboolean g_dontPenalizeTeam; //g_cmds.c
void AddScore( gentity_t *ent, vec3_t origin, int score )
{
	if ( !ent->client ) {
		return;
	}
	// no scoring during pre-match warmup
	if ( level.warmupTime ) {
		return;
	}

	ent->client->ps.persistant[PERS_SCORE] += score;
	if ( g_gametype.integer == GT_TEAM && !g_dontPenalizeTeam )
		level.teamScores[ ent->client->ps.persistant[PERS_TEAM] ] += score;
	CalculateRanks();
}

/*
=================
TossClientItems

rww - Toss the weapon away from the player in the specified direction
=================
*/
void TossClientWeapon(gentity_t *self, vec3_t direction, float speed)
{
	vec3_t vel;
	gitem_t *item;
	gentity_t *launched;
	int weapon = self->s.weapon;
	int ammoSub;

	if (g_gametype.integer == GT_SIEGE)
	{ //no dropping weaps
		return;
	}

	if (weapon <= WP_BRYAR_PISTOL)
	{ //can't have this
		return;
	}

	if (weapon == WP_EMPLACED_GUN ||
		weapon == WP_TURRET)
	{
		return;
	}

	// find the item type for this weapon
	item = BG_FindItemForWeapon( weapon );

	ammoSub = (self->client->ps.ammo[weaponData[weapon].ammoIndex] - bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity);

	if (ammoSub < 0)
	{
		int ammoQuan = item->quantity;
		ammoQuan -= (-ammoSub);

		if (ammoQuan <= 0)
		{ //no ammo
			return;
		}
	}

	vel[0] = direction[0]*speed;
	vel[1] = direction[1]*speed;
	vel[2] = direction[2]*speed;

	launched = LaunchItem(item, self->client->ps.origin, vel);

	launched->s.generic1 = self->s.number;
	launched->s.powerups = level.time + 1500;

	launched->count = bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity;

	self->client->ps.ammo[weaponData[weapon].ammoIndex] -= bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity;

	if (self->client->ps.ammo[weaponData[weapon].ammoIndex] < 0)
	{
		launched->count -= (-self->client->ps.ammo[weaponData[weapon].ammoIndex]);
		self->client->ps.ammo[weaponData[weapon].ammoIndex] = 0;
	}

	if ((self->client->ps.ammo[weaponData[weapon].ammoIndex] < 1 && weapon != WP_DET_PACK) ||
		(weapon != WP_THERMAL && weapon != WP_DET_PACK && weapon != WP_TRIP_MINE))
	{
		int i = 0;
		int weap = -1;

		self->client->ps.stats[STAT_WEAPONS] &= ~(1 << weapon);

		while (i < WP_NUM_WEAPONS)
		{
			if ((self->client->ps.stats[STAT_WEAPONS] & (1 << i)) && i != WP_NONE)
			{ //this one's good
				weap = i;
				break;
			}
			i++;
		}

		if (weap != -1)
		{
			self->s.weapon = weap;
			self->client->ps.weapon = weap;
		}
		else
		{
			self->s.weapon = 0;
			self->client->ps.weapon = 0;
		}

		G_AddEvent(self, EV_NOAMMO, weapon);
	}

	if ( weapon == WP_DISRUPTOR) 
	{
		self->client->ps.zoomMode = 0;
		self->client->ps.zoomLocked = qfalse;
		self->client->ps.zoomLockTime = 0;
	}
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
void TossClientItems( gentity_t *self ) {
	gitem_t		*item;
	int			weapon;
	float		angle;
	int			i;
	gentity_t	*drop;

	if (g_gametype.integer == GT_SIEGE)
	{ //just don't drop anything then
		return;
	}

	// drop the weapon if not a gauntlet or machinegun
	weapon = self->s.weapon;

	// make a special check to see if they are changing to a new
	// weapon that isn't the mg or gauntlet.  Without this, a client
	// can pick up a weapon, be killed, and not drop the weapon because
	// their weapon change hasn't completed yet and they are still holding the MG.
	if ( weapon == WP_BRYAR_PISTOL) {
		if ( self->client->ps.weaponstate == WEAPON_DROPPING ) {
			weapon = self->client->pers.cmd.weapon;
		}
		if ( !( self->client->ps.stats[STAT_WEAPONS] & ( 1 << weapon ) ) ) {
			weapon = WP_NONE;
		}
	}

	self->s.bolt2 = weapon;

	if ( weapon > WP_BRYAR_PISTOL && 
		weapon != WP_EMPLACED_GUN &&
		weapon != WP_TURRET &&
		self->client->ps.ammo[ weaponData[weapon].ammoIndex ] ) {
		gentity_t *te;

		// find the item type for this weapon
		item = BG_FindItemForWeapon( weapon );

		// tell all clients to remove the weapon model on this guy until he respawns
		te = G_TempEntity( vec3_origin, EV_DESTROY_WEAPON_MODEL );
		te->r.svFlags |= SVF_BROADCAST;
		te->s.eventParm = self->s.number;

		// spawn the item
		Drop_Item( self, item, 0 );
	}

	// drop all the powerups if not in teamplay
	if ( g_gametype.integer != GT_TEAM && g_gametype.integer != GT_SIEGE ) {
		angle = 45;
		for ( i = 1 ; i < PW_NUM_POWERUPS ; i++ ) {

			if ( self->client->ps.powerups[ i ] > level.time ) {

				item = BG_FindItemForPowerup( i );
				if ( !item ) {
					continue;
				}

				drop = Drop_Item( self, item, angle );
				// decide how many seconds it has left
				drop->count = ( self->client->ps.powerups[ i ] - level.time ) / 1000;
				if ( drop->count < 1 ) {
					drop->count = 1;
				}
				angle += 45;
			} 
		}
	}
}


/*
==================
LookAtKiller
==================
*/
void LookAtKiller( gentity_t *self, gentity_t *inflictor, gentity_t *attacker ) {
	vec3_t		dir;

	if ( attacker && attacker != self ) {
		VectorSubtract (attacker->s.pos.trBase, self->s.pos.trBase, dir);
	} else if ( inflictor && inflictor != self ) {
		VectorSubtract (inflictor->s.pos.trBase, self->s.pos.trBase, dir);
	} else {
		self->client->ps.stats[STAT_DEAD_YAW] = self->s.angles[YAW];
		return;
	}

	self->client->ps.stats[STAT_DEAD_YAW] = vectoyaw ( dir );

}

/*
==================
GibEntity
==================
*/
void GibEntity( gentity_t *self, int killer ) {
	G_AddEvent( self, EV_GIB_PLAYER, killer );
	self->takedamage = qfalse;
	self->s.eType = ET_INVISIBLE;
	self->r.contents = 0;
}

void BodyRid(gentity_t *ent)
{
	trap_UnlinkEntity( ent );
	ent->physicsObject = qfalse;
}

/*
==================
body_die
==================
*/
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	// NOTENOTE No gibbing right now, this is star wars.
	qboolean doDisint = qfalse;

	if (self->s.eType == ET_NPC)
	{ //well, just rem it then, so long as it's done with its death anim and it's not a standard weapon.
		if ( self->client && self->client->ps.torsoTimer <= 0 &&
			 (meansOfDeath == MOD_UNKNOWN ||
			  meansOfDeath == MOD_WATER ||
			  meansOfDeath == MOD_SLIME ||
			  meansOfDeath == MOD_LAVA ||
			  meansOfDeath == MOD_CRUSH ||
			  meansOfDeath == MOD_TELEFRAG ||
			  meansOfDeath == MOD_FALLING ||
			  meansOfDeath == MOD_SUICIDE ||
			  meansOfDeath == MOD_TARGET_LASER ||
			  meansOfDeath == MOD_TRIGGER_HURT) )
		{
			self->think = G_FreeEntity;
			self->nextthink = level.time;
		}
		return;
	}

	if (self->health < (GIB_HEALTH+1))
	{
		self->health = GIB_HEALTH+1;

		if (self->client && (level.time - self->client->respawnTime) < 2000)
		{
			doDisint = qfalse;
		}
		else
		{
			doDisint = qtrue;
		}
	}

	if (self->client && (self->client->ps.eFlags & EF_DISINTEGRATION))
	{
		return;
	}
	else if (self->s.eFlags & EF_DISINTEGRATION)
	{
		return;
	}

	if (doDisint)
	{
		if (self->client)
		{
			self->client->ps.eFlags |= EF_DISINTEGRATION;
			VectorCopy(self->client->ps.origin, self->client->ps.lastHitLoc);
		}
		else
		{
			self->s.eFlags |= EF_DISINTEGRATION;
			VectorCopy(self->r.currentOrigin, self->s.origin2);

			//since it's the corpse entity, tell it to "remove" itself
			self->think = BodyRid;
			self->nextthink = level.time + 1000;
		}
		return;
	}
}


// these are just for logging, the client prints its own messages
char	*modNames[MOD_MAX] = {
	"MOD_UNKNOWN",
	"MOD_STUN_BATON",
	"MOD_MELEE",
	"MOD_SABER",
	"MOD_BRYAR_PISTOL",
	"MOD_BRYAR_PISTOL_ALT",
	"MOD_BLASTER",
	"MOD_TURBLAST",
	"MOD_DISRUPTOR",
	"MOD_DISRUPTOR_SPLASH",
	"MOD_DISRUPTOR_SNIPER",
	"MOD_BOWCASTER",
	"MOD_REPEATER",
	"MOD_REPEATER_ALT",
	"MOD_REPEATER_ALT_SPLASH",
	"MOD_DEMP2",
	"MOD_DEMP2_ALT",
	"MOD_FLECHETTE",
	"MOD_FLECHETTE_ALT_SPLASH",
	"MOD_ROCKET",
	"MOD_ROCKET_SPLASH",
	"MOD_ROCKET_HOMING",
	"MOD_ROCKET_HOMING_SPLASH",
	"MOD_THERMAL",
	"MOD_THERMAL_SPLASH",
	"MOD_TRIP_MINE_SPLASH",
	"MOD_TIMED_MINE_SPLASH",
	"MOD_DET_PACK_SPLASH",
	"MOD_VEHICLE",
	"MOD_CONC",
	"MOD_CONC_ALT",
	"MOD_FORCE_DARK",
	"MOD_SENTRY",
	"MOD_WATER",
	"MOD_SLIME",
	"MOD_LAVA",
	"MOD_CRUSH",
	"MOD_TELEFRAG",
	"MOD_FALLING",
	"MOD_SUICIDE",
	"MOD_TARGET_LASER",
	"MOD_TRIGGER_HURT"
};


/*
==================
CheckAlmostCapture
==================
*/
void CheckAlmostCapture( gentity_t *self, gentity_t *attacker ) {
	gentity_t	*ent;
	vec3_t		dir;
	char		*classname;

	// if this player was carrying a flag
	if ( self->client->ps.powerups[PW_REDFLAG] ||
		self->client->ps.powerups[PW_BLUEFLAG] ||
		self->client->ps.powerups[PW_NEUTRALFLAG] ) {
		// get the goal flag this player should have been going for
		if ( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY ) {
			if ( self->client->sess.sessionTeam == TEAM_BLUE ) {
				classname = "team_CTF_blueflag";
			}
			else {
				classname = "team_CTF_redflag";
			}
		}
		else {
			if ( self->client->sess.sessionTeam == TEAM_BLUE ) {
				classname = "team_CTF_redflag";
			}
			else {
				classname = "team_CTF_blueflag";
			}
		}
		ent = NULL;
		do
		{
			ent = G_Find(ent, FOFS(classname), classname);
		} while (ent && (ent->flags & FL_DROPPED_ITEM));
		// if we found the destination flag and it's not picked up
		if (ent && !(ent->r.svFlags & SVF_NOCLIENT) ) {
			// if the player was *very* close
			VectorSubtract( self->client->ps.origin, ent->s.origin, dir );
			if ( VectorLength(dir) < 200 ) {
				//self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				if ( attacker->client && attacker != self ) { // we don't want this to trigger by our own sk's
					self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
					++attacker->client->pers.teamState.saves;
				}
			}
		}
	}
}

qboolean G_InKnockDown( playerState_t *ps )
{
	switch ( (ps->legsAnim) )
	{
	case BOTH_KNOCKDOWN1:
	case BOTH_KNOCKDOWN2:
	case BOTH_KNOCKDOWN3:
	case BOTH_KNOCKDOWN4:
	case BOTH_KNOCKDOWN5:
		return qtrue;
		break;
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
		return qtrue;
		break;
	}
	return qfalse;
}

static int G_CheckSpecialDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc )
{
	int deathAnim = -1;

	if ( BG_InRoll( &self->client->ps, self->client->ps.legsAnim ) )
	{
		deathAnim = BOTH_DEATH_ROLL;		//# Death anim from a roll
	}
	else if ( BG_FlippingAnim( self->client->ps.legsAnim ) )
	{
		deathAnim = BOTH_DEATH_FLIP;		//# Death anim from a flip
	}
	else if ( G_InKnockDown( &self->client->ps ) )
	{//since these happen a lot, let's handle them case by case
		int animLength = bgAllAnims[self->localAnimIndex].anims[self->client->ps.legsAnim].numFrames * fabs((float)(bgHumanoidAnimations[self->client->ps.legsAnim].frameLerp));
		switch ( self->client->ps.legsAnim )
		{
		case BOTH_KNOCKDOWN1:
			if ( animLength - self->client->ps.legsTimer > 100 )
			{//on our way down
				if ( self->client->ps.legsTimer > 600 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_KNOCKDOWN2:
			if ( animLength - self->client->ps.legsTimer > 700 )
			{//on our way down
				if ( self->client->ps.legsTimer > 600 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_KNOCKDOWN3:
			if ( animLength - self->client->ps.legsTimer > 100 )
			{//on our way down
				if ( self->client->ps.legsTimer > 1300 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_KNOCKDOWN4:
			if ( animLength - self->client->ps.legsTimer > 300 )
			{//on our way down
				if ( self->client->ps.legsTimer > 350 )
				{//still partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			else
			{//crouch death
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			break;
		case BOTH_KNOCKDOWN5:
			if ( self->client->ps.legsTimer < 750 )
			{//flat
				deathAnim = BOTH_DEATH_LYING_DN;
			}
			break;
		case BOTH_GETUP1:
			if ( self->client->ps.legsTimer < 350 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 800 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );
				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 450 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP2:
			if ( self->client->ps.legsTimer < 150 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 850 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 500 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP3:
			if ( self->client->ps.legsTimer < 250 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 600 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;
				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 150 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_GETUP4:
			if ( self->client->ps.legsTimer < 250 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 600 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 850 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP5:
			if ( self->client->ps.legsTimer > 850 )
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 1500 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_GETUP_CROUCH_B1:
			if ( self->client->ps.legsTimer < 800 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 400 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_GETUP_CROUCH_F1:
			if ( self->client->ps.legsTimer < 800 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 150 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B1:
			if ( self->client->ps.legsTimer < 325 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 725 )
			{//spinning up
				deathAnim = BOTH_DEATH_SPIN_180;	//# Death anim when facing backwards
			}
			else if ( self->client->ps.legsTimer < 900 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 50 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B2:
			if ( self->client->ps.legsTimer < 575 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 875 )
			{//spinning up
				deathAnim = BOTH_DEATH_SPIN_180;	//# Death anim when facing backwards
			}
			else if ( self->client->ps.legsTimer < 900 )
			{//crouching
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else
			{//lying down
				//partially up
				deathAnim = BOTH_DEATH_FALLING_UP;
			}
			break;
		case BOTH_FORCE_GETUP_B3:
			if ( self->client->ps.legsTimer < 150 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 775 )
			{//flipping
				deathAnim = BOTH_DEATHBACKWARD2; //backflip
			}
			else
			{//lying down
				//partially up
				deathAnim = BOTH_DEATH_FALLING_UP;
			}
			break;
		case BOTH_FORCE_GETUP_B4:
			if ( self->client->ps.legsTimer < 325 )
			{//standing up
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 150 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B5:
			if ( self->client->ps.legsTimer < 550 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 1025 )
			{//kicking up
				deathAnim = BOTH_DEATHBACKWARD2; //backflip
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 50 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_B6:
			if ( self->client->ps.legsTimer < 225 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 425 )
			{//crouching up
				vec3_t fwd;
				float thrown = 0;

				AngleVectors( self->client->ps.viewangles, fwd, NULL, NULL );
				thrown = DotProduct( fwd, self->client->ps.velocity );

				if ( thrown < -150 )
				{
					deathAnim = BOTH_DEATHBACKWARD1;	//# Death anim when crouched and thrown back
				}
				else
				{
					deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
				}
			}
			else if ( self->client->ps.legsTimer < 825 )
			{//flipping up
				deathAnim = BOTH_DEATHFORWARD3; //backflip
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 225 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_UP;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_UP;
				}
			}
			break;
		case BOTH_FORCE_GETUP_F1:
			if ( self->client->ps.legsTimer < 275 )
			{//standing up
			}
			else if ( self->client->ps.legsTimer < 750 )
			{//flipping
				deathAnim = BOTH_DEATH14;
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 100 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		case BOTH_FORCE_GETUP_F2:
			if ( self->client->ps.legsTimer < 1200 )
			{//standing
			}
			else
			{//lying down
				if ( animLength - self->client->ps.legsTimer > 225 )
				{//partially up
					deathAnim = BOTH_DEATH_FALLING_DN;
				}
				else
				{//down
					deathAnim = BOTH_DEATH_LYING_DN;
				}
			}
			break;
		}
	}

	return deathAnim;
}

int G_PickDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc )
{//FIXME: play dead flop anims on body if in an appropriate _DEAD anim when this func is called
	int deathAnim = -1;
	int max_health;
	int legAnim = 0;
	vec3_t objVelocity;

	if (!self || !self->client)
	{
		if (!self || self->s.eType != ET_NPC)
		{ //g2animent
			return 0;
		}
	}

	if (self->client)
	{
		max_health = self->client->ps.stats[STAT_MAX_HEALTH];

		if (self->client->inSpaceIndex && self->client->inSpaceIndex != ENTITYNUM_NONE)
		{
			return BOTH_CHOKE3;
		}
	}
	else
	{
		max_health = 60;
	}

	if (self->client)
	{
		VectorCopy(self->client->ps.velocity, objVelocity);
	}
	else
	{
		VectorCopy(self->s.pos.trDelta, objVelocity);
	}

	if ( hitLoc == HL_NONE )
	{
		hitLoc = G_GetHitLocation( self, point );//self->hitLoc
	}

	if (self->client)
	{
		legAnim = self->client->ps.legsAnim;
	}
	else
	{
		legAnim = self->s.legsAnim;
	}

	if (gGAvoidDismember)
	{
		return BOTH_RIGHTHANDCHOPPEDOFF;
	}

	//dead flops
	switch( legAnim )
	{
	case BOTH_DEATH1:		//# First Death anim
	case BOTH_DEAD1:
	case BOTH_DEATH2:			//# Second Death anim
	case BOTH_DEAD2:
	case BOTH_DEATH8:			//# 
	case BOTH_DEAD8:
	case BOTH_DEATH13:			//# 
	case BOTH_DEAD13:
	case BOTH_DEATH14:			//# 
	case BOTH_DEAD14:
	case BOTH_DEATH16:			//# 
	case BOTH_DEAD16:
	case BOTH_DEADBACKWARD1:		//# First thrown backward death finished pose
	case BOTH_DEADBACKWARD2:		//# Second thrown backward death finished pose
		deathAnim = -2;
	case BOTH_DEATH10:			//# 
	case BOTH_DEAD10:
	case BOTH_DEATH15:			//# 
	case BOTH_DEAD15:
	case BOTH_DEADFORWARD1:		//# First thrown forward death finished pose
	case BOTH_DEADFORWARD2:		//# Second thrown forward death finished pose
		deathAnim = -2;
	case BOTH_DEADFLOP1:
		deathAnim = -2;
		break;
	case BOTH_DEAD3:				//# Third Death finished pose
	case BOTH_DEAD4:				//# Fourth Death finished pose
	case BOTH_DEAD5:				//# Fifth Death finished pose
	case BOTH_DEAD6:				//# Sixth Death finished pose
	case BOTH_DEAD7:				//# Seventh Death finished pose
	case BOTH_DEAD9:				//# 
	case BOTH_DEAD11:			//#
	case BOTH_DEAD12:			//# 
	case BOTH_DEAD17:			//# 
	case BOTH_DEAD18:			//# 
	case BOTH_DEAD19:			//# 
	case BOTH_LYINGDEAD1:		//# Killed lying down death finished pose
	case BOTH_STUMBLEDEAD1:		//# Stumble forward death finished pose
	case BOTH_FALLDEAD1LAND:		//# Fall forward and splat death finished pose
	case BOTH_DEATH3:			//# Third Death anim
	case BOTH_DEATH4:			//# Fourth Death anim
	case BOTH_DEATH5:			//# Fifth Death anim
	case BOTH_DEATH6:			//# Sixth Death anim
	case BOTH_DEATH7:			//# Seventh Death anim
	case BOTH_DEATH9:			//# 
	case BOTH_DEATH11:			//#
	case BOTH_DEATH12:			//# 
	case BOTH_DEATH17:			//# 
	case BOTH_DEATH18:			//# 
	case BOTH_DEATH19:			//# 
	case BOTH_DEATHFORWARD1:		//# First Death in which they get thrown forward
	case BOTH_DEATHFORWARD2:		//# Second Death in which they get thrown forward
	case BOTH_DEATHBACKWARD1:	//# First Death in which they get thrown backward
	case BOTH_DEATHBACKWARD2:	//# Second Death in which they get thrown backward
	case BOTH_DEATH1IDLE:		//# Idle while close to death
	case BOTH_LYINGDEATH1:		//# Death to play when killed lying down
	case BOTH_STUMBLEDEATH1:		//# Stumble forward and fall face first death
	case BOTH_FALLDEATH1:		//# Fall forward off a high cliff and splat death - start
	case BOTH_FALLDEATH1INAIR:	//# Fall forward off a high cliff and splat death - loop
	case BOTH_FALLDEATH1LAND:	//# Fall forward off a high cliff and splat death - hit bottom
		deathAnim = -2;
		break;
	}
	if ( deathAnim == -1 )
	{
		if (self->client)
		{
			deathAnim = G_CheckSpecialDeathAnim( self, point, damage, mod, hitLoc );
		}

		if (deathAnim == -1)
		{
			//death anims
			switch( hitLoc )
			{
			case HL_FOOT_RT:
			case HL_FOOT_LT:
				if ( mod == MOD_SABER && !Q_irand( 0, 2 ) )
				{
					return BOTH_DEATH10;//chest: back flip
				}
				else if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_LEG_RT:
				if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_LEG_LT:
				if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_BACK:
				if ( !VectorLengthSquared( objVelocity ) )
				{
					deathAnim = BOTH_DEATH17;//head/back: croak
				}
				else
				{
					if ( !Q_irand( 0, 2 ) )
					{
						deathAnim = BOTH_DEATH4;//back: forward
					}
					else if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH5;//same as 4
					}
					else
					{
						deathAnim = BOTH_DEATH15;//back: forward
					}
				}
				break;
			case HL_CHEST_RT:
			case HL_ARM_RT:
			case HL_HAND_RT:
			case HL_BACK_RT:
				if ( damage <= max_health*0.25 )
				{
					deathAnim = BOTH_DEATH9;//chest right: snap, fall forward
				}
				else if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH3;//chest right: back
				}
				else if ( damage <= max_health*0.75 )
				{
					deathAnim = BOTH_DEATH6;//chest right: spin
				}
				else 
				{
					//TEMP HACK: play spinny deaths less often
					if ( Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH8;//chest right: spin high
					}
					else
					{
						switch ( Q_irand( 0, 2 ) )
						{
						default:
						case 0:
							deathAnim = BOTH_DEATH9;//chest right: snap, fall forward
							break;
						case 1:
							deathAnim = BOTH_DEATH3;//chest right: back
							break;
						case 2:
							deathAnim = BOTH_DEATH6;//chest right: spin
							break;
						}
					}
				}
				break;
			case HL_CHEST_LT:
			case HL_ARM_LT:
			case HL_HAND_LT:
			case HL_BACK_LT:
				if ( damage <= max_health*0.25 )
				{
					deathAnim = BOTH_DEATH11;//chest left: snap, fall forward
				}
				else if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH7;//chest left: back
				}
				else if ( damage <= max_health*0.75 )
				{
					deathAnim = BOTH_DEATH12;//chest left: spin
				}
				else
				{
					//TEMP HACK: play spinny deaths less often
					if ( Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH14;//chest left: spin high
					}
					else
					{
						switch ( Q_irand( 0, 2 ) )
						{
						default:
						case 0:
							deathAnim = BOTH_DEATH11;//chest left: snap, fall forward
							break;
						case 1:
							deathAnim = BOTH_DEATH7;//chest left: back
							break;
						case 2:
							deathAnim = BOTH_DEATH12;//chest left: spin
							break;
						}
					}
				}
				break;
			case HL_CHEST:
			case HL_WAIST:
				if ( damage <= max_health*0.25 || !VectorLengthSquared( objVelocity ) )
				{
					if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH18;//gut: fall right
					}
					else
					{
						deathAnim = BOTH_DEATH19;//gut: fall left
					}
				}
				else if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH2;//chest: backward short
				}
				else if ( damage <= max_health*0.75 )
				{
					if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH1;//chest: backward med
					}
					else
					{
						deathAnim = BOTH_DEATH16;//same as 1
					}
				}
				else
				{
					deathAnim = BOTH_DEATH10;//chest: back flip
				}
				break;
			case HL_HEAD:
				if ( damage <= max_health*0.5 )
				{
					deathAnim = BOTH_DEATH17;//head/back: croak
				}
				else
				{
					deathAnim = BOTH_DEATH13;//head: stumble, fall back
				}
				break;
			default:
				break;
			}
		}
	}

	// Validate.....
	if ( deathAnim == -1 || !BG_HasAnimation( self->localAnimIndex, deathAnim ))
	{
		// I guess we'll take what we can get.....
		deathAnim = BG_PickAnim( self->localAnimIndex, BOTH_DEATH1, BOTH_DEATH25 );
	}

	return deathAnim;
}

gentity_t *G_GetJediMaster(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && ent->client->ps.isJediMaster)
		{
			return ent;
		}

		i++;
	}

	return NULL;
}

/*
-------------------------
G_AlertTeam
-------------------------
*/

void G_AlertTeam( gentity_t *victim, gentity_t *attacker, float radius, float soundDist )
{
	int			radiusEnts[ 128 ];
	gentity_t	*check;
	vec3_t		mins, maxs;
	int			numEnts;
	int			i;
	float		distSq, sndDistSq = (soundDist*soundDist);

	if ( attacker == NULL || attacker->client == NULL )
		return;

	//Setup the bbox to search in
	for ( i = 0; i < 3; i++ )
	{
		mins[i] = victim->r.currentOrigin[i] - radius;
		maxs[i] = victim->r.currentOrigin[i] + radius;
	}

	//Get the number of entities in a given space
	numEnts = trap_EntitiesInBox( mins, maxs, radiusEnts, 128 );

	//Cull this list
	for ( i = 0; i < numEnts; i++ )
	{
		check = &g_entities[radiusEnts[i]];

		//Validate clients
		if ( check->client == NULL )
			continue;

		//only want NPCs
		if ( check->NPC == NULL )
			continue;
		//This NPC specifically flagged to ignore alerts
		if ( check->NPC->scriptFlags & SCF_IGNORE_ALERTS )
			continue;

		//This NPC specifically flagged to ignore alerts
		if ( !(check->NPC->scriptFlags&SCF_LOOK_FOR_ENEMIES) )
			continue;

		//this ent does not participate in group AI
		if ( (check->NPC->scriptFlags&SCF_NO_GROUPS) )
			continue;

		//Skip the requested avoid check if present
		if ( check == victim )
			continue;

		//Skip the attacker
		if ( check == attacker )
			continue;

		//Must be on the same team
		if ( check->client->playerTeam != victim->client->playerTeam )
			continue;

		//Must be alive
		if ( check->health <= 0 )
			continue;

		if ( check->enemy == NULL )
		{//only do this if they're not already mad at someone
			distSq = DistanceSquared( check->r.currentOrigin, victim->r.currentOrigin );
			if ( distSq > 16384 /*128 squared*/ && !trap_InPVS( victim->r.currentOrigin, check->r.currentOrigin ) )
			{//not even potentially visible/hearable
				continue;
			}
			//NOTE: this allows sound alerts to still go through doors/PVS if the teammate is within 128 of the victim...
			if ( soundDist <= 0 || distSq > sndDistSq )
			{//out of sound range
				if ( !InFOV( victim, check, check->NPC->stats.hfov, check->NPC->stats.vfov ) 
					||  !NPC_ClearLOS2( check, victim->r.currentOrigin ) )
				{//out of FOV or no LOS
					continue;
				}
			}

			//FIXME: This can have a nasty cascading effect if setup wrong...
			G_SetEnemy( check, attacker );
		}
	}
}

/*
-------------------------
G_DeathAlert
-------------------------
*/

#define	DEATH_ALERT_RADIUS			512
#define	DEATH_ALERT_SOUND_RADIUS	512

void G_DeathAlert( gentity_t *victim, gentity_t *attacker )
{//FIXME: with all the other alert stuff, do we really need this?
	G_AlertTeam( victim, attacker, DEATH_ALERT_RADIUS, DEATH_ALERT_SOUND_RADIUS );
}

/*
----------------------------------------
DeathFX

Applies appropriate special effects that occur while the entity is dying
Not to be confused with NPC_RemoveBodyEffects (NPC.cpp), which only applies effect when removing the body
----------------------------------------
*/

void DeathFX( gentity_t *ent )
{
	vec3_t		effectPos, right;
	vec3_t		defaultDir;

	if ( !ent || !ent->client )
		return;

	VectorSet(defaultDir, 0, 0, 1);

	// team no longer indicates species/race.  NPC_class should be used to identify certain npc types
	switch(ent->client->NPC_class)
	{
	case CLASS_MOUSE:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 20;
		G_PlayEffectID( G_EffectIndex("env/small_explode"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mouse/misc/death1") );
		break;

	case CLASS_PROBE:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] += 50;
		G_PlayEffectID( G_EffectIndex("explosions/probeexplosion1"), effectPos, defaultDir );
		break;
		
	case CLASS_ATST: 
		AngleVectors( ent->r.currentAngles, NULL, right, NULL );
		VectorMA( ent->r.currentOrigin, 20, right, effectPos );
		effectPos[2] += 180;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		VectorMA( effectPos, -40, right, effectPos );
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		break;

	case CLASS_SEEKER:
	case CLASS_REMOTE:
		G_PlayEffectID( G_EffectIndex("env/small_explode"), ent->r.currentOrigin, defaultDir );
		break;

	case CLASS_GONK:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 5;
		G_Sound( ent, CHAN_AUTO, G_SoundIndex(va("sound/chars/gonk/misc/death%d.wav",Q_irand( 1, 3 ))) );
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		break;

	// should list all remaining droids here, hope I didn't miss any
	case CLASS_R2D2:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 10;
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark2/misc/mark2_explo") );
		break;

	case CLASS_PROTOCOL: //c3p0
	case CLASS_R5D2:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 10;
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark2/misc/mark2_explo") );
		break;

	case CLASS_MARK2:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 15;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark2/misc/mark2_explo") );
		break;

	case CLASS_INTERROGATOR:
		VectorCopy( ent->r.currentOrigin, effectPos );
		effectPos[2] -= 15;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/interrogator/misc/int_droid_explo") );
		break;

	case CLASS_MARK1:
		AngleVectors( ent->r.currentAngles, NULL, right, NULL );
		VectorMA( ent->r.currentOrigin, 10, right, effectPos );
		effectPos[2] -= 15;
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		VectorMA( effectPos, -20, right, effectPos );
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		VectorMA( effectPos, -20, right, effectPos );
		G_PlayEffectID( G_EffectIndex("explosions/droidexplosion1"), effectPos, defaultDir );
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/mark1/misc/mark1_explo") );
		break;

	case CLASS_SENTRY:
		G_Sound( ent, CHAN_AUTO, G_SoundIndex("sound/chars/sentry/misc/sentry_explo") );
		VectorCopy( ent->r.currentOrigin, effectPos );
		G_PlayEffectID( G_EffectIndex("env/med_explode"), effectPos, defaultDir );
		break;

	default:
		break;

	}

}

void G_CheckVictoryScript(gentity_t *self)
{
	if ( !G_ActivateBehavior( self, BSET_VICTORY ) )
	{
		if ( self->NPC && self->s.weapon == WP_SABER )
		{//Jedi taunt from within their AI
			self->NPC->blockedSpeechDebounceTime = 0;//get them ready to taunt
			return;
		}
		if ( self->client && self->client->NPC_class == CLASS_GALAKMECH )
		{
			self->wait = 1;
			TIMER_Set( self, "gloatTime", Q_irand( 5000, 8000 ) );
			self->NPC->blockedSpeechDebounceTime = 0;//get him ready to taunt
			return;
		}
		//FIXME: any way to not say this *right away*?  Wait for victim's death anim/scream to finish?
		if ( self->NPC && self->NPC->group && self->NPC->group->commander && self->NPC->group->commander->NPC && self->NPC->group->commander->NPC->rank > self->NPC->rank && !Q_irand( 0, 2 ) )
		{//sometimes have the group commander speak instead
			self->NPC->group->commander->NPC->greetingDebounceTime = level.time + Q_irand( 2000, 5000 );
		}
		else if ( self->NPC )
		{
			self->NPC->greetingDebounceTime = level.time + Q_irand( 2000, 5000 );
		}
	}
}

void G_AddPowerDuelScore(int team, int score)
{
	int i = 0;
	gentity_t *check;

	while (i < MAX_CLIENTS)
	{
		check = &g_entities[i];
		if (check->inuse && check->client &&
			check->client->pers.connected == CON_CONNECTED && !check->client->iAmALoser &&
			check->client->ps.stats[STAT_HEALTH] > 0 &&
			check->client->sess.sessionTeam != TEAM_SPECTATOR &&
			check->client->sess.duelTeam == team)
		{ //found a living client on the specified team
			check->client->sess.wins += score;
			ClientUserinfoChanged(check->s.number);
		}
		i++;
	}
}

void G_AddPowerDuelLoserScore(int team, int score)
{
	int i = 0;
	gentity_t *check;

	while (i < MAX_CLIENTS)
	{
		check = &g_entities[i];
		if (check->inuse && check->client &&
			check->client->pers.connected == CON_CONNECTED &&
			(check->client->iAmALoser || (check->client->ps.stats[STAT_HEALTH] <= 0 && check->client->sess.sessionTeam != TEAM_SPECTATOR)) &&
			check->client->sess.duelTeam == team)
		{ //found a living client on the specified team
			check->client->sess.losses += score;
			ClientUserinfoChanged(check->s.number);
		}
		i++;
	}
}

static qboolean isAbovePit(gentity_t *ent){
	trace_t result;
	vec3_t under;
	gentity_t *underEnt;

	if (!ent || !ent->client)
		return qfalse;
	
	VectorCopy(ent->client->ps.origin, under);
	under[2] -= 4096;

	trap_Trace(&result, ent->client->ps.origin, NULL, NULL, 
		under, ent->client->ps.clientNum, 
		(CONTENTS_SOLID|CONTENTS_TERRAIN|CONTENTS_TRIGGER));  

	if (result.fraction == 1)
		return qfalse;

	underEnt = &g_entities[result.entityNum];

	if (Q_stricmp(underEnt->classname,"trigger_hurt"))
		return qfalse;

	return qtrue;
}


extern stringID_table_t animTable[MAX_ANIMATIONS+1];

extern void AI_DeleteSelfFromGroup( gentity_t *self );
extern void AI_GroupMemberKilled( gentity_t *self );
extern void Boba_FlyStop( gentity_t *self );
extern qboolean Jedi_WaitingAmbush( gentity_t *self );
void CheckExitRules( void );
extern void Rancor_DropVictim( gentity_t *self );

extern qboolean g_dontFrickinCheck;
extern qboolean g_endPDuel;
extern qboolean g_noPDuelCheck;
extern void SetSiegeClass(gentity_t *ent, char* className);

int G_SiegeClassCount(int team, int classType, qboolean mustBeAlive, int ignoreClientNum)
{
	assert(ignoreClientNum < MAX_CLIENTS);
	if (g_gametype.integer != GT_SIEGE || (team != TEAM_RED && team != TEAM_BLUE))
	{
		//return -1;
		return 0;
	}

	int i, count = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!(ignoreClientNum >= 0 && i == ignoreClientNum) && &g_entities[i] && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED &&
			(g_entities[i].client->sess.sessionTeam == team || !mustBeAlive && g_entities[i].client->sess.sessionTeam == TEAM_SPECTATOR && g_entities[i].client->sess.siegeDesiredTeam == team) &&
			bgSiegeClasses[g_entities[i].client->siegeClass].playerClass == classType)
		{
			if (!mustBeAlive || (g_entities[i].health > 0 && !(g_entities[i].client->tempSpectate > level.time)))
			{
				count++;
			}
		}
	}

	return count;
}

static float GetDistanceFromNearestSiegeitem(gentity_t *ent, int onlyItemsTouchableByThisTeam)
{
	int i, numItemsFound = 0;
	float closestDistance = -1;
	vec3_t difference;

	for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++)
	{
		if (&g_entities[i] && g_entities[i].classname && g_entities[i].classname[0] && !Q_stricmp(g_entities[i].classname, "misc_siege_item") &&
			!(g_entities[i].s.eFlags & EF_NODRAW) && !g_entities[i].genericValue2 && g_entities[i].canPickUp)
		{
			//found a pickupable item that is not hidden or in anyone's possession
			if (onlyItemsTouchableByThisTeam > TEAM_FREE && g_entities[i].genericValue6 == ent->client->sess.sessionTeam)
			{
				//the ent's team can't touch this item
				continue;
			}
			numItemsFound++;
			VectorSubtract(g_entities[i].s.origin, ent->client->ps.origin, difference);
			if (difference && (closestDistance == -1 || VectorLength(difference) < closestDistance))
			{
				closestDistance = VectorLength(difference);
			}
			memset(difference, 0, sizeof(difference));
		}
	}

	if (!numItemsFound || closestDistance == -1)
	{
		return -1;
	}

	return closestDistance;
}

static qboolean CheckSiegeAward(reward_t reward, gentity_t *self, gentity_t *attacker, int mod)
{
	int i;

	switch (reward)
	{
	case REWARD_HUMILIATION:
		if (mod == MOD_CRUSH && attacker->client->ps.m_iVehicleNum)
		{
			//ran someone over in a vehicle
			attacker->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;
			attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_GAUNTLETREWARD;
			return qtrue;
		}
		break;
	case REWARD_DEFEND: //also handles REWARD_DENIED
		if (level.zombies)
		{
			return qfalse;
		}

		if (self->client->holdingObjectiveItem)
		{
			//killed while holding an obj item
			attacker->client->pers.teamState.basedefense++;
			attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
			attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			return qtrue;
		}
		else
		{
			//scan for any nearby obj items
			float distance = GetDistanceFromNearestSiegeitem(self, self->client->sess.sessionTeam);
			if (distance >= 0 && distance <= 128)
			{
				//killed while very close to an item
				attacker->client->pers.teamState.basedefense++;
				attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_DENIEDREWARD;
				return qtrue;
			}
		}

		if (self->client->sess.sessionTeam == TEAM_RED && level.totalObjectivesCompleted == 3 && level.siegeMap == SIEGEMAP_URBAN) {
			// check if killed while on or close to swoop
			int i;
			for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				if (g_entities[i].NPC && g_entities[i].s.NPC_class == CLASS_VEHICLE) {
					vec3_t difference = { 0 };
					VectorSubtract(g_entities[i].r.currentOrigin, self->client->ps.origin, difference);
					float distance = VectorLength(difference);
					if (distance <= 128) {
						attacker->client->pers.teamState.basedefense++;
						attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
						attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
						if (distance > 20)
							self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_DENIEDREWARD;
						return qtrue;
					}
					break;
				}
			}
		}

		break;

	case REWARD_HOLYSHIT:
		if (level.zombies)
		{
			return qfalse;
		}

		if ((mod == MOD_MELEE || mod == MOD_STUN_BATON) && self->m_pVehicle && self->m_pVehicle->m_pPilot && ((gentity_t*)self->m_pVehicle->m_pPilot) &&
			((gentity_t*)self->m_pVehicle->m_pPilot)->client && ((gentity_t*)self->m_pVehicle->m_pPilot)->client->pers.connected == CON_CONNECTED &&
			((gentity_t*)self->m_pVehicle->m_pPilot)->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
		{
			attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
			++attacker->client->pers.teamState.saves;
			((gentity_t*)self->m_pVehicle->m_pPilot)->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
			return qtrue;
		}

		if (level.siegeMap == SIEGEMAP_HOTH)
		{
			if (!level.objectiveJustCompleted && self->m_pVehicle &&
				self->client->ps.origin[0] >= 4210 && self->client->ps.origin[0] <= 4454 &&
				self->client->ps.origin[1] >= -488 && self->client->ps.origin[1] <= 37 &&
				!G_SiegeClassCount(TEAM_BLUE, SPC_SUPPORT, qtrue, -1) && mod != MOD_TARGET_LASER)
			{
				//first objective; walker was killed near door with no living defense techs
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				if (self->m_pVehicle->m_pPilot && ((gentity_t*)self->m_pVehicle->m_pPilot) && ((gentity_t*)self->m_pVehicle->m_pPilot)->client &&
					((gentity_t*)self->m_pVehicle->m_pPilot)->client->pers.connected == CON_CONNECTED && ((gentity_t*)self->m_pVehicle->m_pPilot)->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
				{
					((gentity_t*)self->m_pVehicle->m_pPilot)->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
				return qtrue;
			}
			else if (level.objectiveJustCompleted == 1 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= -750 && self->client->ps.origin[0] <= -643 &&
				self->client->ps.origin[1] >= -138 && self->client->ps.origin[1] <= 121 &&
				self->client->ps.origin[2] <= -128 && mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//second objective; dude was killed very close to hack button
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
			else if (level.objectiveJustCompleted == 3 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->holdingObjectiveItem > 0 && mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//fourth objective; codes carrier was killed very close to button
				vec3_t obj = { -3111, 1763, -199 };
				vec3_t difference;
				if (difference)
				{
					VectorSubtract(self->client->ps.origin, obj, difference);

					if (VectorLength(difference) <= 110 && self->client->ps.origin[2] >= -205 && self->client->ps.origin[2] <= -110)
					{
						attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
						self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
						++attacker->client->pers.teamState.saves;
						return qtrue;
					}
				}
			}
			else if (level.objectiveJustCompleted == 4 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= -1320 && self->client->ps.origin[0] <= -1040 &&
				self->client->ps.origin[1] >= -160 && self->client->ps.origin[1] <= 110 &&
				self->client->ps.origin[2] >= 43 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//fourth objective; dude was killed higher up in the lift shaft
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
		}
		else if (level.siegeMap == SIEGEMAP_NAR)
		{
			if (self->client->isHacking && self->client->ps.hackingTime > level.time && self->client->ps.hackingTime - level.time <= 500 && self->client->sess.sessionTeam == TEAM_RED && (level.objectiveJustCompleted == 0 || level.objectiveJustCompleted == 1 || level.totalObjectivesCompleted == 4) &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//killed while hacking with <= 500 ms remaining on the hack
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
			else if (self->client->holdingObjectiveItem > 0 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= -2349 && self->client->ps.origin[0] <= -1016 &&
				self->client->ps.origin[1] >= 13072 && self->client->ps.origin[1] <= 14515 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//killed for an instant return
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
		}
		else if (level.siegeMap == SIEGEMAP_CARGO)
		{
			if (self->client->isHacking && self->client->ps.hackingTime > level.time && self->client->ps.hackingTime - level.time <= 500 && self->client->sess.sessionTeam == TEAM_RED &&
				(!level.objectiveJustCompleted || (self->client->ps.origin[0] >= 6145 && self->client->ps.origin[0] <= 6525 &&
					self->client->ps.origin[1] >= 822 && self->client->ps.origin[1] <= 1077) || (self->client->ps.origin[0] >= 5435 && self->client->ps.origin[0] <= 5596 &&
						self->client->ps.origin[1] >= -106 && self->client->ps.origin[1] <= 213 && self->client->ps.origin[2] <= 123) || (self->client->ps.origin[0] >= 4991 &&
							self->client->ps.origin[0] <= 5327 && self->client->ps.origin[1] >= -1226 && self->client->ps.origin[1] <= -1007 && self->client->ps.origin[2] <= 123)) &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//killed while hacking with <= 500 ms remaining on the hack
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
			else if (level.totalObjectivesCompleted == 4 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= 6081 && self->client->ps.origin[0] <= 6506 &&
				self->client->ps.origin[1] >= 1049 && self->client->ps.origin[1] <= 1559 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//killed in doorway of cc
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
			else if (level.totalObjectivesCompleted == 6 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= 1244 && self->client->ps.origin[0] <= 1881 &&
				self->client->ps.origin[1] >= 667 && self->client->ps.origin[1] <= 1693 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER)
			{
				//killed while almost escaping on last obj
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
		}
		else if (level.siegeMap == SIEGEMAP_BESPIN) {
			if (!level.totalObjectivesCompleted && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= 605 && self->client->ps.origin[0] <= 835 &&
				self->client->ps.origin[1] >= -2060 && self->client->ps.origin[1] <= -1935 &&
				self->client->ps.origin[2] >= 140 && self->client->ps.origin[2] <= 400 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER) {
				//killed in doorway of first obj
				qboolean foundLock = qfalse;
				int j;
				for (j = 0; j < MAX_GENTITIES; j++) {
					gentity_t *lock = &g_entities[j];
					if (!Q_stricmp(lock->targetname, "locker") && !Q_stricmp(lock->classname, "func_breakable") && lock->health > 0) {
						foundLock = qtrue;
						break;
					}
				}
				if (!foundLock) {
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
					self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
					++attacker->client->pers.teamState.saves;
					return qtrue;
				}
			}
			else if (level.totalObjectivesCompleted == 1 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= 2865 && self->client->ps.origin[0] <= 2985 &&
				self->client->ps.origin[1] >= -2010 && self->client->ps.origin[1] <= -1804 &&
				self->client->ps.origin[2] >= 100 && self->client->ps.origin[2] <= 250 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER) {
				//killed in doorway of 2nd obj
				int j;
				for (j = 0; j < MAX_GENTITIES; j++) {
					gentity_t *counter = &g_entities[j];
					if (!Q_stricmp(counter->targetname, "Obj2_Relay") && !Q_stricmp(counter->classname, "target_counter")) {
						if (!counter->count) {
							attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
							self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
							++attacker->client->pers.teamState.saves;
							return qtrue;
						}
						break;
					}
				}
			}
			else if (level.totalObjectivesCompleted == 3 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->isHacking && self->client->ps.hackingTime > level.time && self->client->ps.hackingTime - level.time <= 500 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER) {
				//killed while hacking with <= 500 ms remaining on 4th obj hack
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
			else if (level.totalObjectivesCompleted == 4 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= 112 && self->client->ps.origin[0] <= 320 &&
				self->client->ps.origin[1] >= 4150 && self->client->ps.origin[1] <= 4300 &&
				self->client->ps.origin[2] >= 330 && self->client->ps.origin[2] <= 900 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER) {
				//killed in lift of 5th obj
				int j;
				for (j = 0; j < MAX_GENTITIES; j++) {
					gentity_t *lift = &g_entities[j];
					if (!Q_stricmp(lift->targetname, "lift5") && !Q_stricmp(lift->classname, "func_door")) {
						if (lift->moverState == MOVER_1TO2) {
							attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
							self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
							++attacker->client->pers.teamState.saves;
							return qtrue;
						}
						break;
					}
				}
			}
		}
		else if (level.siegeMap == SIEGEMAP_URBAN) {
			if (self->client->isHacking && self->client->ps.hackingTime > level.time && self->client->ps.hackingTime - level.time <= 500 && self->client->sess.sessionTeam == TEAM_RED &&
				(!level.totalObjectivesCompleted || (level.totalObjectivesCompleted == 1 && self->client->ps.origin[1] >= 900 && self->client->holdingObjectiveItem) || (level.totalObjectivesCompleted == 2 && self->client->ps.origin[1] >= 1000))) {
				// killed while hacking
				attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				++attacker->client->pers.teamState.saves;
				return qtrue;
			}
			else if (level.totalObjectivesCompleted == 3 && self->client->sess.sessionTeam == TEAM_RED &&
				self->client->ps.origin[0] >= 4242 && self->client->ps.origin[0] <= 4770 &&
				self->client->ps.origin[1] >= 6600 && self->client->ps.origin[1] <= 6842 &&
				self->client->ps.origin[2] >= -23 && self->client->ps.origin[2] <= 172 &&
				mod != MOD_TIMED_MINE_SPLASH && mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TARGET_LASER) { // killed while on swoop near obj
				int i;
				for (i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
					if (g_entities[i].NPC && g_entities[i].s.NPC_class == CLASS_VEHICLE) {
						vec3_t difference = { 0 };
						VectorSubtract(g_entities[i].r.currentOrigin, self->client->ps.origin, difference);
						float distance = VectorLength(difference);
						if (distance < 20) {
							attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
							self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
							++attacker->client->pers.teamState.saves;
							return qtrue;
						}
						break;
					}
				}
			}
		}
		break;

	case REWARD_ASSIST:
		if (self->client->ps.electrifyTime >= level.time)
		{
			//killed while frozen
			for (i = 0; i < MAX_CLIENTS; i++)
			{
				if (&g_entities[i] && &g_entities[i] != self && &g_entities[i] != attacker && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED &&
					g_entities[i].client->pers.teamState.frozeClient == self->client->ps.clientNum && g_entities[i].client->pers.teamState.frozeTime >= level.time &&
					g_entities[i].client->sess.sessionTeam == attacker->client->sess.sessionTeam && g_entities[i].client->sess.sessionTeam != self->client->sess.sessionTeam)
				{
					//killed while frozen by a third party
					g_entities[i].client->pers.teamState.assists++;
					g_entities[i].client->ps.persistant[PERS_ASSIST_COUNT]++;
					g_entities[i].client->rewardTime = level.time + REWARD_SPRITE_TIME;
					int assistStatIndex = -1;
					switch (level.siegeMap) {
					case SIEGEMAP_HOTH: assistStatIndex = g_entities[i].client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_HOTH_OASSIST : SIEGEMAPSTAT_HOTH_DASSIST;			break;
					case SIEGEMAP_DESERT: assistStatIndex = g_entities[i].client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_DESERT_OASSIST : SIEGEMAPSTAT_DESERT_DASSIST;		break;
					case SIEGEMAP_NAR: assistStatIndex = g_entities[i].client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_NAR_OASSIST : SIEGEMAPSTAT_NAR_DASSIST;				break;
					case SIEGEMAP_CARGO: assistStatIndex = g_entities[i].client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_CARGO2_OASSIST : SIEGEMAPSTAT_CARGO2_DASSIST;		break;
					case SIEGEMAP_BESPIN: assistStatIndex = g_entities[i].client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_BESPIN_OASSIST : SIEGEMAPSTAT_BESPIN_DASSIST;		break;
					case SIEGEMAP_URBAN: assistStatIndex = g_entities[i].client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_URBAN_OASSIST : SIEGEMAPSTAT_URBAN_DASSIST;		break;
					}
					if (assistStatIndex != -1)
						g_entities[i].client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][assistStatIndex]++;
					return qtrue;
				}
			}
		}
		break;

	case REWARD_IMPRESSIVE:
		if (mod == MOD_SABER && self->client->pers.connected == CON_CONNECTED)
		{
			//check for long-distance saberthrow kills
			vec3_t difference;
			VectorSubtract(self->client->ps.origin, attacker->client->ps.origin, difference);
			if (difference && VectorLength(difference) >= 950)
			{
				attacker->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
				return qtrue;
			}
		}
		break;

	default:
		break;
	}
	return qfalse;
}

static qboolean CheckSiegeKillAwards(gentity_t *self, gentity_t *attacker, int mod) {
	// set killer
	self->client->sess.siegeStats.killer = attacker - g_entities;

	if (self == attacker) {
		self->client->sess.siegeStats.selfkills[GetSiegeStatRound()]++;
		return qfalse;
	}

	// deaths
	if (self->client->sess.sessionTeam == TEAM_RED)
		self->client->sess.siegeStats.oDeaths[GetSiegeStatRound()]++;
	else if (self->client->sess.sessionTeam == TEAM_BLUE)
		self->client->sess.siegeStats.dDeaths[GetSiegeStatRound()]++;

	if (self->client->sess.sessionTeam == attacker->client->sess.sessionTeam)
		return qfalse;


	// kills
	if (attacker->client->sess.sessionTeam == TEAM_RED && !self->NPC && !self->m_pVehicle) {
		attacker->client->sess.siegeStats.oKills[GetSiegeStatRound()]++;
		// tech kills
		if (self->client->sess.sessionTeam == TEAM_BLUE && bgSiegeClasses[self->client->siegeClass].playerClass == SPC_SUPPORT) {
			char map[MAX_QPATH] = { 0 };
			trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
			if (map[0] && !Q_stricmpn(map, "mp/siege_hoth", 13))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_HOTH_TECHKILL]++;
			else if (map[0] && !Q_stricmp(map, "siege_narshaddaa"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_NAR_TECHKILL]++;
		}
	}
	else if (attacker->client->sess.sessionTeam == TEAM_BLUE && !self->NPC && !self->m_pVehicle) {
		attacker->client->sess.siegeStats.dKills[GetSiegeStatRound()]++;
		attacker->client->pers.fragsSinceObjStart++;
	}

	qboolean gaveAward = qfalse;

	if (CheckSiegeAward(REWARD_HUMILIATION, self, attacker, mod)) { gaveAward = qtrue; }
	if (CheckSiegeAward(REWARD_DEFEND, self, attacker, mod)) { gaveAward = qtrue; } //also handles REWARD_DENIED
	if (CheckSiegeAward(REWARD_ASSIST, self, attacker, mod)) { gaveAward = qtrue; }
	if (CheckSiegeAward(REWARD_IMPRESSIVE, self, attacker, mod)) { gaveAward = qtrue; }
	if (CheckSiegeAward(REWARD_HOLYSHIT, self, attacker, mod)) {
		// saves
		if (attacker->client->sess.sessionTeam == TEAM_BLUE)
			attacker->client->sess.siegeStats.saves[GetSiegeStatRound()]++;
		gaveAward = qtrue;
	}
	
	return gaveAward;
}

/*
==================
player_die
==================
*/
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	gentity_t	*ent;
	int			anim;
	int			contents;
	int			killer;
	int			i;
	char		*killerName, *obit;
	qboolean	wasJediMaster = qfalse;
	int			sPMType = 0;
	qboolean	gaveAward = qfalse;
	int			customObituary = 0;

	if ( self->client->ps.pm_type == PM_DEAD ) {
		return;
	}

	if ( level.intermissiontime ) {
		return;
	}

	//check player stuff
	g_dontFrickinCheck = qfalse;

	if (g_gametype.integer == GT_POWERDUEL)
	{ //don't want to wait til later in the frame if this is the case
		CheckExitRules();

		if ( level.intermissiontime )
		{
			return;
		}
	}

	self->client->emoted = qfalse;

	// idiot selfkilled at the start of the round for some reason; note this
	if (level.wasRestarted && g_gametype.integer == GT_SIEGE && self - g_entities < MAX_CLIENTS && (!attacker || self == attacker) &&
		self->client && (self->client->sess.sessionTeam == TEAM_RED || self->client->sess.sessionTeam == TEAM_BLUE) &&
		(level.siegeStage == SIEGESTAGE_ROUND1 || level.siegeStage == SIEGESTAGE_ROUND2) &&
		level.siegeRoundStartTime && level.time - level.siegeRoundStartTime <= LIVEPUG_CHECK_TIME) {
		level.selfKilledAtStart[self - g_entities] = qtrue;
	}

	if (self - g_entities >= 0 && self - g_entities < MAX_CLIENTS) {
		level.siegeTopTimes[self - g_entities].hasDied = qtrue;
		level.sentriesUsedThisLife[self - g_entities] = 0;
	}

	if (meansOfDeath == MOD_CRUSH && level.siegeMap == SIEGEMAP_CARGO && attacker &&
		VALIDSTRING(attacker->classname) && !Q_stricmp(attacker->classname, "func_rotating")) {
		if (self->client->ps.otherKillerTime >= level.time && g_entities[self->client->ps.otherKiller].inuse && g_entities[self->client->ps.otherKiller].client)
			customObituary = CUSTOMOBITUARY_CARGO_CHOPPED;
		else
			customObituary = CUSTOMOBITUARY_CARGO_CHOPPED_SELF;
	}

	if (self->s.eType == ET_NPC &&
		self->s.NPC_class == CLASS_VEHICLE &&
		self->m_pVehicle &&
		!self->m_pVehicle->m_pVehicleInfo->explosionDelay &&
		(self->m_pVehicle->m_pPilot || self->m_pVehicle->m_iNumPassengers > 0 || self->m_pVehicle->m_pDroidUnit))
	{ //kill everyone on board in the name of the attacker... if the vehicle has no death delay
		gentity_t *murderer = NULL;
		gentity_t *killEnt;
		int i = 0;

		if (self->client->ps.otherKillerTime >= level.time)
		{ //use the last attacker
			murderer = &g_entities[self->client->ps.otherKiller];
			if (!murderer->inuse || !murderer->client)
			{
				murderer = NULL;
			}
			else
			{
				if (murderer->s.number >= MAX_CLIENTS &&
					murderer->s.eType == ET_NPC &&
					murderer->s.NPC_class == CLASS_VEHICLE &&
					murderer->m_pVehicle &&
					murderer->m_pVehicle->m_pPilot)
				{
					gentity_t *murderPilot = &g_entities[murderer->m_pVehicle->m_pPilot->s.number];
					if (murderPilot->inuse && murderPilot->client)
					{ //give the pilot of the offending vehicle credit for the kill
						murderer = murderPilot;
					}
				}
			}
		}
		else if (attacker && attacker->inuse && attacker->client)
		{
			if (attacker->s.number >= MAX_CLIENTS &&
				attacker->s.eType == ET_NPC &&
				attacker->s.NPC_class == CLASS_VEHICLE &&
				attacker->m_pVehicle &&
				attacker->m_pVehicle->m_pPilot)
			{ //set vehicles pilot's killer as murderer
				murderer = &g_entities[attacker->m_pVehicle->m_pPilot->s.number];
				if (murderer->inuse && murderer->client &&murderer->client->ps.otherKillerTime >= level.time)
				{
					murderer = &g_entities[murderer->client->ps.otherKiller];
					if (!murderer->inuse || !murderer->client)
					{
						murderer = NULL;
					}
				}
				else
				{
					murderer = NULL;
				}
			}
			else
			{
				murderer = &g_entities[attacker->s.number];
			}
		}
		else if (self->m_pVehicle->m_pPilot)
		{
			murderer = (gentity_t *)self->m_pVehicle->m_pPilot;
			if (!murderer->inuse || !murderer->client)
			{
				murderer = NULL;
			}
		}

		//no valid murderer.. just use self I guess
		if (!murderer)
		{
			murderer = self;
		}

		if ( self->m_pVehicle->m_pVehicleInfo->hideRider )
		{//pilot is *inside* me, so kill him, too
			killEnt = (gentity_t *)self->m_pVehicle->m_pPilot;
			if (killEnt && killEnt->inuse && killEnt->client)
			{
				G_Damage(killEnt, murderer, murderer, NULL, killEnt->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_BLASTER);
			}
			if ( self->m_pVehicle->m_pVehicleInfo )
			{//FIXME: this wile got stuck in an endless loop, that's BAD!!  This method SUCKS (not initting "i", not incrementing it or using it directly, all sorts of badness), so I'm rewriting it
				//while (i < self->m_pVehicle->m_iNumPassengers)
				int numPass = self->m_pVehicle->m_iNumPassengers;
				for ( i = 0; i < numPass && self->m_pVehicle->m_iNumPassengers; i++ )
				{//go through and eject the last passenger
					killEnt = (gentity_t *)self->m_pVehicle->m_ppPassengers[self->m_pVehicle->m_iNumPassengers-1];
					if ( killEnt )
					{
						self->m_pVehicle->m_pVehicleInfo->Eject(self->m_pVehicle, (bgEntity_t *)killEnt, qtrue);
						if ( killEnt->inuse && killEnt->client )
						{
							G_Damage(killEnt, murderer, murderer, NULL, killEnt->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_BLASTER);
						}
					}
				}
			}
		}
		killEnt = (gentity_t *)self->m_pVehicle->m_pDroidUnit;
		if (killEnt && killEnt->inuse && killEnt->client)
		{
			killEnt->flags &= ~FL_UNDYING;
			G_Damage(killEnt, murderer, murderer, NULL, killEnt->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_BLASTER);
		}
	}

	self->client->ps.emplacedIndex = 0;

	G_BreakArm(self, 0); //unbreak anything we have broken
	self->client->ps.saberEntityNum = self->client->saberStoredIndex; //in case we died while our saber was knocked away.

	self->client->bodyGrabIndex = ENTITYNUM_NONE;
	self->client->bodyGrabTime = 0;

	// check siege stuff
	if (g_gametype.integer == GT_SIEGE && self && self->client && attacker) {
		if ((self->client->sess.sessionTeam == TEAM_RED || self->client->sess.sessionTeam == TEAM_BLUE) && ((attacker - g_entities == ENTITYNUM_WORLD && meansOfDeath == MOD_SUICIDE/*vehicle explosion*/) || (attacker->s.eType == ET_NPC && attacker->s.NPC_class == CLASS_VEHICLE) || (attacker->client && self->client->sess.sessionTeam == OtherTeam(attacker->client->sess.sessionTeam))) && self->client->siegeClass != -1 && bgSiegeClasses[self->client->siegeClass].invenItems & (1 << HI_SHIELD))
			level.canShield[self->client->sess.sessionTeam] = CANSHIELD_YES;
		if (attacker->client)
			gaveAward = CheckSiegeKillAwards(self, attacker, meansOfDeath);
	}

	if (self->client->holdingObjectiveItem > 0)
	{ //carrying a siege objective item - make sure it updates and removes itself from us now in case this is an instant death-respawn situation
		gentity_t *objectiveItem = &g_entities[self->client->holdingObjectiveItem];

		if (objectiveItem->inuse && objectiveItem->think)
		{
            objectiveItem->think(objectiveItem);
		}
	}

	if ( (self->client->inSpaceIndex && self->client->inSpaceIndex != ENTITYNUM_NONE) ||
		 (self->client->ps.eFlags2 & EF2_SHIP_DEATH) )
	{
		self->client->noCorpse = qtrue;
	}

	if ( self->client->NPC_class != CLASS_VEHICLE
		&& self->client->ps.m_iVehicleNum )
	{ //I'm riding a vehicle
		//tell it I'm getting off
		gentity_t *veh = &g_entities[self->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{
			veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)self, qtrue);

			if (veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
			{ //go into "die in ship" mode with flag
				self->client->ps.eFlags2 |= EF2_SHIP_DEATH;

				//put me over where my vehicle exploded
				G_SetOrigin(self, veh->client->ps.origin);
				VectorCopy(veh->client->ps.origin, self->client->ps.origin);
			}
		}
		//droids throw heads if they haven't yet
		switch(self->client->NPC_class)
		{
		case CLASS_R2D2:
			if ( !trap_G2API_GetSurfaceRenderStatus( self->ghoul2, 0, "head" ) )
			{
				vec3_t	up;
				AngleVectors( self->r.currentAngles, NULL, NULL, up );
				G_PlayEffectID( G_EffectIndex("chunks/r2d2head_veh"), self->r.currentOrigin, up );
			}
			break;

		case CLASS_R5D2:
			if ( !trap_G2API_GetSurfaceRenderStatus( self->ghoul2, 0, "head" ) )
			{
				vec3_t	up;
				AngleVectors( self->r.currentAngles, NULL, NULL, up );
				G_PlayEffectID( G_EffectIndex("chunks/r5d2head_veh"), self->r.currentOrigin, up );
			}
			break;
		default:
			break;
		}
	}

	if ( self->NPC )
	{
		if ( self->client && Jedi_WaitingAmbush( self ) )
		{//ambushing trooper
			self->client->noclip = qfalse;
		}
		NPC_FreeCombatPoint( self->NPC->combatPoint, qfalse );
		if ( self->NPC->group )
		{
			AI_GroupMemberKilled( self );
			AI_DeleteSelfFromGroup( self );
		}

		if ( self->NPC->tempGoal )
		{
			G_FreeEntity( self->NPC->tempGoal );
			self->NPC->tempGoal = NULL;
		}
		if (0)
		{
			Boba_FlyStop( self );
		}
		if ( self->s.NPC_class == CLASS_RANCOR )
		{
			Rancor_DropVictim( self );
		}
	}
	if ( attacker && attacker->NPC && attacker->NPC->group && attacker->NPC->group->enemy == self )
	{
		attacker->NPC->group->enemy = NULL;
	}

	//Cheap method until/if I decide to put fancier stuff in (e.g. sabers falling out of hand and slowly
	//holstering on death like sp)
	if (self->client->ps.weapon == WP_SABER &&
		!self->client->ps.saberHolstered &&
		self->client->ps.saberEntityNum)
	{
		if (!self->client->ps.saberInFlight &&
			self->client->saber[0].soundOff)
		{
			G_Sound(self, CHAN_AUTO, self->client->saber[0].soundOff);
		}
		if (self->client->saber[1].soundOff &&
			self->client->saber[1].model[0])
		{
			G_Sound(self, CHAN_AUTO, self->client->saber[1].soundOff);
		}
	}

	//Use any target we had
	if (level.siegeMap == SIEGEMAP_URBAN && attacker && attacker - g_entities >= 0 && attacker - g_entities < MAX_CLIENTS && VALIDSTRING(self->NPC_type) && tolower(*self->NPC_type) == 'w')
		G_UseTargets(self, attacker);
	else if (level.siegeMap == SIEGEMAP_ANSION && attacker && attacker - g_entities >= 0 && attacker - g_entities < MAX_CLIENTS && VALIDSTRING(self->NPC_type) && (!Q_stricmp(self->NPC_type, "Alpha") || !Q_stricmp(self->NPC_type, "Onasi")))
		G_UseTargets(self, attacker);
	else
		G_UseTargets( self, self ); 

	if (g_slowmoDuelEnd.integer && (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) && attacker && attacker->inuse && attacker->client)
	{
		if (!gDoSlowMoDuel)
		{
			gDoSlowMoDuel = qtrue;
			gSlowMoDuelTime = level.time;
		}
	}

	//Make sure the jetpack is turned off.
	Jetpack_Off(self);

	self->client->ps.heldByClient = 0;
	self->client->beingThrown = 0;
	self->client->doingThrow = 0;
	BG_ClearRocketLock( &self->client->ps );
	self->client->isHacking = 0;
	self->client->ps.hackingTime = 0;

	if (inflictor && inflictor->activator && !inflictor->client && !attacker->client &&
		inflictor->activator->client && inflictor->activator->inuse &&
		inflictor->s.weapon == WP_TURRET)
	{
		attacker = inflictor->activator;
	}

	if (self->client && self->client->ps.isJediMaster)
	{
		wasJediMaster = qtrue;
	}

	//if he was charging or anything else, kill the sound
	G_MuteSound(self->s.number, CHAN_WEAPON);

	//Raz: Siege exploit where you could place detpack on your own objectives, change team, and instantly win.
    if ( g_gametype.integer == GT_SIEGE && meansOfDeath == MOD_TEAM_CHANGE 
        && (self->client->sess.siegeDesiredTeam != self->client->sess.sessionTeam) )
		RemoveDetpacks( self );
	else
		BlowDetpacks(self, qtrue); //blow detpacks if they're planted

	self->client->ps.fd.forceDeactivateAll = 1;

	if (g_fixGripKills.integer && self == attacker && meansOfDeath == MOD_SUICIDE && !(g_gametype.integer == GT_SIEGE && self->client->siegeClass != -1 && bgSiegeClasses[self->client->siegeClass].invenItems & (1 << HI_SHIELD)) && self->client->ps.fd.forceGripBeingGripped && self->client->ps.fd.forceGripBeingGripped >= level.time && &g_entities[self->client->ps.otherKiller] && &g_entities[self->client->ps.otherKiller].client)
	{
		meansOfDeath = MOD_FORCE_DARK;
		attacker = &g_entities[self->client->ps.otherKiller];
	}

	if (g_fixPitKills.integer && ((self == attacker || !attacker->client) && !(g_gametype.integer == GT_SIEGE && self->client->siegeClass != -1 && bgSiegeClasses[self->client->siegeClass].invenItems & (1 << HI_SHIELD)) &&
		(meansOfDeath == MOD_CRUSH || meansOfDeath == MOD_FALLING || meansOfDeath == MOD_TRIGGER_HURT || meansOfDeath == MOD_UNKNOWN ||
		(meansOfDeath == MOD_SUICIDE && 
		//(self->client->ps.fallingToDeath || self->client->ps.origin[2] < self->client->ps.fd.forceJumpZStart || self->client->ps.velocity[2] < -600))) &&
		(self->client->ps.fallingToDeath || ((self->client->ps.velocity[2] < -400/*-600*/ || self->client->ps.origin[2] < self->client->ps.fd.forceJumpZStart) && isAbovePit(self)) ))) &&
		self->client->ps.otherKillerTime > level.time))
	{
		attacker = &g_entities[self->client->ps.otherKiller];
		meansOfDeath = MOD_FALLING;
		if (level.siegeMap == SIEGEMAP_URBAN) {
			if (self->client && self->client->ps.origin[1] >= 9800)
				customObituary = CUSTOMOBITUARY_URBAN_BURNED;
			else
				customObituary = CUSTOMOBITUARY_URBAN_DUMPSTERED;
		}
		if (g_fixFallingSounds.integer == 1)
		{
			G_EntitySound(self, CHAN_VOICE, G_SoundIndex("*falling1.wav"));
		}
	}
	else {
		// duo: fix for "was killed by" ==> "was thrown to their doom by" as well as "died." ==> "fell to their death"
		if (meansOfDeath == MOD_TRIGGER_HURT && (level.siegeMap == SIEGEMAP_HOTH || level.siegeMap == SIEGEMAP_NAR || level.siegeMap == SIEGEMAP_BESPIN ||
			(level.siegeMap == SIEGEMAP_ANSION && self->client && self->client->ps.origin[2] < 24))) {
			meansOfDeath = MOD_FALLING;
			if (attacker && VALIDSTRING(attacker->classname) && !Q_stricmp(attacker->classname, "trigger_hurt"))
				attacker = self;
		}
	}

	qboolean isAirFrag = qfalse;
	if (self && self->client && self - g_entities < MAX_CLIENTS &&
		attacker && attacker->client && attacker - g_entities < MAX_CLIENTS &&
		attacker->client->lastAiredOtherClientTime[self - g_entities] && level.time - attacker->client->lastAiredOtherClientTime[self - g_entities] <= 50 &&
		attacker->client->lastAiredOtherClientMeansOfDeath[self - g_entities] == meansOfDeath) {

		switch (meansOfDeath) {
		case MOD_BOWCASTER:
		case MOD_REPEATER_ALT:
		case MOD_FLECHETTE_ALT_SPLASH:
		case MOD_ROCKET:
		case MOD_ROCKET_HOMING:
		case MOD_THERMAL:
		case MOD_CONC:
		case MOD_BRYAR_PISTOL_ALT:
		case MOD_SABER:
			isAirFrag = qtrue;
			break;
		}
	}

	if (meansOfDeath == MOD_TRIGGER_HURT && level.siegeMap == SIEGEMAP_URBAN) {
		if (!attacker || attacker == self || (VALIDSTRING(attacker->classname) && !Q_stricmp(attacker->classname, "trigger_hurt"))) {
			if (self->client && self->client->ps.origin[1] >= 9800)
				customObituary = CUSTOMOBITUARY_URBAN_BURNED_SELF;
			else
				customObituary = CUSTOMOBITUARY_URBAN_DUMPSTERED_SELF;
		}
		else {
			if (self->client && self->client->ps.origin[1] >= 9800)
				customObituary = CUSTOMOBITUARY_URBAN_BURNED;
			else
				customObituary = CUSTOMOBITUARY_URBAN_DUMPSTERED;
		}
	}
	else if (meansOfDeath == MOD_TRIGGER_HURT && level.siegeMap == SIEGEMAP_ANSION && self->client && self->client->ps.origin[2] >= 24) {
		if (!attacker || attacker == self || (VALIDSTRING(attacker->classname) && !Q_stricmp(attacker->classname, "trigger_hurt")))
			customObituary = CUSTOMOBITUARY_ANSION_POISONED_SELF;
		else
			customObituary = CUSTOMOBITUARY_ANSION_POISONED;
	}
	else if (meansOfDeath == MOD_SPECIAL_SENTRYBOMB) {
		meansOfDeath = MOD_UNKNOWN;
		if (!attacker || attacker == self)
			customObituary = CUSTOMOBITUARY_GENERIC_SENTRYBOMBED_SELF;
		else
			customObituary = CUSTOMOBITUARY_GENERIC_SENTRYBOMBED;
	}

	if (level.zombies && meansOfDeath != MOD_SUICIDE && self && self->client && self->client->sess.sessionTeam == TEAM_BLUE)
	{	
		siegeClass_t *siegeClass = BG_SiegeGetClass(TEAM_RED, SPC_JEDI + 3);

		if (siegeClass)
		{
			self->client->switchClassTime = 0;
			SetSiegeClass(self, siegeClass->name);
		}
	}

	// check for an almost capture
	CheckAlmostCapture( self, attacker );

	self->client->ps.pm_type = PM_DEAD;
	self->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;

	if ( attacker ) {
		killer = attacker->s.number;
		if ( attacker->client ) {
			killerName = attacker->client->pers.netname;
		} else {
			killerName = "<non-client>";
		}
	} else {
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ( killer < 0 || killer >= MAX_CLIENTS ) {
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ( meansOfDeath < 0 || meansOfDeath >= sizeof( modNames ) / sizeof( modNames[0] ) ) {
		obit = "<bad obituary>";
	} else {
		obit = modNames[ meansOfDeath ];
	}

	G_LogPrintf("Kill: %i %i %i: %s killed %s by %s\n", 
		killer, self->s.number, meansOfDeath, killerName, 
		self->client->pers.netname, obit );

	if ( g_austrian.integer 
		&& (g_gametype.integer == GT_DUEL) 
		&& level.numPlayingClients >= 2 )
	{
		int spawnTime = (level.clients[level.sortedClients[0]].respawnTime > level.clients[level.sortedClients[1]].respawnTime) ? level.clients[level.sortedClients[0]].respawnTime : level.clients[level.sortedClients[1]].respawnTime;
		G_LogPrintf("Duel Kill Details:\n");
		G_LogPrintf("Kill Time: %d\n", level.time-spawnTime );
		G_LogPrintf("victim: %s, hits on enemy %d\n", self->client->pers.netname, self->client->ps.persistant[PERS_HITS] );
		if ( attacker && attacker->client )
		{
			G_LogPrintf("killer: %s, hits on enemy %d, health: %d\n", attacker->client->pers.netname, attacker->client->ps.persistant[PERS_HITS], attacker->health );
			//also - if MOD_SABER, list the animation and saber style
			if ( meansOfDeath == MOD_SABER )
			{
				G_LogPrintf("killer saber style: %d, killer saber anim %s\n", attacker->client->ps.fd.saberAnimLevel, animTable[(attacker->client->ps.torsoAnim)].name );
			}
		}
	}

	G_LogWeaponKill(killer, meansOfDeath);
	G_LogWeaponDeath(self->s.number, self->s.weapon);
	if (attacker && attacker->client && attacker->inuse)
	{
		G_LogWeaponFrag(killer, self->s.number);
	}

	// broadcast the death event to everyone
	if (self->s.eType != ET_NPC && !g_noPDuelCheck && meansOfDeath != MOD_TEAM_CHANGE)
	{
		ent = G_TempEntity( self->r.currentOrigin, EV_OBITUARY );
		ent->s.eventParm = meansOfDeath;
		ent->s.otherEntityNum = self->s.number;
		ent->s.otherEntityNum2 = killer;
		ent->r.svFlags = SVF_BROADCAST;	// send to everyone
		ent->s.isJediMaster = wasJediMaster;
		assert(customObituary >= 0 && customObituary < MAX_CUSTOMOBITUARIES);
		ent->s.emplacedOwner = customObituary;
		if (isAirFrag && g_creditAirKills.integer)
			ent->s.userInt2 = 1;
	}

	self->enemy = attacker;

	self->client->ps.persistant[PERS_KILLED]++;

	//*CHANGE 31* longest flag holding time keeping track
	if ((self->client->ps.powerups[PW_BLUEFLAG] || self->client->ps.powerups[PW_REDFLAG])){
		const int thisFlaghold = G_GetAccurateTimerOnTrigger( &self->client->pers.teamState.flagsince, self, NULL );

		self->client->pers.teamState.flaghold += thisFlaghold;

		if ( thisFlaghold > self->client->pers.teamState.longestFlaghold )
			self->client->pers.teamState.longestFlaghold = thisFlaghold;

		if ( self->client->ps.powerups[PW_REDFLAG] ) {
			// carried the red flag, so blue team
			level.blueTeamRunFlaghold += thisFlaghold;
		} else if ( self->client->ps.powerups[PW_BLUEFLAG] ) {
			// carried the blue flag, so red team
			level.redTeamRunFlaghold += thisFlaghold;
		}
	}

	if (self == attacker)
	{
		self->client->ps.fd.suicides++;
	}

	if (attacker && attacker->client) {
		attacker->client->lastkilled_client = self->s.number;

		G_CheckVictoryScript(attacker);

		if ( attacker == self || OnSameTeam (self, attacker ) ) {
			if (g_gametype.integer == GT_DUEL)
			{ //in duel, if you kill yourself, the person you are dueling against gets a kill for it
				int otherClNum = -1;
				if (level.sortedClients[0] == self->s.number)
				{
					otherClNum = level.sortedClients[1];
				}
				else if (level.sortedClients[1] == self->s.number)
				{
					otherClNum = level.sortedClients[0];
				}

				if (otherClNum >= 0 && otherClNum < MAX_CLIENTS &&
					g_entities[otherClNum].inuse && g_entities[otherClNum].client &&
					otherClNum != attacker->s.number)
				{
					AddScore( &g_entities[otherClNum], self->r.currentOrigin, 1 );
				}
				else
				{
					AddScore( attacker, self->r.currentOrigin, -1 );
				}
			}
			else
			{
				if (g_gametype.integer == GT_SIEGE) {
					if (level.siegeMap == SIEGEMAP_DUEL && meansOfDeath != MOD_SUICIDE && meansOfDeath != MOD_TEAM_CHANGE) {
						AddScore(self, self->r.currentOrigin, -1);
					}
				}
				else {
					AddScore(self, self->r.currentOrigin, -1);
				}
			}

			if (g_gametype.integer == GT_JEDIMASTER)
			{
				if (self->client && self->client->ps.isJediMaster)
				{ //killed ourself so return the saber to the original position
				  //(to avoid people jumping off ledges and making the saber
				  //unreachable for 60 seconds)
					ThrowSaberToAttacker(self, NULL);
					self->client->ps.isJediMaster = qfalse;
				}
			}
		} else {
			if (g_gametype.integer == GT_JEDIMASTER)
			{
				if ((attacker->client && attacker->client->ps.isJediMaster) ||
					(self->client && self->client->ps.isJediMaster))
				{
					AddScore( attacker, self->r.currentOrigin, 1 );
					
					if (self->client && self->client->ps.isJediMaster)
					{
						ThrowSaberToAttacker(self, attacker);
						self->client->ps.isJediMaster = qfalse;
					}
				}
				else
				{
					gentity_t *jmEnt = G_GetJediMaster();

					if (jmEnt && jmEnt->client)
					{
						AddScore( jmEnt, self->r.currentOrigin, 1 );
					}
				}
			}
			else
			{
				if (self->m_pVehicle && self->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER)
				{
					AddScore(attacker, self->r.currentOrigin, g_swoopKillPoints.integer);
				}
				else
				{
					AddScore(attacker, self->r.currentOrigin, 1);
				}
			}

			if(!gaveAward && (meansOfDeath == MOD_STUN_BATON || meansOfDeath == MOD_MELEE) && !(self->m_pVehicle && !self->m_pVehicle->m_pPilot) && !(self->NPC && !self->m_pVehicle) ) { //duo: added melee
				
				// play humiliation on player
				attacker->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;

				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

				// also play humiliation on target
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_GAUNTLETREWARD;
			}
			// check for two kills in a short amount of time
			// if this is close enough to the last kill, give a reward sound
			if ( level.time - attacker->client->lastKillTime < CARNAGE_REWARD_TIME ) {
				// play excellent on player
				attacker->client->ps.persistant[PERS_EXCELLENT_COUNT]++;

				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}
			attacker->client->lastKillTime = level.time;

		}
	} else {
		if (self->client && self->client->ps.isJediMaster)
		{ //killed ourself so return the saber to the original position
		  //(to avoid people jumping off ledges and making the saber
		  //unreachable for 60 seconds)
			ThrowSaberToAttacker(self, NULL);
			self->client->ps.isJediMaster = qfalse;
		}

		if (g_gametype.integer == GT_DUEL)
		{ //in duel, if you kill yourself, the person you are dueling against gets a kill for it
			int otherClNum = -1;
			if (level.sortedClients[0] == self->s.number)
			{
				otherClNum = level.sortedClients[1];
			}
			else if (level.sortedClients[1] == self->s.number)
			{
				otherClNum = level.sortedClients[0];
			}

			if (otherClNum >= 0 && otherClNum < MAX_CLIENTS &&
				g_entities[otherClNum].inuse && g_entities[otherClNum].client &&
				otherClNum != self->s.number)
			{
				AddScore( &g_entities[otherClNum], self->r.currentOrigin, 1 );
			}
			else
			{
				AddScore( self, self->r.currentOrigin, -1 );
			}
		}
		else
		{
			if (g_gametype.integer != GT_SIEGE)
				AddScore( self, self->r.currentOrigin, -1 );
		}
	}

	// Add team bonuses
	Team_FragBonuses(self, inflictor, attacker);

	// if I committed suicide, the flag does not fall, it returns.
	if (meansOfDeath == MOD_SUICIDE) {
		if ( self->client->ps.powerups[PW_NEUTRALFLAG] ) {		// only happens in One Flag CTF
			Team_ReturnFlag( TEAM_FREE );
			self->client->ps.powerups[PW_NEUTRALFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_REDFLAG] ) {		// only happens in standard CTF
			Team_ReturnFlag( TEAM_RED );
			self->client->ps.powerups[PW_REDFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_BLUEFLAG] ) {	// only happens in standard CTF
			Team_ReturnFlag( TEAM_BLUE );
			self->client->ps.powerups[PW_BLUEFLAG] = 0;
		}
	}

	// if client is in a nodrop area, don't drop anything (but return CTF flags!)
	contents = trap_PointContents( self->r.currentOrigin, -1 );
	if ( !( contents & CONTENTS_NODROP ) && !self->client->ps.fallingToDeath) {
		if (self->s.eType != ET_NPC)
		{
			TossClientItems( self );
		}
	}
	else {
		if ( self->client->ps.powerups[PW_NEUTRALFLAG] ) {		// only happens in One Flag CTF
			Team_ReturnFlag( TEAM_FREE );
		}
		else if ( self->client->ps.powerups[PW_REDFLAG] ) {		// only happens in standard CTF
			Team_ReturnFlag( TEAM_RED );
		}
		else if ( self->client->ps.powerups[PW_BLUEFLAG] ) {	// only happens in standard CTF
			Team_ReturnFlag( TEAM_BLUE );
		}
	}

	if ( MOD_TEAM_CHANGE == meansOfDeath )
	{
		// Give them back a point since they didn't really die.
		if (g_gametype.integer != GT_SIEGE)
			AddScore( self, self->r.currentOrigin, 1 );
	}
	else
	{
		Cmd_Score_f( self );		// show scores
	}

	// send updated scores to any clients that are following this one,
	// or they would get stale scoreboards
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t	*client;

		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		if ( client->sess.spectatorClient == self->s.number ) {
			Cmd_Score_f( g_entities + i );
		}
	}

	self->takedamage = qtrue;	// can still be gibbed

	self->s.weapon = WP_NONE;
	self->s.powerups = 0;
	if (self->s.eType != ET_NPC)
	{ //handled differently for NPCs
		self->r.contents = CONTENTS_CORPSE;
	}
	self->client->ps.zoomMode = 0;	// Turn off zooming when we die

	self->s.loopSound = 0;
	self->s.loopIsSoundset = qfalse;

	if (self->s.eType != ET_NPC)
	{ //handled differently for NPCs
		self->r.maxs[2] = -8;
	}

	// don't allow respawn until the death anim is done
	// g_forcerespawn may force spawning at some later time
	self->client->respawnTime = level.time + 1700;

	// remove powerups
	memset( self->client->ps.powerups, 0, sizeof(self->client->ps.powerups) );

	// NOTENOTE No gib deaths right now, this is star wars.
	{
		// normal death
		
		static int i;

		anim = G_PickDeathAnim(self, self->pos1, damage, meansOfDeath, HL_NONE);

		if (anim >= 1)
		{ //Some droids don't have death anims
			// for the no-blood option, we need to prevent the health
			// from going to gib level
			if ( self->health <= GIB_HEALTH ) {
				self->health = GIB_HEALTH+1;
			}

			// lag compensation for selfkill
			if (g_gametype.integer == GT_SIEGE && meansOfDeath == MOD_SUICIDE && attacker && self && attacker->client && self->client
				&& attacker->s.number < MAX_CLIENTS && self->s.number < MAX_CLIENTS && attacker == self &&
				g_siegeRespawn.integer && level.siegeRespawnCheck && level.siegeRespawnCheck > level.time &&
				self->client->ps.ping > 0 && self->client->ps.ping <= 350) {
				self->client->respawnTime = level.time + 1000 - self->client->ps.ping;
			}
			else {
				self->client->respawnTime = level.time + 1000;
			}

			sPMType = self->client->ps.pm_type;
			self->client->ps.pm_type = PM_NORMAL; //don't want pm type interfering with our setanim calls.

			if (self->inuse)
			{ //not disconnecting
				G_SetAnim(self, NULL, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_RESTART, 0);
			}

			self->client->ps.pm_type = sPMType;

			if (meansOfDeath == MOD_SABER || (meansOfDeath == MOD_MELEE && G_HeavyMelee( attacker )) )//saber or heavy melee (claws)
			{ //update the anim on the actual skeleton (so bolt point will reflect the correct position) and then check for dismem
				G_UpdateClientAnims(self, 1.0f);
			}
			if (meansOfDeath != MOD_SUICIDE){
				G_CheckForDismemberment(self, attacker, self->pos1, damage, anim, qfalse);
			}
		}
		else if (self->NPC && self->client && self->client->NPC_class != CLASS_MARK1 &&
			self->client->NPC_class != CLASS_VEHICLE)
		{ //in this case if we're an NPC it's my guess that we want to get removed straight away.
			self->think = G_FreeEntity;
			self->nextthink = level.time;
		}

		if (wasJediMaster)
		{
			G_AddEvent( self, EV_DEATH1 + i, 1 );
		}
		else
		{
			G_AddEvent( self, EV_DEATH1 + i, 0 );
		}

		if (self != attacker)
		{ //don't make NPCs want to murder you on respawn for killing yourself!
			G_DeathAlert( self, attacker );
		}

		// the body can still be gibbed
		if (!self->NPC)
		{ //don't remove NPCs like this!
			self->die = body_die;
		}

		//It won't gib, it will disintegrate (because this is Star Wars).
		self->takedamage = qtrue;

		// globally cycle through the different death animations
		i = ( i + 1 ) % 3;
	}

	if ( self->NPC )
	{//If an NPC, make sure we start running our scripts again- this gets set to infinite while we fall to our deaths
		self->NPC->nextBStateThink = level.time;
	}

	if ( G_ActivateBehavior( self, BSET_DEATH ) )
	{
	}
	
	if ( self->NPC && (self->NPC->scriptFlags&SCF_FFDEATH) )
	{
		if ( G_ActivateBehavior( self, BSET_FFDEATH ) )  
		{//FIXME: should running this preclude running the normal deathscript?
		}
		G_UseTargets2( self, self, self->target4 );
	}
	
	//rwwFIXMEFIXME: Do this too?

	// Free up any timers we may have on us.
	TIMER_Clear2( self );

	trap_LinkEntity (self);

	if ( self->NPC )
	{
		self->NPC->timeOfDeath = level.time;//this will change - used for debouncing post-death events
	}

	// Start any necessary death fx for this entity
	DeathFX( self );


	if (g_gametype.integer == GT_POWERDUEL && !g_noPDuelCheck)
	{ //powerduel checks
		if (self->client->sess.duelTeam == DUELTEAM_LONE)
		{ //automatically means a win as there is only one
			G_AddPowerDuelScore(DUELTEAM_DOUBLE, 1);
			G_AddPowerDuelLoserScore(DUELTEAM_LONE, 1);
			g_endPDuel = qtrue;
		}
		else if (self->client->sess.duelTeam == DUELTEAM_DOUBLE)
		{
			int i = 0;
			gentity_t *check;
			qboolean heLives = qfalse;

			while (i < MAX_CLIENTS)
			{
				check = &g_entities[i];
				if (check->inuse && check->client && check->s.number != self->s.number &&
					check->client->pers.connected == CON_CONNECTED && !check->client->iAmALoser &&
					check->client->ps.stats[STAT_HEALTH] > 0 &&
					check->client->sess.sessionTeam != TEAM_SPECTATOR &&
					check->client->sess.duelTeam == DUELTEAM_DOUBLE)
				{ //still an active living paired duelist so it's not over yet.
					heLives = qtrue;
					break;
				}
				i++;
			}

			if (!heLives)
			{ //they're all dead, give the lone duelist the win.
				G_AddPowerDuelScore(DUELTEAM_LONE, 1);
				G_AddPowerDuelLoserScore(DUELTEAM_DOUBLE, 1);
				g_endPDuel = qtrue;
			}
		}
	}

	// grass's target_death thing
	if (g_gametype.integer >= GT_TEAM && self && self->client &&
		(self->client->sess.sessionTeam == TEAM_RED || self->client->sess.sessionTeam == TEAM_BLUE) &&
		attacker && attacker != self && attacker->client && attacker->client->sess.sessionTeam != self->client->sess.sessionTeam) {
		char *findClassname = self->client->sess.sessionTeam == TEAM_RED ? "target_deathteam1" : "target_deathteam2";
		gentity_t *grassThing = G_Find(NULL, FOFS(classname), findClassname);
		while (grassThing) {
			if (grassThing->use && !(grassThing->flags & FL_INACTIVE)) {
				CheckSiegeHelpFromUse(grassThing->targetname);
				grassThing->use(grassThing, ent, ent);
			}
			grassThing = G_Find(grassThing, FOFS(classname), findClassname);
		}
	}
}


/*
================
CheckArmor
================
*/
int CheckArmor (gentity_t *ent, int damage, int dflags)
{
	gclient_t	*client;
	int			save;
	int			count;

	if (!damage)
		return 0;

	client = ent->client;

	if (!client)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	if ( client->NPC_class == CLASS_VEHICLE
		&& ent->m_pVehicle
		&& ent->client->ps.electrifyTime > level.time )
	{//ion-cannon has disabled this ship's shields, take damage on hull!
		return 0;
	}
	// armor
	count = client->ps.stats[STAT_ARMOR];

	if (dflags & DAMAGE_HALF_ABSORB)
	{	// Half the damage gets absorbed by the shields, rather than 100%
		save = ceil( damage * ARMOR_PROTECTION );
	}
	else
	{	// All the damage gets absorbed by the shields.
		save = damage;
	}

	// save is the most damage that the armor is elibigle to protect, of course, but it's limited by the total armor.
	if (save >= count)
		save = count;

	if (!save)
		return 0;

	if (dflags & DAMAGE_HALF_ARMOR_REDUCTION)		// Armor isn't whittled so easily by sniper shots.
	{
		client->ps.stats[STAT_ARMOR] -= (int)(save*ARMOR_REDUCTION_FACTOR);
	}
	else
	{
		client->ps.stats[STAT_ARMOR] -= save;
	}

	return save;
}


void G_ApplyKnockback( gentity_t *targ, vec3_t newDir, float knockback )
{
	vec3_t	kvel;
	float	mass;

	if ( targ->physicsBounce > 0 )	//overide the mass
		mass = targ->physicsBounce;
	else
		mass = 200;

	if ( g_gravity.value > 0 )
	{
		VectorScale( newDir, g_knockback.value * (float)knockback / mass * 0.8, kvel );
		kvel[2] = newDir[2] * g_knockback.value * (float)knockback / mass * 1.5;
	}
	else
	{
		VectorScale( newDir, g_knockback.value * (float)knockback / mass, kvel );
	}

	if ( targ->client )
	{
		VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );
	}
	else if ( targ->s.pos.trType != TR_STATIONARY && targ->s.pos.trType != TR_LINEAR_STOP && targ->s.pos.trType != TR_NONLINEAR_STOP )
	{
		VectorAdd( targ->s.pos.trDelta, kvel, targ->s.pos.trDelta );
		VectorCopy( targ->r.currentOrigin, targ->s.pos.trBase );
		targ->s.pos.trTime = level.time;
	}

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if ( targ->client && !targ->client->ps.pm_time ) 
	{
		int		t;

		t = knockback * 2;
		if ( t < 50 ) {
			t = 50;
		}
		if ( t > 200 ) {
			t = 200;
		}
		targ->client->ps.pm_time = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

/*
================
RaySphereIntersections
================
*/
int RaySphereIntersections( vec3_t origin, float radius, vec3_t point, vec3_t dir, vec3_t intersections[2] ) {
	float b, c, d, t;

	//	| origin - (point + t * dir) | = radius
	//	a = dir[0]^2 + dir[1]^2 + dir[2]^2;
	//	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	//	c = (point[0] - origin[0])^2 + (point[1] - origin[1])^2 + (point[2] - origin[2])^2 - radius^2;

	// normalize dir so a = 1
	VectorNormalize(dir);
	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	c = (point[0] - origin[0]) * (point[0] - origin[0]) +
		(point[1] - origin[1]) * (point[1] - origin[1]) +
		(point[2] - origin[2]) * (point[2] - origin[2]) -
		radius * radius;

	d = b * b - 4 * c;
	if (d > 0) {
		t = (- b + sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[0]);
		t = (- b - sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[1]);
		return 2;
	}
	else if (d == 0) {
		t = (- b ) / 2;
		VectorMA(point, t, dir, intersections[0]);
		return 1;
	}
	return 0;
}

/*
===================================
rww - beginning of the majority of the dismemberment and location based damage code.
===================================
*/
char *hitLocName[HL_MAX] = 
{
	"none",	//HL_NONE = 0,
	"right foot",	//HL_FOOT_RT,
	"left foot",	//HL_FOOT_LT,
	"right leg",	//HL_LEG_RT,
	"left leg",	//HL_LEG_LT,
	"waist",	//HL_WAIST,
	"back right shoulder",	//HL_BACK_RT,
	"back left shoulder",	//HL_BACK_LT,
	"back",	//HL_BACK,
	"front right shouler",	//HL_CHEST_RT,
	"front left shoulder",	//HL_CHEST_LT,
	"chest",	//HL_CHEST,
	"right arm",	//HL_ARM_RT,
	"left arm",	//HL_ARM_LT,
	"right hand",	//HL_HAND_RT,
	"left hand",	//HL_HAND_LT,
	"head",	//HL_HEAD
	"generic1",	//HL_GENERIC1,
	"generic2",	//HL_GENERIC2,
	"generic3",	//HL_GENERIC3,
	"generic4",	//HL_GENERIC4,
	"generic5",	//HL_GENERIC5,
	"generic6"	//HL_GENERIC6
};

void G_GetDismemberLoc(gentity_t *self, vec3_t boltPoint, int limbType)
{ //Just get the general area without using server-side ghoul2
	vec3_t fwd, right, up;

	AngleVectors(self->r.currentAngles, fwd, right, up);

	VectorCopy(self->r.currentOrigin, boltPoint);

	switch (limbType)
	{
	case G2_MODELPART_HEAD:
		boltPoint[0] += up[0]*24;
		boltPoint[1] += up[1]*24;
		boltPoint[2] += up[2]*24;
		break;
	case G2_MODELPART_WAIST:
		boltPoint[0] += up[0]*4;
		boltPoint[1] += up[1]*4;
		boltPoint[2] += up[2]*4;
		break;
	case G2_MODELPART_LARM:
		boltPoint[0] += up[0]*18;
		boltPoint[1] += up[1]*18;
		boltPoint[2] += up[2]*18;

		boltPoint[0] -= right[0]*10;
		boltPoint[1] -= right[1]*10;
		boltPoint[2] -= right[2]*10;
		break;
	case G2_MODELPART_RARM:
		boltPoint[0] += up[0]*18;
		boltPoint[1] += up[1]*18;
		boltPoint[2] += up[2]*18;

		boltPoint[0] += right[0]*10;
		boltPoint[1] += right[1]*10;
		boltPoint[2] += right[2]*10;
		break;
	case G2_MODELPART_RHAND:
		boltPoint[0] += up[0]*8;
		boltPoint[1] += up[1]*8;
		boltPoint[2] += up[2]*8;

		boltPoint[0] += right[0]*10;
		boltPoint[1] += right[1]*10;
		boltPoint[2] += right[2]*10;
		break;
	case G2_MODELPART_LLEG:
		boltPoint[0] -= up[0]*4;
		boltPoint[1] -= up[1]*4;
		boltPoint[2] -= up[2]*4;

		boltPoint[0] -= right[0]*10;
		boltPoint[1] -= right[1]*10;
		boltPoint[2] -= right[2]*10;
		break;
	case G2_MODELPART_RLEG:
		boltPoint[0] -= up[0]*4;
		boltPoint[1] -= up[1]*4;
		boltPoint[2] -= up[2]*4;

		boltPoint[0] += right[0]*10;
		boltPoint[1] += right[1]*10;
		boltPoint[2] += right[2]*10;
		break;
	default:
		break;
	}

	return;
}

void G_GetDismemberBolt(gentity_t *self, vec3_t boltPoint, int limbType)
{
	int useBolt = self->genericValue5;
	vec3_t properOrigin, properAngles, addVel;
	mdxaBone_t	boltMatrix;
	float fVSpeed = 0;
	char *rotateBone = NULL;

	switch (limbType)
	{
	case G2_MODELPART_HEAD:
		rotateBone = "cranium";
		break;
	case G2_MODELPART_WAIST:
		if (self->localAnimIndex <= 1)
		{ //humanoid
			rotateBone = "thoracic";
		}
		else
		{
			rotateBone = "pelvis";
		}
		break;
	case G2_MODELPART_LARM:
		rotateBone = "lradius";
		break;
	case G2_MODELPART_RARM:
		rotateBone = "rradius";
		break;
	case G2_MODELPART_RHAND:
		rotateBone = "rhand";
		break;
	case G2_MODELPART_LLEG:
		rotateBone = "ltibia";
		break;
	case G2_MODELPART_RLEG:
		rotateBone = "rtibia";
		break;
	default:
		rotateBone = "rtibia";
		break;
	}

	useBolt = trap_G2API_AddBolt(self->ghoul2, 0, rotateBone);

	VectorCopy(self->client->ps.origin, properOrigin);
	VectorCopy(self->client->ps.viewangles, properAngles);

	//try to predict the origin based on velocity so it's more like what the client is seeing
	VectorCopy(self->client->ps.velocity, addVel);
	VectorNormalize(addVel);

	if (self->client->ps.velocity[0] < 0)
	{
		fVSpeed += (-self->client->ps.velocity[0]);
	}
	else
	{
		fVSpeed += self->client->ps.velocity[0];
	}
	if (self->client->ps.velocity[1] < 0)
	{
		fVSpeed += (-self->client->ps.velocity[1]);
	}
	else
	{
		fVSpeed += self->client->ps.velocity[1];
	}
	if (self->client->ps.velocity[2] < 0)
	{
		fVSpeed += (-self->client->ps.velocity[2]);
	}
	else
	{
		fVSpeed += self->client->ps.velocity[2];
	}

	fVSpeed *= 0.08;

	properOrigin[0] += addVel[0]*fVSpeed;
	properOrigin[1] += addVel[1]*fVSpeed;
	properOrigin[2] += addVel[2]*fVSpeed;

	properAngles[0] = 0;
	properAngles[1] = self->client->ps.viewangles[YAW];
	properAngles[2] = 0;

	trap_G2API_GetBoltMatrix(self->ghoul2, 0, useBolt, &boltMatrix, properAngles, properOrigin, level.time, NULL, self->modelScale);

	boltPoint[0] = boltMatrix.matrix[0][3];
	boltPoint[1] = boltMatrix.matrix[1][3];
	boltPoint[2] = boltMatrix.matrix[2][3];

	trap_G2API_GetBoltMatrix(self->ghoul2, 1, 0, &boltMatrix, properAngles, properOrigin, level.time, NULL, self->modelScale);
		}

void LimbTouch( gentity_t *self, gentity_t *other, trace_t *trace )
{
}

void LimbThink( gentity_t *ent )
{
	float gravity = 3.0f;
	float mass = 0.09f;
	float bounce = 1.3f;

	switch (ent->s.modelGhoul2)
	{
	case G2_MODELPART_HEAD:
		mass = 0.08f;
		bounce = 1.4f;
		break;
	case G2_MODELPART_WAIST:
		mass = 0.1f;
		bounce = 1.2f;
		break;
	case G2_MODELPART_LARM:
	case G2_MODELPART_RARM:
	case G2_MODELPART_RHAND:
	case G2_MODELPART_LLEG:
	case G2_MODELPART_RLEG:
	default:
		break;
	}

	if (ent->speed < level.time)
	{
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
		return;
	}

	if (ent->genericValue5 <= level.time)
	{ //this will be every frame by standard, but we want to compensate in case sv_fps is not 20.
		G_RunExPhys(ent, gravity, mass, bounce, qtrue, NULL, 0);
		ent->genericValue5 = level.time + 50;
	}

	ent->nextthink = level.time;
}

#include "namespace_begin.h"
extern qboolean BG_GetRootSurfNameWithVariant( void *ghoul2, const char *rootSurfName, char *returnSurfName, int returnSize );
#include "namespace_end.h"

void G_Dismember( gentity_t *ent, gentity_t *enemy, vec3_t point, int limbType, float limbRollBase, float limbPitchBase, int deathAnim, qboolean postDeath )
{
	vec3_t	newPoint, dir, vel;
	gentity_t *limb;
	char	limbName[MAX_QPATH];
	char	stubName[MAX_QPATH];
	char	stubCapName[MAX_QPATH];

	if (limbType == G2_MODELPART_HEAD)
	{
		Q_strncpyz( limbName , "head", sizeof( limbName  ) );
		Q_strncpyz( stubCapName, "torso_cap_head", sizeof( stubCapName ) );
	}
	else if (limbType == G2_MODELPART_WAIST)
	{
		Q_strncpyz( limbName, "torso", sizeof( limbName ) );
		Q_strncpyz( stubCapName, "hips_cap_torso", sizeof( stubCapName ) );
	}
	else if (limbType == G2_MODELPART_LARM)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "l_arm", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "torso", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_l_arm", stubName );
	}
	else if (limbType == G2_MODELPART_RARM)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_arm", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "torso", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_arm", stubName );
	}
	else if (limbType == G2_MODELPART_RHAND)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_hand", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_arm", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_hand", stubName );
	}
	else if (limbType == G2_MODELPART_LLEG)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "l_leg", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "hips", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_l_leg", stubName );
	}
	else if (limbType == G2_MODELPART_RLEG)
	{
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_leg", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "hips", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_leg", stubName );
	}
	else
	{//umm... just default to the right leg, I guess (same as on client)
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "r_leg", limbName, sizeof(limbName) );
		BG_GetRootSurfNameWithVariant( ent->ghoul2, "hips", stubName, sizeof(stubName) );
		Com_sprintf( stubCapName, sizeof( stubCapName), "%s_cap_r_leg", stubName );
	}

	if (ent->ghoul2 /*&& limbName */&& trap_G2API_GetSurfaceRenderStatus(ent->ghoul2, 0, limbName))
	{ //is it already off? If so there's no reason to be doing it again, so get out of here.
		return;
	}

	VectorCopy( point, newPoint );
	limb = G_Spawn();
	limb->classname = "playerlimb";

	G_SetOrigin( limb, newPoint );
	VectorCopy( newPoint, limb->s.pos.trBase );
	limb->think = LimbThink;
	limb->touch = LimbTouch;
	limb->speed = level.time + Q_irand(8000, 16000);
	limb->nextthink = level.time + FRAMETIME;

	limb->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	limb->clipmask = MASK_SOLID;
	limb->r.contents = CONTENTS_TRIGGER;
	limb->physicsObject = qtrue;
	VectorSet( limb->r.mins, -6.0f, -6.0f, -3.0f );
	VectorSet( limb->r.maxs, 6.0f, 6.0f, 6.0f );

	limb->s.g2radius = 200;

	limb->s.eType = ET_GENERAL;
	limb->s.weapon = G2_MODEL_PART;
	limb->s.modelGhoul2 = limbType;
	limb->s.modelindex = ent->s.number;
	if (!ent->client)
	{
		limb->s.modelindex = -1;
		limb->s.otherEntityNum2 = ent->s.number;
	}

	VectorClear(limb->s.apos.trDelta);

	if (ent->client)
	{
		VectorCopy(ent->client->ps.viewangles, limb->r.currentAngles);
		VectorCopy(ent->client->ps.viewangles, limb->s.apos.trBase);
	}
	else
	{
		VectorCopy(ent->r.currentAngles, limb->r.currentAngles);
		VectorCopy(ent->r.currentAngles, limb->s.apos.trBase);
	}

	//Set up the ExPhys values for the entity.
	limb->epGravFactor = 0;
	VectorClear(limb->epVelocity);
	VectorSubtract( point, ent->r.currentOrigin, dir );
	VectorNormalize( dir );
	if (ent->client)
	{
		VectorCopy(ent->client->ps.velocity, vel);
	}
	else
	{
		VectorCopy(ent->s.pos.trDelta, vel);
	}
	VectorMA( vel, 80, dir, limb->epVelocity );

	//add some vertical velocity
	if (limbType == G2_MODELPART_HEAD ||
		limbType == G2_MODELPART_WAIST)
	{
		limb->epVelocity[2] += 10;
	}

	if (enemy && enemy->client && ent && ent != enemy && ent->s.number != enemy->s.number &&
		enemy->client->ps.weapon == WP_SABER && enemy->client->olderIsValid &&
		(level.time - enemy->client->lastSaberStorageTime) < 200)
	{ //The enemy has valid saber positions between this and last frame. Use them to factor in direction of the limb.
		vec3_t dif;
		float totalDistance;
		const float distScale = 1.2f;

		//scale down the initial velocity first, which is based on the speed of the limb owner.
		//ExPhys object velocity operates on a slightly different scale than Q3-based physics velocity.
		VectorScale(limb->epVelocity, 0.4f, limb->epVelocity);

		VectorSubtract(enemy->client->lastSaberBase_Always, enemy->client->olderSaberBase, dif);
		totalDistance = VectorNormalize(dif);

		VectorScale(dif, totalDistance*distScale, dif);
		VectorAdd(limb->epVelocity, dif, limb->epVelocity);

		if (ent->client && (ent->client->ps.torsoTimer > 0 || !BG_InDeathAnim(ent->client->ps.torsoAnim)))
		{ //if he's done with his death anim we don't actually want the limbs going far
			vec3_t preVel;

			VectorCopy(limb->epVelocity, preVel);
			preVel[2] = 0;
			totalDistance = VectorNormalize(preVel);

			if (totalDistance < 40.0f)
			{
				float mAmt = 40.0f;

				limb->epVelocity[0] = preVel[0]*mAmt;
				limb->epVelocity[1] = preVel[1]*mAmt;
			}
		}
		else if (ent->client)
		{
			VectorScale(limb->epVelocity, 0.3f, limb->epVelocity);
		}
	}

	if (ent->s.eType == ET_NPC && ent->ghoul2 /*&& limbName && stubCapName*/)
	{ //if it's an npc remove these surfs on the server too. For players we don't even care cause there's no further dismemberment after death.
		trap_G2API_SetSurfaceOnOff(ent->ghoul2, limbName, 0x00000100);
		trap_G2API_SetSurfaceOnOff(ent->ghoul2, stubCapName, 0);
	}

	limb->s.customRGBA[0] = ent->s.customRGBA[0];
	limb->s.customRGBA[1] = ent->s.customRGBA[1];
	limb->s.customRGBA[2] = ent->s.customRGBA[2];
	limb->s.customRGBA[3] = ent->s.customRGBA[3];

	trap_LinkEntity( limb );
}

void DismembermentTest(gentity_t *self)
{
	int sect = G2_MODELPART_HEAD;
	vec3_t boltPoint;

	while (sect <= G2_MODELPART_RLEG)
	{
		G_GetDismemberBolt(self, boltPoint, sect);
		G_Dismember( self, self, boltPoint, sect, 90, 0, BOTH_DEATH1, qfalse );
		sect++;
	}
}

void DismembermentByNum(gentity_t *self, int num)
{
	int sect = G2_MODELPART_HEAD;
	vec3_t boltPoint;

	switch (num)
	{
	case 0:
		sect = G2_MODELPART_HEAD;
		break;
	case 1:
		sect = G2_MODELPART_WAIST;
		break;
	case 2:
		sect = G2_MODELPART_LARM;
		break;
	case 3:
		sect = G2_MODELPART_RARM;
		break;
	case 4:
		sect = G2_MODELPART_RHAND;
		break;
	case 5:
		sect = G2_MODELPART_LLEG;
		break;
	case 6:
		sect = G2_MODELPART_RLEG;
		break;
	default:
		break;
	}

	G_GetDismemberBolt(self, boltPoint, sect);
	G_Dismember( self, self, boltPoint, sect, 90, 0, BOTH_DEATH1, qfalse );
}

int G_GetHitQuad( gentity_t *self, vec3_t hitloc )
{
	vec3_t diff, fwdangles={0,0,0}, right;
	vec3_t clEye;
	float rightdot;
	float zdiff;
	int hitLoc = gPainHitLoc;

	if (self->client)
	{
		VectorCopy(self->client->ps.origin, clEye);
		clEye[2] += self->client->ps.viewheight;
	}
	else
	{
		VectorCopy(self->s.pos.trBase, clEye);
		clEye[2] += 16;
	}

	VectorSubtract( hitloc, clEye, diff );
	diff[2] = 0;
	VectorNormalize( diff );

	if (self->client)
	{
		fwdangles[1] = self->client->ps.viewangles[1];
	}
	else
	{
		fwdangles[1] = self->s.apos.trBase[1];
	}
	// Ultimately we might care if the shot was ahead or behind, but for now, just quadrant is fine.
	AngleVectors( fwdangles, NULL, right, NULL );

	rightdot = DotProduct(right, diff);
	zdiff = hitloc[2] - clEye[2];
	
	if ( zdiff > 0 )
	{
		if ( rightdot > 0.3 )
		{
			hitLoc = G2_MODELPART_RARM;
		}
		else if ( rightdot < -0.3 )
		{
			hitLoc = G2_MODELPART_LARM;
		}
		else
		{
			hitLoc = G2_MODELPART_HEAD;
		}
	}
	else if ( zdiff > -20 )
	{
		if ( rightdot > 0.1 )
		{
			hitLoc = G2_MODELPART_RARM;
		}
		else if ( rightdot < -0.1 )
		{
			hitLoc = G2_MODELPART_LARM;
		}
		else
		{
			hitLoc = G2_MODELPART_HEAD;
		}
	}
	else
	{
		if ( rightdot >= 0 )
		{
			hitLoc = G2_MODELPART_RLEG;
		}
		else
		{
			hitLoc = G2_MODELPART_LLEG;
		}
	}

	return hitLoc;
}

int gGAvoidDismember = 0;

void UpdateClientRenderBolts(gentity_t *self, vec3_t renderOrigin, vec3_t renderAngles);

qboolean G_GetHitLocFromSurfName( gentity_t *ent, const char *surfName, int *hitLoc, vec3_t point, vec3_t dir, vec3_t bladeDir, int mod )
{
	qboolean dismember = qfalse;
	int actualTime;
	int kneeLBolt = -1;
	int kneeRBolt = -1;
	int handRBolt = -1;
	int handLBolt = -1;
	int footRBolt = -1;
	int footLBolt = -1;

	*hitLoc = HL_NONE;

	if ( !surfName || !surfName[0] )
	{
		return qfalse;
	}

	if( !ent->client )
	{
		return qfalse;
	}

	if (!point)
	{
		return qfalse;
	}

	if ( ent->client 
		&& ( ent->client->NPC_class == CLASS_R2D2 
			|| ent->client->NPC_class == CLASS_R2D2 
			|| ent->client->NPC_class == CLASS_GONK
			|| ent->client->NPC_class == CLASS_MOUSE
			|| ent->client->NPC_class == CLASS_SENTRY
			|| ent->client->NPC_class == CLASS_INTERROGATOR
			|| ent->client->NPC_class == CLASS_SENTRY
			|| ent->client->NPC_class == CLASS_PROBE ) )
	{//we don't care about per-surface hit-locations or dismemberment for these guys 
		return qfalse;
	}

	if (ent->localAnimIndex <= 1)
	{ //humanoid
		handLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_hand");
		handRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_hand");
		kneeLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*hips_l_knee");
		kneeRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*hips_r_knee");
		footLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
		footRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");
	}

	if ( ent->client && (ent->client->NPC_class == CLASS_ATST) )
	{
		//FIXME: almost impossible to hit these... perhaps we should
		//		check for splashDamage and do radius damage to these parts?
		//		Or, if we ever get bbox G2 traces, that may fix it, too
		if (!Q_stricmp("head_light_blaster_cann",surfName))
		{
			*hitLoc = HL_ARM_LT;
		}
		else if (!Q_stricmp("head_concussion_charger",surfName))
		{
			*hitLoc = HL_ARM_RT;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_MARK1) )
	{
		if (!Q_stricmp("l_arm",surfName))
		{
			*hitLoc = HL_ARM_LT;
		}
		else if (!Q_stricmp("r_arm",surfName))
		{
			*hitLoc = HL_ARM_RT;
		}
		else if (!Q_stricmp("torso_front",surfName))
		{
			*hitLoc = HL_CHEST;
		}
		else if (!Q_stricmp("torso_tube1",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_tube2",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_tube3",surfName))
		{
			*hitLoc = HL_GENERIC3;
		}
		else if (!Q_stricmp("torso_tube4",surfName))
		{
			*hitLoc = HL_GENERIC4;
		}
		else if (!Q_stricmp("torso_tube5",surfName))
		{
			*hitLoc = HL_GENERIC5;
		}
		else if (!Q_stricmp("torso_tube6",surfName))
		{
			*hitLoc = HL_GENERIC6;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_MARK2) )
	{
		if (!Q_stricmp("torso_canister1",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_canister2",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_canister3",surfName))
		{
			*hitLoc = HL_GENERIC3;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_GALAKMECH) )
	{
		if (!Q_stricmp("torso_antenna",surfName)||!Q_stricmp("torso_antenna_base",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_shield",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else
		{
			*hitLoc = HL_CHEST;
		}
		return(qfalse);
	}

	//FIXME: check the hitLoc and hitDir against the cap tag for the place 
	//where the split will be- if the hit dir is roughly perpendicular to 
	//the direction of the cap, then the split is allowed, otherwise we
	//hit it at the wrong angle and should not dismember...
	actualTime = level.time;
	if ( !Q_strncmp( "hips", surfName, 4 ) )
	{//FIXME: test properly for legs
		*hitLoc = HL_WAIST;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (kneeLBolt>=0)
			{
				trap_G2API_GetBoltMatrix( ent->ghoul2, 0, kneeLBolt, 
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 100 )
				{//actually hit the knee
					*hitLoc = HL_LEG_LT;
				}
			}
			if (*hitLoc == HL_WAIST)
			{
				if (kneeRBolt>=0)
				{
					trap_G2API_GetBoltMatrix( ent->ghoul2, 0, kneeRBolt, 
									&boltMatrix, angles, ent->r.currentOrigin,
									actualTime, NULL, ent->modelScale );
					BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
					if ( DistanceSquared( point, tagOrg ) < 100 )
					{//actually hit the knee
						*hitLoc = HL_LEG_RT;
					}
				}
			}
		}
	}
	else if ( !Q_strncmp( "torso", surfName, 5 ) )
	{
		if ( !ent->client )
		{
			*hitLoc = HL_CHEST;
		}
		else
		{
			vec3_t	t_fwd, t_rt, t_up, dirToImpact;
			float frontSide, rightSide, upSide;
			AngleVectors( ent->client->renderInfo.torsoAngles, t_fwd, t_rt, t_up );

			if (ent->client->renderInfo.boltValidityTime != level.time)
			{
				vec3_t renderAng;

				renderAng[0] = 0;
				renderAng[1] = ent->client->ps.viewangles[YAW];
				renderAng[2] = 0;

				UpdateClientRenderBolts(ent, ent->client->ps.origin, renderAng);
			}

			VectorSubtract( point, ent->client->renderInfo.torsoPoint, dirToImpact );
			frontSide = DotProduct( t_fwd, dirToImpact );
			rightSide = DotProduct( t_rt, dirToImpact );
			upSide = DotProduct( t_up, dirToImpact );
			if ( upSide < -10 )
			{//hit at waist
				*hitLoc = HL_WAIST;
			}
			else
			{//hit on upper torso
				if ( rightSide > 4 )
				{
					*hitLoc = HL_ARM_RT;
				}
				else if ( rightSide < -4 )
				{
					*hitLoc = HL_ARM_LT;
				}
				else if ( rightSide > 2 )
				{
					if ( frontSide > 0 )
					{
						*hitLoc = HL_CHEST_RT;
					}
					else
					{
						*hitLoc = HL_BACK_RT;
					}
				}
				else if ( rightSide < -2 )
				{
					if ( frontSide > 0 )
					{
						*hitLoc = HL_CHEST_LT;
					}
					else
					{
						*hitLoc = HL_BACK_LT;
					}
				}
				else if ( upSide > -3 && mod == MOD_SABER )
				{
					*hitLoc = HL_HEAD;
				}
				else if ( frontSide > 0 )
				{
					*hitLoc = HL_CHEST;
				}
				else
				{
					*hitLoc = HL_BACK;
				}
			}
		}
	}
	else if ( !Q_strncmp( "head", surfName, 4 ) )
	{
		*hitLoc = HL_HEAD;
	}
	else if ( !Q_strncmp( "r_arm", surfName, 5 ) )
	{
		*hitLoc = HL_ARM_RT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (handRBolt>=0)
			{
				trap_G2API_GetBoltMatrix( ent->ghoul2, 0, handRBolt, 
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 256 )
				{//actually hit the hand
					*hitLoc = HL_HAND_RT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "l_arm", surfName, 5 ) )
	{
		*hitLoc = HL_ARM_LT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (handLBolt>=0)
			{
				trap_G2API_GetBoltMatrix( ent->ghoul2, 0, handLBolt, 
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 256 )
				{//actually hit the hand
					*hitLoc = HL_HAND_LT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "r_leg", surfName, 5 ) )
	{
		*hitLoc = HL_LEG_RT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (footRBolt>=0)
			{
				trap_G2API_GetBoltMatrix( ent->ghoul2, 0, footRBolt, 
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 100 )
				{//actually hit the foot
					*hitLoc = HL_FOOT_RT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "l_leg", surfName, 5 ) )
	{
		*hitLoc = HL_LEG_LT;
		if ( ent->client != NULL && ent->ghoul2 )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
			if (footLBolt>=0)
			{
				trap_G2API_GetBoltMatrix( ent->ghoul2, 0, footLBolt, 
								&boltMatrix, angles, ent->r.currentOrigin,
								actualTime, NULL, ent->modelScale );
				BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 100 )
				{//actually hit the foot
					*hitLoc = HL_FOOT_LT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "r_hand", surfName, 6 ) || !Q_strncmp( "w_", surfName, 2 ) )
	{//right hand or weapon
		*hitLoc = HL_HAND_RT;
	}
	else if ( !Q_strncmp( "l_hand", surfName, 6 ) )
	{
		*hitLoc = HL_HAND_LT;
	}

	if (g_dismember.integer == 100)
	{ //full probability...
		if ( ent->client && ent->client->NPC_class == CLASS_PROTOCOL )
		{
			dismember = qtrue;
		}
		else if ( dir && (dir[0] || dir[1] || dir[2]) &&
			bladeDir && (bladeDir[0] || bladeDir[1] || bladeDir[2]) )
		{//we care about direction (presumably for dismemberment)
			//if ( g_dismemberProbabilities->value<=0.0f||G_Dismemberable( ent, *hitLoc ) )
			if (1) //Fix me?
			{//either we don't care about probabilties or the probability let us continue
				char *tagName = NULL;
				float	aoa = 0.5f;
				//dir must be roughly perpendicular to the hitLoc's cap bolt
				switch ( *hitLoc )
				{
					case HL_LEG_RT:
						tagName = "*hips_cap_r_leg";
						break;
					case HL_LEG_LT:
						tagName = "*hips_cap_l_leg";
						break;
					case HL_WAIST:
						tagName = "*hips_cap_torso";
						aoa = 0.25f;
						break;
					case HL_CHEST_RT:
					case HL_ARM_RT:
					case HL_BACK_LT:
						tagName = "*torso_cap_r_arm";
						break;
					case HL_CHEST_LT:
					case HL_ARM_LT:
					case HL_BACK_RT:
						tagName = "*torso_cap_l_arm";
						break;
					case HL_HAND_RT:
						tagName = "*r_arm_cap_r_hand";
						break;
					case HL_HAND_LT:
						tagName = "*l_arm_cap_l_hand";
						break;
					case HL_HEAD:
						tagName = "*torso_cap_head";
						aoa = 0.25f;
						break;
					case HL_CHEST:
					case HL_BACK:
					case HL_FOOT_RT:
					case HL_FOOT_LT:
					default:
						//no dismemberment possible with these, so no checks needed
						break;
				}
				if ( tagName )
				{
					int tagBolt = trap_G2API_AddBolt( ent->ghoul2, 0, tagName );
					if ( tagBolt != -1 )
					{
						mdxaBone_t	boltMatrix;
						vec3_t	tagOrg, tagDir, angles;

						VectorSet( angles, 0, ent->r.currentAngles[YAW], 0 );
						trap_G2API_GetBoltMatrix( ent->ghoul2, 0, tagBolt, 
										&boltMatrix, angles, ent->r.currentOrigin,
										actualTime, NULL, ent->modelScale );
						BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, tagOrg );
						BG_GiveMeVectorFromMatrix( &boltMatrix, NEGATIVE_Y, tagDir );
						if ( DistanceSquared( point, tagOrg ) < 256 )
						{//hit close
							float dot = DotProduct( dir, tagDir );
							if ( dot < aoa && dot > -aoa )
							{//hit roughly perpendicular
								dot = DotProduct( bladeDir, tagDir );
								if ( dot < aoa && dot > -aoa )
								{//blade was roughly perpendicular
									dismember = qtrue;
								}
							}
						}
					}
				}
			}
		}
		else
		{ //hmm, no direction supplied.
			dismember = qtrue;
		}
	}
	return dismember;
}

void G_CheckForDismemberment(gentity_t *ent, gentity_t *enemy, vec3_t point, int damage, int deathAnim, qboolean postDeath)
{
	int hitLoc = -1, hitLocUse = -1;
	vec3_t boltPoint;
	int dismember = g_dismember.integer;

	if (ent->localAnimIndex > 1)
	{
		if (!ent->NPC)
		{
			return;
		}

		if (ent->client->NPC_class != CLASS_PROTOCOL)
		{ //this is the only non-humanoid allowed to do dismemberment.
			return;
		}
	}

	if (!dismember)
	{
		return;
	}

	if (gGAvoidDismember == 1)
	{
		return;
	}

	if (gGAvoidDismember != 2)
	{ //this means do the dismemberment regardless of randomness and damage
		if (Q_irand(0, 100) > dismember)
		{
			return;
		}

		if (damage < 5)
		{
			return;
		}
	}

	if (gGAvoidDismember == 2)
	{
		hitLoc = HL_HAND_RT;
	}
	else
	{
		if (d_saberGhoul2Collision.integer && ent->client && ent->client->g2LastSurfaceTime == level.time)
		{
			char hitSurface[MAX_QPATH];

			trap_G2API_GetSurfaceName(ent->ghoul2, ent->client->g2LastSurfaceHit, 0, hitSurface);

			if (hitSurface[0])
			{
				G_GetHitLocFromSurfName(ent, hitSurface, &hitLoc, point, vec3_origin, vec3_origin, MOD_UNKNOWN);
			}
		}

		if (hitLoc == -1)
		{
			hitLoc = G_GetHitLocation( ent, point );
		}
	}

	switch(hitLoc)
	{
	case HL_FOOT_RT:
	case HL_LEG_RT:
		hitLocUse = G2_MODELPART_RLEG;
		break;
	case HL_FOOT_LT:
	case HL_LEG_LT:
		hitLocUse = G2_MODELPART_LLEG;
		break;
		
	case HL_WAIST:
		hitLocUse = G2_MODELPART_WAIST;
		break;
	case HL_ARM_RT:
		hitLocUse = G2_MODELPART_RARM;
		break;
	case HL_HAND_RT:
		hitLocUse = G2_MODELPART_RHAND;
		break;
	case HL_ARM_LT:
	case HL_HAND_LT:
		hitLocUse = G2_MODELPART_LARM;
		break;
	case HL_HEAD:
		hitLocUse = G2_MODELPART_HEAD;
		break;
	default:
		hitLocUse = G_GetHitQuad(ent, point);
		break;
	}

	if (hitLocUse == -1)
	{
		return;
	}

	if (ent->client)
	{
		G_GetDismemberBolt(ent, boltPoint, hitLocUse);
		if ( g_austrian.integer 
			&& (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) )
		{
			G_LogPrintf( "Duel Dismemberment: %s dismembered at %s\n", ent->client->pers.netname, hitLocName[hitLoc] );
		}
	}
	else
	{
		G_GetDismemberLoc(ent, boltPoint, hitLocUse);
	}
	G_Dismember(ent, enemy, boltPoint, hitLocUse, 90, 0, deathAnim, postDeath);
}

void G_LocationBasedDamageModifier(gentity_t *inflictor, gentity_t *ent, vec3_t point, int mod, int dflags, int *damage)
{
	int hitLoc = -1;

	if (!g_locationBasedDamage.integer || (g_locationBasedDamage.integer != 1 && mod == MOD_SABER))
	{ //then leave it alone
		return;
	}

	if ( (dflags&DAMAGE_NO_HIT_LOC) )
	{ //then leave it alone
		return;
	}

	if (mod == MOD_SABER && *damage <= 1)
	{ //don't bother for idle damage
		return;
	}

	if (!point)
	{
		return;
	}

	if ( ent->client && ent->client->NPC_class == CLASS_VEHICLE )
	{//no location-based damage on vehicles
		return;
	}

	if ((d_saberGhoul2Collision.integer && ent->client && ent->client->g2LastSurfaceTime == level.time && mod == MOD_SABER) || //using ghoul2 collision? Then if the mod is a saber we should have surface data from the last hit (unless thrown).
		(d_projectileGhoul2Collision.integer && ent->client && ent->client->g2LastSurfaceTime == level.time)) //It's safe to assume we died from the projectile that just set our surface index. So, go ahead and use that as the surf I guess.
	{
		char hitSurface[MAX_QPATH];

		trap_G2API_GetSurfaceName(ent->ghoul2, ent->client->g2LastSurfaceHit, 0, hitSurface);

		if (hitSurface[0])
		{
			G_GetHitLocFromSurfName(ent, hitSurface, &hitLoc, point, vec3_origin, vec3_origin, MOD_UNKNOWN);
		}
	}

	if (hitLoc == -1)
	{
		hitLoc = G_GetHitLocation( ent, point );
	}

	if (g_locationBasedDamage.integer == 3) {
		switch (hitLoc) {
		case HL_FOOT_RT:
		case HL_FOOT_LT:
		case HL_LEG_RT:
		case HL_LEG_LT:
			*damage *= 0.7;
			break;
		case HL_WAIST:
		case HL_BACK_RT:
		case HL_BACK_LT:
		case HL_BACK:
		case HL_CHEST_RT:
		case HL_CHEST_LT:
		case HL_CHEST:
		case HL_ARM_RT:
		case HL_ARM_LT:
		case HL_HAND_RT:
		case HL_HAND_LT:
			break; //normal damage
		case HL_HEAD:
			*damage *= 1.3;
			break;
		default:
			break; //do nothing then
		}
	}
	else {
		switch (hitLoc) {
		case HL_FOOT_RT:
		case HL_FOOT_LT:
			*damage *= 0.5;
			break;
		case HL_LEG_RT:
		case HL_LEG_LT:
			*damage *= 0.7;
			break;
		case HL_WAIST:
		case HL_BACK_RT:
		case HL_BACK_LT:
		case HL_BACK:
		case HL_CHEST_RT:
		case HL_CHEST_LT:
		case HL_CHEST:
			break; //normal damage
		case HL_ARM_RT:
		case HL_ARM_LT:
			*damage *= 0.85;
			break;
		case HL_HAND_RT:
		case HL_HAND_LT:
			*damage *= 0.6;
			break;
		case HL_HEAD:
			*damage *= 1.3;
			break;
		default:
			break; //do nothing then
		}
	}
}
/*
===================================
rww - end dismemberment/lbd
===================================
*/

qboolean G_ThereIsAMaster(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->client && ent->client->ps.isJediMaster)
		{
			return qtrue;
		}

		i++;
	}

	return qfalse;
}

void G_Knockdown(gentity_t *victim)
{
	if (victim && victim->client && BG_KnockDownable(&victim->client->ps))
	{
		victim->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
		victim->client->ps.forceDodgeAnim = 0;
		victim->client->ps.forceHandExtendTime = level.time + 1100;
		victim->client->ps.quickerGetup = qfalse;
	}
}

static qboolean IsHeavyDamage(meansOfDeath_t mod, qboolean heavyMelee) {
	switch (mod) {
	case MOD_REPEATER_ALT:
	case MOD_ROCKET:
	case MOD_FLECHETTE_ALT_SPLASH:
	case MOD_ROCKET_HOMING:
	case MOD_THERMAL:
	case MOD_THERMAL_SPLASH:
	case MOD_TRIP_MINE_SPLASH:
	case MOD_TIMED_MINE_SPLASH:
	case MOD_DET_PACK_SPLASH:
	case MOD_VEHICLE:
	case MOD_CONC:
	case MOD_CONC_ALT:
	case MOD_SABER:
	case MOD_TURBLAST:
	case MOD_SUICIDE:
	case MOD_FALLING:
	case MOD_CRUSH:
	case MOD_TELEFRAG:
	case MOD_TRIGGER_HURT:
		return qtrue;
	case MOD_MELEE:
		return heavyMelee;
	default:
		return qfalse;
	}
}

/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
	DAMAGE_HALF_ABSORB		half shields, half health
	DAMAGE_HALF_ARMOR_REDUCTION		Any damage that shields incur is halved
============
*/
extern qboolean gSiegeRoundBegun;

int gPainMOD = 0;
int gPainHitLoc = -1;
vec3_t gPainPoint;

static qboolean IsNonLocationBasedDamageEligibleNPC(gentity_t *ent) {
	if (!ent || ent->s.eType != ET_NPC || !VALIDSTRING(ent->NPC_type))
		return qfalse;

	if (level.siegeMap == SIEGEMAP_ANSION && (!Q_stricmp(ent->NPC_type, "Alpha") || !Q_stricmp(ent->NPC_type, "Onasi")))
		return qtrue;
	if (level.siegeMap == SIEGEMAP_URBAN && tolower(*ent->NPC_type) == 'w')
		return qtrue;

	return qfalse;
}

extern qboolean PM_SaberInBrokenParry(int move);
extern qboolean BG_SabersOff(playerState_t *ps);
extern qboolean SaberAttacking(gentity_t *self);
extern qboolean PM_InSaberAnim(int anim);
extern void saberBackToOwner(gentity_t *saberent);
extern void TimeShiftLerp(float frac, vec3_t start, vec3_t end, vec3_t result);

qboolean SaberThrowInFOV(gentity_t *ent, gentity_t *from, int hFOV, int vFOV, gentity_t *printAngle1, gentity_t *printAngle2) {
	vec3_t	eyes;
	vec3_t	spot;
	vec3_t	deltaVector;
	vec3_t	angles, fromAngles;
	vec3_t	deltaAngles;

	if (from->client)
		VectorCopy(from->client->ps.viewangles, fromAngles);
	else
		VectorCopy(from->s.angles, fromAngles);

	CalcEntitySpot(from, SPOT_ORIGIN, eyes);

	CalcEntitySpot(ent, SPOT_ORIGIN, spot);

	if (g_saberThrowDefenseRewind.value) {
		vec3_t rewoundSpot;
		/*if (spot[0] == ent->s.pos.trBase[0] && spot[1] == ent->s.pos.trBase[1] && spot[2] == ent->s.pos.trBase[2])
			VectorSubtract(spot, ent->s.pos.trDelta, rewoundSpot);
		else*/
			VectorCopy(ent->s.pos.trBase, rewoundSpot);

		if (fabs(g_saberThrowDefenseRewind.value) >= 1.0f) {
			VectorCopy(rewoundSpot, spot);
		}
		else {
			TimeShiftLerp(fabs(g_saberThrowDefenseRewind.value), rewoundSpot, spot, spot);
		}
	}
	if (g_saberDefenseDebug.integer)
		G_TestPoint(spot, 0x00ff00, 5000);

	VectorSubtract(spot, eyes, deltaVector);

	vectoangles(deltaVector, angles);
	deltaAngles[PITCH] = AngleDelta(fromAngles[PITCH], angles[PITCH]);
	deltaAngles[YAW] = AngleDelta(fromAngles[YAW], angles[YAW]);
	if (g_saberDefenseDebug.integer) {
		static int lastPrintTime = 0;
		static char lastPrint[128] = { 0 };
		int now = trap_Milliseconds();
		char thisPrint[128] = { 0 };
		Com_sprintf(thisPrint, sizeof(thisPrint), "Angle: ^5%.2f^7   ", fabs(deltaAngles[YAW]));
		if (!lastPrintTime || now - lastPrintTime > 250 || Q_stricmp(lastPrint, thisPrint)) {
			if (printAngle1)
				PrintIngame(printAngle1 - g_entities, thisPrint);
			if (printAngle2)
				PrintIngame(printAngle2 - g_entities, thisPrint);
			lastPrintTime = now;
			Q_strncpyz(lastPrint, thisPrint, sizeof(lastPrint));
		}
	}
	if (fabs(deltaAngles[PITCH]) <= vFOV && fabs(deltaAngles[YAW]) <= hFOV)
	{
		return qtrue;
	}
	return qfalse;
}

int G_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
	vec3_t dir, vec3_t point, int damage, int dflags, int mod) {
	gclient_t	*client;
	int			take;
	int			asave;
	int			knockback;
	int			max;
	int			subamt = 0;
	float		famt = 0;
	float		hamt = 0;
	float		shieldAbsorbed = 0;
	int			rng;

	if (targ && targ->damageRedirect)
	{
		G_Damage(&g_entities[targ->damageRedirectTo], inflictor, attacker, dir, point, damage, dflags, mod);
		return 0;
	}

	if (mod == MOD_DEMP2 && targ && targ->s.eType && targ->s.eType == ET_NPC && targ->NPC && targ->NPC->stats.nodmgfrom && (targ->NPC->stats.nodmgfrom & FLAG_VEHICLE_FREEZE || targ->NPC->stats.nodmgfrom == -1))
	{
		return 0;
	}

	if (level.siegeMap == SIEGEMAP_URBAN && attacker && attacker->s.eType == ET_NPC && VALIDSTRING(attacker->NPC_type) && tolower(*attacker->NPC_type) == 'w') {
		return 0;
	}

	if (level.siegeMap == SIEGEMAP_BESPIN && attacker && attacker->s.eType == ET_NPC && targ && targ - g_entities < MAX_CLIENTS &&
		targ->client && targ->client->sess.sessionTeam == TEAM_BLUE && VALIDSTRING(attacker->NPC_type) && *attacker->NPC_type == 'C') {
		return 0;
	}

	if (level.siegeMap == SIEGEMAP_ANSION && attacker && attacker->s.eType == ET_NPC && VALIDSTRING(attacker->NPC_type) && (!Q_stricmp(attacker->NPC_type, "Alpha") || !Q_stricmp(attacker->NPC_type, "Onasi")))
		return 0;

	// target emoted == die easily
	if (targ && targ->client && targ->client->emoted && targ - g_entities - MAX_CLIENTS && level.isLivePug != ISLIVEPUG_NO &&
		(targ->client->sess.sessionTeam == TEAM_RED || targ->client->sess.sessionTeam == TEAM_BLUE) &&
		!(attacker && attacker->client && attacker->client->sess.sessionTeam == targ->client->sess.sessionTeam)) {
		damage = 9999;
	}

	// attacker emoted == inflict 0 damage against non-teammates in most cases
	if (attacker && attacker - g_entities < MAX_CLIENTS && attacker->client && attacker->client->emoted &&
		!(targ && targ - g_entities < MAX_CLIENTS && targ->client && targ->client->sess.sessionTeam == attacker->client->sess.sessionTeam)) {
		switch (mod) {
		case MOD_UNKNOWN:
		case MOD_TURBLAST:
		case MOD_SENTRY:
		case MOD_WATER:
		case MOD_SLIME:
		case MOD_LAVA:
		case MOD_CRUSH:
		case MOD_TELEFRAG:
		case MOD_FALLING:
		case MOD_SUICIDE:
		case MOD_TARGET_LASER:
		case MOD_TRIGGER_HURT:
		case MOD_TEAM_CHANGE:
		case MOD_MAX:
		case MOD_SPECIAL_SENTRYBOMB:
			break;
		default:
			return 0;
		}
	}

#ifdef _DEBUG
	int originalDamage = damage;
#endif
	int freeze = FREEZE_DEFAULT, freezeMinOverride = -1, freezeMaxOverride = -1, overrideFreezeTimeActual = -1;
	int jediSplashDmgReduction = JEDISPLASHDMGREDUCTION_DEFAULT, nonJediSaberDmgIncrease = NONJEDISABERDMGINCREASE_DEFAULT;
	qboolean negativeDamageOk = qfalse, onlyKnockback = qfalse;
	float specialDamageParamKnockbackMultiplier = 1.0f;
	if (g_gametype.integer == GT_SIEGE && attacker && attacker->client && attacker->client->siegeClass != -1) {
		for (int i = 0; i < MAX_SPECIALDAMAGEPARAMETERS; i++) {
			specialDamageParam_t *sdp = &bgSiegeClasses[attacker->client->siegeClass].outgoingDamageParam[i];
			if (!(sdp->mods & (1ll << (long long)mod)))
				continue;
			if (attacker == targ) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_SELF)))
					continue;
			}
			else if (targ && targ - g_entities < MAX_CLIENTS && targ->client && targ->client->sess.sessionTeam == targ->client->sess.sessionTeam) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_ALLY)))
					continue;
			}
			else if (targ && targ - g_entities < MAX_CLIENTS && targ->client) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_ENEMY)))
					continue;
			}
			else if (targ && targ->s.NPC_class == CLASS_VEHICLE) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_VEHICLE)))
					continue;
			}
			else {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_ENEMY)))
					continue;
			}

			if (sdp->damageMultiplier != 1.0f)
				damage = (int)((((float)damage) + 0.5f) * sdp->damageMultiplier);
			specialDamageParamKnockbackMultiplier = sdp->knockbackMultiplier;
			if (damage > sdp->damageMax)
				damage = sdp->damageMax;
			if (damage < sdp->damageMin)
				damage = sdp->damageMin;
			freeze = sdp->freeze;
			if (freeze == FREEZE_YES && sdp->freezeMin >= 0 && sdp->freezeMax > 0 && sdp->freezeMin <= sdp->freezeMax) {
				freezeMinOverride = sdp->freezeMin;
				freezeMaxOverride = sdp->freezeMax;
			}
			negativeDamageOk = sdp->negativeDamageOk;
			onlyKnockback = sdp->onlyKnockback;
			jediSplashDmgReduction = sdp->jediSplashDamageReduction;
			nonJediSaberDmgIncrease = sdp->nonJediSaberDamageIncrease;
#ifdef _DEBUG
			Com_Printf("Outgoing damage param: damage %d -> %d, knockback multiplier %.3f\n", originalDamage, damage, specialDamageParamKnockbackMultiplier);
#endif
			if (!damage)
				return 0;
			break;
		}
	}
	if (g_gametype.integer == GT_SIEGE && targ && targ->client && targ->client->siegeClass != -1) {
		for (int i = 0; i < MAX_SPECIALDAMAGEPARAMETERS; i++) {
			specialDamageParam_t *sdp = &bgSiegeClasses[targ->client->siegeClass].incomingDamageParam[i];
			if (!(sdp->mods & (1ll << (long long)mod)))
				continue;
			if (attacker == targ) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_SELF)))
					continue;
			}
			else if (attacker && attacker - g_entities < MAX_CLIENTS && attacker->client && attacker->client->sess.sessionTeam == targ->client->sess.sessionTeam) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_ALLY)))
					continue;
			}
			else if (attacker && attacker - g_entities < MAX_CLIENTS && attacker->client) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_ENEMY)))
					continue;
			}
			else if (attacker && attacker->s.NPC_class == CLASS_VEHICLE) {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_VEHICLE)))
					continue;
			}
			else {
				if (!(sdp->otherEntType & (1 << OTHERENTTYPE_OTHER)))
					continue;
			}

			if (sdp->damageMultiplier != 1.0f)
				damage = (int)((((float)damage) + 0.5f) * sdp->damageMultiplier);
			specialDamageParamKnockbackMultiplier = sdp->knockbackMultiplier;
			if (damage > sdp->damageMax)
				damage = sdp->damageMax;
			if (damage < sdp->damageMin)
				damage = sdp->damageMin;
			freeze = sdp->freeze;
			if (freeze == FREEZE_YES && sdp->freezeMin >= 0 && sdp->freezeMax > 0 && sdp->freezeMin <= sdp->freezeMax) {
				freezeMinOverride = sdp->freezeMin;
				freezeMaxOverride = sdp->freezeMax;
			}
			negativeDamageOk = sdp->negativeDamageOk;
			onlyKnockback = sdp->onlyKnockback;
			jediSplashDmgReduction = sdp->jediSplashDamageReduction;
			nonJediSaberDmgIncrease = sdp->nonJediSaberDamageIncrease;
#ifdef _DEBUG
			Com_Printf("Incoming damage param: damage %d -> %d, knockback multiplier %.3f\n", originalDamage, damage, specialDamageParamKnockbackMultiplier);
#endif
			if (!damage)
				return 0;
			break;
		}
	}

	if (level.siegeMap == SIEGEMAP_ANSION && targ && targ->maxHealth == 2000 &&
		attacker && attacker->client && attacker->client->ps.origin[0] <= 4000 &&
		VALIDSTRING(targ->target) && !Q_stricmp(targ->target, "ansion_obj2_part2") &&
		VALIDSTRING(targ->classname) && !Q_stricmp(targ->classname, "func_breakable")) {
		return 0;
	}

	if (g_gametype.integer == GT_SIEGE && targ && targ->client && targ->client->ps.weapon == WP_MELEE && targ->client->siegeClass != -1 && bgSiegeClasses[targ->client->siegeClass].classflags & (1 << CFL_WONDERWOMAN)) {
		float modifier = 0.5f;
		damage = (int)((((float)damage) + 0.5f) * modifier);
	}

	if (freeze == FREEZE_YES || (freeze != FREEZE_NO && mod == MOD_DEMP2 && targ && targ->inuse && targ->client))
	{
		if (targ->client->ps.electrifyTime < level.time)
		{//electrocution effect
			if (targ->s.eType == ET_NPC && targ->s.NPC_class == CLASS_VEHICLE &&
				targ->m_pVehicle && (targ->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER || targ->m_pVehicle->m_pVehicleInfo->type == VH_WALKER))
			{ //do some extra stuff to speeders/walkers
				if (freeze == FREEZE_YES && freezeMinOverride >= 0 && freezeMaxOverride > 0 && freezeMinOverride <= freezeMaxOverride) {
					rng = Q_irand(freezeMinOverride, freezeMaxOverride);
					overrideFreezeTimeActual = rng;
				}
				else {
					rng = Q_irand(3000, 4000);
				}
				if ((!g_friendlyFreeze.integer && g_gametype.integer >= GT_TEAM && attacker->client && targ->m_pVehicle && targ->m_pVehicle->m_pPilot && (gentity_t *)targ->m_pVehicle->m_pPilot - g_entities < MAX_CLIENTS && ((gentity_t *)(targ->m_pVehicle->m_pPilot))->client &&
					((gentity_t *)(targ->m_pVehicle->m_pPilot))->client->sess.sessionTeam == attacker->client->sess.sessionTeam) ||
					(!g_friendlyFreeze.integer && g_gametype.integer >= GT_TEAM && attacker->client && attacker - g_entities < MAX_CLIENTS && targ->teamnodmg == attacker->client->sess.sessionTeam)) {
					rng = 0;
				}
					targ->client->ps.electrifyTime = level.time + rng;
			}
			else if ((targ->s.NPC_class != CLASS_VEHICLE
				|| (targ->m_pVehicle && targ->m_pVehicle->m_pVehicleInfo->type != VH_FIGHTER)))
			{//don't do this to fighters
				int maxFreezeTime;
				if (freeze == FREEZE_YES && freezeMinOverride >= 0 && freezeMaxOverride > 0 && freezeMinOverride <= freezeMaxOverride)
					maxFreezeTime = freezeMaxOverride;
				else
					maxFreezeTime = 800;
				if (targ->client->sess.skillBoost) {
					float maxFreezeReductionFactor = 0.0f;
					switch (targ->client->sess.skillBoost) {
					case 1:		maxFreezeReductionFactor = g_skillboost1_dempMaxFrozenTimeReduction.value;		break;
					case 2:		maxFreezeReductionFactor = g_skillboost2_dempMaxFrozenTimeReduction.value;		break;
					case 3:		maxFreezeReductionFactor = g_skillboost3_dempMaxFrozenTimeReduction.value;		break;
					case 4:		maxFreezeReductionFactor = g_skillboost4_dempMaxFrozenTimeReduction.value;		break;
					case 5:		maxFreezeReductionFactor = g_skillboost5_dempMaxFrozenTimeReduction.value;		break;
					case 6:		maxFreezeReductionFactor = g_skillboost6_dempMaxFrozenTimeReduction.value;		break;
					case 7:		maxFreezeReductionFactor = g_skillboost7_dempMaxFrozenTimeReduction.value;		break;
					case 8:		maxFreezeReductionFactor = g_skillboost8_dempMaxFrozenTimeReduction.value;		break;
					case 9:		maxFreezeReductionFactor = g_skillboost9_dempMaxFrozenTimeReduction.value;		break;
					case 10:		maxFreezeReductionFactor = g_skillboost10_dempMaxFrozenTimeReduction.value;		break;
					}
					maxFreezeTime -= (int)((float)maxFreezeTime * maxFreezeReductionFactor);
				}
				if (freeze == FREEZE_YES && freezeMinOverride >= 0 && freezeMaxOverride > 0 && freezeMinOverride <= freezeMaxOverride) {
					rng = Q_irand(freezeMinOverride > maxFreezeTime ? maxFreezeTime : freezeMinOverride, maxFreezeTime);
					overrideFreezeTimeActual = rng;
				}
				else {
					rng = Q_irand(300, maxFreezeTime);
				}
				if (!g_friendlyFreeze.integer && g_gametype.integer >= GT_TEAM && attacker->client && attacker - g_entities < MAX_CLIENTS &&
					targ && targ->client && attacker->client->sess.sessionTeam == targ->client->sess.sessionTeam) {
					// fixme? this doesn't account for non-walker non-speeder non-fighter vehicles e.g. rancor_vehicle
					rng = 0;
				}
				overrideFreezeTimeActual = rng;
				targ->client->ps.electrifyTime = level.time + rng;

				// siege freezing stats
				if (g_gametype.integer == GT_SIEGE && attacker && attacker->client && attacker - g_entities >= 0 &&
					attacker - g_entities < MAX_CLIENTS && targ - g_entities >= 0 && targ - g_entities < MAX_CLIENTS &&
					(targ->client->sess.sessionTeam == TEAM_RED || targ->client->sess.sessionTeam == TEAM_BLUE) && targ->client->sess.sessionTeam == OtherTeam(attacker->client->sess.sessionTeam)) {
					int freezeStatIndex = -1, frozenStatIndex = -1;
					if (level.siegeMap == SIEGEMAP_HOTH) {
						freezeStatIndex =  attacker->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_HOTH_OFFDEMP : SIEGEMAPSTAT_HOTH_DEFDEMP;
						frozenStatIndex = targ->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_HOTH_OFFGOTDEMPED : SIEGEMAPSTAT_HOTH_DEFGOTDEMPED;
					} else if (level.siegeMap == SIEGEMAP_DESERT) {
						freezeStatIndex = attacker->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_DESERT_OFFDEMP : SIEGEMAPSTAT_DESERT_DEFDEMP;
						frozenStatIndex = targ->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_DESERT_OFFGOTDEMPED : SIEGEMAPSTAT_DESERT_DEFGOTDEMPED;
					} else if (level.siegeMap == SIEGEMAP_NAR) {
						freezeStatIndex = attacker->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_NAR_OFFDEMP : SIEGEMAPSTAT_NAR_DEFDEMP;
						frozenStatIndex = targ->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_NAR_OFFGOTDEMPED : SIEGEMAPSTAT_NAR_DEFGOTDEMPED;
					} else if (level.siegeMap == SIEGEMAP_CARGO) {
						freezeStatIndex = attacker->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_CARGO2_OFFDEMP : SIEGEMAPSTAT_CARGO2_DEFDEMP;
						frozenStatIndex = targ->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_CARGO2_OFFGOTDEMPED : SIEGEMAPSTAT_CARGO2_DEFGOTDEMPED;
					} else if (level.siegeMap == SIEGEMAP_BESPIN) {
						freezeStatIndex = attacker->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_BESPIN_OFFDEMP : SIEGEMAPSTAT_BESPIN_DEFDEMP;
						frozenStatIndex = targ->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_BESPIN_OFFGOTDEMPED : SIEGEMAPSTAT_BESPIN_DEFGOTDEMPED;
					} else if (level.siegeMap == SIEGEMAP_URBAN) {
						freezeStatIndex = attacker->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_URBAN_OFFDEMP : SIEGEMAPSTAT_URBAN_DEFDEMP;
						frozenStatIndex = targ->client->sess.sessionTeam == TEAM_RED ? SIEGEMAPSTAT_URBAN_OFFGOTDEMPED : SIEGEMAPSTAT_URBAN_DEFGOTDEMPED;
					}
					if (freezeStatIndex != -1 && frozenStatIndex != -1 && frozenStatIndex != freezeStatIndex) {
						attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][freezeStatIndex] += rng;
						targ->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][frozenStatIndex] += rng;
					}
				}
			}
		}
	}

	if (g_gametype.integer == GT_SIEGE && level.intermissiontime) { // allow damaging some NPCs in post game for fun
		if (targ && (targ - g_entities < MAX_CLIENTS || !targ->client || !targ->NPC))
			return 0;
		if (!g_intermissionKnockbackNPCs.integer)
			return 0;
		targ->NPC->stats.nodmgfrom = 0;
		targ->NPC->stats.specialKnockback = 0;
		targ->alliedTeam = 0;
	}
	else {
		if (g_gametype.integer == GT_SIEGE && !gSiegeRoundBegun && mod != MOD_SUICIDE && damage != 6666)
		{ //nothing can be damaged til the round starts, except killturrets command
			return 0;
		}
	}

	if (!targ->takedamage) {
		return 0;
	}

	if (g_antiLaming.integer && targ->classname && targ->classname[0] && !Q_stricmp(targ->classname, "misc_siege_item") && level.siegeMap == SIEGEMAP_DESERT && !level.totalObjectivesCompleted)
	{
		//can't damage the stations until the game starts
		return 0;
	}

	if ((targ->flags&FL_SHIELDED) && mod != MOD_SABER  && !targ->client)
	{//magnetically protected, this thing can only be damaged by lightsabers
		return 0;
	}

	if (level.siegeMap == SIEGEMAP_ANSION && targ->flags & (FL_DMG_BY_SABER_ONLY | FL_DMG_BY_HEAVY_WEAP_ONLY)) {
		if (mod != MOD_SABER && !IsHeavyDamage(mod, G_HeavyMelee(attacker)))
			return 0;
	}
	else {
		if ((targ->flags & FL_DMG_BY_SABER_ONLY) && mod != MOD_SABER)
			return 0;
		if (targ->flags & FL_DMG_BY_HEAVY_WEAP_ONLY && !IsHeavyDamage(mod, G_HeavyMelee(attacker)))
			return 0;
	}

	if (targ->client)
	{//don't take damage when in a walker, or fighter
		//unless the walker/fighter is dead!!! -rww
		if (targ->client->ps.clientNum < MAX_CLIENTS && targ->client->ps.m_iVehicleNum)
		{
			gentity_t *veh = &g_entities[targ->client->ps.m_iVehicleNum];
			if (veh->m_pVehicle/* && veh->health > 0*/) // duo: fix for bug where killing shot on vehicle can also damage pilot
			{
				if (veh->m_pVehicle->m_pVehicleInfo->type == VH_WALKER ||
					veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER)
				{
					if (!(dflags & DAMAGE_NO_PROTECTION))
					{
						return 0;
					}
				}
			}
		}
	}

	if (targ->flags & FL_BBRUSH)
	{
		if (mod == MOD_DEMP2 ||
			mod == MOD_DEMP2_ALT ||
			mod == MOD_BRYAR_PISTOL ||
			mod == MOD_BRYAR_PISTOL_ALT ||
			mod == MOD_MELEE)
		{ //these don't damage bbrushes.. ever
			if (mod != MOD_MELEE || !G_HeavyMelee(attacker))
			{ //let classes with heavy melee ability damage breakable brushes with fists
				return 0;
			}
		}
	}

	if (targ && targ->client && targ->client->sess.siegeDuelInProgress && attacker && attacker->client && attacker->s.number != targ->s.number &&
		(targ->client->sess.siegeDuelIndex != attacker->s.number || !attacker->client->sess.siegeDuelInProgress))
	{
		//target is siegedueling, but attacker is not his duel partner (probably a troll trying to attack a duelist)
		return 0;
	}

	if (attacker && attacker->client && attacker->client->sess.siegeDuelInProgress && targ && targ->client && targ->s.number != attacker->s.number &&
		(attacker->client->sess.siegeDuelIndex != targ->s.number || !targ->client->sess.siegeDuelInProgress))
	{
		//attacker is siegedueling, but target is not his duel partner (maybe a duelist accidentally hit a troll running around)
		return 0;
	}

	if (attacker && attacker->client && attacker->client->sess.siegeDuelInProgress && targ && (!targ->client || targ->s.eType == ET_NPC))
	{
		//a duelist hit a non-client object/NPC
		return 0;
	}

	if (attacker && attacker->client && attacker->client->sess.siegeDuelInProgress && targ && targ->client && targ->client->sess.siegeDuelInProgress && mod == MOD_MELEE)
	{
		return 0;
	}

	// guaranteed 100 damage saber throws on gunners
	if (g_damageFixes.integer & DAMAGEFIXES_SABERTHROW_GUNNERS &&
		g_gametype.integer == GT_SIEGE &&
		nonJediSaberDmgIncrease != NONJEDISABERDMGINCREASE_NO &&
		targ && targ->client && targ - g_entities < MAX_CLIENTS &&
		mod == MOD_SABER && (targ->client->ps.weapon != WP_SABER || nonJediSaberDmgIncrease == NONJEDISABERDMGINCREASE_YES) &&
		attacker && attacker->client && attacker - g_entities < MAX_CLIENTS && attacker->client->ps.saberInFlight &&
		attacker->client->ps.saberMove != LS_DUAL_FB && attacker->client->ps.saberMove != LS_DUAL_LR) {
		// duoTODO: fix damage for saberthrow with dual saber when holding mouse1 with the other saber at the same time

		if (level.time - attacker->client->saberThrowDamageTime[targ - g_entities] < 1000)
			return 0; // duoTODO: fix sabers to return instantly all the time...

		attacker->client->saberThrowDamageTime[targ - g_entities] = level.time;
		damage = 100;
	}

	// improved damage system for saber vs saber
	if (g_damageFixes.integer & DAMAGEFIXES_SABERTHROW_SABERISTS &&
		g_gametype.integer == GT_SIEGE &&
		targ && targ->client && targ - g_entities < MAX_CLIENTS &&
		mod == MOD_SABER && targ->client->ps.weapon == WP_SABER &&
		attacker && attacker->client && attacker - g_entities < MAX_CLIENTS && attacker->client->ps.saberInFlight && attacker->client->ps.saberEntityNum &&
		attacker->client->ps.saberEntityNum != ENTITYNUM_NONE && attacker->client->ps.saberMove != LS_DUAL_FB && attacker->client->ps.saberMove != LS_DUAL_LR) {
		// duoTODO: fix damage for saberthrow with dual saber when holding mouse1 with the other saber at the same time

		if (level.time - attacker->client->saberThrowDamageTime[targ - g_entities] < 1000)
			return 0; // duoTODO: fix sabers to return instantly all the time...

		attacker->client->saberThrowDamageTime[targ - g_entities] = level.time;
		float multiplier;
		gentity_t *thrownSaber = &g_entities[attacker->client->ps.saberEntityNum];

		if (!targ->client->ps.saberEntityNum || BG_SabersOff(&targ->client->ps) || targ->client->ps.saberInFlight) {

			multiplier = 1.0f;

			if (g_saberDefenseDebug.integer) {
				char *reason = "No saber";
				if (!targ->client->ps.saberEntityNum)
					reason = "No saber entity";
				else if (BG_SabersOff(&targ->client->ps))
					reason = "Saber off";
				/*else if (targ->client->ps.weaponstate == WEAPON_RAISING)
					reason = "Weapon raising";*/
				else if (targ->client->ps.saberInFlight)
					reason = "Saber in flight";

				PrintIngame(attacker - g_entities, "Category: ^1%s^7   Multiplier: ^5%0.2f^7", reason, multiplier);
				PrintIngame(targ - g_entities, "Category: ^1%s^7   Multiplier: ^5%0.2f^7", reason, multiplier);
			}
		}
		else if (g_saberThrowDefenseSmallAngle.integer && SaberThrowInFOV(thrownSaber, targ, abs(g_saberThrowDefenseSmallAngle.integer), 180, attacker, targ)) {
			multiplier = fabs(g_saberThrowDefenseSmallDamage.value);
			if (g_saberDefenseDebug.integer) {
				PrintIngame(attacker - g_entities, "Category: ^2Small cone^7   Multiplier: ^5%0.2f^7", multiplier);
				PrintIngame(targ - g_entities, "Category: ^2Small cone^7   Multiplier: ^5%0.2f^7", multiplier);
			}
		}
		else if (g_saberThrowDefenseLargeAngle.integer && SaberThrowInFOV(thrownSaber, targ, abs(g_saberThrowDefenseLargeAngle.integer), 180, attacker, targ)) {
			multiplier = fabs(g_saberThrowDefenseLargeDamage.value);
			if (g_saberDefenseDebug.integer) {
				PrintIngame(attacker - g_entities, "Category: ^3Large cone^7   Multiplier: ^5%0.1f^7", multiplier);
				PrintIngame(targ - g_entities, "Category: ^3Large cone^7   Multiplier: ^5%0.1f^7", multiplier);
			}
		}
		else {
			multiplier = 1.0f;
			if (g_saberDefenseDebug.integer) {
				PrintIngame(attacker - g_entities, "Category: ^1Back^7   Multiplier: ^5%0.2f^7", multiplier);
				PrintIngame(targ - g_entities, "Category: ^1Back^7   Multiplier: ^5%0.2f^7", multiplier);
			}
		}

		qboolean printFinalMultiplier = qfalse;
		if (g_saberThrowDefenseCompromisedBonus.value && (BG_SaberInAttack(targ->client->ps.saberMove) || PM_SaberInBrokenParry(targ->client->ps.saberMove) ||
			/*targ->client->ps.weaponstate == WEAPON_RAISING || */ targ->client->pers.cmd.buttons & BUTTON_ATTACK || SaberAttacking(targ) || (targ->client->ps.saberMove != LS_READY &&
				!targ->client->ps.saberBlocking) || targ->client->ps.saberBlockTime >= level.time || (PM_InSaberAnim(targ->client->ps.torsoAnim) && !targ->client->ps.saberBlocked &&
					targ->client->ps.saberMove != LS_READY && targ->client->ps.saberMove != LS_NONE &&
					(targ->client->ps.saberMove < LS_PARRY_UP || targ->client->ps.saberMove > LS_REFLECT_LL)))) {

			multiplier += fabs(g_saberThrowDefenseCompromisedBonus.value);
			printFinalMultiplier = qtrue;

			if (g_saberDefenseDebug.integer) {
				char *reason = "Saber defense compromised";
				if (BG_SaberInAttack(targ->client->ps.saberMove))
					reason = "In saber move";
				else if (PM_SaberInBrokenParry(targ->client->ps.saberMove))
					reason = "In broken parry";
				/*else if (targ->client->ps.weaponstate == WEAPON_RAISING)
					reason = "Weapon raising";*/
				else if (targ->client->pers.cmd.buttons & BUTTON_ATTACK)
					reason = "Attack button down";
				else if (SaberAttacking(targ))
					reason = "Saber attacking";
				else if (targ->client->ps.saberMove != LS_READY && !targ->client->ps.saberBlocking)
					reason = "Saber move != ready and not blocking";
				else if (targ->client->ps.saberBlockTime >= level.time)
					reason = "In saber block cooldown";
				else if (PM_InSaberAnim(targ->client->ps.torsoAnim) && !targ->client->ps.saberBlocked &&
					targ->client->ps.saberMove != LS_READY && targ->client->ps.saberMove != LS_NONE &&
					(targ->client->ps.saberMove < LS_PARRY_UP || targ->client->ps.saberMove > LS_REFLECT_LL))
					reason = "In saber animation";

				if (g_saberDefenseDebug.integer) {
					PrintIngame(attacker - g_entities, "   Defense compromised multiplier bonus: ^8+%0.2f^7", g_saberThrowDefenseCompromisedBonus.value);
					PrintIngame(targ - g_entities, "   Defense compromised multiplier bonus: ^8+%0.2f^7", g_saberThrowDefenseCompromisedBonus.value);
				}
			}
		}

		if (targ->client->ps.forceHandExtend && g_saberThrowDefensePushPullBonus.value) {
			multiplier += fabs(g_saberThrowDefensePushPullBonus.value);
			printFinalMultiplier = qtrue;

			if (g_saberDefenseDebug.integer) {
				PrintIngame(attacker - g_entities, "   Push/pull multiplier bonus: ^8+%0.2f^7", g_saberThrowDefensePushPullBonus.value);
				PrintIngame(targ - g_entities, "   Push/pull multiplier bonus: ^8+%0.2f^7", g_saberThrowDefensePushPullBonus.value);
			}
		}

		if (printFinalMultiplier && g_saberDefenseDebug.integer) {
			PrintIngame(attacker - g_entities, "   Final multiplier: ^6%0.2f^7", multiplier);
			PrintIngame(targ - g_entities, "   Final multiplier: ^6%0.2f^7", multiplier);
		}

		if (multiplier == 1.0f)
			damage = targ->client->ps.stats[STAT_MAX_HEALTH];
		else
			damage = (int)round((double)targ->client->ps.stats[STAT_MAX_HEALTH] * (double)multiplier);
		damage = Com_Clampi(1, targ->client->ps.stats[STAT_MAX_HEALTH], damage);

		if (g_saberDefenseDebug.integer) {
			PrintIngame(attacker - g_entities, "   Damage: ^5%d^7\n", damage);
			PrintIngame(targ - g_entities, "   Damage: ^5%d^7\n", damage);
		}

		if (!multiplier)
			return 0;
	}

	// only enemy demp/disruptor can damage rockets
	if (g_damageFixes.integer & DAMAGEFIXES_ROCKET_HP &&
		g_gametype.integer == GT_SIEGE &&
		targ && VALIDSTRING(targ->classname) && !Q_stricmp(targ->classname, "rocket_proj") &&
		attacker && attacker - g_entities < MAX_CLIENTS && attacker->client) {
		if (targ->parent && targ->parent - g_entities < MAX_CLIENTS &&
			targ->parent->client && targ->parent->client->sess.sessionTeam == attacker->client->sess.sessionTeam) {
			return 0; // no teamkills
		}
		if (mod != MOD_DISRUPTOR && mod != MOD_DISRUPTOR_SNIPER && mod != MOD_DEMP2 && mod != MOD_DEMP2_ALT)
			return 0; // only allow disruptor and demp mouse2
	}

	if (g_gametype.integer == GT_SIEGE && mod == MOD_SABER &&
		targ && targ->client && targ - g_entities < MAX_CLIENTS &&
		attacker && attacker->client && attacker - g_entities < MAX_CLIENTS &&
		targ->s.groundEntityNum == ENTITYNUM_NONE) {
		trace_t tr;
		vec3_t down;
		VectorCopy(targ->r.currentOrigin, down);
		down[2] -= 4096;
		trap_Trace(&tr, targ->r.currentOrigin, targ->r.mins, targ->r.maxs, down, targ - g_entities, MASK_SOLID);
		VectorSubtract(targ->r.currentOrigin, tr.endpos, down);
		float groundDist = VectorLength(down);
		if (groundDist >= AIRSHOT_GROUND_DISTANCE_THRESHOLD) {
			attacker->client->lastAiredOtherClientTime[targ - g_entities] = level.time;
			attacker->client->lastAiredOtherClientMeansOfDeath[targ - g_entities] = MOD_SABER;
		}
		//PrintIngame(-1, "Ground distance is %0.2f\n", groundDist);
	}

	if ( targ && targ->client && targ->client->ps.duelInProgress )
	{
        // make sure we are attacked by client and not from environment
        if ( attacker && attacker->client && (attacker->s.number != targ->s.number) )
        {
            // make sure we are attacked by our duel oponnent
            if (attacker->s.number == targ->client->ps.duelIndex)
            {
			    if (g_gametype.integer == GT_CTF){
				    if (mod != MOD_BRYAR_PISTOL && mod != MOD_BRYAR_PISTOL_ALT){
					    return 0;
				    }
			    } else {
				    if (mod != MOD_SABER){
					    return 0;
				    }
			    }  
            }
        }
	}

	if ( !(dflags & DAMAGE_NO_PROTECTION) ) 
	{//rage overridden by no_protection
		if (targ && targ->client && (targ->client->ps.fd.forcePowersActive & (1 << FP_RAGE)))
		{
			damage *= 0.5;
		}
	}

	// the intermission has already been qualified for, so don't
	// allow any extra scoring
	if ( level.intermissionQueued ) {
		return 0;
	}
	if ( !inflictor ) {
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}
	if ( !attacker ) {
		attacker = &g_entities[ENTITYNUM_WORLD];
	}

	// shootable doors / buttons don't actually have any health

	//if genericValue4 == 1 then it's glass or a breakable and those do have health
	if ( targ->s.eType == ET_MOVER && targ->genericValue4 != 1 ) {
		if ( targ->use && targ->moverState == MOVER_POS1 ) {
			GlobalUse( targ, inflictor, attacker );
		}
		return 0;
	}

	if (mod == MOD_SABER && targ && VALIDSTRING(targ->classname) && !strcmp(targ->classname, "sentryGun") && g_gametype.integer == GT_SIEGE &&
		attacker && attacker->client && attacker - g_entities < MAX_CLIENTS && g_saberHitsToKillSentry.integer > 0
		/*&& attacker->client->ps.saberInFlight && attacker->client->ps.saberMove != LS_DUAL_FB && attacker->client->ps.saberMove != LS_DUAL_LR*/) {

		if (level.time - attacker->client->saberThrowDamageTime[targ - g_entities] < 200)
			return 0; // duoTODO: fix sabers to return instantly all the time...

		attacker->client->saberThrowDamageTime[targ - g_entities] = level.time;
		damage = Com_Clampi(1, SENTRY_HP, (int)ceilf((float)SENTRY_HP / (float)g_saberHitsToKillSentry.integer));
	}

	// guarantee sabers oneshot mines/detpacks (fix stupid low damage bug)
	if (mod == MOD_SABER && targ && VALIDSTRING(targ->classname) && (!strcmp(targ->classname, "laserTrap") || !strcmp(targ->classname, "detpack"))
		&& attacker && attacker->client && attacker - g_entities < MAX_CLIENTS) {
		damage = 999;
	}

	// reduce damage by the attacker's handicap value
	// unless they are rocket jumping
	if ( attacker->client 
		&& attacker != targ 
		&& attacker->s.eType == ET_PLAYER 
		&& g_gametype.integer != GT_SIEGE ) 
	{
		max = attacker->client->ps.stats[STAT_MAX_HEALTH];
		damage = damage * max / 100;
	}

	if ( !(dflags&DAMAGE_NO_HIT_LOC) )
	{//see if we should modify it by damage location
		if (!IsNonLocationBasedDamageEligibleNPC(targ)) {
			if (targ->inuse && (targ->client || targ->s.eType == ET_NPC) &&
				attacker && attacker->inuse && (attacker->client || attacker->s.eType == ET_NPC))
			{ //check for location based damage stuff.
				G_LocationBasedDamageModifier(attacker, targ, point, mod, dflags, &damage);
			}
		}
	}

	if ( targ->client 
		&& targ->client->NPC_class == CLASS_RANCOR 
		&& (!attacker||!attacker->client||attacker->client->NPC_class!=CLASS_RANCOR) )
	{
		// I guess always do 10 points of damage...feel free to tweak as needed
		if ( damage < 10 )
		{//ignore piddly little damage
			damage = 0;
		}
		else if ( damage >= 10 )
		{
			damage = 10;
		}
	}

	if (targ && targ - g_entities < MAX_CLIENTS && targ->client && attacker && attacker - g_entities < MAX_CLIENTS && attacker->client &&
		targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam) {
		level.siegeTopTimes[targ - g_entities].attackedByNonTeammate = qtrue;
		SpeedRunModeRuined("G_Damage: attacked by non-teammate player");
	}

	client = targ->client;

	if ( client ) {
		if ( client->noclip ) {
			return 0;
		}

		// anything that isn't fall dmg or scoped disruptor has an impact on the flag capture record type
		// this should catch all weapons (except conc alt) + grip and lightning
		// we aren't checking this in FireWeapon because firing has no impact on our run until it changes our trajectory
		if ( mod != MOD_FALLING && mod != MOD_DISRUPTOR_SNIPER ) {
			if ( targ != attacker || mod == MOD_TRIP_MINE_SPLASH || mod == MOD_TIMED_MINE_SPLASH || mod == MOD_DET_PACK_SPLASH ) {
				//client->runInvalid = qtrue;
			} else {
				client->usedWeapon = qtrue;
			}
		}
	}

	if ( !dir ) {
		dflags |= DAMAGE_NO_KNOCKBACK;
	} else {
		VectorNormalize(dir);
	}

	knockback = damage;
	if (specialDamageParamKnockbackMultiplier != 1.0f)
		knockback = (int)((((float)knockback) + 0.5f) * specialDamageParamKnockbackMultiplier);
	if ( knockback > 200 ) {
		knockback = 200;
	}
	if ( targ->flags & FL_NO_KNOCKBACK ) {
		knockback = 0;
	}
	if ( dflags & DAMAGE_NO_KNOCKBACK ) {
		knockback = 0;
	}
	if (level.siegeMap == SIEGEMAP_URBAN && targ->m_pVehicle && targ->m_pVehicle->m_pVehicleInfo && targ->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER && VALIDSTRING(targ->m_pVehicle->m_pVehicleInfo->name) && !Q_stricmp(targ->m_pVehicle->m_pVehicleInfo->name, "swoop_urban")) {
		knockback = 0;
	}

	if (targ->s.eType == ET_NPC && targ->NPC->stats.noKnockbackFrom) //target in question is an npc and noKnockbackFrom is set
	{
		if (mod == MOD_MELEE && targ->NPC->stats.noKnockbackFrom & FLAG_MELEE ||
			mod == MOD_STUN_BATON && targ->NPC->stats.noKnockbackFrom & FLAG_STUNBATON ||
			mod == MOD_SABER && targ->NPC->stats.noKnockbackFrom & FLAG_SABER ||
			(mod == MOD_BRYAR_PISTOL || mod == MOD_BRYAR_PISTOL_ALT) && targ->NPC->stats.noKnockbackFrom & FLAG_PISTOL ||
			mod == MOD_BLASTER && targ->NPC->stats.noKnockbackFrom & FLAG_E11 ||
			(mod == MOD_DISRUPTOR || mod == MOD_DISRUPTOR_SPLASH || mod == MOD_DISRUPTOR_SNIPER) && targ->NPC->stats.noKnockbackFrom & FLAG_DISRUPTOR ||
			mod == MOD_BOWCASTER && targ->NPC->stats.noKnockbackFrom & FLAG_BOWCASTER ||
			(mod == MOD_REPEATER || mod == MOD_REPEATER_ALT || mod == MOD_REPEATER_ALT_SPLASH) && targ->NPC->stats.noKnockbackFrom & FLAG_REPEATER ||
			(mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) && targ->NPC->stats.noKnockbackFrom & FLAG_DEMP ||
			(mod == MOD_FLECHETTE || mod == MOD_FLECHETTE_ALT_SPLASH) && targ->NPC->stats.noKnockbackFrom & FLAG_GOLAN ||
			(mod == MOD_ROCKET || mod == MOD_ROCKET_SPLASH || mod == MOD_ROCKET_HOMING || mod == MOD_ROCKET_HOMING_SPLASH) && targ->NPC->stats.noKnockbackFrom & FLAG_ROCKET ||
			(mod == MOD_THERMAL || mod == MOD_THERMAL_SPLASH) && targ->NPC->stats.noKnockbackFrom & FLAG_THERMAL ||
			(mod == MOD_TRIP_MINE_SPLASH || mod == MOD_TIMED_MINE_SPLASH) && targ->NPC->stats.noKnockbackFrom & FLAG_MINE ||
			mod == MOD_DET_PACK_SPLASH && targ->NPC->stats.noKnockbackFrom & FLAG_DETPACK ||
			(mod == MOD_CONC || mod == MOD_CONC_ALT) && targ->NPC->stats.noKnockbackFrom & FLAG_CONC)
		{
			knockback = 0; //no knockback
		}
	}

	if (targ->s.eType == ET_NPC && targ->NPC->stats.doubleKnockbackFrom) //target in question is an npc and doubleKnockbackFrom is set
	{
		if (mod == MOD_MELEE && targ->NPC->stats.doubleKnockbackFrom & FLAG_MELEE ||
			mod == MOD_STUN_BATON && targ->NPC->stats.doubleKnockbackFrom & FLAG_STUNBATON ||
			mod == MOD_SABER && targ->NPC->stats.doubleKnockbackFrom & FLAG_SABER ||
			(mod == MOD_BRYAR_PISTOL || mod == MOD_BRYAR_PISTOL_ALT) && targ->NPC->stats.doubleKnockbackFrom & FLAG_PISTOL ||
			mod == MOD_BLASTER && targ->NPC->stats.doubleKnockbackFrom & FLAG_E11 ||
			(mod == MOD_DISRUPTOR || mod == MOD_DISRUPTOR_SPLASH || mod == MOD_DISRUPTOR_SNIPER) && targ->NPC->stats.doubleKnockbackFrom & FLAG_DISRUPTOR ||
			mod == MOD_BOWCASTER && targ->NPC->stats.doubleKnockbackFrom & FLAG_BOWCASTER ||
			(mod == MOD_REPEATER || mod == MOD_REPEATER_ALT || mod == MOD_REPEATER_ALT_SPLASH) && targ->NPC->stats.doubleKnockbackFrom & FLAG_REPEATER ||
			(mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) && targ->NPC->stats.doubleKnockbackFrom & FLAG_DEMP ||
			(mod == MOD_FLECHETTE || mod == MOD_FLECHETTE_ALT_SPLASH) && targ->NPC->stats.doubleKnockbackFrom & FLAG_GOLAN ||
			(mod == MOD_ROCKET || mod == MOD_ROCKET_SPLASH || mod == MOD_ROCKET_HOMING || mod == MOD_ROCKET_HOMING_SPLASH) && targ->NPC->stats.doubleKnockbackFrom & FLAG_ROCKET ||
			(mod == MOD_THERMAL || mod == MOD_THERMAL_SPLASH) && targ->NPC->stats.doubleKnockbackFrom & FLAG_THERMAL ||
			(mod == MOD_TRIP_MINE_SPLASH || mod == MOD_TIMED_MINE_SPLASH) && targ->NPC->stats.doubleKnockbackFrom & FLAG_MINE ||
			mod == MOD_DET_PACK_SPLASH && targ->NPC->stats.doubleKnockbackFrom & FLAG_DETPACK ||
			(mod == MOD_CONC || mod == MOD_CONC_ALT) && targ->NPC->stats.doubleKnockbackFrom & FLAG_CONC)
		{
			knockback *= 2; //twice the fall, double the knockback
		}
	}

	if (targ->s.eType == ET_NPC && targ->NPC->stats.tripleKnockbackFrom) //target in question is an npc and tripleKnockbackFrom is set
	{
		if (mod == MOD_MELEE && targ->NPC->stats.tripleKnockbackFrom & FLAG_MELEE ||
			mod == MOD_STUN_BATON && targ->NPC->stats.tripleKnockbackFrom & FLAG_STUNBATON ||
			mod == MOD_SABER && targ->NPC->stats.tripleKnockbackFrom & FLAG_SABER ||
			(mod == MOD_BRYAR_PISTOL || mod == MOD_BRYAR_PISTOL_ALT) && targ->NPC->stats.tripleKnockbackFrom & FLAG_PISTOL ||
			mod == MOD_BLASTER && targ->NPC->stats.tripleKnockbackFrom & FLAG_E11 ||
			(mod == MOD_DISRUPTOR || mod == MOD_DISRUPTOR_SPLASH || mod == MOD_DISRUPTOR_SNIPER) && targ->NPC->stats.tripleKnockbackFrom & FLAG_DISRUPTOR ||
			mod == MOD_BOWCASTER && targ->NPC->stats.tripleKnockbackFrom & FLAG_BOWCASTER ||
			(mod == MOD_REPEATER || mod == MOD_REPEATER_ALT || mod == MOD_REPEATER_ALT_SPLASH) && targ->NPC->stats.tripleKnockbackFrom & FLAG_REPEATER ||
			(mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) && targ->NPC->stats.tripleKnockbackFrom & FLAG_DEMP ||
			(mod == MOD_FLECHETTE || mod == MOD_FLECHETTE_ALT_SPLASH) && targ->NPC->stats.tripleKnockbackFrom & FLAG_GOLAN ||
			(mod == MOD_ROCKET || mod == MOD_ROCKET_SPLASH || mod == MOD_ROCKET_HOMING || mod == MOD_ROCKET_HOMING_SPLASH) && targ->NPC->stats.tripleKnockbackFrom & FLAG_ROCKET ||
			(mod == MOD_THERMAL || mod == MOD_THERMAL_SPLASH) && targ->NPC->stats.tripleKnockbackFrom & FLAG_THERMAL ||
			(mod == MOD_TRIP_MINE_SPLASH || mod == MOD_TIMED_MINE_SPLASH) && targ->NPC->stats.tripleKnockbackFrom & FLAG_MINE ||
			mod == MOD_DET_PACK_SPLASH && targ->NPC->stats.tripleKnockbackFrom & FLAG_DETPACK ||
			(mod == MOD_CONC || mod == MOD_CONC_ALT) && targ->NPC->stats.tripleKnockbackFrom & FLAG_CONC)
		{
			knockback *= 3; //triple the knockback
		}
	}

	if (targ->s.eType == ET_NPC && targ->NPC->stats.quadKnockbackFrom) //target in question is an npc and quadKnockbackFrom is set
	{
		if (mod == MOD_MELEE && targ->NPC->stats.quadKnockbackFrom & FLAG_MELEE ||
			mod == MOD_STUN_BATON && targ->NPC->stats.quadKnockbackFrom & FLAG_STUNBATON ||
			mod == MOD_SABER && targ->NPC->stats.quadKnockbackFrom & FLAG_SABER ||
			(mod == MOD_BRYAR_PISTOL || mod == MOD_BRYAR_PISTOL_ALT) && targ->NPC->stats.quadKnockbackFrom & FLAG_PISTOL ||
			mod == MOD_BLASTER && targ->NPC->stats.quadKnockbackFrom & FLAG_E11 ||
			(mod == MOD_DISRUPTOR || mod == MOD_DISRUPTOR_SPLASH || mod == MOD_DISRUPTOR_SNIPER) && targ->NPC->stats.quadKnockbackFrom & FLAG_DISRUPTOR ||
			mod == MOD_BOWCASTER && targ->NPC->stats.quadKnockbackFrom & FLAG_BOWCASTER ||
			(mod == MOD_REPEATER || mod == MOD_REPEATER_ALT || mod == MOD_REPEATER_ALT_SPLASH) && targ->NPC->stats.quadKnockbackFrom & FLAG_REPEATER ||
			(mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) && targ->NPC->stats.quadKnockbackFrom & FLAG_DEMP ||
			(mod == MOD_FLECHETTE || mod == MOD_FLECHETTE_ALT_SPLASH) && targ->NPC->stats.quadKnockbackFrom & FLAG_GOLAN ||
			(mod == MOD_ROCKET || mod == MOD_ROCKET_SPLASH || mod == MOD_ROCKET_HOMING || mod == MOD_ROCKET_HOMING_SPLASH) && targ->NPC->stats.quadKnockbackFrom & FLAG_ROCKET ||
			(mod == MOD_THERMAL || mod == MOD_THERMAL_SPLASH) && targ->NPC->stats.quadKnockbackFrom & FLAG_THERMAL ||
			(mod == MOD_TRIP_MINE_SPLASH || mod == MOD_TIMED_MINE_SPLASH) && targ->NPC->stats.quadKnockbackFrom & FLAG_MINE ||
			mod == MOD_DET_PACK_SPLASH && targ->NPC->stats.quadKnockbackFrom & FLAG_DETPACK ||
			(mod == MOD_CONC || mod == MOD_CONC_ALT) && targ->NPC->stats.quadKnockbackFrom & FLAG_CONC)
		{
			knockback *= 4; //quadruple the knockback
		}
	}

	if (targ->s.eType == ET_NPC && targ->NPC->stats.specialKnockback) { //target in question is an npc and special knockback is set
		if (targ->NPC->stats.specialKnockback == 3 || (attacker && attacker->client && attacker->client->sess.sessionTeam && targ->NPC->stats.specialKnockback == attacker->client->sess.sessionTeam)) //target in question cannot be knockbacked by attacker
		{
			knockback = 0; //no knockback
		}
	}

	// prevent laming the droid on hoth
	if (level.siegeMap == SIEGEMAP_HOTH && targ->s.eType == ET_NPC && g_gametype.integer == GT_SIEGE && !Q_stricmp(targ->targetname, "droidhead"))
		knockback = 0;

	// figure momentum add, even if the damage won't be taken
	if ( knockback && targ->client ) {
		vec3_t	kvel;
		float	mass;

		mass = 200;

		if (mod == MOD_SABER)
		{
			float saberKnockbackScale = g_saberDmgVelocityScale.value;
			if ( (dflags&DAMAGE_SABER_KNOCKBACK1)
				|| (dflags&DAMAGE_SABER_KNOCKBACK2) )
			{//saber does knockback, scale it by the right number
				if ( !saberKnockbackScale )
				{
					saberKnockbackScale = 1.0f;
				}
				if ( attacker
					&& attacker->client )
				{
					if ( (dflags&DAMAGE_SABER_KNOCKBACK1) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[0].knockbackScale;
						}
					}
					if ( (dflags&DAMAGE_SABER_KNOCKBACK1_B2) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[0].knockbackScale2;
						}
					}
					if ( (dflags&DAMAGE_SABER_KNOCKBACK2) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[1].knockbackScale;
						}
					}
					if ( (dflags&DAMAGE_SABER_KNOCKBACK2_B2) )
					{
						if ( attacker && attacker->client )
						{
							saberKnockbackScale *= attacker->client->saber[1].knockbackScale2;
						}
					}
				}
			}
			VectorScale (dir, (g_knockback.value * (float)knockback / mass)*saberKnockbackScale, kvel);
		}
		else
		{
			VectorScale (dir, g_knockback.value * (float)knockback / mass, kvel);
		}
		VectorAdd (targ->client->ps.velocity, kvel, targ->client->ps.velocity);

		if (attacker && attacker->client && attacker != targ)
		{
			float dur = 5000;
			float dur2 = 100;
			if (targ->client && targ->s.eType == ET_NPC && targ->s.NPC_class == CLASS_VEHICLE)
			{
				dur = 25000;
				dur2 = 25000;
			}
			targ->client->ps.otherKiller = attacker->s.number;
			targ->client->ps.otherKillerTime = level.time + dur;
			targ->client->ps.otherKillerDebounceTime = level.time + dur2;
		}
		// set the timer so that the other client can't cancel
		// out the movement immediately
		if ( !targ->client->ps.pm_time && (g_saberDmgVelocityScale.integer || mod != MOD_SABER || (dflags&DAMAGE_SABER_KNOCKBACK1) || (dflags&DAMAGE_SABER_KNOCKBACK2) || (dflags&DAMAGE_SABER_KNOCKBACK1_B2) || (dflags&DAMAGE_SABER_KNOCKBACK2_B2) ) ) {
			int		t;

			t = knockback * 2;
			if ( t < 50 ) {
				t = 50;
			}
			if ( t > 200 ) {
				t = 200;
			}
			targ->client->ps.pm_time = t;
			targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
		}
	}
	else if (targ->client && targ->s.eType == ET_NPC && targ->s.NPC_class == CLASS_VEHICLE && attacker != targ)
	{
		targ->client->ps.otherKiller = attacker->s.number;
		targ->client->ps.otherKillerTime = level.time + 25000;
		targ->client->ps.otherKillerDebounceTime = level.time + 25000;
	}

	if (targ->s.eType == ET_NPC && targ->NPC->stats.nodmgfrom) //examine nodmgfrom flag
	{
		if (targ->NPC->stats.nodmgfrom == -1) //use -1 as a shortcut for invulnerability
		{
			return 0;
		}
		else if (mod == MOD_MELEE && targ->NPC->stats.nodmgfrom & FLAG_MELEE ||
			mod == MOD_STUN_BATON && targ->NPC->stats.nodmgfrom & FLAG_STUNBATON ||
			mod == MOD_SABER && targ->NPC->stats.nodmgfrom & FLAG_SABER ||
			(mod == MOD_BRYAR_PISTOL || mod == MOD_BRYAR_PISTOL_ALT) && targ->NPC->stats.nodmgfrom & FLAG_PISTOL ||
			mod == MOD_BLASTER && targ->NPC->stats.nodmgfrom & FLAG_E11 ||
			(mod == MOD_DISRUPTOR || mod == MOD_DISRUPTOR_SPLASH || mod == MOD_DISRUPTOR_SNIPER) && targ->NPC->stats.nodmgfrom & FLAG_DISRUPTOR ||
			mod == MOD_BOWCASTER && targ->NPC->stats.nodmgfrom & FLAG_BOWCASTER ||
			(mod == MOD_REPEATER || mod == MOD_REPEATER_ALT || mod == MOD_REPEATER_ALT_SPLASH) && targ->NPC->stats.nodmgfrom & FLAG_REPEATER ||
			(mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) && targ->NPC->stats.nodmgfrom & FLAG_DEMP ||
			(mod == MOD_FLECHETTE || mod == MOD_FLECHETTE_ALT_SPLASH) && targ->NPC->stats.nodmgfrom & FLAG_GOLAN ||
			(mod == MOD_ROCKET || mod == MOD_ROCKET_SPLASH || mod == MOD_ROCKET_HOMING || mod == MOD_ROCKET_HOMING_SPLASH) && targ->NPC->stats.nodmgfrom & FLAG_ROCKET ||
			(mod == MOD_THERMAL || mod == MOD_THERMAL_SPLASH) && targ->NPC->stats.nodmgfrom & FLAG_THERMAL ||
			(mod == MOD_TRIP_MINE_SPLASH || mod == MOD_TIMED_MINE_SPLASH) && targ->NPC->stats.nodmgfrom & FLAG_MINE ||
			mod == MOD_DET_PACK_SPLASH && targ->NPC->stats.nodmgfrom & FLAG_DETPACK ||
			(mod == MOD_CONC || mod == MOD_CONC_ALT) && targ->NPC->stats.nodmgfrom & FLAG_CONC ||
			mod == MOD_FORCE_DARK && targ->NPC->stats.nodmgfrom & FLAG_DARKFORCE ||
			mod == MOD_VEHICLE && targ->NPC->stats.nodmgfrom & FLAG_VEHICLE ||
			mod == MOD_FALLING && targ->NPC->stats.nodmgfrom & FLAG_FALLING ||
			mod == MOD_CRUSH && targ->NPC->stats.nodmgfrom & FLAG_CRUSH ||
			mod == MOD_TRIGGER_HURT && targ->NPC->stats.nodmgfrom & FLAG_TRIGGER_HURT ||
			(mod == MOD_UNKNOWN || mod == MOD_TURBLAST || mod == MOD_WATER || mod == MOD_SLIME || mod == MOD_LAVA || mod == MOD_TELEFRAG || mod == MOD_SUICIDE
				|| mod == MOD_TARGET_LASER || mod == MOD_TEAM_CHANGE || mod == MOD_SENTRY) && targ->NPC->stats.nodmgfrom & FLAG_MISC)
		{
			return 0;
		}
	}

	if (dflags & DAMAGE_KNOCKBACK_ONLY)
		return 0;
	
	if ( (g_trueJedi.integer || g_gametype.integer == GT_SIEGE)
		&& client )
	{//less explosive damage for jedi, more saber damage for non-jedi
		if (jediSplashDmgReduction != JEDISPLASHDMGREDUCTION_NO && (jediSplashDmgReduction == JEDISPLASHDMGREDUCTION_YES || client->ps.trueJedi
			|| (g_gametype.integer == GT_SIEGE && client->ps.weapon == WP_SABER)))
		{//if the target is a trueJedi, reduce splash and explosive damage to 1/2
			switch ( mod )
			{
			case MOD_REPEATER_ALT:
			case MOD_REPEATER_ALT_SPLASH:
			case MOD_DEMP2_ALT:
			case MOD_FLECHETTE_ALT_SPLASH:
			case MOD_ROCKET:
			case MOD_ROCKET_SPLASH:
			case MOD_ROCKET_HOMING:
			case MOD_ROCKET_HOMING_SPLASH:
			case MOD_THERMAL:
			case MOD_THERMAL_SPLASH:
			case MOD_TRIP_MINE_SPLASH:
			case MOD_TIMED_MINE_SPLASH:
			case MOD_DET_PACK_SPLASH:
				damage *= 0.75;
				break;
			}
		}
		else if (nonJediSaberDmgIncrease != NONJEDISABERDMGINCREASE_NO && mod == MOD_SABER && !(targ->NPC && targ->NPC->stats.normalSaberDamage) &&
			(nonJediSaberDmgIncrease == NONJEDISABERDMGINCREASE_YES || client->ps.trueNonJedi || (g_gametype.integer == GT_SIEGE && client->ps.weapon != WP_SABER)))
		{//if the target is a trueNonJedi, take more saber damage... combined with the 1.5 in the w_saber stuff, this is 6 times damage!
			if ( damage < 100 )
			{
				damage *= 4;
				if ( damage > 100 )
				{
					damage = 100;
				}
			}
		}
	}

	if (level.intermissiontime || onlyKnockback)
		return 0;

	if (attacker->client && targ->client && g_gametype.integer == GT_SIEGE && !targ->client->sess.siegeDuelInProgress &&
		targ->client->siegeClass != -1 && (bgSiegeClasses[targ->client->siegeClass].classflags & (1<<CFL_STRONGAGAINSTPHYSICAL)))
	{ //this class is flagged to take less damage from physical attacks.
		//For now I'm just decreasing against any client-based attack, this can be changed later I guess.
		damage *= 0.5;
	}

	if (g_gametype.integer == GT_SIEGE && attacker && attacker->client && attacker - g_entities < MAX_CLIENTS &&
		attacker->client->siegeClass != -1 && bgSiegeClasses[attacker->client->siegeClass].saberOffDamageBoost &&
		mod == MOD_SABER && attacker->client->saberBonusTime && level.time - attacker->client->saberBonusTime <= 1500 &&
		targ && targ->client && targ - g_entities < MAX_CLIENTS && targ->s.weapon != WP_SABER) {
		damage = (int)(((float)damage) * 1.5f);
	}

	if (g_gametype.integer == GT_SIEGE && targ && targ->client && targ - g_entities < MAX_CLIENTS &&
		targ->client->siegeClass != -1 && bgSiegeClasses[targ->client->siegeClass].saberOffExtraDamage &&
		targ->s.weapon == WP_SABER && targ->client->ps.saberHolstered) {
		damage += (int)(((float)damage) * bgSiegeClasses[targ->client->siegeClass].saberOffExtraDamage);
	}

	if (!(attacker && attacker->client && targ && targ == attacker && attacker->client->sess.skillBoost)) { // not a skillboosted player attacking himself
		if (attacker && attacker->client && attacker->client->sess.skillBoost && targ) { // attacker has a skillboost
			float damageMultiplier = 0.0f;
			if (mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT) {
				switch (attacker->client->sess.skillBoost) { // increase damage dealt
				case 1:		damageMultiplier = g_skillboost1_dempDamageDealtBonus.value;		break;
				case 2:		damageMultiplier = g_skillboost2_dempDamageDealtBonus.value;		break;
				case 3:		damageMultiplier = g_skillboost3_dempDamageDealtBonus.value;		break;
				case 4:		damageMultiplier = g_skillboost4_dempDamageDealtBonus.value;		break;
				case 5:		damageMultiplier = g_skillboost5_dempDamageDealtBonus.value;		break;
				case 6:		damageMultiplier = g_skillboost6_dempDamageDealtBonus.value;		break;
				case 7:		damageMultiplier = g_skillboost7_dempDamageDealtBonus.value;		break;
				case 8:		damageMultiplier = g_skillboost8_dempDamageDealtBonus.value;		break;
				case 9:		damageMultiplier = g_skillboost9_dempDamageDealtBonus.value;		break;
				case 10:		damageMultiplier = g_skillboost10_dempDamageDealtBonus.value;		break;
				}
			}
			else {
				switch (attacker->client->sess.skillBoost) { // increase damage dealt
				case 1:		damageMultiplier = g_skillboost1_damageDealtBonus.value;			break;
				case 2:		damageMultiplier = g_skillboost2_damageDealtBonus.value;			break;
				case 3:		damageMultiplier = g_skillboost3_damageDealtBonus.value;			break;
				case 4:		damageMultiplier = g_skillboost4_damageDealtBonus.value;			break;
				case 5:		damageMultiplier = g_skillboost5_damageDealtBonus.value;			break;
				case 6:		damageMultiplier = g_skillboost6_damageDealtBonus.value;		break;
				case 7:		damageMultiplier = g_skillboost7_damageDealtBonus.value;		break;
				case 8:		damageMultiplier = g_skillboost8_damageDealtBonus.value;		break;
				case 9:		damageMultiplier = g_skillboost9_damageDealtBonus.value;		break;
				case 10:		damageMultiplier = g_skillboost10_damageDealtBonus.value;		break;
				}
			}
			damage += (int)((float)damage * damageMultiplier);
		}
		if (targ && targ->client && targ->client->sess.skillBoost) { // target has a skillboost
			float damageMultiplier = 0.0f;
			// reduce damage intake
			switch (targ->client->sess.skillBoost) {
			case 1:		damageMultiplier = g_skillboost1_damageTakenReduction.value;		break;
			case 2:		damageMultiplier = g_skillboost2_damageTakenReduction.value;		break;
			case 3:		damageMultiplier = g_skillboost3_damageTakenReduction.value;		break;
			case 4:		damageMultiplier = g_skillboost4_damageTakenReduction.value;		break;
			case 5:		damageMultiplier = g_skillboost5_damageTakenReduction.value;		break;
			case 6:		damageMultiplier = g_skillboost6_damageTakenReduction.value;		break;
			case 7:		damageMultiplier = g_skillboost7_damageTakenReduction.value;		break;
			case 8:		damageMultiplier = g_skillboost8_damageTakenReduction.value;		break;
			case 9:		damageMultiplier = g_skillboost9_damageTakenReduction.value;		break;
			case 10:		damageMultiplier = g_skillboost10_damageTakenReduction.value;		break;
			}
			damage -= (int)((float)damage * damageMultiplier);
		}
	}

	// check for completely getting out of the damage
	if ( !(dflags & DAMAGE_NO_PROTECTION) ) {

		// if TF_NO_FRIENDLY_FIRE is set, don't do damage to the target
		// if the attacker was on the same team
		if ( targ != attacker)
		{
			if (OnSameTeam (targ, attacker))
			{
				if ( !g_friendlyFire.integer && !(negativeDamageOk && damage < 0) )
				{
					return 0;
				}
			}
			else if (targ->inuse && targ->client &&
				g_gametype.integer >= GT_TEAM &&
				attacker->s.number >= MAX_CLIENTS &&
				attacker->alliedTeam &&
				targ->client->sess.sessionTeam == attacker->alliedTeam &&
				!g_friendlyFire.integer)
			{ //things allied with my team should't hurt me.. I guess
				return 0;
			}
		}

		if (g_gametype.integer == GT_JEDIMASTER && !g_friendlyFire.integer &&
			targ && targ->client && attacker && attacker->client &&
			targ != attacker && !targ->client->ps.isJediMaster && !attacker->client->ps.isJediMaster &&
			G_ThereIsAMaster())
		{
			return 0;
		}

		if (targ->s.number >= MAX_CLIENTS && targ->client 
			&& targ->s.shouldtarget && targ->s.teamowner &&
			attacker && attacker->inuse && attacker->client && targ->s.owner >= 0 && targ->s.owner < MAX_CLIENTS)
		{
			gentity_t *targown = &g_entities[targ->s.owner];

			if (targown && targown->inuse && targown->client && OnSameTeam(targown, attacker))
			{
				if (!g_friendlyFire.integer)
				{
					return 0;
				}
			}
		}

		// check for godmode
		if ( (targ->flags & FL_GODMODE) && targ->s.eType != ET_NPC ) {
			return 0;
		}

		if (targ && targ->client && (targ->client->ps.eFlags & EF_INVULNERABLE) &&
			attacker && attacker->client && targ != attacker)
		{
			if (targ->client->invulnerableTimer <= level.time)
			{
				targ->client->ps.eFlags &= ~EF_INVULNERABLE;
			}
			else
			{
				return 0;
			}
		}
	}

	//check for teamnodmg
	//NOTE: non-client objects hitting clients (and clients hitting clients) purposely doesn't obey this teamnodmg (for emplaced guns)
	if (attacker && (!targ->client || targ->s.eType == ET_NPC))
	{//attacker hit a non-client
		if ( g_gametype.integer == GT_SIEGE &&
			!g_ff_objectives.integer )
		{//in siege mode (and...?)
			if ( targ->teamnodmg )
			{//targ shouldn't take damage from a certain team
				if ( attacker->client )
				{//a client hit a non-client object
					if ( targ->teamnodmg == attacker->client->sess.sessionTeam )
					{
						return 0;
					}
				}
				else if ( attacker->teamnodmg )
				{//a non-client hit a non-client object
					//FIXME: maybe check alliedTeam instead?
					if ( targ->teamnodmg == attacker->teamnodmg )
					{
						if (attacker->activator &&
							attacker->activator->inuse &&
							attacker->activator->s.number < MAX_CLIENTS &&
							attacker->activator->client &&
							attacker->activator->client->sess.sessionTeam != targ->teamnodmg)
						{ //uh, let them damage it I guess.
						}
						else
						{
							return 0;
						}
					}
				}
			}
		}
	}

	// battlesuit protects from all radius damage (but takes knockback)
	// and protects 50% against all damage
	if ( client && client->ps.powerups[PW_BATTLESUIT] && !client->sess.siegeDuelInProgress) {
		G_AddEvent( targ, EV_POWERUP_BATTLESUIT, 0 );
		if ( ( dflags & DAMAGE_RADIUS ) || ( mod == MOD_FALLING ) ) {
			return 0;
		}
		damage *= 0.5;
	}

	// add to the attacker's hit counter (if the target isn't a general entity like a prox mine)
	if ( attacker->client && targ != attacker && targ->health > 0
			&& targ->s.eType != ET_MISSILE
			&& targ->s.eType != ET_GENERAL
			&& client) {
		if ( OnSameTeam( targ, attacker ) ) {
			attacker->client->ps.persistant[PERS_HITS]--;
		} else {
			attacker->client->ps.persistant[PERS_HITS]++;
		}
		attacker->client->ps.persistant[PERS_ATTACKEE_ARMOR] = (targ->health<<8)|(client->ps.stats[STAT_ARMOR]);
	}

	// always give half damage if hurting self... but not in siege.  Heavy weapons need a counter.
	// calculated after knockback, so rocket jumping works
	if ( targ == attacker && !(dflags & DAMAGE_NO_SELF_PROTECTION)) {
		if ( g_gametype.integer == GT_SIEGE )
		{
			if (targ->client && targ->client->sess.skillBoost) { // reduce damage intake
				switch (attacker->client->sess.skillBoost) {
				case 1:		damage *= g_skillboost1_selfDamageFactorOverride.value;		break;
				case 2:		damage *= g_skillboost2_selfDamageFactorOverride.value;		break;
				case 3:		damage *= g_skillboost3_selfDamageFactorOverride.value;		break;
				case 4:		damage *= g_skillboost4_selfDamageFactorOverride.value;		break;
				case 5:		damage *= g_skillboost5_selfDamageFactorOverride.value;		break;
				case 6:		damage *= g_skillboost6_selfDamageFactorOverride.value;		break;
				case 7:		damage *= g_skillboost7_selfDamageFactorOverride.value;		break;
				case 8:		damage *= g_skillboost8_selfDamageFactorOverride.value;		break;
				case 9:		damage *= g_skillboost9_selfDamageFactorOverride.value;		break;
				case 10:		damage *= g_skillboost10_selfDamageFactorOverride.value;		break;
				}
			}
			else {
				damage *= 1.5;
			}
		}
		else
		{
			damage *= 0.5;
		}
	}

	if (negativeDamageOk && damage < 0) {
	}
	else if ( damage < 1 ) {
		damage = 1;
	}
	take = damage;

	// save some from armor
	if (take > 0)
		asave = CheckArmor(targ, take, dflags);
	else
		asave = 0;

#ifdef _DEBUG
	Com_Printf("%d damaged by %d with mod %d for %d (original damage: %d)\n", targ ? targ - g_entities : -1, attacker ? attacker - g_entities : -1, mod, take, originalDamage);
#endif

	if (asave)
	{
		shieldAbsorbed = asave;
	}

	take -= asave;
	if ( targ->client )
	{//update vehicle shields and armor, check for explode 
		if ( targ->client->NPC_class == CLASS_VEHICLE &&
			targ->m_pVehicle )
		{//FIXME: should be in its own function in g_vehicles.c now, too big to be here
			int surface = -1;
			if ( attacker )
			{//so we know the last guy who shot at us
				targ->enemy = attacker;
			}

			if ( ( targ->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL ) )
			{
			}

			targ->m_pVehicle->m_iShields = targ->client->ps.stats[STAT_ARMOR];
			G_VehUpdateShields( targ );
			targ->m_pVehicle->m_iArmor -= take;
			if ( targ->m_pVehicle->m_iArmor <= 0 ) 
			{
				targ->s.eFlags |= EF_DEAD;
				targ->client->ps.eFlags |= EF_DEAD;
				targ->m_pVehicle->m_iArmor = 0;
			}
			if ( targ->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER )
			{//get the last surf that was hit
				if ( targ->client && targ->client->g2LastSurfaceTime == level.time)
				{
					char hitSurface[MAX_QPATH];

					trap_G2API_GetSurfaceName(targ->ghoul2, targ->client->g2LastSurfaceHit, 0, hitSurface);

					if (hitSurface[0])
					{
						surface = G_ShipSurfaceForSurfName( &hitSurface[0] );

						if ( take && surface > 0 )
						{//hit a certain part of the ship
							int deathPoint = 0;

							targ->locationDamage[surface] += take;

							switch(surface)
							{
							case SHIPSURF_FRONT:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_front;
								break;
							case SHIPSURF_BACK:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_back;
								break;
							case SHIPSURF_RIGHT:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_right;
								break;
							case SHIPSURF_LEFT:
								deathPoint = targ->m_pVehicle->m_pVehicleInfo->health_left;
								break;
							default:
								break;
							}

							//presume 0 means it wasn't set and so it should never die.
							if ( deathPoint )
							{
								if ( targ->locationDamage[surface] >= deathPoint)
								{ //this area of the ship is now dead
									if ( G_FlyVehicleDestroySurface( targ, surface ) )
									{//actually took off a surface
										G_VehicleSetDamageLocFlags( targ, surface, deathPoint );
									}
								}
								else
								{
									G_VehicleSetDamageLocFlags( targ, surface, deathPoint );
								}
							}
						}
					}
				}
			}
			if ( targ->m_pVehicle->m_pVehicleInfo->type != VH_ANIMAL )
			{
				if ( attacker 
						//&& attacker->client 
						&& targ != attacker
						&& point 
						&& !VectorCompare( targ->client->ps.origin, point )
						&& targ->m_pVehicle->m_LandTrace.fraction >= 1.0f)
				{//just took a hit, knock us around
					vec3_t	vUp, impactDir;
					float	impactStrength = (damage/200.0f)*10.0f;
					float	dot = 0.0f;
					if ( impactStrength > 10.0f )
					{
						impactStrength = 10.0f;
					}
					//pitch or roll us based on where we were hit
					AngleVectors( targ->m_pVehicle->m_vOrientation, NULL, NULL, vUp );
					VectorSubtract( point, targ->r.currentOrigin, impactDir );
					VectorNormalize( impactDir );
					if ( surface <= 0 )
					{//no surf guess where we were hit, then
						vec3_t	vFwd, vRight;
						AngleVectors( targ->m_pVehicle->m_vOrientation, vFwd, vRight, vUp );
						dot = DotProduct( vRight, impactDir );
						if ( dot > 0.4f )
						{
							surface = SHIPSURF_RIGHT;
						}
						else if ( dot < -0.4f )
						{
							surface = SHIPSURF_LEFT;
						}
						else
						{
							dot = DotProduct( vFwd, impactDir );
							if ( dot > 0.0f )
							{
								surface = SHIPSURF_FRONT;
							}
							else
							{
								surface = SHIPSURF_BACK;
							}
						}
					}
					switch ( surface )
					{
					case SHIPSURF_FRONT:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[PITCH] += impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[PITCH] -= impactStrength;
						}
						break;
					case SHIPSURF_BACK:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[PITCH] -= impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[PITCH] += impactStrength;
						}
						break;
					case SHIPSURF_RIGHT:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[ROLL] -= impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[ROLL] += impactStrength;
						}
						break;
					case SHIPSURF_LEFT:
						dot = DotProduct( vUp, impactDir );
						if ( dot > 0 )
						{
							targ->m_pVehicle->m_vOrientation[ROLL] += impactStrength;
						}
						else
						{
							targ->m_pVehicle->m_vOrientation[ROLL] -= impactStrength;
						}
						break;
					}

				}
			}
		}
	}

	if ( mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT )
	{//FIXME: screw with non-animal vehicles, too?
		if ( client )
		{
			if ( client->NPC_class == CLASS_VEHICLE 
				&& targ->m_pVehicle
				&& targ->m_pVehicle->m_pVehicleInfo
				&& targ->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER )
			{//all damage goes into the disruption of shields and systems
				take = 0;
			}
			else
			{

				if (client->jetPackOn && !(g_gametype.integer == GT_SIEGE && client->siegeClass != -1 && bgSiegeClasses[client->siegeClass].jetpackFreezeImmunity))
				{ //disable jetpack temporarily
					Jetpack_Off(targ);
					client->jetPackToggleTime = level.time + Q_irand(3000, 10000);
				}

				if ( client->NPC_class == CLASS_PROTOCOL || client->NPC_class == CLASS_SEEKER ||
					client->NPC_class == CLASS_R2D2 || client->NPC_class == CLASS_R5D2 ||
					client->NPC_class == CLASS_MOUSE || client->NPC_class == CLASS_GONK )
				{
					// DEMP2 does more damage to these guys.
					take *= 2;
				}
				else if ( client->NPC_class == CLASS_PROBE || client->NPC_class == CLASS_INTERROGATOR ||
							client->NPC_class == CLASS_MARK1 || client->NPC_class == CLASS_MARK2 || client->NPC_class == CLASS_SENTRY ||
							client->NPC_class == CLASS_ATST )
				{
					// DEMP2 does way more damage to these guys.
					take *= 5;
				}
				else
				{
					if (take > 0)
					{
						take /= 3;
						if (take < 1)
						{
							take = 1;
						}
					}
				}
			}
		}
	}

	// siege stats
	int adjustedTake = Com_Clampi(take + asave, targ->health + asave, take + asave); // so damage dealt doesn't exceed the target's maximum health (e.g. rocketing someone with 1 hp == 1 damage)
	if (attacker && attacker->client && targ) {
		// hoth
		if (level.siegeMap == SIEGEMAP_HOTH) {
			// atst
			if (attacker->client->sess.sessionTeam == TEAM_BLUE && targ->s.NPC_class == CLASS_VEHICLE) {
				if (!targ->atstKilled) { // don't credit if atst is already on fire
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_HOTH_ATSTDMG] += adjustedTake;
					if (targ->health - adjustedTake <= 0) {
						targ->atstKilled = qtrue;
						attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_HOTH_ATSTKILL]++;
					}
				}
			}
			// shield gen + cc
			if (attacker->client->sess.sessionTeam == TEAM_RED && VALIDSTRING(targ->paintarget)) {
				if (!Q_stricmp(targ->paintarget, "shieldgen_underattack"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_HOTH_GENDMG] += adjustedTake;
				else if (!Q_stricmp(targ->paintarget, "comcentercounter_attack"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_HOTH_CCDMG] += adjustedTake;
			}
		}
		// desert
		else if (level.siegeMap == SIEGEMAP_DESERT && attacker->client->sess.sessionTeam == TEAM_RED) {
			if (VALIDSTRING(targ->paintarget)) {
				if (!Q_stricmp(targ->paintarget, "wallattack"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_WALLDMG] += adjustedTake;
				else if (!Q_stricmp(targ->paintarget, "rancorgateattack"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_GATEDMG] += adjustedTake;
			}
			else if (VALIDSTRING(targ->target4)) {
				if (!Q_stricmp(targ->target4, "b1counter"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_STATION1DMG] += adjustedTake;
				else if (!Q_stricmp(targ->target4, "b2counter"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_STATION2DMG] += adjustedTake;
				else if (!Q_stricmp(targ->target4, "b3counter"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_STATION3DMG] += adjustedTake;
			}
			else if (targ->spawnflags == 96 && targ->teamnodmg == TEAM_BLUE && !VALIDSTRING(targ->healingclass))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_DESERT_GATEDMG] += adjustedTake;
		}
		// korriban
		else if (level.siegeMap == SIEGEMAP_KORRIBAN) {
			if (attacker->client->sess.sessionTeam == TEAM_RED) {
				if (VALIDSTRING(targ->paintarget)) {
					if (!Q_stricmp(targ->paintarget, "firstgateattack"))
						attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_TEMPLEGATEDMG] += adjustedTake;
					else if (!Q_stricmp(targ->paintarget, "scepterrrom_attack_print")) // raven @ spelling bee champions
						attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_SCEPTERGATEDMG] += adjustedTake;
					else if (!Q_stricmp(targ->paintarget, "altarroom_attack_print"))
						attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_ALTARGATEDMG] += adjustedTake;
				}
				else if (VALIDSTRING(targ->targetname) && !Q_stricmp(targ->targetname, "ragnos_coffin"))
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_KORRI_COFFINDMG] += adjustedTake;
			}
		}
		// nar shaddaa
		else if (level.siegeMap == SIEGEMAP_NAR && attacker->client->sess.sessionTeam == TEAM_RED && VALIDSTRING(targ->paintarget)) {
			if (!Q_stricmp(targ->paintarget, "rstation1attacked"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_NAR_STATION1DMG] += adjustedTake;
			else if (!Q_stricmp(targ->paintarget, "rstation2attacked"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_NAR_STATION2DMG] += adjustedTake;
		}
		// cargo2
		else if (level.siegeMap == SIEGEMAP_CARGO && attacker->client->sess.sessionTeam == TEAM_RED && VALIDSTRING(targ->paintarget)) {
			if (!Q_stricmp(targ->paintarget, "obj2_under_a_tack"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_CARGO2_ARRAYDMG] += adjustedTake;
			else if (!Q_stricmp(targ->paintarget, "powernode1attack"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_CARGO2_NODE1DMG] += adjustedTake;
			else if (!Q_stricmp(targ->paintarget, "powernode2attack"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_CARGO2_NODE2DMG] += adjustedTake;
		}
		// bespin
		else if (level.siegeMap == SIEGEMAP_BESPIN && attacker->client->sess.sessionTeam == TEAM_RED && VALIDSTRING(targ->target)) {
			if (!Q_stricmp(targ->target, "UnlockDoor"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_BESPIN_LOCKDMG] += adjustedTake;
			else if (!Q_stricmpn(targ->target, "breakpanel", 10))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_BESPIN_PANELDMG] += adjustedTake;
			else if (!Q_stricmp(targ->target, "obj3"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_BESPIN_GENDMG] += adjustedTake;
			else if (!Q_stricmp(targ->target, "obj6"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_BESPIN_PODDMG] += adjustedTake;
		}
		else if (level.siegeMap == SIEGEMAP_URBAN && attacker->client->sess.sessionTeam == TEAM_RED) {
			if (VALIDSTRING(targ->target) && !Q_stricmp(targ->target, "spawnblueguy"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_URBAN_BLUEDMG] += adjustedTake;
			else if (VALIDSTRING(targ->target) && !Q_stricmp(targ->target, "spawnredguy"))
				attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_URBAN_REDDMG] += adjustedTake;
			else if (targ->s.eType == ET_NPC && VALIDSTRING(targ->NPC_type) && strlen(targ->NPC_type) >= 10 && tolower(*targ->NPC_type) == 'w') {
				if (tolower(*(targ->NPC_type + 7)) == 'b')
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_URBAN_BLUEDMG] += adjustedTake;
				else if (tolower(*(targ->NPC_type + 7)) == 'r')
					attacker->client->sess.siegeStats.mapSpecific[GetSiegeStatRound()][SIEGEMAPSTAT_URBAN_REDDMG] += adjustedTake;
			}
		}
	}

	//we count only from client to client damage
	if (attacker && attacker->client && targ && targ->client && !targ->m_pVehicle /* duo: don't count damage against vehicles */
		&& attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam
		&& mod > MOD_UNKNOWN && mod <= MOD_FORCE_DARK) {
		// TODO: do we want other kinds of damage?
		// TODO: it does not count rage or protect reduction...
		targ->client->pers.damageTaken += adjustedTake;
		attacker->client->pers.damageCaused += adjustedTake;
		if (g_gametype.integer == GT_SIEGE) {
			if (attacker->client->sess.sessionTeam == TEAM_RED && targ->client->sess.sessionTeam == TEAM_BLUE && !attacker->NPC && !targ->NPC) {
				targ->client->sess.siegeStats.dDamageTaken[GetSiegeStatRound()] += adjustedTake;
				attacker->client->sess.siegeStats.oDamageDealt[GetSiegeStatRound()] += adjustedTake;
			}
			else if (attacker->client->sess.sessionTeam == TEAM_BLUE && targ->client->sess.sessionTeam == TEAM_RED && !attacker->NPC && !targ->NPC) {
				targ->client->sess.siegeStats.oDamageTaken[GetSiegeStatRound()] += adjustedTake;
				attacker->client->sess.siegeStats.dDamageDealt[GetSiegeStatRound()] += adjustedTake;
			}
		}
	}

#if 0//#ifndef FINAL_BUILD
	if ( g_debugDamage.integer ) {
		G_Printf( "%i: client:%i health:%i damage:%i armor:%i\n", level.time, targ->s.number,
			targ->health, take, asave );
	}
#endif

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if ( client ) {
		if ( attacker ) {
			client->ps.persistant[PERS_ATTACKER] = attacker->s.number;
		} else {
			client->ps.persistant[PERS_ATTACKER] = ENTITYNUM_WORLD;
		}
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;
		if ( dir ) {
			VectorCopy ( dir, client->damage_from );
			client->damage_fromWorld = qfalse;
		} else {
			VectorCopy ( targ->r.currentOrigin, client->damage_from );
			client->damage_fromWorld = qtrue;
		}

		if (attacker && attacker->client)
		{
			BotDamageNotification(client, attacker);
		}
		else if (inflictor && inflictor->client)
		{
			BotDamageNotification(client, inflictor);
		}
	}

	// See if it's the player hurting the emeny flag carrier
	if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY) {
		Team_CheckHurtCarrier(targ, attacker);
	}

	if (targ->client) {
		// set the last client who damaged the target
		targ->client->lasthurt_client = attacker->s.number;
		targ->client->lasthurt_mod = mod;
	}

	if ( !(dflags & DAMAGE_NO_PROTECTION) ) 
	{//protect overridden by no_protection
		if (take && targ->client && (targ->client->ps.fd.forcePowersActive & (1 << FP_PROTECT)))
		{
			if (targ->client->ps.fd.forcePower)
			{
				int maxtake = take;

				if (targ->client->forcePowerSoundDebounce < level.time)
				{
					G_PreDefSound(targ->client->ps.origin, PDSOUND_PROTECTHIT);
					targ->client->forcePowerSoundDebounce = level.time + 400;
				}

				if (targ->client->ps.fd.forcePowerLevel[FP_PROTECT] == FORCE_LEVEL_1)
				{
					famt = 1;
					hamt = 0.40;

					if (maxtake > 100)
					{
						maxtake = 100;
					}
				}
				else if (targ->client->ps.fd.forcePowerLevel[FP_PROTECT] == FORCE_LEVEL_2)
				{
					famt = 0.5;
					hamt = 0.60;

					if (maxtake > 200)
					{
						maxtake = 200;
					}
				}
				else if (targ->client->ps.fd.forcePowerLevel[FP_PROTECT] == FORCE_LEVEL_3)
				{
					famt = 0.25;
					hamt = 0.80;

					if (maxtake > 400)
					{
						maxtake = 400;
					}
				}

					targ->client->ps.fd.forcePower -= maxtake*famt;

				subamt = (maxtake*hamt)+(take-maxtake);
				if (targ->client->ps.fd.forcePower < 0)
				{
					subamt += targ->client->ps.fd.forcePower;
					targ->client->ps.fd.forcePower = 0;
				}
				if (subamt)
				{
					// protect saved you from this much damage
					if ( attacker && attacker->client && targ && targ->client
						&& attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam
						&& mod > MOD_UNKNOWN && mod <= MOD_FORCE_DARK ) {
						// TODO: do we want other kinds of damage?
						targ->client->pers.protDmgAvoided += subamt;
					}

					take -= subamt;

					if (take < 0)
					{
						take = 0;
					}
				}
			}
		}
	}

	if (shieldAbsorbed)
	{
		{
			gentity_t	*evEnt;

			// Send off an event to show a shield shell on the player, pointing in the right direction.
			//rww - er.. what the? This isn't broadcast, why is it being set on vec3_origin?!
			evEnt = G_TempEntity(targ->r.currentOrigin, EV_SHIELD_HIT);
			evEnt->s.otherEntityNum = targ->s.number;
			evEnt->s.eventParm = DirToByte(dir);
			evEnt->s.time2=shieldAbsorbed;
		}
	}

	if (level.zombies && targ && targ->client && targ->client->sess.sessionTeam == TEAM_BLUE && attacker && attacker->client && targ != attacker && !g_friendlyFire.integer &&
		mod >= MOD_BRYAR_PISTOL && mod <= MOD_CONC_ALT && mod != MOD_TURBLAST && mod != MOD_VEHICLE)
	{
		//prevent team-switch from allowing projectiles to kill (former) teammates on blue team in zombies mode
		take = 0;
	}

	// do the damage
	if (take) 
	{
		if (targ - g_entities < MAX_CLIENTS && targ->client && targ->health > 0 &&
			targ->client->ps.pm_flags & PMF_STUCK_TO_WALL &&
			g_gametype.integer == GT_SIEGE && targ->client->siegeClass != -1 && bgSiegeClasses[targ->client->siegeClass].classflags & (1 << CFL_SPIDERMAN)) {
			targ->client->pushOffWallTime = level.time;
		}

		if (targ->client && targ->s.number < MAX_CLIENTS && freeze != FREEZE_NO && (freeze ==  FREEZE_YES || mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT))
		{ //uh.. shock them or something. what the hell, I don't know.
            if (targ->client->ps.weaponTime <= 0)
			{ //yeah, we were supposed to be beta a week ago, I don't feel like
				//breaking the game so I'm gonna be safe and only do this only
				//if your weapon is not busy
				if (freeze == FREEZE_YES && overrideFreezeTimeActual != -1) {
					targ->client->ps.weaponTime = overrideFreezeTimeActual;
					targ->client->ps.electrifyTime = level.time + overrideFreezeTimeActual;
				}
				else {
					targ->client->ps.weaponTime = 2000;
					targ->client->ps.electrifyTime = level.time + 2000;
				}
				if (g_gametype.integer == GT_SIEGE && attacker && attacker->client && attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam)
				{
					if (freeze == FREEZE_YES && overrideFreezeTimeActual != -1)
						attacker->client->pers.teamState.frozeTime = level.time + overrideFreezeTimeActual;
					else
						attacker->client->pers.teamState.frozeTime = level.time + 2000;
					attacker->client->pers.teamState.frozeClient = targ->client->ps.clientNum;
				}
				if (targ->client->ps.weaponstate == WEAPON_CHARGING ||
					targ->client->ps.weaponstate == WEAPON_CHARGING_ALT)
				{
					targ->client->ps.weaponstate = WEAPON_READY;
				}
			}
		}

		if ( !(dflags & DAMAGE_NO_PROTECTION) ) 
		{//rage overridden by no_protection
			if (targ->client && (targ->client->ps.fd.forcePowersActive & (1 << FP_RAGE)) && (inflictor->client || attacker->client))
			{
				int oldtake = take;
				take /= (targ->client->ps.fd.forcePowerLevel[FP_RAGE]+1);
			}
		}
		targ->health = targ->health - take;
		
		// check that we didn't put them over their max hp
		if (negativeDamageOk && take < 0) {
			if (targ->client && targ - g_entities < MAX_CLIENTS && targ->client->siegeClass != -1) {
				if (targ->health > bgSiegeClasses[targ->client->siegeClass].maxhealth)
					targ->health = bgSiegeClasses[targ->client->siegeClass].maxhealth;
			}
			else {
				if (targ->maxHealth && targ->health > targ->maxHealth)
					targ->health = targ->maxHealth;
			}
		}

		if ( (targ->flags&FL_UNDYING) )
		{//take damage down to 1, but never die
			if ( targ->health < 1 )
			{
				targ->health = 1;
			}
		}

		if ( targ->client ) {
			targ->client->ps.stats[STAT_HEALTH] = targ->health;
		}

		if ( !(dflags & DAMAGE_NO_PROTECTION) ) 
		{//rage overridden by no_protection
			if (targ->client && (targ->client->ps.fd.forcePowersActive & (1 << FP_RAGE)) && (inflictor->client || attacker->client))
			{
				if (targ->health <= 0)
				{
					targ->health = 1;
				}
				if (targ->client->ps.stats[STAT_HEALTH] <= 0)
				{
					targ->client->ps.stats[STAT_HEALTH] = 1;
				}
			}
		}

		if (targ->health > 0 && (targ->health < targ->lowestHealth || !targ->lowestHealth))
			targ->lowestHealth = targ->health;

		//We want to go ahead and set gPainHitLoc regardless of if we have a pain func,
		//so we can adjust the location damage too.
		if (targ->client && targ->ghoul2 && targ->client->g2LastSurfaceTime == level.time)
		{ //We updated the hit surface this frame, so it's valid.
			char hitSurface[MAX_QPATH];

			trap_G2API_GetSurfaceName(targ->ghoul2, targ->client->g2LastSurfaceHit, 0, hitSurface);

			if (hitSurface[0])
			{
				G_GetHitLocFromSurfName(targ, hitSurface, &gPainHitLoc, point, dir, vec3_origin, mod);
			}
			else
			{
				gPainHitLoc = -1;
			}

			if (gPainHitLoc < HL_MAX && gPainHitLoc >= 0 && targ->locationDamage[gPainHitLoc] < Q3_INFINITE &&
				(targ->s.eType == ET_PLAYER || targ->s.NPC_class != CLASS_VEHICLE))
			{
				targ->locationDamage[gPainHitLoc] += take;

				if (g_armBreakage.integer && !targ->client->ps.brokenLimbs &&
					targ->client->ps.stats[STAT_HEALTH] > 0 && targ->health > 0 &&
					!(targ->s.eFlags & EF_DEAD))
				{ //check for breakage
					if (targ->locationDamage[HL_ARM_RT]+targ->locationDamage[HL_HAND_RT] >= 80)
					{
						G_BreakArm(targ, BROKENLIMB_RARM);
					}
					else if (targ->locationDamage[HL_ARM_LT]+targ->locationDamage[HL_HAND_LT] >= 80)
					{
						G_BreakArm(targ, BROKENLIMB_LARM);
					}
				}
			}
		}
		else
		{
			gPainHitLoc = -1;
		}

		if (targ->maxHealth)
		{ //if this is non-zero this guy should be updated his s.health to send to the client
			G_ScaleNetHealth(targ);
		}

		if ( targ->health <= 0 ) {
			if ( client )
			{
				targ->flags |= FL_NO_KNOCKBACK;

				if (point)
				{
					VectorCopy( point, targ->pos1 );
				}
				else
				{
					VectorCopy(targ->client->ps.origin, targ->pos1);
				}
			}
			else if (targ->s.eType == ET_NPC)
			{ //g2animent
				VectorCopy(point, targ->pos1);
			}

			if (targ->health < -999)
				targ->health = -999;

			// If we are a breaking glass brush, store the damage point so we can do cool things with it.
			if ( targ->r.svFlags & SVF_GLASS_BRUSH )
			{
				VectorCopy( point, targ->pos1 );
				if (dir)
				{
					VectorCopy( dir, targ->pos2 );
				}
				else
				{
					VectorClear(targ->pos2);
				}
			}

			if (targ->s.eType == ET_NPC &&
				targ->client &&
				(targ->s.eFlags & EF_DEAD))
			{ //an NPC that's already dead. Maybe we can cut some more limbs off!
				if ( (mod == MOD_SABER || (mod == MOD_MELEE && G_HeavyMelee( attacker )) )//saber or heavy melee (claws)
					&& take > 2
					&& !(dflags&DAMAGE_NO_DISMEMBER) )
				{
					G_CheckForDismemberment(targ, attacker, targ->pos1, take, targ->client->ps.torsoAnim, qtrue);
				}
			}

			targ->enemy = attacker;
			targ->die (targ, inflictor, attacker, take, mod);
			G_ActivateBehavior( targ, BSET_DEATH );
			return take;
		} 
		else 
		{
			if ( pm->debugMelee || (targ - g_entities < MAX_CLIENTS && targ->client &&
				g_gametype.integer == GT_SIEGE && targ->client->siegeClass != -1 &&
				bgSiegeClasses[targ->client->siegeClass].classflags & (1 << CFL_SPIDERMAN)))
			{//getting hurt makes you let go of the wall
				if ( targ->client && (targ->client->ps.pm_flags&PMF_STUCK_TO_WALL) )
				{
					G_LetGoOfWall( targ );
				}
			}
			if ( targ->pain ) 
			{
				if (targ->s.eType != ET_NPC || mod != MOD_SABER || take > 1)
				{ //don't even notify NPCs of pain if it's just idle saber damage
					gPainMOD = mod;
					if (point)
					{
						VectorCopy(point, gPainPoint);
					}
					else
					{
						VectorCopy(targ->r.currentOrigin, gPainPoint);
					}
					targ->pain (targ, attacker, take);
				}
			}
		}

		G_LogWeaponDamage(attacker->s.number, mod, take);
	}
	return take;
}


/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
qboolean CanDamage (gentity_t *targ, vec3_t origin) {
	vec3_t	dest;
	trace_t	tr;
	vec3_t	midpoint;

	// use the midpoint of the bounds instead of the origin, because
	// bmodels may have their origin is 0,0,0
	VectorAdd (targ->r.absmin, targ->r.absmax, midpoint);
	VectorScale (midpoint, 0.5, midpoint);

	VectorCopy (midpoint, dest);
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (tr.fraction == 1.0 || tr.entityNum == targ->s.number)
		return qtrue;

	// this should probably check in the plane of projection, 
	// rather than in world coordinate, and also include Z
	VectorCopy (midpoint, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (tr.fraction == 1.0)
		return qtrue;


	return qfalse;
}


/*
============
G_RadiusDamage
============
*/
qboolean G_RadiusDamage ( vec3_t origin, gentity_t *attacker, float damage, float radius,
					 gentity_t *ignore, gentity_t *missile, int mod) {
	float		points, dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;
	qboolean	hitClient = qfalse;
	qboolean	roastPeople = qfalse;

	if ( radius < 1 ) {
		radius = 1;
	}

	if (attacker && attacker->inuse && attacker->client) {
		gentity_t *maybeBoosted;
		if (attacker->s.eType == ET_NPC && attacker->s.NPC_class == CLASS_VEHICLE && attacker->m_pVehicle) {
			if (attacker->m_pVehicle->m_pPilot)
				maybeBoosted = (gentity_t *)attacker->m_pVehicle->m_pPilot;
			else if (attacker->lastPilot && attacker->lastPilot - g_entities < MAX_CLIENTS)
				maybeBoosted = attacker->lastPilot;
			else
				maybeBoosted = attacker;
		}
		else {
			maybeBoosted = attacker;
		}

		if (mod != MOD_TRIP_MINE_SPLASH && mod != MOD_TIMED_MINE_SPLASH && maybeBoosted && maybeBoosted - g_entities < MAX_CLIENTS && maybeBoosted->client && maybeBoosted->client->sess.skillBoost) {
			switch (maybeBoosted->client->sess.skillBoost) {
			case 1:		radius += (radius * g_skillboost1_splashRadiusBonus.value);		break;
			case 2:		radius += (radius * g_skillboost2_splashRadiusBonus.value);		break;
			case 3:		radius += (radius * g_skillboost3_splashRadiusBonus.value);		break;
			case 4:		radius += (radius * g_skillboost4_splashRadiusBonus.value);		break;
			case 5:		radius += (radius * g_skillboost5_splashRadiusBonus.value);		break;
			case 6:		radius += (radius * g_skillboost6_splashRadiusBonus.value);		break;
			case 7:		radius += (radius * g_skillboost7_splashRadiusBonus.value);		break;
			case 8:		radius += (radius * g_skillboost8_splashRadiusBonus.value);		break;
			case 9:		radius += (radius * g_skillboost9_splashRadiusBonus.value);		break;
			case 10:		radius += (radius * g_skillboost10_splashRadiusBonus.value);		break;
			}
		}
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;
		if (ent && ent->client && ent->client->sess.siegeDuelInProgress)
			continue; //prevent anyone in siege duel from taking splash damage. since we are using pistol only, this shouldn't be a problem.

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

		if(ent->health <= 0)
			continue;

		points = damage * ( 1.0 - dist / radius );

		if( CanDamage (ent, origin) ) {
			if( LogAccuracyHit( ent, attacker ) ) {
				hitClient = qtrue;
			}
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;
			if (attacker && attacker->inuse && attacker->client &&
				attacker->s.eType == ET_NPC && attacker->s.NPC_class == CLASS_VEHICLE &&
				attacker->m_pVehicle && attacker->m_pVehicle->m_pPilot)
			{ //say my pilot did it.
				if (g_locationBasedDamage_splash.integer)
					G_Damage (ent, NULL, (gentity_t *)attacker->m_pVehicle->m_pPilot, dir, origin, (int)points, DAMAGE_RADIUS, mod);
				else
					G_Damage (ent, NULL, (gentity_t *)attacker->m_pVehicle->m_pPilot, dir, origin, (int)points, DAMAGE_RADIUS | DAMAGE_NO_HIT_LOC, mod);
			}
			else if (attacker && attacker->inuse && attacker->client &&
				attacker->s.eType == ET_NPC && attacker->s.NPC_class == CLASS_VEHICLE &&
				attacker->m_pVehicle && attacker->lastPilot && attacker->lastPilot - g_entities < MAX_CLIENTS)
			{ //say my last pilot did it.
				G_Damage(ent, NULL, attacker->lastPilot, dir, origin, (int)points, DAMAGE_RADIUS, mod);
			}
			else
			{
				if (g_locationBasedDamage_splash.integer)
					G_Damage (ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS, mod);
				else
					G_Damage (ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS | DAMAGE_NO_HIT_LOC, mod);
			}

			if (ent && ent->client && roastPeople && missile &&
				!VectorCompare(ent->r.currentOrigin, missile->r.currentOrigin))
			{ //the thing calling this function can create burn marks on people, so create an event to do so
				gentity_t *evEnt = G_TempEntity(ent->r.currentOrigin, EV_GHOUL2_MARK);

				evEnt->s.otherEntityNum = ent->s.number; //the entity the mark should be placed on
				evEnt->s.weapon = WP_ROCKET_LAUNCHER; //always say it's rocket so we make the right mark

				//Try to place the decal by going from the missile location to the location of the person that was hit
				VectorCopy(missile->r.currentOrigin, evEnt->s.origin);
				VectorCopy(ent->r.currentOrigin, evEnt->s.origin2);

				//it's hacky, but we want to move it up so it's more likely to hit
				//the torso.
				if (missile->r.currentOrigin[2] < ent->r.currentOrigin[2])
				{ //move it up less so the decal is placed lower on the model then
					evEnt->s.origin2[2] += 8;
				}
				else
				{
					evEnt->s.origin2[2] += 24;
				}

				//Special col check
				evEnt->s.eventParm = 1;
			}
		}
	}

	return hitClient;
}

qboolean G_RadiusDamageKnockbackOnly(vec3_t origin, gentity_t *attacker, float damage, float radius,
	gentity_t *ignore, gentity_t *missile, int mod) {
	float		points, dist;
	gentity_t *ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;
	qboolean	hitClient = qfalse;
	qboolean	roastPeople = qfalse;

	if (radius < 1) {
		radius = 1;
	}

	for (i = 0; i < 3; i++) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for (e = 0; e < numListedEntities; e++) {
		ent = &g_entities[entityList[e]];

		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;

		// find the distance from the edge of the bounding box
		for (i = 0; i < 3; i++) {
			if (origin[i] < ent->r.absmin[i]) {
				v[i] = ent->r.absmin[i] - origin[i];
			}
			else if (origin[i] > ent->r.absmax[i]) {
				v[i] = origin[i] - ent->r.absmax[i];
			}
			else {
				v[i] = 0;
			}
		}

		dist = VectorLength(v);
		if (dist >= radius) {
			continue;
		}

		if (ent->health <= 0)
			continue;

		points = damage * (1.0 - dist / radius);

		if (CanDamage(ent, origin)) {
			if (attacker && LogAccuracyHit(ent, attacker)) {
				hitClient = qtrue;
			}
			VectorSubtract(ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;
			if (attacker && attacker->inuse && attacker->client &&
				attacker->s.eType == ET_NPC && attacker->s.NPC_class == CLASS_VEHICLE &&
				attacker->m_pVehicle && attacker->m_pVehicle->m_pPilot)
			{ //say my pilot did it.
				if (g_locationBasedDamage_splash.integer)
					G_Damage(ent, NULL, (gentity_t *)attacker->m_pVehicle->m_pPilot, dir, origin, (int)points, DAMAGE_RADIUS | DAMAGE_KNOCKBACK_ONLY, mod);
				else
					G_Damage(ent, NULL, (gentity_t *)attacker->m_pVehicle->m_pPilot, dir, origin, (int)points, DAMAGE_RADIUS | DAMAGE_NO_HIT_LOC | DAMAGE_KNOCKBACK_ONLY, mod);
			}
			else
			{
				if (g_locationBasedDamage_splash.integer)
					G_Damage(ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS | DAMAGE_KNOCKBACK_ONLY, mod);
				else
					G_Damage(ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS | DAMAGE_NO_HIT_LOC | DAMAGE_KNOCKBACK_ONLY, mod);
			}

			if (ent && ent->client && roastPeople && missile &&
				!VectorCompare(ent->r.currentOrigin, missile->r.currentOrigin))
			{ //the thing calling this function can create burn marks on people, so create an event to do so
				gentity_t *evEnt = G_TempEntity(ent->r.currentOrigin, EV_GHOUL2_MARK);

				evEnt->s.otherEntityNum = ent->s.number; //the entity the mark should be placed on
				evEnt->s.weapon = WP_ROCKET_LAUNCHER; //always say it's rocket so we make the right mark

				//Try to place the decal by going from the missile location to the location of the person that was hit
				VectorCopy(missile->r.currentOrigin, evEnt->s.origin);
				VectorCopy(ent->r.currentOrigin, evEnt->s.origin2);

				//it's hacky, but we want to move it up so it's more likely to hit
				//the torso.
				if (missile->r.currentOrigin[2] < ent->r.currentOrigin[2])
				{ //move it up less so the decal is placed lower on the model then
					evEnt->s.origin2[2] += 8;
				}
				else
				{
					evEnt->s.origin2[2] += 24;
				}

				//Special col check
				evEnt->s.eventParm = 1;
			}
		}
	}

	return hitClient;
}
