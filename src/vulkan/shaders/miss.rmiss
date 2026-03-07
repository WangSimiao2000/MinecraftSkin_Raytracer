#version 460
#extension GL_EXT_ray_tracing : require

// ── Descriptor bindings (same layout as raygen.rgen / closesthit.rchit) ─────

layout(set = 0, binding = 2) uniform RenderUniforms {
    // Camera
    mat4 cameraInverseView;
    mat4 cameraInverseProj;
    vec3 cameraPosition;
    float fov;

    // Light
    vec3 lightPosition;
    float lightRadius;
    vec3 lightColor;
    float lightIntensity;

    // Render parameters
    int samplesPerPixel;
    int maxBounces;
    int shadowSamples;
    int aoSamples;

    // Effect toggles & parameters
    int softShadows;
    int aoEnabled;
    float aoRadius;
    float aoIntensity;
    int dofEnabled;
    float aperture;
    float focusDistance;

    // Background
    int gradientBg;
    float gradientScale;
    vec3 bgCenter;
    vec3 bgEdge;

    // Image dimensions
    int width;
    int height;
} uniforms;

// ── Ray payloads ────────────────────────────────────────────────────────────
// Primary ray color (location 0) — written with background color.
// Shadow/AO rays also invoke this miss shader; the closest-hit shader checks
// secondaryPayload (location 1) for a non-negative sentinel to detect "no hit".

layout(location = 0) rayPayloadInEXT vec3 payloadColor;
layout(location = 1) rayPayloadEXT  vec3 secondaryPayload;

// ── Main ────────────────────────────────────────────────────────────────────

void main() {
    // ── Radial gradient background (matching CPU backgroundColor logic) ──
    //
    // CPU reference (raytracer.cpp):
    //   cx = u - 0.5;  cy = v - 0.5;
    //   dist = sqrt(cx*cx + cy*cy) * 2.0 * gradientScale;  clamped to [0,1]
    //   t = dist * dist;                                    smooth falloff
    //   color = bgCenter * (1-t) + bgEdge * t;

    vec3 bgColor;

    if (uniforms.gradientBg != 0) {
        // Normalised pixel coordinates in [0, 1]
        float u = (float(gl_LaunchIDEXT.x) + 0.5) / float(gl_LaunchSizeEXT.x);
        float v = (float(gl_LaunchIDEXT.y) + 0.5) / float(gl_LaunchSizeEXT.y);

        float cx = u - 0.5;
        float cy = v - 0.5;
        float dist = sqrt(cx * cx + cy * cy) * 2.0 * uniforms.gradientScale;
        dist = clamp(dist, 0.0, 1.0);

        // Smooth quadratic falloff
        float t = dist * dist;
        bgColor = mix(uniforms.bgCenter, uniforms.bgEdge, t);
    } else {
        // Flat background: use center color
        bgColor = uniforms.bgCenter;
    }

    // Write background color for primary rays (payload location 0)
    payloadColor = bgColor;

    // Write non-negative sentinel for shadow/AO rays (payload location 1).
    // The closest-hit shader sets secondaryPayload to vec3(-1) before tracing
    // shadow/AO rays. If the miss shader is invoked, this positive value
    // signals "no geometry was hit" (i.e. the point is lit / unoccluded).
    secondaryPayload = vec3(1.0);
}
