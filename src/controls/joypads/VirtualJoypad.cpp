#ifdef VIRTUAL_JOYPAD

#include "VirtualJoypad.h"
#include "JoypadConfig.h"
#include "windowManager.h"
#include "render.h"
#include "game.h"
#include "util.h"

constexpr Uint32 kShowTouchMarkDelay = 2'000;
constexpr int kMaxFingers = 5;

// width to cover the circle exactly: 2*height*(sqr(2-sqr(2))/sqr(2+sqr(2)))
constexpr int kPadSegmentWidth = 31;
constexpr int kPadSegmentHeight = 40;
constexpr int kPadSize = kPadSegmentHeight;

// leave a bit off the ends to account for bezel-less phones
constexpr int kSlack = 8;

constexpr int kFireButtonSize = 40;
constexpr int kFireButtonX = kVgaWidth - kFireButtonSize - kSlack - 10;
constexpr int kFireButtonY = kVgaHeight - kFireButtonSize - kSlack - 20;

constexpr int kPadCenterX = kPadSize + 10;
constexpr int kPadCenterY = kVgaHeight - kPadSize - 10;

struct TouchInfo {
    SDL_FingerID id = -1;
    Uint32 lastSeen = 0;
    float x = 0;
    float y = 0;
    SDL_Color color;
    GameControlEvents events = kNoGameEvents;
    int button;
    bool down = false;

    bool active() const { return id >= 0; }
};
static std::array<TouchInfo, kMaxFingers> m_touchInfo;

static const SDL_Color kTouchColors[] = {
    { 205, 180, 35, 255 },
    { 33, 210, 159, 255 },
    { 70, 114, 219, 255 },
    { 240, 74, 76, 255 },
};
static int m_currentColor;

void VirtualJoypad::setRenderer(SDL_Renderer *renderer)
{
    m_renderer = renderer;
}

void VirtualJoypad::setConfig(JoypadConfig *config)
{
    assert(config);
    m_config = config;
}

JoypadConfig *VirtualJoypad::config()
{
    return m_config;
}

bool VirtualJoypad::init()
{
    if (!m_texture)
        createTexture();

    return m_texture != nullptr;
}

void VirtualJoypad::setForceRender(bool force)
{
    logInfo("Virtual joypad rendering is %s", force ? "on" : "off");
    m_forceShow = force;
}

void VirtualJoypad::setPlayerNumber(int playerNumber)
{
    logInfo("Setting virtual joypad player number to %d", playerNumber);
    m_playerNumber = playerNumber;
}

void VirtualJoypad::enableTouchTrails(bool enableTouchTrails)
{
    m_showTouchTrails = enableTouchTrails;
}

void VirtualJoypad::enableTransparentButtons(bool transparentButtons)
{
    if (transparentButtons != m_transparentButtons || !m_texture) {
        m_transparentButtons = transparentButtons;
        createTexture();
    }
}

const char *VirtualJoypad::name() const
{
    return "Virtual Joypad";
}

bool VirtualJoypad::guidEqual(const SDL_JoystickGUID& guid) const
{
    return ::guidEqual(guid, kGuid);
}

SDL_JoystickID VirtualJoypad::id() const
{
    return INT_MAX;
}

bool VirtualJoypad::updateTouch(float x, float y, SDL_FingerID fingerId, Uint32 timestamp)
{
    auto it = std::find_if(m_touchInfo.begin(), m_touchInfo.end(), [fingerId](const auto& touch) {
        return touch.id == fingerId;
    });

    if (it == m_touchInfo.end()) {
        it = std::min_element(m_touchInfo.begin(), m_touchInfo.end(), [](const auto& touch1, const auto& touch2) {
            return touch1.lastSeen < touch2.lastSeen;
        });

        assert(it != m_touchInfo.end());
        it->color = kTouchColors[m_currentColor++ % std::size(kTouchColors)];
    }

    it->x = x;
    it->y = y;
    it->id = fingerId;
    it->lastSeen = timestamp;
    auto action = coordinatesToAction(x, y);
    it->button = action;
    it->down = true;
    it->events = joypadActionToEvents(action);

    return shouldRender() && action != kNoDirection;
}

