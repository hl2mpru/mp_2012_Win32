#include "cbase.h"
#include "filesystem.h"
#include "utlvector.h"
#include "utlsymbol.h"
#include "networkstringtable_gamedll.h"
#include "mapentities_shared.h"
#include "toolframework/iserverenginetools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IFileSystem *filesystem;

CUtlSymbolTable m_AlreadyWrittenFileNames;
INetworkStringTable *m_pStringTable;

void OnResourcePrecached(const char *relativePathFileName);

CUtlSymbolTable m_FileNamesInMod;

void OnResourcePrecachedFullPath( char *fullPathFileName, const char *relativeFileName )
{
	Q_FixSlashes( fullPathFileName );
	
	if ( !filesystem->FileExists( fullPathFileName ) )
		return;	// don't allow files for download the server doesn't have

	char m_gameDir[256];
	engine->GetGameDir( m_gameDir, sizeof( m_gameDir ) );
	if ( Q_strncasecmp( m_gameDir, fullPathFileName, Q_strlen( m_gameDir ) ) )
		return;	// the game dir must be part of the full name

	char path[_MAX_PATH];
	Q_snprintf(path, sizeof(path), "%s.bsp", STRING( gpGlobals->mapname ) );
	if ( Q_strstr( fullPathFileName, path ) )
		return;

	// make sure the filename hasn't already been written
	UtlSymId_t filename = m_AlreadyWrittenFileNames.Find( fullPathFileName );
	if ( filename != UTL_INVAL_SYMBOL )
		return;

	// record in list, so we don't write it again
	m_AlreadyWrittenFileNames.AddString( fullPathFileName );

	// add extras for mdl's
	if( Q_strstr( relativeFileName, ".mdl" ) ) {
		// it's a model, get it's other files as well
		char file[_MAX_PATH];
		Q_strncpy(file, relativeFileName, sizeof(file) - 10);
		char *ext = Q_strstr(file, ".mdl");

		Q_strncpy(ext, ".vvd", 10);
		OnResourcePrecached(file);

		Q_strncpy(ext, ".ani", 10);
		OnResourcePrecached(file);

		Q_strncpy(ext, ".dx80.vtx", 10);
		OnResourcePrecached(file);

		Q_strncpy(ext, ".dx90.vtx", 10);
		OnResourcePrecached(file);

		Q_strncpy(ext, ".sw.vtx", 10);
		OnResourcePrecached(file);

		Q_strncpy(ext, ".phy", 10);
		OnResourcePrecached(file);

		Q_strncpy(ext, ".jpg", 10);
		OnResourcePrecached(file);
	}

	//Msg("AddString %s\n", relativeFileName );

	if ( m_pStringTable )
		m_pStringTable->AddString( true, relativeFileName );
}

void OnResourcePrecached(const char *relativePathFileName)
{
	// ignore empty string
	if (relativePathFileName[0] == 0)
		return;

	// ignore files that start with '*' since they signify special models
	if (relativePathFileName[0] == '*')
		return;

	char fullPath[_MAX_PATH];
	if (filesystem->GetLocalPath(relativePathFileName, fullPath, sizeof(fullPath)))
		OnResourcePrecachedFullPath( fullPath, relativePathFileName);
}

void AddFilesToDownload( const char *fName ) 
{
	if ( !filesystem->FileExists( fName, "MOD" ) )
		return;

	char buf[2048];

	FileHandle_t fp = filesystem->Open( fName, "r", "MOD" ); 

	while( filesystem->ReadLine( buf, sizeof( buf ), fp ) ) {
		Q_StripPrecedingAndTrailingWhitespace( buf );
		
		if (buf[0] == 0)
			continue;

		OnResourcePrecached( buf );
	}

	filesystem->Close( fp ); 
}

void OnResourcePrecachedCustomMDL( const char *file ) {
	if( Q_strstr( file, ".mdl" ) ) {
		char newName[256];
		V_FileBase( file, newName, sizeof( newName ) );
		for ( int i = 0; i < m_FileNamesInMod.GetNumStrings(); i++ ) {
			if( Q_strstr( m_FileNamesInMod.String( i ), newName ) ) {
				OnResourcePrecached( m_FileNamesInMod.String( i ) );
			}
		}
	}
}

