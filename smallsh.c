/*
Name: Molly Johnson
ONID: johnsmol
CS 344 Winter 2019
Due: 3/3/19
All information used to create this code is adapted from the OSU CS 344 Fall 2018
lectures, assignment instructions/hints, and my own work from Assignment 2 from this
class unless otherwise specifically indicated.
Note: Also adapted from my own work from 11/14/18 (took the class in the Fall 2018 term but
am retaking this term for a better grade)
*/

//added before #include <stdio.h> to prevent "implicit function declaration" warnings
//with getline. Adapted from:
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
#define MAX_CHARS 2048
#define MAX_ARGS 512
#define EXIT "exit"
#define CD "cd"
#define STATUS "status"
#define FALSE 0
#define TRUE 1
#define INVALID "INVALID"

//global variables
int exitStatus = 0; //exit status for the program. set to 0 to start with by default
int backgroundPossible = TRUE; //flag for if background mode is possible (if SIGSTP command given, should ignore "&" and just run foreground commands)


//function declarations
void catchSIGINT(int signo);
int isBuiltIn(char **inputArgs);
char *getPID();
char *replaceString(char *str, char *orig, char *rep);
void commandOrFileExpand(char *commandOrFile);
void argExpand(char **argsArrayIn, int argCounter);
void exitBuiltIn(char *buffer, char **inputArgs, int argCounter, char *inputFile, char *outputFile, char *background, char *userInput, int *backgroundArrayCopy,
		int *backgroundArray, int backgroundProcessCount);
void changeDirBuiltInWithArg(char **argArray);
void changeDirBuiltInNoArg();
void statusBuiltIn();


/*
NAME
statusbuiltin
SYNOPSIS
built in status command
DESCRIPTION
checks if process exited normally or due to signal
*/
void statusBuiltIn()
{
	//information on how to obtain exit status and terminating signal information using WIFEXITED
	//and WEXITSTATUS and WTERMSIG adapted from:
	//https://stackoverflow.com/questions/27306764/capturing-exit-status-code-of-child-process and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wifexited.html and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wtermsig.html and
	//https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_wexitstatus.html
	//and lecture (only either WIFEXITED or WTERMSIG macros will be non zero, not both)				
	if(WIFEXITED(exitStatus)!=0) //returns not 0 if process exited normally
	{
		//if no foreground command has been called yet, the value of statusIn passed in will just be 0, so is
		//no need for an additional if/else statement to check if a foreground process has been called or not.
		int exitStat = WEXITSTATUS(exitStatus); //returns exit code of the child when child process exited normally
		printf("Exit code was: %d\n", exitStat);
		fflush(stdout);
	}
	else //if process did not exit normally (i.e. ended due to signal), checks termination status to see which signal caused child process to exit
	{
		int termSignal = WTERMSIG(exitStatus);
		printf("Signal code was: %d\n", termSignal);
		fflush(stdout);
	}
}

/*
NAME
changedirbuiltinnoarg
SYNOPSIS
changes current directory w no args
DESCRIPTION
changes current directory to the home directory
*/
void changeDirBuiltInNoArg()
{
	//getting home environment variable adapted from:
	//https://stackoverflow.com/questions/31906192/how-to-use-environment-variable-in-a-c-program and
	//http://man7.org/linux/man-pages/man3/getenv.3.html and
	//https//www.tutorialspoint.com/c_standard_library/c_function_getenv.htm and
	//https://stackoverflow.com/questions/9493234/chdir-to-home-directory
	//chdir information adapted from:
	//https://linux.die.net/man/2/chdir
	//get home directory, then change current directory to home directory
	char *homeDir = getenv("HOME"); 
	chdir(homeDir);
}

/*
NAME
changedirbuiltinwitharg
SYNOPSIS
changes to specified directory
DESCRIPTION
changes current directory to one specified by user. if doesn't exist, gives error message
*/
void changeDirBuiltInWithArg(char **argArray)
{
	//chdir information adapted from:
	//https://linux.die.net/man/2/chdir
	//info on how to check if chdir() was successful or not adapted from:
	//https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.bpxbd00/rtchd.htm
	if(chdir(argArray[1]) != 0)
	{
		perror("chdir() to your specified directory has failed, no such directory here\n");
		fflush(stdout);
	}
	else
	{
		chdir(argArray[1]);
	}
}

