#include "cbase.h"
#include "particle_system.h"

class CChargeToken : public CBaseEntity
{
	DECLARE_CLASS( CChargeToken, CBaseEntity );
	DECLARE_DATADESC();

public:

	static CChargeToken *CreateChargeToken( const Vector &vecOrigin, CBaseEntity *pOwner, CBaseEntity *pTarget );

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

	void	FadeAndDie( void );
	void	SeekThink( void );
	void	SeekTouch( CBaseEntity	*pOther );
	void	SetTargetEntity( CBaseEntity *pTarget ) { m_hTarget = pTarget; }

private:

	Vector	GetSteerVector( const Vector &vecForward );

	float	m_flLifetime;
	EHANDLE m_hTarget;
	CHandle<CParticleSystem> m_hSpitEffect;
};