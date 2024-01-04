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

#define MQ_KEY_CREATE_ID 42
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
        //create mq_key
        p_context->shared_key = ftok("mq_key.txt", MQ_KEY_CREATE_ID);
        if(p_context->shared_key == -1 ){
            perror("Erreur lors de la creation de la clé IPC \n");
            return -1;
        }

        //set-up the mq FIFO
        p_context->message_queue_id = msgget(p_context->shared_key,IPC_CREAT | 0666);
        if(p_context->message_queue_id == -1){
            perror("Erreur lors de la création de la file de message \n");
            return -1;
        }
        p_context->main_process_pid = getpid();
        //set-up source / destination lister_pid to 0
        p_context->source_lister_pid = 0;
        p_context->destination_lister_pid = 0;
        p_context->processes_count = 0;
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
                free(lister_config);
            } else if (func == analyzer_process_loop) {
                if (p_context->source_analyzers_pids != NULL && p_context->processes_count > 0) {
                    analyzer_configuration_t *analyzer_config = (analyzer_configuration_t *) parameters;
                    if(analyzer_config->my_recipient_id == MSG_TYPE_TO_SOURCE_ANALYZERS) {
                        p_context->source_analyzers_pids[p_context->processes_count] = child_pid;
                        p_context->processes_count++;
                    }
                    if(analyzer_config->my_recipient_id == MSG_TYPE_TO_DESTINATION_ANALYZERS){
                        p_context->destination_analyzers_pids[p_context->processes_count] = child_pid;
                        p_context->processes_count++;
                    }
                    free(analyzer_config);
                }
            }
        }
        return child_pid
    }
}

/*!
 * @brief lister_process_loop is the lister process function (@see make_process)
 * @param parameters is a pointer to its parameters, to be cast to a lister_configuration_t
 */
void lister_process_loop(void *parameters) {
    if(parameters){
        lister_configuration_t *lister_config = (lister_configuration_t *) parameters;
        //attente de reception du message d'analyse de repertoire
        analyze_dir_command_t dir_message;
        memset(&dir_message,0, sizeof(analyze_dir_command_t));
        if(msgrcv(lister_config->my_receiver_id,&dir_message, sizeof(analyze_dir_command_t),COMMAND_CODE_ANALYZE_DIR,0) == -1){
            perror("Erreur lors de la reception du message");
            exit(EXIT_FAILURE);
        }
        //creation d'une liste de fichier + remplissages du path_name de chaque element
        files_list_t  build_list;
        //envoye des n premiers elements de la liste vers les n analyzeurs
        //boucle infinie
        //transmission des entrées à jour au main process
        //fin du processus
        simple_command_t end_message;
        memset(&end_message,0, sizeof(simple_command_t));
        if(msgrcv(lister_config->my_receiver_id,&end_message, sizeof(simple_command_t),COMMAND_CODE_TERMINATE,0) == -1){
            perror("Erreur lors de la reception du message");
            exit(EXIT_FAILURE);
        }
        // send code TERMINATE_OK au main
        send_terminate_confirm(lister_config->my_recipient_id,COMMAND_CODE_TERMINATE_OK);
    }
}

/*!
 * @brief analyzer_process_loop is the analyzer process function
 * @param parameters is a pointer to its parameters, to be cast to an analyzer_configuration_t
 */
void analyzer_process_loop(void *parameters) {
    if(parameters){
        analyzer_configuration_t *analyzer_config = (analyzer_configuration_t *) parameters;
        //declaration des variables recevant les messages
        simple_command_t end_message;
        memset(&end_message,0, sizeof(simple_command_t));
        analyze_dir_command_t dir_message;
        memset(&dir_message,0, sizeof(analyze_dir_command_t));
        analyze_file_command_t file_message;
        memset(&file_message,0, sizeof(analyze_file_command_t));

        //boucle infini
        while(1){
            //gestion message de terminaison
            int end_result = msgrcv(analyzer_config->my_receiver_id,&end_message, sizeof(simple_command_t),COMMAND_CODE_TERMINATE,IPC_NOWAIT);
            if(end_result == -1){
                if(errno == ENOMSG){
                    // attendre 1000 mili seconde
                    usleep(100000);
                }else{
                    perror("Erreur lors de la lecture du message");
                    exit(EXIT_FAILURE);
                }
            }else{
                // message de terminaison reçu
                break;
            }

            //gestion message d'analyse fichier / dossier
            int dir_result = msgrcv(analyzer_config->my_receiver_id,&dir_message, sizeof(analyze_dir_command_t),COMMAND_CODE_ANALYZE_DIR,IPC_NOWAIT);
            if(dir_result == -1){
                if(errno == ENOMSG){
                    // attendre 1000 mili seconde
                    usleep(100000);
                }else{
                    perror("Erreur lors de la lecture du message");
                    exit(EXIT_FAILURE);
                }
            }else {
                // message d'analyse de repertoire reçu -> tratement
                break;
            }
            int file_result = msgrcv(analyzer_config->my_receiver_id,&file_message, sizeof(analyze_file_command_t),COMMAND_CODE_ANALYZE_FILE,IPC_NOWAIT);
            if(file_result == -1){
                if(errno == ENOMSG){
                    // attendre 1000 mili seconde
                    usleep(100000);
                }else{
                    perror("Erreur lors de la lecture du message");
                    exit(EXIT_FAILURE);
                }
            }else{
                // message d'analyse de fichier reçu -> tratement
                break;
            }
        }

        // send code TERMINATE_OK au main
        send_terminate_confirm(analyzer_config->my_recipient_id,COMMAND_CODE_TERMINATE_OK);
    }
}

/*!
 * @brief clean_processes cleans the processes by sending them a terminate command and waiting to the confirmation
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the processes context
 */
void clean_processes(configuration_t *the_config, process_context_t *p_context) {
    // Do nothing if not parallel
    if(the_config->is_parallel){
        // Send terminate
        //envoye des messages de terminaison des processus fils
        send_terminate_command(p_context->message_queue_id,p_context->source_lister_pid);
        send_terminate_command(p_context->message_queue_id,p_context->destination_lister_pid);
        simple_command_t end_message;
        memset(&end_message,0, sizeof(simple_command_t));

        // Wait for responses
        //Attente de la reception du message de comfirmation de terminaison des processus fils
        if(msgrcv(p_context->message_queue_id,&end_message, sizeof(simple_command_t),COMMAND_CODE_TERMINATE_OK,0) == -1){
            perror("Erreur lors de la reception du message de terminaison ");
            exit(EXIT_FAILURE);
        }

        // Free allocated memory
        //Libération de la mémoire allouer
        free(p_context->destination_analyzers_pids);
        free(p_context->source_analyzers_pids);

        // Free the MQ
        //Destruction de la file de message
        if(msgctl(p_context->message_queue_id,IPC_RMID,NULL) == -1){
            perror("Erreur durant la suppression de la file de message");
            exit(EXIT_FAILURE);
        }
        free(p_context);
    }
}
void request_element_details(int msg_queue, files_list_entry_t *entry, lister_configuration_t *cfg, int *current_analyzers){

}