/*
Name: Molly Johnson
ONID: johnsmol
CS 344 Winter 2019
Due: 3/3/19
All information used to create this code is adapted from the OSU CS 344 Winter 2019
lectures and assignment instructions/hints unless otherwise specifically indicated.
Note: Also adapted from my own work from 11/14/18 (took the class in the Fall 2018 term but
am retaking this term for a better grade).

fflush(stdout) used after every printf() in this assignment at the advice of the instructor
in the assignment instructions.
*/

//added #define _GNU_SOURCE before #include <stdio.h> to prevent "implicit function declaration" 
//warnings with getline. Adapted from:
//https://stackoverflow.com/questions/8480929/scratchbox2-returns-implicit-declaration-of-function-getline-among-other-weir
#define _GNU_SOURCE

//include all header files
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>

//constant macro definitions
//max values for chars and args were determined by the CS344 assignment3 instructions
//(max chars is actually 2048 but i added one additional char for the null terminator)
#define MAX_CHARS 2049
#define MAX_ARGS 512
#define EXIT "exit"
#define CD "cd"
#define STATUS "status"
#define FALSE 0
#define TRUE 1
#define INVALID "INVALID"

//global variables
/*
//flag for if background is possible (if SIGSTP command given, should ignore "&" and
//just run it as a foreground command)
int backgroundPossibleGlobal = TRUE;

//exit status for the program. set to 0 to start w/ by default, can be changed if program
//encounters errors and needs to exit w/ a non-zero status
int exitStatusGlobal = 0;
*/

//function declarations

/*
NAME

SYNOPSIS

DESCRIPTION

*/
void GetInputString(char *userInputString){
	char *buffer;
	size_t bufsize = MAX_CHARS;
	size_t characters;
	buffer = (char *)malloc(bufsize * sizeof(char));
	if(buffer == NULL){
		printf("GETLINE BUFFER ERROR, UNABLE TO ALLOCATE\n");
		fflush(stdout);
		exit(1);
	}
	characters = getline(&buffer, &bufsize, stdin);
	buffer[strcspn(buffer, "\n")] = '\0';

	strcpy(userInputString, buffer);
	free(buffer);
	buffer = NULL;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int main(){
	//create string of max chars allowed for user input and memset to null terminators
	char userInput[MAX_CHARS];
	memset(userInput, '\0', sizeof(userInput));

	//get user input as long as the user hasn't entered "exit"
	do
	{
		printf(": ");

		GetInputString(userInput);

		printf("%s\n", userInput);
	}while(strcmp(userInput, "exit") != 0);
	//}while(IsExit(userInput) == FALSE);
		
	return 0;
}
