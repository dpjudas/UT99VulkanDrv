
#include "Precomp.h"
#include "UVulkanViewport.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanObjects.h"
#include "VulkanBuilders.h"
#include "VulkanPostprocess.h"
#include "VulkanTexture.h"
#include "SceneBuffers.h"
#include "SceneRenderPass.h"
#include "SceneSamplers.h"
#include "FileResource.h"

IMPLEMENT_CLASS(UVulkanViewport);

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC		((USHORT) 0x01)
#endif

#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE	((USHORT) 0x02)
#endif

#ifndef HID_USAGE_GENERIC_JOYSTICK
#define HID_USAGE_GENERIC_JOYSTICK	((USHORT) 0x04)
#endif

#ifndef HID_USAGE_GENERIC_GAMEPAD
#define HID_USAGE_GENERIC_GAMEPAD	((USHORT) 0x05)
#endif

#ifndef RIDEV_INPUTSINK
#define RIDEV_INPUTSINK	(0x100)
#endif

static bool shaderbuilderinited = false;

UVulkanViewport::UVulkanViewport() : ViewportStatus(Vulkan_ViewportOpening)
{
	guard(UVulkanViewport::UVulkanViewport);

	if (!shaderbuilderinited)
	{
		ShaderBuilder::init();
		shaderbuilderinited = true;
	}

	WNDCLASSEX classdesc = {};
	classdesc.cbSize = sizeof(WNDCLASSEX);
	classdesc.hInstance = GetModuleHandle(0);
	classdesc.style = CS_VREDRAW | CS_HREDRAW;
	classdesc.lpszClassName = L"UVulkanViewport";
	classdesc.lpfnWndProc = &UVulkanViewport::WndProc;
	RegisterClassEx(&classdesc);

	CreateWindowEx(WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW, L"UVulkanViewport", L"Unreal Tournament", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, 0, 0, GetModuleHandle(0), this);

	RAWINPUTDEVICE rid;
	rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid.usUsage = HID_USAGE_GENERIC_MOUSE;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = WindowHandle;
	BOOL result = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

	ColorBytes = 4;

	unguard;
}

void UVulkanViewport::Destroy()
{
	guard(UVulkanViewport::Destroy);

	FreeResources();

	if (WindowHandle)
	{
		DestroyWindow(WindowHandle);
		WindowHandle = 0;
	}

	Super::Destroy();
	unguard;
}

void UVulkanViewport::ShutdownAfterError()
{
	FreeResources();

	if (WindowHandle)
	{
		DestroyWindow(WindowHandle);
		WindowHandle = 0;
	}

	Super::ShutdownAfterError();
}

UBOOL UVulkanViewport::Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize)
{
	guard(UVulkanViewport::LockWindow);
	UVulkanClient* Client = GetOuterUVulkanClient();
	clock(Client->DrawCycles);
	unclock(Client->DrawCycles);
	return UViewport::Lock(FlashScale, FlashFog, ScreenClear, RenderLockFlags, HitData, HitSize);
	unguard;
}

