#include "util.h"

static PoolPlayer *findPoolPlayer(word teamId, byte playerOrdinal)
{
    auto begin = reinterpret_cast<PoolPlayer *>(swos.poolplyrDatBuffer);
    auto end = begin + kNumPoolPlayers;
    auto player = std::find_if(begin, end, [=](const PoolPlayer& candidate) {
        return candidate.teamId == teamId && candidate.playerOrdinal == playerOrdinal;
    });

    return player != end ? player : nullptr;
}

// Original SWOS starts this search one record before POOLPLYR.DAT and advances
// before comparing. That relies on the preceding variable remaining exactly
// 58 bytes away, which is not true after unused VM data has been removed.
void SWOS::cseg_5A666()
{
    auto player = findPoolPlayer(D0.asWord(), D1.as<byte>());
    if (!player)
        errorExit("Foreign-market player %u/%u is missing from POOLPLYR.DAT", D0.asWord(), D1.as<byte>());

    A0 = player;
}
