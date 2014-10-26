#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32
	#include <getopt.h>
#else
	#ifndef _GETOPT_
	#define _GETOPT_

	#include <stdio.h>                  /* for EOF */ 
	#include <string.h>                 /* for strchr() */ 

	char *optarg = NULL;    /* pointer to the start of the option argument  */ 
	extern int   optind;       /* number of the next argv[] to be evaluated    */ 
	//extern int   opterr;       /* non-zero if a question mark should be returned */

	int getopt(int argc, char *argv[], char *opstring); 
	#endif //_GETOPT_
#endif //WIN32

#ifdef __cplusplus
}
#endif

struct option
{
#if 1//defined (__STDC__) && __STDC__
	const char *name;
#else
	char *name;
#endif
	/* has_arg can't be an enum because some compilers complain about
	type mismatches in all the code that assumes it is an int.  */
	int has_arg;
	int *flag;
	int val;
};

/* Names for the values of the `has_arg' field of `struct option'.  */

#define	no_argument		0
#define required_argument	1
#define optional_argument	2