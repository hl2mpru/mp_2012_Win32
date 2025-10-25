#include "cbase.h"
#include "filesystem.h"
#include "Sprite.h"
#include "npc_playercompanion.h"
#include "viewport_panel_names.h"
#include "te_effect_dispatch.h"
#include "hl2mp_admins.h"
#include "eventqueue.h"
#include "items.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNPC_merchant : public CNPC_PlayerCompanion
{
public:
	DECLARE_CLASS( CNPC_merchant, CNPC_PlayerCompanion );

	void Spawn( void );
	void SelectModel();
	void Precache( void );
	bool IgnorePlayerPushing( void ) { return true; };
	Class_T Classify ( void );
	bool ShouldLookForBetterWeapon() { return false; } 
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	CHandle<CSprite> m_hSprite;
};

LINK_ENTITY_TO_CLASS( npc_merchant, CNPC_merchant );

Class_T	CNPC_merchant::Classify ( void )
{
	if( GetActiveWeapon() )
		return CLASS_PLAYER_ALLY;

	return	CLASS_NONE;
}

ConVar sk_merchant_health("sk_merchant_health","100");

//=========================================================
// Spawn
//=========================================================
void CNPC_merchant::Spawn()
{
	BaseClass::Spawn();
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetUse( &CNPC_merchant::Use );
	
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_DOORS_GROUP | bits_CAP_TURN_HEAD | bits_CAP_DUCK | bits_CAP_SQUAD );
	CapabilitiesAdd( bits_CAP_USE_WEAPONS );
	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_FRIENDLY_DMG_IMMUNE );
	CapabilitiesAdd( bits_CAP_AIM_GUN );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_USE_SHOT_REGULATOR );

	m_iHealth = sk_merchant_health.GetInt();

	NPCInit();
	Vector origin = EyePosition();
	origin.z += 12.0;
	m_hSprite = CSprite::SpriteCreate( "hl2mp.ru/merchant.vmt", origin, false );
	if ( m_hSprite ) {
		m_hSprite->SetRenderMode( kRenderGlow );
		m_hSprite->SetScale( 0.2f );
		m_hSprite->SetGlowProxySize( 64.0f );
		m_hSprite->SetParent( this );
		m_hSprite->TurnOn();
	}

	int iAttachment = LookupBone( "ValveBiped.Bip01_R_Hand" );
	if( GetActiveWeapon() && iAttachment > 0 ) {
		SetCollisionGroup( COLLISION_GROUP_PLAYER );
	}
	else {
		if( GetActiveWeapon() )
			Weapon_Drop( GetActiveWeapon() );

		AddEFlags( EFL_NO_DISSOLVE );
		m_takedamage = DAMAGE_NO;
		AddSpawnFlags( SF_NPC_WAIT_FOR_SCRIPT );
		CapabilitiesClear();
	}
	UTIL_SayTextAll("#HL2MP_MerchantOnMap");
}

//#define TP_EFFECT_1 "weapon_muzzle_flash_assaultrifle"
//#define TP_EFFECT_2 "strider_headbeating_01b"

