// This class holds data gathered during the game and required for replays
// (sprite coordinates, camera coordinates...)

#pragma once

class ReplayData
{
public:
    static constexpr char kReplaysDir[8] = { "replays" };

    ReplayData();

    void startNewReplay();
    void finishCurrentReplay();
    void handleHighglightsBufferOverflow(dword *endPtr, bool playing);
    void replayStarting();

    bool gotReplay() const;

    bool load(const char *filename);
    bool save(const char *filename, bool overwrite);

private:
    void fetchHilHeader();
    void fetchHighlightsScene(dword *endPtr);
    void ensureEndSegmentMarker();
    void copyChunk(void *dest);
    char *rawData();

#pragma pack(push, 1)
    struct RawDword {
        RawDword() {}
        RawDword(dword data) : data(data) {}
        RawDword(const RawDword&) {}  // keep it uninitialized when resizing the vector
        RawDword& operator=(const RawDword&) = default;
        bool operator!=(int i) const { return data != i; }

        dword data;
    };
#pragma pack(pop)
    static_assert(sizeof(RawDword) == sizeof(dword), "Life is full of obstacles");

    // make sure not to use push_back()
    std::vector<RawDword> m_replayData;

    char m_header[kHilHeaderSize];
    size_t m_offset = 0;
};
