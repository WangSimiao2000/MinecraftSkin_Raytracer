/**
 * Unit tests for GUI pose control panel.
 *
 * Feature: pose-control
 * Tests: slider ranges, reset behavior, preset selection
 *
 * Requirements: 6.1, 6.5, 6.6
 */

#include <gtest/gtest.h>

#include <QApplication>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>

#include "gui/main_window.h"
#include "scene/pose.h"

// Ensure a QApplication exists for the entire test suite
class PoseControlGUITest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char appName[] = "test";
            static char* argv[] = { appName, nullptr };
            app_ = new QApplication(argc, argv);
        }
    }

    void SetUp() override {
        window_ = new MainWindow();
    }

    void TearDown() override {
        delete window_;
        window_ = nullptr;
    }

    // Helper: collect all rotation sliders (18 total: 6 parts × 3 axes)
    // and translation sliders (3 total) from the window.
    // MainWindow stores them as private members, so we access via findChildren
    // and identify them by their range.
    // The pose sliders all have range [-180, 180].
    // Light sliders have range [-100, 100].
    // We can distinguish them by range.
    QList<QSlider*> getPoseSliders() {
        QList<QSlider*> result;
        for (auto* s : window_->findChildren<QSlider*>()) {
            if (s->minimum() == -180 && s->maximum() == 180) {
                result.append(s);
            }
        }
        return result;
    }

    // In setupUi(), sliders are created in this order:
    //   Head rot(X,Y,Z), Body rot(X,Y,Z), Trans(X,Y,Z),
    //   RightArm rot(X,Y,Z), LeftArm rot(X,Y,Z),
    //   RightLeg rot(X,Y,Z), LeftLeg rot(X,Y,Z)
    // So translation sliders are at indices 6-8, not at the end.
    static constexpr int kTransSliderStart = 6;  // index of first translation slider
    static constexpr int kNumTransSliders = 3;

    // Get the rotation slider for a given body part and axis,
    // accounting for the translation sliders inserted after body (part 1).
    QSlider* getRotSlider(const QList<QSlider*>& all, int part, int axis) {
        int idx = part * 3 + axis;
        if (part >= 2) idx += kNumTransSliders; // skip translation sliders
        return all[idx];
    }

    QSlider* getTransSlider(const QList<QSlider*>& all, int axis) {
        return all[kTransSliderStart + axis];
    }

    QComboBox* getPoseCombo() {
        // There's only one QComboBox in MainWindow (poseCombo_)
        auto combos = window_->findChildren<QComboBox*>();
        return combos.isEmpty() ? nullptr : combos.first();
    }

    QPushButton* getResetButton() {
        // Find the reset button by its text "重置姿态"
        for (auto* btn : window_->findChildren<QPushButton*>()) {
            if (btn->text() == QString::fromUtf8("重置姿态")) {
                return btn;
            }
        }
        return nullptr;
    }

    MainWindow* window_ = nullptr;
    static QApplication* app_;
};

QApplication* PoseControlGUITest::app_ = nullptr;

// ── Test 1: All rotation sliders have range [-180, 180] ─────────────────────
// Validates: Requirement 6.1
TEST_F(PoseControlGUITest, SliderRangesAreCorrect) {
    auto sliders = getPoseSliders();

    // 6 body parts × 3 axes + 3 torso translation = 21 sliders with [-180, 180]
    ASSERT_EQ(sliders.size(), 21);

    for (auto* slider : sliders) {
        EXPECT_EQ(slider->minimum(), -180) << "Slider minimum should be -180";
        EXPECT_EQ(slider->maximum(), 180) << "Slider maximum should be 180";
    }
}

// ── Test 2: Reset button sets all slider values to 0 ────────────────────────
// Validates: Requirement 6.6
TEST_F(PoseControlGUITest, ResetSetsAllValuesToZero) {
    auto sliders = getPoseSliders();
    ASSERT_FALSE(sliders.isEmpty());

    // Set some non-zero values on sliders
    for (auto* slider : sliders) {
        slider->setValue(42);
    }

    // Click the reset button
    auto* resetBtn = getResetButton();
    ASSERT_NE(resetBtn, nullptr) << "Reset button not found";
    resetBtn->click();

    // All sliders should now be 0
    for (auto* slider : sliders) {
        EXPECT_EQ(slider->value(), 0) << "Slider value should be 0 after reset";
    }
}

// ── Test 3: Preset selection updates sliders correctly ──────────────────────
// Validates: Requirement 6.5
TEST_F(PoseControlGUITest, PresetSelectionUpdatesSliders) {
    auto* combo = getPoseCombo();
    ASSERT_NE(combo, nullptr);

    auto poses = getBuiltinPoses();
    ASSERT_GT(poses.size(), 1u) << "Need at least 2 builtin poses for this test";

    // Select the "Walking" preset (index 1)
    combo->setCurrentIndex(1);

    const Pose& walkPose = poses[1];
    const PartPose* parts[] = {
        &walkPose.head, &walkPose.body, &walkPose.rightArm,
        &walkPose.leftArm, &walkPose.rightLeg, &walkPose.leftLeg
    };

    auto allPoseSliders = getPoseSliders();
    ASSERT_EQ(allPoseSliders.size(), 21);

    // Verify each body part's rotation sliders match the preset
    for (int part = 0; part < 6; ++part) {
        float expected[] = { parts[part]->rotX, parts[part]->rotY, parts[part]->rotZ };
        for (int axis = 0; axis < 3; ++axis) {
            EXPECT_EQ(getRotSlider(allPoseSliders, part, axis)->value(),
                      static_cast<int>(expected[axis]))
                << "Part " << part << " axis " << axis
                << " should match preset value " << expected[axis];
        }
    }

    // Translation sliders should be 0 for walking preset (no torso translation)
    for (int axis = 0; axis < 3; ++axis) {
        EXPECT_EQ(getTransSlider(allPoseSliders, axis)->value(), 0)
            << "Translation axis " << axis << " should be 0 for walking preset";
    }
}
