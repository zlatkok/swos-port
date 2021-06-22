#include "menuBackground.h"
#include "menuItemRenderer.h"
#include "windowManager.h"
#include "render.h"
#include "file.h"
#include "util.h"

static SDL_Texture *m_background;
static auto m_backgroundRes = AssetResolution::kInvalid;

static void initMenuBackground(char *filename);
static void loadMenuBackgroundIfNeeded(AssetResolution = AssetResolution::kInvalid, AssetResolution = AssetResolution::kInvalid);

void initMenuBackground()
{
    registerAssetResolutionChangeHandler(loadMenuBackgroundIfNeeded);
    loadMenuBackgroundIfNeeded();
}

void drawMenuBackground(int offsetLine /* = 0 */, int numLines /* = kVgaHeight */)
{
    assert(offsetLine <= 0 && offsetLine + numLines <= kVgaHeight);

    int width, height;
    std::tie(width, height) = getWindowSize();

    SDL_Rect src{ 0, 0 };
    if (m_background)
        SDL_QueryTexture(m_background, nullptr, nullptr, &src.w, &src.h);

    auto renderer = getRenderer();
    if (src.w < width || src.h < height) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    if (m_background) {
        SDL_Rect dst{ 0, offsetLine * height / kVgaHeight, width, numLines * height / kVgaHeight };
        SDL_RenderCopy(renderer, m_background, &src, &dst);
    }
}

static void loadMenuBackgroundIfNeeded(AssetResolution, AssetResolution)
{
    static_assert(static_cast<int>(AssetResolution::kNumResolutions) == 3, "Missing background image resolution");

    auto res = getAssetResolution();
    if (m_backgroundRes == res && m_background)
        return;

    char pathBuf[64];
    auto p = stpcpy(pathBuf, getAssetDir());
    *p++ = getDirSeparator();

    constexpr char kFilenamePrefix[] = "swtitle.";
    assert(p + sizeof(kFilenamePrefix) < pathBuf + sizeof(pathBuf));
    strcpy(p, kFilenamePrefix);

    initMenuBackground(pathBuf);

    m_backgroundRes = res;
}

static SDL_RWops *openImageFile(char *filename)
{
    auto dot = strrchr(filename, '.');
    assert(dot);

    for (auto ext : { "png", "jpg", "jpeg", "bmp", }) {
        strcpy(dot + 1, ext);
        const auto& path = pathInRootDir(filename);
        if (auto f = SDL_RWFromFile(path.c_str(), "rb")) {
            logInfo("Loading background image %s", path.c_str());
            return f;
        }
    }

    return nullptr;
}

static void initMenuBackground(char *filename)
{
    if (auto f = openImageFile(filename)) {
        if (m_background)
            SDL_DestroyTexture(m_background);
        m_background = IMG_LoadTexture_RW(getRenderer(), f, true);
        if (!m_background)
            logWarn("Failed to load background image %s, error: %s", filename, IMG_GetError());
    } else {
        logWarn("Failed to open background image file %s", filename);
    }
}

void SWOS::LoadFillAndSwtitle()
{
    initMenuBackground();
}
