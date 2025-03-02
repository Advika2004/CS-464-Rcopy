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

#define MAXBUF 80

typedef struct ServerParams{
    double error_rate;
    int port_number;
}ServerParams;

void processClient(int socketNum);
ServerParams checkArgs(int argc, char *argv[]);