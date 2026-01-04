#define _GNU_SOURCE
// Prevent narrow math functions from being declared (for compatibility)
// This must be defined before any system headers that might include math.h
#define __NO_MATH_NARROW_FUNCTIONS
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <libgen.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "bet.h"
#include "config.h"
#include "misc.h"
#include "err.h"
#include "commands.h"
#include "dealer.h"

// Config file paths - will be initialized to absolute paths
static char dealer_config_ini_file_buf[PATH_MAX];
static char player_config_ini_file_buf[PATH_MAX];
static char cashier_config_ini_file_buf[PATH_MAX];
static char bets_config_ini_file_buf[PATH_MAX];
static char blockchain_config_ini_file_buf[PATH_MAX];
static char verus_dealer_config_buf[PATH_MAX];
static char verus_player_config_file_buf[PATH_MAX];
static char verus_ids_keys_config_buf[PATH_MAX];
static char rpc_credentials_config_buf[PATH_MAX];

char *dealer_config_ini_file = dealer_config_ini_file_buf;
char *rpc_credentials_file = rpc_credentials_config_buf;
char *player_config_ini_file = player_config_ini_file_buf;
char *cashier_config_ini_file = cashier_config_ini_file_buf;
char *bets_config_ini_file = bets_config_ini_file_buf;
char *blockchain_config_ini_file = blockchain_config_ini_file_buf;
char *verus_dealer_config = verus_dealer_config_buf;
char *verus_player_config_file = verus_player_config_file_buf;
char *verus_ids_keys_config_file = verus_ids_keys_config_buf;

/**
 * Initialize config file paths relative to the executable location
 * This allows the program to be run from any directory
 */
void bet_init_config_paths(void)
{
	char exe_path[PATH_MAX] = { 0 };
	char exe_dir[PATH_MAX] = { 0 };
	char exe_dir_copy[PATH_MAX] = { 0 };
	ssize_t len = 0;

	// Get the executable path
	len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len == -1) {
		// Fallback: use current directory
		strncpy(exe_dir, ".", sizeof(exe_dir) - 1);
	} else {
		exe_path[len] = '\0';
		// Get directory from executable path (dirname modifies the string, so use a copy)
		strncpy(exe_dir_copy, exe_path, sizeof(exe_dir_copy) - 1);
		char *dir = dirname(exe_dir_copy);
		if (dir != NULL) {
			strncpy(exe_dir, dir, sizeof(exe_dir) - 1);
		}
	}

	// If executable is in bin/, go up one level to poker/ then to config/
	// Otherwise assume we're in poker/ directory
	char config_base[PATH_MAX];
	if (strstr(exe_dir, "/bin") != NULL || strstr(exe_dir, "bin") != NULL) {
		// Executable is in bin/, config is in ../config/
		snprintf(config_base, sizeof(config_base), "%s/../config", exe_dir);
	} else {
		// Executable is in poker/, config is in ./config/
		snprintf(config_base, sizeof(config_base), "%s/config", exe_dir);
	}

	// Initialize all config paths
	snprintf(dealer_config_ini_file, sizeof(dealer_config_ini_file_buf), "%s/dealer_config.ini", config_base);
	snprintf(player_config_ini_file, sizeof(player_config_ini_file_buf), "%s/player_config.ini", config_base);
	snprintf(cashier_config_ini_file, sizeof(cashier_config_ini_file_buf), "%s/cashier_config.ini", config_base);
	snprintf(bets_config_ini_file, sizeof(bets_config_ini_file_buf), "%s/bets.ini", config_base);
	snprintf(blockchain_config_ini_file, sizeof(blockchain_config_ini_file_buf), "%s/blockchain_config.ini", config_base);
	snprintf(verus_dealer_config, sizeof(verus_dealer_config_buf), "%s/verus_dealer.ini", config_base);
	snprintf(verus_player_config_file, sizeof(verus_player_config_file_buf), "%s/verus_player.ini", config_base);
	snprintf(verus_ids_keys_config_file, sizeof(verus_ids_keys_config_buf), "%s/verus_ids_keys.ini", config_base);
	
	// RPC credentials file is in poker/ directory (not config/) since it contains secrets
	// Go up one level from config_base to get poker directory
	char poker_base[PATH_MAX];
	if (strstr(exe_dir, "/bin") != NULL || strstr(exe_dir, "bin") != NULL) {
		// Executable is in bin/, poker/ is ..
		snprintf(poker_base, sizeof(poker_base), "%s/..", exe_dir);
	} else {
		// Executable is in poker/
		snprintf(poker_base, sizeof(poker_base), "%s", exe_dir);
	}
	snprintf(rpc_credentials_file, sizeof(rpc_credentials_config_buf), "%s/.rpccredentials", poker_base);
}

struct verus_player_config player_config = { 0 };

bits256 game_id;

/* GUI WebSocket port - configurable, defaults to 9000 */
int gui_ws_port = DEFAULT_GUI_WS_PORT;

/* Globals declared in include/common.h */
int32_t is_table_private = 0;
char table_password[128] = { 0 };
char player_name[128] = { 0 };
char verus_pid[128] = { 0 };
// bet_ln_config removed - Lightning Network support removed, using CHIPS-only payments

