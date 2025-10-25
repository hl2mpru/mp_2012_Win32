//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Soundscapes.txt resource file processor
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include <KeyValues.h>
#include "engine/IEngineSound.h"
#include "filesystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundchars.h"
//#include "view.h"
#include "engine/ivdebugoverlay.h"
#include "tier0/icommandline.h"
#include "hl2mp_player_fix.h"
#include "c_soundscape.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Only allow recursive references to be 8 levels deep.
// This test will flag any circular references and bail.
#define MAX_SOUNDSCAPE_RECURSION	8

const float DEFAULT_SOUND_RADIUS = 36.0f;

ConVar soundscape_fadetime( "soundscape_fadetime", "3.0", FCVAR_CHEAT, "Time to crossfade sound effects between soundscapes" );

// singleton system
ConVar *C_SoundscapeSystem::m_pDSPVolumeVar = NULL;
ConVar *C_SoundscapeSystem::m_pSoundMixerVar = NULL;

#define SOUNDSCAPE_MANIFEST_FILE				"scripts/soundscapes_manifest.txt"

void C_SoundscapeSystem::AddSoundScapeFile( const char *filename )
{
	KeyValues *script = new KeyValues( filename );
#ifndef _XBOX
	if ( script->LoadFromFile( filesystem, filename ) )
#else
	if ( filesystem->LoadKeyValues( *script, IFileSystem::TYPE_SOUNDSCAPE, filename, "GAME" ) )
#endif
	{
		// parse out all of the top level sections and save their names
		KeyValues *pKeys = script;
		while ( pKeys )
		{
			// save pointers to all sections in the root
			// each one is a soundscape
			if ( pKeys->GetFirstSubKey() )
			{
				m_soundscapes.AddToTail( pKeys );
			}
			pKeys = pKeys->GetNextKey();
		}
		// Keep pointer around so we can delete it at exit
		m_SoundscapeScripts.AddToTail( script );
	}
	else
	{
		script->deleteThis();
	}
}

// parse the script file, setup index table
bool C_SoundscapeSystem::Init()
{
	m_loopingSoundId = 0;

	const char *mapname = STRING( gpGlobals->mapname);
	const char *mapSoundscapeFilename = NULL;
	if ( mapname && *mapname )
	{
		mapSoundscapeFilename = UTIL_VarArgs( "scripts/soundscapes_%s.txt", mapname );
	}

	KeyValues *manifest = new KeyValues( SOUNDSCAPE_MANIFEST_FILE );
	if ( filesystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDSCAPE, SOUNDSCAPE_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				//Msg("Add %s\n", sub->GetString());
				AddSoundScapeFile( sub->GetString() );
				if ( mapSoundscapeFilename && FStrEq( sub->GetString(), mapSoundscapeFilename ) )
				{
					mapSoundscapeFilename = NULL; // we've already loaded the map's soundscape
				}
				continue;
			}

			Msg( "C_SoundscapeSystem::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SOUNDSCAPE_MANIFEST_FILE, sub->GetName() );
		}

		if ( mapSoundscapeFilename && filesystem->FileExists( mapSoundscapeFilename ) )
		{
			AddSoundScapeFile( mapSoundscapeFilename );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SOUNDSCAPE_MANIFEST_FILE );
	}

	manifest->deleteThis();

	return true;
}


int C_SoundscapeSystem::FindSoundscapeByName( const char *pSoundscapeName )
{
	// UNDONE: Bad perf, linear search!
	for ( int i = m_soundscapes.Count()-1; i >= 0; --i )
	{
		if ( !Q_stricmp( m_soundscapes[i]->GetName(), pSoundscapeName ) )
			return i;
	}

	return -1;
}

KeyValues *C_SoundscapeSystem::SoundscapeByIndex( int index )
{
	if ( m_soundscapes.IsValidIndex(index) )
		return m_soundscapes[index];
	return NULL;
}

