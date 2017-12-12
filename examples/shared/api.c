#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include "liblwm2m.h"
#include <errno.h>

#include "connection.h"
#include "api.h"

static api_handler api;

int create_api(){

	int addressFamily = AF_INET;
	const char * localPort = "5693";

	api.sock = create_socket(localPort, addressFamily, SOCK_STREAM);
	
	if (api.sock < 0)
    {
        fprintf(stderr, "Error opening socket: %d\r\n", errno);
        return -1;
    }
    else
    {
    	listen(api.sock,3);
    }

}

int api_new_connection(){

	struct sockaddr_in cli_addr;
	socklen_t clilen;
	fd_set readfds;
	struct timespec timeout;			//	Estructura de los tiempos de ejecucíon
	int scount;							//	Numero de FD mas alto.
	int status_io;						//	Estado de entrada salida de socket
	int status_select;					//	Estado de la consulta al socket
	const sigset_t sigmask;				//	Puntero a la mascara de la señal del socket leido

    FD_ZERO (&readfds);
	FD_SET (api.sock, &readfds);

	scount = api.sock+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 1;
	status_select = pselect (scount,
				&readfds,
			 	NULL,
			 	NULL,
			 	&timeout,
			 	&sigmask);

	if ( status_select > 0 && FD_ISSET ( api.sock, &readfds) ) {
		printf("Ok api\n");
		clilen = sizeof(cli_addr);
    	api.sock_client = accept(api.sock, (struct sockaddr *) &cli_addr, &clilen);

    	if (api.sock_client < 0)
    	{
    		fprintf(stderr, "Error opening socket: %d\r\n", errno);
    	}
	}else{
		printf("Error\n");
	}
}

void api_get_message(){

	char buffer[256];
	int n;
	if (api.sock_client > 0 )
	{
		bzero(buffer,256);
		if ( (n = read(api.sock_client, buffer, 255)) < 0 )
		{
			fprintf(stderr, "Error leyendo el socket\n");
		}else{
			printf("Leemos del socket %s\n", buffer);
		}

		if ( (n = write(api.sock_client, "Tengo tu mensaje", 16)) < 0 )
		{
			fprintf(stderr, "Error escribiendo el socket\n");
		}else{
			printf("Leemos del socket %s\n", buffer);
		}    		
	}
}

void close_api(){
	close(api.sock);	
}

void close_client_api(){
	close(api.sock_client);
}