
#include "Precomp.h"
#include "HRTFAudioSubsystem.h"
#include "AudioSource.h"

IMPLEMENT_CLASS(UHRTFAudioSubsystem);

UHRTFAudioSubsystem::UHRTFAudioSubsystem()
{
	guard(UHRTFAudioSubsystem::UHRTFAudioSubsystem);
	unguard;
}

void UHRTFAudioSubsystem::StaticConstructor()
{
	guard(UHRTFAudioSubsystem::StaticConstructor);

	UEnum* OutputRates = new(GetClass(), TEXT("OutputRates"))UEnum(nullptr);
	new(OutputRates->Names)FName(TEXT("8000Hz"));
	new(OutputRates->Names)FName(TEXT("11025Hz"));
	new(OutputRates->Names)FName(TEXT("16000Hz"));
	new(OutputRates->Names)FName(TEXT("22050Hz"));
	new(OutputRates->Names)FName(TEXT("32000Hz"));
	new(OutputRates->Names)FName(TEXT("44100Hz"));
	new(OutputRates->Names)FName(TEXT("48000Hz"));
	new(GetClass(), TEXT("UseFilter"), RF_Public)UBoolProperty(CPP_PROPERTY(UseFilter), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("UseSurround"), RF_Public)UBoolProperty(CPP_PROPERTY(UseSurround), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("UseStereo"), RF_Public)UBoolProperty(CPP_PROPERTY(UseStereo), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("UseCDMusic"), RF_Public)UBoolProperty(CPP_PROPERTY(UseCDMusic), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("UseDigitalMusic"), RF_Public)UBoolProperty(CPP_PROPERTY(UseDigitalMusic), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("ReverseStereo"), RF_Public)UBoolProperty(CPP_PROPERTY(ReverseStereo), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("Latency"), RF_Public)UIntProperty(CPP_PROPERTY(Latency), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("OutputRate"), RF_Public)UByteProperty(CPP_PROPERTY(OutputRate), TEXT("Audio"), CPF_Config, OutputRates);
	new(GetClass(), TEXT("Channels"), RF_Public)UIntProperty(CPP_PROPERTY(Channels), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("MusicVolume"), RF_Public)UByteProperty(CPP_PROPERTY(MusicVolume), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("SoundVolume"), RF_Public)UByteProperty(CPP_PROPERTY(SoundVolume), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("AmbientFactor"), RF_Public)UFloatProperty(CPP_PROPERTY(AmbientFactor), TEXT("Audio"), CPF_Config);
	new(GetClass(), TEXT("DopplerSpeed"), RF_Public)UFloatProperty(CPP_PROPERTY(DopplerSpeed), TEXT("Audio"), CPF_Config);

	unguard;
}

UBOOL UHRTFAudioSubsystem::Init()
{
	guard(UHRTFAudioSubsystem::Init);
	if (!Mixer)
		Mixer = AudioMixer::Create();
	return true;
	unguard;
}

void UHRTFAudioSubsystem::CleanUp()
{
	guard(UHRTFAudioSubsystem::CleanUp);
	unguard;
}

void UHRTFAudioSubsystem::PostEditChange()
{
	guard(UHRTFAudioSubsystem::PostEditChange);
	UAudioSubsystem::PostEditChange();

	OutputRate = Clamp<BYTE>(OutputRate, 0, 6);
	Latency = Clamp(Latency, 10, 250);
	Channels = Clamp(Channels, 0, 32);
	DopplerSpeed = Clamp(DopplerSpeed, 1.0f, 100000.0f);
	AmbientFactor = Clamp(AmbientFactor, 0.0f, 10.0f);

	unguard;
}

UBOOL UHRTFAudioSubsystem::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UHRTFAudioSubsystem::Exec);
	return false;
	unguard;
}

