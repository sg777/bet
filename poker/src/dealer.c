#include "bet.h"
#include "dealer.h"
#include "deck.h"
#include "cards.h"
#include "game.h"
#include "err.h"
#include "misc.h"
#include "commands.h"
#include "dealer_registration.h"
#include "config.h"
#include "poker_vdxf.h"

struct d_deck_info_struct d_deck_info;
struct game_meta_info_struct game_meta_info;

char all_t_d_p_keys[all_t_d_p_keys_no][128] = { T_D_DECK_KEY,    T_D_P1_DECK_KEY, T_D_P2_DECK_KEY, T_D_P3_DECK_KEY,
						T_D_P4_DECK_KEY, T_D_P5_DECK_KEY, T_D_P6_DECK_KEY, T_D_P7_DECK_KEY,
						T_D_P8_DECK_KEY, T_D_P9_DECK_KEY };

char all_t_d_p_key_names[all_t_d_p_keys_no][128] = { "t_d_deck",    "t_d_p1_deck", "t_d_p2_deck", "t_d_p3_deck",
						     "t_d_p4_deck", "t_d_p5_deck", "t_d_p6_deck", "t_d_p7_deck",
						     "t_d_p8_deck", "t_d_p9_deck" };

char all_game_keys[all_game_keys_no][128] = { T_GAME_INFO_KEY };

char all_game_key_names[all_game_keys_no][128] = { "t_game_info" };

int32_t num_of_players;
char player_ids[CARDS_MAXPLAYERS][MAX_ID_LEN];

int32_t add_dealer(char *dealer_id)
{
	int32_t retval = OK;
	cJSON *dealers_info = NULL, *dealers = NULL, *out = NULL;

	if (!dealer_id) {
		return ERR_NULL_ID;
	}
	if (!is_id_exists(dealer_id, 0)) {
		return ERR_ID_NOT_FOUND;
	}

	if (!id_cansignfor(DEALERS_ID, 0, &retval)) {
		return retval;
	}

	dealers_info = cJSON_CreateObject();
	dealers = poker_list_dealers();
	if (!dealers) {
		dealers = cJSON_CreateArray();
	}
	jaddistr(dealers, dealer_id);
	cJSON_AddItemToObject(dealers_info, "dealers", dealers);
		out = poker_update_key_json(verus_config.initialized ? verus_config.dealers_short : DEALERS_ID, verus_config.initialized ? verus_config.dealers_key : DEALERS_KEY, dealers_info, false);

	if (!out) {
		return ERR_UPDATEIDENTITY;
	}
	dlg_info("%s", cJSON_Print(out));

	return retval;
}

int32_t dealer_sb_deck(char *id, bits256 *player_r, int32_t player_id)
{
	int32_t retval = OK;
	char str[65], *game_id_str = NULL;
	cJSON *d_blinded_deck = NULL;

	game_id_str = poker_get_key_str(id, T_GAME_ID_KEY);

	dlg_info("Player::%d deck...", player_id);
	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		dlg_info("%s", bits256_str(str, player_r[i]));
	}
	shuffle_deck_db(player_r, CARDS_MAXCARDS, d_deck_info.d_permi);
	blind_deck_d(player_r, CARDS_MAXCARDS, d_deck_info.dealer_r);

	dlg_info("Player::%d deck blinded with dealer secret key...", player_id);
	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		dlg_info("%s", bits256_str(str, player_r[i]));
	}

	d_blinded_deck = cJSON_CreateArray();
	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		jaddistr(d_blinded_deck, bits256_str(str, player_r[i]));
	}
	dlg_info("Updating Player ::%d blinded deck at the key ::%s by dealer..", player_id, all_t_d_p_keys[player_id]);
	cJSON *out = poker_append_key_json(id, get_key_data_vdxf_id(all_t_d_p_keys[player_id], game_id_str),
						       d_blinded_deck, true);

	if (!out)
		retval = ERR_DECK_BLINDING_DEALER;
	dlg_info("%s", cJSON_Print(out));

	return retval;
}

