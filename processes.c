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
        if(the_config->verbose){
            printf("Creating MQ shared key \n");
        }
        p_context->shared_key = ftok("mq_key.txt", MQ_KEY_CREATE_ID);
        if(p_context->shared_key == -1 ){
            perror("Erreur lors de la creation de la clé IPC \n");
            return -1;
        }

        //set-up the main mq FIFO
        if(the_config->verbose){
            printf("Creating message FIFO \n");
        }
        p_context->message_queue_id = msgget(p_context->shared_key,IPC_CREAT | 0666);
        if(p_context->message_queue_id == -1){
            perror("Erreur lors de la création de la file de message \n");
            return -1;
        }
        p_context->main_process_pid = getpid();
        p_context->processes_count = the_config->processes_count;

        //allocation des tableau d'analyzer
        p_context->source_analyzers_pids = (pid_t *) malloc(sizeof(pid_t)*p_context->processes_count);
        p_context->destination_analyzers_pids = (pid_t *) malloc(sizeof(pid_t)*p_context->processes_count);

        //set-up source / destination lister_pid to 0
        lister_configuration_t lister = {0,p_context->message_queue_id,p_context->processes_count,p_context->shared_key};
        analyzer_configuration_t analyzer = {0,p_context->message_queue_id,p_context->shared_key,the_config->uses_md5};
        void *parameter = &lister;
        if(the_config->verbose){
            printf("Creation of source / destination lister process\n");
        }
        lister.my_recipient_id = MSG_TYPE_TO_SOURCE_LISTER;
        p_context->source_lister_pid = make_process(p_context,lister_process_loop,parameter);
        if(p_context->source_lister_pid <=0){
            perror("Erreur lors de la creation du processus lister");
            return -1;
        }
        lister.my_recipient_id = MSG_TYPE_TO_DESTINATION_LISTER;
        p_context->destination_lister_pid = make_process(p_context,lister_process_loop,parameter);
        if(p_context->destination_lister_pid <=0){
            perror("Erreur lors de la creation du processus lister");
            return -1;
        }

        parameter = &analyzer;
        if(the_config->verbose){
            printf("Creation of source / destination analyzer process \n");
        }
        for(int i = 0; i< p_context->processes_count;i++){
            analyzer.my_recipient_id = MSG_TYPE_TO_SOURCE_ANALYZERS;
            p_context->source_analyzers_pids[i] = make_process(p_context,analyzer_process_loop,parameter);
            if(p_context->source_analyzers_pids[i] <= 0){
                perror("Erreur lors de la creation du processus analyzer");
                return -1;
            }
            analyzer.my_recipient_id = MSG_TYPE_TO_DESTINATION_ANALYZERS;
            p_context->destination_analyzers_pids[i] = make_process(p_context,analyzer_process_loop,parameter);
            if(p_context->destination_analyzers_pids[i] <= 0){
                perror("Erreur lors de la creation du processus analyzer");
                return -1;
            }
        }
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
            exit(EXIT_SUCCESS);
        }else{
            exit(EXIT_FAILURE);
        }
    }else {
        return child_pid;
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
        make_list(&build_list,dir_message.target);
        int file_send = 0;
        files_list_entry_t *current_entry = build_list.head;
        //envoye des n premiers elements de la liste vers les n analyzeurs
        while(current_entry != NULL &&  file_send < lister_config->analyzers_count){
            //analyze file
            request_element_details(lister_config->my_receiver_id,current_entry,lister_config,&file_send);

            //get next entry
            current_entry = current_entry->next;
            ++file_send;
        }
        //boucle infinie
        bool running = true;
        files_list_t complete_list;
        files_list_entry_transmit_t receipt_entry;
        memset(&receipt_entry,0, sizeof(files_list_entry_transmit_t));
        while(current_entry != NULL && running){
            //reception des reponses des analyzer -> envoye de la prochaine entry
            if(msgrcv(lister_config->my_receiver_id,&receipt_entry, sizeof(files_list_entry_transmit_t),COMMAND_CODE_FILE_ANALYZED,0) == -1){
                perror("Erreur lors de la lecture du message");
                exit(EXIT_FAILURE);
            }
            //stockage de l'entrée reçu
            add_entry_to_tail(complete_list,receipt_entry.payload);
            --file_send;
            if(current_entry != NULL) {
                send_analyze_file_command(lister_config->my_receiver_id, COMMAND_CODE_ANALYZE_FILE, current_entry);
                current_entry = current_entry->next;
                ++file_send;
            }
            if(file_send == 0){
                running = false;
            }
        }
        current_entry = complete_list.head
        clear_files_list(&build_list);
        //envoye du message de fin de completion de liste
        send_list_end(lister_config->my_receiver_id,MSG_TYPE_TO_MAIN);
        //transmission des entrées à jour au main process une par une
        while(current_entry != NULL){
            send_files_list_element(lister_config->my_receiver_id,MSG_TYPE_TO_MAIN,current_entry);
            current_entry = current_entry->next;
        }
        //fin du processus
        simple_command_t end_message;
        memset(&end_message,0, sizeof(simple_command_t));
        if(msgrcv(lister_config->my_receiver_id,&end_message, sizeof(simple_command_t),COMMAND_CODE_TERMINATE,0) == -1){
            perror("Erreur lors de la reception du message");
            exit(EXIT_FAILURE);
        }
        // send code TERMINATE_OK au main
        send_terminate_confirm(lister_config->my_receiver_id,MSG_TYPE_TO_MAIN);
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
                send_analyze_dir_command(analyzer_config->my_receiver_id,COMMAND_CODE_FILE_ANALYZED,dir_message.target);
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
                // message d'analyse de fichier reçu -> traitement
                files_list_entry_t *entry = &file_message.payload;
                get_file_stats(entry);
                //send response
                send_analyze_file_response(analyzer_config->my_receiver_id,COMMAND_CODE_FILE_ANALYZED,entry);
                break;
            }
        }

        // send code TERMINATE_OK au main
        send_terminate_confirm(analyzer_config->my_receiver_id,MSG_TYPE_TO_MAIN);
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
        int count_lister_end = 0;
        simple_command_t end_message;
        // Send terminate
        //envoye des messages de terminaison des processus fils
        send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_SOURCE_LISTER);
        send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_DESTINATION_LISTER);
        send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_SOURCE_ANALYZERS);
        send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_DESTINATION_ANALYZERS);
        // Wait for responses
        //Attente de la reception du message de comfirmation de terminaison des processus fils
        while(count_lister_end < 4){
            memset(&end_message,0, sizeof(simple_command_t));
            if(msgrcv(p_context->message_queue_id,&end_message, sizeof(simple_command_t),COMMAND_CODE_TERMINATE_OK,0) == -1){
                perror("Erreur lors de la reception du message de terminaison ");
                exit(EXIT_FAILURE);
            }
            ++count_lister_end;
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
    send_analyze_file_command(msg_queue,COMMAND_CODE_ANALYZE_FILE,entry);
}