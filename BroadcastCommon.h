#define MAX_STRING_LENGTH 256
#define MAX_CONFIG_LINES 25

void DieWithError(char *errorMessage);  /* External error handling function */
int readconfig(char *,char *[],char *[]);