void EffectSpawn( CBaseEntity *pTarget )
{
	Vector pOrigin = pTarget->GetAbsOrigin();
	pOrigin.z +=32;
	
	//DispatchParticleEffect( TP_EFFECT_2, entity->GetAbsOrigin()+Vector(0.0, 0.0, 10.0), vec3_angle );
	//DispatchParticleEffect( TP_EFFECT_1, entity->GetAbsOrigin()+Vector(0.0, 0.0, 32.0), vec3_angle );
	
	UTIL_EmitAmbientSound( ENTINDEX( pTarget ), pOrigin, "hl2mp.ru/rndspawn.wav", 1.0, SNDLVL_60dB, 0, 100 );
	
	CBaseEntity *pBeam = CreateEntityByName( "env_beam" );
	if( pBeam ) {
		pBeam->KeyValue("origin", pOrigin);
		pBeam->KeyValue("BoltWidth", "1.8");
		pBeam->KeyValue("life", ".5");
		pBeam->KeyValue("LightningStart", reinterpret_cast<uintptr_t>(pTarget));
		pBeam->KeyValue("NoiseAmplitude", "10.4");
		pBeam->KeyValue("Radius", "200");
		pBeam->KeyValue("renderamt", "150");
		pBeam->KeyValue("rendercolor", "0 255 0");
		pBeam->KeyValue("spawnflags", "34");
		pBeam->KeyValue("StrikeTime", "-.5");
		pBeam->KeyValue("targetname", reinterpret_cast<uintptr_t>(pTarget));
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

void CNPC_merchant::Precache()
{
	BaseClass::Precache();
	PrecacheModel( STRING( GetModelName() ) );
	//PrecacheParticleSystem( TP_EFFECT_1 );
	//PrecacheParticleSystem( TP_EFFECT_2 );
}

void CNPC_merchant::SelectModel()
{
	const char *szModel = STRING( GetModelName() );
	if (!szModel || !*szModel)
		SetModelName( AllocPooledString("models/zombie/classic.mdl") );
}

ConVar sv_merchant_url("sv_merchant_url","http://hl2mp.ru/motd/buy/27400/");
ConVar sv_merchant_url_hide("sv_merchant_url_hide","0");
ConVar sv_merchant_url_unload("sv_merchant_url_unload","1");

bool firstload[64];
bool inShop[64];
bool inInventory[64];

class testTimer : public CBasePlayer
{
	DECLARE_CLASS( testTimer, CBaseEntity );
	DECLARE_DATADESC();
public:
	void ContextThink( void );
};

BEGIN_DATADESC( testTimer )
	DEFINE_THINKFUNC( ContextThink ),
END_DATADESC()

void testTimer::ContextThink()
{
	if( inShop[entindex()] || inInventory[entindex()] ) {
		AddFlag( FL_FROZEN );
		AddFlag( FL_NOTARGET );
		AddFlag( FL_GODMODE );
		SetNextThink(gpGlobals->curtime + .2, "MerchantContext" );
	}
	else {
		RemoveFlag( FL_FROZEN );
		RemoveFlag( FL_NOTARGET );
		RemoveFlag( FL_GODMODE );
	}
}

extern const char *UTIL_TranslateMsg( const char *szLanguage, const char *msg );

void openshop( CBasePlayer *player )
{
	int pIndex = engine->IndexOfEdict( player->edict() );
	const char *szLanguage = engine->GetClientConVarValue( pIndex, "cl_language" );
	firstload[pIndex] = true;
	inInventory[pIndex] = false;
	inShop[pIndex] = true;
	char msg[254];
	Q_snprintf( msg, sizeof(msg), "%s/index.php?key=%ld&userid=%d&language=%s", sv_merchant_url.GetString(), reinterpret_cast<uintptr_t>(player), player->GetUserID(), szLanguage );
	KeyValues *data = new KeyValues("data");
	const char* title = UTIL_TranslateMsg( engine->GetClientConVarValue( pIndex, "cl_language" ), "#HL2MP_MerchantTitle" );
	data->SetString( "title", title );		// info panel title
	data->SetString( "type", "2" );			// show userdata from stringtable entry
	data->SetString( "msg",	msg );		// use this stringtable entry
	data->SetString("cmd", "5" );
	data->SetBool( "unload", sv_merchant_url_unload.GetBool() );
	player->ShowViewPortPanel( PANEL_INFO, !sv_merchant_url_hide.GetBool(), data );
	data->deleteThis();
}

void CC_closed_htmlpage( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if (!pPlayer)
		return;

	int pIndex = engine->IndexOfEdict( pPlayer->edict() );
	inShop[pIndex] = false;
	inInventory[pIndex] = false;
	firstload[pIndex] = false;
}

static ConCommand closed_htmlpage( "closed_htmlpage", CC_closed_htmlpage, "", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE );

void CNPC_merchant::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if( pPlayer )
		openshop( pPlayer );
}

void CC_openshop( const CCommand& args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( pPlayer && IsAdmin( pPlayer, FL_ADMIN_MERCHANT ) )
		openshop( pPlayer );
}

static ConCommand merchant("merchant", CC_openshop, "Force open merchant menu!", FCVAR_GAMEDLL );

void CC_sv_merchant_give( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *modify = args.Arg(1)+1;
	int userid = atoi(modify);
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player )
		return;
	
	const char *classname = args.Arg(2);
	if ( Q_stristr(classname, "weapon_" ) ) {
		CBaseEntity *item = player->GiveNamedItem( classname );
		if( !item )
			Msg( "{\"Merchant\":false}\n" );
		else {
			item->SetPlayerMP( player );
			item->AddSpawnFlags( SF_NPC_NO_WEAPON_DROP );
			
			const char *section = args.Arg(4);
			item->SetSectionName( section );
			
			for (int i=0; i<MAX_WEAPONS; ++i) 
			{
				CBaseCombatWeapon *pWeapon = player->GetWeapon(i);
				if (!pWeapon)
					continue;

				bool pEmpty = !pWeapon->HasAmmo();
				if( !pEmpty && FClassnameIs(pWeapon, classname) ) {
					player->Weapon_Switch(pWeapon);
					break;
				}
			}
		}
	}
	else {
		bool result = false;
		int count = atoi(args.Arg(3));
		do {
			CItem *pItem = dynamic_cast<CItem *>( CreateEntityByName(classname) );
			if( pItem ) {
				pItem->AddSpawnFlags( SF_NORESPAWN );
				if( !pItem->MyTouch( player ) ) {
					pItem->Remove();
					break;
				}
				result = true;
			}
		}
		while(count--);
		if( !result )
			Msg( "{\"Merchant\":false}\n" );
	}
}

