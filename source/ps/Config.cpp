// Last modified: 22 November 2003 (Mark Thompson)

// TODO: A few changes from VFS -> CFile usage if required.
// TODO: Optimizations, when we've decided what needs to be done.

#include "Config.h"
#include "res/res.h"

using namespace std;

DEFINE_ERROR( PS_FILE_NOT_FOUND, "A data file required by the engine could \
not be found. Check that it exists within the game directory or archive tree." );
DEFINE_ERROR( PS_FILE_LOAD_FAILURE, "One or more data files required by the \
engine could not be loaded. These files may have been deleted or corrupted." );
DEFINE_ERROR( PS_FILE_NODYNAMIC, "A data file was modified during execution, \
but the engine cannot make one or more of these alterations while the game is \
running." );

//--------------------------------------------------------
// SConfigData: Internal file representation
//--------------------------------------------------------

SConfigData::SConfigData( CStr _Filename, void* _Data, LoaderFunction _DynamicLoader, bool _Static )
{
	Filename = _Filename;
	Data = _Data;
	DynamicLoader = _DynamicLoader;
	Static = _Static;
	Timestamp = TIME_UNREGISTERED;
}

//--------------------------------------------------------
// CConfig: Dynamic Data Manager (singleton)
//--------------------------------------------------------

//--------------------------------------------------------
// CConfig::CConfig()
//--------------------------------------------------------

CConfig::CConfig()
{
	Clear();
	Attach( NULL );
	i = m_FileList.begin();
}

//--------------------------------------------------------
// CConfig::Register()
//
//  Add a file to the registered list.
//--------------------------------------------------------

PS_RESULT CConfig::Register( CStr Filename, void* Data, LoaderFunction DynamicLoader, bool Static )
{
	assert( DynamicLoader != NULL );

	if( m_LogFile )
	{
		CStr Report = _T( "Adding file: " );
		Report += Filename;
		m_LogFile->WriteText( (const TCHAR*)Report );
	}
	
	// Might as well check we can find the thing.
	// This could need changing if vfs_stat doesn't
	// search archives...

	struct stat dy;

	if( vfs_stat( Filename, &dy ) )
	{
		if( m_LogFile )
		{
			CStr Error = _T( "File not found on: " );
			Error += Filename;
			m_LogFile->WriteError( (const TCHAR*)Error );
		}
		return( PS_FILE_NOT_FOUND );
	}
	
	m_FileList.push_back( SConfigData( Filename, Data, DynamicLoader, Static ) );

	i = m_FileList.begin();

	if( !Static )
		return( PS_OK ); 

	// Load static files at the point of registration.

	PS_RESULT Result;
	if( ( Result = DynamicLoader( Filename, Data ) ) != PS_OK )
	{
		if( m_LogFile )
		{
			CStr Error = _T( "Load failed on: " );
			Error += Filename;
			Error += CStr( "Load function returned: " );
			Error += CStr( Result );
			m_LogFile->WriteError( (const TCHAR*)Error );
		}
	}
	return( Result );
}

//--------------------------------------------------------
// CConfig::Update()
//
//  Check timestamps of files and reload as required.
//--------------------------------------------------------

