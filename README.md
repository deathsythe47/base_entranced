# base_entranced

a multiplayer server mod for Jedi Knight: Jedi Academy's Siege gametype

by Duo

a fork of Sil's old [base_enhanced](https://github.com/TheSil/base_enhanced) CTF server mod

# About base_entranced

base_entranced is intended mainly for the Siege gametype, although it can be played with any gametype. It is intended for **classic, competitive Siege gameplay**, not as a general-purpose "hangout server" mod.

base_entranced is the official server mod of the siege community [(www.jasiege.com)](https://www.jasiege.com). Due to its large amount of bugfixes and enhancements, playing siege on any other mod is extremely buggy and frustrating, not to mention that most actively played siege maps now *require* base_entranced's enhanced mapping framework to function properly.

base_entranced has three goals:
* Fixing bugs.
* Adding enhancements to basejka siege gameplay and server administration.
* Providing an enhanced mapping framework that allows for many more possilibities for siege mappers.

base_entranced remains close to base JA siege gameplay. It is a simple improvement of base JA siege, with the same classic gameplay in mind. The only huge change to gameplay is the anti-spam cvars. Doorspam, minespam, and shieldspam are massive fundamental flaws in the game design, and needed to be addressed. For years (until this mod), the stupidly overpowered nature of this spam was avoided by BS "honor systems" and "gentleman's agreements" that should never be a thing in a PVP game. These agreements were violated regularly, whether by accident or not. Even violating one of these agreements a single time (for example, dropping an extra shield) could and would completely alter the outcome of a match. Being able to completely break the game by spamming was absolutely essential to fix. With these fixes, players can use every possible tool at their disposal to impede enemies, just like any normal PVP game. Apart from these spam fixes, everything else generally adheres to the "stay-close-to-basejka" philosophy.

You can discuss base_entranced on the **base_entranced forum** at [www.jasiege.com](https://www.jasiege.com)

base_entranced includes additional support for [Newmod](https://jkanewmod.github.io), which is the recommended client mod for Siege.

base_entranced is icensed under GPLv2 as free software. You are free to use, modify and redistribute base_entranced following the terms in LICENSE.txt.

# Notice to server admins

base_entranced is intended to be fully usable "out of the box." Most cvars default to their ideal/recommended setting for a pug server, as far as the Siege community is concerned. You shouldn't really need to change anything to get your server running, although the cvars are nevertheless there for you to change if you would like to customize your server to your liking.

**You MUST use the following dedicated server executable for this mod to work** https://github.com/avygeil/openjk

# base_entranced features
These are unique features for base_entranced.

#### `g_autoStats`
0 = no stats (base JKA)

1 = (default) automatically prints stats at the end of each round, including objective times, kills, deaths, damage, etc. Some maps also have their own unique stats for that map. Clients can also access these stats manually by using the `stats` command.

#### `g_classLimits`
0 = no limits (base JKA)

1 = (default) enable class limits + automatic overrides from map/server mod

2 = enable class limits + DO NOT use automatic overrides from map/server mod

If `g_classLimits` is enabled, you can use twelve cvars to limit the number of people who can play a particular class. For example, `dAssaultLimit 1` limits the number of defense assaults to one, `oJediLimit 2` limits offense to two jedis, etc. With `g_classLimits 1`, some automatic overrides for community maps will be applied from bsp files and from hardcoded limits in the server mod itself.

#### `g_delayClassUpdate`
0 = instantly broadcast class changes

1 = (default) instantly broadcast class changes to teammates; delay class change broadcast for enemies until after the respawn. This is a huge improvement to gameplay. Prevents the stupid "oh you changed class 150ms before the respawn? Let me counter you by changing class 140ms before the respawn" garbage. It also allows for more coordination between teammates, since they can now press their class change binds early, and change to synergize with each other well in advance of the spawn.

#### `g_siegeTiebreakEnd`
0 = no tiebreaking rules (base JKA)

1 = (default) enforce traditional siege community tiebreaking rule. If round 1 offense got held for maximum time(20 minutes on most maps), game will end once round 2 offense has completed one more objective. Round 2 offense will be declared the winner of the match.

#### `g_fixSiegeScoring`
0 = dumb base JKA scoring (20 pts per obj, 30 pts for final obj, 10 pt bonus at end)

1 = (default) improved/logical scoring (100 pts per obj)

#### `g_fixVoiceChat`
0 = enemies can hear your voice chats and see icon over your head (base JKA)

1 = (default) only teammates can hear your voice chats and see icon over your head (except for spot_air and spot_emplaced, which are used to BM your enemies)

#### `iLikeToDoorSpam`
0 = (default) door spam prohibited for blobs, golan balls, rockets, conc primaries, thermals, and bowcaster alternates within a limited distance of enemies in your FOV. Wait until door opens to fire (skilled players already do this). Does not apply if a walker, shield, or someone using protect or mindtrick is nearby. Warning: turning on this setting will cause terrible players to complain.

1 = door spam allowed, have fun immediately getting hit to 13hp because some shitty was raining blobs/golans/etc on the door before you entered (base JKA)

#### `iLikeToMineSpam`
0 = (default) mine spam prohibited within a limited distance of enemies in your FOV. No throwing mines at incoming enemies anymore (skilled players already refrain from this). Does not apply if a walker, shield, or someone using protect or mindtrick is nearby. Warning: turning on this setting will cause terrible players to complain.

1 = mine spam allowed, have fun insta-dying because some shitty was holding down mouse2 with mines before you entered the door (bonus points for "was planting, bro" excuse) (base JKA)

#### `iLikeToShieldSpam`
0 = (default) shield spam prohibited; you have to be killed by an enemy or walker explosion to place a new shield. You can place a new shield at each objective, with one freebie ("Yo shield") during the 20 seconds immediately after an objective.

1 = shield spam allowed, have fun getting shield spammed

#### `g_autoKorribanSpam`
0 = spam-related cvars are unaffected by map

1 = (default) `iLikeToDoorSpam` and `iLikeToMineSpam` automatically get set to 1 for Korriban (where spam is essential), and automatically get set to 0 for all other maps

#### `g_siegeGhosting`
0 = enable idiotic siege ghosting mechanic where the informational advantage is held by the team with more dead people (???)

1 = disable ghosting; when you are dead you can click to cycle through following your teammates as if you were a spec, similar to literally any other video game. has some visual issues unless you use a compatible client mod that fixes them

2 = (default) same as 1, but if nobody on the team is alive then you view some useless spot on the map.

#### Toptimes records and speedruns
The server database tracks per-objective and per-map statistics on fastest offense times, and longest defense times. For more information see https://forums.jasiege.com/phpBB3/viewtopic.php?f=23&t=215

#### Live pug detection
The server automatically detects when a pug (2v2 or greater) is live. It notifies you at the beginning if there's an issue (e.g. AFK player), and will automatically pause if someone 999s for several seconds (`g_autoPause999`, default 3)

#### Skillboost/senseboost/aimboost
You can give bad players a boost to help them be competitive. `skillboost <player> <number 0 through 5>` gives them a boost to their stats, while `senseboost <player> <number 0 through 5>` gives them an intermittent wallhack to improve their game sense and `aimboost <player> <number 0 through 5>` allows their near-misses with disruptor to be changed to hits. Stats affected by skillboost include damage output, incoming damage, force regen time, movement speed, self damage factor, and splash radius. Clients can use the `skillboost`, `senseboost`, and `aimboost` commands to see who is currently boosted. Generally, levels 1-3 should be used most of the time; 4-5 are more extreme boosts and should only be used for people who have absolutely no idea what they are doing. Some parameters of boosts can be changed with cvars beginning with `g_skillboost` and `g_senseboost`; use `cvarlist g_skillboost` or `cvarlist g_senseboost` to see all of them. Using these boosts judiciously and conservatively is recommended; sometimes all that's needed is just giving a single person a single level one boost.

#### `g_emotes`
0 = disable

1 = (default) clients can use custom animations with the `emote` command. This is just a fun way to BM when the enemies are all dead and you are going to selfkill. It makes you auto-selfkill for the next spawn, and prevents you from dealing damage until you die. Therefore, you can't use it to cheese animations and hide inside the floor or something.

#### `g_fixSaberDefense`
0 = base JA saber blocking

1 = old fixed saber defense, for more information see https://forums.jasiege.com/phpBB3/viewtopic.php?f=18&t=133

2 = (default) newer fixed saber defense, e.g. prevents bullshit staff blocks on projectiles shot into your back from behind

#### `g_fixGolanDamage`
0 = bugged base JA golan damage (see discord thread)

1 = fixed golan damage (default)

#### `g_locationBasedDamage`
0 = no location-based damage

1 = (old base JA default) base JA location-based damage

2 = saber always hits for 1.00 damage multiplier

3 = (default) saber always hits for 1.00 damage multiplier, hands/arms count as chest, and feet count as legs

#### `g_fixShield`
0 = bugged basejka shield behavior; shields placed along the x-axis of a map have a 25% taller hitbox than shields placed along the y-axis (even though they do not visually display as 25% taller in unpatched client mods)

1 = (default) fix all shields; all shields are the correct height

2 = break all shields; all shields are 25% taller

#### `g_flechetteSpread`
0 = use base JA golan primary spread system, where one pellet is dead center and the other four pellets are *completely* random, e.g. they could all be way off in one corner.

1 = (default) improved golan primary spread pattern, with one pellet dead center and each of the other four pellets guaranteed to fire somewhere into each quadrant (±x, ±y).

#### `g_improvedDisarm`
0 = disable

1 = you only get disarmed for 2 seconds, and can regenerate force while disarmed. note that if using the saber throw vs. saberist fixes (see `g_damageFixes` bitflag 4 below) you won't really be getting disarmed anyway unless you do it to yourself intentionally.

#### `g_fixDodge`
0 = dumb base behavior; attempt to dodge disrupts/snipes before attempting to saber block

1 = (default) attempt to saber block disrupts/snipes before attempting to dodge

#### `g_fixDempSaberThrow`
0 = disable

1 = (default) fix base jka bug where demp CC would never decrease during saber throw (e.g. throw -> demped -> hold mouse2 for 9 years -> saber returns -> still must wait 2 seconds to unfreeze)

#### `g_fixHothBunkerLift`
0 = normal lift behavior for Hoth codes bunker lift (base JKA)

1 = (default) Hoth codes bunker lift requires pressing `+use` button (prevents you from killing yourself way too easily on this dumb lift)

#### `g_hothHangarHack`
0 = no changes

1 = (default) Hoth hangar hack time reduced to 5 seconds

2 = any class can do hangar hack

-1 = both

#### `g_fixHothDoorSounds`
0 = Hoth bunker doors at first objective are silent (bug from base JKA)

1 = (default) Hoth bunker doors at first objective use standard door sounds

#### `g_fixHothHangarTurrets`
0 = hoth hangar turrets can shoot at people in the lift shaft, absolutely idiotic mechanic and basically up to RNG whether you successfully dodge the shots even with constant zigzagging

1 = (default) hoth turrets do not shoot at people in the lift shaft

#### `g_removeHothHangarTurrets`
1 = (default) remove hoth hangar turrets entirely

#### `g_antiHothCodesLiftLame`
0 = normal behavior for Hoth codes delivery bunker lift

1 = (default) defenders cannot call up Hoth codes delivery bunker lift if a non-Jedi codes carrier is inside the bunker

#### `g_antiHothHangarLiftLame`
0 = normal behavior for Hoth hangar lift (base JKA)

1 = defense tech uses a 2 second hack to call up the lift. Returns to normal behavior after the hangar objective is completed.

2 = any player on defense is prevented from calling up the lift if any player on offense is nearby ("nearby" is defined as between the boxes in the middle of the hangar and the lift). Returns to normal behavior after the hangar objective is completed.

3 = use both 1 and 2 methods.

4 = (default) use both 1 and 2 methods, plus, after the infirmary has been breached, only allow the defense to call the lift up once within 15 seconds of the infirmary breach. (default setting)

#### `g_antiHothInfirmaryLiftLame`
0 = normal behavior for Hoth infirmary "short" lift (base JKA)

1 = (default) defense cannot call the "short" lift up with the top button; they must use the lower button

#### `g_fixLiftkillTraps`
0 = disable

1 = (default) sentries/shields placed at the top of lifts cannot crush people who come up the lift; instead they are destroyed

#### `g_antiLaming`
0 = (default) no anti-laming provisions (base JKA, suggested setting for pug servers)

1 = laming codes/crystals/scepters/parts, objective skipping, and killing stations with swoops @ desert 1st obj is punished by automatically being killed.

2 = laming codes/crystals/scepters/parts, objective skipping, and killing stations with swoops @ desert 1st obj is punished by automatically being kicked from the server.

#### `g_autoKorribanFloatingItems`
0 = `g_floatingItems` is unaffected by map change

1 = (default) `g_floatingItems` automatically gets set to 1 for Korriban, and automatically gets set to 0 for all other maps

#### `g_nextmapWarning`
0 = no warning (base JKA)

1 = (default) when nextmap vote is called in round 2, a warning message appears (so you don't accidentally reset the timer going up when starting round 2)

#### `g_improvedTeamchat`
0 = base JKA team chat

1 = show selected class as "location" during countdown, show "(DEAD)" in teamchat for dead players, hide location from teamchat between spectators, hide location from teamchat during intermission

2 = (default) all of the above, plus show HP in teamchat for alive players

#### `g_fixFallingSounds`
0 = base JKA sound (normal death sound)

1 = (default) use falling death scream sound when damaged by a `trigger_hurt` entity for >= 100 damage (i.e., death pits). Also plays scream sound if selfkilling while affected by `g_fixPitKills`

#### `g_fixEweb`
0 = base JKA eweb behavior (huge annoying recoil, etc)

1 = (default) remove eweb recoil, remove "unfolding" animation when pulling out eweb, make eweb crosshair start closer to normal crosshair

#### `g_enableCloak`
0 = (default) remove cloak from all siege classes (eliminates need for no-cloak PK3 patches)

1 = cloak enabled (base JKA)

#### `g_fixRancorCharge`
0 = (default) base JKA behavior - rancor can charge/jump through `BLOCKNPC` areas (e.g. desert arena door)

1 = rancor cannot charge/jump through `BLOCKNPC` areas

#### `g_friendlyFreeze`
0 = (default) can't freeze allied vehicles with demp; can't give allies useless electrocution effect

1 = enable (base JKA)

#### `g_damageFixes`
bitflag 1 = removes randomness BS with saber throw damage on gunners; saber throw now deals a consistent 100 damage on enemy gunners. Also implements a consistent system for thrown saber travel -- sabers to always continue traveling through people they kill and get stopped from additional travel by people they fail to kill.

bitflag 2 = removed rocket HP except for enemy tenloss and demp altfire

bitflag 4 = new system for saberthrow damage on enemy saberists, which removes randomness BS and takes into account facing angles, saber holstered/ignited, push/pull, etc. in a consistent way. Also disables saber throw disarm, which was random BS as well.

-1 = (default) enable all

0 = disable (base JKA)

#### `g_infiniteCharge`
0 = no infinite charging bug ("bugfix" from Sil's base_enhanced)

1 = infinite charging bug enabled (classic behavior, brought back by popular demand. hold `+useforce` or `+button2` to hold weapon charge indefinitely)

2 = (default) infinite charge bug enabled, but you don't have to hold down any extra buttons.

#### `g_fixGripKills`
0 = normal selfkilling while gripped (base JKA)

1 = (default) selfkilling while gripped counts as a kill for the gripper. This prevents people from denying enemies' kills with selfkill (similar to `g_fixPitKills` from base_enhanced)

#### `g_antiCallvoteTakeover`
0 = normal vote calling for `map`, `g_gametype`, `pug`, `pub`, `kick`, `clientkick`, and `lockteams` votes (base JKA)

1 = (default) calling a vote for `map`, `g_gametype`, `pug`, `pub`, `kick`, `clientkick`, or `lockteams` when 6+ players are connected requires at least 2+ people to be ingame. This prevents a lone player calling lame unpopular votes when most of the server is in spec unable to vote no.

#### `g_moreTaunts`
0 = base JKA behavior (only allow `taunt` in non-duel gametypes)

1 = (default) enable `gloat`, `flourish`, and `bow` in non-duel gametypes)

#### `g_botJumping`
0 = (default) bots jump around like crazy on maps without botroute support (base JKA)

1 = bots stay on the ground on maps without botroute support

#### `g_botDefaultSiegeClass`
scout = (default) bots join as scout instead of assault

#### `g_botAimbot`
0 = disable

1 = (default) bots aimbot people with disruptor

#### `g_swoopKillPoints`
The number of points you gain from killing swoops (1 = base JKA). Set to 0 (the default) so you don't gain points from farming swoops.

#### `g_sexyDisruptor`
0 = (default) lethal sniper shots with full charge (1.5 seconds or more) cause incineration effect (fixed base JKA setting, which was bugged)

1 = all lethal sniper shots cause incineration effect (this is just for fun/cool visuals and makes it like singeplayer)

2 = same as 1 but also for primary fire

#### `siege_restart`
rcon command that restarts the current map with siege timer going up from 00:00. Before this, there was no server command to reset siege to round 1, the only way was `callvote nextmap` (lol)

#### `forceround2 mm:ss`
Restarts current map with siege timer going down from a specified time. For example, `forceround2 7:30` starts siege in round 2 with the timer going down from 7:30. Can be executed from rcon or callvote.

#### `killturrets`
Removes all turrets from the map. Useful for capt duels. Can be executed from rcon or callvote.

#### `greenDoors`
"Greens" (unlocks) all doors on the map. For testing purposes only; should not be used in live games.

#### `autocfg_map`
0 = (default) no automatic cfg execution (base JKA)

1 = server will automatically execute `mapcfgs/mapname.cfg` at the beginning of any siege round according to whatever the current map is. For example, if you change to `mp/siege_desert`, the server will automatically execute `mapcfgs/mp/siege_desert.cfg` (if it exists). This should eliminate the need for map-specific cvars like `g_autoKorribanFloatingItems`, etc.

#### `autocfg_unknown`
0 = (default) if `autocfg_map` is enabled, but the server is unable to find `mapcfgs/mapname.cfg`, nothing will happen.

1 = if `autocfg_map` is enabled, but the server is unable to find `mapcfgs/mapname.cfg`, the server will instead execute `mapcfgs/unknown.cfg` as a fallback (if it exists).

#### `g_defaultMap`
If specified, the server will change to this map after 60 seconds of being empty.

#### `g_hothRebalance`
0 = hoth classes are unchanged

bitflag 1 = o assault gets big bacta

bitflag 2 = o hw gets regular bacta and e11

bitflag 4 = d jedi gets heal 3

-1 = enable all

default setting: 3 (combination of 1 and 2)

#### `g_fixHoth2ndObj`
0 = disable.

1 = (default) the base JA incarnation of Hoth's second objective Hoth is quite problematic. it's a "feast or famine" scenario where you either rush it and capture in under 20 seconds, or fail the rush and get held quite easily for a very long time (inverted bell curve). enabling this cvar implements the following changes, which make rushing *more* difficult, but normal completion of the obj *less* difficult (bell curve)

* The defense can now teleport to the second obj from the first obj bunkers by pressing +use from anywhere inside either bunker.
* Buffed the cave route by adding a health generator and streamlining movement (lobotomized the Wampa, moved him away from the wall, removed the inner rocks, and reduced the HP of the outer rocks by 93% (to 100 from 1500)).
* Buffed the ion route by changing the hack to be instantaneous and performable by any class.
* Reduced the HP of the two big turrets by 50% (to 600 from 1200).
* Added a health generator just up the ramp from the ion hack door.

#### `g_antiSelfMax`
0 = disable

1 = you cannot selfkill within the one second after a spawn (prevents accidental SK)

#### `g_improvedHoming`
0 = disable

1 = (default) enable improved homing for rocket launchers (fixes weird behaviors, false positive locks etc)

#### `g_siegeRespawnAutoChange`
0 = disable (useful for pub servers)

1 = (default) `g_siegeRespawn` automatically changes between 1 and 20 depending on if the map was restarted (via vote or rcon)

#### `g_preventJoiningLargerTeam`
0 = (default) disable

1 = prevent people from joining the team that has more people than the other team (useful for pub servers)

#### `g_autoRestartAfterIntermission`
0 = (default) disable

15 = (example) automatically do a 15-second rs for each round, similar to the old UJ server mod (useful for pub servers)

#### `g_speedrunRoundOneRestart`
0 = disable (useful for pub servers)

1 = (default) automatically restart in round 1 without team swapping if the map is completed in speedrun mode

#### `g_joinMenuHack`
0 = disable

1 = (default) allows peasants on outdated client mods to use the join menu with modified classes on base maps

#### `g_multiUseGenerators`
0 = base JKA weird buggy behavior when multiple people are using an armor/ammo/health generator

1 = (default) allow multiple people to use armor/ammo/health generators at once without weird buggy behavior

#### `g_dispenserLifetime`
20 = base JKA 20 seconds

25 = (default) ammo canisters last for 5 seconds longer than base JKA; makes sense since spawn times are 20 seconds so they don't disappear right when you get to them

#### `g_techAmmoForAllWeapons`
0 = disable (base JKA)

1 = (default) tech refills ammo for all weapons simultaneously instead of just whatever weapon you are holding at the primary fire rate of the weapon you are holding

#### `g_healWalkerWithAmmoCans`
0 = (default) disable

1 = enable weird base JKA behavior where you can heal the walker with ammo cans (leads to the walker "stealing" ammo cans)

#### `g_autoStart`
0 = disable (base JKA)

1 = (default) automatically restart when everyone has used the `ready` command and is not AFK. configurable parameters for this include `g_autoStartTimer`, `g_autoStartAFKThreshold`, and `g_autoStartMinPlayers`

#### `g_unlagged`
0 = disable

1 = (default) enable unlagged; anyone can use it with the `unlagged` command even if they don't have a supporting client mod

#### `g_tickFix`
0 = disable the per-area tickrate fixes and stop tracking capture records at `sv_fps` other than 30

1 = (default) master switch for the tickrate fixes, which make gameplay timing independent of `sv_fps`. Each area also has its own switch, all default 1 except `g_tickFixSaberDamage` (default 2: 0 stock, 1 per-window damage, 2 sweeps on the 50 ms grid) and `g_tickFixSaberSample` (default 2: 0 chord, 1 parabola, 2 re-timed read), and only effective while `g_tickFix` is on (`g_tickFixStats` is reserved): `g_tickFixInput`, `g_tickFixForce`, `g_tickFixSabers`, `g_tickFixSaberDamage`, `g_tickFixSaberAnim`, `g_tickFixSaberBox`, `g_tickFixSaberSample`, `g_tickFixSaberOrder`, `g_tickFixWeapons`, `g_tickFixStations`, `g_tickFixItems`, `g_tickFixClient`, `g_tickFixMovers`, `g_tickFixNPC`, `g_tickFixStats`, `g_tickFixSiege`. While `g_tickFix` is on, capture records are also tracked at `sv_fps` other than 30.

#### `g_tickFixSaberAnim`
0 = a saber wielder's server-side skeleton switches to a new animation on the first server frame after the change

1 = (default) with `g_tickFixSaberDamage 2`, a saber wielder's skeleton shows, at the 50 ms grid times the blade sweeps use, the animation phase an `sv_fps 20` server's skeleton showed (a changed animation is applied for the 20 Hz frame that would have applied it, with its start time corrected for the engine evaluating skeletons one frame late), so swings and parries are sampled at the same phases at any `sv_fps`. The wielder's per-frame angle update (torso twist toward the view, the head look that lerps 15% per call, the spine correction) also runs once per grid step, with the step's time, so those angles converge as they did on a 20 Hz server; between steps the step's angles stand, and the look target the head turns toward is the one the per-frame block check found on the previous grid frame (the check runs after the angle update, so a 20 Hz frame saw the previous frame's result). The spine correction inside that update reads the skeleton without rebuilding it, which on a 20 Hz server is the pose a fresh read in that frame builds, so on shorter frames the skeleton is rebuilt first with its anims shifted by the photo's offset. Guns, NPCs, vehicle entities, saber locks and death animations are unaffected (a saber-wielding rider is a saber wielder and follows the grid). Effective only with `g_tickFix`

#### `g_tickFixSaberBox`
0 = the held saber's blocking box is built from the blade's position on the previous server frame

1 = (default) with `g_tickFixSaberDamage 2`, the held saber's box is built from the blade's newest 50 ms grid sample (when at most 100 ms old), the same blade that blade-on-blade contact is tested against, and the box is rebuilt on grid frames only (before the wielder's own sweep, as every 20 Hz frame did) and kept as an absolute box between them, so during the frame of grid time g every trace meets the box of the sample at g-50 at any `sv_fps`. As on a 20 Hz server, that box also holds the first blade's tip at g (the saber entity's origin, which a 20 Hz frame read at the grid time); on shorter frames it is taken from the wielder's photo at g rather than the frame-time read. The thrown saber's box is unchanged. Effective only with `g_tickFix`

#### `g_tickFixSaberOrder`
0 = a client command that arrives between server frames is run on arrival

1 = (default) with `g_tickFixSaberDamage 2`, a command that arrives between frames with an arrival time at or after the 50 ms grid time the next frame photographs is held, with every later command of that client, until that frame's saber photos are done, then run in arrival order at its own arrival time, before the frame ends. A 20 Hz server took its photo at the grid time, before any command that arrived after it, so the photo again sees every player as of the grid time (facing, position, swing). The cost: on a frame with a grid time, a command that arrived after the grid time runs up to one frame later (8 ms at 125 Hz). At `sv_fps 20` and below nothing is held. Effective only with `g_tickFix`

#### `g_tickFixSaberSample`
0 = with `g_tickFixSaberDamage 2`, a blade's 50 ms grid sample that falls between two server frames is interpolated linearly between those two frame reads

1 = with `g_tickFixSaberDamage 2`, that sample is the parabola through the last three frame reads: a blade turns about 60 degrees within a 33 ms frame, and the straight chord between two reads puts the tip about 10 units off its arc where the parabola stays within a few

2 = (default) with `g_tickFixSaberDamage 2`, that sample is a real read: the engine evaluates skeletons one frame late and ignores the time a read asks for, but honors each anim's start time, so the saber wielder's anims (and the anim each blends from) are re-set with their starts moved later by the gap between the grid time and the frame, the blade bolt is read, and they are set back. The read then shows the pose a 20 Hz server read at that grid time. Only for clients whose anims start on the grid (`g_tickFixSaberAnim`); at `sv_fps 20` every grid time is a frame, so nothing is interpolated or re-timed. The body a photo's blade sweep is tested against is re-timed the same way (the engine builds a collision skeleton at the frame time it is given, one frame fresher than its bolt reads, so the shift there is the photo's offset in the frame less the anim start's frame-length correction), so a hit on a moving defender lands as it did at 20 Hz. Effective only with `g_tickFix`

#### `g_smoothClientsAnchor`
0 = basejka player extrapolation (with `g_smoothClients`), where other players only visibly move when one of their packets arrives

1 = (default) anchor each player's extrapolation to when their latest packet arrived, so they keep moving smoothly between packets. Requires `g_smoothClients`. Tuned with `g_smoothClientsLead` (default 30, ms of extra lead, 0-200) and `g_smoothClientsCap` (default 100, maximum ms a player is extrapolated past their last packet, 0-1000; the actual limit is their measured packet interval plus jitter)

#### `g_smoothClientsFollow`
0 = spectators following a player see their raw position

1 = (default) advance a followed player's position by the time since their last packet, up to the same limit as `g_smoothClientsAnchor`

#### Improved projectile pushing/deflection
Pushing/deflection has been reworked in a way consistent with base siege gameplay, but in a bug-free manner. You can enable this with the recommended settings: `g_siegeReflectionFix 1`, `g_randomConeReflection -1`, `g_coneReflectAngle 30`, `g_breakRNG 0`. To disable, use the settings `g_siegeReflectionFix 0`, `g_randomConeReflection 0`, `g_coneReflectAngle 30`, `g_breakRNG -1`.

#### Chat limits
(Defaults are 0) you can set `g_chatLimit`, `g_teamChatLimit`, and `g_voiceChatLimit` to reduce the maximum amount of times people can spam chat during one second.

#### Custom team/class overrides
You can override the classes for any siege map. Use `g_redTeam <teamName>` and `g_blueTeam <teamName>`. For example, to use Korriban classes on any map, you could type `g_redTeam Siege3_Jedi` and `g_blueTeam Siege3_DarkJedi`.

To reset to base classes, use `0` or `none` as the argument.

This also works with votes; you can do `callvote g_redTeam <teamName>`. Enable this vote with `g_allow_vote_customTeams`.

Make sure to use the correct team name, which is written inside the .team file -- NOT filename of the .team file itself. The base classes leave out a final "s" in some of the filenames (`Siege1_Rebels` versus `Siege1_Rebel`).

A few important clientside bugs to be aware of:
* If custom teams/classes are in use, you cannot use the Join Menu to join that team. You must either use `team r` or `team b` (easiest method), autojoin, or use a CFG classbind.
* Ravensoft decided to combine force powers and items into one menu/cycle in JKA; however, if you have both items and force powers, it will only display the force powers. So for example if you are using Korriban classes on Hoth and want to place a shield as D tesh, you need to use a `use_field` bind.
* If the server is using teams/class that you don't have at all (like completely new classes, or classes for a map you don't have), you will see people as using Kyle skin with no sounds and no class icons.

#### `g_autoResetCustomTeams`
0 = retain custom teams/classes between map change votes

1 = (default) `g_redTeam` and `g_blueTeam` are automatically reset to normal classes when map is changed via `callvote`

#### `g_requireMoreCustomTeamVotes`
0 = 51% yes votes required for all votes to pass (base JKA)

1 = (default) custom team/class votes require 75% yes votes. This does not apply if the argument is `0` or `none` (resetting to normal classes)

#### `g_forceDTechItems`
This cvar helps custom team/class overrides by adding some extra weapons/items to the defense tech. Note: these do NOT apply to Korriban. The mod is hardcoded to ignore these values for Korriban. This cvar is only used when custom teams are in use, and does not affect any classes that already have demp/shield.

0 = no additional weapons/items

1 = only Hoth DTech gets demp only

2 = only Hoth DTech gets shield only

3 = only Hoth DTech gets demp and shield

4 = all DTechs get demp only

5 = (default) all DTechs get shield only

6 = all DTechs get demp and shield

7 = all DTechs get shield; only Hoth DTech gets demp also

#### Weapon spawn preference
Clients can decide their own preference of which weapon they would like to be holding when they spawn. To define your own preference list, type into your client JA: `setu prefer <15 letters>`
* `L` : Melee
* `S` : Saber
* `P` : Pistol
* `Y` : Bryar Pistol
* `E` : E-11
* `U` : Disruptor
* `B` : Bowcaster
* `I` : Repeater
* `D` : Demp
* `G` : Golan
* `R` : Rocket Launcher
* `C` : Concussion Rifle
* `T` : Thermal Detonators
* `M` : Trip Mines
* `K` : Det Packs

Your most-preferred weapons go at the beginning; least-preferred weapons go at the end. For example, you could enter `setu prefer RCTIGDUEBSMKYPL`

Note that this must contain EXACTLY 15 letters(one for each weapon). Also note that the command is `setu` with the letter `U` (as in "universe") at the end. Add this to your autoexec.cfg if you want ensure that it runs every time. Clients who do not enter this, or enter an invalid value, will simply use base JKA weapon priority.

#### `join`
Clientside command. Use to join a specific class and team, e.g. `join rj` for red jedi.

#### `help` / `rules` / `info`
Client command; displays some helpful commands and features that clients should be aware of (how to use `whois`, `class`, etc) as well as version number of currently-running server mod.

#### `serverstatus2`
Client command; displays many cvars to the client that are not shown with basejka `serverstatus` command.

#### `followtarget`
Client command; also triggered by pressing your `+use` bind from spectator. Follows the player closest to where you are aiming.

#### `changes`
Client command; if the creator of the current map included a file named `maps/map_name_goes_here.changes`, it will print the contents of that file. Helpful for knowing the changelog for the current version of the map.

#### `classes`
Client command; displays all class loadouts on the current map.

#### New item binds
`use_dispenser` = dispense an ammo can (works while jetpacking)

`use_anybacta` = use any bacta that you have

`use_pack` = throw a health pack if the map supports it (works while jetpacking)

You can also dispense ammo cans by pressing your `saberAttackCycle` (saber stance) bind.

#### Broadcast `siegeStatus` in serverinfo
base_entranced broadcasts some useful information, such as which round it currently is, what objective they are on, how much time is left, etc in the serverinfo. If you click to read the serverinfo from the game menu, you can see this information without connecting to the server.

#### Reset siege to round 1 on map change vote
No more changing maps with timer still going down.

#### Random teams/capts in siege
base_enhanced supports random teams/capts, but it doesn't work for siege mode. In base_entranced this is fixed and you can generate random teams/capts even in siege(players must set "ready" status by using `ready` command)

#### Map voting
You can vote to start a new pug with `callvote newpug <optional letters for maps>`, which allows players to vote for the map they want to play. If an argument is not specified, it will default to using the maps specified in `g_defaultPugMaps`. Each letter corresponds to a map name defined via cvar. For example, to make mp/siege_desert the map for the letter "d", you set `vote_map_d` to `mp/siege_desert` and set `vote_map_shortname_d` to `Desert`. Players can `callvote nextpug` to continue the pug map rotation. You can optionally enable `g_multiVoteRNG` to have votes merely *increase the chance* for a map to be randomly picked by the server, but it's not recommended.

#### Unlimited class-changing during countdown
Completely removed the cooldown for class-changing during the countdown.

#### Improved `tell`
You can still use partial client names with `tell` (for example, `tell pada hi` will tell the player Padawan a message saying "hi")

#### Improved `forceteam`
Use partial client name with `forceteam`. Optionally, you can include an additional argument for the number of seconds until they can change teams again (defaults to 0); for example, `rcon forceteam douchebag r 60`

#### `forceclass` and `unforceclass`
Teams can call special, team-only votes to force a teammate to a certain class for 60 seconds. Use the command `callteamvote`. For example, `callteamvote forceclass pad j` will force Padawan to play jedi for 60 seconds. Use `callteamvote unforceclass pad` to undo this restriction. Use `teamvote yes` and `teamvote no` to vote on these special teamvotes. These commands can also be executed with rcon directly.

#### `g_teamVoteFix`
(default: 1) There is a bug preventing the on-screen teamvote text from displaying on clients' screens if they are running certain client mods (such as SMod). This workaround prints some text on your screen so you can see what the vote is for.

#### `forceready` and `forceunready`
Use `forceready <clientnumber>` and `forceunready <clientnumber>` to force a player to have ready or not ready status. Use -1 to force everybody.

#### `g_allow_ready`
(default: 1) Use to enable/disable players from using the `ready` command.

#### `rename`
Rcon command to forcibly rename a player. Use partial client name or client number. Optionally, you can include an additional argument for the number of seconds until they can rename again (defaults to 0); for example, `rcon rename douchebag padawan 60`

#### Duplicate names fix
Players now gain a JA+-style client number appended to their name if they try to copy someone else's name.

#### Lockdown
Due to the possibility of troll players making trouble on the server and spamming reconnect under VPN IPs, you can lock down the server with `g_lockdown` (default: 0). While enabled, only players who are whitelisted will be allowed to chat, join, rename, or vote. In addition, non-whitelisted players will be renamed to "Client 13" (or whatever their client number is) when they connect. You can add/remove whitelisted players by using the command `whitelist`.

#### Probation
As a less severe alternative to banning troublemakers, you can simply place them under probation. Take their unique id from the server logs and write it into `probation.txt` in the server's /base/ folder (separate multiple ids with line breaks). Then the player will be treated according to the cvar `g_probation`:

0 = nothing unusual happens; treat players on probation normally

1 = players on probation cannot vote, call votes, use `tell` to send or receive messages, or use teamchat in spec. their votes are not needed for votes to pass.

2 = (default) same as 1, but they also cannot change teams without being forceteamed by admin.

#### Siege captain dueling
You can now challenge and accept captain duels using the basejka `engage_duel` command/bind (assuming server has `g_privateDuel 1` enabled). Both players receive 100 HP, 0 armor, pistol only, 125% speed, no items, no force powers, offense can go through defense-only doors, and turrets are automatically destroyed.

#### Awards/medals support
Humiliation, impressive, etc. Extra awards are being implemented (work-in-progress) for siege maps. You can get rewards if you use a compatible clientside mod such as Smod and have `cg_drawRewards` enabled in your client game.

#### Public server / Pug server modes
Use `callvote pug` to exec serverside `pug.cfg` or `callvote pub` to exec serverside `pub.cfg` (server admin must obviously create and configure these cfg files). Allow vote with `g_allow_vote_pug` and `g_allow_vote_pub`

#### `removePassword`
In basejka, it is impossible to remove an existing server password with rcon; the only way is by cfg. Now you can simply use the rcon command `removePassword` to clear the value of `g_password`.

#### More custom character colors
Some models allow you to use custom color shading (for example, trandoshan and weequay). Basejka had a lower limit of 100 for these settings(to ensure colors couldn't be too dark); this limit has been removed in base_entranced. Now you can play as a black trandoshan if you want. As in basejka, use the clientside commands `char_color_red`, `char_color_green`, and `char_color_blue` (valid values are between 0-255)

#### Selfkill lag compensation
Selfkilling with high ping can be frustrating on base servers when you accidentally max yourself due to selfkilling too late. base_entranced's lag compensation allows you to selfkill slightly closer to the respawn wave if you have high ping by subtracting your ping from the minimum delay of 1000 milliseconds. For example, if you have 200 ping, you will only have to wait 800 milliseconds before you can respawn instead of 1000. This is because you actually pressed your selfkill bind earlier and the server simply did not receive that packet until later.

#### Map updates/improvements
Some maps have hardcoded fixes in base_entranced in order to eliminate the need for releasing pk3 patches. Yes, some of these are "hacky," but it's better than forcing everyone to redownload the maps.

#### Enhanced mapping framework
base_entranced provides siege mapmakers with powerful new tools to have more control over their maps. You can do interesting things with these capabilities that are not possible in base JKA.

Mapmakers can add a text file with the filename `maps/mapname_goes_here.changes` to print a list of changes in the current version when a player enters `changes` in the console (make sure to include /mp/ sub-folder if needed).

Mapmakers can add some new extra keys to `worldspawn` entity for additional control over their maps:

Mapmakers can set the new class limits keys in `worldspawn`, which automatically sets the cvars such as `dAssaultLimit` on servers with `g_classLimits` set to `1`. For example, adding the key `dHWLimit` with value `1` will cause the server to set the cvar `dHWLimit` to `1`.

Mapmakers can set the new `forceOnNpcs` key in `worldspawn` to 1-3, which forces the server to execute `g_forceOnNpcs` to a desired number. If set, this cvar overrides `victimOfForce` for all NPCs on the map. If this key is not set, it will default to 0 (no force on NPCs - basejka setting).

Mapmakers can set the new `siegeRespawn` key in `worldspawn`, which forces the server to execute `g_siegeRespawn` to a desired number. If this key is not set, it will default to 20 (base JKA default).

Mapmakers can set the new `siegeTeamSwitch` key in `worldspawn`, which forces the server to execute `g_siegeTeamSwitch` to a desired number. If this key is not set, it will default to 1 (base JKA default).

Mapmakers can set the new `ghostcamera_origin` key (format: x y z) and `ghostcamera_yaw` keys in `worldspawn`, which governs where the ghost camera should be looking (with `g_siegeGhosting 2` and all teammates dead.

Mapmakers can define in `worldspawn` certain metadata about objectives for use with the top times feature. Enable this feature by setting the `combinedobjs` key to `1`. Then, using bitflags, follow the example below for siege_narshaddaa, which combines the third and fourth objs (station 1 and station 2) into one combined objective for the purposes of top times, by combining bitflags 4 and 8. To be clear, this feature is *only* for the top times feature, allowing objectives to be combined; in this example, you have to complete *both* stations objectives in order to trigger the "stations" objective (which is not a real objective). Without this feature, top times would be triggered for each station individually, which would allow you to achieve inhumanly fast times on either station by killing it extremely soon after killing the previous one.
```
combinedobjs    1

combinedobj1    1
combinedobj2    2
combinedobj3    12
combinedobj4    16
combinedobj5    32

combinedobj1name    Entrance
combinedobj2name    Checkpoint
combinedobj3name    Stations
combinedobj4name    Bridge
combinedobj5name    Codes
```

Mapmakers can add the new `target_icontoggle` entity, which can toggle an entity's icon on/off. Make sure to only target entities that you want to toggle icons for. If `spawnflags` has the `1` bitflag set, it will enable the icon. If `spawnflags` has the `2` bitflag set, it will disable the icon. If neither, it will toggle it. If `spawnflags` has the `128` bitflag set, the `target_icontoggle` will be considered inactive, and needs to be activated by a `target_activate` first. It is strongly recommended to only target things that *can legitimately have icons*, such as siege icon entities, ammo generators, etc., since there is, by design, no check performed on what type of entity is being targeted.

Mapmakers can add the new `target_gear` entity, which can add, remove, or set certain parameters for whoever triggered it. Set the `type` key to either `add`, `remove`, or `set`. Here is an example:
```
classname    target_gear
type    add
weapons    WP_ROCKET_LAUNCHER|WP_FLECHETTE
powerups    PW_FORCE_BOON
items    HI_MEDPAC|HI_SEEKER
ammo    AMMO_ROCKETS|AMMO_METAL_BOLTS
ammoamount    15
health    5
armor    5
force    FP_LIGHTNING,1|FP_LEVITATION,1
```
This is a ridiculous example, but it is just used to show some possibilities. This particular entity gives you rocket launcher and golan, gives you boon (or adds 25 seconds if you already have it), gives you a bacta and a seeker, adds 15 rockets and metallic bolts, adds 5 health, adds 5 armor, and adds one level of lightning and jump.

Mapmakers can add some new extra flags to .scl siege class files for additional control over siege classes:
* `ammoblaster <#>`
* `ammopowercell <#>`
* `ammometallicbolts <#>`
* `ammorockets <#>`
* `ammothermals <#>`
* `ammotripmines <#>`
* `ammodetpacks <#>`
* `maxsentries <#>` - place multiple sentries
* `dispensehealthpaks 1` - throw health packs instead of ammo cans
* `jetpackfreezeimmunity 1` - cannot have jetpack disabled by demp while already in flight
* `audiomindtrick 1` - cargo3 style mind trick
* `saberoffdamageboost 1` - cargo3 style saber damage boost
* `shortburstjetpack 1` - urban style jetpack
* `chargingdempremovesspawnshield 1` - hitting mouse2 with demp immediately removes green invulnerability shield
* `pull1IfSaberInAir 1` - force pull level reduced to 1 if saber is in air
* `detkilldelay 1000` - detpacks must be >= 1000ms old in order to detonate from death
* `classflags CFL_SPIDERMAN` - permanent wallgrab
* `classflags CFL_GRAPPLE` - melee grapple
* `classflags CFL_KICK` - melee kick
* `classflags CFL_WONDERWOMAN` - with melee equipped you block disrupts, reflect non-explosive projectiles, and take 50% damage from everything else
* `classflags CFL_SMALLSHIELD` - place a small shield that lasts forever but has limited HP instead of the base jka shield
* `saberoffextradamage 0.5` - take 50% extra damage while saber is equipped and holstered

Note about ammo: for example, adding `ammorockets 5` will cause a class to spawn with 5 rockets, and it will only be able to obtain a maximum of 5 rockets from ammo dispensers and ammo canisters. Note that the `CFL_EXTRA_AMMO` classflag still works in conjunction with these custom ammo amounts; for example, `ammodetpacks 3` combined with `CFL_EXTRA_AMMO` will give 6 detpacks (plus double ammo for all other weapons)

Mapmakers can customize weapons in their .scl siege class files. Up to 32 damage incoming damage modifiers and 32 outgoing damage modifiers are supported for each class. Examples:
```
    outgoingdmg1_mods    MOD_DISRUPTOR|MOD_DISRUPTOR_SPLASH|MOD_DISRUPTOR_SNIPER
    outgoingdmg1_dmgMult    0.25
    outgoingdmg1_freeze    yes
    outgoingdmg1_freezeMin  1000
    outgoingdmg2_freezeMax  2000
    outgoingdmg1_knockbackmult    8

    outgoingdmg2_mods    MOD_MELEE
    outgoingdmg2_otherenttype    OTHERENTTYPE_ALLY
    outgoingdmg2_negativedmgok    yes
    outgoingdmg2_mindmg    -10
    outgoingdmg2_maxdmg    -10

    outgoingdmg3_mods    MOD_BRYAR_PISTOL|MOD_BRYAR_PISTOL_ALT
    outgoingdmg3_otherenttype OTHERENTTYPE_ENEMY|OTHERENTTYPE_VEHICLE|OTHERENTTYPE_OTHER
    outgoingdmg3_mindmg    500
    outgoingdmg3_maxdmg    500
    outgoingdmg3_onlyknockback yes

    incomingdmg1_mods    MOD_REPEATER|MOD_REPEATER_ALT|MOD_REPEATER_ALT_SPLASH
    incomingdmg1_otherenttype OTHERENTTYPE_SELF|OTHERENTTYPE_ENEMY
    incomingdmg1_dmgMult    0
```
These are ridiculous examples, but they are just used to show some possibilities. In the first example, this class using disruptor on any entity will cause 25% damage, double knockback (25% * 8), and causes freezing randomly between 1000ms and 2000ms. In the second example, this class punching allies heals them for 10 health. In the third example, this class using pistol on anything *other than* allies/self causes 500 damage worth of knockback, but no actual damage. In the fourth example, this class getting repeatered by themself or enemies causes 0 damage. Other possible keys not listed in the above examples include:

`xxxdmgx_jedisplashdmgreduction 1/0` - force this class use/not use the base JKA 25% splash damage reduction for saberists (omit for default/base JKA default behavior)

`xxxdmgx_nonjedisaberdmgincrease 1/0` - force this class to use/not use the base JKA 4x saber damage (with max of 100) increase on non-saberists (omit for default/base JKA behavior)

Mapmakers can add the new `notouchbreak` key (use value `1`) to breakables to make sure they don't get damaged from touching them (for example, `func_glass` entities).

Mapmakers can add the new `2` spawnflag in `target_random` entities to make it actually randomize correctly.

Mapmakers can add the new `drawicon` key to shield/health/ammo generators. Use `drawicon 0` to hide its icon from the radar display (defaults to 1). The main intent of this is to hide shield/health/ammo generators from the radar that are not yet accessible to the players. For example, hiding the Hoth infirmary ammo generators until offense has reached the infirmary objective (use an `info_siege_radaricon` with the icon of the generator and toggle it on/off).

Mapmakers can add some new extra keys to `misc_siege_item` for additional control over siege items:

`autorespawn 0` = item will not automatically respawn when return timer expires. Must be targeted again (e.g., by a hack) to respawn.

`autorespawn 1` = item will automatically respawn when return timer expires (default/basejka)

`respawntime <#>` = item will take this many milliseconds to return after dropped and untouched (defaults to 20000, which is the basejka setting). Use `respawntime -1` to make the item never return.

`hideIconWhileCarried 0` = item's radar icon will be shown normally (default/basejka)

`hideIconWhileCarried 1` = item's radar icon will be hidden while item is carried, and will reappear when dropped

`idealClassType` = this item can only be picked up by this class type on team 1. Use the first letter of the class, e.g. `idealClassType s` for scout.

`idealClassTypeTeam2` = this item can only be picked up by this class type on team 2. Use the first letter of the class, e.g. `idealClassTypeTeam2 s` for scout.

`speedMultiplier` = this item causes a carrier on team 1 to change their speed, e.g. `speedMultiplier 0.5` to make the carrier move at 50% speed.

`speedMultiplierTeam2` = this item causes a carrier on team 2 to change their speed, e.g. `speedMultiplierTeam2 0.5` to make the carrier move at 50% speed.

`removeFromOwnerOnUse 0` = player holding item will continue to hold item after using it

`removeFromOwnerOnUse 1` = player holding item will lose posessesion of item upon using it (default/basjeka)

`removeFromGameOnuse 0` = item entity will remain in existence after using it

`removeFromGameOnUse 1` = item entity will be completely deleted from the game world upon using it (default/basejka)

`despawnOnUse 0` = item will not undergo any special "despawning" upon use (default/basejka)

`despawnOnUse 1` = item will be "despawned" (made invisible, untouchable, and hidden from radar) upon use

An example use case of the "`onUse`" keys could be to allow an item to respawn and be used multiple times, using a combination of `removeFromOwnerOnUse 1`, `removeFromGameOnUse 0`, and `despawnOnUse 1`.

Mapmakers are advised to include the new `healingteam` key to healable `func_breakable`s. Because this key is missing from basejka, if the server is using custom team/class overrides, both teams are able to heal `func_breakable`s. For example, `healingteam 2` ensures only defense will be able to heal it. base_entranced includes hardcoded overrides for Hoth, Desert and Nar Shaddaa, which is why this bug is not noticeable there.

Mapmakers can use the new entity `target_delay_cancel` to cancel the pending target-firing of a `target_delay`. This can be used to create Counter-Strike-style bomb-defusal objectives in which one team must plant a bomb, and the other team must defuse it. For example, an offense hack(planting the bomb) could trigger a `target_delay` for a 10 second delay for the bomb detonation, and a defense hack(defusing the bomb) could trigger a `target_delay_cancel` to cancel the explosion.

Mapmakers can now use the new `recallsiegeitem` key (set to `1`) on triggers to force a siege item to respawn if it is not currently being carried. Put the targetname of the item as the value of `recalltarget` key. It will respawn in its original spot unless you specify the targetname of a `target_position` entity as the value of the `recallorigin` key, in which case it will spawn at that `target_position`'s location.

Mapmakers can now use the `setteamallow` key on `trigger_once`, `trigger_multiple`, and `target_relay`. If it has this key set to `1` or `2` and targets a door, then that door will only have its `teamallow` property set when it's used by the trigger/relay. For example, you can now have a 100% locked door that nobody can access, and then it can be hacked so that it is still locked but now allows passage for one team. Use `1` for red team, `2` for blue team.

Mapmakers can now use the `justopen` key on `trigger_once`, `trigger_multiple`, and `target_relay`. If it has this key set and targets a locked door, then that door will just open once instead of unlocking (like the Ambush Room door at Cargo's first obj). You can set it to `1` for standard behavior (like cargo 1st obj door) or you can set it to `2` to allow only red team opening it or set it to `3` to allow only blue team opening it.

Mapmakers can use the `remainusableifunlocked` key on `func_door`s, which allows the door to remain usable (via targetname, i.e. from hacking a panel) after becoming unlocked. Ordinarily, once a locked door becomes unlocked, it is no longer usable in this way.

Mapmakers can add support for three-dimensional siege help in certain client mods by including a file with the filename `maps/map_name_goes_here.siegehelp` such as the following:
```json
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
		 "item":"codes",
		 "size":"large",
		 "origin":[6070, 1910, 179],
		 "redMsg":"Get codes",
		 "blueMsg":"Defend codes spawn"
	  }
   ]
}
```

Mapmakers can add some new extra flags to .npc files for additional control over NPCs:

`specialKnockback 1` = NPC cannot be knockbacked by red team

`specialKnockback 2` = NPC cannot be knockbacked by blue team

`specialKnockback 3` = NPC cannot be knockbacked by any team

`victimOfForce 1` = Red team can use force powers on this NPC

`victimOfForce 2` = Blue team can use force powers on this NPC

`victimOfForce 3` = Everybody can use force powers on this NPC

`nodmgfrom <#>` = This NPC is immune to damage from these weapons

`noKnockbackFrom <#>` = This NPC is immune to knockback from these weapons

`doubleKnockbackFrom <#>` = This NPC receives 2x knockback from these weapons

`tripleKnockbackFrom <#>` = This NPC receives 3x knockback from these weapons

`quadKnockbackFrom <#>` = This NPC receives 4x knockback from these weapons

`normalSaberDamage 1` = This NPC does not receive the basejka damage boost from sabers due to not having a saber equipped

* Bit values for weapons:
* Melee				1
* Stun baton			2
* Saber				4
* Pistol				8
* E11				16
* Disruptor		32
* Bowcaster			64
* Repeater			128
* Demp				256
* Golan			512
* Rocket				1024
* Thermal			2048			
* Mine				4096
* Detpack			8192
* Conc				16384
* Dark Force			32768
* Vehicle			65536
* Falling			131072
* Crush				262144
* Trigger_hurt		524288
* Other (lava, sentry, etc)				1048576
* Demp freezing immunity (not fully tested)		2097152

For example, to make an NPC receive double knockback from melee, stun baton, and pistol, add 1+2+8=11, so use the flag `doubleKnockbackFrom 11`

Note that `specialKnockback` overrides any other 0x/2x/3x/4x knockback flags.

Special note on `nodmgfrom`: you can use -1 as shortcut for complete damage immunity(godmode). Also note that using -1 or "demp freezing immunity" will prevent demp from damaging NPC, knockbacking an NPC, or causing electrocution effect.

Mapmakers can use `idealClassType` for triggers activated by red team; use the first letter of the class, e.g. `idealClassType s` for scout. Similarly, use `idealClassTypeTeam2` for blue team.

For a list of special words that you can use in .scl class files (e.g. `FP_LIGHTNING`, `WP_REPEATER`, etc.), [click here.](https://github.com/deathsythe47/base_entranced/blob/master/game/bg_saga.c#L45)

Note that if a map includes these new special features, and is then played on a non-base_entranced server, those features will obviously not work.

#### Additional control over vote-calling
In addition to the base_enhanced vote controls, you can use these:
* `g_allow_vote_randomteams` (default: 1)
* `g_allow_vote_randomcapts` (default: 1)
* `g_allow_vote_cointoss` (default: 1)
* `g_allow_vote_q` (default: 1)
* `g_allow_vote_killturrets` (default: 1)
* `g_allow_vote_pug` (default: 0)
* `g_allow_vote_pub` (default: 0)
* `g_allow_vote_customTeams` (default: 0)
* `g_allow_vote_forceclass` (default: 1)
* `g_allow_vote_zombies` (default: 1)
* `g_allow_vote_lockteams` (default: 1)

#### Zombies
"Zombies" is an unnoficial quasi-gametype that has been played by the siege community to kill time over years. It is a hide-and-seek game that involves one offense jedi hunting down defense gunners after some initial setup time. Gunners who die join offense jedi and hunt until there is only one gunner left.

Zombies receives some much-needed help in base_entranced. To activate the zombies features, use `zombies` (rcon command) or `callvote zombies` (assuming you enabled `g_allow_vote_zombies`). Zombies features include:
* All doors are automatically greened
* All turrets are automatically destroyed
* `g_siegeRespawn` is automatically set to 5
* No changing to defense jedi
* No changing to offense gunners
* Non-selfkill deaths by blue gunners cause them to instantly switch to red jedi
* No joining the spectators(so you can't spec a gunner and then know where he is after you join red)
* Silences several "was breached" messages on some maps
* Removes forcefields blocking player movement on some maps (e.g. cargo2 command center door)
* Prevents some objectives from being completed (e.g.cargo 2 command center)
* Hides locations in blue team teamoverlay(so you can't instantly track them down after you join red)
* Hides locations in blue team teamchat(so you can't instantly track them down after you join red)
* No using jetpack on cargo2 2nd obj
* Removes auto-detonation timer for detpacks

#### Bugfixes and other changes:

Note: this list is not necessarily intended to be comprehensive. This mod fixes a ton of bugs; they are probably not all documented here.

* Hoth bridge is forced to be crusher (prevents bridge lame).
* Fixed thermals bugging lifts.
* Fixed seekers attacking walkers and fighters.
* Fixed sentries attacking rancors, wampas, walkers, and fighters.
* Fixed bug with `nextmap` failed vote causing siege to reset to round 1 anyway.
* Fixed bug with Sil's ctf flag code causing weird lift behavior(people not getting crushed, thermals causing lifts to reverse, etc)
* Fixed bug where round 2 timer wouldn't function properly on maps with `roundover_target` missing from the .siege file.
* Fixed bug where `nextmap` would change anyway when a gametype vote failed.
* Fixed base_enhanced ammo code causing rocket classes not to be able to obtain >10 rockets from ammo canisters.
* Removed Sil's frametime code fixes, which caused knockback launches to have high height and low distance.
* Fixed polls getting cut-off after the first word if you didn't use quotation marks. Also announce poll text for people without a compatible client mod.
* Fixed a bug in Sil's ammo code where `ammo_power_converter`s didn't check for custom maximum amounts (different thing from `ammo_floor_unit`s)
* Fixed a bug in Sil's ammo code where direct-contact `+use` ammo-dispensing didn't check for custom maximum amounts
* Fixed a bug where some people couldn't see spectator chat, caused by the countdown teamchat bugfix.
* Fixed bug with `class` command not working during countdown.
* Added confirmation messages to the `class` command
* Fixed Bryar pistol not having unlimited ammo.
* Changes to `g_allowVote` are now announced to the server.
* Fixed `flourish` not working with gun equipped.
* You can no longer switch teams in the moment after you died, or while being gripped by a Rancor.
* You can no longer be `forceteam`ed to the same team you are already on (prevents admin abusing forced selfkill).
* Fixed a bug with `forceteam``specall``randomteams``randomcapts`auto inactivity timeout not working on dead players.
* Fixed jumping on breakable objects with the `thin` flag not breaking them as a result of Sil's base_enhanced code.
* Fixed `target_print` overflow causing server crashes.
* Fixed bug with not regenerating force until a while after you captured an item with `forcelimit 1` (e.g. Korriban crystals)
* Fixed `bot_minplayers` not working in siege gametype.
* Fixed bug with shield disappearing when placing it near a turret.
* Fixed bug that made it possible to teamkill with emplaced gun even with friendly fire disabled.
* Fixed a rare bug with everyone being forced to spec and shown class selection menu.
* Fixed a bug with final objective sounds (e.g. "primary objective complete") not being played(note: due to a clientside bug, these sounds currently do not play for the base maps).
* Cleaned up the displaying of radar icons on Hoth, Nar Shaddaa, Desert, and siege_codes. Fixes some icons being displayed when they shouldn't (for example, the only icon you should see at Hoth 1st obj is the 1st obj; you don't need to see any of the other objs or ammo gens or anything).
* Mind trick has been hardcoded to be removed from siege_codes, saving the need for me to release a new pk3 update for that map.
* Fixed a bug on the last objective of Desert caused by delivering parts within 1 second of each other.
* Fixed poor performance of force sight causing players to randomly disappear and reappear.
* Improved health bar precision.
* Fixed not regenerating force after force jumping into a vehicle.
* You can now taunt while moving.
* Teamoverlay data is now broadcast to spectators who are following other players.
* Fixed improper initialization of votes causing improper vote counts and improper display of teamvotes.
* Generic "you can only change classes once every 5 seconds" message has been replaced with a message containing the remaining number of seconds until you can change classes.
* Fixed bug with animal vehicles running in place after stopping.
* Fixed bug with "impressive" award being triggered for shooting NPCs.
* Added a workaround (Hoth only) for the bug where vehicles getting crushed cause their pilot to become invisible. Instead, the vehicle will instantly die. (see https://github.com/JACoders/OpenJK/issues/840)
* Fixed incorrect sentry explosion location when dropping it a long vertical distance.
* Voice chat is now allowed while dead.
* Fixed bug with siege items getting stuck when they respawn.
* Fixed double rocket explosion bug when shooting directly between someone's feet with rocket surfing disabled.
* Fixed primary mines not triggering if you were too far away.
* Fixed not being able to use icons for big Hoth turrets.
* Fixed not being able to re-activate inactive big Hoth turrets.
* Fixed not being able to trigger dead turrets.
* Fixed inactive turrets lighting up when respawning.
* Fixed issues with turrets tracking players who are cloaked or changed teams.
* Turret icons are now removed if the turret is destroyed and cannot respawn.
* A small sound is now played when dispensing an ammo canister.
* Fixed "using" mind trick when no enemy players were around.
* The correct pain sounds are now used for Siege (HP-percentage-based instead of HP-based).
* Fixed turrets teamkilling with splash damage.
* Fixed detpacks exploding when near a mover but not touching it.
* Fixed incorrect "station breached" messages on siege_narshaddaa.
* Fixed server crash when capturing an objective with a vehicle that has no pilot.
* Fixed being able to suicide by pistoling the walker.
* Lift kills now properly credit the killer.
* Kills via vehicle weapon after the pilot exited the vehicle now properly credit the killer.
* Falling deaths on certain maps now use the correct "fell to their death"/"thrown to their doom" message instead of generic "died"/"killed" message.
* 3-second-long fade-to-black falling deaths are now filtered to instant deaths in Siege.
* Filtered map callvotes to lowercase.
* Hoth map vote is automatically filtered to hoth2 if the server has it.
* Fixed SQL DB lag present in base_enhanced.
* Classes with shield (i.e. tech on many maps) cannot trigger the `g_fixPitKills` feature by manually SKing over a pit to get an early death at the hands of an enemy.
* Fixed bug where shields could not be destroyed while being attacked.
* Fixed rare bug with incorrect walker spawn on Hoth.
* Improved update rate of tech information on teammates (health/armor/ammo).
* Reduced the 5-second class change cooldown to 1 second.
* Fixed a bug where your armor could decrease over time if your maximum armor was higher than your maximum health.
* Fixed an issue with improper precaching of Siege skins if the mapmaker didn't include `default` as the skin in the .scl file.
* Fixed inconsistent timing of the ATST spawn on Hoth.
* Fixed not being able to dispense ammo cans while too close to a surface.
* Fixed small turrets causing splash damage.
* Fixed a bug where not all of your detpacks would explode if 10 of them were placed at the same time.
* Fixed players having different physics depending on their framerate (requires `pmove_float 1` and `g_gravity 760`).
* Fixed direct thermals potentially causing less damage than splash.
* Fixed single-objective maps not ending properly.
* Fixed not following the same person from spectator after the round ends or map changes.
* Fixed weird behavior with emplaced guns (touching them without mounting them, long cooldown after exiting one).
* Fixed big turrets' (like on Hoth) hitboxes not being accurate (invisible walls around them tanking shots)
* Fixed spawn invulnerability expiring during pause
* Fixed sometimes getting stuck and being unable to move when exiting the walker
* Fixed the inaccurate, massive hitboxes of large Hoth turrets
* Fixed hacks getting interrupted during server pause
* Pause/unpause votes can be called by simply typing "pause" or "unpause" in the chat
* Fixed mines and detpacks sometimes mysteriously tanking saber throws


# Features that are also in Alpha's base_enhanced
These are features in base_entranced that are also available in Alpha's base_enhanced (https://github.com/Avygeil/base_enhanced), the official server mod of the CTF community. base_entranced and Alpha's base_enhanced share the same ancestor (Sil's base_enhanced), and they are both open source, so they share a number of features. Note that I have not attempted to list every base_enhanced feature here; only the ones that are most relevant to siege.

#### Discord integration
You can have the server automatically post pug results via Discord webhook, configurable using the cvars `g_webhookId` and `g_webhookToken`

#### Country detection
The country of anyone who connects will be printed in the connect message.

#### Improved `status`
The `rcon status` command has been improved for readability, showing more information, etc.

#### Chat tokens
Clients can use the following chat tokens:

* `$h` = current health
* `$a` = current armor
* `$f` = current force
* `$m` = current ammo
* `$w` = current weapon (added in base_entranced)
* `$l` = closest weapon spawn (not useful for siege)

#### Advanced random map voting
Instead of the traditional random map voting to have the server pick a map, `g_allow_vote_maprandom`, if set to a number higher than `1`, will cause the server to randomly pick a few maps from a pool, after which players can vote to increase the weight of their preferred map with `vote 1`, `vote 2`, etc.

#### `g_teamOverlayForce`
0 = (default) base jka team overlay behavior

1 = broadcast force points in the armor field of team overlay. broadcast actual armor in the unused bits of the powerups field, which compatible client mods can display separately

#### `sv_passwordlessSpectators`
0 = (default) normal server password behavior

1 = prevent people from joining red/blue team if they do not have the correct password entered in their client (using `password` command or setting through the GUI). The server will automatically abolish its general password requirement if this is set(no password needed to connect to the server). This could be useful for opening up private/pug servers to the public for spectating.

#### `lockteams`
Callvote or rcon command; shortcut for setting `g_maxGameClients`. Use arguments `2s`, `3s`, `4s`, `5s`, `6s`, `7s`, or `reset` to specify amount. For example, `lockteams 4s` is the same as setting `g_maxGameClients` to 8.

Teams are automatically unlocked at intermission, or if there are 0 players in-game.

#### `g_teamOverlayUpdateRate`
The interval in milliseconds for teamoverlay data to be updated and sent out to clients. Defaults to 250 (base JKA is 1000).

#### `g_balanceSaber` (bitflag cvar)
0 = (default) basejka saber moves

1 = all saber stances can use kick move

2 = all saber stances can use backflip move

4 = using saber offense level 1 grants you all three saber stances

8 = (just for screwing around, not recommended) using saber offense level 2/3 grants you Tavion/Desann stance

#### `g_balanceSeeing`
0 = (default) basejka force sense behavior

1 = when crouching with saber up with sense 3, dodging disruptor shots is guaranteed

#### `g_allowMoveDisable`
-1 = (default) allow clients to disable some shitty saber moves

#### `g_fixForceJumpAnimationLock`
0 = (default) disable

1 = fix superman jumps going lower than regular jumps

#### `g_maxNameLength`
Sets the maximum permissible player name length. 35 is the basejka default; anything higher than that is untested (this cvar was intended to be set *lower* than 35).

#### `clientDesc`
Rcon command to see client mods people are using, if possible.

#### `shadowMute`
Rcon command to secretly mute people. Only shadowmuted players can see each others' chats.

#### Simplified private messaging
Instead of using tell, you can send private messages to people by pressing your normal chat bind and typing two @ symbols followed by their name and the message, e.g. `@@pad hello dude`

#### Bugfixes and other changes
* Troll/box characters (WSI fonts) are now disallowed from being in player names due to breaking formatting.
* Mitigated the ability of laggers to warp around unexpectedly by enforcing a minimum update rate.
* Fixed the bug where you would fire a shot and continue zooming in upon scoping with disruptor.
* Fixed performing DFA saber moves when simply holding W+attack while moving up a slope.
* Fixed a Sil bug that caused detpacks to float 1 unit higher than they should.
* Database load now happens when changing maps instead of at the start of every round, dramatically reducing restart times.
* Fixed grip draining your force but not actually gripping if you were at just the right distance from the target.

# Features that are also in Sil's base_enhanced
These are features in base_entranced that are also available in Sil's now-inactive base_enhanced mod (https://github.com/TheSil/base_enhanced), the legacy server mod of the CTF and siege communities. Since base_entranced was originally based on Sil's base_enhanced, and they are both open source, they share a number of features. Note that I have not attempted to list every base_enhanced feature here; only the ones that are most relevant to siege.

#### `class`
Clientside command. Use first letter of class to change, like `class a` for assault, `class s` for scout, etc. For maps with more than 6 classes, you can use `class 7`, `class 8`, etc.

#### `g_rocketSurfing`
0 = (default) no rocket surfing (ideal setting)

1 = bullshit rocket surfing enabled; landing on top of a rocket will not explode the rocket (base JKA)

#### `g_floatingItems`
0 = (default) no floating siege items (ideal setting for most maps)

1 = siege items float up walls when dropped - annoying bug on most maps, but classic strategy for korriban (base JKA)

#### `g_selfkillPenalty`
Set to 0 (the default) so you don't lose points when you SK.

#### `g_fixPitKills`
0 = normal pit kills (base JKA)

1 = (default) if you selfkill while above a pit, it grants a kill to whoever pushed you into the pit. This prevents people from denying enemies' kills with selfkill.

#### `g_maxGameClients`
0 = (default) people can freely join the game

other number = only this many players may join the game; the reset must stay in spectators

####"Joined the red/blue team" message
See when someone joined a team in the center of your screen in siege mode.

#### `pause` and `unpause`
Use command `pause` or `unpause` (also can be called as vote) to stop the game temporarily. Useful if someone lags out. Stops game timer, siege timer, spawn timer, etc.

#### `whois`
Use command `whois` to see a list of everyone in the server as well as their most-used alias. Optionally specify a client number or partial name to see the top aliases of that particular player.

#### Auto-click on death
If you die 1 second before the spawn, the game now automatically "clicks" on your behalf to make the respawn.

#### Random teams/capts
Use `randomteams 2 2` for random 2v2, etc. and `randomcapts` for random captains. Use `shuffleteams 2 2` for random teams that are different from the current teams. Make sure clients use `ready` to be eligible for selection (or use `forceready``forceunready` through rcon)

#### `specall`
Use the rcon command `specall` to force all players to spec.

#### Start round with one player
No longer need two players to start running around ingame in siege mode.

#### Better logs
Log detailed user info, rcon commands, and crash attempts. Use `g_hacklog <filename>`, `g_logclientinfo 1`, and `g_logrcon 1`.

#### Coin toss
Call a `cointoss` vote for random heads/tails result. Also works as an rcon command.

#### Polls
Ask everyone a question with `callvote q`. For example, `callvote q Keep same teams and restart?`

#### HTTP auto downloading
Set url with `g_dlurl`; clients with compatible mods such as SMod can download

#### Quiet rcon
Mis-typed commands are no longer sent out as a chat message to everyone on the server.

#### Fixed siege chat
* Spectator chat can be seen by people who are in-game
* Chat from dead players can be seen

#### `npc spawnlist`
Use this command to list possible npc spawns. Note that ingame console only lets you scroll up so far; use qconsole.log to see entire list.

#### Control vote-calling
Prevent calling votes for some things:
* `g_allow_vote_gametype` (default: 1023)
* `g_allow_vote_kick` (default: 1)
* `g_allow_vote_restart` (default: 1)
* `g_allow_vote_map` (default: 1)
* `g_allow_vote_nextmap` (default: 1)
* `g_allow_vote_timelimit` (default: 1)
* `g_allow_vote_fraglimit` (default: 1)
* `g_allow_vote_maprandom` (default: 4)
* `g_allow_vote_warmup` (default: 1)

#### Bugfixes and other changes:
* When you run someone over in the ATST, you get a kill.
* No more spying on the enemy teamchat during siege countdown.
* Bugfix for not scoring points on Hoth first obj.
* No more getting stuck because you spawned in a shield.
* "Was blasted" message when you get splashed by turrets instead of generic "died" message.
* No more seeker/sentry shooting at specs, teammates, or disconnected clients.
* No more rancor spec bug or SKing after rancor grab.
* No more weird camera and seeker glitch from walker.
* Bugfix for renaming causing saber style change.
* Bugfix for camera getting stuck when incinerated by sniper shot.
* Bugfix for dying while RDFAing and getting camera stuck.
* Bugfix for disconnecting in ATST and going invisible.
* No more SK when already dead.
* No more weird spec glitch with possessing someone else's body.
* Bugfix for sentry placed in lift.
* Allow for many more siege maps/classes on server.
* Bugfix for `map_restart 999999` (now maximum 10)
* Bugfix for spec voting.
* Bugfix for flechette stuck in wall.
* Bugfix for bots getting messed up when changing gametypes.
* Bugfix for healing NPCs/vehicles in siege.
* Bugfix for invisible/super-bugged shield because someone bodyblocked your placement.
* Bugfix for teamnodmg for NPCs.
* Golan alternate fire properly uses "was shrapnelled by" now.
* Trip mine alternate fire properly uses "was mined by" now.
* Bugfix for mines doing less splash damage when killed by player in slot 0.
* Miscellaneous slot 0 bugfixes.
* Fixed bryar pistol animations.
* Fixed players rolling/running through mines unharmed
* Fixed weird bug when getting incinerated while rolling.
* Fixed telefragging jumping players when spawning.
* Fixed NPCs attacking players in temporary dead mode.
* Fixed sentries having retarded height detection for enemies.
* Fixed `CFL_SINGLE_ROCKET` being able to obtain >1 rocket.
* Fixed camera bug when speccing someone riding a swoop.
* Security/crash fixes.
* Probably more fixes.
