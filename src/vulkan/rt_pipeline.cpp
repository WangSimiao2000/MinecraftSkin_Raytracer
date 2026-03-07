#ifdef HAVE_VULKAN_RT

#include "vulkan/rt_pipeline.h"

#include <algorithm>
#include <fstream>
#include <vector>

// ── Helper: align a value up to the given alignment ─────────────────────────

static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// ── SPIR-V file loading ─────────────────────────────────────────────────────

VkShaderModule RTPipeline::loadShaderModule(VkDevice device,
                                            const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw VulkanError("Failed to open shader file: " + path,
                          VK_ERROR_INITIALIZATION_FAILED);
    }

    auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || (fileSize % 4) != 0) {
        throw VulkanError("Invalid SPIR-V file size: " + path,
                          VK_ERROR_INITIALIZATION_FAILED);
    }

    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()),
              static_cast<std::streamsize>(fileSize));
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode    = code.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
             "Failed to create shader module from: " + path);

    return shaderModule;
}

// ── Descriptor set layout ───────────────────────────────────────────────────

void RTPipeline::createDescriptorSetLayout(VkDevice device) {
    // Matches the shader bindings in raygen.rgen / closesthit.rchit:
    //   0 – acceleration structure (TLAS)
    //   1 – storage image (output)
    //   2 – uniform buffer (RenderUniforms)
    //   3 – storage buffer (vertex positions)
    //   4 – storage buffer (UV coordinates)
    //   5 – storage buffer (index buffer)
    //   6 – combined image sampler (skin texture)

    VkDescriptorSetLayoutBinding bindings[7] = {};

    // binding 0: TLAS
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // binding 1: output image
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // binding 2: uniform buffer
    bindings[2].binding         = 2;
    bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                                | VK_SHADER_STAGE_MISS_BIT_KHR;

    // binding 3: vertex positions (storage buffer)
    bindings[3].binding         = 3;
    bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // binding 4: UV coordinates (storage buffer)
    bindings[4].binding         = 4;
    bindings[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // binding 5: index buffer (storage buffer)
    bindings[5].binding         = 5;
    bindings[5].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // binding 6: skin texture (combined image sampler)
    bindings[6].binding         = 6;
    bindings[6].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutCI{};
    layoutCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCI.bindingCount = 7;
    layoutCI.pBindings    = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr,
                                         &descriptorSetLayout_),
             "Failed to create descriptor set layout");
}

// ── Pipeline creation ───────────────────────────────────────────────────────

void RTPipeline::createPipeline(VkDevice device,
                                VkShaderModule raygenModule,
                                VkShaderModule chitModule,
                                VkShaderModule missModule) {
    // ── Pipeline layout ─────────────────────────────────────────────────
    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts    = &descriptorSetLayout_;

    VK_CHECK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &pipelineLayout_),
             "Failed to create pipeline layout");

    // ── Shader stages ───────────────────────────────────────────────────
    // 0: raygen, 1: closest-hit, 2: miss
    VkPipelineShaderStageCreateInfo stages[3] = {};

    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[0].module = raygenModule;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[1].module = chitModule;
    stages[1].pName  = "main";

    stages[2].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[2].stage  = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[2].module = missModule;
    stages[2].pName  = "main";

    // ── Shader groups ───────────────────────────────────────────────────
    // Group 0: raygen
    // Group 1: hit (closest-hit at stage index 1)
    // Group 2: miss
    VkRayTracingShaderGroupCreateInfoKHR groups[3] = {};

    // Raygen group
    groups[0].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader      = 0; // stage index 0
    groups[0].closestHitShader   = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader       = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

    // Hit group (triangles)
    groups[1].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[1].generalShader      = VK_SHADER_UNUSED_KHR;
    groups[1].closestHitShader   = 1; // stage index 1
    groups[1].anyHitShader       = VK_SHADER_UNUSED_KHR;
    groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

    // Miss group
    groups[2].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[2].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[2].generalShader      = 2; // stage index 2
    groups[2].closestHitShader   = VK_SHADER_UNUSED_KHR;
    groups[2].anyHitShader       = VK_SHADER_UNUSED_KHR;
    groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

    // ── Create the ray tracing pipeline ─────────────────────────────────
    VkRayTracingPipelineCreateInfoKHR pipelineCI{};
    pipelineCI.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineCI.stageCount                   = 3;
    pipelineCI.pStages                      = stages;
    pipelineCI.groupCount                   = 3;
    pipelineCI.pGroups                      = groups;
    pipelineCI.maxPipelineRayRecursionDepth = 2; // primary + one bounce
    pipelineCI.layout                       = pipelineLayout_;

    // vkCreateRayTracingPipelinesKHR is a device-level function that may
    // need to be loaded dynamically. On most loaders it is available via
    // the standard dispatch table after enabling the extension.
    auto vkCreateRayTracingPipelinesKHR_ =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
            vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

    if (!vkCreateRayTracingPipelinesKHR_) {
        throw VulkanError("Failed to load vkCreateRayTracingPipelinesKHR",
                          VK_ERROR_EXTENSION_NOT_PRESENT);
    }

    VK_CHECK(vkCreateRayTracingPipelinesKHR_(device, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                              1, &pipelineCI, nullptr, &pipeline_),
             "Failed to create ray tracing pipeline");
}

