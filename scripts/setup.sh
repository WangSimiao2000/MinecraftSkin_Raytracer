#!/usr/bin/env bash
#
# MCSkin RaytraceRenderer — Linux 构建配置脚本
# 支持: Ubuntu/Debian, Fedora/RHEL, Arch Linux
#
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
err()   { echo -e "${RED}[ERROR]${NC} $*"; }
fatal() { err "$*"; exit 1; }
ask()   { echo -ne "${CYAN}[?]${NC} $* "; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# ── 发行版检测 ───────────────────────────────────────────────────────────────

DISTRO=""

detect_distro() {
    if [[ ! -f /etc/os-release ]]; then
        fatal "无法检测 Linux 发行版 (/etc/os-release 不存在)"
    fi
    . /etc/os-release
    case "$ID" in
        ubuntu|debian|linuxmint|pop) DISTRO="debian" ;;
        fedora|rhel|centos|rocky|alma) DISTRO="fedora" ;;
        arch|manjaro|endeavouros) DISTRO="arch" ;;
        *) DISTRO="unknown" ;;
    esac

    echo ""
    info "发行版: ${BOLD}${PRETTY_NAME:-$ID}${NC} (类型: $DISTRO)"
    echo ""
}

# ── 依赖检测 ─────────────────────────────────────────────────────────────────

has_cmd() { command -v "$1" &>/dev/null; }

check_cmake() {
    if ! has_cmd cmake; then return 1; fi
    local ver major minor
    ver=$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+')
    major=$(echo "$ver" | cut -d. -f1)
    minor=$(echo "$ver" | cut -d. -f2)
    (( major > 3 || (major == 3 && minor >= 22) ))
}

check_compiler() { has_cmd g++ || has_cmd clang++; }
check_git() { has_cmd git; }

check_qt6() {
    if has_cmd qmake6; then return 0; fi
    if has_cmd qmake && qmake --version 2>&1 | grep -q "Qt version 6"; then return 0; fi
    if has_cmd pkg-config && pkg-config --exists Qt6Widgets 2>/dev/null; then return 0; fi
    return 1
}

check_opengl() {
    for p in /usr/lib/libGL.so /usr/lib64/libGL.so /usr/lib/x86_64-linux-gnu/libGL.so; do
        [[ -f "$p" ]] && return 0
    done
    has_cmd pkg-config && pkg-config --exists gl 2>/dev/null && return 0
    return 1
}

# ── 包名映射 ─────────────────────────────────────────────────────────────────

get_pkg() {
    local dep="$1"
    case "$DISTRO" in
        debian)
            case "$dep" in
                cmake)    echo "cmake" ;;
                compiler) echo "build-essential" ;;
                git)      echo "git" ;;
                qt6)      echo "qt6-base-dev libqt6opengl6-dev libqt6network6-dev qt6-base-dev-tools" ;;
                opengl)   echo "libgl-dev" ;;
            esac ;;
        fedora)
            case "$dep" in
                cmake)    echo "cmake" ;;
                compiler) echo "gcc-c++" ;;
                git)      echo "git" ;;
                qt6)      echo "qt6-qtbase-devel" ;;
                opengl)   echo "mesa-libGL-devel" ;;
            esac ;;
        arch)
            case "$dep" in
                cmake)    echo "cmake" ;;
                compiler) echo "base-devel" ;;
                git)      echo "git" ;;
                qt6)      echo "qt6-base" ;;
                opengl)   echo "mesa" ;;
            esac ;;
    esac
}

install_pkg() {
    local pkgs="$1"
    [[ -z "$pkgs" ]] && return 0
    case "$DISTRO" in
        debian) sudo apt-get update -qq && sudo apt-get install -y $pkgs ;;
        fedora) sudo dnf install -y $pkgs ;;
        arch)   sudo pacman -Syu --noconfirm $pkgs ;;
    esac
}

# ── 检查并处理依赖 ───────────────────────────────────────────────────────────

DEPS=(cmake compiler git qt6 opengl)
LABELS=("CMake (>= 3.22)" "C++ 编译器" "Git" "Qt6 (Widgets, OpenGL, Network)" "OpenGL 开发库")

missing_auto=()
missing_manual=()
missing_manual_labels=()

