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
// bet_dcv_create_invoice_request removed - broken code (references undefined 'bytes'), Lightning Network removed

// bet_dcv_invoice_pay removed - Lightning Network removed, calls removed bet_dcv_create_invoice_request

int32_t bet_dcv_pay(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	cJSON *invoiceInfo = NULL, *payResponse = NULL;
	int argc, retval = OK;
	char **argv = NULL, *invoice = NULL;

	invoice = jstr(argjson, "invoice");
	invoiceInfo = cJSON_Parse(invoice);
	argc = 3;
	bet_alloc_args(argc, &argv);
	argv = bet_copy_args(argc, "lightning-cli", "pay", jstr(invoiceInfo, "bolt11"));

	payResponse = cJSON_CreateObject();
	retval = make_command(argc, argv, &payResponse);
	if (retval != OK) {
		dlg_error("%s", bet_err_str(retval));
	}
	bet_dealloc_args(argc, &argv);
	return retval;
}

void bet_dcv_paymentloop(void *_ptr)
{
	uint8_t flag = 1;
	struct privatebet_info *bet = _ptr;
	(void)_ptr; /* unused parameters */
	// Nanomsg removed - communication now via websockets only
	// The payment loop is handled through websocket callbacks
	(void)flag; // Suppress unused parameter warning
	(void)bet; // Suppress unused parameter warning
}

/***************************************************************
Here contains the functions which are specific to players and BVV
****************************************************************/

int32_t bet_player_create_invoice(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars,
				  char *deckid)
{
	int argc, retval = OK;
	char **argv = NULL, params[2][arg_size];
	cJSON *invoiceInfo = NULL, *invoice = NULL;

	if (jint(argjson, "playerid") == bet->myplayerid) {
		vars->player_funds += jint(argjson, "betAmount");

		argc = 5;
		bet_alloc_args(argc, &argv);
		argv = bet_copy_args(argc, "lightning-cli", "invoice", params[0], params[1], "Winning claim");
		invoice = cJSON_CreateObject();
		retval = make_command(argc, argv, &invoice);
		if (retval != OK) {
			dlg_error("%s", bet_err_str(retval));
		}
		dlg_info("invoice::%s\n", cJSON_Print(invoice));
		invoiceInfo = cJSON_CreateObject();
		cJSON_AddStringToObject(invoiceInfo, "method", "invoice");
		cJSON_AddNumberToObject(invoiceInfo, "playerid", bet->myplayerid);
		cJSON_AddStringToObject(invoiceInfo, "label", params[1]);
		cJSON_AddStringToObject(invoiceInfo, "invoice", cJSON_Print(invoice));
		// Nanomsg removed - no longer used
		retval = OK;
	}
	bet_dealloc_args(argc, &argv);
	return retval;
}

int32_t bet_player_create_invoice_request(cJSON *argjson, struct privatebet_info *bet, int32_t amount)
{
	int32_t retval = OK;
	cJSON *betInfo = NULL;

	betInfo = cJSON_CreateObject();
	cJSON_AddStringToObject(betInfo, "method", "invoiceRequest");
	cJSON_AddNumberToObject(betInfo, "round", jint(argjson, "round"));
	cJSON_AddNumberToObject(betInfo, "playerID", bet->myplayerid);
	cJSON_AddNumberToObject(betInfo, "betAmount", amount);

	// Nanomsg removed - no longer used
	retval = OK;
	return retval;
}

int32_t bet_player_invoice_request(cJSON *argjson, cJSON *actionResponse, struct privatebet_info *bet, int32_t amount)
{
	int32_t retval = OK;
	cJSON *betInfo = NULL;

	betInfo = cJSON_CreateObject();
	cJSON_AddStringToObject(betInfo, "method", "bettingInvoiceRequest");
	cJSON_AddNumberToObject(betInfo, "round", jint(argjson, "round"));
	cJSON_AddNumberToObject(betInfo, "playerID", bet->myplayerid);
	cJSON_AddNumberToObject(betInfo, "invoice_amount", amount);
	cJSON_AddItemToObject(betInfo, "actionResponse", actionResponse);
	dlg_info("%s", cJSON_Print(betInfo));
	// Nanomsg removed - no longer used
	retval = OK;
	return retval;
}

int32_t bet_player_invoice_pay(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars, int amount)
{
	pthread_t pay_t;
	int32_t retval = OK;

	retval = bet_player_create_invoice_request(argjson, bet, amount);
	if (retval != OK)
		return retval;

	if (OS_thread_create(&pay_t, NULL, (void *)bet_player_paymentloop, (void *)bet) != 0) {
		retval = ERR_PTHREAD_LAUNCHING;
	}
	if (pthread_join(pay_t, NULL)) {
		retval = ERR_PTHREAD_JOINING;
	}
	return retval;
}

void bet_player_paymentloop(void *_ptr)
{
	// Nanomsg removed - communication now via websockets only
	// The payment loop is handled through websocket callbacks
	(void)_ptr; // Suppress unused parameter warning
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