const char *C_SoundscapeSystem::SoundscapeNameByIndex( int index )
{
	if ( index < m_soundscapes.Count() )
	{
		return m_soundscapes[index]->GetName();
	}

	return NULL;
}

void C_SoundscapeSystem::Shutdown()
{
	m_hPlayer = NULL;

	for ( int i = m_loopingSounds.Count() - 1; i >= 0; --i )
	{
		loopingsound_t &sound = m_loopingSounds[i];

		// sound is done, remove from list.
		StopLoopingSound( sound );
	}
	
	// These are only necessary so we can use shutdown/init calls
	// to flush soundscape data
	m_loopingSounds.RemoveAll();
	m_randomSounds.RemoveAll();
	m_soundscapes.RemoveAll();
	m_params.ent.Set( NULL );
	m_params.soundscapeIndex = -1;

	while ( m_SoundscapeScripts.Count() > 0 )
	{
		KeyValues *kv = m_SoundscapeScripts[ 0 ];
		m_SoundscapeScripts.Remove( 0 );
		kv->deleteThis();
	}
}

void C_SoundscapeSystem::ForceSoundscape( const char *pSoundscapeName, float radius )
{
	int index = FindSoundscapeByName( pSoundscapeName );
	if ( index >= 0 )
	{
		m_forcedSoundscapeIndex = index;
		m_forcedSoundscapeRadius = radius;
		StartNewSoundscape( SoundscapeByIndex(index) );
	}
	else
	{
		DevWarning("Can't find soundscape %s\n", pSoundscapeName );
	}
}

void C_SoundscapeSystem::DevReportSoundscapeName( int index )
{
	const char *pName = "none";
	if ( index >= 0 && index < m_soundscapes.Count() )
	{
		pName = m_soundscapes[index]->GetName();
	}
	if( m_hPlayer ) {
		const char *msg = UTIL_VarArgs( "Soundscape: %s %s", pName, m_hPlayer->GetPlayerName() );
		UTIL_SayTextAll( msg );
	}
}


// This makes all currently playing loops fade toward their target volume
void C_SoundscapeSystem::UpdateLoopingSounds( float frametime )
{
	float period = soundscape_fadetime.GetFloat();
	float amount = frametime;
	if ( period > 0 )
	{
		amount *= 1.0 / period;
	}

	int fadeCount = m_loopingSounds.Count();
	while ( fadeCount > 0 )
	{
		fadeCount--;
		loopingsound_t &sound = m_loopingSounds[fadeCount];

		if ( sound.volumeCurrent != sound.volumeTarget )
		{
			sound.volumeCurrent = Approach( sound.volumeTarget, sound.volumeCurrent, amount );
			if ( sound.volumeTarget == 0 && sound.volumeCurrent == 0 )
			{
				// sound is done, remove from list.
				StopLoopingSound( sound );
				m_loopingSounds.FastRemove( fadeCount );
			}
			else
			{
				// tell the engine about the new volume
				UpdateLoopingSound( sound );
			}
		}
	}
}

void C_SoundscapeSystem::FrameUpdatePostEntityThink( void ) 
{
	if( m_hPlayer )
		UpdateAudioParams( m_hPlayer->GetAudioParams() );

	if ( m_forcedSoundscapeIndex >= 0 )
	{
		// generate fake positional sources
		CBasePlayer *pPlayer = m_hPlayer;
		if ( pPlayer )
		{
			Vector origin, forward, right;
			pPlayer->EyePositionAndVectors( &origin, &forward, &right, NULL );
			
			// put the sound origins at the corners of a box around the player
			m_params.localSound.Set( 0, origin + m_forcedSoundscapeRadius * (forward-right) );
			m_params.localSound.Set( 1, origin + m_forcedSoundscapeRadius * (forward+right) );
			m_params.localSound.Set( 2, origin + m_forcedSoundscapeRadius * (-forward-right) );
			m_params.localSound.Set( 3, origin + m_forcedSoundscapeRadius * (-forward+right) );
			m_params.localBits = 0x0007;
		}
	}
	// fade out the old sounds over soundscape_fadetime seconds
	UpdateLoopingSounds( gpGlobals->frametime );
	UpdateRandomSounds( gpGlobals->curtime );
}


