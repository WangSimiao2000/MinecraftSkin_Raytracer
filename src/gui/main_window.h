#pragma once

#include <QMainWindow>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <thread>

#include "gui/raster_preview.h"
#include "scene/scene.h"
#include "scene/pose.h"
#include "skin/skin_parser.h"

class SkinFetcher;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onImportSkin();
    void onFetchByUsername();
    void onSkinFetched(const QString& filePath);
    void onSkinFetchError(const QString& message);
    void onRenderExport();
    void onLightPosChanged();
    void onBounceCountChanged(int value);
    void onPoseChanged(int index);
    void onRenderProgress(int done, int total);
    void onRenderFinished(const QString& outputPath, bool success);

private:
    void setupUi();
    void loadSkinFile(const QString& filePath);
    void rebuildScene();
    void setControlsEnabled(bool enabled);

    RasterPreview* preview_;
    QSlider* lightX_;
    QSlider* lightY_;
    QSlider* lightZ_;
    QSpinBox* bounceCount_;
    QSpinBox* sppCount_;
    QSpinBox* outputWidth_;
    QSpinBox* outputHeight_;
    QPushButton* importBtn_;
    QLineEdit* usernameEdit_;
    QPushButton* fetchBtn_;
    QComboBox* poseCombo_;
    QCheckBox* aoCheck_;
    QSpinBox* aoSamples_;
    QCheckBox* dofCheck_;
    QDoubleSpinBox* aperture_;
    QCheckBox* gradientBgCheck_;
    QPushButton* renderBtn_;
    QProgressBar* progressBar_;
    SkinFetcher* skinFetcher_;
    Scene scene_;
    std::optional<SkinData> currentSkin_;
    std::vector<Pose> poses_;
    bool skinLoaded_ = false;
    int bounceCountValue_ = 2;
    std::thread renderThread_;
};
