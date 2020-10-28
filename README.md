# Unreal Tournament VulkanDrv
This project implements a vulkan render device for Unreal Tournament (UT99). It also includes a new window manager.

The primary features are:

- Uses vulkan to render the game
- No microstutters when used with a 144 hz monitor
- Raw input for the mouse
- Instant alt-tabbing to and from the game
- High quality backbuffer (rgba16f)

## Compiling the source

The project files were made for Visual Studio 2019. Open it, select the release configuration and press build.

## Usage

1. Copy VulkanDrv.dll and VulkanDrv.int to the UT system folder.
2. Open UnrealTournament.ini
3. In the [Engine.Engine] section, change GameRenderDevice to VulkanDrv.VulkanRenderDevice
4. In the [Engine.Engine] section, change ViewportManager to VulkanDrv.VulkanClient

Last, add the following sections to the end of the file:

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

Adjust FullscreenViewportX and FullscreenViewportY to match your monitor resolution. And that's it! Unreal Tournament should now be using vulkan.

## License

Please see LICENSE.md for the details.
