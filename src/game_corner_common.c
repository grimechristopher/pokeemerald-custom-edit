#include "global.h"
#include "game_corner_common.h"
#include "bg.h"
#include "coins.h"
#include "event_data.h"
#include "field_message_box.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "palette.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/coins.h"
#include "constants/items.h"
#include "constants/songs.h"

// ========================================
// Minigame Name Strings
// ========================================

static const u8 sText_MinigameName_Snake[] = _("SNAKE");
static const u8 sText_MinigameName_FlappyBird[] = _("FLAPPY BIRD");
static const u8 sText_MinigameName_Blackjack[] = _("BLACKJACK");
static const u8 sText_MinigameName_VoltorbFlip[] = _("VOLTORB FLIP");
static const u8 sText_MinigameName_Gacha[] = _("GACHA");
static const u8 sText_MinigameName_Pachinko[] = _("PACHINKO");
static const u8 sText_MinigameName_BlockStacker[] = _("BLOCK STACKER");
static const u8 sText_MinigameName_Pinball[] = _("PINBALL");
static const u8 sText_MinigameName_Derby[] = _("DERBY");

// ========================================
// UI Message Strings
// ========================================

static const u8 sText_YouNeed[] = _("You need ");
static const u8 sText_CoinsToPlay[] = _(" Coins\nto play this game!");
static const u8 sText_NoCoinCase[] = _("You need a COIN CASE\nto play!");
static const u8 sText_GameOver[] = _("GAME OVER!\nScore: ");
static const u8 sText_CoinsPlus[] = _("\nCoins: +");
static const u8 sText_NewHighScore[] = _("NEW HIGH SCORE!\n");

// ========================================
// Minigame Registry
// ========================================

// Forward declarations (will be implemented when each minigame is ported)
// Snake - implemented in game_corner_snake.c
extern void Snake_Init(void);
extern void Snake_Main(void);
extern void Snake_Exit(void);
static void FlappyBird_Init(void);
static void FlappyBird_Main(void);
static void FlappyBird_Exit(void);
static void Blackjack_Init(void);
static void Blackjack_Main(void);
static void Blackjack_Exit(void);
static void VoltorbFlip_Init(void);
static void VoltorbFlip_Main(void);
static void VoltorbFlip_Exit(void);
static void Gacha_Init(void);
static void Gacha_Main(void);
static void Gacha_Exit(void);
static void Pachinko_Init(void);
static void Pachinko_Main(void);
static void Pachinko_Exit(void);
static void BlockStacker_Init(void);
static void BlockStacker_Main(void);
static void BlockStacker_Exit(void);
static void Pinball_Init(void);
static void Pinball_Main(void);
static void Pinball_Exit(void);
static void Derby_Init(void);
static void Derby_Main(void);
static void Derby_Exit(void);

