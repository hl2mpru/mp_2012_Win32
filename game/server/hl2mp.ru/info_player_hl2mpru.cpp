#include "cbase.h"

class iph : public CBaseEntity
{
public:
	DECLARE_CLASS( iph, CBaseEntity );
	DECLARE_DATADESC();

	void Spawn( void );
	void InputMovePlayers( inputdata_t &inputdata );
	void TimerThink( void );
	CBasePlayer *GetNearestVisiblePlayer( void );
	void TpAllPlayer( const Vector newPosition, const QAngle newAngles, CBasePlayer *ignore = NULL );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	bool m_bTpAll;
	float m_fRadius;
	int	m_iDisabled;
};

LINK_ENTITY_TO_CLASS( info_player_hl2mpru, iph );

BEGIN_DATADESC( iph )

	DEFINE_THINKFUNC( TimerThink ),
	DEFINE_KEYFIELD( m_bTpAll, FIELD_BOOLEAN, "tpall" ),
	DEFINE_KEYFIELD( m_fRadius, FIELD_FLOAT, "radius" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "MovePlayers", InputMovePlayers ),
	DEFINE_KEYFIELD( m_iDisabled, FIELD_INTEGER, "StartDisabled" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

END_DATADESC()

void iph::Spawn( void ) 
{
	if( m_fRadius == 0.0 )
		m_fRadius = 512.0;

	SetThink( &iph::TimerThink );
	SetNextThink( gpGlobals->curtime + 0.05 );
}

void iph::InputMovePlayers( inputdata_t &inputdata ) 
{
	Vector origin = GetAbsOrigin();
	origin[2] += 8.0;
	TpAllPlayer( origin, GetAbsAngles() );
}

extern EHANDLE g_pSpawnSpot;

void iph::TimerThink( void )
{
	CBasePlayer *pPlayer = GetNearestVisiblePlayer();
	if( pPlayer && pPlayer->IsAlive() && !m_iDisabled ) {
		g_pSpawnSpot = this;
		if( m_bTpAll )
			TpAllPlayer( GetAbsOrigin(), GetAbsAngles(), pPlayer );

		trace_t tr;
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 16.0 ), MASK_ALL, pPlayer, COLLISION_GROUP_PLAYER, &tr);
		if( tr.fraction != 1.0 && tr.m_pEnt && !tr.m_pEnt->IsPlayer() ) {
			if( tr.m_pEnt->GetParent() )
				SetParent( tr.m_pEnt->GetParent() );
			else
				SetParent( tr.m_pEnt );
		}
		return;
	}
	SetNextThink( gpGlobals->curtime + 0.05 );
}

CBasePlayer *iph::GetNearestVisiblePlayer( void )
{
	Vector pos = GetAbsOrigin();
	pos[2] += 32.0;

	CBasePlayer *pPlayer = UTIL_GetNearestPlayer( pos );

	if( pPlayer ) {
		float flDist = ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();

		if( sqrt( flDist ) < m_fRadius && FVisible( pPlayer, MASK_SOLID_BRUSHONLY ) )
			return pPlayer;
	}

	return NULL;
}

#define	FL_SAVERESTORE_CLIENT_NOTP (1<<0)
#define	FL_SAVERESTORE_NPC_NOTP (1<<1)
#define	FL_SAVERESTORE_NPC_NORESTORE (1<<2)

extern void SaveClientState( CBasePlayer *pPlayer, int pFlags = NULL );

void iph::TpAllPlayer( const Vector newPosition, const QAngle newAngles, CBasePlayer *ignore  )
{
	for( int i=1;i<=gpGlobals->maxClients;i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if( pPlayer && (ignore != pPlayer) && pPlayer->IsConnected() && pPlayer->IsAlive() ) {
			if( pPlayer->IsInAVehicle() )
				pPlayer->LeaveVehicle();

			pPlayer->SetAbsOrigin( newPosition );
			pPlayer->SetAbsAngles( newAngles );
			pPlayer->Teleport( &newPosition, &newAngles, NULL );
		}
	}
	SaveClientState( NULL, FL_SAVERESTORE_CLIENT_NOTP );
	engine->ServerCommand( "sm_savetp_clearpoint\n");
}

void iph::InputEnable( inputdata_t &inputdata )
{
	m_iDisabled = FALSE;
}

void iph::InputDisable( inputdata_t &inputdata )
{
	m_iDisabled = TRUE;
}