#define _DEFAULT_SOURCE
#include <stdio.h> //basic standard lib
#include <stdlib.h> //module/header for basic dynamic memory allocation
#include <stdbool.h> //for boolean operation
#include <memoryapi.h> //for windows custom dynamic memory allocation api
#include <stdint.h> //for uintptr_t

#define PAGESIZE 4096 //windows page size by default


//Page headers - Stored at each start of a page representing meta data of the page
typedef struct PageNode{
    void* nextPageNode; //self explanatory - it's next "Node"
    void* currPagePtr; //self explanatory - the pageHeader itself
    size_t freeBytesLeft; //self explanatory - how much bytes are currently taken
} PageNode;


//Memmory Block headers - Stored at each start of a mem block and represents the meta data of the block
//memory blocks will have 
typedef struct MemBlock{
    long long size; //size in bytes - CHANGED TO LONG LONG
    void* currMemBlock; //curr pointer to the memory block, WILL BE -1 IF THE CURRENT BLOCK IS THE LAST
    void* nextMemBlock; //WILL BE NULL IF THERE IS NO NEXT BLOCKnext pointer to the next memory block --> could be potentially the next page
    bool free; //true -> free, false -> ain't

} MemBlock;
// pageNode+MemBlock ................ MemBlock MemBlock ......
#define METADATASIZE sizeof(PageNode)
#define MIN_SPLIT_SIZE (alignMemorySize(sizeof(MemBlock)) + 8) //a protocol that decides if it is worth splitting a request into a new second memory block or just give the user the extra bytes

PageNode* firstPage = NULL;

//align each memory block to exactly a 64 bit environment - prevents memory missalignment
size_t alignMemorySize(size_t requestedSize)
{
    if(requestedSize % 8 == 0)
        return requestedSize;
    size_t finalSize;
    int tempResult = requestedSize + 8;
    finalSize = tempResult - ( 8 - (tempResult % 8));
    // tempResult--> (num + 8) - ( 8 - (ans % 8) --> gives round num
    return finalSize;
}



//function is used for freeing memmory block so we know in which page we gotta handle updating the metadata
//Logic fix: Ensure currPage actually updates to avoid infinite loop
PageNode* findCurrentPage(MemBlock* target)
{
    if(firstPage==NULL)
        return NULL;
    PageNode* currPage = firstPage;
    uintptr_t targetAddr = (uintptr_t)target;

    while(currPage != NULL)
    {
        uintptr_t pageStart = (uintptr_t)currPage->currPagePtr;
        uintptr_t pageEnd = pageStart + PAGESIZE;

        if(targetAddr >= pageStart && targetAddr < pageEnd)
            return currPage;
        
        currPage = (PageNode*)currPage->nextPageNode; // Updated to assign next node
    }
    return NULL;

}


void* fetchMemBlockData(void* ptr) //will be used for free function, taking the user given pointer to data, and retrieving metadata by reversing back (uintptr_t)user_ptr - size(MemBlock)
{
    // Using pointer arithmetic: subtracting 1 from a MemBlock* moves it back by sizeof(MemBlock)
    return (void*)((MemBlock*)ptr - 1);
}


//the idea is when we free a memmory block.. if the original size of the total occupation is bigger than what's requested, we can split 
//it into two memmory blocks
//we can always track the neccesary metadata for everything by storing in the functions range the first pointer that is free
//so lets say we occupy 2 blocks that are free - before we itirate each time we store the first memmory block we met in a row

//this function will free a page
void freePage(PageNode* target)
{
    PageNode* currPage = firstPage;
    void* pagePtr = currPage->currPagePtr; //to free the page at the end

    PageNode* nextPage = (PageNode*)currPage->nextPageNode;
    if(currPage == target)
    {
        if(nextPage == NULL)
        {
            firstPage = NULL;
        }
        else
        {
            firstPage = nextPage;
        }
        
    }
    else
    {
        while(nextPage != NULL)
        {
            if(nextPage == target)
            {
                currPage->nextPageNode = nextPage->nextPageNode; //get rid of the sight of the target Node
            }
            currPage = nextPage;
            nextPage = (PageNode*)nextPage->nextPageNode;
        }
    }
    VirtualFree(target,0,MEM_RELEASE); //Actually release the allocated page by its pointer to memmory
}

