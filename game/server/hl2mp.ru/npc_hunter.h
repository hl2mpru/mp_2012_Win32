//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Expose an IsAHunter function
//
//=============================================================================//
#include "ai_behavior_follow.h"
#include "ai_moveprobe.h"
#include "ai_senses.h"
#include "ai_speech.h"
#include "ai_task.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_baseactor.h"
#include "ai_waypoint.h"
#include "ai_link.h"
#include "ai_hint.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "ai_tacticalservices.h"
#include "beam_shared.h"
#include "datacache/imdlcache.h"
#include "eventqueue.h"
#include "gib.h"
#include "globalstate.h"
#include "hierarchy.h"
#include "movevars_shared.h"
#include "npcevent.h"
#include "saverestore_utlvector.h"
#include "particle_parse.h"
#include "te_particlesystem.h"
#include "sceneentity.h"
#include "shake.h"
#include "soundenvelope.h"
#include "soundent.h"
#include "SpriteTrail.h"
#include "IEffects.h"
#include "engine/IEngineSound.h"
#include "bone_setup.h"
#include "studio.h"
#include "ai_route.h"
#include "ammodef.h"
#include "npc_bullseye.h"
#include "physobj.h"
#include "ai_memory.h"
#include "collisionutils.h"
#include "shot_manipulator.h"
#include "steamjet.h"
#include "physics_prop_ragdoll.h"
#include "vehicle_base.h"
#include "coordsize.h"
#include "hl2_shareddefs.h"
#include "te_effect_dispatch.h"
#include "beam_flags.h"
#include "prop_combine_ball.h"
#include "explode.h"
#include "weapon_physcannon.h"
#include "weapon_striderbuster.h"
#include "monstermaker.h"
#include "weapon_rpg.h"

#ifndef NPC_HUNTER_H
#define NPC_HUNTER_H

class CHunterFlechette : public CPhysicsProp, public IParentPropInteraction
{
	DECLARE_CLASS( CHunterFlechette, CPhysicsProp );

public:

	CHunterFlechette();
	~CHunterFlechette();

	Class_T Classify() { return CLASS_NONE; }
	
	bool WasThrownBack()
	{
		return m_bThrownBack;
	}

public:

	void Spawn();
	void Activate();
	void Precache();
	void Shoot( Vector &vecVelocity, bool bBright );
	void SetSeekTarget( CBaseEntity *pTargetEntity );
	void Explode();

	bool CreateVPhysics();

	unsigned int PhysicsSolidMaskForEntity() const;
	static CHunterFlechette *FlechetteCreate( const Vector &vecOrigin, const QAngle &angAngles, CBaseEntity *pentOwner = NULL );

	// IParentPropInteraction
	void OnParentCollisionInteraction( parentCollisionInteraction_t eType, int index, gamevcollisionevent_t *pEvent );
	void OnParentPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );

protected:

	void SetupGlobalModelData();

	void StickTo( CBaseEntity *pOther, trace_t &tr );

	void BubbleThink();
	void DangerSoundThink();
	void ExplodeThink();
	void DopplerThink();
	void SeekThink();

	bool CreateSprites( bool bBright );

	void FlechetteTouch( CBaseEntity *pOther );

	Vector m_vecShootPosition;
	EHANDLE m_hSeekTarget;
	bool m_bThrownBack;

	DECLARE_DATADESC();
	//DECLARE_SERVERCLASS();
};


#if defined( _WIN32 )
#pragma once
#endif

class CBaseEntity;

/// true if given entity pointer is a hunter.
bool Hunter_IsHunter(CBaseEntity *pEnt);

// call throughs for member functions

void Hunter_StriderBusterAttached( CBaseEntity *pHunter, CBaseEntity *pAttached );
void Hunter_StriderBusterDetached( CBaseEntity *pHunter, CBaseEntity *pAttached );
void Hunter_StriderBusterLaunched( CBaseEntity *pBuster );

#endif