void C_SoundscapeSystem::UpdateAudioParams( audioparams_t &audio )
{
	if ( m_params.soundscapeIndex == audio.soundscapeIndex && m_params.ent.Get() == audio.ent.Get() )
		return;

	m_params = audio;
	m_forcedSoundscapeIndex = -1;

	if ( audio.ent.Get() && audio.soundscapeIndex >= 0 && audio.soundscapeIndex < m_soundscapes.Count() )
	{
		DevReportSoundscapeName( audio.soundscapeIndex );
		StartNewSoundscape( m_soundscapes[audio.soundscapeIndex] );
	}
	else
	{
		// bad index (and the soundscape file actually existed...)
		if ( audio.ent.Get() != 0 &&
			 audio.soundscapeIndex != -1 )
		{
			Msg("Error: Bad soundscape!\n");
		}
	}
}

// Called when a soundscape is activated (leading edge of becoming the active soundscape)
void C_SoundscapeSystem::StartNewSoundscape( KeyValues *pSoundscape )
{
	int i;

	// Reset the system
	// fade out the current loops
	for ( i = m_loopingSounds.Count()-1; i >= 0; --i )
	{
		m_loopingSounds[i].volumeTarget = 0;
		if ( !pSoundscape )
		{
			// if we're cancelling the soundscape, stop the sound immediately
			m_loopingSounds[i].volumeCurrent = 0;
		}
	}
	// update ID
	m_loopingSoundId++;

	// clear all random sounds
	m_randomSounds.RemoveAll();
	m_nextRandomTime = gpGlobals->curtime;

	if ( pSoundscape )
	{
		subsoundscapeparams_t params;
		params.allowDSP = true;
		params.wroteSoundMixer = false;
		params.wroteDSPVolume = false;

		params.masterVolume = 1.0;
		params.startingPosition = 0;
		params.recurseLevel = 0;
		params.positionOverride = -1;
		params.ambientPositionOverride = -1;
		StartSubSoundscape( pSoundscape, params );

		if ( !params.wroteDSPVolume )
		{
			m_pDSPVolumeVar->Revert();
		}
		if ( !params.wroteSoundMixer )
		{
			m_pSoundMixerVar->Revert();
		}
	}
}

void C_SoundscapeSystem::StartSubSoundscape( KeyValues *pSoundscape, subsoundscapeparams_t &params )
{
	// Parse/process all of the commands
	KeyValues *pKey = pSoundscape->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "dsp" ) )
		{
			if ( params.allowDSP )
			{
				ProcessDSP( pKey );
				//ProcessDSPPlayer( pKey );
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "dsp_player" ) )
		{
			if ( params.allowDSP )
			{
				ProcessDSPPlayer( pKey );
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "playlooping" ) )
		{
			ProcessPlayLooping( pKey, params );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "playrandom" ) )
		{
			ProcessPlayRandom( pKey, params );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "playsoundscape" ) )
		{
			ProcessPlaySoundscape( pKey, params );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "Soundmixer" ) )
		{
			if ( params.allowDSP )
			{
				ProcessSoundMixer( pKey, params );
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "dsp_volume" ) )
		{
			if ( params.allowDSP )
			{
				ProcessDSPVolume( pKey, params );
			}
		}
		// add new commands here
		else
		{
			DevMsg( 1, "Soundscape %s:Unknown command %s\n", pSoundscape->GetName(), pKey->GetName() );
		}
		pKey = pKey->GetNextKey();
	}
}

// add a process for each new command here

