#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
    POLICY_PERCENTAGE,
    POLICY_SIZE_GB
} policy_type;

typedef struct {
    char *path;
    policy_type type;
    double value;
    char **allowed_extensions;
    int num_extensions;
} monitored_path;

typedef struct {
    monitored_path *paths;
    int num_paths;
    int check_interval_seconds;
} configuration;

configuration *load_config(const char *filename);
void free_config(configuration *config);

#endif // CONFIG_H