# MCSkin RaytraceRenderer

Minecraft 皮肤光线追踪渲染器 —— 一个基于纯 CPU 软件光线追踪的桌面应用，可将标准 Minecraft 皮肤文件渲染为高质量图像。

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt6](https://img.shields.io/badge/Qt-6-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## 功能特性

- **皮肤导入**：支持 64×64（新版）和 64×32（旧版）Minecraft 皮肤 PNG 文件，自动识别格式
- **3D 网格构建**：自动将皮肤纹理映射到标准 Minecraft 角色盒体模型（头部、躯干、四肢），支持内层和外层皮肤
- **光线追踪渲染**：Blinn-Phong 光照模型，支持漫反射、镜面高光、阴影和多次反射
- **多线程加速**：基于图块（tile）的并行渲染，自动利用所有 CPU 核心
- **实时预览**：OpenGL 光栅化预览窗口，支持鼠标拖拽旋转和滚轮缩放
- **参数调节**：光源位置（XYZ 滑块）、反弹次数（0-10）、输出分辨率（64-4096）
- **PNG 导出**：渲染结果导出为 PNG 文件，带进度条显示

## 项目结构

```
.
├── CMakeLists.txt              # 根构建配置
├── src/
│   ├── main.cpp                # 程序入口
│   ├── math/                   # 基础数学类型（Vec3, Color, Ray）
│   ├── skin/                   # 皮肤解析（SkinParser, Image, TextureRegion）
│   ├── scene/                  # 场景数据（Triangle, Mesh, Scene, Camera, MeshBuilder）
│   ├── raytracer/              # 光线追踪核心（Intersection, Shading, RayTracer, TileRenderer）
│   ├── output/                 # 图像输出（ImageWriter）
│   └── gui/                    # Qt GUI（RasterPreview, MainWindow）
├── tests/                      # 单元测试 + 属性测试（138 个测试用例）
└── third_party/stb/            # stb_image / stb_image_write 头文件
```

## 依赖项

| 依赖 | 版本要求 | 说明 |
|------|---------|------|
| CMake | ≥ 3.22 | 构建系统 |
| C++ 编译器 | 支持 C++17 | GCC 9+ / Clang 10+ |
| Qt 6 | ≥ 6.2 | GUI 框架（Widgets, OpenGL, OpenGLWidgets） |
| GLM | 任意 | 数学库（通过系统包管理器安装） |
| Google Test | 任意 | 单元测试框架 |
| RapidCheck | 自动获取 | 属性测试框架（CMake FetchContent 自动下载） |
| stb | 已内置 | PNG 读写（third_party/ 目录下） |
| OpenGL | ≥ 3.3 | 实时预览渲染 |

## 编译

### Ubuntu / Debian

安装依赖：

```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    qt6-base-dev libqt6opengl6-dev \
    libglm-dev libgtest-dev \
    libgl-dev
```

构建项目：

```bash
# 克隆项目（如果还没有）
# git clone <repo-url> && cd MCSkin_RaytraceRenderer

# 创建构建目录并编译
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

> RapidCheck 会在首次构建时通过 CMake FetchContent 自动从 GitHub 下载，无需手动安装。

### Arch Linux

```bash
sudo pacman -S base-devel cmake qt6-base glm gtest
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Fedora

```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel glm-devel gtest-devel mesa-libGL-devel
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## 运行

```bash
# 启动 GUI 应用
./build/src/mcskin_raytracer
```

### 使用步骤

1. 点击 **「导入皮肤」** 按钮，选择一个 Minecraft 皮肤 PNG 文件（64×64 或 64×32）
2. 预览窗口会立即显示 3D 模型，可用鼠标拖拽旋转、滚轮缩放
3. 调整右侧控制面板的参数：
   - **光源位置**：拖动 X / Y / Z 滑块，预览实时更新
   - **反弹次数**：设置光线最大反射次数（0 = 仅直接光照，越大越真实但越慢）
   - **输出分辨率**：设置导出图像的宽高（像素）
4. 点击 **「渲染并导出」**，选择保存路径，等待进度条完成
5. 渲染完成后会弹出提示，显示输出文件路径

## 运行测试

```bash
# 运行全部 138 个测试（单元测试 + 属性测试）
./build/tests/mcskin_tests

# 运行特定测试套件
./build/tests/mcskin_tests --gtest_filter="SkinParser*"
./build/tests/mcskin_tests --gtest_filter="MeshBuilder*"
./build/tests/mcskin_tests --gtest_filter="TileRenderer*"

# 查看所有可用测试
./build/tests/mcskin_tests --gtest_list_tests
```

### 测试覆盖

项目包含 138 个测试用例，覆盖 14 个属性测试（每个至少 100 次随机迭代）：

| 模块 | 单元测试 | 属性测试 |
|------|---------|---------|
| 数学类型（Vec3, Color） | ✅ | — |
| 皮肤解析（SkinParser） | ✅ | 属性 1-3：区域正确性、旧版镜像、无效文件拒绝 |
| 网格构建（MeshBuilder） | ✅ | 属性 4-6：完整性、外层扩展、透明跳过 |
| 光照着色（Shading） | ✅ | 属性 7-8：Blinn-Phong 正确性、阴影射线 |
| 光线追踪（RayTracer） | ✅ | 属性 9-10：递归深度、透明穿透 |
| 图块渲染（TileRenderer） | ✅ | 属性 11-12：完整覆盖、多线程确定性 |
| 图像输出（ImageWriter） | ✅ | 属性 13-14：分辨率一致性、有效 PNG |

## 技术架构

```
皮肤 PNG ──→ SkinParser ──→ SkinData ──→ MeshBuilder ──→ Scene
                                                          │
                                          ┌───────────────┼───────────────┐
                                          ▼               ▼               ▼
                                    RasterPreview    TileRenderer    GUI 控制面板
                                    (OpenGL 预览)   (多线程光追)    (参数调节)
                                                          │
                                                          ▼
                                                    ImageWriter ──→ PNG 文件
```

- **渲染核心**：纯 CPU 软件光线追踪，slab method 光线-AABB 求交，Blinn-Phong 着色
- **并行策略**：将图像划分为 32×32 图块，工作线程通过原子计数器抢占式分配任务
- **预览**：OpenGL 3.3 Core Profile，每个网格构建纹理图集（atlas），Blinn-Phong shader
- **线程安全**：渲染在后台 `std::thread` 执行，通过 `QMetaObject::invokeMethod(Qt::QueuedConnection)` 更新 UI
