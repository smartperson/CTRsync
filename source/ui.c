#include <stdarg.h>
#include <string.h>
#include <3ds.h>
#include <time.h>

#include "sync_config.h"
//bottom screen is 40x30 chars
//top screen is 50x30 chars
#define OPTION_OFFSET 2
#define BOTTOM_WIDTH 50
#define OPTION_WIDTH BOTTOM_WIDTH - 5
#define INSTRUCTIONS_ROW 27
#define MODE_ROW 28

#define MODE_PUSH_TEXT "     Mode: \x1b[0;32mPUSH\x1b[0m console files to remote host     "
#define MODE_PULL_TEXT "       Mode: \x1b[0;31mPULL\x1b[0m remote files to console        "

int selectedOption = 0;
int maxOption = 0;
bool _is_push = true;

void setStatus(char *string, ...) {
    printFormat(1, "\x1b[29;0H\x1b[7m\x1b[2K");
    va_list argptr;
    va_start(argptr, string);
    printFormatArgs(1, string, argptr);
    printFormat(1, CONSOLE_RESET);
    va_end(argptr);
    
}

void setTitle(const char* string) {
    printFormat(1, "\x1b[0;0H\x1b[7m\x1b[2K");
    printFormat(1, "%s%s", string, CONSOLE_RESET);
}

void setOption(int index, bool isLocal, const char *string) {
    if (isLocal)
        printFormat(1, "\x1b[%d;1H\x1b[0;32m", index*2+OPTION_OFFSET);
    else
        printFormat(1, "\x1b[%d;1H\x1b[0;31m", index*2+1+OPTION_OFFSET);
    if(strlen(string) > OPTION_WIDTH)
        printFormat(1, "%*.*s..", OPTION_WIDTH, OPTION_WIDTH, string);
    else
        printFormat(1, "%-*s", OPTION_WIDTH, string);
    if (isLocal)
        printFormat(1, "->", CONSOLE_RESET);
    if (index > maxOption) maxOption = index;
}

void _setModeText() {
    printFormat(1, "\x1b[%d;0H\x1b[2K", MODE_ROW);
    if (_is_push)
        printFormat(1, "%s", MODE_PUSH_TEXT);
    else
        printFormat(1, "%s", MODE_PULL_TEXT);    
}

void ui_setInstructions(const char *string) {
    printFormat(1, "\x1b[%d;0H%s", INSTRUCTIONS_ROW, string);
    _setModeText();
}

bool ui_toggleXferMode() {
    _is_push = !_is_push;
    _setModeText();
    return _is_push;
}

int previousPercentage = 0;
time_t _time_prev = 0, _time_cur = 0;
intmax_t _bytes_prev; double xferSpeed;
void ui_printXferStats(intmax_t fileSize, intmax_t bytesReadTotal, int bytesRead) {
    if (_bytes_prev > bytesReadTotal) {
        _bytes_prev = 0;
        _time_prev = 0;
    }
    _time_cur = time(NULL);
    if (_time_prev == 0) _time_prev = _time_cur;
    
    int percentage =  (int)((bytesReadTotal * 100)/fileSize);
    if ((percentage != previousPercentage) || ((_time_cur - _time_prev) >= 1)) {//update the numbers
        printFormat(0, "\x1b[u");
        if (percentage < 25) {
            printFormat(0, CONSOLE_RED);
        } else if (percentage < 75) {
            printFormat(0, CONSOLE_YELLOW);
        } else {
            printFormat(0, CONSOLE_GREEN);
        }
        previousPercentage = percentage;
        if ((_time_cur - _time_prev) >= 1) {
            xferSpeed = (double)(bytesReadTotal - _bytes_prev)/((double)(_time_cur - _time_prev))/1024;
            _bytes_prev = bytesReadTotal;
            _time_prev = _time_cur;
        }
        printFormat(0, "%d%%  (%.2fKB/s)        %s", percentage, xferSpeed, CONSOLE_RESET);
    }
}

void ui_printError(const char *string) {
    printFormat(0, "%s%s%s", CONSOLE_RED, string, CONSOLE_RESET);
}

int getSelectedOption() {
    return selectedOption;
}

void initSelectedOption() {
    selectedOption = 0;
    //printFormat(1, "%s", CONSOLE_RESET);
    printFormat(1, "\x1b[%d;0H%s>%s", OPTION_OFFSET, CONSOLE_WHITE, CONSOLE_RESET);
}

void setSelectedOptionNext() {
    printFormat(1, "\x1b[%d;0H ", selectedOption*2+OPTION_OFFSET);
    selectedOption = (selectedOption+1) % (maxOption+1);
    printFormat(1, "\x1b[%d;0H>", selectedOption*2+OPTION_OFFSET);
}

void setSelectedOptionPrevious() {
    printFormat(1, "\x1b[%d;0H ", selectedOption*2+OPTION_OFFSET);
    selectedOption = selectedOption - 1;
    if (selectedOption < 0) selectedOption += (maxOption+1);
    printFormat(1, "\x1b[%d;0H%s>%s", selectedOption*2+OPTION_OFFSET, CONSOLE_WHITE, CONSOLE_RESET);
}