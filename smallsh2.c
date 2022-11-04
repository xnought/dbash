#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

void parseCmdPrompt(char *buffer, struct cmd *cmdInfo, int foregroundMode)
{
	char *rest = buffer;
	char *token;
	int hitSpecialChar = -1;
	char *lastWord;

	resetCmd(cmdInfo);

	/* the first token is the command name */
	token = strtok_r(rest, " ", &rest);
	addToken(cmdInfo, token);
	cmdInfo->cmdName = token;

	/* the next tokens are arguments until we hit a '<' or a '>' */
	while ((token = strtok_r(rest, " ", &rest)) != NULL && cmdInfo->tokenCount < MAX_TOKENS)
	{
		/* add each as a token */
		addToken(cmdInfo, token);
		if (hitSpecialChar == -1)
		{

			if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, "&") == 0)
			{
				/* subtract 1 since we want an index */
				hitSpecialChar = cmdInfo->tokenCount - 1;
			}
			else
			{
				/* if not a special char, its an arg!*/
				addArg(cmdInfo, token);
			}
		}
	}

	int i = hitSpecialChar;
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
		else if (strcmp(cmdInfo->tokens[i], "&") == 0 && foregroundMode == 0)
		{
			cmdInfo->background = 1;
		}
		i++;
	}

	/* if a background process, send io to dev/null if not specified */
	if (cmdInfo->background)
	{
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

	while (1)
	{
		/* 1. The command prompt */
		getCmdPrompt(buffer);

		/* 2. parse the buffer for the command for key info */
		parseCmdPrompt(buffer, &cmdInfo, foregroundMode);
	}

	return 0;
}
