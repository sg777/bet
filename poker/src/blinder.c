#include "bet.h"
#include "blinder.h"
#include "config.h"
#include "deck.h"
#include "cards.h"
#include "err.h"
#include "game.h"
#include "poker_vdxf.h"
#include <stdbool.h>

extern int32_t g_start_block;

// Forward declaration - defined later in file
static int32_t ensure_cashier_game_id_initialized(char *table_id);

char all_t_b_p_keys[all_t_b_p_keys_no][128] = { T_B_P1_DECK_KEY, T_B_P2_DECK_KEY, T_B_P3_DECK_KEY, T_B_P4_DECK_KEY,
						T_B_P5_DECK_KEY, T_B_P6_DECK_KEY, T_B_P7_DECK_KEY, T_B_P8_DECK_KEY,
						T_B_P9_DECK_KEY, T_B_DECK_KEY };

char all_t_b_p_key_names[all_t_b_p_keys_no][128] = { "t_b_p1_deck", "t_b_p2_deck", "t_b_p3_deck", "t_b_p4_deck",
						     "t_b_p5_deck", "t_b_p6_deck", "t_b_p7_deck", "t_b_p8_deck",
						     "t_b_p9_deck", "t_b_deck" };

struct b_deck_info_struct b_deck_info;

int32_t cashier_sb_deck(char *id, bits256 *d_blinded_deck, int32_t player_id)
{
	int32_t retval = OK;
	char str[65], *game_id_str = NULL;
	cJSON *b_blinded_deck = NULL;

	game_id_str = poker_get_key_str(id, T_GAME_ID_KEY);
	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		dlg_info("%s", bits256_str(str, d_blinded_deck[i]));
	}
	shuffle_deck_db(d_blinded_deck, CARDS_MAXCARDS, b_deck_info.b_permi);
	blind_deck_b(d_blinded_deck, CARDS_MAXCARDS, b_deck_info.cashier_r[player_id]);

	b_blinded_deck = cJSON_CreateArray();
	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		dlg_info("%d::%s", i, bits256_str(str, d_blinded_deck[i]));
		jaddibits256(b_blinded_deck, d_blinded_deck[i]);
	}
	char *b_blinded_deck_str = cJSON_PrintUnformatted(b_blinded_deck);
	if (b_blinded_deck_str) {
		dlg_info("b_blinded_deck::%s", b_blinded_deck_str);
		free(b_blinded_deck_str);
	}
	cJSON *out = poker_append_key_json(id, get_key_data_vdxf_id(all_t_b_p_keys[player_id], game_id_str),
						       b_blinded_deck, true);

	if (!out)
		retval = ERR_DECK_BLINDING_CASHIER;

	if (out) {
		char *out_str = cJSON_PrintUnformatted(out);
		if (out_str) {
			dlg_info("%s", out_str);
			free(out_str);
		}
	}

	return retval;
}

void cashier_init_deck(char *table_id)
{
	int32_t num_players;
	char *game_id_str = NULL;
	cJSON *t_player_info = NULL;

	bet_permutation(b_deck_info.b_permi, CARDS_MAXCARDS);

	game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	t_player_info = get_cJSON_from_id_key_vdxfid_from_height(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str), g_start_block);
	num_players = jint(t_player_info, "num_players");
	for (int32_t i = 0; i < num_players; i++) {
		gen_deck(b_deck_info.cashier_r[i], CARDS_MAXCARDS);
	}
}

int32_t cashier_shuffle_deck(char *id)
{
	int32_t num_players = 0, retval = OK;
	char *game_id_str = NULL;
	cJSON *t_d_p_deck_info = NULL, *t_player_info = NULL;
	bits256 t_d_p_deck[CARDS_MAXCARDS];

	cashier_init_deck(id);
	game_id_str = poker_get_key_str(id, T_GAME_ID_KEY);
	t_player_info = get_cJSON_from_id_key_vdxfid_from_height(id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str), g_start_block);
	num_players = jint(t_player_info, "num_players");

	for (int32_t i = 0; i < num_players; i++) {
		t_d_p_deck_info =
			get_cJSON_from_id_key_vdxfid_from_height(id, get_key_data_vdxf_id(all_t_d_p_keys[(i + 1)], game_id_str), g_start_block);
		int32_t deck_size = cJSON_GetArraySize(t_d_p_deck_info);
		if (deck_size > CARDS_MAXCARDS) {
			deck_size = CARDS_MAXCARDS;
		}
		for (int32_t j = 0; j < deck_size; j++) {
			t_d_p_deck[j] = jbits256i(t_d_p_deck_info, j);
		}
		retval = cashier_sb_deck(id, t_d_p_deck, i);
		if (retval)
			return retval;
	}

	return retval;
}

