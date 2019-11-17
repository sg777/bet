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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>


#include "bet.h"
#include "common.h"
#include "network.h"
#include "gfshare.h"
#include "cards777.h"


char* BET_getIPAddress()
{
	
	int fd;
    struct ifreq ifr;
	
	 fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	 /* I want to get an IPv4 IP address */
	 ifr.ifr_addr.sa_family = AF_INET;
	
	 /* I want IP address attached to "eth0" */
	 strncpy(ifr.ifr_name, "eth0", IF_NAMESIZE-1);
	
	 ioctl(fd, SIOCGIFADDR, &ifr);
	
	 close(fd);
	
	 /* display result */
	 printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	 return (inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
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