void freeMemBlock(void* userPtr)
{
    MemBlock* currBlock = (MemBlock*)fetchMemBlockData(userPtr);
    currBlock->free = true;
    PageNode* currPage = findCurrentPage(currBlock);
    currPage->freeBytesLeft += currBlock->size;
    //if the page is empty entierly --> free page
    if(currPage->freeBytesLeft == PAGESIZE)
    {
        freePage(currPage);
    }
}



//scanning for a free spot for memmory function
//by taking the amount of free bytes in a row and checking if it is enough to fit the target amount
//if there is a possibility to save a bit of bytes by not giving the whole
//free space - we can just create a mem block - and create another one next to it
//that way if we want to use that leftover space we absolutely can recycle the old block to new ones and avoid reconstructing everything
//block splitting will be handled by the father function - allocateMemBlock

//this function will check if there is enough space in the page 
//and additionally will return the space that's avaliable besides what's required
//to have a chance at making the users request yfm?
long long spaceLeft(PageNode* page, uintptr_t ptr,  size_t requestedSize)
{//4088 is the last byte that can be allocated with aligment and stay inside the pages border
    requestedSize = alignMemorySize(requestedSize);
    uintptr_t pageEnd = (uintptr_t)(page->currPagePtr) + 4088;
    
    // Space remaining = Page End - (current pointer + header size)
    long long available = (long long)(pageEnd - (ptr + alignMemorySize(sizeof(MemBlock))));
    

    if (available < (long long)requestedSize) {
        return -9999; 
    }
    return available;
}

// the function will check if there is a possibility to allocate from the page
// less bytes then there were from the previously used pointer
// will return the available space inside a memmory block - including void spaces
// nextPtr: pass the next block in the chain, or the 'end' of the sequence you are checking
// the function will check if there is a possibility to allocate from the page
// less bytes then there were from the previously used pointer
// will return the available space inside a memmory block - including void spaces
// nextPtr: pass the next block in the chain, or the 'end' of the sequence you are checking
long long spaceLeftInMemBlock(PageNode* page, uintptr_t ptr, size_t requestedSize)
{
    MemBlock* currBlock = (MemBlock*)ptr;
    long long totalSpaceAccumulated = 0;

    // We only want to "bridge" blocks if they are consecutive and FREE
    while(currBlock != NULL && currBlock->free)
    {
        // NEW LOGIC: If there is NO next block, this is the true end of the page (The Caboose)
        // We calculate space from here to the 4088 boundary.
        if(currBlock->nextMemBlock == NULL) {
            totalSpaceAccumulated += spaceLeft(page, (uintptr_t)currBlock, requestedSize);
            return totalSpaceAccumulated; 
        }

        // Case B: Standard Block with a neighbor (could be size > 0 or size == -1)
        // Since we have a nextMemBlock, we only calculate the gap between headers.
        uintptr_t currentHeaderEnd = (uintptr_t)currBlock + alignMemorySize(sizeof(MemBlock));
        long long gap = (long long)((uintptr_t)currBlock->nextMemBlock - currentHeaderEnd);
        
        totalSpaceAccumulated += gap;
        
        // Move to the neighbor to continue bridging
        currBlock = (MemBlock*)currBlock->nextMemBlock;
    }

    return totalSpaceAccumulated;
}

