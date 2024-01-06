// ui_engine.c

#include "jp_engine.h"
#include "g_local.h"

//#if 0

#ifdef HOOK_GETSTATUS_FIX
	//--------------------------------
	//	Name:	getstatus flood fix
	//	Desc:	Adds limit for getstatus per second packets
	//	Hook:	SV_ConnectionlessPacket
	//	Retn:	SV_ConnectionlessPacket
	//--------------------------------
	#ifdef _WIN32
		#define GS_HOOKPOS		 0x443F9B	
		#define GS_RETURNPOS     0x443FA0
		#define GS_SVCPOS        0x443870
		#define GI_HOOKPOS		 0x443FCA	
		#define GI_RETURNPOS     0x443FCF
		#define GI_SVCPOS        0x443A10

	#elif defined(MACOS_X)
		#error HOOK_GETSTATUS_FIX not available on Mac OSX
	#else
		#define GS_HOOKPOS		 0x8056E50
		#define GS_RETURNPOS     0x8056E55
		#define GS_SVCPOS        0x8056574
		#define GI_HOOKPOS		 0x8056E95	
		#define GI_RETURNPOS     0x8056E9A
		#define GI_SVCPOS        0x8056784
	#endif

	unsigned int testip = 0;
	unsigned int getstatus_LastIP = 0;
	int getstatus_TimeToReset = 0;
	int getstatus_Counter = 0;
	int getstatus_LastReportTime = 0;
	int getstatus_UniqueIPCount = 0;
	int getstatus_lastpackettime = 0;

	typedef struct{
		unsigned ip;
		int count;
	} iprecord;

#define MAX_UNIQUE_IPS_AFTER_OVERFLOW 8
#define MAX_TRIES_PER_UNIQUE_IP 2
#define GETSTATUS_UNIFORM 0

	int CheckStatusCount(){
		static iprecord uniqueIps[MAX_UNIQUE_IPS_AFTER_OVERFLOW];

		//check for ban first
		if (G_FilterGetstatusPacket(testip)){
			//on black list, ban
			return 0;
		}

		if (!getstatus_TimeToReset){
			getstatus_TimeToReset = level.time + 1000;
		}

		if ((getstatus_Counter >= g_maxstatusrequests.integer) 
			&& getstatus_UniqueIPCount != MAX_UNIQUE_IPS_AFTER_OVERFLOW) {
			//big counter did already overflow, check for few unique ips
			//that we can allow over that
			int i;
			getstatus_LastIP = testip;

			++getstatus_Counter; //just to keep big counter updated
			
			for(i=0;i < getstatus_UniqueIPCount;++i){
				if (uniqueIps[i].ip == testip){
					//ip already requested
					if (uniqueIps[i].count < MAX_TRIES_PER_UNIQUE_IP){
						//we can allow this one
						++uniqueIps[i].count;
						return 1;
					} else {
						//nah, this one used his limit already
						return 0;
					}

				}
			}

			//ip not found, add it!
			uniqueIps[getstatus_UniqueIPCount].ip = testip;
			uniqueIps[getstatus_UniqueIPCount].count = 1;
			++getstatus_UniqueIPCount;
			return 1;
		}
		getstatus_LastIP = testip;
		++getstatus_Counter;

#if GETSTATUS_UNIFORM
		if (getstatus_lastpackettime){
			//if ((level.time - getstatus_lastpackettime) < (1000/g_maxstatusrequests.integer)){
			if (g_maxstatusrequests.integer*(level.time - getstatus_lastpackettime) < 1000){
				//too soon, dont allow
				getstatus_lastpackettime = level.time;
				return 0;
			}
		}

		getstatus_lastpackettime = level.time;
		return 1;
#else
		return (getstatus_Counter < g_maxstatusrequests.integer);
#endif

	}

	HOOK( GetStatusFix )
	{//getstatuf flood fix
		__StartHook( GetStatusFix )
		{
#ifdef _WIN32
			__asm2__( mov eax, [esp+28h+4]);
			__asm2__( mov testip, eax);
#else
			__asm2__( mov testip, ecx);
#endif

			if (CheckStatusCount()){
				__asm2__( mov ecx, GS_SVCPOS);
				__asm1__( call ecx ); //run SVC_Status
			}

			__asm1__( push GS_RETURNPOS );
			__asm1__( ret );
		}
		__EndHook( GetStatusFix )
	}

	HOOK( GetInfoFix )
	{//getinfo flood fix
		__StartHook( GetInfoFix )
		{

#ifdef _WIN32
			__asm2__( mov eax, [esp+28h+4]);
			__asm2__( mov testip, eax);
#else
			__asm2__( mov testip, ecx);
#endif

			if (CheckStatusCount()){
				__asm2__( mov ecx, GI_SVCPOS);
				__asm1__( call ecx ); //run SVC_Status
			}

			__asm1__( push GI_RETURNPOS );
			__asm1__( ret );
		}
		__EndHook( GetInfoFix )
	}



