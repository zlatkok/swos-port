#include "audio.h"
#include "music.h"
#include "util.h"
#include "log.h"
#include "file.h"
#include "swos.h"
#include "audioOptions.mnu.h"
#include <dirent.h>

constexpr int kMaxVolume = MIX_MAX_VOLUME;
constexpr int kMinVolume = 0;

constexpr int kChantsVolume = 55;

constexpr char *kSentinel = reinterpret_cast<char *>(-1);

static int16_t m_volume = 100;                      // master sound volume
static std::atomic<int16_t> m_musicVolume = 100;    // atomic since ADL thread will need access to it

static int m_actualFrequency;
static int m_actualChannels;

static bool m_hasAudioDir;

static const std::array<const char *, 5> kAudioExtensions = { "raw", "mp3", "wav", "ogg", "flac" };

enum SfxSampleIndex {
    kBackgroundCrowd, kBounce, kHomeGoal, kKick, kWhistle, kMissGoal, kEndGameWhistle, kFoulWhistle, kChant4l, kChant10l, kChant8l,
    kNumSoundEffects,
};

static Mix_Chunk *loadRaw(char *buffer, size_t size, bool is11KhzSample);

struct SampleData {
    char *buffer = nullptr;
    size_t size = 0;
    bool isRaw = false;
    bool is11Khz = false;
    Mix_Chunk *chunk = nullptr;
    size_t hash;

    SampleData() {}
    SampleData(char *buffer, size_t size, bool isRaw, bool is11Khz) : buffer(buffer), size(size), isRaw(isRaw), is11Khz(is11Khz) {
        assert(buffer);
    }
    void free() {
        Mix_FreeChunk(chunk);
        chunk = nullptr;

        delete[] buffer;
        buffer = nullptr;
    }
    Mix_Chunk *getChunk() {
        if (!chunk) {
            if (isRaw) {
                chunk = loadRaw(buffer, size, is11Khz);
            } else {
                auto rwOps = SDL_RWFromMem(buffer, size);
                if (rwOps)
                    chunk = Mix_LoadWAV_RW(rwOps, 0);
            }

            if (chunk)
                hash = ::hash(chunk->abuf, chunk->alen);
        }

        assert(chunk);
        return chunk;
    }
};

static std::array<SampleData, 76 + 52> m_originalCommentarySamples;
static std::array<SampleData, kNumSoundEffects> m_sfxSamples;

static int m_refereeWhistleSampleChannel = -1;
static int m_commentaryChannel = -1;
static int m_chantsChannel = -1;

static Mix_Chunk *m_chantsSample;
static SampleData m_resultSample;
static int m_lastResultSampleIndex = -1;

static SampleData m_introChantSample;
static SampleData m_endGameCrowdSample;

static int m_fadeInChantTimer;
static int m_fadeOutChantTimer;
static int m_nextChantTimer;

static void (*m_playCrowdChantsFunction)();
static bool m_interestingGame;

static Mix_Chunk *m_lastPlayedComment;
static size_t m_lastPlayedCommentHash;
static int m_lastPlayedCategory;

#pragma pack(push, 1)
struct WaveFileHeader {
    dword id;
    dword size;
    dword format;
};

struct WaveFormatHeader {
    dword id;
    dword size;
    word format;
    word channels;
    dword frequency;
    dword byteRate;
    word sampleSize;
    word bitsPerSample;
};

struct WaveDataHeader {
    dword id;
    dword length;
};
#pragma pack(pop)

constexpr int kSizeofWaveHeader = sizeof(WaveFileHeader) + sizeof(WaveFormatHeader) + sizeof(WaveDataHeader);

//
// sample loading
//

struct Sample {
    SampleData sampleData;
    int frequency;

    Sample(const SampleData& sampleData, int frequency = 1) : sampleData(sampleData), frequency(frequency) {}

    Sample(char *buffer, size_t size, bool isRaw, bool is11Khz, int frequency = 1)
    : sampleData(buffer, size, isRaw, is11Khz), frequency(frequency) {}

    bool operator==(const Sample& other) const {
        return sampleData.chunk == other.sampleData.chunk;
    }
};

struct SampleTable {
    const char *dir;
    std::vector<Sample> samples;
    int lastPlayedIndex = -1;

    SampleTable(const char *dir) : dir(dir) {}
};

// these must match exactly with directories in the sample tables below
enum CommentarySampleTableIndex {
    kCorner, kDirtyTackle, kEndGameRout, kEndGameSensational, kEndGameSoClose, kFoul, kGoal, kGoodPass, kGoodTackle,
    kHeader, kHitBar, kHitPost, kInjury, kKeeperClaimed, kKeeperSaved, kNearMiss, kOwnGoal, kPenalty, kPenaltyMissed,
    kPenaltySaved, kPenaltyGoal, kRedCard, kSubstitution, kChangeTactics, kThrowIn, kYellowCard,
    kNumSampleTables,
};

// all the comments heard in the game
static std::array<SampleTable, kNumSampleTables> m_sampleTables = {{
    { "corner" },
    { "dirty_tackle" },
    { "end_game_rout" },
    { "end_game_sensational" },
    { "end_game_so_close" },
    { "free_kick" },
    { "goal" },
    { "good_play" },
    { "good_tackle" },
    { "header" },
    { "hit_bar" },
    { "hit_post" },
    { "injury" },
    { "keeper_claimed" },
    { "keeper_saved" },
    { "near_miss" },
    { "own_goal" },
    { "penalty" },
    { "penalty_missed" },
    { "penalty_saved" },
    { "penalty_scored" },
    { "red_card" },
    { "substitution" },
    { "tactic_changed" },
    { "throw_in" },
    { "yellow_card" },
}};

