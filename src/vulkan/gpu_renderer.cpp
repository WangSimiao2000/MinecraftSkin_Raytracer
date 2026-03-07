#include "vulkan/gpu_renderer.h"

// ═══════════════════════════════════════════════════════════════════════════
// Stub implementation when Vulkan RT is not available
// ═══════════════════════════════════════════════════════════════════════════

#ifndef HAVE_VULKAN_RT

GpuRenderer::GpuRenderer(const std::string& shaderDir)
    : shaderDir_(shaderDir) {}

RenderResult GpuRenderer::render(const Scene& /*scene*/,
                                 const RenderConfig& /*config*/,
                                 ProgressCallback /*progress*/) {
    RenderResult result;
    result.success = false;
    result.errorMessage = "GPU ray tracing is not available (built without Vulkan RT support)";
    return result;
}

#else // HAVE_VULKAN_RT

#include "vulkan/vk_buffer.h"
#include "vulkan/acceleration_structure.h"
#include "vulkan/rt_pipeline.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>
#include <functional>
#include <vector>

// ── Progress stage constants ────────────────────────────────────────────────

static constexpr int kStageInit   = 1;
static constexpr int kStageUpload = 2;
static constexpr int kStageBuild  = 3;
static constexpr int kStagePipe   = 4;
static constexpr int kStageTrace  = 5;
static constexpr int kStageRead   = 6;
static constexpr int kStageTotal  = 6;

static void reportStage(const IRenderer::ProgressCallback& cb, int stage) {
    if (cb) cb(stage, kStageTotal);
}

// ── Helper: single-use command buffer execution ─────────────────────────────

static void executeCommands(VulkanContext& ctx,
                            const std::function<void(VkCommandBuffer)>& fn) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = ctx.commandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(ctx.device(), &allocInfo, &cmdBuf),
             "Failed to allocate command buffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo),
             "Failed to begin command buffer");

    fn(cmdBuf);

    VK_CHECK(vkEndCommandBuffer(cmdBuf),
             "Failed to end command buffer");

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuf;

    VK_CHECK(vkQueueSubmit(ctx.computeQueue(), 1, &submitInfo, VK_NULL_HANDLE),
             "Failed to submit command buffer");

    VK_CHECK(vkQueueWaitIdle(ctx.computeQueue()),
             "Failed to wait for queue idle");

    vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmdBuf);
}

// ── Helper: transition image layout ─────────────────────────────────────────

static void transitionImageLayout(VulkanContext& ctx,
                                  VkImage image,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout,
                                  VkAccessFlags srcAccess,
                                  VkAccessFlags dstAccess,
                                  VkPipelineStageFlags srcStage,
                                  VkPipelineStageFlags dstStage) {
    executeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = oldLayout;
        barrier.newLayout                       = newLayout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = srcAccess;
        barrier.dstAccessMask                   = dstAccess;

        vkCmdPipelineBarrier(cmdBuf, srcStage, dstStage,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    });
}

// ── RAII wrapper for VkImage + VkDeviceMemory ───────────────────────────────

struct VkImageAlloc {
    VkDevice       device = VK_NULL_HANDLE;
    VkImage        image  = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkImageAlloc() = default;

    ~VkImageAlloc() { destroy(); }

    VkImageAlloc(VkImageAlloc&& o) noexcept
        : device(o.device), image(o.image), memory(o.memory) {
        o.device = VK_NULL_HANDLE;
        o.image  = VK_NULL_HANDLE;
        o.memory = VK_NULL_HANDLE;
    }

    VkImageAlloc& operator=(VkImageAlloc&& o) noexcept {
        if (this != &o) {
            destroy();
            device = o.device; image = o.image; memory = o.memory;
            o.device = VK_NULL_HANDLE; o.image = VK_NULL_HANDLE; o.memory = VK_NULL_HANDLE;
        }
        return *this;
    }

    VkImageAlloc(const VkImageAlloc&) = delete;
    VkImageAlloc& operator=(const VkImageAlloc&) = delete;

private:
    void destroy() {
        if (device != VK_NULL_HANDLE) {
            if (image  != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr); image = VK_NULL_HANDLE; }
            if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); memory = VK_NULL_HANDLE; }
        }
    }
};

