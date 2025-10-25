//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Hornetgun
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
//#include "basecombatweapon_shared.h"
#include "weapon_cubemap.h"
//#include "basecombatcharacter.h"
//#include "AI_BaseNPC.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "hl2_player.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "hl1_npc_hornet.h"

//-----------------------------------------------------------------------------
// CWeaponHgun
//-----------------------------------------------------------------------------
class CWeaponHgun : public CWeaponCubemap
{
	DECLARE_CLASS( CWeaponHgun, CWeaponCubemap );
public:

	CWeaponHgun( void );

	void	Precache( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	WeaponIdle( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	bool	Reload( void );

	virtual void ItemPostFrame( void );
	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

private:

	float	m_flRechargeTime;
	int		m_iFirePhase;
};

LINK_ENTITY_TO_CLASS( weapon_hornetgun, CWeaponHgun );

PRECACHE_WEAPON_REGISTER( weapon_hornetgun );

BEGIN_DATADESC( CWeaponHgun )
	DEFINE_FIELD( m_flRechargeTime,	FIELD_TIME ),
	DEFINE_FIELD( m_iFirePhase,		FIELD_INTEGER ),
END_DATADESC()

acttable_t	CWeaponHgun::m_acttable[] = 
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
};

IMPLEMENT_ACTTABLE(CWeaponHgun);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponHgun::CWeaponHgun( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;

	m_flRechargeTime = 0.0;
	m_iFirePhase = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHgun::Precache( void )
{
	UTIL_PrecacheOther( "hornet" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponHgun::PrimaryAttack( void )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		return;
	}

	WeaponSound( SINGLE );
#if !defined(CLIENT_DLL)
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 200, 0.2 );
#endif
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector	vForward, vRight, vUp;
	QAngle	vecAngles;

	pPlayer->EyeVectors( &vForward, &vRight, &vUp );
	VectorAngles( vForward, vecAngles );

	CBaseEntity *pHornet = CBaseEntity::Create( "hornet", pPlayer->Weapon_ShootPosition() + vForward * 16 + vRight * 8 + vUp * -12, vecAngles, pPlayer );
	if( pHornet )
		pHornet->SetAbsVelocity( vForward * 300 );

	m_flRechargeTime = gpGlobals->curtime + 0.5;
	
	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

	pPlayer->ViewPunch( QAngle( -2, 0, 0 ) );

	m_flNextPrimaryAttack = m_flNextPrimaryAttack + 0.25;

	if (m_flNextPrimaryAttack < gpGlobals->curtime )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.25;
	}

	SetWeaponIdleTime( random->RandomFloat( 10, 15 ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponHgun::SecondaryAttack( void )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		return;
	}

	WeaponSound( SINGLE );
#if !defined(CLIENT_DLL)
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 200, 0.2 );
#endif
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	CBaseEntity *pHornet;
	Vector vecSrc;

	Vector	vForward, vRight, vUp;
	QAngle	vecAngles;

	pPlayer->EyeVectors( &vForward, &vRight, &vUp );
	VectorAngles( vForward, vecAngles );

	vecSrc = pPlayer->Weapon_ShootPosition() + vForward * 16 + vRight * 8 + vUp * -12;

	m_iFirePhase++;
	switch ( m_iFirePhase )
	{
	case 1:
		vecSrc = vecSrc + vUp * 8;
		break;
	case 2:
		vecSrc = vecSrc + vUp * 8;
		vecSrc = vecSrc + vRight * 8;
		break;
	case 3:
		vecSrc = vecSrc + vRight * 8;
		break;
	case 4:
		vecSrc = vecSrc + vUp * -8;
		vecSrc = vecSrc + vRight * 8;
		break;
	case 5:
		vecSrc = vecSrc + vUp * -8;
		break;
	case 6:
		vecSrc = vecSrc + vUp * -8;
		vecSrc = vecSrc + vRight * -8;
		break;
	case 7:
		vecSrc = vecSrc + vRight * -8;
		break;
	case 8:
		vecSrc = vecSrc + vUp * 8;
		vecSrc = vecSrc + vRight * -8;
		m_iFirePhase = 0;
		break;
	}

	pHornet = CBaseEntity::Create( "hornet", vecSrc, vecAngles, pPlayer );
	if( pHornet ) {
		pHornet->SetAbsVelocity( vForward * 1200 );
		pHornet->SetThink( &CNPC_Hornet::StartDart );
	}

	m_flRechargeTime = gpGlobals->curtime + 0.5;

	pPlayer->ViewPunch( QAngle( -2, 0, 0 ) );
	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.1;
	SetWeaponIdleTime( random->RandomFloat( 10, 15 ) );
}

void CWeaponHgun::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	int iAnim;
	float flRand = random->RandomFloat( 0, 1 );
	if ( flRand <= 0.75 )
	{
		iAnim = ACT_VM_IDLE;
	}
	else
	{
		iAnim = ACT_VM_FIDGET;
	}

	SendWeaponAnim( iAnim );
}

bool CWeaponHgun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	bool bRet;

	bRet = BaseClass::Holster( pSwitchingTo );

	if ( bRet )
	{
		CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
		
		if ( pPlayer )
		{
#if !defined(CLIENT_DLL)            
			//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
            int iCount = pPlayer->GetAmmoCount( m_iPrimaryAmmoType );
            if ( iCount <= 0 )
            {
                pPlayer->GiveAmmo( iCount+1, m_iPrimaryAmmoType, true );
            }
#endif            
		}
	}

	return bRet;
}

bool CWeaponHgun::Reload( void )
{
	if ( m_flRechargeTime >= gpGlobals->curtime )
	{
		return true;
	}

	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return true;
	}

#ifdef CLIENT_DLL
#else
	if ( !g_pGameRules->CanHaveAmmo( pPlayer, m_iPrimaryAmmoType ) )
		return true;

	while ( ( m_flRechargeTime < gpGlobals->curtime ) && g_pGameRules->CanHaveAmmo( pPlayer, m_iPrimaryAmmoType ) )
	{
		pPlayer->GiveAmmo( 1, m_iPrimaryAmmoType, true );
		m_flRechargeTime += 0.5;
	}
#endif

	return true;
}

void CWeaponHgun::ItemPostFrame( void )
{
	Reload();

	BaseClass::ItemPostFrame();
}