// change DSP effect
void C_SoundscapeSystem::ProcessDSP( KeyValues *pDSP )
{
	int roomType = pDSP->GetInt();
	CPASAttenuationFilter filter( m_hPlayer );
	enginesound->SetRoomType( filter, roomType );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDSPPlayer - 
//-----------------------------------------------------------------------------
void C_SoundscapeSystem::ProcessDSPPlayer( KeyValues *pDSPPlayer )
{
	int dspType = pDSPPlayer->GetInt();
	CPASAttenuationFilter filter( m_hPlayer );
	enginesound->SetPlayerDSP( filter, dspType, false );
}


void C_SoundscapeSystem::ProcessSoundMixer( KeyValues *pSoundMixer, subsoundscapeparams_t &params )
{
	CHL2MP_Player_fix *pPlayer = m_hPlayer;
	if ( !pPlayer || pPlayer->CanSetSoundMixer() )
	{
		m_pSoundMixerVar->SetValue( pSoundMixer->GetString() );
		params.wroteSoundMixer = true;
	}
}

void C_SoundscapeSystem::ProcessDSPVolume( KeyValues *pKey, subsoundscapeparams_t &params )
{
	m_pDSPVolumeVar->SetValue( pKey->GetFloat() );
	params.wroteDSPVolume = true;
}

// start a new looping sound
void C_SoundscapeSystem::ProcessPlayLooping( KeyValues *pAmbient, const subsoundscapeparams_t &params )
{
	float volume = 0;
	soundlevel_t soundlevel = ATTN_TO_SNDLVL(ATTN_NORM);
	const char *pSoundName = NULL;
	int pitch = PITCH_NORM;
	int positionIndex = -1;
	bool suppress = false;
	KeyValues *pKey = pAmbient->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "volume" ) )
		{
			volume = params.masterVolume * RandomInterval( ReadInterval( pKey->GetString() ) );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "pitch" ) )
		{
			pitch = RandomInterval( ReadInterval( pKey->GetString() ) );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "wave" ) )
		{
			pSoundName = pKey->GetString();
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "position" ) )
		{
			positionIndex = params.startingPosition + pKey->GetInt();
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "attenuation" ) )
		{
			soundlevel = ATTN_TO_SNDLVL( RandomInterval( ReadInterval( pKey->GetString() ) ) );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "soundlevel" ) )
		{
			if ( !Q_strncasecmp( pKey->GetString(), "SNDLVL_", strlen( "SNDLVL_" ) ) )
			{
				soundlevel = TextToSoundLevel( pKey->GetString() );
			}
			else
			{
				soundlevel = (soundlevel_t)((int)RandomInterval( ReadInterval( pKey->GetString() ) ));
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "suppress_on_restore" ) )
		{
			suppress = Q_atoi( pKey->GetString() ) != 0 ? true : false;
		}
		else
		{
			Msg( "Ambient %s:Unknown command %s\n", pAmbient->GetName(), pKey->GetName() );
		}
		pKey = pKey->GetNextKey();
	}

	if ( positionIndex < 0 )
	{
		positionIndex = params.ambientPositionOverride;
	}
	else if ( params.positionOverride >= 0 )
	{
		positionIndex = params.positionOverride;
	}

	// Sound is mared as "suppress_on_restore" so don't restart it
	if ( IsBeingRestored() && suppress )
	{
		return;
	}

	if ( volume != 0 && pSoundName != NULL )
	{
		if ( positionIndex < 0 )
		{
			AddLoopingAmbient( pSoundName, volume, pitch );
		}
		else
		{
			if ( positionIndex > 31 || !(m_params.localBits & (1<<positionIndex) ) )
			{
				// suppress sounds if the position isn't available
				//DevMsg( 1, "Bad position %d\n", positionIndex );
				return;
			}
			AddLoopingSound( pSoundName, false, volume, soundlevel, pitch, m_params.localSound[positionIndex] );
		}
	}
}

void C_SoundscapeSystem::TouchSoundFile( char const *wavefile )
{
	filesystem->GetFileTime( UTIL_VarArgs( "sound/%s", PSkipSoundChars( wavefile ) ), "GAME" );
}

