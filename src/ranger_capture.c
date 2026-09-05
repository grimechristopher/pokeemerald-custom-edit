#include "global.h"
#include "main.h"
#include "malloc.h"
#include "task.h"
#include "bg.h"
#include "window.h"
#include "text.h"
#include "sound.h"
#include "gpu_regs.h"
#include "palette.h"
#include "string_util.h"
#include "random.h"
#include "scanline_effect.h"
#include "menu.h"
#include "battle.h"
#include "pokemon.h"
#include "overworld.h"
#include "ow_abilities.h"
#include "constants/songs.h"
#include "constants/battle.h"
#include "constants/rgb.h"
#include "constants/ranger_capture.h"
#include "gba/io_reg.h"
#include "ranger_capture.h"

// ---- Layout constants ----
#define SCREEN_TILE_W  30
#define SCREEN_TILE_H  20

// Left info panel: tile columns 0-7
#define PANEL_LEFT_W   8

// Lane area: tile columns 8-27
#define LANE_START_COL 8
#define LANE_END_COL   27
#define HIT_ZONE_COL   26

// Note rows (each lane is 2 tiles tall, with 1-tile borders above/below)
#define LANE_UP_ROW    4
#define LANE_RT_ROW    8
#define LANE_DN_ROW    12
#define LANE_LT_ROW    16

// Lane indices
#define LANE_UP    0
#define LANE_RIGHT 1
#define LANE_DOWN  2
#define LANE_LEFT  3
#define LANE_COUNT 4

// Max notes on screen
#define MAX_NOTES  8

// Note states
#define NOTE_INACTIVE 0
#define NOTE_ACTIVE   1

// Note kinds
#define NOTE_KIND_NORMAL 0
#define NOTE_KIND_ATTACK 1

// Hit result values
#define HIT_PERFECT 0
#define HIT_GOOD    1
#define HIT_OK      2
#define HIT_MISS    3

// Loop progress
#define LOOP_PROGRESS_MAX 100

// Ranger state machine states (internal)
#define RSTATE_INIT        0
#define RSTATE_SETUP_GFX   1
#define RSTATE_COUNTDOWN   2
#define RSTATE_PLAYING     3
#define RSTATE_SUCCESS_ANIM 4
#define RSTATE_FAIL_ANIM   5
#define RSTATE_FADE_OUT    6
#define RSTATE_EXIT        7

// Window IDs
#define WIN_TITLE    0
#define WIN_LOOPS    1
#define WIN_FEEDBACK 2
#define WIN_CNT      3

// Inline tile indices
#define TILE_BLACK     0
#define TILE_LANE_BG   1
#define TILE_NOTE_UP   2
#define TILE_NOTE_RT   3
#define TILE_NOTE_DN   4
#define TILE_NOTE_LT   5
#define TILE_HIT_ZONE  6
#define TILE_BORDER    7
#define TILE_METER_ON  8
#define TILE_METER_OFF 9
#define TILE_NOTE_ATTACK 10
#define TILE_COUNT     11

// Palette color indices (in BG palette 0)
#define COL_BLACK     1
#define COL_DARK_GRAY 2
#define COL_YELLOW    3
#define COL_GREEN     4
#define COL_BLUE      5
#define COL_RED       6
#define COL_WHITE     7
#define COL_ORANGE    8
#define COL_METER_ON  9
#define COL_METER_OFF 10
#define COL_ATTACK    11

// Notes spawned per loop total (spread across lanes)
#define NOTES_PER_LOOP_TOTAL (LANE_COUNT * 3)

// Frame constants
#define FEEDBACK_DURATION     30
#define COUNTDOWN_DURATION   180
#define SUCCESS_ANIM_DURATION 120
#define FAIL_ANIM_DURATION     90

// Spacing between note spawns (in frames)
#define NOTE_SPAWN_INTERVAL(spd) ((spd) * 8)

struct RangerNote {
    u8  lane;
    s16 tileCol;
    u8  state;
    u8  kind;
};

