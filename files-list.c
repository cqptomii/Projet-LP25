#include <files-list.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <file-properties.h>
#include <stdio.h>

/*!
 * @brief clear_files_list clears a files list
 * @param list is a pointer to the list to be cleared
 * This function is provided, you don't need to implement nor modify it
 */
void clear_files_list(files_list_t *list) {
    while (list->head) {
        files_list_entry_t *tmp = list->head;
        list->head = tmp->next;
        free(tmp);
    }
}

/*!
 *  @brief add_file_entry adds a new file to the files list.
 *  It adds the file in an ordered manner (strcmp) and fills its properties
 *  by calling stat on the file.
 *  If the file already exists, it does nothing and returns 0
 *  @param list the list to add the file entry into
 *  @param file_path the full path (from the root of the considered tree) of the file
 *  @return 0 if success, -1 else (out of memory)
 */
files_list_entry_t *add_file_entry(files_list_t *list, char *file_path) {
    files_list_entry_t *newel = (files_list_entry_t* )malloc(sizeof(files_list_entry_t));
    strcpy(newel->path_and_name,file_path);
    newel->next = NULL;
    newel->prev = NULL;
    if(get_file_stats(newel) == -1){
        printf("Error during get_file_stats \n");
        return NULL;
    }else {
        if(list) {
            if (!list->head) {
                add_entry_to_tail(list, newel);
                return list->head;
            }else {
                files_list_entry_t *cmp = list->head;
                int verif_length;
                while(1){
                    verif_length = (int)(strlen(cmp->path_and_name)-strlen(file_path));
                    if(verif_length > 0){
                        break;
                    }
                    if(verif_length == 0){
                        verif_length = strcmp(cmp->path_and_name,file_path);
                        if(verif_length > 0 || verif_length == 0){
                            break;
                        }
                    }
                    if(!cmp->next){
                        break;
                    }
                    cmp=cmp->next;
                }
                if(verif_length == 0){
                    return NULL;
                }else{
                    if(verif_length > 0) {
                        if(cmp == list->head){
                            newel->next = cmp;
                            cmp->prev = newel;
                            list->head = newel;
                        }else {
                            newel->next = cmp;
                            newel->prev = cmp->prev;
                            (cmp->prev)->next = newel;
                            cmp->prev = newel;
                        }
                        return list->head;
                    }
                }
                if(!cmp->next){
                    add_entry_to_tail(list,newel);
                    return list->tail;
                }
            }
        }
        return NULL;
    }
}

/*!
 * @brief add_entry_to_tail adds an entry directly to the tail of the list
 * It supposes that the entries are provided already ordered, e.g. when a lister process sends its list's
 * elements to the main process.
 * @param list is a pointer to the list to which to add the element
 * @param entry is a pointer to the entry to add. The list becomes owner of the entry.
 * @return 0 in case of success, -1 else
 */
int add_entry_to_tail(files_list_t *list, files_list_entry_t *entry) {
    if(list) {
        if (!list->head) {
            list->head = entry;
            list->tail = entry;
            return 0;
        }else {
            if (list->head == list->tail) {
                (list->head)->next = entry;
                entry->prev = list->head;
                list->tail = entry;
                return 0;
            }else {
                entry->prev = list->tail;
                (list->tail)->next = entry;
                list->tail = entry;
                return 0;
            }
        }
    }
    return -1;
}

/*!
 *  @brief find_entry_by_name looks up for a file in a list
 *  The function uses the ordering of the entries to interrupt its search
 *  @param list the list to look into
 *  @param file_path the full path of the file to look for
 *  @param start_of_src the position of the name of the file in the source directory (removing the source path)
 *  @param start_of_dest the position of the name of the file in the destination dir (removing the dest path)
 *  @return a pointer to the element found, NULL if none were found.
 */
files_list_entry_t *find_entry_by_name(files_list_t *list, char *file_path, size_t start_of_src, size_t start_of_dest) {
    if(!list || !file_path){
        return NULL;
    }
    if(list->head) {
        files_list_entry_t *cmp = list->head;
        size_t path_len = strlen(file_path) - start_of_dest;
        while (cmp->next) {
            size_t cmp_len = strlen(cmp->path_and_name) - start_of_src;
            if (cmp_len >= path_len && strncmp(cmp->path_and_name+start_of_src,file_path+start_of_dest, path_len)==0){
                return cmp;
            }
            cmp = cmp->next;
        }
        size_t cmp_len = strlen(cmp->path_and_name) - start_of_src;
        if(cmp_len >= path_len && (strncmp(cmp->path_and_name + start_of_src, file_path + start_of_dest, path_len)==0)){
            return cmp;
        }
    }
    return NULL;
}

/*!
 * @brief display_files_list displays a files list
 * @param list is the pointer to the list to be displayed
 * This function is already provided complete.
 */
void display_files_list(files_list_t *list) {
    if (!list)
        return;
    
    for (files_list_entry_t *cursor=list->head; cursor != NULL; cursor=cursor->next) {
        printf("%s\n", cursor->path_and_name);
    }
}

/*!
 * @brief display_files_list_reversed displays a files list from the end to the beginning
 * @param list is the pointer to the list to be displayed
 * This function is already provided complete.
 */
void display_files_list_reversed(files_list_t *list) {
    if (!list)
        return;
    
    for (files_list_entry_t *cursor=list->tail; cursor!=NULL; cursor=cursor->prev) {
        printf("%s\n", cursor->path_and_name);
    }
}
