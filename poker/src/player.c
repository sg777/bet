#include "bet.h"
#include "player.h"
#include "err.h"
#include "misc.h"
#include "commands.h"
#include "deck.h"
#include "game.h"
#include "config.h"
#include "print.h"
#include "poker_vdxf.h"
#include "storage.h"

struct p_deck_info_struct p_deck_info;

int32_t player_init_deck()
{
	int32_t retval = OK;
	char str[65], game_id_str[65];
	cJSON *cjson_player_cards = NULL, *player_deck = NULL;

	// Player ID numbering range from [0...8]
	if ((p_deck_info.player_id < 0) || (p_deck_info.player_id > 8))
		return ERR_INVALID_PLAYER_ID;

	p_deck_info.p_kp = gen_keypair();

	gen_deck(p_deck_info.player_r, CARDS_MAXCARDS);

	// Save deck info to local DB immediately after generation (for rejoin support)
	bits256_str(game_id_str, p_deck_info.game_id);
	retval = save_player_deck_info(game_id_str, player_config.table_id, p_deck_info.player_id);
	if (retval != OK) {
		dlg_error("Failed to save player deck info to local DB");
		// Continue anyway - this is non-fatal for first run
	}

	player_deck = cJSON_CreateObject();
	jaddnum(player_deck, "id", p_deck_info.player_id);
	jaddbits256(player_deck, "pubkey", p_deck_info.p_kp.prod);
	jadd(player_deck, "cardinfo", cjson_player_cards = cJSON_CreateArray());
	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		jaddistr(cjson_player_cards, bits256_str(str, p_deck_info.player_r[i].prod));
	}

	dlg_info("Updating %s key...", T_GAME_ID_KEY);
	cJSON *out = poker_append_key_hex(player_config.verus_pid, T_GAME_ID_KEY,
					     bits256_str(str, p_deck_info.game_id), false);

	dlg_info("Updating %s key...", PLAYER_DECK_KEY);
	out = poker_append_key_json(
		player_config.verus_pid, get_key_data_vdxf_id(PLAYER_DECK_KEY, bits256_str(str, p_deck_info.game_id)),
		player_deck, true);
	dlg_info("%s", cJSON_Print(out));

	dlg_info("Updating game state...");
	out = append_game_state(player_config.verus_pid, G_DECK_SHUFFLING_P, NULL);
	if (!out)
		return ERR_GAME_STATE_UPDATE;
	dlg_info("%s", cJSON_Print(out));

	return OK;
}

int32_t decode_card(bits256 b_blinded_card, bits256 blinded_value, cJSON *dealer_blind_info)
{
	int32_t card_value = -1;
	char str1[65], str2[65];
	bits256 blinded_value_inv, d_blinded_card;

	blinded_value_inv = crecip_donna(blinded_value);
	d_blinded_card = fmul_donna(blinded_value_inv, b_blinded_card);

	dlg_info("Dealer blinded card :: %s", bits256_str(str1, d_blinded_card));

	for (int32_t i = 0; i < CARDS_MAXCARDS; i++) {
		for (int32_t j = 0; j < CARDS_MAXCARDS; j++) {
			if (strcmp(bits256_str(str1, d_blinded_card),
				   bits256_str(str2, curve25519(p_deck_info.player_r[i].priv,
								jbits256i(dealer_blind_info, j)))) == 0) {
				card_value = p_deck_info.player_r[i].priv.bytes[30];
				dlg_info("card::%x\n", p_deck_info.player_r[i].priv.bytes[30]);
			}
		}
	}
	return card_value;
}

