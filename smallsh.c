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
it does, no input redirect is needed, function returns false. otherwise the user
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
needsoutputredirect
SYNOPSIS
checks if the current process needs its output redirected from stdout to a file or not
DESCRIPTION
takes an output file name (or NO_ACTION if no output file was given by the user). calls
the StringMatch function to see if the output file name matches NO_ACTION. if it does,
no output redirection is needed, function returns false. otherwise the user entered an
output file name and the process' output will need to be redirected to the file so the
function returns true.
*/
int NeedsOutputRedirect(char *outputFileIn){
	//call StringMatch to check if the output file matches NO_ACTION (i.e. doesn't need
	//output redirect) or if it doesn't match that (and thus the user entered an actual
	//file name for redirection)
	if(StringMatch(outputFileIn, NO_ACTION) == TRUE){
		//if output file name matches NO_ACTION, no redirect will be needed. return false.
		return FALSE;
	}
	//if the output file name did not match NO_ACTION, redirection will be needed. return true.
	return TRUE;
}

/*
NAME
exitbuiltin
SYNOPSIS
cleans up any remaining processes running (kills them) and exits with the correct exit status
DESCRIPTION

*/
void ExitBuiltIn(int foregroundProcessCountIn, int backgroundProcessCountIn, int backgroundPidArrayIn[], int foregroundPidArrayIn[], int childExitStatusIn){
	//printf("hey you're in the EXIT function correctly\n"); fflush(stdout);	
	if(foregroundProcessCountIn > 0){
		for(int k = 0; k < foregroundProcessCountIn; k++){
			kill(foregroundPidArrayIn[k], SIGKILL);
		}
	}
	if(backgroundProcessCountIn > 0){
		for(int m = 0; m < backgroundProcessCountIn; m++){
			kill(backgroundPidArrayIn[m], SIGKILL);
		}
	}
	exit(childExitStatusIn);
}

/*
NAME
statusbuiltin
SYNOPSIS
gives the status of the last process
DESCRIPTION
takes the status integer of a process in as a parameter. uses WIFEXITED and WIFSIGNALED macros
to determine if the process exited normally (and if so what was the exit value) or if it was
terminated by a signal (and if so what was the terminating signal). if neither one is true,
prints an error and exits w a non-zero value. returns void. note: a process can have either
an exit status or a terminating signal but not both!
*/
void StatusBuiltIn(int childExitStatusIn){
	//additional information on how to obtain exit status and terminating signal information using WIFEXITED
	//and WEXITSTATUS and WIFSIGNALED and  WTERMSIG adapted from:
	//https://stackoverflow.com/questions/27306764/capturing-exit-status-code-of-child-process and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wifexited.html and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wtermsig.html and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wexitstatus.html

	//if WIFEXITED returns non-zero, means process terminated normally 
	if(WIFEXITED(childExitStatusIn) != 0){
		//if process terminated normally, use WEXITSTATUS to get the actual exit value
		//(WEXITSTATUS will return this number)
		int exitStatus = WEXITSTATUS(childExitStatusIn);

		//print the exit value of the process that terminated normally
		printf("exit value %d\n", exitStatus); fflush(stdout);
	}
	//if WIFSIGNALED returns non-zero, means process was terminated by a signal.
	else if(WIFSIGNALED(childExitStatusIn) != 0){
		//if process was terminated by a signal, use WTERMSIG to get the terminating signal number
		//(WTERMSIG will return this number)
		int termSignal = WTERMSIG(childExitStatusIn);

		//print the signal number of the process that was terminated by the signal
		printf("terminated by signal %d\n", termSignal); fflush(stdout);
	}
	//if neither one returns non-zero, is a major problem (one and only one of them should be returing non-zero)
	else{
		//if somehow neither WIFEXITED nor WIFSIGNALED returned non-zero, print the error and exit w/ a non-zero value
		perror("neither WIFEXITED nor WIFSIGNALED returned a non-zero value, major error in your status checking!\n");
		exit(1); 
	}
}

/*
NAME
execute
SYNOPSIS
calls execvp() on commands that aren't built-ins
DESCRIPTION
takes in an array of strings (a non-buil-in command and either no arguments or up to 512 arguments (as listed 
in assignment specs). calls execvp(), which is an exec() function variant that uses the path variable to look
for non-built-in commands and requires the path start (i.e. pointer to the array of string inputs) as the first argument, 
and the array itself as the second argument.  (Note: the array must be null-terminated in order for execvp() to know
when the end of the array has been reached!). if execvp() encounters an error, set the exit status to a non-zero
value and call exit with this value. returns void.
*/
void Execute(char **parsedInput, int *childExitStatusIn){
	//execvp() use also adapted from:
	////https://stackoverflow.com/questions/27541910/how-to-use-execvp and
	//http://www.csl.mtu.edu/cs4411.ck/www/NOTES/process/fork/exec.html and
	//http://www.cs.ecu.edu/karl/4630/sum01/example1.html and
	//http://man7.org/linux/man-pages/man3/exec.3.html
	
	//call execvp() with the parsed input array pointer as the path (first arg) and the parsed input array
	//itself as the second arg. parsed input is a NULL-terminated array. check if execvp returns < 0 (which
	//would indicate an execvp() error).
	if(execvp(parsedInput[0], parsedInput) < 0){
		//if execvp had an error, print an error message to the user
		printf("%s: no such file or directory\n", parsedInput[0]); fflush(stdout);

		//set the exit value to non-zero
		*childExitStatusIn = 1;

		//exit with the non-zero exit value
		exit(*childExitStatusIn);
	}
}

