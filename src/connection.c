#include "stdinc.h"

static CNCB serv_connect_callback;
struct Callback *connected_cb;

static void
serv_connect_callback(fde_t *fd, int status, void *data)
{
  struct Client *client = (struct Client*)data;
  struct Server *server = NULL;

  assert(client != NULL);

  server = client->server;

  assert(server != NULL);
  assert(&server->fd == fd);

  if(status != COMM_OK)
  {
    printf("serv_connect_callback: Connect failed :(\n");
    exit(1);
  }

  printf("serv_connect_callback: Connect succeeded!\n");
  comm_setselect(fd, COMM_SELECT_READ, read_packet, client, 0);

  dlinkAdd(client, &client->node, &global_server_list);
  
  execute_callback(connected_cb, client);
}

void 
connect_server()
{
  struct Client *client = make_client(NULL);
  struct Server *server = make_server(client);

  memcpy(server->pass, Connect.password, 20);
  strlcpy(client->name, Connect.name, sizeof(client->name));
  strlcpy(client->host, Connect.host, sizeof(client->host));

  SetConnecting(client);
    
  dlinkAdd(client, &client->node, &global_client_list);

  if(comm_open(&server->fd, AF_INET, SOCK_STREAM, 0, NULL) < 0)
  {
    printf("connect_server: Could not open socket\n");
    exit(1);
  }

  comm_connect_tcp(&server->fd, Connect.host, Connect.port,
      NULL, 0, serv_connect_callback, client, AF_INET, CONNECTTIMEOUT);
  
}

void *
server_connected(va_list args)
{
  return NULL;
}