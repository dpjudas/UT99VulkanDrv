
#include "Precomp.h"
#include "UVulkanClient.h"
#include "UVulkanViewport.h"

IMPLEMENT_CLASS(UVulkanClient);

UVulkanClient::UVulkanClient()
{
	guard(UVulkanClient::UVulkanClient);
	unguard;
}

void UVulkanClient::StaticConstructor()
{
	guard(UVulkanClient::StaticConstructor);
	new(GetClass(), TEXT("StartupFullscreen"), RF_Public) UBoolProperty(CPP_PROPERTY(StartupFullscreen), TEXT("Display"), CPF_Config);
	unguard;
}

void UVulkanClient::NotifyDestroy(void* Src)
{
	guard(UVulkanClient::NotifyDestroy);
	unguard;
}

void UVulkanClient::Destroy()
{
	guard(UVulkanClient::Destroy);
	Super::Destroy();
	unguard;
}

void UVulkanClient::PostEditChange()
{
}

void UVulkanClient::ShutdownAfterError()
{
	// Kill the audio subsystem.
	if (Engine && Engine->Audio)
	{
		Engine->Audio->ConditionalShutdownAfterError();
	}

	// Release all viewports.
	for (INT i = Viewports.Num() - 1; i >= 0; i--)
	{
		UVulkanViewport* Viewport = (UVulkanViewport*)Viewports(i);
		Viewport->ConditionalShutdownAfterError();
	}

	Super::ShutdownAfterError();
}

void UVulkanClient::Init(UEngine* InEngine)
{
	guard(UVulkanClient::UVulkanClient);

	UClient::Init(InEngine);
	PostEditChange();

	if (ParseParam(appCmdLine(), TEXT("defaultres")))
	{
		WindowedViewportX = FullscreenViewportX = 800;
		WindowedViewportY = FullscreenViewportY = 600;
	}

	unguard;
}

void UVulkanClient::ShowViewportWindows(DWORD ShowFlags, INT DoShow)
{
	guard(UVulkanClient::ShowViewportWindows);
	for (int i = 0; i < Viewports.Num(); i++)
	{
		UVulkanViewport* Viewport = (UVulkanViewport*)Viewports(i);
		if ((Viewport->Actor->ShowFlags & ShowFlags) == ShowFlags)
			ShowWindow(Viewport->WindowHandle, SW_SHOW);
	}
	unguard;
}

void UVulkanClient::EnableViewportWindows(DWORD ShowFlags, INT DoEnable)
{
	guard(UVulkanClient::EnableViewportWindows);
	unguard;
}

UBOOL UVulkanClient::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UVulkanClient::Exec);
	if (UClient::Exec(Cmd, Ar))
	{
		return 1;
	}
	return 0;
	unguard;
}

void UVulkanClient::Tick()
{
	guard(UVulkanClient::Tick);

	// Tick the viewports.
	for (INT i = 0; i < Viewports.Num(); i++)
	{
		UVulkanViewport* Viewport = CastChecked<UVulkanViewport>(Viewports(i));
		Viewport->Tick();
	}

	// Blit any viewports that need blitting.
	UVulkanViewport* BestViewport = NULL;
	for (INT i = 0; i < Viewports.Num(); i++)
	{
		UVulkanViewport* Viewport = CastChecked<UVulkanViewport>(Viewports(i));
		check(!Viewport->HoldCount);
		if (!(Viewport->WindowHandle))
		{
			// Window was closed via close button.
			delete Viewport;
			return;
		}
		else if (Viewport->IsRealtime() && Viewport->SizeX && Viewport->SizeY && (!BestViewport || BestViewport->LastUpdateTime > Viewport->LastUpdateTime))
		{
			BestViewport = Viewport;
		}
	}
	if (BestViewport)
		BestViewport->Repaint(1);

	unguard;
}

void UVulkanClient::MakeCurrent(UViewport* InViewport)
{
	guard(UVulkanClient::MakeCurrent);
	for (INT i = 0; i < Viewports.Num(); i++)
	{
		UViewport* OldViewport = Viewports(i);
		if (OldViewport->Current && OldViewport != InViewport)
		{
			OldViewport->Current = 0;
			OldViewport->UpdateWindowFrame();
		}
	}
	if (InViewport)
	{
		InViewport->Current = 1;
		InViewport->UpdateWindowFrame();
	}
	unguard;
}

UViewport* UVulkanClient::NewViewport(const FName Name)
{
	guard(UVulkanClient::NewViewport);
	return new(this, Name) UVulkanViewport();
	unguard;
}
