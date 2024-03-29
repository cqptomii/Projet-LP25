#include <messages.h>
#include <sys/msg.h>
#include <string.h>

// Functions in this file are required for inter processes communication

/*!
 * @brief send_file_entry sends a file entry, with a given command code
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @param cmd_code is the cmd code to process the entry.
 * @return the result of the msgsnd function
 * Used by the specialized functions send_analyze*
 */
int send_file_entry(int msg_queue, int recipient, files_list_entry_t *file_entry, int cmd_code) {
       
    files_list_entry_transmit_t msg;
    
    msg.mtype = recipient;
    msg.op_code = (char)cmd_code;
    msg.payload = *file_entry;
    msg.reply_to = msg_queue;
    size_t msg_length = sizeof(files_list_entry_transmit_t) - sizeof(long);
    return msgsnd(msg_queue, &msg, msg_length, 0);
}

/*!
 * @brief send_analyze_dir_command sends a command to analyze a directory
 * @param msg_queue is the id of the MQ used to send the command
 * @param recipient is the recipient of the message (mtype)
 * @param target_dir is a string containing the path to the directory to analyze
 * @return the result of msgsnd
 */
int send_analyze_dir_command(int msg_queue, int recipient, char *target_dir) {
    analyze_dir_command_t dir_command;
    dir_command.mtype = recipient;
    dir_command.op_code = COMMAND_CODE_ANALYZE_DIR;
    strcpy(dir_command.target,target_dir);
    size_t msg_length = sizeof(analyze_dir_command_t) - sizeof(long);

    return msgsnd(msg_queue, &dir_command, msg_length, 0);
}

// The 3 following functions are one-liners

/*!
 * @brief send_analyze_file_command sends a file entry to be analyzed
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_analyze_file_command(int msg_queue, int recipient, files_list_entry_t *file_entry) {
    return send_file_entry(msg_queue, recipient, file_entry, COMMAND_CODE_ANALYZE_FILE);
}

/*!
 * @brief send_analyze_file_response sends a file entry after analyze
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_analyze_file_response(int msg_queue, int recipient, files_list_entry_t *file_entry) {
    return send_file_entry(msg_queue,recipient,file_entry,COMMAND_CODE_FILE_ANALYZED);
}

/*!
 * @brief send_files_list_element sends a files list entry from a complete files list
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_files_list_element(int msg_queue, int recipient, files_list_entry_t *file_entry) {
    return send_file_entry(msg_queue,recipient,file_entry,COMMAND_CODE_FILE_ENTRY);
}

/*!
 * @brief send_list_end sends the end of list message to the main process
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @return the result of msgsnd
 */
int send_list_end(int msg_queue, int recipient) {
    simple_command_t end_message;
    end_message.mtype = recipient;
    end_message.message = COMMAND_CODE_LIST_COMPLETE;
    size_t msg_length = sizeof(simple_command_t) - sizeof(long);
    return msgsnd(msg_queue,&end_message,msg_length,0);
}

/*!
 * @brief send_terminate_command sends a terminate command to a child process so it stops
 * @param msg_queue is the MQ id used to send the command
 * @param recipient is the target of the terminate command
 * @return the result of msgsnd
 */
int send_terminate_command(int msg_queue, int recipient) {
    simple_command_t terminate_message;
    terminate_message.mtype = recipient;
    terminate_message.message = COMMAND_CODE_TERMINATE;
    size_t msg_length = sizeof(simple_command_t) - sizeof(long);
    return msgsnd(msg_queue,&terminate_message,msg_length,0);
}

/*!
 * @brief send_terminate_confirm sends a terminate confirmation from a child process to the requesting parent.
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @return the result of msgsnd
 */
int send_terminate_confirm(int msg_queue, int recipient) {
    simple_command_t confirm_message;
    confirm_message.mtype = recipient;
    confirm_message.message = COMMAND_CODE_TERMINATE_OK;
    size_t msg_length = sizeof(simple_command_t) - sizeof(long);

    return msgsnd(msg_queue,&confirm_message,msg_length,0);
}
