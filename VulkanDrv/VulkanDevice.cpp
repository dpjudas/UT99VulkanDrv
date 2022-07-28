
#include "Precomp.h"
#include "VulkanDevice.h"
#include "VulkanObjects.h"
#include <algorithm>
#include <set>
#include <string>

VulkanDevice::VulkanDevice(HWND window, int vk_device, bool vk_debug, std::function<void(const char* typestr, const std::string& msg)> printLogCallback) : window(window), vk_device(vk_device), vk_debug(vk_debug), printLogCallback(printLogCallback)
{
	try
	{
		InitVolk();
		CreateInstance();
		CreateSurface();
		SelectPhysicalDevice();
		SelectFeatures();
		CreateDevice();
		CreateAllocator();
	}
	catch (...)
	{
		ReleaseResources();
		throw;
	}
}

VulkanDevice::~VulkanDevice()
{
	ReleaseResources();
}

void VulkanDevice::SelectFeatures()
{
	UsedDeviceFeatures.samplerAnisotropy = PhysicalDevice.Features.samplerAnisotropy;
	UsedDeviceFeatures.fragmentStoresAndAtomics = PhysicalDevice.Features.fragmentStoresAndAtomics;
	UsedDeviceFeatures.depthClamp = PhysicalDevice.Features.depthClamp;
	UsedDeviceFeatures.shaderClipDistance = PhysicalDevice.Features.shaderClipDistance;
}

bool VulkanDevice::CheckRequiredFeatures(const VkPhysicalDeviceFeatures &f)
{
	return
		f.samplerAnisotropy == VK_TRUE &&
		f.fragmentStoresAndAtomics == VK_TRUE;
}

void VulkanDevice::SelectPhysicalDevice()
{
	AvailableDevices = GetPhysicalDevices(instance, ApiVersion);
	if (AvailableDevices.empty())
		VulkanError("No Vulkan devices found. Either the graphics card has no vulkan support or the driver is too old.");

	for (size_t idx = 0; idx < AvailableDevices.size(); idx++)
	{
		const auto &info = AvailableDevices[idx];

		if (!CheckRequiredFeatures(info.Features))
			continue;

		std::set<std::string> requiredExtensionSearch(EnabledDeviceExtensions.begin(), EnabledDeviceExtensions.end());
		for (const auto &ext : info.Extensions)
			requiredExtensionSearch.erase(ext.extensionName);
		if (!requiredExtensionSearch.empty())
			continue;

		VulkanCompatibleDevice dev;
		dev.device = &AvailableDevices[idx];

		// Figure out what can present
		for (int i = 0; i < (int)info.QueueFamilies.size(); i++)
		{
			VkBool32 presentSupport = false;
			VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(info.Device, i, surface, &presentSupport);
			if (result == VK_SUCCESS && info.QueueFamilies[i].queueCount > 0 && presentSupport)
			{
				dev.presentFamily = i;
				break;
			}
		}

		// The vulkan spec states that graphics and compute queues can always do transfer.
		// Furthermore the spec states that graphics queues always can do compute.
		// Last, the spec makes it OPTIONAL whether the VK_QUEUE_TRANSFER_BIT is set for such queues, but they MUST support transfer.
		//
		// In short: pick the first graphics queue family for everything.
		for (int i = 0; i < (int)info.QueueFamilies.size(); i++)
		{
			const auto &queueFamily = info.QueueFamilies[i];
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				dev.graphicsFamily = i;
				dev.graphicsTimeQueries = queueFamily.timestampValidBits != 0;
				break;
			}
		}

		if (dev.graphicsFamily != -1 && dev.presentFamily != -1)
		{
			SupportedDevices.push_back(dev);
		}
	}

	if (SupportedDevices.empty())
		VulkanError("No Vulkan device supports the minimum requirements of this application");

	// The device order returned by Vulkan can be anything. Prefer discrete > integrated > virtual gpu > cpu > other
	std::stable_sort(SupportedDevices.begin(), SupportedDevices.end(), [&](const auto &a, const auto b) {

		// Sort by GPU type first. This will ensure the "best" device is most likely to map to vk_device 0
		static const int typeSort[] = { 4, 1, 0, 2, 3 };
		int sortA = a.device->Properties.deviceType < 5 ? typeSort[a.device->Properties.deviceType] : (int)a.device->Properties.deviceType;
		int sortB = b.device->Properties.deviceType < 5 ? typeSort[b.device->Properties.deviceType] : (int)b.device->Properties.deviceType;
		if (sortA != sortB)
			return sortA < sortB;

		// Then sort by the device's unique ID so that vk_device uses a consistent order
		int sortUUID = memcmp(a.device->Properties.pipelineCacheUUID, b.device->Properties.pipelineCacheUUID, VK_UUID_SIZE);
		return sortUUID < 0;
	});

	size_t selected = vk_device;
	if (selected >= SupportedDevices.size())
		selected = 0;

	// Enable optional extensions we are interested in, if they are available on this device
	for (const auto &ext : SupportedDevices[selected].device->Extensions)
	{
		for (const auto &opt : OptionalDeviceExtensions)
		{
			if (strcmp(ext.extensionName, opt) == 0)
			{
				EnabledDeviceExtensions.push_back(opt);
			}
		}
	}

	PhysicalDevice = *SupportedDevices[selected].device;
	graphicsFamily = SupportedDevices[selected].graphicsFamily;
	presentFamily = SupportedDevices[selected].presentFamily;
	graphicsTimeQueries = SupportedDevices[selected].graphicsTimeQueries;
}

