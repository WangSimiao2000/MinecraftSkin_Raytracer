#pragma once

#include <string>

/// Result of GPU ray-tracing capability detection.
struct GpuCapability {
    bool available = false;
    std::string deviceName;
};

/// Detects whether the system has a Vulkan-capable GPU that supports
/// the hardware ray-tracing extensions (VK_KHR_ray_tracing_pipeline
/// and VK_KHR_acceleration_structure).
class DeviceCapabilityDetector {
public:
    /// Enumerate physical devices and check for RT extension support.
    /// Returns safely even when Vulkan is not installed — never throws.
    static GpuCapability detect();
};

// ── Stub implementation when Vulkan RT is compiled out ──────────────────────
#ifndef HAVE_VULKAN_RT
inline GpuCapability DeviceCapabilityDetector::detect() {
    return {false, {}};
}
#endif
