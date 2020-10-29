# Unreal Tournament VulkanDrv
This project implements a vulkan render device for Unreal Tournament (UT99). It also includes a new window manager.

## Compiling the source

The project files were made for Visual Studio 2019. Open VulkanDrv.sln, select the release configuration and press build.

## Using VulkanDrv as the render device

In the [Engine.Engine] section of UnrealTournament.ini, change GameRenderDevice to VulkanDrv.VulkanRenderDevice

Add the following section to the file:

	[VulkanDrv.VulkanRenderDevice]
	UsePrecache=False
	VSync=True
	DetailTextures=True
	DescFlags=0
	Description=
	HighDetailActors=True
	Coronas=True
	ShinySurfaces=True
	VolumetricLighting=True

## Using VulkanDrv as the window manager

Optionally, you can also replace the window manager. This makes UT always use raw input for the mouse. It also fixes the system mouse cursor sometimes appearing when you alt-tab to and from UT. Please note that this window manager is only compatible with VulkanDrv as the render device (for now).

In the [Engine.Engine] section of UnrealTournament.ini, change ViewportManager to VulkanDrv.VulkanClient

Add the following section to the file:

	[VulkanDrv.VulkanClient]
	StartupFullscreen=True
	ParticleDensity=0
	NoFractalAnim=False
	NoDynamicLights=False
	Decals=True
	MinDesiredFrameRate=200.000000
	NoLighting=False
	ScreenFlashes=False
	SkinDetail=High
	TextureDetail=High
	CurvedSurfaces=True
	CaptureMouse=True
	Brightness=0.800000
	MipFactor=1.000000
	FullscreenViewportX=1920
	FullscreenViewportY=1080
	FullscreenColorBits=32
	WindowedViewportX=1700
	WindowedViewportY=900
	WindowedColorBits=32

Adjust FullscreenViewportX and FullscreenViewportY to match your monitor resolution.

## License

Please see LICENSE.md for the details.