void UHRTFAudioSubsystem::SetViewport(UViewport* InViewport)
{
	guard(UHRTFAudioSubsystem::SetViewport);

	for (size_t i = 0; i < PlayingSounds.size(); i++)
		StopSound(i);

	if (Viewport != InViewport)
	{
		if (Viewport)
		{
			Mixer->PlayMusic({});
			for (TObjectIterator<UMusic> MusicIt; MusicIt; ++MusicIt)
			{
				if (MusicIt->Handle)
					UnregisterMusic(*MusicIt);
			}
		}

		Viewport = InViewport;

		if (Viewport)
		{
			if (Viewport->Actor->Song && Viewport->Actor->Transition == MTRAN_None)
				Viewport->Actor->Transition = MTRAN_Instant;

			PlayingSounds.resize(Channels);
		}
	}
	unguard;
}

UViewport* UHRTFAudioSubsystem::GetViewport()
{
	return Viewport;
}

void UHRTFAudioSubsystem::Update(FPointRegion Region, FCoords& Listener)
{
	guard(UHRTFAudioSubsystem::Update);

	AActor* ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;

	// See if any new ambient sounds need to be started
	UBOOL Realtime = Viewport->IsRealtime() && Viewport->Actor->Level->Pauser == TEXT("");
	if (Realtime)
	{
		guard(StartAmbience);
		for (INT i = 0; i < Viewport->Actor->GetLevel()->Actors.Num(); i++)
		{
			AActor* Actor = Viewport->Actor->GetLevel()->Actors(i);
			if (Actor && Actor->AmbientSound && FDistSquared(ViewActor->Location, Actor->Location) <= Square(Actor->WorldSoundRadius()))
			{
				INT Id = Actor->GetIndex() * 16 + SLOT_Ambient * 2;
				for (size_t j = 0; j < PlayingSounds.size(); j++)
				{
					if (PlayingSounds[j].Id == Id)
						break;

					if (j == Channels)
						PlaySound(Actor, Id, Actor->AmbientSound, Actor->Location, AmbientFactor * Actor->SoundVolume / 255.0f, Actor->WorldSoundRadius(), Actor->SoundPitch / 64.0f);
				}
			}
		}
		unguard;
	}

	guard(UpdateAmbience);
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		PlayingSound& Playing = PlayingSounds[i];
		if ((Playing.Id & 14) == SLOT_Ambient * 2)
		{
			if (FDistSquared(ViewActor->Location, Playing.Actor->Location) > Square(Playing.Actor->WorldSoundRadius()) ||
				Playing.Actor->AmbientSound != Playing.Sound ||
				!Realtime)
			{
				// Ambient sound went out of range
				StopSound(i);
			}
			else
			{
				// Update basic sound properties
				Playing.Volume = 2.0f * (AmbientFactor * Playing.Actor->SoundVolume / 255.0f);
				Playing.Radius = Playing.Actor->WorldSoundRadius();
				Playing.Pitch = Playing.Actor->SoundPitch / 64.0f;
			}
		}
	}
	unguard;

	// Update all active sounds
	guard(UpdateSounds);
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		PlayingSound& Playing = PlayingSounds[i];
		if (Playing.Actor)
			check(Playing.Actor->IsValid());
