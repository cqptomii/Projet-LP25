#include "sync.h"
#include <dirent.h>
#include <string.h>
#include <processes.h>
#include <utility.h>
#include <messages.h>
#include "file-properties.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>



/*!
 * @brief synchronize is the main function for synchronization
 * It will build the lists (source and destination), then make a third list with differences, and apply differences to the destination
 * It must adapt to the parallel or not operation of the program.
 * @param the_config is a pointer to the configuration
 * @param p_context is a pointer to the processes context
 */
void synchronize(configuration_t *the_config, process_context_t *p_context) {
    if(the_config->is_parallel){

    }
    else{
        // Init list
        files_list_t source;
        source.head=NULL;
        source.tail=NULL;
        files_list_t destination;
        destination.head=NULL;
        destination.tail=NULL;
        files_list_t difference;
        difference.head=NULL;
        difference.tail=NULL;
        //Build source / destination / difference
        make_files_list(&source,the_config->source);
        if (the_config->verbose) {
            display_files_list(&source);
        }
        if(the_config->verbose){
            printf("\n\n");
        }
        make_files_list(&destination,the_config->destination);
        if (the_config->verbose) {
            display_files_list(&destination);
        }
        if(the_config->verbose){
            printf("\n\n");
        }
        files_list_entry_t *cmp_source = source.head;
        files_list_entry_t *cmp_destination;
        if(the_config->verbose){
            printf("Source and destination comparaison \n");
        }
        while(cmp_source){
            if(destination.head) {
                cmp_destination=find_entry_by_name(&destination, cmp_source->path_and_name, strlen(the_config->source), strlen(the_config->destination));
                if (!cmp_destination) {
                    if(the_config->verbose){
                        printf("Add file %s to difference \n",cmp_source->path_and_name);
                    }
                    add_file_entry(&difference, cmp_source->path_and_name);
                }else{
                    if (mismatch(cmp_source, cmp_destination, the_config->uses_md5)) {
                        add_file_entry(&difference, cmp_source->path_and_name);
                        if(the_config->verbose){
                            printf("Add file %s to difference \n",cmp_source->path_and_name);
                        }
                    }
                }
            }else{
                add_file_entry(&difference,cmp_source->path_and_name);
                if(the_config->verbose){
                    printf("Add file %s to difference \n",cmp_source->path_and_name);
                }
            }
            cmp_source=cmp_source->next;
        }
        make_files_list(&difference,NULL);
        if (the_config->verbose) {
            display_files_list(&difference);
        }
        // Apply difference into destination
        files_list_entry_t *cmp_difference = difference.head;
        if(!the_config->dry_run) {
            while (cmp_difference) {
                copy_entry_to_destination(cmp_difference, the_config);
                cmp_difference = cmp_difference->next;
            }
        }
        if(the_config->verbose) {
            printf(" clear files lists  : ");
        }
        clear_files_list(&difference);
        clear_files_list(&source);
        clear_files_list(&destination);
        if(the_config->verbose) {
            printf(" End \n");
        }
    }
    return;
}

/*!
 * @brief mismatch tests if two files with the same name (one in source, one in destination) are equal
 * @param lhd a files list entry from the source
 * @param rhd a files list entry from the destination
 * @has_md5 a value to enable or disable MD5 sum check
 * @return true if both files are not equal, false else
 */
bool mismatch(files_list_entry_t *lhd, files_list_entry_t *rhd, bool has_md5) {
    if(the_config->verbose) {
        printf(" Verification of files differences : ");
    }
    if (has_md5==true) {
          if (memcmp(lhd->md5sum, rhd->md5sum, sizeof(lhd->md5sum)) != 0) {
              perror("MD5 sum are different \n");
              if(the_config->verbose) {
                  printf(" EQUAL \n");
              }
              return true;  // Les empreintes MD5 sont différentes
          }
    }
    if (difftime(lhd->mtime.tv_nsec,rhd->mtime.tv_nsec) == 0.0 && difftime(lhd->mtime.tv_sec,rhd->mtime.tv_sec) == 0.0){
        if(lhd->size == rhd->size){
           if(lhd->entry_type == rhd->entry_type){
               if(lhd->mode == rhd->mode){
                   if(the_config->verbose) {
                       printf(" DIFFERENT \n");
                   }
                   return false;
               }
           }
       }
    }
    if(the_config->verbose) {
        printf(" EQUAL \n");
    }
    return true;
}

/*!
 * @brief make_files_list buils a files list in no parallel mode
 * @param list is a pointer to the list that will be built
 * @param target_path is the path whose files to list
 */
void make_files_list(files_list_t *list, char *target_path) {
    if(the_config->verbose) {
        printf("Build file list on target : %s  | ",target_path);
    }
    make_list(list,target_path);
    files_list_entry_t *cmp=list->head;
    while(cmp){
        if(get_file_stats(cmp)==-1){
            if(the_config->verbose) {
                printf(" Failed \n");
            }
            printf("Error  \n");
            return;
        }
        cmp = cmp->next;
    }
    if(the_config->verbose) {
        printf(" Succes \n");
    }
}

/*!
 * @brief make_files_lists_parallel makes both (src and dest) files list with parallel processing
 * @param src_list is a pointer to the source list to build
 * @param dst_list is a pointer to the destination list to build
 * @param the_config is a pointer to the program configuration
 * @param msg_queue is the id of the MQ used for communication
 */