bool VulkanDevice::SupportsDeviceExtension(const char *ext) const
{
	return std::find(EnabledDeviceExtensions.begin(), EnabledDeviceExtensions.end(), ext) != EnabledDeviceExtensions.end();
}

void VulkanDevice::CreateAllocator()
{
	VmaAllocatorCreateInfo allocinfo = {};
	allocinfo.vulkanApiVersion = ApiVersion;
	if (SupportsDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) && SupportsDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME))
		allocinfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	if (SupportsDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
		allocinfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocinfo.physicalDevice = PhysicalDevice.Device;
	allocinfo.device = device;
	allocinfo.instance = instance;
	allocinfo.preferredLargeHeapBlockSize = 64 * 1024 * 1024;
	if (vmaCreateAllocator(&allocinfo, &allocator) != VK_SUCCESS)
		VulkanError("Unable to create allocator");
}

void VulkanDevice::CreateDevice()
{
	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	std::set<int> neededFamilies;
	neededFamilies.insert(graphicsFamily);
	neededFamilies.insert(presentFamily);

	for (int index : neededFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = index;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	VkPhysicalDeviceFeatures2 deviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	VkPhysicalDeviceBufferDeviceAddressFeatures deviceAddressFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
	VkPhysicalDeviceAccelerationStructureFeaturesKHR deviceAccelFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };

	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = (uint32_t)EnabledDeviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = EnabledDeviceExtensions.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceFeatures2.features = UsedDeviceFeatures;
	deviceAddressFeatures.bufferDeviceAddress = true;
	deviceAccelFeatures.accelerationStructure = true;
	rayQueryFeatures.rayQuery = true;
	descriptorIndexingFeatures.descriptorBindingPartiallyBound = true;
	descriptorIndexingFeatures.runtimeDescriptorArray = true;

	void** next = const_cast<void**>(&deviceCreateInfo.pNext);
	if (SupportsDeviceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
	{
		*next = &deviceFeatures2;
		next = &deviceFeatures2.pNext;
	}
	else // vulkan 1.0 specified features in a different way
	{
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures2.features;
	}
	if (SupportsDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) && PhysicalDevice.BufferDeviceAddressFeatures.bufferDeviceAddress)
	{
		*next = &deviceAddressFeatures;
		next = &deviceAddressFeatures.pNext;
	}
	if (SupportsDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) && PhysicalDevice.AccelerationStructureFeatures.accelerationStructure)
	{
		*next = &deviceAccelFeatures;
		next = &deviceAccelFeatures.pNext;
	}
	if (SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME) && PhysicalDevice.RayQueryFeatures.rayQuery)
	{
		*next = &rayQueryFeatures;
		next = &rayQueryFeatures.pNext;
	}
	if (SupportsDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) && PhysicalDevice.DescriptorIndexingFeatures.descriptorBindingPartiallyBound && PhysicalDevice.DescriptorIndexingFeatures.runtimeDescriptorArray)
	{
		*next = &descriptorIndexingFeatures;
		next = &descriptorIndexingFeatures.pNext;
	}

	VkResult result = vkCreateDevice(PhysicalDevice.Device, &deviceCreateInfo, nullptr, &device);
	CheckVulkanError(result, "Could not create vulkan device");

	volkLoadDevice(device);

	vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
}