struct RangerCapture {
    u8  rstate;
    u8  resultMode;
    u8  loopsNeeded;
    u8  loopsCompleted;
    u8  maxMisses;
    u8  missCount;
    u8  noteSpeed;
    u8  attackNoteChance;
    u8  moveTimer;
    u16 countdownTimer;
    s16 loopProgress;
    u8  feedbackTimer;
    u16 animTimer;
    u16 noteSpawnTimer;
    u8  notesSpawnedThisLoop;
    u16 tilemapBuffer[32 * 32];
    struct RangerNote notes[MAX_NOTES];
};

// ---- Globals ----
u8 gRangerCaptureState;
MainCallback gRangerCapture_ReturnCallback;

EWRAM_DATA static struct RangerCapture *sRanger = NULL;

static enum Species sStagedStylerSpecies;
static u8 sStagedStylerLevel;
static u8 sStylerCaptureOutcome;

// ---- Forward declarations ----
static void RangerCapture_VBlankCB(void);
static void RangerCapture_MainCB(void);
static void Task_RangerCapture(u8 taskId);

// ---- Inline solid-color tile generator ----
// 4bpp: each byte holds 2 pixels. For color c, byte = c | (c << 4).
// Each u32 = 4 bytes (8 pixels). Each tile = 8 rows of 8 pixels.
#define MAKE_BYTE(c)  ((u32)((c) | ((c) << 4)))
#define MAKE_ROW(c)   (MAKE_BYTE(c) | (MAKE_BYTE(c) << 8) | (MAKE_BYTE(c) << 16) | (MAKE_BYTE(c) << 24))
#define SOLID_TILE(c) { \
    MAKE_ROW(c), MAKE_ROW(c), MAKE_ROW(c), MAKE_ROW(c), \
    MAKE_ROW(c), MAKE_ROW(c), MAKE_ROW(c), MAKE_ROW(c)  \
}

static const u32 sRangerBgTiles[TILE_COUNT][8] = {
    [TILE_BLACK]    = SOLID_TILE(COL_BLACK),
    [TILE_LANE_BG]  = SOLID_TILE(COL_DARK_GRAY),
    [TILE_NOTE_UP]  = SOLID_TILE(COL_YELLOW),
    [TILE_NOTE_RT]  = SOLID_TILE(COL_GREEN),
    [TILE_NOTE_DN]  = SOLID_TILE(COL_BLUE),
    [TILE_NOTE_LT]  = SOLID_TILE(COL_RED),
    [TILE_HIT_ZONE] = SOLID_TILE(COL_WHITE),
    [TILE_BORDER]   = SOLID_TILE(COL_ORANGE),
    [TILE_METER_ON] = SOLID_TILE(COL_METER_ON),
    [TILE_METER_OFF]= SOLID_TILE(COL_METER_OFF),
    [TILE_NOTE_ATTACK] = SOLID_TILE(COL_ATTACK),
};

// 16-color BG palette (palette slot 0, colors 0..15)
static const u16 sRangerBgPal[16] = {
    RGB(0,  0,  0),   //  0: transparent
    RGB(0,  0,  0),   //  1: COL_BLACK
    RGB(10, 10, 10),  //  2: COL_DARK_GRAY
    RGB(31, 31, 0),   //  3: COL_YELLOW  (UP)
    RGB(0,  25, 0),   //  4: COL_GREEN   (RIGHT)
    RGB(0,  0,  31),  //  5: COL_BLUE    (DOWN)
    RGB(31, 0,  0),   //  6: COL_RED     (LEFT)
    RGB(31, 31, 31),  //  7: COL_WHITE   (hit zone)
    RGB(31, 16, 0),   //  8: COL_ORANGE  (border)
    RGB(31, 28, 0),   //  9: COL_METER_ON
    RGB(8,  8,  8),   // 10: COL_METER_OFF
    RGB(20, 0, 20),   // 11: COL_ATTACK
    RGB(0,  0,  0),
    RGB(0,  0,  0),
    RGB(0,  0,  0),
    RGB(0,  0,  0),
};

// BG template
static const struct BgTemplate sRangerBgTemplate = {
    .bg            = 0,
    .charBaseIndex = 0,
    .mapBaseIndex  = 31,
    .screenSize    = 0,
    .paletteMode   = 0,
    .priority      = 0,
    .baseTile      = 0,
};

