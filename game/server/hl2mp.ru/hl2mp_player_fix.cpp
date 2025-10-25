#include "cbase.h"
#include "hl2mp_player_fix.h"
#include "hl2mp_admins.h"
#include "vehicle_base.h"
#include "vehicle_crane.h"
#include "charge_token.h"
#include "in_buttons.h"
#include "info_player_spawn.h"
#include "func_tank.h"
#include "te_effect_dispatch.h"
#include "ai_behavior_follow.h"
//#include "c_soundscape.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(player, CHL2MP_Player_fix);

ConVar sv_healthbar("sv_healthbar","0");
ConVar sv_healthbar_sprite("sv_healthbar_sprite","sprites/health_arrow_format.vmt");
ConVar sv_healthbar_z("sv_healthbar_z","10");
ConVar sv_healthbar_frame("sv_healthbar_frame","101");

extern void RestoreClientState( CBasePlayer *pPlayer );

#define SF_PLAYEREQUIP_USEONLY 0x0001
#define SF_DISABLED 0x0002

void CHL2MP_Player_fix::Spawn(void)
{
	BaseClass::Spawn();

	m_flNextChargeTime = 0.0f;
	m_hHealerTarget = NULL;

	bool addDefault = true;
	CBaseEntity	*pWeaponEntity = NULL;

	while ( (pWeaponEntity = gEntList.FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != NULL)
	{
		if( pWeaponEntity->GetSpawnFlags() & SF_PLAYEREQUIP_USEONLY || pWeaponEntity->GetSpawnFlags() & SF_DISABLED )
			continue;
		
		pWeaponEntity->Touch( this );
		addDefault = false;
	}

	if( addDefault ) {
		m_Local.m_bWearingSuit = true;
		GiveDefaultItems();
	}

	RestoreClientState( this );

	InitSprinting();
	ResetAnimation();
	SetPlayerUnderwater( false );
}

extern const char *UTIL_TranslateMsg( char const *szLanguage, const char *msg);

void CHL2MP_Player_fix::DeathNotice ( CBaseEntity *pVictim ) 
{
	if( pVictim->IsNPC() && !FClassnameIs( pVictim, "hornet" ) ) {
		const char *msg = UTIL_TranslateMsg( engine->GetClientConVarValue( entindex(), "cl_language" ), "#HL2MP_FriendNpcDeath" );
		ClientPrint( this, HUD_PRINTTALK, UTIL_VarArgs( msg, nameReplace( pVictim ) ) );
	}
}

CBaseEntity* CHL2MP_Player_fix::GetAimTarget ( float range ) 
{
	Vector vecMuzzleDir;
	AngleVectors(EyeAngles(), &vecMuzzleDir);
	Vector vecStart, vecEnd;
	VectorMA( EyePosition(), range, vecMuzzleDir, vecEnd );
	VectorMA( EyePosition(), 5,   vecMuzzleDir, vecStart );
	trace_t tr;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	return tr.m_pEnt;
}

ConVar sv_healer("sv_healer", "1");
ConVar sv_healer_auto("sv_healer_auto", "0");
ConVar sv_healer_charge_interval("sv_healer_charge_interval", "1.0");
ConVar sv_test_bot("sv_test_bot", "0");
ConVar sv_npchp_x("sv_npchp_x","0.01");
ConVar sv_npchp_y("sv_npchp_y","0.56");
ConVar sv_npchp("sv_npchp","1");
ConVar sv_npchp_holdtime("sv_npchp_holdtime","0.2");
ConVar sv_player_pickup("sv_player_pickup", "0");

extern void openinventory( CBasePlayer *pPlayer );

CHL2MP_Player_fix::CHL2MP_Player_fix()
{
	m_flLastMovement = gpGlobals->curtime;
	//gSoundscapeSystem.LevelInitPreEntity();
	//gSoundscapeSystem.LevelInitPostEntity();
}

CHL2MP_Player_fix::~CHL2MP_Player_fix()
{
	//gSoundscapeSystem.Shutdown();
}

bool CHL2MP_Player_fix::CanSetSoundMixer( void )
{
	// Can't set sound mixers when we're in freezecam mode, since it has a code-enforced mixer
	return (GetObserverMode() != OBS_MODE_FREEZECAM);
}

CON_COMMAND(player_forceafk, "Force afk player")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int userid = atoi( args.Arg( 1 ) );
	CHL2MP_Player_fix *player = ( CHL2MP_Player_fix * )UTIL_PlayerByUserId( userid );
	if( !player )
		return;

	player->m_bInAfk = true;
	player->m_flLastMovement = 0.0;
	player->CommitSuicide();
}

