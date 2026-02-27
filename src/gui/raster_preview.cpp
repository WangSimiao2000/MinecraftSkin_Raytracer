#include "gui/raster_preview.h"
#include <cmath>
#include <QCursor>
#include <QFont>
#include <QPainter>
#include <QVector3D>

// ─── Shader sources ─────────────────────────────────────────────────────────

static const char* meshVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = uProjection * uView * worldPos;
}
)";

static const char* meshFragmentShader = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;

uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.01) discard;

    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;  // two-sided lighting
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 H = normalize(L + V);

    float ambient = 0.15;
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 color = ambient * texColor.rgb
               + 0.7 * diff * texColor.rgb
               + 0.3 * spec * vec3(1.0);

    FragColor = vec4(color, texColor.a);
}
)";

static const char* lightVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* lightFragmentShader = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// ─── Helper: generate UV sphere vertices ────────────────────────────────────

static std::vector<float> generateSphereVertices(float radius, int stacks, int sectors) {
    std::vector<float> verts;
    const float PI = 3.14159265358979f;

    // Generate positions
    std::vector<float> positions;
    for (int i = 0; i <= stacks; ++i) {
        float phi = PI * float(i) / float(stacks);
        for (int j = 0; j <= sectors; ++j) {
            float theta = 2.0f * PI * float(j) / float(sectors);
            float x = radius * sinf(phi) * cosf(theta);
            float y = radius * cosf(phi);
            float z = radius * sinf(phi) * sinf(theta);
            positions.push_back(x);
            positions.push_back(y);
            positions.push_back(z);
        }
    }

    // Generate triangles
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            int cur = i * (sectors + 1) + j;
            int next = cur + sectors + 1;

            auto addVert = [&](int idx) {
                verts.push_back(positions[idx * 3]);
                verts.push_back(positions[idx * 3 + 1]);
                verts.push_back(positions[idx * 3 + 2]);
            };

            addVert(cur);
            addVert(next);
            addVert(cur + 1);

            addVert(cur + 1);
            addVert(next);
            addVert(next + 1);
        }
    }
    return verts;
}

// ─── Constructor / Destructor ───────────────────────────────────────────────

RasterPreview::RasterPreview(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(400, 300);
    setFocusPolicy(Qt::StrongFocus);

    frameTimer_ = new QTimer(this);
    frameTimer_->setInterval(16); // ~60fps
    connect(frameTimer_, &QTimer::timeout, this, &RasterPreview::onFrameTick);
}

RasterPreview::~RasterPreview() {
    makeCurrent();
    glMeshes_.clear();
    lightVao_.destroy();
    lightVbo_.destroy();
    doneCurrent();
}

// ─── Public API ─────────────────────────────────────────────────────────────

void RasterPreview::setScene(const Scene& scene) {
    scene_ = &scene;
    lightPos_ = scene.light.position;
    if (initialized_) {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }
    update();
}

void RasterPreview::setLightPosition(const Vec3& pos) {
    lightPos_ = pos;
    update();
}

void RasterPreview::setCameraRotation(float yaw, float pitch) {
    cameraYaw_ = yaw;
    cameraPitch_ = pitch;
    update();
}

void RasterPreview::setInteractionEnabled(bool enabled) {
    interactionEnabled_ = enabled;
    if (!enabled && cameraMode_ == CameraMode::Free) {
        exitFreeMode();
    }
}

Camera RasterPreview::currentCamera() const {
    Camera cam;
    cam.up = Vec3(0.0f, 1.0f, 0.0f);
    cam.fov = 45.0f; // matches projectionMatrix()

    if (cameraMode_ == CameraMode::Free) {
        cam.position = cameraController_.position();
        cam.target = cameraController_.target();
    } else {
        float yawRad = cameraYaw_ * 3.14159265f / 180.0f;
        float pitchRad = cameraPitch_ * 3.14159265f / 180.0f;
        float camX = cameraDistance_ * cosf(pitchRad) * sinf(yawRad);
        float camY = cameraDistance_ * sinf(pitchRad);
        float camZ = cameraDistance_ * cosf(pitchRad) * cosf(yawRad);
        cam.position = Vec3(camX, camY + 18.0f, camZ);
        cam.target = Vec3(0.0f, 18.0f, 0.0f);
    }
    return cam;
}

// ─── OpenGL lifecycle ───────────────────────────────────────────────────────