cJSON *bet_read_json_file(char *file_name)
{
	FILE *fp = NULL;
	cJSON *json_data = NULL;
	char *data = NULL;
	char buf[256];
	size_t cap = 4096;
	size_t len = 0;

	data = calloc(cap, 1);
	if (!data)
		goto end;

	fp = fopen(file_name, "r");
	if (fp == NULL) {
		dlg_error("Failed to open file %s\n", file_name);
		goto end;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		size_t blen = strlen(buf);
		if (len + blen + 1 > cap) {
			size_t newcap = cap;
			while (len + blen + 1 > newcap)
				newcap *= 2;
			char *tmp = realloc(data, newcap);
			if (!tmp)
				goto end;
			data = tmp;
			/* Ensure newly-allocated tail is NUL (for safety) */
			memset(data + cap, 0x00, newcap - cap);
			cap = newcap;
		}
		memcpy(data + len, buf, blen);
		len += blen;
		data[len] = '\0';
	}

	json_data = cJSON_Parse(data);
end:
	if (fp)
		fclose(fp);
	if (data)
		free(data);
	return json_data;
}

void bet_parse_dealer_config_ini_file()
{
	dictionary *ini = NULL;

	ini = iniparser_load(dealer_config_ini_file);
	if (ini == NULL) {
		dlg_error("error in parsing %s", dealer_config_ini_file);
	} else {
		if (-1 != iniparser_getint(ini, "table:max_players", -1)) {
			max_players = iniparser_getint(ini, "table:max_players", -1);
		}
		if (0 != iniparser_getdouble(ini, "table:big_blind", 0)) {
			BB_in_chips = iniparser_getdouble(ini, "table:big_blind", 0);
			SB_in_chips = BB_in_chips / 2;
		}
		// min_stake and max_stake are now in CHIPS directly (not in BB multiples)
		if (0 != iniparser_getdouble(ini, "table:min_stake", 0)) {
			table_min_stake = iniparser_getdouble(ini, "table:min_stake", 0);
		}
		if (0 != iniparser_getdouble(ini, "table:max_stake", 0)) {
			table_max_stake = iniparser_getdouble(ini, "table:max_stake", 0);
		}

		if (0 != iniparser_getdouble(ini, "dealer:chips_tx_fee", 0)) {
			chips_tx_fee = iniparser_getdouble(ini, "dealer:chips_tx_fee", 0);
		}
		if (0 != iniparser_getdouble(ini, "dealer:dcv_commission", 0)) {
			dcv_commission_percentage = iniparser_getdouble(ini, "dealer:dcv_commission", 0);
		}
		if (NULL != iniparser_getstring(ini, "dealer:gui_host", NULL)) {
			/* Avoid overflowing fixed-size global buffer */
			snprintf(dcv_hosted_gui_url, sizeof(dcv_hosted_gui_url), "%s",
				 iniparser_getstring(ini, "dealer:gui_host", NULL));
		}
		// Read gui_ws_port from config, use default if not set or invalid
		int port = iniparser_getint(ini, "dealer:gui_ws_port", DEFAULT_DEALER_WS_PORT);
		if (port > 0 && port <= 65535) {
			gui_ws_port = port;
		} else {
			gui_ws_port = DEFAULT_DEALER_WS_PORT;
		}
		threshold_value = iniparser_getint(ini, "dealer:min_cashiers", threshold_value);
		if (-1 != iniparser_getboolean(ini, "private table:is_table_private", -1)) {
			is_table_private = iniparser_getboolean(ini, "private table:is_table_private", -1);
		}
		if (NULL != iniparser_getstring(ini, "private table:table_password", NULL)) {
			snprintf(table_password, sizeof(table_password), "%s",
				 iniparser_getstring(ini, "private table:table_password", NULL));
		}
		// bet_ln_config removed - Lightning Network support removed, using CHIPS-only payments
		iniparser_freedict(ini);
	}
}

void bet_parse_player_config_ini_file()
{
	dictionary *ini = NULL;

	ini = iniparser_load(player_config_ini_file);
	if (ini == NULL) {
		dlg_error("error in parsing %s", player_config_ini_file);
	} else {
		if (0 != iniparser_getdouble(ini, "player:max_allowed_dcv_commission", 0)) {
			max_allowed_dcv_commission = iniparser_getdouble(ini, "player:max_allowed_dcv_commission", 0);
		}
		if (0 != iniparser_getint(ini, "player:table_stake_size", 0)) {
			table_stack_in_bb = iniparser_getint(ini, "player:table_stake_size", 0);
		}
		if (0 != iniparser_getstring(ini, "player:name", NULL)) {
			snprintf(player_name, sizeof(player_name), "%s",
				 iniparser_getstring(ini, "player:name", NULL));
		}
		if (-1 != iniparser_getboolean(ini, "private table:is_table_private", -1)) {
			is_table_private = iniparser_getboolean(ini, "private table:is_table_private", -1);
		}
		if (NULL != iniparser_getstring(ini, "private table:table_password", NULL)) {
			snprintf(table_password, sizeof(table_password), "%s",
				 iniparser_getstring(ini, "private table:table_password", NULL));
		}
		// bet_ln_config removed - Lightning Network support removed, using CHIPS-only payments
		int port = iniparser_getint(ini, "player:gui_ws_port", DEFAULT_PLAYER_WS_PORT);
		if (port > 0 && port <= 65535) {
			gui_ws_port = port;
		} else {
			gui_ws_port = DEFAULT_PLAYER_WS_PORT;
		}
		iniparser_freedict(ini);
	}
}

