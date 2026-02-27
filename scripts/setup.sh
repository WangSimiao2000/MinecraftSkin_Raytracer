#!/usr/bin/env bash
#
# MCSkin RaytraceRenderer — 一键安装依赖脚本
# 支持: Ubuntu/Debian, Fedora/RHEL, Arch Linux, macOS (Homebrew)
#
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ -f /etc/os-release ]]; then
        . /etc/os-release
        case "$ID" in
            ubuntu|debian|linuxmint|pop) echo "debian" ;;
            fedora|rhel|centos|rocky|alma) echo "fedora" ;;
            arch|manjaro|endeavouros) echo "arch" ;;
            *) echo "unknown" ;;
        esac
    else
        echo "unknown"
    fi
}

install_debian() {
    info "检测到 Debian/Ubuntu 系发行版"
    sudo apt update
    sudo apt install -y \
        build-essential cmake git \
        qt6-base-dev libqt6opengl6-dev qt6-base-dev-tools \
        libglm-dev libgtest-dev \
        libgl-dev
    info "Debian/Ubuntu 依赖安装完成"
}

install_fedora() {
    info "检测到 Fedora/RHEL 系发行版"
    sudo dnf install -y \
        gcc-c++ cmake git \
        qt6-qtbase-devel \
        glm-devel gtest-devel \
        mesa-libGL-devel
    info "Fedora 依赖安装完成"
}

install_arch() {
    info "检测到 Arch Linux 系发行版"
    sudo pacman -Syu --noconfirm \
        base-devel cmake git \
        qt6-base \
        glm gtest \
        mesa
    info "Arch Linux 依赖安装完成"
}

install_macos() {
    info "检测到 macOS"
    if ! command -v brew &>/dev/null; then
        error "未找到 Homebrew，请先安装: https://brew.sh"
    fi
    brew install cmake qt@6 glm googletest
    info "macOS 依赖安装完成"
    warn "构建时可能需要设置 Qt6 路径:"
    echo "  export CMAKE_PREFIX_PATH=\$(brew --prefix qt@6)"
}

build_project() {
    info "开始构建项目..."
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

    mkdir -p "$PROJECT_DIR/build"
    cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$PROJECT_DIR/build" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
    info "构建完成"
    info "可执行文件: $PROJECT_DIR/build/src/mcskin_raytracer"
    info "测试程序:   $PROJECT_DIR/build/tests/mcskin_tests"
}

# ── Main ─────────────────────────────────────────────────────────────────────

OS=$(detect_os)

case "$OS" in
    debian) install_debian ;;
    fedora) install_fedora ;;
    arch)   install_arch ;;
    macos)  install_macos ;;
    *)      error "不支持的操作系统。请手动安装: CMake ≥3.22, Qt6, GLM, Google Test, OpenGL" ;;
esac

echo ""
read -rp "是否立即构建项目? [Y/n] " answer
case "${answer:-Y}" in
    [Yy]*) build_project ;;
    *)     info "跳过构建。手动构建: mkdir build && cd build && cmake .. && make -j\$(nproc)" ;;
esac

echo ""
info "安装完成! RapidCheck 会在首次构建时通过 CMake FetchContent 自动下载。"
