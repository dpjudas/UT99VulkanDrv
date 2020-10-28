/*=============================================================================
	AudioSubsystem.cpp: Unreal audio interface object.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
	Based on the UGalaxyAudioSubsystem interface.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "AudioPrivate.h"

/*------------------------------------------------------------------------------------
	UGenericAudioSubsystem.
------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UGenericAudioSubsystem);

UGenericAudioSubsystem::UGenericAudioSubsystem()
{
	guard(UGenericAudioSubsystem::UGenericAudioSubsystem);

	MusicFade			= 1.0;
	CurrentCDTrack		= 255;
	LastTime			= appSeconds();
	
	unguard;
}

void UGenericAudioSubsystem::StaticConstructor()
{
	guard(UGenericAudioSubsystem::StaticConstructor);

	UEnum* OutputRates = new( GetClass(), TEXT("OutputRates") )UEnum( NULL );
		new( OutputRates->Names )FName( TEXT("8000Hz" ) );
		new( OutputRates->Names )FName( TEXT("11025Hz") );
		new( OutputRates->Names )FName( TEXT("16000Hz") );
		new( OutputRates->Names )FName( TEXT("22050Hz") );
		new( OutputRates->Names )FName( TEXT("32000Hz") );
		new( OutputRates->Names )FName( TEXT("44100Hz") );
		new( OutputRates->Names )FName( TEXT("48000Hz") );
	new(GetClass(),TEXT("UseFilter"),       RF_Public)UBoolProperty  (CPP_PROPERTY(UseFilter      ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("UseSurround"),     RF_Public)UBoolProperty  (CPP_PROPERTY(UseSurround    ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("UseStereo"),       RF_Public)UBoolProperty  (CPP_PROPERTY(UseStereo      ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("UseCDMusic"),      RF_Public)UBoolProperty  (CPP_PROPERTY(UseCDMusic     ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("UseDigitalMusic"), RF_Public)UBoolProperty  (CPP_PROPERTY(UseDigitalMusic), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("ReverseStereo"),   RF_Public)UBoolProperty  (CPP_PROPERTY(ReverseStereo  ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("Latency"),         RF_Public)UIntProperty   (CPP_PROPERTY(Latency        ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("OutputRate"),      RF_Public)UByteProperty  (CPP_PROPERTY(OutputRate     ), TEXT("Audio"), CPF_Config, OutputRates );
	new(GetClass(),TEXT("Channels"), 		RF_Public)UIntProperty   (CPP_PROPERTY(Channels), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("MusicVolume"),     RF_Public)UByteProperty  (CPP_PROPERTY(MusicVolume    ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("SoundVolume"),     RF_Public)UByteProperty  (CPP_PROPERTY(SoundVolume    ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("AmbientFactor"),   RF_Public)UFloatProperty (CPP_PROPERTY(AmbientFactor  ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("DopplerSpeed"),    RF_Public)UFloatProperty (CPP_PROPERTY(DopplerSpeed   ), TEXT("Audio"), CPF_Config );

	unguard;
}

/*------------------------------------------------------------------------------------
	UObject Interface.
------------------------------------------------------------------------------------*/

void UGenericAudioSubsystem::PostEditChange()
{
	guard(UGenericAudioSubsystem::PostEditChange);

	// Validate configurable variables.
	OutputRate      = Clamp(OutputRate,(BYTE)0,(BYTE)6);
	Latency         = Clamp(Latency,10,250);
	Channels 		= Clamp(Channels,0,MAX_EFFECTS_CHANNELS);
	DopplerSpeed    = Clamp(DopplerSpeed,1.f,100000.f);
	AmbientFactor   = Clamp(AmbientFactor,0.f,10.f);
	SetVolumes();

	unguard;
}

void UGenericAudioSubsystem::Destroy()
{
	guard(UGenericAudioSubsystem::Destroy);
	if( Initialized )
	{
		// Unhook.
		USound::Audio = NULL;
		UMusic::Audio = NULL;

		// Shut down viewport.
		SetViewport( NULL );

		// Stop CD.
		if( UseCDMusic && CurrentCDTrack != 255 )
			StopCDAudio();

		// Shutdown soundsystem.
		safecall(AudioShutdown());

		debugf( NAME_Exit, TEXT("Generic audio subsystem shut down.") );
	}
	Super::Destroy();
	unguard;
}

void UGenericAudioSubsystem::ShutdownAfterError()
{
	guard(UGenericAudioSubsystem::ShutdownAfterError);

	// Unhook.
	USound::Audio = NULL;
	UMusic::Audio = NULL;

	// Safely shut down.
	debugf( NAME_Exit, TEXT("UGenericAudioSubsystem::ShutdownAfterError") );
	safecall(AudioStopOutput());
	if( Viewport )
		safecall(AudioShutdown());
	Super::ShutdownAfterError();
	unguard;
}