static ConCommand sv_merchant_give( "sv_merchant_give", CC_sv_merchant_give, "", FCVAR_GAMEDLL );

void CC_sv_merchant_client_info( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if( strcmpi(args.Arg(1),"@all" ) == 0 ) {
		for( int i=1; i<=gpGlobals->maxClients; i++ )
		{
			CBasePlayer *player = UTIL_PlayerByIndex( i );
			if( player && !player->IsFakeClient() ) {
				CSteamID steamID;
				player->GetSteamID( &steamID );
				const char *szLanguage = engine->GetClientConVarValue( engine->IndexOfEdict( player->edict() ), "cl_language" );
				Msg( "{\"pUserId\": %d, \"pSteamID64\": \"%llu\", \"pLanguage\": \"%s\"}\n", player->GetUserID(), steamID.ConvertToUint64(), szLanguage );
			}
		}
		return;
	}

	int userid = atoi( args.Arg( 1 ) );
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player )
		return;

	int pIndex = engine->IndexOfEdict( player->edict() );

	if( strcmpi(args.Arg(2),"leaveshop" ) == 0 ) {
		inShop[pIndex] = false;
		inInventory[pIndex] = false;
		return;
	}
	
	if( strcmpi(args.Arg(2),"inmotd" ) == 0 ) {
		Msg( "{\"pInShop\":%s, \"pInInventory\":%s, \"pFirstLoad\":%s}\n", inShop[pIndex] ? "true" : "false", inInventory[pIndex] ? "true" : "false", firstload[pIndex] ? "true" : "false" );
		return;
	}

	if( strcmpi(args.Arg(2),"firstload" ) == 0 ) {
		Msg( "{\"Merchant\":%s}\n", firstload[pIndex] ? "true" : "false" );
		if( firstload[pIndex] && inShop[pIndex] ) {
			player->RegisterThinkContext( "MerchantContext" );
			player->SetContextThink( &testTimer::ContextThink, gpGlobals->curtime, "MerchantContext" );
		}
		firstload[pIndex] = false;
		return;
	}

	CSteamID steamID;
	player->GetSteamID( &steamID );
	Msg( "{\"pFrag\": %d, \"pAdmin\": %d, \"pKey\": %ld, \"pTeam\": %d, \"pSteamID64\": \"%llu\"}\n", player->FragCount(), IsAdmin( player, FL_ADMIN ), reinterpret_cast<uintptr_t>(player), player->GetTeamNumber(), steamID.ConvertToUint64() );
}

static ConCommand sv_merchant_client_info( "sv_merchant_client_info", CC_sv_merchant_client_info, "", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE );

void CC_sv_merchant_decrement_frag( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int frag = atoi(args.Arg(2));
	int userid = atoi(args.Arg(1));
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if(player)
		player->IncrementFragCount(-frag);
}

static ConCommand sv_merchant_decrement_frag( "sv_merchant_decrement_frag", CC_sv_merchant_decrement_frag, "", FCVAR_GAMEDLL );

