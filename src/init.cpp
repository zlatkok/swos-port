#include "log.h"
#include "file.h"
#include "util.h"
#include "audio.h"
#include "render.h"
#include "options.h"
#include "replays.h"
#include "controls.h"

#ifdef NDEBUG
constexpr int kSentinelSize = 0;
#else
constexpr char kSentinelMagic[] = "nick.slaughter@mariah.key.com";
constexpr int kSentinelSize = sizeof(kSentinelMagic);
#endif

// this buffer will masquerade as DOS memory ;)
// decreased by the size of pitch patterns buffer
static char dosMemBuffer[342'402 + kSentinelSize];

// allocate buffer big enough to hold every single pitch pattern (if each would be unique)
static char pitchPatternsBuffer[(42 * 53 + 42) * 256 + kSentinelSize];

// extended memory buffers
static char linBuf384k[(34 * 16 * 30 * 16) * 3 + kSentinelSize] = {0, };

#ifdef DEBUG
template <size_t N>
void verifyBlock(char (&array)[N])
{
    assert(!memcmp(array + sizeof(array) - kSentinelSize, kSentinelMagic, kSentinelSize));
}

void checkMemory()
{
    verifyBlock(dosMemBuffer);
    verifyBlock(pitchPatternsBuffer);
    verifyBlock(linBuf384k);
}
#endif

static void printIntroString()
{
    auto str = aSensibleWorldOfSoccerV2_0;
    while (*str != '\r')
        std::cout << *str++;

    std::cout << "[Win32 Port]\n";
}

static void setupDosBaseMemory()
{
    g_pitchPatterns = pitchPatternsBuffer;

    dosMemLinAdr = dosMemBuffer;
    dosMemOfs30000h = dosMemBuffer + 0x30000;
    dosMemOfs4fc00h = dosMemBuffer + 0x3d400;   // names don't match offsets anymore, but oh well ;)
    dosMemOfs60c00h = dosMemBuffer + 0x4e400;

#ifdef DEBUG
    memcpy(dosMemBuffer + sizeof(dosMemBuffer) - kSentinelSize, kSentinelMagic, kSentinelSize);
    memcpy(pitchPatternsBuffer + sizeof(pitchPatternsBuffer) - kSentinelSize, kSentinelMagic, kSentinelSize);
#endif
}

static void setupExtendedMemory()
{
    linAdr384k = linBuf384k;

    g_memAllOk = 1;
    g_gotExtraMemoryForSamples = 1;

#ifdef DEBUG
    memcpy(linBuf384k + sizeof(linBuf384k) - kSentinelSize, kSentinelMagic, kSentinelSize);
#endif
}

static void init()
{
    skipIntro = 0;
    printIntroString();

    setupDatBuffer = 0;     // disable this permanently
    EGA_graphics = 0;
    g_videoSpeedIndex = 100;

    g_inputControls = 6;    // set this up for CheckControls to avoid touching g_*ScanCode variables

    logInfo("Setting up base and extended memory pointers");
    setupDosBaseMemory();
    setupExtendedMemory();

    initAudio();

    initReplays();
}

// showImageReel
//
// imageList -> array of char pointers to image names, terminated with -1
//
// returns: true if ended normally, false if user aborted
//
static bool showImageReel(const char **imageList)
{
    int _kVgaWidth = kMenuScreenWidth, _kVgaHeight = kMenuScreenHeight;
    int _kVgaScreenSize = _kVgaWidth * _kVgaHeight;

    assert(imageList);

    int k256ImageSize = _kVgaScreenSize + kVgaPaletteSize;

    auto imageBuffer = linAdr384k;
    auto currentImageBuffer = imageBuffer + _kVgaScreenSize;

    // 3 image buffer will be used, images 0 and 2 being empty
    memset(imageBuffer, 0, 3 * _kVgaScreenSize);

    // first image palette is used for all scrolling images
    if (loadFile(*imageList, currentImageBuffer, -1, false) != k256ImageSize)
        return true;

    setPalette(currentImageBuffer + _kVgaScreenSize);

    do {
        // clear palette area
        memset(currentImageBuffer + _kVgaScreenSize, 0, kVgaPaletteSize);

        // scroll down line by line
        for (int lineOffset = 0; lineOffset < 2 * _kVgaHeight * _kVgaWidth; lineOffset += _kVgaWidth) {
            frameDelay();
            updateScreen_MenuScreen(imageBuffer + lineOffset);

            if (anyInputActive()) {
                // copy image at the proper position to linAdr384k so it can fade-out properly (when menu comes in)
                memmove(linAdr384k, imageBuffer + lineOffset, _kVgaScreenSize);
                return false;
            }
        }

        while (true) {
            if (*++imageList == (const char *)-1)
                return true;

            if (loadFile(*imageList, currentImageBuffer, -1, false) == k256ImageSize)
                break;
        }
    } while (true);

    return true;
}

static void initRandomSeed()
{
    auto tm = getCurrentTime();
    auto hundredths = tm.msec / 10;

    auto seed = static_cast<int>(hundredths * hundredths);
    auto lo = (seed & 0xff) ^ tm.min;
    seed = lo | (seed & 0xff00);

    assert(seed <= 0xffff);
    randomSeed = seed;
}

static bool initIntroAnimation()
{
    resetGameAudio();

    soccerBinPtr = dosMemLinAdr;
    introCurrentSoundBuffer = dosMemLinAdr + kVirtualScreenSize;
    soccerBinSoundChunkBuffer = introCurrentSoundBuffer + 47628;
    introCurrentSoundBufferChunk = soccerBinSoundChunkBuffer;
    soccerBinSoundDataPtr = soccerBinSoundChunkBuffer + 47628;
    introScreenBuffer = linAdr384k + kVirtualScreenSize;
    introAborted = 0;

    do {
        if (soccerBinHandle = openFile(aArcSoccer_bin)) {
            introFrameDataOffset = 0;
            introFrameDataChunkRemainder = 0;
            introFirstChunkLoaded = 0;

            if (fread(soccerBinPtr, 20, 1, soccerBinHandle) != 1)
                break;

            introNumFrames = *reinterpret_cast<dword *>(soccerBinPtr + 12);
            auto size = *reinterpret_cast<word *>(soccerBinPtr + 16);

            if (fread(soccerBinPtr, size, 1, soccerBinHandle) != 1)
                break;

            memcpy(introCurrentSoundBuffer, soccerBinPtr, 47628);

            size = *reinterpret_cast<word *>(soccerBinPtr + 47628);
            if (fread(soccerBinPtr, size, 1, soccerBinHandle) != 1)
                break;

            memcpy(soccerBinSoundChunkBuffer, soccerBinPtr, 47628);

            size = *reinterpret_cast<word *>(soccerBinPtr + 47628);
            if (fread(soccerBinPtr, size, 1, soccerBinHandle) != 1)
                break;

            memcpy(soccerBinSoundDataPtr + 47628, soccerBinPtr, 47628);
            memcpy(soccerBinSoundDataPtr, soccerBinSoundChunkBuffer, 47628);

            introSoundBufferIndex = 95256;
            introCurrentFrameDataPtr = soccerBinPtr + 47628;
        }

        clearScreen();
        return false;
    } while (false);

    logWarn("Failed to read soccer.bin");
    SWOS::CloseSoccerBin();
    return true;
}

void SWOS::CloseSoccerBin()
{
    if (soccerBinHandle)
        fclose(soccerBinHandle);
}

void SWOS::IntroEnsureFrameDataLoaded()
{
    constexpr int kChunkSize = 2048;

    int dest = 0;
    int numChunks = 0;
    int subSize = 0;
    auto wasFirstChunkLoaded = introFirstChunkLoaded;

    if (!introFirstChunkLoaded) {
        introFirstChunkLoaded = 1;
        numChunks = 1;
    }

    if (wasFirstChunkLoaded || introFrameDataSize > kChunkSize) {
        introFrameDataChunkRemainder = introFrameDataOffset % kChunkSize;
        auto bytesUntilNextChunk = kChunkSize - introFrameDataChunkRemainder;

        if (introFrameDataChunkRemainder + introFrameDataSize < kChunkSize)
            return;

        if (introFrameDataChunkRemainder) {
            dest = introFrameDataChunkRemainder;
            memcpy(soccerBinPtr + dest, soccerBinPtr + introFrameDataOffset, bytesUntilNextChunk);
            dest += bytesUntilNextChunk;
            subSize = bytesUntilNextChunk;
            if (introFrameDataSize == bytesUntilNextChunk)
                return;
        } else {
            introFrameDataChunkRemainder = 0;
        }

        auto res = std::div(introFrameDataSize - subSize, kChunkSize);
        numChunks = res.quot + (res.rem > 0);
    }

    fread(soccerBinPtr + dest, numChunks * kChunkSize, 1, soccerBinHandle);
    introFrameDataOffset = introFrameDataChunkRemainder;
}

static void introDelay()
{
    SDL_Delay(35);
}

static char *introDrawFrame(char *frameData)
{
    introCurrentFrameDataPtr = frameData;

    if (!introAnimationTimer) {
        setPalette(frameData, kVgaPaletteNumColors / 2);
        frameData += kVgaPaletteSize / 2;

        int index = 0;
        auto dest = introScreenBuffer;

        do {
            auto pixel = *frameData++;
            if (pixel < 0) {
                pixel &= 0x7f;

                auto count = *frameData++;
                if (!count)
                    break;

                do {
                    dest[index] = pixel;
                    index += 2;
                    if (index >= kVgaWidth) {
                        index = 0;
                        dest += 2 * kVgaWidth;
                    }
                } while (--count);
            } else {
                dest[index] = pixel;
                index += 2;
                if (index >= kVgaWidth) {
                    index = 0;
                    dest += 2 * kVgaWidth;
                }
            }
        } while (true);

        auto src = introScreenBuffer;
        for (int y = 0; y < kVgaHeight / 2; y++) {
            for (int x = 0; x < kVgaWidth / 2; x++) {
                src[1] = (src[0] + src[2]) / 2;
                src[kVgaWidth] = (src[0] + src[2 * kVgaWidth]) / 2;
                src[kVgaWidth + 1] = (src[0] + src[2 * kVgaWidth + 2]) / 2;
                src += 2;
            }
            src += kVgaWidth;
        }

        updateScreen(introScreenBuffer, 32, 148);
        while (introCountDownTimer) {
            introDelay();
            timerProc();
        }
        timerProc();
        introCountDownTimer = introWaitInterval;
    } else {
        frameData += introFrameDataSize - 1768;
        if (introAnimationTimer < 4) {
            while (introCountDownTimer > 4 - introAnimationTimer) {
                introDelay();
                timerProc();
            }
            introCountDownTimer = 4;
            introAnimationTimer = 0;
        } else {
            while (!introCountDownTimer) {
                introDelay();
                timerProc();
            }
            introCountDownTimer = 4;
            introAnimationTimer = 4;
        }
        timerProc();
    }

    SAFE_INVOKE(IntroFillSoundBuffer);
    frameData += 1764;

//    if (introCurrentSoundBufferNum == 26)
//        playIntroSample(soccerBinSoundDataPtr + introCurrentSoundBufferChunkOffset, kIntroBufferSize / 2, 127, 0);

    return frameData;
}

__declspec(naked) void SWOS::IntroDrawFrame()
{
    __asm {
        push esi
        call introDrawFrame
        add  esp, 4
        mov  esi, eax
        retn
    }
}

static void showImageReelsAndIntro()
{
    bool aborted = false;
    if (!disableImageReels()) {
        logInfo("Showing image reels");
        aborted = !showImageReel(partOneImages) || !showImageReel(partTwoImages);
    }

    getPalette(g_fadePalette);
    SAFE_INVOKE(FadeOutToBlack);

    if (!disableIntro() && !aborted && initIntroAnimation()) {
        logInfo("Playing intro");
        PlayIntroAnimation();
        getPalette(g_fadePalette);
        SAFE_INVOKE(FadeOutToBlack);
    }
}

char *league_name_table[80] = {
    "albanian_league",
    "austrian_league",
    "belgian_league",
    "bulgarian_league",
    "croatian_league",  
    "cypriot_league",
    "czech_league",
    "danish_league",
    "english_leagues",
    "",          
    "estonian_league",  
    "faroe_isles_league",
    "finnish_league",
    "frenche_leagues",
    "german_leagues",
    "greek_league",
    "hungarian_league",
    "icelandic_league",
    "irish_league",
    "israeli_league",
    "italian_leagues",
    "latvian_league",
    "lithuanian_league",
    "luxembourg_league",
    "maltese_league",
    "dutch_leagues",
    "northern_irish_league",
    "norwegian_league",
    "polish_league",
    "portugese_league",
    "romanian_league", 
    "russian_league",
    "san_marino_league",
    "scotland_leagues",
    "slovakian_league",
    "spanish_leagues",
    "swedish_league",
    "swiss_league",
    "turkish_league",
    "ukrainian_league",
    "welsh_league",
    "yugoslavian_league",
    "algerian_league",
    "argentinian_league",
    "australian_leagues",
    "bolivian_league",
    "brasilian_league",
    "",
    "chilean_league",
    "colombian_league",
    "ecuador_league",
    "el_salvador_leagues",
    "",
    "",
    "",
    "japan_leagues",
    "",
    "",
    "",
    "",
    "mexican_leagues",
    "",
    "new_zealand_leagues",
    "",
    "paraguayan_league",
    "peruvian_league",
    "surinam_league",
    "taiwanese_league",
    "",
    "south_african_league",
    "",
    "uruguayan_league",
    "",
    "american_league",
    "",
    "indian_league",
    "belarussian_league",
    "venezuelan_league",
    "slovenian_league",
    "ghana_league"
};

char *division_name_table[4] = {
    "premier",
    "division 1",
    "division 2",
    "division 3"
};

char *month_name_table[12] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

char *season_type_table[2] = {
	"Yearly",
	"Seasonal"
};

int totalTeams = 0;

int isTeamFix = 0;
int isMonthFix = 0;
int isEachMatchFix = 0;
int isGreekFix = 0;

void DebugLeagueData()
{
    byte a, b;

    a = albanian_league.BMonth;
    b = albanian_league.league[0].Teams;
    logInfo("albanian_league.BMonth = %d", a);
    logInfo("albanian_league.league[0].Teams = %d", b);

    a = english_leagues.BMonth;
    b = english_leagues.league[0].Teams;
    logInfo("english_leagues.BMonth = %d", a);
    logInfo("english_leagues.league[0].Teams = %d", b);

    a = mexican_leagues.league[0].Teams;
    b = mexican_leagues.league[1].Teams;
    logInfo("mexican_leagues.league[0].Teams = %d", a);
    logInfo("mexican_leagues.league[1].Teams = %d", b);
}

NumLeagues GetNumLeagues(byte n)
{
    byte i, j;
    char fn[256];
    NumLeagues r;
    byte NumTeams;
    byte bDummy;
    byte buffer[684];
    FILE* pFile;

    for (i = 0; i <= 5; i++)
        r.num[i] = 0;

    sprintf(fn, "DATA\\TEAM.%03d.", n);

    pFile = fopen(fn, "rb");
    if (pFile == NULL) return r;

    fread(&bDummy, 1, 1, pFile);
    fread(&bDummy, 1, 1, pFile);

    NumTeams = bDummy;
    for (i = 0; i <= NumTeams - 1; i++) {
        fread(buffer, 1, 684, pFile);
        if (buffer[25] == 0) r.num[0]++;
        if (buffer[25] == 1) r.num[1]++;
        if (buffer[25] == 2) r.num[2]++;
        if (buffer[25] == 3) r.num[3]++;
        if (buffer[25] == 4) r.num[4]++;
		r.num[5] = 0;
		for (j = 0; j <= 4; j++) {
			r.num[5] += r.num[j];
		}
    }
	totalTeams += r.num[5];

    fclose(pFile);   
    return r;
}

/*
	Fix#1(TeamNumFix): Fix up number of teams in exe after team.xxx check (no mismatch!)

	Fix#2(MonthFix): Dependency #1, Fix up month only for leagues which have been fixed up (increased in #1)
		if Bmonth < Emonth then Calendar = YEARLY (same year)
		if Bmonth > Emonth then Calendar = SEASONAL (from one year to next)
		YEARLY:
			Earliest Bmonth = January
			Latest Emonth = December
			If earliest Bmonth is reached, then Emonth must be extended if needed!
			Maximal range: January - December!!!
		SEASONAL:
			Earliest Bmonth = July
			Latest Emonth = June
			If earliest Bmonth is reached, then Emonth must be extended if needed!
			Maximal range: July - June!!!

	Fix#3(EachMatchFix): Dependency:#2, if items per any leagues > 12 then number of match = 2

	Fix#4(GreekFix): Dependency:None (for greek_league only), 
		if teams > 16 then Bmonth = 7(Oct) else Bmonth = 8(Sep) (original greek has 18 teams)
		greek_cup.BMonth = 8(Oct), EMonth = 5(May)
*/
void FixupLeague(League *_league)
{
	byte isTaiwaneseFix = 0;
	byte isSanMarinoFix = 0;
	byte isSpanishFix = 0;
	
	byte i;
    byte after, before;
    NumLeagues numLeagues;

	byte teamFixedup = 0;
	byte calendarType; // 0:YEARLY, 1:SEASONAL 
	byte bMonth, eMonth, bMonthOld, eMonthOld;

	if (isTeamFix != 1) return;	
    numLeagues = GetNumLeagues(_league->TeamNr);

	// #1 (TeamFix)
    for (i = 0; i <= _league->Leagues - 1; i++) {
		// Matching number of teams of each division
        if (numLeagues.num[i] != _league->league[i].Teams) {
			if (numLeagues.num[i] > _league->league[i].Teams)
				teamFixedup = 1;
			if (isTeamFix == 1) {			
				after  = numLeagues.num[i];
				before = _league->league[i].Teams;
				_league->league[i].Teams = numLeagues.num[i];
				logInfo(
					"Fix#1. TeamNumFix: %s, %s, %d is replaced with %d.", 
					league_name_table[_league->TeamNr],
					division_name_table[i],
					before, after
				);
			}
        }
    }
	
	// #2 (MonthFix)
	if (teamFixedup == 1) {
		bMonth = _league->BMonth / 8;
		eMonth = _league->EMonth / 8;
		bMonthOld = bMonth;
		eMonthOld = eMonth;
		
		if (bMonth < eMonth) {
			calendarType = 0;
		}
		else {
			calendarType = 1;
		}
		
		if (calendarType == 0) {	// YEARLY
			if (bMonth - 1 >= 0) {
				bMonth--;
			}
			else {
				eMonth++;
			}
		}
		else {						// SEASONAL
			if (bMonth - 1 >= 6) {
				bMonth--;
			}
			else {
				eMonth++;
			}
		}
		if (isMonthFix == 1) {
			_league->BMonth = bMonth * 8;
			_league->EMonth = eMonth * 8;
		
			if (bMonth != bMonthOld) {
				logInfo(
					"Fix#2. MonthFix: %s, BMonth %s is replaced with %s.",
					league_name_table[_league->TeamNr],
					month_name_table[bMonthOld], month_name_table[bMonth]
				);		
			}
			else {
				logInfo(
					"Fix#2. MonthFix: %s, EMonth %s is replaced with %s.",
					league_name_table[_league->TeamNr],
					month_name_table[eMonthOld], month_name_table[eMonth]
				);		
			}
		}
	}

	// #3 (MatEachFix)
	if (isEachMatchFix == 1) {
		byte MatEachFixedup = 0;
		byte oldMatEach;
		if (teamFixedup == 1) {
			for (i = 0; i <= _league->Leagues - 1; i++) {
				if (_league->league[i].Teams > 12) {
					MatEachFixedup = 1;
				}
			}
			if (MatEachFixedup == 1) {
				oldMatEach = _league->MatEach;
				_league->MatEach = 2;
				logInfo(
					"Fix#3. MatEachFix: %s, MatEach %d is replaced with %d.",
					league_name_table[_league->TeamNr],
					oldMatEach, _league->MatEach
				);
			}
		}
	}

	// #4 (greek_league fix)
	if (isGreekFix == 1) {
		if (_league->TeamNr == 15) {
			///*
			bMonthOld = _league->BMonth / 8;
			_league->BMonth = 7 * 8;  // 7(Oct)
			logInfo(
				"Fix#4. GreekFix: greek_league.BMonth = %s, EMonth = %s.",
				month_name_table[bMonthOld], month_name_table[_league->BMonth / 8]
			);
			//*/
			greek_cup.BMonth = 8 * 8;
			greek_cup.EMonth = 5 * 8;
			logInfo(
				"Fix#4. GreekFix: greek_cup.BMonth = September, EMonth = June."
			);
		}
	}

	// taiwanese_league fix for Sync1.2exp
	if (isTaiwaneseFix == 1) {
		if (_league->TeamNr == 67) {  // Taiwanese
			bMonthOld = _league->BMonth / 8;
			// _league->BMonth = 7 * 8;  // 5(Jun)
			
			_league->BMonth = 0 * 8;
			_league->EMonth = 11 * 8;
			_league->MatEach = 2;

			logInfo(
				" Special case fixed up: %s, BMonth %s is replaced with %s.",
				league_name_table[_league->TeamNr],
				month_name_table[bMonthOld], month_name_table[_league->BMonth / 8]
			);
		}
	}

	// san_marino_league fix for Sync1.2exp
	if (isSanMarinoFix == 1) {
		if (_league->TeamNr == 32) {  // SanMarino
			bMonthOld = _league->BMonth / 8;
			// _league->BMonth = 7 * 8;  // 7(Oct)

			_league->BMonth = 0 * 8;
			_league->EMonth = 11 * 8;
			_league->MatEach = 2;

			logInfo(
				" Special case fixed up: %s, BMonth %s is replaced with %s.",
				league_name_table[_league->TeamNr],
				month_name_table[bMonthOld], month_name_table[_league->BMonth / 8]
			);
		}
	}

	// spanish_leagues fix
	if (isSpanishFix == 1) {
		if (_league->TeamNr == 35) {
			bMonthOld = _league->BMonth / 8;
			_league->BMonth = 7 * 8;  // 7(Oct)
			logInfo(
				" Special case fixed up: %s, BMonth %s is replaced with %s.",
				league_name_table[_league->TeamNr],
				month_name_table[bMonthOld], month_name_table[_league->BMonth / 8]
			);
		}
	}
}

void FixupLeagueData()
{
	if (!isCareerCrashFix()) return;

	if (getCareerCrashFix(0) == 1) isTeamFix = 1;
	if (getCareerCrashFix(1) == 1) isMonthFix = 1;
	if (getCareerCrashFix(2) == 1) isEachMatchFix = 1;
	if (getCareerCrashFix(3) == 1) isGreekFix = 1;

	logInfo("Career Crash Fix#1: isTeamFix = %d\n", isTeamFix);
	logInfo("Career Crash Fix#2: isMonthFix = %d\n", isMonthFix);
	logInfo("Career Crash Fix#3: isEachMatchFix = %d\n", isEachMatchFix);
	logInfo("Career Crash Fix#4: isGreekFix = %d\n", isGreekFix);

    // Europe (43)   
    FixupLeague(&albanian_league);
    FixupLeague(&austrian_league);
    FixupLeague(&belarussian_league);
    FixupLeague(&belgian_league);
    FixupLeague(&bulgarian_league);
          
    FixupLeague(&croatian_league);  
    FixupLeague(&cypriot_league);
    FixupLeague(&czech_league);
    FixupLeague(&danish_league);
    FixupLeague(&english_leagues);
          
    FixupLeague(&estonian_league);  
    FixupLeague(&faroe_isles_league);
    FixupLeague(&finnish_league);
    FixupLeague(&frenche_leagues);
    FixupLeague(&german_leagues);
          
    FixupLeague(&greek_league);
    FixupLeague(&dutch_leagues);
    FixupLeague(&hungarian_league);
    FixupLeague(&icelandic_league);
    FixupLeague(&israeli_league);

    FixupLeague(&italian_leagues);
    FixupLeague(&latvian_league);
    FixupLeague(&lithuanian_league);
    FixupLeague(&luxembourg_league);
    FixupLeague(&maltese_league);

    FixupLeague(&northern_irish_league);
    FixupLeague(&norwegian_league);
    FixupLeague(&polish_league);
    FixupLeague(&portugese_league);
    FixupLeague(&irish_league);

    FixupLeague(&romanian_league); 
    FixupLeague(&russian_league);
    FixupLeague(&san_marino_league);
    FixupLeague(&scotland_leagues);
    FixupLeague(&slovakian_league);
          
    FixupLeague(&slovenian_league);
    FixupLeague(&spanish_leagues);
    FixupLeague(&swedish_league);
    FixupLeague(&swiss_league);
    FixupLeague(&turkish_league);
          
    FixupLeague(&ukrainian_league);
    FixupLeague(&welsh_league);
    FixupLeague(&yugoslavian_league);
          
    // North America (3)
    FixupLeague(&el_salvador_leagues);
    FixupLeague(&mexican_leagues);
    FixupLeague(&american_league);

    // South America (11)
    FixupLeague(&argentinian_league);
    FixupLeague(&bolivian_league);
    FixupLeague(&brasilian_league);
    FixupLeague(&chilean_league);
    FixupLeague(&colombian_league);
    FixupLeague(&ecuador_league);
    FixupLeague(&paraguayan_league);
    FixupLeague(&peruvian_league);
    FixupLeague(&surinam_league);
    FixupLeague(&uruguayan_league);
    FixupLeague(&venezuelan_league);

    // Africa (3)
    FixupLeague(&algerian_league);
    FixupLeague(&ghana_league);
    FixupLeague(&south_african_league);

    // Asia (3)
    FixupLeague(&indian_league);
    FixupLeague(&japan_leagues);
    FixupLeague(&taiwanese_league);
          
    // Oceania (2)
    FixupLeague(&australian_leagues);
    FixupLeague(&new_zealand_leagues);

    // National Teams
    // Africa
    // South America
    // North America
    // Asia
    // Oceania

    // Etc
    // customsLeague

	logInfo("Total number of teams for career is %d.", totalTeams);
}

// called from SWOS at the start
void SWOS::SWOS()
{
    init();

#if !defined(SWOS_TEST) && defined(NDEBUG)
    showImageReelsAndIntro();
#endif
//initIntroAnimation();
//PlayIntroAnimation();

    controlWord = 0;
    g_pl2Status = 0;
    g_pl1Status = 0;
    pl1FinalStatus = -1;
    pl2FinalStatus = -1;

    logInfo("Loading title and menu music");
    SAFE_INVOKE(LoadTitleAndStartMusic);
    initRandomSeed();

    someColorConversionFlag = 1;    // not sure what this is

    goalBasePtr = currentHilBuffer;
    nextGoalPtr = reinterpret_cast<dword *>(hilFileBuffer);

    loadFile(aSprite_dat, spritesIndex);
    loadFile(aCharset_dat, dosMemOfs30000h);

    logInfo("Fixing up initial charset");
    SAFE_INVOKE(FixupInitialCharset);
    D0 = 0;
    SAFE_INVOKE(ChainFont);
    SAFE_INVOKE(Randomize2);

    vsPtr = linAdr384k;
    screenWidth = kVgaWidth;
    spriteFixupFlag = 0;

    // flush controls
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);

#ifndef NDEBUG
    // verify that the string offsets are OK
    assert(!memcmp(aChairmanScenes + 0x4dc1, "YUGOSLAVIA", 11));
    assert(!memcmp(aChairmanScenes + 0x2e5, "THE PRESIDENT", 14));
    assert(!memcmp(aChairmanScenes + 0x2f, "RETURN TO GAME", 15));
    assert(!memcmp(aChairmanScenes + 0x30c, "EXCELLENT JOB", 13));
    assert(!memcmp(aChairmanScenes + 0x58c3, "QUEENSLAND", 11));
    assert(!memcmp(aChairmanScenes + 0x331, "APPRECIATE IT", 14));
    assert(!memcmp(aChairmanScenes + 0x3d1c, "DISK FULL", 9));
#endif

	FixupLeagueData();

#ifndef SWOS_TEST
    logInfo("Going to main menu");
    MainMenu();
#endif
}
