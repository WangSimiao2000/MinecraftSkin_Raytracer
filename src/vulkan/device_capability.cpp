// Device capability detection — works in both HAVE_VULKAN_RT and non-Vulkan builds.
// Uses dynamic loading so detection works even without a Vulkan runtime.

#ifdef HAVE_VULKAN_RT

#include "vulkan/device_capability.h"

#include <vulkan/vulkan.h>
#include <cstring>
#include <vector>

// ── Platform dynamic-library helpers ────────────────────────────────────────
#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
    using LibHandle = HMODULE;
    static LibHandle loadLib()            { return LoadLibraryA("vulkan-1.dll"); }
    static void      freeLib(LibHandle h) { if (h) FreeLibrary(h); }
    template<typename F>
    static F getProc(LibHandle h, const char* name) {
        return reinterpret_cast<F>(GetProcAddress(h, name));
    }
#else
#   include <dlfcn.h>
    using LibHandle = void*;
    static LibHandle loadLib() {
        LibHandle h = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!h) h = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        return h;
    }
    static void freeLib(LibHandle h) { if (h) dlclose(h); }
    template<typename F>
    static F getProc(LibHandle h, const char* name) {
        return reinterpret_cast<F>(dlsym(h, name));
    }
#endif

// ── Function-pointer typedefs ───────────────────────────────────────────────
using PFN_vkCreateInstance_                    = VkResult(*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
using PFN_vkDestroyInstance_                   = void(*)(VkInstance, const VkAllocationCallbacks*);
using PFN_vkEnumeratePhysicalDevices_          = VkResult(*)(VkInstance, uint32_t*, VkPhysicalDevice*);
using PFN_vkGetPhysicalDeviceProperties_       = void(*)(VkPhysicalDevice, VkPhysicalDeviceProperties*);
using PFN_vkEnumerateDeviceExtensionProperties_ = VkResult(*)(VkPhysicalDevice, const char*, uint32_t*, VkExtensionProperties*);

// ── Required extensions ─────────────────────────────────────────────────────
static const char* const kRequiredExtensions[] = {
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_acceleration_structure",
};
constexpr size_t kRequiredExtCount = sizeof(kRequiredExtensions) / sizeof(kRequiredExtensions[0]);

// ── Helper: check if a device supports all required extensions ──────────────
static bool deviceSupportsRT(
    PFN_vkEnumerateDeviceExtensionProperties_ enumExts,
    VkPhysicalDevice device)
{
    uint32_t count = 0;
    if (enumExts(device, nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
        return false;

    std::vector<VkExtensionProperties> exts(count);
    if (enumExts(device, nullptr, &count, exts.data()) != VK_SUCCESS)
        return false;

    size_t found = 0;
    for (size_t r = 0; r < kRequiredExtCount; ++r) {
        for (uint32_t e = 0; e < count; ++e) {
            if (std::strcmp(exts[e].extensionName, kRequiredExtensions[r]) == 0) {
                ++found;
                break;
            }
        }
    }
    return found == kRequiredExtCount;
}

// ── DeviceCapabilityDetector::detect() ──────────────────────────────────────
GpuCapability DeviceCapabilityDetector::detect()
{
    GpuCapability result;

    // 1. Dynamically load the Vulkan loader library.
    LibHandle lib = loadLib();
    if (!lib)
        return result; // Vulkan runtime not installed — that's fine.

    // 2. Resolve the handful of entry points we need.
    auto vkCreateInstance   = getProc<PFN_vkCreateInstance_>(lib, "vkCreateInstance");
    auto vkDestroyInstance  = getProc<PFN_vkDestroyInstance_>(lib, "vkDestroyInstance");
    auto vkEnumDevices      = getProc<PFN_vkEnumeratePhysicalDevices_>(lib, "vkEnumeratePhysicalDevices");
    auto vkGetDeviceProps   = getProc<PFN_vkGetPhysicalDeviceProperties_>(lib, "vkGetPhysicalDeviceProperties");
    auto vkEnumDeviceExts   = getProc<PFN_vkEnumerateDeviceExtensionProperties_>(lib, "vkEnumerateDeviceExtensionProperties");

    if (!vkCreateInstance || !vkDestroyInstance || !vkEnumDevices ||
        !vkGetDeviceProps || !vkEnumDeviceExts) {
        freeLib(lib);
        return result;
    }

    // 3. Create a minimal Vulkan instance (no extensions, no layers).
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "MCSkin_RTDetect";
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS || !instance) {
        freeLib(lib);
        return result;
    }

    // 4. Enumerate physical devices.
    uint32_t deviceCount = 0;
    if (vkEnumDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
        vkDestroyInstance(instance, nullptr);
        freeLib(lib);
        return result;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        freeLib(lib);
        return result;
    }

    // 5. Find the first device that supports both RT extensions.
    for (uint32_t i = 0; i < deviceCount; ++i) {
        if (deviceSupportsRT(vkEnumDeviceExts, devices[i])) {
            VkPhysicalDeviceProperties props{};
            vkGetDeviceProps(devices[i], &props);

            result.available   = true;
            result.deviceName  = props.deviceName;
            break;
        }
    }

    // 6. Cleanup.
    vkDestroyInstance(instance, nullptr);
    freeLib(lib);
    return result;
}

#endif // HAVE_VULKAN_RT
