#include <log.h>
#include <cstdio>
#include <ctime>

static FILE *log_file = stderr;

void log_set_file(FILE *file)
{
    log_file = file;
}

void log_printf(const char *fmt, ...)
{
    va_list ap;
    time_t time_log = time(NULL);
    struct tm* tm_log = localtime(&time_log);

    va_start(ap, fmt);
    fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d]\t", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday, tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
    fputc('\n', log_file);
    fflush(log_file);
}
