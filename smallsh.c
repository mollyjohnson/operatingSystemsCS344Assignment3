/*
Name: Molly Johnson
ONID: johnsmol
CS 344 Winter 2019
Due: 3/3/19
All information used to create this code is adapted from the OSU CS 344 Winter 2019
lectures and assignment instructions/hints unless otherwise specifically indicated.
Note: Also adapted from my own work from 11/14/18 (took the class in the Fall 2018 term but
am retaking this term for a better grade).

note: fflush(stdout) used after every printf() in this assignment at the advice of the instructor
in the assignment instructions to make sure the output buffers get flushed every time I try to print.
*/

//added #define _GNU_SOURCE before #include <stdio.h> to prevent "implicit function declaration" 
//warnings with getline. Adapted from:
//https://stackoverflow.com/questions/8480929/scratchbox2-returns-implicit-declaration-of-function-getline-among-other-weir
#define _GNU_SOURCE

//include all header files:
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

//constant macro definitions:
//max values for chars and args were determined by the CS344 assignment3 instructions.
//max chars is actually 2048 but i added one additional char for the null terminator.
//max args is actually 512  but i added one additional arg for the NULL arg req'd to null-terminate
//an array sent to execvp() so execvp() knows when the end of the array has been reached.
//max forks an arbitrary value i created to prevent fork-bombing the server.
#define MAX_CHARS 2049
#define MAX_ARGS 513
#define MAX_FORKS 500
#define EXIT "exit"
#define CD "cd"
#define STATUS "status"
#define FALSE 0
#define TRUE 1
#define NO_ACTION "NO_ACTION"

//global variables:
//flag for if background is possible (if SIGTSTP command given, should ignore "&" and
//just run it as a foreground command). set to true by default so initially background
//mode is possible until the user enters ctrl-Z. used as a global variable since can't
//pass in additional parameters into the signal handler function. (and was the recommended
//method on the piazza discussion board).
int backgroundPossibleGlobal = TRUE;

//function declarations:
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
void Execute(char **parsedInput, int *childExitStatusIn);
void StatusBuiltIn(int childExitStatusIn);
void StatusBuiltInBackground(int backgroundChildExitStatusIn);
int RedirectInputFile(char *inputFileIn);
int RedirectOutputFile(char *outputFileIn);
void ExitBuiltIn(int foregroundProcessCountIn, int backgroundProcessCountIn, int backgroundPidArrayIn[], int foregroundPidArrayIn[], int childExitStatusIn);
int NeedsOutputRedirect(char *outputFileIn);
int NeedsInputRedirect(char *inputFileIn);
void CheckBackgroundProcesses(int *backgroundProcessCountIn, int backgroundPidArrayIn[], int *childExitStatusBckd);
void CatchSIGTSTP(int signo);

/*
NAME
catchsigtstp
SYNOPSIS
enters or exits foreground-only mode
DESCRIPTION
signal handler the parent process uses to catch/handle a SIGTSTP signal. receives the signal number (an int) as a parameter.
checks if the background mode is currently possible (i.e. if the boolean for background mode possible is true).
if it is, prints message saying foreground-only mode is being entered. prints message to the user. uses write instead of 
printf() to make sure signal handler is re-entrant. sets the background possible global boolean to false. if the background
mode possible boolean was already false, prints a message to the user that they're exiting the foreground-only mode. also
uses write instead of printf to prevent re-entrancy problems (printf isn't reentrant whereas write is). sets the background
possible global boolean to true. returns void.
*/
void CatchSIGTSTP(int signo){
	//check if the background mode is currently possible (i.e. if backgroundPossibleGlobal is true)
	if(backgroundPossibleGlobal == TRUE ){
		//if was true, print message to user that they're entering the foreground-only mode
		char *message = "\nEntering foreground-only mode (& is now ignored)\n: ";
		
		//use write() instead of printf() to print the message since write() is re-entrant.
		//lectures state strlen() isn't re-entrant, but students/instructors on the piazza 
		//discussion board found that recently it became a re-entrant function and could thus
		//be used for this assignment.
		write(STDOUT_FILENO, message, strlen(message)); fflush(stdout);

		//set the background possible global variable to false to enter foreground-only mode
		backgroundPossibleGlobal = FALSE;
	}
	else{ //backgroundPossibleGlobal is false
		//if was false, print message to the user that they're exiting the foreground-only mode
		char *message2 = "\nExiting foreground-only mode\n: ";

		//again use write instead of printf due to re-entrancy
		write(STDOUT_FILENO, message2, strlen(message2)); fflush(stdout);

		//set the background possible global variable to true to exit foreground-only mode
		backgroundPossibleGlobal = TRUE;
	}
}

