#include "cbase.h"

class gct : public CBaseEntity
{
	DECLARE_CLASS( gct, CBaseEntity );
	DECLARE_DATADESC();

public:	
	void Spawn( void );
	void InputStart( inputdata_t &inputData );
	void InputStop( inputdata_t &inputData );
	void InputSetTimerLabel( inputdata_t &inputData );
	void TimerThink( void );

private:
	bool m_stop;
	float m_fTimer;
	const char *m_iszTimerText;
	COutputEvent m_OnTimer;
};

LINK_ENTITY_TO_CLASS( game_countdown_timer, gct );
LINK_ENTITY_TO_CLASS( hud_timer, gct );

BEGIN_DATADESC( gct )

	DEFINE_INPUTFUNC( FIELD_FLOAT, "StartTimer", InputStart ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "Start", InputStart ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopTimer", InputStop ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Stop", InputStop ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetTimerLabel", InputSetTimerLabel ),
	DEFINE_OUTPUT( m_OnTimer, "OnTimer"),
	DEFINE_THINKFUNC( TimerThink ),

	DEFINE_KEYFIELD( m_iszTimerText, FIELD_STRING, "TimerText" ),

END_DATADESC()

void gct::Spawn( void ) 
{
	m_stop = FALSE;
	m_fTimer = 0.0;
	if( m_iszTimerText == NULL )
		m_iszTimerText = "Game countdown";
}

void gct::InputStart( inputdata_t &inputData )
{
	//Stop other timer
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, GetClassname() ) ) != NULL )
	{
		gct *pTimer = dynamic_cast<gct *>( pEntity );
		if( pTimer && ( pTimer != this ) )
			pTimer->m_stop = true;
	}

	m_stop = FALSE;
	m_fTimer = inputData.value.Float();
	SetThink( &gct::TimerThink );
	SetNextThink( gpGlobals->curtime + 1.0 );
}

void gct::InputSetTimerLabel( inputdata_t &inputData )
{
	m_iszTimerText = inputData.value.String();
}

void gct::InputStop( inputdata_t &inputData )
{
	m_stop = TRUE;
}

extern const char *UTIL_TranslateMsg( const char *szLanguage, const char *msg );

void gct::TimerThink( void )
{
	if( m_stop )
		return;

	if(--m_fTimer < 0.0) {
		m_OnTimer.FireOutput(this, this);
		return;
	}

	hudtextparms_s tTextParam;
	tTextParam.x			= -1.0;
	tTextParam.y			= 0.0;
	tTextParam.effect		= 0;
	tTextParam.r1			= 255;
	tTextParam.g1			= 170;
	tTextParam.b1			= 0;
	tTextParam.a1			= 255;
	tTextParam.r2			= 255;
	tTextParam.g2			= 170;
	tTextParam.b2			= 0;
	tTextParam.a2			= 255;
	tTextParam.fadeinTime	= 0.0;
	tTextParam.fadeoutTime	= 0.0;
	tTextParam.holdTime		= 1.5;
	tTextParam.fxTime		= 0.0;
	tTextParam.channel		= 0;

	char msg[100];
	int min, sec;
	min = floor(m_fTimer/60);
	sec = m_fTimer - min*60;

	for( int i=1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( i );
		if( player && !player->IsFakeClient() ) {
			const char *szLanguage = engine->GetClientConVarValue( ENTINDEX( player ), "cl_language" );
			const char *TimerText = UTIL_TranslateMsg( szLanguage, m_iszTimerText );
			if(sec < 10)
				Q_snprintf( msg, sizeof(msg), "%s\n%d:0%d", TimerText, min,sec );
			else
				Q_snprintf( msg, sizeof(msg), "%s\n%d:%d", TimerText, min,sec );

			UTIL_HudMessage( player, tTextParam, msg );
		}
	}

	SetNextThink( gpGlobals->curtime + 1.0 );
}