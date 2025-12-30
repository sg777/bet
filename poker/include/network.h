#ifndef NETWORK_H
#define NETWORK_H

#include "bet.h"

// Active network functions
char *bet_get_etho_ip();
char *bet_tcp_sock_address(int32_t bindflag, char *str, char *ipaddr, uint16_t port);

// Legacy pub-sub/nanomsg functions - stubs in network.c and cashier.c (to be replaced with Verus ID communication)
cJSON *bet_msg_dealer_with_response_id(cJSON *argjson, char *dealer_ip, char *message);
cJSON *bet_msg_cashier_with_response_id(cJSON *argjson, char *cashier_ip, char *method_name);
int32_t bet_msg_cashier(cJSON *argjson, char *cashier_ip);
int32_t *bet_msg_multiple_cashiers(cJSON *argjson, char **cashier_ips, int no_of_cashiers);

// bet_nanosock removed - nanomsg removed, function never called
// bet_send_data removed - nanomsg removed, function never called

#endif /* NETWORK_H */