void ParseKeyValue( const char *file, const char *section )
{
	KeyValues *main = new KeyValues("EntityList");
	main->LoadFromFile( filesystem, file, "MOD" );

	KeyValues *entry = main->FindKey( section );
	if( entry != NULL ) {
		for( KeyValues *pKV = entry->GetFirstSubKey(); pKV != NULL; pKV = pKV->GetNextKey() ) {
			const char *szValue = pKV->GetString("model");
			if( szValue[0] != '\0' ) {
				OnResourcePrecachedCustomMDL( szValue );
				//OnResourcePrecached( pKV->GetString("model") );
			}
			
			szValue =  pKV->GetString("classname");
			if( szValue[0] != '\0' )
				AddFilesToDownload( UTIL_VarArgs( "configs/fastdl/custom/%s.txt", szValue ) );

		}
	}

	main->deleteThis();
}

void RecursiveFindFiles( char const *current, char const *pathID )
{
	FileFindHandle_t fh;
	char path[ 512 ];
	if ( current[ 0 ] )
        Q_snprintf( path, sizeof( path ), "%s/*.*", current );
	else
		Q_snprintf( path, sizeof( path ), "*.*" );

	Q_FixSlashes( path );

	char const *fn = g_pFullFileSystem->FindFirstEx( path, pathID, &fh );
	if ( fn ) {
		do {
			if ( fn[0] != '.' && Q_strnicmp( fn, "custom", 7 ) ) {
				if ( g_pFullFileSystem->FindIsDirectory( fh ) ) {
					char nextdir[ 512 ];
					if ( current[ 0 ] )
						Q_snprintf( nextdir, sizeof( nextdir ), "%s/%s", current, fn );
					else
						Q_snprintf( nextdir, sizeof( nextdir ), "%s", fn );

					RecursiveFindFiles( nextdir, pathID );
				}
				else {
					char ext[ 10 ];
					Q_ExtractFileExtension( fn, ext, sizeof( ext ) );

					if ( !Q_stricmp( ext, "mdl" ) ) {
						char relative[ 512 ];
						if ( current[ 0 ] )
							Q_snprintf( relative, sizeof( relative ), "%s/%s", current, fn );
						else
							Q_snprintf( relative, sizeof( relative ), "%s", fn );

						//Msg( "Found '%s/%s'\n", current, fn );

						//Q_FixSlashes( relative );

						m_FileNamesInMod.AddString( relative );
					}
				}
			}

			fn = g_pFullFileSystem->FindNext( fh );

		} while ( fn );

		g_pFullFileSystem->FindClose( fh );
	}
}

void OnLevelLoadStart( const char *levelName, const char *pMapData )
{
	// reset the duplication list
	m_AlreadyWrittenFileNames.RemoveAll();

	m_pStringTable = networkstringtable->FindTable( "downloadables" );

	AddFilesToDownload( "configs/fastdl/default.txt" );

	AddFilesToDownload( UTIL_VarArgs( "maps/%s.fastdl", levelName ) );

	OnResourcePrecached( UTIL_VarArgs( "maps/%s_particles.txt", levelName ) );

	m_FileNamesInMod.RemoveAll();
	RecursiveFindFiles("", "MOD");

	//��������
	ParseKeyValue( "configs/merchant.txt", levelName );

	//RND NPC DEFAULT
	ParseKeyValue( "rndnpc.txt", "npclist" );

	//RND NPC MAP
	ParseKeyValue( UTIL_VarArgs( "maps/%s_rndnpc.txt", levelName ), "npclist" );

	//������ � �����
	char szTokenBuffer[MAPKEY_MAXLENGTH];

	for ( ; true; pMapData = MapEntity_SkipToNextEntity(pMapData, szTokenBuffer) ) {
		char token[MAPKEY_MAXLENGTH];
		pMapData = MapEntity_ParseToken( pMapData, token );

		if (!pMapData)
			break;

		if (token[0] != '{') {
			Error( "MapEntity_ParseAllEntities: found %s when expecting {", token);
			continue;
		}

		CEntityMapData entData( (char*)pMapData );
		char szValue[MAPKEY_MAXLENGTH];
		
		if (entData.ExtractValue("classname", szValue))
			AddFilesToDownload( UTIL_VarArgs( "configs/fastdl/custom/%s.txt", szValue ) );

		if (entData.ExtractValue("NPCType", szValue))
			AddFilesToDownload( UTIL_VarArgs( "configs/fastdl/custom/%s.txt", szValue ) );

		if (entData.ExtractValue("model", szValue)) {
			OnResourcePrecachedCustomMDL( szValue );
			//OnResourcePrecached( className );
		}

		if (entData.ExtractValue("texture", szValue)) {
			OnResourcePrecached( UTIL_VarArgs( "materials/%s.vmt", szValue ) );
			OnResourcePrecached( UTIL_VarArgs( "materials/%s.vtf", szValue ) );
		}
	}
}