/*
NAME
changedirbuiltinnoargs
SYNOPSIS
changes the directory to home
DESCRIPTION
when the user doesn't enter any args and just enters "cd" (i.e. change directory), use getenv() to get
the home path, then chdir() to this home path. (didn't do error check on directory because isn't a user-et
directory, all users will have a home path). returns void.
*/
void ChangeDirBuiltInNoArgs(){
	//using getenv and home to find the home directory path adapted from:
	//https://stackoverflow.com/questions/31906192/how-to-use-environment-variable-in-a-c-program and
	//http://man7.org/linux/man-pages/man3/getenv.3.html and
	//https//www.tutorialspoint.com/c_standard_library/c_function_getenv.htm and
	//https://stackoverflow.com/questions/9493234/chdir-to-home-directory
	//chdir information adapted from:
	//https://linux.die.net/man/2/chdir

	//get home path using getenv()
	char *homeDir = getenv("HOME");

	//change directory using chdir() and the home path found by getenv()
	chdir(homeDir);
}

/*
NAME
changedirbuiltinonearg
SYNOPSIS
changes directory to a user-specified directory
DESCRIPTION
is called when the user enters "cd" and one arg (or more, but any args beyond the first will be ignored). first
arg is the relative or absolute file path the user is wanting to change directory to. will attempt to change to
this specified directory using chdir(). if chdir() returns non-zero (i.e. fails), print error message to the
user. returns void.
*/
void ChangeDirBuiltInOneArg(char *directoryArg){
	//attempt to change to the user-specified directory by calling chdir() with their first input arg. If
	//chdir() returns non-zero, print message to user that chdir() to their specified directory has failed.
	if(chdir(directoryArg) != 0){
		printf("chdir() to your specified directory has failed, no such directory there.\n"); fflush(stdout);
	}
}

/*
NAME
isblank
SYNOPSIS
checks if user entered a blank line
DESCRIPTION
takes in the user's input as a parameter. checks if all of the user's input are spaces (i.e. a "blank"
line). i it is a blank line, returns true. else returns false.
*/
int IsBlank(char *userInputIn){
	//create counter for space chars and initialize to 0
	int spaceCount = 0;

	//loop through each char of the user input string
	for(int j = 0; j < strlen(userInputIn); j++){
		//if the current char is a space, increment the space count
		if(userInputIn[j] == ' '){
			spaceCount++;
		}
	}

	//after looping through all of the characters in the user input string,
	//check if the number of spaces is equal to the string length of the user input (minus one
	//for the null-terminator).  else return false.
	if(spaceCount == (strlen(userInputIn) - 1)){
	//if these numbers are equal, the entire line was composed of just
	//spaces (i.e. blank). return true.
		return TRUE;
	}
	//else return false
	return FALSE;
}

/*
NAME
iscomment
SYNOPSIS
checks if the user entered a comment
DESCRIPTION
receives user input string as a parameter. checks the first char of the user's input
to see if it's a '#' char. if it is, return true (is a comment). else, return false.
*/
int IsComment(char *userInputIn){
	//check if first char of the user's input string is a '#' char
	if(userInputIn[0] == '#'){
		//if it is, return true
		return TRUE;
	}
	//else, return false
	return FALSE;
}

/*
NAME
isnewline
SYNOPSIS
checks if the user only hit enter (i.e. line is "blank" because the user only entered a newline char)
DESCRIPTION
receives user input string as a parameter. checks if the entire string is just one newline char. if
it is, return true (is a "blank" line composed of only a newline char). else return false.
*/
int IsNewline(char *userInputIn){
	//call StringMatch to check if the user input string is just a newline character.
	if(StringMatch(userInputIn, "\n") == TRUE){
		//if it is, return true
		return TRUE;
	}
	//else, return false
	return FALSE;
}

