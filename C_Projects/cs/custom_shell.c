#include <memoryapi.h>
#include <stdio.h> //basic standard lib
#include <stdlib.h> //module/header for basic dynamic memory allocation
#include <stdbool.h> //for boolean operation
#include <memoryapi.h> //for windows custom dynamic memory allocation api
#include <stdint.h> //for uintptr_t
#include <io.h> //for windows file search
//sys/stat and sys/types to handle path types(DIR/FILE)
#include <sys/stat.h>
#include <sys/types.h>

#include "../cma/custom_memory_allocator.h"

typedef struct ShellCommand{
    char* commandName;
    char* args;
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
    {"ls", "alo","ODE","FFE"},
    {"cd", "","TDE","FFE"},
    {"mkdir", "p","TDN","FFN"},
    {"exit", "","FFE","FFE"}
};

#define DB_SIZE (sizeof(COMMAND_DB) / sizeof(CommandRule))











//'F' for file and 'D' for dir
char determinePathType(char* token)
{
    struct stat st;
    _stat(token,&st);

    if(st.st_mode & _S_IFDIR) return 'D';
    return 'F';
}
//'F' for doesnt exists 'T' for accessable and 'P' for permission denied
char pathAccessable(char* token)
{
    int result = _access(token,0);
    if(result == 0)
    {
        if(_access(token,6) != 0) return 'P'; 
        return 'T';
    }
    else return 'F';
}

//return value meanings are the same as pathAccessables

char dirCreatable(char* token)
{
    if(strcmp(pathAccessable(token),'F') != 0) return 'F';
    //here it will be way more complicated since in the close future, we will handle correctness in a way that we will:
    //loop from the top dir of the path to the bottom until we find a the start of the non existient directory - and if in the non existient create-ready part
    //we will find a part that exists - the dir automatically becomes uncreatable

}

