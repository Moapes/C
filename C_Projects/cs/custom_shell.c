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

//header
#include "./custom_shell.h"

#include "../cma/custom_memory_allocator.h"


typedef struct ShellCommand{
    char* commandName;
    uint64_t* args;
    char* path1;
    char* path2;
    //handles for piping logic
    HANDLE hIn;
    HANDLE hOut;
    DWORD execAttrs;
    ShellCommand* nextCommand;//to create a chain-like (a linked list) system
}ShellCommand;


typedef struct CommandRule{
    const char* name;
    const char* allowedFlags; // Example: "-alo"
    const char* path1Requirements; //'F' for not allowed, 'T' for requried and 'O' for optional + 'D' for directory and 'F' for a file + 'N' for a new directory and 'E' for existing path required 
    const char* path2Requirements;//same with pathRequired
} CommandRule;


const CommandRule COMMAND_DB[] = {
    {"ls", "aAlhRrtS","ODE","FFE"},
    {"cd", "","TDE","FFE"},
    {"mkdir", "p","TDN","FFN"},
    {"exe","","TDE"},
    {"exit", "","FFE","FFE"},
    {"redir","","TFN","FFE"}
};

#define DB_SIZE (sizeof(COMMAND_DB) / sizeof(CommandRule))
#define MAX_INPUT_SIZE 200

#define NUMBER_OF_FLAGS 52

#define DISTANCE_BETWEEN_ASCII_LOWER_UPPER 6

#define MIN_CHILDREN_COUNT 16

static char* VIRTUAL_REDIR = "redir";

//bitwise offsets for execAttrs
#define ATTR_REDIR_IN 2
#define ATTR_REDIR_OUT 4
#define ATTR_PIPE_IN 8
#define ATTR_PIPE_OUT 16

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
    int cOfDirParts = sumOfDirParts(dir);
    int c = 0;
    while(tempToken != NULL)
    {
        c++;           
        if(c != cOfDirParts)
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
        return 'T';
    }
    FindClose(hFind);
    return 'F';

}

