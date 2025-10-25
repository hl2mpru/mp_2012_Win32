#include "cbase.h"
#include "ai_basenpc.h"
#include "mapentities.h"
#include "te_effect_dispatch.h"
#include "ai_moveprobe.h"
#include "hl2mp_player.h"
#include "ai_network.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "ai_node.h"
#include "eventqueue.h"
#include "hl2mp_admins.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( info_spawnnode, CServerOnlyPointEntity );

static void DispatchActivate( CBaseEntity *pEntity )
{
	bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
	pEntity->Activate();
	mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );
}

class npcRandom : public CServerOnlyPointEntity
{
	DECLARE_CLASS( npcRandom, CServerOnlyPointEntity );
	DECLARE_DATADESC();
public:
	npcRandom() {};
	~npcRandom()
	{
		if(manifest)
			manifest->deleteThis();
	}

	void Spawn( void );
	void TimerThink( void );
	bool FindRandomSpot( Vector *pResult, QAngle *pAngle, const char *szGroup = NULL );
	bool ItSafeZone( Vector pSpot );
	bool CheckSpotForRadius( Vector *pResult, const Vector &pSpot, CAI_BaseNPC *pNPC, float radius );
	void EffectSpawn( Vector vStartPost, CAI_BaseNPC *pNPC = NULL );
	virtual	int	 ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }

	KeyValues *manifest;
	KeyValues *lastpoint;
	KeyValues *sub;

	int m_nCurrentNode;
	float m_flWaveTime;
	float m_flWaveInterval;
	bool bNoFan;
	EHANDLE m_pLastPoint;

	//Показывать визуально точки на карте spawnnode из конфига
	bool dShowGroup;
};

class npcRandom *gpRnd;

void ShowSpawn( const char *removeName = NULL ) {
	if( removeName != NULL ) {
		CBaseEntity *ent = gEntList.FindEntityByName( NULL, removeName );
		if( ent )
			ent->Remove();
		return;
	}

	KeyValues *entry= gpRnd->manifest->FindKey( "spawnnode" );
	for( KeyValues *pKV = entry->GetFirstSubKey(); pKV != NULL; pKV = pKV->GetNextKey() )
	{
		if( gEntList.FindEntityByName( NULL, entry->GetName() ) ) {
			
			continue;
		}
		
		CBaseEntity *pPoint = CreateEntityByName( "prop_dynamic" );
		if( pPoint ) {
			pPoint->KeyValue("origin", pKV->GetString("origin") );
			pPoint->KeyValue("angles", pKV->GetString("angles") );
			pPoint->KeyValue("targetname", pKV->GetName() );
			pPoint->KeyValue( "fademindist", "-1" );
			pPoint->KeyValue( "fadescale", "1" );
			pPoint->KeyValue( "MaxAnimTime", "10" );
			pPoint->KeyValue( "MinAnimTime", "5" );
			pPoint->KeyValue( "model", "models/player.mdl" );
			pPoint->KeyValue( "modelscale", "1.0" );
			pPoint->KeyValue( "renderamt", "255" );
			pPoint->KeyValue( "rendercolor", "255 255 255" );
			pPoint->KeyValue( "solid", "6" );
			pPoint->KeyValue( "spawnflags", "256" );
			DispatchSpawn( pPoint );
			//g_EventQueue.AddEvent( pPoint, "Kill", 2.5f, NULL, NULL );
		}
	}
}

CON_COMMAND( mp_rndnpc_showspawn_toogle, "Показать точки респауна для групп" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !UTIL_IsCommandIssuedByServerAdmin() && pPlayer && !IsAdmin( pPlayer, FL_ADMIN ) )
		return;

	if( gpRnd->dShowGroup ) {
		KeyValues *entry = gpRnd->manifest->FindKey( "spawnnode" );
		for( KeyValues *pKV = entry->GetFirstSubKey(); pKV != NULL; pKV = pKV->GetNextKey() )
		{
			CBaseEntity *ent = NULL;
			while ( ( ent = gEntList.FindEntityByName(ent, pKV->GetName() ) ) != NULL )
			{
				ent->Remove();
			}
		}
		gpRnd->dShowGroup = false;
	}
	else {
		gpRnd->dShowGroup = true;
		ShowSpawn();
	}
}

#define VectorToString(v) (static_cast<const char *>(CFmtStr("%f %f %f", (v).x, (v).y, (v).z)))

