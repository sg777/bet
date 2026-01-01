#include "bet.h"
#include "commands.h"
#include "misc.h"
#include "err.h"
#include "game.h"
#include "storage.h"
#include "config.h"
#include "dealer_registration.h"

struct table player_t = { 0 };

/* Forward declarations for internal helpers */
static cJSON *get_cmm_key_data(const char *id, int16_t full_id, const char *key);
static const char *get_key_data_type(const char *key_name);
static char *get_str_from_id_key_vdxfid(char *id, char *key_vdxfid);
static cJSON *get_z_getoperationstatus(char *op_id);
static cJSON *get_t_player_info(char *table_id);
static bool check_if_enough_funds_avail(char *table_id);
static int32_t check_if_d_t_available(char *dealer_id, char *table_id, cJSON **t_table_info);
static cJSON *get_available_t_of_d(char *dealer_id);
static bool is_playerid_added(char *table_id);
static int32_t check_player_join_status(char *table_id, char *verus_pid);

char *get_vdxf_id(const char *key_name)
{
	cJSON *params = NULL, *result = NULL;
	int32_t retval;

	if (!key_name)
		return NULL;

	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, cJSON_CreateString(key_name));
	
	retval = chips_rpc("getvdxfid", params, &result);
	cJSON_Delete(params);
	
	if (retval != OK || !result)
		return NULL;

	return jstr(result, "vdxfid");
}

char *get_key_vdxf_id(char *key_name)
{
	if (!key_name)
		return NULL;

	return get_vdxf_id(get_full_key(key_name));
}

char *get_full_key(char *key_name)
{
	char *full_key = NULL;
	const char *prefix = NULL;

	if (!key_name)
		return NULL;

	prefix = bet_get_key_prefix();
	full_key = calloc(1, 256);
	strncpy(full_key, prefix, 128);
	strncat(full_key, key_name, 128 - strlen(prefix) - 1);
	full_key[255] = '\0';

	return full_key;
}

/* Internal helper - get data type for a key (all keys use bytevector) */
static const char *get_key_data_type(const char *key_name)
{
	(void)key_name; // All keys use bytevector type
	return BYTEVECTOR_VDXF_ID;
}

char *get_key_data_vdxf_id(char *key_name, char *data)
{
	char full_key[256] = { 0 };

	if ((!key_name) || (!data))
		return NULL;

	// Concatenate key_name.data to form the full key
	snprintf(full_key, sizeof(full_key), "%s.%s", key_name, data);

	return get_vdxf_id(full_key);
}

static cJSON *update_with_retry(const char *method, cJSON *params)
{
	int32_t retries = 3, i = 0, retval;
	cJSON *result = NULL;

	do {
		retval = chips_rpc(method, params, &result);
		if (retval == OK && result && jint(result, "error") == 0) {
			// updateidentity returns txid directly as a string, not as {"tx": "txid"}
			// Check if result is a string (direct txid) or object with "tx" field
			const char *txid = NULL;
			if (result->type == cJSON_String) {
				txid = result->valuestring;
			} else {
				txid = jstr(result, "tx");
			}
			
			if (txid) {
				wait_for_a_blocktime();
				if (check_if_tx_exists(txid))
					break;
			} else {
				// No txid to check, assume success
				break;
			}
		}
		dlg_warn("Retrying %s", method);
		if (result) cJSON_Delete(result);
		result = NULL;
		sleep(1);
		i++;
	} while (i < retries);

	if (result && jint(result, "error")) {
		dlg_error("Error in %s", method);
	}
	return result;
}

cJSON *update_cmm(char *id, cJSON *cmm)
{
	cJSON *id_info = NULL, *params = NULL, *result = NULL;

	if ((NULL == id) || (NULL == verus_chips_cli)) {
		return NULL;
	}

	id_info = cJSON_CreateObject();
	cJSON_AddStringToObject(id_info, "name", id);
	cJSON_AddStringToObject(id_info, "parent", get_vdxf_id(bet_get_poker_id_fqn()));
	cJSON_AddItemToObject(id_info, "contentmultimap", cJSON_Duplicate(cmm, 1));

	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, id_info);

	result = update_with_retry("updateidentity", params);
	cJSON_Delete(params);
	return result;
}

cJSON *get_cmm(const char *id, int16_t full_id)
{
	int32_t retval = OK;
	char id_param[256] = { 0 };
	cJSON *params = NULL, *result = NULL, *cmm = NULL;

	if (NULL == id) {
		return NULL;
	}

	strncpy(id_param, id, sizeof(id_param) - 1);
	if (0 == full_id) {
		snprintf(id_param + strlen(id_param), sizeof(id_param) - strlen(id_param), ".%s", bet_get_poker_id_fqn());
	}
	
	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, cJSON_CreateString(id_param));
	cJSON_AddItemToArray(params, cJSON_CreateNumber(-1));
	
	retval = chips_rpc("getidentity", params, &result);
	cJSON_Delete(params);
	
	if (retval != OK || !result) {
		dlg_info("%s", bet_err_str(retval));
		return NULL;
	}

	cmm = cJSON_GetObjectItem(cJSON_GetObjectItem(result, "identity"), "contentmultimap");
	if (cmm) {
		cmm->next = NULL;
	}
	return cmm;
}

/* Internal helper - get key data from CMM */
static cJSON *get_cmm_key_data(const char *id, int16_t full_id, const char *key)
{
	cJSON *cmm = NULL, *cmm_key_data = NULL;

	if ((id == NULL) || (key == NULL))
		return NULL;

	if ((cmm = get_cmm(id, full_id)) == NULL)
		return NULL;
	if ((cmm_key_data = cJSON_GetObjectItem(cmm, key)) == NULL)
		return NULL;
	cmm_key_data->next = NULL;
	return cmm_key_data;
}

/* Internal helper - get cashiers info */
static cJSON *get_cashiers_info(char *cashier_id)
{
	return get_cmm_key_data(cashier_id, 0, get_vdxf_id(verus_config.initialized ? verus_config.cashiers_key : CASHIERS_KEY));
}

/* Internal helper - get z_getoperationstatus */
static cJSON *get_z_getoperationstatus(char *op_id)
{
	int32_t retval;
	cJSON *params = NULL, *op_id_arr = NULL, *result = NULL;

	if (NULL == op_id) {
		return NULL;
	}
	
	op_id_arr = cJSON_CreateArray();
	jaddistr(op_id_arr, op_id);
	
	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, op_id_arr);
	
	retval = chips_rpc("z_getoperationstatus", params, &result);
	cJSON_Delete(params);
	
	if (retval != OK) {
		return NULL;
	}
	return result;
}