/*------------------------------------------------------------------------------------
	UAudioSubsystem Interface.
------------------------------------------------------------------------------------*/

UBOOL UGenericAudioSubsystem::Init()
{
	guard(UGenericAudioSubsystem::Init);

	// Initialize Unreal Audio library.
	guard(InitAudio);
	OutputMode = AUDIO_16BIT;
	if( UseFilter )
		OutputMode |= AUDIO_COSINE;
	if( UseStereo )
		OutputMode |= AUDIO_STEREO;
	OutputMode |= AUDIO_2DAUDIO;
	INT Rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
	INT Rate = Rates[OutputRate];
	if (AudioInit( Rate, OutputMode, Latency ) == 0)
		return false;
	unguard;

	// Allocate voices.
	guard(AllocateVoices);
	verify(AllocateVoices(Channels));
	unguard;
	
	// Initialized!
	USound::Audio = this;
	UMusic::Audio = this;
	Initialized = 1;

	debugf( NAME_Init, TEXT("Generic audio subsystem initialized.") );
	return 1;
	unguard;
}

void UGenericAudioSubsystem::SetViewport( UViewport* InViewport )
{
	guard(UGenericAudioSubsystem::SetViewport);

	// Stop playing sounds.
	for( INT i=0; i<Channels; i++ )
		StopSound( i );

	// Remember the viewport.
	if( Viewport != InViewport )
	{
		if( Viewport )
		{
			// Unregister everything.
			for( TObjectIterator<UMusic> MusicIt; MusicIt; ++MusicIt )
				if( MusicIt->Handle )
					UnregisterMusic( *MusicIt );

			// Shut down.
			safecall(AudioStopOutput());
		}
		Viewport = InViewport;
		if( Viewport )
		{
			// Determine startup parameters.
			if( Viewport->Actor->Song && Viewport->Actor->Transition==MTRAN_None )
				Viewport->Actor->Transition = MTRAN_Instant;

			// Start sound output.
			guard(AudioStartOutput);
			/*
			if (OutputRate == 0)
				OutputRate = 1;
			else if (OutputRate == 2)
				OutputRate = 3;
			else if ((OutputRate == 4) || (OutputRate = 6))
				OutputRate = 5;
			*/
			INT Rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
			INT Rate = Rates[OutputRate];
			INT Result = AudioStartOutput( Rate, OutputMode, Latency );
			if (Result == 0)
			{
				// Initialization failed.
				debugf( NAME_Init, TEXT("Failed to initialize audio subsystem.") );
				Viewport = NULL;
				return;
			}
			unguard;
			SetVolumes();			
		}
	}
	unguard;
}

UViewport* UGenericAudioSubsystem::GetViewport()
{
	guard(UGenericAudioSubsystem::GetViewport);
	return Viewport;
	unguard;
}

void UGenericAudioSubsystem::RegisterSound( USound* Sound )
{
	guard(UGenericAudioSubsystem::RegisterSound);
	checkSlow(Sound);
	if( !Sound->Handle )
	{
		// Set the handle to avoid reentrance.
		Sound->Handle = (void*)-1;

		// Load the data.
		Sound->Data.Load();
		debugf( NAME_DevSound, TEXT("Register sound: %s (%i)"), Sound->GetPathName(), Sound->Data.Num() );
		check(Sound->Data.Num()>0);

		// Register the sound.
		guard(LoadSample);
		MemChunk SoundChunk;
		SoundChunk.Data			= &Sound->Data(0);
		SoundChunk.DataLength	= Sound->Data.Num();
		SoundChunk.Position		= 0;
		Sound->Handle = LoadSample( &SoundChunk, Sound->GetFullName() );
		if( !Sound->Handle )
			appErrorf( TEXT("Invalid sound format in %s"), Sound->GetFullName() );
		unguardf(( TEXT("(%i)"), Sound->Data.Num() ));

		// Unload the data.
		Sound->Data.Unload();
	}
	unguard;
}

void UGenericAudioSubsystem::UnregisterSound( USound* Sound )
{
	guard(UGenericAudioSubsystem::UnregisterSound);
	check(Sound);
	if( Sound->Handle )
	{
		debugf( NAME_DevSound, TEXT("Unregister sound: %s"), Sound->GetFullName() );

		// Stop this sound.
		for( INT i=0; i<Channels; i++ )
			if ( PlayingSounds[i].Sound == Sound )
				StopSound( i );

		// Unload this sound.
		safecall( UnloadSample( (Sample*) Sound->Handle ) );
	}
	unguard;
}

