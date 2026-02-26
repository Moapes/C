#include <stdio.h> //include the basic things like printing, inputting etc..
#include <stdbool.h> //include the basics of boolean operations
#include <stddef.h> //header of size_t stuff
#include <stdlib.h> //for dynamic memory allocation

int main(){
    int myNumbers[5] = {10, 20, 30, 40, 50};
    int *start = &myNumbers[1]; //by adding the '*' we declare that the variable is a pointer to an integer instead of an actual integer
    int *end = &myNumbers[4];
    printf("%ld\n", end - start); //equals to 3
    //You can subtract two pointers that point to elements in the same array to find out how many elements are between them:

    int *pi = myNumbers; //int pointer
    
    printf("%p\n",(void*)pi); 
    printf("%p\n", (void*)(pi + 1)); //moves by sizeof(int) --> 4 bytes
    //(void*) is a pointer type that has no specific data type, and is just 
    //a raw pointer to memory with no value of a specific type tied to it
    //so u cannot use it for things involving the value


    //pointer to pointer phenomenon
    int myNum = 10;       // normal variable
    int *ptr = &myNum;    // pointer to int
    int **pptr = &ptr;    // pointer to pointer

    printf("myNum = %d\n", myNum);
    printf("*ptr = %d\n", *ptr);
    printf("**pptr = %d\n", **pptr);



    //files.. yesss filess
    FILE *fptr; //create a pointer to the file
    //fptr = fopen(filename, mode), open the file by directory
    /*
    mode indicates what will we do, read it, edit it,
    etc.. so we can get the appropriate access to it
    w - write to a file
    a - append new data to a file
    r - read from a file*/
    /*
    remember that to write the whole directory down
    you are supposed to write '\', which is a special
    character so you gotta add another '\' on top of it
    like this:*/
    fptr = fopen("C:\\directoryname\\filename.txt", "w");
    fprintf(fptr,"some text"); //write to a file/ append to it
    //be cautious as if you don't "a" - append data to a file the data will get overitten

    char myString[100];
    fgets(myString, 100, fptr);//read from a file, like stdin but instead of reading off of user input we read off of the file directly
    fclose(fptr); //closes the file


    //good summary of structs
    /*
    // Define a struct
    struct Car {
    char brand[30];
    int year;
    };

    int main() {
    struct Car car = {"Toyota", 2020};

    // Declare a pointer to the struct
    struct Car *ptr = &car;

    // Access members using the -> operator
    printf("Brand: %s\n", ptr->brand);
    printf("Year: %d\n", ptr->year);

    return 0;
    }*/


    /*unions are essentially the same,
    but they are more memory efficient since
    they only hold the last union/struct member
    in their memory slot instead of all the members
    example:
    union myUnion {
        int myNum;
        char myLetter;
        char myString[30];
    };

    int main() {
    union myUnion u1;

    u1.myNum = 1000;

    // Since this is the last value written to the union, myNum no longer holds 1000 - its value is now invalid
    u1.myLetter = 'A';

    printf("myNum: %d\n", u1.myNum); // This value is no longer reliable
    printf("myLetter: %c\n", u1.myLetter); // Prints 'A'

    return 0;
    }*/

// :


    // typedef float Temperature;

    // int main() {
    // Temperature today = 25.5;
    // Temperature tomorrow = 18.6;

    // printf("Today: %.1f C\n", today);
    // printf("Tomorrow: %.1f C\n", tomorrow);

    // return 0;
    // }




    // typedef with struct
    // typedef can be useful with struct, because it lets you avoid writing struct every time:

    // Example
    // #include <stdio.h>

    // // Without typedef:
    // struct Car {
    //   char brand[30];
    //   int year;
    // };

    // // With typedef:
    // typedef struct {
    //   char brand[30];
    //   int year;
    // } Car;

    // int main() {
    //   struct Car car1 = {"BMW", 1999}; // needs "struct"
    //   Car car2 = {"Ford", 1969}; // shorter with typedef

    //   printf("%s %d\n", car1.brand, car1.year);
    //   printf("%s %d\n", car2.brand, car2.year);
    //   return 0;
    // }   
    return 0;
}
