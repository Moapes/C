#include <memoryapi.h>
#include <stdint.h> //for bitwise operations
#include <stdio.h> //basic standard lib
#include <stdlib.h> //module/header for basic dynamic memory allocation
#include <stdbool.h> //for boolean operation
#include <memoryapi.h> //for windows custom dynamic memory allocation api
#include <string.h>
#include <ctype.h>  // You'll need this for isupper()
#include <stdint.h> //for uintptr_t
#include <io.h> //for windows file search
//sys/stat and sys/types to handle path types(DIR/FILE)
#include <sys/stat.h>
#include <sys/types.h>

#include <windows.h>//for searching in the file system(windows official lib btw)

//for existing directory analysis of contents
#include <windows.h>

#include "../cma/custom_memory_allocator.h"

typedef struct ShellCommand{
    char* commandName;
    uint64_t* args;
    char* path1;
    char* path2;
}ShellCommand;


typedef struct CommandRule{
    const char* name;
    const char* allowedFlags; // Example: "-alo"
    const char* path1Requirements; //'F' for not allowed, 'T' for requried and 'O' for optional + 'D' for directory and 'F' for a file + 'N' for a new directory and 'E' for existing path required 
    const char* path2Requirements;//same with pathRequired
} CommandRule;

static const CommandRule COMMAND_DB[] = {
    {"ls", "aAlhRrtS","ODE","FFE"},
    {"cd", "","TDE","FFE"},
    {"mkdir", "p","TDN","FFN"},
    {"exit", "","FFE","FFE"}
};

#define DB_SIZE (sizeof(COMMAND_DB) / sizeof(CommandRule))
#define MAX_INPUT_SIZE 200

#define NUMBER_OF_FLAGS 52

#define DISTANCE_BETWEEN_ASCII_LOWER_UPPER 6

#define MIN_CHILDREN_COUNT 16



void print_binary64(uint64_t n) {
    // We iterate from the most significant bit (63) down to 0
    for (int i = 63; i >= 0; i--) {
        // Create a mask with a 1 at the current position 'i'
        // Then bitwise AND it with our number 'n'
        uint64_t mask = (uint64_t)1 << i;
        
        if (n & mask) {
            printf("1");
        } else {
            printf("0");
        }

        // Optional: Add a space every 8 bits for readability
        if (i > 0 && i % 8 == 0) {
            printf(" ");
        }
    }
    printf("\n");
}

bool arg_exists(uint64_t* offsetString,char c)
{
    bool isLowerCase = c >= 'a' && c <= 'z';
    int offset = c - 'A';

    if(isLowerCase) offset -= DISTANCE_BETWEEN_ASCII_LOWER_UPPER;
    return *offsetString & ((uint64_t)1 << offset);
}

int sumOfDirParts(char* dir)
{
    char* token = strtok(dir,"/");
    int c = 0;
    while(token != NULL)
    {
        token = strtok(NULL,"/");
        c++;    
    }
    return c;
}

//'F' for doesnt exists 'T' for accessable and 'P' for permission denied -> this is gonna be used only for path reading and not creation!!
char pathAccessable(char* dir)
{
    int result = _access(dir,0);
    if(result == 0)
    {
        if(_access(dir,6) != 0) return 'P'; 
        return 'T';
    }
    else return 'F';
}
//return 'E' for error, 'F' for file and 'D' for.. you guessed it -> a directory
char determinePathType(char* path)
{
    DWORD attrs = GetFileAttributes(path);
    if(attrs == INVALID_FILE_ATTRIBUTES) return 'E';
    else if(attrs & FILE_ATTRIBUTE_DIRECTORY) return 'D';
    else return 'F';
}

//return value meanings are the same as pathAccessables
//NOTE: the next two function pathCreatable aswell as pathAccessable should only be used after the directory has been confirmed to exist

