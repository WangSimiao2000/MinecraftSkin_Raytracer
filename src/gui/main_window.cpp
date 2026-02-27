#include "gui/main_window.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaObject>
#include <QScrollArea>
#include <QColorDialog>

#include "skin/skin_parser.h"
#include "skin/skin_fetcher.h"
#include "scene/mesh_builder.h"
#include "raytracer/tile_renderer.h"
#include "output/image_writer.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , poses_(getBuiltinPoses())
{
    setWindowTitle(tr("Minecraft 皮肤光线追踪渲染器"));
    resize(1100, 750);
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
    mainLayout->addWidget(preview_, 1);

    // --- Right: scrollable control panel ---
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMaximumWidth(280);
    auto* panelWidget = new QWidget();
    auto* panel = new QVBoxLayout(panelWidget);

    // Import button
    importBtn_ = new QPushButton(tr("导入皮肤"), this);
    panel->addWidget(importBtn_);

    // Username fetch
    auto* fetchGroup = new QGroupBox(tr("按用户名获取"), this);
    auto* fetchLayout = new QHBoxLayout(fetchGroup);
    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setPlaceholderText(tr("输入正版用户名"));
    fetchBtn_ = new QPushButton(tr("获取"), this);
    fetchLayout->addWidget(usernameEdit_);
    fetchLayout->addWidget(fetchBtn_);
    panel->addWidget(fetchGroup);

    skinFetcher_ = new SkinFetcher(this);
    connect(fetchBtn_, &QPushButton::clicked, this, &MainWindow::onFetchByUsername);
    connect(usernameEdit_, &QLineEdit::returnPressed, this, &MainWindow::onFetchByUsername);
    connect(skinFetcher_, &SkinFetcher::finished, this, &MainWindow::onSkinFetched);
    connect(skinFetcher_, &SkinFetcher::error, this, &MainWindow::onSkinFetchError);

    // Pose selector
    auto* poseGroup = new QGroupBox(tr("动作"), this);
    auto* poseLayout = new QHBoxLayout(poseGroup);
    poseCombo_ = new QComboBox(this);
    for (const auto& p : poses_) {
        poseCombo_->addItem(QString::fromStdString(p.name));
    }
    poseLayout->addWidget(poseCombo_);
    panel->addWidget(poseGroup);
    connect(poseCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPoseChanged);

    // Light position
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

    // Render settings
    auto* renderGroup = new QGroupBox(tr("渲染设置"), this);
    auto* renderForm = new QFormLayout(renderGroup);

    bounceCount_ = new QSpinBox(this);
    bounceCount_->setRange(0, 10);
    bounceCount_->setValue(2);
    renderForm->addRow(tr("反弹次数:"), bounceCount_);

    sppCount_ = new QSpinBox(this);
    sppCount_->setRange(1, 256);
    sppCount_->setValue(4);
    renderForm->addRow(tr("采样数 (AA):"), sppCount_);

    panel->addWidget(renderGroup);

    // Visual effects
    auto* fxGroup = new QGroupBox(tr("视觉效果"), this);
    auto* fxForm = new QFormLayout(fxGroup);

    gradientBgCheck_ = new QCheckBox(tr("径向渐变背景"), this);
    gradientBgCheck_->setChecked(true);
    fxForm->addRow(gradientBgCheck_);

    gradientScale_ = new QDoubleSpinBox(this);
    gradientScale_->setRange(0.1, 5.0);
    gradientScale_->setSingleStep(0.1);
    gradientScale_->setValue(1.0);
    fxForm->addRow(tr("渐变范围:"), gradientScale_);

    auto setButtonColor = [](QPushButton* btn, const QColor& color) {
        btn->setStyleSheet(
            QString("background-color: %1; border: 1px solid #888; min-height: 20px;")
                .arg(color.name()));
        btn->setText(color.name());
    };

    bgCenterColorBtn_ = new QPushButton(this);
    setButtonColor(bgCenterColorBtn_, bgCenterColor_);
    fxForm->addRow(tr("中心颜色:"), bgCenterColorBtn_);

    bgEdgeColorBtn_ = new QPushButton(this);
    setButtonColor(bgEdgeColorBtn_, bgEdgeColor_);
    fxForm->addRow(tr("边缘颜色:"), bgEdgeColorBtn_);

    connect(bgCenterColorBtn_, &QPushButton::clicked, this, [this, setButtonColor]() {
        QColor c = QColorDialog::getColor(bgCenterColor_, this, tr("选择中心颜色"));
        if (c.isValid()) {
            bgCenterColor_ = c;
            setButtonColor(bgCenterColorBtn_, c);
        }
    });
    connect(bgEdgeColorBtn_, &QPushButton::clicked, this, [this, setButtonColor]() {
        QColor c = QColorDialog::getColor(bgEdgeColor_, this, tr("选择边缘颜色"));
        if (c.isValid()) {
            bgEdgeColor_ = c;
            setButtonColor(bgEdgeColorBtn_, c);
        }
    });

    aoCheck_ = new QCheckBox(tr("环境光遮蔽 (AO)"), this);
    aoCheck_->setChecked(true);
    fxForm->addRow(aoCheck_);

    aoSamples_ = new QSpinBox(this);
    aoSamples_->setRange(4, 64);
    aoSamples_->setValue(16);
    fxForm->addRow(tr("AO 采样数:"), aoSamples_);

    dofCheck_ = new QCheckBox(tr("景深 (DOF)"), this);
    dofCheck_->setChecked(true);
    fxForm->addRow(dofCheck_);

    aperture_ = new QDoubleSpinBox(this);
    aperture_->setRange(0.0, 5.0);
    aperture_->setSingleStep(0.1);
    aperture_->setValue(0.3);
    fxForm->addRow(tr("光圈大小:"), aperture_);

    panel->addWidget(fxGroup);

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

    // Render & export
    renderBtn_ = new QPushButton(tr("渲染并导出"), this);
    panel->addWidget(renderBtn_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);
    panel->addWidget(progressBar_);

    panel->addStretch();
    scrollArea->setWidget(panelWidget);
    mainLayout->addWidget(scrollArea);

    // Connections
    connect(importBtn_, &QPushButton::clicked, this, &MainWindow::onImportSkin);
    connect(renderBtn_, &QPushButton::clicked, this, &MainWindow::onRenderExport);
    connect(lightX_, &QSlider::valueChanged, this, &MainWindow::onLightPosChanged);
    connect(lightY_, &QSlider::valueChanged, this, &MainWindow::onLightPosChanged);
    connect(lightZ_, &QSlider::valueChanged, this, &MainWindow::onLightPosChanged);
    connect(bounceCount_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onBounceCountChanged);

    // Default scene
    scene_ = MeshBuilder::buildDefaultScene();
    preview_->setScene(scene_);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void MainWindow::onImportSkin()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("导入皮肤文件"), QString(),
        tr("PNG 文件 (*.png);;所有文件 (*)"));
    if (filePath.isEmpty()) return;
    loadSkinFile(filePath);
}

