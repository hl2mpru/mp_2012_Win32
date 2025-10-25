#include "cbase.h"
#include "entitylist.h"
#include "triggers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPointTrigger : public CBaseTrigger
{
	DECLARE_CLASS( CPointTrigger, CBaseTrigger );

public:
	void	Spawn( void );
	void	RadiusThink( void );
	
	DECLARE_DATADESC();

	bool		m_bOneTouch;
	float		m_flRadius;
};

BEGIN_DATADESC( CPointTrigger )

	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "TriggerRadius" ),
	DEFINE_KEYFIELD( m_bOneTouch, FIELD_BOOLEAN, "TriggerOnce" ),
	DEFINE_FUNCTION( RadiusThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( point_trigger, CPointTrigger );

void CPointTrigger::Spawn(void)
{
	SetThink( NULL );
	SetUse( NULL );
	SetThink( &CPointTrigger::RadiusThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	BaseClass::Spawn();
}

void CPointTrigger::RadiusThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );
	if( m_bDisabled )
		return;

	CBaseEntity *ent = NULL;
	while ((ent = gEntList.NextEnt(ent)) != NULL)
	{
		float flDist = ( ent->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();

		if( sqrt( flDist ) <= m_flRadius ) {
			if( PassesTriggerFilters(ent) ) {
				StartTouch( ent );
				if( m_bOneTouch ) {
					Remove();
				}
			}
		}
	}
}