void bet_parse_cashier_config_ini_file()
{
	cJSON *cashiers_info = NULL;
	dictionary *ini = NULL;

	ini = iniparser_load(cashier_config_ini_file);
	if (ini == NULL) {
		dlg_error("error in parsing %s", cashier_config_ini_file);
	} else {
		char str[64];
		int i = 1;
		snprintf(str, sizeof(str), "cashier:node-%d", i);
		cashiers_info = cJSON_CreateArray();
		while (NULL != iniparser_getstring(ini, str, NULL)) {
			cJSON_AddItemToArray(cashiers_info, cJSON_Parse(iniparser_getstring(ini, str, NULL)));
			memset(str, 0x00, sizeof(str));
			snprintf(str, sizeof(str), "cashier:node-%d", ++i);
		}
		no_of_notaries = cJSON_GetArraySize(cashiers_info);
		notary_node_ips = (char **)malloc(no_of_notaries * sizeof(char *));
		notary_node_pubkeys = (char **)malloc(no_of_notaries * sizeof(char *));
		notary_status = (int *)malloc(no_of_notaries * sizeof(int));

		for (int32_t i = 0; i < no_of_notaries; i++) {
			cJSON *node_info = cJSON_CreateObject();
			node_info = cJSON_GetArrayItem(cashiers_info, i);

			notary_node_ips[i] = (char *)malloc(strlen(jstr(node_info, "ip")) + 1);
			memset(notary_node_ips[i], 0x00, strlen(jstr(node_info, "ip")) + 1);

			notary_node_pubkeys[i] = (char *)malloc(strlen(jstr(node_info, "pubkey")) + 1);
			memset(notary_node_pubkeys[i], 0x00, strlen(jstr(node_info, "pubkey")) + 1);

			strncpy(notary_node_ips[i], jstr(node_info, "ip"), strlen(jstr(node_info, "ip")));
			strncpy(notary_node_pubkeys[i], jstr(node_info, "pubkey"), strlen(jstr(node_info, "pubkey")));
		}
		int port = iniparser_getint(ini, "cashier:gui_ws_port", DEFAULT_CASHIER_WS_PORT);
		if (port > 0 && port <= 65535) {
			gui_ws_port = port;
		} else {
			gui_ws_port = DEFAULT_CASHIER_WS_PORT;
		}
		iniparser_freedict(ini);
	}
}

void bet_display_cashier_hosted_gui()
{
	dictionary *ini = NULL;

	ini = iniparser_load(player_config_ini_file);
	if (ini == NULL) {
		dlg_error("error in parsing %s", player_config_ini_file);
	} else {
		char str[64];
		int i = 1;
		snprintf(str, sizeof(str), "gui:cashier-%d", i);
		while (NULL != iniparser_getstring(ini, str, NULL)) {
			if (check_url(iniparser_getstring(ini, str, NULL)))
				dlg_warn("%s", iniparser_getstring(ini, str, NULL));
			memset(str, 0x00, sizeof(str));
			snprintf(str, sizeof(str), "gui:cashier-%d", ++i);
		}
	}
}

static int32_t ini_sec_exists(dictionary *ini, char *sec_name)
{
	int32_t n, retval = -1;

	n = iniparser_getnsec(ini);
	dlg_info("number of sections:: %d", n);
	for (int32_t i = 0; i < n; i++) {
		dlg_info("%s::%s", iniparser_getsecname(ini, i), sec_name);
		if (strcmp(iniparser_getsecname(ini, i), sec_name) == 0) {
			retval = OK;
			break;
		}
	}
	return retval;
}

int32_t bet_parse_bets()
{
	int32_t retval = OK, bet_no = 0;
	dictionary *ini = NULL;
	char key_name[40];
	cJSON *bets_info = NULL, *info = NULL;

	ini = iniparser_load(bets_config_ini_file);
	if (ini == NULL) {
		retval = ERR_INI_PARSING;
		dlg_error("error in parsing %s", bets_config_ini_file);
		return retval;
	}
	info = cJSON_CreateObject();
	cJSON_AddStringToObject(info, "method", "bets");
	cJSON_AddNumberToObject(info, "balance", chips_get_balance());
	bets_info = cJSON_CreateArray();
	while (1) {
		cJSON *bet = cJSON_CreateObject();

		cJSON_AddNumberToObject(bet, "bet_id", bet_no);
		memset(key_name, 0x00, sizeof(key_name));
		sprintf(key_name, "bets:%d:desc", bet_no);
		if (NULL != iniparser_getstring(ini, key_name, NULL)) {
			cJSON_AddStringToObject(bet, "desc", iniparser_getstring(ini, key_name, NULL));
		} else {
			break;
		}
		memset(key_name, 0x00, sizeof(key_name));
		sprintf(key_name, "bets:%d:predictions", bet_no);
		if (NULL != iniparser_getstring(ini, key_name, NULL)) {
			cJSON_AddStringToObject(bet, "predictions", iniparser_getstring(ini, key_name, NULL));
		} else {
			break;
		}
		memset(key_name, 0x00, sizeof(key_name));
		sprintf(key_name, "bets:%d:range", bet_no);
		if (NULL != iniparser_getstring(ini, key_name, NULL)) {
			cJSON_AddStringToObject(bet, "range", iniparser_getstring(ini, key_name, NULL));
		} else {
			break;
		}
		cJSON_AddItemToArray(bets_info, bet);
		bet_no++;
	}
	cJSON_AddItemToObject(info, "bets_info", bets_info);
	dlg_info("\n%s", cJSON_Print(info));
	return retval;
}