#if 0
		if (PlayingSounds[i].Id == 0)
		{
			// Sound is not playing.
			continue;
		}
		else if (Playing.Channel && Mixer->SoundFinished(Playing.Channel))
		{
			// Sound is finished.
			StopSound(i);
		}
		else
		{
			// Update positioning from actor, if available.
			if (Playing.Actor)
				Playing.Location = Playing.Actor->Location;

			// Update the priority.
			Playing.Priority = SoundPriority(Viewport, Playing.Location, Playing.Volume, Playing.Radius);

			// Compute the spatialization.
			FVector Location = Playing.Location.TransformPointBy(Coords);
			FLOAT PanAngle = appAtan2(Location.X, Abs(Location.Z));

			// Despatialize sounds when you get real close to them.
			FLOAT CenterDist = 0.1 * Playing.Radius;
			FLOAT Size = Location.Size();
			if (Location.SizeSquared() < Square(CenterDist))
				PanAngle *= Size / CenterDist;

			// Compute panning and volume.
			INT SoundPan = Clamp((INT)(AUDIO_MAXPAN / 2 + PanAngle * AUDIO_MAXPAN * 7 / 8 / PI), 0, AUDIO_MAXPAN);
			FLOAT Attenuation = Clamp(1.0 - Size / Playing.Radius, 0.0, 1.0);
			INT SoundVolume = Clamp((INT)(AUDIO_MAXVOLUME * Playing.Volume * Attenuation * EFFECT_FACTOR), 0, AUDIO_MAXVOLUME);
			if (ReverseStereo)
				SoundPan = AUDIO_MAXPAN - SoundPan;
			if (Location.Z < 0.0 && UseSurround)
				SoundPan = AUDIO_MIDPAN | AUDIO_SURPAN;

			// Compute doppler shifting (doesn't account for player's velocity).
			FLOAT Doppler = 1.0;
			if (Playing.Actor)
			{
				FLOAT V = (Playing.Actor->Velocity/*-ViewActor->Velocity*/) | (Playing.Actor->Location - ViewActor->Location).SafeNormal();
				Doppler = Clamp(1.0 - V / DopplerSpeed, 0.5, 2.0);
			}

			// Update the sound.
			Sample* Sample = GetSound(Playing.Sound);
			FVector Z(0, 0, 0);
			FVector L(Location.X / 400.0, Location.Y / 400.0, Location.Z / 400.0);

			if (Playing.Channel)
			{
				// Update an existing sound.
				guard(UpdateSample);
				Mixer->UpdateSound(Playing.Channel, (INT)(Sample->SamplesPerSec * Playing.Pitch * Doppler), SoundVolume, SoundPan );
				Playing.Channel->BasePanning = SoundPan;
				unguard;
			}
			else
			{
				// Start this new sound.
				guard(StartSample);
				if (!Playing.Channel)
					Playing.Channel = Mixer->PlaySound(i + 1, Sample, (INT)(Sample->SamplesPerSec * Playing.Pitch * Doppler), SoundVolume, SoundPan);
				check(Playing.Channel);
				unguard;
			}
		}
#endif
	}
	unguard;

	guard(UpdateMusic);
	if (Viewport->Actor && Viewport->Actor->Transition != MTRAN_None)
	{
		// To do: this needs to fade out the old song before switching

		if (CurrentSong)
		{
			Mixer->PlayMusic({});
			CurrentSong = nullptr;
		}

		CurrentSong = Viewport->Actor->Song;
		CurrentSection = Viewport->Actor->SongSection;

		if (CurrentSong && UseDigitalMusic)
		{
			CurrentSong->Data.Load();
			debugf(NAME_DevSound, TEXT("Register music: %s (%i)"), CurrentSong->GetPathName(), CurrentSong->Data.Num());

			try
			{
				uint8_t* ptr = (uint8_t*)CurrentSong->Data.GetData();
				int subsong = CurrentSection != 255 ? CurrentSection : 0;
				std::vector<uint8_t> data(ptr, ptr + CurrentSong->Data.Num());
				Mixer->PlayMusic(AudioSource::CreateMod(std::move(data), true, 0, subsong));
			}
			catch (const std::exception& e)
			{
				std::vector<TCHAR> tmp;
				for (const char* c = e.what(); *c != 0; c++)
					tmp.push_back((TCHAR)(*c));
				tmp.push_back(0);
				appErrorf(TEXT("Invalid music format in %s: %s"), CurrentSong->GetFullName(), tmp.data());
			}

			CurrentSong->Data.Unload();
		}

		Viewport->Actor->Transition = MTRAN_None;
	}
	unguard;

	guard(UpdateMixer);
	Mixer->Update();
	unguard;

	unguard;
}

