#include "g_local.h"
#include "bg_saga.h"

// the converter's next give time for this activator and its G_TickDueSince stamp
static int *G_ConverterTime(gentity_t *self, gentity_t *activator, int **lastEval)
{
	if (g_multiUseGenerators.integer && activator - g_entities < MAX_CLIENTS)
	{
		*lastEval = &self->perPlayerLastEval[activator - g_entities];
		return &self->perPlayerTime[activator - g_entities];
	}
	*lastEval = &self->tickLastEval;
	return &self->setTime;
}

/*
ticks a converter owes on this call. with the fix a call between ticks owes none and the caller returns at once,
as when only the use key reached it
*/
static int G_ConverterDue(int *timePtr, int *lastEval, int period)
{
	if (TICKFIX(Stations))
		return G_TickDueSince(timePtr, lastEval, period);
	return *timePtr < level.time ? 1 : 0;
}

/*
================
health_power_converter_use
================
*/
void health_power_converter_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int dif, add;
	int stop = 1;

	if (!activator || !activator->client)
	{
		return;
	}

	if (activator->client->sess.siegeDuelInProgress)
	{
		return; //no getting health while in a siege duel
	}

	if (level.zombies)
	{
		return;
	}

	if (g_gametype.integer >= GT_TEAM && self->healingteam && self->healingteam != activator->client->sess.sessionTeam)
		return;

	int *lastEval;
	int *timePtr = G_ConverterTime(self, activator, &lastEval);
	int n = G_ConverterDue(timePtr, lastEval, G_TickUseStrict(self->genericValue5));

	if (!n && TICKFIX(Stations))
	{
		return;
	}

	for (; n > 0; n--)
	{
		if (!self->s.loopSound)
		{
			self->s.loopSound = G_SoundIndex("sound/player/pickuphealth.wav");
		}
		if (!TICKFIX(Stations))
		{
			*timePtr = level.time + self->genericValue5; // duo: changed to use chargerate instead of hardcoded 100
		}

		dif = activator->client->ps.stats[STAT_MAX_HEALTH] - activator->health;

		if (dif > 0)					// Already at full armor?
		{
			if (dif > self->genericValue17) // 5
			{
				add = self->genericValue17; // 5
			}
			else
			{
				add = dif;
			}

			if (self->count < add)
			{
				add = self->count;
			}

			//self->count -= add;
			stop = 0;

			self->fly_sound_debounce_time = level.time + 500;
			self->activator = activator;

			activator->health += add;
			TICKPROBE("station_health", activator->s.number, add);
		}
	}

	if (stop)
	{
		// duo: added sound
		if (self->s.loopSound && *timePtr < level.time)
		{
			if (self->count <= 0)
			{
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/ammocon_empty"));
			}
			else
			{
				G_Sound(self, CHAN_AUTO, self->genericValue7);
			}
		}
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
	}
}

