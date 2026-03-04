#include "scene/mesh_builder.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool isRegionFullyTransparent(const TextureRegion& region) {
    for (const auto& pixel : region.pixels) {
        if (pixel.a != 0.0f) return false;
    }
    return true;
}

bool MeshBuilder::isFullyTransparent(const BodyPartTexture& tex) {
    return isRegionFullyTransparent(tex.top)
        && isRegionFullyTransparent(tex.bottom)
        && isRegionFullyTransparent(tex.front)
        && isRegionFullyTransparent(tex.back)
        && isRegionFullyTransparent(tex.left)
        && isRegionFullyTransparent(tex.right);
}

// Rotate a point around a pivot by angles (degrees) on X and Z axes
// Rotate a point around a pivot by angles (degrees) on X, Y, and Z axes (applied in X → Y → Z order)
static Vec3 rotateAroundPivot(const Vec3& point, const Vec3& pivot,
                               float rotXDeg, float rotYDeg, float rotZDeg) {
    Vec3 p = point - pivot;

    // Rotate around X axis (pitch: forward/backward)
    if (std::fabs(rotXDeg) > 0.01f) {
        float rad = rotXDeg * static_cast<float>(M_PI) / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float newY = p.y * cosA - p.z * sinA;
        float newZ = p.y * sinA + p.z * cosA;
        p.y = newY;
        p.z = newZ;
    }

    // Rotate around Y axis (yaw: left/right turn)
    if (std::fabs(rotYDeg) > 0.01f) {
        float rad = rotYDeg * static_cast<float>(M_PI) / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float newX = p.x * cosA + p.z * sinA;
        float newZ = -p.x * sinA + p.z * cosA;
        p.x = newX;
        p.z = newZ;
    }

    // Rotate around Z axis (roll: sideways lean)
    if (std::fabs(rotZDeg) > 0.01f) {
        float rad = rotZDeg * static_cast<float>(M_PI) / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float newX = p.x * cosA - p.y * sinA;
        float newY = p.x * sinA + p.y * cosA;
        p.x = newX;
        p.y = newY;
    }

    return p + pivot;
}


static void rotateMesh(Mesh& mesh, const Vec3& pivot, float rotXDeg, float rotYDeg, float rotZDeg) {
    if (std::fabs(rotXDeg) < 0.01f && std::fabs(rotYDeg) < 0.01f && std::fabs(rotZDeg) < 0.01f) return;

    for (auto& tri : mesh.triangles) {
        tri.v0 = rotateAroundPivot(tri.v0, pivot, rotXDeg, rotYDeg, rotZDeg);
        tri.v1 = rotateAroundPivot(tri.v1, pivot, rotXDeg, rotYDeg, rotZDeg);
        tri.v2 = rotateAroundPivot(tri.v2, pivot, rotXDeg, rotYDeg, rotZDeg);
        // Rotate normal too (no translation needed, pivot at origin)
        tri.normal = rotateAroundPivot(tri.normal, Vec3(0,0,0), rotXDeg, rotYDeg, rotZDeg);
    }
}

Mesh MeshBuilder::buildBox(const BodyPartTexture& tex,
                           const Vec3& position,
                           const Vec3& size,
                           float offset) {
    Mesh mesh;
    mesh.isOuterLayer = (offset > 0.0f);
    mesh.triangles.reserve(12);

    // Face mapping: character faces +Z (toward camera).
    // +Z = front, -Z = back, +X = character's left, -X = character's right
    mesh.ownedTextures[0] = tex.front;
    mesh.ownedTextures[1] = tex.back;
    mesh.ownedTextures[2] = tex.left;
    mesh.ownedTextures[3] = tex.right;
    mesh.ownedTextures[4] = tex.top;
    mesh.ownedTextures[5] = tex.bottom;

    float hw = size.x / 2.0f + offset;
    float hh = size.y / 2.0f + offset;
    float hd = size.z / 2.0f + offset;
    float px = position.x, py = position.y, pz = position.z;

    Vec3 v000(px-hw, py-hh, pz-hd), v100(px+hw, py-hh, pz-hd);
    Vec3 v010(px-hw, py+hh, pz-hd), v110(px+hw, py+hh, pz-hd);
    Vec3 v001(px-hw, py-hh, pz+hd), v101(px+hw, py-hh, pz+hd);
    Vec3 v011(px-hw, py+hh, pz+hd), v111(px+hw, py+hh, pz+hd);

    auto addFace = [&](const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                       const Vec3& normal, const TextureRegion* texRegion) {
        Triangle t1;
        t1.v0 = a; t1.v1 = b; t1.v2 = c;
        t1.normal = normal;
        t1.u0 = 0.0f; t1.v0_uv = 0.0f;
        t1.u1 = 1.0f; t1.v1_uv = 0.0f;
        t1.u2 = 1.0f; t1.v2_uv = 1.0f;
        t1.texture = texRegion;
        mesh.triangles.push_back(t1);

        Triangle t2;
        t2.v0 = a; t2.v1 = c; t2.v2 = d;
        t2.normal = normal;
        t2.u0 = 0.0f; t2.v0_uv = 0.0f;
        t2.u1 = 1.0f; t2.v1_uv = 1.0f;
        t2.u2 = 0.0f; t2.v2_uv = 1.0f;
        t2.texture = texRegion;
        mesh.triangles.push_back(t2);
    };

    // -Z face = back (away from camera), +Z face = front (toward camera)
    addFace(v010, v110, v100, v000, Vec3(0,0,-1), &mesh.ownedTextures[1]);  // back
    addFace(v111, v011, v001, v101, Vec3(0,0,1),  &mesh.ownedTextures[0]);  // front
    addFace(v110, v111, v101, v100, Vec3(1,0,0),  &mesh.ownedTextures[2]);  // left (+X = char's left)
    addFace(v011, v010, v000, v001, Vec3(-1,0,0), &mesh.ownedTextures[3]);  // right (-X = char's right)
    addFace(v011, v111, v110, v010, Vec3(0,1,0),  &mesh.ownedTextures[4]);  // top
    addFace(v000, v100, v101, v001, Vec3(0,-1,0), &mesh.ownedTextures[5]);  // bottom

    return mesh;
}

