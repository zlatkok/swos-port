#pragma once

class SoundSample
{
public:
    SoundSample() {}
    SoundSample(const char *path, int chance = 1);
    SoundSample(const SoundSample& other);
    SoundSample(SoundSample&& other);
    ~SoundSample() { free(); }

    SoundSample& operator=(SoundSample&& other);
    SoundSample& operator=(const SoundSample& other) = delete;

    static SoundSample createNullSample();

    void free();
    bool loadFromFile(const char *path);

    Mix_Chunk *chunk();
    bool isChunkLoaded() const;
    bool hasData() const;
    int chanceModifier() const;
    void setChanceModifier(int chanceModifier);
    size_t hash() const;

    bool operator==(const SoundSample& other) const;

private:
    void assign(const SoundSample& other);
    void convertRawToWav();
    void loadChunk();
    std::tuple<char *, size_t, bool> loadWithAnyAudioExtension(const char *path, size_t baseNameLength, const char *ext);

    static size_t getAudioFileOffset(const char *ext, bool isRaw);
    static bool isRawExtension(const char *ext);
    static bool is11KhzSample(const char *name);
    static int getMaxAudioExensionLength();

    char *m_buffer = nullptr;
    unsigned m_size = 0;
    bool m_isRaw = false;
    bool m_is11Khz = false;
    Mix_Chunk *m_chunk = nullptr;
    unsigned m_hash = 0;
    int m_chanceModifier = 1;

    static const std::array<const char *, 5> kSupportedAudioExtensions;
};
