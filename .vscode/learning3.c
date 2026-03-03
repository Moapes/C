#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

bool checkDistances(char* s, int* distance, int distanceSize)
{   
    int* distancesArr = calloc(distanceSize, sizeof(int)); 
    for(int i = 0;i < distanceSize; i++)
    {
        distancesArr[i] = -1;
    }

    int i = 0;
    while(s[i] != '\0')
    {
        int posInDistance = (int)(s[i] - 'a');
        printf("%d\n",posInDistance);
        if(distancesArr[posInDistance] != -1)
        {
            printf("Last Letter Found inside position: %d",i);
            if(i - distancesArr[posInDistance] - 1 < distance[(int)(s[i] -'a')]) return false;
        }
        else
        {
            distancesArr[posInDistance] = i;
        }
        i++;
    }
    free(distancesArr);
    return true;
}


//we will have a firstIndex and lastIndex
//each time, if the prefix remains(remains = true) we will increment the lastIndex by one
//the moment that the prefix stops, we will increment the starting index and make the lastindex point to the same spot
//we will change the maxPrefix only if the current one is longer than maxPrefix
//to change the max prefix we will save it in a variable that 

char* longestCommonPrefix(char** strs, int strsSize) {
    for (int i = 0; strs[0][i]; i++) {
        for (int j = 1; j < strsSize; j++) {
            if (strs[j][i] != strs[0][i]) {
                strs[0][i] = '\0';
                return strs[0];
            }
        }
    }
    return strs[0];
}
        




// int divide(int dividend, int divisor) {
//     int result;
//     int nC = 0;
//     int temp;
//     if(divisor == 0) return 0;//CANT DEVIDE BY 0
//     if(dividend == 0) return 0;//0 divided by X is always 0
//     if(divisor == 1) return dividend; //DEVISION BY 1 returns the starting dividend
//     bool flipOfResultRequired = false;
//     if(divisor < 0 && dividend < 0) flipOfResultRequired = true;
//     if(divisor > 0)
//     {
//         temp = divisor;
//         divisor -= temp;
//         divisor -= temp;   
//         nC++;
//     }  
//     if(dividend > 0)
//     {
//         temp = dividend;
//         dividend -= temp;
//         dividend -= temp;
//         nC++;
//     }  
//     if(nC == 2) flipOfResultRequired = true;//the result is supposed to be negative
//     int i = 0;
//     for(i = 0; dividend <= divisor; i--)
//     {
//         dividend -= divisor;
//     } 
//     if(flipOfResultRequired) 
//     {
//         if(i == INT_MIN) i = INT_MAX;
//         else{
//             temp = i;
//             i -= temp;
//             i -= temp;
//         }

//     }

//     return i;
    
// }


int divide(int dividend, int divisor) 
{
    // 1. Cast to long long immediately to prevent overflow and shift errors
    long long lDvd = (long long)dividend;
    long long lDvs = (long long)divisor;
    
    // 2. Determine the sign and convert to absolute values
    bool isNegative = (lDvd < 0) ^ (lDvs < 0);
    if (lDvd < 0) lDvd = -lDvd;
    if (lDvs < 0) lDvs = -lDvs;
    
    long long result = 0;
    
    // 3. Fast Bit-Shifting Logic
    while (lDvd >= lDvs) {
        long long tempDvs = lDvs;
        long long multiple = 1;
        
        // Double the divisor until it's bigger than the remaining dividend
        while (lDvd >= (tempDvs << 1)) {
            tempDvs <<= 1;
            multiple <<= 1;
        }
        
        lDvd -= tempDvs;
        result += multiple;
    }
    
    // 4. Apply the sign
    long long finalResult = isNegative ? -result : result;
    
    // 5. Shrink to INT_MIN/MAX if outside the int bounds
    if (finalResult > INT_MAX) return INT_MAX;
    if (finalResult < INT_MIN) return INT_MIN;
    
    return (int)finalResult;
}

//we will always check the range between the current row and the required one, and then swap adjacent rows until we complete whats required

int zerosRequired(int** grid,int gridSize, int* gridColSize,int targetRowNum)
{
    int z;
    int currRow = 0;
    for(z = *gridColSize ; currRow <= targetRowNum; z--) currRow++;
    return z;
}


int zerosInRow(int** grid,int gridSize, int* gridColSize,int targetRowNum, int currRow)
{
    int zeros = 0;
    for(int col = targetRowNum + 1; col < *gridColSize; col ++) if(grid[currRow][col] == 0) zeros++;
    return zeros;
}

int getClosestValidRow(int** grid,int gridSize,int* gridColSize,int targetRowNum) {

    int zRequired = zerosRequired(grid,gridSize,gridColSize,targetRowNum);
    //calc how many zeros are required
    for(int r = targetRowNum + 1; r < gridSize; r++) if(zerosInRow(grid,gridSize,gridColSize,targetRowNum,r) >= zRequired) return r;
    
    //if no row has been found:
    return -1;//error code
}







int minSwaps(int** grid, int gridSize, int* gridColSize) {
    int timesSwapped = 0;
    for(int row = 0; row < gridSize - 1; row++)
    {
        //in case the row is already correct:
        if(zerosRequired(grid,gridSize,gridColSize,row) <= zerosInRow(grid,gridSize,gridColSize,row,row)) continue;
        int target = getClosestValidRow(grid,gridSize,gridColSize,row);
        if(target == -1) return target;//return -1 because a row remains unfixed
        for(int currRow = target; currRow > row; currRow--)
        {
            //the actual swap 
            int* temp = grid[currRow];
            grid[currRow] = grid[currRow - 1];
            grid[currRow - 1] = temp; 

            timesSwapped++;
        }

    }
    return timesSwapped++;
}





int main()
{
    //checkDistances funk:
    // int* distance = calloc(26,sizeof(int)); 
    // distance[0] = 51;
    // char* s = "abbccddeeffgghhiijjkkllmmnnooppqqrrssttuuvvwwxxyyzza";
    // bool returnVal = checkDistances(s,distance,26);
    // printf("result : %d",returnVal);

    //longestPrefix funk:

    // int strsSize = 2;
    // char** strs = malloc((size_t)(strsSize * sizeof(char*)));

    // strs[0] = strdup("cir");
    // strs[1] = strdup("car");

    // char* result = longestCommonPrefix(strs,strsSize);
    // int resultLen = strlen((result));
    // for(int i = 0; i < strsSize; i++) printf("%c ",result[i]);
    // for(int i = 0; i < strsSize; i++) free(strs[i]);
    // free(strs);
    // free(result);


    //devide funk:
    // printf("%d",divide(-6,3));


    //minswaps funk:
    int n = 3;
    int raw_data[3][3] = {
        {0, 0, 1},
        {1, 1, 0},
        {1, 0, 0}
    };

    // Prepare the int** grid structure
    int** grid = (int**)malloc(n * sizeof(int*));
    int* gridColSize = (int*)malloc(n * sizeof(int));

    for (int i = 0; i < n; i++) {
        grid[i] = (int*)malloc(n * sizeof(int));
        gridColSize[i] = n;
        for (int j = 0; j < n; j++) {
            grid[i][j] = raw_data[i][j];
        }
    }

    // Call the function
    int result = minSwaps(grid, n, gridColSize);

    // Output result
    if (result != -1) {
        printf("Minimum swaps required: %d\n", result);
    } else {
        printf("Impossible to arrange grid.\n");
    }

    // Clean up memory
    for (int i = 0; i < n; i++) free(grid[i]);
    free(grid);
    free(gridColSize);

    return 0;
}