void CC_mp_rndnpc_addspawn( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( pPlayer && !IsAdmin( pPlayer, FL_ADMIN ) )
		return;

	KeyValues *sub = new KeyValues( VectorToString( pPlayer->GetAbsOrigin() ) );
	sub->SetString( "group", args.Arg(1) );
	sub->SetString( "origin", VectorToString( pPlayer->GetAbsOrigin() ) );
	sub->SetString( "angles", VectorToString( pPlayer->GetAbsAngles() ) );
	
	KeyValues *spawnnode = gpRnd->manifest->FindKey( "spawnnode", true );
	gpRnd->manifest->RemoveSubKey( spawnnode );
	spawnnode->AddSubKey( sub );
	gpRnd->manifest->AddSubKey( spawnnode );

	char fullPath[_MAX_PATH];
	if( filesystem->GetLocalPath( UTIL_VarArgs( "maps/%s_rndnpc.txt", STRING( gpGlobals->mapname) ), fullPath, sizeof(fullPath) ) ) 
		gpRnd->manifest->SaveToFile( filesystem, fullPath );

	ShowSpawn();
}

static ConCommand mp_rndnpc_addspawn( "mp_rndnpc_addspawn", CC_mp_rndnpc_addspawn, "", FCVAR_GAMEDLL );

LINK_ENTITY_TO_CLASS( info_npc_random, npcRandom );

BEGIN_DATADESC( npcRandom )

	DEFINE_THINKFUNC( TimerThink ),

END_DATADESC()

void npcRandom::Spawn( void )
{
	gpRnd = this;
	dShowGroup = false;
	SetThink( &npcRandom::TimerThink );
	SetNextThink( gpGlobals->curtime + 1.0 );

	SetName( AllocPooledString("info_npc_random") );

	m_nCurrentNode = 0;

	manifest = new KeyValues("EntityList");
	if( !manifest->LoadFromFile(filesystem, UTIL_VarArgs( "maps/%s_rndnpc.txt", STRING( gpGlobals->mapname) ), "GAME") && !manifest->LoadFromFile(filesystem, "rndnpc.txt", "GAME") ) {
		manifest->deleteThis();
		Remove();
		return;
	}

	KeyValues *settings = manifest->FindKey( "settings" );
	if( settings != NULL ) {
		if( settings->GetInt("enable") )
			engine->ServerCommand( UTIL_VarArgs("mp_rndnpc %d\n", settings->GetInt("enable") ) );

		if( settings->FindKey("maxnpc") ) 
			engine->ServerCommand( UTIL_VarArgs("mp_rndnpc_maxnpc %d\n", settings->GetInt("maxnpc") ) );

		if( settings->FindKey("visible") )
			engine->ServerCommand( UTIL_VarArgs("mp_rndnpc_visible %d\n", settings->GetInt("visible") ) );

		if( settings->FindKey("randomspot") )
			engine->ServerCommand( UTIL_VarArgs("mp_rndnpc_randomspot %d\n", settings->GetInt("randomspot") ) );

		if( settings->FindKey("wavetime") )
			engine->ServerCommand( UTIL_VarArgs("mp_rndnpc_wavetime %d\n", settings->GetInt("wavetime") ) );

		if( settings->FindKey("waveinterval") )
			engine->ServerCommand( UTIL_VarArgs("mp_rndnpc_waveinterval %d\n", settings->GetInt("waveinterval") ) );
	}

	PrecacheSound("hl2mp.ru/rndspawn.wav");
}

void CC_mp_rndnpc_kill( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if( ent->GetOwnerEntity() && ent->GetOwnerEntity()->ClassMatches( "info_npc_random" ) )
			ent->TakeDamage( CTakeDamageInfo( ent, ent, ent->GetHealth()+1000, DMG_GENERIC ) );
	}
}

static ConCommand mp_rndnpc_kill( "mp_rndnpc_kill", CC_mp_rndnpc_kill, "", FCVAR_GAMEDLL );

ConVar mp_rndnpc( "mp_rndnpc", "0" );
ConVar mp_rndnpc_maxnpc( "mp_rndnpc_maxnpc", "50" );
ConVar mp_rndnpc_visible( "mp_rndnpc_visible", "1" );
ConVar mp_rndnpc_randomspot( "mp_rndnpc_randomspot", "1" );
ConVar mp_rndnpc_wavetime( "mp_rndnpc_wavetime", "300" );
ConVar mp_rndnpc_waveinterval( "mp_rndnpc_waveinterval", "120" );

