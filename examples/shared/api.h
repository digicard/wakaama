typedef struct
{
    int 	          sock;
    int 	          sock_client;
} api_handler;

api_handler * create_api();
void close_api(api_handler * api);
void close_client_api(api_handler * api);
int api_new_connection(api_handler * api);
char * api_read(api_handler * api);
void api_write( api_handler * api , char * string);