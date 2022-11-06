#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

/* hard limits */
#define MAX_BUFFER 2048
#define MAX_TOKENS 517
#define MAX_ARGS 512
#define MAX_BG_PROCESSES 100
#define MAX_PID 10

/* semantically meaningful numbers */
#define EMPTY_PID -1

/* signal handler toggles this on sig stop signal */
int foregroundMode = 0;

/*
	this struct will hold the key information
	parsed from the user input shell command
*/
struct cmd
{
	/* all tokens parsed from the buffer 512 + '>' + '<' + 'fout' + 'fin' + 'background' = 517 */
	char *tokens[MAX_TOKENS];
	int tokenCount;

	/*
		all the arguments parsed from the tokens
		: cmdName args_1...args_512
	*/
	char *cmdName;
	char *args[MAX_ARGS];
	int argCount;

	/* files to redirect stdin or stdout to */
	char *inputRedir;
	char *outputRedir;

	/* should we run this program in the background? */
	int background;
};

/*
	deletes the last argument from the cmd struct
 */
void popArg(struct cmd *cmdInfo)
{
	if (cmdInfo->argCount > 0)
	{
		cmdInfo->argCount--;
		cmdInfo->args[cmdInfo->argCount] = NULL;
	}
}

/*
	adds a string argument to the cmd struct
 */
void addArg(struct cmd *cmdInfo, char *token)
{
	cmdInfo->args[cmdInfo->argCount] = token;
	cmdInfo->argCount++;
}

/*
	adds a token to the cmd struct
 */
void addToken(struct cmd *cmdInfo, char *token)
{
	cmdInfo->tokens[cmdInfo->tokenCount] = token;
	cmdInfo->tokenCount++;
}

/*
	Given a 2D array of strings,
	set all those char pointers to NULL
 */
void clearArrayOfStrings(char **array, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		array[i] = NULL;
	}
}

/*
	make sure the parsed cmd structure
	is entirely cleared out to the default state
*/
void resetCmd(struct cmd *cmdInfo)
{
	clearArrayOfStrings(cmdInfo->tokens, MAX_TOKENS);
	cmdInfo->tokenCount = 0;

	clearArrayOfStrings(cmdInfo->args, MAX_ARGS);
	cmdInfo->argCount = 0;

	cmdInfo->cmdName = NULL;

	cmdInfo->inputRedir = NULL;
	cmdInfo->outputRedir = NULL;

	cmdInfo->background = 0;
}

/*
	frees malloced strings through tokens
	since args are a subset of tokens, this will clear those
*/
void freeTokens(struct cmd *cmdInfo)
{
	int i;
	for (i = 0; i < cmdInfo->tokenCount; i++)
	{
		free(cmdInfo->tokens[i]);
	}
}

int isBlank(struct cmd *cmdInfo)
{
	return cmdInfo->cmdName == NULL;
}

int isComment(struct cmd *cmdInfo)
{
	return cmdInfo->cmdName[0] == '#';
}

void printExitStatus(int status)
{
	printf("exit value of %d\n", WEXITSTATUS(status));
	fflush(stdout);
}
void printSignal(int status)
{
	printf("terminated by signal %d\n", WTERMSIG(status));
	fflush(stdout);
}

void exitOrSignalStatus(int status)
{
	if (WIFSIGNALED(status))
	{
		printSignal(status);
	}
	else if (WIFEXITED(status))
	{
		printExitStatus(status);
	}
}

/*
	toggle mode and log to the user which mode we are in
	uses write since it is reentrant
 */
void foregroundModeSignal(int sig)
{
	foregroundMode = !foregroundMode;
	if (foregroundMode)
	{
		write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 51);
		fflush(stdout);
	}
	else
	{
		write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 31);
		fflush(stdout);
	}
}

/*
	Counts how many times $$ is found in a given string
 */
int count$$(char *token)
{
	int i = 0;
	int j = 1;
	int tokenLen = strlen(token);
	char a;
	char b;
	int count = 0;

	/* while we're looking within the string... */
	while (i < tokenLen && token[i] != 0 && token[j] != 0)
	{
		/* find out if found a $$*/
		a = token[i];
		b = token[j];
		if (a == '$' && b == '$')
		{
			/* count that we've found it and move on */
			count++;
			i++;
			j++;
		}
		i++;
		j++;
	}
	return count;
}