void UGenericAudioSubsystem::UnregisterMusic( UMusic* Music )
{
	guard(UGenericAudioSubsystem::UnregisterMusic);

	unguard;
}

UBOOL UGenericAudioSubsystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UGenericAudioSubsystem::Exec);
	const TCHAR* Str = Cmd;
	if( ParseCommand(&Str,TEXT("ASTAT")) )
	{
		if( ParseCommand(&Str,TEXT("Audio")) )
		{
			AudioStats ^= 1;
			return 1;
		}
		if( ParseCommand(&Str,TEXT("Detail")) )
		{
			DetailStats ^= 1;
			return 1;
		}
	}
	return 0;
	
	unguard;
}

UBOOL UGenericAudioSubsystem::PlaySound
(
	AActor*	Actor,
	INT		Id,
	USound*	Sound,
	FVector	Location,
	FLOAT	Volume,
	FLOAT	Radius,
	FLOAT	Pitch
)
{
	guard(UGenericAudioSubsystem::PlaySound);
	check(Radius);
	if( !Viewport || !Sound )
		return 0;

	// Allocate a new slot if requested.
	if( (Id&14)==2*SLOT_None )
		Id = 16 * --FreeSlot;

	// Compute priority.
	FLOAT Priority = SoundPriority( Viewport, Location, Volume, Radius );

	// If already playing, stop it.
	INT   Index        = -1;
	FLOAT BestPriority = Priority;
	for( INT i=0; i<Channels; i++ )
	{
		FPlayingSound& Playing = PlayingSounds[i];
		if( (Playing.Id&~1)==(Id&~1) )
		{
			// Skip if not interruptable.
			if( Id&1 )
				return 0;

			// Stop the sound.
			Index = i;
			break;
		}
		else if( Playing.Priority<=BestPriority )
		{
			Index = i;
			BestPriority = Playing.Priority;
		}
	}

	// If no sound, or its priority is overruled, stop it.
	if( Index==-1 )
		return 0;

	// Put the sound on the play-list.
	StopSound( Index );
	if( Sound!=(USound*)-1 )
		PlayingSounds[Index] = FPlayingSound( Actor, Id, Sound, Location, Volume, Radius, Pitch, Priority );
	return 1;

	unguard;
}

void UGenericAudioSubsystem::NoteDestroy( AActor* Actor )
{
	guard(UGenericAudioSubsystem::NoteDestroy);
	check(Actor);
	check(Actor->IsValid());

	// Stop referencing actor.
	for( INT i=0; i<Channels; i++ )
	{
		if( PlayingSounds[i].Actor==Actor )
		{
			if( (PlayingSounds[i].Id&14)==SLOT_Ambient*2 )
			{
				// Stop ambient sound when actor dies.
				StopSound( i );
			}
			else
			{
				// Unbind regular sounds from actors.
				PlayingSounds[i].Actor = NULL;
			}
		}
	}

	unguard;
}

void UGenericAudioSubsystem::RenderAudioGeometry( FSceneNode* Frame )
{
	guard(UGenericAudioSubsystem::RenderAudioGeometry);

	unguard;
}