// start a new looping sound
void C_SoundscapeSystem::TouchPlayLooping( KeyValues *pAmbient )
{
	KeyValues *pKey = pAmbient->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "wave" ) )
		{
			char const *pSoundName = pKey->GetString();

			// Touch the file
			TouchSoundFile( pSoundName );
		}

		pKey = pKey->GetNextKey();
	}
}

Vector C_SoundscapeSystem::GenerateRandomSoundPosition()
{
	float angle = random->RandomFloat( -180, 180 );
	float sinAngle, cosAngle;
	SinCos( angle, &sinAngle, &cosAngle );
	CHL2MP_Player_fix *pPlayer = m_hPlayer;
	if ( pPlayer )
	{
		Vector origin, forward, right;
		pPlayer->EyePositionAndVectors( &origin, &forward, &right, NULL );
		return origin + DEFAULT_SOUND_RADIUS * (cosAngle * right + sinAngle * forward);
	}
	else
	{
		//return CurrentViewOrigin() + DEFAULT_SOUND_RADIUS * (cosAngle * CurrentViewRight() + sinAngle * CurrentViewForward());
		return vec3_origin;
	}
}

void C_SoundscapeSystem::TouchSoundFiles()
{
	if ( !CommandLine()->FindParm( "-makereslists" ) )
		return;

	int c = m_soundscapes.Count();
	for ( int i = 0; i < c ; ++i )
	{
		TouchWaveFiles( m_soundscapes[ i ] );
	}
}

void C_SoundscapeSystem::TouchWaveFiles( KeyValues *pSoundScape )
{
	KeyValues *pKey = pSoundScape->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "playlooping" ) )
		{
			TouchPlayLooping( pKey );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "playrandom" ) )
		{
			TouchPlayRandom( pKey );
		}

		pKey = pKey->GetNextKey();
	}

}

// puts a recurring random sound event into the queue
void C_SoundscapeSystem::TouchPlayRandom( KeyValues *pPlayRandom )
{
	KeyValues *pKey = pPlayRandom->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "rndwave" ) )
		{
			KeyValues *pWaves = pKey->GetFirstSubKey();
			while ( pWaves )
			{
				TouchSoundFile( pWaves->GetString() );

				pWaves = pWaves->GetNextKey();
			}
		}

		pKey = pKey->GetNextKey();
	}
}

// puts a recurring random sound event into the queue
void C_SoundscapeSystem::ProcessPlayRandom( KeyValues *pPlayRandom, const subsoundscapeparams_t &params )
{
	randomsound_t sound;
	sound.Init();
	sound.masterVolume = params.masterVolume;
	int positionIndex = -1;
	bool suppress = false;
	bool randomPosition = false;
	KeyValues *pKey = pPlayRandom->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "volume" ) )
		{
			sound.volume = ReadInterval( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "pitch" ) )
		{
			sound.pitch = ReadInterval( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "attenuation" ) )
		{
			interval_t atten = ReadInterval( pKey->GetString() );
			sound.soundlevel.start = ATTN_TO_SNDLVL( atten.start );
			sound.soundlevel.range = ATTN_TO_SNDLVL( atten.start + atten.range ) - sound.soundlevel.start;
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "soundlevel" ) )
		{
			if ( !Q_strncasecmp( pKey->GetString(), "SNDLVL_", strlen( "SNDLVL_" ) ) )
			{
				sound.soundlevel.start = TextToSoundLevel( pKey->GetString() );
				sound.soundlevel.range = 0;
			}
			else
			{
				sound.soundlevel = ReadInterval( pKey->GetString() );
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "time" ) )
		{
			sound.time = ReadInterval( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "rndwave" ) )
		{
			KeyValues *pWaves = pKey->GetFirstSubKey();
			sound.pWaves = pWaves;
			sound.waveCount = 0;
			while ( pWaves )
			{
				sound.waveCount++;
				pWaves = pWaves->GetNextKey();
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "position" ) )
		{
			if ( !Q_strcasecmp( pKey->GetString(), "random" ) )
			{
				randomPosition = true;
			}
			else
			{
				positionIndex = params.startingPosition + pKey->GetInt();
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "suppress_on_restore" ) )
		{
			suppress = Q_atoi( pKey->GetString() ) != 0 ? true : false;
		}
		else
		{
			DevMsg( 1, "Random Sound %s:Unknown command %s\n", pPlayRandom->GetName(), pKey->GetName() );
		}

		pKey = pKey->GetNextKey();
	}

	if ( positionIndex < 0 )
	{
		positionIndex = params.ambientPositionOverride;
	}
	else if ( params.positionOverride >= 0 )
	{
		positionIndex = params.positionOverride;
		randomPosition = false; // override trumps random position
	}

	// Sound is mared as "suppress_on_restore" so don't restart it
	if ( IsBeingRestored() && suppress )
	{
		return;
	}

	if ( sound.waveCount != 0 )
	{
		if ( positionIndex < 0 && !randomPosition )
		{
			sound.isAmbient = true; //origin true hl2mp2.ru
			AddRandomSound( sound );
		}
		else
		{
			sound.isAmbient = false;
			if ( randomPosition )
			{
				sound.isRandom = true;
			}
			else
			{
				if ( positionIndex > 31 || !(m_params.localBits & (1<<positionIndex) ) )
				{
					// suppress sounds if the position isn't available
					//DevMsg( 1, "Bad position %d\n", positionIndex );
					return;
				}
				sound.position = m_params.localSound[positionIndex];
			}
			AddRandomSound( sound );
		}
	}
}