//here we will have to check the directory itself and if there is an existing file with the same exact name
char pathCreatable(char *dir)
{
    //similiarly to dirCreatable but only the last part of the dir is supposed to be nonexistent and should be a file

    //get the header of the dir(the file)
    char* tempToken = strtok(dir,"/");//temp token for itiration
    char* restOfDir[strlen(dir)];
    char* headerToken;
    int cOfDirParts = sumOfDirParts(dir);
    int c = 0;
    while(tempToken != NULL)
    {
        c++;
        if(c == cOfDirParts) strcpy(headerToken,tempToken);            
        else
        {
            strcat(*restOfDir,tempToken);
            strcat(*restOfDir,"/");
        }


        tempToken = strtok(NULL,"/");
    }
    char pathAcsble = pathAccessable(*restOfDir);
    if(pathAccessable(*restOfDir) != 'T') return pathAcsble;//path isnt accessable therefore no file can be created

    //now we will check if there is a file with an idential name in this dir to decide what we doin
    
    //add a * wild card to the dir.. that way instead of searching for a specific file - we will itirate through the whole 
    // char pathWithWildcard[MAX_PATH];
    // sprintf(pathWithWildcard,"%s*",restOfDir);

    WIN32_FIND_DATA findData;

    HANDLE hFind = FindFirstFile(dir, &findData);
    if(hFind == INVALID_HANDLE_VALUE) 
    {
        FindClose(hFind);
        return 'F';
    }
    FindClose(hFind);
    return 'T';
    // do
    // {
    //     if(strcmp(findData.cFileName,".") == 0 || strcmp(findData.cFileName,"..") == 0) continue;
    //     else if(strcmp(findData.cFileName,headerToken) == 0)
    //     {
    //         FindClose(hFind);
    //         return 'F';
    //     } return 'F';
    // } while (FindNextFile(hFind,&findData));

    // FindClose(hFind);
    // return 'T';

}

//cwd - current workin directory
//the idea - we will loop from start to end on parts of the path, devided by the '/' thingy
/*
NOTES: we cannot go back from the root dir which is C:/ or D:/ (.. operation)

first handle the most upper part of the dir:
- if its starts like this C:/ or D:/ it starts as an absolute path
- if it starts with regular letters, like lets say Documents/etc.. we have to treat it as ./Documents meaning we append the CWD at the start
- if it starts with a ../ --> we have to go back one dir from the CWD and then keep going with it -> so ../Documents can be become C:/Users/Miron/Documents when the CWD was C:/Miron/Pictures
- if it starts with a / -> we simply treat it like a ./ as in the second point


in the middle
if there is a ./ or a /./ at any point we just jump to the next token
if there is a .. at any point, what we do is we cut the complete path back by the most upfront path - so C:/Users/Miron/Documents becomes C:/Users/Miron
*/

/*
refined idea:
we will go from the lowest point of the dir to the highest, basically reading the tokens from last to first by crafting an array of all the tokens
we will constantly check if we start to go over the CWD 
once we hit a .. -> we will enter a while loop that will - until the last .. sum up the amount of times we need to remove the smallest dir part from the temp CWD
our goal is at some point.. meet the end of the CWD - which is the disk itself e.g: D:/ or P:/ any letter Combo that represents a root drive

*/
void generateAbsolutePath(char* inputPath, char* cwd,char* finalDestinationPath)
{
    char userInputPath[256];
    strncpy(userInputPath, inputPath, sizeof(userInputPath) - 1);
    char* completePath[256] = {0};
    char* token = strtok(userInputPath,"/");
    char* arrOfTokenPointers[128] = {0};
    char* arrOfCWDPointers[128] = {0};
    //built an array of token pointers so we can scan it from top to bottom
    int i = 0;
    while(token != NULL)
    {
        arrOfTokenPointers[i] = token;
        i++; 
        token = strtok(NULL,"/");
    }
    
    bool passedDriverRoot = false;
    int j = 0;
    char* cwdToken = strtok(cwd,"/");
    while(cwdToken != NULL)
    {
        arrOfCWDPointers[j] = cwdToken;
        j++;
        cwdToken = strtok(NULL,"/");
    }
    bool includeWholeCWD = false;
    bool isAbsolute = false;
    bool includeMostCWD = false;
    //built the top dir
    if(arrOfTokenPointers[0][0] == '.')
    {
        if(arrOfTokenPointers[0][1] == '.')includeMostCWD = true;
        else 
        {
            includeWholeCWD = true;
            includeMostCWD = true;
        }


    }


 
    else if(isupper(arrOfTokenPointers[0][0]) && arrOfTokenPointers[0][1] == ':')
    {
        printf("absolute path found\n");
        isAbsolute = true;//root dir
    } 

    else if(islower(arrOfTokenPointers[0][0]) || isupper(arrOfTokenPointers[0][0]) && arrOfTokenPointers[0][1] != ':') 
    {
        printf("Regular word detected at the start\n");
        includeWholeCWD = true;//regular word, treat it as a regular ./ situation
    }
    //debug for first dir appearance handling, flags checkup first
    int iOfCompletePath = 0;
    if(includeMostCWD || includeWholeCWD) 
    {//j - is the len of the CWD by tokens
        for(int d = 0; d < j ; d++)
        {
            if(d == j - 1 && !includeWholeCWD) break;
            completePath[d] = arrOfCWDPointers[d];
            iOfCompletePath++;
        }
    }

    //now the result is that we have a starting dir to work with
    int dir;
    if(isAbsolute || includeWholeCWD) dir = 0;
    else dir = 1;
    for(dir; dir < i; dir++)
    {
        // printf("Scanning inputPath...  %s \n",arrOfTokenPointers[dir]);
        if(strcmp(arrOfTokenPointers[dir], ".") == 0)
        {
            // printf("Found a /./ case scenario, skeeping... \n");
            continue;
        } 
        else if(strcmp(arrOfTokenPointers[dir], "..") == 0)
        {
            if(iOfCompletePath > 0) 
            {
                // Look at the folder we are CURRENTLY in (index - 1)
                char* currentDir = completePath[iOfCompletePath - 1];
                
                if(isupper(currentDir[0]) && currentDir[1] == ':') {
                    continue; // Cannot go above root
                }
                iOfCompletePath--; // Move the stack pointer down
            }
        }
        else
        {
            completePath[iOfCompletePath] = arrOfTokenPointers[dir];
            iOfCompletePath++;
        }
    }
    //build the actual char*
    memset(finalDestinationPath, 0, sizeof(finalDestinationPath));//remove memory garbage first

    for(int dih = 0; dih < iOfCompletePath; dih++)
    {
        strcat(finalDestinationPath,completePath[dih]);
        if(dih + 1 != iOfCompletePath) strcat(finalDestinationPath,"/");//the if insures we do not 
    }
}

