#pragma once

#include <stdint.h>
#include <stdbool.h>
#define STR_MAX 1024

typedef struct {
    char source[STR_MAX];
    char destination[STR_MAX];
    uint8_t processes_count;
    bool is_parallel;
    bool uses_md5;
    bool verbose;
    bool dry_run;
} configuration_t;

void init_configuration(configuration_t *the_config);
int set_configuration(configuration_t *the_config, int argc, char *argv[]);