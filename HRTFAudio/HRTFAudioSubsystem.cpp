
#include "Precomp.h"
#include "HRTFAudioSubsystem.h"
#include "AudioSource.h"
#include "resource.h"

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
	{
		try
		{
			HRSRC resourceInfo = FindResource(hInstance, MAKEINTRESOURCE(IDR_HRTF), TEXT("Zip"));
			if (!resourceInfo)
				throw std::runtime_error("FindResource(IDR_HRTF, Zip) failed");

			HGLOBAL resource = LoadResource(hInstance, resourceInfo);
			if (!resource)
				throw std::runtime_error("LoadResource(IDR_HRTF, Zip) failed");

			size_t zipSize = SizeofResource(hInstance, resourceInfo);

			void* zipData = LockResource(resource);
			if (!zipData)
			{
				FreeResource(resource);
				throw std::runtime_error("LockResource(IDR_HRTF, Zip) failed");
			}

			Mixer = AudioMixer::Create(zipData, zipSize);

			UnlockResource(resource);
			FreeResource(resource);
		}
		catch (const std::exception& e)
		{
			std::vector<TCHAR> tmp;
			for (const char* c = e.what(); *c != 0; c++)
				tmp.push_back((TCHAR)(*c));
			tmp.push_back(0);
			appErrorf(TEXT("%s"), tmp.data());
			return false;
		}
	}
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
	const TCHAR* Str = Cmd;
	if (ParseCommand(&Str, TEXT("ASTAT")))
	{
		if (ParseCommand(&Str, TEXT("Audio")))
		{
			AudioStats = !AudioStats;
			return 1;
		}
	}
	return 0;
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

	StartAmbience();
	UpdateAmbience();
	UpdateSounds(Listener);
	UpdateMusic();
	UpdateReverb();

	Mixer->SetMusicVolume(MusicVolume / 255.0f * 0.50f);
	Mixer->SetSoundVolume(SoundVolume / 255.0f * 0.25f);

	guard(UpdateMixer);
	Mixer->Update();
	unguard;

	unguard;
}

void UHRTFAudioSubsystem::StartAmbience()
{
	guard(StartAmbience);
	UBOOL Realtime = Viewport->IsRealtime() && Viewport->Actor->Level->Pauser == TEXT("");
	if (Realtime)
	{
		AActor* ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
		for (INT i = 0; i < Viewport->Actor->GetLevel()->Actors.Num(); i++)
		{
			AActor* Actor = Viewport->Actor->GetLevel()->Actors(i);
			if (Actor && Actor->AmbientSound && FDistSquared(ViewActor->Location, Actor->Location) <= Square(Actor->WorldSoundRadius()))
			{
				INT Id = Actor->GetIndex() * 16 + SLOT_Ambient * 2;
				bool foundSound = false;
				for (size_t j = 0; j < PlayingSounds.size(); j++)
				{
					if (PlayingSounds[j].Id == Id)
					{
						foundSound = true;
						break;
					}
				}
				if (!foundSound)
					PlaySound(Actor, Id, Actor->AmbientSound, Actor->Location, AmbientFactor * Actor->SoundVolume / 255.0f, Actor->WorldSoundRadius(), Actor->SoundPitch / 64.0f);
			}
		}
	}
	unguard;
}

void UHRTFAudioSubsystem::UpdateAmbience()
{
	guard(UpdateAmbience);
	AActor* ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
	UBOOL Realtime = Viewport->IsRealtime() && Viewport->Actor->Level->Pauser == TEXT("");
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
}

