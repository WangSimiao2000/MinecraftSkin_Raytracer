#include <gtest/gtest.h>
#include "vulkan/device_capability.h"

// ─── GpuCapability struct defaults ──────────────────────────────────────────

TEST(DeviceCapability, DefaultConstructGpuCapability) {
    GpuCapability cap;
    EXPECT_FALSE(cap.available);
    EXPECT_TRUE(cap.deviceName.empty());
}

// ─── Detection tests (work in both stub and real Vulkan modes) ──────────────

TEST(DeviceCapability, DetectReturnsValidResult) {
    GpuCapability cap = DeviceCapabilityDetector::detect();
    // In stub mode (VULKAN_RT off): available == false, deviceName empty.
    // In real mode: available may be true or false depending on hardware.
    if (cap.available) {
        EXPECT_FALSE(cap.deviceName.empty());
    } else {
        // Device name may or may not be empty when unavailable
        SUCCEED();
    }
}

TEST(DeviceCapability, DetectDoesNotCrashOnRepeatedCalls) {
    // Call detect() multiple times — must never throw or crash.
    for (int i = 0; i < 10; ++i) {
        GpuCapability cap = DeviceCapabilityDetector::detect();
        (void)cap; // just ensure no crash
    }
    SUCCEED();
}