// char validatePath()




// NOTE: WE HAVE TO MODIFY PATH1TOKEN AND PATH2TOKEN WITH the returnAbsolutePath that will take any user input and interpert it (will work only if the complete path is valid)
char* checkInputErrors(char* targetInputBuffer,char* cwd)
{
    char inputBuffer[1024];
    strcpy(inputBuffer, targetInputBuffer);

    if(!inputBuffer || inputBuffer[0] == '\0') return NULL;

    char* nameToken = NULL; 
    char* argsToken = NULL;
    char* path1Token = NULL;
    char* path2Token = NULL;
    int countOfSection = 0;

    static char errorBuffer[256];
    
    char* token = strtok(inputBuffer, " ");

    while(token != NULL)
    {
        switch (countOfSection)
        {
            case 0:
                nameToken = token;
                break;

            case 1:
                if (token[0] == '-') argsToken = token; 
                else path1Token = token;
                break;

            case 2:
                path1Token = token;
                break;
            case 3:
                path2Token = token;
                break;
                
        }

        countOfSection++;
        token = strtok(NULL, " ");        
    }

    //checkup for the command itself
    //1.it exists
    //2.we will check if it requires a file path
    bool cmdExists = false;
    int commandPos;
    const char* path1Requirements;
    const char* path2Requirements;
    for(int c = 0; c < DB_SIZE; c++)
    {
        if(strcmp(COMMAND_DB[c].name, nameToken) == 0)
        {
            cmdExists = true;
            //save pathRequirement
            path1Requirements = COMMAND_DB[c].path1Requirements;
            path2Requirements = COMMAND_DB[c].path2Requirements;
            commandPos = c;//save the position of the command to simplify validating the rest of the command structure and syntax
            break;
        } 
    }
    if(!cmdExists)//if the command name input is incorrect we stop
    {
        sprintf(errorBuffer,"The Command: '%s' is not recognised in the scope of this shell.\n",nameToken);
        return errorBuffer;
    }


    //now time to check command args(only invalid if there is an invalid arg)
    if(argsToken)
    {
        //we strip the '-' part / we make the pointer to the string point to the index 1 [essentially cutting out the first '-' part]
        argsToken = argsToken + 1;
        for(int a = 0; a < strlen(argsToken); a++)
        {
            bool ArgFound = false;
            for(int a2 = 0; a2 < strlen(COMMAND_DB[commandPos].allowedFlags); a2++)
            if(COMMAND_DB[commandPos].allowedFlags[a2] == argsToken[a])
            {
                ArgFound = true;
                break;
            } 
            if(!ArgFound)
            {
                sprintf(errorBuffer,"The Argument/Flag '%c' is not recognised for the following Command : '%s'.\n",argsToken[a],COMMAND_DB[commandPos].name);
                return errorBuffer;
            }
        }
    }



    //Check PATHS (will be the most complicated)
    if(path1Token != NULL) 
    {
        //generate full path if it even exists ofc
        char* fullPath1 = (char*)miron_malloc(256);
        generateAbsolutePath(path1Token,cwd,fullPath1);
        //path1requirement check:
        if(COMMAND_DB[commandPos].path1Requirements[0] == 'F')//path isnt allowed
        {
            if(path1Token)
            {
                sprintf(errorBuffer,"Unknown command argument: '%s'\n",path1Token);
                return errorBuffer;
            }
        }
        else//path is optional/required
        {
            if(COMMAND_DB[commandPos].path1Requirements[0] == 'T' && !path1Token)//path required and not given
            {
                sprintf(errorBuffer,"(1) Argument missing (PATH)\n");
                return errorBuffer;
            }

            //this is for existing paths, we will have another part where we will handle
            char pathRequiredStatus = COMMAND_DB[commandPos].path1Requirements[2];
            if(pathRequiredStatus == 'E')
            {
                switch (pathAccessable(path1Token))
                {
                    case 'F'://path doesn't exists
                        sprintf(errorBuffer,"The given path: '%s' doesn't exist on this device\n",path1Token);
                        return errorBuffer;
                    case 'P'://path exists but innecasible(insufficient perms)
                        sprintf(errorBuffer,"Insufficient permissions for the given path: ''\n");
                    case 'T'://path accessable!
                        char pathType = determinePathType(path1Token);
                        char pathTypeRequired = COMMAND_DB[commandPos].path1Requirements[1];
                        if(pathTypeRequired != pathType)
                        {   
                            sprintf(errorBuffer,"Wrong path type given(%s instead of %s)\n",pathType == 'D' ? "Directory" : "File",pathTypeRequired == 'D' ? "Directory" : "File");
                            return errorBuffer;
                        }
                        return NULL;
                }
            }
            //a making of a new path -> returns 'N'
            else 
            {
                switch(pathCreatable(fullPath1))
                {
                    case 'F':
                        sprintf(errorBuffer,"failed to create the new %s, incorrect Directory\n",path1Requirements[1] == 'D' ? "Directroy" : "File");
                        return errorBuffer;
                    case 'P':
                        sprintf(errorBuffer,"insufficient permissions to access the directory\n");
                        return errorBuffer;
                    case 'T':
                        //success
                        return NULL;
                }
            }
        }
    }



    return NULL;

}

