#include "menuBackground.h"
#include "menuItemRenderer.h"
#include "windowManager.h"
#include "render.h"
#include "game.h"
#include "file.h"
#include "util.h"

static std::string m_backgroundName;
static SDL_Texture *m_background;

static void loadMenuBackground(std::string name);
static void reloadMenuBackground(AssetResolution = AssetResolution::kInvalid, AssetResolution = AssetResolution::kInvalid);
static SDL_RWops *openImageFile(const std::string& path);

void initMenuBackground()
{
    registerAssetResolutionChangeHandler(reloadMenuBackground);
    setStandardMenuBackgroundImage();
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

void setStandardMenuBackgroundImage()
{
    constexpr char kBackgroundImageBasename[] = "swtitle";
    setMenuBackgroundImage(kBackgroundImageBasename);
}

void setMenuBackgroundImage(const char *name)
{
    if (strcmp(m_backgroundName.c_str(), name)) {
        m_backgroundName = name;
        loadMenuBackground(name);
    }
}

void unloadMenuBackground()
{
    if (m_background) {
        logInfo("Destroying menu background");
        SDL_DestroyTexture(m_background);
    }
    m_background = nullptr;
    m_backgroundName.clear();
}

// Not a reference since background name gets cleared when the old background is destroyed.
static void loadMenuBackground(std::string name)
{
    if (name.empty() || m_background)
        unloadMenuBackground();
    if (!name.empty()) {
        auto path = getPathInAssetDir(name.c_str());
        if (auto f = openImageFile(path)) {
            m_background = IMG_LoadTexture_RW(getRenderer(), f, true);
            if (m_background) {
                m_backgroundName = name;
                logInfo("Menu background set to \"%s\"", name.c_str());
            } else {
                logWarn("Failed to load background image \"%s\", error: %s", name.c_str(), IMG_GetError());
            }
        } else {
            logWarn("Failed to open background image file \"%s\"", name.c_str());
        }
    }
}

static void reloadMenuBackground(AssetResolution, AssetResolution)
{
    if (!isMatchRunning()) {
        loadMenuBackground(m_backgroundName);
    } else {
        assert(m_backgroundName.empty());
    }
}

static SDL_RWops *openImageFile(const std::string& path)
{
    for (auto ext : { "png", "jpg", "jpeg", "bmp", }) {
        const auto& rootPath = pathInRootDir((path + '.' + ext).c_str());
        if (auto f = SDL_RWFromFile(rootPath.c_str(), "rb"))
            return f;
    }

    return nullptr;
}