bool bet_is_new_block_set()
{
	dictionary *ini = NULL;
	bool is_new_block_set = false;

	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;

	char *config_file = NULL;
	const char *suffix = "/bet/privatebet/config/blockchain_config.ini";
	size_t need = strlen(homedir) + strlen(suffix) + 1;
	config_file = calloc(1, need);
	if (!config_file) {
		return false;
	}
	snprintf(config_file, need, "%s%s", homedir, suffix);
	ini = iniparser_load(config_file);
	if (ini == NULL) {
		dlg_error("error in parsing %s", config_file);
	} else {
		if (-1 != iniparser_getboolean(ini, "blockchain:new_block", -1)) {
			is_new_block_set = iniparser_getboolean(ini, "blockchain:new_block", -1);
		}
	}
	if (config_file)
		free(config_file);
	return is_new_block_set;
}

void bet_parse_blockchain_config_ini_file()
{
	dictionary *ini = NULL;

	ini = iniparser_load(blockchain_config_ini_file);
	if (ini == NULL) {
		dlg_error("error in parsing %s", blockchain_config_ini_file);
	} else {
		if (NULL != iniparser_getstring(ini, "blockchain:blockchain_cli", NULL)) {
			memset(blockchain_cli, 0x00, sizeof(blockchain_cli));
			strncpy(blockchain_cli, iniparser_getstring(ini, "blockchain:blockchain_cli", "chips-cli"),
				sizeof(blockchain_cli));
			if (!((strcmp(blockchain_cli, chips_cli) == 0) ||
			      (strcmp(blockchain_cli, verus_chips_cli) == 0))) {
				dlg_warn(
					"The blockchain client configured in ./config/blockchain_config.ini is not in the supported list of clients, so setting it do default chips-cli");
				memset(blockchain_cli, 0x00, sizeof(blockchain_cli));
				strncpy(blockchain_cli, chips_cli, sizeof(blockchain_cli));
			}
		}
	}
}

int32_t bet_parse_verus_dealer()
{
	int32_t retval = OK;
	dictionary *ini = NULL;
	struct table t = { 0 };

	ini = iniparser_load(verus_dealer_config);
	if (!ini)
		return ERR_INI_PARSING;

	// Parse verus section
	if (NULL != iniparser_getstring(ini, "verus:dealer_id", NULL)) {
		strncpy(t.dealer_id, iniparser_getstring(ini, "verus:dealer_id", NULL), sizeof(t.dealer_id) - 1);
	}
	if (NULL != iniparser_getstring(ini, "verus:cashier_id", NULL)) {
		strncpy(t.cashier_id, iniparser_getstring(ini, "verus:cashier_id", NULL), sizeof(t.cashier_id) - 1);
	} else {
		// Default to main cashier ID if not specified
		strncpy(t.cashier_id, "cashier", sizeof(t.cashier_id) - 1);
	}

	// Parse table section
	if (-1 != iniparser_getint(ini, "table:max_players", -1)) {
		t.max_players = (uint8_t)iniparser_getint(ini, "table:max_players", -1);
	}
	if (0 != iniparser_getdouble(ini, "table:big_blind", 0)) {
		float_to_uint32_s(&t.big_blind, iniparser_getdouble(ini, "table:big_blind", 0));
	}
	// min_stake and max_stake are now in CHIPS directly
	if (0 != iniparser_getdouble(ini, "table:min_stake", 0)) {
		float_to_uint32_s(&t.min_stake, iniparser_getdouble(ini, "table:min_stake", 0));
	}
	if (0 != iniparser_getdouble(ini, "table:max_stake", 0)) {
		float_to_uint32_s(&t.max_stake, iniparser_getdouble(ini, "table:max_stake", 0));
	}
	if (NULL != iniparser_getstring(ini, "table:table_id", NULL)) {
		strncpy(t.table_id, iniparser_getstring(ini, "table:table_id", NULL), sizeof(t.table_id) - 1);
	}

	iniparser_freedict(ini);
	retval = dealer_init(t);
	return retval;
}

