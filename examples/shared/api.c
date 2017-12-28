/*
 Copyright (c) 2017, Digicard Sistemas

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 Pablo Martin Perez <pmperez@digicard.net>

*/

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

/*
 * Llamar tantas veces como instancias de host API se necesite
 * Default llama 1 vez desde lwm2mserver.c
 */
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

/*
 * Utilizar para encontrar conexion.
 * Genera nueva estructura API_OPERATION que mantiene los 
 * datos de la tarea pedida.
 */
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

	api_operation * operationLast, * operationNew;

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

    		operationNew = (api_operation *)malloc(sizeof(api_operation));
    		operationNew->sock = _socket;
    		operationNew->buffer = (char *)malloc(sizeof(char)*512);
    		operationNew->command = (char *)malloc(sizeof(char)*512);
		    operationNew->url = (char *)malloc(sizeof(char)*512);
		    operationNew->value = (char *)malloc(sizeof(char)*512);
		    operationNew->next = NULL;

    		if (api->operation == NULL)
	        {
	        	api->operation = (api_operation *)malloc(sizeof(api_operation));
	        	api->operation = operationNew;
	        }else{
	        	operationLast = api->operation;
	    		while( operationLast->next ){
	    			operationLast = operationLast->next;
	    		}
	    		operationLast->next = operationNew;
	        }
    		fcntl(operationNew->sock, F_SETFL, O_NONBLOCK);

    	}
    	printf("Ok api\n");
	}
}

/*
 * Busca nueva entrada de mensajes.
 * Si encuentra, api_operation se completa.
 */
void api_read( api_operation * apioper ){
	
	if (apioper->sock == -1) return;

	int n;
	fd_set readfds;
	struct timespec timeout;			
	int scount;							
	int status_select;					
	int z;
	const sigset_t sigmask;				
	char buff;
	char * token = NULL;

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

	if (status_select == -1)
	{
		// Conexion cerrada
		// Queda el espacio vacio
		// Habria que hacer una tarea de acomodamiento
		close_api(apioper->sock);
		apioper->sock = -1;
		free(apioper->buffer);
		free(apioper->command);
		free(apioper->url);
		free(apioper->value);
		return;
	}

	clear(apioper->command);
	clear(apioper->buffer);
	clear(apioper->value);
	if (apioper->sock > 0 && status_select > 0)
	{
		int i = 0;
		while (1){
			n = read(apioper->sock, &buff, 1 );
			if ( n <= 0 ) break;	// Fin de mensaje
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
                if (z == 3) apioper->value = token;
                z++;
            }
            printf("%s %d %s %s\n", apioper->command, apioper->client_id, apioper->url, apioper->value);
        }
	}
}

/*
 * Escribe mensaje en el socket de la operacion.
 */
void api_write( api_operation * apioper , char * string){
	int n;
	if ( (n = write( apioper->sock, string, (int) strlen( string ) )) < 0 )
	{
		fprintf(stderr, "Error escribiendo el socket\n");
	}
}

/*
 * Notifica a todas las operaciones que necesitan datos de clientID y uriP
 */
void api_notify( api_handler * api, uint16_t clientID, lwm2m_uri_t * uriP, uint8_t * data){

	char * urlBuff = (char *)malloc(sizeof(char)*255);

	uri_toString(uriP, urlBuff, 255, NULL);

	for( api_operation * apioper = api->operation;
             apioper != NULL; 
             apioper = apioper->next )
    {
        if ( apioper->sock > 0 &&
         	 apioper->client_id == clientID &&
         	 strcmp(urlBuff, apioper->url) == 0)
        {
        	api_write(apioper, (char *)data);
        }

    }
}

/*
 * Cierra un socket
 */
void close_api( int socket ){
	close(socket);
}

/*
 * Lista las apps conectadas a la api
 */
int api_list_clients( api_handler * api ){
	int total = -1;
	api_operation * apioper;
	for(apioper = api->operation; apioper != NULL; apioper = apioper->next){
		total++;
	}
	return total;
}

/*
 * Limpia el buffer
 */
void clear(char *buff)
{
	if (buff == NULL) return;
	int i;
	for (i = 0; *(buff + i) != '\0'; ++i)
		*(buff + i) = 0;
}