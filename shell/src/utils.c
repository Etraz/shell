#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"

void 
printcommand(command *pcmd, int k)
{
	int flags;

	printf("\tCOMMAND %d\n",k);
	if (pcmd==NULL){
		printf("\t\t(NULL)\n");
		return;
	}

	printf("\t\targv=:");
	argseq * argseq = pcmd->args;
	do{
		printf("%s:", argseq->arg);
		argseq= argseq->next;
	}while(argseq!=pcmd->args);

	printf("\n\t\tredirections=:");
	redirseq * redirs = pcmd->redirs;
	if (redirs){
		do{	
			flags = redirs->r->flags;
			printf("(%s,%s):",redirs->r->filename,IS_RIN(flags)?"<": IS_ROUT(flags) ?">": IS_RAPPEND(flags)?">>":"??");
			redirs= redirs->next;
		} while (redirs!=pcmd->redirs);	
	}

	printf("\n");
}

void
printpipeline(pipeline * p, int k)
{
	int c;
	command ** pcmd;

	commandseq * commands= p->commands;

	printf("PIPELINE %d\n",k);
	
	if (commands==NULL){
		printf("\t(NULL)\n");
		return;
	}
	c=0;
	do{
		printcommand(commands->com,++c);
		commands= commands->next;
	}while (commands!=p->commands);

	printf("Totally %d commands in pipeline %d.\n",c,k);
	printf("Pipeline %sin background.\n", (p->flags & INBACKGROUND) ? "" : "NOT ");
}

void
printparsedline(pipelineseq * ln)
{
	int c;
	pipelineseq * ps = ln;

	if (!ln){
		printf("%s\n",SYNTAX_ERROR_STR);
		return;
	}
	c=0;

	do{
		printpipeline(ps->pipeline,++c);
		ps= ps->next;
	} while(ps!=ln);

	printf("Totally %d pipelines.",c);
	printf("\n");
}

command *
pickfirstcommand(pipelineseq * ppls)
{
	if ((ppls==NULL)
		|| (ppls->pipeline==NULL)
		|| (ppls->pipeline->commands==NULL)
		|| (ppls->pipeline->commands->com==NULL))	return NULL;
	
	return ppls->pipeline->commands->com;
}

void 
setupBuffer(buffer * B){
	B->bufferSize = 2 * MAX_LINE_LENGTH;
	B->startPosition = B->buf;
	B->endPosition = B->buf;
	*(B->endPosition) = '\0';
	B->readMore = true;
}

void
readToBuffer(buffer * B){
	/*
	 * If stdin is character device;
	*/
	if (needToPrintPrompt){
			sigprocmask(SIG_BLOCK, &withCHLD, &withoutCHLD);	
			printMessages();
			write(0,PROMPT_STR,strlen(PROMPT_STR));
			B->startPosition = B->buf;
			B->endPosition = B->buf;
			sigprocmask(SIG_SETMASK, &withoutCHLD, NULL);	
	}
	/*
	* B->buf is full so we need to move the memory 
	*/
	if ((B->buf) + B->bufferSize == B->endPosition){
			memmove(B->buf, B->startPosition, sizeof(char) * (B->endPosition - B->startPosition));
			B->endPosition = B->buf + (B->endPosition - B->startPosition);
			B->startPosition = B->buf;
	}
	ssize_t numberOfReadChars;
	do{
		numberOfReadChars = read(0, B->endPosition, B->bufferSize - (B->endPosition - B->buf));
	} while (numberOfReadChars == -1);
	/*
	* stdin is closed putting '\n' because we won't read anything more
	*/
	
	if (numberOfReadChars == 0){
			B->readMore = false;
			*(B->endPosition) = '\n';
	}
	B->endPosition += numberOfReadChars;
	*(B->endPosition) = '\0';
}

char *
getLineFormBuffer(buffer * B){
	/*
	* finding first occurance of '\n'
	*/
	char * currentEnd = strchr(B->startPosition,'\n');
	/*
	* if there is no '\n' to be found we need to get more data to buffer or return NULL
	*/
	while (B->readMore && currentEnd == NULL){
			if (B->endPosition - B->startPosition > MAX_LINE_LENGTH){
					do {
							B->startPosition = B->buf;
							B->endPosition = B->buf;
							readToBuffer(B);
							currentEnd = strchr(B->startPosition, '\n');
					} while (B->readMore && currentEnd == NULL);
					if (currentEnd == NULL)
							return NULL;
					B->startPosition = currentEnd + 1;
					continue;
			}
			readToBuffer(B);
			currentEnd = strchr(B->startPosition, '\n');
	}
	if (currentEnd == NULL)
			return NULL;

	*currentEnd = '\0';
	char * toReturn = B->startPosition;
	B->startPosition = currentEnd + 1;
	return toReturn;
}

