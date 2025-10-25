
#include "cbase.h"

class CWeaponCubemap : public CBaseCombatWeapon
{
public:

	DECLARE_CLASS( CWeaponCubemap, CBaseCombatWeapon );

	void	Precache( void );

	//bool	HasAnyAmmo( void )	{ return true; }

	void	Spawn( void );

	DECLARE_SERVERCLASS();
};