void npcRandom::TimerThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.25 );

	if( !mp_rndnpc.GetBool() )
		return;

	if( m_flWaveTime == 0.0 )
		m_flWaveTime = gpGlobals->curtime + mp_rndnpc_wavetime.GetFloat();

	if( m_flWaveInterval > gpGlobals->curtime ) {
		m_flWaveTime = gpGlobals->curtime + mp_rndnpc_wavetime.GetFloat();
		return;
	}

	if( m_flWaveTime < gpGlobals->curtime )
		m_flWaveInterval = gpGlobals->curtime + mp_rndnpc_waveinterval.GetFloat();

	int maxnpc = 0;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
		if( ent->GetOwnerEntity() == this )
			maxnpc++;

	float totalPlayers = UTIL_GetPlayerOnline();
	
	int allowmaxnpc = int( floor( totalPlayers / gpGlobals->maxClients * mp_rndnpc_maxnpc.GetInt() + 0.5 ) );

	if( allowmaxnpc <= maxnpc )
		return;
	
	if( sub == NULL ) {
		sub = manifest->FindKey( "npclist" );
		sub = sub->GetFirstSubKey();
	}

	if( sub == NULL ) {
		Msg("ERROR info_npc_random sub=null!\n");
		Remove();
		return;
	}

	maxnpc = 0;
	ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
		if( ent->GetOwnerEntity() == this && FClassnameIs(ent,sub->GetString("classname")) )
			maxnpc++;

	if( sub->FindKey("maxonmap") && maxnpc >= sub->GetInt("maxonmap") ) {
		sub = sub->GetNextKey();
		return;
	}

	bNoFan = false;
	Vector pSpot;
	QAngle pAngle = vec3_angle;
	if( !FindRandomSpot( &pSpot, &pAngle, sub->GetString("group") ) || ItSafeZone( pSpot ) ) {
		sub = sub->GetNextKey();
		return;
	}

	CAI_BaseNPC *pNPC = dynamic_cast< CAI_BaseNPC * >( CreateEntityByName( sub->GetString("classname") ) );
	if( pNPC == NULL )
		return;

	pNPC->KeyValue( "origin", VectorToString( pSpot ) );
	pNPC->KeyValue( "angles", VectorToString( pAngle ) );
	pNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );
	pNPC->AddSpawnFlags( SF_NPC_FADE_CORPSE );
	if( sub->FindKey( "skin") )
		pNPC->KeyValue( "skin",sub->GetString("skin") );

	if( sub->FindKey("additionalequipment") )
		pNPC->KeyValue( "additionalequipment", sub->GetString("additionalequipment") );

	if( sub->FindKey("model") )
		pNPC->KeyValue( "model", sub->GetString("model") );

	bool isFriend = sub->GetBool("friend");

	if( isFriend ) {
		pNPC->pfriend = sub->GetBool("friend");
		pNPC->SetCollisionGroup( COLLISION_GROUP_PLAYER );
	}

	//pNPC->KeyValue( "npcname", UTIL_VarArgs( "[RND]%s", nameReplace(pNPC) ) );
	DispatchSpawn( pNPC );
	pNPC->SetOwnerEntity( this );

	DispatchActivate( pNPC );

	pNPC->CapabilitiesAdd( bits_CAP_MOVE_JUMP );

	if( CheckSpotForRadius( &pSpot, pSpot, pNPC, pNPC->GetHullWidth() * 1.5) ) {
		pNPC->SetAbsOrigin( pSpot );
		//EffectSpawn( pSpot, pNPC );
		Msg( "info_npc_random: Spawn: %s NodeID: %d\n", sub->GetString("classname"), m_nCurrentNode );
		sub = sub->GetNextKey();
		SetNextThink( gpGlobals->curtime + 0.5 );
		return;
	}
	
	sub = sub->GetNextKey();
	UTIL_Remove( pNPC );
}

