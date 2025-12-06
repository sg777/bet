#ifndef NETWORK_H
#define NETWORK_H

#include "bet.h"
char *bet_get_etho_ip();
char *bet_tcp_sock_address(int32_t bindflag, char *str, char *ipaddr, uint16_t port);
// bet_nanosock removed - nanomsg removed, function never called
cJSON *bet_msg_dealer_with_response_id(cJSON *argjson, char *dealer_ip, char *message);
// bet_send_data removed - nanomsg removed, function never called

#endif /* NETWORK_H */