cJSON *update_cashiers(char *ip)
{
	cJSON *cashiers_info = NULL, *out = NULL, *ip_obj = NULL, *cashier_ips = NULL;

	cashiers_info = cJSON_CreateObject();
	cashier_ips = get_cashiers_info("cashiers");

	ip_obj = cJSON_CreateObject();
	cJSON_AddStringToObject(ip_obj, get_vdxf_id(STRING_VDXF_ID), ip);

	if (NULL == cashier_ips)
		cashier_ips = cJSON_CreateArray();
	else {
		for (int i = 0; i < cJSON_GetArraySize(cashier_ips); i++) {
			if (0 == strcmp(jstr(cJSON_GetArrayItem(cashier_ips, i), get_vdxf_id(STRING_VDXF_ID)), ip)) {
				dlg_info("%s::%d::The latest data of this ID contains this %s\n", __FUNCTION__,
					 __LINE__, ip);
				goto end;
			}
		}
	}

	cJSON_AddItemToArray(cashier_ips, ip_obj);
	cJSON_AddItemToObject(cashiers_info, get_vdxf_id(verus_config.initialized ? verus_config.cashiers_key : CASHIERS_KEY), cashier_ips);
	out = update_cmm("cashiers", cashiers_info);

end:
	return out;
}

int32_t get_player_id(int *player_id)
{
	char *game_id_str = NULL, hexstr[65];
	cJSON *t_player_info = NULL, *player_info = NULL;

	game_id_str = get_str_from_id_key(player_config.table_id, get_vdxf_id(T_GAME_ID_KEY));
	if (!game_id_str) {
		return ERR_GAME_ID_NOT_FOUND;
	}
	p_deck_info.game_id = bits256_conv(game_id_str);
	dlg_info("%s::%s", game_id_str, bits256_str(hexstr, p_deck_info.game_id));

	t_player_info = get_cJSON_from_id_key_vdxfid(player_config.table_id,
						     get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
	if (!t_player_info) {
		return ERR_T_PLAYER_INFO_NULL;
	}
	player_info = cJSON_CreateArray();
	player_info = jobj(t_player_info, "player_info");
	if (!player_info) {
		return ERR_T_PLAYER_INFO_CORRUPTED;
	}
	dlg_info("%s", cJSON_Print(player_info));

	for (int32_t i = 0; i < cJSON_GetArraySize(player_info); i++) {
		if (strstr(jstri(player_info, i), player_config.verus_pid)) {
			strtok(jstri(player_info, i), "_");
			strtok(NULL, "_");
			*player_id = atoi(strtok(NULL, "_"));
			dlg_info("player id::%d", *player_id);
			return OK;
		}
	}
	return ERR_PLAYER_NOT_EXISTS;
}

int32_t join_table()
{
	int32_t retval = OK;
	cJSON *join_request = NULL, *op_id = NULL, *op_id_info = NULL, *update_result = NULL;
	char *txid = NULL;

	// Step 1: Send funds to cashier address
	dlg_info("Sending payin to cashier: %s", bet_get_cashiers_id_fqn());
	op_id = verus_sendcurrency_data(bet_get_cashiers_id_fqn(), default_chips_tx_fee, NULL);
	if (op_id == NULL)
		return ERR_SENDCURRENCY;
	
	dlg_info("sendcurrency result: %s", cJSON_Print(op_id));

	// sendcurrency returns opid as a string directly or {"opid": "..."} 
	const char *op_id_str = NULL;
	if (op_id->type == cJSON_String) {
		op_id_str = op_id->valuestring;
	} else {
		op_id_str = jstr(op_id, "opid");
		if (!op_id_str) op_id_str = jstr(op_id, "op_id");
	}
	
	if (!op_id_str) {
		dlg_error("No operation ID returned from sendcurrency");
		return ERR_SENDCURRENCY;
	}

	// Wait for operation to complete
	dlg_info("Waiting for operation: %s", op_id_str);
	op_id_info = get_z_getoperationstatus((char *)op_id_str);
	if (op_id_info) {
		dlg_info("Operation status: %s", cJSON_Print(op_id_info));
		while (jitem(op_id_info, 0) && 
		       jstr(jitem(op_id_info, 0), "status") &&
		       0 == strcmp(jstr(jitem(op_id_info, 0), "status"), "executing")) {
			sleep(1);
			op_id_info = get_z_getoperationstatus((char *)op_id_str);
		}
		
		if (!jitem(op_id_info, 0) || 
		    !jstr(jitem(op_id_info, 0), "status") ||
		    0 != strcmp(jstr(jitem(op_id_info, 0), "status"), "success")) {
			dlg_error("sendcurrency operation failed");
			return ERR_SENDCURRENCY;
		}

		txid = jstr(jobj(jitem(op_id_info, 0), "result"), "txid");
		if (!txid) {
			dlg_error("Failed to get txid from operation result");
			return ERR_SENDCURRENCY;
		}
		strncpy(player_config.txid, txid, sizeof(player_config.txid) - 1);
		dlg_info("payin_tx: %s", player_config.txid);
	} else {
		dlg_error("Failed to get operation status");
		return ERR_SENDCURRENCY;
	}

	// Step 2: Update player identity with join request info
	// This allows dealer to discover which player wants to join which table
	// Player checks t_player_info to know if they're accepted (no status field needed)
	join_request = cJSON_CreateObject();
	cJSON_AddStringToObject(join_request, "dealer_id", player_config.dealer_id);
	cJSON_AddStringToObject(join_request, "table_id", player_config.table_id);
	cJSON_AddStringToObject(join_request, "cashier_id", bet_get_cashiers_id_fqn());
	cJSON_AddStringToObject(join_request, "payin_tx", player_config.txid);

	dlg_info("Updating player identity with join request: %s", cJSON_Print(join_request));
	update_result = append_cmm_from_id_key_data_cJSON(
		player_config.verus_pid, 
		"chips.vrsc::poker.sg777z.p_join_request",
		join_request, 
		false
	);

	cJSON_Delete(join_request);

	if (!update_result) {
		dlg_error("Failed to update player identity with join request");
		return ERR_UPDATEIDENTITY;
	}
	dlg_info("Join request stored on player identity");

	// Step 3: Wait for dealer to add player to table
	retval = check_player_join_status(player_config.table_id, player_config.verus_pid);
	if (retval) {
		/*
		 TODO::This is where TX is success but verus_pid is not added to the table.
		 In such scenarios, using dispute resolution protocol the player can get funds back.
		*/
		return retval;
	}

	return retval;
}

static void copy_table_to_struct_t(cJSON *t_table_info)
{
	player_t.max_players = jint(t_table_info, "max_players");
	float_to_uint32_s(&player_t.big_blind, jdouble(t_table_info, "big_blind"));
	float_to_uint32_s(&player_t.min_stake, jdouble(t_table_info, "min_stake"));
	float_to_uint32_s(&player_t.max_stake, jdouble(t_table_info, "max_stake"));
	strcpy(player_t.table_id, jstr(t_table_info, "table_id"));
	strcpy(player_t.dealer_id, jstr(t_table_info, "dealer_id"));
}

int32_t chose_table()
{
	int32_t retval = OK;
	cJSON *t_table_info = NULL, *dealer_ids = NULL;

	t_table_info = cJSON_CreateObject();
	retval = check_if_d_t_available(player_config.dealer_id, player_config.table_id, &t_table_info);
	if (retval == OK) {
		copy_table_to_struct_t(t_table_info);
		dlg_info("Configured Dealer ::%s, Table ::%s are chosen", player_t.dealer_id, player_t.table_id);
		return retval;
	}

	if (retval == ERR_DUPLICATE_PLAYERID)
		return retval;

	dlg_info("Unable to join preconfigured table ::%s, checking for any other available tables...",
		 bet_err_str(retval));

	dealer_ids = cJSON_CreateArray();
	dealer_ids = get_cJSON_from_id_key(bet_get_dealers_id_fqn(), verus_config.initialized ? verus_config.dealers_key : DEALERS_KEY, 1);
	if (!dealer_ids) {
		return ERR_NO_DEALERS_FOUND;
	}
	for (int32_t i = 0; i < cJSON_GetArraySize(dealer_ids); i++) {
		t_table_info = get_available_t_of_d(jstri(dealer_ids, i));
		if (t_table_info) {
			strncpy(player_config.dealer_id, jstri(dealer_ids, i), sizeof(player_config.dealer_id));
			strncpy(player_config.table_id, jstr(t_table_info, "table_id"), sizeof(player_config.table_id));
			copy_table_to_struct_t(t_table_info);
			dlg_info("Available Dealer ::%s, Table ::%s are chosen", player_t.dealer_id, player_t.table_id);
			return OK;
		}
	}
	return ERR_NO_TABLES_FOUND;
}

int32_t find_table()
{
	int32_t retval = OK;

	/*
	* Check if the configured table meets the preconditions for the player to join the table
	*/
	if ((retval = chose_table()) != OK) {
		return retval;
	}
	/*
	* Check if the player wallet has suffiecient funds to join the table chosen
	*/
	if (!check_if_enough_funds_avail(player_t.table_id)) {
		return ERR_CHIPS_INSUFFICIENT_FUNDS;
	}

	return retval;
}

bool is_id_exists(const char *id, int16_t full_id)
{
	int32_t retval = OK;
	char id_param[256] = { 0 };
	cJSON *params = NULL, *result = NULL;

	if (!id || id[0] == '\0') {
		dlg_error("is_id_exists called with empty ID");
		return false;
	}

	strncpy(id_param, id, sizeof(id_param) - 1);
	if (0 == full_id) {
		snprintf(id_param + strlen(id_param), sizeof(id_param) - strlen(id_param), ".%s", bet_get_poker_id_fqn());
	}
	
	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, cJSON_CreateString(id_param));
	cJSON_AddItemToArray(params, cJSON_CreateNumber(-1));
	
	retval = chips_rpc("getidentity", params, &result);
	cJSON_Delete(params);
	
	if (result) cJSON_Delete(result);
	return (retval == OK);
}