check_and_handle_deps() {
    info "正在检查依赖..."
    echo ""

    for i in "${!DEPS[@]}"; do
        local dep="${DEPS[$i]}"
        local label="${LABELS[$i]}"
        if "check_${dep}"; then
            echo -e "  ${GREEN}✓${NC} ${label}"
        else
            if [[ "$DISTRO" != "unknown" ]]; then
                missing_auto+=("$dep")
                echo -e "  ${RED}✗${NC} ${label}  ${YELLOW}(可自动安装)${NC}"
            else
                missing_manual+=("$dep")
                missing_manual_labels+=("$label")
                echo -e "  ${RED}✗${NC} ${label}  ${RED}(需手动安装)${NC}"
            fi
        fi
    done
    echo ""

    # 手动安装提示
    if [[ ${#missing_manual[@]} -gt 0 ]]; then
        warn "以下依赖无法自动安装，请手动安装后重新运行:"
        for lbl in "${missing_manual_labels[@]}"; do
            echo -e "  ${RED}•${NC} $lbl"
        done
        echo ""
    fi

    # 自动安装
    if [[ ${#missing_auto[@]} -gt 0 ]]; then
        ask "是否自动安装缺失的依赖? [Y/n]"
        read -r answer
        case "${answer:-Y}" in
            [Yy]*)
                local all_pkgs=""
                for dep in "${missing_auto[@]}"; do
                    all_pkgs+=" $(get_pkg "$dep")"
                done
                info "安装: $all_pkgs"
                install_pkg "$all_pkgs"
                info "依赖安装完成"
                echo ""
                ;;
            *)
                warn "跳过自动安装"
                echo ""
                ;;
        esac
    fi

    if [[ ${#missing_auto[@]} -eq 0 && ${#missing_manual[@]} -eq 0 ]]; then
        info "所有依赖已就绪!"
        echo ""
    fi

    # 有手动依赖时询问是否继续
    if [[ ${#missing_manual[@]} -gt 0 ]]; then
        ask "仍然继续? (可能会失败) [y/N]"
        read -r answer
        case "${answer:-N}" in
            [Yy]*) warn "继续，缺失依赖可能导致构建失败" ;;
            *)     fatal "请先安装缺失的依赖" ;;
        esac
    fi
}

# ── 模块选择 ─────────────────────────────────────────────────────────────────

BUILD_APP=true
BUILD_TESTS=true
BUILD_TYPE="Release"

select_modules() {
    echo -e "${BOLD}请选择要编译的模块:${NC}"
    echo "  1) 全部 (应用程序 + 测试)"
    echo "  2) 仅应用程序 (mcskin_raytracer)"
    echo "  3) 仅测试 (mcskin_tests)"
    ask "选项 [1/2/3] (默认 1):"
    read -r choice
    case "${choice:-1}" in
        2) BUILD_APP=true;  BUILD_TESTS=false; info "编译: 仅应用程序" ;;
        3) BUILD_APP=false; BUILD_TESTS=true;  info "编译: 仅测试" ;;
        *) BUILD_APP=true;  BUILD_TESTS=true;  info "编译: 全部" ;;
    esac

    echo ""
    echo -e "${BOLD}构建类型:${NC}"
    echo "  1) Release   2) Debug   3) RelWithDebInfo"
    ask "选项 [1/2/3] (默认 1):"
    read -r bt
    case "${bt:-1}" in
        2) BUILD_TYPE="Debug" ;;
        3) BUILD_TYPE="RelWithDebInfo" ;;
        *) BUILD_TYPE="Release" ;;
    esac
    info "构建类型: $BUILD_TYPE"
    echo ""
}

# ── 构建 ─────────────────────────────────────────────────────────────────────

build_project() {
    info "开始构建..."
    local build_dir="$PROJECT_DIR/build"
    mkdir -p "$build_dir"

    local cmake_args=(-S "$PROJECT_DIR" -B "$build_dir" -DCMAKE_BUILD_TYPE="$BUILD_TYPE")
    [[ "$BUILD_TESTS" == true ]] && cmake_args+=(-DBUILD_TESTS=ON) || cmake_args+=(-DBUILD_TESTS=OFF)

    info "cmake ${cmake_args[*]}"
    cmake "${cmake_args[@]}"

    local jobs
    jobs=$(nproc 2>/dev/null || echo 4)

    local targets=()
    [[ "$BUILD_APP" == true ]]   && targets+=(mcskin_raytracer)
    [[ "$BUILD_TESTS" == true ]] && targets+=(mcskin_tests)

    for t in "${targets[@]}"; do
        info "构建: $t"
        cmake --build "$build_dir" --target "$t" -j"$jobs"
    done

    echo ""
    info "构建完成!"
    [[ "$BUILD_APP" == true ]]   && info "可执行文件: $build_dir/src/mcskin_raytracer"
    [[ "$BUILD_TESTS" == true ]] && info "测试程序:   $build_dir/tests/mcskin_tests"

    if [[ "$BUILD_TESTS" == true ]]; then
        echo ""
        ask "立即运行测试? [y/N]"
        read -r rt
        case "${rt:-N}" in
            [Yy]*) (cd "$build_dir" && ctest --output-on-failure) ;;
        esac
    fi
}

# ── Main ─────────────────────────────────────────────────────────────────────

echo ""
echo -e "${BOLD}══════════════════════════════════════════════════${NC}"
echo -e "${BOLD}  MCSkin RaytraceRenderer — Linux 构建配置脚本${NC}"
echo -e "${BOLD}══════════════════════════════════════════════════${NC}"

detect_distro
check_and_handle_deps
select_modules

ask "开始构建? [Y/n]"
read -r do_build
case "${do_build:-Y}" in
    [Yy]*) build_project ;;
    *)     info "跳过。手动构建: mkdir build && cd build && cmake .. && make -j\$(nproc)" ;;
esac

echo ""
info "GLM, Google Test, RapidCheck 会在首次构建时通过 FetchContent 自动下载。"
