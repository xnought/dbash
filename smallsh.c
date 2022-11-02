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
		clearLineBuffer(cmdBuffer);
		commandPrompt(cmdBuffer);

		if (isBlank(cmdBuffer) || isComment(cmdBuffer))
		{
			printf("blank or comment\n");
			continue;
		}

		/* otherwise tokenize! */
	}
}
