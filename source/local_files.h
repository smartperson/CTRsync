void create_local_directory(char *relativePath);
FILE *get_local_file_handle(char *relativePath, char *mode);
DIR *get_local_dir_handle(char *dirPath);
char *copy_concatenated_path(char *basePath, char *addedComponent);
char *copy_absolute_local_path(char *relativePath);
void local_path_stat(char *relativePath, struct stat *path_stat);