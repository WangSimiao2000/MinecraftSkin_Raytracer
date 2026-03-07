#pragma once

#ifdef HAVE_VULKAN_RT

#include "vulkan/vk_context.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

/// RAII wrapper around a VkBuffer + VkDeviceMemory pair.
/// Automatically destroys both resources when the object goes out of scope.
class VkBufferAlloc {
public:
    VkBufferAlloc() = default;

    VkBufferAlloc(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize size)
        : device_(device), buffer_(buffer), memory_(memory), size_(size) {}

    ~VkBufferAlloc() { destroy(); }

    // Move-only.
    VkBufferAlloc(VkBufferAlloc&& other) noexcept
        : device_(other.device_)
        , buffer_(other.buffer_)
        , memory_(other.memory_)
        , size_(other.size_) {
        other.device_ = VK_NULL_HANDLE;
        other.buffer_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.size_   = 0;
    }

    VkBufferAlloc& operator=(VkBufferAlloc&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = other.device_;
            buffer_ = other.buffer_;
            memory_ = other.memory_;
            size_   = other.size_;
            other.device_ = VK_NULL_HANDLE;
            other.buffer_ = VK_NULL_HANDLE;
            other.memory_ = VK_NULL_HANDLE;
            other.size_   = 0;
        }
        return *this;
    }

    VkBufferAlloc(const VkBufferAlloc&) = delete;
    VkBufferAlloc& operator=(const VkBufferAlloc&) = delete;

    VkBuffer       buffer() const { return buffer_; }
    VkDeviceMemory memory() const { return memory_; }
    VkDeviceSize   size()   const { return size_; }

    /// Returns true if this allocation holds valid Vulkan handles.
    explicit operator bool() const { return buffer_ != VK_NULL_HANDLE; }

private:
    void destroy() {
        if (device_ != VK_NULL_HANDLE) {
            if (buffer_ != VK_NULL_HANDLE) {
                vkDestroyBuffer(device_, buffer_, nullptr);
                buffer_ = VK_NULL_HANDLE;
            }
            if (memory_ != VK_NULL_HANDLE) {
                vkFreeMemory(device_, memory_, nullptr);
                memory_ = VK_NULL_HANDLE;
            }
        }
    }

    VkDevice       device_ = VK_NULL_HANDLE;
    VkBuffer       buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize   size_   = 0;
};

/// Static helper functions for Vulkan buffer operations.
class VkBufferHelper {
public:
    /// Find a memory type index that satisfies the given requirements.
    /// Throws VulkanError if no suitable memory type is found.
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                                   uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties);

    /// Create a buffer with the specified usage and memory properties.
    static VkBufferAlloc createBuffer(VulkanContext& ctx,
                                      VkDeviceSize size,
                                      VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags memoryProperties);

    /// Create a device-local buffer by staging data through a host-visible buffer.
    /// The returned buffer lives in device-local memory.
    static VkBufferAlloc createDeviceLocal(VulkanContext& ctx,
                                           const void* data,
                                           VkDeviceSize size,
                                           VkBufferUsageFlags usage);

    /// Read back GPU buffer data to CPU memory.
    static std::vector<uint8_t> readback(VulkanContext& ctx,
                                         VkBuffer srcBuffer,
                                         VkDeviceSize size);

    /// Get the device address of a buffer (needed for acceleration structures).
    static VkDeviceAddress getBufferDeviceAddress(VulkanContext& ctx, VkBuffer buffer);

private:
    /// Execute a single-use command buffer (records, submits, waits).
    static void executeOneTimeCommands(VulkanContext& ctx,
                                       const std::function<void(VkCommandBuffer)>& fn);
};

#endif // HAVE_VULKAN_RT