/*
NAME
getinputstring
SYNOPSIS
obtains user input as one long string from the user using getline
DESCRIPTION
receives userinputstring (blank) as a parameter. uses getline to get the user's input
as one long string. checks if getline returns -1 to account for getline problems that
can arise due to signals. checks if the user input is blank, a comment, or just a newline
character. if it's none of those things, newline char added by getline will be removed
and the getilne buffer will be copied into the user input string. if any of the input checks
(isblank, iscomment, isnewline) return true, the user input string will instead be set to
NO_ACTION (since these inputs should cause no action to be taken and the user just returned
back to the command line for new input). the buffer then gets freed. returns void.
*/
void GetInputString(char *userInputString){
	//getline use adapted from my own work in OSU CS 344 Winter 2019 Assignment 2

	//create buffer of size MAX_CHARS (2048 as given in the assignment instructions plus one for null terminator)
	char *buffer;
	size_t bufsize = MAX_CHARS;
	size_t characters;

	//malloc the buffer and check that malloc is successful
	buffer = (char *)malloc(bufsize * sizeof(char));

	//if buffer == NULL, malloc had an error. print error message and exit with value of 1.
	if(buffer == NULL){
		perror("GETLINE BUFFER ERROR, UNABLE TO ALLOCATE\n");
		exit(1);
	}

	//keep looping (by using while(1)) to get the line of user input. check for if getline returns -1
	//is used to make sure getline doesn't encounter any problems due to signals
	while(1){
		//call getline to get user input from stdin and put it in the buffer
		characters = getline(&buffer, &bufsize, stdin);

		//check if getline returned -1
		if(characters == -1){
			//if getline returned -1, use clearerr on stdin and let it loop back around
			clearerr(stdin);
		}
		else{
			//else if getline was successful (didn't return -1), go ahead and break out of loop
			break;
		}
	}

	//check if the user entered a blank line, a comment, or only a newline char
	if((IsBlank(buffer) == FALSE) && (IsComment(buffer) == FALSE) && (IsNewline(buffer) == FALSE)){
		//if user didn't enter any of these types of input, remove the newline char from the buffer,
		//replacing it with a null terminating character
		buffer[strcspn(buffer, "\n")] = '\0';

		//copy the buffer with the newline char removed into the userinput string variable
		strcpy(userInputString, buffer);
	}
	else{
		//else if the user did enter either a blank line, comment, or only a newline char, don't
		//do anything with the buffer. set the user input string to NO_ACTION (since those types
		//of inputs should result in no action taking place and the user being returned to
		//the command line and re-prompted).
		strcpy(userInputString, NO_ACTION);
	}
	
	//free the buffer that was malloc'd for getline and set to NULL
	free(buffer);
	buffer = NULL;
}

/*
NAME
getpid
SYNOPSIS
gets the int pid of the current process and transforms it into the same pid but in string form
DESCRIPTION
takes no parameters. gets the int pid of the current process using getpid(). converts this into
a string version of the pid using snprintf. returns the static string version of the process pid.
*/
char *GetPID(){

	//create static string to hold the string version of the int pid (make it static so the string 
	//will remain after function exits)
	static char returnStringPID[] = "";
	
	//get current process pid int
	int pid = getpid();	

	//using snprintf to convert an int into a string is adapted from my own work previously 
	//created for OSU CS 344 Winter term, assignment 2/13/19 

	//convert int pid into string pid.
	//get length of pid
	int length = snprintf(NULL, 0, "%d", pid); fflush(stdout);

	//create a temporary string variable for the string version of the PID and malloc it to length + 1 (for null term)
	char *stringPID = malloc(length + 1);

	//check that malloc was succcessful. if not successful (i.e. returns NULL), print error and exit w/ non-zero exit status
	if(stringPID == NULL){
		perror("ERROR, NOT ALLOCATED\n");
		exit(1);
	}

	//use snprintf to set the string variable to the string version of the int pid
	snprintf(stringPID, length + 1, "%d", pid); fflush(stdout);

	//create another temporary string variable. copy the string version of the pid into another temporary variable 
	//(so the original one that was malloc'd can be freed.  (when didn't use a malloc'd string had some errors when trying to use snprintf,
	//and when tried to copy the malloc'd version directly into the static string to be returned also got errors, but worked when using a
	//temporary variable).
	char *copyStringPID = stringPID;

	//copy the temporary string pid variable into the static string that will be returned
	strcpy(returnStringPID, copyStringPID);

	//free memory created for the original temporary string variable that was malloc'd
	free(stringPID);
	stringPID = NULL;

	//return static string version of the int pid
	return returnStringPID;
}