//the function will take modify the token of args and make it so it turns to a string of offsetted bits corresponding to their position in the A-z spot
//for example the flag B will be at offset 1 

//example:
//input : ABD
//output: ...000001011
uint64_t loadArgsToken(char* token)
{
    uint64_t argsToken = 0;//make it ...00000000 first
    for(int i = 0; i < strlen(token); i++)
    {
        bool isLowerCase = token[i] >= 'a' && token[i] <= 'z';
        
        int offset = token[i] - 'A';//calc offset by the distance from the first letter of the two alphabets(uppercase --> lowercase)
        if(isLowerCase) offset -= DISTANCE_BETWEEN_ASCII_LOWER_UPPER;//if the letter is lowercase , we gotta decrement the distance between the two alphabets
        argsToken |= 1U << offset;
    }
    return argsToken;
}

//this function takes an input that will look like this:
//<command>  -<flags> <path> (each property can be null besides the command name block)
//NOTE: WE WILL UPDATE IT SO THE CheckInputErrors function will already give us the tokens required for the parsing, so we won't do the parsing and the dir absolution 2 times and only 1 time, saving memmory and computing time
ShellCommand* parse_input(char* inputBuffer,char* cwd)
{
    if (!inputBuffer || inputBuffer[0] == '\0') return NULL;
    char* output = checkInputErrors(inputBuffer,cwd);
    if(output)
    {
        printf("%s",output);
        return NULL;
    }

    char* nameToken = NULL; 
    uint64_t argsToken = 0;
    char path1Token[256] = {0};
    char path2Token[256] = {0};
    size_t nameLen = 0;
    size_t argsLen = 0;
    size_t path1Len = 0;
    size_t path2Len = 0;

    char* token = strtok(inputBuffer, " ");
    int countOfSection = 0;

    bool foundFirstQoutes = false;


    while (token != NULL)
    {
        switch (countOfSection)
        {
            case 0:

                nameToken = token;
                nameLen = strlen(nameToken) + 1;
                break;

            case 1:
                if (token[0] == '-')
                {
                    argsToken = loadArgsToken(token);
                    argsLen = sizeof(uint64_t);
                }
                else
                {
                    generateAbsolutePath(token,cwd,path1Token);
                    path1Len = strlen(path1Token) + 1;
                    countOfSection++;
                }
                break;

            case 2:
                generateAbsolutePath(token,cwd,path1Token);
                path1Len = strlen(path1Token) + 1;
                break;
            case 3:
                generateAbsolutePath(token,cwd,path1Token);
                path2Len = strlen(path2Token) + 1;
                break;
        }   

        countOfSection++;
        token = strtok(NULL, " ");
    }
    //if path1 is optional and was not inputted - we have to put the cwd instead
    if(countOfSection <= 3 && path1Len == 0)
    {
        strcpy(path1Token,cwd);
        path1Len = strlen(cwd) + 1;
    }
    //same goes with path2
    else if(countOfSection == 4 && path2Len == 0)
    {
        strcpy(path2Token,cwd);
        path2Len = strlen(cwd) + 1;
    }
    // generateAbsolutePath(path1Token,cwd,path1Token);
    // generateAbsolutePath(path2Token,cwd,path2Token);
    //calc total size of all the user data to know exactly how much to allocate (totalsize + the size of the struct itself)
    size_t totalSize = nameLen + argsLen + path1Len + path2Len;

    ShellCommand* parsedCommand = miron_malloc(sizeof(ShellCommand) + totalSize);

    char* dataStart = (char*)(parsedCommand + 1);

    if(nameLen)
    {
        parsedCommand->commandName = dataStart;
        memcpy(dataStart, nameToken, nameLen);
        dataStart += nameLen;
    }
    else parsedCommand->commandName = NULL;

    if(argsLen)
    {
        uint64_t* tempDataStart = (uint64_t*)dataStart;
        parsedCommand->args = tempDataStart;
        *(uint64_t*)dataStart = argsToken;
        dataStart += sizeof(uint64_t);
    }
    else parsedCommand->args = NULL;

    if(path1Len)
    {
        parsedCommand->path1 = dataStart;
        memcpy(dataStart, path1Token, path1Len);
    }
    else parsedCommand->path1 = NULL;

    if(path2Len)
    {
        parsedCommand->path2 = dataStart;
        memcpy(dataStart, path2Token, path2Len);
    }
    else parsedCommand->path2 = NULL;

    return parsedCommand;
}



