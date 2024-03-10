#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<assert.h>

#define MAX_LINE 80 /*The maximum length command*/
#define BUFF_SIZE 32
#define PNULL NULL
#define TOK_DELIMITER " \t\n\r"

/******************************************************************************
 * Check if user included "&" in the command
 * ***************************************************************************/
int check_ampersand(char** args)
{
	int i = 1, j = 0;
	while(NULL != args[i])
	{
		/*check the presence of "&" at the end of line*/
		if((strcmp(args[i], "&") == 0) && (args[i+1] == NULL))
		{
			args[i] = NULL;
			return 0;
		}
		i++;
	}

	/*check if "&" is appended to the last command in the line
	 * eg: ps&, tcpdump&
	 */
	i--;
	while('\0' != args[i][j])
	{
		if('&' == args[i][j])
		{
			args[i][j] = '\0';
			return 0;
		}
		j++;
	}

	return 1;
}
/******************************************************************************
 * Execute the command using child process
 * ***************************************************************************/
void execute_cmd(char** args, int should_wait)
{
	pid_t pid;
	
	pid = fork();

	if(pid < 0)
	{
		/*error in spawning  child process*/
		perror("osh");
		return;
	}
	else if(pid == 0)
	{
		/*child process executes the command*/
		if(-1 == execvp(args[0], args))
			perror("osh");
		exit(EXIT_FAILURE);
	}
	else
	{
		if(should_wait)
			wait(NULL);
	}

}
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
	FILE* fp = fopen("history.txt", "w");
	assert(fp != NULL);
	char *lastCmd[MAX_LINE/2 + 1]; /*to store last executed command*/

	while(should_run)
	{
		int idx = 0;

		printf("osh> ");
		fflush(stdout);

		input = read_line();
		split_line(input, args);

		if(NULL == args[0])
			continue;

		if(strcmp(args[0], "!!") == 0)
		{
			if(NULL == lastCmd[0])
			{
				fprintf(stderr,"osh: No commands in history\n");
				continue;
			}

			if(strcmp(lastCmd[0], "cd")== 0)
                	{
                        	if(lastCmd[1] == NULL)
                        	{
                                	fprintf(stderr,"osh: expected arguments to \"cd\"\n");
                                	return 1;
                        	}
                        	else if(0 != chdir(lastCmd[1]))
                        	{
                                	perror("osh");
                                	return 1;
                        	}
                	}
                	else
                	{		
                        	/**
                        	* After reading user input, the steps are:
                        	* (1) fork a child process using fork()
                        	* (2) the child process will invoke execvp()
                        	* (3) parent will invoke wait() unless command included &
                        	*/

                        	int should_wait = check_ampersand(lastCmd);
                        	execute_cmd(lastCmd, should_wait);
                	}
		}
		else
		{
			/*write the command in the history*/
			fprintf(fp,"%s\n", input);
		
			/*copy the command in lastCmd*/
			memset(lastCmd, 0, sizeof(lastCmd));
			while(NULL != args[idx])
			{
				lastCmd[idx] = args[idx];
				idx++;
			};
			lastCmd[idx] = NULL;


			if(strcmp(args[0], "exit") == 0)
				should_run = 0;
			else if(strcmp(args[0], "cd")== 0)
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
			else
			{
				/**
                 		* After reading user input, the steps are:
                 		* (1) fork a child process using fork()
                 		* (2) the child process will invoke execvp()
                 		* (3) parent will invoke wait() unless command included &
                 		*/
			
				int should_wait = check_ampersand(args);
				execute_cmd(args, should_wait);
			}

		}
	}

	fclose(fp);
	return 0;
}
