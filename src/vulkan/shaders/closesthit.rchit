#version 460
#extension GL_EXT_ray_tracing : require

// ── Constants (matching CPU shading.h defaults) ─────────────────────────────

const float KD        = 0.75;   // Diffuse coefficient
const float KS        = 0.15;   // Specular coefficient
const float AMBIENT_K = 0.20;   // Ambient light coefficient
const float SHININESS = 16.0;   // Specular exponent

const float SHADOW_EPSILON = 1e-3;
const float ALPHA_THRESHOLD = 0.01; // Transparent pixel discard threshold
const float PI = 3.14159265358979;

// ── Descriptor bindings ─────────────────────────────────────────────────────

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, rgba8) uniform image2D outputImage;

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

// ── Vertex & index storage buffers ──────────────────────────────────────────
// Positions: float3 per vertex, laid out as [x,y,z, x,y,z, ...]
// Triangles use 3 consecutive vertices: tri i → vertices 3i, 3i+1, 3i+2

layout(set = 0, binding = 3) readonly buffer VertexPositions {
    float positions[];
} vertexData;

// UV coordinates: float2 per vertex, laid out as [u,v, u,v, ...]
// Same vertex ordering as positions.
layout(set = 0, binding = 4) readonly buffer VertexUVs {
    float uvs[];
} uvData;

// Index buffer: uint32 per index
layout(set = 0, binding = 5) readonly buffer IndexBuffer {
    uint indices[];
} indexData;

// Skin texture sampler
layout(set = 0, binding = 6) uniform sampler2D skinTexture;

// ── Ray payload ─────────────────────────────────────────────────────────────

layout(location = 0) rayPayloadInEXT vec3 payloadColor;

// Secondary payload for shadow / AO / bounce rays
layout(location = 1) rayPayloadEXT vec3 secondaryPayload;

// ── Hit attributes (barycentrics from intersection) ─────────────────────────

hitAttributeEXT vec2 attribs;

// ── PCG hash RNG (same as raygen.rgen) ──────────────────────────────────────

uint pcgHash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float randomFloat(inout uint seed) {
    seed = pcgHash(seed);
    return float(seed) / 4294967296.0;
}

// ── Utility: concentric disk sample ─────────────────────────────────────────

vec2 sampleDisk(inout uint seed) {
    float u = randomFloat(seed);
    float v = randomFloat(seed);
    float angle = 2.0 * PI * u;
    float radius = sqrt(v);
    return vec2(cos(angle), sin(angle)) * radius;
}

// ── Utility: cosine-weighted hemisphere sample ──────────────────────────────
// Returns a direction in tangent space (z-up) with cosine-weighted distribution.

vec3 sampleHemisphere(inout uint seed) {
    float u1 = randomFloat(seed);
    float u2 = randomFloat(seed);
    float r = sqrt(u1);
    float theta = 2.0 * PI * u2;
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(max(0.0, 1.0 - u1));
    return vec3(x, y, z);
}

// ── Build orthonormal basis from normal ─────────────────────────────────────

