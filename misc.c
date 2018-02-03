#include <stdlib.h>
#include <stdio.h>

void perror_exit(const char * msg)
{
	perror(msg);
	exit(0);
}

void puts_exit(const char * msg)
{
	puts(msg);
	exit(0);
}