UBOOL UHRTFAudioSubsystem::GetLowQualitySetting()
{
	guard(UHRTFAudioSubsystem::GetLowQualitySetting);
	return false;
	unguard;
}

void UHRTFAudioSubsystem::RenderAudioGeometry(FSceneNode* Frame)
{
	guard(UHRTFAudioSubsystem::RenderAudioGeometry);
	unguard;
}

void UHRTFAudioSubsystem::PostRender(FSceneNode* Frame)
{
	guard(UHRTFAudioSubsystem::PostRender);
	Frame->Viewport->Canvas->Color = FColor(255, 255, 255);

	//Frame->Viewport->Canvas->CurX = 0;
	//Frame->Viewport->Canvas->CurY = 64;
	//Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("HRTFAudioSubsystem Statistics"));

	unguard;
}

void UHRTFAudioSubsystem::RegisterMusic(UMusic* Music)
{
	guard(UHRTFAudioSubsystem::RegisterMusic);
	unguard;
}

void UHRTFAudioSubsystem::RegisterSound(USound* Sound)
{
	guard(UHRTFAudioSubsystem::RegisterSound);

	if (!Sound->Handle)
	{
		// Set the handle to avoid reentrance.
		Sound->Handle = (void*)-1;

		Sound->Data.Load();
		debugf(NAME_DevSound, TEXT("Register sound: %s (%i)"), Sound->GetPathName(), Sound->Data.Num());

		try
		{
			uint8_t* ptr = (uint8_t*)Sound->Data.GetData();
			Sound->Handle = Mixer->AddSound(AudioSource::CreateWav(std::vector<uint8_t>(ptr, ptr + Sound->Data.Num())));
		}
		catch (const std::exception& e)
		{
			Sound->Handle = nullptr;

			std::vector<TCHAR> tmp;
			for (const char* c = e.what(); *c != 0; c++)
				tmp.push_back((TCHAR)(*c));
			tmp.push_back(0);
			appErrorf(TEXT("Invalid sound format in %s: %s"), Sound->GetFullName(), tmp.data());
		}

		Sound->Data.Unload();
	}

	unguard;
}

void UHRTFAudioSubsystem::UnregisterSound(USound* Sound)
{
	guard(UHRTFAudioSubsystem::UnregisterSound);
	if (Sound->Handle)
	{
		for (size_t i = 0; i < PlayingSounds.size(); i++)
		{
			if (PlayingSounds[i].Sound == Sound)
				StopSound(i);
		}

		Mixer->RemoveSound((AudioSound*)Sound->Handle);
		Sound->Handle = nullptr;
	}
	unguard;
}

void UHRTFAudioSubsystem::UnregisterMusic(UMusic* Music)
{
	guard(UHRTFAudioSubsystem::UnregisterMusic);
	unguard;
}

UBOOL UHRTFAudioSubsystem::PlaySound(AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch)
{
	guard(UHRTFAudioSubsystem::PlaySound);
	return true;
	unguard;
}

void UHRTFAudioSubsystem::StopSound(size_t index)
{
	guard(UHRTFAudioSubsystem::StopSound);

	PlayingSound& sound = PlayingSounds[index];

	if (sound.Channel)
	{
		guard(StopSample);
		//Mixer->StopSound(Playing.Channel);
		unguard;
	}
	PlayingSounds[index] = {};

	unguard;
}

void UHRTFAudioSubsystem::NoteDestroy(AActor* Actor)
{
	guard(UHRTFAudioSubsystem::NoteDestroy);

	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		if (PlayingSounds[i].Actor == Actor)
		{
			if ((PlayingSounds[i].Id & 14) == SLOT_Ambient * 2)
			{
				// Stop ambient sound when actor dies
				StopSound(i);
			}
			else
			{
				// Unbind regular sounds from actors
				PlayingSounds[i].Actor = nullptr;
			}
		}
	}

	unguard;
}
