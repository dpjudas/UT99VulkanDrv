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

VkHdr enables HDR mode for monitors supporting HDR. This causes overbright pixels to become brighter rather than saturating to white.

VkExclusiveFullscreen enables vulkan's exclusive full screen feature. It is off by default as some users have reported problems with it.

The VkDeviceIndex selects which vulkan device in the system the render device should use. Type 'GetVkDevices' in the system console to get the list of available devices.

For debugging, VkDebug can be set to true. This enables the vulkan debug layer and will cause the render device to output extra information into the UnrealTournament.log file. 'VkMemStats' can also be typed into the console.

## Brightness controls console commands:

	vk_contrast <number> - defaults to 1.0
	vk_saturation <number> - defaults to 1.0 
	vk_brightness <number> - defaults to 0.0
	vk_grayformula <number> - defaults to 1, can be 0, 1 or 2

## Debugging console commands:

	vstat memory
	vstat resources
	vstat draw
	vkmemstats

## License

Please see LICENSE.md for the details.