void printLinkTarget(const char* linkPath) {
    // 1. Open the link file (but don't follow it yet!)
    HANDLE hFile = CreateFileA(linkPath, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, 
                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        char targetPath[MAX_PATH];
        // 2. Ask Windows for the "Final" destination
        DWORD length = GetFinalPathNameByHandleA(hFile, targetPath, MAX_PATH, FILE_NAME_NORMALIZED);
        
        if (length > 0 && length < MAX_PATH) {
            // Windows adds a "\\?\" prefix to long paths, let's skip it for display
            char* displayPath = (strncmp(targetPath, "\\\\?\\", 4) == 0) ? targetPath + 4 : targetPath;
            printf(" -> %s", displayPath);
        }
        CloseHandle(hFile);
    }
}
/*
when we start to check each and every file and if its a dir we call the function again but we modify the scanned dir to be the sub-dir and return once it gets to the end
*/


//NOTE: for simplicity and testing, the first lsRecursive version WILL be without any checking of other command flags(only -R)
void lsRecursive(ShellCommand* currCommand,char* cwd,char* search_path,int offset)
{
    WIN32_FIND_DATA findData;
    ZeroMemory(&findData, sizeof(WIN32_FIND_DATA)); // The Windows way to memset
    HANDLE hFind = FindFirstFile(search_path,&findData);

    if(hFind == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        if(error == ERROR_ACCESS_DENIED)
        {
            printf("permission denied : '%s'\n",search_path);
        }
        // else if (error == ERROR_FILE_NOT_FOUND) --> this is how to check if a file is empty
        return;
    }
    //cycle through the folder, the moment we find another folder - we cycling through it through an additional function call
    bool showAll = arg_exists(currCommand->args,'a');//check -a flag
    bool showAlmostAll = arg_exists(currCommand->args,'A');//check -A flag

    char sortType = '\0';//we will collectievly check all of the sort-related flags, and the farest one in the alphabet will get chosen(ASCII)
    bool sortBySize = arg_exists(currCommand->args,'S');//sort by size, biggest first unless reversed
    bool sortByTimeStamp = arg_exists(currCommand->args,'t');//recent first if not reversed
    if(sortBySize) sortType = 't';
    if(sortBySize) sortType = 'S';
    bool reverseSort = arg_exists(currCommand->args,'r');//reverse the sorting if this flag is set

    int childCount = 0;
    int currentlyAllocatedCount = MIN_CHILDREN_COUNT;
    WIN32_FIND_DATA** sortedFileArray = (WIN32_FIND_DATA**)malloc(sizeof(WIN32_FIND_DATA*) * MIN_CHILDREN_COUNT);
    WIN32_FIND_DATA* filesArray = (WIN32_FIND_DATA*)malloc(sizeof(WIN32_FIND_DATA) * MIN_CHILDREN_COUNT);
    do{
        bool isNavigationDot = findData.cFileName[0] == '.';
        if(isNavigationDot && !showAll) continue; //if its a . / .. and there is no -a flag we skip aswell

        bool isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);
        if(isHidden &&  !showAlmostAll)
        {
            if(!showAll) continue;//if its hidden and there is no -a flag we skip
        }
        
        if(childCount == currentlyAllocatedCount)//we hit the limit/ re-allocate
        {
            currentlyAllocatedCount *= 2; //expand the allocated amount
            WIN32_FIND_DATA** temp1 = realloc(sortedFileArray,sizeof(WIN32_FIND_DATA*) * currentlyAllocatedCount);
            WIN32_FIND_DATA* temp2 = realloc(filesArray,sizeof(WIN32_FIND_DATA) * currentlyAllocatedCount);
            bool newArrayLocation = false;

            if(temp2 != filesArray) //if memory management has led to switching places in the memory, we will have to resync all the pointers that point to the files
            {
                newArrayLocation = true;
            }
            if(temp1 != NULL) sortedFileArray = temp1;
            if(temp2 != NULL) filesArray = temp2;       
            if(newArrayLocation) for(int i = 0; i < childCount; i++) sortedFileArray[i] = &filesArray[i];//resync all the pointers
        }
        //register the found child and insert it to the not sorted yet array:

        memcpy(&filesArray[childCount],&findData,sizeof(WIN32_FIND_DATA));
        sortedFileArray[childCount] = &filesArray[childCount];
        childCount++;
    } while(FindNextFile(hFind,&findData) != 0);

    //time to sort the loaded up array if needed:
    if(sortType)
    {
        for(int i = 0; i < childCount; i++)
        {
            for(int j = i;j < childCount; j++)
            {
                //check if a swap is needed by the sortType
                if(sortType == 't')
                {
                    if(reverseSort)//farest to recent
                    {
                        if(CompareFileTime(&sortedFileArray[i]->ftLastWriteTime,&sortedFileArray[j]->ftLastWriteTime) < 0)
                        {
                            WIN32_FIND_DATA* temp = sortedFileArray[j];
                            sortedFileArray[j] = sortedFileArray[i];
                            sortedFileArray[i] = temp; 
                        }
                    }
                    else//recent to farest
                    {
                        if(CompareFileTime(&sortedFileArray[i]->ftLastWriteTime,&sortedFileArray[j]->ftLastWriteTime) > 0)
                        {
                            WIN32_FIND_DATA* temp = sortedFileArray[j];
                            sortedFileArray[j] = sortedFileArray[i];
                            sortedFileArray[i] = temp; 
                        }
                    }
                }
                else if(sortType == 'S')
                {
                    uint64_t currFileSize = ((uint64_t)sortedFileArray[j]->nFileSizeHigh << 32) + sortedFileArray[j]->nFileSizeLow;
                    uint64_t targetFileSize = ((uint64_t)sortedFileArray[i]->nFileSizeHigh << 32) + sortedFileArray[i]->nFileSizeLow;
                    if(reverseSort)//smallest to biggest
                    {
                        if(currFileSize < targetFileSize)
                        {
                            WIN32_FIND_DATA* temp = sortedFileArray[j];
                            sortedFileArray[j] = sortedFileArray[i];
                            sortedFileArray[i] = temp; 
                        }
                    }
                    else//bigger to smallest
                    {
                        if(currFileSize > targetFileSize)
                        {
                            WIN32_FIND_DATA* temp = sortedFileArray[j];
                            sortedFileArray[j] = sortedFileArray[i];
                            sortedFileArray[i] = temp; 
                        }
                    }
                }
            }
        }
    }
    bool showMoreFileInfo = arg_exists(currCommand->args,'l');
    for(int i = 0; i < childCount; i++)
    {
        bool isNavigationDot = sortedFileArray[i]->cFileName[0] == '.';

        //print the offset\padding:
        for(int i = 0;i < offset; i++) printf("  ");
        //print the actual find:
        printf("%s",sortedFileArray[i]->cFileName);
        DWORD attrs = sortedFileArray[i]->dwFileAttributes;
        if(showMoreFileInfo)
        {   
            if(attrs & FILE_ATTRIBUTE_REPARSE_POINT)
            if(showAlmostAll || showAll) printf("  %s ",attrs & FILE_ATTRIBUTE_HIDDEN ? "(hidden)" : "");
            if(attrs & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                printf("l");
                printLinkTarget(sortedFileArray[i]->cFileName);
            }
            char x = '-';
            char* name = sortedFileArray[i]->cFileName;
            if(strstr(name,".exe") || strstr(name,".bat") || strstr(name,".cmd")) x = 'x';
            printf("%c",attrs & FILE_ATTRIBUTE_DIRECTORY ? 'd' : '-');
            printf("%s%c",attrs & FILE_ATTRIBUTE_READONLY ? "r-" : "rw",x);
            
            
        }
        printf("\n");
        if(!isNavigationDot)//of course we will print . / .. but not explore it
        {
            if(sortedFileArray[i]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)//true if its a directory:
            {//tatoo the current folder to our search_path
                int tempLen = strlen(search_path) + strlen(sortedFileArray[i]->cFileName) + 3;// + 1 for the null terminator and + 2 for the  new additional '/*'
                char temp[tempLen];
                strcpy(temp,search_path);
                temp[strlen(search_path) - 1] = '\0';//remove / null terminate the last *
                //cut the last * before moving to the next dir to avoid something like this: D:/Games/*/*/*/*, we dont want that right gentlemen?
                //add the new additional dir --> prev/curr/*
                strcat(temp,"/");
                strcat(temp,sortedFileArray[i]->cFileName);
                strcat(temp,"/*");
                lsRecursive(currCommand,cwd,temp,offset + 1);
            }
        }
    } 
    FindClose(hFind);
    free(sortedFileArray);
    free(filesArray);
    return;
}