/*
NAME
exitbuiltin
SYNOPSIS
built in exit function
DESCRIPTION
cleans up memory and exits
*/
void exitBuiltIn(char *buffer, char **inputArgs, int argCounter, char *inputFile, char *outputFile, char *background, char *userInput, int *backgroundArrayCopy,
	int *backgroundArray, int backgroundProcessCount)
{	
	//free memory (since if exit called will use sigterm and thus terminate the job/process, not allowing
	//the main function to reach completion and free this allocated memory normally

	//kill remaining processes
	//using kill adapted from:
	//http://www.linuxquestions.org/questions/programming-9/how-to-kill-a-child-and-all-its-subsequent-child-process-in-c-247798/ and
	//https://www.booleanworld.com/kill-process-linux/ and
	//https://www.quora.com/What-is-the-difference-between-the-SIGINT-and-SIGTERM-signals-in-Linux-What%E2%80%99s-the-difference-between-the-SIGKILL-and-SIGSTOP-signals
	//call kill command and exit
	for(int j = 0; j < backgroundProcessCount; j++)
	{
	 	kill(backgroundArray[j], SIGTERM);
	}

	free(buffer);
	buffer = NULL; 
	for(int k = 0; k < argCounter; k++)
	{
		free(inputArgs[k]);
	}
	free(inputArgs);
	inputArgs = NULL;
	//free(command);
	//command = NULL;
	free (inputFile);
	inputFile = NULL;
	free (outputFile);
	outputFile = NULL;
	free (background);
	background = NULL;
	free(backgroundArrayCopy);
	backgroundArrayCopy = NULL;
	free(userInput);
	userInput = NULL;

		//kill(0, SIGTERM);
	//kill(0, SIGKILL);
	exit(0);
}

