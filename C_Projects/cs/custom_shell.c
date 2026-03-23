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
    {"exe","","TDE"},
    {"exit", "","FFE","FFE"},
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
    if(offsetString == 0) return false;
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

}

void generateAbsolutePath(char* inputPath, char* cwd,char* finalDestinationPath)
{
    if(inputPath == NULL) 
    {
        memset(finalDestinationPath, 0, sizeof(finalDestinationPath));//remove memory garbage first
        memcpy(finalDestinationPath,cwd,strlen(cwd));
        return;
    }
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


//the function will take modify the token of args and make it so it turns to a string of offsetted bits corresponding to their position in the A-z spot
//for example the flag B will be at offset 1 

//example:
//input : ABD
//output: ...000001011
uint64_t loadArgsToken(char* token)
{
    uint64_t argsToken = 0;//make it ...00000000 first
    if(token == NULL) return 0;
    for(int i = 0; i < strlen(token); i++)
    {
        bool isLowerCase = token[i] >= 'a' && token[i] <= 'z';
        
        int offset = token[i] - 'A';//calc offset by the distance from the first letter of the two alphabets(uppercase --> lowercase)
        if(isLowerCase) offset -= DISTANCE_BETWEEN_ASCII_LOWER_UPPER;//if the letter is lowercase , we gotta decrement the distance between the two alphabets
        argsToken |= (uint64_t)1 << offset;
    }
    return argsToken;
}


void translateENVars(char* destinationPath, char* userPath) 
{
    char* src = userPath;
    char* dst = destinationPath;
    while(*src)
    {
        if(*src == '%')
        {
            char* secondPercent = strchr(src + 1,'%');
            if(secondPercent)//found it
            {
                size_t lenOfKey = secondPercent - (src + 1);//+1 since the src points to the first %
                char key[lenOfKey];
                //copy everything to the key:
                strncpy(key,src + 1,lenOfKey);
                key[lenOfKey] = '\0';//null terminate jsut in case
                char value[4096];//we can do a big value doesnt matter
                printf("[%s]",key);
                DWORD res = GetEnvironmentVariableA(key, value, 4096);
                if(res > 0)//success
                {
                    char* vPtr = value;
                    while(*vPtr)
                    {
                        *dst++ = *vPtr++;//load the value
                    }
                    src = secondPercent + 1;//move the src pointer past ts
                    continue;
                }
            }
        }
        *dst++ = *src++;
    }
    *dst = '\0';//dont forget to null terminate
}


// NOTE: WE HAVE TO MODIFY PATH1TOKEN AND PATH2TOKEN WITH the returnAbsolutePath that will take any user input and interpert it (will work only if the complete path is valid)
char* checkInputErrors(char* targetInputBuffer,char* cwd,char** fN,uint64_t* args,char** p1,char** p2)
{
    if(!targetInputBuffer || targetInputBuffer[0] == '\0') return NULL;
    int originalFullLen = strlen(targetInputBuffer);//original size for safe iteration
    char* nameToken = NULL; 
    char* argsToken = NULL;
    char* path1Token = NULL;
    char* path2Token = NULL;
    int countOfSection = 0;
    //these two variables will help us determine boundaries:
    int start = 0;
    int end = 0;

    static char errorBuffer[256];
    char* currPtr = targetInputBuffer;//copy a pointer to the start to use it for advancing on the inputBuffer

    // --- 1. Get COMMAND NAME ---
    start = strspn(currPtr, " "); 
    // Use strcspn to find the first SPACE (the end of the word)
    end = start + strcspn(currPtr + start, " "); 
    nameToken = &targetInputBuffer[start];
    targetInputBuffer[end] = '\0'; // Cut it

    // --- 2. Advance to the NEXT section ---
    currPtr = &targetInputBuffer[end + 1]; 
    start = strspn(currPtr, " ");
    currPtr += start; // Now currPtr is at the start of Args or Path1

    bool argsMet = false;
    bool path1Met = false;
    // --- 3. Handle ARGS (if they exist) ---
    if(currPtr < &targetInputBuffer[originalFullLen])//quick boundary check
    {
        if (*currPtr == '-') {
            argsToken = currPtr;
            end = strcspn(currPtr, " "); 
            currPtr[end] = '\0'; // Cut the args
            
            // Advance currPtr past the args to find Path1
            currPtr += (end + 1);
            currPtr += strspn(currPtr, " "); // Skip spaces after args
            argsMet = true;
        }

        // --- 4. Handle PATH 1 ---
        if(currPtr < &targetInputBuffer[originalFullLen])//quick boundary check
        {
            if (*currPtr == '"') {
                path1Token = currPtr + 1; // Skip the opening "
                end = strcspn(path1Token, "\""); // Find the closing "
                
                if (path1Token[end] == '\0') {
                    sprintf(errorBuffer, "Syntax Error: Missing closing quote for Path 1\n");
                    return errorBuffer;
                }
                
                path1Token[end] = '\0'; // Cut the path
                currPtr = &path1Token[end + 1]; // Move past Path 1
                path1Met = true;
            }
        
            if(!path1Met && !argsMet)//the user just typed random shii instead of opening for args or path
            {
                sprintf(errorBuffer,"Syntax Error: Unexpected input in the command call\n");
                return errorBuffer;
            }
        }
    
        // --- 5. Handle PATH 2 ---
        start = strspn(currPtr, " "); // Skip spaces between Path 1 and Path 2
        currPtr += start;
        if(currPtr < &targetInputBuffer[originalFullLen])//quick boundary check
        {

            if (*currPtr == '"') {
                path2Token = currPtr + 1; // Skip opening "
                end = strcspn(path2Token, "\"");
                
                if (path2Token[end] == '\0') {
                    sprintf(errorBuffer, "Syntax Error: Missing closing quote for Path 2\n");
                    return errorBuffer;
                }
                
                path2Token[end] = '\0'; // Cut Path 2
            }
            else//again, the user hasn't typed shii right, we need a "".. not a random character
            {
                sprintf(errorBuffer,"Syntax Error: Unexpected input in the command call\n");
                return errorBuffer;        
            }
        }
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
        if(strlen(argsToken) == 0)//if the user just typed '-' and stopped
        {
            sprintf(errorBuffer,"Invalid Arguments Syntax: missing actual flags\n");
            return errorBuffer;
        }
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
    //generate full path if it even exists ofc
    //path1requirement check:
    if(COMMAND_DB[commandPos].path1Requirements[0] == 'F')//path isnt allowed
    {
        if(*p1)
        {
            sprintf(errorBuffer,"Unknown command argument: '%s'\n",path1Token);
            return errorBuffer;
        }
    }   
    else//path is optional/required
    {
        if(COMMAND_DB[commandPos].path1Requirements[0] == 'T' && *p1 == NULL)//path required and not given
        {
            sprintf(errorBuffer,"(1) Argument missing (PATH)\n");
            return errorBuffer;
        }
        char tempPath1[256] = {0};
        char* vPtr = tempPath1;
        translateENVars(vPtr,path1Token);
        generateAbsolutePath(vPtr,cwd,*p1);
        //this is for existing paths, we will have another part where we will handle
        char pathRequiredStatus = COMMAND_DB[commandPos].path1Requirements[2];
        if(pathRequiredStatus == 'E')
        {
            switch (pathAccessable(*p1))
            {
                case 'F'://path doesn't exists
                    sprintf(errorBuffer,"The given path: '%s' doesn't exist on this device\n",path1Token);
                    return errorBuffer;
                case 'P'://path exists but innecasible(insufficient perms)
                    sprintf(errorBuffer,"Insufficient permissions for the given path: ''\n");
                    return errorBuffer;
                case 'T'://path accessable!
                    char pathType = determinePathType(*p1);
                    char pathTypeRequired = COMMAND_DB[commandPos].path1Requirements[1];
                    if(pathTypeRequired != pathType)
                    {   
                        sprintf(errorBuffer,"Wrong path type given(%s instead of %s)\n",pathType == 'D' ? "Directory" : "File",pathTypeRequired == 'D' ? "Directory" : "File");
                        return errorBuffer;
                    }
            }
        }
        //a making of a new path -> returns 'N'
        else 
        {
            switch(pathCreatable(*p1))
            {
                case 'F':
                    sprintf(errorBuffer,"failed to create the new %s, incorrect Directory\n",path1Requirements[1] == 'D' ? "Directroy" : "File");
                    return errorBuffer;
                case 'P':
                    sprintf(errorBuffer,"insufficient permissions to access the directory\n");
                    return errorBuffer;
            }
        }
    }
    
    //if we passed all the tests.. we load all the info up to the command block args:
    *fN = nameToken;
    uint64_t argsResult = loadArgsToken(argsToken);
    *args = argsResult;
    return NULL;

}




//this function takes an input that will look like this:
//<command>  -<flags> <path> (each property can be null besides the command name block)
//NOTE: WE WILL UPDATE IT SO THE CheckInputErrors function will already give us the tokens required for the parsing, so we won't do the parsing and the dir absolution 2 times and only 1 time, saving memmory and computing time
ShellCommand* parse_input(char* inputBuffer,char* cwd)
{
    char* nameToken = NULL; 
    uint64_t argsToken = 0;
    char* path1Token = miron_malloc(256);
    char* path2Token = miron_malloc(256);

    if (!inputBuffer || inputBuffer[0] == '\0') return NULL;
    char* output = checkInputErrors(inputBuffer,cwd,&nameToken,&argsToken,&path1Token,&path2Token);
    if(output)
    {
        printf("%s",output);
        freeMemBlock(path1Token);
        freeMemBlock(path2Token);
        return NULL;
    }

    //calc total size of all the user data to know exactly how much to allocate (totalsize + the size of the struct itself)
    size_t totalSize = strlen(nameToken) + sizeof(uint64_t) + strlen(path1Token) + strlen(path2Token);

    ShellCommand* parsedCommand = miron_malloc(sizeof(ShellCommand) + totalSize);

    char* dataStart = (char*)(parsedCommand + 1);

    if(nameToken != NULL)
    {
        parsedCommand->commandName = dataStart;
        memcpy(dataStart, nameToken, strlen(nameToken) + 1);//same heree brotein shake
        dataStart += strlen(nameToken) + 1;
    }
    else parsedCommand->commandName = NULL;

    if(argsToken != 0)
    {
        uint64_t* tempDataStart = (uint64_t*)dataStart;
        parsedCommand->args = tempDataStart;
        *tempDataStart = argsToken;
        dataStart += sizeof(uint64_t);
        *dataStart = '\0';
        dataStart += 1; 
    }
    else parsedCommand->args = NULL;

    if(strlen(path1Token) != 0)
    {
        parsedCommand->path1 = dataStart;
        memcpy(dataStart, path1Token, strlen(path1Token) + 1);//+1 to capture the null terminator
        dataStart += strlen(path1Token) + 1;
    }
    else parsedCommand->path1 = NULL;

    if(strlen(path2Token) != 0)
    {
        parsedCommand->path2 = dataStart;
        memcpy(dataStart, path2Token, strlen(path2Token) + 1);//same here
        dataStart += strlen(path1Token) + 1;
    }
    else parsedCommand->path2 = NULL;
    freeMemBlock(path1Token);
    freeMemBlock(path2Token);
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
//function for showing nice file size(until TB only)
const char* formatSize(uint64_t bytes) {
    static char buffer[32];
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = (double)bytes;

    while (size >= 1024 && i < 4) {
        size /= 1024;
        i++;
    }

    // If it's just bytes, don't show decimals. Otherwise, show 2 decimal places.
    if (i == 0) {
        snprintf(buffer, sizeof(buffer), "%llu %s", bytes, units[i]);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[i]);
    }

    return buffer;
}

//NOTE: for simplicity and testing, the first lsRecursive version WILL be without any checking of other command flags(only -R)
void ls_wrapped(ShellCommand* currCommand,char* cwd,char* search_path,int offset)
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
        FindClose(hFind);
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
    bool isRecursive = arg_exists(currCommand->args,'R');
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
            printf("  ---  ");
            
            // 1. Permissions / Type String
            char type = (attrs & FILE_ATTRIBUTE_DIRECTORY) ? 'd' : '-';
            if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) type = 'l'; // Symbolic link
            
            char* name = sortedFileArray[i]->cFileName;
            char exec = (strstr(name,".exe") || strstr(name,".bat") || strstr(name,".cmd")) ? 'x' : '-';
            char* readWrite = (attrs & FILE_ATTRIBUTE_READONLY) ? "r-" : "rw";
            
            printf("%c%s%c ", type, readWrite, exec);

            // 2. File Size (Combining High and Low DWORDs)
            uint64_t fileSize = ((uint64_t)sortedFileArray[i]->nFileSizeHigh << 32) | sortedFileArray[i]->nFileSizeLow;
            
            // If it's a directory, typical 'ls' behavior is to show 0 or a block size
            if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                printf("%10s ", "<DIR>");
            } else {
                // Use our new human-readable formatter
                printf("%10s ", formatSize(fileSize));
            }

            // 3. Modification Time
            SYSTEMTIME stUTC, stLocal;
            FileTimeToSystemTime(&sortedFileArray[i]->ftLastWriteTime, &stUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal); // Convert to your local time

            printf(" %02d/%02d/%d %02d:%02d ", 
                stLocal.wDay, stLocal.wMonth, stLocal.wYear, 
                stLocal.wHour, stLocal.wMinute);

            if(attrs & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                printLinkTarget(sortedFileArray[i]->cFileName);
            }
            
            printf(" --- ");
        }
        printf("\n");
        //recursion:
        if(isRecursive)
        {
            if(!isNavigationDot)//of course we will print . / .. but not explore it
            {
                if(sortedFileArray[i]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)//true if its a directory:
                {//tatoo the current folder to our search_path
                    int tempLen = strlen(search_path) + strlen(sortedFileArray[i]->cFileName) + 3;// + 1 for the null terminator and + 2 for the  new additional '/*'
                    char temp[tempLen];
                    memset(temp,0,tempLen);
                    strcpy(temp,search_path);
                    temp[strlen(search_path) - 1] = '\0';//remove / null terminate the last *
                    //cut the last * before moving to the next dir to avoid something like this: D:/Games/*/*/*/*, we dont want that right gentlemen?
                    //add the new additional dir --> prev/curr/*
                    // strcat(temp,"/");
                    strcat(temp,sortedFileArray[i]->cFileName);
                    strcat(temp,"/*");
                    ls_wrapped(currCommand,cwd,temp,offset + 1);
                }
            }
        }
    } 
    FindClose(hFind);
    free(sortedFileArray);
    free(filesArray);
    return;
}