void MainWindow::onFetchByUsername()
{
    QString username = usernameEdit_->text().trimmed();
    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请输入用户名"));
        return;
    }
    fetchBtn_->setEnabled(false);
    fetchBtn_->setText(tr("获取中..."));
    skinFetcher_->fetch(username);
}

void MainWindow::onSkinFetched(const QString& filePath)
{
    fetchBtn_->setEnabled(true);
    fetchBtn_->setText(tr("获取"));
    loadSkinFile(filePath);
}

void MainWindow::onSkinFetchError(const QString& message)
{
    fetchBtn_->setEnabled(true);
    fetchBtn_->setText(tr("获取"));
    QMessageBox::warning(this, tr("获取失败"), message);
}

void MainWindow::loadSkinFile(const QString& filePath)
{
    auto result = SkinParser::parse(filePath.toStdString());
    if (!result.isOk()) {
        QMessageBox::warning(this, tr("导入失败"), QString::fromStdString(*result.error));
        return;
    }

    currentSkin_ = *result.value;
    skinLoaded_ = true;
    rebuildScene();
}

void MainWindow::rebuildScene()
{
    int poseIdx = poseCombo_->currentIndex();
    Pose pose = (poseIdx >= 0 && poseIdx < static_cast<int>(poses_.size()))
                ? poses_[poseIdx] : Pose{};

    if (currentSkin_.has_value()) {
        scene_ = MeshBuilder::buildScene(*currentSkin_, pose);
    } else {
        // Default white model with pose
        scene_ = MeshBuilder::buildDefaultScene(pose);
    }

    scene_.light.position = Vec3(
        static_cast<float>(lightX_->value()),
        static_cast<float>(lightY_->value()),
        static_cast<float>(lightZ_->value()));

    preview_->setScene(scene_);
}

