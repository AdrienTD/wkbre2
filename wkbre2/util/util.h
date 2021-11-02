// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#define ferr(s, ...) {fc_ferr(__FILE__, __LINE__, s, ##__VA_ARGS__);}

void fc_ferr(const char *f, int l, const char *s, ...);

