#include "pitchUndo.h"
#include "pitch.h"
#include "pattern.h"
#include "alloc.h"
#include "util.h"
#include <assert.h>
#include <ctype.h>

typedef enum UndoType {
    kChangePattern,
    kDeletePattern,
    kOptimizePitch,
    kInsertNewTile,
} UndoType;

enum {
    kNumInlineNulledPatterns = 12,
};

typedef struct UndoRecord {
    UndoType type;
    struct UndoRecord *next;
    int groupId;        // for bulk operations such as "replace all patterns with #", it has to undo all
    bool groupLeader;
    byte changedFlags;
    __int16 numJoinedOperations;
} UndoRecord;

typedef struct PatternChangeUndoRecord {
    struct UndoRecord;
    int destPatternIndex;
    int oldPattern;
    int newPattern;
} PatternChangeUndoRecord;

typedef struct PatternDeletionUndoRecord {
    struct UndoRecord;
    int pitchNo;
    int deletedPatternNo;
    int numNulledPatterns;
    byte data[PATTERN_BYTE_SIZE];
    union {
        int nulledPatternsInline[kNumInlineNulledPatterns];
        int *nulledPatterns;
    };
} PatternDeletionUndoRecord;

typedef struct PitchOptimizationUndoRecord {
    struct UndoRecord;
    uint *ptrs;
    char *patternData;
    int *used;
    int numPatterns;
} PitchOptimizationUndoRecord;

typedef struct InsertNewTileUndoRecord {
    struct UndoRecord;
    int *numPatterns;
} InsertNewTileUndoRecord;

extern PitchInfo pt;

static UndoRecord *m_undoList[MAX_PITCH];
static UndoRecord *m_redoList[MAX_PITCH];

static int m_groupId;

static void undoChangePattern(PatternChangeUndoRecord *rec, char *data, uint *ptrs, char *msgBuf);
static void redoChangePattern(const PatternChangeUndoRecord *rec, char *data, uint *ptrs, char *msgBuf);
static void undoDeletePattern(const PatternDeletionUndoRecord *rec, PitchPatterns *patterns, char *msgBuf);
static void redoDeletePattern(const PatternDeletionUndoRecord *rec, char *msgBuf);
static void undoPitchOptimization(PitchOptimizationUndoRecord *rec, PitchPatterns *patterns, char *msgBuf);
static void redoOptimizePitch(PitchOptimizationUndoRecord *rec, PitchPatterns *patterns, char *msgBuf);
static void undoInsertNewTile(PitchPatterns *patterns, char *msgBuf);
static void redoInsertNewTile(InsertNewTileUndoRecord *rec, char *msgBuf);
static void swapPitchData(PitchOptimizationUndoRecord *rec, PitchPatterns *patterns);
static UndoRecord *allocUndoRecord(UndoType type, byte aggregated, byte changedFlags);
static void deleteRedoList(int pitchNo);
static void deleteAllUndoLists();
static void deleteAllRedoLists();
static void formatPatternChangedMessage(bool undo, int index, int oldNo, int newNo, char *buf);
static void freeDeletePatternRecord(PatternDeletionUndoRecord *rec);
static void freeOptimizePitchRecord(PitchOptimizationUndoRecord *rec);
static void addToList(UndoRecord *rec, UndoRecord **root);
static UndoRecord *removeFirstFromList(UndoRecord **root);
static void deleteList(UndoRecord **root);

bool undoPitchEdit(int pitchNo, PitchPatterns *patterns, char *msgBuf)
{
    if (!m_undoList[pitchNo])
        return false;

    UndoRecord *rec, *next;
    int numOps = 0;

    do {
        rec = removeFirstFromList(&m_undoList[pitchNo]);
        if (!rec)   // shouldn't happen
            return true;

        switch (rec->type) {
        case kChangePattern:
            if (rec->groupLeader)
                rec->numJoinedOperations = numOps;
            undoChangePattern((PatternChangeUndoRecord *)rec, patterns->data, patterns->ptrs, msgBuf);
            break;
        case kDeletePattern:
            undoDeletePattern((PatternDeletionUndoRecord *)rec, patterns, msgBuf);
            break;
        case kOptimizePitch:
            undoPitchOptimization((PitchOptimizationUndoRecord *)rec, patterns, msgBuf);
            break;
        case kInsertNewTile:
            undoInsertNewTile(patterns, msgBuf);
            break;
        default:
            assert(false);
        }

        next = rec->next;
        numOps++;

        if (rec->groupLeader) {
            g.changedFlags |= rec->changedFlags;
            g.changedFlags &= rec->changedFlags;
        }

        addToList(rec, &m_redoList[pitchNo]);
    } while (next && next->groupId == rec->groupId);

    return true;
}

