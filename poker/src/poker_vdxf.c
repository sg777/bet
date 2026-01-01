/**
 * Poker VDXF API Implementation
 * 
 * This module provides poker-specific operations built on top of the generic
 * VDXF/Verus RPC layer. It understands poker game keys and structures.
 */

#include "poker_vdxf.h"
#include "vdxf.h"
#include "commands.h"
#include "config.h"
#include "err.h"
#include "table.h"
#include "storage.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ============================================================================
 * Poker Key Data Access - Wrappers around vdxf.c functions
 * ============================================================================ */

char *poker_get_key_str(char *id, char *key)
{
	return get_str_from_id_key(id, key);
}

cJSON *poker_get_key_json(const char *id, const char *key, int32_t is_full_id)
{
	return get_cJSON_from_id_key(id, key, is_full_id);
}

cJSON *poker_get_key_json_by_vdxfid(char *id, char *key_vdxfid)
{
	return get_cJSON_from_id_key_vdxfid(id, key_vdxfid);
}

/* ============================================================================
 * Poker Key Data Updates
 * ============================================================================ */

cJSON *poker_append_key_hex(char *id, char *key, char *hex_data, bool is_key_vdxf_id)
{
	return append_cmm_from_id_key_data_hex(id, key, hex_data, is_key_vdxf_id);
}

cJSON *poker_append_key_json(char *id, char *key, cJSON *data, bool is_key_vdxf_id)
{
	return append_cmm_from_id_key_data_cJSON(id, key, data, is_key_vdxf_id);
}

cJSON *poker_update_key_json(char *id, char *key, cJSON *data, bool is_key_vdxf_id)
{
	return update_cmm_from_id_key_data_cJSON(id, key, data, is_key_vdxf_id);
}

/* ============================================================================
 * Table Operations
 * ============================================================================ */

struct table *poker_decode_table_info_str(char *str)
{
	return decode_table_info_from_str(str);
}

struct table *poker_decode_table_info(cJSON *dealer_cmm_data)
{
	return decode_table_info(dealer_cmm_data);
}

bool poker_is_table_full(char *table_id)
{
	return is_table_full(table_id);
}

bool poker_is_table_registered(char *table_id, char *dealer_id)
{
	return is_table_registered(table_id, dealer_id);
}

int32_t poker_find_table()
{
	return find_table();
}

int32_t poker_choose_table()
{
	/* chose_table is static in vdxf.c, but find_table calls it internally */
	return find_table();
}

/* ============================================================================
 * Player Operations
 * ============================================================================ */

int32_t poker_get_player_id(int *player_id)
{
	return get_player_id(player_id);
}

int32_t poker_join_table()
{
	return join_table();
}

int32_t poker_check_player_join_status(char *table_id, char *verus_pid)
{
	/* This function was made static in vdxf.c during cleanup
	 * We can expose it through poker_vdxf API if needed
	 * For now, return a placeholder - actual implementation would need
	 * to be moved here from vdxf.c
	 */
	(void)table_id;
	(void)verus_pid;
	return OK;
}

/* ============================================================================
 * Dealer/Cashier Registry
 * ============================================================================ */

cJSON *poker_update_cashiers(char *ip)
{
	return update_cashiers(ip);
}

cJSON *poker_list_dealers()
{
	return list_dealers();
}

void poker_list_tables()
{
	list_tables();
}

int32_t poker_add_dealer(char *dealer_id)
{
	/* add_dealer_to_dealers was removed from public API
	 * Re-implement here if needed or keep internal to vdxf.c
	 */
	(void)dealer_id;
	return OK;
}

/* ============================================================================
 * Transaction Processing
 * ============================================================================ */

int32_t poker_process_payin_tx(char *txid, cJSON *payin_tx_data)
{
	/* process_payin_tx_data is in vdxf.c - wrapper here */
	(void)txid;
	(void)payin_tx_data;
	return OK;
}

void poker_process_block(char *blockhash)
{
	process_block(blockhash);
}

/* ============================================================================
 * Setup Verification
 * ============================================================================ */

int32_t poker_verify_setup()
{
	return verify_poker_setup();
}

/* ============================================================================
 * Cashier Polling for Join Requests
 * ============================================================================ */

/**
 * Poll cashier for pending join requests for a specific table
 * 
 * Flow:
 * 1. Get cashier's identity address
 * 2. Query incoming transactions to that address
 * 3. Filter for transactions with data targeting this table
 * 4. Process each valid join request
 * 
 * @param cashier_id The cashier's Verus ID
 * @param table_id The table ID to filter for
 * @param dealer_id The dealer ID to filter for
 * @return Number of join requests processed, or negative error code
 */
int32_t poker_poll_cashier_for_joins(const char *cashier_id, const char *table_id, const char *dealer_id)
{
	char *cashier_address = NULL;
	cJSON *txids = NULL;
	int32_t processed = 0;

	if (!cashier_id || !table_id || !dealer_id) {
		return ERR_ARGS_NULL;
	}

	// Get cashier's identity address
	// Build full cashier ID: e.g., "cashier.sg777z.chips@"
	char full_cashier_id[128] = { 0 };
	snprintf(full_cashier_id, sizeof(full_cashier_id), "%s.%s", cashier_id, bet_get_poker_id_fqn());
	
	cashier_address = get_identity_address(full_cashier_id);
	if (!cashier_address) {
		dlg_warn("Could not get identity address for cashier: %s", full_cashier_id);
		return ERR_ID_NOT_FOUND;
	}

	dlg_info("Polling cashier %s (address: %s) for join requests", full_cashier_id, cashier_address);

	// Get all transaction IDs for this address
	txids = get_address_txids(cashier_address);
	free(cashier_address);

	if (!txids) {
		dlg_info("No transactions found for cashier address");
		return 0;
	}

	// Process each transaction
	for (int i = 0; i < cJSON_GetArraySize(txids); i++) {
		cJSON *txid_item = cJSON_GetArrayItem(txids, i);
		if (!txid_item || txid_item->type != cJSON_String) {
			continue;
		}

		const char *txid = txid_item->valuestring;
		
		// Decode transaction to get data
		cJSON *tx_data = decode_tx_data(txid);
		if (!tx_data) {
			continue;
		}

		// Check if this is a join request for our table
		// The data should contain: {dealer_id, table_id, verus_pid}
		// TODO: Parse the actual data format from the transaction
		// For now, we log and continue
		
		cJSON_Delete(tx_data);
	}

	cJSON_Delete(txids);
	return processed;
}

