#pragma once

class Sample
{
public:
};

class DLL_EXPORT_CLASS UHRTFAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(UHRTFAudioSubsystem, UAudioSubsystem, CLASS_Config, Audio)

public:
	UHRTFAudioSubsystem();
	void StaticConstructor();

	UBOOL Init() override;
	void CleanUp() override;

	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;

	void SetViewport(UViewport* Viewport) override;
	UViewport* GetViewport() override;

	void Update(FPointRegion Region, FCoords& Listener) override;
	UBOOL GetLowQualitySetting() override;
	void RenderAudioGeometry(FSceneNode* Frame) override;
	void PostRender(FSceneNode* Frame) override;

	void RegisterMusic(UMusic* Music) override;
	void RegisterSound(USound* Music) override;

	void UnregisterSound(USound* Sound) override;
	void UnregisterMusic(UMusic* Music) override;

	UBOOL PlaySound(AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch) override;
	void NoteDestroy(AActor* Actor) override;

	Sample* GetSound(USound* Sound)
	{
		if (!Sound->Handle)
			RegisterSound(Sound);
		return (Sample*)Sound->Handle;
	}

	static float SoundAttenuation(UViewport* Viewport, FVector Location, FLOAT Volume, FLOAT Radius)
	{
		AActor* target = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
		return std::max(Volume * (1.0f - (Location - target->Location).Size() / Radius), 0.0f);
	}

private:
	UViewport* viewport = nullptr;

	BITFIELD UseFilter = false;
	BITFIELD UseSurround = false;
	BITFIELD UseStereo = false;
	BITFIELD UseCDMusic = false;
	BITFIELD UseDigitalMusic = false;
	BITFIELD ReverseStereo = false;
	UINT Latency = 50;
	BYTE OutputRate = 6;
	UINT Channels = 2;
	BYTE MusicVolume = 100;
	BYTE SoundVolume = 100;
	FLOAT AmbientFactor = 1.0f;
	FLOAT DopplerSpeed = 1.0f;
};
