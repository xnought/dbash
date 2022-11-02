#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE 2048

void readStdIn(char *buffer)
{
	fgets(buffer, MAX_LINE, stdin);

	/* it records the new line, so remove that */
	size_t lineLength = strlen(buffer);
	if (lineLength > 0)
	{
		buffer[lineLength - 1] = 0;
	}
}

/*
	replaces the entire string with blanks
*/
void clearLineBuffer(char *lineBuffer)
{
	memset(lineBuffer, 0, MAX_LINE);
}

int isBlank(char *lineBuffer)
{
	return lineBuffer[0] == '\0';
}
int isComment(char *lineBuffer)
{
	return lineBuffer[0] == '#';
}

/*
	Print a : and get the user input
	returns the size of the buffer
 */
void commandPrompt(char *lineBuffer)
{
	/* command prompts with ':' and expects an input */
	printf(": ");
	readStdIn(lineBuffer);
}

int main()
{
	char *cmdBuffer[MAX_LINE];
	while (1)
	{
		/* take in user input : ______________ */
		clearLineBuffer(cmdBuffer);
		commandPrompt(cmdBuffer);

		/* don't do anything if not a command */
		if (isBlank(cmdBuffer) || isComment(cmdBuffer))
		{
			continue;
		}

		/* otherwise tokenize so I can use the input! */
		/* 1. use tokens to expand $$ */
		/* 2. parse tokens for cd, exit, and status and run those commands */
		/* 3. otherwise, execute what the user gave */
		/* 4. send < into stdin */
		/* 5. send stdout to > file */
		/* 6. override signals */
	}
}