void UHRTFAudioSubsystem::UpdateSounds(FCoords& Listener)
{
	guard(UpdateSounds);
	AActor* ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		PlayingSound& Playing = PlayingSounds[i];
		if (Playing.Actor)
			check(Playing.Actor->IsValid());

		if (Playing.Id == 0)
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
			Playing.Priority = SoundPriority(Viewport, Playing.Id, Playing.Location, Playing.Volume, Playing.Radius);

			// Compute the spatialization.
			FVector Location = Playing.Location.TransformPointBy(Listener);
			if (ReverseStereo)
				Location.X = -Location.X;
			float PanAngle = appAtan2(Location.X, Abs(Location.Z));

			// Despatialize sounds when you get real close to them.
			FLOAT CenterDist = 0.1 * Playing.Radius;
			FLOAT Size = Location.Size();
			if (Location.SizeSquared() < Square(CenterDist))
			{
				float t = (Size / CenterDist);
				PanAngle *= t;
				Location = (Location.SafeNormal() * t + FVector(0.0f, 0.0f, 2.0f) * (1.0f - t)).SafeNormal();
			}
			else
			{
				Location = Location.SafeNormal();
			}

			// Compute panning and volume.
			float SoundPan = Clamp(PanAngle * 7 / 8 / PI, -1.0f, 1.0f);
			float Attenuation = Clamp(1.0f - (Size * Size) / (Playing.Radius * Playing.Radius), 0.0f, 1.0f);
			float SoundVolume = Clamp(Playing.Volume * Attenuation, 0.0f, 1.0f);

			// Compute doppler shifting (doesn't account for player's velocity).
			FLOAT Doppler = 1.0f;
			if (Playing.Actor)
			{
				FLOAT V = (Playing.Actor->Velocity) | (Playing.Actor->Location - ViewActor->Location).SafeNormal();
				Doppler = Clamp(1.0f - V / DopplerSpeed, 0.5f, 2.0f);
			}

			// Lower volume for anything not directly visible
			if (Playing.Actor)
			{
				if (Viewport->Actor->XLevel->Model->FastLineCheck(ViewActor->Location, Playing.Actor->Location) == 0)
				{
					SoundVolume *= 0.75f;
				}
			}

			// 469 changed SLOT_Interface to behave differently. Simulate what the old ClientPlaySound function did:
			if ((Playing.Id & 14) == 2 * SLOT_Interface)
			{
				SoundVolume = 4.0f;
				SoundPan = 0.0f;
				Doppler = 1.0f;
				Location = FVector(0.0f, 0.0f, 1.0f);
			}

			// Update the sound.
			AudioSound* Sound = GetSound(Playing.Sound);

			Playing.CurrentVolume = SoundVolume;
			if (Playing.Channel)
			{
				Mixer->UpdateSound(Playing.Channel, Sound, SoundVolume, SoundPan, Playing.Pitch * Doppler, Location.X, Location.Y, Location.Z);
			}
			else
			{
				Playing.Channel = Mixer->PlaySound(i + 1, Sound, SoundVolume, SoundPan, Playing.Pitch * Doppler, Location.X, Location.Y, Location.Z);
			}
		}
	}
	unguard;
}

void UHRTFAudioSubsystem::UpdateMusic()
{
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
}