void generateAbsolutePath(char* inputPath, char* inputCwd,char* finalDestinationPath)
{
    char cwdBuf[strlen(inputCwd)];
    strcpy(cwdBuf,inputCwd);
    char* cwd = cwdBuf;
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

    //NOTE: for a correct file redirection, rules must be followed:
    /*
    1.only a single " " thingy
    2.it must be an accessable file
    */

    static char errorBuffer[256];
    char* currPtr = targetInputBuffer;//copy a pointer to the start to use it for advancing on the inputBuffer


    // --- 1. Get COMMAND NAME ---
    start = strspn(currPtr, " "); 
    //first check if its a redirection of a file scenario
    if(targetInputBuffer[start] == '"')
    {
        char* startOfFileName = &targetInputBuffer[start+1];
        if(startOfFileName != NULL)
        {
            char* fileNameEnd = strpbrk(startOfFileName,"\"");
            if(fileNameEnd == NULL)
            {
                sprintf(errorBuffer, "Syntax Error: Missing closing quote at supposed filename\n");
                return errorBuffer;
            }
            *fileNameEnd = '\0';//null terminate the qoute to establish bounadry
            char tempPath1[256] = {0};
            char* vPtr = tempPath1;
            generateAbsolutePath(startOfFileName,cwd,*p1);
            translateENVars(vPtr,*p1);
            DWORD dwAttrib = GetFileAttributes(*p1);
            if(dwAttrib != INVALID_FILE_ATTRIBUTES)//actual dir
            {
                if(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)//Its a folder(bad)
                {
                    sprintf(errorBuffer, "Logical Error: can't redirect folders / links\n");
                    return errorBuffer;
                }
                //a file we found it less go
                //last check, we gotta see that there is nothing outside the qoutation:
                if(strpbrk(fileNameEnd + 1," ") != NULL)//looks sum like this: "D:/Games/Game1.exe" -PP
                {
                    sprintf(errorBuffer,"Syntax Error: invalid output redirection args\n");
                    return errorBuffer;
                }
                //if everything is corret, we will put the info down
                *fN = VIRTUAL_REDIR;
                return NULL;
            }
        }
        
    }
    else
    {
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
        generateAbsolutePath(path1Token,cwd,*p1);
        translateENVars(vPtr,*p1);

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




ShellCommand* parse_input(char* inputBuffer,char* cwd)
{
    char* nameToken = NULL; 
    uint64_t argsToken = 0;
    char* path1Token = malloc(256);
    memset(path1Token,0,256);
    char* path2Token = malloc(256);
    memset(path2Token,0,256);

    if (!inputBuffer || inputBuffer[0] == '\0') return NULL;
    char* output = checkInputErrors(inputBuffer,cwd,&nameToken,&argsToken,&path1Token,&path2Token);
    if(output)
    {
        printf("%s\n",output);
        free(path1Token);
        free(path2Token);
        return NULL;
    }

    size_t userInputSize = strlen(nameToken) + sizeof(uint64_t) + strlen(path1Token) + strlen(path2Token) + 4; // +4 for null terminators
    size_t additionalSize = sizeof(ShellCommand) + (sizeof(HANDLE) * 2) + sizeof(ShellCommand*) + sizeof(DWORD);
    size_t totalSize = userInputSize + additionalSize;
    ShellCommand* parsedCommand = malloc(totalSize);

    // FIX 1: Initialize the WHOLE block
    memset(parsedCommand, 0, totalSize);

    char* dataStart = (char*)(parsedCommand) + additionalSize;

    if(nameToken != NULL)
    {
        parsedCommand->commandName = dataStart;
        memcpy(dataStart, nameToken, strlen(nameToken) + 1);
        dataStart += strlen(nameToken) + 1;
    }

    if(argsToken != 0)
    {
        uint64_t* tempDataStart = (uint64_t*)dataStart;
        parsedCommand->args = tempDataStart;
        *tempDataStart = argsToken;
        dataStart += sizeof(uint64_t);
        *dataStart = '\0';
        dataStart += 1; 
    }

    if(strlen(path1Token) != 0)
    {
        parsedCommand->path1 = dataStart;
        memcpy(dataStart, path1Token, strlen(path1Token) + 1);
        dataStart += strlen(path1Token) + 1;
    }

    if(strlen(path2Token) != 0)
    {
        parsedCommand->path2 = dataStart;
        memcpy(dataStart, path2Token, strlen(path2Token) + 1);
        // FIX 2: Use the correct token length for the increment
        dataStart += strlen(path2Token) + 1; 
    }

    free(path1Token);
    free(path2Token);
    
    return parsedCommand;
}


void freeChainedCommands(ShellCommand** firstCommand)
{
    if(firstCommand == NULL || *firstCommand == NULL) return;
    freeChainedCommands(&((*firstCommand)->nextCommand));
    HANDLE hInput = (*firstCommand)->hIn;
    HANDLE hOutput = (*firstCommand)->hOut;
    if(hInput != NULL && hInput != GetStdHandle(STD_INPUT_HANDLE)) CloseHandle(hInput);
    if(hOutput != NULL && hOutput != GetStdHandle(STD_OUTPUT_HANDLE)) CloseHandle(hOutput);
    free(*firstCommand);
    *firstCommand = NULL;
}

ShellCommand** chainCommands(char* inputBuffer,char* cwd)
{
    if (inputBuffer == NULL) return NULL;

    char* currPtr = inputBuffer;
    ShellCommand* firstCommand = NULL;
    ShellCommand* prevCommand = NULL;
    ShellCommand* currCommand = NULL;

    // --- STEP 1: Handle the First Command ---
    int end = strcspn(currPtr, "<>|");
    char* endOfCommand = currPtr + end;
    char divider = *endOfCommand; 
    bool isLast = (divider == '\0'); // It's the last one if we hit the end of string

    *endOfCommand = '\0'; // Split the first command
    firstCommand = parse_input(currPtr, cwd);
    
    if (firstCommand == NULL) return NULL; // If first parse fails, we're done

    currCommand = firstCommand;
    prevCommand = firstCommand;
    // Move pointer past the divider for the next iteration
    currPtr = endOfCommand + 1;

    int c = 1;
    while(true)
    {
        end = strcspn(currPtr,"<>|");
        char* endOfCommand = currPtr + end;
        bool isLast = *endOfCommand !='|' && *endOfCommand != '<' && *endOfCommand != '>';  //if a |<> isnt met that means that this is the last command

        char dividerTemp = *endOfCommand;//save the type of 
        *endOfCommand = '\0';
        currCommand = parse_input(currPtr,cwd);
        c++;
        if(currCommand == NULL)
        {
            prevCommand->nextCommand = NULL;
            break;
        }

        if(prevCommand != NULL)//make the combo
        {
            bool comboFailed = false;
            switch (divider)
            {
                case '|':
                    HANDLE hRead, hWrite;
                    SECURITY_ATTRIBUTES sp;

                    sp.nLength = sizeof(SECURITY_ATTRIBUTES);
                    sp.bInheritHandle = TRUE;    // CRITICAL: Allows the child process to "see" the handle
                    sp.lpSecurityDescriptor = NULL;

                    if (!CreatePipe(&hRead, &hWrite, &sp, 0)) {
                        printf("piping creation failed..\n");
                        comboFailed = true;
                    }
                    prevCommand->hOut = hWrite;
                    prevCommand->hOut = hWrite;
                    prevCommand->execAttrs |= ATTR_PIPE_OUT;
                    currCommand->hIn = hRead;
                    currCommand->execAttrs |= ATTR_PIPE_IN;

                    break;
                case '>': 
                    // 1. Setup Security (Same as you did for Pipe)
                    SECURITY_ATTRIBUTES sl = { sizeof(sl), NULL, TRUE }; 

                    // 2. Open/Create the file
                    // nextCommand->args[0] is the filename (e.g., "out.txt")
                    HANDLE mFile = CreateFile(
                        currCommand->path1,// Filename
                        GENERIC_WRITE,            // We want to write to it
                        FILE_SHARE_READ,          // Let others read it while we work
                        &sl,                      // Inheritance MUST be TRUE
                        CREATE_ALWAYS,            // Overwrite if exists, create if not
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                    );

                    if (mFile == INVALID_HANDLE_VALUE) {
                        printf("Error: Could not open file for redirection.\n");
                        comboFailed = false;
                    } else 
                    {
                        prevCommand->hOut = mFile;//If everything went good.. we insert the hOut to be the file instead of the printing/output
                        prevCommand->execAttrs |= ATTR_REDIR_OUT;
                        currCommand->execAttrs |= ATTR_REDIR_IN;
                    }
                    break;
            

                case '<':
                    // 1. Setup Security (Same as you did for Pipe)
                    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE }; 

                    // 2. Open/Create the file
                    // nextCommand->args[0] is the filename (e.g., "out.txt")
                    HANDLE hFile = CreateFile(
                        currCommand->path1,// Filename
                        GENERIC_WRITE,            // We want to write to it
                        FILE_SHARE_READ,          // Let others read it while we work
                        &sa,                      // Inheritance MUST be TRUE
                        CREATE_ALWAYS,            // Overwrite if exists, create if not
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                    );

                    if (hFile == INVALID_HANDLE_VALUE) {
                        printf("Error: Could not open file for redirection.\n");
                        comboFailed = false;
                    } else
                    {
                        currCommand->hOut = hFile;//If everything went good.. we insert the hOut to be the file instead of the printing/output
                        currCommand->execAttrs |= ATTR_REDIR_OUT;
                        prevCommand->execAttrs |= ATTR_REDIR_IN;
                    } 
                    break;
            }
            
        }

        prevCommand->nextCommand = currCommand;
        if(dividerTemp == '\0') break;
        if(isLast) break;
        prevCommand = currCommand;
        currPtr += strspn(currPtr, " ");
        divider = dividerTemp;
    }        
    
    //free all the parsedCommands with a recursive function:
    ShellCommand** finalResult = malloc(sizeof(ShellCommand*));
    if(finalResult) *finalResult = firstCommand;
    return finalResult;
}





bool executeInternal(ShellCommand** command,char** cwd)
{
    char* cName = (*command)->commandName;
    if(strcmp(cName,"redir") == 0) return true;//skip redir
    bool success = false;
    if(strcmp(cName,"cd") == 0) success = cd(*command,cwd);
    else if(strcmp(cName,"ls") == 0) success = ls(*command,*cwd);
    // else if(strcmp(cName,"redir") == 0) success = redir(*command);
    else if(strcmp(cName,"exe") == 0) success = exe(*command);
    return success;
}

bool executeCommand(ShellCommand** command, char** cwd)
{
    bool success;
    success = executeInternal(command,cwd);
    return success;
}
//first we will modify the commmands to write files if there is a handle to it
void executeCommandsChain(ShellCommand** firstCommand,char** cwd)
{
    ShellCommand** curr = firstCommand;//save the pointer
    bool success = true;
    while(curr != NULL && *curr != NULL && success)
    {
        success = executeCommand(curr, cwd);
        curr = &((*curr)->nextCommand);
    }   
    freeChainedCommands(firstCommand);
}




int main()
{
    char first_cwd_text[] = "D:/C/C_Projects/cma";//first default cwd, in the future will be modifiable
    char* cwd = malloc(strlen(first_cwd_text) + 1);//+1 for null terminator
    strcpy(cwd,first_cwd_text);
    char* inputBuffer = (char*)malloc(MAX_INPUT_SIZE);
    memset(inputBuffer,0,MAX_INPUT_SIZE);
    while(1)//loop forever and ask for command input from the user until he exits(exit command)
    {
        printf("%s> ",cwd);
        fgets(inputBuffer,MAX_INPUT_SIZE, stdin);
        inputBuffer[strcspn(inputBuffer, "\n")] = '\0';
        if(strlen(inputBuffer) == 0) continue;//additional shield to protect the allocator

        ShellCommand** firstCommand = chainCommands(inputBuffer,cwd);
        executeCommandsChain(firstCommand,&cwd);
        free(firstCommand);
        
        rewind(stdin);
        memset(inputBuffer,0,MAX_INPUT_SIZE);
    }
    free(inputBuffer);
    free(cwd);

    return 0;
}