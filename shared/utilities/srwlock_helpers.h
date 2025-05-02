#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

class CSRWLockScope
{
public:
	CSRWLockScope( PSRWLOCK pLock = nullptr ) :
		Lock( SRWLOCK_INIT ), bAcquired( false )
	{
		// if pLock is null, initialize the lock
		if ( pLock != nullptr )
			Lock = *pLock;
		else
			// @test: some sources suggest that this is not necessary as SRWLocks are automatically initialized, but i'm not sure so just to be safe, we initialize it
			::InitializeSRWLock( &Lock );

		bAcquired = ::TryAcquireSRWLockExclusive( &Lock );
	}

	~CSRWLockScope( )
	{
		// @test: is this if necessary?
		if ( bAcquired )
			::ReleaseSRWLockExclusive( &Lock );
	}

	// remove move and copy semantics
	CSRWLockScope( const CSRWLockScope& ) = delete;
	CSRWLockScope( CSRWLockScope&& ) = delete;
	CSRWLockScope& operator=( const CSRWLockScope& ) = delete;
	CSRWLockScope& operator=( CSRWLockScope&& ) = delete;

	explicit operator bool( ) const
	{
		return bAcquired;
	}

private:
	SRWLOCK Lock = SRWLOCK_INIT;
	BOOLEAN bAcquired = false;
};

class CSRWLockGuard
{
public:
	explicit CSRWLockGuard( SRWLOCK* pLock ) :
		pLock( pLock )
	{
		::AcquireSRWLockExclusive( pLock );
	}

	~CSRWLockGuard( )
	{
		::ReleaseSRWLockExclusive( pLock );
	}

	// remove move and copy semantics
	CSRWLockGuard( const CSRWLockGuard& ) = delete;
	CSRWLockGuard( CSRWLockGuard&& ) = delete;

private:
	PSRWLOCK pLock = nullptr;
};
