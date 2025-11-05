#include "service.h"
#include <string.h>

int main(int argc, char *argv[])
{
    // If the command is to install the DirSentinelService
    if (argc > 1 && strcmp(argv[1], "install") == 0)
    {
        service_install();
        return 0;
    }

    // If the command is to uninstall the DirSentinelService
    if (argc > 1 && strcmp(argv[1], "uninstall") == 0)
    {
        service_uninstall();
        return 0;
    }

    // If the command is to run the DirSentinelService
    service_run();

    return 0;
}