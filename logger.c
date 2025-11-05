#include "logger.h"
#include "utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>

static FILE *log_file = NULL;

void log_init()
{
    char path[MAX_PATH];
    get_executable_path(path, sizeof(path));
    strcat(path, "\\dirsentinel.log");
    log_file = fopen(path, "a");
}

void log_message(const char *format, ...)
{
    if (log_file)
    {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, buffer);
        fflush(log_file);
    }
}

void log_close()
{
    if (log_file)
    {
        fclose(log_file);
    }
}