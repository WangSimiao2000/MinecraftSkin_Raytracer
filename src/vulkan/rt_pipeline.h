#pragma once

#ifdef HAVE_VULKAN_RT

#include "vulkan/vk_context.h"
#include "vulkan/vk_buffer.h"

#include <string>

/// Manages the Vulkan ray tracing pipeline, shader binding table,
/// descriptor set layout / pool / set, and shader modules.
/// All resources are released in the destructor (RAII).
class RTPipeline {
public:
    /// Construct the pipeline by loading SPIR-V shaders from \p shaderDir.
    /// Expected files: raygen.rgen.spv, closesthit.rchit.spv, miss.rmiss.spv.
    /// Throws VulkanError on failure.
    explicit RTPipeline(VulkanContext& ctx, const std::string& shaderDir);

    ~RTPipeline();

    // Non-copyable, non-movable.
    RTPipeline(const RTPipeline&) = delete;
    RTPipeline& operator=(const RTPipeline&) = delete;

    /// Bind resources into the descriptor set.
    /// Must be called before dispatch().
    void bind(VulkanContext& ctx,
              VkAccelerationStructureKHR tlas,
              VkImageView outputImageView,
              VkBuffer uniformBuffer,
              VkBuffer vertexBuffer,   VkDeviceSize vertexBufferSize,
              VkBuffer uvBuffer,       VkDeviceSize uvBufferSize,
              VkBuffer indexBuffer,    VkDeviceSize indexBufferSize,
              VkImageView textureView, VkSampler textureSampler);

    /// Record a vkCmdTraceRaysKHR command into \p cmdBuf.
    void dispatch(VkCommandBuffer cmdBuf, uint32_t width, uint32_t height) const;

private:
    // ── Shader module loading ───────────────────────────────────────────
    VkShaderModule loadShaderModule(VkDevice device, const std::string& path);

    // ── Creation helpers ────────────────────────────────────────────────
    void createDescriptorSetLayout(VkDevice device);
    void createPipeline(VkDevice device,
                        VkShaderModule raygenModule,
                        VkShaderModule chitModule,
                        VkShaderModule missModule);
    void createSBT(VulkanContext& ctx);
    void createDescriptorPoolAndSet(VkDevice device);

    void cleanup();

    VkDevice device_ = VK_NULL_HANDLE;

    // Shader modules
    VkShaderModule raygenModule_ = VK_NULL_HANDLE;
    VkShaderModule chitModule_   = VK_NULL_HANDLE;
    VkShaderModule missModule_   = VK_NULL_HANDLE;

    // Descriptor set layout, pool, set
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool_      = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet_       = VK_NULL_HANDLE;

    // Pipeline
    VkPipelineLayout          pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline                pipeline_       = VK_NULL_HANDLE;

    // Shader Binding Table
    VkBufferAlloc sbtBuffer_;

    VkStridedDeviceAddressRegionKHR raygenRegion_{};
    VkStridedDeviceAddressRegionKHR missRegion_{};
    VkStridedDeviceAddressRegionKHR hitRegion_{};
    VkStridedDeviceAddressRegionKHR callableRegion_{}; // unused, zeroed
};

#endif // HAVE_VULKAN_RT