void CC_sv_merchant_entity_kill( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	bool result = false;
	int entindex = atoi( args.Arg( 1 ) );
	if( entindex != 0 ) {
		CBaseEntity *pEntity = UTIL_EntityByIndex( entindex );
		if( pEntity ) {
			pEntity->SetPlayerMP( NULL );
			pEntity->SetSectionName( NULL );

			if( Q_stristr(pEntity->GetClassname(), "weapon_" ) ) {
				CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( pEntity );
				if( pWeapon ) {
					pWeapon->AddSpawnFlags( SF_NPC_NO_WEAPON_DROP );
					CBaseCombatCharacter *pOwner = pWeapon->GetOwner();
					if( pOwner ) {
						pOwner->Weapon_Drop( pWeapon );
						result = true;
					}
				}
			}

			CBaseCombatCharacter *pNpc = dynamic_cast<CBaseCombatCharacter*>( pEntity );
			if( pNpc ) {
				CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( pNpc->GetActiveWeapon() );
				if( pWeapon )
					pWeapon->AddSpawnFlags( SF_NPC_NO_WEAPON_DROP );
			}

			if( !result ) {
				pEntity->TakeDamage( CTakeDamageInfo( pEntity, pEntity, pEntity->GetHealth()+100, DMG_GENERIC ) );

				CBaseAnimating *pDisolvingAnimating = dynamic_cast<CBaseAnimating*>( pEntity );
				if ( pDisolvingAnimating )
					pDisolvingAnimating->Dissolve( "", gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
				else
					g_EventQueue.AddEvent( pEntity, "Kill", 0.0f, NULL, NULL );

				result = true;
			}
		}
	}
	if( result )
		Msg( "{\"Merchant\":true}\n" );
	else
		Msg( "{\"Merchant\":false}\n" );
}

static ConCommand sv_merchant_entity_kill( "sv_merchant_entity_kill", CC_sv_merchant_entity_kill, "", FCVAR_GAMEDLL );

void CC_sv_merchant_give_npc( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *modify = args.Arg(1)+1;
	int userid = atoi(modify);
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player ) {
		Msg( "{\"Rcpt\":false}\n" );
		return;
	}

	const char *classname = args.Arg(2);
	int limit = atoi(args.Arg(3));
	const char *weapon = args.Arg(4);
	int health = atoi(args.Arg(5));
	const char *section = args.Arg(6);

	if( !health )
		health = 1000;

	if( classname ) {
		CBaseEntity *ent = NULL;
		int maxnpc = 0;
		while ( (ent = gEntList.NextEnt(ent)) != NULL )
		{
			if( ent->GetSectionName() != NULL && FStrEq( section, ent->GetSectionName() ) )
			{
				if (ent->GetPlayerMP() == player)
					maxnpc++;

				if(maxnpc >= limit && limit != 0) {
					Msg( "{\"Merchant\":false}\n" );
					return;
				}
			}
		}

		CBaseEntity *entity = CreateEntityByName( classname );
		if ( entity != NULL )
		{
			entity->SetAbsOrigin( player->GetAbsOrigin() + Vector(0.0, 0.0, 10.0) );
			entity->SetCollisionGroup( COLLISION_GROUP_PLAYER );
			entity->SetPlayerMP( player );
			entity->SetSectionName( section );
			entity->KeyValue( "squadname", "merchant" );
			if( weapon )
				entity->KeyValue( "additionalequipment", weapon );

			DispatchSpawn( entity );
			entity->Activate();
			EffectSpawn( entity );

			if( entity->IsNPC() ) {
				INPCInteractive *pInteractive = dynamic_cast<INPCInteractive *>( entity );
				if( pInteractive )
					pInteractive->NotifyInteraction( NULL );

				entity->SetHealth( health );
				entity->SetMaxHealth( health );
				entity->m_takedamage = DAMAGE_YES;
				entity->AddEFlags( EFL_NO_DISSOLVE );
				entity->AddSpawnFlags( SF_NPC_FADE_CORPSE );
			}
			Msg( "{\"Merchant\":true}\n" );
		}
		else
			Msg( "{\"Merchant\":false}\n" );
	}
}

static ConCommand sv_merchant_give_npc( "sv_merchant_give_npc", CC_sv_merchant_give_npc, "", FCVAR_GAMEDLL );

bool TeleportAllNpc( CBasePlayer *pPlayer ) {
	bool ready = false;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if (ent->GetPlayerMP() && (ent->GetPlayerMP() == pPlayer) && !FClassnameIs( ent, "npc_turret_ceiling" ) && !Q_stristr(ent->GetClassname(), "prop_vehicle" ) && !Q_stristr(ent->GetClassname(), "weapon_" ) ) {
			EffectSpawn( ent );
			Vector newOrigin = pPlayer->GetAbsOrigin() + Vector(0.0, 0.0, 8.0);
			ent->Teleport(&newOrigin,&pPlayer->GetAbsAngles(),&vec3_origin);
			ready = true;
		}
	}
	return ready;
}

