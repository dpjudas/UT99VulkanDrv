#pragma once

#include "UVulkanClient.h"

enum EVulkanViewportStatus
{
	Vulkan_ViewportOpening = 0, // Viewport is opening.
	Vulkan_ViewportNormal = 1, // Viewport is operating normally.
	Vulkan_ViewportClosing = 2, // Viewport is closing and CloseViewport has been called.
};

struct ReceivedWindowMessage
{
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;
};

class UVulkanViewport : public UViewport
{
public:
	DECLARE_CLASS(UVulkanViewport, UViewport, CLASS_Transient, VulkanDrv)
	DECLARE_WITHIN(UVulkanClient)

	// Constructor.
	UVulkanViewport();

	// UObject interface.
	void Destroy() override;
	void ShutdownAfterError() override;

	// UViewport interface.
	UBOOL Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData = NULL, INT* HitSize = 0) override;
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;
	UBOOL ResizeViewport(DWORD BlitFlags, INT NewX = INDEX_NONE, INT NewY = INDEX_NONE, INT NewColorBytes = INDEX_NONE) override;
	UBOOL IsFullscreen() override;
	void Unlock(UBOOL Blit) override;
	void Repaint(UBOOL Blit) override;
	void SetModeCursor() override;
	void UpdateWindowFrame() override;
	void OpenWindow(DWORD ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY) override;
	void CloseWindow() override;
	void UpdateInput(UBOOL Reset) override;
	void* GetWindow() override;
	void* GetServer() override;
	void SetMouseCapture(UBOOL Capture, UBOOL Clip, UBOOL FocusOnly) override;

	// UVulkanViewport interface.
	void ToggleFullscreen();
	void EndFullscreen();
	UBOOL CauseInputEvent(INT iKey, EInputAction Action, FLOAT Delta = 0.0);
	void TryRenderDevice(const TCHAR* ClassName, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void Tick();
	void PauseGame();
	void ResumeGame();

	LRESULT OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam);
	static LRESULT CALLBACK WndProc(HWND windowhandle, UINT msg, WPARAM wparam, LPARAM lparam);

	// Variables.
	HWND WindowHandle = 0;
	EVulkanViewportStatus ViewportStatus;
	INT HoldCount;
	DWORD BlitFlags;
	bool Paused = false;

	// Mouse.
	int MouseMoveX = 0;
	int MouseMoveY = 0;

	std::vector<ReceivedWindowMessage> ReceivedMessages;
};