int32_t check_player_join_status(char *table_id, char *verus_pid)
{
	int32_t block_count = 0, retval = ERR_PLAYER_NOT_ADDED;
	int32_t block_wait_time =
		5; //This is the wait time in number of blocks upto which player can look for its table joining update

	block_count = chips_get_block_count() + block_wait_time;
	dlg_info("Checking player joining status, default wait time is 5 blocks");
	do {
		cJSON *t_player_info = get_t_player_info(table_id);
		cJSON *player_info = jobj(t_player_info, "player_info");
		for (int32_t i = 0; (player_info) && (i < cJSON_GetArraySize(player_info)); i++) {
			if (strstr(jstri(player_info, i), player_config.verus_pid)) {
				dlg_info("t_player_info :: %s", cJSON_Print(t_player_info));
				return OK;
			}
		}
		sleep(2);
	} while (chips_get_block_count() <= block_count);

	return retval;
}

cJSON *verus_sendcurrency_data(const char *id, double amount, cJSON *data)
{
	int32_t retval, minconf = 1;
	double fee = 0.0001;
	cJSON *currency_detail = NULL, *result = NULL, *tx_params = NULL, *params = NULL;

	(void)data; // Data parameter currently unused - join info stored on player identity instead

	if (amount < 0) {
		dlg_error("Amount cannot be negative");
		return NULL;
	}

	if (amount == 0) {
		amount = default_chips_tx_fee;
	}
	//Full ID needs to be provided for the sendcurrency command
	if ((!id) || (!is_id_exists(id, 1))) {
		dlg_error("Invalid ID provided");
		return NULL;
	}

	tx_params = cJSON_CreateArray();

	// Output: currency to identity address
	currency_detail = cJSON_CreateObject();
	cJSON_AddStringToObject(currency_detail, "currency", CHIPS);
	cJSON_AddNumberToObject(currency_detail, "amount", amount);
	cJSON_AddStringToObject(currency_detail, "address", id);
	cJSON_AddItemToArray(tx_params, currency_detail);

	// Build params array: [source, destinations, minconf, fee]
	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, cJSON_CreateString("*"));
	cJSON_AddItemToArray(params, cJSON_Duplicate(tx_params, 1));
	cJSON_AddItemToArray(params, cJSON_CreateNumber(minconf));
	cJSON_AddItemToArray(params, cJSON_CreateNumber(fee));

	dlg_info("sendcurrency params: %s", cJSON_Print(params));

	retval = chips_rpc("sendcurrency", params, &result);
	cJSON_Delete(params);
	cJSON_Delete(tx_params);

	if (retval != OK) {
		dlg_error("sendcurrency failed: %s", bet_err_str(retval));
		return NULL;
	}
	return result;
}