int32_t reveal_card(char *table_id)
{
	int32_t retval = OK, player_id, card_id, card_value = -1;
	char *game_id_str = NULL, str[65];
	cJSON *game_state_info = NULL, *bv_info = NULL, *b_blinded_deck = NULL, *dealer_blind_info = NULL, *bv = NULL;
	bits256 b_blinded_card, blinded_value;

	game_state_info = get_game_state_info(table_id);
	player_id = jint(game_state_info, "player_id");
	card_id = jint(game_state_info, "card_id");

	if ((player_id == p_deck_info.player_id) || (player_id == -1)) {
		game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);

		while (1) {
			bv_info = get_cJSON_from_id_key_vdxfid(table_id,
							       get_key_data_vdxf_id(T_CARD_BV_KEY, game_id_str));
			if (!bv_info) {
				dlg_info("BV INFO hasn't revealed its secret yet");
				wait_for_a_blocktime();
				continue;
			}
			dlg_info("%s", cJSON_Print(bv_info));

			bv = jobj(bv_info, "bv");
			if (!bv) {
				// TODO:: This needs to be handled
				dlg_error("BV is missing");
			}
			dlg_info("%s", cJSON_Print(bv));
			if ((jint(bv_info, "card_id") == card_id) && (jint(bv_info, "player_id") == player_id))
				break;
		}

		b_blinded_deck = get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(all_t_b_p_keys[player_id],
											     game_id_str));
		b_blinded_card = jbits256i(b_blinded_deck, card_id);
		if (player_id == -1)
			blinded_value = jbits256i(bv, player_id);
		else
			blinded_value = jbits256i(bv, 0);

		dlg_info("blinded_value::%s", bits256_str(str, blinded_value));
		dlg_info("blinded_card::%s", bits256_str(str, b_blinded_card));
		dealer_blind_info =
			get_cJSON_from_id_key_vdxfid(table_id, get_key_data_vdxf_id(T_D_DECK_KEY, game_id_str));
		dlg_info("dealer_blind_info::%s", cJSON_Print(dealer_blind_info));
		card_value = decode_card(b_blinded_card, blinded_value, dealer_blind_info);
		dlg_info("card_value ::%d", card_value);
		if (card_value == -1) {
			retval = ERR_CARD_DECODING_FAILED;
		}
	}
	return retval;
}

static int32_t handle_player_reveal_card(char *table_id)
{
	int32_t retval = OK;
	cJSON *game_state_info = NULL, *player_game_state_info = NULL;

	player_game_state_info = get_game_state_info(player_config.verus_pid);
	game_state_info = get_game_state_info(table_id);

	if (!game_state_info) {
		// TODO:: This error needs to be handled.
		return retval;
	}
	if (jint(game_state_info, "player_id") != p_deck_info.player_id) {
		// Not this players turn
		dlg_info("Not this players turn...");
		return retval;
	}
	if ((!player_game_state_info) || (jint(game_state_info, "card_id") > jint(player_game_state_info, "card_id"))) {
		retval = reveal_card(table_id);
		if (retval == OK) {
			append_game_state(player_config.verus_pid, G_REVEAL_CARD_P_DONE, game_state_info);
			dlg_info("Updating players revealed card info :: %s", cJSON_Print(game_state_info));
		}
	}
	return retval;
}

int32_t handle_game_state_player(char *table_id)
{
	int32_t game_state, retval = OK;

	game_state = get_game_state(table_id);
	dlg_info("%s", game_state_str(game_state));
	switch (game_state) {
	case G_REVEAL_CARD:
		retval = handle_player_reveal_card(table_id);
		break;
	}
	return retval;
}

