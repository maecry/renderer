#include "win.h"

// used: __readfsdword
#include <intrin.h>
// used: stringcompare
#include "crt.h"

void* WIN::GetModuleBaseHandle( const wchar_t* wszModuleName )
{
	const _PEB* pPEB = reinterpret_cast< _PEB* >( __readgsqword( 0x60 ) );

	if ( wszModuleName == nullptr )
		return pPEB->ImageBaseAddress;

	::EnterCriticalSection( pPEB->LoaderLock );

	void* pModuleBase = nullptr;
	for ( LIST_ENTRY* pListEntry = pPEB->Ldr->InLoadOrderModuleList.Flink; pListEntry != &pPEB->Ldr->InLoadOrderModuleList; pListEntry = pListEntry->Flink )
	{
		const _LDR_DATA_TABLE_ENTRY* pEntry = CONTAINING_RECORD( pListEntry, _LDR_DATA_TABLE_ENTRY, InLoadOrderLinks );

		if ( pEntry->BaseDllName.Buffer != nullptr && CRT::StringCompare( wszModuleName, pEntry->BaseDllName.Buffer ) == 0 )
		{
			pModuleBase = pEntry->DllBase;
			break;
		}
	}

	::LeaveCriticalSection( pPEB->LoaderLock );

	return pModuleBase;
}

size_t WIN::GetModuleBaseSize( const void* hModuleBase )
{
	const _PEB* pPEB = reinterpret_cast< _PEB* >( __readgsqword( 0x60 ) );

	if ( hModuleBase == nullptr )
		hModuleBase = pPEB->ImageBaseAddress;

	const auto pIDH = static_cast< const IMAGE_DOS_HEADER* >( hModuleBase );
	if ( pIDH->e_magic != IMAGE_DOS_SIGNATURE )
	{
		return 0U;
	}

	const auto pINH = reinterpret_cast< const IMAGE_NT_HEADERS* >( static_cast< const std::uint8_t* >( hModuleBase ) + pIDH->e_lfanew );
	if ( pINH->Signature != IMAGE_NT_SIGNATURE )
	{
		return 0U;
	}

	return pINH->OptionalHeader.SizeOfImage;
}

const wchar_t* WIN::GetModuleBaseFileName( const void* hModuleBase )
{
	const _PEB* pPEB = reinterpret_cast< _PEB* >( __readgsqword( 0x60 ) );

	if ( hModuleBase == nullptr )
		hModuleBase = pPEB->ImageBaseAddress;

	::EnterCriticalSection( pPEB->LoaderLock );

	const wchar_t* wszModuleName = nullptr;
	for ( LIST_ENTRY* pListEntry = pPEB->Ldr->InLoadOrderModuleList.Flink; pListEntry != &pPEB->Ldr->InLoadOrderModuleList; pListEntry = pListEntry->Flink )
	{
		const _LDR_DATA_TABLE_ENTRY* pEntry = CONTAINING_RECORD( pListEntry, _LDR_DATA_TABLE_ENTRY, InLoadOrderLinks );

		if ( pEntry->DllBase == hModuleBase )
		{
			wszModuleName = pEntry->BaseDllName.Buffer;
			break;
		}
	}

	::LeaveCriticalSection( pPEB->LoaderLock );

	return wszModuleName;
}

void* WIN::GetExportAddress( const void* hModuleBase, const char* szProcedureName )
{
	const auto pBaseAddress = static_cast< const std::uint8_t* >( hModuleBase );

	const auto pIDH = static_cast< const IMAGE_DOS_HEADER* >( hModuleBase );
	if ( pIDH->e_magic != IMAGE_DOS_SIGNATURE )
		return nullptr;

	const auto pINH = reinterpret_cast< const IMAGE_NT_HEADERS64* >( pBaseAddress + pIDH->e_lfanew );
	if ( pINH->Signature != IMAGE_NT_SIGNATURE )
		return nullptr;

	const IMAGE_OPTIONAL_HEADER64* pIOH = &pINH->OptionalHeader;
	const std::uintptr_t nExportDirectorySize = pIOH->DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].Size;
	const std::uintptr_t uExportDirectoryAddress = pIOH->DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].VirtualAddress;

	if ( nExportDirectorySize == 0U || uExportDirectoryAddress == 0U )
	{
		return nullptr;
	}

	const auto pIED = reinterpret_cast< const IMAGE_EXPORT_DIRECTORY* >( pBaseAddress + uExportDirectoryAddress );
	const auto pNamesRVA = reinterpret_cast< const std::uint32_t* >( pBaseAddress + pIED->AddressOfNames );
	const auto pNameOrdinalsRVA = reinterpret_cast< const std::uint16_t* >( pBaseAddress + pIED->AddressOfNameOrdinals );
	const auto pFunctionsRVA = reinterpret_cast< const std::uint32_t* >( pBaseAddress + pIED->AddressOfFunctions );

	// Perform binary search to find the export by name
	std::size_t nRight = pIED->NumberOfNames, nLeft = 0U;
	while ( nRight != nLeft )
	{
		// Avoid INT_MAX/2 overflow
		const std::size_t uMiddle = nLeft + ( ( nRight - nLeft ) >> 1U );
		const int iResult = CRT::StringCompare( szProcedureName, reinterpret_cast< const char* >( pBaseAddress + pNamesRVA[ uMiddle ] ) );

		if ( iResult == 0 )
		{
			const std::uint32_t uFunctionRVA = pFunctionsRVA[ pNameOrdinalsRVA[ uMiddle ] ];

			// Check if it's a forwarded export
			if ( uFunctionRVA >= uExportDirectoryAddress && uFunctionRVA - uExportDirectoryAddress < nExportDirectorySize )
			{
				// Forwarded exports are not supported
				break;
			}

			return const_cast< std::uint8_t* >( pBaseAddress ) + uFunctionRVA;
		}

		if ( iResult > 0 )
			nLeft = uMiddle + 1;
		else
			nRight = uMiddle;
	}

	return nullptr;
}