CON_COMMAND(sv_merchant_setowner, "Setting the owner to an item")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
	
	int userid = atoi( args.Arg( 1 ) );
	CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
	if( !pPlayer ) {
		Msg( "{\"Rcpt\":false}\n" );
		return;
	}

	bool ready = false;
	CBaseEntity *pEntity = UTIL_EntityByIndex( atoi( args.Arg( 2 ) ) );
	if( pEntity ) {
		pEntity->SetPlayerMP( pPlayer );
		ready = true;
	}

	if( ready )
		Msg( "{\"Merchant\":true}\n" );
	else
		Msg( "{\"Merchant\":false}\n" );
}

CON_COMMAND(sv_merchant_tp_npc, "")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
	
	int userid = atoi( args.Arg( 1 ) );
	CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
	if( !pPlayer )
		return;

	bool ready = false;
	int entindex = atoi( args.Arg( 2 ) );
	if( entindex > 0 ) {
		CBaseEntity *pEntity = UTIL_EntityByIndex( entindex );
		if( pEntity && !FClassnameIs( pEntity, "npc_turret_ceiling" ) && pEntity != pPlayer->GetVehicleEntity() ) {
			EffectSpawn( pEntity );
			Vector newOrigin = pPlayer->GetAbsOrigin() + Vector( 0.0, 0.0, 8.0 );
			pEntity->Teleport( &newOrigin, &pPlayer->GetAbsAngles(), &vec3_origin );
			ready = true;
		}
	}
	else {
		ready = TeleportAllNpc( pPlayer );
	}

	if( !ready ) {
		const char *msg = UTIL_TranslateMsg( engine->GetClientConVarValue( ENTINDEX( pPlayer ), "cl_language" ), "#HL2MP_MerchantTeleportFalse" );
		UTIL_SayText(msg,pPlayer);
		Msg( "{\"Merchant\":false}\n" );
		return;
	}
	EffectSpawn( pPlayer );
	const char *msg = UTIL_TranslateMsg( engine->GetClientConVarValue( ENTINDEX( pPlayer ), "cl_language" ), "#HL2MP_MerchantTeleportTrue" );
	UTIL_SayText(msg,pPlayer);
	Msg( "{\"Merchant\":true}\n" );
}

void openinventory( CBasePlayer *player )
{
	int pIndex = engine->IndexOfEdict( player->edict() );
	firstload[pIndex] = true;
	inInventory[pIndex] = true;
	inShop[pIndex] = false;
	const char *szLanguage = engine->GetClientConVarValue( engine->IndexOfEdict( player->edict() ), "cl_language" );
	char msg[254];
	Q_snprintf( msg, sizeof(msg), "%s/index.php?inventory=true&key=%ld&userid=%d&language=%s", sv_merchant_url.GetString(), reinterpret_cast<uintptr_t>( player ), player->GetUserID(), szLanguage );
	KeyValues *data = new KeyValues("data");
	const char* title = UTIL_TranslateMsg( engine->GetClientConVarValue( pIndex, "cl_language" ), "#HL2MP_MerchantInventoryTitle" );
	data->SetString( "title", title );		// info panel title
	data->SetString( "type", "2" );			// show userdata from stringtable entry
	data->SetString( "msg",	msg );		// use this stringtable entry
	data->SetString("cmd", "5" );
	data->SetBool( "unload", sv_merchant_url_unload.GetBool() );
	player->ShowViewPortPanel( PANEL_INFO, !sv_merchant_url_hide.GetBool(), data );
	data->deleteThis();
}

