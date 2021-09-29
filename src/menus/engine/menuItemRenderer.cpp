#include "menuItemRenderer.h"
#include "windowManager.h"
#include "menus.h"
#include "color.h"

constexpr int kMaxTextureWidth = 2048;
constexpr int kMaxTextureHeight = 2048;
constexpr int kSafeBorder = 2;

static const std::array<std::array<Color, 33>, 7> kGradients = {{
    {{
        // dark blue to black
        { 0, 0, 0 }, { 0, 0, 8 }, { 0, 0, 16 }, { 0, 0, 24 }, { 0, 0, 32 }, { 0, 0, 40 }, { 0, 0, 48 },
        { 0, 0, 56 }, { 0, 0, 64 }, { 0, 0, 72 }, { 0, 0, 80 }, { 0, 0, 88 }, { 0, 0, 96 }, { 0, 0, 104 },
        { 0, 0, 112 }, { 0, 0, 120 }, { 0, 0, 132 }, { 0, 0, 140 }, { 0, 0, 148 }, { 0, 0, 156 },
        { 0, 0, 164 }, { 0, 0, 172 }, { 0, 0, 180 }, { 0, 0, 188 }, { 0, 0, 196 }, { 0, 0, 204 },
        { 0, 0, 212 }, { 0, 0, 220 }, { 0, 0, 228 }, { 0, 0, 236 }, { 0, 0, 244 }, { 0, 0, 252 },
        { 0, 0, 252 },
    }},
    {{
        // orange to brown
        { 108, 36, 0 }, { 112, 36, 0 }, { 116, 40, 0 }, { 120, 44, 0 }, { 124, 48, 0 }, { 128, 52, 0 },
        { 136, 56, 0 }, { 140, 56, 0 }, { 144, 60, 0 }, { 148, 64, 0 }, { 152, 68, 0 }, { 156, 72, 0 },
        { 164, 76, 0 }, { 168, 80, 0 }, { 172, 88, 0 }, { 176, 92, 0 }, { 180, 96, 0 }, { 184, 100, 0 },
        { 192, 104, 0 }, { 196, 112, 0 }, { 200, 116, 0 }, { 204, 120, 0 }, { 208, 124, 0 }, { 212, 132, 0 },
        { 216, 136, 0 }, { 224, 144, 0 }, { 228, 148, 0 }, { 232, 152, 0 }, { 236, 160, 0 }, { 240, 168, 0 },
        { 244, 172, 0 }, { 252, 180, 0 }, { 252, 180, 0 },
    }},
    {{
        // pink to gray/dark gray
        { 36, 36, 36 }, { 40, 40, 40 }, { 48, 48, 48 }, { 56, 52, 52 }, { 60, 60, 60 }, { 68, 64, 64 },
        { 76, 72, 72 }, { 80, 76, 76 }, { 88, 80, 80 }, { 96, 88, 88 }, { 104, 92, 92 }, { 108, 96, 96 },
        { 116, 104, 104 }, { 124, 108, 108 }, { 128, 112, 112 }, { 136, 116, 116 }, { 144, 124, 124 },
        { 148, 128, 128 }, { 156, 132, 132 }, { 164, 136, 136 }, { 168, 140, 140 }, { 176, 144, 144 },
        { 184, 148, 148 }, { 188, 152, 152 }, { 196, 156, 156 }, { 204, 160, 160 }, { 212, 164, 164 },
        { 216, 168, 168 }, { 224, 168, 168 }, { 232, 172, 172 }, { 236, 176, 176 }, { 244, 180, 180 },
        { 244, 180, 180 },
    }},
    {{
        // bright red to red
        { 252, 0, 0 }, { 252, 0, 0 }, { 252, 4, 4 }, { 252, 8, 8 }, { 252, 8, 8 }, { 252, 12, 12 },
        { 252, 16, 16 }, { 252, 20, 20 }, { 252, 24, 24 }, { 252, 28, 28 }, { 252, 32, 32 }, { 252, 36, 36 },
        { 252, 40, 40 }, { 252, 44, 44 }, { 252, 48, 48 }, { 252, 52, 52 }, { 252, 56, 56 }, { 252, 60, 60 },
        { 252, 64, 64 }, { 252, 68, 68 }, { 252, 72, 72 }, { 252, 76, 76 }, { 252, 80, 80 }, { 252, 84, 84 },
        { 252, 88, 88 }, { 252, 92, 92 }, { 252, 96, 96 }, { 252, 100, 100 }, { 252, 104, 104 },
        { 252, 108, 108 }, { 252, 112, 112 }, { 252, 116, 116 }, { 252, 116, 116 },
    }},
    {{
        // purple to Burgundy
        { 108, 0, 36 }, { 112, 0, 44 }, { 116, 0, 52 }, { 120, 4, 64 }, { 124, 4, 72 }, { 128, 8, 84 },
        { 136, 12, 92 }, { 140, 12, 104 }, { 144, 16, 116 }, { 148, 20, 128 }, { 152, 24, 136 },
        { 156, 28, 148 }, { 164, 32, 160 }, { 160, 36, 168 }, { 160, 40, 172 }, { 156, 44, 176 },
        { 152, 48, 180 }, { 152, 56, 184 }, { 152, 60, 192 }, { 148, 64, 196 }, { 144, 72, 200 },
        { 144, 76, 204 }, { 144, 84, 208 }, { 140, 88, 212 }, { 140, 96, 216 }, { 140, 100, 224 },
        { 140, 108, 228 }, { 140, 112, 232 }, { 140, 120, 236 }, { 140, 128, 240 }, { 140, 136, 244 },
        { 144, 144, 252 }, { 144, 144, 252 },
    }},
    {{
        // light blue to blue
        { 0, 0, 252 }, { 0, 4, 248 }, { 4, 16, 248 }, { 4, 28, 248 }, { 8, 36, 244 }, { 12, 48, 244 },
        { 12, 60, 244 }, { 16, 68, 244 }, { 20, 76, 240 }, { 20, 84, 240 }, { 24, 96, 240 },
        { 28, 104, 236 }, { 32, 112, 236 }, { 32, 116, 236 }, { 36, 124, 236 }, { 40, 132, 232 },
        { 40, 140, 232 }, { 44, 148, 232 }, { 48, 152, 228 }, { 48, 160, 228 }, { 52, 164, 228 },
        { 56, 172, 228 }, { 56, 176, 224 }, { 60, 180, 224 }, { 64, 188, 224 }, { 64, 192, 220 },
        { 68, 196, 220 }, { 68, 200, 220 }, { 72, 204, 216 }, { 76, 208, 216 }, { 76, 212, 216 },
        { 80, 216, 216 }, { 80, 216, 216 },
    }},
    {{
        // yellow to green to black
        { 16, 32, 0 }, { 16, 44, 0 }, { 20, 56, 0 }, { 24, 68, 0 }, { 24, 80, 0 }, { 28, 88, 0 },
        { 28, 100, 0 }, { 32, 112, 0 }, { 32, 124, 0 }, { 36, 136, 0 }, { 36, 144, 0 }, { 48, 152, 0 },
        { 60, 156, 0 }, { 68, 160, 0 }, { 80, 168, 0 }, { 88, 172, 0 }, { 100, 176, 0 }, { 108, 180, 0 },
        { 120, 188, 0 }, { 132, 192, 0 }, { 140, 196, 0 }, { 152, 204, 0 }, { 160, 208, 0 }, { 172, 212, 0 },
        { 180, 216, 0 }, { 192, 224, 0 }, { 204, 228, 0 }, { 212, 232, 0 }, { 224, 240, 0 }, { 232, 244, 0 },
        { 244, 248, 0 }, { 252, 252, 0 }, { 252, 252, 0 },
    }},
}};