/*
NAME
replacestring
SYNOPSIS
replaces a string for var expansion 
DESCRIPTION
replaces specified substring of a string with a new substring for variable expansion
Adapted from: https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c and
https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
*/
char *replaceString(char *str, char *orig, char *rep)
{
	char *result;
	int i = 0;
	int cnt = 0;

	//save lengths of the replacement substring (pid) and the original substring ("$$")
	int newWlen = strlen(rep);
	int oldWlen = strlen(orig);

	//go through each char in the original long string, to check for occurrences of the original substring ("$$")
	for(i = 0; str[i] != '\0'; i++)
	{
		if (strstr(&str[i], orig) == &str[i])
		{
			cnt++;
			i += oldWlen - 1;
		}
	}

	result = (char *)malloc(i + cnt *(newWlen - oldWlen) + 1);
	i = 0;

	//replace each occurrence of the orig substring("$$") with the new subtsring (pid)
	while (*str)
	{
		if(strstr(str, orig) == str)
		{
			strcpy(&result[i], rep);
			i += newWlen;
			str += oldWlen;
		}
		else
		{
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
commandorfileexpand
SYNOPSIS
expands var name of a command or file
DESCRIPTION
expands every $$ in a variable to the parent pid
*/
void commandOrFileExpand(char *commandOrFile)
{
	char *str;
	char *orig;
	char *rep;
	str = (char*)malloc((MAX_CHARS +1) * sizeof(char));
	orig = (char*)malloc((MAX_CHARS +1) * sizeof(char));
	rep = (char*)malloc((MAX_CHARS +1) * sizeof(char));

	//get parent process pid
	char *pid = getPID();
	strcpy(str, commandOrFile); //set str to orig command/filename
	strcpy(orig, "$$"); //set orig substring to be replaced to be "$$"
	strcpy(rep, pid); //set replacement string for substring to be the parent pid

	//call replaceString to replace $$ with pid
	char *newStr = replaceString(str, orig, rep); 

	//copy the newly expanded string into the command or file 
	strcpy(commandOrFile, newStr);
	fflush(stdout);

	//free memory
	free(str);
	str = NULL;
	free(orig);
	orig = NULL;
	free(rep);
	rep = NULL;
}

/*
NAME
argexpand
SYNOPSIS
expands var name of each arg
DESCRIPTION
expands every $$ in every arg element to the parent pid
*/
void argExpand(char **argsArrayIn, int argCounter)
{
	for(int k = 0; k < argCounter; k++)
	{
		//check for each occurrence of 2 $$ signs in all args
		if(strstr(argsArrayIn[k], "$$") != NULL)
		{
			char *str;
			char *orig;
			char *rep;
			str = (char*)malloc((MAX_CHARS +1) * sizeof(char));
			orig = (char*)malloc((MAX_CHARS +1) * sizeof(char));
			rep = (char*)malloc((MAX_CHARS +1) * sizeof(char));

			//get the parent process pid
			char *pid = getPID();
			strcpy(str, argsArrayIn[k]); //set str to orig command/filename
			strcpy(orig, "$$"); //set orig substring to be replaced to be "$$"
			strcpy(rep, pid); //set replacement string for substring to be the parent pid

			//call replaceString to replace $$ with pid
			char *newStr = replaceString(str, orig, rep); 

			//copy the newly expanded string into the argument
			strcpy(argsArrayIn[k], newStr);

			//free memory
			free(str);
			str = NULL;
			free(orig);
			orig = NULL;
			free(rep);
			rep = NULL;
		}
	}
}

/*
NAME
getpid
SYNOPSIS
gets parent pid 
DESCRIPTION
gets parent pid,converts the int to a string 
Adapted directly from my own previous work (assignment 2)
*/
char *getPID()
{
	//static so will remain after function exits
	static char returnStringPID[] = "";
	
	//get parent process pid int
	int pid = getpid();	

	//convert int pid into string pid
	int length = snprintf(NULL, 0, "%d", pid);
	fflush(stdout);
	char *stringPID = malloc(length + 1);
	if(stringPID == NULL)
	{
		printf("ERROR, NOT ALLOCATED\n");
		fflush(stdout);
		exit(1);
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
isbuiltin
SYNOPSIS
checks if command is built in
DESCRIPTION
checks if command is built in (exit, status, or cd). if yes, returns true. else returns false.
*/
int isBuiltIn(char **inputArgs)
{
	//int used instead of bool. 0 = false, 1 = true. initialize to false
	int boolVal = FALSE;

	//check if first element of argument array is a built in command
	if((strcmp(inputArgs[0], EXIT) == 0) || (strcmp(inputArgs[0], CD) == 0) || (strcmp(inputArgs[0], STATUS) == 0))
	{
		//command is a built in, set bool to true
		boolVal = TRUE;
	}

	return boolVal;
}

/*
NAME
catchsigint
SYNOPSIS
sigint handler
DESCRIPTION
catches the sigint and handles it, prints message
*/
void catchSIGINT(int signo)
{
	//information about using write() adapted from:
	//http://codewiki.wikidot.com/c:system-calls:write (in addition to lecture example)
	//sigaction information adapted from:
	//http://www.tutorialspoint.com/unix_system_calls/sigaction.htm and
	//http://man7.org/linux/man-pages/man2/sigaction.2.html and
	//https://web.mst.edu/~ercal/284/SignalExamples/sigaction-EX1.c
	char *message = "SIGINT. CTRL-Z to stop.\n";
	write(STDOUT_FILENO, message, 28);
	fflush(stdout);
	//0 = stdin, 1 = stdout, 2 = stderr
	//write(1, message, 28);
}

/*
NAME
main
SYNOPSIS
is the main function
DESCRIPTION
creates signal structs, args/command/input and output file arrays, calls other functions,
takes and parses user's input, calls builtin commands or forks child processes if needed, determines
if need to run in background mode or foreground mode, uses execvp to call non-built in
processes. realize it is too long and a bit messy but ran out of time to break up into more functions.
*/
int main()
{
	//SIGINT handler
	//sigaction information adapted from:
	//http://www.tutorialspoint.com/unix_system_calls/sigaction.htm and
	//http://man7.org/linux/man-pages/man2/sigaction.2.html and
	//https://web.mst.edu/~ercal/284/SignalExamples/sigaction-EX1.c
	struct sigaction SIGINT_action = {0};
	//struct sigaction SIGINT_action = {{0}};
	SIGINT_action.sa_handler = catchSIGINT;
	sigfillset(&SIGINT_action.sa_mask);
	sigaction(SIGINT, &SIGINT_action, NULL);

	//dynamic array of ints adapted from:
	//https://stackoverflow.com/questions/40464927/how-to-dynamically-allocate-an-array-of-integers-in-c
	int max = MAX_ARGS + 1;
	int *backgroundArray = (int*)malloc(max * sizeof(int));
	int *backgroundArrayCopy = backgroundArray;
	int backgroundProcessCount = 0;
	int sourceFD, targetFD, result;
	char *userInput;
	int numInputs;
	userInput = (char *)malloc((MAX_CHARS + 1) * sizeof(char));
		
	if(userInput == NULL)
	{
		printf("ERROR, UNABLE TO ALLOCATE\n");
		fflush(stdout);
		exit(1);
	}

	do{
		//char *command;
		char *inputFile;
		char *outputFile;
		char *background;
		//command = (char *)malloc((MAX_CHARS + 1) * sizeof(char));
		inputFile = (char *)malloc((MAX_CHARS + 1) * sizeof(char));
		outputFile = (char *)malloc((MAX_CHARS + 1) * sizeof(char));
		background = (char *)malloc((MAX_CHARS + 1) * sizeof(char));

		if(inputFile == NULL ||
		outputFile == NULL || background == NULL)
		{
			printf("ERROR, UNABLE TO ALLOCATE\n");
			fflush(stdout);
			exit(1);
		}
		
		//strcpy(command, INVALID);
		strcpy(inputFile, INVALID);
		strcpy(outputFile, INVALID);
		strcpy(background, INVALID);

		char **inputArgs;
		inputArgs = malloc((MAX_CHARS + 1) * sizeof(char*));
		if(inputArgs == NULL)
		{
			printf("ERROR, UNABLE TO ALLOCATE\n");
			fflush(stdout);
			exit(1);
		}

		int argCounter = 0;
		numInputs = 0;
		printf(": ");
		//use of fflush adapted from:
		//https://www.tutorialspoint.com/c_standard_library/c_function_fflush.htm
		fflush(stdout);
		
		char *buffer;
		size_t bufsize = MAX_CHARS + 1;
		size_t characters;
		buffer = (char *)malloc(bufsize * sizeof(char));
		if (buffer == NULL)
		{
			printf("ERROR, UNABLE TO ALLOCATE\n");
			fflush(stdout);
			exit(1);
		}

		//make sure to account for possible signal interrupts with getline (from lecture)
		while(1)
		{
			characters = getline(&buffer, &bufsize, stdin);
			//if getline gets interrupted and returns -1, remove the error
			//status from stdin before resuming
			if(characters == -1)
			{
				clearerr(stdin);
			}
			else
			{
				//break out of loop, received valid input
				break;
			}
		}

		int isBuilt = FALSE;
		//if user entered some input other than just hitting enter (i.e. a blank line)
		//and also didn't enter a comment (check pos 0 of buffer string)
		if((strcmp(buffer, "\n") != 0) && (buffer[0] != '#'))
		{
			//remove the newline char that getline adds
			char *bufferNoNewLine = strtok(buffer, "\n");
			strcpy(userInput, bufferNoNewLine);

			//strtok use adapted from:
			//https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm
			char *space = " ";
			char *token;
			token = strtok(userInput, space);
			//inputArgs[argCounter] = malloc((MAX_CHARS + 1) * sizeof(char));
			inputArgs[argCounter] = malloc((MAX_CHARS + 1) * sizeof(char)); 
			strcpy(inputArgs[argCounter], token);
			numInputs++;
			argCounter++;
			//commandOrFileExpand(command);
			//strcpy(inputArgs[argCounter], token);
			int isOutFile = FALSE;
			int isInFile = FALSE;
			//int isBackground = FALSE;
			while(token != NULL)
			{
				token = strtok(NULL, space);
				if(token != NULL)
				{
					//check if token is an input file redirect, output file redirect, or background indicator
					if(strcmp(token, "<") == 0)
					{
						//set to true so next arg can be assigned to input file
						isInFile = TRUE;
					}
					else if(strcmp(token, ">") == 0)
					{
						//set to true so next arg can be assigned to output file
						isOutFile = TRUE;
					}
					/*else if(strcmp(token, "&") == 0)
					{
						strcpy(background, "&");
						isBackground = TRUE;
					}*/
					else
					{
						//check if this token is an input file, output file, or argument
						if(isInFile == TRUE)
						{
							//if this is input file, copy it to inputFile variable,
							strcpy(inputFile, token);
							commandOrFileExpand(inputFile);
							isInFile = FALSE;
						}
						else if(isOutFile == TRUE)
						{
							//if this is output file, copy it to outputFile variable,
							strcpy(outputFile, token);
							commandOrFileExpand(outputFile);
							isOutFile = FALSE;
						}
						else //if isn't an input file or an output file or redirect char (> or <), add to args array
						{
							inputArgs[argCounter] = malloc((MAX_CHARS + 1) * sizeof(char));
							strcpy(inputArgs[argCounter], token);
							argCounter++;
							numInputs++;
						}
					}
				}
			}

			//check if last arg is the "&" background character or not.
			//if is, remove from args array.
			//skip if no args present
			if(argCounter >1)
			{
				if(strcmp(inputArgs[argCounter - 1], "&") == 0)
				{
					strcpy(background, "&");
					free(inputArgs[argCounter - 1]);
					inputArgs[argCounter - 1] = NULL;
					argCounter--;
				}
			}

			//check all args for $$ and expand if needed
			argExpand(inputArgs, argCounter);

			//check if command is a built in
			isBuilt = isBuiltIn(inputArgs);
			if(isBuilt == TRUE)
			{
				if(strcmp(inputArgs[0], STATUS) == 0)
				{
					//since status is initialized to 0 to start with, so if no foreground process
					//has been run before this command it will have an exit status of 0 as described
					//in the assignment instructions
					statusBuiltIn();
				}
				else if(strcmp(inputArgs[0], CD) == 0)
				{
					if(argCounter == 1) //if cd called w no args
					{
						changeDirBuiltInNoArg();
					}
					else if(argCounter == 2) //if cd called w one arg
					{
						changeDirBuiltInWithArg(inputArgs);
					}
					else //if cd called w too many args
					{
						printf("you entered too many arguments for the cd command. This command can only take 0 or 1 arguments\n");
						fflush(stdout);
					}

				}
				else if(strcmp(inputArgs[0], EXIT) == 0)
				{
					//exitBuiltIn();	
					//won't check other args, just exits if exit entered as command, other args if present will be ignored and program will exit
					exitBuiltIn(buffer, inputArgs, argCounter, inputFile, outputFile, background, userInput, backgroundArrayCopy,backgroundArray,backgroundProcessCount );
				}
				//else
				//{
				//}
			}
			else //else isn't a built in command
			{
				//forking info adapted from https://www.geeksforgeeks.org/fork-system-call/ and
				//https://stackoverflow.com/questions/32810981/fork-function-in-c (and the lecture)
				pid_t spawnpid = -5;

				//fork w the child  pid
				spawnpid = fork();

				//if something went wrongy, errno var set, no child process created
				if(spawnpid == -1)
				{
					perror("forking error!\n");
					fflush(stdout);
					exit(1);
				}
				//in the child process, fork() returns 0
				else if (spawnpid == 0)
				{
					//file in/out redirect w forking adapted from https://stackoverflow.com/questions/5938139/redirecting-input-and-output-of-a-child-process-in-c
					//(and lecture)
					
					//redirect if needed. if a file name = INVALID, no redirect required
					if(strcmp(inputFile, INVALID) != 0) //if input file redirect needed
					{
						//open input file
						sourceFD = open(inputFile, O_RDONLY);		
						if(sourceFD == -1)
						{
							perror("error opening input file\n");
							exit(1);
						}
						else
						{
							//dup2 (duplicate file descriptor) info adapted from:
							//https://linux.die.net/man/2/dup2 (and lecture)
							result = dup2(sourceFD, 0);
							if (result == -1)
							{
								perror("error with dup2 and input file\n");
								exit(2);
							}
							//close on exec
							fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
						}
					}
					if(((strcmp(inputFile, INVALID) == 0) && backgroundPossible == TRUE) && (strcmp(background,INVALID) != 0))
					{
						static char *backgroundRedirect = "/dev/null";
						//open input file
						sourceFD = open(backgroundRedirect, O_RDONLY);		
						if(sourceFD == -1)
						{
							perror("error opening input file\n");
							exit(1);
						}
						else
						{
							//dup2 (duplicate file descriptor) info adapted from:
							//https://linux.die.net/man/2/dup2 (and lecture)
							result = dup2(sourceFD, 0);
							if (result == -1)
							{
								perror("error with dup2 and input file\n");
								exit(2);
							}
							//close on exec
							fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
						}
					}

					//}
					if(strcmp(outputFile, INVALID) != 0) //if output file redirect needed
					{
						//open output file
						//unix permissions info adapted from:
						//http://permissions-calculator.org/decode/0644/ (and lecture)
						targetFD = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if(targetFD == -1)
						{
							perror("error opening output file\n");
							exit(1);
						}
						else
						{
							result = dup2(targetFD, 1);
							if(result == -1)
							{
								perror("error with dup2 and output file\n");
								exit(2);
							}
							//close on exec
							fcntl(targetFD, F_SETFD, FD_CLOEXEC);
						}
					}
					if(((strcmp(outputFile, INVALID) == 0) && backgroundPossible == TRUE) && (strcmp(background,INVALID) != 0))
					{
						static char *backgroundRedirectOut = "/dev/null";
						//open output file
						//unix permissions info adapted from:
						//http://permissions-calculator.org/decode/0644/ (and lecture)
						targetFD = open(backgroundRedirectOut, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if(targetFD == -1)
						{
							perror("error opening output file\n");
							exit(1);
						}
						else
						{
							result = dup2(targetFD, 1);
							if(result == -1)
							{
								perror("error with dup2 and output file\n");
								exit(2);
							}
							//close on exec
							fcntl(targetFD, F_SETFD, FD_CLOEXEC);
						}
					}
					//using execvp() to make sure use PATH variable (suggested by TA
					//in OSU CS 344 slack discussion) instead of other variants of exec functions
					//execvp info adapted from:
					//https://stackoverflow.com/questions/27541910/how-to-use-execvp and
					//http://www.csl.mtu.edu/cs4411.ck/www/NOTES/process/fork/exec.html and
					//http://www.cs.ecu.edu/karl/4630/sum01/example1.html (and lecture)

					if(execvp(inputArgs[0], inputArgs) < 0)
					{
						//printf("execvp error with command %s\n", inputArgs[0]);
						//fflush(stdout);
						perror("execvp error with your command\n");
						exit(2);
					}
				}
				//in the parent process, fork() returns process id of the child process that was just created
				else
				{
					//check if background is possible and user requested it
					if((backgroundPossible == TRUE) && (strcmp(background, "&") == 0))
					{
						//waitpid, background, and WNOHANG info adapted from:
						//https://linux.die.net/man/2/waitpid and lecture and
						//https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.1.0/com.ibm.zos.v2r1.bpxbd00/rtwaip.htm
						//and discussion on the CS 344 OSU slack discussion board
						pid_t waitPID = waitpid(spawnpid, &exitStatus, WNOHANG);
						if(waitPID == -1)
						{
							perror("wait() error\n");
							exit(1);
						}
						printf("this process is in the background: %d\n", spawnpid);
						fflush(stdout);
					}
					else //don't use background keep in foreground
					{
						pid_t waitPID = waitpid(spawnpid, &exitStatus, 0);
						if(waitPID == -1)
						{
							perror("wait() error\n");
							exit(1);
						}
						backgroundProcessCount++;
						backgroundArray[backgroundProcessCount] = spawnpid;
						//printf("this pid is in the foreground: %d\n", spawnpid);
						fflush(stdout);
					}
				}
			}
			
		}
		//if user just hits enter (i.e. a blank line) or writes a comment, set userInput to invalid (set to something to prevent segfault) and re prompt
		else
		{
			strcpy(userInput, "INVALID");
			printf("\n");
			fflush(stdout);
		}



		//free memory
		free(buffer);
		buffer = NULL;
		//printf("the command is: %s\n", command);
		//fflush(stdout);
		//printf("the input file is: %s\n", inputFile);
		//fflush(stdout);
		//printf("the output file is: %s\n", outputFile);
		//fflush(stdout);
		//printf("the background is: %s\n", background);
		//fflush(stdout);
		//printf("the parent pid is: %s\n", getPID());
		//fflush(stdout);

		for(int k = 0; k < argCounter; k++)
		{
			//printf("arg %d is: %s\n", k, inputArgs[k]);
			//fflush(stdout);
			free(inputArgs[k]);
		}
		free(inputArgs);
		inputArgs = NULL;
		//free(command);
		//command = NULL;
		free (inputFile);
		inputFile = NULL;
		free (outputFile);
		outputFile = NULL;
		free (background);
		background = NULL;

	//print non-terminated remaining background processes
	//adapted from: https://stackoverflow.com/questions/26381944/how-to-check-if-a-forked-process-is-still-running-from-the-c-program
	/*if((strcmp(background, "&") == 0) && (backgroundProcessCount > 0)){
	for(int i = 0; i < backgroundProcessCount; i++)
	{
		if((waitpid(backgroundArray[i], &exitStatus, WNOHANG)) < 0)
		{
			printf("background process pid %d not completed\n", backgroundArray[i]);
			fflush(stdout);
		}
	}
	}*/
	//background process print causing core dump, needs fixed

	}while((strcmp(userInput, "exit") != 0) || (numInputs != 1));

	//free memory
	free(backgroundArrayCopy);
	backgroundArrayCopy = NULL;
	free(userInput);
	userInput = NULL;
	
	return 0;
}
