#ifndef _BUILTINS_H_
#define _BUILTINS_H_

#define BUILTIN_ERROR 2

typedef struct {
	char* name;
	int (*fun)(char**); 
} builtin_pair;

extern builtin_pair builtins_table[];

/*
 * checks for use of builtins
 * returns 1 if command is building and uses it
*/
int
checkForBuiltins(char*, char **);

#endif /* !_BUILTINS_H_ */
