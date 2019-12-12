#pragma once

#define ferr(s, ...) {fc_ferr(__FILE__, __LINE__, s, ##__VA_ARGS__);}

void fc_ferr(char *f, int l, char *s, ...);

