# Unreal Tournament VulkanDrv
This project implements a vulkan render device for Unreal Tournament (UT99).

## Compiling the source

The project files were made for Visual Studio 2022. Open VulkanDrv.sln, select the release configuration and press build.

Note: This project can no longer be built without the 469 SDK. It also requires 469c or newer to run.

## Using VulkanDrv as the render device

Copy the VulkanDrv.dll and VulkanDrv.int files to the Unreal Tournament system folder.

In the [Engine.Engine] section of UnrealTournament.ini, change GameRenderDevice to VulkanDrv.VulkanRenderDevice

Add the following section to the file:

	[VulkanDrv.VulkanRenderDevice]
	LODBias=-0.500000
	GammaOffsetBlue=0.000000
	GammaOffsetGreen=0.000000
	GammaOffsetRed=0.000000
	GammaOffset=0.000000
	GammaMode=D3D9
	UseVSync=True
	LightMode=Normal
	OccludeLines=True
	Hdr=False
	AntialiasMode=MSAA_4x
	Saturation=255
	Contrast=128
	LinearBrightness=128
	VkDebug=False
	VkDeviceIndex=0
	VkExclusiveFullscreen=False

## Description of settings

- LightMode:
  - Normal: The default rendering of the Direct3D render devices that shipped with the game
  - OneXBlending: Halve the brightness of the light maps (all map surfaces are half as bright). This effectively makes the actors brighter relative to actors
  - BrighterActors: This doubles the brightness of the actors instead of making the light maps dimmer
- GammaOffset, GammaOffsetRed, GammaOffsetGreen, GammaOfsetBlue: Add additional gamma to all pixels. GammaOffset for all color channels, the others for specific ones
- GammaMode:
  - D3D9: Use the gamma calculations from the other Direct3D render devices
  - XOpenGL: Use the gamma calculations from the XOpenGL render device
- Hdr: Overbright pixels will use the high dynamic range of the monitor. Note: this will only work if HDR is enabled in the Windows display settings and if you have a HDR capable monitor
- AntialiasMode:
  - Off: No anti alias applied
  - MSAA_2x: 2x multisampling
  - MSAA_4x: 4x multisampling
- Saturation: Saturation color control. 255 is the default. 128 is black and white. Zero inversely saturates the colors.
- Contrast: Contrast color control. 128 is the default.
- LinearBrightness: True brightness control (UT's brightness control is actually gamma control). 128 is the default.
- OccludeLines: If true, lines are occluded by geometry in the Unreal editor.
- LODBias: Adjusts the level-of-detail bias for textures. A number greater than zero will bias it towards using lower detail mipmaps. A negative number will bias it towards using higher level mipmaps
- Bloom: Adds bloom to overbright pixels
- BloomAmount: Controls how strong the bloom blur should be. 128 is the default.

## Description of VulkanDrv specific settings

- VkDebug enables the vulkan debug layer and will cause the render device to output extra information into the UnrealTournament.log file. 'VkMemStats' can also be typed into the console.
- VkExclusiveFullscreen enables vulkan's exclusive full screen feature. It is off by default as some users have reported problems with it.
- VkDeviceIndex selects which vulkan device in the system the render device should use. Type 'GetVkDevices' in the system console to get the list of available devices.

## Using D3D11Drv as the render device

This project now also contains a Direct3D 11 render device. To use it, copy the D3D11Drv.dll and D3D11Drv.int files to the Unreal Tournament system folder.

In the [Engine.Engine] section of UnrealTournament.ini, change GameRenderDevice to D3D11Drv.D3D11RenderDevice

Add the following section to the file:

	[D3D11Drv.D3D11RenderDevice]
	LODBias=-0.500000
	GammaOffsetBlue=0.000000
	GammaOffsetGreen=0.000000
	GammaOffsetRed=0.000000
	GammaOffset=0.000000
	GammaMode=D3D9
	UseVSync=True
	LightMode=Normal
	OccludeLines=True
	Hdr=False
	AntialiasMode=MSAA_4x
	Saturation=255
	Contrast=128
	LinearBrightness=128

## License

Please see LICENSE.md for the details.
