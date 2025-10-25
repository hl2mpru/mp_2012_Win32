#include "cbase.h"
#include "point_worldtext.h"
#include "Sprite.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CInfoWayPoint : public CPointWorldText
{
public:
	DECLARE_CLASS( CInfoWayPoint, CPointWorldText );

	virtual void Spawn( void ) OVERRIDE;
	virtual bool KeyValue( const char *szKeyName, const char *szValue ) OVERRIDE;

	void InputEnable( inputdata_t &inputdata )
	{
		RemoveEffects( EF_NODRAW );
		m_hSprite->TurnOn();
	};

	void InputDisable( inputdata_t &inputdata )
	{
		AddEffects( EF_NODRAW );
		m_hSprite->TurnOff();
	};

	DECLARE_DATADESC();

	CHandle<CSprite> m_hSprite;
	int m_iDisable;
};

LINK_ENTITY_TO_CLASS(info_waypoint, CInfoWayPoint);

BEGIN_DATADESC( CInfoWayPoint )
	DEFINE_KEYFIELD( m_iDisable, FIELD_INTEGER, "StartDisabled" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

void CInfoWayPoint::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );

	color32 tmp = { 255, 170, 0, 255 };
	m_colTextColor = tmp;

	m_nFont = 8;
	m_nOrientation = 2;
	m_flTextSize = 4;

	Vector origin = GetAbsOrigin();
	origin.z += 6.0;

	m_hSprite = CSprite::SpriteCreate( "hl2mp.ru/waypoint_move.vmt", origin, false );
	if ( m_hSprite ) {
		m_hSprite->SetRenderMode( kRenderGlow );
		m_hSprite->SetScale( 0.15f );
		m_hSprite->SetGlowProxySize( 64.0f );
		m_hSprite->SetParent( this );
		//m_hSprite->TurnOn();
	}

	//if( m_iDisable )
	{
		AddEffects( EF_NODRAW );
		m_hSprite->TurnOff();
	}

	BaseClass::Spawn();
}


bool CInfoWayPoint::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "text" ) )
	{
		V_strncpy( m_szText.GetForModify(), szValue, sizeof(m_szText) );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


class CPointMessageMP : public CPointWorldText
{
public:
	DECLARE_CLASS( CPointMessageMP, CPointWorldText );

	virtual void Spawn( void ) OVERRIDE;
	virtual bool KeyValue( const char *szKeyName, const char *szValue ) OVERRIDE;
};

LINK_ENTITY_TO_CLASS(point_message_multiplayer, CPointMessageMP);
LINK_ENTITY_TO_CLASS(point_message, CPointMessageMP);

void CPointMessageMP::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );

	m_nFont = 4;
	m_nOrientation = 2;
	m_flTextSize = 2;

	BaseClass::Spawn();
}

bool CPointMessageMP::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "message" ) )
	{
		V_strncpy( m_szText.GetForModify(), szValue, sizeof(m_szText) );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}