#include <liblwm2m.h>

api_handler * create_api();
void close_api(int socket);
int api_new_connection(api_handler * api);
char * api_read(api_clients * apicli);
void api_write( api_clients * apicli , char * string);
int api_list_clients( api_handler * api );