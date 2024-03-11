#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<assert.h>
#include<fcntl.h>

#define MAX_LINE 80 /*The maximum length command*/
#define BUFF_SIZE 32
#define PNULL NULL
#define TOK_DELIMITER " \t\n\r"
#define READ_END 0
#define WRITE_END 1

typedef struct{
	int flag;
	char* fileName;
}redirect_st;

typedef struct{
	int flag;
	char** cmd1;
	char** cmd2;
}pipe_st;

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
void execute_cmd(char** args, int should_wait, redirect_st* redirect, pipe_st* pipeCmd)
{
	pid_t pid;
	int pipeFD[2];
	int inputFD = STDIN_FILENO;
	int outputFD = STDOUT_FILENO;
	int pipePID1 = 0;
	int pipePID2 = 0;
	
	if(0 != redirect->flag)
        {
                if(1 == redirect->flag)
                {
			outputFD = open(redirect->fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if(outputFD < 0)
			{
				perror("osh");
				return;
			}
                }
                else if(2 == redirect->flag)
                {
			inputFD = open(redirect->fileName, O_RDONLY);
			if(inputFD < 0)
			{
				perror("osh");
				return;
			}
                }
        }

	pid = fork();

	if(pid < 0)
	{
		/*error in spawning  child process*/
		perror("osh");
		return;
	}
	else if(pid == 0)
	{
		if(0 != redirect->flag)
		{
			if(STDIN_FILENO != inputFD)
			{
				dup2(inputFD, STDIN_FILENO);
				close(inputFD);
			}
			else if(STDOUT_FILENO != outputFD)
			{
				dup2(outputFD, STDOUT_FILENO);
				close(outputFD);
			}
		}
		else if(1 == pipeCmd->flag)
		{
			if(pipe(pipeFD) == -1)
			{
				perror("osh");
				return;
			}

			pipePID1 = fork();

			if(pipePID1 < 0)
			{
				perror("osh");
				return;
			}
			else if(pipePID1 == 0)
			{
				close(pipeFD[READ_END]);

				dup2(pipeFD[WRITE_END], STDOUT_FILENO);

				close(pipeFD[WRITE_END]);

				if(-1 == execvp(pipeCmd->cmd1[0], pipeCmd->cmd1))
					perror("osh");
				exit(EXIT_FAILURE);
			}

			pipePID2 = fork();

			if(pipePID2 < 0)
			{
				perror("osh");
				return;
			}
			else if(pipePID2 == 0)
			{
				close(pipeFD[WRITE_END]);

				dup2(pipeFD[READ_END], STDIN_FILENO);

				close(pipeFD[READ_END]);


				if(-1 == execvp(pipeCmd->cmd2[0], pipeCmd->cmd2))
					perror("osh");
				exit(EXIT_FAILURE);
			}
			
			/*parent process*/
			close(pipeFD[READ_END]);
			close(pipeFD[WRITE_END]);

			waitpid(pipePID1, NULL, 0);
			waitpid(pipePID2, NULL, 0);
			exit(EXIT_SUCCESS);
		}

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
/******************************************************************************
 * Check for redirection
 * ***************************************************************************/
redirect_st* check_redirection(char** args)
{
	redirect_st* redirect = (redirect_st*)malloc(sizeof(redirect_st));
	int i = 0;

	while(NULL != args[i])
	{
		if(strcmp(args[i], ">") == 0)
		{
			redirect->flag = 1;
			redirect->fileName = args[i+1];
			args[i] = NULL;
			return redirect;
		}
		else if(strcmp(args[i], "<") == 0)
		{
			redirect->flag = 2;
			redirect->fileName = args[i+1];
			args[i] = NULL;
			return redirect;
		}
	
		i++;
	}
	
	redirect->flag = 0;
	redirect->fileName = 0;

	return redirect;
}

/******************************************************************************
 * Check for pipe
 * ***************************************************************************/
pipe_st* check_pipe(char** args)
{
	pipe_st* pipeCmd = (pipe_st*)malloc(sizeof(pipe_st));
	int i = 0;

	while(NULL != args[i])
	{
		if(strcmp(args[i],"|") == 0)
		{
			pipeCmd->flag = 1;
			pipeCmd->cmd1 = &args[0];
			args[i] = NULL;
			pipeCmd->cmd2 = &args[i+1];
			return pipeCmd;
		}
		i++;
	}

	pipeCmd->flag = 0;
	pipeCmd->cmd1 = NULL;
	pipeCmd->cmd2 = NULL;

	return pipeCmd;
}

/******************************************************************************
 * Execute the command
 * ***************************************************************************/
int execute(char** args)
{
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
        else
        {
        	/**
                * After reading user input, the steps are:
                * (1) fork a child process using fork()
                * (2) the child process will invoke execvp()
                * (3) parent will invoke wait() unless command included &
                */

		int should_wait = check_ampersand(args);
		redirect_st* redirect = check_redirection(args);
		pipe_st* pipeCmd = check_pipe(args);
                execute_cmd(args, should_wait, redirect, pipeCmd);
	}

	return 0;
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
	
			if(0 != execute(lastCmd))
				return 1;
			
		}
		else
		{
			if(strcmp(args[0], "exit") == 0)
				should_run = 0;
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

				if(0 != execute(args))
					return 1;
			}
		}
	}

	fclose(fp);
	return 0;
}