void lsRecursiveWrapper(ShellCommand* currCommand,char* cwd)//Wrapper function
{
    char search_path[256];
    snprintf(search_path, 256, "%s/*",currCommand->path1);
    int offset = 0;
    lsRecursive(currCommand,cwd,search_path,offset);//the fourth arg for the function is the offset
    //the offset will be incremented from each level of recursive searching,so in the highest folder it'll be 0, and in the next inner one itll be 1 so it will look like this example:
    /*
    folder
        folder 2
        file 1
        folder 3
            folder 4
            file 2
            file 3
        folder 5
    */
}




void lsNoArgs(ShellCommand* currCommand,char* cwd)//WIP
{
    char search_path[256];
    //add a /* filter to basically tell the findData of windows.h to take everything that is inside that directory
    snprintf(search_path, 256, "%s/*",currCommand->path1); 

    WIN32_FIND_DATA findData;

    HANDLE hFind = FindFirstFile(search_path,&findData);

    //itirate through the dir until we find the border
    do
    {
        if(findData.cFileName[0] == '.' || findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN && !arg_exists(currCommand->args,'a')) continue; //skip .. and . dirs if there is no -a flag
        printf("%s \n",findData.cFileName);
        
    }while(FindNextFile(hFind, &findData ) != 0 );
}