int32_t bet_parse_verus_dealer_with_reset(bool reset)
{
	int32_t retval = OK;
	dictionary *ini = NULL;
	struct table t = { 0 };

	ini = iniparser_load(verus_dealer_config);
	if (!ini)
		return ERR_INI_PARSING;

	// Parse verus section
	if (NULL != iniparser_getstring(ini, "verus:dealer_id", NULL)) {
		strncpy(t.dealer_id, iniparser_getstring(ini, "verus:dealer_id", NULL), sizeof(t.dealer_id) - 1);
	}
	if (NULL != iniparser_getstring(ini, "verus:cashier_id", NULL)) {
		strncpy(t.cashier_id, iniparser_getstring(ini, "verus:cashier_id", NULL), sizeof(t.cashier_id) - 1);
	} else {
		strncpy(t.cashier_id, "cashier", sizeof(t.cashier_id) - 1);
	}

	// Parse table section
	if (-1 != iniparser_getint(ini, "table:max_players", -1)) {
		t.max_players = (uint8_t)iniparser_getint(ini, "table:max_players", -1);
	}
	if (0 != iniparser_getdouble(ini, "table:big_blind", 0)) {
		float_to_uint32_s(&t.big_blind, iniparser_getdouble(ini, "table:big_blind", 0));
	}
	if (0 != iniparser_getdouble(ini, "table:min_stake", 0)) {
		float_to_uint32_s(&t.min_stake, iniparser_getdouble(ini, "table:min_stake", 0));
	}
	if (0 != iniparser_getdouble(ini, "table:max_stake", 0)) {
		float_to_uint32_s(&t.max_stake, iniparser_getdouble(ini, "table:max_stake", 0));
	}
	if (NULL != iniparser_getstring(ini, "table:table_id", NULL)) {
		strncpy(t.table_id, iniparser_getstring(ini, "table:table_id", NULL), sizeof(t.table_id) - 1);
	}

	iniparser_freedict(ini);
	
	if (reset) {
		retval = dealer_init_with_reset(t);
	} else {
		retval = dealer_init(t);
	}
	return retval;
}

int32_t bet_parse_verus_player()
{
	int32_t retval = OK;
	dictionary *ini = NULL;

	ini = iniparser_load(verus_player_config_file);
	if (!ini)
		return ERR_INI_PARSING;

	if (NULL != iniparser_getstring(ini, "verus:dealer_id", NULL)) {
		strncpy(player_config.dealer_id, iniparser_getstring(ini, "verus:dealer_id", NULL),
			sizeof(player_config.dealer_id));
	}
	if (NULL != iniparser_getstring(ini, "verus:table_id", NULL)) {
		strncpy(player_config.table_id, iniparser_getstring(ini, "verus:table_id", NULL),
			sizeof(player_config.table_id));
	}
	if (NULL != iniparser_getstring(ini, "verus:wallet_addr", NULL)) {
		strncpy(player_config.wallet_addr, iniparser_getstring(ini, "verus:wallet_addr", NULL),
			sizeof(player_config.wallet_addr));
	}
	if (NULL != iniparser_getstring(ini, "verus:player_id", NULL)) {
		strncpy(player_config.verus_pid, iniparser_getstring(ini, "verus:player_id", NULL),
			sizeof(player_config.verus_pid));
	}
	// Read WebSocket port for GUI mode (default: 9001)
	player_config.ws_port = iniparser_getint(ini, "verus:ws_port", DEFAULT_PLAYER_WS_PORT);
	
	//Check if all IDs are valid
	if ((player_config.dealer_id[0] == '\0') || (player_config.table_id[0] == '\0') || (player_config.verus_pid[0] == '\0') ||
	    !is_id_exists(player_config.dealer_id, 0) || !is_id_exists(player_config.table_id, 0)) {
		return ERR_CONFIG_PLAYER_ARGS;
	}
	//Check if the node has player IDs priv keys
	if (!id_cansignfor(player_config.verus_pid, 0, &retval)) {
		return retval;
	}

	return retval;
}

/* Verus IDs and Keys Configuration */
struct verus_ids_keys_config verus_config = { 0 };

