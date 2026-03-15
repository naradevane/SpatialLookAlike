#pragma once

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRA_LEAN
#  define VC_EXTRA_LEAN
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <tchar.h>
#include <windows.h>
#include <winhttp.h>
#include <objidl.h>
#include <gdiplus.h>

#include <rxregsvc.h>
#include <dbmain.h>
#include <aced.h>
#include <acdocman.h>
#include <acgi.h>
#include <acgs.h>

static constexpr wchar_t kTileHost[]   = L"mt1.google.com";
static constexpr wchar_t kTilePath[] = L"/vt/lyrs=s&x=%d&y=%d&z=%d&scale=2";

static constexpr int kTileSizePx       = 256;
static constexpr int kMaxCacheEntries  = 1024;
static constexpr int kDefaultZoom      = 15;
static constexpr double kDebounceSec   = 0.05;

struct TileKey {
    int z;
    int x;
    int y;

    // Penangkal Error C++ Terbaru
    bool operator==(const TileKey& other) const {
        return z == other.z && x == other.x && y == other.y;
    }

    // Syarat wajib buat masuk std::set
    bool operator<(const TileKey& other) const {
        if (z != other.z) return z < other.z;
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

struct TileKeyHash {
    std::size_t operator()(const TileKey& k) const noexcept {
        std::size_t h = static_cast<std::size_t>(k.z) * 1000003ULL;
        h ^= static_cast<std::size_t>(k.x) * 999983ULL + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(k.y) * 998981ULL + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

struct TileData {
    TileKey              key;
    int                  width  = 0;
    int                  height = 0;
    std::vector<uint8_t> bgra;
    bool                 valid  = false;

    std::size_t byteSize() const noexcept {
        return static_cast<std::size_t>(width) * height * 4;
    }
};

using TileDataPtr = std::shared_ptr<TileData>;

namespace TileMath {
    constexpr double kPi = 3.14159265358979323846;

    inline int lon2tileX(double lon, int z) noexcept {
        return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << z)));
    }

    inline int lat2tileY(double lat, int z) noexcept {
        double latR = lat * kPi / 180.0;
        double val  = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / kPi) / 2.0;
        return static_cast<int>(std::floor(val * (1 << z)));
    }

    inline double tileX2lon(int x, int z) noexcept {
        return x / static_cast<double>(1 << z) * 360.0 - 180.0;
    }

    inline double tileY2lat(int y, int z) noexcept {
        double n = kPi - 2.0 * kPi * y / static_cast<double>(1 << z);
        return 180.0 / kPi * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
    }

    inline int estimateZoom(double viewWidthDeg, int vpWidthPx = 1920) noexcept {
        double tilesWanted = static_cast<double>(vpWidthPx) / kTileSizePx;
        double z = std::log2(tilesWanted * 360.0 / viewWidthDeg);
        int zi   = static_cast<int>(std::round(z));
        return std::max(0, std::min(zi, 19));
    }
}

namespace WebMercatorMath {
    constexpr double kEarthRadius = 6378137.0;
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kOriginShift = kPi * kEarthRadius;

    inline void MercatorToLL(double x, double y, double& latDeg, double& lonDeg) noexcept {
        lonDeg = (x / kOriginShift) * 180.0;
        double yDeg = (y / kOriginShift) * 180.0;
        latDeg = 180.0 / kPi * (2.0 * std::atan(std::exp(yDeg * kPi / 180.0)) - kPi / 2.0);
    }

    inline void LLtoMercator(double latDeg, double lonDeg, double& x, double& y) noexcept {
        x = lonDeg * kOriginShift / 180.0;
        double latRad = latDeg * kPi / 180.0;
        y = std::log(std::tan((kPi / 4.0) + (latRad / 2.0))) * kEarthRadius;
    }
}