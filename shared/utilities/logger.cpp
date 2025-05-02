#include "logger.h"

// used: getworkingpath
#include "filehelpers.h"

bool CBaseLogger::AttachConsole( const wchar_t* wszTitle ) noexcept
{
	// allocate memory for console
	if ( ::AllocConsole( ) != TRUE )
		return false;

	if ( !SetupStream( ) )
		return false;



	// @test: unnecessary as fas as we don't use std::cout etc
	if ( ::SetStdHandle( STD_OUTPUT_HANDLE, hConsoleStream ) != TRUE )
		return false;

	// set console window title
	if ( ::SetConsoleTitleW( wszTitle ) != TRUE )
		return false;

	return true;
}

void CBaseLogger::DetachConsole( ) noexcept
{
	CloseStream( );

	// free allocated memory for console
	if ( ::FreeConsole( ) != TRUE )
		return;

	// close console window
	if ( const HWND hConsoleWindow = ::GetConsoleWindow( ); hConsoleWindow != nullptr )
		::PostMessageW( hConsoleWindow, WM_CLOSE, 0U, 0L );
}

bool CBaseLogger::OpenFile( const wchar_t* wszFileName ) noexcept
{
	wchar_t wszFilePath[ MAX_PATH ];
	if ( !FILEHELPERS::GetWorkingPath( wszFilePath ) )
		return false;

	// append filename to path
	CRT::StringCat( wszFilePath, wszFileName );
	if ( !FILEHELPERS::CreatePath( wszFilePath ) )
		return false;

	// open file output stream
	if ( hFileStream = ::CreateFileW( wszFilePath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr ); hFileStream == INVALID_HANDLE_VALUE )
		return false;

	// insert UTF-8 BOM
	::WriteFile( hFileStream, "\xEF\xBB\xBF", 3UL, nullptr, nullptr );

	return true;
}

void CBaseLogger::CloseFile( ) noexcept
{ 
	if ( hFileStream != INVALID_HANDLE_VALUE )
		::CloseHandle( hFileStream );
}

bool CBaseLogger::SetupStream( ) noexcept
{
	// open console output stream
	if ( hConsoleStream = ::CreateFileW( L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr ); hConsoleStream == INVALID_HANDLE_VALUE )
		return false;

	return true;
}

void CBaseLogger::CloseStream( ) noexcept
{
	if ( hConsoleStream != INVALID_HANDLE_VALUE )
		::CloseHandle( hConsoleStream );
}

void CBaseLogger::WriteMessageEx( const char* szMessage, const std::size_t nMessageLength )
{ 
	if ( hConsoleStream != INVALID_HANDLE_VALUE )
		// write message to console
		::WriteConsoleA( hConsoleStream, szMessage, nMessageLength, nullptr, nullptr );


	if ( hFileStream != INVALID_HANDLE_VALUE )
		// write message to file
		::WriteFile( hFileStream, szMessage, nMessageLength, nullptr, nullptr );
}

void CBaseLogger::WriteMessageW( const wchar_t* wszMessage )
{
	const std::size_t nMessageLength = CRT::StringLengthMultiByte( wszMessage );

	char* szMessage = MEM_STACKALLOC( char*, nMessageLength + 1U );
	CRT::StringUnicodeToMultiByte( szMessage, nMessageLength + 1U, wszMessage );

	WriteMessageEx( szMessage, nMessageLength );
}