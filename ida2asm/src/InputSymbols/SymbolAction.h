#pragma once

enum SymbolAction : uint16_t {
    kNone = 0,
    kRemove = 1,
    kNull = 2,
    kExport = 4,
    kImport = 8,
    kSaveCppRegisters = 16,
    kOnEnter = 32,
    kReplace = 64,
    kInsertCall = 128,
    kRemoveEndRange = 256,
    kRemoveSolo = 512,
    kTypeSizes = 1024,
};

ENABLE_FLAGS(SymbolAction)
