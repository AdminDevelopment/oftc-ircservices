#include "setup.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "defines.h"
#include "irc_libio.h"

#include "conf.h"
#include "client.h"
#include "connection.h"
#include "dbm.h"
#include "hash.h"
#include "packet.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#define TRUE 1
#define FALSE 0
