#pragma once

#include "AudioMixer.h"

struct PlayingSound
{
	INT Id = 0;
	INT Channel = 0;
	AActor* Actor = nullptr;
	USound* Sound = nullptr;
	float Volume = 1.0f;
	float Radius = 1.0f;
	float Pitch = 1.0f;
};

class DLL_EXPORT_CLASS UHRTFAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(UHRTFAudioSubsystem, UAudioSubsystem, CLASS_Config, Audio)

public:
	UHRTFAudioSubsystem();
	void StaticConstructor();

	UBOOL Init() override;
	void CleanUp() override;
	void PostEditChange() override;

	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;

	void SetViewport(UViewport* Viewport) override;
	UViewport* GetViewport() override;

	void Update(FPointRegion Region, FCoords& Listener) override;
	UBOOL GetLowQualitySetting() override;
	void RenderAudioGeometry(FSceneNode* Frame) override;
	void PostRender(FSceneNode* Frame) override;

	void RegisterMusic(UMusic* Music) override;
	void RegisterSound(USound* Sound) override;

	void UnregisterSound(USound* Sound) override;
	void UnregisterMusic(UMusic* Music) override;

	UBOOL PlaySound(AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch) override;
	void NoteDestroy(AActor* Actor) override;

	AudioSound* GetSound(USound* Sound)
	{
		if (!Sound->Handle)
			RegisterSound(Sound);
		return (AudioSound*)Sound->Handle;
	}

	static float SoundPriority(UViewport* Viewport, FVector Location, FLOAT Volume, FLOAT Radius)
	{
		AActor* target = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
		return std::max(Volume * (1.0f - (Location - target->Location).Size() / Radius), 0.0f);
	}

private:
	void StopSound(size_t index);

	UViewport* Viewport = nullptr;

	BITFIELD UseFilter;
	BITFIELD UseSurround;
	BITFIELD UseStereo;
	BITFIELD UseCDMusic;
	BITFIELD UseDigitalMusic;
	BITFIELD ReverseStereo;
	INT Latency;
	BYTE OutputRate;
	INT Channels;
	BYTE MusicVolume;
	BYTE SoundVolume;
	FLOAT AmbientFactor;
	FLOAT DopplerSpeed;

	std::unique_ptr<AudioMixer> Mixer;
	std::vector<PlayingSound> PlayingSounds;
	UMusic* CurrentSong = nullptr;
	int CurrentSection = 255;
};
