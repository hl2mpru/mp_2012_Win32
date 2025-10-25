#include "cbase.h"
#include "filesystem.h"
#include "hl2mp_admins.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_PlayersModels {
public:
	C_PlayersModels() {
		manifest = new KeyValues("ModelsList");
	}

	~C_PlayersModels() {}

	void init() {
		manifest->LoadFromFile(filesystem, "skins.txt", "GAME");
	}

	void clear() {
		manifest->Clear();
	}

	void SetPlayerModelDB( CBasePlayer *pPlayer, const char *szModel ) {
		const char *pSteamID = UTIL_VarArgs( "%llu", pPlayer->GetSteamIDAsUInt64() ) ;
		manifest->SetString(pSteamID, szModel );
		char fullPath[_MAX_PATH];
		if( filesystem->GetLocalPath( "skins.txt", fullPath, sizeof(fullPath) ) )
			manifest->SaveToFile( filesystem, fullPath, "GAME" );
	}

	const char *GetPlayerModelDB( CBasePlayer *pPlayer ) {
		const char *pSteamID = UTIL_VarArgs( "%llu", pPlayer->GetSteamIDAsUInt64() ) ;
		return manifest->GetString( pSteamID );
	}

	KeyValues *manifest;
};

C_PlayersModels gpModels;

CON_COMMAND(sm_models, "Models player")
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !UTIL_IsCommandIssuedByServerAdmin() && pPlayer && !IsAdmin( pPlayer, FL_ADMIN ) )
		return;

	const char *modify = args.Arg(1)+1;
	int userid = atoi(modify);
	pPlayer = UTIL_PlayerByUserId( userid );
	if( !pPlayer )
		return;

	const char *szModel = args.Arg(2);

	if( !filesystem->FileExists( szModel ) ) {
		Msg( "szModel: %s Not found\n", szModel );
		return;
	}

	if( !UTIL_IsCommandIssuedByServerAdmin() )
		gpModels.SetPlayerModelDB( pPlayer, szModel );
	else {
		int i = modelinfo->GetModelIndex( szModel );
		if ( i == -1 )
			engine->PrecacheModel( szModel );

		pPlayer->SetModel( szModel );
	}
}


CON_COMMAND(killid, "Kill player #userid")
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !UTIL_IsCommandIssuedByServerAdmin() && pPlayer && !IsAdmin( pPlayer, FL_ADMIN ) )
		return;

	const char *modify = args.Arg(1);
	int userid = atoi(modify);
	pPlayer = UTIL_PlayerByUserId( userid );
	if( !pPlayer )
		return;

	pPlayer->CommitSuicide();
}

class C_MapRecovery {
public:
	C_MapRecovery() {
		m_initialised = false;
		manifest = new KeyValues("MapRecovery");
	}

	~C_MapRecovery() {}

	void init() {
		manifest->LoadFromFile(filesystem, "maprecovery.txt", "GAME");
		m_initialised = true;
	}

	void clear() {
		manifest->Clear();
	}

	void SetMapDB( const char *szMap ) {
		manifest->SetString("mapname", szMap );
		char fullPath[_MAX_PATH];
		if( filesystem->GetLocalPath( "maprecovery.txt", fullPath, sizeof(fullPath) ) )
			manifest->SaveToFile( filesystem, fullPath, "GAME" );
	}

	const char *GetMapDB() {
		return manifest->GetString( "mapname" );
	}

	bool m_initialised;
	KeyValues *manifest;
};

C_MapRecovery gpMapRecovery;

class C_ConCommandHelper : public CAutoGameSystemPerFrame
{
public:
	virtual char const *Name() { return "C_ConCommandHelper"; }
	C_ConCommandHelper( char const *name ) : CAutoGameSystemPerFrame( name ) {}

	C_ConCommandHelper() {}

	~C_ConCommandHelper() {}

	virtual void LevelInitPreEntity( void )
	{
		CUtlString sCmd;
		sCmd.Format( "exec maps/%s.cfg\n", STRING( gpGlobals->mapname) );
		engine->ServerCommand( sCmd.Get() );

		if( !gpMapRecovery.m_initialised )
		{
			gpMapRecovery.init();
			const char *pMapRecovery = gpMapRecovery.GetMapDB();
			if( Q_stricmp( pMapRecovery, STRING( gpGlobals->mapname) ) )
			{
				engine->ServerCommand( UTIL_VarArgs("changelevel %s\n", pMapRecovery ) );
			}
		}
		else
		{
			gpMapRecovery.SetMapDB( STRING( gpGlobals->mapname) );
		}
		
		gpModels.init();
	}

	virtual void LevelShutdownPostEntity()
	{
		gpModels.clear();
	}

	virtual void FrameUpdatePostEntityThink( void ) 
	{
		for( int i=1; i<=gpGlobals->maxClients; i++ )
		{
			CBasePlayer *player = UTIL_PlayerByIndex( i );
			if( !player || !player->IsAlive() || player->IsFakeClient() )
				continue;

			const char *szModel = gpModels.GetPlayerModelDB( player );
			if( szModel[0] == '\0' )
				continue;

			if( !Q_stricmp( szModel, STRING( player->GetModelName() ) ) )
				continue;

			engine->PrecacheModel( szModel );
			player->SetModel( szModel );
		}
	}
};

C_ConCommandHelper gConCommandHelper;