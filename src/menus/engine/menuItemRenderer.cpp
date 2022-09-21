#include "menuItemRenderer.h"
#include "windowManager.h"
#include "gameFieldMapping.h"
#include "render.h"
#include "drawPrimitives.h"
#include "menus.h"
#include "unpackMenu.h"
#include "color.h"
#include <future>

constexpr int kMinTextureWidth = 2048;
constexpr int kMinTextureHeight = 2048;
constexpr int kSafeBorder = 2;

static bool m_useGradientFill;
static bool m_initialized;

// Menu item backgrounds, radial color gradients in essence:
// - each pixel at (x,y) gets a color depending on the distance from the coordinate beginning
// - gradient stop colors are arranged following the screen diagonal at every 1/32
// - colors in between are linearly interpolated.
//
// d = sqr(width^2 + height^2)
// l = sqr(x^2 + y^2)
// s = l / d * 32 ; note that l/d is in [0, 1) range, and s in [0, 32)
// fraction = s - floor(s)
// color at (x,y) = gradient[floor(s)] * (1 - fraction) + gradient[ceil(s)] * fraction
//
//       +----------/ #32
//       .         .
//       .        .
//       |       /
//       |      / #2
//       |     /
//       |    /
//       |   / #1
//       |  /
//       | /
//       |/ #0
//       +----------
//
constexpr int kNumGradients = 7;

