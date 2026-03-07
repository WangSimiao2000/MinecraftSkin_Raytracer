#pragma once

#ifdef HAVE_VULKAN_RT

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <string>

/// Exception type for Vulkan errors.
class VulkanError : public std::runtime_error {
public:
    VulkanError(const std::string& msg, VkResult result)
        : std::runtime_error(msg + " (VkResult " + std::to_string(result) + ")")
        , vkResult(result) {}

    VkResult vkResult;
};

/// Convenience macro – throws VulkanError on failure.
#define VK_CHECK(result, msg)                        \
    do {                                             \
        VkResult _r = (result);                      \
        if (_r != VK_SUCCESS) throw VulkanError(msg, _r); \
    } while (0)

/// Manages the core Vulkan device lifecycle:
///   VkInstance → physical device → logical device → compute queue → command pool.
/// All resources are released in the destructor (RAII).
class VulkanContext {
public:
    /// Creates and initialises all Vulkan objects.
    /// Throws VulkanError if any step fails.
    VulkanContext();

    ~VulkanContext();

    // Non-copyable, non-movable (simple RAII).
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // ── Accessors ───────────────────────────────────────────────────────
    VkInstance                instance()       const { return instance_; }
    VkPhysicalDevice          physicalDevice() const { return physicalDevice_; }
    VkDevice                  device()         const { return device_; }
    VkQueue                   computeQueue()   const { return computeQueue_; }
    VkCommandPool             commandPool()    const { return commandPool_; }
    uint32_t                  computeQueueFamily() const { return computeQueueFamily_; }

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR&
    rtPipelineProperties() const { return rtPipelineProps_; }

    // ── Dynamically loaded KHR extension function pointers ──────────────
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR    = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR             vkCreateRayTracingPipelinesKHR             = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR       vkGetRayTracingShaderGroupHandlesKHR       = nullptr;
    PFN_vkCmdTraceRaysKHR                          vkCmdTraceRaysKHR                          = nullptr;

private:
    void loadDeviceFunctions();
    void createInstance();
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void cleanup();

    VkInstance       instance_       = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice         device_         = VK_NULL_HANDLE;
    VkQueue          computeQueue_   = VK_NULL_HANDLE;
    VkCommandPool    commandPool_    = VK_NULL_HANDLE;
    uint32_t         computeQueueFamily_ = 0;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProps_{};
};

#endif // HAVE_VULKAN_RT
