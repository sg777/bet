/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "bet.h"
#include "common.h"
#include "network.h"
#include "gfshare.h"
#include "cards777.h"


char* BET_getIPAddress()
{
	
	char host[256];
	char *IP;
	struct hostent *host_entry=NULL;
	if(gethostname(host, sizeof(host))==-1)
		goto end;

	if((host_entry = gethostbyname(host))==NULL)
		goto end;
	
	IP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

	end:
		return IP;
}

char *BET_transportname(int32_t bindflag,char *str,char *ipaddr,uint16_t port)
{
    sprintf(str,"tcp://%s:%u",bindflag == 0 ? ipaddr : "*",port); // ws is worse
    return(str);
}

int32_t BET_nanosock(int32_t bindflag,char *endpoint,int32_t nntype)
{
    int32_t sock,timeout;
    if ( (sock= nn_socket(AF_SP,nntype)) >= 0 )
    {
        if ( bindflag == 0 )
        {
            if ( nn_connect(sock,endpoint) < 0 )
            {
                printf("connect to %s error for %s\n",endpoint,nn_strerror(nn_errno()));
                nn_close(sock);
                return(-1);
            } else printf("nntype.%d connect to %s connectsock.%d\n",nntype,endpoint,sock);
        }
        else
        {
            if ( nn_bind(sock,endpoint) < 0 )
            {
                printf("bind to %s error for %s\n",endpoint,nn_strerror(nn_errno()));
                nn_close(sock);
                return(-1);
            } else printf("(%s) bound\n",endpoint);
        }
        timeout = 1;
        nn_setsockopt(sock,NN_SOL_SOCKET,NN_RCVTIMEO,&timeout,sizeof(timeout));
        timeout = 100;
        nn_setsockopt(sock,NN_SOL_SOCKET,NN_SNDTIMEO,&timeout,sizeof(timeout));
        //maxsize = 2 * 1024 * 1024;
        //nn_setsockopt(sock,NN_SOL_SOCKET,NN_RCVBUF,&maxsize,sizeof(maxsize));
        if ( nntype == NN_SUB )
            nn_setsockopt(sock,NN_SUB,NN_SUB_SUBSCRIBE,"",0);
    }
    return(sock);
}


