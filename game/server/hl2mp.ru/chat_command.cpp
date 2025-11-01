#include "cbase.h"
#include "filesystem.h"
#include "hl2mp_admins.h"
//#include "multiplay_gamerules.h"
#include "hl2mp_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern const char *UTIL_TranslateMsg( char const *szLanguage, const char *msg);

struct PlayerInfo
{
	CHandle<CBasePlayer> m_hPlayer;
	uint64 SteamID64;
	float flNextSave;
	float flNextMTP;
	int tpCount;
	Vector SaveOrigin;
};

CUtlVector<PlayerInfo> m_PlayerInfo;

Vector vGlobalPoint;

int GetPlayerInfo( CBasePlayer *pPlayer ) {
	for ( int i = 0; i < m_PlayerInfo.Count(); i++ ) {
		if( m_PlayerInfo[i].SteamID64 == pPlayer->GetSteamIDAsUInt64() ) {
			m_PlayerInfo[i].m_hPlayer = pPlayer;
			return i;
		}
	}

	PlayerInfo player;
	player.m_hPlayer = pPlayer;
	player.SteamID64 = pPlayer->GetSteamIDAsUInt64();
	player.tpCount = 5;
	player.SaveOrigin = vec3_origin;
	player.flNextMTP = 0.0;
	player.flNextSave = 0.0;
	return m_PlayerInfo.AddToTail( player );
}

ConVar sm_savetp( "sm_savetp", "1" );

CON_COMMAND(sm_savetp_addtp, "Add tp to Player")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *modify = args.Arg(1)+1;
	int userid = atoi(modify);
	CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
	if( !pPlayer )
		return;

	int addTp = atoi( args.Arg( 2 ) );
	int iIndex = GetPlayerInfo( pPlayer );
	m_PlayerInfo[iIndex].tpCount += addTp;

	const char *szLanguage = engine->GetClientConVarValue( pPlayer->entindex(), "cl_language" );
	const char *nmsg = UTIL_VarArgs( UTIL_TranslateMsg( szLanguage, "#HL2MP_SaveTpAddTp"), addTp, m_PlayerInfo[iIndex].tpCount );
	UTIL_SayText( nmsg, pPlayer );
}

void SaveTpClearPoint()
{
	for ( int i = 0; i < m_PlayerInfo.Count(); i++ ) {
		m_PlayerInfo[i].SaveOrigin = vec3_origin;
	}

	UTIL_SayTextAll("#HL2MP_SaveTpClean");
}

CON_COMMAND(sm_savetp_clearpoint, "Cleaning savepoints")
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	SaveTpClearPoint();
}

extern bool TeleportAllNpc( CBasePlayer *pPlayer );

//RTV
ConVar mp_enable_rtv( "mp_enable_rtv", "1" );
CUtlVector<EHANDLE> g_vecRTVVoters;
int m_iRequiredVotes;
int m_fWaitVotes;