/*
NAME
replacestring
SYNOPSIS
replaces a specified substring in a string with another specified substring
DESCRIPTION
takes in three strings, one (str) which is the complete string, another (orig)
which is the substring that's to be replaces, and another (rep) that is the replacement
string meant to be inserted in the place of the original substring. returns the
new string that contains the replacement substring in place of the original substring.
*/
char *ReplaceString(char *str, char *orig, char *rep){
	//replace string method adapted from:
	//Adapted from: https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c and
	//https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/

	//create result string (will be a temporary string variable to hold the newly expanded string with the replacement substring)
	char *result;

	//create two counter variables and initialize to zero
	int i = 0;
	int cnt = 0;

	//save original length of the replacement substring (string version of the pid)
	int newWlen = strlen(rep);

	//save the original length of the original substring to be replaced ("$$")
	int oldWlen = strlen(orig);

	//loop through each char in the original long string for the entire length of the string (i.e. until null term is reached), 
	//checking for occurrences of the original substring ("$$")
	for(i = 0; str[i] != '\0'; i++){
		//strstr information adapted from:
		//https://www.tutorialspoint.com/c_standard_library/c_function_strstr.htm
		
		//strstr returns a pointer to the first occurence in the main string of any of
		//the sequence of characters specified in the substring to be searched for. returns
		//null if no sequence found. so in this case, strstr shoud return null if no instances
		//of $$ are found. Should return pointer to the first occurrence in the main string of the substring.
		if (strstr(&str[i], orig) == &str[i]){
			//if an occurrence of '$' was found, increment the substring count (cnt)
			cnt++;

			//set the i loop variable to add the old length of the substring ('$$') - 1.
			i += oldWlen - 1;
		}
	}

	//malloc the result string to the appropriate size using the i loop variable, cnt '$' counter
	//variable, and length of the replacement string (pid) minus the length of the original substring ("$$")
	result = (char *)malloc(i + cnt *(newWlen - oldWlen) + 1);

	//reset the i looping variable to zero.
	i = 0;

	//replace each occurrence of the orig substring("$$") with the new subtsring (pid)
	//loop through the entire string
	while (*str){
		//if strstr returns a pointer to the first occurrence of the "$$" substring (instead of null)
		if(strstr(str, orig) == str){
			//copy the replacement substring into the result string, starting at the pointer where
			//"$$" was first found.
			strcpy(&result[i], rep);

			//set i to add the new length to itself (i.e. the length of the new replacement
			//substring, the pid)
			i += newWlen;
			
			//set the string to add the old length to itself (i.e. the length of the original
			//substring, "$$"
			str += oldWlen;
		}
		//if strstr returns null, set the next char after the current one in result to the next
		//char in the complete original string (since will need the rest of the original string added to
		//the end of the newly inserted substring)
		else{
			result[i++] = *str++;
		}
	}

	//set result to a new string to be returned so result memory can be freed
	//set last char value in result to a null terminator
	result[i] = '\0';

	//create another static string of maxchars + 1 (for null term). make statis so it persists
	//after the function exits, and memset it to null terminators.
	static char returnStr[MAX_CHARS + 1];
	memset(returnStr, '\0', sizeof(returnStr));

	//use strcpy to copy the malloc'd result string (the new string with the substring replacement/
	//expansion) into the static string
	strcpy(returnStr, result);

	//free memory from the malloc'd temp string and set to NULL
	free(result);
	result = NULL;

	//return newly expanded static string, which should now contain the string version of the pid
	//in place of every instance of "$$"
	return returnStr;

}

/*
NAME
variableexpand
SYNOPSIS
performs variable expansion, replacing every "$$" with the pid
DESCRIPTION
takes in a variable string. creates an original long string, an original substring,
the new substring to be inserted into the main string to replace the original substring,
and the newly expanded string. will call the ReplaceString function to obtain the newly 
expanded string. will set the string passed in to this new string using strcpy. returns void.
*/
void VariableExpand(char *varIn){
	//create original long string
	char *str;

	//create  original substring ("$$")
	char *orig;

	//create new substring for expansion (pid)
	char *rep;

	//malloc these strings
	str = (char*)malloc((MAX_CHARS) * sizeof(char));
	orig = (char*)malloc((MAX_CHARS) * sizeof(char));
	rep = (char*)malloc((MAX_CHARS) * sizeof(char));

	//check that malloc was successful (i.e. none of them == NULL)
	if((str == NULL) || (orig == NULL) || (rep == NULL)){
		//if any were not malloc'd successfully, print error message and exit w non-zero value
		perror("VARIABLE EXPANSION STRING MALLOC ERROR\n");
		exit(1);
	}

	//call GetPID function to get string version of the pid
	char *pid = GetPID();

	//copy the variable passed in as a parameter to the original long string
	strcpy(str, varIn);

	//set the original substring to "$$" (per the assignment instructions)
	strcpy(orig, "$$");

	//set the replacement string to the string version of the pid returned by GetPID()
	strcpy(rep, pid);

	//create the newly expanded string and set it to the string returned by the call to
	//the ReplaceString function. pass in the main long string, original substring, and new
	//replacement substring to the ReplaceString function
	char *newStr = ReplaceString(str, orig, rep);

	//copy the newly expanded string to the variable passed into this function
	strcpy(varIn, newStr);

	//free and set to NULL all of the temporary string and substring variables that were malloc'd
	free(str);
	str = NULL;
	free(orig);
	orig = NULL;
	free(rep);
	rep = NULL;
}

