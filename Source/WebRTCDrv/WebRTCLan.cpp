/*=============================================================================
	WebRTCLan.cpp — UE1 LAN server discovery answered from the WebRTC peer list.

	The stock discovery script runs over AUdpLink, and a browser has no UDP, so
	this answers it with synthetic packets built from the glue's server list:
	  - the passive listener (Unreal 1's ClientBeaconReceiver binds the beacon
	    port and waits for "<Product> <address> <text>" beacons);
	  - the "REPORTQUERY" sweep to ServerBeaconPort+0..9, answered with
	    "<Product> <QueryPort>" from the server's address, then the "\info\"
	    query answered with "\key\value\" pairs (UT99's UBrowser).
	HP2's content has no UdpLink script (no IpDrv.u), so nothing in this game
	reaches these hooks.

	A server is a synthetic address in 10.199.0.0/16 whose low 16 bits index a
	room table: FURL parses the dotted address as a host, and
	UWebRTCNetDriver::InitConnect translates it back to the room.

	IpDrv calls in through WebRTCLan.h; this file uses no IpDrv types.
=============================================================================*/

#include "Engine.h"
#include "UnNet.h"
#include "WebRTCLan.h"

#ifdef __EMSCRIPTEN__
#include "ue_abi.h"	// the discovery imports (ue_net_discover_servers ... ue_net_get_server_ping)

/*-----------------------------------------------------------------------------
	Tunables + state.
-----------------------------------------------------------------------------*/

// 10.199.0.0/16 — private, and far away from anything a real LAN hands out. The low 16 bits are the
// index into GRooms, which is why the table must be append-only.
#define LAN_ADDR_BASE     0x0AC70000u
#define LAN_ADDR_MASK     0xFFFF0000u
// The "query port" we advertise. UT99's browser sends its \info\ there; nothing binds it for real.
#define LAN_QUERY_PORT    7778
// The game port that goes in the row / the join URL. Cosmetic — the room is what actually routes.
#define LAN_GAME_PORT     7777
// Re-run discovery at most this often; the browser re-queries on a timer and on REFRESH.
#define LAN_DISCOVERY_MIN_INTERVAL  3.0
// Unreal 1's ClientBeaconReceiver ages entries out after BeaconTimeout (5 s default), so a listening
// client needs a fresh beacon well inside that or rows blink out.
#define LAN_BEACON_REPEAT_INTERVAL  2.0

struct FLanServer
{
	FString Room, Name, Map, GameType;
	INT     Players, MaxPlayers, Ping;
	FLanServer() : Players(0), MaxPlayers(0), Ping(0) {}
};

// Append-only: an index handed out in a synthetic address must keep meaning the same room for as long
// as the session lives, because the player may join minutes after the row was drawn.
static TArray<FString>    GRooms;
static TArray<FLanServer> GServers;
static UBOOL              GDiscoveryPending = 0;
// appSeconds() returns FTime (32.32 fixed point, no < or >=); this file keeps DOUBLE timestamps.
static inline DOUBLE LanNow() { return (DOUBLE)appSeconds().GetFloat(); }

static DOUBLE             GLastDiscoveryAt  = 0.0;
// Set by the net driver when we are the listen server, so we never advertise ourselves to ourselves.
static FString            GLocalRoom;
// Set where proxying is unsafe (WebRTCLan_Bind); acted on from the tick.
static UBOOL              GWantKick = 0;

// A link that wants synthetic traffic. `WantsBeacons` is set for a passive (beacon-port) listener.
struct FLanLink
{
	const void* Id;
	INT     BoundPort;
	UBOOL   WantsBeacons;      // Unreal 1: emit unsolicited beacons on a timer
	DOUBLE  NextBeaconAt;
	UBOOL   bLoggedFirstPoll;
	// A sweep this link asked for that discovery could not answer yet.
	UBOOL   bSweepPending;
	DOUBLE  SweepDeadline;
	DOUBLE  LastPendingLog;
};
static TArray<FLanLink> GLinks;

struct FLanReply
{
	const void* LinkId;
	DWORD   Addr;
	INT     Port;
	FString Text;
	FLanReply() : LinkId(NULL), Addr(0), Port(0) {}
	FLanReply( const void* InLinkId, DWORD InAddr, INT InPort, const FString& InText )
	: LinkId(InLinkId), Addr(InAddr), Port(InPort), Text(InText) {}
};
static TArray<FLanReply> GReplies;

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

