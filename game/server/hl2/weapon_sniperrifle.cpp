//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a sniper rifle weapon.
//			
//			Primary attack: fires a single high-powered shot, then reloads.
//			Secondary attack: cycles sniper scope through zoom levels.
//
// TODO: Circular mask around crosshairs when zoomed in.
// TODO: Shell ejection.
// TODO: Finalize kickback.
// TODO: Animated zoom effect?
//
//=============================================================================//

#include "cbase.h"
#include "ammodef.h"
#include "npcevent.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "gamerules.h"				// For g_pGameRules
#include "in_buttons.h"
#include "soundent.h"
#include "vstdlib/random.h"
#include "npc_strider.h"
#include "hl2mp_player_fix.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSniperBullet : public CBaseEntity
{
public:
	DECLARE_CLASS( CSniperBullet, CBaseEntity );

	CSniperBullet( void ) { Init(); }

	Vector	m_vecDir;

	Vector		m_vecStart;
	Vector		m_vecEnd;

	float	m_flLastThink;
	float	m_SoundTime;
	int		m_AmmoType;
	int		m_PenetratedAmmoType;
	float	m_Speed;
	bool	m_bDirectShot;

	void Precache( void );
	bool IsActive( void ) { return m_fActive; }

	bool Start( const Vector &vecOrigin, const Vector &vecTarget, CBaseEntity *pOwner, bool bDirectShot );
	void Stop( void );

	void BulletThink( void );

	void Init( void );

	DECLARE_DATADESC();

private:

	// Only one shot per sniper at a time. If a bullet hasn't
	// hit, the shooter must wait.
	bool	m_fActive;

	// This tracks how many times this single bullet has 
	// struck. This is for penetration, so the bullet can
	// go through things.
	int		m_iImpacts;
};



#define SNIPER_CONE_PLAYER					vec3_origin	// Spread cone when fired by the player.
#define SNIPER_CONE_NPC						vec3_origin	// Spread cone when fired by NPCs.
#define SNIPER_BULLET_COUNT_PLAYER			1			// Fire n bullets per shot fired by the player.
#define SNIPER_BULLET_COUNT_NPC				1			// Fire n bullets per shot fired by NPCs.
#define SNIPER_TRACER_FREQUENCY_PLAYER		0			// Draw a tracer every nth shot fired by the player.
#define SNIPER_TRACER_FREQUENCY_NPC			0			// Draw a tracer every nth shot fired by NPCs.
#define SNIPER_KICKBACK						3			// Range for punchangle when firing.

#define SNIPER_ZOOM_RATE					0.4			// Interval between zoom levels in seconds.


//-----------------------------------------------------------------------------
// Discrete zoom levels for the scope.
//-----------------------------------------------------------------------------
static int g_nZoomFOV[] =
{
	25,
	5
};

class CWeaponSniperRifle : public CBaseCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponSniperRifle, CBaseCombatWeapon );

	CWeaponSniperRifle(void);
	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	void Precache( void );
	void Spawn( void );
	const Vector &GetBulletSpread( void );
	const char *GetTracerType( void ) { return "StriderTracer"; }
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void ItemPostFrame( void );
	void PrimaryAttack( void );
	bool Reload( void );
	void Zoom( void );
	float GetFireRate( void ) { return 1; };
	float	GetMinRestTime() { return 0.5; }
	float	GetMaxRestTime() { return 0.5; }

	DECLARE_ACTTABLE();

private:
	float m_fNextZoom;
	int m_nZoomLevel;
};

LINK_ENTITY_TO_CLASS( weapon_sniperrifle, CWeaponSniperRifle );
PRECACHE_WEAPON_REGISTER(weapon_sniperrifle);

BEGIN_DATADESC( CWeaponSniperRifle )

	DEFINE_FIELD( m_fNextZoom, FIELD_FLOAT ),
	DEFINE_FIELD( m_nZoomLevel, FIELD_INTEGER ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Maps base activities to weapons-specific ones so our characters do the right things.
//-----------------------------------------------------------------------------
acttable_t CWeaponSniperRifle::m_acttable[] = 
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_AR2,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_AR2,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_AR2,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_AR2,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_AR2,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_AR2,					false },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },		// FIXME: hook to AR2 unique
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },		// FIXME: hook to AR2 unique
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },		// FIXME: hook to AR2 unique

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },		// FIXME: hook to AR2 unique
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },		// FIXME: hook to AR2 unique
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
//	{ ACT_RANGE_ATTACK2, ACT_RANGE_ATTACK_AR2_GRENADE, true },
};

IMPLEMENT_ACTTABLE(CWeaponSniperRifle);

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CWeaponSniperRifle::CWeaponSniperRifle( void )
{
	m_fNextZoom = gpGlobals->curtime;
	m_nZoomLevel = 0;

	m_bReloadsSingly = true;

	m_fMinRange1		= 65;
	m_fMinRange2		= 65;
	m_fMaxRange1		= 8048;
	m_fMaxRange2		= 8048;
}

void CWeaponSniperRifle::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SNIPER_RIFLE_FIRE:
		{
			if( gpGlobals->curtime <= m_flNextPrimaryAttack )
				return;

			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();
			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );
			WeaponSound( SINGLE_NPC );
			pOperator->DoMuzzleFlash();
			m_iClip1 = m_iClip1 - 1;

			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );
			pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, GetAmmoDef()->Index( "SniperRound" ), 0 );
			m_flNextPrimaryAttack = gpGlobals->curtime + 3.0;
		}
		break;

		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Turns off the zoom when the rifle is holstered.
