#pragma once

#include <assert.h>

#define COLOR_TABLE_SIZE 27
#define START_PATTERN 42  /* starting pattern (rows 1&2 = special animated patterns, not counted as pitch) */

#define PATTERN_LENGTH 16
#define PATTERN_BYTE_SIZE (PATTERN_LENGTH * PATTERN_LENGTH)

#define PITCH_PATTERN_W (PITCH_W / PATTERN_LENGTH)
#define PITCH_PATTERN_H (PITCH_H / PATTERN_LENGTH)

/* information about patterns in each pitch */
typedef struct _PitchPatterns {
    uint *ptrs;       /* array of pointers to patterns (index file)        */
    char *data;       /* actual pattern data                               */
    int  *used;       // number of timers pattern used in the pitch
    int  numPtrs;
    int numPatterns;
    uchar changed;
} PitchPatterns;

/* global patterns informations */
typedef struct _Patterns_info {
    schar curPitch;
    short curPattern;
} Patterns_info;

void DrawPattern(char *where, uint pitch);
void CopyPattern(char *from, char *where, uint pitch);
void deletePattern(int pitchNo, int patternNo);
void deletePatternWithoutUndo(int pitchNo, int patternNo);
void showPattern(int pitchNo, int patternNo);
bool SaveChangesToPatterns(int forcedSavePitchNo);
byte *PatternsGetPalette(void);
bool LoadPitch(int pitchNo, char *errorBuf);
bool saveIndexFile(int pitchNo);

// always use these macros for pattern access, it's absolutely the safest way
#define getPatternNoAtIndex(patterns, i) \
    (int)(assert((patterns) && (int)(i) >= 0 && (int)(i) < (int)((patterns)->numPtrs) && \
    (int)(((patterns)->ptrs[i] - (uint)((patterns)->data)) / PATTERN_BYTE_SIZE) >= 0 && \
    (int)(((patterns)->ptrs[i] - (uint)((patterns)->data)) / PATTERN_BYTE_SIZE) < (patterns)->numPatterns), \
    ((patterns)->ptrs[i] - (uint)((patterns)->data)) / PATTERN_BYTE_SIZE)

#define getPatternNoFromPtr(patterns, ptr) \
    (int)(assert((patterns) && (ptr) && \
    (char *)(ptr) >= (char *)(patterns)->ptrs && \
    (char *)(ptr) < (char *)(patterns)->ptrs + (patterns)->numPtrs * sizeof((patterns)->ptrs[0]) && \
    (int)((*(uint *)(ptr) - (uint)(patterns)->data) / PATTERN_BYTE_SIZE) >= 0 && \
    (int)((*(uint *)(ptr) - (uint)(patterns)->data) / PATTERN_BYTE_SIZE) < (patterns)->numPatterns), \
    (*(uint *)(ptr) - (uint)(patterns)->data) / PATTERN_BYTE_SIZE)

extern const Mode PatternMode;