void VulkanDevice::CreateSurface()
{
	VkWin32SurfaceCreateInfoKHR windowCreateInfo = {};
	windowCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	windowCreateInfo.hwnd = window;
	windowCreateInfo.hinstance = GetModuleHandle(nullptr);

	VkResult result = vkCreateWin32SurfaceKHR(instance, &windowCreateInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
		VulkanError("Could not create vulkan surface");
}

void VulkanDevice::CreateInstance()
{
	AvailableLayers = GetAvailableLayers();
	Extensions = GetExtensions();
	EnabledExtensions = GetPlatformExtensions();

	std::string debugLayer = "VK_LAYER_KHRONOS_validation";
	bool wantDebugLayer = vk_debug;
	bool debugLayerFound = false;
	if (wantDebugLayer)
	{
		for (const VkLayerProperties& layer : AvailableLayers)
		{
			if (layer.layerName == debugLayer)
			{
				EnabledValidationLayers.push_back(layer.layerName);
				EnabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				debugLayerFound = true;
				break;
			}
		}
	}

	// Enable optional instance extensions we are interested in
	for (const auto &ext : Extensions)
	{
		for (const auto &opt : OptionalExtensions)
		{
			if (strcmp(ext.extensionName, opt) == 0)
			{
				EnabledExtensions.push_back(opt);
			}
		}
	}

	// Try get the highest vulkan version we can get
	VkResult result = VK_ERROR_INITIALIZATION_FAILED;
	for (uint32_t apiVersion : { VK_API_VERSION_1_2, VK_API_VERSION_1_1, VK_API_VERSION_1_0 })
	{
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "VulkanDrv";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "VulkanDrv";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = apiVersion;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint32_t)EnabledExtensions.size();
		createInfo.enabledLayerCount = (uint32_t)EnabledValidationLayers.size();
		createInfo.ppEnabledLayerNames = EnabledValidationLayers.data();
		createInfo.ppEnabledExtensionNames = EnabledExtensions.data();

		result = vkCreateInstance(&createInfo, nullptr, &instance);
		if (result >= VK_SUCCESS)
		{
			ApiVersion = apiVersion;
			break;
		}
	}
	CheckVulkanError(result, "Could not create vulkan instance");

	volkLoadInstance(instance);

	if (debugLayerFound)
	{
		VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo = {};
		dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbgCreateInfo.messageSeverity =
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbgCreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dbgCreateInfo.pfnUserCallback = DebugCallback;
		dbgCreateInfo.pUserData = this;
		result = vkCreateDebugUtilsMessengerEXT(instance, &dbgCreateInfo, nullptr, &debugMessenger);
		CheckVulkanError(result, "vkCreateDebugUtilsMessengerEXT failed");

		DebugLayerActive = true;
	}
}

