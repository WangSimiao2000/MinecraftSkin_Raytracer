#include "gui/main_window.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaObject>

#include "skin/skin_parser.h"
#include "scene/mesh_builder.h"
#include "raytracer/tile_renderer.h"
#include "output/image_writer.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Minecraft 皮肤光线追踪渲染器"));
    resize(1024, 720);
    setupUi();
}

MainWindow::~MainWindow()
{
    if (renderThread_.joinable())
        renderThread_.join();
}

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* mainLayout = new QHBoxLayout(central);

    // --- Left: preview ---
    preview_ = new RasterPreview(this);
    preview_->setMinimumSize(400, 400);
    mainLayout->addWidget(preview_, /*stretch=*/1);

    // --- Right: control panel ---
    auto* panel = new QVBoxLayout();

    // Import button
    importBtn_ = new QPushButton(tr("导入皮肤"), this);
    panel->addWidget(importBtn_);

    // Light position group
    auto* lightGroup = new QGroupBox(tr("光源位置"), this);
    auto* lightForm = new QFormLayout(lightGroup);

    auto makeSlider = [this](int defaultVal) {
        auto* s = new QSlider(Qt::Horizontal, this);
        s->setRange(-100, 100);
        s->setValue(defaultVal);
        return s;
    };

    lightX_ = makeSlider(0);
    lightY_ = makeSlider(40);
    lightZ_ = makeSlider(30);
    lightForm->addRow(tr("X:"), lightX_);
    lightForm->addRow(tr("Y:"), lightY_);
    lightForm->addRow(tr("Z:"), lightZ_);
    panel->addWidget(lightGroup);

    // Bounce count
    auto* bounceGroup = new QGroupBox(tr("反弹次数"), this);
    auto* bounceLayout = new QHBoxLayout(bounceGroup);
    bounceCount_ = new QSpinBox(this);
    bounceCount_->setRange(0, 10);
    bounceCount_->setValue(2);
    bounceLayout->addWidget(bounceCount_);
    panel->addWidget(bounceGroup);

    // Samples per pixel (Monte Carlo AA)
    auto* sppGroup = new QGroupBox(tr("采样数 (AA)"), this);
    auto* sppLayout = new QHBoxLayout(sppGroup);
    sppCount_ = new QSpinBox(this);
    sppCount_->setRange(1, 256);
    sppCount_->setValue(4);
    sppLayout->addWidget(sppCount_);
    panel->addWidget(sppGroup);

    // Output resolution
    auto* resGroup = new QGroupBox(tr("输出分辨率"), this);
    auto* resForm = new QFormLayout(resGroup);
    outputWidth_ = new QSpinBox(this);
    outputWidth_->setRange(64, 4096);
    outputWidth_->setValue(800);
    outputHeight_ = new QSpinBox(this);
    outputHeight_->setRange(64, 4096);
    outputHeight_->setValue(600);
    resForm->addRow(tr("宽度:"), outputWidth_);
    resForm->addRow(tr("高度:"), outputHeight_);
    panel->addWidget(resGroup);

    // Render & export button
    renderBtn_ = new QPushButton(tr("渲染并导出"), this);
    panel->addWidget(renderBtn_);

    // Progress bar (hidden by default)
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);
    panel->addWidget(progressBar_);

    panel->addStretch();
    mainLayout->addLayout(panel);

    // --- Connect signals ---
    connect(importBtn_, &QPushButton::clicked, this, &MainWindow::onImportSkin);
    connect(renderBtn_, &QPushButton::clicked, this, &MainWindow::onRenderExport);

    connect(lightX_, &QSlider::valueChanged, this, &MainWindow::onLightPosChanged);
    connect(lightY_, &QSlider::valueChanged, this, &MainWindow::onLightPosChanged);
    connect(lightZ_, &QSlider::valueChanged, this, &MainWindow::onLightPosChanged);

    connect(bounceCount_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onBounceCountChanged);

    // Show default white model on startup
    scene_ = MeshBuilder::buildDefaultScene();
    preview_->setScene(scene_);
}

