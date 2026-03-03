#ifndef MIRON_MEMORY_H
#define MIRON_MEMORY_H

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <memoryapi.h>

/* --- Definitions & Constants --- */
#define PAGESIZE 4096
#define METADATASIZE sizeof(PageNode)

/* --- Structures --- */

// Page headers - Meta data for each allocated 4KB page
typedef struct PageNode {
    void* nextPageNode;    // Pointer to the next PageNode in the chain
    void* currPagePtr;     // Pointer to the start of the page memory
    size_t freeBytesLeft;  // Remaining space in the page
} PageNode;

// Memory Block headers - Meta data for each individual allocation within a page
typedef struct MemBlock {
    long long size;        // Size of the data payload; -1 indicates unconfigured/wilderness
    void* currMemBlock;    // Pointer to this header
    void* nextMemBlock;    // Pointer to the next header; NULL if it's the last in the page
    bool free;             // Allocation status: true = available, false = occupied
} MemBlock;

/* --- Core Allocator API --- */

/**
 * miron_malloc: The primary interface for requesting memory.
 * Will scan existing pages for a free block or allocate a new page if necessary.
 */
void* miron_malloc(size_t size);

/**
 * freeMemBlock: Frees a previously allocated block.
 * Takes the pointer provided to the user and retrieves the hidden metadata.
 */
void freeMemBlock(void* userPtr);

/* --- Helper & Internal Logic Prototypes --- */

// Aligns requested size to 8-byte boundaries (64-bit alignment)
size_t alignMemorySize(size_t requestedSize);

// Internal: Allocates a fresh 4KB page from the OS via VirtualAlloc
void* allocatePage();

// Internal: Searches for a suitable free block within a specific page
void* allocateMemBlock(PageNode* page, size_t requestedSize);

// Internal: Calculates available space in a block or sequence of free blocks
long long spaceLeftInMemBlock(PageNode* page, uintptr_t ptr, size_t requestedSize);

// Internal: Calculates space remaining until the page boundary (4088 bytes)
long long spaceLeft(PageNode* page, uintptr_t ptr, size_t requestedSize);

// Internal: Locates the PageNode containing a specific memory block
PageNode* findCurrentPage(MemBlock* target);

// Internal: Retrieves the MemBlock header from a user-facing data pointer
void* fetchMemBlockData(void* ptr);

#endif // MIRON_MEMORY_H