/*
================
shield_power_converter_use
================
*/
void shield_power_converter_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int dif, add;
	int stop = 1;

	if (!activator || !activator->client)
	{
		return;
	}

	if (activator->client->sess.siegeDuelInProgress)
	{
		return; //no getting armor while in a siege duel
	}

	int *lastEval;
	int *timePtr = G_ConverterTime(self, activator, &lastEval);
	int n = 0;

	// before the armor check, so its empty sound is per tick too
	if (TICKFIX(Stations))
	{
		n = G_ConverterDue(timePtr, lastEval, G_TickUseStrict(self->genericValue5));
		if (!n)
		{
			return;
		}
	}

	if (g_gametype.integer == GT_SIEGE
		&& other
		&& other->client
		&& other->client->siegeClass)
	{
		if (!bgSiegeClasses[other->client->siegeClass].maxarmor)
		{//can't use it!
			G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/shieldcon_empty"));
			return;
		}
	}

	if (!TICKFIX(Stations))
	{
		n = G_ConverterDue(timePtr, lastEval, self->genericValue5);
	}

	for (; n > 0; n--)
	{
		int	maxArmor;
		if (!self->s.loopSound)
		{
			self->s.loopSound = G_SoundIndex("sound/interface/shieldcon_run");
			self->s.loopIsSoundset = qfalse;
		}
		if (!TICKFIX(Stations))
		{
			*timePtr = level.time + self->genericValue5; // duo: changed to use chargerate instead of hardcoded 100
		}

		if (g_gametype.integer == GT_SIEGE
			&& other
			&& other->client
			&& other->client->siegeClass != -1)
		{
			maxArmor = bgSiegeClasses[other->client->siegeClass].maxarmor;
		}
		else
		{
			maxArmor = activator->client->ps.stats[STAT_MAX_HEALTH];
		}
		dif = maxArmor - activator->client->ps.stats[STAT_ARMOR];

		if (dif > 0)					// Already at full armor?
		{
			if (dif > self->genericValue17) // 2
			{
				add = self->genericValue17; // 2
			}
			else
			{
				add = dif;
			}

			if (self->count < add)
			{
				add = self->count;
			}

			if (!self->genericValue12)
			{
				self->count -= add;
			}
			stop = 0;

			self->fly_sound_debounce_time = level.time + 500;
			self->activator = activator;

			activator->client->ps.stats[STAT_ARMOR] += add;
			TICKPROBE("station_shield", activator->s.number, add);
			if (activator->client->ps.stats[STAT_ARMOR] > 0)
				activator->client->immuneFromDamageUntil = 0; // refresh armor tanking

			if (self->count <= 0)
			{
				*timePtr = 0;
				break;
			}
		}
	}

	if (stop || self->count <= 0)
	{
		if (self->s.loopSound && *timePtr < level.time)
		{
			if (self->count <= 0)
			{
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/shieldcon_empty"));
			}
			else
			{
				G_Sound(self, CHAN_AUTO, self->genericValue7);
			}
		}
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
		if (*timePtr < level.time)
		{
			*timePtr = level.time + (TICKFIX(Stations) ? G_TickUseStrict(self->genericValue5 + 100) : self->genericValue5 + 100);
		}
	}
}