void
executeLine(pipelineseq * ln){
	command *com;
	if (!checkPipelineseq(ln)){
			fprintf(stderr,"%s\n",SYNTAX_ERROR_STR);
			return;
	}
	pipelineseq * temp = ln;
	do {
		createAndRunPipeline(temp->pipeline);
		temp = temp->next;
	} while (temp != ln);
	
	
}

int
createAndRunPipeline(pipeline * pipel){
	int numberOfCommands = checkPipeline(pipel);
	if (numberOfCommands == 1)
			return runCommandWithBuiltins(pipel->commands->com, pipel->flags);
	sigprocmask(SIG_BLOCK, &withCHLD, &withoutCHLD);	
	commandseq * curComSeq = pipel->commands;
	int lastInput = 0;
	for (int i = 1; i < numberOfCommands; ++i){
			int fd[2];
			int pipeReturnValue = pipe(fd);
			int forkReturnValue = fork();
			if (pipeReturnValue < 0 || forkReturnValue < 0){
					perror(NULL);
					return 1;
			}
			//Parent
			if (forkReturnValue){
					if (pipel->flags != INBACKGROUND)
							addToForeground(forkReturnValue);
					if(lastInput){
							close(lastInput);
					}
					lastInput = fd[0];
					close(fd[1]);
					curComSeq = curComSeq->next;
			}
			//Child
			else {
					struct sigaction intAction;

					intAction.sa_handler = SIG_DFL;
					sigemptyset (&intAction.sa_mask);
					intAction.sa_flags = 0;
					sigaction (SIGINT, &intAction, NULL);
					if (pipel->flags == INBACKGROUND)
							setpgid(0,0);
					if(lastInput){
							close(0);
							dup2(lastInput,0);
							close(lastInput);
					}
					close(1);
					dup2(fd[1],1);
					close(fd[1]);
					close(fd[0]);
					runCommand(curComSeq->com);
			}
	}
	int forkReturnValue = fork();
	if (forkReturnValue ==-1){
			perror(NULL);
			return 1;
	}
	//Parent
	if (forkReturnValue){
			if (pipel->flags != INBACKGROUND)
					addToForeground(forkReturnValue);
			close(lastInput);
			while (indexOfLastForegroundPid){
					sigsuspend(&withoutCHLD);
			}
	}
	//Child
	else {
			struct sigaction defAction;

			defAction.sa_handler = SIG_DFL;
			sigemptyset (&defAction.sa_mask);
			defAction.sa_flags = 0;
			sigaction (SIGINT, &defAction, NULL);
			sigaction (SIGCHLD, &defAction, NULL);
			if (pipel->flags == INBACKGROUND)
					setpgid(0,0);
			close(0);
			dup2(lastInput,0);
			close(lastInput);
			runCommand(curComSeq->com);
	}
	sigprocmask(SIG_SETMASK, &withoutCHLD, NULL);	
	return 0;
}

int
runCommandWithBuiltins(command * com, int pipelineFlags){
	if (com == NULL)
			return 1;
	char *commandName = com->args->arg;
	char ** parsedArgs = createArgv(com);
	if (checkForBuiltins(commandName, parsedArgs)){
			free(parsedArgs);
			return 0;
	}
	sigprocmask(SIG_BLOCK, &withCHLD, &withoutCHLD);	
	int forkReturnValue = fork();
	if(forkReturnValue == -1){
			perror(NULL);
			free(parsedArgs);
			return 1;
	}
	if (forkReturnValue){
			int status;
			free(parsedArgs);
			if (pipelineFlags != INBACKGROUND)
					addToForeground(forkReturnValue);
			while (indexOfLastForegroundPid){
					sigsuspend(&withoutCHLD);
			}
	}
	else{
			if (pipelineFlags == INBACKGROUND)
					setpgid(0,0);
			changeIO(com);
			errno = 0;
			struct sigaction defAction;
			defAction.sa_handler = SIG_DFL;
			sigemptyset (&defAction.sa_mask);
			defAction.sa_flags = 0;
			sigaction (SIGINT, &defAction, NULL);
			sigaction (SIGCHLD, &defAction, NULL);
			if (execvp(commandName, parsedArgs) == -1){
					free(parsedArgs);
					if(errno == ENOENT){
							fprintf(stderr,"%s: no such file or directory\n", commandName);
							exit(EXEC_FAILURE);
					}
					else if(errno == EACCES){
							fprintf(stderr,"%s: permission denied\n", commandName);
							exit(EXEC_FAILURE);
					}
					else{
							fprintf(stderr,"%s: exec error\n", commandName);
							exit(EXEC_FAILURE);
					}
			}
			exit(0);
	}
	sigprocmask(SIG_SETMASK, &withoutCHLD, NULL);	
}

