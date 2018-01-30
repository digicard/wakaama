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

#include "api.h"

#define BACKLOG 10
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define API_DEBUG 1
#if API_DEBUG
#define  API_LOG(STR); printf("\x1b[34m""[API]"); printf(STR); printf("\n""\x1b[0m");
#else
#define API_LOG(STR, ... );
#endif


char * * api_parse_message(const char * message,const char token, int * n_messages);
void api_my_free_parsed_message(char * * message_array, int n_messages);
int API_read_socket(api_operation_t * apioper);
void API_list(void);

int * obs_socket_aux=NULL;
//long M=0;
api_handler_t * head_api_list=NULL;

int sigpipe_error=0;


void * my_malloc(size_t T)
{
	//M+=1;
	//printf("M:\"%ld\" malloc\n",M);
	return malloc(T);
}


void my_free(void * T)
{
	//M-=1;
	//printf("M:\"%ld\" FREE\n",M);
	return free(T);
}


enum{
	OK=0,
	ERROR=1
};

void SIGPIPE_HANDLER(int s){
	sigpipe_error=1;
}

int API_command_check(API_command_desc_t * command_array,api_operation_t * apioper){

	int n_messages;
	char * * parsed_message=api_parse_message(apioper->buffer,'_',&n_messages);
	int i=0;
	while(command_array[i].command_name!=NULL){
		if(command_array[i].n_args==n_messages&&(!(strcmp(command_array[i].command_name,parsed_message[0]))))
		{
			switch(n_messages){
				case 4:
				apioper->value=my_malloc(sizeof(char)*(strlen(parsed_message[3])+1));
				strcpy(apioper->value,parsed_message[3]);
				(apioper->value)[strlen(parsed_message[3])]=0;
				case 3:
				apioper->url=my_malloc(sizeof(char)*(strlen(parsed_message[2])+1));
				strcpy(apioper->url,parsed_message[2]);
				(apioper->url)[strlen(parsed_message[2])]=0;
				case 2:
				apioper->client_id=atoi(parsed_message[1]);
				case 1:
				apioper->API_command=&(command_array[i]);
				break;
				default:
				break;
			}
			api_my_free_parsed_message(parsed_message,n_messages);
			apioper->has_message=1;
			return 1;
		}
		i++;
	}
	api_my_free_parsed_message(parsed_message,n_messages);
	return 0;
} 

api_handler_t * API_list_search(int api_id)
{
	api_handler_t * result=head_api_list;

	while(result!=NULL&&result->api_handler_id!=api_id)
		result=result->next;
	return result;
}
void API_list_add(int new_api_id)
{
	api_handler_t * new_item=my_malloc(sizeof(api_handler_t));
	new_item->lwm2mH=NULL;
	new_item->sock=-1;
	new_item->operation=NULL;
	new_item->next=NULL;
	new_item->command_array=NULL;
	new_item->api_handler_id=new_api_id;
	new_item->next=head_api_list;
	head_api_list=new_item;
}
api_handler_t * API_list_remove(api_handler_t * head,int api_id)
{
	api_handler_t * curr=head;
	if(curr==NULL)
		return NULL;
	if(curr->api_handler_id==api_id)
	{
		curr=head->next;
		api_free(head);
		return curr;
	}
	while(curr->next!=NULL)
	{
		if(curr->next->api_handler_id==api_id) {
			api_handler_t * api_to_my_free=curr->next;
			curr->next=curr->next->next;
			api_free(api_to_my_free);
			break;
		}
		curr=curr->next;
	}
	return head;
}

int API_create(int new_api_id)
{
	signal (SIGPIPE, SIGPIPE_HANDLER);
	if(new_api_id<0)
		return ERROR;
	if(head_api_list==NULL)
	{	
		API_LOG("API LIST EMPTY");

		API_list_add(new_api_id);
		API_LOG("API CREATED");
		return OK;
	}
	if(API_list_search(new_api_id)==NULL)
	{
		API_list_add(new_api_id);
		API_LOG("API CREATED");
	}
	
	if(API_list_search(new_api_id)==NULL)
		return ERROR;
	return OK;
}

int API_config(int api_id,API_command_desc_t * command_array,char * port,lwm2m_context_t * lwm2mH)
{
	int	sockaux;	/*socket auxiliar*/
	int	aux; 		/*variable auxiliar*/
	int addressFamily = AF_INET;

	api_handler_t * desired_api=API_list_search(api_id);
	if(desired_api==NULL)
		return ERROR;
	desired_api->command_array=command_array;
	desired_api->lwm2mH=lwm2mH;
	desired_api->operation=NULL;

	desired_api->sock=create_socket(port, addressFamily, SOCK_STREAM);
	if (desired_api->sock > 0){
		listen(desired_api->sock,BACKLOG);
		printf("SERVER API on localhost:%s \n", port);
	}else
	printf("SERVER API COULD NOT OPEN SOCKET\n", port);
	API_LOG("CONFIG OK" );
	return OK;
}