/*
	Replaces all substring $$ with the process id of the parent
 */
void replaceAll$$(char *str, char *expanded, char *pidStr, int occurs)
{
	int i = 0;
	/* while we're looking through the characters in the string... */
	while (str[0])
	{
		/* if found the substring $$ */
		if (strstr(str, "$$") == str)
		{
			/* at that location instead of $$, do the pid  */
			strcpy(&expanded[i], pidStr);
			i += strlen(pidStr);
			str += 2;
		}
		else
		{
			/* adds the normal characters to the new result */
			expanded[i] = str[0];
			/* move on in the expanded string too */
			i++;
			/* shuv the string over, (dont consider ones before)*/
			*str++;
		}
	}
}
/*
	Expands $$ in the string to process ID
	returns a copy of the string that is >= the original
	the result needs to be freed with free()
 */
char *variable$$Expansion(char *string, char *pidString)
{
	/* compute memory increase */
	int dollarsCount = count$$(string);
	int memoryIncrease = dollarsCount * (strlen(pidString) - 2);
	int newSize = (strlen(string) + memoryIncrease + 1);

	/* allocate larger or equal to length string */
	char *largerMemory = (char *)malloc(sizeof(char) * newSize);
	memset(largerMemory, 0, newSize);

	/* if increase in size, do the $$ expansion, otherwise just do copy */
	if (dollarsCount > 0)
	{
		replaceAll$$(string, largerMemory, pidString, dollarsCount);
	}
	else
	{
		strcpy(largerMemory, string);
	}

	return largerMemory;
}

/*
	Given a buffer, parse the important information into
	a cmd struct. This will give you all the nice information
	needed for execution
 */
void parseCmdPrompt(char *buffer, struct cmd *cmdInfo, int foregroundMode, char *pidString)
{
	char *rest = buffer;
	char *token;
	char *tokenCopy;
	int hitSpecialChar = -1;
	int i;

	resetCmd(cmdInfo);

	/* the first token is the command name */
	token = strtok_r(rest, " ", &rest);
	if (token != NULL)
	{
		tokenCopy = variable$$Expansion(token, pidString);
		addToken(cmdInfo, tokenCopy);
	}
	cmdInfo->cmdName = cmdInfo->tokens[0];

	/* the next tokens are arguments until we hit a '<' or a '>' */
	while ((token = strtok_r(rest, " ", &rest)) != NULL && cmdInfo->tokenCount < MAX_TOKENS)
	{
		/* add each as a token */
		tokenCopy = variable$$Expansion(token, pidString);
		addToken(cmdInfo, tokenCopy);
		if (hitSpecialChar == -1)
		{

			if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0)
			{
				/* subtract 1 since we want an index */
				hitSpecialChar = cmdInfo->tokenCount - 1;
			}
			else
			{
				/* if not a special char, its an arg! */
				addArg(cmdInfo, tokenCopy);
			}
		}
	}

	/* if we stopped early for a special char (< or >) */
	if (hitSpecialChar != -1)
	{
		/* determine which I found, and add the file to the cmd struct */
		i = hitSpecialChar;
		while (i < cmdInfo->tokenCount)
		{

			if (strcmp(cmdInfo->tokens[i], "<") == 0)
			{
				i++;
				cmdInfo->inputRedir = cmdInfo->tokens[i];
			}
			else if (strcmp(cmdInfo->tokens[i], ">") == 0)
			{
				i++;
				cmdInfo->outputRedir = cmdInfo->tokens[i];
			}
			i++;
		}
	}

	/* if a background process, send io to dev/null if not specified */
	if (cmdInfo->tokenCount > 0 && strcmp(cmdInfo->tokens[cmdInfo->tokenCount - 1], "&") == 0)
	{
		/* if we are struct foreground mode, no need to even do this stuff */
		if (!foregroundMode)
		{
			cmdInfo->background = 1;
			if (cmdInfo->inputRedir == NULL)
			{
				cmdInfo->inputRedir = "/dev/null";
			}
			if (cmdInfo->outputRedir == NULL)
			{
				cmdInfo->outputRedir = "/dev/null";
			}
		}

		/* if never stopped short, pop off the & at the end of the arg */
		/* this is an artifact of adding all the tokens */
		if (hitSpecialChar == -1)
		{
			popArg(cmdInfo);
		}
	}
}

