#include "cbase.h"
#include "in_buttons.h"
#include "hl2mp_player_fix.h"
#include "eventqueue.h"

#include "tier0/memdbgon.h"

class HL2CMedkit:public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( HL2CMedkit, CBaseCombatWeapon );

	void Precache( void );
	bool Reload( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void ItemPostFrame( void );

	DECLARE_ACTTABLE();
};

LINK_ENTITY_TO_CLASS( weapon_medkit, HL2CMedkit );
PRECACHE_WEAPON_REGISTER( weapon_medkit );

acttable_t HL2CMedkit::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SLAM,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SLAM,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SLAM,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SLAM,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SLAM,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SLAM,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SLAM,					false },
};

IMPLEMENT_ACTTABLE( HL2CMedkit );

bool HL2CMedkit::Reload( void )
{
	WeaponIdle( );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void HL2CMedkit::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "HealthVial.Touch" );

	UTIL_PrecacheOther( "item_healthvial" );
}

extern ConVar sk_healthvial;

//-----------------------------------------------------------------------------
// Purpose: Main attack
//-----------------------------------------------------------------------------
void HL2CMedkit::PrimaryAttack( void )
{
	CHL2MP_Player_fix *pOwner =  dynamic_cast<CHL2MP_Player_fix *>( ToBasePlayer( GetOwner() ) );
	
	if ( pOwner == NULL )
		return;

	if ( gpGlobals->curtime <= m_flNextPrimaryAttack ) 
		return;

	pOwner->SetAnimation( PLAYER_ATTACK1 );

	CBaseEntity *pTarget = pOwner->GetAimTarget( 94 );
	if( pTarget != NULL && pOwner->FVisible(pTarget, MASK_SHOT) && ( ( pTarget->IsNPC() && pOwner->IRelationType( pTarget ) == D_LI ) || pTarget->IsPlayer() ) && pTarget->IsAlive() && pTarget->GetMaxHealth() != 0 && pTarget->GetHealth() < pTarget->GetMaxHealth() ) {
		
		int maxHealth = pTarget->GetMaxHealth();
		//int m_rHealth = random->RandomInt(2,7);
		int m_rHealth = sk_healthvial.GetInt();
		if( pTarget->IsNPC() && maxHealth > 100 )
			m_rHealth = maxHealth/100*m_rHealth;
		pTarget->TakeHealth( m_rHealth, DMG_GENERIC );

		SendWeaponAnim( ACT_VM_SWINGHIT );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration() + 0.25;
		m_bInReload = true;
		pOwner->RemoveAmmo( 1, m_iSecondaryAmmoType );
		WeaponSound( SINGLE );
		return;
	}

	SendWeaponAnim( ACT_VM_SWINGMISS );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration() + 0.25;
}

void HL2CMedkit::SecondaryAttack( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	if ( gpGlobals->curtime <= m_flNextSecondaryAttack )
		return;

	SendWeaponAnim( ACT_VM_SWINGHIT );
	pOwner->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration() + 0.25;

	m_bInReload = true;

	Vector vecSrc;
	Vector	vForward, vRight, vUp;
	QAngle	vecAngles;

	pOwner->EyeVectors( &vForward, &vRight, &vUp );
	VectorAngles( vForward, vecAngles );
	vecAngles.x -= 158;
	vecAngles.y += 22;
	vecAngles.z -= 22;
	vecSrc = pOwner->Weapon_ShootPosition() + vForward * 16 + vRight * 8 + vUp * -12;
	vecSrc.z += 10;
	CBaseEntity * pItem = CBaseEntity::Create( "item_healthvial", vecSrc, vecAngles, pOwner );
	if( pItem ) {
		//pItem->SetRenderColor(48, 183, 213);

		Vector vecFacing = pOwner->BodyDirection3D( );
		vecSrc = vecSrc + vecFacing * 18.0;
		// BUGBUG: is this because vecSrc is not from Weapon_ShootPosition()???
		vecSrc.z += 24.0f;

		Vector vecThrow;
		GetOwner()->GetVelocity( &vecThrow, NULL );
		vecThrow += vecFacing * 500;

		pItem->AddSpawnFlags( SF_NORESPAWN );
		pItem->ApplyAbsVelocityImpulse( vecThrow );
		pItem->SetLocalAngularVelocity( QAngle( 0, 400, 0 ) );
		pOwner->RemoveAmmo( 1, m_iSecondaryAmmoType );
		//D@Ni1986: Антиспам, удаляем через 2 минуты аптечку
		g_EventQueue.AddEvent( pItem, "Kill", 120.0f, NULL, NULL );
	}
}

void HL2CMedkit::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	if ( !HasSecondaryAmmo() )
		pPlayer->SwitchToNextBestWeapon( this );

	if( m_bInReload && (m_flNextPrimaryAttack <= gpGlobals->curtime) ) {
		m_bInReload = false;
		SendWeaponAnim( ACT_VM_DRAW );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
		return;
	}

	if( (pPlayer->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
		PrimaryAttack();

	pPlayer->m_nButtons &= ~IN_ATTACK;

	BaseClass::ItemPostFrame();
}
