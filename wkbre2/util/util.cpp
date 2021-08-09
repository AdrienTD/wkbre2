#include "util.h"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void fc_ferr(char *f, int l, char *s, ...)
{
	char t[512]; va_list v;
	va_start(v, s);
	_vsnprintf(t, 511, s, v); t[511] = 0;
	char msg[600];
	sprintf_s(msg, "Error: %s\nLook at: %s, Line %i\n", t, f, l);
	puts(msg);
	MessageBoxA(NULL, msg, "Fatal Error", 16);
	fflush(stdout); abort(); exit(-2);
}