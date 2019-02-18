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
//(max chars is actually 2048 but i added one additional char for the null terminator).
//(max args is actually 2048 but i added one additional arg for the NULL arg req'd to null-terminate
//an array sent to execvp() so execvp() knows when the end of the array has been reached).
#define MAX_CHARS 2049
#define MAX_ARGS 513
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
	//getline use adapted from my own work in OSU CS 344 Winter 2019 Assignment 2
	char *buffer;
	size_t bufsize = MAX_CHARS;
	size_t characters;
	buffer = (char *)malloc(bufsize * sizeof(char));
	if(buffer == NULL){
		printf("GETLINE BUFFER ERROR, UNABLE TO ALLOCATE\n");
		fflush(stdout); exit(1);
	}
	while(1){
		characters = getline(&buffer, &bufsize, stdin);
		if(characters == -1){
			clearerr(stdin);
		}
		else{
			break;
		}
	}
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
int StringMatch(char *string1, char *string2){
	if(strcmp(string1, string2) == 0){
		return TRUE;
	}
	return FALSE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int GetArgs(char **parsedInput, char *userInputString, char *inputFileIn, char *outputFileIn, int *isBackgroundBool){
	int inputCount = 0;
	int isOutFile = FALSE;
	int isInFile = FALSE;
	int isBackground = FALSE;
	char *space = " ";
	char *token;

	token = strtok(userInputString, space);
	parsedInput[inputCount] = malloc((MAX_CHARS) * sizeof(char));

	if(parsedInput[inputCount] == NULL){
		printf("USER INPUT MALLOC ERROR\n");
		fflush(stdout); exit(1);
	}

	strcpy(parsedInput[inputCount], token);
	inputCount++;

	while(token != NULL){
		token = strtok(NULL, space);
		if(token != NULL){
			if(StringMatch(token, "<") == TRUE){
				isInFile = TRUE;		
			}
			else if(StringMatch(token, ">") == TRUE){
				isOutFile = TRUE;	
			}
			else{
				if(isInFile == TRUE){
					strcpy(inputFileIn, token);
					//CommandOrFileExpand(inputFileIn);
					isInFile = FALSE;
				}
				else if(isOutFile == TRUE){
					strcpy(outputFileIn, token);
					//CommandOrFileExpand(outputFileIn);
					isOutFile = FALSE;
				}
				else{
					parsedInput[inputCount] = malloc((MAX_CHARS) * sizeof(char));

					if(parsedInput[inputCount] == NULL){
						printf("USER INPUT MALLOC ERROR\n");
						fflush(stdout); exit(1);
					}

					strcpy(parsedInput[inputCount], token);

					inputCount++;
				}
				
			}
		}
	}
	return inputCount;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int main(){
	
	//get user input as long as the user hasn't entered "exit"
	char command[MAX_CHARS];
	do{
		char inputFile[MAX_CHARS];
		memset(inputFile, '\0', sizeof(inputFile));
		char outputFile[MAX_CHARS];
		memset(outputFile, '\0', sizeof(outputFile));
		int isBackground = FALSE;

		char userInputStr[MAX_CHARS];
		memset(userInputStr, '\0', sizeof(userInputStr));

		char **parsedUserInput= malloc((MAX_ARGS) * sizeof(char*));
		if(parsedUserInput == NULL){
			printf("USER INPUT MALLOC ERROR\n");
			fflush(stdout); exit(1);
		}

		printf(": ");
		fflush(stdout);

		GetInputString(userInputStr);

		int numInputs = GetArgs(parsedUserInput, userInputStr, inputFile, outputFile, &isBackground);

		memset(command, '\0', sizeof(command));
		strcpy(command, parsedUserInput[0]);

		printf("command: %s\n", parsedUserInput[0]);
		for(int k = 1; k < numInputs; k++){
			printf("arg %d: %s\n", k, parsedUserInput[k]);
		}
		printf("input file: %s\n", inputFile);
		printf("output file %s\n", outputFile);

		for(int i = 0; i < numInputs; i++){
			free(parsedUserInput[i]);
			parsedUserInput[i] = NULL;
		}

		free(parsedUserInput);
		parsedUserInput= NULL;

	}while(StringMatch(command, EXIT) == FALSE);
	//}while(IsExit(userInput) == FALSE);
		
			
	return 0;
}
