#include "cbase.h"
#include "weapon_flaregun.h"

#include "tier0/memdbgon.h"

class HL2CFlaregun:public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( HL2CFlaregun, CBaseCombatWeapon );

	void Precache( void );
	void PrimaryAttack( void );
	void ItemPostFrame( void );
	float GetFireRate( void ) { return 1; };

	DECLARE_ACTTABLE();
};

LINK_ENTITY_TO_CLASS( weapon_flaregun, HL2CFlaregun );
PRECACHE_WEAPON_REGISTER( weapon_flaregun );

acttable_t HL2CFlaregun::m_acttable[] = 
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_PISTOL,				false },
};

IMPLEMENT_ACTTABLE( HL2CFlaregun );

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void HL2CFlaregun::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Flare.Touch" );

	PrecacheScriptSound( "Weapon_FlareGun.Burn" );

	UTIL_PrecacheOther( "env_flare" );
}

//-----------------------------------------------------------------------------
// Purpose: Main attack
//-----------------------------------------------------------------------------
void HL2CFlaregun::PrimaryAttack( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	if ( gpGlobals->curtime <= m_flNextPrimaryAttack )
		return;

	m_iClip1 = m_iClip1 - 1;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pOwner->SetAnimation( PLAYER_ATTACK1 );
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;

	CFlare *pFlare = CFlare::Create( pOwner->Weapon_ShootPosition(), pOwner->EyeAngles(), pOwner, FLARE_DURATION );

	if ( pFlare == NULL )
		return;
	
	Vector forward;
	pOwner->EyeVectors( &forward );

	pFlare->SetAbsVelocity( forward * 1500 );
	pFlare->VPhysicsInitNormal( SOLID_BBOX, 0, false );
	pFlare->SetCollisionGroup( COLLISION_GROUP_PLAYER );

	WeaponSound( SINGLE );
}

void HL2CFlaregun::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	if ( m_bInReload && (m_flNextPrimaryAttack <= gpGlobals->curtime) ) {
		FinishReload();
		m_bInReload = false;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		return;
	}
	BaseClass::ItemPostFrame();
}
