#include "filehelpers.h"

// used: stringlengthmultibyte, stringmultibytetounicode
#include "crt.h"
// used: heapalloc, heapfree
#include "memory.h"
// used: getmodulefilename
#include "win.h"

#pragma region filehelpers_struct
// filehandle_t and file_t are used to handle file operations, this is not exported to the user

typedef HANDLE FileHandle_t;

struct File_t
{
	static void* LoadFileToMemory( const char* szFileName, size_t* pnOutFileSize, int iPaddingBytes );

	const FileHandle_t Open( const char* szFileName, const DWORD dwDesiredAccess, const DWORD dwShareMode, const LPSECURITY_ATTRIBUTES lpSecurityAttributes, const DWORD dwCreationDisposition, const DWORD dwFlagsAndAttributes, const HANDLE hTemplateFile )
	{
		hFile = CreateFileA( szFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
		return hFile;
	}

	const FileHandle_t Open( const char* szFileName, const DWORD dwDesiredAccess )
	{
		return Open( szFileName, dwDesiredAccess, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	}

	const FileHandle_t Open( const char* szFileName )
	{
		return Open( szFileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	}

	void Close( )
	{
		::CloseHandle( hFile );
	}

	bool Read( void* pBuffer, const DWORD dwToRead, LPDWORD lpRead = nullptr, LPOVERLAPPED lpOverlapped = nullptr )
	{
		return ::ReadFile( hFile, pBuffer, dwToRead, lpRead, nullptr ) == TRUE ? true : false;
	}

	bool Write( const void* pBuffer, const DWORD dwToWrite, LPDWORD lpWritten, LPOVERLAPPED lpOverlapped )
	{
		return ::WriteFile( hFile, pBuffer, dwToWrite, lpWritten, nullptr ) == TRUE ? true : false;
	}

	DWORD GetFileSize( LPDWORD lpFileSizeHigh = nullptr )
	{
		return ::GetFileSize( hFile, lpFileSizeHigh );
	}

	[[nodiscard]] bool IsValid( ) const
	{
		return hFile != INVALID_HANDLE_VALUE;
	}

	FileHandle_t hFile;
};

#pragma endregion

void* FILEHELPERS::LoadFileToMemory( const char* szFileName, size_t* pnOutFileSize, int iPaddingBytes )
{
	CRT_ASSERTION( szFileName != nullptr );
	if ( pnOutFileSize != nullptr )
		*pnOutFileSize = 0;

	File_t file;
	if ( file.Open( szFileName, GENERIC_READ ); !file.IsValid( ) )
		return nullptr;

	const DWORD dwFileSize = file.GetFileSize( );
	if ( dwFileSize == INVALID_FILE_SIZE )
	{
		file.Close( );
		return nullptr;
	}

	void* pData = MEM::HeapAlloc( dwFileSize + iPaddingBytes );
	// this should never happened
	if ( pData == nullptr )
	{
		file.Close( );
		return nullptr;
	}

	DWORD dwReaded = 0;
	if ( !file.Read( pData, dwFileSize, &dwReaded, nullptr ) )
	{
		file.Close( );
		MEM::HeapFree( pData );
		return nullptr;
	}

	if ( iPaddingBytes > 0 )
		CRT::MemorySet( ( void* )( ( ( char* )pData ) + dwFileSize ), 0, static_cast< size_t >( iPaddingBytes ) );

	file.Close( );

	if ( pnOutFileSize != nullptr )
		*pnOutFileSize = dwFileSize;

	return pData;
}

bool FILEHELPERS::GetWorkingPath( wchar_t* wszDestination )
{
	if ( GetModuleFileNameW( nullptr, wszDestination, MAX_PATH ) == 0 )
		return false;

	// remove the module name
	wchar_t* pwszLastSlash = CRT::StringCharR( wszDestination, L'\\' );
	if ( pwszLastSlash != nullptr )
		*pwszLastSlash = L'\0';

	return true;
}

bool FILEHELPERS::CreatePath( const wchar_t* wszPath )
{
	bool bSuccess = true;
	
	if ( !::CreateDirectoryW( wszPath, nullptr ) )
	{
		if ( ::GetLastError( ) != ERROR_ALREADY_EXISTS )
		{
			bSuccess = false;
			CRT_ASSERTION( false && "failed to create default working directory, because one or more intermediate directories don't exist" );
		}
	}

	return bSuccess;
}