void C_SoundscapeSystem::ProcessPlaySoundscape( KeyValues *pPlaySoundscape, subsoundscapeparams_t &paramsIn )
{
	subsoundscapeparams_t subParams = paramsIn;
	
	// sub-soundscapes NEVER set the DSP effects
	subParams.allowDSP = false;
	subParams.recurseLevel++;
	if ( subParams.recurseLevel > MAX_SOUNDSCAPE_RECURSION )
	{
		DevMsg( "Error!  Soundscape recursion overrun!\n" );
		return;
	}
	KeyValues *pKey = pPlaySoundscape->GetFirstSubKey();
	const char *pSoundscapeName = NULL;
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "volume" ) )
		{
			subParams.masterVolume = paramsIn.masterVolume * RandomInterval( ReadInterval( pKey->GetString() ) );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "position" ) )
		{
			subParams.startingPosition = paramsIn.startingPosition + pKey->GetInt();
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "positionoverride" ) )
		{
			if ( paramsIn.positionOverride < 0 )
			{
				subParams.positionOverride = paramsIn.startingPosition + pKey->GetInt();
				// positionoverride is only ever used to make a whole soundscape come from a point in space
				// So go ahead and default ambients there too.
				subParams.ambientPositionOverride = paramsIn.startingPosition + pKey->GetInt();
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "ambientpositionoverride" ) )
		{
			if ( paramsIn.ambientPositionOverride < 0 )
			{
				subParams.ambientPositionOverride = paramsIn.startingPosition + pKey->GetInt();
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "name" ) )
		{
			pSoundscapeName = pKey->GetString();
		}
		else if ( !Q_strcasecmp(pKey->GetName(), "soundlevel") )
		{
			DevMsg(1,"soundlevel not supported on sub-soundscapes\n");
		}
		else
		{
			DevMsg( 1, "Playsoundscape %s:Unknown command %s\n", pSoundscapeName ? pSoundscapeName : pPlaySoundscape->GetName(), pKey->GetName() );
		}
		pKey = pKey->GetNextKey();
	}

	if ( pSoundscapeName )
	{
		KeyValues *pSoundscapeKeys = SoundscapeByIndex( FindSoundscapeByName( pSoundscapeName ) );
		if ( pSoundscapeKeys )
		{
			StartSubSoundscape( pSoundscapeKeys, subParams );
		}
		else
		{
			DevMsg( 1, "Trying to play unknown soundscape %s\n", pSoundscapeName );
		}
	}
}

