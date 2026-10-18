/*=============================================================================
	WebRTCLan.h — the IpDrv-facing face of the synthetic LAN (WebRTCLan.cpp).

	Free of IpDrv types (no AUdpLink, no sockaddr): IpDrv calls into WebRTCDrv,
	never the reverse, so there is no module cycle. The link is identified by an
	opaque id — AUdpLink passes its own `this`.
=============================================================================*/

#ifndef WEBRTCLAN_H
#define WEBRTCLAN_H

/** A UdpLink bound a port. Binding the beacon port is what starts Unreal 1's passive discovery. */
void WebRTCLan_Bind( const void* LinkId, INT Port );

/** A UdpLink went away — drop its pending replies. */
void WebRTCLan_Unbind( const void* LinkId );

/**
 * A UdpLink is sending. Returns TRUE if the synthetic LAN answered or swallowed it. On the web AUdpLink
 * has no socket and never calls sendto(), whatever this returns.
 */
UBOOL WebRTCLan_Send( const void* LinkId, DWORD DestAddr, INT DestPort, const TCHAR* Text );

/**
 * Pop one synthetic packet for this link, to be delivered through the stock eventReceivedText path.
 * Also drives the discovery state machine, so it must be called every tick of a bound link.
 */
UBOOL WebRTCLan_NextReply( const void* LinkId, DWORD* OutAddr, INT* OutPort, FString& OutText );

/** The synthetic dotted address that stands for `Room` (stable for the session). */
FString WebRTCLan_HostForRoom( const TCHAR* Room );

/** Reverse: the room a synthetic address means, or "" if the host is not one of ours. */
FString WebRTCLan_RoomForHost( const TCHAR* Host );

/** Told by the net driver when we are hosting, so our own room is not advertised back to us. */
void WebRTCLan_SetLocalRoom( const TCHAR* Room );

#endif // WEBRTCLAN_H