void dealer_init_deck()
{
	bet_permutation(d_deck_info.d_permi, CARDS_MAXCARDS);
	gen_deck(d_deck_info.dealer_r, CARDS_MAXCARDS);
}

int32_t dealer_table_init(struct table *t)
{
	int32_t game_state = G_ZEROIZED_STATE, retval = OK;
	char hexstr[65];
	cJSON *out = NULL, *t_game_info = NULL;
	char *game_id_vdxfid = NULL;

	if (!is_id_exists(t->table_id, 0))
		return ERR_ID_NOT_FOUND;

	game_state = get_game_state(t->table_id);

	switch (game_state) {
	case G_ZEROIZED_STATE:
		// Generate new game ID
		game_id = rand256(0);
		bits256_str(hexstr, game_id);
		dlg_info("Generated new game_id: %s", hexstr);
		
		// Store game_id
		dlg_info("Updating %s key...", T_GAME_ID_KEY);
		out = poker_append_key_hex(t->table_id, T_GAME_ID_KEY, hexstr, false);
		if (!out)
			return ERR_TABLE_LAUNCH;
		dlg_info("%s", cJSON_Print(out));

		// Get VDXF key for game info using the in-memory game_id
		game_id_vdxfid = get_key_data_vdxf_id(T_GAME_INFO_KEY, hexstr);
		if (!game_id_vdxfid) {
			dlg_error("Failed to get game_info vdxfid");
			return ERR_TABLE_LAUNCH;
		}
		
		// Set game state to TABLE_ACTIVE using the correct key
		dlg_info("Updating Game state to %s with key %s...", game_state_str(G_TABLE_ACTIVE), game_id_vdxfid);
		t_game_info = cJSON_CreateObject();
		cJSON_AddNumberToObject(t_game_info, "game_state", G_TABLE_ACTIVE);
		out = poker_append_key_json(t->table_id, game_id_vdxfid, t_game_info, true);
		if (!out)
			return ERR_GAME_STATE_UPDATE;
		dlg_info("%s", cJSON_Print(out));
		// No break is intentional
	case G_TABLE_ACTIVE:
		// Get game_id from chain if we're resuming
		if (game_state == G_TABLE_ACTIVE) {
			char *game_id_str = poker_get_key_str(t->table_id, T_GAME_ID_KEY);
			if (!game_id_str) {
				dlg_error("Failed to get game_id from chain");
				return ERR_GAME_ID_NOT_FOUND;
			}
			strncpy(hexstr, game_id_str, sizeof(hexstr) - 1);
			game_id_vdxfid = get_key_data_vdxf_id(T_GAME_INFO_KEY, hexstr);
		}
		
		// Set start_block to current block height for join request polling
		t->start_block = chips_get_block_count();
		dlg_info("Table start_block set to: %d", t->start_block);
		
		dlg_info("Updating %s key...", T_TABLE_INFO_KEY);
		out = poker_append_key_json(
			t->table_id, get_key_data_vdxf_id(T_TABLE_INFO_KEY, hexstr),
			struct_table_to_cJSON(t), true);
		if (!out)
			return ERR_TABLE_LAUNCH;
		dlg_info("%s", cJSON_Print(out));

		// Set game state to TABLE_STARTED using the correct key
		dlg_info("Updating Game state to %s...", game_state_str(G_TABLE_STARTED));
		t_game_info = cJSON_CreateObject();
		cJSON_AddNumberToObject(t_game_info, "game_state", G_TABLE_STARTED);
		out = poker_append_key_json(t->table_id, game_id_vdxfid, t_game_info, true);
		if (!out)
			return ERR_GAME_STATE_UPDATE;
		dlg_info("%s", cJSON_Print(out));
		break;
	default:
		// Table is already started - load start_block and player_ids from chain
		{
			char *game_id_str = poker_get_key_str(t->table_id, T_GAME_ID_KEY);
			if (game_id_str) {
				cJSON *t_table_info = get_cJSON_from_id_key_vdxfid(t->table_id, 
					get_key_data_vdxf_id(T_TABLE_INFO_KEY, game_id_str));
				if (t_table_info) {
					t->start_block = jint(t_table_info, "start_block");
					dlg_info("Loaded start_block from chain: %d", t->start_block);
				}
				
				// Load player_ids from t_player_info
				cJSON *t_player_info = get_cJSON_from_id_key_vdxfid(t->table_id,
					get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
				if (t_player_info) {
					num_of_players = jint(t_player_info, "num_players");
					cJSON *player_info_arr = cJSON_GetObjectItem(t_player_info, "player_info");
					if (player_info_arr && player_info_arr->type == cJSON_Array) {
						for (int32_t i = 0; i < num_of_players && i < CARDS_MAXPLAYERS; i++) {
							cJSON *item = cJSON_GetArrayItem(player_info_arr, i);
							if (item && item->valuestring) {
								// Parse the player_id from format: "p1_<public_key>_<slot>"
								char *player_info_str = item->valuestring;
								// Extract player_id (part before the first underscore)
								char *underscore = strchr(player_info_str, '_');
								if (underscore) {
									size_t id_len = underscore - player_info_str;
									if (id_len < MAX_ID_LEN) {
										strncpy(player_ids[i], player_info_str, id_len);
										player_ids[i][id_len] = '\0';
										dlg_info("Loaded player %d ID: %s", i, player_ids[i]);
									}
								}
							}
						}
					}
				}
			}
		}
		dlg_info("Table is in game, at state ::%s", game_state_str(game_state));
	}
	return retval;
}

bool is_players_shuffled_deck(char *table_id)
{
	int32_t game_state, num_players = 0, count = 0;
	;
	char *game_id_str = NULL;
	cJSON *t_player_info = NULL;

	game_state = get_game_state(table_id);

	if (game_state == G_DECK_SHUFFLING_P) {
		return true;
	} else if (game_state == G_PLAYERS_JOINED) {
		game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
		t_player_info =
			get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str));
		num_players = jint(t_player_info, "num_players");
		for (int32_t i = 0; i < num_players; i++) {
			if (G_DECK_SHUFFLING_P == get_game_state(player_ids[i]))
				count++;
		}
		if (count == num_players)
			return true;
	}
	return false;
}

