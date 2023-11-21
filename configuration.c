#include <configuration.h>
#include <stddef.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

typedef enum {DATE_SIZE_ONLY, NO_PARALLEL} long_opt_values;

/*!
 * @brief function display_help displays a brief manual for the program usage
 * @param my_name is the name of the binary file
 * This function is provided with its code, you don't have to implement nor modify it.
 */
void display_help(char *my_name) {
    printf("%s [options] source_dir destination_dir\n", my_name);
    printf("Options: \t-n <processes count>\tnumber of processes for file calculations\n");
    printf("         \t-h display help (this text)\n");
    printf("         \t--date_size_only disables MD5 calculation for files\n");
    printf("         \t--no-parallel disables parallel computing (cancels values of option -n)\n");
}

/*!
 * @brief init_configuration initializes the configuration with default values
 * @param the_config is a pointer to the configuration to be initialized
 */
void init_configuration(configuration_t *the_config) {
    configuration_t default_config={.source="",.destination="",.processes_count=0,.is_parallel=false,.uses_md5=false};

    the_config->uses_md5=default_config.uses_md5;
    the_config->is_parallel=default_config.is_parallel;
    the_config->processes_count=default_config.processes_count;
    strcpy(the_config->source,default_config.source);
    strcpy(the_config->destination,default_config.destination);
}

/*!
 * @brief set_configuration updates a configuration based on options and parameters passed to the program CLI
 * @param the_config is a pointer to the configuration to update
 * @param argc is the number of arguments to be processed
 * @param argv is an array of strings with the program parameters
 * @return -1 if configuration cannot succeed, 0 when ok
 */
int set_configuration(configuration_t *the_config, int argc, char *argv[]) {
    bool checkSdOption =true;
    // Copy source_dir and destination_dir in the_config
    strcpy(the_config->source,argv[1]);
    strcpy(the_config->destination,argv[2]);

    // Check source_dir , destination_dir existence
    if(!strcmp(the_config->source,"") || !strcmp(the_config->destination,"")){
        checkSdOption=false;
    }
    if(checkSdOption) {
        int opt = 0;
        struct option my_opts[] = {
                {.name="date-size-only", .has_arg=1, .flag=0, .val='d'},
                {.name="no-parallel", .has_arg=1, .flag=0, .val='p'},
                {.name="verbose", .has_arg=0, .flag=0, .val='c'},
                {.name=0, .has_arg=0, .flag=0, .val=0}, // last element must be zero
        };

        while ((opt = (getopt_long(argc, argv, "n::", my_opts, NULL))) != -1) {
            switch (opt) {
                case 'd':
                    the_config->uses_md5=optarg;
                    break;
                case 'p':
                    the_config->is_parallel=optarg;
                    break;
                case 'n':
                    break;
            }
        }
        return 0;
    }
    else{
        return -1;
    }
}
