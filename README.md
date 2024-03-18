# Unreal Tournament Render Devices
This project implements Vulkan, Direct3D 12, and Direct3D 11 render devices for Unreal Tournament (UT99).

## Compiling the source

The project files were made for Visual Studio 2022. Open VulkanDrv.sln, select the release configuration and press build.

Note: This project requires the 469 SDK. It also requires 469c or newer to run.

## Using VulkanDrv, D3D11Drv or D3D12Drv as the render device

Copy the .dll and .int files files to the Unreal Tournament system folder.

In the [Engine.Engine] section of UnrealTournament.ini, change GameRenderDevice to VulkanDrv.VulkanRenderDevice, D3D12Drv.D3D12RenderDevice or D3D11Drv.D3D11RenderDevice. Then launch the game.

All the render devices supports the following renderdev specific settings in each their section of the UnrealTournament.ini file:

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

VulkanDrv specific settings:

	VkDebug=False
	VkDeviceIndex=0
	VkExclusiveFullscreen=False

D3D12Drv specific settings:

	UseDebugLayer=False

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

- VkDebug enables the vulkan debug layer and will make the render device output extra information into the UnrealTournament.log file. 'VkMemStats' can also be typed into the console.
- VkExclusiveFullscreen enables vulkan's exclusive full screen feature. It is off by default as some users have reported problems with it.
- VkDeviceIndex selects which vulkan device in the system the render device should use. Type 'GetVkDevices' in the system console to get the list of available devices.

## Description of D3D12Drv specific settings

- UseDebugLayer enables the D3D12 debug layer and will make the render device output extra information into the UnrealTournament.log file for any errors or warnings.

## License

Please see LICENSE.md for the details.
