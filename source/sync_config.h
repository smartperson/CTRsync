#include <libconfig.h>
#include <sys/lock.h>
#include <3ds.h>

config_t *loadConfig();
const char *config_hostname();
int config_hostport();
const char *config_username();
const char *config_privatekey();
const char *config_publickey();
int config_sync_pair_count();
const char *config_sync_local_path(int index);
const char *config_sync_remote_path(int index);

char* localSharedPath(); //Eventually load this info from a config files.
char* remoteSharedPath(); //this, too

PrintConsole loggingConsole;
PrintConsole interfaceConsole;

void printFormat(int toInterface, const char *string, ...);
void printFormatArgs(int toInterface, const char *string, va_list argptr);