/*
NAME
statusbuiltinbackground
SYNOPSIS
gives the status of the last background process
DESCRIPTION
takes the background status integer variable in as a parameter. uses WIFEXITED and WIFSIGNALED
macros to determine if the background process exited normally (and if so what was the exit
value) or if it was terminated by a signal (and if so what was the terminating signal). if
neither one is true, prints an error and exits w a non-zero exit value. returns void. note:
a process can have either an exit status or a terminating signal but not both!
*/
void StatusBuiltInBackground(int backgroundChildExitStatusIn){
	//additional information on how to obtain exit status and terminating signal information using WIFEXITED
	//and WEXITSTATUS and WIFSIGNALED and  WTERMSIG adapted from:
	//https://stackoverflow.com/questions/27306764/capturing-exit-status-code-of-child-process and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wifexited.html and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wtermsig.html and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wexitstatus.html
	
	//if WIFEXITED returns non-zero, means process terminated normally
	if(WIFEXITED(backgroundChildExitStatusIn) != 0){
		//if process terminated normally, use WEXITSTATUS to get the actual exit value
		//(WEXITSTATUS will return this number)
		int backgroundExitStatus = WEXITSTATUS(backgroundChildExitStatusIn);

		//print the exit value of the background process that terminated normally
		printf("background exit value %d\n", backgroundExitStatus); fflush(stdout);
	}
	//if WIFSIGNALED returns non-zero, means process was terminated by a signal.
	else if(WIFSIGNALED(backgroundChildExitStatusIn) != 0){
		//if process was terminated by a signal, use WTERMSIG to get the terminating signal number
		//(WTERMSIG will return this number)
		int backgroundTermSignal = WTERMSIG(backgroundChildExitStatusIn);

		//print the signal number of the background process that was terminated by the signal
		printf("terminated by signal %d\n", backgroundTermSignal); fflush(stdout);
	}
	//if neither one returns non-zero, is a major problem (one and only one of them should be returning non-zero)
	else{
		//if somehow neither WIFEXITED nor WIFSIGNALED returned non-zero, print the error and exit w a non-zero exit value.
		perror("neither WIFEXITED nor WIFSIGNALED returned a non-zero value, major error in your status checking!\n");
		exit(1);
	}
}