void bet_parse_verus_ids_keys_config(void)
{
	dictionary *ini = NULL;
	const char *value = NULL;

	ini = iniparser_load(verus_ids_keys_config_file);
	if (ini == NULL) {
		dlg_warn("Could not load %s, using default values", verus_ids_keys_config_file);
		// Initialize with defaults (from #defines)
		strncpy(verus_config.parent_id, POKER_ID_FQN, sizeof(verus_config.parent_id) - 1);
		strncpy(verus_config.cashier_id, CASHIERS_ID_FQN, sizeof(verus_config.cashier_id) - 1);
		strncpy(verus_config.dealer_id, DEALERS_ID_FQN, sizeof(verus_config.dealer_id) - 1);
		strncpy(verus_config.cashiers_short, CASHIERS_ID, sizeof(verus_config.cashiers_short) - 1);
		strncpy(verus_config.dealers_short, DEALERS_ID, sizeof(verus_config.dealers_short) - 1);
		strncpy(verus_config.poker_short, POKER_ID, sizeof(verus_config.poker_short) - 1);
		strncpy(verus_config.key_prefix, "chips.vrsc::poker.sg777z.", sizeof(verus_config.key_prefix) - 1);
		strncpy(verus_config.cashiers_key, CASHIERS_KEY, sizeof(verus_config.cashiers_key) - 1);
		strncpy(verus_config.dealers_key, DEALERS_KEY, sizeof(verus_config.dealers_key) - 1);
		strncpy(verus_config.t_game_id_key, T_GAME_ID_KEY, sizeof(verus_config.t_game_id_key) - 1);
		strncpy(verus_config.t_table_info_key, T_TABLE_INFO_KEY, sizeof(verus_config.t_table_info_key) - 1);
		strncpy(verus_config.t_player_info_key, T_PLAYER_INFO_KEY, sizeof(verus_config.t_player_info_key) - 1);
		strncpy(verus_config.t_d_deck_key, T_D_DECK_KEY, sizeof(verus_config.t_d_deck_key) - 1);
		strncpy(verus_config.t_b_deck_key, T_B_DECK_KEY, sizeof(verus_config.t_b_deck_key) - 1);
		strncpy(verus_config.t_card_bv_key, T_CARD_BV_KEY, sizeof(verus_config.t_card_bv_key) - 1);
		strncpy(verus_config.t_game_info_key, T_GAME_INFO_KEY, sizeof(verus_config.t_game_info_key) - 1);
		strncpy(verus_config.player_deck_key, PLAYER_DECK_KEY, sizeof(verus_config.player_deck_key) - 1);
		strncpy(verus_config.t_d_p1_deck_key, T_D_P1_DECK_KEY, sizeof(verus_config.t_d_p1_deck_key) - 1);
		strncpy(verus_config.t_d_p2_deck_key, T_D_P2_DECK_KEY, sizeof(verus_config.t_d_p2_deck_key) - 1);
		strncpy(verus_config.t_d_p3_deck_key, T_D_P3_DECK_KEY, sizeof(verus_config.t_d_p3_deck_key) - 1);
		strncpy(verus_config.t_d_p4_deck_key, T_D_P4_DECK_KEY, sizeof(verus_config.t_d_p4_deck_key) - 1);
		strncpy(verus_config.t_d_p5_deck_key, T_D_P5_DECK_KEY, sizeof(verus_config.t_d_p5_deck_key) - 1);
		strncpy(verus_config.t_d_p6_deck_key, T_D_P6_DECK_KEY, sizeof(verus_config.t_d_p6_deck_key) - 1);
		strncpy(verus_config.t_d_p7_deck_key, T_D_P7_DECK_KEY, sizeof(verus_config.t_d_p7_deck_key) - 1);
		strncpy(verus_config.t_d_p8_deck_key, T_D_P8_DECK_KEY, sizeof(verus_config.t_d_p8_deck_key) - 1);
		strncpy(verus_config.t_d_p9_deck_key, T_D_P9_DECK_KEY, sizeof(verus_config.t_d_p9_deck_key) - 1);
		strncpy(verus_config.t_b_p1_deck_key, T_B_P1_DECK_KEY, sizeof(verus_config.t_b_p1_deck_key) - 1);
		strncpy(verus_config.t_b_p2_deck_key, T_B_P2_DECK_KEY, sizeof(verus_config.t_b_p2_deck_key) - 1);
		strncpy(verus_config.t_b_p3_deck_key, T_B_P3_DECK_KEY, sizeof(verus_config.t_b_p3_deck_key) - 1);
		strncpy(verus_config.t_b_p4_deck_key, T_B_P4_DECK_KEY, sizeof(verus_config.t_b_p4_deck_key) - 1);
		strncpy(verus_config.t_b_p5_deck_key, T_B_P5_DECK_KEY, sizeof(verus_config.t_b_p5_deck_key) - 1);
		strncpy(verus_config.t_b_p6_deck_key, T_B_P6_DECK_KEY, sizeof(verus_config.t_b_p6_deck_key) - 1);
		strncpy(verus_config.t_b_p7_deck_key, T_B_P7_DECK_KEY, sizeof(verus_config.t_b_p7_deck_key) - 1);
		strncpy(verus_config.t_b_p8_deck_key, T_B_P8_DECK_KEY, sizeof(verus_config.t_b_p8_deck_key) - 1);
		strncpy(verus_config.t_b_p9_deck_key, T_B_P9_DECK_KEY, sizeof(verus_config.t_b_p9_deck_key) - 1);
		verus_config.initialized = 1;
		return;
	}

	// Load from config file
	if ((value = iniparser_getstring(ini, "identities:parent_id", NULL)) != NULL) {
		strncpy(verus_config.parent_id, value, sizeof(verus_config.parent_id) - 1);
		verus_config.parent_id[sizeof(verus_config.parent_id) - 1] = '\0';
	} else {
		strncpy(verus_config.parent_id, POKER_ID_FQN, sizeof(verus_config.parent_id) - 1);
	}

	if ((value = iniparser_getstring(ini, "identities:cashier_id", NULL)) != NULL) {
		strncpy(verus_config.cashier_id, value, sizeof(verus_config.cashier_id) - 1);
		verus_config.cashier_id[sizeof(verus_config.cashier_id) - 1] = '\0';
	} else {
		strncpy(verus_config.cashier_id, CASHIERS_ID_FQN, sizeof(verus_config.cashier_id) - 1);
	}

	if ((value = iniparser_getstring(ini, "identities:dealer_id", NULL)) != NULL) {
		strncpy(verus_config.dealer_id, value, sizeof(verus_config.dealer_id) - 1);
		verus_config.dealer_id[sizeof(verus_config.dealer_id) - 1] = '\0';
	} else {
		strncpy(verus_config.dealer_id, DEALERS_ID_FQN, sizeof(verus_config.dealer_id) - 1);
	}

	if ((value = iniparser_getstring(ini, "identities:cashiers_short", NULL)) != NULL) {
		strncpy(verus_config.cashiers_short, value, sizeof(verus_config.cashiers_short) - 1);
	} else {
		strncpy(verus_config.cashiers_short, CASHIERS_ID, sizeof(verus_config.cashiers_short) - 1);
	}

	if ((value = iniparser_getstring(ini, "identities:dealers_short", NULL)) != NULL) {
		strncpy(verus_config.dealers_short, value, sizeof(verus_config.dealers_short) - 1);
	} else {
		strncpy(verus_config.dealers_short, DEALERS_ID, sizeof(verus_config.dealers_short) - 1);
	}

	if ((value = iniparser_getstring(ini, "identities:poker_short", NULL)) != NULL) {
		strncpy(verus_config.poker_short, value, sizeof(verus_config.poker_short) - 1);
	} else {
		strncpy(verus_config.poker_short, POKER_ID, sizeof(verus_config.poker_short) - 1);
	}

	// Load key prefix
	if ((value = iniparser_getstring(ini, "keys:key_prefix", NULL)) != NULL) {
		strncpy(verus_config.key_prefix, value, sizeof(verus_config.key_prefix) - 1);
		verus_config.key_prefix[sizeof(verus_config.key_prefix) - 1] = '\0';
	} else {
		strncpy(verus_config.key_prefix, "chips.vrsc::poker.sg777z.", sizeof(verus_config.key_prefix) - 1);
	}

	// Load individual keys (build full key from prefix + key name)
	char full_key[256];
	if ((value = iniparser_getstring(ini, "keys:cashiers_key", NULL)) != NULL) {
		snprintf(full_key, sizeof(full_key), "%s%s", verus_config.key_prefix, value);
		strncpy(verus_config.cashiers_key, full_key, sizeof(verus_config.cashiers_key) - 1);
	} else {
		strncpy(verus_config.cashiers_key, CASHIERS_KEY, sizeof(verus_config.cashiers_key) - 1);
	}

	if ((value = iniparser_getstring(ini, "keys:dealers_key", NULL)) != NULL) {
		snprintf(full_key, sizeof(full_key), "%s%s", verus_config.key_prefix, value);
		strncpy(verus_config.dealers_key, full_key, sizeof(verus_config.dealers_key) - 1);
	} else {
		strncpy(verus_config.dealers_key, DEALERS_KEY, sizeof(verus_config.dealers_key) - 1);
	}

	// Load other keys
	#define LOAD_KEY(cfg_field, ini_key, default_key) \
		if ((value = iniparser_getstring(ini, "keys:" ini_key, NULL)) != NULL) { \
			snprintf(full_key, sizeof(full_key), "%s%s", verus_config.key_prefix, value); \
			strncpy(verus_config.cfg_field, full_key, sizeof(verus_config.cfg_field) - 1); \
		} else { \
			strncpy(verus_config.cfg_field, default_key, sizeof(verus_config.cfg_field) - 1); \
		}

	LOAD_KEY(t_game_id_key, "t_game_id_key", T_GAME_ID_KEY);
	LOAD_KEY(t_table_info_key, "t_table_info_key", T_TABLE_INFO_KEY);
	LOAD_KEY(t_player_info_key, "t_player_info_key", T_PLAYER_INFO_KEY);
	LOAD_KEY(t_d_deck_key, "t_d_deck_key", T_D_DECK_KEY);
	LOAD_KEY(t_b_deck_key, "t_b_deck_key", T_B_DECK_KEY);
	LOAD_KEY(t_card_bv_key, "t_card_bv_key", T_CARD_BV_KEY);
	LOAD_KEY(t_game_info_key, "t_game_info_key", T_GAME_INFO_KEY);
	LOAD_KEY(player_deck_key, "player_deck_key", PLAYER_DECK_KEY);
	LOAD_KEY(t_d_p1_deck_key, "t_d_p1_deck_key", T_D_P1_DECK_KEY);
	LOAD_KEY(t_d_p2_deck_key, "t_d_p2_deck_key", T_D_P2_DECK_KEY);
	LOAD_KEY(t_d_p3_deck_key, "t_d_p3_deck_key", T_D_P3_DECK_KEY);
	LOAD_KEY(t_d_p4_deck_key, "t_d_p4_deck_key", T_D_P4_DECK_KEY);
	LOAD_KEY(t_d_p5_deck_key, "t_d_p5_deck_key", T_D_P5_DECK_KEY);
	LOAD_KEY(t_d_p6_deck_key, "t_d_p6_deck_key", T_D_P6_DECK_KEY);
	LOAD_KEY(t_d_p7_deck_key, "t_d_p7_deck_key", T_D_P7_DECK_KEY);
	LOAD_KEY(t_d_p8_deck_key, "t_d_p8_deck_key", T_D_P8_DECK_KEY);
	LOAD_KEY(t_d_p9_deck_key, "t_d_p9_deck_key", T_D_P9_DECK_KEY);
	LOAD_KEY(t_b_p1_deck_key, "t_b_p1_deck_key", T_B_P1_DECK_KEY);
	LOAD_KEY(t_b_p2_deck_key, "t_b_p2_deck_key", T_B_P2_DECK_KEY);
	LOAD_KEY(t_b_p3_deck_key, "t_b_p3_deck_key", T_B_P3_DECK_KEY);
	LOAD_KEY(t_b_p4_deck_key, "t_b_p4_deck_key", T_B_P4_DECK_KEY);
	LOAD_KEY(t_b_p5_deck_key, "t_b_p5_deck_key", T_B_P5_DECK_KEY);
	LOAD_KEY(t_b_p6_deck_key, "t_b_p6_deck_key", T_B_P6_DECK_KEY);
	LOAD_KEY(t_b_p7_deck_key, "t_b_p7_deck_key", T_B_P7_DECK_KEY);
	LOAD_KEY(t_b_p8_deck_key, "t_b_p8_deck_key", T_B_P8_DECK_KEY);
	LOAD_KEY(t_b_p9_deck_key, "t_b_p9_deck_key", T_B_P9_DECK_KEY);

	#undef LOAD_KEY

	verus_config.initialized = 1;
	iniparser_freedict(ini);
	dlg_info("Loaded Verus IDs and Keys configuration from %s", verus_ids_keys_config_file);
}