bool redoPitchEdit(int pitchNo, PitchPatterns *patterns, char *msgBuf)
{
    if (!m_redoList[pitchNo])
        return false;

    UndoRecord *rec, *next;
    do {
        rec = removeFirstFromList(&m_redoList[pitchNo]);

        switch (rec->type) {
        case kChangePattern:
            redoChangePattern((PatternChangeUndoRecord *)rec, patterns->data, patterns->ptrs, msgBuf);
            break;
        case kDeletePattern:
            redoDeletePattern((PatternDeletionUndoRecord *)rec, msgBuf);
            break;
        case kOptimizePitch:
            redoOptimizePitch((PitchOptimizationUndoRecord *)rec, patterns, msgBuf);
            break;
        case kInsertNewTile:
            redoInsertNewTile((InsertNewTileUndoRecord *)rec, msgBuf);
            break;
        default:
            assert(false);
        }

        next = rec->next;
        addToList(rec, &m_undoList[pitchNo]);
    } while (next && next->groupId == rec->groupId);

    return true;
}

void clearUndoBuffers()
{
    deleteAllUndoLists();
    deleteAllRedoLists();
}

void logPatternChangedForUndo(int destPatternIndex, int oldPattern, int newPattern, byte aggregated, byte changedFlags)
{
    deleteRedoList(pt.curPitch);

    PatternChangeUndoRecord *rec = (PatternChangeUndoRecord *)allocUndoRecord(kChangePattern, aggregated, changedFlags);
    rec->destPatternIndex = destPatternIndex;
    rec->oldPattern = oldPattern;
    rec->newPattern = newPattern;

    addToList((UndoRecord *)rec, &m_undoList[pt.curPitch]);
}

void logPatternDeletionForUndo(int pitchNo, int patternNo, PitchPatterns *patterns, byte changedFlags)
{
    deleteRedoList(pitchNo);

    PatternDeletionUndoRecord *rec = (PatternDeletionUndoRecord *)allocUndoRecord(kDeletePattern, false, changedFlags);
    rec->pitchNo = pitchNo;
    rec->deletedPatternNo = patternNo;
    rec->numNulledPatterns = 0;
    rec->changedFlags = changedFlags;

    byte *patternData = (byte *)patterns->data + patternNo * PATTERN_BYTE_SIZE;
    memcpy(rec->data, patternData, PATTERN_BYTE_SIZE);

    for (int i = 0; i < patterns->numPtrs; i++)
        if (getPatternNoAtIndex(patterns, i) == patternNo)
            rec->numNulledPatterns++;

    int *originalIndices = rec->nulledPatternsInline;
    if (rec->numNulledPatterns > kNumInlineNulledPatterns)
        originalIndices = rec->nulledPatterns = xmalloc(rec->numNulledPatterns * sizeof(*rec->nulledPatterns));

    for (int i = 0, j = 0; i < patterns->numPtrs; i++)
        if (getPatternNoAtIndex(patterns, i) == patternNo)
            originalIndices[j++] = i;

    addToList((UndoRecord *)rec, &m_undoList[pitchNo]);
}

void logTileOptimizationForUndo(PitchPatterns *patterns, byte changedFlags)
{
    deleteRedoList(pt.curPitch);

    PitchOptimizationUndoRecord *rec = (PitchOptimizationUndoRecord *)allocUndoRecord(kOptimizePitch, false, changedFlags);
    rec->ptrs = patterns->ptrs;
    rec->patternData = (char *)patterns->data;
    rec->used = patterns->used;
    rec->numPatterns = patterns->numPatterns;

    addToList((UndoRecord *)rec, &m_undoList[pt.curPitch]);
}

