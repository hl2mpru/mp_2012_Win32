#include "cbase.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CAdmins : public CAutoGameSystemPerFrame
{
public:
	virtual char const *Name() { return "CAdmins"; }
	CAdmins( char const *name ) : CAutoGameSystemPerFrame( name ) {}

	CAdmins() {}

	~CAdmins() {}

	virtual void LevelInitPreEntity( void ) {
		manifest = new KeyValues("AdminsList");
		manifest->LoadFromFile(filesystem, "admins.txt", "GAME");
	}

	virtual void LevelShutdownPostEntity() {
		if( manifest )
			manifest->deleteThis();
	}

	void Save() {
		char fullPath[_MAX_PATH];
		if (filesystem->GetLocalPath("admins.txt", fullPath, sizeof(fullPath)))
			manifest->SaveToFile( filesystem, fullPath, "GAME" );
	}

	int AdminFlags( const char *pSteamID ) {
		int kvFlags = 0;
		KeyValues *pClient = manifest->FindKey( pSteamID );
	
		if( pClient != NULL )
			kvFlags = pClient->GetInt("pFlags");
	
		return kvFlags;
	}

	void AdminAddFlags( const char *pSteamID, int pFlags ) {
		int kvFlags = 0;
	
		KeyValues *pClient = manifest->FindKey( pSteamID );
	
		if( pClient == NULL) {
			pClient = new KeyValues( pSteamID );
			manifest->AddSubKey( pClient );
		}

		kvFlags = pClient->GetInt("pFlags");
		kvFlags |= pFlags;
		pClient->SetInt("pFlags", kvFlags );

		Save();
	}

	void AdminRemoveFlags( const char *pSteamID, int pFlags ) {
		int kvFlags = 0;
	
		KeyValues *pClient = manifest->FindKey( pSteamID );
	
		if( pClient == NULL) {
			pClient = new KeyValues( pSteamID );
			manifest->AddSubKey( pClient );
		}

		kvFlags = pClient->GetInt("pFlags");
		kvFlags &= ~pFlags;
	
		if( kvFlags == 0 )
			manifest->RemoveSubKey( pClient );
		else
			pClient->SetInt("pFlags", kvFlags );

		Save();
	}

	void AdminList( void ) {
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			Msg( "{\"pSteamID64\": \"%s\", \"pFlags\": %d}\n", sub->GetName(), sub->GetInt("pFlags") );
		}
	}

	KeyValues *manifest;
};

CAdmins gAdmins;

bool IsAdmin( CBaseEntity *pEntity, int pFlags ) {
	CBasePlayer *pPlayer = ToBasePlayer( pEntity );
	if( !pPlayer )
		return false;

	const char *pSteamId = UTIL_VarArgs("%llu", pPlayer->GetSteamIDAsUInt64() );
	int kvFlags = gAdmins.AdminFlags( pSteamId );
	if( kvFlags == 0 )
		kvFlags = gAdmins.AdminFlags( "default" );

	if( kvFlags & pFlags )
		return true;

	return false;
}

CON_COMMAND(sv_admin_flags, "")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *pSteamID = args.Arg(1);

	Msg( "%d\n", gAdmins.AdminFlags( pSteamID ) );
}

CON_COMMAND(sv_admin_addflags, "")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *pSteamID = args.Arg(1);
	int pFlags = atoi(args.Arg(2));
	gAdmins.AdminAddFlags( pSteamID, pFlags );
}

CON_COMMAND(sv_admin_removeflags, "")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *pSteamID = args.Arg(1);
	int pFlags = atoi(args.Arg(2));
	gAdmins.AdminRemoveFlags( pSteamID, pFlags );
}

CON_COMMAND(sv_admin_list, "")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	gAdmins.AdminList();
}