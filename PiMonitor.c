/* PiMonitor.c -- Monitors various analog & Digital channnels and spits them out on a UDP socket */
/* Based on adc.c from ABElectronics and an example bit of code for UDP Broadcast		 */
/* Also include code from wiringPi to handle the interupts to measure windspeed			*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>  /* for sockaddr_in */
#include <unistd.h>     /* for close() */
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <wiringPi.h>
#include <time.h>

#include "PiMonitor.h"

/* MAIN LOOP */
int main(int argc, char *argv[]) 
{
int i;
float val;
float calVal;
int channel;
int retValue;

char sendString[255];                 /* String to broadcast */
unsigned int sendStringLen;       /* Length of string to broadcast */
/* Could use a struct like for channels but re-suing old code */
char *configkeys[MAX_CONFIG_LINES];          /* config file entries */
char *configvalues[MAX_CONFIG_LINES];          /* config file entries */
char *configfile="/etc/pimonitor/pimonitor.conf";
char *channelsfile="/etc/pimonitor/channels.conf";
char *logfile;
FILE *logfilepointer;

int numChannels=0;
Channels aChannels[9];

for(i=1;i<argc;++i)
	{
	if(strcmp(argv[i],"--debug") == 0)
		{
		if(argc<=(++i)) DieWithError("Useage: PiMonitor --debug <level>  Please include an integer debug level >= 1\n");
		}
	debuglevel = atoi(argv[i]);
	}
		

/* Allocate some memory for our array of pointers */
for(i = 0;i < MAX_CONFIG_LINES;++i)
        {
        configkeys[i]=malloc(MAX_STRING_LENGTH);
        configvalues[i]=malloc(MAX_STRING_LENGTH);
        }

/* Read config file - not a lot here */
readconfig(configfile,configkeys,configvalues);

/* Allocate config file */
for(i = 0 ; i < MAX_CONFIG_LINES ; ++i)
        {
        if(strcmp(configkeys[i],"BroadcastIP") == 0 )
                strcpy(broadcastIP,configvalues[i]);
        if(strcmp(configkeys[i],"BroadcastPort") == 0 )
                broadcastPort = atoi(configvalues[i]);
	if(strcmp(configkeys[i],"LogFile") == 0 ) 
		logfile = configvalues[i];
        }


/* read the config file for channels (usually, channels.conf) */
numChannels = readchannels(channelsfile,aChannels);
if(debuglevel>0) printf("Number of Channels = %i\n",numChannels);


/* Just set this to something - first reading will be off, but not important */
retValue = gettimeofday(&lastPulseTime,NULL);

int windSpeedPin = 0;

/* Setup WiringPi */
retValue = wiringPiSetup();
if(debuglevel>0) printf("wiringPiSetup() - %i\n",retValue);

pullUpDnControl(windSpeedPin,PUD_UP);

/* register call back function for measuring wind speed */
retValue = wiringPiISR(windSpeedPin,INT_EDGE_FALLING,*measureWindSpeed);
if(debuglevel>0) printf("wiringPiISR() - %i\n",retValue);

if(debuglevel>0) printf("Broadcasting on IP: %s on Port %i\n",broadcastIP,broadcastPort);

/* Create socket for sending/receiving datagrams */
if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

/* Set socket to allow broadcast */
broadcastPermission = 1;
if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0)
        DieWithError("setsockopt() failed");

/* Construct local address structure */
memset(&broadcastAddr, 0, sizeof(broadcastAddr));   /* Zero out structure */
broadcastAddr.sin_family = AF_INET;                 /* Internet address family */
broadcastAddr.sin_addr.s_addr = inet_addr(broadcastIP);/* Broadcast IP address */
broadcastAddr.sin_port = htons(broadcastPort);         /* Broadcast port */

while(1)
	{
	for(channel=1;channel<=numChannels;++channel)
		{
		val = getadc(aChannels[channel].ADC,aChannels[channel].ADC_CHANNEL,aChannels[channel].divisor);
		calVal = (val*aChannels[channel].multiplier)+aChannels[channel].offset;
		if(debuglevel>0) printf ("Channel:%d String:%s Value:%2.4f Calibrated Value: %4.2f\n",channel,aChannels[channel].broadcastName,val,calVal);  
		sendStringLen = sprintf(sendString,"%s %.2f",aChannels[channel].broadcastName,(val*aChannels[channel].multiplier)+aChannels[channel].offset);

		/* Broadcast sendString in datagram to clients once */
		if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) != sendStringLen)
       			DieWithError("sendto() sent a different number of bytes than expected"); 
		}
	sleep(1);
	}
}


