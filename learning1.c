#include <stdio.h> //include the basic things like printing, inputting etc..
#include <stdbool.h> //include the basics of boolean operations
#include <stddef.h> //header of size_t stuff

int main() {

    printf("%.2f <-- this is a float!\n",2.33232);
    printf("%d <-- this is an integer!\n",5);
    printf("%.2lf <-- this is a double!\n",5.44);
    printf("%c <-- this is a character!\n",'d');
    printf("%s <-- this is a string!\n","hello");

    //convert int to a float
    float sum = (float) 5 / 2; 


    //constants
    const int myUntouchableNum = 15;
    //myNum = 10; //error


    //math operations + bitwise operations

    int x = 10;     // =   basic assignment

    x += 5;         // +=  x = x + 5  → 15
    x -= 3;         // -=  x = x - 3  → 12
    x *= 2;         // *=  x = x * 2  → 24
    x /= 4;         // /=  x = x / 4  → 6
    x %= 4;         // %=  x = x % 4  → 2 (remainder)

    x &= 3;         // &=  bitwise AND
                    // 2 = 10
                    // 3 = 11
                    // --------
                    //     10 → 2

    x |= 4;         // |=  bitwise OR
                    // 2 = 010
                    // 4 = 100
                    // --------
                    //     110 → 6

    x ^= 1;         // ^=  bitwise XOR (flip bit)
                    // 6 = 110
                    // 1 = 001
                    // --------
                    //     111 → 7

    x <<= 1;        // <<= left shift (multiply by 2)
                    // 7 << 1 → 14

    x >>= 2;        // >>= right shift (divide by 4)
                    // 14 >> 2 → 3

    printf("Final x = %d\n", x);


    int myNumbers[] = {10, 25, 50, 75, 100};
    //the %zu return data type is meant for
    //the size_t data type
    //this is a data type for measurments of
    //sizes by bytes
    printf("%zu", sizeof(myNumbers));  // Prints 20 
    /*
    it prints 20 instead of 5 since sizeof is based
    of size by bytes and not by amount of variables
    integer - 4 bytes so 5 integers is 4 * 5 bytes
    or 20 bytes in short
    */
   //here is an example for a size_ t variable
   size_t size_of_int = sizeof(int); // equals 4 for 4 bytes

   //strlen is good for measuring string length
   //because unlike sizeof, strlen
   //doesn't include the \0 character when counting
   //aka the null terminator
   size_t len = strlen("hello");



   //in c, strings are arrays of chars:
   char hello[] = "hello";
   printf("%c",hello[1]); // prints 'e'
   printf("%s",hello); //output the whole string


   char str1[20] = "hello ";
   char str2[] = "world!";

   strcat(str1,str2); //combine two strings onto the first one
   strcpy(str2,str1); //copy one string to another
   strcmp(str1,str2); //compare two strings 
   //strcmp will result:
   /*
   return 0 (%d) if they are equal
   return otherwise if they are not equal
   */


   //user input
   int myNum;
   printf("Type a number: \n");
   //save user input
   scanf("%d", &myNum);
    //the '&' bascially means
    //to type the user input onto
    //the actual memory address of the myNum var
    //instead of a copy of the variable
    //if you won't do this the input won't be saved
    //when taking string input, the reference operator
    // aka '&' does not have to be used and also the string length must be specified beforehand

    /*
    but scanf in strings has limitations
    since it does not count space, tabs
    or anything like that as part of the 
    string and as a terminatring character
    so for strings its better to use
    fgets() that way we can input more than
    a single word:*/
    char fullName[30];
    fgets(fullName, sizeof(fullName), stdin);


    int myAge = 43;
    printf("%p", &myAge); //will display the pointer to the memory address in hexedecimal
    //also the var name of an array 
    //represents the first memory address of the first element of the array
    //we can advance it - myArray + 1
    //to get the value of it
    //we use a deferenciator '*' like this:
    //*(myArray) --> gets value of the memory address
    return 0;

}