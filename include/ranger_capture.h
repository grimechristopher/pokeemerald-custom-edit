#ifndef GUARD_RANGER_CAPTURE_H
#define GUARD_RANGER_CAPTURE_H

#include "main.h"

// Minigame result states (stored in gRangerCaptureState)
#define RANGER_CAPTURE_IDLE     0
#define RANGER_CAPTURE_RUNNING  1
#define RANGER_CAPTURE_SUCCESS  2
#define RANGER_CAPTURE_FAIL     3

struct RangerCaptureParams
{
    u32 catchRate;
    u8  level;
    bool8 isIncapacitated; // asleep or frozen - eases difficulty
    bool8 isLowHp;           // under 25% max HP - eases difficulty
};

struct RangerDifficulty
{
    u8 loopsNeeded;
    u8 noteSpeed;
    u8 maxMisses;
    u8 attackNoteChance; // percent chance a spawned note is an attack note
};

struct RangerDifficulty ComputeRangerCaptureDifficulty(struct RangerCaptureParams params);

extern u8 gRangerCaptureState;
extern MainCallback gRangerCapture_ReturnCallback;

void RangerCapture_Init(void);

#endif // GUARD_RANGER_CAPTURE_H