/*
NAME
stringmatch
SYNOPSIS
checks if two strings match each other
DESCRIPTION
receives two strings as parameters. uses strcmp to compare the two strings. if they match, returns
true. else returns false. created this function because was getting tripped up at times with how
strcmp returns 0 if two strings match, while i have 0 set to false everywhere else in the program so
it seemed like should be returning non-zero. after making mistakes with it a few times decided to create
my own function to check strings.
*/
int StringMatch(char *string1, char *string2){
	//use strcmp to see if string one and two are equal. (if equal, strcmp will return 0. will return
	//non-zero if the two strings are not equal.
	if(strcmp(string1, string2) == 0){
		//if the two strings are a match, return true
		return TRUE;
	}
	//if the two strings are not a match, return false
	return FALSE;
}

/*
NAME
getargs
SYNOPSIS
parses the user input string into separate pieces
DESCRIPTION
takes the long user input string and parses it into arguments, input file (if there), output file (if there), 
and sets the background bool to either true or false (depending on if the user entered '&' as their last arg
or not). puts the parsed arguments into an array of strings (parsedInput). returns the number of inputs. 
*/
int GetArgs(char **parsedInput, char *userInputString, char *inputFileIn, char *outputFileIn, int *isBackgroundBool){
	//use of strtok to parse input with a given token (i.e. the "spaces" between input arguments for this assignment)
	//is adapted from my own work for OSU cs 344 assignment 2 winter 2019 2/13/19 and
	//http://www.cplusplus.com/reference/cstring/strtok/

	//initialize input count to 0
	int inputCount = 0;

	//initialize is output file bool to false
	int isOutFile = FALSE;

	//initialize is input file bool to false
	int isInFile = FALSE;

	//create "space" string and a string for the "token" to be used by strtok
	char *space = " ";
	char *token;

	//use strtok to split the string into tokens (each token is a piece of input) using
	//the space " " chars as delimiters between each argument to be parsed.
	token = strtok(userInputString, space);

	//malloc the first parsed input in the parsed input array of strings
	parsedInput[inputCount] = malloc((MAX_CHARS) * sizeof(char));

	//check that it malloc'd correctly. if not, print error and exit w non-zero value
	if(parsedInput[inputCount] == NULL){
		perror("USER INPUT MALLOC ERROR\n");
		exit(1);
	}

	//copy the input arg (token) into the current empty string in the array of strings
	strcpy(parsedInput[inputCount], token);

	//use strstr to check if the current input from the user needs variable expansion.
	//if strstr == NULL, doesn't need expansion. if returns not equal to NULL, call
	//the variable expand function to expand any instance of $$ in the input arg to
	//the pid
	if(strstr(parsedInput[inputCount], "$$") != NULL){
		VariableExpand(parsedInput[inputCount]);
	}

	//increment the input count
	inputCount++;

	//loop through entire user input string (i.e. while the token != NULL)
	while(token != NULL){
		//get the next arg using space " " as a delimiter
		token = strtok(NULL, space);

		//check that the next arg isn't NULL
		if(token != NULL){
			//if next arg isn't NULL, check if it's  "<" (which would mean the next arg is going to be an input file for redirection)
			if(StringMatch(token, "<") == TRUE){
				//if the current arg is a "<", set input file bool to true so next arg will go to input file string instead of arg array
				isInFile = TRUE;		
			}
			//if the current arg is ">", set the output file bool to true so next arg will go to output file string instead of arg array
			else if(StringMatch(token, ">") == TRUE){
				isOutFile = TRUE;	
			}
			//else if current arg isn't "<" or ">" and thus isn't an indication for upcoming redirection but rather just a regular argument
			else{
				//if the input file bool is true, this arg is an input file for redirection
				if(isInFile == TRUE){
					//copy this token arg into the input file string instead of the parsed args array
					strcpy(inputFileIn, token);

					//check if this input file requires variable expansion. if so, call VariableExpand
					if(strstr(inputFileIn, "$$") != NULL){
						VariableExpand(inputFileIn);
					}
					//reset input file bool to false
					isInFile = FALSE;
				}
				//if the output file bool is true, this arg is an output file for redirection
				else if(isOutFile == TRUE){
					//copy this token arg into the output file string instead of the parsed args array
					strcpy(outputFileIn, token);

					//check if this output file requires variable expansion. if so, call VariableExpand
					if(strstr(outputFileIn, "$$") != NULL){
						VariableExpand(outputFileIn);
					}

					//reset output file bool to false
					isOutFile = FALSE;
				}
				//else if the current arg is neither an input file or an output file
				else{
					//malloc the next arg in the parsed input array
					parsedInput[inputCount] = malloc((MAX_CHARS) * sizeof(char));

					//check that malloc was successful. if not successful (== NULL), print error message
					//and exit with a non-zero value.
					if(parsedInput[inputCount] == NULL){
						perror("USER INPUT MALLOC ERROR\n");
						exit(1);
					}

					//use strcpy to copy the current arg (i.e. "token) into the next string in the parsed input array of strings.
					strcpy(parsedInput[inputCount], token);

					//check if this arg needs variable expansion or not. if it does (i.e. != NULL), call VariableExpand
					if(strstr(parsedInput[inputCount], "$$") != NULL){
						VariableExpand(parsedInput[inputCount]);
					}

					//increment the input count
					inputCount++;
				}
				
			}
		}
	}

	//check if the number of inputs is greater than one (because the user can't enter nothing but a background signal, would be nothing to do and should
	//just return an error when attempted to be called by execvp()).
	if(inputCount > 1){
		//if there's more than one input, check if the last input matches "&" (i.e. the background indicator) using StringMatch.
		if(StringMatch(parsedInput[inputCount-1], "&") == TRUE){
			//if the last input matches "&", set the background bool to TRUE
			*isBackgroundBool = TRUE;

			//free the last element of the parsed input array of input strings and set it to NULL (since it was equal to "&" which shouldn't be passed to
			//execvp() or any built in command and should just be used to set the bool variable)
			free(parsedInput[inputCount - 1]);
			parsedInput[inputCount - 1] = NULL;

			//decrement the input count since the last input arg was removed from the parsed inputs array of strings
			inputCount--;
		}
	}

	//return the number of inputs
	return inputCount;
}