static FLanLink* FindLink( const void* Id )
{
	for( INT i=0; i<GLinks.Num(); i++ )
		if( GLinks(i).Id == Id )
			return &GLinks(i);
	return NULL;
}

static FLanLink& TouchLink( const void* Id )
{
	FLanLink* L = FindLink( Id );
	if( L )
		return *L;
	FLanLink& N = GLinks( GLinks.AddZeroed() );
	N.Id = Id;
	return N;
}

/**
 * Stable index for a room; GRooms is append-only.
 *
 * Appended with `new( Array )T( Item )`, not `Array.AddItem( Item )`: AddItem assigns into an
 * unconstructed slot, and assigning an FString reallocs whatever garbage pointer is there.
 */
static INT RoomIndex( const FString& Room )
{
	for( INT i=0; i<GRooms.Num(); i++ )
		if( appStricmp( *GRooms(i), *Room )==0 )
			return i;
	new( GRooms )FString( Room );
	return GRooms.Num() - 1;
}

static DWORD AddrForIndex( INT Index )
{
	return LAN_ADDR_BASE | (DWORD)(Index & 0xFFFF);
}

static UBOOL IsSyntheticAddr( DWORD Addr )
{
	return (Addr & LAN_ADDR_MASK) == LAN_ADDR_BASE;
}

/** The beacon product string the stock script will compare against. Read the ini, never guess: Unreal 1
 *  compares with `==` (case-sensitive), so a hardcoded "unreal" would silently match nothing. */
static FString BeaconProduct()
{
	FString P;
	if( GConfig->GetString( TEXT("UBrowser.UBrowserLocalFact"), TEXT("BeaconProduct"), P ) && P.Len() )
		return P;
	if( GConfig->GetString( TEXT("IpDrv.UdpBeacon"), TEXT("BeaconProduct"), P ) && P.Len() )
		return P;
	return FString(TEXT("unreal"));
}

/*-----------------------------------------------------------------------------
	Discovery.
-----------------------------------------------------------------------------*/

static void KickDiscovery()
{
	DOUBLE Now = LanNow();
	if( GDiscoveryPending || Now - GLastDiscoveryAt < LAN_DISCOVERY_MIN_INTERVAL )
		return;
	GLastDiscoveryAt  = Now;
	GDiscoveryPending = 1;
	ue_net_discover_servers( "" );   // "" = every room on this signalling server
	if( GUfrontLog )
		debugf( NAME_DevNet, TEXT("[WebRTC-LAN] discovery started") );
}

/**
 * Drain a finished discovery into GServers. No-op while one is still in flight.
 *
 * Polls at most four times a second, which keeps the module alive: every ue_net_* call here is a
 * `__proxy: 'sync'` round trip from the engine pthread to the main thread, this runs from AUdpLink::Tick
 * once per link per frame, and asking every frame overflows Emscripten's proxy queue
 * (Aborted: "emscripten_proxy_async failed").
 */
static DOUBLE GLastPumpAt = 0.0;

