#include <ctype.h>
#include <string.h>
#include "animation.h"
#include "swspr.h"
#include "printstr.h"
#include "draw.h"
#include "util.h"

extern Sprites_info s;
extern unsigned char pal[768];

typedef struct _AnimationData {
    const char *name;
    int frameDelay;
    const short *directions[8];
    int durations[8];
    uchar loops[8];
} AnimationData;

#include "animation_data.inc"

static const char animationHelp[] =
    "left/right\t\t\t- \rprevious/next\nanimation\n"
    "up/down\t\t\t- change direction\n"
    "space\t\t\t\t- restart animation\n"
    "p\t\t\t\t\t- pause/unpause\n"
    "control + left/right\t- previous/next frame\n"
    "shift (hold)\t\t\t- slow motion\n"
    "0..9\t\t\t\t- \rchange background\ncolor\n"
    "+/-\t\t\t\t- zoom/unzoom\n";

#define AUTO_RESTART_TICKS (3 * 70)
#define RESTART_NOTICE_TICKS (2 * 70)
#define SLOW_MOTION_FACTOR 2
#define MANUAL_RESTART_NOTICE_TICKS 70

static int animationIndex;
static int direction;
static int frameIndex;
static int frameDelay;
static int frameTimer;
static int spriteIndex;
static int displayedFrameIndex;
static int stoppedTicks;
static int restartNoticeTicks;
static int slowMotion;
static int slowMotionTick;
static int paused;
static int elapsedTicks;
static schar zoomFactor = 1;
static uchar backgroundColor = BLUE;

static const char *const directionNames[] = {
    "TOP", "TOP-RIGHT", "RIGHT", "BOTTOM-RIGHT",
    "BOTTOM", "BOTTOM-LEFT", "LEFT", "TOP-LEFT"
};

static const short *CurrentFrames()
{
    return animationData[animationIndex].directions[direction];
}

static void SelectAvailableDirection()
{
    int offset;
    if (CurrentFrames())
        return;
    for (offset = 1; offset < 8; offset++) {
        int candidate = (direction + offset) & 7;
        if (animationData[animationIndex].directions[candidate]) {
            direction = candidate;
            return;
        }
    }
}

static void ResetAnimation()
{
    const short *frames = CurrentFrames();
    int commandsLeft = 1024;
    frameIndex = 0;
    frameDelay = animationData[animationIndex].frameDelay;
    frameTimer = 1;
    spriteIndex = -1;
    displayedFrameIndex = -1;
    stoppedTicks = 0;
    restartNoticeTicks = 0;
    slowMotionTick = 0;
    elapsedTicks = 0;
    while (frames && commandsLeft--) {
        int frame = frames[frameIndex];
        if (frame >= 0) {
            spriteIndex = frame;
            displayedFrameIndex = frameIndex++;
            frameTimer = max(frameDelay, 1);
            break;
        }
        if (frame == -999)
            frameIndex = 0;
        else if (frame == -101) {
            stoppedTicks = 1;
            break;
        } else if (frame <= -100)
            frameIndex += frame + 100;
        else {
            frameDelay = -frame;
            frameIndex++;
        }
    }
}

static void AnimationTick()
{
    const short *frames;
    int frame;

    if (g.mode != MODE_ANIMATION)
        return;
    if (paused || isControlDown())
        return;
    if (restartNoticeTicks && !--restartNoticeTicks)
        UpdateScreen();
    if (stoppedTicks) {
        if (++stoppedTicks >= AUTO_RESTART_TICKS) {
            ResetAnimation();
        } else {
            if (stoppedTicks == RESTART_NOTICE_TICKS)
                UpdateScreen();
            return;
        }
    }
    if (slowMotion && ++slowMotionTick < SLOW_MOTION_FACTOR)
        return;
    slowMotionTick = 0;
    elapsedTicks++;
    if (--frameTimer > 0)
        return;
    frames = CurrentFrames();
    if (!frames)
        return;
    for (;;) {
        frame = frames[frameIndex];
        if (frame >= 0) {
            spriteIndex = frame;
            displayedFrameIndex = frameIndex;
            frameIndex++;
            frameTimer = max(frameDelay, 1);
            break;
        }
        if (frame == -999)
            frameIndex = 0;
        else if (frame == -101) {
            frameTimer = max(frameDelay, 1);
            stoppedTicks = 1;
            break;
        } else if (frame <= -100)
            frameIndex += frame + 100;
        else {
            frameDelay = -frame;
            frameTimer = -frame;
            frameIndex++;
        }
    }
    UpdateScreen();
}

