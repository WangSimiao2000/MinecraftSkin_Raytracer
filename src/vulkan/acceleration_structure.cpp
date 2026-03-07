#ifdef HAVE_VULKAN_RT

#include "vulkan/acceleration_structure.h"

#include <cmath>
#include <cstring>
#include <functional>

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
             "Failed to allocate command buffer for AS build");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo),
             "Failed to begin command buffer for AS build");

    fn(cmdBuf);

    VK_CHECK(vkEndCommandBuffer(cmdBuf),
             "Failed to end command buffer for AS build");

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuf;

    VK_CHECK(vkQueueSubmit(ctx.computeQueue(), 1, &submitInfo, VK_NULL_HANDLE),
             "Failed to submit AS build command buffer");

    VK_CHECK(vkQueueWaitIdle(ctx.computeQueue()),
             "Failed to wait for AS build completion");

    vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmdBuf);
}

// ── Pose → VkTransformMatrixKHR ─────────────────────────────────────────────

void AccelerationStructure::cacheDeviceFunctions(VulkanContext& ctx) {
    if (!pfnCreateAS_) {
        pfnCreateAS_      = ctx.vkCreateAccelerationStructureKHR;
        pfnDestroyAS_     = ctx.vkDestroyAccelerationStructureKHR;
        pfnGetBuildSizes_ = ctx.vkGetAccelerationStructureBuildSizesKHR;
        pfnGetASAddr_     = ctx.vkGetAccelerationStructureDeviceAddressKHR;
        pfnCmdBuildAS_    = ctx.vkCmdBuildAccelerationStructuresKHR;
    }
}

VkTransformMatrixKHR AccelerationStructure::poseToTransform(const Mesh& mesh) {
    // Start with identity.
    VkTransformMatrixKHR xform{};
    xform.matrix[0][0] = 1.0f;
    xform.matrix[1][1] = 1.0f;
    xform.matrix[2][2] = 1.0f;

    if (!mesh.hasRotation)
        return xform;

    // Convert degrees → radians.
    const float degToRad = 3.14159265358979323846f / 180.0f;
    const float rx = mesh.rotX * degToRad;
    const float ry = mesh.rotY * degToRad;
    const float rz = mesh.rotZ * degToRad;

    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);
    const float cz = std::cos(rz), sz = std::sin(rz);

    // Combined rotation matrix R = Rz * Ry * Rx  (row-major 3×3).
    const float r00 = cy * cz;
    const float r01 = cz * sx * sy - cx * sz;
    const float r02 = sx * sz + cx * cz * sy;

    const float r10 = cy * sz;
    const float r11 = cx * cz + sx * sy * sz;
    const float r12 = cx * sy * sz - cz * sx;

    const float r20 = -sy;
    const float r21 = cy * sx;
    const float r22 = cx * cy;

    // Translation = pivot - R * pivot  (rotate around pivot).
    const float px = mesh.pivot.x;
    const float py = mesh.pivot.y;
    const float pz = mesh.pivot.z;

    const float tx = px - (r00 * px + r01 * py + r02 * pz);
    const float ty = py - (r10 * px + r11 * py + r12 * pz);
    const float tz = pz - (r20 * px + r21 * py + r22 * pz);

    // Fill the 3×4 row-major matrix.
    xform.matrix[0][0] = r00;  xform.matrix[0][1] = r01;  xform.matrix[0][2] = r02;  xform.matrix[0][3] = tx;
    xform.matrix[1][0] = r10;  xform.matrix[1][1] = r11;  xform.matrix[1][2] = r12;  xform.matrix[1][3] = ty;
    xform.matrix[2][0] = r20;  xform.matrix[2][1] = r21;  xform.matrix[2][2] = r22;  xform.matrix[2][3] = tz;

    // If there's also a torso hierarchical transform, compose it.
    if (mesh.hasTorsoTransform) {
        const float trx = mesh.torsoRotX * degToRad;
        const float try_ = mesh.torsoRotY * degToRad;
        const float trz = mesh.torsoRotZ * degToRad;

        const float tcx = std::cos(trx), tsx = std::sin(trx);
        const float tcy = std::cos(try_), tsy = std::sin(try_);
        const float tcz = std::cos(trz), tsz = std::sin(trz);

        // Torso rotation matrix T = Tz * Ty * Tx
        float t00 = tcy * tcz;
        float t01 = tcz * tsx * tsy - tcx * tsz;
        float t02 = tsx * tsz + tcx * tcz * tsy;

        float t10 = tcy * tsz;
        float t11 = tcx * tcz + tsx * tsy * tsz;
        float t12 = tcx * tsy * tsz - tcz * tsx;

        float t20 = -tsy;
        float t21 = tcy * tsx;
        float t22 = tcx * tcy;

        // Torso translation: translate + rotate around torsoPivot
        const float tpx = mesh.torsoPivot.x;
        const float tpy = mesh.torsoPivot.y;
        const float tpz = mesh.torsoPivot.z;

        float ttx = tpx - (t00 * tpx + t01 * tpy + t02 * tpz) + mesh.torsoTranslation.x;
        float tty = tpy - (t10 * tpx + t11 * tpy + t12 * tpz) + mesh.torsoTranslation.y;
        float ttz = tpz - (t20 * tpx + t21 * tpy + t22 * tpz) + mesh.torsoTranslation.z;

        // Compose: result = T * current
        // current is stored in xform.matrix (3×4 row-major)
        float c[3][4];
        std::memcpy(c, xform.matrix, sizeof(c));

        for (int row = 0; row < 3; ++row) {
            float tRow[3];
            if (row == 0)      { tRow[0] = t00; tRow[1] = t01; tRow[2] = t02; }
            else if (row == 1) { tRow[0] = t10; tRow[1] = t11; tRow[2] = t12; }
            else               { tRow[0] = t20; tRow[1] = t21; tRow[2] = t22; }

            float tTrans = (row == 0) ? ttx : (row == 1) ? tty : ttz;

            for (int col = 0; col < 3; ++col) {
                xform.matrix[row][col] = tRow[0] * c[0][col]
                                       + tRow[1] * c[1][col]
                                       + tRow[2] * c[2][col];
            }
            // Translation column
            xform.matrix[row][3] = tRow[0] * c[0][3]
                                 + tRow[1] * c[1][3]
                                 + tRow[2] * c[2][3]
                                 + tTrans;
        }
    }

    return xform;
}

