#pragma once

enum RegisterEnum
{
    kEax, kEbx, kEcx, kEdx, kEsi, kEdi, kEbp, kEsp,
    kD0, kD1, kD2, kD3, kD4, kD5, kD6, kD7, kA0, kA1, kA2, kA3, kA4, kA5, kA6,
    kNoReg,
    kAmigaRegsStart = kD0,
    kNumIntelRegisters = kAmigaRegsStart - kEax,
    kNumRegisters = kNoReg,
};
