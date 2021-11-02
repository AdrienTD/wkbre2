// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once
#include <cstddef>

#ifndef _WIN32
void _sleep(int duration);
void Sleep(int duration);
void gets_s(char* destination, size_t len);
template<size_t N> void gets_s(char (&array)[N]) { gets_s(array, N); }
void __debugbreak();
char* itoa(int num, char* buffer, int base);
#endif