/*
	tell the user what the background just added
	and add it to the list of active bg processes
 */
void logBackgroundProcess(pid_t bgProcesses[MAX_BG_PROCESSES], pid_t id)
{
	int i;
	/*
		by construction bgProcesses is -1 where no process ID
		and something else if not -1, add to first available spot
	*/
	for (i = 0; i < MAX_BG_PROCESSES; i++)
	{
		if (bgProcesses[i] == -1)
		{
			bgProcesses[i] = id;
			break;
		}
	}

	printf("background pid is %d\n", id);
	fflush(stdout);
}

/*
	Takes in the command information
	and executes the program looking into PATH

	Also if < exists, redirect stdin
	Also if > exists, redirect stdout
*/
void execCmd(struct cmd *cmdInfo, int *status, pid_t bgProcesses[MAX_BG_PROCESSES],
			 struct sigaction sigInt, struct sigaction sigStop)
{
	pid_t id = fork();

	/* if the process failed, we are donezo*/
	if (id == -1)
	{
		perror("fork failed");
		exit(1);
	}

	/* fork off to child clone */
	if (id == 0)
	{

		/* all children ignore signal stop */
		sigStop.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &sigStop, NULL);

		/* foreground processes make sure to exit with sig int */
		if (!cmdInfo->background)
		{
			sigInt.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sigInt, NULL);
		}

		/* handle for standard input redirect */
		if (cmdInfo->inputRedir != NULL)
		{
			int fd = open(cmdInfo->inputRedir, O_RDONLY);
			/* if the file cannot be opened, tell the user! And exit just the child process*/
			if (fd == -1)
			{
				printf("ERROR: cannot open %s for input\n", cmdInfo->inputRedir);
				fflush(stdout);
				exit(1);
			}
			dup2(fd, 0);
			close(fd);
		}

		/* handle for standard output redirect */
		if (cmdInfo->outputRedir != NULL)
		{
			/*
				in write only, create a new file if not found
				and truncate if it is found
			*/
			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
			int fd = open(cmdInfo->outputRedir, O_WRONLY | O_CREAT | O_TRUNC, mode);

			/* if the file cannot be opened, tell the user! And exit just the child process*/
			if (fd == -1)
			{
				printf("ERROR: cannot open %s for output\n", cmdInfo->outputRedir);
				fflush(stdout);
				exit(1);
			}
			dup2(fd, 1);
			close(fd);
		}

		/*
			exec vp needs an array with the cmd at the start and the args
			then with a NULL at the last index
		*/
		char *cmdAndArgs[cmdInfo->argCount + 1 + 1];
		for (int i = 0; i < cmdInfo->argCount; i++)
		{
			cmdAndArgs[i + 1] = cmdInfo->args[i];
		}
		cmdAndArgs[0] = cmdInfo->cmdName;
		cmdAndArgs[cmdInfo->argCount + 1] = NULL;

		/* execute or handle the not found execute */
		if (execvp(cmdInfo->cmdName, cmdAndArgs) == -1)
		{
			printf("ERROR: %s not found\n", cmdInfo->cmdName);
			fflush(stdout);
			exit(1);
		}
	}
	else
	{
		/* parent process */
		int childStatus;
		if (cmdInfo->background == 0)
		{
			/* if a foreground process, actually wait! */
			waitpid(id, &childStatus, 0);
			*status = childStatus;

			/* if foreground exited by user with sig int, print the terminating signal */
			if (WIFSIGNALED(*status))
			{
				printSignal(*status);
			}
		}
		else
		{
			/* if it is a background process, don't actually wait, just log it as running */
			/* status gets updated later on when we actually for real wait() */
			pid_t childId = waitpid(id, &childStatus, WNOHANG);
			logBackgroundProcess(bgProcesses, id);
		}
	}
}

/*
	read in a line from stdin
	into the buffer used for the shell
 */
void readIntoBuffer(char *buffer)
{
	memset(buffer, 0, MAX_BUFFER);
	fgets(buffer, MAX_BUFFER, stdin);

	/* fgets introduces \n, strip it! */
	int userInputLen = strlen(buffer);
	if (userInputLen != 0)
	{
		buffer[userInputLen - 1] = 0;
	}
}