PS_RESULT CConfig::Update()
{
	int slice = 0;
	int failed = 0;
	struct stat FileInfo;

	for( slice = 0; ( i != m_FileList.end() ) && ( slice < CONFIG_SLICE ); i++ )
	{
		// Ignore static files

		if( i->Static )
			continue;
		slice++;

		// TODO: CFile change on following line.

		if( vfs_stat( i->Filename, &FileInfo ) )
		{
			// We can't find the file.
			// If VFS ends up implemented in such a way as vfs_stat doesn't
			// search archives, the following code is needed...
			/*
			if( i->Timestamp )
			{
				// And it's already been loaded once, don't do so again.
				continue;
			}
			// == TIME_UNREGISTERED. Load it, and set the modified date
			// to now so that if it does turn up later on with a time
			// after the start of the program, it will get loaded.
			i->Timestamp = time( NULL );
			*/
			// Otherwise;
			failed++;
			if( m_LogFile )
			{
				CStr Error = _T( "File not found on: " );
				Error += i->Filename;
				m_LogFile->WriteError( (const TCHAR*)Error );
			}
			continue;
		}
		else
		{
			if( i->Timestamp == FileInfo.st_mtime )
			{
				// This file has the same modification time as it did last
				// time we checked.
				continue;
			}
			i->Timestamp = FileInfo.st_mtime;
		}
		// If we reach here, the file needs to be (re)loaded.
		
		// Note also that polling every frame via _stat() for a file which 
		// either does not exist (or exists only in an archive) could be a 
		// considerable waste of time, but if not done the game won't pick
		// up on modified versions of archived files moved into the main
		// directory trees. Also, alternatives to polling don't tend to be
		// portable.

		slice--; 
		
		// Reloaded files do not count against the slice quota.

		if( m_LogFile )
		{
			CStr Report = _T( "Reloading file: " );
			Report += i->Filename;
			m_LogFile->WriteText( (const TCHAR*)Report );
		}

		PS_RESULT Result;
		if( ( Result = i->DynamicLoader( i->Filename, i->Data ) ) != PS_OK )
		{
			if( m_LogFile )
			{
				CStr Error = _T( "Load failed on: " );
				Error += CStr( i->Filename );
				Error += CStr( "Load function returned: " );
				Error += CStr( Result );
				m_LogFile->WriteError( (const TCHAR*)Error );
			}
			failed++;
			if( Result != PS_FILE_NODYNAMIC )
				return( PS_FILE_LOAD_FAILURE ); // Oops. Serious problem, bail.
		}
	}
	if( i == m_FileList.end() ) i = m_FileList.begin();
	if( failed )
		return( PS_FILE_NODYNAMIC );
	return( PS_OK );
}

//--------------------------------------------------------
// CConfig::ReloadAll()
//
//  Reloads all files.
//--------------------------------------------------------

PS_RESULT CConfig::ReloadAll()
{
	// Mostly identical to Update(), above.
	int failed = 0;
	int notfound = 0; 
	struct stat FileInfo;

	for( i = m_FileList.begin(); i != m_FileList.end(); i++ )
	{
		// TODO: CFile change on following line.
		
		if( vfs_stat( i->Filename, &FileInfo ) )
		{
			// We can't find the file.
			// Next block may need to be uncommented if VFS_stat 
			// doesn't search archives in the final ver.
			/*
			char filepath[PATH_MAX];
			if( vfs_realpath( i->Filename, filepath ) )
			{
				// Oops.
				notfound++;
				if( m_LogFile )
				{
					CStr Error = _T( "File not found on: " );
					Error += i->Filename;
					m_LogFile->WriteError( (const TCHAR*)Error );
				}
				continue;
			}
			i->Filename = CStr( filepath );
			i->Timestamp = time( NULL );
			*/
			notfound++;
			if( m_LogFile )
			{
				CStr Error = _T( "File not found on: " );
				Error += i->Filename;
				m_LogFile->WriteError( (const TCHAR*)Error );
			}
			continue;
		}
		else
		{
			i->Timestamp = FileInfo.st_mtime;
		}

		// And load them all again...

		if( m_LogFile )
		{
			CStr Report = _T( "Reloading file: " );
			Report += i->Filename;
			m_LogFile->WriteText( (const TCHAR*)Report );
		}

		PS_RESULT Result;
		if( ( Result = i->DynamicLoader( i->Filename, i->Data ) ) != PS_OK )
		{
			if( m_LogFile )
			{
				CStr Error = _T( "Load failed on: " );
				Error += i->Filename;
				Error += CStr( "Load function returned: " );
				Error += CStr( Result );
				m_LogFile->WriteError( (const TCHAR*)Error );
			}
			failed++;
			if( Result != PS_FILE_NODYNAMIC )
				return( PS_FILE_LOAD_FAILURE ); // Oops. Serious problem, bail.
		}
	}

	i = m_FileList.begin();

	if( notfound )
		return( PS_FILE_NOT_FOUND );
	if( failed )
		return( PS_FILE_NODYNAMIC );
	return( PS_OK );
}

//--------------------------------------------------------
// CConfig::Clear()
//
//  Erases registered and static lists.
//--------------------------------------------------------

void CConfig::Clear()
{
	m_FileList.clear();
}

//--------------------------------------------------------
// CConfig::Attach()
//
//  Attaches (or detaches, with a NULL argument) a logfile class.
//--------------------------------------------------------

void CConfig::Attach( CLogFile* LogFile )
{
	m_LogFile = LogFile;
}