// Minigame metadata registry
const struct MinigameMetadata gMinigameRegistry[MINIGAME_COUNT] = {
    [MINIGAME_SNAKE] = {
        .id = MINIGAME_SNAKE,
        .name = sText_MinigameName_Snake,
        .entryCost = B_SNAKE_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_SNAKE,
        .initFunc = Snake_Init,
        .mainFunc = Snake_Main,
        .exitFunc = Snake_Exit,
    },
    [MINIGAME_FLAPPYBIRD] = {
        .id = MINIGAME_FLAPPYBIRD,
        .name = sText_MinigameName_FlappyBird,
        .entryCost = B_FLAPPYBIRD_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_FLAPPYBIRD,
        .initFunc = FlappyBird_Init,
        .mainFunc = FlappyBird_Main,
        .exitFunc = FlappyBird_Exit,
    },
    [MINIGAME_BLACKJACK] = {
        .id = MINIGAME_BLACKJACK,
        .name = sText_MinigameName_Blackjack,
        .entryCost = B_BLACKJACK_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_BLACKJACK,
        .initFunc = Blackjack_Init,
        .mainFunc = Blackjack_Main,
        .exitFunc = Blackjack_Exit,
    },
    [MINIGAME_VOLTORB_FLIP] = {
        .id = MINIGAME_VOLTORB_FLIP,
        .name = sText_MinigameName_VoltorbFlip,
        .entryCost = B_VOLTORB_FLIP_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_VOLTORB_FLIP,
        .initFunc = VoltorbFlip_Init,
        .mainFunc = VoltorbFlip_Main,
        .exitFunc = VoltorbFlip_Exit,
    },
    [MINIGAME_GACHA] = {
        .id = MINIGAME_GACHA,
        .name = sText_MinigameName_Gacha,
        .entryCost = B_GACHA_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_GACHA,
        .initFunc = Gacha_Init,
        .mainFunc = Gacha_Main,
        .exitFunc = Gacha_Exit,
    },
    [MINIGAME_PACHINKO] = {
        .id = MINIGAME_PACHINKO,
        .name = sText_MinigameName_Pachinko,
        .entryCost = B_PACHINKO_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_PACHINKO,
        .initFunc = Pachinko_Init,
        .mainFunc = Pachinko_Main,
        .exitFunc = Pachinko_Exit,
    },
    [MINIGAME_BLOCK_STACKER] = {
        .id = MINIGAME_BLOCK_STACKER,
        .name = sText_MinigameName_BlockStacker,
        .entryCost = B_BLOCK_STACKER_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_BLOCK_STACKER,
        .initFunc = BlockStacker_Init,
        .mainFunc = BlockStacker_Main,
        .exitFunc = BlockStacker_Exit,
    },
    [MINIGAME_PINBALL] = {
        .id = MINIGAME_PINBALL,
        .name = sText_MinigameName_Pinball,
        .entryCost = B_PINBALL_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_PINBALL,
        .initFunc = Pinball_Init,
        .mainFunc = Pinball_Main,
        .exitFunc = Pinball_Exit,
    },
    [MINIGAME_DERBY] = {
        .id = MINIGAME_DERBY,
        .name = sText_MinigameName_Derby,
        .entryCost = B_DERBY_ENTRY_COST,
        .unlockFlag = FLAG_UNLOCKED_GAMECORNER_DERBY,
        .initFunc = Derby_Init,
        .mainFunc = Derby_Main,
        .exitFunc = Derby_Exit,
    },
};

// Placeholder implementations (will be replaced when porting each minigame)
static void FlappyBird_Init(void) {}
static void FlappyBird_Main(void) {}
static void FlappyBird_Exit(void) {}
static void Blackjack_Init(void) {}
static void Blackjack_Main(void) {}
static void Blackjack_Exit(void) {}
static void VoltorbFlip_Init(void) {}
static void VoltorbFlip_Main(void) {}
static void VoltorbFlip_Exit(void) {}
static void Gacha_Init(void) {}
static void Gacha_Main(void) {}
static void Gacha_Exit(void) {}
static void Pachinko_Init(void) {}
static void Pachinko_Main(void) {}
static void Pachinko_Exit(void) {}
static void BlockStacker_Init(void) {}
static void BlockStacker_Main(void) {}
static void BlockStacker_Exit(void) {}
static void Pinball_Init(void) {}
static void Pinball_Main(void) {}
static void Pinball_Exit(void) {}
static void Derby_Init(void) {}
static void Derby_Main(void) {}
static void Derby_Exit(void) {}

// ========================================
// Initialization & Cleanup
// ========================================

void GameCorner_InitMinigame(enum MinigameId gameId)
{
    if (gameId >= MINIGAME_COUNT)
        return;

    // Common setup for all minigames
    SetVBlankCallback(NULL);
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetTasks();
    ResetPaletteFade();

    // Call game-specific init
    const struct MinigameMetadata *game = &gMinigameRegistry[gameId];
    if (game->initFunc != NULL)
        game->initFunc();
}

void GameCorner_ExitMinigame(void)
{
    // Common cleanup
    FreeAllWindowBuffers();
    SetVBlankCallback(NULL);
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetTasks();
    ResetPaletteFade();

    // Return to field
    SetMainCallback2(CB2_ReturnToField);
}

bool8 GameCorner_CheckUnlocked(enum MinigameId gameId)
{
    if (gameId >= MINIGAME_COUNT)
        return FALSE;

    // If unlock all is enabled, always return true
    if (B_GAMECORNER_UNLOCK_ALL)
        return TRUE;

    const struct MinigameMetadata *game = &gMinigameRegistry[gameId];
    return FlagGet(game->unlockFlag);
}

void GameCorner_Unlock(enum MinigameId gameId)
{
    if (gameId >= MINIGAME_COUNT)
        return;

    const struct MinigameMetadata *game = &gMinigameRegistry[gameId];
    FlagSet(game->unlockFlag);
}

void GameCorner_UnlockAll(void)
{
    for (u32 i = 0; i < MINIGAME_COUNT; i++)
        FlagSet(gMinigameRegistry[i].unlockFlag);
}

