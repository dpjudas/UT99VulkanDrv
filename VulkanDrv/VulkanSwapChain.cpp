
#include "Precomp.h"
#include "VulkanSwapChain.h"
#include "VulkanObjects.h"

VulkanSwapChain::VulkanSwapChain(VulkanDevice *device, bool vsync) : vsync(vsync), device(device)
{
}

VulkanSwapChain::~VulkanSwapChain()
{
	releaseResources();
}

uint32_t VulkanSwapChain::acquireImage(int width, int height, VulkanSemaphore *semaphore, VulkanFence *fence)
{
	if (lastSwapWidth != width || lastSwapHeight != height || !swapChain)
	{
		recreate(width, height);
		lastSwapWidth = width;
		lastSwapHeight = height;
	}

	uint32_t imageIndex;
	while (true)
	{
		if (!swapChain)
		{
			imageIndex = 0xffffffff;
			break;
		}

		VkResult result = vkAcquireNextImageKHR(device->device, swapChain, 1'000'000'000, semaphore ? semaphore->semaphore : VK_NULL_HANDLE, fence ? fence->fence : VK_NULL_HANDLE, &imageIndex);
		if (result == VK_SUCCESS)
		{
			break;
		}
		else if (result == VK_SUBOPTIMAL_KHR)
		{
			lastSwapWidth = 0;
			lastSwapHeight = 0;
			break;
		}
		else if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreate(width, height);
		}
		else if (result == VK_NOT_READY || result == VK_TIMEOUT)
		{
			imageIndex = 0xffffffff;
			break;
		}
		else
		{
			throw std::runtime_error("Failed to acquire next image!");
		}
	}
	return imageIndex;
}

void VulkanSwapChain::queuePresent(uint32_t imageIndex, VulkanSemaphore *semaphore)
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = semaphore ? 1 : 0;
	presentInfo.pWaitSemaphores = semaphore ? &semaphore->semaphore : VK_NULL_HANDLE;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;
	VkResult result = vkQueuePresentKHR(device->presentQueue, &presentInfo);
	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		lastSwapWidth = 0;
		lastSwapHeight = 0;
	}
	else if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
	{
		// The spec says we can recover from this.
		// However, if we are out of memory it is better to crash now than in some other weird place further away from the source of the problem.

		throw std::runtime_error("vkQueuePresentKHR failed: out of memory");
	}
	else if (result == VK_ERROR_DEVICE_LOST)
	{
		throw std::runtime_error("vkQueuePresentKHR failed: device lost");
	}
	else if (result == VK_ERROR_SURFACE_LOST_KHR)
	{
		throw std::runtime_error("vkQueuePresentKHR failed: surface lost");
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("vkQueuePresentKHR failed");
	}
}

void VulkanSwapChain::recreate(int width, int height)
{
	releaseViews();
	swapChainImages.clear();

	selectFormat();
	selectPresentMode();

	VkSwapchainKHR oldSwapChain = swapChain;
	createSwapChain(width, height, oldSwapChain);
	if (oldSwapChain)
		vkDestroySwapchainKHR(device->device, oldSwapChain, nullptr);

	if (swapChain)
	{
		getImages();
		createViews();
	}
}

bool VulkanSwapChain::createSwapChain(int width, int height, VkSwapchainKHR oldSwapChain)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities = getSurfaceCapabilities();

	actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	actualExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, actualExtent.height));
	if (actualExtent.width == 0 || actualExtent.height == 0)
	{
		swapChain = VK_NULL_HANDLE;
		return false;
	}

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
		imageCount = surfaceCapabilities.maxImageCount;

	imageCount = std::min(imageCount, (uint32_t)2); // Only use two buffers (triple buffering sucks! good for benchmarks, bad for mouse input)

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = device->surface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = swapChainFormat.format;
	swapChainCreateInfo.imageColorSpace = swapChainFormat.colorSpace;
	swapChainCreateInfo.imageExtent = actualExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { (uint32_t)device->graphicsFamily, (uint32_t)device->presentFamily };
	if (device->graphicsFamily != device->presentFamily)
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // If alpha channel is passed on to the DWM or not
	swapChainCreateInfo.presentMode = swapChainPresentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = oldSwapChain;

	VkResult result = vkCreateSwapchainKHR(device->device, &swapChainCreateInfo, nullptr, &swapChain);
	if (result != VK_SUCCESS)
	{
		swapChain = VK_NULL_HANDLE;
		return false;
	}

	return true;
}

