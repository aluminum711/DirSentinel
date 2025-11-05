#include "monitor.h"
#include "config.h"
#include "logger.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>

static volatile int g_monitoring = 0;

static char *format_size(ULONGLONG size, char *buffer)
{
    if (size >= 1024 * 1024 * 1024)
    {
        sprintf(buffer, "%.2f GB", (double)size / (1024 * 1024 * 1024));
    }
    else if (size >= 1024 * 1024)
    {
        sprintf(buffer, "%.2f MB", (double)size / (1024 * 1024));
    }
    else if (size >= 1024)
    {
        sprintf(buffer, "%.2f KB", (double)size / 1024);
    }
    else
    {
        sprintf(buffer, "%llu Bytes", size);
    }
    return buffer;
}

static ULONGLONG get_directory_size(const char *path)
{
    ULARGE_INTEGER total_size;
    total_size.QuadPart = 0;

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = INVALID_HANDLE_VALUE;

    char search_path[MAX_PATH];
    sprintf(search_path, "%s\\*", path);

    find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    do
    {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0)
            {
                char sub_dir_path[MAX_PATH];
                sprintf(sub_dir_path, "%s\\%s", path, find_data.cFileName);
                total_size.QuadPart += get_directory_size(sub_dir_path);
            }
        }
        else
        {
            ULARGE_INTEGER file_size;
            file_size.LowPart = find_data.nFileSizeLow;
            file_size.HighPart = find_data.nFileSizeHigh;
            total_size.QuadPart += file_size.QuadPart;
        }
    } while (FindNextFile(find_handle, &find_data) != 0);

    FindClose(find_handle);

    return total_size.QuadPart;
}

static int check_policy(monitored_path *path)
{
    ULONGLONG dir_size = get_directory_size(path->path);
    char dir_size_str[32], total_bytes_str[32], value_str[32], threshold_str[32];

    if (path->type == POLICY_PERCENTAGE)
    {
        ULARGE_INTEGER free_bytes, total_bytes, total_free_bytes;
        if (GetDiskFreeSpaceEx(path->path, &free_bytes, &total_bytes, &total_free_bytes))
        {
            double percentage = ((double)dir_size / total_bytes.QuadPart) * 100;
            ULONGLONG threshold_bytes = (ULONGLONG)((path->value / 100.0) * total_bytes.QuadPart);
            log_message("Path: %s, Usage: %s / %s (%.2f%%), Policy: > %.2f%% (%s)",
                        path->path, format_size(dir_size, dir_size_str),
                        format_size(total_bytes.QuadPart, total_bytes_str), percentage, path->value,
                        format_size(threshold_bytes, threshold_str));
            return percentage > path->value;
        }
    }
    else if (path->type == POLICY_SIZE_GB)
    {
        ULONGLONG size_bytes = (ULONGLONG)(path->value * 1024 * 1024 * 1024);
        log_message("Path: %s, Size: %s, Policy: > %s",
                    path->path, format_size(dir_size, dir_size_str),
                    format_size(size_bytes, value_str));
        return dir_size > size_bytes;
    }

    return 0;
}

static void find_oldest_file_recursive(const char *path, monitored_path *policy, char *oldest_file_path, FILETIME *oldest_time)
{
    WIN32_FIND_DATA find_data;
    HANDLE find_handle = INVALID_HANDLE_VALUE;

    char search_path[MAX_PATH];
    sprintf(search_path, "%s\\*", path);

    find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0)
            {
                char sub_dir_path[MAX_PATH];
                sprintf(sub_dir_path, "%s\\%s", path, find_data.cFileName);
                find_oldest_file_recursive(sub_dir_path, policy, oldest_file_path, oldest_time);
            }
        }
        else
        {
            const char *ext = strrchr(find_data.cFileName, '.');
            if (ext)
            {
                for (int i = 0; i < policy->num_extensions; i++)
                {
                    if (strcmp(ext, policy->allowed_extensions[i]) == 0)
                    {
                        if (CompareFileTime(&find_data.ftLastWriteTime, oldest_time) < 0)
                        {
                            *oldest_time = find_data.ftLastWriteTime;
                            sprintf(oldest_file_path, "%s\\%s", path, find_data.cFileName);
                        }
                        break;
                    }
                }
            }
        }
    } while (FindNextFile(find_handle, &find_data) != 0);

    FindClose(find_handle);
}

static void apply_deletion_policy(monitored_path *path)
{
    while (check_policy(path))
    {
        char oldest_file_path[MAX_PATH] = {0};
        FILETIME oldest_time = {0xFFFFFFFF, 0xFFFFFFFF};

        find_oldest_file_recursive(path->path, path, oldest_file_path, &oldest_time);

        if (oldest_file_path[0] != '\0')
        {
            if (DeleteFile(oldest_file_path))
            {
                char log_msg[MAX_PATH + 50];
                sprintf(log_msg, "Deleted file: %s", oldest_file_path);
                log_message(log_msg);
            }
            else
            {
                char log_msg[MAX_PATH + 50];
                sprintf(log_msg, "Error: DeleteFile failed for %s", oldest_file_path);
                log_message(log_msg);
                break; // Exit loop if deletion fails
            }
        }
        else
        {
            log_message("No more files to delete.");
            break; // Exit loop if no more files to delete
        }
    }
}

void start_monitoring()
{
    g_monitoring = 1;
    while (g_monitoring)
    {
        // Load configuration
        configuration *config = load_config("config.json");
        if (!config)
        {
            log_message("Error: Failed to load configuration");
            Sleep(config->check_interval_seconds * 1000); // Wait 10 seconds before retrying
            continue;
        }

        log_message("Configuration loaded, starting policy check");

        // Iterate through monitored paths
        for (int i = 0; i < config->num_paths; i++)
        {
            monitored_path *path = &config->paths[i];
            char log_msg[MAX_PATH + 50];
            sprintf(log_msg, "Checking path: %s", path->path);
            log_message(log_msg);

            if (check_policy(path))
            {
                sprintf(log_msg, "Policy exceeded for path: %s", path->path);
                log_message(log_msg);
                apply_deletion_policy(path);
            }
        }

        // Free configuration
        free_config(config);

        log_message("Policy check finished, sleeping");

        // Wait for a while before next check
        Sleep(config->check_interval_seconds * 1000); // 10 seconds
    }
}

void stop_monitoring()
{
    g_monitoring = 0;
}