#pragma once

#include "renderer/renderer.h"

#include <string>

#ifdef HAVE_VULKAN_RT

#include "vulkan/vk_context.h"

#include <glm/glm.hpp>

/// GPU uniform buffer layout – must match the RenderUniforms block in the
/// ray tracing shaders exactly (std140 layout rules).
///
/// std140 alignment rules applied:
///   mat4  → align 16, size 64
///   vec3  → align 16, size 12 (consumes 16 bytes unless followed by a scalar)
///   float → align 4,  size 4
///   int   → align 4,  size 4
struct alignas(16) GpuRenderUniforms {
    // Camera (offset 0)
    glm::mat4 cameraInverseView;   // 0
    glm::mat4 cameraInverseProj;   // 64
    glm::vec3 cameraPosition;      // 128  (vec3 aligned to 16)
    float fov;                     // 140

    // Light (offset 144)
    glm::vec3 lightPosition;       // 144  (vec3 aligned to 16)
    float lightRadius;             // 156
    glm::vec3 lightColor;          // 160  (vec3 aligned to 16)
    float lightIntensity;          // 172

    // Render parameters (offset 176)
    int samplesPerPixel;           // 176
    int maxBounces;                // 180
    int shadowSamples;             // 184
    int aoSamples;                 // 188

    // Effect toggles & parameters (offset 192)
    int softShadows;               // 192
    int aoEnabled;                 // 196
    float aoRadius;                // 200
    float aoIntensity;             // 204
    int dofEnabled;                // 208
    float aperture;                // 212
    float focusDistance;            // 216

    // Background (offset 220)
    int gradientBg;                // 220
    float gradientScale;           // 224
    float _pad0;                   // 228  padding to align bgCenter to 16
    float _pad1;                   // 232
    float _pad2;                   // 236
    glm::vec3 bgCenter;           // 240  (vec3 aligned to 16)
    float _pad3;                   // 252  padding after vec3
    glm::vec3 bgEdge;             // 256  (vec3 aligned to 16)
    float _pad4;                   // 268

    // Image dimensions (offset 272)
    int width;                     // 272
    int height;                    // 276
};

/// Pack a RenderConfig + Scene camera/light into a GpuRenderUniforms struct.
GpuRenderUniforms packUniforms(const RenderConfig& config, const Scene& scene);

#endif // HAVE_VULKAN_RT

/// GPU rendering backend using Vulkan hardware ray tracing.
/// When HAVE_VULKAN_RT is not defined, render() always returns failure.
class GpuRenderer : public IRenderer {
public:
    /// \param shaderDir  Directory containing compiled SPIR-V shaders.
    explicit GpuRenderer(const std::string& shaderDir = "shaders/");

    RenderResult render(const Scene& scene,
                        const RenderConfig& config,
                        ProgressCallback progress = nullptr) override;

private:
    std::string shaderDir_;
};
