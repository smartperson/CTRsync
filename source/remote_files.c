// Methods to manipulate remote files over SFTP with libssh2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>

#include <fcntl.h>

#include <sys/types.h>
#include <libssh2.h>
#include <libssh2_sftp.h>

#include <3ds.h>

#include "remote_files.h"
#include "sync_config.h"
#include "local_files.h"
#include "ui.h"

char mem[1024]; //careful about where this gets set and read in the recursion
LIBSSH2_SFTP_ATTRIBUTES fileAttributes;
#define NET_BUFFER_SIZE 1*32*1024    // SFTP seems to be very fast with a 40KiB buffer
#define FILE_BUFFER_SIZE 1*64*1024 // 32KB seems to work. 1MiB supposedly optimizes file I/O perf
#define RING_BUFFER_SIZE 1*1536*1024 // 1.5MiB because reasons
#define STDIO_BUFFER_SIZE 2*1024*1024

FILE *localFile;
intmax_t totalBytes;
intmax_t fileSize;
LightLock ringBufferLock;
char *_stdio_file_buf;

void rf_init() {
    rf_netBuffer = (char *)malloc(NET_BUFFER_SIZE);
    rf_fileBuffer = (char *)malloc(FILE_BUFFER_SIZE);
    rf_ringBuffer = ringbuf_new(RING_BUFFER_SIZE);
    _stdio_file_buf = (char *)malloc(STDIO_BUFFER_SIZE);
    LightLock_Init(&ringBufferLock);
}

void rf_free() {
    free(rf_netBuffer);
    free(rf_fileBuffer);
    free(_stdio_file_buf);
    ringbuf_free(&rf_ringBuffer);
}

void rf_thread_file_write() {
    size_t writtenBytes = 0;
    if (fileSize == 0) return;
    while (writtenBytes < fileSize) {
        size_t ringBytes = ringbuf_bytes_used(rf_ringBuffer);
        if ((ringBytes >= FILE_BUFFER_SIZE) || (ringBytes == (fileSize - writtenBytes))) {
            size_t bytesToWrite = (ringBytes < FILE_BUFFER_SIZE) ? ringBytes: FILE_BUFFER_SIZE;
            // LightLock_Lock(&ringBufferLock);
            ringbuf_memcpy_from(rf_fileBuffer, rf_ringBuffer, bytesToWrite);
            // LightLock_Unlock(&ringBufferLock);
            writtenBytes += fwrite(rf_fileBuffer, 1, bytesToWrite, localFile);
            usleep(10000);
        } else {
            // printFormat(0, "** rftfw waiting 65, ringBytes used %d fs %d wb %d\n", ringBytes, fileSize, writtenBytes);
            usleep(10000);
        }
    }
}

void download_file(LIBSSH2_SFTP *sftp_session, char *absoluteRemotePath, char *relativePath) {
    libssh2_sftp_stat(sftp_session, absoluteRemotePath, &fileAttributes);
    totalBytes = 0;
    fileSize = fileAttributes.filesize;
    LIBSSH2_SFTP_HANDLE *remoteHandle = libssh2_sftp_open(sftp_session, absoluteRemotePath, LIBSSH2_FXF_READ, 0);
    printFormat(0, "%s\n\x1b[s", relativePath);
    if (!remoteHandle) {
        printFormat(0,  "Failed to read file.\n", absoluteRemotePath);
        return;
    }

    localFile = get_local_file_handle(relativePath, "wb");
    setvbuf(localFile, _stdio_file_buf, _IOFBF, STDIO_BUFFER_SIZE);
    int bytesRead;
    Thread fileThread = threadCreate(rf_thread_file_write, NULL, 80*1024, 40, -1, false);
    while((bytesRead = libssh2_sftp_read(remoteHandle, rf_netBuffer, NET_BUFFER_SIZE)) > 0) {
        //LightLock_Lock(&ringBufferLock);
        size_t freeBytes = ringbuf_bytes_free(rf_ringBuffer);
        //LightLock_Unlock(&ringBufferLock);
        while (freeBytes < bytesRead) {
            usleep(10000);
            //LightLock_Lock(&ringBufferLock);
            freeBytes = ringbuf_bytes_free(rf_ringBuffer);
            // printFormat(0, "df, %d free %d read %d size\n", freeBytes, bytesRead, fileSize);
            //LightLock_Unlock(&ringBufferLock);
        }
        // LightLock_Lock(&ringBufferLock);
        ringbuf_memcpy_into(rf_ringBuffer, rf_netBuffer, bytesRead);
        // LightLock_Unlock(&ringBufferLock);
        totalBytes += bytesRead;
        ui_printXferStats(fileSize, totalBytes, bytesRead);
    }
    threadJoin(fileThread, U64_MAX);
    threadFree(fileThread);
    ringbuf_reset(rf_ringBuffer);
    fclose(localFile);
    libssh2_sftp_close(remoteHandle);
    printFormat(0, "\n");
}

