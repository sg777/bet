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
#include <net/if.h>
#include <sys/ioctl.h>

#include "network.h"
#include "bet.h"
#include "cards777.h"
#include "gfshare.h"
#include "err.h"

char *bet_get_etho_ip()
{
	struct ifreq ifr;
	int fd;
	unsigned char ip_address[15];

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	memcpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	strcpy((char *)ip_address, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	return (inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
}

char *bet_tcp_sock_address(int32_t bindflag, char *str, char *ipaddr, uint16_t port)
{
	sprintf(str, "tcp://%s:%u", bindflag == 0 ? ipaddr : "*",
		port); // ws is worse
	return (str);
}

// bet_nanosock removed - nanomsg/nano sockets no longer used

// bet_msg_dealer_with_response_id - stub implementation (nanomsg removed, but still called)
cJSON *bet_msg_dealer_with_response_id(cJSON *argjson, char *dealer_ip, char *message)
{
	// Nanomsg removed - function no longer functional
	dlg_error("bet_msg_dealer_with_response_id: nanomsg removed - not implemented");
	return NULL;
}

// bet_send_data removed - nanomsg/nano sockets no longer used, function never called