static int getSampleFrequency(const char *str, size_t len)
{
    assert(str);
    assert(len == strlen(str));

    int frequency = 1;

    if (len >= 4) {
        auto end = str + len - 1;

        for (auto p = end; p >= str; p--) {
            if (*p == '.') {
                end = p - 1;
                break;
            }
        }

        if (end - str >= 4 && isdigit(*end)) {
            int value = 0;
            int power = 1;
            auto digit = end;

            while (digit >= str && isdigit(*digit)) {
                value += power * (*digit - '0');
                power *= 10;
                digit--;
            }

            if (digit >= str + 2 && *digit == 'x' && (digit[-1] == '_' || digit[-1] == '-'))
                frequency = value;
        }
    }

    return frequency;
}

static size_t getBasenameLength(const char *filename, size_t len)
{
    assert(filename);

    for (int i = static_cast<int>(len) - 1; i >= 0; i--)
        if (filename[i] == '.')
            return i ? i - 1 : 0;

    return len;
}

static Mix_Chunk *loadRaw(char *buffer, size_t size, bool is11KhzSample)
{
    static const char kWaveHeader[] = {
        'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',                                 // file header
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 8, 0,    // format header
        'd', 'a', 't', 'a', 0, 0, 0, 0,                                                     // data header
    };
    static_assert(sizeof(kWaveHeader) == kSizeofWaveHeader, "Wave header format invalid");

    memcpy(buffer, kWaveHeader, kSizeofWaveHeader);
    size -= kSizeofWaveHeader;

    auto waveHeader = reinterpret_cast<WaveFileHeader *>(buffer);
    waveHeader->size = sizeof(kWaveHeader) + size - 8;

    auto waveFormatHeader = reinterpret_cast<WaveFormatHeader *>(reinterpret_cast<char *>(waveHeader) + sizeof(*waveHeader));
    waveFormatHeader->frequency = is11KhzSample ? 11025 : 22050;
    waveFormatHeader->byteRate = waveFormatHeader->frequency;

    auto waveDataHeader = reinterpret_cast<WaveDataHeader *>(reinterpret_cast<char *>(waveFormatHeader) + sizeof(*waveFormatHeader));
    waveDataHeader->length = size;

    auto rwOps = SDL_RWFromMem(buffer, size + sizeof(kWaveHeader));
    if (!rwOps)
        return nullptr;

    return Mix_LoadWAV_RW(rwOps, 1);
}

static std::pair<bool, bool> isKnownExtension(const char *filename, size_t len)
{
    assert(filename);

    if (len < 4)
        return {};

    bool knownExt = false;
    bool isRaw = false;

    for (size_t i = 0; i < kAudioExtensions.size() - 1; i++) {
        auto ext = kAudioExtensions[i];

        if (filename[len - 4] == '.' && !_stricmp(filename + len - 3, ext)) {
            knownExt = true;
            isRaw = ext[0] == 'r';
            break;
        }
    }

    if (!knownExt && len >= 5 && !_stricmp(filename + len - 5, ".flac"))
        knownExt = true;

    return { knownExt, isRaw };
}

static bool is11KhzSample(const char *name)
{
    auto len = strlen(name);
    return len >= 13 && !_stricmp(name + len - 13, "\\endgamew.raw") || len >= 9 && !_stricmp(name + len - 9, "\\foul.raw");
}

static void loadCustomCommentary()
{
    assert(m_sampleTables.size() == kNumSampleTables);
    assert(!strcmp(m_sampleTables[kEndGameSoClose].dir, "end_game_so_close"));
    assert(!strcmp(m_sampleTables[kYellowCard].dir, "yellow_card"));

    const std::string audioPath = joinPaths("audio", "commentary") + getDirSeparator();

    for (int i = 0; i < kNumSampleTables; i++) {
        auto& table = m_sampleTables[i];
        table.lastPlayedIndex = -1;

        if (!table.samples.empty())
            continue;

        const auto& dirPath = audioPath + table.dir + getDirSeparator();
        const auto& dirFullPath = pathInRootDir((audioPath + table.dir).c_str());
        auto dir = opendir(dirFullPath.c_str());

        if (dir) {
            for (dirent *entry; entry = readdir(dir); ) {
                if (entry->d_namlen < 4)
                    continue;

                bool knownExt, isRaw;
                std::tie(knownExt, isRaw) = isKnownExtension(entry->d_name, entry->d_namlen);

                if (knownExt) {
                    auto samplePath = dirPath + entry->d_name;
                    auto data = loadFile(samplePath.c_str());

                    if (data.first) {
                        int frequency = getSampleFrequency(entry->d_name, entry->d_namlen);
                        bool is11Khz = isRaw && is11KhzSample(entry->d_name);

                        table.samples.emplace_back(data.first, data.second, isRaw, is11Khz, frequency);
                        logInfo("`%s' loaded OK, frequency: %d", samplePath.c_str(), frequency);
                    } else {
                        logWarn("Failed to load sample `%s'", samplePath.c_str());
                    }
                }
            }

            closedir(dir);
        }
    }
}

