// Methods to manipulate remote files over SFTP with libssh2
#include <libssh2.h>
#include <libssh2_sftp.h>
#include "ringbuf.h"

#ifndef REMOTEFILES_H
#define REMOTEFILES_H
bool remote_cancelOperation; //set it to true to discontinue the network op from another thread
bool remote_isRunning;
struct ringbuf_t *rf_ringBuffer;
char *rf_fileBuffer;
char *rf_netBuffer;
#endif /* !REMOTEFILES_H */

void rf_init();
void rf_free();
void fetch_remote_directory(LIBSSH2_SFTP *sftp_session, char *dirPath);
void create_remote_directory(LIBSSH2_SFTP *sftp_session, char *dirPath);
void push_local_directory(LIBSSH2_SFTP *sftp_session, char *dirPath);
void push_local_file(LIBSSH2_SFTP *sftp_session, char *dirPath);