static const std::array<int, 16> kBackgroundToGradient = {{
    2, 2, 2, 0, 1, 1, 1, 2, 0, 2, 3, 4, 1, 5, 6, 6
}};

// We're counting that there will be no entries with the same top left points
struct EntryKey {
    unsigned x;
    unsigned y;
    unsigned color;

    EntryKey(const MenuEntry& entry)
        : x(entry.x), y(entry.y), color(entry.backgroundColor()) {}
    EntryKey(const MenuEntry *entry) : EntryKey(*entry) {}

    bool operator==(const EntryKey& other) const {
        return x == other.x && y == other.y && color == other.color;
    }
};

namespace std {
    template<>
    struct hash<EntryKey> {
        size_t operator()(const EntryKey& key) const {
            return key.color | (key.x << 10) | (key.y << 20);
        }
    };
}

struct EntryRenderInfo {
    EntryRenderInfo() = default;

    SDL_Rect src;
    SDL_FRect dst;
    int color;
    int texture;
#ifdef DEBUG
    const MenuEntry *entry;
#endif
};

static std::unordered_map<EntryKey, EntryRenderInfo> m_cache;
static std::vector<SDL_Texture *> m_textures;
static std::pair<int, int> m_cacheScreenDimensions;

static std::vector<EntryRenderInfo> m_currentSurfaceEntries;
static int m_nextTextureIndex;

