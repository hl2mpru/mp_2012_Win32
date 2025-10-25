//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_SOUNDSCAPE_H
#define C_SOUNDSCAPE_H
#ifdef _WIN32
#pragma once
#endif

class IGameSystem;
class CHL2MP_Player_fix;
struct audioparams_t;

// Keep an array of all looping sounds so they can be faded in/out
// OPTIMIZE: Get a handle/pointer to the engine's sound channel instead 
//			of searching each frame!
struct loopingsound_t
{
	Vector		position;		// position (if !isAmbient)
	const char *pWaveName;		// name of the wave file
	float		volumeTarget;	// target volume level (fading towards this)
	float		volumeCurrent;	// current volume level
	soundlevel_t soundlevel;	// sound level (if !isAmbient)
	int			pitch;			// pitch shift
	int			id;				// Used to fade out sounds that don't belong to the most current setting
	bool		isAmbient;		// Ambient sounds have no spatialization - they play from everywhere
};

#include "interval.h"

struct randomsound_t
{
	Vector		position;
	float		nextPlayTime;	// time to play a sound from the set
	interval_t	time;
	interval_t	volume;
	interval_t	pitch;
	interval_t	soundlevel;
	float		masterVolume;
	int			waveCount;
	bool		isAmbient;
	bool		isRandom;
	KeyValues	*pWaves;

	void Init()
	{
		memset( this, 0, sizeof(*this) );
	}
};

struct subsoundscapeparams_t
{
	int		recurseLevel;		// test for infinite loops in the script / circular refs
	float	masterVolume;
	int		startingPosition;
	int		positionOverride;	// forces all sounds to this position
	int		ambientPositionOverride;	// forces all ambient sounds to this position
	bool	allowDSP;
	bool	wroteSoundMixer;
	bool	wroteDSPVolume;
};

class C_SoundscapeSystem : public CAutoGameSystemPerFrame
{
public:

	virtual char const *Name() { return "C_SoundScapeSystem"; }
	C_SoundscapeSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	C_SoundscapeSystem()
	{
		m_nRestoreFrame = -1;
	}

	~C_SoundscapeSystem()
	{
		Shutdown();
	}

	void OnStopAllSounds()
	{
		m_params.entIndex.Set( NULL );
		m_params.soundscapeIndex = -1;
		m_loopingSounds.Purge();
		m_randomSounds.Purge();
	}

	// IClientSystem hooks, not needed
	virtual void LevelInitPreEntity( void )
	{
		Shutdown();
		Init();
		if( m_hPlayer ) {
			const char *msg = UTIL_VarArgs("INIT C_SoundscapeSystem: %d", this);
			UTIL_SayTextAll( msg );
		}
		TouchSoundFiles();
	}

	virtual void LevelInitPostEntity() 
	{
		if ( !m_pSoundMixerVar )
		{
			m_pSoundMixerVar = (ConVar *)cvar->FindVar( "snd_soundmixer" );
		}
		if ( !m_pDSPVolumeVar )
		{
			m_pDSPVolumeVar = (ConVar *)cvar->FindVar( "dsp_volume" );
		}
	}

	// The level is shutdown in two parts
	virtual void LevelShutdownPreEntity() {}
	// Entities are deleted / released here...
	virtual void LevelShutdownPostEntity()
	{
		OnStopAllSounds();
	}

	virtual void OnSave() {}
	virtual void OnRestore()
	{
		m_nRestoreFrame = gpGlobals->framecount;
	}
	virtual void SafeRemoveIfDesired() {}

	// Called before rendering
	virtual void PreRender() { }

	// Called after rendering
	virtual void PostRender() { }

	// IClientSystem hooks used
	virtual bool Init();
	virtual void Shutdown();
	// Gets called each frame
	virtual void FrameUpdatePostEntityThink( void );