// Window templates
static const struct WindowTemplate sRangerWinTemplates[WIN_CNT + 1] = {
    [WIN_TITLE] = {
        .bg         = 0,
        .tilemapLeft = 0,
        .tilemapTop  = 0,
        .width      = 8,
        .height     = 3,
        .paletteNum = 15,
        .baseBlock  = 128,
    },
    [WIN_LOOPS] = {
        .bg         = 0,
        .tilemapLeft = 0,
        .tilemapTop  = 6,
        .width      = 7,
        .height     = 2,
        .paletteNum = 15,
        .baseBlock  = 128 + 8 * 3,
    },
    [WIN_FEEDBACK] = {
        .bg         = 0,
        .tilemapLeft = 16,
        .tilemapTop  = 9,
        .width      = 12,
        .height     = 2,
        .paletteNum = 15,
        .baseBlock  = 128 + 8 * 3 + 7 * 2,
    },
    [WIN_CNT] = DUMMY_WIN_TEMPLATE,
};

// ---- Text strings ----
static const u8 sText_RangerStyler[]  = _("RANGER STYLER");
static const u8 sText_Loops[]         = _("LOOPS:");
static const u8 sText_Perfect[]       = _("PERFECT!");
static const u8 sText_Good[]          = _("GOOD!");
static const u8 sText_Ok[]            = _("OK!");
static const u8 sText_Miss[]          = _("MISS!");
static const u8 sText_Countdown3[]    = _("  3");
static const u8 sText_Countdown2[]    = _("  2");
static const u8 sText_Countdown1[]    = _("  1");
static const u8 sText_Go[]            = _("  GO!");
static const u8 sText_Caught[]        = _("Caught!");
static const u8 sText_BrokeFree[]     = _("Broke free!");

// ---- VBlank callback ----
static void RangerCapture_VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

// ---- Main callback ----
static void RangerCapture_MainCB(void)
{
    RunTasks();
    UpdatePaletteFade();
}

// ---- Entry point called by SetMainCallback2 ----
static void RangerCapture_InitCommon(u8 resultMode)
{
    SetVBlankCallback(NULL);
    ResetTasks();
    ResetSpriteData();
    ResetPaletteFade();
    FreeAllSpritePalettes();
    ScanlineEffect_Stop();

    sRanger = AllocZeroed(sizeof(struct RangerCapture));
    sRanger->resultMode = resultMode;

    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BG0CNT, 0);
    SetGpuReg(REG_OFFSET_BG1CNT, 0);
    SetGpuReg(REG_OFFSET_BG2CNT, 0);
    SetGpuReg(REG_OFFSET_BG3CNT, 0);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);

    sRanger->rstate = RSTATE_INIT;
    CreateTask(Task_RangerCapture, 0);

    SetVBlankCallback(RangerCapture_VBlankCB);
    SetMainCallback2(RangerCapture_MainCB);
}

void RangerCapture_Init(void)
{
    RangerCapture_InitCommon(RANGER_RESULT_MODE_BATTLE);
}

void RangerCapture_InitStandalone(void)
{
    RangerCapture_InitCommon(RANGER_RESULT_MODE_SCRIPTED);
}

// Must be called before RangerCapture_InitStandalone (i.e. before dostylercapture runs)
// so the scripted encounter knows which species/level to stage - mirrors setwildbattle's
// existing contract with its own wild battle command.
void SetStagedStylerCaptureMon(enum Species species, u8 level)
{
    sStagedStylerSpecies = species;
    sStagedStylerLevel = level;
}

u8 GetStylerCaptureOutcome(void)
{
    return sStylerCaptureOutcome;
}

// ---- Helpers ----

static u8 GetLaneRow(u8 lane)
{
    switch (lane)
    {
    case LANE_UP:    return LANE_UP_ROW;
    case LANE_RIGHT: return LANE_RT_ROW;
    case LANE_DOWN:  return LANE_DN_ROW;
    default:         return LANE_LT_ROW;
    }
}

static u8 GetNoteTile(u8 lane, u8 kind)
{
    if (kind == NOTE_KIND_ATTACK)
        return TILE_NOTE_ATTACK;

    switch (lane)
    {
    case LANE_UP:    return TILE_NOTE_UP;
    case LANE_RIGHT: return TILE_NOTE_RT;
    case LANE_DOWN:  return TILE_NOTE_DN;
    default:         return TILE_NOTE_LT;
    }
}

