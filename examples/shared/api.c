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
	
	if (_api->sock > 0){
        listen(_api->sock,3);
        _api->clients = (api_clients *)malloc(sizeof(api_clients));
	}

    return _api;

}

int api_new_connection( api_handler * api ){

	struct sockaddr_in cli_addr;
	socklen_t clilen;
	fd_set readfds;
	struct timespec timeout;			//	Estructura de los tiempos de ejecucíon
	int scount;							//	Numero de FD mas alto.
	int status_io;						//	Estado de entrada salida de socket
	int status_select;					//	Estado de la consulta al socket
	const sigset_t sigmask;				//	Puntero a la mascara de la señal del socket leido
	int _socket;
	int index;

	api_clients * apicli;
	api_clients * _apicli;

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

		_socket = accept(api->sock, (struct sockaddr *) &cli_addr, &clilen);

    	if ( _socket < 0 ){
    		fprintf(stderr, "Error opening socket: %d\r\n", errno);
    	}else{
    		apicli = api->clients;
    		while( NULL != apicli){
    			apicli = apicli->next;
    		}
    		apicli = (api_clients *)malloc(sizeof(api_clients));
    		_apicli = api->clients;
    		while( NULL != _apicli->next){
    			_apicli = _apicli->next;
    		}
    		apicli->next = _apicli->next;
    		_apicli->next = apicli;
    		apicli->sock = _socket;
    		apicli->buffer = NULL;
    		fcntl(apicli->sock, F_SETFL, O_NONBLOCK);
 
    	}

    	printf("Ok api\n");
    	
	}

}

char * api_read( api_clients * apicli ){
	
	int n;
	fd_set readfds;
	struct timespec timeout;			//	Estructura de los tiempos de ejecucíon
	int scount;							//	Numero de FD mas alto.
	int status_select;					//	Estado de la consulta al socket
	const sigset_t sigmask;				//	Puntero a la mascara de la señal del socket leido
	char * _buffer;
	char buff;
    FD_ZERO (&readfds);
	FD_SET (apicli->sock, &readfds);

	_buffer = (char *)malloc(sizeof(char));

	scount = apicli->sock+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 1;
	status_select = pselect (scount,
				&readfds,
			 	NULL,
			 	NULL,
			 	&timeout,
			 	&sigmask);

	if (apicli->sock > 0 && status_select > 0)
	{
		strcpy(_buffer, "\0");
		while (1){
			n = read(apicli->sock, &buff, 1 );
			if ( n < 0 ) break;	// Fin de mensaje
			if ( n == 0 ) { // El cliente no esta mas
				close_api(apicli->sock);
				_buffer = NULL;
				break;
			}
			sprintf(_buffer, "%s%c", _buffer, buff );
		}
	}else{
		_buffer = NULL;
	}

	return _buffer;
}

void api_write( api_clients * apicli , char * string){
	
	int n;

	if ( (n = write( apicli->sock, string, (int) strlen( string ) )) < 0 )
	{
		fprintf(stderr, "Error escribiendo el socket\n");
	}

}

void close_api( int socket ){
	close(socket);
}

int api_list_clients( api_handler * api ){
	int total = -1;
	api_clients * apicli;

	for(apicli = api->clients; apicli != NULL; apicli = apicli->next){
		total++;
	}
	
	return total;
}