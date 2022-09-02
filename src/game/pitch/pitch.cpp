#include "pitch.h"
#include "pitchDatabase.h"
#include "windowManager.h"
#include "loadTexture.h"
#include "camera.h"
#include "game.h"
#include "replays.h"
#include "render.h"
#include "gameFieldMapping.h"
#include "file.h"
#include "util.h"
#include "random.h"

constexpr int kSwosPatternSize = 16;

static_assert(kPitchWidth == kPitchPatternWidth * kSwosPatternSize &&
    kPitchHeight == kPitchPatternHeight * kSwosPatternSize, "Pitch dimensions mismatch");

constexpr float kZoomIncrement = 1.f / 70 / 4;
constexpr float kMaxZoom = 2.5;
constexpr float kMaxZoomVelocityFactor = .1f;

enum PitchTypes {
    kRandom = -1,
    kFrozen = 0,
    kMuddy = 1,
    kWet = 2,
    kSoft = 3,
    kNormal = 4,
    kDry = 5,
    kHard = 6,
    kMaxPitchType = 6,
};

static int m_pitchType;
static int m_pitchNumber;

static int m_res;
static float m_zoom;
static Uint32 m_lastZoomChangeTicks;

static void setPitchType();
static void setPitchNumber();
static void drawPitch(float xOfs, float yOfs, int row, int column, int numPatternsX, int numPatternsY);
static void reloadPitch(AssetResolution oldResolution, AssetResolution newResolution);
static std::pair<float, float> clipPitch(float widthNormalized, float heightNormalized, float& x, float& y);
static float getMinimumZoom(float scale = getGameScale());
static float getDefaultZoom(float scale);
static float getZoomIncrement();
static void clipZoom();

void initPitches()
{
    m_res = static_cast<int>(getAssetResolution());
    registerAssetResolutionChangeHandler(reloadPitch);
}

void setPitchTypeAndNumber()
{
    setPitchType();
    setPitchNumber();
}

void loadPitch()
{
    logInfo("Loading pitch %d (type: %d), asset resolution %d", m_pitchNumber, m_pitchType, m_res);

    auto pitchIndex = kPitchIndices[m_pitchNumber];
    auto start = kPitchPatternStartIndices[m_pitchNumber];

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
    // clip zoom first since the screen dimensions might have changed
    clipZoom();

    assert(m_zoom > 0);

    auto width = getFieldWidth();
    auto height = getFieldHeight();

    auto scale = getGameScale();

    auto widthNormalized = width / m_zoom;
    auto heightNormalized = height / m_zoom;

    auto x = cameraX + (kVgaWidth - widthNormalized) / 2;
    auto y = cameraY + (kVgaHeight - heightNormalized) / 2;

    auto offsets = clipPitch(widthNormalized, heightNormalized, x, y);

    auto numPatternsX = widthNormalized / kSwosPatternSize + 1;
    auto numPatternsY = heightNormalized / kSwosPatternSize + 1;
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

bool resetZoom()
{
    return setZoomFactor(getDefaultZoom(getGameScale()));
}

float getZoomFactor()
{
    return m_zoom;
}

void initZoomFactor(float zoom)
{
    m_zoom = zoom;
}

bool setZoomFactor(float zoom, float step /* = 0 */)
{
    auto initialZoom = m_zoom;

    if (auto scale = getGameScale()) {
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
        assert(false);
    }

    bool zoomChanged = initialZoom != m_zoom;
    if (zoomChanged)
        m_lastZoomChangeTicks = SDL_GetTicks();

    return zoomChanged;
}

Uint32 getLastZoomChangeTime()
{
    return m_lastZoomChangeTicks;
}

static void setPitchType()
{
    // 12 (months) x 7 (pitch types)
    // probabilities in percents (sum = 100)
    static const std::array<std::array<byte, 7>, 12> kPitchTypeSeasonalProbabilities = {{
        30, 20, 30, 20, 0, 0, 0,
        20, 30, 20, 20, 10, 0, 0,
        10, 30, 10, 30, 20, 0, 0,
        0, 10, 10, 30, 40, 10, 0,
        0, 0, 0, 10, 40, 40, 10,
        0, 0, 0, 0, 40, 40, 20,
        0, 0, 0, 0, 30, 30, 40,
        0, 0, 0, 0, 50, 30, 20,
        0, 0, 0, 20, 40, 30, 10,
        0, 20, 0, 40, 30, 10, 0,
        10, 30, 10, 40, 10, 0, 0,
        20, 30, 20, 30, 0, 0, 0,
    }};
    static const std::array<byte, 7> kPitchTypeProbabilities = { 5, 5, 10, 20, 30, 20, 10 };

    m_pitchType = 0;

    if (swos.gamePitchTypeOrSeason || swos.gamePitchType == -1) {
        auto probabilities = swos.gamePitchTypeOrSeason ? kPitchTypeProbabilities.data() : kPitchTypeSeasonalProbabilities[swos.gameSeason].data();
        auto probability = SWOS::rand() * 100 / 256;    // range 0..99
        while (probability >= *probabilities) {
            probability -= *probabilities++;
            m_pitchType++;
        }
    } else {
        m_pitchType = swos.gamePitchType;
    }

    assert(m_pitchType >= kRandom && m_pitchType <= kMaxPitchType);
}

static void setPitchNumber()
{
    static const std::array<byte, 16> kPitchNumberProbabilities = {
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 4, 4, 2, 2, 3, 3
    };

    if (swos.g_trainingGame) {
        m_pitchNumber = kTrainingPitchIndex;
    } else {
        int index;
        if (swos.plg_D0_param) {
            // pick random pitch (used in friendlies)
            index = SWOS::rand() & 0xf;
        } else {
            // pseudo-random, but always the same in regards to the teams that are playing; used in career
            index = swos.topTeamIngame.teamName[0] | (swos.topTeamIngame.teamName[1] << 8);
            index ^= swos.topTeamIngame.prShirtCol;
            index ^= swos.topTeamIngame.prShortsCol;
            index &= 0xf;
        }

        m_pitchNumber = kPitchNumberProbabilities[index];
    }

    assert(m_pitchNumber >= 0 && m_pitchNumber < kNumPitches);
}

static void drawPitch(float xOfs, float yOfs, int row, int column, int numPatternsX, int numPatternsY)
{
    int patternSize = kPatternSizes[m_res];
    auto start = kPitchPatternStartIndices[m_pitchNumber];
    auto indices = kPitchIndices[m_pitchNumber];

    auto scale = getGameScale();

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

static float getMinimumZoom(float scale /* = getGameScale() */)
{
    assert(scale);

    auto fieldWidth = getFieldWidth();
    auto fieldHeight = getFieldHeight();

    assert(fieldWidth <= kPitchWidth && fieldHeight <= kPitchHeight);

    return std::max(fieldWidth / kPitchWidth, fieldHeight / kPitchHeight);
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

static void clipZoom()
{
    m_zoom = std::max(m_zoom, getMinimumZoom());
    m_zoom = std::min(m_zoom, kMaxZoom);
}