static void SetTile(u8 col, u8 row, u8 tileIdx)
{
    sRanger->tilemapBuffer[row * 32 + col] = tileIdx;
}

static u8 LaneRestoreTile(s16 col)
{
    return (col == HIT_ZONE_COL) ? TILE_HIT_ZONE : TILE_LANE_BG;
}

// ---- Tilemap building ----

static void BuildInitialTilemap(void)
{
    u32 r, c, lane;

    // Black background everywhere
    for (r = 0; r < 20; r++)
        for (c = 0; c < 30; c++)
            SetTile(c, r, TILE_BLACK);

    // Left panel: dark gray
    for (r = 0; r < 20; r++)
        for (c = 0; c < PANEL_LEFT_W; c++)
            SetTile(c, r, TILE_LANE_BG);

    // Lane rows (4 lanes, each occupying row, row+1 with borders)
    for (lane = 0; lane < LANE_COUNT; lane++)
    {
        u8 row = GetLaneRow(lane);

        // Top border row
        for (c = LANE_START_COL; c <= LANE_END_COL; c++)
            SetTile(c, row - 1, TILE_BORDER);

        // 2-row lane background
        for (c = LANE_START_COL; c <= LANE_END_COL; c++)
        {
            SetTile(c, row,     TILE_LANE_BG);
            SetTile(c, row + 1, TILE_LANE_BG);
        }

        // Bottom border row
        for (c = LANE_START_COL; c <= LANE_END_COL; c++)
            SetTile(c, row + 2, TILE_BORDER);
    }

    // Hit zone: white tiles in all lane rows
    for (lane = 0; lane < LANE_COUNT; lane++)
    {
        u8 row = GetLaneRow(lane);
        SetTile(HIT_ZONE_COL, row,     TILE_HIT_ZONE);
        SetTile(HIT_ZONE_COL, row + 1, TILE_HIT_ZONE);
    }
}

// ---- Loop meter (8 tiles wide in left panel, rows 10-11) ----
static void UpdateLoopMeter(void)
{
    u32 i;
    u32 filled = (u32)(sRanger->loopProgress) * 8 / LOOP_PROGRESS_MAX;
    for (i = 0; i < 8; i++)
        SetTile(i, 10, (i < filled) ? TILE_METER_ON : TILE_METER_OFF);
    for (i = 0; i < 8; i++)
        SetTile(i, 11, (i < filled) ? TILE_METER_ON : TILE_METER_OFF);
}

// ---- Text printing helpers ----
static void PrintTitle(void)
{
    u8 color[3] = {0, 2, 3};
    FillWindowPixelBuffer(WIN_TITLE, PIXEL_FILL(0));
    AddTextPrinterParameterized4(WIN_TITLE, FONT_SMALL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_RangerStyler);
    CopyWindowToVram(WIN_TITLE, COPYWIN_FULL);
    PutWindowTilemap(WIN_TITLE);
}

static void PrintLoopCount(void)
{
    u8 color[3] = {0, 2, 3};
    u8 buf[16];
    u8 *ptr = buf;

    FillWindowPixelBuffer(WIN_LOOPS, PIXEL_FILL(0));
    AddTextPrinterParameterized4(WIN_LOOPS, FONT_SMALL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_Loops);

    ptr = ConvertIntToDecimalStringN(ptr, sRanger->loopsCompleted, STR_CONV_MODE_LEFT_ALIGN, 1);
    *ptr++ = CHAR_SLASH;
    ptr = ConvertIntToDecimalStringN(ptr, sRanger->loopsNeeded, STR_CONV_MODE_LEFT_ALIGN, 1);
    *ptr = EOS;

    AddTextPrinterParameterized4(WIN_LOOPS, FONT_SMALL, 44, 0, 0, 0, color, TEXT_SKIP_DRAW, buf);
    CopyWindowToVram(WIN_LOOPS, COPYWIN_FULL);
    PutWindowTilemap(WIN_LOOPS);
}

static void PrintFeedback(u8 hitResult)
{
    u8 color[3] = {0, 7, 0};
    const u8 *text;

    switch (hitResult)
    {
    case HIT_PERFECT: text = sText_Perfect; break;
    case HIT_GOOD:    text = sText_Good;    break;
    case HIT_OK:      text = sText_Ok;      break;
    default:          text = sText_Miss;    break;
    }

    FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
    AddTextPrinterParameterized4(WIN_FEEDBACK, FONT_SMALL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, text);
    CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
    PutWindowTilemap(WIN_FEEDBACK);
}