bool npcRandom::FindRandomSpot( Vector *pResult, QAngle *pAngle, const char *szGroup )
{
	KeyValues *spawnnode= manifest->FindKey( "spawnnode" );
	if( spawnnode ) {
		if( lastpoint == NULL )
			lastpoint = spawnnode->GetFirstSubKey();

		Vector pKvVector;
		UTIL_StringToVector( pKvVector.Base(), lastpoint->GetString("origin") );
		*pResult = pKvVector + Vector( 0, 0, 32 );

		QAngle pKvAngle;
		UTIL_StringToVector( pKvAngle.Base(), lastpoint->GetString("angles") );
		*pAngle = pKvAngle;

		if( szGroup ) {
			if( strcmpi( szGroup, lastpoint->GetString("group") ) == 0 ) {
				lastpoint = lastpoint->GetNextKey();
				bNoFan = true;
				return true;
			}
			lastpoint = lastpoint->GetNextKey();
			return false;
		}
		bNoFan = true;
		lastpoint = lastpoint->GetNextKey();
		return true;
	}

	bool randomspot = mp_rndnpc_randomspot.GetBool();
	m_pLastPoint = gEntList.FindEntityByClassname( m_pLastPoint, "info_spawnnode" );
	if( !m_pLastPoint )
		m_pLastPoint = gEntList.FindEntityByClassname( m_pLastPoint, "info_spawnnode" );

	if( m_pLastPoint ) {
		*pResult = m_pLastPoint->GetAbsOrigin();
		*pAngle = m_pLastPoint->GetAbsAngles();
		return true;
	}
	
	if( randomspot )
		m_nCurrentNode = random->RandomInt( 0, g_pBigAINet->NumNodes()-1 );
	else
		if( m_nCurrentNode++ >= g_pBigAINet->NumNodes() )
			m_nCurrentNode = 0;

	CAI_Node *pNode = g_pBigAINet->GetNode( m_nCurrentNode );
	if( pNode != NULL && pNode->GetType() == NODE_GROUND ) {
		*pResult = pNode->GetOrigin() + Vector( 0, 0, 32 );
		return true;
	}
	return false;
}

const char *gpPointListSpawn[] = {
	"info_player_coop",
	"info_player_deathmatch",
	"info_player_start",
	"info_player_combine",
	"info_player_rebel"
};

bool npcRandom::ItSafeZone( Vector pSpot )
{
	for ( int i = 0; i < ARRAYSIZE( gpPointListSpawn ); ++i )
	{
		CBaseEntity *pSpawn = NULL;
		while ( ( pSpawn = gEntList.FindEntityByClassname( pSpawn, gpPointListSpawn[i] ) ) != NULL )
		{
			float flDist = ( pSpawn->GetAbsOrigin() - pSpot ).LengthSqr();

			if( sqrt( flDist ) < 256.0 )
				return true;
		}
	}

	for (int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient)
	{
		CBasePlayer *pEnt = UTIL_PlayerByIndex(iClient);
		if ( !pEnt || !pEnt->IsAlive() )
			continue;

		float flDist = ( pEnt->GetAbsOrigin() - pSpot ).LengthSqr();

		if( mp_rndnpc_visible.GetBool() && pEnt->FInViewCone( pSpot ) && pEnt->FVisible( pSpot, MASK_SOLID_BRUSHONLY ) || sqrt( flDist ) < 256.0 )
			return true;
	}

	KeyValues *safezone = manifest->FindKey( "safezone" );
	if( safezone == NULL )
		return false;

	for ( KeyValues *sub = safezone->GetFirstSubKey(); sub != NULL ; sub = sub->GetNextKey() ) 
	{
		Vector pVector;
		UTIL_StringToVector(pVector.Base(), sub->GetString("origin") );

		if( sub->GetBool("visible") ) {
			trace_t tr;
			UTIL_TraceLine(pVector, pSpot, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr);
			if( tr.fraction == 1.0 )
				return true;
		}

		float flDist = ( pSpot - pVector ).LengthSqr();

		if( sqrt( flDist ) <= sub->GetFloat("radius") )
			return true;

	}
	return false;
}

