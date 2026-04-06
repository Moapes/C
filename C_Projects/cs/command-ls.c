#include "./command-ls.h"

// void printLinkTarget(const char* linkPath) {
//     // 1. Open the link file (but don't follow it yet!)
//     HANDLE hFile = CreateFileA(linkPath, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, 
//                               FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

//     if (hFile != INVALID_HANDLE_VALUE) {
//         char targetPath[MAX_PATH];
//         // 2. Ask Windows for the "Final" destination
//         DWORD length = GetFinalPathNameByHandleA(hFile, targetPath, MAX_PATH, FILE_NAME_NORMALIZED);
        
//         if (length > 0 && length < MAX_PATH) {
//             // Windows adds a "\\?\" prefix to long paths, let's skip it for display
//             char* displayPath = (strncmp(targetPath, "\\\\?\\", 4) == 0) ? targetPath + 4 : targetPath;
//             printf(" -> %s", displayPath);
//         }
//         CloseHandle(hFile);
//     }
// }

//NOTE: for simplicity and testing, the first lsRecursive version WILL be without any checking of other command flags(only -R)
bool ls_wrapped(ShellCommand* currCommand,char* cwd,char* search_path,int offset)
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
        return false;
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
        char outputBuffer[1024] = {0};
        bool isNavigationDot = sortedFileArray[i]->cFileName[0] == '.';

        //print the offset\padding:
        for(int i = 0;i < offset; i++) printf("  ");
        //print the actual find:
        strcat(outputBuffer,sortedFileArray[i]->cFileName);
        DWORD attrs = sortedFileArray[i]->dwFileAttributes;
        if(showMoreFileInfo)
        {   
            strcat(outputBuffer,"  ---  ");
            
            // 1. Permissions / Type String
            char type = (attrs & FILE_ATTRIBUTE_DIRECTORY) ? 'd' : '-';
            if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) type = 'l'; // Symbolic link
            
            char* name = sortedFileArray[i]->cFileName;
            char exec = (strstr(name,".exe") || strstr(name,".bat") || strstr(name,".cmd")) ? 'x' : '-';
            char* readWrite = (attrs & FILE_ATTRIBUTE_READONLY) ? "r-" : "rw";
            
            // 1. Create a small buffer for this specific line
            char line[MAX_PATH + 128]; 

            // 2. Format the entire string at once. 
            // %c handles the chars (type, exec), %s handles the string (readWrite)
            snprintf(line, sizeof(line), "%c%s%c %s\n", 
                    type, 
                    readWrite, 
                    exec, 
                    name);

            // 3. Now add the completed line to your main outputBuffer
            strcat(outputBuffer, line);
            
            // 2. File Size (Combining High and Low DWORDs)
            uint64_t fileSize = ((uint64_t)sortedFileArray[i]->nFileSizeHigh << 32) | sortedFileArray[i]->nFileSizeLow;
            
            // If it's a directory, typical 'ls' behavior is to show 0 or a block size
            if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                strcat(outputBuffer," <DIR> ");
            } else {
                // Use our new human-readable formatter
                strcat(outputBuffer,formatSize(fileSize));
            }

            // 3. Modification Time
            SYSTEMTIME stUTC, stLocal;
            FileTimeToSystemTime(&sortedFileArray[i]->ftLastWriteTime, &stUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal); // Convert to your local time

            char dateBuf[64];

            snprintf(dateBuf, sizeof(dateBuf)," %02d/%02d/%d %02d:%02d ", 
                stLocal.wDay, stLocal.wMonth, stLocal.wYear, 
                stLocal.wHour, stLocal.wMinute);

            strcat(outputBuffer,dateBuf);
            // if(attrs & FILE_ATTRIBUTE_REPARSE_POINT)
            // {
            //     strcat(outputBuffer,sortedFileArray[i]->cFileName);
            // }
            
            strcat(outputBuffer,"  ---  ");
        }
        strcat(outputBuffer,"\n");
        if(currCommand->execAttrs & ATTR_PIPE_OUT || currCommand->execAttrs & ATTR_REDIR_OUT)//send the buffer to the target file if there is one(by handle)
        {
            DWORD bytesWritten;
            if(!WriteFile(currCommand->hOut,outputBuffer,sizeof(outputBuffer),&bytesWritten,NULL))
            {
                printf("Error Writing to file: %lu\n",GetLastError());
                return false;
            }
        }
        else printf("%s",outputBuffer);//no handle --> just print it
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
                    bool success = ls_wrapped(currCommand,cwd,temp,offset + 1);
                    if(!success) return false;
                }
            }
        }
    } 
    FindClose(hFind);
    free(sortedFileArray);
    free(filesArray);
    return true;
}

bool lsNoArgs(ShellCommand* currCommand,char* cwd,char* search_path)//WIP
{
    //add a /* filter to basically tell the findData of windows.h to take everything that is inside that directory
    strcat(search_path,"/*"); 

    WIN32_FIND_DATA findData;

    HANDLE hFind = FindFirstFile(search_path,&findData);

    //itirate through the dir until we find the border
    HANDLE destination = currCommand->hOut;
    bool destinationExists = destination != NULL;
    DWORD bytesWritten;
    do
    {
        if(findData.cFileName[0] == '.' || findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue; //skip .. and . dirs if there is no -a flag
        if(destinationExists)//if we need to write the output to another handle and not regular printing:
        {
            if(!WriteFile(destination, findData.cFileName, strlen(findData.cFileName),&bytesWritten, NULL))
            {
                printf("Failed to write to pipe, Error %lu\n",GetLastError());
                return false;
            }
        }

        else printf("%s \n",findData.cFileName);//if no handle just print
    }while(FindNextFile(hFind, &findData ) != 0 );


    return true;
}

bool ls(ShellCommand* currCommand,char* cwd)
{
    char search_path[MAX_PATH] = {0};
    char readBuff[1024];
    HANDLE source = currCommand->hIn;
    DWORD execAttrs = currCommand->execAttrs;
    if(execAttrs & ATTR_PIPE_IN)//check if I need to read info from anywhere else
    {   
        DWORD bytesRead;
        while(ReadFile(source, readBuff ,strlen(readBuff),&bytesRead, NULL) && bytesRead > 0)
        {//in every iteration we gotta load the chunk to the search_path
            readBuff[bytesRead] = '\0';//cut the top off of it

            strcat(search_path,readBuff);//append it at the end of search_path
        }
    }
    else if(search_path[0] == '\0')//just take the input from the command
    {
        snprintf(search_path, MAX_PATH, "%s/*",currCommand->path1);
    }
    uint64_t* args = (uint64_t*)(currCommand->args);
    bool noArgs = !args;//we wont check args at all if there are none for the duration of the commands doings
    if(noArgs)//we can safely return true after since there cannot be any errors with args, and the shell commmand already finished the user inputs path
    {
        bool success = lsNoArgs(currCommand,cwd,search_path);
        if(!success) return false;
        return true;
    }

    
    int offset = 0;
    bool success = ls_wrapped(currCommand,cwd,search_path,offset);
    if(!success) return false;
    HANDLE dest = currCommand->hOut;
    return true;
}