static void mapOriginalSamples()
{
    struct SampleMapping {
        CommentarySampleTableIndex tableIndex;
        std::pair<int, int> sampleIndexRange;
    } static const kSampleMapping[] = {
        // preloaded comments
        { kGoal, { 0, 6 } },
        { kKeeperSaved, { 6, 13 } },
        { kOwnGoal, { 13, 19 } },
        { kNearMiss, { 19, 21 } },
        { kNearMiss, { 28, 30 } },
        { kKeeperClaimed, { 21, 28 } },
        { kHitPost, { 28, 36 } },
        { kHitBar, { 35, 37 } },
        { kGoodTackle, { 38, 42 } },
        { kGoodPass, { 42, 46 } },
        { kFoul, { 46, 49 } },
        { kDirtyTackle, { 46, 53 } },
        { kPenalty, { 53, 57 } },
        { kPenaltySaved, { 57, 60 } },
        { kPenaltySaved, { 6, 8 } },
        { kPenaltySaved, { 11, 13 } },
        { kPenaltySaved, { 9, 10 } },
        { kPenaltyMissed, { 60, 64 } },
        { kPenaltyGoal, { 64, 68 } },
        { kHeader, { 68, 76 } },
        // on-demand comments
        { kCorner, { 76, 82 } },
        { kChangeTactics, { 82, 88 } },
        { kSubstitution, { 88, 94 } },
        { kRedCard, { 94, 106 } },
        { kEndGameSoClose, { 106, 108 } },
        { kEndGameRout, { 108, 109 } },
        { kEndGameSensational, { 109, 111 } },
        { kYellowCard, { 111, 124 } },
        { kThrowIn, { 124, 128 } },
    };

    for (const auto& mapping : kSampleMapping) {
        auto& table = m_sampleTables[mapping.tableIndex];

        if (!table.samples.empty())
            continue;

        const auto& range = mapping.sampleIndexRange;

        for (int i = range.first; i < range.second; i++) {
            const auto& sampleData = m_originalCommentarySamples[i];
            if (sampleData.buffer)
                table.samples.emplace_back(sampleData);
        }
    }

    // all the samples that have double chance of getting played
    static const std::tuple<CommentarySampleTableIndex, int, int> kDoubleChanceSamples[] = {
        { kGoal, 0, 2 },
        { kKeeperSaved, 6, 7 },
        { kOwnGoal, 15, 17 },
        { kFoul, 46, 47 },
        { kDirtyTackle, 46, 47 },
        { kCorner, 77, 79 },
        { kChangeTactics, 82, 84 },
        { kSubstitution, 88, 90 },
        { kRedCard, 94, 98 },
        { kYellowCard, 111, 114 },
    };

    for (const auto& sampleData : kDoubleChanceSamples) {
        auto& table = m_sampleTables[std::get<0>(sampleData)];
        auto start = std::get<1>(sampleData);
        auto end = std::get<2>(sampleData);

        while (start < end) {
            const auto& sampleData = m_originalCommentarySamples[start];
            if (sampleData.buffer) {
                auto it = std::find(table.samples.begin(), table.samples.end(), sampleData);
                if (it != table.samples.end())
                    it->frequency = 2;
            }
            start++;
        }
    }
}

static void loadOriginalSamples()
{
    // unique samples used in tables of on-demand comment filenames
    static const char *kOnDemandSamples[] = {
        aHardM10_v__raw, aHardM10_w__raw, aHardM10_y__raw, aHardM313_1__ra, aHardM313_2__ra, aHardM313_3__ra, aHardM10_5__raw,
        aHardM10_7__raw, aHardM10_8__raw, aHardM10_9__raw, aHardM10_a__raw, aHardM10_b__raw, aHardM233_j__ra, aHardM233_k__ra,
        aHardM233_l__ra, aHardM233_m__ra, aHardM10_3__raw, aHardM10_4__raw, aHardM196_8__ra, aHardM196_9__ra, aHardM196_a__ra,
        aHardM196_b__ra, aHardM196_c__ra, aHardM196_d__ra, aHardM196_e__ra, aHardM196_f__ra, aHardM196_g__ra, aHardM196_h__ra,
        aHardM196_i__ra, aHardM196_j__ra, aHardM406_f__ra, aHardM406_g__ra, aHardM406_h__ra, aHardM406_i__ra, aHardM406_j__ra,
        aHardM443_7__ra, aHardM443_8__ra, aHardM443_9__ra, aHardM443_a__ra, aHardM443_b__ra, aHardM443_c__ra, aHardM443_d__ra,
        aHardM443_e__ra, aHardM443_f__ra, aHardM443_g__ra, aHardM443_h__ra, aHardM443_i__ra, aHardM443_j__ra, aHardM406_3__ra,
        aHardM406_8__ra, aHardM406_7__ra, aHardM406_9__ra,
        kSentinel,
    };

    int i = 0;
    for (auto ptr : { commentaryTable, kOnDemandSamples }) {
        for (; *ptr != kSentinel; ptr++, i++) {
            auto& sampleData = m_originalCommentarySamples[i];
            if (!sampleData.buffer) {
                auto fileData = loadFile(*ptr);
                sampleData.buffer = fileData.first;
                sampleData.size = fileData.second;
                sampleData.isRaw = true;
                sampleData.is11Khz = is11KhzSample(*ptr);
            }
        }
    }

    assert(i == m_originalCommentarySamples.size());

    mapOriginalSamples();
}

static void initGameAudio();

void SWOS::LoadCommentary()
{
    if (g_soundOff || !g_commentary)
        return;

    logInfo("Loading commentary...");

    auto audioDirPath = pathInRootDir("audio");
    auto audioDir = opendir(audioDirPath.c_str());

    m_hasAudioDir = audioDir != nullptr;

    if (m_hasAudioDir) {
        closedir(audioDir);
        loadCustomCommentary();
    } else {
        loadOriginalSamples();
    }
}

