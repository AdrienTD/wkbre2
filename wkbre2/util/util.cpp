#include "util.h"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>

void fc_ferr(char *f, int l, char *s, ...)
{
	char t[512]; va_list v;
	va_start(v, s);
	_vsnprintf(t, 511, s, v); t[511] = 0;
	printf("Error: %s\nLook at: %s, Line %i\n", t, f, l);
	fflush(stdout); abort(); exit(-2);
}