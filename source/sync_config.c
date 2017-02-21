// #include <stdio.h>
#include <libconfig.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <3ds.h>

#include "sync_config.h"
#include "ui.h"

config_t cfg, *cf;
config_setting_t *sync_pairs;
LightLock consoleLock;

config_t *loadConfig() {
    LightLock_Init(&consoleLock);
    cf = &cfg;
    config_init(cf);
    if (!config_read_file(cf, "/3ds/CTRSync/config.cfg")) {
        /* fprintf(stdout, "%s:%d - %s\n",
        config_error_file(cf),
        config_error_line(cf),
        config_error_text(cf));*/
        config_destroy(cf);
        return NULL;
    }
    sync_pairs = config_lookup(cf, "sync_pairs");
    return cf;
}

const char *config_hostname() {
    const char *hostname;
    if (config_lookup_string(cf, "host", &hostname)) {
        return hostname;
    }
    return "no host listed";
}

int config_hostport() {
    int port;
    if (config_lookup_int(cf, "port", &port)) {
        return port;
    }
    return 22;    
}

const char *config_username() {
    const char *username;
    if (config_lookup_string(cf, "username", &username)) {
        return username;
    }
    return "root";    
}

const char *config_privatekey() {
    const char *privatekey;
    if (config_lookup_string(cf, "privatekey", &privatekey)) {
        return privatekey;
    }
    return "/3ds/CTRSync/id_3ds";
}

const char *config_publickey() {
    const char *publickey;
    if (config_lookup_string(cf, "publickey", &publickey)) {
        return publickey;
    }
    return "/3ds/CTRSync/id_3ds.pub";
}

int config_sync_pair_count() {
    if (sync_pairs) return config_setting_length(sync_pairs);
    return 0;
}
const char *config_sync_local_path(int index) {
    config_setting_t *sync_pair = config_setting_get_elem(sync_pairs, index);
    const char *local_path;
    if (config_setting_lookup_string(sync_pair, "local", &local_path))
        return local_path;
    return NULL;
}
const char *config_sync_remote_path(int index) {
    config_setting_t *sync_pair = config_setting_get_elem(sync_pairs, index);
    const char *remote_path;
    if (config_setting_lookup_string(sync_pair, "remote", &remote_path))
        return remote_path;
    return NULL;
}

char* localSharedPath() {
    return config_sync_local_path(getSelectedOption());
}

char* remoteSharedPath() {
    return config_sync_remote_path(getSelectedOption());
}

void printFormat(int toInterface, const char *string, ...) {
    LightLock_Lock(&consoleLock);
    if (toInterface) {
        consoleSelect(&interfaceConsole);
    } else {
        consoleSelect(&loggingConsole);
    }
    va_list argptr;
    va_start(argptr, string);
    vprintf(string, argptr);
    va_end(argptr);
    fflush(stdout);
    LightLock_Unlock(&consoleLock);
}

void printFormatArgs(int toInterface, const char *string, va_list argptr) {
    LightLock_Lock(&consoleLock);
    if (toInterface) {
        consoleSelect(&interfaceConsole);
    } else {
        consoleSelect(&loggingConsole);
    }
    vprintf(string, argptr);
    fflush(stdout);
    LightLock_Unlock(&consoleLock);
}