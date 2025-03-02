/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include "server.h"

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				

	ServerParams serverParam = checkArgs(argc, argv);
		
	socketNum = udpServerSetup(serverParam.port_number);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void processClient(int socketNum)
{
	int dataLen = 0; 
	char buffer[MAXBUF + 1];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);	
	
	buffer[0] = '\0';
	while (buffer[0] != '.')
	{
		dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
	
		printf("Received message from client with ");
		printIPInfo(&client);
		printf(" Len: %d \'%s\'\n", dataLen, buffer);

		// just for fun send back to client number of bytes received
		sprintf(buffer, "bytes: %d", dataLen);
		safeSendto(socketNum, buffer, strlen(buffer)+1, 0, (struct sockaddr *) & client, clientAddrLen);

	}
}

// error-rate and optional port number
ServerParams checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	ServerParams params;

	if (argc > 3 || argc < 3)
	{
		fprintf(stderr, "Usage %s error-rate optional-port-number\n", argv[0]);
		exit(-1);
	}
	
	//if right amount of arguments, check if the values given are valid
	if (argc == 3)
	{
		if (atoi(argv[2]) <= 0 || atof(argv[1]) < 0.0 || atof(argv[1]) > 1.0) {
        	printf("Error: Invalid values. Make sure port-number is positive and error-rate is between 0 and 1\n");
        	exit(1);
		}
		else{
			params.error_rate = atof(argv[1]);
			params.port_number = atoi(argv[2]);
		}
	}
	return params;
}