/*
	: ___________
	and returns the __________ part (the text the user input)

	the sig action magic here is because if I've not entered something,
	sig stop should interrupt and continue to the next line,
	but if we're in a foreground process, we should not exit and continue
 */
void getCmdPrompt(char *buffer, struct sigaction sigStop)
{

	sigStop.sa_flags = 0;
	sigaction(SIGTSTP, &sigStop, NULL);

	printf(": ");
	fflush(stdout);

	readIntoBuffer(buffer);

	sigStop.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &sigStop, NULL);
}

void checkBackgroundProcesses(pid_t bgProcesses[MAX_BG_PROCESSES], int *status)
{
	int i = 0;
	pid_t res;
	int childStatus;
	int exitSignal;
	for (i = 0; i < MAX_BG_PROCESSES; i++)
	{
		/* look at the background process id*/
		if (bgProcesses[i] != EMPTY_PID)
		{
			/* if exited in some way, tell the user its done! */
			res = waitpid(bgProcesses[i], &childStatus, WNOHANG);
			if (res > 0)
			{
				*status = childStatus;
				printf("background pid %d is done: ", bgProcesses[i]);
				fflush(stdout);
				exitOrSignalStatus(*status);

				/* and remove this one from the list of background processes since it finished*/
				bgProcesses[i] = EMPTY_PID;
			}
		}
	}
}

int main()
{
	char buffer[MAX_BUFFER];
	struct cmd cmdInfo;
	int status = 0;

	pid_t parentPid = getpid();
	char pidString[MAX_PID] = {'\0'};

	pid_t bgProcesses[MAX_BG_PROCESSES];
	int i;

	struct sigaction sigInt = {0};
	struct sigaction sigStop = {0};

	/* store the pid as a string for $$ expansion later */
	sprintf(pidString, "%d", parentPid);

	/* initialize all background processes to nothing */
	for (i = 0; i < MAX_BG_PROCESSES; i++)
	{
		bgProcesses[i] = EMPTY_PID;
	}

	/* set parent to ignore interrupt signal */
	sigInt.sa_handler = SIG_IGN;
	sigfillset(&sigInt.sa_mask);
	sigInt.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sigInt, NULL);

	/* set the parent to listen to stop signal for mode toggling */
	sigStop.sa_handler = foregroundModeSignal;
	sigfillset(&sigStop.sa_mask);
	sigaction(SIGTSTP, &sigStop, NULL);

	/* the main shell logic */
	while (1)
	{
		/* 1. The command prompt */
		getCmdPrompt(buffer, sigStop);

		/* 2. parse the buffer for the command for key info */
		parseCmdPrompt(buffer, &cmdInfo, foregroundMode, pidString);

		/* given the command is something of substance (not comment or blank) */
		if (!isBlank(&cmdInfo) && !isComment(&cmdInfo))
		{
			/* base implemented function exit, cd and status */
			if (strcmp(cmdInfo.cmdName, "exit") == 0)
			{
				/* kill all child processes upon exit */
				for (int i = 0; i < MAX_BG_PROCESSES; i++)
				{
					if (bgProcesses[i] != -1)
					{
						kill(bgProcesses[i], SIGKILL);
					}
				}

				/* kill the shell, but first free the tokens in memory */
				freeTokens(&cmdInfo);
				break;
			}
			else if (strcmp(cmdInfo.cmdName, "cd") == 0)
			{
				if (cmdInfo.argCount == 0)
				{
					/* no file? no problem!, just cd back to HOME Env var */
					chdir(getenv("HOME"));
				}
				else
				{
					/* otherwise, go to that file, or tell the user houston we have a problem */
					if (chdir(cmdInfo.args[0]) == -1)
					{
						printf("%s does not exist\n", cmdInfo.args[0]);
						fflush(stdout);
					}
				}
			}
			else if (strcmp(cmdInfo.cmdName, "status") == 0)
			{
				exitOrSignalStatus(status);
			}
			else
			{
				/* not a base command? Then execute with the program from PATH var */
				execCmd(&cmdInfo, &status, bgProcesses, sigInt, sigStop);
			}
		}

		/*
			check for background processes that have finished,
			then print out the pid and status of the finished process
		 */
		checkBackgroundProcesses(bgProcesses, &status);

		/* free the strings allocated from malloc for tokens */
		freeTokens(&cmdInfo);
	}

	return 0;
}