cJSON *getaddressutxos(char verus_addresses[][100], int n)
{
	int32_t retval;
	cJSON *addresses = NULL, *addr_info = NULL, *params = NULL, *result = NULL;

	addresses = cJSON_CreateArray();
	addr_info = cJSON_CreateObject();

	for (int32_t i = 0; i < n; i++) {
		cJSON_AddItemToArray(addresses, cJSON_CreateString(verus_addresses[i]));
	}

	cJSON_AddItemToObject(addr_info, "addresses", addresses);
	
	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, addr_info);
	
	retval = chips_rpc("getaddressutxos", params, &result);
	cJSON_Delete(params);
	
	if (retval != OK) {
		return NULL;
	}
	return result;
}

struct table *decode_table_info_from_str(char *str)
{
	uint8_t *table_data = NULL;
	struct table *t = NULL;

	if (str == NULL)
		return NULL;

	table_data = calloc(1, (strlen(str) + 1) / 2);
	decode_hex(table_data, (strlen(str) + 1) / 2, str);

	t = calloc(1, sizeof(struct table));
	t = (struct table *)table_data;

end:
	return t;
}

struct table *decode_table_info(cJSON *dealer_cmm_data)
{
	char *str = NULL;
	uint8_t *table_data = NULL;
	struct table *t = NULL;

	if (!dealer_cmm_data)
		return NULL;

	str = jstr(cJSON_GetArrayItem(dealer_cmm_data, 0), get_vdxf_id(STRING_VDXF_ID));

	if (!str)
		return NULL;

	table_data = calloc(1, (strlen(str) + 1) / 2);
	decode_hex(table_data, (strlen(str) + 1) / 2, str);

	t = calloc(1, sizeof(struct table));
	t = (struct table *)table_data;

end:
	return t;
}

static void update_player_ids(cJSON *t_player_info)
{
	cJSON *player_info = jobj(t_player_info, "player_info");
	if (player_info == NULL) {
		dlg_error("Failed to retrieve player info");
		return;
	}

	num_of_players = cJSON_GetArraySize(player_info);
	for (int32_t i = 0; i < num_of_players; i++) {
		char *player_record = jstri(player_info, i);
		if (player_record) {
			// The player record is in the form of playerverusid_txid_pid, using strtok the first token we get is verus player ID.
			char *player_id = strtok(player_record, "_");
			if (player_id) {
				strncpy(player_ids[i], player_id, sizeof(player_ids[i]) - 1);
				player_ids[i][sizeof(player_ids[i]) - 1] = '\0'; // Ensure null-termination
				dlg_info("Player %d ID: %s", i + 1, player_ids[i]);
			}
		}
	}
}

bool is_table_full(char *table_id)
{
	if (table_id == NULL) {
		dlg_error("Invalid table ID provided");
		return false;
	}

	int32_t game_state = get_game_state(table_id);
	if (game_state < G_TABLE_STARTED) {
		return false;
	}

	char *game_id_str = get_str_from_id_key(table_id, T_GAME_ID_KEY);
	if (game_id_str == NULL) {
		dlg_error("Failed to retrieve game ID for table %s", table_id);
		return false;
	}

	cJSON *t_player_info =
		get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
	cJSON *t_table_info =
		get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_TABLE_INFO_KEY, game_id_str));

	if (t_player_info == NULL || t_table_info == NULL) {
		free(game_id_str);
		if (t_player_info != NULL) cJSON_Delete(t_player_info);
		if (t_table_info != NULL) cJSON_Delete(t_table_info);
		return false;
	}

	int num_players = jint(t_player_info, "num_players");
	int max_players = jint(t_table_info, "max_players");

	bool is_full = (num_players >= max_players);

	if (is_full && bet_node_type == dealer) {
		update_player_ids(t_player_info);
	}

	if (is_full) {
		dlg_info("Table %s is full (%d/%d players)", table_id, num_players, max_players);
	}

	free(game_id_str);
	cJSON_Delete(t_player_info);
	cJSON_Delete(t_table_info);

	return is_full;
}

/* Internal helper - check if player ID is already added to table */
static bool is_playerid_added(char *table_id)
{
	int32_t game_state;
	char *game_id_str = NULL;
	cJSON *t_player_info = NULL, *player_info = NULL;

	game_state = get_game_state(table_id);
	if (game_state == G_TABLE_STARTED) {
		game_id_str = get_str_from_id_key(table_id, T_GAME_ID_KEY);
		t_player_info = get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
		player_info = jobj(t_player_info, "player_info");
		for (int32_t i = 0; i < cJSON_GetArraySize(player_info); i++) {
			if (strstr(jstri(player_info, i), player_config.verus_pid))
				return true;
		}
	}
	return false;
}

/* Internal helper - get available table of dealer */
static cJSON *get_available_t_of_d(char *dealer_id)
{
	int32_t game_state;
	cJSON *t_table_info = NULL;

	if (!dealer_id)
		return NULL;

	t_table_info = get_cJSON_from_id_key(dealer_id, T_TABLE_INFO_KEY, 0);
	if (!t_table_info)
		return NULL;

	game_state = get_game_state(jstr(t_table_info, "table_id"));

	if ((game_state == G_TABLE_STARTED) && (!is_table_full(jstr(t_table_info, "table_id"))) &&
	    (!is_playerid_added(jstr(t_table_info, "table_id")))) {
		return t_table_info;
	}
	return NULL;
}

/* Internal helper - check if dealer table is available */
static int32_t check_if_d_t_available(char *dealer_id, char *table_id, cJSON **t_table_info)
{
	int32_t retval = OK;
	int32_t game_state;

	if ((!dealer_id) || (!table_id) || (!is_dealer_registered(dealer_id)) || (!is_id_exists(table_id, 0))) {
		return ERR_CONFIG_PLAYER_ARGS;
	}

	*t_table_info = get_cJSON_from_id_key(dealer_id, T_TABLE_INFO_KEY, 0);
	if (*t_table_info == NULL) {
		return ERR_T_TABLE_INFO_NULL;
	}

	if ((0 == strcmp(jstr(*t_table_info, "table_id"), table_id))) {
		if (is_playerid_added(table_id)) {
			return ERR_DUPLICATE_PLAYERID;
		}
		game_state = get_game_state(table_id);
		if (game_state < G_TABLE_STARTED) {
			return ERR_TABLE_IS_NOT_STARTED;
		} else if (game_state > G_TABLE_STARTED) {
			return ERR_TABLE_IS_FULL;
		}
		if (is_table_full(table_id)) {
			return ERR_TABLE_IS_FULL;
		}
	}
	return retval;
}