static void PumpDiscovery()
{
	if( !GDiscoveryPending )
		return;
	const DOUBLE PumpNow = LanNow();
	if( PumpNow - GLastPumpAt < 0.25 )
		return;
	GLastPumpAt = PumpNow;
	if( !ue_net_is_discovery_done() )
		return;
	GDiscoveryPending = 0;

	// Each server is copy-constructed in place (`new( Array )T( Item )`): TArray<T> is a POD container, so
	// AddItem() assigns into an unconstructed slot, and FString's operator= reallocs whatever pointer the
	// allocator left there (FLanServer holds four). On a freshly zeroed heap that happens to work. Removal and
	// Empty() do destruct the elements they drop.
	GServers.Empty();
	const INT Count = ue_net_get_server_count();
	char nameBuf[256], roomBuf[256], mapBuf[128], gtBuf[128];
	for( INT i=0; i<Count; i++ )
	{
		appMemzero( nameBuf, sizeof(nameBuf) );
		appMemzero( roomBuf, sizeof(roomBuf) );
		appMemzero( mapBuf,  sizeof(mapBuf)  );
		appMemzero( gtBuf,   sizeof(gtBuf)   );
		if( !ue_net_get_server_info( i, nameBuf, sizeof(nameBuf), roomBuf, sizeof(roomBuf) ) )
			continue;
		FString Room = ANSI_TO_TCHAR(roomBuf);
		// Skip our own listen server: a host browsing its own LAN list should not see itself, and
		// joining it would be a peer connecting to itself.
		if( GLocalRoom.Len() && appStricmp( *Room, *GLocalRoom )==0 )
			continue;
		ue_net_get_server_map( i, mapBuf, sizeof(mapBuf) );
		ue_net_get_server_gametype( i, gtBuf, sizeof(gtBuf) );

		FLanServer S;
		S.Room       = Room;
		S.Name       = ANSI_TO_TCHAR(nameBuf);
		S.Map        = mapBuf[0] ? FString(ANSI_TO_TCHAR(mapBuf)) : FString(TEXT("?"));
		S.GameType   = ANSI_TO_TCHAR(gtBuf);
		S.Players    = ue_net_get_server_players( i );
		S.MaxPlayers = ue_net_get_server_maxplayers( i );
		S.Ping       = ue_net_get_server_ping( i );
		if( S.MaxPlayers <= 0 ) S.MaxPlayers = 16;
		if( S.Ping      <= 0 )  S.Ping = 1;
		if( !S.Name.Len() )     S.Name = Room;
		const FString Host = WebRTCLan_HostForRoom( *S.Room );   // also reserves the room's index
		if( GUfrontLog )
			debugf( NAME_DevNet, TEXT("[WebRTC-LAN] '%s' room=%s map=%s %i/%i %ims -> %s"),
				*S.Name, *S.Room, *S.Map, S.Players, S.MaxPlayers, S.Ping, *Host );
		new( GServers )FLanServer( S );
	}
	if( GUfrontLog )
		debugf( NAME_DevNet, TEXT("[WebRTC-LAN] discovery done: %i server(s)"), GServers.Num() );
}

/*-----------------------------------------------------------------------------
	Reply builders.
-----------------------------------------------------------------------------*/

// Placement new, like every other append in this file (AddItem assigns into an unconstructed slot).
static void Queue( const void* LinkId, DWORD Addr, INT Port, const FString& Text )
{
	new( GReplies )FLanReply( LinkId, Addr, Port, Text );
}

/** Unreal 1: an unsolicited server beacon. "<Product> <address> <what the menu shows>". */
static void QueueBeacons( const void* LinkId )
{
	const FString Product = BeaconProduct();
	for( INT i=0; i<GServers.Num(); i++ )
	{
		const FLanServer& S = GServers(i);
		const DWORD Addr = AddrForIndex( RoomIndex( S.Room ) );
		// The menu splits on the first space: everything before it is the join address, the rest is the
		// row label. So the label must not be empty, and the address must not contain a space.
		Queue( LinkId, Addr, LAN_GAME_PORT,
			FString::Printf( TEXT("%s %s %s (%s) %i/%i"),
				*Product, *WebRTCLan_HostForRoom( *S.Room ), *S.Name, *S.Map, S.Players, S.MaxPlayers ) );
	}
}

/** UT99: the reply to REPORTQUERY — "<Product> <QueryPort>", from the server's synthetic address. */
static void QueueBeaconQueries( const void* LinkId )
{
	const FString Product = BeaconProduct();
	for( INT i=0; i<GServers.Num(); i++ )
	{
		const DWORD Addr = AddrForIndex( RoomIndex( GServers(i).Room ) );
		Queue( LinkId, Addr, LAN_QUERY_PORT, FString::Printf( TEXT("%s %i"), *Product, LAN_QUERY_PORT ) );
	}
}

/** UT99: the GameSpy-style \key\value\ answer UBrowserServerPing parses. */
static void QueueServerInfo( const void* LinkId, DWORD Addr )
{
	const INT Index = (INT)(Addr & 0xFFFF);
	if( Index < 0 || Index >= GRooms.Num() )
		return;
	const FString Room = GRooms( Index );
	for( INT i=0; i<GServers.Num(); i++ )
	{
		const FLanServer& S = GServers(i);
		if( appStricmp( *S.Room, *Room )!=0 )
			continue;
		// `maptitle` doubles as the row's display name, `gamever` keeps the row out of the
		// version-mismatch filter, and `worldlog\false` keeps the ngWorldStats column honest.
		Queue( LinkId, Addr, LAN_QUERY_PORT, FString::Printf(
			TEXT("\\hostname\\%s\\hostport\\%i\\mapname\\%s\\maptitle\\%s\\gametype\\%s")
			TEXT("\\numplayers\\%i\\maxplayers\\%i\\gamemode\\openplaying\\gamever\\%i")
			TEXT("\\minnetver\\%i\\worldlog\\false\\wantworldlog\\false\\final\\"),
			*S.Name, LAN_GAME_PORT, *S.Map, *S.Map, *S.GameType,
			S.Players, S.MaxPlayers, ENGINE_VERSION, ENGINE_MIN_NET_VERSION ) );
		return;
	}
}

