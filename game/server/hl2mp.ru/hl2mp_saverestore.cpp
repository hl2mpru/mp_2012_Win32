#include "cbase.h"
#include "saverestore.h"
#include "saverestoretypes.h"
#include "networkstringtable_gamedll.h"
#include "datacache/imdlcache.h"
#include "tier1/utlbuffer.h"
#include "entityapi.h"
#include "gameinterface.h"
#include "tier1/memstack.h"
#include "physics_saverestore.h"
#include "ai_saverestore.h"
#include "ai_basenpc.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	FL_SAVERESTORE_CLIENT_NOTP (1<<0)
#define	FL_SAVERESTORE_NPC_NOTP (1<<1)
#define	FL_SAVERESTORE_NPC_NORESTORE (1<<2)

extern CServerGameDLL g_ServerGameDLL;

class CSaveMemory : public CMemoryStack
{
public:
	CSaveMemory()
	{
		MEM_ALLOC_CREDIT();
		Init( 32*1024*1024, 64, 2*1024*1024 + 192*1024 );
	}

	int m_nSaveAllocs;
};

CSaveMemory &GetSaveMemory()
{
	static CSaveMemory g_SaveMemory;
	return g_SaveMemory;
}

void *SaveAllocMemory( size_t num, size_t size, bool bClear = false )
{
	MEM_ALLOC_CREDIT();
	++GetSaveMemory().m_nSaveAllocs;
	int nBytes = num * size;
	return GetSaveMemory().Alloc( nBytes, bClear );
}

void SaveFreeMemory( void *pSaveMem )
{
	--GetSaveMemory().m_nSaveAllocs;
	if ( !GetSaveMemory().m_nSaveAllocs )
		GetSaveMemory().FreeAll( false );
}

void SaveResetMemory()
{
	GetSaveMemory().m_nSaveAllocs = 0;
	GetSaveMemory().FreeAll( false );
}

struct SaveFileSectionsInfo_t
{
	int nBytesSymbols;
	int nSymbols;
	int nBytesDataHeaders;
	int nBytesData;
	
	int SumBytes() const
	{
		return ( nBytesSymbols + nBytesDataHeaders + nBytesData );
	}
};

struct SaveFileSections_t
{
	char *pSymbols;
	char *pDataHeaders;
	char *pData;
};

struct SaveFileHeaderTag_t
{
	int id;
	int version;
	
	bool operator==(const SaveFileHeaderTag_t &rhs) const { return ( memcmp( this, &rhs, sizeof(SaveFileHeaderTag_t) ) == 0 ); }
	bool operator!=(const SaveFileHeaderTag_t &rhs) const { return ( memcmp( this, &rhs, sizeof(SaveFileHeaderTag_t) ) != 0 ); }
};

const struct SaveFileHeaderTag_t CURRENT_SAVEFILE_HEADER_TAG = { MAKEID('V','A','L','V'), 0x0073 }; 

char const *GetSaveGameMapName( char const *level )
{
	static char mapname[ 256 ];
	Q_FileBase( level, mapname, sizeof( mapname ) );
	return mapname;
}

char *GetSaveDir(void)
{
	if ( !filesystem->FileExists( "save", "MOD" ) )
		filesystem->CreateDirHierarchy( "save", "MOD" );

	static char szDirectory[256];
	Q_memset(szDirectory, 0, 256);
	Q_strncpy(szDirectory, "save/", sizeof( szDirectory ) );
	return szDirectory;
}

void EntityPatchWrite( CSaveRestoreData *pSaveData, const char *level )
{
	char			name[256];
	int				i, size;

	Q_snprintf( name, sizeof( name ), "//%s/%s%s.HL3", "MOD", GetSaveDir(), level);// DON'T FixSlashes on this, it needs to be //MOD

	size = 0;
	for ( i = 0; i < pSaveData->NumEntities(); i++ )
	{
		if ( pSaveData->GetEntityInfo(i)->flags & FENTTABLE_REMOVED )
			size++;
	}

	int nBytesEntityPatch = sizeof(int) + size * sizeof(int);
	void *pBuffer = new byte[nBytesEntityPatch];
	CUtlBuffer buffer( pBuffer, nBytesEntityPatch );

	// Patch count
	buffer.Put( &size, sizeof(int) );
	for ( i = 0; i < pSaveData->NumEntities(); i++ )
	{
		if ( pSaveData->GetEntityInfo(i)->flags & FENTTABLE_REMOVED )
			buffer.Put( &i, sizeof(int) );
	}
	
	filesystem->AsyncWrite( name, pBuffer, nBytesEntityPatch, true, false );
	filesystem->AsyncFinishAllWrites();
}

struct SAVE_HEADER 
{
	DECLARE_SIMPLE_DATADESC();