void UHRTFAudioSubsystem::UpdateReverb()
{
	guard(UHRTFAudioSubsystem::UpdateReverb);
	AActor* ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
	if (ViewActor->Region.Zone && ViewActor->Region.Zone->bReverbZone)
	{
		AZoneInfo* zone = ViewActor->Region.Zone;
		float volume = zone->MasterGain / 255.0f;
		float hfcutoff = Clamp(zone->CutoffHz, 0, 44100);
		std::vector<float> time;
		std::vector<float> gain;
		for (int i = 0; i < ARRAY_COUNT(zone->Delay); i++)
		{
			time.push_back(Clamp(zone->Delay[i] / 500.0f, 0.001f, 0.340f));
			gain.push_back(Clamp(zone->Gain[i] / 255.0f, 0.001f, 0.999f));
		}
		Mixer->SetReverb(volume, hfcutoff, std::move(time), std::move(gain));
	}
	else
	{
		Mixer->SetReverb(0.0f, 0.0f, {}, {});
	}
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

	if (AudioStats)
	{
		Frame->Viewport->Canvas->CurX = 0;
		Frame->Viewport->Canvas->CurY = 16;
		Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("HRTFAudioSubsystem Statistics"));
		for (INT i = 0; i < Channels; i++)
		{
			if (PlayingSounds[i].Channel)
			{
				INT Factor = 8;

				Frame->Viewport->Canvas->CurX = 10;
				Frame->Viewport->Canvas->CurY = 24 + Factor * i;
				Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Channel %2i: Vol: %05.2f %s"), i, PlayingSounds[i].CurrentVolume, PlayingSounds[i].Sound->GetFullName());
			}
			else
			{
				INT Factor = 8;

				Frame->Viewport->Canvas->CurX = 10;
				Frame->Viewport->Canvas->CurY = 24 + Factor * i;
				if (i >= 10)
					Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Channel %i:  None"), i);
				else
					Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Channel %i: None"), i);
			}
		}

		if (Viewport && Viewport->Actor)
		{
			INT Factor = 8;

			AActor* ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
			if (ViewActor->Region.Zone && ViewActor->Region.Zone->bReverbZone)
			{
				AZoneInfo* zone = ViewActor->Region.Zone;
				float volume = zone->MasterGain / 255.0f;
				float hfcutoff = Clamp(zone->CutoffHz, 0, 44100);
				for (int i = 0; i < ARRAY_COUNT(zone->Delay); i++)
				{
					float delay = Clamp(zone->Delay[i] / 500.0f, 0.001f, 0.340f);
					float gain = Clamp(zone->Gain[i] / 255.0f, 0.001f, 0.999f);

					Frame->Viewport->Canvas->CurX = 10;
					Frame->Viewport->Canvas->CurY = 24 + Factor * (Channels + 1 + i);
					Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Reverb %i: Delay %05.3f Gain %05.2f"), i, delay, gain);
				}
			}
			else
			{
				Frame->Viewport->Canvas->CurX = 10;
				Frame->Viewport->Canvas->CurY = 24 + Factor * (Channels + 1);
				Frame->Viewport->Canvas->WrappedPrintf(Frame->Viewport->Canvas->SmallFont, 0, TEXT("Not in a reverb zone"));
			}
		}
	}

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
		Sound->Data.Load();
		debugf(NAME_DevSound, TEXT("Register sound: %s (%i)"), Sound->GetPathName(), Sound->Data.Num());

		try
		{
			uint8_t* ptr = (uint8_t*)Sound->Data.GetData();
			std::vector<uint8_t> data(ptr, ptr + Sound->Data.Num());

#if 0
			FILE* file = _wfopen((std::wstring(L"c:\\development\\") + Sound->GetFullName() + L".wav").c_str(), L"wb");
			fwrite(data.data() + pos, data.size() - pos, 1, file);
			fclose(file);
#endif

			// Search for smpl chunk in wav file to find its loop flags, if any:
			// (yes, this code is very ugly but obviously I don't care anymore)
			AudioLoopInfo loopinfo;
			if (data.size() > 44 && memcmp(data.data(), "RIFF", 4) == 0 && memcmp(data.data() + 8, "WAVE", 4) == 0)
			{
				size_t pos = 12;
				while (pos + 8 < data.size())
				{
					if (memcmp(data.data() + pos, "smpl", 4) == 0 && pos + 0x2c <= data.size())
					{
						uint32_t chunksize = *(uint32_t*)(data.data() + pos + 4);
						uint32_t endpos = pos + 8 + chunksize;

						uint32_t sampleLoops = *(uint32_t*)(data.data() + pos + 0x24);
						pos += 0x2c;

						if (sampleLoops != 0 && pos + 0x18 <= data.size())
						{
							uint32_t type = *(uint32_t*)(data.data() + pos + 0x04);
							uint32_t start = *(uint32_t*)(data.data() + pos + 0x08);
							uint32_t end = *(uint32_t*)(data.data() + pos + 0x0c);

							loopinfo.Looped = true;
							// if (type & 1)
							//	loopinfo.BidiLoop = true;
							loopinfo.LoopStart = start;
							loopinfo.LoopEnd = end;
						}

						pos = endpos;
					}
					else
					{
						uint32_t chunksize = *(uint32_t*)(data.data() + pos + 4);
						pos += 8 + chunksize;
					}
				}
			}

			Sound->Handle = Mixer->AddSound(AudioSource::CreateWav(std::move(data)), loopinfo);
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

	if (!Viewport || !Sound)
		return false;

	// Allocate a new slot if requested.
	if ((Id & 14) == 2 * SLOT_None)
		Id = 16 * --FreeSlot;

	FLOAT Priority = SoundPriority(Viewport, Id, Location, Volume, Radius);

	// If already playing, stop it.
	size_t Index = PlayingSounds.size();
	FLOAT BestPriority = Priority;
	for (size_t i = 0; i < PlayingSounds.size(); i++)
	{
		PlayingSound& Playing = PlayingSounds[i];
		if ((Playing.Id & ~1) == (Id & ~1))
		{
			// Skip if not interruptable.
			if (Id & 1)
				return 0;

			// Stop the sound.
			Index = i;
			break;
		}
		else if (Playing.Priority <= BestPriority)
		{
			Index = i;
			BestPriority = Playing.Priority;
		}
	}

	// If no sound, or its priority is overruled, stop it.
	if (Index == PlayingSounds.size())
		return 0;

	// Put the sound on the play-list.
	StopSound(Index);
	PlayingSounds[Index] = PlayingSound(Actor, Id, Sound, Location, Volume, Radius, Pitch, Priority);

	return true;
	unguard;
}

void UHRTFAudioSubsystem::StopSound(size_t index)
{
	guard(UHRTFAudioSubsystem::StopSound);

	PlayingSound& Playing = PlayingSounds[index];

	if (Playing.Channel)
	{
		guard(StopSample);
		Mixer->StopSound(Playing.Channel);
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
