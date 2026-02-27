#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QMatrix4x4>
#include <QTimer>
#include <vector>
#include <memory>
#include <set>

#include "gui/camera_controller.h"
#include "scene/scene.h"

// GPU-side mesh data for a single Mesh.
// Non-copyable/non-movable because QOpenGLVertexArrayObject is a QObject.
// Stored via unique_ptr in the vector.
struct GLMeshData {
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
    int vertexCount = 0;
    std::unique_ptr<QOpenGLTexture> texture;

    GLMeshData() = default;
    GLMeshData(const GLMeshData&) = delete;
    GLMeshData& operator=(const GLMeshData&) = delete;
    GLMeshData(GLMeshData&&) = delete;
    GLMeshData& operator=(GLMeshData&&) = delete;
};

class RasterPreview : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit RasterPreview(QWidget* parent = nullptr);
    ~RasterPreview() override;

    void setScene(const Scene& scene);
    void setLightPosition(const Vec3& pos);
    void setCameraRotation(float yaw, float pitch);
    Camera currentCamera() const;

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void uploadMeshes();
    void buildLightIndicator();
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;

    void onFrameTick();
    void enterFreeMode();
    void exitFreeMode();
    QMatrix4x4 freeViewMatrix() const;

    // Scene data
    const Scene* scene_ = nullptr;
    Vec3 lightPos_{5.0f, 30.0f, 20.0f};

    // Camera orbit
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 20.0f;
    float cameraDistance_ = 60.0f;

    // Mouse interaction
    QPoint lastMousePos_;

    // Shaders
    std::unique_ptr<QOpenGLShaderProgram> meshShader_;
    std::unique_ptr<QOpenGLShaderProgram> lightShader_;

    // GPU mesh data
    std::vector<std::unique_ptr<GLMeshData>> glMeshes_;

    // Light indicator geometry
    QOpenGLVertexArrayObject lightVao_;
    QOpenGLBuffer lightVbo_{QOpenGLBuffer::VertexBuffer};
    int lightVertexCount_ = 0;

    bool initialized_ = false;

    // Free camera mode
    CameraMode cameraMode_ = CameraMode::Orbit;
    CameraController cameraController_;
    QTimer* frameTimer_ = nullptr;
    std::set<int> pressedKeys_;
};