static void StepAnimationFrame(int step)
{
    const short *frames = CurrentFrames();
    int i;
    if (!frames)
        return;
    stoppedTicks = 0;
    restartNoticeTicks = 0;
    if (step < 0) {
        for (i = displayedFrameIndex - 1; i >= 0; i--) {
            if (frames[i] >= 0) {
                displayedFrameIndex = i;
                spriteIndex = frames[i];
                frameIndex = i + 1;
                frameTimer = max(frameDelay, 1);
                elapsedTicks = max(0, elapsedTicks - frameDelay);
                UpdateScreen();
                return;
            }
        }
        return;
    }
    frameTimer = 1;
    /* Ctrl itself pauses normal playback; temporarily step through one frame. */
    for (;;) {
        int frame = frames[frameIndex];
        if (frame >= 0) {
            spriteIndex = frame;
            displayedFrameIndex = frameIndex++;
            frameTimer = max(frameDelay, 1);
            elapsedTicks += frameDelay;
            break;
        }
        if (frame == -999)
            frameIndex = 0;
        else if (frame == -101)
            frameIndex = 0;
        else if (frame <= -100)
            frameIndex += frame + 100;
        else {
            frameDelay = -frame;
            frameIndex++;
        }
    }
    UpdateScreen();
}

static bool InitAnimation()
{
    return TRUE;
}

static void InitAnimationMode(uint oldMode)
{
    SetPalette(pal, &g.pbits, TRUE);
    ResetAnimation();
    RegisterSWOSProc(AnimationTick);
    AnimationTick();
}

static void FinishAnimationMode()
{
    RegisterSWOSProc(NULL);
}

