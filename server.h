#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "pollLib.h"

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
int doGetFilenameState();
int doDoneState();
int doSendDataState();
int doWaitOnAckState();
int doWaitOnEOFAckState();