/* Internal helper - check if enough funds available */
static bool check_if_enough_funds_avail(char *table_id)
{
	double balance = 0.0, min_stake = 0.0;
	char *game_id_str = NULL;
	cJSON *t_table_info = NULL;

	game_id_str = get_str_from_id_key(table_id, get_vdxf_id(T_GAME_ID_KEY));
	if (!game_id_str)
		return false;

	t_table_info = get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_TABLE_INFO_KEY, game_id_str));
	if (t_table_info) {
		min_stake = jdouble(t_table_info, "min_stake");
		balance = chips_get_balance();
		if (balance > min_stake + RESERVE_AMOUNT)
			return true;
	}
	return false;
}

/* Internal helper - get player info for table */
static cJSON *get_t_player_info(char *table_id)
{
	int32_t game_state;
	char *game_id_str = NULL;
	cJSON *t_player_info = NULL;

	game_state = get_game_state(table_id);
	if (game_state == G_TABLE_STARTED) {
		game_id_str = get_str_from_id_key(table_id, T_GAME_ID_KEY);
		t_player_info = get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
	}
	return t_player_info;
}

/*
key --> Full key
*/
char *get_str_from_id_key(char *id, char *key)
{
	cJSON *cmm = NULL;
	cJSON *first_item = NULL;

	cmm = get_cmm_key_data(id, 0, get_vdxf_id(key));
	if (!cmm) {
		return NULL;
	}

	first_item = cJSON_GetArrayItem(cmm, 0);
	if (!first_item) {
		return NULL;
	}

	// VDXF data can be stored in two formats:
	// 1. Simple array of hex strings: ["hexdata"]
	// 2. Nested object format: [{"bytevector_vdxfid": "hexdata"}]
	if (first_item->type == cJSON_String) {
		// Simple format: array contains hex string directly
		return first_item->valuestring;
	} else if (first_item->type == cJSON_Object) {
		// Nested format: array contains object with bytevector key
		return jstr(first_item, get_vdxf_id(get_key_data_type(key)));
	}

	return NULL;
}

static char *get_str_from_id_key_vdxfid(char *id, char *key_vdxfid)
{
	cJSON *cmm = NULL;
	cJSON *first_item = NULL;

	cmm = get_cmm_key_data(id, 0, key_vdxfid);
	if (!cmm) {
		return NULL;
	}

	first_item = cJSON_GetArrayItem(cmm, 0);
	if (!first_item) {
		return NULL;
	}

	// VDXF data can be stored in two formats:
	// 1. Simple array of hex strings: ["hexdata"]
	// 2. Nested object format: [{"bytevector_vdxfid": "hexdata"}]
	if (first_item->type == cJSON_String) {
		return first_item->valuestring;
	} else if (first_item->type == cJSON_Object) {
		return jstr(first_item, get_vdxf_id(get_key_data_type(key_vdxfid)));
	}

	return NULL;
}

cJSON *get_cJSON_from_id_key(const char *id, const char *key, int32_t is_full_id)
{
	cJSON *cmm = NULL;
	cJSON *first_item = NULL;
	const char *hex_str = NULL;

	cmm = get_cmm_key_data(id, is_full_id, get_vdxf_id(key));
	if (!cmm) {
		return NULL;
	}

	first_item = cJSON_GetArrayItem(cmm, 0);
	if (!first_item) {
		return NULL;
	}

	// VDXF data can be stored in two formats:
	// 1. Simple array of hex strings: ["hexdata"]
	// 2. Nested object format: [{"bytevector_vdxfid": "hexdata"}]
	if (first_item->type == cJSON_String) {
		// Simple format: array contains hex string directly
		hex_str = first_item->valuestring;
	} else if (first_item->type == cJSON_Object) {
		// Nested format: array contains object with bytevector key
		hex_str = jstr(first_item, get_vdxf_id(get_key_data_type(key)));
	}

	if (!hex_str) {
		return NULL;
	}

	return hex_cJSON(hex_str);
}

cJSON *get_cJSON_from_table_id_key(char *table_id, char *key)
{
	char *game_id_str = NULL;
	cJSON *cmm = NULL;

	game_id_str = get_str_from_id_key(table_id, T_GAME_ID_KEY);
	if (!game_id_str)
		return NULL;

	cmm = get_cmm_key_data(table_id, 0, get_key_data_vdxf_id(key, game_id_str));
	if (cmm) {
		return hex_cJSON(jstr(cJSON_GetArrayItem(cmm, 0), get_vdxf_id(get_key_data_type(key))));
	}
	return NULL;
}

cJSON *get_cJSON_from_id_key_vdxfid(char *id, char *key_vdxfid)
{
	cJSON *cmm = NULL;
	cJSON *first_item = NULL;
	const char *hex_str = NULL;

	cmm = get_cmm_key_data(id, 0, key_vdxfid);
	if (!cmm) {
		return NULL;
	}

	first_item = cJSON_GetArrayItem(cmm, 0);
	if (!first_item) {
		return NULL;
	}

	// VDXF data can be stored in two formats:
	// 1. Simple array of hex strings: ["hexdata"]
	// 2. Nested object format: [{"bytevector_vdxfid": "hexdata"}]
	if (first_item->type == cJSON_String) {
		// Simple format: array contains hex string directly
		hex_str = first_item->valuestring;
	} else if (first_item->type == cJSON_Object) {
		// Nested format: array contains object with bytevector key
		hex_str = jstr(first_item, get_vdxf_id(get_key_data_type(key_vdxfid)));
	}

	if (!hex_str) {
		return NULL;
	}

	return hex_cJSON(hex_str);
}