void RasterPreview::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Compile mesh shader
    meshShader_ = std::make_unique<QOpenGLShaderProgram>();
    meshShader_->addShaderFromSourceCode(QOpenGLShader::Vertex, meshVertexShader);
    meshShader_->addShaderFromSourceCode(QOpenGLShader::Fragment, meshFragmentShader);
    meshShader_->link();

    // Compile light indicator shader
    lightShader_ = std::make_unique<QOpenGLShaderProgram>();
    lightShader_->addShaderFromSourceCode(QOpenGLShader::Vertex, lightVertexShader);
    lightShader_->addShaderFromSourceCode(QOpenGLShader::Fragment, lightFragmentShader);
    lightShader_->link();

    buildLightIndicator();

    initialized_ = true;

    if (scene_) {
        uploadMeshes();
    }
}

void RasterPreview::paintGL() {
    // Re-apply GL state (QPainter may have changed it in the previous frame)
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view;
    QVector3D viewPos;

    if (cameraMode_ == CameraMode::Free) {
        view = freeViewMatrix();
        Vec3 pos = cameraController_.position();
        viewPos = QVector3D(pos.x, pos.y, pos.z);
    } else {
        view = viewMatrix();
        float yawRad = cameraYaw_ * 3.14159265f / 180.0f;
        float pitchRad = cameraPitch_ * 3.14159265f / 180.0f;
        float camX = cameraDistance_ * cosf(pitchRad) * sinf(yawRad);
        float camY = cameraDistance_ * sinf(pitchRad);
        float camZ = cameraDistance_ * cosf(pitchRad) * cosf(yawRad);
        viewPos = QVector3D(camX, camY + 18.0f, camZ);
    }

    QMatrix4x4 proj = projectionMatrix();
    QMatrix4x4 model; // identity

    // ── Draw meshes ──
    if (!glMeshes_.empty()) {
        meshShader_->bind();
        meshShader_->setUniformValue("uModel", model);
        meshShader_->setUniformValue("uView", view);
        meshShader_->setUniformValue("uProjection", proj);
        meshShader_->setUniformValue("uLightPos",
            QVector3D(lightPos_.x, lightPos_.y, lightPos_.z));
        meshShader_->setUniformValue("uViewPos", viewPos);

        for (auto& gm : glMeshes_) {
            if (gm->vertexCount == 0) continue;
            gm->vao.bind();
            if (gm->texture) {
                gm->texture->bind(0);
                meshShader_->setUniformValue("uTexture", 0);
            }
            glDrawArrays(GL_TRIANGLES, 0, gm->vertexCount);
            gm->vao.release();
        }
        meshShader_->release();
    }

    // ── Draw light indicator ──
    if (lightVertexCount_ > 0) {
        lightShader_->bind();
        QMatrix4x4 lightModel;
        lightModel.translate(lightPos_.x, lightPos_.y, lightPos_.z);
        QMatrix4x4 mvp = proj * view * lightModel;
        lightShader_->setUniformValue("uMVP", mvp);
        lightShader_->setUniformValue("uColor", QVector3D(1.0f, 1.0f, 0.0f));

        lightVao_.bind();
        glDrawArrays(GL_TRIANGLES, 0, lightVertexCount_);
        lightVao_.release();
        lightShader_->release();
    }

    // ── Draw mode indicator text ──
    QPainter painter(this);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Sans", 10));
    QString modeText = (cameraMode_ == CameraMode::Free)
        ? QString::fromUtf8("自由模式")
        : QString::fromUtf8("轨道模式");
    painter.drawText(10, height() - 10, modeText);
    painter.end();
}

void RasterPreview::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

// ─── Mouse interaction ──────────────────────────────────────────────────────

void RasterPreview::mousePressEvent(QMouseEvent* event) {
    if (!interactionEnabled_) return;
    if (event->button() == Qt::RightButton) {
        if (cameraMode_ == CameraMode::Orbit) {
            enterFreeMode();
        } else {
            exitFreeMode();
        }
        return;
    }
    lastMousePos_ = event->pos();
}