static void ClearFeedback(void)
{
    FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
    CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
}

// ---- Difficulty calculation ----

struct RangerDifficulty ComputeRangerCaptureDifficulty(struct RangerCaptureParams params)
{
    struct RangerDifficulty diff;

    if (params.catchRate >= 150)
    {
        diff.loopsNeeded = 3;
        diff.noteSpeed = 6;
        diff.maxMisses = 5;
        diff.attackNoteChance = 10;
    }
    else if (params.catchRate >= 100)
    {
        diff.loopsNeeded = 4;
        diff.noteSpeed = 5;
        diff.maxMisses = 4;
        diff.attackNoteChance = 20;
    }
    else if (params.catchRate >= 45)
    {
        diff.loopsNeeded = 5;
        diff.noteSpeed = 4;
        diff.maxMisses = 3;
        diff.attackNoteChance = 30;
    }
    else
    {
        diff.loopsNeeded = 6;
        diff.noteSpeed = 3;
        diff.maxMisses = 2;
        diff.attackNoteChance = 40;
    }

    // Higher-level targets push the tempo up, same direction a lower catch rate does.
    if (params.level >= 50 && diff.noteSpeed > 3)
        diff.noteSpeed--;
    if (params.level >= 80 && diff.noteSpeed > 3)
        diff.noteSpeed--;

    // Easier if asleep/frozen
    if (params.isIncapacitated)
        diff.noteSpeed++;

    // Easier if low HP
    if (params.isLowHp)
        diff.noteSpeed++;

    return diff;
}

static void CalculateDifficultyForMode(void)
{
    struct RangerCaptureParams params = {0};
    struct RangerDifficulty diff;

    if (sRanger->resultMode == RANGER_RESULT_MODE_SCRIPTED)
    {
        params.catchRate = gSpeciesInfo[sStagedStylerSpecies].catchRate;
        params.level = sStagedStylerLevel;
        // No live battle mon to read status/HP from for a scripted encounter.
        params.isIncapacitated = FALSE;
        params.isLowHp = FALSE;
    }
    else
    {
        params.catchRate = gSpeciesInfo[gBattleMons[gBattlerTarget].species].catchRate;
        params.level = gBattleMons[gBattlerTarget].level;
        params.isIncapacitated = (gBattleMons[gBattlerTarget].status1 & STATUS1_INCAPACITATED) != 0;
        params.isLowHp = gBattleMons[gBattlerTarget].hp * 4 < gBattleMons[gBattlerTarget].maxHP;
    }

    diff = ComputeRangerCaptureDifficulty(params);
    sRanger->loopsNeeded = diff.loopsNeeded;
    sRanger->noteSpeed = diff.noteSpeed;
    sRanger->maxMisses = diff.maxMisses;
    sRanger->attackNoteChance = diff.attackNoteChance;
}

// ---- Note management ----
static void SpawnNote(u8 lane, u8 kind)
{
    u32 i;
    for (i = 0; i < MAX_NOTES; i++)
    {
        if (sRanger->notes[i].state == NOTE_INACTIVE)
        {
            sRanger->notes[i].lane    = lane;
            sRanger->notes[i].tileCol = LANE_START_COL;
            sRanger->notes[i].state   = NOTE_ACTIVE;
            sRanger->notes[i].kind    = kind;
            return;
        }
    }
}