// ========================================
// Coin Management
// ========================================

bool8 GameCorner_HasEnoughCoins(u16 cost)
{
    // Free play mode ignores costs
    if (B_GAMECORNER_FREE_PLAY)
        return TRUE;

    return GetCoins() >= cost;
}

bool8 GameCorner_TryTakeCoins(u16 cost)
{
    // Free play mode doesn't deduct coins
    if (B_GAMECORNER_FREE_PLAY)
        return TRUE;

    if (!GameCorner_HasEnoughCoins(cost))
        return FALSE;

    SetCoins(GetCoins() - cost);
    return TRUE;
}

void GameCorner_AddCoins(u16 amount)
{
    u32 currentCoins = GetCoins();
    u32 newCoins = currentCoins + amount;

    // Apply coin multiplier
    newCoins = (newCoins * B_GAMECORNER_COIN_MULTIPLIER) / 10;

    // Cap at 9999 (existing coin system limit)
    if (newCoins > MAX_COINS)
        newCoins = MAX_COINS;

    SetCoins((u16)newCoins);
}

void GameCorner_DisplayCoins(void)
{
    // This would show a coin count window
    // Implementation depends on UI design
    // For now, this is a placeholder
}

// ========================================
// High Score Management
// ========================================

// Map minigame IDs to game stat indices
static const u8 sMinigameHighScoreStats[MINIGAME_COUNT] = {
    [MINIGAME_SNAKE]         = GAME_STAT_SNAKE_HIGH_SCORE,
    [MINIGAME_FLAPPYBIRD]    = GAME_STAT_FLAPPYBIRD_HIGH_SCORE,
    [MINIGAME_BLACKJACK]     = GAME_STAT_BLACKJACK_HIGH_SCORE,
    [MINIGAME_VOLTORB_FLIP]  = GAME_STAT_VOLTORB_FLIP_HIGH_SCORE,
    [MINIGAME_GACHA]         = GAME_STAT_GACHA_PLAYS,  // Gacha uses plays as "score"
    [MINIGAME_PACHINKO]      = GAME_STAT_PACHINKO_HIGH_SCORE,
    [MINIGAME_BLOCK_STACKER] = GAME_STAT_BLOCK_STACKER_HIGH_SCORE,
    [MINIGAME_PINBALL]       = GAME_STAT_SNAKE_HIGH_SCORE,  // TODO: Add PINBALL stat when expanding stats
    [MINIGAME_DERBY]         = GAME_STAT_SNAKE_HIGH_SCORE,  // TODO: Add DERBY stat when expanding stats
};

static const u8 sMinigamePlayCountStats[MINIGAME_COUNT] = {
    [MINIGAME_SNAKE]         = GAME_STAT_SNAKE_PLAYS,
    [MINIGAME_FLAPPYBIRD]    = GAME_STAT_FLAPPYBIRD_PLAYS,
    [MINIGAME_BLACKJACK]     = GAME_STAT_BLACKJACK_WINS,
    [MINIGAME_VOLTORB_FLIP]  = GAME_STAT_VOLTORB_FLIP_PLAYS,
    [MINIGAME_GACHA]         = GAME_STAT_GACHA_PLAYS,
    [MINIGAME_PACHINKO]      = GAME_STAT_SNAKE_PLAYS,  // TODO: Add PACHINKO_PLAYS when expanding stats
    [MINIGAME_BLOCK_STACKER] = GAME_STAT_SNAKE_PLAYS,  // TODO: Add STACKER_PLAYS when expanding stats
    [MINIGAME_PINBALL]       = GAME_STAT_SNAKE_PLAYS,  // TODO: Add PINBALL_PLAYS when expanding stats
    [MINIGAME_DERBY]         = GAME_STAT_SNAKE_PLAYS,  // TODO: Add DERBY_PLAYS when expanding stats
};

u32 GameCorner_GetHighScore(enum MinigameId gameId)
{
    if (gameId >= MINIGAME_COUNT)
        return 0;

    if (!B_GAMECORNER_HIGH_SCORES)
        return 0;

    u32 score = GetGameStat(sMinigameHighScoreStats[gameId]);

    // Sanity check: unreasonably high scores = corruption
    if (score > 999999)
    {
        SetGameStat(sMinigameHighScoreStats[gameId], 0);
        return 0;
    }

    return score;
}