void ammo_generic_power_converter_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int add;
	int stop = 1;

	if (!activator || !activator->client)
	{
		return;
	}

	int myAmmoTypes = g_gametype.integer == GT_SIEGE ? TypesOfAmmoPlayerHasGunsFor(activator) : -1;

	int *lastEval;
	int *timePtr = G_ConverterTime(self, activator, &lastEval);
	// the stock path gives on every call while giving; only the use key set its rate
	int n = G_ConverterDue(timePtr, lastEval, G_TickStrict(100));

	if (!n && TICKFIX(Stations))
	{
		return;
	}

	for (; n > 0; n--)
	{
		qboolean gaveSome = qfalse;
		int i = AMMO_BLASTER;
		if (!self->s.loopSound)
		{
			self->s.loopSound = G_SoundIndex("sound/interface/ammocon_run");
			self->s.loopIsSoundset = qfalse;
		}
		self->fly_sound_debounce_time = level.time + 500;
		self->activator = activator;
		while (i < AMMO_MAX)
		{
			if (!(myAmmoTypes & (1 << i))) { // duo: only give ammo for ammo types that we have weapons for
				i++;
				continue;
			}
			int tickAmmoBefore = activator->client->ps.ammo[i];

			add = ammoData[i].max * 0.05;
			if (add < 1)
			{
				add = 1;
			}

			if (g_gametype.integer == GT_SIEGE && i == AMMO_ROCKETS && (bgSiegeClasses[activator->client->siegeClass].classflags & (1 << CFL_SINGLE_ROCKET)) && (bgSiegeClasses[activator->client->siegeClass].classflags & (1 << CFL_EXTRA_AMMO)))
			{
				if (activator->client->ps.ammo[i] < 2)
				{
					if (Add_Ammo(activator, i, add) > 0)
					{
						gaveSome = qtrue;
						stop = 0;
					}
				}
			}
			else if (g_gametype.integer == GT_SIEGE && i == AMMO_ROCKETS && (bgSiegeClasses[activator->client->siegeClass].classflags & (1 << CFL_SINGLE_ROCKET)))
			{
				if (activator->client->ps.ammo[i] < 1)
				{
					activator->client->ps.ammo[i] = 1;
				}
			}
			else if (g_gametype.integer == GT_SIEGE && i == AMMO_ROCKETS && bgSiegeClasses[activator->client->siegeClass].ammorockets && (bgSiegeClasses[activator->client->siegeClass].classflags & (1 << CFL_EXTRA_AMMO)))
			{
				if (activator->client->ps.ammo[i] < (bgSiegeClasses[activator->client->siegeClass].ammorockets * 2))
				{
					if (Add_Ammo(activator, i, add) > 0)
					{
						gaveSome = qtrue;
						stop = 0;
					}
				}
			}
			else if (g_gametype.integer == GT_SIEGE && i == AMMO_ROCKETS && bgSiegeClasses[activator->client->siegeClass].ammorockets)
			{
				if (activator->client->ps.ammo[i] < bgSiegeClasses[activator->client->siegeClass].ammorockets)
				{
					if (Add_Ammo(activator, i, add) > 0)
					{
						gaveSome = qtrue;
						stop = 0;
					}
				}
			}
			else if (g_gametype.integer == GT_SIEGE && i == AMMO_ROCKETS && (bgSiegeClasses[activator->client->siegeClass].classflags & (1 << CFL_EXTRA_AMMO)))
			{
				if (activator->client->ps.ammo[i] < 20)
				{
					if (Add_Ammo(activator, i, add) > 0)
					{
						gaveSome = qtrue;
						stop = 0;
					}
				}
			}
			else if (g_gametype.integer == GT_SIEGE && i == AMMO_ROCKETS)
			{
				if (activator->client->ps.ammo[i] < 10)
				{
					if (Add_Ammo(activator, i, add) > 0)
					{
						gaveSome = qtrue;
						stop = 0;
					}
				}
			}
			else
			{
				if (Add_Ammo(activator, i, add) > 0)
				{
					gaveSome = qtrue;
					stop = 0;
				}
			}

			if (g_tickProbe.integer && activator->client->ps.ammo[i] > tickAmmoBefore) {
				if (self->classname && !Q_stricmp(self->classname, "misc_ammo_floor_unit"))
					TICKPROBE("station_ammo_generic", activator->s.number, activator->client->ps.ammo[i] - tickAmmoBefore);
				else
					TICKPROBE("station_ammo", activator->s.number, activator->client->ps.ammo[i] - tickAmmoBefore);
			}
			i++;
			if (!self->genericValue12 && gaveSome)
			{
				int sub = (add * 0.2);
				if (sub < 1)
				{
					sub = 1;
				}
				self->count -= sub;
				if (self->count <= 0)
				{
					self->count = 0;
					stop = 1;
					break;
				}
			}
		}
		if (self->count <= 0)
		{
			break;
		}
	}

	if (stop || self->count <= 0)
	{
		// with the fix the time holds the next tick, and this is only reached on a tick
		if (self->s.loopSound && (TICKFIX(Stations) || *timePtr < level.time))
		{
			if (self->count <= 0)
			{
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/interface/ammocon_empty"));
			}
			else
			{
				G_Sound(self, CHAN_AUTO, self->genericValue7);
			}
		}
		self->s.loopSound = 0;
		self->s.loopIsSoundset = qfalse;
		if (TICKFIX(Stations))
		{
			*timePtr = level.time + G_TickUseStrict(self->genericValue5 + 100);
		}
		else if (*timePtr < level.time)
		{
			*timePtr = level.time + self->genericValue5 + 100;
		}
	}
}