bool ls(ShellCommand* currCommand,char* cwd)
{
    uint64_t* args = (uint64_t*)(currCommand->args);
    bool noArgs = !args;//we wont check args at all if there are none for the duration of the commands doings
    if(noArgs)//we can safely return true after since there cannot be any errors with args, and the shell commmand already finished the user inputs path
    {
        lsNoArgs(currCommand,cwd);
        return true;
    }
    bool isRecursive = arg_exists(args,'R');//check if the bit on the offset for recursiveness is on
    bool showAll = arg_exists(args,'a');
    bool showAlmostAll = arg_exists(args,'A');
    if(isRecursive)//here we will have to jump to a whole different sub-function since the scanning of files here is entierly different:
    {
        lsRecursiveWrapper(currCommand,cwd);
    }

    //flags that tells how to print out the file info
    // bool l = strchr(currCommand->args,'l') != NULL;//uses a long listing format, show perms, num of links, owner, group, size in bytes, and the last time of modification
    // bool h = strchr(currCommand->args,'h') != NULL;//human readable --> displayes file sizes in human-readable formats like K/M/G

    //flags that relate to what files to show and what not to:
    // bool a = strchr(currCommand->args,'a') != NULL;//Lists all files including hidden and .. / .
    // bool A = strchr(currCommand->args,'A') != NULL;//lists everything except .. / .

    //flag that tells how to search through the files (Recursive or not)
    // bool R = strchr(currCommand->args,'R') != NULL;//recursively list contents of all subdirs

    //flags that tell how to sort the order of files showing
    // bool t = strchr(currCommand->args,'t') != NULL;//sort the list by modification time(recent first)
    // bool r = strchr(currCommand->args,'r') != NULL;//reverse the sorting order
    // bool S = strchr(currCommand->args,'S') != NULL;//sort list by size

}