char fileCreatable(char* token)
{
    //similiarly to dirCreatable but only the last part of the dir is supposed to be nonexistent and should be a file
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
char* returnAbsolutePath(char* userInputPath, char* cwd)
{
    char completePath[256];

}


//NOTE: WE HAVE TO MODIFY PATH1TOKEN AND PATH2TOKEN WITH the returnAbsolutePath that will take any user input and interpert it (will work only if the complete path is valid)
char* checkInputErrors(char* inputBuffer)
{
    if(!inputBuffer || inputBuffer[0] == '\0') return NULL;

    char* nameToken = NULL; 
    char* argsToken = NULL;
    char* path1Token = NULL;
    char* path2Token = NULL;
    int countOfSection;

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
    char* path1Requirements;
    char* path2Requirements;
    for(int c = 0; c < DB_SIZE; c++)
    {
        if(strcmp(COMMAND_DB[c].name, nameToken))
        {
            cmdExists = true;
            //save pathRequirement
            path1Requirements = COMMAND_DB[c].path1Requirements;
            path2Requirements = COMMAND_DB[c].path2Requirements;
            commandPos = c;//save the position of the command to simplify validating the rest of the command structure and syntax
            break;
        } 
    }
    if(!cmdExists)
    {
        sprintf(errorBuffer,sizeof(errorBuffer),"The Command: '%s' is not recognised in the scope of this shell.",nameToken);
        return errorBuffer;
    }


    //now time to check command args(only invalid if there is an invalid arg)
    if(argsToken)
    {
        for(int a = 0; a < strlen(argsToken); a++)
        {
            bool ArgFound = false;
            for(int a2 = 0; a2 < strlen(COMMAND_DB[commandPos].allowedFlags); a2++) if(strcmp(COMMAND_DB[commandPos].allowedFlags[a2], argsToken[a])) ArgFound = ~ArgFound;
            if(!ArgFound)
            {
                sprintf(errorBuffer,sizeof(errorBuffer),"The Argument/Flag '%c' is not recognised for the following Command : '%s'.",argsToken[a],COMMAND_DB[commandPos].name);
                return errorBuffer;
            }
        }
    }




    //Check PATHS (will be the most complicated)
    // - for this we will use the (WIP)

    //path1requirement check:
    if(strcmp(COMMAND_DB[commandPos].path1Requirements[0],'F') == 0)//path isnt allowed
    {
        if(path1Token)
        {
            sprintf(errorBuffer,sizeof(errorBuffer),"Unknown command argument: '%s'",path1Token);
            return errorBuffer;
        }
    }
    else//path is optional/required
    {
        if(strcmp(COMMAND_DB[commandPos].path1Requirements[0],'T') == 0 && !path1Token)//path required and not given
        {
            sprintf(errorBuffer,sizeof(errorBuffer),"(1) Argument missing (PATH)");
            return errorBuffer;
        }

        //this is for existing paths, we will have another part where we will handle
        char pathRequiredStatus = COMMAND_DB[commandPos].path1Requirements[2];
        if(strcmp(pathRequiredStatus,'E') == 0)
        {
            switch (pathAccessable(path1Token))
            {
                case 'F'://path doesn't exists
                    sprintf(errorBuffer,sizeof(errorBuffer),"The given path: '%s' doesn't exist on this device",path1Token);
                    return errorBuffer;
                case 'P'://path exists but innecasible(insufficient perms)
                    sprintf(errorBuffer,sizeof(errorBuffer),"Insufficient permissions for the given path: ''");
                case 'T'://path accessable!
                    char pathType = determinePathType(path1Token);
                    char pathTypeRequired = COMMAND_DB[commandPos].path1Requirements[1];
                    if(strcmp(pathTypeRequired,pathType) != 0)
                    {   
                        sprintf(errorBuffer,sizeof(errorBuffer),"Wrong path type given(%s instead of %s)",pathType == 'D' ? "Directory" : "File",pathTypeRequired == 'D' ? "Directory" : "File");
                        return errorBuffer;
                    }
                    return NULL;
            }
        }
        //a making of a new path
        else
        {
            
        }
    }



    return NULL;

}




//this function takes an input that will look like this:
//<command>  -<flags> <path> (each property can be null besides the command name block)
//NOTE: WE WILL UPDATE IT SO THE CheckInputErrors function will already give us the tokens required for the parsing, so we won't do the parsing and the dir absolution 2 times and only 1 time, saving memmory and computing time
ShellCommand* parse_input(char* inputBuffer)
{
    if (!inputBuffer || inputBuffer[0] == '\0')
        return NULL;

    char* nameToken = NULL; 
    char* argsToken = NULL;
    char* path1Token = NULL;
    char* path2Token = NULL;

    size_t nameLen = 0;
    size_t argsLen = 0;
    size_t path1Len = 0;
    size_t path2Len = 0;

    char* token = strtok(inputBuffer, " ");
    int countOfSection = 0;

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
                    argsToken = token; 
                    argsLen = strlen(argsToken) + 1;
                }
                else
                {
                    path1Token = token;
                    path1Len = strlen(path1Token) + 1;
                }
                break;

            case 2:
                path1Token = token;
                path1Len = strlen(path1Token) + 1;
                break;
            case 3:
                path2Token = token;
                path2Len = strlen(path2Token) + 1;
        }

        countOfSection++;
        token = strtok(NULL, " ");
    }

    size_t totalSize = nameLen + argsLen + path1Len + path2Len;

    ShellCommand* parsedCommand =
        miron_malloc(sizeof(ShellCommand) + totalSize);

    char* dataStart = (char*)(parsedCommand + 1);

    if (nameLen)
    {
        parsedCommand->commandName = dataStart;
        memcpy(dataStart, nameToken, nameLen);
        dataStart += nameLen;
    }
    else
        parsedCommand->commandName = NULL;

    if (argsLen)
    {
        parsedCommand->args = dataStart;
        memcpy(dataStart, argsToken, argsLen);
        dataStart += argsLen;
    }
    else
        parsedCommand->args = NULL;

    if (path1Len)
    {
        parsedCommand->path1 = dataStart;
        memcpy(dataStart, path1Token, path1Len);
    }
    else
        parsedCommand->path1 = NULL;

    if (path2Len)
    {
        parsedCommand->path2 = dataStart;
        memcpy(dataStart, path2Token, path2Len);
    }
    else
        parsedCommand->path2 = NULL;

    return parsedCommand;
}




int main()
{
    // 1. Define the raw text
    char* sourceText = "ls -alo C:/MironComputer/Documents";
    
    // 2. Allocate space on YOUR heap (+1 for the null terminator!)
    // We use strlen(sourceText) + 1 to ensure it's a valid C-string
    char* heapInput = (char*)miron_malloc(strlen(sourceText) + 1);
    
    // 3. Copy the literal into your writable heap block
    strcpy(heapInput, sourceText);

    // 4. Now parse it (strtok will now succeed because heapInput is writable)
    ShellCommand* cmd = parse_input(heapInput);

    if (cmd) {
        printf("command name : %s\n", cmd->commandName);
        printf("args         : %s\n", cmd->args ? cmd->args : "NONE");
        // printf("file path    : %s\n", cmd->filePath ? cmd->filePath : "NONE");
        
        // Since you "packed" the struct into one block, this one call 
        // cleans up the struct AND the strings.
        freeMemBlock((void*)(cmd)); 
    }

    // Don't forget to free the input buffer itself if you're done with it!
    freeMemBlock((void*)(heapInput));

    return 0;
}
