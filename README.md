# Unreal Tournament VulkanDrv
This project implements a vulkan render device for Unreal Tournament (UT99).

## Compiling the source

The project files were made for Visual Studio 2019. Open VulkanDrv.sln, select the release configuration and press build.

Note: This project can no longer be built without the 469 SDK. It also requires 469c to run.

## Using VulkanDrv as the render device

Copy the VulkanDrv.dll and VulkanDrv.int files to the Unreal Tournament system folder.

In the [Engine.Engine] section of UnrealTournament.ini, change GameRenderDevice to VulkanDrv.VulkanRenderDevice

Add the following section to the file:

	[VulkanDrv.VulkanRenderDevice]
	UsePrecache=False
	UseVSync=True
	Multisample=16
	DetailTextures=True
	DescFlags=0
	Description=
	HighDetailActors=True
	Coronas=True
	ShinySurfaces=True
	VolumetricLighting=True
	VkDebug=False
	VkDeviceIndex=0
	VkHdr=False
	VkExclusiveFullscreen=False
	LODBias=-0.5
	ActorXBlending=False
	OneXBlending=False

## Description of VulkanDrv specific settings

- VkDebug enables the vulkan debug layer and will cause the render device to output extra information into the UnrealTournament.log file. 'VkMemStats' can also be typed into the console.
- VkHdr enables HDR mode for monitors supporting HDR. This causes overbright pixels to become brighter rather than saturating to white.
- VkExclusiveFullscreen enables vulkan's exclusive full screen feature. It is off by default as some users have reported problems with it.
- VkDeviceIndex selects which vulkan device in the system the render device should use. Type 'GetVkDevices' in the system console to get the list of available devices.
- OneXBlending halves the lightmap light contribution. This makes actors appear brighter at the cost losing overbright lightmaps.
- ActorXBlending is an alternative to OneXBlending. Here it increases the brightness of actors instead to better match the lightmaps.
- LODBias Adjusts the level-of-detail bias for textures. A number greater than zero will bias it towards using lower detail mipmaps. A negative number will bias it towards using higher level mipmaps.

## Brightness controls console commands:

	vk_contrast <number> - defaults to 1.0
	vk_saturation <number> - defaults to 1.0 
	vk_brightness <number> - defaults to 0.0
	vk_grayformula <number> - defaults to 1, can be 0, 1 or 2

## Debugging console commands:

	vstat resources
	vstat draw

## Using D3D11Drv as the render device

This project now also contains a Direct3D 11 render device. To use it, copy the D3D11Drv.dll and D3D11Drv.int files to the Unreal Tournament system folder.

In the [Engine.Engine] section of UnrealTournament.ini, change GameRenderDevice to D3D11Drv.D3D11RenderDevice

Add the following section to the file:

	[D3D11Drv.D3D11RenderDevice]
	UsePrecache=False
	UseVSync=True
	Multisample=4
	DetailTextures=True
	DescFlags=0
	Description=
	HighDetailActors=True
	Coronas=True
	ShinySurfaces=True
	VolumetricLighting=True
	LODBias=-0.5
	ActorXBlending=False
	OneXBlending=False

## Description of D3D11Drv specific settings

- OneXBlending halves the lightmap light contribution. This makes actors appear brighter at the cost losing overbright lightmaps.
- ActorXBlending is an alternative to OneXBlending. Here it increases the brightness of actors instead to better match the lightmaps.
- LODBias Adjusts the level-of-detail bias for textures. A number greater than zero will bias it towards using lower detail mipmaps. A negative number will bias it towards using higher level mipmaps.

## Brightness controls console commands:

	d3d_contrast <number> - defaults to 1.0
	d3d_saturation <number> - defaults to 1.0 
	d3d_brightness <number> - defaults to 0.0
	d3d_grayformula <number> - defaults to 1, can be 0, 1 or 2

## License

Please see LICENSE.md for the details.