UBOOL UVulkanViewport::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UVulkanViewport::Exec);
	if (UViewport::Exec(Cmd, Ar))
	{
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("EndFullscreen")))
	{
		EndFullscreen();
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("ToggleFullscreen")))
	{
		ToggleFullscreen();
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("Iconify")))
	{
		ShowWindow(WindowHandle, SW_MINIMIZE);
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetCurrentRes")))
	{
		Ar.Logf(TEXT("%ix%i"), SizeX, SizeY, (ColorBytes ? ColorBytes : 2) * 8);
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetCurrentColorDepth")))
	{
		Ar.Logf(TEXT("%i"), (ColorBytes ? ColorBytes : 2) * 8);
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetColorDepths")))
	{
		Ar.Log(TEXT("16 32"));
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetCurrentRenderDevice")))
	{
		Ar.Log(RenDev->GetClass()->GetPathName());
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("SetRenderDevice")))
	{
		FString Saved = RenDev->GetClass()->GetPathName();
		INT SavedSizeX = SizeX, SavedSizeY = SizeY, SavedColorBytes = ColorBytes, SavedFullscreen = ((BlitFlags & BLIT_Fullscreen) != 0);
		TryRenderDevice(Cmd, SizeX, SizeY, ColorBytes, SavedFullscreen);
		if (!RenDev)
		{
			TryRenderDevice(*Saved, SavedSizeX, SavedSizeY, SavedColorBytes, SavedFullscreen);
			check(RenDev);
			Ar.Log(TEXT("0"));
		}
		else Ar.Log(TEXT("1"));
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetRes")))
	{
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("SetRes")))
	{
		INT X = appAtoi(Cmd);
		TCHAR* CmdTemp = (TCHAR*)(appStrchr(Cmd, 'x') ? appStrchr(Cmd, 'x') + 1 : appStrchr(Cmd, 'X') ? appStrchr(Cmd, 'X') + 1 : TEXT(""));
		INT Y = appAtoi(CmdTemp);
		Cmd = CmdTemp;
		CmdTemp = (TCHAR*)(appStrchr(Cmd, 'x') ? appStrchr(Cmd, 'x') + 1 : appStrchr(Cmd, 'X') ? appStrchr(Cmd, 'X') + 1 : TEXT(""));
		INT C = appAtoi(CmdTemp);
		INT NewColorBytes = C ? C / 8 : ColorBytes;
		if (X && Y)
		{
			HoldCount++;
			UBOOL Result = RenDev->SetRes(X, Y, NewColorBytes, IsFullscreen());
			HoldCount--;
			if (!Result)
				EndFullscreen();
		}
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("Preferences")))
	{
		// No preferences window.

		return 1;
	}
	else return 0;
	unguard;
}

UBOOL UVulkanViewport::ResizeViewport(DWORD NewBlitFlags, INT InNewX, INT InNewY, INT InNewColorBytes)
{
	guard(UVulkanViewport::ResizeViewport);
	UVulkanClient* Client = GetOuterUVulkanClient();

	debugf(TEXT("Resizing X viewport. X: %i Y: %i"), InNewX, InNewY);

	// Remember viewport.
	UViewport* SavedViewport = NULL;
	if (Client->Engine->Audio && !GIsEditor && !(GetFlags() & RF_Destroyed))
		SavedViewport = Client->Engine->Audio->GetViewport();

	// Accept default parameters.
	INT NewX = InNewX == INDEX_NONE ? SizeX : InNewX;
	INT NewY = InNewY == INDEX_NONE ? SizeY : InNewY;
	INT NewColorBytes = InNewColorBytes == INDEX_NONE ? ColorBytes : InNewColorBytes;

	// Default resolution handling.
	NewX = InNewX != INDEX_NONE ? InNewX : (NewBlitFlags & BLIT_Fullscreen) ? Client->FullscreenViewportX : Client->WindowedViewportX;
	NewY = InNewX != INDEX_NONE ? InNewY : (NewBlitFlags & BLIT_Fullscreen) ? Client->FullscreenViewportY : Client->WindowedViewportY;

	if (NewBlitFlags & BLIT_Fullscreen)
	{
		// Changing to fullscreen.
		bool visible = IsWindowVisible(WindowHandle);
		SetWindowLongPtr(WindowHandle, GWL_EXSTYLE, WS_EX_APPWINDOW);
		SetWindowLongPtr(WindowHandle, GWL_STYLE, WS_OVERLAPPED | (GetWindowLongPtr(WindowHandle, GWL_STYLE) & WS_VISIBLE));
		SetWindowPos(WindowHandle, 0, 0, 0, NewX, NewY, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

		// Grab mouse and keyboard.
		SetMouseCapture(1, 1, 0);
	}
	else
	{
		// Changing to windowed mode.
		SetWindowLongPtr(WindowHandle, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW);
		SetWindowLongPtr(WindowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW | (GetWindowLongPtr(WindowHandle, GWL_STYLE) & WS_VISIBLE));
		SetWindowPos(WindowHandle, 0, 0, 0, NewX, NewY, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

		// End mouse and keyboard grab.
		SetMouseCapture(0, 0, 0);
	}

	// Update audio.
	if (SavedViewport && SavedViewport != Client->Engine->Audio->GetViewport())
		Client->Engine->Audio->SetViewport(SavedViewport);

	// Update the window.
	UpdateWindowFrame();

	// Set new info.
	DWORD OldBlitFlags = BlitFlags;
	BlitFlags = NewBlitFlags & ~BLIT_ParameterFlags;
	SizeX = NewX;
	SizeY = NewY;
	ColorBytes = NewColorBytes ? NewColorBytes : ColorBytes;

	// Save info.
	if (RenDev && !GIsEditor)
	{
		if (NewBlitFlags & BLIT_Fullscreen)
		{
			if (NewX && NewY)
			{
				Client->FullscreenViewportX = NewX;
				Client->FullscreenViewportY = NewY;
			}
		}
		else
		{
			if (NewX && NewY)
			{
				Client->WindowedViewportX = NewX;
				Client->WindowedViewportY = NewY;
			}
		}
		Client->SaveConfig();
	}
	return 1;
	unguard;
}

UBOOL UVulkanViewport::IsFullscreen()
{
	guard(UVulkanViewport::IsFullscreen);
	return (BlitFlags & BLIT_Fullscreen) != 0;
	unguard;
}

void UVulkanViewport::Unlock(UBOOL Blit)
{
	guard(UVulkanViewport::Unlock);
	UViewport::Unlock(Blit);
	unguard;
}

void UVulkanViewport::Repaint(UBOOL Blit)
{
	guard(UVulkanViewport::Repaint);
	GetOuterUVulkanClient()->Engine->Draw(this, Blit);
	unguard;
}

void UVulkanViewport::SetModeCursor()
{
	guard(UVulkanViewport::SetModeCursor);
	// Set the mouse cursor according to Unreal or UnrealEd's mode, or to an hourglass if a slow task is active.
	unguard;
}

void UVulkanViewport::UpdateWindowFrame()
{
	guard(UVulkanViewport::UpdateWindowFrame);

	// If not a window, exit.
	if (HoldCount || !WindowHandle || (BlitFlags & BLIT_Fullscreen) || (BlitFlags & BLIT_Temporary))
		return;

	unguard;
}

void UVulkanViewport::OpenWindow(DWORD ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY)
{
	guard(UVulkanViewport::OpenWindow);
	check(Actor);
	check(!HoldCount);

	UVulkanClient* C = GetOuterUVulkanClient();

	if (!WindowHandle)
		return;

	debugf(TEXT("Opening vulkan viewport."));

	// Create or update the window.
	SizeX = C->FullscreenViewportX;
	SizeY = C->FullscreenViewportY;

	// Create rendering device.
	if (!RenDev && !GIsEditor && !ParseParam(appCmdLine(), TEXT("nohard")))
		TryRenderDevice(TEXT("ini:Engine.Engine.GameRenderDevice"), NewX, NewY, ColorBytes, C->StartupFullscreen);
	check(RenDev);

	if (!IsWindowVisible(WindowHandle))
	{
		ShowWindow(WindowHandle, SW_SHOW);
		SetActiveWindow(WindowHandle);
	}

	UpdateWindowFrame();
	Repaint(1);

	unguard;
}

void UVulkanViewport::CloseWindow()
{
	guard(UVulkanViewport::CloseWindow);
	if (WindowHandle && ViewportStatus == Vulkan_ViewportNormal)
	{
		ViewportStatus = Vulkan_ViewportClosing;
		DestroyWindow(WindowHandle);
		WindowHandle = 0;
	}
	unguard;
}

void UVulkanViewport::UpdateInput(UBOOL Reset)
{
	guard(UVulkanViewport::UpdateInput);
	unguard;
}

void* UVulkanViewport::GetWindow()
{
	guard(UVulkanViewport::GetWindow);
	return (void*)WindowHandle;
	unguard;
}

void* UVulkanViewport::GetServer()
{
	guard(UVulkanViewport::GetServer);
	return nullptr;
	unguard;
}

void UVulkanViewport::SetMouseCapture(UBOOL Capture, UBOOL Clip, UBOOL FocusOnly)
{
	guard(UVulkanViewport::SetMouseCapture);

	bWindowsMouseAvailable = !Capture;

	if (FocusOnly)
		return;

	unguard;
}

void UVulkanViewport::ToggleFullscreen()
{
	guard(UVulkanViewport::ToggleFullscreen);
	if (BlitFlags & BLIT_Fullscreen)
	{
		EndFullscreen();
	}
	else if (!(Actor->ShowFlags & SHOW_ChildWindow))
	{
		TryRenderDevice(TEXT("ini:Engine.Engine.GameRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1);
		if (!RenDev)
			TryRenderDevice(TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1);
		if (!RenDev)
			TryRenderDevice(TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 0);
	}
	unguard;
}

void UVulkanViewport::EndFullscreen()
{
	guard(UVulkanViewport::EndFullscreen);
	UVulkanClient* Client = GetOuterUVulkanClient();
	if (RenDev && RenDev->FullscreenOnly)
	{
		// This device doesn't support fullscreen, so use a window-capable rendering device.
		TryRenderDevice(TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 0);
		check(RenDev);
	}
	else if (RenDev && (BlitFlags & BLIT_OpenGL))
	{
		RenDev->SetRes(INDEX_NONE, INDEX_NONE, ColorBytes, 0);
	}
	else
	{
		ResizeViewport(BLIT_DibSection);
	}
	UpdateWindowFrame();
	unguard;
}

UBOOL UVulkanViewport::CauseInputEvent(INT iKey, EInputAction Action, FLOAT Delta)
{
	guard(UVulkanViewport::CauseInputEvent);

	// Route to engine if a valid key; some keyboards produce key codes that go beyond IK_MAX.
	if (iKey >= 0 && iKey < IK_MAX)
		return GetOuterUVulkanClient()->Engine->InputEvent(this, (EInputKey)iKey, Action, Delta);
	else
		return 0;

	unguard;
}

void UVulkanViewport::TryRenderDevice(const TCHAR* ClassName, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UVulkanViewport::TryRenderDevice);

	// Shut down current rendering device.
	if (RenDev)
	{
		RenDev->Exit();
		delete RenDev;
		RenDev = NULL;
	}

	// Use appropriate defaults.
	UVulkanClient* C = GetOuterUVulkanClient();
	if (NewX == INDEX_NONE)
		NewX = Fullscreen ? C->FullscreenViewportX : C->WindowedViewportX;
	if (NewY == INDEX_NONE)
		NewY = Fullscreen ? C->FullscreenViewportY : C->WindowedViewportY;

	// Find device driver.
	UClass* RenderClass = UObject::StaticLoadClass(URenderDevice::StaticClass(), NULL, ClassName, NULL, 0, NULL);
	if (RenderClass)
	{
		HoldCount++;
		RenDev = ConstructObject<URenderDevice>(RenderClass, this);
		if (RenDev->Init(this, NewX, NewY, NewColorBytes, Fullscreen))
		{
			if (GIsRunning)
				Actor->GetLevel()->DetailChange(RenDev->HighDetailActors);
		}
		else
		{
			delete RenDev;
			RenDev = NULL;
		}
		HoldCount--;
	}
	GRenderDevice = RenDev;
	unguard;
}

void UVulkanViewport::Tick()
{
	guard(UVulkanViewport::Tick);
	UVulkanClient* Client = GetOuterUVulkanClient();

	if (!WindowHandle)
		return;

	MSG msg;
	while (WindowHandle && PeekMessage(&msg, WindowHandle, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	std::vector<ReceivedWindowMessage> messages;
	messages.swap(ReceivedMessages);

	for (const ReceivedWindowMessage& e : messages)
	{
		switch (e.msg)
		{
		case WM_CREATE:
			// Window has been created.
			ViewportStatus = Vulkan_ViewportNormal;

			// Make this viewport current and update its title bar.
			GetOuterUClient()->MakeCurrent(this);
			break;
		case WM_DESTROY:
			// Window has been destroyed.
			if (BlitFlags & BLIT_Fullscreen)
				EndFullscreen();

			if (ViewportStatus == Vulkan_ViewportNormal)
			{
				// Closed normally.
				ViewportStatus = Vulkan_ViewportClosing;
				delete this;
			}
			break;
		case WM_CHAR:
			if (e.wparam >= 0 && e.wparam < IK_MAX)
				Client->Engine->Key(this, (EInputKey)e.wparam);
			break;
		case WM_KEYDOWN:
			CauseInputEvent((EInputKey)e.wparam, IST_Press);
			break;
		case WM_KEYUP:
			CauseInputEvent((EInputKey)e.wparam, IST_Release);
			break;
		case WM_LBUTTONDOWN:
			CauseInputEvent(IK_LeftMouse, IST_Press);
			break;
		case WM_MBUTTONDOWN:
			CauseInputEvent(IK_MiddleMouse, IST_Press);
			break;
		case WM_RBUTTONDOWN:
			CauseInputEvent(IK_RightMouse, IST_Press);
			break;
		case WM_LBUTTONUP:
			CauseInputEvent(IK_LeftMouse, IST_Release);
			break;
		case WM_MBUTTONUP:
			CauseInputEvent(IK_MiddleMouse, IST_Release);
			break;
		case WM_RBUTTONUP:
			CauseInputEvent(IK_RightMouse, IST_Release);
			break;
		case WM_MOUSEWHEEL:
			if (GET_WHEEL_DELTA_WPARAM(msg.wParam) != 0)
			{
				EInputKey key = GET_WHEEL_DELTA_WPARAM(msg.wParam) > 0 ? IK_MouseWheelUp : IK_MouseWheelDown;
				CauseInputEvent(key, IST_Press);
				CauseInputEvent(key, IST_Release);
			}
			break;
		case WM_SETFOCUS:
			ResumeGame();
			break;
		case WM_KILLFOCUS:
			PauseGame();
			break;
		}
	}

	if (Paused)
	{
		MouseMoveX = 0;
		MouseMoveY = 0;
		return;
	}

	// Deliver mouse behavior to the engine.
	if (MouseMoveX != 0 || MouseMoveY != 0)
	{
		int DX = MouseMoveX;
		int DY = MouseMoveY;
		MouseMoveX = 0;
		MouseMoveY = 0;

		// Send to input subsystem.
		if (DX)
			CauseInputEvent(IK_MouseX, IST_Axis, +DX);
		if (DY)
			CauseInputEvent(IK_MouseY, IST_Axis, -DY);
	}

	unguard;
}

void UVulkanViewport::PauseGame()
{
	guard(UVulkanViewport::PauseGame);

	if (Paused)
		return;

	Paused = true;

	// Pause the game if applicable.
	if (GIsRunning)
		Exec(TEXT("SETPAUSE 1"), *this);

	// Release the mouse.
	SetMouseCapture(0, 0, 0);
	SetDrag(0);

	// Reset the input buffer.
	Input->ResetInput();

	// End fullscreen.
	//if (BlitFlags & BLIT_Fullscreen)
	//	EndFullscreen();
	//GetOuterUClient()->MakeCurrent(NULL);

	unguard;
}

void UVulkanViewport::ResumeGame()
{
	guard(UVulkanViewport::PauseGame);

	if (!Paused)
		return;

	Paused = false;

	// Unpause the game if applicable.
	Exec(TEXT("SETPAUSE 0"), *this);

	// Reset the input buffer.
	Input->ResetInput();

	// Snag the mouse again.
	SetMouseCapture(1, 1, 0);

	// Make this viewport current.
	//GetOuterUClient()->MakeCurrent(this);

	// Return to fullscreen.
	//if (BlitFlags & BLIT_Fullscreen)
	//	TryRenderDevice(TEXT("ini:Engine.Engine.GameRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1);

	unguard;
}

LRESULT UVulkanViewport::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_CREATE ||
		msg == WM_DESTROY ||
		msg == WM_SHOWWINDOW ||
		msg == WM_KILLFOCUS ||
		msg == WM_LBUTTONDOWN ||
		msg == WM_LBUTTONUP ||
		msg == WM_MBUTTONDOWN ||
		msg == WM_MBUTTONUP ||
		msg == WM_RBUTTONDOWN ||
		msg == WM_RBUTTONUP ||
		msg == WM_MOUSEWHEEL ||
		msg == WM_CHAR ||
		msg == WM_KEYDOWN ||
		msg == WM_KEYUP ||
		msg == WM_SETFOCUS ||
		msg == WM_KILLFOCUS)
	{
		ReceivedMessages.push_back({ msg, wparam, lparam });
	}

	if (msg == WM_CREATE)
	{
		Device = new VulkanDevice(WindowHandle);
		SwapChain = new VulkanSwapChain(Device, true);
		ImageAvailableSemaphore = new VulkanSemaphore(Device);
		RenderFinishedSemaphore = new VulkanSemaphore(Device);
		RenderFinishedFence = new VulkanFence(Device);
		TransferSemaphore = new VulkanSemaphore(Device);
		CommandPool = new VulkanCommandPool(Device, Device->graphicsFamily);
		FrameDeleteList = new DeleteList();
		SceneSamplers = new ::SceneSamplers(Device);
		CreateSceneVertexBuffer();
		CreateSceneDescriptorSetLayout();
		CreateScenePipelineLayout();
		CreateNullTexture();
		PostprocessModel = new ::Postprocess();
		Postprocess = new VulkanPostprocess(this);
	}
	else if (msg == WM_DESTROY)
	{
		FreeResources();
	}
	else if (msg == WM_INPUT)
	{
		HRAWINPUT handle = (HRAWINPUT)lparam;
		UINT size = 0;
		UINT result = GetRawInputData(handle, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
		if (result == 0 && size > 0)
		{
			std::vector<uint8_t*> buffer(size);
			result = GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER));
			if (result >= 0)
			{
				RAWINPUT* rawinput = (RAWINPUT*)buffer.data();
				if (rawinput->header.dwType == RIM_TYPEMOUSE)
				{
					MouseMoveX += rawinput->data.mouse.lLastX;
					MouseMoveY += rawinput->data.mouse.lLastY;
				}
			}
		}
		return DefWindowProc(WindowHandle, msg, wparam, lparam);
	}
	else if (msg == WM_SETFOCUS)
	{
		ShowCursor(FALSE);
	}
	else if (msg == WM_KILLFOCUS)
	{
		ShowCursor(TRUE);
	}

	return DefWindowProc(WindowHandle, msg, wparam, lparam);
}

void UVulkanViewport::FreeResources()
{
	if (Device) vkDeviceWaitIdle(Device->device);
	DeleteFrameObjects();
	if (SceneVertices) { SceneVertexBuffer->Unmap(); SceneVertices = nullptr; }
	delete SceneVertexBuffer; SceneVertexBuffer = nullptr; SceneVertices = nullptr; SceneVertexPos = 0;
	delete NullTextureView; NullTextureView = nullptr;
	delete NullTexture; NullTexture = nullptr;
	delete SceneSamplers; SceneSamplers = nullptr;
	delete Postprocess; Postprocess = nullptr;
	delete PostprocessModel; PostprocessModel = nullptr;
	delete SceneRenderPass; SceneRenderPass = nullptr;
	delete SceneBuffers; SceneBuffers = nullptr;
	delete SceneDescriptorSetLayout; SceneDescriptorSetLayout = nullptr;
	delete ScenePipelineLayout; ScenePipelineLayout = nullptr;
	ClearTextureCache();
	delete ImageAvailableSemaphore; ImageAvailableSemaphore = nullptr;
	delete RenderFinishedSemaphore; RenderFinishedSemaphore = nullptr;
	delete RenderFinishedFence; RenderFinishedFence = nullptr;
	delete TransferSemaphore; TransferSemaphore = nullptr;
	delete CommandPool; CommandPool = nullptr;
	delete SwapChain; SwapChain = nullptr;
	delete Device; Device = nullptr;
}

LRESULT UVulkanViewport::WndProc(HWND windowhandle, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_CREATE)
	{
		CREATESTRUCT* createstruct = (CREATESTRUCT*)lparam;
		UVulkanViewport* viewport = (UVulkanViewport*)createstruct->lpCreateParams;
		viewport->WindowHandle = windowhandle;
		SetWindowLongPtr(windowhandle, GWL_USERDATA, (LONG_PTR)viewport);
		return viewport->OnWindowMessage(msg, wparam, lparam);
	}
	else
	{
		UVulkanViewport* viewport = (UVulkanViewport*)GetWindowLongPtr(windowhandle, GWL_USERDATA);
		if (viewport)
		{
			LRESULT result = viewport->OnWindowMessage(msg, wparam, lparam);
			if (msg == WM_DESTROY)
			{
				SetWindowLongPtr(windowhandle, GWL_USERDATA, 0);
				viewport->WindowHandle = 0;
			}
			return result;
		}
		else
		{
			return DefWindowProc(windowhandle, msg, wparam, lparam);
		}
	}
}

void UVulkanViewport::SubmitCommands(bool present)
{
	if (present)
	{
		RECT clientbox = {};
		GetClientRect(WindowHandle, &clientbox);

		PresentImageIndex = SwapChain->acquireImage(clientbox.right, clientbox.bottom, ImageAvailableSemaphore);
		if (PresentImageIndex != 0xffffffff)
		{
			PPViewport box;
			box.x = 0;
			box.y = 0;
			box.width = clientbox.right;
			box.height = clientbox.bottom;
			Postprocess->drawPresentTexture(box);
		}
	}

	if (TransferCommands)
	{
		TransferCommands->end();

		QueueSubmit submit;
		submit.addCommandBuffer(TransferCommands);
		submit.addSignal(TransferSemaphore);
		submit.execute(Device, Device->graphicsQueue);
	}

	if (DrawCommands)
		DrawCommands->end();

	QueueSubmit submit;
	if (DrawCommands)
	{
		submit.addCommandBuffer(DrawCommands);
	}
	if (TransferCommands)
	{
		submit.addWait(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TransferSemaphore);
	}
	if (present && PresentImageIndex != 0xffffffff)
	{
		submit.addWait(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ImageAvailableSemaphore);
		submit.addSignal(RenderFinishedSemaphore);
	}
	submit.execute(Device, Device->graphicsQueue, RenderFinishedFence);

	if (present && PresentImageIndex != 0xffffffff)
	{
		SwapChain->queuePresent(PresentImageIndex, RenderFinishedSemaphore);
	}

	vkWaitForFences(Device->device, 1, &RenderFinishedFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(Device->device, 1, &RenderFinishedFence->fence);

	delete DrawCommands; DrawCommands = nullptr;
	delete TransferCommands; TransferCommands = nullptr;
	DeleteFrameObjects();
}

VulkanCommandBuffer* UVulkanViewport::GetTransferCommands()
{
	if (!TransferCommands)
	{
		TransferCommands = CommandPool->createBuffer().release();
		TransferCommands->begin();
	}
	return TransferCommands;
}

VulkanCommandBuffer* UVulkanViewport::GetDrawCommands()
{
	if (!DrawCommands)
	{
		DrawCommands = CommandPool->createBuffer().release();
		DrawCommands->begin();
	}
	return DrawCommands;
}

void UVulkanViewport::DeleteFrameObjects()
{
	delete FrameDeleteList; FrameDeleteList = nullptr;
	FrameDeleteList = new DeleteList();
}

std::unique_ptr<VulkanShader> UVulkanViewport::CreateVertexShader(VulkanDevice* device, const std::string& name, const std::string& defines)
{
	ShaderBuilder builder;
	builder.setVertexShader(LoadShaderCode(name, defines));
	return builder.create(device);
}

std::unique_ptr<VulkanShader> UVulkanViewport::CreateFragmentShader(VulkanDevice* device, const std::string& name, const std::string& defines)
{
	ShaderBuilder builder;
	builder.setFragmentShader(LoadShaderCode(name, defines));
	return builder.create(device);
}

std::unique_ptr<VulkanShader> UVulkanViewport::CreateComputeShader(VulkanDevice* device, const std::string& name, const std::string& defines)
{
	ShaderBuilder builder;
	builder.setComputeShader(LoadShaderCode(name, defines));
	return builder.create(device);
}

std::string UVulkanViewport::LoadShaderCode(const std::string& filename, const std::string& defines)
{
	const char* shaderversion = R"(
		#version 450
		#extension GL_ARB_separate_shader_objects : enable
	)";
	return shaderversion + defines + "\r\n#line 1\r\n" + FileResource::readAllText(filename);
}

void UVulkanViewport::CreateScenePipelineLayout()
{
	PipelineLayoutBuilder builder;
	builder.addSetLayout(SceneDescriptorSetLayout);
	builder.addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants));
	ScenePipelineLayout = builder.create(Device).release();
}

