
#include "Precomp.h"
#include "UVulkanViewport.h"

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

UVulkanViewport::UVulkanViewport() : ViewportStatus(Vulkan_ViewportOpening)
{
	guard(UVulkanViewport::UVulkanViewport);

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

	if (msg == WM_INPUT)
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