void logInsertNewPatternForUndo(int pitchNo, PitchPatterns *patterns)
{
    deleteRedoList(pitchNo);
    InsertNewTileUndoRecord *rec = (InsertNewTileUndoRecord *)allocUndoRecord(kInsertNewTile, false, g.changedFlags);
    rec->numPatterns = &patterns->numPatterns;
    addToList((UndoRecord *)rec, &m_undoList[pitchNo]);
}

static void undoChangePattern(PatternChangeUndoRecord *rec, char *data, uint *ptrs, char *msgBuf)
{
    ptrs[rec->destPatternIndex] = rec->oldPattern * PATTERN_BYTE_SIZE + (uint)data;
    if (rec->groupLeader) {
        if (rec->numJoinedOperations > 0)
            wsprintf(msgBuf, "RESTORED %d TILES", rec->numJoinedOperations + 1);
        else
            formatPatternChangedMessage(true, rec->destPatternIndex, rec->newPattern, rec->oldPattern, msgBuf);
    }
}

static void redoChangePattern(const PatternChangeUndoRecord *rec, char *data, uint *ptrs, char *msgBuf)
{
    ptrs[rec->destPatternIndex] = rec->newPattern * PATTERN_BYTE_SIZE + (uint)data;

    if (rec->groupLeader) {
        if (rec->numJoinedOperations > 0)
            wsprintf(msgBuf, "REAPPLIED CHANGES TO %d TILES", rec->numJoinedOperations + 1);
        else
            formatPatternChangedMessage(false, rec->destPatternIndex, rec->oldPattern, rec->newPattern, msgBuf);
    }

    g.changedFlags |= PITCH_CHANGED;
}

static void undoDeletePattern(const PatternDeletionUndoRecord *rec, PitchPatterns *patterns, char *msgBuf)
{
    assert(rec->groupLeader && !rec->numJoinedOperations);

    byte *dataDest = (char *)patterns->data + PATTERN_BYTE_SIZE * rec->deletedPatternNo;
    if (rec->deletedPatternNo < (int)patterns->numPatterns++ - 1)
        memmove(dataDest + PATTERN_BYTE_SIZE, dataDest,
            (patterns->numPatterns - rec->deletedPatternNo - 1) * PATTERN_BYTE_SIZE);
    memcpy(dataDest, rec->data, PATTERN_BYTE_SIZE);

    for (int i = 0; i < patterns->numPtrs; i++)
        if (getPatternNoAtIndex(patterns, i) >= rec->deletedPatternNo)
            patterns->ptrs[i] += PATTERN_BYTE_SIZE;

    uint patternPtr = rec->deletedPatternNo * PATTERN_BYTE_SIZE + (uint)patterns->data;
    const int *indices = rec->numNulledPatterns > kNumInlineNulledPatterns ?
        rec->nulledPatterns : rec->nulledPatternsInline;
    for (int i = 0; i < rec->numNulledPatterns; i++)
        patterns->ptrs[*indices++] = patternPtr;

    wsprintf(msgBuf, "TILE %d RESTORED", rec->deletedPatternNo);
}

static void redoDeletePattern(const PatternDeletionUndoRecord *rec, char *msgBuf)
{
    assert(rec->groupLeader && !rec->numJoinedOperations);

    deletePatternWithoutUndo(rec->pitchNo, rec->deletedPatternNo);
    wsprintf(msgBuf, "TILE %d DELETED AGAIN", rec->deletedPatternNo);
}

static void undoPitchOptimization(PitchOptimizationUndoRecord *rec, PitchPatterns *patterns, char *msgBuf)
{
    assert(rec->groupLeader && !rec->numJoinedOperations);
    swapPitchData(rec, patterns);
    strcpy(msgBuf, "PITCH DE-OPTIMIZED.");
}