static int m_x = kSafeBorder;
static int m_y = kSafeBorder;
static int m_maxX;
static int m_maxY;

static void clearCache();
static void ensureCacheValidity();
static void enqueueEntryBackgroundForCaching(const MenuEntry& entry);
static SDL_Surface *createSurface(int width, int height);
static SDL_Texture *createTexture(SDL_Surface *surface);
static void createAndFillTextureAtlas();
static void renderEntryBackgroundSingleColor(const EntryRenderInfo& surfaceEntry);
static void fillEntryBackground(SDL_Surface *surface, const EntryRenderInfo& surfaceEntry);

void cacheMenuItemBackgrounds()
{
    clearCache();

    auto menu = getCurrentMenu();
    auto entries = menu->entries();

    for (unsigned i = 0; i < menu->numEntries; i++) {
        const auto& entry = entries[i];
        if (entry.visible() && entry.background == kEntryFrameAndBackColor && entry.backgroundColor() != kNoBackground) {
            assert(m_cache.find(entry) == m_cache.end());
            enqueueEntryBackgroundForCaching(entry);
        }
    }

    createAndFillTextureAtlas();
    m_cacheScreenDimensions = getWindowSize();
}

static SDL_FRect getTransformedRect(const MenuEntry *entry)
{
    return mapRect(entry->x, entry->y, entry->width, entry->height);
}

static EntryRenderInfo getEntryRenderInfo(const MenuEntry& entry, int texture = 0)
{
    EntryRenderInfo info;

    info.dst = getTransformedRect(&entry);

    auto xRoundedUp = std::ceil(info.dst.x);
    auto yRoundedUp = std::ceil(info.dst.y);
    info.dst.w -= (xRoundedUp - info.dst.x);
    info.dst.h -= (yRoundedUp - info.dst.y);
    info.dst.x = xRoundedUp;
    info.dst.y = yRoundedUp;

    // we'll use rounded-down values for source rectangle, since there's no option for it to be float
    info.src.x = 0;
    info.src.y = 0;
    info.src.w = static_cast<int>(std::floor(info.dst.w));
    info.src.h = static_cast<int>(std::floor(info.dst.h));

    info.color = entry.backgroundColor();
    info.texture = texture;

#ifdef DEBUG
    info.entry = &entry;
#endif

    return info;
}

static void renderBackground(const EntryRenderInfo& entry)
{
    if (auto texture = m_textures[entry.texture])
        SDL_RenderCopyF(getRenderer(), texture, &entry.src, &entry.dst);
    else
        renderEntryBackgroundSingleColor(entry);
}