void buildBasis(vec3 N, out vec3 T, out vec3 B) {
    vec3 up = (abs(N.x) < 0.9) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

// ── Main ────────────────────────────────────────────────────────────────────

void main() {
    // ── Barycentric coordinates ─────────────────────────────────────────
    // attribs.xy = (beta, gamma); alpha = 1 - beta - gamma
    float baryAlpha = 1.0 - attribs.x - attribs.y;
    float baryBeta  = attribs.x;
    float baryGamma = attribs.y;

    // ── Retrieve triangle vertex data ───────────────────────────────────
    // gl_PrimitiveID gives the triangle index within the BLAS geometry.
    // Each triangle has 3 consecutive indices in the index buffer.
    // gl_InstanceCustomIndexEXT could offset into per-mesh data if needed.
    uint triIdx = uint(gl_PrimitiveID);
    uint i0 = indexData.indices[triIdx * 3 + 0];
    uint i1 = indexData.indices[triIdx * 3 + 1];
    uint i2 = indexData.indices[triIdx * 3 + 2];

    // Fetch vertex positions
    vec3 p0 = vec3(vertexData.positions[i0 * 3 + 0],
                   vertexData.positions[i0 * 3 + 1],
                   vertexData.positions[i0 * 3 + 2]);
    vec3 p1 = vec3(vertexData.positions[i1 * 3 + 0],
                   vertexData.positions[i1 * 3 + 1],
                   vertexData.positions[i1 * 3 + 2]);
    vec3 p2 = vec3(vertexData.positions[i2 * 3 + 0],
                   vertexData.positions[i2 * 3 + 1],
                   vertexData.positions[i2 * 3 + 2]);

    // Fetch UV coordinates
    vec2 uv0 = vec2(uvData.uvs[i0 * 2 + 0], uvData.uvs[i0 * 2 + 1]);
    vec2 uv1 = vec2(uvData.uvs[i1 * 2 + 0], uvData.uvs[i1 * 2 + 1]);
    vec2 uv2 = vec2(uvData.uvs[i2 * 2 + 0], uvData.uvs[i2 * 2 + 1]);

    // ── Interpolate UV at hit point ─────────────────────────────────────
    vec2 hitUV = baryAlpha * uv0 + baryBeta * uv1 + baryGamma * uv2;

    // ── Sample skin texture ─────────────────────────────────────────────
    vec4 texColor = texture(skinTexture, hitUV);

    // Handle transparent pixels: if alpha is below threshold, treat as miss
    if (texColor.a < ALPHA_THRESHOLD) {
        // Ignore this hit — continue the ray through transparent geometry.
        // We write a zero color; the caller (raygen) will get the miss result
        // via the secondary trace. For simplicity, re-trace past this hit.
        float tMin = gl_HitTEXT + 0.001;
        float tMax = 10000.0;
        traceRayEXT(
            topLevelAS,
            gl_RayFlagsOpaqueEXT,
            0xFF,
            0, 0, 0,
            gl_WorldRayOriginEXT,
            tMin,
            gl_WorldRayDirectionEXT,
            tMax,
            0  // payload location 0
        );
        // payloadColor is now set by the recursive trace (hit or miss)
        return;
    }

    // ── Compute surface normal from triangle vertices ───────────────────
    // Transform positions to world space using gl_ObjectToWorldEXT (mat4x3)
    mat4x3 objToWorld = gl_ObjectToWorldEXT;
    vec3 wp0 = objToWorld * vec4(p0, 1.0);
    vec3 wp1 = objToWorld * vec4(p1, 1.0);
    vec3 wp2 = objToWorld * vec4(p2, 1.0);

    vec3 edge1 = wp1 - wp0;
    vec3 edge2 = wp2 - wp0;
    vec3 N = normalize(cross(edge1, edge2));

    // Ensure normal faces the incoming ray direction
    if (dot(N, gl_WorldRayDirectionEXT) > 0.0) {
        N = -N;
    }

    // ── Hit point in world space ────────────────────────────────────────
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    // ── Initialize RNG seed ─────────────────────────────────────────────
    uint seed = pcgHash(
        pcgHash(uint(gl_LaunchIDEXT.x)) +
        pcgHash(uint(gl_LaunchIDEXT.y) * 1973u) +
        pcgHash(uint(gl_PrimitiveID) * 6133u)
    );

    // ── View direction (from hit point toward camera) ───────────────────
    vec3 V = normalize(uniforms.cameraPosition - hitPoint);

    // ── Light direction (from hit point toward light) ───────────────────
    vec3 L = normalize(uniforms.lightPosition - hitPoint);

    // ── Shadow visibility ───────────────────────────────────────────────
    float visibility = 1.0;

    if (uniforms.softShadows != 0 && uniforms.shadowSamples > 1
        && uniforms.lightRadius > 1e-4) {
        // Soft shadows: sample area light disk
        // Build a local frame at the light for disk sampling
        vec3 toPoint = normalize(hitPoint - uniforms.lightPosition);
        vec3 tangent, bitangent;
        buildBasis(toPoint, tangent, bitangent);

        int litCount = 0;
        int totalSamples = uniforms.shadowSamples;

        for (int i = 0; i < totalSamples; ++i) {
            // Stratified disk sampling (matching CPU computeSoftShadow)
            float angle = 2.0 * PI * randomFloat(seed);
            float r = uniforms.lightRadius * sqrt(randomFloat(seed));
            vec3 offset = tangent * (r * cos(angle)) + bitangent * (r * sin(angle));
            vec3 samplePos = uniforms.lightPosition + offset;

            // Shadow ray from hit point toward light sample
            vec3 shadowOrigin = hitPoint + N * SHADOW_EPSILON;
            vec3 toLight = samplePos - shadowOrigin;
            float distToLight = length(toLight);
            vec3 shadowDir = toLight / distToLight;

            // Trace shadow ray using sentinel pattern:
            // Set payload to sentinel (-1). If ray misses all geometry,
            // the miss shader writes a non-negative value. If geometry is
            // hit, SkipClosestHitShader means payload stays at sentinel.
            secondaryPayload = vec3(-1.0);
            traceRayEXT(
                topLevelAS,
                gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF,
                0, 0,
                0,  // miss shader index
                shadowOrigin,
                0.0,
                shadowDir,
                distToLight,
                1   // payload location 1
            );

            // Miss shader was invoked → no occlusion → lit
            if (secondaryPayload.x >= 0.0) {
                litCount++;
            }
        }

        visibility = float(litCount) / float(totalSamples);
    } else {
        // Hard shadow: single ray toward light center
        vec3 shadowOrigin = hitPoint + N * SHADOW_EPSILON;
        vec3 toLight = uniforms.lightPosition - shadowOrigin;
        float distToLight = length(toLight);
        vec3 shadowDir = toLight / distToLight;

        secondaryPayload = vec3(-1.0);
        traceRayEXT(
            topLevelAS,
            gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
            0xFF,
            0, 0,
            0,
            shadowOrigin,
            0.0,
            shadowDir,
            distToLight,
            1
        );

        visibility = (secondaryPayload.x >= 0.0) ? 1.0 : 0.0;
    }

    // ── Blinn-Phong shading (matching CPU shading.h) ────────────────────
    // Ambient component
    vec3 ambient = texColor.rgb * AMBIENT_K;

    // Diffuse component
    float NdotL = max(0.0, dot(N, L));
    vec3 diffuse = texColor.rgb * uniforms.lightColor * (KD * NdotL * visibility);

    // Specular component (Blinn-Phong: half-vector)
    vec3 H = normalize(L + V);
    float NdotH = max(0.0, dot(N, H));
    float specFactor = pow(NdotH, SHININESS);
    vec3 specular = uniforms.lightColor * (KS * specFactor * visibility);

    vec3 color = ambient + diffuse + specular;

    // ── Ambient Occlusion ───────────────────────────────────────────────
    if (uniforms.aoEnabled != 0 && uniforms.aoSamples > 0) {
        vec3 tangent, bitangent;
        buildBasis(N, tangent, bitangent);

        int occluded = 0;
        int totalAO = uniforms.aoSamples;

        for (int i = 0; i < totalAO; ++i) {
            // Cosine-weighted hemisphere sample in tangent space
            vec3 localDir = sampleHemisphere(seed);

            // Transform to world space
            vec3 worldDir = localDir.x * tangent + localDir.y * bitangent + localDir.z * N;

            vec3 aoOrigin = hitPoint + N * SHADOW_EPSILON;

            secondaryPayload = vec3(-1.0);
            traceRayEXT(
                topLevelAS,
                gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF,
                0, 0,
                0,
                aoOrigin,
                0.0,
                worldDir,
                uniforms.aoRadius,
                1
            );

            // If hit (payload stays sentinel) → occluded
            if (secondaryPayload.x < 0.0) {
                occluded++;
            }
        }

        float aoFactor = 1.0 - uniforms.aoIntensity * (float(occluded) / float(totalAO));
        color *= aoFactor;
    }

    // ── Recursive bounces (reflection) ──────────────────────────────────
    // Trace reflection rays up to maxBounces depth.
    // gl_RayTmaxEXT is not a depth counter; we use a simple approach:
    // check current recursion depth via the incoming ray's hit distance
    // as a proxy. For proper depth tracking, we'd need a payload field.
    // Here we use a simple single-bounce reflection controlled by maxBounces.

    if (uniforms.maxBounces > 1) {
        // Reflect the incoming ray direction about the surface normal
        vec3 incomingDir = gl_WorldRayDirectionEXT;
        vec3 reflectDir = reflect(incomingDir, N);

        vec3 reflectOrigin = hitPoint + N * SHADOW_EPSILON;

        // Trace reflection ray (uses payload location 1 to avoid overwriting)
        secondaryPayload = vec3(0.0);
        traceRayEXT(
            topLevelAS,
            gl_RayFlagsOpaqueEXT,
            0xFF,
            0, 0, 0,
            reflectOrigin,
            0.001,
            reflectDir,
            10000.0,
            1
        );

        // Blend reflection with direct illumination
        // Use a small reflection coefficient (Fresnel approximation)
        float fresnel = KS + (1.0 - KS) * pow(1.0 - max(0.0, dot(V, N)), 5.0);
        color = mix(color, secondaryPayload, fresnel * 0.3);
    }

    // ── Clamp and output ────────────────────────────────────────────────
    payloadColor = clamp(color, 0.0, 1.0);
}