static std::tuple<char *, size_t, bool> loadAnyAudioFile(const char *path)
{
    assert(path);

    char buf[128];
    int len = strlen(path);

    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '.') {
            len = i + 1;
            break;
        }
    }

    memcpy(buf, path, len);

    for (auto ext : kAudioExtensions) {
        bool isRaw = ext[0] == 'r';
        size_t offset = isRaw ? kSizeofWaveHeader : 0;

        strcpy(buf + len, ext);
        auto data = loadFile(buf, offset);

        if (data.first)
            return { data.first, data.second, isRaw };
    }

    logWarn("`%s' not found with any supported extension", path);
    return {};
}


void SWOS::LoadSoundEffects()
{
    if (g_soundOff)
        return;

    logInfo("Loading sound effects...");

    goodPassTimer = 0;  // why is this here... >_<
    playIntroChantSampleOffset = PlayIntroChantSample;
    playFansChant8lOffset = PlayFansChant8lSample;
    playFansChant10lOffset = PlayFansChant10lSample;
    playFansChant4lOffset = PlayFansChant4lSample;

    std::string prefix;
    if (m_hasAudioDir)
        prefix = std::string("audio") + getDirSeparator();

    int i = 0;
    for (auto p = soundEffectsTable; *p != kSentinel; p++, i++) {
        if (m_sfxSamples[i].buffer)
            continue;

        auto filename = *p;
        if (m_hasAudioDir)
            filename += 4;  // skip "sfx\" prefix

        auto fileData = loadAnyAudioFile(m_hasAudioDir ? (prefix + filename).c_str() : filename);
        assert(std::get<0>(fileData));

        bool is11Khz = is11KhzSample(filename);

        m_sfxSamples[i] = { std::get<0>(fileData), std::get<1>(fileData), std::get<2>(fileData), is11Khz };
    }

    assert(i == m_sfxSamples.size());
}

void SWOS::LoadIntroChant()
{
    if (g_soundOff)
        return;

    auto color = leftTeamIngame.prShirtCol;
    auto chantIndex = introTeamChantIndices[color];

    std::string prefix;
    if (m_hasAudioDir)
        prefix = std::string("audio") + getDirSeparator();

    if (chantIndex >= 0) {
        auto introChantFile = introTeamChantsTable[chantIndex];
        m_introChantSample.free();

        logInfo("Picked intro chant %d `%s'", chantIndex, introChantFile);

        if (m_hasAudioDir)
            introChantFile += 4;    // skip "sfx\" prefix

        auto fileData = loadAnyAudioFile(m_hasAudioDir ? (prefix + introChantFile).c_str() : introChantFile);
        assert(std::get<0>(fileData));

        bool is11Khz = is11KhzSample(introChantFile);

        m_introChantSample = { std::get<0>(fileData), std::get<1>(fileData), std::get<2>(fileData), is11Khz };

        playIntroChantSampleOffset = PlayIntroChantSample;
    } else {
        logInfo("Not playing intro chant");
        m_introChantSample.free();

        playIntroChantSampleOffset = PlayFansChant4lSample;
    }
}

static void playChant(Mix_Chunk *chunk)
{
    if (g_soundOff || !g_crowdChantsOn)
        return;

    if (chunk) {
        Mix_VolumeChunk(chunk, kChantsVolume);
        m_chantsChannel = Mix_PlayChannel(-1, chunk, -1);
        m_chantsSample = chunk;
    }
}

static void playChant(SfxSampleIndex index)
{
    if (g_soundOff || !g_crowdChantsOn)
        return;

    assert(index == kChant4l || index == kChant8l || index == kChant10l);

    logDebug("Playing chant index %d", index);
    auto& sample = m_sfxSamples[index];
    auto chunk = sample.getChunk();

    if (!chunk)
        logWarn("Sound effect %d failed to load");

    playChant(chunk);
}

void SWOS::PlayIntroChantSample()
{
    if (m_introChantSample.buffer) {
        logDebug("Playing intro chant %#x", m_introChantSample);
        auto chunk = m_introChantSample.getChunk();

        if (chunk)
            playChant(chunk);
        else
            logWarn("Failed to load intro chant");
    }
}

static void playSfx(SfxSampleIndex index, int volume = MIX_MAX_VOLUME, int loopCount = 0)
{
    if (g_soundOff)
        return;

    assert(index >= 0 && index < kNumSoundEffects);

    auto chunk = m_sfxSamples[index].getChunk();
    if (chunk) {
        Mix_VolumeChunk(chunk, volume);
        Mix_PlayChannel(-1, chunk, loopCount);
    } else {
        logWarn("Failed to load sound effect %d", index);
    }
}

static bool commentPlaying()
{
    return m_commentaryChannel >= 0 && Mix_Playing(m_commentaryChannel);
}

static void playComment(Mix_Chunk *chunk, bool interrupt = true)
{
    assert(chunk);

    if (commentPlaying()) {
        if (!interrupt) {
            logDebug("Playing comment aborted since the previous comment is still playing");
            return;
        }

        logDebug("Interrupting previous comment");
        Mix_HaltChannel(m_commentaryChannel);
    }

    Mix_VolumeChunk(chunk, MIX_MAX_VOLUME);
    m_commentaryChannel = Mix_PlayChannel(-1, chunk, 0);

    m_lastPlayedComment = chunk;
    m_lastPlayedCommentHash = hash(chunk->abuf, chunk->alen);
}

static bool isLastPlayedComment(const Sample& sample)
{
    assert(sample.sampleData.chunk);

    if (m_lastPlayedComment) {
        return sample.sampleData.hash == m_lastPlayedCommentHash && sample.sampleData.chunk->alen == m_lastPlayedComment->alen &&
            !memcmp(sample.sampleData.chunk->abuf, m_lastPlayedComment->abuf, sample.sampleData.chunk->alen);
    }

    return false;
}

