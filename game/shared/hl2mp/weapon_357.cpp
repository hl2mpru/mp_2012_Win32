
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
#else
	#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeapon357 C_Weapon357
#endif

//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------

class CWeapon357 : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS( CWeapon357, CBaseHL2MPCombatWeapon );
public:

	CWeapon357( void );

	void SecondaryAttack( void );

	void	PrimaryAttack( void );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:
	
	CWeapon357( const CWeapon357 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( Weapon357, DT_Weapon357 )

BEGIN_NETWORK_TABLE( CWeapon357, DT_Weapon357 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeapon357 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_357, CWeapon357 );
PRECACHE_WEAPON_REGISTER( weapon_357 );


#ifndef CLIENT_DLL
acttable_t CWeapon357::m_acttable[] = 
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



IMPLEMENT_ACTTABLE( CWeapon357 );

#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon357::CWeapon357( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= false;
}

#include "npc_manhack.h"

void CWeapon357::SecondaryAttack( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	const char *classname = "npc_manhack";
	CBaseEntity *ent = NULL;
	int maxnpc = 0;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if ((ent->m_iClassname != NULL_STRING	&& FStrEq(classname, STRING(ent->m_iClassname))) ||
				(ent->GetClassname()!=NULL && FStrEq(classname, ent->GetClassname())))
		{
			if (ent->GetPlayerMP() == pOwner)
				maxnpc++;
		}
	}

	if( maxnpc >= 8) {
		ClientPrint( pOwner, HUD_PRINTCENTER, "Limit reached 8, alive Manhack!" );
		return;
	}

	if ( pOwner->GetAmmoCount( m_iSecondaryAmmoType ) <= 0 )
	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		return;
	}
	
	if ( m_flNextSecondaryAttack >= gpGlobals->curtime )
		return;

	Vector	vecEye = pOwner->EyePosition();
	Vector	vForward, vRight;

	pOwner->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
	vForward[2] += 0.1f;

	Vector vecThrow;
	pOwner->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 1200;
	
	CNPC_Manhack *m_hManhack = (CNPC_Manhack *)CreateEntityByName( "npc_manhack" );
	if(m_hManhack) {
		m_hManhack->SetLocalOrigin( pOwner->Weapon_ShootPosition() );
		m_hManhack->AddSpawnFlags( SF_MANHACK_PACKED_UP );
		m_hManhack->SetPlayerMP(pOwner);
		m_hManhack->Spawn();
		m_hManhack->CreateVPhysics();
		m_hManhack->RemoveSolidFlags( FSOLID_NOT_SOLID );
		m_hManhack->SetMoveType( MOVETYPE_VPHYSICS );
		m_hManhack->ClearSchedule( "Manhack released by metropolice" );
		m_hManhack->NotifyInteraction( NULL );
		m_hManhack->SetCollisionGroup( COLLISION_GROUP_PLAYER );
		m_hManhack->SetHealth(500);
		m_hManhack->SetMaxHealth(500);
		IPhysicsObject *pPhysicsObject = m_hManhack->VPhysicsGetObject();
		if ( pPhysicsObject )
		{
			pPhysicsObject->SetVelocity( &vecThrow, NULL );
		}
		m_hManhack->EmitSound("NPC_Manhack.Bat");
	}
	
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pOwner->SetAnimation( PLAYER_ATTACK1 );
	pOwner->RemoveAmmo( 1, m_iSecondaryAmmoType );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;

	m_iClip1--;

	Vector vecSrc		= pPlayer->Weapon_ShootPosition();
	Vector vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );	

	FireBulletsInfo_t info( 1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_pAttacker = pPlayer;

	// Fire the bullets, and force the first shot to be perfectly accuracy
	pPlayer->FireBullets( info );

	//Disorient the player
	QAngle angles = pPlayer->GetLocalAngles();

	angles.x += random->RandomInt( -1, 1 );
	angles.y += random->RandomInt( -1, 1 );
	angles.z = 0;

#ifndef CLIENT_DLL
	pPlayer->SnapEyeAngles( angles );
#endif

	pPlayer->ViewPunch( QAngle( -8, random->RandomFloat( -2, 2 ), 0 ) );

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 ); 
	}
}
