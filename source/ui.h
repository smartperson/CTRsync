//don't send any newlines for setStatus, or bad things will happen.
void setTitle(const char* string);
void ui_setInstructions(const char *string);
bool ui_toggleXferMode();
void setStatus(char *string, ...);
void ui_printXferStats(intmax_t fileSize, intmax_t totalBytesXfer, int bytesRead);
void ui_printError(const char *string);

void setOption(int index, bool isLocal, const char *string);
void initSelectedOption();
int getSelectedOption();
void setSelectedOptionNext();
void setSelectedOptionPrevious();