void fetch_remote_directory(LIBSSH2_SFTP *sftp_session, char *dirPath) {
    int rc; //return code used for all calls in this stack layer
    char *fullRemotePath;
    int fullRemotePathLength = 0;
    if (strlen(dirPath) > 0) {
        fullRemotePathLength = snprintf(NULL, 0, "%s%s/", remoteSharedPath(), dirPath) +1;
        fullRemotePath = (char *)malloc(fullRemotePathLength);
        snprintf(fullRemotePath, fullRemotePathLength, "%s%s/", remoteSharedPath(), dirPath);
    } else {
        fullRemotePath = strdup(remoteSharedPath());
    }
    
    LIBSSH2_SFTP_HANDLE *sftp_handle =
        libssh2_sftp_opendir(sftp_session, fullRemotePath);
    
    if (!sftp_handle) {
        printFormat(0, "Unable to open remote SFTP directory: %ld\n",
                libssh2_sftp_last_error(sftp_session));
        return;
    }
    
    //keep calling until we get 0 to read every file in the list
    rc = libssh2_sftp_readdir(sftp_handle, mem, sizeof(mem), &fileAttributes);
    while(rc > 0) {
        if (remote_cancelOperation) break;
        mem[rc]='\0'; //need null-terminated for scanf, which readdir doesn't guarantee
        // printf("\nFound contents of length %d flags %lu dir? %d\n", rc, fileAttributes.flags, LIBSSH2_SFTP_S_ISDIR(fileAttributes.permissions));
        int newSubdirectoryLength = snprintf(NULL, 0, "%s/%s", dirPath, mem)+1; //Passing in 0 for length does not include \0 in the returned character count, so we add 1 here.
        char *remoteSubdirectory = (char *)malloc(newSubdirectoryLength);
        snprintf(remoteSubdirectory, newSubdirectoryLength, "%s/%s", dirPath, mem);
        int newAbsolutePathLength = snprintf(NULL, 0, "%s/%s", fullRemotePath, mem)+1;
        char *newAbsolutePath = (char *)malloc(newAbsolutePathLength);
        snprintf(newAbsolutePath, newAbsolutePathLength, "%s/%s", fullRemotePath, mem);
        if (LIBSSH2_SFTP_S_ISREG(fileAttributes.permissions)) {
            download_file(sftp_session, newAbsolutePath, remoteSubdirectory);
            // printf("\nDownloaded file.\n");
        } else if (LIBSSH2_SFTP_S_ISDIR(fileAttributes.permissions)) {
            if ( !((strncmp(mem, ".", rc) == 0) || (strncmp(mem, "..", rc) == 0) || (strncmp(mem, ".DS_Store", rc) == 0)) ) {
                create_local_directory(remoteSubdirectory);
                // printf("Looking into dir %s\n", remoteSubdirectory);
                fetch_remote_directory(sftp_session, remoteSubdirectory);
            }
        }
        free(newAbsolutePath);
        free(remoteSubdirectory);
        
        rc = libssh2_sftp_readdir(sftp_handle, mem, sizeof(mem), &fileAttributes);
    }
    free(fullRemotePath); //should be safe to free after we've opened this path and read it all
    
    if (rc < 0) {
        //char *errorMessage;
        //int errorMessageLength;
        //libssh2_session_last_error(session, &errorMessage, &errorMessageLength, 0);
        printFormat(0, "Unable to read dir with SFTP: %ld\n",
                libssh2_sftp_last_error(sftp_session));
        // failExit("opening dir with SFTP failure.\n");
        return;
    }
    libssh2_sftp_close_handle(sftp_handle);
}

void create_remote_directory(LIBSSH2_SFTP *sftp_session, char *dirPath) {
    char *fullRemotePath;
    int fullRemotePathLength = 0;
    if (strlen(dirPath) > 0) {
        fullRemotePathLength = snprintf(NULL, 0, "%s%s/", remoteSharedPath(), dirPath) +1;
        fullRemotePath = (char *)malloc(fullRemotePathLength);
        snprintf(fullRemotePath, fullRemotePathLength, "%s%s/", remoteSharedPath(), dirPath);
    } else {
        fullRemotePath = strdup(remoteSharedPath());
    }
    LIBSSH2_SFTP_HANDLE *sftp_handle =
        libssh2_sftp_opendir(sftp_session, fullRemotePath);
    if(!sftp_handle) { //if handle is null, need to created remote dir
        // printf("created remote directory %s\n", fullRemotePath);
        libssh2_sftp_mkdir(sftp_session, fullRemotePath, 0755);
    } else {
        libssh2_sftp_close_handle(sftp_handle);
    }
}

