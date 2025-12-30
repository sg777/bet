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
#include "payment.h"
#include "bet.h"
#include "client.h"
#include "commands.h"
#include "common.h"
#include "network.h"
#include "err.h"
#include "misc.h"
#include "storage.h"
/***************************************************************
Here contains the functions which are specific to DCV
****************************************************************/
// Legacy Lightning Network functions - stubs (to be replaced with CHIPS transactions)
int32_t bet_dcv_pay(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	dlg_warn("bet_dcv_pay: Lightning Network deprecated - use CHIPS transactions instead");
	(void)argjson; (void)bet; (void)vars;
	return ERR_LN;
}

void bet_dcv_paymentloop(void *_ptr)
{
	// Legacy function - no longer used
	(void)_ptr;
	dlg_warn("bet_dcv_paymentloop: Legacy function called - no longer functional");
}

/***************************************************************
Here contains the functions which are specific to players and BVV
****************************************************************/
// Legacy Lightning Network invoice functions - stubs (to be replaced with CHIPS transactions)
int32_t bet_player_create_invoice_request(cJSON *argjson, struct privatebet_info *bet, int32_t amount)
{
	dlg_warn("bet_player_create_invoice_request: Lightning Network deprecated");
	(void)argjson; (void)bet; (void)amount;
	return ERR_LN;
}

int32_t bet_player_invoice_request(cJSON *argjson, cJSON *actionResponse, struct privatebet_info *bet, int32_t amount)
{
	dlg_warn("bet_player_invoice_request: Lightning Network deprecated");
	(void)argjson; (void)actionResponse; (void)bet; (void)amount;
	return ERR_LN;
}

int32_t bet_player_invoice_pay(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars, int amount)
{
	dlg_warn("bet_player_invoice_pay: Lightning Network deprecated - use CHIPS transactions instead");
	(void)argjson; (void)bet; (void)vars; (void)amount;
	return ERR_LN;
}

void bet_player_paymentloop(void *_ptr)
{
	// Legacy function - no longer used
	(void)_ptr;
	dlg_warn("bet_player_paymentloop: Legacy function called - no longer functional");
}

int32_t bet_player_log_bet_info(cJSON *argjson, struct privatebet_info *bet, int32_t amount, int32_t action)
{
	int32_t retval = OK;
	cJSON *bet_info = NULL, *tx_id = NULL;
	char *hex_data = NULL;

	bet_info = cJSON_CreateObject();
	cJSON_AddStringToObject(bet_info, "method", "bet");
	cJSON_AddStringToObject(bet_info, "table_id", table_id);
	cJSON_AddNumberToObject(bet_info, "round", jint(argjson, "round"));
	cJSON_AddNumberToObject(bet_info, "playerID", bet->myplayerid);
	cJSON_AddNumberToObject(bet_info, "betAmount", amount);
	cJSON_AddNumberToObject(bet_info, "action", action);
	cJSON_AddStringToObject(bet_info, "tx_type", "game_info");

	hex_data = calloc(tx_data_size * 2, sizeof(char));
	str_to_hexstr(cJSON_Print(bet_info), hex_data);
	tx_id = cJSON_CreateObject();
	tx_id = chips_transfer_funds_with_data(0.0, legacy_m_of_n_msig_addr, hex_data);

	dlg_info("Address at which we are recording the game moves::%s", legacy_m_of_n_msig_addr);
	if (tx_id == NULL) {
		retval = ERR_GAME_RECORD_TX;
	} else {
		retval = bet_store_game_info_details(cJSON_Print(tx_id), table_id);
		dlg_info("tx to record the game move info::%s", cJSON_Print(tx_id));
	}

	if (retval != OK) {
		dlg_error("%s", bet_err_str(retval));
	}
	return retval;
}

/***************************************************************
Common wallet functionality for all node types (dealer, player, cashier)
These functions return cJSON response objects that can be sent via
the node-specific WebSocket write functions.
****************************************************************/

/**
 * Handle get_bal_info wallet request - common for all node types
 * Returns a cJSON object with balance information
 */
cJSON *bet_wallet_get_bal_info(void)
{
	cJSON *bal_info = NULL;

	bal_info = cJSON_CreateObject();
	cJSON_AddStringToObject(bal_info, "method", "bal_info");
	cJSON_AddNumberToObject(bal_info, "chips_bal", chips_get_balance());
	// Lightning Network support removed - ln_bal field removed

	return bal_info;
}

/**
 * Handle get_addr_info wallet request - common for all node types
 * Returns a cJSON object with address information
 */
cJSON *bet_wallet_get_addr_info(void)
{
	cJSON *addr_info = NULL;

	addr_info = cJSON_CreateObject();
	cJSON_AddStringToObject(addr_info, "method", "addr_info");
	cJSON_AddStringToObject(addr_info, "chips_addr", chips_get_new_address());
	// Lightning Network support removed - ln_addr field removed

	return addr_info;
}

/**
 * Handle withdrawRequest wallet request - common for all node types
 * Returns a cJSON object with withdrawal request information
 */
cJSON *bet_wallet_withdraw_request(void)
{
	cJSON *withdraw_response_info = NULL;

	withdraw_response_info = cJSON_CreateObject();
	cJSON_AddStringToObject(withdraw_response_info, "method", "withdrawResponse");
	cJSON_AddNumberToObject(withdraw_response_info, "balance", chips_get_balance());
	cJSON_AddNumberToObject(withdraw_response_info, "tx_fee", chips_tx_fee);
	cJSON_AddItemToObject(withdraw_response_info, "addrs", chips_list_address_groupings());

	return withdraw_response_info;
}

/**
 * Handle withdraw wallet request - common for all node types
 * Returns a cJSON object with withdrawal transaction information
 */
cJSON *bet_wallet_withdraw(cJSON *argjson)
{
	cJSON *withdraw_info = NULL;
	double amount = 0.0;
	char *addr = NULL;

	amount = jdouble(argjson, "amount");
	addr = jstr(argjson, "addr");
	
	if (addr == NULL || amount <= 0.0) {
		dlg_error("Invalid withdraw parameters");
		return NULL;
	}

	withdraw_info = cJSON_CreateObject();
	cJSON_AddStringToObject(withdraw_info, "method", "withdrawInfo");
	cJSON_AddItemToObject(withdraw_info, "tx", chips_transfer_funds(amount, addr));

	return withdraw_info;
}
