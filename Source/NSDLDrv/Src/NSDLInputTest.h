/*=============================================================================
	NSDLInputTest.h: the input test channel (-INPUTTEST=<file> on the command line).

	A test harness appends command lines to <file>; the viewport runs them as they arrive, at the start of
	each input tick. Pad commands drive a virtual game controller made with SDL's virtual joystick API, which
	takes the client's pad place, so everything from SDL's game controller onwards is the real path. Commands:
	  pad attach | pad detach
	  pad button <sdl button name> <0|1>    a b x y back guide start leftstick rightstick leftshoulder
	                                        rightshoulder dpup dpdown dpleft dpright
	  pad axis <sdl axis name> <value>      leftx lefty rightx righty: -32768..32767 (y down is +);
	                                        lefttrigger righttrigger: 0..32767
	  exec <console command>                run on the viewport
	  watch <ticks> <tag>                   one "NSDLInputTest: watch <tag> <i> <seconds> <yaw> <pitch> <speed>
	                                        <x> <y> <vx> <vy> <ax> <ay> <walking>" line per input tick, for <ticks>
	                                        ticks (yaw, pitch: the pawn's rotation, which Harry moves along; speed:
	                                        the horizontal one; x, y: the location; v, a: the velocity and the
	                                        acceleration; walking: bIsWalking)
	  log <text>                            a marker line
	Without the switch nothing here runs.
=============================================================================*/

#include <stdio.h>

class FNSDLInputTest
{
public:
	FNSDLInputTest( const TCHAR* InPath );
	~FNSDLInputTest();
	// Runs the commands that have arrived, stopping after one that changes the virtual pad so that every
	// change is seen by at least one event poll.
	void Tick( class UNSDLViewport* Viewport );

private:
	FILE*			File;
	ANSICHAR		Partial[1024];
	INT				PartialLen;
	SDL_Joystick*	Virtual;
	SDL_JoystickID	VirtualID;
	INT				WatchLeft;
	INT				WatchIndex;
	FString			WatchTag;
	UBOOL Execute( UNSDLViewport* Viewport, const TCHAR* Line );
	void Detach();
};
