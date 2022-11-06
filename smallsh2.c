#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_BUFFER 2048
#define MAX_TOKENS 517
#define MAX_ARGS 512
#define MAX_BG_PROCESSES 100

struct cmd
{
	/* all tokens parsed from the buffer 512 + '>' + '<' + 'fout' + 'fin' + 'background' = 517*/
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

int isBlank(struct cmd *cmdInfo)
{
	return cmdInfo->cmdName == NULL;
}
int isComment(struct cmd *cmdInfo)
{
	return cmdInfo->cmdName[0] == '#';
}
void clearArrayOfStrings(char **array, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		array[i] = NULL;
	}
}
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
void clearBuffer(char *buffer)
{
	memset(buffer, 0, MAX_BUFFER);
}
/*
	read in a line from stdin
	into the buffer used for the shell
 */
void readIntoBuffer(char *buffer)
{
	clearBuffer(buffer);
	fgets(buffer, MAX_BUFFER, stdin);

	/* fgets introduces \n, strip it! */
	int userInputLen = strlen(buffer);
	if (userInputLen != 0)
	{
		buffer[userInputLen - 1] = 0;
	}
}

void popArg(struct cmd *cmdInfo)
{
	if (cmdInfo->argCount > 0)
	{
		cmdInfo->argCount--;
		cmdInfo->args[cmdInfo->argCount] = NULL;
	}
}
void addArg(struct cmd *cmdInfo, char *token)
{
	cmdInfo->args[cmdInfo->argCount] = token;
	cmdInfo->argCount++;
}
void addToken(struct cmd *cmdInfo, char *token)
{
	cmdInfo->tokens[cmdInfo->tokenCount] = token;
	cmdInfo->tokenCount++;
}

void freeTokens(struct cmd *cmdInfo)
{
	int i;
	for (i = 0; i < cmdInfo->tokenCount; i++)
	{
		free(cmdInfo->tokens[i]);
	}
}

int dollarsOccursIn(char *token)
{
	int count = 0;
	int i = 0;
	int j = 1;
	int tokenLen = strlen(token);
	char a, b;
	while (i < tokenLen && token[i] != 0 && token[j] != 0)
	{
		a = token[i];
		b = token[j];
		if (a == '$' && b == '$')
		{
			count++;
			i++;
			j++;
		}
		i++;
		j++;
	}
	return count;
}