void CHL2MP_Player_fix::PostThink(void)
{
	BaseClass::PostThink();
	
	//gSoundscapeSystem.m_hPlayer = this;

	if( IsAlive() && m_bInAfk )
	{
		m_flLastMovement = gpGlobals->curtime;
		m_bInAfk = false;
	}

	if ( m_afButtonLast != m_nButtons )
	{
		m_flLastMovement = gpGlobals->curtime;
	}

	if ( !m_bInAfk && m_flLastMovement + 300 < gpGlobals->curtime )
	{
		//Host_Say2( edict(), "I'm afk!", false );
		m_bInAfk = true;
		CommitSuicide();
	}

	if ( m_bInAfk && m_flLastMovement + 600 < gpGlobals->curtime && !IsAdmin( this, FL_ADMIN_NOAFK ) )
	{
		engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", GetUserID() ) );
		m_flLastMovement = gpGlobals->curtime;
	}

	CBaseEntity *m_hAimTarget = GetAimTarget();
	
	if( IsAlive() && m_fNextHealth <= gpGlobals->curtime && m_iHealth < 40  ) {
		 m_fNextHealth = gpGlobals->curtime + RandomFloat(1.0, 3.0);
		 TakeHealth(1,DMG_GENERIC);
	}

	/*CBaseEntity *ent = NULL;
	while ((ent = gEntList.NextEnt(ent)) != NULL)
	{
		if ( ent->GetPlayerMP() && ent->GetPlayerMP() == this && ent->IsNPC() ) {
			CAI_BaseNPC *pNpc = dynamic_cast<CAI_BaseNPC *>( ent );
			CAI_FollowBehavior *pBehavior;
			if ( !pNpc->GetBehavior( &pBehavior ) )
				continue;
			pBehavior->SetFollowTarget( this );
		}
	}*/

	if( m_hHealSprite == NULL ) {
		m_hHealSprite = CSprite::SpriteCreate( sv_healthbar_sprite.GetString(), EyePosition() + Vector(0,0,sv_healthbar_z.GetFloat()), false );
		
		if ( m_hHealSprite ) {
			m_hHealSprite->SetGlowProxySize( 32.0f );
			//m_hHealSprite->SetParent( this );
			m_hHealSprite->KeyValue("frame",sv_healthbar_frame.GetFloat());
			m_hHealSprite->KeyValue("targetname","hl2mp.ru-healthbar");
			DispatchSpawn( m_hHealSprite );
			m_hHealSprite->m_flFrame = m_hHealSprite->Frames();
			m_hHealSprite->SetOwnerEntity( this );
		}
	}

	if( m_hHealSprite && ( sv_healthbar.GetBool() || IsAdmin( this, FL_ADMIN_HEALTH_BAR ) ) ) {
		if( m_hAimTarget && ( m_hAimTarget->IsNPC() || m_hAimTarget->IsPlayer() ) && m_hAimTarget->IsAlive() )
			m_hHealBarTarget = m_hAimTarget;
		else if( m_hHealBarTarget ) {
			m_hHealBarTarget = NULL;
			m_hHealSprite->SetParent( NULL );
			m_hHealSprite->AddEffects( EF_NODRAW );
		}

		if( m_hHealBarTarget ) {
			float curhp = m_hHealBarTarget->GetHealth() / ( m_hHealBarTarget->GetMaxHealth() / 100.0 );
			if( m_hHealSprite->GetMoveParent() != m_hHealBarTarget ) {
				m_hHealSprite->SetParent( NULL );
				m_hHealSprite->SetParent( m_hHealBarTarget );
				m_hHealSprite->RemoveEffects( EF_NODRAW );
			}
			
			float flDist = ( GetAbsOrigin() - m_hHealBarTarget->GetAbsOrigin() ).LengthSqr();

			if( sqrt( flDist ) > 2000.0 ) {
				m_hHealSprite->SetRenderMode( kRenderNone );
			}
			else if( sqrt( flDist ) > 800.0 ) {
				m_hHealSprite->SetScale(0.05);
				m_hHealSprite->SetRenderMode( kRenderGlow );
			}
			else {
				m_hHealSprite->SetRenderMode( kRenderTransAdd );
				m_hHealSprite->SetScale(0.2);
			}

			float curframe = curhp * ( m_hHealSprite->Frames() / 100 );
			color32 m_color = m_hHealSprite->GetRenderColor();
		
			if( curhp < 40 ) {
				m_color.r = 255;
				m_color.g = 0;
				m_color.b = 0;
			}

			if( curhp >= 50 ) {
				CBaseEntity *m_hPTarget = m_hHealBarTarget;
				CAI_BaseNPC *pNpc = dynamic_cast<CAI_BaseNPC *>( m_hPTarget );
				if( pNpc && ( pNpc->IRelationType( this ) == D_HT || pNpc->IRelationType( this ) == D_FR ) ) {
					m_color.r = 255;
					m_color.g = 176;
					m_color.b = 0;
				}
				else if( pNpc && ( pNpc->IRelationType( this ) == D_LI ) ) {
					m_color.r = 0;
					m_color.g = 255;
					m_color.b = 0;
				}
				else {
					m_color.r = 0;
					m_color.g = 255;
					m_color.b = 0;
				}
			}

			m_hHealSprite->SetRenderColor( m_color.r, m_color.g, m_color.b );
			if( curframe < 0 )
				curframe = 0;

			m_hHealSprite->m_flFrame.Set( curframe );
		}
	}

	if( sv_test_bot.GetBool() ) {
		//if( m_hBot == NULL ) {
			edict_t *pEdict = engine->CreateFakeClient( "hl2mp.ru" );
			if (pEdict)
			{
				m_hBot = ((CHL2MP_Player_fix *)CBaseEntity::Instance( pEdict ));
				m_hBot->ClearFlags();
				m_hBot->AddFlag( FL_CLIENT  );
			}
	//	}
		if( m_hBot ) {
			m_hBot->Think();
		}
	}
	
	//АнтиЗастревание, после 10 секунд происходит телепорт игрока на респаун
	if( IsAlive() && m_StuckLast != 0 && !m_hVehicle ) {
		if( m_fStuck == 0.0 )
			m_fStuck = gpGlobals->curtime + 10.0;
		
		if( m_fStuck <= gpGlobals->curtime ) {
			CBaseEntity *pSpawn = EntSelectSpawnPoint();
			if( pSpawn ) {
				SetAbsOrigin( pSpawn->GetAbsOrigin() );
				SetAbsAngles( pSpawn->GetAbsAngles() );
				Teleport( &pSpawn->GetAbsOrigin(), &pSpawn->GetAbsAngles(), NULL );
				m_fStuck = 0.0;
			}
		}
		const char *msg = UTIL_TranslateMsg( engine->GetClientConVarValue( entindex(), "cl_language" ), "#HL2MP_AntiStuckMsg" );
		ClientPrint( this, HUD_PRINTCENTER, msg );	
	}
	else {
		m_fStuck = 0.0;
	}

	if( !GetViewEntity() ) {
		EnableControl(TRUE);
		if ( GetActiveWeapon() )
				GetActiveWeapon()->RemoveEffects( EF_NODRAW );
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		SetViewEntity( this );
	}

	if( m_nButtons & IN_RELOAD ) {
		if( !m_bWalkBT && gpGlobals->curtime >= m_fHoldR ) {
			openinventory( this );
			m_bWalkBT = true;
		}
	}
	else {
		m_bWalkBT = false;
		m_fHoldR = gpGlobals->curtime + 0.5;
	}

	if( m_bForceServerRagdoll == 1 )
		m_bForceServerRagdoll = 0;

	if( m_nButtons & IN_USE && m_hAimTarget && !m_hAimTarget->GetPlayerMP() && !m_hAimTarget->IsAlive() && IRelationType( m_hAimTarget ) != D_LI ) {
		if( m_fLoadFirmware <= gpGlobals->curtime && m_hAimTarget->IsNPC() ) {
			m_fLoadFirmware = gpGlobals->curtime + 0.1;
			if( m_iLoadFirmware >= 100 ) {
				m_iLoadFirmware = 100;
				if( FClassnameIs( m_hAimTarget, "npc_turret_ceiling" ) )
					m_hAimTarget->Spawn();
				m_hAimTarget->SetPlayerMP( this );
				m_hHealSprite->SetParent( NULL );
				m_hHealSprite->AddEffects( EF_NODRAW );
			}
			else {
				float curframe = m_iLoadFirmware++ * ( m_hHealSprite->Frames() / 100 );
				m_hHealSprite->m_flFrame.Set( curframe );
				m_hHealSprite->SetRenderColor( 153, 204, 255, 255 );
				m_hHealSprite->SetParent( m_hAimTarget );
				m_hHealSprite->SetRenderMode( kRenderTransAdd );
				m_hHealSprite->SetScale(0.2);
				m_hHealSprite->RemoveEffects( EF_NODRAW );
			}
		}
	}
	else if( m_iLoadFirmware > 0 ) {
		m_hHealSprite->SetParent( NULL );
		m_hHealSprite->AddEffects( EF_NODRAW );
		m_fLoadFirmware = 0.0;
		m_iLoadFirmware = 0;
	}

	if( m_nButtons & IN_USE && m_hAimTarget && (m_hAimTarget->IsPlayer() || m_hAimTarget->IsNPC()) ) {
		if( m_hHealerTarget != m_hAimTarget )
			m_hHealerTarget = m_hAimTarget;

		if( m_hHealerTarget && !m_hHealerTarget->GetPlayerMP() && m_hHealerTarget->IsNPC() ) {
			if( IRelationType( m_hHealerTarget ) != D_LI )
				m_hHealerTarget = NULL;
		}
	}
	else if( !sv_healer_auto.GetInt() ) {
		if( m_hHealerTarget )
			m_hHealerTarget = NULL;
	}

	if( ( sv_healer.GetInt() || IsAdmin( this, FL_ADMIN_TOKEN_HEALER ) ) && m_hHealerTarget ) {
		if( !IsAlive() || !m_hHealerTarget->IsAlive() || m_hHealerTarget->GetHealth() >= m_hHealerTarget->GetMaxHealth() || m_hHealerTarget->GetMaxHealth() == 0 || !FVisible(m_hHealerTarget, MASK_SHOT) )
			m_hHealerTarget = NULL;

		if( m_hHealerTarget && m_flNextChargeTime < gpGlobals->curtime ) {
			m_flNextChargeTime = gpGlobals->curtime + sv_healer_charge_interval.GetFloat();
			Vector vecSrc;
			if( !GetAttachment( LookupAttachment( "anim_attachment_LH" ), vecSrc ) )
				vecSrc = Weapon_ShootPosition();

			CChargeToken::CreateChargeToken(vecSrc, this, m_hHealerTarget);
			m_fNextHealth = gpGlobals->curtime + 10.0;
		}
	}

	if( IsAlive() && sv_npchp.GetBool() && m_hAimTarget ) {
		const char *szLanguage = engine->GetClientConVarValue( entindex(), "cl_language" );
		char msg[255];
		hudtextparms_s tTextParam;
		tTextParam.x = sv_npchp_x.GetFloat();
		tTextParam.y = sv_npchp_y.GetFloat();
		tTextParam.effect = 0;
		tTextParam.r1 = 255;
		tTextParam.g1 = 255;
		tTextParam.b1 = 255;
		tTextParam.a1 = 255;
		tTextParam.fadeinTime = 0;
		tTextParam.fadeoutTime = 0;
		tTextParam.holdTime = sv_npchp_holdtime.GetFloat();
		tTextParam.fxTime = 0;
		tTextParam.channel = 5;
		if( m_hAimTarget->IsNPC() ) {
			float result = m_hAimTarget->GetHealth() / (m_hAimTarget->GetMaxHealth() / 100.0);
			int health = ceil( result );
			if( health < 0 )
				return;

			const char *prefix = NULL;
			CAI_BaseNPC *pNpc = dynamic_cast<CAI_BaseNPC *>(m_hAimTarget);
			if( !pNpc )
				return;

			if( pNpc->IRelationType( this ) == D_LI ) {
				tTextParam.r1 = 255;
				tTextParam.g1 = 170;
				tTextParam.b1 = 0;
				tTextParam.a1 = 255;
				prefix = UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpPrefixFriend");
			}
			else if( pNpc->IRelationType( this ) == D_HT || pNpc->IRelationType( this ) == D_FR ) {
				prefix = UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpPrefixEnemy");
				//D@Ni1986 Отключаем показ хп врагов! Лишнее это
				if( !IsAdmin( this, FL_ADMIN_ENEMY_HEALTH ) )
					return;
			}
			else {
				prefix = UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpPrefixNone");
			}
			const char *enemy = UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpPrefixNone");
			if( m_hAimTarget->GetEnemy() ) {
				float nresult = m_hAimTarget->GetEnemy()->GetHealth() / (m_hAimTarget->GetEnemy()->GetMaxHealth() / 100.0);
				int nhealth = floor( nresult );
				if( nhealth < 0 )
					nhealth = 0;
				enemy = UTIL_VarArgs( "%s(%d%%)", nameReplace( m_hAimTarget->GetEnemy() ), nhealth );
			}
			if( m_hAimTarget->GetPlayerMP() ) {
				Q_snprintf( msg, sizeof(msg), UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpFriend"), prefix, nameReplace( m_hAimTarget ), health, m_hAimTarget->GetScore(),m_hAimTarget->GetPlayerMP()->GetPlayerName(), enemy );
			}
			else {
				Q_snprintf( msg, sizeof(msg), UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpDefault"), prefix, nameReplace( m_hAimTarget ), health );
			}
			
			UTIL_HudMessage(this, tTextParam, msg);
		}

		CPropVehicleDriveable *veh = dynamic_cast< CPropVehicleDriveable * >( m_hAimTarget );
		if( veh ) {
	   		if( veh->GetPlayerMP() && !m_hVehicle ) {
				Q_snprintf( msg, sizeof(msg), UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpOwner"), veh->GetPlayerMP()->GetPlayerName() );
				UTIL_HudMessage( this, tTextParam, msg );
			}
			else {
				CBasePlayer *driver = ToHL2MPPlayer( veh->GetDriver() );
				if( driver && driver->GetVehicleEntity() == veh && driver != this ) {
					Q_snprintf(msg, sizeof(msg), UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpDriver"), driver->GetPlayerName());
					UTIL_HudMessage(this, tTextParam, msg);
				}
			}
		}

		CPropCrane *veh2 = dynamic_cast< CPropCrane * >( m_hAimTarget );
		if( veh2 ) {
			CBasePlayer *driver = ToHL2MPPlayer( veh2->GetDriver() );
			if( driver && driver->GetVehicleEntity() == veh2 && driver != this ) {
				Q_snprintf(msg, sizeof(msg), UTIL_TranslateMsg( szLanguage, "#HL2MP_ShowHpDriver"), driver->GetPlayerName());
				UTIL_HudMessage(this, tTextParam, msg);
			}
		}
	}
}

void CHL2MP_Player_fix::PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize)
{
	if( !sv_player_pickup.GetBool() && !IsAdmin( this, FL_ADMIN_PICKUP ) || m_flBlockUse > gpGlobals->curtime )
		return;

	if ( GetGroundEntity() == pObject )
		return;
	
	if ( bLimitMassAndSize == true ) {
		if ( CBasePlayer::CanPickupObject( pObject, 35, 128 ) == false )
			 return;
	}

	if ( pObject->HasNPCsOnIt() )
		return; 
	
	//Блокируем спам USE
	m_flBlockUse = gpGlobals->curtime + 1.0;

	PlayerPickupObject( this, pObject );
}

ConVar sv_player_random_start("sv_player_random_start", "0");

EHANDLE g_pSpawnSpot;

CBaseEntity* CHL2MP_Player_fix::EntSelectSpawnPoint(void)
{
	
	if( g_pSpawnSpot )
		return g_pSpawnSpot;

	if( sv_player_random_start.GetInt() ) {
		int playeronline = 0;
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
			if( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && !pPlayer->IsInAVehicle() && pPlayer != this )
				playeronline++;
		}

		if( playeronline != 0 ) {
			int rndplayer = RandomInt( 1, playeronline );
			playeronline = 0;
			for( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
				if( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && !pPlayer->IsInAVehicle() && pPlayer != this ) {
					playeronline++;
					if( rndplayer == playeronline ) {
						if (pPlayer->GetFlags() & FL_DUCKING) {
							//m_nButtons |= IN_DUCK;
							AddFlag(FL_DUCKING);
							m_Local.m_bDucked = true;
							m_Local.m_bDucking = true;
							m_Local.m_flDucktime = 0.0f;
							SetViewOffset(VEC_DUCK_VIEW_SCALED(pPlayer));
							SetCollisionBounds(VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
						}
						return pPlayer;
					}
				}
			}
		}
	}
	
	const char *pSpawnpointName = "info_player_transation";

	CSpawnPoint *pEnt2 = (CSpawnPoint *)gEntList.FindEntityByClassname( NULL, pSpawnpointName );
	if( pEnt2 && pEnt2->m_iDisabled == FALSE ) {
		pEnt2->m_iDisabled = TRUE;
		return pEnt2;
	}
	else if( pEnt2 )
		pEnt2->Remove();

	pSpawnpointName = "info_player_deathmatch";

	if( HL2MPRules()->IsTeamplay() == true ) {
		if( GetTeamNumber() == TEAM_COMBINE ) {
			pSpawnpointName = "info_player_combine";
		}
		else if( GetTeamNumber() == TEAM_REBELS ) {
			pSpawnpointName = "info_player_rebel";
		}
	}

	if( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL ) {
		pSpawnpointName = "info_player_deathmatch";
	}

	if( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL ) {
		pSpawnpointName = "info_player_coop";
	}

	if( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL ) {
		pSpawnpointName = "info_player_start";
		#define SF_PLAYER_START_MASTER	1
		CBaseEntity *pEnt = NULL;
		while( ( pEnt = gEntList.FindEntityByClassname( pEnt, pSpawnpointName ) ) != NULL ) {
			if( pEnt->HasSpawnFlags( SF_PLAYER_START_MASTER ) )
				return pEnt;
		}
	}

	int spawn = 0;
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = gEntList.FindEntityByClassname( pEnt, pSpawnpointName ) ) != NULL ) {
		CSpawnPoint *pSpawn = (CSpawnPoint *)pEnt;
		if( pSpawn && pSpawn->m_iDisabled == FALSE ) {
			spawn++;
		}
	}

	int rndspawn = random->RandomInt( 1, spawn );
	spawn = 0;
	pEnt = NULL;
	while( ( pEnt = gEntList.FindEntityByClassname( pEnt, pSpawnpointName ) ) != NULL ) {
		CSpawnPoint *pSpawn = (CSpawnPoint *)pEnt;
		if( pSpawn && pSpawn->m_iDisabled == FALSE ) {
			spawn++;
			if( spawn == rndspawn ) {
				return pEnt;
			}
		}
	}

	return CBaseEntity::Instance(INDEXENT(0));
}

ConVar sv_only_one_team("sv_only_one_team", "3");

void CHL2MP_Player_fix::ChangeTeam(int iTeam)
{
	if( g_fGameOver == false && ( sv_only_one_team.GetInt() != 0 ) && !IsFakeClient() && ( iTeam != 1 ) )
		iTeam = sv_only_one_team.GetInt();

	BaseClass::ChangeTeam(iTeam);
}

void CHL2MP_Player_fix::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	CTakeDamageInfo inputInfo = info;
	if( IsAdmin( inputInfo.GetAttacker(), FL_ADMIN_FF ) )
		inputInfo.SetForceFriendlyFire( true );

	BaseClass::TraceAttack( inputInfo, vecDir, ptr, pAccumulator );
}

