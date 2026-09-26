/*=============================================================================
	VorbisLayout.c: libvorbisfile's own view of its main struct, for the driver's layout guard.

	Compiled in the NOpenALDrvVorbisLayout target, which (like libogg/libvorbis) is built WITHOUT the
	engine's -fpack-struct=4, i.e. exactly as libvorbisfile sees OggVorbis_File. NOpenALDrv.cpp includes the
	same header under #pragma pack(push,8) and compares sizeof() in Init (template-drivers.md §11.2 A5): a
	mismatch means ov_open would write past the object the driver allocated.
=============================================================================*/

#include <vorbis/vorbisfile.h>

int NOpenALDrv_VorbisFileSize( void )
{
	return (int)sizeof(OggVorbis_File);
}