void API_step()
{
	api_handler_t * api=head_api_list;
	struct sockaddr_in cli_addr;
	socklen_t clilen;
	fd_set readfds;
	struct timespec timeout;			//	Estructura de los tiempos de ejecucíon
	int scount;							//	Numero de FD mas alto.
	int status_io;						//	Estado de entrada salida de socket
	int status_select;					//	Estado de la consulta al socket	
	int _socket;
	int index;


	while(api!=NULL){

		FD_ZERO (&readfds);
		FD_SET (api->sock, &readfds);

		scount = api->sock+1;
		timeout.tv_sec = 0;
		timeout.tv_nsec = 0;
		status_select = pselect (scount,
			&readfds,
			NULL,
			NULL,
			&timeout,
			NULL);

		if ( status_select > 0 && FD_ISSET ( api->sock, &readfds) ) {
			printf("Evaluan2 socket\n");

			clilen = sizeof(cli_addr);

			_socket = accept(api->sock, (struct sockaddr *) &cli_addr, &clilen);

			if ( _socket < 0 ){
				fprintf(stderr, "Error opening socket: %d\r\n", errno);
			}else{    
				api_operation_t * operationNew;		
				operationNew = (api_operation_t *)my_malloc(sizeof(api_operation_t));
				if(operationNew==NULL){
					perror("Could not allocate operationNew\n");
					exit (0);
				}
				operationNew->sock = _socket;
				operationNew->client_id = -1;
				operationNew->buffer = NULL;
				operationNew->url = NULL;
				operationNew->value = NULL;
				operationNew->closed=0;
				operationNew->has_message=0;
				operationNew->API_handler=api;
				operationNew->next = api->operation;
				api->operation=operationNew;

				fcntl(operationNew->sock, F_SETFL, O_NONBLOCK);

				printf("[API] Nueva operacion %d\n",operationNew->sock);
			}
		}else{

		}
		api_operation_t * apioper = api->operation;
		while (apioper!=NULL){

			if (apioper->sock <= 0 || apioper->closed) 
			{   
				api_operation_t * apioper_AUX=apioper->next;
				printf("[API] Sock: %d removido.\n",apioper->sock);
				api_remove_operation(api, apioper);
				apioper = apioper_AUX; 
				continue;
			}
			if(API_read_socket(apioper)){

				if ( API_command_check(api->command_array,apioper) )
				{
					printf("[API] Ejecutando callback de %s.\n", apioper->API_command->command_name);
					apioper->API_command->callback(api,apioper,apioper->API_command->userData);
				}
				else
				{
					api_operation_t * apioper_AUX=apioper->next;
					printf("[API] Sock: %d removido. No hay comando.\n",apioper->sock);
					api_remove_operation(api, apioper);
					apioper = apioper_AUX; 
					continue;
				}
			}
			if(apioper->closed){
				api_operation_t * apioper_AUX=apioper->next;
				printf("[API] Sock: %d removido. Socket disponible sin datos.\n",apioper->sock);
				api_remove_operation(api, apioper);
				apioper = apioper_AUX; 
				continue;
			}
			apioper = apioper->next;
		}


		api=api->next;
	}
}

/*
 * Busca nueva entrada de mensajes.
 * Es importante controlar toda la estructura,
 * en este punto para asegurar la comunicacion.
 * Si encuentra, api_operation se completa.
 */
int API_read_socket(api_operation_t * apioper){
	int n=0;
	fd_set readfds;
	struct timespec timeout;			
	int scount;							
	int status_select;		
	char buff[MAX_SIZE];
	if(apioper->buffer!=NULL)
	{
		my_free(apioper->buffer);
		apioper->buffer=NULL;
	}
	FD_ZERO (&readfds);
	FD_SET (apioper->sock, &readfds);

	scount = apioper->sock+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;

	status_select = pselect (scount,
		&readfds,
		NULL,
		NULL,
		&timeout,
		NULL);

	if (status_select == -1)
	{
		apioper->closed = 1;
		return 0;
	}

	if (status_select > 0)
	{
		int i = 0;
		while (1){
			n = read(apioper->sock, buff, MAX_SIZE-1 );
			buff[n]=0;
			if ( n <= 0 ) break;	// Fin de mensaje
			if(apioper->buffer==NULL)
			{
				apioper->buffer=my_malloc(sizeof(char)*(n+1));
				strncpy(apioper->buffer,buff,n);
				apioper->buffer[n]=0;
			}
			else
			{
				char * aux=my_malloc(sizeof(char)*(strlen(apioper->buffer)+n+1));
				strncpy(aux,apioper->buffer,strlen(apioper->buffer));
				strncat(aux,buff,n);
				aux[strlen(apioper->buffer)+n]=0;
				my_free(apioper->buffer);
				apioper->buffer=aux;
			}
		}

	}
	if (apioper->buffer!=NULL){
		printf("[API] Socket data: \"%s\"\n",apioper->buffer);
		return 1;
	}
	return 0;

}