//in order to assign the previous page the current block - we have to pass the previous block pointer to the allocating function
uintptr_t findPreviousMemBlock(PageNode* page,uintptr_t targetBlock)
{
    MemBlock* firstMemBlock = (MemBlock*)(page->currPagePtr + sizeof(PageNode));
    if ((uintptr_t)firstMemBlock == targetBlock) {//return null if the first mem block is already the target one
        return (uintptr_t)NULL;
    }

    MemBlock* scanningBlock = firstMemBlock; //make a pseudo copy of the scanningPtr so we won't lose it
    MemBlock* next;

    while(scanningBlock != NULL && (uintptr_t)scanningBlock->nextMemBlock != targetBlock)
    {
        next = (MemBlock*)(scanningBlock->nextMemBlock);
        long long nextSize = next->size;
        if(nextSize == -1) //the next block is a recycled one and, it has to be jumped upon
        {          
            if((uintptr_t)(next) != targetBlock) scanningBlock = (MemBlock*)next->nextMemBlock;
        }
        else{
            scanningBlock = (MemBlock*)scanningBlock->nextMemBlock;
        }

    }
    return (uintptr_t)scanningBlock;
}



//in this function we will make use of the spaceLeftFunction etc.
//this function will itirate over the whole page until it finds a spot for the memblock
//this function will also split the result into two mem blocks if possible
void* allocateMemBlock(PageNode* page,size_t requestedSize)
{
    if(page == NULL) return NULL;//avoid segfault at first page
    MemBlock* prev = NULL; //NOTE: previous of the first free block, not curr
    MemBlock* curr = (MemBlock*)((uintptr_t)(page->currPagePtr + sizeof(PageNode)));//first block
    MemBlock* firstFreeBlock = curr;
    size_t freeBytesSequence = 0;
    long long availableSpace = 0;//here we store the spaceLeftInMemBlock result

    printf("\n--- Scanning Page %p for %zu bytes ---\n", (void*)page->currPagePtr, requestedSize);

    while(curr != NULL)
    {
        availableSpace = spaceLeftInMemBlock(page,(uintptr_t)(firstFreeBlock),requestedSize);
        freeBytesSequence += availableSpace;

        if(curr->size == -1)
        {
            if(freeBytesSequence < alignMemorySize(requestedSize))
            {
                return NULL;
            }
        }

        printf("[DEBUG] Block: %p | Free: %d | Size: %lld | Total Seq: %lld\n", 
                (void*)curr, curr->free, curr->size, (long long)(freeBytesSequence));

        if(curr->free)
        {
            if(availableSpace == -9999)
            {
                printf("[DEBUG] !! Boundary Error (-9999)\n");
                return NULL;
            } //error code for no space in page
            if(alignMemorySize(requestedSize) > freeBytesSequence)//if there is no space available inside the memmory block
            {
                printf("[DEBUG] Small block, adding to sequence...\n");
                curr = (MemBlock*)curr->nextMemBlock;
            }
            else //if there is space in that memmory block (good spot found!!)- maybe we will split memmory blocks here(we take in the fact the memmory block can be the first)
            { //FINISH IT

                size_t alignedReq = alignMemorySize(requestedSize);

                // WORTHINESS PROTOCOL: (check if splittable)
                // We need room for: Aligned Request + New Header + Minimum Payload (8)
                if(freeBytesSequence >= (long long)(alignedReq + alignMemorySize(sizeof(MemBlock)) + 8))
                {
                    printf("[DEBUG] >> ACTION: SPLITTING at %p\n", (void*)firstFreeBlock);
                    // 1. Configure the current block for the user
                    firstFreeBlock->free = false;
                    firstFreeBlock->size = (long long)alignedReq;

                    // 2. Calculate the position of the NEW recycled header
                    // We move past our current header and our data payload
                    uintptr_t nextHeaderAddr = (uintptr_t)firstFreeBlock + alignMemorySize(sizeof(MemBlock)) + firstFreeBlock->size;
                    
                    MemBlock* newBlock = (MemBlock*)nextHeaderAddr;
                    
                    // 3. THE STITCH: Link the new block into the chain
                    newBlock->nextMemBlock = curr->nextMemBlock; // Keep the original chain intact
                    firstFreeBlock->nextMemBlock = (void*)newBlock;
                    MemBlock* prevBlock = (MemBlock*)findPreviousMemBlock(page,(uintptr_t)firstFreeBlock);
                    if(prevBlock != NULL)   
                    {
                        prevBlock->nextMemBlock = (void*)firstFreeBlock;
                        if(prevBlock->size == -1) prevBlock->size = (uintptr_t)firstFreeBlock + sizeof(MemBlock) + firstFreeBlock->size;
                    }
                
                    // 4. Initialize the new recycled block
                    newBlock->currMemBlock = (void*)nextHeaderAddr;
                    newBlock->size = -1;
                    newBlock->nextMemBlock = firstFreeBlock->nextMemBlock;
                    newBlock->free = true;

                    // Update page metadata
                    page->freeBytesLeft -= (alignedReq + alignMemorySize(sizeof(MemBlock)));

                    return (void*)((uintptr_t)firstFreeBlock + alignMemorySize(sizeof(MemBlock)));

                }
                else //not worth splitting - just give the user the extra bytes (less than 40 usually)
                {
                    printf("[DEBUG] >> ACTION: ABSORBING into %p (Too small to split)\n", (void*)firstFreeBlock);
                    firstFreeBlock->free = false; //now its occupied
                    firstFreeBlock->size = freeBytesSequence + availableSpace; //the total size of the whole thing - we will give the user some extra bytes - won't exceed 40-30
                    MemBlock* prevBlock = (MemBlock*)findPreviousMemBlock(page,(uintptr_t)firstFreeBlock);
                    if(prevBlock != NULL)   
                    {
                        prevBlock->nextMemBlock = (void*)firstFreeBlock;
                        if(prevBlock->size == -1) prevBlock->size = (uintptr_t)firstFreeBlock + sizeof(MemBlock) + firstFreeBlock->size;
                    }
                    if(spaceLeft(page,(uintptr_t)(firstFreeBlock + firstFreeBlock->size),MIN_SPLIT_SIZE) >= MIN_SPLIT_SIZE)//check if we can add a next ptr(all we need is to make sure here that the block will be usable - is the min size or above)
                    {
                        
                        firstFreeBlock->nextMemBlock = (void*)((uintptr_t)(firstFreeBlock + firstFreeBlock->size));
                        MemBlock* nextBlock = (MemBlock*)(firstFreeBlock->nextMemBlock);
                        if(nextBlock->size == -1) //check if the next block needs pre-configuration:
                        {
                        //assign unconfigured flags
                        nextBlock->size = -1;
                        nextBlock->free = true;
                        }
                        page->freeBytesLeft -= freeBytesSequence;
                        return (void*)((uintptr_t)firstFreeBlock + alignMemorySize(sizeof(MemBlock)));
                    }
                }
            }
        }
        else//if the memmory block isn't free
        {
            printf("[DEBUG] Block occupied, skipping...\n");
            freeBytesSequence = 0;
            prev = curr;
            curr = (MemBlock*)curr->nextMemBlock;
            firstFreeBlock = curr;

        }
        
        // if(curr->nextMemBlock == NULL) { //last mem block
        //     uintptr_t nextPos = 0;
        //     size_t alignedReq = alignMemorySize(requestedSize);
        //     if(curr->size == -1) freeBytesSequence += spaceLeft(page,curr,alignedReq);
        //     else  freeBytesSequence += curr->size;
            
        //     if(freeBytesSequence < alignedReq) return NULL;
        //     nextPos = (uintptr_t)(curr) + alignMemorySize(sizeof(MemBlock)) + freeBytesSequence;
        //     //add the part that goes from the worthness protocol
        //     if(freeBytesSequence >= (long long)(alignedReq + alignMemorySize(sizeof(MemBlock)) + 8))
        //     {
        //         printf("[DEBUG] >> ACTION: SPLITTING at %p\n", (void*)firstFreeBlock);
        //         // 1. Configure the current block for the user
        //         firstFreeBlock->free = false;
        //         firstFreeBlock->size = (long long)alignedReq;

        //         // 2. Calculate the position of the NEW recycled header
        //         // We move past our current header and our data payload
        //         uintptr_t nextHeaderAddr = (uintptr_t)firstFreeBlock + alignMemorySize(sizeof(MemBlock)) + firstFreeBlock->size;
                
        //         MemBlock* newBlock = (MemBlock*)nextHeaderAddr;
                
        //         // 3. THE STITCH: Link the new block into the chain
        //         newBlock->nextMemBlock = curr->nextMemBlock; // Keep the original chain intact
        //         firstFreeBlock->nextMemBlock = (void*)newBlock;
        //         MemBlock* prevBlock = (MemBlock*)findPreviousMemBlock(page,(uintptr_t)firstFreeBlock);
        //         if(prevBlock != NULL)   prevBlock->nextMemBlock = (void*)firstFreeBlock;
            
        //         // 4. Initialize the new recycled block
        //         newBlock->currMemBlock = (void*)nextHeaderAddr;
        //         newBlock->size = -1;
        //         newBlock->nextMemBlock = firstFreeBlock->nextMemBlock;
        //         newBlock->free = true;

        //         // Update page metadata
        //         page->freeBytesLeft -= (alignedReq + alignMemorySize(sizeof(MemBlock)));

        //         return (void*)((uintptr_t)firstFreeBlock + alignMemorySize(sizeof(MemBlock)));

        //     }
        //     else //not worth splitting - just give the user the extra bytes (less than 40 usually)
        //     {
        //         printf("[DEBUG] >> ACTION: ABSORBING into %p (Too small to split)\n", (void*)firstFreeBlock);
        //         firstFreeBlock->free = false; //now its occupied
        //         firstFreeBlock->size = freeBytesSequence + availableSpace; //the total size of the whole thing - we will give the user some extra bytes - won't exceed 40-30
        //         MemBlock* prevBlock = (MemBlock*)findPreviousMemBlock(page,(uintptr_t)firstFreeBlock);
        //         if(prevBlock != NULL)   prevBlock->nextMemBlock = (void*)firstFreeBlock;//assign the next ptr to the prev if it exists
        //         // if (prev->free && prevBlock != NULL) prev->size = (long long)(((uintptr_t)(firstFreeBlock) - (uintptr_t)(prev)) - alignMemorySize(sizeof(MemBlock)));
                
        //         if(spaceLeft(page,(uintptr_t)(firstFreeBlock + firstFreeBlock->size),MIN_SPLIT_SIZE) >= MIN_SPLIT_SIZE)//check if we can add a next ptr(all we need is to make sure here that the block will be usable - is the min size or above)
        //         {
                    
        //             firstFreeBlock->nextMemBlock = (void*)((uintptr_t)(firstFreeBlock + firstFreeBlock->size));
        //             MemBlock* nextBlock = (MemBlock*)(firstFreeBlock->nextMemBlock);
        //             if(nextBlock->size == -1) //check if the next block needs pre-configuration:
        //             {
        //             //assign unconfigured flags
        //             nextBlock->size = -1;
        //             nextBlock->free = true;
        //             }
        //             page->freeBytesLeft -= freeBytesSequence;
        //             return (void*)((uintptr_t)firstFreeBlock + alignMemorySize(sizeof(MemBlock)));
        //         }
            // }



        // }
    }
    printf("[DEBUG] !! Page Full/End reached\n");
    return NULL;
}


