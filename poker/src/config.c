#define _GNU_SOURCE
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

char *dealer_config_ini_file = dealer_config_ini_file_buf;
char *player_config_ini_file = player_config_ini_file_buf;
char *cashier_config_ini_file = cashier_config_ini_file_buf;
char *bets_config_ini_file = bets_config_ini_file_buf;
char *blockchain_config_ini_file = blockchain_config_ini_file_buf;
char *verus_dealer_config = verus_dealer_config_buf;
char *verus_player_config_file = verus_player_config_file_buf;

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
}

struct verus_player_config player_config = { 0 };

bits256 game_id;

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
		if (0 != iniparser_getint(ini, "table:min_stake", 0)) {
			table_min_stake = iniparser_getint(ini, "table:min_stake", 0) * BB_in_chips;
		}
		if (0 != iniparser_getint(ini, "table:max_stake", 0)) {
			table_max_stake = iniparser_getint(ini, "table:max_stake", 0) * BB_in_chips;
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
		threshold_value = iniparser_getint(ini, "dealer:min_cashiers", threshold_value);
		if (-1 != iniparser_getboolean(ini, "private table:is_table_private", -1)) {
			is_table_private = iniparser_getboolean(ini, "private table:is_table_private", -1);
		}
		if (NULL != iniparser_getstring(ini, "private table:table_password", NULL)) {
			snprintf(table_password, sizeof(table_password), "%s",
				 iniparser_getstring(ini, "private table:table_password", NULL));
		}
		// bet_ln_config removed - Lightning Network support removed, using CHIPS-only payments
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
	struct table t;

	ini = iniparser_load(verus_dealer_config);
	if (!ini)
		return ERR_INI_PARSING;

	if (NULL != iniparser_getstring(ini, "verus:dealer_id", NULL)) {
		strncpy(t.dealer_id, iniparser_getstring(ini, "verus:dealer_id", NULL), sizeof(t.dealer_id));
	}
	if (-1 != iniparser_getint(ini, "table:max_players", -1)) {
		t.max_players = (uint8_t)iniparser_getint(ini, "table:max_players", -1);
	}
	if (0 != iniparser_getdouble(ini, "table:big_blind", 0)) {
		float_to_uint32_s(&t.big_blind, iniparser_getdouble(ini, "table:big_blind", 0));
	}
	if (0 != iniparser_getint(ini, "table:min_stake", 0)) {
		float_to_uint32_s(&t.min_stake, (iniparser_getint(ini, "table:min_stake", 0) * BB_in_chips));
	}
	if (0 != iniparser_getint(ini, "table:max_stake", 0)) {
		float_to_uint32_s(&t.max_stake, (iniparser_getint(ini, "table:max_stake", 0) * BB_in_chips));
	}
	if (NULL != iniparser_getstring(ini, "table:table_id", NULL)) {
		strncpy(t.table_id, iniparser_getstring(ini, "table:table_id", NULL), sizeof(t.table_id));
	}

	retval = dealer_init(t);
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
	//Check if all IDs are valid
	if ((!player_config.dealer_id) || (!player_config.table_id) || (!player_config.verus_pid) ||
	    !is_id_exists(player_config.dealer_id, 0) || !is_id_exists(player_config.table_id, 0)) {
		return ERR_CONFIG_PLAYER_ARGS;
	}
	//Check if the node has player IDs priv keys
	if (!id_cansignfor(player_config.verus_pid, 0, &retval)) {
		return retval;
	}

	return retval;
}
