#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUFFER 2048
#define MAX_TOKENS 517
#define MAX_ARGS 512

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
	return strcmp(cmdInfo->cmdName, "#") == 0;
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
void clearArrayOfStrings(char **array, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		array[i] = NULL;
	}
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

char *copyToken(char *token)
{
	int length = strlen(token);
	char *tokenCpy = (char *)malloc(sizeof(char) * length);
	strcpy(tokenCpy, token);
	return tokenCpy;
}
void freeTokens(struct cmd *cmdInfo)
{
	int i;
	for (i = 0; i < cmdInfo->tokenCount; i++)
	{
		if (cmdInfo->tokens[i] != NULL)
		{
			free(cmdInfo->tokens[i]);
		}
	}
}

void parseCmdPrompt(char *buffer, struct cmd *cmdInfo, int foregroundMode)
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
		tokenCopy = copyToken(token);
		addToken(cmdInfo, tokenCopy);
	}
	cmdInfo->cmdName = cmdInfo->tokens[0];

	/* the next tokens are arguments until we hit a '<' or a '>' */
	while ((token = strtok_r(rest, " ", &rest)) != NULL && cmdInfo->tokenCount < MAX_TOKENS)
	{
		/* add each as a token */
		tokenCopy = copyToken(token);
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
	if (cmdInfo->tokenCount > 0 && strcmp(cmdInfo->tokens[cmdInfo->tokenCount - 1], "&") == 0 && foregroundMode == 0)
	{
		cmdInfo->background = 1;
		/* if never stopped short, pop off the & at the end of the arg */
		if (hitSpecialChar == -1)
		{
			popArg(cmdInfo);
		}

		if (cmdInfo->inputRedir == NULL)
		{
			cmdInfo->inputRedir = "dev/null";
		}
		if (cmdInfo->outputRedir == NULL)
		{
			cmdInfo->outputRedir = "dev/null";
		}
	}
}

void getCmdPrompt(char *buffer)
{
	printf(": ");
	fflush(stdout);

	readIntoBuffer(buffer);
}

int main()
{
	char buffer[MAX_BUFFER];
	struct cmd cmdInfo;
	int foregroundMode = 0;
	pid_t smallshPid = getpid();

	while (1)
	{
		/* 1. The command prompt */
		getCmdPrompt(buffer);

		/* 2. parse the buffer for the command for key info */
		parseCmdPrompt(buffer, &cmdInfo, foregroundMode);

		if (isBlank(&cmdInfo) || isComment(&cmdInfo))
		{
			continue;
		}

		freeTokens(&cmdInfo);
	}

	return 0;
}
