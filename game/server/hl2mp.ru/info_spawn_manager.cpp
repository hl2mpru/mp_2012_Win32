#include "cbase.h"

class ism : public CBaseEntity
{
public:
	DECLARE_CLASS( ism, CBaseEntity );
	DECLARE_DATADESC();

	void SetCheckPoint( inputdata_t &inputData );
	void TeleportPlayers( inputdata_t &inputData );
	void MovePlayers( inputdata_t &inputData );
	void TpAllPlayer( const Vector newPosition, const QAngle newAngles );

	COutputEvent m_OnFinish;
};

extern EHANDLE g_pSpawnSpot;

LINK_ENTITY_TO_CLASS( info_spawn_manager, ism );
LINK_ENTITY_TO_CLASS( info_player_checkpoint, ism );
LINK_ENTITY_TO_CLASS( info_checkpoint, ism );

BEGIN_DATADESC( ism )

	DEFINE_INPUTFUNC( FIELD_STRING, "SetCheckPoint", SetCheckPoint ),
	DEFINE_INPUTFUNC( FIELD_STRING, "TeleportPlayers", TeleportPlayers ),
	DEFINE_INPUTFUNC( FIELD_STRING, "MovePlayers", MovePlayers ),
	DEFINE_OUTPUT( m_OnFinish, "OnFinish"),
	
END_DATADESC()

void ism::SetCheckPoint( inputdata_t &inputData )
{
	CBaseEntity *pMaster = gEntList.FindEntityByName( NULL, inputData.value.StringID(), NULL, this );
	
	if( pMaster )
		g_pSpawnSpot = pMaster;
	else
		g_pSpawnSpot = this;
}

void ism::TeleportPlayers( inputdata_t &inputData )
{
	CBaseEntity *pMaster = gEntList.FindEntityByName( NULL, inputData.value.StringID(), NULL, this );
	if( pMaster )
		TpAllPlayer( pMaster->GetAbsOrigin(), vec3_angle );	

}

void ism::MovePlayers( inputdata_t &inputData )
{
	CBaseEntity *pMaster = gEntList.FindEntityByName( NULL, inputData.value.StringID(), NULL, this );

	if( !pMaster )
		pMaster = inputData.pActivator;

	if( !pMaster )
		pMaster = g_pSpawnSpot;
	
	if( pMaster )
		TpAllPlayer( pMaster->GetAbsOrigin(), vec3_angle );

	m_OnFinish.FireOutput( inputData.pActivator, inputData.pCaller );
}

#define	FL_SAVERESTORE_CLIENT_NOTP (1<<0)
#define	FL_SAVERESTORE_NPC_NOTP (1<<1)
#define	FL_SAVERESTORE_NPC_NORESTORE (1<<2)

extern void SaveClientState( CBasePlayer *pPlayer, int pFlags = NULL );

void ism::TpAllPlayer( Vector newPosition, QAngle newAngles  )
{
	SaveClientState( NULL, FL_SAVERESTORE_CLIENT_NOTP );

	for( int i=1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() ) {
			pPlayer->LeaveVehicle();
			pPlayer->SetGroundEntity( NULL );
			newPosition.z += 5.0f;
			pPlayer->SetAbsOrigin( newPosition );
			//pPlayer->SetAbsAngles( newAngles );
			//pPlayer->Teleport( &newPosition, &newAngles, NULL );
		}
	}

	engine->ServerCommand( "sm_savetp_clearpoint\n");
}