void MainWindow::onPoseChanged(int /*index*/)
{
    rebuildScene();
}

void MainWindow::onRenderExport()
{
    if (scene_.meshes.empty()) {
        QMessageBox::warning(this, tr("提示"), tr("场景为空，无法渲染"));
        return;
    }

    QString outputPath = QFileDialog::getSaveFileName(
        this, tr("保存渲染图像"), QString(),
        tr("PNG 文件 (*.png);;所有文件 (*)"));
    if (outputPath.isEmpty()) return;
    if (!outputPath.endsWith(".png", Qt::CaseInsensitive))
        outputPath += ".png";

    renderBtn_->setEnabled(false);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);

    // Disable all controls during rendering
    setControlsEnabled(false);

    RayTracer::Config config;
    config.width = outputWidth_->value();
    config.height = outputHeight_->value();
    config.maxBounces = bounceCountValue_;
    config.samplesPerPixel = sppCount_->value();
    config.tileSize = 32;
    config.threadCount = 0;

    // Visual effects
    config.gradientBg = gradientBgCheck_->isChecked();
    config.gradientScale = static_cast<float>(gradientScale_->value());
    config.bgCenter = Color(bgCenterColor_.redF(), bgCenterColor_.greenF(),
                            bgCenterColor_.blueF(), 1.0f);
    config.bgEdge = Color(bgEdgeColor_.redF(), bgEdgeColor_.greenF(),
                          bgEdgeColor_.blueF(), 1.0f);
    config.aoEnabled = aoCheck_->isChecked();
    config.aoSamples = aoSamples_->value();
    config.dofEnabled = dofCheck_->isChecked();
    config.aperture = static_cast<float>(aperture_->value());

    scene_.camera = preview_->currentCamera();

    Scene sceneCopy = scene_;
    std::string outPathStd = outputPath.toStdString();

    if (renderThread_.joinable())
        renderThread_.join();

    renderThread_ = std::thread([this, sceneCopy = std::move(sceneCopy),
                                  config, outPathStd]() {
        Image image = TileRenderer::render(sceneCopy, config,
            [this](int done, int total) {
                QMetaObject::invokeMethod(this,
                    [this, done, total]() { onRenderProgress(done, total); },
                    Qt::QueuedConnection);
            });

        bool ok = ImageWriter::writePNG(image, outPathStd);
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
    setControlsEnabled(true);
    progressBar_->setVisible(false);

    if (success) {
        QMessageBox::information(this, tr("渲染完成"),
            tr("渲染完成！图像已保存至：\n%1").arg(outputPath));
    } else {
        QMessageBox::warning(this, tr("保存失败"),
            tr("无法保存图像至：\n%1\n请检查文件路径是否可写。").arg(outputPath));
    }
}

void MainWindow::setControlsEnabled(bool enabled)
{
    importBtn_->setEnabled(enabled);
    usernameEdit_->setEnabled(enabled);
    fetchBtn_->setEnabled(enabled);
    poseCombo_->setEnabled(enabled);
    lightX_->setEnabled(enabled);
    lightY_->setEnabled(enabled);
    lightZ_->setEnabled(enabled);
    bounceCount_->setEnabled(enabled);
    sppCount_->setEnabled(enabled);
    outputWidth_->setEnabled(enabled);
    outputHeight_->setEnabled(enabled);
    gradientBgCheck_->setEnabled(enabled);
    gradientScale_->setEnabled(enabled);
    bgCenterColorBtn_->setEnabled(enabled);
    bgEdgeColorBtn_->setEnabled(enabled);
    aoCheck_->setEnabled(enabled);
    aoSamples_->setEnabled(enabled);
    dofCheck_->setEnabled(enabled);
    aperture_->setEnabled(enabled);
    renderBtn_->setEnabled(enabled);
    preview_->setInteractionEnabled(enabled);
}
