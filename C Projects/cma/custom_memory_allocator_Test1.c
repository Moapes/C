#define _DEFAULT_SOURCE
#include <stdio.h> //basic standard lib
#include <stdlib.h> //module/header for basic dynamic memory allocation
#include <stdbool.h> //for boolean operation
#include <memoryapi.h> //for windows custom dynamic memory allocation api

//in the future I will add calloc tho
//malloc and free features - from scratch using the windows memoryapi header

//virtual allocation 

// LPVOID VirtualAlloc(
//   [in, optional] LPVOID lpAddress, //mostly NONE, but u can insert a memory address to here in which the memory will point to
//   [in]           SIZE_T dwSize, //size in bytes to allocate, windows rounds it up to 4096 bytes/ 4KB as a single "page"
//   [in]           DWORD  flAllocationType, //do MEM_RESERVE | MEM_COMMIT, basically reserve the memory + give it to us
//   [in]           DWORD  flProtect //perms level for us to manage the memory - we will mostly use : PAGE_READWRITE
// );

//virtual freeing of memory:

// BOOL VirtualFree(
//     LPVOID lpAddress, //here we must insert the exact pointer that we've recieved at the functions virtualalloc counterpart
//     SIZE_T dwSize, //depends of dwFreeType, 0 if dwFreeType is MEM_RELEASE
//     DWORD  dwFreeType //MEM_RELEASE --> completely free memory
// );

// typedef struct malloc_memory_block{
//     char free; //'1' for free, '-1' for not free
//     size_t size;
//     void* firstPtr;

// } malloc_memory_block;



//version 1, failed I gotta do it differently..

// void *firstPtrEver = NULL;
// bool foundFirstPtr = false;


// //important, later.. we will have to somehow split the page into multiple memory blocks, and only allocate more once we ran out of memory completely
// size_t alignMemorySize(size_t requestedSize)
// {
//     if(requestedSize % 8 == 0)
//         return requestedSize;
//     size_t finalSize;
//     int tempResult = requestedSize + 8;
//     finalSize = tempResult - ( 8 - (tempResult % 8));
//     // tempResult--> (num + 8) - ( 8 - (ans % 8) --> gives round num

//     return finalSize;
// }

// //food for thought: have a starting point, and so I will have to literally scan for free space until I find one souly by my metadata headers
// //this way I can handle as much mallocing as I want


// //return a pointer to user memory stretching in (size) bytes

// void* my_malloc(size_t size)
// {
//     if(size <= 0)
//         return NULL;  
//     if(!foundFirstPtr)
//     {

//         //pointer to the metadata space

//         //metadata contains:
//         /*
//         char free -> '1' for free and '-1' for not free
//         void nextPtr* pointer to the next memory block (so we can handle further malloc calls and allocate the following heap memory)
//         size_t size -> the size of memory user data allocated
//         */
//         void *firstPtr;
//         void *pseudoPtr;
        // size_t metaDataSize = alignMemorySize((firstPtr) + sizeof(char)); //metadata block size calculation
        // size_t wholeBlockSize = alignMemorySize(size + metaDataSize); //calc total block size
        // //the allocated memory will include the metadata so we gotta size it up with addition to the metadata size in bytes.
        // firstPtr = VirtualAlloc(NULL, wholeBlockSize , MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); //allocate space and store the first pointer to it inside ptr

        
//         void *nextPtr = firstPtr + wholeBlockSize; //pointer to the next free memory (right after the allocated block)
//         char free = '-1'; //signing in the meta data that the memory is occupied
//         firstPtr =  


//         firstPtrEver = firstPtr; //now the very first pointer to the memory stream is found and will be relied on in free memory scanning

//         return firstPtr + metaDataSize; //return user data pointer
//     }

//     else{ //here we will implement searching for free memory

//     }

// }


// //freeing memory
// void my_free(void* ptr)
// {
    
// }


// //find the first block of memory available
// void* findFreeMemory()
// {

// }




// ---------------------- version 2 ----------------------

// ---------------------- main idea ----------------------
//have a counter that counts how much bytes are occupied - 
///have a counter for the total bytes allocated - 
//by these two parameters decide if to allocate a new page or not 



//crappy whiteboard for myself:

// [4096]:
// [metadata (16 bytes)][userdata (1024 bytes)] + [metadata (16 bytes)][userdata (1024 bytes)] + [metadata (16 bytes)][userdata (1024 bytes)] + oops not enough space(adding another page):
// [4096]:
// [metadata (16 bytes)][userdata (1024 bytes)]


size_t totalBytesOccupied = 0;
size_t totalBytesAllocated = 0;
bool debugging = false;
bool firstPageInitiated = false;



