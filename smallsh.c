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
#define NO_ACTION "NO_ACTION"

//global variables
//flag for if background is possible (if SIGSTP command given, should ignore "&" and
//just run it as a foreground command)
int backgroundPossibleGlobal = TRUE;

//exit status for the program. set to 0 to start w/ by default, can be changed if program
//encounters errors and needs to exit w/ a non-zero status
int exitStatusGlobal = 0;

//function declarations
int StringMatch(char *string1, char *string2);
void GetInputString(char *userInputString);
char *GetPID();
char *ReplaceString(char *str, char *orig, char *rep);
void VariableExpand(char *varIn);
int GetArgs(char **parsedInput, char *userInputString, char *inputFileIn, char *outputFileIn, int *isBackgroundBool);
int IsBlank(char *userInputIn);
int IsComment(char *userInputIn);
int IsNewline(char *userInputIn);
int IsExit(char *userInputIn);
int IsStatus(char *userInputIn);
int IsChangeDir(char *userInputIn);
int IsNoAction(char *userInputIn);
void ChangeDirBuiltInNoArgs();
void ChangeDirBuiltInOneArg(char *directoryArg);

/*
NAME

SYNOPSIS

DESCRIPTION

*/
void ChangeDirBuiltInNoArgs(){
	char *homeDir = getenv("HOME");
	chdir(homeDir);
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
void ChangeDirBuiltInOneArg(char *directoryArg){
	if(chdir(directoryArg) != 0){
		printf("chdir() to your specified directory has failed, no such directory there.\n");
		fflush(stdout);
	}
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int IsBlank(char *userInputIn){
	int spaceCount = 0;

	for(int j = 0; j < strlen(userInputIn); j++){
		if(userInputIn[j] == ' '){
			spaceCount++;
		}
	}

	if(spaceCount == (strlen(userInputIn) - 1)){
		return TRUE;
	}
	return FALSE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int IsComment(char *userInputIn){
	if(userInputIn[0] == '#'){
		return TRUE;
	}
	return FALSE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int IsNewline(char *userInputIn){
	if(StringMatch(userInputIn, "\n") == TRUE){
		return TRUE;
	}
	return FALSE;
}

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

	if((IsBlank(buffer) == FALSE) && (IsComment(buffer) == FALSE) && (IsNewline(buffer) == FALSE)){
		buffer[strcspn(buffer, "\n")] = '\0';
		strcpy(userInputString, buffer);
	}
	else{
		
		strcpy(userInputString, NO_ACTION);
	}
	
	free(buffer);
	buffer = NULL;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
char *GetPID(){
	//static so will remain after function exits
	static char returnStringPID[] = "";
	
	//get parent process pid int
	int pid = getpid();	

	//convert int pid into string pid
	int length = snprintf(NULL, 0, "%d", pid);
	fflush(stdout);
	char *stringPID = malloc(length + 1);
	if(stringPID == NULL){
		printf("ERROR, NOT ALLOCATED\n");
		fflush(stdout); exit(1);
	}
	snprintf(stringPID, length + 1, "%d", pid);
	fflush(stdout);
	char *copyStringPID = stringPID;
	strcpy(returnStringPID, copyStringPID);

	//free memory
	free(stringPID);
	stringPID = NULL;

	//return string version of the int pid
	return returnStringPID;

}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
char *ReplaceString(char *str, char *orig, char *rep){
	char *result;
	int i = 0;
	int cnt = 0;

	//save lengths of the replacement substring (pid) and the original substring ("$$")
	int newWlen = strlen(rep);
	int oldWlen = strlen(orig);

	//go through each char in the original long string, to check for occurrences of the original substring ("$$")
	for(i = 0; str[i] != '\0'; i++){
		if (strstr(&str[i], orig) == &str[i]){
			cnt++;
			i += oldWlen - 1;
		}
	}

	result = (char *)malloc(i + cnt *(newWlen - oldWlen) + 1);
	i = 0;

	//replace each occurrence of the orig substring("$$") with the new subtsring (pid)
	while (*str){
		if(strstr(str, orig) == str){
			strcpy(&result[i], rep);
			i += newWlen;
			str += oldWlen;
		}
		else{
			result[i++] = *str++;
		}
	}

	//set result to a new string to be returned so result memory can be freed
	result[i] = '\0';
	static char returnStr[MAX_CHARS + 1];
	memset(returnStr, '\0', sizeof(returnStr));
	strcpy(returnStr, result);

	//free memory
	free(result);
	result = NULL;

	//return newly expanded string
	return returnStr;

}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
void VariableExpand(char *varIn){
	char *str;
	char *orig;
	char *rep;
	str = (char*)malloc((MAX_CHARS) * sizeof(char));
	orig = (char*)malloc((MAX_CHARS) * sizeof(char));
	rep = (char*)malloc((MAX_CHARS) * sizeof(char));

	char *pid = GetPID();
	strcpy(str, varIn);
	strcpy(orig, "$$");
	strcpy(rep, pid);

	char *newStr = ReplaceString(str, orig, rep);

	strcpy(varIn, newStr);

	free(str);
	str = NULL;
	free(orig);
	orig = NULL;
	free(rep);
	rep = NULL;
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
	if(strstr(parsedInput[inputCount], "$$") != NULL){
		VariableExpand(parsedInput[inputCount]);
	}
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

					if(strstr(inputFileIn, "$$") != NULL){
						VariableExpand(inputFileIn);
					}
					isInFile = FALSE;
				}
				else if(isOutFile == TRUE){
					strcpy(outputFileIn, token);

					if(strstr(outputFileIn, "$$") != NULL){
						VariableExpand(outputFileIn);
					}

					isOutFile = FALSE;
				}
				else{
					parsedInput[inputCount] = malloc((MAX_CHARS) * sizeof(char));

					if(parsedInput[inputCount] == NULL){
						printf("USER INPUT MALLOC ERROR\n");
						fflush(stdout); exit(1);
					}

					strcpy(parsedInput[inputCount], token);

					if(strstr(parsedInput[inputCount], "$$") != NULL){
						VariableExpand(parsedInput[inputCount]);
					}

					inputCount++;
				}
				
			}
		}
	}

	if(inputCount > 1){
		if(StringMatch(parsedInput[inputCount-1], "&") == TRUE){
			*isBackgroundBool = TRUE;
			free(parsedInput[inputCount - 1]);
			parsedInput[inputCount - 1] = NULL;
			inputCount--;
		}
	}

	return inputCount;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int IsExit(char *userInputIn){
	if(StringMatch(userInputIn, EXIT) == TRUE){
		return TRUE;
	}
	return FALSE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int IsStatus(char *userInputIn){
	if(StringMatch(userInputIn, STATUS) == TRUE){
		return TRUE;
	}
	return FALSE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int IsChangeDir(char *userInputIn){
	if(StringMatch(userInputIn, CD) == TRUE){
		return TRUE;
	}
	return FALSE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int IsNoAction(char *userInputIn){
	if(StringMatch(userInputIn, NO_ACTION) == TRUE){
		return TRUE;
	}
	return FALSE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int main(){
	
	//get user input as long as the user hasn't entered "exit"
	char command[MAX_CHARS];
	int forkCount = 0;
	do{
		char inputFile[MAX_CHARS];
		memset(inputFile, '\0', sizeof(inputFile));
		strcpy(inputFile, NO_ACTION);

		char outputFile[MAX_CHARS];
		memset(outputFile, '\0', sizeof(outputFile));
		strcpy(outputFile, NO_ACTION);

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
		

		if(IsExit(parsedUserInput[0]) == TRUE){
			printf("user entered exit\n"); fflush(stdout);
		}
		else if(IsStatus(parsedUserInput[0]) == TRUE){
			printf("user entered status\n"); fflush(stdout); 
		}
		else if(IsChangeDir(parsedUserInput[0]) == TRUE){
			if(numInputs == 1){
				printf("user entered change dir w no args\n"); fflush(stdout); 
				ChangeDirBuiltInNoArgs();
			}
			else if(numInputs > 1){
				printf("user entered change dir w >= 1 arg\n"); fflush(stdout); 
				printf("cd arg is: %s\n", parsedUserInput[1]); fflush(stdout); 
				ChangeDirBuiltInOneArg(parsedUserInput[1]);
			}
		}
		else if(IsNoAction(parsedUserInput[0]) == TRUE){
			printf("no action should be taken\n"); fflush(stdout); 
		}
		else{
			printf("user entered a non-built in\n"); fflush(stdout); 

			//make the array NULL-terminated for execvp() so it knows where the end
			//of the array is
			parsedUserInput[numInputs] = NULL;

			//if user indicated to run the process in the background AND background
			//mode is currently allowed
			if((isBackground == TRUE) && (backgroundPossibleGlobal == TRUE)){
				printf("user wants background mode & it's allowed\n"); fflush(stdout);
			}
			//if the user didn't indicate to run the process in the background or if
			//they did want to run the process in the background but background mode
			//isn't currently allowed
			else{
				printf("user wants foreground mode (or background and it's not allowed)\n"); fflush(stdout);
				
				pid_t spawnpid = -5;
				int childExitStatus = -5;

				if(forkCount < 50){
					spawnpid = fork();
					switch(spawnpid){
						case -1:
							perror("Hull Breach!"); exit(1); //error, no child process created
							break;
						case 0: //i am the child
							printf("i am the child!\n"); fflush(stdout);
							printf("child (%d): sleeping for 1 second\n", getpid()); fflush(stdout);
							sleep(1);
							printf("child (%d): converting into \'ls -a\'\n", getpid()); fflush(stdout);
							if(execvp(parsedUserInput[0], parsedUserInput) < 0){
								perror("error in execvp()"); exit(2);
							}
							break;
						default: //i am the parent
							printf("i am the parent!\n"); fflush(stdout);
							printf("parent %d: sleeping for 2 seconds\n", getpid()); fflush(stdout);
							sleep(2);
							printf("parent (%d): waiting for child(%d) to terminate\n", getpid(), spawnpid); fflush(stdout);
							pid_t actualPID = waitpid(spawnpid, &childExitStatus, 0);
							printf("parent (%d): child(%d) terminated, exiting!\n", getpid(), actualPID); fflush(stdout);
							break;
					}
				}
				else{ //fork bombed
					perror("FORK BOMB!"); exit(1);  
				}
			}
		}

		memset(command, '\0', sizeof(command));
		strcpy(command, parsedUserInput[0]);

		printf("command: %s\n", parsedUserInput[0]); fflush(stdout); 
		for(int k = 1; k < numInputs; k++){
			printf("arg %d: %s\n", k, parsedUserInput[k]); fflush(stdout); 
		}
		printf("input file: %s\n", inputFile); fflush(stdout); 
		printf("output file %s\n", outputFile); fflush(stdout); 
		printf("background status is: %d\n", isBackground); fflush(stdout); 

		for(int i = 0; i < numInputs; i++){
			free(parsedUserInput[i]);
			parsedUserInput[i] = NULL;
		}
		free(parsedUserInput);
		parsedUserInput= NULL;

	}while(IsExit(command) == FALSE);
		
	return 0;
}