void make_files_lists_parallel(files_list_t *src_list, files_list_t *dst_list, configuration_t *the_config, int msg_queue) {

}

/*!
 * @brief copy_entry_to_destination copies a file from the source to the destination
 * It keeps access modes and mtime (@see utimensat)
 * Pay attention to the path so that the prefixes are not repeated from the source to the destination
 * Use sendfile to copy the file, mkdir to create the directory
 */
void copy_entry_to_destination(files_list_entry_t *source_entry, configuration_t *the_config) {
    char file_created_path[PATH_SIZE];
    //delete prefix from file_list_entry
    if (S_ISREG(source_entry->mode)) {
        concat_path(file_created_path, the_config->destination, source_entry->path_and_name+strlen(the_config->source)+1);
        if(the_config->verbose) {
            printf("|| Copying %s into %s ||", file_created_path, the_config->destination);
        }
        int source_fd = open(source_entry->path_and_name, O_RDONLY);
        if (source_fd == -1) {
            printf(" Failed \n");
            perror("Error during source file opening \n");
            return;
        }
        //Create intermediate folders
        char *sep="/";
        char *token = strtok(source_entry->path_and_name+strlen(the_config->source)+1,sep);
        char dir_path[256] = "";
        strcat(dir_path,the_config->destination);
        while(strcmp(dir_path,file_created_path) != 0){
            strcat(dir_path,sep);
            strcat(dir_path,token);
            if(strcmp(dir_path,file_created_path)!=0) {
                if(mkdir(dir_path,0777) != 0) {
                    if(the_config->verbose) {
                        printf(" Failed \n");
                    }
                    perror("Canno't open the path \n");
                    return;
                }else{

                }
            }
            token = strtok(NULL,sep);
        }
        int destination_fd = open(file_created_path, O_WRONLY | O_CREAT | O_TRUNC, source_entry->mode);
        if (destination_fd == -1) {
            if(the_config->verbose) {
                printf(" Failed \n");
            }
            perror("Error during destination file opening \n");
            return;
        }

        off_t offset = 0;
        ssize_t bytes_copied = sendfile(destination_fd, source_fd, &offset, source_entry->size);
        if (bytes_copied == -1) {
            if(the_config->verbose) {
                printf(" Failed \n");
            }
            perror("Error copying file contents");
            return;
        }
        close(source_fd);
        close(destination_fd);
        // Keeping access modes and mtime
        struct timespec mtime[2] = {source_entry->mtime, source_entry->mtime};
        if (utimensat(AT_FDCWD, file_created_path, mtime, 0) == -1) {
            if(the_config->verbose) {
                printf(" Failed \n");
            }
            perror("Error setting acces modes and mtime");
            return;
        }
        if(the_config->verbose) {
            printf("Succes \n");
        }
    }
}

/*!
 * @brief make_list lists files in a location (it recurses in directories)
 * It doesn't get files properties, only a list of paths
 * This function is used by make_files_list and make_files_list_parallel
 * @param list is a pointer to the list that will be built
 * @param target is the target dir whose content must be listed
 */
void make_list(files_list_t *list, char *target) {
    if(!list || !target){
        return;
    }
    DIR *target_dir;
    struct dirent *dir_entry;
    char path_file[PATH_SIZE];
    if(!(target_dir = open_dir(target))){// Check file opening
        return;
    }
    if (the_config->verbose) {
        printf(" Dir : %s opening \n",target);
    }
    while((dir_entry=get_next_entry(target_dir)) != NULL){
        concat_path(path_file, target, dir_entry->d_name);
        if(dir_entry->d_type == DT_REG){
            add_file_entry(list, path_file);
            if (the_config->verbose) {
                printf("add file %s to the current list \n",path_file);
            }
        }
        if(dir_entry->d_type == DT_DIR){
            make_list(list,path_file);
        }
    }
    if (the_config->verbose) {
        printf("Close Dir \n");
    }
    closedir(target_dir);
}

/*!
 * @brief open_dir opens a dir
 * @param path is the path to the dir
 * @return a pointer to a dir, NULL if it cannot be opened
 */
DIR *open_dir(char *path) {
    if(!path){
        return NULL;
    }
    DIR *directories = NULL;
    directories = opendir(path);
    if(!directories){
        perror(strerror(errno));
        return NULL;
    }
    else{
        return directories;
    }
}

/*!
 * @brief get_next_entry returns the next entry in an already opened dir
 * @param dir is a pointer to the dir (as a result of opendir, @see open_dir)
 * @return a struct dirent pointer to the next relevant entry, NULL if none found (use it to stop iterating)
 * Relevant entries are all regular files and dir, except . and ..
 */
struct dirent *get_next_entry(DIR *dir)  {
    if(!dir){
        return NULL;
    }
    struct dirent *next_entry;
    while((next_entry = readdir(dir))!=NULL){
        if(next_entry->d_type == DT_REG || next_entry->d_type == DT_DIR){
            if(strcmp(next_entry->d_name, ".") != 0 && strcmp(next_entry->d_name, "..") != 0){
                return next_entry;
            }
        }
    }
    return NULL;
}