void drawMenuItemSolidBackground(const MenuEntry *entry)
{
    assert(entry);

    ensureCacheValidity();

    auto it = m_cache.find(entry);
    if (it != m_cache.end()) {
        renderBackground(it->second);
    } else {
        auto renderInfo = getEntryRenderInfo(*entry);
        auto surface = createSurface(renderInfo.src.w, renderInfo.src.h);
        if (surface) {
            fillEntryBackground(surface, renderInfo);
            m_textures.push_back(createTexture(surface));
            renderInfo.texture = m_textures.size() - 1;
            m_cache.insert({ entry, renderInfo });
            renderBackground(renderInfo);
        } else {
            renderEntryBackgroundSingleColor(renderInfo);
        }
    }
}

void drawMenuItemInnerFrame(int x, int y, int width, int height, word color)
{
    assert(x >= 0 && x + width <= kMenuScreenWidth && y >= 0 && y + height <= kMenuScreenHeight && color < 16);

    const auto& rgbColor = kMenuPalette[color];
    drawRectangle(x, y, width, height, rgbColor);
}

void drawMenuItemOuterFrame(MenuEntry *entry)
{
    assert(entry->x >= 0 && entry->x + entry->width <= kMenuScreenWidth && entry->y >= 0 && entry->y + entry->height <= kMenuScreenHeight);

    static const std::array<Color, 5> kOuterFrameColors = {{
        { 180, 180, 180 }, { 252, 108, 0 }, { 252, 252, 0 }, { 144, 144, 252 }, { 252, 0, 0 },
    }};
    static const std::array<uint8_t, 16> kOuterFrameColorIndices = {{
        0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 3, 4, 0, 0, 0,
    }};

    const auto& color = kOuterFrameColors[kOuterFrameColorIndices[entry->backgroundColor()]];
    drawRectangle(entry->x, entry->y, entry->width, entry->height, color);
}

static void clearCache()
{
    assert(m_currentSurfaceEntries.empty());

    m_cache.clear();

    for (const auto& texture : m_textures)
        if (texture)
            SDL_DestroyTexture(texture);

    m_textures.clear();

    m_nextTextureIndex = 0;
    m_x = kSafeBorder;
    m_y = kSafeBorder;
    m_maxX = kSafeBorder;
    m_maxY = kSafeBorder;
}

static void ensureCacheValidity()
{
    const auto& screenDimensions = getWindowSize();
    if (m_cacheScreenDimensions != screenDimensions) {
        logInfo("Screen dimensions changed, cached: %d, %d -> actual: %d, %d", m_cacheScreenDimensions.first,
            m_cacheScreenDimensions.second, screenDimensions.first, screenDimensions.second);
        clearCache();
        m_cacheScreenDimensions = screenDimensions;
    }
}

static void createSingleBigTexture(const MenuEntry& entry, EntryRenderInfo& info)
{
    SDL_Texture *texture = nullptr;
    if (auto surface = createSurface(info.src.w, info.src.h)) {
        fillEntryBackground(surface, info);
        texture = createTexture(surface);
    }

    m_textures.push_back(texture);

    info.src.x = 0;
    info.src.y = 0;
}