cJSON *append_cmm_from_id_key_data_hex(char *id, char *key, char *hex_data, bool is_key_vdxf_id)
{
	char *data_type = NULL, *data_key = NULL;
	cJSON *data_obj = NULL, *cmm_obj = NULL;

	cmm_obj = cJSON_CreateObject();
	cmm_obj = get_cmm(id, 0);

	//dlg_info("cmm_old::%s", cJSON_Print(cmm_obj));
	if (is_key_vdxf_id) {
		data_type = get_vdxf_id(BYTEVECTOR_VDXF_ID);
		data_key = key;
	} else {
		data_type = get_vdxf_id(get_key_data_type(key));
		data_key = get_vdxf_id(key);
	}

	data_obj = cJSON_CreateObject();
	jaddstr(data_obj, data_type, hex_data);
	//dlg_info("data::%s", cJSON_Print(data_obj));

	cJSON_DeleteItemFromObject(cmm_obj, data_key);
	cJSON_AddItemToObject(cmm_obj, data_key, data_obj);
	//dlg_info("cmm_new::%s", cJSON_Print(cmm_obj));

	return update_cmm(id, cmm_obj);
}

cJSON *append_cmm_from_id_key_data_cJSON(char *id, char *key, cJSON *data, bool is_key_vdxf_id)
{
	char *hex_data = NULL;

	if (!data)
		return NULL;

	cJSON_hex(data, &hex_data);
	if (!hex_data)
		return NULL;

	return append_cmm_from_id_key_data_hex(id, key, hex_data, is_key_vdxf_id);
}

cJSON *update_cmm_from_id_key_data_hex(char *id, char *key, char *hex_data, bool is_key_vdxf_id)
{
	if (!id || !key || !hex_data) {
		dlg_error("%s: Invalid input parameters", __func__);
		return NULL;
	}

	char *data_type = get_vdxf_id(BYTEVECTOR_VDXF_ID);
	char *data_key = is_key_vdxf_id ? key : get_vdxf_id(key);

	if (!data_type || !data_key) {
		dlg_error("%s: Failed to determine data type or key", __func__);
		return NULL;
	}

	cJSON *data_obj = cJSON_CreateObject();
	if (!data_obj) {
		dlg_error("%s: Failed to create data JSON object", __func__);
		return NULL;
	}

	cJSON_AddStringToObject(data_obj, data_type, hex_data);

	cJSON *data_key_obj = cJSON_CreateObject();
	if (!data_key_obj) {
		dlg_error("%s: Failed to create key JSON object", __func__);
		cJSON_Delete(data_obj);
		return NULL;
	}

	cJSON_AddItemToObject(data_key_obj, data_key, data_obj);

	cJSON *result = update_cmm(id, data_key_obj);
	if (!result) {
		dlg_error("%s: Failed to update CMM", __func__);
		cJSON_Delete(data_key_obj);
	}

	return result;
}

cJSON *update_cmm_from_id_key_data_cJSON(char *id, char *key, cJSON *data, bool is_key_vdxf_id)
{
	if (!id || !key || !data) {
		dlg_error("%s: Invalid input parameters", __func__);
		return NULL;
	}

	char *hex_data = NULL;
	cJSON *result = NULL;

	cJSON_hex(data, &hex_data);
	if (!hex_data) {
		dlg_error("%s: Failed to convert cJSON to HEX", __func__);
		return NULL;
	}

	result = update_cmm_from_id_key_data_hex(id, key, hex_data, is_key_vdxf_id);
	if (!result) {
		dlg_error("%s: Failed to update CMM with hex data", __func__);
	}

	free(hex_data);
	return result;
}

static int32_t do_payin_tx_checks(char *txid, cJSON *payin_tx_data)
{
	int32_t retval = OK, game_state;
	double amount = 0;
	char *game_id_str = NULL, *table_id = NULL, *dealer_id = NULL, *verus_pid = NULL;
	cJSON *t_player_info = NULL, *player_info = NULL, *t_table_info = NULL;

	if ((!txid) || (!payin_tx_data))
		return ERR_NO_PAYIN_DATA;

	dlg_info("Payin TX Data ::%s", cJSON_Print(payin_tx_data));
	table_id = jstr(payin_tx_data, "table_id");
	dealer_id = jstr(payin_tx_data, "dealer_id");
	verus_pid = jstr(payin_tx_data, "verus_pid");

	//Check the table ID and dealer ID mentioned in Payin TX are valid.
	if (!is_id_exists(table_id, 0) || !is_id_exists(dealer_id, 0) || !is_id_exists(verus_pid, 0)) {
		return ERR_ID_NOT_FOUND;
	}

	//Check if the table is started, table is started by the dealer during dealer init.
	game_state = get_game_state(table_id);
	if (game_state != G_TABLE_STARTED) {
		return ERR_INVALID_TABLE_STATE;
	}

	//	Check whether if the table is FULL or not.
	if (is_table_full(table_id)) {
		return ERR_TABLE_IS_FULL;
	}

	//Load the table info into local variables for further checks
	game_id_str = get_str_from_id_key(table_id, T_GAME_ID_KEY);
	t_table_info = get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_TABLE_INFO_KEY, game_id_str));

	//Check the amount of funds that the player deposited at Cashier and see if these funds are with in the range of [min-max] stake.
	amount = chips_get_balance_on_address_from_tx(get_vdxf_id(bet_get_cashiers_id_fqn()), txid);
	if ((amount < jdouble(t_table_info, "min_stake")) && (amount > jdouble(t_table_info, "max_stake"))) {
		dlg_error("funds deposited ::%f should be in the range %f::%f\n", amount,
			  jdouble(t_table_info, "min_stake"), jdouble(t_table_info, "max_stake"));
		return ERR_PAYIN_TX_INVALID_FUNDS;
	}
	// Check for a duplicate join request and duplicate verus_pid, verus player ID should be unique so that ensures a player can occupy atmost one position on the table.
	t_player_info = cJSON_CreateObject();
	t_player_info = get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
	if (!t_player_info) {
		return OK; //Means no one joined yet
	}

	player_info = cJSON_CreateArray();
	player_info = cJSON_GetObjectItem(t_player_info, "player_info");
	for (int32_t i = 0; i < cJSON_GetArraySize(player_info); i++) {
		if ((strstr(jstri(player_info, i), verus_pid)) && (strstr(jstri(player_info, i), txid))) {
			return ERR_DUP_PAYIN_UPDATE_REQ;
		} else if (strstr(jstri(player_info, i), verus_pid)) {
			return ERR_DUPLICATE_PLAYERID;
		}
	}
	return retval;
}

