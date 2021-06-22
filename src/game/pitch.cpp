#include "pitch.h"
#include "pitchDatabase.h"
#include "loadTexture.h"
#include "camera.h"
#include "game.h"
#include "render.h"
#include "file.h"
#include "util.h"

static constexpr int kSwosPatternSize = 16;

static int m_res;

static void reloadPitch(AssetResolution oldResolution, AssetResolution newResolution);

void initPitches()
{
    m_res = static_cast<int>(getAssetResolution());
    registerAssetResolutionChangeHandler(reloadPitch);
}

static int getPitchNo()
{
    int pitchNo = swos.pitchNumberAndType >> 8;
    assert(static_cast<size_t>(pitchNo) < kNumPitches);

    return pitchNo;
}

void loadPitch()
{
    int pitchNo = getPitchNo();

    auto pitchIndex = kPitchIndices[pitchNo];
    auto start = kPitchPatternStartIndices[pitchNo];

    for (int i = 0; i < kPitchHeight; i++) {
        for (int j = 0; j < kPitchWidth; j++) {
            auto patternIndex = pitchIndex[i][j];
            const auto& pattern = kPatterns[m_res][start + patternIndex];
            auto textureNo = pattern.texture;
            auto& texture = m_pitchTextures[m_res][textureNo];
            if (!texture) {
                auto filename = kTextureFilenames[textureNo];
                texture = loadTexture(filename);
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
            }
        }
    }
}

// Using floating point SDL calls resulted with visible stitches in patterns. Switched to integer, rounding
// pattern size to higher number to avoid running out of pixels to draw if the camera is at the rightmost point.
static void drawPitch(float xOfs, float yOfs, int row, int column, int numPatternsX, int numPatternsY)
{
    int screenWidth, screenHeight;
    std::tie(screenWidth, screenHeight) = getWindowSize();

    int patternSize = kPatternSizes[m_res];
    int pitchNo = getPitchNo();
    auto start = kPitchPatternStartIndices[pitchNo];
    auto indices = kPitchIndices[pitchNo];

    auto xScale = getXScale();
    auto yScale = getYScale();

    auto destWidth = kSwosPatternSize * xScale;
    auto destHeight = kSwosPatternSize * yScale;

    auto destXOfs = -xOfs * xScale;
    auto destYOfs = -yOfs * yScale;
    auto currentXOfs = destXOfs;
    auto currentYOfs = destYOfs;

    struct RenderPattern {
        RenderPattern() {}
        RenderPattern(const SDL_Rect& src, const SDL_FRect& dst, SDL_Texture *texture, int8_t textureIndex)
            : src(src), dst(dst), texture(texture), textureIndex(textureIndex) {}

        SDL_Rect src;
        SDL_FRect dst;
        SDL_Texture *texture;
        int8_t textureIndex;

        bool operator<(const RenderPattern& other) const {
            return textureIndex < other.textureIndex;
        }
    };

    std::array<RenderPattern, kPitchWidth * kPitchHeight> renderPatterns;
    int numPatterns = 0;

    for (int i = row; i < row + numPatternsY; i++) {
        for (int j = column; j < column + numPatternsX; j++) {
            auto patternIndex = indices[i][j];
            if (patternIndex != UINT16_MAX) {
                const auto& pattern = kPatterns[m_res][start + patternIndex];
                const auto texture = m_pitchTextures[m_res][pattern.texture];

                SDL_Rect srcRect{ pattern.x, pattern.y, patternSize, patternSize };
                SDL_FRect dstRect{ currentXOfs, currentYOfs, destWidth, destHeight };

                renderPatterns[numPatterns++] = { srcRect, dstRect, texture, pattern.texture };
            }

            currentXOfs += destWidth;
        }

        currentXOfs = destXOfs;
        currentYOfs += destHeight;
    }

    // reduce the number of texture switch calls
    std::sort(renderPatterns.begin(), renderPatterns.begin() + numPatterns);

    auto renderer = getRenderer();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    for (int i = 0; i < numPatterns; i++) {
        const auto& renderPattern = renderPatterns[i];
        SDL_RenderCopyF(renderer, renderPattern.texture, &renderPattern.src, &renderPattern.dst);
    }
}

// Sets view to cameraX and cameraY.
// Updates oldCameraX and oldCameraY.
//
void SWOS::ScrollToCurrent()
{
    auto cameraX = getCameraX();
    auto cameraY = getCameraY();

    // make sure to skip the top invisible row
    int row = swos.g_cameraY / kSwosPatternSize - 1;
    int column = swos.g_cameraX / kSwosPatternSize;
    float xOfs = FixedPoint(cameraX.whole() % kSwosPatternSize, cameraX.fraction());
    float yOfs = FixedPoint(cameraY.whole() % kSwosPatternSize, cameraY.fraction());

    int numPatternsX = kVgaWidth / kSwosPatternSize + 2;
    int numPatternsY = kVgaHeight / kSwosPatternSize + 2;
    numPatternsX = std::min(numPatternsX, kPitchWidth - column);
    numPatternsY = std::min(numPatternsY, kPitchHeight - row);

    drawPitch(xOfs, yOfs, row, column, numPatternsX, numPatternsY);

    setCameraX(cameraX);
    setCameraY(cameraY);
    swos.g_oldCameraX = cameraX.whole();
    swos.g_oldCameraY = cameraY.whole();

    swos.cameraCoordinatesValid = 1;
}

void SWOS::DrawAnimatedPatterns()
{
    if (!swos.replayState && !swos.g_trainingGame && swos.showFansCounter)
        swos.showFansCounter--;
}

static void reloadPitch(AssetResolution oldResolution, AssetResolution newResolution)
{
    if (oldResolution != AssetResolution::kInvalid) {
        for (auto& texture : m_pitchTextures[static_cast<int>(oldResolution)]) {
            if (texture) {
                SDL_DestroyTexture(texture);
                texture = nullptr;
            }
        }
    }

    m_res = static_cast<int>(newResolution);

    if (isMatchRunning() && newResolution != AssetResolution::kInvalid)
        loadPitch();
}
