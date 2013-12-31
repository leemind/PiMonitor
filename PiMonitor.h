// define adc chips addresses and channel modes
#define ADC_1           0x68
#define ADC_2           0x69
#define ADC_CHANNEL1    0x9C
#define ADC_CHANNEL2    0xBC
#define ADC_CHANNEL3    0xDC
#define ADC_CHANNEL4    0xFC

#define PULSE_COUNTS	10

#define MAX_STRING_LENGTH 256
#define MAX_CONFIG_LINES 25

const float varDivisior = 64; // from pdf sheet on adc addresses and config for 18 bit mode
static float varMultiplier = 0;

static int pulseCounter = 0;
static struct timeval lastPulseTime; 

/* Making these static so I can use in the callback.  Probably a better way... */
static int sock;                         /* Socket */
static struct sockaddr_in broadcastAddr; /* Broadcast address */
static char broadcastIP[MAX_STRING_LENGTH];                /* IP broadcast address */
static unsigned short broadcastPort;     /* Server port */
static int broadcastPermission;          /* Socket opt to set permission to broadcast */

typedef struct Channels { char broadcastName[MAX_STRING_LENGTH]; int multiplier; } Channels;

void measureWindSpeed();
float getadc (int chn);
void DieWithError(char *errorMessage);  /* External error handling function */
int readconfig(char *,char *[],char *[]);
int readchannels(char *,Channels[]);