static void UpdateNotes(void)
{
    u32 i;

    // Spawn notes periodically
    sRanger->noteSpawnTimer++;
    if (sRanger->noteSpawnTimer >= NOTE_SPAWN_INTERVAL(sRanger->noteSpeed))
    {
        sRanger->noteSpawnTimer = 0;
        if (sRanger->notesSpawnedThisLoop < NOTES_PER_LOOP_TOTAL)
        {
            u8 lane = sRanger->notesSpawnedThisLoop % LANE_COUNT;
            u8 kind = (Random() % 100 < sRanger->attackNoteChance) ? NOTE_KIND_ATTACK : NOTE_KIND_NORMAL;
            SpawnNote(lane, kind);
            sRanger->notesSpawnedThisLoop++;
        }
    }

    // Move notes every noteSpeed frames
    sRanger->moveTimer++;
    if (sRanger->moveTimer < sRanger->noteSpeed)
        return;
    sRanger->moveTimer = 0;

    for (i = 0; i < MAX_NOTES; i++)
    {
        if (sRanger->notes[i].state != NOTE_ACTIVE)
            continue;

        u8  row  = GetLaneRow(sRanger->notes[i].lane);
        s16 col  = sRanger->notes[i].tileCol;

        // Erase old position
        if (col >= LANE_START_COL && col <= LANE_END_COL)
        {
            SetTile(col, row,     LaneRestoreTile(col));
            SetTile(col, row + 1, LaneRestoreTile(col));
        }

        col++;
        sRanger->notes[i].tileCol = col;

        if (col > LANE_END_COL)
        {
            sRanger->notes[i].state = NOTE_INACTIVE;

            if (sRanger->notes[i].kind == NOTE_KIND_ATTACK)
            {
                // Letting an attack note through is correct play - small reward, no penalty.
                sRanger->loopProgress += 3;
                if (sRanger->loopProgress > LOOP_PROGRESS_MAX)
                    sRanger->loopProgress = LOOP_PROGRESS_MAX;
                UpdateLoopMeter();
            }
            else
            {
                // Missed
                sRanger->missCount++;
                sRanger->loopProgress -= 15;
                if (sRanger->loopProgress < 0)
                    sRanger->loopProgress = 0;
                PrintFeedback(HIT_MISS);
                sRanger->feedbackTimer = FEEDBACK_DURATION;
                UpdateLoopMeter();
                PlaySE(SE_BALL_BOUNCE_4);
            }
        }
        else
        {
            // Draw at new position
            u8 noteTile = GetNoteTile(sRanger->notes[i].lane, sRanger->notes[i].kind);
            SetTile(col, row,     noteTile);
            SetTile(col, row + 1, noteTile);
        }
    }
}

// ---- Input handling ----
static void HandleInput(void)
{
    u8 pressedLane;

    if      (JOY_NEW(DPAD_UP))    pressedLane = LANE_UP;
    else if (JOY_NEW(DPAD_RIGHT)) pressedLane = LANE_RIGHT;
    else if (JOY_NEW(DPAD_DOWN))  pressedLane = LANE_DOWN;
    else if (JOY_NEW(DPAD_LEFT))  pressedLane = LANE_LEFT;
    else                          return;

    // Find nearest active note in the pressed lane
    u32 i;
    s32 bestDelta = 100;
    u32 bestIdx   = MAX_NOTES;

    for (i = 0; i < MAX_NOTES; i++)
    {
        if (sRanger->notes[i].state == NOTE_ACTIVE
         && sRanger->notes[i].lane == pressedLane)
        {
            s32 d = sRanger->notes[i].tileCol - HIT_ZONE_COL;
            if (d < 0) d = -d;
            if (d < bestDelta) { bestDelta = d; bestIdx = i; }
        }
    }

    u8 hitResult;
    bool8 consumeNote = FALSE;

    if (bestIdx < MAX_NOTES && sRanger->notes[bestIdx].kind == NOTE_KIND_ATTACK && bestDelta <= 2)
    {
        // Pressing into the target's counterattack is backwards - that's on you.
        hitResult = HIT_MISS;
        consumeNote = TRUE;
        sRanger->missCount++;
        sRanger->loopProgress -= 25;
        if (sRanger->loopProgress < 0)
            sRanger->loopProgress = 0;
        PlaySE(SE_BALL_BOUNCE_4);
    }
    else if (bestDelta == 0)
    {
        hitResult = HIT_PERFECT;
        consumeNote = TRUE;
        sRanger->loopProgress += 8;
        PlaySE(SE_SUCCESS);
    }
    else if (bestDelta == 1)
    {
        hitResult = HIT_GOOD;
        consumeNote = TRUE;
        sRanger->loopProgress += 5;
        PlaySE(SE_SELECT);
    }
    else if (bestDelta == 2)
    {
        hitResult = HIT_OK;
        consumeNote = TRUE;
        sRanger->loopProgress += 2;
        PlaySE(SE_SELECT);
    }
    else
    {
        // Miss - no nearby note
        hitResult = HIT_MISS;
        sRanger->missCount++;
        sRanger->loopProgress -= 15;
        if (sRanger->loopProgress < 0)
            sRanger->loopProgress = 0;
        PlaySE(SE_BALL_BOUNCE_4);
    }

    // Consume the hit note
    if (consumeNote && bestIdx < MAX_NOTES)
    {
        u8  row = GetLaneRow(pressedLane);
        s16 col = sRanger->notes[bestIdx].tileCol;
        sRanger->notes[bestIdx].state = NOTE_INACTIVE;
        if (col >= LANE_START_COL && col <= LANE_END_COL)
        {
            SetTile(col, row,     LaneRestoreTile(col));
            SetTile(col, row + 1, LaneRestoreTile(col));
        }
    }

    if (sRanger->loopProgress > LOOP_PROGRESS_MAX)
        sRanger->loopProgress = LOOP_PROGRESS_MAX;

    PrintFeedback(hitResult);
    sRanger->feedbackTimer = FEEDBACK_DURATION;
    UpdateLoopMeter();
}