//-----------------------------------------------------------------------------
bool CWeaponSniperRifle::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if (pPlayer != NULL)
	{
		if ( m_nZoomLevel != 0 )
		{
			engine->ClientCommand( pPlayer->edict(), "r_screenoverlay 0" );
			pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
			pPlayer->StopZooming();
			pPlayer->ShowViewModel(true);
			m_nZoomLevel = 0;
		}
	}

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded to handle the zoom functionality.
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::ItemPostFrame( void )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if( pPlayer == NULL )
		return;

	if( m_nZoomLevel != 0 && (pPlayer->m_nButtons & IN_ZOOM) )
	{
		pPlayer->m_nButtons &= ~IN_ZOOM;
	}

	if ( m_bInReload && m_nZoomLevel != 0 )
	{
		engine->ClientCommand( pPlayer->edict(), "r_screenoverlay 0" );
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		pPlayer->StopZooming();
		pPlayer->ShowViewModel(true);
		m_nZoomLevel = 0;
	}

	if( m_bInReload && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		FinishReload();
		m_bInReload = false;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
		return;
	}

	if( pPlayer->m_nButtons & IN_ATTACK2 )
	{
		if( m_fNextZoom <= gpGlobals->curtime )
		{
			Zoom();
			pPlayer->m_nButtons &= ~IN_ATTACK2;
		}
	}

	BaseClass::ItemPostFrame();
}

void CWeaponSniperRifle::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Same as base reload but doesn't change the owner's next attack time. This
//			lets us zoom out while reloading. This hack is necessary because our
//			ItemPostFrame is only called when the owner's next attack time has
//			expired.
// Output : Returns true if the weapon was reloaded, false if no more ammo.
//-----------------------------------------------------------------------------
bool CWeaponSniperRifle::Reload( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if (!pOwner)
	{
		return false;
	}

	if ( m_iClip1 > 0 )
			return false;

	WeaponSound(RELOAD);
	SendWeaponAnim( ACT_VM_RELOAD );

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	m_bInReload = true;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast safely.
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
		return;

	if ( gpGlobals->curtime >= m_flNextPrimaryAttack ) {
		// MUST call sound before removing a round from the clip of a CMachineGun dvs: does this apply to the sniper rifle? I don't know.
		WeaponSound(SINGLE);
		pPlayer->DoMuzzleFlash();

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );

		// player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );

		// Don't fire again until fire animation has completed
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;

		Vector vecSrc	 = pPlayer->Weapon_ShootPosition();
		Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );	

		/*CSniperBullet	*pBullet;

		pBullet = (CSniperBullet *)Create( "sniperbullet", vecSrc, pPlayer->GetLocalAngles(), NULL );

		Assert( pBullet != NULL );

		Vector vecMuzzleDir;
		AngleVectors( pPlayer->EyeAngles(), &vecMuzzleDir);
		Vector vecStart, vecEnd;
		VectorMA( pPlayer->EyePosition(), 6000, vecMuzzleDir, vecEnd );
		VectorMA( pPlayer->EyePosition(), 5,   vecMuzzleDir, vecStart );
		trace_t tr;
		UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

		if( !pBullet->Start( vecSrc, tr.endpos, this, false ) )
		{
			
		}

		pBullet->SetOwnerEntity( this );

		short sFlashSprite = PrecacheModel( "sprites/muzzleflash1.vmt" );
		CPVSFilter filter( vecSrc );
		te->Sprite( filter, 0.0, &vecSrc, sFlashSprite, 0.3, 255 );
		*/
		// Fire the bullets
		FireBullets( SNIPER_BULLET_COUNT_PLAYER, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, GetAmmoDef()->Index( "SniperRound" ), 1, -1, -1, 0, pPlayer );

		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 200, 0.2 );

		QAngle vecPunch(random->RandomFloat( -SNIPER_KICKBACK, SNIPER_KICKBACK ), 0, 0);
		pPlayer->ViewPunch(vecPunch);
		//pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
		m_iClip1 = m_iClip1 - 1;
	}

	// Register a muzzleflash for the AI.
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
}

//-----------------------------------------------------------------------------
// Purpose: Zooms in using the sniper rifle scope.
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::Zoom( void )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if (!pPlayer || m_bInReload )
	{
		return;
	}

	if (m_nZoomLevel >= sizeof(g_nZoomFOV) / sizeof(g_nZoomFOV[0]))
	{
		engine->ClientCommand( pPlayer->edict(), "r_screenoverlay 0" );
		pPlayer->StopZooming();
		pPlayer->ShowViewModel(true);
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		WeaponSound(SPECIAL2);	
		m_nZoomLevel = 0;
	}
	else
	{
		if( m_nZoomLevel == 0 ) {
			pPlayer->m_Local.m_iHideHUD |= HIDEHUD_CROSSHAIR;
			engine->ClientCommand( pPlayer->edict(), "r_screenoverlay hl2mp.ru/scope_sniper02.vmt" );
			//pPlayer->StartZooming();
			pPlayer->ShowViewModel(false);
		}

		pPlayer->SetFOV( pPlayer, g_nZoomFOV[m_nZoomLevel], 0.4 );
		WeaponSound(SPECIAL1);
		m_nZoomLevel++;
	}

	m_fNextZoom = gpGlobals->curtime + SNIPER_ZOOM_RATE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : virtual const Vector&
//-----------------------------------------------------------------------------
const Vector &CWeaponSniperRifle::GetBulletSpread( void )
{
	static Vector cone = SNIPER_CONE_PLAYER;
	return cone;
}