void VirtualJoypad::removeTouch(SDL_FingerID fingerId)
{
    auto it = std::find_if(m_touchInfo.begin(), m_touchInfo.end(), [fingerId](const auto& touch) {
        return touch.id == fingerId;
    });
    assert(it != m_touchInfo.end() && std::count_if(m_touchInfo.begin(), m_touchInfo.end(),
        [fingerId](const auto& touch) { return touch.id == fingerId; }) == 1);

    if (it != m_touchInfo.end()) {
        it->events = kNoGameEvents;
        it->button = kNoDirection;
        it->down = false;
    } else {
        assert(false);
    }
}

void VirtualJoypad::render(SDL_Renderer *renderer)
{
    m_viewport = getViewport();

    if (m_showTouchTrails && !isMatchRunning())
        renderTouchPoints(renderer);

    toggleSpinningS();

    if (shouldRender())
        renderJoypadLayout(renderer);
}

bool VirtualJoypad::shouldRender() const
{
    if (m_forceShow)
        return true;

    if (!m_playerNumber || !isMatchRunning())
        return false;

    if (swos.topTeamData.playerNumber != m_playerNumber && swos.bottomTeamData.playerNumber != m_playerNumber)
        return false;

    return true;
}

GameControlEvents VirtualJoypad::events() const
{
    auto events = kNoGameEvents;

    for (auto& touch : m_touchInfo)
        if (touch.active() && touch.down)
            events |= touch.events;

    return events;
}

GameControlEvents VirtualJoypad::allEventsMask() const
{
    assert(m_config);
    return m_config->allEventsMask();
}

JoypadElementValueList VirtualJoypad::elementValues() const
{
    JoypadElementValueList result;

    auto events = this->events();
    int hatMask = 0;

    if (events & kGameEventUp)
        hatMask |= SDL_HAT_UP;
    if (events & kGameEventDown)
        hatMask |= SDL_HAT_DOWN;
    if (events & kGameEventLeft)
        hatMask |= SDL_HAT_LEFT;
    if (events & kGameEventRight)
        hatMask |= SDL_HAT_RIGHT;

    if (hatMask)
        result.emplace_back(JoypadElement::kHat, 0, hatMask);

    if (events & kGameEventKick)
        result.emplace_back(JoypadElement::kButton, 0, 1);

    return result;
}

int VirtualJoypad::numButtons() const
{
    return 1;
}
int VirtualJoypad::numHats() const
{
    return 1;
}
int VirtualJoypad::numAxes() const
{
    return 0;
}
int VirtualJoypad::numBalls() const
{
    return 0;
}

uint64_t VirtualJoypad::lastSelected() const
{
    return m_lastSelected;
}

void VirtualJoypad::updateLastSelected(uint64_t time)
{
    m_lastSelected = time;
}

void VirtualJoypad::createTexture()
{
    logInfo("Creating virtual joypad texture");

    if (m_texture)
        SDL_DestroyTexture(m_texture);

    auto surface = allocateSurface();
    if (!surface) {
        logWarn("Failed to create virtual joypad surface");
        return;
    }

    bool locked = SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) >= 0;

    auto pixels = reinterpret_cast<Uint32 *>(surface->pixels);
    auto pixel = SDL_MapRGBA(surface->format, 255, 255, 255, 255);
    int scale = m_padWidth / kPadSegmentWidth;
    int slack = 0;
    int frac = 0;
    unsigned alphaFrac = 255 << 16;
    unsigned alphaDec = (static_cast<uint64_t>(alphaFrac) << 16) / ((2 * surface->h << 16) / surface->w);

    for (int i = 0; i < surface->h; i++) {
        if (frac >= surface->h) {
            frac -= surface->h;
            alphaFrac += 255 << 16;
            slack++;
            assert(slack < surface->w);
        }

        memset(&pixels[i * surface->pitch / 4], 0, surface->pitch);
        if (m_transparentButtons) {
            for (int j = slack; j < surface->w - slack; j++)
                pixels[i * surface->pitch / 4 + j] = pixel;
        } else {
            int alpha = (alphaFrac >> 16) + ((alphaFrac >> 15) & 1);
            assert(alpha >= 0 && alpha <= 255);

            Uint32 alphaPixel = (pixel & 0xffffff) | (alpha << 24);
            auto setPixel = [&](int x, Uint32 pixel) {
                pixels[i * surface->pitch / 4 + slack + x] = pixel;
                pixels[i * surface->pitch / 4 + surface->w - slack - 1 - x] = pixel;
            };

            if (i < scale) {
                setPixel(0, alphaPixel);
                memset(&pixels[i * surface->pitch / 4 + slack + 1], pixel, (surface->w - slack - 2) * 4);
            } else {
                setPixel(0, alphaPixel);
                for (int i = 1; i < scale - 1; i++)
                    setPixel(i, pixel);
            }
        }

        frac += surface->w / 2;
        alphaFrac -= alphaDec;
    }

    if (locked)
        SDL_UnlockSurface(surface);

    SDL_SetColorKey(surface, 1, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    m_texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
}