void* WIN::GetImportAddress( const void* hModuleBase, const char* szProcedureName )
{
	const auto pBaseAddress = static_cast< const std::uint8_t* >( hModuleBase );

	const auto pIDH = static_cast< const IMAGE_DOS_HEADER* >( hModuleBase );
	if ( pIDH->e_magic != IMAGE_DOS_SIGNATURE )
		return nullptr;

	const auto pINH = reinterpret_cast< const IMAGE_NT_HEADERS* >( pBaseAddress + pIDH->e_lfanew );
	if ( pINH->Signature != IMAGE_NT_SIGNATURE )
		return nullptr;

	const IMAGE_OPTIONAL_HEADER* pIOH = &pINH->OptionalHeader;
	const std::uintptr_t nImportDirectorySize = pIOH->DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ].Size;
	const std::uintptr_t uImportDirectoryAddress = pIOH->DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ].VirtualAddress;

	if ( nImportDirectorySize == 0U || uImportDirectoryAddress == 0U )
	{
		return nullptr;
	}

	const auto pIID = reinterpret_cast< const IMAGE_IMPORT_DESCRIPTOR* >( pBaseAddress + uImportDirectoryAddress );
	auto pOriginalFirstThunk = reinterpret_cast< const IMAGE_THUNK_DATA* >( pBaseAddress + pIID->OriginalFirstThunk );
	auto pFirstThunk = reinterpret_cast< const IMAGE_THUNK_DATA* >( pBaseAddress + pIID->FirstThunk );

	while ( !( pOriginalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG ) && pOriginalFirstThunk->u1.AddressOfData )
	{
		const auto pImageImport = reinterpret_cast< const IMAGE_IMPORT_BY_NAME* >( pBaseAddress + pOriginalFirstThunk->u1.AddressOfData );

		if ( CRT::StringCompare( szProcedureName, pImageImport->Name ) == 0 )
			return reinterpret_cast< void* >( pFirstThunk->u1.Function );

		++pOriginalFirstThunk;
		++pFirstThunk;
	}

	return nullptr;
}

bool WIN::GetSectionInfo( const void* hModuleBase, const char* szSectionName, UCHAR** ppSectionStart, ULONGLONG* pnSectionSize )
{
	const auto pBaseAddress = static_cast< const std::uint8_t* >( hModuleBase );

	const auto pIDH = static_cast< const IMAGE_DOS_HEADER* >( hModuleBase );
	if ( pIDH->e_magic != IMAGE_DOS_SIGNATURE )
		return false;

	const auto pINH = reinterpret_cast< const IMAGE_NT_HEADERS* >( pBaseAddress + pIDH->e_lfanew );
	if ( pINH->Signature != IMAGE_NT_SIGNATURE )
		return false;

	const IMAGE_SECTION_HEADER* pISH = IMAGE_FIRST_SECTION( pINH );

	// go through all code sections
	for ( WORD i = 0U; i < pINH->FileHeader.NumberOfSections; i++, pISH++ )
	{
		// @test: use case insensitive comparison instead?
		if ( CRT::StringCompareN( szSectionName, reinterpret_cast< const char* >( pISH->Name ), IMAGE_SIZEOF_SHORT_NAME ) == 0 )
		{
			if ( ppSectionStart != nullptr )
				*ppSectionStart = const_cast< std::uint8_t* >( pBaseAddress ) + pISH->VirtualAddress;

			if ( pnSectionSize != nullptr )
				*pnSectionSize = pISH->SizeOfRawData;

			return true;
		}
	}

	return false;
}