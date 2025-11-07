#include "config.h"
#include "cJSON.h"
#include "logger.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

configuration *load_config(const char *filename)
{
    char path[MAX_PATH];
    get_executable_path(path, sizeof(path));
    strcat(path, filename);

    char log_buffer[512];
    sprintf(log_buffer, "Attempting to load config from: %s", path);
    log_message(log_buffer);

    FILE *file = fopen(path, "r");
    if (!file)
    {
        log_message("Error: Cannot open config file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(length + 1);
    fread(buffer, 1, length, file);
    fclose(file);

    buffer[length] = '\0';

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (!json)
    {
        log_message("Error: Cannot parse config file");
        return NULL;
    }

    configuration *config = (configuration *)malloc(sizeof(configuration));
    cJSON *interval_json = cJSON_GetObjectItem(json, "check_interval_seconds");
    if (cJSON_IsNumber(interval_json))
    {
        config->check_interval_seconds = interval_json->valueint;
    }
    else
    {
        config->check_interval_seconds = 60; // 默认值
    }

    cJSON *policies = cJSON_GetObjectItem(json, "policies");
    config->num_paths = cJSON_GetArraySize(policies);
    config->paths = (monitored_path *)malloc(config->num_paths * sizeof(monitored_path));

    for (int i = 0; i < config->num_paths; i++)
    {
        cJSON *path_item = cJSON_GetArrayItem(policies, i);
        config->paths[i].path = strdup(cJSON_GetObjectItem(path_item, "path")->valuestring);

        cJSON *policy = cJSON_GetObjectItem(path_item, "policy");
        const char *type_str = cJSON_GetObjectItem(policy, "type")->valuestring;
        if (strcmp(type_str, "percentage") == 0)
        {
            config->paths[i].type = POLICY_PERCENTAGE;
        }
        else
        {
            config->paths[i].type = POLICY_SIZE_GB;
        }
        config->paths[i].value = cJSON_GetObjectItem(policy, "value")->valuedouble;

        cJSON *extensions = cJSON_GetObjectItem(path_item, "allowed_extensions");
        config->paths[i].num_extensions = cJSON_GetArraySize(extensions);
        config->paths[i].allowed_extensions = (char **)malloc(config->paths[i].num_extensions * sizeof(char *));

        for (int j = 0; j < config->paths[i].num_extensions; j++)
        {
            config->paths[i].allowed_extensions[j] = strdup(cJSON_GetArrayItem(extensions, j)->valuestring);
        }

        // recursive flag (optional; default true)
        cJSON *recursive = cJSON_GetObjectItem(path_item, "recursive");
        if (cJSON_IsBool(recursive))
        {
            config->paths[i].recursive = cJSON_IsTrue(recursive) ? 1 : 0;
        }
        else
        {
            config->paths[i].recursive = 1;
        }

        // excluded_subdirs (optional)
        cJSON *excluded = cJSON_GetObjectItem(path_item, "excluded_subdirs");
        if (cJSON_IsArray(excluded))
        {
            config->paths[i].num_excluded_subdirs = cJSON_GetArraySize(excluded);
            config->paths[i].excluded_subdirs = (char **)malloc(config->paths[i].num_excluded_subdirs * sizeof(char *));
            for (int j = 0; j < config->paths[i].num_excluded_subdirs; j++)
            {
                config->paths[i].excluded_subdirs[j] = strdup(cJSON_GetArrayItem(excluded, j)->valuestring);
            }
        }
        else
        {
            config->paths[i].excluded_subdirs = NULL;
            config->paths[i].num_excluded_subdirs = 0;
        }

        // included_subdirs (optional)
        cJSON *included = cJSON_GetObjectItem(path_item, "included_subdirs");
        if (cJSON_IsArray(included))
        {
            config->paths[i].num_included_subdirs = cJSON_GetArraySize(included);
            config->paths[i].included_subdirs = (char **)malloc(config->paths[i].num_included_subdirs * sizeof(char *));
            for (int j = 0; j < config->paths[i].num_included_subdirs; j++)
            {
                config->paths[i].included_subdirs[j] = strdup(cJSON_GetArrayItem(included, j)->valuestring);
            }
        }
        else
        {
            config->paths[i].included_subdirs = NULL;
            config->paths[i].num_included_subdirs = 0;
        }
    }

    cJSON_Delete(json);

    log_message("Config loaded successfully. Policies:");
    for (int i = 0; i < config->num_paths; i++) {
        char policy_buffer[1024];
        sprintf(policy_buffer, "  - Path: %s, Policy: %s, Value: %.2f, Extensions: %d",
                config->paths[i].path,
                config->paths[i].type == POLICY_PERCENTAGE ? "percentage" : "size_gb",
                config->paths[i].value,
                config->paths[i].num_extensions);
        log_message(policy_buffer);
    }

    return config;
}

void free_config(configuration *config)
{
    if (!config)
    {
        return;
    }

    for (int i = 0; i < config->num_paths; i++)
    {
        free(config->paths[i].path);
        for (int j = 0; j < config->paths[i].num_extensions; j++)
        {
            free(config->paths[i].allowed_extensions[j]);
        }
        free(config->paths[i].allowed_extensions);
        if (config->paths[i].excluded_subdirs)
        {
            for (int j = 0; j < config->paths[i].num_excluded_subdirs; j++)
            {
                free(config->paths[i].excluded_subdirs[j]);
            }
            free(config->paths[i].excluded_subdirs);
        }
        if (config->paths[i].included_subdirs)
        {
            for (int j = 0; j < config->paths[i].num_included_subdirs; j++)
            {
                free(config->paths[i].included_subdirs[j]);
            }
            free(config->paths[i].included_subdirs);
        }
    }
    free(config->paths);
    free(config);
}