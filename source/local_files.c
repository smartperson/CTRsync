#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "sync_config.h"
#include "local_files.h"

int dirOpenCount = 0;

void create_local_directory(char *relativePath) {
    char *absoluteLocalPath = copy_absolute_local_path(relativePath);
    struct stat dir_stat = {0};
    if (stat(absoluteLocalPath, &dir_stat) == -1) {
        mkdir(absoluteLocalPath, 0755);
    }
    free(absoluteLocalPath);
}

FILE *get_local_file_handle(char *relativePath, char *mode) {
    char *absoluteLocalPath = copy_absolute_local_path(relativePath);
    FILE *localFile = fopen(absoluteLocalPath, mode);
    free(absoluteLocalPath);
    return localFile;
}

DIR *get_local_dir_handle(char *dirPath) {
    char *absoluteLocalPath = copy_absolute_local_path(dirPath);
    DIR *openedDir = opendir(absoluteLocalPath);
    free(absoluteLocalPath);
    return openedDir;
}

char *copy_concatenated_path(char *basePath, char *addedComponent) {
    int newPathLength;
    char *newPath;
    newPathLength = snprintf(NULL, 0, "%s/%s", basePath, addedComponent)+1;
    newPath = (char *)malloc(newPathLength);
    snprintf(newPath, newPathLength, "%s/%s", basePath, addedComponent);
    return newPath;
}

void local_path_stat(char *relativePath, struct stat *path_stat) {
    char *absolutePath = copy_absolute_local_path(relativePath);
    stat(absolutePath, path_stat);
    free(absolutePath);
}

char *copy_absolute_local_path(char *relativePath) { //if you use this, you better free it
    int absoluteLocalLength;
    char *absoluteLocalPath;
    absoluteLocalLength = snprintf(NULL, 0, "%s%s", localSharedPath(), relativePath)+1;
    absoluteLocalPath = (char *)malloc(absoluteLocalLength);
    snprintf(absoluteLocalPath, absoluteLocalLength, "%s%s", localSharedPath(), relativePath);
    return absoluteLocalPath;
}