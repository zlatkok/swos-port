#ifdef VIRTUAL_JOYPAD

#pragma once

#include "gameControlEvents.h"
#include "JoypadElementValue.h"

class JoypadConfig;

class VirtualJoypad
{
public:
    static constexpr SDL_JoystickGUID kGuid{ 'z', 'k', 'z', 0x50, 0xcc, 0xe4, 0x44, 0xb2, 0xb4, 0xba, 0x11, 0xc0, 0x07, 'z', 'k', 'z' };

    void setRenderer(SDL_Renderer *renderer);
    void setConfig(JoypadConfig *config);
    JoypadConfig *config();

    bool init();
    void setForceRender(bool force);
    void setPlayerNumber(int playerNumber);
    bool touchTrailsEnabled() const;
    void enableTouchTrails(bool touchTrails);
    bool transparentButtonsEnabled() const;
    void enableTransparentButtons(bool transparentButtons);

    const char *name() const;
    bool guidEqual(const SDL_JoystickGUID& guid) const;
    SDL_JoystickID id() const;

    bool updateTouch(float x, float y, SDL_FingerID fingerId, Uint32 timestamp);
    void removeTouch(SDL_FingerID fingerId);
    void render(SDL_Renderer *renderer);
    bool shouldRender() const;

    GameControlEvents events() const;
    GameControlEvents allEventsMask() const;
    JoypadElementValueList elementValues() const;
    bool anyUnassignedButtonDown() const { return false; }
    bool anyButtonDown() const { return false; }

    int numButtons() const;
    int numHats() const;
    int numAxes() const;
    int numBalls() const;

    uint64_t lastSelected() const;
    void updateLastSelected(uint64_t time);

private:
    enum JoypadAction
    {
        // keep it in clockwise direction starting with top
        kTop,
        kTopRight,
        kRight,
        kBottomRight,
        kBottom,
        kBottomLeft,
        kLeft,
        kTopLeft,
        kNumDirections,
        kFire,
        kNoDirection = -1,
    };

    void createTexture();
    SDL_Surface *allocateSurface();
    void renderJoypadLayout(SDL_Renderer *renderer);
    void renderTouchPoints(SDL_Renderer *renderer);
    void checkIfTogglingSpinningS();

    std::pair<int, int> transformCoordinates(float x, float y) const;
    JoypadAction coordinatesToAction(float xNorm, float yNorm) const;
    static GameControlEvents joypadActionToEvents(JoypadAction action);

    uint64_t m_lastSelected = 0;
    JoypadConfig *m_config = nullptr;
    SDL_Renderer *m_renderer = nullptr;
    int m_playerNumber = 0;

    SDL_Texture *m_texture = nullptr;
    int m_padWidth;
    int m_padHeight;

    int m_hatValue = 0;
    bool m_fire = false;

    SDL_Rect m_viewport;
    bool m_showTouchTrails = false;
    bool m_transparentButtons = true;

    bool m_forceShow = false;
};

#endif