int32_t dealer_shuffle_deck(char *id)
{
	int32_t retval = OK;
	char *game_id_str = NULL, str[65];
	cJSON *t_d_deck_info = NULL;
	bits256 t_p_r[CARDS_MAXCARDS];

	dealer_init_deck();
	game_id_str = poker_get_key_str(id, T_GAME_ID_KEY);

	for (int32_t i = 0; i < num_of_players; i++) {
		cJSON *player_deck =
			get_cJSON_from_id_key_vdxfid(player_ids[i], get_key_data_vdxf_id(PLAYER_DECK_KEY, game_id_str));
		cJSON *cardinfo = cJSON_GetObjectItem(player_deck, "cardinfo");
		for (int32_t j = 0; j < cJSON_GetArraySize(cardinfo); j++) {
			t_p_r[j] = jbits256i(cardinfo, j);
		}
		retval = dealer_sb_deck(id, t_p_r, (i + 1));
		if (retval)
			return retval;
	}

	t_d_deck_info = cJSON_CreateArray();
	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		jaddistr(t_d_deck_info, bits256_str(str, d_deck_info.dealer_r[i].prod));
	}
	dlg_info("Updating the key :: %s, which contains public points of dealer blinded values..", T_D_DECK_KEY);
	cJSON *out = poker_append_key_json(id, get_key_data_vdxf_id(T_D_DECK_KEY, game_id_str),
						       t_d_deck_info, true);
	if (!out)
		retval = ERR_DECK_BLINDING_DEALER;
	dlg_info("%s", cJSON_Print(out));

	return retval;
}

