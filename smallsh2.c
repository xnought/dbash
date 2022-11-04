#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_BUFFER 2048

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

void cmdPrompt(char *buffer)
{
	printf(": ");
	fflush(stdout);

	readIntoBuffer(buffer);
}

int main()
{
	char buffer[MAX_BUFFER];

	while (1)
	{
		/* 1. The command prompt */
		cmdPrompt(buffer);
	}

	return 0;
}