//Page headers
typedef struct PageNode{
    PageNode *nextPageNode; //self explanatory - it's next "Node"
    void *currPagePtr; //self explanatory - the pageHeader itself
    int bytesOccupied; //self explanatory - how much bytes are currently taken

} PageNode;

PageNode *firstPageNode = NULL;



//if there is still space for more memory blocks without allocation, we will just give out blocks of memory to the user
//if the user wishes to free the block, allow the user to do it(changing in metadata), look down for more:
//if the whole page got free, we will essentially 
//we will have a linked list of all of the pages, so if there is a new malloc request, we will have to go over all of the pages until we find a suitable spot
//that means we will have to be VERY precise with our metadata


//will work only if there is enough bytes in the most recent page
//we will first need to do a function to check in what page are we in so that way we can track the amount of bytes used up in that page - hasn't done yet
// - most part of that is down only need to update stats for the memory page 
// - more importantly, we have to check that if the memory block that is gettin' allocated is the last inside the page - it will point to the 
//next page if there is any.

void* allocateMemoryBlock(size_t requestedSize)
{
    void *firstPtr; 
    char free = '-1'; //first Meta Data member (in order)
    void *nextBlockPointer; //second Meta Data member(in order)

    size_t metaDataSize = alignMemorySize((nextBlockPointer) + sizeof(char)); //metadata block size calculation
    size_t wholeBlockSize = alignMemorySize(requestedSize + metaDataSize); //calc total block size
    //the allocated memory will include the metadata so we gotta size it up with addition to the metadata size in bytes.
    firstPtr = VirtualAlloc(NULL, wholeBlockSize , MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); //allocate space and store the first pointer to it inside ptr
    nextBlockPointer = firstPtr + wholeBlockSize;
    //embed free status to the block
    char *ptrForFreeStatus = firstPtr;
    *ptrForFreeStatus = free;

    //embed next block pointer to the block
    char *ptrForNextPtr  = ptrForFreeStatus + 1;
    *ptrForFreeStatus = nextBlockPointer;
    
    //return user data pointer 
    void *userPtr = firstPtr + metaDataSize;
    return userPtr;
} 




//free memory block, if the whole block gets empty, free it all  
//we will have to:
//make the previous block point to the next upcoming block - meaning we need a function that can locate both previous and next block
// - next block is decided by the metadata of the current block
// - previous block will be found 
//later we will have 
void freeMemoryBlock(void* ptr)
{

    char *charPtr = ptr;
    *charPtr = '1'; //modify the value in that memory address

    
}

//If the conditions are met the whole page will get free'd
//we will make it so the previous page(if there is any) - will point out to the very next page(again.. if there is any)
//if we got only a single page(worst case scenario btw) - we will have to basically reset everything and make it so
void freePage(void* pagePtr)
{
    //remove the pageNode from the page linked list first:
    
    PageNode *copyOfFirstNode = firstPageNode;
    //loop through the linked list, if we find the target pagePtr, we will cut it out of the linked list 
    // while(copyOfFirstNode->nextPageNode->currPagePtr != NULL)
    // {
    
    // }



    VirtualFree(pagePtr, 0, MEM_RELEASE); //free the whole page 


}

    //WIP -will search for and return a valid position for a new block
    //this function will actually handle using the freePage
void* scanForFreeMemory(size_t requstedSize)
{

}

//allocate a new page and return the pointer to it
//append the 
void* allocateNewPage()
{
    totalBytesAllocated += 4096;
    if(firstPageNode == NULL) //if the first page is not initalised, create it
    {
        firstPageNode->bytesOccupied = 0;
        firstPageNode->nextPageNode = NULL;
        return firstPageNode;
    }
    else{
        PageNode *copyOfFirstNode = firstPageNode; //make an alternate path to loop over the Pages to avoid losing the address of the first page

        while(copyOfFirstNode != NULL) //loop until we find an empty spot for a new page
        {
            copyOfFirstNode = copyOfFirstNode->nextPageNode;
        }
    
        //now when we found a null page, we will turn it into a new one
        copyOfFirstNode->bytesOccupied = 0;
        copyOfFirstNode->nextPageNode = NULL;   

        return copyOfFirstNode;
    }
}


PageNode getPageByPtr(void* TargetPtr)
{

}

void* my_malloc(size_t requestedSize)
{
    if(requestedSize > 4080) //if the size will be above 4096, no need to check, since our allocator is unable to process more than that
        return NULL;

    void *suitablePtr = scanForFreeMemory(requestedSize); //find a suitable spot for the memory block, if NONE is returned:
    if(suitablePtr == NULL)
    {
        allocateNewPage(); //allocate a new page
    }
    
}




int main() //future functionality testing
{

    return 0;
}