ConVar mp_plr_nodmg_plr("mp_plr_nodmg_plr", "0");

int CHL2MP_Player_fix::OnTakeDamage( const CTakeDamageInfo &info )
{
	CTakeDamageInfo inputInfo = info; 
	
	if( inputInfo.GetAttacker() ) {
		if( IsAdmin( inputInfo.GetAttacker(), FL_ADMIN_FF ) )
			inputInfo.SetForceFriendlyFire( true );

		if( mp_plr_nodmg_plr.GetInt() && IsPlayer() && inputInfo.GetAttacker()->IsPlayer() && ( inputInfo.GetAttacker() != this ) && !IsAdmin( inputInfo.GetAttacker(), FL_ADMIN_FF ) )
			return 0;

		if( !inputInfo.GetAttacker()->IsPlayer() && IRelationType( inputInfo.GetAttacker() ) == D_LI )
			return 0;
	}
	
	return BaseClass::OnTakeDamage( inputInfo );
}

void CHL2MP_Player_fix::UpdateOnRemove( void )
{
	//Удаляем вещи игрока
	CBaseEntity *ent = NULL;
	while ((ent = gEntList.NextEnt(ent)) != NULL)
	{
		if ( ent->GetPlayerMP() && ent->GetPlayerMP() == this )
			UTIL_Remove(ent);
	}
	BaseClass::UpdateOnRemove();
}