// special kind of looping sound with no spatialization
int C_SoundscapeSystem::AddLoopingAmbient( const char *pSoundName, float volume, int pitch )
{
	return AddLoopingSound( pSoundName, true, volume, SNDLVL_NONE, pitch, vec3_origin );
}

// add a looping sound to the list
// NOTE: will reuse existing entry (fade from current volume) if possible
//		this prevents pops
int C_SoundscapeSystem::AddLoopingSound( const char *pSoundName, bool isAmbient, float volume, soundlevel_t soundlevel, int pitch, const Vector &position )
{
	loopingsound_t *pSoundSlot = NULL;
	int soundSlot = m_loopingSounds.Count() - 1;
	bool bForceSoundUpdate = false;
	while ( soundSlot >= 0 )
	{
		loopingsound_t &sound = m_loopingSounds[soundSlot];

		// NOTE: Will always restart/crossfade positional sounds
		if ( sound.id != m_loopingSoundId && 
			sound.pitch == pitch && 
			!Q_strcasecmp( pSoundName, sound.pWaveName ) )
		{
			// Ambient sounds can reuse the slots.
			if ( isAmbient == true && 
				sound.isAmbient == true )
			{
				// reuse this sound
				pSoundSlot = &sound;
				break;
			}
			// Positional sounds can reuse the slots if the positions are the same.
			else if ( isAmbient == sound.isAmbient )
			{
				if ( VectorsAreEqual( position, sound.position, 0.1f ) )
				{
					// reuse this sound
					pSoundSlot = &sound;
					break;
				}
				else
				{
					// If it's trying to fade out one positional sound and fade in another, then it gets screwy
					// because it'll be sending alternating commands to the sound engine, referencing the same sound
					// (SOUND_FROM_WORLD, CHAN_STATIC, pSoundName). One of the alternating commands will be as
					// it fades the sound out, and one will be fading the sound in. 
					// Because this will occasionally cause the sound to vanish entirely, we stop the old sound immediately.
					StopLoopingSound(sound);
					pSoundSlot = &sound;

					// make a note to update the sound immediately. Otherwise, if its volume happens to be
					// the same as the old sound's volume, it will never update at all.
					bForceSoundUpdate = true; 
					break;
				}
			}
		}
		soundSlot--;
	}

	if ( soundSlot < 0 )
	{
		// can't find the sound in the list, make a new one
		soundSlot = m_loopingSounds.AddToTail();
		
		//if ( !enginesound->IsSoundPrecached( pSoundName ) )
			//enginesound->PrecacheSound( pSoundName );
			
		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName =  pSoundName;
		ep.m_nPitch = pitch;
		ep.m_nFlags = SND_CHANGE_VOL;

		if ( isAmbient ) {
			ep.m_SoundLevel = SNDLVL_NONE;
			ep.m_flVolume = 0.00;
			m_loopingSounds[soundSlot].volumeCurrent = 0.0;
		}
		else {
			ep.m_SoundLevel = soundlevel;
			ep.m_flVolume = 0.05;
			ep.m_pOrigin = &position;
			m_loopingSounds[soundSlot].volumeCurrent = 0.05;
		}

		CSingleUserRecipientFilter filter(m_hPlayer);
		CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
	}

	loopingsound_t &sound = m_loopingSounds[soundSlot];
	// fill out the slot
	sound.pWaveName = pSoundName;
	sound.volumeTarget = volume;
	sound.pitch = pitch;
	sound.id = m_loopingSoundId;
	sound.isAmbient = isAmbient;
	sound.position = position;
	sound.soundlevel = soundlevel;
	
	if (bForceSoundUpdate)
	{
		UpdateLoopingSound(sound);
	}

	return soundSlot;
}