/*
NAME
checkbackgroundprocesses
SYNOPSIS
checks background processes to see if any have exited or been terminated
DESCRIPTION
takes as parameters the number of background process counts, the array of background PIDs that are still
processing (i.e. not exited or terminated by a signal), and the background child exit status. if the process
count is 1 or more, will go through each background process and use waitpid with the WNOHANG option to check if
the status for each pid is available (0 = process terminated) or if the status is not yet available. using the
WNOHANG option will allow the waitpid function to return immediately whether the process being checked has
terminated yet or not. if a background process has completed (terminated or exited), will print a message to
the user stating which background pid is done and then call the status built in function to get either the
exit value or terminating signal for that background process pid. returns void.
*/
void CheckBackgroundProcesses(int *backgroundProcessCountIn, int backgroundPidArrayIn[], int *childExitStatusBckd){
	//check that there are 1 or more background processes that haven't yet exited/been terminated
	if(*backgroundProcessCountIn > 0){
		//had trouble with waitpid expecting an int not a pointer to an int, so set a temporary
		//background status int to the background child exit status that was passed in
		int backgroundStatTemp = *childExitStatusBckd; 

		//loop through all background processes not yet exited/terminated
		for(int k = 0; k < *backgroundProcessCountIn; k++){
			//also had trouble passing in the array element to waitpid so set a temporary background spawnpid 
			//to the pid in the current array position.
			pid_t backgroundSpawnPid = backgroundPidArrayIn[k];

			//use waitpid to get the status of the current background process pid. pass in the temp background
			//spawnpid, temp background exit status(so it'll be changed to the correct status by the waitpid
			//function), and the WNOHANG option (which will cause waitpid to return immediately whether the process
			//has exited/been terminated or not, as we don't want to be waiting on background processes if they're
			//not done, we should be just returning control to the command line and checking periodically for
			//if they're done).
			pid_t actualBackgroundPID = waitpid(backgroundSpawnPid, &backgroundStatTemp, WNOHANG);

			//set the background status that was passed in to the value of the temp background status variable
			*childExitStatusBckd = backgroundStatTemp;

			//if waitpid returned  -1, an error occurred. print error message and exit w non-zero value.
			if(actualBackgroundPID == -1){
				perror("waitpid for background process error!\n"); exit(1);
			}

			//if waitpid returned a non-zero value (but not -1), means child process terminated. (return of 0 means
			//the status for the pid is not yet available. in that case, will do nothing).
			else if(actualBackgroundPID != 0){
				//if child process terminated, print message to the user that that background pid is done. 
				printf("background pid %d is done: ", actualBackgroundPID); fflush(stdout);

				//loop through all but the last background pid (starting from the position of the pid that was found
				//to be terminated/exited)
				for(int i = k; i < *backgroundProcessCountIn - 1; i++){
					//shift all background pids over to the left one from that point to prevent pids that have already
					//been terminated/exited from being in the array
					backgroundPidArrayIn[i] = backgroundPidArrayIn[i + 1];					
				}
				//use the status background built in function to get either the exit value or the terminating signal for
				//the background process that was just terminated or exited.
				StatusBuiltInBackground(backgroundStatTemp);

				//decrease the background process count by one (since shifted all remaining pids so that the array length
				//is one less than before and the exited/terminated pid has been removed)
				(*backgroundProcessCountIn) = ((*backgroundProcessCountIn) - 1);
			}
		}
	}
}

/*
NAME
redirectinputfile
SYNOPSIS
redirects stdin to a specified input file
DESCRIPTION
takes in the input file name as a parameter. opens the file. if not able to open,
sets exit status to a non-zero number and prints a message to the user.
if able to be opened, uses dup2 to redirect stdin(0) to the source file descriptor.
checks for dup2 error and if there's an error also sets the exit value to a non-zero
number. returns the child exit status integer (0 if successful, non-zero if are errors).
*/
int RedirectInputFile(char *inputFileIn){
	//create temporary child exit status variable, set to 0 by default
	int childExitStat = 0;

	//set source file descriptor to value returned by open, open the input file (read only)
	int sourceFD = open(inputFileIn, O_RDONLY);

	//if open returned -1, was an error opening the file. set exit status to 1 and print
	//error message to the user.
	if(sourceFD == -1){
		childExitStat = 1;
		printf("cannot open %s for input\n", inputFileIn); fflush(stdout);	
	}

	//set dupresult to the value returned by dup2. pass in the source file descriptor and the
	//stdin value (which is 0) to redirect input from stdin to the source file descriptor.
	int dupResult = dup2(sourceFD, 0);

	//if dup2 returns -1, the redirect had an error. set exit status to 1.
	if(dupResult == -1){
		childExitStat = 1;
	}

	//return the child exit status
	return childExitStat;
}

