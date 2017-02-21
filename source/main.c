#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <3ds.h>
#include <libssh2.h>
#include <libssh2_sftp.h>

#include "sync_config.h"
#include "ui.h"
#include "remote_files.h"

#define APP_TITLE "CTRsync 0.0.1 Pre"
#define INSTRUCTIONS_MENU      "      L+R Switch direction   A+B Begin xfer      "
#define INSTRUCTIONS_OPERATING "              B to cancel gracefully             "

#define STACKSIZE (150 * 1024)
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x10000//0x100000

static u32 *SOC_buffer = NULL;
s32 sock = -1, csock = -1;

unsigned long hostaddr = 0xC0A80116;
int auth_pw = 0;

const char *fingerprint;
char *userauthlist;
LIBSSH2_SESSION *session;
int rc;
LIBSSH2_SFTP *sftp_session;
LIBSSH2_SFTP_HANDLE *sftp_handle;
bool push_mode = true;

__attribute__((format(printf,1,2)))
void failExit(const char *fmt, ...);
int runUserMenu();
void runOperatingMenu();
void error_noConfig();

static void kbd_callback(const char *name, int name_len, 
             const char *instruction, int instruction_len, int num_prompts,
             const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
             LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
             void **abstract)
{
/*    int i;
    size_t n;
    char buf[1024];
    (void)abstract;

    fprintf(stdout, "Performing keyboard-interactive authentication.\n");

    fprintf(stdout, "Authentication name: '");
    fwrite(name, 1, name_len, stdout);
    fprintf(stdout, "'\n");

    fprintf(stdout, "Authentication instruction: '");
    fwrite(instruction, 1, instruction_len, stdout);
    fprintf(stdout, "'\n");

    fprintf(stdout, "Number of prompts: %d\n\n", num_prompts);

    for (i = 0; i < num_prompts; i++) {
        fprintf(stdout, "Prompt %d from server: '", i);
        fwrite(prompts[i].text, 1, prompts[i].length, stdout);
        fprintf(stdout, "'\n");

        // responses[i].text = strdup(buf);
        // responses[i].length = n;
        responses[i].text = strdup(password);
        responses[i].length = strlen(password);

        fprintf(stdout, "Response %d from user is '", i);
        fwrite(responses[i].text, 1, responses[i].length, stdout);
        fprintf(stdout, "'\n\n");
    }

    fprintf(stdout,
        "Done. Sending keyboard-interactive responses to server now.\n");
    */
}

//---------------------------------------------------------------------------------
void socShutdown() {
//---------------------------------------------------------------------------------
    if(session)
    {
        libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
        libssh2_session_free(session);
    }
    if (sock) close(sock);
    libssh2_exit();
    // printf("waiting for socExit...\n");
    socExit();
    // fsExit();
}