// stop this loop forever
void C_SoundscapeSystem::StopLoopingSound( loopingsound_t &loopSound )
{
	CBaseEntity::StopSound( SOUND_FROM_WORLD, CHAN_STATIC, loopSound.pWaveName );
}

// update with new volume
void C_SoundscapeSystem::UpdateLoopingSound( loopingsound_t &loopSound )
{
	//if ( !enginesound->IsSoundPrecached( loopSound.pWaveName ) )
			//enginesound->PrecacheSound( loopSound.pWaveName );
	
	CSingleUserRecipientFilter filter(m_hPlayer);

	EmitSound_t ep;
	ep.m_nChannel = CHAN_STATIC;
	ep.m_pSoundName =  loopSound.pWaveName;
	ep.m_flVolume = loopSound.volumeCurrent;
	ep.m_SoundLevel = loopSound.soundlevel;
	ep.m_nFlags = SND_CHANGE_VOL;
	ep.m_nPitch = loopSound.pitch;
	ep.m_pOrigin = &loopSound.position;

	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

// add a recurring random sound event
int C_SoundscapeSystem::AddRandomSound( const randomsound_t &sound )
{
	int index = m_randomSounds.AddToTail( sound );
	m_randomSounds[index].nextPlayTime = gpGlobals->curtime + 0.5 * RandomInterval( sound.time );
	
	return index;
}

// play a random sound randomly from this parameterization table
void C_SoundscapeSystem::PlayRandomSound( randomsound_t &sound )
{
	Assert( sound.waveCount > 0 );

	int waveId = random->RandomInt( 0, sound.waveCount-1 );
	KeyValues *pWaves = sound.pWaves;
	while ( waveId > 0 && pWaves )
	{
		pWaves = pWaves->GetNextKey();
		waveId--;
	}
	if ( !pWaves )
		return;
	
	const char *pWaveName = pWaves->GetString();
	
	if ( !pWaveName )
		return;

	//if ( !enginesound->IsSoundPrecached( pWaveName ) )
			//enginesound->PrecacheSound( pWaveName );

	EmitSound_t ep;
	ep.m_nChannel = CHAN_STATIC;
	ep.m_pSoundName =  pWaveName;
	ep.m_flVolume = sound.masterVolume * RandomInterval( sound.volume );
	ep.m_nPitch = (int)RandomInterval( sound.pitch );

	soundlevel_t soundlevel;

	if ( sound.isAmbient ) {
		soundlevel = SNDLVL_NONE;
	}
	else {
		soundlevel = (soundlevel_t)(int)RandomInterval( sound.soundlevel );
		if( soundlevel <= 0 )
			soundlevel = SNDLVL_NORM;
		if ( sound.isRandom )
		{
			sound.position = GenerateRandomSoundPosition();
		}
		ep.m_pOrigin = &sound.position;
	}

	ep.m_SoundLevel = soundlevel;
	

	CSingleUserRecipientFilter filter(m_hPlayer);
	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

// walk the list of random sound commands and update
void C_SoundscapeSystem::UpdateRandomSounds( float gameTime )
{
	if ( gameTime < m_nextRandomTime )
		return;

	m_nextRandomTime = gameTime + 3600;	// add some big time to check again (an hour)

	for ( int i = m_randomSounds.Count()-1; i >= 0; i-- )
	{
		// time to play?
		if ( gameTime >= m_randomSounds[i].nextPlayTime )
		{
			// UNDONE: add this in to fix range?
			// float dt = m_randomSounds[i].nextPlayTime - gameTime;
			PlayRandomSound( m_randomSounds[i] );

			// now schedule the next occurrance
			// UNDONE: add support for "play once" sounds? FastRemove() here.
			m_randomSounds[i].nextPlayTime = gameTime + RandomInterval( m_randomSounds[i].time );
		}

		// update next time to check the queue
		if ( m_randomSounds[i].nextPlayTime < m_nextRandomTime )
		{
			m_nextRandomTime = m_randomSounds[i].nextPlayTime;
		}
	}
}
