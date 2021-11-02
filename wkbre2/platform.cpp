// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include <cstdio>
#include <charconv>
#include <chrono>
#include <thread>
#include <utility>

#ifndef _WIN32 // temp
void _sleep(int duration) {std::this_thread::sleep_for(std::chrono::milliseconds(duration));}
void Sleep(int duration) {_sleep(duration);}
void gets_s(char* destination, size_t len) {fgets(destination, len, stdin);}
void __debugbreak() {}
char* itoa(int num, char* buffer, int base) {std::to_chars(buffer, buffer+10, num, base);}
#endif
