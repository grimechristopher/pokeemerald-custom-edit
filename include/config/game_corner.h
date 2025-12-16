#ifndef GUARD_CONFIG_GAME_CORNER_H
#define GUARD_CONFIG_GAME_CORNER_H

// ========================================
// GAME CORNER MINIGAMES
// ========================================
// Enable/disable individual minigames.
// TRUE: Minigame is available (default)
// FALSE: Minigame is disabled (hidden from menus)
// ========================================

// Tier 1 - Simple Games
#define B_GAMECORNER_SNAKE              TRUE    // Snake game (grid-based)
#define B_GAMECORNER_FLAPPYBIRD         TRUE    // Flappy Bird clone
#define B_GAMECORNER_BLACKJACK          TRUE    // Blackjack card game

// Tier 2 - Medium Games
#define B_GAMECORNER_VOLTORB_FLIP       TRUE    // Voltorb Flip (HGSS puzzle)
#define B_GAMECORNER_GACHA              TRUE    // Gacha prize machines
#define B_GAMECORNER_PACHINKO           TRUE    // Pachinko (Japanese pinball)

// Tier 3 - Complex Games
#define B_GAMECORNER_BLOCK_STACKER      TRUE    // Block Stacker (Tetris-like)
#define B_GAMECORNER_PINBALL            TRUE    // Pokemon Pinball adaptation
#define B_GAMECORNER_DERBY              TRUE    // Pokemon racing game

// ========================================
// DIFFICULTY SETTINGS
// ========================================
// Adjust gameplay difficulty for supported games.
// GEN_3: Easy (original Ruby/Sapphire difficulty)
// GEN_4: Normal (balanced)
// GEN_5: Hard (increased challenge)
// GEN_LATEST: Very Hard (expert mode)
// ========================================

#define B_GAMECORNER_DIFFICULTY         GEN_4   // Default: Normal difficulty

// Per-game difficulty overrides (optional)
#define B_SNAKE_DIFFICULTY              GEN_4   // Snake-specific difficulty
#define B_FLAPPYBIRD_DIFFICULTY         GEN_4   // Flappy Bird-specific difficulty
#define B_VOLTORB_FLIP_DIFFICULTY       GEN_4   // Voltorb Flip-specific difficulty

// ========================================
// GAMEPLAY FEATURES
// ========================================

// High score persistence using game stats
#define B_GAMECORNER_HIGH_SCORES        TRUE    // Track high scores across saves

// Show "NEW RECORD!" message when high score is beaten
#define B_GAMECORNER_HIGH_SCORE_FANFARE TRUE    // Play fanfare for new records

// Allow replaying without exiting to map
#define B_GAMECORNER_QUICK_REPLAY       TRUE    // "Play Again?" prompt after game over

// Tutorial messages on first play
#define B_GAMECORNER_TUTORIALS          TRUE    // Show tutorial messages

// Unlock all games from start (useful for testing)
#define B_GAMECORNER_UNLOCK_ALL         FALSE   // Default: games start locked

// ========================================
// COIN REWARDS
// ========================================

// Multiplier for coin rewards
// 10: Original rewards (10 coins for score 100)
// 20: Double rewards (20 coins for score 100)
// 5: Half rewards (5 coins for score 100)
#define B_GAMECORNER_COIN_MULTIPLIER    10      // Default: 1.0x (10 = 100%)

// Minimum coins awarded (even for score 0)
#define B_GAMECORNER_MIN_COINS          1       // Always get at least 1 coin

// Maximum coins per game session
#define B_GAMECORNER_MAX_COINS_PER_GAME 999     // Cap at 999 coins per session

// ========================================
// ENTRY COSTS
// ========================================
// Coins required to play each minigame.
// 0: Free to play (useful for testing)
// ========================================