// ── Shader Binding Table ────────────────────────────────────────────────────

void RTPipeline::createSBT(VulkanContext& ctx) {
    const auto& props = ctx.rtPipelineProperties();

    const VkDeviceSize handleSize      = props.shaderGroupHandleSize;
    const VkDeviceSize handleAlignment = props.shaderGroupHandleAlignment;
    const VkDeviceSize baseAlignment   = props.shaderGroupBaseAlignment;

    // Each SBT entry is aligned to handleAlignment.
    const VkDeviceSize handleSizeAligned = alignUp(handleSize, handleAlignment);

    // SBT layout: [raygen | miss | hit]
    // Each region is aligned to baseAlignment.
    const VkDeviceSize raygenRegionSize = alignUp(handleSizeAligned, baseAlignment);
    const VkDeviceSize missRegionSize   = alignUp(handleSizeAligned, baseAlignment);
    const VkDeviceSize hitRegionSize    = alignUp(handleSizeAligned, baseAlignment);

    const VkDeviceSize sbtSize = raygenRegionSize + missRegionSize + hitRegionSize;

    // ── Retrieve shader group handles from the pipeline ─────────────────
    const uint32_t groupCount  = 3;
    const uint32_t handleBytes = groupCount * static_cast<uint32_t>(handleSize);
    std::vector<uint8_t> handles(handleBytes);

    auto vkGetRayTracingShaderGroupHandlesKHR_ =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(ctx.device(),
                                "vkGetRayTracingShaderGroupHandlesKHR"));

    if (!vkGetRayTracingShaderGroupHandlesKHR_) {
        throw VulkanError("Failed to load vkGetRayTracingShaderGroupHandlesKHR",
                          VK_ERROR_EXTENSION_NOT_PRESENT);
    }

    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR_(
                 ctx.device(), pipeline_, 0, groupCount, handleBytes, handles.data()),
             "Failed to get ray tracing shader group handles");

    // ── Create SBT buffer (host-visible for simplicity) ─────────────────
    sbtBuffer_ = VkBufferHelper::createBuffer(
        ctx, sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // ── Map and fill the SBT ────────────────────────────────────────────
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(ctx.device(), sbtBuffer_.memory(), 0, sbtSize, 0, &mapped),
             "Failed to map SBT buffer");

    auto* dst = static_cast<uint8_t*>(mapped);

    // Group 0 → raygen region
    std::memcpy(dst, handles.data() + 0 * handleSize, handleSize);

    // Group 2 → miss region  (group order: raygen=0, hit=1, miss=2)
    std::memcpy(dst + raygenRegionSize,
                handles.data() + 2 * handleSize, handleSize);

    // Group 1 → hit region
    std::memcpy(dst + raygenRegionSize + missRegionSize,
                handles.data() + 1 * handleSize, handleSize);

    vkUnmapMemory(ctx.device(), sbtBuffer_.memory());

    // ── Fill strided device address regions ─────────────────────────────
    VkDeviceAddress sbtAddr =
        VkBufferHelper::getBufferDeviceAddress(ctx, sbtBuffer_.buffer());

    raygenRegion_.deviceAddress = sbtAddr;
    raygenRegion_.stride        = handleSizeAligned;
    raygenRegion_.size          = raygenRegionSize;

    missRegion_.deviceAddress = sbtAddr + raygenRegionSize;
    missRegion_.stride        = handleSizeAligned;
    missRegion_.size          = missRegionSize;

    hitRegion_.deviceAddress = sbtAddr + raygenRegionSize + missRegionSize;
    hitRegion_.stride        = handleSizeAligned;
    hitRegion_.size          = hitRegionSize;

    // callableRegion_ stays zeroed (no callable shaders).
}

// ── Descriptor pool and set ─────────────────────────────────────────────────

void RTPipeline::createDescriptorPoolAndSet(VkDevice device) {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             3 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1 },
    };

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.maxSets       = 1;
    poolCI.poolSizeCount = static_cast<uint32_t>(
        sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolCI.pPoolSizes    = poolSizes;

    VK_CHECK(vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool_),
             "Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &descriptorSetLayout_;

    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet_),
             "Failed to allocate descriptor set");
}

// ── Bind resources ──────────────────────────────────────────────────────────