// ---- State handlers ----

static void DoInit(void)
{
    CalculateDifficultyForMode();
    sRanger->rstate = RSTATE_SETUP_GFX;
}

static void DoSetupGfx(void)
{
    LoadPalette(sRangerBgPal, 0, sizeof(sRangerBgPal));

    ResetBgsAndClearDma3BusyFlags(0);
    InitBgFromTemplate(&sRangerBgTemplate);
    SetBgTilemapBuffer(0, sRanger->tilemapBuffer);
    LoadBgTiles(0, sRangerBgTiles, TILE_COUNT * 32, 0);

    BuildInitialTilemap();
    UpdateLoopMeter();

    InitWindows(sRangerWinTemplates);
    DeactivateAllTextPrinters();
    PrintTitle();
    PrintLoopCount();

    CopyBgTilemapBufferToVram(0);
    ShowBg(0);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_BG0_ON);
    BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

    sRanger->countdownTimer = 0;
    sRanger->rstate = RSTATE_COUNTDOWN;
}

static void DoCountdown(void)
{
    u8 color[3] = {0, 7, 6};

    if (gPaletteFade.active)
        return;

    sRanger->countdownTimer++;

    if (sRanger->countdownTimer == 1)
    {
        FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
        AddTextPrinterParameterized4(WIN_FEEDBACK, FONT_NORMAL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_Countdown3);
        CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
        PutWindowTilemap(WIN_FEEDBACK);
        PlaySE(SE_SELECT);
    }
    else if (sRanger->countdownTimer == 60)
    {
        FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
        AddTextPrinterParameterized4(WIN_FEEDBACK, FONT_NORMAL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_Countdown2);
        CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
        PlaySE(SE_SELECT);
    }
    else if (sRanger->countdownTimer == 120)
    {
        FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
        AddTextPrinterParameterized4(WIN_FEEDBACK, FONT_NORMAL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_Countdown1);
        CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
        PlaySE(SE_SELECT);
    }
    else if (sRanger->countdownTimer >= COUNTDOWN_DURATION)
    {
        FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
        AddTextPrinterParameterized4(WIN_FEEDBACK, FONT_NORMAL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_Go);
        CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
        PutWindowTilemap(WIN_FEEDBACK);
        PlaySE(SE_SUCCESS);
        sRanger->rstate = RSTATE_PLAYING;
        sRanger->feedbackTimer = 60; // leave "GO!" up for 1 second
    }
}