void RasterPreview::mouseMoveEvent(QMouseEvent* event) {
    if (!interactionEnabled_) return;
    if (cameraMode_ == CameraMode::Free) {
        QPoint center = QPoint(width() / 2, height() / 2);
        int dx = event->pos().x() - center.x();
        int dy = event->pos().y() - center.y();

        if (dx != 0 || dy != 0) {
            cameraController_.handleMouseMove(static_cast<float>(dx), static_cast<float>(dy));
            QCursor::setPos(mapToGlobal(center));
        }
        return;
    }

    int dx = event->pos().x() - lastMousePos_.x();
    int dy = event->pos().y() - lastMousePos_.y();
    lastMousePos_ = event->pos();

    if (event->buttons() & Qt::LeftButton) {
        cameraYaw_ -= dx * 0.5f;
        cameraPitch_ += dy * 0.3f;
        cameraPitch_ = std::clamp(cameraPitch_, -89.0f, 89.0f);
        update();
    }
}

void RasterPreview::wheelEvent(QWheelEvent* event) {
    if (!interactionEnabled_) return;
    float delta = event->angleDelta().y() / 120.0f;
    cameraDistance_ -= delta * 3.0f;
    cameraDistance_ = std::clamp(cameraDistance_, 10.0f, 200.0f);
    update();
}

// ─── Keyboard / focus handling ──────────────────────────────────────────────

void RasterPreview::keyPressEvent(QKeyEvent* event) {
    if (!interactionEnabled_) return;
    if (cameraMode_ != CameraMode::Free) {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }

    int key = event->key();
    pressedKeys_.insert(key);

    switch (key) {
        case Qt::Key_W: cameraController_.setMoveFlag(CameraController::Forward, true); break;
        case Qt::Key_S: cameraController_.setMoveFlag(CameraController::Backward, true); break;
        case Qt::Key_A: cameraController_.setMoveFlag(CameraController::Left, true); break;
        case Qt::Key_D: cameraController_.setMoveFlag(CameraController::Right, true); break;
        default: QOpenGLWidget::keyPressEvent(event); return;
    }
}

void RasterPreview::keyReleaseEvent(QKeyEvent* event) {
    if (!interactionEnabled_) return;
    if (cameraMode_ != CameraMode::Free) {
        QOpenGLWidget::keyReleaseEvent(event);
        return;
    }

    int key = event->key();
    pressedKeys_.erase(key);

    switch (key) {
        case Qt::Key_W: cameraController_.setMoveFlag(CameraController::Forward, false); break;
        case Qt::Key_S: cameraController_.setMoveFlag(CameraController::Backward, false); break;
        case Qt::Key_A: cameraController_.setMoveFlag(CameraController::Left, false); break;
        case Qt::Key_D: cameraController_.setMoveFlag(CameraController::Right, false); break;
        default: QOpenGLWidget::keyReleaseEvent(event); return;
    }
}

void RasterPreview::focusOutEvent(QFocusEvent* event) {
    if (cameraMode_ == CameraMode::Free) {
        exitFreeMode();
    }
    QOpenGLWidget::focusOutEvent(event);
}

// ─── Frame tick / free view matrix stubs (implemented in tasks 3.4, 3.5) ────

void RasterPreview::onFrameTick() {
    cameraController_.update();
    update();
}

QMatrix4x4 RasterPreview::freeViewMatrix() const {
    Vec3 pos = cameraController_.position();
    Vec3 tgt = cameraController_.target();

    QVector3D eye(pos.x, pos.y, pos.z);
    QVector3D center(tgt.x, tgt.y, tgt.z);
    QVector3D up(0.0f, 1.0f, 0.0f);

    QMatrix4x4 v;
    v.lookAt(eye, center, up);
    return v;
}

// ─── Mode switching ─────────────────────────────────────────────────────────

