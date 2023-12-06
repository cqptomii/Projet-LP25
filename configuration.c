#include <configuration.h>
#include <stddef.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
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
    printf("         \t--verbose enable mode verbose\n");
    printf("         \t--dry-run enable mode dry run \n");
}

/*!
 * @brief init_configuration initializes the configuration with default values
 * @param the_config is a pointer to the configuration to be initialized
 */
void init_configuration(configuration_t *the_config) {
    configuration_t default_config={.source="",.destination="",.processes_count=1,.is_parallel=false,.uses_md5=false,.verbose=false,.dry_run=false};

    the_config->uses_md5=default_config.uses_md5;
    the_config->is_parallel=default_config.is_parallel;
    the_config->processes_count=default_config.processes_count;
    the_config->dry_run=default_config.dry_run;
    the_config->verbose=default_config.verbose;
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
    // Copy source_dir and destination_dir in the_config
    // Check source_dir , destination_dir existence

    int opt = 0,parameter_count=0;
    struct option my_opts[] = {
            {.name="date-size-only", .has_arg=0, .flag=0, .val='d'},
            {.name="no-parallel", .has_arg=0, .flag=0, .val='p'},
            {.name="verbose", .has_arg=0, .flag=0, .val='v'},
            {.name="dry-run", .has_arg=0, .flag=0, .val='r'},
            {.name=0, .has_arg=0, .flag=0, .val=0}, // last element must be zero
    };
    while ((opt = (getopt_long(argc, argv, "n::h::", my_opts, NULL))) != -1) {
        switch (opt) {
            case 'd':
                the_config->uses_md5=optarg;
                parameter_count++;
                break;
            case'p':
                the_config->is_parallel=optarg;
                parameter_count++;
                break;
                case 'v':
                the_config->verbose=true;
                parameter_count++;
                break;
            case 'r':
                the_config->dry_run=true;
                parameter_count++;
                break;
            case 'n':
                if(optarg){
                    the_config->processes_count=(int) strtol(optarg,NULL,10);
                    parameter_count++;
                    break;
                }
                if(optind != argc){
                    the_config->processes_count=(int) strtol(argv[optind],NULL,10);
                    parameter_count+=2;
                    break;
                }
                break;
            case 'h':
                display_help(argv[0]);
                parameter_count++;
                break;
            default:
                break;
        }
    }
    if((argc-parameter_count)<2){
        return -1;
    }
    else {
        // Copy source_dir and destination_dir in the_config
        strcpy(the_config->source,argv[argc-1]);
        strcpy(the_config->destination,argv[argc-2]);
        return 0;
    }
}
