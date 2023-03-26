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

bool needToPrintPrompt;
sigset_t withCHLD, withoutCHLD;


int
main(int argc, char *argv[])
{
	struct stat sb; 
	fstat(0,&sb);
	needToPrintPrompt =  ((sb.st_mode & S_IFMT) == S_IFCHR);
	pipelineseq * ln;
	buffer B;
	setupBuffer(&B);
	sigemptyset(&withCHLD);
	sigaddset(&withCHLD, SIGCHLD);	

	struct sigaction chldAction, intAction;

	chldAction.sa_handler = sigchldHandler;
	sigemptyset (&chldAction.sa_mask);
	chldAction.sa_flags = 0;

	intAction.sa_handler = SIG_IGN;
	sigemptyset (&intAction.sa_mask);
	intAction.sa_flags = 0;

	sigaction (SIGCHLD, &chldAction, NULL);
	sigaction (SIGINT, &intAction, NULL);
	while (true){
			char * lineToParse = getLineFormBuffer(&B);
			if (lineToParse == NULL)
					break;
			if (strcmp(lineToParse,"") == 0)
					continue;
			ln = parseline(lineToParse);
			executeLine(ln);
			fflush(stdout);
	}
	write(0,"\n",1);
}

