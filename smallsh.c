#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"

#define MAX_LINE 2048
#define MAX_ARGS 512
#define MAX_PID 10

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

struct llNode
{
	void *data;
	struct llNode *next;
};

struct ll
{
	struct llNode *start;
	struct llNode *end;
	int length;
};

struct ll *initLinkedList()
{
	struct ll *list = (struct ll *)malloc(sizeof(struct ll));
	list->start = NULL;
	list->end = NULL;
	list->length = 0;
	return list;
}
void pushLinkedList(struct ll *list, void *data)
{
	struct llNode *node = (struct llNode *)malloc(sizeof(struct llNode));
	node->data = data;
	node->next = NULL;

	if (list->start == NULL)
	{
		list->start = node;
		list->end = node;
	}
	else
	{
		list->end->next = node;
		list->end = node;
	}
	list->length++;
}
void freeLinkedListNode(struct llNode *node, void (*freeData)(void *))
{
	if (node == NULL)
	{
		return;
	}
	if (freeData)
	{
		freeData(node->data);
	}
	free(node);
}
void freeLinkedList(struct ll *list, void (*freeData)(void *))
{
	struct llNode *curNode = list->start;
	struct llNode *nextNode = curNode;
	while (curNode)
	{
		nextNode = curNode->next;
		freeLinkedListNode(curNode, freeData);
		curNode = nextNode;
	}
	free(list);
}

void freeToken(char *token)
{
	free(token);
}
char *allocToken(int length)
{
	char *token = (char *)malloc(sizeof(char) * length);
	return token;
}
char *copyToken(char *token)
{
	int length = strlen(token);
	char *tokenCpy = allocToken(length);
	strcpy(tokenCpy, token);
	return tokenCpy;
}
/*
	dash tokenizer
*/
struct ll *tokenizer(char *lineBuffer)
{
	struct ll *tokens = initLinkedList();

	char *token;
	char *tokenCpy;
	char *rest = lineBuffer;
	while (token = strtok_r(rest, " ", &rest))
	{
		pushLinkedList(tokens, copyToken(token));
	}

	return tokens;
}

void printTokens(struct ll *tokens)
{
	struct llNode *curNode = tokens->start;
	while (curNode)
	{
		printf("%s\n", (char *)curNode->data);
		curNode = curNode->next;
	}
}

/*
	expand the $$ to the process id
 */
void expandVariables(struct ll *tokens)
{
	pid_t curProcess = getpid();
	char *curProcessStr;

	struct llNode *curNode = tokens->start;
	while (curNode)
	{
		char *token = (char *)curNode->data;
		if (strcmp(token, "$$") == 0)
		{
			freeToken(token);
			/* then allocate for curProcess string */
			curProcessStr = (char *)malloc(sizeof(char) * MAX_PID);
			memset(curProcessStr, 0, MAX_PID);
			sprintf(curProcessStr, "%d", curProcess);
			curNode->data = curProcessStr;
		}
		curNode = curNode->next;
	}
}

int main()
{
	char *cmdBuffer[MAX_LINE];
	struct ll *tokens;

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
		tokens = tokenizer(cmdBuffer);
		/* 1. use tokens to expand $$ */
		expandVariables(tokens);

		/* 2. parse tokens for cd, exit, and status and run those commands */
		/* 3. otherwise, execute what the user gave */
		/* 4. send < into stdin */
		/* 5. send stdout to > file */
		/* 6. override signals */

		printTokens(tokens);
		freeLinkedList(tokens, freeToken);
	}
}