// ── BLAS construction ───────────────────────────────────────────────────────

void AccelerationStructure::buildBLAS(VulkanContext& ctx, const Mesh& mesh) {
    device_ = ctx.device();
    cacheDeviceFunctions(ctx);

    const auto& tris = mesh.triangles;
    if (tris.empty()) return;

    // ── Extract vertex positions and build an index buffer ──────────────
    const uint32_t vertexCount = static_cast<uint32_t>(tris.size()) * 3;
    const uint32_t indexCount  = vertexCount;

    std::vector<float>    positions(vertexCount * 3);
    std::vector<uint32_t> indices(indexCount);

    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& t = tris[i];
        const uint32_t base = static_cast<uint32_t>(i) * 3;

        positions[base * 3 + 0] = t.v0.x;
        positions[base * 3 + 1] = t.v0.y;
        positions[base * 3 + 2] = t.v0.z;

        positions[(base + 1) * 3 + 0] = t.v1.x;
        positions[(base + 1) * 3 + 1] = t.v1.y;
        positions[(base + 1) * 3 + 2] = t.v1.z;

        positions[(base + 2) * 3 + 0] = t.v2.x;
        positions[(base + 2) * 3 + 1] = t.v2.y;
        positions[(base + 2) * 3 + 2] = t.v2.z;

        indices[base + 0] = base;
        indices[base + 1] = base + 1;
        indices[base + 2] = base + 2;
    }

    // ── Upload to GPU ───────────────────────────────────────────────────
    const VkDeviceSize vertexSize = positions.size() * sizeof(float);
    const VkDeviceSize indexSize  = indices.size() * sizeof(uint32_t);

    auto vertexBuffer = VkBufferHelper::createDeviceLocal(
        ctx, positions.data(), vertexSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    auto indexBuffer = VkBufferHelper::createDeviceLocal(
        ctx, indices.data(), indexSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    // ── Describe the triangle geometry ──────────────────────────────────
    VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{};
    trianglesData.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    trianglesData.vertexData.deviceAddress =
        VkBufferHelper::getBufferDeviceAddress(ctx, vertexBuffer.buffer());
    trianglesData.vertexStride = 3 * sizeof(float);
    trianglesData.maxVertex    = vertexCount - 1;
    trianglesData.indexType    = VK_INDEX_TYPE_UINT32;
    trianglesData.indexData.deviceAddress =
        VkBufferHelper::getBufferDeviceAddress(ctx, indexBuffer.buffer());

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = trianglesData;

    // ── Query build sizes ───────────────────────────────────────────────
    const uint32_t primitiveCount = static_cast<uint32_t>(tris.size());

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    pfnGetBuildSizes_(
        ctx.device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    // ── Create the AS backing buffer and the AS object ──────────────────
    auto asBuffer = VkBufferHelper::createBuffer(
        ctx, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = asBuffer.buffer();
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    VK_CHECK(pfnCreateAS_(ctx.device(), &createInfo, nullptr, &blas),
             "Failed to create BLAS");

    // ── Scratch buffer ──────────────────────────────────────────────────
    auto scratchBuffer = VkBufferHelper::createBuffer(
        ctx, sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // ── Build ───────────────────────────────────────────────────────────
    buildInfo.dstAccelerationStructure  = blas;
    buildInfo.scratchData.deviceAddress =
        VkBufferHelper::getBufferDeviceAddress(ctx, scratchBuffer.buffer());

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    executeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
        pfnCmdBuildAS_(cmdBuf, 1, &buildInfo, &pRangeInfo);
    });

    // ── Get device address for TLAS referencing ─────────────────────────
    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = blas;
    VkDeviceAddress blasAddr = pfnGetASAddr_(ctx.device(), &addrInfo);

    // ── Store the entry ─────────────────────────────────────────────────
    BLASEntry entry;
    entry.handle        = blas;
    entry.buffer        = std::move(asBuffer);
    entry.deviceAddress = blasAddr;
    blasEntries_.push_back(std::move(entry));

    // vertexBuffer, indexBuffer, scratchBuffer are freed here (RAII).
}

// ── TLAS construction ───────────────────────────────────────────────────────

void AccelerationStructure::buildTLAS(VulkanContext& ctx,
                                      const std::vector<Mesh>& meshes) {
    device_ = ctx.device();
    cacheDeviceFunctions(ctx);

    if (blasEntries_.empty()) return;

    const uint32_t instanceCount = static_cast<uint32_t>(blasEntries_.size());

    // ── Build instance descriptors ──────────────────────────────────────
    std::vector<VkAccelerationStructureInstanceKHR> instances(instanceCount);

    for (uint32_t i = 0; i < instanceCount; ++i) {
        auto& inst = instances[i];
        std::memset(&inst, 0, sizeof(inst));

        // Use the mesh's pose to derive the transform.
        inst.transform = (i < meshes.size()) ? poseToTransform(meshes[i])
                                             : VkTransformMatrixKHR{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};

        inst.instanceCustomIndex                    = i;
        inst.mask                                   = 0xFF;
        inst.instanceShaderBindingTableRecordOffset  = 0;
        inst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        inst.accelerationStructureReference         = blasEntries_[i].deviceAddress;
    }

    // ── Upload instances to GPU ─────────────────────────────────────────
    const VkDeviceSize instanceSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    instanceBuffer_ = VkBufferHelper::createDeviceLocal(
        ctx, instances.data(), instanceSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    // ── Describe the instance geometry ──────────────────────────────────
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.arrayOfPointers = VK_FALSE;
    instancesData.data.deviceAddress =
        VkBufferHelper::getBufferDeviceAddress(ctx, instanceBuffer_.buffer());

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances = instancesData;

    // ── Query build sizes ───────────────────────────────────────────────
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    pfnGetBuildSizes_(
        ctx.device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    // ── Create the TLAS backing buffer and AS object ────────────────────
    tlasBuffer_ = VkBufferHelper::createBuffer(
        ctx, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = tlasBuffer_.buffer();
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VK_CHECK(pfnCreateAS_(ctx.device(), &createInfo, nullptr, &tlas_),
             "Failed to create TLAS");

    // ── Scratch buffer ──────────────────────────────────────────────────
    auto scratchBuffer = VkBufferHelper::createBuffer(
        ctx, sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // ── Build ───────────────────────────────────────────────────────────
    buildInfo.dstAccelerationStructure  = tlas_;
    buildInfo.scratchData.deviceAddress =
        VkBufferHelper::getBufferDeviceAddress(ctx, scratchBuffer.buffer());

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = instanceCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    executeCommands(ctx, [&](VkCommandBuffer cmdBuf) {
        pfnCmdBuildAS_(cmdBuf, 1, &buildInfo, &pRangeInfo);
    });

    // scratchBuffer freed here (RAII).
}

// ── Cleanup ─────────────────────────────────────────────────────────────────

void AccelerationStructure::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;
    if (!pfnDestroyAS_) return;

    // Destroy TLAS first (it references BLASes).
    if (tlas_ != VK_NULL_HANDLE) {
        pfnDestroyAS_(device_, tlas_, nullptr);
        tlas_ = VK_NULL_HANDLE;
    }
    // tlasBuffer_ and instanceBuffer_ are freed by their destructors.
    tlasBuffer_     = VkBufferAlloc{};
    instanceBuffer_ = VkBufferAlloc{};

    // Destroy all BLASes.
    for (auto& entry : blasEntries_) {
        if (entry.handle != VK_NULL_HANDLE) {
            pfnDestroyAS_(device_, entry.handle, nullptr);
            entry.handle = VK_NULL_HANDLE;
        }
        // entry.buffer freed by its destructor via move-assign to default.
    }
    blasEntries_.clear();

    device_ = VK_NULL_HANDLE;
}

AccelerationStructure::~AccelerationStructure() {
    cleanup();
}

// ── Move operations ─────────────────────────────────────────────────────────

AccelerationStructure::AccelerationStructure(AccelerationStructure&& other) noexcept
    : device_(other.device_)
    , pfnCreateAS_(other.pfnCreateAS_)
    , pfnDestroyAS_(other.pfnDestroyAS_)
    , pfnGetBuildSizes_(other.pfnGetBuildSizes_)
    , pfnGetASAddr_(other.pfnGetASAddr_)
    , pfnCmdBuildAS_(other.pfnCmdBuildAS_)
    , blasEntries_(std::move(other.blasEntries_))
    , tlas_(other.tlas_)
    , tlasBuffer_(std::move(other.tlasBuffer_))
    , instanceBuffer_(std::move(other.instanceBuffer_))
{
    other.device_ = VK_NULL_HANDLE;
    other.tlas_   = VK_NULL_HANDLE;
}

AccelerationStructure& AccelerationStructure::operator=(AccelerationStructure&& other) noexcept {
    if (this != &other) {
        cleanup();
        device_         = other.device_;
        pfnCreateAS_    = other.pfnCreateAS_;
        pfnDestroyAS_   = other.pfnDestroyAS_;
        pfnGetBuildSizes_ = other.pfnGetBuildSizes_;
        pfnGetASAddr_   = other.pfnGetASAddr_;
        pfnCmdBuildAS_  = other.pfnCmdBuildAS_;
        blasEntries_    = std::move(other.blasEntries_);
        tlas_           = other.tlas_;
        tlasBuffer_     = std::move(other.tlasBuffer_);
        instanceBuffer_ = std::move(other.instanceBuffer_);
        other.device_   = VK_NULL_HANDLE;
        other.tlas_     = VK_NULL_HANDLE;
    }
    return *this;
}

#endif // HAVE_VULKAN_RT