static void playComment(CommentarySampleTableIndex tableIndex, bool interrupt = true)
{
    if (g_soundOff || !g_commentary || g_muteCommentary)
        return;

    auto& table = m_sampleTables[tableIndex];
    auto& samples = table.samples;

    if (!samples.empty()) {
        int range = 0;
        for (const auto& sample : samples)
            range += sample.frequency;

        int i = getRandomInRange(0, range - 1);

        size_t j = 0;
        for (; j < samples.size(); j++) {
            i -= samples[j].frequency;
            if (i < 0)
                break;
        }

        assert(j >= 0 && j < static_cast<int>(samples.size()));

        while (!samples.empty()) {
            auto chunk = samples[j].sampleData.getChunk();

            if (!chunk) {
                logWarn("Failed to load comment %d from category %d", j, tableIndex);
                samples.erase(samples.begin() + j);
                table.lastPlayedIndex -= table.lastPlayedIndex >= static_cast<int>(j);
                j -= j == samples.size();
                continue;
            }

            if (samples.size() > 1 && (table.lastPlayedIndex == j || isLastPlayedComment(samples[j]))) {
                j = (j + 1) % samples.size();
                continue;
            }

            if (samples.size() > 2 && table.lastPlayedIndex == j) {
                j = (j + 1) % samples.size();
                continue;
            }

            logDebug("Playing comment %d from category %d", j, tableIndex);
            playComment(chunk, interrupt);
            table.lastPlayedIndex = j;
            m_lastPlayedCategory = tableIndex;
            return;
        }
    } else {
        logWarn("Comment table %d empty", tableIndex);
    }
}

//
// SWOS play sound hooks
//

// in:
//     A6 -> player's team
//
void SWOS::PlayerDoingHeader_157()
{
    auto team = A6.as<TeamGeneralInfo *>();
    assert(team);

    if (team->playerNumber)
        playComment(kHeader, false);
}

// in:
//     A6 -> player's team
//
void SWOS::PlayerTackled_59()
{
    auto team = A6.as<TeamGeneralInfo *>();
    assert(team);

    if (team->playerNumber)
        playComment(kInjury);
}

void SWOS::PlayPenaltyGoalComment()
{
    playComment(kPenaltyGoal);
    performingPenalty = 0;
}

void SWOS::PlayPenaltyMissComment()
{
    playComment(kPenaltyMissed);
    performingPenalty = 0;
}

void SWOS::PlayPenaltySavedComment()
{
    playComment(kPenaltySaved);
    performingPenalty = 0;
}

void SWOS::PlayPenaltyComment()
{
    performingPenalty = -1;
    playComment(kPenalty);
}

void SWOS::PlayFoulComment()
{
    playComment(kFoul);
}

void SWOS::PlayHarderFoulComment()
{
    playComment(kDirtyTackle);
}

void SWOS::PlayGoodPassComment()
{
    playComment(kGoodPass, false);
}

void SWOS::PlayGoodTackleComment()
{
    playComment(kGoodTackle, false);
}

void SWOS::PlayPostHitComment()
{
    playComment(kHitPost);
}

void SWOS::PlayBarHitComment()
{
    playComment(kHitBar);
}

void SWOS::PlayKeeperClaimedComment()
{
    if (performingPenalty) {
        playComment(kPenaltySaved);
        performingPenalty = 0;
    } else {
        // don't interrupt penalty saved comments
        if (m_lastPlayedCategory != kPenaltySaved || !commentPlaying())
            playComment(kKeeperClaimed);
    }
}

void SWOS::PlayNearMissComment()
{
    if (performingPenalty || penaltiesState == -1)
        PlayPenaltyMissComment();
    else
        playComment(kNearMiss);
}

void SWOS::PlayGoalkeeperSavedComment()
{
    if (performingPenalty || penaltiesState == -1)
        PlayPenaltySavedComment();
    else
        playComment(kKeeperSaved);
}

// fix original SWOS bug where penalty flag remains set when penalty is missed, but it's not near miss
void SWOS::CheckIfBallOutOfPlay_251()
{
    performingPenalty = 0;
}

void SWOS::PlayOwnGoalComment()
{
    playComment(kOwnGoal);
}

void SWOS::PlayGoalComment()
{
    if (performingPenalty || penaltiesState == -1)
        PlayPenaltyGoalComment();
    else
        playComment(kGoal);
}

void SWOS::PlayYellowCardSample()
{
    playComment(kYellowCard);
}

void SWOS::PlayRedCardSample()
{
    playComment(kRedCard);
}

void SWOS::PlayCornerSample()
{
    playComment(kCorner);
}

void SWOS::PlayTacticsChangeSample()
{
    playComment(kChangeTactics);
}

void SWOS::PlaySubstituteSample()
{
    playComment(kSubstitution);
}

void SWOS::PlayThrowInSample()
{
    playComment(kThrowIn);
}

void SWOS::PlayCrowdNoiseSample()
{
    // this is the first audio function to be called right before the game
    initGameAudio();

    playSfx(kBackgroundCrowd, 100, -1);
}

void SWOS::PlayMissGoalSample()
{
    playSfx(kMissGoal);
}

void SWOS::PlayHomeGoalSample()
{
    playSfx(kHomeGoal);
}

void SWOS::PlayAwayGoalSample()
{
    // playing same sample as home goal, but perfectly able to separate if needed
    playSfx(kHomeGoal);
}

