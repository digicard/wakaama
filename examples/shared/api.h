#include <liblwm2m.h>

/*
 * API for APPs
 */

typedef struct _api_operation_
{
    int 						sock;
    char * 						buffer;
    char * 						command;
    char * 						url;
    int 						client_id;
    char * 						value;
    int							has_message;
    struct _api_operation_ * 	next;
} api_operation;

typedef struct
{
    int               			sock;
    api_operation *     		operation;
} api_handler;

api_handler * 	create_api();
void 			close_api(int socket);
int 			api_new_connection(api_handler * api);
void 			api_read(api_operation * apicli);
void 			api_write( api_operation * apicli , char * string);
int 			api_list_clients( api_handler * api );
void 			api_notify( api_handler * api, uint16_t clientID, lwm2m_uri_t * uriP, uint8_t * data);