Mesh MeshBuilder::buildBoxWithPose(const BodyPartTexture& tex,
                                   const Vec3& position,
                                   const Vec3& size,
                                   float offset,
                                   const Vec3& pivot,
                                   const PartPose& partPose,
                                   const Vec3& torsoTranslation,
                                   const Vec3& torsoPivot,
                                   const PartPose& torsoPose) {
    Mesh mesh = buildBox(tex, position, size, offset);

    // Store unrotated triangles for raytracer (local space, before any rotation)
    mesh.localTriangles = mesh.triangles;

    // Store part rotation params
    mesh.hasRotation = true;
    mesh.pivot = pivot;
    mesh.rotX = partPose.rotX;
    mesh.rotY = partPose.rotY;
    mesh.rotZ = partPose.rotZ;

    // Step 1: Apply part local rotation around part's pivot (X → Y → Z)
    rotateMesh(mesh, pivot, partPose.rotX, partPose.rotY, partPose.rotZ);

    // Check if torso transform is non-zero
    bool hasTorso = (std::fabs(torsoPose.rotX) > 0.01f ||
                     std::fabs(torsoPose.rotY) > 0.01f ||
                     std::fabs(torsoPose.rotZ) > 0.01f ||
                     std::fabs(torsoTranslation.x) > 0.01f ||
                     std::fabs(torsoTranslation.y) > 0.01f ||
                     std::fabs(torsoTranslation.z) > 0.01f);

    // Store torso transform fields in mesh (for raytracer inverse transform)
    mesh.hasTorsoTransform = hasTorso;
    mesh.torsoTranslation = torsoTranslation;
    mesh.torsoPivot = torsoPivot;
    mesh.torsoRotX = torsoPose.rotX;
    mesh.torsoRotY = torsoPose.rotY;
    mesh.torsoRotZ = torsoPose.rotZ;

    if (hasTorso) {
        // Step 2: Apply torso rotation around torso pivot
        rotateMesh(mesh, torsoPivot, torsoPose.rotX, torsoPose.rotY, torsoPose.rotZ);

        // Step 3: Apply torso translation to all vertices
        for (auto& tri : mesh.triangles) {
            tri.v0 = tri.v0 + torsoTranslation;
            tri.v1 = tri.v1 + torsoTranslation;
            tri.v2 = tri.v2 + torsoTranslation;
        }
    }

    return mesh;
}

