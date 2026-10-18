/*=============================================================================
	FFileManagerLinux.h: Unreal Linux based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( FILE* InFile, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		guard(FArchiveFileReader::FArchiveFileReader);
		fseek( File, 0, SEEK_SET );
		ArIsLoading = ArIsPersistent = 1;
		unguard;
	}
	~FArchiveFileReader()
	{
		guard(FArchiveFileReader::~FArchiveFileReader);
		if( File )
			Close();
		unguard;
	}
	void Precache( INT HintCount )
	{
		guardSlow(FArchiveFileReader::Precache);
		checkSlow(Pos==BufferBase+BufferCount);
		BufferBase = Pos;
		BufferCount = Min( Min( HintCount, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		if( fread( Buffer, BufferCount, 1, File )!=1 && BufferCount!=0 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("fread failed: BufferCount=%i Error=%i"), BufferCount, ferror(File) );
			return;
		}
		unguardSlow;
	}
	void Seek( INT InPos )
	{
		guard(FArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( TEXT("seek Failed %i/%i: %i %i"), InPos, Size, Pos, ferror(File) );
		}
		Pos         = InPos;
		BufferBase  = Pos;
		BufferCount = 0;
		unguard;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileReader::Close);
		if( File )
			fclose( File );
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReader::Serialize);
		while( Length>0 )
		{
			INT Copy = Min( Length, BufferBase+BufferCount-Pos );
			if( Copy==0 )
			{
				if( Length >= ARRAY_COUNT(Buffer) )
				{
					if( fread( V, Length, 1, File )!=1 )
					{
						ArIsError = 1;
						Error->Logf( TEXT("fread failed: Length=%i Error=%i"), Length, ferror(File) );
					}
					Pos += Length;
					BufferBase += Length;
					return;
				}
				Precache( MAXINT );
				Copy = Min( Length, BufferBase+BufferCount-Pos );
				if( Copy<=0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
				}
				if( ArIsError )
					return;
			}
			appMemcpy( V, Buffer+Pos-BufferBase, Copy );
			Pos       += Copy;
			Length    -= Copy;
			V          = (BYTE*)V + Copy;
		}
		unguardSlow;
	}
	// FName file form for HP2's persistent actors (ULevel::Save/LoadPersistentActors write <Map>_pa.usa through a
	// plain file archive), as in FFileManagerWindows.h: an INT byte length including the terminator, then the
	// TCHARs. An implausible length or an unterminated string sets the archive error and yields NAME_None, so the
	// tag loop stops instead of applying garbage.
	FArchive& operator<<( FName& Name )
	{
		guard(FArchiveFileReader<<Name);
		if( ArIsLoading )
		{
			TCHAR Buffer[NAME_SIZE];
			INT   Len = 0;
			Serialize( &Len, sizeof(Len) );
			if( ArIsError || Len < (INT)sizeof(TCHAR) || Len > (INT)sizeof(Buffer) || Len % sizeof(TCHAR) )
			{
				ArIsError = 1;
				Name = NAME_None;
				return *this;
			}
			Serialize( Buffer, Len );
			if( ArIsError || Buffer[Len/sizeof(TCHAR)-1] != 0 )
			{
				ArIsError = 1;
				Name = NAME_None;
				return *this;
			}
			Name = FName( Buffer );
		}
		return *this;
		unguard;
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	BYTE			Buffer[1024];
};
class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( FILE* InFile, FOutputDevice* InError )
	:	File		(InFile)
	,	Error		( InError )
	,	Pos			(0)
	,	BufferCount	(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		guard(FArchiveFileWriter::~FArchiveFileWriter);
		if( File )
			Close();
		File = NULL;
		unguard;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("SeekFailed",TEXT("Core")) );
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileWriter::Close);
		Flush();
		if( File && fclose( File ) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Copy;
		while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
		{
			appMemcpy( Buffer+BufferCount, V, Copy );
			BufferCount += Copy;
			Length      -= Copy;
			V            = (BYTE*)V + Copy;
			Flush();
		}
		if( Length )
		{
			appMemcpy( Buffer+BufferCount, V, Length );
			BufferCount += Length;
		}
	}
	// The writer half of FArchiveFileReader's FName form (persistent actors).
	FArchive& operator<<( FName& Name )
	{
		guard(FArchiveFileWriter<<Name);
		if( ArIsSaving )
		{
			TCHAR Buffer[NAME_SIZE];
			appStrncpy( Buffer, *Name, NAME_SIZE );
			INT Len = (appStrlen(Buffer)+1)*sizeof(TCHAR);
			Serialize( &Len, sizeof(Len) );
			Serialize( Buffer, Len );
		}
		return *this;
		unguard;
	}
	void Flush()
	{
		if( BufferCount && fwrite( Buffer, BufferCount, 1, File )!=1 )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		BufferCount=0;
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[4096];
};

class FFileManagerLinux : public FFileManagerGeneric
{
	// Path normalisation for every entry point: HP2 content uses Windows paths in mixed case (HGame.u opens
	// "Cutscenes\\<file>.int" in System/CUTSCENES, music is "..\\music\\<Song>.ogg"). '\\' becomes '/', and when
	// the exact path does not exist each component is resolved case-insensitively against its directory; a
	// component that matches nothing is kept as given (a file about to be created). Positive resolutions are cached.
	class FFixedPath
	{
	public:
		ANSICHAR Path[PATH_MAX];
		FFixedPath( const TCHAR* In ) { FFileManagerLinux::FixPath( In, Path, ARRAY_COUNT(Path) ); }
		operator const ANSICHAR*() const { return Path; }
	};
	static TMap<FString,FString>& FixPathCache()
	{
		static TMap<FString,FString>* Cache = new TMap<FString,FString>;
		return *Cache;
	}
	static void FixPath( const TCHAR* In, ANSICHAR* Out, INT OutSize )
	{
		appStrncpy( Out, TCHAR_TO_ANSI(In), OutSize );
		for( ANSICHAR* C=Out; *C; C++ )
			if( *C=='\\' )
				*C = '/';
		struct stat St;
		if( !*Out || stat( Out, &St )==0 )
			return;
		// The cache allocates through GMalloc, which appInit installs before it creates GConfig.
		const UBOOL CanCache = GConfig!=NULL;
		if( CanCache )
		{
			FString* Cached = FixPathCache().Find( FString(Out) );
			if( Cached )
			{
				appStrncpy( Out, **Cached, OutSize );
				return;
			}
		}
		// Walk the components; Out is rebuilt in place (a resolved name has the same length as the original).
		ANSICHAR Original[PATH_MAX];
		appStrncpy( Original, Out, ARRAY_COUNT(Original) );
		UBOOL Changed = 0;
		ANSICHAR* Start = Out;
		while( *Start=='/' )
			Start++;
		while( *Start )
		{
			ANSICHAR* End = Start;
			while( *End && *End!='/' )
				End++;
			const ANSICHAR Saved = *End;
			*End = 0;
			if( appStrcmp(Start,".")!=0 && appStrcmp(Start,"..")!=0 && stat( Out, &St )!=0 )
			{
				// Look the component up in its parent, case-insensitively.
				ANSICHAR Name[PATH_MAX];
				appStrncpy( Name, Start, ARRAY_COUNT(Name) );
				*Start = 0;
				DIR* Dir = opendir( Start==Out ? "." : Out );
				UBOOL Found = 0;
				if( Dir )
				{
					for( struct dirent* Ent=readdir(Dir); Ent; Ent=readdir(Dir) )
						if( appStricmp( Ent->d_name, Name )==0 )
						{
							appStrcpy( Start, Ent->d_name );
							Found = Changed = 1;
							break;
						}
					closedir( Dir );
				}
				if( !Found )
				{
					// Nothing to fold to: keep the rest verbatim (a new file or directory).
					appStrcpy( Start, Name );
					*End = Saved;
					break;
				}
			}
			*End = Saved;
			Start = *End ? End+1 : End;
		}
		if( Changed && CanCache )
			FixPathCache().Set( Original, Out );
	}
	static UBOOL WildcardMatch( const ANSICHAR* Pattern, const ANSICHAR* Name )
	{
		// Iterative '*'/'?' glob with single-star backtracking; case-insensitive.
		const ANSICHAR *Star=NULL, *Resume=NULL;
		while( *Name )
		{
			if( *Pattern=='*' )
			{
				Star = Pattern++;
				Resume = Name;
			}
			else if( *Pattern=='?' || (*Pattern && appToUpper(*Pattern)==appToUpper(*Name)) )
			{
				Pattern++;
				Name++;
			}
			else if( Star )
			{
				Pattern = Star+1;
				Name = ++Resume;
			}
			else return 0;
		}
		while( *Pattern=='*' )
			Pattern++;
		return *Pattern==0;
	}
	static UBOOL IsDirectory( const ANSICHAR* Path )
	{
		struct stat St;
		return stat( Path, &St )==0 && S_ISDIR(St.st_mode);
	}
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerLinux::CreateFileReader);
		FILE* File = fopen(FFixedPath(Filename), TCHAR_TO_ANSI(TEXT("rb")));
		if( !File )
		{
			if( Flags & FILEREAD_NoFail )
				appErrorf(TEXT("Failed to read file: %s"),Filename);
			return NULL;
		}
		fseek( File, 0, SEEK_END );
		return new(TEXT("LinuxFileReader"))FArchiveFileReader(File,Error,ftell(File));
		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerLinux::CreateFileWriter);
		FFixedPath Fixed( Filename );
		if( Flags & FILEWRITE_EvenIfReadOnly )
		{
			chmod(Fixed, S_IRUSR | S_IWUSR);
		}
		if( (Flags & FILEWRITE_NoReplaceExisting) && FileSize(Filename)>=0 )
			return NULL;
		const TCHAR* Mode = (Flags & FILEWRITE_Append) ? TEXT("ab") : TEXT("wb"); 
		FILE* File = fopen(Fixed,TCHAR_TO_ANSI(Mode));
		if( !File )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to write: %s"), Filename );
			return NULL;
		}
		if( Flags & FILEWRITE_Unbuffered )
			setvbuf( File, 0, _IONBF, 0 );
		return new(TEXT("LinuxFileWriter"))FArchiveFileWriter(File,Error);
		unguard;
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		guard(FFileManagerLinux::Delete);
		FFixedPath Fixed( Filename );
		if( EvenReadOnly )
		{
			chmod(Fixed, S_IRUSR | S_IWUSR);
		}
		return unlink(Fixed)==0 || (errno==ENOENT && !RequireExists);
		unguard;
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		guard(FFileManagerLinux::MakeDirectory);
		if( Tree )
			return FFileManagerGeneric::MakeDirectory( Path, Tree );
#ifdef PLATFORM_WIN32
		// The launcher and ucc use this manager on Windows too; mkdir takes no mode there.
		return mkdir(FFixedPath(Path))==0 || errno==EEXIST;
#else
		return mkdir(FFixedPath(Path), S_IRUSR | S_IWUSR | S_IXUSR)==0 || errno==EEXIST;
#endif
		unguard;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		guard(FFileManagerLinux::DeleteDirectory);
		if( Tree )
			return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );
		return rmdir(FFixedPath(Path))==0 || (errno==ENOENT && !RequireExists);
		unguard;
	}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		guard(FFileManagerLinux::FindFiles);
		TArray<FString> Result;
	
		DIR *Dirp;
		struct dirent* Direntp;
		char Path[PATH_MAX];
		char File[PATH_MAX];
		char *Filestart;
		char *Cur;
		UBOOL Match;

		// Initialize Path to Filename; FixPath also turns '\\' into '/' and folds the directory part's case.
		FixPath( Filename, Path, ARRAY_COUNT(Path) );
	
		// Separate path and filename.
		Filestart = Path;
		for( Cur = Path; *Cur != '\0'; Cur++ )
			if( *Cur == '/' )
				Filestart = Cur + 1;

		// Store filename and remove it from Path.
		appStrcpy( File, Filestart );
		*Filestart = '\0';

		// Check for empty path.
		if (appStrlen( Path ) == 0)
			appSprintf( Path, "./" );

		// Open directory, get first entry.
		Dirp = opendir( Path );
		if (Dirp == NULL)
				return Result;
		Direntp = readdir( Dirp );

		// Check each entry.
		while( Direntp != NULL )
		{
			// A real wildcard match like Win32 FindFirstFile ('*' and '?' anywhere, case-insensitive, "*.*" =
			// everything): SavePersistentActorCache copies the "*_pa.*" files into the save slot by pattern.
			Match = appStrcmp( File, "*.*" ) == 0 || WildcardMatch( File, Direntp->d_name );

			// Honour the Files/Directories flags and skip "." and "..".
			if( Match && (!appStrcmp(Direntp->d_name,".") || !appStrcmp(Direntp->d_name,"..")) )
				Match = false;
			if( Match && !(Files && Directories) )
			{
				char Full[PATH_MAX];
				appStrncpy( Full, Path, ARRAY_COUNT(Full) );
				appStrncat( Full, Direntp->d_name, ARRAY_COUNT(Full) );
				const UBOOL Dir = IsDirectory( Full );
				Match = Dir ? Directories : Files;
			}

			// Does this entry match the Filename?
			if( Match )
			{
				// Yes, add the file name to Result.
				new(Result)FString(Direntp->d_name);
			}
		
			// Get next entry.
			Direntp = readdir( Dirp );
		}

		// Close directory.
		closedir( Dirp );

		return Result;
		unguard;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		guard(FFileManagerLinux::SetDefaultDirectory);
		return chdir(FFixedPath(Filename))==0;
		unguard;
	}
	FString GetDefaultDirectory()
	{
		guard(FFileManagerLinux::GetDefaultDirectory);
		{
			ANSICHAR Buffer[1024]="";
			if( getcwd( Buffer, ARRAY_COUNT(Buffer) ) == NULL )
			{
				// Fallback to current directory string if getcwd fails.
				appStrcpy( Buffer, "." );
			}
			return appFromAnsi( Buffer );
		}
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
