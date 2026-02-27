#include "raytracer/tile_renderer.h"
#include "raytracer/intersection.h"
#include "scene/scene.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <random>
#include <cmath>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::vector<TileRenderer::TileError> TileRenderer::errors_;

std::vector<Tile> TileRenderer::generateTiles(int imageWidth, int imageHeight, int tileSize) {
    if (imageWidth <= 0 || imageHeight <= 0 || tileSize <= 0) {
        return {};
    }

    std::vector<Tile> tiles;
    int cols = (imageWidth + tileSize - 1) / tileSize;
    int rows = (imageHeight + tileSize - 1) / tileSize;
    tiles.reserve(cols * rows);

    for (int ty = 0; ty < rows; ++ty) {
        for (int tx = 0; tx < cols; ++tx) {
            Tile tile;
            tile.x = tx * tileSize;
            tile.y = ty * tileSize;
            tile.width = std::min(tileSize, imageWidth - tile.x);
            tile.height = std::min(tileSize, imageHeight - tile.y);
            tiles.push_back(tile);
        }
    }
    return tiles;
}

// Generate a DOF ray using thin-lens model
static Ray generateDOFRay(const Scene& scene, float u, float v, float aspectRatio,
                          float aperture, float focusDist,
                          std::mt19937& rng) {
    // First generate the pinhole ray to find the focus point
    Ray pinholeRay = scene.camera.generateRay(u, v, aspectRatio);

    if (aperture < 1e-6f) return pinholeRay;

    Vec3 forward = (scene.camera.target - scene.camera.position).normalize();
    Vec3 right = forward.cross(scene.camera.up).normalize();
    Vec3 camUp = right.cross(forward);

    // Focus point along the pinhole ray
    Vec3 focusPoint = pinholeRay.origin + pinholeRay.direction * focusDist;

    // Random point on lens disk
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float angle = 2.0f * static_cast<float>(M_PI) * dist(rng);
    float radius = aperture * std::sqrt(dist(rng));
    float lensX = radius * std::cos(angle);
    float lensY = radius * std::sin(angle);

    Vec3 lensOffset = right * lensX + camUp * lensY;
    Vec3 newOrigin = scene.camera.position + lensOffset;
    Vec3 newDir = (focusPoint - newOrigin).normalize();

    return Ray(newOrigin, newDir);
}

void TileRenderer::renderTile(const Tile& tile,
                              const Scene& scene,
                              const RayTracer::Config& config,
                              Image& output) {
    float aspectRatio = static_cast<float>(config.width) / static_cast<float>(config.height);
    int spp = std::max(1, config.samplesPerPixel);

    std::mt19937 rng(static_cast<unsigned>(tile.y * config.width + tile.x));
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Compute focus distance
    float focusDist = config.focusDistance;
    if (focusDist <= 0.0f) {
        focusDist = (scene.camera.target - scene.camera.position).length();
    }

    for (int py = tile.y; py < tile.y + tile.height; ++py) {
        for (int px = tile.x; px < tile.x + tile.width; ++px) {
            Color accum(0.0f, 0.0f, 0.0f, 0.0f);

            for (int s = 0; s < spp; ++s) {
                float jx = (spp == 1) ? 0.5f : dist(rng);
                float jy = (spp == 1) ? 0.5f : dist(rng);

                float u = (static_cast<float>(px) + jx) / static_cast<float>(config.width);
                float v = (static_cast<float>(py) + jy) / static_cast<float>(config.height);

                Ray ray;
                if (config.dofEnabled && config.aperture > 1e-6f) {
                    ray = generateDOFRay(scene, u, v, aspectRatio,
                                         config.aperture, focusDist, rng);
                } else {
                    ray = scene.camera.generateRay(u, v, aspectRatio);
                }

                Color c = RayTracer::traceRay(ray, scene, 0, config.maxBounces,
                                              ShadingParams{}, &config);

                // If the ray missed, use the gradient background with proper u,v
                // (traceRay returns approximate bg for misses; override here)
                HitResult testHit = intersectScene(ray, scene);
                if (!testHit.hit) {
                    c = RayTracer::backgroundColor(scene, u, v, &config);
                }

                accum.r += c.r;
                accum.g += c.g;
                accum.b += c.b;
                accum.a += c.a;
            }

            float inv = 1.0f / static_cast<float>(spp);
            output.pixels[py * output.width + px] = Color(
                accum.r * inv, accum.g * inv, accum.b * inv, accum.a * inv);
        }
    }
}

Image TileRenderer::render(const Scene& scene,
                           const RayTracer::Config& config,
                           std::function<void(int, int)> progressCallback) {
    int threadCount = config.threadCount;
    if (threadCount <= 0) {
        threadCount = static_cast<int>(std::thread::hardware_concurrency());
        if (threadCount <= 0) threadCount = 1;
    }

    std::vector<Tile> tiles = generateTiles(config.width, config.height, config.tileSize);
    int totalTiles = static_cast<int>(tiles.size());

    Image output(config.width, config.height);
    errors_.clear();

    if (totalTiles == 0) {
        return output;
    }

    std::atomic<int> nextTile{0};
    std::atomic<int> completedTiles{0};
    std::mutex progressMutex;
    std::mutex errorMutex;

    auto worker = [&]() {
        while (true) {
            int idx = nextTile.fetch_add(1);
            if (idx >= totalTiles) break;

            try {
                renderTile(tiles[idx], scene, config, output);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(errorMutex);
                errors_.push_back({idx, e.what()});
            } catch (...) {
                std::lock_guard<std::mutex> lock(errorMutex);
                errors_.push_back({idx, "Unknown error"});
            }

            int done = completedTiles.fetch_add(1) + 1;
            if (progressCallback) {
                std::lock_guard<std::mutex> lock(progressMutex);
                progressCallback(done, totalTiles);
            }
        }
    };

    std::vector<std::thread> threads;
    int numThreads = std::min(threadCount, totalTiles);
    threads.reserve(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    return output;
}

const std::vector<TileRenderer::TileError>& TileRenderer::lastErrors() {
    return errors_;
}