bool8 GameCorner_IsNewHighScore(enum MinigameId gameId, u32 score)
{
    if (!B_GAMECORNER_HIGH_SCORES)
        return FALSE;

    return score > GameCorner_GetHighScore(gameId);
}

void GameCorner_UpdateHighScore(enum MinigameId gameId, u32 score)
{
    if (gameId >= MINIGAME_COUNT)
        return;

    if (!B_GAMECORNER_HIGH_SCORES)
        return;

    // Sanity check score
    if (score > 999999)
        score = 999999;

    u32 currentHigh = GetGameStat(sMinigameHighScoreStats[gameId]);
    if (score > currentHigh)
        SetGameStat(sMinigameHighScoreStats[gameId], score);
}

void GameCorner_IncrementPlayCount(enum MinigameId gameId)
{
    if (gameId >= MINIGAME_COUNT)
        return;

    IncrementGameStat(sMinigamePlayCountStats[gameId]);
}

u32 GameCorner_GetPlayCount(enum MinigameId gameId)
{
    if (gameId >= MINIGAME_COUNT)
        return 0;

    return GetGameStat(sMinigamePlayCountStats[gameId]);
}

// ========================================
// UI Helpers
// ========================================

void GameCorner_ShowInsufficientCoinsMessage(u16 required)
{
    ConvertIntToDecimalStringN(gStringVar1, required, STR_CONV_MODE_LEADING_ZEROS, 4);
    StringCopy(gStringVar4, sText_YouNeed);
    StringAppend(gStringVar4, gStringVar1);
    StringAppend(gStringVar4, sText_CoinsToPlay);
    ShowFieldAutoScrollMessage(gStringVar4);
}

void GameCorner_ShowNoCoinCaseMessage(void)
{
    ShowFieldAutoScrollMessage(sText_NoCoinCase);
}

void GameCorner_ShowGameOverMessage(u32 score, u32 coinsWon)
{
    ConvertIntToDecimalStringN(gStringVar1, score, STR_CONV_MODE_LEFT_ALIGN, 6);
    ConvertIntToDecimalStringN(gStringVar2, coinsWon, STR_CONV_MODE_LEFT_ALIGN, 4);

    StringCopy(gStringVar4, sText_GameOver);
    StringAppend(gStringVar4, gStringVar1);
    StringAppend(gStringVar4, sText_CoinsPlus);
    StringAppend(gStringVar4, gStringVar2);

    ShowFieldAutoScrollMessage(gStringVar4);
}

void GameCorner_ShowHighScoreMessage(u32 newHighScore)
{
    if (!B_GAMECORNER_HIGH_SCORE_FANFARE)
        return;

    ConvertIntToDecimalStringN(gStringVar1, newHighScore, STR_CONV_MODE_LEFT_ALIGN, 6);
    StringCopy(gStringVar4, sText_NewHighScore);
    StringAppend(gStringVar4, gStringVar1);
    ShowFieldAutoScrollMessage(gStringVar4);
}

// ========================================
// Input Helpers
// ========================================

bool8 GameCorner_WaitForAnyButton(void)
{
    return JOY_NEW(A_BUTTON | B_BUTTON | START_BUTTON);
}

bool8 GameCorner_WaitForAButton(void)
{
    return JOY_NEW(A_BUTTON);
}

bool8 GameCorner_WaitForBButton(void)
{
    return JOY_NEW(B_BUTTON);
}

bool8 GameCorner_CheckQuitRequested(void)
{
    // Allow B button or START to quit during gameplay
    return JOY_NEW(B_BUTTON | START_BUTTON);
}

// ========================================
// Audio Helpers
// ========================================

void GameCorner_PlayDefaultBGM(void)
{
    if (B_GAMECORNER_CUSTOM_BGM)
    {
        // Will use custom BGM when available
        // For now, use existing Game Corner music
        PlayBGM(MUS_GAME_CORNER);
    }
    else
    {
        PlayBGM(MUS_GAME_CORNER);
    }
}

void GameCorner_PlayWinSE(void)
{
    PlaySE(SE_WIN_OPEN);
}

void GameCorner_PlayLoseSE(void)
{
    PlaySE(SE_FAILURE);
}

void GameCorner_PlayMenuSelectSE(void)
{
    PlaySE(SE_SELECT);
}

void GameCorner_PlayCoinSE(void)
{
    PlaySE(SE_SHOP);
}

void GameCorner_StopBGM(void)
{
    if (IsBGMPlaying())
        FadeOutBGM(4);
}
