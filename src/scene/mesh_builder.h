#pragma once

#include "skin/skin_parser.h"
#include "scene/scene.h"
#include "scene/pose.h"

class MeshBuilder {
public:
    // Build the complete character scene from skin data with optional pose
    static Scene buildScene(const SkinData& skin, const Pose& pose = Pose{});

    // Build a default white-model scene (no skin imported)
    static Scene buildDefaultScene();

    // Build a single box mesh for a body part
    // position: center of the box
    // size: (width, height, depth)
    // offset: expansion amount (0 for inner, 0.5 for outer)
    static Mesh buildBox(const BodyPartTexture& tex,
                         const Vec3& position,
                         const Vec3& size,
                         float offset);

    // Build a box mesh with rotation around a pivot point
    static Mesh buildBoxWithPose(const BodyPartTexture& tex,
                                 const Vec3& position,
                                 const Vec3& size,
                                 float offset,
                                 const Vec3& pivot,
                                 const PartPose& partPose);

    // Check if all pixels in all 6 faces of a BodyPartTexture are fully transparent
    static bool isFullyTransparent(const BodyPartTexture& tex);
};