void UGenericAudioSubsystem::Update( FPointRegion Region, FCoords& Coords )
{
	guard(UGenericAudioSubsystem::Update);
	if( !Viewport )
		return;
	
	// Lock to sync sound.
	ALock;
	
	// Time passes...
	FTime DeltaTime = appSeconds() - LastTime;
	LastTime += DeltaTime;
	DeltaTime = Clamp( DeltaTime, 0.0, 1.0 );

	AActor *ViewActor = Viewport->Actor->ViewTarget?Viewport->Actor->ViewTarget:Viewport->Actor;

	// See if any new ambient sounds need to be started.
	UBOOL Realtime = Viewport->IsRealtime() && Viewport->Actor->Level->Pauser==TEXT("");
	if( Realtime )
	{
		guard(StartAmbience);
		for( INT i=0; i<Viewport->Actor->GetLevel()->Actors.Num(); i++ )
		{
			AActor* Actor = Viewport->Actor->GetLevel()->Actors(i);
			if
			(	Actor
			&&	Actor->AmbientSound
			&&	FDistSquared(ViewActor->Location,Actor->Location)<=Square(Actor->WorldSoundRadius()) )
			{
				INT Id = Actor->GetIndex()*16+SLOT_Ambient*2;
				for( INT j=0; j<Channels; j++ )
					if( PlayingSounds[j].Id==Id )
						break;
				if( j==Channels )
					PlaySound( Actor, Id, Actor->AmbientSound, Actor->Location, AmbientFactor*Actor->SoundVolume/255.0, Actor->WorldSoundRadius(), Actor->SoundPitch/64.0 );
			}
		}
		unguard;
	}

	// Update all playing ambient sounds.
	guard(UpdateAmbience);
	for( INT i=0; i<Channels; i++ )
	{
		FPlayingSound& Playing = PlayingSounds[i];
		if( (Playing.Id&14)==SLOT_Ambient*2 )
		{
			check(Playing.Actor);
			if
			(	FDistSquared(ViewActor->Location,Playing.Actor->Location)>Square(Playing.Actor->WorldSoundRadius())
			||	Playing.Actor->AmbientSound!=Playing.Sound 
			||  !Realtime )
			{
				// Ambient sound went out of range.
				StopSound( i );
			}
			else
			{
				// Update basic sound properties.
				FLOAT Brightness = 2.0 * (AmbientFactor*Playing.Actor->SoundVolume/255.0);
				if( Playing.Actor->LightType!=LT_None )
				{
					FPlane Color;
					Brightness *= Playing.Actor->LightBrightness/255.0;
//					Viewport->GetOuterUClient()->Engine->Render->GlobalLighting( (Viewport->Actor->ShowFlags & SHOW_PlayerCtrl)!=0, Playing.Actor, Brightness, Color );
				}
				Playing.Volume = Brightness;
				Playing.Radius = Playing.Actor->WorldSoundRadius();
				Playing.Pitch  = Playing.Actor->SoundPitch/64.0;
			}
		}
	}
	unguard;

	// Update all active sounds.
	guard(UpdateSounds);
	for( INT Index=0; Index<Channels; Index++ )
	{
		FPlayingSound& Playing = PlayingSounds[Index];
		if( Playing.Actor )
			check(Playing.Actor->IsValid());
		if( PlayingSounds[Index].Id==0 )
		{
			// Sound is not playing.
			continue;
		}
		else if( Playing.Channel && SampleFinished(Playing.Channel) )
		{
			// Sound is finished.
			StopSound( Index );
		}
		else
		{
			// Update positioning from actor, if available.
			if( Playing.Actor )
				Playing.Location = Playing.Actor->Location;void UpdateSample( Voice* InVoice, INT Freq, INT Volume, INT Panning );

			// Update the priority.
			Playing.Priority = SoundPriority( Viewport, Playing.Location, Playing.Volume, Playing.Radius );

			// Compute the spatialization.
			FVector Location = Playing.Location.TransformPointBy( Coords );
			FLOAT   PanAngle = appAtan2(Location.X, Abs(Location.Z));

			// Despatialize sounds when you get real close to them.
			FLOAT CenterDist  = 0.1*Playing.Radius;
			FLOAT Size        = Location.Size();
			if( Location.SizeSquared() < Square(CenterDist) )
				PanAngle *= Size / CenterDist;

			// Compute panning and volume.
			INT     SoundPan      = Clamp( (INT)(AUDIO_MAXPAN/2 + PanAngle*AUDIO_MAXPAN*7/8/PI), 0, AUDIO_MAXPAN );
			FLOAT   Attenuation = Clamp(1.0-Size/Playing.Radius,0.0,1.0);
			INT     SoundVolume   = Clamp( (INT)(AUDIO_MAXVOLUME * Playing.Volume * Attenuation * EFFECT_FACTOR), 0, AUDIO_MAXVOLUME );
			if( ReverseStereo )
				SoundPan = AUDIO_MAXPAN - SoundPan;
			if( Location.Z<0.0 && UseSurround )
				SoundPan = AUDIO_MIDPAN | AUDIO_SURPAN;

			// Compute doppler shifting (doesn't account for player's velocity).
			FLOAT Doppler=1.0;
			if( Playing.Actor )
			{
				FLOAT V = (Playing.Actor->Velocity/*-ViewActor->Velocity*/) | (Playing.Actor->Location - ViewActor->Location).SafeNormal();
				Doppler = Clamp( 1.0 - V/DopplerSpeed, 0.5, 2.0 );
			}

			// Update the sound.
			Sample* Sample = GetSound(Playing.Sound);
			FVector Z(0,0,0);
			FVector L(Location.X/400.0,Location.Y/400.0,Location.Z/400.0);

			if( Playing.Channel )
			{
				// Update an existing sound.
				guard(UpdateSample);
				UpdateSample
				( 
					Playing.Channel,
					(INT) (Sample->SamplesPerSec * Playing.Pitch * Doppler),
					SoundVolume,
					SoundPan
				);
				Playing.Channel->BasePanning = SoundPan;
				unguard;
			}
			else
			{
				// Start this new sound.
				guard(StartSample);
				if( !Playing.Channel ) 
					Playing.Channel = StartSample
						( Index+1, Sample, 
						  (INT) (Sample->SamplesPerSec * Playing.Pitch * Doppler), 
						  SoundVolume, SoundPan );
				check(Playing.Channel);
				unguard;
			}
		}
	}
	unguard;

	// Unlock.
	AUnlock;
	unguard;
}

