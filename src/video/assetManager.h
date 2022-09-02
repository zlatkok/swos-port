#pragma once

enum class AssetResolution {
    k4k, kHD, kLowRes, kNumResolutions, kInvalid = kNumResolutions,
};

constexpr auto kNumAssetResolutions = static_cast<size_t>(AssetResolution::kNumResolutions);

using AssetResolutionChangeHandler = std::function<void(AssetResolution, AssetResolution)>;

void updateAssetResolution(int width, int height);
AssetResolution getAssetResolution();
void registerAssetResolutionChangeHandler(AssetResolutionChangeHandler handler);
const char *getAssetDir();
std::string getPathInAssetDir(const char *path);