#endif //HOOK_GETSTATUS_FIX

#ifdef HOOK_RCONExtensions
        //--------------------------------
        //      Name:   RCON example hook
        //      Desc:   Just a simple hook in SVC_RemoteCommand.
        //                              Com_Printf the client it came from
        //      Hook:   SVC_RemoteCommand
        //      Retn:   SV_ExecuteClientMessage
        //--------------------------------
     
        #ifdef _WIN32
                #define RCON_HOOKPOS    0x443CED
                #define RCON_RETFUNC    0x410EE0
                #define RCON_ORIGBYTES { 0xE8, 0xEE, 0xD1, 0xFC, 0xFF }
        #else
                #define RCON_HOOKPOS    0x8056B26
                #define RCON_RETFUNC    0x80744E4
                #define RCON_ORIGBYTES { 0xE8, 0xB9, 0xD9, 0x01, 0x00 }
        #endif //_WIN32
     
        static void USED RCONExtensions( const char *command, const char* ip )
        {
			static char passwordBuffer[64];
			const char* p;
			const char *cmdBeginning;
			int size;
			qboolean passwordOk;

			if (!g_logrcon.integer){
				return;
			}

			//mask the password, we dont want to have log full 
			//of passwords that people accidently used
			p = command + 5;
			while (*p && *p!=' '){
				++p;
			}
			size = p - command - 5; 

			if (size > sizeof(passwordBuffer)-1){
				size = sizeof(passwordBuffer)-1;
			}
			
			Q_strncpyz(passwordBuffer,command+5,size+1);
			cmdBeginning = command+5+size+1;

			//compare password
			passwordOk = qfalse;
			if (!Q_strncmp(passwordBuffer,g_rconpassword.string,sizeof(passwordBuffer))){
				passwordOk = qtrue;
			}

			G_RconLog("rcon command {%s} from {%s}, {%s}\n",
				cmdBeginning, ip, passwordOk ? "passOK" : "passBAD");
        }
     
        HOOK( RCONExtensions )
        {//RCON Extensions
                __StartHook( RCONExtensions )
                {
                        __asm1__( pushad );                     // Secure registers
#ifdef _WIN32
						__asm1__( push 0x0052BDA0 );          // Pass ip to RconFix
                        __asm1__( push [esi] );               // Pass command to RconFix
#else
						__asm1__( push 0x08202460 );          // Pass ip to RconFix
						__asm1__( push 0x08202040 );          // Pass command to RconFix
#endif
                        __asm1__( call RCONExtensions );        // Call
                        __asm2__( add esp, 8 );                 // Clean up stack
                        __asm1__( popad );                      // Restore registers
                        __asm1__( push RCON_RETFUNC );          // Push Com_Milliseconds (In the engine)
                        __asm1__( ret );                        // Return execution flow into Com_Milliseconds as if nothing happened
                }
                __EndHook( RCONExtensions )
        }
#endif //HOOK_RCONExtensions