//WIP
void* allocatePage()
{
    // 1. Request a fresh 4096-byte page from Windows
    void* newPageAddr = VirtualAlloc(NULL, PAGESIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (newPageAddr == NULL) return NULL;

    // 2. Initialize the PageNode at the very beginning of the page
    PageNode* newNode = (PageNode*)newPageAddr;
    newNode->currPagePtr = newPageAddr;
    newNode->nextPageNode = NULL;
    newNode->freeBytesLeft = PAGESIZE - sizeof(PageNode); // Only the header is "taken" initially

    // 3. Initialize the first "Wilderness" MemBlock immediately after the PageNode
    // We use (uintptr_t) + sizeof to ensure we move exactly the right number of bytes
    MemBlock* firstBlock = (MemBlock*)((uintptr_t)newPageAddr + sizeof(PageNode));
    firstBlock->size = -1;                // Your flag for "unconfigured/end of page"
    firstBlock->currMemBlock = firstBlock;
    firstBlock->nextMemBlock = NULL;
    firstBlock->free = true;

    // 4. Link this page into your global linked list
    if(firstPage == NULL)
    {
        firstPage = newNode;
    }
    else
    {
        PageNode* scanningPage = firstPage;
        while(scanningPage->nextPageNode != NULL)
        {
            scanningPage = (PageNode*)(scanningPage->nextPageNode);
        }
        scanningPage->nextPageNode = newNode;
    }

    return (void*)newNode;
}


void* miron_malloc(size_t size) {
    // 1. If we have no pages, make one!
    if (firstPage == NULL) {
        firstPage = (PageNode*)allocatePage();
        printf("new Page initialised because it is the first\n");
    }

    // 2. Try to allocate in existing pages
    PageNode* curr = firstPage;
    while (curr != NULL) {
        void* ptr = allocateMemBlock(curr, size);
        if (ptr != NULL) return ptr; // SUCCESS!
        
        // 3. If this page was full, check the next one
        if (curr->nextPageNode == NULL) {
            // 4. We hit the end and still no space? Create a new page!
            curr->nextPageNode = allocatePage();
            printf("new Page is initialised because all the pages are taken\n");
        }
        curr = (PageNode*)curr->nextPageNode;
    }
    return NULL; 
}


//testing space
int main()
{
    void* ptr1 = miron_malloc(16);
    void* ptr2 = miron_malloc(32);
    void* ptr3 = miron_malloc(64);

    printf("Ptr1: %p\nPtr2: %p\nPtr3: %p\n", ptr1, ptr2, ptr3);
    // printf("%zu",alignMemorySize(sizeof(PageNode)));
    return 0;
}

/*
thinking zone:

all things to take into account(logic):

procedure with handling each request:
Loop through each and every page that we have to check until we find an available spot
    in every page:
        - we start from the first mem block of the page
            - we have a prev = null and a current memblock pointers
            - we have a mem block that will save the first free block from the start of the sequence
            - when we reach a mem block with current:
                - if the block is free, we add the amount of bytes it can potentially provide
                  to a counter of free bytes (using a function)
                - if the blocks size is -1 -> its unconfigured.. so we calculate 
                  how much it can provide us (using the same function as before) which is the distance between the start of it and the end of the page (byte 4088)
                  - we will have a flag to handle the special procedure for the physically last mem block/s
                - if we get to a certain curr block - and we get to a point where the free bytes it gave is sufficient
                  we will always take in mind - the start of the first free block and the end of curr
                - if we are still not enough in bytes - we will check for void bytes - which essentially means 
                  if there are leftover and untouched bytes between two memmory blocks - the curr and next one


functions to have:
 - a function that can return the amount of space available from the firstFreeMemBlock to the last available byte in the row incuding the requested mem block(leftover size)
    it will return -9999 if there is no more space left to even fit the mem block 
    it will return a negative number(the amount insufficient number of bytes)
    it will return 0 or above if there is enough space to store the mem block in the currently scanned region(firstFreeMemBlock --> currBlock->nextPtr - 1)

 - a function that will 

in every single situation no matter what we have to:

1.we will have to track the previous node -> the previous one to the firstFreeMemBlock
2.we will handle being aware of not over-allocating bytes in memmory blocks and split memmory blocks into multiple for recycling

if nothing special happens:
    we check if the current range from firstFreeMemBlock to currBlock + its end
    is sufficient + is within the bound of the page + when we add the void spots from in between.
if we meet the last memBlock (size = -1):
    first we check if the          
*/