const char *bet_get_cashiers_id_fqn(void)
{
	if (verus_config.initialized) {
		return verus_config.cashier_id;
	}
	return CASHIERS_ID_FQN;
}

const char *bet_get_cashier_short_name(void)
{
	// Return just the short name (e.g., "cashier") for use with update_cmm
	// which adds the parent automatically
	if (verus_config.initialized && verus_config.cashier_id[0] != '\0') {
		// Extract short name from FQN (e.g., "cashier.sg777z.chips.vrsc@" -> "cashier")
		static char short_name[64];
		strncpy(short_name, verus_config.cashier_id, sizeof(short_name) - 1);
		short_name[sizeof(short_name) - 1] = '\0';
		char *dot = strchr(short_name, '.');
		if (dot) {
			*dot = '\0';  // Truncate at first dot
		}
		return short_name;
	}
	return "cashier";
}

const char *bet_get_dealers_id_fqn(void)
{
	if (verus_config.initialized) {
		return verus_config.dealer_id;
	}
	return DEALERS_ID_FQN;
}

const char *bet_get_poker_id_fqn(void)
{
	if (verus_config.initialized) {
		return verus_config.parent_id;
	}
	return POKER_ID_FQN;
}

const char *bet_get_key_prefix(void)
{
	if (verus_config.initialized) {
		return verus_config.key_prefix;
	}
	return "chips.vrsc::poker.sg777z.";
}