char * * api_parse_message(const char * message,const char token, int * n_messages)
{
	uint16_t N=0;
	char ** parsed_message=NULL;
	char * aux=my_malloc(sizeof(char)*MAX_SIZE);
	int size=MAX_SIZE;
	int aux_2;
	int index_0,index_1;
	*n_messages=0;
	if(message==NULL||message[0]=='\0')
	{
		return NULL;
	}
	N=0;
	aux_2=0;
	while(message[N]!='\0')
	{
		if(message[N]!=token)
		{
			aux_2+=1;
		}
		N++;
	}
	if(aux_2==0)
	{
		return NULL;
	}



	N=0;
	aux_2=0;
	while(1)
	{
		if(message[N]=='\0'||message[N]==token)
		{
			char ** aux_parsed_message;
			(*n_messages)+=1;
			aux[aux_2]=0;

			aux_parsed_message=my_malloc(sizeof(char *)*(*n_messages));
			memcpy(aux_parsed_message,parsed_message,sizeof(char *)*((*n_messages)-1));

			aux_parsed_message[(*n_messages)-1]=my_malloc(sizeof(char)*(strlen(aux)+1));
			
			strncpy(aux_parsed_message[(*n_messages)-1],aux,strlen(aux));
			(aux_parsed_message[(*n_messages)-1])[strlen(aux)]=0;
			my_free(parsed_message);
			parsed_message=aux_parsed_message;

			//printf("[API] %d mensaje parseado.\n",(*n_messages));

			aux_2=0;
		}else{
			aux[aux_2]=message[N];
			aux_2++;
		}
		if(message[N]=='\0')
		{	
			break;
		}
		N+=1;
		if((size-1)<aux_2)
		{
			char * aux_move;
			size+=MAX_SIZE;
			aux_move=my_malloc(sizeof(char)*size);
			memcpy(aux_move,aux,sizeof(char)*(size-MAX_SIZE));
			my_free(aux);
			aux=aux_move;
		}
	}
	free(aux);
	//printf("[API] %d mensajes parseados.\n",(*n_messages));
	return parsed_message;
}

void api_my_free_parsed_message(char * * message_array, int n_messages){
	uint16_t i;
	for(i=0;i<n_messages;i++)
	{
		//printf("[API] Liberando mensajes paresados. Mensaje %d \n",i+1);
		my_free(message_array[i]);
	}
	//printf("[API] Liberando array de mensajes.\n");
	my_free(message_array);
	//printf("[API] Parsed MSG liberados.\n");
}

/*
 * Escribe mensaje en el socket de la operacion.
 */
void api_write( api_operation_t * apioper , char * string){
	fd_set writefd;
	struct timespec timeout;			
	int scount;							
	int status_select;			
	FD_ZERO (&writefd);
	FD_SET (apioper->sock, &writefd);
	scount = apioper->sock+1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;
	status_select = pselect (scount,
		NULL,
		&writefd,
		NULL,
		&timeout,
		NULL);
	if (status_select>0)
	{	
		int n ;
		if(string==NULL){
			n= write( apioper->sock, "NO_DATA", (int) strlen( "NO_DATA" ) );
			printf("\t DATAwrittenNO_DATA\n");
		}
		else{
			n= write( apioper->sock, string, (int) strlen( string ) );
			printf("\t DATAwritten%s\n", string);
		}
		if(sigpipe_error==1){
			close(apioper->sock);
			apioper->sock=-1;
			apioper->closed=1;
			sigpipe_error=0;
		}
		if ( n <= 0 )
		{
			fprintf(stderr, "Error escribiendo el socket\n");
			apioper->closed=1;
		}
	}
}

/*
 * Notifica a todas las operaciones que necesitan datos de clientID y uriP
 */