static const std::array<std::array<Color, 33>, kNumGradients> kGradients = {{
    {{
        // dark blue to black
        { 0, 0, 0 }, { 0, 0, 8 }, { 0, 0, 16 }, { 0, 0, 24 }, { 0, 0, 32 }, { 0, 0, 40 }, { 0, 0, 48 },
        { 0, 0, 56 }, { 0, 0, 64 }, { 0, 0, 72 }, { 0, 0, 80 }, { 0, 0, 88 }, { 0, 0, 96 }, { 0, 0, 104 },
        { 0, 0, 112 }, { 0, 0, 120 }, { 0, 0, 132 }, { 0, 0, 140 }, { 0, 0, 148 }, { 0, 0, 156 },
        { 0, 0, 164 }, { 0, 0, 172 }, { 0, 0, 180 }, { 0, 0, 188 }, { 0, 0, 196 }, { 0, 0, 204 },
        { 0, 0, 212 }, { 0, 0, 220 }, { 0, 0, 228 }, { 0, 0, 236 }, { 0, 0, 244 }, { 0, 0, 252 },
        { 0, 0, 255 },
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
        { 144, 144, 252 }, { 144, 144, 255 },
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

// Fixed point color, 9 bits integer part (highest bit = sign bit), 23 bits fraction
struct ColorF
{
    static constexpr int kColorShift = 23;
    static constexpr int kColorRoundLimit = 0x400000;

    int32_t r;
    int32_t g;
    int32_t b;

    ColorF(float rF, float gF, float bF)
    {
        assert(rF >= 0 && rF <= 255 && gF >= 0 && gF <= 255 && bF >= 0 && bF <= 255);
        r = floatToFixed(rF);
        g = floatToFixed(gF);
        b = floatToFixed(bF);
    }
    static int floatToFixed(float value)
    {
        float whole, fraction = std::modf(value, &whole);
        return static_cast<int>(whole) << kColorShift | static_cast<int>(fraction * ((1 << kColorShift) - 1));
    }
};

#define FIXED_TO_INT(value) (((value) >> ColorF::kColorShift) + (((value) & ((1 << ColorF::kColorShift) - 1)) > ColorF::kColorRoundLimit))

struct Texture {
    Texture(SDL_Texture *texture, int refCount) : texture(texture), refCount(refCount) {}
    SDL_Texture *texture = nullptr;
    int refCount = 0;
};

using TextureList = std::list<Texture>;

// x, y coordinates, gradient and resolution should make an entry unique.
// Possible optimization is to remove entries that are completely contained inside other entries, but
// it's not worth it (complicated to implement and not that much too gain).
struct EntryRenderInfo {
    EntryRenderInfo(const MenuEntry& entry) {
        dst = getTransformedRect(&entry);

        auto xRoundedUp = std::ceil(dst.x);
        auto yRoundedUp = std::ceil(dst.y);
        dst.w -= (xRoundedUp - dst.x);
        dst.h -= (yRoundedUp - dst.y);
        dst.x = xRoundedUp;
        dst.y = yRoundedUp;

        // we'll use rounded-down values for source rectangle, since there's currently no option for it to be float
        src.x = 0;
        src.y = 0;
        src.w = static_cast<int>(std::floor(dst.w));
        src.h = static_cast<int>(std::floor(dst.h));

        assert(entry.backgroundColor() < kBackgroundToGradient.size());
        gradient = kBackgroundToGradient[entry.backgroundColor()];

#ifdef DEBUG
        this->entry = &entry;
#endif
    }
    EntryRenderInfo(const MenuEntry *entry) : EntryRenderInfo(*entry) {}

    static SDL_FRect getTransformedRect(const MenuEntry *entry) {
        return mapRect(entry->x, entry->y, entry->width, entry->height);
    }

    SDL_Rect src;
    SDL_FRect dst;
    int gradient;
    TextureList::iterator texture;
    const void *menu = nullptr;
#ifdef DEBUG
    const MenuEntry *entry;
#endif
};

static std::pair<int, int> m_screenDimensions;

struct EntryKey {
    int x;
    int y;
    int width;
    int height;
    int gradient;
    std::pair<int, int> screenDimensions;

    EntryKey(const MenuEntry& entry)
        : x(entry.x), y(entry.y), width(entry.width), height(entry.height),
        gradient(kBackgroundToGradient[entry.backgroundColor()]), screenDimensions(m_screenDimensions)
    {}
    EntryKey(const MenuEntry *entry) : EntryKey(*entry) {}

    bool operator==(const EntryKey& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height &&
            gradient == other.gradient && screenDimensions == other.screenDimensions;
    }
};

namespace std {
    template<>
    struct hash<EntryKey> {
        size_t operator()(const EntryKey& key) const {
            return (key.x | (key.y << 8) | (key.width << 16) | (key.height << 24)) ^
                (key.screenDimensions.first | (key.screenDimensions.second << 16));
        }
    };
}

// Structure needed for grouping entries into a single surface/texture.
struct TexturePartitionContext {
    bool hasContent() const {
        return maxX > kSafeBorder || maxY > kSafeBorder;
    }
    int x = kSafeBorder;
    int y = kSafeBorder;
    int maxX = kSafeBorder;
    int maxY = kSafeBorder;
    std::vector<EntryRenderInfo> entries;
    std::vector<const MenuEntry *> menuEntries;
    SDL_Surface *surface = nullptr;
};
using QueuedTextures = std::vector<TexturePartitionContext>;

static constexpr size_t kMaxCachedMenus = 6;

using EntryCache = std::unordered_map<EntryKey, EntryRenderInfo>;
static std::array<EntryCache, kNumGradients> m_cache;

using CachedMenu = std::pair<const void *, std::pair<int, int>>;
static std::list<CachedMenu> m_cachedMenus;

static TextureList m_textures = {{ nullptr, 1 }};   // keep null texture first always
static TextureList::iterator m_lastTexture;

static int m_maxTextureAtlasWidth;
static int m_maxTextureAtlasHeight;

static bool updateScreenDimensions();
static bool currentMenuInCache();
static void trimCache();
static SDL_Texture *lookupTexture(const EntryRenderInfo& entry);
static EntryRenderInfo *lookupEntry(const MenuEntry *entry);
static void createTexture(SDL_Surface *surface, std::vector<EntryRenderInfo>& entries);
static void createTexture(SDL_Surface *surface, EntryRenderInfo& entry);
static TextureList::iterator createTexture(SDL_Surface *surface, int refCount);
static void insertEntry(const MenuEntry *entry, const EntryRenderInfo& renderInfo);
static EntryCache::iterator deleteCachedEntry(EntryCache& cache, EntryCache::iterator cacheIt);
static void clearMenuCache(bool all);
static void clearMenuCache(const CachedMenu& menu);
static void ensureCacheValidity();
static void enqueueEntryBackgroundForCaching(const MenuEntry& entry, QueuedTextures& context);
static SDL_Surface *createSurface(int width, int height);
static void createAndFillTextureAtlases(QueuedTextures& context);
static void renderEntryBackgroundSingleColor(const EntryRenderInfo& surfaceEntry);
static void fillEntryBackground(SDL_Surface *surface, const EntryRenderInfo& surfaceEntry);

void initMenuItemRenderer()
{
    updateScreenDimensions();
    m_initialized = true;
}

void cacheMenuItemBackgrounds()
{
    if (!m_useGradientFill || currentMenuInCache())
        return;

    trimCache();

    auto startTime = SDL_GetPerformanceCounter();
    updateScreenDimensions();

    auto menu = getCurrentMenu();
    auto entries = menu->entries();
    QueuedTextures queuedTextures(1);

    for (unsigned i = 0; i < menu->numEntries; i++) {
        const auto& entry = entries[i];
        if (entry.visible() && entry.background == kEntryFrameAndBackColor && entry.backgroundColor() != kNoBackground) {
            if (!lookupEntry(&entry))
                enqueueEntryBackgroundForCaching(entry, queuedTextures);
        }
    }

    if (queuedTextures.size() > 1 || queuedTextures.back().hasContent()) {
        createAndFillTextureAtlases(queuedTextures);
        m_cachedMenus.emplace_back(getCurrentPackedMenu(), m_screenDimensions);
        auto interval = SDL_GetPerformanceCounter() - startTime;
        logInfo("Background cache built in %.2fms", static_cast<double>(interval * 1000) / SDL_GetPerformanceFrequency());
    }
}

static void renderBackground(const EntryRenderInfo& entry)
{
    if (auto texture = lookupTexture(entry))
        SDL_RenderCopyF(getRenderer(), texture, &entry.src, &entry.dst);
    else
        renderEntryBackgroundSingleColor(entry);
}

void drawMenuItemSolidBackground(const MenuEntry *entry)
{
    assert(entry);

    if (!m_useGradientFill) {
        renderEntryBackgroundSingleColor(entry);
        return;
    }

    ensureCacheValidity();

    auto entryRenderInfo = lookupEntry(entry);
    if (entryRenderInfo) {
        renderBackground(*entryRenderInfo);
    } else {
        EntryRenderInfo renderInfo(*entry);
        auto surface = createSurface(renderInfo.src.w, renderInfo.src.h);
        if (surface) {
            fillEntryBackground(surface, renderInfo);
            createTexture(surface, renderInfo);
            insertEntry(entry, renderInfo);
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

bool menuGradientFillEnabled()
{
    return m_useGradientFill;
}

void enableMenuGradientFill(bool enable)
{
    if (enable != m_useGradientFill) {
        m_useGradientFill = enable;
        if (m_initialized) {
            if (enable)
                cacheMenuItemBackgrounds();
            else
                clearMenuCache(true);
        }
    }
}

static bool updateScreenDimensions()
{
    const auto& screenDimensions = getWindowSize();
    if (screenDimensions != m_screenDimensions) {
        SDL_RendererInfo info;
        SDL_GetRendererInfo(getRenderer(), &info);
        m_maxTextureAtlasWidth = std::min(info.max_texture_width, m_screenDimensions.first);
        m_maxTextureAtlasWidth = std::max(m_maxTextureAtlasWidth, kMinTextureWidth);
        m_maxTextureAtlasHeight = std::min(info.max_texture_height, m_screenDimensions.second);
        m_maxTextureAtlasHeight = std::max(m_maxTextureAtlasHeight, kMinTextureHeight);
        logInfo("Screen dimensions changed, cached: %d, %d -> actual: %d, %d", m_screenDimensions.first,
            m_screenDimensions.second, screenDimensions.first, screenDimensions.second);
        m_screenDimensions = screenDimensions;
        return true;
    } else {
        return false;
    }
}

static bool currentMenuInCache()
{
    auto menu = getCurrentPackedMenu();
    auto it = std::find_if(m_cachedMenus.begin(), m_cachedMenus.end(), [menu](const auto& cachedMenu) {
        return cachedMenu.first == menu && cachedMenu.second == m_screenDimensions;
    });
    return it != m_cachedMenus.end();
}

static void trimCache()
{
    if (m_cachedMenus.size() >= kMaxCachedMenus) {
        clearMenuCache(m_cachedMenus.front());
        m_cachedMenus.pop_front();
    }
}

static SDL_Texture *lookupTexture(const EntryRenderInfo& entry)
{
    return entry.texture != m_textures.end() ? entry.texture->texture : nullptr;
}

static EntryRenderInfo *lookupEntry(const MenuEntry *entry)
{
    int gradient = kBackgroundToGradient[entry->backgroundColor()];
    auto it = m_cache[gradient].find(entry);
    if (it != m_cache[gradient].end()) {
        // force the last used menu to "own" this entry (so it doesn't get reclaimed prematurely)
        it->second.menu = getCurrentPackedMenu();
        return &it->second;
    } else {
        return nullptr;
    }
}

static void createTexture(SDL_Surface *surface, std::vector<EntryRenderInfo>& entries)
{
    auto it = createTexture(surface, entries.size());
    for (auto& entry : entries)
        entry.texture = it;
}

static void createTexture(SDL_Surface *surface, EntryRenderInfo& entry)
{
    auto it = createTexture(surface, 1);
    entry.texture = it;
}

static TextureList::iterator createTexture(SDL_Surface *surface, int refCount)
{
    assert(surface);

    auto texture = SDL_CreateTextureFromSurface(getRenderer(), surface);
    if (!texture)
        logWarn("Failed to create menu item texture, size %d x %d", surface->w, surface->h);

    SDL_FreeSurface(surface);

    if (texture) {
        m_textures.emplace_back(texture, refCount);
        return std::prev(m_textures.end());
    } else {
        assert(!m_textures.empty() && m_textures.front().texture == nullptr && m_textures.front().refCount == 1);
        return m_textures.begin();
    }
}

static void insertEntry(const MenuEntry *entry, const EntryRenderInfo& renderInfo)
{
    m_cache[renderInfo.gradient].insert({ entry, renderInfo });
}

static EntryCache::iterator deleteCachedEntry(EntryCache& cache, EntryCache::iterator cacheIt)
{
    auto& textureIt = cacheIt->second.texture;
    if (textureIt != m_textures.end() && textureIt->texture) {
        if (!--textureIt->refCount) {
            SDL_DestroyTexture(textureIt->texture);
            m_textures.erase(textureIt);
        }
    }
    return cache.erase(cacheIt);
}

static void clearMenuCache(bool all)
{
    if (all) {
        for (auto& cache : m_cache)
            cache.clear();

        for (auto& texture : m_textures)
            if (texture.texture)
                SDL_DestroyTexture(texture.texture);

        m_textures.clear();
    } else {
        auto currentMenu = getCurrentMenu();
        for (auto& cache : m_cache) {
            for (auto it = cache.begin(); it != cache.end(); ) {
                if (it->second.menu == currentMenu)
                    it = deleteCachedEntry(cache, it);
                else
                    ++it;
            }
        }
    }
}

static void clearMenuCache(const CachedMenu& menu)
{
    for (auto& cache : m_cache) {
        for (auto it = cache.begin(); it != cache.end(); ) {
            if (it->second.menu == menu.first && it->first.screenDimensions == menu.second)
                it = cache.erase(it);
            else
                ++it;
        }
    }
}

static void ensureCacheValidity()
{
    if (updateScreenDimensions())
        clearMenuCache(false);
}

static void enqueueEntryBackgroundForCaching(const MenuEntry& entry, QueuedTextures& queuedTextures)
{
    assert(entry.background == kEntryFrameAndBackColor && entry.backgroundColor() != kNoBackground);
    assert(!queuedTextures.empty());

    EntryRenderInfo info(entry);
    auto& context = queuedTextures.back();

    if (info.src.w > m_screenDimensions.first || info.src.h > m_screenDimensions.second) {
        assert(false);
        context.entries.push_back(info);
        context.menuEntries.push_back(&entry);
        context.maxX = info.src.w;
        context.maxY = info.src.y;
        queuedTextures.emplace_back();
    } else if (context.y + info.src.h + kSafeBorder <= m_maxTextureAtlasHeight) {
        if (context.x + info.src.w + kSafeBorder <= m_maxTextureAtlasWidth) {
            info.src.x = context.x;
            info.src.y = context.y;
            context.entries.push_back(info);
            context.menuEntries.push_back(&entry);

            context.x += info.src.w + kSafeBorder;
            context.maxX = std::max(context.maxX, context.x);
            context.maxY = std::max(context.maxY, context.y + info.src.h + kSafeBorder);
        } else {
            info.src.x = kSafeBorder;
            info.src.y = context.maxY;
            context.entries.push_back(info);
            context.menuEntries.push_back(&entry);

            context.x = info.src.w + 2 * kSafeBorder;
            context.maxX = std::max(context.maxX, context.x);
            context.y = context.maxY;
            context.maxY += info.src.h + kSafeBorder;
        }
    } else {
        queuedTextures.emplace_back();

        auto& context = queuedTextures.back();
        context.x = info.src.w + 2 * kSafeBorder;
        context.y = kSafeBorder;
        context.maxX = context.x;
        context.maxY = info.src.h + kSafeBorder;

        info.src.x = kSafeBorder;
        info.src.y = kSafeBorder;
        context.entries.push_back(info);
        context.menuEntries.push_back(&entry);
    }
    assert(!context.entries.empty());
}

static SDL_Surface *createSurface(int width, int height)
{
    auto surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface)
        logWarn("Failed to create menu item surface, size %d x %d", width, height);

    return surface;
}

void createAndFillTextureAtlases(QueuedTextures& queuedTextures)
{
    std::vector<std::future<void>> futures;
    for (auto& context : queuedTextures) {
        if (context.hasContent()) {
            context.surface = createSurface(context.maxX, context.maxY);
            if (context.surface) {
                for (const auto& surfaceEntry : context.entries) {
                    assert(surfaceEntry.src.x < context.maxX && surfaceEntry.src.y < context.maxY);
                    futures.emplace_back(std::async(std::launch::async, fillEntryBackground, context.surface, surfaceEntry));
                }
            } else {
                logWarn("Failed to allocate %d x %d surface for menu item background texture atlas", context.maxX, context.maxY);
            }
        }
    }

    // Normally we should just let the futures get destroyed and they'll block until the job's done, but
    // I'm getting occasional abort() in debug mode, so let's explicitly wait() on them to be safe
    for (const auto& future : futures)
        future.wait();

    for (auto& context : queuedTextures) {
        if (context.hasContent() && context.surface)
            createTexture(context.surface, context.entries);

        assert(context.entries.size() == context.menuEntries.size());
        for (size_t i = 0; i < context.entries.size(); i++)
            insertEntry(context.menuEntries[i], context.entries[i]);
    }
}

// Renders plain single-color background in case surface/texture allocation fails.
static void renderEntryBackgroundSingleColor(const EntryRenderInfo& surfaceEntry)
{
    assert(surfaceEntry.gradient >= 0 && surfaceEntry.gradient < kNumGradients);

    const auto& gradient = kGradients[surfaceEntry.gradient];
    SDL_SetRenderDrawColor(getRenderer(), gradient[16].r, gradient[16].g, gradient[16].b, 255);
    SDL_RenderFillRectF(getRenderer(), &surfaceEntry.dst);
}

static ColorF colorAtPoint(float x, float y, float diagonalSegment, int gradient)
{
    // length from (0,0) to (x,y) divided by screen diagonal: [0..1) times 32 (# of gradient segments - 1)
    auto gradientSegment = std::sqrt(x*x + y*y) / diagonalSegment;
    assert(gradientSegment >= 0 && gradientSegment < 32);

    auto lo = static_cast<int>(std::floor(gradientSegment));
    auto hi = static_cast<int>(std::ceil(gradientSegment));
    auto frac = gradientSegment - static_cast<float>(lo);

    const auto& c1 = kGradients[gradient][lo];
    const auto& c2 = kGradients[gradient][hi];

    auto r = c1.r * (1 - frac) + c2.r * frac;
    auto g = c1.g * (1 - frac) + c2.g * frac;
    auto b = c1.b * (1 - frac) + c2.b * frac;

    return { r, g, b };
}

static void fillEntryBackground(SDL_Surface *surface, const EntryRenderInfo& surfaceEntry)
{
    assert(surfaceEntry.src.x >= 0 && surfaceEntry.src.y >= 0 &&
        surfaceEntry.src.x + surfaceEntry.src.w <= surface->w && surfaceEntry.src.y + surfaceEntry.src.h <= surface->h);
    assert(surfaceEntry.gradient >= 0 && surfaceEntry.gradient < kNumGradients);

    int screenWidth, screenHeight;
    std::tie(screenWidth, screenHeight) = getWindowSize();

    assert((std::round(surfaceEntry.dst.x) >= 0 && std::round(surfaceEntry.dst.x + surfaceEntry.dst.w) <= screenWidth ||
        std::round(surfaceEntry.dst.x) < 0 && std::round(surfaceEntry.dst.x + surfaceEntry.dst.w) < 0) &&
        (std::round(surfaceEntry.dst.y) >= 0 && std::round(surfaceEntry.dst.y + surfaceEntry.dst.h) <= screenHeight ||
        std::round(surfaceEntry.dst.y) < 0 && std::round(surfaceEntry.dst.y + surfaceEntry.dst.h) < 0));

    // length of the screen diagonal divided into 32 parts (color intensities for each gradient color)
    auto diagSeg = std::sqrt(static_cast<float>(screenWidth) * screenWidth + screenHeight * screenHeight) / 32.0f;

    // only calculate exact values at the vertex points of the rectangle
    // c1 -- c2
    //  |    |
    // c3 -- c4
    auto y1 = static_cast<float>(screenHeight - 1 - surfaceEntry.dst.y);
    auto y2 = static_cast<float>(screenHeight - surfaceEntry.dst.y - surfaceEntry.src.h);
    auto x1 = static_cast<float>(surfaceEntry.dst.x);
    auto x2 = static_cast<float>(surfaceEntry.dst.x + surfaceEntry.src.w - 1);

    // fetch R,G,B values for vertex points
    const auto& c1 = colorAtPoint(x1, y1, diagSeg, surfaceEntry.gradient);
    const auto& c2 = colorAtPoint(x2, y1, diagSeg, surfaceEntry.gradient);
    const auto& c3 = colorAtPoint(x1, y2, diagSeg, surfaceEntry.gradient);
    const auto& c4 = colorAtPoint(x2, y2, diagSeg, surfaceEntry.gradient);

    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0)
        return;

    auto p = reinterpret_cast<Uint32 *>(surface->pixels) + surface->pitch / 4 * surfaceEntry.src.y + surfaceEntry.src.x;

    // linearly interpolate between the vertex points
    auto rLeft = c1.r;
    auto gLeft = c1.g;
    auto bLeft = c1.b;
    auto rRight = c2.r;
    auto gRight = c2.g;
    auto bRight = c2.b;
    auto rLeftInc = (c3.r - c1.r) / (surfaceEntry.src.h - 1);
    auto gLeftInc = (c3.g - c1.g) / (surfaceEntry.src.h - 1);
    auto bLeftInc = (c3.b - c1.b) / (surfaceEntry.src.h - 1);
    auto rRightInc = (c4.r - c2.r) / (surfaceEntry.src.h - 1);
    auto gRightInc = (c4.g - c2.g) / (surfaceEntry.src.h - 1);
    auto bRightInc = (c4.b - c2.b) / (surfaceEntry.src.h - 1);

    for (int y = 0; y < surfaceEntry.src.h; y++) {
        int r = rLeft;
        int g = gLeft;
        int b = bLeft;
        auto rInc = (rRight - rLeft) / (surfaceEntry.src.w - 1);
        auto gInc = (gRight - gLeft) / (surfaceEntry.src.w - 1);
        auto bInc = (bRight - bLeft) / (surfaceEntry.src.w - 1);

        for (int x = 0; x < surfaceEntry.src.w; x++) {
            unsigned rDest = FIXED_TO_INT(r);
            unsigned gDest = FIXED_TO_INT(g);
            unsigned bDest = FIXED_TO_INT(b);

            p[y * surface->pitch / 4 + x] = (rDest << surface->format->Rshift) | (gDest << surface->format->Gshift) |
                (bDest << surface->format->Bshift) | (255 << surface->format->Ashift);

            r += rInc;
            g += gInc;
            b += bInc;
        }

        rLeft += rLeftInc;
        gLeft += gLeftInc;
        bLeft += bLeftInc;
        rRight += rRightInc;
        gRight += gRightInc;
        bRight += bRightInc;
    }

    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
}