void rf_thread_file_read() {
    size_t readBytes = 0;
    if (fileSize == 0) return;
    while ((readBytes = fread_unlocked(rf_fileBuffer, 1, FILE_BUFFER_SIZE, localFile))) {
    // while ((readBytes = FILE_BUFFER_SIZE)) {
//    int fileNum = fileno(localFile);
//    while ((readBytes = read(fileNum, rf_fileBuffer, FILE_BUFFER_SIZE))) {
        size_t freeBytes = ringbuf_bytes_free(rf_ringBuffer);
        while (freeBytes < readBytes) {
            usleep(5000);
            //LightLock_Lock(&ringBufferLock);
            freeBytes = ringbuf_bytes_free(rf_ringBuffer);
            //LightLock_Unlock(&ringBufferLock);
        }
        LightLock_Lock(&ringBufferLock);
        ringbuf_memcpy_into(rf_ringBuffer, rf_fileBuffer, readBytes);
        LightLock_Unlock(&ringBufferLock);
    }
    
}

void push_local_file(LIBSSH2_SFTP *sftp_session, char *dirPath) {
    struct stat fileInfo;
    local_path_stat(dirPath, &fileInfo);
    fileSize = (intmax_t)fileInfo.st_size;

    char *fullRemotePath;
    int fullRemotePathLength = 0;
    fullRemotePathLength = snprintf(NULL, 0, "%s%s", remoteSharedPath(), dirPath) +1;
    fullRemotePath = (char *)malloc(fullRemotePathLength);
    snprintf(fullRemotePath, fullRemotePathLength, "%s%s", remoteSharedPath(), dirPath);
    printFormat(0, "%s\n\x1b[s", dirPath);
    LIBSSH2_SFTP_HANDLE *sftp_handle =
        libssh2_sftp_open(sftp_session, fullRemotePath, LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC, 0755);
    
    localFile = get_local_file_handle(dirPath, "rb");
    // setvbuf(localFile, _stdio_file_buf, _IOFBF, STDIO_BUFFER_SIZE);
    
    totalBytes = 0;
    
    if(!sftp_handle) {
        printFormat(0, "Error writing remote file %s", fullRemotePath);
    } else {
        Thread fileThread = threadCreate(rf_thread_file_read, NULL, 80*1024, 43, -1, false);
        size_t bytesToSend;
        size_t availableBytes;
        while(totalBytes < fileSize) {
            bytesToSend = NET_BUFFER_SIZE;
            availableBytes = ringbuf_bytes_used(rf_ringBuffer);
            while((availableBytes < NET_BUFFER_SIZE)) {
                if (availableBytes == (fileSize - totalBytes)) {
                    bytesToSend = fileSize - totalBytes;
                    // printFormat(0, "! fs %lu tb %lu bts %lu\n", fileSize, totalBytes, bytesToSend);
                    break;
                }
                usleep(5000);
                availableBytes = ringbuf_bytes_used(rf_ringBuffer);
            }
            LightLock_Lock(&ringBufferLock);
            ringbuf_memcpy_from(rf_netBuffer, rf_ringBuffer, bytesToSend);
            LightLock_Unlock(&ringBufferLock);
            size_t bytesWritten = 0;
            char *rf_netBuffer_pos = rf_netBuffer;
            while((bytesWritten = libssh2_sftp_write(sftp_handle, rf_netBuffer_pos, bytesToSend)) > 0) {
                rf_netBuffer_pos += bytesWritten;
                bytesToSend -= bytesWritten;
                totalBytes += bytesWritten;
                // printFormat(0, "totalBytes %lu\n", totalBytes);
            }
            // totalBytes += bytesToSend;
            ui_printXferStats(fileSize, totalBytes, bytesToSend);
        }
        libssh2_sftp_close_handle(sftp_handle);
        threadJoin(fileThread, U64_MAX);
        threadFree(fileThread);
        ringbuf_reset(rf_ringBuffer);
    }
    free(fullRemotePath);
    fclose(localFile);
    printFormat(0, "\n");
}

void push_local_directory(LIBSSH2_SFTP *sftp_session, char *dirPath) {
    DIR *parentDirectory = get_local_dir_handle(dirPath);
    //struct dirent *prev_dir = NULL;
    struct dirent *dir = NULL;
    if (parentDirectory)
    {
        //prev_dir = malloc(sizeof(struct dirent) + NAME_MAX + 1);
        while ((dir = readdir(parentDirectory)) != NULL)
        {
            if (remote_cancelOperation) break;
            char *filePath = copy_concatenated_path(dirPath, dir->d_name);
            struct stat path_stat;
            local_path_stat(filePath, &path_stat);
            if (S_ISREG(path_stat.st_mode)) {
                // push the file
                push_local_file(sftp_session, filePath);
            } else if (S_ISDIR(path_stat.st_mode)) {
                // create the dir on remote, then push everything in it up, recursively
                create_remote_directory(sftp_session, filePath);
                push_local_directory(sftp_session, filePath);
            }
            free(filePath);
        }
        closedir(parentDirectory);
        //free(prev_dir);
    }
}