int32_t reveal_bv(char *table_id)
{
	int32_t player_id, card_id, num_players, retval = OK;
	char *game_id_str = NULL, hexstr[65];
	cJSON *bv_info = NULL, *game_state_info = NULL, *t_player_info = NULL, *out = NULL, *card_bv = NULL;

	game_state_info = get_game_state_info(table_id);
	player_id = jint(game_state_info, "player_id");
	card_id = jint(game_state_info, "card_id");

	game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);

	bv_info = cJSON_CreateArray();
	if (player_id == -1) {
		t_player_info =
			get_cJSON_from_id_key_vdxfid_from_height(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str), g_start_block);
		num_players = jint(t_player_info, "num_players");
		for (int32_t i = 0; i < num_players; i++) {
			jaddistr(bv_info, bits256_str(hexstr, b_deck_info.cashier_r[i][card_id].priv));
		}
	} else {
		jaddistr(bv_info, bits256_str(hexstr, b_deck_info.cashier_r[player_id][card_id].priv));
	}

	card_bv = cJSON_CreateObject();
	jaddnum(card_bv, "player_id", player_id);
	jaddnum(card_bv, "card_id", card_id);
	jadd(card_bv, "bv", bv_info);

	{
		char *card_bv_str = cJSON_PrintUnformatted(card_bv);
		if (card_bv_str) {
			dlg_info("bv_info::%s", card_bv_str);
			free(card_bv_str);
		}
	}
	out = poker_append_key_json(table_id, get_key_data_vdxf_id(T_CARD_BV_KEY, game_id_str), card_bv,
					true);

	if (!out) {
		retval = ERR_BV_UPDATE;
	}
	return retval;
}

static int32_t handle_bv_reveal_card(char *table_id)
{
	int32_t retval = OK;
	char *game_id_str = NULL;
	cJSON *card_bv = NULL, *game_state_info = NULL;

	game_state_info = get_game_state_info(table_id);
	game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	card_bv = get_cJSON_from_id_key_vdxfid_from_height(table_id, get_key_data_vdxf_id(T_CARD_BV_KEY, game_id_str), g_start_block);

	if (card_bv && ((jint(game_state_info, "player_id") == jint(card_bv, "player_id")) &&
			(jint(game_state_info, "card_id") == jint(card_bv, "card_id")))) {
		dlg_info("Cashier revealed its Blinding Value for the player_id::%d, card_id::%d...",
			 jint(card_bv, "player_id"), jint(card_bv, "card_id"));
		return retval;
	}
	retval = reveal_bv(table_id);
	return retval;
}

// Process settlement - cashier sends funds to players
static int32_t cashier_process_settlement(char *table_id)
{
	int32_t retval = OK;
	char *game_id_str = NULL;
	cJSON *settlement_info = NULL, *settle_amounts = NULL, *player_ids = NULL;
	cJSON *payout_txs = NULL;
	
	dlg_info("=== PROCESSING POT SETTLEMENT ===");
	
	game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	if (!game_id_str) {
		dlg_error("Failed to get game_id");
		return ERR_GAME_ID_NOT_FOUND;
	}
	
	// Read settlement info from table ID
	settlement_info = get_cJSON_from_id_key_vdxfid_from_height(table_id,
		get_key_data_vdxf_id(T_SETTLEMENT_INFO_KEY, game_id_str), g_start_block);
	
	if (!settlement_info) {
		dlg_error("No settlement info found");
		return ERR_INVALID_POS;
	}
	
	const char *status = jstr(settlement_info, "status");
	if (!status || strcmp(status, "pending") != 0) {
		dlg_info("Settlement not pending (status: %s)", status ? status : "null");
		return OK;
	}
	
	settle_amounts = cJSON_GetObjectItem(settlement_info, "settle_amounts");
	player_ids = cJSON_GetObjectItem(settlement_info, "player_ids");
	
	if (!settle_amounts || !player_ids) {
		dlg_error("Missing settle_amounts or player_ids in settlement info");
		return ERR_INVALID_POS;
	}
	
	int num_players = cJSON_GetArraySize(settle_amounts);
	payout_txs = cJSON_CreateArray();
	
	// Send funds to each player
	for (int i = 0; i < num_players; i++) {
		cJSON *amount_item = cJSON_GetArrayItem(settle_amounts, i);
		cJSON *player_id_item = cJSON_GetArrayItem(player_ids, i);
		
		if (!amount_item || !player_id_item) continue;
		
		double amount = amount_item->valuedouble;
		const char *player_id = player_id_item->valuestring;
		
		if (amount <= 0) {
			dlg_info("Player %s has no payout (amount: %f)", player_id, amount);
			cJSON_AddItemToArray(payout_txs, cJSON_CreateString(""));
			continue;
		}
		
		dlg_info("Sending %f CHIPS to player %s", amount, player_id);
		
		// Create payout data
		cJSON *payout_data = cJSON_CreateObject();
		cJSON_AddStringToObject(payout_data, "type", "game_settlement");
		cJSON_AddStringToObject(payout_data, "game_id", game_id_str);
		cJSON_AddStringToObject(payout_data, "table_id", table_id);
		cJSON_AddNumberToObject(payout_data, "player_slot", i);
		
		// Send using sendcurrency
		cJSON *tx_result = verus_sendcurrency_data(player_id, amount, payout_data);
		
		if (tx_result) {
			const char *txid = NULL;
			if (tx_result->type == cJSON_String) {
				txid = tx_result->valuestring;
			} else {
				txid = jstr(tx_result, "opid");
			}
			dlg_info("Payout TX for player %s: %s", player_id, txid ? txid : "pending");
			cJSON_AddItemToArray(payout_txs, cJSON_CreateString(txid ? txid : "pending"));
		} else {
			dlg_error("Failed to send payout to player %s", player_id);
			cJSON_AddItemToArray(payout_txs, cJSON_CreateString("failed"));
		}
		
		cJSON_Delete(payout_data);
	}
	
	// Update settlement info with payout TXs and mark complete
	cJSON *updated_settlement = cJSON_Duplicate(settlement_info, 1);
	cJSON_DeleteItemFromObject(updated_settlement, "status");
	cJSON_AddStringToObject(updated_settlement, "status", "completed");
	cJSON_AddItemToObject(updated_settlement, "payout_txs", payout_txs);
	
	// Write updated settlement to table ID
	cJSON *out = poker_update_key_json(table_id,
		get_key_data_vdxf_id(T_SETTLEMENT_INFO_KEY, game_id_str),
		updated_settlement, true);
	
	if (!out) {
		dlg_error("Failed to update settlement status");
		cJSON_Delete(updated_settlement);
		return ERR_GAME_STATE_UPDATE;
	}
	
	dlg_info("Settlement complete - updating cashier game state");
	
	// Ensure cashier's game_id is set before updating game state
	ensure_cashier_game_id_initialized(table_id);
	
	// Update cashier's game state to complete
	append_game_state(bet_get_cashiers_id_fqn(), G_SETTLEMENT_COMPLETE, NULL);
	
	cJSON_Delete(updated_settlement);
	return retval;
}

