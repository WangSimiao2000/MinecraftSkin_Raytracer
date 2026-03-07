#ifdef HAVE_VULKAN_RT

#include "vulkan/vk_context.h"

#include <cstring>
#include <vector>

// ── Required device extensions for hardware ray tracing ─────────────────────
static const char* const kRequiredDeviceExtensions[] = {
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    // Dependencies required by the above two:
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
};
static constexpr uint32_t kRequiredDeviceExtCount =
    static_cast<uint32_t>(sizeof(kRequiredDeviceExtensions) / sizeof(kRequiredDeviceExtensions[0]));

// ── Validation layer name ───────────────────────────────────────────────────
#ifndef NDEBUG
static const char* const kValidationLayer = "VK_LAYER_KHRONOS_validation";
#endif

// ── Helpers ─────────────────────────────────────────────────────────────────

/// Returns true if the physical device supports every extension in
/// kRequiredDeviceExtensions.
static bool deviceSupportsAllExtensions(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (uint32_t r = 0; r < kRequiredDeviceExtCount; ++r) {
        bool found = false;
        for (const auto& ext : available) {
            if (std::strcmp(ext.extensionName, kRequiredDeviceExtensions[r]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

/// Returns the index of the first queue family that supports compute, or -1.
static int32_t findComputeQueueFamily(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            return static_cast<int32_t>(i);
    }
    return -1;
}

// ── VulkanContext implementation ────────────────────────────────────────────

VulkanContext::VulkanContext() {
    try {
        createInstance();
        selectPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
    } catch (...) {
        cleanup();
        throw;
    }
}

VulkanContext::~VulkanContext() {
    cleanup();
}

// ── Instance creation ───────────────────────────────────────────────────────

void VulkanContext::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "MCSkin_RayTracer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "MCSkin";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

#ifndef NDEBUG
    // Enable validation layers in debug builds.
    createInfo.enabledLayerCount   = 1;
    createInfo.ppEnabledLayerNames = &kValidationLayer;
#endif

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance_),
             "Failed to create Vulkan instance");
}

// ── Physical device selection ───────────────────────────────────────────────

void VulkanContext::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0)
        throw VulkanError("No Vulkan physical devices found", VK_ERROR_INITIALIZATION_FAILED);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (const auto& dev : devices) {
        if (!deviceSupportsAllExtensions(dev))
            continue;

        int32_t queueFamily = findComputeQueueFamily(dev);
        if (queueFamily < 0)
            continue;

        physicalDevice_      = dev;
        computeQueueFamily_  = static_cast<uint32_t>(queueFamily);

        // Query ray tracing pipeline properties (needed for SBT alignment).
        rtPipelineProps_ = {};
        rtPipelineProps_.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &rtPipelineProps_;
        vkGetPhysicalDeviceProperties2(dev, &props2);

        return;
    }

    throw VulkanError("No GPU with required ray tracing extensions found",
                      VK_ERROR_FEATURE_NOT_PRESENT);
}

// ── Logical device creation ─────────────────────────────────────────────────

void VulkanContext::createLogicalDevice() {
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCI{};
    queueCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCI.queueFamilyIndex = computeQueueFamily_;
    queueCI.queueCount       = 1;
    queueCI.pQueuePriorities = &queuePriority;

    // ── Chain the required features ─────────────────────────────────────
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bdaFeatures{};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
    bdaFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.pNext = &bdaFeatures;
    asFeatures.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
    rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeatures.pNext = &asFeatures;
    rtFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &rtFeatures;

    VkDeviceCreateInfo deviceCI{};
    deviceCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.pNext                   = &features2;
    deviceCI.queueCreateInfoCount    = 1;
    deviceCI.pQueueCreateInfos       = &queueCI;
    deviceCI.enabledExtensionCount   = kRequiredDeviceExtCount;
    deviceCI.ppEnabledExtensionNames = kRequiredDeviceExtensions;

    VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCI, nullptr, &device_),
             "Failed to create Vulkan logical device");

    vkGetDeviceQueue(device_, computeQueueFamily_, 0, &computeQueue_);

    loadDeviceFunctions();
}

// ── Command pool creation ───────────────────────────────────────────────────

void VulkanContext::loadDeviceFunctions() {
#define LOAD_VK(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) throw VulkanError("Failed to load " #name, VK_ERROR_INITIALIZATION_FAILED)

    LOAD_VK(vkCreateAccelerationStructureKHR);
    LOAD_VK(vkDestroyAccelerationStructureKHR);
    LOAD_VK(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_VK(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_VK(vkCmdBuildAccelerationStructuresKHR);
    LOAD_VK(vkCreateRayTracingPipelinesKHR);
    LOAD_VK(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_VK(vkCmdTraceRaysKHR);

#undef LOAD_VK
}

void VulkanContext::createCommandPool() {
    VkCommandPoolCreateInfo poolCI{};
    poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCI.queueFamilyIndex = computeQueueFamily_;

    VK_CHECK(vkCreateCommandPool(device_, &poolCI, nullptr, &commandPool_),
             "Failed to create Vulkan command pool");
}

// ── Cleanup (RAII) ──────────────────────────────────────────────────────────

void VulkanContext::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        if (commandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
            commandPool_ = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

#endif // HAVE_VULKAN_RT