void threadMain(void *arg)
{
    //printf("thread execution begun\n");
    //unsigned long hostaddr;
    int sock, i, auth_pw = 0;
    struct sockaddr_in sin;
    const char *fingerprint;
    char *userauthlist;
    LIBSSH2_SESSION *session;
    int rc;
    LIBSSH2_SFTP *sftp_session;

    /*
    * The application code is responsible for creating the socket
    * and establishing the connection
    */
    usleep(1000000);
    struct hostent *host_entity = gethostbyname(config_hostname());
    struct in_addr **addr_list;
    if (!host_entity)
        failExit("DNS failed to resolve server hostname");
    //hostaddr = inet_aton(hostent->h_addr);
    addr_list = (struct in_addr **)host_entity->h_addr_list;
    char *hostaddr_a = inet_ntoa(*addr_list[0]);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(config_hostport());
    sin.sin_addr.s_addr = inet_addr(hostaddr_a);

    int connect_status = connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in));
    if (connect_status != 0) {
       failExit("connect: %d %s\n", connect_status, strerror(connect_status));
    }
    setStatus("Connected to host.");
  
    // Set socket non blocking so we can still read input to exit
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
  
    /* Create a session instance
     */
    session = libssh2_session_init();
    setStatus("Session initialized.");
    if(!session)
        failExit("session init: %d %s\n", 0, strerror(0));;
    // libssh2_trace(session, ~(0)); //trace (almost) all the things!  
    /* Since we have set non-blocking, tell libssh2 we are blocking */
    //libssh2_session_set_blocking(session, 1);
    //libssh2_session_flag(session, LIBSSH2_FLAG_COMPRESS, 1);

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    libssh2_session_flag(session, LIBSSH2_FLAG_COMPRESS, 1);
    setStatus("Handshaking");
    rc = libssh2_session_handshake(session, sock);
    if(rc) {
        fprintf(stdout, "Failure establishing SSH session: %d\n", rc);
        failExit("establishing ssh session: %d %s\n", rc, strerror(rc));
    }
    setStatus("ssh handshake successful");
    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    // fprintf(stdout, "Fingerprint: ");
    for(i = 0; i < 20; i++) {
        // fprintf(stdout, "%02X ", (unsigned char)fingerprint[i]);
    }
    // fprintf(stdout, "\n");

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, config_username(), strlen(config_username()));
    // fprintf(stdout, "Authentication methods: %s\n", userauthlist);
    if (strstr(userauthlist, "password") != NULL) {
        auth_pw |= 1;
    }
    if (strstr(userauthlist, "keyboard-interactive") != NULL) {
        auth_pw |= 2;
    }
    if (strstr(userauthlist, "publickey") != NULL) {
        auth_pw |= 4;
    }
    
    if (auth_pw & 4) {
        /* Or by public key */
        if (libssh2_userauth_publickey_fromfile(session, config_username(), config_publickey(), config_privatekey(), NULL)) {
            setStatus("Authentication by public key failed!");
            failExit("authentication by public key\n");
        } else {
            setStatus("Authentication by public key succeeded.");
        }
    } else if (auth_pw & 2) {
        /* We could authenticate via password, but that would require storing a password, or prompting for one, and we'd rather not. */
        /* if (libssh2_userauth_password(session, username, password)) {
            fprintf(stdout, "Authentication by password failed.\n");
            failExit("authentication by password failure\n");
        } */
    } else if (auth_pw & 1) {
        /* Or via keyboard-interactive */
        if (libssh2_userauth_keyboard_interactive(session, config_username(), &kbd_callback) ) {
            fprintf(stdout,
                "\tAuthentication by keyboard-interactive failed!\n");
            failExit("Authentication by keyboard-interactive failed.");
        } else {
            fprintf(stdout,
                "\tAuthentication by keyboard-interactive succeeded.\n");
        }
    } else {
        fprintf(stdout, "No supported authentication methods found!\n");
        failExit("authentication methods not found\n");
    }

    printFormat(0, "libssh2_sftp_init()\n");
    sftp_session = libssh2_sftp_init(session);

    if (!sftp_session) {
        fprintf(stdout, "Unable to init SFTP session\n");
        failExit("init sftp session");
    }
    //create local shared dir if it doesn't exist
    struct stat dir_stat = {0};
    if (stat(localSharedPath(), &dir_stat) == -1) {
        // printf("Creating local directory.");
        mkdir(localSharedPath(), 0755);
    }
    //create remote shared dir if it doesn't exist
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    //assume if the remote stat fails it is because the directory doesn't exist
    if (libssh2_sftp_stat(sftp_session, remoteSharedPath(), &attrs) < 0)
        create_remote_directory(sftp_session, "");
    
    if (push_mode) {
        setStatus("Performing local to host push...");
        push_local_directory(sftp_session, "");
    } else {
        setStatus("Performing host to local pull...");
        fetch_remote_directory(sftp_session, "");
    }
    if (remote_cancelOperation)
        setStatus("Folder sync canceled. START to exit.");
    else
        setStatus("Folder sync complete. START to exit.");
    libssh2_sftp_shutdown(sftp_session);

    libssh2_session_disconnect(session, "Normal shutdown, thank you for playing.");
    libssh2_session_free(session);
    close(sock);
    sock = 0;
    remote_isRunning = false;
    remote_cancelOperation = false;
    threadExit(0);
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//--------------------------------------------------------------------------------
    gfxInitDefault();

    // register gfxExit to be run when app quits
    // this can help simplify error handling
    atexit(gfxExit);
    sdmcWriteSafe(false);
    int ret;

    // allocate buffer for SOC service
    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if(SOC_buffer == NULL) {
        failExit("memalign: failed to allocate\n");
    }
    // Now intialise soc:u service
    if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
        failExit("socInit: 0x%08X\n", (unsigned int)ret);
    }
    // fsInit();
    // register socShutdown to run at exit
    // atexit functions execute in reverse order so this runs before gfxExit
    atexit(socShutdown);
    
    //initialize libssh2, to remain running for app lifetime
    ret = libssh2_init (0);
    if (ret != 0) {
        // fprintf(stdout, "libssh2 initialization failed (%d)\n", rc);
        failExit("libssh2 initialization: %d %s\n", ret, strerror(ret));
    }

    consoleInit(GFX_TOP, &interfaceConsole);
    consoleInit(GFX_BOTTOM, &loggingConsole);
    
    if(!loadConfig()) error_noConfig();
    rf_init(); //set up our networking buffers
    //printFormat(1, "host: %s port: %d\n", config_hostname(), config_hostport());
    //printFormat(1, "public: %s private: %s user: %s\n", config_publickey(), config_privatekey(), config_username());
    setTitle(APP_TITLE);
    ui_setInstructions(INSTRUCTIONS_MENU);
    
    consoleSelect(&loggingConsole);
    for(int i = 0; i < config_sync_pair_count(); i++) {
        setOption(i, true, config_sync_local_path(i));
        setOption(i, false, config_sync_remote_path(i));
    }
    initSelectedOption();
    
    osSetSpeedupEnable(true); //can we make it faster this way?
    APT_SetAppCpuTimeLimit(90); //might be faster still?
    Thread threads[1];
    s32 prio = 0;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    setStatus("         Run a transfer, or START to exit        ");
    while (runUserMenu() >= 0){
        remote_isRunning = true;
        ui_setInstructions(INSTRUCTIONS_OPERATING);
        threads[0] = threadCreate(threadMain, NULL, STACKSIZE, prio-1, -1, true);
        setStatus("Network thread running");
        runOperatingMenu();
        threadJoin(threads[0], U64_MAX);
        // threadFree(threads[0]);
        ui_setInstructions(INSTRUCTIONS_MENU);
        setStatus("         Run a transfer, or START to exit        ");
    }
    rf_free(); //free up buffer memories
    socShutdown();
    // fsExit();
    gfxExit();
    return 0;
}