static bool AnimationKeyProc(uint code, uint data)
{
    switch (code) {
    case '0': backgroundColor = BLACK;       break;
    case '1': backgroundColor = WHITE;       break;
    case '2': backgroundColor = LIGHT_GRAY;  break;
    case '3': backgroundColor = LIGHT_BROWN; break;
    case '4': backgroundColor = BROWN;       break;
    case '5': backgroundColor = YELLOW;      break;
    case '6': backgroundColor = PITCH_GREEN; break;
    case '7': backgroundColor = BRIGHT_BLUE; break;
    case '8': backgroundColor = BLUE;        break;
    case '9': backgroundColor = DARK_BLUE;   break;
    case VK_LEFT:
        if (isControlDown()) {
            StepAnimationFrame(-1);
            break;
        }
        animationIndex = (animationIndex + NUM_ANIMATIONS - 1) % NUM_ANIMATIONS;
        SelectAvailableDirection();
        ResetAnimation();
        AnimationTick();
        break;
    case VK_RIGHT:
        if (isControlDown()) {
            StepAnimationFrame(1);
            break;
        }
        animationIndex = (animationIndex + 1) % NUM_ANIMATIONS;
        SelectAvailableDirection();
        ResetAnimation();
        AnimationTick();
        break;
    case VK_UP:
        direction = (direction + 7) & 7;
        ResetAnimation();
        AnimationTick();
        break;
    case VK_DOWN:
        direction = (direction + 1) & 7;
        ResetAnimation();
        AnimationTick();
        break;
    case VK_SPACE:
        ResetAnimation();
        restartNoticeTicks = MANUAL_RESTART_NOTICE_TICKS;
        AnimationTick();
        break;
    case 'P':
        paused ^= 1;
        break;
    case VK_CONTROL:
        break;
    case VK_SHIFT:
        slowMotion = TRUE;
        slowMotionTick = 0;
        UpdateScreen();
        break;
    case VK_ADD:
        zoomFactor = min(zoomFactor + 1, MAX_ZOOM);
        break;
    case VK_SUBTRACT:
        zoomFactor = max(zoomFactor - 1, 1);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

static bool AnimationKeyUpProc(uint code, uint data)
{
    if (code == VK_SHIFT) {
        slowMotion = FALSE;
        slowMotionTick = 0;
        UpdateScreen();
        return TRUE;
    }
    if (code == VK_CONTROL)
        return TRUE;
    return FALSE;
}

static void FormatAnimationName(char *output, const char *input)
{
    size_t length = strlen(input);
    int previousLower = FALSE;
    int previousDigit = FALSE;
    if (length >= 9 && !strcmp(input + length - 9, "AnimTable"))
        length -= 9;
    while (length--) {
        int upper = isupper((uchar)*input);
        int digit = isdigit((uchar)*input);
        if ((upper && (previousLower || previousDigit)) || (digit && previousLower))
            *output++ = ' ';
        *output++ = (char)toupper((uchar)*input);
        previousLower = islower((uchar)*input);
        previousDigit = digit;
        input++;
    }
    *output = '\0';
}

static byte *AnimationDraw(byte *pbits, const uint pitch)
{
    char name[160], animationText[40], timeText[40], spriteText[40], missingText[80];
    int duration, displayTicks, currentSeconds, currentHundredths;
    int totalSeconds, totalHundredths;
    int i;
    for (i = 0; i < HEIGHT; i++)
        memset(pbits + i * pitch, backgroundColor, WIDTH);
    if (spriteIndex >= 0 && spriteIndex < NUM_SPRITES) {
        Sprite *sprite = &s.sprites[spriteIndex];
        DrawSprite((WIDTH - sprite->width) / 2, (HEIGHT - sprite->nlines) / 2,
            pbits, sprite, pitch, -1);
    }
    Zoom(pbits, zoomFactor, pitch);
    FormatAnimationName(name, animationData[animationIndex].name);
    wsprintf(name + strlen(name), " (%s)", directionNames[direction]);
    PrintString(name, 0, 0, pbits, pitch, TRUE, -1, ALIGN_UPLEFT);
    wsprintf(animationText, "ANIMATION %d/%d", animationIndex + 1, NUM_ANIMATIONS);
    PrintString(animationText, 0, 10, pbits, pitch, FALSE, -1, ALIGN_LEFT);
    duration = animationData[animationIndex].durations[direction];
    displayTicks = max(elapsedTicks, 0);
    if (duration > 0 && animationData[animationIndex].loops[direction])
        displayTicks %= duration;
    else if (duration > 0)
        displayTicks = min(displayTicks, duration);
    currentSeconds = displayTicks / 70;
    currentHundredths = displayTicks % 70 * 100 / 70;
    totalSeconds = duration / 70;
    totalHundredths = duration % 70 * 100 / 70;
    wsprintf(timeText, "%02d:%02d/%02d:%02d", currentSeconds, currentHundredths, totalSeconds, totalHundredths);
    PrintString(timeText, 0, 17, pbits, pitch, FALSE, -1, ALIGN_LEFT);
    if (spriteIndex >= 0) {
        wsprintf(spriteText, "SPRITE %d", spriteIndex);
        PrintString(spriteText, 0, 24, pbits, pitch, FALSE, -1, ALIGN_LEFT);
    }
    if (!CurrentFrames()) {
        wsprintf(missingText, "NO %s DIRECTION ANIMATION", directionNames[direction]);
        PrintString(missingText, 0, 0, pbits, pitch, FALSE, -1, ALIGN_CENTERX | ALIGN_CENTERY);
    }
    if (paused || isControlDown())
        PrintString("PAUSED", 0, HEIGHT - 42, pbits, pitch, FALSE, -1, ALIGN_CENTERX);
    if (slowMotion)
        PrintString("SLOW-MO", 0, HEIGHT - 24, pbits, pitch, FALSE, -1, ALIGN_CENTERX);
    else if (restartNoticeTicks || stoppedTicks >= RESTART_NOTICE_TICKS)
        PrintString("RESTARTING", 0, HEIGHT - 24, pbits, pitch, FALSE, -1, ALIGN_CENTERX);
    return pbits;
}

static byte *AnimationGetPalette()
{
    return pal;
}

const Mode AnimationMode = {
    InitAnimation, InitAnimationMode, FinishAnimationMode, AnimationKeyProc,
    AnimationKeyUpProc, AnimationDraw, AnimationGetPalette, (char *)animationHelp,
    "ANIMATION MODE", VK_F10, 0
};
