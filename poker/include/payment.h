#ifndef PAYMENT_H
#define PAYMENT_H

#include "bet.h"

/***************************************************************
Here contains the functions which are specific to DCV
****************************************************************/
// Legacy Lightning Network functions - stubs in payment.c (to be replaced with CHIPS transactions)
int32_t bet_dcv_pay(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars);
void bet_dcv_paymentloop(void *_ptr);

/***************************************************************
Here contains the functions which are specific to players and BVV
****************************************************************/
// Legacy Lightning Network invoice functions - stubs in payment.c (to be replaced with CHIPS transactions)
int32_t bet_player_create_invoice_request(cJSON *argjson, struct privatebet_info *bet, int32_t amount);
int32_t bet_player_invoice_request(cJSON *argjson, cJSON *actionResponse, struct privatebet_info *bet, int32_t amount);
int32_t bet_player_invoice_pay(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars, int amount);
void bet_player_paymentloop(void *_ptr);

// Active function (CHIPS-based, not legacy)
int32_t bet_player_log_bet_info(cJSON *argjson, struct privatebet_info *bet, int32_t amount, int32_t action);

/***************************************************************
Common wallet functionality for all node types
These functions return cJSON response objects that should be
sent via the node-specific WebSocket write functions.
****************************************************************/
cJSON *bet_wallet_get_bal_info(void);
cJSON *bet_wallet_get_addr_info(void);
cJSON *bet_wallet_withdraw_request(void);
cJSON *bet_wallet_withdraw(cJSON *argjson);

#endif /* PAYMENT_H */