void UGenericAudioSubsystem::PostRender( FSceneNode* Frame )
{
	guard(UGenericAudioSubsystem::PostRender);
	Frame->Viewport->Canvas->Color = FColor(255,255,255);
	if( AudioStats )
	{
		Frame->Viewport->Canvas->CurX=0;
		Frame->Viewport->Canvas->CurY=16;
		Frame->Viewport->Canvas->WrappedPrintf
		(
			Frame->Viewport->Canvas->SmallFont,
			0, TEXT("GenericAudioSubsystem Statistics")
		);
		for (INT i=0; i<Channels; i++)
		{
			if (PlayingSounds[i].Channel)
			{
				INT Factor;
				if (DetailStats)
					Factor = 16;
				else
					Factor = 8;
					
				// Current Sound.
				Frame->Viewport->Canvas->CurX=10;
				Frame->Viewport->Canvas->CurY=24 + Factor*i;
				Frame->Viewport->Canvas->WrappedPrintf
				( Frame->Viewport->Canvas->SmallFont, 0, TEXT("Channel %2i: %s"),
					i, PlayingSounds[i].Sound->GetFullName() );

				if (DetailStats)
				{
					// Play meter.
					VoiceStats CurrentStats;
					GetVoiceStats( &CurrentStats, PlayingSounds[i].Channel );
					Frame->Viewport->Canvas->CurX=10;
					Frame->Viewport->Canvas->CurY=32 + Factor*i;
					Frame->Viewport->Canvas->WrappedPrintf
					( Frame->Viewport->Canvas->SmallFont, 0, TEXT("  [%s] %05.1f\% Vol: %05.2f"),
						*CurrentStats.CompletionString, CurrentStats.Completion*100, PlayingSounds[i].Volume );
				}
			} else {
				INT Factor;
				if (DetailStats)
					Factor = 16;
				else
					Factor = 8;
					
				Frame->Viewport->Canvas->CurX=10;
				Frame->Viewport->Canvas->CurY=24 + Factor*i;
				if (i >= 10)
					Frame->Viewport->Canvas->WrappedPrintf
					( Frame->Viewport->Canvas->SmallFont, 0, TEXT("Channel %i:  None"),
						i );
				else
					Frame->Viewport->Canvas->WrappedPrintf
					( Frame->Viewport->Canvas->SmallFont, 0, TEXT("Channel %i: None"),
						i );

				if (DetailStats)
				{
					// Play meter.
					Frame->Viewport->Canvas->CurX=10;
					Frame->Viewport->Canvas->CurY=32 + Factor*i;
					Frame->Viewport->Canvas->WrappedPrintf
					( Frame->Viewport->Canvas->SmallFont, 0, TEXT("  [----------]") );
				}
			}
		}
	}
	unguard;
}

/*------------------------------------------------------------------------------------
	Internals.
------------------------------------------------------------------------------------*/

void UGenericAudioSubsystem::SetVolumes()
{
	guard(UGenericAudioSubsystem::SetVolumes);

	// Normalize the volumes.
	FLOAT NormSoundVolume = SoundVolume/255.0;
	FLOAT NormMusicVolume = Clamp(MusicVolume/255.0,0.0,1.0);

	// Set music and effects volumes.
	verify( SetSampleVolume( 127*NormSoundVolume ) );
	if( UseDigitalMusic )
		verify( SetMusicVolume( 127*NormMusicVolume*Max(MusicFade,0.f) ) );
	if( UseCDMusic )
		SetCDAudioVolume( 127*NormMusicVolume*Max(MusicFade,0.f) );

	unguard;
}

void UGenericAudioSubsystem::StopSound( INT Index )
{
	guard(UGenericAudioSubsystem::StopSound);
	FPlayingSound& Playing = PlayingSounds[Index];

	if( Playing.Channel )
	{
		guard(StopSample);
		StopSample( Playing.Channel );
		unguard;
	}
	PlayingSounds[Index] = FPlayingSound();

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
