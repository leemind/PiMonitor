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

#include "BroadcastCommon.h"
#include "PiMonitor.h"

/* MAIN LOOP */
int main(int argc, char **argv) 
{
int i;
float val;
int channel;
int retValue;
// setup multiplier based on input voltage range and divisor value
varMultiplier = (2.4705882/varDivisior)/1000;

char sendString[255];                 /* String to broadcast */
unsigned int sendStringLen;       /* Length of string to broadcast */
char *configkeys[MAX_CONFIG_LINES];          /* config file entries */
char *configvalues[MAX_CONFIG_LINES];          /* config file entries */
char *configfile="/etc/pimonitor/pimonitor.conf";
char *channelsfile="/etc/pimonitor/channels.conf";
char *logfile;
FILE *logfilepointer;

int numChannels;
Channels aChannels[9];

numChannels = readChannels(channelsfile,aChannels);

retValue = gettimeofday(&lastPulseTime,NULL);

int windSpeedPin = 0;

/* Setup WiringPi */
retValue = wiringPiSetup();
printf("wiringPiSetup() - %i\n",retValue);

pullUpDnControl(windSpeedPin,PUD_UP);

/* register call back function for measuring wind speed */
retValue = wiringPiISR(windSpeedPin,INT_EDGE_FALLING,*measureWindSpeed);
printf("wiringPiISR() - %i\n",retValue);



/* TEMP HARDCODE! */
broadcastPort = 2345;
broadcastIP = "255.255.255.255";
aChannels[1].broadcastName = "BatteryVoltage";
aChannels[1].multiplier = 1;
aChannels[8].broadcastName = "WindDirection";
aChannels[8].multiplier = 1;

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
	for(channel=8;channel<=numChannels;++channel)
		{
		val = getadc(channel);
		printf ("Channel: %d  - %2.4fV\n",channel,val);  
		sendStringLen = sprintf(sendString,"%s %.2f",aChannels[channel].broadcastName,val*aChannels[channel].multiplier);

		/* Broadcast sendString in datagram to clients once */
		if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) != sendStringLen)
       			DieWithError("sendto() sent a different number of bytes than expected"); 
		}
	sleep(1);
	}

/*  if (argc>1) channel=atoi(argv[1]);
  if (channel <1|channel>8) channel=1;
  // loop for 500 samples and print to terminal
  for (i=0;i<500;i++){
    val = getadc (channel);
	if (val <= 5.5) {
		printf ("Channel: %d  - %2.4fV\n",channel,val);  
	}
    sleep (0.5);
  }
  return 0; 
*/
}


float getadc (int chn){
  unsigned int fh,dummy, adc, adc_channel;
  float val;
  __u8  res[4];
  // select chip and channel from args
  switch (chn){
    case 1: { adc=ADC_1; adc_channel=ADC_CHANNEL1; }; break;
    case 2: { adc=ADC_1; adc_channel=ADC_CHANNEL2; }; break;
    case 3: { adc=ADC_1; adc_channel=ADC_CHANNEL3; }; break;
    case 4: { adc=ADC_1; adc_channel=ADC_CHANNEL4; }; break;
    case 5: { adc=ADC_2; adc_channel=ADC_CHANNEL1; }; break;
    case 6: { adc=ADC_2; adc_channel=ADC_CHANNEL2; }; break;
    case 7: { adc=ADC_2; adc_channel=ADC_CHANNEL3; }; break;
    case 8: { adc=ADC_2; adc_channel=ADC_CHANNEL4; }; break;
    default: { adc=ADC_1; adc_channel=ADC_CHANNEL1; }; break;
  }
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
  while (res[3] & 128){
	  // read 4 bytes of data
	  i2c_smbus_read_i2c_block_data(fh,adc_channel,4,res);
  }
  usleep(50000);
  close (fh);

  // shift bits to product result
  dummy = ((res[0] & 0b00000001) << 16) | (res[1] << 8) | res[2];

  // check if positive or negative number and invert if needed
  if (res[0]>=128) dummy = ~(0x020000 - dummy);
 
  val = dummy * varMultiplier;
  return val;
}

void measureWindSpeed()
{
if(pulseCounter < PULSE_COUNTS)
	{
	++pulseCounter;
	printf("Pulse Counter = %i\n",pulseCounter);
	return;
	}
struct timeval now;
gettimeofday(&now,NULL);
char sendString[255];                 /* String to broadcast */
unsigned int sendStringLen;       /* Length of string to broadcast */
double val;

int useconds = (now.tv_sec*1000000+now.tv_usec) - (lastPulseTime.tv_sec*1000000+lastPulseTime.tv_usec);


val = (1000000*PULSE_COUNTS)/useconds;

sendStringLen = sprintf(sendString,"WindSpeed %.2f",val);

/* Broadcast sendString in datagram to clients once */
if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) != sendStringLen)
	DieWithError("sendto() sent a different number of bytes than expected"); 

printf("Avg Time Diff = %i\n",useconds/PULSE_COUNTS);
printf("Freq = %.2f\n",val);

lastPulseTime.tv_sec = now.tv_sec;
lastPulseTime.tv_usec = now.tv_usec;

pulseCounter = 0;
}


// #include <stdio.h>  /* for perror() */
// #include <stdlib.h> /* for exit() */
// #include <string.h>
// #include "BroadcastCommon.h"

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
/*      printf("Line %d: Key=%s Value=%s\n",linenum,key,value); */
        arrayloc++;
        }
return arrayloc;
}

int readchannels(char *configfile, Channels *channel)
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
        strcpy(channel[arrayloc].broadcastName,key);
	channel[arrayloc].multipler = atoi(value);
        printf("Line %d: Key=%s Value=%s\n",linenum,key,value); 
        arrayloc++;
        }
return arrayloc;
}

