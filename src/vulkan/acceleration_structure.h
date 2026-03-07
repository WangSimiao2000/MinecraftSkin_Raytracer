#pragma once

#ifdef HAVE_VULKAN_RT

#include "vulkan/vk_context.h"
#include "vulkan/vk_buffer.h"
#include "scene/mesh.h"

#include <vector>

/// Holds a single bottom-level acceleration structure and its backing storage.
struct BLASEntry {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkBufferAlloc              buffer;          // backing memory
    VkDeviceAddress            deviceAddress = 0;
};

/// Manages BLAS / TLAS construction and lifetime (RAII).
/// All Vulkan resources are released in the destructor.
class AccelerationStructure {
public:
    AccelerationStructure() = default;
    ~AccelerationStructure();

    // Non-copyable, movable.
    AccelerationStructure(const AccelerationStructure&) = delete;
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;
    AccelerationStructure(AccelerationStructure&& other) noexcept;
    AccelerationStructure& operator=(AccelerationStructure&& other) noexcept;

    /// Build a bottom-level acceleration structure from a mesh's triangles.
    /// The mesh's triangles are uploaded as vertex + index data.
    void buildBLAS(VulkanContext& ctx, const Mesh& mesh);

    /// Build the top-level acceleration structure that references all BLAS
    /// entries with their respective transforms (derived from each Mesh pose).
    void buildTLAS(VulkanContext& ctx,
                   const std::vector<Mesh>& meshes);

    // ── Accessors ───────────────────────────────────────────────────────
    VkAccelerationStructureKHR tlas() const { return tlas_; }

    const std::vector<BLASEntry>& blasEntries() const { return blasEntries_; }

private:
    /// Convert a Mesh's rotation pose into a VkTransformMatrixKHR (3×4 row-major).
    static VkTransformMatrixKHR poseToTransform(const Mesh& mesh);

    void cleanup();

    VkDevice device_ = VK_NULL_HANDLE;

    // Dynamically loaded KHR function pointers (cached from VulkanContext)
    PFN_vkCreateAccelerationStructureKHR           pfnCreateAS_   = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          pfnDestroyAS_  = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR    pfnGetBuildSizes_ = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR pfnGetASAddr_  = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        pfnCmdBuildAS_ = nullptr;

    void cacheDeviceFunctions(VulkanContext& ctx);

    // Bottom-level
    std::vector<BLASEntry> blasEntries_;

    // Top-level
    VkAccelerationStructureKHR tlas_       = VK_NULL_HANDLE;
    VkBufferAlloc              tlasBuffer_;
    VkBufferAlloc              instanceBuffer_;
};

#endif // HAVE_VULKAN_RT
