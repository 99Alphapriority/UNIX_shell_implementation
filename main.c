#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#define MAX_LINE 80 /*The maximum length command*/
#define BUFF_SIZE 32
#define PNULL NULL
#define TOK_DELIMITER " \t\n\r"

/******************************************************************************
 * Split line into tokens, creating an array of command input by the user
 * ***************************************************************************/
void split_line(char* line, char** tokens)
{
	int position = 0;
	char *token;

	if(PNULL == tokens)
	{
		fprintf(stderr,"[%s:%d] memory not found for args[]\n",__func__, __LINE__);
		exit(EXIT_FAILURE);
	}

	token = strtok(line, TOK_DELIMITER);

	while(PNULL != token)
	{
		tokens[position] = token;
		position++;

		if(MAX_LINE/2 == position)
		{
			tokens[position] = PNULL;
			return;
		}

		token = strtok(NULL, TOK_DELIMITER);
	}

	tokens[position] = PNULL;
	return;
}
/******************************************************************************
 * Read a line from stdin
 *****************************************************************************/
char* read_line(void)
{
	char *line = NULL;
	ssize_t bufsize = 0;

	if(getline(&line, &bufsize, stdin) == -1)
	{
		if(feof(stdin))
		{
			exit(EXIT_SUCCESS);
		}
		else
		{
			fprintf(stderr,"[%s:%d] getline\n",__func__,__LINE__);
		}
	}

	return line;
}

int main(int argc, char **argv)
{
	char *args[MAX_LINE/2 + 1]; /*command line arguments*/
	int should_run = 1; /*flag to determine when to exit program*/
	char* input;

	while(should_run)
	{
		printf("osh> ");
		fflush(stdout);

		input = read_line();
		split_line(input, args);

		if(strcmp(args[0], "exit") == 0)
			should_run = 0;

		if(strcmp(args[0], "cd")== 0)
		{
			if(args[1] == NULL)
			{
				fprintf(stderr,"osh: expected arguments to \"cd\"\n");
				return 1;	
			}
			else if(0 != chdir(args[1]))
			{
				perror("osh");
				return 1;
			}
		}


		execvp(args[0], args);

		/**
		 * After reading user input, the steps are:
		 * (1) fork a child process using fork()
		 * (2) the child process will invoke execvp()
		 * (3) parent will invoke wait() unless command included &
		 */

	}

	return 0;
}
