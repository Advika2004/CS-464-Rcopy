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

#define MAXBUF 80
#define MAX_FILENAME 100

//! should this be some length? is it specified somwhere
#define MAX_REMOTE_MACHINE 256 

typedef struct RcopyParams {
    // src filename, dest filename, window size, buffer size, error-rate, remote-machine, remote-port.
    char src_filename[MAX_FILENAME + 1];
    char dest_filename[MAX_FILENAME + 1];
    int window_size;
    int buffer_size;
    double error_rate;
    char remote_machine[MAX_REMOTE_MACHINE + 1];
    int remote_port;
}RcopyParams;

void talkToServer(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);
RcopyParams checkArgs(int argc, char * argv[]);
