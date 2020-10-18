#include "ReplayData.h"
#include "file.h"
#include "util.h"

constexpr int kInitialReplayCapacity = 1'500'000;
constexpr int kNumScenesOffset = 3'616;

ReplayData::ReplayData()
{
    m_replayData.reserve(kInitialReplayCapacity);
}

void ReplayData::startNewReplay()
{
    m_replayData.clear();
}

void ReplayData::finishCurrentReplay()
{
    fetchHilHeader();
    fetchHighlightsScene(swos.currentHilPtr + 1);
}

void ReplayData::handleHighglightsBufferOverflow(dword *endPtr, bool playing)
{
    assert(endPtr >= swos.nextGoalPtr);

    if (playing) {
        assert(m_offset <= m_replayData.size());
        copyChunk(swos.goalBasePtr);
    } else {
        fetchHighlightsScene(endPtr);
    }
}

void ReplayData::replayStarting()
{
    m_offset = 0;

    memcpy(swos.hilFileBuffer, m_header, sizeof(m_header));
    copyChunk(swos.hilFileBuffer + kHilHeaderSize);

    // endianess alert
    assert(m_header[kNumScenesOffset] == 1 && m_header[kNumScenesOffset + 1] == 0);
}

bool ReplayData::gotReplay() const
{
    return !m_replayData.empty();
}

bool ReplayData::load(const char *filename)
{
    auto path = joinPaths(kReplaysDir, filename);
    auto f = openFile(path.c_str());
    if (!f)
        return false;

    bool result = false;
    auto size = SDL_RWsize(f);

    if (size >= 0) {
        int remainingSize = static_cast<int>(size) - kHilHeaderSize;

        if (remainingSize > 0) {
            m_replayData.resize(remainingSize);

            bool readOk = SDL_RWread(f, m_header, kHilHeaderSize, 1) == 1 &&
                SDL_RWread(f, m_replayData.data(), remainingSize, 1) == 1;

            if (readOk) {
                m_offset = 0;
                result = true;
            }
        }
    }

    SDL_RWclose(f);
    return result;
}

bool ReplayData::save(const char *filename, bool overwrite)
{
    auto mode = overwrite ? "wb" : "ab";

    auto path = joinPaths(kReplaysDir, filename);
    auto f = openFile(path.c_str(), mode);
    if (!f)
        return false;

    ensureEndSegmentMarker();

    bool result = SDL_RWwrite(f, m_header, kHilHeaderSize, 1) == 1 &&
        SDL_RWwrite(f, m_replayData.data(), m_replayData.size(), 1) == 1;

    SDL_RWclose(f);

    if (!result)
        logWarn("Failed to save replay file %s", path.c_str());

    return result;
}

void ReplayData::fetchHilHeader()
{
    memcpy(m_header, swos.hilFileBuffer, kHilHeaderSize);

    // endianess alert
    m_header[kNumScenesOffset] = 1;
    m_header[kNumScenesOffset + 1] = 0;
}

void ReplayData::fetchHighlightsScene(dword *endPtr)
{
    assert(endPtr >= swos.goalBasePtr && endPtr <= swos.nextGoalPtr);

    auto bytesLeft = (endPtr - swos.goalBasePtr) * sizeof(dword);
    auto oldSize = m_replayData.size();
    m_replayData.resize(oldSize + bytesLeft);
    memcpy(rawData() + oldSize, swos.goalBasePtr, bytesLeft);
}

void ReplayData::ensureEndSegmentMarker()
{
    auto oldSize = m_replayData.size();
    auto last = oldSize / sizeof(dword) - 1;

    if (m_replayData[last] != -1) {
        m_replayData.resize(oldSize + sizeof(dword));
        m_replayData[last + 1] = -1;
    }
}

void ReplayData::copyChunk(void *dest)
{
    auto chunkSize = std::min<size_t>(m_replayData.size() - m_offset, kSingleHighlightBufferSize);
    memcpy(dest, rawData() + m_offset, chunkSize);
    m_offset += chunkSize;
}

char *ReplayData::rawData()
{
    return reinterpret_cast<char *>(m_replayData.data());
}
