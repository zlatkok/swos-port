#include "pitch.h"
#include "pitchDatabase.h"
#include "loadTexture.h"
#include "camera.h"
#include "game.h"
#include "replays.h"
#include "render.h"
#include "file.h"
#include "util.h"

constexpr int kSwosPatternSize = 16;

constexpr int kPitchWidth = kPitchPatternWidth * kSwosPatternSize;
constexpr int kPitchHeight = kPitchPatternHeight * kSwosPatternSize;

constexpr float kZoomIncrement = 1.f / 70 / 4;
constexpr float kMaxZoom = 2.5;
constexpr float kMaxZoomVelocityFactor = .1f;

static int m_res;

static float m_zoom;

static void drawPitch(float xOfs, float yOfs, int row, int column, int numPatternsX, int numPatternsY);
static void reloadPitch(AssetResolution oldResolution, AssetResolution newResolution);
static std::pair<float, float> clipPitch(float widthNormalized, float heightNormalized, float& x, float& y);
static float getMinimumZoom(float scale = getScale());
static float getDefaultZoom(float scale);
static float getZoomIncrement();

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
    logInfo("Loading pitch %d", pitchNo);

    auto pitchIndex = kPitchIndices[pitchNo];
    auto start = kPitchPatternStartIndices[pitchNo];

    for (int i = 0; i < kPitchPatternHeight; i++) {
        for (int j = 0; j < kPitchPatternWidth; j++) {
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

// Renders pitch at cameraX and cameraY.
std::pair<float, float> drawPitch(float cameraX, float cameraY)
{
    assert(m_zoom > 0);

    int width, height;
    std::tie(width, height) = getWindowSize();

    auto scale = getScale();

    auto widthNormalized = width / scale / m_zoom;
    auto heightNormalized = height / scale / m_zoom;

    auto x = cameraX + (kVgaWidth - widthNormalized) / 2;
    auto y = cameraY + (kVgaHeight - heightNormalized) / 2;

    auto offsets = clipPitch(widthNormalized, heightNormalized, x, y);

    auto numPatternsX = width / scale / m_zoom / kSwosPatternSize + 1;
    auto numPatternsY = height / scale / m_zoom / kSwosPatternSize + 1;
    numPatternsX = std::min(static_cast<float>(kPitchPatternWidth), numPatternsX);
    numPatternsY = std::min(static_cast<float>(kPitchPatternHeight), numPatternsY);
    numPatternsX = std::ceil(numPatternsX);
    numPatternsY = std::ceil(numPatternsY);

    assert(numPatternsX > 0 && numPatternsY > 0);

    int xInt = static_cast<int>(x);
    auto xDiv = std::div(xInt, kSwosPatternSize);
    int column = xDiv.quot;
    auto xOfs = (x - xInt + xDiv.rem) * scale * m_zoom;

    int yInt = static_cast<int>(y);
    auto yDiv = std::div(yInt, kSwosPatternSize);
    // make sure to skip the top invisible row
    int row = yDiv.quot - 1;
    row = std::max(0, row);
    auto yOfs = (y - yInt + yDiv.rem) * scale * m_zoom;

    assert(row >= 0 && column >= 0);

    drawPitch(xOfs, yOfs, row, column, static_cast<int>(numPatternsX), static_cast<int>(numPatternsY));

    // unfortunately, this has to stay until UpdateCameraBreakMode() is converted
    swos.cameraCoordinatesValid = 1;

    return offsets;
}

std::pair<float, float> drawPitchAtCurrentCamera()
{
    return drawPitch(getCameraX(), getCameraY());
}

void SWOS::ScrollToCurrent()
{
    drawPitchAtCurrentCamera();
}

bool zoomIn(float step /* = 0 */)
{
    return setZoomFactor(m_zoom + getZoomIncrement(), step);
}

bool zoomOut(float step /* = 0 */)
{
    return setZoomFactor(m_zoom - getZoomIncrement(), step);
}

float getZoomFactor()
{
    return m_zoom;
}

bool setZoomFactor(float zoom, float step /* = 0 */)
{
    auto initialZoom = m_zoom;

    auto scale = getScale();
    if (scale) {
        if (!zoom) {
            m_zoom = getDefaultZoom(scale);
        } else {
            if (step && m_zoom != zoom) {
                auto mul = zoom > m_zoom ? std::ceil(zoom / step) : std::floor(zoom / step);
                zoom = step * mul;
            }
            m_zoom = std::min(kMaxZoom, zoom);
            auto minZoom = getMinimumZoom(scale);
            m_zoom = std::max(minZoom, m_zoom);
        }
    } else {
        m_zoom = zoom;
    }

    return initialZoom != m_zoom;
}

static void drawPitch(float xOfs, float yOfs, int row, int column, int numPatternsX, int numPatternsY)
{
    int patternSize = kPatternSizes[m_res];
    int pitchNo = getPitchNo();
    auto start = kPitchPatternStartIndices[pitchNo];
    auto indices = kPitchIndices[pitchNo];

    auto scale = getScale();

    auto destPatternSize = kSwosPatternSize * scale * m_zoom;

    auto destXOfs = -xOfs;
    auto destYOfs = -yOfs;
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

    std::array<RenderPattern, kPitchPatternWidth * kPitchPatternHeight> renderPatterns;
    int numPatterns = 0;

    for (int i = row; i < row + numPatternsY; i++) {
        for (int j = column; j < column + numPatternsX; j++) {
            auto patternIndex = indices[i][j];
            if (patternIndex != UINT16_MAX) {
                const auto& pattern = kPatterns[m_res][start + patternIndex];
                const auto texture = m_pitchTextures[m_res][pattern.texture];

                SDL_Rect srcRect{ pattern.x, pattern.y, patternSize, patternSize };
                SDL_FRect dstRect{ currentXOfs, currentYOfs, destPatternSize, destPatternSize };

                renderPatterns[numPatterns++] = { srcRect, dstRect, texture, pattern.texture };
            }

            currentXOfs += destPatternSize;
        }

        currentXOfs = destXOfs;
        currentYOfs += destPatternSize;
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

void SWOS::DrawAnimatedPatterns()
{
    // must decrease this so the camera would start moving at the start of the game
    if (!replayingNow() && !swos.g_trainingGame && swos.showFansCounter)
        swos.showFansCounter--;
    //...
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

static std::pair<float, float> clipPitch(float widthNormalized, float heightNormalized, float& x, float& y)
{
    auto xOffset = 0.f;
    auto yOffset = 0.f;

    if (x < 0) {
        xOffset = -x;
        x = 0;
    } else if (x + widthNormalized > kPitchWidth) {
        xOffset = kPitchWidth - widthNormalized - x;
        x = kPitchWidth - widthNormalized;
    }

    if (y < kSwosPatternSize) {
        yOffset = kSwosPatternSize - y;
        y = kSwosPatternSize;
    } else if (y + heightNormalized > kPitchHeight) {
        yOffset = kPitchHeight - heightNormalized - y;
        y = kPitchHeight - heightNormalized;
    }

    return { xOffset, yOffset };
}

static float getMinimumZoom(float scale /* = getScale() */)
{
    assert(scale);

    int width, height;
    std::tie(width, height) = getWindowSize();

    auto normalizedWidth = width / scale;
    auto normalizedHeight = height / scale;

    assert(normalizedWidth <= kPitchWidth && normalizedHeight <= kPitchHeight);

    auto minZoom = std::max(normalizedWidth / kPitchWidth, normalizedHeight / kPitchHeight);
    return minZoom;
}

static float getDefaultZoom(float scale)
{
    constexpr float kTargetVgaWidth = 435;

    int width, height;
    std::tie(width, height) = getWindowSize();
    return width / (kTargetVgaWidth * scale);
}

static float getZoomIncrement()
{
    auto zoomRange = kMaxZoom - getMinimumZoom();
    return (m_zoom - getMinimumZoom()) / zoomRange * kMaxZoomVelocityFactor + kZoomIncrement;
}
