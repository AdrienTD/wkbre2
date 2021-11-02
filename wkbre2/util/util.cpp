// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "util.h"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

void fc_ferr(const char *f, int l, const char *s, ...)
{
	char t[512]; va_list v;
	va_start(v, s);
	vsnprintf(t, sizeof(t), s, v);
	char msg[600];
	snprintf(msg, sizeof(msg), "Error: %s\nLook at: %s, Line %i\n", t, f, l);
	puts(msg);
#ifdef _WIN32
	MessageBoxA(NULL, msg, "Fatal Error", 16);
#endif
	fflush(stdout); abort(); exit(-2);
}