#include <defines.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*!
 * @brief concat_path concatenates suffix to prefix into result
 * It checks if prefix ends by / and adds this token if necessary
 * It also checks that result will fit into PATH_SIZE length
 * @param result the result of the concatenation
 * @param prefix the first part of the resulting path
 * @param suffix the second part of the resulting path
 * @return a pointer to the resulting path, NULL when concatenation failed
 */
char *concat_path(char *result, const char *prefix, const char *suffix) {
    if (result == NULL || prefix == NULL || suffix == NULL) {
        return NULL;
    }
    size_t result_len = strlen(prefix) + strlen(suffix) + 2; 
    if (result_len > PATH_SIZE) {
        return NULL;
    }
    strcpy(result, prefix);
    if (result[strlen(result) - 1] != '/') {
        strcat(result, "/");
    }
    strcat(result, suffix);
    return result;
}
