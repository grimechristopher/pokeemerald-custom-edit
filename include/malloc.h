#ifndef GUARD_ALLOC_H
#define GUARD_ALLOC_H


#define FREE_AND_SET_NULL(ptr)          \
do {                                    \
    Free(ptr);                          \
    ptr = NULL;                         \
} while (0)

#define TRY_FREE_AND_SET_NULL(ptr) if (ptr != NULL) FREE_AND_SET_NULL(ptr)

#define MALLOC_SYSTEM_ID 0xA3A3

struct MemBlock
{
    // Whether this block is currently allocated.
    u16 allocated:1;

    u16 unused_00:3;

    // High 12 bits of location pointer.
    u16 locationHi:12;

    // Magic number used for error checking. Should equal MALLOC_SYSTEM_ID.
    u16 magic;

    // Size of the block (not including this header struct). 19 bits caps a
    // single block (so also HEAP_SIZE, its one initial free block) at 512 KB.
    u32 size:19;

    // Low 13 bits of location pointer.
    u32 locationLo:13;

    // Previous block pointer. Equals sHeapStart if this is the first block.
    struct MemBlock *prev;

    // Next block pointer. Equals sHeapStart if this is the last block.
    struct MemBlock *next;

    // Data in the memory block. (Arrays of length 0 are a GNU extension.)
    u8 data[0];
};

// Increased to 320 KB (was 0x40000 = 256 KB, before that 0x1C300 = 112 KB) - large enough
// to hold MoveSaveBlocks_ResetHeap()'s simultaneous scratch copies of SaveBlock2 + SaveBlock1
// + PokemonStorage (302,168 bytes at 128-byte BoxPokemon / 72-box storage), with ~25 KB spare.
// NOTE: must stay under 512 KB - MemBlock.size is a bitfield that can't represent a bigger
// single (e.g. first, all-free) block; going over silently truncates and corrupts the heap.
#define HEAP_SIZE 0x50000
extern u8 gHeap[HEAP_SIZE];

#if TESTING || !defined(NDEBUG)

#define Alloc(size) Alloc_(size, __FILE__ ":" STR(__LINE__))
#define AllocUnchecked(size) AllocUnchecked_(size, __FILE__ ":" STR(__LINE__))

#define AllocZeroed(size) AllocZeroed_(size, __FILE__ ":" STR(__LINE__))
#define AllocZeroedUnchecked(size) AllocZeroedUnchecked_(size, __FILE__ ":" STR(__LINE__))

#else

#define Alloc(size) Alloc_(size, NULL)
#define AllocUnchecked(size) AllocUnchecked_(size, NULL)
#define AllocZeroed(size) AllocZeroed_(size, NULL)
#define AllocZeroedUnchecked(size) AllocZeroedUnchecked_(size, NULL)

#endif

void *Alloc_(u32 size, const char *location);
void *AllocUnchecked_(u32 size, const char *location);
void *AllocZeroed_(u32 size, const char *location);
void *AllocZeroedUnchecked_(u32 size, const char *location);
void Free(void *pointer);
void InitHeap(void *heapStart, u32 heapSize);
void PrintHeap(void);

const struct MemBlock *HeapHead(void);
const char *MemBlockLocation(const struct MemBlock *block);

#endif // GUARD_ALLOC_H