void UVulkanViewport::CreateSceneDescriptorSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	//builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	SceneDescriptorSetLayout = builder.create(Device).release();
}

void UVulkanViewport::CreateSceneVertexBuffer()
{
	size_t size = sizeof(SceneVertex) * 1'000'000;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
	builder.setMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	builder.setSize(size);

	SceneVertexBuffer = builder.create(Device).release();
	SceneVertices = (SceneVertex*)SceneVertexBuffer->Map(0, size);
	SceneVertexPos = 0;
}

void UVulkanViewport::CreateNullTexture()
{
	auto cmdbuffer = GetTransferCommands();

	ImageBuilder imgbuilder;
	imgbuilder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
	imgbuilder.setSize(1, 1);
	imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	NullTexture = imgbuilder.create(Device).release();

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(NullTexture, VK_FORMAT_R8G8B8A8_UNORM);
	NullTextureView = viewbuilder.create(Device).release();

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(4);
	auto stagingbuffer = builder.create(Device);
	auto data = (uint32_t*)stagingbuffer->Map(0, 4);
	data[0] = 0xffffffff;
	stagingbuffer->Unmap();

	PipelineBarrier imageTransition0;
	imageTransition0.addImage(NullTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
	imageTransition0.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { 1, 1, 1 };
	cmdbuffer->copyBufferToImage(stagingbuffer->buffer, NullTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	PipelineBarrier imageTransition1;
	imageTransition1.addImage(NullTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	imageTransition1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	FrameDeleteList->buffers.push_back(std::move(stagingbuffer));
}

VulkanTexture* UVulkanViewport::GetTexture(FTextureInfo* texture, DWORD polyFlags)
{
	if (!texture)
		return nullptr;

	VulkanTexture*& tex = TextureCache[texture->CacheID];
	if (!tex)
	{
		tex = new VulkanTexture(this, *texture, polyFlags);
	}
	else if (texture->bRealtimeChanged)
	{
		texture->bRealtimeChanged = 0;
		tex->Update(this, *texture, polyFlags);
	}
	return tex;
}

VulkanDescriptorSet* UVulkanViewport::GetTextureDescriptorSet(VulkanTexture* tex, VulkanTexture* lightmap)
{
	auto& descriptorSet = TextureDescriptorSets[{ tex, lightmap }];
	if (!descriptorSet)
	{
		if (SceneDescriptorPoolSetsLeft == 0)
		{
			DescriptorPoolBuilder builder;
			builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 * 2);
			builder.setMaxSets(1000);
			SceneDescriptorPool.push_back(builder.create(Device).release());
			SceneDescriptorPoolSetsLeft = 1000;
		}

		descriptorSet = SceneDescriptorPool.back()->allocate(SceneDescriptorSetLayout).release();
		SceneDescriptorPoolSetsLeft--;

		WriteDescriptors writes;
		int i = 0;
		for (VulkanTexture* texture : { tex, lightmap })
		{
			if (texture)
				writes.addCombinedImageSampler(descriptorSet, i++, texture->imageView.get(), SceneSamplers->repeat.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			else
				writes.addCombinedImageSampler(descriptorSet, i++, NullTextureView, SceneSamplers->repeat.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		writes.updateSets(Device);
	}
	return descriptorSet;
}

void UVulkanViewport::ClearTextureCache()
{
	for (auto it : TextureDescriptorSets)
		delete it.second;
	TextureDescriptorSets.clear();

	for (auto pool : SceneDescriptorPool)
		delete pool;
	SceneDescriptorPool.clear();
	SceneDescriptorPoolSetsLeft = 0;

	for (auto it : TextureCache)
		delete it.second;
	TextureCache.clear();
}