float getadc(unsigned int adc,unsigned int adc_channel, int varDivisor) {
unsigned int fh,dummy;
float val;
__u8  res[4];

// setup multiplier based on input voltage range and divisor value
float varMultiplier = (2.4705882/varDivisior)/1000;

// open /dev/i2c-0 for version 1 Raspberry Pi boards
// open /dev/i2c-1 for version 2 Raspberry Pi boards
fh = open("/dev/i2c-1", O_RDWR);
ioctl(fh,I2C_SLAVE,adc);
// send request for channel
i2c_smbus_write_byte (fh, adc_channel);
usleep (50000);
// read 4 bytes of data
i2c_smbus_read_i2c_block_data(fh,adc_channel,4,res);
// loop to check new value is available and then return value
while (res[3] & 128)
	{
	// read 4 bytes of data
	i2c_smbus_read_i2c_block_data(fh,adc_channel,4,res);
	}
usleep(50000);
close (fh);

  // shift bits to product result
  dummy = ((res[0] & 0b00000001) << 16) | (res[1] << 8) | res[2];

if(debuglevel>0) printf("Dummy: %i %x %x %x\n",dummy,res[0],res[1],res[2]);

// check if positive or negative number and invert if needed
//  if (res[0]>=128) dummy = ~(0x020000 - dummy);
if (res[0]>=128) dummy = 0;
 
//if(debuglevel>0) printf("Dummy: %i %x %x %x\n",dummy,res[0],res[1],res[2]);

  val = dummy * varMultiplier;
  return val;
}

/* This is called on a GPIO interupt */
void measureWindSpeed()
{
if(pulseCounter < PULSE_COUNTS)
	{
	++pulseCounter;
	if(debuglevel>0) printf("Pulse Counter = %i\n",pulseCounter);
	return;
	}
struct timeval now;
gettimeofday(&now,NULL);
char sendString[255];                 /* String to broadcast */
unsigned int sendStringLen;       /* Length of string to broadcast */
double val;

int useconds = (now.tv_sec*1000000+now.tv_usec) - (lastPulseTime.tv_sec*1000000+lastPulseTime.tv_usec);


val = (double) (1000000*PULSE_COUNTS)/useconds;

sendStringLen = sprintf(sendString,"WindSpeed %.2f",val);

/* Broadcast sendString in datagram to clients once */
if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) != sendStringLen)
	DieWithError("sendto() sent a different number of bytes than expected"); 

if(debuglevel>0) printf("Avg Time Diff = %i\n",useconds/PULSE_COUNTS);
if(debuglevel>0) printf("Freq = %.2f\n",val);

lastPulseTime.tv_sec = now.tv_sec;
lastPulseTime.tv_usec = now.tv_usec;

pulseCounter = 0;
}

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
        if(debuglevel>0) printf("Line %d: Key=%s Value=%s\n",linenum,key,value); 
        arrayloc++;
        }
return arrayloc-1;
}

int readchannels(char *configfile, Channels channel[])
{
char line[MAX_STRING_LENGTH];
int linenum=0;
int arrayloc=1;
FILE *file;
int len=0;

if(debuglevel>0) printf("Channels File %s\n",configfile);

file = fopen(configfile,"r");

if(file == NULL) DieWithError("Channels file is NULL");

while(fgets(line,MAX_STRING_LENGTH,file) !=NULL)
        {
        char key[MAX_STRING_LENGTH],value1[MAX_STRING_LENGTH],value2[MAX_STRING_LENGTH],value3[MAX_STRING_LENGTH],value4[MAX_STRING_LENGTH],value5[MAX_STRING_LENGTH];
        len = strlen(line);
        line[len-1]='\0';

        linenum++;

        if(line[0] == '#' || line[0] == '\n') continue;

        if(sscanf(line, "%s %s %s %s %s %s",key,value1,value2,value3,value4,value5) != 6)
                {
                fprintf(stderr,"Syntax Error in file %s at line %d\n",configfile,linenum);
                continue;
                }
        strcpy(channel[arrayloc].broadcastName,key);
	channel[arrayloc].multiplier = atof(value1);
	channel[arrayloc].ADC = (unsigned int)strtol(value2,NULL,0);
	channel[arrayloc].ADC_CHANNEL = (unsigned int)strtol(value3,NULL,0);
	channel[arrayloc].divisor = atoi(value4);
	channel[arrayloc].offset = atoi(value5);
        if(debuglevel>0) printf("Line %d: Key=%s Value1=%s Value2=%s Value3=%s Value4=%i Value5=%i\n",linenum,key,value1,value2,value3,value4,value5); 
	if(debuglevel>0) printf("Converted: Multiplier %.2f ADC:%i ADC_CHANNEL:%i Divisor:%i Offset:%i\n",channel[arrayloc].multiplier,channel[arrayloc].ADC,channel[arrayloc].ADC_CHANNEL,channel[arrayloc].divisor,channel[arrayloc].offset);
        arrayloc++;
        }
return arrayloc-1;
}

