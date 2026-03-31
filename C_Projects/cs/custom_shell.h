#ifndef SHELL_H
#define SHELL_H

/* --- Standard Libraries --- */
#include <stdint.h>
#include <stdbool.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Constants & Macros --- */
#define MAX_INPUT_SIZE 200
#define DISTANCE_BETWEEN_ASCII_LOWER_UPPER 6
#define MIN_CHILDREN_COUNT 16
static char* VIRTUAL_REDIR = "redir";

//bitwise offsets for execAttrs
#define ATTR_REDIR_IN 2
#define ATTR_REDIR_OUT 4
#define ATTR_PIPE_IN 8
#define ATTR_PIPE_OUT 16
/* --- Forward Declarations --- */
// This tells the compiler "ShellCommand exists" so your other .h files 
// and the struct itself can use pointers to it immediately.
typedef struct ShellCommand ShellCommand;

/* --- Structure Definitions --- */

struct ShellCommand {
    char* commandName;
    uint64_t* args;
    char* path1;
    char* path2;
    HANDLE hIn;
    HANDLE hOut;
    DWORD execAttrs;
    ShellCommand* nextCommand; // This now works because of the forward declaration
};

typedef struct CommandRule {
    const char* name;
    const char* allowedFlags;
    const char* path1Requirements; // 'F'=No, 'T'=Req, 'O'=Opt | 'D'=Dir, 'F'=File | 'N'=New, 'E'=Exists
    const char* path2Requirements;
} CommandRule;

typedef struct CommandChain {
    CommandRule* chain;
} CommandChain;

/* --- Database External Declaration --- */
extern const CommandRule COMMAND_DB[];
extern const size_t DB_SIZE;

/* --- Function Prototypes --- */

// Utility & Bitwise
void print_binary64(uint64_t n);
bool arg_exists(uint64_t* offsetString, char c);
uint64_t loadArgsToken(char* token);
const char* formatSize(uint64_t bytes);

// Path & Filesystem Analysis
int sumOfDirParts(char* dir);
char pathAccessable(char* dir);
char determinePathType(char* path);
char pathCreatable(char* dir);
void generateAbsolutePath(char* inputPath, char* cwd, char* finalDestinationPath);
void translateENVars(char* destinationPath, char* userPath);

// Parsing & Validation
char* checkInputErrors(char* targetInputBuffer, char* cwd, char** fN, uint64_t* args, char** p1, char** p2);
ShellCommand* parse_input(char* inputBuffer, char* cwd);
CommandChain* craftCommandChain(char* inputBuffer, char* cwd);

/* --- Sub-Module Includes --- */
// We include these AFTER the structs are defined so they can 
// use 'ShellCommand*' in their function prototypes.
#include "../cma/custom_memory_allocator.h"
#include "command-cd.h"
#include "command-exe.h"
#include "command-ls.h"
#include "command-redir.h"

#endif // SHELL_H