#define B_SNAKE_ENTRY_COST              10      // 10 coins to play Snake
#define B_FLAPPYBIRD_ENTRY_COST         20      // 20 coins to play Flappy Bird
#define B_BLACKJACK_ENTRY_COST          50      // 50 coins to play Blackjack
#define B_VOLTORB_FLIP_ENTRY_COST       0       // Free (rewards come from gameplay)
#define B_GACHA_ENTRY_COST              20      // 20 coins per pull
#define B_PACHINKO_ENTRY_COST           10      // 10 coins to play Pachinko
#define B_BLOCK_STACKER_ENTRY_COST      30      // 30 coins to play Block Stacker
#define B_PINBALL_ENTRY_COST            50      // 50 coins to play Pinball
#define B_DERBY_ENTRY_COST              100     // 100 coins to enter Derby

// ========================================
// AUDIO SETTINGS
// ========================================

// Use custom minigame BGM (TRUE) or reuse existing Game Corner music (FALSE)
#define B_GAMECORNER_CUSTOM_BGM         FALSE   // Default: reuse existing music

// Sound effects volume (percentage)
#define B_GAMECORNER_SFX_VOLUME         100     // 100 = full volume, 50 = half volume

// ========================================
// PERFORMANCE SETTINGS
// ========================================

// Target framerate (always 60 for GBA, but useful for debugging)
#define B_GAMECORNER_TARGET_FPS         60      // Do not change

// Enable performance profiling (adds debug overlay)
#define B_GAMECORNER_SHOW_FPS           FALSE   // Show FPS counter (debug only)

// Particle effect quality
// 0: No particles (best performance)
// 1: Minimal particles
// 2: Full particles (default)
#define B_GAMECORNER_PARTICLE_QUALITY   2       // Default: full particles

// ========================================
// DEBUG SETTINGS
// ========================================
// WARNING: These should be FALSE in release builds!
// ========================================

// Enable debug mode (shows collision boxes, state info, etc.)
#define B_GAMECORNER_DEBUG_MODE         FALSE   // Debug overlays

// God mode (invincibility, infinite lives)
#define B_GAMECORNER_GOD_MODE           FALSE   // Invincibility

// Always win (for testing win conditions)
#define B_GAMECORNER_ALWAYS_WIN         FALSE   // Auto-win

// Skip countdown timers (instant start)
#define B_GAMECORNER_SKIP_COUNTDOWN     FALSE   // Skip 3-2-1-GO

// Free entry (ignore coin costs)
#define B_GAMECORNER_FREE_PLAY          FALSE   // No coin costs

// ========================================
// INTEGRATION SETTINGS
// ========================================

// Location for new minigames
// TRUE: Mossdeep Game Corner (default)
// FALSE: Add to Mauville Game Corner
#define B_GAMECORNER_USE_MOSSDEEP       TRUE    // Use new location

// Show minigames in Trainer Card stats
#define B_GAMECORNER_SHOW_IN_TRAINER_CARD TRUE  // Show stats

// ========================================
// COMPILE-TIME VALIDATION
// ========================================

#if B_GAMECORNER_COIN_MULTIPLIER < 1
    #error "B_GAMECORNER_COIN_MULTIPLIER must be at least 1"
#endif

#if B_GAMECORNER_MAX_COINS_PER_GAME > 9999
    #error "B_GAMECORNER_MAX_COINS_PER_GAME cannot exceed coin system limit (9999)"
#endif

#if B_GAMECORNER_TARGET_FPS != 60
    #warning "B_GAMECORNER_TARGET_FPS should be 60 for GBA hardware"
#endif

// Ensure debug features are disabled in release builds
#ifndef DEBUG
    #if B_GAMECORNER_DEBUG_MODE == TRUE
        #error "B_GAMECORNER_DEBUG_MODE must be FALSE in release builds"
    #endif
    #if B_GAMECORNER_GOD_MODE == TRUE
        #error "B_GAMECORNER_GOD_MODE must be FALSE in release builds"
    #endif
    #if B_GAMECORNER_FREE_PLAY == TRUE
        #error "B_GAMECORNER_FREE_PLAY must be FALSE in release builds"
    #endif
#endif

#endif // GUARD_CONFIG_GAME_CORNER_H
