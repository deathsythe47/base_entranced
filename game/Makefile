#
# Linux Makefile for GCC for Jedi Academy SDK
#
# Credits
#         - JA Coders (http://jacoders.co.cc)
#         - Raz0r
#         - TrippHop
#         - Xycaleth
#         - Sil
#

# Compiler - GCC
CC = gcc
#CC = /opt/intel/composer_xe_2013_sp1.0.080/bin/ia32/icc

# Compiler Flags
# I***          		include paths
# D***          		preprocessor definitions
# fPIC          		required to build shared object
# w             		disable warnings
# O3            		optimisation (warning: may cause issues)
# m32           		force 32-bit compilation
# -masm=intel   		assembler syntax�

# Compiler Flags - swap CONFIG for release/debug builds
CONFIG = -O2 -DFINAL_BUILD -DNDEBUG -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -D_GNU_SOURCE -std=c99
#CONFIG = -O0 -g -DFINAL_BUILD -DNDEBUG -D_GNU_SOURCE
#CONFIG = -g3 -D_DEBUG -D_GNU_SOURCE
INCLUDES = -I./ -I../include/
DEFINES = -DMISSION_PACK -DQAGAME -D_JK2 -D__linux__ -D__GCC__
OPTIONS = -Wall -fno-strict-aliasing -m32 -Wno-incompatible-pointer-types -fcommon #-masm=intel
FLAGS = $(INCLUDES) $(DEFINES) $(OPTIONS) $(CONFIG)
LIBS = -pthread ../lib/libsodium.a
#LIBS = -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -D_GNU_SOURCE /usr/local/lib/libcurl.a -lrt -Wl,-Bstatic -lz -Wl,-Bdynamic 
#LIBS = -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -D_GNU_SOURCE /usr/local/lib/libcurl.a -lrt -Wl,-Bstatic -lz -Wl,-Bdynamic 
#LIBS = -U_FORTIFY_SOURCE /usr/local/lib/libcurl.a -lrt -lz
#LIBS = -static /usr/local/lib/libcurl.a -lrt -lz 


# Target file
TARGET =    ../Final/jampgamei386.so

# Object Files
FILES =     ai_main.o               ai_util.o               ai_wpnav.o              AnimalNPC.o             bg_g2_utils.o           bg_misc.o \
            bg_panimate.o           bg_pmove.o              bg_saber.o              bg_saberLoad.o          bg_saga.o               bg_slidemove.o \
            bg_vehicleLoad.o        bg_weapons.o            crypto.o                FighterNPC.o            g_active.o              g_arenas.o \
            g_bot.o                 g_client.o              g_cmds.o                g_combat.o              g_exphysics.o           g_ICARUScb.o \
            g_items.o               g_log.o                 g_main.o                g_mem.o                 g_misc.o                g_missile.o  \
            g_mover.o               g_nav.o                 g_navnew.o              g_object.o              g_saga.o                g_session.o \
            g_spawn.o               g_strap.o               g_svcmds.o              g_syscalls.o            g_target.o              g_team.o  \
            g_timer.o               g_trigger.o             g_turret.o              g_turret_G2.o           g_utils.o               g_vehicles.o  \
            g_vehicleTurret.o       g_weapon.o              kdtree.o                NPC.o                   NPC_AI_Atst.o           NPC_AI_Default.o        NPC_AI_Droid.o \
            NPC_AI_GalakMech.o      NPC_AI_Grenadier.o      NPC_AI_Howler.o         NPC_AI_ImperialProbe.o  NPC_AI_Interrogator.o   NPC_AI_Jedi.o  \
            NPC_AI_Mark1.o          NPC_AI_Mark2.o          NPC_AI_MineMonster.o    NPC_AI_Rancor.o         NPC_AI_Remote.o         NPC_AI_Seeker.o  \
            NPC_AI_Sentry.o         NPC_AI_Sniper.o         NPC_AI_Stormtrooper.o   NPC_AI_Utils.o          NPC_AI_Wampa.o          NPC_behavior.o \
            NPC_combat.o            NPC_goal.o              NPC_misc.o              NPC_move.o              NPC_reactions.o         NPC_senses.o  \
            NPC_sounds.o            NPC_spawn.o             NPC_stats.o             NPC_utils.o             q_math.o                q_shared.o  \
            sha1.o                  SpeederNPC.o            tri_coll_test.o         WalkerNPC.o             w_force.o               w_saber.o  \
            jp_engine.o             jp_enginehelper.o       g_crash.o               sqlite3.o               g_database.o            g_database_upgrade.o  \
            collections.o           xxhash.o                g_accounts.o            cJSON.o					g_generators.o			g_transfers.o \
			g_table.o				sha3.o
			

# "make"
$(TARGET):  $(FILES)
	$(CC) -shared -m32 -o $(TARGET) $(FILES) $(LIBS)
	chmod +x $(TARGET)

# "make .c.o"
.c.o:
	$(CC) $(FLAGS) -c $*.c -o $*.o

# "make clean"
clean:
	rm -f $(FILES) $(TARGET)

jp_engine.o: OPTIONS += -masm=intel
