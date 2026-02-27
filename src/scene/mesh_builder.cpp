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
static Vec3 rotateAroundPivot(const Vec3& point, const Vec3& pivot,
                               float rotXDeg, float rotZDeg) {
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

static void rotateMesh(Mesh& mesh, const Vec3& pivot, float rotXDeg, float rotZDeg) {
    if (std::fabs(rotXDeg) < 0.01f && std::fabs(rotZDeg) < 0.01f) return;

    for (auto& tri : mesh.triangles) {
        tri.v0 = rotateAroundPivot(tri.v0, pivot, rotXDeg, rotZDeg);
        tri.v1 = rotateAroundPivot(tri.v1, pivot, rotXDeg, rotZDeg);
        tri.v2 = rotateAroundPivot(tri.v2, pivot, rotXDeg, rotZDeg);
        // Rotate normal too (no translation needed)
        tri.normal = rotateAroundPivot(tri.normal, Vec3(0,0,0), rotXDeg, rotZDeg);
    }
}

Mesh MeshBuilder::buildBox(const BodyPartTexture& tex,
                           const Vec3& position,
                           const Vec3& size,
                           float offset) {
    Mesh mesh;
    mesh.isOuterLayer = (offset > 0.0f);
    mesh.triangles.reserve(12);

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

    addFace(v010, v110, v100, v000, Vec3(0,0,-1), &mesh.ownedTextures[0]);
    addFace(v111, v011, v001, v101, Vec3(0,0,1),  &mesh.ownedTextures[1]);
    addFace(v110, v111, v101, v100, Vec3(1,0,0),  &mesh.ownedTextures[2]);
    addFace(v011, v010, v000, v001, Vec3(-1,0,0), &mesh.ownedTextures[3]);
    addFace(v011, v111, v110, v010, Vec3(0,1,0),  &mesh.ownedTextures[4]);
    addFace(v000, v100, v101, v001, Vec3(0,-1,0), &mesh.ownedTextures[5]);

    return mesh;
}

Mesh MeshBuilder::buildBoxWithPose(const BodyPartTexture& tex,
                                   const Vec3& position,
                                   const Vec3& size,
                                   float offset,
                                   const Vec3& pivot,
                                   const PartPose& partPose) {
    Mesh mesh = buildBox(tex, position, size, offset);
    rotateMesh(mesh, pivot, partPose.rotX, partPose.rotZ);
    return mesh;
}

Scene MeshBuilder::buildScene(const SkinData& skin, const Pose& pose) {
    Scene scene;

    // Body part definitions with pivot points (joint locations)
    // Pivot = top of the part for limbs (shoulder/hip joint)
    struct PartDef {
        const BodyPartTexture* inner;
        const BodyPartTexture* outer;
        Vec3 position;
        Vec3 size;
        Vec3 pivot;          // rotation pivot point
        const PartPose* pose;
    };

    // Head pivot: bottom of head (neck joint) = (0, 24, 0)
    // Body pivot: center (no rotation typically)
    // Arms pivot: top of arm (shoulder) = (±6, 24, 0)
    // Legs pivot: top of leg (hip) = (±2, 12, 0)
    PartDef parts[] = {
        { &skin.head,     &skin.headOuter,     Vec3(0, 28, 0),  Vec3(8, 8, 8),   Vec3(0, 24, 0),  &pose.head },
        { &skin.body,     &skin.bodyOuter,     Vec3(0, 18, 0),  Vec3(8, 12, 4),  Vec3(0, 18, 0),  &pose.body },
        { &skin.rightArm, &skin.rightArmOuter, Vec3(-6, 18, 0), Vec3(4, 12, 4),  Vec3(-6, 24, 0), &pose.rightArm },
        { &skin.leftArm,  &skin.leftArmOuter,  Vec3(6, 18, 0),  Vec3(4, 12, 4),  Vec3(6, 24, 0),  &pose.leftArm },
        { &skin.rightLeg, &skin.rightLegOuter, Vec3(-2, 6, 0),  Vec3(4, 12, 4),  Vec3(-2, 12, 0), &pose.rightLeg },
        { &skin.leftLeg,  &skin.leftLegOuter,  Vec3(2, 6, 0),   Vec3(4, 12, 4),  Vec3(2, 12, 0),  &pose.leftLeg },
    };

    for (const auto& part : parts) {
        bool hasPose = (std::fabs(part.pose->rotX) > 0.01f || std::fabs(part.pose->rotZ) > 0.01f);

        if (hasPose) {
            scene.meshes.push_back(buildBoxWithPose(*part.inner, part.position, part.size,
                                                     0.0f, part.pivot, *part.pose));
            if (!isFullyTransparent(*part.outer)) {
                scene.meshes.push_back(buildBoxWithPose(*part.outer, part.position, part.size,
                                                         0.5f, part.pivot, *part.pose));
            }
        } else {
            scene.meshes.push_back(buildBox(*part.inner, part.position, part.size, 0.0f));
            if (!isFullyTransparent(*part.outer)) {
                scene.meshes.push_back(buildBox(*part.outer, part.position, part.size, 0.5f));
            }
        }
    }

    // Rotate entire model 180° around Y axis so the face points toward +Z (toward camera)
    // Y-axis 180° rotation: (x, y, z) → (-x, y, -z), normal likewise
    for (auto& mesh : scene.meshes) {
        for (auto& tri : mesh.triangles) {
            tri.v0.x = -tri.v0.x; tri.v0.z = -tri.v0.z;
            tri.v1.x = -tri.v1.x; tri.v1.z = -tri.v1.z;
            tri.v2.x = -tri.v2.x; tri.v2.z = -tri.v2.z;
            tri.normal.x = -tri.normal.x; tri.normal.z = -tri.normal.z;
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