#ifdef HOOK_INFOBOOM_FIX
	//--------------------------------
	//	Name:	q3infoboom fix
	//	Desc:	Lowers buffer size and logs q3infoboom atempt
	//	Hook:	MSG_ReadStringLine
	//	Retn:	MSG_ReadStringLine
	//--------------------------------
	#ifdef _WIN32
		#define IB_HOOKPOS		  0x418B2A	
		#define IB_RETURNPOSOK    0x418AF0
		#define IB_RETURNPOSFAIL  0x418B32
	#elif defined(MACOS_X)
		#error HOOK_INFOBOOM_FIX not available on Mac OSX
	#else
		#define IB_HOOKPOS		  0x807803B
		#define IB_RETURNPOSOK    0x8078014
		#define IB_RETURNPOSFAIL  0x8078043
	#endif

	void LogInfoboomAttempt(){
		static int lastReportTime = 0;
		//q3infoboom tends to be spammy, allow to log only once in 5 minutes

		if (!lastReportTime || (level.time - lastReportTime > 60*1000)){
			G_HackLog("Q3InfoBoom attempt from %i.%i.%i.%i.\n",
					(unsigned char)(testip),
					(unsigned char)(testip>>8),
					(unsigned char)(testip>>16),
					(unsigned char)(testip>>24));
			lastReportTime = level.time;
		}
	}

	HOOK( InfoboomFix )
	{//q3infoboom fix
		__StartHook( InfoboomFix )
		{
#ifdef _WIN32
			__asm2__( cmp esi, 0x1FF );
#else
			__asm2__( cmp ebx, 0x1FF );
#endif
			__asm1__( jge fail );
			__asm1__( push IB_RETURNPOSOK );
			__asm1__( ret );

			__asmL__(fail: );
#ifdef _WIN32
			__asm2__( mov eax, [esp+0x28+4]);
#else
			__asm2__( mov eax, [esp+0x38]);
#endif
			__asm2__( mov testip, eax);

			LogInfoboomAttempt();
			__asm1__( push IB_RETURNPOSFAIL );
			__asm1__( ret );

		}
		__EndHook( InfoboomFix )
	}

#endif //HOOK_INFOBOOM_FIX

#ifdef FULLSERVER_CONNECT_REPORT
	//--------------------------------
	//	Name:	full server client connecting report
	//	Desc:	Reports when client is attempting to connect on full server
	//	Hook:	SV_DirectConnect
	//	Retn:	SV_DirectConnect
	//--------------------------------
	#ifdef _WIN32
		//#define FS_HOOKPOS	  0x43CC67
		//#define FS_RETURNPOS	  0x43CC74	
		#define FS_HOOKPOS		  0x43CC6C
		#define FS_RETURNPOS	  0x43CC71	
		#define FS_ESPOFFSET	  0x3C
		#define FS_COMDPRINTF	  0x40FDB0
	#elif defined(MACOS_X)
		#error FULLSERVER_CONNECT_REPORT not available on Mac OSX
	#else
		#define FS_HOOKPOS		  0x804C901	
		#define FS_RETURNPOS	  0x804C906	
		#define FS_ESPOFFSET	  0x18 //TODO: find out linux offset
		#define FS_COMDPRINTF	  0x8072ED4
	#endif


	#define MAX_CONNECTING_PEOPLE_LOG	8

	typedef struct{
		unsigned ip;
		int lastAttemptTime;
	} IPLog;

	static IPLog connectingIPLog[MAX_CONNECTING_PEOPLE_LOG];
	void LogFullserverConnect(const char* userinfo){
		int i;
		int freeslot = -1;

        unsigned ip = 0;
        getIpFromString( Info_ValueForKey( userinfo, "ip" ), &ip );

		for(i=0;i<MAX_CONNECTING_PEOPLE_LOG;++i){
			if (connectingIPLog[i].ip == ip){
				//we are logging this one already
				if (connectingIPLog[i].lastAttemptTime + 10000 < level.time){
					//been long time, consider this as a new attempt
					trap_SendServerCommand( -1, 
						va("print \"%s^7 is trying to connect\n\"", Info_ValueForKey(userinfo, "name")) );
				}

				connectingIPLog[i].lastAttemptTime = level.time;
				return;
			}

			if ((connectingIPLog[i].ip == 0) 
				|| (connectingIPLog[i].lastAttemptTime + 10000 < level.time)){
				freeslot = i;
			}

		}
				
		if (freeslot == -1){
			//not found free space, dont bother
			return;
		}

		connectingIPLog[freeslot].ip = ip;
		connectingIPLog[freeslot].lastAttemptTime = level.time;
		trap_SendServerCommand( -1, 
				va("print \"%s^7 is trying to connect\n\"", Info_ValueForKey(userinfo, "name")) );

	}

	HOOK( FullServerConnect )
	{//full server connect report
		__StartHook( FullServerConnect )
		{
			__asm2__( mov  ecx, FS_COMDPRINTF); 
			__asm1__( call ecx);

			__asm2__( lea  ecx, [esp+FS_ESPOFFSET]); 
			__asm1__( push  ecx); 
			__asm1__( call  LogFullserverConnect); 
			__asm1__( pop   ecx); 

			__asm1__( push FS_RETURNPOS );
			__asm1__( ret );

		}
		__EndHook( FullServerConnect )
	}

#endif //FULLSERVER_CONNECT_REPORT

