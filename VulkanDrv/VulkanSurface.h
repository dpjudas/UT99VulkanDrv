#pragma once

class VulkanInstance;

class VulkanSurface
{
public:
	VulkanSurface(std::shared_ptr<VulkanInstance> instance, HWND window);
	~VulkanSurface();

	std::shared_ptr<VulkanInstance> Instance;
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
};