void replaceDollars(char *str, char *expanded, char *pidStr, int occurs)
{
	int i = 0;
	while (str[0])
	{
		if (strstr(str, "$$") == str)
		{
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
	the result needs to be freed
 */
char *replaceAll$$WithPid(char *string, char *pidString)
{
	int dollarsCount = dollarsOccursIn(string);
	int memoryIncrease = dollarsCount * (strlen(pidString) - 2);
	int newSize = (strlen(string) + memoryIncrease + 1);
	char *largerMemory = (char *)malloc(sizeof(char) * newSize);
	memset(largerMemory, 0, newSize);

	if (dollarsCount > 0)
	{
		replaceDollars(string, largerMemory, pidString, dollarsCount);
	}
	else
	{
		strcpy(largerMemory, string);
	}
	return largerMemory;
}

void parseCmdPrompt(char *buffer, struct cmd *cmdInfo, int foregroundMode, pid_t pid)
{
	char *rest = buffer;
	char *token;
	char *tokenCopy;
	int hitSpecialChar = -1;
	int i;
	char pidString[10] = {'\0'};
	sprintf(pidString, "%d", pid);

	resetCmd(cmdInfo);

	/* the first token is the command name */
	token = strtok_r(rest, " ", &rest);
	if (token != NULL)
	{
		tokenCopy = replaceAll$$WithPid(token, pidString);
		addToken(cmdInfo, tokenCopy);
	}
	cmdInfo->cmdName = cmdInfo->tokens[0];

	/* the next tokens are arguments until we hit a '<' or a '>' */
	while ((token = strtok_r(rest, " ", &rest)) != NULL && cmdInfo->tokenCount < MAX_TOKENS)
	{
		/* add each as a token */
		tokenCopy = replaceAll$$WithPid(token, pidString);
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
				/* if not a special char, its an arg!*/
				addArg(cmdInfo, tokenCopy);
			}
		}
	}

	if (hitSpecialChar != -1)
	{
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
		if (hitSpecialChar == -1)
		{
			popArg(cmdInfo);
		}
	}
}

void getCmdPrompt(char *buffer)
{
	printf(": ");
	fflush(stdout);

	readIntoBuffer(buffer);
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

	if (id == -1)
	{
		perror("fork failed");
		exit(1);
	}

	/* fork off to child  */
	if (id == 0)
	{
		/* all children ignore signal stop */
		sigStop.sa_handler = SIG_IGN;
		sigfillset(&sigInt.sa_mask);

		/* foreground processes make sure to exit with sig int */
		if (!cmdInfo->background)
		{
			/* if foreground process */
			sigInt.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sigInt, NULL);
		}

		/* handle for standard input redirect */
		if (cmdInfo->inputRedir != NULL)
		{
			int fd = open(cmdInfo->inputRedir, O_RDONLY);
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
			/* give read and write access to anyone, this is not satanic despite the octal */
			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
			int fd = open(cmdInfo->outputRedir, O_WRONLY | O_CREAT | O_TRUNC, mode);
			if (fd == -1)
			{
				printf("ERROR: cannot open %s for output\n", cmdInfo->outputRedir);
				fflush(stdout);
				exit(1);
			}
			dup2(fd, 1);
			close(fd);
		}

		/* combine the args and commands into one array for execvp */
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
	/* parent process */
	else
	{
		int childStatus;
		if (cmdInfo->background == 0)
		{
			waitpid(id, &childStatus, 0);
			*status = childStatus;
		}
		else
		{
			/* if it is a background process, wait no hang */
			pid_t childId = waitpid(id, &childStatus, WNOHANG);

			/* add to first available space */
			for (int i = 0; i < MAX_BG_PROCESSES; i++)
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
	}
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
	int childExited = WIFEXITED(status);
	int signalExited = !childExited;
	if (signalExited)
	{
		printSignal(status);
	}
	else
	{
		printExitStatus(status);
	}
}

int foregroundMode = 0;
void foregroundModeSignal(int signo)
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

int main()
{
	char buffer[MAX_BUFFER];
	struct cmd cmdInfo;
	pid_t smallshPid = getpid();
	int status = 0;
	pid_t bgProcesses[MAX_BG_PROCESSES] = {-1};

	for (int i = 0; i < MAX_BG_PROCESSES; i++)
	{
		bgProcesses[i] = -1;
	}

	/* ignore the sig int in parent like in lecture */
	struct sigaction sigInt = {0};
	struct sigaction sigStop = {0};

	/* ignore sig interrupt */
	sigInt.sa_handler = SIG_IGN;
	sigfillset(&sigInt.sa_mask);
	sigaction(SIGINT, &sigInt, NULL);

	/* sig stop will toggle the global foreground mode */
	sigStop.sa_handler = foregroundModeSignal;
	sigfillset(&sigStop.sa_mask);
	sigaction(SIGTSTP, &sigStop, NULL);

	while (1)
	{
		/* 1. The command prompt */
		getCmdPrompt(buffer);

		/* 2. parse the buffer for the command for key info */
		parseCmdPrompt(buffer, &cmdInfo, foregroundMode, smallshPid);

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

				freeTokens(&cmdInfo);
				break;
			}
			else if (strcmp(cmdInfo.cmdName, "cd") == 0)
			{
				if (cmdInfo.argCount == 0)
				{
					chdir(getenv("HOME"));
				}
				else
				{
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
				/* otherwise not a base command, execute it with exec! */
				execCmd(&cmdInfo, &status, bgProcesses, sigInt, sigStop);
			}
		}

		freeTokens(&cmdInfo);

		/* if foreground exited by user, print the terminating signal */
		if (!WIFEXITED(status) && WTERMSIG(status) == SIGINT)
		{
			printSignal(status);
		}

		/* otherwise check if the background processed has finished without
		actually waiting
		*/
		int i = 0;
		pid_t res;
		int childStatus;
		int exitSignal;
		for (i = 0; i < MAX_BG_PROCESSES; i++)
		{
			/* look at the background process id*/
			if (bgProcesses[i] != -1)
			{
				/* if exited in some way, tell the user its done! */
				res = waitpid(bgProcesses[i], &childStatus, WNOHANG);
				if (res != 0)
				{
					status = childStatus;
					printf("background pid %d is done: ", bgProcesses[i]);
					fflush(stdout);
					exitOrSignalStatus(status);

					/* and remove this one from the list of background processes since it finished*/
					bgProcesses[i] = -1;
				}
			}
		}
	}

	return 0;
}
