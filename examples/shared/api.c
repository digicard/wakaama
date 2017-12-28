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

	api_handler * _api = api;
	api_operation * apioperLast, * apioperNew;

    FD_ZERO (&readfds);
	FD_SET (_api->sock, &readfds);

	scount = _api->sock+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 1;
	status_select = pselect (scount,
				&readfds,
			 	NULL,
			 	NULL,
			 	&timeout,
			 	&sigmask);

	if ( status_select > 0 && FD_ISSET ( _api->sock, &readfds) ) {

		clilen = sizeof(cli_addr);

		_socket = accept(_api->sock, (struct sockaddr *) &cli_addr, &clilen);

    	if ( _socket < 0 ){
    		fprintf(stderr, "Error opening socket: %d\r\n", errno);
    	}else{    		

    		apioperNew = (api_operation *)malloc(sizeof(api_operation));
    		apioperNew->sock = _socket;
    		apioperNew->buffer = (char *)malloc(sizeof(char)*512);
    		apioperNew->command = (char *)malloc(sizeof(char)*512);
		    apioperNew->url = (char *)malloc(sizeof(char)*512);
		    apioperNew->value = (char *)malloc(sizeof(char)*512);
		    apioperNew->next = NULL;

    		// Primera vez
    		if (_api->operation == NULL)
	        {
	        	_api->operation = (api_operation *)malloc(sizeof(api_operation));
	        	_api->operation = apioperNew;
	        }else{
	        	apioperLast = _api->operation;
    		
	    		while( apioperLast->next ){
	    			apioperLast = apioperLast->next;
	    		}

	    		apioperLast->next = apioperNew;
	        }

    		fcntl(apioperNew->sock, F_SETFL, O_NONBLOCK);
    	}

    	printf("Ok api\n");
    	
	}

}

void api_read( api_operation * apioper ){
	
	int n;
	fd_set readfds;
	struct timespec timeout;			
	int scount;							
	int status_select;					
	int z;
	const sigset_t sigmask;				
	char buff;
	char * token = NULL;

	//api_operation * apioper = apioper;

    FD_ZERO (&readfds);
	FD_SET (apioper->sock, &readfds);

	scount = apioper->sock+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 1;
	status_select = pselect (scount,
				&readfds,
			 	NULL,
			 	NULL,
			 	&timeout,
			 	&sigmask);

	vaciar(apioper->command);
	vaciar(apioper->buffer);

	if (apioper->sock > 0 && status_select > 0)
	{
		int i = 0;
		while (1){

			n = read(apioper->sock, &buff, 1 );

			if ( n < 0 ) break;	// Fin de mensaje
			if ( n == 0 ) { // El cliente no esta mas
				close_api(apioper->sock);
				free(apioper->buffer);
				break;
			}

			if (apioper->buffer == NULL)
				apioper->buffer = (char *)malloc(sizeof(char)*512);

			*(apioper->buffer + i++) = buff;
		}
		
		if (apioper->buffer != NULL)
        {
        	*(apioper->buffer + i) = '\0';
            z = 0;
            while ((token = strsep(&apioper->buffer, "_")) != NULL){
                if (z == 0) apioper->command = token;
                if (z == 1) apioper->client_id = atoi(token);
                if (z == 2) apioper->url = token;
                z++;
            }
        }
	}
}

void api_write( api_operation * apioper , char * string){
	int n;
	if ( (n = write( apioper->sock, string, (int) strlen( string ) )) < 0 )
	{
		fprintf(stderr, "Error escribiendo el socket\n");
	}
}

void close_api( int socket ){
	close(socket);
}

int api_list_clients( api_handler * api ){
	int total = -1;
	api_operation * apioper;
	for(apioper = api->operation; apioper != NULL; apioper = apioper->next){
		total++;
	}
	return total;
}

void vaciar(char *buff)
{
	if (buff == NULL) return;
	int i;
	for (i = 0; *(buff + i) != '\0'; ++i)
		*(buff + i) = 0;
}