#include "scene/mesh_builder.h"

// Helper: check if a single TextureRegion is fully transparent (all alpha == 0)
static bool isRegionFullyTransparent(const TextureRegion& region) {
    for (const auto& pixel : region.pixels) {
        if (pixel.a != 0.0f) {
            return false;
        }
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

Mesh MeshBuilder::buildBox(const BodyPartTexture& tex,
                           const Vec3& position,
                           const Vec3& size,
                           float offset) {
    Mesh mesh;
    mesh.isOuterLayer = (offset > 0.0f);
    mesh.triangles.reserve(12);

    // Copy textures into the mesh so it owns them (avoids dangling pointers)
    // Order: front=0, back=1, left=2, right=3, top=4, bottom=5
    mesh.ownedTextures[0] = tex.front;
    mesh.ownedTextures[1] = tex.back;
    mesh.ownedTextures[2] = tex.left;
    mesh.ownedTextures[3] = tex.right;
    mesh.ownedTextures[4] = tex.top;
    mesh.ownedTextures[5] = tex.bottom;

    float hw = size.x / 2.0f + offset;
    float hh = size.y / 2.0f + offset;
    float hd = size.z / 2.0f + offset;

    float px = position.x;
    float py = position.y;
    float pz = position.z;

    // 8 vertices of the box
    Vec3 v000(px - hw, py - hh, pz - hd);
    Vec3 v100(px + hw, py - hh, pz - hd);
    Vec3 v010(px - hw, py + hh, pz - hd);
    Vec3 v110(px + hw, py + hh, pz - hd);
    Vec3 v001(px - hw, py - hh, pz + hd);
    Vec3 v101(px + hw, py - hh, pz + hd);
    Vec3 v011(px - hw, py + hh, pz + hd);
    Vec3 v111(px + hw, py + hh, pz + hd);

    // Helper lambda to add a quad as 2 triangles
    // Vertices should be in counter-clockwise order when viewed from outside
    auto addFace = [&](const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                       const Vec3& normal, const TextureRegion* texRegion) {
        // Triangle 1: a, b, c  (UV: (0,0), (1,0), (1,1))
        Triangle t1;
        t1.v0 = a; t1.v1 = b; t1.v2 = c;
        t1.normal = normal;
        t1.u0 = 0.0f; t1.v0_uv = 0.0f;
        t1.u1 = 1.0f; t1.v1_uv = 0.0f;
        t1.u2 = 1.0f; t1.v2_uv = 1.0f;
        t1.texture = texRegion;
        mesh.triangles.push_back(t1);

        // Triangle 2: a, c, d  (UV: (0,0), (1,1), (0,1))
        Triangle t2;
        t2.v0 = a; t2.v1 = c; t2.v2 = d;
        t2.normal = normal;
        t2.u0 = 0.0f; t2.v0_uv = 0.0f;
        t2.u1 = 1.0f; t2.v1_uv = 1.0f;
        t2.u2 = 0.0f; t2.v2_uv = 1.0f;
        t2.texture = texRegion;
        mesh.triangles.push_back(t2);
    };

    // Front face (-Z)
    addFace(v010, v110, v100, v000, Vec3(0, 0, -1), &mesh.ownedTextures[0]);

    // Back face (+Z)
    addFace(v111, v011, v001, v101, Vec3(0, 0, 1), &mesh.ownedTextures[1]);

    // Left face (+X)
    addFace(v110, v111, v101, v100, Vec3(1, 0, 0), &mesh.ownedTextures[2]);

    // Right face (-X)
    addFace(v011, v010, v000, v001, Vec3(-1, 0, 0), &mesh.ownedTextures[3]);

    // Top face (+Y)
    addFace(v011, v111, v110, v010, Vec3(0, 1, 0), &mesh.ownedTextures[4]);

    // Bottom face (-Y)
    addFace(v000, v100, v101, v001, Vec3(0, -1, 0), &mesh.ownedTextures[5]);

    return mesh;
}

Scene MeshBuilder::buildScene(const SkinData& skin) {
    Scene scene;

    // Body part definitions: {texture, outerTexture, position, size}
    struct PartDef {
        const BodyPartTexture* inner;
        const BodyPartTexture* outer;
        Vec3 position;
        Vec3 size;
    };

    PartDef parts[] = {
        { &skin.head,     &skin.headOuter,     Vec3(0, 28, 0), Vec3(8, 8, 8) },
        { &skin.body,     &skin.bodyOuter,     Vec3(0, 18, 0), Vec3(8, 12, 4) },
        { &skin.rightArm, &skin.rightArmOuter, Vec3(-6, 18, 0), Vec3(4, 12, 4) },
        { &skin.leftArm,  &skin.leftArmOuter,  Vec3(6, 18, 0), Vec3(4, 12, 4) },
        { &skin.rightLeg, &skin.rightLegOuter, Vec3(-2, 6, 0), Vec3(4, 12, 4) },
        { &skin.leftLeg,  &skin.leftLegOuter,  Vec3(2, 6, 0), Vec3(4, 12, 4) },
    };

    for (const auto& part : parts) {
        // Inner layer (offset = 0)
        scene.meshes.push_back(buildBox(*part.inner, part.position, part.size, 0.0f));

        // Outer layer (offset = 0.5), skip if fully transparent
        if (!isFullyTransparent(*part.outer)) {
            scene.meshes.push_back(buildBox(*part.outer, part.position, part.size, 0.5f));
        }
    }

    // Default light: above and in front of the character
    scene.light.position = Vec3(0, 40, 30);
    scene.light.color = Color(1, 1, 1, 1);
    scene.light.intensity = 1.0f;

    // Default camera: looking at the character from the front
    scene.camera.position = Vec3(0, 18, 50);
    scene.camera.target = Vec3(0, 18, 0);
    scene.camera.up = Vec3(0, 1, 0);
    scene.camera.fov = 60.0f;

    // Background color
    scene.backgroundColor = Color(0.2f, 0.3f, 0.5f, 1.0f);

    return scene;
}

Scene MeshBuilder::buildDefaultScene() {
    // Create a white 1x1 texture for all faces
    auto whiteTex = []{
        BodyPartTexture t;
        auto make = [](int w, int h) {
            return TextureRegion(w, h, std::vector<Color>(w * h, Color(1, 1, 1, 1)));
        };
        // Dimensions don't matter much for a uniform color; use 1x1
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
    // Outer layers: fully transparent (default TextureRegion has no pixels)
    // so buildScene will skip them via isFullyTransparent check.

    return buildScene(skin);
}