	void PrintDebugInfo()
	{
		Msg( "\n------- CLIENT SOUNDSCAPES -------\n" );
		for ( int i=0; i < m_soundscapes.Count(); i++ )
		{
			Msg( "- %d: %s\n", i, m_soundscapes[i]->GetName() );
		}
		if ( m_forcedSoundscapeIndex >= 0 )
		{
			Msg( "- PLAYING DEBUG SOUNDSCAPE: %d [%s]\n", m_forcedSoundscapeIndex, SoundscapeNameByIndex(m_forcedSoundscapeIndex) );
		}
		Msg( "- CURRENT SOUNDSCAPE: %d [%s]\n", m_params.soundscapeIndex.Get(), SoundscapeNameByIndex(m_params.soundscapeIndex) );
		Msg( "----------------------------------\n\n" );
	}

	
	// local functions
	void UpdateAudioParams( audioparams_t &audio );
	void GetAudioParams( audioparams_t &out ) const { out = m_params; }
	int GetCurrentSoundscape() 
	{ 
		if ( m_forcedSoundscapeIndex >= 0 )
			return m_forcedSoundscapeIndex;
		return m_params.soundscapeIndex; 
	}
	void DevReportSoundscapeName( int index );
	void UpdateLoopingSounds( float frametime );
	int AddLoopingAmbient( const char *pSoundName, float volume, int pitch );
	void UpdateLoopingSound( loopingsound_t &loopSound );
	void StopLoopingSound( loopingsound_t &loopSound );
	int AddLoopingSound( const char *pSoundName, bool isAmbient, float volume, 
		soundlevel_t soundLevel, int pitch, const Vector &position );
	int AddRandomSound( const randomsound_t &sound );
	void PlayRandomSound( randomsound_t &sound );
	void UpdateRandomSounds( float gameClock );
	Vector GenerateRandomSoundPosition();

	void ForceSoundscape( const char *pSoundscapeName, float radius );

	int FindSoundscapeByName( const char *pSoundscapeName );
	const char *SoundscapeNameByIndex( int index );
	KeyValues *SoundscapeByIndex( int index );
	
	// main-level soundscape processing, called on new soundscape
	void StartNewSoundscape( KeyValues *pSoundscape );
	void StartSubSoundscape( KeyValues *pSoundscape, subsoundscapeparams_t &params );

	// root level soundscape keys
	// add a process for each new command here
	// "dsp"
	void ProcessDSP( KeyValues *pDSP );
	// "dsp_player"
	void ProcessDSPPlayer( KeyValues *pDSPPlayer );
	// "playlooping"
	void ProcessPlayLooping( KeyValues *pPlayLooping, const subsoundscapeparams_t &params );	
	// "playrandom"
	void ProcessPlayRandom( KeyValues *pPlayRandom, const subsoundscapeparams_t &params );
	// "playsoundscape"
	void ProcessPlaySoundscape( KeyValues *pPlaySoundscape, subsoundscapeparams_t &params );
	// "soundmixer"
	void ProcessSoundMixer( KeyValues *pSoundMixer, subsoundscapeparams_t &params );
	// "dsp_volume"
	void ProcessDSPVolume( KeyValues *pKey, subsoundscapeparams_t &params );

	CHandle<CHL2MP_Player_fix> m_hPlayer;

private:

	bool	IsBeingRestored() const
	{
		return gpGlobals->framecount == m_nRestoreFrame ? true : false;
	}

	void	AddSoundScapeFile( const char *filename );

	void		TouchPlayLooping( KeyValues *pAmbient );
	void		TouchPlayRandom( KeyValues *pPlayRandom );
	void		TouchWaveFiles( KeyValues *pSoundScape );
	void		TouchSoundFile( char const *wavefile );

	void		TouchSoundFiles();
	
	int							m_nRestoreFrame;

	CUtlVector< KeyValues * >	m_SoundscapeScripts;	// The whole script file in memory
	CUtlVector<KeyValues *>		m_soundscapes;			// Lookup by index of each root section
	audioparams_t				m_params;				// current player audio params
	CUtlVector<loopingsound_t>	m_loopingSounds;		// list of currently playing sounds
	CUtlVector<randomsound_t>	m_randomSounds;			// list of random sound commands
	float						m_nextRandomTime;		// next time to play a random sound
	int							m_loopingSoundId;		// marks when the sound was issued
	int							m_forcedSoundscapeIndex;// >= 0 if this a "forced" soundscape? i.e. debug mode?
	float						m_forcedSoundscapeRadius;// distance to spatialized sounds

	static ConVar *m_pDSPVolumeVar;
	static ConVar *m_pSoundMixerVar;

};

#endif // C_SOUNDSCAPE_H