Scene MeshBuilder::buildScene(const SkinData& skin, const Pose& pose) {
    Scene scene;

    // Torso pivot point (torso center) — root of the hierarchy
    const Vec3 torsoPivot(0, 18, 0);

    // Body part definitions with pivot points (joint locations)
    struct PartDef {
        const BodyPartTexture* inner;
        const BodyPartTexture* outer;
        Vec3 position;
        Vec3 size;
        Vec3 pivot;          // rotation pivot point
        const PartPose* pose;
        bool isTorso;        // true only for the torso (root node)
    };

    // Head pivot: bottom of head (neck joint) = (0, 24, 0)
    // Body pivot: center = (0, 18, 0)
    // Arms pivot: top of arm (shoulder) = (±6, 24, 0)
    // Legs pivot: top of leg (hip) = (±2, 12, 0)
    PartDef parts[] = {
        { &skin.head,     &skin.headOuter,     Vec3(0, 28, 0),  Vec3(8, 8, 8),   Vec3(0, 24, 0),  &pose.head,     false },
        { &skin.body,     &skin.bodyOuter,     Vec3(0, 18, 0),  Vec3(8, 12, 4),  Vec3(0, 18, 0),  &pose.body,     true  },
        { &skin.rightArm, &skin.rightArmOuter, Vec3(-6, 18, 0), Vec3(4, 12, 4),  Vec3(-6, 24, 0), &pose.rightArm, false },
        { &skin.leftArm,  &skin.leftArmOuter,  Vec3(6, 18, 0),  Vec3(4, 12, 4),  Vec3(6, 24, 0),  &pose.leftArm,  false },
        { &skin.rightLeg, &skin.rightLegOuter, Vec3(-2, 6, 0),  Vec3(4, 12, 4),  Vec3(-2, 12, 0), &pose.rightLeg, false },
        { &skin.leftLeg,  &skin.leftLegOuter,  Vec3(2, 6, 0),   Vec3(4, 12, 4),  Vec3(2, 12, 0),  &pose.leftLeg,  false },
    };

    // Detect whether any torso transform is active (check all 3 rotation axes + torso translation)
    bool hasTorsoTransform = (std::fabs(pose.body.rotX) > 0.01f ||
                              std::fabs(pose.body.rotY) > 0.01f ||
                              std::fabs(pose.body.rotZ) > 0.01f ||
                              std::fabs(pose.torsoTranslation.x) > 0.01f ||
                              std::fabs(pose.torsoTranslation.y) > 0.01f ||
                              std::fabs(pose.torsoTranslation.z) > 0.01f);

    for (const auto& part : parts) {
        bool hasLocalPose = (std::fabs(part.pose->rotX) > 0.01f ||
                             std::fabs(part.pose->rotY) > 0.01f ||
                             std::fabs(part.pose->rotZ) > 0.01f);

        // A part needs buildBoxWithPose if it has local rotation OR torso transform applies
        bool hasPose = hasLocalPose || hasTorsoTransform;

        if (hasPose) {
            if (part.isTorso) {
                // Torso is the root: its own rotation IS the local rotation.
                // Pass torsoTranslation directly, but with zero torsoPose (no parent transform).
                scene.meshes.push_back(buildBoxWithPose(*part.inner, part.position, part.size,
                                                         0.0f, part.pivot, *part.pose,
                                                         pose.torsoTranslation, Vec3{}, PartPose{}));
                if (!isFullyTransparent(*part.outer)) {
                    scene.meshes.push_back(buildBoxWithPose(*part.outer, part.position, part.size,
                                                             0.5f, part.pivot, *part.pose,
                                                             pose.torsoTranslation, Vec3{}, PartPose{}));
                }
            } else {
                // Child parts: apply local rotation, then torso rotation around torso pivot, then torso translation
                scene.meshes.push_back(buildBoxWithPose(*part.inner, part.position, part.size,
                                                         0.0f, part.pivot, *part.pose,
                                                         pose.torsoTranslation, torsoPivot, pose.body));
                if (!isFullyTransparent(*part.outer)) {
                    scene.meshes.push_back(buildBoxWithPose(*part.outer, part.position, part.size,
                                                             0.5f, part.pivot, *part.pose,
                                                             pose.torsoTranslation, torsoPivot, pose.body));
                }
            }
        } else {
            scene.meshes.push_back(buildBox(*part.inner, part.position, part.size, 0.0f));
            if (!isFullyTransparent(*part.outer)) {
                scene.meshes.push_back(buildBox(*part.outer, part.position, part.size, 0.5f));
            }
        }
    }

    scene.light.position = Vec3(0, 40, 30);
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;

    scene.camera.position = Vec3(0, 18, 50);
    scene.camera.target = Vec3(0, 18, 0);
    scene.camera.up = Vec3(0, 1, 0);
    scene.camera.fov = 60.0f;

    scene.backgroundColor = Color(0.2f, 0.3f, 0.5f, 1.0f);

    return scene;
}

Scene MeshBuilder::buildDefaultScene(const Pose& pose) {
    auto whiteTex = []{
        BodyPartTexture t;
        auto make = [](int w, int h) {
            return TextureRegion(w, h, std::vector<Color>(w * h, Color(1, 1, 1, 1)));
        };
        t.front = make(1, 1); t.back = make(1, 1);
        t.left  = make(1, 1); t.right = make(1, 1);
        t.top   = make(1, 1); t.bottom = make(1, 1);
        return t;
    };

    SkinData skin{};
    skin.format = SkinData::NEW_64x64;
    skin.head = whiteTex(); skin.body = whiteTex();
    skin.rightArm = whiteTex(); skin.leftArm = whiteTex();
    skin.rightLeg = whiteTex(); skin.leftLeg = whiteTex();

    return buildScene(skin, pose);
}
