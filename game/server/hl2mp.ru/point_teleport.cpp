#include "cbase.h"
#include "entitylist.h"
#include "triggers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SF_TELEPORT_TO_SPAWN_POS	0x00000001
#define	SF_TELEPORT_INTO_DUCK		0x00000002 ///< episodic only: player should be ducked after this teleport

class CPointNewTeleport : public CBaseTrigger
{
	DECLARE_CLASS( CPointNewTeleport, CBaseTrigger );

public:
	void	Spawn( void );
	void	RadiusThink( void );

	void	Activate( void );

	void InputTeleport( inputdata_t &inputdata );

private:
	bool	EntityMayTeleport( CBaseEntity *pTarget );
	Vector m_vSaveOrigin;
	QAngle m_vSaveAngles;
	const char *szDistination;
	float		m_flRadius;
	bool m_bEnabled;
	DECLARE_DATADESC();
};

BEGIN_DATADESC( CPointNewTeleport )

	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( szDistination, FIELD_STRING, "TeleportDestination" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Teleport", InputTeleport ),
	DEFINE_KEYFIELD( m_bEnabled, FIELD_BOOLEAN, "StartEnabled" ),
	DEFINE_FUNCTION( RadiusThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( point_teleport, CPointNewTeleport );

void CPointNewTeleport::Spawn( void )
{
	SetThink( NULL );
	SetUse( NULL );
	SetThink( &CPointNewTeleport::RadiusThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_bDisabled = !m_bEnabled;
	BaseClass::Spawn();
}

bool CPointNewTeleport::EntityMayTeleport( CBaseEntity *pTarget )
{
	if ( pTarget->GetMoveParent() != NULL )
	{
		// Passengers in a vehicle are allowed to teleport; their behavior handles it
		CBaseCombatCharacter *pBCC = pTarget->MyCombatCharacterPointer();
		if ( pBCC == NULL || ( pBCC != NULL && pBCC->IsInAVehicle() == false ) )
			return false;
	}

	return true;
}

void CPointNewTeleport::Activate( void )
{
	// Start with our origin point
	m_vSaveOrigin = GetAbsOrigin();
	m_vSaveAngles = GetAbsAngles();

	// Save off the spawn position of the target if instructed to do so
	if ( m_spawnflags & SF_TELEPORT_TO_SPAWN_POS )
	{
		CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, m_target );
		if ( pTarget )
		{
			// If teleport object is in a movement hierarchy, remove it first
			if ( EntityMayTeleport( pTarget ) )
			{
				// Save the points
				m_vSaveOrigin = pTarget->GetAbsOrigin();
				m_vSaveAngles = pTarget->GetAbsAngles();
			}
			else
			{
				Warning("ERROR: (%s) can't teleport object (%s) as it has a parent (%s)!\n",GetDebugName(),pTarget->GetDebugName(),pTarget->GetMoveParent()->GetDebugName());
				BaseClass::Activate();
				return;
			}
		}
		else
		{
			Warning("ERROR: (%s) target '%s' not found. Deleting.\n", GetDebugName(), STRING(m_target));
			UTIL_Remove( this );
			return;
		}
	}

	BaseClass::Activate();
}

void CPointNewTeleport::InputTeleport( inputdata_t &inputdata )
{
	// Attempt to find the entity in question
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, m_target, this, inputdata.pActivator, inputdata.pCaller );
	if ( pTarget == NULL )
		return;

	// If teleport object is in a movement hierarchy, remove it first
	if ( EntityMayTeleport( pTarget ) == false )
	{
		Warning("ERROR: (%s) can't teleport object (%s) as it has a parent (%s)!\n",GetDebugName(),pTarget->GetDebugName(),pTarget->GetMoveParent()->GetDebugName());
		return;
	}

	// in episodic, we have a special spawn flag that forces Gordon into a duck
#ifdef HL2_EPISODIC
	if ( (m_spawnflags & SF_TELEPORT_INTO_DUCK) && pTarget->IsPlayer() ) 
	{
		CBasePlayer *pPlayer = ToBasePlayer( pTarget );
		if ( pPlayer != NULL )
		{
			pPlayer->m_nButtons |= IN_DUCK;
			pPlayer->AddFlag( FL_DUCKING );
			pPlayer->m_Local.m_bDucked = true;
			pPlayer->m_Local.m_bDucking = true;
			pPlayer->m_Local.m_flDucktime = 0.0f;
			pPlayer->SetViewOffset( VEC_DUCK_VIEW_SCALED( pPlayer ) );
			pPlayer->SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		}
	}		
#endif

	pTarget->Teleport( &m_vSaveOrigin, &m_vSaveAngles, NULL );
}

void CPointNewTeleport::RadiusThink( void )
{
	CBaseEntity *ent = NULL;
	while ((ent = gEntList.NextEnt(ent)) != NULL)
	{
		float flDist = ( ent->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();

		if( sqrt( flDist ) <= m_flRadius ) {
			if( PassesTriggerFilters(ent) && !m_bDisabled ) {
				CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, m_target, this );
				if ( pTarget == NULL )
					return;
				CBaseEntity *pDistTarget = gEntList.FindEntityByName( NULL, szDistination, this );
				if ( pDistTarget == NULL )
					return;
				m_vSaveOrigin = pDistTarget->GetAbsOrigin();
				m_vSaveAngles = pDistTarget->GetAbsAngles();
				pTarget->Teleport( &m_vSaveOrigin, &m_vSaveAngles, NULL);
			}
		}
	}
	SetNextThink( gpGlobals->curtime + 0.1f );
}