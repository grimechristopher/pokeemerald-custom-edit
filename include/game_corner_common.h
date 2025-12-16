#ifndef GUARD_GAME_CORNER_COMMON_H
#define GUARD_GAME_CORNER_COMMON_H

#include "constants/game_stat.h"
#include "constants/flags.h"

// Minigame IDs
enum MinigameId {
    MINIGAME_SNAKE,
    MINIGAME_FLAPPYBIRD,
    MINIGAME_BLACKJACK,
    MINIGAME_VOLTORB_FLIP,
    MINIGAME_GACHA,
    MINIGAME_PACHINKO,
    MINIGAME_BLOCK_STACKER,
    MINIGAME_PINBALL,
    MINIGAME_DERBY,
    MINIGAME_COUNT
};

// Minigame metadata structure
struct MinigameMetadata {
    enum MinigameId id;
    const u8 *name;           // "Snake", "Flappy Bird", etc.
    u16 entryCost;            // Coins required to play
    u16 unlockFlag;           // FLAG_UNLOCKED_GAMECORNER_*
    void (*initFunc)(void);   // Minigame-specific init function
    void (*mainFunc)(void);   // Minigame main loop
    void (*exitFunc)(void);   // Cleanup function
};

// External registry (defined in game_corner_common.c)
extern const struct MinigameMetadata gMinigameRegistry[MINIGAME_COUNT];

// ========================================
// Initialization & Cleanup
// ========================================

void GameCorner_InitMinigame(enum MinigameId gameId);
void GameCorner_ExitMinigame(void);
bool8 GameCorner_CheckUnlocked(enum MinigameId gameId);
void GameCorner_Unlock(enum MinigameId gameId);
void GameCorner_UnlockAll(void);

// ========================================
// Coin Management
// ========================================

bool8 GameCorner_HasEnoughCoins(u16 cost);
bool8 GameCorner_TryTakeCoins(u16 cost);
void GameCorner_AddCoins(u16 amount);
void GameCorner_DisplayCoins(void);

// ========================================
// High Score Management
// ========================================

u32 GameCorner_GetHighScore(enum MinigameId gameId);
bool8 GameCorner_IsNewHighScore(enum MinigameId gameId, u32 score);
void GameCorner_UpdateHighScore(enum MinigameId gameId, u32 score);
void GameCorner_IncrementPlayCount(enum MinigameId gameId);
u32 GameCorner_GetPlayCount(enum MinigameId gameId);

// ========================================
// UI Helpers
// ========================================

void GameCorner_ShowInsufficientCoinsMessage(u16 required);
void GameCorner_ShowNoCoinCaseMessage(void);
void GameCorner_ShowGameOverMessage(u32 score, u32 coinsWon);
void GameCorner_ShowHighScoreMessage(u32 newHighScore);

// ========================================
// Input Helpers
// ========================================

bool8 GameCorner_WaitForAnyButton(void);
bool8 GameCorner_WaitForAButton(void);
bool8 GameCorner_WaitForBButton(void);
bool8 GameCorner_CheckQuitRequested(void);

// ========================================
// Audio Helpers
// ========================================

void GameCorner_PlayDefaultBGM(void);
void GameCorner_PlayWinSE(void);
void GameCorner_PlayLoseSE(void);
void GameCorner_PlayMenuSelectSE(void);
void GameCorner_PlayCoinSE(void);
void GameCorner_StopBGM(void);

#endif // GUARD_GAME_CORNER_COMMON_H