/// Create a VkImage with the given parameters and allocate + bind device-local memory.
static VkImageAlloc createImage(VulkanContext& ctx,
                                uint32_t width, uint32_t height,
                                VkFormat format,
                                VkImageUsageFlags usage) {
    VkImageCreateInfo imageCI{};
    imageCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType     = VK_IMAGE_TYPE_2D;
    imageCI.format        = format;
    imageCI.extent        = { width, height, 1 };
    imageCI.mipLevels     = 1;
    imageCI.arrayLayers   = 1;
    imageCI.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage         = usage;
    imageCI.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageAlloc alloc;
    alloc.device = ctx.device();

    VK_CHECK(vkCreateImage(ctx.device(), &imageCI, nullptr, &alloc.image),
             "Failed to create VkImage");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(ctx.device(), alloc.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = VkBufferHelper::findMemoryType(
        ctx.physicalDevice(), memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &alloc.memory),
             "Failed to allocate image memory");

    VK_CHECK(vkBindImageMemory(ctx.device(), alloc.image, alloc.memory, 0),
             "Failed to bind image memory");

    return alloc;
}

// ── RAII wrapper for VkImageView ────────────────────────────────────────────

struct VkImageViewAlloc {
    VkDevice    device = VK_NULL_HANDLE;
    VkImageView view   = VK_NULL_HANDLE;

    VkImageViewAlloc() = default;
    ~VkImageViewAlloc() {
        if (device != VK_NULL_HANDLE && view != VK_NULL_HANDLE)
            vkDestroyImageView(device, view, nullptr);
    }

    VkImageViewAlloc(VkImageViewAlloc&& o) noexcept : device(o.device), view(o.view) {
        o.device = VK_NULL_HANDLE; o.view = VK_NULL_HANDLE;
    }
    VkImageViewAlloc& operator=(VkImageViewAlloc&& o) noexcept {
        if (this != &o) {
            if (device != VK_NULL_HANDLE && view != VK_NULL_HANDLE)
                vkDestroyImageView(device, view, nullptr);
            device = o.device; view = o.view;
            o.device = VK_NULL_HANDLE; o.view = VK_NULL_HANDLE;
        }
        return *this;
    }
    VkImageViewAlloc(const VkImageViewAlloc&) = delete;
    VkImageViewAlloc& operator=(const VkImageViewAlloc&) = delete;
};

static VkImageViewAlloc createImageView(VulkanContext& ctx, VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewCI{};
    viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image                           = image;
    viewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format                          = format;
    viewCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.baseMipLevel   = 0;
    viewCI.subresourceRange.levelCount     = 1;
    viewCI.subresourceRange.baseArrayLayer = 0;
    viewCI.subresourceRange.layerCount     = 1;

    VkImageViewAlloc alloc;
    alloc.device = ctx.device();
    VK_CHECK(vkCreateImageView(ctx.device(), &viewCI, nullptr, &alloc.view),
             "Failed to create image view");
    return alloc;
}

// ── RAII wrapper for VkSampler ──────────────────────────────────────────────

struct VkSamplerAlloc {
    VkDevice  device  = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    VkSamplerAlloc() = default;
    ~VkSamplerAlloc() {
        if (device != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE)
            vkDestroySampler(device, sampler, nullptr);
    }

    VkSamplerAlloc(VkSamplerAlloc&& o) noexcept : device(o.device), sampler(o.sampler) {
        o.device = VK_NULL_HANDLE; o.sampler = VK_NULL_HANDLE;
    }
    VkSamplerAlloc& operator=(VkSamplerAlloc&& o) noexcept {
        if (this != &o) {
            if (device != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE)
                vkDestroySampler(device, sampler, nullptr);
            device = o.device; sampler = o.sampler;
            o.device = VK_NULL_HANDLE; o.sampler = VK_NULL_HANDLE;
        }
        return *this;
    }
    VkSamplerAlloc(const VkSamplerAlloc&) = delete;
    VkSamplerAlloc& operator=(const VkSamplerAlloc&) = delete;
};

static VkSamplerAlloc createNearestSampler(VulkanContext& ctx) {
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter    = VK_FILTER_NEAREST;
    samplerCI.minFilter    = VK_FILTER_NEAREST;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VkSamplerAlloc alloc;
    alloc.device = ctx.device();
    VK_CHECK(vkCreateSampler(ctx.device(), &samplerCI, nullptr, &alloc.sampler),
             "Failed to create texture sampler");
    return alloc;
}