#ifdef HOOK_DoneDL
        //--------------------------------
        //      Name:   DoneDL Fix
        //      Desc:   /donedl is often abused to make flags disappear and other game-related data
        //                      We prevent active clients from using this.
        //      Hook:   SV_DoneDownload_f
        //      Retn:   SV_SendClientGameState or SV_DoneDownload_f
        //--------------------------------
 
#ifdef _WIN32
        #define DDL_HOOKPOS                     0x43AEF4        //   setting CS_PRIMED in SV_SendClientGameState
        #define DDL_RETSUCCESS					0x43AEFA        //      SV_SendClientGameState
#else
        #define DDL_HOOKPOS                     0x804CF54       //   setting CS_PRIMED in SV_SendClientGameState
        #define DDL_RETSUCCESS                  0x804CF5A       //      SV_SendClientGameState
#endif //_WIN32


		typedef enum {
			CS_FREE,		// can be reused for a new connection
			CS_ZOMBIE,		// client has been disconnected, but don't reuse
							// connection for a couple seconds
			CS_CONNECTED,	// has been assigned to a client_t, but no gamestate yet
			CS_PRIMED,		// gamestate has been sent, but client hasn't sent a usercmd
			CS_ACTIVE		// client is fully in game
		} clientState_t;

        static void USED DoneDL_Handler( clientState_t *state )
        {
			// fix: set CS_PRIMED only when CS_CONNECTED is current state
			if (*state == CS_CONNECTED)
				*state = CS_PRIMED;
        }
 
        HOOK( DoneDL )
        {//DoneDL patch
                __StartHook( DoneDL )
                {
                        __asm1__( pushad );
                        __asm1__( push esi );
                        __asm1__( call DoneDL_Handler );
                        __asm2__( add esp, 4 );
                        __asm1__( popad );
                        __asm1__( push DDL_RETSUCCESS );
                        __asm1__( ret );
                }
                __EndHook( DoneDL )
        }
#endif //HOOK_DoneDL
 
#ifdef HOOK_NEGATIVEPORT_FIX

	#ifdef _WIN32
		#define PF_REPLACEPOS 0x4AA69D
	#else
		#define PF_REPLACEPOS 0x81A41A9
	#endif

#endif //HOOK_NEGATIVEPORT_FIX

#ifdef HOOK_TIMEWARPPING_FIX
        //--------------------------------
        //      Name:   TimeWrapping Fix
        //      Desc:   Fix of time wrapping after 23 days not properly restarting the server.
        //      Hook:   SV_Frame
        //      Retn:   SV_Frame 
        //--------------------------------
 
#ifdef _WIN32
        #define TW_HOOKPOS                     0x444671       //   call to SV_Shutdown() for map overflow
        #define TW_RETSUCCESS                  0x4446AB       //   after map_restart 0

        #define TW_HOOKPOS2                    0x44468F       //   call to SV_Shutdown() for numEntities overflow
        #define TW_RETSUCCESS2                 0x4446AB       //   after map_restart 0
#else
        #define TW_HOOKPOS                     0x8057A78       //   call to SV_Shutdown() for map overflow
        #define TW_RETSUCCESS                  0x8057A89       //   after map_restart 0

        #define TW_HOOKPOS2                    0x8057A56       //   call to SV_Shutdown() for numEntities overflow
        #define TW_RETSUCCESS2                 0x8057A67       //   after map_restart 0

#endif //_WIN32

        void TimeWrappingRestartMap()
        {
            char mapname[128];

            trap_Cvar_VariableStringBuffer("mapname",mapname,sizeof(mapname));
            trap_SendConsoleCommand( EXEC_APPEND, va( "killserver; map \"%s\";\n", mapname ) );
        }

        void TimeWrappingHandler( )
        {
            static qboolean restartIssued = qfalse;

            // safety condition - to prevent command overflow
            if ( !restartIssued )
            {
                 TimeWrappingRestartMap();

                restartIssued = qtrue;
            }  
        }

        HOOK( TimeWrapping )
        {
                __StartHook( TimeWrapping )
                {
                        __asm1__( pushad );                          
                        TimeWrappingHandler();
                        __asm1__( popad ); 
                        __asm1__( push TW_RETSUCCESS );
                        __asm1__( ret );
                }
                __EndHook( TimeWrapping )
        } 

        HOOK( TimeWrapping2 )
        {
                __StartHook( TimeWrapping2 )
                {
                        __asm1__( pushad );                          
                        TimeWrappingHandler();
                        __asm1__( popad ); 
                        __asm1__( push TW_RETSUCCESS2 );
                        __asm1__( ret );
                }
                __EndHook( TimeWrapping2 )
        } 

