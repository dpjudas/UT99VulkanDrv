#pragma once

#include <functional>
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>

class VulkanSwapChain;
class VulkanSemaphore;
class VulkanFence;

class VulkanPhysicalDevice
{
public:
	VkPhysicalDevice Device = VK_NULL_HANDLE;

	std::vector<VkExtensionProperties> Extensions;
	std::vector<VkQueueFamilyProperties> QueueFamilies;
	VkPhysicalDeviceProperties Properties = {};
	VkPhysicalDeviceFeatures Features = {};
	VkPhysicalDeviceMemoryProperties MemoryProperties = {};
	VkPhysicalDeviceBufferDeviceAddressFeatures BufferDeviceAddressFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
	VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructureFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	VkPhysicalDeviceRayQueryFeaturesKHR RayQueryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	VkPhysicalDeviceDescriptorIndexingFeatures DescriptorIndexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };
};

class VulkanCompatibleDevice
{
public:
	VulkanPhysicalDevice *device = nullptr;
	int graphicsFamily = -1;
	int presentFamily = -1;
	bool graphicsTimeQueries = false;
};

class VulkanDevice
{
public:
	VulkanDevice(HWND window, int vk_device = 0, bool vk_debug = false, std::function<void(const char* typestr, const std::string& msg)> printLogCallback = {});
	~VulkanDevice();

	void SetDebugObjectName(const char *name, uint64_t handle, VkObjectType type)
	{
		if (!DebugLayerActive) return;

		VkDebugUtilsObjectNameInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		info.objectHandle = handle;
		info.objectType = type;
		info.pObjectName = name;
		vkSetDebugUtilsObjectNameEXT(device, &info);
	}

	HWND window;

	// Physical device info
	std::vector<VulkanPhysicalDevice> AvailableDevices;
	std::vector<VulkanCompatibleDevice> SupportedDevices;

	// Instance setup
	std::vector<VkLayerProperties> AvailableLayers;
	std::vector<VkExtensionProperties> Extensions;
	std::vector<const char *> EnabledExtensions;
	std::vector<const char *> OptionalExtensions = { VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME };
	std::vector<const char*> EnabledValidationLayers;
	uint32_t ApiVersion = {};

	// Device setup
	VkPhysicalDeviceFeatures UsedDeviceFeatures = {};
	std::vector<const char *> EnabledDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::vector<const char *> OptionalDeviceExtensions =
	{
		VK_EXT_HDR_METADATA_EXTENSION_NAME,
		VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
	};
	VulkanPhysicalDevice PhysicalDevice;
	bool DebugLayerActive = false;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;

	int graphicsFamily = -1;
	int presentFamily = -1;
	bool graphicsTimeQueries = false;

	bool SupportsDeviceExtension(const char* ext) const;

private:
	int vk_device;
	bool vk_debug;
	std::function<void(const char* typestr, const std::string& msg)> printLogCallback;

	void CreateInstance();
	void CreateSurface();
	void SelectPhysicalDevice();
	void SelectFeatures();
	void CreateDevice();
	void CreateAllocator();
	void ReleaseResources();

	static bool CheckRequiredFeatures(const VkPhysicalDeviceFeatures &f);

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	static void InitVolk();
	static std::vector<VkLayerProperties> GetAvailableLayers();
	static std::vector<VkExtensionProperties> GetExtensions();
	static std::vector<const char *> GetPlatformExtensions();
	static std::vector<VulkanPhysicalDevice> GetPhysicalDevices(VkInstance instance, uint32_t apiVersion);
};

std::string VkResultToString(VkResult result);

inline void VulkanError(const char *text)
{
	throw std::runtime_error(text);
}

inline void CheckVulkanError(VkResult result, const char *text)
{
	if (result >= VK_SUCCESS)
		return;

	throw std::runtime_error(text + std::string(": ") + VkResultToString(result));
}
