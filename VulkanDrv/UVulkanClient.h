#pragma once

class UVulkanClient : public UClient, public FNotifyHook
{
public:
	DECLARE_CLASS(UVulkanClient, UClient, CLASS_Transient | CLASS_Config, VulkanDrv)

	UVulkanClient();
	void StaticConstructor();

	// FNotifyHook interface.
	void NotifyDestroy(void* Src) override;

	// UObject interface.
	void Destroy() override;
	void PostEditChange() override;
	void ShutdownAfterError() override;

	// UClient interface.
	void Init(UEngine* InEngine) override;
	void ShowViewportWindows(DWORD ShowFlags, INT DoShow) override;
	void EnableViewportWindows(DWORD ShowFlags, INT DoEnable) override;
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar = *GLog) override;
	void Tick() override;
	void MakeCurrent(UViewport* InViewport) override;
	UViewport* NewViewport(const FName Name) override;

	// Configuration.
	BITFIELD StartupFullscreen;
	BITFIELD SlowVideoBuffering;

	// Variables.
	UBOOL InMenuLoop;
	UBOOL ConfigReturnFullscreen;
	INT NormalMouseInfo[3];
	INT CaptureMouseInfo[3];
};
