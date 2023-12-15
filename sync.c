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
    if (the_config->is_parallel) {

    }
    else {
        // Init list
        files_list_t *source = (files_list_t *)malloc(sizeof(files_list_t));
        source->head = NULL;
        source->tail = NULL;
        files_list_t *destination = (files_list_t *)malloc(sizeof(files_list_t));
        destination->head = NULL;
        destination->tail = NULL;
        files_list_t *difference = (files_list_t *)malloc(sizeof(files_list_t));
        difference->head = NULL;
        difference->tail = NULL;

        //Build source / destination / difference
        make_files_list(source,the_config->source);
        make_files_list(destination,the_config->destination);


        files_list_entry_t *cmp_source = source->head;
        files_list_entry_t *cmp_destination = destination->head;
        while (cmp_source) {
            while (cmp_destination) {
                if (mismatch(cmp_source, cmp_destination, the_config->uses_md5)) {
                    add_file_entry(difference, cmp_source->path_and_name);
                }
                cmp_destination = cmp_destination->next;
            }
            cmp_source = cmp_source->next;
        }
        printf("Difference \n");
        display_files_list(difference);
        /*
        // Apply difference into destination
        files_list_entry_t *cmp_difference = difference->head;
        while (cmp_difference->next) {
            copy_entry_to_destination(difference->head, the_config);
        }
         */
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
  if (has_md5) {
      if (memcmp(lhd->md5sum, rhd->md5sum, sizeof(lhd->md5sum)) != 0) {
          return true;  // Les empreintes MD5 sont différentes
      }
  }
  if (difftime(lhd->mtime.tv_sec,rhd->mtime.tv_sec) == 0) {
      if (lhd->size == rhd->size) {
          if (lhd->entry_type == rhd->entry_type) {
              if (lhd->mode == rhd->mode) {
                  return false;
              }
          }
      }
  }
  return true;
}

/*!
 * @brief make_files_list buils a files list in no parallel mode
 * @param list is a pointer to the list that will be built
 * @param target_path is the path whose files to list
 */
void make_files_list(files_list_t *list, char *target_path) {
    make_list(list,target_path);
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
    char destination_path[PATH_SIZE];
    if (S_ISREG(source_entry->mode)) {
        concat_path(destination_path, the_config->destination, source_entry->path_and_name);
        int source_fd = open(source_entry->path_and_name, O_RDONLY);
        if (source_fd == -1) {
            perror("Error opening source file");
            return;
        }
        int destination_fd = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, source_entry->mode);
        if (destination_fd == -1) {
            perror("Error opening destination file");
            close(source_fd);
            return;
        }
        off_t offset = 0;
        ssize_t bytes_copied = sendfile(destination_fd, source_fd, &offset, source_entry->size);
        if (bytes_copied == -1) {
            perror("Error copying file contents");
        }
        close(source_fd);
        close(destination_fd);
        // Keeping aess modes and mtime
        utimensat(AT_FDCWD, destination_path, &(struct timespec[]){source_entry->mtime, source_entry->mtime}, 0);
    } else if (S_ISDIR(source_entry->mode)) {
        concat_path(destination_path, the_config->destination, source_entry->path_and_name);
        if (mkdir(destination_path, source_entry->mode) == -1) {
            perror("Error creating destination directory");
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
    DIR *target_dir;
    struct dirent *dir_entry;
    char path_file[PATH_SIZE];
    if (!(target_dir = open_dir(target))) { // Check file opening
        return;
    }
    while ((dir_entry=get_next_entry(target_dir)) != NULL) {
        concat_path(path_file, target, dir_entry->d_name);
        if (dir_entry->d_type == DT_REG) {
            add_file_entry(list, path_file);
        }
        if (dir_entry->d_type == DT_DIR) {
            make_list(list,path_file);
        }
    }
    closedir(target_dir);
}

/*!
 * @brief open_dir opens a dir
 * @param path is the path to the dir
 * @return a pointer to a dir, NULL if it cannot be opened
 */
DIR *open_dir(char *path) {
    DIR *directories = NULL;
    directories = opendir(path);
    if (!directories) {
        perror(strerror(errno));
        return NULL;
    }
    else {
        return directories;
    }
}

/*!
 * @brief get_next_entry returns the next entry in an already opened dir
 * @param dir is a pointer to the dir (as a result of opendir, @see open_dir)
 * @return a struct dirent pointer to the next relevant entry, NULL if none found (use it to stop iterating)
 * Relevant entries are all regular files and dir, except . and ..
 */
struct dirent *get_next_entry(DIR *dir) {
    struct dirent *next_entry;
    while ((next_entry = readdir(dir)) != NULL) {
        if (next_entry->d_type == DT_REG || next_entry->d_type == DT_DIR) {
            if (strcmp(next_entry->d_name, ".") != 0 && strcmp(next_entry->d_name, "..") != 0) {
                return next_entry;
            }
        }
    }
    return NULL;
}
