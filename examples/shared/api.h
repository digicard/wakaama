typedef struct
{
    int 	          sock;
    int 	          sock_client;
} api_handler;

int create_api();
void close_api();
void close_client_api();
int api_new_connection();
void api_get_message();