
#include "Precomp.h"
#include "HRTFAudioSubsystem.h"

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
	return true;
	unguard;
}

void UHRTFAudioSubsystem::CleanUp()
{
	guard(UHRTFAudioSubsystem::CleanUp);
	unguard;
}

UBOOL UHRTFAudioSubsystem::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UHRTFAudioSubsystem::Exec);
	return false;
	unguard;
}

void UHRTFAudioSubsystem::SetViewport(UViewport* Viewport)
{
	guard(UHRTFAudioSubsystem::SetViewport);
	viewport = Viewport;

	if (Viewport)
	{
		if (Viewport->Actor->Song && Viewport->Actor->Transition == MTRAN_None)
			Viewport->Actor->Transition = MTRAN_Instant;
	}
	unguard;
}

UViewport* UHRTFAudioSubsystem::GetViewport()
{
	return viewport;
}

void UHRTFAudioSubsystem::Update(FPointRegion Region, FCoords& Listener)
{
	guard(UHRTFAudioSubsystem::Update);
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
	unguard;
}

void UHRTFAudioSubsystem::RegisterMusic(UMusic* Music)
{
	guard(UHRTFAudioSubsystem::RegisterMusic);
	unguard;
}

void UHRTFAudioSubsystem::RegisterSound(USound* Music)
{
	guard(UHRTFAudioSubsystem::RegisterSound);
	unguard;
}

void UHRTFAudioSubsystem::UnregisterSound(USound* Sound)
{
	guard(UHRTFAudioSubsystem::UnregisterSound);
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

void UHRTFAudioSubsystem::NoteDestroy(AActor* Actor)
{
	guard(UHRTFAudioSubsystem::NoteDestroy);
	unguard;
}
