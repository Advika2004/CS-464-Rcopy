// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "pollLib.h"
#include "window.h"


#define MAXBUF 80
#define HEADER_SIZE 7  // 4 bytes seq#, 2 bytes checksum, 1 byte flag
#define MAX_FILENAME_LEN 100
#define MAX_PAYLOAD_LEN 1400
#define MAX_WINDOW_LEN 4
#define MAX_BUFFER_LEN 2
#define MAX_HEADER_LEN 7
#define ACK_BUFF_SIZE 7
#define CONTROL_BUFF_SIZE 11

//! should this be some length? is it specified somwhere
#define MAX_REMOTE_MACHINE 256 

//Struct for the packet
typedef struct Packet{
    uint32_t sequence_number; 
    uint16_t checksum;
    uint8_t flag;
    uint8_t data[1400]; 
} __attribute__((packed));

//struct to store input
typedef struct RcopyParams {
    // src filename, dest filename, window size, buffer size, error-rate, remote-machine, remote-port.
    char src_filename[MAX_FILENAME_LEN + 1];
    char dest_filename[MAX_FILENAME_LEN + 1];
    int window_size;
    int buffer_size;
    double error_rate; 
    char remote_machine[MAX_REMOTE_MACHINE + 1];
    int remote_port; 
} RcopyParams; 

//void talkToServer(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);
RcopyParams checkArgs(int argc, char * argv[]);
uint8_t* makeFilenamePDUBeforeChecksum(RcopyParams params);
uint16_t calculateFilenameChecksum(uint8_t* buffer);
uint8_t* makeFilenamePDUAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum);
void printFilenamePDU(uint8_t *buffer);
int doSendFilenameState(RcopyParams params, int socketNum, struct sockaddr_in6 * server, uint8_t* buffer);
int doFileValidState(RcopyParams params, int socketNum, struct sockaddr_in6 * server, uint8_t* buffer);
int doGetDataState();
int checkDestFile(RcopyParams params);
void doDoneState(int socketNum, FILE *destFile);
uint8_t* makeFilenameACKBeforeChecksum(uint8_t flag);
uint16_t calculateFilenameChecksumACK(uint8_t* buffer);
uint8_t* makeFilenameACKAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum);
void writePayloadToFile(Packet *packet, FILE *file, RcopyParams params);
uint8_t getPacketFlag(Packet *packet);
uint8_t* makeRRPacketBeforeChecksum(uint32_t localSeq, uint32_t rrSequence);
uint8_t* makeRR_SREJPacketAfterChecksum(uint8_t *buffer, uint16_t calculated_sum);
uint16_t calculateRR_SREJPacketChecksum(uint8_t *buffer);
uint8_t* makeSREJPacketBeforeChecksum(uint32_t localSeq, uint32_t missingPacket);



//global to hold the current file that is open
FILE *curr_file_open;

// make it a global, upon start this is the one given by the command line
int main_server_port;
int current_server_port;