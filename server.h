#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <asm-generic/fcntl.h>
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include <signal.h>   //For signal() and SIGCHLD
#include <sys/wait.h> // For waitpid()
#include "pollLib.h"
#include "window.h"

#define MAXBUF 80
#define ACK_BUFF_SIZE 7

#define MAX_FILENAME_LEN 100
#define MAX_PAYLOAD_LEN 1400
#define MAX_WINDOW_LEN 4
#define MAX_BUFFER_LEN 2
#define MAX_HEADER_LEN 7

typedef struct ServerParams{
    double error_rate;
    int port_number;
}ServerParams;

void processClient(int socketNum);
ServerParams checkArgs(int argc, char *argv[]);
int doGetFilenameState(char* filename, struct sockaddr_in6 * client, socklen_t clientAddrLen);
int doSendDataState(char *filename, uint16_t buffer_size, uint32_t window_size, int child_server_socket, struct sockaddr_in6 *client, socklen_t clientAddrLen);
int doDoneState();
int doWaitOnAckState();
int doWaitOnEOFAckState();
uint32_t getSequenceFromBuffer(uint8_t *buffer);
void startFSM(char* filename, uint16_t buffer_size, uint32_t window_size, struct sockaddr_in6 * client, socklen_t clientAddrLen, int main_server_socket, ServerParams serverParam);
uint8_t* makeTalkHereNowBeforeChecksum();
uint8_t* makeVALIDFilenameACKBeforeChecksum();
uint8_t* makeFilenameACKAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum);
uint8_t* makeERRORFilenameACKBeforeChecksum();
uint16_t calculateFilenameChecksumACK(uint8_t* buffer);
uint8_t* makeTalkHereNowAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum);
uint16_t calculateFilenameChecksumFILENAME(uint8_t* buffer);
int doDoneState(int child_server_socket);
void handleZombies(int sig);
uint8_t* getDataChunk(FILE *fp, int buffer_size, int *bytes_read);
uint8_t* makeDataPacketBeforeChecksum(uint8_t* data_chunk, uint8_t buffer_size);
uint16_t calculateDataPacketChecksum(uint8_t *buffer, int buffer_size);
uint8_t* makeDataPacketAfterChecksum(uint8_t *buffer, uint16_t calculated_checksum);

void printPDU(uint8_t *buffer);

int child_server_socket = 0;
uint32_t data_sequence_number = 0;