void CC_sv_merchant_inventory( const CCommand &args )
{
	if( UTIL_GetCommandClient() )
		openinventory( UTIL_GetCommandClient() );

	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int userid = atoi( args.Arg( 1 ) );
	CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
	if( !pPlayer )
		return;
	
	for (int i=0; i<MAX_WEAPONS; ++i) 
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if (!pWeapon)
			continue;

		Msg( "{ \"pSection\": \"%s\", \"pClassName\": \"%s\", \"pEntIndex\": %d, \"pUserID\": %d, \"pEmpty\": \"%s\" }\n", pWeapon->GetSectionName(), pWeapon->GetClassname(), ENTINDEX( pWeapon ), pWeapon->GetPlayerMP() ? pWeapon->GetPlayerMP()->GetUserID() : 0, !pWeapon->HasAmmo() ? "true" : "false" );
	}

	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if( Q_stristr( ent->GetClassname(), "weapon_" ) )
			continue;

		if (ent->GetPlayerMP() && (ent->GetPlayerMP() == pPlayer) ) {
			float result = ent->GetHealth() / (ent->GetMaxHealth() / 100.0);
			int health = ceil( result );
			Msg( "{ \"pSection\": \"%s\", \"pHealth\": %d, \"pScore\": %d, \"pEntIndex\": %d, \"pUserID\": %d, \"pClassName\": \"%s\" }\n", ent->GetSectionName(), health, ent->GetScore(), ENTINDEX( ent ), ent->GetPlayerMP()->GetUserID(), ent->GetClassname() );
		}
	}
}

static ConCommand sv_merchant_inventory( "sv_merchant_inventory", CC_sv_merchant_inventory, "", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE );

void CC_sv_merchant_equip( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int userid = atoi(args.Arg(1));
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player || !player->IsAlive() )
		return;

	const char *classname = args.Arg(2);
	
	int bSpawn = atoi( args.Arg(3) );
	bool result = false;
	if( bSpawn > 0 ) {
		CBaseEntity *pWeapon = player->GiveNamedItem(classname);
		if( pWeapon )
			pWeapon->AddSpawnFlags( SF_NPC_NO_WEAPON_DROP );
	}
	for ( int i=0; i<MAX_WEAPONS; ++i ) 
	{
		CBaseCombatWeapon *pWeapon = player->GetWeapon(i);
		if (!pWeapon)
			continue;

		bool pEmpty = !pWeapon->HasAmmo();
		if( !pEmpty && FClassnameIs(pWeapon, classname) ) {
			player->Weapon_Switch(pWeapon);
			result = true;
			break;
		}
	}
	if( result )
		Msg( "{\"Merchant\":true}\n" );
	else
		Msg( "{\"Merchant\":false}\n" );
}

static ConCommand sv_merchant_equip( "sv_merchant_equip", CC_sv_merchant_equip, "", FCVAR_GAMEDLL );

#define VectorToString(v) (static_cast<const char *>(CFmtStr("%f %f %f", (v).x, (v).y, (v).z)))

struct MerchantListEntry
{
	const char *szName;
	float flNextSpawn;
};

extern void OnResourcePrecached( const char *relativePathFileName );

class CNPC_MerchantSpawner : public CAutoGameSystemPerFrame
{
public:
	virtual char const *Name() { return "CNPC_MerchantSpawner"; }
	CNPC_MerchantSpawner( char const *name ) : CAutoGameSystemPerFrame( name ) {}

	CNPC_MerchantSpawner() {}

	~CNPC_MerchantSpawner() {}

	virtual void LevelShutdownPostEntity()
	{
		if(!manifest)
			return;

		manifest->deleteThis();
		m_AlreadySpawn.RemoveAll();
	}

	virtual void LevelInitPreEntity( void )
	{
		manifest = new KeyValues("EntityList");
		manifest->LoadFromFile( filesystem, "configs/merchant.txt", "MOD" );
		OnResourcePrecached("materials/hl2mp.ru/merchant_buy.vmt");
		OnResourcePrecached("materials/hl2mp.ru/merchant_buy.vtf");
	}

