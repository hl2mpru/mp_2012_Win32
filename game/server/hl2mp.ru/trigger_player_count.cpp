#include "cbase.h"
#include "triggers.h"

class tpc : public CBaseTrigger
{
public:
	DECLARE_CLASS( tpc, CBaseTrigger );
	DECLARE_DATADESC();
	void Spawn( void );
	void StartTouch(CBaseEntity *pOther);
	void EndTouch(CBaseEntity *pOther);
	void OnThink( void );
	void InputTeleportPlayersNotTouching( inputdata_t &inputdata );

protected:
	COutputEvent m_OnAllPlayersEntered;
	COutputEvent m_OnPlayersIn;
	
	int m_iPlayersIn;
	int m_iPlayersInGame;
	int m_iPlayerValue;
	bool m_bUseHUD;
	bool m_iPlayerIn[MAX_PLAYERS];
};

//Obsidian
LINK_ENTITY_TO_CLASS( trigger_player_count, tpc );
//Synergy
LINK_ENTITY_TO_CLASS( trigger_coop, tpc );

BEGIN_DATADESC( tpc )
	
	DEFINE_THINKFUNC( OnThink ),
	DEFINE_KEYFIELD( m_iPlayerValue, FIELD_INTEGER, "PlayerValue" ),
	DEFINE_KEYFIELD( m_bUseHUD, FIELD_BOOLEAN, "UseHUD" ),
	//Obsidian
	DEFINE_OUTPUT( m_OnAllPlayersEntered, "OnAllPlayersEntered"),
	//Synergy
	DEFINE_OUTPUT( m_OnPlayersIn, "OnPlayersIn"),
	DEFINE_INPUTFUNC( FIELD_VOID, "TeleportPlayersNotTouching", InputTeleportPlayersNotTouching ),

END_DATADESC()

void tpc::Spawn( void )
{
	BaseClass::Spawn();
	InitTrigger();
	SetThink( &tpc::OnThink );
	SetNextThink( gpGlobals->curtime + 2.0 );

	if( m_iPlayerValue == 0 )
		m_iPlayerValue = 100;
}

void tpc::StartTouch(CBaseEntity *pOther)
{
	BaseClass::StartTouch( pOther );

	if( !pOther || !pOther->IsPlayer() )
		return;

	m_iPlayerIn[pOther->entindex()] = true;
	m_iPlayersIn++;
	int result = int( floor( 100.0 / m_iPlayersInGame * m_iPlayersIn + 0.5 ) );
	if( result >= m_iPlayerValue ) {
		m_OnAllPlayersEntered.FireOutput(pOther, this);
		m_OnPlayersIn.FireOutput(pOther, this);
	}
}

void tpc::EndTouch(CBaseEntity *pOther)
{
	BaseClass::EndTouch( pOther );

	if( !pOther || !pOther->IsPlayer() )
		return;

	m_iPlayerIn[pOther->entindex()] = false;
	m_iPlayersIn--;
}

extern const char *UTIL_TranslateMsg( const char *szLanguage, const char *msg );

void tpc::OnThink( void )
{
	int playeronline = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if ( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && !pPlayer->IsFakeClient() ) {
			playeronline++;
			if( m_iPlayerIn[i] && m_bUseHUD ) {
				int result = int( floor( m_iPlayersInGame / 100.0 * m_iPlayerValue + 0.5 ) );
				if( result != 1 ) {
					const char* msg = UTIL_TranslateMsg( engine->GetClientConVarValue( ENTINDEX( pPlayer ), "cl_language" ), "#HL2MP_TrrigerCountMsg" );
					ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "%s %d/%d", msg, m_iPlayersIn, result) );
				}
			}
		}
	}
	m_iPlayersInGame = playeronline;
	
	SetNextThink( gpGlobals->curtime + 0.05 );
}

void tpc::InputTeleportPlayersNotTouching( inputdata_t &inputdata )
{
	if ( m_target == NULL_STRING )
		return;

	CBaseEntity *pMaster = gEntList.FindEntityByName( NULL, STRING( m_target ), NULL, this );
	if( pMaster == NULL )
		return;

	for( int i=1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() ) {
			if( m_iPlayerIn[i] == false ) 
				continue;
			
			if( pPlayer->IsInAVehicle() )
				pPlayer->LeaveVehicle();

			pPlayer->SetAbsOrigin( pMaster->GetAbsOrigin() );
			pPlayer->SetAbsAngles( pMaster->GetAbsAngles() );
			pPlayer->Teleport( &pMaster->GetAbsOrigin(), &pMaster->GetAbsAngles(), NULL );
		}
	}
}