int main()
{
    char cwd[] = "D:";//first default cwd, in the future will be modifiable
    char* inputBuffer = (char*)miron_malloc(MAX_INPUT_SIZE);
    while(1)//loop forever and ask for command input from the user until he exits(exit command)
    {
        printf("%s> ",cwd);
        fgets(inputBuffer,MAX_INPUT_SIZE, stdin);
        inputBuffer[strcspn(inputBuffer, "\n")] = '\0';
        if(strlen(inputBuffer) == 0) continue;//additional shield to protect the allocator

        ShellCommand* currCommand = parse_input(inputBuffer,cwd);
        if(currCommand == NULL) continue;
        if(strcmp(currCommand->commandName,"exit") == 0)//if the user typed exit --> we close the shell down 
        {
            printf("Closing current shell...\n");
            break;
        }
        // if(strcmp(currCommand->commandName,"ls") == 0) ls(currCommand,cwd);
        if(strcmp(currCommand->commandName,"ls") == 0) ls(currCommand,cwd);//lsRecursive basic test


        memset(inputBuffer, 0, MAX_INPUT_SIZE);//null terminate the whole input buffer after every single interpertation
        freeMemBlock(currCommand);
    }
    // char* chars = miron_malloc(4);
    // fgets(chars, sizeof(chars),stdin);
    // chars[strcspn(chars,"\n")] = 0;
    // uint64_t n = loadArgsToken(chars);
    // print_binary64(n);

    


    // while(1)
    // {
    //     printf("\n%s>",cwd);

    //     if (fgets(inputBuffer, sizeof(inputBuffer), stdin) == NULL) break;

    //     // 3. Remove the annoying newline '\n' at the end
    //     inputBuffer[strcspn(inputBuffer, "\n")] = 0;
    
    //     if (strcmp(inputBuffer, "exit") == 0) {
    //         break;
    //     }
    //     printf("%s",checkInputErrors(inputBuffer,cwd));
    // }


    // // 1. Define the raw text
    // char* sourceText = "ls -alo C:/MironComputer/Documents";
    
    // // 2. Allocate space on YOUR heap (+1 for the null terminator!)
    // // We use strlen(sourceText) + 1 to ensure it's a valid C-string
    // char* heapInput = (char*)miron_malloc(strlen(sourceText) + 1);
    
    // // 3. Copy the literal into your writable heap block
    // strcpy(heapInput, sourceText);

    // // 4. Now parse it (strtok will now succeed because heapInput is writable)
    // ShellCommand* cmd = parse_input(heapInput);

    // if (cmd) {
    //     printf("command name : %s\n", cmd->commandName);
    //     printf("args         : %s\n", cmd->args ? cmd->args : "NONE");
    //     // printf("file path    : %s\n", cmd->filePath ? cmd->filePath : "NONE");
        
    //     // Since you "packed" the struct into one block, this one call 
    //     // cleans up the struct AND the strings.
    //     freeMemBlock((void*)(cmd)); 
    // }

    // // Don't forget to free the input buffer itself if you're done with it!
    // freeMemBlock((void*)(heapInput));



    // // Simulated Current Working Directory
    // char cwd[] = "C:/Users/Miron/Projects";
    
    // // Test 1: Complex Relative Path
    // char input1[] = "../Documents/./Secret/../Notes";
    // // Expected: "C:/Users/Miron/Notes"

    // // Test 2: Implicit Relative Path (no dots)
    // char input2[] = "Downloads/Pictures";
    // // Expected: "C:/Users/Miron/Projects/Downloads/Pictures"

    // // Test 3: Absolute Path (should ignore CWD)
    // char input3[] = "D:/Games/Minecraft";
    // // Expected: "D:/Games/Minecraft"

    // // Test 4: Root Guard (trying to go above C:/)
    // char input4[] = "../../../../../..";
    // // Expected: "C:/" (or NULL depending on your choice)

    // // Example call (assuming you handle the return string allocation)
    // printf("CWD: %s\n", cwd);
    // printf("Input: %s\n", input2);
    
    // char* result = miron_malloc(256);
    // returnAbsolutePath(input2, cwd,result);
    
    
    // printf("Resolved: %s\n", result);

    return 0;
}
