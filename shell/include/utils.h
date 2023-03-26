#ifndef _UTILS_H_
#define _UTILS_H_

#include "siparse.h"

extern bool needToPrintPrompt;

extern volatile size_t indexOfLastForegroundPid;

extern sigset_t withCHLD, withoutCHLD;



void printcommand(command *, int);
void printpipeline(pipeline *, int);
void printparsedline(pipelineseq *);

command * pickfirstcommand(pipelineseq *);


/*
 * Buffer of inputs from stdin to mshell. Works simmilary to ADT queue.
 * startPosition is pointer to start of the next line to get
 * endPosition is pointer next free position
*/
typedef struct buffer{
	int bufferSize;
	char buf[2 * MAX_LINE_LENGTH + 1];
	char * startPosition;
	char * endPosition;
	bool readMore;
} buffer;

/*
 * Setting up buffer so it is possible to work on it
*/
void
setupBuffer(buffer *);


/*
 * Read form stdin to buffer. When buffer is full memmove function will be used to put remainder at start
 * When stdin is closed function sets readMore to false
 * This function shouldn't be used in code, but is used while using getLineFormBuffer(buffer *).
 * 
*/
void 
readToBuffer(buffer *);

/*
 * Function prepares one line from buffer and returns pointer to start of the line
 * wnd also puts '\0' char at the end.
 * When readMore is false and buffer is empty returns NULL
 * If line longer than MAX_LINE_LENGTH is spotted then it would be skipped if is at the end
 * clears the buffer and read 'till newline is spotted
 * If stdin is character device writes prompt and overrides all data in buffer (not sure if is that a good idea)
*/
char * 
getLineFormBuffer(buffer *);



/*
 * Executing parsed line
*/
void
executeLine(pipelineseq *);

/*
 * changes stdin and stdout if needed
*/
void 
changeIO(command *);

/*
 * checks if piplineseq have empty command in it
 * return 0 if such command is in there or pipelineseq is null or one of pipelines is null
*/
int
checkPipelineseq(pipelineseq *);

/*
 * check if pipeline have empty command in it
 * return 0 if such command is in there or pipeline is null
 * return number of commands in pipeline in other situaction
*/
int
checkPipeline(pipeline *);

/*
 * Creates Argv table from command
 * unfortunately it uses malloc ):
*/
char **
createArgv(command *);

/*
 * Creates needed pipes for one pipeline and returns 0 on sucess
 * Returns 1 when something bad happend
*/
int
createAndRunPipeline(pipeline *);

/*
 * run command with buildin check
 * if command isn't buildin forks
 * allows to run in background
 * returns 0 on succes and 1 on fail
*/
int
runCommandWithBuiltins(command *, int);

/*
 * run command without buidin check and don't use fork
 * exits with 0 on succes and 1 on fail
*/
int
runCommand(command *);


/*
 * check if process with given pid was in foreground and remove it
 * returns 1 if it was there and removal was succesful or 0 if process was in background
*/
int
wasInForeground(pid_t);

/*
 * add process to the foreground
*/
void
addToForeground(pid_t);


/*
 * handle exit of child 
 * if child was in background prepare message to store and if 
 * it wasn't decrement foreground counter
*/
void
sigchldHandler();


/*
 * save child pid and exit status to the buffer
*/
void
addMessage(int, int);


/*
 * create and print messages from buffer
*/
void
printMessages();

#endif /* !_UTILS_H_ */