#endif //HOOK_TIMEWARPPING_FIX
#ifdef _WIN32
#define PILOT_NETWORK_REPLACEPOS 0x41952B
#else
#define PILOT_NETWORK_REPLACEPOS 0x8079A53
#endif
static hookEntry_t hooks[] =
{//List of hooks to be placed
	#ifdef HOOK_GETSTATUS_FIX
		HOOKDEF( GS_HOOKPOS, 0xE9, Hook_GetStatusFix, "getstatus flood fix" ),
		HOOKDEF( GI_HOOKPOS, 0xE9, Hook_GetInfoFix, "getinfo flood fix" ),
	#endif
	#ifdef HOOK_INFOBOOM_FIX
		HOOKDEF( IB_HOOKPOS, 0xE9, Hook_InfoboomFix, "q3infoboom fix" ),
	#endif
	#ifdef FULLSERVER_CONNECT_REPORT
		HOOKDEF( FS_HOOKPOS, 0xE9, Hook_FullServerConnect, "full server connect report" ),
	#endif
	#ifdef HOOK_RCON_LOG
		HOOKDEF( RL_HOOKPOS1, 0xE9, Hook_RconLog, "rcon log" ),
		#if !defined(_WIN32) && !defined(MACOS_X)
		HOOKDEF( RL_HOOKPOS2, 0xE9, Hook_RconLog2, "rcon log linux part2" ),
		#endif
	#endif
	#ifdef HOOK_RCONExtensions
		HOOKDEF( RCON_HOOKPOS, 0xE8, Hook_RCONExtensions, "rcon log" ),
	#endif	
    #ifdef HOOK_DoneDL
        HOOKDEF( DDL_HOOKPOS,  0xE9,   Hook_DoneDL,       "donedl fix"  ), 
    #endif
    #ifdef HOOK_TIMEWARPPING_FIX
        HOOKDEF( TW_HOOKPOS,  0xE9,   Hook_TimeWrapping,      "timewrapping fix"  ), 
        HOOKDEF( TW_HOOKPOS2, 0xE9,  Hook_TimeWrapping2,      "numentities overflow fix"  )
    #endif         

};
TABLESIZE( hooks, numHooks );

static replaceEntry_t replaces[] =
{//List of memory replaces to be placed
	#ifdef HOOK_NEGATIVEPORT_FIX
		REPLACEDEF( PF_REPLACEPOS, {'h'SEP'u'}, 2, "unsigned port fix" ),
	#endif

		REPLACEDEF(PILOT_NETWORK_REPLACEPOS,{ 0x8C }, 1, "pilot network bandwidth fix"),
    // time wrap 2 mins test - LINUX
    //REPLACEDEF( 0x805792C, {0xC0 SEP 0xD4 SEP 0x01 SEP 0x00}, 4, "time wrap test" ) 
    // time wrap 2 mins test - WINDOWS
    //REPLACEDEF( 0x44466B, {0xC0 SEP 0xD4 SEP 0x01 SEP 0x00}, 4, "time wrap test" ) 

};
TABLESIZE( replaces, numReplaces );

qboolean enginePatched = qfalse;

qboolean PatchEngine( void )
{
	int i;
	qboolean success = qtrue;

    //Com_Printf("Patching engine.\n");

	memset(connectingIPLog, 0, sizeof(connectingIPLog));

	//Place hooks
	for ( i=0; i<numHooks; i++ ){
		if (!PlaceHook( &hooks[i] )){
			success = qfalse;
		}
	}

	//Memory Overwrites
	for ( i=0; i<numReplaces; i++ ){
		if (!ReplaceMemory(&replaces[i])){
			success = qfalse;
		}
	}

	enginePatched = qtrue;

	return success;

}

qboolean UnpatchEngine( void )
{
	int i;
	qboolean success = qtrue;

	if (!enginePatched){
		return qtrue;
	}

    //Com_Printf("Unpatching engine.\n");
	
	//Remove hooks
	for ( i=0; i<numHooks; i++ ){
		if (!RemoveHook( &hooks[i] )){
			success = qfalse;
		}
	}

	//Memory Restores
	for ( i=0; i<numReplaces; i++ ){
		if (!RestoreMemory(&replaces[i])){
			success = qfalse;
		}
	}

	enginePatched = qfalse;

	return success;
	
}

//#endif

