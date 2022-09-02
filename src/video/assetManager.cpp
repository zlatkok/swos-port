#include "assetManager.h"
#include "file.h"

static_assert(static_cast<int>(AssetResolution::kNumResolutions) == 3, "Update resolutions array");

struct AssetResolutionInfo {
    AssetResolution res;
    int width;
    int height;
};

static constexpr std::array<AssetResolutionInfo, 3> kAssetResolutionsInfo = {{
    { AssetResolution::kLowRes, 960, 540 },
    { AssetResolution::kHD, 1920, 1080 },
    { AssetResolution::k4k, 3840, 2160 },
}};

static AssetResolution m_resolution = AssetResolution::kLowRes;
static std::vector<AssetResolutionChangeHandler> m_resChangeHandlers;

void updateAssetResolution(int width, int height)
{
    int minDiff = INT_MAX;
    auto oldResolution = m_resolution;
    m_resolution = AssetResolution::kLowRes;

    for (const auto& info : kAssetResolutionsInfo) {
        int diff = std::abs(info.width - width) + std::abs(info.height - height);
        if (diff < minDiff) {
            m_resolution = info.res;
            minDiff = diff;
        }
    }

    if (m_resolution != oldResolution)
        for (const auto& handler : m_resChangeHandlers)
            handler(oldResolution, m_resolution);
}

AssetResolution getAssetResolution()
{
    return m_resolution;
}

void registerAssetResolutionChangeHandler(AssetResolutionChangeHandler handler)
{
    m_resChangeHandlers.push_back(handler);
}

const char *getAssetDir()
{
    static_assert(static_cast<int>(AssetResolution::kNumResolutions) == 3, "Somewhere out there in the space");

    switch (m_resolution) {
    case AssetResolution::k4k: return "assets" DIR_SEPARATOR "4k";
    case AssetResolution::kHD: return "assets" DIR_SEPARATOR "hd";
    case AssetResolution::kLowRes: return "assets" DIR_SEPARATOR "low-res";
    }

    return nullptr;
}

std::string getPathInAssetDir(const char *path)
{
    return std::string(getAssetDir()) + getDirSeparator() + path;
}
