#pragma once

#include <vector>
#include <functional>
#include <string>
#include "skin/image.h"
#include "scene/scene.h"
#include "raytracer/raytracer.h"

// 渲染图块
struct Tile {
    int x, y;           // 图块左上角坐标
    int width, height;  // 图块尺寸
};

class TileRenderer {
public:
    // Generate tiles that cover the entire image.
    // Edge tiles are clipped to image bounds.
    static std::vector<Tile> generateTiles(int imageWidth, int imageHeight, int tileSize);

    // Render the scene using multiple threads, one tile at a time per thread.
    // progressCallback is called (completedTiles, totalTiles) after each tile finishes.
    // Returns the rendered image. Errors in individual tiles are recorded but
    // do not abort the remaining work.
    static Image render(const Scene& scene,
                        const RayTracer::Config& config,
                        std::function<void(int, int)> progressCallback = nullptr);

    // Render a single tile into the output image.
    static void renderTile(const Tile& tile,
                           const Scene& scene,
                           const RayTracer::Config& config,
                           Image& output);

    // Errors collected from worker threads (tile index → message).
    struct TileError {
        int tileIndex;
        std::string message;
    };

    // Retrieve errors from the last render() call.
    static const std::vector<TileError>& lastErrors();

private:
    static std::vector<TileError> errors_;
};
