#ifndef __API__
#define __API__
#include <liblwm2m.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include "../../core/internals.h"
#include "connection.h"
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include "liblwm2m.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
/*
 * API for APPs
 */



#define API_COMMAND_END_LIST {NULL, 0, NULL, 0,NULL}



typedef struct _api_operation_t_
{
    int                         sock;
    char *                      buffer;
    struct _API_command_desc_t * API_command;
    struct _api_handler_t *     API_handler;
    char *                      url;
    int                         client_id;
    char *                      value;
    int                         has_message;
    int                         closed;
    struct _api_operation_t_ *  next;
} api_operation_t;



typedef struct _api_handler_t
{
    lwm2m_context_t *           lwm2mH;
    int                         api_handler_id;
    int                         sock;
    api_operation_t *           operation;
    struct _api_handler_t *     next;
    struct _API_command_desc_t * command_array;
} api_handler_t;

typedef void (*API_command_handler_t) (api_handler_t * api,api_operation_t * apioper,void * userData);

typedef struct _API_command_desc_t
{
    char *              command_name;
    int                 n_args;
    API_command_handler_t   callback;
    int                 subscribe_comm;
    void *              userData;
} API_command_desc_t;

typedef struct{
    api_handler_t * api;
    api_operation_t * operation;
} api_handler_and_operation_t;

api_handler_t *     create_api(void);
void                close_api(int socket);
int                 api_new_connection(api_handler_t * api);
int                 api_operation_check(api_operation_t * apicli);
void                api_write( api_operation_t * apicli , char * string);
int                 api_total_clients( api_handler_t * api );
void                api_notify( api_handler_t * api, uint16_t clientID, lwm2m_uri_t * uriP, uint8_t * data,void * userData);
void                api_remove_operation(api_handler_t * api, api_operation_t * apioper);
void                api_send_observation(uint8_t * data);
void                api_free(api_handler_t * api);


int API_create(int new_api_id);
int API_config(int api_id,API_command_desc_t * command_array,char * port,lwm2m_context_t * lwm2mH);
void API_step(void);
void API_send_subscribe_change(api_handler_t * api,
                    lwm2m_uri_t * uri,
                    const char * value,
                    size_t valueLength);
api_handler_t * API_list_search(int api_id);
#endif