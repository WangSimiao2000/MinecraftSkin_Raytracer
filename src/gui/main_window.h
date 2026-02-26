#pragma once

#include <QMainWindow>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <thread>

#include "gui/raster_preview.h"
#include "scene/scene.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onImportSkin();
    void onRenderExport();
    void onLightPosChanged();
    void onBounceCountChanged(int value);
    void onRenderProgress(int done, int total);
    void onRenderFinished(const QString& outputPath, bool success);

private:
    void setupUi();

    RasterPreview* preview_;
    QSlider* lightX_;
    QSlider* lightY_;
    QSlider* lightZ_;
    QSpinBox* bounceCount_;
    QSpinBox* outputWidth_;
    QSpinBox* outputHeight_;
    QPushButton* importBtn_;
    QPushButton* renderBtn_;
    QProgressBar* progressBar_;
    Scene scene_;
    bool skinLoaded_ = false;
    int bounceCountValue_ = 2;
    std::thread renderThread_;
};
