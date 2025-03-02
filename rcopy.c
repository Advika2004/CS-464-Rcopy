#include "rcopy.h"

int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct

	RcopyParams params = checkArgs(argc, argv);
	
	socketNum = setupUdpClientToServer(&server, params.remote_machine, params.remote_port);
	
	talkToServer(socketNum, &server);
	
	close(socketNum);

	return 0;
}

void talkToServer(int socketNum, struct sockaddr_in6 * server)
{
	int serverAddrLen = sizeof(struct sockaddr_in6);
	char * ipString = NULL;
	int dataLen = 0; 
	char buffer[MAXBUF+1];
	
	buffer[0] = '\0';
	while (buffer[0] != '.')
	{
		dataLen = readFromStdin(buffer);

		printf("Sending: %s with len: %d\n", buffer,dataLen);
	
		safeSendto(socketNum, buffer, dataLen, 0, (struct sockaddr *) server, serverAddrLen);
		
		safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
		
		// print out bytes received
		ipString = ipAddressToString(server);
		printf("Server with ip: %s and port %d said it received %s\n", ipString, ntohs(server->sin6_port), buffer);
	      
	}
}

int readFromStdin(char * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

// checks the arguments to the client program and 
// src filename, dest filename, window size, buffer size, error-rate, remote-machine, remote-port. 
RcopyParams checkArgs(int argc, char * argv[])
{
	
	RcopyParams parameters;

    // check if more command line arguments than allowed
	if (argc != 8)
	{
		printf("usage: %s src-filename dest-filename window-size buffer-size error-rate remote-machine remote-port \n", argv[0]);
		exit(1);
	}

	// make sure filenames are not above 100 characters 
	if (strlen(argv[1]) > MAX_FILENAME || strlen(argv[1]) == 0 || strlen(argv[2]) > MAX_FILENAME || strlen(argv[2]) == 0) {
        printf("Error: Filenames must be between 1 and %d characters.\n", MAX_FILENAME);
        exit(1);
    }

    if (atoi(argv[3]) <= 0 || atoi(argv[4]) <= 0 || atoi(argv[7]) <= 0 ||
        atof(argv[5]) < 0.0 || atof(argv[5]) > 1.0) {
        printf("Error: Invalid values. Make sure window-size, buffer-size, and port-number are positive and error-rate is between 0 and 1\n");
        exit(1);
	}

	// if they are valid, then put them into the struct

	strncpy(parameters.src_filename, argv[1], MAX_FILENAME);
    parameters.src_filename[MAX_FILENAME] = '\0';

	strncpy(parameters.dest_filename, argv[1], MAX_FILENAME);
    parameters.dest_filename[MAX_FILENAME] = '\0';

	parameters.window_size = atoi(argv[3]);
	parameters.buffer_size = atoi(argv[4]);
	parameters.error_rate = atof(argv[5]);

	strncpy(parameters.remote_machine, argv[6], MAX_REMOTE_MACHINE);
    parameters.remote_machine[MAX_REMOTE_MACHINE] = '\0';

	parameters.remote_port = atoi(argv[7]);

	printf("Parsed Arguments:\n");
    printf("  Source File: %s\n", parameters.src_filename);
    printf("  Destination File: %s\n", parameters.dest_filename);
    printf("  Window Size: %d packets\n", parameters.window_size);
    printf("  Buffer Size: %d bytes\n", parameters.buffer_size);
    printf("  Error Rate: %.2f\n", parameters.error_rate);
    printf("  Remote Machine: %s\n", parameters.remote_machine);
    printf("  Remote Port: %d\n", parameters.remote_port);

	return parameters;
}





