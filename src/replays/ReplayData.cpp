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
    bool result = false;

    auto path = joinPaths(kReplaysDir, filename);
    auto f = openFile(path.c_str());
    if (!f)
        return false;

    if (fseek(f, 0, SEEK_END) == 0) {
        auto size = ftell(f);
        int remainingSize = size - kHilHeaderSize;

        if (remainingSize > 0) {
            m_replayData.resize(remainingSize);

            bool readOk = fseek(f, 0, SEEK_SET) == 0 && fread(m_header, kHilHeaderSize, 1, f) == 1 &&
                fread(m_replayData.data(), remainingSize, 1, f) == 1;

            if (readOk) {
                m_offset = 0;
                result = true;
            }
        }
    }

    fclose(f);
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

    bool result = fwrite(m_header, kHilHeaderSize, 1, f) == 1 &&
        fwrite(m_replayData.data(), m_replayData.size(), 1, f) == 1;

    fclose(f);

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