void RasterPreview::enterFreeMode() {
    cameraMode_ = CameraMode::Free;

    // Compute eye position from orbit parameters
    float yawRad = cameraYaw_ * 3.14159265f / 180.0f;
    float pitchRad = cameraPitch_ * 3.14159265f / 180.0f;
    float camX = cameraDistance_ * cosf(pitchRad) * sinf(yawRad);
    float camY = cameraDistance_ * sinf(pitchRad);
    float camZ = cameraDistance_ * cosf(pitchRad) * cosf(yawRad);
    Vec3 eyePos(camX, camY + 18.0f, camZ);

    // The orbit camera looks at (0, 18, 0). Compute yaw/pitch for CameraController.
    // CameraController forward: (cos(pitch)*sin(yaw), sin(pitch), -cos(pitch)*cos(yaw))
    // Direction from eye to center = (0,18,0) - eyePos
    Vec3 center(0.0f, 18.0f, 0.0f);
    Vec3 dir = (center - eyePos).normalize();

    // Extract yaw and pitch from direction vector
    float freePitch = std::asin(std::clamp(dir.y, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
    float freeYaw = std::atan2(dir.x, -dir.z) * 180.0f / 3.14159265f;

    cameraController_.setPosition(eyePos);
    cameraController_.setYawPitch(freeYaw, freePitch);

    // Hide cursor and grab mouse
    setCursor(Qt::BlankCursor);
    setMouseTracking(true);
    setFocus();

    // Start frame timer
    if (frameTimer_) {
        frameTimer_->start();
    }

    update();
}

void RasterPreview::exitFreeMode() {
    cameraMode_ = CameraMode::Orbit;

    // Recover orbit parameters from CameraController state
    Vec3 pos = cameraController_.position();
    Vec3 center(0.0f, 18.0f, 0.0f);
    Vec3 offset = pos - center;

    cameraDistance_ = offset.length();
    cameraDistance_ = std::clamp(cameraDistance_, 10.0f, 200.0f);

    // Orbit yaw/pitch from offset vector
    cameraYaw_ = std::atan2(offset.x, offset.z) * 180.0f / 3.14159265f;
    cameraPitch_ = std::asin(std::clamp(offset.y / cameraDistance_, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
    cameraPitch_ = std::clamp(cameraPitch_, -89.0f, 89.0f);

    // Restore cursor
    setCursor(Qt::ArrowCursor);
    setMouseTracking(false);

    // Clear pressed keys
    pressedKeys_.clear();
    cameraController_.setMoveFlag(CameraController::Forward, false);
    cameraController_.setMoveFlag(CameraController::Backward, false);
    cameraController_.setMoveFlag(CameraController::Left, false);
    cameraController_.setMoveFlag(CameraController::Right, false);

    // Stop frame timer
    if (frameTimer_) {
        frameTimer_->stop();
    }

    update();
}

// ─── Camera matrices ────────────────────────────────────────────────────────

QMatrix4x4 RasterPreview::viewMatrix() const {
    float yawRad = cameraYaw_ * 3.14159265f / 180.0f;
    float pitchRad = cameraPitch_ * 3.14159265f / 180.0f;

    float camX = cameraDistance_ * cosf(pitchRad) * sinf(yawRad);
    float camY = cameraDistance_ * sinf(pitchRad);
    float camZ = cameraDistance_ * cosf(pitchRad) * cosf(yawRad);

    // Orbit around the center of the character model (~y=18)
    QVector3D eye(camX, camY + 18.0f, camZ);
    QVector3D center(0.0f, 18.0f, 0.0f);
    QVector3D up(0.0f, 1.0f, 0.0f);

    QMatrix4x4 v;
    v.lookAt(eye, center, up);
    return v;
}

QMatrix4x4 RasterPreview::projectionMatrix() const {
    QMatrix4x4 p;
    float aspect = float(width()) / float(std::max(1, height()));
    p.perspective(45.0f, aspect, 0.1f, 500.0f);
    return p;
}

// ─── Upload mesh data to GPU ────────────────────────────────────────────────

void RasterPreview::uploadMeshes() {
    glMeshes_.clear();
    if (!scene_) return;

    // For each Mesh, we build a texture atlas from its 6 owned face textures
    // (front=0, back=1, left=2, right=3, top=4, bottom=5) laid out in a
    // 3-column x 2-row grid. Each cell has uniform size (maxW x maxH).
    // Triangle UVs are remapped from [0,1] per-face into atlas sub-regions.

    for (const auto& mesh : scene_->meshes) {
        auto gm = std::make_unique<GLMeshData>();

        // Find max face texture dimensions for uniform atlas cells
        int maxW = 1, maxH = 1;
        for (int i = 0; i < 6; ++i) {
            maxW = std::max(maxW, mesh.ownedTextures[i].width);
            maxH = std::max(maxH, mesh.ownedTextures[i].height);
        }

        int atlasW = maxW * 3;  // 3 columns
        int atlasH = maxH * 2;  // 2 rows

        // Build atlas pixel data (RGBA8)
        std::vector<unsigned char> atlasPixels(atlasW * atlasH * 4, 0);

        for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
            const auto& tex = mesh.ownedTextures[faceIdx];
            int col = faceIdx % 3;
            int row = faceIdx / 3;
            int offX = col * maxW;
            int offY = row * maxH;

            for (int y = 0; y < tex.height && y < maxH; ++y) {
                for (int x = 0; x < tex.width && x < maxW; ++x) {
                    const Color& c = tex.pixels[y * tex.width + x];
                    int dstIdx = ((offY + y) * atlasW + (offX + x)) * 4;
                    atlasPixels[dstIdx + 0] = static_cast<unsigned char>(std::clamp(c.r, 0.0f, 1.0f) * 255.0f + 0.5f);
                    atlasPixels[dstIdx + 1] = static_cast<unsigned char>(std::clamp(c.g, 0.0f, 1.0f) * 255.0f + 0.5f);
                    atlasPixels[dstIdx + 2] = static_cast<unsigned char>(std::clamp(c.b, 0.0f, 1.0f) * 255.0f + 0.5f);
                    atlasPixels[dstIdx + 3] = static_cast<unsigned char>(std::clamp(c.a, 0.0f, 1.0f) * 255.0f + 0.5f);
                }
            }
        }

        // Create OpenGL texture
        gm->texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        gm->texture->create();
        gm->texture->setSize(atlasW, atlasH);
        gm->texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        gm->texture->allocateStorage();
        gm->texture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, atlasPixels.data());
        gm->texture->setMinificationFilter(QOpenGLTexture::Nearest);
        gm->texture->setMagnificationFilter(QOpenGLTexture::Nearest);
        gm->texture->setWrapMode(QOpenGLTexture::ClampToEdge);

        // Build vertex data with remapped UVs into atlas space
        std::vector<float> vertexData;
        vertexData.reserve(mesh.triangles.size() * 3 * 8);

        for (const auto& tri : mesh.triangles) {
            // Determine which face index this triangle belongs to
            int faceIdx = -1;
            if (tri.texture) {
                for (int i = 0; i < 6; ++i) {
                    if (tri.texture == &mesh.ownedTextures[i]) {
                        faceIdx = i;
                        break;
                    }
                }
            }

            // Compute atlas UV offset and scale for this face
            float uOff = 0.0f, vOff = 0.0f;
            float uScale = 1.0f / 3.0f;
            float vScale = 1.0f / 2.0f;
            if (faceIdx >= 0) {
                int col = faceIdx % 3;
                int row = faceIdx / 3;
                uOff = float(col) / 3.0f;
                vOff = float(row) / 2.0f;

                // Account for actual texture size within the cell
                const auto& faceTex = mesh.ownedTextures[faceIdx];
                if (faceTex.width > 0 && faceTex.height > 0) {
                    uScale = float(faceTex.width) / float(atlasW);
                    vScale = float(faceTex.height) / float(atlasH);
                }
            }

            auto remapU = [&](float u) { return uOff + u * uScale; };
            auto remapV = [&](float v) { return vOff + v * vScale; };

            auto addVertex = [&](const Vec3& pos, const Vec3& n, float u, float v) {
                vertexData.push_back(pos.x);
                vertexData.push_back(pos.y);
                vertexData.push_back(pos.z);
                vertexData.push_back(n.x);
                vertexData.push_back(n.y);
                vertexData.push_back(n.z);
                vertexData.push_back(remapU(u));
                vertexData.push_back(remapV(v));
            };

            addVertex(tri.v0, tri.normal, tri.u0, tri.v0_uv);
            addVertex(tri.v1, tri.normal, tri.u1, tri.v1_uv);
            addVertex(tri.v2, tri.normal, tri.u2, tri.v2_uv);
        }

        gm->vertexCount = static_cast<int>(mesh.triangles.size() * 3);

        // Create VAO/VBO
        gm->vao.create();
        gm->vao.bind();

        gm->vbo.create();
        gm->vbo.bind();
        gm->vbo.allocate(vertexData.data(),
                         static_cast<int>(vertexData.size() * sizeof(float)));

        int stride = 8 * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(6 * sizeof(float)));

        gm->vbo.release();
        gm->vao.release();

        glMeshes_.push_back(std::move(gm));
    }
}

// ─── Light indicator ────────────────────────────────────────────────────────

void RasterPreview::buildLightIndicator() {
    auto verts = generateSphereVertices(0.8f, 8, 8);
    lightVertexCount_ = static_cast<int>(verts.size() / 3);

    lightVao_.create();
    lightVao_.bind();

    lightVbo_.create();
    lightVbo_.bind();
    lightVbo_.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(float)));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    lightVbo_.release();
    lightVao_.release();
}
