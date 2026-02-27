#pragma once

#include <vector>
#include <array>
#include <memory>
#include "scene/triangle.h"
#include "skin/texture_region.h"

// 网格：一组三角形
// Owns its texture data so that Triangle::texture pointers remain valid
// after the original BodyPartTexture goes out of scope.
struct Mesh {
    std::vector<Triangle> triangles;
    bool isOuterLayer = false;  // 是否为外层网格

    // Owned texture regions: front, back, left, right, top, bottom
    std::array<TextureRegion, 6> ownedTextures;

    // Pose rotation (for raytracer inverse-transform approach)
    bool hasRotation = false;
    Vec3 pivot;        // rotation pivot point
    float rotX = 0.0f; // pitch degrees
    float rotZ = 0.0f; // roll degrees

    // Unrotated triangles for AABB intersection in local space
    std::vector<Triangle> localTriangles;

    Mesh() = default;

    // Custom copy constructor: deep copy and re-point texture pointers
    Mesh(const Mesh& other)
        : triangles(other.triangles)
        , isOuterLayer(other.isOuterLayer)
        , ownedTextures(other.ownedTextures)
        , hasRotation(other.hasRotation)
        , pivot(other.pivot)
        , rotX(other.rotX)
        , rotZ(other.rotZ)
        , localTriangles(other.localTriangles)
    {
        fixupTexturePointers(other);
    }

    // Custom copy assignment
    Mesh& operator=(const Mesh& other) {
        if (this != &other) {
            triangles = other.triangles;
            isOuterLayer = other.isOuterLayer;
            ownedTextures = other.ownedTextures;
            hasRotation = other.hasRotation;
            pivot = other.pivot;
            rotX = other.rotX;
            rotZ = other.rotZ;
            localTriangles = other.localTriangles;
            fixupTexturePointers(other);
        }
        return *this;
    }

    // Custom move constructor
    Mesh(Mesh&& other) noexcept
        : triangles(std::move(other.triangles))
        , isOuterLayer(other.isOuterLayer)
        , ownedTextures(std::move(other.ownedTextures))
        , hasRotation(other.hasRotation)
        , pivot(other.pivot)
        , rotX(other.rotX)
        , rotZ(other.rotZ)
        , localTriangles(std::move(other.localTriangles))
    {
        fixupTexturePointersAfterMove(other);
    }

    // Custom move assignment
    Mesh& operator=(Mesh&& other) noexcept {
        if (this != &other) {
            triangles = std::move(other.triangles);
            isOuterLayer = other.isOuterLayer;
            ownedTextures = std::move(other.ownedTextures);
            hasRotation = other.hasRotation;
            pivot = other.pivot;
            rotX = other.rotX;
            rotZ = other.rotZ;
            localTriangles = std::move(other.localTriangles);
            fixupTexturePointersAfterMove(other);
        }
        return *this;
    }

private:
    // After copy: remap triangle texture pointers from other's textures to ours
    void fixupTexturePointers(const Mesh& other) {
        for (auto& tri : triangles) {
            if (tri.texture) {
                for (int i = 0; i < 6; ++i) {
                    if (tri.texture == &other.ownedTextures[i]) {
                        tri.texture = &ownedTextures[i];
                        break;
                    }
                }
            }
        }
    }

    // After move: the old pointers pointed to other's ownedTextures which are now
    // moved into ours. Since std::array move moves elements, the addresses change.
    // We need to remap based on index.
    void fixupTexturePointersAfterMove(const Mesh& other) {
        for (auto& tri : triangles) {
            if (tri.texture) {
                for (int i = 0; i < 6; ++i) {
                    if (tri.texture == &other.ownedTextures[i]) {
                        tri.texture = &ownedTextures[i];
                        break;
                    }
                }
            }
        }
    }
};