void SWOS::PlayRefereeWhistleSample()
{
    playSfx(kWhistle, 42);
}

void SWOS::PlayEndGameWhistleSample()
{
    playSfx(kEndGameWhistle, 42);
}

void SWOS::PlayFoulWhistleSample()
{
    playSfx(kFoulWhistle, 42);
}

void SWOS::PlayKickSample()
{
    playSfx(kKick, 25);
}

void SWOS::PlayBallBounceSample()
{
    playSfx(kBounce, 42);
}

static void playCrowdChants();

void SWOS::PlayEnqueuedSamples()
{
    if (playingYellowCardTimer >= 0 && !--playingYellowCardTimer) {
        PlayYellowCardSample();
        playingYellowCardTimer = -1;
    } else if (playingRedCardTimer >= 0 && !--playingRedCardTimer) {
        PlayRedCardSample();
        playingRedCardTimer = -1;
    } else if (playingGoodPassTimer >= 0 && !--playingGoodPassTimer) {
        PlayGoodPassComment();
        playingGoodPassTimer = -1;
    } else if (playingThrowInSample >= 0 && !--playingThrowInSample) {
        PlayThrowInSample();
        playingThrowInSample = -1;
    } else if (playingCornerSample >= 0 && !--playingCornerSample) {
        PlayCornerSample();
        playingCornerSample = -1;
    } else if (substituteSampleTimer >= 0 && !--substituteSampleTimer) {
        PlaySubstituteSample();
        substituteSampleTimer = -1;
    } else if (tacticsChangedSampleTimer >= 0 && !--tacticsChangedSampleTimer) {
        PlayTacticsChangeSample();
        tacticsChangedSampleTimer = -1;
    }

    playCrowdChants();
}

static void loadAndPlayEndGameCrowdSample(int index)
{
    if (g_trainingGame || !g_commentary)
        return;

    assert(index >= 0 && index <= 3);

    // no need to worry about this still being played
    m_endGameCrowdSample.free();

    auto filename = endGameCrowdSamples[index];

    std::string path;
    if (m_hasAudioDir)
        path = joinPaths("audio", "fx") + getDirSeparator() + (filename + 5);   // skip "hard\" part

    auto fileData = loadAnyAudioFile(m_hasAudioDir ? path.c_str() : filename);
    assert(std::get<0>(fileData));

    m_endGameCrowdSample = { std::get<0>(fileData), std::get<1>(fileData), std::get<2>(fileData), false };

    auto chunk = m_endGameCrowdSample.getChunk();
    if (chunk) {
        Mix_VolumeChunk(chunk, MIX_MAX_VOLUME);
        Mix_PlayChannel(-1, chunk, 0);
    } else {
        logWarn("Failed to load end game sample %s", filename);
    }
}

void SWOS::LoadAndPlayEndGameComment()
{
    if (g_soundOff)
        return;

    int index;
    // weird way of comparing, it might be like this because of team positions?
    if (team1GoalsDigit2 == team2GoalsDigit2)
        index = 2;
    else if (team2GoalsDigit2 < team1GoalsDigit2)
        index = 0;
    else
        index = 1;

    loadAndPlayEndGameCrowdSample(index);

    int goalDiff = std::abs(statsTeam1Goals - statsTeam2Goals);
    if (goalDiff >= 3) {
        PlayItsBeenACompleteRoutComment();
    } else if (!goalDiff) {
        PlayDrawComment();
    } else if (statsTeam1Goals + statsTeam2Goals >= 4) {
        PlaySensationalGameComment();
    }
}

void SWOS::PlayDrawComment()
{
    playComment(kEndGameSoClose);
}

void SWOS::PlaySensationalGameComment()
{
    playComment(kEndGameSensational);
}

void SWOS::PlayItsBeenACompleteRoutComment()
{
    playComment(kEndGameRout);
}

void SWOS::PlayFansChant4lSample()
{
    playChant(kChant4l);
}

void SWOS::PlayFansChant8lSample()
{
    playChant(kChant8l);
}

void SWOS::PlayFansChant10lSample()
{
    playChant(kChant10l);
}

static void stopChants()
{
    if (m_chantsChannel >= 0) {
        logDebug("Stopping the chants");
        Mix_HaltChannel(m_chantsChannel);
        m_chantsChannel = -1;
        m_chantsSample = nullptr;
    }
}

static void setVolumeOrStopChants(int volume)
{
    if (m_chantsChannel != -1) {
        volume &= 0xff;
        if (volume) {
            assert(m_chantsSample);
            Mix_VolumeChunk(m_chantsSample, volume);
        } else {
            stopChants();
        }
    }
}

void SWOS::RunStoppageEventsAndSetAnimationTables_320()
{
    // fix SWOS bug where crowd chants only get reloaded when auto replays are on
    loadCrowdChantSampleFlag = 1;
}

void SWOS::GameLoop_224()
{
    // the code doesn't even get to test the flag unless the replay is started
    if (loadCrowdChantSampleFlag) {
        LoadCrowdChantSample();
        loadCrowdChantSampleFlag = 0;
    }
}