/*
NAME
isexit
SYNOPSIS
checks if the user entered exit
DESCRIPTION
receives the user input long string as a parameter. uses StringMatch to see if it matches "exit". if it does,
returns true. else returns false.
*/
int IsExit(char *userInputIn){
	//use StringMatch to check if the user entered "exit"
	if(StringMatch(userInputIn, EXIT) == TRUE){
		//if user input and "exit" matches, return true
		return TRUE;
	}
	//else return false
	return FALSE;
}

/*
NAME
isstatus
SYNOPSIS
checks if the user entered status
DESCRIPTION
receives the user input long string as a parameter. uses StringMatch to see if it matches "status". if it does,
returns true. else returns false.
*/
int IsStatus(char *userInputIn){
	//use StringMatch to check if the user entered "status"
	if(StringMatch(userInputIn, STATUS) == TRUE){
		//if user input and "status" matches, return true
		return TRUE;
	}
	//else return false
	return FALSE;
}

/*
NAME
ischangedir
SYNOPSIS
checks if the user entered cd (change directory)
DESCRIPTION
receives the user input long string as a parameter. uses StringMatch to see if it matches "cd". if it does,
returns true. else returns false.
*/
int IsChangeDir(char *userInputIn){
	//use StringMatch to check if the user entered "cd"
	if(StringMatch(userInputIn, CD) == TRUE){
		//if user input and "cd" matches, return true
		return TRUE;
	}
	//else return false
	return FALSE;
}

/*
NAME
isnoaction
SYNOPSIS
checks if the user entered something that requires no action
DESCRIPTION
receives the user input long string as a parameter. uses StringMatch to see if it matches "NO_ACTION". if it
does, returns true. else returns false. (note: the user's input has already been checked for if it's a blank line
composed of all spaces, a commend i.e. a line beginning with #, or a blank line composed of just a newline char. if
the user's input matched any of these things, the user's input string was set to NO_ACTION prior to being passed into
this function).
*/
int IsNoAction(char *userInputIn){
	//use StringMatch to check if the user's input matches the string NO_ACTION
	if(StringMatch(userInputIn, NO_ACTION) == TRUE){
		//if user input and NO_ACTION matches, return true
		return TRUE;
	}
	//else return false
	return FALSE;
}

