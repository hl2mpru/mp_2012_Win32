#include "cbase.h"
#include "charge_token.h"
#include "hl2mp_player_fix.h"

LINK_ENTITY_TO_CLASS( charge_token, CChargeToken );

BEGIN_DATADESC( CChargeToken )
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hSpitEffect, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLifetime, FIELD_TIME ),
	
	DEFINE_ENTITYFUNC( SeekThink ),
	DEFINE_ENTITYFUNC( SeekTouch ),
END_DATADESC()

ConVar sv_healer_particle("sv_healer_particle", "vortigaunt_charge_token");

//-----------------------------------------------------------------------------
// Purpose: Create a charge token for the player to collect
// Input  : &vecOrigin - Where we start
//			*pOwner - Who created us
//			*pTarget - Who we're seeking towards
//-----------------------------------------------------------------------------
CChargeToken *CChargeToken::CreateChargeToken( const Vector &vecOrigin, CBaseEntity *pOwner, CBaseEntity *pTarget )
{
	CChargeToken *pToken = (CChargeToken *) CreateEntityByName( "charge_token" );
	if ( pToken == NULL )
		return NULL;

	// Set up our internal data
	UTIL_SetOrigin( pToken, vecOrigin );
	pToken->SetOwnerEntity( pOwner );
	pToken->SetTargetEntity( pTarget );
	pToken->SetThink( &CChargeToken::SeekThink );
	pToken->SetTouch( &CChargeToken::SeekTouch );
	pToken->Spawn();
	
	// Start out at the same velocity as our owner
	Vector vecInitialVelocity;
	CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating *>(pOwner);
	if ( pAnimating != NULL )
	{
		vecInitialVelocity = pAnimating->GetGroundSpeedVelocity();
	}
	else
	{
		vecInitialVelocity = pTarget->GetSmoothedVelocity();				
	}

	// Start out at that speed
	pToken->SetAbsVelocity( vecInitialVelocity );
	pToken->EmitSound("Weapon_Physgun.Off");
	return pToken;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChargeToken::Precache( void )
{
	PrecacheParticleSystem( sv_healer_particle.GetString() );
	PrecacheScriptSound( "Weapon_Physgun.Off" );
	PrecacheScriptSound( "HealthVial.Touch" );
}

//-----------------------------------------------------------------------------
// Purpose: We want to move through grates!
//-----------------------------------------------------------------------------
unsigned int CChargeToken::PhysicsSolidMaskForEntity( void ) const 
{
	return MASK_SHOT; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChargeToken::Spawn( void )
{
	Precache();

	// Point-sized
	UTIL_SetSize( this, -Vector(1,1,1), Vector(1,1,1) );

	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
	SetGravity( 0.0f );

	// No model but we still need to force this!
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	SetNextThink( gpGlobals->curtime + 0.05f );

	m_flLifetime = gpGlobals->curtime + 8.0;
	m_hSpitEffect = (CParticleSystem *)CreateEntityByName( "info_particle_system" );
	if( m_hSpitEffect != NULL ) {
		m_hSpitEffect->KeyValue( "start_active", "1" );
		m_hSpitEffect->KeyValue( "effect_name", sv_healer_particle.GetString() );
		m_hSpitEffect->SetParent( this );
		m_hSpitEffect->SetLocalOrigin( vec3_origin );
		DispatchSpawn( m_hSpitEffect );
		if ( gpGlobals->curtime > 0.5f )
			m_hSpitEffect->Activate();
	}
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Creates an influence vector which causes the token to move away from obstructions
//-----------------------------------------------------------------------------
Vector CChargeToken::GetSteerVector( const Vector &vecForward )
{
	Vector vecSteer = vec3_origin;
	Vector vecRight, vecUp;
	VectorVectors( vecForward, vecRight, vecUp );

	// Use two probes fanned out a head of us
	Vector vecProbe;
	float flSpeed = GetAbsVelocity().Length();

	// Try right 
	vecProbe = vecForward + vecRight;	
	vecProbe *= flSpeed;

	// We ignore multiple targets
	CTraceFilterSimpleList filterSkip( COLLISION_GROUP_NONE );
	filterSkip.AddEntityToIgnore( this );
	filterSkip.AddEntityToIgnore( GetOwnerEntity() );
	filterSkip.AddEntityToIgnore( m_hTarget );

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecProbe, MASK_SHOT, &filterSkip, &tr );
	vecSteer -= vecRight * 100.0f * ( 1.0f - tr.fraction );

	// Try left
	vecProbe = vecForward - vecRight;
	vecProbe *= flSpeed;

	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecProbe, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	vecSteer += vecRight * 100.0f * ( 1.0f - tr.fraction );

	return vecSteer;
}

#define VTOKEN_MAX_SPEED	320.0f	// U/sec
#define VTOKEN_ACCEL_SPEED	320.0f	// '

//-----------------------------------------------------------------------------
// Purpose: Move towards our target entity with accel/decel parameters
//-----------------------------------------------------------------------------
void CChargeToken::SeekThink( void )
{
	// Move away from the creator and towards the target
	if ( m_hTarget == NULL || m_flLifetime < gpGlobals->curtime || !m_hTarget->IsAlive() )
	{
		// TODO: Play an extinguish sound and fade out
		FadeAndDie();
		return;
	}

	CBaseEntity *pOwner = GetOwnerEntity();
	if( pOwner && pOwner->IsPlayer() ) {
		if( m_hTarget->GetHealth() >= m_hTarget->GetMaxHealth() || !FVisible( m_hTarget, MASK_SHOT ) ) {
			FadeAndDie();
			return;
		}
	}

	// Find the direction towards our goal and start to go there
	Vector vecDir = ( m_hTarget->WorldSpaceCenter() - GetAbsOrigin() );
	VectorNormalize( vecDir );

	float flSpeed = GetAbsVelocity().Length();
	float flDelta = gpGlobals->curtime - GetLastThink();

	if ( flSpeed < VTOKEN_MAX_SPEED )
	{
		// Accelerate by the desired amount
		flSpeed += ( VTOKEN_ACCEL_SPEED * flDelta );
		if ( flSpeed > VTOKEN_MAX_SPEED )
			flSpeed = VTOKEN_MAX_SPEED;

	}

	// Steer!
	Vector vecRight, vecUp;
	VectorVectors( vecDir, vecRight, vecUp );
	Vector vecOffset = vec3_origin;
	vecOffset += vecUp * cos( gpGlobals->curtime * 20.0f ) * 200.0f * gpGlobals->frametime;
	vecOffset += vecRight * sin( gpGlobals->curtime * 15.0f ) * 200.0f * gpGlobals->frametime;
	
	vecOffset += GetSteerVector( vecDir );

	SetAbsVelocity( ( vecDir * flSpeed ) + vecOffset );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChargeToken::SeekTouch( CBaseEntity *pOther )
{
	if ( pOther != m_hTarget )
		return;

	EmitSound( "HealthVial.Touch" );

	int health = pOther->GetHealth();
	if ( health < pOther->GetMaxHealth() )
	{
		int maxHealth = pOther->GetMaxHealth();
		int m_rHealth = random->RandomInt(2,7);
		if( pOther->IsNPC() && maxHealth > 100 )
			m_rHealth = maxHealth/100*m_rHealth;

		pOther->TakeHealth( m_rHealth, DMG_GENERIC );
		
		CHL2MP_Player_fix *pPlayer = dynamic_cast<CHL2MP_Player_fix*>( GetOwnerEntity() );
		if( pPlayer ) {
			int health = pPlayer->m_iHealth;
			if( health <= 40 ) {
				CTakeDamageInfo	info(this, this, 1, DMG_DISSOLVE);
				pPlayer->TakeDamage(info);
			}
			else
				pPlayer->m_iHealth -= 1;
			
		}
	}

	SetSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	SetParent( pOther );

	FadeAndDie();
}

void CChargeToken::FadeAndDie( void )
{
	if ( m_hSpitEffect ) {
		UTIL_Remove( m_hSpitEffect );
		m_hSpitEffect = NULL;
	}

	SetTouch( NULL );

	SetAbsVelocity( vec3_origin );

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 2.0f );
}