/*-----------------------------------------------------------------------------
	The IpDrv-facing hooks.
-----------------------------------------------------------------------------*/

void WebRTCLan_Bind( const void* LinkId, INT Port )
{
	FLanLink& L = TouchLink( LinkId );
	L.BoundPort = Port;

	// Warm the cache on any bind, not just the beacon port: a browser's link binds, sweeps once and is gone
	// a few seconds later, and discovery (a peer-list XHR plus a status handshake per server) is slower than
	// that; a warm GServers also answers the next query.
	//
	// Deferred to WebRTCLan_NextReply, not called here: BindPort can run inside a script event this file
	// delivered (a beacon reply -> FoundServer -> spawn UBrowserServerPing -> BindPort), i.e. inside the
	// tick, script and an Asyncify-unwound stack, and a sync cross-thread ue_net_* call from there hangs
	// the engine.
	GWantKick = 1;

	// Unreal 1's ClientBeaconReceiver is passive: it binds the beacon port and waits for servers to
	// shout. Nothing will ever arrive to trigger us, so binding that port is the trigger.
	INT BeaconPort = 7776;
	GConfig->GetInt( TEXT("IpDrv.UdpBeacon"), TEXT("BeaconPort"), BeaconPort );
	if( Port == BeaconPort )
	{
		L.WantsBeacons = 1;
		L.NextBeaconAt = 0.0;
		KickDiscovery();
		debugf( NAME_DevNet, TEXT("[WebRTC-LAN] beacon listener bound on port %i"), Port );
	}
}

void WebRTCLan_Unbind( const void* LinkId )
{
	for( INT i=GLinks.Num()-1; i>=0; i-- )
		if( GLinks(i).Id == LinkId )
			GLinks.Remove( i );
	for( INT i=GReplies.Num()-1; i>=0; i-- )
		if( GReplies(i).LinkId == LinkId )
			GReplies.Remove( i );
}

UBOOL WebRTCLan_Send( const void* LinkId, DWORD DestAddr, INT DestPort, const TCHAR* Text )
{
	if( !Text )
		return 0;
	TouchLink( LinkId );

	// UT99's LAN sweep: ten of these (ServerBeaconPort+0..9). The browser broadcasts once when its window
	// opens and asks again only on REFRESH, while discovery takes hundreds of milliseconds, so answering from
	// the cache alone usually gives an empty list. The sweep is remembered and answered once discovery has
	// servers, the way replies to a real LAN broadcast trickle in after it.
	if( appStrnicmp( Text, TEXT("REPORTQUERY"), 11 )==0 )
	{
		KickDiscovery();
		FLanLink& L = TouchLink( LinkId );
		L.WantsBeacons  = 0;
		L.bSweepPending = 1;
		L.SweepDeadline = LanNow() + 30.0;   // stop re-kicking eventually; the browser has REFRESH
		PumpDiscovery();
		if( GServers.Num() > 0 )
		{
			QueueBeaconQueries( LinkId );
			L.bSweepPending = 0;
		}
		return 1;
	}
	// "REPORT" (the plain beacon request) — same answer shape as an unsolicited beacon.
	if( appStrnicmp( Text, TEXT("REPORT"), 6 )==0 )
	{
		KickDiscovery();
		PumpDiscovery();
		QueueBeacons( LinkId );
		return 1;
	}
	// A GameSpy query aimed at one of our synthetic servers.
	if( IsSyntheticAddr( DestAddr ) && Text[0]=='\\' )
	{
		PumpDiscovery();
		QueueServerInfo( LinkId, DestAddr );
		return 1;
	}
	// Anything else addressed at a synthetic server is ours to swallow: there is no socket that could
	// carry it.
	if( IsSyntheticAddr( DestAddr ) )
		return 1;

	return 0;
}

