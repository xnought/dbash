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

/*
	This function will take the buffer and decide if it is blank or not
*/
int isBlankLineOrComment(char *lineBuffer)
{
	/* check first character is null or #*/
	char firstChar = lineBuffer[0];
	int blankOrComment = firstChar == '\0' || firstChar == '#';
	return blankOrComment;
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
	char *buffer[MAX_LINE];
	while (1)
	{
		clearLineBuffer(buffer);
		commandPrompt(buffer);

		if (isBlankLineOrComment(buffer))
		{
			continue;
		}
	}
}