int
runCommand(command * com){
	if (com == NULL)
			exit(1);
	char *commandName = com->args->arg;
	char ** parsedArgs = createArgv(com);
	changeIO(com);
	errno = 0;
	if (execvp(commandName, parsedArgs) == -1){
			free(parsedArgs);
			if(errno == ENOENT){
					fprintf(stderr,"%s: no such file or directory\n", commandName);
					exit(EXEC_FAILURE);
			}
			else if(errno == EACCES){
					fprintf(stderr,"%s: permission denied\n", commandName);
					exit(EXEC_FAILURE);
			}
			else{
					fprintf(stderr,"%s: exec error\n", commandName);
					exit(EXEC_FAILURE);
			}
	}
	exit(0);
}

void
changeIO(command *com){
	if (com->redirs == NULL){
			return;
	}
	redirseq * p = com ->redirs;
	redirseq * temp = p;
	do {
			errno = 0;
			bool t = false;
			if(IS_RIN(temp->r->flags)){
					t = (freopen(temp->r->filename,"r", stdin) == NULL);
			}
			if(IS_ROUT(temp->r->flags)){
					t = (freopen(temp->r->filename,"w", stdout) == NULL);
			}
			if(IS_RAPPEND(temp->r->flags)){
					t = (freopen(temp->r->filename,"a", stdout) == NULL);
			}
			if (t){
					if(errno == ENOENT){
							fprintf(stderr,"%s: no such file or directory\n", temp->r->filename);
							exit(EXEC_FAILURE);
					}
					else if(errno == EACCES){
							fprintf(stderr,"%s: permission denied\n", temp->r->filename);
							exit(EXEC_FAILURE);
					}
			}
			temp = temp->next;
	} while(p != temp);
}

int
checkPipelineseq(pipelineseq * pipeseq){
	if (pipeseq == NULL)
			return 0;
	pipelineseq * temp = pipeseq;
	do{
			if (checkPipeline(temp->pipeline) == 0)
					return 0;
			temp = temp->next;
	} while (pipeseq != temp);
	return 1;
}

int
checkPipeline(pipeline * pipe){
	if (pipe == NULL || pipe->commands == NULL)
			return 0;
	if (pipe->commands->next == pipe->commands)
			return 1;
	commandseq * p = pipe->commands;
	commandseq * temp = p;
	int counter = 0;
	do{
		if (p->com == NULL || p->com->args == NULL || p->com->args->arg == NULL || p->com->args->arg[0] == '\0')
				return 0;
		temp = temp->next;
		++counter;
	} while (p != temp);
	return counter;
}

char **
createArgv(command * com){
	int counter = 0;
	argseq * p = com ->args;
	argseq * temp = p->next;
	while(p != temp){
			temp = temp->next;
			++counter;
	}
	char ** parsedArgs = malloc((counter + 2)*sizeof(char*));
	int i = 0;
	p = com->args;
	temp = p;
	do {
			parsedArgs[i] = temp->arg;
			temp = temp->next;
			++i;
	} while(p != temp);
	parsedArgs[i] = NULL;
	return parsedArgs;
}

static pid_t ForegroundPids[MAX_LINE_LENGTH / 2];

volatile size_t indexOfLastForegroundPid = 0;

void
addToForeground(pid_t pid){
	ForegroundPids[indexOfLastForegroundPid] = pid;
	++indexOfLastForegroundPid;
}

int
wasInForeground(pid_t pid){
	for (size_t i = 0; i < indexOfLastForegroundPid; ++i){
			if (ForegroundPids[i] == pid){
					--indexOfLastForegroundPid;
					ForegroundPids[i] = ForegroundPids[indexOfLastForegroundPid];
					return 1;
			}
	}
	return 0;
}


void
sigchldHandler(){
	pid_t pid;
	int wstatus;
	while ((pid = waitpid(-1,&wstatus,WNOHANG)) > 0){
			if (wasInForeground(pid) == 0 && needToPrintPrompt){
					addMessage((int)pid, wstatus);
			}
	}
}

int messageBuffer[MAX_NUMBER_OF_EXIT_MESSAGES][2];
volatile size_t indexOfLastMessage = 0;

void
addMessage(int pid, int wstatus){
	if (indexOfLastMessage < MAX_NUMBER_OF_EXIT_MESSAGES){
			messageBuffer[indexOfLastMessage][0] = pid;
			messageBuffer[indexOfLastMessage][1] = wstatus;
			++indexOfLastMessage;
	}
}

void
printMessages(){
	for (int i = 0; i < indexOfLastMessage; ++i){
			int pid = messageBuffer[indexOfLastMessage][0];
			int wstatus = messageBuffer[indexOfLastMessage][1];
			char message[70];
			if (WIFEXITED(wstatus))
					sprintf(message,"Background process %d terminated. (exited with status %d)\n", pid, WEXITSTATUS(wstatus));
			else if(WIFSIGNALED(wstatus))
					sprintf(message,"Background process %d terminated. (killed by signal %d)\n", pid, WTERMSIG(wstatus));
			else
					sprintf(message,"Background process %d terminated.\n", pid);
			write(0, message, strlen(message));
	}
	indexOfLastMessage = 0;
}
