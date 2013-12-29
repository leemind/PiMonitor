#include <stdio.h>  /* for perror() */
#include <stdlib.h> /* for exit() */
#include <string.h>
#include "BroadcastCommon.h"


void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int readconfig(char *configfile,char *keys[],char *values[])
{
char line[MAX_STRING_LENGTH];
int linenum=0;
int arrayloc=0;
/*char configfile[]="/etc/alerter/broadcastsender.conf"; */
FILE *file;
int len=0;

file = fopen(configfile,"r");

if(file == NULL) exit(1);

while(fgets(line,MAX_STRING_LENGTH,file) !=NULL)
        {
        char key[MAX_STRING_LENGTH],value[MAX_STRING_LENGTH];

        len = strlen(line);

        line[len-1]='\0';

        linenum++;

        if(line[0] == '#' || line[0] == '\n') continue;

        if(sscanf(line, "%s %[^\n]s",key,value) != 2)
                {
                fprintf(stderr,"Syntax Error in file %s at line %d\n",configfile,linenum);
                continue;
                }
        strcpy(keys[arrayloc],key);
        strcpy(values[arrayloc],value);
/*      printf("Line %d: Key=%s Value=%s\n",linenum,key,value); */
        arrayloc++;
        }
return arrayloc;
}