int32_t handle_verus_player()
{
	int32_t retval = OK;
	char game_id_str[65];

	// Check if poker is ready
	if ((retval = verify_poker_setup()) != OK) {
		dlg_error("Poker not ready: %s", bet_err_str(retval));
		return retval;
	}

	// Parse Verus player configuration
	if ((retval = bet_parse_verus_player()) != OK) {
		dlg_error("Failed to parse Verus player configuration: %s", bet_err_str(retval));
		return retval;
	}

	// Find a table
	bool already_joined = false;
	if ((retval = poker_find_table()) != OK) {
		if (retval == ERR_DUPLICATE_PLAYERID) {
			dlg_info("Player already exists in the table. Skipping join, proceeding to deck shuffling.");
			already_joined = true;
			retval = OK;  // Clear the error since we're handling it
		} else {
			dlg_error("Failed to find table: %s", bet_err_str(retval));
			return retval;
		}
	}
	dlg_info("Table found");
	print_struct_table(&player_t);

	// Join the table (skip if already joined)
	if (!already_joined) {
		if ((retval = poker_join_table()) != OK) {
			dlg_error("Failed to join table: %s", bet_err_str(retval));
			return retval;
		}
		dlg_info("Table Joined");
	}

	// Get player ID
	if ((retval = get_player_id(&p_deck_info.player_id)) != OK) {
		dlg_error("Failed to get player ID: %s", bet_err_str(retval));
		return retval;
	}
	dlg_info("Player ID: %d", p_deck_info.player_id);

	// Get the current game_id from the table
	char *game_id_from_table = poker_get_key_str(player_config.table_id, T_GAME_ID_KEY);
	if (game_id_from_table) {
		p_deck_info.game_id = bits256_conv(game_id_from_table);
		strncpy(game_id_str, game_id_from_table, sizeof(game_id_str) - 1);
		game_id_str[sizeof(game_id_str) - 1] = '\0';
		dlg_info("Game ID from table: %s", game_id_str);
	} else {
		// No game_id yet - will be set during deck init
		memset(game_id_str, 0, sizeof(game_id_str));
	}

	// Check if game is already past deck shuffling phase
	int32_t current_game_state = get_game_state(player_config.table_id);
	if (current_game_state > G_DECK_SHUFFLING_B) {
		// Game is already in progress - try to load deck info from local DB
		dlg_info("Game is in progress (state: %s). Attempting to load deck info from local DB...",
			 game_state_str(current_game_state));
		
		if (strlen(game_id_str) > 0) {
			retval = load_player_deck_info(game_id_str);
			if (retval == OK) {
				dlg_info("Successfully loaded player deck info from local DB!");
				dlg_info("Rejoining game with player_id=%d", p_deck_info.player_id);
			} else {
				dlg_error("Failed to load deck info from local DB: %s", bet_err_str(retval));
				dlg_error("Cannot rejoin - deck keys not found. Please wait for a new game.");
				return ERR_GAME_ALREADY_STARTED;
			}
		} else {
			dlg_error("No game_id found on table. Cannot rejoin.");
			return ERR_GAME_ALREADY_STARTED;
		}
	} else if (current_game_state >= G_PLAYERS_JOINED && current_game_state <= G_DECK_SHUFFLING_B) {
		// Game is in deck shuffling phase - check if we have local deck info
		if (already_joined && strlen(game_id_str) > 0) {
			retval = load_player_deck_info(game_id_str);
			if (retval == OK) {
				dlg_info("Loaded existing deck info for this game from local DB");
				// Skip deck init since we already have our deck
			} else {
				// No local deck info - need to initialize
				dlg_info("No saved deck info found, initializing new deck...");
				if ((retval = player_init_deck()) != OK) {
					dlg_error("Failed to initialize player deck: %s", bet_err_str(retval));
					return retval;
				}
				dlg_info("Player deck shuffling info updated to table");
			}
		} else {
			// First join - initialize deck
			if ((retval = player_init_deck()) != OK) {
				dlg_error("Failed to initialize player deck: %s", bet_err_str(retval));
				return retval;
			}
			dlg_info("Player deck shuffling info updated to table");
		}
	} else {
		// Game not started yet or waiting for players
		if ((retval = player_init_deck()) != OK) {
			dlg_error("Failed to initialize player deck: %s", bet_err_str(retval));
			return retval;
		}
		dlg_info("Player deck shuffling info updated to table");
	}

	// Main game loop
	while (1) {
		retval = handle_game_state_player(player_config.table_id);
		if (retval != OK) {
			dlg_error("Error in game state handling: %s", bet_err_str(retval));
			return retval;
		}
		sleep(2);
	}

	return retval;
}