static void DoPlaying(void)
{
    if (sRanger->feedbackTimer > 0)
    {
        sRanger->feedbackTimer--;
        if (sRanger->feedbackTimer == 0)
            ClearFeedback();
    }

    HandleInput();
    UpdateNotes();
    CopyBgTilemapBufferToVram(0);

    // Check loop completion
    if (sRanger->loopProgress >= LOOP_PROGRESS_MAX)
    {
        sRanger->loopProgress = 0;
        sRanger->loopsCompleted++;
        sRanger->notesSpawnedThisLoop = 0;
        sRanger->noteSpawnTimer = 0;
        PrintLoopCount();
        UpdateLoopMeter();
        CopyBgTilemapBufferToVram(0);

        if (sRanger->loopsCompleted >= sRanger->loopsNeeded)
        {
            gRangerCaptureState = RANGER_CAPTURE_SUCCESS;
            sRanger->animTimer  = 0;
            sRanger->rstate     = RSTATE_SUCCESS_ANIM;
            u8 color[3] = {0, 4, 0};
            FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
            AddTextPrinterParameterized4(WIN_FEEDBACK, FONT_SMALL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_Caught);
            CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
            PutWindowTilemap(WIN_FEEDBACK);
            PlaySE(SE_SUCCESS);
        }
    }

    // Check miss limit
    if (sRanger->missCount >= sRanger->maxMisses && sRanger->rstate == RSTATE_PLAYING)
    {
        gRangerCaptureState = RANGER_CAPTURE_FAIL;
        sRanger->animTimer  = 0;
        sRanger->rstate     = RSTATE_FAIL_ANIM;
        u8 color[3] = {0, 6, 0};
        FillWindowPixelBuffer(WIN_FEEDBACK, PIXEL_FILL(0));
        AddTextPrinterParameterized4(WIN_FEEDBACK, FONT_SMALL, 0, 0, 0, 0, color, TEXT_SKIP_DRAW, sText_BrokeFree);
        CopyWindowToVram(WIN_FEEDBACK, COPYWIN_FULL);
        PutWindowTilemap(WIN_FEEDBACK);
    }
}

static void DoSuccessAnim(void)
{
    sRanger->animTimer++;
    if (sRanger->animTimer >= SUCCESS_ANIM_DURATION)
        sRanger->rstate = RSTATE_FADE_OUT;
}

static void DoFailAnim(void)
{
    sRanger->animTimer++;
    if (sRanger->animTimer >= FAIL_ANIM_DURATION)
        sRanger->rstate = RSTATE_FADE_OUT;
}

static void DoFadeOut(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    sRanger->rstate = RSTATE_EXIT;
}

static void DoExit(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    FreeAllWindowBuffers();
    HideBg(0);
    ResetBgsAndClearDma3BusyFlags(0);

    if (sRanger->resultMode == RANGER_RESULT_MODE_SCRIPTED)
    {
        if (gRangerCaptureState == RANGER_CAPTURE_SUCCESS)
        {
            struct Pokemon mon;
            u32 personality = GetMonPersonality(sStagedStylerSpecies,
                GetSynchronizedGender(STATIC_WILDMON_ORIGIN, sStagedStylerSpecies),
                GetSynchronizedNature(STATIC_WILDMON_ORIGIN, sStagedStylerSpecies),
                RANDOM_UNOWN_LETTER);

            CreateMonWithIVs(&mon, sStagedStylerSpecies, sStagedStylerLevel, personality, OTID_STRUCT_PLAYER_ID, USE_RANDOM_IVS);
            GiveMonInitialMoveset(&mon);
            GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
            sStylerCaptureOutcome = STYLER_RESULT_CAUGHT;
        }
        else
        {
            sStylerCaptureOutcome = STYLER_RESULT_FAILED;
        }

        // Only the battle path's caller normally resets this handshake global; since a
        // scripted run has no such caller, reset it here so the battle path still sees
        // RANGER_CAPTURE_IDLE the next time it launches the minigame.
        gRangerCaptureState = RANGER_CAPTURE_IDLE;

        Free(sRanger);
        sRanger = NULL;
        DestroyTask(taskId);
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        return;
    }

    Free(sRanger);
    sRanger = NULL;

    DestroyTask(taskId);
    SetMainCallback2(gRangerCapture_ReturnCallback);
}

// ---- Main task ----
static void Task_RangerCapture(u8 taskId)
{
    switch (sRanger->rstate)
    {
    case RSTATE_INIT:         DoInit();              break;
    case RSTATE_SETUP_GFX:    DoSetupGfx();          break;
    case RSTATE_COUNTDOWN:    DoCountdown();         break;
    case RSTATE_PLAYING:      DoPlaying();           break;
    case RSTATE_SUCCESS_ANIM: DoSuccessAnim();       break;
    case RSTATE_FAIL_ANIM:    DoFailAnim();          break;
    case RSTATE_FADE_OUT:     DoFadeOut();           break;
    case RSTATE_EXIT:         DoExit(taskId);        break;
    }
}