bool ExtraChatCommand( CBasePlayer *pPlayer, char *msg ) {
	int iIndex = GetPlayerInfo( pPlayer );

	const char *szLanguage = engine->GetClientConVarValue( pPlayer->entindex(), "cl_language" );

	if( !Q_stricmp( msg, "currentmap" ) ) 
	{
		ClientPrint( pPlayer, HUD_PRINTTALK, STRING( gpGlobals->mapname ) );
		return true;
	}

	if( !Q_stricmp( msg, "timeleft" ) ) 
	{
		int iTimeRemaining = (int)HL2MPRules()->GetMapRemainingTime();
    
		if ( iTimeRemaining == 0 )
		{
			ClientPrint( pPlayer, HUD_PRINTTALK, "This game has no timelimit." );
		}
		else
		{
			int iMinutes, iSeconds;
			iMinutes = iTimeRemaining / 60;
			iSeconds = iTimeRemaining % 60;

			char minutes[8];
			char seconds[8];

			Q_snprintf( minutes, sizeof(minutes), "%d", iMinutes );
			Q_snprintf( seconds, sizeof(seconds), "%2.2d", iSeconds );

			ClientPrint( pPlayer, HUD_PRINTTALK, "Time left in map: %s1:%s2", minutes, seconds );
		}

		return true;
	}

	if( !Q_stricmp( msg, "nextmap" ) ) 
	{
		char szNextMap[MAX_MAP_NAME];

		if ( nextlevel.GetString() && *nextlevel.GetString() )
		{
			Q_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
		}
		else
		{
			MultiplayRules()->GetNextLevelName( szNextMap, sizeof(szNextMap) );
		}

		ClientPrint(pPlayer, HUD_PRINTTALK, szNextMap);

		return true;
	}

	if( !Q_stricmp( msg, "rtv" ) && mp_enable_rtv.GetBool() ) 
	{
		if( m_fWaitVotes >= gpGlobals->curtime )
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "RTV: Try again later!");
			return true;
		}
		else if( g_vecRTVVoters.Find( pPlayer ) != -1 )
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "RTV: You have already voted!");
			return true;
		}

		g_vecRTVVoters.AddToTail( pPlayer );

		char szMessage[256];
		Q_snprintf(szMessage, sizeof(szMessage), "RTV: %d/%d votes needed to change map!", g_vecRTVVoters.Count(), m_iRequiredVotes);
		UTIL_ClientPrintAll(HUD_PRINTTALK, szMessage);

		return true;
	}

	if( !pPlayer->IsAlive() )
		return false;

	if( !Q_stricmp( msg, "mtp" ) || !Q_stricmp( msg, "ьез" ) ) {
		if( m_PlayerInfo[iIndex].flNextMTP > gpGlobals->curtime )
		{
			int sec = floor( m_PlayerInfo[iIndex].flNextMTP - gpGlobals->curtime );
			const char *nmsg = UTIL_VarArgs( UTIL_TranslateMsg( szLanguage, "#HL2MP_MtpWait"), sec );
			UTIL_SayText( nmsg, pPlayer );
		}
		else
		{
			engine->ServerCommand( UTIL_VarArgs( "sv_merchant_tp_npc %d\n", pPlayer->GetUserID() ) );
			m_PlayerInfo[iIndex].flNextMTP = gpGlobals->curtime + 60.0;
		}
		return true;
		
	}

	if( ( !Q_stricmp( msg, "save" ) || !Q_stricmp( msg, "ыфму" ) || !Q_stricmp( msg, "tp" ) || !Q_stricmp( msg, "ез" ) ) && !sm_savetp.GetBool() ) {
		UTIL_SayText( "#HL2MP_SaveTpDisabled", pPlayer );
		return true;
	}

	if( !Q_stricmp( msg, "save" ) || !Q_stricmp( msg, "ыфму" ) ) {
		if( m_PlayerInfo[iIndex].flNextSave > gpGlobals->curtime ) {
			int sec = floor( m_PlayerInfo[iIndex].flNextSave - gpGlobals->curtime );
			const char *nmsg = UTIL_VarArgs( UTIL_TranslateMsg( szLanguage, "#HL2MP_SaveTpWait"), sec );
			UTIL_SayText( nmsg, pPlayer );
		}
		else if( m_PlayerInfo[iIndex].tpCount <= 0 ) {
			UTIL_SayText( "#HL2MP_SaveTpNotHave", pPlayer );
		}
		else if( pPlayer->m_Local.m_bDucked || ( pPlayer->GetGroundEntity() && !FClassnameIs( pPlayer->GetGroundEntity(), "worldspawn" ) ) ) {
			UTIL_SayText( "#HL2MP_SaveTpCantSave", pPlayer );
		}
		else {
			UTIL_SayText( "#HL2MP_SaveTpPointCreate", pPlayer );
			m_PlayerInfo[iIndex].SaveOrigin = pPlayer->GetAbsOrigin();
			m_PlayerInfo[iIndex].flNextSave = gpGlobals->curtime + 60.0;
		}
		return true;
	}

	if( !Q_stricmp( msg, "tp" ) || !Q_stricmp( msg, "ез" ) ) {
		if( m_PlayerInfo[iIndex].tpCount <= 0 )
			UTIL_SayText( "#HL2MP_SaveTpOverTp", pPlayer );
		else if( m_PlayerInfo[iIndex].SaveOrigin == vec3_origin ) {
			UTIL_SayText( "#HL2MP_SaveTpNotFound", pPlayer );
		}
		else {
			m_PlayerInfo[iIndex].tpCount -= 1;
			const char *nmsg = UTIL_VarArgs( UTIL_TranslateMsg( szLanguage, "#HL2MP_SaveTpTrue"), m_PlayerInfo[iIndex].tpCount );
			UTIL_SayText( nmsg, pPlayer );
			pPlayer->Teleport( &m_PlayerInfo[iIndex].SaveOrigin, NULL, NULL );
		}
		return true;
	}

	if( ( !Q_stricmp( msg, "saveg" ) || !Q_stricmp( msg, "ыфмуп" ) ) && IsAdmin( pPlayer, FL_ADMIN ) ) {
		UTIL_SayTextAll( "#HL2MP_SaveTpGlobalMade" );
		vGlobalPoint = pPlayer->GetAbsOrigin();
		return true;
	}

	if( !Q_stricmp( msg, "tpg" ) || !Q_stricmp( msg, "езп" ) ) {
		if( vGlobalPoint == vec3_origin )
			UTIL_SayText( "#HL2MP_SaveTpGlobalFalse", pPlayer );
		else {
			UTIL_SayText( "#HL2MP_SaveTpGlobalTrue", pPlayer );
			pPlayer->Teleport( &vGlobalPoint, NULL, NULL );
		}
		return true;
	}

	if( !Q_stricmp( msg, "setview" ) && IsAdmin( pPlayer, FL_ADMIN ) ) {
		Vector vecMuzzleDir;
		AngleVectors(pPlayer->EyeAngles(), &vecMuzzleDir);
		Vector vecStart, vecEnd;
		VectorMA( pPlayer->EyePosition(), 3000, vecMuzzleDir, vecEnd );
		VectorMA( pPlayer->EyePosition(), 5,   vecMuzzleDir, vecStart );
		trace_t tr;
		UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if( tr.m_pEnt ) {
			pPlayer->SetViewEntity( tr.m_pEnt );
		}
		return true;
	}

	if( !Q_stricmp( msg, "resetview" ) && IsAdmin( pPlayer, FL_ADMIN ) ) {
		pPlayer->SetViewEntity( pPlayer );
		return true;
	}

	return false;
}