const char *bet_get_full_key_name(const char *key_name)
{
	static char full_key[256];
	if (!key_name) {
		return NULL;
	}
	snprintf(full_key, sizeof(full_key), "%s%s", bet_get_key_prefix(), key_name);
	return full_key;
}

/* RPC Credentials Configuration */
struct rpc_credentials rpc_config = { 0 };

void bet_parse_rpc_credentials(void)
{
	dictionary *ini = NULL;
	const char *value = NULL;

	// Default values
	strncpy(rpc_config.url, "http://127.0.0.1:22778", sizeof(rpc_config.url) - 1);
	strncpy(rpc_config.user, "", sizeof(rpc_config.user) - 1);
	strncpy(rpc_config.password, "", sizeof(rpc_config.password) - 1);
	rpc_config.use_rest_api = 0;  // Default to CLI mode for backwards compatibility
	rpc_config.initialized = 0;

	ini = iniparser_load(rpc_credentials_file);
	if (ini == NULL) {
		dlg_warn("Could not load %s, RPC credentials not configured", rpc_credentials_file);
		dlg_info("To use REST API, create %s with [rpc] section containing url, user, password", rpc_credentials_file);
		return;
	}

	// Load RPC URL
	if ((value = iniparser_getstring(ini, "rpc:url", NULL)) != NULL) {
		strncpy(rpc_config.url, value, sizeof(rpc_config.url) - 1);
		rpc_config.url[sizeof(rpc_config.url) - 1] = '\0';
	}

	// Load RPC user
	if ((value = iniparser_getstring(ini, "rpc:user", NULL)) != NULL) {
		strncpy(rpc_config.user, value, sizeof(rpc_config.user) - 1);
		rpc_config.user[sizeof(rpc_config.user) - 1] = '\0';
	}

	// Load RPC password
	if ((value = iniparser_getstring(ini, "rpc:password", NULL)) != NULL) {
		strncpy(rpc_config.password, value, sizeof(rpc_config.password) - 1);
		rpc_config.password[sizeof(rpc_config.password) - 1] = '\0';
	}

	// Load use_rest_api flag (default to 1 if credentials file exists)
	rpc_config.use_rest_api = iniparser_getboolean(ini, "rpc:use_rest_api", 1);

	rpc_config.initialized = 1;
	iniparser_freedict(ini);
	
	dlg_info("Loaded RPC credentials from %s", rpc_credentials_file);
	dlg_info("RPC URL: %s, User: %s, REST API: %s", 
		rpc_config.url, rpc_config.user, 
		rpc_config.use_rest_api ? "enabled" : "disabled");
}

const char *bet_get_rpc_url(void)
{
	if (rpc_config.initialized) {
		return rpc_config.url;
	}
	return "http://127.0.0.1:22778";
}

const char *bet_get_rpc_user(void)
{
	if (rpc_config.initialized) {
		return rpc_config.user;
	}
	return "";
}

const char *bet_get_rpc_password(void)
{
	if (rpc_config.initialized) {
		return rpc_config.password;
	}
	return "";
}

int bet_use_rest_api(void)
{
	return rpc_config.initialized && rpc_config.use_rest_api;
}
