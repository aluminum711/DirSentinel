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
    int recursive; // 1 = recurse subdirectories, 0 = only top-level
    char **excluded_subdirs; // names of immediate subdirectories to skip
    int num_excluded_subdirs;
    char **included_subdirs; // if provided, only descend into these subdirectories
    int num_included_subdirs;
} monitored_path;

typedef struct {
    monitored_path *paths;
    int num_paths;
    int check_interval_seconds;
} configuration;

configuration *load_config(const char *filename);
void free_config(configuration *config);

#endif // CONFIG_H