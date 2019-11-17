#include "file.h"
#include "render.h"

// in:
//     D0 - pitch file number(0 - based)
//
void SWOS::ReadPitchFile()
{
    assert(D0 < 6);

    logInfo("Loading pitch file %d", D0.data);

    // this runs right after virtual screen for menus is trashed; restore it so it can fade-out properly
    // also note that screen pitch is changed by this point
    memcpy(linAdr384k, linAdr384k + 2 * kVgaScreenSize, kVgaScreenSize);
    for (int i = 0; i < kVgaHeight; i++)
        memcpy(linAdr384k + i * kVgaWidth, linAdr384k + 2 * kVgaScreenSize + i * kVgaWidth, kVgaWidth);

    memset(g_pitchDatBuffer, 0, sizeof(g_pitchDatBuffer));
    memset(g_currentMenu, 0, skillsPerQuadrant - g_currentMenu);

    auto pitchFilenamesTable = &ofsPitch1Dat;
    auto pitchFilename = pitchFilenamesTable[D0];
    auto buffer = g_pitchDatBuffer + 180;

    if (auto file = openFile(pitchFilename)) {
        for (int i = 0; i < 55; i++) {
            if (!fread(buffer, 168, 1, file))
                logWarn("Failed reading pitch file %s, line %d", pitchFilename, i);
            buffer += 176;
        }
        fclose(file);
    }

    auto blkFilename = aPitch1_blk + 16 * D0;
    loadFile(blkFilename, g_pitchPatterns);
}

// in:
//      D1 - x index in g_pitchDatBuffer
//      D2 - y         - || -
//      D3 - x screen coordinate / 16
//      D4 - y      - || -       / 16
//
void SWOS::DrawBackPattern()
{
    auto xTile = D1.as<int16_t>();
    auto yTile = D2.as<int16_t>();
    auto xScreenTile = D3.as<int16_t>();
    auto yScreenTile = D4.as<int16_t>();

    constexpr int kMaxTileX = 43;
    constexpr int kMaxTileY = 56;

    // do the clipping
    if (xTile < 0 || xTile > kMaxTileX || yTile < 0 || yTile > kMaxTileY ||
        xScreenTile < 0 || xScreenTile > kMaxTileX || yScreenTile < 0 || yScreenTile > kMaxTileY)
        return;

    auto base = reinterpret_cast<char **>(g_pitchDatBuffer);
    auto src = base[44 * yTile + xTile];
    auto dst = linAdr384k + 16 * (kGameScreenWidth * yScreenTile + xScreenTile);

    for (int i = 0; i < 16; i++) {
        memcpy(dst, src, 16);
        dst += kGameScreenWidth;
        src += 16;
    }
}

void SWOS::ResetAnimatedPatternsForBothTeams()
{
    memset(animatedPatterns, -1, sizeof(animatedPatterns));
}
