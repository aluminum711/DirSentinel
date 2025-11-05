#include "service.h"
#include "monitor.h"
#include "logger.h"
#include <windows.h>
#include <stdio.h>

#define SERVICE_NAME "DirSentinelService"

SERVICE_STATUS_HANDLE g_service_status_handle;
SERVICE_STATUS g_service_status;

VOID WINAPI service_main(DWORD argc, LPTSTR *argv);
VOID WINAPI service_ctrl_handler(DWORD ctrl_code);

void service_install()
{
    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!sc_manager)
    {
        printf("Error: OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    char path[MAX_PATH];
    if (!GetModuleFileName(NULL, path, MAX_PATH))
    {
        printf("Error: GetModuleFileName failed (%d)\n", GetLastError());
        CloseServiceHandle(sc_manager);
        return;
    }

    SC_HANDLE sc_service = CreateService(
        sc_manager,
        SERVICE_NAME,
        SERVICE_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        path,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL);

    if (!sc_service)
    {
        printf("Error: CreateService failed (%d)\n", GetLastError());
    }
    else
    {
        printf("Service installed successfully.\n");
        CloseServiceHandle(sc_service);
    }

    CloseServiceHandle(sc_manager);
}

void service_uninstall()
{
    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!sc_manager)
    {
        printf("Error: OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    SC_HANDLE sc_service = OpenService(sc_manager, SERVICE_NAME, DELETE);
    if (!sc_service)
    {
        printf("Error: OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(sc_manager);
        return;
    }

    if (!DeleteService(sc_service))
    {
        printf("Error: DeleteService failed (%d)\n", GetLastError());
    }
    else
    {
        printf("Service uninstalled successfully.\n");
    }

    CloseServiceHandle(sc_service);
    CloseServiceHandle(sc_manager);
}

void service_run()
{
    SERVICE_TABLE_ENTRY service_table[] =
    {
        { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)service_main },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(service_table))
    {
        printf("Error: StartServiceCtrlDispatcher failed (%d)\n", GetLastError());
    }
}

VOID WINAPI service_main(DWORD argc, LPTSTR *argv)
{
    g_service_status_handle = RegisterServiceCtrlHandler(SERVICE_NAME, service_ctrl_handler);
    if (!g_service_status_handle)
    {
        log_message("Error: RegisterServiceCtrlHandler failed");
        return;
    }

    g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_service_status.dwCurrentState = SERVICE_START_PENDING;
    g_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_service_status.dwWin32ExitCode = 0;
    g_service_status.dwServiceSpecificExitCode = 0;
    g_service_status.dwCheckPoint = 0;
    g_service_status.dwWaitHint = 0;

    SetServiceStatus(g_service_status_handle, &g_service_status);

    log_init();
    log_message("Service starting");

    g_service_status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_service_status_handle, &g_service_status);

    // Start the monitoring loop
    start_monitoring();

    log_message("Service stopping");
    log_close();

    g_service_status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_service_status_handle, &g_service_status);
}

VOID WINAPI service_ctrl_handler(DWORD ctrl_code)
{
    switch (ctrl_code)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        g_service_status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_service_status_handle, &g_service_status);
        log_message("Service stop request received");
        // Stop the monitoring loop
        stop_monitoring();
        break;
    default:
        break;
    }
}