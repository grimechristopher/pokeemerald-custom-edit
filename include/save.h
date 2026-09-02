#ifndef GUARD_SAVE_H
#define GUARD_SAVE_H

#include "main.h"

// Each 4 KiB flash sector contains 3968 bytes of actual data followed by 116 bytes of SaveBlock3 and then 12 bytes of footer.
#define SECTOR_DATA_SIZE 3968
#define SAVE_BLOCK_3_CHUNK_SIZE 116
#define SECTOR_FOOTER_SIZE 12
#define SECTOR_SIZE (SECTOR_DATA_SIZE + SAVE_BLOCK_3_CHUNK_SIZE + SECTOR_FOOTER_SIZE)

// No redundant backup copy: only one physical save slot exists. A write that's
// interrupted mid-flash has no second copy to fall back to (see README) - this
// trades the stock corruption safety net for ~248 KB of reclaimed flash space.
#define NUM_SAVE_SLOTS 1

// If the sector's signature field is not this value then the sector is either invalid or empty.
#define SECTOR_SIGNATURE 0x8012025

#define SPECIAL_SECTOR_SENTINEL 0xB39D

#define SECTOR_ID_SAVEBLOCK2          0      // 1 sector
#define SECTOR_ID_SAVEBLOCK1_START    1
#define SECTOR_ID_SAVEBLOCK1_END      (SECTOR_ID_SAVEBLOCK1_START + 17 - 1)     // 17 sectors = 67 KB for multi-region data
#define SECTOR_ID_PKMN_STORAGE_START  (SECTOR_ID_SAVEBLOCK1_END + 1)
#define SECTOR_ID_PKMN_STORAGE_END    (SECTOR_ID_PKMN_STORAGE_START + 71 - 1)   // 71 sectors - 70 sectors (276,480 B) for the
                                                                                 // boxes[][] array at 128 B/mon, +1 for the rest
                                                                                 // of struct PokemonStorage (box names/wallpapers/
                                                                                 // etc, ~1.3 KB) that 70 alone doesn't cover
#define NUM_SECTORS_PER_SLOT          (SECTOR_ID_PKMN_STORAGE_END + 1)          // 1 + 17 + 71 sectors; the only save slot
#define SECTOR_ID_HOF_1               NUM_SECTORS_PER_SLOT
#define SECTOR_ID_HOF_2               (SECTOR_ID_HOF_1 + 1)
#define SECTOR_ID_TRAINER_HILL        (SECTOR_ID_HOF_1 + 2)
#define SECTOR_ID_RECORDED_BATTLE     (SECTOR_ID_HOF_1 + 3)
#define SECTOR_ID_RECORDED_BATTLE_2   (SECTOR_ID_HOF_1 + 4)    // struct RecordedBattleSave outgrew one sector once
                                                                 // struct Pokemon started growing - see recorded_battle.c
#define SECTORS_COUNT                 (SECTOR_ID_HOF_1 + 5)    // save slot + 5 special sectors

#define NUM_HOF_SECTORS 2

#define SAVE_STATUS_EMPTY    0
#define SAVE_STATUS_OK       1
#define SAVE_STATUS_CORRUPT  2
#define SAVE_STATUS_NO_FLASH 4
#define SAVE_STATUS_ERROR    0xFF

// Special sector id value for certain save functions to
// indicate that no specific sector should be used.
#define FULL_SAVE_SLOT 0xFFFF

// SetDamagedSectorBits states
enum
{
    ENABLE,
    DISABLE,
    CHECK // unused
};

// Do save types
enum
{
    SAVE_NORMAL,
    SAVE_LINK, // Link / Battle Frontier
    SAVE_EREADER, // deprecated in Emerald
    SAVE_HALL_OF_FAME,
    SAVE_OVERWRITE_DIFFERENT_FILE,
    SAVE_HALL_OF_FAME_ERASE_BEFORE // unused
};

// A save sector location holds a pointer to the data for a particular sector
// and the size of that data. For 72-box storage, offsets can exceed 65535, so size is u32.
struct SaveSectorLocation
{
    void *data;
    u32 size;  // Changed from u16 to u32 for large structure offsets
};

struct SaveSector
{
    u8 data[SECTOR_DATA_SIZE];
    u8 saveBlock3Chunk[SAVE_BLOCK_3_CHUNK_SIZE];
    u16 id;
    u16 checksum;
    u32 signature;
    u32 counter;
}; // size is SECTOR_SIZE (0x1000)

#define SECTOR_SIGNATURE_OFFSET offsetof(struct SaveSector, signature)
#define SECTOR_COUNTER_OFFSET   offsetof(struct SaveSector, counter)

extern u16 gLastWrittenSector;
extern u32 gLastSaveCounter;
extern u16 gLastKnownGoodSector;
extern u32 gDamagedSaveSectors;
extern u32 gSaveCounter;
extern struct SaveSector *gFastSaveSector;
extern u16 gIncrementalSectorId;
extern u16 gSaveFileStatus;
extern MainCallback gGameContinueCallback;
extern struct SaveSectorLocation gRamSaveSectorLocations[];

extern struct SaveSector gSaveDataBuffer;

void ClearSaveData(void);
void Save_ResetSaveCounters(void);
u8 HandleSavingData(u8 saveType);
u8 TrySavingData(u8 saveType);
bool8 LinkFullSave_Init(void);
bool8 LinkFullSave_WriteSector(void);
bool8 LinkFullSave_ReplaceLastSector(void);
bool8 LinkFullSave_SetLastSectorSignature(void);
bool8 WriteSaveBlock2(void);
bool8 WriteSaveBlock1Sector(void);
u8 LoadGameSave(u8 saveType);
u16 GetSaveBlocksPointersBaseOffset(void);
u32 TryReadSpecialSaveSector(u8 sector, u8 *dst);
u32 TryWriteSpecialSaveSector(u8 sector, u8 *src);
void Task_LinkFullSave(u8 taskId);

// save_failed_screen.c
void DoSaveFailedScreen(u8 saveType);

#endif // GUARD_SAVE_H
