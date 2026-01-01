#ifndef CONFIG_H
#define CONFIG_H

#include "bet.h"

/* Verus IDs and Keys Configuration Structure */
struct verus_ids_keys_config {
	char parent_id[128];
	char cashier_id[128];
	char dealer_id[128];
	char cashiers_short[64];
	char dealers_short[64];
	char poker_short[64];
	char key_prefix[128];
	char cashiers_key[128];
	char dealers_key[128];
	char t_game_id_key[128];
	char t_table_info_key[128];
	char t_player_info_key[128];
	char t_d_deck_key[128];
	char t_b_deck_key[128];
	char t_card_bv_key[128];
	char t_game_info_key[128];
	char player_deck_key[128];
	char t_d_p1_deck_key[128];
	char t_d_p2_deck_key[128];
	char t_d_p3_deck_key[128];
	char t_d_p4_deck_key[128];
	char t_d_p5_deck_key[128];
	char t_d_p6_deck_key[128];
	char t_d_p7_deck_key[128];
	char t_d_p8_deck_key[128];
	char t_d_p9_deck_key[128];
	char t_b_p1_deck_key[128];
	char t_b_p2_deck_key[128];
	char t_b_p3_deck_key[128];
	char t_b_p4_deck_key[128];
	char t_b_p5_deck_key[128];
	char t_b_p6_deck_key[128];
	char t_b_p7_deck_key[128];
	char t_b_p8_deck_key[128];
	char t_b_p9_deck_key[128];
	int32_t initialized;
};

extern struct verus_ids_keys_config verus_config;

/* RPC Credentials Configuration Structure */
struct rpc_credentials {
	char url[256];
	char user[128];
	char password[256];
	int use_rest_api;  /* 1 = use REST API, 0 = use CLI */
	int initialized;
};

extern struct rpc_credentials rpc_config;

/* RPC Credentials Functions */
void bet_parse_rpc_credentials(void);
const char *bet_get_rpc_url(void);
const char *bet_get_rpc_user(void);
const char *bet_get_rpc_password(void);
int bet_use_rest_api(void);

void bet_init_config_paths(void);
cJSON *bet_read_json_file(char *file_name);

/* Config file paths - declared in config.c */
extern char *dealer_config_ini_file;
extern char *player_config_ini_file;
extern char *cashier_config_ini_file;
extern char *verus_player_config_file;

void bet_parse_dealer_config_ini_file();
void bet_parse_player_config_ini_file();
void bet_parse_cashier_config_ini_file();
void bet_display_cashier_hosted_gui();
int32_t bet_parse_bets();
void bet_parse_blockchain_config_ini_file();
bool bet_is_new_block_set();
int32_t bet_parse_verus_dealer();
int32_t bet_parse_verus_player();
void bet_parse_verus_ids_keys_config(void);
const char *bet_get_cashiers_id_fqn(void);
const char *bet_get_dealers_id_fqn(void);
const char *bet_get_poker_id_fqn(void);
const char *bet_get_key_prefix(void);
const char *bet_get_full_key_name(const char *key_name);
#endif /* CONFIG_H */