static void enqueueEntryBackgroundForCaching(const MenuEntry& entry)
{
    assert(entry.background == kEntryFrameAndBackColor && entry.backgroundColor() != kNoBackground);

    auto info = getEntryRenderInfo(entry, m_nextTextureIndex);

    if (info.src.w > kMaxTextureWidth || info.src.h > kMaxTextureHeight) {
        createSingleBigTexture(entry, info);

        for (auto& surfaceEntry : m_currentSurfaceEntries)
            surfaceEntry.texture++;
        for (auto& cachEntry : m_cache)
            if (cachEntry.second.texture == m_nextTextureIndex)
                cachEntry.second.texture++;

        m_nextTextureIndex++;
        m_cache.insert({ entry, info });
        return;
    }

    if (m_y + info.src.h + kSafeBorder <= kMaxTextureHeight) {
        if (m_x + info.src.w + kSafeBorder <= kMaxTextureWidth) {
            info.src.x = m_x;
            info.src.y = m_y;
            m_currentSurfaceEntries.push_back(info);

            m_x += info.src.w + kSafeBorder;
            m_maxX = std::max(m_maxX, m_x);
            m_maxY = std::max(m_maxY, m_y + info.src.h + kSafeBorder);
        } else {
            info.src.x = kSafeBorder;
            info.src.y = m_maxY;
            m_currentSurfaceEntries.push_back(info);

            m_x = info.src.w + 2 * kSafeBorder;
            m_maxX = std::max(m_maxX, m_x);
            m_y = m_maxY;
            m_maxY += info.src.h + kSafeBorder;
        }
    } else {
        createAndFillTextureAtlas();
        m_x = info.src.w + 2 * kSafeBorder;
        m_y = kSafeBorder;
        m_maxY = info.src.h + kSafeBorder;
        info.src.x = kSafeBorder;
        info.src.y = kSafeBorder;
        info.texture++;
        m_nextTextureIndex++;
        m_currentSurfaceEntries.push_back(info);
    }

    assert(!m_currentSurfaceEntries.empty());

    m_cache.insert({ entry, m_currentSurfaceEntries.back() });
}

static SDL_Surface *createSurface(int width, int height)
{
    auto surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface)
        logWarn("Failed to create menu item surface, size %d x %d", width, height);

    return surface;
}

static SDL_Texture *createTexture(SDL_Surface *surface)
{
    assert(surface);

    auto texture = SDL_CreateTextureFromSurface(getRenderer(), surface);
    if (!texture)
        logWarn("Failed to create menu item texture, size %d x %d", surface->w, surface->h);

    SDL_FreeSurface(surface);

    return texture;
}

void createAndFillTextureAtlas()
{
    if (m_maxX <= kSafeBorder && m_maxY <= kSafeBorder)
        return;

    if (auto surface = createSurface(m_maxX, m_maxY)) {
        for (const auto& surfaceEntry : m_currentSurfaceEntries) {
            assert(surfaceEntry.src.x < m_maxX && surfaceEntry.src.y < m_maxY);
            fillEntryBackground(surface, surfaceEntry);
        }
        m_textures.push_back(createTexture(surface));
    } else {
        logWarn("Failed to allocate %d x %d surface for menu item background texture atlas", m_maxX, m_maxY);
        m_textures.push_back(nullptr);
    }

    m_currentSurfaceEntries.clear();
    m_maxX = kSafeBorder;
    m_maxY = kSafeBorder;
}

// Renders plain single-color background in case surface/texture allocation fails.
static void renderEntryBackgroundSingleColor(const EntryRenderInfo& surfaceEntry)
{
    assert(surfaceEntry.color >= 0 && surfaceEntry.color < 16);

    const auto& gradient = kGradients[kBackgroundToGradient[surfaceEntry.color]];
    SDL_SetRenderDrawColor(getRenderer(), gradient[16].r, gradient[16].g, gradient[16].b, 255);

    SDL_RenderFillRectF(getRenderer(), &surfaceEntry.dst);
}

static ColorF colorAtPoint(float x, float y, float diagonalSegment, int color)
{
    auto gradient = std::sqrt(x*x + y*y) / diagonalSegment;
    assert(gradient >= 0 && gradient < 32);

    auto lo = static_cast<int>(std::floor(gradient));
    auto hi = static_cast<int>(std::ceil(gradient));
    auto frac = gradient - static_cast<float>(lo);

    const auto& c1 = kGradients[kBackgroundToGradient[color]][lo];
    const auto& c2 = kGradients[kBackgroundToGradient[color]][hi];

    auto r = c1.r * (1 - frac) + c2.r * frac;
    auto g = c1.g * (1 - frac) + c2.g * frac;
    auto b = c1.b * (1 - frac) + c2.b * frac;

    return { r, g, b };
}