	virtual void FrameUpdatePostEntityThink( void ) 
	{
		KeyValues *entry = manifest->FindKey( STRING( gpGlobals->mapname ) );
		if( entry != NULL ) {
			for( KeyValues *pKV = entry->GetFirstSubKey(); pKV != NULL; pKV = pKV->GetNextKey() )
			{
				if( gEntList.FindEntityByName( NULL, pKV->GetName() ) )
					continue;

				bool result = true;

				for ( int i = 0; i < m_AlreadySpawn.Count(); i++ )
				{
					if( Q_strstr( pKV->GetName(), m_AlreadySpawn[i].szName ) ) {
						if( m_AlreadySpawn[i].flNextSpawn <= 0.0 )
							m_AlreadySpawn[i].flNextSpawn = gpGlobals->curtime + 120.0;

						if( m_AlreadySpawn[i].flNextSpawn > gpGlobals->curtime )
							result = false;
						else
							m_AlreadySpawn.FastRemove( i );
						
						break;
					}
				}

				if( !result )
					continue;

				Vector pOrigin;
				UTIL_StringToVector( pOrigin.Base(), pKV->GetString("origin") );
				
				CBasePlayer *pPlayer = UTIL_GetNearestPlayer( pOrigin );
				if( pPlayer == NULL )
					continue;

				float flDist = ( pOrigin - pPlayer->GetAbsOrigin() ).LengthSqr();
				
				if( sqrt( flDist ) >= 512.0 )
					continue;

				CAI_BaseNPC *pNPC = dynamic_cast< CAI_BaseNPC * >( CreateEntityByName( "npc_merchant" ) );
				if( pNPC == NULL )
					continue;

				pNPC->KeyValue( "origin", pKV->GetString("origin") );
				pNPC->KeyValue( "angles", pKV->GetString("angle") );
				pNPC->KeyValue( "model", pKV->GetString("model") );

				if( Q_strstr( pKV->GetString("model"), "zombie" ) )
						pNPC->KeyValue( "body", "1" );
				else
					pNPC->KeyValue( "additionalequipment", "weapon_ar2" );

				pNPC->KeyValue( "targetname", pKV->GetName() );
				pNPC->KeyValue( "npcname", "merchant" );
				pNPC->KeyValue( "spawnflags", "8708" );
				DispatchSpawn( pNPC );
				MerchantListEntry pMerchant;
				pMerchant.szName = pKV->GetName();
				pMerchant.flNextSpawn = 0.0;
				m_AlreadySpawn.AddToTail( pMerchant );
			}
		}
	}

	void Save() {
		char fullPath[_MAX_PATH];
		if( filesystem->GetLocalPath( "configs/merchant.txt", fullPath, sizeof(fullPath) ) )
			manifest->SaveToFile( filesystem, fullPath, "GAME" );
	}

	void AddMerchant( Vector vOrigin, QAngle qAngle, const char *szModel ) {
		KeyValues *pMerchant = manifest->FindKey( STRING( gpGlobals->mapname ), true );

		KeyValues *pSub = new KeyValues( UTIL_VarArgs( "%d", RandomInt( 1, 999999 ) ) );
		pMerchant->AddSubKey( pSub );

		pSub->SetString( "origin", VectorToString( vOrigin ) );
		pSub->SetString( "angle", VectorToString( qAngle ) );
		pSub->SetString( "model", szModel );
		
		Save();
	}

	bool DelMerchant( const char *szName ) {
		KeyValues *pMerchant = manifest->FindKey( STRING( gpGlobals->mapname ) );
		if( pMerchant == NULL )
			return false;

		KeyValues *pSub = pMerchant->FindKey( szName );
		if( pSub == NULL )
			return false;
			
		pMerchant->RemoveSubKey( pSub );
		Save();
		return true;
	}

	KeyValues *manifest;

	CUtlVector< MerchantListEntry > m_AlreadySpawn;
};

CNPC_MerchantSpawner gMerchant;

CON_COMMAND(sv_merchant_add, "Add merchant AIM")
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !pPlayer || !IsAdmin( pPlayer, FL_ADMIN ) )
		return;

	Vector vecMuzzleDir;
	AngleVectors( pPlayer->EyeAngles(), &vecMuzzleDir );
	Vector vecStart, vecEnd;
	VectorMA( pPlayer->EyePosition(), 8000, vecMuzzleDir, vecEnd );
	VectorMA( pPlayer->EyePosition(), 5,   vecMuzzleDir, vecStart );
	trace_t tr;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	QAngle pAngle = pPlayer->GetAbsAngles();
	pAngle[1] -= 180;

	gMerchant.AddMerchant( tr.endpos, pAngle, args.Arg(1) );
}

CON_COMMAND(sv_merchant_remove, "Del AIM merchant")
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !pPlayer || !IsAdmin( pPlayer, FL_ADMIN ) )
		return;

	Vector vecMuzzleDir;
	AngleVectors( pPlayer->EyeAngles(), &vecMuzzleDir );
	Vector vecStart, vecEnd;
	VectorMA( pPlayer->EyePosition(), 8000, vecMuzzleDir, vecEnd );
	VectorMA( pPlayer->EyePosition(), 5,   vecMuzzleDir, vecStart );
	trace_t tr;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if( tr.m_pEnt ) {
		string_t mName = tr.m_pEnt->GetEntityName();
		if( gMerchant.DelMerchant( STRING( mName) ) )
			UTIL_Remove( tr.m_pEnt );
	}
}