void VulkanSwapChain::createViews()
{
	swapChainImageViews.reserve(swapChainImages.size());
	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		device->setDebugObjectName("SwapChainImage", (uint64_t)swapChainImages[i], VK_OBJECT_TYPE_IMAGE);

		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainFormat.format;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VkImageView view;
		VkResult result = vkCreateImageView(device->device, &createInfo, nullptr, &view);
		if (result != VK_SUCCESS)
			throw std::runtime_error("Could not create image view for swapchain image");

		device->setDebugObjectName("SwapChainImageView", (uint64_t)view, VK_OBJECT_TYPE_IMAGE_VIEW);

		swapChainImageViews.push_back(view);
	}
}

void VulkanSwapChain::selectFormat()
{
	std::vector<VkSurfaceFormatKHR> surfaceFormats = getSurfaceFormats();
	if (surfaceFormats.empty())
		throw std::runtime_error("No surface formats supported");

	if (surfaceFormats.size() == 1 && surfaceFormats.front().format == VK_FORMAT_UNDEFINED)
	{
		swapChainFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
		swapChainFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return;
	}

	for (const auto &format : surfaceFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			swapChainFormat = format;
			return;
		}
	}

	swapChainFormat = surfaceFormats.front();
}

void VulkanSwapChain::selectPresentMode()
{
	std::vector<VkPresentModeKHR> presentModes = getPresentModes();

	if (presentModes.empty())
		throw std::runtime_error("No surface present modes supported");

	swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (vsync)
	{
		bool supportsFifoRelaxed = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != presentModes.end();
		if (supportsFifoRelaxed)
			swapChainPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	}
	else
	{
		bool supportsMailbox = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();
		bool supportsImmediate = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end();
		if (supportsMailbox)
			swapChainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if (supportsImmediate)
			swapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
}

void VulkanSwapChain::getImages()
{
	uint32_t imageCount;
	VkResult result = vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetSwapchainImagesKHR failed");

	swapChainImages.resize(imageCount);
	result = vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swapChainImages.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetSwapchainImagesKHR failed (2)");
}

void VulkanSwapChain::releaseViews()
{
	for (auto &view : swapChainImageViews)
	{
		vkDestroyImageView(device->device, view, nullptr);
	}
	swapChainImageViews.clear();
}

void VulkanSwapChain::releaseResources()
{
	releaseViews();
	if (swapChain)
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
}

VkSurfaceCapabilitiesKHR VulkanSwapChain::getSurfaceCapabilities()
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice.device, device->surface, &surfaceCapabilities);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
	return surfaceCapabilities;
}

std::vector<VkSurfaceFormatKHR> VulkanSwapChain::getSurfaceFormats()
{
	uint32_t surfaceFormatCount = 0;
	VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice.device, device->surface, &surfaceFormatCount, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceFormatsKHR failed");
	else if (surfaceFormatCount == 0)
		return {};

	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice.device, device->surface, &surfaceFormatCount, surfaceFormats.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceFormatsKHR failed");
	return surfaceFormats;
}

std::vector<VkPresentModeKHR> VulkanSwapChain::getPresentModes()
{
	uint32_t presentModeCount = 0;
	VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice.device, device->surface, &presentModeCount, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModesKHR failed");
	else if (presentModeCount == 0)
		return {};

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice.device, device->surface, &presentModeCount, presentModes.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModesKHR failed");
	return presentModes;
}