UBOOL WebRTCLan_NextReply( const void* LinkId, DWORD* OutAddr, INT* OutPort, FString& OutText )
{
	// Grant any deferred discovery kick here: this is tick context, which is safe to proxy from.
	if( GWantKick )
	{
		GWantKick = 0;
		KickDiscovery();
	}
	// One log line per link, the first time it polls: replies are delivered only to a link that ticks.
	{
		FLanLink* L0 = FindLink( LinkId );
		if( L0 && !L0->bLoggedFirstPoll )
		{
			L0->bLoggedFirstPoll = 1;
			debugf( NAME_DevNet, TEXT("[WebRTC-LAN] link %p is polling (port %i)"), LinkId, L0->BoundPort );
		}
	}
	PumpDiscovery();

	// Passive Unreal 1 listeners get a fresh beacon every couple of seconds, because the receiver ages
	// rows out on BeaconTimeout and a stale list looks exactly like a broken one.
	FLanLink* L = FindLink( LinkId );
	if( L && L->WantsBeacons )
	{
		DOUBLE Now = LanNow();
		if( Now >= L->NextBeaconAt )
		{
			L->NextBeaconAt = Now + LAN_BEACON_REPEAT_INTERVAL;
			KickDiscovery();
			QueueBeacons( LinkId );
		}
	}

	// A sweep we owed an answer to: deliver it as soon as discovery has one, and keep asking until it
	// does (or until the deadline — after that the player's REFRESH is the retry).
	if( L && L->bSweepPending )
	{
		// While a sweep is outstanding, log it every few seconds, so a link that stopped ticking and a
		// discovery that found nothing look different.
		DOUBLE Now2 = LanNow();
		if( Now2 - L->LastPendingLog > 5.0 )
		{
			L->LastPendingLog = Now2;
			if( GUfrontLog )
				debugf( NAME_DevNet, TEXT("[WebRTC-LAN] sweep pending: discovery %s, %i server(s) known"),
					GDiscoveryPending ? TEXT("in flight") : TEXT("idle"), GServers.Num() );
		}
		if( GServers.Num() > 0 )
		{
			QueueBeaconQueries( LinkId );
			L->bSweepPending = 0;
			debugf( NAME_DevNet, TEXT("[WebRTC-LAN] answered a pending LAN sweep with %i server(s)"), GServers.Num() );
		}
		else if( LanNow() > L->SweepDeadline )
		{
			L->bSweepPending = 0;
			debugf( NAME_DevNet, TEXT("[WebRTC-LAN] LAN sweep found nothing before its deadline") );
		}
		else
		{
			KickDiscovery();
		}
	}

	for( INT i=0; i<GReplies.Num(); i++ )
	{
		if( GReplies(i).LinkId != LinkId )
			continue;
		*OutAddr = GReplies(i).Addr;
		*OutPort = GReplies(i).Port;
		OutText  = GReplies(i).Text;
		GReplies.Remove( i );
		return 1;
	}
	return 0;
}

FString WebRTCLan_HostForRoom( const TCHAR* Room )
{
	const DWORD A = AddrForIndex( RoomIndex( FString(Room) ) );
	return FString::Printf( TEXT("%i.%i.%i.%i"), (A>>24)&0xFF, (A>>16)&0xFF, (A>>8)&0xFF, A&0xFF );
}

FString WebRTCLan_RoomForHost( const TCHAR* Host )
{
	if( !Host )
		return FString(TEXT(""));
	// Parse a.b.c.d without pulling in a socket header (this file must not depend on IpDrv).
	INT Oct[4] = {0,0,0,0}, N = 0;
	const TCHAR* P = Host;
	while( N < 4 )
	{
		if( *P < '0' || *P > '9' )
			return FString(TEXT(""));
		INT V = 0;
		while( *P >= '0' && *P <= '9' )
			V = V*10 + (*P++ - '0');
		if( V > 255 )
			return FString(TEXT(""));
		Oct[N++] = V;
		if( N < 4 )
		{
			if( *P != '.' )
				return FString(TEXT(""));
			P++;
		}
	}
	if( *P != 0 )
		return FString(TEXT(""));
	const DWORD Addr = ((DWORD)Oct[0]<<24) | ((DWORD)Oct[1]<<16) | ((DWORD)Oct[2]<<8) | (DWORD)Oct[3];
	if( !IsSyntheticAddr( Addr ) )
		return FString(TEXT(""));
	const INT Index = (INT)(Addr & 0xFFFF);
	if( Index < 0 || Index >= GRooms.Num() )
		return FString(TEXT(""));
	return GRooms( Index );
}

void WebRTCLan_SetLocalRoom( const TCHAR* Room )
{
	GLocalRoom = Room ? Room : TEXT("");
}

#endif // __EMSCRIPTEN__

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