/*
* Reads the key T_PLAYER_INFO_KEY
* Increment num_players and append player data to player_info array.
*/
static cJSON *compute_updated_t_player_info(char *txid, cJSON *payin_tx_data)
{
	int32_t num_players = 0;
	char *game_id_str = NULL, pa_tx_id[256] = { 0 };
	cJSON *t_player_info = NULL, *player_info = NULL, *updated_t_player_info = NULL;

	game_id_str = get_str_from_id_key_vdxfid(jstr(payin_tx_data, "table_id"), get_vdxf_id(T_GAME_ID_KEY));
	if (!game_id_str)
		return NULL;

	t_player_info = get_cJSON_from_id_key_vdxfid(jstr(payin_tx_data, "table_id"),
						     get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
	player_info = cJSON_CreateArray();
	if (t_player_info) {
		num_players = jint(t_player_info, "num_players");
		player_info = cJSON_GetObjectItem(t_player_info, "player_info");
		if (!player_info) {
			dlg_error("Error with data on Key :: %s", T_PLAYER_INFO_KEY);
			return NULL;
		}
	}
	sprintf(pa_tx_id, "%s_%s_%d", jstr(payin_tx_data, "verus_pid"), txid, num_players);
	jaddistr(player_info, pa_tx_id);
	num_players++;

	updated_t_player_info = cJSON_CreateObject();
	cJSON_AddNumberToObject(updated_t_player_info, "num_players", num_players);
	cJSON_AddItemToObject(updated_t_player_info, "player_info", player_info);

	return updated_t_player_info;
}

int32_t process_payin_tx_data(char *txid, cJSON *payin_tx_data)
{
	int32_t retval = OK;
	char *game_id_str = NULL;
	cJSON *updated_t_player_info = NULL, *out = NULL;

	retval = do_payin_tx_checks(txid, payin_tx_data);
	if (retval == ERR_DUP_PAYIN_UPDATE_REQ) {
		dlg_warn("%s", bet_err_str(retval));
		return OK;
	} else if (retval != OK) {
		/*
			Reverse the payin TX for any of the scenarios where player is unable to join the table. Like,
			1. Table is FULL
			2. Table ID doesn't exists
			3. Duplicate PA
		*/
		dlg_error("Reversing the Payin TX due to :: %s", bet_err_str(retval));
		double amount = chips_get_balance_on_address_from_tx(get_vdxf_id(CASHIERS_ID_FQN), txid);
		cJSON *tx = chips_transfer_funds(amount, jstr(payin_tx_data, "verus_pid"));
		dlg_info("Payback TX::%s", cJSON_Print(tx));
		return retval;
	}

	game_id_str = get_str_from_id_key_vdxfid(jstr(payin_tx_data, "table_id"), get_vdxf_id(T_GAME_ID_KEY));
	if (!game_id_str) {
		return ERR_GAME_ID_NOT_FOUND;
	}

	// Prepare the cJSON object with the new player details
	updated_t_player_info = compute_updated_t_player_info(txid, payin_tx_data);
	if (!updated_t_player_info) {
		return ERR_T_PLAYER_INFO_UPDATE;
	}

	//Update the t_player_info.<game_id> key of the table id with newly join requested player details.
	dlg_info("%s", cJSON_Print(updated_t_player_info));
	out = append_cmm_from_id_key_data_cJSON(jstr(payin_tx_data, "table_id"),
						get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str),
						updated_t_player_info, true);
	dlg_info("%s", cJSON_Print(out));

	return retval;
}

void process_block(char *blockhash)
{
	int32_t blockcount = 0, retval = OK;
	char verus_addr[1][100];
	strncpy(verus_addr[0], bet_get_cashiers_id_fqn(), sizeof(verus_addr[0]) - 1);
	verus_addr[0][sizeof(verus_addr[0]) - 1] = '\0';
	cJSON *blockjson = NULL, *payin_tx_data = NULL;

	if (!bet_is_new_block_set()) {
		dlg_info("Flag to process new block info is not set in blockchain_config.ini");
		return;
	}

	blockjson = cJSON_CreateObject();
	blockjson = chips_get_block_from_block_hash(blockhash);

	if (blockjson == NULL) {
		dlg_error("Failed to get block info from blockhash");
		return;
	}
	blockcount = jint(blockjson, "height");
	if (blockcount <= 0) {
		dlg_error("Invalid block height, check if the underlying blockchain is syncing right");
		return;
	}
	dlg_info("received blockhash of block height = %d", blockcount);

	if (!is_id_exists(bet_get_cashiers_id_fqn(), 1)) {
		dlg_error("Cashiers ID ::%s doesn't exists", bet_get_cashiers_id_fqn());
		return;
	}

	/*
	* List all the utxos attached to the registered cashiers, like cashier.sg777z.chips.vrsc@
	* Look for the utxos that matches to the current processing block height
	* Extract the tx data 
	* Process the tx data to see if this tx is intended and related to poker
	* If the tx is related to poker, do all checks and add the players info to the table
	*/
	cJSON *argjson = cJSON_CreateObject();
	argjson = getaddressutxos(verus_addr, 1);
	for (int32_t i = 0; i < cJSON_GetArraySize(argjson); i++) {
		if (jint(cJSON_GetArrayItem(argjson, i), "height") == blockcount) {
			dlg_info("tx_id::%s", jstr(cJSON_GetArrayItem(argjson, i), "txid"));
			payin_tx_data = chips_extract_tx_data_in_JSON(jstr(cJSON_GetArrayItem(argjson, i), "txid"));
			if (!payin_tx_data)
				continue;
			retval = process_payin_tx_data(jstr(cJSON_GetArrayItem(argjson, i), "txid"), payin_tx_data);
			if (retval != OK) {
				dlg_error("In processing TX :: %s, Err::%s",
					  jstr(cJSON_GetArrayItem(argjson, i), "txid"), bet_err_str(retval));
				retval = OK;
			}
		}
	}
end:
	dlg_info("Done\n");
}

/*
* The list_dealers() function fetches data from the dealers_key in dealer_id identity
* The data is stored in the following JSON format:
* {
*   "dealers": [
*     "dealer_id1@", 
*     "dealer_id2@",
*     ...
*   ]
* }
* Returns: cJSON array containing dealer IDs, or NULL if no dealers found
*/
cJSON *list_dealers()
{
	cJSON *dealers = NULL, *dealers_arr = NULL;

	dealers = cJSON_CreateObject();
	dealers = get_cJSON_from_id_key(bet_get_dealers_id_fqn(), verus_config.initialized ? verus_config.dealers_key : DEALERS_KEY, 1);

	if (!dealers) {
		dlg_info("No dealers has been added to dealer.sg777z.chips.vrsc@ yet.");
		return NULL;
	}
	dealers_arr = cJSON_GetObjectItem(dealers, "dealers");
	return dealers_arr;
}

