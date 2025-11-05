#include "utils.h"
#include <windows.h>
#include <string.h>

void get_executable_path(char *path, int size)
{
    GetModuleFileName(NULL, path, size);
    char *last_slash = strrchr(path, '\\');
    if (last_slash)
    {
        *(last_slash + 1) = '\0';
    }
}