void api_notify( api_handler_t * api, uint16_t clientID, lwm2m_uri_t * uriP, uint8_t * data,void * userData){
	if(userData==NULL) return;
	char urlBuff[MAX_SIZE];
	memset(urlBuff,0,MAX_SIZE);
	api_operation_t * apioper=(userData==NULL?NULL : (api_operation_t *) userData);
	api_operation_t * curr_op=(userData==NULL?NULL : ((api_operation_t *) userData)->API_handler->operation);
	uri_toString(uriP, urlBuff, 255, NULL);
	printf("Notificacion de cliente: %d\n", clientID);
	printf("\t con url : %s\n", urlBuff);
	if (data!=NULL)
		printf("\t \"%s\"\n", data);
	API_list();

	if(curr_op!=NULL){
		while(curr_op!=NULL){
			if(curr_op->has_message==1)
			{
				if(strcmp(curr_op->url,urlBuff)==0
					&&curr_op->client_id==apioper->client_id
					&&curr_op->API_command==apioper->API_command)
				{
					api_write(curr_op, (char *)data);
				}else {
				}
			}
			curr_op=curr_op->next;
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
int api_total_clients( api_handler_t * api ){
	int total = 0;
	api_operation_t * apioper;
	for(apioper = api->operation; apioper != NULL; apioper = apioper->next){
		total++;
	}
	return total;
}



void api_remove_operation(api_handler_t * api, api_operation_t * apioper){

	api_operation_t * next, * current;
	printf("[API] Removiento operacion %d \n",apioper->sock);

	if(api->operation==NULL)
		return;//no existe nada en la cola
	current=api->operation;
	while( current!=NULL ){
		if (current==apioper)
			break;
		current = current->next;
	}
	if(current==NULL) //sale si no existe
		return;

	if(current==api->operation)//es el primer elemento
	{
		printf("[API] Primer elemento de la cola. \n");
		api->operation=api->operation->next;
	}
	else
	{
		current = api->operation;
		while( current->next!=apioper){
			current = current->next;
		}
		current->next=current->next->next;
	}
	printf("[API] Cerran2 socket %d \n",apioper->sock);
	close_api(apioper->sock);
	printf("[API] Conexion cerrada de operacion %d \n", apioper->sock);
	my_free(apioper->buffer);
	if(apioper->url!=NULL)
		my_free(apioper->url);
	if(apioper->value!=NULL)
		my_free(apioper->value);
	my_free(apioper);

	if (api->operation==NULL)
	{
		printf("[API] Cola de operaciones vacia. \n");
	}
	printf("[API] Desaparece\n");

}
void api_free(api_handler_t * api)
{
	while(api->operation!=NULL){
		api_operation_t * aux=api->operation;
		api->operation=api->operation->next;
		close(aux->sock);
		if(aux->buffer!=NULL)
			my_free(aux->buffer);
		my_free(aux);
	}
	close(api->sock);
	my_free(api);
}

void API_list()
{
	api_handler_t * api=head_api_list;
	while(api!=NULL){
		int i=0;
		api_operation_t * apioper=api->operation;
		while(apioper!=NULL)
		{
			i++;
			apioper=apioper->next;
		}
		printf("[API] API \"%d\" OPERACIONES \"%d\"\n",api->api_handler_id,i);
		api=api->next;
	}
}

void API_send_subscribe_change(api_handler_t * api,
	lwm2m_uri_t * uri,
	const char * value,
	size_t valueLength)
{
	if (api==NULL)
	{
		return;
	}
	API_command_desc_t * command_sub=
	api->command_array;
	api_operation_t * curr_op=api->operation;
	int i=0;
	if (curr_op==NULL||command_sub==NULL)
	{
		return;
	}
	while(command_sub[i].command_name!=NULL
		&&command_sub[i].subscribe_comm==0)
		i++;

	if(command_sub[i].command_name==NULL)
		return;

	command_sub=&(command_sub[i]);

	char urlBuff[MAX_SIZE];
	memset(urlBuff,0,MAX_SIZE);

	uri_toString(uri, urlBuff, 255, NULL);
	printf("\t con url : \"%s\"\n", urlBuff);
	API_list();
	if (value!=NULL)
		printf("\t \"%s\"\n", value);

	if(curr_op!=NULL){
		while(curr_op!=NULL){
			if(curr_op->has_message==1)
			{
				if(strcmp(curr_op->url,urlBuff)==0
					&&command_sub==curr_op->API_command)
				{
					int bufflen=strlen("{\"bn\":\"\",\"e\":[{\"n\":\"\",\"sv\":\"\"}]}")+MAX_SIZE;
					char json[bufflen];
					memset(json,0,bufflen);
					sprintf(json,"{\"bn\":\"%s\",\"e\":[{\"n\":\"%d\",\"sv\":\"%s\"}]}",urlBuff,uri->resourceId,value);
					api_write(curr_op, json);
				}
			}
			curr_op=curr_op->next;
		}
	}
}