	int		saveId;
	int		version;
	int		skillLevel;
	int		connectionCount;
	int		lightStyleCount;
	int		mapVersion;
	float	time; // This is renamed to include the __USE_VCR_MODE prefix due to a #define on win32 from the VCR mode changes
								// The actual save games have the string "time__USE_VCR_MODE" in them
	char	mapName[32];
	char	skyName[32];
};

BEGIN_SIMPLE_DATADESC( SAVE_HEADER )

//	DEFINE_FIELD( saveId, FIELD_INTEGER ),
//	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_FIELD( skillLevel, FIELD_INTEGER ),
	DEFINE_FIELD( connectionCount, FIELD_INTEGER ),
	DEFINE_FIELD( lightStyleCount, FIELD_INTEGER ),
	DEFINE_FIELD( mapVersion, FIELD_INTEGER ),
	DEFINE_FIELD( time, FIELD_TIME ),
	DEFINE_ARRAY( mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( skyName, FIELD_CHARACTER, 32 ),
END_DATADESC()

struct SAVELIGHTSTYLE 
{
	DECLARE_SIMPLE_DATADESC();

	int		index;
	char	style[64];
};

BEGIN_SIMPLE_DATADESC( SAVELIGHTSTYLE )
	DEFINE_FIELD( index, FIELD_INTEGER ),
	DEFINE_ARRAY( style, FIELD_CHARACTER, 64 ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( levellist_t )
	DEFINE_ARRAY( mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( landmarkName, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( pentLandmark, FIELD_EDICT ),
	DEFINE_FIELD( vecLandmarkOrigin, FIELD_VECTOR ),
END_DATADESC()

void Finish( CSaveRestoreData *save )
{
	char **pTokens = save->DetachSymbolTable();
	if ( pTokens )
		SaveFreeMemory( pTokens );

	entitytable_t *pEntityTable = save->DetachEntityTable();
	if ( pEntityTable )
		SaveFreeMemory( pEntityTable );

	save->PurgeEntityHash();
	SaveFreeMemory( save );

	gpGlobals->pSaveData = nullptr;

	save->~CSaveRestoreData();
}

void ResetSaveData( void )
{
	FileFindHandle_t popHandle;
	const char *pPopFileName = filesystem->FindFirst( "save/*.hl?", &popHandle );
	while ( pPopFileName && pPopFileName[ 0 ] != '\0' ) {
		if ( filesystem->FindIsDirectory( popHandle ) )
		{
			pPopFileName = filesystem->FindNext( popHandle );
			continue;
		}
		char szBaseName[_MAX_PATH];
		V_snprintf( szBaseName, sizeof( szBaseName ), "save/%s", pPopFileName );
		filesystem->RemoveFile( szBaseName, "MOD" );
		pPopFileName = filesystem->FindNext( popHandle );
	}
	filesystem->FindClose( popHandle );

	ResetGlobalState();

	if( gpGlobals->pSaveData )
		Finish( gpGlobals->pSaveData );

	SaveResetMemory();
}

extern ConVar skill;

void SaveGameStateGlobals( CSaveRestoreData *pSaveData )
{
	SAVE_HEADER header;

	INetworkStringTable * table = networkstringtable->FindTable( "lightstyles" );

	// Write global data
	header.version 			= 9057; //Как правильно? Версия чего?
	header.skillLevel 		= skill.GetInt();	// This is created from an int even though it's a float
	header.connectionCount 	= pSaveData->levelInfo.connectionCount;
	header.time	= gpGlobals->curtime; //Время сохранения?
	ConVarRef skyname( "sv_skyname" );
	if ( skyname.IsValid() )
		Q_strncpy( header.skyName, skyname.GetString(), sizeof( header.skyName ) );
	else
		Q_strncpy( header.skyName, "unknown", sizeof( header.skyName ) );

	Q_strncpy( header.mapName, STRING( gpGlobals->mapname ), sizeof( header.mapName ) );
	header.lightStyleCount 	= 0;
	header.mapVersion = gpGlobals->mapversion;

	int i;
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ ) {
		const char * ligthStyle = (const char*) table->GetStringUserData( i, NULL );
		if ( ligthStyle && ligthStyle[0] )
			header.lightStyleCount++;
	}

	pSaveData->levelInfo.time = 0.0; // prohibits rebase of header.time (why not just save time as a field_float and ditch this hack?)
	g_ServerGameDLL.SaveWriteFields( pSaveData, "Save Header", &header, NULL, SAVE_HEADER::m_DataMap.dataDesc, SAVE_HEADER::m_DataMap.dataNumFields );
	pSaveData->levelInfo.time = header.time;
	
	// Write adjacency list
	for ( i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
		g_ServerGameDLL.SaveWriteFields( pSaveData, "ADJACENCY", pSaveData->levelInfo.levelList + i, NULL, levellist_t::m_DataMap.dataDesc, levellist_t::m_DataMap.dataNumFields );

	// Write the lightstyles
	SAVELIGHTSTYLE	light;
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ ) {
		const char * ligthStyle = (const char*) table->GetStringUserData( i, NULL );

		if ( ligthStyle && ligthStyle[0] ) {
			light.index = i;
			Q_strncpy( light.style, ligthStyle, sizeof( light.style ) );
			g_ServerGameDLL.SaveWriteFields( pSaveData, "LIGHTSTYLE", &light, NULL, SAVELIGHTSTYLE::m_DataMap.dataDesc, SAVELIGHTSTYLE::m_DataMap.dataNumFields );
		}
	}
}

#define VectorToString(v) (static_cast<const char *>(CFmtStr("%f %f %f", (v).x, (v).y, (v).z)))

extern char st_szNextMap[32];

void SaveClientState( CBasePlayer *pPlayer, int pFlags = NULL ) 
{
	if( pPlayer == NULL ) {
		FileFindHandle_t popHandle;
		const char *pPopFileName = filesystem->FindFirst( "save/*.hl2", &popHandle );
		while ( pPopFileName && pPopFileName[ 0 ] != '\0' ) {
			if ( filesystem->FindIsDirectory( popHandle ) ) {
				pPopFileName = filesystem->FindNext( popHandle );
				continue;
			}

			char szBaseName[_MAX_PATH];
			V_snprintf( szBaseName, sizeof( szBaseName ), "save/%s", pPopFileName );
			KeyValuesAD manifest( "ClientSaveRestore" );
			if( !manifest->LoadFromFile( filesystem, szBaseName ) )
				return;

			KeyValues *pClient = manifest->FindKey( "pClient" );
			if( pClient != NULL )
				pClient->SetInt("pFlags", pFlags );

			manifest->SaveToFile( filesystem, szBaseName );
			pPopFileName = filesystem->FindNext( popHandle );
		}
		filesystem->FindClose( popHandle );
		return;
	}

	bool result = false;

	KeyValues *manifest = new KeyValues( UTIL_VarArgs("%llu", pPlayer->GetSteamIDAsUInt64() ) );

	if( st_szNextMap[0] != '\0' )
		manifest->SetString("mapname", st_szNextMap );
	else
		manifest->SetString("mapname", STRING( gpGlobals->mapname ) );

	if( pPlayer->IsAlive() ) {
		result = true;
 
		KeyValues *pClient = new KeyValues("pClient");
		pClient->SetUint64("SteamID64", pPlayer->GetSteamIDAsUInt64() );
		/*if( pPlayer->GetGroundEntity() == NULL ) {
			pClient->SetString("origin", VectorToString( pPlayer->GetAbsOrigin() ) );
			pClient->SetString("angles", VectorToString( pPlayer->GetAbsAngles() ) );
			pClient->SetString("velocity", VectorToString( pPlayer->GetAbsVelocity() ) );
		}*/
		pClient->SetInt("m_iHealth", pPlayer->GetHealth() );
		pClient->SetInt("m_ArmorValue", pPlayer->ArmorValue() );
		pClient->SetInt("m_iFrags", pPlayer->FragCount() );
		pClient->SetInt("m_iDeaths", pPlayer->DeathCount() );
		pClient->SetString("model", STRING( pPlayer->GetModelName() ) );

		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( pPlayer->GetActiveWeapon() );
		if( pWeapon )
			pClient->SetString("weapon_in_hand", pWeapon->GetClassname() );

		manifest->AddSubKey( pClient );

		KeyValues *pWeapons = new KeyValues("pWeapons");
		for (int i=0; i<MAX_WEAPONS; ++i) 
		{
			CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
			if ( !pWeapon )
				continue;

			KeyValues *pKV = new KeyValues( pWeapon->GetClassname() );
			pKV->SetString("classname", pWeapon->GetClassname() );
			pKV->SetString("section", pWeapon->GetSectionName() );

			if( pWeapon->UsesClipsForAmmo1() )
			{
				pKV->SetInt("m_iClip1", pWeapon->Clip1() );
				pKV->SetInt("m_iPrimaryAmmoCount", pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() ) );
			}
			else
			{
				pKV->SetInt("m_iPrimaryAmmoCount", pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() ) );
			}

			if( pWeapon->UsesClipsForAmmo2() )
			{
				pKV->SetInt("m_iClip2", pWeapon->Clip2() );
				pKV->SetInt("m_iSecondaryAmmoCount", pPlayer->GetAmmoCount( pWeapon->GetSecondaryAmmoType() ) );
			}
			else
			{
				pKV->SetInt("m_iSecondaryAmmoCount", pPlayer->GetAmmoCount( pWeapon->GetSecondaryAmmoType() ) );
			}

			pWeapons->AddSubKey( pKV );
		}
		manifest->AddSubKey( pWeapons );
	}

	KeyValues *pEntity = new KeyValues("pEntity");
	CBaseEntity *ent = NULL;
	while ((ent = gEntList.NextEnt(ent)) != NULL)
	{
		if ( !ent->GetPlayerMP() || ent->GetPlayerMP() != pPlayer || Q_stristr( ent->GetClassname(), "weapon_" ) || !ent->IsNPC() )
			continue;

		result = true;

		KeyValues *pKV = new KeyValues( UTIL_VarArgs( "%s_%d", ent->GetClassname(), ENTINDEX( ent ) ) );
		pKV->SetString("classname", ent->GetClassname() );
		pKV->SetString("origin", VectorToString( ent->GetAbsOrigin() ) );
		pKV->SetString("angles", VectorToString( ent->GetAbsAngles() ) );
		pKV->SetString("section", ent->GetSectionName() );
		if( ent->IsNPC() ) {
			CBaseCombatCharacter *pNpc = dynamic_cast<CBaseCombatCharacter*>( ent );
			if( pNpc ) {
				pKV->SetInt("health", ent->GetHealth() );
				pKV->SetInt("max_health", ent->GetMaxHealth() );
				pKV->SetInt("score", ent->GetScore() );

				CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( pNpc->GetActiveWeapon() );
				if( pWeapon )
					pKV->SetString("additionalequipment", pWeapon->GetClassname() );
			}
		}
		pEntity->AddSubKey( pKV );
	}
	manifest->AddSubKey( pEntity );

	if( result )
		manifest->SaveToFile( filesystem, UTIL_VarArgs("save/%llu.hl2", pPlayer->GetSteamIDAsUInt64() ) );

	manifest->deleteThis();
}

void RestoreClientState( CBasePlayer *pPlayer ) {
	if( !pPlayer )
		return;

	KeyValuesAD manifest( UTIL_VarArgs("%llu", pPlayer->GetSteamIDAsUInt64() ) );
	if( !manifest->LoadFromFile( filesystem, UTIL_VarArgs("save/%llu.hl2", pPlayer->GetSteamIDAsUInt64() ) ) )
		return;

	filesystem->RemoveFile( UTIL_VarArgs("save/%llu.hl2", pPlayer->GetSteamIDAsUInt64() ) );

	if( strcmpi( STRING( gpGlobals->mapname ), manifest->GetString("mapname") ) != 0 )
		return;

	unsigned short pFlags = NULL;

	KeyValues *pClient = manifest->FindKey( "pClient" );
	if( pClient != NULL ) {
		pFlags = pClient->GetInt("pFlags");
		/*if( !( pFlags & FL_SAVERESTORE_CLIENT_NOTP ) && pClient->GetString("origin", NULL ) ) {
			Vector origin;
			UTIL_StringToVector( origin.Base(), pClient->GetString("origin") );
			QAngle angles;
			UTIL_StringToVector( angles.Base(), pClient->GetString("angles") );
			Vector velocity;
			UTIL_StringToVector( velocity.Base(), pClient->GetString("velocity") );
			pPlayer->Teleport( &origin, &angles, &velocity );
		}*/

		pPlayer->SetHealth( pClient->GetInt("m_iHealth", 100 ) );
		pPlayer->SetArmorValue( pClient->GetInt("m_ArmorValue") );
		pPlayer->ResetFragCount();
		pPlayer->IncrementFragCount( pClient->GetInt("m_iFrags") );
		pPlayer->ResetDeathCount();
		pPlayer->IncrementDeathCount( pClient->GetInt("m_iDeaths") );

		const char *pModel = pClient->GetString("model", "models/alyx.mdl");
		if ( !engine->IsModelPrecached( pModel ) )
			engine->PrecacheModel( pModel, false );
		
		pPlayer->SetModel( pModel );
	}

	KeyValues *pWeapons = manifest->FindKey("pWeapons");
	if( pWeapons != NULL ) {
		pPlayer->RemoveAllWeapons();
		pPlayer->RemoveAllAmmo();
		for( KeyValues *pKV = pWeapons->GetFirstSubKey(); pKV != NULL; pKV = pKV->GetNextKey() )
		{
			CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>( pPlayer->GiveNamedItem( pKV->GetString("classname") ) );
			if( !pWeapon )
				continue;

			if( pWeapon->UsesClipsForAmmo1() )
			{
				pWeapon->m_iClip1 = pKV->GetInt("m_iClip1");
				pPlayer->SetAmmoCount( pKV->GetInt("m_iPrimaryAmmoCount"), pWeapon->GetPrimaryAmmoType() );
			}
			else
			{
				pWeapon->SetPrimaryAmmoCount( pKV->GetInt("m_iPrimaryAmmoCount") );
				pPlayer->SetAmmoCount( pKV->GetInt("m_iPrimaryAmmoCount"), pWeapon->GetPrimaryAmmoType() );
			}

			if( pWeapon->UsesClipsForAmmo2() )
			{
				pWeapon->m_iClip2 = pKV->GetInt("m_iClip2");
				pPlayer->SetAmmoCount( pKV->GetInt("m_iSecondaryAmmoCount"), pWeapon->GetSecondaryAmmoType() );
			}
			else
			{
				pWeapon->SetSecondaryAmmoCount( pKV->GetInt("m_iSecondaryAmmoCount") );
				pPlayer->SetAmmoCount( pKV->GetInt("m_iSecondaryAmmoCount"), pWeapon->GetSecondaryAmmoType() );
			}

			const char *pSection = pKV->GetString("section");
			if( pSection[0] != '\0') {
				pWeapon->SetSectionName( pSection );
				pWeapon->SetPlayerMP( pPlayer );
			}

			
		}
		if( pClient != NULL && pClient->GetString("weapon_in_hand") ) {
			CBaseCombatWeapon *pWeapon = pPlayer->Weapon_OwnsThisType( pClient->GetString("weapon_in_hand") );
			if( pWeapon )
				pPlayer->Weapon_Switch( pWeapon );
		}
	}

	KeyValues *pEntity = manifest->FindKey("pEntity");
	if( !( pFlags & FL_SAVERESTORE_NPC_NORESTORE ) && pEntity != NULL ) {
		for( KeyValues *pKV = pEntity->GetFirstSubKey(); pKV != NULL; pKV = pKV->GetNextKey() )
		{
			CBaseEntity *pEnt = CreateEntityByName( pKV->GetString("classname") );
			if( !pEnt )
				continue;

			pEnt->KeyValue( "squadname", "merchant" );

			for( KeyValues *sub = pKV->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
			{
				if( FStrEq( sub->GetName(), "classname" ) )
					continue;

				if( FStrEq( sub->GetName(), "origin" ) && pFlags & FL_SAVERESTORE_NPC_NOTP ) {
					pEnt->KeyValue( "origin", VectorToString( pPlayer->GetAbsOrigin() ) );
					continue;
				}

				//Msg("%s %s\n", sub->GetName(), pKV->GetString( sub->GetName() ) );
				pEnt->KeyValue( sub->GetName(), pKV->GetString( sub->GetName() ) );
			}

			pEnt->SetSectionName( pKV->GetString("section") );
			pEnt->SetPlayerMP( pPlayer );

			DispatchSpawn( pEnt );
			pEnt->Activate();

			pEnt->SetCollisionGroup( COLLISION_GROUP_PLAYER );

			if( pEnt->IsNPC() ) {
				INPCInteractive *pInteractive = dynamic_cast<INPCInteractive *>( pEnt );
				if( pInteractive )
					pInteractive->NotifyInteraction( NULL );

				pEnt->SetScore( pKV->GetInt("score") );
				pEnt->SetHealth( pKV->GetInt("health", 100 ) );
				pEnt->SetMaxHealth( pKV->GetInt("max_health", 100 ) );
				pEnt->m_takedamage = DAMAGE_YES;
				pEnt->AddEFlags( EFL_NO_DISSOLVE );
				pEnt->AddSpawnFlags( SF_NPC_FADE_CORPSE );
			}
		}
	}
}

bool SaveGameState( bool bTransition, CSaveRestoreData **ppReturnSaveData )
{
	SaveClientState( NULL, FL_SAVERESTORE_CLIENT_NOTP | FL_SAVERESTORE_NPC_NORESTORE );

	//Удаляем ентити которые в очереди на удаление!
	gEntList.CleanupDeleteList();

	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	int i;
	SaveFileSectionsInfo_t sectionsInfo;
	SaveFileSections_t sections;

	if ( ppReturnSaveData )
		*ppReturnSaveData = NULL;

	/*if ( bTransition )
	{
		if ( m_bClearSaveDir )
		{
			m_bClearSaveDir = false;
			DoClearSaveDir( IsXSave() );
		}
	}*/
	
	CSaveRestoreData *pSaveData = SaveInit( 0 );
	if ( !pSaveData )
	{
		Msg("[hl2mp.ru] SaveGameState: pSaveData == NULL\n");
		return false;
	}

	pSaveData->bAsync = false;

	//---------------------------------
	// Save the data
	sections.pData = pSaveData->AccessCurPos();
	
	//---------------------------------
	// Pre-save
	g_ServerGameDLL.PreSave( pSaveData );

	// Build the adjacent map list (after entity table build by game in presave)
	if ( bTransition )
		pSaveData->levelInfo.connectionCount = BuildChangeList( pSaveData->levelInfo.levelList, MAX_LEVEL_CONNECTIONS );
	else
		pSaveData->levelInfo.connectionCount = 0;

	SaveGameStateGlobals( pSaveData );

	g_ServerGameDLL.Save( pSaveData );
	
	sectionsInfo.nBytesData = pSaveData->AccessCurPos() - sections.pData;

	//---------------------------------
	// Save necessary tables/dictionaries/directories
	sections.pDataHeaders = pSaveData->AccessCurPos();
	
	g_ServerGameDLL.WriteSaveHeaders( pSaveData );
	
	sectionsInfo.nBytesDataHeaders = pSaveData->AccessCurPos() - sections.pDataHeaders;

	//---------------------------------
	// Write the save file symbol table
	sections.pSymbols = pSaveData->AccessCurPos();

	for( i = 0; i < pSaveData->SizeSymbolTable(); i++ ) {
		const char *pszToken = ( pSaveData->StringFromSymbol( i ) ) ? pSaveData->StringFromSymbol( i ) : "";
		if ( !pSaveData->Write( pszToken, strlen(pszToken) + 1 ) )
			break;
	}

	sectionsInfo.nBytesSymbols = pSaveData->AccessCurPos() - sections.pSymbols;
	sectionsInfo.nSymbols = pSaveData->SizeSymbolTable();

	//---------------------------------
	// Output to disk
	char name[256];
	int nBytesStateFile = sizeof(CURRENT_SAVEFILE_HEADER_TAG) + 
		sizeof(sectionsInfo) + 
		sectionsInfo.nBytesSymbols + 
		sectionsInfo.nBytesDataHeaders + 
		sectionsInfo.nBytesData;

	void *pBuffer = new byte[nBytesStateFile];
	CUtlBuffer buffer( pBuffer, nBytesStateFile );

	// Write the header -- THIS SHOULD NEVER CHANGE STRUCTURE, USE SAVE_HEADER FOR NEW HEADER INFORMATION
	// THIS IS ONLY HERE TO IDENTIFY THE FILE AND GET IT'S SIZE.

	buffer.Put( &CURRENT_SAVEFILE_HEADER_TAG, sizeof(CURRENT_SAVEFILE_HEADER_TAG) );

	// Write out the tokens and table FIRST so they are loaded in the right order, then write out the rest of the data in the file.
	buffer.Put( &sectionsInfo, sizeof(sectionsInfo) );
	buffer.Put( sections.pSymbols, sectionsInfo.nBytesSymbols );
	buffer.Put( sections.pDataHeaders, sectionsInfo.nBytesDataHeaders );
	buffer.Put( sections.pData, sectionsInfo.nBytesData );

	Q_snprintf( name, 256, "//%s/%s%s.HL1", "MOD", GetSaveDir(), GetSaveGameMapName( STRING( gpGlobals->mapname) ) ); // DON'T FixSlashes on this, it needs to be //MOD

	Msg("[hl2mp.ru] SaveGameState: Write file %s\n", name);
	filesystem->AsyncWrite( name, pBuffer, nBytesStateFile, true, false );
	filesystem->AsyncFinishAllWrites();
	
	pBuffer = NULL;
	
	EntityPatchWrite( pSaveData, GetSaveGameMapName( STRING( gpGlobals->mapname) ) );
	if ( !ppReturnSaveData )
		Finish( pSaveData );
	else
		*ppReturnSaveData = pSaveData;

	return true;
}

CSaveRestoreData *LoadSaveData( const char *level )
{
	char			name[260];
	FileHandle_t	pFile;

	Q_snprintf( name, sizeof( name ), "//%s/%s%s.HL1", "MOD", GetSaveDir(), level);// DON'T FixSlashes on this, it needs to be //MOD
	Msg ("[hl2mp.ru] Loading game from %s...\n", name);

	pFile = filesystem->Open( name, "rb" );
	if (!pFile) {
		Msg ("[hl2mp.ru] ERROR: couldn't open.\n");
		return NULL;
	}

	//---------------------------------
	// Read the header
	SaveFileHeaderTag_t tag;
	if ( filesystem->Read( &tag, sizeof(tag), pFile ) != sizeof(tag) )
		return NULL;

	// Is this a valid save?
	if ( tag != CURRENT_SAVEFILE_HEADER_TAG )
		return NULL;

	//---------------------------------
	// Read the sections info and the data
	//
	SaveFileSectionsInfo_t sectionsInfo;
	
	if ( filesystem->Read( &sectionsInfo, sizeof(sectionsInfo), pFile ) != sizeof(sectionsInfo) )
		return NULL;

	void *pSaveMemory = SaveAllocMemory( sizeof(CSaveRestoreData) + sectionsInfo.SumBytes(), sizeof(char) );
	if ( !pSaveMemory )
		return 0;

	CSaveRestoreData *pSaveData = MakeSaveRestoreData( pSaveMemory );
	Q_strncpy( pSaveData->levelInfo.szCurrentMapName, level, sizeof( pSaveData->levelInfo.szCurrentMapName ) );
	
	if ( filesystem->Read( (char *)(pSaveData + 1), sectionsInfo.SumBytes(), pFile ) != sectionsInfo.SumBytes() ) {
		// Free the memory and give up
		Finish( pSaveData );
		return NULL;
	}

	filesystem->Close( pFile );
	
	//---------------------------------
	// Parse the symbol table
	char *pszTokenList = (char *)(pSaveData + 1);// Skip past the CSaveRestoreData structure

	if ( sectionsInfo.nBytesSymbols > 0 )
	{
		pSaveMemory = SaveAllocMemory( sectionsInfo.nSymbols, sizeof(char *), true );
		if ( !pSaveMemory )
		{
			SaveFreeMemory( pSaveData );
			return NULL;
		}

		pSaveData->InitSymbolTable( (char**)pSaveMemory, sectionsInfo.nSymbols );

		// Make sure the token strings pointed to by the pToken hashtable.
		for( int i = 0; i<sectionsInfo.nSymbols; i++ )
		{
			if ( *pszTokenList )
			{
				Verify( pSaveData->DefineSymbol( pszTokenList, i ) );
			}
			while( *pszTokenList++ );				// Find next token (after next null)
		}
	}
	else
	{
		pSaveData->InitSymbolTable( NULL, 0 );
	}

	Assert( pszTokenList - (char *)(pSaveData + 1) == sectionsInfo.nBytesSymbols );

	//---------------------------------
	// Set up the restore basis
	int size = sectionsInfo.SumBytes() - sectionsInfo.nBytesSymbols;

	pSaveData->levelInfo.connectionCount = 0;
	pSaveData->Init( (char *)(pszTokenList), size );	// The point pszTokenList was incremented to the end of the tokens
	pSaveData->levelInfo.fUseLandmark = true;
	pSaveData->levelInfo.time = 0;
	VectorCopy( vec3_origin, pSaveData->levelInfo.vecLandmarkOffset );
	gpGlobals->pSaveData = (CSaveRestoreData*)pSaveData;

	return pSaveData;
}

void ParseSaveTables( CSaveRestoreData *pSaveData, SAVE_HEADER *pHeader, int updateGlobals )
{
	int				i;
	SAVELIGHTSTYLE	light;
	INetworkStringTable * table = networkstringtable->FindTable( "lightstyles" );
	
	// Re-base the savedata since we re-ordered the entity/table / restore fields
	pSaveData->Rebase();
	// Process SAVE_HEADER
	g_ServerGameDLL.SaveReadFields( pSaveData, "Save Header", pHeader, NULL, SAVE_HEADER::m_DataMap.dataDesc, SAVE_HEADER::m_DataMap.dataNumFields );
//	header.version = ENGINE_VERSION;

	pSaveData->levelInfo.mapVersion = pHeader->mapVersion;
	pSaveData->levelInfo.connectionCount = pHeader->connectionCount;
	//pSaveData->levelInfo.time = pHeader->time__USE_VCR_MODE;
	pSaveData->levelInfo.time = gpGlobals->curtime;
	pSaveData->levelInfo.fUseLandmark = true;
	VectorCopy( vec3_origin, pSaveData->levelInfo.vecLandmarkOffset );

	// Read adjacency list
	for ( i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
		g_ServerGameDLL.SaveReadFields( pSaveData, "ADJACENCY", pSaveData->levelInfo.levelList + i, NULL, levellist_t::m_DataMap.dataDesc, levellist_t::m_DataMap.dataNumFields );
	
	if ( updateGlobals )
  	{
  		for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
  			table->SetStringUserData( i, 1, "" );
  	}

	for ( i = 0; i < pHeader->lightStyleCount; i++ )
	{
		g_ServerGameDLL.SaveReadFields( pSaveData, "LIGHTSTYLE", &light, NULL, SAVELIGHTSTYLE::m_DataMap.dataDesc, SAVELIGHTSTYLE::m_DataMap.dataNumFields );
		if ( updateGlobals )
		{
			table->SetStringUserData( light.index, Q_strlen(light.style)+1, light.style );
		}
	}
}

void EntityPatchRead( CSaveRestoreData *pSaveData, const char *level )
{
	char			name[260];
	FileHandle_t	pFile;
	int				i, size, entityId;

	Q_snprintf(name, sizeof( name ), "//%s/%s%s.HL3", "MOD", GetSaveDir(), GetSaveGameMapName( level ) );// DON'T FixSlashes on this, it needs to be //MOD

	pFile = filesystem->Open( name, "rb" );
	if ( pFile )
	{
		// Patch count
		filesystem->Read( &size, sizeof(int), pFile );
		for ( i = 0; i < size; i++ )
		{
			filesystem->Read( &entityId, sizeof(int), pFile );
			pSaveData->GetEntityInfo(entityId)->flags = FENTTABLE_REMOVED;
		}
		filesystem->Close( pFile );
	}
}

extern ConVar skill;

int LoadGameState( char const *level, bool createPlayers )
{
	SAVE_HEADER		header;
	CSaveRestoreData *pSaveData;
	pSaveData = LoadSaveData( GetSaveGameMapName( level ) );
	if ( !pSaveData )		// Couldn't load the file
		return 0;

	g_ServerGameDLL.ReadRestoreHeaders( pSaveData );

	ParseSaveTables( pSaveData, &header, 1 );
	EntityPatchRead( pSaveData, level );
	
	skill.SetValue( header.skillLevel );

	//Q_strncpy( sv.m_szMapname, header.mapName, sizeof( sv.m_szMapname ) );
	ConVarRef skyname( "sv_skyname" );
	if ( skyname.IsValid() )
		skyname.SetValue( header.skyName );
	
	// Create entity list
	g_ServerGameDLL.Restore( pSaveData, createPlayers );

	Finish( pSaveData );

	//sv.m_nTickCount = (int)( header.time__USE_VCR_MODE / host_state.interval_per_tick );
	// SUCCESS!
	return 1;
}

void LandmarkOrigin( CSaveRestoreData *pSaveData, Vector& output, const char *pLandmarkName )
{
	int i;

	for ( i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
	{
		if ( !stricmp( pSaveData->levelInfo.levelList[i].landmarkName, pLandmarkName ) )
		{
			VectorCopy( pSaveData->levelInfo.levelList[i].vecLandmarkOrigin, output );
			return;
		}
	}

	VectorCopy( vec3_origin, output );
}

int EntryInTable( CSaveRestoreData *pSaveData, const char *pMapName, int index )
{
	int i;

	index++;
	for ( i = index; i < pSaveData->levelInfo.connectionCount; i++ )
	{
		if ( !stricmp( pSaveData->levelInfo.levelList[i].mapName, pMapName ) )
			return i;
	}

	return -1;
}

void LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName )
{
	CSaveRestoreData currentLevelData, *pSaveData;
	int				i, test, flags, index, movedCount = 0;
	SAVE_HEADER		header;
	Vector			landmarkOrigin;

	//memset( &currentLevelData, 0, sizeof(CSaveRestoreData) );
	gpGlobals->pSaveData = &currentLevelData;
	// Build the adjacent map list
	g_ServerGameDLL.BuildAdjacentMapList();
	bool foundprevious = false;

	for ( i = 0; i < currentLevelData.levelInfo.connectionCount; i++ )
	{
		// make sure the previous level is in the connection list so we can
		// bring over the player.
		if ( !strcmpi( currentLevelData.levelInfo.levelList[i].mapName, pOldLevel ) )
		{
			foundprevious = true;
		}

		for ( test = 0; test < i; test++ )
		{
			// Only do maps once
			if ( !stricmp( currentLevelData.levelInfo.levelList[i].mapName, currentLevelData.levelInfo.levelList[test].mapName ) )
				break;
		}
		// Map was already in the list
		if ( test < i )
			continue;

		//ConMsg("Merging entities from %s ( at %s )\n", currentLevelData.levelInfo.levelList[i].mapName, currentLevelData.levelInfo.levelList[i].landmarkName );
		pSaveData = LoadSaveData( GetSaveGameMapName( currentLevelData.levelInfo.levelList[i].mapName ) );

		if ( pSaveData )
		{
			g_ServerGameDLL.ReadRestoreHeaders( pSaveData );

			ParseSaveTables( pSaveData, &header, 0 );
			EntityPatchRead( pSaveData, currentLevelData.levelInfo.levelList[i].mapName );
			pSaveData->levelInfo.time = gpGlobals->curtime;// - header.time;
			pSaveData->levelInfo.fUseLandmark = true;
			flags = 0;
			LandmarkOrigin( &currentLevelData, landmarkOrigin, pLandmarkName );
			LandmarkOrigin( pSaveData, pSaveData->levelInfo.vecLandmarkOffset, pLandmarkName );
			VectorSubtract( landmarkOrigin, pSaveData->levelInfo.vecLandmarkOffset, pSaveData->levelInfo.vecLandmarkOffset );
			if ( !stricmp( currentLevelData.levelInfo.levelList[i].mapName, pOldLevel ) )
				flags |= FENTTABLE_PLAYER;

			index = -1;
			while ( 1 )
			{
				index = EntryInTable( pSaveData, STRING( gpGlobals->mapname ), index );
				if ( index < 0 )
					break;
				flags |= 1<<index;
			}
			
			if ( flags )
				movedCount = g_ServerGameDLL.CreateEntityTransitionList( pSaveData, flags );

			// If ents were moved, rewrite entity table to save file
			if ( movedCount )
				EntityPatchWrite( pSaveData, GetSaveGameMapName( currentLevelData.levelInfo.levelList[i].mapName ) );

			Finish( pSaveData );
		}
	}
	gpGlobals->pSaveData = NULL;
	if ( !foundprevious )
		Msg( "[hl2mp.ru] Level transition ERROR\nCan't find connection to %s from %s\n", pOldLevel, STRING( gpGlobals->mapname ) );
}