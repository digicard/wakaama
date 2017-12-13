#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include "liblwm2m.h"
#include <errno.h>
#include <fcntl.h>

#include "connection.h"
#include "api.h"

api_handler * create_api(){

	api_handler * _api;
	
	_api = (api_handler *)malloc(sizeof(api_handler));

	int addressFamily = AF_INET;
	const char * localPort = "5693";

	_api->sock = create_socket(localPort, addressFamily, SOCK_STREAM);
	_api->sock_client = -1;
	
	if (_api->sock > 0)
        listen(_api->sock,3);

    return _api;

}

int api_new_connection( api_handler * api){

	struct sockaddr_in cli_addr;
	socklen_t clilen;
	fd_set readfds;
	struct timespec timeout;			//	Estructura de los tiempos de ejecucíon
	int scount;							//	Numero de FD mas alto.
	int status_io;						//	Estado de entrada salida de socket
	int status_select;					//	Estado de la consulta al socket
	const sigset_t sigmask;				//	Puntero a la mascara de la señal del socket leido

    FD_ZERO (&readfds);
	FD_SET (api->sock, &readfds);

	scount = api->sock+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 1;
	status_select = pselect (scount,
				&readfds,
			 	NULL,
			 	NULL,
			 	&timeout,
			 	&sigmask);

	if ( status_select > 0 && FD_ISSET ( api->sock, &readfds) ) {
		clilen = sizeof(cli_addr);
    	api->sock_client = accept(api->sock, (struct sockaddr *) &cli_addr, &clilen);

    	if (api->sock_client < 0)
    	{
    		fprintf(stderr, "Error opening socket: %d\r\n", errno);
    	}else{
    		fcntl(api->sock_client, F_SETFL, O_NONBLOCK);
    	}
    	printf("Ok api\n");
	}

}

char * api_read( api_handler * api ){
	
	int n;
	fd_set readfds;
	struct timespec timeout;			//	Estructura de los tiempos de ejecucíon
	int scount;							//	Numero de FD mas alto.
	int status_select;					//	Estado de la consulta al socket
	const sigset_t sigmask;				//	Puntero a la mascara de la señal del socket leido
	char * _buffer;
	char buff;
    FD_ZERO (&readfds);
	FD_SET (api->sock_client, &readfds);

	_buffer = (char *)malloc(sizeof(char));

	scount = api->sock_client+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 1;
	status_select = pselect (scount,
				&readfds,
			 	NULL,
			 	NULL,
			 	&timeout,
			 	&sigmask);

	if (api->sock_client > 0 && status_select > 0)
	{
		strcpy(_buffer, "\0");
		while ( 1 ){
			n = read(api->sock_client, &buff, 1 );
			if ( n <= 0 ) break;
			sprintf(_buffer, "%s%c", _buffer, buff );
		}		
		// if ( (n = write(api->sock_client, "Tengo tu mensaje", 16)) < 0 )
		// {
		// 	fprintf(stderr, "Error escribiendo el socket\n");
		// }

	}else{
		_buffer = NULL;
	}

	return _buffer;
}

void api_write( api_handler * api , char * string){
	
	int n;

	if ( (n = write(api->sock_client, string, (int) strlen( string ) )) < 0 )
	{
		fprintf(stderr, "Error escribiendo el socket\n");
	}

}

void close_api( api_handler * api){
	close(api->sock);	
}

void close_client_api( api_handler * api){
	close(api->sock_client);
}