void RTPipeline::bind(VulkanContext& ctx,
                      VkAccelerationStructureKHR tlas,
                      VkImageView outputImageView,
                      VkBuffer uniformBuffer,
                      VkBuffer vertexBuffer,   VkDeviceSize vertexBufferSize,
                      VkBuffer uvBuffer,       VkDeviceSize uvBufferSize,
                      VkBuffer indexBuffer,    VkDeviceSize indexBufferSize,
                      VkImageView textureView, VkSampler textureSampler) {
    VkDevice device = ctx.device();

    // ── binding 0: acceleration structure ────────────────────────────────
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures    = &tlas;

    // ── binding 1: output image ─────────────────────────────────────────
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView   = outputImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // ── binding 2: uniform buffer ───────────────────────────────────────
    VkDescriptorBufferInfo uniformInfo{};
    uniformInfo.buffer = uniformBuffer;
    uniformInfo.offset = 0;
    uniformInfo.range  = VK_WHOLE_SIZE;

    // ── binding 3: vertex positions ─────────────────────────────────────
    VkDescriptorBufferInfo vertexInfo{};
    vertexInfo.buffer = vertexBuffer;
    vertexInfo.offset = 0;
    vertexInfo.range  = vertexBufferSize;

    // ── binding 4: UV coordinates ───────────────────────────────────────
    VkDescriptorBufferInfo uvInfo{};
    uvInfo.buffer = uvBuffer;
    uvInfo.offset = 0;
    uvInfo.range  = uvBufferSize;

    // ── binding 5: index buffer ─────────────────────────────────────────
    VkDescriptorBufferInfo indexInfo{};
    indexInfo.buffer = indexBuffer;
    indexInfo.offset = 0;
    indexInfo.range  = indexBufferSize;

    // ── binding 6: skin texture ─────────────────────────────────────────
    VkDescriptorImageInfo textureInfo{};
    textureInfo.sampler     = textureSampler;
    textureInfo.imageView   = textureView;
    textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // ── Write descriptors ───────────────────────────────────────────────
    VkWriteDescriptorSet writes[7] = {};

    // 0: TLAS
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext           = &asWrite;
    writes[0].dstSet          = descriptorSet_;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    // 1: output image
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = descriptorSet_;
    writes[1].dstBinding      = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo      = &imageInfo;

    // 2: uniform buffer
    writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet          = descriptorSet_;
    writes[2].dstBinding      = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo     = &uniformInfo;

    // 3: vertex positions
    writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet          = descriptorSet_;
    writes[3].dstBinding      = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo     = &vertexInfo;

    // 4: UV coordinates
    writes[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet          = descriptorSet_;
    writes[4].dstBinding      = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[4].pBufferInfo     = &uvInfo;

    // 5: index buffer
    writes[5].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet          = descriptorSet_;
    writes[5].dstBinding      = 5;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[5].pBufferInfo     = &indexInfo;

    // 6: skin texture
    writes[6].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[6].dstSet          = descriptorSet_;
    writes[6].dstBinding      = 6;
    writes[6].descriptorCount = 1;
    writes[6].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[6].pImageInfo      = &textureInfo;

    vkUpdateDescriptorSets(device, 7, writes, 0, nullptr);
}

// ── Dispatch (record trace rays command) ────────────────────────────────────

void RTPipeline::dispatch(VkCommandBuffer cmdBuf,
                          uint32_t width, uint32_t height) const {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);

    auto vkCmdTraceRaysKHR_ =
        reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
            vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));

    if (!vkCmdTraceRaysKHR_) {
        throw VulkanError("Failed to load vkCmdTraceRaysKHR",
                          VK_ERROR_EXTENSION_NOT_PRESENT);
    }

    vkCmdTraceRaysKHR_(cmdBuf,
                        &raygenRegion_,
                        &missRegion_,
                        &hitRegion_,
                        &callableRegion_,
                        width, height, 1);
}

// ── Constructor ─────────────────────────────────────────────────────────────

RTPipeline::RTPipeline(VulkanContext& ctx, const std::string& shaderDir) {
    device_ = ctx.device();

    try {
        // Ensure trailing separator
        std::string dir = shaderDir;
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
            dir += '/';

        raygenModule_ = loadShaderModule(device_, dir + "raygen.rgen.spv");
        chitModule_   = loadShaderModule(device_, dir + "closesthit.rchit.spv");
        missModule_   = loadShaderModule(device_, dir + "miss.rmiss.spv");

        createDescriptorSetLayout(device_);
        createPipeline(device_, raygenModule_, chitModule_, missModule_);
        createSBT(ctx);
        createDescriptorPoolAndSet(device_);
    } catch (...) {
        cleanup();
        throw;
    }
}

// ── Destructor / cleanup ────────────────────────────────────────────────────

RTPipeline::~RTPipeline() {
    cleanup();
}

void RTPipeline::cleanup() {
    if (device_ == VK_NULL_HANDLE)
        return;

    // Descriptor pool (frees the set implicitly)
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
        descriptorSet_  = VK_NULL_HANDLE;
    }

    // SBT buffer is cleaned up by VkBufferAlloc RAII (move-assign empty).
    sbtBuffer_ = VkBufferAlloc{};

    // Pipeline
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    // Descriptor set layout
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }

    // Shader modules
    if (raygenModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, raygenModule_, nullptr);
        raygenModule_ = VK_NULL_HANDLE;
    }
    if (chitModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, chitModule_, nullptr);
        chitModule_ = VK_NULL_HANDLE;
    }
    if (missModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, missModule_, nullptr);
        missModule_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
}

#endif // HAVE_VULKAN_RT