bool npcRandom::CheckSpotForRadius( Vector *pResult, const Vector &pSpot, CAI_BaseNPC *pNPC, float radius )
{
	QAngle fan;
	Vector vStartPos;
	fan.x = 0;
	fan.z = 0;
	trace_t tr;

	UTIL_TraceLine(pSpot, pSpot - Vector( 0, 0, 64 ), MASK_SOLID, pNPC, COLLISION_GROUP_NONE, &tr);
	if( !(UTIL_PointContents( tr.endpos ) & CONTENTS_WATER ) && tr.fraction != 1.0 && !tr.allsolid && strcmp("**studio**", tr.surface.name) != 0 ) {
		vStartPos = tr.endpos;
		vStartPos.z += 32.0;
	}
	else
		return false;

	if( bNoFan == false ) {
		for( fan.y = 0 ; fan.y < 360 ; fan.y += 90.0 )
		{
			Vector vecTest;
			Vector vecDir;

			AngleVectors( fan, &vecDir );

			vecTest = vStartPos + vecDir * radius;

			UTIL_TraceLine( vStartPos, vecTest, MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );
			if( tr.fraction != 1.0 )
				return false;
			else
				UTIL_TraceLine( vecTest, vecTest - Vector( 0, 0, 48 ), MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );

			if( tr.fraction == 1.0 )
				return false;
		}
	}
	
	UTIL_TraceHull( vStartPos,
						vStartPos + Vector( 0, 0, 10 ),
						pNPC->GetHullMins(),
						pNPC->GetHullMaxs(),
						MASK_NPCSOLID,
						pNPC,
						COLLISION_GROUP_NONE,
						&tr );

	if( tr.fraction == 1.0 /*&& pNPC->GetMoveProbe()->CheckStandPosition( tr.endpos, MASK_NPCSOLID )*/ ) {
		*pResult = tr.endpos;
		return true;
	}
	return false;
}

void npcRandom::EffectSpawn( Vector vStartPost, CAI_BaseNPC *pNPC )
{
	Vector pOrigin = vStartPost;
	pOrigin.z +=32;
	UTIL_EmitAmbientSound( ENTINDEX( pNPC ), pOrigin, "hl2mp.ru/rndspawn.wav", 1.0, SNDLVL_70dB, 0, 100 );
	
	CBaseEntity *pBeam = CreateEntityByName( "env_beam" );
	if( pBeam ) {
		pBeam->KeyValue("origin", pOrigin);
		pBeam->KeyValue("BoltWidth", "1.8");
		pBeam->KeyValue("life", ".5");
		pBeam->KeyValue("LightningStart", reinterpret_cast<uintptr_t>(pNPC));
		pBeam->KeyValue("NoiseAmplitude", "10.4");
		pBeam->KeyValue("Radius", "200");
		pBeam->KeyValue("renderamt", "150");
		pBeam->KeyValue("rendercolor", "0 255 0");
		pBeam->KeyValue("spawnflags", "34");
		pBeam->KeyValue("StrikeTime", "-.5");
		pBeam->KeyValue("targetname", reinterpret_cast<uintptr_t>(pNPC));
		pBeam->KeyValue("texture", "sprites/lgtning.vmt");
		DispatchSpawn(pBeam);
		g_EventQueue.AddEvent( pBeam, "TurnOn", 0.0f, NULL, NULL ); 
		g_EventQueue.AddEvent( pBeam, "TurnOff", 1.0f, NULL, NULL );
		g_EventQueue.AddEvent( pBeam, "Kill", 2.5f, NULL, NULL );
	}

	CBaseEntity *pSprite = CreateEntityByName( "env_sprite" );
	if( pSprite ) {
		pSprite->KeyValue("origin", pOrigin);
		pSprite->KeyValue("model", "sprites/fexplo1.vmt");
		pSprite->KeyValue("rendercolor", "77 210 130");
		pSprite->KeyValue("renderfx", "14");
		pSprite->KeyValue("rendermode", "3");
		pSprite->KeyValue("spawnflags", "2");
		pSprite->KeyValue("framerate", "10");
		pSprite->KeyValue("GlowProxySize","32");
		pSprite->KeyValue("frame","18");
		DispatchSpawn(pSprite);
		g_EventQueue.AddEvent( pSprite, "ShowSprite", 0.0f, NULL, NULL );
		g_EventQueue.AddEvent( pSprite, "Kill", 2.5f, NULL, NULL );
	}

	CBaseEntity *pSprite2 = CreateEntityByName( "env_sprite" );
	if( pSprite2 ) {
		pSprite2->KeyValue("origin", pOrigin);
		pSprite2->KeyValue("model", "sprites/xflare1.vmt");
		pSprite2->KeyValue("rendercolor", "184 250 214");
		pSprite2->KeyValue("renderfx", "14");
		pSprite2->KeyValue("rendermode", "3");
		pSprite2->KeyValue("spawnflags", "2");
		pSprite2->KeyValue("framerate", "10");
		pSprite2->KeyValue("GlowProxySize","32");
		pSprite2->KeyValue("frame","19");
		DispatchSpawn(pSprite2);
		g_EventQueue.AddEvent( pSprite2, "ShowSprite", 0.0f, NULL, NULL );
		g_EventQueue.AddEvent( pSprite2, "Kill", 2.5f, NULL, NULL );
	}
}