class C_HandlerHelper : public CAutoGameSystemPerFrame
{
public:
	virtual char const *Name() { return "C_HandlerHelper"; }
	C_HandlerHelper( char const *name ) : CAutoGameSystemPerFrame( name ) {}

	C_HandlerHelper() {}

	~C_HandlerHelper() {}

	virtual void LevelInitPreEntity( void )
	{
		vGlobalPoint = vec3_origin;

		if( gpGlobals->eLoadType == MapLoad_Transition )
			SaveTpClearPoint();
		else
			m_PlayerInfo.RemoveAll();

		m_fWaitVotes = gpGlobals->curtime + 30;
	}

	virtual void FrameUpdatePostEntityThink( void ) 
	{
		//Проверяем валидных игроков
		FOR_EACH_VEC( g_vecRTVVoters, i )
		{
			if( !g_vecRTVVoters[i] )
			{
				g_vecRTVVoters.FastRemove( i );
				continue;
			}
		}

		int iTotalPlayers = 0;
        
		for (int i = 1; i <= gpGlobals->maxClients; i++) {
			CBasePlayer* pClient = UTIL_PlayerByIndex(i);
			if (pClient && !pClient->IsFakeClient()) {
				iTotalPlayers++;
			}
		}

		m_iRequiredVotes = (int)ceil(iTotalPlayers * 0.5f); // 50% от общего числа игроков

		// Проверяем, достигнут ли необходимый порог
		if( g_vecRTVVoters.Count() >= m_iRequiredVotes && m_iRequiredVotes > 0 ) {
			UTIL_ClientPrintAll(HUD_PRINTTALK, "RTV: Голосование успешно! Меняем карту...");
            
			// Выполняем команду смены карты
			engine->ServerCommand("changelevel_next\n");
            
			// Очищаем список голосовавших после успешного RTV
			g_vecRTVVoters.RemoveAll();
		}

		for ( int i = 0; i < m_PlayerInfo.Count(); i++ ) {
			if( m_PlayerInfo[i].flNextMTP != 0.0 && m_PlayerInfo[i].flNextMTP <= gpGlobals->curtime ) {
				m_PlayerInfo[i].flNextMTP = 0.0;
				if( m_PlayerInfo[i].m_hPlayer )
					UTIL_SayText( "#HL2MP_MtpAllow", m_PlayerInfo[i].m_hPlayer );
			}

			if( m_PlayerInfo[i].flNextSave != 0.0 && m_PlayerInfo[i].flNextSave <= gpGlobals->curtime ) {
				m_PlayerInfo[i].flNextSave = 0.0;
				if( m_PlayerInfo[i].m_hPlayer )
					UTIL_SayText( "#HL2MP_SaveTpAllow", m_PlayerInfo[i].m_hPlayer );
			}
		}
	}
};

C_HandlerHelper gHandlerHelper;