SDL_Surface *VirtualJoypad::allocateSurface()
{
    int scaleFactor = 8;
    m_padWidth = kPadSegmentWidth * scaleFactor;
    m_padHeight = kPadSegmentHeight * scaleFactor;

    while (scaleFactor) {
        auto surface = SDL_CreateRGBSurface(0, m_padWidth, m_padHeight, 32, kRedMask, kGreenMask, kBlueMask, kAlphaMask);
        if (surface)
            return surface;
        logWarn("Failed to allocate %dx%d surface", m_padWidth, m_padHeight);
        m_padWidth /= 2;
        m_padHeight /= 2;
        scaleFactor /= 2;
    }

    logWarn("Failed to allocate surface for virtual joypad texture");
    return nullptr;
}

void VirtualJoypad::renderJoypadLayout(SDL_Renderer *renderer)
{
    if (!m_texture)
        return;

    static constexpr int kActiveColor[] = { 116, 26, 178, 111 };
    static constexpr int kInactiveColor[] = { 61, 13, 165, 110 };

    SDL_Rect destRect{ kPadCenterX - kPadSegmentWidth / 2, kPadCenterY - kPadSegmentHeight, kPadSegmentWidth, kPadSegmentHeight };
    SDL_Point p{ kPadSegmentWidth / 2, kPadSegmentHeight };
    double angle = 0;

    SDL_BlendMode oldBlendMode;
    if (m_transparentButtons) {
        SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    } else {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);

    bool activeSegments[kNumDirections] = {};
    bool fireActive = false;

    for (const auto& touch : m_touchInfo) {
        if (touch.active() && touch.button != kNoDirection) {
            if (touch.button == kFire) {
                fireActive = true;
            } else {
                assert(touch.button >= 0 && touch.button < kNumDirections);
                activeSegments[touch.button] = true;
            }
        }
    }

    for (int i = 0; i < kNumDirections; i++) {
        const auto& color = activeSegments[i] ? kActiveColor : kInactiveColor;
        SDL_SetTextureColorMod(m_texture, color[0], color[1], color[2]);
        if (m_transparentButtons)
            SDL_SetTextureAlphaMod(m_texture, color[3]);

        SDL_RenderCopyEx(renderer, m_texture, nullptr, &destRect, angle, &p, SDL_FLIP_NONE);
        angle += 360. / static_cast<int>(kNumDirections);
    }

    const auto& color = fireActive ? kActiveColor : kInactiveColor;
    SDL_SetRenderDrawColor(renderer, color[0], color[1], color[2], color[3]);

    SDL_Rect fireButtonRect{ kFireButtonX, kFireButtonY, kFireButtonSize, kFireButtonSize };
    if (m_transparentButtons)
        SDL_RenderFillRect(renderer, &fireButtonRect);
    else
        SDL_RenderDrawRect(renderer, &fireButtonRect);

    if (m_transparentButtons)
        SDL_SetRenderDrawBlendMode(renderer, oldBlendMode);
}

void VirtualJoypad::renderTouchPoints(SDL_Renderer *renderer)
{
    auto now = SDL_GetTicks();

    for (const auto& touch : m_touchInfo) {
        if (touch.lastSeen + kShowTouchMarkDelay >= now) {
            SDL_SetRenderDrawColor(renderer, touch.color.r, touch.color.g, touch.color.b, touch.color.a);
            int x, y;
            std::tie(x, y) = transformCoordinates(touch.x, touch.y);
            auto r = SDL_Rect{ x, y, 4, 4 };
            SDL_RenderDrawRect(renderer, &r);
        }
    }
}

void VirtualJoypad::toggleSpinningS()
{
    constexpr int kSpriteX = 285;
    constexpr int kSpriteY = 14;
    constexpr int kSpriteWidth = 32;
    constexpr int kSpriteHeight = 23;

    static bool s_holding;

    for (const auto& touch : m_touchInfo) {
        if (touch.active() && touch.down) {
            int x, y;
            std::tie(x, y) = transformCoordinates(touch.x, touch.y);

            if (x >= kSpriteX && x < kSpriteX + kSpriteWidth && y >= kSpriteY && y < kSpriteY + kSpriteHeight) {
                if (!s_holding)
                    swos.g_spinBigS ^= 1;

                s_holding = true;
                return;
            }
        }
    }

    s_holding = false;
}