static std::vector<std::string> split(const std::string& s, const std::string& seperator)
{
	std::vector<std::string> output;
	std::string::size_type prev_pos = 0, pos = 0;

	while ((pos = s.find(seperator, pos)) != std::string::npos)
	{
		std::string substring(s.substr(prev_pos, pos - prev_pos));

		output.push_back(substring);

		pos += seperator.length();
		prev_pos = pos;
	}

	output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word
	return output;
}
VkBool32 VulkanDevice::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
{
	VulkanDevice* device = (VulkanDevice*)userData;

	static std::mutex mtx;
	static std::set<std::string> seenMessages;
	static int totalMessages;

	std::unique_lock<std::mutex> lock(mtx);

	std::string msg = callbackData->pMessage;

	// Attempt to parse the string because the default formatting is totally unreadable and half of what it writes is totally useless!
	auto parts = split(msg, " | ");
	if (parts.size() == 3)
	{
		msg = parts[2];
		size_t pos = msg.find(" The Vulkan spec states:");
		if (pos != std::string::npos)
			msg = msg.substr(0, pos);

		if (callbackData->objectCount > 0)
		{
			msg += " (";
			for (uint32_t i = 0; i < callbackData->objectCount; i++)
			{
				if (i > 0)
					msg += ", ";
				if (callbackData->pObjects[i].pObjectName)
					msg += callbackData->pObjects[i].pObjectName;
				else
					msg += "<noname>";
			}
			msg += ")";
		}
	}

	bool found = seenMessages.find(msg) != seenMessages.end();
	if (!found)
	{
		if (totalMessages < 20)
		{
			totalMessages++;
			seenMessages.insert(msg);

			const char *typestr;
			bool showcallstack = false;
			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				typestr = "vulkan error";
				showcallstack = true;
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			{
				typestr = "vulkan warning";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			{
				typestr = "vulkan info";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
			{
				typestr = "vulkan verbose";
			}
			else
			{
				typestr = "vulkan";
			}

			if (device->printLogCallback)
				device->printLogCallback(typestr, msg);
		}
	}

	return VK_FALSE;
}

std::vector<VkLayerProperties> VulkanDevice::GetAvailableLayers()
{
	uint32_t layerCount;
	VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	return availableLayers;
}

std::vector<VkExtensionProperties> VulkanDevice::GetExtensions()
{
	uint32_t extensionCount = 0;
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	return extensions;
}

std::vector<VulkanPhysicalDevice> VulkanDevice::GetPhysicalDevices(VkInstance instance, uint32_t apiVersion)
{
	uint32_t deviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (result == VK_ERROR_INITIALIZATION_FAILED) // Some drivers return this when a card does not support vulkan
		return {};
	CheckVulkanError(result, "vkEnumeratePhysicalDevices failed");
	if (deviceCount == 0)
		return {};

	std::vector<VkPhysicalDevice> devices(deviceCount);
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	CheckVulkanError(result, "vkEnumeratePhysicalDevices failed (2)");

	std::vector<VulkanPhysicalDevice> devinfo(deviceCount);
	for (size_t i = 0; i < devices.size(); i++)
	{
		auto &dev = devinfo[i];
		dev.Device = devices[i];

		vkGetPhysicalDeviceMemoryProperties(dev.Device, &dev.MemoryProperties);
		vkGetPhysicalDeviceProperties(dev.Device, &dev.Properties);

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(dev.Device, &queueFamilyCount, nullptr);
		dev.QueueFamilies.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(dev.Device, &queueFamilyCount, dev.QueueFamilies.data());

		uint32_t deviceExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(dev.Device, nullptr, &deviceExtensionCount, nullptr);
		dev.Extensions.resize(deviceExtensionCount);
		vkEnumerateDeviceExtensionProperties(dev.Device, nullptr, &deviceExtensionCount, dev.Extensions.data());

		auto checkForExtension = [&](const char* name)
		{
			for (const auto& ext : dev.Extensions)
			{
				if (strcmp(ext.extensionName, name) == 0)
					return true;
			}
			return false;
		};

		if (apiVersion != VK_API_VERSION_1_0)
		{
			VkPhysicalDeviceFeatures2 deviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

			void** next = const_cast<void**>(&deviceFeatures2.pNext);
			if (checkForExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
			{
				*next = &dev.BufferDeviceAddressFeatures;
				next = &dev.BufferDeviceAddressFeatures.pNext;
			}
			if (checkForExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
			{
				*next = &dev.AccelerationStructureFeatures;
				next = &dev.AccelerationStructureFeatures.pNext;
			}
			if (checkForExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
			{
				*next = &dev.RayQueryFeatures;
				next = &dev.RayQueryFeatures.pNext;
			}
			if (checkForExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
			{
				*next = &dev.DescriptorIndexingFeatures;
				next = &dev.DescriptorIndexingFeatures.pNext;
			}

			vkGetPhysicalDeviceFeatures2(dev.Device, &deviceFeatures2);
			dev.Features = deviceFeatures2.features;
			dev.BufferDeviceAddressFeatures.pNext = nullptr;
			dev.AccelerationStructureFeatures.pNext = nullptr;
			dev.RayQueryFeatures.pNext = nullptr;
			dev.DescriptorIndexingFeatures.pNext = nullptr;
		}
		else
		{
			vkGetPhysicalDeviceFeatures(dev.Device, &dev.Features);
		}
	}
	return devinfo;
}

std::vector<const char *> VulkanDevice::GetPlatformExtensions()
{
	return
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
	};
}

void VulkanDevice::InitVolk()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		VulkanError("Unable to find Vulkan");
	}
	auto iver = volkGetInstanceVersion();
	if (iver == 0)
	{
		VulkanError("Vulkan not supported");
	}
}

void VulkanDevice::ReleaseResources()
{
	if (device)
		vkDeviceWaitIdle(device);

	if (allocator)
		vmaDestroyAllocator(allocator);

	if (device)
		vkDestroyDevice(device, nullptr);
	device = nullptr;

	if (surface)
		vkDestroySurfaceKHR(instance, surface, nullptr);
	surface = 0;

	if (debugMessenger)
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	if (instance)
		vkDestroyInstance(instance, nullptr);
	instance = nullptr;
}

std::string VkResultToString(VkResult result)
{
	switch (result)
	{
	case VK_SUCCESS: return "success";
	case VK_NOT_READY: return "not ready";
	case VK_TIMEOUT: return "timeout";
	case VK_EVENT_SET: return "event set";
	case VK_EVENT_RESET: return "event reset";
	case VK_INCOMPLETE: return "incomplete";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "out of host memory";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "out of device memory";
	case VK_ERROR_INITIALIZATION_FAILED: return "initialization failed";
	case VK_ERROR_DEVICE_LOST: return "device lost";
	case VK_ERROR_MEMORY_MAP_FAILED: return "memory map failed";
	case VK_ERROR_LAYER_NOT_PRESENT: return "layer not present";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "extension not present";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "feature not present";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "incompatible driver";
	case VK_ERROR_TOO_MANY_OBJECTS: return "too many objects";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "format not supported";
	case VK_ERROR_FRAGMENTED_POOL: return "fragmented pool";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "out of pool memory";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "invalid external handle";
	case VK_ERROR_SURFACE_LOST_KHR: return "surface lost";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "native window in use";
	case VK_SUBOPTIMAL_KHR: return "suboptimal";
	case VK_ERROR_OUT_OF_DATE_KHR: return "out of date";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "incompatible display";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "validation failed";
	case VK_ERROR_INVALID_SHADER_NV: return "invalid shader";
	case VK_ERROR_FRAGMENTATION_EXT: return "fragmentation";
	case VK_ERROR_NOT_PERMITTED_EXT: return "not permitted";
	default: break;
	}
	return "vkResult " + std::to_string((int)result);
}
