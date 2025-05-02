#pragma once
// used: [win] winapi
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>

// used: source_location
#include <source_location>

#include "crt.h"

#pragma region logger_definitions
using LogLevel_t = std::uint8_t;

enum ELogLevel : LogLevel_t
{
	kLogLevel_None = 0,
	kLogLevel_Info,
	kLogLevel_Warning,
	kLogLevel_Error
};

using LogColorFlags_t = std::uint16_t;
enum ELogColorFlags : LogColorFlags_t
{
	kLogColor_Foreground_Blue = FOREGROUND_BLUE,
	kLogColor_Foreground_Green = FOREGROUND_GREEN,
	kLogColor_Foreground_Red = FOREGROUND_RED,
	kLogColor_Foreground_Intensity = FOREGROUND_INTENSITY,
	kLogColor_Foreground_Gray = FOREGROUND_INTENSITY,
	kLogColor_Foreground_Cyan = FOREGROUND_GREEN | FOREGROUND_BLUE,
	kLogColor_Foreground_Magenta = FOREGROUND_RED | FOREGROUND_BLUE,
	kLogColor_Foreground_Yellow = FOREGROUND_RED | FOREGROUND_GREEN,
	kLogColor_Foreground_Black = 0U,
	kLogColor_Foreground_White = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,

	kLogColor_Background_Blue = BACKGROUND_BLUE,
	kLogColor_Background_Green = BACKGROUND_GREEN,
	kLogColor_Background_Red = BACKGROUND_RED,
	kLogColor_Background_Intensity = BACKGROUND_INTENSITY,
	kLogColor_Background_Gray = BACKGROUND_INTENSITY,
	kLogColor_Background_Cyan = BACKGROUND_GREEN | BACKGROUND_BLUE,
	kLogColor_Background_Magenta = BACKGROUND_RED | BACKGROUND_BLUE,
	kLogColor_Background_Yellow = BACKGROUND_RED | BACKGROUND_GREEN,
	kLogColor_Background_Black = 0U,
	kLogColor_Background_White = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,

	/* [internal] */
	kLogColor_Foreground_Default = kLogColor_Background_Black | kLogColor_Foreground_White,
};

#pragma endregion

/*
* LOGGER
* - simple logging system with file and console(colored) output
* used for debugging and fetching values/errors at run-time
* @todo: threadsafe logger
*/

class CBaseLogger
{
public:
	CBaseLogger( ) noexcept = default;
	virtual ~CBaseLogger( ) 
	{
		CloseStream( );
		CloseFile( );
	};

	// support attach/detach console
	bool AttachConsole( const wchar_t* wszTitle = L"[dev] - console" ) noexcept;
	void DetachConsole( ) noexcept;

	// or if the application is a console application, you can use this function to attach the console to the application
	bool SetupStream( ) noexcept;
	void CloseStream( ) noexcept;

	// open/close logger file output
	bool OpenFile( const wchar_t* wszFileName = L"logs.txt" ) noexcept;
	void CloseFile( ) noexcept;

	// write message to console/file
	void WriteMessageEx( const char* szMessage, const std::size_t nMessageLength );
	void WriteMessageW( const wchar_t* wszMessage );

protected:
	// console write stream
	HANDLE hConsoleStream = nullptr;
	// file write stream
	HANDLE hFileStream = nullptr;
};