// ── Upload skin texture to GPU ──────────────────────────────────────────────

/// Build a flat RGBA8 atlas from all mesh texture regions and upload to GPU.
/// Returns the VkImage, VkImageView, and VkSampler for the skin texture.
/// The atlas is simply the first mesh's ownedTextures[0..5] stitched together,
/// but for Minecraft skins the original 64×64 (or 64×32) image is what the
/// shaders expect.  Since we don't have the original image at this point,
/// we build a small 1×1 white fallback texture.  The closest-hit shader
/// uses per-vertex UVs that index into per-face texture regions stored in
/// the vertex/UV storage buffers, so the combined image sampler is only
/// used as a fallback.  A proper implementation would pass the original
/// skin Image through the Scene; for now we upload a 1×1 white pixel.
static void uploadTexture(VulkanContext& ctx,
                          const Scene& scene,
                          VkImageAlloc& outImage,
                          VkImageViewAlloc& outView,
                          VkSamplerAlloc& outSampler) {
    // Determine texture dimensions.  If the scene has meshes with texture
    // data we could reconstruct the atlas, but the simplest correct approach
    // is a 1×1 white pixel (the shaders use vertex-interpolated UVs and
    // per-triangle texture data via storage buffers).
    // TODO: pass original skin Image through Scene for full texture support.
    const uint32_t texW = 1;
    const uint32_t texH = 1;
    const uint8_t whitePixel[4] = { 255, 255, 255, 255 };

    const VkDeviceSize imageSize = texW * texH * 4;

    // Create staging buffer
    auto staging = VkBufferHelper::createBuffer(
        ctx, imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(ctx.device(), staging.memory(), 0, imageSize, 0, &mapped),
             "Failed to map texture staging buffer");
    std::memcpy(mapped, whitePixel, static_cast<size_t>(imageSize));
    vkUnmapMemory(ctx.device(), staging.memory());

    // Create the VkImage
    outImage = createImage(ctx, texW, texH, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    // Transition to TRANSFER_DST_OPTIMAL
    transitionImageLayout(ctx, outImage.image,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          0, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy staging buffer → image
    executeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
        VkBufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { texW, texH, 1 };

        vkCmdCopyBufferToImage(cmdBuf, staging.buffer(), outImage.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });

    // Transition to SHADER_READ_ONLY_OPTIMAL
    transitionImageLayout(ctx, outImage.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

    // Create image view and sampler
    outView    = createImageView(ctx, outImage.image, VK_FORMAT_R8G8B8A8_UNORM);
    outSampler = createNearestSampler(ctx);
}

// ── Pack RenderConfig into GPU uniform buffer ───────────────────────────────

GpuRenderUniforms packUniforms(const RenderConfig& config, const Scene& scene) {
    GpuRenderUniforms u{};

    // ── Camera matrices ─────────────────────────────────────────────────
    const auto& cam = scene.camera;
    float aspectRatio = static_cast<float>(config.width) / static_cast<float>(config.height);

    glm::vec3 eye(cam.position.x, cam.position.y, cam.position.z);
    glm::vec3 center(cam.target.x, cam.target.y, cam.target.z);
    glm::vec3 up(cam.up.x, cam.up.y, cam.up.z);

    glm::mat4 view = glm::lookAt(eye, center, up);
    float fovRad = glm::radians(cam.fov);
    glm::mat4 proj = glm::perspective(fovRad, aspectRatio, 0.001f, 10000.0f);

    u.cameraInverseView = glm::inverse(view);
    u.cameraInverseProj = glm::inverse(proj);
    u.cameraPosition    = eye;
    u.fov               = cam.fov;

    // ── Light ───────────────────────────────────────────────────────────
    u.lightPosition  = glm::vec3(scene.light.position.x,
                                 scene.light.position.y,
                                 scene.light.position.z);
    u.lightRadius    = config.lightRadius;
    u.lightColor     = glm::vec3(scene.light.color.r,
                                 scene.light.color.g,
                                 scene.light.color.b);
    u.lightIntensity = scene.light.intensity;

    // ── Render parameters ───────────────────────────────────────────────
    u.samplesPerPixel = config.samplesPerPixel;
    u.maxBounces      = config.maxBounces;
    u.shadowSamples   = config.shadowSamples;
    u.aoSamples       = config.aoSamples;

    // ── Effect toggles ──────────────────────────────────────────────────
    u.softShadows  = config.softShadows ? 1 : 0;
    u.aoEnabled    = config.aoEnabled   ? 1 : 0;
    u.aoRadius     = config.aoRadius;
    u.aoIntensity  = config.aoIntensity;
    u.dofEnabled   = config.dofEnabled  ? 1 : 0;
    u.aperture     = config.aperture;
    u.focusDistance = config.focusDistance;

    // ── Background ──────────────────────────────────────────────────────
    u.gradientBg    = config.gradientBg ? 1 : 0;
    u.gradientScale = config.gradientScale;
    u._pad0 = 0.0f;
    u._pad1 = 0.0f;
    u._pad2 = 0.0f;
    u.bgCenter = glm::vec3(config.bgCenter.r, config.bgCenter.g, config.bgCenter.b);
    u._pad3    = 0.0f;
    u.bgEdge   = glm::vec3(config.bgEdge.r, config.bgEdge.g, config.bgEdge.b);
    u._pad4    = 0.0f;

    // ── Image dimensions ────────────────────────────────────────────────
    u.width  = config.width;
    u.height = config.height;

    return u;
}

// ── Extract scene geometry into flat arrays ─────────────────────────────────

static void extractGeometry(const Scene& scene,
                            std::vector<float>& positions,
                            std::vector<float>& uvs,
                            std::vector<uint32_t>& indices) {
    // Count total triangles across all meshes.
    size_t totalTris = 0;
    for (const auto& mesh : scene.meshes)
        totalTris += mesh.triangles.size();

    const size_t totalVerts = totalTris * 3;
    positions.resize(totalVerts * 3);
    uvs.resize(totalVerts * 2);
    indices.resize(totalVerts);

    uint32_t vertIdx = 0;
    for (const auto& mesh : scene.meshes) {
        for (const auto& tri : mesh.triangles) {
            // Vertex positions (3 floats each)
            positions[vertIdx * 3 + 0] = tri.v0.x;
            positions[vertIdx * 3 + 1] = tri.v0.y;
            positions[vertIdx * 3 + 2] = tri.v0.z;

            positions[(vertIdx + 1) * 3 + 0] = tri.v1.x;
            positions[(vertIdx + 1) * 3 + 1] = tri.v1.y;
            positions[(vertIdx + 1) * 3 + 2] = tri.v1.z;

            positions[(vertIdx + 2) * 3 + 0] = tri.v2.x;
            positions[(vertIdx + 2) * 3 + 1] = tri.v2.y;
            positions[(vertIdx + 2) * 3 + 2] = tri.v2.z;

            // UV coordinates (2 floats each)
            uvs[vertIdx * 2 + 0] = tri.u0;
            uvs[vertIdx * 2 + 1] = tri.v0_uv;

            uvs[(vertIdx + 1) * 2 + 0] = tri.u1;
            uvs[(vertIdx + 1) * 2 + 1] = tri.v1_uv;

            uvs[(vertIdx + 2) * 2 + 0] = tri.u2;
            uvs[(vertIdx + 2) * 2 + 1] = tri.v2_uv;

            // Indices (trivial: 0, 1, 2, 3, 4, 5, ...)
            indices[vertIdx + 0] = vertIdx;
            indices[vertIdx + 1] = vertIdx + 1;
            indices[vertIdx + 2] = vertIdx + 2;

            vertIdx += 3;
        }
    }
}

// ── Constructor ─────────────────────────────────────────────────────────────

GpuRenderer::GpuRenderer(const std::string& shaderDir)
    : shaderDir_(shaderDir) {}

// ── Main render method ──────────────────────────────────────────────────────

RenderResult GpuRenderer::render(const Scene& scene,
                                 const RenderConfig& config,
                                 ProgressCallback progress) {
    try {
        // ── Stage 1: Initialize Vulkan context ──────────────────────────
        reportStage(progress, kStageInit);
        VulkanContext ctx;

        // ── Stage 2: Upload scene data ──────────────────────────────────
        reportStage(progress, kStageUpload);

        // Extract geometry
        std::vector<float>    positions;
        std::vector<float>    uvs;
        std::vector<uint32_t> indices;
        extractGeometry(scene, positions, uvs, indices);

        const VkDeviceSize vertexSize = positions.size() * sizeof(float);
        const VkDeviceSize uvSize     = uvs.size() * sizeof(float);
        const VkDeviceSize indexSize  = indices.size() * sizeof(uint32_t);

        auto vertexBuffer = VkBufferHelper::createDeviceLocal(
            ctx, positions.data(), vertexSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        auto uvBuffer = VkBufferHelper::createDeviceLocal(
            ctx, uvs.data(), uvSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        auto indexBuffer = VkBufferHelper::createDeviceLocal(
            ctx, indices.data(), indexSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        // Upload skin texture
        VkImageAlloc     texImage;
        VkImageViewAlloc texView;
        VkSamplerAlloc   texSampler;
        uploadTexture(ctx, scene, texImage, texView, texSampler);

        // Pack uniforms and upload
        GpuRenderUniforms uniforms = packUniforms(config, scene);
        auto uniformBuffer = VkBufferHelper::createDeviceLocal(
            ctx, &uniforms, sizeof(GpuRenderUniforms),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        // ── Stage 3: Build acceleration structures ──────────────────────
        reportStage(progress, kStageBuild);

        AccelerationStructure accelStruct;
        for (const auto& mesh : scene.meshes) {
            accelStruct.buildBLAS(ctx, mesh);
        }
        accelStruct.buildTLAS(ctx, scene.meshes);

        // ── Stage 4: Create pipeline ────────────────────────────────────
        reportStage(progress, kStagePipe);

        RTPipeline pipeline(ctx, shaderDir_);

        // Create output image
        const uint32_t outW = static_cast<uint32_t>(config.width);
        const uint32_t outH = static_cast<uint32_t>(config.height);

        auto outputImage = createImage(ctx, outW, outH, VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        auto outputView = createImageView(ctx, outputImage.image, VK_FORMAT_R8G8B8A8_UNORM);

        // Transition output image to GENERAL for shader writes
        transitionImageLayout(ctx, outputImage.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL,
                              0, VK_ACCESS_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        // Bind all resources
        pipeline.bind(ctx, accelStruct.tlas(), outputView.view,
                      uniformBuffer.buffer(),
                      vertexBuffer.buffer(), vertexSize,
                      uvBuffer.buffer(),     uvSize,
                      indexBuffer.buffer(),  indexSize,
                      texView.view, texSampler.sampler);

        // ── Stage 5: Dispatch ray tracing ───────────────────────────────
        reportStage(progress, kStageTrace);

        executeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
            pipeline.dispatch(cmdBuf, outW, outH);
        });

        // ── Stage 6: Readback results ───────────────────────────────────
        reportStage(progress, kStageRead);

        // Transition output image to TRANSFER_SRC_OPTIMAL for readback
        transitionImageLayout(ctx, outputImage.image,
                              VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                              VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Create a host-visible buffer for readback
        const VkDeviceSize readbackSize = outW * outH * 4; // RGBA8
        auto readbackBuffer = VkBufferHelper::createBuffer(
            ctx, readbackSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Copy image → buffer
        executeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
            VkBufferImageCopy region{};
            region.bufferOffset      = 0;
            region.bufferRowLength   = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel       = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = 1;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = { outW, outH, 1 };

            vkCmdCopyImageToBuffer(cmdBuf, outputImage.image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   readbackBuffer.buffer(), 1, &region);
        });

        // Map and convert to Image
        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(ctx.device(), readbackBuffer.memory(), 0, readbackSize, 0, &mapped),
                 "Failed to map readback buffer");

        Image resultImage(config.width, config.height);
        const auto* pixels = static_cast<const uint8_t*>(mapped);
        for (int i = 0; i < config.width * config.height; ++i) {
            resultImage.pixels[i] = Color(
                pixels[i * 4 + 0] / 255.0f,
                pixels[i * 4 + 1] / 255.0f,
                pixels[i * 4 + 2] / 255.0f,
                pixels[i * 4 + 3] / 255.0f
            );
        }
        vkUnmapMemory(ctx.device(), readbackBuffer.memory());

        // ── All RAII objects clean up automatically ──────────────────────
        RenderResult result;
        result.success = true;
        result.image   = std::move(resultImage);
        return result;

    } catch (const VulkanError& e) {
        RenderResult result;
        result.success      = false;
        result.errorMessage = e.what();
        return result;
    } catch (const std::exception& e) {
        RenderResult result;
        result.success      = false;
        result.errorMessage = std::string("GPU render error: ") + e.what();
        return result;
    }
}

#endif // HAVE_VULKAN_RT
