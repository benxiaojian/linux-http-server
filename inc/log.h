#pragma once

#include <cstdio>
#include <cstdarg>


void log_set_file(FILE *file);

void log_printf(const char *fmt, ...);


#define LOG( ... ) log_printf(__VA_ARGS__)
