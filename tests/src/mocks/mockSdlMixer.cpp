#include "mockSdlMixer.h"

static std::array<Mix_Chunk *, MIX_CHANNELS> m_channels;
static int m_playChunkTimesCalled = 0;
static std::vector<Mix_Chunk *> m_lastPlayedChunks;

void resetMockSdlMixer()
{
    m_channels = {};
    m_playChunkTimesCalled = 0;
    m_lastPlayedChunks.clear();
}

int numTimesPlayChunkCalled()
{
    return m_playChunkTimesCalled;
}

const std::vector<Mix_Chunk *>& getLastPlayedChunks()
{
    return m_lastPlayedChunks;
}

Mix_Chunk *getLastPlayedChunk()
{
    return m_lastPlayedChunks.empty() ? nullptr : m_lastPlayedChunks.back();
}

int numberOfChannelsPlaying()
{
    return std::count_if(m_channels.begin(), m_channels.end(), [](const auto chunk) { return chunk != nullptr; });
}

int Mix_HaltChannel(int channel)
{
    if (channel >= 0 && channel < MIX_CHANNELS)
        m_channels[channel] = nullptr;
    else if (channel == -1)
        m_channels = {};

    return 1;
}

int Mix_VolumeChunk(Mix_Chunk *, int)
{
    return 0;
}

int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks)
{
    m_playChunkTimesCalled++;

    if (channel == -1) {
        auto it = std::find_if(m_channels.begin(), m_channels.end(), [](const auto chunk) {
            return chunk == nullptr;
        });
        if (it != m_channels.end())
            channel = it - m_channels.begin();
    }

    if (channel < 0 || channel >= static_cast<int>(m_channels.size())) {
        assert(false);
        return -1;
    }

    m_channels[channel] = chunk;
    m_lastPlayedChunks.push_back(chunk);

    return channel;
}

int Mix_Playing(int channel)
{
    if (channel >= 0 && channel < static_cast<int>(m_channels.size())) {
        return m_channels[channel] != nullptr;
    } else if (channel == -1) {
        return std::count_if(std::begin(m_channels), std::end(m_channels), [](const auto chunk) {
            return chunk != nullptr;
        });
    }

    return 0;
}

// I'd rather not mock this, but can't have anything from SDL Mixer linked in
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int freeSrc)
{
    assert(src);

    auto chunk = new Mix_Chunk;

    chunk->allocated = 1;
    chunk->volume = MIX_MAX_VOLUME;
    chunk->alen = static_cast<int>(SDL_RWsize(src));
    chunk->abuf = new Uint8[chunk->alen];
    SDL_RWread(src, chunk->abuf, chunk->alen, 1);

    if (freeSrc)
        SDL_RWclose(src);

    return chunk;
}

void Mix_FreeChunk(Mix_Chunk *chunk)
{
    if (chunk) {
        delete[] chunk->abuf;
        chunk->abuf = nullptr;

        if (chunk->allocated)
            delete chunk;
    }
}