void SWOS::LoadCrowdChantSample()
{
    if (g_soundOff || !g_commentary)
        return;

    bool wasInteresting = m_interestingGame;
    m_interestingGame = false;

    int sampleIndex = -1;

    if (!statsTeam1Goals || !statsTeam2Goals) {
        if (statsTeam1Goals != statsTeam2Goals) {
            // one team scored, other is losing with 0
            int goalDiff = std::max(statsTeam1Goals, statsTeam2Goals);
            if (goalDiff <= 6) {
                if (!getRandomInRange(0, 1)) {
                    // 50% chance of cheering up the losing team ;)
                    logDebug("Let's cheer up the losing team");
                    sampleIndex = 6;
                } else {
                    logDebug("Choosing %d-0 chant", goalDiff);
                    // otherwise pick x-nil chant sample, where x = [1..6]
                    sampleIndex = goalDiff - 1;
                }
            }
        }
    } else if (statsTeam1Goals == statsTeam2Goals) {
        // equalizer goal scored
        logDebug("Choosing equalizer chant");
        sampleIndex = 7;
    }

    if (sampleIndex >= 0) {
        assert(sampleIndex >= 0 && sampleIndex <= 7);

        stopChants();

        if (m_lastResultSampleIndex != sampleIndex) {
            m_resultSample.free();

            auto filename = resultChantFilenames[sampleIndex];

            std::string path;
            if (m_hasAudioDir)
                path = joinPaths("audio", "fx") + getDirSeparator() + (filename + 5);   // skip "hard\" part

            auto fileData = loadAnyAudioFile(m_hasAudioDir ? path.c_str() : filename);

            assert(std::get<0>(fileData));
            m_resultSample = { std::get<0>(fileData), std::get<1>(fileData), std::get<2>(fileData), false };

            m_lastResultSampleIndex = sampleIndex;
        }

        playFansChant10lOffset = PlayResultSample;
        m_interestingGame = true;

    } else {
        // game not very interesting
        playFansChant10lOffset = PlayFansChant10lSample;
    }

    // a change from original SWOS: we will reset chant function after each goal (SWOS would only reset if "interesting" game)
    // this is to prevent fans chanting "three nil" after the result has changed to 3-1
    m_playCrowdChantsFunction = nullptr;

    if (wasInteresting && !m_interestingGame && m_chantsChannel >= 0 && Mix_Playing(m_chantsChannel)) {
        if (m_fadeInChantTimer) {
            m_fadeOutChantTimer = m_fadeInChantTimer;
            m_fadeInChantTimer = 0;
        } else if (!m_fadeOutChantTimer) {
            m_fadeOutChantTimer = MIX_MAX_VOLUME;
            m_fadeInChantTimer = 0;
        }
    }
}

void SWOS::PlayResultSample()
{
    auto chunk = m_resultSample.getChunk();
    if (chunk)
        playChant(chunk);
    else
        logWarn("Failed to load result sample");
}

static void playCrowdChants()
{
    if (g_soundOff || !g_crowdChantsOn || g_trainingGame)
        return;

    if (m_fadeInChantTimer) {
        m_fadeInChantTimer--;
        int volume = (MIX_MAX_VOLUME - m_fadeInChantTimer) / 2;
        if (volume)
            setVolumeOrStopChants(volume);
    } else if (m_nextChantTimer) {
        m_nextChantTimer--;
    } else if (m_fadeOutChantTimer) {
        m_fadeOutChantTimer--;
        int volume = m_fadeOutChantTimer / 2;
        setVolumeOrStopChants(volume);
    } else if (getRandomInRange(0, 255) >= 128) {
        int pause = getRandomInRange(0, 255) * 2;
        m_nextChantTimer = pause;
    } else {
        if (!m_playCrowdChantsFunction || getRandomInRange(0, 255) >= 128) {
            // result chant will keep playing as long as the game is "interesting"
            int index = 2;
            if (!m_interestingGame)
                index = getRandomInRange(0, 3);

            logDebug("Picked chant function %d", index);
            m_playCrowdChantsFunction = (&playIntroChantSampleOffset)[index];
        }

        assert(m_playCrowdChantsFunction);
        logDebug("Invoking crowd chant function");
        m_playCrowdChantsFunction();

        m_fadeInChantTimer = MIX_MAX_VOLUME;
        m_fadeOutChantTimer = MIX_MAX_VOLUME;

        m_nextChantTimer = getRandomInRange(0, 255) * 2 + 500;  // next chant: after 500-1012 ticks
    }
}

void SWOS::TurnCrowdChantsOn()
{
    m_chantsChannel = -1;
    m_fadeInChantTimer = 0;
    m_fadeOutChantTimer = 0;
    m_nextChantTimer = 0;
    g_crowdChantsOn = 1;
}

void SWOS::TurnCrowdChantsOff()
{
    g_crowdChantsOn = 0;
    stopChants();
}

static void verifySpec(int frequency, int format, int channels)
{
    Uint16 actualFormat;

    Mix_QuerySpec(&m_actualFrequency, &actualFormat, &m_actualChannels);
    logInfo("Audio initialized at %dkHz, format: %d, number of channels: %d", m_actualFrequency, actualFormat, m_actualChannels);

    if (m_actualFrequency != frequency || actualFormat != format || m_actualChannels < channels)
        logWarn("Didn't get the desired audio specification, asked for frequency: %d, format: %d, channels: %d",
            frequency, format, channels);
}

static void resetMenuAudio()
{
    Mix_CloseAudio();

    m_refereeWhistleSampleChannel = -1;
    m_commentaryChannel = -1;
    m_chantsChannel = -1;

    if (Mix_OpenAudio(kMenuFrequency, MIX_DEFAULT_FORMAT, 2, kMenuChunkSize))
        sdlErrorExit("SDL Mixer failed to initialize for the menus");

    verifySpec(kMenuFrequency, MIX_DEFAULT_FORMAT, 2);

    synchronizeMixVolume();
}

