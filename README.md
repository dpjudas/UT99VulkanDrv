# Unreal Tournament VulkanDrv
This project implements a vulkan render device for Unreal Tournament (UT99).

## Compiling the source

The project files were made for Visual Studio 2019. Open VulkanDrv.sln, select the release configuration and press build.

## Using VulkanDrv as the render device

Copy the VulkanDrv.dll and VulkanDrv.int files to the Unreal Tournament system folder.

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

## License

Please see LICENSE.md for the details.
