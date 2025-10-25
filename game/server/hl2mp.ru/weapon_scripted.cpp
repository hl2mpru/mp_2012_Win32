#include "cbase.h"
#include "soundent.h"

class WeaponScripted : public CBaseEntity
{
public:
	DECLARE_CLASS( WeaponScripted, CBaseEntity );
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( weapon_scripted, WeaponScripted );

static const char *RandomWeapon[] = 
{
	"weapon_ar2",
	"weapon_smg1",
	"weapon_pistol",
	"weapon_357",
	"weapon_shotgun",
	"weapon_crossbow",
};

void WeaponScripted::Spawn( void )
{
	CBaseEntity *entity = CreateEntityByName( RandomWeapon[rand() % sizeof(RandomWeapon)/sizeof(RandomWeapon[0])] );
	if( entity ) {
		entity->SetAbsOrigin( GetAbsOrigin() );
		entity->SetAbsAngles( GetAbsAngles() );
		DispatchSpawn( entity );
	}
	Remove();
}