/*
NAME
redirectoutputfile
SYNOPSIS
redirects stdout to a specified output file
DESCRIPTION
takes in the output file name as a paremeter. opens the file (if created already) or creates
one if not already created. if not able to open/create, set exit status to non-zero and print
error message to the user. if able to be opened/created, uses dup2 to redirect stdout (1) to the
target file descriptor. checks dup2 for error and if there's an error and if there's an error also
sets the exit value to a non-zero number. returns the child exit status integer (0 if successful,
non-zero if there are errors).
*/
int RedirectOutputFile(char *outputFileIn){
	//create temporary child exit status variable, set to 0 by default
	int childExitStat = 0;

	//set target file descriptor to value returned by open, open or create or truncate(i.e.
	//overwrite previous file contents if was created)
	int targetFD = open(outputFileIn, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	//if open returned -1, was an error opening/creating/truncating the file. set exit
	//status to 1 and print error message to the user.
	if(targetFD == -1){
		printf("cannot open %s for output\n", outputFileIn); fflush(stdout);
		childExitStat = 1;
	}

	//set dupresult to the value returned by dup2. pass in the target file descriptor and the
	//stdout value (which is 1) to redirect output from stdout to the target file descriptor
	int dupResult = dup2(targetFD, 1);

	//if dup2 returns -1, the redirect had an error. set exit status to 1.
	if(dupResult == -1){
		childExitStat = 1;
	}	

	//return the child exit status
	return childExitStat;
}

/*
NAME
needsinputredirect
SYNOPSIS
checks if the current process needs its input redirected from stdin to a file or not
DESCRIPTION
takes in an input file name (or NO_ACTION if no input file was given by the user).
Calls the StringMatch function to see if the input file name matches NO_ACTION. if
it is, no input redirect is needed, function returns false. otherwise the user
entered an input file name and the process' input will need to be redirected to the
file so the function returns true.
*/
int NeedsInputRedirect(char *inputFileIn){
	//call StringMatch to check if the input file matches NO_ACTION (i.e. doesn't need
	//input redirect) or if it doesn't match that (and thus the user entered an actual
	//file name for redirection)
	if(StringMatch(inputFileIn, NO_ACTION) == TRUE){
		//if the input file name matches NO_ACTION, no redirect will be needed. return false.
		return FALSE; 
	}
	//if the input file name did not match NO_ACTION, redirection will be needed. return true.
	return TRUE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
int NeedsOutputRedirect(char *outputFileIn){
	if(StringMatch(outputFileIn, NO_ACTION) == TRUE){
		return FALSE;
	}
	return TRUE;
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
void ExitBuiltIn(int foregroundProcessCountIn, int backgroundProcessCountIn, int backgroundPidArrayIn[], int foregroundPidArrayIn[], int childExitStatusIn){
	//printf("hey you're in the EXIT function correctly\n"); fflush(stdout);	
	if(foregroundProcessCountIn > 0){
		for(int k = 0; k < foregroundProcessCountIn; k++){
			//printf("killing foreground process %d\n", k + 1); fflush(stdout);
			kill(foregroundPidArrayIn[k], SIGKILL);
		}
	}
	if(backgroundProcessCountIn > 0){
		for(int m = 0; m < backgroundProcessCountIn; m++){
			//printf("killing background process %d\n", m + 1); fflush(stdout);
			kill(backgroundPidArrayIn[m], SIGKILL);
		}
	}
	exit(childExitStatusIn);
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
void StatusBuiltIn(int childExitStatusIn){
	if(WIFEXITED(childExitStatusIn) != 0){
		//printf("the foreground process exited normally\n"); fflush(stdout);
		int exitStatus = WEXITSTATUS(childExitStatusIn);
		//printf("the exit status of the last foreground process was: %d\n", exitStatus); fflush(stdout);
		printf("exit value %d\n", exitStatus); fflush(stdout);
	}
	else if(WIFSIGNALED(childExitStatusIn) != 0){
		//printf("the foreground process was terminated by a signal\n"); fflush(stdout);
		int termSignal = WTERMSIG(childExitStatusIn);
		//printf("the terminating signal of the last foreground process was: %d\n", termSignal); fflush(stdout);
		printf("terminated by signal %d\n", termSignal); fflush(stdout);
	}
	else{
		perror("neither WIFEXITED nor WIFSIGNALED returned a non-zero value, major error in your status checking!\n");
		exit(1); 
	}
}

/*
NAME

SYNOPSIS

DESCRIPTION

*/
void Execute(char **parsedInput, int *childExitStatusIn){
	if(execvp(parsedInput[0], parsedInput) < 0){
		//perror("Failure with execvp()! Command could not be executed. Exit status will be set to 1.\n");
		printf("%s: no such file or directory\n", parsedInput[0]); fflush(stdout);
		*childExitStatusIn = 1;
		//printf("child exit exec error status is: %d\n", *childExitStatusIn); fflush(stdout); 
		exit(*childExitStatusIn);
	}
}

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
		printf("chdir() to your specified directory has failed, no such directory there.\n"); fflush(stdout);
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
		perror("GETLINE BUFFER ERROR, UNABLE TO ALLOCATE\n");
		exit(1);
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
	int length = snprintf(NULL, 0, "%d", pid); fflush(stdout);
	char *stringPID = malloc(length + 1);
	if(stringPID == NULL){
		perror("ERROR, NOT ALLOCATED\n");
		exit(1);
	}
	snprintf(stringPID, length + 1, "%d", pid); fflush(stdout);
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
	//int isBackground = FALSE;
	char *space = " ";
	char *token;

	token = strtok(userInputString, space);
	parsedInput[inputCount] = malloc((MAX_CHARS) * sizeof(char));

	if(parsedInput[inputCount] == NULL){
		perror("USER INPUT MALLOC ERROR\n");
		exit(1);
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
						perror("USER INPUT MALLOC ERROR\n");
						exit(1);
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
	struct sigaction SIGINT_action = {{0}};
	//SIGINT_action.sa_handler = CatchSIGINT;
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0; 
	sigaction(SIGINT, &SIGINT_action, NULL);

	struct sigaction SIGTSTP_action = {{0}};
	SIGTSTP_action.sa_handler = CatchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	//exit status for the foreground processes. set to 0 to start w/ by default (so if user calls
	//status before any foreground processes have been run it's zero). can be changed if a foreground
	//process encounters errors and needs to exit w/ a non-zero status
	int childExitStatus = 0;
	int backgroundExitStatus = 0;
	int foregroundProcessCount = 0;
	int backgroundProcessCount = 0;
	int forkCount = 0;

	//Instructor brewster on osu cs 344 slack message board stated this kind of fixed array to store
	//PIDs was acceptable for this assignment (his example was an array of 128 integer pids).
	int backgroundPidArray[1000];
	int foregroundPidArray[1000];

	//get user input as long as the user hasn't entered "exit"
	char command[MAX_CHARS];
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
			perror("USER INPUT MALLOC ERROR\n");
			exit(1);
		}

		printf(": "); fflush(stdout);

		GetInputString(userInputStr);

		int numInputs = GetArgs(parsedUserInput, userInputStr, inputFile, outputFile, &isBackground);

		if(IsExit(parsedUserInput[0]) == TRUE){
			//printf("user entered exit\n"); fflush(stdout);
			ExitBuiltIn(foregroundProcessCount, backgroundProcessCount, backgroundPidArray, foregroundPidArray, childExitStatus);
		}
		else if(IsStatus(parsedUserInput[0]) == TRUE){
			//printf("user entered status\n"); fflush(stdout); 
			StatusBuiltIn(childExitStatus);
		}
		else if(IsChangeDir(parsedUserInput[0]) == TRUE){
			if(numInputs == 1){
				//printf("user entered change dir w no args\n"); fflush(stdout); 
				ChangeDirBuiltInNoArgs();
			}
			else if(numInputs > 1){
				//printf("user entered change dir w >= 1 arg\n"); fflush(stdout); 
				//printf("cd arg is: %s\n", parsedUserInput[1]); fflush(stdout); 
				ChangeDirBuiltInOneArg(parsedUserInput[1]);
			}
		}
		else if(IsNoAction(parsedUserInput[0]) == TRUE){
			//printf("no action should be taken\n"); fflush(stdout); 
		}
		else{
			//printf("user entered a non-built in\n"); fflush(stdout); 

			//make the array NULL-terminated for execvp() so it knows where the end
			//of the array is
			parsedUserInput[numInputs] = NULL;

			//if user indicated to run the process in the background AND background
			//mode is currently allowed
			if((isBackground == TRUE) && (backgroundPossibleGlobal == TRUE)){
				//printf("user wants background mode & it's allowed\n"); fflush(stdout);
				pid_t backgroundspawnpid = -5;
				if(forkCount < MAX_FORKS){
					backgroundspawnpid = fork();
					switch(backgroundspawnpid){
						case -1:
							perror("Hull Breach!"); exit(1); //error, no child process created
							break;
						case 0: //i am the child
							SIGINT_action.sa_handler = SIG_IGN;
							sigaction(SIGINT, &SIGINT_action, NULL);
							SIGTSTP_action.sa_handler = SIG_IGN;
							sigaction(SIGTSTP, &SIGTSTP_action, NULL);
							//printf("i am the background child!\n"); fflush(stdout);

							//printf("background child (%d): sleeping for 1 second\n", getpid()); fflush(stdout);
							//sleep(2);
							//printf("background pid is %d\n", getpid()); fflush(stdout);
							if(NeedsInputRedirect(inputFile) == TRUE){
								//printf("background input file is gonna be redirected!\n"); fflush(stdout);
								if(RedirectInputFile(inputFile) == 1){
									childExitStatus = 1;
									exit(childExitStatus);
								}
							}
							else{ //redirect input to dev/null
								RedirectInputFile("/dev/null");
							}
							if(NeedsOutputRedirect(outputFile) == TRUE){
								//printf("background output file is gonna be redirected!\n"); fflush(stdout);
								if(RedirectOutputFile(outputFile) == 1){
									childExitStatus = 1;
									exit(childExitStatus);
								}
							}
							else{ //redirect output to dev/null
								RedirectOutputFile("/dev/null");
							}
							Execute(parsedUserInput, &childExitStatus);
							break;	
						default: //i am the parent
							//printf("i am the parent!\n"); fflush(stdout);
							//printf("parent %d: sleeping for 2 seconds\n", getpid()); fflush(stdout);
							//sleep(3);
							//printf("parent (%d): waiting for child (%d) to terminate\n", getpid(), backgroundspawnpid); fflush(stdout);
							//isBackgroundGlobal = TRUE;
							backgroundPidArray[backgroundProcessCount] = backgroundspawnpid;
							printf("background pid is %d\n", backgroundspawnpid); fflush(stdout);
							backgroundProcessCount++;
							//pid_t actualBackgroundPID = waitpid(backgroundspawnpid, &childExitStatus, WNOHANG);
							break;
					}
				}
				else{ //fork bombed
					perror("FORK BOMB! EXITING!"); exit(1);
				}
			}
			//if the user didn't indicate to run the process in the background or if
			//they did want to run the process in the background but background mode
			//isn't currently allowed
			else{
				//printf("user wants foreground mode (or background and it's not allowed)\n"); fflush(stdout);
				
				pid_t spawnpid = -5;

				if(forkCount < MAX_FORKS){
					spawnpid = fork();
					switch(spawnpid){
						case -1:
							perror("Hull Breach!"); exit(1); //error, no child process created
							break;
						case 0: //i am the child
							SIGINT_action.sa_handler = SIG_DFL;
							sigaction(SIGINT, &SIGINT_action, NULL);
							SIGTSTP_action.sa_handler = SIG_IGN;
							sigaction(SIGTSTP, &SIGTSTP_action, NULL);

							//printf("i am the child!\n"); fflush(stdout);
							//printf("child (%d): sleeping for 1 second\n", getpid()); fflush(stdout);
							//sleep(2);
							//printf("child (%d): converting into \'ls -a\'\n", getpid()); fflush(stdout);
							if(NeedsInputRedirect(inputFile) == TRUE){
								//printf("foreground input file is gonna be redirected!\n");fflush(stdout);
								if(RedirectInputFile(inputFile) == 1){
									childExitStatus = 1;
									exit(childExitStatus);
								}
								
							}
							if(NeedsOutputRedirect(outputFile) == TRUE){
								//printf("foreground output file is gonna be redirected!\n"); fflush(stdout);
								if(RedirectOutputFile(outputFile) == 1){
									childExitStatus = 1;
									exit(childExitStatus);
								}
							}
							Execute(parsedUserInput, &childExitStatus);
							break;
						default: //i am the parent
							//printf("i am the parent!\n"); fflush(stdout);
							//printf("parent %d: sleeping for 2 seconds\n", getpid()); fflush(stdout);
							//sleep(3);
							//printf("parent (%d): waiting for child(%d) to terminate\n", getpid(), spawnpid); fflush(stdout);
							//isForegroundGlobal = TRUE;
							foregroundPidArray[foregroundProcessCount] = spawnpid;
							foregroundProcessCount++;
							pid_t actualPID = waitpid(spawnpid, &childExitStatus, 0);
							if(actualPID == -1){
								perror("waitpid for background process error!\n"); exit(1);
							}	
							//printf("parent (%d): child(%d) terminated, exiting!\n", getpid(), actualPID); fflush(stdout);
							
							//SigintSignalStatusCheck(childExitStatus);
							StatusBuiltIn(childExitStatus);
							break;
					}
				}
				else{ //fork bombed
					perror("FORK BOMB! EXITING!"); exit(1);  
				}
			}
		}

		//printf("num foreground processes run: %d\n", foregroundProcessCount); fflush(stdout);
		//printf("num background processes run: %d\n", backgroundProcessCount); fflush(stdout);

		/*
		if(foregroundProcessCount > 0){
			for(int k = 0; k < foregroundProcessCount; k++){
				printf("foreground process %d pid: %d\n", k + 1, foregroundPidArray[k]); fflush(stdout);
			}
		}
		printf("THE BACKGROUND PROCESS COUNT IS: %d\n", backgroundProcessCount); fflush(stdout); 
		if(backgroundProcessCount > 0){
			for(int m = 0; m < backgroundProcessCount; m++){
				printf("background process %d pid: %d\n", m + 1, backgroundPidArray[m]); fflush(stdout);
			}
		}
		*/

		CheckBackgroundProcesses(&backgroundProcessCount, backgroundPidArray, &backgroundExitStatus);

		memset(command, '\0', sizeof(command));
		strcpy(command, parsedUserInput[0]);

		//printf("command: %s\n", parsedUserInput[0]); fflush(stdout); 
		/*
		for(int k = 1; k < numInputs; k++){
			printf("arg %d: %s\n", k, parsedUserInput[k]); fflush(stdout); 
		}
		*/
		//printf("input file: %s\n", inputFile); fflush(stdout); 
		//printf("output file %s\n", outputFile); fflush(stdout); 
		//printf("background status is: %d\n", isBackground); fflush(stdout); 

		for(int i = 0; i < numInputs; i++){
			free(parsedUserInput[i]);
			parsedUserInput[i] = NULL;
		}
		free(parsedUserInput);
		parsedUserInput= NULL;

	}while(IsExit(command) == FALSE);
	//printf("DOES THIS PRINT AFTER THE USER HITS EXIT?\n"); fflush(stdout);
		
	return 0;
}