void lsNoArgs(ShellCommand* currCommand,char* cwd)//WIP
{
    char search_path[2400];
    //add a /* filter to basically tell the findData of windows.h to take everything that is inside that directory
    snprintf(search_path, 2400, "%s/*",currCommand->path1); 

    WIN32_FIND_DATA findData;

    HANDLE hFind = FindFirstFile(search_path,&findData);

    //itirate through the dir until we find the border
    do
    {
        if(findData.cFileName[0] == '.' || findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue; //skip .. and . dirs if there is no -a flag
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

    char search_path[2400];
    snprintf(search_path, 2400, "%s/*",currCommand->path1);
    int offset = 0;
    ls_wrapped(currCommand,cwd,search_path,offset);

}

//in the cd, we will reallocate the amount of bytes needed to hold the new cwd and switch the main's pointer to it(and get rid of the old one)
void cd(ShellCommand* currCommand,char** cwd)
{
    int newLen = strlen(currCommand->path1);//check how much we need to allocate for the new cwd
    char* newCWDPtr = (char*)miron_malloc(newLen + 1);//allocated it( +1 for null terminator)
    strcpy(newCWDPtr,currCommand->path1);//copy the contents to the new pointer
    freeMemBlock(*cwd);//free the old pointer
    *cwd = newCWDPtr;//new pointerr
}

void exe(ShellCommand* currCommand)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if(!CreateProcessA(
        NULL,
        currCommand->path1,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    ))
    {
        printf("Process Creation Failed (%d).\n",GetLastError());
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    printf("\nProcess finished. Returning to shell...\n");
}


int main()
{
    char first_cwd_text[] = "D:";//first default cwd, in the future will be modifiable
    char* cwd = miron_malloc(strlen(first_cwd_text) + 1);//+1 for null terminator
    strcpy(cwd,first_cwd_text);
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
        if(strcmp(currCommand->commandName,"ls") == 0) ls(currCommand,cwd);//ls command
        if(strcmp(currCommand->commandName,"cd") == 0) cd(currCommand,&cwd);//cd command
        if(strcmp(currCommand->commandName,"exe") == 0) exe(currCommand);//exe command


        memset(inputBuffer, 0, MAX_INPUT_SIZE);//null terminate the whole input buffer after every single interpertation
        freeMemBlock(currCommand);
    }
    freeMemBlock(inputBuffer);
    freeMemBlock(cwd);

    return 0;
}
