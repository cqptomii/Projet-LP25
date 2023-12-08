#include "file-properties.h"
#include "sync.h"
#include <sys/stat.h>
#include <dirent.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "defines.h"
#include <fcntl.h>
#include <stdio.h>
#include <utility.h>

/*!
 * @brief get_file_stats gets all of the required information for a file (inc. directories)
 * @param the files list entry
 * You must get:
 * - for files:
 *   - mode (permissions)
 *   - mtime (in nanoseconds)
 *   - size
 *   - entry type (FICHIER)
 *   - MD5 sum
 * - for directories:
 *   - mode
 *   - entry type (DOSSIER)
 * @return -1 in case of error, 0 else
 */
int get_file_stats(files_list_entry_t *entry) {
    struct stat buf;
    if(stat(entry->path_and_name,&buf)){
       return -1;
    }
    // if entry is File
    if(S_ISREG(buf.st_mode)){
        entry->entry_type=FICHIER;
        entry->mode=buf.st_mode;
        entry->mtime.tv_nsec= buf.st_mtime/100;
        entry->size=buf.st_size;
        compute_file_md5(entry);
        return 0;
    }
    //if entry is Directories
    if(S_ISDIR(buf.st_mode)){
        entry->entry_type=DOSSIER;
        entry->mode=buf.st_mode;
        return 0;
    }
    return -1;
}

/*!
 * @brief compute_file_md5 computes a file's MD5 sum
 * @param the pointer to the files list entry
 * @return -1 in case of error, 0 else
 * Use libcrypto functions from openssl/evp.h
 */
int compute_file_md5(files_list_entry_t *entry) {
    FILE *f = fopen(entry->path_and_name, "rb");
    if (!f) {
        printf("Erreur dans l'ouverture du fichier");
        return -1;
    }

    //INITIALISATION
    EVP_MD_CTX *operations; //Structure représentant le contexte de hachage
    const EVP_MD *hachage; //Structure vers un algorithme de hachage
    unsigned char buffer[PATH_SIZE];
    unsigned char md5_valeur[EVP_MAX_MD_SIZE]; //Création d'un tableau pouvant contenir au maximum 128 bits
    unsigned int digest_len;
    OpenSSL_add_all_digests();
    hachage = EVP_get_digestbyname("md5");
    if (!hachage){
        printf("Unknown message digest\n");
        return -1;
    }
    operations = EVP_MD_CTX_new();
    if(!operations){
        printf("Error creating context\n");
        return -1;
    }
    EVP_DigestInit_ex(operations, hachage, NULL);
    //HACHAGE
    while (1){
        int bytes = fread(buffer, 1, PATH_SIZE, f);
        if (bytes <= 0) break;
        EVP_DigestUpdate(operations, buffer, bytes);
    }

    //CALCUL FIN
    EVP_DigestFinal_ex(operations, md5_valeur, &digest_len);
    EVP_MD_CTX_free(operations);
    fclose(f);
    memcpy(entry->md5sum, md5_valeur, sizeof(entry->md5sum));
    return 0;
}

/*!
 * @brief directory_exists tests the existence of a directory
 * @path_to_dir a string with the path to the directory
 * @return true if directory exists, false else
 */
bool directory_exists(char *path_to_dir) {
	DIR *directories = open_dir(path_to_dir);
	if(directories){
		closedir(directories);
		return true;
	}
	else{
		return false;
	}
}

/*!
 * @brief is_directory_writable tests if a directory is writable
 * @param path_to_dir the path to the directory to test
 * @return true if dir is writable, false else
 * Hint: try to open a file in write mode in the target directory.
 */
bool is_directory_writable(char *path_to_dir) {
    struct dirent *file_entry;
    char path_file[PATH_SIZE];
	DIR *directories = open_dir(path_to_dir);
    if(!directories) {
        printf("Directory not writtable \n");
        return false;
    }
    file_entry = get_next_entry(directories);
    while(file_entry){
        if(concat_path(path_file,path_to_dir,file_entry->d_name)){
            if(file_entry->d_type==DT_DIR){
                return is_directory_writable(path_file);
            }
            if(file_entry->d_type==DT_REG) {
                FILE *open_file = fopen(path_file, "w");
                if (open_file) {
                    fclose(open_file);
                    closedir(directories);
                    return true;
                }
            }
        }
        file_entry= get_next_entry(directories );
    }
    printf("Cannot open the file");
    closedir(directories);
    return false;
}

