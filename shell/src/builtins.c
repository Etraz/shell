#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <signal.h>

#include "builtins.h"

void error(char *);
int echo(char*[]);
int undefined(char *[]);
int lexit(char *[]);
int lcd(char *[]);
int lkill(char *[]);
int lls(char * []);




builtin_pair builtins_table[]={
	{"exit",	&lexit},
	{"lecho",	&echo},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,NULL}
};

void error(char * name){
	fprintf(stderr, "Builtin %s error.\n", name);
}

int 
echo( char * argv[])
{
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int 
undefined(char * argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}

int
lexit( char* argv[]){
	if(argv[1] != NULL){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	exit(EXIT_SUCCESS);
}

int
lcd(char* argv[]){
	if(argv[1] == NULL){
			if(!chdir(getenv("HOME"))){
					return 0;
			}
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	if(argv[2] != NULL || chdir(argv[1])){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	return 0;
}

int
lls(char* argv[]){
	if(argv[1] != NULL){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	char  cwdName[PATH_MAX];
	getcwd(cwdName, PATH_MAX);
	DIR* cwd = opendir(cwdName);
	struct dirent *temp = readdir(cwd);
	while(temp != NULL){
			if(temp->d_name[0] != '.')
					printf("%s\n",temp->d_name);
			temp = readdir(cwd);
	}
	closedir(cwd);
	return 0;
}

int
lkill (char* argv[]){
	if (argv[1] == NULL){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	if (argv[2] == NULL){
			char *temp;
			errno = 0;
			int pid = strtol(argv[1],&temp, 10);
			if (temp == argv[1] || *temp != '\0' || ((pid == LONG_MIN || pid == LONG_MAX) && errno == ERANGE)){
					error(argv[0]);
					return BUILTIN_ERROR;
			}
			if (kill(pid,SIGINT) != 0){
					error(argv[0]);
					return BUILTIN_ERROR;
			}
			return 0;
	}
	if (argv[3] != NULL || argv[1][0] != '-'){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	char *temp;
	errno = 0;
	int pid = strtol(argv[2],&temp, 10);
	if (temp == argv[2] || *temp != '\0' || ((pid == LONG_MIN || pid == LONG_MAX) && errno == ERANGE)){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	errno = 0;
	int signal = strtol(argv[1] + 1,&temp, 10);
	if (temp == (argv[1] + 1)|| *temp != '\0' || ((pid == LONG_MIN || pid == LONG_MAX) && errno == ERANGE)){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	if (kill(pid,signal) != 0){
			error(argv[0]);
			return BUILTIN_ERROR;
	}
	return 0;
}

int
checkForBuiltins(char* commandName, char ** Argv){
	for (int i = 0; builtins_table[i].name != NULL; ++i){
			if(!strcmp(commandName, builtins_table[i].name)){
					(*builtins_table[i].fun)(Argv);
					return 1;
			}
	}
	return 0;
}