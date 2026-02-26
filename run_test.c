#define _DEFAULT_SOURCE
#include <stdio.h> //basic standard lib
#include <stdlib.h> //module/header for basic dynamic memory allocation
#include <stdbool.h> //for boolean operation
#include <memoryapi.h> //for windows custom dynamic memory allocation api

int main()
{
    size_t size = 5;
    int ye = size;
    printf("%d",ye);


    return 0;
}


// //virtual allocation 

// // LPVOID VirtualAlloc(
// //   [in, optional] LPVOID lpAddress, //mostly NONE, but u can insert a memory address to here in which the memory will point to
// //   [in]           SIZE_T dwSize, //size in bytes to allocate, windows rounds it up to 4096 bytes/ 4KB as a single "page"
// //   [in]           DWORD  flAllocationType, //do MEM_RESERVE | MEM_COMMIT, basically reserve the memory + give it to us
// //   [in]           DWORD  flProtect //perms level for us to manage the memory - we will mostly use : PAGE_READWRITE
// // );

// //virtual freeing of memory:

// // BOOL VirtualFree(
// //     LPVOID lpAddress, //here we must insert the exact pointer that we've recieved at the functions virtualalloc counterpart
// //     SIZE_T dwSize, //depends of dwFreeType, 0 if dwFreeType is MEM_RELEASE
// //     DWORD  dwFreeType //MEM_RELEASE --> completely free memory
// // );

// // typedef struct malloc_memory_block{
// //     char free; //'1' for free, '-1' for not free
// //     size_t size;
// //     void* firstPtr;

// // } malloc_memory_block;

// //return a pointer to user memory stretching in (size) bytes
// void* my_malloc(size_t size)
// {   //pointer to the metadata space
//     //metadata contains:
//     /*
//     char free -> '1' for free and '0' for not free
//     void nextPtr* pointer to the next memory block (so we can handle further malloc calls and allocate the following heap memory)
//     size_t size -> the size of memory user data allocated
//     */
//     void *firstPtr;
//     void *pseudoPtr;
//     size_t metaDataSize = sizeof(firstPtr) + sizeof(char); //metadata block size calculation
//     size_t wholeBlockSize = size + metaDataSize; //calc total block size
//     //the allocated memory will include the metadata so we gotta size it up with addition to the metadata size in bytes.
//     firstPtr = VirtualAlloc(NULL, wholeBlockSize , MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); //allocate space and store the first pointer to it inside ptr
    







    



    

//     return firstPtr + metaDataSize + 1; //return user data pointer
// }


// void my_free(void* ptr)
// {

// }

