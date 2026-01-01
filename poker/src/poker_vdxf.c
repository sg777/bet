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