static void redoOptimizePitch(PitchOptimizationUndoRecord *rec, PitchPatterns *patterns, char *msgBuf)
{
    assert(rec->groupLeader && !rec->numJoinedOperations);
    swapPitchData(rec, patterns);
    strcpy(msgBuf, "PITCH RE-OPTIMIZED.");
    g.changedFlags |= PITCH_CHANGED | PATTERNS_CHANGED;
}

static void undoInsertNewTile(PitchPatterns *patterns, char *msgBuf)
{
    patterns->numPatterns--;
    strcpy(msgBuf, "NEW PATTERN REMOVED");
}

static void redoInsertNewTile(InsertNewTileUndoRecord *rec, char *msgBuf)
{
    assert(rec->groupLeader && !rec->numJoinedOperations);
    ++*rec->numPatterns;
    strcpy(msgBuf, "NEW PATTERN RESTORED");
    g.changedFlags |= PATTERNS_CHANGED;
}

static void swapPitchData(PitchOptimizationUndoRecord *rec, PitchPatterns *patterns)
{
    swap(patterns->ptrs, rec->ptrs);
    swap((char *)patterns->data, rec->patternData);
    swap(patterns->numPatterns, rec->numPatterns);
    swap(patterns->used, rec->used);
    resetPitchData(false);
}

static UndoRecord *allocUndoRecord(UndoType type, byte aggregated, byte changedFlags)
{
    UndoRecord *rec;
    int size;

    switch (type) {
    case kChangePattern:
        size = sizeof(PatternChangeUndoRecord);
        break;
    case kDeletePattern:
        size = sizeof(PatternDeletionUndoRecord);
        break;
    case kOptimizePitch:
        size = sizeof(PitchOptimizationUndoRecord);
        break;
    case kInsertNewTile:
        size = sizeof(InsertNewTileUndoRecord);
        break;
    default:
        assert(false);
        return nullptr;
    }

    rec = xmalloc(size);
    rec->type = type;
    rec->changedFlags = changedFlags;
    if (!aggregated)
        m_groupId++;
    rec->groupLeader = !aggregated;
    rec->groupId = m_groupId;
    rec->numJoinedOperations = 0;

    return rec;
}

static void deleteRedoList(int pitchNo)
{
    deleteList(&m_redoList[pitchNo]);
}

static void deleteAllUndoLists()
{
    for (int i = 0; i < sizeofarray(m_undoList); i++)
        deleteList(&m_undoList[i]);
}

static void deleteAllRedoLists()
{
    for (int i = 0; i < sizeofarray(m_redoList); i++)
        deleteList(&m_redoList[i]);
}

static void formatPatternChangedMessage(bool undo, int index, int fromNo, int toNo, char *buf)
{
    int x = (index - START_PATTERN) / kPatternsPerWidth;
    int y = (index - START_PATTERN) % kPatternsPerWidth;
    assert(x >= 0 && y >= 0);
    wsprintf(buf, "%s:\nTILE AT %d (%d, %d)\nFROM %d TO %d", undo ? "UNDO" : "REDO", index, x, y, fromNo, toNo);
}

static void freeDeletePatternRecord(PatternDeletionUndoRecord *rec)
{
    if (rec->numNulledPatterns > kNumInlineNulledPatterns)
        xfree(rec->nulledPatterns);
}

static void freeOptimizePitchRecord(PitchOptimizationUndoRecord *rec)
{
    xfree(rec->ptrs);
    xfree(rec->patternData);
    xfree(rec->used);
}

static void addToList(UndoRecord *rec, UndoRecord **root)
{
    assert(rec && root);
    rec->next = *root;
    *root = rec;
}

static UndoRecord *removeFirstFromList(UndoRecord **root)
{
    assert(root && *root);
    UndoRecord *rec = *root;
    *root = (*root)->next;
    return rec;
}

static void deleteList(UndoRecord **root)
{
    UndoRecord *rec = *root;
    while (rec) {
        UndoRecord *next = rec->next;
        if (rec->type == kDeletePattern)
            freeDeletePatternRecord((PatternDeletionUndoRecord *)rec);
        else if (rec->type == kOptimizePitch)
            freeOptimizePitchRecord((PitchOptimizationUndoRecord *)rec);
        xfree(rec);
        rec = next;
    }
    *root = nullptr;
}