void resetGameAudio()
{
    Mix_CloseAudio();

    if (Mix_OpenAudio(kGameFrequency, MIX_DEFAULT_FORMAT, 1, kGameChunkSize))
        sdlErrorExit("SDL Mixer failed to initialize for the game");

    Mix_Volume(-1, m_volume);
    verifySpec(kGameFrequency, MIX_DEFAULT_FORMAT, 1);
}

void ensureMenuAudioFrequency()
{
    int frequency;
    if (Mix_QuerySpec(&frequency, nullptr, nullptr) && frequency != kMenuFrequency)
        resetMenuAudio();
}

void initAudio()
{
    if (!g_soundOff)
        resetMenuAudio();
}

void finishAudio()
{
    finishMusic();
    Mix_CloseAudio();
}

static void channelFinished(int channel)
{
    if (channel == m_commentaryChannel) {
        logDebug("Commentary finished playing on channel %d", channel);
        m_commentaryChannel = -1;
    } else if (channel == m_chantsChannel) {
        logDebug("Chants finished playing on channel %d", channel);
        m_chantsChannel = -1;
    } else {
        logDebug("Channel %d finished playing", channel);
    }
}

// switch from menu to game
static void initGameAudio()
{
    if (g_soundOff)
        return;

    fadeOutMusic();
    resetGameAudio();

    Mix_ChannelFinished(channelFinished);

    m_refereeWhistleSampleChannel = -1;
    m_commentaryChannel = -1;
    m_chantsChannel = -1;

    m_lastPlayedCategory = -1;
    m_lastPlayedComment = nullptr;

    m_chantsSample = nullptr;

    m_resultSample.free();
    m_endGameCrowdSample.free();

    m_lastResultSampleIndex = -1;

    int m_fadeInChantTimer = 0;
    int m_fadeOutChantTimer = 0;
    int m_nextChantTimer = 0;

    m_playCrowdChantsFunction = nullptr;
}

void SWOS::StopAudio()
{
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
}

int playIntroSample(void *buffer, int size, int volume, int loopCount)
{
    assert(m_actualFrequency == kGameFrequency && m_actualChannels <= 2);
//    assert(size == kIntroBufferSize);
    assert(loopCount == 0);

//    static Mix_Chunk *chunk;
//    Mix_FreeChunk(chunk);

    //char waveBuffer[8 * kIntroBufferSize];
    //fillWaveBuffer(waveBuffer, buffer, size);

//    chunk = chunkFromBuffer(waveBuffer, kSizeofWaveHeader + size);
//    static uint16_t stereoBuffer[kIntroBufferSize];

//    return Mix_PlayChannel(0, chunk, 0);
    return -1;
}

__declspec(naked) void SWOS::PlayIntroSample()
{
    //__asm {
    //    push ecx
    //    push ebx
    //    push eax
    //    push edx
    //    call playIntroSample
    //    add  esp, 16
    //    retn
    //}
}

//
// option variables
//

template <typename T>
static void setVolume(T& dest, int volume, const char *desc)
{
    if (volume < 0 || volume > kMaxVolume)
        logWarn("Invalid value given for %s volume (%d), clamping", desc, volume);

    volume = std::min(volume, kMaxVolume);
    volume = std::max(volume, 0);

    dest = volume;
}

int getMasterVolume()
{
    return m_volume;
}

void setMasterVolume(int volume, bool apply /* = true */)
{
    setVolume(m_volume, volume, "master");

    if (apply && !g_soundOff)
        Mix_Volume(-1, m_volume);
}

int getMusicVolume()
{
    return m_musicVolume;
}

void setMusicVolume(int volume, bool apply /* = true */)
{
    setVolume(m_musicVolume, volume, "music");

    if (apply && !g_soundOff && !g_musicOff)
        Mix_VolumeMusic(m_musicVolume);
}

//
// audio options menu
//

void showAudioOptionsMenu()
{
    showMenu(audioOptionsMenu);
}

static void restartMusic()
{
    g_midiPlaying = 0;
    SWOS::CDROM_StopAudio();
    TryRestartingMusic();
}

static void toggleMasterSound()
{
    g_soundOff = !g_soundOff;

    if (g_soundOff) {
        finishMusic();
        finishAudio();
    } else {
        initAudio();
        if (g_menuMusic)
            restartMusic();
    }

    DrawMenuItem();
}

static void toggleMenuMusic()
{
    g_menuMusic = !g_menuMusic;
    g_musicOff = !g_menuMusic;

    if (g_menuMusic)
        restartMusic();
    else
        finishMusic();

    DrawMenuItem();
}

static void increaseVolume()
{
    if (getMasterVolume() < kMaxVolume) {
        setMasterVolume(getMasterVolume() + 1);
        DrawMenuItem();
    }
}

static void decreaseVolume()
{
    if (getMasterVolume() > 0) {
        setMasterVolume(getMasterVolume() - 1);
        DrawMenuItem();
    }
}

static void volumeBeforeDraw()
{
    auto entry = A5.as<MenuEntry *>();
    entry->u2.number = m_volume;
}

static void increaseMusicVolume()
{
    if (getMusicVolume() < kMaxVolume) {
        setMusicVolume(getMusicVolume() + 1);
        DrawMenuItem();
    }
}

static void decreaseMusicVolume()
{
    if (getMusicVolume() > 0) {
        setMusicVolume(getMusicVolume() - 1);
        DrawMenuItem();
    }
}

static void musicVolumeBeforeDraw()
{
    auto entry = A5.as<MenuEntry *>();
    entry->u2.number = getMusicVolume();
}

static void toggleCrowdChants()
{
    if (!g_crowdChantsOn)
        SWOS::TurnCrowdChantsOn();
    else
        SWOS::TurnCrowdChantsOff();
}
