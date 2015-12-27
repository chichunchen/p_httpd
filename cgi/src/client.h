#ifndef __CLIENT_H
#define __CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/uio.h>
#include "dbg.h"
#include "request.h"
#include "html.h"
#include "sock.h"

extern Request requests[REQUEST_MAX_NUM];

void new_connection(int index);
void clients_handler();

#endif
