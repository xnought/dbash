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
		: script args_1...args_512
	*/
	char *script;
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

	cmdInfo->script = NULL;

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

void parseCmdPrompt(char *buffer, struct cmd *cmdInfo)
{
	resetCmd(cmdInfo);
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

	while (1)
	{
		/* 1. The command prompt */
		getCmdPrompt(buffer);

		/* 2. parse the buffer for the command */
		parseCmdPrompt(buffer, &cmdInfo);
	}

	return 0;
}