void list_tables()
{
	cJSON *dealers = NULL;

	dealers = list_dealers();
	for (int i = 0; i < cJSON_GetArraySize(dealers); i++) {
		dlg_info("dealer_id::%s", jstri(dealers, i));
		cJSON *table_info = get_cJSON_from_id_key(jstri(dealers, i), T_TABLE_INFO_KEY, 0);
		if (table_info) {
			dlg_info("%s", cJSON_Print(table_info));
			if (is_id_exists(jstr(table_info, "table_id"), 0)) {
				char *game_id = NULL;
				game_id = get_str_from_id_key(jstr(table_info, "table_id"), get_vdxf_id(T_GAME_ID_KEY));
				if (game_id) {
					cJSON *player_info = get_cJSON_from_id_key_vdxfid(
						jstr(table_info, "table_id"),
						get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id));
					dlg_info("Player Info of Table ::%s is ::%s", jstr(table_info, "table_id"),
						 cJSON_Print(player_info));
				}
			}
		}
	}
}

int32_t verify_poker_setup()
{
	if (!is_id_exists(bet_get_cashiers_id_fqn(), 1)) {
		dlg_error("Cashiers ID %s does not exist", bet_get_cashiers_id_fqn());
		return ERR_CASHIERS_ID_NOT_FOUND;
	}

	if (!is_id_exists(bet_get_dealers_id_fqn(), 1)) {
		dlg_error("Dealers ID %s does not exist", bet_get_dealers_id_fqn());
		return ERR_DEALERS_ID_NOT_FOUND;
	}

	cJSON *dealers = get_cJSON_from_id_key(bet_get_dealers_id_fqn(), verus_config.initialized ? verus_config.dealers_key : DEALERS_KEY, 1);
		if (!dealers) {
			dlg_error("No dealers found in %s", bet_get_dealers_id_fqn());
		return ERR_NO_DEALERS_FOUND;
	}

	if (cJSON_GetArraySize(dealers) == 0) {
		dlg_error("Dealers list is empty");
		cJSON_Delete(dealers);
		return ERR_NO_DEALERS_REGISTERED;
	}

	dlg_info("Poker system is ready. Found %d registered dealer(s)", cJSON_GetArraySize(dealers));
	cJSON_Delete(dealers);

	return OK;
}

int32_t add_dealer_to_dealers(char *dealer_id)
{
	cJSON *dealers = NULL, *out = NULL;
	int32_t dealer_added = 0;

	dealers = list_dealers();

	for (int32_t i = 0; i < cJSON_GetArraySize(dealers); i++) {
		if (0 == strcmp(dealer_id, jstri(dealers, i))) {
			dealer_added = 1;
			break;
		}
	}
	if (!dealer_added) {
		if (!dealers) {
			dealers = cJSON_CreateArray();
		}
		cJSON_AddItemToArray(dealers, cJSON_CreateString(dealer_id));
		out = append_cmm_from_id_key_data_cJSON(verus_config.initialized ? verus_config.dealers_short : "dealers", verus_config.initialized ? verus_config.dealers_key : DEALERS_KEY, dealers, false);
		if (!out) {
			return ERR_UPDATEIDENTITY;
		}
	}
	return OK;
}

int32_t id_canspendfor(char *id, int32_t full_id, int32_t *err_no)
{
	int32_t retval = OK, id_canspendfor_value = false;
	char id_param[256] = { 0 };
	cJSON *params = NULL, *result = NULL, *obj = NULL;

	if (!is_id_exists(id, full_id)) {
		*err_no = ERR_ID_NOT_FOUND;
		return false;
	}

	strncpy(id_param, id, sizeof(id_param) - 1);
	if (0 == full_id) {
		snprintf(id_param + strlen(id_param), sizeof(id_param) - strlen(id_param), ".%s", bet_get_poker_id_fqn());
	}
	
	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, cJSON_CreateString(id_param));
	cJSON_AddItemToArray(params, cJSON_CreateNumber(-1));
	
	retval = chips_rpc("getidentity", params, &result);
	cJSON_Delete(params);

	if (retval != OK) {
		*err_no = retval;
		return false;
	}

	if (((obj = jobj(result, "canspendfor")) != NULL) && (is_cJSON_True(obj)))
		id_canspendfor_value = true;
	else
		*err_no = ERR_ID_AUTH;

	if (result) cJSON_Delete(result);
	return id_canspendfor_value;
}

int32_t id_cansignfor(char *id, int32_t full_id, int32_t *err_no)
{
	int32_t retval = OK, id_cansignfor_value = false;
	char id_param[256] = { 0 };
	cJSON *params = NULL, *result = NULL, *obj = NULL;

	if (!is_id_exists(id, full_id)) {
		*err_no = ERR_ID_NOT_FOUND;
		return false;
	}

	strncpy(id_param, id, sizeof(id_param) - 1);
	if (0 == full_id) {
		snprintf(id_param + strlen(id_param), sizeof(id_param) - strlen(id_param), ".%s", bet_get_poker_id_fqn());
	}
	
	params = cJSON_CreateArray();
	cJSON_AddItemToArray(params, cJSON_CreateString(id_param));
	cJSON_AddItemToArray(params, cJSON_CreateNumber(-1));
	
	retval = chips_rpc("getidentity", params, &result);
	cJSON_Delete(params);

	if (retval != OK) {
		*err_no = retval;
		return false;
	}

	if (((obj = jobj(result, "cansignfor")) != NULL) && (is_cJSON_True(obj)))
		id_cansignfor_value = true;
	else
		*err_no = ERR_ID_AUTH;

	if (result) cJSON_Delete(result);
	return id_cansignfor_value;
}

bool is_table_registered(char *table_id, char *dealer_id)
{
	cJSON *t_table_info = NULL;

	t_table_info = get_cJSON_from_id_key(dealer_id, T_TABLE_INFO_KEY, 0);
	if (t_table_info && (strcmp(table_id, jstr(t_table_info, "table_id")) == 0)) {
		return true;
	}
	return false;
}