// Ensure cashier's ID has T_GAME_ID_KEY set (required for append_game_state to work)
static int32_t ensure_cashier_game_id_initialized(char *table_id)
{
	static bool initialized = false;
	if (initialized) return OK;
	
	// Get game_id from the table
	char *game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	if (!game_id_str) {
		dlg_error("Cannot initialize cashier game_id - table has no game_id");
		return ERR_GAME_ID_NOT_FOUND;
	}
	
	// Set T_GAME_ID_KEY on the cashier's ID
	dlg_info("Initializing cashier's T_GAME_ID_KEY: %s", game_id_str);
	cJSON *out = poker_append_key_hex((char *)bet_get_cashiers_id_fqn(), T_GAME_ID_KEY, game_id_str, false);
	if (!out) {
		dlg_error("Failed to set game_id on cashier's ID");
		return ERR_GAME_STATE_UPDATE;
	}
	
	initialized = true;
	dlg_info("Cashier game_id initialized successfully");
	return OK;
}

int32_t handle_game_state_cashier(char *table_id)
{
	int32_t game_state, retval = OK;
	static int32_t last_logged_state = -1;

	game_state = get_game_state(table_id);
	if (game_state != last_logged_state) {
		dlg_info("%s", game_state_str(game_state));
		last_logged_state = game_state;
	}
	switch (game_state) {
	case G_ZEROIZED_STATE:
	case G_TABLE_ACTIVE:
	case G_TABLE_STARTED:
	case G_PLAYERS_JOINED:
	case G_DECK_SHUFFLING_P:
	case G_DECK_SHUFFLING_B:
		break;
	case G_DECK_SHUFFLING_D:
		// Ensure cashier's game_id is set before updating game state
		retval = ensure_cashier_game_id_initialized(table_id);
		if (retval) break;
		
		retval = cashier_shuffle_deck(table_id);
		if (!retval)
			append_game_state(bet_get_cashiers_id_fqn(), G_DECK_SHUFFLING_B, NULL);
		break;
	case G_REVEAL_CARD:
		retval = handle_bv_reveal_card(table_id);
		break;
	case G_SETTLEMENT_PENDING:
		dlg_info("Processing pot settlement...");
		retval = cashier_process_settlement(table_id);
		break;
	case G_SETTLEMENT_COMPLETE:
		dlg_info("Settlement complete - game finished");
		break;
	}
	return retval;
}

int32_t cashier_game_init(char *table_id)
{
	int32_t retval = OK;

	while (1) {
		retval = handle_game_state_cashier(table_id);
		if (retval)
			return retval;
		sleep(2);
	}
}