// --- Slot implementations (Task 10.2) ---

void MainWindow::onImportSkin()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("导入皮肤文件"), QString(),
        tr("PNG 文件 (*.png);;所有文件 (*)"));

    if (filePath.isEmpty())
        return;

    auto result = SkinParser::parse(filePath.toStdString());
    if (!result.isOk()) {
        QMessageBox::warning(this, tr("导入失败"), QString::fromStdString(*result.error));
        return;
    }

    scene_ = MeshBuilder::buildScene(*result.value);

    // Apply current light slider values to the scene
    scene_.light.position = Vec3(
        static_cast<float>(lightX_->value()),
        static_cast<float>(lightY_->value()),
        static_cast<float>(lightZ_->value()));

    skinLoaded_ = true;
    preview_->setScene(scene_);
}

void MainWindow::onRenderExport()
{
    if (scene_.meshes.empty()) {
        QMessageBox::warning(this, tr("提示"), tr("场景为空，无法渲染"));
        return;
    }

    // Choose output file path
    QString outputPath = QFileDialog::getSaveFileName(
        this, tr("保存渲染图像"), QString(),
        tr("PNG 文件 (*.png);;所有文件 (*)"));

    if (outputPath.isEmpty())
        return;

    // Ensure .png extension
    if (!outputPath.endsWith(".png", Qt::CaseInsensitive))
        outputPath += ".png";

    // Disable render button to prevent double-click
    renderBtn_->setEnabled(false);

    // Show and reset progress bar
    progressBar_->setValue(0);
    progressBar_->setVisible(true);

    // Build render config from UI values
    RayTracer::Config config;
    config.width = outputWidth_->value();
    config.height = outputHeight_->value();
    config.maxBounces = bounceCountValue_;
    config.samplesPerPixel = sppCount_->value();
    config.tileSize = 32;
    config.threadCount = 0; // auto-detect

    // Sync camera from preview so the raytraced output matches the viewport
    scene_.camera = preview_->currentCamera();

    // Capture scene and path for the worker thread
    Scene sceneCopy = scene_;
    std::string outPathStd = outputPath.toStdString();

    // Join any previous render thread
    if (renderThread_.joinable())
        renderThread_.join();

    // Launch rendering in background thread
    renderThread_ = std::thread([this, sceneCopy = std::move(sceneCopy),
                                  config, outPathStd]() {
        // Render with progress callback that updates UI via queued connection
        Image image = TileRenderer::render(sceneCopy, config,
            [this](int done, int total) {
                QMetaObject::invokeMethod(this,
                    [this, done, total]() { onRenderProgress(done, total); },
                    Qt::QueuedConnection);
            });

        // Save PNG
        bool ok = ImageWriter::writePNG(image, outPathStd);

        // Notify UI on main thread
        QString path = QString::fromStdString(outPathStd);
        QMetaObject::invokeMethod(this,
            [this, path, ok]() { onRenderFinished(path, ok); },
            Qt::QueuedConnection);
    });
}

void MainWindow::onLightPosChanged()
{
    Vec3 pos(static_cast<float>(lightX_->value()),
             static_cast<float>(lightY_->value()),
             static_cast<float>(lightZ_->value()));
    scene_.light.position = pos;
    preview_->setLightPosition(pos);
}

void MainWindow::onBounceCountChanged(int value)
{
    bounceCountValue_ = value;
}

void MainWindow::onRenderProgress(int done, int total)
{
    progressBar_->setVisible(true);
    progressBar_->setMaximum(total);
    progressBar_->setValue(done);
}

void MainWindow::onRenderFinished(const QString& outputPath, bool success)
{
    // Re-enable render button
    renderBtn_->setEnabled(true);

    // Hide progress bar
    progressBar_->setVisible(false);

    if (success) {
        QMessageBox::information(this, tr("渲染完成"),
            tr("渲染完成！图像已保存至：\n%1").arg(outputPath));
    } else {
        QMessageBox::warning(this, tr("保存失败"),
            tr("无法保存图像至：\n%1\n请检查文件路径是否可写。").arg(outputPath));
    }
}
