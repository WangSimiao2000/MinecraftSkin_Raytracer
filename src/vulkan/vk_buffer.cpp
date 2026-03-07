#ifdef HAVE_VULKAN_RT

#include "vulkan/vk_buffer.h"

#include <cstring>
#include <functional>

// ── Memory type selection ───────────────────────────────────────────────────

uint32_t VkBufferHelper::findMemoryType(VkPhysicalDevice physicalDevice,
                                        uint32_t typeFilter,
                                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw VulkanError("Failed to find suitable memory type", VK_ERROR_FEATURE_NOT_PRESENT);
}

// ── Single-use command buffer helper ────────────────────────────────────────

void VkBufferHelper::executeOneTimeCommands(VulkanContext& ctx,
                                            const std::function<void(VkCommandBuffer)>& fn) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = ctx.commandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(ctx.device(), &allocInfo, &cmdBuf),
             "Failed to allocate one-time command buffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo),
             "Failed to begin one-time command buffer");

    fn(cmdBuf);

    VK_CHECK(vkEndCommandBuffer(cmdBuf),
             "Failed to end one-time command buffer");

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuf;

    VK_CHECK(vkQueueSubmit(ctx.computeQueue(), 1, &submitInfo, VK_NULL_HANDLE),
             "Failed to submit one-time command buffer");

    VK_CHECK(vkQueueWaitIdle(ctx.computeQueue()),
             "Failed to wait for queue idle");

    vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmdBuf);
}

// ── Buffer creation ─────────────────────────────────────────────────────────

VkBufferAlloc VkBufferHelper::createBuffer(VulkanContext& ctx,
                                           VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkMemoryPropertyFlags memoryProperties) {
    VkBufferCreateInfo bufferCI{};
    bufferCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size        = size;
    bufferCI.usage       = usage;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(ctx.device(), &bufferCI, nullptr, &buffer),
             "Failed to create buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(ctx.device(), buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(ctx.physicalDevice(),
                                               memReqs.memoryTypeBits,
                                               memoryProperties);

    // If the buffer needs a device address, enable the flag on the allocation.
    VkMemoryAllocateFlagsInfo flagsInfo{};
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        allocInfo.pNext = &flagsInfo;
    }

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult result = vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(ctx.device(), buffer, nullptr);
        throw VulkanError("Failed to allocate buffer memory", result);
    }

    result = vkBindBufferMemory(ctx.device(), buffer, memory, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(ctx.device(), memory, nullptr);
        vkDestroyBuffer(ctx.device(), buffer, nullptr);
        throw VulkanError("Failed to bind buffer memory", result);
    }

    return VkBufferAlloc(ctx.device(), buffer, memory, size);
}

// ── Device-local buffer via staging ─────────────────────────────────────────

VkBufferAlloc VkBufferHelper::createDeviceLocal(VulkanContext& ctx,
                                                const void* data,
                                                VkDeviceSize size,
                                                VkBufferUsageFlags usage) {
    // Create a host-visible staging buffer.
    auto staging = createBuffer(
        ctx, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Map, copy, unmap.
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(ctx.device(), staging.memory(), 0, size, 0, &mapped),
             "Failed to map staging buffer");
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(ctx.device(), staging.memory());

    // Create the device-local destination buffer.
    auto deviceLocal = createBuffer(
        ctx, size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Copy staging → device-local.
    executeOneTimeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmdBuf, staging.buffer(), deviceLocal.buffer(), 1, &region);
    });

    // Staging buffer is automatically destroyed when it goes out of scope.
    return deviceLocal;
}

// ── GPU → CPU readback ──────────────────────────────────────────────────────

std::vector<uint8_t> VkBufferHelper::readback(VulkanContext& ctx,
                                              VkBuffer srcBuffer,
                                              VkDeviceSize size) {
    // Create a host-visible destination buffer.
    auto hostBuf = createBuffer(
        ctx, size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Copy GPU buffer → host buffer.
    executeOneTimeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmdBuf, srcBuffer, hostBuf.buffer(), 1, &region);
    });

    // Map and read.
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(ctx.device(), hostBuf.memory(), 0, size, 0, &mapped),
             "Failed to map readback buffer");

    std::vector<uint8_t> result(static_cast<size_t>(size));
    std::memcpy(result.data(), mapped, result.size());
    vkUnmapMemory(ctx.device(), hostBuf.memory());

    // Host buffer is automatically destroyed when it goes out of scope.
    return result;
}

// ── Buffer device address ───────────────────────────────────────────────────

VkDeviceAddress VkBufferHelper::getBufferDeviceAddress(VulkanContext& ctx, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info{};
    info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buffer;
    return vkGetBufferDeviceAddress(ctx.device(), &info);
}

#endif // HAVE_VULKAN_RT
