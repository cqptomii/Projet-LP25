#include "processes.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <stdio.h>
#include <messages.h>
#include <file-properties.h>
#include <sync.h>
#include <string.h>
#include <errno.h>

/*!
 * @brief prepare prepares (only when parallel is enabled) the processes used for the synchronization.
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the program processes context
 * @return 0 if all went good, -1 else
 */
int prepare(configuration_t *the_config, process_context_t *p_context) {
    if (!the_config->is_parallel) {
        return 0;
    }else{
        //set source / destination lister_pid to 0
        p_context->source_lister_pid = 0;
        p_context->destination_lister_pid = 0;
        p_context->processes_count = 0;
        p_context->main_process_pid = getpid();
    }
    return 0;
}

/*!
 * @brief make_process creates a process and returns its PID to the parent
 * @param p_context is a pointer to the processes context
 * @param func is the function executed by the new process
 * @param parameters is a pointer to the parameters of func
 * @return the PID of the child process (it never returns in the child process)
 */
int make_process(process_context_t *p_context, process_loop_t func, void *parameters) {
    
    pid_t child_pid = fork();

    if (child_pid < 0) {
        perror("Erreur lors de la création du processus");
        return -1;
    }

    if (child_pid == 0) {
        // Dans le processus enfant, on exécute la fonction spécifiée par func avec les paramètres fournis
        if(func == lister_process_loop  || func == analyzer_process_loop ) {
            func(parameters);
        }
    }else {
        // Dans le processus parent, on stock le PID du processus enfant dans p_context->source_lister_pid et *source_analyzers_pids
        if (p_context != NULL && p_context->processes_count > 0) {
            if (func == lister_process_loop) {
                lister_configuration_t *lister_config = (lister_configuration_t *) parameters;
                if(lister_config->my_recipient_id == MSG_TYPE_TO_SOURCE_LISTER){
                    p_context->source_lister_pid = child_pid;
                }
                if(lister_config->my_recipient_id == MSG_TYPE_TO_DESTINATION_LISTER){
                    p_context->destination_lister_pid = child_pid;
                }
            } else if (func == analyzer_process_loop) {
                if (p_context->source_analyzers_pids != NULL && p_context->processes_count > 0) {
                    analyzer_configuration_t *analyzer_config = (analyzer_configuration_t *) parameters;
                    if(analyzer_config->my_recipient_id == MSG_TYPE_TO_SOURCE_ANALYZERS){
                        p_context->source_analyzers_pids = child_pid;
                    }
                    if(analyzer_config->my_recipient_id == MSG_TYPE_TO_DESTINATION_ANALYZERS){
                        p_context->destination_analyzers_pids = child_pid;
                    }
                }
            }
            return child_pid;
        }
    }
}

/*!
 * @brief lister_process_loop is the lister process function (@see make_process)
 * @param parameters is a pointer to its parameters, to be cast to a lister_configuration_t
 */
void lister_process_loop(void *parameters) {
    if(parameters){
    }
}

/*!
 * @brief analyzer_process_loop is the analyzer process function
 * @param parameters is a pointer to its parameters, to be cast to an analyzer_configuration_t
 */
void analyzer_process_loop(void *parameters) {
    if(parameters){
    }
}

/*!
 * @brief clean_processes cleans the processes by sending them a terminate command and waiting to the confirmation
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the processes context
 */
void clean_processes(configuration_t *the_config, process_context_t *p_context) {
    if(the_config->is_parallel){
        send_terminate_command(p_context->message_queue_id,p_context->main_process_pid);
        while(msgget(p_context->shared_key,0) < 0 ){
        }
    }
    // Do nothing if not parallel
    // Send terminate
    // Wait for responses
    // Free allocated memory
    // Free the MQ
}
