#pragma once

#include "math/vec3.h"
#include <string>
#include <vector>

// Rotation angles (degrees) for each body part around its joint.
// Positive X = forward tilt, positive Y = outward rotation, positive Z = sideways tilt.
struct PartPose {
    float rotX = 0.0f; // pitch (forward/backward)
    float rotY = 0.0f; // yaw (left/right turn)
    float rotZ = 0.0f; // roll (sideways lean)
};

struct Pose {
    std::string name;
    PartPose head;
    PartPose body;
    PartPose rightArm;
    PartPose leftArm;
    PartPose rightLeg;
    PartPose leftLeg;
    Vec3 torsoTranslation{0.0f, 0.0f, 0.0f};
};

// Built-in pose library
inline std::vector<Pose> getBuiltinPoses() {
    std::vector<Pose> poses;

    // 0: Default (T-pose / standing)
    poses.push_back({"站立", {}, {}, {}, {}, {}, {}});

    // 1: Walking
    poses.push_back({"行走", 
        {0, 0.0f, 0},           // head
        {0, 0.0f, 0},           // body
        {30, 0.0f, 0},          // right arm forward
        {-30, 0.0f, 0},         // left arm backward
        {-25, 0.0f, 0},         // right leg backward
        {25, 0.0f, 0}           // left leg forward
    });

    // 2: Running
    poses.push_back({"奔跑",
        {-5, 0.0f, 0},          // head slightly down
        {5, 0.0f, 0},           // body leaning forward
        {50, 0.0f, 0},          // right arm far forward
        {-50, 0.0f, 0},         // left arm far backward
        {-45, 0.0f, 0},         // right leg backward
        {45, 0.0f, 0}           // left leg forward
    });

    // 3: Waving
    poses.push_back({"挥手",
        {5, 0.0f, 0},           // head tilted
        {0, 0.0f, 0},           // body
        {-140, 0.0f, -20},      // right arm raised and waving
        {0, 0.0f, 0},           // left arm relaxed
        {0, 0.0f, 0},           // right leg
        {0, 0.0f, 0}            // left leg
    });

    // 4: Sitting
    poses.push_back({"坐下",
        {0, 0.0f, 0},           // head
        {0, 0.0f, 0},           // body
        {-10, 0.0f, 0},         // right arm slightly back
        {-10, 0.0f, 0},         // left arm slightly back
        {-90, 0.0f, 0},         // right leg bent forward
        {-90, 0.0f, 0}          // left leg bent forward
    });

    // 5: Fighting stance
    poses.push_back({"战斗",
        {-10, 0.0f, 0},         // head looking forward
        {5, 0.0f, 0},           // body leaning
        {-90, 0.0f, 10},        // right arm raised
        {20, 0.0f, -10},        // left arm guard
        {-15, 0.0f, 0},         // right leg back
        {20, 0.0f, 0}           // left leg forward
    });

    // 6: Dabbing
    poses.push_back({"Dab",
        {30, 0.0f, 15},         // head tucked into arm
        {0, 0.0f, 5},           // body slight twist
        {-45, 0.0f, 30},        // right arm up diagonal
        {150, 0.0f, -10},       // left arm extended
        {0, 0.0f, 0},           // right leg
        {0, 0.0f, 0}            // left leg
    });

    return poses;
}
