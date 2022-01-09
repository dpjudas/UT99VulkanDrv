#pragma once

#include "AudioMixer.h"

struct PlayingSound
{
	PlayingSound() = default;
	PlayingSound(AActor* Actor, INT Id, USound* Sound, FVector Location, float Volume, float Radius, float Pitch, float Priority) : Actor(Actor), Id(Id), Sound(Sound), Location(Location), Volume(Volume), Radius(Radius), Pitch(Pitch), Priority(Priority) { }

	INT Id = 0;
	INT Channel = 0;
	float Priority = 0.0f;
	AActor* Actor = nullptr;
	USound* Sound = nullptr;
	FVector Location = { 0.0f, 0.0f, 0.0f };
	float Volume = 1.0f;
	float Radius = 1.0f;
	float Pitch = 1.0f;
	float CurrentVolume = 0.0f;
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

	static float SoundPriority(UViewport* Viewport, INT Id, FVector Location, FLOAT Volume, FLOAT Radius)
	{
		if ((Id & 14) == 2 * SLOT_Interface) // 469 modified how interface priority works
			return 4.0f;
		AActor* target = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
		return std::max(Volume * (1.0f - (Location - target->Location).Size() / Radius), 0.0f);
	}

private:
	void StartAmbience();
	void UpdateAmbience();
	void UpdateSounds(FCoords& Listener);
	void UpdateMusic();
	void UpdateReverb();
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
	int FreeSlot = 0x07ffffff;
	bool AudioStats = false;
};