//---------------------------------------------------------------------------------
void failExit(const char *fmt, ...) {
//---------------------------------------------------------------------------------

    if(session)
    {
      libssh2_session_disconnect(session, "Normal shutdown, thank you for playing.");
      libssh2_session_free(session);
    }
    libssh2_exit();

	if(sock>0) close(sock);
	if(csock>0) close(csock);

	va_list ap;

	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	
  while (aptMainLoop()) {
   gspWaitForVBlank();
   hidScanInput();
  
   u32 kDown = hidKeysDown();
   if (kDown & KEY_B) exit(0);
  }
}

// returns selected option, or -1 is user has chosen to exit
int runUserMenu() {
    u32 kDownOld = 0, kHeldOld = 0;//, kUpOld = 0; //In these variables there will be information about keys detected in the previous frame
    while (aptMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();
        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u32 kDown = hidKeysDown();
        //hidKeysHeld returns information about which buttons have are held down in this frame
        u32 kHeld = hidKeysHeld();
        //hidKeysUp returns information about which buttons have been just released
        //u32 kUp = hidKeysUp();
        if (kDown & KEY_START) {
            return -1;
        } // break in order to return to hbmenu
        if ((kDownOld & KEY_DDOWN) && !(kDown & KEY_DDOWN) && (kHeld & KEY_DDOWN)) {
            setSelectedOptionNext();
        }
        if ((kDownOld & KEY_DUP)   && !(kDown & KEY_DUP)   && (kHeld & KEY_DUP)) {
            setSelectedOptionPrevious();
        }
    
        if (kHeld == (KEY_A | KEY_B)) {
            return getSelectedOption();
        }
        if ((kHeld == (KEY_L | KEY_R)) && (kHeldOld != (KEY_L | KEY_R))) {
            push_mode = ui_toggleXferMode();
            
        }

        //Set keys old values for the next frame
        kDownOld = kDown;
        kHeldOld = kHeld;
        //kUpOld = kUp;

        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();

        //Wait for VBlank
        gspWaitForVBlank();
    }
    return -1;
}

void runOperatingMenu() {
    while (aptMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();
        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u32 kDown = hidKeysDown();
        //hidKeysHeld returns information about which buttons have are held down in this frame
        if (kDown & KEY_B) { //B to cancel the operation
            setStatus("Canceling...");
            remote_cancelOperation = true;
            return;
        }
        if (!remote_isRunning) break;

        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();

        //Wait for VBlank
        gspWaitForVBlank();
    }
}

void error_noConfig() {
    ui_printError("Could not load configuration file at /3ds/CTRsync/config.cfg.\n");
    // make our folder if it's missing
    struct stat dir_stat = {0};
    if (stat("/3ds", &dir_stat) == -1) {
        // printf("Creating local directory.");
        mkdir("/3ds", 0755);
    }
    if (stat("/3ds/CTRsync", &dir_stat) == -1) {
        // printf("Creating local directory.");
        mkdir("/3ds/CTRsync", 0755);
    }
    
    // check if our sample cfg exists. If not recreate it.
    if (stat("/3ds/CTRsync/config.cfg.example", &dir_stat) == -1) {
        ui_printError("Use /3ds/CTRsync/config.cfg.example as a template for your own config, then relaunch CTRsync.\n");
        FILE *configExampleFile = fopen("/3ds/CTRsync/config.cfg.example", "w");
        const char *configExampleContents = "/* Copy this config file and rename it to config.cfg in the /3ds/CTRsync/ folder.\n\
 * If CTRSync does not find a valid config file, it will recreate this example one.\n\
 */\n\
host = \"192.168.1.1\"    // Put your computer host here. It can be a domain name or IPv4 IP.\n\
port = 22               // If missing we will assume this as default.\n\
username = \"your_user\"\n\
\n\
private_key = \"/3ds/CTRsync/id_3ds\"     // There is no support for stored passwords or keyboard input.\n\
public_key = \"/3ds/CTRsync/id_3ds.pub\"  // ...and there probably never will be.\n\
\n\
// You can place up to 5 pairs of folders to synchronize in this list.\n\
// More than that may not display in the CTRsync UI.\n\
sync_pairs = (\n\
    {   local: \"/JKSV\";\n\
        remote: \"/Users/your_user/Dropbox/3DS/saves\";\n\
    },\n\
    {   local: \"/luma/dumps\";\n\
        remote: \"/Users/your_user/Dropbox/3DS/luma_dumps\";\n\
    }\n\
)\n\
";
        
        fprintf(configExampleFile, "%s", configExampleContents);
        fclose(configExampleFile);
    }
}