/*
NAME
main
SYNOPSIS
is the main function. creates/initializes variables, calls other needed functions, provides the main loop for getting user input.
DESCRIPTION
is the main function. sets up the sigaction structs for SIGINT and SIGTSTP. calls various other needed functions. keeps track
of the exit status of the last foreground child, uses a fork counter to prevent fork bombs, keeps track of the background exit status,
number of background processes and keeps their pids in an array, forks children for both background and foreground processes, etc.
forks background and foreground child processes. receives no parameters. returns an int. 
*/
int main(){
	//additional sigaction information adapted from:
	//http://www.tutorialspoint.com/unix_system_calls/sigaction.htm and
	//http://man7.org/linux/man-pages/man2/sigaction.2.html and
	//https://web.mst.edu/~ercal/284/SignalExamples/sigaction-EX1.c
	
	//set up SIGINT struct and signal handler:
	
	//initialize sigaction struct to empty {0}. double braces used to prevent warning errors
	//from the gcc compiler.
	struct sigaction SIGINT_action = {{0}};

	//instead of passing a function pointer, since we want the parent to ignore the SIGINT signal,
	//set the signal handler to SIG_IGN (i.e. signal ignore, so the parent process will ignore the SIGINT signal).
	SIGINT_action.sa_handler = SIG_IGN;

	//block/delay all signals arriving while this mask is in place
	sigfillset(&SIGINT_action.sa_mask);

	//set flags to zero (not using SA_RESTART because in my getline i use the other option listed in
	//the lecture notes, checking if getline returns -1 and if so continuing to loop for input, only
	//breaking out of the getline input loop when getline != -1).
	SIGINT_action.sa_flags = 0; 

	//specify an alternative signal handler function to be called (but it won't be used since
	//the SA_SIGINFO flag isn't set in sa_flags)
	sigaction(SIGINT, &SIGINT_action, NULL);


	//set up SIGTSTP struct and signal handler:

	//initialize sigaction struct to empty {0}. double braces used to prevent warning errors
	//from the gcc compiler.
	struct sigaction SIGTSTP_action = {{0}};

	//pass function pointer to the function CatchSIGTSTP, which will be the custom signal handler
	//function written by me.
	SIGTSTP_action.sa_handler = CatchSIGTSTP;

	//block/delay all signals arriving while this mask is in place
	sigfillset(&SIGTSTP_action.sa_mask);

	//set flags to zero (again, not using SA_RESTART because in my getline i use the other option listed in
	//the lecture notes, checking if getline returns -1 and if so continuing to loop for input, only
	//breaking out of the getline input loop when getline != -1) 
	SIGTSTP_action.sa_flags = 0;

	//specify an alternative signal handler function to be called (but it won't be used since
	//the SA_SIGINFO flag isn't set in sa_flags)
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	//exit status for the foreground and background processes. set to 0 to start w/ by default (so if user calls
	//status before any foreground processes have been run it's zero). can be changed if a foreground
	//process encounters errors and needs to exit w/ a non-zero status or if a foreground process
	//is terminated by a signal. can also be changed if a background process encounters errors
	//and needs to exit w/ a non-zero status or if it's terminated by a signal.
	int childExitStatus = 0;
	int backgroundExitStatus = 0;

	//process counter(used to perform periodic check on any background processes not yet exited or terminated by a signal)
	int foregroundProcessCount = 0;
	int backgroundProcessCount = 0;

	//fork counter (used to prevent fork bombs)
	int forkCount = 0;

	//Instructor brewster on osu cs 344 slack message board stated this kind of fixed array to store
	//PIDs was acceptable for this assignment (his example was an array of 128 integer pids).
	int backgroundPidArray[1000];
	int foregroundPidArray[1000];

	//create fixed character array to get the command (using this here because otherwise the user input received
	//in the do-while loop won't persist beyond the loop and thus can't be checked in the do-while conditional for
	//if the user has entered exit).
	char command[MAX_CHARS];
	
	//get user input as long as the user hasn't entered "exit"
	do{
		//create string for input file name of max chars, memset to null terminators. initialize to "NO_ACTION" (can be changed
		//later if the user actually enters an input file name)
		char inputFile[MAX_CHARS];
		memset(inputFile, '\0', sizeof(inputFile));
		strcpy(inputFile, NO_ACTION);

		//create string for output file name of max chars, memset to null terminators. initialize to "NO_ACTION" (can be
		//changed later if the user actually enters an output file name)
		char outputFile[MAX_CHARS];
		memset(outputFile, '\0', sizeof(outputFile));
		strcpy(outputFile, NO_ACTION);

		//create bool variable to determine if the current process has been requested to be in the background or not (should get
		//tripped even if the foreground-only mode is on). initialize to false, can be changed later if the last arg of the user
		//input is "&", which would mean the user requested the process to run in background mode and thus isBackground should be TRUE;
		int isBackground = FALSE;

		//create and memset with null terminators a string of MAX_CHARS length to hold the user's input. (don't set to NO_ACTION because
		//the user will have input for some kind here, unlike w the input/output files or background request, which are optional).
		char userInputStr[MAX_CHARS];
		memset(userInputStr, '\0', sizeof(userInputStr));

		//create the array of strings for parsed user input and malloc it
		char **parsedUserInput= malloc((MAX_ARGS) * sizeof(char*));

		//check that the malloc was successful. if not (== NULL), print error message to user and exit w a non-zero status
		if(parsedUserInput == NULL){
			perror("USER INPUT MALLOC ERROR\n");
			exit(1);
		}

		//print the command prompt to the user
		printf(": "); fflush(stdout);

		//call GetInputString to get the long input string from the user (using getline in the GetInputString function)
		GetInputString(userInputStr);

		//create and set the number of inputs to the number returned by the GetArgs function. Call the GetArgs function
		//to parse the arguments into user input command/args (put into the parsed user input array of strings), input file
		//for redirection (optional), output file for redirection (optional), and background toggle (optional)
		int numInputs = GetArgs(parsedUserInput, userInputStr, inputFile, outputFile, &isBackground);

		//if the user entered "exit", call the built in exit function
		if(IsExit(parsedUserInput[0]) == TRUE){
			ExitBuiltIn(foregroundProcessCount, backgroundProcessCount, backgroundPidArray, foregroundPidArray, childExitStatus);
		}
		//else if the user entered "status", call the built in status function
		else if(IsStatus(parsedUserInput[0]) == TRUE){
			StatusBuiltIn(childExitStatus);
		}
		//else if the user entered "cd", check the number of inputs
		else if(IsChangeDir(parsedUserInput[0]) == TRUE){
			//if the user entered "cd" and the num of inputs was 1, means user only entered "cd", call ChangeDirBuiltInNoArgs
			if(numInputs == 1){
				ChangeDirBuiltInNoArgs();
			}
			//else if the user entered "cd" and the num of inputs was > 1, means user entered cd plus one or more args,
			//call ChangeDirBuiltInOneArg with the first argument given by the user (even if user entered many args, only
			//use the first one as their desired file path, ignore any following args)
			else if(numInputs > 1){
				ChangeDirBuiltInOneArg(parsedUserInput[1]);
			}
		}
		//else if the user entered a comment, blank line of spaces, or a blank line of just one newline char, do nothing so user
		//just gets back to command prompt and can put in new input
		else if(IsNoAction(parsedUserInput[0]) == TRUE){
		}
		//else the user entered a non-built in command that will need to be execvp()'d
		else{
			//make sure the array to be used for execvp() is NULL-terminated so execvp() knows where the end
			//of the array is!!!!!
			parsedUserInput[numInputs] = NULL;

			//if user indicated to run the process in the background AND background
			//mode is currently allowed (i.e. user is NOT currently in foreground-only mode)
			if((isBackground == TRUE) && (backgroundPossibleGlobal == TRUE)){
				//create backgroundspawnpid and set to an impossible value (pids will never be negative numbers)
				pid_t backgroundspawnpid = -5;

				//check the fork count to prevent fork bombing
				if(forkCount < MAX_FORKS){

					//call fork() to create a background child process
					backgroundspawnpid = fork();
					switch(backgroundspawnpid){
						case -1: //error
							//if fork returns -1, was an error, no child process created. print message and exit w non-zero value
							perror("Hull Breach!"); exit(1);
							break;
						case 0: //i am the child
							//if fork returns 0, are in the child process now.

							//set the SIGINT_action.sa_handler to SIG_IGN (only SIG_IGN and SIG_DFL can persist through exec calls).
							//this will cause the background process to ignore any SIGINT (ctrl-C) signals and continue running.
							//SIG_IGN = ignore signal, SIG_DFL = respond to signals with default actions
							SIGINT_action.sa_handler = SIG_IGN;

							//set sigaction too (tried just setting the handler to SIG_IGN, but the signal didn't get ignored properly until
							//i added the sigaction alternative signal handler part even though the sa_handler is being used and not sa_sigaction)
							sigaction(SIGINT, &SIGINT_action, NULL);

							//set the SIGTSTP.sa_handler to SIG_IGN (only SIG_IGN and SIG_DFL can persist through exec calls).
							//this will cause the background process to ignore any SIGINT signals and continue running.
							SIGTSTP_action.sa_handler = SIG_IGN;
							
							//set sigaction too (tried just setting the handler to SIG_IGN, but the signal didn't get ignored properly until
							//i added the sigaction alternative signal handler part even though the sa_handler is being used and not sa_sigaction)
							sigaction(SIGTSTP, &SIGTSTP_action, NULL);
							
							//check if the background process needs input file redirection
							if(NeedsInputRedirect(inputFile) == TRUE){
								//if it does need redirection, redirect stdin to the input file using the RedirectInputFile function.
								//check if redirect failed (will return 1 if it failed).
								if(RedirectInputFile(inputFile) == 1){
									//if redirect input failed, set child exit status to 1 and exit w this exit status
									childExitStatus = 1;
									exit(childExitStatus);
								}
							}
							//else if the user did not enter any input redirect file, redirect input to /dev/null using the RedirectInputFile function
							else{ //redirect input to dev/null
								RedirectInputFile("/dev/null");
							}

							//check if the background process needs output file redirection
							if(NeedsOutputRedirect(outputFile) == TRUE){
								//if it does need redirection, redirect stdout to the output file using the RedirectOutputFile function.
								//check if redirect failed (will return 1 if failed).
								if(RedirectOutputFile(outputFile) == 1){
									//if redirect output failed, set child exit status to 1 and exit w this exit status
									childExitStatus = 1;
									exit(childExitStatus);
								}
							}

							//else if the user did not enter any output redirect file, redirect output to /dev/null using the RedirectOutputFile function
							else{ //redirect output to dev/null
								RedirectOutputFile("/dev/null");
							}
							//call the Execute function (execvp() will be called within this function)
							Execute(parsedUserInput, &childExitStatus);
							break;	
						default: //i am the parent
							//add the current background pid to the array of pids to be checked w waitpid (and WNOHANG) periodically later
							backgroundPidArray[backgroundProcessCount] = backgroundspawnpid;

							//print the pid of the background process just started
							printf("background pid is %d\n", backgroundspawnpid); fflush(stdout);

							//increment the background process counter
							backgroundProcessCount++;
							break;
					}
				}
				else{ //fork bombed
					//if fork bombed, print error message and exit w non-zero exit status
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

	//do while loop condition is to continue looping until the user has entered "exit" (i.e. IsExit is TRUE)
	}while(IsExit(command) == FALSE);
		
	//return 0 since this is the main function
	return 0;
}
