#include "global.h"
#include "test/test.h"
#include "ranger_capture.h"

static struct RangerCaptureParams BaseParams(u32 catchRate)
{
    struct RangerCaptureParams params = {0};
    params.catchRate = catchRate;
    params.level = 5;
    return params;
}

TEST("RangerCapture: high catch rate is the easiest tier")
{
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(BaseParams(150));
    EXPECT_EQ(diff.loopsNeeded, 3);
    EXPECT_EQ(diff.noteSpeed, 6);
    EXPECT_EQ(diff.maxMisses, 5);
    EXPECT_EQ(diff.attackNoteChance, 10);
}

TEST("RangerCapture: low catch rate is the hardest tier")
{
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(BaseParams(10));
    EXPECT_EQ(diff.loopsNeeded, 6);
    EXPECT_EQ(diff.noteSpeed, 3);
    EXPECT_EQ(diff.maxMisses, 2);
    EXPECT_EQ(diff.attackNoteChance, 40);
}

TEST("RangerCapture: level 50+ speeds up notes by one")
{
    struct RangerCaptureParams params = BaseParams(150);
    params.level = 50;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 5);
}

TEST("RangerCapture: level 80+ speeds up notes by two")
{
    struct RangerCaptureParams params = BaseParams(150);
    params.level = 80;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 4);
}

TEST("RangerCapture: level speedup never drops speed below 3")
{
    struct RangerCaptureParams params = BaseParams(10); // already at noteSpeed 3
    params.level = 80;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 3);
}

TEST("RangerCapture: incapacitated target eases note speed by one")
{
    struct RangerCaptureParams params = BaseParams(150);
    params.isIncapacitated = TRUE;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 7);
}

TEST("RangerCapture: low HP eases note speed by one")
{
    struct RangerCaptureParams params = BaseParams(150);
    params.isLowHp = TRUE;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 7);
}

TEST("RangerCapture: incapacitated and low HP eases stack")
{
    struct RangerCaptureParams params = BaseParams(150);
    params.isIncapacitated = TRUE;
    params.isLowHp = TRUE;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 8);
}
