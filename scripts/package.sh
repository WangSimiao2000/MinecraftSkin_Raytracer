#!/usr/bin/env bash
#
# MCSkin RaytraceRenderer — Linux release packaging script
# Builds Release, bundles Qt libraries, packages into a distributable tarball.
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
err()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
RELEASE_DIR="$PROJECT_DIR/release"
APP_NAME="MCSkin_RaytraceRenderer"
EXE_NAME="mcskin_raytracer"

VERSION="1.0.0"
SKIP_BUILD=false
QT_PATH=""

# -- Parse arguments --

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "  -v, --version VERSION   Set version string (default: 1.0.0)"
    echo "  -q, --qt-path PATH      Qt6 installation prefix"
    echo "  -s, --skip-build        Skip build, package existing binary"
    echo "  -h, --help              Show this help"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--version)   VERSION="$2"; shift 2 ;;
        -q|--qt-path)   QT_PATH="$2"; shift 2 ;;
        -s|--skip-build) SKIP_BUILD=true; shift ;;
        -h|--help)      usage ;;
        *) err "Unknown option: $1" ;;
    esac
done

# -- Build Release --

if [[ "$SKIP_BUILD" == false ]]; then
    info "Building Release..."
    mkdir -p "$BUILD_DIR"

    cmake_args=(-S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF)
    [[ -n "$QT_PATH" ]] && cmake_args+=("-DCMAKE_PREFIX_PATH=$QT_PATH")

    cmake "${cmake_args[@]}"
    cmake --build "$BUILD_DIR" --target "$EXE_NAME" -j"$(nproc 2>/dev/null || echo 4)"
    info "Build complete."
fi

# -- Verify exe --

EXE_PATH="$BUILD_DIR/src/$EXE_NAME"
[[ -f "$EXE_PATH" ]] || err "Executable not found: $EXE_PATH"

# -- Prepare release directory --

echo ""
info "Preparing release directory..."
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/lib"

cp "$EXE_PATH" "$RELEASE_DIR/"

# -- Collect shared library dependencies --

info "Collecting shared libraries..."

collect_libs() {
    local binary="$1"
    # ldd output: libFoo.so.6 => /usr/lib/libFoo.so.6 (0x...)
    ldd "$binary" 2>/dev/null | while read -r line; do
        # Only bundle Qt and ICU libs (system libs like libc, libm, libGL stay as system deps)
        if echo "$line" | grep -qE '(libQt6|libicu)'; then
            local lib_path
            lib_path=$(echo "$line" | awk '{print $3}')
            if [[ -n "$lib_path" && -f "$lib_path" ]]; then
                echo "$lib_path"
            fi
        fi
    done
}

# Collect from main binary
mapfile -t LIBS < <(collect_libs "$EXE_PATH" | sort -u)

for lib in "${LIBS[@]}"; do
    cp -L "$lib" "$RELEASE_DIR/lib/"
    info "  Bundled: $(basename "$lib")"
done

# Also collect transitive deps from bundled libs
for lib in "$RELEASE_DIR/lib/"*.so*; do
    [[ -f "$lib" ]] || continue
    mapfile -t EXTRA < <(collect_libs "$lib" | sort -u)
    for elib in "${EXTRA[@]}"; do
        local_name=$(basename "$elib")
        if [[ ! -f "$RELEASE_DIR/lib/$local_name" ]]; then
            cp -L "$elib" "$RELEASE_DIR/lib/"
            info "  Bundled (transitive): $local_name"
        fi
    done
done

# -- Copy Qt plugins --

find_qt_plugins() {
    # Try to find Qt plugins directory
    local qt_lib_dir=""

    if [[ -n "$QT_PATH" && -d "$QT_PATH/plugins" ]]; then
        echo "$QT_PATH/plugins"
        return
    fi

    # Infer from one of the bundled Qt libs
    for lib in "$RELEASE_DIR/lib/"libQt6Core.so*; do
        [[ -f "$lib" ]] || continue
        local real_path
        real_path=$(readlink -f "$(ldd "$EXE_PATH" | grep libQt6Core | awk '{print $3}')" 2>/dev/null || true)
        if [[ -n "$real_path" ]]; then
            qt_lib_dir=$(dirname "$real_path")
            local qt_prefix
            qt_prefix=$(dirname "$qt_lib_dir")
            if [[ -d "$qt_prefix/plugins" ]]; then
                echo "$qt_prefix/plugins"
                return
            fi
        fi
    done

    # System paths
    for p in /usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib64/qt6/plugins /usr/lib/qt6/plugins; do
        [[ -d "$p" ]] && echo "$p" && return
    done
}

QT_PLUGINS_DIR=$(find_qt_plugins)
if [[ -n "$QT_PLUGINS_DIR" && -d "$QT_PLUGINS_DIR" ]]; then
    info "Qt plugins: $QT_PLUGINS_DIR"
    # Essential plugins for a GUI app
    for plugin_dir in platforms xcbglintegrations wayland-shell-integration; do
        if [[ -d "$QT_PLUGINS_DIR/$plugin_dir" ]]; then
            mkdir -p "$RELEASE_DIR/plugins/$plugin_dir"
            cp -L "$QT_PLUGINS_DIR/$plugin_dir/"*.so "$RELEASE_DIR/plugins/$plugin_dir/" 2>/dev/null || true
            info "  Plugins: $plugin_dir"
        fi
    done
else
    warn "Qt plugins directory not found. App may need system Qt at runtime."
fi

# -- Create launcher script --

cat > "$RELEASE_DIR/run.sh" << 'LAUNCHER'
#!/usr/bin/env bash
# Launcher script - sets up library paths and runs the application
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
exec "$SCRIPT_DIR/mcskin_raytracer" "$@"
LAUNCHER
chmod +x "$RELEASE_DIR/run.sh"

# -- Package tarball --

echo ""
ARCH=$(uname -m)
TAR_NAME="${APP_NAME}_v${VERSION}_linux_${ARCH}.tar.gz"
TAR_PATH="$PROJECT_DIR/$TAR_NAME"

info "Packaging: $TAR_NAME"
tar -czf "$TAR_PATH" -C "$PROJECT_DIR" release --transform="s/^release/${APP_NAME}_v${VERSION}/"

SIZE=$(du -h "$TAR_PATH" | cut -f1)
echo ""
info "Done. Package: $TAR_PATH ($SIZE)"
echo ""
echo -e "${BOLD}Next steps:${NC}"
echo -e "  ${CYAN}1) git tag v$VERSION${NC}"
echo -e "  ${CYAN}2) git push origin v$VERSION${NC}"
echo -e "  ${CYAN}3) Upload $TAR_NAME to GitHub Releases${NC}"
echo ""
echo -e "${BOLD}Users run:${NC}"
echo -e "  ${CYAN}tar xzf $TAR_NAME${NC}"
echo -e "  ${CYAN}./${APP_NAME}_v${VERSION}/run.sh${NC}"