int32_t handle_game_state(struct table *t)
{
	int32_t game_state, retval = OK;

	if (!t) {
		return ERR_ARGS_NULL;
	}

	game_state = get_game_state(t->table_id);
	dlg_info("%s", game_state_str(game_state));
	switch (game_state) {
	case G_TABLE_STARTED:
		// Poll cashier for pending join requests (from start_block onwards)
		// If start_block is 0 (old data), poll from block 1
		if (t->cashier_id[0] != '\0') {
			int32_t start = (t->start_block > 0) ? t->start_block : 1;
			int32_t joins = poker_poll_cashier_for_joins(t->cashier_id, t->table_id, 
			                                              t->dealer_id, start);
			if (joins > 0) {
				dlg_info("Processed %d join requests from cashier", joins);
			}
		}
		// Check if enough players have joined
		if (poker_is_table_full(t->table_id)) {
			append_game_state(t->table_id, G_PLAYERS_JOINED, NULL);
		}
		break;
	case G_PLAYERS_JOINED:
		if (is_players_shuffled_deck(t->table_id))
			append_game_state(t->table_id, G_DECK_SHUFFLING_P, NULL);
		break;
	case G_DECK_SHUFFLING_P:
		retval = dealer_shuffle_deck(t->table_id);
		if (!retval)
			append_game_state(t->table_id, G_DECK_SHUFFLING_D, NULL);
		break;
	case G_DECK_SHUFFLING_B:
		dlg_info("Its time for game");
		retval = init_game_state(t->table_id);
		break;
	case G_REVEAL_CARD:
		if (is_card_drawn(t->table_id) == OK) {
			dlg_info("Card is drawn");
			retval = verus_receive_card(t->table_id, dcv_vars);
		}
		break;
	}
	return retval;
}

int32_t register_table(struct table t)
{
	int32_t retval = OK;
	cJSON *d_table_info = NULL, *out = NULL;

	d_table_info = poker_get_key_json(t.dealer_id, T_TABLE_INFO_KEY, 0);
	if (d_table_info == NULL) {
		out = poker_update_key_json(t.dealer_id, get_vdxf_id(T_TABLE_INFO_KEY),
							struct_table_to_cJSON(&t), true);
		if (!out)
			retval = ERR_RESERVED;
	}
	return retval;
}

int32_t dealer_init(struct table t)
{
	int32_t retval = OK;
	double balance = 0;

	balance = chips_get_balance();
	if (balance < RESERVE_AMOUNT) {
		dlg_info("Wallet balance ::%f, Minimum funds needed to host a table :: %f", balance, RESERVE_AMOUNT);
		return ERR_CHIPS_INSUFFICIENT_FUNDS;
	}
	if ((!id_cansignfor(t.dealer_id, 0, &retval)) || (!id_cansignfor(t.table_id, 0, &retval))) {
		return retval;
	}

	if (!is_dealer_registered(t.dealer_id)) {
		// TODO:: An automated mechanism to register the dealer with dealer.sg777z.chips.vrsc@ need to be worked out
		return ERR_DEALER_UNREGISTERED;
	}

	if (is_table_registered(t.table_id, t.dealer_id)) {
		dlg_info("Table::%s is already registered with the dealer ::%s", t.table_id, t.dealer_id);
	} else {
		// TODO:: At the moment only one table we are registering with the dealer, if any other table exists it will be replaced with new table info
		retval = register_table(t);
		if (retval) {
			dlg_error("Table::%s, registration at dealer::%s is failed", t.table_id, t.dealer_id);
			return retval;
		}
	}

	retval = dealer_table_init(&t);
	if (retval != OK) {
		dlg_info("Table Init is failed");
		return retval;
	}

	dlg_info("Dealer ready. Table: %s, Dealer: %s, Cashier: %s", t.table_id, t.dealer_id, t.cashier_id);
	dlg_info("Waiting for players to join via cashier...");
	
	while (1) {
		retval = handle_game_state(&t);
		if (retval)
			return retval;
		sleep(2);
	}
	return retval;
}