static void fillEntryBackground(SDL_Surface *surface, const EntryRenderInfo& surfaceEntry)
{
    assert(surfaceEntry.src.x >= 0 && surfaceEntry.src.y >= 0 &&
        surfaceEntry.src.x + surfaceEntry.src.w <= surface->w && surfaceEntry.src.y + surfaceEntry.src.h <= surface->h);
    assert(surfaceEntry.color >= 0 && surfaceEntry.color < 16);

    int screenWidth, screenHeight;
    std::tie(screenWidth, screenHeight) = getWindowSize();

    assert((std::round(surfaceEntry.dst.x) >= 0 && std::round(surfaceEntry.dst.x + surfaceEntry.dst.w) <= screenWidth ||
        std::round(surfaceEntry.dst.x) < 0 && std::round(surfaceEntry.dst.x + surfaceEntry.dst.w) < 0) &&
        (std::round(surfaceEntry.dst.y) >= 0 && std::round(surfaceEntry.dst.y + surfaceEntry.dst.h) <= screenHeight ||
        std::round(surfaceEntry.dst.y) < 0 && std::round(surfaceEntry.dst.y + surfaceEntry.dst.h) < 0));

    // length of the screen diagonal divided into 32 parts (color intensities for each gradient color)
    auto diagSeg = std::sqrt(static_cast<float>(screenWidth) * screenWidth + screenHeight * screenHeight) / 32.0f;

    // only calculate exact values at the vertex points of the rectangle
    // 1 -- 2
    // 3 -- 4
    auto y1 = static_cast<float>(screenHeight - 1 - surfaceEntry.dst.y);
    auto y2 = static_cast<float>(screenHeight - surfaceEntry.dst.y - surfaceEntry.src.h);
    auto x1 = static_cast<float>(surfaceEntry.dst.x);
    auto x2 = static_cast<float>(surfaceEntry.dst.x + surfaceEntry.src.w - 1);

    // calculate R, G, B values for vertex points
    const auto& c1 = colorAtPoint(x1, y1, diagSeg, surfaceEntry.color);
    const auto& c2 = colorAtPoint(x2, y1, diagSeg, surfaceEntry.color);
    const auto& c3 = colorAtPoint(x1, y2, diagSeg, surfaceEntry.color);
    const auto& c4 = colorAtPoint(x2, y2, diagSeg, surfaceEntry.color);

    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0)
        return;

    auto p = reinterpret_cast<Uint32 *>(surface->pixels) + surface->pitch / 4 * surfaceEntry.src.y + surfaceEntry.src.x;

    // linearly interpolate between the vertex points
    for (int y = 0; y < surfaceEntry.src.h; y++) {
        auto rLeft = c1.r + (c3.r - c1.r) / (surfaceEntry.src.h - 1) * y;
        auto gLeft = c1.g + (c3.g - c1.g) / (surfaceEntry.src.h - 1) * y;
        auto bLeft = c1.b + (c3.b - c1.b) / (surfaceEntry.src.h - 1) * y;
        auto rRight = c2.r + (c4.r - c2.r) / (surfaceEntry.src.h - 1) * y;
        auto gRight = c2.g + (c4.g - c2.g) / (surfaceEntry.src.h - 1) * y;
        auto bRight = c2.b + (c4.b - c2.b) / (surfaceEntry.src.h - 1) * y;

        for (int x = 0; x < surfaceEntry.src.w; x++) {
            unsigned r = std::lround(rLeft + (rRight - rLeft) / (surfaceEntry.src.w - 1) * x);
            unsigned g = std::lround(gLeft + (gRight - gLeft) / (surfaceEntry.src.w - 1) * x);
            unsigned b = std::lround(bLeft + (bRight - bLeft) / (surfaceEntry.src.w - 1) * x);

            p[y * surface->pitch / 4 + x] = (r << surface->format->Rshift) | (g << surface->format->Gshift) |
                (b << surface->format->Bshift) | (255 << surface->format->Ashift);
        }
    }

    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
}