std::pair<int, int> VirtualJoypad::transformCoordinates(float x, float y) const
{
    std::pair<int, int> result;
    result.first = static_cast<int>(x * m_viewport.w);
    result.second = static_cast<int>(y * m_viewport.h);
    return result;
}

GameControlEvents VirtualJoypad::joypadActionToEvents(JoypadAction action)
{
    switch (action) {
    case kBottomLeft:
        return kGameEventDown | kGameEventLeft;
    case kLeft:
        return kGameEventLeft;
    case kTopLeft:
        return kGameEventUp | kGameEventLeft;
    case kTop:
        return kGameEventUp;
    case kTopRight:
        return kGameEventUp | kGameEventRight;
    case kRight:
        return kGameEventRight;
    case kBottomRight:
        return kGameEventDown | kGameEventRight;
    case kBottom:
        return kGameEventDown;
    case kFire:
        return kGameEventKick;
    default:
        return kNoGameEvents;
    }
}

// lines:
//          (0) \/ (1)
// (3) left-top >< (2) right-top
//              /\
//
static bool leftOfLine(int line, int x, int y)
{
    switch (line) {
    case 0:
        if (x < 0) {
            if (y >= 0)
                return false;
            if (y*y >= 2 * x*x)
                return y*y*y*y - 8 * x*x * (y*y - x*x) >= 0;
            else
                return false;
        } else {
            if (y <= 0)
                return true;
            if (y*y >= 2 * x*x)
                return y*y*y*y - 8 * x*x * (y*y - x*x) <= 0;
            else
                return true;
        }
    case 1:
        if (x < 0) {
            if (y <= 0)
                return true;
            if (y*y >= 2 * x*x)
                return y*y*y*y - 8 * x*x * (y*y - x*x) <= 0;
            else
                return true;
        } else {
            if (y > 0)
                return false;
            if (y*y >= 2 * x*x)
                return y*y*y*y - 8 * x*x * (y*y - x*x) >= 0;
            else
                return false;
        }
    case 2:
        if (x < 0) {
            if (y <= 0)
                return true;
            if (x*x >= 2 * y*y)
                return x*x*x*x - 8 * y*y * (x*x - y*y) >= 0;
            else
                return false;
        } else {
            if (y > 0)
                return false;
            if (x*x >= 2 * y*y)
                return x*x*x*x - 8 * y*y * (x*x - y*y) <= 0;
            else
                return true;
        }
    case 3:
        if (x < 0) {
            if (y >= 0)
                return false;
            if (x*x >= 2 * y*y)
                return x*x*x*x - 8 * y*y * (x*x - y*y) <= 0;
            else
                return true;
        } else {
            if (y <= 0)
                return true;
            if (x*x >= 2 * y*y)
                return x*x*x*x - 8 * y*y * (x*x - y*y) >= 0;
            else
                return false;
        }
        break;
    }

    return assert(false), false;
}

auto VirtualJoypad::coordinatesToAction(float xNorm, float yNorm) const -> JoypadAction
{
    int x, y;
    std::tie(x, y) = transformCoordinates(xNorm, yNorm);

    if (x >= kFireButtonX && x < kFireButtonX + kFireButtonSize && y >= kFireButtonY && y < kFireButtonY + kFireButtonSize)
        return kFire;

    if (x < kPadCenterX - kPadSize || x >= kPadCenterX + kPadSize || y < kPadCenterY - kPadSize || y >= kPadCenterY + kPadSize)
        return kNoDirection;

    // place the origin at the center of the pad
    x -= kPadCenterX;
    y -= kPadCenterY;

    if (leftOfLine(1, x, y)) {
        if (leftOfLine(3, x, y))
            return leftOfLine(0, x, y) ? kTop : kTopLeft;
        else
            return leftOfLine(2, x, y) ? kLeft: kBottomLeft;
    } else {
        if (leftOfLine(3, x, y))
            return leftOfLine(2, x, y) ? kTopRight: kRight;
        else
            return leftOfLine(0, x, y) ? kBottomRight: kBottom;
    }
}

#endif
